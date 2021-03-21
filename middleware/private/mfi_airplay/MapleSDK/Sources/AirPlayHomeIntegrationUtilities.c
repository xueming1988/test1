//
//  Copyright © 2018 Apple Inc. All rights reserved.
//

#include "AirPlayHomeIntegrationUtilities.h"
#include "AirPlayReceiverServer.h"
#include "AirPlayReceiverServerPriv.h"
#include "CFUtils.h"
#include "CommonServices.h"
#include "DebugServices.h"
#include "KeychainUtils.h"
#include "PrintFUtils.h"
#include "StringUtils.h"
#include "UUIDUtils.h"
#include "ed25519.h"

typedef uint64_t SharedKeyStoreAccessGroup;
#define kSharedKeyStoreAccessGroupAirPlay (1)
#define kSharedKeyStoreAccessGroupHomeKit (2)

#define kSharedKeyStoreMaxAccessories (100)

#define kAirPlayHomeKitEnabled CFSTR("HomeKit Enabled")
#define kAirPlayHomeKitAccessLevel CFSTR("HomeKit Access Level")
#define kAirPlayIdentifier CFSTR("identifier")
#define kAirPlayPublicKey CFSTR("pk")
#define kAirPlayPrivateKey CFSTR("sk")
#define kAirPlayPairedAccessories CFSTR("AirPlay Accessories")
#define kHomeKitPairedAccessories CFSTR("HomeKit Accessories")
#define kAirPlayPairedAccessoryIdentifier CFSTR("identifier")
#define kAirPlayPairedAccessoryPermission CFSTR("permissions")
#define kAirPlayPairedAccessoryPublicKey CFSTR("pk")

ulog_define(AirPlayHomeIntegrationUtilities, kLogLevelNotice, kLogFlags_Default, "AirPlay-Home", NULL);
#define appu_ucat() &log_category_from_name(AirPlayHomeIntegrationUtilities)
#define appu_ulog(LEVEL, ...) ulog(appu_ucat(), (LEVEL), __VA_ARGS__)
#define appu_dlog(LEVEL, ...) dlogc(appu_ucat(), (LEVEL), __VA_ARGS__)

#if 0
#pragma mark -
#endif
static CFMutableDictionaryRef _readPairingConfiguration(AirPlayReceiverServerRef server);
static void _savePairingConfiguration(AirPlayReceiverServerRef server, CFDictionaryRef configuration);
static OSStatus _findPeer(
    AirPlayReceiverServerRef server,
    SharedKeyStoreAccessGroup inAccessGroup,
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    uint8_t outPK[32],
    uint64_t* outPermissions);
static OSStatus _savePeer(
    AirPlayReceiverServerRef server,
    SharedKeyStoreAccessGroup inAccessGroup,
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    const uint8_t inPK[32]);
static CFIndex _indexOfPairingIdentifer(
    AirPlayReceiverServerRef server,
    SharedKeyStoreAccessGroup inAccessGroup,
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    uint8_t outPK[32],
    uint64_t* outPermissions);
static CFMutableArrayRef _pairedAccessories(
    AirPlayReceiverServerRef server,
    SharedKeyStoreAccessGroup inAccessGroup);
static Boolean _hasAdminPeer(CFArrayRef pairedAccessories);

