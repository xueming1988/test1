/*
	Copyright (C) Apple Inc. All Rights Reserved.
*/

#include "AirPlaySessionManager.h"
#include "APReceiverMediaRemoteServices.h"
#include "AirPlayCommon.h"
#include "AirPlayReceiverServer.h"
#include "AirPlayReceiverServerPriv.h"
#include "AirPlayReceiverSessionPriv.h"
#include "AtomicUtils.h"
#include "CFUtils.h"
#include "CommonServices.h"
#include "LogUtils.h"

#include CF_HEADER
#include CF_RUNTIME_HEADER

#include <pthread.h>

#define kAirPlaySessionManagerKey_RCSObjRef CFSTR("rcsObjRef")
#define kAirPlaySessionManagerKey_ReqProcRef CFSTR("reqProcRef")

#define AirPlayRemoteControlSessionControlTypeStr(t) \
    ((t) == kAirPlayRemoteControlSessionControlType_None ? "Control Type None" : ((t) == kAirPlayRemoteControlSessionControlType_Relay ? "Control Type Relay" : ((t) == kAirPlayRemoteControlSessionControlType_Direct ? "Control Type Direct" : "Control Type Unknown")))

struct AirPlaySessionManagerOpaque {
    CFRuntimeBase base; // must be the first field

    dispatch_queue_t httpQueue; // Receiver HTTP queue
    AirPlayReceiverServerRef receiverServer; // Server managing the request processing - not retained
    AirPlayReceiverSessionRef masterSession; // master control request processor
    CFMutableArrayRef remoteSessions; // array of remote control request processor
    CFMutableDictionaryRef routingDict; // dictionary to re-route a message from one sender to another sender
    // key: token. value: CFArrayRef
    // CFArrayRef contains 2 entries - route1Info and route2Info
    // route1Info and route2Info contain the following:
    // Key1: "rcsObjRef". Value1: CFNumberRef RCS Object Reference - right now, RCS Object ref is streamID
    // Key2: "reqProcRef". Value2: Int64 request processor reference
    uint32_t tokenCounter; // stored in each session, used to map incoming route to outgoing route
    pthread_mutex_t lock;

#if (AIRPLAY_METRICS)
    uint32_t relayCount; // Number of relayed commands
#endif
};

ulog_define(AirPlaySessionManager, kLogLevelNotice, kLogFlags_Default, "AirPlaySessionManager", NULL);
#define ap_ucat() &log_category_from_name(AirPlaySessionManager)
#define ap_ulog(LEVEL, ...) ulog(ap_ucat(), (LEVEL), __VA_ARGS__)
#define ap_ulog_private(LEVEL, ...) ulog(ap_ucat(), (LEVEL), __VA_ARGS__)

static dispatch_once_t gAirPlaySessionManagerInitOnce = 0;
static CFTypeID gAirPlayReceiverSessionnManagerTypeID = _kCFRuntimeNotATypeID;

static OSStatus _AddRoute(
    AirPlaySessionManagerRef inSessionManager,
    AirPlayReceiverSessionRef inMasterSession,
    CFNumberRef inMasterRCSObjRef,
    AirPlayReceiverSessionRef inRemoteSession,
    CFNumberRef inRemoteRCSObjRef,
    CFNumberRef inTokenRef);
static void _Finalize(CFTypeRef inObj);
static void _GetTypeID(void* inContext);
static void removeOldestRemoteSession(AirPlaySessionManagerRef inSessionManager);

static const CFRuntimeClass kAirPlaySessionManagerClass = {
    0, // version
    "AirPlaySessionManager", // className
    NULL, // init
    NULL, // copy
    _Finalize, // finalize
    NULL, // equal -- NULL means pointer equality.
    NULL, // hash  -- NULL means pointer hash.
    NULL, // copyFormattingDesc
    NULL, // copyDebugDesc
    NULL, // reclaim
    NULL // refcount
};

//===========================================================================================================================

