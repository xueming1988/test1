/*
	File:    AirPlayReceiverPOSIX.c
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
	
	Copyright (C) 2013-2014 Apple Inc. All Rights Reserved.
	
	POSIX platform plugin for AirPlay.
*/

#include "AirPlayCommon.h"
#include "AirPlayReceiverServer.h"
#include "AirPlayReceiverServerPriv.h"
#include "AirPlayReceiverSession.h"
#include "AirPlayReceiverSessionPriv.h"
#include "AirPlaySettings.h"
#include "AudioUtils.h"
#include "StringUtils.h"
#include "SystemUtils.h"
#include "AirPlayReceiverPOSIX.h"

typedef struct _airplay_notify{
        char notify[32];
        long lposition;
        long lduration;
        char title[512];
        char artist[512];
        char album[512];
        char album_uri[128];
}airplay_notify;

#if 0
#pragma mark == Structures ==
#endif

//===========================================================================================================================
//	Structures
//===========================================================================================================================

// AirPlayReceiverServerPlatformData

typedef struct
{
	uint32_t						systemBufferSizeMicros;
	uint32_t						systemSampleRate;

}	AirPlayReceiverServerPlatformData;

// AirPlayAudioStreamPlatformContext

typedef struct
{
	AirPlayStreamType				type;
	AirPlayAudioFormat				format;
	Boolean							input;
	Boolean							loopback;
	CFStringRef						audioType;
	Boolean							volumeControl;
	
	AirPlayStreamType				activeType;
	AirPlayReceiverSessionRef		session;
	AudioStreamRef					stream;
	Boolean							started;
	
}	AirPlayAudioStreamPlatformContext;

// AirPlayReceiverSessionPlatformData

typedef struct
{
	AirPlayReceiverSessionRef				session;
	AirPlayAudioStreamPlatformContext		mainAudioCtx;
	Boolean									sessionStarted;
	
}	AirPlayReceiverSessionPlatformData;

#if 0
#pragma mark == Prototypes ==
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static OSStatus	_SetUpStreams( AirPlayReceiverSessionRef inSession, CFDictionaryRef inParams );
static void		_TearDownStreams( AirPlayReceiverSessionRef inSession, CFDictionaryRef inParams );
static OSStatus	_UpdateStreams( AirPlayReceiverSessionRef inSession );
static void
	_AudioOutputCallBack( 
		uint32_t	inSampleTime, 
		uint64_t	inHostTime, 
		void *		inBuffer, 
		size_t		inLen, 
		void *		inContext );
static void _HandleAudioSessionEvent( AudioSessionEventType inType, CFTypeRef inParam, void *inContext );

#if 0
#pragma mark == Globals ==
#endif

//===========================================================================================================================
//	Globals
//===========================================================================================================================

ulog_define( AirPlayReceiverPlatform, kLogLevelTrace, kLogFlags_Default, "AirPlay",  NULL );
#define atrp_ucat()					&log_category_from_name( AirPlayReceiverPlatform )
#define atrp_ulog( LEVEL, ... )		ulog( atrp_ucat(), (LEVEL), __VA_ARGS__ )

#if 0
#pragma mark -
#pragma mark == Server-level APIs ==
#endif

//===========================================================================================================================
//	AirPlayReceiverServerPlatformInitialize
//===========================================================================================================================