#if 0
#pragma mark -
#endif
OSStatus
AirPlayReceiverPairingDelegateCopyIdentity(
    Boolean inAllowCreate,
    char** outIdentifier,
    uint8_t outPK[32],
    uint8_t outSK[32],
    void* inContext)
{
    OSStatus status = kNoErr;
    Boolean update = false;
    CFMutableDictionaryRef configuration = _readPairingConfiguration((AirPlayReceiverServerRef)inContext);

    CFStringRef identifier = (CFStringRef)CFDictionaryGetValue(configuration, kAirPlayIdentifier);
    CFDataRef pk = (CFDataRef)CFDictionaryGetValue(configuration, kAirPlayPublicKey);
    CFDataRef sk = (CFDataRef)CFDictionaryGetValue(configuration, kAirPlayPrivateKey);

    if (outIdentifier && identifier == NULL && !inAllowCreate)
        return kNotFoundErr;
    if (outPK && pk == NULL && !inAllowCreate)
        return kNotFoundErr;
    if (outSK && sk == NULL && !inAllowCreate)
        return kNotFoundErr;

    if (!identifier) {
        uint8_t uuid[16];
        char cstr[64];

        UUIDGet(uuid);
        UUIDtoCString(uuid, false, cstr);

        CFDictionarySetCString(configuration, kAirPlayIdentifier, cstr, strlen(cstr));

        update = true;
    }

    if (pk == NULL || sk == NULL) {
        uint8_t publicKey[32];
        uint8_t secretKey[32];

        Ed25519_make_key_pair(publicKey, secretKey);

        CFDictionarySetData(configuration, kAirPlayPublicKey, publicKey, 32);
        CFDictionarySetData(configuration, kAirPlayPrivateKey, secretKey, 32);

        update = true;
    }

    if (outIdentifier) {
        char cstr[64];

        CFDictionaryGetCString(configuration, kAirPlayIdentifier, cstr, sizeof(cstr), NULL);

        *outIdentifier = strndup(cstr, sizeof(cstr));

        require_action(*outIdentifier, exit, status = kNoMemoryErr);
    }

    if (outPK)
        CFDictionaryGetData(configuration, kAirPlayPublicKey, (void*)outPK, 32, NULL, NULL);
    if (outSK)
        CFDictionaryGetData(configuration, kAirPlayPrivateKey, (void*)outSK, 32, NULL, NULL);
exit:
    if (update)
        _savePairingConfiguration((AirPlayReceiverServerRef)inContext, configuration);

    return status;
}

OSStatus
AirPlayReceiverPairingDelegateFindPeerAirPlay(
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    uint8_t outPK[32],
    uint64_t* outPermissions,
    void* inContext)
{
    OSStatus status = kNoErr;

    require_action_quiet(inContext, exit, status = kParamErr);
    require_action_quiet(inIdentifierPtr && inIdentifierLen > 0, exit, status = kParamErr);

    status = _findPeer(
        (AirPlayReceiverServerRef)inContext,
        kSharedKeyStoreAccessGroupAirPlay,
        inIdentifierPtr,
        inIdentifierLen,
        outPK,
        outPermissions);
exit:
    appu_ulog(kLogLevelTrace, "%s: (status:%#m)\n", __func__, status);
    return status;
}

OSStatus
AirPlayReceiverPairingDelegateSavePeerAirPlay(
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    const uint8_t inPK[32],
    void* inContext)
{
    OSStatus status = kNoErr;

    require_action_quiet(inContext, exit, status = kParamErr);
    require_action_quiet(inIdentifierPtr && inIdentifierLen > 0, exit, status = kParamErr);
    require_action_quiet(inPK, exit, status = kParamErr);

    status = _savePeer((AirPlayReceiverServerRef)inContext,
        kSharedKeyStoreAccessGroupAirPlay,
        inIdentifierPtr,
        inIdentifierLen,
        inPK);
exit:
    appu_ulog(kLogLevelTrace, "%s: (status:%#m)\n", __func__, status);
    return status;
}

#if (AIRPLAY_HOME_CONFIGURATION)
OSStatus
AirPlayReceiverPairingDelegateFindPeerHomeKit(
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    uint8_t outPK[32],
    uint64_t* outPermissions,
    void* inContext)
{
    OSStatus status = kNoErr;

    require_action_quiet(inContext, exit, status = kParamErr);
    require_action_quiet(inIdentifierPtr && inIdentifierLen > 0, exit, status = kParamErr);

    status = _findPeer(
        (AirPlayReceiverServerRef)inContext,
        kSharedKeyStoreAccessGroupHomeKit,
        inIdentifierPtr,
        inIdentifierLen,
        outPK,
        outPermissions);
    if (status == kNotFoundErr) {
        status = _findPeer(
            (AirPlayReceiverServerRef)inContext,
            kSharedKeyStoreAccessGroupAirPlay,
            inIdentifierPtr,
            inIdentifierLen,
            outPK,
            outPermissions);
    }
exit:
    appu_ulog(kLogLevelTrace, "%s: (status:%#m)\n", __func__, status);
    return status;
}

