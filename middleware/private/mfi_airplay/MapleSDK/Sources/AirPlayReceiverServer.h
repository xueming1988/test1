/*
	Copyright (C) 2012-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef __AirPlayReceiverServer_h__
#define __AirPlayReceiverServer_h__

#include "AirPlayCommon.h"
#include "CFUtils.h"
#include "CommonServices.h"
#include "DebugServices.h"
#include "PairingUtils.h"

#include CF_HEADER
#include LIBDISPATCH_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark == Creation ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerCreate
	@abstract	Creates the server and initializes the server. Caller must CFRelease it when done.
*/
typedef struct AirPlayReceiverServerPrivate* AirPlayReceiverServerRef;
typedef struct AirPlayReceiverSessionPrivate* AirPlayReceiverSessionRef;
typedef struct AirPlayReceiverConnectionPrivate* AirPlayReceiverConnectionRef;

CFTypeID AirPlayReceiverServerGetTypeID(void);
OSStatus AirPlayReceiverServerCreate(AirPlayReceiverServerRef* outServer);
OSStatus AirPlayReceiverServerCreateExt(CFTypeRef inDeviceID, CFTypeRef inAudioHint, uint64_t inPort, AirPlayReceiverServerRef* outServer);
OSStatus AirPlayReceiverServerCreateWithConfigFilePath(CFStringRef inConfigFilePath, AirPlayReceiverServerRef* outServer);
dispatch_queue_t AirPlayReceiverServerGetDispatchQueue(AirPlayReceiverServerRef inServer);

#if 0
#pragma mark -
#pragma mark == Delegate ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		AirPlayReceiverServerDelegate
	@abstract	Allows functionality to be delegated to external code.
*/
typedef OSStatus (*AirPlayReceiverServerControl_f)(
    AirPlayReceiverServerRef inServer,
    CFStringRef inCommand,
    CFTypeRef inQualifier,
    CFDictionaryRef inParams,
    CFDictionaryRef* outParams,
    void* inContext);

typedef CFTypeRef (*AirPlayReceiverServerCopyProperty_f)(
    AirPlayReceiverServerRef inServer,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    OSStatus* outErr,
    void* inContext);

typedef OSStatus (*AirPlayReceiverServerSetProperty_f)(
    AirPlayReceiverServerRef inServer,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    CFTypeRef inValue,
    void* inContext);

typedef void (*AirPlayReceiverSessionCreated_f)(
    AirPlayReceiverServerRef inServer,
    AirPlayReceiverSessionRef inSession,
    void* inContext);

typedef void (*AirPlayReceiverSessionFailed_f)(
    AirPlayReceiverServerRef inServer,
    OSStatus inReason,
    void* inContext);

typedef struct
{
    void* context; // Context pointer for the delegate to use.
    void* context2; // Extra context pointer for the delegate to use.
    AirPlayReceiverServerControl_f control_f; // Function to call for control requests.
    AirPlayReceiverServerCopyProperty_f copyProperty_f; // Function to call for copyProperty requests.
    AirPlayReceiverServerSetProperty_f setProperty_f; // Function to call for setProperty requests.
    AirPlayReceiverSessionCreated_f sessionCreated_f; // Function to call when a session is created.
    AirPlayReceiverSessionFailed_f sessionFailed_f; // Function to call when a session creation fails.
} AirPlayReceiverServerDelegate;

#define AirPlayReceiverServerDelegateInit(PTR) memset((PTR), 0, sizeof(AirPlayReceiverServerDelegate));

void AirPlayReceiverServerSetDelegate(AirPlayReceiverServerRef inServer, const AirPlayReceiverServerDelegate* inDelegate);

#if 0
#pragma mark -
#pragma mark == Control ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerControl
	@abstract	Controls the server.
*/
OSStatus
AirPlayReceiverServerControl(
    CFTypeRef inServer, // Must be AirPlayReceiverServerRef.
    uint32_t inFlags,
    CFStringRef inCommand,
    CFTypeRef inQualifier,
    CFDictionaryRef inParams,
    CFDictionaryRef* outParams);

// Convenience accessors.

#define AirPlayReceiverServerStart(SERVER) \
    AirPlayReceiverServerControl((SERVER), kCFObjectFlagDirect, CFSTR(kAirPlayCommand_StartServer), NULL, NULL, NULL)

#define AirPlayReceiverServerStop(SERVER) \
    AirPlayReceiverServerControl((SERVER), kCFObjectFlagDirect, CFSTR(kAirPlayCommand_StopServer), NULL, NULL, NULL)

#define AirPlayReceiverServerUpdateRegistration(SERVER) \
    AirPlayReceiverServerControl((SERVER), kCFObjectFlagDirect, CFSTR(kAirPlayCommand_ResetBonjour), NULL, NULL, NULL)