OSStatus	AirPlayReceiverServerPlatformInitialize( AirPlayReceiverServerRef inServer )
{
	OSStatus								err;
	AirPlayReceiverServerPlatformData *		platform;
	
	platform = (AirPlayReceiverServerPlatformData *) calloc( 1, sizeof( *platform ) );
	require_action( platform, exit, err = kNoMemoryErr );
	inServer->platformPtr = platform;
	err = kNoErr;

exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverServerPlatformFinalize
//===========================================================================================================================

void	AirPlayReceiverServerPlatformFinalize( AirPlayReceiverServerRef inServer )
{
	AirPlayReceiverServerPlatformData * const	platform = (AirPlayReceiverServerPlatformData *) inServer->platformPtr;
	
	if( !platform ) return;
	
	free( platform );
	inServer->platformPtr = NULL;
}

//===========================================================================================================================
//	AirPlayReceiverServerPlatformControl
//===========================================================================================================================

OSStatus
	AirPlayReceiverServerPlatformControl( 
		CFTypeRef			inServer, 
		uint32_t			inFlags, 
		CFStringRef			inCommand, 
		CFTypeRef			inQualifier, 
		CFDictionaryRef		inParams, 
		CFDictionaryRef *	outParams )
{
	AirPlayReceiverServerRef const				server		= (AirPlayReceiverServerRef) inServer;
	OSStatus									err;
	
	(void) inFlags;
	
	if( 0 ) {}
	
	// UpdatePrefs
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_UpdatePrefs ) ) )
	{
	}
	
	// Other
	
	else if( server->delegate.control_f )
	{
		err = server->delegate.control_f( server, inCommand, inQualifier, inParams, outParams, server->delegate.context );
		goto exit;
	}
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
//	AirPlayReceiverServerPlatformCopyProperty
//===========================================================================================================================

CFTypeRef
	AirPlayReceiverServerPlatformCopyProperty( 
		CFTypeRef	inServer, 
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		OSStatus *	outErr )
{
	AirPlayReceiverServerRef const		server = (AirPlayReceiverServerRef) inServer;
	CFTypeRef							value  = NULL;
	OSStatus							err;
	
	(void) inFlags;
	
	if( 0 ) {}
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_SkewCompensation ) ) )
	{
		OSStatus localErr;
		value = AudioSessionCopyProperty( kAudioSessionProperty_SupportsVarispeed, &localErr );
		if( (value == NULL) || (localErr != kNoErr) )
		{
			value = CFRetain( kCFBooleanFalse );
		}
		err = kNoErr;
	}
	
	// SystemFlags
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_StatusFlags ) ) )
	{
		// Always report an audio link until we can detect it correctly.
		
		value = CFNumberCreateInt64( kAirPlayStatusFlag_AudioLink );
		require_action( value, exit, err = kUnknownErr );
	}
	
    // Features
    
    else if (CFEqual( inProperty, CFSTR( kAirPlayProperty_Features )))
    {
        AirPlayFeatures     features = 0;

        AirPlayReceiverPlatformGetSupportedMetadata( &features );

        features |= kAirPlayFeature_AudioPCM;
        value = CFNumberCreate( NULL, kCFNumberSInt64Type, &features );
        require_action( value, exit, err = kNoMemoryErr );
    }

	// Other
	
	else if( server->delegate.copyProperty_f )
	{
		value = server->delegate.copyProperty_f( server, inProperty, inQualifier, &err, server->delegate.context );
		goto exit;
	}
	else
	{
		err = kNotHandledErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	if( err )		ForgetCF( &value );
	if( outErr )	*outErr = err;
	return( value );
}

//===========================================================================================================================
//	AirPlayReceiverServerPlatformSetProperty
//===========================================================================================================================

