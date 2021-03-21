/*
	File:    CFLiteNotifications.h
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
	
	Copyright (C) 2008-2013 Apple Inc. All Rights Reserved.
*/

#ifndef	__CFLiteNotifications_h__
#define	__CFLiteNotifications_h__

#include "APSCommonServices.h"
#include "APSDebugServices.h"
#include "CFCompat.h"

#ifdef __cplusplus
extern "C" {
#endif

//===========================================================================================================================
//	Constants
//===========================================================================================================================

typedef enum
{
	CFNotificationSuspensionBehaviorDrop				= 1,
	CFNotificationSuspensionBehaviorCoalesce			= 2,
	CFNotificationSuspensionBehaviorHold				= 3,
	CFNotificationSuspensionBehaviorDeliverImmediately	= 4
	
}	CFNotificationSuspensionBehavior;

enum
{
	kCFNotificationDeliverImmediately	= ( 1 << 0 ),
	kCFNotificationPostToAllSessions	= ( 1 << 1 )
};

//===========================================================================================================================
//	Types
//===========================================================================================================================

typedef struct CFNotificationCenter *		CFNotificationCenterRef;

typedef void
	( *CFNotificationCallback )( 
		CFNotificationCenterRef inCenter, 
		void *					inObserver, 
		CFStringRef				inName, 
		const void *			inObject, 
		CFDictionaryRef			inUserInfo );

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

OSStatus	CFNotificationCenter_Initialize( void );
void		CFNotificationCenter_Finalize( void );

CFNotificationCenterRef	CFNotificationCenterGetLocalCenter( void );
CFNotificationCenterRef	CFNotificationCenterGetDistributedCenter( void );

void
	CFNotificationCenterAddObserver(
		CFNotificationCenterRef 			inCenter, 
		const void *						inObserver, 
		CFNotificationCallback				inCallBack, 
		CFStringRef							inName, 
		const void *						inObject, 
		CFNotificationSuspensionBehavior	inSuspensionBehavior );

void
	CFNotificationCenterRemoveObserver(
		CFNotificationCenterRef	inCenter, 
		const void *			inObserver, 
		CFStringRef				inName, 
		const void *			inObject );

void	CFNotificationCenterRemoveEveryObserver( CFNotificationCenterRef inCenter, const void *inObserver );

void
	CFNotificationCenterPostNotification( 
		CFNotificationCenterRef inCenter, 
		CFStringRef				inName, 
		const void *			inObject, 
		CFDictionaryRef			inUserInfo, 
		Boolean					inDeliverImmediately );

void
	CFNotificationCenterPostNotificationWithOptions(
		CFNotificationCenterRef	inCenter, 
		CFStringRef				inName, 
		const void *			inObject, 
		CFDictionaryRef			inUserInfo, 
		CFOptionFlags			inOptions );

#if 0
#pragma mark == Debugging ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	CFLiteNotifications_Test
	@abstract	Unit test.
*/

OSStatus	CFLiteNotifications_Test( void );

#ifdef __cplusplus
}
#endif

#endif // __CFLiteNotifications_h__
