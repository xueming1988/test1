/*
	Copyright (C) 2010-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/
/*!
	@header		AudioStream API
	@discussion	Provides APIs for audio streams.
*/

#ifndef __AudioUtils_h__
#define __AudioUtils_h__

#include "AirPlayCommon.h"
#include "CFUtils.h"
#include "CommonServices.h"

#include CF_HEADER
#include COREAUDIO_HEADER
#include LIBDISPATCH_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark -
#pragma mark == AudioStream ==
#endif

//===========================================================================================================================
/*!	@group		AudioStream
	@abstract	Plays or records a stream of audio.
	@discussion
	
	The general flow is that the audio stream is created; configured with input or output flags, format of the samples, 
	preferred latency, audio callback, etc.; and then started. When started, the implementation will generally set up 
	2 or 3 buffers of audio samples (often silence at first). The size of the buffer depends on the preferred latency, 
	but is generally less than 50 ms. Smaller and fewer buffers reduces latency since the minimum latency is the number
	of samples in each buffer times the number of buffers plus any latency introduced by the driver and/or hardware.
	Smaller buffers can increase CPU usage because it needs to wake up the CPU to supply data more frequently. It also 
	increases the likelihood of the hardware running dry and dropping audio if there are thread scheduling delays that
	prevent the audio thread from running in time to provide new samples to the hardware.
	
	As each buffer completes in the hardware, it reuses the buffer by calling the AudioStream callback to provide more 
	data. It then re-schedules the buffer with the audio hardware to play when the current buffer is finished. For systems
	that don't provide direct access to the audio hardware, it may use another mechanism, such as a file descriptor. The
	AudioStream implementation waits for the file descriptor to become writable, calls the AudioStream callback to fill 
	audio data into a buffer, and then writes that buffer to the file descriptor. This process repeats as long as the 
	audio stream is started. The audio driver wakes up the audio thread when buffers complete by indicating the file 
	descriptor is writable.
	
	Timing is important for proper synchronization of audio. When the AudioStream callback is invoked, it provides the 
	sample number for the first sample to be filled in and a host time in the future for when the first sample will be
	heard. The host time should be as close as possible to when the sample will really be heard. If the hardware or driver
	supports it, the sample time would come directly from the hardware's playback position within the buffer. This can 
	be correlated with a host time by getting an accurate host time at the beginning of the audio buffer the hardware is
	playing.
*/

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamGetTypeID
	@abstract	Gets the CF type ID of all AudioStream objects.
*/
CFTypeID AudioStreamGetTypeID(void);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamCreate
	@abstract	Creates a new AudioStream.
*/
typedef struct AudioStreamPrivate* AudioStreamRef;

OSStatus AudioStreamCreate(AudioStreamRef* outStream);
#define AudioStreamForget(X)              \
    do {                                  \
        if (*(X)) {                       \
            AudioStreamStop(*(X), false); \
            CFRelease(*(X));              \
            *(X) = NULL;                  \
        }                                 \
    } while (0)

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamInitialize
	@abstract	Initialize function for DLL-based AudioStreams.
	@discussion	Called when AudioStream is created to give the DLL a chance to initialize itself.
*/
OSStatus AudioStreamInitialize(AudioStreamRef inStream);
typedef OSStatus (*AudioStreamInitialize_f)(AudioStreamRef inStream);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamFinalize
	@abstract	Finalize function for DLL-based AudioStreams.
	@discussion	Called when AudioStream is finalized to give the DLL a chance to finalize itself.
*/
void AudioStreamFinalize(AudioStreamRef inStream);
typedef void (*AudioStreamFinalize_f)(AudioStreamRef inStream);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamGetContext / AudioStreamSetContext
	@abstract	Gets/sets a context pointer for DLL implementors to access their internal state.
