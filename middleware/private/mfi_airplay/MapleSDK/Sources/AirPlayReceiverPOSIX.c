/*
	Copyright (C) 2013-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
	
	POSIX platform plugin for AirPlay.
*/

#include "AudioUtils.h"

#include "AirPlayCommon.h"
#include "AirPlayReceiverServer.h"
#include "AirPlayReceiverServerPriv.h"
#include "AirPlayReceiverSession.h"
#include "AirPlayReceiverSessionPriv.h"

#if (AIRPLAY_METRICS)
#include "AirPlayReceiverMetrics.h"
#endif
#ifdef LINKPLAY
#include "AudioPlay.h"
#endif

#if 0
#pragma mark == Structures ==
#endif

//===========================================================================================================================
//	Structures
//===========================================================================================================================

// AirPlayReceiverServerPlatformData

typedef struct
{
    uint32_t systemBufferSizeMicros;
    uint32_t systemSampleRate;
    dispatch_queue_t delegateQ; // Serialize messages to the delegates
} AirPlayReceiverServerPlatformData;

// AirPlayAudioStreamPlatformContext

typedef struct
{
    AirPlayStreamType type;
    AirPlayAudioFormat format;
    Boolean input;
    Boolean loopback;
    CFStringRef audioType;
    double vocoderSampleRate;

    AirPlayStreamType activeType;
    AirPlayReceiverSessionRef session;
    AudioStreamRef stream;
    Boolean started;

} AirPlayAudioStreamPlatformContext;

// AirPlayReceiverSessionPlatformData

typedef struct
{
    AirPlayReceiverSessionRef session;
    AirPlayAudioStreamPlatformContext mainAudioCtx;
    AirPlayAudioStreamPlatformContext altAudioCtx;
    Boolean sessionStarted;

} AirPlayReceiverSessionPlatformData;

#if 0
#pragma mark == Prototypes ==
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static OSStatus _SetUpStreams(AirPlayReceiverSessionRef inSession, CFDictionaryRef inParams);
static void _TearDownStreams(AirPlayReceiverSessionRef inSession, CFDictionaryRef inParams);
static OSStatus _UpdateStreams(AirPlayReceiverSessionRef inSession);
static void
_AudioInputCallBack(
    uint32_t inSampleTime,
    uint64_t inHostTime,
    const void* inBuffer,
    size_t inLen,
    void* inContext);
static void
_AudioOutputCallBack(
    uint32_t inSampleTime,
    uint64_t inHostTime,
    void* inBuffer,
    size_t inLen,
    void* inContext);

#if (defined(LEGACY_REGISTER_SCREEN_HID))
static CFArrayRef _HIDCopyDevices(AirPlayReceiverSessionRef inSession, OSStatus* outErr);
static OSStatus _HIDStart(AirPlayReceiverSessionRef inSession);
static void _HIDStop(AirPlayReceiverSessionRef inSession);
#endif

static CFTypeRef serverCopyPropertyDelegate(CFTypeRef inServer, CFObjectFlags inFlags, CFStringRef inProperty, CFTypeRef inQualifier, OSStatus* outErr);
static OSStatus serverSetPropertyDelegate(CFTypeRef inServer, CFObjectFlags inFlags, CFStringRef inProperty, CFTypeRef inQualifier, CFTypeRef inValue);
static CFTypeRef sessionCopyPropertyDelegate(CFTypeRef inSession, CFObjectFlags inFlags, CFStringRef inProperty, CFTypeRef inQualifier, OSStatus* outErr);
static OSStatus sessionSetPropertyDelegate(CFTypeRef inSession, CFObjectFlags inFlags, CFStringRef inProperty, CFTypeRef inQualifier, CFTypeRef inValue);

#if 0
#pragma mark == Globals ==
#endif

//===========================================================================================================================
//	Globals
//===========================================================================================================================

ulog_define(AirPlayReceiverPlatform, kLogLevelTrace, kLogFlags_Default, "AirPlay", NULL);
#define atrp_ucat() &log_category_from_name(AirPlayReceiverPlatform)
#define atrp_ulog(LEVEL, ...) ulog(atrp_ucat(), (LEVEL), __VA_ARGS__)

#if 0
#pragma mark -
#pragma mark == Server-level APIs ==
#endif

//===========================================================================================================================
//	AirPlayReceiverServerPlatformInitialize
//===========================================================================================================================

