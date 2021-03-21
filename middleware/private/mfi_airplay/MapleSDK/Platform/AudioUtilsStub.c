/*
	Copyright (C) 2012-2018 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
 */

#include "AudioUtils.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>

#include "CommonServices.h"
#include "PrintFUtils.h"
#include "ThreadUtils.h"
#include "TickUtils.h"
#ifdef LINKPLAY
#include <sys/shm.h>
#include <alsa/asoundlib.h>
#include "AudioPlay.h"
#include "wm_util.h"
#include "mv_message.h"
#endif

#define MAX_WAIT_PVM_TIMEOUT 5  /* ms */

#include CF_HEADER
#include LIBDISPATCH_HEADER

#include CF_RUNTIME_HEADER

#if defined(POST_AUDIO_PROCESS)

static POST_AUDIO_IPC_SHM_S *kPostAudioIpcShm = NULL;

extern POST_AUDIO_IPC_SHM_S *GetPostAudioIPCShm(void);
extern void ReleasePostAudioIPCShm(void *shm);
extern uint64_t ReadPostAudioShmDataAndCaculateDelayTs(POST_AUDIO_IPC_SHM_S *post_audio_ipc_shm, uint64_t tsNow, uint32_t mSampleRate);
#endif


#ifdef A98_BELKIN_MODULE
#define PLUGIN_FRAME_ALIGN_SIZE 128 //set in alsa conf.
#define PLUGIN_CONSUME_SIZE 1024 //Must be a multiple of frame align size
extern wm_circle_buf_ptr gAirPlayCircluBuffer;
#endif

//===========================================================================================================================
//	AudioStream
//===========================================================================================================================

typedef struct AudioStreamPrivate* AudioStreamImpRef;
struct AudioStreamPrivate {
    CFRuntimeBase base; // CF type info. Must be first.
    Boolean prepared; // True if AudioStreamPrepare has been called (and stop hasn't yet).
    Boolean keepRunning; // True if the Audiothread is to keep running.
    pthread_t audioThread;
    pthread_t* audioThreadPtr;
    int threadPriority; // Priority to run threads.
#if LINKPLAY
    unsigned char outputBuffer[MAX_AUDIO_OUTPUT_BUFF_SIZE];
    uint32_t outputMaxLen;
    uint32_t outputSampleTime;
    snd_pcm_t *outputPCMHandle;

#ifdef A98_BELKIN_MODULE
    pthread_t pluginPlayThread;
#endif

#endif
    void* delegateContext; // Context for the session delegate
    AudioStreamOutputCallback_f outputCallbackPtr; // Function to call to read audio to output.
    void* outputCallbackCtx; // Context to pass to audio output callback function.
    AudioStreamBasicDescription format; // Format of the audio data.
    uint32_t preferredLatencyMics; // Max latency the app can tolerate.
    uint32_t streamType; // AirPlay Stream type (e.g. main, alt).
};

#define _AudioStreamGetImp(STREAM) (STREAM)

static void _AudioStreamGetTypeID(void* inContext);
static void _AudioStreamFinalize(CFTypeRef inCF);
static void* _AudioThreadFn(void* inArg);

static dispatch_once_t gAudioStreamInitOnce = 0;
static CFTypeID gAudioStreamTypeID = _kCFRuntimeNotATypeID;
static const CFRuntimeClass kAudioStreamClass = {
    0, // version
    "AudioStream", // className
    NULL, // init
    NULL, // copy
    _AudioStreamFinalize, // finalize
    NULL, // equal -- NULL means pointer equality.
    NULL, // hash  -- NULL means pointer hash.
    NULL, // copyFormattingDesc
    NULL, // copyDebugDesc
    NULL, // reclaim
    NULL // refcount
};

Boolean kPerformSkewAdjustRightAway = false;
//===========================================================================================================================
//	Logging
//===========================================================================================================================

ulog_define(AudioStream, kLogLevelNotice, kLogFlags_Default, "AudioStream", NULL);
#define as_dlog(LEVEL, ...) dlogc(&log_category_from_name(AudioStream), (LEVEL), __VA_ARGS__)
#define as_ulog(LEVEL, ...) ulog(&log_category_from_name(AudioStream), (LEVEL), __VA_ARGS__)

//===========================================================================================================================
//	AudioStreamGetTypeID
//===========================================================================================================================