OSStatus
AirPlayReceiverPairingDelegateSavePeerHomeKit(
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    const uint8_t inPK[32],
    void* inContext)
{
    OSStatus status = kNoErr;

    require_action_quiet(inContext, exit, status = kParamErr);
    require_action_quiet(inIdentifierPtr && inIdentifierLen > 0, exit, status = kParamErr);
    require_action_quiet(inPK, exit, status = kParamErr);

    status = _savePeer((AirPlayReceiverServerRef)inContext,
        kSharedKeyStoreAccessGroupHomeKit,
        inIdentifierPtr,
        inIdentifierLen,
        inPK);
exit:
    appu_ulog(kLogLevelTrace, "%s: (status:%#m)\n", __func__, status);
    return status;
}

#if 0
#pragma mark -
#endif
OSStatus AirPlayReceiverPairingDelegateAddPairing(CFDictionaryRef inAccessory, void* inContext)
{
    OSStatus status = kNoErr;
    AirPlayReceiverServerRef server = (AirPlayReceiverServerRef)inContext;
    CFMutableArrayRef pairedAccessories;
    CFIndex accessoryIndex;
    char identifier[64 + 1];

    require_action_quiet(inContext, exit, status = kParamErr);
    require_action_quiet(inAccessory, exit, status = kParamErr);

    CFDictionaryGetCString(inAccessory, kAirPlayPairedAccessoryIdentifier, identifier, sizeof(identifier), &status);
    require_noerr_action_quiet(status, exit, status = kNotFoundErr);

    pairedAccessories = _pairedAccessories(server, kSharedKeyStoreAccessGroupHomeKit);
    server = (AirPlayReceiverServerRef)inContext;
    accessoryIndex = _indexOfPairingIdentifer(server, kSharedKeyStoreAccessGroupHomeKit, identifier, strlen(identifier), NULL, NULL);

    if (accessoryIndex != kCFNotFound) {
        appu_ulog(kLogLevelTrace, "%s: (Removed Duplicate)\n", __func__);
        CFArrayRemoveValueAtIndex(pairedAccessories, accessoryIndex);
    }

    CFArrayInsertValueAtIndex(_pairedAccessories(server, kSharedKeyStoreAccessGroupHomeKit), 0, inAccessory);
    _savePairingConfiguration(server, _readPairingConfiguration(server));
exit:
    appu_ulog(kLogLevelTrace, "%s: (status:%#m)\n", __func__, status);
    return status;
}

OSStatus AirPlayReceiverPairingDelegateRemovePairing(CFDictionaryRef inAccessory, void* inContext)
{
    OSStatus status = kNoErr;
    AirPlayReceiverServerRef server;
    CFIndex accessoryIndex;
    char identifier[64 + 1];

    require_action_quiet(inContext, exit, status = kParamErr);
    require_action_quiet(inAccessory, exit, status = kParamErr);

    CFDictionaryGetCString(inAccessory, kAirPlayPairedAccessoryIdentifier, identifier, sizeof(identifier), &status);
    require_noerr_action_quiet(status, exit, status = kNotFoundErr);

    server = (AirPlayReceiverServerRef)inContext;
    accessoryIndex = _indexOfPairingIdentifer(server, kSharedKeyStoreAccessGroupHomeKit, identifier, strlen(identifier), NULL, NULL);
    if (accessoryIndex != kCFNotFound) {
        CFMutableArrayRef pairedAccessories = _pairedAccessories(server, kSharedKeyStoreAccessGroupHomeKit);
        if (accessoryIndex < CFArrayGetCount(pairedAccessories)) {
            CFArrayRemoveValueAtIndex(pairedAccessories, accessoryIndex);
            _savePairingConfiguration(server, _readPairingConfiguration(server));
        }
    } else {
        status = kNotFoundErr;
    }
exit:
    appu_ulog(kLogLevelTrace, "%s: (status:%#m)\n", __func__, status);
    return status;
}