// Use AirPlayReceiverServerUpdateRegistration(). This is for
// backwards compatability with SDK 2.0.0 - 2.0.2.
#define AirPlayReceiverServerUpdateRegisteration AirPlayReceiverServerUpdateRegistration

#define AirPlayReceiverServerControlAsync(SERVER, COMMAND, QUALIFIER, PARAMS, RESPONSE_QUEUE, RESPONSE_FUNC, RESPONSE_CONTEXT) \
    CFObjectControlAsync((SERVER), AirPlayReceiverServerGetDispatchQueue(SERVER), AirPlayReceiverServerControl,                \
        kCFObjectFlagAsync, (COMMAND), (QUALIFIER), (PARAMS), (RESPONSE_QUEUE), (RESPONSE_FUNC), (RESPONSE_CONTEXT))

#if 0
#pragma mark -
#pragma mark == Properties ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerCopyProperty
	@abstract	Copies a property from the server.
*/
CF_RETURNS_RETAINED
CFTypeRef
AirPlayReceiverServerCopyProperty(
    CFTypeRef inServer,
    uint32_t inFlags,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    OSStatus* outErr);

// Convenience accessors.

#define AirPlayReceiverServerGetBoolean(SERVER, PROPERTY, QUALIFIER, OUT_ERR)                              \
    CFObjectGetPropertyBooleanSync((SERVER), NULL, AirPlayReceiverServerCopyProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (OUT_ERR))

#define AirPlayReceiverServerGetCString(SERVER, PROPERTY, QUALIFIER, BUF, MAX_LEN, OUT_ERR)                \
    CFObjectGetPropertyCStringSync((SERVER), NULL, AirPlayReceiverServerCopyProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (BUF), (MAX_LEN), (OUT_ERR))

#define AirPlayReceiverServerGetDouble(SERVER, PROPERTY, QUALIFIER, OUT_ERR)                              \
    CFObjectGetPropertyDoubleSync((SERVER), NULL, AirPlayReceiverServerCopyProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (OUT_ERR))

#define AirPlayReceiverServerGetInt64(SERVER, PROPERTY, QUALIFIER, OUT_ERR)                              \
    CFObjectGetPropertyInt64Sync((SERVER), NULL, AirPlayReceiverServerCopyProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (OUT_ERR))

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerSetProperty
	@abstract	Sets a property on the server.
*/
OSStatus
AirPlayReceiverServerSetProperty(
    CFTypeRef inServer, // Must be AirPlayReceiverServerRef.
    uint32_t inFlags,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    CFTypeRef inValue);

// Convenience accessors.

#define AirPlayReceiverServerSetBoolean(SERVER, PROPERTY, QUALIFIER, VALUE)                           \
    CFObjectSetPropertyBoolean((SERVER), NULL, AirPlayReceiverServerSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (VALUE))

#define AirPlayReceiverServerSetBooleanAsync(SERVER, PROPERTY, QUALIFIER, VALUE)        \
    CFObjectSetPropertyBoolean((SERVER), AirPlayReceiverServerGetDispatchQueue(SERVER), \
        AirPlayReceiverServerSetProperty, kCFObjectFlagAsync, (PROPERTY), (QUALIFIER), (VALUE))

#define AirPlayReceiverServerSetCString(SERVER, PROPERTY, QUALIFIER, STR, LEN)                        \
    CFObjectSetPropertyCString((SERVER), NULL, AirPlayReceiverServerSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (STR), (LEN))

#define AirPlayReceiverServerSetData(SERVER, PROPERTY, QUALIFIER, PTR, LEN)                        \
    CFObjectSetPropertyData((SERVER), NULL, AirPlayReceiverServerSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (PTR), (LEN))

#define AirPlayReceiverServerSetDouble(SERVER, PROPERTY, QUALIFIER, VALUE)                           \
    CFObjectSetPropertyDouble((SERVER), NULL, AirPlayReceiverServerSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (VALUE))

#define AirPlayReceiverServerSetInt64(SERVER, PROPERTY, QUALIFIER, VALUE)                           \
    CFObjectSetPropertyInt64((SERVER), NULL, AirPlayReceiverServerSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (VALUE))

#define AirPlayReceiverServerSetPropertyF(SERVER, PROPERTY, QUALIFIER, FORMAT, ...)             \
    CFObjectSetPropertyF((SERVER), NULL, AirPlayReceiverServerSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (FORMAT), __VA_ARGS__)

#define AirPlayReceiverServerSetPropertyV(SERVER, PROPERTY, QUALIFIER, FORMAT, ARGS)            \
    CFObjectSetPropertyV((SERVER), NULL, AirPlayReceiverServerSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (FORMAT), (ARGS))

