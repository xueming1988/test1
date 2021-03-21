/*
	Copyright (C) 2007-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef __AirPlayReceiverSession_h__
#define __AirPlayReceiverSession_h__

#include "AirPlayCommon.h"
#include "AirPlayReceiverServer.h"
#include "CFUtils.h"
#include "CommonServices.h"
#include "NetUtils.h"
#include "PairingUtils.h"

#include CF_HEADER
#include LIBDISPATCH_HEADER

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark -
#pragma mark == Creation ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionCreate
	@abstract	Creates an AirPlay receiver session.
*/
typedef struct
{
    AirPlayReceiverServerRef server; // Server managing the session.
    NetTransportType transportType; // Network transport type for the session.
    const sockaddr_ip* peerAddr; // sockaddr of the server.
    uint64_t clientDeviceID; // Unique device ID of the client sending to us.
    uint8_t clientIfMACAddr[6]; // Client's MAC address of the interface this session is connected on.
    uint64_t clientSessionID; // Unique session ID from the client.
    uint32_t clientVersion; // Source version of the client or 0 if unknown.
    char ifName[IF_NAMESIZE + 1]; // Name of the interface the connection was accepted on.
    Boolean useEvents; // True if the client supports events.
    AirPlayReceiverConnectionRef connection; // Control connection for the session
    Boolean isRemoteControlSession;

} AirPlayReceiverSessionCreateParams;

CFTypeID AirPlayReceiverSessionGetTypeID(void);
OSStatus
AirPlayReceiverSessionCreate(
    AirPlayReceiverSessionRef* outSession,
    const AirPlayReceiverSessionCreateParams* inParams);

dispatch_queue_t AirPlayReceiverSessionGetDispatchQueue(AirPlayReceiverSessionRef inSession);

OSStatus
AirPlayReceiverSessionSetSecurityInfo(
    AirPlayReceiverSessionRef inSession,
    const uint8_t inKey[16],
    const uint8_t inIV[16]);

OSStatus
AirPlayReceiverSessionSetHomeKitSecurityContext(
    AirPlayReceiverSessionRef inSession,
    PairingSessionRef inHKPairVerifySession);

#if 0
#pragma mark -
#pragma mark == Delegate ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		AirPlayReceiverSessionDelegate
	@abstract	Allows functionality to be delegated to external code.
*/
typedef OSStatus (*AirPlayReceiverSessionInitialize_f)(AirPlayReceiverSessionRef inSession, void* inContext);
typedef void (*AirPlayReceiverSessionFinalize_f)(AirPlayReceiverSessionRef inSession, void* inContext);
typedef void (*AirPlayReceiverSessionStarted_f)(AirPlayReceiverSessionRef inSession, void* inContext);

typedef OSStatus (*AirPlayReceiverSessionControl_f)(
    AirPlayReceiverSessionRef inSession,
    CFStringRef inCommand,
    CFTypeRef inQualifier,
    CFDictionaryRef inParams,
    CFDictionaryRef* outParams,
    void* inContext);

typedef CFTypeRef (*AirPlayReceiverSessionCopyProperty_f)(
    AirPlayReceiverSessionRef inSession,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    OSStatus* outErr,
    void* inContext);

typedef OSStatus (*AirPlayReceiverSessionSetProperty_f)(
    AirPlayReceiverSessionRef inSession,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    CFTypeRef inValue,
    void* inContext);

typedef void (*AirPlayReceiverSessionModesChanged_f)(
    AirPlayReceiverSessionRef inSession,
    const AirPlayModeState* inState,
    void* inContext);

typedef void (*AirPlayReceiverSessionRequestUI_f)(
    AirPlayReceiverSessionRef inSession,
    CFStringRef inURL,
    void* inContext);

typedef void (*AirPlayReceiverSessionDuckAudio_f)(
    AirPlayReceiverSessionRef inSession,
    double inDurationSecs,
    double inVolume,
    void* inContext);

typedef void (*AirPlayReceiverSessionUnduckAudio_f)(
    AirPlayReceiverSessionRef inSession,
    double inDurationSecs,
    void* inContext);

