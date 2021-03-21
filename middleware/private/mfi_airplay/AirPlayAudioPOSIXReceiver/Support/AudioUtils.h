/*
	File:    AudioUtils.h
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
	
	Copyright (C) 2010-2014 Apple Inc. All Rights Reserved.
*/
/*!
	@header		AudioStream API
	@discussion	Provides APIs for audio streams.
*/

#ifndef	__AudioUtils_h__
#define	__AudioUtils_h__

#include "CFUtils.h"
#include "APSCommonServices.h"

#include CF_HEADER
#include COREAUDIO_HEADER
#include LIBDISPATCH_HEADER

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IsDTSEncodedData
	@internal
	@abstract	Returns true if the buffer contains data that looks like DTS-encoded data.
*/

Boolean	IsDTSEncodedData( const void *inData, size_t inByteCount );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	IsSilencePCM
	@internal
	@abstract	Returns true if the buffer contains 16-bit PCM samples of only silence (0's).
*/

Boolean	IsSilencePCM( const void *inData, size_t inByteCount );

typedef struct SineTable *		SineTableRef;
struct SineTable
{
	int			sampleRate;
	int			frequency;
	int			position;
	int16_t		values[ 1 ]; // Variable length array.
};

OSStatus	SineTable_Create( SineTableRef *outTable, int inSampleRate, int inFrequency );
void		SineTable_Delete( SineTableRef inTable );
void		SineTable_GetSamples( SineTableRef inTable, int inBalance, int inSampleCount, void *inSampleBuffer );

typedef struct
{
	Q16x16		currentVolume;
	Q16x16		targetVolume;
	int32_t		volumeIncrement; // Q2.30 format.
	uint32_t	rampStepsRemaining;
	
}	VolumeAdjusterContext;

void	VolumeAdjusterInit( VolumeAdjusterContext *ctx );
void	VolumeAdjusterSetVolumeDB( VolumeAdjusterContext *ctx, Float32 inDB );
void	VolumeAdjusterApply( VolumeAdjusterContext *ctx, int16_t *inSamples, size_t inCount, size_t inChannels );

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
	@internal
	@abstract	Gets the CF type ID of all AudioStream objects.
*/
CFTypeID	AudioStreamGetTypeID( void );

typedef struct AudioStreamPrivate *		AudioStreamRef;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamCreate
	@abstract	Creates a new AudioStream.
*/
OSStatus	AudioStreamCreate( AudioStreamRef *outStream );
#define 	AudioStreamForget( X ) do { if( *(X) ) { AudioStreamStop( *(X), false ); CFRelease( *(X) ); *(X) = NULL; } } while( 0 )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamSetInputCallback / AudioStreamSetOutputCallback
	@internal
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
typedef void
	( *AudioStreamInputCallback_f )( 
		uint32_t		inSampleTime, 
		uint64_t		inHostTime, 
		const void *	inBuffer, 
		size_t			inLen, 
		void *			inContext );

void	AudioStreamSetInputCallback( AudioStreamRef inStream, AudioStreamInputCallback_f inFunc, void *inContext );
typedef void ( *AudioStreamSetInputCallback_f )( AudioStreamRef inStream, AudioStreamInputCallback_f inFunc, void *inContext );

typedef void
	( *AudioStreamOutputCallback_f )( 
		uint32_t	inSampleTime, 
		uint64_t	inHostTime, 
		void *		inBuffer, 
		size_t		inLen, 
		void *		inContext );

void	AudioStreamSetOutputCallback( AudioStreamRef inStream, AudioStreamOutputCallback_f inFunc, void *inContext );
typedef void ( *AudioStreamSetOutputCallback_f )( AudioStreamRef inStream, AudioStreamOutputCallback_f inFunc, void *inContext );

// [String] Type of audio content (e.g. telephony, media, etc.).
#define kAudioStreamProperty_AudioType				CFSTR( "audioType" )
	#define kAudioStreamAudioType_Default			CFSTR( "default" )

// [Data] AudioStreamBasicDescription of the data format for the input/output callback(s).
#define kAudioStreamProperty_Format					CFSTR( "format" )

// [Boolean] Use this stream to read audio from a microphone or other input. Default is false.
#define kAudioStreamProperty_Input					CFSTR( "input" )

// [Number] Sets the lowest latency the caller thinks it will need in microseconds.
#define kAudioStreamProperty_PreferredLatency		CFSTR( "preferredLatency" )

// [String] Name for thread(s) created by the audio stream.
#define kAudioStreamProperty_ThreadName				CFSTR( "threadName" )

// [Number] Priority for thread(s) created by the audio stream.
#define kAudioStreamProperty_ThreadPriority			CFSTR( "threadPriority" )

// [Number] Stream type (value is AudioStreamType).
#define kAudioStreamProperty_StreamType				CFSTR( "streamType" )

// Convenience getters.

#define AudioStreamCopyProperty( OBJECT, PROPERTY, QUALIFIER, OUT_ERROR ) \
	CFObjectCopyProperty( (OBJECT), NULL, _AudioStreamCopyProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (OUT_ERROR) )

#define AudioStreamGetPropertyBoolean( OBJECT, PROPERTY, QUALIFIER, OUT_ERROR ) \
	CFObjectGetPropertyBooleanSync( (OBJECT), NULL, _AudioStreamCopyProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (OUT_ERROR) )

#define AudioStreamGetPropertyCString( OBJECT, PROPERTY, QUALIFIER, BUF, MAX_LEN, OUT_ERROR ) \
	CFObjectGetPropertyCStringSync( (OBJECT), NULL, _AudioStreamCopyProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), \
		(BUF), (MAX_LEN), (OUT_ERROR) )