OSStatus
	AirPlayReceiverServerPlatformSetProperty( 
		CFTypeRef	inServer, 
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		CFTypeRef	inValue )
{
	AirPlayReceiverServerRef const		server = (AirPlayReceiverServerRef) inServer;
	OSStatus							err;
	
	(void) inFlags;
	
	if( 0 ) {}
	
	// Other
	
	else if( server->delegate.setProperty_f )
	{
		err = server->delegate.setProperty_f( server, inProperty, inQualifier, inValue, server->delegate.context );
		goto exit;
	}
	else
	{
		err = kNotHandledErr;
		goto exit;
	}
	err = kNoErr;
	
exit:
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Session-level APIs ==
#endif

//===========================================================================================================================
//	AirPlayReceiverSessionPlatformInitialize
//===========================================================================================================================

OSStatus	AirPlayReceiverSessionPlatformInitialize( AirPlayReceiverSessionRef inSession )
{
	OSStatus									err;
	AirPlayReceiverSessionPlatformData *		spd;
	
	spd = (AirPlayReceiverSessionPlatformData *) calloc( 1, sizeof( *spd ) );
	require_action( spd, exit, err = kNoMemoryErr );
	spd->session = inSession;
	inSession->platformPtr = spd;
	err = kNoErr;

	AudioSessionSetEventHandler( _HandleAudioSessionEvent, inSession );

exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayReceiverSessionPlatformFinalize
//===========================================================================================================================

void	AirPlayReceiverSessionPlatformFinalize( AirPlayReceiverSessionRef inSession )
{
	AirPlayReceiverSessionPlatformData * const		spd = (AirPlayReceiverSessionPlatformData *) inSession->platformPtr;
	
	if( !spd ) return;
	
	spd->sessionStarted = false;
	_TearDownStreams( inSession, NULL );
	
	free( spd );
	inSession->platformPtr = NULL;
}

//===========================================================================================================================
//	AirPlayReceiverSessionPlatformControl
//===========================================================================================================================

OSStatus
	AirPlayReceiverSessionPlatformControl( 
		CFTypeRef			inSession, 
		uint32_t			inFlags, 
		CFStringRef			inCommand, 
		CFTypeRef			inQualifier, 
		CFDictionaryRef		inParams, 
		CFDictionaryRef *	outParams )
{
	AirPlayReceiverSessionRef const					session = (AirPlayReceiverSessionRef) inSession;
	AirPlayReceiverSessionPlatformData * const		spd = (AirPlayReceiverSessionPlatformData *) session->platformPtr;
	OSStatus										err;
	double											duration, finalVolume;
	
	(void) inFlags;
	
	if( 0 ) {}
	
	// DuckAudio
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_DuckAudio ) ) )
	{
		duration = CFDictionaryGetDouble( inParams, CFSTR( kAirPlayKey_DurationMs ), &err );
		if( err || ( duration < 0 ) ) duration = 500;
		duration /= 1000;
		
		finalVolume = CFDictionaryGetDouble( inParams, CFSTR( kAirPlayProperty_Volume ), &err );
		finalVolume = !err ? DBtoLinear( (float) finalVolume ) : 0.2;
		finalVolume = Clamp( finalVolume, 0.0, 1.0 );
		
		// Duck main audio if started
		if( spd->mainAudioCtx.stream && spd->mainAudioCtx.started )
		{
			atrp_ulog( kLogLevelNotice, "Ducking audio to %f within %f seconds\n", finalVolume, duration );
			AudioStreamRampVolume( spd->mainAudioCtx.stream, finalVolume, duration, session->server->queue );
		}

		// Notify client of duck command as well
		if( session->delegate.duckAudio_f )
		{
			atrp_ulog( kLogLevelNotice, "Delegating ducking of audio to %f within %f seconds\n", finalVolume, duration );
			session->delegate.duckAudio_f( session, duration, finalVolume, session->delegate.context );
		}
	}
	
	// UnduckAudio
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_UnduckAudio ) ) )
	{
		duration = CFDictionaryGetDouble( inParams, CFSTR( kAirPlayKey_DurationMs ), &err );
		if( err || ( duration < 0 ) ) duration = 500;
		duration /= 1000;
		
		// Unduck main audio if started
		if( spd->mainAudioCtx.stream && spd->mainAudioCtx.started )
		{
			atrp_ulog( kLogLevelNotice, "Unducking audio within %f seconds\n", duration );
			AudioStreamRampVolume( spd->mainAudioCtx.stream, 1.0, duration, session->server->queue );
		}
		
		// Notify client of unduck command as well
		if( session->delegate.unduckAudio_f )
		{
			atrp_ulog( kLogLevelNotice, "Delegating unducking of audio within %f seconds\n", duration );
			session->delegate.unduckAudio_f( session, duration, session->delegate.context );
		}
	}
	
	// SetUpStreams
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_SetUpStreams ) ) )
	{
		err = _SetUpStreams( session, inParams );
		require_noerr( err, exit );
	}
	
	// TearDownStreams
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_TearDownStreams ) ) )
	{
		_TearDownStreams( session, inParams );
	}
	
	// StartSession
	
	else if( CFEqual( inCommand, CFSTR( kAirPlayCommand_StartSession ) ) )
	{
		spd->sessionStarted = true;
		err = _UpdateStreams( session );
		require_noerr( err, exit );
	}
	
	// Other
	
	else if( session->delegate.control_f )
	{
		err = session->delegate.control_f( session, inCommand, inQualifier, inParams, outParams, session->delegate.context );
		goto exit;
	}
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
//	AirPlayReceiverSessionPlatformCopyProperty
//===========================================================================================================================

