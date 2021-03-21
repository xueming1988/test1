/*
	File:    AirTunesServer.c
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

	Copyright (C) 2007-2014 Apple Inc. All Rights Reserved.
*/

#include "AirTunesServer.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "AirPlayCommon.h"
#include "AirPlayReceiverServer.h"
#include "AirPlayReceiverServerPriv.h"
#include "AirPlayReceiverSession.h"
#include "AirPlayReceiverSessionPriv.h"
#include "AirPlaySettings.h"
#include "AirPlayVersion.h"
#include "APSCommonServices.h"
#include "APSDebugServices.h"
#include "Base64Utils.h"
#include "CFUtils.h"
#include "HTTPServer.h"
#include "HTTPUtils.h"
#include "NetUtils.h"
#include "RandomNumberUtils.h"
#include "StringUtils.h"
#include "SystemUtils.h"
#include "ThreadUtils.h"
#include "TickUtils.h"
#include "URLUtils.h"

#if( AIRPLAY_DACP )
	#include "AirTunesDACP.h"
#endif
	#include "APSDMAP.h"
	#include "APSDMAPUtils.h"
#if( AIRPLAY_MFI )
	#include "APSMFiSAP.h"
#endif
#if( AIRPLAY_SDP_SETUP )
	#include "SDPUtils.h"
#endif

//===========================================================================================================================
//	Internal
//===========================================================================================================================

typedef struct
{
	HTTPServerRef		httpServer;                     // HTTP server
	dispatch_queue_t	queue;                          // queue to run all operations and deliver callbacks on

	Float32				volume;
	uint8_t				httpTimedNonceKey[ 16 ];

	Boolean				started;
}	AirTunesServer;

typedef struct AirTunesConnection
{
	AirTunesServer				*atServer;

	uint64_t					clientDeviceID;
	uint8_t						clientInterfaceMACAddress[ 6 ];
	char						clientName[ 128 ];
	uint64_t					clientSessionID;
	uint32_t					clientVersion;
	Boolean						didAnnounce;
	Boolean						didAudioSetup;
	Boolean						didRecord;

	char						ifName[ IF_NAMESIZE + 1 ];	// Name of the interface the connection was accepted on.

	Boolean						httpAuthentication_IsAuthenticated;

	AirPlayCompressionType		compressionType;
	uint32_t					samplesPerFrame;

#if( AIRPLAY_MFI )
	APSMFiSAPRef				MFiSAP;
	Boolean						MFiSAPDone;
#endif
	uint8_t						encryptionKey[ 16 ];
	uint8_t						encryptionIV[ 16 ];
	Boolean						usingEncryption;

	uint32_t					minLatency, maxLatency;

	AirPlayReceiverSessionRef	session;
}	AirTunesConnection;

AirPlayCompressionType				gAirPlayAudioCompressionType	= kAirPlayCompressionType_Undefined;
static AirTunesServer *				gAirTunesServer					= NULL;
static Boolean						gAirTunesPlaybackAllowed		= true;
static Boolean						gAirTunesDenyInterruptions		= false;
#if( AIRPLAY_DACP )
	static AirTunesDACPClientRef	gAirTunesDACPClient				= NULL;
#endif
#if( AIRPLAY_PASSWORDS )
	static pthread_mutex_t			gAuthThrottleLock				= PTHREAD_MUTEX_INITIALIZER;
	static uint32_t					gAuthThrottleCounter1			= 0;
	static uint32_t					gAuthThrottleCounter2			= 0;
	static uint64_t					gAuthThrottleStartTicks			= 0;
#endif

ulog_define( AirTunesServerCore, kLogLevelNotice, kLogFlags_Default, "AirPlay",  NULL );
#define ats_ucat()						&log_category_from_name( AirTunesServerCore )
#define ats_ulog( LEVEL, ... )			ulog( ats_ucat(), (LEVEL), __VA_ARGS__ )

ulog_define( AirTunesServerHTTP, kLogLevelNotice, kLogFlags_Default, "AirPlay",  NULL );
#define ats_http_ucat()					&log_category_from_name( AirTunesServerHTTP )
#define ats_http_ulog( LEVEL, ... )		ulog( ats_http_ucat(), (LEVEL), __VA_ARGS__ )

	#define PairingDescription( atCnx )				""

	#define AirTunesConnection_DidSetup( atCnx )	( (atCnx)->didAudioSetup )

#if 0
#pragma mark == Prototypes ==
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static HTTPConnectionRef _AirTunesServer_FindActiveConnection( AirTunesServer *inServer );
static void _AirTunesServer_HijackConnections( AirTunesServer *inServer, HTTPConnectionRef inHijacker );
static void _AirTunesServer_RemoveAllConnections( AirTunesServer *inServer, HTTPConnectionRef inConnectionToKeep );

static OSStatus		_connectionInitialize( HTTPConnectionRef inCnx );
static void			_connectionFinalize( HTTPConnectionRef inCnx );
static void			_connectionDestroy( HTTPConnectionRef inCnx );
static OSStatus		_connectionHandleMessage( HTTPConnectionRef inCnx, HTTPMessageRef inMsg );

#if( AIRPLAY_SDP_SETUP )
	static HTTPStatus	_requestProcessAnnounce( HTTPConnectionRef inCnx, HTTPMessageRef inMsg );
	static OSStatus		_requestProcessAnnounceAudio( HTTPConnectionRef inCnx, HTTPMessageRef inMsg );
#endif // AIRPLAY_SDP_SETUP
#if( AIRPLAY_MFI )
	static HTTPStatus	_requestProcessAuthSetup( HTTPConnectionRef inCnx, HTTPMessageRef inMsg );
#endif
static HTTPStatus	_requestProcessCommand( HTTPConnectionRef inCnx, HTTPMessageRef inMsg );
static HTTPStatus	_requestProcessFeedback( HTTPConnectionRef inCnx, HTTPMessageRef inMsg );
	static HTTPStatus	_requestProcessFlush( HTTPConnectionRef inCnx, HTTPMessageRef inMsg );
static HTTPStatus	_requestProcessGetLog( HTTPConnectionRef inCnx );
static HTTPStatus	_requestProcessInfo( HTTPConnectionRef inCnx, HTTPMessageRef inMsg );
#if( AIRPLAY_METRICS )
	static HTTPStatus	_requestProcessMetrics( HTTPConnectionRef inCnx, HTTPMessageRef inMsg );
#endif
static HTTPStatus	_requestProcessOptions( HTTPConnectionRef inCnx );
static HTTPStatus	_requestProcessGetParameter( HTTPConnectionRef inCnx, HTTPMessageRef inMsg );
static HTTPStatus	_requestProcessSetParameter( HTTPConnectionRef inCnx, HTTPMessageRef inMsg );
	static HTTPStatus
		_requestProcessSetParameterArtwork(
			HTTPConnectionRef	inCnx,
			HTTPMessageRef		inMsg,
			const char *		inMIMETypeStr,
			size_t				inMIMETypeLen );
	static HTTPStatus	_requestProcessSetParameterDMAP( HTTPConnectionRef inCnx, HTTPMessageRef inMsg );
static HTTPStatus	_requestProcessSetParameterText( HTTPConnectionRef inCnx, HTTPMessageRef inMsg );
static HTTPStatus	_requestProcessSetProperty( HTTPConnectionRef inCnx, HTTPMessageRef inMsg );
static HTTPStatus	_requestProcessRecord( HTTPConnectionRef inCnx, HTTPMessageRef inMsg );
static HTTPStatus	_requestProcessSetup( HTTPConnectionRef inCnx, HTTPMessageRef inMsg );
#if( AIRPLAY_SDP_SETUP )
	static HTTPStatus	_requestProcessSetupSDPAudio( HTTPConnectionRef inCnx, HTTPMessageRef inMsg );
#endif // AIRPLAY_SDP_SETUP
static HTTPStatus	_requestProcessSetupPlist( HTTPConnectionRef inCnx, HTTPMessageRef inMsg );
static HTTPStatus	_requestProcessTearDown( HTTPConnectionRef inCnx, HTTPMessageRef inMsg );

static OSStatus		_requestCreateSession( HTTPConnectionRef inCnx, Boolean inUseEvents );
static OSStatus
	_requestDecryptKey(
		HTTPConnectionRef		inCnx,
		CFDictionaryRef			inRequestParams,
		AirPlayEncryptionType	inType,
		uint8_t					inKeyBuf[ 16 ],
		uint8_t					inIVBuf[ 16 ] );
#if( AIRPLAY_PASSWORDS )
	static HTTPStatus	_requestHTTPAuthorization_CopyPassword( HTTPAuthorizationInfoRef inInfo, char **outPassword );
	static Boolean		_requestHTTPAuthorization_IsValidNonce( HTTPAuthorizationInfoRef inInfo );
#endif
#if( AIRPLAY_PASSWORDS )
	static Boolean		_requestRequiresAuth( HTTPMessageRef inMsg );
#endif
static HTTPStatus
	_requestSendPlistResponse(
		HTTPConnectionRef	inCnx,
		HTTPMessageRef		inMsg,
		CFPropertyListRef	inPlist,
		OSStatus *			outErr );


static HTTPStatus
	_requestSendPlistResponseInfo(
		HTTPConnectionRef	inCnx,
		HTTPMessageRef		inMsg,
		CFPropertyListRef	inPlist,
		OSStatus *			outErr );
#if 0
#pragma mark == AirTunes Control ==
#endif

//===========================================================================================================================
//	AirTunesServer_EnsureCreated
//===========================================================================================================================

OSStatus AirTunesServer_EnsureCreated( void )
{
	OSStatus err;

	if( !gAirTunesServer )
	{
		HTTPServerDelegate delegate;

		gAirTunesServer	= (AirTunesServer *) calloc( 1, sizeof( AirTunesServer ) );
		require_action( gAirTunesServer, exit, err = kNoMemoryErr );

		gAirTunesServer->queue = dispatch_queue_create( "AirTunesServerQueue", 0 );
		require_action( gAirTunesServer->queue, exit, err = kNoMemoryErr );

		gAirTunesPlaybackAllowed = true;

		RandomBytes( gAirTunesServer->httpTimedNonceKey, sizeof( gAirTunesServer->httpTimedNonceKey ) );
		gAirTunesServer->volume = 0;

		HTTPServerDelegateInit( &delegate );
		delegate.initializeConnection_f = _connectionInitialize;
		delegate.handleMessage_f        = _connectionHandleMessage;
		delegate.finalizeConnection_f   = _connectionFinalize;

		err = HTTPServerCreate( &gAirTunesServer->httpServer, &delegate );
		require_noerr( err, exit );

		gAirTunesServer->httpServer->listenPort = -kAirPlayPort_RTSPControl;
		HTTPServerSetDispatchQueue( gAirTunesServer->httpServer, gAirTunesServer->queue );
		//HTTPServerSetLogging( gAirTunesServer->httpServer, ats_ucat() );

#if( AIRPLAY_DACP )
		check( gAirTunesDACPClient == NULL );
		err = AirTunesDACPClient_Create( &gAirTunesDACPClient );
		require_noerr( err, exit );
#endif
	}
	err = kNoErr;

exit:
	if ( err ) AirTunesServer_EnsureDeleted();
	return err;
}

//===========================================================================================================================
//	AirTunesServer_EnsureDeleted
//===========================================================================================================================

void	AirTunesServer_EnsureDeleted( void )
{
	if( gAirTunesServer )
	{
#if( AIRPLAY_DACP )
		if( gAirTunesDACPClient )
		{
			AirTunesDACPClient_Delete( gAirTunesDACPClient );
			gAirTunesDACPClient = NULL;
		}
#endif

		HTTPServerForget( &gAirTunesServer->httpServer );
		dispatch_forget( &gAirTunesServer->queue );
		ForgetMem( &gAirTunesServer );
	}
}

//===========================================================================================================================
//	AirTunesServer_Start
//===========================================================================================================================

OSStatus	AirTunesServer_Start( void )
{
	OSStatus		err;

	require_action( gAirTunesServer && gAirTunesServer->httpServer, exit, err = kNotInitializedErr );
	require_action_quiet( !gAirTunesServer->started, exit, err = kNoErr );

	// Start HTTPServer synchronously
	err = HTTPServerStartSync( gAirTunesServer->httpServer );
	require_noerr( err, exit );

	gAirTunesServer->started = true;
	err = kNoErr;

exit:
	return err;
}

//===========================================================================================================================
//	AirTunesServer_GetListenPort
//===========================================================================================================================

int	AirTunesServer_GetListenPort( void )
{
	if( gAirTunesServer && gAirTunesServer->httpServer ) return( gAirTunesServer->httpServer->listeningPort );
	return( 0 );
}

//===========================================================================================================================
//	AirTunesServer_SetDenyInterruptions
//===========================================================================================================================

void	AirTunesServer_SetDenyInterruptions( Boolean inDeny )
{
	if( inDeny != gAirTunesDenyInterruptions )
	{
		gAirTunesDenyInterruptions = inDeny;

	}
}

//===========================================================================================================================
//	AirTunesServer_FailureOccurred
//===========================================================================================================================

void	AirTunesServer_FailureOccurred( OSStatus inError )
{
	DEBUG_USE_ONLY( inError );

	ats_ulog( kLogLevelNotice, "### Failure: %#m\n", inError );
	_AirTunesServer_RemoveAllConnections( gAirTunesServer, NULL );
}

//===========================================================================================================================
//	AirTunesServer_StopAllConnections
//===========================================================================================================================