OSStatus AirPlayReceiverServerPlatformInitialize(AirPlayReceiverServerRef inServer)
{
    OSStatus err;
    AirPlayReceiverServerPlatformData* platform;

    platform = (AirPlayReceiverServerPlatformData*)calloc(1, sizeof(*platform));
    require_action(platform, exit, err = kNoMemoryErr);
    platform->delegateQ = dispatch_queue_create("AirPlayReceiverDelegateQueue", 0);
    require_action(platform->delegateQ, exit, err = kNoMemoryErr);

    inServer->platformPtr = platform;
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	AirPlayReceiverServerPlatformFinalize
//===========================================================================================================================

void AirPlayReceiverServerPlatformFinalize(AirPlayReceiverServerRef inServer)
{
    AirPlayReceiverServerPlatformData* const platform = (AirPlayReceiverServerPlatformData*)inServer->platformPtr;

    if (!platform)
        return;
    dispatch_forget(&platform->delegateQ);

    free(platform);
    inServer->platformPtr = NULL;
}

//===========================================================================================================================
//	AirPlayReceiverServerPlatformControl
//===========================================================================================================================

OSStatus
AirPlayReceiverServerPlatformControl(
    CFTypeRef inServer,
    uint32_t inFlags,
    CFStringRef inCommand,
    CFTypeRef inQualifier,
    CFDictionaryRef inParams,
    CFDictionaryRef* outParams)
{
    AirPlayReceiverServerRef const server = (AirPlayReceiverServerRef)inServer;
    OSStatus err;

    (void)inFlags;

    if (0) {
    }

    // Other

    else if (server->delegate.control_f) {
        err = server->delegate.control_f(server, inCommand, inQualifier, inParams, outParams, server->delegate.context);
        goto exit;
    } else {
        err = kNotHandledErr;
        goto exit;
    }
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	AirPlayReceiverServerPlatformCopyProperty
//===========================================================================================================================

static CFTypeRef serverCopyPropertyDelegate(CFTypeRef inServer, CFObjectFlags inFlags, CFStringRef inProperty, CFTypeRef inQualifier, OSStatus* outErr)
{
    AirPlayReceiverServerRef const server = (AirPlayReceiverServerRef)inServer;
    CFTypeRef value = NULL;
    (void)inFlags;

    if (CFEqual(inProperty, CFSTR(kAirPlayProperty_ServerInfo)) && inQualifier && CFGetTypeID(inQualifier) == CFArrayGetTypeID()) {
        CFIndex idx;
        CFArrayRef properties = (CFArrayRef)inQualifier;
        CFMutableDictionaryRef theDict = NULL;

        theDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require(theDict, exit);

        for (idx = 0; idx < CFArrayGetCount(properties); idx++) {
            CFStringRef property = (CFStringRef)CFArrayGetValueAtIndex(properties, idx);
            CFTypeRef propertyValue = server->delegate.copyProperty_f(server, (CFStringRef)property, NULL, NULL, server->delegate.context);
            if (propertyValue) {
                CFDictionarySetValue(theDict, property, propertyValue);
                CFRelease(propertyValue);
            }
        }

        if (CFDictionaryGetCount(theDict) > 0)
            value = theDict;
        else
            CFRelease(theDict);
    exit:
        if (outErr)
            *outErr = (value == NULL ? kNotHandledErr : kNoErr);
    } else {
        value = server->delegate.copyProperty_f(server, inProperty, inQualifier, outErr, server->delegate.context);
    }

    return value;
}

CFTypeRef
AirPlayReceiverServerPlatformCopyProperty(
    CFTypeRef inServer,
    uint32_t inFlags,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    OSStatus* outErr)
{
    AirPlayReceiverServerRef const server = (AirPlayReceiverServerRef)inServer;
    CFTypeRef value = NULL;
    OSStatus err;

    (void)inFlags;

    if (0) {
    }

    // SystemFlags

    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_StatusFlags))) {
        // Always report an audio link until we can detect it correctly.

        value = CFNumberCreateInt64(kAirPlayStatusFlag_AudioLink);
        require_action(value, exit, err = kUnknownErr);
    }

#if (AIRPLAY_METRICS)
    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_MetricsHistory))) {
        value = (server->recordedTimeStamps ? CFArrayCreateCopy(NULL, server->recordedTimeStamps) : CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks));
        require_action_quiet(value, exit, err = kUnknownErr);
    }
#endif

    // Other

    else if (server->delegate.copyProperty_f) {
        AirPlayReceiverServerPlatformData* platform = (AirPlayReceiverServerPlatformData*)server->platformPtr;
        require_action(platform && platform->delegateQ, exit, err = kNotHandledErr);

        // Always go through the dispatch queue
        inFlags &= ~kCFObjectFlagDirect;
        value = CFObjectCopyProperty(inServer, platform->delegateQ, serverCopyPropertyDelegate, inFlags, inProperty, inQualifier, &err);
        goto exit;
    } else {
        err = kNotHandledErr;
        goto exit;
    }
    err = kNoErr;

exit:
    if (err)
        ForgetCF(&value);
    if (outErr)
        *outErr = err;
    return (value);
}