static void _Finalize(CFTypeRef inObj)
{
    AirPlaySessionManagerRef sessionManagerRef = (AirPlaySessionManagerRef)inObj;
    OSStatus err = kNoErr;

    ForgetCF(&sessionManagerRef->masterSession);
    ForgetCF(&sessionManagerRef->remoteSessions);
    ForgetCF(&sessionManagerRef->routingDict);
    err = pthread_mutex_destroy(&sessionManagerRef->lock);
    check_noerr(err);

    ap_ulog(kLogLevelChatty, "sessionManagerRef %p finalized\n", sessionManagerRef);
}

//===========================================================================================================================
//	AirPlaySessionManagerGetTypeID
//===========================================================================================================================
CFTypeID AirPlaySessionManagerGetTypeID(void)
{
    dispatch_once_f(&gAirPlaySessionManagerInitOnce, NULL, _GetTypeID);
    return (gAirPlayReceiverSessionnManagerTypeID);
}

//===========================================================================================================================
//	_GetTypeID
//===========================================================================================================================

static void _GetTypeID(void* inContext)
{
    (void)inContext;

    gAirPlayReceiverSessionnManagerTypeID = _CFRuntimeRegisterClass(&kAirPlaySessionManagerClass);
    check(gAirPlayReceiverSessionnManagerTypeID != _kCFRuntimeNotATypeID);
}

//===========================================================================================================================

OSStatus AirPlaySessionManagerCreate(
    AirPlayReceiverServerRef inReceiverServer,
    dispatch_queue_t inHTTPQueue,
    CFAllocatorRef inAllocator,
    AirPlaySessionManagerRef* outSessionManagerRef)
{
    OSStatus err = kNoErr;
    AirPlaySessionManagerRef sessionManagerRef = NULL;
    size_t extra = sizeof(struct AirPlaySessionManagerOpaque) - sizeof(CFRuntimeBase);

    sessionManagerRef = (AirPlaySessionManagerRef)_CFRuntimeCreateInstance(inAllocator, AirPlaySessionManagerGetTypeID(), extra, NULL);
    require_action(sessionManagerRef, exit, err = kAirPlaySessionManagerError_NoMemory);
    memset((char*)sessionManagerRef + sizeof(CFRuntimeBase), 0, extra);

    sessionManagerRef->receiverServer = inReceiverServer;
    sessionManagerRef->httpQueue = inHTTPQueue;
    sessionManagerRef->tokenCounter = 0; // start tokens from 1. zero is reserved. Increment first and then use.

    err = pthread_mutex_init(&sessionManagerRef->lock, NULL);
    require_noerr(err, exit);

    ap_ulog(kLogLevelChatty, "Receiver Session Manager %p created\n", sessionManagerRef);
    *outSessionManagerRef = sessionManagerRef;
    sessionManagerRef = NULL;

exit:
    CFReleaseNullSafe(sessionManagerRef);
    return err;
}

static void removeOldestRemoteSession(AirPlaySessionManagerRef inSessionManager)
{
    require(CFArrayGetCount(inSessionManager->remoteSessions) > 0, exit);

    AirPlayReceiverSessionRef oldestSession = (AirPlayReceiverSessionRef)CFArrayGetValueAtIndex(inSessionManager->remoteSessions, 0);
    require(oldestSession->isRemoteControlSession, exit);

    ap_ulog(kLogLevelNotice, "%s:%p sessionCount:%d\n", __func__, oldestSession, CFArrayGetCount(inSessionManager->remoteSessions));

    CFRetain(oldestSession);
    AirPlayReceiverSessionTearDown(oldestSession, NULL, kNoResourcesErr, NULL);
    CFRelease(oldestSession);

exit:
    return;
}

