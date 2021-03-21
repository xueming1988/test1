/*
	File:    PlatformAudioUtils_POSIX_Stub.c
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

	Subject to all of these terms and in?consideration of your agreement to abide by them, Apple grants
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

	Copyright (C) 2012-2013 Apple Inc. All Rights Reserved.
*/

#include "AudioUtils.h"

#include <errno.h>
#include <math.h>
#include <poll.h>
#include <stdlib.h>

#include "CommonServices.h"
#include "DebugServices.h"
#include "MathUtils.h"
#include "TickUtils.h"
#include "ThreadUtils.h"
#include <alsa/asoundlib.h>
#include "DACPCommon.h"

#ifdef AUDIO_SRC_OUTPUT
extern void updateDmixConfDefault(void);
#endif

#include CF_HEADER
#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

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

static AudioStreamRef me_g;
static AudioStreamAudioCallback_f inFunc_g;
static void *inContext_g;
static int need_to_stop = 0;
static int alsa_need_to_wait = 0;
static int alsa_is_ready = 0;

pthread_t audio_stream_callback_ptr;
snd_pcm_uframes_t period_size;
#define AUDIO_BUFFER_SIZE	352/2 //*4*16
#define DEFAULT_FORMAT		SND_PCM_FORMAT_S16_LE
#define DEFAULT_SPEED 		44100
#define DEFAULT_CHANNEL 	2
#define DEFAULT_BITS 		16
#define AO_ALSA_BUFFER_TIME 100000

void *audio_stream_callback(void *argu);

#define NUM_CHANNELS DEFAULT_CHANNEL
static pthread_mutex_t alsa_mutex = PTHREAD_MUTEX_INITIALIZER;
static snd_pcm_t *alsa_handle = NULL;
static snd_pcm_hw_params_t *alsa_params = NULL;
static pthread_mutex_t airplay_client_mutex = PTHREAD_MUTEX_INITIALIZER;
static int airplay_client = 0;
static int audio_init(int sampling_rate);
static int audio_play(char* outbuf, int samples, void* priv_data);
static void audio_deinit(void);
int is_airplay_ready(void);
//#define FUNCTION_TRACE() printf("== Enter %s ==\n", __FUNCTION__)
#define FUNCTION_TRACE()

#define RINGBUF_ENABLE 0
#if RINGBUF_ENABLE
#define ALSA_BUF_NUM 20
typedef struct
{
	short *pbuf[ALSA_BUF_NUM];
	int play_index;
	int fill_index;
	int free_count;
} Alsa_ringbuf_info;

static Alsa_ringbuf_info ringbuf;
static pthread_mutex_t alsa_ringbuf_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t audio_stream_play_ptr;

void *audio_stream_play_thread(void *argu)
{
	int need_to_play = 0;

	(void)argu;
	while (1)
	{
		if( need_to_stop )
			break;

		pthread_mutex_lock(&alsa_ringbuf_mutex);
		need_to_play = ALSA_BUF_NUM - ringbuf.free_count;
		pthread_mutex_unlock(&alsa_ringbuf_mutex);

		if( need_to_play )
		{
			audio_play((char *)ringbuf.pbuf[ringbuf.play_index], AUDIO_BUFFER_SIZE/4, NULL);
			pthread_mutex_lock(&alsa_ringbuf_mutex);
			ringbuf.play_index = (ringbuf.play_index + 1 )%ALSA_BUF_NUM;
			++ringbuf.free_count;
			pthread_mutex_unlock(&alsa_ringbuf_mutex);
		}
		else
		{
			//printf("%s need_to_play=%d, play_index=%d, free_count=%d\n",  __FUNCTION__, need_to_play, ringbuf.play_index, ringbuf.free_count);
			usleep(1000);
		}
	}

	printf("Exit audio_stream_play_thread\n");
	return 0;
}