//===========================================================================================================================
//	AirPlayReceiverServerPlatformSetProperty
//===========================================================================================================================

static OSStatus serverSetPropertyDelegate(CFTypeRef inServer, CFObjectFlags inFlags, CFStringRef inProperty, CFTypeRef inQualifier, CFTypeRef inValue)
{
    AirPlayReceiverServerRef const server = (AirPlayReceiverServerRef)inServer;
    (void)inFlags;

    return server->delegate.setProperty_f(server, inProperty, inQualifier, inValue, server->delegate.context);
}

OSStatus
AirPlayReceiverServerPlatformSetProperty(
    CFTypeRef inServer,
    uint32_t inFlags,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    CFTypeRef inValue)
{
    AirPlayReceiverServerRef const server = (AirPlayReceiverServerRef)inServer;
    OSStatus err;

    (void)inFlags;

    if (0) {
    }

    // Other

    else if (server->delegate.setProperty_f) {
        AirPlayReceiverServerPlatformData* platform = (AirPlayReceiverServerPlatformData*)server->platformPtr;
        require_action(platform && platform->delegateQ, exit, err = kNotHandledErr);

        // Always go through the dispatch queue
        inFlags &= ~kCFObjectFlagDirect;
        err = CFObjectSetProperty(inServer, platform->delegateQ, serverSetPropertyDelegate, inFlags, inProperty, inQualifier, inValue);
        goto exit;
    } else {
        err = kNotHandledErr;
        goto exit;
    }
    err = kNoErr;

exit:
    return (err);
}

#if 0
#pragma mark -
#pragma mark == Session-level APIs ==
#endif

//===========================================================================================================================
//	AirPlayReceiverSessionPlatformInitialize
//===========================================================================================================================

