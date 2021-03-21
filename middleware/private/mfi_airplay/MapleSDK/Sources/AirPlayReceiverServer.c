/*
	Copyright (C) 2012-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "AirPlayReceiverServer.h"
#include "AirPlayHomeIntegrationUtilities.h"
#include "AirPlayReceiverLogRequest.h"
#include "AirPlayReceiverMetrics.h"
#include "AirPlayReceiverServerPriv.h"
#include "CFCompat.h"
#include "CommonServices.h"

#include "CFLiteBinaryPlist.h"
#include "NetTransportChaCha20Poly1305.h"
#include "NetUtils.h"
#include "RandomNumberUtils.h"
#include "StringUtils.h"
#include "ThreadUtils.h"
#include "TickUtils.h"
#include "TimeUtils.h"
#include "URLUtils.h"
#include "UUIDUtils.h"
#include "ed25519.h"

#if (AIRPLAY_SDP_SETUP)
#include "Base64Utils.h"
#include "SDPUtils.h"
#endif

#include "AirPlayCommon.h"
#include "AirPlayReceiverCommand.h"
#include "AirPlayReceiverSession.h"
#include "AirPlayReceiverSessionPriv.h"
#include "AirPlayUtils.h"
#include "AirPlayVersion.h"

#if (AIRPLAY_META_DATA)
#include "APSDMAP.h"
#endif

#if (0 || (TARGET_OS_POSIX && DEBUG))
#include "DebugIPCUtils.h"
#endif
#if (TARGET_OS_POSIX)
#include <signal.h>
#endif

#include CF_HEADER
#include CF_RUNTIME_HEADER
#include "dns_sd.h"
#include LIBDISPATCH_HEADER

#if (TARGET_OS_BSD)
#include <net/if_media.h>
#include <net/if.h>
#endif
#include "PairingUtils.h"
#ifdef LINKPLAY
#include "Platform_linkplay.h"
#endif

typedef enum {
    kAirPlayHTTPConnectionTypeAny = 0,
    kAirPlayHTTPConnectionTypeAudio = 1,
} AirPlayHTTPConnectionTypes;

// When building for the SDK we don't use a keychain group and we don't
// request cloud sync.
#define PairingSessionSetKeychainInfo_AirPlaySDK(SESSION) \
    PairingSessionSetKeychainInfo((SESSION),              \
        NULL,                                             \
        kPairingKeychainIdentityType_AirPlay,             \
        kPairingKeychainIdentityLabel_AirPlay,            \
        kPairingKeychainIdentityLabel_AirPlay,            \
        kPairingKeychainPeerType_AirPlay,                 \
        kPairingKeychainPeerLabel_AirPlay,                \
        kPairingKeychainPeerLabel_AirPlay,                \
        kPairingKeychainFlags_None)

#define PairingSessionSetKeychainInfo_AirPlaySDK_HomeKit(SESSION) \
    PairingSessionSetKeychainInfo((SESSION),                      \
        NULL,                                                     \
        kPairingKeychainIdentityType_AirPlay,                     \
        kPairingKeychainIdentityLabel_AirPlay,                    \
        kPairingKeychainIdentityLabel_AirPlay,                    \
        kPairingKeychainPeerType_AirPlay_HomeKit,                 \
        kPairingKeychainPeerLabel_AirPlay_HomeKit,                \
        kPairingKeychainPeerLabel_AirPlay_HomeKit,                \
        kPairingKeychainFlags_None)

#if 0
#pragma mark == Prototypes ==
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static char*
AirPlayGetConfigCString(
    AirPlayReceiverServerRef inServer,
    CFStringRef inKey,
    char* inBuffer,
    size_t inMaxLen,
    OSStatus* outErr);

static OSStatus AirPlayGetDeviceModel(AirPlayReceiverServerRef inServer, char* inBuffer, size_t inMaxLen);

static void _GetTypeID(void* inContext);
static void _GlobalInitialize(void* inContext);

#if (0 || (TARGET_OS_POSIX && DEBUG))
static OSStatus _HandleDebug(CFDictionaryRef inRequest, CFDictionaryRef* outResponse, void* inContext);
#endif

static OSStatus _InitializeConfig(AirPlayReceiverServerRef inServer);
static void _Finalize(CFTypeRef inCF);

static void DNSSD_API
_BonjourRegistrationHandler(
    DNSServiceRef inRef,
    DNSServiceFlags inFlags,
    DNSServiceErrorType inError,
    const char* inName,
    const char* inType,
    const char* inDomain,
    void* inContext);
static void _RestartBonjour(AirPlayReceiverServerRef inServer);
static void _RetryBonjour(void* inArg);
static void _StopBonjour(AirPlayReceiverServerRef inServer, const char* inReason);
static void _UpdateBonjour(AirPlayReceiverServerRef inServer);
static OSStatus _UpdateBonjourAirPlay(AirPlayReceiverServerRef inServer, CFDataRef* outTXTRecord);
static void _UpdateDeviceID(AirPlayReceiverServerRef inServer);
static HTTPServerRef _CreateHTTPServerForPort(AirPlayReceiverServerRef inServer, int inListenPort);
static void _StartServers(AirPlayReceiverServerRef inServer);
static void _StopServers(AirPlayReceiverServerRef inServer);

static Boolean _IsConnectionActive(HTTPConnectionRef inConnection);
static void _HijackConnections(AirPlayReceiverServerRef inServer, HTTPConnectionRef inHijacker);
static void _RemoveAllConnections(AirPlayReceiverServerRef inServer);
static void _DestroyConnection(HTTPConnectionRef inCnx);
static int _GetListenPort(AirPlayReceiverServerRef inServer);

#if (AIRPLAY_RAOP_SERVER)
static OSStatus _UpdateBonjourRAOP(AirPlayReceiverServerRef inServer, CFDataRef* outTXTRecord);
#endif

static void
_HandleHTTPConnectionCreated(
    HTTPServerRef inServer,
    HTTPConnectionRef inCnx,
    void* inCnxExtraPtr,
    void* inContext);
static OSStatus _HandleHTTPConnectionInitialize(HTTPConnectionRef inCnx, void* inContext);
static void _HandleHTTPConnectionFinalize(HTTPConnectionRef inCnx, void* inContext);
static void _HandleHTTPConnectionClose(HTTPConnectionRef inCnx, void* inContext);
static OSStatus _HandleHTTPConnectionMessage(HTTPConnectionRef inCnx, HTTPMessageRef inMsg, void* inContext);

static HTTPStatus _requestProcessAudioMode(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
static HTTPStatus _requestProcessAuthSetup(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
static HTTPStatus _requestProcessCommand(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
#if (AIRPLAY_HOME_CONFIGURATION)
static HTTPStatus _requestProcessConfigure(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
#endif
static HTTPStatus _requestProcessFeedback(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
static HTTPStatus _requestProcessFlush(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);

static HTTPStatus _requestProcessInfo(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
static HTTPStatus _requestProcessPairSetupPINStart(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest);
static HTTPStatus _requestProcessOptions(AirPlayReceiverConnectionRef inCnx);
static HTTPStatus _requestProcessPairSetup(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
static HTTPStatus _requestProcessPairVerify(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
static HTTPStatus _requestProcessPairAdd(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
static HTTPStatus _requestProcessPairRemove(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
static HTTPStatus _requestProcessPairList(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
static HTTPStatus _requestProcessFlushBuffered(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
static HTTPStatus _requestProcessGetParameter(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
static HTTPStatus _requestProcessSetParameter(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
static HTTPStatus _requestProcessSetParameterText(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
static HTTPStatus _requestProcessSetProperty(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
static DNSServiceErrorType _txtRecordSetCFString(TXTRecordRef* txtRecord, const char* key, CFStringRef cfString);
static DNSServiceErrorType _txtRecordSetBoolean(TXTRecordRef* txtRecord, const char* key, Boolean flag);
static DNSServiceErrorType _txtRecordSetUInt64(TXTRecordRef* txtRef, const char* key, uint64_t features);

#if (AIRPLAY_SDP_SETUP)
static HTTPStatus _requestProcessAnnounce(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest);
static OSStatus _requestProcessAnnounceAudio(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
static HTTPConnectionRef _FindActiveConnection(AirPlayReceiverServerRef inServer);
#endif // AIRPLAY_SDP_SETUP

static OSStatus _AirPlayReceiverConnectionConfigureEncryptionWithPairingSession(AirPlayReceiverConnectionRef cnx, PairingSessionRef pairingSession);

#if (AIRPLAY_DACP)
static void _HandleDACPStatus(OSStatus inStatus, void* inContext);
#endif

static HTTPStatus _requestProcessSetRateAndAnchorTime(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest);
static HTTPStatus _requestProcessSetPeers(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest);
static OSStatus _rateAndAnchorTimeFromPlist(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest, AirPlayAnchor* outAnchor);

#if (AIRPLAY_META_DATA)
static HTTPStatus
_requestProcessSetParameterArtwork(
    AirPlayReceiverConnectionRef inCnx,
    HTTPMessageRef inMsg,
    const char* inMIMETypeStr,
    size_t inMIMETypeLen);
static HTTPStatus _requestProcessSetParameterDMAP(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
static OSStatus _DMAPContentBlock_GetNextChunk(
    const uint8_t* inSrc,
    const uint8_t* inEnd,
    DMAPContentCode* outCode,
    size_t* outSize,
    const uint8_t** outData,
    const uint8_t** outSrc);
const DMAPContentCodeEntry* _DMAPFindEntryByContentCode(DMAPContentCode inCode);
#endif

static HTTPStatus _requestProcessRecord(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
static HTTPStatus _requestProcessSetup(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
static HTTPStatus _requestProcessSetupPlist(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
#if (AIRPLAY_SDP_SETUP)
static HTTPStatus _requestProcessSetupSDPAudio(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest);
#endif // AIRPLAY_SDP_SETUP

static HTTPStatus _requestProcessTearDown(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);
static void _updateGrouping(AirPlayReceiverServerRef inServer, CFDictionaryRef inRequestParams);
static void _clearGrouping(AirPlayReceiverServerRef inServer);
static OSStatus
_requestCreateSession(
    AirPlayReceiverConnectionRef inCnx,
    Boolean inUseEvents,
    Boolean isRemoteControlOnly);
static OSStatus
_requestDecryptKey(
    AirPlayReceiverConnectionRef inCnx,
    CFDictionaryRef inRequestParams,
    AirPlayEncryptionType inType,
    uint8_t inKeyBuf[16],
    uint8_t inIVBuf[16]);
static void _requestReportIfIncompatibleSender(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inMsg);

#if (TARGET_OS_POSIX)
static void _requestNetworkChangeListenerHandleEvent(uint32_t inEvent, void* inContext);
static void _tearDownNetworkListener(void* inContext);
#endif

static HTTPStatus _verifyUserPassword(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest);
static HTTPStatus _requestHTTPAuthorization_CopyPassword(HTTPAuthorizationInfoRef inInfo, char** outPassword);
static Boolean _requestHTTPAuthorization_IsValidNonce(HTTPAuthorizationInfoRef inInfo);
static Boolean _requestRequiresAuth(HTTPMessageRef inRequest);
static Boolean _requestRequiresHKAdmin(HTTPMessageRef inRequest);
static OSStatus _createPairingSession(AirPlayReceiverServerRef inServer, PairingSessionType type, CFStringRef accessGroup, PairingSessionRef* outSession);

#if (AIRPLAY_METRICS)
static HTTPStatus AirPlayReceiverClearHome(AirPlayReceiverConnectionRef inCnx);
#endif

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static dispatch_once_t gAirPlayReceiverServerInitOnce = 0;
static CFTypeID gAirPlayReceiverServerTypeID = _kCFRuntimeNotATypeID;

static const CFRuntimeClass kAirPlayReceiverServerClass = {
    0, // version
    "AirPlayReceiverServer", // className
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

static dispatch_once_t gAirPlayReceiverInitOnce = 0;

#define PairingDescription(cnx) ((cnx)->pairingVerified ? " (Paired)" : "")

#define AirPlayReceiverConnectionDidSetup(aprCnx) ((aprCnx)->didAudioSetup || (aprCnx)->didScreenSetup)

ulog_define(AirPlayReceiverServer, kLogLevelNotice, kLogFlags_Default, "AirPlay", NULL);
#define aprs_ucat() &log_category_from_name(AirPlayReceiverServer)
#define aprs_ulog(LEVEL, ...) ulog(aprs_ucat(), (LEVEL), __VA_ARGS__)
#define aprs_dlog(LEVEL, ...) dlogc(aprs_ucat(), (LEVEL), __VA_ARGS__)

ulog_define(AirPlayReceiverServerHTTP, kLogLevelNotice, kLogFlags_Default, "AirPlay", NULL);
#define aprs_http_ucat() &log_category_from_name(AirPlayReceiverServerHTTP)
#define aprs_http_ulog(LEVEL, ...) ulog(aprs_http_ucat(), (LEVEL), __VA_ARGS__)

STATIC_INLINE void
AirPlayReceiverServerSetValueFromDictionaryForKey(
    AirPlayReceiverServerRef inServer,
    CFDictionaryRef fromDict,
    CFMutableDictionaryRef toDictionary,
    CFStringRef key)
{
    CFTypeRef obj = NULL;

    if (inServer == NULL || inServer->config == NULL || (obj = CFDictionaryGetValue(inServer->config, key)) == NULL)
        obj = CFDictionaryGetValue(fromDict, key);

    if (obj)
        CFDictionarySetValue(toDictionary, key, obj);
}

//===========================================================================================================================
//	AirPlayCopyServerInfo
//===========================================================================================================================

CFDictionaryRef
AirPlayCopyServerInfo(
    AirPlayReceiverServerRef inServer,
    AirPlayReceiverSessionRef inSession,
    CFArrayRef inProperties,
    OSStatus* outErr)
{
    CFDictionaryRef result = NULL;
    OSStatus err = kNoErr;
    CFMutableDictionaryRef info = NULL;
    CFDictionaryRef propertiesDict = NULL;
    char str[PATH_MAX + 1];
    char* cptr;
    uint64_t u64;
    static const CFStringRef propertiesArray[] = {
        CFSTR(kAirPlayProperty_AudioLatencies),
        CFSTR(kAirPlayProperty_BluetoothIDs),
        CFSTR(kAirPlayProperty_ExtendedFeatures),
        CFSTR(kAirPlayKey_FirmwareRevision),
        CFSTR(kAirPlayKey_HardwareRevision),
        CFSTR(kAirPlayProperty_Manufacturer),
        CFSTR(kAirPlayProperty_NameIsFactoryDefault),
        CFSTR(kAirPlayKey_OSInfo),
        CFSTR(kAirPlayProperty_PTPInfo),
    };
    static CFIndex numProperties = sizeof(propertiesArray) / sizeof(propertiesArray[0]);
    static CFArrayRef sProperties = NULL;

    require_action(inServer, exit, err = kParamErr);

    (void)inSession;

    info = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(info, exit, err = kNoMemoryErr);

    if (sProperties == NULL)
        sProperties = CFArrayCreate(NULL, (const void**)propertiesArray, numProperties, &kCFTypeArrayCallBacks);

    require_action(sProperties, exit, err = kNoMemoryErr);

    propertiesDict = (CFDictionaryRef)AirPlayReceiverServerCopyProperty(inServer, kCFObjectFlagDirect, CFSTR(kAirPlayProperty_ServerInfo), sProperties, outErr);
    require(propertiesDict, exit);

    AirPlayReceiverServerSetValueFromDictionaryForKey(inServer, propertiesDict, info, CFSTR(kAirPlayProperty_AudioLatencies));
    AirPlayReceiverServerSetValueFromDictionaryForKey(inServer, propertiesDict, info, CFSTR(kAirPlayProperty_ExtendedFeatures));
    AirPlayReceiverServerSetValueFromDictionaryForKey(inServer, propertiesDict, info, CFSTR(kAirPlayKey_FirmwareRevision));
    AirPlayReceiverServerSetValueFromDictionaryForKey(inServer, propertiesDict, info, CFSTR(kAirPlayKey_HardwareRevision));
    AirPlayReceiverServerSetValueFromDictionaryForKey(inServer, propertiesDict, info, CFSTR(kAirPlayKey_Manufacturer));
    AirPlayReceiverServerSetValueFromDictionaryForKey(inServer, propertiesDict, info, CFSTR(kAirPlayKey_OSInfo));
    AirPlayReceiverServerSetValueFromDictionaryForKey(NULL, propertiesDict, info, CFSTR(kAirPlayProperty_BluetoothIDs));
    AirPlayReceiverServerSetValueFromDictionaryForKey(NULL, propertiesDict, info, CFSTR(kAirPlayKey_NameIsFactoryDefault));
    AirPlayReceiverServerSetValueFromDictionaryForKey(NULL, propertiesDict, info, CFSTR(kAirPlayProperty_PTPInfo));

    CFDictionarySetValue(info, CFSTR(kAirPlayKey_SDKRevision), CFSTR(AIRPLAY_SDK_REVISION));
    CFDictionarySetValue(info, CFSTR(kAirPlayKey_SDKBuild), CFSTR(AIRPLAY_SDK_BUILD));
    CFDictionarySetValue(info, CFSTR(kAirPlayKey_SourceVersion), CFSTR(kAirPlaySourceVersionStr));
    CFDictionarySetValue(info, CFSTR(kAirPlayKey_KeepAliveSendStatsAsBody), kCFBooleanTrue); // Supports statistics as part of the keep alive body.
    CFDictionarySetValue(info, CFSTR(kAirPlayKey_KeepAliveLowPower), kCFBooleanTrue); // Supports receiving UDP beacon as keep alive.

    CFDictionarySetCString(info, CFSTR(kAirPlayKey_FirmwareBuildDate), __DATE__, kSizeCString);
    CFDictionarySetCString(info, CFSTR(kAirPlayKey_DeviceID), MACAddressToCString(inServer->deviceID, str), kSizeCString);
#if (!AIRPLAY_VOLUME_CONTROL)
    CFDictionarySetInt64(info, CFSTR(kAirPlayProperty_VolumeControlType), kAirPlayVolumeControlType_None);
#endif
    if ((u64 = AirPlayGetFeatures(inServer)) != 0)
        CFDictionarySetInt64(info, CFSTR(kAirPlayKey_Features), u64);

    if ((u64 = AirPlayGetStatusFlags(inServer)) != 0)
        CFDictionarySetInt64(info, CFSTR(kAirPlayKey_StatusFlags), u64);

    if (AirPlayGetDeviceModel(inServer, str, sizeof(str)) == kNoErr)
        CFDictionarySetCString(info, CFSTR(kAirPlayKey_Model), str, kSizeCString);

    if (AirPlayGetDeviceName(inServer, str, sizeof(str)) == kNoErr)
        CFDictionarySetCString(info, CFSTR(kAirPlayKey_Name), str, kSizeCString);

    if (strcmp(kAirPlayProtocolVersionStr, "1.0") != 0)
        CFDictionarySetCString(info, CFSTR(kAirPlayKey_ProtocolVersion), kAirPlayProtocolVersionStr, kSizeCString);

    if (AirPlayCopyHomeKitPairingIdentity(inServer, &cptr, NULL) == kNoErr) {
        CFDictionarySetCString(info, CFSTR(kAirPlayKey_PublicHKID), cptr, kSizeCString);
        free(cptr);
    }

    // TXT records
    if (inProperties) {
        CFRange range = CFRangeMake(0, CFArrayGetCount(inProperties));
        if (CFArrayContainsValue(inProperties, range, CFSTR(kAirPlayKey_TXTAirPlay))) {
            CFDataRef data = NULL;
            if (_UpdateBonjourAirPlay(inServer, &data) == kNoErr && data != NULL) {
                CFDictionarySetValue(info, CFSTR(kAirPlayKey_TXTAirPlay), data);
                CFRelease(data);
            }
        }
    }

    result = info;
    info = NULL;
    err = kNoErr;
exit:
    CFReleaseNullSafe(info);
    CFReleaseNullSafe(propertiesDict);
    if (outErr && *outErr == kNoErr)
        *outErr = err;
    return (result);
}

//===========================================================================================================================
//	AirPlayGetConfigCString
//===========================================================================================================================

static char*
AirPlayGetConfigCString(
    AirPlayReceiverServerRef inServer,
    CFStringRef inKey,
    char* inBuffer,
    size_t inMaxLen,
    OSStatus* outErr)
{
    char* result = "";
    OSStatus err;

    require_action(inServer, exit, err = kNotInitializedErr);
    require_action_quiet(inServer->config, exit, err = kNotFoundErr);

    result = CFDictionaryGetCString(inServer->config, inKey, inBuffer, inMaxLen, &err);
    require_noerr_quiet(err, exit);

exit:
    if (outErr)
        *outErr = err;
    return (result);
}

//===========================================================================================================================
//	AirPlayGetDeviceID
//===========================================================================================================================

uint64_t AirPlayGetDeviceID(AirPlayReceiverServerRef inServer, uint8_t* outDeviceID)
{
    if (outDeviceID)
        memcpy(outDeviceID, inServer->deviceID, 6);
    return (ReadBig48(inServer->deviceID));
}

//===========================================================================================================================
//	AirPlayGetDeviceName
//===========================================================================================================================

OSStatus AirPlayGetDeviceName(AirPlayReceiverServerRef inServer, char* inBuffer, size_t inMaxLen)
{
    OSStatus err = -1;
    CFTypeRef obj;

    require_action(inMaxLen > 0, exit, err = kSizeErr);

    obj = AirPlayReceiverServerPlatformCopyProperty(inServer, kCFObjectFlagDirect,
        CFSTR(kAirPlayKey_Name), NULL, NULL);

    *inBuffer = '\0';

    if (obj == NULL || CFGetTypeID(obj) != CFStringGetTypeID() || !CFStringGetCString((CFStringRef)obj, inBuffer, inMaxLen, kCFStringEncodingUTF8)) {
        gethostname(inBuffer, inMaxLen);
    }
    CFReleaseNullSafe(obj);
    err = kNoErr;
exit:
    return (err);
}

//===========================================================================================================================
//	AirPlayGetDeviceModel
//===========================================================================================================================

static OSStatus AirPlayGetDeviceModel(AirPlayReceiverServerRef inServer, char* inBuffer, size_t inMaxLen)
{
    OSStatus err = -1;

    require_action(inMaxLen > 0, exit, err = kSizeErr);

    CFTypeRef obj = AirPlayReceiverServerPlatformCopyProperty(inServer, kCFObjectFlagDirect,
        CFSTR(kAirPlayKey_Model), NULL, NULL);
    *inBuffer = '\0';
    if (obj == NULL || CFGetTypeID(obj) != CFStringGetTypeID() || !CFStringGetCString((CFStringRef)obj, inBuffer, inMaxLen, kCFStringEncodingUTF8)) {
        AirPlayGetConfigCString(inServer, CFSTR(kAirPlayKey_Model), inBuffer, inMaxLen, NULL);
        if (*inBuffer == '\0') {
            strlcpy(inBuffer, "AirPlayGeneric1,1", inMaxLen); // If no model, use a default.
        }
    }

    CFReleaseNullSafe(obj);
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	AirPlayGetFeatures
//===========================================================================================================================

AirPlayFeatures AirPlayGetFeatures(AirPlayReceiverServerRef inServer)
{
    AirPlayFeatures features = 0;

    features |= kAirPlayFeature_Audio;
    features |= kAirPlayFeature_AudioAES_128_MFi_SAPv1;
    features |= kAirPlayFeature_AudioPCM;

    features |= kAirPlayFeature_RedundantAudio;
    features |= kAirPlayFeature_AudioAppleLossless;
    features |= kAirPlayFeature_AudioAAC_LC;
    features |= kAirPlayFeature_AudioUnencrypted;
    features |= kAirPlayFeature_UnifiedBonjour;
    features |= kAirPlayFeature_SupportsBufferedAudio;
    features |= kAirPlayFeature_HKPairingAndEncrypt;
    features |= kAirPlayFeature_TransientPairing;
    features |= kAirPlayFeature_SupportsHKPairingAndAccessControl;
    features |= kAirPlayFeature_SupportsHKPeerManagement;
    features |= kAirPlayFeature_Supports1588Clock;
    features |= kAirPlayFeature_AudioMetaDataProgress;
    
#if (AIRPLAY_META_DATA)
    features |= kAirPlayFeature_AudioMetaDataArtwork;
    features |= kAirPlayFeature_AudioMetaDataText;
#endif

#if !TARGET_OS_MACOSX
    features |= kAirPlayFeature_HKPairingAndEncrypt;
#endif

    features |= AirPlayReceiverServerPlatformGetInt64(inServer, CFSTR(kAirPlayProperty_Features), NULL, NULL);

    return (features);
}

//===========================================================================================================================
//	AirPlayGetRequiredSenderFeatures
//===========================================================================================================================

AirPlayFeatures AirPlayGetRequiredSenderFeatures(AirPlayReceiverServerRef inServer)
{
    AirPlayFeatures features = 0;

    (void)inServer;

    return (features);
}

//===========================================================================================================================
//	AirPlayGetMinimumClientOSBuildVersion
//===========================================================================================================================

OSStatus AirPlayGetMinimumClientOSBuildVersion(AirPlayReceiverServerRef inServer, char* inBuffer, size_t inMaxLen)
{
    OSStatus err;

    AirPlayGetConfigCString(inServer, CFSTR(kAirPlayKey_ClientOSBuildVersionMin), inBuffer, inMaxLen, &err);
    if (err) {
        AirPlayReceiverServerGetCString(inServer, CFSTR(kAirPlayKey_ClientOSBuildVersionMin), NULL, inBuffer, inMaxLen, &err);
    }
    return (err);
}

//===========================================================================================================================
//	AirPlayGetPairingPublicKeyID
//===========================================================================================================================

OSStatus AirPlayCopyHomeKitPairingIdentity(AirPlayReceiverServerRef inServer, char** outIdentifier, uint8_t outPK[32])
{
    OSStatus err;
    PairingSessionRef pairingSession;

    err = _createPairingSession(inServer, kPairingSessionType_None, kPairingKeychainAccessGroup_AirPlay, &pairingSession);
    require_noerr(err, exit);

    PairingSessionSetKeychainInfo_AirPlaySDK(pairingSession);

    err = PairingSessionCopyIdentity(pairingSession, true, outIdentifier, outPK, NULL);
    CFRelease(pairingSession);
    require_noerr(err, exit);

exit:
    return (err);
}

static OSStatus _createPairingSession(AirPlayReceiverServerRef inServer, PairingSessionType type, CFStringRef accessGroup, PairingSessionRef* outSession)
{
    OSStatus err = kNoErr;
    PairingSessionRef pairingSession = NULL;
    PairingDelegate pairingDelegate;

    require_action(outSession, exit, err = kParamErr);

    PairingDelegateInit(&pairingDelegate);
    pairingDelegate.context = inServer;

    pairingDelegate.copyIdentity_f = AirPlayReceiverPairingDelegateCopyIdentity;
    if (!CFStringCompare(accessGroup, kPairingKeychainAccessGroup_AirPlay, 0)) {
        pairingDelegate.findPeer_f = AirPlayReceiverPairingDelegateFindPeerAirPlay;
        pairingDelegate.savePeer_f = AirPlayReceiverPairingDelegateSavePeerAirPlay;
    } else if (!CFStringCompare(accessGroup, kPairingKeychainAccessGroup_HomeKit, 0)) {
        pairingDelegate.findPeer_f = AirPlayReceiverPairingDelegateFindPeerHomeKit;
        pairingDelegate.savePeer_f = AirPlayReceiverPairingDelegateSavePeerHomeKit;
    }

    err = PairingSessionCreate(&pairingSession, &pairingDelegate, type);
    require_noerr(err, exit);

    PairingSessionSetLogging(pairingSession, aprs_ucat());

    PairingSessionSetAddPairingHandler(pairingSession, AirPlayReceiverPairingDelegateAddPairing, inServer);
    PairingSessionSetRemovePairingHandler(pairingSession, AirPlayReceiverPairingDelegateRemovePairing, inServer);
    PairingSessionSetListPairingsHandler(pairingSession, AirPlayReceiverPairingDelegateCreateListPairings, inServer);

exit:
    if (err == kNoErr)
        *outSession = pairingSession;

    return err;
}

//===========================================================================================================================
//	AirPlayGetStatusFlags
//===========================================================================================================================

AirPlayStatusFlags AirPlayGetStatusFlags(AirPlayReceiverServerRef inServer)
{
    AirPlayStatusFlags flags = 0;

    flags |= (AirPlayStatusFlags)AirPlayReceiverServerPlatformGetInt64(inServer,
        CFSTR(kAirPlayProperty_StatusFlags), NULL, NULL);
    if (HIIsAccessControlEnabled(inServer)) {
        flags |= kAirPlayStatusFlag_HKAccessControl;
    }

    char playPassword[256] = { 0 };
    AirPlayReceiverServerPlatformGetCString(inServer, CFSTR(kAirPlayProperty_Password), NULL, playPassword, sizeof(playPassword), NULL);
    if (*playPassword != '\0')
        flags |= kAirPlayStatusFlag_PasswordRequired;
    if (inServer->group.uuid && inServer->group.supportsRelay) {
        flags |= kAirPlayStatusFlag_RemoteControlRelay;
    }

    return (flags);
}

//===========================================================================================================================
//	AirPlayReceiverServerSetLogLevel
//===========================================================================================================================

void AirPlayReceiverServerSetLogLevel(void)
{
#if (DEBUG || 0)
    LogControl(
        "?AirPlayReceiverCore:level=info"
        ",AirPlayScreenServerCore:level=trace"
        ",AirPlayReceiverSessionScreenCore:level=trace"
        ",AirTunesServerCore:level=info");
    dlog_control("?DebugServices.*:level=info");
#endif
}

//===========================================================================================================================
//	AirPlayReceiverServerSetLogPath
//===========================================================================================================================

void AirPlayReceiverServerSetLogPath(void)
{
#if (AIRPLAY_LOG_TO_FILE_PRIMARY)
    LogControl(kAirPlayPrimaryLogConfig);
#elif (AIRPLAY_LOG_TO_FILE_SECONDARY)
    LogControl(kAirPlaySecondaryLogConfig);
#endif
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	AirPlayReceiverServerGetTypeID
//===========================================================================================================================

EXPORT_GLOBAL
CFTypeID AirPlayReceiverServerGetTypeID(void)
{
    dispatch_once_f(&gAirPlayReceiverServerInitOnce, NULL, _GetTypeID);
    return (gAirPlayReceiverServerTypeID);
}

//===========================================================================================================================
//	AirPlayReceiverServerCreate
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus AirPlayReceiverServerCreateWithConfigFilePath(CFStringRef inConfigFilePath, AirPlayReceiverServerRef* outServer)
{
    OSStatus err;
    AirPlayReceiverServerRef me;
    size_t extraLen;

    dispatch_once_f(&gAirPlayReceiverInitOnce, NULL, _GlobalInitialize);

    extraLen = sizeof(*me) - sizeof(me->base);
    me = (AirPlayReceiverServerRef)_CFRuntimeCreateInstance(NULL, AirPlayReceiverServerGetTypeID(), (CFIndex)extraLen, NULL);
    require_action(me, exit, err = kNoMemoryErr);
    memset(((uint8_t*)me) + sizeof(me->base), 0, extraLen);

    me->timeoutDataSecs = kAirPlayDataTimeoutSecs;

    me->queue = dispatch_queue_create("AirPlayReceiverServerQueue", 0);
    me->httpQueue = dispatch_queue_create("AirPlayReceiverServerHTTPQueue", 0);

    if (!inConfigFilePath) {
        me->configFilePath = strdup(AIRPLAY_CONFIG_FILE_PATH);
    } else {
        err = CFStringCopyUTF8CString(inConfigFilePath, &me->configFilePath);
        require_noerr(err, exit);
    }
    _InitializeConfig(me);

    RandomBytes(me->httpTimedNonceKey, sizeof(me->httpTimedNonceKey));

    me->powerState = kPTPPowerStateUndefined;

    err = AirPlayReceiverServerPlatformInitialize(me);
    require_noerr(err, exit);

    *outServer = me;
    me = NULL;
    err = kNoErr;

exit:
    if (me)
        CFRelease(me);
    return (err);
}

#if 0
#pragma mark -
#endif

EXPORT_GLOBAL
OSStatus AirPlayReceiverServerCreateExt(CFTypeRef inDeviceID, CFTypeRef inAudioHint, uint64_t inPort, AirPlayReceiverServerRef* outServer)
{
    OSStatus status = AirPlayReceiverServerCreateWithConfigFilePath(NULL, outServer);

    if (status == kNoErr) {
        if (inPort) {
            (*outServer)->mediaPort = inPort;
        }

        if (inAudioHint) {
            (*outServer)->multiZoneAudioHint = inAudioHint;
            CFRetain(inAudioHint);
        }

        if (inDeviceID) {
            (*outServer)->multiZoneDeviceID = inDeviceID;
            CFRetain(inDeviceID);
        }
    }
    return status;
}

EXPORT_GLOBAL
OSStatus AirPlayReceiverServerCreate(AirPlayReceiverServerRef* outServer)
{
    return AirPlayReceiverServerCreateExt(NULL, NULL, 0, outServer);
}

//===========================================================================================================================
//	AirPlayReceiverServerSetDelegate
//===========================================================================================================================

EXPORT_GLOBAL
void AirPlayReceiverServerSetDelegate(AirPlayReceiverServerRef inServer, const AirPlayReceiverServerDelegate* inDelegate)
{
    inServer->delegate = *inDelegate;
}

//===========================================================================================================================
//	AirPlayReceiverServerGetDispatchQueue
//===========================================================================================================================

EXPORT_GLOBAL
dispatch_queue_t AirPlayReceiverServerGetDispatchQueue(AirPlayReceiverServerRef inServer)
{
    return (inServer->queue);
}

//===========================================================================================================================
//	AirPlayReceiverServerControl
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
AirPlayReceiverServerControl(
    CFTypeRef inServer,
    uint32_t inFlags,
    CFStringRef inCommand,
    CFTypeRef inQualifier,
    CFDictionaryRef inParams,
    CFDictionaryRef* outParams)
{
    AirPlayReceiverServerRef const server = (AirPlayReceiverServerRef)inServer;
    OSStatus err;

    if (0) {
    }

    // GetLogs

    else if (server->delegate.control_f && CFEqual(inCommand, CFSTR(kAirPlayCommand_GetLogs))) {
        aprs_ulog(kLogLevelNotice, "Get Log Archive\n");
        err = server->delegate.control_f(server, inCommand, inQualifier, inParams, outParams, server->delegate.context);
        goto exit;
    }

    // UpdateBonjour

    else if (CFEqual(inCommand, CFSTR(kAirPlayCommand_UpdateBonjour))) {
        _UpdateBonjour(server);
    }

    // StartServer / StopServer

    else if (CFEqual(inCommand, CFSTR(kAirPlayCommand_StartServer))) {
        if (!server->serversStarted) {
            server->started = true;
            _UpdateDeviceID(server);
            _StartServers(server);
        }
    } else if (CFEqual(inCommand, CFSTR(kAirPlayCommand_StopServer))) {
        if (server->started) {
            server->started = false;
            _StopServers(server);
        }
    }

    else if (CFEqual(inCommand, CFSTR(kAirPlayCommand_ResetBonjour))) {
        _RestartBonjour(server);
    }

    // SessionDied

    else if (CFEqual(inCommand, CFSTR(kAirPlayCommand_SessionDied))) {
        aprs_ulog(kLogLevelNotice, "### Failure: %#m\n", kTimeoutErr);
        _RemoveAllConnections(server);
    }

    // Other

    else {
        err = AirPlayReceiverServerPlatformControl(server, inFlags, inCommand, inQualifier, inParams, outParams);
        goto exit;
    }
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	AirPlayReceiverServerCopyProperty
//===========================================================================================================================

EXPORT_GLOBAL
CFTypeRef
AirPlayReceiverServerCopyProperty(
    CFTypeRef inServer,
    uint32_t inFlags,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    OSStatus* outErr)
{
    AirPlayReceiverServerRef const server = (AirPlayReceiverServerRef)inServer;
    OSStatus err;
    CFTypeRef value = NULL;

    if (0) {
    }

    // Playing

    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_Playing))) {
        value = server->playing ? kCFBooleanTrue : kCFBooleanFalse;
        CFRetain(value);
    }

    // Source version

    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_SourceVersion))) {
        value = CFSTR(kAirPlaySourceVersionStr);
        CFRetain(value);
    }

    // Other

    else {
        value = AirPlayReceiverServerPlatformCopyProperty(server, inFlags, inProperty, inQualifier, &err);
        goto exit;
    }
    err = kNoErr;

exit:
    if (outErr)
        *outErr = err;
    return (value);
}

//===========================================================================================================================
//	AirPlayReceiverServerSetProperty
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
AirPlayReceiverServerSetProperty(
    CFTypeRef inServer,
    uint32_t inFlags,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    CFTypeRef inValue)
{
    AirPlayReceiverServerRef const server = (AirPlayReceiverServerRef)inServer;
    OSStatus err;

    if (0) {
    }

    // DeviceID

    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_DeviceID))) {
        CFGetData(inValue, server->deviceID, sizeof(server->deviceID), NULL, NULL);
    }

    // InterfaceName

    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_InterfaceName))) {
        CFGetCString(inValue, server->ifname, sizeof(server->ifname));
    }

    // Playing

    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_Playing))) {
        server->playing = CFGetBoolean(inValue, NULL);
#if (AIRPLAY_DACP)
        if (server->playing) {
            AirTunesDACPClient_StartSession(server->dacpClient);
        } else {
            AirTunesDACPClient_StopSession(server->dacpClient);
        }
#endif
        // If we updated Bonjour while we were playing and had to defer it, do it now that we've stopped playing.
        if (!server->playing && server->started && server->serversStarted && server->bonjourRestartPending) {
            _RestartBonjour(server);
        }
    }

    // Other

    else {
        err = AirPlayReceiverServerPlatformSetProperty(server, inFlags, inProperty, inQualifier, inValue);
        goto exit;
    }
    err = kNoErr;

exit:
    return (err);
}

#if 0
#pragma mark -
#endif

#if AIRPLAY_HOME_CONFIGURATION

static CFDictionaryRef _AirPlayReceiverServerCreateConfigurationDictionary(AirPlayReceiverServerRef server, OSStatus* outErr)
{
    OSStatus err = kNoErr;
    CFStringRef deviceName = NULL;
    CFStringRef playPassword = NULL;
    CFStringRef adminKey = NULL;
    CFTypeRef enableHKAccessControl = NULL;
    CFNumberRef accessControlLevel = NULL;
    char* keyIdentifier = NULL;
    uint8_t publicKey[32];
    CFMutableDictionaryRef configDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    require_action(configDict, exit, err = kNoMemoryErr);

    deviceName = (CFStringRef)AirPlayReceiverServerCopyProperty(server, 0, CFSTR(kAirPlayKey_Name), NULL, &err);
    if (deviceName) {
        require_action(CFGetTypeID(deviceName) == CFStringGetTypeID(), exit, err = kInternalErr);
        CFDictionarySetValue(configDict, CFSTR(kAirPlayConfigurationKey_DeviceName), deviceName);
    }

    playPassword = (CFStringRef)AirPlayReceiverServerCopyProperty(server, 0, CFSTR(kAirPlayProperty_Password), NULL, &err);
    if (playPassword && CFStringGetLength(playPassword)) {
        require_action(CFGetTypeID(playPassword) == CFStringGetTypeID(), exit, err = kInternalErr);
        CFDictionarySetValue(configDict, CFSTR(kAirPlayConfigurationKey_Password), playPassword);
    }

    CFDictionarySetBoolean(configDict, CFSTR(kAirPlayConfigurationKey_EnableHKAccessControl), HIIsAccessControlEnabled(server));
    CFDictionarySetUInt64(configDict, CFSTR(kAirPlayConfigurationKey_AccessControlLevel), HIGetAccessControlLevel(server));

    err = AirPlayCopyHomeKitPairingIdentity(server, &keyIdentifier, publicKey);
    if (!err) {
        CFDictionarySetCString(configDict, CFSTR(kAirPlayConfigurationKey_PublicIdentifier), keyIdentifier, kSizeCString);
        ForgetMem(&keyIdentifier);

        CFDictionarySetData(configDict, CFSTR(kAirPlayConfigurationKey_PublicKey), publicKey, sizeof(publicKey));
    }

exit:
    CFReleaseNullSafe(deviceName);
    CFReleaseNullSafe(playPassword);
    CFReleaseNullSafe(adminKey);
    CFReleaseNullSafe(enableHKAccessControl);
    CFReleaseNullSafe(accessControlLevel);

    if (err != kNoErr) {
        ForgetCF(&configDict);
        ForgetMem(&keyIdentifier);
    }
    if (outErr) {
        *outErr = err;
    }
    return configDict;
}

#endif

static HTTPStatus _AirPlayReceiverConnectionCheckRequestAuthorization(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    HTTPStatus status = kHTTPStatus_Forbidden;
    Boolean hkAccessControlEnabled = false;
    AirPlayHomeKitAccessControlLevel hkAccessControlLevel = 0llu;

    // Does the request require auth?

    if (!_requestRequiresAuth(inRequest)) {
        return kHTTPStatus_OK;
    }

    // Is HK pairing sufficient for auth?

    hkAccessControlEnabled = HIIsAccessControlEnabled(inCnx->server);
    hkAccessControlLevel = HIGetAccessControlLevel(inCnx->server);

    if (hkAccessControlEnabled) {
        if (hkAccessControlLevel != kAirPlayHomeKitAccessControlLevel_EveryoneOnNetwork && !inCnx->pairingVerified) {
            aprs_dlog(kLogLevelNotice, "### Rejecting request--AccessControlLevel %d encryption not set up\n", hkAccessControlLevel);
            return kHTTPStatus_Forbidden;
        }

        if (_requestRequiresHKAdmin(inRequest)) {
            if (inCnx->pairingVerifiedAsHomeKitAdmin) {
                status = kHTTPStatus_OK;
            } else {
                const char* const pathPtr = inRequest->header.url.pathPtr;
                size_t const pathLen = inRequest->header.url.pathLen;

                aprs_ulog(kLogLevelNotice, "### Rejecting HK admin request %.*s -- Home pair-verify %d Admin pair-verify %d\n", pathLen, pathPtr, inCnx->pairingVerifiedAsHomeKit, inCnx->pairingVerifiedAsHomeKitAdmin);
                status = kHTTPStatus_Forbidden;
            }
            return status;
        }

        switch (hkAccessControlLevel) {
        //If we're in 'On Network' mode, being paired is sufficient. However not being paired is not insufficient (password auth, etc.)
        case kAirPlayHomeKitAccessControlLevel_EveryoneOnNetwork:
            if (inCnx->pairingVerified || inCnx->pairingVerifiedAsHomeKit || inCnx->pairingVerifiedAsHomeKitAdmin || inCnx->httpAuthentication_IsAuthenticated) {
                status = kHTTPStatus_OK;
            } else {
                char playPassword[256] = { 0 };
                AirPlayReceiverServerPlatformGetCString(inCnx->server, CFSTR(kAirPlayProperty_Password), NULL, playPassword, sizeof(playPassword), NULL);
                if (playPassword[0] != '\0') {
                    status = _verifyUserPassword(inCnx, inRequest);
                } else {
                    inCnx->httpAuthentication_IsAuthenticated = true;
                    status = kHTTPStatus_OK;
                }
            }
            break;

        //If we're in 'Home Only' (or admin) mode, not being verified is insufficient.
        case kAirPlayHomeKitAccessControlLevel_Standard:
            status = (inCnx->pairingVerifiedAsHomeKit || inCnx->pairingVerifiedAsHomeKitAdmin) ? kHTTPStatus_OK : kHTTPStatus_Forbidden;
            break;

        case kAirPlayHomeKitAccessControlLevel_AdminsOnly:
            status = inCnx->pairingVerifiedAsHomeKitAdmin ? kHTTPStatus_OK : kHTTPStatus_Forbidden;
            break;

        default:
            status = kHTTPStatus_Forbidden;
            aprs_dlog(kLogLevelTrace, "### Unhandled hkAccessControlLevel:%d\n", hkAccessControlLevel);
            break;
        }

        if (status == kHTTPStatus_Forbidden) {
            aprs_ulog(kLogLevelNotice, "### HK auth denied HKAccessControlLevel:%d  pairingVerifiedAsHomeKitAdmin:%d pairingVerifiedAsHomeKit:%d pairingVerified:%d\n", hkAccessControlLevel, inCnx->pairingVerifiedAsHomeKitAdmin, inCnx->pairingVerifiedAsHomeKit, inCnx->pairingVerified);
        }
    } else if (!inCnx->pairingVerified && !inCnx->httpAuthentication_IsAuthenticated) // && inCnx->authSetupSuccessful )
    {
        char playPassword[256] = { 0 };
        AirPlayReceiverServerPlatformGetCString(inCnx->server, CFSTR(kAirPlayProperty_Password), NULL, playPassword, sizeof(playPassword), NULL);
        if (playPassword[0] != '\0') {
            status = _verifyUserPassword(inCnx, inRequest);
        } else {
            status = kHTTPStatus_OK;
            inCnx->httpAuthentication_IsAuthenticated = true;
        }
    } else {
        status = inCnx->pairingVerified || (inCnx->authSetupSuccessful && inCnx->httpAuthentication_IsAuthenticated) ? kHTTPStatus_OK : kHTTPStatus_Forbidden;
    }

    return status;
}

static HTTPStatus _verifyUserPassword(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    static pthread_mutex_t gAuthThrottleLock = PTHREAD_MUTEX_INITIALIZER;
    static uint32_t gAuthThrottleCounter1 = 0;
    static uint32_t gAuthThrottleCounter2 = 0;
    static uint64_t gAuthThrottleStartTicks = 0;

    const char* const methodPtr = inRequest->header.methodPtr;
    size_t const methodLen = inRequest->header.methodLen;
    HTTPStatus status = kAuthenticationErr;

    HTTPAuthorizationInfo authInfo;

    memset(&authInfo, 0, sizeof(authInfo));
    authInfo.serverScheme = kHTTPAuthorizationScheme_Digest;
    authInfo.copyPasswordFunction = _requestHTTPAuthorization_CopyPassword;
    authInfo.copyPasswordContext = inCnx;
    authInfo.isValidNonceFunction = _requestHTTPAuthorization_IsValidNonce;
    authInfo.isValidNonceContext = inCnx;
    authInfo.headerPtr = inRequest->header.buf;
    authInfo.headerLen = inRequest->header.len;
    authInfo.requestMethodPtr = methodPtr;
    authInfo.requestMethodLen = methodLen;
    authInfo.requestURLPtr = inRequest->header.urlPtr;
    authInfo.requestURLLen = inRequest->header.urlLen;
    status = HTTPVerifyAuthorization(&authInfo);

    if (status != kHTTPStatus_OK) {
        if (authInfo.badMatch) {
            uint64_t ticks, nextTicks;

            pthread_mutex_lock(&gAuthThrottleLock);
            ticks = UpTicks();
            if (gAuthThrottleStartTicks == 0)
                gAuthThrottleStartTicks = ticks;
            if (gAuthThrottleCounter1 < 10) {
                ++gAuthThrottleCounter1;
            } else {
                gAuthThrottleCounter2 = gAuthThrottleCounter2 ? (gAuthThrottleCounter2 * 2) : 1;
                if (gAuthThrottleCounter2 > 10800)
                    gAuthThrottleCounter2 = 10800; // 3 hour cap.
                nextTicks = gAuthThrottleStartTicks + (gAuthThrottleCounter2 * UpTicksPerSecond());
                if (nextTicks > ticks) {
                    pthread_mutex_unlock(&gAuthThrottleLock);
                    status = kHTTPStatus_NotEnoughBandwidth;
                    goto exit;
                }
            }
            pthread_mutex_unlock(&gAuthThrottleLock);
        }

        goto exit;
    }
    pthread_mutex_lock(&gAuthThrottleLock);
    gAuthThrottleCounter1 = 0;
    gAuthThrottleCounter2 = 0;
    gAuthThrottleStartTicks = 0;
    pthread_mutex_unlock(&gAuthThrottleLock);

    inCnx->httpAuthentication_IsAuthenticated = true;

exit:
    return status;
}

//===========================================================================================================================
//	_requestHTTPAuthorization_CopyPassword
//===========================================================================================================================

static HTTPStatus _requestHTTPAuthorization_CopyPassword(HTTPAuthorizationInfoRef inInfo, char** outPassword)
{
    HTTPStatus status;
    char* str;
    char playPassword[256] = { 0 };
    AirPlayReceiverConnectionRef cnx = (AirPlayReceiverConnectionRef)inInfo->copyPasswordContext;

    AirPlayReceiverServerGetCString(cnx->server, CFSTR(kAirPlayProperty_Password), NULL, playPassword, sizeof(playPassword), NULL);

    str = strdup(playPassword);
    require_action(str, exit, status = kHTTPStatus_InternalServerError);
    *outPassword = str;
    status = kHTTPStatus_OK;

exit:
    return (status);
}

//===========================================================================================================================
//	_requestHTTPAuthorization_IsValidNonce
//===========================================================================================================================

static Boolean _requestHTTPAuthorization_IsValidNonce(HTTPAuthorizationInfoRef inInfo)
{
    AirPlayReceiverConnectionRef cnx = (AirPlayReceiverConnectionRef)inInfo->isValidNonceContext;
    OSStatus err;

    err = HTTPVerifyTimedNonce(inInfo->requestNoncePtr, inInfo->requestNonceLen, 30,
        kHTTPTimedNonceETagPtr, kHTTPTimedNonceETagLen,
        cnx->server->httpTimedNonceKey, sizeof(cnx->server->httpTimedNonceKey));
    return (err == kNoErr);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_GetTypeID
//===========================================================================================================================

static void _GetTypeID(void* inContext)
{
    (void)inContext;

    gAirPlayReceiverServerTypeID = _CFRuntimeRegisterClass(&kAirPlayReceiverServerClass);
    check(gAirPlayReceiverServerTypeID != _kCFRuntimeNotATypeID);
}

#if (0 || (TARGET_OS_POSIX && DEBUG))
//===========================================================================================================================
//	_HandleDebug
//===========================================================================================================================

static OSStatus _HandleDebug(CFDictionaryRef inRequest, CFDictionaryRef* outResponse, void* inContext)
{
    OSStatus err;
    CFStringRef opcode;
    DataBuffer dataBuf;
    CFMutableDictionaryRef response;

    (void)inContext;

    DataBuffer_Init(&dataBuf, NULL, 0, 10 * kBytesPerMegaByte);
    response = NULL;

    opcode = (CFStringRef)CFDictionaryGetValue(inRequest, kDebugIPCKey_Command);
    require_action_quiet(opcode, exit, err = kNotHandledErr);
    require_action(CFIsType(opcode, CFString), exit, err = kTypeErr);

    if (0) {
    } // Empty if to simplify else if's below.

    // Show

    else if (CFEqual(opcode, kDebugIPCOpCode_Show)) {
        int verbose;

        verbose = (int)CFDictionaryGetInt64(inRequest, CFSTR("verbose"), NULL);

        DataBuffer_AppendF(&dataBuf, " verbose=%d\n", verbose);
        err = CFPropertyListCreateFormatted(NULL, &response, "{%kO=%.*s}",
            kDebugIPCKey_Value, (int)dataBuf.bufferLen, dataBuf.bufferPtr);
        require_noerr(err, exit);
    }

    // Other

    else {
        aprs_dlog(kLogLevelNotice, "### Unsupported debug command: %@\n", opcode);
        err = kNotHandledErr;
        goto exit;
    }

    if (response)
        CFDictionarySetValue(response, kDebugIPCKey_ResponseType, opcode);
    *outResponse = response;
    response = NULL;
    err = kNoErr;

exit:
    DataBuffer_Free(&dataBuf);
    if (response)
        CFRelease(response);
    return (err);
}
#endif //( 0 || ( TARGET_OS_POSIX && DEBUG ) )

//===========================================================================================================================
//	_GlobalInitialize
//
//	Perform one-time initialization of things global to the entire process.
//===========================================================================================================================

static void _GlobalInitialize(void* inContext)
{
    (void)inContext;

#if (TARGET_OS_POSIX)
    signal(SIGPIPE, SIG_IGN); // Ignore SIGPIPE signals so we get EPIPE errors from APIs instead of a signal.
#endif
    MFiPlatform_Initialize(); // Initialize at app startup to cache cert to speed up first time connect.

    // Setup logging.

    AirPlayReceiverServerSetLogLevel();
    AirPlayReceiverLogRequestInit();

#if (TARGET_OS_POSIX && DEBUG)
    DebugIPC_EnsureInitialized(_HandleDebug, NULL);
#endif
}

//===========================================================================================================================
//	_InitializeConfig
//===========================================================================================================================

static OSStatus _InitializeConfig(AirPlayReceiverServerRef inServer)
{
    OSStatus err;
    CFDataRef data;
#if TARGET_OS_WIN32 && UNICODE
    int unicodeFlags, requiredBytes;
    CFMutableDataRef tempData;
#endif
    CFDictionaryRef config = NULL;
#if (defined(LEGACY_REGISTER_SCREEN_HID))
    CFArrayRef array;
    CFIndex i, n;
    CFDictionaryRef dict;
#endif

    (void)inServer;

    // Read the config file (if it exists). Try binary plist format first since it has a unique signature to detect a
    // valid file. If it's a binary plist then parse it as an INI file and convert it to a dictionary.

    data = CFDataCreateWithFilePath(inServer->configFilePath, NULL);
    require_action_quiet(data, exit, err = kNotFoundErr);

#if TARGET_OS_WIN32 && UNICODE
    unicodeFlags = IS_TEXT_UNICODE_REVERSE_SIGNATURE | IS_TEXT_UNICODE_SIGNATURE;
    IsTextUnicode(CFDataGetBytePtr(data), CFDataGetLength(data), &unicodeFlags);

    // The return value of IsTextUnicode isn't reliable... all we're interested in is the presence of a BOM,
    // which is correctly reported via the output flags regardless of the return value
    if (unicodeFlags) {
        // WideCharToMultiByte only supports native endian
        require_action(!(unicodeFlags & IS_TEXT_UNICODE_REVERSE_SIGNATURE), exit, err = kUnsupportedDataErr);

        requiredBytes = WideCharToMultiByte(CP_UTF8, 0, ((LPCWSTR)CFDataGetBytePtr(data)) + 1, (int)(CFDataGetLength(data) / sizeof(WCHAR)) - 1, NULL, 0, NULL, NULL);
        require_action(requiredBytes, exit, err = kUnsupportedDataErr);

        tempData = CFDataCreateMutable(kCFAllocatorDefault, requiredBytes);
        require_action(tempData, exit, err = kNoMemoryErr);

        CFDataSetLength(tempData, requiredBytes);
        WideCharToMultiByte(CP_UTF8, 0, ((LPCWSTR)CFDataGetBytePtr(data)) + 1, (int)(CFDataGetLength(data) / sizeof(WCHAR)) - 1, (LPSTR)CFDataGetMutableBytePtr(tempData), requiredBytes, NULL, NULL);
        CFRelease(data);
        data = tempData;
        tempData = NULL;
    }
#endif

    config = (CFDictionaryRef)CFPropertyListCreateWithData(NULL, data, kCFPropertyListImmutable, NULL, NULL);
    if (!config) {
        config = CFDictionaryCreateWithINIBytes(CFDataGetBytePtr(data), (size_t)CFDataGetLength(data),
            kINIFlag_MergeGlobals, CFSTR(kAirPlayKey_Name), NULL);
    }
    CFRelease(data);
    if (config && !CFIsType(config, CFDictionary)) {
        dlogassert("Bad type for config file: %s", inServer->configFilePath);
        CFRelease(config);
        config = NULL;
    }

#if (defined(LEGACY_REGISTER_SCREEN_HID))
    // Register static HID devices.

    array = config ? CFDictionaryGetCFArray(config, CFSTR(kAirPlayKey_HIDDevices), NULL) : NULL;
    if (!array)
        array = config ? CFDictionaryGetCFArray(config, CFSTR(kAirPlayKey_HIDDevice), NULL) : NULL;
    n = array ? CFArrayGetCount(array) : 0;
    for (i = 0; i < n; ++i) {
        HIDDeviceRef hid;

        dict = CFArrayGetCFDictionaryAtIndex(array, i, NULL);
        check(dict);
        if (!dict)
            continue;

        err = HIDDeviceCreateVirtual(&hid, dict);
        check_noerr(err);
        if (err)
            continue;

        err = HIDRegisterDevice(hid);
        CFRelease(hid);
        check_noerr(err);
    }

    // Register static displays.

    array = config ? CFDictionaryGetCFArray(config, CFSTR(kAirPlayKey_Displays), NULL) : NULL;
    if (!array)
        array = config ? CFDictionaryGetCFArray(config, CFSTR(kAirPlayKey_Display), NULL) : NULL;
    n = array ? CFArrayGetCount(array) : 0;
    for (i = 0; i < n; ++i) {
        ScreenRef screen;

        dict = CFArrayGetCFDictionaryAtIndex(array, i, NULL);
        check(dict);
        if (!dict)
            continue;

        err = ScreenCreate(&screen, dict);
        check_noerr(err);
        if (err)
            continue;

        err = ScreenRegister(screen);
        CFRelease(screen);
        check_noerr(err);
    }
#endif

    // Save off the config dictionary for future use.

    inServer->config = config;
    config = NULL;
    err = kNoErr;

exit:
    // There is a significant amount of code that assumes there is always a config dictionary, so create an empty one in case of error.
    if (err)
        inServer->config = CFDictionaryCreate(NULL, NULL, NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFReleaseNullSafe(config);
    return (err);
}

//===========================================================================================================================
//	_Finalize
//===========================================================================================================================

static void _Finalize(CFTypeRef inCF)
{
    AirPlayReceiverServerRef const me = (AirPlayReceiverServerRef)inCF;

    _StopServers(me);
    AirPlayReceiverServerPlatformFinalize(me);

    dispatch_forget(&me->queue);
    dispatch_forget(&me->httpQueue);
    ForgetMem(&me->configFilePath);
    ForgetCF(&me->config);
    ForgetCF(&me->multiZoneAudioHint);
    ForgetCF(&me->multiZoneDeviceID);
}

//===========================================================================================================================
//	_UpdateDeviceID
//===========================================================================================================================

static void _UpdateDeviceID(AirPlayReceiverServerRef inServer)
{
    OSStatus err = kNoErr;
    int i;
    CFTypeRef obj = NULL;

    // DeviceID

    if (inServer->multiZoneDeviceID) {
        CFGetMACAddress(inServer->multiZoneDeviceID, inServer->deviceID, &err);
        require_noerr_action(err, exit, exit(1));
    } else {
        obj = AirPlayReceiverServerPlatformCopyProperty(inServer, kCFObjectFlagDirect, CFSTR(kAirPlayProperty_DeviceID), NULL, NULL);
        if (obj) {
            CFGetMACAddress(obj, inServer->deviceID, &err);
        }
        if (!obj || err) {
            for (i = 1; i < 10; ++i) {
                uint64_t u64 = GetPrimaryMACAddress(inServer->deviceID, NULL);
                if (u64 != 0)
                    break;
                sleep(1);
            }
        }
        CFReleaseNullSafe(obj);
    }
exit:
    return;
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_BonjourRegistrationHandler
//===========================================================================================================================

static void DNSSD_API
_BonjourRegistrationHandler(
    DNSServiceRef inRef,
    DNSServiceFlags inFlags,
    DNSServiceErrorType inError,
    const char* inName,
    const char* inType,
    const char* inDomain,
    void* inContext)
{
    AirPlayReceiverServerRef const server = (AirPlayReceiverServerRef)inContext;
    DNSServiceRef* servicePtr = NULL;
    const char* serviceType = "?";

    (void)inFlags;

    if (inRef == server->bonjourAirPlay) {
        servicePtr = &server->bonjourAirPlay;
        serviceType = kAirPlayBonjourServiceType;
    }

    if (inError == kDNSServiceErr_ServiceNotRunning) {
        aprs_ulog(kLogLevelNotice, "### Bonjour server crashed for %s\n", serviceType);
        if (servicePtr)
            DNSServiceForget(servicePtr);
        _UpdateBonjour(server);
    } else if (inError) {
        aprs_ulog(kLogLevelNotice, "### Bonjour registration error for %s: %#m\n", serviceType, inError);
    } else {
        aprs_ulog(kLogLevelNotice, "Registered Bonjour %s.%s%s\n", inName, inType, inDomain);
    }
}

//===========================================================================================================================
//	_RestartBonjour
//===========================================================================================================================

static void _RestartBonjour(AirPlayReceiverServerRef inServer)
{
    inServer->bonjourRestartPending = false;

    // Ignore if we've been disabled.

    if (!inServer->started) {
        aprs_dlog(kLogLevelNotice, "Ignoring Bonjour restart while disabled\n");
        goto exit;
    }

    // Ignore if Bonjour hasn't been started.

    if (!inServer->bonjourAirPlay) {
        aprs_dlog(kLogLevelNotice, "Ignoring Bonjour restart since Bonjour wasn't started\n");
        goto exit;
    }

    // Some clients stop resolves after ~2 minutes to reduce multicast traffic so if we changed something in the
    // TXT record, such as the password state, those clients wouldn't be able to detect that state change.
    // To work around this, completely remove the Bonjour service, wait 2 seconds to give time for Bonjour to
    // flush out the removal then re-add the Bonjour service so client will re-resolve it.

    _StopBonjour(inServer, "restart");

    aprs_dlog(kLogLevelNotice, "Waiting for 2 seconds for Bonjour to remove service\n");
    sleep(2);

    aprs_dlog(kLogLevelNotice, "Starting Bonjour service for restart\n");
    _UpdateBonjour(inServer);

exit:
    return;
}

//===========================================================================================================================
//	_RetryBonjour
//===========================================================================================================================

static void _RetryBonjour(void* inArg)
{
    AirPlayReceiverServerRef const server = (AirPlayReceiverServerRef)inArg;

    aprs_ulog(kLogLevelNotice, "Retrying Bonjour after failure\n");
    dispatch_source_forget(&server->bonjourRetryTimer);
    server->bonjourStartTicks = 0;
    _UpdateBonjour(server);
}

//===========================================================================================================================
//	_StopBonjour
//===========================================================================================================================

static void _StopBonjour(AirPlayReceiverServerRef inServer, const char* inReason)
{
    dispatch_source_forget(&inServer->bonjourRetryTimer);
    inServer->bonjourStartTicks = 0;

    if (inServer->bonjourAirPlay) {
        DNSServiceForget(&inServer->bonjourAirPlay);
        aprs_ulog(kLogLevelNotice, "Deregistered Bonjour %s for %s\n", kAirPlayBonjourServiceType, inReason);
    }

#if (AIRPLAY_RAOP_SERVER)
    if (inServer->bonjourRAOP) {
        DNSServiceForget(&inServer->bonjourRAOP);
        aprs_ulog(kLogLevelNotice, "Deregistered Bonjour %s for %s\n", kAirPlayBonjourServiceType, inReason);
    }
#endif
}

//===========================================================================================================================
//	_UpdateBonjour
//
//	Update all Bonjour services.
//===========================================================================================================================

static void _UpdateBonjour(AirPlayReceiverServerRef inServer)
{
    OSStatus err, firstErr = kNoErr;
    dispatch_source_t timer;
    Boolean activated = true;
    uint64_t ms;

    if (!inServer->serversStarted || !activated) {
        _StopBonjour(inServer, "disable");
        return;
    }
    if (inServer->bonjourStartTicks == 0)
        inServer->bonjourStartTicks = UpTicks();

    err = _UpdateBonjourAirPlay(inServer, NULL);
    if (!firstErr)
        firstErr = err;

#if (AIRPLAY_RAOP_SERVER)
    err = _UpdateBonjourRAOP(inServer, NULL);
    if (!firstErr)
        firstErr = err;
#endif

    if (!firstErr) {
        dispatch_source_forget(&inServer->bonjourRetryTimer);
    } else if (!inServer->bonjourRetryTimer) {
        ms = UpTicksToMilliseconds(UpTicks() - inServer->bonjourStartTicks);
        ms = (ms < 11113) ? (11113 - ms) : 1; // Use 11113 to avoid being syntonic with 10 second re-launching.
        aprs_ulog(kLogLevelNotice, "### Bonjour failed, retrying in %llu ms: %#m\n", ms, firstErr);

        inServer->bonjourRetryTimer = timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, inServer->queue);
        check(timer);
        if (timer) {
            dispatch_set_context(timer, inServer);
            dispatch_source_set_event_handler_f(timer, _RetryBonjour);
            dispatch_source_set_timer(timer, dispatch_time_milliseconds(ms), INT64_MAX, kNanosecondsPerSecond);
            dispatch_resume(timer);
        }
    }
}

//===========================================================================================================================
//	_UpdateBonjourAirPlay
//
//	Update Bonjour for the _airplay._tcp service.
//===========================================================================================================================

static OSStatus _UpdateBonjourAirPlay(AirPlayReceiverServerRef inServer, CFDataRef* outTXTRecord)
{
    OSStatus err;
    TXTRecordRef txtRec;
    uint8_t txtBuf[1024];
    const uint8_t* txtPtr;
    uint16_t txtLen;
    char cstr[256];
    char* ptr;
    uint8_t pk[32];
    int n;
    uint32_t u32;
    uint64_t u64;
    uint16_t port;
    DNSServiceFlags flags;
    const char* domain;
    uint32_t ifindex;
    CFStringRef str;

    TXTRecordCreate(&txtRec, (uint16_t)sizeof(txtBuf), txtBuf);

    // Access control level
    u64 = (uint64_t)HIGetAccessControlLevel(inServer);
    u32 = (uint32_t)u64;
    n = snprintf(cstr, sizeof(cstr), "%u", u32);
    err = TXTRecordSetValue(&txtRec, kAirPlayTXTKey_AccessControlLevel, (uint8_t)n, cstr);
    require_noerr(err, exit);

    // DeviceID

    MACAddressToCString(inServer->deviceID, cstr);
    err = TXTRecordSetValue(&txtRec, kAirPlayTXTKey_DeviceID, (uint8_t)strlen(cstr), cstr);
    require_noerr(err, exit);

    // Features

    err = _txtRecordSetUInt64(&txtRec, kAirPlayTXTKey_Features, AirPlayGetFeatures(inServer));
    require_noerr(err, exit);

    // Required Sender Features

    err = _txtRecordSetUInt64(&txtRec, kAirPlayTXTKey_RequiredSenderFeatures, AirPlayGetRequiredSenderFeatures(inServer));
    require_noerr(err, exit);

    // FirmwareRevision

    *cstr = '\0';
    AirPlayGetConfigCString(inServer, CFSTR(kAirPlayKey_FirmwareRevision), cstr, sizeof(cstr), NULL);
    if (*cstr == '\0') {
        AirPlayReceiverServerPlatformGetCString(inServer, CFSTR(kAirPlayKey_FirmwareRevision), NULL,
            cstr, sizeof(cstr), NULL);
    }

    if (*cstr != '\0') {
        CFStringRef firmwareVersion = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("p%d.%s"), KAirPlayPosixSourceVersion, cstr);
        require_action(firmwareVersion, exit, err = kNoMemoryErr);

        err = _txtRecordSetCFString(&txtRec, kAirPlayTXTKey_FirmwareVersion, firmwareVersion);
        CFRelease(firmwareVersion);
        require_noerr(err, exit);
    }

    // Flags

    u32 = AirPlayGetStatusFlags(inServer);
    if (u32 != 0) {
        n = snprintf(cstr, sizeof(cstr), "0x%x", u32);
        err = TXTRecordSetValue(&txtRec, kAirPlayTXTKey_Flags, (uint8_t)n, cstr);
        require_noerr(err, exit);
    }

    // Model

    *cstr = '\0';
    AirPlayGetDeviceModel(inServer, cstr, sizeof(cstr));
    if (*cstr != '\0') {
        err = TXTRecordSetValue(&txtRec, kAirPlayTXTKey_Model, (uint8_t)strlen(cstr), cstr);
        require_noerr(err, exit);
    }

    // Manufacturer

    str = (CFStringRef)AirPlayReceiverServerPlatformCopyProperty(inServer, kCFObjectFlagDirect,
        CFSTR(kAirPlayProperty_Manufacturer),
        NULL, NULL);
    err = _txtRecordSetCFString(&txtRec, kAirPlayKey_Manufacturer, str ? str : CFSTR(""));
    CFReleaseNullSafe(str);
    require_noerr(err, exit);

    // Serial Number

    str = (CFStringRef)AirPlayReceiverServerPlatformCopyProperty(inServer, kCFObjectFlagDirect,
        CFSTR(kAirPlayProperty_SerialNumber),
        NULL, NULL);
    err = _txtRecordSetCFString(&txtRec, kAirPlayKey_SerialNumber, str ? str : CFSTR(""));
    CFReleaseNullSafe(str);
    require_noerr(err, exit);

    // Protocol version

    ptr = kAirPlayProtocolVersionStr;
    if (strcmp(ptr, "1.0") != 0) {
        err = TXTRecordSetValue(&txtRec, kAirPlayTXTKey_ProtocolVersion, (uint8_t)strlen(ptr), ptr);
        require_noerr(err, exit);
    }

    // Source Version

    ptr = kAirPlaySourceVersionStr;
    err = TXTRecordSetValue(&txtRec, kAirPlayTXTKey_SourceVersion, (uint8_t)strlen(ptr), ptr);
    require_noerr(err, exit);

    // HK pairing identity
    ptr = NULL;
    err = AirPlayCopyHomeKitPairingIdentity(inServer, &ptr, pk);
    require_noerr_action(err, exit, FreeNullSafe(ptr));

    err = TXTRecordSetValue(&txtRec, kAirPlayTXTKey_PublicHKID, strlen(ptr), ptr);
    require_noerr_action(err, exit, FreeNullSafe(ptr));

    /* ptr still contain the public key - since we don't have system pairing identifier this is the our group id. */
    if (inServer->group.uuid) {
        err = _txtRecordSetCFString(&txtRec, kAirPlayTXTKey_GroupUUID, inServer->group.uuid);
        require_noerr_action(err, exit, FreeNullSafe(ptr));

        err = _txtRecordSetBoolean(&txtRec, kAirPlayTXTKey_GroupContainsGroupLeader, inServer->group.containsLeader);
        require_noerr_action(err, exit, FreeNullSafe(ptr));

        err = _txtRecordSetBoolean(&txtRec, kAirPlayTXTKey_IsGroupLeader, inServer->group.isGroupLeader);
        require_noerr_action(err, exit, FreeNullSafe(ptr));
    } else {
        err = TXTRecordSetValue(&txtRec, kAirPlayTXTKey_GroupUUID, strlen(ptr), ptr);
        require_noerr_action(err, exit, FreeNullSafe(ptr));

        err = _txtRecordSetBoolean(&txtRec, kAirPlayTXTKey_GroupContainsGroupLeader, false);
        require_noerr_action(err, exit, FreeNullSafe(ptr));
    }

    if (inServer->powerState == kPTPPowerStateUndefined) {
        CFNumberRef ptpPowerState = (CFNumberRef)AirPlayReceiverServerPlatformCopyProperty(inServer, kCFObjectFlagDirect,
            CFSTR(kAirPlayProperty_PTPPowerState),
            NULL, NULL);
        if (ptpPowerState == NULL || !CFNumberGetValue(ptpPowerState, kCFNumberSInt8Type, &inServer->powerState))
            inServer->powerState = kPTPPowerStateNoBattery;

        CFReleaseNullSafe(ptpPowerState);
    }

    if (inServer->powerState != kPTPPowerStateNoBattery) {
        CFDictionaryRef bs = (CFDictionaryRef)AirPlayReceiverServerPlatformCopyProperty(inServer, kCFObjectFlagDirect,
            CFSTR(kAirPlayProperty_BatteryState), NULL, NULL);
        if (bs != NULL) {
            n = snprintf(cstr, sizeof(cstr), "%d,%d,%d",
                CFDictionaryGetBoolean(bs, CFSTR(kAirPlayKey_BatteryWallPowered), NULL),
                CFDictionaryGetBoolean(bs, CFSTR(kAirPlayKey_BatteryIsChargig), NULL),
                (uint8_t)CFDictionaryGetUInt8(bs, CFSTR(kAirPlayKey_BatteryPercentRemaining), NULL));
            CFRelease(bs);
            (void)TXTRecordSetValue(&txtRec, kAirPlayTXTKey_BatteryInfo, (uint8_t)n, cstr);
        }
    }

    FreeNullSafe(ptr);

    // Public key:

    MemZeroSecure(cstr, sizeof(cstr));
    DataToHexCString(pk, 32, cstr);
    err = TXTRecordSetValue(&txtRec, kAirPlayTXTKey_PublicKey, (uint8_t)strlen(cstr), cstr);
    require_noerr(err, exit);

    // If we're only building the TXT record then return it and exit without registering.

    txtPtr = TXTRecordGetBytesPtr(&txtRec);
    txtLen = TXTRecordGetLength(&txtRec);
    if (outTXTRecord) {
        CFDataRef data;

        data = CFDataCreate(NULL, txtPtr, txtLen);
        require_action(data, exit, err = kNoMemoryErr);
        *outTXTRecord = data;
        goto exit;
    }

    // Update the service.

    if (inServer->bonjourAirPlay) {
        err = DNSServiceUpdateRecord(inServer->bonjourAirPlay, NULL, 0, txtLen, txtPtr, 0);
        if (!err)
            aprs_ulog(kLogLevelNotice, "Updated Bonjour TXT for %s\n", kAirPlayBonjourServiceType);
        else {
            // Update record failed or isn't supported so deregister to force a re-register below.

            aprs_ulog(kLogLevelNotice, "Bonjour TXT update failed for %s, deregistering so we can force a re-register %s\n",
                kAirPlayBonjourServiceType);
            DNSServiceForget(&inServer->bonjourAirPlay);
        }
    }
    if (!inServer->bonjourAirPlay) {

        ptr = NULL;
        *cstr = '\0';
        AirPlayGetDeviceName(inServer, cstr, sizeof(cstr));
        ptr = cstr;

        flags = 0;
        domain = kAirPlayBonjourServiceDomain;

        port = (uint16_t)_GetListenPort(inServer);
        require_action(port > 0, exit, err = kInternalErr);

        ifindex = kDNSServiceInterfaceIndexAny;
        if (*inServer->ifname != '\0') {
            ifindex = if_nametoindex(inServer->ifname);
            require_action_quiet(ifindex != 0, exit, err = kNotFoundErr);
        }
        err = DNSServiceRegister(&inServer->bonjourAirPlay, flags, ifindex, ptr, kAirPlayBonjourServiceType, domain, NULL,
            htons(port), txtLen, txtPtr, _BonjourRegistrationHandler, inServer);
        require_noerr_quiet(err, exit);

        aprs_ulog(kLogLevelNotice, "Registering Bonjour %s port %d\n", kAirPlayBonjourServiceType, port);
    }

