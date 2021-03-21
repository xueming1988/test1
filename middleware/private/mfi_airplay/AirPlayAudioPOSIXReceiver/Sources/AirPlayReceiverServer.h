/*
	File:    AirPlayReceiverServer.h
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
	
	Copyright (C) 2012-2014 Apple Inc. All Rights Reserved.
*/
/*!
    @header AirPlay Server Platform APIs
    @discussion Platform APIs related to AirPlay Server.
*/

#ifndef	__AirPlayReceiverServer_h__
#define	__AirPlayReceiverServer_h__

#include "AirPlayCommon.h"
#include "APSCommonServices.h"
#include "APSDebugServices.h"
#include "CFUtils.h"

#include CF_HEADER
#include LIBDISPATCH_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark == Creation ==
#endif

typedef struct AirPlayReceiverServerPrivate *		AirPlayReceiverServerRef;
typedef struct AirPlayReceiverSessionPrivate *		AirPlayReceiverSessionRef;

CFTypeID			AirPlayReceiverServerGetTypeID( void );
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerCreate
	@internal
	@abstract	Creates the server and initializes the server. Caller must CFRelease it when done.
*/
OSStatus			AirPlayReceiverServerCreate( AirPlayReceiverServerRef *outServer );
#if( AIRPLAY_CONFIG_FILES )
OSStatus			AirPlayReceiverServerCreateWithConfigFilePath( CFStringRef inConfigFilePath, AirPlayReceiverServerRef *outServer );
#endif
dispatch_queue_t	AirPlayReceiverServerGetDispatchQueue( AirPlayReceiverServerRef inServer );

#if 0
#pragma mark -
#pragma mark == Delegate ==
#endif

typedef OSStatus
	( *AirPlayReceiverServerControl_f )( 
		AirPlayReceiverServerRef	inServer, 
		CFStringRef					inCommand, 
		CFTypeRef					inQualifier, 
		CFDictionaryRef				inParams, 
		CFDictionaryRef *			outParams, 
		void *						inContext );

typedef CFTypeRef
	( *AirPlayReceiverServerCopyProperty_f )( 
		AirPlayReceiverServerRef	inServer, 
		CFStringRef					inProperty, 
		CFTypeRef					inQualifier, 
		OSStatus *					outErr, 
		void *						inContext );

typedef OSStatus
	( *AirPlayReceiverServerSetProperty_f )( 
		AirPlayReceiverServerRef	inServer, 
		CFStringRef					inProperty, 
		CFTypeRef					inQualifier, 
		CFTypeRef					inValue, 
		void *						inContext );

typedef void
	( *AirPlayReceiverSessionCreated_f )( 
		AirPlayReceiverServerRef	inServer, 
		AirPlayReceiverSessionRef	inSession, 
		void *						inContext );
	
typedef void
	( *AirPlayReceiverSessionFailed_f )(
		AirPlayReceiverServerRef	inServer,
		OSStatus					inReason,
		void *						inContext );

typedef struct
{
	void *									context;			// Context pointer for the delegate to use.
	void *									context2;			// Extra context pointer for the delegate to use.
	AirPlayReceiverServerControl_f			control_f;			// Function to call for control requests.
	AirPlayReceiverServerCopyProperty_f		copyProperty_f;		// Function to call for copyProperty requests.
	AirPlayReceiverServerSetProperty_f		setProperty_f;		// Function to call for setProperty requests.
	AirPlayReceiverSessionCreated_f			sessionCreated_f;	// Function to call when a session is created.
	AirPlayReceiverSessionFailed_f			sessionFailed_f;	// Function to call when a session creation fails.
	
}	AirPlayReceiverServerDelegate;

#define AirPlayReceiverServerDelegateInit( PTR )	memset( (PTR), 0, sizeof( AirPlayReceiverServerDelegate ) );

void	AirPlayReceiverServerSetDelegate( AirPlayReceiverServerRef inServer, const AirPlayReceiverServerDelegate *inDelegate );

#if 0
#pragma mark -
#pragma mark == Control ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerControl
	@internal
	@abstract	Controls the server.
*/
OSStatus
	AirPlayReceiverServerControl( 
		CFTypeRef			inServer, // Must be AirPlayReceiverServerRef.
		uint32_t			inFlags, 
		CFStringRef			inCommand, 
		CFTypeRef			inQualifier, 
		CFDictionaryRef		inParams, 
		CFDictionaryRef *	outParams );