void	AirTunesServer_StopAllConnections( void )
{
	_AirTunesServer_RemoveAllConnections( gAirTunesServer, NULL );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_AirTunesServer_PlaybackAllowed
//===========================================================================================================================
extern int platform_get_playback_allowed(void);
static Boolean _AirTunesServer_PlaybackAllowed( void )
{
	if( !platform_get_playback_allowed())
		return false;

	if( gAirTunesPlaybackAllowed )
	{
		OSStatus		err;
		Boolean			b;

		b = AirPlayReceiverServerPlatformGetBoolean( gAirPlayReceiverServer, CFSTR( kAirPlayProperty_PlaybackAllowed ),
			NULL, &err );
		if( b || ( err == kNotHandledErr ) )
		{
			return( true );
		}
	}
	return( false );
}

//===========================================================================================================================
//	AirTunesServer_AllowPlayback
//===========================================================================================================================

void	AirTunesServer_AllowPlayback( void )
{
	if( !gAirTunesPlaybackAllowed )
	{
		dlog( kLogLevelNotice, "*** ALLOW PLAYBACK ***\n" );
		gAirTunesPlaybackAllowed = true;

		#if( AIRPLAY_DACP )
			AirTunesServer_ScheduleDACPCommand( "setproperty?dmcp.device-prevent-playback=0" );
		#endif
	}
}

//===========================================================================================================================
//	AirTunesServer_PreventPlayback
//===========================================================================================================================

void	AirTunesServer_PreventPlayback( void )
{
	if( gAirTunesPlaybackAllowed )
	{
		dlog( kLogLevelNotice, "*** PREVENT PLAYBACK ***\n" );
		gAirTunesPlaybackAllowed = false;
		_AirTunesServer_RemoveAllConnections( gAirTunesServer, NULL );

		#if( AIRPLAY_DACP )
			AirTunesServer_ScheduleDACPCommand( "setproperty?dmcp.device-prevent-playback=1" );
		#endif
	}
}

#if 0
#pragma mark -
#endif

#if( AIRPLAY_DACP )
//===========================================================================================================================
//	AirTunesServer_SetDACPCallBack
//===========================================================================================================================

OSStatus	AirTunesServer_SetDACPCallBack( AirTunesDACPClientCallBackPtr inCallBack, void *inContext )
{
	return( AirTunesDACPClient_SetCallBack( gAirTunesDACPClient, inCallBack, inContext ) );
}

//===========================================================================================================================
//	AirTunesServer_ScheduleDACPCommand
//===========================================================================================================================

OSStatus	AirTunesServer_ScheduleDACPCommand( const char *inCommand )
{
	return( AirTunesDACPClient_ScheduleCommand( gAirTunesDACPClient, inCommand ) );
}
#endif

#if 0
#pragma mark -
#pragma mark == AirTunesServer ==
#endif

//===========================================================================================================================
//	_AirTunesServer_FindActiveConnection
//===========================================================================================================================

static HTTPConnectionRef _AirTunesServer_FindActiveConnection( AirTunesServer *inServer )
{
	HTTPConnectionRef cnx;

	if( !( inServer && inServer->httpServer ) ) return NULL;

	for( cnx = inServer->httpServer->connections; cnx; cnx = cnx->next )
	{
		AirTunesConnection *atCnx = (AirTunesConnection *) cnx->userContext;

		if( atCnx->didAnnounce )
		{
			return( cnx );
		}
	}

	return( NULL );
}

//===========================================================================================================================
//	_AirTunesServer_HijackConnections
//===========================================================================================================================

static void _AirTunesServer_HijackConnections( AirTunesServer *inServer, HTTPConnectionRef inHijacker )
{
	HTTPConnectionRef *	next;
	HTTPConnectionRef	conn;

	if( !( inServer && inServer->httpServer ) ) return;

	// Close any connections that have started audio (should be 1 connection at most).
	// This leaves connections that haven't started audio because they may just be doing OPTIONS requests, etc.

	next = &inServer->httpServer->connections;
	while( ( conn = *next ) != NULL )
	{
		AirTunesConnection *atConn = (AirTunesConnection *) conn->userContext;

		if( ( conn != inHijacker ) && atConn->didAnnounce )
		{
			ats_ulog( kLogLevelNotice, "*** Hijacking connection %##a for %##a\n", &conn->peerAddr, &inHijacker->peerAddr );
			*next = conn->next;
			_connectionDestroy( conn );
		}
		else
		{
			next = &conn->next;
		}
	}

}

//===========================================================================================================================
//	_AirTunesServer_RemoveAllConnections
//===========================================================================================================================

static void _AirTunesServer_RemoveAllConnections( AirTunesServer *inServer, HTTPConnectionRef inConnectionToKeep )
{
	HTTPConnectionRef *	next;
	HTTPConnectionRef	conn;

	if( !( inServer && inServer->httpServer ) ) return;

	next = &inServer->httpServer->connections;
	while( ( conn = *next ) != NULL )
	{
		if( conn == inConnectionToKeep )
		{
			next = &conn->next;
			continue;
		}

		*next = conn->next;
		_connectionDestroy( conn );
	}

	check( inServer->httpServer->connections == inConnectionToKeep );
	check( !inServer->httpServer->connections || ( inServer->httpServer->connections->next == NULL ) );
}

#if 0
#pragma mark -
#pragma mark == AirTunesConnection ==
#endif

//===========================================================================================================================
//	_connectionInitialize
//===========================================================================================================================

static OSStatus _connectionInitialize( HTTPConnectionRef inCnx )
{
	OSStatus err;
	AirTunesConnection *atCnx;

	atCnx = (AirTunesConnection *) calloc( 1, sizeof( AirTunesConnection ) );
	require_action( atCnx, exit, err = kNoMemoryErr );

	atCnx->atServer			= gAirTunesServer;
	atCnx->compressionType	= kAirPlayCompressionType_Undefined;
	inCnx->userContext		= atCnx;
	err						= kNoErr;

    SocketSetKeepAlive( inCnx->sock, kAirPlayDataTimeoutSecs / 5, 5 );

exit:
	return err;
}

//===========================================================================================================================
//	_connectionFinalize
//===========================================================================================================================

static void _connectionFinalize( HTTPConnectionRef inCnx )
{
	AirTunesConnection *atCnx;

	atCnx = (AirTunesConnection *) inCnx->userContext;
	if( !atCnx ) return;

	atCnx->atServer = NULL;

#if( AIRPLAY_DACP )
	AirTunesDACPClient_StopSession( gAirTunesDACPClient );
#endif
	if( atCnx->session )
	{
		AirPlayReceiverSessionTearDown( atCnx->session, NULL, atCnx->didAnnounce ? kConnectionErr : kNoErr, NULL );
		CFRelease( atCnx->session );
		atCnx->session = NULL;
		AirPlayReceiverServerSetBoolean( gAirPlayReceiverServer, CFSTR( kAirPlayProperty_Playing ), NULL, false );
	}
#if( AIRPLAY_MFI )
	if( atCnx->MFiSAP )
	{
		APSMFiSAP_Delete( atCnx->MFiSAP );
		atCnx->MFiSAP = NULL;
		atCnx->MFiSAPDone = false;
	}
#endif

	ForgetMem( &inCnx->userContext );
}

//===========================================================================================================================
//	_connectionDestroy
//===========================================================================================================================

static void _connectionDestroy( HTTPConnectionRef inCnx )
{
	HTTPConnectionStop( inCnx );
	if( inCnx->selfAddr.sa.sa_family != AF_UNSPEC )
	{
		ats_ulog( kLogLevelInfo, "Closing  connection from %##a to %##a\n", &inCnx->peerAddr, &inCnx->selfAddr );
	}
	CFRelease( inCnx );
}

static void refresh_client_info(uint64_t new_dacpID, uint32_t new_activeRemoteID)
{
	static uint64_t old_dacpID = 0;
	static uint32_t old_activeRemoteID = 0;

	if( new_dacpID  && new_activeRemoteID )
	{
		if( (old_dacpID != new_dacpID) || (old_activeRemoteID != new_activeRemoteID))
		{
			fprintf(stderr, "%s Client changed: old(%llX, %u) new(%llX, %u)\n",
				__FUNCTION__, old_dacpID, old_activeRemoteID, new_dacpID, new_activeRemoteID);
			old_dacpID = new_dacpID;
			old_activeRemoteID = new_activeRemoteID;
			char mv_msg[512];
			memset((void *)&mv_msg, 0, sizeof(mv_msg));
			sprintf(mv_msg, "AIRPLAY_PLAYDACP-ID:%llXACTIVE_REMOTE:%u", new_dacpID, new_activeRemoteID);
			SocketClientReadWriteMsg("/tmp/Requesta01controller",mv_msg,strlen(mv_msg),NULL,NULL,0);
		}
	}
}

//===========================================================================================================================
//	_connectionHandleMessage
//===========================================================================================================================

#define GetHeaderValue( req, name, outVal, outValLen ) \
	HTTPGetHeaderField( (req)->header.buf, (req)->header.len, name, NULL, NULL, outVal, outValLen, NULL )

static OSStatus _connectionHandleMessage( HTTPConnectionRef inCnx, HTTPMessageRef request )
{
	OSStatus				err;
	HTTPMessageRef			response	= inCnx->responseMsg;
	const char * const		methodPtr	= request->header.methodPtr;
	size_t const			methodLen	= request->header.methodLen;
	const char * const		pathPtr		= request->header.url.pathPtr;
	size_t const			pathLen		= request->header.url.pathLen;
	Boolean					logHTTP		= true;
	AirTunesConnection *	atCnx		= (AirTunesConnection *) inCnx->userContext;
	const char *			httpProtocol;
	HTTPStatus				status;
	const char *			cSeqPtr		= NULL;
	size_t					cSeqLen		= 0;

	require_action( atCnx, exit, err = kParamErr );

	httpProtocol = ( strnicmp_prefix(request->header.protocolPtr, request->header.protocolLen, "HTTP" ) == 0 )
		? "HTTP/1.1" : kAirTunesHTTPVersionStr;

	// OPTIONS and /feedback requests are too chatty so don't log them by default.
	if( ( ( strnicmpx( methodPtr, methodLen, "OPTIONS" ) == 0 ) ||
		( ( strnicmpx( methodPtr, methodLen, "POST" ) == 0 ) && ( strnicmp_suffix( pathPtr, pathLen, "/feedback" ) == 0 ) ) ) &&
		!log_category_enabled( ats_http_ucat(), kLogLevelChatty ) )
	{
		logHTTP = false;
	}
	if( logHTTP ) LogHTTP( ats_http_ucat(), ats_http_ucat(), request->header.buf, request->header.len,
		request->bodyPtr, request->bodyLen );

	if( atCnx->session ) ++atCnx->session->source.activityCount;

	GetHeaderValue( request, kHTTPHeader_CSeq, &cSeqPtr, &cSeqLen );

	// Parse the client device's ID. If not provided (e.g. older device) then fabricate one from the IP address.

	HTTPScanFHeaderValue( request->header.buf, request->header.len, kAirPlayHTTPHeader_DeviceID, "%llx", &atCnx->clientDeviceID );
	if( atCnx->clientDeviceID == 0 ) atCnx->clientDeviceID = SockAddrToDeviceID( &inCnx->peerAddr );

	if( *atCnx->clientName == '\0' )
	{
		const char *		namePtr	= NULL;
		size_t				nameLen	= 0;

		GetHeaderValue( request, kAirPlayHTTPHeader_ClientName, &namePtr, &nameLen );
		if( nameLen > 0 ) TruncateUTF8( namePtr, nameLen, atCnx->clientName, sizeof( atCnx->clientName ), true );
	}

#if( AIRPLAY_PASSWORDS )
	// Verify authentication if required.

	if( !atCnx->httpAuthentication_IsAuthenticated && _requestRequiresAuth( request ) )
	{
		if( *gAirPlayReceiverServer->playPassword != '\0' )
		{
			HTTPAuthorizationInfo		authInfo;

			memset( &authInfo, 0, sizeof( authInfo ) );
			authInfo.serverScheme			= kHTTPAuthorizationScheme_Digest;
			authInfo.copyPasswordFunction	= _requestHTTPAuthorization_CopyPassword;
			authInfo.copyPasswordContext	= inCnx;
			authInfo.isValidNonceFunction	= _requestHTTPAuthorization_IsValidNonce;
			authInfo.isValidNonceContext	= inCnx;
			authInfo.headerPtr				= request->header.buf;
			authInfo.headerLen				= request->header.len;
			authInfo.requestMethodPtr		= methodPtr;
			authInfo.requestMethodLen		= methodLen;
			authInfo.requestURLPtr			= request->header.urlPtr;
			authInfo.requestURLLen			= request->header.urlLen;
			status = HTTPVerifyAuthorization( &authInfo );
			if( status != kHTTPStatus_OK )
			{
				if( authInfo.badMatch )
				{
					uint64_t		ticks, nextTicks;

					pthread_mutex_lock( &gAuthThrottleLock );
						ticks = UpTicks();
						if( gAuthThrottleStartTicks == 0 ) gAuthThrottleStartTicks = ticks;
						if( gAuthThrottleCounter1 < 10 )
						{
							++gAuthThrottleCounter1;
						}
						else
						{
							gAuthThrottleCounter2 = gAuthThrottleCounter2 ? ( gAuthThrottleCounter2 * 2 ) : 1;
							if( gAuthThrottleCounter2 > 10800 ) gAuthThrottleCounter2 = 10800; // 3 hour cap.
							nextTicks = gAuthThrottleStartTicks + ( gAuthThrottleCounter2 * UpTicksPerSecond() );
							if( nextTicks > ticks )
							{
								pthread_mutex_unlock( &gAuthThrottleLock );
								ats_ulog( kLogLevelNotice, "Auth throttle %llu seconds\n", ( nextTicks - ticks ) / UpTicksPerSecond() );
								status = kHTTPStatus_NotEnoughBandwidth;
								goto SendResponse;
							}
						}
					pthread_mutex_unlock( &gAuthThrottleLock );
				}

				goto SendResponse;
			}
			pthread_mutex_lock( &gAuthThrottleLock );
				gAuthThrottleCounter1	= 0;
				gAuthThrottleCounter2	= 0;
				gAuthThrottleStartTicks	= 0;
			pthread_mutex_unlock( &gAuthThrottleLock );
		}
		atCnx->httpAuthentication_IsAuthenticated = true;
	}
#endif

#if( AIRPLAY_DACP )
	// Save off the latest DACP info for sending commands back to the controller.

	if( atCnx->didAudioSetup )
	{
		uint64_t		dacpID;
		uint32_t		activeRemoteID;

		if( HTTPScanFHeaderValue( request->header.buf, request->header.len, "DACP-ID", "%llX", &dacpID ) == 1 )
		{
			AirTunesDACPClient_SetDACPID( gAirTunesDACPClient, dacpID );
		}
		if( HTTPScanFHeaderValue( request->header.buf, request->header.len, "Active-Remote", "%u", &activeRemoteID ) == 1 )
		{
			AirTunesDACPClient_SetActiveRemoteID( gAirTunesDACPClient, activeRemoteID );
		}
		refresh_client_info(dacpID, activeRemoteID);
	}
#endif

	// Process the request. Assume success initially, but we'll change it if there is an error.
	// Note: methods are ordered below roughly to how often they are used (most used earlier).

	err = HTTPHeader_InitResponse( &response->header, httpProtocol, kHTTPStatus_OK, NULL );
	require_noerr( err, exit );

	response->bodyLen = 0;


	if(      strnicmpx( methodPtr, methodLen, "OPTIONS" )			== 0 ) status = _requestProcessOptions( inCnx );
	else if( strnicmpx( methodPtr, methodLen, "SET_PARAMETER" )		== 0 ) status = _requestProcessSetParameter( inCnx, request );
	else if( strnicmpx( methodPtr, methodLen, "FLUSH" )				== 0 ) status = _requestProcessFlush( inCnx, request );
	else if( strnicmpx( methodPtr, methodLen, "GET_PARAMETER" ) 	== 0 ) status = _requestProcessGetParameter( inCnx, request );
	else if( strnicmpx( methodPtr, methodLen, "RECORD" )			== 0 ) status = _requestProcessRecord( inCnx, request );
#if( AIRPLAY_SDP_SETUP )
	else if( strnicmpx( methodPtr, methodLen, "ANNOUNCE" )			== 0 ) status = _requestProcessAnnounce( inCnx, request );
#endif
	else if( strnicmpx( methodPtr, methodLen, "SETUP" )				== 0 ) status = _requestProcessSetup( inCnx, request );
	else if( strnicmpx( methodPtr, methodLen, "TEARDOWN" )			== 0 ) status = _requestProcessTearDown( inCnx, request );
	else if( strnicmpx( methodPtr, methodLen, "GET" )				== 0 )
	{
		if( 0 ) {}
		else if( strnicmp_suffix( pathPtr, pathLen, "/log" )		== 0 ) status = _requestProcessGetLog( inCnx );
		else if( strnicmp_suffix( pathPtr, pathLen, "/info" )		== 0 ) status = _requestProcessInfo( inCnx, request );
		else { dlog( kLogLevelNotice, "### Unsupported GET: '%.*s'\n", (int) pathLen, pathPtr ); status = kHTTPStatus_NotFound; }
	}
	else if( strnicmpx( methodPtr, methodLen, "POST" )				== 0 )
	{
		if( 0 ) {}
		#if( AIRPLAY_MFI )
		else if( strnicmp_suffix( pathPtr, pathLen, "/auth-setup" )	== 0 ) status = _requestProcessAuthSetup( inCnx, request );
		#endif
		else if( strnicmp_suffix( pathPtr, pathLen, "/command" )	== 0 ) status = _requestProcessCommand( inCnx, request );
		#if( AIRPLAY_MFI )
		else if( strnicmp_suffix( pathPtr, pathLen, "/diag-info" )	== 0 ) status = kHTTPStatus_OK;
		#endif
		else if( strnicmp_suffix( pathPtr, pathLen, "/feedback" )	== 0 ) status = _requestProcessFeedback( inCnx, request );
		else if( strnicmp_suffix( pathPtr, pathLen, "/info" )		== 0 ) status = _requestProcessInfo( inCnx, request );
		#if( AIRPLAY_METRICS )
		else if( strnicmp_suffix( pathPtr, pathLen, "/metrics" )	== 0 ) status = _requestProcessMetrics( inCnx, request );
		#endif
		/*  transfer the message belongs to wac  */
		else if( strnicmp_suffix( pathPtr, pathLen, "/config" )    == 0 ||
			strnicmp_suffix( pathPtr, pathLen, "/configured" )    == 0)
		{
			/*
			 FIXME: just stop airplay services for wac, a bug of airport utility ?
			 * why airport utility send "/config" and "/configured" messages to _raop(airplay) when _mfi-config(WAC) exists
			 */
			dlogassert( "!!!!![Airplay] Bad POST: '%.*s'", (int) pathLen, pathPtr );
			if( strnicmp_suffix( pathPtr, pathLen, "/configured" )	  == 0)
			{
				//HTTPHeader_SetField( &response->header, kHTTPHeader_ContentLength, "0" );
				printf("%s send response to client[%s] len=%d\n", __FUNCTION__, response->header.buf, response->header.len);
				// 	HTTP/1.1 200 OK
				memset((void *)&response->header.buf, 0, sizeof(response->header.buf));
				sprintf(response->header.buf, "HTTP/1.1 200 OK\r\n\r\n");
				response->header.len = strlen(response->header.buf);
				HTTPConnectionSendResponse( inCnx );
				system("echo 1 > /tmp/airplay_receive_configed");
				return kNoErr;
 			}

			if( access("/tmp/wac_mode", R_OK) == 0 )
				SocketClientReadWriteMsg("/tmp/RequestAirplay","stopAirplay", strlen("stopAirplay"),NULL,NULL,0);
			status = kHTTPStatus_NotFound;
		}
		else { dlogassert( "Bad POST: '%.*s'", (int) pathLen, pathPtr ); status = kHTTPStatus_NotFound; }
	}
	else { dlogassert( "Bad method: %.*s", (int) methodLen, methodPtr ); status = kHTTPStatus_NotImplemented; }
	goto SendResponse;

SendResponse:

	// If an error occurred, reset the response message with a new status.

	if( status != kHTTPStatus_OK )
	{
		err = HTTPHeader_InitResponse( &response->header, httpProtocol, status, NULL );
		require_noerr( err, exit );
		response->bodyLen = 0;

		err = HTTPHeader_SetField( &response->header, kHTTPHeader_ContentLength, "0" );
		require_noerr( err, exit );
	}

	// Server

	err = HTTPHeader_SetField( &response->header, kHTTPHeader_Server, "AirTunes/%s", kAirPlaySourceVersionStr );
	require_noerr( err, exit );

	// WWW-Authenticate

	if( status == kHTTPStatus_Unauthorized )
	{
		char		nonce[ 64 ];

		err = HTTPMakeTimedNonce( kHTTPTimedNonceETagPtr, kHTTPTimedNonceETagLen,
			atCnx->atServer->httpTimedNonceKey, sizeof( atCnx->atServer->httpTimedNonceKey ),
			nonce, sizeof( nonce ), NULL );
		require_noerr( err, exit );

		err = HTTPHeader_SetField( &response->header, kHTTPHeader_WWWAuthenticate, "Digest realm=\"airplay\", nonce=\"%s\"", nonce );
		require_noerr( err, exit );
	}

	// CSeq

	if( cSeqPtr )
	{
		err = HTTPHeader_SetField( &response->header, kHTTPHeader_CSeq, "%.*s", (int) cSeqLen, cSeqPtr );
		require_noerr( err, exit );
	}

	// Apple-Challenge

	if( logHTTP ) LogHTTP( ats_http_ucat(), ats_http_ucat(), response->header.buf, response->header.len,
		response->bodyPtr, response->bodyLen );

	err = HTTPConnectionSendResponse( inCnx );
	require_noerr( err, exit );

exit:
	return( err );
}

#if 0
#pragma mark -
#pragma mark == Request Processing ==
#endif

#if( AIRPLAY_SDP_SETUP )
//===========================================================================================================================
//	_requestProcessAnnounce
//===========================================================================================================================

static HTTPStatus _requestProcessAnnounce( HTTPConnectionRef inCnx, HTTPMessageRef request )
{
	HTTPStatus			status;
	OSStatus			err;
	const char *		src;
	const char *		end;
	const char *		ptr;
	size_t				len;
	const char *		sectionPtr;
	const char *		sectionEnd;
	const char *		valuePtr;
	size_t				valueLen;
	AirTunesConnection	*atCnx = (AirTunesConnection *) inCnx->userContext;

	ats_ulog( kAirPlayPhaseLogLevel, "Announce\n" );

	// Make sure content type is SDP.

	src = NULL;
	len = 0;
	GetHeaderValue( request, kHTTPHeader_ContentType, &src, &len );
	if( strnicmpx( src, len, kMIMEType_SDP ) != 0 )
	{
		dlogassert( "bad Content-Type: '%.*s'", (int) len, src );
		status = kHTTPStatus_NotAcceptable;
		goto exit;
	}

	// Reject the request if we're not in the right state.

	if( atCnx->didAnnounce )
	{
		dlogassert( "ANNOUNCE not allowed twice" );
		status = kHTTPStatus_MethodNotValidInThisState;
		goto exit;
	}

	// Reject all requests if playback is not allowed.

	if( !_AirTunesServer_PlaybackAllowed() )
	{
		ats_ulog( kLogLevelNotice, "### Announce denied because playback is not allowed\n" );
		status = kHTTPStatus_NotEnoughBandwidth; // Make us look busy to the sender.
		goto exit;
	}

	// If we're denying interrupts then reject if there's already an active session.
	// Otherwise, hijack any active session to start the new one (last-in-wins).

	if( gAirTunesDenyInterruptions )
	{
		HTTPConnectionRef activeCnx;

		activeCnx = _AirTunesServer_FindActiveConnection( atCnx->atServer );
		if( activeCnx )
		{
			ats_ulog( kLogLevelNotice, "Denying interruption from %##a due to %##a\n", &inCnx->peerAddr, &activeCnx->peerAddr );
			status = kHTTPStatus_NotEnoughBandwidth;
			goto exit;
		}
	}
	_AirTunesServer_HijackConnections( atCnx->atServer, inCnx );

	// Get the session ID from the origin line with the format: <username> <session-id> ...

	src = (const char *) request->bodyPtr;
	end = src + request->bodyOffset;
	err = SDPFindSessionSection( src, end, &sectionPtr, &sectionEnd, &src );
	require_noerr_action( err, exit, status = kHTTPStatus_SessionNotFound );

	valuePtr = NULL;
	valueLen = 0;
	SDPFindType( sectionPtr, sectionEnd, 'o', &valuePtr, &valueLen, NULL );
	SNScanF( valuePtr, valueLen, "%*s %llu", &atCnx->clientSessionID );

	// Get the sender's name from the session information.

	valuePtr = NULL;
	valueLen = 0;
	SDPFindType( sectionPtr, sectionEnd, 'i', &valuePtr, &valueLen, NULL );
	TruncateUTF8( valuePtr, valueLen, atCnx->clientName, sizeof( atCnx->clientName ), true );

	// Audio

	err = SDPFindMediaSection( src, end, NULL, NULL, &ptr, &len, &src );
	require_noerr_action( err, exit, status = kHTTPStatus_NotAcceptable );

	if( strncmp_prefix( ptr, len, "audio" ) == 0 )
	{
		err = _requestProcessAnnounceAudio( inCnx, request );
		require_noerr_action( err, exit, status = kHTTPStatus_NotAcceptable );
	}

	// Success.

	strlcpy( gAirPlayAudioStats.ifname, inCnx->ifName, sizeof( gAirPlayAudioStats.ifname ) );
	atCnx->didAnnounce = true;
	status = kHTTPStatus_OK;

exit:
	return( status );
}

//===========================================================================================================================
//	_requestProcessAnnounceAudio
//===========================================================================================================================

static OSStatus _requestProcessAnnounceAudio( HTTPConnectionRef inCnx, HTTPMessageRef request )
{
	OSStatus			err;
	const char *		src;
	const char *		end;
	const char *		ptr;
	size_t				len;
	const char *		mediaSectionPtr;
	const char *		mediaSectionEnd;
	int					n;
	int					mediaPayloadType;
	int					payloadType;
	const char *		encodingPtr;
	size_t				encodingLen;
	const char *		aesKeyPtr;
#if( AIRTUNES_FAIRPLAY || AIRPLAY_MFI || AIRTUNES_RSA )
	size_t				aesKeyLen;
#endif
	const char *		aesIVPtr;
	size_t				aesIVLen;
	size_t				decodedLen;
	AirTunesConnection	*atCnx = (AirTunesConnection *) inCnx->userContext;

	src = (const char *) request->bodyPtr;
	end = src + request->bodyOffset;

	//-----------------------------------------------------------------------------------------------------------------------
	//	Compression
	//-----------------------------------------------------------------------------------------------------------------------

	err = SDPFindMediaSection( src, end, &mediaSectionPtr, &mediaSectionEnd, &ptr, &len, &src );
	require_noerr( err, exit );

	n = SNScanF( ptr, len, "audio 0 RTP/AVP %d", &mediaPayloadType );
	require_action( n == 1, exit, err = kUnsupportedErr );

	err = SDPFindAttribute( mediaSectionPtr, mediaSectionEnd, "rtpmap", &ptr, &len, NULL );
	require_noerr( err, exit );

	n = SNScanF( ptr, len, "%d %&s", &payloadType, &encodingPtr, &encodingLen );
	require_action( n == 2, exit, err = kMalformedErr );
	require_action( payloadType == mediaPayloadType, exit, err = kMismatchErr );

	if( 0 ) {} // Empty if to simplify conditional logic below.

	// AppleLossless

	else if( strnicmpx( encodingPtr, encodingLen, "AppleLossless" ) == 0 )
	{
		int		frameLength, version, bitDepth, pb, mb, kb, channelCount, maxRun, maxFrameBytes, avgBitRate, sampleRate;

		// Parse format parameters. For example: a=fmtp:96 352 0 16 40 10 14 2 255 0 0 44100

		err = SDPFindAttribute( mediaSectionPtr, mediaSectionEnd, "fmtp", &ptr, &len, NULL );
		require_noerr( err, exit );

		n = SNScanF( ptr, len, "%d %d %d %d %d %d %d %d %d %d %d %d",
			&payloadType, &frameLength, &version, &bitDepth, &pb, &mb, &kb, &channelCount, &maxRun, &maxFrameBytes,
			&avgBitRate, &sampleRate );
		require_action( n == 12, exit, err = kMalformedErr );
		require_action( payloadType == mediaPayloadType, exit, err = kMismatchErr );

		atCnx->compressionType = kAirPlayCompressionType_ALAC;
		atCnx->samplesPerFrame = frameLength;
	}

	// AAC

	// PCM

	else if( ( strnicmpx( encodingPtr, encodingLen, "L16/44100/2" )	== 0 ) ||
			 ( strnicmpx( encodingPtr, encodingLen, "L16" )			== 0 ) )
	{
		atCnx->compressionType = kAirPlayCompressionType_PCM;
		atCnx->samplesPerFrame = kAirPlaySamplesPerPacket_PCM;
		ats_ulog( kLogLevelNotice, "*** Not using compression\n" );
	}

	// Unknown audio format.

	else
	{
		dlogassert( "Unsupported encoding: '%.*s'", (int) encodingLen, encodingPtr );
		err = kUnsupportedDataErr;
		goto exit;
	}
	gAirPlayAudioCompressionType = atCnx->compressionType;

	//-----------------------------------------------------------------------------------------------------------------------
	//	Encryption
	//-----------------------------------------------------------------------------------------------------------------------

	aesKeyPtr = NULL;
#if( AIRTUNES_FAIRPLAY || AIRPLAY_MFI || AIRTUNES_RSA )
	aesKeyLen = 0;
#endif
	aesIVPtr  = NULL;
	aesIVLen  = 0;

	// MFi

#if( AIRPLAY_MFI )
	if( !aesKeyPtr )
	{
		SDPFindAttribute( mediaSectionPtr, mediaSectionEnd, "mfiaeskey", &aesKeyPtr, &aesKeyLen, NULL );
		if( aesKeyPtr )
		{
			// Decode and decrypt the AES session key with MFiSAP.

			err = Base64Decode( aesKeyPtr, aesKeyLen, atCnx->encryptionKey, sizeof( atCnx->encryptionKey ), &aesKeyLen );
			require_noerr( err, exit );
			require_action( aesKeyLen == sizeof( atCnx->encryptionKey ), exit, err = kSizeErr );

			require_action( atCnx->MFiSAP, exit, err = kAuthenticationErr );
			err = APSMFiSAP_Decrypt( atCnx->MFiSAP, atCnx->encryptionKey, aesKeyLen, atCnx->encryptionKey );
			require_noerr( err, exit );
		}
	}
#endif

	// RSA

	// Decode the AES initialization vector (IV).

	SDPFindAttribute( mediaSectionPtr, mediaSectionEnd, "aesiv", &aesIVPtr, &aesIVLen, NULL );
	if( aesKeyPtr && aesIVPtr )
	{
		err = Base64Decode( aesIVPtr, aesIVLen, atCnx->encryptionIV, sizeof( atCnx->encryptionIV ), &decodedLen );
		require_noerr( err, exit );
		require_action( decodedLen == sizeof( atCnx->encryptionIV ), exit, err = kSizeErr );

		atCnx->usingEncryption = true;
	}
	else if( aesKeyPtr || aesIVPtr )
	{
		ats_ulog( kLogLevelNotice, "### Key/IV missing: key %p/%p\n", aesKeyPtr, aesIVPtr );
		err = kMalformedErr;
		goto exit;
	}
	else
	{
		atCnx->usingEncryption = false;
	}

	// Minimum Latency

	atCnx->minLatency = kAirTunesPlayoutDelay; // Default value for old clients.
	ptr = NULL;
	len = 0;
	SDPFindAttribute( mediaSectionPtr, mediaSectionEnd, "min-latency", &ptr, &len, NULL );
	if( len > 0 )
	{
		n = SNScanF( ptr, len, "%u", &atCnx->minLatency );
		require_action( n == 1, exit, err = kMalformedErr );
	}

	// Maximum Latency

	atCnx->maxLatency = kAirPlayAudioLatencyOther; // Default value for old clients.
	ptr = NULL;
	len = 0;
	SDPFindAttribute( mediaSectionPtr, mediaSectionEnd, "max-latency", &ptr, &len, NULL );
	if( len > 0 )
	{
		n = SNScanF( ptr, len, "%u", &atCnx->maxLatency );
		require_action( n == 1, exit, err = kMalformedErr );
	}

exit:
	return( err );
}
#endif // AIRPLAY_SDP_SETUP

#if( AIRPLAY_MFI )
//===========================================================================================================================
//	_requestProcessAuthSetup
//===========================================================================================================================

static HTTPStatus _requestProcessAuthSetup( HTTPConnectionRef inCnx, HTTPMessageRef request )
{
	HTTPStatus				status;
	OSStatus				err;
	uint8_t *				outputPtr;
	size_t					outputLen;
	AirTunesConnection *	atCnx		= (AirTunesConnection *) inCnx->userContext;
	HTTPMessageRef			response	= inCnx->responseMsg;

	ats_ulog( kAirPlayPhaseLogLevel, "MFi\n" );
	outputPtr = NULL;
	require_action( request->bodyOffset > 0, exit, status = kHTTPStatus_BadRequest );

	// Let MFi-SAP process the input data and generate output data.

	if( atCnx->MFiSAPDone && atCnx->MFiSAP )
	{
		APSMFiSAP_Delete( atCnx->MFiSAP );
		atCnx->MFiSAP = NULL;
		atCnx->MFiSAPDone = false;
	}
	if( !atCnx->MFiSAP )
	{
		err = APSMFiSAP_Create( &atCnx->MFiSAP, kAPSMFiSAPVersion1 );
		require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	}

	err = APSMFiSAP_Exchange( atCnx->MFiSAP, request->bodyPtr, request->bodyOffset, &outputPtr, &outputLen, &atCnx->MFiSAPDone );
	require_noerr_action( err, exit, status = kHTTPStatus_Forbidden );

	// Send the MFi-SAP output data in the response.

	err = HTTPMessageSetBody( response, kMIMEType_Binary, outputPtr, outputLen );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );

	status = kHTTPStatus_OK;

exit:
	if( outputPtr ) free( outputPtr );
	return( status );
}
#endif // AIRPLAY_MFI