void *audio_stream_callback(void *argu)
{
	uint32_t SampleNo = 0;
	uint64_t	HostTime;
	int i;
	(void)argu;

	int need_to_fill=0;
	pthread_mutex_lock(&alsa_ringbuf_mutex);
	memset((void *)&ringbuf, 0, sizeof(ringbuf));
	ringbuf.pbuf[0] = (short*)malloc(sizeof(short)*AUDIO_BUFFER_SIZE*ALSA_BUF_NUM);
	printf("%s %d ringbuf.pbuf[0] =%p\n", __FUNCTION__, __LINE__, ringbuf.pbuf[0] );
	if( ringbuf.pbuf[0] != NULL )
	{
		memset(ringbuf.pbuf[0], 0x0, sizeof(short)*AUDIO_BUFFER_SIZE*ALSA_BUF_NUM);
		ringbuf.free_count = ALSA_BUF_NUM;
		ringbuf.fill_index = 0;
		ringbuf.play_index = 0;
		for(i=0; i<ALSA_BUF_NUM; ++i)
		{
			ringbuf.pbuf[i] = (char *)ringbuf.pbuf[0] + (sizeof(short)*AUDIO_BUFFER_SIZE*i);
			printf(" ringbuf.pbuf[%d] =%p\n",i, ringbuf.pbuf[i] );
		}
	}
	else
	{
		printf("!!!! malloc failed??\n");
	}
	pthread_mutex_unlock(&alsa_ringbuf_mutex);

	while (1)
	{
		if( need_to_stop )
		{
			printf("!!!%s stop playing\n", __FUNCTION__);
			break;
		}
		pthread_mutex_lock(&alsa_ringbuf_mutex);
		if(ringbuf.free_count > 0 )
			need_to_fill = 1;
		else
			need_to_fill = 0;
		pthread_mutex_unlock(&alsa_ringbuf_mutex);

		if( need_to_fill )
		{
			HostTime = UpTicks();
			inFunc_g(SampleNo, HostTime, ringbuf.pbuf[ringbuf.fill_index], AUDIO_BUFFER_SIZE, inContext_g);
			SampleNo = SampleNo + (AUDIO_BUFFER_SIZE/4);
			pthread_mutex_lock(&alsa_ringbuf_mutex);
			--ringbuf.free_count;
			ringbuf.fill_index = (ringbuf.fill_index+1)%ALSA_BUF_NUM;
			pthread_mutex_unlock(&alsa_ringbuf_mutex);
		}
		else
		{
			usleep(20000);
			//printf("%s need_to_fill=%d, SampleNo=%d, fill_index=%d, free_count=%d\n", __FUNCTION__, need_to_fill, SampleNo, ringbuf.fill_index, ringbuf.free_count);
		}

	}

	printf("Exit audio_stream_callback\n");
	return 0;
}
#else
static short *pBuffer = NULL;
void *audio_stream_callback(void *argu)
{
	uint32_t SampleNo = 0;
	uint64_t	HostTime;
	int oldstate = 0;
	int oldtype = 0;
	int ret;
	int ConfirmAudioBuff=0;

	AudioStreamRef const        me = (AudioStreamRef) argu;
	if( me->hasThreadPriority )
		SetCurrentThreadPriority(me->threadPriority);

	(void)argu;

	ret =  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
	//printf("pthread_setcancelstate PTHREAD_CANCEL_ENABLE=%d, ret=%d, old=%d\n", PTHREAD_CANCEL_ENABLE, ret, oldstate);
	ret = pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &oldtype);
	//printf("pthread_setcanceltype PTHREAD_CANCEL_DEFERRED=%d, ret=%d, old=%d\n", PTHREAD_CANCEL_DEFERRED, ret, oldtype);

	pBuffer = (short*)malloc(sizeof(short)*AUDIO_BUFFER_SIZE);
	memset(pBuffer, 0x0, AUDIO_BUFFER_SIZE);

	while (1)
	{
		if( alsa_need_to_wait || !alsa_is_ready )
		{
			printf("%s %d alsa_need_to_wait=%d, %d\n", __FUNCTION__, __LINE__, alsa_need_to_wait, alsa_is_ready);
			usleep(1000);
			SampleNo = 0;
			continue;
		}

		if( need_to_stop )
		{
			printf("!!!%s stop playing\n", __FUNCTION__);
			break;
		}

		if(access("/tmp/airplay_stop", R_OK)==0 )
		{
			/* send pause command to current client, prevent it playing next song to device again */
			SocketClientReadWriteMsg("/tmp/RequestAppleRemote","AIRPLAY_SWITCH_OUT",strlen("AIRPLAY_SWITCH_OUT"),NULL,NULL,0);
			//sleep(1);
			//printf("!!! /tmp/airplay_stop found! AirTunesServer_StopAllConnections\n");
			//AirTunesServer_StopAllConnections();
			remove("/tmp/airplay_stop");
			SampleNo = 0;
			continue;
		}

		if( ConfirmAudioBuff >= 0 )
		{
			HostTime = UpTicks();
			inFunc_g(SampleNo, HostTime, pBuffer, AUDIO_BUFFER_SIZE, inContext_g);
			ret = audio_play((char *)pBuffer, AUDIO_BUFFER_SIZE/4, NULL);
			//uint64_t new_time =  MicrosecondsToUpTicks(((AUDIO_BUFFER_SIZE/4)*10000)/441);
			//SleepUntilUpTicks(HostTime + new_time);
			SampleNo = SampleNo + (AUDIO_BUFFER_SIZE/4);
		}
		/* make the receiver thread get some data, for 1 second */
		else
		{
			++ConfirmAudioBuff;
			usleep(100000);
		}
	}

	printf("Exit audio_stream_callback\n");
	return 0;
}
#endif