OSStatus AirPlaySessionManagerAddSession(
    AirPlaySessionManagerRef inSessionManager,
    AirPlayReceiverSessionRef inSession)
{
    static int kMaxSessions = 25;
    OSStatus err = kNoErr;
    CFIndex index = 0;

    require_action(inSessionManager, exit, err = kAirPlaySessionManagerError_InvalidParameter);
    require_action(inSession, exit, err = kAirPlaySessionManagerError_InvalidParameter);

    if (inSession->isRemoteControlSession) {
        ap_ulog(kLogLevelTrace, "Adding to Remote Control\n");
        if (inSessionManager->remoteSessions == NULL) {
            inSessionManager->remoteSessions = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
            require_action(inSessionManager->remoteSessions, exit, err = kAirPlaySessionManagerError_NoMemory);
        }

        // add new requestProcessor reference to remoteControl array
        // check if session is already present in the array
        index = CFArrayGetFirstIndexOfValue(
            inSessionManager->remoteSessions,
            CFRangeMake(0, CFArrayGetCount(inSessionManager->remoteSessions)),
            inSession);
        if (index == kCFNotFound) {
            // insert new session if the session is not part of the array already
            CFArrayAppendValue(inSessionManager->remoteSessions, inSession);
            ap_ulog(kLogLevelTrace, "Appending session...\n");

            // We can't handle an unlimited number of remote connections - we'll run out of fds.
            if (CFArrayGetCount(inSessionManager->remoteSessions) > kMaxSessions) {
                removeOldestRemoteSession(inSessionManager);
            }
        } else {
            ap_ulog(kLogLevelNotice, "Session %p already at index:%d...\n", index);
        }
    } else if (inSessionManager->masterSession != inSession) {
        ap_ulog(kLogLevelTrace, "Replacing master:%p with %p\n", inSessionManager->masterSession, inSession);
        ReplaceCF(&(inSessionManager->masterSession), inSession);
    }
exit:
    ap_ulog(kLogLevelChatty, "%s:%p sessionCount will be %d\n", __func__, inSession,
        (inSessionManager->remoteSessions ? CFArrayGetCount(inSessionManager->remoteSessions) : 0) + (inSessionManager->masterSession ? 1 : 0));
    return err;
}

OSStatus AirPlaySessionManagerRemoveSession(
    AirPlaySessionManagerRef inSessionManager,
    AirPlayReceiverSessionRef inSession)
{
    OSStatus err = kNoErr;
    CFIndex index = 0;

    require_action_quiet(inSessionManager, exit, err = kAirPlaySessionManagerError_InvalidParameter);
    require_action_quiet(inSession, exit, err = kAirPlaySessionManagerError_InvalidParameter);

    ap_ulog(kLogLevelTrace, "%s:%p\n", __func__, inSession);
    if (!inSession->isRemoteControlSession && inSessionManager->masterSession == inSession) {
        ap_ulog(kLogLevelNotice, "Releasing master session:%p\n", inSession);
        ForgetCF(&(inSessionManager->masterSession));
    } else if (inSession->isRemoteControlSession) {
        ap_ulog(kLogLevelNotice, "Removing remote session:%p\n", inSession);
        require_quiet(inSessionManager->remoteSessions, exit);
        // remove remote control request processor reference from remoteControl array
        // check if session is already present in the array
        index = CFArrayGetFirstIndexOfValue(
            inSessionManager->remoteSessions,
            CFRangeMake(0, CFArrayGetCount(inSessionManager->remoteSessions)),
            inSession);
        require_action((index != kCFNotFound), exit, err = kNoErr);
        ap_ulog(kLogLevelTrace, "Removed entry from Remote Control Array. %@ (%d)\n", inSession, CFGetRetainCount(inSession));
        CFArrayRemoveValueAtIndex(inSessionManager->remoteSessions, index);
    }

exit:
    return err;
}

OSStatus AirPlaySessionManagerRemoteSessions(
    AirPlaySessionManagerRef inSessionManager,
    CFArrayRef* outSessions)
{
    *outSessions = NULL;
    if (inSessionManager->remoteSessions) {
        *outSessions = CFArrayCreateCopy(NULL, inSessionManager->remoteSessions);
    }
    return (inSessionManager->remoteSessions ? kNoErr : kNotFoundErr);
}