//===========================================================================================================================
//	_requestProcessCommand
//===========================================================================================================================

static HTTPStatus _requestProcessCommand( HTTPConnectionRef inCnx, HTTPMessageRef request )
{
	HTTPStatus				status;
	OSStatus				err;
	CFDictionaryRef			requestDict;
	CFStringRef				command = NULL;
	CFDictionaryRef			params;
	CFDictionaryRef			responseDict;
	AirTunesConnection *	atCnx = (AirTunesConnection *) inCnx->userContext;

	requestDict = CFDictionaryCreateWithBytes( request->bodyPtr, request->bodyOffset, &err );
	require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );

	command = CFDictionaryGetCFString( requestDict, CFSTR( kAirPlayKey_Type ), NULL );
	require_action( command, exit, err = kParamErr; status = kHTTPStatus_ParameterNotUnderstood );

	params = CFDictionaryGetCFDictionary( requestDict, CFSTR( kAirPlayKey_Params ), NULL );

	// Perform the command and send its response.

	require_action( atCnx->session, exit, err = kNotPreparedErr; status = kHTTPStatus_SessionNotFound );
	responseDict = NULL;
	err = AirPlayReceiverSessionControl( atCnx->session, kCFObjectFlagDirect, command, NULL, params, &responseDict );
	require_noerr_action_quiet( err, exit, status = kHTTPStatus_UnprocessableEntity );
	if( !responseDict )
	{
		responseDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		require_action( responseDict, exit, err = kNoMemoryErr; status = kHTTPStatus_InternalServerError );
	}

	status = _requestSendPlistResponse( inCnx, request, responseDict, &err );
	CFRelease( responseDict );

