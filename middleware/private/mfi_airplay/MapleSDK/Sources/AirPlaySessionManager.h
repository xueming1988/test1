/*
	Copyright (C) Apple Inc. All Rights Reserved.
*/

#ifndef __AirPlaySessionManager_h__
#define __AirPlaySessionManager_h__

#include "AirPlayCommon.h"
#include "AirPlayReceiverServer.h"
#include "AirPlayReceiverSession.h"
#include "CommonServices.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    kAirPlaySessionManagerError_InvalidParameter = -72240,
    kAirPlaySessionManagerError_UnexpectedState = -72241,
    kAirPlaySessionManagerError_SecurityRequired = -72242,
    kAirPlaySessionManagerError_PropertyNotFound = -72243,
    kAirPlaySessionManagerError_EntryNotFound = -72244,
    kAirPlaySessionManagerError_NoMemory = -72245,
} AirPlaySessionManagerError;

typedef struct AirPlaySessionManagerOpaque* AirPlaySessionManagerRef;

EXPORT_GLOBAL CFTypeID AirPlaySessionManagerGetTypeID(void);

OSStatus AirPlaySessionManagerCreate(
    AirPlayReceiverServerRef inReceiverServer,
    dispatch_queue_t inHTTPQueue,
    CFAllocatorRef inAllocator,
    AirPlaySessionManagerRef* outSessionManagerRef);

OSStatus AirPlaySessionManagerAddSession(
    AirPlaySessionManagerRef inSessionManager,
    AirPlayReceiverSessionRef inSession);

OSStatus AirPlaySessionManagerRemoveSession(
    AirPlaySessionManagerRef inSessionManager,
    AirPlayReceiverSessionRef inSession);

OSStatus AirPlaySessionManagerRemoteSessions(
    AirPlaySessionManagerRef inSessionManager,
    CFArrayRef* outSessions);

Boolean AirPlaySessionManagerIsMasterSessionActive(
    AirPlaySessionManagerRef inSessionManager);

AirPlayReceiverSessionRef AirPlaySessionManagerCopyMasterSession(
    AirPlaySessionManagerRef inSessionManager);

Boolean AirPlaySessionManagerCanHijack(
    AirPlaySessionManagerRef inSessionManager,
    AirPlayReceiverSessionRef inSession);

OSStatus AirPlaySessionManagerHijack(
    AirPlaySessionManagerRef inSessionManager,
    AirPlayReceiverSessionRef inSession);

// TODO: use dictionary for RCS, to hide clientType and clientUUID in it.
OSStatus AirPlaySessionManagerRegisterNewRoute(
    AirPlaySessionManagerRef inSessionManager,
    AirPlayReceiverSessionRef inRemoteSession,
    CFNumberRef inRemoteRCSObjRef, // TODO: pass dictionary containing inClientTypeUUID and inClientUUID
    CFStringRef inClientTypeUUID,
    CFStringRef inClientUUID,
    AirPlayRemoteControlSessionControlType inControlType,
    CFNumberRef* outTokenRef);

OSStatus AirPlaySessionManagerRouteMessage(
    AirPlaySessionManagerRef inSessionManager,
    AirPlayReceiverSessionRef inRemoteSession,
    CFNumberRef inRCSObjRef,
    CFDictionaryRef inRCSEntryDict,
    CFDictionaryRef inMessageParams);

OSStatus AirPlaySessionManagerUnregisterRoute(
    AirPlaySessionManagerRef inSessionManager,
    CFNumberRef inTokenRef);

#if (AIRPLAY_METRICS)

CFIndex AirPlaySessionManagerRemoteSessionCount(AirPlaySessionManagerRef inSessionManager);

CFIndex AirPlaySessionManagerRelayCount(AirPlaySessionManagerRef inSessionManager);

#endif

#ifdef __cplusplus
}
#endif

#endif /* __AirPlaySessionManager_h__ */