static int CreateAlsaPipe(void)
{
	int err=0;
	err = audio_init(DEFAULT_SPEED);
	if(err < 0){
		fprintf(stderr, "audio_init error: %s %s, %d\n", snd_strerror(err),
			__FUNCTION__, __LINE__);
		return err;
	}

	return 1;
}


//===========================================================================================================================
//	AudioStreamCreate
//===========================================================================================================================

OSStatus	AudioStreamCreate( AudioStreamRef *outStream )
{
	OSStatus			err;

    /*
     * PLATFORM_TO_DO:
     *
     *   - Do necessary platform setup for stream create
     *   - Return an identifier for the stream in the outStream parameter
     */

	(void) outStream;

	FUNCTION_TRACE();
	pthread_mutex_lock(&airplay_client_mutex);
	remove("/tmp/airplay_stop");
	SocketClientReadWriteMsg("/tmp/Requesta01controller","setPlayerCmd:switchtoairplay", strlen("setPlayerCmd:switchtoairplay"),NULL,NULL,0);
	system("pkill imuzop");
	if( airplay_client == 0 )
	{
		CreateAlsaPipe();
	}
	else
	{
		alsa_need_to_wait = 1;
		/* send pause command to current client, prevent it playing next song to device again */
		//SocketClientReadWriteMsg("/tmp/RequestAppleRemote","AIRPLAY_SWITCH_OUT",strlen("AIRPLAY_SWITCH_OUT"),NULL,NULL,0);
		SocketClientReadWriteMsg("/tmp/RequestAppleRemote","AIRPLAY_PAUSE",strlen("AIRPLAY_PAUSE"),NULL,NULL,0);
		printf("== audio stream already created! ==\n");
	}
	++airplay_client;
	*outStream = (void *) alsa_handle;
	pthread_mutex_unlock(&airplay_client_mutex);
	err = kNoErr;
	return( err );
}

//===========================================================================================================================
//	AudioStreamSetAudioCallback
//===========================================================================================================================

void	AudioStreamSetAudioCallback( AudioStreamRef me, AudioStreamAudioCallback_f inFunc, void *inContext )
{
    (void) me;
    (void) inFunc;
    (void) inContext;
	me_g =  me;
	inFunc_g =  inFunc;
	inContext_g =	inContext;
	FUNCTION_TRACE();

    /*
     * PLATFORM_TO_DO:
     *
     *   - Save the callback function pointer and  context in stream specific structure.
     */
}

//===========================================================================================================================
//	AudioStreamSetFlags
//===========================================================================================================================

OSStatus	AudioStreamSetFlags( AudioStreamRef me, AudioStreamFlags inFlags )
{
    (void) me;
    (void) inFlags;
	FUNCTION_TRACE();

    /*
     * PLATFORM_TO_DO:
     *
     *   - Save the inFlags in stream specific structure.
     */

	return( kNoErr );
}

//===========================================================================================================================
//	AudioStreamSetFormat
//===========================================================================================================================