exit:
	if( err ) ats_ulog( kLogLevelNotice, "### Command '%@' failed: %#m, %#m\n", command, status, err );
	CFReleaseNullSafe( requestDict );
	return( status );
}

//===========================================================================================================================
//	_requestProcessFeedback
//===========================================================================================================================

static HTTPStatus _requestProcessFeedback( HTTPConnectionRef inCnx, HTTPMessageRef request )
{
	HTTPStatus					status;
	OSStatus					err;
	CFDictionaryRef				input	= NULL;
	CFMutableDictionaryRef		output	= NULL;
	CFDictionaryRef				dict;
	AirTunesConnection *		atCnx	= (AirTunesConnection *) inCnx->userContext;

	input = CFDictionaryCreateWithBytes( request->bodyPtr, request->bodyOffset, &err );
	require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );

	output = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( output, exit, err = kNoMemoryErr; status = kHTTPStatus_InternalServerError );

	if( atCnx->session )
	{
		dict = NULL;
		AirPlayReceiverSessionControl( atCnx->session, kCFObjectFlagDirect, CFSTR( kAirPlayCommand_UpdateFeedback ), NULL,
			input, &dict );
		if( dict )
		{
			CFDictionaryMergeDictionary( output, dict );
			CFRelease( dict );
		}
	}

	status = _requestSendPlistResponse( inCnx, request, ( CFDictionaryGetCount( output ) > 0 ) ? output : NULL, &err );

exit:
	if( err ) ats_ulog( kLogLevelNotice, "### Feedback failed: %#m, %#m\n", status, err );
	CFReleaseNullSafe( input );
	CFReleaseNullSafe( output );
	return( status );
}

extern void update_flush_tick(uint64_t t);
//===========================================================================================================================
//	_requestProcessFlush
//===========================================================================================================================

static HTTPStatus _requestProcessFlush( HTTPConnectionRef inCnx, HTTPMessageRef request )
{
	OSStatus				err;
	HTTPStatus				status;
	uint16_t				flushSeq;
	uint32_t				flushTS;
	uint32_t				lastPlayedTS;
	AirTunesConnection *	atCnx		= (AirTunesConnection *) inCnx->userContext;
	HTTPMessageRef			response	= inCnx->responseMsg;

	ats_ulog( kLogLevelInfo, "Flush22\n" );

	if( !atCnx->didRecord )
	{
		dlogassert( "FLUSH not allowed when not playing" );
		status = kHTTPStatus_MethodNotValidInThisState;
		goto exit;
	}

	// Flush everything before the specified seq/ts.

	err = HTTPParseRTPInfo( request->header.buf, request->header.len, &flushSeq, &flushTS );

	require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );
	err = AirPlayReceiverSessionFlushAudio( atCnx->session, flushTS, flushSeq, &lastPlayedTS );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	//printf("%s len[%d] buf[%s] flushTS=%u lastPlayedTS=%u %d\n", __FUNCTION__, request->header.len,  request->header.buf, flushTS, lastPlayedTS,
	//	 (flushTS-lastPlayedTS));
	// for pause check
	//update_flush_tick(UpTicks());

	err = HTTPHeader_SetField( &response->header, kHTTPHeader_RTPInfo, "rtptime=%u", lastPlayedTS );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );

	status = kHTTPStatus_OK;

exit:
	return( status );
}

//===========================================================================================================================
//	_requestProcessGetLog
//===========================================================================================================================

static HTTPStatus _requestProcessGetLog( HTTPConnectionRef inCnx )
{
	HTTPStatus		status;
	OSStatus		err;
	DataBuffer		dataBuf;
	uint8_t *		ptr;
	size_t			len;
	HTTPMessageRef	response = inCnx->responseMsg;

	DataBuffer_Init( &dataBuf, NULL, 0, 10 * kBytesPerMegaByte );

	{
		ats_ulog( kLogLevelNotice, "### Rejecting log request from non-internal build\n" );
		status = kHTTPStatus_NotFound;
		goto exit;
	}

	DataBuffer_AppendF( &dataBuf, "AirPlay Diagnostics\n" );
	DataBuffer_AppendF( &dataBuf, "===================\n" );
	AirTunesDebugAppendShowData( "globals", &dataBuf );
	AirTunesDebugAppendShowData( "stats", &dataBuf );
	AirTunesDebugAppendShowData( "rtt", &dataBuf );
	AirTunesDebugAppendShowData( "retrans", &dataBuf );
	AirTunesDebugAppendShowData( "retransDone", &dataBuf );

	DataBuffer_AppendF( &dataBuf, "+-+ syslog +--\n" );
	DataBuffer_RunProcessAndAppendOutput( &dataBuf, "syslog" );
	DataBuffer_AppendFile( &dataBuf, kAirPlayPrimaryLogPath );

	err = DataBuffer_Commit( &dataBuf, &ptr, &len );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );

	err = HTTPMessageSetBody( response, "text/plain", ptr, len );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );

	status = kHTTPStatus_OK;