OSStatus AirPlayReceiverSessionPlatformInitialize(AirPlayReceiverSessionRef inSession)
{
    OSStatus err;
    AirPlayReceiverSessionPlatformData* spd;

    spd = (AirPlayReceiverSessionPlatformData*)calloc(1, sizeof(*spd));
    require_action(spd, exit, err = kNoMemoryErr);
    spd->session = inSession;
    inSession->platformPtr = spd;
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	AirPlayReceiverSessionPlatformFinalize
//===========================================================================================================================

void AirPlayReceiverSessionPlatformFinalize(AirPlayReceiverSessionRef inSession)
{
    AirPlayReceiverSessionPlatformData* const spd = (AirPlayReceiverSessionPlatformData*)inSession->platformPtr;

    if (!spd)
        return;

#if (defined(LEGACY_REGISTER_SCREEN_HID))
    _HIDStop(inSession);
#endif
    spd->sessionStarted = false;
    _TearDownStreams(inSession, NULL);

    free(spd);
    inSession->platformPtr = NULL;
}

//===========================================================================================================================
//	AirPlayReceiverSessionPlatformControl
//===========================================================================================================================

OSStatus
AirPlayReceiverSessionPlatformControl(
    CFTypeRef inSession,
    uint32_t inFlags,
    CFStringRef inCommand,
    CFTypeRef inQualifier,
    CFDictionaryRef inParams,
    CFDictionaryRef* outParams)
{
    AirPlayReceiverSessionRef const session = (AirPlayReceiverSessionRef)inSession;
    AirPlayReceiverSessionPlatformData* const spd = (AirPlayReceiverSessionPlatformData*)session->platformPtr;
    OSStatus err;
    double duration, finalVolume;

    (void)inFlags;

    if (0) {
    }

    // DuckAudio

    else if (CFEqual(inCommand, CFSTR(kAirPlayCommand_DuckAudio))) {
        duration = CFDictionaryGetDouble(inParams, CFSTR(kAirPlayKey_DurationMs), &err);
        if (err || (duration < 0))
            duration = 500;
        duration /= 1000;

        finalVolume = CFDictionaryGetDouble(inParams, CFSTR(kAirPlayProperty_Volume), &err);
        finalVolume = !err ? finalVolume : LinearToDB(0.2);

        // Notify client of duck command
        if (session->delegate.duckAudio_f) {
            atrp_ulog(kLogLevelNotice, "Delegating ducking of audio to %f within %f seconds\n", finalVolume, duration);
            session->delegate.duckAudio_f(session, duration, finalVolume, session->delegate.context);
        } else {
            CFNumberRef value = (CFNumberRef)AirPlayReceiverSessionPlatformCopyProperty(inSession, 0x0, CFSTR(kAirPlayProperty_Volume), NULL, NULL);
            if (value == NULL || !CFNumberGetValue(value, kCFNumberDoubleType, &session->lastDuckedVolume))
                session->lastDuckedVolume = LinearToDB(0.5);
            CFNumberRef volume = CFNumberCreate(NULL, kCFNumberDoubleType, &finalVolume);
            AirPlayReceiverSessionPlatformSetProperty(session, inFlags, CFSTR(kAirPlayProperty_Volume), NULL, volume);

            ForgetCF(&value);
            ForgetCF(&volume);
        }
    }

    // UnduckAudio

    else if (CFEqual(inCommand, CFSTR(kAirPlayCommand_UnduckAudio))) {
        duration = CFDictionaryGetDouble(inParams, CFSTR(kAirPlayKey_DurationMs), &err);
        if (err || (duration < 0))
            duration = 500;
        duration /= 1000;

        // Notify client of unduck command
        if (session->delegate.unduckAudio_f) {
            atrp_ulog(kLogLevelNotice, "Delegating unducking of audio within %f seconds\n", duration);
            session->delegate.unduckAudio_f(session, duration, session->delegate.context);
        } else {
            CFNumberRef value = CFNumberCreate(NULL, kCFNumberDoubleType, &session->lastDuckedVolume);
            AirPlayReceiverSessionPlatformSetProperty(inSession, inFlags, CFSTR(kAirPlayProperty_Volume), NULL, value);
            session->lastDuckedVolume = LinearToDB(0.5);
            CFRelease(value);
        }
    }

    // SetUpStreams

    else if (CFEqual(inCommand, CFSTR(kAirPlayCommand_SetUpStreams))) {
        err = _SetUpStreams(session, inParams);
        require_noerr(err, exit);
    }

    // TearDownStreams

    else if (CFEqual(inCommand, CFSTR(kAirPlayCommand_TearDownStreams))) {
        _TearDownStreams(session, inParams);
    }

    // StartSession

    else if (CFEqual(inCommand, CFSTR(kAirPlayCommand_StartSession))) {
        spd->sessionStarted = true;
        err = _UpdateStreams(session);
        require_noerr(err, exit);
    }

    // Other

    else if (session->delegate.control_f) {
        err = session->delegate.control_f(session, inCommand, inQualifier, inParams, outParams, session->delegate.context);
        goto exit;
    } else {
        err = kNotHandledErr;
        goto exit;
    }
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	AirPlayReceiverSessionPlatformCopyProperty
//===========================================================================================================================

static CFTypeRef sessionCopyPropertyDelegate(CFTypeRef inSession, CFObjectFlags inFlags, CFStringRef inProperty, CFTypeRef inQualifier, OSStatus* outErr)
{
    AirPlayReceiverSessionRef const session = (AirPlayReceiverSessionRef)inSession;
    (void)inFlags;

    return session->delegate.copyProperty_f(session, inProperty, inQualifier, outErr, session->delegate.context);
}

CFTypeRef
AirPlayReceiverSessionPlatformCopyProperty(
    CFTypeRef inSession,
    uint32_t inFlags,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    OSStatus* outErr)
{
    AirPlayReceiverSessionRef const session = (AirPlayReceiverSessionRef)inSession;
    AirPlayReceiverSessionPlatformData* const spd = (AirPlayReceiverSessionPlatformData*)session->platformPtr;
    OSStatus err;
    CFTypeRef value = NULL;

    (void)inFlags;
    (void)spd;
    require_action_quiet(!session->isRemoteControlSession, exit, err = kNoErr; atrp_ulog(kLogLevelNotice, "CopyProperty '%@' not supported for RemoteControlSession\n", inProperty));
    if (session->delegate.copyProperty_f) {
        AirPlayReceiverServerPlatformData* platform = (AirPlayReceiverServerPlatformData*)session->server->platformPtr;
        require_action(platform && platform->delegateQ, exit, err = kNotHandledErr);

        // Always go through the dispatch queue
        inFlags &= ~kCFObjectFlagDirect;
        value = CFObjectCopyProperty(inSession, platform->delegateQ, sessionCopyPropertyDelegate, inFlags, inProperty, inQualifier, &err);
    } else {
        err = kNotHandledErr;
    }

    if (err != kNoErr) {

        if (0) {
        }

#if (defined(LEGACY_REGISTER_SCREEN_HID))
        // HIDDevices

        else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_HIDDevices))) {
            value = _HIDCopyDevices(session, &err);
        }
#endif

#if (AIRPLAY_VOLUME_CONTROL)
        // Volume

        else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_Volume)) && spd->mainAudioCtx.stream) {
            value = AudioStreamPropertyCopyValue(spd->mainAudioCtx.stream, inProperty, NULL);
        }
#endif

        else {
            err = kNotHandledErr;
        }
    }
exit:
    if (outErr)
        *outErr = err;
    return (value);
}

//===========================================================================================================================
//	AirPlayReceiverSessionPlatformSetProperty
//===========================================================================================================================