CFTypeRef
	AirPlayReceiverSessionPlatformCopyProperty( 
		CFTypeRef	inSession, 
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		OSStatus *	outErr )
{
	AirPlayReceiverSessionRef const					session = (AirPlayReceiverSessionRef) inSession;
	AirPlayReceiverSessionPlatformData * const		spd		= (AirPlayReceiverSessionPlatformData *) session->platformPtr;
	OSStatus										err;
	CFTypeRef										value = NULL;
	
	(void) inFlags;
	(void) spd;
	
	if( 0 ) {}
	
	// Volume
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_Volume ) ) )
	{
		double		volume, dB;
		
		if( spd && spd->mainAudioCtx.stream && spd->mainAudioCtx.started )
		{
			volume = AudioStreamGetVolume( spd->mainAudioCtx.stream, NULL );
		}
		else
		{
			volume = 1.0;
		}
		dB = ( volume > 0 ) ? ( 20.0 * log10( volume ) ) : kAirTunesSilenceVolumeDB;
		value = CFNumberCreate( NULL, kCFNumberDoubleType, &dB );
		require_action( value, exit, err = kNoMemoryErr );
	}

	// Other
	
	else if( session->delegate.copyProperty_f )
	{
		value = session->delegate.copyProperty_f( session, inProperty, inQualifier, &err, session->delegate.context );
		goto exit;
	}
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


static void report_position(uint32_t durationSeconds, uint32_t elapsedSeconds)
{
	static airplay_notify pairplay_notify;
	static struct timeval last_tm = {0,0};
	struct timeval now_tm;
	gettimeofday(&now_tm, NULL);	
	unsigned int time_ms = (now_tm.tv_sec*1000 + now_tm.tv_usec/1000) - (last_tm.tv_sec*1000 + last_tm.tv_usec/1000);
	if( time_ms < 1500 )
		return;
	memset(&pairplay_notify, 0, sizeof(airplay_notify));
	strcpy(pairplay_notify.notify,"AIRPLAY_POSITION");
	pairplay_notify.lduration = durationSeconds*1000;
	pairplay_notify.lposition = elapsedSeconds*1000;
	SocketClientReadWriteMsg("/tmp/Requesta01controller", &pairplay_notify,sizeof(airplay_notify),NULL,NULL,0);
	gettimeofday(&last_tm, NULL);	
}
//===========================================================================================================================
//	AirPlayReceiverSessionPlatformSetProperty
//===========================================================================================================================