exit:
	DataBuffer_Free( &dataBuf );
	return( status );
}

//===========================================================================================================================
//	_requestProcessInfo
//===========================================================================================================================

static HTTPStatus _requestProcessInfo( HTTPConnectionRef inCnx, HTTPMessageRef request )
{
	HTTPStatus					status;
	OSStatus					err;
	CFMutableDictionaryRef		requestDict;
	CFMutableArrayRef			qualifier = NULL;
	CFDictionaryRef				responseDict;
	const char *				src;
	const char *				end;
	const char *				namePtr;
	size_t						nameLen;
	char *						nameBuf;
	uint32_t					userVersion;
	AirTunesConnection *		atCnx = (AirTunesConnection *) inCnx->userContext;

	requestDict = CFDictionaryCreateMutableWithBytes( request->bodyPtr, request->bodyOffset, &err );
	require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );

	qualifier = (CFMutableArrayRef) CFDictionaryGetCFArray( requestDict, CFSTR( kAirPlayKey_Qualifier ), NULL );
	if( qualifier ) CFRetain( qualifier );

	src = request->header.url.queryPtr;
	end = src + request->header.url.queryLen;
	while( ( err = URLGetOrCopyNextVariable( src, end, &namePtr, &nameLen, &nameBuf, NULL, NULL, NULL, &src ) ) == kNoErr )
	{
		err = CFArrayEnsureCreatedAndAppendCString( &qualifier, namePtr, nameLen );
		if( nameBuf ) free( nameBuf );
		require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	}

	HTTPScanFHeaderValue( request->header.buf, request->header.len, kAirPlayHTTPHeader_ProtocolVersion, "%u", &userVersion );
	if( atCnx->session ) AirPlayReceiverSessionSetUserVersion( atCnx->session, userVersion );

	responseDict = AirPlayCopyServerInfo( atCnx->session, qualifier, inCnx->ifMACAddress , &err );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );

	status = _requestSendPlistResponseInfo( inCnx, request, responseDict, &err );
	CFReleaseNullSafe( responseDict );

exit:
	CFReleaseNullSafe( qualifier );
	CFReleaseNullSafe( requestDict );
	if( err ) ats_ulog( kLogLevelNotice, "### Get info failed: %#m, %#m\n", status, err );
	return( status );
}

#if( AIRPLAY_METRICS )
//===========================================================================================================================
//	_requestProcessMetrics
//===========================================================================================================================

static HTTPStatus _requestProcessMetrics( HTTPConnectionRef inCnx, HTTPMessageRef request )
{
	HTTPStatus			status;
	OSStatus			err;
	CFDictionaryRef		input	= NULL;
	CFDictionaryRef		metrics	= NULL;
	AirTunesConnection	*atCnx = (AirTunesConnection *) inCnx->userContext;

	ats_ulog( kAirPlayPhaseLogLevel, "Metrics\n" );
	require_action_quiet( atCnx->session, exit, err = kNotInUseErr; status = kHTTPStatus_SessionNotFound );

	input = CFDictionaryCreateWithBytes( request->bodyPtr, request->bodyOffset, &err );
	require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );

	metrics = (CFDictionaryRef) AirPlayReceiverSessionCopyProperty( atCnx->session, kCFObjectFlagDirect,
		CFSTR( kAirPlayProperty_Metrics ), input, NULL );
	if( !metrics )
	{
		metrics = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
		require_action( metrics, exit, err = kNoMemoryErr; status = kHTTPStatus_InternalServerError );
	}

	status = _requestSendPlistResponse( inCnx, request, metrics, &err );

exit:
	if( err ) ats_ulog( kLogLevelNotice, "### Metrics failed: %#m, %#m\n", status, err );
	CFReleaseNullSafe( input );
	CFReleaseNullSafe( metrics );
	return( status );
}
#endif  // AIRPLAY_METRICS

//===========================================================================================================================
//	_requestProcessOptions
//===========================================================================================================================

static HTTPStatus _requestProcessOptions( HTTPConnectionRef inCnx )
{
	HTTPMessageRef response = inCnx->responseMsg;

	HTTPHeader_SetField( &response->header, kHTTPHeader_Public,
		"ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, TEARDOWN, OPTIONS, GET_PARAMETER, SET_PARAMETER, POST, GET" );

	return( kHTTPStatus_OK );
}

//===========================================================================================================================
//	_requestProcessGetParameter
//===========================================================================================================================

static HTTPStatus _requestProcessGetParameter( HTTPConnectionRef inCnx, HTTPMessageRef request )
{
	HTTPStatus				status;
	OSStatus				err;
	const char *			src;
	const char *			end;
	size_t					len;
	char					tempStr[ 256 ];
	char					responseBuf[ 256 ];
	int						n;
	HTTPMessageRef			response	= inCnx->responseMsg;
	AirTunesConnection *	atCnx		= (AirTunesConnection *) inCnx->userContext;

	if( !AirTunesConnection_DidSetup( atCnx ) )
	{
		dlogassert( "GET_PARAMETER not allowed before SETUP" );
		status = kHTTPStatus_MethodNotValidInThisState;
		goto exit;
	}

	// Check content type.

	err = GetHeaderValue( request, kHTTPHeader_ContentType, &src, &len );
	if( err )
	{
		dlogassert( "No Content-Type header" );
		status = kHTTPStatus_BadRequest;
		goto exit;
	}
	if( strnicmpx( src, len, "text/parameters" ) != 0 )
	{
		dlogassert( "Bad Content-Type: '%.*s'", (int) len, src );
		status = kHTTPStatus_HeaderFieldNotValid;
		goto exit;
	}

	// Parse parameters. Each parameter is formatted as <name>\r\n

	src = (const char *) request->bodyPtr;
	end = src + request->bodyOffset;
	while( src < end )
	{
		char				c;
		const char *		namePtr;
		size_t				nameLen;

		namePtr = src;
		while( ( src < end ) && ( ( c = *src ) != '\r' ) && ( c != '\n' ) ) ++src;
		nameLen = (size_t)( src - namePtr );
		if( ( nameLen == 0 ) || ( src >= end ) )
		{
			dlogassert( "Bad parameter: '%.*s'", (int) request->bodyOffset, request->bodyPtr );
			status = kHTTPStatus_ParameterNotUnderstood;
			goto exit;
		}

		// Process the parameter.

		if( 0 ) {}

		// Volume

		else if( strnicmpx( namePtr, nameLen, "volume" ) == 0 )
		{
			Float32		dbVolume;

			if( atCnx->session )
			{
				dbVolume = (Float32) AirPlayReceiverSessionPlatformGetDouble( atCnx->session, CFSTR( kAirPlayProperty_Volume ),
					NULL, &err );
				require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
			}
			else
			{
				dbVolume = atCnx->atServer->volume;
			}

			n = snprintf( responseBuf, sizeof( responseBuf ), "volume: %f\r\n", dbVolume );
			err = HTTPMessageSetBody( response, "text/parameters", responseBuf, (size_t) n );
			require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
		}

		// Name

		else if( strnicmpx( namePtr, nameLen, "name" ) == 0 )
		{
			err = AirPlayGetDeviceName( tempStr, sizeof( tempStr ) );
			require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );

			n = snprintf( responseBuf, sizeof( responseBuf ), "name: %s\r\n", tempStr );
			err = HTTPMessageSetBody( response, "text/parameters", responseBuf, (size_t) n );
			require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
		}

		// Other

		else
		{
			dlogassert( "Unknown parameter: '%.*s'", (int) nameLen, namePtr );
			status = kHTTPStatus_ParameterNotUnderstood;
			goto exit;
		}

		while( ( src < end ) && ( ( ( c = *src ) == '\r' ) || ( c == '\n' ) ) ) ++src;
	}

	status = kHTTPStatus_OK;

exit:
	return( status );
}

//===========================================================================================================================
//	_requestProcessSetParameter
//===========================================================================================================================

static HTTPStatus _requestProcessSetParameter( HTTPConnectionRef inCnx, HTTPMessageRef request )
{
	HTTPStatus				status;
	const char *			src;
	size_t					len;
	AirTunesConnection *	atCnx	= (AirTunesConnection *) inCnx->userContext;

	if( !AirTunesConnection_DidSetup( atCnx ) )
	{
		dlogassert( "SET_PARAMETER not allowed before SETUP" );
		status = kHTTPStatus_MethodNotValidInThisState;
		goto exit;
	}

	src = NULL;
	len = 0;
	GetHeaderValue( request, kHTTPHeader_ContentType, &src, &len );
	if( ( len == 0 ) && ( request->bodyOffset == 0 ) )			status = kHTTPStatus_OK;
	else if( strnicmp_prefix( src, len, "image/" ) == 0 )		status = _requestProcessSetParameterArtwork( inCnx, request, src, len );
	else if( strnicmpx( src, len, kMIMEType_DMAP ) == 0 )		status = _requestProcessSetParameterDMAP( inCnx, request );
	else if( strnicmpx( src, len, "text/parameters" ) == 0 )	status = _requestProcessSetParameterText( inCnx, request );
	else if( MIMETypeIsPlist( src, len ) )						status = _requestProcessSetProperty( inCnx, request );
	else { dlogassert( "Bad Content-Type: '%.*s'", (int) len, src ); status = kHTTPStatus_HeaderFieldNotValid; goto exit; }

exit:
	return( status );
}

//===========================================================================================================================
//	_requestProcessSetParameterArtwork
//===========================================================================================================================

static HTTPStatus
	_requestProcessSetParameterArtwork(
		HTTPConnectionRef	inCnx,
		HTTPMessageRef		request,
		const char *		inMIMETypeStr,
		size_t				inMIMETypeLen )
{
	HTTPStatus					status;
	OSStatus					err;
	uint32_t					u32;
	CFNumberRef					applyTS  = NULL;
	CFMutableDictionaryRef		metaData = NULL;
	AirTunesConnection *		atCnx	 = (AirTunesConnection *) inCnx->userContext;

	err = HTTPParseRTPInfo( request->header.buf, request->header.len, NULL, &u32 );
	require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );

	applyTS = CFNumberCreateInt64( u32 );
	require_action( applyTS, exit, status = kHTTPStatus_InternalServerError );

	metaData = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( metaData, exit, status = kHTTPStatus_InternalServerError );

	CFDictionarySetCString( metaData, kAirPlayMetaDataKey_ArtworkMIMEType, inMIMETypeStr, inMIMETypeLen );
	if( request->bodyOffset > 0 )
	{
		CFDictionarySetData( metaData, kAirPlayMetaDataKey_ArtworkData, request->bodyPtr, request->bodyOffset );
	}
	else
	{
		CFDictionarySetValue( metaData, kAirPlayMetaDataKey_ArtworkData, kCFNull );
	}

	err = AirPlayReceiverSessionSetProperty( atCnx->session, kCFObjectFlagDirect, CFSTR( kAirPlayProperty_MetaData ),
		applyTS, metaData );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	status = kHTTPStatus_OK;

exit:
	CFReleaseNullSafe( applyTS );
	CFReleaseNullSafe( metaData );
	return( status );
}

//===========================================================================================================================
//	_requestProcessSetParameterDMAP
//===========================================================================================================================