static OSStatus sessionSetPropertyDelegate(CFTypeRef inSession, CFObjectFlags inFlags, CFStringRef inProperty, CFTypeRef inQualifier, CFTypeRef inValue)
{
    AirPlayReceiverSessionRef const session = (AirPlayReceiverSessionRef)inSession;
    (void)inFlags;

    return session->delegate.setProperty_f(session, inProperty, inQualifier, inValue, session->delegate.context);
}

OSStatus
AirPlayReceiverSessionPlatformSetProperty(
    CFTypeRef inSession,
    uint32_t inFlags,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    CFTypeRef inValue)
{
    AirPlayReceiverSessionRef const session = (AirPlayReceiverSessionRef)inSession;
    AirPlayReceiverSessionPlatformData* const spd = (AirPlayReceiverSessionPlatformData*)session->platformPtr;
    OSStatus err;

    (void)spd;
    (void)inFlags;

    require_action_quiet(!session->isRemoteControlSession, exit, err = kNoErr; atrp_ulog(kLogLevelNotice, "SetProperty '%@' not supported for RemoteControlSession\n", inProperty));
    if (0) {
    }

#if (AIRPLAY_VOLUME_CONTROL)
    // Volume
    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_Volume))) {
        if (spd && spd->mainAudioCtx.stream && spd->mainAudioCtx.started) {
            double const dB = CFGetDouble(inValue, NULL);
            atrp_ulog(kLogLevelNotice, "Setting volume to dB=%f, linear=%f\n", dB, Clamp(DBtoLinear(dB), 0.0, 1.0));
            err = AudioStreamPropertySetDouble(spd->mainAudioCtx.stream, kAudioStreamProperty_Volume, dB);

#ifdef LINKPLAY
            audio_set_volume(dB);
#endif
            require_noerr(err, exit);
        }
    }
#endif

    // Other

    else if (session->delegate.setProperty_f) {
        AirPlayReceiverServerPlatformData* platform = (AirPlayReceiverServerPlatformData*)session->server->platformPtr;
        require_action(platform && platform->delegateQ, exit, err = kNotHandledErr);

        // Always go through the dispatch queue
        inFlags &= ~kCFObjectFlagDirect;
        err = CFObjectSetProperty(inSession, platform->delegateQ, sessionSetPropertyDelegate, inFlags, inProperty, inQualifier, inValue);
        goto exit;
    } else {
        atrp_ulog(kLogLevelNotice, "unhandled session property '%@' \n", inProperty);
        err = kNotHandledErr;
        goto exit;
    }
    err = kNoErr;

exit:
    return (err);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_SetUpStreams
//===========================================================================================================================

static OSStatus _SetUpStreams(AirPlayReceiverSessionRef inSession, CFDictionaryRef inParams)
{
    AirPlayReceiverSessionPlatformData* const spd = (AirPlayReceiverSessionPlatformData*)inSession->platformPtr;
    OSStatus err;
    CFArrayRef streams;
    CFIndex i, n;
    CFDictionaryRef streamDesc;
    AirPlayStreamType streamType;
    AirPlayAudioStreamPlatformContext* streamCtx;
    CFStringRef cfstr;
    CFDictionaryRef dict;

    require_action_quiet(!inSession->isRemoteControlSession, exit, err = kNoErr);
    streams = CFDictionaryGetCFArray(inParams, CFSTR(kAirPlayKey_Streams), NULL);
    n = streams ? CFArrayGetCount(streams) : 0;
    for (i = 0; i < n; ++i) {
        streamDesc = CFArrayGetCFDictionaryAtIndex(streams, i, &err);
        require_noerr(err, exit);

        streamType = (AirPlayStreamType)CFDictionaryGetInt64(streamDesc, CFSTR(kAirPlayKey_Type), NULL);
        switch (streamType) {
        case kAirPlayStreamType_GeneralAudio:
            streamCtx = &spd->mainAudioCtx;
            break;
        case kAirPlayStreamType_BufferedAudio:
            streamCtx = &spd->mainAudioCtx;
            break;

        case kAirPlayStreamType_RemoteControl:
            continue;
        default:
            atrp_ulog(kLogLevelNotice, "### Unsupported stream type %d\n", streamType);
            continue;
        }
        streamCtx->type = streamType;
        streamCtx->format = CFDictionaryGetInt64(streamDesc, CFSTR(kAirPlayKey_AudioFormat), NULL);
        if (streamCtx->format == kAirPlayAudioFormat_Invalid) {
            streamCtx->format = kAirPlayAudioFormat_ALAC_44KHz_16Bit_Stereo;
        }
        streamCtx->input = CFDictionaryGetBoolean(streamDesc, CFSTR(kAirPlayKey_Input), &err);
        streamCtx->loopback = CFDictionaryGetBoolean(streamDesc, CFSTR(kAirPlayKey_AudioLoopback), NULL);

        cfstr = CFDictionaryGetCFString(streamDesc, CFSTR(kAirPlayKey_AudioType), NULL);
        if (!cfstr)
            cfstr = CFSTR(kAirPlayAudioType_Default);
        ReplaceCF(&streamCtx->audioType, cfstr);

        dict = CFDictionaryGetCFDictionary(streamDesc, CFSTR(kAirPlayKey_VocoderInfo), NULL);
        if (dict) {
            streamCtx->vocoderSampleRate = CFDictionaryGetDouble(dict, CFSTR(kAirPlayVocoderInfoKey_SampleRate), NULL);
        }
    }

    err = _UpdateStreams(inSession);
    require_noerr(err, exit);

#if (defined(LEGACY_REGISTER_SCREEN_HID))
    if (IsValidSocket(inSession->eventSock)) {
        err = _HIDStart(inSession);
        require_noerr(err, exit);
    }
#endif

exit:
    if (err)
        _TearDownStreams(inSession, inParams);
    return (err);
}