CFTypeID AudioStreamGetTypeID(void)
{
    dispatch_once_f(&gAudioStreamInitOnce, NULL, _AudioStreamGetTypeID);
    return (gAudioStreamTypeID);
}

static void _AudioStreamGetTypeID(void* inContext)
{
    (void)inContext;

    gAudioStreamTypeID = _CFRuntimeRegisterClass(&kAudioStreamClass);
    check(gAudioStreamTypeID != _kCFRuntimeNotATypeID);
}

//===========================================================================================================================
//	AudioStreamCreate
//===========================================================================================================================

OSStatus AudioStreamCreate(AudioStreamRef* outStream)
{
    OSStatus err;
    AudioStreamRef me;
    size_t extraLen;

    extraLen = sizeof(*me) - sizeof(me->base);
    me = (AudioStreamRef)_CFRuntimeCreateInstance(NULL, AudioStreamGetTypeID(), (CFIndex)extraLen, NULL);
    require_action(me, exit, err = kNoMemoryErr);
    memset(((uint8_t*)me) + sizeof(me->base), 0, extraLen);

    // $$$ TODO: Other initialization goes here.
    // This function is only called when AudioUtils is compiled into the AirPlay library.

    *outStream = me;
    me = NULL;
    err = kNoErr;

exit:
    CFReleaseNullSafe(me);
    return (err);
}

//===========================================================================================================================
//	_AudioStreamFinalize
//===========================================================================================================================

static void _AudioStreamFinalize(CFTypeRef inCF)
{
    AudioStreamRef const me = (AudioStreamRef)inCF;
    // $$$ TODO: Last chance to free any resources allocated by this object.
    // This function is called when AudioUtils is compiled into the AirPlay library, when the retain count of an AudioStream
    // object goes to zero.
    AudioStreamStop(me, false);
}

//===========================================================================================================================
//	AudioStreamSetInputCallback
//===========================================================================================================================

void AudioStreamSetInputCallback(AudioStreamRef inStream, AudioStreamInputCallback_f inFunc, void* inContext)
{
    (void)inStream;
    (void)inFunc;
    (void)inContext;
}

//===========================================================================================================================
//	AudioStreamSetOutputCallback
//===========================================================================================================================

void AudioStreamSetOutputCallback(AudioStreamRef inStream, AudioStreamOutputCallback_f inFunc, void* inContext)
{
    AudioStreamImpRef const me = _AudioStreamGetImp(inStream);

    me->outputCallbackPtr = inFunc;
    me->outputCallbackCtx = inContext;
}

//===========================================================================================================================
//	_AudioStreamCopyProperty
//===========================================================================================================================

CFTypeRef
_AudioStreamCopyProperty(
    CFTypeRef inObject,
    CFStringRef inProperty,
    OSStatus* outErr)
{
    AudioStreamImpRef const me = _AudioStreamGetImp((AudioStreamRef)inObject);
    OSStatus err;
    CFTypeRef value = NULL;

    if (0) {
    }

    // AudioType
    else if (CFEqual(inProperty, kAudioStreamProperty_AudioType)) {
        // $$$ TODO: Return the current audio type.
    }

    // Format

    else if (CFEqual(inProperty, kAudioStreamProperty_Format)) {
        value = CFDataCreate(NULL, (const uint8_t*)&me->format, sizeof(me->format));
        require_action(value, exit, err = kNoMemoryErr);
    }

    // Input

    else if (CFEqual(inProperty, kAudioStreamProperty_Input)) {
    }

    // PreferredLatency

    else if (CFEqual(inProperty, kAudioStreamProperty_PreferredLatency)) {
        value = CFNumberCreateInt64(me->preferredLatencyMics);
        require_action(value, exit, err = kNoMemoryErr);
    }

    // StreamType

    else if (CFEqual(inProperty, kAudioStreamProperty_StreamType)) {
        value = CFNumberCreateInt64(me->streamType);
        require_action(value, exit, err = kNoMemoryErr);
    }

    // ThreadName

    else if (CFEqual(inProperty, kAudioStreamProperty_ThreadName)) {
        // $$$ TODO: If your implementation uses a helper thread, return its name here.
    }

    // ThreadPriority

    else if (CFEqual(inProperty, kAudioStreamProperty_ThreadPriority)) {
        // $$$ TODO: If your implementation uses a helper thread, return its priority here.
    }

    // Other

    else {
        err = kNotHandledErr;
        goto exit;
    }
    err = kNoErr;

exit:
    if (outErr)
        *outErr = err;
    return (value);
}

