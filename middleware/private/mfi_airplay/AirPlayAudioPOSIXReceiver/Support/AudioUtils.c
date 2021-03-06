/*
	File:    AudioUtils.c
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

#include "AudioUtils.h"

#include <math.h>
#include <stdlib.h>

#include "APSDebugServices.h"

//===========================================================================================================================
//	IsDTSEncodedData
//===========================================================================================================================

Boolean	IsDTSEncodedData( const void *inData, size_t inByteCount )
{
	const uint16_t *		p;
	const uint16_t *		e;
	
	p = (const uint16_t *) inData;
	e = p + ( inByteCount / 2 );
	for( ; ( e - p ) > 3; ++p )
	{
		if( ( p[ 0 ] == 0x7FFE ) && ( p[ 1 ] == 0x8001 ) )							return( true ); // Normal DTS Sync
		if( ( p[ 0 ] == 0x1FFF ) && ( p[ 1 ] == 0xE800 ) && ( p[ 2 ] == 0x07F1 ) )	return( true ); // 14-bit DTS Sync
	}
	return( false );
}

//===========================================================================================================================
//	IsSilencePCM
//===========================================================================================================================

Boolean	IsSilencePCM( const void *inData, size_t inByteCount )
{
	const int16_t *		src;
	const int16_t *		end;
	
	src = (const int16_t *) inData;
	end = src + ( inByteCount / 2 );
	for( ; src != end; ++src )
	{
		if( *src != 0 )
		{
			return( false );
		}
	}
	return( true );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	SineWave_Init
//===========================================================================================================================

OSStatus	SineTable_Create( SineTableRef *outTable, int inSampleRate, int inFrequency )
{
	OSStatus			err;
	SineTableRef		obj;
	double				scale;
	int					i;
	
	obj = (SineTableRef) malloc( offsetof( struct SineTable, values ) + ( inSampleRate * sizeof( int16_t ) ) );
	require_action( obj, exit, err = kNoMemoryErr );
	
	obj->sampleRate	= inSampleRate;
	obj->frequency	= inFrequency;
	obj->position	= 0;
	
	scale = 2.0 * 3.14159265358 / inSampleRate;
	for( i = 0; i < inSampleRate; ++i )
	{
		obj->values[ i ] = (int16_t)( 32767 * sin( ( (double) i ) * scale ) );
	}
	
	*outTable = obj;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	SineTable_Delete
//===========================================================================================================================

void	SineTable_Delete( SineTableRef inTable )
{
	free( inTable );
}

//===========================================================================================================================
//	SineWave_GetSamples
//===========================================================================================================================

void	SineTable_GetSamples( SineTableRef inTable, int inBalance, int inSampleCount, void *inSampleBuffer )
{
	const int16_t * const		values		= inTable->values;
	const int					sampleRate	= inTable->sampleRate;
	const int					frequency	= inTable->frequency;
	int							position;
	int16_t						sample;
	int16_t *					dst;
	int16_t *					lim;
	
	position = inTable->position;
	dst = (int16_t *) inSampleBuffer;
	lim = dst + ( inSampleCount * 2 );
	while( dst < lim )
	{
		sample = values[ position ];
		position += frequency;
		if( position  > sampleRate )
		{
			position -= sampleRate;
		}
		if( inBalance < 0 )
		{
			*dst++ = sample;
			*dst++ = 0;
		}
		else if( inBalance == 0 )
		{
			*dst++ = sample;
			*dst++ = sample;
		}
		else
		{
			*dst++ = 0;
			*dst++ = sample;
		}
	}
	inTable->position = position;
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	VolumeAdjusterInit
//===========================================================================================================================

void	VolumeAdjusterInit( VolumeAdjusterContext *ctx )
{
	ctx->currentVolume = kQ16_1pt0;
	ctx->targetVolume  = kQ16_1pt0;
}

//===========================================================================================================================
//	VolumeAdjusterSetVolumeDB
//===========================================================================================================================

void	VolumeAdjusterSetVolumeDB( VolumeAdjusterContext *ctx, Float32 inDB )
{
	ctx->targetVolume		= ( inDB != 0 ) ? ( (Q16x16)( kQ16_1pt0 * powf( 10, inDB / 20 ) ) ) : kQ16_1pt0;
	ctx->volumeIncrement	= ( ( ctx->targetVolume - ctx->currentVolume ) << 14 ) / 256; // Convert to Q2.30 format / 256.
	ctx->rampStepsRemaining	= 256;
}

//===========================================================================================================================
//	VolumeAdjusterApply
//===========================================================================================================================

void	VolumeAdjusterApply( VolumeAdjusterContext *ctx, int16_t *inSamples, size_t inCount, size_t inChannels )
{
	int16_t *		dst = inSamples;
	Q16x16			scale;
	
	if( ctx->currentVolume != ctx->targetVolume )
	{
		int32_t		ramp;
		size_t		i, n, ii;
		
		ramp = ctx->currentVolume << 14; // Convert Q16.16 to Q2.30 format.
		n = Min( inCount, ctx->rampStepsRemaining );
		for( i = 0; i < n; ++i )
		{
			ramp += ctx->volumeIncrement;
			scale = ramp >> 14; // Convert back to Q16.16 format.
			for( ii = 0; ii < inChannels; ++ii )
			{
				*dst = (int16_t)( ( ( *dst * scale ) + kQ16_0pt5 ) >> 16 );
				++dst;
			}
		}
		ctx->rampStepsRemaining -= n;
		if( ctx->rampStepsRemaining == 0 )
		{
			ctx->currentVolume = ctx->targetVolume;
		}
	}
	if( ctx->currentVolume == 0 )
	{
		const int16_t * const		lim = inSamples + ( inCount * inChannels );
		
		while( dst < lim )
		{
			*dst++ = 0;
		}
	}
	else if( ctx->currentVolume != kQ16_1pt0 )
	{
		const int16_t * const		lim = inSamples + ( inCount * inChannels );
		
		scale = ctx->currentVolume;
		for( ; dst < lim; ++dst )
		{
			*dst = (int16_t)( ( ( *dst * scale ) + kQ16_0pt5 ) >> 16 );
		}
	}
}

#include "AirPlayCommon.h"
#include "APSCommonServices.h"
#include "MathUtils.h"
#include "TickUtils.h"
#include "ThreadUtils.h"

#include CF_HEADER
#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

//===========================================================================================================================
//	AudioSessionSetEventHandler
//===========================================================================================================================

void AudioSessionSetEventHandler( AudioSessionEventHandler_f inHandler, void *inContext )
{
	(void) inHandler;
	(void) inContext;
	
	// This implementation should remain empty.
}

//===========================================================================================================================
//	AudioSessionEnsureSetup
//===========================================================================================================================

void
	AudioSessionEnsureSetup(
		Boolean		inHasInput,
		uint32_t	inPreferredSystemSampleRate,
		uint32_t	inPreferredSystemBufferSizeMicros )
{
	(void) inHasInput;
	(void) inPreferredSystemSampleRate;
	(void) inPreferredSystemBufferSizeMicros;

	// This implementation should remain empty.
}

//===========================================================================================================================
//	AudioSessionEnsureTornDown
//===========================================================================================================================

void AudioSessionEnsureTornDown( void )
{
	// This implementation should remain empty.
}

//===========================================================================================================================
//	AudioStreamRampVolume
//===========================================================================================================================

OSStatus
	AudioStreamRampVolume( 
		AudioStreamRef		inStream, 
		double				inFinalVolume, 
		double				inDurationSecs, 
		dispatch_queue_t	inQueue )
{
	(void) inStream;
	(void) inFinalVolume;
	(void) inDurationSecs;
	(void) inQueue;

	// This implementation should remain empty.

	return( kNoErr );
}

//===========================================================================================================================
//	AudioStreamSetOutputCallback
//===========================================================================================================================
void	AudioStreamSetOutputCallback( AudioStreamRef inStream, AudioStreamOutputCallback_f inFunc, void *inContext )
{
    AudioStreamSetAudioCallback( inStream, inFunc, inContext );
}

//===========================================================================================================================
//	_AudioStreamCopyProperty
//===========================================================================================================================

CFTypeRef
	_AudioStreamCopyProperty( 
		CFTypeRef		inObject,
		CFObjectFlags	inFlags, 
		CFStringRef		inProperty, 
		CFTypeRef		inQualifier, 
		OSStatus *		outErr )
{
	AudioStreamRef 				me = (AudioStreamRef) inObject;
	OSStatus					err;
	CFTypeRef					value = NULL;
	int64_t						s64;
	
	(void) inFlags;
	(void) inQualifier;
	
	if( 0 ) {}
	
	// Latency
	
	else if( CFEqual( inProperty, kAudioStreamProperty_Latency ) )
	{
		s64 = AudioStreamGetLatency( me, &err );
		require_noerr( err, exit );
		
		value = CFNumberCreateInt64( s64 );
		require_action( value, exit, err = kNoMemoryErr );
	}
	
	// Other
	
	else
	{
		err = kNotHandledErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( outErr ) *outErr = err;
	return( value );
}

//===========================================================================================================================
//	_AudioStreamSetProperty
//===========================================================================================================================

OSStatus
	_AudioStreamSetProperty( 
		CFTypeRef		inObject,
		CFObjectFlags	inFlags, 
		CFStringRef		inProperty, 
		CFTypeRef		inQualifier, 
		CFTypeRef		inValue )
{
	AudioStreamRef const		me = (AudioStreamRef) inObject;
	OSStatus					err;
	
	(void) inFlags;
	(void) inQualifier;
	

	if( 0 ) {}
	
	// AudioType

	else if( CFEqual( inProperty, kAudioStreamProperty_AudioType ) )
	{
        /* Audio Type will be "Default" from iOS and NULL from iTunes */
	    check( ((inValue == NULL) || CFEqual( inValue, kAudioStreamAudioType_Default )) );
	}
	
	// Format

	else if( CFEqual( inProperty, kAudioStreamProperty_Format ) )
	{
	    AudioStreamBasicDescription		inFormat;

        CFGetData( inValue, &inFormat, sizeof( inFormat ), NULL, &err );
        require_noerr( err, exit );

        err = AudioStreamSetFormat ( me, &inFormat );
        require_noerr( err, exit );
	}
	
	// Input

	else if( CFEqual( inProperty, kAudioStreamProperty_Input ) )
	{
        bool    input;

        input = CFGetBoolean( inValue, NULL );
        /* Just confirm if the input is false */
		require_action( input == false, exit, err = kParamErr );
	}
	
	// PreferredLatency
	
	else if( CFEqual( inProperty, kAudioStreamProperty_PreferredLatency ) )
	{
		uint32_t    msecs;

		msecs = (uint32_t) CFGetInt64( inValue, &err );
        require_noerr( err, exit );

        err = AudioStreamSetPreferredLatency( me, msecs );
        require_noerr( err, exit );
	}
	
	// StreamType
	
	else if( CFEqual( inProperty, kAudioStreamProperty_StreamType ) )
	{
        uint32_t    streamType;

        /* Just confirm if the strea type is General */
        streamType = (uint32_t) CFGetInt64( inValue, &err );
		require_action( streamType == kAirPlayStreamType_GeneralAudio, exit, err = kParamErr);
	}
	
	// ThreadName
	
	else if( CFEqual( inProperty, kAudioStreamProperty_ThreadName ) )
	{
        const char *	inName;

        inName = CFStringGetCStringPtr( (CFStringRef) inValue, kCFStringEncodingUTF8 );

        err = AudioStreamSetThreadName( me, inName );
        require_noerr( err, exit );
	}
	
	// ThreadPriority
	
	else if( CFEqual( inProperty, kAudioStreamProperty_ThreadPriority ) )
	{
		uint32_t     prio;

		prio = (uint32_t) CFGetInt64( inValue, &err );
		require_noerr( err, exit );

        AudioStreamSetThreadPriority( me, prio );
	}
	
	// Other
	
	else
	{
		err = kNotHandledErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}


//===========================================================================================================================
// AudioSessionCopyProperty
//===========================================================================================================================
CFTypeRef      AudioSessionCopyProperty( CFStringRef inPropertyName, OSStatus *outErr )
{

       OSStatus                                        err;
       CFTypeRef                                       value = NULL;

       if( 0 ) {}

       // Supports Varispeed

       else if( CFEqual( inPropertyName, kAudioSessionProperty_SupportsVarispeed ) )
       {
#ifdef PLATFORM_SKEW_COMPENSATION_ENABLE
           value = CFRetain( kCFBooleanTrue );
#else
           value = CFRetain( kCFBooleanFalse );
#endif
       }

       // Other

       else
       {
               err = kNotHandledErr;
               goto exit;
       }
       err = kNoErr;

exit:
       if( outErr ) *outErr = err;
       return( value );
}