//===========================================================================================================================
//	_TearDownStreams
//===========================================================================================================================

static void _TearDownStreams(AirPlayReceiverSessionRef inSession, CFDictionaryRef inParams)
{
    AirPlayReceiverSessionPlatformData* const spd = (AirPlayReceiverSessionPlatformData*)inSession->platformPtr;
    OSStatus err;
    CFArrayRef streams;
    CFIndex i, n;
    CFDictionaryRef streamDesc;
    AirPlayStreamType streamType;
    AirPlayAudioStreamPlatformContext* streamCtx;

    require_action_quiet(!inSession->isRemoteControlSession, exit, err = kNoErr);
    streams = inParams ? CFDictionaryGetCFArray(inParams, CFSTR(kAirPlayKey_Streams), NULL) : NULL;
    n = streams ? CFArrayGetCount(streams) : 0;
    for (i = 0; i < n; ++i) {
        streamDesc = CFArrayGetCFDictionaryAtIndex(streams, i, &err);
        require_noerr(err, exit);

        streamType = (AirPlayStreamType)CFDictionaryGetInt64(streamDesc, CFSTR(kAirPlayKey_Type), NULL);
        switch (streamType) {
        case kAirPlayStreamType_GeneralAudio:
            streamCtx = &spd->mainAudioCtx;
            break;
        case kAirPlayStreamType_BufferedAudio:
            streamCtx = &spd->mainAudioCtx;
            break;

        case kAirPlayStreamType_RemoteControl:
            continue;
        default:
            atrp_ulog(kLogLevelNotice, "### Unsupported stream type %d\n", streamType);
            continue;
        }
        streamCtx->type = kAirPlayStreamType_Invalid;
    }
    if (n == 0) {
        spd->mainAudioCtx.type = kAirPlayStreamType_Invalid;
        spd->altAudioCtx.type = kAirPlayStreamType_Invalid;
#if (defined(LEGACY_REGISTER_SCREEN_HID))
        _HIDStop(inSession);
#endif
    }
    _UpdateStreams(inSession);

exit:
    return;
}

//===========================================================================================================================
//	_UpdateStreams
//===========================================================================================================================

