/*
	Copyright (C) 2007-2019 Apple Inc. All Rights Reserved.
*/

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>

#include "CFUtils.h"
#include "DebugServices.h"
#include "NetUtils.h"
#include "StringUtils.h"

#include "AirPlayReceiverServer.h"
#include "AirPlayReceiverSession.h"
#include "AirPlayReceiverSessionPriv.h"
#include "AirPlayUtils.h"
#include "AirPlayVersion.h"
#include "AudioUtils.h"
#include "AirPlayHomeIntegrationUtilities.h"

#ifdef LINKPLAY
#include "Platform_linkplay.h"
#endif


//===========================================================================================================================
//	Internals
//===========================================================================================================================

static CFTypeRef _AirPlayHandleServerCopyProperty(
    AirPlayReceiverServerRef inServer,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    OSStatus* outErr,
    void* inContext);
static OSStatus _AirPlayHandleServerSetProperty(
    AirPlayReceiverServerRef inServer,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    CFTypeRef inValue,
    void* inContext);
static void _AirPlayHandleSessionCreated(
    AirPlayReceiverServerRef inServer,
    AirPlayReceiverSessionRef inSession,
    void* inContext);
static CFTypeRef _AirPlayHandleSessionCopyProperty(
    AirPlayReceiverSessionRef inSession,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    OSStatus* outErr,
    void* inContext);
static void _AirPlayHandleSessionFinalized(AirPlayReceiverSessionRef inSession, void* inContext);
static void _AirPlayHandleSessionStarted(AirPlayReceiverSessionRef inSession, void* inContext);
static OSStatus _AirPlayHandleSessionControl(
    AirPlayReceiverSessionRef inSession,
    CFStringRef inCommand,
    CFTypeRef inQualifier,
    CFDictionaryRef inParams,
    CFDictionaryRef* outParams,
    void* inContext);

ulog_define(AtApp, kLogLevelNotice, kLogFlags_Default, "Airtunesd", NULL);
#define at_app_ucat() &log_category_from_name(AtApp)
#define at_app_ulog(LEVEL, ...) ulog(at_app_ucat(), (LEVEL), __VA_ARGS__)
#define at_app_dlog(LEVEL, ...) dlogc(at_app_ucat(), (LEVEL), __VA_ARGS__)

static OSStatus appInitialize(void);
static void AppFinalize(void);

CFArrayRef GetAudioFormats(OSStatus* outErr);
CFArrayRef GetAudioLatencies(OSStatus* outErr);

//static Boolean gAirPlayNameIsFactoryDefault = true;

AirPlayReceiverServerRef gAirPlayServer = NULL;


//===========================================================================================================================
//	main
//===========================================================================================================================

int main(int argc, char** argv)
{
    OSStatus err;

    (void)argc;
    (void)argv;

    signal(SIGPIPE, SIG_IGN); // Ignore SIGPIPE signals so we get EPIPE errors from APIs instead of a signal.

#ifdef LINKPLAY
    platform_linkplay_init();
#endif

    if ((err = appInitialize()) == kNoErr) {
        CFRunLoopRun();
    }
    AppFinalize();

    return (err ? 1 : 0);
}

#if 0
#pragma mark -
#endif

static OSStatus appInitialize()
{
    AirPlayReceiverServerDelegate delegate;
    OSStatus err;

#if (CFLITE_ENABLED)
    err = CFRunLoopEnsureInitialized();
    require_noerr(err, exit);
#endif

    // Create the AirPlay server. This advertise via Bonjour and starts listening for connections.
    err = AirPlayReceiverServerCreate(&gAirPlayServer);
    require_noerr(err, exit);

    // Register ourself as a delegate to receive server-level events, such as when a session is created.
    AirPlayReceiverServerDelegateInit(&delegate);
    delegate.copyProperty_f = _AirPlayHandleServerCopyProperty;
    delegate.setProperty_f = _AirPlayHandleServerSetProperty;
    delegate.sessionCreated_f = _AirPlayHandleSessionCreated;
    // delegate.sessionFailed_f = _AirPlayHandleSessionFailed;
#ifdef LINKPLAY
    delegate.control_f = AirPlayReciverHandleIncomingControl;
#endif
    AirPlayReceiverServerSetDelegate(gAirPlayServer, &delegate);

    // Start the server and run until the app quits.
    AirPlayReceiverServerStart(gAirPlayServer);

exit:
    return (err);
}

static void AppFinalize()
{
    AirPlayReceiverServerStop(gAirPlayServer);
    CFReleaseNullSafe(gAirPlayServer);
}

//===========================================================================================================================
//	_AirPlayHandleServerCopyProperty
//===========================================================================================================================

