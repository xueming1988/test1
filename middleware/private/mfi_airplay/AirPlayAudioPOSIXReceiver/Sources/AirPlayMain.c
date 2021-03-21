/*
	File:    AirPlayMain.c
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

#include "AirPlayMain.h"

#include "AirPlayCommon.h"
#include "AirPlayReceiverServer.h"
#include "AirPlayVersion.h"
#include "APSCommonServices.h"
#include "APSDebugServices.h"
#include "RandomNumberUtils.h"
#include "StringUtils.h"
#include "SystemUtils.h"
#include "ThreadUtils.h"

	#include <errno.h>
	#include <syslog.h>

#if( AIRPLAY_IPC_SERVER )
	#include "AirPlayReceiverIPCCommon.h"
#endif

//===========================================================================================================================
//	Internals
//===========================================================================================================================

	static void *	AirPlayMainThread( void *inArg );
	
	static pthread_t		gAirPlayMainThread;
	static pthread_t *		gAirPlayMainThreadPtr = NULL;

//===========================================================================================================================
//	main
//===========================================================================================================================

int	AirPlayMain( int argc, const char **argv )
{
	OSStatus						err;
	AirPlayReceiverServerRef		server = NULL;
	int								argi;
	const char *					arg;
	const char *					ifname = NULL;
	
	for( argi = 1; argi < argc; )
	{
		arg = argv[ argi++ ];
		if( 0 ) {}
		
		// Unknown
		
		else
		{
			fprintf( stderr, "warning: unknown option '%s'\n", arg );
		}
	}
	
#if( AIRPLAY_IPC_SERVER )
	(void) ifname;

	err = AirPlayReceiverIPCServerInitialize();
	require_noerr( err, exit );
#else
	err = AirPlayReceiverServerCreate( &server );
	require_noerr( err, exit );
	
	if( ifname ) AirPlayReceiverServerSetCString( server, CFSTR( kAirPlayProperty_InterfaceName ), NULL, ifname, kSizeCString );
	
	err = AirPlayReceiverServerStart( server );
	require_noerr( err, exit );
#endif
	
	CFRunLoopRun();
	
	AirPlayReceiverServerStop( server );
	
exit:
	CFReleaseNullSafe( server );
	return( err ? 1 : 0 );
}

//===========================================================================================================================
//	AirPlayStartMain
//===========================================================================================================================

OSStatus	AirPlayStartMain( void )
{
	OSStatus		err;
	
	require_action( gAirPlayMainThreadPtr == NULL, exit, err = kAlreadyInUseErr );
	
	err = pthread_create( &gAirPlayMainThread, NULL, AirPlayMainThread, NULL );
	require_noerr( err, exit );
	gAirPlayMainThreadPtr = &gAirPlayMainThread;
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayStopMain
//===========================================================================================================================

OSStatus	AirPlayStopMain( void )
{
	OSStatus		err;
	
	require_action( gAirPlayMainThreadPtr, exit, err = kNotInUseErr );
	
	CFRunLoopStop( CFRunLoopGetMain() );
	
	err = pthread_join( gAirPlayMainThread, NULL );
	check_noerr( err );
	gAirPlayMainThreadPtr = NULL;
	
exit:
	return( err );
}

//===========================================================================================================================
//	AirPlayMainThread
//===========================================================================================================================

static void *	AirPlayMainThread( void *inArg )
{
	(void) inArg;
	
	pthread_setname_np_compat( "AirPlayMain" );
	AirPlayMain( 0, NULL );
	return( NULL );
}