exit:
    TXTRecordDeallocate(&txtRec);
    return (err);
}

#if (AIRPLAY_RAOP_SERVER)
//===========================================================================================================================
//	_UpdateBonjourRAOP
//
//	Update Bonjour for the _raop._tcp server.
//===========================================================================================================================

static OSStatus _UpdateBonjourRAOP(AirPlayReceiverServerRef inServer, CFDataRef* outTXTRecord)
{
    OSStatus err;
    TXTRecordRef txtRec;
    uint8_t txtBuf[1024];
    const uint8_t* txtPtr;
    uint16_t txtLen;
    char str[256];
    const char* ptr;
    uint8_t pk[32];
    size_t len;
    uint32_t u32;
    int n;
    AirPlayFeatures features;
    uint8_t deviceID[6];
    const uint8_t* a;
    uint16_t port;
    uint32_t flags;
    const char* domain;
    uint32_t ifindex;

    TXTRecordCreate(&txtRec, (uint16_t)sizeof(txtBuf), txtBuf);

    // CompressionTypes

    u32 = kAirPlayCompressionType_PCM;
    u32 |= kAirPlayCompressionType_ALAC;
    BitListString_Make(u32, str, &len);
    err = TXTRecordSetValue(&txtRec, kRAOPTXTKey_CompressionTypes, (uint8_t)len, str);
    require_noerr(err, exit);

    // Digest Auth from RFC-2617 (i.e. use lowercase hex digits)

    err = TXTRecordSetValue(&txtRec, kRAOPTXTKey_RFC2617DigestAuth, sizeof_string("true"), "true");
    require_noerr(err, exit);

    // Encryption Types

    u32 = kAirPlayEncryptionType_None;
    u32 |= kAirPlayEncryptionType_MFi_SAPv1;
    BitListString_Make(u32, str, &len);
    err = TXTRecordSetValue(&txtRec, kRAOPTXTKey_EncryptionTypes, (uint8_t)len, str);
    require_noerr(err, exit);

    // Features
    features = AirPlayGetFeatures(inServer);
    err = _txtRecordSetUInt64(&txtRec, kRAOPTXTKey_Features, features);
    require_noerr(err, exit);

    // FirmwareRevision

    *str = '\0';
    {
        AirPlayReceiverServerPlatformGetCString(inServer, CFSTR(kAirPlayKey_FirmwareRevision), NULL,
            str, sizeof(str), NULL);
    }
    if (*str != '\0') {
        CFStringRef firmwareVersion = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("p%d.%s"), KAirPlayPosixSourceVersion, str);
        require_action(firmwareVersion, exit, err = kNoMemoryErr);

        err = _txtRecordSetCFString(&txtRec, kRAOPTXTKey_FirmwareSourceVersion, firmwareVersion);
        CFRelease(firmwareVersion);
        require_noerr(err, exit);
    }
    // MetaDataTypes -- Deprecated as of Mac OS X 10.9 and iOS 7.0 and later, but needed for older senders.

    u32 = kAirTunesMetaDataType_Progress;