*/
void* AudioStreamGetContext(AudioStreamRef inStream);
void AudioStreamSetContext(AudioStreamRef inStream, void* inContext);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamSetInputCallback / AudioStreamSetOutputCallback
	@abstract	Sets a function to be called for audio input or output (depending on the direction of the stream).
	
	@param		inSampleTime	Sample number for the first sample in the buffer. This sample number should increment by 
								the number of samples in each buffer. If there is a gap in the sample number sequence, it
								means something caused an audible glitch, such as software not being able to fill buffers
								fast enough (e.g. something held off the audio thread for too long). The callback will 
								need to handle this by skipping samples in the gap and fill from the specified sample time.
	
	@param		inHost			UpTicks()-compatible timestamp for when the first sample the buffer will be heard. This 
								takes into consideration software and hardware buffer and other latencies to provide a 
								host time that's as accurate as possible.
	
	@param		inBuffer		For input, the buffer contains the audio received from the hardware (e.g. microphone).
								For output, the callback is expected to write audio data to be played to this buffer.
	
	@param		inLen			Number of bytes in the buffer.
	
	@param		inContext		Pointer that was supplied when callback was set.
								
*/
typedef void (*AudioStreamInputCallback_f)(
    uint32_t inSampleTime,
    uint64_t inHostTime,
    const void* inBuffer,
    size_t inLen,
    void* inContext);

void AudioStreamSetInputCallback(AudioStreamRef inStream, AudioStreamInputCallback_f inFunc, void* inContext);
typedef void (*AudioStreamSetInputCallback_f)(AudioStreamRef inStream, AudioStreamInputCallback_f inFunc, void* inContext);

typedef void (*AudioStreamOutputCallback_f)(
    uint32_t inSampleTime,
    uint64_t inHostTime,
    void* inBuffer,
    size_t inLen,
    void* inContext);

void AudioStreamSetOutputCallback(AudioStreamRef inStream, AudioStreamOutputCallback_f inFunc, void* inContext);
typedef void (*AudioStreamSetOutputCallback_f)(AudioStreamRef inStream, AudioStreamOutputCallback_f inFunc, void* inContext);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		AudioStreamProperties
	@abstract	Properties of an audio stream.
*/

// [String] Type of audio content (e.g. telephony, media, etc.).
#define kAudioStreamProperty_AudioType CFSTR("audioType")
#define kAudioStreamAudioType_Alert CFSTR("alert")
#define kAudioStreamAudioType_Compatibility CFSTR("compatibility")
#define kAudioStreamAudioType_Default CFSTR("default")
#define kAudioStreamAudioType_Media CFSTR("media")
#define kAudioStreamAudioType_SpeechRecognition CFSTR("speechRecognition")
#define kAudioStreamAudioType_Telephony CFSTR("telephony")

// [Data:AudioStreamBasicDescription] Format for the input/output callback(s).
#define kAudioStreamProperty_Format CFSTR("format")

// [Boolean] Use this stream to read audio from a microphone or other input. Default is false.
#define kAudioStreamProperty_Input CFSTR("input")

// [Number] Gets the estimated latency of the current configuration.
#define kAudioStreamProperty_Latency CFSTR("latency")

// [Number] Sets the lowest latency the caller thinks it will need in microseconds.
#define kAudioStreamProperty_PreferredLatency CFSTR("preferredLatency")

// [Number:AudioStreamType] Type of stream. See kAudioStreamType_*.
#define kAudioStreamProperty_StreamType CFSTR("streamType")

// [String] Name for thread(s) created by the audio stream.
#define kAudioStreamProperty_ThreadName CFSTR("threadName")

// [Number] Priority for thread(s) created by the audio stream.
#define kAudioStreamProperty_ThreadPriority CFSTR("threadPriority")

// [Number] The native sample rate of the stream
#define kAudioStreamProperty_VocoderSampleRate CFSTR("vocoderSampleRate")

// [AnyCFType]: Whatever data (CFStringRef, CFNumberRef, CFDataRef, CFDictionaryRef) the platform needs for support with multizone implementations
#define kAudioStreamProperty_MultiZoneAudioHint CFSTR("multiZoneAudioHint")

