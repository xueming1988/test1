/*
	File:    AirTunesDACP.c
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
	
	Copyright (C) 2008-2013 Apple Inc. All Rights Reserved.
	
	To Do:
		
		-- Optionally send idle command every N seconds while session is active.
		-- Lazily make the thread when a request comes in.
		-- Automatically tear down connection and/or thread if idle for N seconds.
*/

#include "AirTunesDACP.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "AirPlayReceiverSession.h"
#include "AirPlayVersion.h"
#include "APSCommonServices.h"
#include "APSDebugServices.h"
#include "DACPCommon.h"
#include "dns_sd.h"
#include "HTTPNetUtils.h"
#include "HTTPUtils.h"
#include "NetUtils.h"
#include "StringUtils.h"
#include "ThreadUtils.h"
#include "TickUtils.h"

#include CF_HEADER

//===========================================================================================================================
//	Configuration
//===========================================================================================================================

// AIRTUNES_DACP_LOG_MESSAGES -- 1=Log all DACP requests and responses.

#if( !AIRTUNES_DACP_LOG_MESSAGES )
	#define AIRTUNES_DACP_LOG_MESSAGES		0
#endif

// AIRTUNES_HAVE_MDNS_GETADDRINFO -- 1 if this getaddrinfo resolves .local names. 0 requires manual DNS-SD lookups

#if( !defined( AIRTUNES_HAVE_MDNS_GETADDRINFO ) )
		#define AIRTUNES_HAVE_MDNS_GETADDRINFO		0
#endif

//===========================================================================================================================
//	Types
//===========================================================================================================================

typedef struct AirTunesDACPRequest *	AirTunesDACPRequestRef;
struct AirTunesDACPRequest
{
	AirTunesDACPRequestRef		nextRequest;
	Boolean						internal;
	char *						command;
};

// AirTunesDACPClient

#define kAirTunesDACPClient_Magic			0x64616370 // dacp
#define kAirTunesDACPClient_MagicBad		0x44414350 // DACP

#define kAirTunesDACPClient_MaxRequests		10

struct AirTunesDACPClient
{
	uint32_t							magic;			// Must be kAirTunesDACPClient_Magic 'dacp' if valid.
	pthread_mutex_t						mutex;
	pthread_mutex_t *					mutexPtr;
	pthread_cond_t						condition;		
	pthread_cond_t *					conditionPtr;
	pthread_t							thread;
	pthread_t *							threadPtr;
	NetSocketRef						netSock;
	Boolean								quit;			// true if we're quitting.
	Boolean								started;		// true if a session has been started. false if stopped.
	Boolean								connected;		// true if a session connection has been made.
	AirTunesDACPClientCallBackPtr		userCallBack;
	void *								userContext;
	
	AirTunesDACPRequestRef				requestList;
	int									requestCount;
	HTTPHeader							responseHeader;
	uint8_t								responseBodyBuf[ 32 * 1024 ];
	size_t								responseBodyLen;
	
	uint64_t							dacpID;
	uint32_t							activeRemoteID;
	
	Boolean								gotController;
	char								controllerDNSName[ kDNSServiceMaxDomainName ];
	uint16_t							controllerTCPPort;
	
	Boolean								gotAddrs;
	char *								addrsStr;
};

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

DEBUG_STATIC OSStatus
	_AirTunesDACPClient_ScheduleCommandInternal( 
		AirTunesDACPClientRef	inClient, 
		const char *			inCommand, 
		Boolean					inInternal );
DEBUG_STATIC void		_AirTunesDACPClient_FreeRequest( AirTunesDACPRequestRef inRequest );
DEBUG_STATIC void *		_AirTunesDACPClient_Thread( void *inArg );

DEBUG_STATIC OSStatus	_AirTunesDACPClient_SendCommand( AirTunesDACPClientRef inClient, const char *inCommand );
DEBUG_STATIC OSStatus	_AirTunesDACPClient_ResolveController( AirTunesDACPClientRef inClient );
DEBUG_STATIC void DNSSD_API
	_AirTunesDACPClient_ResolveControllerCallBack(
		DNSServiceRef			inServiceRef,
		DNSServiceFlags			inFlags,
		uint32_t				inIFI,
		DNSServiceErrorType		inErrorCode,
		const char *			inFullName,
		const char *			inTargetName,
		uint16_t				inPort,
		uint16_t				inTXTLen,
		const unsigned char *	inTXTPtr,
		void *					inContext );