static OSStatus _UpdateStreams(AirPlayReceiverSessionRef inSession)
{
    AirPlayReceiverSessionPlatformData* const spd = (AirPlayReceiverSessionPlatformData*)inSession->platformPtr;
    OSStatus err;
    AirPlayAudioStreamPlatformContext* streamCtx;
    AudioStreamBasicDescription asbd;

    // Update main audio stream.

    streamCtx = &spd->mainAudioCtx;
    if ((streamCtx->type != kAirPlayStreamType_Invalid) && !streamCtx->stream) {
        atrp_ulog(kLogLevelNotice, "Main audio setting up %s for %@, input %s, loopback %s, volume:%f\n",
            AirPlayAudioFormatToString(streamCtx->format),
            streamCtx->audioType ? streamCtx->audioType : CFSTR(kAirPlayAudioType_Default),
            streamCtx->input ? "yes" : "no",
            streamCtx->loopback ? "yes" : "no",
#if (AIRPLAY_VOLUME_CONTROL)
            inSession->server->volume);
#else
            0);
#endif

        streamCtx->activeType = streamCtx->type;
        streamCtx->session = inSession;
        err = AudioStreamCreate(&streamCtx->stream);
        //TODO check what to do with inSession->server->audioStreamOptions; they are not used in AudioStreamCreate
        require_noerr(err, exit);
        AudioStreamSetDelegateContext(streamCtx->stream, inSession->delegate.context);

        if (streamCtx->input)
            AudioStreamSetInputCallback(streamCtx->stream, _AudioInputCallBack, streamCtx);
        AudioStreamSetOutputCallback(streamCtx->stream, _AudioOutputCallBack, streamCtx);
        AudioStreamPropertySetInt64(streamCtx->stream, kAudioStreamProperty_StreamType, streamCtx->activeType);
        AudioStreamPropertySetCString(streamCtx->stream, kAudioStreamProperty_ThreadName, "AirPlayAudioMain", kSizeCString);
        AudioStreamPropertySetInt64(streamCtx->stream, kAudioStreamProperty_ThreadPriority, kAirPlayThreadPriority_AudioReceiver);

        err = AudioStreamPropertySetBoolean(streamCtx->stream, kAudioStreamProperty_Input, streamCtx->input);
        require_noerr(err, exit);
        err = _AudioStreamSetProperty(streamCtx->stream, kAudioStreamProperty_AudioType, streamCtx->audioType);
        require_noerr(err, exit);

        if (inSession->server->multiZoneAudioHint != NULL) {
            err = _AudioStreamSetProperty(streamCtx->stream, kAudioStreamProperty_MultiZoneAudioHint, inSession->server->multiZoneAudioHint);
            require_noerr(err, exit);
        }
        err = AirPlayAudioFormatToPCM(streamCtx->format, &asbd);
        require_noerr(err, exit);
        AudioStreamSetFormat(streamCtx->stream, &asbd);

        if (streamCtx->vocoderSampleRate > 0)
            AudioStreamPropertySetDouble(streamCtx->stream, kAudioStreamProperty_VocoderSampleRate, streamCtx->vocoderSampleRate);

        err = AudioStreamPrepare(streamCtx->stream);
        require_noerr(err, exit);
#if (AIRPLAY_VOLUME_CONTROL)
        AudioStreamPropertySetDouble(streamCtx->stream, kAudioStreamProperty_Volume, inSession->server->volume);
#endif
    } else if ((streamCtx->type == kAirPlayStreamType_Invalid) && streamCtx->stream) {
        AudioStreamForget(&streamCtx->stream);
        ForgetCF(&streamCtx->audioType);
        streamCtx->started = false;
        streamCtx->session = NULL;
        streamCtx->vocoderSampleRate = 0;
        streamCtx->activeType = kAirPlayStreamType_Invalid;
        atrp_ulog(kLogLevelNotice, "Main audio torn down\n");
    }

    if ((streamCtx->type == kAirPlayStreamType_Invalid) && streamCtx->stream) {
        AudioStreamForget(&streamCtx->stream);
        ForgetCF(&streamCtx->audioType);
        streamCtx->started = false;
        streamCtx->session = NULL;
        streamCtx->vocoderSampleRate = 0;
        streamCtx->activeType = kAirPlayStreamType_Invalid;
        atrp_ulog(kLogLevelNotice, "Alt audio torn down\n");
    }

    // If audio has started, make sure all the streams are started.

    if (spd->sessionStarted) {
        streamCtx = &spd->mainAudioCtx;
        if (streamCtx->stream && !streamCtx->started) {
            err = AudioStreamStart(streamCtx->stream);
            if (err)
                atrp_ulog(kLogLevelWarning, "### Main audio start failed: %#m\n", err);
            streamCtx->started = true;
        }

        streamCtx = &spd->altAudioCtx;
        if (streamCtx->stream && !streamCtx->started) {
            err = AudioStreamStart(streamCtx->stream);
            if (err)
                atrp_ulog(kLogLevelWarning, "### Alt audio start failed: %#m\n", err);
            streamCtx->started = true;
        }
    }
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	_AudioInputCallBack
//===========================================================================================================================

static void
_AudioInputCallBack(
    uint32_t inSampleTime,
    uint64_t inHostTime,
    const void* inBuffer,
    size_t inLen,
    void* inContext)
{
    AirPlayAudioStreamPlatformContext* const streamCtx = (AirPlayAudioStreamPlatformContext*)inContext;
    OSStatus err;

    require_quiet(!streamCtx->loopback, exit);

    err = AirPlayReceiverSessionWriteAudio(streamCtx->session, streamCtx->activeType, inSampleTime, inHostTime,
        inBuffer, inLen);
    require_noerr(err, exit);

exit:
    return;
}

//===========================================================================================================================
//	_AudioOutputCallBack
//===========================================================================================================================

static void
_AudioOutputCallBack(
    uint32_t inSampleTime,
    uint64_t inHostTime,
    void* inBuffer,
    size_t inLen,
    void* inContext)
{
    AirPlayAudioStreamPlatformContext* const streamCtx = (AirPlayAudioStreamPlatformContext*)inContext;
    OSStatus err;

    err = AirPlayReceiverSessionReadAudio(streamCtx->session, streamCtx->activeType, inSampleTime, inHostTime,
        inBuffer, inLen);
    require_noerr(err, exit);

    if (streamCtx->input && streamCtx->loopback) {
        err = AirPlayReceiverSessionWriteAudio(streamCtx->session, streamCtx->activeType, inSampleTime, inHostTime,
            inBuffer, inLen);
        require_noerr(err, exit);
    }

exit:
    return;
}