CFArrayRef AirPlayReceiverPairingDelegateCreateListPairings(OSStatus* outErr, void* inContext)
{
    CFArrayRef outArray = NULL;
    CFMutableArrayRef pairedAccessories = NULL;

    require_quiet(inContext, exit);

    pairedAccessories = _pairedAccessories((AirPlayReceiverServerRef)inContext, kSharedKeyStoreAccessGroupHomeKit);
    if (pairedAccessories != NULL) {
        outArray = CFArrayCreateCopy(NULL, (CFArrayRef)pairedAccessories);
    }
exit:
    if (outErr) {
        *outErr = outArray == NULL ? kNoMemoryErr : kNoErr;
    }
    appu_ulog(kLogLevelTrace, "%s:\n(%@)\n", __func__, outArray);

    return outArray;
}

#if 0
#pragma mark -
#endif
OSStatus HISetAccessControlEnabled(AirPlayReceiverServerRef server, Boolean enabled)
{
    OSStatus status = kNoErr;
    CFMutableDictionaryRef configuration = _readPairingConfiguration(server);

    if (enabled) {
        require_action_string(_hasAdminPeer(_pairedAccessories(server, kSharedKeyStoreAccessGroupHomeKit)), exit, status = kStateErr,
            "Refusing to enable HK without admin peer.");
    }

    if (CFDictionaryGetBoolean(configuration, kAirPlayHomeKitEnabled, NULL) != enabled) {
        CFDictionarySetBoolean(configuration, kAirPlayHomeKitEnabled, enabled);
        if (!enabled) {
            CFDictionarySetInt64(configuration, kAirPlayHomeKitAccessLevel, kAirPlayHomeKitAccessControlLevel_EveryoneOnNetwork);
            CFDictionaryRemoveValue(configuration, kHomeKitPairedAccessories);

            AirPlayReceiverServerSetProperty(server, 0x0, CFSTR(kAirPlayProperty_Password), NULL, CFSTR(""));
        }
        _savePairingConfiguration(server, configuration);
    }
exit:
    return status;
}

Boolean HIIsAccessControlEnabled(AirPlayReceiverServerRef server)
{
    return CFDictionaryGetBoolean(_readPairingConfiguration(server), kAirPlayHomeKitEnabled, NULL);
}

OSStatus HISetAccessControlLevel(AirPlayReceiverServerRef server, CFNumberRef inValue)
{
    OSStatus status = kNoErr;
    int64_t acl = CFGetInt64(inValue, NULL);

    CFMutableDictionaryRef configuration = _readPairingConfiguration(server);
    if (CFDictionaryGetInt64(configuration, kAirPlayHomeKitAccessLevel, NULL) != acl) {
        CFDictionarySetInt64(configuration, kAirPlayHomeKitAccessLevel, acl);
        _savePairingConfiguration(server, configuration);
    } else {
        status = kMismatchErr;
    }

    return status;
}

AirPlayHomeKitAccessControlLevel HIGetAccessControlLevel(AirPlayReceiverServerRef server)
{
    return (AirPlayHomeKitAccessControlLevel)CFDictionaryGetInt64(_readPairingConfiguration(server), kAirPlayHomeKitAccessLevel, NULL);
}

#endif

#if 0
#pragma mark -
#endif
static OSStatus _findPeer(
    AirPlayReceiverServerRef server,
    SharedKeyStoreAccessGroup inAccessGroup,
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    uint8_t outPK[32],
    uint64_t* outPermissions)
{
    OSStatus status = kNoErr;

    require_action_quiet(server, exit, status = kParamErr);
    require_action_quiet(inIdentifierPtr && inIdentifierLen > 0, exit, status = kParamErr);

    if (_indexOfPairingIdentifer(server, inAccessGroup, inIdentifierPtr, inIdentifierLen, outPK, outPermissions) == kCFNotFound)
        status = kNotFoundErr;
exit:
    return status;
}