#if (AIRPLAY_META_DATA)
    if (features & kAirPlayFeature_AudioMetaDataArtwork)
        u32 |= kAirTunesMetaDataType_Artwork;
    if (features & kAirPlayFeature_AudioMetaDataText)
        u32 |= kAirTunesMetaDataType_Text;
#endif
    BitListString_Make(u32, str, &len);
    err = TXTRecordSetValue(&txtRec, kRAOPTXTKey_MetaDataTypes, (uint8_t)len, str);
    require_noerr(err, exit);

    // Model

    *str = '\0';
    AirPlayGetDeviceModel(inServer, str, sizeof(str));
    err = TXTRecordSetValue(&txtRec, kRAOPTXTKey_AppleModel, (uint8_t)strlen(str), str);
    require_noerr(err, exit);

    // PasswordRequired
    char playPassword[256] = { 0 };
    AirPlayReceiverServerPlatformGetCString(inServer, CFSTR(kAirPlayProperty_Password), NULL, playPassword, sizeof(playPassword), NULL);

    if (!inServer->pinMode && *playPassword != '\0') {
        err = TXTRecordSetValue(&txtRec, kRAOPTXTKey_Password, sizeof_string("true"), "true");
        require_noerr(err, exit);
    }

    // StatusFlags

    u32 = AirPlayGetStatusFlags(inServer);
    if (u32 != 0) {
        n = snprintf(str, sizeof(str), "0x%x", u32);
        err = TXTRecordSetValue(&txtRec, kRAOPTXTKey_StatusFlags, (uint8_t)n, str);
        require_noerr(err, exit);
    }

    // TransportTypes (audio)

    err = TXTRecordSetValue(&txtRec, kRAOPTXTKey_TransportTypes, 3, "UDP");
    require_noerr(err, exit);

    // Version (protocol)

    u32 = ((uint32_t)kAirPlayProtocolMajorVersion << 16) + kAirPlayProtocolMinorVersion;
    n = snprintf(str, sizeof(str), "%d", u32);
    err = TXTRecordSetValue(&txtRec, kRAOPTXTKey_ProtocolVersion, (uint8_t)n, str);
    require_noerr(err, exit);

    // Version (source)
    ptr = kAirPlaySourceVersionStr;
    err = TXTRecordSetValue(&txtRec, kRAOPTXTKey_SourceVersion, (uint8_t)strlen(ptr), ptr);
    require_noerr(err, exit);

    // Public key
    err = AirPlayCopyHomeKitPairingIdentity(inServer, NULL, pk);
    require_noerr(err, exit);
    MemZeroSecure(str, sizeof(str));
    DataToHexCString(pk, 32, str);
    err = TXTRecordSetValue(&txtRec, kRAOPTXTKey_PublicKey, (uint8_t)strlen(str), str);
    require_noerr(err, exit);

    // If we're only building the TXT record then return it and exit without registering.

    txtPtr = TXTRecordGetBytesPtr(&txtRec);
    txtLen = TXTRecordGetLength(&txtRec);
    if (outTXTRecord) {
        CFDataRef data;

        data = CFDataCreate(NULL, txtPtr, txtLen);
        require_action(data, exit, err = kNoMemoryErr);
        *outTXTRecord = data;
        goto exit;
    }

    // Update the service.

    if (inServer->bonjourRAOP) {
        err = DNSServiceUpdateRecord(inServer->bonjourRAOP, NULL, 0, txtLen, txtPtr, 0);
        if (!err)
            aprs_ulog(kLogLevelNotice, "Updated Bonjour TXT for %s\n", kRAOPBonjourServiceType);
        else {
            // Update record failed or isn't supported so deregister to force a re-register below.

            DNSServiceForget(&inServer->bonjourRAOP);
        }
    }
    if (!inServer->bonjourRAOP) {
        // Publish the service. The name is in the format <mac address>@<speaker name> such as "001122AABBCC@My Device".

        AirPlayGetDeviceID(inServer, deviceID);
        a = deviceID;
        n = snprintf(str, sizeof(str), "%02X%02X%02X%02X%02X%02X@", a[0], a[1], a[2], a[3], a[4], a[5]);
        err = AirPlayGetDeviceName(inServer, &str[n], sizeof(str) - n);
        require_noerr(err, exit);

        flags = 0;
        domain = kRAOPBonjourServiceDomain;

        ifindex = kDNSServiceInterfaceIndexAny;
        if (*inServer->ifname != '\0') {
            ifindex = if_nametoindex(inServer->ifname);
            require_action_quiet(ifindex != 0, exit, err = kNotFoundErr);
        }
        port = (uint16_t)_GetListenPort(inServer);
        err = DNSServiceRegister(&inServer->bonjourRAOP, flags, ifindex, str, kRAOPBonjourServiceType, domain, NULL,
            htons(port), txtLen, txtPtr, _BonjourRegistrationHandler, inServer);
        require_noerr_quiet(err, exit);

        aprs_ulog(kLogLevelNotice, "Registering Bonjour %s.%s port %u\n", str, kRAOPBonjourServiceType, port);
    }

exit:
    TXTRecordDeallocate(&txtRec);
    return (err);
}
#endif // AIRPLAY_RAOP_SERVER

#if 0
#pragma mark -
#endif

static HTTPServerRef _CreateHTTPServerForPort(AirPlayReceiverServerRef inServer, int inListenPort)
{
    OSStatus err;
    HTTPServerRef httpServer;
    HTTPServerDelegate delegate;

    httpServer = NULL;

    HTTPServerDelegateInit(&delegate);
    delegate.context = inServer;
    delegate.connectionExtraLen = sizeof(struct AirPlayReceiverConnectionPrivate);
    delegate.connectionCreated_f = _HandleHTTPConnectionCreated;

    err = HTTPServerCreate(&httpServer, &delegate);
    require_noerr(err, exit);

    httpServer->listenPort = -(inListenPort);

    HTTPServerSetDispatchQueue(httpServer, inServer->httpQueue);
//HTTPServerSetLogging( inServer->httpServer, aprs_ucat() );
exit:
    return (httpServer);
}

//===========================================================================================================================
//	_StartServers
//===========================================================================================================================

static void _StartServers(AirPlayReceiverServerRef inServer)
{
    OSStatus err;

    if (inServer->serversStarted)
        return;

    if (!inServer->sessionMgr) {
        AirPlaySessionManagerCreate(inServer, inServer->httpQueue, NULL, &inServer->sessionMgr);
    }

    // Create the servers first.

    check(inServer->httpServer == NULL);
    uint64_t port = inServer->mediaPort;
    if (port == 0) {
        port = AirPlayReceiverServerGetInt64(inServer, CFSTR(kAirPlayKey_MediaControlPort), NULL, &err);
        if (err) {
            port = kAirPlayFixedPort_MediaControl;
        }
    }
    inServer->httpServer = _CreateHTTPServerForPort(inServer, (int)port);

#if (AIRPLAY_HTTP_SERVER_LEGACY)
    check(inServer->httpServerLegacy == NULL);
    inServer->httpServerLegacy = _CreateHTTPServerForPort(inServer, kAirPlayFixedPort_RTSPControl);
#endif

    // After all the servers are created, apply settings then start them (synchronously).

    err = HTTPServerStartSync(inServer->httpServer);
    check_noerr(err);

#if (AIRPLAY_HTTP_SERVER_LEGACY)
    err = HTTPServerStartSync(inServer->httpServerLegacy);
    check_noerr(err);
#endif

#if (AIRPLAY_DACP)
    check(inServer->dacpClient == NULL);
    err = AirTunesDACPClient_Create(&inServer->dacpClient);
    check_noerr(err);
    AirTunesDACPClient_SetCallBack(inServer->dacpClient, _HandleDACPStatus, inServer);
#endif

    inServer->serversStarted = true;
    _UpdateBonjour(inServer);

    aprs_ulog(kLogLevelNotice, "AirPlay servers started\n");
}

//===========================================================================================================================
//	_StopServers
//===========================================================================================================================

static void _StopServers(AirPlayReceiverServerRef inServer)
{
    _StopBonjour(inServer, "stop");

#if (AIRPLAY_DACP)
    ForgetCustom(&inServer->dacpClient, AirTunesDACPClient_Delete);
#endif

    if (inServer->httpServer) {
        HTTPServerStopSync(inServer->httpServer);
        ForgetCF(&inServer->httpServer);
    }

#if (AIRPLAY_HTTP_SERVER_LEGACY)
    HTTPServerForget(&inServer->httpServerLegacy);
#endif

    ForgetCF(&inServer->sessionMgr);

    if (inServer->serversStarted) {
        aprs_ulog(kLogLevelNotice, "AirPlay servers stopped\n");
        inServer->serversStarted = false;
    }
}

#if 0
#pragma mark -
#pragma mark == http server control ==
#endif

//===========================================================================================================================
//	_IsConnectionActive
//===========================================================================================================================

static Boolean _IsConnectionActive(HTTPConnectionRef inConnection)
{
    Boolean isActive = false;
    AirPlayReceiverConnectionRef aprsCnx = (AirPlayReceiverConnectionRef)inConnection->delegate.context;
    if (aprsCnx && aprsCnx->didAnnounce)
        isActive = true;
    return isActive;
}

//===========================================================================================================================
//	_HijackHTTPServerConnections
//
//	This function should be called on inServer->httpQueue.
//===========================================================================================================================