OSStatus	AudioStreamSetFormat( AudioStreamRef me, const AudioStreamBasicDescription *inFormat )
{
    (void) me;
    (void) inFormat;
	FUNCTION_TRACE();

    /*
     * PLATFORM_TO_DO:
     *
     *   - Save the inFormat in stream specific structure.
     */
#if 0
		printf("[APPLE]AudioStreamSetFormat()----  begin ---------- \n");
		printf("[APPLE]AudioStreamSetFormat()----  mSampleRate %d---------- \n", inFormat->mSampleRate);
		printf("[APPLE]AudioStreamSetFormat()----  mFormatID %d---------- \n", inFormat->mFormatID);
		printf("[APPLE]AudioStreamSetFormat()----  mFormatFlags %d---------- \n", inFormat->mFormatFlags);
		printf("[APPLE]AudioStreamSetFormat()----  mBytesPerPacket %d---------- \n", inFormat->mBytesPerPacket);
		printf("[APPLE]AudioStreamSetFormat()----  mFramesPerPacket %d---------- \n", inFormat->mFramesPerPacket);
		printf("[APPLE]AudioStreamSetFormat()----  mBytesPerFrame %d---------- \n", inFormat->mBytesPerFrame);
		printf("[APPLE]AudioStreamSetFormat()----  mChannelsPerFrame %d---------- \n", inFormat->mChannelsPerFrame);
		printf("[APPLE]AudioStreamSetFormat()----  mBitsPerChannel %d---------- \n", inFormat->mBitsPerChannel);
#endif
	return( kNoErr );
}

//===========================================================================================================================
//	AudioStreamGetLatency
//===========================================================================================================================

uint32_t	AudioStreamGetLatency( AudioStreamRef me, OSStatus *outErr )
{
	uint32_t				result = 0;
	OSStatus				err = kNoErr;

    (void) me;
    printf("%s()\n", __FUNCTION__);

    /*
     * PLATFORM_TO_DO:
     *
     *   - Get the platform's audio latency.
     *   - Typically use the audio buffer size and sample rate to calcuate the platform's latency
     */


	*outErr = err;
	return( result );
}

//===========================================================================================================================
//	AudioStreamSetPreferredLatency
//===========================================================================================================================

OSStatus	AudioStreamSetPreferredLatency( AudioStreamRef me, uint32_t inMics )
{
    (void) me;
    (void) inMics;
    printf("%s(%d)\n", __FUNCTION__, inMics);

    /*
     * PLATFORM_TO_DO:
     *
     *   - Save the inMics preferred latency in stream specific structure.
     */

	return( kNoErr );
}

//===========================================================================================================================
//	AudioStreamSetThreadName
//===========================================================================================================================

OSStatus	AudioStreamSetThreadName( AudioStreamRef me, const char *inName )
{
	OSStatus		err;
	char *			name;

    (void) me;
	FUNCTION_TRACE();

	if( inName )
	{
		name = strdup( inName );
	}
	else
	{
		name = NULL;
	}

    /*
     * PLATFORM_TO_DO:
     *
     *   - Save the thread name in stream specific structure.
     */

	err = kNoErr;

	return( err );
}

//===========================================================================================================================
//	AudioStreamSetThreadPriority
//===========================================================================================================================

void	AudioStreamSetThreadPriority( AudioStreamRef me, int inPriority )
{
    (void) me;
    (void) inPriority;
	FUNCTION_TRACE();

    /*
     * PLATFORM_TO_DO:
     *
     *   - Save the thread priority in stream specific structure.
     */
    me->threadPriority = inPriority;
    me->hasThreadPriority = true;

}

//===========================================================================================================================
//	AudioStreamGetVolume
//===========================================================================================================================

double	AudioStreamGetVolume( AudioStreamRef me, OSStatus *outErr )
{
	double								normalizedVolume = 1.0;
	OSStatus							err = kNoErr;

	(void) me;
	FUNCTION_TRACE();
    /*
     * PLATFORM_TO_DO:
     *
     *   - Use platform specific function to get the current volume setting.
     */

	err = kNoErr;
	if( outErr )	*outErr = err;

	return( normalizedVolume );
}