static OSStatus _savePeer(
    AirPlayReceiverServerRef server,
    SharedKeyStoreAccessGroup inAccessGroup,
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    const uint8_t inPK[32])
{
    OSStatus status = kNoErr;
    CFIndex accessoryIndex = kCFNotFound;
    CFMutableDictionaryRef accessoryInfo = NULL;
    CFMutableDictionaryRef configuration = NULL;
    CFMutableArrayRef pairedAccessories = NULL;

    require_action_quiet(server, exit, status = kParamErr);
    require_action_quiet(inIdentifierPtr && inIdentifierLen > 0, exit, status = kParamErr);
    require_action_quiet(inPK, exit, status = kParamErr);

    configuration = _readPairingConfiguration(server);
    accessoryIndex = _indexOfPairingIdentifer(server, inAccessGroup, inIdentifierPtr, inIdentifierLen, NULL, NULL);
    pairedAccessories = _pairedAccessories(server, inAccessGroup);

    if (accessoryIndex == kCFNotFound) {
        accessoryInfo = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require_action_quiet(accessoryInfo, exit, status = kNoMemoryErr);

        CFDictionarySetData(accessoryInfo, kAirPlayPairedAccessoryPublicKey, inPK, 32);
        CFDictionarySetCString(accessoryInfo, kAirPlayPairedAccessoryIdentifier, inIdentifierPtr, inIdentifierLen);
    } else {
        accessoryInfo = CFDictionaryCreateMutableCopy(NULL, 0, CFArrayGetValueAtIndex(pairedAccessories, accessoryIndex));
        require_action_quiet(accessoryInfo, exit, status = kNoMemoryErr);

        CFDictionarySetData(accessoryInfo, kAirPlayPairedAccessoryPublicKey, inPK, 32);
        CFArrayRemoveValueAtIndex(pairedAccessories, accessoryIndex);
    }
    CFArrayInsertValueAtIndex(pairedAccessories, 0, accessoryInfo);

    _savePairingConfiguration(server, configuration);
exit:
    CFReleaseNullSafe(accessoryInfo);

    return status;
}

static CFIndex _indexOfPairingIdentifer(
    AirPlayReceiverServerRef server,
    SharedKeyStoreAccessGroup inAccessGroup,
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    uint8_t outPK[32],
    uint64_t* outPermissions)
{
    CFIndex outIndex = kCFNotFound;
    CFIndex index;

    CFArrayRef pairedAccessories = _pairedAccessories(server, inAccessGroup);
    require_quiet(pairedAccessories, exit);

    for (index = 0; index < CFArrayGetCount(pairedAccessories); index++) {
        OSStatus err = kNotFoundErr;

        char identifier[64 + 1];

        CFDictionaryRef accessory = CFArrayGetValueAtIndex(pairedAccessories, index);
        CFDictionaryGetCString(accessory, kAirPlayPairedAccessoryIdentifier, identifier, sizeof(identifier), &err);
        if (err == kNoErr && memcmp(identifier, inIdentifierPtr, inIdentifierLen) == 0 && inIdentifierLen == strlen(identifier)) {
            outIndex = index;
            if (outPK)
                CFDictionaryGetData(accessory, kAirPlayPairedAccessoryPublicKey, outPK, 32, NULL, NULL);
            if (outPermissions)
                *outPermissions = CFDictionaryGetInt64(accessory, kAirPlayPairedAccessoryPermission, NULL);
            break;
        }
    }
exit:
    return outIndex;
}

static CFMutableArrayRef _pairedAccessories(
    AirPlayReceiverServerRef server,
    SharedKeyStoreAccessGroup inAccessGroup)
{
    CFMutableDictionaryRef configuration = _readPairingConfiguration(server);
    CFStringRef accessoryGroup = inAccessGroup == kSharedKeyStoreAccessGroupAirPlay ? kAirPlayPairedAccessories : kHomeKitPairedAccessories;
    CFMutableArrayRef accessories = (CFMutableArrayRef)CFDictionaryGetValue(configuration, accessoryGroup);

    if (accessories == NULL) {
        accessories = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
        CFDictionarySetValue(configuration, accessoryGroup, accessories);
        CFRelease(accessories);
    }

    return accessories;
}