Boolean AirPlaySessionManagerIsMasterSessionActive(AirPlaySessionManagerRef inSessionManager)
{
    return inSessionManager->masterSession != NULL;
}

AirPlayReceiverSessionRef AirPlaySessionManagerCopyMasterSession(AirPlaySessionManagerRef inSessionManager)
{
    CFRetainNullSafe(inSessionManager->masterSession);
    return (inSessionManager->masterSession);
}

Boolean AirPlaySessionManagerCanHijack(
    AirPlaySessionManagerRef inSessionManager,
    AirPlayReceiverSessionRef inSession)
{
    Boolean canHijack = true;

    require(inSessionManager, exit);
    require(inSession, exit);

    Boolean denyInterruptions = inSessionManager->receiverServer && inSessionManager->receiverServer->denyInterruptions;
    if (denyInterruptions) {
        if (inSessionManager->masterSession && inSessionManager->masterSession == inSession)
            canHijack = false;
    }
exit:
    return canHijack;
}

OSStatus AirPlaySessionManagerHijack(
    AirPlaySessionManagerRef inSessionManager,
    AirPlayReceiverSessionRef inSession)
{
    OSStatus err = kNoErr;

    require_action(inSessionManager, exit, err = kAirPlaySessionManagerError_InvalidParameter);
    require_action(inSession, exit, err = kAirPlaySessionManagerError_InvalidParameter);

    AirPlayReceiverServerHijackConnection(inSessionManager->receiverServer, inSessionManager->masterSession, inSession);

exit:
    return err;
}

static OSStatus _AddRoute(
    AirPlaySessionManagerRef inSessionManager,
    AirPlayReceiverSessionRef inMasterSession,
    CFNumberRef inMasterRCSObjRef,
    AirPlayReceiverSessionRef inRemoteSession,
    CFNumberRef inRemoteRCSObjRef,
    CFNumberRef inTokenRef)
{
    OSStatus err = kNoErr;
    CFMutableArrayRef arrayEntry = NULL;
    CFMutableDictionaryRef route1Info = NULL;
    CFMutableDictionaryRef route2Info = NULL;

    require_action(inSessionManager, exit, err = kParamErr);
    require_action(inMasterSession, exit, err = kParamErr);
    require_action(inRemoteSession, exit, err = kParamErr);

    pthread_mutex_lock(&inSessionManager->lock);
    if (!inSessionManager->routingDict) {
        inSessionManager->routingDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }
    pthread_mutex_unlock(&inSessionManager->lock);
    require_action(inSessionManager->routingDict, exit, err = kStateErr);

    arrayEntry = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    require_action(arrayEntry, exit, err = kNoMemoryErr);

    route1Info = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(route1Info, exit, err = kNoMemoryErr);
    CFDictionarySetValue(route1Info, kAirPlaySessionManagerKey_RCSObjRef, inMasterRCSObjRef);
    CFDictionarySetValue(route1Info, kAirPlaySessionManagerKey_ReqProcRef, inMasterSession);
    CFArrayAppendValue(arrayEntry, route1Info);

    // add route2Info for relays
    route2Info = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(route2Info, exit, err = kNoMemoryErr);
    CFDictionarySetValue(route2Info, kAirPlaySessionManagerKey_RCSObjRef, inRemoteRCSObjRef);
    CFDictionarySetValue(route2Info, kAirPlaySessionManagerKey_ReqProcRef, inRemoteSession);
    CFArrayAppendValue(arrayEntry, route2Info);

    pthread_mutex_lock(&inSessionManager->lock);
    CFDictionarySetValue(inSessionManager->routingDict, inTokenRef, arrayEntry);
    ap_ulog(kLogLevelChatty, "Session manager routing information after adding new route: %@\n", inSessionManager->routingDict);
    pthread_mutex_unlock(&inSessionManager->lock);

exit:
    CFReleaseNullSafe(arrayEntry);
    CFReleaseNullSafe(route1Info);
    CFReleaseNullSafe(route2Info);

    return (err);
}