// Convenience accessors.

#define AirPlayReceiverServerStart( SERVER )	\
	AirPlayReceiverServerControl( (SERVER), kCFObjectFlagDirect, CFSTR( kAirPlayCommand_StartServer ), NULL, NULL, NULL )

#define AirPlayReceiverServerStop( SERVER )	\
	AirPlayReceiverServerControl( (SERVER), kCFObjectFlagDirect, CFSTR( kAirPlayCommand_StopServer ), NULL, NULL, NULL )

#define AirPlayReceiverServerControlF( SERVER, COMMAND, QUALIFIER, OUT_RESPONSE, FORMAT, ... ) \
	CFObjectControlSyncF( (SERVER), NULL, AirPlayReceiverServerControl, kCFObjectFlagDirect, \
		(COMMAND), (QUALIFIER), (OUT_RESPONSE), (FORMAT), __VA_ARGS__ )

#define AirPlayReceiverServerControlV( SERVER, COMMAND, QUALIFIER, OUT_RESPONSE, FORMAT, ARGS ) \
	CFObjectControlSyncV( (SERVER), NULL, AirPlayReceiverServerControl, kCFObjectFlagDirect, \
		(COMMAND), (QUALIFIER), (OUT_RESPONSE), (FORMAT), (ARGS) )

#define AirPlayReceiverServerControlAsync( SERVER, COMMAND, QUALIFIER, PARAMS, RESPONSE_QUEUE, RESPONSE_FUNC, RESPONSE_CONTEXT ) \
	CFObjectControlAsync( (SERVER), AirPlayReceiverServerGetDispatchQueue( SERVER ), AirPlayReceiverServerControl, \
		kCFObjectFlagAsync, (COMMAND), (QUALIFIER), (PARAMS), (RESPONSE_QUEUE), (RESPONSE_FUNC), (RESPONSE_CONTEXT) )

#define AirPlayReceiverServerControlAsyncF( SERVER, COMMAND, QUALIFIER, RESPONSE_QUEUE, RESPONSE_FUNC, RESPONSE_CONTEXT, FORMAT, ... ) \
	CFObjectControlAsyncF( (SERVER), AirPlayReceiverServerGetDispatchQueue( SERVER ), AirPlayReceiverServerControl, \
		kCFObjectFlagAsync, (COMMAND), (QUALIFIER), (RESPONSE_QUEUE), (RESPONSE_FUNC), (RESPONSE_CONTEXT), (FORMAT), __VA_ARGS__ )