static Boolean _hasAdminPeer(CFArrayRef pairedAccessories)
{
    CFIndex index;

    for (index = 0; index < CFArrayGetCount(pairedAccessories); index++) {
        CFDictionaryRef accessoryInfo = CFArrayGetValueAtIndex(pairedAccessories, index);
        PairingPermissions permissions = CFDictionaryGetUInt32(accessoryInfo, kAirPlayPairedAccessoryPermission, NULL);
        if (permissions & kPairingPermissionsAdmin) {
            return true;
        }
    }
    return false;
}

#if 0
#pragma mark -
#endif
static CFMutableDictionaryRef _readPairingConfiguration(AirPlayReceiverServerRef server)
{
    static CFMutableDictionaryRef sPairingConfigurationDictionary = NULL;

    if (sPairingConfigurationDictionary == NULL)
        sPairingConfigurationDictionary = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFMutableDictionaryRef pairingConfigurationDictionary = NULL;
    CFStringRef serverIdentifier = NULL;

    char cstr[256];
    serverIdentifier = CFStringCreateWithCString(NULL, MACAddressToCString(server->deviceID, cstr), kCFStringEncodingUTF8);
    require(serverIdentifier, exit);

    pairingConfigurationDictionary = (CFMutableDictionaryRef)CFDictionaryGetValue(sPairingConfigurationDictionary, serverIdentifier);
    if (pairingConfigurationDictionary == NULL) {

        OSStatus err = kNoErr;
        CFTypeRef configuration = AirPlayReceiverServerCopyProperty(server, 0, CFSTR(kAirPlayProperty_HomeIntegrationData), NULL, &err);
        CFMutableDictionaryRef configurationDictionary = NULL;
        if (err == kNoErr && configuration != NULL) {
            if (CFGetTypeID(configuration) == CFDataGetTypeID())
                configurationDictionary = (CFMutableDictionaryRef)CFPropertyListCreateWithData(NULL, (CFDataRef)configuration, kCFPropertyListMutableContainers, NULL, NULL);
        }

        if (configurationDictionary == NULL)
            configurationDictionary = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

        CFDictionarySetValue(sPairingConfigurationDictionary, serverIdentifier, configurationDictionary);
        pairingConfigurationDictionary = (CFMutableDictionaryRef)CFDictionaryGetValue(sPairingConfigurationDictionary, serverIdentifier);

        CFReleaseNullSafe(configurationDictionary);
        CFReleaseNullSafe(configuration);
    }
exit:
    CFForget(&serverIdentifier);

    return pairingConfigurationDictionary;
}

static void _savePairingConfiguration(AirPlayReceiverServerRef server, CFDictionaryRef configuration)
{
    require_quiet(server, exit);
    require_quiet(configuration, exit);

    CFMutableArrayRef hkAccessories = _pairedAccessories(server, kSharedKeyStoreAccessGroupHomeKit);
    CFMutableArrayRef apAccessories = _pairedAccessories(server, kSharedKeyStoreAccessGroupAirPlay);

    if (CFArrayGetCount(hkAccessories) + CFArrayGetCount(apAccessories) > kSharedKeyStoreMaxAccessories) {
        if (HIIsAccessControlEnabled(server) && CFArrayGetCount(apAccessories) > 0) {
            CFArrayRemoveAllValues(apAccessories);
        }
    }

    appu_ulog(kLogLevelTrace, "%@", configuration);

    CFDataRef data = CFPropertyListCreateData(NULL, configuration, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
    require_quiet(data, exit);

    AirPlayReceiverServerSetProperty(server, 0, CFSTR(kAirPlayProperty_HomeIntegrationData), NULL, (CFTypeRef)data);
    CFRelease(data);
exit:
    return;
}