typedef struct
{
    void* context; // Context pointer for the delegate to use.
    AirPlayReceiverSessionInitialize_f initialize_f;
    AirPlayReceiverSessionFinalize_f finalize_f;
    AirPlayReceiverSessionStarted_f started_f;
    AirPlayReceiverSessionControl_f control_f; // Function to call for control requests.
    AirPlayReceiverSessionCopyProperty_f copyProperty_f; // Function to call for copyProperty requests.
    AirPlayReceiverSessionSetProperty_f setProperty_f; // Function to call for setProperty requests.
    AirPlayReceiverSessionModesChanged_f modesChanged_f; // Function to call when the controller changed the accessory modes.
    AirPlayReceiverSessionRequestUI_f requestUI_f; // Function to call when the controller requests accessory UI.
    AirPlayReceiverSessionDuckAudio_f duckAudio_f; // Function to call when the controller requests audio ducking and the session does not own the audio context.
    AirPlayReceiverSessionUnduckAudio_f unduckAudio_f; // Function to call when the controller requests audio ducking and the session does not own the audio context.

} AirPlayReceiverSessionDelegate;

#define AirPlayReceiverSessionDelegateInit(PTR) memset((PTR), 0, sizeof(AirPlayReceiverSessionDelegate));

void AirPlayReceiverSessionSetDelegate(AirPlayReceiverSessionRef inSession, const AirPlayReceiverSessionDelegate* inDelegate);

#if 0
#pragma mark -
#pragma mark == Control ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionControl
	@abstract	Controls the session.
*/
OSStatus
AirPlayReceiverSessionControl(
    CFTypeRef inSession, // Must be a AirPlayReceiverSessionRef.
    uint32_t inFlags,
    CFStringRef inCommand,
    CFTypeRef inQualifier,
    CFDictionaryRef inParams,
    CFDictionaryRef* outParams);

#if 0
#pragma mark -
#pragma mark == Properties ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionCopyProperty
	@abstract	Copies a property from the session.
*/
CF_RETURNS_RETAINED
CFTypeRef
AirPlayReceiverSessionCopyProperty(
    CFTypeRef inSession, // Must be AirPlayReceiverSessionRef.
    uint32_t inFlags,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    OSStatus* outErr);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionSetProperty
	@abstract	Sets a property on the session.
*/
OSStatus
AirPlayReceiverSessionSetProperty(
    CFTypeRef inSession, // Must be AirPlayReceiverSessionRef.
    uint32_t inFlags,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    CFTypeRef inValue);

// Convenience accessors.

#define AirPlayReceiverSessionSetBoolean(SESSION, PROPERTY, QUALIFIER, VALUE)                           \
    CFObjectSetPropertyBoolean((SESSION), NULL, AirPlayReceiverSessionSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (VALUE))

#define AirPlayReceiverSessionSetCString(SESSION, PROPERTY, QUALIFIER, STR, LEN)                        \
    CFObjectSetPropertyCString((SESSION), NULL, AirPlayReceiverSessionSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (STR), (LEN))

#define AirPlayReceiverSessionSetData(SESSION, PROPERTY, QUALIFIER, PTR, LEN)                        \
    CFObjectSetPropertyData((SESSION), NULL, AirPlayReceiverSessionSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (PTR), (LEN))

#define AirPlayReceiverSessionSetDouble(SESSION, PROPERTY, QUALIFIER, VALUE)                           \
    CFObjectSetPropertyDouble((SESSION), NULL, AirPlayReceiverSessionSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (VALUE))

#define AirPlayReceiverSessionSetInt64(SESSION, PROPERTY, QUALIFIER, VALUE)                           \
    CFObjectSetPropertyInt64((SESSION), NULL, AirPlayReceiverSessionSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (VALUE))

#define AirPlayReceiverSessionSetPropertyF(SESSION, PROPERTY, QUALIFIER, FORMAT, ...)             \
    CFObjectSetPropertyF((SESSION), NULL, AirPlayReceiverSessionSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (FORMAT), __VA_ARGS__)

#define AirPlayReceiverSessionSetPropertyV(SESSION, PROPERTY, QUALIFIER, FORMAT, ARGS)            \
    CFObjectSetPropertyV((SESSION), NULL, AirPlayReceiverSessionSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (FORMAT), (ARGS))

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionSetup
	@abstract	Sets up an AirPlay receiver session.
	
	@params		inRequestParams		Input parameters used to configure or update the session.
	@param		outResponseParams	Output parameters to return to the client so it can configure/update its side of the session.
