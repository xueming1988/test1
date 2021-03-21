/*
    File:    PlatformAudioUtilsNetBSD.c
    Package: AirPlayAudioPOSIXReceiver
    Version: AirPlay_Audio_POSIX_Receiver_211.1.p8
    
    Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
    capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
    Apple software is governed by and subject to the terms and conditions of your MFi License,
    including, but not limited to, the restrictions specified in the provision entitled ”Public
    Software”, and is further subject to your agreement to the following additional terms, and your
    agreement that the use, installation, modification or redistribution of this Apple software
    constitutes acceptance of these additional terms. If you do not agree with these additional terms,
    please do not use, install, modify or redistribute this Apple software.
    
    Subject to all of these terms and in consideration of your agreement to abide by them, Apple grants
    you, for as long as you are a current and in good-standing MFi Licensee, a personal, non-exclusive
    license, under Apple's copyrights in this original Apple software (the "Apple Software"), to use,
    reproduce, and modify the Apple Software in source form, and to use, reproduce, modify, and
    redistribute the Apple Software, with or without modifications, in binary form. While you may not
    redistribute the Apple Software in source form, should you redistribute the Apple Software in binary
    form, you must retain this notice and the following text and disclaimers in all such redistributions
    of the Apple Software. Neither the name, trademarks, service marks, or logos of Apple Inc. may be
    used to endorse or promote products derived from the Apple Software without specific prior written
    permission from Apple. Except as expressly stated in this notice, no other rights or licenses,
    express or implied, are granted by Apple herein, including but not limited to any patent rights that
    may be infringed by your derivative works or by other works in which the Apple Software may be
    incorporated.
    
    Unless you explicitly state otherwise, if you provide any ideas, suggestions, recommendations, bug
    fixes or enhancements to Apple in connection with this software (“Feedback”), you hereby grant to
    Apple a non-exclusive, fully paid-up, perpetual, irrevocable, worldwide license to make, use,
    reproduce, incorporate, modify, display, perform, sell, make or have made derivative works of,
    distribute (directly or indirectly) and sublicense, such Feedback in connection with Apple products
    and services. Providing this Feedback is voluntary, but if you do provide Feedback to Apple, you
    acknowledge and agree that Apple may exercise the license granted above without the payment of
    royalties or further consideration to Participant.
    
    The Apple Software is provided by Apple on an "AS IS" basis. APPLE MAKES NO WARRANTIES, EXPRESS OR
    IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY
    AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR
    IN COMBINATION WITH YOUR PRODUCTS.
    
    IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION
    AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
    (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.
    
    Copyright (C) 2012-2014 Apple Inc. All Rights Reserved.

    AirPlay POSIX NetBSD Audio Platform Implementation
*/


#include <errno.h>
#include <math.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/audioio.h>
#include <sys/event.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include "AudioUtils.h"
#include "MathUtils.h"
#include "TickUtils.h"


// Enable to add detailed patform audio debug logs
#undef PLATFORM_AUDIO_DETAILED_DEBUG


//===========================================================================================================================
//  Structures
//===========================================================================================================================

struct AudioStreamPrivate
{
    CFRuntimeBase                   base;                   // CF type info. Must be first.
    int                             audioKQ;                // KEvent queue
    int                             audioFD;                // File descriptor for the audio device
    int                             quitFD;                 // File descriptor used to make the audio thread quit
    uint8_t *                       audioBuffer;            // Buffer between the user callback and the OS audio stack.
    size_t                          audioBufferSize;        // audio buffer size
    pthread_t                       thread;                 // Thread processing audio.
    pthread_t *                     threadPtr;              // Ptr to audio thread for NULL testing.

    AudioStreamAudioCallback_f      audioCallbackPtr;       // Function to call to give input audio or get output audio.
    void *                          audioCallbackCtx;       // Context to pass to audio callback function.
    AudioStreamFlags                flags;                  // Flags indicating input or output, etc.
    AudioStreamBasicDescription     format;                 // Format of the audio data.
    uint32_t                        preferredLatencyMics;   // Max latency the app can tolerate.
    char *                          threadName;             // Name to use when creating threads.
    int                             threadPriority;         // Priority to run threads.
    Boolean                         hasThreadPriority;      // True if a thread priority has been set.
};