OSStatus
	AirPlayReceiverSessionPlatformSetProperty( 
		CFTypeRef	inSession, 
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		CFTypeRef	inValue )
{
	AirPlayReceiverSessionRef const					session	= (AirPlayReceiverSessionRef) inSession;
	AirPlayReceiverSessionPlatformData * const		spd		= (AirPlayReceiverSessionPlatformData *) session->platformPtr;
	OSStatus										err;
	
	(void) spd;
	(void) inFlags;
	
	if( 0 ) {}
	
	// Volume
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_Volume ) ) )
	{
		if( spd && spd->mainAudioCtx.stream && spd->mainAudioCtx.started )
		{
			double const		dB = (Float32) CFGetDouble( inValue, NULL );
			double				linear;
			
			if(      dB <= kAirTunesMinVolumeDB ) linear = 0.0;
			else if( dB >= kAirTunesMaxVolumeDB ) linear = 1.0;
			else linear = pow( 10, dB / 20 );
			linear = Clamp( linear, 0.0, 1.0 );
			//atrp_ulog( kLogLevelNotice, "Setting volume to dB=%f, linear=%f\n", dB, linear );
			err = AudioStreamSetVolume( spd->mainAudioCtx.stream, dB );
			require_noerr( err, exit );
		}
	}
	
	else if( CFEqual( inProperty, CFSTR( kAirPlayProperty_Skew ) ) )
	{
		if( spd && spd->mainAudioCtx.stream )
		{
			AudioStreamBasicDescription asbd;
			int32_t skewSamples, varispeedRate;
			
			err = AirPlayAudioFormatToASBD( spd->mainAudioCtx.format, &asbd, NULL );
			require_noerr( err, exit );
			
			skewSamples = (int32_t) CFGetInt64( inValue, NULL );
			varispeedRate = (int32_t) asbd.mSampleRate + skewSamples;
			
			err = AudioStreamSetVarispeedRate( spd->mainAudioCtx.stream, (double) varispeedRate );
			require_noerr( err, exit );
		}
		else
		{
			atrp_ulog( kLogLevelError, "Attempting to set Skew before stream has been created\n" );
			err = kNotHandledErr;
			goto exit;
		}
	}
	
    // Metadata
    
    else if (CFEqual( inProperty, CFSTR( kAirPlayProperty_MetaData )))
    {
        CFDictionaryRef     inMetaDatDict = (CFDictionaryRef) inValue;
	    CFTypeRef		    albumArtRef;
        char*               tempStr;

        airplay_notify *pairplay_notify=(airplay_notify *)malloc(sizeof(airplay_notify));
        if( pairplay_notify )
        {
            memset(pairplay_notify,0,sizeof(airplay_notify));
            strcpy(pairplay_notify->notify,"AIRPLAY_MEDADATA");
        }

        // Title
        tempStr = CFDictionaryCopyCString(inMetaDatDict, kAirPlayMetaDataKey_Title, &err);
        if (err == kNoErr)
        { 
            AirPlayReceiverPlatformSetMetadata( eAirPlayMetadataTitle, tempStr );
            if( pairplay_notify ) strcpy(pairplay_notify->title, tempStr);
            free(tempStr);
        }


        // Album
        tempStr = CFDictionaryCopyCString(inMetaDatDict, kAirPlayMetaDataKey_Album, &err);
        if (err == kNoErr)
        {
            AirPlayReceiverPlatformSetMetadata( eAirPlayMetadataAlbum, tempStr );
            if( pairplay_notify ) strcpy(pairplay_notify->album, tempStr);
            free(tempStr);
        }


        // Artist
        tempStr = CFDictionaryCopyCString(inMetaDatDict, kAirPlayMetaDataKey_Artist, &err);
        if (err == kNoErr)
        {
            AirPlayReceiverPlatformSetMetadata( eAirPlayMetadataArtist, tempStr );
            if( pairplay_notify ) strcpy(pairplay_notify->artist, tempStr);
            free(tempStr);
        }

        // Composer
        tempStr = CFDictionaryCopyCString(inMetaDatDict, kAirPlayMetaDataKey_Composer, &err);
        if (err == kNoErr)
        {
            AirPlayReceiverPlatformSetMetadata( eAirPlayMetadataComposer, tempStr );
            free(tempStr);
        }	

        if( pairplay_notify )
        {
            SocketClientReadWriteMsg("/tmp/Requesta01controller",pairplay_notify,sizeof(airplay_notify),NULL,NULL,0);
            free(pairplay_notify);
		}

        // Artwork 
	    albumArtRef = CFDictionaryGetValue( inMetaDatDict, kAirPlayMetaDataKey_ArtworkData );

	    if ( albumArtRef )
	    {
            uint8_t             *artwork = NULL;
            size_t              artworkSize = 0;
            char                *mimeTypeStr = NULL;
            AirPlayAlbumArt_t   albumArt;

		    artwork = CFGetData( albumArtRef, NULL, 0, &artworkSize, &err );

            if ( artwork )
            { 
                // Get artwork mime type from dictionary
                mimeTypeStr = CFDictionaryCopyCString(inMetaDatDict, kAirPlayMetaDataKey_ArtworkMIMEType, &err);

                if (mimeTypeStr == NULL) mimeTypeStr = strdup("image/png"); // defaults to png if not present in dict
            }

            albumArt.bufferPtr  = artwork;
            albumArt.bufferSize = artworkSize;
            albumArt.mimeStr    = mimeTypeStr;

            AirPlayReceiverPlatformSetMetadata( eAirPlayMetadataArtwork, &albumArt );

            if (mimeTypeStr) free(mimeTypeStr);
        }
    }


    //
    // Progress
    //
    else if (CFEqual( inProperty, CFSTR( kAirPlayProperty_Progress )))
    {
        AirPlayProgressInfo_t   progressInfo;
        progressInfo.elapsedSeconds = (uint32_t) CFDictionaryGetDouble( (CFDictionaryRef) inValue, kAirPlayMetaDataKey_ElapsedTime, NULL );
        progressInfo.durationSeconds  = (uint32_t) CFDictionaryGetDouble( (CFDictionaryRef) inValue, kAirPlayMetaDataKey_Duration, NULL );
        report_position(progressInfo.durationSeconds, progressInfo.elapsedSeconds);
        AirPlayReceiverPlatformSetMetadata( eAirPlayMetadataProgress, &progressInfo );
    }

	// Other
	
	else if( session->delegate.setProperty_f )
	{
		err = session->delegate.setProperty_f( session, inProperty, inQualifier, inValue, session->delegate.context );
		goto exit;
	}
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
//	AirPlayReceiverServerGetRef
//===========================================================================================================================
AirPlayReceiverServerRef AirPlayReceiverServerGetRef( void )
{
    return ( gAirPlayReceiverServer );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_SetUpStreams
//===========================================================================================================================

static OSStatus	_SetUpStreams( AirPlayReceiverSessionRef inSession, CFDictionaryRef inParams )
{
	AirPlayReceiverSessionPlatformData * const		spd = (AirPlayReceiverSessionPlatformData *) inSession->platformPtr;
	OSStatus										err;
	CFArrayRef										streams;
	CFIndex											i, n;
	CFDictionaryRef									streamDesc;
	AirPlayStreamType								streamType;
	AirPlayAudioStreamPlatformContext *				streamCtx;
	CFStringRef										cfstr;
	
	streams = CFDictionaryGetCFArray( inParams, CFSTR( kAirPlayKey_Streams ), NULL );
	n = streams ? CFArrayGetCount( streams ) : 0;
	for( i = 0; i < n; ++i )
	{
		streamDesc = CFArrayGetCFDictionaryAtIndex( streams, i, &err );
		require_noerr( err, exit );
		
		streamType = (AirPlayStreamType) CFDictionaryGetInt64( streamDesc, CFSTR( kAirPlayKey_Type ), NULL );
		switch( streamType )
		{
			case kAirPlayStreamType_GeneralAudio:	streamCtx = &spd->mainAudioCtx; break;
			case kAirPlayStreamType_MainAudio:		streamCtx = &spd->mainAudioCtx; break;
			default: atrp_ulog( kLogLevelNotice, "### Unsupported stream type %d\n", streamType ); continue;
		}
		streamCtx->type				= streamType;
		streamCtx->format			= CFDictionaryGetInt64( streamDesc, CFSTR( kAirPlayKey_AudioFormat ), NULL );
		if( streamCtx->format == kAirPlayAudioFormat_Invalid ) streamCtx->format = kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo;
		streamCtx->input			= CFDictionaryGetBoolean( streamDesc, CFSTR( kAirPlayKey_Input ), &err );
		streamCtx->loopback			= CFDictionaryGetBoolean( streamDesc, CFSTR( kAirPlayKey_AudioLoopback ), NULL );
		streamCtx->volumeControl	= ( streamType == kAirPlayStreamType_GeneralAudio );
		
		cfstr = CFDictionaryGetCFString( streamDesc, CFSTR( kAirPlayKey_AudioType ), NULL );
		ReplaceCF( &streamCtx->audioType, cfstr );
	}
	
	err = _UpdateStreams( inSession );
	require_noerr( err, exit );
	
exit:
	if( err ) _TearDownStreams( inSession, inParams );
	return( err );
}

//===========================================================================================================================
//	_TearDownStreams
//===========================================================================================================================

static void	_TearDownStreams( AirPlayReceiverSessionRef inSession, CFDictionaryRef inParams )
{
	AirPlayReceiverSessionPlatformData * const		spd = (AirPlayReceiverSessionPlatformData *) inSession->platformPtr;
	OSStatus										err;
	CFArrayRef										streams;
	CFIndex											i, n;
	CFDictionaryRef									streamDesc;
	AirPlayStreamType								streamType;
	AirPlayAudioStreamPlatformContext *				streamCtx;
	
	streams = inParams ? CFDictionaryGetCFArray( inParams, CFSTR( kAirPlayKey_Streams ), NULL ) : NULL;
	n = streams ? CFArrayGetCount( streams ) : 0;
	for( i = 0; i < n; ++i )
	{
		streamDesc = CFArrayGetCFDictionaryAtIndex( streams, i, &err );
		require_noerr( err, exit );
		
		streamType = (AirPlayStreamType) CFDictionaryGetInt64( streamDesc, CFSTR( kAirPlayKey_Type ), NULL );
		switch( streamType )
		{
			case kAirPlayStreamType_GeneralAudio:	streamCtx = &spd->mainAudioCtx; break;
			case kAirPlayStreamType_MainAudio:		streamCtx = &spd->mainAudioCtx; break;
			default: atrp_ulog( kLogLevelNotice, "### Unsupported stream type %d\n", streamType ); continue;
		}
		streamCtx->type = kAirPlayStreamType_Invalid;
	}
	if( n == 0 )
	{
		spd->mainAudioCtx.type = kAirPlayStreamType_Invalid;
	}
	_UpdateStreams( inSession );
		
exit:
	return;
}

//===========================================================================================================================
//	_UpdateStreams
//===========================================================================================================================

static OSStatus	_UpdateStreams( AirPlayReceiverSessionRef inSession )
{
	AirPlayReceiverSessionPlatformData * const		spd		= (AirPlayReceiverSessionPlatformData *) inSession->platformPtr;
	Boolean const									useMain	= ( spd->mainAudioCtx.type != kAirPlayStreamType_Invalid );
	Boolean const									useAlt	= false;
	OSStatus										err;
	AirPlayAudioStreamPlatformContext *				streamCtx;
	AudioStreamBasicDescription						asbd;
	
	if( useMain || useAlt )
	{
		AirPlayReceiverServerPlatformData * const platform = (AirPlayReceiverServerPlatformData *) inSession->server->platformPtr;
		uint32_t systemBufferSizeMicros = platform->systemBufferSizeMicros;

		if( ( systemBufferSizeMicros == 0 ) && ( ( spd->mainAudioCtx.type == kAirPlayStreamType_MainAudio ) || useAlt ) )
		{
			// Reduce HAL buffer size to 5 ms. Below that may cause HAL overloads.
			systemBufferSizeMicros = 5000;
		}
		
		AudioSessionEnsureSetup( spd->mainAudioCtx.input, platform->systemSampleRate, systemBufferSizeMicros );
	}
	
	// Update main audio stream.
	
	streamCtx = &spd->mainAudioCtx;
	if( ( streamCtx->type != kAirPlayStreamType_Invalid ) && !streamCtx->stream )
	{
#if 0
		atrp_ulog( kLogLevelNotice, "Main audio setting up %s for %@, input %s, loopback %s, volume %s\n",
			AirPlayAudioFormatToString( streamCtx->format ), 
			streamCtx->audioType ? streamCtx->audioType : CFSTR( kAirPlayAudioType_Default ),
			streamCtx->input			? "yes" : "no",
			streamCtx->loopback			? "yes" : "no",
			streamCtx->volumeControl	? "yes" : "no" );
#endif
		
		streamCtx->activeType = streamCtx->type;
		streamCtx->session = inSession;
		err = AudioStreamCreate( &streamCtx->stream );
		require_noerr( err, exit );
		
		AudioStreamSetOutputCallback( streamCtx->stream, _AudioOutputCallBack, streamCtx );
		AudioStreamSetPropertyInt64( streamCtx->stream, kAudioStreamProperty_StreamType, NULL, streamCtx->activeType );
		AudioStreamSetPropertyCString( streamCtx->stream, kAudioStreamProperty_ThreadName,
			NULL, "AirPlayAudioMain", kSizeCString );
		AudioStreamSetPropertyInt64( streamCtx->stream, kAudioStreamProperty_ThreadPriority,
			NULL, kAirPlayThreadPriority_AudioReceiver );

		err = AudioStreamSetPropertyBoolean( streamCtx->stream, kAudioStreamProperty_Input, NULL, streamCtx->input );
		require_noerr( err, exit );
		err = AudioStreamSetProperty( streamCtx->stream, kAudioStreamProperty_AudioType, NULL, streamCtx->audioType );
		require_noerr( err, exit );
		
		err = AirPlayAudioFormatToPCM( streamCtx->format, &asbd );
		require_noerr( err, exit );
		err = AudioStreamSetPropertyData( streamCtx->stream, kAudioStreamProperty_Format,
			NULL, &asbd, sizeof( asbd ) );
		require_noerr( err, exit );
		
		err = AudioStreamPrepare( streamCtx->stream );
		require_noerr( err, exit );
	}
	else if( ( streamCtx->type == kAirPlayStreamType_Invalid ) && streamCtx->stream )
	{
		AudioStreamForget( &streamCtx->stream );
		ForgetCF( &streamCtx->audioType );
		streamCtx->started = false;
		streamCtx->session = NULL;
		streamCtx->activeType = kAirPlayStreamType_Invalid;
		atrp_ulog( kLogLevelNotice, "Main audio torn down\n" );
	}
	
	if( !useMain && !useAlt )	AudioSessionEnsureTornDown();
	
	// If audio has started, make sure all the streams are started.
	
	if( spd->sessionStarted )
	{
		streamCtx = &spd->mainAudioCtx;
		if( streamCtx->stream && !streamCtx->started )
		{
			err = AudioStreamStart( streamCtx->stream );
			if( err ) atrp_ulog( kLogLevelWarning, "### Main audio start failed: %#m\n", err );
			streamCtx->started = true;
		}
		
	}
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_AudioOutputCallBack
//===========================================================================================================================

static void
	_AudioOutputCallBack( 
		uint32_t	inSampleTime, 
		uint64_t	inHostTime, 
		void *		inBuffer, 
		size_t		inLen, 
		void *		inContext )
{
	AirPlayAudioStreamPlatformContext * const		streamCtx = (AirPlayAudioStreamPlatformContext *) inContext;
	OSStatus										err;
	
	err = AirPlayReceiverSessionReadAudio( streamCtx->session, streamCtx->activeType, inSampleTime, inHostTime, 
		inBuffer, inLen );
	require_noerr( err, exit );
	
exit:
	return;
}

//===========================================================================================================================
//	_HandleAudioSessionEvent
//===========================================================================================================================

static void _HandleAudioSessionEvent( AudioSessionEventType inType, CFTypeRef inParam, void *inContext )
{
	AirPlayReceiverSessionRef const					session	= (AirPlayReceiverSessionRef) inContext;
	AirPlayReceiverSessionPlatformData * const		spd		= (AirPlayReceiverSessionPlatformData *) session->platformPtr;

	(void) inParam;

	switch( inType )
	{
		case kAudioSessionEventAudioInterrupted:
			// Restart if we've been interrupted.
			spd->mainAudioCtx.started = false;
			_UpdateStreams( session );
			break;
		
		case kAudioSessionEventAudioServicesWereReset:
			// Rebuild streams.
		
			AudioStreamForget( &spd->mainAudioCtx.stream );
			ForgetCF( &spd->mainAudioCtx.audioType );
			spd->mainAudioCtx.started = false;
			
			_UpdateStreams( session );
			break;

		default:
			dlogassert( "Bad event type: %u", inType );
			break;
	}
}

#if 0
#pragma mark -
#pragma mark == Utilities ==
#endif