#define AirPlayReceiverServerControlAsyncV( SERVER, COMMAND, QUALIFIER, RESPONSE_QUEUE, RESPONSE_FUNC, RESPONSE_CONTEXT, FORMAT, ARGS ) \
	CFObjectControlAsyncV( (SERVER), AirPlayReceiverServerGetDispatchQueue( SERVER ), AirPlayReceiverServerControl, \
		kCFObjectFlagAsync, (COMMAND), (QUALIFIER), (RESPONSE_QUEUE), (RESPONSE_FUNC), (RESPONSE_CONTEXT) (FORMAT), (ARGS) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerPostEvent
	@abstract	Post events to AirPlay stack to notify of platform changes

	@param	inServer	Server Identifier of the AirPlay Server 
	@param	inEvent	    Even being notified
	@param	inQualifier	Reserved
	@param	inParams	Reserved

	@result	kNoErr if successful or an error code indicating failure.

	@discussion	

	Platform should call this function to notify AirPlay stack of the following events:
    
	<pre>
	@textblock

              inEvent
              -------
        - kAirPlayEvent_PrefsChanged
             Notify AirPlay about change in accessory preference like password change, name change etc.

	@/textblock
	</pre>

*/
OSStatus AirPlayReceiverServerPostEvent( CFTypeRef inServer, CFStringRef inEvent, CFTypeRef inQualifier, CFDictionaryRef inParams );

#define AirPlayReceiverServerPostEvent( SERVER, EVENT, QUALIFIER, PARAMS ) \
	CFObjectControlAsync( (SERVER), AirPlayReceiverServerGetDispatchQueue( SERVER ), AirPlayReceiverServerControl, \
		kCFObjectFlagAsync, (EVENT), (QUALIFIER), (PARAMS), NULL, NULL, NULL )

#if 0
#pragma mark -
#pragma mark == Properties ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerCopyProperty
	@internal
	@abstract	Copies a property from the server.
*/
CF_RETURNS_RETAINED
CFTypeRef
	AirPlayReceiverServerCopyProperty( 
		CFTypeRef	inServer, 
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		OSStatus *	outErr );

// Convenience accessors.

#define AirPlayReceiverServerGetBoolean( SERVER, PROPERTY, QUALIFIER, OUT_ERR ) \
	CFObjectGetPropertyBooleanSync( (SERVER), NULL, AirPlayReceiverServerCopyProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (OUT_ERR) )

#define AirPlayReceiverServerGetCString( SERVER, PROPERTY, QUALIFIER, BUF, MAX_LEN, OUT_ERR ) \
	CFObjectGetPropertyCStringSync( (SERVER), NULL, AirPlayReceiverServerCopyProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (BUF), (MAX_LEN), (OUT_ERR) )

#define AirPlayReceiverServerGetDouble( SERVER, PROPERTY, QUALIFIER, OUT_ERR ) \
	CFObjectGetPropertyDoubleSync( (SERVER), NULL, AirPlayReceiverServerCopyProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (OUT_ERR) )

#define AirPlayReceiverServerGetInt64( SERVER, PROPERTY, QUALIFIER, OUT_ERR ) \
	CFObjectGetPropertyInt64Sync( (SERVER), NULL, AirPlayReceiverServerCopyProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (OUT_ERR) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerSetProperty
	@internal
	@abstract	Sets a property on the server.
*/
OSStatus
	AirPlayReceiverServerSetProperty( 
		CFTypeRef	inServer, // Must be AirPlayReceiverServerRef.
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		CFTypeRef	inValue );

// Convenience accessors.

#define AirPlayReceiverServerSetBoolean( SERVER, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyBoolean( (SERVER), NULL, AirPlayReceiverServerSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (VALUE) )
	
#define AirPlayReceiverServerSetBooleanAsync( SERVER, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyBoolean( (SERVER), AirPlayReceiverServerGetDispatchQueue( SERVER ), \
		AirPlayReceiverServerSetProperty, kCFObjectFlagAsync, (PROPERTY), (QUALIFIER), (VALUE) )

#define AirPlayReceiverServerSetCString( SERVER, PROPERTY, QUALIFIER, STR, LEN ) \
	CFObjectSetPropertyCString( (SERVER), NULL, AirPlayReceiverServerSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (STR), (LEN) )

#define AirPlayReceiverServerSetData( SERVER, PROPERTY, QUALIFIER, PTR, LEN ) \
	CFObjectSetPropertyData( (SERVER), NULL, AirPlayReceiverServerSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (PTR), (LEN) )

#define AirPlayReceiverServerSetDouble( SERVER, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyDouble( (SERVER), NULL, AirPlayReceiverServerSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (VALUE) )

#define AirPlayReceiverServerSetInt64( SERVER, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyInt64( (SERVER), NULL, AirPlayReceiverServerSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (VALUE) )

#define AirPlayReceiverServerSetPropertyF( SERVER, PROPERTY, QUALIFIER, FORMAT, ... ) \
	CFObjectSetPropertyF( (SERVER), NULL, AirPlayReceiverServerSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (FORMAT), __VA_ARGS__ )

#define AirPlayReceiverServerSetPropertyV( SERVER, PROPERTY, QUALIFIER, FORMAT, ARGS ) \
	CFObjectSetPropertyV( (SERVER), NULL, AirPlayReceiverServerSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (FORMAT), (ARGS) )

#if( AIRPLAY_DACP )
//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerSendDACPCommand
	@abstract	Sends a DACP command from the accessory to the controller.
	
	@param	inServer	Server Identifier of the AirPlay Server 
	@param	inCommand	C string DACP command to send. See DACPCommon.h for string constants.

	@result	kNoErr if successful or an error code indicating failure.

	@discussion	

	Platform should call this function to send DACP commands to the controlling device. 
    
	<pre>
	@textblock

        Action			DACP Command String					Comment
        ------			-------------------					-------
        Play			kDACPCommandStr_Play			Start playback of the current AirPlay session (only for separate play button).
        Stop			kDACPCommandStr_Stop			Stop playback of the current AirPlay session.
        Pause			kDACPCommandStr_Pause			Pause playback of the current AirPlay session (only for separate pause button).
        Pause Toggle		kDACPCommandStr_PlayPause		Toggle between play and pause of current AirPlay session (only for combined pause/play button).
        Next Item		kDACPCommandStr_NextItem		Go to the next song in the playlist.
        Previous Item		kDACPCommandStr_PrevItem		Go to the previous song in the playlist.
        Repeat Toggle		kDACPCommandStr_RepeatAdvance		Repeat Advance.
        Shuffle Toggle		kDACPCommandStr_ShuffleToggle		Shuffle Toggle.
        Volume Up		kDACPCommandStr_VolumeUp		Raise the volume of the current AirPlay session.
        Volume Down		kDACPCommandStr_VolumeDown		Lower the volume of the current AirPlay session.
        Set Propety		kDACPCommandStr_SetProperty		Set the following properties:
        		Volume Level		kDACPProperty_DeviceVolume		Set the volume to a specific level
	@/textblock
	</pre>

    - Accessory should call the following when the user unmutes the accessory:
	<pre>
	@textblock
        AirPlayReceiverServerSendDACPCommand(kDACPCommandStr_SetProperty kDACPProperty_DeviceVolume "=-144.0")
	@/textblock
	</pre>

    Note that the accessory should also remember the volume level before the mute, so that volume can be restored to that level when
    user unmutes in the future.

    - Accessory should call the following when the user unmutes the accessory:
	<pre>
	@textblock
        AirPlayReceiverServerSendDACPCommand(kDACPCommandStr_SetProperty kDACPProperty_DeviceVolume "=<previous volume level>")
	@/textblock
	</pre>

    - Accessory should call the following to set volume to a specific level.
	<pre>
	@textblock
        AirPlayReceiverServerSendDACPCommand(kDACPCommandStr_SetProperty kDACPProperty_DeviceVolume "=<value>")
	@/textblock
	</pre>

    Note that volume level mentioned above is a floating-point dB attenuation value, where 0.0 is full volume and -144.0 is completely
    muted.  The practical volume range utilized for the AirPlay stream has a basis of -30dB to 0dB with a special cased mute level of
    -144dB (linear volume of 0) in order to avoid infinities.  The following equations can be used to convert between a dB value and a
    linear volume:

	<pre>
	@textblock
        linear volume = pow( 10, dB / 20 )
        dB value      = 20 * log10( linear volume )
	@/textblock
	</pre>


    - Accessory should do the following when switching away from an active AirPlay session, to ensure that the sender
    device is notified of the audio input switching away from AirPlay.
	<pre>
	@textblock
        AirPlayReceiverServerSendDACPCommand(kDACPCommandStr_SetProperty kDACPProperty_DevicePreventPlayback "=1")
        AirPlayReceiverServerSendDACPCommand(kDACPCommandStr_SetProperty kDACPProperty_DeviceBusy "=1")
        AirPlayReceiverServerSendDACPCommand(kDACPCommandStr_SetProperty kDACPProperty_DevicePreventPlayback "=0")
	@/textblock
	</pre>


    - Accessory should do the following when switching into AirPlay input when AirPlay session is inactive, to ensure that the sender
    device is notified of the audio input switching to AirPlay.
	<pre>
	@textblock
        AirPlayReceiverServerSendDACPCommand(kDACPCommandStr_SetProperty kDACPProperty_DeviceBusy "=0")
	@/textblock
	</pre>


*/
OSStatus AirPlayReceiverServerSendDACPCommand( CFTypeRef inServer, CFStringRef inCommand );

#define AirPlayReceiverServerSendDACPCommand( SERVER, COMMAND ) \
	AirPlayReceiverServerControlF( (SERVER), CFSTR( kAirPlayCommand_SendDACP ), NULL, NULL, \
		"{%kO=%s}", CFSTR( kAirPlayKey_DACPCommand ), (COMMAND) )
#endif

#if 0
#pragma mark -
#pragma mark == Platform ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerPlatformInitialize
	@internal
	@abstract	Initializes the platform-specific aspects of the server. Called once when the server is created.
*/
OSStatus	AirPlayReceiverServerPlatformInitialize( AirPlayReceiverServerRef inServer );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerPlatformFinalize
	@internal
	@abstract	Finalizes the platform-specific aspects of the server. Called once when the server is released.
*/
void	AirPlayReceiverServerPlatformFinalize( AirPlayReceiverServerRef inServer );

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerPlatformControl
	@internal
	@abstract	Controls the platform-specific aspects of the server.
*/
OSStatus
	AirPlayReceiverServerPlatformControl( 
		CFTypeRef			inServer, // Must be AirPlayReceiverServerRef.
		uint32_t			inFlags, 
		CFStringRef			inCommand, 
		CFTypeRef			inQualifier, 
		CFDictionaryRef		inParams, 
		CFDictionaryRef *	outParams );

// Convenience accessors.

#define AirPlayReceiverServerPlatformControlF( SERVER, COMMAND, QUALIFIER, OUT_PARAMS, FORMAT, ... ) \
	CFObjectControlSyncF( (SERVER), NULL, AirPlayReceiverServerPlatformControl, kCFObjectFlagDirect, \
		(COMMAND), (QUALIFIER), (OUT_PARAMS), (FORMAT), __VA_ARGS__ )

#define AirPlayReceiverServerPlatformControlV( SERVER, COMMAND, QUALIFIER, OUT_PARAMS, FORMAT, ARGS ) \
	CFObjectControlSyncF( (SERVER), NULL, AirPlayReceiverServerPlatformControl, kCFObjectFlagDirect, \
		(COMMAND), (QUALIFIER), (OUT_PARAMS), (FORMAT), (ARGS) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerPlatformCopyProperty
	@internal
	@abstract	Copies/gets a platform-specific property from the server.
*/
CF_RETURNS_RETAINED
CFTypeRef
	AirPlayReceiverServerPlatformCopyProperty( 
		CFTypeRef	inServer, // Must be AirPlayReceiverServerRef.
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		OSStatus *	outErr );

// Convenience accessors.

#define AirPlayReceiverServerPlatformGetBoolean( SERVER, PROPERTY, QUALIFIER, OUT_ERR ) \
	CFObjectGetPropertyBooleanSync( (SERVER), NULL, AirPlayReceiverServerPlatformCopyProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (OUT_ERR) )

#define AirPlayReceiverServerPlatformGetCString( SERVER, PROPERTY, QUALIFIER, BUF, MAX_LEN, OUT_ERR ) \
	CFObjectGetPropertyCStringSync( (SERVER), NULL, AirPlayReceiverServerPlatformCopyProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (BUF), (MAX_LEN), (OUT_ERR) )

#define AirPlayReceiverServerPlatformGetDouble( SERVER, PROPERTY, QUALIFIER, OUT_ERR ) \
	CFObjectGetPropertyDoubleSync( (SERVER), NULL, AirPlayReceiverServerPlatformCopyProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (OUT_ERR) )

#define AirPlayReceiverServerPlatformGetInt64( SERVER, PROPERTY, QUALIFIER, OUT_ERR ) \
	CFObjectGetPropertyInt64Sync( (SERVER), NULL, AirPlayReceiverServerPlatformCopyProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (OUT_ERR) )

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerPlatformSetProperty
	@internal
	@abstract	Sets a platform-specific property on the server.
*/
OSStatus
	AirPlayReceiverServerPlatformSetProperty( 
		CFTypeRef	inServer, // Must be AirPlayReceiverServerRef.
		uint32_t	inFlags, 
		CFStringRef	inProperty, 
		CFTypeRef	inQualifier, 
		CFTypeRef	inValue );

// Convenience accessors.

#define AirPlayReceiverServerPlatformSetBoolean( SERVER, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyBoolean( (SERVER), NULL, AirPlayReceiverServerPlatformSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (VALUE) )

#define AirPlayReceiverServerPlatformSetCString( SERVER, PROPERTY, QUALIFIER, STR, LEN ) \
	CFObjectSetPropertyCString( (SERVER), NULL, AirPlayReceiverServerPlatformSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (STR), (LEN) )

#define AirPlayReceiverServerPlatformSetDouble( SERVER, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyDouble( (SERVER), NULL, AirPlayReceiverServerPlatformSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (VALUE) )

#define AirPlayReceiverServerPlatformSetInt64( SERVER, PROPERTY, QUALIFIER, VALUE ) \
	CFObjectSetPropertyInt64( (SERVER), NULL, AirPlayReceiverServerPlatformSetProperty, kCFObjectFlagDirect, \
		(PROPERTY), (QUALIFIER), (VALUE) )


#ifdef __cplusplus
}
#endif

#endif // __AirPlayReceiverServer_h__