//===========================================================================================================================
//  Prototypes
//===========================================================================================================================

static void *   _AudioStreamThread( void *inArg );
static int      getAudioPlayDevInfo( int inFD, audio_info_t *audioInfoPtr );
static int      flushAudioPlayDev( int inFD );
static int      isAudioPlayDevInError( int fd );
#ifdef PLATFORM_AUDIO_DETAILED_DEBUG
static void     dumpAudioPlayDevInfo( int fd, char *str );
#endif


//===========================================================================================================================
//  Globals
//===========================================================================================================================

static struct AudioStreamPrivate    gAudioStream;
double                              gCurrentVolumeLevel = 0;



//===========================================================================================================================
//  AudioStreamCreate
//===========================================================================================================================

OSStatus    AudioStreamCreate( AudioStreamRef *outStream )
{
    OSStatus            err;
    AudioStreamRef      me;
    size_t              extraLen;
    
    printf("%s()\n", __FUNCTION__);
    me = &gAudioStream;

    extraLen = sizeof( *me ) - sizeof( me->base );
    memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
    
    me->audioKQ     = -1;
    me->audioFD     = -1;
    
    require_action( me->threadPtr == NULL, exit, err = kAlreadyInitializedErr );
    
    *outStream = me;
    err = kNoErr;
    
exit:
    return( err );
}


//===========================================================================================================================
//  AudioStreamSetAudioCallback
//===========================================================================================================================

void    AudioStreamSetAudioCallback( AudioStreamRef me, AudioStreamAudioCallback_f inFunc, void *inContext )
{
    printf("%s()\n", __FUNCTION__);
    me->audioCallbackPtr = inFunc;
    me->audioCallbackCtx = inContext;
}

//===========================================================================================================================
//  AudioStreamSetFlags
//===========================================================================================================================

OSStatus    AudioStreamSetFlags( AudioStreamRef me, AudioStreamFlags inFlags )
{
    (void) me;
    (void) inFlags;

    printf("%s()\n", __FUNCTION__);
    me->flags = inFlags;
    return( kNoErr );
}

//===========================================================================================================================
//  AudioStreamSetFormat
//===========================================================================================================================

OSStatus    AudioStreamSetFormat( AudioStreamRef me, const AudioStreamBasicDescription *inFormat )
{
    printf("%s()\n", __FUNCTION__);
    me->format = *inFormat;
    return( kNoErr );
}

//===========================================================================================================================
//  AudioStreamGetLatency
//===========================================================================================================================

uint32_t    AudioStreamGetLatency( AudioStreamRef me, OSStatus *outErr )
{
    uint32_t                result = 0;
    OSStatus                err;
    
    printf("%s()\n", __FUNCTION__);

    require_action( me->audioFD, exit, err = kNotPreparedErr );
    result = (uint32_t)( ( ( (uint64_t) me->audioBufferSize ) * kMicrosecondsPerSecond ) / me->format.mSampleRate );
    err = kNoErr;

exit:
    if (outErr) *outErr = err;
    return( result );
}

//===========================================================================================================================
//  AudioStreamSetPreferredLatency
//===========================================================================================================================

OSStatus    AudioStreamSetPreferredLatency( AudioStreamRef me, uint32_t inMics )
{
    printf("%s()\n", __FUNCTION__);
    me->preferredLatencyMics = inMics;
    return( kNoErr );
}

//===========================================================================================================================
//  AudioStreamSetThreadName
//===========================================================================================================================

OSStatus    AudioStreamSetThreadName( AudioStreamRef me, const char *inName )
{
    OSStatus        err;
    char *          name;
    
    printf("%s()\n", __FUNCTION__);
    if( inName )
    {
        name = strdup( inName );
        require_action( name, exit, err = kNoMemoryErr );
    }
    else
    {
        name = NULL;
    }
    if( me->threadName ) free( me->threadName );
    me->threadName = name;
    err = kNoErr;
    
exit:
    return( err );
}