static HTTPStatus _requestProcessSetParameterDMAP( HTTPConnectionRef inCnx, HTTPMessageRef request )
{
	HTTPStatus					status;
	OSStatus					err;
	CFNumberRef					applyTS  = NULL;
	CFMutableDictionaryRef		metaData = NULL;
	DMAPContentCode				code;
	const uint8_t *				src;
	const uint8_t *				end;
	const uint8_t *				ptr;
	size_t						len;
	int64_t						persistentID = 0, itemID = 0, songID = 0;
	Boolean						hasPersistentID = false, hasItemID = false, hasSongID = false;
	int64_t						s64;
	uint32_t					u32;
	uint8_t						u8;
	double						d;
	CFTypeRef					tempObj;
	AirTunesConnection *		atCnx = (AirTunesConnection *) inCnx->userContext;

	err = HTTPParseRTPInfo( request->header.buf, request->header.len, NULL, &u32 );
	require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );

	applyTS = CFNumberCreateInt64( u32 );
	require_action( applyTS, exit, status = kHTTPStatus_InternalServerError );

	metaData = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( metaData, exit, status = kHTTPStatus_InternalServerError );

	end = request->bodyPtr + request->bodyOffset;
	err = APSDMAPContentBlock_GetNextChunk( request->bodyPtr, end, &code, &len, &src, NULL );
	require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );
	require_action( code == kDMAPListingItemCode, exit, status = kHTTPStatus_BadRequest );

	end = src + len;
	while( APSDMAPContentBlock_GetNextChunk( src, end, &code, &len, &ptr, &src ) == kNoErr )
	{
		switch( code )
		{
			case kDAAPSongAlbumCode:
				CFDictionarySetCString( metaData, kAirPlayMetaDataKey_Album, ptr, len );
				break;

			case kDAAPSongArtistCode:
				CFDictionarySetCString( metaData, kAirPlayMetaDataKey_Artist, ptr, len );
				break;

			case kDAAPSongComposerCode:
				CFDictionarySetCString( metaData, kAirPlayMetaDataKey_Composer, ptr, len );
				break;

			case kDAAPSongGenreCode:
				CFDictionarySetCString( metaData, kAirPlayMetaDataKey_Genre, ptr, len );
				break;

			case kDMAPItemNameCode:
				CFDictionarySetCString( metaData, kAirPlayMetaDataKey_Title, ptr, len );
				break;

			case kDAAPSongDataKindCode:
				CFDictionarySetBoolean( metaData, kAirPlayMetaDataKey_IsStream,
					( ( len == 1 ) && ( ptr[ 0 ] == kDAAPSongDataKind_RemoteStream ) ) );
				break;

			case kDAAPSongTrackNumberCode:
				if( len != 2 ) { dlogassert( "### Bad track number length: %zu\n", len ); break; }
				s64 = ReadBig16( ptr );
				CFDictionarySetInt64( metaData, kAirPlayMetaDataKey_TrackNumber, s64 );
				break;

			case kDAAPSongTrackCountCode:
				if( len != 2 ) { dlogassert( "### Bad track count length: %zu\n", len ); break; }
				s64 = ReadBig16( ptr );
				CFDictionarySetInt64( metaData, kAirPlayMetaDataKey_TotalTracks, s64 );
				break;

			case kDAAPSongDiscNumberCode:
				if( len != 2 ) { dlogassert( "### Bad disc number length: %zu\n", len ); break; }
				s64 = ReadBig16( ptr );
				CFDictionarySetInt64( metaData, kAirPlayMetaDataKey_DiscNumber, s64 );
				break;

			case kDAAPSongDiscCountCode:
				if( len != 2 ) { dlogassert( "### Bad disc count length: %zu\n", len ); break; }
				s64 = ReadBig16( ptr );
				CFDictionarySetInt64( metaData, kAirPlayMetaDataKey_TotalDiscs, s64 );
				break;

			case kDMAPPersistentIDCode:
				if( len != 8 ) { dlogassert( "### Bad persistent ID length: %zu\n", len ); break; }
				persistentID = ReadBig64( ptr );
				hasPersistentID = true;
				break;

			case kDAAPSongTimeCode:
				if( len != 4 ) { dlogassert( "### Bad song time length: %zu\n", len ); break; }
				s64 = ReadBig32( ptr );
				CFDictionarySetDouble( metaData, kAirPlayMetaDataKey_Duration, s64 / 1000.0 /* ms -> secs */ );
				break;

			case kDMAPItemIDCode:
				if( len != 4 ) { dlogassert( "### Bad item ID length: %zu\n", len ); break; }
				itemID = ReadBig32( ptr );
				hasItemID = true;
				break;

			case kExtDAAPITMSSongIDCode:
				if( len != 4 ) { dlogassert( "### Bad song ID length: %zu\n", len ); break; }
				songID = ReadBig32( ptr );
				hasSongID = true;
				break;

			case kDACPPlayerStateCode:
				if( len != 1 ) { dlogassert( "### Bad player state length: %zu\n", len ); break; }
				u8 = *ptr;
				if(      u8 == kDACPPlayerState_Paused )	d = 0.0;
				else if( u8 == kDACPPlayerState_Stopped )	d = 0.0;
				else if( u8 == kDACPPlayerState_FastFwd )	d = 2.0;
				else if( u8 == kDACPPlayerState_Rewind )	d = -1.0;
				else										d = 1.0;
				CFDictionarySetDouble( metaData, kAirPlayMetaDataKey_Rate, d );
				break;

			case 0x63654A56: // 'ceJV'
			case 0x63654A43: // 'ceJC'
			case 0x63654A53: // 'ceJS'
				// These aren't needed, but are sent by some clients so ignore to avoid an assert about it.
				break;

			default:
				#if( DEBUG )
				{
					const DMAPContentCodeEntry *		e;

					e = APSDMAPFindEntryByContentCode( code );
					if( e ) dlog( kLogLevelChatty,  "Unhandled DMAP: '%C'  %-36s  %s\n", code, e->name, e->codeSymbol );
				}
				#endif
				break;
		}
	}
	if(      hasPersistentID )	CFDictionarySetInt64( metaData, kAirPlayMetaDataKey_UniqueID, persistentID );
	else if( hasItemID )		CFDictionarySetInt64( metaData, kAirPlayMetaDataKey_UniqueID, itemID );
	else if( hasSongID )		CFDictionarySetInt64( metaData, kAirPlayMetaDataKey_UniqueID, songID );
	else
	{
		// Emulate a unique ID from other info that's like to change with each new song.

		s64 = 0;

		tempObj = CFDictionaryGetValue( metaData, kAirPlayMetaDataKey_Album );
		if( tempObj ) s64 ^= CFHash( tempObj );

		tempObj = CFDictionaryGetValue( metaData, kAirPlayMetaDataKey_Artist );
		if( tempObj ) s64 ^= CFHash( tempObj );

		tempObj = CFDictionaryGetValue( metaData, kAirPlayMetaDataKey_Title );
		if( tempObj ) s64 ^= CFHash( tempObj );

		tempObj = CFDictionaryGetValue( metaData, kAirPlayMetaDataKey_TrackNumber );
		if( tempObj ) s64 ^= CFHash( tempObj );

		CFDictionarySetInt64( metaData, kAirPlayMetaDataKey_UniqueID, s64 );
	}

	err = AirPlayReceiverSessionSetProperty( atCnx->session, kCFObjectFlagDirect, CFSTR( kAirPlayProperty_MetaData ),
		applyTS, metaData );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	status = kHTTPStatus_OK;

exit:
	CFReleaseNullSafe( applyTS );
	CFReleaseNullSafe( metaData );
	return( status );
}

//===========================================================================================================================
//	_requestProcessSetParameterText
//===========================================================================================================================

static HTTPStatus _requestProcessSetParameterText( HTTPConnectionRef inCnx, HTTPMessageRef request )
{
	HTTPStatus			status;
	OSStatus			err;
	const char *		src;
	const char *		end;
	AirTunesConnection*	atCnx = (AirTunesConnection *) inCnx->userContext;

	// Parse parameters. Each parameter is formatted as <name>: <value>\r\n

	src = (const char *) request->bodyPtr;
	end = src + request->bodyOffset;
	while( src < end )
	{
		char				c;
		const char *		namePtr;
		size_t				nameLen;
		const char *		valuePtr;
		size_t				valueLen;

		// Parse the name.

		namePtr = src;
		while( ( src < end ) && ( *src != ':' ) ) ++src;
		nameLen = (size_t)( src - namePtr );
		if( ( nameLen == 0 ) || ( src >= end ) )
		{
			dlogassert( "Bad parameter: '%.*s'", (int) request->bodyOffset, request->bodyPtr );
			status = kHTTPStatus_ParameterNotUnderstood;
			goto exit;
		}
		++src;
		while( ( src < end ) && ( ( ( c = *src ) == ' ' ) || ( c == '\t' ) ) ) ++src;

		// Parse the value.

		valuePtr = src;
		while( ( src < end ) && ( ( c = *src ) != '\r' ) && ( c != '\n' ) ) ++src;
		valueLen = (size_t)( src - valuePtr );

		// Process the parameter.

		if( 0 ) {}

		// Volume

		else if( strnicmpx( namePtr, nameLen, "volume" ) == 0 )
		{
			char		tempStr[ 256 ];

			require_action( valueLen < sizeof( tempStr ), exit, status = kHTTPStatus_HeaderFieldNotValid );
			memcpy( tempStr, valuePtr, valueLen );
			tempStr[ valueLen ] = '\0';

			atCnx->atServer->volume = (Float32) strtod( tempStr, NULL );
			err = AirPlayReceiverSessionPlatformSetDouble( atCnx->session, CFSTR( kAirPlayProperty_Volume ),
				NULL, atCnx->atServer->volume );
			require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
		}

		// Progress

		else if( strnicmpx( namePtr, nameLen, "progress" ) == 0 )
		{
			unsigned int				startTS, currentTS, stopTS;
			int							n;
			CFMutableDictionaryRef		progressDict;

			n = SNScanF( valuePtr, valueLen, "%u/%u/%u", &startTS, &currentTS, &stopTS );
			require_action( n == 3, exit, status = kHTTPStatus_HeaderFieldNotValid );

			progressDict = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
			require_action( progressDict, exit, status = kHTTPStatus_InternalServerError );
			CFDictionarySetInt64( progressDict, CFSTR( kAirPlayKey_StartTimestamp ), startTS );
			CFDictionarySetInt64( progressDict, CFSTR( kAirPlayKey_CurrentTimestamp ), currentTS );
			CFDictionarySetInt64( progressDict, CFSTR( kAirPlayKey_EndTimestamp ), stopTS );

			err = AirPlayReceiverSessionSetProperty( atCnx->session, kCFObjectFlagDirect, CFSTR( kAirPlayProperty_Progress ),
				NULL, progressDict );
			CFRelease( progressDict );
			require_noerr_action_quiet( err, exit, status = kHTTPStatus_UnprocessableEntity );
		}

		// Other

		else
		{
			(void) err;
			(void) valueLen;

			dlogassert( "Unknown parameter: '%.*s'", (int) nameLen, namePtr );
			status = kHTTPStatus_ParameterNotUnderstood;
			goto exit;
		}

		while( ( src < end ) && ( ( ( c = *src ) == '\r' ) || ( c == '\n' ) ) ) ++src;
	}

	status = kHTTPStatus_OK;

exit:
	return( status );
}

//===========================================================================================================================
//	_requestProcessSetProperty
//===========================================================================================================================

static HTTPStatus _requestProcessSetProperty( HTTPConnectionRef inCnx, HTTPMessageRef request )
{
	HTTPStatus				status;
	OSStatus				err;
	CFDictionaryRef			requestDict;
	CFStringRef				property = NULL;
	CFTypeRef				qualifier;
	CFTypeRef				value;
	AirTunesConnection *	atCnx = (AirTunesConnection *) inCnx->userContext;

	requestDict = CFDictionaryCreateWithBytes( request->bodyPtr, request->bodyOffset, &err );
	require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );

	property = CFDictionaryGetCFString( requestDict, CFSTR( kAirPlayKey_Property ), NULL );
	require_action( property, exit, err = kParamErr; status = kHTTPStatus_BadRequest );

	qualifier	= CFDictionaryGetValue( requestDict, CFSTR( kAirPlayKey_Qualifier ) );
	value		= CFDictionaryGetValue( requestDict, CFSTR( kAirPlayKey_Value ) );

	// Set the property on the session.

	require_action( atCnx->session, exit, err = kNotPreparedErr; status = kHTTPStatus_SessionNotFound );
	err = AirPlayReceiverSessionSetProperty( atCnx->session, kCFObjectFlagDirect, property, qualifier, value );
	require_noerr_action_quiet( err, exit, status = kHTTPStatus_UnprocessableEntity );

	status = kHTTPStatus_OK;

exit:
	if( err ) ats_ulog( kLogLevelNotice, "### Set property '%@' failed: %#m, %#m\n", property, status, err );
	CFReleaseNullSafe( requestDict );
	return( status );
}

//===========================================================================================================================
//	_requestProcessRecord
//===========================================================================================================================

static HTTPStatus _requestProcessRecord( HTTPConnectionRef inCnx, HTTPMessageRef request )
{
	HTTPStatus							status;
	OSStatus							err;
	const char *						src;
	const char *						end;
	size_t								len;
	const char *						namePtr;
	size_t								nameLen;
	const char *						valuePtr;
	size_t								valueLen;
	AirPlayReceiverSessionStartInfo		startInfo;
	AirTunesConnection *				atCnx = (AirTunesConnection *) inCnx->userContext;
	HTTPMessageRef						response = inCnx->responseMsg;

	ats_ulog( kAirPlayPhaseLogLevel, "Record\n" );

	if( !AirTunesConnection_DidSetup( atCnx ) )
	{
		dlogassert( "RECORD not allowed before SETUP" );
		status = kHTTPStatus_MethodNotValidInThisState;
		goto exit;
	}

	memset( &startInfo, 0, sizeof( startInfo ) );
	startInfo.clientName	= atCnx->clientName;
	startInfo.transportType	= inCnx->transportType;

	// Parse session duration info.

	src = NULL;
	len = 0;
	GetHeaderValue( request, kAirPlayHTTPHeader_Durations, &src, &len );
	end = src + len;
	while( HTTPParseParameter( src, end, &namePtr, &nameLen, &valuePtr, &valueLen, NULL, &src ) == kNoErr )
	{
		if(      strnicmpx( namePtr, nameLen, "b" )  == 0 ) startInfo.bonjourMs		= TextToInt32( valuePtr, valueLen, 10 );
		else if( strnicmpx( namePtr, nameLen, "c" )  == 0 ) startInfo.connectMs		= TextToInt32( valuePtr, valueLen, 10 );
		else if( strnicmpx( namePtr, nameLen, "au" ) == 0 ) startInfo.authMs		= TextToInt32( valuePtr, valueLen, 10 );
		else if( strnicmpx( namePtr, nameLen, "an" ) == 0 ) startInfo.announceMs	= TextToInt32( valuePtr, valueLen, 10 );
		else if( strnicmpx( namePtr, nameLen, "sa" ) == 0 ) startInfo.setupAudioMs	= TextToInt32( valuePtr, valueLen, 10 );
	}

	// Start the session.

	err = AirPlayReceiverSessionPlatformSetDouble( atCnx->session, CFSTR( kAirPlayProperty_Volume ), NULL, atCnx->atServer->volume );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );

	err = AirPlayReceiverSessionStart( atCnx->session, &startInfo );
	if( AirPlayIsBusyError( err ) ) { status = kHTTPStatus_NotEnoughBandwidth; goto exit; }
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );

	err = HTTPHeader_SetField( &response->header, "Audio-Latency", "%u", atCnx->session->platformAudioLatency );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );

	atCnx->didRecord = true;
	status = kHTTPStatus_OK;

exit:
	return( status );
}

//===========================================================================================================================
//	_requestProcessSetup
//===========================================================================================================================