//===========================================================================================================================
//	_AudioStreamSetProperty
//===========================================================================================================================

OSStatus
_AudioStreamSetProperty(
    CFTypeRef inObject,
    CFStringRef inProperty,
    CFTypeRef inValue)
{
    AudioStreamImpRef const me = _AudioStreamGetImp((AudioStreamRef)inObject);
    OSStatus err;

    if (0) {
    }

    // Volume

    else if (CFEqual(inProperty, kAudioStreamProperty_Volume)) {
#ifdef LINKPLAY            
        double dbVolume = (double)CFGetDouble(inValue, &err);
        require_noerr(err, exit);
        
        as_ulog(kLogLevelNotice, "sync volume [%f] db to receiver.\n", dbVolume);
        audio_set_volume(dbVolume);
#endif
    }

    // AudioType

    else if (CFEqual(inProperty, kAudioStreamProperty_AudioType)) {
        // $$$ TODO: Use the audio type to enable certain types of audio processing.
        // For example, if the audio type is "telephony", echo cancellation should be enabled;
        // if the audio type is "speech recognition", non-linear processing algorithms should be disabled.
    }

    // Format

    else if (CFEqual(inProperty, kAudioStreamProperty_Format)) {
        CFGetData(inValue, &me->format, sizeof(me->format), NULL, &err);
        require_noerr(err, exit);
    }
    // Input

    else if (CFEqual(inProperty, kAudioStreamProperty_Input)) {
    }
    // PreferredLatency

    else if (CFEqual(inProperty, kAudioStreamProperty_PreferredLatency)) {
        me->preferredLatencyMics = (uint32_t)CFGetInt64(inValue, &err);
        require_noerr(err, exit);
    }

    // StreamType

    else if (CFEqual(inProperty, kAudioStreamProperty_StreamType)) {
        me->streamType = (uint32_t)CFGetInt64(inValue, &err);
        require_noerr(err, exit);
    }

    // ThreadName

    else if (CFEqual(inProperty, kAudioStreamProperty_ThreadName)) {
        // $$$ TODO: If your implementation uses a helper thread, set the name of the thread to the string passed in
        // to this property.  See SetThreadName().
    }

    // ThreadPriority

    else if (CFEqual(inProperty, kAudioStreamProperty_ThreadPriority)) {
        // $$$ TODO: If your implementation uses a helper thread, set the priority of the thread to the string passed in
        // to this property.  See SetCurrentThreadPriority().
        me->threadPriority = (int)CFGetInt64(inValue, &err);
    }

    // Other

    else {
        as_ulog(kLogLevelNotice, "%s: Unhandled %@:%@\n", __PRETTY_FUNCTION__, inProperty, inValue);
        err = kNotHandledErr;
        goto exit;
    }
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	AudioStreamSetFormat
//===========================================================================================================================

void AudioStreamSetFormat(AudioStreamRef inStream, const AudioStreamBasicDescription* inFormat)
{
    AudioStreamImpRef const me = _AudioStreamGetImp(inStream);
    me->format = *inFormat;
}

//===========================================================================================================================
//	AudioStreamSetDelegateContext
//===========================================================================================================================

void AudioStreamSetDelegateContext(AudioStreamRef inStream, void* inContext)
{
    AudioStreamImpRef const me = _AudioStreamGetImp(inStream);
    me->delegateContext = inContext;
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//  AudioStreamPrepare
//===========================================================================================================================
extern unsigned int gSampleTimeCorrect;
OSStatus AudioStreamPrepare(AudioStreamRef inStream)
{
    AudioStreamImpRef const me = _AudioStreamGetImp(inStream);
    OSStatus err;

    // $$$ TODO: This is where the audio processing chain should be set up based on the properties previously set on the
    // AudioStream object:
    //	me->format specifies the sample rate, channel count, and bit-depth.
    //	me->input specifies whether or not the processing chain should be set up to record audio from the accessory's
    //	          microphone(s).
    // Audio output should always be prepared.
    // If the audio processing chain is successfully set up, me->prepared should be set to true.

    if (me->prepared) {
        return kNoErr;
    }
    
#ifdef LINKPLAY
    audio_client_init();
#endif

    me->prepared = true;
    me->keepRunning = true;
    err = kNoErr;

    if (err)
        AudioStreamStop(inStream, false);
    return (err);
}

//===========================================================================================================================
//	AudioStreamStart
//===========================================================================================================================

OSStatus AudioStreamStart(AudioStreamRef inStream)
{
    AudioStreamImpRef const me = _AudioStreamGetImp(inStream);
    OSStatus err;

    if (!me->prepared) {
        err = AudioStreamPrepare(inStream);
        require_noerr(err, exit);
    }

    // $$$ TODO: This is where the audio processing chain should be started.
    //
    // me->outputCallbackPtr should be invoked periodically to retrieve a continuous stream of samples to be output.
    // When calling me->outputCallbackPtr(), a buffer is provided for the caller to write into.  Equally important
    // is the inSampleTime and inHostTime arguments.  It is important that accurate { inSampleTime, inHostTime } pairs
    // be provided to the caller.  inSampleTime should be a (reasonably) current running total of the number of samples
    // that have hit the speaker since AudioStreamStart() was called.  inHostTime is the system time, in units of ticks,
    // corresponding to inSampleTime (see TickUtils.h).  This information will be returned to the controller and is
    // a key piece in allowing the controller to derive the relationship between the controller's system clock and the
    // accessory's audio (DAC) clock for A/V sync.
    //
    // If input has been requested (me->input == true), then me->inputCallbackPtr should also be invoked periodically
    // to provide a continuous stream of samples from the accessory's microphone (possibly with some processing, depending
    // on the audio type, see kAudioStreamProperty_AudioType).  If no audio samples are available for whatever reason,
    // the me->inputCallbackPtr should be called with a buffer of zeroes.

    require_action(me->audioThreadPtr == NULL, exit, err = kAlreadyInitializedErr);
    err = pthread_create(&me->audioThread, NULL, _AudioThreadFn, inStream);
    require_noerr(err, exit);
    me->audioThreadPtr = &me->audioThread;
exit:
    if (err)
        AudioStreamStop(inStream, false);
    return (err);
}

//===========================================================================================================================
//	AudioStreamStop
//===========================================================================================================================

void AudioStreamStop(AudioStreamRef inStream, Boolean inDrain)
{
    AudioStreamImpRef const me = _AudioStreamGetImp(inStream);

    // $$$ TODO: This is where the audio processing chain should be stopped, and the audio processing chain torn down.
    // When AudioStreamStop() returns, the object should return to the state similar to before AudioStreamPrepare()
    // was called, so this function is responsible for undoing any resource allocation performed in AudioStreamPrepare().
    (void)inDrain;
    
    if (me->audioThreadPtr) {
        me->prepared = false;
        me->keepRunning = false;

        pthread_join(me->audioThread, NULL);
        me->audioThreadPtr = NULL;
        
#ifdef LINKPLAY
        audio_client_deinit();
#endif        
    }
}

#if 0
#pragma mark -
#endif

static snd_pcm_sframes_t _AudioGetPcmAvailDelay(snd_pcm_t *handle, snd_pcm_sframes_t *delay)
{
#ifdef LINKPLAY
    int err = 0;

    err = snd_pcm_delay(handle, delay);

    if(err < 0) {
        return -1;
    }

#ifdef A98_BELKIN_MODULE
    if(gAirPlayCircluBuffer)
    {
        wm_circle_buf_lock(gAirPlayCircluBuffer);

        int circleBufferDelay = wm_circle_buf_readable(gAirPlayCircluBuffer);

        wm_circle_buf_unlock(gAirPlayCircluBuffer);

        circleBufferDelay /= 4;
        as_ulog(kLogLevelVerbose, "circle buffer delay [%d]\n", circleBufferDelay);

        *delay += circleBufferDelay;//MUST do delay compensation from circle buffer.
    }
#endif

    snd_pcm_sframes_t available = snd_pcm_avail_update(handle);    

#ifdef RESAMPLE_BEFORE_ALSA
    get_extra_resample_delay(&available, delay);
#endif

    return available;
#else
    int err;
    snd_pcm_status_t *status;

    snd_pcm_status_alloca(&status);
    err = snd_pcm_status(handle, status);
    if (err < 0)
        return err;

    *timestamp = UpTicks();

    *delay = snd_pcm_status_get_delay(status);
    return snd_pcm_status_get_avail(status);
#endif
}


static inline uint64_t _AudioGetNextWriteHostTimeNs(uint64_t CurrNanoTs, snd_pcm_sframes_t PcmDelay, int InPcmSampleRate,int InAlsaOutputSampleRate)
{
    uint64_t retNextWriteHostTs = 0;
    uint64_t nSampleOutLantencyNanoseconds = 0;
    
#if !defined(POST_AUDIO_PROCESS)
    (void) InAlsaOutputSampleRate;
#else
    const uint64_t fixedDelay = 22000000;// TO DO, configuration Per projects
    nSampleOutLantencyNanoseconds = read_post_audio_shm_and_caculate_delay_ts(kPostAudioIpcShm, CurrNanoTs, InAlsaOutputSampleRate) + fixedDelay;
#endif

    retNextWriteHostTs = CurrNanoTs + (nSampleOutLantencyNanoseconds + (uint64_t)PcmDelay * 1000000000LL / InPcmSampleRate) + gAirplayPrivCtrlCtx.ExtraAudioDelayMs * 1000000LL;

    return retNextWriteHostTs;
}

#ifdef A98_BELKIN_MODULE
static int _WritePluginPcm(AudioStreamImpRef me, uint8_t *bufferPtr, int frames, int bytesPerFrame);
#else
static void _AudioWriteSilence(snd_pcm_t *outputPCMHandle, unsigned char *outputBuffer, snd_pcm_sframes_t unitSize)
{
    //OSStatus err;
    snd_pcm_sframes_t n = 0, frames_unit = unitSize;

    if (!outputPCMHandle || !outputBuffer || unitSize <= 0)
        return;
    
    frames_unit = unitSize;
    
    while(frames_unit > 0) {
        n = snd_pcm_writei(outputPCMHandle, outputBuffer, frames_unit);
    
        if(n <= 0) {
            snd_pcm_recover(outputPCMHandle, n, 1);
           // as_ulog(kLogLevelTrace, "(%s,%d)### ALSA initial write error %#m -> %#m\n", __FILE__, __LINE__, (OSStatus) n, err);
            continue;
        }
    
        frames_unit -= n;
    }

}
#endif


static inline void _AudioPcmOutputPerFrames(snd_pcm_t *outputPCMHandle, uint8_t * BufferPtr, snd_pcm_sframes_t frames, uint32_t bytesPerFrame)
{
    OSStatus err = kNoErr;
    snd_pcm_sframes_t n;

    while(frames > 0) {
        n = snd_pcm_writei(outputPCMHandle, BufferPtr, frames);
#if 0
        if(n <= 0) {
            err = snd_pcm_recover(me->outputPCMHandle, n, 0);
            as_ulog(kLogLevelNotice, "(%s,%d)### ALSA drain buffer error %#m -> %#m\n", __func__, __LINE__, (OSStatus) n,
                    (OSStatus) n);
            continue;
        }
#else
        if(-EAGAIN == n) {
            snd_pcm_wait(outputPCMHandle, 1000);
            continue;
        } else if(n < 0) {
            err = snd_pcm_recover(outputPCMHandle, n, 1);
            as_ulog(kLogLevelNotice, "(%s,%d)### ALSA drain buffer error %#m -> %#m\n", __func__, __LINE__, (OSStatus) n, err);
            continue;
        }
#endif
        BufferPtr += (n * bytesPerFrame);
        frames -= n;
    }
}


static inline void _AudioPotentialAlsaReset(uint64_t CurrTs, uint64_t *TsSinceAlsaStarted, AudioStreamImpRef const me, snd_pcm_sframes_t inFrames)
{
    //Reset Alsa pipe every 1hour if next track detected.
    const uint64_t max_duration_of_reset_alsa = (60*60*1000000000LL);

    if (gAirplayPrivCtrlCtx.TrackChanged)
    {
        if ((CurrTs - (*TsSinceAlsaStarted)) >= max_duration_of_reset_alsa)
        {
            as_ulog(kLogLevelNotice, "Next track detected, reset alsa pipe.\n");
            
            gAirplayPrivCtrlCtx.TrackChanged = 0;
            kPerformSkewAdjustRightAway = 1;
            
            snd_pcm_drain(me->outputPCMHandle);
            snd_pcm_prepare(me->outputPCMHandle);
        
            *TsSinceAlsaStarted = CurrTs;//reset Ts
                                    
            bzero(me->outputBuffer, me->outputMaxLen);
#ifdef A98_BELKIN_MODULE
            _WritePluginPcm(me, me->outputBuffer, inFrames, me->format.mBytesPerFrame);
#else
            _AudioWriteSilence(me->outputPCMHandle, me->outputBuffer, inFrames);
#endif
        }
    }

}


static inline uint8_t _AudioLateFrameSkipAndSkew(uint64_t TmDiff, AudioStreamImpRef me, snd_pcm_sframes_t frames)
{
    uint8_t skip = 0;
    const uint64_t MaxExcessiveTmDiff = 50000000;//ns
    
    if (me && TmDiff >= MaxExcessiveTmDiff)
    {
        as_ulog(kLogLevelNotice, "get valid buffer takes %llu ns skip it.\n", TmDiff);
        kPerformSkewAdjustRightAway = true;
    
        //Write silence to prevent alsa pipeline error
        bzero(me->outputBuffer, me->outputMaxLen);
#ifdef A98_BELKIN_MODULE
       _WritePluginPcm(me, me->outputBuffer, frames, me->format.mBytesPerFrame);
#else
        _AudioWriteSilence(me->outputPCMHandle, me->outputBuffer, frames);
#endif
    
        me->outputSampleTime += frames;
        
        skip = 1;
    }

    return skip;
}


#ifdef A98_BELKIN_MODULE
static void *_PlugInPlayThreadFn(void *inArg)
{
    uint8_t readBuffer[PLUGIN_CONSUME_SIZE * 4] = {0};
    AudioStreamImpRef const me = _AudioStreamGetImp((AudioStreamRef)inArg);

    int bytesPerFrame = me->format.mBytesPerFrame;

    snd_pcm_t *outputPCMHandle = NULL;

    audio_client_get_alsa_handler(&outputPCMHandle);

    as_ulog(kLogLevelNotice, "AudioStreamOutput PluginPlay started.\n");

    for(;me->keepRunning;)
    {
        if (gAirPlayCircluBuffer)
        {
            wm_circle_buf_lock(gAirPlayCircluBuffer);

            int bufferReadAvail = wm_circle_buf_readable(gAirPlayCircluBuffer);

            wm_circle_buf_unlock(gAirPlayCircluBuffer);

            snd_pcm_sframes_t alsaWritable = snd_pcm_avail_update(outputPCMHandle);

            if(alsaWritable < 0){
                int err = snd_pcm_recover(me->outputPCMHandle, alsaWritable, 0);
                as_ulog(kLogLevelVerbose, "avail error:%d, %s\n", err, snd_strerror(err));
                usleep(10000);
                continue;
            }

            bufferReadAvail /= bytesPerFrame;

            //as_ulog(kLogLevelNotice, "bufferReadAvail [%d] alsaWritable [%d] res [%d].\n", bufferReadAvail, alsaWritable, alsaWritable % 64 );

            if(alsaWritable > PLUGIN_FRAME_ALIGN_SIZE && alsaWritable % PLUGIN_FRAME_ALIGN_SIZE != 0)
                alsaWritable -= (alsaWritable % PLUGIN_FRAME_ALIGN_SIZE);

            if(bufferReadAvail > PLUGIN_FRAME_ALIGN_SIZE && bufferReadAvail % PLUGIN_FRAME_ALIGN_SIZE != 0)
                bufferReadAvail -= (bufferReadAvail % PLUGIN_FRAME_ALIGN_SIZE);
                
            if(bufferReadAvail >= PLUGIN_FRAME_ALIGN_SIZE && alsaWritable >= PLUGIN_CONSUME_SIZE)
            {
                int readLen = 0;

                if(bufferReadAvail >= PLUGIN_CONSUME_SIZE)
                    bufferReadAvail = PLUGIN_CONSUME_SIZE;

                readLen = Min(bufferReadAvail, alsaWritable);

                //as_ulog(kLogLevelNotice, "readLen [%d] bufferReadAvail [%d] alsaWritable [%d].\n", readLen, bufferReadAvail, alsaWritable);

                wm_circle_buf_lock(gAirPlayCircluBuffer);

                wm_circle_buf_read(gAirPlayCircluBuffer, (char*)readBuffer, readLen * bytesPerFrame);

                wm_circle_buf_unlock(gAirPlayCircluBuffer);

                _AudioPcmOutputPerFrames(outputPCMHandle, readBuffer, readLen, bytesPerFrame);
            }
            else
            {
                usleep(1000);
            }
        }
    }

    as_ulog(kLogLevelNotice, "AudioStreamOutput PluginPlay stopped.\n");

    return NULL;
}

static int _WritePluginPcm(AudioStreamImpRef me, uint8_t *bufferPtr, int frames, int bytesPerFrame)
{
    if (gAirPlayCircluBuffer)
    {
        int writeAvail = 0;

RETRY:

        if(me->keepRunning == false)
          return 0;
    
        wm_circle_buf_lock(gAirPlayCircluBuffer);

        writeAvail = wm_circle_buf_writable(gAirPlayCircluBuffer);

        wm_circle_buf_unlock(gAirPlayCircluBuffer);

        writeAvail /= bytesPerFrame;

        as_ulog(kLogLevelVerbose, "circle buffer writeAvail [%d] frames [%d]\n", writeAvail, frames);

        if (writeAvail >= frames)
        {
            wm_circle_buf_lock(gAirPlayCircluBuffer);
        
            wm_circle_buf_write(gAirPlayCircluBuffer, (char*)bufferPtr, frames * bytesPerFrame);
            
            wm_circle_buf_unlock(gAirPlayCircluBuffer);
        }
        else
        {        
            usleep(500);
            goto RETRY;
        }
    }

    return 0;
}
#endif


static void * _AudioThreadFn(void *inArg)
{
    AudioStreamImpRef const me = _AudioStreamGetImp((AudioStreamRef)inArg);

#ifdef LINKPLAY
    me->format.mBytesPerFrame = 4;/* 2 channel + (16bit/8)bytes */
    me->outputMaxLen = MAX_AUDIO_OUTPUT_BUFF_SIZE;
    me->outputSampleTime = 0;
    audio_client_get_alsa_handler(&me->outputPCMHandle);

#ifdef LOG_TIMESTAMP
    FILE *log_tm_fp = fopen("/tmp/airplay_timestamp.log", "w+");

    if (log_tm_fp == NULL){
         as_ulog(kLogLevelNotice, "log airplay_timestamp failed.\n");
    }

    fprintf(log_tm_fp, "%s","frames, delay, outputSampleTime, hostTime\n");
#endif    
#endif
    int32_t fixedVolume = 1500;
    int result = 1;
    snd_pcm_sframes_t frames;
    snd_pcm_sframes_t delay;
    uint64_t hostTime;
    OSStatus err = kNoErr;
    size_t const bytesPerUnit = me->format.mBytesPerFrame;
    uint8_t * ptr = NULL;

    uint64_t AlsaOutputSampleRate = me->format.mSampleRate;
    uint64_t TsSinceAlsaStarted = UpTicks();

    as_ulog(kLogLevelNotice, "AudioStreamOutputThread started.\n");

    SetThreadName("AudioStreamOutputThread");

#if defined(POST_AUDIO_PROCESS)
    kPostAudioIpcShm = get_post_audio_ipc_shm();
    AlsaOutputSampleRate = 48000;
#endif

#ifdef A98_BELKIN_MODULE
    if(NULL == gAirPlayCircluBuffer && (0 != wm_circle_buffer_shm_init(&gAirPlayCircluBuffer)))
    {
        as_ulog(kLogLevelError, "AudioStreamOutput wm_circle_buffer_shm_init failed !!\n");
        return NULL;
    }

    err = pthread_create(&me->pluginPlayThread, NULL, _PlugInPlayThreadFn, inArg);

    if(err != kNoErr)
    {
        as_ulog(kLogLevelError, "AudioStreamOutput PlugInPlayback start failed !!\n");
        return NULL;
    }
#endif

    gAirplayPrivCtrlCtx.AudioOutputPlaybackThreadExited = false;

    if (me->threadPriority > 0)
        SetCurrentThreadPriority(me->threadPriority);

    bzero(me->outputBuffer, me->outputMaxLen);

    frames = me->outputMaxLen / me->format.mBytesPerFrame;

    me->outputSampleTime = 0;

    for(; me->keepRunning;) {
        snd_pcm_state_t pcm_state = snd_pcm_state(me->outputPCMHandle);
    
        if (pcm_state == SND_PCM_STATE_RUNNING || pcm_state == SND_PCM_STATE_DRAINING)
        {
            result = 1;
        }
        else
        {
            bzero(me->outputBuffer, me->outputMaxLen);

#ifdef A98_BELKIN_MODULE
            _WritePluginPcm(me, me->outputBuffer, CONFIG_AUDIO_PERIOD_SIZE, me->format.mBytesPerFrame);
#else
            _AudioWriteSilence(me->outputPCMHandle, me->outputBuffer, CONFIG_AUDIO_PERIOD_SIZE);
#endif
            me->outputSampleTime += CONFIG_AUDIO_PERIOD_SIZE;
            result = 0;
        }

        if(me->keepRunning == false) {
            break;
        }

        if(result) 
        {
            frames = _AudioGetPcmAvailDelay(me->outputPCMHandle, &delay);

            if(frames <= 0) {
                pcm_state = snd_pcm_state(me->outputPCMHandle);

                if (pcm_state != SND_PCM_STATE_RUNNING && pcm_state != SND_PCM_STATE_DRAINING)
                {
                    err = snd_pcm_recover(me->outputPCMHandle, frames, 0);
                    bzero(me->outputBuffer, me->outputMaxLen);
#ifdef A98_BELKIN_MODULE
                    _WritePluginPcm(me, me->outputBuffer, CONFIG_AUDIO_PERIOD_SIZE, me->format.mBytesPerFrame);
#else
                    _AudioWriteSilence(me->outputPCMHandle, me->outputBuffer, CONFIG_AUDIO_PERIOD_SIZE);
#endif
                    me->outputSampleTime += CONFIG_AUDIO_PERIOD_SIZE;
                    as_ulog(kLogLevelNotice, "add _AudioWriteSilence (%s,%d)### ALSA output avail error %#m -> %#m,snd_strerror is %s\n", __func__, __LINE__, (OSStatus) frames,err, snd_strerror(err));
                }
                else
                {
                    //buffer is full,wait for pcm become available
                    snd_pcm_wait(me->outputPCMHandle, MAX_WAIT_PVM_TIMEOUT);
                }

                continue;
            }

            ptr = me->outputBuffer;
            frames = Min(frames, (snd_pcm_sframes_t)(me->outputMaxLen / bytesPerUnit));

            uint64_t T1 = UpTicks();

            hostTime = _AudioGetNextWriteHostTimeNs(T1, delay, me->format.mSampleRate, AlsaOutputSampleRate);

            me->outputCallbackPtr(me->outputSampleTime, hostTime, ptr, (size_t)(frames * bytesPerUnit), me->outputCallbackCtx);

            uint64_t T2 = UpTicks();

            _AudioPotentialAlsaReset(T2, &TsSinceAlsaStarted, me, frames);

            /* stop write late buffer and skew to prevent unsync. */
            if (_AudioLateFrameSkipAndSkew((T2 - T1), me, frames))
            {
                me->outputSampleTime += frames;
                continue;
            }
            else if(gAirplayPrivCtrlCtx.RequestForceSkewAdjust){
                as_ulog(kLogLevelNotice, "request force skew adjust per outside msg.\n");
                kPerformSkewAdjustRightAway = true;
                gAirplayPrivCtrlCtx.RequestForceSkewAdjust = false;
            }

            me->outputSampleTime += frames;

            audio_client_process_fade_and_silence(&ptr, frames, me->format.mBytesPerFrame, &fixedVolume);

#ifdef RESAMPLE_BEFORE_ALSA
            do_resample_before_alsa(&ptr, &frames);
#endif

#ifdef A98_BELKIN_MODULE
            _WritePluginPcm(me, ptr, frames, me->format.mBytesPerFrame);
#else
            // drain the buffer we just read
            _AudioPcmOutputPerFrames(me->outputPCMHandle, ptr, frames, me->format.mBytesPerFrame);
#endif
        }
    }

    as_ulog(kLogLevelNotice, "AudioStreamOutputThread stopped.\n");

    gAirplayPrivCtrlCtx.AudioOutputPlaybackThreadExited = true;

#ifdef A98_BELKIN_MODULE
    pthread_join(me->pluginPlayThread, NULL);
    wm_circle_buf_reset(gAirPlayCircluBuffer);
#endif

#if defined(POST_AUDIO_PROCESS)
    if(kPostAudioIpcShm)
    {
        release_post_audio_ipc_shm((void*)kPostAudioIpcShm);
        kPostAudioIpcShm = NULL;
    }
#endif

#ifdef LOG_TIMESTAMP
    if(log_tm_fp) {
        fclose(log_tm_fp);
    }
#endif

    return NULL;
}