//===========================================================================================================================

OSStatus AirPlaySessionManagerRegisterNewRoute(
    AirPlaySessionManagerRef inSessionManager,
    AirPlayReceiverSessionRef inRemoteSession,
    CFNumberRef inRemoteRCSObjRef, // TODO: pass dictionary containing inClientTypeUUID and inClientUUID
    CFStringRef inClientTypeUUID,
    CFStringRef inClientUUID,
    AirPlayRemoteControlSessionControlType inControlType,
    CFNumberRef* outTokenRef)
{
    OSStatus err = kNoErr;
    int64_t routeMappingToken = 0;
    CFNumberRef routeMappingTokenRef = NULL;
    CFNumberRef masterRCSObjRef = NULL;

    require_action_quiet(inSessionManager, exit, err = kParamErr);
    require_action_quiet(inRemoteSession, exit, err = kParamErr);
    require_action_quiet((inControlType == kAirPlayRemoteControlSessionControlType_Relay), exit, err = kParamErr);
    require_action_quiet(inSessionManager->masterSession && inSessionManager->masterSession != inRemoteSession, exit, err = kParamErr);

    // send a request to master controller (sender) to create a remote control session for new remote control sender if:
    // 1) master request processor is active and
    // 2) current request processor is not equal to the master request processor
    atomic_add_32(&inSessionManager->tokenCounter, 1);
    routeMappingToken = inSessionManager->tokenCounter;
    routeMappingTokenRef = CFNumberCreate(NULL, kCFNumberSInt64Type, &routeMappingToken);
    require_action(routeMappingTokenRef, exit, err = kNoMemoryErr);

    ap_ulog(kLogLevelTrace, "Creating relay\n");
    err = AirPlayReceiverSessionCreateRemoteControlSessionOnSender(inSessionManager->masterSession, routeMappingTokenRef, inClientTypeUUID, inClientUUID, &masterRCSObjRef);
    require_noerr(err, exit);

    err = _AddRoute(inSessionManager, inSessionManager->masterSession, masterRCSObjRef, inRemoteSession, inRemoteRCSObjRef, routeMappingTokenRef);
    require_noerr(err, exit);

    if (outTokenRef)
        *outTokenRef = routeMappingTokenRef;

exit:
    CFReleaseNullSafe(routeMappingTokenRef);
    return err;
}

//===========================================================================================================================