static void
_HijackHTTPServerConnections(HTTPServerRef inHTTPServer, HTTPConnectionRef inHijacker)
{
    HTTPConnectionRef* next;
    HTTPConnectionRef conn;

    if (inHTTPServer) {
        next = &inHTTPServer->connections;
        while ((conn = *next) != NULL) {
            if ((conn != inHijacker) && _IsConnectionActive(conn)) {
                aprs_ulog(kLogLevelNotice, "*** Hijacking connection %##a for %##a\n", &conn->peerAddr, &inHijacker->peerAddr);
                *next = conn->next;
                _DestroyConnection(conn);
            } else {
                next = &conn->next;
            }
        }
    }
}

//===========================================================================================================================
//	_HijackConnections
//
//	This function should be called on inServer->httpQueue.
//===========================================================================================================================

static void _HijackConnections(AirPlayReceiverServerRef inServer, HTTPConnectionRef inHijacker)
{
    if (!inServer)
        return;
    if (!inServer->httpServer)
        return;

    // Close any connections that have started audio (should be 1 connection at most).
    // This leaves connections that haven't started audio because they may just be doing OPTIONS requests, etc.

    _HijackHTTPServerConnections(inServer->httpServer, inHijacker);

#if (AIRPLAY_HTTP_SERVER_LEGACY)
    if (!inServer->httpServerLegacy)
        return;
    _HijackHTTPServerConnections(inServer->httpServerLegacy, inHijacker);
#endif
}

//===========================================================================================================================
//	_IsConnectionOfType
//
//	This function should be called on inServer->httpQueue.
//==========================================================================================================================='

static Boolean _IsConnectionOfType(HTTPConnectionRef inConnection, AirPlayHTTPConnectionTypes inConnectionType)
{
    Boolean isConnectionOfRequestedType = false;

    (void)inConnection;

    if (inConnectionType == kAirPlayHTTPConnectionTypeAny) {
        isConnectionOfRequestedType = true;
    } else if (inConnectionType == kAirPlayHTTPConnectionTypeAudio) {
        {
            isConnectionOfRequestedType = true;
        }
    }

    return (isConnectionOfRequestedType);
}

//===========================================================================================================================
//	_RemoveHTTPServerConnections
//
//	This function should be called on inServer->httpQueue.
//==========================================================================================================================='

typedef struct
{
    AirPlayHTTPConnectionTypes connectionTypeToRemove;
    HTTPServerRef httpServer;
} AirPlayReceiverServer_RemoveHTTPConnectionsParams;

static void _RemoveHTTPServerConnections(void* inArg)
{
    AirPlayReceiverServer_RemoveHTTPConnectionsParams* params = (AirPlayReceiverServer_RemoveHTTPConnectionsParams*)inArg;
    HTTPConnectionRef* next;
    HTTPConnectionRef conn;

    if (params->httpServer) {
        next = &params->httpServer->connections;
        while ((conn = *next) != NULL) {
            if (_IsConnectionOfType(conn, params->connectionTypeToRemove)) {
                *next = conn->next;
                _DestroyConnection(conn);
            } else {
                next = &conn->next;
            }
        }

        check(params->httpServer->connections == NULL);
        CFRelease(params->httpServer);
    }
    free(params);
}

//===========================================================================================================================
//	_RemoveHTTPServerConnectionsOfType
//
//  This function should NOT be called on inServer->httpQueue.
//===========================================================================================================================

static void _RemoveHTTPServerConnectionsOfType(AirPlayReceiverServerRef inServer, HTTPServerRef inHTTPServer, AirPlayHTTPConnectionTypes inConnectionTypeToRemove)
{
    AirPlayReceiverServer_RemoveHTTPConnectionsParams* params = NULL;

    if (!inHTTPServer)
        return;

    params = (AirPlayReceiverServer_RemoveHTTPConnectionsParams*)calloc(1, sizeof(*params));
    require(params, exit);

    CFRetain(inHTTPServer); // it will be released in _RemoveHTTPServerConnections
    params->httpServer = inHTTPServer;
    params->connectionTypeToRemove = inConnectionTypeToRemove;

    dispatch_sync_f(inServer->httpQueue, params, _RemoveHTTPServerConnections);
    params = NULL;

exit:
    if (params)
        free(params);
}

//===========================================================================================================================
//	_RemoveAllAudioConnections
//
//  This function should NOT be called on inServer->httpQueue.
//===========================================================================================================================

static void _RemoveAllAudioConnections(void* inArg)
{
    AirPlayReceiverServerRef airPlayReceiverServer = (AirPlayReceiverServerRef)inArg;
    if (!airPlayReceiverServer)
        return;

    // Note that if we failed to tear down some MediaControl context for unified path, we won't tear down that connection here,
    // and the sender might continue to think it is still airplaying...

    _RemoveHTTPServerConnectionsOfType(airPlayReceiverServer, airPlayReceiverServer->httpServer, kAirPlayHTTPConnectionTypeAudio);

#if (AIRPLAY_HTTP_SERVER_LEGACY)
    _RemoveHTTPServerConnectionsOfType(airPlayReceiverServer, airPlayReceiverServer->httpServerLegacy, kAirPlayHTTPConnectionTypeAudio);
#endif
    CFRelease(airPlayReceiverServer);
    aprs_ulog(kLogLevelNotice, "Stopped all AirPlay Audio Connections\n");
}

//===========================================================================================================================
//	_RemoveAllConnections
//
//  This function should NOT be called on inServer->httpQueue.
//===========================================================================================================================

static void _RemoveAllConnections(AirPlayReceiverServerRef inServer)
{
    if (!inServer)
        return;

    _RemoveHTTPServerConnectionsOfType(inServer, inServer->httpServer, kAirPlayHTTPConnectionTypeAny);

#if (AIRPLAY_HTTP_SERVER_LEGACY)
    _RemoveHTTPServerConnectionsOfType(inServer, inServer->httpServerLegacy, kAirPlayHTTPConnectionTypeAny);
#endif
}

//===========================================================================================================================
//	_GetListenPort
//===========================================================================================================================

static int _GetListenPort(AirPlayReceiverServerRef inServer)
{
    if (inServer->httpServer)
        return (inServer->httpServer->listeningPort);
    return (0);
}

//===========================================================================================================================
//	AirPlayReceiverServerStopAllConnections
//===========================================================================================================================

void AirPlayReceiverServerStopAllAudioConnections(AirPlayReceiverServerRef inServer)
{
    if (!inServer)
        return;

    CFRetain(inServer);
    dispatch_async_f(inServer->queue, inServer, _RemoveAllAudioConnections);
}

void AirPlayReceiverServerHijackConnection(
    AirPlayReceiverServerRef inReceiverServer,
    CFTypeRef inActiveSession,
    AirPlayReceiverSessionRef inHijackerSession)
{
    (void)inActiveSession;

    _HijackConnections(inReceiverServer, inHijackerSession->connection->httpCnx);
}

OSStatus AirPlayReceiverServerSendMediaCommandWithOptions(
    AirPlayReceiverServerRef inReceiverServer,
    CFStringRef inCommand,
    CFDictionaryRef inParams)
{
    OSStatus status = kNoErr;
    AirPlayReceiverSessionRef masterSession = NULL;
    CFMutableDictionaryRef request = NULL;

    require_action_quiet(inReceiverServer, exit, status = kParamErr);
    require_action_quiet(inCommand, exit, status = kParamErr);

    if (AirPlaySessionManagerIsMasterSessionActive(inReceiverServer->sessionMgr))
        masterSession = AirPlaySessionManagerCopyMasterSession(inReceiverServer->sessionMgr);

    if (masterSession && masterSession->clientVersion >= kAirPlaySourceVersion_MediaRemoteCommads_Min) {
        if (inParams != NULL)
            request = CFDictionaryCreateMutableCopy(NULL, 0, inParams);
        else
            request = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

        require_action(request, exit, status = kNoMemoryErr);
        CFDictionarySetValue(request, CFSTR(kAirPlayKey_Type), CFSTR(kAirPlayCommand_SendMediaRemoteCommand));
        CFDictionarySetValue(request, CFSTR("value"), inCommand);

        status = AirPlayReceiverSessionSendCommand(masterSession, request, NULL, NULL);
    }
#if (AIRPLAY_DACP)
    else {
#define kDACPCommandStr_DiscretePause "discrete-pause"
#define kDACPCommandStr_Play "play"
#define kDACPCommandStr_PlayPause "playpause"
#define kDACPCommandStr_NextItem "nextitem"
#define kDACPCommandStr_PrevItem "previtem"
#define kDACPCommandStr_ShuffleToggle "shuffletoggle"
#define kDACPCommandStr_RepeatAdvance "repeatadv"
#define kDACPCommandStr_VolumeDown "volumedown"
#define kDACPCommandStr_VolumeUp "volumeup"

        const char* dacpCommand = NULL;

        if (false) {
        } else if (CFEqual(inCommand, CFSTR(kAPMediaRemote_Pause)))
            dacpCommand = kDACPCommandStr_DiscretePause;
        else if (CFEqual(inCommand, CFSTR(kAPMediaRemote_Play)))
            dacpCommand = kDACPCommandStr_Play;
        else if (CFEqual(inCommand, CFSTR(kAPMediaRemote_PlayPause)))
            dacpCommand = kDACPCommandStr_PlayPause;
        else if (CFEqual(inCommand, CFSTR(kAPMediaRemote_NextTrack)))
            dacpCommand = kDACPCommandStr_NextItem;
        else if (CFEqual(inCommand, CFSTR(kAPMediaRemote_PreviousTrack)))
            dacpCommand = kDACPCommandStr_PrevItem;
        else if (CFEqual(inCommand, CFSTR(kAPMediaRemote_ShuffleToggle)))
            dacpCommand = kDACPCommandStr_ShuffleToggle;
        else if (CFEqual(inCommand, CFSTR(kAPMediaRemote_RepeatToggle)))
            dacpCommand = kDACPCommandStr_RepeatAdvance;
        else if (CFEqual(inCommand, CFSTR(kAPMediaRemote_VolumeDown)))
            dacpCommand = kDACPCommandStr_VolumeDown;
        else if (CFEqual(inCommand, CFSTR(kAPMediaRemote_VolumeUp)))
            dacpCommand = kDACPCommandStr_VolumeUp;
        else if (CFEqual(inCommand, CFSTR(kAPMediaRemote_AllowPlayback)))
            dacpCommand = "setproperty?dmcp.device-prevent-playback=0";
        else if (CFEqual(inCommand, CFSTR(kAPMediaRemote_PreventPlayback)))
            dacpCommand = "setproperty?dmcp.device-prevent-playback=1";

        require_action(dacpCommand != NULL, exit, status = kNotFoundErr);
        status = AirTunesDACPClient_ScheduleCommand(inReceiverServer->dacpClient, dacpCommand);
    }
#endif
exit:
    CFReleaseNullSafe(masterSession);
    CFReleaseNullSafe(request);

    return status;
}

Boolean AirPlayReceiverServerIsHomeAccessControlEnabled(AirPlayReceiverServerRef inReceiverServer)
{
    return HIIsAccessControlEnabled(inReceiverServer);
}

static void _HandlePairVerifyHomeKitCompletion(HTTPMessageRef inMsg);

#if 0
#pragma mark -
#pragma mark == AirPlayReceiverConnection ==
#endif

//===========================================================================================================================
//	_HandleHTTPConnectionCreated
//===========================================================================================================================

static void
_HandleHTTPConnectionCreated(
    HTTPServerRef inServer,
    HTTPConnectionRef inCnx,
    void* inCnxExtraPtr,
    void* inContext)
{
    AirPlayReceiverServerRef const server = (AirPlayReceiverServerRef)inContext;
    AirPlayReceiverConnectionRef const cnx = (AirPlayReceiverConnectionRef)inCnxExtraPtr;
    HTTPConnectionDelegate delegate;

    (void)inServer;

    cnx->server = server;
    cnx->httpCnx = inCnx;

    HTTPConnectionDelegateInit(&delegate);
    delegate.context = cnx;
    delegate.initialize_f = _HandleHTTPConnectionInitialize;
    delegate.finalize_f = _HandleHTTPConnectionFinalize;
    delegate.close_f = _HandleHTTPConnectionClose;
    delegate.handleMessage_f = _HandleHTTPConnectionMessage;
    HTTPConnectionSetDelegate(inCnx, &delegate);
}

//===========================================================================================================================
//	_HandleHTTPConnectionClose
//===========================================================================================================================

static void _HandleHTTPConnectionClose(HTTPConnectionRef inCnx, void* inContext)
{
    AirPlayReceiverConnectionRef const cnx = (AirPlayReceiverConnectionRef)inContext;
    AirPlayReceiverSessionRef session;
    Boolean allStreamsTornDown = false;

    (void)inCnx;

    session = cnx->session;
    cnx->session = NULL;
    if (session) {
        // Check to see if all streams have been torn down.

        if ((session->mainAudioCtx.type == kAirPlayStreamType_Invalid)
            && (session->altAudioCtx.type == kAirPlayStreamType_Invalid)) {
            allStreamsTornDown = true;
        }

        AirPlayReceiverSessionTearDown(session, NULL, (!cnx->didAnnounce || allStreamsTornDown) ? kNoErr : kConnectionErr, NULL);
        if (session->server) {
            if (!session->isRemoteControlSession)
                _clearGrouping(session->server);

            AirPlaySessionManagerRemoveSession(session->server->sessionMgr, session);
        }

        CFRelease(session);
    }
}

//===========================================================================================================================
//	_HandleHTTPConnectionInitialize
//===========================================================================================================================

static OSStatus _HandleHTTPConnectionInitialize(HTTPConnectionRef inCnx, void* inContext)
{
    (void)inContext;
    SocketSetKeepAlive(inCnx->sock, kAirPlayDataTimeoutSecs / 5, 5);

    return kNoErr;
}

//===========================================================================================================================
//	_HandleHTTPConnectionFinalize
//===========================================================================================================================

static void _HandleHTTPConnectionFinalize(HTTPConnectionRef inCnx, void* inContext)
{
    AirPlayReceiverConnectionRef const cnx = (AirPlayReceiverConnectionRef)inContext;

    (void)inCnx;

    cnx->server = NULL;

#if (TARGET_OS_POSIX)
    _tearDownNetworkListener(cnx);
#endif
    ForgetCF(&cnx->pairSetupSessionHomeKit);
    ForgetCF(&cnx->pairVerifySessionHomeKit);
    ForgetCF(&cnx->addPairingSession);
    ForgetCF(&cnx->listPairingsSession);
    ForgetCF(&cnx->removePairingSession);

    ForgetCF(&cnx->logs);
    if (cnx->MFiSAP) {
        MFiSAP_Delete(cnx->MFiSAP);
        cnx->MFiSAP = NULL;
        cnx->MFiSAPDone = false;
        cnx->authSetupSuccessful = false;
    }
}

//===========================================================================================================================
//	_DestroyConnection
//===========================================================================================================================

static void _DestroyConnection(HTTPConnectionRef inCnx)
{
    HTTPConnectionStop(inCnx);
    if (inCnx->selfAddr.sa.sa_family != AF_UNSPEC) {
        aprs_ulog(kLogLevelInfo, "Closing  connection from %##a to %##a\n", &inCnx->peerAddr, &inCnx->selfAddr);
    }
    CFRelease(inCnx);
}

#ifdef LINKPLAY
static int _WalkAround_wac_configured_msg(HTTPConnectionRef inCnx, const char *pathPtr, size_t pathLen)
{
    /* sometimes /configured msg received by airplayd, we have to walkaround it */
    if (g_AirplayWaitingForWacConfiguredMsg && strnicmp_suffix(pathPtr, pathLen, "/configured") == 0)
    {
         aprs_dlog(kLogLevelNotice, "### WAC is waiting /configured msg but we received....\n");

         HTTPMessageRef httpRespRef = inCnx->responseMsg;
         
         //  HTTP/1.1 200 OK
         memset((void *)&httpRespRef->header.buf, 0, sizeof(httpRespRef->header.buf));
         sprintf(httpRespRef->header.buf, "HTTP/1.1 200 OK\r\n\r\n");
         httpRespRef->header.len = strlen(httpRespRef->header.buf);
         HTTPConnectionSendResponse(inCnx);

         //Notify WAC that we received configured Msg.
         SocketClientReadWriteMsg("/tmp/RequestWACServer", WAC_MSG_AIRPLAY_RECVED_CONFIGURED, strlen(WAC_MSG_AIRPLAY_RECVED_CONFIGURED), NULL, NULL, 0);

         //system("echo 1 > /tmp/airplay_receive_configed");

         g_AirplayWaitingForWacConfiguredMsg = 0;
         
         return 1;
    }

    return 0;
}
#endif

//===========================================================================================================================
//	_HandleHTTPConnectionMessage
//===========================================================================================================================

#define GetHeaderValue(req, name, outVal, outValLen) \
    HTTPGetHeaderField((req)->header.buf, (req)->header.len, name, NULL, NULL, outVal, outValLen, NULL)

static OSStatus _HandleHTTPConnectionMessage(HTTPConnectionRef inCnx, HTTPMessageRef inRequest, void* inContext)
{
    OSStatus err;
    HTTPMessageRef response = inCnx->responseMsg;
    const char* const methodPtr = inRequest->header.methodPtr;
    size_t const methodLen = inRequest->header.methodLen;
    const char* const pathPtr = inRequest->header.url.pathPtr;
    size_t const pathLen = inRequest->header.url.pathLen;
    Boolean logHTTP = true;
    const char* httpProtocol;
    HTTPStatus status = kHTTPStatus_OK;
    const char* cSeqPtr = NULL;
    size_t cSeqLen = 0;
    AirPlayReceiverConnectionRef cnx = (AirPlayReceiverConnectionRef)inContext;

    require_action(cnx, exit, err = kParamErr);

    if (NULL == strstr(pathPtr, "/feedback"))
        aprs_ulog(kLogLevelNotice, "method:'%.*s' path:'%.*s'\n", (int)methodLen, methodPtr, (int)pathLen, pathPtr);

#ifdef LINKPLAY
    if(_WalkAround_wac_configured_msg(inCnx, pathPtr, pathLen)) {
        err = kNoErr;
        goto exit;
    }
#endif
    
    httpProtocol = (strnicmp_prefix(inRequest->header.protocolPtr, inRequest->header.protocolLen, "HTTP") == 0)
        ? "HTTP/1.1"
        : kAirTunesHTTPVersionStr;

    inCnx->delegate.httpProtocol = httpProtocol;

    // OPTIONS and /feedback requests are too chatty so don't log them by default.

    if (((strnicmpx(methodPtr, methodLen, "OPTIONS") == 0) || ((strnicmpx(methodPtr, methodLen, "POST") == 0) && (strnicmp_suffix(pathPtr, pathLen, "/feedback") == 0))) && !log_category_enabled(aprs_http_ucat(), kLogLevelChatty)) {
        logHTTP = false;
    }
    if (logHTTP)
        LogHTTP(aprs_http_ucat(), aprs_http_ucat(), inRequest->header.buf, inRequest->header.len,
            inRequest->bodyPtr, inRequest->bodyLen);

    AirPlayReceiverSessionBumpActivityCount(cnx->session);

    GetHeaderValue(inRequest, kHTTPHeader_CSeq, &cSeqPtr, &cSeqLen);

    // Parse the client device's ID. If not provided (e.g. older device) then fabricate one from the IP address.

    HTTPScanFHeaderValue(inRequest->header.buf, inRequest->header.len, kAirPlayHTTPHeader_DeviceID, "%llx", &cnx->clientDeviceID);
    if (cnx->clientDeviceID == 0)
        cnx->clientDeviceID = SockAddrToDeviceID(&inCnx->peerAddr);

    if (*cnx->clientName == '\0') {
        const char* namePtr = NULL;
        size_t nameLen = 0;

        GetHeaderValue(inRequest, kAirPlayHTTPHeader_ClientName, &namePtr, &nameLen);
        if (nameLen > 0)
            TruncateUTF8(namePtr, nameLen, cnx->clientName, sizeof(cnx->clientName), true);
    }

    if (strnicmp_suffix(pathPtr, pathLen, "/logs") == 0) {
#if AIRPLAY_LOG_REQUEST
        status = AirPlayReceiverLogRequestGetLogs(cnx, inRequest);
#else
        status = kHTTPStatus_NotFound;
#endif
        goto SendResponse;
    }

    // Process the request. Assume success initially, but we'll change it if there is an error.
    // Note: methods are ordered below roughly to how often they are used (most used earlier).

    err = HTTPHeader_InitResponse(&response->header, httpProtocol, kHTTPStatus_OK, NULL);
    require_noerr(err, exit);
    response->bodyLen = 0;

    status = _AirPlayReceiverConnectionCheckRequestAuthorization(cnx, inRequest);
    require_quiet(status == kHTTPStatus_OK, SendResponse);

#if (AIRPLAY_DACP)
    if (cnx->didAudioSetup) {
        uint64_t dacpID;
        uint32_t activeRemoteID;

        if (HTTPScanFHeaderValue(inRequest->header.buf, inRequest->header.len, "DACP-ID", "%llX", &dacpID) == 1) {
            AirTunesDACPClient_SetDACPID(cnx->server->dacpClient, dacpID);
        }
        if (HTTPScanFHeaderValue(inRequest->header.buf, inRequest->header.len, "Active-Remote", "%u", &activeRemoteID) == 1) {
            AirTunesDACPClient_SetActiveRemoteID(cnx->server->dacpClient, activeRemoteID);
        }
    }
#endif

    if (strnicmpx(methodPtr, methodLen, "OPTIONS") == 0)
        status = _requestProcessOptions(cnx);
    else if (strnicmpx(methodPtr, methodLen, "FLUSH") == 0)
        status = _requestProcessFlush(cnx, inRequest);
    else if (strnicmpx(methodPtr, methodLen, "FLUSHBUFFERED") == 0)
        status = _requestProcessFlushBuffered(cnx, inRequest);
    else if (strnicmpx(methodPtr, methodLen, "SET_PARAMETER") == 0)
        status = _requestProcessSetParameter(cnx, inRequest);
    else if (strnicmpx(methodPtr, methodLen, "GET_PARAMETER") == 0)
        status = _requestProcessGetParameter(cnx, inRequest);
#if (AIRPLAY_SDP_SETUP)
    else if (strnicmpx(methodPtr, methodLen, "ANNOUNCE") == 0)
        status = _requestProcessAnnounce(cnx, inRequest);
#endif // AIRPLAY_SDP_SETUP
    else if (strnicmpx(methodPtr, methodLen, "SETRATEANCHORTIME") == 0)
        status = _requestProcessSetRateAndAnchorTime(cnx, inRequest);
    else if (strnicmpx(methodPtr, methodLen, "SETPEERS") == 0)
        status = _requestProcessSetPeers(cnx, inRequest);
    else if (strnicmpx(methodPtr, methodLen, "RECORD") == 0)
        status = _requestProcessRecord(cnx, inRequest);
    else if (strnicmpx(methodPtr, methodLen, "SETUP") == 0)
        status = _requestProcessSetup(cnx, inRequest);
    else if (strnicmpx(methodPtr, methodLen, "TEARDOWN") == 0)
        status = _requestProcessTearDown(cnx, inRequest);
    else if (strnicmpx(methodPtr, methodLen, "GET") == 0) {
        if (0) {
        } else if (strnicmp_suffix(pathPtr, pathLen, "/info") == 0)
            status = _requestProcessInfo(cnx, inRequest);
#if (AIRPLAY_METRICS)
        else if (strnicmp_suffix(pathPtr, pathLen, "/metrics") == 0)
            status = AirPlayReceiverMetrics(cnx, inRequest);
        else if (strnicmp_suffix(pathPtr, pathLen, "/history") == 0)
            status = AirPlayReceiverHistory(cnx, inRequest);
        else if (strncmp_prefix(pathPtr, pathLen, "/favicon") == 0)
            status = kHTTPStatus_NotFound;
#endif
        else {
            dlog(kLogLevelNotice, "### Unsupported GET: '%.*s'\n", (int)pathLen, pathPtr);
            status = kHTTPStatus_NotFound;
        }
    } else if (strnicmpx(methodPtr, methodLen, "POST") == 0) {
        if (0) {
        } else if (strnicmp_suffix(pathPtr, pathLen, "/auth-setup") == 0)
            status = _requestProcessAuthSetup(cnx, inRequest);
        else if (strnicmp_suffix(pathPtr, pathLen, "/command") == 0)
            status = _requestProcessCommand(cnx, inRequest);
#if (AIRPLAY_HOME_CONFIGURATION)
        else if (strnicmp_suffix(pathPtr, pathLen, "/configure") == 0)
            status = _requestProcessConfigure(cnx, inRequest);
#endif
        else if (strnicmp_suffix(pathPtr, pathLen, "/diag-info") == 0)
            status = kHTTPStatus_OK;
        else if (strnicmp_suffix(pathPtr, pathLen, "/feedback") == 0)
            status = _requestProcessFeedback(cnx, inRequest);
        else if (strnicmp_suffix(pathPtr, pathLen, "/info") == 0)
            status = _requestProcessInfo(cnx, inRequest);
        else if (strnicmp_suffix(pathPtr, pathLen, "/audioMode") == 0)
            status = _requestProcessAudioMode(cnx, inRequest);
#if (AIRPLAY_METRICS)
        else if (strnicmp_suffix(pathPtr, pathLen, "/metrics") == 0)
            status = AirPlayReceiverMetricsProcess(cnx, inRequest);
        else if (strnicmp_suffix(pathPtr, pathLen, "/history") == 0)
            status = AirPlayReceiverHistory(cnx, inRequest);
        else if (strnicmp_suffix(pathPtr, pathLen, "/reset") == 0)
            status = AirPlayReceiverClearHome(cnx);
#endif
        else if (strnicmp_suffix(pathPtr, pathLen, "/pair-pin-start") == 0)
            status = _requestProcessPairSetupPINStart(cnx, inRequest);
        else if (strnicmp_suffix(pathPtr, pathLen, "/pair-setup") == 0)
            status = _requestProcessPairSetup(cnx, inRequest);
        else if (strnicmp_suffix(pathPtr, pathLen, "/pair-verify") == 0)
            status = _requestProcessPairVerify(cnx, inRequest);
        else if (strnicmp_suffix(pathPtr, pathLen, "/pair-add") == 0)
            status = _requestProcessPairAdd(cnx, inRequest);
        else if (strnicmp_suffix(pathPtr, pathLen, "/pair-remove") == 0)
            status = _requestProcessPairRemove(cnx, inRequest);
        else if (strnicmp_suffix(pathPtr, pathLen, "/pair-list") == 0)
            status = _requestProcessPairList(cnx, inRequest);
        else {
            dlogassert("Bad POST: '%.*s'", (int)pathLen, pathPtr);
            status = kHTTPStatus_NotFound;
        }
    } else {
#if (AIRPLAY_METRICS)
        AirPlayReceiverMetricsSupplementTimeStamp(cnx, CFSTR("ERROR: Bad method."));
#endif
        dlogassert("Bad method: %.*s", (int)methodLen, methodPtr);
        status = kHTTPStatus_NotImplemented;
    }

SendResponse:

#if (AIRPLAY_METRICS)
    AirPlayReceiverMetricsUpdateTimeStamp(cnx, inRequest);
#endif

    // If an error occurred, reset the response message with a new status.

    if (status != kHTTPStatus_OK) {
        err = HTTPHeader_InitResponse(&response->header, httpProtocol, status, NULL);
        require_noerr(err, exit);
        response->bodyLen = 0;

        err = HTTPHeader_AddFieldF(&response->header, kHTTPHeader_ContentLength, "0");
        require_noerr(err, exit);
    }

    // Server

    err = HTTPHeader_AddFieldF(&response->header, kHTTPHeader_Server, "AirTunes/%s", kAirPlaySourceVersionStr);
    require_noerr(err, exit);

    // WWW-Authenticate

    if (status == kHTTPStatus_Unauthorized) {
        char nonce[64];

        err = HTTPMakeTimedNonce(kHTTPTimedNonceETagPtr, kHTTPTimedNonceETagLen,
            cnx->server->httpTimedNonceKey, sizeof(cnx->server->httpTimedNonceKey),
            nonce, sizeof(nonce), NULL);
        require_noerr(err, exit);

        err = HTTPHeader_SetField(&response->header, kHTTPHeader_WWWAuthenticate, "Digest realm=\"airplay\", nonce=\"%s\"", nonce);
        require_noerr(err, exit);
    }

    // CSeq

    if (cSeqPtr) {
        err = HTTPHeader_AddFieldF(&response->header, kHTTPHeader_CSeq, "%.*s", (int)cSeqLen, cSeqPtr);
        require_noerr(err, exit);
    }

    if (logHTTP)
        LogHTTP(aprs_http_ucat(), aprs_http_ucat(), response->header.buf, response->header.len, response->bodyPtr, response->bodyLen);

    err = HTTPConnectionSendResponse(inCnx);
    require_noerr(err, exit);

exit:
    aprs_ulog(kLogLevelTrace, "******** method:'%.*s' path:'%.*s' (err:%#m status:%d)\n", (int)methodLen, methodPtr, (int)pathLen, pathPtr, err, status);
    return (err);
}

#if 0
#pragma mark -
#pragma mark == Request Processing ==
#endif

#if (AIRPLAY_SDP_SETUP)
//===========================================================================================================================
//	_requestProcessAnnounce
//===========================================================================================================================