static CFTypeRef
_AirPlayHandleServerCopyProperty(AirPlayReceiverServerRef inServer,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    OSStatus* outErr,
    void* inContext)
{
    (void)inServer;
    (void)inQualifier;
    (void)inContext;

    CFTypeRef value = NULL;
    OSStatus err = kNoErr;

    if (0) {
    }

    else if (CFEqual(inProperty, CFSTR(kAirPlayKey_SourceVersion))) {
        value = CFStringCreateWithCString(kCFAllocatorDefault, kAirPlayMarketingVersionStr, kCFStringEncodingUTF8);
    }

    else if (CFEqual(inProperty, CFSTR(kAirPlayKey_Model))) {
#ifdef LINKPLAY
        value = CFStringCreateWithCString(kCFAllocatorDefault, (const char *)(g_wiimu_shm->model_name), kCFStringEncodingUTF8);
#else
        value = CFStringCreateWithCString(kCFAllocatorDefault, "Your Model", kCFStringEncodingUTF8);
#endif
    }

    else if (CFEqual(inProperty, CFSTR(kAirPlayKey_StatusFlags))) {
        // see also kAirPlayStatusFlag_ProblemsExist, kAirPlayStatusFlag_Unconfigured
        AirPlayStatusFlags acpSystemFlags = kAirPlayStatusFlag_AudioLink;
        value = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("0x%x"), acpSystemFlags);
    }

    else if (CFEqual(inProperty, CFSTR(kAirPlayKey_Name))) {
#ifdef LINKPLAY
        value = CFStringCreateWithCString(kCFAllocatorDefault, (const char *)GetGlobalAirplayDevName(), kCFStringEncodingUTF8);
#else
        value = CFStringCreateWithCString(kCFAllocatorDefault, "Your Speaker Name",  kCFStringEncodingUTF8);
#endif
    }

    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_NameIsFactoryDefault))) {
        int NameIsFactoryDefault = false;
#ifdef LINKPLAY
        if(0 == strcmp(GetGlobalAirplayDevName(), g_wiimu_shm->device_name)) {
            NameIsFactoryDefault = true;
        } else {
            NameIsFactoryDefault = false;
        }
#endif
        value = NameIsFactoryDefault ? kCFBooleanTrue : kCFBooleanFalse;
    }

    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_PTPInfo))) {
        // PTP version string: name, branch and version (e.g. "OpenAVNU ArtAndLogic-aPTP-changes 1.1")
        value = CFStringCreateWithCString(kCFAllocatorDefault, "SDK PTP 1.0", kCFStringEncodingUTF8);
    }

    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_AudioJackStatus))) {
        // see also kACPAudioNothingInserted, kAirPlayAudioJackStatus_Disconnected, kAirPlayAudioJackStatus_ConnectedDigital
        value = CFStringCreateWithCString(kCFAllocatorDefault, kAirPlayAudioJackStatus_ConnectedAnalog, kCFStringEncodingUTF8);
    }

    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_AudioFormats))) {
        value = GetAudioFormats(&err);
    }

    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_AudioLatencies))) {
        value = GetAudioLatencies(&err);
    }

    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_BluetoothIDs))) {
        value = NULL;
    }

    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_ExtendedFeatures))) {
        value = NULL;
    }

    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_Features))) {
        value = NULL;
    }

    else if (CFEqual(inProperty, CFSTR(kAirPlayKey_FirmwareRevision))) {
#ifdef LINKPLAY
        value = CFStringCreateWithCString(kCFAllocatorDefault, (const char *)(g_wiimu_shm->firmware_ver),
                                          kCFStringEncodingUTF8);
#else
        value = NULL;
#endif
    } else if(CFEqual(inProperty, CFSTR(kAirPlayKey_HardwareRevision))) {
        value = NULL;
    }

    else if (CFEqual(inProperty, CFSTR(kAirPlayKey_Manufacturer))) {
#ifdef LINKPLAY
        value = CFStringCreateWithCString(kCFAllocatorDefault, (const char *)(g_wiimu_shm->manufacturer),
                                          kCFStringEncodingUTF8);
#else
        value = NULL;
#endif
    } else {
#ifndef LINKPLAY
        at_app_ulog(kLogLevelNotice, "<- Unhandled server copy property %@... \n", inProperty);
        err = kNotHandledErr;
#else
        return platform_linkplay_handle_server_copy_property(inServer, inProperty, inQualifier, outErr, inContext);
#endif
    }

    if (outErr) {
        *outErr = err;
    }
    return (value);
}