// [Number] Stream type. See AudioStreamType.
#define kAudioSessionKey_Type CFSTR("type")

// [String] Type of audio content (e.g. telephony, media, etc.). See kAudioStreamAudioType_*.
#define kAudioSessionKey_AudioType CFSTR("audioType")

// [Number] Number of audio channels (e.g. 2 for stereo).
#define kAudioSessionKey_Channels CFSTR("ch")
// [Number] Number of samples per second (e.g. 44100).
#define kAudioSessionKey_SampleRate CFSTR("sr")

// [Number] Bit size of each audio sample (e.g. "16").
#define kAudioSessionKey_SampleSize CFSTR("ss")
// [Number] Input latency in microseconds.
#define kAudioSessionKey_InputLatencyMicros CFSTR("inputLatencyMicros")

// [Number] Output latency in microseconds.
#define kAudioSessionKey_OutputLatencyMicros CFSTR("outputLatencyMicros")

// [Number] dB attenuation level for audio.
// kAirTunesMinVolumeDB      (-30 dB) is the minimum volume level.
// kAirTunesMaxVolumeDB        (0 dB) is the maximum volume level.
//
// The following converts between linear and dB volumes:
//		dB		= 20 * log10( linear )
//		linear	= pow( 10, dB / 20 )
#define kAudioStreamProperty_Volume CFSTR("volume")

// Type of stream.
typedef uint32_t AudioStreamType;
#define kAudioStreamType_Invalid 0 // Reserved for an invalid type.
#define kAudioStreamType_MainAudio 100 // RTP payload type for low-latency audio input/output.
#define kAudioStreamType_AltAudio 101 // RTP payload type for low-latency UI sounds, alerts, etc. output.
#define kAudioStreamType_MainHighAudio 102 // RTP payload type for high-latency audio output. UDP.

// Internals

CFTypeRef _AudioStreamCopyProperty(CFTypeRef inObject, CFStringRef inKey, OSStatus* outErr);
typedef CFTypeRef (*_AudioStreamCopyProperty_f)(CFTypeRef inObject, CFStringRef inKey, OSStatus* outErr);
OSStatus _AudioStreamSetProperty(CFTypeRef inObject, CFStringRef inKey, CFTypeRef inValue);
typedef OSStatus (*_AudioStreamSetProperty_f)(CFTypeRef inObject, CFStringRef inKey, CFTypeRef inValue);

CFObjectDefineStandardAccessors(AudioStreamRef, AudioStreamProperty, _AudioStreamCopyProperty, _AudioStreamSetProperty)

    void AudioStreamSetFormat(AudioStreamRef me, const AudioStreamBasicDescription* inFormat);
typedef OSStatus (*AudioStreamSetFormat_f)(AudioStreamRef me, const AudioStreamBasicDescription* inFormat);
void AudioStreamSetDelegateContext(AudioStreamRef me, void* inContext);
typedef OSStatus (*AudioStreamSetDelegateContext_f)(AudioStreamRef me, void* inContext);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamPrepare
	@abstract	Prepares the audio stream so things like latency can be reported, but doesn't start playing audio.
*/
OSStatus AudioStreamPrepare(AudioStreamRef inStream);
typedef OSStatus (*AudioStreamPrepare_f)(AudioStreamRef inStream);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamStart
	@abstract	Starts the stream (callbacks will start getting invoked after this).
*/
OSStatus AudioStreamStart(AudioStreamRef inStream);
typedef OSStatus (*AudioStreamStart_f)(AudioStreamRef inStream);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamStop
	@abstract	Stops the stream. No callbacks will be received after this returns.
*/
void AudioStreamStop(AudioStreamRef inStream, Boolean inDrain);
typedef void (*AudioStreamStop_f)(AudioStreamRef inStream, Boolean inDrain);

#ifdef __cplusplus
}
#endif

#endif // __AudioUtils_h__