//===========================================================================================================================
//	AudioStreamSetVolume
//===========================================================================================================================
static int old_volume = -1;
OSStatus	AudioStreamSetVolume( AudioStreamRef me, double inVolume )
{
	OSStatus					err;

	(void) me;
	(void) inVolume;
	FUNCTION_TRACE();
    /*
     * PLATFORM_TO_DO:
     *
     *   - Use platform specific function to set the current volume setting.
     *   - Remember the non-zero volume level - to use later during unmute
     */

	err = kNoErr;
	double f = inVolume;
	int hwVol;
	char sendBuf[32];
	hwVol=(f<-30.0)?0:(((f+30.0) *100.0 / 30.0)+0.5);
	if(hwVol>100)
		hwVol=100;

	//printf("!!!!! %s %f, old=%d, new=%d !!!!\n", __FUNCTION__, inVolume, old_volume, hwVol);
#if 0
	if( old_volume == hwVol )
	{
		printf("AudioStreamSetVolume volume not change, vol=%d\n", old_volume);
		return err;
	}
#endif
	old_volume = hwVol;
	int fd = open("/tmp/vol_time", O_RDONLY);
	struct timeval tm;
	int seconds = -1;
	char buf[64];
	if( fd >= 0 )
	{
		memset((void *)buf, 0, sizeof(buf));
		memset((void *)&tm, 0, sizeof(tm));
		gettimeofday(&tm, NULL);
		if (read(fd, buf, sizeof(buf)) > 0 )
		{
			seconds = tm.tv_sec - atoi(buf);
		}
		close(fd);
	}

	if( seconds >= 0 && seconds <= 3 )
	{
		printf("%s do not set volume back, seconds=%d\n", __FUNCTION__, seconds);
		return (err);
	}
#if 0
	sprintf(sendBuf,"setPlayerCmd:vol:%d", hwVol);
	system("echo 1 > /tmp/vol_changed_by_airplay");
	SocketClientReadWriteMsg("/tmp/Requesta01controller",sendBuf,strlen(sendBuf),NULL,NULL,0);
#else
	memset((void *)sendBuf, 0, sizeof(sendBuf));
	//sprintf(sendBuf, "GNOTIFY=AIRPLAY_VOLUME%d", hwVol);
    sprintf(sendBuf,"setPlayerCmd:airplay_vol:%d", hwVol);
	SocketClientReadWriteMsg("/tmp/Requesta01controller",sendBuf,strlen(sendBuf),NULL,NULL,0);
#endif
	return( err );
}

//===========================================================================================================================
//	AudioStreamPrepare
//===========================================================================================================================

OSStatus	AudioStreamPrepare( AudioStreamRef me )
{
	OSStatus				err = kNoErr;

    (void) me;
	FUNCTION_TRACE();

    /*
     * PLATFORM_TO_DO:
     *
     *   - Validate Audio Format (ex. endianness, sign, bitsPerSample etc) and return kUnsupportedErr if not supported.
     *   - Setup platform's audio engine and configure the various audio settings.
     *      + Number of Channels
     *      + Bits per sample
     *      + Sample rate
     *      + endianness
     *      + sign
     *      + specified latency
     *
     */

	return( err );
}

//===========================================================================================================================
//	AudioStreamStart
//===========================================================================================================================

OSStatus	AudioStreamStart( AudioStreamRef me )
{
	OSStatus		err = kNoErr;

    (void) me;
	FUNCTION_TRACE();
    /*
     * PLATFORM_TO_DO:
     *
     *   - Create a new thread or use an existing thread for this audio stream
     *   - When starting a thread for this audio stream, the following must be done if needed:
     *      + Setup inter thread communication mechanisms like socket, pipe or common memory.
     *      + Set earlier specified thread name thread priority.
     *
     *   - Kickoff platform's audio infrastructure to start the audio rendering.
     *
     *   - Loop in the thread (until stream stops) calling previously set Audio Callback Function to retrieve audio samples.
     *   - Render retrieved audio samples
     *
     */
	pthread_mutex_lock(&airplay_client_mutex);
	need_to_stop = 0;
	if( airplay_client <= 1 )
	{
		alsa_is_ready = 1;
		pthread_create(&audio_stream_callback_ptr, NULL, &audio_stream_callback, me);
		pthread_detach(audio_stream_callback_ptr);

#if RINGBUF_ENABLE
		pthread_create(&audio_stream_play_ptr, NULL, &audio_stream_play_thread, NULL);
		pthread_detach(audio_stream_play_ptr);
#endif
	}
	else
	{
		printf("== audio stream thread already created! ==\n");
	}
	pthread_mutex_unlock(&airplay_client_mutex);

	return( err );
}

//===========================================================================================================================
//	AudioStreamStop
//===========================================================================================================================