static HTTPStatus _requestProcessAnnounce(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    HTTPStatus status;
    OSStatus err;
    const char* src;
    const char* end;
    const char* ptr;
    size_t len;
    const char* sectionPtr;
    const char* sectionEnd;
    const char* valuePtr;
    size_t valueLen;

    aprs_ulog(kAirPlayPhaseLogLevel, "Announce (%##a)\n", &inCnx->httpCnx->peerAddr);

    // Make sure content type is SDP.

    src = NULL;
    len = 0;
    GetHeaderValue(inRequest, kHTTPHeader_ContentType, &src, &len);
    if (strnicmpx(src, len, kMIMEType_SDP) != 0) {
        dlogassert("bad Content-Type: '%.*s'", (int)len, src);
        status = kHTTPStatus_NotAcceptable;
        goto exit;
    }

    // Reject the request if we're not in the right state.

    if (inCnx->didAnnounce) {
        dlogassert("ANNOUNCE not allowed twice");
        status = kHTTPStatus_MethodNotValidInThisState;
        goto exit;
    }

    // If we're denying interrupts then reject if there's already an active session.
    // Otherwise, hijack any active session to start the new one (last-in-wins).

    if (inCnx->server->denyInterruptions) {
        HTTPConnectionRef activeCnx;

        activeCnx = _FindActiveConnection(inCnx->server);
        if (activeCnx) {
            aprs_ulog(kLogLevelNotice, "Denying interruption from %##a due to %##a\n", &inCnx->httpCnx->peerAddr, &activeCnx->peerAddr);
            status = kHTTPStatus_NotEnoughBandwidth;
            goto exit;
        }
    }
    _HijackConnections(inCnx->server, inCnx->httpCnx);

    // Get the session ID from the origin line with the format: <username> <session-id> ...

    src = (const char*)inRequest->bodyPtr;
    end = src + inRequest->bodyOffset;
    err = SDPFindSessionSection(src, end, &sectionPtr, &sectionEnd, &src);
    require_noerr_action(err, exit, status = kHTTPStatus_SessionNotFound);

    valuePtr = NULL;
    valueLen = 0;
    SDPFindType(sectionPtr, sectionEnd, 'o', &valuePtr, &valueLen, NULL);
    SNScanF(valuePtr, valueLen, "%*s %llu", &inCnx->clientSessionID);

    // Get the sender's name from the session information.

    valuePtr = NULL;
    valueLen = 0;
    SDPFindType(sectionPtr, sectionEnd, 'i', &valuePtr, &valueLen, NULL);
    TruncateUTF8(valuePtr, valueLen, inCnx->clientName, sizeof(inCnx->clientName), true);

    // Audio

    err = SDPFindMediaSection(src, end, NULL, NULL, &ptr, &len, &src);
    require_noerr_action(err, exit, status = kHTTPStatus_NotAcceptable);

    if (strncmp_prefix(ptr, len, "audio") == 0) {
        err = _requestProcessAnnounceAudio(inCnx, inRequest);
        require_noerr_action(err, exit, status = kHTTPStatus_NotAcceptable);
    }

    // Success.

    strlcpy(gAirPlayAudioStats.ifname, inCnx->ifName, sizeof(gAirPlayAudioStats.ifname));
    inCnx->didAnnounce = true;

    status = kHTTPStatus_OK;

exit:
    return (status);
}

//===========================================================================================================================
//	_requestProcessAnnounceAudio
//===========================================================================================================================

static OSStatus _requestProcessAnnounceAudio(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    OSStatus err;
    const char* src;
    const char* end;
    const char* ptr;
    size_t len;
    const char* mediaSectionPtr;
    const char* mediaSectionEnd;
    int n;
    int mediaPayloadType;
    int payloadType;
    const char* encodingPtr;
    size_t encodingLen;
    const char* aesKeyPtr;
    size_t aesKeyLen;
    const char* aesIVPtr;
    size_t aesIVLen;
    size_t decodedLen;

    src = (const char*)inRequest->bodyPtr;
    end = src + inRequest->bodyOffset;

    //-----------------------------------------------------------------------------------------------------------------------
    //	Compression
    //-----------------------------------------------------------------------------------------------------------------------

    err = SDPFindMediaSection(src, end, &mediaSectionPtr, &mediaSectionEnd, &ptr, &len, &src);
    require_noerr(err, exit);

    n = SNScanF(ptr, len, "audio 0 RTP/AVP %d", &mediaPayloadType);
    require_action(n == 1, exit, err = kUnsupportedErr);

    err = SDPFindAttribute(mediaSectionPtr, mediaSectionEnd, "rtpmap", &ptr, &len, NULL);
    require_noerr(err, exit);

    n = SNScanF(ptr, len, "%d %&s", &payloadType, &encodingPtr, &encodingLen);
    require_action(n == 2, exit, err = kMalformedErr);
    require_action(payloadType == mediaPayloadType, exit, err = kMismatchErr);

    if (0) {
    } // Empty if to simplify conditional logic below.

// AppleLossless

#if (AIRPLAY_ALAC)
    else if (strnicmpx(encodingPtr, encodingLen, "AppleLossless") == 0) {
        int frameLength, version, bitDepth, pb, mb, kb, channelCount, maxRun, maxFrameBytes, avgBitRate, sampleRate;

        // Parse format parameters. For example: a=fmtp:96 352 0 16 40 10 14 2 255 0 0 44100

        err = SDPFindAttribute(mediaSectionPtr, mediaSectionEnd, "fmtp", &ptr, &len, NULL);
        require_noerr(err, exit);

        n = SNScanF(ptr, len, "%d %d %d %d %d %d %d %d %d %d %d %d",
            &payloadType, &frameLength, &version, &bitDepth, &pb, &mb, &kb, &channelCount, &maxRun, &maxFrameBytes,
            &avgBitRate, &sampleRate);
        require_action(n == 12, exit, err = kMalformedErr);
        require_action(payloadType == mediaPayloadType, exit, err = kMismatchErr);

        inCnx->compressionType = kAirPlayCompressionType_ALAC;
        inCnx->framesPerPacket = frameLength;
    }
#endif

    // AAC

    else if (strnicmpx(encodingPtr, encodingLen, "mpeg4-generic/44100/2") == 0) {
        const char* paramsPtr;
        const char* paramsEnd;
        size_t paramsLen;
        const char* valuePtr;
        size_t valueLen;

        err = SDPFindAttribute(mediaSectionPtr, mediaSectionEnd, "fmtp", &ptr, &len, NULL);
        require_noerr(err, exit);

        n = SNScanF(ptr, len, "%d %&c", &payloadType, &paramsPtr, &paramsLen);
        require_action(n == 2, exit, err = kMalformedErr);
        require_action(payloadType == mediaPayloadType, exit, err = kMismatchErr);
        paramsEnd = paramsPtr + paramsLen;

        valuePtr = "AAC"; // Default to "AAC" for older clients that don't send a mode parameter.
        valueLen = 3;
        SDPFindParameter(paramsPtr, paramsEnd, "mode", &valuePtr, &valueLen, NULL);
        if (0) {
        } // Empty if to simplify conditional logic below.
        else if (strncmpx(valuePtr, valueLen, "AAC") == 0)
            inCnx->compressionType = kAirPlayCompressionType_AAC_LC;
#if (AIRPLAY_AAC_ELD)
        else if (strncmpx(valuePtr, valueLen, "AAC-eld") == 0)
            inCnx->compressionType = kAirPlayCompressionType_AAC_ELD;
#endif
        else {
            aprs_ulog(kLogLevelNotice, "### Unsuported AAC format: '%.*s'\n", (int)len, ptr);
            err = kUnsupportedDataErr;
            goto exit;
        }

        valuePtr = "1024"; // Default to 1024 samples per frame.
        valueLen = 4;
        SDPFindParameter(paramsPtr, paramsEnd, "constantDuration", &valuePtr, &valueLen, NULL);
        n = SNScanF(valuePtr, valueLen, "%u", &inCnx->framesPerPacket);
        require_action(n == 1, exit, err = kUnsupportedDataErr);
    }

    // PCM

    else if ((strnicmpx(encodingPtr, encodingLen, "L16/44100/2") == 0) || (strnicmpx(encodingPtr, encodingLen, "L16") == 0)) {
        inCnx->compressionType = kAirPlayCompressionType_PCM;
        inCnx->framesPerPacket = kAirPlaySamplesPerPacket_PCM;
        aprs_ulog(kLogLevelNotice, "*** Not using compression\n");
    }

    // Unknown audio format.

    else {
        dlogassert("Unsupported encoding: '%.*s'", (int)encodingLen, encodingPtr);
        err = kUnsupportedDataErr;
        goto exit;
    }

    //-----------------------------------------------------------------------------------------------------------------------
    //	Encryption
    //-----------------------------------------------------------------------------------------------------------------------

    aesKeyPtr = NULL;
    aesKeyLen = 0;
    aesIVPtr = NULL;
    aesIVLen = 0;

    // MFi

    if (!aesKeyPtr) {
        SDPFindAttribute(mediaSectionPtr, mediaSectionEnd, "mfiaeskey", &aesKeyPtr, &aesKeyLen, NULL);
        if (aesKeyPtr) {
            // Decode and decrypt the AES session key with MFiSAP.

            err = Base64Decode(aesKeyPtr, aesKeyLen, inCnx->encryptionKey, sizeof(inCnx->encryptionKey), &aesKeyLen);
            require_noerr(err, exit);
            require_action(aesKeyLen == sizeof(inCnx->encryptionKey), exit, err = kSizeErr);

            require_action(inCnx->MFiSAP, exit, err = kAuthenticationErr);
            err = MFiSAP_Decrypt(inCnx->MFiSAP, inCnx->encryptionKey, aesKeyLen, inCnx->encryptionKey);
            require_noerr(err, exit);
        }
    }

    // RSA

    // Decode the AES initialization vector (IV).

    SDPFindAttribute(mediaSectionPtr, mediaSectionEnd, "aesiv", &aesIVPtr, &aesIVLen, NULL);
    if (aesKeyPtr && aesIVPtr) {
        err = Base64Decode(aesIVPtr, aesIVLen, inCnx->encryptionIV, sizeof(inCnx->encryptionIV), &decodedLen);
        require_noerr(err, exit);
        require_action(decodedLen == sizeof(inCnx->encryptionIV), exit, err = kSizeErr);

        inCnx->usingEncryption = true;
    } else if (aesKeyPtr || aesIVPtr) {
        aprs_ulog(kLogLevelNotice, "### Key/IV missing: key %p/%p\n", aesKeyPtr, aesIVPtr);
        err = kMalformedErr;
        goto exit;
    } else {
        inCnx->usingEncryption = false;
    }

    // Minimum Latency

    inCnx->minLatency = kAirTunesPlayoutDelay; // Default value for old clients.
    ptr = NULL;
    len = 0;
    SDPFindAttribute(mediaSectionPtr, mediaSectionEnd, "min-latency", &ptr, &len, NULL);
    if (len > 0) {
        n = SNScanF(ptr, len, "%u", &inCnx->minLatency);
        require_action(n == 1, exit, err = kMalformedErr);
    }

    // Maximum Latency

    inCnx->maxLatency = kAirPlayAudioLatencyOther; // Default value for old clients.
    ptr = NULL;
    len = 0;
    SDPFindAttribute(mediaSectionPtr, mediaSectionEnd, "max-latency", &ptr, &len, NULL);
    if (len > 0) {
        n = SNScanF(ptr, len, "%u", &inCnx->maxLatency);
        require_action(n == 1, exit, err = kMalformedErr);
    }

exit:
    return (err);
}

//===========================================================================================================================
//	_FindActiveConnection
//===========================================================================================================================

static HTTPConnectionRef _FindActiveConnection(AirPlayReceiverServerRef inServer)
{
    HTTPConnectionRef cnx;

    if (!inServer)
        return NULL;
    if (inServer->httpServer) {
        for (cnx = inServer->httpServer->connections; cnx; cnx = cnx->next) {
            if (_IsConnectionActive(cnx))
                return (cnx);
        }
    }

#if (AIRPLAY_HTTP_SERVER_LEGACY)
    if (inServer->httpServerLegacy) {
        for (cnx = inServer->httpServerLegacy->connections; cnx; cnx = cnx->next) {
            if (_IsConnectionActive(cnx))
                return (cnx);
        }
    }
#endif

    return (NULL);
}

#endif // AIRPLAY_SDP_SETUP

//===========================================================================================================================
//	_requestProcessSetRateAndAnchorTime
//===========================================================================================================================

static HTTPStatus _requestProcessSetRateAndAnchorTime(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    AirPlayAnchor anchorTime;
    const char* ptr;
    size_t len;
    HTTPStatus status = kHTTPStatus_OK;
    OSStatus err = kNoErr;

    aprs_ulog(kAirPlayPhaseLogLevel, "SetRateAndAnchor (%##a)\n", &inCnx->httpCnx->peerAddr);

    ptr = NULL;
    len = 0;
    GetHeaderValue(inRequest, kHTTPHeader_ContentType, &ptr, &len);

    err = _rateAndAnchorTimeFromPlist(inCnx, inRequest, &anchorTime);
    require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);

    if (inCnx->session) {
        err = AirPlayReceiverSessionSetRateAndAnchorTime(inCnx->session, &anchorTime);
        require_noerr_action_quiet(err, exit, status = kHTTPStatus_BadRequest);
    }

exit:
    if (err) {
        aprs_ulog(kLogLevelNotice, "SetRateAndAnchor err = %#m\n", err);
    }

    return status;
}

//===========================================================================================================================
//	_rateAndAnchorTimeFromPlist
//===========================================================================================================================

static OSStatus
_rateAndAnchorTimeFromPlist(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest, AirPlayAnchor* outAnchor)
{
    CFDictionaryRef requestParams = NULL;
    OSStatus err;

    outAnchor->rate = 0;
    outAnchor->rtpTime = 0;
    outAnchor->netTime.timelineID = 0;
    outAnchor->netTime.secs = 0;
    outAnchor->netTime.frac = 0;
    outAnchor->netTime.flags = AirTunesTimeFlag_Invalid;

    requestParams = (CFDictionaryRef)CFBinaryPlistV0CreateWithData(inRequest->bodyPtr, inRequest->bodyOffset, &err);
    require_noerr(err, exit);
    require_action(CFGetTypeID(requestParams) == CFDictionaryGetTypeID(), exit, err = kParamErr);

#if (AIRPLAY_METRICS)
    AirPlayReceiverMetricsSupplementTimeStamp(inCnx, requestParams);
#else
    (void)inCnx;
#endif

    outAnchor->rate = CFDictionaryGetUInt32(requestParams, CFSTR(kAirPlayKey_Rate), &err);
    require_noerr(err, exit);

    if (outAnchor->rate > 0) {
        outAnchor->rtpTime = CFDictionaryGetUInt32(requestParams, CFSTR(kAirPlayKey_RTPTime), &err);
        require_noerr(err, exit);

        outAnchor->netTime.secs = CFDictionaryGetSInt32(requestParams, CFSTR(kAirPlayKey_NetworkTimeSecs), &err);
        require_noerr(err, exit);

        outAnchor->netTime.frac = CFDictionaryGetUInt64(requestParams, CFSTR(kAirPlayKey_NetworkTimeFrac), &err);
        require_noerr(err, exit);

        outAnchor->netTime.timelineID = CFDictionaryGetUInt64(requestParams, CFSTR(kAirPlayKey_NetworkTimeTimelineID), &err);
        require_noerr(err, exit);

        //networkTime.flags = CFDictionaryGetUInt32( requestParams, CFSTR( kAPAudioRequestKey_NetworkTimeFlags ), &err );
        //require_noerr_action( err, exit, status = kHTTPStatus_BadRequest );
        outAnchor->netTime.flags = 0;
    }

exit:
    CFReleaseNullSafe(requestParams);

    return err;
}

//===========================================================================================================================
//    _requestProcessSetPeers
//===========================================================================================================================

static HTTPStatus _requestProcessSetPeers(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    OSStatus err = kNoErr;
    HTTPStatus status = kHTTPStatus_OK;
    CFArrayRef peers = NULL;
    const char* ptr;
    size_t len;

    require_action(inCnx->session, exit, err = kNotPreparedErr; status = kHTTPStatus_SessionNotFound);

    aprs_ulog(kAirPlayPhaseLogLevel, "SetPeers (%##a)\n", &inCnx->httpCnx->peerAddr);

    GetHeaderValue(inRequest, kHTTPHeader_ContentType, &ptr, &len);
    peers = (CFArrayRef)CFBinaryPlistV0CreateWithData(inRequest->bodyPtr, inRequest->bodyOffset, &err);
    require_action(peers, exit, err = kMalformedErr; status = kHTTPStatus_ParameterNotUnderstood);
    require_action(CFGetTypeID(peers) == CFArrayGetTypeID(), exit, err = kMalformedErr; status = kHTTPStatus_ParameterNotUnderstood);

#if (AIRPLAY_METRICS)
    AirPlayReceiverMetricsSupplementTimeStamp(inCnx, peers);
#endif

    err = AirPlayReceiverSessionSetPeers(inCnx->session, peers);
    require_noerr_action((err != kNoErr), exit, status = kHTTPStatus_BadRequest);

exit:
    CFReleaseNullSafe(peers);

    return (status);
}

static HTTPStatus _requestProcessAudioMode(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    CFStringRef audioMode = NULL;
    CFDictionaryRef requestDict = NULL;
    HTTPStatus status;
    OSStatus err;

    require_action(inRequest->bodyOffset > 0, exit, status = kHTTPStatus_BadRequest);
    require_action(inCnx->session && !inCnx->session->isRemoteControlSession, exit, status = kNotPreparedErr);

    requestDict = (CFDictionaryRef)CFBinaryPlistV0CreateWithData(inRequest->bodyPtr, inRequest->bodyOffset, &err);
    require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);
    require_action(CFGetTypeID(requestDict) == CFDictionaryGetTypeID(), exit, status = kHTTPStatus_BadRequest);

    audioMode = (CFStringRef)CFDictionaryGetValue(requestDict, CFSTR("audioMode"));
    require_action(audioMode && CFGetTypeID(audioMode) == CFStringGetTypeID(), exit, status = kHTTPStatus_BadRequest);

    if (CFStringCompare(audioMode, CFSTR("moviePlayback"), 0x0) == kCFCompareEqualTo)
        err = AirPlayReceiverSessionSetInt64(inCnx->session, CFSTR(kAirPlayProperty_AudioMode), NULL, kAirPlayAudioMode_MoviePlayback);
    else if (CFStringCompare(audioMode, CFSTR("default"), 0x0) == kCFCompareEqualTo)
        err = AirPlayReceiverSessionSetInt64(inCnx->session, CFSTR(kAirPlayProperty_AudioMode), NULL, kAirPlayAudioMode_Default);
    else
        err = kNotHandledErr;

    status = (err == kNotHandledErr ? kHTTPStatus_NotFound : (err == kHTTPStatus_OK ? kHTTPStatus_OK : kHTTPStatus_BadRequest));
exit:
    CFReleaseNullSafe(requestDict);
    return status;
}

//===========================================================================================================================
//	_requestProcessAuthSetup
//===========================================================================================================================

static HTTPStatus _requestProcessAuthSetup(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    HTTPStatus status;
    OSStatus err;
    uint8_t* outputPtr;
    size_t outputLen;
    HTTPMessageRef response = inCnx->httpCnx->responseMsg;

    aprs_ulog(kAirPlayPhaseLogLevel, "MFi (%##a)\n", &inCnx->httpCnx->peerAddr);

    outputPtr = NULL;
    require_action(inRequest->bodyOffset > 0, exit, status = kHTTPStatus_BadRequest);

    // Let MFi-SAP process the input data and generate output data.

    if (inCnx->MFiSAPDone && inCnx->MFiSAP) {
        MFiSAP_Delete(inCnx->MFiSAP);
        inCnx->MFiSAP = NULL;
        inCnx->MFiSAPDone = false;
        inCnx->authSetupSuccessful = false;
    }
    if (!inCnx->MFiSAP) {
        err = MFiSAP_Create(&inCnx->MFiSAP, kMFiSAPVersion1);
        require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);
    }

    err = MFiSAP_Exchange(inCnx->MFiSAP, inRequest->bodyPtr, inRequest->bodyOffset, &outputPtr, &outputLen, &inCnx->MFiSAPDone);
    require_noerr_action(err, exit, status = kHTTPStatus_Forbidden);

    if (inCnx->MFiSAPDone) {
        inCnx->authSetupSuccessful = true;
    }

    // Send the MFi-SAP output data in the response.

    err = HTTPMessageSetBodyPtr(response, kMIMEType_Binary, outputPtr, outputLen);
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);
    outputPtr = NULL;

    status = kHTTPStatus_OK;

exit:
    if (outputPtr)
        free(outputPtr);
    return (status);
}

//===========================================================================================================================
//	_requestProcessCommand
//===========================================================================================================================

static HTTPStatus _requestProcessCommand(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    HTTPStatus status = kHTTPStatus_OK;
    OSStatus err = kNoErr;
    CFMutableDictionaryRef responseDict = NULL;
    CFDictionaryRef requestDict = NULL;
    CFStringRef commandType = NULL;
    CFMutableDictionaryRef entryDict = NULL;
    AirPlayReceiverSessionRef masterSession = NULL;

#define kAirPlayCommand_Mute "mute"
#define kAirPlayCommand_Pause "pause"
#define kAirPlayCommand_UpdateMRSupportedCommands "updateMRSupportedCommands"

    aprs_ulog(kLogLevelTrace, "Command (%##a)\n", &inCnx->httpCnx->peerAddr);

    require_action(inCnx->session, exit, err = kNotPreparedErr; status = kHTTPStatus_SessionNotFound);
    require_action(inRequest->bodyOffset > 0, exit, status = kHTTPStatus_BadRequest);

    masterSession = AirPlaySessionManagerCopyMasterSession(inCnx->server->sessionMgr);

    requestDict = (CFDictionaryRef)CFBinaryPlistV0CreateWithData(inRequest->bodyPtr, inRequest->bodyOffset, &err);
    require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);
    require_action(CFGetTypeID(requestDict) == CFDictionaryGetTypeID(), exit, status = kHTTPStatus_BadRequest);

    responseDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(responseDict, exit, status = kAirPlayTransportMessageProcessingStatus_AllocationFailed);

    commandType = CFDictionaryGetCFString(requestDict, CFSTR(kAirPlayKey_Type), NULL);
    if (commandType != NULL) {
        aprs_ulog(kLogLevelTrace, "Received command: '%@'\n", commandType);

        if (CFEqual(commandType, CFSTR(kAirPlayCommand_Mute))) {
            require_action(masterSession, exit, status = kHTTPStatus_BadRequest);
            err = AirPlayReceiverSessionPlatformSetDouble(masterSession, CFSTR(kAirPlayProperty_Volume), NULL, kAirTunesSilenceVolumeDB);
            require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);
        } else if (CFEqual(commandType, CFSTR(kAirPlayCommand_Pause))) {
            require_action(masterSession, exit, status = kHTTPStatus_BadRequest);
            AirPlayAnchor anchor = masterSession->source.setRateTiming.anchor;
            anchor.rate = 0;
            err = AirPlayReceiverSessionSetRateAndAnchorTime(masterSession, &anchor);
            require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);
        }
#if (AIRPLAY_META_DATA)
        else if (CFEqual(commandType, CFSTR(kAirPlayCommand_UpdateMRSupportedCommands))) {
#define kAirPlayKey_MRSupportedCommandsFromSender "mrSupportedCommandsFromSender"
#define kAirPlayKey_CommandInfoEnabledKey "kCommandInfoEnabledKey"
#define kAirPlayKey_CommandInfoCommandKey "kCommandInfoCommandKey"

            CFDictionaryRef params = CFDictionaryGetCFDictionary(requestDict, CFSTR(kAirPlayKey_Params), NULL);
            require_action(params != NULL, exit, err = kParamErr; status = kHTTPStatus_BadRequest);

            CFArrayRef supportedCommands = (CFArrayRef)CFDictionaryGetValue(params, CFSTR(kAirPlayKey_MRSupportedCommandsFromSender));
            require_action(supportedCommands, exit, err = kHTTPStatus_BadRequest);
            require_action(CFGetTypeID(supportedCommands) == CFArrayGetTypeID(), exit, err = kHTTPStatus_BadRequest);

            AirPlayUIElementFlag supportedUIElements = 0x0;

#if (AIRPLAY_VOLUME_CONTROL)
            supportedUIElements |= (kAirPlayUIElementFlag_VolumeUp | kAirPlayUIElementFlag_VolumeDown | kAirPlayUIElementFlag_VolumeControl);
#endif
            if (!masterSession->isLiveBroadcast) {
                supportedUIElements |= kAirPlayUIElementFlag_ProgressInfo;
            }

            CFIndex index;
            for (index = 0; index < CFArrayGetCount(supportedCommands); index++) {
                CFPropertyListFormat format;
                OSStatus error;

                CFDataRef data = (CFDataRef)CFArrayGetValueAtIndex(supportedCommands, index);
                CFDictionaryRef dict = (CFDictionaryRef)CFPropertyListCreateWithData(NULL, data, 0x0, &format, NULL);
                if (dict == NULL)
                    continue;

                int64_t command = CFDictionaryGetInt64(dict, CFSTR(kAirPlayKey_CommandInfoCommandKey), &error);
                if (error == kNoErr) {
                    Boolean enabled = CFDictionaryGetInt64(dict, CFSTR(kAirPlayKey_CommandInfoEnabledKey), NULL);
                    if (enabled) {
                        switch (command) {
                        case MRMediaRemoteCommandPlay:
                            supportedUIElements |= kAirPlayUIElementFlag_Play;
                            break;
                        case MRMediaRemoteCommandPause:
                            supportedUIElements |= kAirPlayUIElementFlag_Pause;
                            break;
                        case MRMediaRemoteCommandTogglePlayPause:
                            supportedUIElements |= kAirPlayUIElementFlag_PlayPause;
                            break;
                        case MRMediaRemoteCommandNextTrack:
                            supportedUIElements |= kAirPlayUIElementFlag_NextTrack;
                            break;
                        case MRMediaRemoteCommandPreviousTrack:
                            supportedUIElements |= kAirPlayUIElementFlag_PreviousTrack;
                            break;
                        case MRMediaRemoteCommandAdvanceShuffleMode:
                            supportedUIElements |= kAirPlayUIElementFlag_ShuffleToggle;
                            break;
                        case MRMediaRemoteCommandAdvanceRepeatMode:
                            supportedUIElements |= kAirPlayUIElementFlag_RepeatToggle;
                            break;
                        default:
                            break;
                        }
                    }
                }
                CFReleaseNullSafe(dict);
            }
            CFNumberRef flags = CFNumberCreateUInt64(supportedUIElements);
            err = AirPlayReceiverSessionSetProperty(masterSession, kCFObjectFlagDirect, CFSTR(kAirPlayProperty_SupportedUIElements), NULL, flags);
            CFRelease(flags);
            require_noerr_action_quiet(err, exit, status = kHTTPStatus_BadRequest);
        }
#endif
        else {
            aprs_ulog(kLogLevelError, "Unknow command: '%@'\n%@\n", commandType, requestDict);
            require_noerr_action_quiet((err = kParamErr), exit, status = kHTTPStatus_BadRequest);
        }
    } else {
        CFDictionaryRef outResponse = NULL;
        CFDictionaryRef commandParams;
        uint64_t streamID = 0;

        require_action(masterSession, exit, status = kHTTPStatus_BadRequest);

        if (!HTTPScanFHeaderValue(inRequest->header.buf, inRequest->header.len, kMediaControlHTTPHeader_StreamID, "%llu", &streamID))
            require_action(false, exit, status = kHTTPStatus_BadRequest;);

        commandParams = CFDictionaryGetCFDictionary(requestDict, CFSTR(kAirPlayKey_Params), NULL);
        require_action(commandParams, exit, status = kHTTPStatus_BadRequest);

        entryDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require_action(entryDict, exit, status = kAirPlayTransportMessageProcessingStatus_AllocationFailed);
        CFDictionarySetUInt64(entryDict, CFSTR(kMediaControlHTTPHeader_StreamID), streamID);
        CFDictionarySetValue(entryDict, CFSTR(kAirPlayKey_Params), commandParams);

        err = AirPlayReceiverSessionControl(inCnx->session, kCFObjectFlagDirect, CFSTR(kAirPlayKey_DidReceiveData), NULL, entryDict, &outResponse);
        require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

        if (outResponse) {
            CFDictionaryMergeDictionary(responseDict, outResponse);
            CFRelease(outResponse);
        }
    }

    status = AirPlayReceiverSendRequestPlistResponse(inCnx->httpCnx, inRequest, responseDict, &err);
exit:
    if (err) {
        if (commandType)
            aprs_ulog(kLogLevelError, "### Command from Client Identifier '%@' failed: %#m, %#m\n", commandType, status, err);
        else
            aprs_ulog(kLogLevelError, "### Command processing failed with error %#m\n", err);
    }
    CFReleaseNullSafe(entryDict);
    CFReleaseNullSafe(responseDict);
    CFReleaseNullSafe(requestDict);
    CFReleaseNullSafe(masterSession);

    return status;
}

#if (AIRPLAY_HOME_CONFIGURATION)