//===========================================================================================================================
//  AudioStreamSetThreadPriority
//===========================================================================================================================

void    AudioStreamSetThreadPriority( AudioStreamRef me, int inPriority )
{
    printf("%s()\n", __FUNCTION__);
    me->threadPriority = inPriority;
    me->hasThreadPriority = true;
}

//===========================================================================================================================
//  AudioStreamGetVolume
//===========================================================================================================================

double  AudioStreamGetVolume( AudioStreamRef me, OSStatus *outErr )
{
    audio_info_t                        audioInfo;
    int                                 err;
    double                              normalizedVolume = 1.0;
    long                                minVolume = 0, maxVolume = 255;    // NetBSD's range
    
    printf("%s()\n", __FUNCTION__);

    /*
     * NetBSD provides two ways to control volume:
     *
     *  1. audio device's play.gain - single _set_ value (0 - 255) 
     *             Command Line control: audioct -w play.gain=<value>
     *
     *  OR
     *
     *  2. mixer device's outputs.master - _set_ value one for each channel ( 0 - 255 for each channel). 
     *             Command Line control: mixerctl -w outputs.master=<value>,<value> (for stereo)
     *
     * Changing of either one changes the other one as well. So going to
     * use audio device play.gain to do our volume control. Is much easier and works fine.
     */
    
    err = ioctl( me->audioFD, AUDIO_GETINFO, &audioInfo );
    require( err >= 0, exit );

    /* normalize the [0->255] value to [0.0->1.0] range */
    normalizedVolume = TranslateValue( audioInfo.play.gain, minVolume, maxVolume, 0.0, 1.0 );
    normalizedVolume = Clamp( normalizedVolume, 0.0, 1.0 );

    err = kNoErr;
    
exit:
    if( outErr )    *outErr = err;
    return( normalizedVolume );
}

//===========================================================================================================================
//  AudioStreamSetVolume
//===========================================================================================================================

OSStatus    AudioStreamSetVolume( AudioStreamRef me, double inVolume )
{
    OSStatus                    err;
    audio_info_t                audioInfo;
    long                        minVolume = 0, maxVolume = 255;    // NetBSD's range
    long                        newNormalizedVolume;
    
    printf("%s()\n", __FUNCTION__);
    
    if(      inVolume <= 0 ) newNormalizedVolume = minVolume;
    else if( inVolume >= 1 ) newNormalizedVolume = maxVolume;
    else
    {
        /* normalize the ncoming [0.0->1.0] value to [0->255] range */
        newNormalizedVolume = (long) TranslateValue( inVolume, 0.0, 1.0, minVolume, maxVolume );
        newNormalizedVolume = Clamp( newNormalizedVolume, minVolume, maxVolume );
    }

    /* Set requested new volume on the audio device */
    printf("Setting Audio Device volume to inLinear=%f normalizedVolume=%ld\n", inVolume, newNormalizedVolume);
    AUDIO_INITINFO( &audioInfo );
    audioInfo.play.gain = newNormalizedVolume;
    err = ioctl( me->audioFD, AUDIO_SETINFO, &audioInfo );
    require_action( err >= 0, exit, err = kUnknownErr );

    // save the volume level if it is not mute. This will be used during unmute by PlatformRemote.c
    if (inVolume != 0.0 )   gCurrentVolumeLevel = inVolume;
    
    err = kNoErr;

exit:
    return( err );
}

    
//===========================================================================================================================
//  AudioStreamPrepare
//===========================================================================================================================