#if( !AIRTUNES_HAVE_MDNS_GETADDRINFO )
	DEBUG_STATIC OSStatus	_AirTunesDACPClient_ResolveAddresses( AirTunesDACPClientRef inClient );
	DEBUG_STATIC void DNSSD_API
		_AirTunesDACPClient_ResolveAddressesCallBack(
			DNSServiceRef				inServiceRef,
			DNSServiceFlags				inFlags,
			uint32_t					inIFI,
			DNSServiceErrorType			inErrorCode,
			const char *				inHostName,
			const struct sockaddr *		inAddr,
			uint32_t					inTTL,
			void *						inContext );
#endif

//===========================================================================================================================
//	AirTunesDACPClient_Create
//===========================================================================================================================

OSStatus	AirTunesDACPClient_Create( AirTunesDACPClientRef *outClient )
{
	OSStatus					err;
	AirTunesDACPClientRef		obj;
	
	obj = (AirTunesDACPClientRef) calloc( 1, sizeof( *obj ) );
	require_action( obj, exit, err = kNoMemoryErr );
	
	obj->magic = kAirTunesDACPClient_Magic;
	
	err = pthread_mutex_init( &obj->mutex, NULL );
	require_noerr( err, exit );
	obj->mutexPtr = &obj->mutex;
	
	err = pthread_cond_init( &obj->condition, NULL );
	require_noerr( err, exit );
	obj->conditionPtr = &obj->condition;
	
	err = NetSocket_Create( &obj->netSock );
	require_noerr( err, exit );
	
	err = pthread_create( &obj->thread, NULL, _AirTunesDACPClient_Thread, obj );
	require_noerr( err, exit );
	obj->threadPtr = &obj->thread;
	
	*outClient = obj;
	obj = NULL;
	
exit:
	if( obj ) AirTunesDACPClient_Delete( obj );
	return( err );
}

//===========================================================================================================================
//	AirTunesDACPClient_Delete
//===========================================================================================================================

void	AirTunesDACPClient_Delete( AirTunesDACPClientRef inClient )
{
	OSStatus					err;
	AirTunesDACPRequestRef		request;
	
	DEBUG_USE_ONLY( err );
	
	require( inClient, exit );	
	require( inClient->magic == kAirTunesDACPClient_Magic, exit );
	
	// Cancel any current network operations, signal the thread, and wait for it to quit.
	
	if( inClient->mutexPtr )
	{
		pthread_mutex_lock( inClient->mutexPtr );
		inClient->quit = true;
		pthread_mutex_unlock( inClient->mutexPtr );
	}
	if( inClient->netSock )
	{
		err = NetSocket_Cancel( inClient->netSock );
		check_noerr( err );
	}
	if( inClient->threadPtr )
	{
		check( inClient->conditionPtr );
		err = pthread_cond_signal( &inClient->condition );
		check_noerr( err );
		
		err = pthread_join( inClient->thread, NULL );
		check_noerr( err );
		
		inClient->threadPtr = NULL;
	}
	
	inClient->magic = kAirTunesDACPClient_MagicBad;
	
	// Clean up resources.
	
	if( inClient->netSock )
	{
		err = NetSocket_Delete( inClient->netSock );
		check_noerr( err );
		inClient->netSock = NULL;
	}
	if( inClient->conditionPtr )
	{
		err = pthread_cond_destroy( inClient->conditionPtr );
		check_noerr( err );
		inClient->conditionPtr = NULL;
	}
	if( inClient->mutexPtr )
	{
		err = pthread_mutex_destroy( inClient->mutexPtr );
		check_noerr( err );
		inClient->mutexPtr = NULL;
	}
	while( ( request = inClient->requestList ) != NULL )
	{
		dlog( kLogLevelWarning, "### deleting request %p (%s) before running it\n", request, request->command );
		inClient->requestList = request->nextRequest;
		_AirTunesDACPClient_FreeRequest( request );
	}
	ForgetMem( &inClient->addrsStr );
	free( inClient );
	
exit:
	return;
}