void	AudioStreamStop( AudioStreamRef me, Boolean inDrain )
{

    (void) me;
    (void) inDrain;
	FUNCTION_TRACE();
	pthread_mutex_lock(&airplay_client_mutex);
	if( --airplay_client == 0 )
	{
	need_to_stop = 1;
	sleep(1);

    /*
     * PLATFORM_TO_DO:
     *
     *   - Notify the audio thread to stop rendering.
     *   - Wait for the thread to finish.
     *      + If killing the thread, ensure join is done here.
     *
     *   - Shutdown/reset platform's audio infrastructure
     *   - cleanup buffers etc used for this stream.
     *
     */
	audio_deinit();
	SocketClientReadWriteMsg("/tmp/Requesta01controller","AIRPLAY_STOP",strlen("AIRPLAY_STOP"),NULL,NULL,0);
	CFRelease( me );
	alsa_is_ready = 0;
	}
	else
	{
		printf("==%s another client came in, do not stop ==\n", __FUNCTION__);
		audio_deinit();
		CFRelease( me );
		CreateAlsaPipe();
		alsa_need_to_wait = 0;
		alsa_is_ready = 1;
	}
	pthread_mutex_unlock(&airplay_client_mutex);
}


//===========================================================================================================================
//	AudioStreamSetVarispeedRate
//===========================================================================================================================

OSStatus	AudioStreamSetVarispeedRate( AudioStreamRef inStream, double inHz )
{
    printf("%s(): inStream=%p inHz=%f\n", __FUNCTION__, inStream, inHz);

    return( kNoErr );
}


#if 0
void audio_reset(void)
{
	pthread_mutex_lock(&alsa_mutex);
	if (alsa_handle)
	{
		snd_pcm_drop(alsa_handle);
		snd_pcm_prepare(alsa_handle);
	}
	pthread_mutex_unlock(&alsa_mutex);
}
#endif


/* recover from an alsa exception */
static inline int alsa_error_recovery( int err)
{
	int i = 0;
	if (err == -EPIPE) {
		/* FIXME: underrun length detection */
		//printf("underrun, restarting...\n");
		/* output buffer underrun */
		err = snd_pcm_prepare(alsa_handle);
		if (err < 0){
			return err;
		}
		usleep(1000);
	} else if (err == -ESTRPIPE) {
		/* application was suspended, wait until suspend flag clears */
		printf("underrun, snd_pcm_resume\n");
		while ((err = snd_pcm_resume(alsa_handle)) == -EAGAIN)
		{
			usleep(1000);
			if( ++i >= 3 )
				break;
		}

		if (err < 0) {
			/* unable to wake up pcm device, restart it */
			err = snd_pcm_prepare(alsa_handle);
			if (err < 0)
				return err;
		}
		return 0;
	}

	/* error isn't recoverable */
	return err;
}

int audio_play(char* outbuf, int samples, void* priv_data)
{
   	int len = samples;
	char *ptr = (char *) outbuf;
	int stop = 0;
	(void)priv_data;

	if (!alsa_handle)
		return -1;

	while( len > 0 )
	{
		int err = snd_pcm_writei(alsa_handle, ptr, len);
		if ( err < 0)
		{
			//printf("!!%s snd_pcm_writei return %d, try to recover\n", __FUNCTION__, err);
			pthread_mutex_lock(&airplay_client_mutex);
			stop = (need_to_stop || alsa_need_to_wait || !alsa_is_ready)?1:0;
			pthread_mutex_unlock(&airplay_client_mutex);
			if ( stop )
			{
				printf("%s need_to_stop=%d, alsa_need_to_wait=%d, alsa_is_ready=%d\n",
					__FUNCTION__, need_to_stop, alsa_need_to_wait, alsa_is_ready);
				return -2;
			}
			if (err == -EAGAIN || err == -EINTR) continue;

			err = alsa_error_recovery( err);
			if (err < 0) {
					//printf("write error: %s\n", snd_strerror(err));
					return err;
			}else
				continue;
		}
		len -= err;
		ptr += (err<<2);
	}
	return 0;
}