OSStatus    AudioStreamPrepare( AudioStreamRef me )
{
    OSStatus                err;
    audio_info_t            audioInfo;
    struct kevent           ke;
    int                     n;
    
    printf("%s()\n", __FUNCTION__);
    require_action( me->format.mFormatID == kAudioFormatLinearPCM, exit, err = kFormatErr );


    // create the kqueue we will use to block on

    me->audioKQ = kqueue();
    require_action( me->audioKQ >= 0, exit, err = kNoResourcesErr );

    // Open the audio device and set it up for non-blocking I/O since we'll use a kqueue for blocking.
    
    me->audioFD = open( "/dev/audio", O_WRONLY );
    require_action( me->audioFD >= 0, exit, err = kOpenErr );
    
    err = fcntl( me->audioFD, F_SETFL, fcntl( me->audioFD, F_GETFL, 0 ) | O_NONBLOCK );
    require_action( err != -1, exit, err = kUnknownErr );
    
#ifdef PLATFORM_AUDIO_DETAILED_DEBUG
    dumpAudioPlayDevInfo( me->audioFD, "Before Set");
#endif

    // Configure the audio device for 44100 Hz, 16-bit, stereo, PCM samples.
    
    AUDIO_INITINFO( &audioInfo );
    audioInfo.play.sample_rate  = 44100;
    audioInfo.play.channels     = 2;
    audioInfo.play.encoding     = AUDIO_ENCODING_SLINEAR;
    audioInfo.play.precision    = 16;
    audioInfo.mode              = AUMODE_PLAY_ALL; // Don't require writing missed data to "catch up" if we write too slow.
    
    err = ioctl( me->audioFD, AUDIO_SETINFO, &audioInfo );
    err = map_noerr_errno( err );
    require_noerr( err, exit );
    
#ifdef PLATFORM_AUDIO_DETAILED_DEBUG
    dumpAudioPlayDevInfo( me->audioFD, "After Set");
#endif
    
    err = ioctl( me->audioFD, AUDIO_GETINFO, &audioInfo );
    err = map_noerr_errno( err );
    require_noerr( err, exit );
    
    printf( "Audio Device Configuration:\n" );
    printf( "    buffer_size: %u\n", audioInfo.play.buffer_size );
    printf( "    blocksize:   %u\n", audioInfo.blocksize );
    printf( "    hiwat:       %u\n", audioInfo.hiwat );
    printf( "    lowat:       %u\n", audioInfo.lowat );
    printf( "\n" );
    
    me->audioBufferSize = audioInfo.blocksize * audioInfo.lowat;
    me->audioBuffer = (uint8_t *) malloc( me->audioBufferSize );
    require_action( me->audioBuffer, exit, err = kNoMemoryErr );
    
    // Register with kqueue for writability notification.
    
    EV_SET( &ke, me->audioFD, EVFILT_WRITE, EV_ADD, 0, 0, 0 );
    n = kevent( me->audioKQ, &ke, 1, NULL, 0, NULL );
    require_action( n == 0, exit, err = kUnknownErr );

    err = kNoErr;
    
exit:
    if( err ) AudioStreamStop( me, false );
    return( err );
}

//===========================================================================================================================
//  AudioStreamStart
//===========================================================================================================================

OSStatus    AudioStreamStart( AudioStreamRef me )
{
    OSStatus        err;
    struct kevent   ke;
    int             n;
    
    printf("%s()\n", __FUNCTION__);

    require_action( me->audioKQ, exit, err = kNotPreparedErr );
    require_action( me->audioFD, exit, err = kNotPreparedErr );

    // Start a thread to provide audio to the hardware. This is done in a separate thread rather than the receiver
    // thread because the receiver thread is where all the time-consuming decompression and decryption occurs so 
    // we don't want to hold off the audio hardware to decompress new packets when there may already be audio queued.
    
    err = pthread_create( &me->thread, NULL, _AudioStreamThread, me );
    require_noerr( err, exit );
    me->threadPtr = &me->thread;
    

    // Create a file to communicate with the thread when quit time arrives
    me->quitFD = open( "/tmp/airplayd.audiothread.quit", O_CREAT | O_RDWR, 0666 );
    require_action( me->quitFD != -1, exit, err = kNoMemoryErr );

    // Register with kqueue for written notification.
    
    EV_SET( &ke, me->quitFD, EVFILT_VNODE, EV_ADD | EV_ENABLE | EV_CLEAR, NOTE_WRITE, 0, 0 );
    n = kevent( me->audioKQ, &ke, 1, NULL, 0, NULL );
    require_action( n != -1, exit, err = kUnknownErr );

    err = kNoErr;

exit:
    if( err ) AudioStreamStop( me, false );
    return( err );
}