#if 0
#pragma mark -
#pragma mark == Platform ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerPlatformInitialize
	@abstract	Initializes the platform-specific aspects of the server. Called once when the server is created.
*/
OSStatus AirPlayReceiverServerPlatformInitialize(AirPlayReceiverServerRef inServer);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerPlatformFinalize
	@abstract	Finalizes the platform-specific aspects of the server. Called once when the server is released.
*/
void AirPlayReceiverServerPlatformFinalize(AirPlayReceiverServerRef inServer);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerPlatformControl
	@abstract	Controls the platform-specific aspects of the server.
*/
OSStatus
AirPlayReceiverServerPlatformControl(
    CFTypeRef inServer, // Must be AirPlayReceiverServerRef.
    uint32_t inFlags,
    CFStringRef inCommand,
    CFTypeRef inQualifier,
    CFDictionaryRef inParams,
    CFDictionaryRef* outParams);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerPlatformCopyProperty
	@abstract	Copies/gets a platform-specific property from the server.
*/
CF_RETURNS_RETAINED
CFTypeRef
AirPlayReceiverServerPlatformCopyProperty(
    CFTypeRef inServer, // Must be AirPlayReceiverServerRef.
    uint32_t inFlags,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    OSStatus* outErr);

// Convenience accessors.

#define AirPlayReceiverServerPlatformGetBoolean(SERVER, PROPERTY, QUALIFIER, OUT_ERR)                              \
    CFObjectGetPropertyBooleanSync((SERVER), NULL, AirPlayReceiverServerPlatformCopyProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (OUT_ERR))

#define AirPlayReceiverServerPlatformGetCString(SERVER, PROPERTY, QUALIFIER, BUF, MAX_LEN, OUT_ERR)                \
    CFObjectGetPropertyCStringSync((SERVER), NULL, AirPlayReceiverServerPlatformCopyProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (BUF), (MAX_LEN), (OUT_ERR))

#define AirPlayReceiverServerPlatformGetDouble(SERVER, PROPERTY, QUALIFIER, OUT_ERR)                              \
    CFObjectGetPropertyDoubleSync((SERVER), NULL, AirPlayReceiverServerPlatformCopyProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (OUT_ERR))

#define AirPlayReceiverServerPlatformGetInt64(SERVER, PROPERTY, QUALIFIER, OUT_ERR)                              \
    CFObjectGetPropertyInt64Sync((SERVER), NULL, AirPlayReceiverServerPlatformCopyProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (OUT_ERR))

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerPlatformSetProperty
	@abstract	Sets a platform-specific property on the server.
*/
OSStatus
AirPlayReceiverServerPlatformSetProperty(
    CFTypeRef inServer, // Must be AirPlayReceiverServerRef.
    uint32_t inFlags,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    CFTypeRef inValue);

// Convenience accessors.

#define AirPlayReceiverServerPlatformSetBoolean(SERVER, PROPERTY, QUALIFIER, VALUE)                           \
    CFObjectSetPropertyBoolean((SERVER), NULL, AirPlayReceiverServerPlatformSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (VALUE))

#define AirPlayReceiverServerPlatformSetCString(SERVER, PROPERTY, QUALIFIER, STR, LEN)                        \
    CFObjectSetPropertyCString((SERVER), NULL, AirPlayReceiverServerPlatformSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (STR), (LEN))

#define AirPlayReceiverServerPlatformSetDouble(SERVER, PROPERTY, QUALIFIER, VALUE)                           \
    CFObjectSetPropertyDouble((SERVER), NULL, AirPlayReceiverServerPlatformSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (VALUE))

#define AirPlayReceiverServerPlatformSetInt64(SERVER, PROPERTY, QUALIFIER, VALUE)                           \
    CFObjectSetPropertyInt64((SERVER), NULL, AirPlayReceiverServerPlatformSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (VALUE))

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerStopAllAudioConnections
	@abstract	Stops all Audio-capable HTTP connections.
*/
void AirPlayReceiverServerStopAllAudioConnections(AirPlayReceiverServerRef inServer);

void AirPlayReceiverServerHijackConnection(
    AirPlayReceiverServerRef inReceiverServer,
    CFTypeRef inActiveSession,
    AirPlayReceiverSessionRef inHijackerSession);

OSStatus AirPlayReceiverServerSendMediaCommandWithOptions(
    AirPlayReceiverServerRef inReceiverServer,
    CFStringRef inCommand,
    CFDictionaryRef inParams);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverServerIsHomeAccessControlEnabled
	@abstract	Returns true if the accessory has been added to a Home.
 */
Boolean AirPlayReceiverServerIsHomeAccessControlEnabled(
    AirPlayReceiverServerRef inReceiverServer);

#ifdef __cplusplus
}
#endif

#endif // __AirPlayReceiverServer_h__