OSStatus AirPlaySessionManagerUnregisterRoute(
    AirPlaySessionManagerRef inSessionManager,
    CFNumberRef inTokenRef)
{
    OSStatus err = kNoErr;
    CFArrayRef arrayEntry = NULL;
    CFDictionaryRef route1Info = NULL;
    CFDictionaryRef route2Info = NULL;
    CFIndex count = 0;
    CFNumberRef route1RCSObjRef = NULL;
    AirPlayReceiverSessionRef route1SessionRef = NULL;

    require_action_quiet(inSessionManager, exit, err = kParamErr);
    require_action_quiet(inTokenRef, exit, err = kParamErr);

    pthread_mutex_lock(&inSessionManager->lock);
    if (CFDictionaryContainsKey(inSessionManager->routingDict, inTokenRef)) {
        arrayEntry = (CFArrayRef)CFDictionaryGetValue(inSessionManager->routingDict, inTokenRef);
        CFRetainNullSafe(arrayEntry);
    }
    pthread_mutex_unlock(&inSessionManager->lock);
    require_action(arrayEntry, exit, err = kStateErr);

    count = CFArrayGetCount(arrayEntry);
    require_action((count == 2), exit, err = kStateErr); // arrayEntry should contain atleast one element

    route1Info = CFArrayGetValueAtIndex(arrayEntry, 0); // route1Info contains master request processor route info
    require_action(route1Info, exit, err = kStateErr);

    // retrieve RCS object reference from route1Info
    route1RCSObjRef = (CFNumberRef)CFDictionaryGetValue(route1Info, kAirPlaySessionManagerKey_RCSObjRef);
    require_action(route1RCSObjRef, exit, err = kStateErr);
    route1SessionRef = (AirPlayReceiverSessionRef)(CFDictionaryGetValue(route1Info, kAirPlaySessionManagerKey_ReqProcRef));
    require_action(route1SessionRef, exit, err = kStateErr);

    route2Info = CFArrayGetValueAtIndex(arrayEntry, 1); // route2Info contains remote request processor route info
    if (route2Info) {
        // if route2Info exists, messages are being relayed. route1Info corresponds to the master and route2Info
        // corresponds to the remote route. Delete master's entry here and remote's entry will be deleted by
        // the session itself
        err = AirPlayReceiverSessionDeleteAndUnregisterRemoteControlSession(route1SessionRef, route1RCSObjRef);
        require_noerr(err, exit);
    }

    // remove entry from session manager's routing dictionary since the route does not exist anymore. equivalent to _RemoveRoute()
    pthread_mutex_lock(&inSessionManager->lock);
    CFDictionaryRemoveValue(inSessionManager->routingDict, inTokenRef);
    ap_ulog(kLogLevelChatty, "Session manager routing information after deleting route: %@\n", inSessionManager->routingDict);
    pthread_mutex_unlock(&inSessionManager->lock);

exit:
    CFReleaseNullSafe(arrayEntry);

    return err;
}

