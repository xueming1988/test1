/*
	File:    SystemUtils.c
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

#if( !defined( _CRT_SECURE_NO_DEPRECATE ) )
	#define _CRT_SECURE_NO_DEPRECATE		1
#endif

#include "SystemUtils.h"

#include <ctype.h>
#include <string.h>

#include "APSCommonServices.h"
#include "APSDebugServices.h"
#include "CFUtils.h"
#include "StringUtils.h"

	#include <sys/stat.h>
	#include <sys/sysctl.h>

int CompareOSBuildVersionStrings( const char *inVersion1, const char *inVersion2 )
{
	int result;
	int major1, major2, build1, build2;
	char minor1, minor2;
		
	result = sscanf( inVersion1, "%u%c%u", &major1, &minor1, &build1 );
	require_action( result == 3, exit, result = -1 );
	minor1 = (char) toupper( (int) minor1 );

	result = sscanf( inVersion2, "%u%c%u", &major2, &minor2, &build2 );
	require_action( result == 3, exit, result = 1 );
	minor2 = (char) toupper( (int) minor2 );

	result = ( major1 != major2 ) ? ( major1 - major2 ) : ( ( minor1 != minor2 ) ? ( minor1 - minor2 ) : ( build1 - build2 ) );
	
exit:
	return( result );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	GestaltSetHook
//===========================================================================================================================

static GestaltHook_f		gGestaltHook_f   = NULL;
static void *				gGestaltHook_ctx = NULL;

void	GestaltSetHook( GestaltHook_f inHook, void *inContext )
{
	gGestaltHook_f   = inHook;
	gGestaltHook_ctx = inContext;
}

//===========================================================================================================================
//	GestaltCopyAnswer
//===========================================================================================================================

CFTypeRef	GestaltCopyAnswer( CFStringRef inQuestion, CFDictionaryRef inOptions, OSStatus *outErr )
{
	CFTypeRef		answer;
	
	if( gGestaltHook_f )
	{
		answer = gGestaltHook_f( inQuestion, inOptions, outErr, gGestaltHook_ctx );
		if( answer ) return( answer );
	}
	
	if( outErr ) *outErr = kNotFoundErr;
	return( NULL );
}

//===========================================================================================================================
//	GestaltGetBoolean
//===========================================================================================================================

Boolean	GestaltGetBoolean( CFStringRef inQuestion, CFDictionaryRef inOptions, OSStatus *outErr )
{
	Boolean			b;
	CFTypeRef		obj;
	
	obj = GestaltCopyAnswer( inQuestion, inOptions, outErr );
	if( obj )
	{
		b = CFGetBoolean( obj, outErr );
		CFRelease( obj );
	}
	else
	{
		b = false;
	}
	return( b );
}

//===========================================================================================================================
//	GestaltGetCString
//===========================================================================================================================

char *	GestaltGetCString( CFStringRef inQuestion, CFDictionaryRef inOptions, char *inBuf, size_t inMaxLen, OSStatus *outErr )
{
	CFTypeRef		obj;
	char *			ptr;
	
	obj = GestaltCopyAnswer( inQuestion, inOptions, outErr );
	if( obj )
	{
		ptr = CFGetCString( obj, inBuf, inMaxLen );
		CFRelease( obj );
		if( outErr ) *outErr = kNoErr;
	}
	else
	{
		ptr = inBuf;
	}
	return( ptr );
}

//===========================================================================================================================
//	GestaltGetData
//===========================================================================================================================

uint8_t *
	GestaltGetData( 
		CFStringRef		inQuestion, 
		CFDictionaryRef	inOptions, 
		void *			inBuf, 
		size_t			inMaxLen, 
		size_t *		outLen, 
		OSStatus *		outErr )
{
	uint8_t *		ptr;
	CFTypeRef		obj;
	
	obj = GestaltCopyAnswer( inQuestion, inOptions, outErr );
	if( obj )
	{
		ptr = CFGetData( obj, inBuf, inMaxLen, outLen, outErr );
		CFRelease( obj );
	}
	else
	{
		ptr = NULL;
		if( outLen ) *outLen = 0;
	}
	return( ptr );
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	GetDeviceInternalModelString
//===========================================================================================================================

//===========================================================================================================================
//	GetDeviceName
//===========================================================================================================================

//===========================================================================================================================
//	GetDeviceName
//===========================================================================================================================

//===========================================================================================================================
//	GetDeviceUniqueID
//===========================================================================================================================

//===========================================================================================================================
//	GetSystemBuildVersionString
//===========================================================================================================================