//===========================================================================================================================
// _requestProcessConfigure
//===========================================================================================================================
static HTTPStatus _requestProcessConfigure(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    (void)inCnx;
    HTTPStatus status = kHTTPStatus_OK;
    OSStatus err;
    Boolean shouldRestartBonjour = false;
    Boolean shouldUpdateBonjour = false;
    CFDictionaryRef requestDict = NULL;
    CFDictionaryRef requestConfig = NULL;
    CFDictionaryRef responseDict = NULL;

    if (inRequest->bodyOffset > 0) {
        requestDict = (CFDictionaryRef)CFBinaryPlistV0CreateWithData(inRequest->bodyPtr, inRequest->bodyOffset, &err);
        require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);
        require_action(CFGetTypeID(requestDict) == CFDictionaryGetTypeID(), exit, status = kHTTPStatus_BadRequest);
    }

    if (requestDict != NULL) {
        requestConfig = (CFDictionaryRef)CFDictionaryGetCFDictionary(requestDict, CFSTR(kAirPlayConfigurationKey_ConfigurationDictionary), &err);
        require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);
    }

    aprs_dlog(kLogLevelTrace, "Configuration change requested: %@\n", requestConfig);

    if (requestConfig != NULL) {

        // Enable first so we don't start setting up if we don't have permissions to administrate in the first place.
        if (CFDictionaryContainsKey(requestConfig, CFSTR(kAirPlayConfigurationKey_EnableHKAccessControl))) {
            Boolean enableHKAccessControl = CFDictionaryGetBoolean(requestConfig, CFSTR(kAirPlayConfigurationKey_EnableHKAccessControl), &err);
            require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);
            err = HISetAccessControlEnabled(inCnx->server, enableHKAccessControl);
            require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);
            shouldUpdateBonjour = true;
        }

        if (CFDictionaryContainsKey(requestConfig, CFSTR(kAirPlayConfigurationKey_DeviceName))) {
            CFStringRef deviceName = CFDictionaryGetCFString(requestConfig, CFSTR(kAirPlayConfigurationKey_DeviceName), &err);
            require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);

            if (AirPlayReceiverServerSetProperty(inCnx->server, 0, CFSTR(kAirPlayKey_Name), NULL, deviceName) == kNoErr) {
                shouldRestartBonjour = true;
            }
        }

        if (CFDictionaryContainsKey(requestConfig, CFSTR(kAirPlayConfigurationKey_Password))) {
            CFStringRef password = CFDictionaryGetCFString(requestConfig, CFSTR(kAirPlayConfigurationKey_Password), &err);
            require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);
            AirPlayReceiverServerSetProperty(inCnx->server, 0, CFSTR(kAirPlayProperty_Password), NULL, password);
            shouldUpdateBonjour = true;
        }

        if (CFDictionaryContainsKey(requestConfig, CFSTR(kAirPlayConfigurationKey_AccessControlLevel))) {
            CFNumberRef accessControlLevel = CFDictionaryGetCFNumber(requestConfig, CFSTR(kAirPlayConfigurationKey_AccessControlLevel), &err);
            require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);
            if (HISetAccessControlLevel(inCnx->server, accessControlLevel) == kNoErr) {
                shouldUpdateBonjour = true;
            }
        }
    }

    responseDict = _AirPlayReceiverServerCreateConfigurationDictionary(inCnx->server, &err);
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

    aprs_dlog(kLogLevelTrace, "Configuration updated: %@\n", responseDict);

    status = AirPlayReceiverSendRequestPlistResponse(inCnx->httpCnx, inRequest, responseDict, &err);

    if (shouldRestartBonjour) {
        _RestartBonjour(inCnx->server);
    } else if (shouldUpdateBonjour) {
        _UpdateBonjour(inCnx->server);
    }
exit:
    if (err)
        aprs_ulog(kLogLevelNotice, "### Configure failed: %#m, %#m\n", status, err);
    CFReleaseNullSafe(requestDict);
    CFReleaseNullSafe(responseDict);
    return (status);
}
#endif

//===========================================================================================================================
//	_requestProcessFeedback
//===========================================================================================================================

static HTTPStatus _requestProcessFeedback(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    HTTPStatus status;
    OSStatus err;
    CFDictionaryRef input = NULL;
    CFDictionaryRef output = NULL;

    if (inRequest->bodyLen) {
        input = (CFDictionaryRef)CFBinaryPlistV0CreateWithData(inRequest->bodyPtr, inRequest->bodyLen, &err);
        require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);
        require_action(CFGetTypeID(input) == CFDictionaryGetTypeID(), exit, status = kHTTPStatus_BadRequest);
    }

    if (inCnx->session) {
        AirPlayReceiverSessionControl(inCnx->session, kCFObjectFlagDirect, CFSTR(kAirPlayCommand_UpdateFeedback), NULL,
            input, &output);
    }

    status = AirPlayReceiverSendRequestPlistResponse(inCnx->httpCnx, inRequest, (output && CFDictionaryGetCount(output) > 0) ? output : NULL, &err);

exit:
    if (err)
        aprs_ulog(kLogLevelNotice, "### Feedback failed: %#m, %#m\n", status, err);
    CFReleaseNullSafe(input);
    CFReleaseNullSafe(output);
    return (status);
}

//===========================================================================================================================
//	_requestProcessFlush
//===========================================================================================================================

static HTTPStatus _requestProcessFlush(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    OSStatus err;
    HTTPStatus status;
    uint32_t lastPlayedTS;
    uint16_t flushUntilSeq16;
    HTTPMessageRef response = inCnx->httpCnx->responseMsg;
    AirPlayFlushPoint flushUntil;

    aprs_ulog(kAirPlayPhaseLogLevel, "Flush (%##a)\n", &inCnx->httpCnx->peerAddr);

    if (!inCnx->didRecord) {
        dlogassert("FLUSH not allowed when not playing");
        status = kHTTPStatus_MethodNotValidInThisState;
        goto exit;
    }

    // Flush everything before the specified seq/ts.

    err = HTTPParseRTPInfo(inRequest->header.buf, inRequest->header.len, &flushUntilSeq16, &flushUntil.ts);
    flushUntil.seq = flushUntilSeq16;
    require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);

#if (AIRPLAY_METRICS)
    AirPlayReceiverMetricsSupplementTimeStampF(inCnx, "FlushReq=%u FlushTS=%u", flushUntil.seq, flushUntil.ts);
#endif

    err = AirPlayReceiverSessionFlushAudio(inCnx->session, NULL, flushUntil, &lastPlayedTS);
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

    err = HTTPHeader_AddFieldF(&response->header, kHTTPHeader_RTPInfo, "rtptime=%u", lastPlayedTS);
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

    status = kHTTPStatus_OK;

exit:
    return (status);
}

//===========================================================================================================================
//    _requestProcessFlushBuffered
//===========================================================================================================================

static HTTPStatus _requestProcessFlushBuffered(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    HTTPStatus status;
    OSStatus err;
    CFDictionaryRef requestParams = NULL;
    uint32_t lastPlayedTS;
    HTTPMessageRef response = inCnx->httpCnx->responseMsg;
    AirPlayFlushPoint flushFrom;
    AirPlayFlushPoint flushUntil;
    AirPlayFlushPoint const* flushFromP = &flushFrom;

    requestParams = (CFDictionaryRef)CFBinaryPlistV0CreateWithData(inRequest->bodyPtr, inRequest->bodyOffset, &err);
    require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);
    require_action(CFGetTypeID(requestParams) == CFDictionaryGetTypeID(), exit, status = kHTTPStatus_BadRequest);

#if (AIRPLAY_METRICS)
    AirPlayReceiverMetricsSupplementTimeStamp(inCnx, requestParams);
#endif

    // The flush-from and flush-from-timestamp are optional but if present they need to appear together
    flushFrom.seq = CFDictionaryGetUInt32(requestParams, CFSTR(kAirPlayKey_FlushFromSeqNum), &err);
    if (!err) {
        flushFrom.ts = CFDictionaryGetUInt32(requestParams, CFSTR(kAirPlayKey_FlushFromTimestamp), &err);
    }
    if (err == kNotFoundErr) {
        flushFromP = NULL;
        err = kNoErr;
    }
    require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);

    // The flush-until and flush-until-timestamp are required
    flushUntil.seq = CFDictionaryGetUInt32(requestParams, CFSTR(kAirPlayKey_FlushUntilSeqNum), &err);
    require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);

    flushUntil.ts = CFDictionaryGetUInt32(requestParams, CFSTR(kAirPlayKey_FlushUntilTimestamp), &err);
    require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);

    err = AirPlayReceiverSessionFlushAudio(inCnx->session, flushFromP, flushUntil, &lastPlayedTS);
    //err = AirPlayReceiverSessionFlushAudio( inCnx->session, flushUntilTS, flushUntilSeq, &lastPlayedTS );
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

    err = HTTPHeader_AddFieldF(&response->header, kHTTPHeader_RTPInfo, "rtptime=%u", lastPlayedTS);
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

    status = kHTTPStatus_OK;

exit:
    CFReleaseNullSafe(requestParams);

    return status;
}

//===========================================================================================================================
//	_requestProcessInfo
//===========================================================================================================================

static HTTPStatus _requestProcessInfo(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    HTTPStatus status;
    OSStatus err;
    CFDictionaryRef requestDict = NULL;
    CFMutableArrayRef qualifier = NULL;
    CFDictionaryRef responseDict = NULL;
    const char* src;
    const char* end;
    const char* namePtr;
    size_t nameLen;
    char* nameBuf;
    uint32_t userVersion = 0;
    size_t len;

    //    if ( !inCnx->session )
    //    {
    //        dlogassert( "/info not allowed before SETUP" );
    //        return( kHTTPStatus_MethodNotValidInThisState );
    //    }

    if (inRequest->bodyLen > 0) {
        requestDict = (CFDictionaryRef)CFBinaryPlistV0CreateWithData(inRequest->bodyPtr, inRequest->bodyLen, &err);
        require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);
        require_action(CFGetTypeID(requestDict) == CFDictionaryGetTypeID(), exit, status = kHTTPStatus_BadRequest);
    }

    qualifier = (CFMutableArrayRef)CFDictionaryGetCFArray(requestDict, CFSTR(kAirPlayKey_Qualifier), NULL);
    if (qualifier)
        CFRetain(qualifier);

    src = inRequest->header.url.queryPtr;
    end = src + inRequest->header.url.queryLen;
    while ((err = URLGetOrCopyNextVariable(src, end, &namePtr, &nameLen, &nameBuf, NULL, NULL, NULL, &src)) == kNoErr) {
        err = CFArrayEnsureCreatedAndAppendCString(&qualifier, namePtr, nameLen);
        if (nameBuf)
            free(nameBuf);
        require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);
    }

    HTTPScanFHeaderValue(inRequest->header.buf, inRequest->header.len, kAirPlayHTTPHeader_ProtocolVersion, "%u", &userVersion);
    if (inCnx->session)
        AirPlayReceiverSessionSetUserVersion(inCnx->session, userVersion);

    responseDict = AirPlayCopyServerInfo(inCnx->server, inCnx->session, qualifier, &err);
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

#if (AIRPLAY_METRICS)
    AirPlayReceiverMetricsSupplementTimeStamp(inCnx, responseDict);
#endif

    src = NULL;
    len = 0;
    GetHeaderValue(inRequest, kHTTPHeader_ContentType, &src, &len);

    if (responseDict && strnicmp_prefix(src, len, kMIMEType_JSON) == 0) {
        status = AirPlayReceiverMetricsSendJSON(inCnx, inRequest, responseDict);
    } else {
        status = AirPlayReceiverSendRequestPlistResponse(inCnx->httpCnx, inRequest, responseDict, &err);
    }

    CFReleaseNullSafe(responseDict);

exit:
    CFReleaseNullSafe(qualifier);
    CFReleaseNullSafe(requestDict);
    if (err)
        aprs_ulog(kLogLevelNotice, "### Get info failed: %#m, %#m\n", status, err);
    return (status);
}

//===========================================================================================================================
//	_requestProcessPairSetupPINStart
//===========================================================================================================================
static HTTPStatus _requestProcessPairSetupPINStart(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    (void)inCnx;
    (void)inRequest;
    return kHTTPStatus_OK;
}

//===========================================================================================================================
//	_requestProcessOptions
//===========================================================================================================================

static HTTPStatus _requestProcessOptions(AirPlayReceiverConnectionRef inCnx)
{
    HTTPMessageRef response = inCnx->httpCnx->responseMsg;

    HTTPHeader_AddFieldF(&response->header, kHTTPHeader_Public, "ANNOUNCE, SETUP, RECORD, PAUSE, FLUSH, FLUSHBUFFERED, TEARDOWN, OPTIONS, POST, GET, PUT");

    return (kHTTPStatus_OK);
}

//===========================================================================================================================
//	_requestProcessPairSetupCoreUtils
//===========================================================================================================================

static HTTPStatus _requestProcessPairSetupCoreUtils(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest, int inPairingType)
{
    HTTPStatus status = kHTTPStatus_InternalServerError;
    OSStatus err = kNoErr;
    char cptr[256] = "";
    char* cstrptr = NULL;
    uint8_t* outputPtr = NULL;
    size_t outputLen = 0L;
    Boolean done = false;
    Boolean hasPassword = false;
    HTTPMessageRef response = inCnx->httpCnx->responseMsg;

    aprs_ulog(kLogLevelNotice, "Control pair-setup HK (%##a), type %d \n", &inCnx->httpCnx->peerAddr, inPairingType);

    AirPlayReceiverServerPlatformGetCString(inCnx->server, CFSTR(kAirPlayProperty_Password), NULL, cptr, sizeof(cptr), NULL);
    hasPassword = *cptr != '\0';

    if (inPairingType == kAirPlayPairingType_Unauthenticated && hasPassword) {
        err = kSecurityRequiredErr;
        status = kHTTPStatus_Forbidden;
        goto exit;
    }

    if (!hasPassword) {
        strcpy(cptr, "3939");
    }

    if (!inCnx->pairSetupSessionHomeKit) {
        aprs_ulog(kLogLevelNotice, "[%s:%d] create Pairing session as it is null\n", __func__, __LINE__);
        err = _createPairingSession(inCnx->server, kPairingSessionType_SetupServer,
            kPairingKeychainAccessGroup_AirPlay, &inCnx->pairSetupSessionHomeKit);
        require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);
        PairingSessionSetKeychainInfo_AirPlaySDK(inCnx->pairSetupSessionHomeKit);

        err = AirPlayCopyHomeKitPairingIdentity(inCnx->server, &cstrptr, NULL);
        require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

        err = PairingSessionSetIdentifier(inCnx->pairSetupSessionHomeKit, cstrptr, strlen(cstrptr));
        require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

        err = PairingSessionSetSetupCode(inCnx->pairSetupSessionHomeKit, cptr, kSizeCString);
        require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);
    }

    if (inPairingType == kAirPlayPairingType_Transient) {
        PairingSessionSetFlags(inCnx->pairSetupSessionHomeKit, kPairingFlag_Transient);
    }

    err = PairingSessionExchange(inCnx->pairSetupSessionHomeKit, inRequest->bodyPtr, inRequest->bodyOffset,
        &outputPtr, &outputLen, &done);
    if (err == kBackoffErr) {
        status = kHTTPStatus_NotEnoughBandwidth;
        goto exit;
    }
    require_noerr_action_quiet(err, exit, status = kHTTPStatus_ConnectionAuthorizationRequired);

    if (done) {
        if (inPairingType == kAirPlayPairingType_Transient) {
            ReplaceCF(&inCnx->pairVerifySessionHomeKit, inCnx->pairSetupSessionHomeKit);
            ForgetCF(&inCnx->pairSetupSessionHomeKit);

            CFRetain(inCnx->httpCnx);
            response->userContext1 = inCnx->httpCnx;
            response->completion = _HandlePairVerifyHomeKitCompletion;
        } else {
            ForgetCF(&inCnx->pairSetupSessionHomeKit);
        }
    }

    err = HTTPMessageSetBodyPtr(response, kMIMEType_Binary, outputPtr, outputLen);
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);
    outputPtr = NULL;

    status = kHTTPStatus_OK;

exit:
    FreeNullSafe(outputPtr);
    FreeNullSafe(cstrptr);
    if (err)
        aprs_ulog(kLogLevelNotice, "### Control pair-setup HK failed: %#m\n", err);
    return (status);
}

//===========================================================================================================================
//	_requestProcessPairSetup
//===========================================================================================================================

static HTTPStatus _requestProcessPairSetup(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    HTTPStatus status = kHTTPStatus_IamATeapot;
    int pairingType = 0;
    Boolean useCoreUtilsPairing = false;

    useCoreUtilsPairing = (HTTPScanFHeaderValue(inRequest->header.buf, inRequest->header.len, kAirPlayHTTPHeader_HomeKitPairing, "%d", &pairingType) == 1);

    if (useCoreUtilsPairing) {
        status = _requestProcessPairSetupCoreUtils(inCnx, inRequest, pairingType);
    } else {
        status = _requestProcessAuthSetup(inCnx, inRequest);
    }

    return (status);
}

static OSStatus _AirPlayReceiverConnectionConfigureEncryptionWithPairingSession(AirPlayReceiverConnectionRef cnx, PairingSessionRef pairingSession)
{
    OSStatus err = kNoErr;
    NetTransportDelegate delegate;
    uint8_t readKey[32];
    uint8_t writeKey[32];

    memset(&delegate, 0, sizeof(delegate));

    err = PairingSessionDeriveKey(pairingSession, kAirPlayPairingControlKeySaltPtr, kAirPlayPairingControlKeySaltLen,
        kAirPlayPairingControlKeyReadInfoPtr, kAirPlayPairingControlKeyReadInfoLen, sizeof(readKey), readKey);
    require_noerr(err, exit);

    err = PairingSessionDeriveKey(pairingSession, kAirPlayPairingControlKeySaltPtr, kAirPlayPairingControlKeySaltLen,
        kAirPlayPairingControlKeyWriteInfoPtr, kAirPlayPairingControlKeyWriteInfoLen, sizeof(writeKey), writeKey);
    require_noerr(err, exit);

    err = NetTransportChaCha20Poly1305Configure(&delegate, aprs_http_ucat(), writeKey, NULL, readKey, NULL);
    require_noerr(err, exit);
    MemZeroSecure(readKey, sizeof(readKey));
    MemZeroSecure(writeKey, sizeof(writeKey));
    HTTPConnectionSetTransportDelegate(cnx->httpCnx, &delegate);

exit:
    return err;
}

//===========================================================================================================================
//
//===========================================================================================================================

static void _HandlePairVerifyHomeKitCompletion(HTTPMessageRef inMsg)
{
    HTTPConnectionRef httpCnx = (HTTPConnectionRef)inMsg->userContext1;
    AirPlayReceiverConnectionRef cnx = httpCnx->delegate.context;
    OSStatus err = kNoErr;
    int pairingType = 0;

    // Set up security for the connection. All future requests and responses will be encrypted.

    require_action(cnx->pairVerifySessionHomeKit, exit, err = kNotPreparedErr);

    err = _AirPlayReceiverConnectionConfigureEncryptionWithPairingSession(cnx, cnx->pairVerifySessionHomeKit);
    require_noerr(err, exit);

    cnx->pairingVerified = true;

    if (HTTPScanFHeaderValue(httpCnx->requestMsg->header.buf, httpCnx->requestMsg->header.len, kAirPlayHTTPHeader_HomeKitPairing, "%d", &pairingType) == 1) {
        if (pairingType == kAirPlayPairingType_HomeKit) {
            cnx->pairingVerifiedAsHomeKit = true;
        }
        if (pairingType == kAirPlayPairingType_HomeKitAdmin) {
            cnx->pairingVerifiedAsHomeKit = true;
            cnx->pairingVerifiedAsHomeKitAdmin = true;
        }
    }

    aprs_ulog(kLogLevelTrace, "Pair-verify HK succeeded\n");

exit:
    if (err) {
        ForgetCF(&cnx->pairVerifySessionHomeKit);
        aprs_ulog(kLogLevelWarning, "### Pair-verify HK completion failed: %#m\n", err);
    }
    CFRelease(httpCnx);
}

//===========================================================================================================================
//	_requestProcessPairVerifyCoreUtils
//===========================================================================================================================

static HTTPStatus _requestProcessPairVerifyCoreUtils(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    HTTPStatus status;
    OSStatus err;
    Boolean done;
    uint8_t* outputPtr = NULL;
    size_t outputLen = 0;
    char* cstrptr = NULL;
    HTTPMessageRef response = inCnx->httpCnx->responseMsg;
    uint32_t pairingType = 0;
    Boolean useHomeKitKeychain = false;

    HTTPScanFHeaderValue(inRequest->header.buf, inRequest->header.len, kAirPlayHTTPHeader_PairDerive, "%d", &inCnx->pairDerive);
    HTTPScanFHeaderValue(inRequest->header.buf, inRequest->header.len, kAirPlayHTTPHeader_HomeKitPairing, "%u", &pairingType);

    useHomeKitKeychain = (pairingType == kAirPlayPairingType_HomeKit || pairingType == kAirPlayPairingType_HomeKitAdmin);

    if (!inCnx->pairVerifySessionHomeKit) {
        aprs_ulog(kLogLevelNotice, "[%s:%d] create Pairing session as it is null\n", __func__, __LINE__);
        err = _createPairingSession(inCnx->server, kPairingSessionType_VerifyServer,
            useHomeKitKeychain ? kPairingKeychainAccessGroup_HomeKit : kPairingKeychainAccessGroup_AirPlay, &inCnx->pairVerifySessionHomeKit);
        require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

        if (useHomeKitKeychain) {
            PairingSessionSetKeychainInfo_AirPlaySDK_HomeKit(inCnx->pairVerifySessionHomeKit);
        } else {
            PairingSessionSetKeychainInfo_AirPlaySDK(inCnx->pairVerifySessionHomeKit);
        }

        if (pairingType == kAirPlayPairingType_HomeKitAdmin) {
            const void* keys[] = { CFSTR(kPairingACL_Admin) };
            const void* values[] = { kCFBooleanTrue };
            CFDictionaryRef acl = CFDictionaryCreate(NULL, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            PairingSessionSetACL(inCnx->pairVerifySessionHomeKit, acl);
            CFRelease(acl);
        }

        err = AirPlayCopyHomeKitPairingIdentity(inCnx->server, &cstrptr, NULL);
        require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

        err = PairingSessionSetIdentifier(inCnx->pairVerifySessionHomeKit, cstrptr, strlen(cstrptr));
        require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);
    }

    err = PairingSessionExchange(inCnx->pairVerifySessionHomeKit, inRequest->bodyPtr, inRequest->bodyOffset, &outputPtr, &outputLen, &done);
    if (err == kNotPreparedErr) {
        status = kHTTPStatus_NetworkAuthenticationRequired;
        goto exit;
    }
    if (err == kSignatureErr) {
        status = kHTTPStatus_Forbidden;
        goto exit;
    }
    require_noerr_action_quiet(err, exit, status = kHTTPStatus_BadRequest);
    if (done) {
        // Prepare to configure the connection for encryption/decryption, but send the response before setting up encryption.

        CFRetain(inCnx->httpCnx);
        response->userContext1 = inCnx->httpCnx;
        response->completion = _HandlePairVerifyHomeKitCompletion;
    }

    err = HTTPMessageSetBodyPtr(response, kMIMEType_Binary, outputPtr, outputLen);
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);
    outputPtr = NULL;

    status = kHTTPStatus_OK;

exit:
    FreeNullSafe(outputPtr);
    FreeNullSafe(cstrptr);
    if (err)
        aprs_ulog(kLogLevelNotice, "### Control pair-verify HK failed: %d, %#m\n", status, err);
    return (status);
}

//===========================================================================================================================
//	_requestProcessPairVerify
//===========================================================================================================================

static HTTPStatus _requestProcessPairVerify(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    HTTPStatus status = kHTTPStatus_IamATeapot;
    int pairingType = 0;
    Boolean useHomeKitPairing = false;

    aprs_ulog(kLogLevelNotice, "Control pair-verify (%##a) %d\n", &inCnx->httpCnx->peerAddr, inCnx->pairingCount + 1);
    ++inCnx->pairingCount;

    HTTPScanFHeaderValue(inRequest->header.buf, inRequest->header.len, kAirPlayHTTPHeader_PairDerive, "%d", &inCnx->pairDerive);

    useHomeKitPairing = (HTTPScanFHeaderValue(inRequest->header.buf, inRequest->header.len, kAirPlayHTTPHeader_HomeKitPairing, "%d", &pairingType) == 1);

    aprs_ulog(kLogLevelNotice, "pair-verify useHomeKitPairing %d pairingType %d\n", useHomeKitPairing, pairingType);

    if (!useHomeKitPairing) {
        _requestReportIfIncompatibleSender(inCnx, inRequest);
        status = kHTTPStatus_Forbidden;
    } else
        status = _requestProcessPairVerifyCoreUtils(inCnx, inRequest);

    return (status);
}

static OSStatus _performPairingExchange(AirPlayReceiverServerRef server, PairingSessionRef* pairingSessionPtr, PairingSessionType pairingSessionType, uint8_t* inputData, size_t inputLen, uint8_t** outputDataPtr, size_t* outputLen)
{
    OSStatus err = kNoErr;
    Boolean done = false;

    require_action(pairingSessionPtr != NULL, bail, err = kParamErr);

    if (!*pairingSessionPtr) {
        err = _createPairingSession(server, pairingSessionType, kPairingKeychainAccessGroup_HomeKit, pairingSessionPtr);
        require_noerr(err, bail);
        PairingSessionSetKeychainInfo_AirPlaySDK_HomeKit(*pairingSessionPtr);
    }

    err = PairingSessionExchange(*pairingSessionPtr, inputData, inputLen, outputDataPtr, outputLen, &done);
    require_noerr(err, bail);

    if (done) {
        ForgetCF(pairingSessionPtr);
        aprs_ulog(kLogLevelNotice, "Admin pairing operation (type %s) is done.\n", PairingSessionTypeToString(pairingSessionType));
    }

bail:
    if (err) {
        aprs_ulog(kLogLevelError, "### Control pair-admin (type %s) failed: %#m\n", PairingSessionTypeToString(pairingSessionType), err);
    }
    return (err);
}

static HTTPStatus _processPairingRequest(AirPlayReceiverConnectionRef connection, HTTPMessageRef inRequest, PairingSessionRef* pairingSessionPtr, PairingSessionType pairingSessionType)
{
    HTTPStatus status = kHTTPStatus_IamATeapot;
    OSStatus err = kNoErr;
    Boolean hkAccessControlEnabled = false;
    Boolean hkPairVerified = true; //connection->pairingVerified;
    Boolean allowHKPairAdminOperation = false;
    uint8_t* pairingExchangeResponseData = NULL;
    size_t pairingExchangeResponseLen = 0L;

    // Pairing administration is authorized on first connection or if we've successfully pair verified.
    hkAccessControlEnabled = HIIsAccessControlEnabled(connection->server);

    allowHKPairAdminOperation = !hkAccessControlEnabled || hkPairVerified;
    require_action(allowHKPairAdminOperation, bail, status = kHTTPStatus_ConnectionAuthorizationRequired);

    err = _performPairingExchange(connection->server, pairingSessionPtr, pairingSessionType, inRequest->bodyPtr, inRequest->bodyOffset, &pairingExchangeResponseData, &pairingExchangeResponseLen);
    require_noerr_action(err, bail, status = kHTTPStatus_InternalServerError);

    err = HTTPMessageSetBodyPtr(connection->httpCnx->responseMsg, kMIMEType_Binary, pairingExchangeResponseData, pairingExchangeResponseLen);
    require_noerr_action(err, bail, status = kHTTPStatus_InternalServerError);

    //The HTTP connection will free the response body
    pairingExchangeResponseData = NULL;
    status = kHTTPStatus_OK;

bail:
    FreeNullSafe(pairingExchangeResponseData);
    return (status);
}

static HTTPStatus _requestProcessPairAdd(AirPlayReceiverConnectionRef connection, HTTPMessageRef inRequest)
{
    return _processPairingRequest(connection, inRequest, &connection->addPairingSession, kPairingSessionType_AddPairingServer);
}

static HTTPStatus _requestProcessPairRemove(AirPlayReceiverConnectionRef connection, HTTPMessageRef inRequest)
{
    return _processPairingRequest(connection, inRequest, &connection->removePairingSession, kPairingSessionType_RemovePairingServer);
}

static HTTPStatus _requestProcessPairList(AirPlayReceiverConnectionRef connection, HTTPMessageRef inRequest)
{
    return _processPairingRequest(connection, inRequest, &connection->listPairingsSession, kPairingSessionType_ListPairingsServer);
}

//===========================================================================================================================
//	_requestProcessGetParameter
//===========================================================================================================================