static HTTPStatus _requestProcessSetup( HTTPConnectionRef inCnx, HTTPMessageRef request )
{
#if( AIRPLAY_SDP_SETUP )
	OSStatus				err;
	AirTunesConnection *	atCnx	= (AirTunesConnection *) inCnx->userContext;
#endif
	HTTPStatus				status;
	const char *			ptr;
	size_t					len;

	// Reject all requests if playback is not allowed.

	if( !_AirTunesServer_PlaybackAllowed() )
	{
		ats_ulog( kLogLevelNotice, "### Setup denied because playback is not allowed\n" );
		status = kHTTPStatus_NotEnoughBandwidth; // Make us look busy to the sender.
		goto exit;
	}

	ptr = NULL;
	len = 0;
	GetHeaderValue( request, kHTTPHeader_ContentType, &ptr, &len );
	if( MIMETypeIsPlist( ptr, len ) )
	{
		status = _requestProcessSetupPlist( inCnx, request );
		goto exit;
	}

#if( AIRPLAY_SDP_SETUP )
	if( !atCnx->didAnnounce )
	{
		dlogassert( "SETUP not allowed before ANNOUNCE" );
		status = kHTTPStatus_MethodNotValidInThisState;
		goto exit;
	}

	err = URLGetNextPathSegment( &request->header.url, &ptr, &len );
	require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );

	ptr = "audio"; // Default to audio for older audio-only clients that don't append /audio.
	len = 5;
	URLGetNextPathSegment( &request->header.url, &ptr, &len );
	if(      strnicmpx( ptr, len, "audio" ) == 0 ) status = _requestProcessSetupSDPAudio( inCnx, request );
	else
#endif
	{
		dlogassert( "Bad setup URL '%.*s'", (int) request->header.urlLen, request->header.urlPtr );
		status = kHTTPStatus_BadRequest;
		goto exit;
	}

exit:
	return( status );
}

#if( AIRPLAY_SDP_SETUP )
//===========================================================================================================================
//	_requestProcessSetupSDPAudio
//===========================================================================================================================

static HTTPStatus _requestProcessSetupSDPAudio( HTTPConnectionRef inCnx, HTTPMessageRef request )
{
	HTTPStatus					status;
	OSStatus					err;
	CFMutableDictionaryRef		requestParams		= NULL;
	CFMutableArrayRef			requestStreams		= NULL;
	CFMutableDictionaryRef		requestStreamDesc	= NULL;
	CFDictionaryRef				responseParams		= NULL;
	CFArrayRef					outputStreams;
	CFDictionaryRef				outputAudioStreamDesc;
	CFIndex						i, n;
	const char *				src;
	const char *				end;
	char *						dst;
	char *						lim;
	size_t						len;
	const char *				namePtr;
	size_t						nameLen;
	const char *				valuePtr;
	size_t						valueLen;
	char						tempStr[ 128 ];
	int							tempInt;
	int							dataPort, controlPort, eventPort, timingPort;
	Boolean						useEvents = false;
	AirTunesConnection *		atCnx				= (AirTunesConnection *) inCnx->userContext;
	HTTPMessageRef				response			= inCnx->responseMsg;

	ats_ulog( kAirPlayPhaseLogLevel, "Setup audio\n" );

	if( !atCnx->didAnnounce )
	{
		dlogassert( "SETUP not allowed before ANNOUNCE" );
		status = kHTTPStatus_MethodNotValidInThisState;
		goto exit;
	}
	if( atCnx->didAudioSetup )
	{
		dlogassert( "SETUP audio not allowed twice" );
		status = kHTTPStatus_MethodNotValidInThisState;
		goto exit;
	}

	requestParams = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( requestParams, exit, status = kHTTPStatus_InternalServerError );

	requestStreams = CFArrayCreateMutable( NULL, 0, &kCFTypeArrayCallBacks );
	require_action( requestStreams, exit, status = kHTTPStatus_InternalServerError );
	CFDictionarySetValue( requestParams, CFSTR( kAirPlayKey_Streams ), requestStreams );

	requestStreamDesc = CFDictionaryCreateMutable( NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );
	require_action( requestStreamDesc, exit, status = kHTTPStatus_InternalServerError );
	CFArrayAppendValue( requestStreams, requestStreamDesc );

	// Parse the transport type. The full transport line looks similar to this:
	//
	//		Transport: RTP/AVP/UDP;unicast;interleaved=0-1;mode=record;control_port=6001;timing_port=6002

	err = GetHeaderValue( request, kHTTPHeader_Transport, &src, &len );
	require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );
	end = src + len;

	err = HTTPParseParameter( src, end, &namePtr, &nameLen, NULL, NULL, NULL, &src );
	require_noerr_action( err, exit, status = kHTTPStatus_HeaderFieldNotValid );

	if( strnicmpx( namePtr, nameLen, "RTP/AVP/UDP" ) == 0 ) {}
	else { dlogassert( "Bad transport: '%.*s'", (int) nameLen, namePtr ); status = kHTTPStatus_HeaderFieldNotValid; goto exit; }

	CFDictionarySetInt64( requestStreamDesc, CFSTR( kAirPlayKey_Type ), kAirPlayStreamType_GeneralAudio );

	// Parse transport parameters.

	while( HTTPParseParameter( src, end, &namePtr, &nameLen, &valuePtr, &valueLen, NULL, &src ) == kNoErr )
	{
		if( strnicmpx( namePtr, nameLen, "control_port" ) == 0 )
		{
			n = SNScanF( valuePtr, valueLen, "%d", &tempInt );
			require_action( n == 1, exit, status = kHTTPStatus_HeaderFieldNotValid );
			CFDictionarySetInt64( requestStreamDesc, CFSTR( kAirPlayKey_Port_Control ), tempInt );
		}
		else if( strnicmpx( namePtr, nameLen, "timing_port" ) == 0 )
		{
			n = SNScanF( valuePtr, valueLen, "%d", &tempInt );
			require_action( n == 1, exit, status = kHTTPStatus_HeaderFieldNotValid );
			CFDictionarySetInt64( requestParams, CFSTR( kAirPlayKey_Port_Timing ), tempInt );
		}
		else if( strnicmpx( namePtr, nameLen, "redundant" ) == 0 )
		{
			n = SNScanF( valuePtr, valueLen, "%d", &tempInt );
			require_action( n == 1, exit, status = kHTTPStatus_HeaderFieldNotValid );
			CFDictionarySetInt64( requestStreamDesc, CFSTR( kAirPlayKey_RedundantAudio ), tempInt );
		}
		else if( strnicmpx( namePtr, nameLen, "mode" ) == 0 )
		{
			if(      strnicmpx( valuePtr, valueLen, "record" )	== 0 ) {}
			else if( strnicmpx( valuePtr, valueLen, "screen" )	== 0 )
			{
				CFDictionarySetBoolean( requestParams, CFSTR( kAirPlayKey_UsingScreen ), true );
			}
			else dlog( kLogLevelWarning, "### Unsupported 'mode' value: %.*s%s%.*s\n",
				(int) nameLen, namePtr, valuePtr ? "=" : "", (int) valueLen, valuePtr );
		}
		else if( strnicmpx( namePtr, nameLen, "x-events" )		== 0 ) useEvents = true;
		else if( strnicmpx( namePtr, nameLen, "events") 		== 0 ) {} // Ignore deprecated event scheme.
		else if( strnicmpx( namePtr, nameLen, "unicast" )		== 0 ) {} // Ignore
		else if( strnicmpx( namePtr, nameLen, "interleaved" )	== 0 ) {} // Ignore
		else dlog( kLogLevelWarning, "### Unsupported transport param: %.*s%s%.*s\n",
			(int) nameLen, namePtr, valuePtr ? "=" : "", (int) valueLen, valuePtr );
	}

	// Set up compression.

	if( atCnx->compressionType == kAirPlayCompressionType_PCM ) {}
	else if( atCnx->compressionType == kAirPlayCompressionType_ALAC ) {}
	else { dlogassert( "Bad compression: %d", atCnx->compressionType ); status = kHTTPStatus_HeaderFieldNotValid; goto exit; }

	CFDictionarySetInt64( requestStreamDesc, CFSTR( kAirPlayKey_CompressionType ), atCnx->compressionType );
	CFDictionarySetInt64( requestStreamDesc, CFSTR( kAirPlayKey_SamplesPerFrame ), atCnx->samplesPerFrame );
	CFDictionarySetInt64( requestStreamDesc, CFSTR( kAirPlayKey_LatencyMin ), atCnx->minLatency );
	CFDictionarySetInt64( requestStreamDesc, CFSTR( kAirPlayKey_LatencyMax ), atCnx->maxLatency );

	// Set up the session.

	if( !atCnx->session )
	{
		err = _requestCreateSession( inCnx, useEvents );
		require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	}
	if( atCnx->usingEncryption )
	{
		err = AirPlayReceiverSessionSetSecurityInfo( atCnx->session, atCnx->encryptionKey, atCnx->encryptionIV );
		require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	}
	err = AirPlayReceiverSessionSetup( atCnx->session, requestParams, &responseParams );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );

	// Convert the output params to an RTSP transport header.

	outputStreams = CFDictionaryGetCFArray( responseParams, CFSTR( kAirPlayKey_Streams ), &err );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );

	outputAudioStreamDesc = NULL;
	n = CFArrayGetCount( outputStreams );
	for( i = 0; i < n; ++i )
	{
		outputAudioStreamDesc = (CFDictionaryRef) CFArrayGetValueAtIndex( outputStreams, i );
		require_action( CFIsType( outputAudioStreamDesc, CFDictionary ), exit, status = kHTTPStatus_InternalServerError );

		tempInt = (int) CFDictionaryGetInt64( outputAudioStreamDesc, CFSTR( kAirPlayKey_Type ), NULL );
		if( tempInt == kAirPlayStreamType_GeneralAudio ) break;
		outputAudioStreamDesc = NULL;
	}
	require_action( outputAudioStreamDesc, exit, status = kHTTPStatus_InternalServerError );

	dataPort = (int) CFDictionaryGetInt64( outputAudioStreamDesc, CFSTR( kAirPlayKey_Port_Data ), NULL );
	require_action( dataPort > 0, exit, status = kHTTPStatus_InternalServerError );

	controlPort = (int) CFDictionaryGetInt64( outputAudioStreamDesc, CFSTR( kAirPlayKey_Port_Control ), NULL );
	require_action( controlPort > 0, exit, status = kHTTPStatus_InternalServerError );

	eventPort = 0;
	if( useEvents )
	{
		eventPort = (int) CFDictionaryGetInt64( responseParams, CFSTR( kAirPlayKey_Port_Event ), NULL );
		require_action( eventPort > 0, exit, status = kHTTPStatus_InternalServerError );
	}

	timingPort = (int) CFDictionaryGetInt64( responseParams, CFSTR( kAirPlayKey_Port_Timing ), NULL );
	require_action( timingPort > 0, exit, status = kHTTPStatus_InternalServerError );

	// Send the response.

	dst = tempStr;
	lim = dst + sizeof( tempStr );
	err = snprintf_add( &dst, lim, "RTP/AVP/UDP;unicast;mode=record;server_port=%d;control_port=%d;timing_port=%d",
		dataPort, controlPort, timingPort );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );

	if( eventPort > 0 )
	{
		err = snprintf_add( &dst, lim, ";event_port=%d", eventPort );
		require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	}
	err = HTTPHeader_SetField( &response->header, kHTTPHeader_Transport, "%s", tempStr );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );

	err = HTTPHeader_SetField( &response->header, kHTTPHeader_Session, "1" );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );

	*tempStr = '\0';
	AirPlayReceiverServerPlatformGetCString( gAirPlayReceiverServer, CFSTR( kAirPlayProperty_AudioJackStatus ), NULL,
		tempStr, sizeof( tempStr ), NULL );
	if( *tempStr == '\0' ) strlcpy( tempStr, kAirPlayAudioJackStatus_ConnectedUnknown, sizeof( tempStr ) );
	err = HTTPHeader_SetField( &response->header, "Audio-Jack-Status", "%s", tempStr );
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );

#if( AIRPLAY_DACP )
	AirTunesDACPClient_StartSession( gAirTunesDACPClient );
#endif
	AirPlayReceiverServerSetBoolean( gAirPlayReceiverServer, CFSTR( kAirPlayProperty_Playing ), NULL, true );

	atCnx->didAudioSetup = true;
	status = kHTTPStatus_OK;

exit:
	CFReleaseNullSafe( requestParams );
	CFReleaseNullSafe( requestStreams );
	CFReleaseNullSafe( requestStreamDesc );
	CFReleaseNullSafe( responseParams );
	return( status );
}

#endif  // AIRPLAY_SDP_SETUP

//===========================================================================================================================
//	_requestProcessSetupPlist
//===========================================================================================================================