//===========================================================================================================================
//	AirTunesDACPClient_SetCallBack
//===========================================================================================================================

OSStatus
	AirTunesDACPClient_SetCallBack( 
		AirTunesDACPClientRef			inClient, 
		AirTunesDACPClientCallBackPtr	inCallBack, 
		void *							inContext )
{
	OSStatus		err;
	
	require_action( inClient, exit, err = kParamErr );
	require_action( inClient->magic == kAirTunesDACPClient_Magic, exit, err = kBadReferenceErr );
	
	inClient->userCallBack	= inCallBack;
	inClient->userContext	= inContext;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirTunesDACPClient_SetDACPID
//===========================================================================================================================

OSStatus	AirTunesDACPClient_SetDACPID( AirTunesDACPClientRef inClient, uint64_t inDACPID )
{
	OSStatus		err;
	
	require_action( inClient, exit, err = kParamErr );
	require_action( inClient->magic == kAirTunesDACPClient_Magic, exit, err = kBadReferenceErr );
	
	inClient->dacpID = inDACPID;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirTunesDACPClient_SetActiveRemoteID
//===========================================================================================================================

OSStatus	AirTunesDACPClient_SetActiveRemoteID( AirTunesDACPClientRef inClient, uint32_t inActiveRemoteID )
{
	OSStatus		err;
	
	require_action( inClient, exit, err = kParamErr );
	require_action( inClient->magic == kAirTunesDACPClient_Magic, exit, err = kBadReferenceErr );
	
	inClient->activeRemoteID = inActiveRemoteID;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirTunesDACPClient_StartSession
//===========================================================================================================================

OSStatus	AirTunesDACPClient_StartSession( AirTunesDACPClientRef inClient )
{
	OSStatus		err;
	
	require_action( inClient, exit, err = kParamErr );
	require_action( inClient->magic == kAirTunesDACPClient_Magic, exit, err = kBadReferenceErr );
	
	pthread_mutex_lock( &inClient->mutex );
	remove("/tmp/airplay_stop");
	inClient->started = true;
	pthread_mutex_unlock( &inClient->mutex );
	err = kNoErr;
	//fprintf(stderr, "%s %d, dacp-id:%llX, active-remoteid:0x%x\n", 
	//	__FUNCTION__, __LINE__, inClient->dacpID, inClient->activeRemoteID);
	SocketClientReadWriteMsg("/tmp/Requesta01controller","setPlayerCmd:switchtoairplay", strlen("setPlayerCmd:switchtoairplay"),NULL,NULL,0);
	system("pkill imuzop");
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirTunesDACPClient_StopSession
//===========================================================================================================================

OSStatus	AirTunesDACPClient_StopSession( AirTunesDACPClientRef inClient )
{
	OSStatus		err;
	Boolean			wasStarted;
	
	require_action( inClient, exit, err = kParamErr );
	require_action( inClient->magic == kAirTunesDACPClient_Magic, exit, err = kBadReferenceErr );
	
	pthread_mutex_lock( &inClient->mutex );
	
	wasStarted = inClient->started;
	inClient->started = false;
	
	pthread_mutex_unlock( &inClient->mutex );
	
	if( wasStarted )
	{
		err = _AirTunesDACPClient_ScheduleCommandInternal( inClient, "stopSession", true );
		check_noerr( err );
	}
	
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirTunesDACPClient_ScheduleCommand
//===========================================================================================================================

OSStatus	AirTunesDACPClient_ScheduleCommand( AirTunesDACPClientRef inClient, const char *inCommand )
{
	return( _AirTunesDACPClient_ScheduleCommandInternal( inClient, inCommand, false ) );
}

//===========================================================================================================================
//	_AirTunesDACPClient_ScheduleCommandInternal
//===========================================================================================================================

DEBUG_STATIC OSStatus
	_AirTunesDACPClient_ScheduleCommandInternal( 
		AirTunesDACPClientRef	inClient, 
		const char *			inCommand, 
		Boolean					inInternal )
{
	OSStatus					err;
	Boolean						locked;
	AirTunesDACPRequestRef		newRequest;
	AirTunesDACPRequestRef *	requestEnd;
	
	locked		= false;
	newRequest	= NULL;
	
	require_action( inClient, exit, err = kParamErr );
	require_action( inClient->magic == kAirTunesDACPClient_Magic, exit, err = kBadReferenceErr );
	
	pthread_mutex_lock( &inClient->mutex );
	locked = true;
	
	// Reject new external requests if we're already at the max.
	
	if( !inInternal && ( inClient->requestCount >= kAirTunesDACPClient_MaxRequests ) )
	{
		dlog( kLogLevelWarning, "### too many outstanding DACP requests (%d)...rejecting command \"%s\" @ %N\n", 
			inClient->requestCount, inCommand );
		err = kNoResourcesErr;
		goto exit;
	}
	
	// Reject external requests if we don't have a valid controller yet (avoids a timeout later).
	
	if( !inInternal && ( ( inClient->dacpID == 0 ) || ( inClient->activeRemoteID == 0 ) ) )
	{
		dlog( kLogLevelWarning, "### no DACP controller yet...rejecting command \"%s\"  @ %N\n", inCommand );
		err = kTimeoutErr; // $$$ TO DO: treat this as a timeout for now for AppleTV compatibility.
		goto exit;
	}
	
	// Allocate a new request.
	
	newRequest = (AirTunesDACPRequestRef) calloc( 1, sizeof( *newRequest ) );
	require_action( newRequest, exit, err = kNoMemoryErr );
	
	newRequest->internal = inInternal;
	newRequest->command = strdup( inCommand );
	require_action( newRequest->command, exit, err = kNoMemoryErr );
	
	// Append to the request list.
	
	for( requestEnd = &inClient->requestList; *requestEnd; requestEnd = &( *requestEnd )->nextRequest ) {}
	*requestEnd = newRequest;
	newRequest = NULL;
	++inClient->requestCount;
	
	dlog( kLogLevelVerbose, "DACP command \"%s\" scheduled @ %N %s\n", inCommand, inInternal ? "(internal)" : "" );
	
	pthread_mutex_unlock( &inClient->mutex );
	locked = false;
	
	// Signal the thread so it knows there's a new request to process.
	
	err = pthread_cond_signal( &inClient->condition );
	check_noerr( err );
	
	err = kNoErr;
	
exit:
	if( newRequest )	_AirTunesDACPClient_FreeRequest( newRequest );
	if( locked )		pthread_mutex_unlock( &inClient->mutex );
	return( err );
}

//===========================================================================================================================
//	_AirTunesDACPClient_FreeRequest
//===========================================================================================================================

DEBUG_STATIC void	_AirTunesDACPClient_FreeRequest( AirTunesDACPRequestRef inRequest )
{
	if( inRequest->command ) free( inRequest->command );
	free( inRequest );
}

//===========================================================================================================================
//	_AirTunesDACPClient_Thread
//===========================================================================================================================

DEBUG_STATIC void *	_AirTunesDACPClient_Thread( void *inArg )
{
	AirTunesDACPClientRef const		me = (AirTunesDACPClientRef) inArg;
	OSStatus						err;
	AirTunesDACPRequestRef			request;
	
	pthread_setname_np_compat( "AirPlayDACP" );
	for( ;; )
	{
		// Wait for a request.
		
		pthread_mutex_lock( &me->mutex );
		while( !me->quit && ( ( request = me->requestList ) == NULL ) )
		{
			pthread_cond_wait( &me->condition, &me->mutex );
		}
		if( me->quit )
		{
			pthread_mutex_unlock( &me->mutex );
			break;
		}
		
		me->requestList = request->nextRequest;
		--me->requestCount;
		check( me->requestCount >= 0 );
		pthread_mutex_unlock( &me->mutex );
		
		// Process the request.
		
		if( request->internal )
		{
			if( strcmp( request->command, "stopSession" ) == 0 )
			{
				if( me->connected )
				{
					NetSocket_Disconnect( me->netSock, 2 );
					me->connected = false;
				}
				
				dlog( kLogLevelVerbose, "DACP command \"%s\" completed @ %N (internal)\n", request->command );
			}
			else
			{
				dlogassert( "unknown internal command: \"%s\"", request->command );
			}
		}
		else
		{
			err = _AirTunesDACPClient_SendCommand( me, request->command );
			if( err )
			{
				if( me->userCallBack ) me->userCallBack( err, me->userContext );
			}
		}
		
		_AirTunesDACPClient_FreeRequest( request );
	}
	
	dlog( kLogLevelTrace, "exiting DACP thread\n" );
	return( NULL );
}

//===========================================================================================================================
//	_AirTunesDACPClient_SendCommand
//===========================================================================================================================

DEBUG_STATIC OSStatus	_AirTunesDACPClient_SendCommand( AirTunesDACPClientRef inClient, const char *inCommand )
{
	OSStatus			err;
	Boolean				needDisconnect;
	SocketRef			nativeSock;
	char				buf[ 2048 ];
	HTTPHeader *		header;
	int					n;
	iovec_t				iov;
	iovec_t *			iop;
	int					ion;
	size_t				len;
	
	needDisconnect = false;
	
	// Check if the DACP controller idle-disconnected us by looking for a pending EOF.
	
	if( inClient->connected )
	{
		err = NetSocket_Peek( inClient->netSock, 1, 1, buf, &len, 0 );
		if( err == kConnectionErr )
		{
			dlog( kLogLevelNotice, "Cleaning up after peer disconnected to a force new connection\n" );
			NetSocket_Disconnect( inClient->netSock, 2 );
			inClient->connected = false;
		}
	}
	
	// Connect to the DACP controller if we're not already connected.
	
	if( !inClient->connected )
	{
		const char *		host;
		
		// Resolve via Bonjour to get the DNS name of the DACP controller.
		
		err = _AirTunesDACPClient_ResolveController( inClient );
		if( err == kCanceledErr ) goto exit;
		if( err )
		{
			dlog( kLogLevelNotice, "### Resolve DACP ID 0x%016llX failed: %#m\n", inClient->dacpID, err );
			goto exit;
		}
		
		// If we don't have Bonjour support built into getaddrinfo then use Bonjour to get the address list.
		
		#if( !AIRTUNES_HAVE_MDNS_GETADDRINFO )
			err = _AirTunesDACPClient_ResolveAddresses( inClient );
			if( err == kCanceledErr ) goto exit;
			if( err )
			{
				dlog( kLogLevelNotice, "### failed to resolve addresses for %s: %#m\n", inClient->controllerDNSName, err );
				goto exit;
			}
			
			host = inClient->addrsStr;
		#else
			host = inClient->controllerDNSName;
		#endif
		
		// Connect to the DACP controller.
		
		dlog( kLogLevelNotice, "DACP connecting to %s\n", host );
		
		err = NetSocket_TCPConnect( inClient->netSock, host, inClient->controllerTCPPort, 5 );
		if( err == kCanceledErr ) goto exit;
		if( err )
		{
			dlog( kLogLevelNotice, "### failed to connect to controller %s:%u: %#m\n", host, inClient->controllerTCPPort, err );
			goto exit;
		}
		
		// If a session has been started then stay connected. Otherwise, disconnect after sending the command.
		
		if( inClient->started ) inClient->connected = true;
		else					needDisconnect		= true;
		
		dlog( kLogLevelTrace, "    DACP connected     to  %s:%u %s\n", inClient->controllerDNSName, 
			inClient->controllerTCPPort, needDisconnect ? "transiently" : "" );
	}
	
	// Build the DACP request.
	
	n = snprintf( buf, sizeof( buf ), 
		"GET /ctrl-int/1/%s HTTP/1.1\r\n"
		"Host: %s\r\n"
		"Active-Remote: %u\r\n"
		"%s: %s\r\n"
		"\r\n", 
		inCommand, inClient->controllerDNSName, inClient->activeRemoteID, 
		kHTTPHeader_UserAgent, kAirPlayUserAgentStr );
	require_action( ( n > 0 ) && ( n < (int) sizeof( buf ) ), exit, err = kInternalErr );
	
#if( AIRTUNES_DACP_LOG_MESSAGES )
	dlog( kLogLevelNotice, "********** DACP REQUEST **********\n" );
	dlog( kLogLevelNotice, "%s", buf );
#endif
	
	// Write the request.
	
	nativeSock = NetSocket_GetNative( inClient->netSock );
	
	iov.iov_base = buf;
	iov.iov_len  = (unsigned int) n;
	iop = &iov;
	ion = 1;
	for( ;; )
	{
		err = SocketWriteData( nativeSock, &iop, &ion );
		if( err == kNoErr )			break;
		if( err == EWOULDBLOCK )	err = kNoErr;
		require_noerr( err, exit );
		
		err = NetSocket_Wait( inClient->netSock, nativeSock, kNetSocketWaitType_Write, 5 );
		if( err == kCanceledErr ) goto exit;
		require_noerr( err, exit );
	}
	
	// Read the response header.
	
	header = &inClient->responseHeader;
	header->len = 0;
	for( ;; )
	{
		err = HTTPReadHeader( nativeSock, header->buf, sizeof( header->buf ), &header->len );
		if( err == kNoErr )			break;
		if( err == EWOULDBLOCK )	err = kNoErr;
		require_noerr( err, exit );
		
		err = NetSocket_Wait( inClient->netSock, nativeSock, kNetSocketWaitType_Read, 5 );
		if( err == kCanceledErr ) goto exit;
		require_noerr( err, exit );
	}
	
	err = HTTPHeader_Parse( header );
	require_noerr( err, exit );
	require_action( header->statusCode != 0, exit, err = kResponseErr );
	require_action( IsHTTPSuccessStatusCode( header->statusCode ), exit, err = header->statusCode );
	
	// Read the response body. We expect a small body so if it doesn't fit in our fixed buffer then fail.
	
	require_action( header->contentLength < sizeof( inClient->responseBodyBuf ), exit, err = kSizeErr );
	len = (size_t) header->contentLength;
	
	inClient->responseBodyLen = 0;
	if( len > 0 )
	{
		err = NetSocket_Read( inClient->netSock, len, len, inClient->responseBodyBuf, &inClient->responseBodyLen, -1 );
		if( err == kCanceledErr ) goto exit;
		if( err == kConnectionErr )	goto exit;
		require_noerr( err, exit );
	}
	
#if( AIRTUNES_DACP_LOG_MESSAGES )
	dlog( kLogLevelNotice, "********** DACP RESPONSE **********\n" );
	dlog( kLogLevelNotice, "%s", header->buf );
#endif
	
exit:
	if( needDisconnect )
	{
		NetSocket_Disconnect( inClient->netSock, 2 );
		dlog( kLogLevelTrace, "    DACP disconnected from %s:%u transiently\n", 
			inClient->controllerDNSName, inClient->controllerTCPPort );
	}
	else if( err && inClient->connected )
	{
		dlog( kLogLevelNotice, "Cleaning up connection after error: %#m\n", err );
		NetSocket_Disconnect( inClient->netSock, 0 );
		inClient->connected = false;
	}
	if( !err )	dlog( kLogLevelVerbose, "DACP command \"%s\" completed @ %N\n", inCommand );
	else		dlog( kLogLevelVerbose, "DACP command \"%s\" failed    @ %N: %#m\n", inCommand, err );
	return( err );
}

//===========================================================================================================================
//	_AirTunesDACPClient_ResolveController
//===========================================================================================================================

DEBUG_STATIC OSStatus	_AirTunesDACPClient_ResolveController( AirTunesDACPClientRef inClient )
{
	OSStatus			err;
	char				name[ kDNSServiceMaxDomainName ];
	DNSServiceRef		resolverRef;
	SocketRef			resolverSock;
	uint64_t			timeout;
	
	resolverRef = NULL;
	
	inClient->gotController = false;
	
	// Start a Bonjour resolve of the iTunes_Ctrl controller.
	
	SNPrintF( name, sizeof( name ), "%s%016llX", kDACPBonjourServiceNamePrefix, inClient->dacpID );
	err = DNSServiceResolve( &resolverRef, 0, kDNSServiceInterfaceIndexAny, name, kDACPBonjourServiceType,
		kDACPBonjourServiceDomain, _AirTunesDACPClient_ResolveControllerCallBack, inClient );
	require_noerr( err, exit );
	
	// Wait for the Bonjour response.
	
	resolverSock = DNSServiceRefSockFD( resolverRef );
	if( IsValidSocket( resolverSock ) )
	{
		for( ;; )
		{
			err = NetSocket_Wait( inClient->netSock, resolverSock, kNetSocketWaitType_Read, 5 );
			require_noerr_quiet( err, exit );
			
			err = DNSServiceProcessResult( resolverRef );
			require_noerr( err, exit );
			
			if( inClient->gotController )
			{
				break;
			}
		}
	}
	else
	{
		// No socket so assume we're using the DNS-SD shim that runs async and poll for completion.
		
		timeout = UpTicks() + ( 5 * UpTicksPerSecond() );
		while( !inClient->gotController )
		{
			if( UpTicks() >= timeout )
			{
				err = kTimeoutErr;
				break;
			}
			usleep( 50000 );
		}
	}
	
exit:
	if( resolverRef ) DNSServiceRefDeallocate( resolverRef );
	return( err );
}

//===========================================================================================================================
//	_AirTunesDACPClient_ResolveControllerCallBack
//===========================================================================================================================

DEBUG_STATIC void DNSSD_API
	_AirTunesDACPClient_ResolveControllerCallBack(
		DNSServiceRef			inServiceRef,
		DNSServiceFlags			inFlags,
		uint32_t				inIFI,
		DNSServiceErrorType		inErrorCode,
		const char *			inFullName,
		const char *			inTargetName,
		uint16_t				inPort,
		uint16_t				inTXTLen,
		const unsigned char *	inTXTPtr,
		void *					inContext )
{
	AirTunesDACPClientRef const		me = (AirTunesDACPClientRef) inContext;
	
	(void) inServiceRef;	// Unused
	(void) inFlags;			// Unused
	(void) inIFI;			// Unused
	(void) inFullName;		// Unused
	(void) inTXTLen;		// Unused
	(void) inTXTPtr;		// Unused
	
	if( inErrorCode == -65791 ) goto exit; // Ignore mStatus_ConfigChanged
	require_noerr( inErrorCode, exit );
	
	strlcpy( me->controllerDNSName, inTargetName, sizeof( me->controllerDNSName ) );
	me->controllerTCPPort = ntohs( inPort );
	me->gotController = true;
	
exit:
	return;
}

#if( !AIRTUNES_HAVE_MDNS_GETADDRINFO )

//===========================================================================================================================
//	_AirTunesDACPClient_ResolveAddresses
//===========================================================================================================================

DEBUG_STATIC OSStatus	_AirTunesDACPClient_ResolveAddresses( AirTunesDACPClientRef inClient )
{
	OSStatus			err;
	DNSServiceRef		resolverRef;
	SocketRef			resolverSock;
	uint64_t			timeout;
	
	resolverRef = NULL;
	
	inClient->gotAddrs = false;
	ForgetMem( &inClient->addrsStr );
	
	// Start the async address queries.
	
	err = DNSServiceGetAddrInfo( &resolverRef, 0, kDNSServiceInterfaceIndexAny, 0, inClient->controllerDNSName, 
		_AirTunesDACPClient_ResolveAddressesCallBack, inClient );
	require_noerr( err, exit );
	
	// Wait for the Bonjour response.
	
	resolverSock = DNSServiceRefSockFD( resolverRef );
	if( IsValidSocket( resolverSock ) )
	{
		for( ;; )
		{
			err = NetSocket_Wait( inClient->netSock, resolverSock, kNetSocketWaitType_Read, 5 );
			if( err )
			{
				// If we got at least 1 address then treat it as a success.
				
				if( inClient->addrsStr )
				{
					dlog( kLogLevelNotice, "### DACP resolve addresses timed out, but got some: %s\n", inClient->addrsStr );
					inClient->gotAddrs = true;
					err = kNoErr;
				}
				break;
			}
			
			err = DNSServiceProcessResult( resolverRef );
			require_noerr( err, exit );
			
			if( inClient->gotAddrs )
			{
				break;
			}
		}
	}
	else
	{
		// No socket so assume we're using the DNS-SD shim that runs async and poll for completion.
		
		timeout = UpTicks() + ( 5 * UpTicksPerSecond() );
		while( !inClient->gotAddrs )
		{
			if( UpTicks() >= timeout )
			{
				// If we got at least 1 address then treat it as a success.
				
				if( inClient->addrsStr )
				{
					dlog( kLogLevelNotice, "### DACP resolve addresses timed out, but got some: %s\n", inClient->addrsStr );
					inClient->gotAddrs = true;
				}
				else
				{
					err = kTimeoutErr;
				}
				break;
			}
			usleep( 50000 );
		}
	}
	
exit:
	if( resolverRef ) DNSServiceRefDeallocate( resolverRef );
	return( err );
}

//===========================================================================================================================
//	_AirTunesDACPClient_ResolveAddressesCallBack
//===========================================================================================================================

DEBUG_STATIC void DNSSD_API
	_AirTunesDACPClient_ResolveAddressesCallBack(
		DNSServiceRef				inServiceRef,
		DNSServiceFlags				inFlags,
		uint32_t					inIFI,
		DNSServiceErrorType			inErrorCode,
		const char *				inHostName,
		const struct sockaddr *		inAddr,
		uint32_t					inTTL,
		void *						inContext )
{
	AirTunesDACPClientRef const		me = (AirTunesDACPClientRef) inContext;
	OSStatus						err;
	char							str[ 128 ];
	
	(void) inServiceRef;	// Unused
	(void) inIFI;			// Unused
	(void) inHostName;		// Unused
	(void) inTTL;			// Unused
	
	if( inErrorCode == -65791 ) goto exit; // Ignore mStatus_ConfigChanged
	require_noerr( inErrorCode, exit );
	require( inAddr, exit );
#if( defined( AF_INET6 ) )
	require( ( inAddr->sa_family == AF_INET ) || ( inAddr->sa_family == AF_INET6 ), exit );
#else
	require(   inAddr->sa_family == AF_INET, exit );
#endif
	require_quiet( inFlags & kDNSServiceFlagsAdd, exit );
	
	// Append the address string to our comma-separated list of addresses.
	
	err = SockAddrToString( inAddr, kSockAddrStringFlagsNone, str );
	require_noerr( err, exit );
		
	if( me->addrsStr )
	{
		char *		newStr;
		ssize_t		n;
		
		// Insert IPv6 addresses at the front to prioritize them over IPv4.
		
		#if( defined( AF_INET6 ) )
		if( ( inAddr->sa_family == AF_INET6 ) && 
			IN6_IS_ADDR_LINKLOCAL( &( (const struct sockaddr_in6 *) inAddr )->sin6_addr ) )
		{
			n = ASPrintF( &newStr, "%s,%s", str, me->addrsStr );
			require_action( n > 0, exit, err = kNoMemoryErr );
		}
		else
		#endif
		{
			n = ASPrintF( &newStr, "%s,%s", me->addrsStr, str );
			require_action( n > 0, exit, err = kNoMemoryErr );
		}
		free( me->addrsStr );
		me->addrsStr = newStr;
	}
	else
	{
		me->addrsStr = strdup( str );
		require_action( me->addrsStr, exit, err = kNoMemoryErr );
	}
	
	// If there's no more coming then mark us as done.
	
	if( !( inFlags & kDNSServiceFlagsMoreComing ) )
	{
		me->gotAddrs = true;
	}
	
exit:
	return;
}

#endif // !AIRTUNES_HAVE_MDNS_GETADDRINFO