//===========================================================================================================================
//    _AirPlayHandleServerSetProperty
//===========================================================================================================================
static OSStatus
_AirPlayHandleServerSetProperty(AirPlayReceiverServerRef inServer,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    CFTypeRef inValue,
    void* inContext)
{
    (void)inServer;
    (void)inQualifier;
    (void)inValue;
    (void)inContext;

    OSStatus status = kNoErr;

    if (0) {
    } else if(CFEqual(inProperty, CFSTR(kAirPlayKey_Name))) {
#ifdef LINKPLAY
        status = platform_linkplay_set_airplay_name(inValue);
#endif
    } else if(CFEqual(inProperty, CFSTR(kAirPlayProperty_Password))) {
#ifdef LINKPLAY
        status = platform_linkplay_set_airplay_pswd(inValue);
#endif
    } else if(CFEqual(inProperty, CFSTR(kAirPlayKey_GroupUUID))) {
#ifdef LINKPLAY
        status = platform_linkplay_set_group_uuid(inValue);
#endif
    } else if(CFEqual(inProperty, CFSTR(kAirPlayProperty_HomeIntegrationData))) {
#ifdef LINKPLAY
        status = platform_linkplay_set_airplay_HM_integrationData(inValue);
#endif
    } else {
        at_app_ulog(kLogLevelNotice, "<- Unhandled server set property %@... \n", inProperty);
        status = kNotHandledErr;
    }

    return status;
}

//===========================================================================================================================
//	_AirPlayHandleSessionCreated
//===========================================================================================================================

static void
_AirPlayHandleSessionCreated(AirPlayReceiverServerRef inServer,
    AirPlayReceiverSessionRef inSession,
    void* inContext)
{
    AirPlayReceiverSessionDelegate delegate;
    (void)inServer;
    (void)inContext;

    // Register ourself as a delegate to receive session-level events, such as modes changes.
    AirPlayReceiverSessionDelegateInit(&delegate);
    delegate.finalize_f = _AirPlayHandleSessionFinalized;
    delegate.started_f = _AirPlayHandleSessionStarted;
    delegate.control_f = _AirPlayHandleSessionControl;
    delegate.copyProperty_f = _AirPlayHandleSessionCopyProperty;
#ifdef LINKPLAY
    delegate.setProperty_f = AirPlayReceiverSessionSetPropertyHandler;
#endif
    delegate.modesChanged_f = NULL;
    delegate.requestUI_f = NULL;
    delegate.duckAudio_f = NULL;
    delegate.unduckAudio_f = NULL;

    AirPlayReceiverSessionSetDelegate(inSession, &delegate);
}

//===========================================================================================================================
//	_AirPlayHandleSessionFinalized
//===========================================================================================================================

static void _AirPlayHandleSessionFinalized(AirPlayReceiverSessionRef inSession, void* inContext)
{
    (void)inSession;
    (void)inContext;
}

//===========================================================================================================================
//	_AirPlayHandleSessionCopyProperty
//===========================================================================================================================

static CFTypeRef
_AirPlayHandleSessionCopyProperty(AirPlayReceiverSessionRef inSession,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    OSStatus* outErr,
    void* inContext)
{
    CFTypeRef value = NULL;
    OSStatus err;

    (void)inSession;
    (void)inQualifier;
    (void)inContext;

    if (CFStringCompare(inProperty, CFSTR(kAirPlayProperty_Modes), 0) == kCFCompareEqualTo) {
        err = kNoErr;
    } else if (CFStringCompare(inProperty, CFSTR(kAirPlayProperty_Volume), 0) == kCFCompareEqualTo) {

#ifdef LINKPLAY
        double d = platform_linkplay_volume_percent_to_db();
#else
        double d = LinearToDB(0.041246);
#endif

        at_app_ulog(kLogLevelNotice, "get volume db [%f]\n", d);

        value = CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, (const void *)&d);
        err = kNoErr;
    } else {
#ifndef LINKPLAY
        at_app_ulog(kLogLevelNotice, "Unsupported session copy property request: %@\n", inProperty);
        err = kNotHandledErr;
#else
        return platform_linkplay_handle_session_copy_property(inSession, inProperty, inQualifier, outErr, inContext);
#endif
    }

    if (outErr)
        *outErr = err;
    return (value);
}

//===========================================================================================================================
//	_AirPlayHandleSessionStarted
//===========================================================================================================================

static void _AirPlayHandleSessionStarted(AirPlayReceiverSessionRef inSession, void* inContext)
{
    (void)inSession;
    (void)inContext;
}

//===========================================================================================================================
//	_AirPlayHandleSessionControl
//===========================================================================================================================

static OSStatus
_AirPlayHandleSessionControl(
    AirPlayReceiverSessionRef inSession,
    CFStringRef inCommand,
    CFTypeRef inQualifier,
    CFDictionaryRef inParams,
    CFDictionaryRef* outParams,
    void* inContext)
{
    OSStatus err;

    (void)inSession;
    (void)inQualifier;
    (void)inParams;
    (void)outParams;
    (void)inContext;

    if (CFEqual(inCommand, CFSTR(kAirPlayCommand_FlushAudio))) {
        at_app_ulog(kLogLevelTrace, "Flush audio control request\n");
        err = kNoErr;
    } else {
        at_app_ulog(kLogLevelVerbose, "Unsupported session control request: %@\n", inCommand);
        err = kNotHandledErr;
    }

    return (err);
}