static HTTPStatus _requestProcessSetupPlist( HTTPConnectionRef inCnx, HTTPMessageRef request )
{
	HTTPStatus					status;
	OSStatus					err;
	CFMutableDictionaryRef		requestParams  = NULL;
	CFDictionaryRef				responseParams = NULL;
	uint8_t						sessionUUID[ 16 ];
	char						cstr[ 64 ];
	size_t						len;
	uint64_t					u64;
	AirPlayEncryptionType		et;
	AirTunesConnection *		atCnx = (AirTunesConnection *) inCnx->userContext;

	ats_ulog( kAirPlayPhaseLogLevel, "Setup\n" );

	// If we're denying interrupts then reject if there's already an active session.
	// Otherwise, hijack any active session to start the new one (last-in-wins).

	if( gAirTunesDenyInterruptions )
	{
		HTTPConnectionRef activeCnx;

		activeCnx = _AirTunesServer_FindActiveConnection( atCnx->atServer );
		if( activeCnx && ( activeCnx != inCnx ) )
		{
			ats_ulog( kLogLevelNotice, "Denying interruption from %##a due to %##a\n", &inCnx->peerAddr, &activeCnx->peerAddr );
			status = kHTTPStatus_NotEnoughBandwidth;
			err = kNoErr;
			goto exit;
		}
	}
	_AirTunesServer_HijackConnections( atCnx->atServer, inCnx );

	requestParams = CFDictionaryCreateMutableWithBytes( request->bodyPtr, request->bodyOffset, &err );
	require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );

	u64 = CFDictionaryGetMACAddress( requestParams, CFSTR( kAirPlayKey_DeviceID ), NULL, &err );
	if( !err ) atCnx->clientDeviceID = u64;

	strncpy( atCnx->ifName, inCnx->ifName, sizeof( atCnx->ifName ) );

	CFDictionaryGetMACAddress( requestParams, CFSTR( kAirPlayKey_MACAddress ), atCnx->clientInterfaceMACAddress, &err );

	CFDictionaryGetCString( requestParams, CFSTR( kAirPlayKey_Name ), atCnx->clientName, sizeof( atCnx->clientName ), NULL );

	CFDictionaryGetData( requestParams, CFSTR( kAirPlayKey_SessionUUID ), sessionUUID, sizeof( sessionUUID ), &len, &err );
	if( !err )
	{
		require_action( len == sizeof( sessionUUID ), exit, err = kSizeErr; status = kHTTPStatus_BadRequest );
		atCnx->clientSessionID = ReadBig64( sessionUUID );
	}

	*cstr = '\0';
	CFDictionaryGetCString( requestParams, CFSTR( kAirPlayKey_SourceVersion ), cstr, sizeof( cstr ), NULL );
	if( *cstr != '\0' ) atCnx->clientVersion = TextToSourceVersion( cstr, kSizeCString );

	// Set up the session.

	if( !atCnx->session )
	{
		err = _requestCreateSession( inCnx, true );
		require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
		strlcpy( gAirPlayAudioStats.ifname, inCnx->ifName, sizeof( gAirPlayAudioStats.ifname ) );
	}

	et = (AirPlayEncryptionType) CFDictionaryGetInt64( requestParams, CFSTR( kAirPlayKey_EncryptionType ), &err );
	if( !err && ( et != kAirPlayEncryptionType_None ) )
	{
		uint8_t key[ 16 ], iv[ 16 ];

		err = _requestDecryptKey( inCnx, requestParams, et, key, iv );
		require_noerr_action( err, exit, status = kHTTPStatus_KeyManagementError );

		err = AirPlayReceiverSessionSetSecurityInfo( atCnx->session, key, iv );
		MemZeroSecure( key, sizeof( key ) );
		require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
	}
	CFDictionaryRemoveValue( requestParams, CFSTR( kAirPlayKey_EncryptionKey ) );
	CFDictionaryRemoveValue( requestParams, CFSTR( kAirPlayKey_EncryptionIV ) );

	err = AirPlayReceiverSessionSetup( atCnx->session, requestParams, &responseParams );
	require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );

	atCnx->didAnnounce		= true;
	atCnx->didAudioSetup	= true;
#if( AIRPLAY_DACP )
	AirTunesDACPClient_StartSession( gAirTunesDACPClient );
#endif
	AirPlayReceiverServerSetBoolean( gAirPlayReceiverServer, CFSTR( kAirPlayProperty_Playing ), NULL, true );

	status = _requestSendPlistResponse( inCnx, request, responseParams, &err );
	require_noerr( err, exit );

exit:
	CFReleaseNullSafe( requestParams );
	CFReleaseNullSafe( responseParams );
	if( err ) ats_ulog( kLogLevelNotice, "### Setup session failed: %#m\n", err );
	return( status );
}

//===========================================================================================================================
//	_requestProcessTearDown
//===========================================================================================================================

static HTTPStatus _requestProcessTearDown( HTTPConnectionRef inCnx, HTTPMessageRef request )
{
	CFDictionaryRef			requestDict;
	Boolean					done = true;
	AirTunesConnection *	atCnx = (AirTunesConnection *) inCnx->userContext;

	requestDict = CFDictionaryCreateWithBytes( request->bodyPtr, request->bodyOffset, NULL );
	ats_ulog( kLogLevelNotice, "Teardown %?@\n", log_category_enabled( ats_ucat(), kLogLevelVerbose ), requestDict );

	if( atCnx->session )
	{
		AirPlayReceiverSessionTearDown( atCnx->session, requestDict, kNoErr, &done );
	}
	if( done )
	{
		ForgetCF( &atCnx->session );
		AirPlayReceiverServerSetBoolean( gAirPlayReceiverServer, CFSTR( kAirPlayProperty_Playing ), NULL, false );
		gAirPlayAudioCompressionType = kAirPlayCompressionType_Undefined;
		atCnx->didAnnounce = false;
		atCnx->didAudioSetup = false;
		atCnx->didRecord = false;
	}
	CFReleaseNullSafe( requestDict );
	return( kHTTPStatus_OK );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_requestCreateSession
//===========================================================================================================================

static OSStatus _requestCreateSession( HTTPConnectionRef inCnx, Boolean inUseEvents )
{
	OSStatus								err;
	AirPlayReceiverSessionCreateParams		createParams;
	AirTunesConnection *					atCnx = (AirTunesConnection *) inCnx->userContext;

	require_action_quiet( !atCnx->session, exit, err = kNoErr );

	memset( &createParams, 0, sizeof( createParams ) );
	createParams.server				= gAirPlayReceiverServer;
	createParams.transportType		= inCnx->transportType;
	createParams.peerAddr			= &inCnx->peerAddr;
	createParams.clientDeviceID		= atCnx->clientDeviceID;
	createParams.clientSessionID	= atCnx->clientSessionID;
	createParams.clientVersion		= atCnx->clientVersion;
	createParams.useEvents			= inUseEvents;

	memcpy( createParams.clientIfMACAddr, atCnx->clientInterfaceMACAddress, sizeof( createParams.clientIfMACAddr ) );
	strncpy( createParams.ifName, atCnx->ifName, sizeof( createParams.ifName ) );

	err = AirPlayReceiverSessionCreate( &atCnx->session, &createParams );
	require_noerr( err, exit );

exit:
	return( err );
}

//===========================================================================================================================
//	_requestDecryptKey
//===========================================================================================================================

static OSStatus
	_requestDecryptKey(
		HTTPConnectionRef		inCnx,
		CFDictionaryRef			inRequestParams,
		AirPlayEncryptionType	inType,
		uint8_t					inKeyBuf[ 16 ],
		uint8_t					inIVBuf[ 16 ] )
{
	OSStatus				err;
	const uint8_t *			keyPtr;
	size_t					len;
	AirTunesConnection *	atCnx = (AirTunesConnection *) inCnx->userContext;

	keyPtr = CFDictionaryGetData( inRequestParams, CFSTR( kAirPlayKey_EncryptionKey ), NULL, 0, &len, &err );
	require_noerr( err, exit );

	if( 0 ) {}
#if( AIRPLAY_MFI )
	else if( inType == kAirPlayEncryptionType_MFi_SAPv1 )
	{
		require_action( atCnx->MFiSAP, exit, err = kAuthenticationErr );
		err = APSMFiSAP_Decrypt( atCnx->MFiSAP, keyPtr, len, inKeyBuf );
		require_noerr( err, exit );
	}
#endif
	else
	{
		(void) atCnx;
		ats_ulog( kLogLevelWarning, "### Bad ET: %d\n", inType );
		err = kParamErr;
		goto exit;
	}

	CFDictionaryGetData( inRequestParams, CFSTR( kAirPlayKey_EncryptionIV ), inIVBuf, 16, &len, &err );
	require_noerr( err, exit );
	require_action( len == 16, exit, err = kSizeErr );

exit:
	return( err );
}

#if( AIRPLAY_PASSWORDS )
//===========================================================================================================================
//	_requestHTTPAuthorization_CopyPassword
//===========================================================================================================================

static HTTPStatus _requestHTTPAuthorization_CopyPassword( HTTPAuthorizationInfoRef inInfo, char **outPassword )
{
	HTTPStatus		status;
	char *			str;

	(void) inInfo; // Unused

	str = strdup( gAirPlayReceiverServer->playPassword );
	require_action( str, exit, status = kHTTPStatus_InternalServerError );
	*outPassword = str;
	status = kHTTPStatus_OK;

exit:
	return( status );
}

//===========================================================================================================================
//	_requestHTTPAuthorization_IsValidNonce
//===========================================================================================================================

static Boolean _requestHTTPAuthorization_IsValidNonce( HTTPAuthorizationInfoRef inInfo )
{
	HTTPConnectionRef		cnx = (HTTPConnectionRef) inInfo->isValidNonceContext;
	AirTunesConnection *	atCnx = (AirTunesConnection *) cnx->userContext;
	OSStatus				err;

	err = HTTPVerifyTimedNonce( inInfo->requestNoncePtr, inInfo->requestNonceLen, 30,
		kHTTPTimedNonceETagPtr, kHTTPTimedNonceETagLen,
		atCnx->atServer->httpTimedNonceKey, sizeof( atCnx->atServer->httpTimedNonceKey ) );
	return( !err );
}
#endif  // AIRPLAY_PASSWORDS

#if( AIRPLAY_PASSWORDS )
//===========================================================================================================================
//	_requestRequiresAuth
//===========================================================================================================================

static Boolean _requestRequiresAuth( HTTPMessageRef request )
{
	const char * const		methodPtr	= request->header.methodPtr;
	size_t const			methodLen	= request->header.methodLen;
	const char * const		pathPtr		= request->header.url.pathPtr;
	size_t const			pathLen		= request->header.url.pathLen;

	if( strnicmpx( methodPtr, methodLen, "POST" ) == 0 )
	{
		if( 0 ) {}
		#if( AIRPLAY_MFI )
		else if( strnicmp_suffix( pathPtr, pathLen, "/auth-setup" )		== 0 ) return( false );
		#endif
		else if( strnicmp_suffix( pathPtr, pathLen, "/info" )			== 0 ) return( false );
		else if( strnicmp_suffix( pathPtr, pathLen, "/perf" )			== 0 ) return( false );
	}
	else if( strnicmpx( methodPtr, methodLen, "GET" ) == 0 )
	{
		if(      strnicmp_suffix( pathPtr, pathLen, "/info" )			== 0 ) return( false );
		else if( strnicmp_suffix( pathPtr, pathLen, "/log" )			== 0 ) return( false );
		else if( strnicmp_suffix( pathPtr, pathLen, "/logs" )			== 0 ) return( false );
	}
	return( true );
}
#endif

//===========================================================================================================================
//	_requestSendPlistResponse
//===========================================================================================================================

static HTTPStatus _requestSendPlistResponse( HTTPConnectionRef inCnx, HTTPMessageRef request, CFPropertyListRef inPlist, OSStatus *outErr )
{
	HTTPStatus			status;
	OSStatus			err;
	const char *		httpProtocol;
	CFDataRef			data = NULL;
	const uint8_t *		ptr;
	size_t				len;
	HTTPMessageRef		response = inCnx->responseMsg;

	if( response->header.len == 0 )
	{
		httpProtocol = ( strnicmp_prefix( request->header.protocolPtr, request->header.protocolLen, "HTTP" ) == 0 )
			? "HTTP/1.1" : kAirTunesHTTPVersionStr;
		err = HTTPHeader_InitResponse( &response->header, httpProtocol, kHTTPStatus_OK, NULL );
		require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
		response->bodyLen = 0;
	}

	if( inPlist )
	{
		data = CFPropertyListCreateData( NULL, inPlist, kCFPropertyListBinaryFormat_v1_0, 0, NULL );
		require_action( data, exit, err = kUnknownErr; status = kHTTPStatus_InternalServerError );
		ptr = CFDataGetBytePtr( data );
		len = (size_t) CFDataGetLength( data );
		err = HTTPMessageSetBody( response, kMIMEType_AppleBinaryPlist, ptr, len );
	}
	else
	{
		err = HTTPMessageSetBody( response, NULL, NULL, 0 );
	}
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );

	status = kHTTPStatus_OK;

exit:
	CFReleaseNullSafe( data );
	if( outErr ) *outErr = err;
	return( status );
}

//===========================================================================================================================
//	_requestSendPlistResponseInfo
//===========================================================================================================================

static HTTPStatus _requestSendPlistResponseInfo( HTTPConnectionRef inCnx, HTTPMessageRef request, CFPropertyListRef inPlist, OSStatus *outErr )
{
	HTTPStatus			status;
	OSStatus			err;
	const char *		httpProtocol;
	CFDataRef			data = NULL;
	const uint8_t *		ptr;
	size_t				len;
	HTTPMessageRef		response = inCnx->responseMsg;

	if( response->header.len == 0 )
	{
		httpProtocol = ( strnicmp_prefix( request->header.protocolPtr, request->header.protocolLen, "HTTP" ) == 0 )
			? "HTTP/1.1" : kAirTunesHTTPVersionStr;
		err = HTTPHeader_InitResponse( &response->header, httpProtocol, kHTTPStatus_OK, NULL );
		require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );
		response->bodyLen = 0;
	}

	if( inPlist )
	{
		data = CFPropertyListCreateDataInfo( NULL, inPlist, kCFPropertyListBinaryFormat_v1_0, 0, NULL );
		require_action( data, exit, err = kUnknownErr; status = kHTTPStatus_InternalServerError );
		ptr = CFDataGetBytePtr( data );
		len = (size_t) CFDataGetLength( data );
		err = HTTPMessageSetBody( response, kMIMEType_AppleBinaryPlist, ptr, len );
	}
	else
	{
		err = HTTPMessageSetBody( response, NULL, NULL, 0 );
	}
	require_noerr_action( err, exit, status = kHTTPStatus_InternalServerError );

	status = kHTTPStatus_OK;

exit:
	CFReleaseNullSafe( data );
	if( outErr ) *outErr = err;
	return( status );
}