#define AudioStreamGetPropertyDouble( OBJECT, PROPERTY, QUALIFIER, OUT_ERROR ) \
	CFObjectGetPropertyDoubleSync( (OBJECT), NULL, _AudioStreamCopyProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (OUT_ERROR) )

#define AudioStreamGetPropertyInt64( OBJECT, PROPERTY, QUALIFIER, OUT_ERROR ) \
	CFObjectGetPropertyInt64Sync( (OBJECT), NULL, _AudioStreamCopyProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (OUT_ERROR) )

// Convenience setters.

#define AudioStreamSetProperty( STREAM, PROPERTY, QUALIFIER, VALUE ) \
	_AudioStreamSetProperty( (STREAM), kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (VALUE) )

#define AudioStreamSetPropertyBoolean( STREAM, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyBoolean( (STREAM), NULL, _AudioStreamSetProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (VALUE) )

#define AudioStreamSetPropertyCString( STREAM, PROPERTY, QUALIFIER, STR, LEN ) \
	CFObjectSetPropertyCString( (STREAM), NULL, _AudioStreamSetProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (STR), (LEN) )

#define AudioStreamSetPropertyData( STREAM, PROPERTY, QUALIFIER, PTR, LEN ) \
	CFObjectSetPropertyData( (STREAM), NULL, _AudioStreamSetProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (PTR), (LEN) )

#define AudioStreamSetPropertyDouble( STREAM, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyDouble( (STREAM), NULL, _AudioStreamSetProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (VALUE) )

#define AudioStreamSetPropertyInt64( STREAM, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyInt64( (STREAM), NULL, _AudioStreamSetProperty, kCFObjectFlagDirect, (PROPERTY), (QUALIFIER), (VALUE) )

// Internals

CFTypeRef
	_AudioStreamCopyProperty( 
		CFTypeRef		inObject, // Must be a AudioStreamRef
		CFObjectFlags	inFlags, 
		CFStringRef		inProperty, 
		CFTypeRef		inQualifier, 
		OSStatus *		outErr );
typedef CFTypeRef
	( *_AudioStreamCopyProperty_f )( 
		CFTypeRef		inObject, // Must be a AudioStreamRef
		CFObjectFlags	inFlags, 
		CFStringRef		inProperty, 
		CFTypeRef		inQualifier, 
		OSStatus *		outErr );
OSStatus
	_AudioStreamSetProperty( 
		CFTypeRef		inObject, // Must be a AudioStreamRef
		CFObjectFlags	inFlags, 
		CFStringRef		inProperty, 
		CFTypeRef		inQualifier, 
		CFTypeRef		inValue );