static HTTPStatus _requestProcessGetParameter(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    HTTPStatus status;
    OSStatus err;
    const char* src;
    const char* end;
    size_t len;
    char tempStr[256];
    char responseBuf[256];
    int n;
    HTTPMessageRef response = inCnx->httpCnx->responseMsg;

    if (!AirPlayReceiverConnectionDidSetup(inCnx)) {
        dlogassert("GET_PARAMETER not allowed before SETUP");
        status = kHTTPStatus_MethodNotValidInThisState;
        goto exit;
    }

    // Check content type.

    err = GetHeaderValue(inRequest, kHTTPHeader_ContentType, &src, &len);
    if (err) {
        dlogassert("No Content-Type header");
        status = kHTTPStatus_BadRequest;
        goto exit;
    }
    if (strnicmpx(src, len, "text/parameters") != 0) {
        dlogassert("Bad Content-Type: '%.*s'", (int)len, src);
        status = kHTTPStatus_HeaderFieldNotValid;
        goto exit;
    }

    // Parse parameters. Each parameter is formatted as <name>\r\n

    src = (const char*)inRequest->bodyPtr;
    end = src + inRequest->bodyOffset;
    while (src < end) {
        char c;
        const char* namePtr;
        size_t nameLen;

        namePtr = src;
        while ((src < end) && ((c = *src) != '\r') && (c != '\n'))
            ++src;
        nameLen = (size_t)(src - namePtr);
        if ((nameLen == 0) || (src >= end)) {
            dlogassert("Bad parameter: '%.*s'", (int)inRequest->bodyOffset, inRequest->bodyPtr);
            status = kHTTPStatus_ParameterNotUnderstood;
            goto exit;
        }

#if (AIRPLAY_METRICS)
        AirPlayReceiverMetricsSupplementTimeStampF(inCnx, "%.*s", (int)nameLen, namePtr);
#endif
        // Process the parameter.

        if (0) {
        }

        // Volume

        else if (strnicmpx(namePtr, nameLen, "volume") == 0) {
            Float32 dbVolume;
#if (AIRPLAY_VOLUME_CONTROL)
            dbVolume = inCnx->server->volume;
            if (inCnx->session && !inCnx->session->isRemoteControlSession) {
                Float32 sessionVolume = (Float32)AirPlayReceiverSessionPlatformGetDouble(inCnx->session, CFSTR(kAirPlayProperty_Volume), NULL, &err);
                if (err == kNoErr) {
                    if (sessionVolume != kAirTunesSilenceVolumeDB) {
                        inCnx->server->volume = dbVolume = Clamp(sessionVolume, kAirTunesMinVolumeDB, kAirTunesMaxVolumeDB);
                        aprs_ulog(kLogLevelNotice, "%s:server volume set to %.1f\n", __ROUTINE__, inCnx->server->volume);
                    } else
                        dbVolume = sessionVolume;
                }
            }
#else
            dbVolume = kAirTunesMinVolumeDB;
            aprs_ulog(kLogLevelNotice, "%s:server volume not supported\n", __ROUTINE__);
#endif

            n = snprintf(responseBuf, sizeof(responseBuf), "volume: %f\r\n", dbVolume);
            char* data = strdup(responseBuf);
            err = HTTPMessageSetBodyPtr(response, "text/parameters", data, (size_t)n);
            require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

#if (AIRPLAY_METRICS)
            AirPlayReceiverMetricsSupplementTimeStampF(inCnx, "%.*s", n, data);
#endif
        }

        // Name

        else if (strnicmpx(namePtr, nameLen, "name") == 0) {
            err = AirPlayGetDeviceName(inCnx->server, tempStr, sizeof(tempStr));
            require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

            n = snprintf(responseBuf, sizeof(responseBuf), "name: %s\r\n", tempStr);
            char* data = strdup(responseBuf);
            err = HTTPMessageSetBodyPtr(response, "text/parameters", data, (size_t)n);
            require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

#if (AIRPLAY_METRICS)
            AirPlayReceiverMetricsSupplementTimeStampF(inCnx, "%.*s", n, data);
#endif
        }

        // Other

        else {
            dlogassert("Unknown parameter: '%.*s'", (int)nameLen, namePtr);
            status = kHTTPStatus_ParameterNotUnderstood;
            goto exit;
        }

        while ((src < end) && (((c = *src) == '\r') || (c == '\n')))
            ++src;
    }

    status = kHTTPStatus_OK;

exit:
    return (status);
}

//===========================================================================================================================
//	_requestProcessSetParameter
//===========================================================================================================================

static HTTPStatus _requestProcessSetParameter(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    HTTPStatus status;
    const char* src;
    size_t len;

    if (!AirPlayReceiverConnectionDidSetup(inCnx)) {
        dlogassert("SET_PARAMETER not allowed before SETUP");
        status = kHTTPStatus_MethodNotValidInThisState;
        goto exit;
    }

    src = NULL;
    len = 0;
    GetHeaderValue(inRequest, kHTTPHeader_ContentType, &src, &len);
    if ((len == 0) && (inRequest->bodyOffset == 0))
        status = kHTTPStatus_OK;
#if (AIRPLAY_META_DATA)
    else if (strnicmp_prefix(src, len, "image/") == 0)
        status = _requestProcessSetParameterArtwork(inCnx, inRequest, src, len);
    else if (strnicmpx(src, len, kMIMEType_DMAP) == 0)
        status = _requestProcessSetParameterDMAP(inCnx, inRequest);
#endif
    else if (strnicmpx(src, len, "text/parameters") == 0)
        status = _requestProcessSetParameterText(inCnx, inRequest);
    else if (MIMETypeIsPlist(src, len))
        status = _requestProcessSetProperty(inCnx, inRequest);
    else {
        dlogassert("Bad Content-Type: '%.*s'", (int)len, src);
        status = kHTTPStatus_HeaderFieldNotValid;
        goto exit;
    }

#if (AIRPLAY_METRICS)
    AirPlayReceiverMetricsSupplementTimeStampF(inCnx, "content type = %.*s", (int)len, src);
#endif

exit:
    return (status);
}

#if (AIRPLAY_META_DATA)

static HTTPStatus
_requestProcessSetParameterArtwork(
    AirPlayReceiverConnectionRef inCnx,
    HTTPMessageRef inRequest,
    const char* inMIMETypeStr,
    size_t inMIMETypeLen)
{
    HTTPStatus status;
    OSStatus err;
    uint32_t u32;
    CFNumberRef applyTS = NULL;
    CFMutableDictionaryRef metaData = NULL;

    err = HTTPParseRTPInfo(inRequest->header.buf, inRequest->header.len, NULL, &u32);
    require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);

    applyTS = CFNumberCreateInt64(u32);
    require_action(applyTS, exit, status = kHTTPStatus_InternalServerError);

    metaData = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(metaData, exit, status = kHTTPStatus_InternalServerError);

    CFDictionarySetCString(metaData, kAirPlayMetaDataKey_ArtworkMIMEType, inMIMETypeStr, inMIMETypeLen);
    if (inRequest->bodyOffset > 0) {
        CFDictionarySetData(metaData, kAirPlayMetaDataKey_ArtworkData, inRequest->bodyPtr, inRequest->bodyOffset);
    } else {
        CFDictionarySetValue(metaData, kAirPlayMetaDataKey_ArtworkData, kCFNull);
    }

    err = AirPlayReceiverSessionSetProperty(inCnx->session, kCFObjectFlagDirect, CFSTR(kAirPlayProperty_MetaData), applyTS, metaData);
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);
    status = kHTTPStatus_OK;
exit:
    CFReleaseNullSafe(applyTS);
    CFReleaseNullSafe(metaData);
    return (status);
}

//===========================================================================================================================
//	_requestProcessSetParameterDMAP
//===========================================================================================================================

static HTTPStatus _requestProcessSetParameterDMAP(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    HTTPStatus status;
    OSStatus err;
    CFNumberRef applyTS = NULL;
    CFMutableDictionaryRef metaData = NULL;
    DMAPContentCode code = kDMAPUndefinedCode;
    const uint8_t* src = 0;
    const uint8_t* end;
    const uint8_t* ptr;
    size_t len = 0;
    int64_t persistentID = 0, itemID = 0, songID = 0;
    Boolean hasPersistentID = false, hasItemID = false, hasSongID = false;
    int64_t s64;
    uint32_t u32;
    uint8_t u8;
    double d;
    CFTypeRef tempObj;

    err = HTTPParseRTPInfo(inRequest->header.buf, inRequest->header.len, NULL, &u32);
    require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);

    applyTS = CFNumberCreateInt64(u32);
    require_action(applyTS, exit, status = kHTTPStatus_InternalServerError);

    metaData = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(metaData, exit, status = kHTTPStatus_InternalServerError);

    end = inRequest->bodyPtr + inRequest->bodyOffset;
    err = _DMAPContentBlock_GetNextChunk(inRequest->bodyPtr, end, &code, &len, &src, NULL);
    require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);
    require_action(code == kDMAPListingItemCode, exit, status = kHTTPStatus_BadRequest);

    end = src + len;
    while (_DMAPContentBlock_GetNextChunk(src, end, &code, &len, &ptr, &src) == kNoErr) {
        switch (code) {
        case kDAAPSongAlbumCode:
            CFDictionarySetCString(metaData, kAirPlayMetaDataKey_Album, ptr, len);
            break;

        case kDAAPSongArtistCode:
            CFDictionarySetCString(metaData, kAirPlayMetaDataKey_Artist, ptr, len);
            break;

        case kDAAPSongComposerCode:
            CFDictionarySetCString(metaData, kAirPlayMetaDataKey_Composer, ptr, len);
            break;

        case kDAAPSongGenreCode:
            CFDictionarySetCString(metaData, kAirPlayMetaDataKey_Genre, ptr, len);
            break;

        case kDMAPItemNameCode:
            CFDictionarySetCString(metaData, kAirPlayMetaDataKey_Title, ptr, len);
            break;

        case kDAAPSongDataKindCode:
            inCnx->session->isLiveBroadcast = ((len == 1) && (ptr[0] == kDAAPSongDataKind_RemoteStream));
            CFDictionarySetBoolean(metaData, kAirPlayMetaDataKey_IsStream, inCnx->session->isLiveBroadcast);
            break;

        case kDAAPSongTrackNumberCode:
            if (len != 2) {
                dlogassert("### Bad track number length: %zu\n", len);
                break;
            }
            s64 = ReadBig16(ptr);
            CFDictionarySetInt64(metaData, kAirPlayMetaDataKey_TrackNumber, s64);
            break;

        case kDAAPSongTrackCountCode:
            if (len != 2) {
                dlogassert("### Bad track count length: %zu\n", len);
                break;
            }
            s64 = ReadBig16(ptr);
            CFDictionarySetInt64(metaData, kAirPlayMetaDataKey_TotalTracks, s64);
            break;

        case kDAAPSongDiscNumberCode:
            if (len != 2) {
                dlogassert("### Bad disc number length: %zu\n", len);
                break;
            }
            s64 = ReadBig16(ptr);
            CFDictionarySetInt64(metaData, kAirPlayMetaDataKey_DiscNumber, s64);
            break;

        case kDAAPSongDiscCountCode:
            if (len != 2) {
                dlogassert("### Bad disc count length: %zu\n", len);
                break;
            }
            s64 = ReadBig16(ptr);
            CFDictionarySetInt64(metaData, kAirPlayMetaDataKey_TotalDiscs, s64);
            break;

        case kDMAPPersistentIDCode:
            if (len != 8) {
                dlogassert("### Bad persistent ID length: %zu\n", len);
                break;
            }
            persistentID = ReadBig64(ptr);
            hasPersistentID = true;
            break;

        case kDAAPSongTimeCode:
            if (len != 4) {
                dlogassert("### Bad song time length: %zu\n", len);
                break;
            }
            s64 = ReadBig32(ptr);
            CFDictionarySetDouble(metaData, kAirPlayMetaDataKey_Duration, s64 / 1000.0 /* ms -> secs */);
            break;

        case kDMAPItemIDCode:
            if (len != 4) {
                dlogassert("### Bad item ID length: %zu\n", len);
                break;
            }
            itemID = ReadBig32(ptr);
            hasItemID = true;
            break;

        case kExtDAAPITMSSongIDCode:
            if (len != 4) {
                dlogassert("### Bad song ID length: %zu\n", len);
                break;
            }
            songID = ReadBig32(ptr);
            hasSongID = true;
            break;

        case kDACPPlayerStateCode:
            if (len != 1) {
                dlogassert("### Bad player state length: %zu\n", len);
                break;
            }
            u8 = *ptr;
            if (u8 == kDACPPlayerState_Paused)
                d = 0.0;
            else if (u8 == kDACPPlayerState_Stopped)
                d = 0.0;
            else if (u8 == kDACPPlayerState_FastFwd)
                d = 2.0;
            else if (u8 == kDACPPlayerState_Rewind)
                d = -1.0;
            else
                d = 1.0;
            CFDictionarySetDouble(metaData, kAirPlayMetaDataKey_Rate, d);
            break;

        case kDAAPSongContentRatingCode:
            if (len != 1) {
                dlogassert("### Bad content rating length: %zu\n", len);
                break;
            }
            u8 = *ptr;
            CFDictionarySetBoolean(metaData, kAirPlayMetaDataKey_Explicit, u8 == kDAAPContentRating_SomeAdvisory);
            break;

        case 0x63654A56: // 'ceJV'
        case 0x63654A43: // 'ceJC'
        case 0x63654A53: // 'ceJS'
            // These aren't needed, but are sent by some clients so ignore to avoid an assert about it.
            break;

        default:
#if (DEBUG)
        {
            const DMAPContentCodeEntry* e;

            e = _DMAPFindEntryByContentCode(code);
            if (e)
                dlog(kLogLevelChatty, "Unhandled DMAP: '%C'  %-36s  %s\n", code, e->name, e->codeSymbol);
        }
#endif
        break;
        }
    }
    if (hasPersistentID)
        CFDictionarySetInt64(metaData, kAirPlayMetaDataKey_UniqueID, persistentID);
    else if (hasItemID)
        CFDictionarySetInt64(metaData, kAirPlayMetaDataKey_UniqueID, itemID);
    else if (hasSongID)
        CFDictionarySetInt64(metaData, kAirPlayMetaDataKey_UniqueID, songID);
    else {
        // Emulate a unique ID from other info that's like to change with each new song.

        s64 = 0;

        tempObj = CFDictionaryGetValue(metaData, kAirPlayMetaDataKey_Album);
        if (tempObj)
            s64 ^= CFHash(tempObj);

        tempObj = CFDictionaryGetValue(metaData, kAirPlayMetaDataKey_Artist);
        if (tempObj)
            s64 ^= CFHash(tempObj);

        tempObj = CFDictionaryGetValue(metaData, kAirPlayMetaDataKey_Title);
        if (tempObj)
            s64 ^= CFHash(tempObj);

        tempObj = CFDictionaryGetValue(metaData, kAirPlayMetaDataKey_TrackNumber);
        if (tempObj)
            s64 ^= CFHash(tempObj);

        CFDictionarySetInt64(metaData, kAirPlayMetaDataKey_UniqueID, s64);
    }

    err = AirPlayReceiverSessionSetProperty(inCnx->session, kCFObjectFlagDirect, CFSTR(kAirPlayProperty_MetaData),
        applyTS, metaData);
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);
    status = kHTTPStatus_OK;
exit:
    CFReleaseNullSafe(applyTS);
    CFReleaseNullSafe(metaData);
    return (status);
}

#endif // AIRPLAY_META_DATA

//===========================================================================================================================
//	_requestProcessSetParameterText
//===========================================================================================================================

static HTTPStatus _requestProcessSetParameterText(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    HTTPStatus status;
    OSStatus err;
    const char* src;
    const char* end;
#if (!(AIRPLAY_VOLUME_CONTROL || AIRPLAY_META_DATA))
    (void)inCnx;
#endif

    // Parse parameters. Each parameter is formatted as <name>: <value>\r\n

    src = (const char*)inRequest->bodyPtr;
    end = src + inRequest->bodyOffset;
    while (src < end) {
        char c;
        const char* namePtr;
        size_t nameLen;
        const char* valuePtr;
        size_t valueLen;

        // Parse the name.

        namePtr = src;
        while ((src < end) && (*src != ':'))
            ++src;
        nameLen = (size_t)(src - namePtr);
        if ((nameLen == 0) || (src >= end)) {
            dlogassert("Bad parameter: '%.*s'", (int)inRequest->bodyOffset, inRequest->bodyPtr);
            status = kHTTPStatus_ParameterNotUnderstood;
            goto exit;
        }
        ++src;
        while ((src < end) && (((c = *src) == ' ') || (c == '\t')))
            ++src;

        // Parse the value.

        valuePtr = src;
        while ((src < end) && ((c = *src) != '\r') && (c != '\n'))
            ++src;
        valueLen = (size_t)(src - valuePtr);

#if (AIRPLAY_METRICS)
        AirPlayReceiverMetricsSupplementTimeStampF(inCnx, "%.*s=%.*s", (int)nameLen, namePtr, (int)valueLen, valuePtr);
#endif

        // Process the parameter.

        if (0) {
        }

#if (AIRPLAY_VOLUME_CONTROL)
        // Volume

        else if (strnicmpx(namePtr, nameLen, "volume") == 0 && inCnx->session && !inCnx->session->isRemoteControlSession) {
            char tempStr[256];

            require_action(valueLen < sizeof(tempStr), exit, status = kHTTPStatus_HeaderFieldNotValid);
            memcpy(tempStr, valuePtr, valueLen);
            tempStr[valueLen] = '\0';

            Float32 dbVolume = (Float32)strtod(tempStr, NULL);
            aprs_ulog(kLogLevelNotice, "%s: dbVolume = %.1f\n", __ROUTINE__, dbVolume);

            err = AirPlayReceiverSessionPlatformSetDouble(inCnx->session, CFSTR(kAirPlayProperty_Volume), NULL, dbVolume);
            require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);
            if (dbVolume != kAirTunesSilenceVolumeDB) {
                inCnx->server->volume = dbVolume;
                aprs_ulog(kLogLevelNotice, "%s:server volume set to %.1f\n", __ROUTINE__, inCnx->server->volume);
            }
        }
#endif

        // Progress

        else if (strnicmpx(namePtr, nameLen, "progress") == 0) {
            unsigned int startTS, currentTS, stopTS;
            int n;

            n = SNScanF(valuePtr, valueLen, "%u/%u/%u", &startTS, &currentTS, &stopTS);
            require_action(n == 3, exit, status = kHTTPStatus_HeaderFieldNotValid);
            require_action(inCnx->session, exit, status = kHTTPStatus_PreconditionFailed);

            TrackTimingMetaData trackTiming = { startTS, stopTS };
            AirPlayReceiverSessionSetProgress(inCnx->session, trackTiming);
        }

        // Other

        else {
            (void)err;
            (void)valueLen;

            dlogassert("Unknown parameter: '%.*s'", (int)nameLen, namePtr);
            status = kHTTPStatus_ParameterNotUnderstood;
            goto exit;
        }

        while ((src < end) && (((c = *src) == '\r') || (c == '\n')))
            ++src;
    }

    status = kHTTPStatus_OK;

exit:
    return (status);
}

//===========================================================================================================================
//	_requestProcessSetProperty
//===========================================================================================================================

static HTTPStatus _requestProcessSetProperty(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    HTTPStatus status;
    OSStatus err;
    CFDictionaryRef requestDict;
    CFStringRef property = NULL;
    CFTypeRef qualifier;
    CFTypeRef value;

    requestDict = (CFDictionaryRef)CFBinaryPlistV0CreateWithData(inRequest->bodyPtr, inRequest->bodyLen, &err);
    //requestDict = CFDictionaryCreateWithBytes( inRequest->bodyPtr, inRequest->bodyOffset, &err );
    require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);

    property = CFDictionaryGetCFString(requestDict, CFSTR(kAirPlayKey_Property), NULL);
    require_action(property, exit, err = kParamErr; status = kHTTPStatus_BadRequest);

    qualifier = CFDictionaryGetValue(requestDict, CFSTR(kAirPlayKey_Qualifier));
    value = CFDictionaryGetValue(requestDict, CFSTR(kAirPlayKey_Value));

    // Set the property on the session.

    require_action(inCnx->session, exit, err = kNotPreparedErr; status = kHTTPStatus_SessionNotFound);
    err = AirPlayReceiverSessionSetProperty(inCnx->session, kCFObjectFlagDirect, property, qualifier, value);
    require_noerr_action_quiet(err, exit, status = kHTTPStatus_UnprocessableEntity);

    status = kHTTPStatus_OK;

exit:
    if (err)
        aprs_ulog(kLogLevelNotice, "### Set property '%@' failed: %#m, %#m\n", property, status, err);
    CFReleaseNullSafe(requestDict);
    return (status);
}

//===========================================================================================================================
//	_requestProcessRecord
//===========================================================================================================================

static HTTPStatus _requestProcessRecord(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    HTTPStatus status;
    OSStatus err;
    const char* src;
    const char* end;
    size_t len;
    const char* namePtr;
    size_t nameLen;
    const char* valuePtr;
    size_t valueLen;
    AirPlayReceiverSessionStartInfo startInfo;

    aprs_ulog(kAirPlayPhaseLogLevel, "Record (%##a)\n", &inCnx->httpCnx->peerAddr);

    if (!AirPlayReceiverConnectionDidSetup(inCnx)) {
        dlogassert("RECORD not allowed before SETUP");
        status = kHTTPStatus_MethodNotValidInThisState;
        goto exit;
    }

    memset(&startInfo, 0, sizeof(startInfo));
    startInfo.clientName = inCnx->clientName;
    startInfo.transportType = inCnx->httpCnx->transportType;

    // Parse session duration info.

    src = NULL;
    len = 0;
    GetHeaderValue(inRequest, kAirPlayHTTPHeader_Durations, &src, &len);
    end = src + len;
    while (HTTPParseParameter(src, end, &namePtr, &nameLen, &valuePtr, &valueLen, NULL, &src) == kNoErr) {
        if (strnicmpx(namePtr, nameLen, "b") == 0)
            startInfo.bonjourMs = TextToInt32(valuePtr, valueLen, 10);
        else if (strnicmpx(namePtr, nameLen, "c") == 0)
            startInfo.connectMs = TextToInt32(valuePtr, valueLen, 10);
        else if (strnicmpx(namePtr, nameLen, "au") == 0)
            startInfo.authMs = TextToInt32(valuePtr, valueLen, 10);
        else if (strnicmpx(namePtr, nameLen, "an") == 0)
            startInfo.announceMs = TextToInt32(valuePtr, valueLen, 10);
        else if (strnicmpx(namePtr, nameLen, "sa") == 0)
            startInfo.setupAudioMs = TextToInt32(valuePtr, valueLen, 10);
        else if (strnicmpx(namePtr, nameLen, "ss") == 0)
            startInfo.setupScreenMs = TextToInt32(valuePtr, valueLen, 10);
    }

    // Start the session.

    err = AirPlayReceiverSessionStart(
        inCnx->session,
        &startInfo);
    if (AirPlayIsBusyError(err)) {
        status = kHTTPStatus_NotEnoughBandwidth;
        goto exit;
    }
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

#if (TARGET_OS_POSIX)
    err = NetworkChangeListenerCreate(&inCnx->networkChangeListener);
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);
    NetworkChangeListenerSetDispatchQueue(inCnx->networkChangeListener, AirPlayReceiverServerGetDispatchQueue(inCnx->server));
    NetworkChangeListenerSetHandler(inCnx->networkChangeListener, _requestNetworkChangeListenerHandleEvent, inCnx);
    NetworkChangeListenerStart(inCnx->networkChangeListener);
#endif

    inCnx->didRecord = true;
    status = kHTTPStatus_OK;

exit:
    return (status);
}

//===========================================================================================================================
//	_requestProcessSetup
//===========================================================================================================================

static HTTPStatus _requestProcessSetup(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
#if (AIRPLAY_SDP_SETUP)
    OSStatus err;
#endif
    HTTPStatus status = kHTTPStatus_BadRequest;
    const char* ptr;
    size_t len;

    ptr = NULL;
    len = 0;
    GetHeaderValue(inRequest, kHTTPHeader_ContentType, &ptr, &len);
    if (MIMETypeIsPlist(ptr, len)) {
        status = _requestProcessSetupPlist(inCnx, inRequest);
        goto exit;
    }

#if (AIRPLAY_SDP_SETUP)
    if (!inCnx->didAnnounce) {
        dlogassert("SETUP not allowed before ANNOUNCE");
        status = kHTTPStatus_MethodNotValidInThisState;
        goto exit;
    }

    err = URLGetNextPathSegment(&inRequest->header.url, &ptr, &len);
    require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);

    ptr = "audio"; // Default to audio for older audio-only clients that don't append /audio.
    len = 5;
    URLGetNextPathSegment(&inRequest->header.url, &ptr, &len);
    if (strnicmpx(ptr, len, "audio") == 0)
        status = _requestProcessSetupSDPAudio(inCnx, inRequest);
    else
#endif // AIRPLAY_SDP_SETUP
    {
        dlogassert("Bad setup URL '%.*s'", (int)inRequest->header.urlLen, inRequest->header.urlPtr);
        status = kHTTPStatus_BadRequest;
        goto exit;
    }

exit:
    return (status);
}

#if (AIRPLAY_SDP_SETUP)
//===========================================================================================================================
//	_requestProcessSetupSDPAudio
//===========================================================================================================================

static HTTPStatus _requestProcessSetupSDPAudio(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    HTTPStatus status;
    OSStatus err;
    CFMutableDictionaryRef requestParams = NULL;
    CFMutableArrayRef requestStreams = NULL;
    CFMutableDictionaryRef requestStreamDesc = NULL;
    CFDictionaryRef responseParams = NULL;
    CFArrayRef outputStreams;
    CFDictionaryRef outputAudioStreamDesc;
    CFIndex i, n;
    const char* src;
    const char* end;
    char* dst;
    char* lim;
    size_t len = 0;
    const char* namePtr;
    size_t nameLen;
    const char* valuePtr;
    size_t valueLen;
    char tempStr[128];
    int tempInt;
    int dataPort, controlPort, eventPort, timingPort;
    Boolean useEvents = false;
    HTTPMessageRef response = inCnx->httpCnx->responseMsg;

    aprs_ulog(kAirPlayPhaseLogLevel, "Setup SDP audio (%##a)\n", &inCnx->httpCnx->peerAddr);

    if (!inCnx->didAnnounce) {
        dlogassert("SETUP not allowed before ANNOUNCE");
        status = kHTTPStatus_MethodNotValidInThisState;
        goto exit;
    }
    if (inCnx->didAudioSetup) {
        dlogassert("SETUP audio not allowed twice");
        status = kHTTPStatus_MethodNotValidInThisState;
        goto exit;
    }

    requestParams = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(requestParams, exit, status = kHTTPStatus_InternalServerError);

    requestStreams = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    require_action(requestStreams, exit, status = kHTTPStatus_InternalServerError);
    CFDictionarySetValue(requestParams, CFSTR(kAirPlayKey_Streams), requestStreams);

    requestStreamDesc = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(requestStreamDesc, exit, status = kHTTPStatus_InternalServerError);
    CFArrayAppendValue(requestStreams, requestStreamDesc);

    // Parse the transport type. The full transport line looks similar to this:
    //
    //		Transport: RTP/AVP/UDP;unicast;interleaved=0-1;mode=record;control_port=6001;timing_port=6002

    err = GetHeaderValue(inRequest, kHTTPHeader_Transport, &src, &len);
    require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);
    end = src + len;

    err = HTTPParseParameter(src, end, &namePtr, &nameLen, NULL, NULL, NULL, &src);
    require_noerr_action(err, exit, status = kHTTPStatus_HeaderFieldNotValid);

    if (strnicmpx(namePtr, nameLen, "RTP/AVP/UDP") == 0) {
    } else {
        dlogassert("Bad transport: '%.*s'", (int)nameLen, namePtr);
        status = kHTTPStatus_HeaderFieldNotValid;
        goto exit;
    }

    CFDictionarySetInt64(requestStreamDesc, CFSTR(kAirPlayKey_Type), kAirPlayStreamType_GeneralAudio);

    // Parse transport parameters.

    while (HTTPParseParameter(src, end, &namePtr, &nameLen, &valuePtr, &valueLen, NULL, &src) == kNoErr) {
        if (strnicmpx(namePtr, nameLen, "control_port") == 0) {
            n = SNScanF(valuePtr, valueLen, "%d", &tempInt);
            require_action(n == 1, exit, status = kHTTPStatus_HeaderFieldNotValid);
            CFDictionarySetInt64(requestStreamDesc, CFSTR(kAirPlayKey_Port_Control), tempInt);
        } else if (strnicmpx(namePtr, nameLen, "timing_port") == 0) {
            n = SNScanF(valuePtr, valueLen, "%d", &tempInt);
            require_action(n == 1, exit, status = kHTTPStatus_HeaderFieldNotValid);
            CFDictionarySetInt64(requestParams, CFSTR(kAirPlayKey_Port_Timing), tempInt);
        } else if (strnicmpx(namePtr, nameLen, "redundant") == 0) {
            n = SNScanF(valuePtr, valueLen, "%d", &tempInt);
            require_action(n == 1, exit, status = kHTTPStatus_HeaderFieldNotValid);
            CFDictionarySetInt64(requestStreamDesc, CFSTR(kAirPlayKey_RedundantAudio), tempInt);
        } else if (strnicmpx(namePtr, nameLen, "mode") == 0) {
            if (strnicmpx(valuePtr, valueLen, "record") == 0) {
            } else if (strnicmpx(valuePtr, valueLen, "screen") == 0) {
                CFDictionarySetBoolean(requestParams, CFSTR(kAirPlayKey_UsingScreen), true);
            } else
                dlog(kLogLevelWarning, "### Unsupported 'mode' value: %.*s%s%.*s\n",
                    (int)nameLen, namePtr, valuePtr ? "=" : "", (int)valueLen, valuePtr);
        } else if (strnicmpx(namePtr, nameLen, "x-events") == 0)
            useEvents = true;
        else if (strnicmpx(namePtr, nameLen, "events") == 0) {
        } // Ignore deprecated event scheme.
        else if (strnicmpx(namePtr, nameLen, "unicast") == 0) {
        } // Ignore
        else if (strnicmpx(namePtr, nameLen, "interleaved") == 0) {
        } // Ignore
        else
            dlog(kLogLevelWarning, "### Unsupported transport param: %.*s%s%.*s\n",
                (int)nameLen, namePtr, valuePtr ? "=" : "", (int)valueLen, valuePtr);
    }

    // Set up compression.

    if (inCnx->compressionType == kAirPlayCompressionType_PCM) {
    } else if (inCnx->compressionType == kAirPlayCompressionType_AAC_LC) {
    }