#if 0
#pragma mark -
#pragma mark == HID ==
#endif

#if defined(LEGACY_REGISTER_SCREEN_HID)
//===========================================================================================================================
//	_HIDCopyDevices
//===========================================================================================================================

static CFArrayRef _HIDCopyDevices(AirPlayReceiverSessionRef inSession, OSStatus* outErr)
{
    CFArrayRef result = NULL;
    CFMutableArrayRef descriptions;
    CFArrayRef devices = NULL;
    ScreenRef mainScreen = NULL;
    CFIndex i, n;
    OSStatus err;
    CFTypeRef obj;

    (void)inSession;
    descriptions = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    require_action(descriptions, exit, err = kNoMemoryErr);

    mainScreen = ScreenCopyMain(&err);
    require_noerr(err, exit);

    devices = HIDCopyDevices(&err);
    require_noerr(err, exit);

    n = devices ? CFArrayGetCount(devices) : 0;
    for (i = 0; i < n; ++i) {
        HIDDeviceRef device;
        CFMutableDictionaryRef description;

        device = (HIDDeviceRef)CFArrayGetValueAtIndex(devices, i);
        description = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require_action(description, skip, err = kNoMemoryErr);

        obj = HIDDeviceCopyID(device);
        require_action(obj, skip, err = kNoMemoryErr);
        CFDictionarySetValue(description, CFSTR(kAirPlayKey_UUID), obj);
        CFRelease(obj);

        obj = HIDDeviceCopyProperty(device, kHIDDeviceProperty_ReportDescriptor, NULL, &err);
        require_noerr(err, skip);
        CFDictionarySetValue(description, CFSTR(kAirPlayKey_HIDDescriptor), obj);
        CFRelease(obj);

        obj = HIDDeviceCopyProperty(device, kHIDDeviceProperty_CountryCode, NULL, NULL);
        if (obj) {
            CFDictionarySetValue(description, CFSTR(kAirPlayKey_HIDCountryCode), obj);
            CFRelease(obj);
        }

        obj = HIDDeviceCopyProperty(device, kHIDDeviceProperty_Name, NULL, NULL);
        if (obj) {
            CFDictionarySetValue(description, CFSTR(kAirPlayKey_Name), obj);
            CFRelease(obj);
        }

        obj = HIDDeviceCopyProperty(device, kHIDDeviceProperty_ProductID, NULL, NULL);
        if (obj) {
            CFDictionarySetValue(description, CFSTR(kAirPlayKey_HIDProductID), obj);
            CFRelease(obj);
        }

        obj = HIDDeviceCopyProperty(device, kHIDDeviceProperty_SampleRate, NULL, NULL);
        if (obj) {
            CFDictionarySetValue(description, CFSTR(kAirPlayKey_SampleRate), obj);
            CFRelease(obj);
        }

        obj = HIDDeviceCopyProperty(device, kHIDDeviceProperty_VendorID, NULL, NULL);
        if (obj) {
            CFDictionarySetValue(description, CFSTR(kAirPlayKey_HIDVendorID), obj);
            CFRelease(obj);
        }

        if (mainScreen) {
            obj = ScreenCopyProperty(mainScreen, kScreenProperty_UUID, NULL, NULL);
            if (obj) {
                CFDictionarySetValue(description, CFSTR(kAirPlayKey_DisplayUUID), obj);
                CFRelease(obj);
            }
        }

        CFArrayAppendValue(descriptions, description);
        err = kNoErr;

    skip:
        ForgetCF(&description);
        if (err)
            atrp_ulog(kLogLevelNotice, "### Report HID device %d of %d failed: %#m\n", (int)i, (int)n, err);
    }

    result = descriptions;
    descriptions = NULL;
    err = kNoErr;

exit:
    CFReleaseNullSafe(mainScreen);
    CFReleaseNullSafe(devices);
    CFReleaseNullSafe(descriptions);
    if (outErr)
        *outErr = err;
    return (result);
}

//===========================================================================================================================
//	_HIDStart
//===========================================================================================================================

static OSStatus _HIDStart(AirPlayReceiverSessionRef inSession)
{
    HIDSetSession(inSession);

    atrp_ulog(kLogLevelTrace, "HID started\n");

    return (kNoErr);
}

//===========================================================================================================================
//	_HIDStop
//===========================================================================================================================

static void _HIDStop(AirPlayReceiverSessionRef inSession)
{
    (void)inSession;

    atrp_ulog(kLogLevelTrace, "HID stopping\n");
    HIDSetSession(NULL);
}

#if 0
#pragma mark -
#pragma mark == Utilities ==
#endif

#endif