//===========================================================================================================================
//  AudioStreamStop
//===========================================================================================================================

void    AudioStreamStop( AudioStreamRef me, Boolean inDrain )
{
    int         err, ret;
    Boolean     doRelease = false;

    (void) err;

    printf("%s(drain=%d)\n", __FUNCTION__, inDrain);

    if( me->threadPtr )
    {
        /* Notify the audio thread to stop rendering and quit. */
        ret = write( me->quitFD, "q", strlen("q") );
        
        if ( ret == -1 )
        {
            printf("write to quitFD failed. Using thread cancel instead.\n");
            ret = pthread_cancel( me->thread );
            check( ret == -1 );
        }

        /* Now wait for the thread to finish. */
        err = pthread_join( me->thread, NULL );
        check_noerr( err );

        me->threadPtr = NULL;
        ForgetFD( &me->quitFD );
        doRelease = true;
    }

    /* Shutdown/reset platform's audio infrastructure. Cleanup buffers etc used for this stream. */
    if( me->audioFD != -1)
    {
        if( inDrain )
        {
            /* drain the remaining samples */
            err = ioctl( me->audioFD, AUDIO_DRAIN );
            check( err != -1 );
        }
        ForgetFD( &me->audioFD );
    }

    ForgetMem( &me->audioBuffer );
    ForgetMem( &me->threadName );

    if( doRelease ) CFRelease( me );
}

//===========================================================================================================================
//  AudioStreamSetVarispeedRate
//===========================================================================================================================

OSStatus    AudioStreamSetVarispeedRate( AudioStreamRef inStream, double inHz )
{
    printf("%s(): inStream=%p inHz=%f\n", __FUNCTION__, inStream, inHz);

    /*
     * Platform capable of its own high quality skew compensation should use the passed in values
     * to control its skew compensation mechanism here 
     */

    return( kNoErr );
}



//===========================================================================================================================
//  _AudioStreamThread
//===========================================================================================================================

static void *   _AudioStreamThread( void *inArg )
{
    AudioStreamRef const        me = (AudioStreamRef) inArg;
    size_t const                bytesPerUnit    = me->format.mBytesPerFrame;
    uint64_t                    hostTime;
    uint32_t                    sampleTime      = 0;
    struct kevent               ke;
    int                         n;
    OSStatus                    err;
    
    if( me->hasThreadPriority )
    {
        int                     policy;
        struct sched_param      sched;

        err = pthread_getschedparam( pthread_self(), &policy, &sched );
        check_noerr( err );

        sched.sched_priority = me->threadPriority;
        err = pthread_setschedparam( pthread_self(), SCHED_FIFO, &sched );
        check_noerr( err );
    }
    
    for( ;; )
    {
        n = kevent( me->audioKQ, NULL, 0, &ke, 1, NULL );
        err = ( n > 0 ) ? 0 : errno;
        if( err == EINTR ) continue;
        if( err )
        {
            printf( "kevent error: %d", err );
            usleep( 100000 );
            continue;
        }

        if( ( ke.filter == EVFILT_WRITE ) && ( ke.ident == (uintptr_t) me->audioFD ) )
        {
            size_t      size;
            
            size = Min( ke.data, me->audioBufferSize );
            check( size > 0 );
            
            // Call AirPlay callback to retrieve the next set of audio samples.

            hostTime = UpTicks();
            me->audioCallbackPtr( sampleTime, hostTime, me->audioBuffer, size, me->audioCallbackCtx );
            sampleTime += (size / bytesPerUnit);
                
            // check for audio play device error
            if ( isAudioPlayDevInError( me->audioFD ))
            {
                printf("Audio Play Device Error detected.\n");
                flushAudioPlayDev( me->audioFD );
                usleep( 100000 );
            }

            // write the retrieved audio samples to the audio device

            n = write( me->audioFD, me->audioBuffer, size );
            if ( n == -1 )
            {
                printf("%s() - write ret -1 errno=%d\n", __FUNCTION__, errno);
#ifdef PLATFORM_AUDIO_DETAILED_DEBUG
                dumpAudioPlayDevInfo( me->audioFD, "Write Error");
#endif
                break;
            }
            else if ( n < (int) size )
            {
                printf("%s() - write requested=%zu actual=%d\n", __FUNCTION__, size, n );
            }
            err = ( n == (int) size ) ? 0 : errno;
            check_noerr( err );
        }

        else if( ke.filter == EVFILT_VNODE )
        {
            // The quit file was written to. We use it only toquit, so exit now.
            printf( "Quitting. kevent: filter=%d, ident=%p", ke.filter, (void *) ke.ident );
            
            break;
        }

        else
        {
            printf( "unknown kevent: filter=%d, ident=%p", ke.filter, (void *) ke.ident );
            usleep( 100000 );
        }
    }
            
    return( NULL );
}