typedef OSStatus
	( *_AudioStreamSetProperty_f )( 
		CFTypeRef		inObject, // Must be a AudioStreamRef
		CFObjectFlags	inFlags, 
		CFStringRef		inProperty, 
		CFTypeRef		inQualifier, 
		CFTypeRef		inValue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamSetVarispeedRate
	@abstract	Sets the fine-grained sample rate for use when varispeed is enabled for skew compensation.
	
	@param		inStream		Stream for which the new rate is being notified
	@param		inHz			New sample rate to be used by platform to normalize the skew detected by AirPlay
	
	@discussion
	
	This function will be called by AirPlay when it detects skew in the stream being played out. Input pamarter inHz specifes the
	new sample rate to be used by the Platform to normalize the detected skew. Platform should adjust the audio hardware to playout
	the stream with the new sample rate. Note that this functions will be called only on Platforms that specify its own skew
	compensation ability.
*/
OSStatus	AudioStreamSetVarispeedRate( AudioStreamRef inStream, double inHz );
typedef OSStatus ( *AudioStreamSetVarispeedRate_f )( AudioStreamRef inStream, double inHz );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamGetVolume
	@abstract	Gets the current volume of the stream as a linear 0.0-1.0 volume.
*/
double	AudioStreamGetVolume( AudioStreamRef inStream, OSStatus *outErr );
typedef double ( *AudioStreamGetVolume_f )( AudioStreamRef inStream, OSStatus *outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamSetVolume
	@abstract	Sets the volume of the stream to a linear 0.0-1.0 volume.
*/
OSStatus	AudioStreamSetVolume( AudioStreamRef inStream, double inVolume );
typedef OSStatus ( *AudioStreamSetVolume_f )( AudioStreamRef inStream, double inVolume );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamRampVolume
	@internal
	@abstract	Ramps the volume to a final volume over the specified time.
*/
OSStatus
	AudioStreamRampVolume( 
		AudioStreamRef		inStream, 
		double				inFinalVolume, 
		double				inDurationSecs, 
		dispatch_queue_t	inQueue );
typedef OSStatus
	( *AudioStreamRampVolume_f )( 
		AudioStreamRef		inStream, 
		double				inFinalVolume, 
		double				inDurationSecs, 
		dispatch_queue_t	inQueue );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamPrepare
	@abstract	Prepares the audio stream so things like latency can be reported, but doesn't start playing audio.
*/
OSStatus	AudioStreamPrepare( AudioStreamRef inStream );
typedef OSStatus ( *AudioStreamPrepare_f )( AudioStreamRef inStream );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamStart
	@abstract	Starts the stream (callbacks will start getting invoked after this).
*/
OSStatus	AudioStreamStart( AudioStreamRef inStream );
typedef OSStatus ( *AudioStreamStart_f )( AudioStreamRef inStream );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamStop
	@abstract	Stops the stream. No callbacks will be received after this returns.
*/
void	AudioStreamStop( AudioStreamRef inStream, Boolean inDrain );
typedef void ( *AudioStreamStop_f )( AudioStreamRef inStream, Boolean inDrain );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamTest
	@internal
	@abstract	Unit test.
*/
OSStatus	AudioStreamTest( Boolean inInput );

#if 0
#pragma mark -
#pragma mark == AudioSession ==
#endif

typedef uint64_t		AudioSessionAudioFormat;
#define kAudioSessionAudioFormat_Invalid					0
#define kAudioSessionAudioFormat_PCM_8KHz_16Bit_Mono		( 1 << 2 )	// 0x00000004
#define kAudioSessionAudioFormat_PCM_8KHz_16Bit_Stereo		( 1 << 3 )	// 0x00000008
#define kAudioSessionAudioFormat_PCM_16KHz_16Bit_Mono		( 1 << 4 )	// 0x00000010
#define kAudioSessionAudioFormat_PCM_16KHz_16Bit_Stereo		( 1 << 5 )	// 0x00000020
#define kAudioSessionAudioFormat_PCM_24KHz_16Bit_Mono		( 1 << 6 )	// 0x00000040
#define kAudioSessionAudioFormat_PCM_24KHz_16Bit_Stereo		( 1 << 7 )	// 0x00000080
#define kAudioSessionAudioFormat_PCM_32KHz_16Bit_Mono		( 1 << 8 )	// 0x00000100
#define kAudioSessionAudioFormat_PCM_32KHz_16Bit_Stereo		( 1 << 9 )	// 0x00000200
#define kAudioSessionAudioFormat_PCM_44KHz_16Bit_Mono		( 1 << 10 )	// 0x00000400
#define kAudioSessionAudioFormat_PCM_44KHz_16Bit_Stereo		( 1 << 11 )	// 0x00000800
#define kAudioSessionAudioFormat_PCM_44KHz_24Bit_Mono		( 1 << 12 )	// 0x00001000
#define kAudioSessionAudioFormat_PCM_44KHz_24Bit_Stereo		( 1 << 13 )	// 0x00002000
#define kAudioSessionAudioFormat_PCM_48KHz_16Bit_Mono		( 1 << 14 )	// 0x00004000
#define kAudioSessionAudioFormat_PCM_48KHz_16Bit_Stereo		( 1 << 15 )	// 0x00008000
#define kAudioSessionAudioFormat_PCM_48KHz_24Bit_Mono		( 1 << 16 )	// 0x00010000
#define kAudioSessionAudioFormat_PCM_48KHz_24Bit_Stereo		( 1 << 17 )	// 0x00020000
	
// The keys below are used to describe a list of latencies prior to AudioStream setup.
// We need these latencies due to the restrictions of the audio stack on iOS.
// Audio latencies are reported as an ordered array of dictionaries (from least restrictive to the most restrictive).
// Each dictionary contains the following keys:
//		[kAudioSessionKey_Type] - if not specified, then latencies are good for all stream types
//		[kAudioSessionKey_AudioType] - if not specified, then latencies are good for all audio types
//		[kAudioSessionKey_SampleRate] - if not specified, then latencies are good for all sample rates
//		[kAudioSessionKey_SampleSize] - if not specified, then latencies are good for all sample sizes
//		[kAudioSessionKey_Channels] - if not specified, then latencies are good for all channel counts
//		[kAudioSessionKey_CompressionType] - if not specified, then latencies are good for all compression types
//		kAudioSessionKey_InputLatencyMicros
//		kAudioSessionKey_OutputLatencyMicros
	
// [Number] Stream type. See AudioStreamType.
#define kAudioSessionKey_Type					CFSTR( "type" )
	
// [String] Type of audio content (e.g. telephony, media, etc.). See kAudioStreamAudioType_*.
#define kAudioSessionKey_AudioType				CFSTR( "audioType" )
	
// [Number] Number of audio channels (e.g. 2 for stereo).
#define kAudioSessionKey_Channels				CFSTR( "ch" )

// [Number] Type of compression used. See kAudioSessionKeyCompressionType_* constants.
#define kAudioSessionKey_CompressionType		CFSTR( "ct" )
	#define kAudioSessionKeyCompressionType_PCM				( 1 << 0 ) // 0x01: Uncompressed PCM.
	
// [Number] Number of samples per second (e.g. 44100).
#define kAudioSessionKey_SampleRate				CFSTR( "sr" )

// [Number] Bit size of each audio sample (e.g. "16").
#define	kAudioSessionKey_SampleSize				CFSTR( "ss" )
	
// [Number] Input latency in microseconds.
#define kAudioSessionKey_InputLatencyMicros		CFSTR( "inputLatencyMicros" )

// [Number] Output latency in microseconds.
#define kAudioSessionKey_OutputLatencyMicros		CFSTR( "outputLatencyMicros" )
	
// [Number] Output latency in microseconds.
#define kAudioStreamProperty_Latency    kAudioSessionKey_OutputLatencyMicros

	
typedef uint32_t		AudioStreamType;
#define kAudioStreamType_Invalid		  0 // Reserved for an invalid type.
#define kAudioStreamType_MainAudio		100 // RTP payload type for low-latency audio input/output. UDP.

typedef uint32_t	AudioSessionEventType;
#define kAudioSessionEventAudioServicesWereReset	1 // Underlying audio services were reset.
#define kAudioSessionEventAudioInterrupted			2 // Interruption occured in the audio services.

#define AudioSessionEventToString( X ) ( \
	( (X) == kAudioSessionEventAudioServicesWereReset )	? "Reset"		: \
	( (X) == kAudioSessionEventAudioInterrupted )		? "Interrupted"	: \
														  "?" )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioSessionSetEventHandler
	@internal
	@abstract	Sets the function to call when an event arrives.
*/
typedef void ( *AudioSessionEventHandler_f )( AudioSessionEventType inType, CFTypeRef inParam, void *inContext );
void	AudioSessionSetEventHandler( AudioSessionEventHandler_f inHandler, void *inContext );
typedef void ( *AudioSessionSetEventHandler_f )( AudioSessionEventHandler_f inHandler, void *inContext );

// [Boolean] Read-only.  Returns kCFBooleanTrue if the session supports varispeed.
#define kAudioSessionProperty_SupportsVarispeed					CFSTR( "supportsVarispeed" )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioSessionCopyProperty
	@internal
	@abstract	Copies a property of the underlying audio session and hardware.
*/
CFTypeRef	AudioSessionCopyProperty( CFStringRef inPropertyName, OSStatus *outErr );
typedef CFTypeRef ( *AudioSessionCopyProperty_f )( CFStringRef inPropertyName, OSStatus *outErr );
	
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioSessionCopyLatencies
	@internal
	@abstract	Copies audio latencies for all stream types, audio types and audio formats supported by the underlying hardware.
*/
CFArrayRef	AudioSessionCopyLatencies( OSStatus *outErr );
typedef CFArrayRef ( *AudioSessionCopyLatencies_f )( OSStatus *outErr );
	
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioSessionEnsureSetup
	@internal
	@abstract	Ensure that audio services are initialized.
*/
void
	AudioSessionEnsureSetup(
		Boolean		inHasInput,
		uint32_t	inPreferredSystemSampleRate,
		uint32_t	inPreferredSystemBufferSizeMicros );
typedef void
	( *AudioSessionEnsureSetup_f )(
		Boolean		inHasInput,
		uint32_t	inPreferredSystemSampleRate,
		uint32_t	inPreferredSystemBufferSizeMicros );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioSessionEnsureTornDown
	@internal
	@abstract	Signal that any audio services required by AirPlay are no longer required.
*/
void	AudioSessionEnsureTornDown( void );
typedef void ( *AudioSessionEnsureTornDown_f )( void );
	
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioSessionGetSupportedInputFormats
	@internal
	@abstract	Gets the set of input formats that are supported by the underlying hardware.
*/
AudioSessionAudioFormat	AudioSessionGetSupportedInputFormats( AudioStreamType inStreamType );
typedef AudioSessionAudioFormat ( *AudioSessionGetSupportedInputFormats_f )( AudioStreamType inStreamType );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioSessionGetSupportedOutputFormats
	@internal
	@abstract	Gets the set of output formats that are supported by the underlying hardware.
*/
AudioSessionAudioFormat	AudioSessionGetSupportedOutputFormats( AudioStreamType inStreamType );
typedef AudioSessionAudioFormat ( *AudioSessionGetSupportedOutputFormats_f )( AudioStreamType inStreamType );

typedef uint32_t		AudioStreamFlags;

#define kAudioStreamFlag_Input		( 1 << 0 )	// Use this stream to read data from a microphone or other input.
#define kAudioStreamFlag_Output		0			// Absence of the input flag means to use this stream for audio output.

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamSetFormat
	@abstract	Sets the format to provided to the callback for input or the format provided by the callback for output.
*/
OSStatus	AudioStreamSetFormat( AudioStreamRef inStream, const AudioStreamBasicDescription *inFormat );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamGetLatency
	@abstract	Gets the minimum latency the system can achieve (may be higher if samples are already queued).
	@discussion
    This function should return the minimum latency (in microseconds) the system can achieve, which should include 
    any DSP delays introduced by the accessory hardware after the audio samples have been retrieved from AirPlay Core. 
    This latency information is used by AirPlay to ensure synchronization of the AirPlay audio when streaming 
    simultaneously to multiple accessories.
	
*/
uint32_t	AudioStreamGetLatency( AudioStreamRef inStream, OSStatus *outErr );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamSetPreferredLatency
	@abstract	Sets the lowest latency the caller thinks it will need. Defaults to 100 ms.
*/
OSStatus	AudioStreamSetPreferredLatency( AudioStreamRef inStream, uint32_t inMics );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamSetThreadName 
	@abstract	Sets the name of threads created by the clock.
*/
OSStatus	AudioStreamSetThreadName( AudioStreamRef inStream, const char *inName );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamSetThreadPriority
	@abstract	Sets the priority of threads created by the clock.
*/
void		AudioStreamSetThreadPriority( AudioStreamRef inStream, int inPriority );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamSetFlags
	@abstract	Streams the AudioStream.

	inFlags will be kAudioStreamFlag_Output.
*/
OSStatus	AudioStreamSetFlags( AudioStreamRef inStream, AudioStreamFlags inFlags );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamAudioCallback_f
	@abstract	Callback function to be called by Platform to retrieve audio samples from AirPlay.
	
	@param		inSampleTime	Audio sample number/count for the first sample to be placed in the buffer.
	@param		inHostTime		UpTicks() compatible timestamp for when the first sample in the buffer will be heard.
	@param		inBuffer		Buffer to receive the audio samples. The format is configured during stream setup.
	@param		inLen			Number of bytes to fill into the buffer.
	@param		inContext		Context that was passed in AudioStreamSetAudioCallback().
	
	@discussion
	
	Platform should call this AirPlay audio callback function from a separate platform audio thread to retrieve the audio samples from
	AirPlay, and then render the audio on the audio output path. If there isn't enough audio buffered to satisfy this request, the 
	missing data will be filled in with silence. 
    
	It is recommended that platform utilize available hardware mechanisms (for ex.  low buffer threshold notification) as the trigger 
	to invoke the audio callback function when the audio hardware needs the next chunk of audio. The periodicity of this callback 
	invocation should be chosen based on the following requirements:

	- It should be invoked frequent enough to ensure that there is very little likelihood of the hardware running dry due to thread
		scheduling delays, which could prevent the audio thread from running in time to provide new samples to the hardware. So it should
		  be based on platform properties like thread scheduling, thread priorities, cpu usage etc.

	- It should not be invoked with too much audio samples already queued in the audio HW buffer, as it could lead to silence samples
		  being returned to the platform if the AirPlay buffer is close to empty. This will lead to audio drops which could have been
		  avoided.

	Timing information is important for proper synchronization of audio. The inSampleTime parameter should specify the sample
	count/number for the first sample to be filled in. The inHostTime parameter should specify an UpTick() compatible timestamp in the
	future when the first sample will be heard. The developer should use the pending queued audio samples in the audio HW
	buffer to ensure that the inHostTime timestamp is as close as possible to when the sample will really be heard. 


	As mentioned earlier, buffering too much audio samples in the HW buffer can lead to the scenario in which silence samples could be
	returned in the platform buffer if the airplay buffer is close to empty. So it is recommended that platform buffer only  2 x “low
	buffer threshold” worth of audio samples in the audio HW buffer. And when the hardware consumes/plays out the audio samples and hits
	the “low buffer threshold”, the audio callback function should be invoked again. This will ensure that:

		+ audio callback function is called with sufficient audio samples in the HW buffer to prevent underflows.
		+ too much audio samples is not buffered in the HW buffer when invoking the callback function.

	Note that the Buffer size provided to the callback function will impact the additional latency for audio to be heard. So the sizing
	of this should not be too big as to introduce too much latency.  So the size of the buffer should depend on the preferred latency,
	but should be generally around 50ms.
*/
typedef void
    ( *AudioStreamAudioCallback_f )( 
        uint32_t	inSampleTime, 
        uint64_t	inHostTime, 
        void *		inBuffer, 
        size_t		inLen, 
        void *		inContext );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioStreamSetAudioCallback
	@abstract	Sets a function to be called for audio input or output (depending on the direction of the stream).
*/
void		AudioStreamSetAudioCallback( AudioStreamRef inStream, AudioStreamAudioCallback_f inFunc, void *inContext );

#ifdef __cplusplus
}
#endif

#endif	// __AudioUtils_h__