*/
OSStatus
AirPlayReceiverSessionSetup(
    AirPlayReceiverSessionRef inSession,
    CFDictionaryRef inRequestParams,
    CFDictionaryRef* outResponseParams);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionTearDown
	@abstract	Sets up an AirPlay receiver session.
	
	@param		inSession	Session to apply tear down.
	@param		inRequest	Request describing streams to tear down. May be NULL if it was an unexpected tear down.
	@param		inReason	Reason for tearing down. kNoErr means a clean teardown. Other errors mean an unexpected teardown.
	@param		outDone		If non-NULL, receives true if session is done and can be deleted (or false to keep it alive).
*/
void AirPlayReceiverSessionTearDown(
    AirPlayReceiverSessionRef inSession,
    CFDictionaryRef inRequest,
    OSStatus inReason,
    Boolean* outDone);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionStart
	@abstract	Starts the session after it has been set up.
*/
typedef struct
{
    const char* clientName;
    NetTransportType transportType;
    uint32_t bonjourMs;
    uint32_t connectMs;
    uint32_t authMs;
    uint32_t announceMs;
    uint32_t setupAudioMs;
    uint32_t setupScreenMs;
    uint32_t recordMs;

} AirPlayReceiverSessionStartInfo;

OSStatus
AirPlayReceiverSessionStart(
    AirPlayReceiverSessionRef inSession,
    AirPlayReceiverSessionStartInfo* inInfo);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionSendCommand
	@abstract	Sends a command to the controller and calls a completion function with the response.
*/
typedef void (*AirPlayReceiverSessionCommandCompletionFunc)(OSStatus inStatus, CFDictionaryRef inResponse, void* inContext);

OSStatus
AirPlayReceiverSessionSendCommand(
    AirPlayReceiverSessionRef inSession,
    CFDictionaryRef inRequest,
    AirPlayReceiverSessionCommandCompletionFunc inCompletion,
    void* inContext);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionFlushAudio
	@abstract	Flush any queued audio until the specified timestamp or sequence number.
*/

typedef struct {
    uint32_t seq;
    uint32_t ts;
} AirPlayFlushPoint;

OSStatus
AirPlayReceiverSessionFlushAudio(
    AirPlayReceiverSessionRef inSession,
    AirPlayFlushPoint const* flushFrom,
    AirPlayFlushPoint flushUntil,
    uint32_t* outLastTS);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionReadAudio
	@abstract	Reads output audio (e.g. music to play on the accessory) that has been received by the accessory.
	
	@param		inSession		Session to read audio from.
	@param		inType			Audio stream to read from.
	@param		inSampleTime	Audio sample time for the first sample to be placed in the buffer.
	@param		inHostTime		Wall clock time for when the first sample in the buffer should be heard.
	@param		inBuffer		Buffer to receive the audio samples. The format is configured during stream setup.
	@param		inLen			Number of bytes to fill into the buffer.
	
	@discussion
	
	The flow of operation is that audio sent to the accessory for playback is received, de-packetized, decrypted, decoded, 
	and buffered according to timing information in the stream. When the platform's audio stack needs the next chunk of 
	audio to play, it calls this function to read the audio that has been buffered for the specified timestamp. If there 
	isn't enough audio buffered to satisfy this request, the missing data will be filled in with a best guess or silence. 
	The platform can then provide this audio data to its audio stack for playback.
*/
OSStatus
AirPlayReceiverSessionReadAudio(
    AirPlayReceiverSessionRef inSession,
    AirPlayStreamType inType,
    uint32_t inSampleTime,
    uint64_t inHostTime,
    void* inBuffer,
    size_t inLen);