OSStatus AirPlaySessionManagerRouteMessage(
    AirPlaySessionManagerRef inSessionManager,
    AirPlayReceiverSessionRef inRemoteSession,
    CFNumberRef inRCSObjRef,
    CFDictionaryRef inRCSEntryDict,
    CFDictionaryRef inMessageParams)
{
    OSStatus err = kNoErr;
    CFArrayRef arrayEntry = NULL;
    CFDictionaryRef route1Info = NULL;
    CFDictionaryRef route2Info = NULL;
    CFIndex count = 0;
    CFNumberRef route1RCSObjRef = 0;
    AirPlayReceiverSessionRef route1ReqProcRef = NULL;
    CFNumberRef route2RCSObjRef = 0;
    AirPlayReceiverSessionRef route2ReqProcRef = NULL;
    AirPlayReceiverSessionRef outSessionReqProc = NULL;
    CFNumberRef outRCSObjRef = NULL;
    CFMutableDictionaryRef requestDict = NULL;
    CFStringRef clientUUID = NULL;
    CFNumberRef tokenRef = NULL;
    AirPlayRemoteControlSessionControlType controlType = kAirPlayRemoteControlSessionControlType_None;

    require_action(inSessionManager, exit, err = kParamErr);
    require_action(inSessionManager->masterSession, exit, err = kParamErr);
    require_action(inRemoteSession, exit, err = kParamErr);
    require_action(inRCSObjRef, exit, err = kParamErr);

    clientUUID = (CFStringRef)CFDictionaryGetValue(inRCSEntryDict, CFSTR(kAirPlayKey_ClientUUID));
    require_action(clientUUID, exit, err = kStateErr);

    tokenRef = (CFNumberRef)CFDictionaryGetValue(inRCSEntryDict, CFSTR(kAirPlayKey_TokenRef));
    require_action(tokenRef, exit, err = kStateErr);

    controlType = (AirPlayRemoteControlSessionControlType)CFDictionaryGetInt64(inRCSEntryDict, CFSTR(kAirPlayKey_ControlType), NULL);
    require_action(controlType != kAirPlayRemoteControlSessionControlType_None, exit, err = kStateErr);

    pthread_mutex_lock(&inSessionManager->lock);
    if (CFDictionaryContainsKey(inSessionManager->routingDict, tokenRef)) {
        arrayEntry = (CFArrayRef)CFDictionaryGetValue(inSessionManager->routingDict, tokenRef);
        CFRetainNullSafe(arrayEntry);
    }
    pthread_mutex_unlock(&inSessionManager->lock);
    require_action(arrayEntry, exit, err = kStateErr);

    count = CFArrayGetCount(arrayEntry);
    require_action(count == 2, exit, err = kStateErr); // arrayEntry should always contain only two elements

    route1Info = CFArrayGetValueAtIndex(arrayEntry, 0);
    require_action(route1Info, exit, err = kStateErr);
    route2Info = CFArrayGetValueAtIndex(arrayEntry, 1);
    require_action(route2Info, exit, err = kStateErr);

    // retrieve RCS object references and reqProcRefs from route1Info and route2Info
    route1RCSObjRef = (CFNumberRef)CFDictionaryGetValue(route1Info, kAirPlaySessionManagerKey_RCSObjRef);
    require_action(route1RCSObjRef, exit, err = kStateErr);
    route1ReqProcRef = (AirPlayReceiverSessionRef)(CFDictionaryGetValue(route1Info, kAirPlaySessionManagerKey_ReqProcRef));
    require_action(route1ReqProcRef, exit, err = kStateErr);

    route2RCSObjRef = (CFNumberRef)CFDictionaryGetValue(route2Info, kAirPlaySessionManagerKey_RCSObjRef);
    require_action(route2RCSObjRef, exit, err = kStateErr);
    route2ReqProcRef = (AirPlayReceiverSessionRef)(CFDictionaryGetValue(route2Info, kAirPlaySessionManagerKey_ReqProcRef));
    require_action(route2ReqProcRef, exit, err = kStateErr);

    // compare route1Info with input arguments to check if route1Info is the input route. If yes, route2Info is the output route
    if (CFEqual(inRCSObjRef, route1RCSObjRef) && (inRemoteSession == route1ReqProcRef)) {
        outRCSObjRef = route2RCSObjRef;
        outSessionReqProc = route2ReqProcRef;
    }
    // compare route2Info with input arguments to check if route1Info is the input route. if yes, route1Info is the output route
    else if (CFEqual(inRCSObjRef, route2RCSObjRef) && (inRemoteSession == route2ReqProcRef)) {
        outRCSObjRef = route1RCSObjRef;
        outSessionReqProc = route1ReqProcRef;
    } else {
        require_action(false, exit, err = kStateErr);
    }

    // re-route message
    ap_ulog(kLogLevelTrace, "Relaying a message from [%p, %@] to [%p, %@]\n", inRemoteSession, inRCSObjRef, outSessionReqProc, outRCSObjRef);
    requestDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(requestDict, CFSTR(kAirPlayKey_Params), inMessageParams);

#if (AIRPLAY_METRICS)
    inSessionManager->relayCount++;
#endif

    err = AirPlayReceiverSessionSendCommandForObject(outSessionReqProc, outRCSObjRef, requestDict, NULL, NULL);
    require_noerr(err, exit);

exit:
    CFReleaseNullSafe(requestDict);
    CFReleaseNullSafe(arrayEntry);
    return (err);
}

#if (AIRPLAY_METRICS)

CFIndex AirPlaySessionManagerRemoteSessionCount(AirPlaySessionManagerRef inSessionManager)
{
    CFIndex remoteSessionCount = 0;

    require_quiet(inSessionManager, exit);
    require_quiet(inSessionManager->remoteSessions, exit);
    remoteSessionCount = CFArrayGetCount(inSessionManager->remoteSessions);

exit:
    return remoteSessionCount;
}

CFIndex AirPlaySessionManagerRelayCount(AirPlaySessionManagerRef inSessionManager)
{
    int relayCount = 0;

    require_quiet(inSessionManager, exit);
    require_quiet(inSessionManager->remoteSessions, exit);
    relayCount = inSessionManager->relayCount;

exit:
    return relayCount;
}

#endif