#if (AIRPLAY_AAC_ELD)
    else if (inCnx->compressionType == kAirPlayCompressionType_AAC_ELD) {
    }
#endif
#if (AIRPLAY_ALAC)
    else if (inCnx->compressionType == kAirPlayCompressionType_ALAC) {
    }
#endif
    else {
        dlogassert("Bad compression: %d", inCnx->compressionType);
        status = kHTTPStatus_HeaderFieldNotValid;
        goto exit;
    }

    CFDictionarySetInt64(requestStreamDesc, CFSTR(kAirPlayKey_CompressionType), inCnx->compressionType);
    CFDictionarySetInt64(requestStreamDesc, CFSTR(kAirPlayKey_FramesPerPacket), inCnx->framesPerPacket);
    CFDictionarySetInt64(requestStreamDesc, CFSTR(kAirPlayKey_LatencyMin), inCnx->minLatency);
    CFDictionarySetInt64(requestStreamDesc, CFSTR(kAirPlayKey_LatencyMax), inCnx->maxLatency);

    // Set up the session.

    if (!inCnx->session) {
        // false indicates that the session is master -> not a remote control session
        err = _requestCreateSession(inCnx, useEvents, false);
        require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

        err = AirPlaySessionManagerAddSession(inCnx->server->sessionMgr, inCnx->session);
        require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);
    }

    if (inCnx->usingEncryption) {
        err = AirPlayReceiverSessionSetSecurityInfo(inCnx->session, inCnx->encryptionKey, inCnx->encryptionIV);
        require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);
    }
    err = AirPlayReceiverSessionSetup(inCnx->session, requestParams, &responseParams);
    require_noerr_action(err, exit, status = (err == ECONNREFUSED) ? kHTTPStatus_OriginError : kHTTPStatus_InternalServerError);

    // Convert the output params to an RTSP transport header.

    outputStreams = CFDictionaryGetCFArray(responseParams, CFSTR(kAirPlayKey_Streams), &err);
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

    outputAudioStreamDesc = NULL;
    n = CFArrayGetCount(outputStreams);
    for (i = 0; i < n; ++i) {
        outputAudioStreamDesc = (CFDictionaryRef)CFArrayGetValueAtIndex(outputStreams, i);
        require_action(CFIsType(outputAudioStreamDesc, CFDictionary), exit, status = kHTTPStatus_InternalServerError);

        tempInt = (int)CFDictionaryGetInt64(outputAudioStreamDesc, CFSTR(kAirPlayKey_Type), NULL);
        if (tempInt == kAirPlayStreamType_GeneralAudio)
            break;
        outputAudioStreamDesc = NULL;
    }
    require_action(outputAudioStreamDesc, exit, status = kHTTPStatus_InternalServerError);

    dataPort = (int)CFDictionaryGetInt64(outputAudioStreamDesc, CFSTR(kAirPlayKey_Port_Data), NULL);
    require_action(dataPort > 0, exit, status = kHTTPStatus_InternalServerError);

    controlPort = (int)CFDictionaryGetInt64(outputAudioStreamDesc, CFSTR(kAirPlayKey_Port_Control), NULL);
    require_action(controlPort > 0, exit, status = kHTTPStatus_InternalServerError);

    eventPort = 0;
    if (useEvents) {
        eventPort = (int)CFDictionaryGetInt64(responseParams, CFSTR(kAirPlayKey_Port_Event), NULL);
        require_action(eventPort > 0, exit, status = kHTTPStatus_InternalServerError);
    }

    timingPort = (int)CFDictionaryGetInt64(responseParams, CFSTR(kAirPlayKey_Port_Timing), NULL);
    require_action(timingPort > 0, exit, status = kHTTPStatus_InternalServerError);

    // Send the response.

    dst = tempStr;
    lim = dst + sizeof(tempStr);
    err = snprintf_add(&dst, lim, "RTP/AVP/UDP;unicast;mode=record;server_port=%d;control_port=%d;timing_port=%d",
        dataPort, controlPort, timingPort);
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

    if (eventPort > 0) {
        err = snprintf_add(&dst, lim, ";event_port=%d", eventPort);
        require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);
    }
    err = HTTPHeader_AddFieldF(&response->header, kHTTPHeader_Transport, "%s", tempStr);
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

    err = HTTPHeader_AddFieldF(&response->header, kHTTPHeader_Session, "1");
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

    *tempStr = '\0';
    AirPlayReceiverServerPlatformGetCString(inCnx->server, CFSTR(kAirPlayProperty_AudioJackStatus), NULL,
        tempStr, sizeof(tempStr), NULL);
    if (*tempStr == '\0')
        strlcpy(tempStr, kAirPlayAudioJackStatus_ConnectedUnknown, sizeof(tempStr));
    err = HTTPHeader_AddFieldF(&response->header, "Audio-Jack-Status", "%s", tempStr);
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

    AirPlayReceiverServerSetBoolean(inCnx->server, CFSTR(kAirPlayProperty_Playing), NULL, true);

    inCnx->didAudioSetup = true;
    inCnx->didAnnounce = true;
    status = kHTTPStatus_OK;

exit:
    CFReleaseNullSafe(requestParams);
    CFReleaseNullSafe(requestStreams);
    CFReleaseNullSafe(requestStreamDesc);
    CFReleaseNullSafe(responseParams);
    return (status);
}

#endif // AIRPLAY_SDP_SETUP

//===========================================================================================================================
//	_requestProcessSetupPlist
//===========================================================================================================================

static HTTPStatus _requestProcessSetupPlist(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    HTTPStatus status;
    OSStatus err;
    CFDictionaryRef requestParams = NULL;
    CFDictionaryRef responseParams = NULL;
    uint8_t sessionUUID[16];
    char cstr[64];
    size_t len;
    uint64_t u64;
    AirPlayEncryptionType et;

    aprs_ulog(kAirPlayPhaseLogLevel, "Setup (%##a)\n", &inCnx->httpCnx->peerAddr);

    requestParams = (CFDictionaryRef)CFBinaryPlistV0CreateWithData(inRequest->bodyPtr, inRequest->bodyOffset, &err);
    require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);
    require_action(CFGetTypeID(requestParams) == CFDictionaryGetTypeID(), exit, status = kHTTPStatus_BadRequest);

#if (AIRPLAY_METRICS)
    AirPlayReceiverMetricsSupplementTimeStamp(inCnx, requestParams);
#endif

    u64 = CFDictionaryGetMACAddress(requestParams, CFSTR(kAirPlayKey_DeviceID), NULL, &err);
    if (!err)
        inCnx->clientDeviceID = u64;

    snprintf(inCnx->ifName, sizeof(inCnx->ifName), "%s", inCnx->httpCnx->ifName);

    CFDictionaryGetMACAddress(requestParams, CFSTR(kAirPlayKey_MACAddress), inCnx->clientInterfaceMACAddress, &err);
    CFDictionaryGetCString(requestParams, CFSTR(kAirPlayKey_Name), inCnx->clientName, sizeof(inCnx->clientName), NULL);
    CFDictionaryGetData(requestParams, CFSTR(kAirPlayKey_SessionUUID), sessionUUID, sizeof(sessionUUID), &len, &err);
    if (!err) {
        require_action(len == sizeof(sessionUUID), exit, err = kSizeErr; status = kHTTPStatus_BadRequest);
        inCnx->clientSessionID = ReadBig64(sessionUUID);
    }

    *cstr = '\0';
    CFDictionaryGetCString(requestParams, CFSTR(kAirPlayKey_SourceVersion), cstr, sizeof(cstr), NULL);
    if (*cstr != '\0')
        inCnx->clientVersion = TextToSourceVersion(cstr, kSizeCString);

    // Set up the session.

    if (!inCnx->session) {
        Boolean isRemoteControlOnly = false;
        isRemoteControlOnly = CFDictionaryGetBoolean(requestParams, CFSTR(kAirPlayKey_IsRemoteControlOnly), NULL);

        AirPlaySessionManagerRef sessionManager = inCnx->server->sessionMgr;
        if (!isRemoteControlOnly && AirPlaySessionManagerIsMasterSessionActive(sessionManager)) {
            _HijackConnections(inCnx->server, inCnx->httpCnx); //rb Need to let session manager know we hijacked
        }
        err = _requestCreateSession(inCnx, true, isRemoteControlOnly);
        require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);
        strlcpy(gAirPlayAudioStats.ifname, inCnx->ifName, sizeof(gAirPlayAudioStats.ifname));

        err = AirPlaySessionManagerAddSession(inCnx->server->sessionMgr, inCnx->session); // if remote control, add to remote control array in session manager
        require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

        if (!inCnx->session->isRemoteControlSession)
            AirPlayReceiverServerSetBoolean(inCnx->server, CFSTR(kAirPlayProperty_Playing), NULL, true);
    }

    if (inCnx->pairVerifySessionHomeKit) {
        AirPlayReceiverSessionSetHomeKitSecurityContext(inCnx->session, inCnx->pairVerifySessionHomeKit);
    } else {
        et = (AirPlayEncryptionType)CFDictionaryGetInt64(requestParams, CFSTR(kAirPlayKey_EncryptionType), &err);
        if (!err && (et != kAirPlayEncryptionType_None)) {
            uint8_t key[16], iv[16];

            err = _requestDecryptKey(inCnx, requestParams, et, key, iv);
            require_noerr_action(err, exit, status = kHTTPStatus_KeyManagementError);

            err = AirPlayReceiverSessionSetSecurityInfo(inCnx->session, key, iv);
            MemZeroSecure(key, sizeof(key));
            require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);
        }
    }

    if (!inCnx->session->isRemoteControlSession)
        _updateGrouping(inCnx->server, requestParams);

    err = AirPlayReceiverSessionSetup(inCnx->session, requestParams, &responseParams);
    require_noerr_action(err, exit, status = kHTTPStatus_BadRequest);

    (void)AirPlayReceiverSendRequestPlistResponse(inCnx->httpCnx, inRequest, responseParams, &err);
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

    inCnx->didAnnounce = true;
    inCnx->didAudioSetup = true;
    inCnx->didScreenSetup = true;
    status = kHTTPStatus_OK;

#if (AIRPLAY_METRICS)
    AirPlayReceiverMetricsSupplementTimeStamp(inCnx, responseParams);
#endif

exit:
    CFReleaseNullSafe(requestParams);
    CFReleaseNullSafe(responseParams);
    if (err)
        aprs_ulog(kLogLevelNotice, "### Setup session failed: %#m\n", err);
    return (status);
}

//===========================================================================================================================
//	_updateGrouping
//===========================================================================================================================

static void _updateGrouping(AirPlayReceiverServerRef inServer, CFDictionaryRef inRequestParams)
{
    CFStringRef groupUUID = CFDictionaryGetCFString(inRequestParams, CFSTR(kAirPlayKey_GroupUUID), NULL);
    Boolean senderSupportsRelay = CFDictionaryGetBoolean(inRequestParams, CFSTR(kAirPlayKey_SenderSupportsRelay), NULL);

    if (groupUUID && senderSupportsRelay) {
        Boolean containsLeader = CFDictionaryGetBoolean(inRequestParams, CFSTR(kAirPlayKey_GroupContainsGroupLeader), NULL);
        // Did the group info change?
        if (!(CFEqualNullSafe(inServer->group.uuid, groupUUID) && inServer->group.containsLeader == containsLeader)) {
            inServer->group.containsLeader = containsLeader;
            inServer->group.isGroupLeader = CFDictionaryGetBoolean(inRequestParams, CFSTR(kAirPlayKey_IsGroupLeader), NULL);
            inServer->group.supportsRelay = senderSupportsRelay;

            ReplaceCF(&inServer->group.uuid, groupUUID);
            AirPlayReceiverServerSetProperty(inServer, 0, CFSTR(kAirPlayKey_GroupUUID), NULL, groupUUID);
            _UpdateBonjour(inServer);
        }
    }
}

static void _clearGrouping(AirPlayReceiverServerRef inServer)
{
    require(inServer, exit);

    CFForget(&inServer->group.uuid);
    inServer->group.containsLeader = false;
    inServer->group.isGroupLeader = false;
    inServer->group.supportsRelay = false;
    _UpdateBonjour(inServer);

    AirPlayReceiverServerSetBoolean(inServer, CFSTR(kAirPlayProperty_Playing), NULL, false);
exit:
    return;
}

//===========================================================================================================================
//	_requestProcessTearDown
//===========================================================================================================================

static HTTPStatus _requestProcessTearDown(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    CFDictionaryRef requestDict = NULL;
    Boolean done = true;

    aprs_ulog(kAirPlayPhaseLogLevel, "TearDown (%##a)\n", &inCnx->httpCnx->peerAddr);

    if (inRequest->bodyOffset > 0) {
        requestDict = (CFDictionaryRef)CFBinaryPlistV0CreateWithData(inRequest->bodyPtr, inRequest->bodyOffset, NULL);
        if (requestDict && (CFGetTypeID(requestDict) != CFDictionaryGetTypeID())) {
            CFRelease(requestDict);
            requestDict = NULL;
        }
    }
    aprs_ulog(kLogLevelNotice, "Teardown %?@\n", log_category_enabled(aprs_ucat(), kLogLevelVerbose), requestDict);

    if (inCnx->session) {
        AirPlayReceiverSessionTearDown(inCnx->session, requestDict, kNoErr, &done);
    }
    if (done) {
        if (inCnx->session && !inCnx->session->isRemoteControlSession)
            _clearGrouping(inCnx->server);
        AirPlaySessionManagerRemoveSession(inCnx->server->sessionMgr, inCnx->session);
#if (TARGET_OS_POSIX)
        _tearDownNetworkListener(inCnx);
#endif
        ForgetCF(&inCnx->session);

        inCnx->didAnnounce = false;
        inCnx->didAudioSetup = false;
        inCnx->didScreenSetup = false;
        inCnx->didRecord = false;
    }

    CFReleaseNullSafe(requestDict);
    return (kHTTPStatus_OK);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_requestCreateSession
//===========================================================================================================================

static OSStatus
_requestCreateSession(
    AirPlayReceiverConnectionRef inCnx,
    Boolean inUseEvents,
    Boolean isRemoteControlOnly)
{
    OSStatus err;
    AirPlayReceiverSessionCreateParams createParams;

    require_action_quiet(!inCnx->session, exit, err = kNoErr);

    memset(&createParams, 0, sizeof(createParams));
    createParams.server = inCnx->server;
    createParams.transportType = inCnx->httpCnx->transportType;
    createParams.peerAddr = &inCnx->httpCnx->peerAddr;
    createParams.clientDeviceID = inCnx->clientDeviceID;
    createParams.clientSessionID = inCnx->clientSessionID;
    createParams.clientVersion = inCnx->clientVersion;
    createParams.useEvents = inUseEvents;
    createParams.connection = inCnx;
    createParams.isRemoteControlSession = isRemoteControlOnly;

    memcpy(createParams.clientIfMACAddr, inCnx->clientInterfaceMACAddress, sizeof(createParams.clientIfMACAddr));
    snprintf(inCnx->ifName, sizeof(inCnx->ifName), "%s", inCnx->httpCnx->ifName);

    err = AirPlayReceiverSessionCreate(&inCnx->session, &createParams);
    require_noerr(err, exit);

exit:
    return (err);
}

//===========================================================================================================================
//	_requestDecryptKey
//===========================================================================================================================

static OSStatus
_requestDecryptKey(
    AirPlayReceiverConnectionRef inCnx,
    CFDictionaryRef inRequestParams,
    AirPlayEncryptionType inType,
    uint8_t inKeyBuf[16],
    uint8_t inIVBuf[16])
{
    OSStatus err;
    const uint8_t* keyPtr;
    size_t len;

    keyPtr = CFDictionaryGetData(inRequestParams, CFSTR(kAirPlayKey_EncryptionKey), NULL, 0, &len, &err);
    require_noerr(err, exit);

    if (0) {
    } else if (inType == kAirPlayEncryptionType_MFi_SAPv1) {
        require_action(inCnx->MFiSAP, exit, err = kAuthenticationErr);
        err = MFiSAP_Decrypt(inCnx->MFiSAP, keyPtr, len, inKeyBuf);
        require_noerr(err, exit);
    } else {
        (void)inCnx;
        aprs_ulog(kLogLevelWarning, "### Bad ET: %d\n", inType);
        err = kParamErr;
        goto exit;
    }

    CFDictionaryGetData(inRequestParams, CFSTR(kAirPlayKey_EncryptionIV), inIVBuf, 16, &len, &err);
    require_noerr(err, exit);
    require_action(len == 16, exit, err = kSizeErr);

exit:
    return (err);
}

//===========================================================================================================================
//	_requestReportIfIncompatibleSender
//===========================================================================================================================

static void _requestReportIfIncompatibleSender(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    const char* userAgentPtr = NULL;
    size_t userAgentLen = 0;
    const char* ptr;
    uint32_t sourceVersion = 0;

    (void)inCnx; // unused

    GetHeaderValue(inRequest, kHTTPHeader_UserAgent, &userAgentPtr, &userAgentLen);
    ptr = (userAgentLen > 0) ? ((const char*)memchr(userAgentPtr, '/', userAgentLen)) : NULL;
    if (ptr) {
        ++ptr;
        sourceVersion = TextToSourceVersion(ptr, (size_t)((userAgentPtr + userAgentLen) - ptr));
    }
    if (sourceVersion < SourceVersionToInteger(200, 30, 0)) {
        aprs_ulog(kLogLevelNotice, "### Reporting incompatible sender: '%.*s'\n", (int)userAgentLen, userAgentPtr);
    }
}

//===========================================================================================================================
//	_requestRequiresAuth
//===========================================================================================================================

static Boolean _requestRequiresAuth(HTTPMessageRef inRequest)
{
    const char* const methodPtr = inRequest->header.methodPtr;
    size_t const methodLen = inRequest->header.methodLen;
    const char* const pathPtr = inRequest->header.url.pathPtr;
    size_t const pathLen = inRequest->header.url.pathLen;

    if (strnicmpx(methodPtr, methodLen, "POST") == 0) {
        if (0) {
        }
#if (AIRPLAY_VODKA)
        else if (strnicmp_suffix(pathPtr, pathLen, "/authorize") == 0)
            return (false);
#endif
        else if (strnicmp_suffix(pathPtr, pathLen, "/auth-setup") == 0)
            return (false);

#if (AIRPLAY_METRICS)
        else if (strnicmp_suffix(pathPtr, pathLen, "/history") == 0)
            return (false);
        else if (strnicmp_suffix(pathPtr, pathLen, "/metrics") == 0)
            return (false);
        else if (strnicmp_suffix(pathPtr, pathLen, "/reset") == 0)
            return (false);
#endif
        else if (strnicmp_suffix(pathPtr, pathLen, "/info") == 0)
            return (false);
        else if (strnicmp_suffix(pathPtr, pathLen, "/pair-pin-start") == 0)
            return (false);
        else if (strnicmp_suffix(pathPtr, pathLen, "/pair-setup-pin") == 0)
            return (false);
        else if (strnicmp_suffix(pathPtr, pathLen, "/pair-setup") == 0)
            return (false);
        else if (strnicmp_suffix(pathPtr, pathLen, "/pair-verify") == 0)
            return (false);
        else if (strnicmp_suffix(pathPtr, pathLen, "/perf") == 0)
            return (false);
    } else if (strnicmpx(methodPtr, methodLen, "GET") == 0) {
        if (strnicmp_suffix(pathPtr, pathLen, "/info") == 0)
            return (false);
        else if (strnicmp_suffix(pathPtr, pathLen, "/log") == 0)
            return (false);
        else if (strnicmp_suffix(pathPtr, pathLen, "/logs") == 0)
            return (false);
#if (AIRPLAY_VIDEO)
        else if (strnicmp_suffix(pathPtr, pathLen, "/server-info") == 0)
            return (false);
        else if (strnicmp_suffix(pathPtr, pathLen, "/slideshow-features") == 0)
            return (false);
#endif
#if (AIRPLAY_METRICS)
        else if (strnicmp_suffix(pathPtr, pathLen, "/metrics") == 0)
            return (false);
        else if (strnicmp_suffix(pathPtr, pathLen, "/reset") == 0)
            return (false);
        else if (strncmp_prefix(pathPtr, pathLen, "/favicon") == 0)
            return (false);
#endif
    }
    return (true);
}

static Boolean _requestRequiresHKAdmin(HTTPMessageRef inRequest)
{
    const char* const pathPtr = inRequest->header.url.pathPtr;
    size_t const pathLen = inRequest->header.url.pathLen;

    return (strnicmp_suffix(pathPtr, pathLen, "/configure") == 0
        || strnicmp_suffix(pathPtr, pathLen, "/pair-add") == 0
        || strnicmp_suffix(pathPtr, pathLen, "/pair-remove") == 0
        || strnicmp_suffix(pathPtr, pathLen, "/pair-list") == 0);
}

//===========================================================================================================================
//	AirPlayReceiverSendRequestPlistResponse
//===========================================================================================================================

HTTPStatus AirPlayReceiverSendRequestPlistResponse(HTTPConnectionRef inCnx, HTTPMessageRef inRequest, CFPropertyListRef inPlist, OSStatus* outErr)
{
    HTTPStatus status;
    OSStatus err = kNoErr;
    const char* httpProtocol;
    const uint8_t* ptr;
    size_t len;
    HTTPMessageRef response = inCnx->responseMsg;

    if (response->header.len == 0) {
        httpProtocol = (strnicmp_prefix(inRequest->header.protocolPtr, inRequest->header.protocolLen, "HTTP") == 0)
            ? "HTTP/1.1"
            : kAirTunesHTTPVersionStr;
        err = HTTPHeader_InitResponse(&response->header, httpProtocol, kHTTPStatus_OK, NULL);
        require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);
        response->bodyLen = 0;
    }

    if (inPlist) {
        ptr = CFBinaryPlistV0Create(inPlist, &len, NULL);
        require_action(ptr, exit, err = kUnknownErr; status = kHTTPStatus_InternalServerError);
        err = HTTPMessageSetBodyPtr(response, kMIMEType_AppleBinaryPlist, ptr, len);
    }
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

    status = kHTTPStatus_OK;

exit:
    if (outErr)
        *outErr = err;
    return (status);
}

#if (TARGET_OS_POSIX)
//===========================================================================================================================
//	_requestNetworkChangeListenerHandleEvent
//===========================================================================================================================

static void _requestNetworkChangeListenerHandleEvent(uint32_t inEvent, void* inContext)
{
    OSStatus err;
    AirPlayReceiverConnectionRef cnx = (AirPlayReceiverConnectionRef)inContext;
    uint32_t flags;
    Boolean sessionDied = false;
    uint64_t otherFlags = 0;

    if (inEvent == kNetworkEvent_Changed) {
        err = SocketGetInterfaceInfo(cnx->httpCnx->sock, cnx->ifName, NULL, NULL, NULL, NULL, &flags, NULL, &otherFlags, NULL);
        if (err) {
            aprs_ulog(kLogLevelInfo, "### Can't get interface's %s info: err = %d; killing session.\n", cnx->ifName, err);
            sessionDied = true;
            goto exit;
        }
        if (!(flags & IFF_RUNNING)) {
            aprs_ulog(kLogLevelInfo, "### Interface %s is not running; killing session.\n", cnx->ifName);
            sessionDied = true;
            goto exit;
        }
        if (otherFlags & kNetInterfaceFlag_Inactive) {
            aprs_ulog(kLogLevelInfo, "### Interface %s is inactive; killing session.\n", cnx->ifName);
            sessionDied = true;
            goto exit;
        }
    }

exit:
    if (sessionDied) {
        dispatch_async_f(cnx->server->httpQueue, cnx, _tearDownNetworkListener);
        AirPlayReceiverServerControl(cnx->server, kCFObjectFlagDirect, CFSTR(kAirPlayCommand_SessionDied), cnx->session, NULL, NULL);
    }
}

static void _tearDownNetworkListener(void* inContext)
{
    AirPlayReceiverConnectionRef cnx = (AirPlayReceiverConnectionRef)inContext;
    if (cnx->networkChangeListener) {
        NetworkChangeListenerSetHandler(cnx->networkChangeListener, NULL, NULL);
        NetworkChangeListenerStop(cnx->networkChangeListener);
        ForgetCF(&cnx->networkChangeListener);
    }
}
#endif

#if (AIRPLAY_META_DATA)

// Include the .i file multiple times with different options to define constants, etc.

#define DMAP_DEFINE_CONTENT_CODE_NAMES 1
#include "APSDMAP.i"

#define DMAP_DEFINE_CONTENT_CODE_TABLE 1
#include "APSDMAP.i"

OSStatus
_DMAPContentBlock_GetNextChunk(
    const uint8_t* inSrc,
    const uint8_t* inEnd,
    DMAPContentCode* outCode,
    size_t* outSize,
    const uint8_t** outData,
    const uint8_t** outSrc)
{
    OSStatus err;
    DMAPContentCode code;
    uint32_t size;

    require_action_quiet(inSrc < inEnd, exit, err = kNotFoundErr);
    require_action((inEnd - inSrc) >= 8, exit, err = kUnderrunErr);
    code = ReadBig32(inSrc);
    inSrc += 4;
    size = ReadBig32(inSrc);
    inSrc += 4;
    require_action(((size_t)(inEnd - inSrc)) >= size, exit, err = kUnderrunErr);

    *outCode = code;
    *outSize = size;
    *outData = inSrc;
    if (outSrc)
        *outSrc = inSrc + size;
    err = kNoErr;

exit:
    return (err);
}

const DMAPContentCodeEntry* _DMAPFindEntryByContentCode(DMAPContentCode inCode)
{
    const DMAPContentCodeEntry* e;

    for (e = kDMAPContentCodeTable; e->code != kDMAPUndefinedCode; ++e) {
        if (e->code == inCode) {
            return (e);
        }
    }
    return (NULL);
}

#endif

#if 0
#pragma mark == TXT Record ==
#endif

static DNSServiceErrorType _txtRecordSetCFString(TXTRecordRef* txtRecord, const char* key, CFStringRef cfString)
{
    char str[512];
    Boolean success;
    DNSServiceErrorType err;

    success = CFStringGetCString(cfString, str, sizeof(str), kCFStringEncodingUTF8);
    if (success) {
        err = TXTRecordSetValue(txtRecord, key, (uint8_t)strlen(str), str);
    } else {
        err = kDNSServiceErr_BadParam;
    }

    return err;
}

static DNSServiceErrorType _txtRecordSetBoolean(TXTRecordRef* txtRecord, const char* key, Boolean flag)
{
    const char* value = flag ? "1" : "0";
    return TXTRecordSetValue(txtRecord, key, (uint8_t)strlen(value), value);
}

static DNSServiceErrorType _txtRecordSetUInt64(TXTRecordRef* txtRef, const char* key, uint64_t features)
{
    int n;
    char cstr[256];
    uint32_t u32 = (uint32_t)((features >> 32) & UINT32_C(0xFFFFFFFF));
    if (u32 != 0) {
        n = snprintf(cstr, sizeof(cstr), "0x%X,0x%X", (uint32_t)(features & UINT32_C(0xFFFFFFFF)), u32);
    } else {
        n = snprintf(cstr, sizeof(cstr), "0x%X", (uint32_t)(features & UINT32_C(0xFFFFFFFF)));
    }
    return TXTRecordSetValue(txtRef, key, (uint8_t)n, cstr);
}

#if (AIRPLAY_METRICS)
//===========================================================================================================================
//    AirPlayReceiverClearHome
//===========================================================================================================================
static HTTPStatus AirPlayReceiverClearHome(AirPlayReceiverConnectionRef inCnx)
{
    HTTPMessageRef response = inCnx->httpCnx->responseMsg;
    if (HISetAccessControlEnabled(inCnx->server, false) == kNoErr) {
        _UpdateBonjour(inCnx->server);
    }
    HTTPMessageSetBodyPtr(response, kMIMEType_Binary, NULL, 0);

    return kHTTPStatus_OK;
}
#endif

#if (AIRPLAY_DACP)
#if 0
#pragma mark == DACP Support ==
#endif
//===========================================================================================================================
//	_HandleDACPStatus
//===========================================================================================================================
static void _HandleDACPStatus(OSStatus inStatus, void* inContext)
{
    (void)inContext;
    aprs_ulog(kLogLevelNotice, "_HandleDACPStatus:%d\n", (int)inStatus);
}
#endif // AIRPLAY_DACP