OSStatus
AirPlayReceiverSessionReadAudio2(
    AirPlayReceiverSessionRef inSession,
    AirPlayStreamType inType,
    uint32_t inSampleTime,
    uint64_t inHostTime,
    void* inBuffer,
    size_t inLen,
    size_t inMinLen,
    size_t* outSuppliedLen);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionWriteAudio
	@abstract	Writes input audio (e.g. from the accessory's microphone) to be sent from the accessory.
	
	@param		inSession		Session to write audio to.
	@param		inType			Audio stream to write to.
	@param		inSampleTime	Audio sample time for the first sample in the buffer.
	@param		inHostTime		Wall clock time for the first sample in the buffer.
	@param		inBuffer		Buffer of audio samples to write. The format is configured during stream setup.
	@param		inLen			Number of bytes to write.
	
	@discussion
	
	The flow of operation is that when the platform's audio stack provides audio (e.g. from a microphone on the accessory), 
	the platform calls this function to encode, encrypt, packetize, and send over the network.
*/
OSStatus
AirPlayReceiverSessionWriteAudio(
    AirPlayReceiverSessionRef inSession,
    AirPlayStreamType inType,
    uint32_t inSampleTime,
    uint64_t inHostTime,
    const void* inBuffer,
    size_t inLen);

#if 0
#pragma mark -
#pragma mark == Platform ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionPlatformInitialize
	@abstract	Initializes the platform-specific aspects of the session. Called once per session after it is created.
	@discussion
	
	This gives the platform a chance to set up any per-session state.
	AirPlayReceiverSessionPlatformFinalize will be called when the session ends.
*/
OSStatus AirPlayReceiverSessionPlatformInitialize(AirPlayReceiverSessionRef inSession);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverPlatformFinalize
	@abstract	Finalizes the platform-specific aspects of the session. Called once per session before it is deleted.
	@discussion
	
	This gives the platform a chance to clean up any per-session state. This must handle being called during a partial 
	initialization (e.g. failure mid-way through initialization). In certain situations, this may be called without 
	of a TearDownStreams control request so it needs to clean up all per-session state.
*/
void AirPlayReceiverSessionPlatformFinalize(AirPlayReceiverSessionRef inSession);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionPlatformControl
	@abstract	Controls the platform-specific aspects of the session.
*/
OSStatus
AirPlayReceiverSessionPlatformControl(
    CFTypeRef inSession, // Must be a AirPlayReceiverSessionRef.
    uint32_t inFlags,
    CFStringRef inCommand,
    CFTypeRef inQualifier,
    CFDictionaryRef inParams,
    CFDictionaryRef* outParams);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionPlatformCopyProperty
	@abstract	Copies a platform-specific property from the session.
*/
CF_RETURNS_RETAINED
CFTypeRef
AirPlayReceiverSessionPlatformCopyProperty(
    CFTypeRef inSession, // Must be a AirPlayReceiverSessionRef.
    uint32_t inFlags,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    OSStatus* outErr);

// Convenience accessors.

#define AirPlayReceiverSessionPlatformGetBoolean(SESSION, PROPERTY, QUALIFIER, OUT_ERR)                              \
    CFObjectGetPropertyBooleanSync((SESSION), NULL, AirPlayReceiverSessionPlatformCopyProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (OUT_ERR))

#define AirPlayReceiverSessionPlatformGetCString(SESSION, PROPERTY, QUALIFIER, BUF, MAX_LEN, OUT_ERR)                \
    CFObjectGetPropertyCStringSync((SESSION), NULL, AirPlayReceiverSessionPlatformCopyProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (BUF), (MAX_LEN), (OUT_ERR))

#define AirPlayReceiverSessionPlatformGetDouble(SESSION, PROPERTY, QUALIFIER, OUT_ERR)                              \
    CFObjectGetPropertyDoubleSync((SESSION), NULL, AirPlayReceiverSessionPlatformCopyProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (OUT_ERR))

#define AirPlayReceiverSessionPlatformGetInt64(SESSION, PROPERTY, QUALIFIER, OUT_ERR)                              \
    CFObjectGetPropertyInt64Sync((SESSION), NULL, AirPlayReceiverSessionPlatformCopyProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (OUT_ERR))

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionPlatformSetProperty
	@abstract	Sets a platform-specific property on the session.
*/
OSStatus
AirPlayReceiverSessionPlatformSetProperty(
    CFTypeRef inSession, // Must be a AirPlayReceiverSessionRef.
    uint32_t inFlags,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    CFTypeRef inValue);

// Convenience accessors.

#define AirPlayReceiverSessionPlatformSetBoolean(SESSION, PROPERTY, QUALIFIER, VALUE)                           \
    CFObjectSetPropertyBoolean((SESSION), NULL, AirPlayReceiverSessionPlatformSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (VALUE))

#define AirPlayReceiverSessionPlatformSetCString(SESSION, PROPERTY, QUALIFIER, STR, LEN)                        \
    CFObjectSetPropertyCString((SESSION), NULL, AirPlayReceiverSessionPlatformSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (STR), (LEN))

#define AirPlayReceiverSessionPlatformSetDouble(SESSION, PROPERTY, QUALIFIER, VALUE)                           \
    CFObjectSetPropertyDouble((SESSION), NULL, AirPlayReceiverSessionPlatformSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (VALUE))

#define AirPlayReceiverSessionPlatformSetInt64(SESSION, PROPERTY, QUALIFIER, VALUE)                           \
    CFObjectSetPropertyInt64((SESSION), NULL, AirPlayReceiverSessionPlatformSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (VALUE))

#define AirPlayReceiverSessionPlatformSetPropertyF(SESSION, PROPERTY, QUALIFIER, FORMAT, ...)             \
    CFObjectSetPropertyF((SESSION), NULL, AirPlayReceiverSessionPlatformSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (FORMAT), __VA_ARGS__)

#define AirPlayReceiverSessionPlatformSetPropertyV(SESSION, PROPERTY, QUALIFIER, FORMAT, ARGS)            \
    CFObjectSetPropertyV((SESSION), NULL, AirPlayReceiverSessionPlatformSetProperty, kCFObjectFlagDirect, \
        (PROPERTY), (QUALIFIER), (FORMAT), (ARGS))

#if 0
#pragma mark -
#pragma mark == Helpers ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionChangeModes
	@abstract	Builds and sends a mode change request to the controller.
	
	@param		inSession		Session to request a mode change on.
	@param		inChanges		Changes being request. Initialize with AirPlayModeChangesInit() then set fields.
	@param		inReason		Optional reason for the change. Mainly for diagnostics. May be NULL.
	@param		inCompletion	Optional completion function to call when the request completes. May be NULL.
	@param		inContext		Optional context ptr to pass to completion function. May be NULL.
*/
OSStatus
AirPlayReceiverSessionChangeModes(
    AirPlayReceiverSessionRef inSession,
    const AirPlayModeChanges* inChanges,
    CFStringRef inReason,
    AirPlayReceiverSessionCommandCompletionFunc inCompletion,
    void* inContext);

// Screen

#define AirPlayReceiverSessionTakeScreen(SESSION, PRIORITY, TAKE_CONSTRAINT, BORROW_CONSTRAINT, REASON, COMPLETION, CONTEXT)  \
    AirPlayReceiverSessionChangeResourceMode((SESSION), kAirPlayResourceID_MainScreen, kAirPlayTransferType_Take, (PRIORITY), \
        (TAKE_CONSTRAINT), (BORROW_CONSTRAINT), (REASON), (COMPLETION), (CONTEXT))

#define AirPlayReceiverSessionUntakeScreen(SESSION, REASON, COMPLETION, CONTEXT)                                    \
    AirPlayReceiverSessionChangeResourceMode((SESSION), kAirPlayResourceID_MainScreen, kAirPlayTransferType_Untake, \
        kAirPlayTransferPriority_NotApplicable, kAirPlayConstraint_NotApplicable, kAirPlayConstraint_NotApplicable, \
        (REASON), (COMPLETION), (CONTEXT))

#define AirPlayReceiverSessionBorrowScreen(SESSION, PRIORITY, UNBORROW_CONSTRAINT, REASON, COMPLETION, CONTEXT)                 \
    AirPlayReceiverSessionChangeResourceMode((SESSION), kAirPlayResourceID_MainScreen, kAirPlayTransferType_Borrow, (PRIORITY), \
        kAirPlayConstraint_NotApplicable, (UNBORROW_CONSTRAINT), (REASON), (COMPLETION), (CONTEXT))

#define AirPlayReceiverSessionUnborrowScreen(SESSION, REASON, COMPLETION, CONTEXT)                                    \
    AirPlayReceiverSessionChangeResourceMode((SESSION), kAirPlayResourceID_MainScreen, kAirPlayTransferType_Unborrow, \
        kAirPlayTransferPriority_NotApplicable, kAirPlayConstraint_NotApplicable, kAirPlayConstraint_NotApplicable,   \
        (REASON), (COMPLETION), (CONTEXT))

// MainAudio

#define AirPlayReceiverSessionTakeMainAudio(SESSION, PRIORITY, TAKE_CONSTRAINT, BORROW_CONSTRAINT, REASON, COMPLETION, CONTEXT) \
    AirPlayReceiverSessionChangeResourceMode((SESSION), kAirPlayResourceID_MainAudio, kAirPlayTransferType_Take, (PRIORITY),    \
        (TAKE_CONSTRAINT), (BORROW_CONSTRAINT), (REASON), (COMPLETION), (CONTEXT))

#define AirPlayReceiverSessionUntakeMainAudio(SESSION, REASON, COMPLETION, CONTEXT)                                 \
    AirPlayReceiverSessionChangeResourceMode((SESSION), kAirPlayResourceID_MainAudio, kAirPlayTransferType_Untake,  \
        kAirPlayTransferPriority_NotApplicable, kAirPlayConstraint_NotApplicable, kAirPlayConstraint_NotApplicable, \
        (REASON), (COMPLETION), (CONTEXT))

#define AirPlayReceiverSessionBorrowMainAudio(SESSION, PRIORITY, UNBORROW_CONSTRAINT, REASON, COMPLETION, CONTEXT)             \
    AirPlayReceiverSessionChangeResourceMode((SESSION), kAirPlayResourceID_MainAudio, kAirPlayTransferType_Borrow, (PRIORITY), \
        kAirPlayConstraint_NotApplicable, (UNBORROW_CONSTRAINT), (REASON), (COMPLETION), (CONTEXT))

#define AirPlayReceiverSessionUnborrowMainAudio(SESSION, REASON, COMPLETION, CONTEXT)                                \
    AirPlayReceiverSessionChangeResourceMode((SESSION), kAirPlayResourceID_MainAudio, kAirPlayTransferType_Unborrow, \
        kAirPlayTransferPriority_NotApplicable, kAirPlayConstraint_NotApplicable, kAirPlayConstraint_NotApplicable,  \
        (REASON), (COMPLETION), (CONTEXT))

OSStatus
AirPlayReceiverSessionChangeResourceMode(
    AirPlayReceiverSessionRef inSession,
    AirPlayResourceID inResourceID,
    AirPlayTransferType inType,
    AirPlayTransferPriority inPriority,
    AirPlayConstraint inTakeConstraint,
    AirPlayConstraint inBorrowConstraint,
    CFStringRef inReason,
    AirPlayReceiverSessionCommandCompletionFunc inCompletion,
    void* inContext);

// AppStates

#define AirPlayReceiverSessionChangePhoneCall(SESSION, ON_PHONE, REASON, COMPLETION, CONTEXT)        \
    AirPlayReceiverSessionChangeAppState((SESSION), kAirPlaySpeechMode_NotApplicable,                \
        (ON_PHONE) ? kAirPlayTriState_True : kAirPlayTriState_False, kAirPlayTriState_NotApplicable, \
        (REASON), (COMPLETION), (CONTEXT))

#define AirPlayReceiverSessionChangeSpeechMode(SESSION, SPEECH_MODE, REASON, COMPLETION, CONTEXT)  \
    AirPlayReceiverSessionChangeAppState((SESSION), (SPEECH_MODE), kAirPlayTriState_NotApplicable, \
        kAirPlayTriState_NotApplicable, (REASON), (COMPLETION), (CONTEXT))

#define AirPlayReceiverSessionChangeTurnByTurn(SESSION, TURN_BY_TURN, REASON, COMPLETION, CONTEXT)                    \
    AirPlayReceiverSessionChangeAppState((SESSION), kAirPlaySpeechMode_NotApplicable, kAirPlayTriState_NotApplicable, \
        (TURN_BY_TURN) ? kAirPlayTriState_True : kAirPlayTriState_False, (REASON), (COMPLETION), (CONTEXT))

OSStatus
AirPlayReceiverSessionChangeAppState(
    AirPlayReceiverSessionRef inSession,
    AirPlaySpeechMode inSpeechMode,
    AirPlayTriState inPhoneCall,
    AirPlayTriState inTurnByTurn,
    CFStringRef inReason,
    AirPlayReceiverSessionCommandCompletionFunc inCompletion,
    void* inContext);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionMakeModeStateFromDictionary
	@abstract	Parses a mode state dictionary (e.g. from a ModesChanged command) into an AirPlayModeState structure.
	
	@param		inSession		Session the dictionary came from.
	@param		inDict			Mode state dictionary to convert from.
	@param		outModes		Receives the parsed mode state.
*/
OSStatus
AirPlayReceiverSessionMakeModeStateFromDictionary(
    AirPlayReceiverSessionRef inSession,
    CFDictionaryRef inDict,
    AirPlayModeState* outModes);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionForceKeyFrame
	@abstract	Builds and sends a key frame request to the sender side via the control channel.
 
	@param		inSession		Session from which to request a key frame.
	@param		inCompletion	Optional completion function to call when the request completes. May be NULL.
	@param		inContext		Optional context ptr to pass to completion function. May be NULL.
*/
OSStatus
AirPlayReceiverSessionForceKeyFrame(
    AirPlayReceiverSessionRef inSession,
    AirPlayReceiverSessionCommandCompletionFunc inCompletion,
    void* inContext);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionRequestSiriAction
	@abstract	Builds and sends Siri action command to the controller.
	
	@param		inSession		Session to request a Siri action command on.
	@param		inAction		Action to be performed (see kAirPlaySiriAction_*).
*/
OSStatus
AirPlayReceiverSessionRequestSiriAction(
    AirPlayReceiverSessionRef inSession,
    AirPlaySiriAction inAction,
    AirPlayReceiverSessionCommandCompletionFunc inCompletion,
    void* inContext);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionRequestUI
	@abstract	Builds and sends request UI command to the controller.
	
	@param		inSession		Session to request request UI command on.
	@param		inURL			Optional UI describing the UI to reuest (e.g. "http://maps.apple.com/?q" to show a map).
	@param		inCompletion	Optional completion function to call when the request completes. May be NULL.
	@param		inContext		Optional context ptr to pass to completion function. May be NULL.
*/
OSStatus
AirPlayReceiverSessionRequestUI(
    AirPlayReceiverSessionRef inSession,
    CFStringRef inURL,
    AirPlayReceiverSessionCommandCompletionFunc inCompletion,
    void* inContext);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionSetNightMode
	@abstract	Builds and sends set night mode command to the controller.
	
	@param		inSession		Session to request a night mode command on.
	@param		inNightMode		Whether to enable or disable night mode.
	@param		inCompletion	Optional completion function to call when the request completes. May be NULL.
	@param		inContext		Optional context ptr to pass to completion function. May be NULL.
*/
OSStatus
AirPlayReceiverSessionSetNightMode(
    AirPlayReceiverSessionRef inSession,
    Boolean inNightMode,
    AirPlayReceiverSessionCommandCompletionFunc inCompletion,
    void* inContext);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionSetLimitedUI
	@abstract	Builds and sends set limited UI command to the controller.
	
	@param		inSession				Session to request a limited UI command on.
	@param		inLimitUI				Whether or not to limit UI.
	@param		inCompletion			Optional completion function to call when the request completes. May be NULL.
	@param		inContext				Optional context ptr to pass to completion function. May be NULL.
*/
OSStatus
AirPlayReceiverSessionSetLimitedUI(
    AirPlayReceiverSessionRef inSession,
    Boolean inLimitUI,
    AirPlayReceiverSessionCommandCompletionFunc inCompletion,
    void* inContext);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayReceiverSessionSendHIDReport
	@abstract	Builds and sends a HID report to the controller.
	 
	@param		inSession				Session to request a HID report command on.
	@param		deviceUID				The identifier for the HID device.
	@param		inPtr					The HID report.
	@param		inLen					The length of the HID report.
*/
OSStatus
AirPlayReceiverSessionSendHIDReport(
    AirPlayReceiverSessionRef inSession,
    uint32_t deviceUID,
    const uint8_t* inPtr,
    size_t inLen);

OSStatus
AirPlayReceiverSessionSendMediaCommandWithOptions(
    AirPlayReceiverSessionRef inSession,
    CFStringRef command,
    CFDictionaryRef options);

#ifdef __cplusplus
}
#endif

#endif // __AirPlayReceiverSession_h__