//===========================================================================================================================
//  flushAudioPlayDev
//===========================================================================================================================

static int     flushAudioPlayDev( int inFD )
{
    int     err;

    err = ioctl( inFD, AUDIO_FLUSH );
    printf("AUDIO_FLUSH ioctl ret=%d\n", err);

    return( err );
}


//===========================================================================================================================
//  getAudioPlayDevInfo
//===========================================================================================================================

static int     getAudioPlayDevInfo( int inFD, audio_info_t *audioInfoPtr )
{
    int     err;

    require_action( audioInfoPtr, exit, err = -1 );

    err = ioctl( inFD, AUDIO_GETINFO, audioInfoPtr );
    require( err >= 0, exit );

    err = 0;

exit:
    return( err );
}


//===========================================================================================================================
//  isAudioPlayDevInError
//===========================================================================================================================

static int     isAudioPlayDevInError( int fd )
{
    audio_info_t    audioInfo;
    int             ret, error;

    ret = getAudioPlayDevInfo( fd, &audioInfo );
    require_action( ret >= 0, exit, error = 1 );

    error = audioInfo.play.error;

exit:
    return( error );
}


#ifdef PLATFORM_AUDIO_DETAILED_DEBUG
//===========================================================================================================================
//  dumpAudioPlayDevInfo
//===========================================================================================================================

static void     dumpAudioPlayDevInfo( int fd, char *str )
{
    audio_info_t audioInfo;
    int                     err;

    printf("---- Audio Play Device Info %s ----\n", str ? str : "");

    err = getAudioPlayDevInfo( fd, &audioInfo );
    require( err >= 0, exit );


    printf("\tBlock Size=%d\n", audioInfo.blocksize);
    printf("\tmode=%d\n", audioInfo.mode);
    printf("\thiwat=%d\n", audioInfo.hiwat);
    printf("\tlowat=%d\n", audioInfo.lowat);

    printf("\tBuffer Size=%d\n", audioInfo.play.buffer_size);
    printf("\tsample rate=%d\n", audioInfo.play.sample_rate);
    printf("\tchannels=%d\n", audioInfo.play.channels);
    printf("\tencoding=%d\n", audioInfo.play.encoding);
    printf("\tprecision=%d\n", audioInfo.play.precision);
    printf("\tgain=%d\n", audioInfo.play.gain);
    printf("\tseek=%d\n", audioInfo.play.seek);
    printf("\t#samples=%d\n", audioInfo.play.samples);
    printf("\teof=%d\n", audioInfo.play.eof);
    printf("\tpause=%d\n", audioInfo.play.pause);
    printf("\terror=%d\n", audioInfo.play.error);
    printf("\twaiting=%d\n", audioInfo.play.waiting);
    printf("\tbalance=%d\n", audioInfo.play.balance);
    printf("\topen=%d\n", audioInfo.play.open);
    printf("\tactive=%d\n", audioInfo.play.active);

    printf("-----------------------------------\n");

exit:
    return;
}
#endif

