/*
	File:    HTTPMessage.c
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
	
	Copyright (C) 2011-2014 Apple Inc. All Rights Reserved.
*/

#include "HTTPMessage.h"

#include "APSCommonServices.h"
#include "HTTPNetUtils.h"
#include "NetUtils.h"

#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static void _HTTPMessageGetTypeID( void *inContext );
static void	_HTTPMessageFinalize( CFTypeRef inCF );

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static dispatch_once_t			gHTTPMessageInitOnce = 0;
static CFTypeID					gHTTPMessageTypeID = _kCFRuntimeNotATypeID;
static const CFRuntimeClass		kHTTPMessageClass = 
{
	0,						// version
	"HTTPMessage",			// className
	NULL,					// init
	NULL,					// copy
	_HTTPMessageFinalize,	// finalize
	NULL,					// equal -- NULL means pointer equality.
	NULL,					// hash  -- NULL means pointer hash.
	NULL,					// copyFormattingDesc
	NULL,					// copyDebugDesc
	NULL,					// reclaim
	NULL					// refcount
};

//===========================================================================================================================
//	HTTPMessageGetTypeID
//===========================================================================================================================

CFTypeID	HTTPMessageGetTypeID( void )
{
	dispatch_once_f( &gHTTPMessageInitOnce, NULL, _HTTPMessageGetTypeID );
	return( gHTTPMessageTypeID );
}

static void _HTTPMessageGetTypeID( void *inContext )
{
	(void) inContext;
	
	gHTTPMessageTypeID = _CFRuntimeRegisterClass( &kHTTPMessageClass );
	check( gHTTPMessageTypeID != _kCFRuntimeNotATypeID );
}

//===========================================================================================================================
//	HTTPMessageCreate
//===========================================================================================================================

OSStatus	HTTPMessageCreate( HTTPMessageRef *outMessage )
{
	OSStatus			err;
	HTTPMessageRef		me;
	size_t				extraLen;
	
	extraLen = sizeof( *me ) - sizeof( me->base );
	me = (HTTPMessageRef) _CFRuntimeCreateInstance( NULL, HTTPMessageGetTypeID(), (CFIndex) extraLen, NULL );
	require_action( me, exit, err = kNoMemoryErr );
	memset( ( (uint8_t *) me ) + sizeof( me->base ), 0, extraLen );
	
	me->maxBodyLen = kHTTPDefaultMaxBodyLen;
	HTTPMessageReset( me );
	
	*outMessage = me;
	me = NULL;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	_HTTPMessageFinalize
//===========================================================================================================================

static void	_HTTPMessageFinalize( CFTypeRef inCF )
{
	HTTPMessageRef const		me = (HTTPMessageRef) inCF;
	
	HTTPMessageReset( me );
}

//===========================================================================================================================
//	HTTPMessageReset
//===========================================================================================================================

void	HTTPMessageReset( HTTPMessageRef inMsg )
{
	inMsg->header.len	= 0;
	inMsg->headerRead	= false;
	inMsg->bodyPtr		= inMsg->smallBodyBuf;
	inMsg->bodyLen		= 0;
	inMsg->bodyOffset	= 0;
	inMsg->timeoutNanos	= kHTTPNoTimeout;
	ForgetMem( &inMsg->bigBodyBuf );
}

//===========================================================================================================================
//	HTTPMessageReadMessage
//===========================================================================================================================

OSStatus	HTTPMessageReadMessage( SocketRef inSock, HTTPMessageRef inMsg )
{
	HTTPHeader * const		hdr = &inMsg->header;
	OSStatus				err;
	size_t					len;
	
	if( !inMsg->headerRead )
	{
		err = SocketReadHTTPHeader( inSock, hdr );
		require_noerr_quiet( err, exit );
		inMsg->headerRead = true;
		
		require_action( hdr->contentLength <= inMsg->maxBodyLen, exit, err = kSizeErr );
		err = HTTPMessageSetBodyLength( inMsg, (size_t) hdr->contentLength );
		require_noerr( err, exit );
	}
	
	len = hdr->extraDataLen;
	if( len > 0 )
	{
		len = Min( len, inMsg->bodyLen );
		memmove( inMsg->bodyPtr, hdr->extraDataPtr, len );
		hdr->extraDataPtr += len;
		hdr->extraDataLen -= len;
		inMsg->bodyOffset += len;
	}
	
	err = SocketReadData( inSock, inMsg->bodyPtr, inMsg->bodyLen, &inMsg->bodyOffset );
	require_noerr_quiet( err, exit );
	
exit:
	return( err );
}

//===========================================================================================================================
//	HTTPMessageSetBody
//===========================================================================================================================

OSStatus	HTTPMessageSetBody( HTTPMessageRef inMsg, const char *inContentType, const void *inData, size_t inLen )
{
	OSStatus		err;
	
	err = inMsg->header.firstErr;
	require_noerr( err, exit );
	
	HTTPMessageSetBodyLength( inMsg, inLen );
	if( inData && ( inData != inMsg->bodyPtr ) )  // Handle inData pointing to the buffer.
	{
		memmove( inMsg->bodyPtr, inData, inLen ); // memmove in case inData is in the middle of the buffer.
	}
	
	HTTPHeader_SetField( &inMsg->header, "Content-Length", "%zu", inLen );
	if( inContentType ) HTTPHeader_SetField( &inMsg->header, "Content-Type", inContentType );
	err = kNoErr;
	
exit:
	if( err && !inMsg->header.firstErr ) inMsg->header.firstErr = err;
	return( err );
}

//===========================================================================================================================
//	HTTPMessageSetBodyLength
//===========================================================================================================================

OSStatus	HTTPMessageSetBodyLength( HTTPMessageRef inMsg, size_t inLen )
{
	OSStatus		err;
	
	ForgetMem( &inMsg->bigBodyBuf );
	if( inLen <= sizeof( inMsg->smallBodyBuf ) )
	{
		inMsg->bodyPtr = inMsg->smallBodyBuf;
	}
	else
	{
		inMsg->bigBodyBuf = (uint8_t *) malloc( inLen );
		require_action( inMsg->bigBodyBuf, exit, err = kNoMemoryErr );
		inMsg->bodyPtr = inMsg->bigBodyBuf;
	}
	inMsg->bodyLen = inLen;
	err = kNoErr;
	
exit:
	return( err );
}

//===========================================================================================================================
//	HTTPMessageSetCompletionBlock
//===========================================================================================================================