extern long long tickSincePowerOn(void);
int audio_init(int sampling_rate)
{
	int rc, dir = 0;
	snd_pcm_uframes_t frames = 32;
	unsigned int    buffer_time=AO_ALSA_BUFFER_TIME;
	pthread_mutex_init(&alsa_mutex, NULL);
#if ENABLE_RING_BUFFER
	pthread_mutex_init(&alsa_ringbuf_mutex);
#endif
	pthread_mutex_lock(&alsa_mutex);
#ifdef AUDIO_SRC_OUTPUT
	int i;
	long long ts1 = tickSincePowerOn();
	for(i=0; i<30; ++i)
	{
		if( is_airplay_ready() == 1 )
			break;
		usleep(100000);
	}
	printf("====%s wait %lld ms is_airplay_ready=%d i=%d\n", __FUNCTION__, tickSincePowerOn()-ts1, is_airplay_ready(), i);
	updateDmixConfDefault();
#endif
	rc = snd_pcm_open(&alsa_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (rc < 0)
	{
		fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
		return rc;
	}

#if	!defined(PLATFORM_INGENIC)
    	snd_pcm_info_set(alsa_handle, SNDSRC_AIRPLAY);
#endif
	snd_pcm_hw_params_alloca(&alsa_params);
	snd_pcm_hw_params_any(alsa_handle, alsa_params);
	snd_pcm_hw_params_set_access(alsa_handle, alsa_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(alsa_handle, alsa_params, SND_PCM_FORMAT_S16);
	snd_pcm_hw_params_set_channels(alsa_handle, alsa_params, NUM_CHANNELS);
	snd_pcm_hw_params_set_rate_near(alsa_handle, alsa_params, (unsigned int *)&sampling_rate, &dir);
	snd_pcm_hw_params_set_period_size_near(alsa_handle, alsa_params, &frames, &dir);
	rc = snd_pcm_hw_params_set_buffer_time_near(alsa_handle, alsa_params, &buffer_time, 0);

	//printf("%s frames=%d, rc=%d buffer_time=%d\n", __FUNCTION__, frames, rc, buffer_time);
	rc = snd_pcm_hw_params(alsa_handle, alsa_params);
	if (rc < 0)
	{
		fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
		return rc;
	}

	snd_pcm_sw_params_t *sw_params;
	snd_pcm_hw_params_get_buffer_size (alsa_params, &frames);
	//printf("snd_pcm_hw_params_get_buffer_size =%d\n", frames);
	snd_pcm_sw_params_alloca (&sw_params);
	snd_pcm_sw_params_current (alsa_handle, sw_params);
	int bytes = AUDIO_BUFFER_SIZE; //352*4*16; // sync with audio driver, one period size ?
	snd_pcm_sw_params_set_start_threshold(alsa_handle, sw_params, bytes);
	//printf("snd_pcm_sw_params_set_start_threshold %d\n", bytes);
	pthread_mutex_unlock(&alsa_mutex);
	return 0;
}

static void audio_deinit(void)
{
	printf("%s %d\n", __FUNCTION__, __LINE__);
	pthread_cancel(audio_stream_callback_ptr);
	pthread_join(audio_stream_callback_ptr, NULL);

	if (alsa_handle)
	{
		//snd_pcm_drain(alsa_handle);
		snd_pcm_close(alsa_handle);
		alsa_handle = NULL;
		fprintf(stderr, "de-init alsa done!=%d\n", (int)alsa_handle);
	}
#if RINGBUF_ENABLE
	pthread_cancel(audio_stream_play_ptr);
	pthread_join(audio_stream_play_ptr, NULL);

	if( ringbuf.pbuf[0] )
	{
		free(ringbuf.pbuf[0]);
		ringbuf.pbuf[0] = NULL;
	}
	memset((void *)&ringbuf, 0, sizeof(ringbuf));
	pthread_mutex_destroy(&alsa_ringbuf_mutex);
#else
	if( pBuffer )
	{
		free(pBuffer);
		pBuffer = NULL;
	}
#endif

	pthread_mutex_destroy(&alsa_mutex);
}

#include "wm_util.h"
extern int PLAYER_MODE_ISWIFI(WIIMU_CONTEXT* shm, int mode);
int platform_get_playback_allowed(void)
{
// if manual input switch enabled and current input is not wifi, need to notify clients to prevent playing
#ifdef MANUAL_INPUT_SWITCH_ENABLE
	WIIMU_CONTEXT* g_wiimu_shm = WiimuContextGet();
	if( g_wiimu_shm && g_wiimu_shm->play_mode && !PLAYER_MODE_ISWIFI(g_wiimu_shm, g_wiimu_shm->play_mode))
	{
		printf("===%s play_mode=%d, playmode is not wifi, prevent playing\n", __FUNCTION__, g_wiimu_shm->play_mode);
		return 1; // should return busy ?
	}
#endif
	return 1;
}

int is_airplay_ready(void)
{
	WIIMU_CONTEXT* g_wiimu_shm = WiimuContextGet();
	if( g_wiimu_shm )
		return g_wiimu_shm->airplay_in_play;
	return -1;
}


