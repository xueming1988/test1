/*
	Copyright (C) 2010-2019 Apple Inc. All Rights Reserved.
*/

#include <stdlib.h>
#include <string.h>

#include <Security/SecPolicyPriv.h>
#include <Security/Security.h>

#include "CommonServices.h"
#include "DebugServices.h"
#include "MFiCertificates.h"
#include "MFiSAP.h"
#include "SoftLinking.h"
#include "SystemUtils.h"

#include CF_HEADER
#include SHA_HEADER

#if (TARGET_OS_IPHONE)
#include <Security/SecCertificatePriv.h>
#include <Security/SecCMS.h>
#endif

//===========================================================================================================================

#if (TARGET_OS_IPHONE)
#define MFiAuthFrameworkName IAPAuthentication

SOFT_LINK_FRAMEWORK(PrivateFrameworks, IAPAuthentication)

SOFT_LINK_FUNCTION(IAPAuthentication, MFiVerifyCertificateSerialNumber,
    OSStatus,
    (const void* inSNPtr, size_t inSNLen),
    (inSNPtr, inSNLen))
#define MFiVerifyCertificateSerialNumber soft_MFiVerifyCertificateSerialNumber
#else
#define MFiAuthFrameworkName MFiAuthentication

SOFT_LINK_FRAMEWORK(PrivateFrameworks, MFiAuthentication)

SOFT_LINK_FUNCTION(MFiAuthentication, MFiVerifyCertificateSerialNumber,
    OSStatus,
    (const void* inSNPtr, size_t inSNLen),
    (inSNPtr, inSNLen))
#define MFiVerifyCertificateSerialNumber soft_MFiVerifyCertificateSerialNumber
#endif

//===========================================================================================================================

static OSStatus
_SecCertificateCreateWithBytes(
    const void* inPtr,
    size_t inLen,
    SecCertificateRef* outCertificate);
static OSStatus
_SecKeyVerifySignedSHA1(
    SecKeyRef inPublicKey,
    const uint8_t* inSHA1,
    const uint8_t* inSignaturePtr,
    size_t inSignatureLen);
static OSStatus _VerifySerialNumber(const uint8_t* inSNPtr, size_t inSNLen);

ulog_define(MFiClientCore, kLogLevelNotice, kLogFlags_Default, "MFiClient", NULL);
#define mfic_ulog(LEVEL, ...) ulog(&log_category_from_name(MFiClientCore), (LEVEL), __VA_ARGS__)

//===========================================================================================================================

OSStatus
MFiPlatform_VerifySignature(
    const void* inDataPtr,
    size_t inDataLen,
    const void* inSignaturePtr,
    size_t inSignatureLen,
    const void* inCertificatePtr,
    size_t inCertificateLen)
{
    OSStatus err;
    CFMutableArrayRef certificateArray;
    CFMutableArrayRef trustedCertificateArray = NULL;
    CFArrayRef tempCFArray = NULL;
    CFDataRef tempCFData;
    SecCertificateRef leafCertificateRef;
    SecCertificateRef certificateRef;
    SecPolicyRef policyRef;
    SecTrustRef trustRef = NULL;
    SecTrustResultType trustResult;
    SecKeyRef publicKeyRef;
    uint8_t digest[SHA_DIGEST_LENGTH];
    CFDataRef snData;

    mfic_ulog(kLogLevelVerbose, "MFi challenge:\n%1.2H\n", inDataPtr, (int)inDataLen, (int)inDataLen);
    mfic_ulog(kLogLevelVerbose, "MFi certificate:\n%1.2H\n", inCertificatePtr, (int)inCertificateLen, (int)inCertificateLen);
    mfic_ulog(kLogLevelVerbose, "MFi signature:\n%1.2H\n", inSignaturePtr, (int)inSignatureLen, (int)inSignatureLen);

    certificateArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    require_action(certificateArray, exit, err = kNoMemoryErr);

    // Convert the PKCS#7 message containing the certificate into a SecCertificateRef.

    tempCFData = CFDataCreate(NULL, (const uint8_t*)inCertificatePtr, (CFIndex)inCertificateLen);
    require_action(tempCFData, exit, err = kNoMemoryErr);

#if (TARGET_OS_IPHONE)
    tempCFArray = SecCMSCertificatesOnlyMessageCopyCertificates(tempCFData);
    CFRelease(tempCFData);
    require_action(tempCFArray, exit, err = kMalformedErr);
#else
    {
        SecExternalFormat format;

        format = kSecFormatPKCS7;
        err = SecItemImport(tempCFData, NULL, &format, NULL, 0, NULL, NULL, &tempCFArray);
        CFRelease(tempCFData);
        require_noerr(err, exit);
    }
#endif

    require_action(CFArrayGetCount(tempCFArray) > 0, exit, err = kCountErr);
    leafCertificateRef = (SecCertificateRef)CFArrayGetValueAtIndex(tempCFArray, 0);
    require_action(CFGetTypeID(leafCertificateRef) == SecCertificateGetTypeID(), exit, err = kTypeErr);
    CFArrayAppendValue(certificateArray, leafCertificateRef);

// Create a SecTrustRef to verify the signature.

#if (TARGET_OS_IPHONE)
    policyRef = SecPolicyCreateiAP();
#else
    policyRef = SecPolicyCreateBasicX509();
#endif
    require_action(policyRef, exit, err = kUnknownErr);

    err = SecTrustCreateWithCertificates(certificateArray, policyRef, &trustRef);
    CFRelease(policyRef);
    require_noerr(err, exit);

#if (TARGET_OS_MACOSX)
    err = SecTrustSetOptions(trustRef, kSecTrustOptionAllowExpired | kSecTrustOptionAllowExpiredRoot);
    require_noerr(err, exit);
#endif

// Tell the Security framework not to use any Keychains.

#if (!TARGET_OS_IPHONE)
    {
        CFArrayRef emptyArray;

        emptyArray = CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
        require_action(emptyArray, exit, err = kNoMemoryErr);
        err = SecTrustSetKeychains(trustRef, emptyArray);
        CFRelease(emptyArray);
        require_noerr(err, exit);
    }
#endif

    // Add trusted certificates for the sub-CA.

    trustedCertificateArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    require_action(trustedCertificateArray, exit, err = kNoMemoryErr);

    err = _SecCertificateCreateWithBytes(kiPodAccessoryCA1, kiPodAccessoryCA1Len, &certificateRef);
    require_noerr(err, exit);
    CFArrayAppendValue(trustedCertificateArray, certificateRef);
    CFRelease(certificateRef);

    err = _SecCertificateCreateWithBytes(kiPodAccessoryCA2, kiPodAccessoryCA2Len, &certificateRef);
    require_noerr(err, exit);
    CFArrayAppendValue(trustedCertificateArray, certificateRef);
    CFRelease(certificateRef);

    err = SecTrustSetAnchorCertificates(trustRef, trustedCertificateArray);
    require_noerr(err, exit);

    err = SecTrustSetAnchorCertificatesOnly(trustRef, true);
    require_noerr(err, exit);

    // Verify the certificate and signature.

    err = SecTrustEvaluate(trustRef, &trustResult);
    require_noerr(err, exit);
    require_action_quiet(
        (trustResult == kSecTrustResultProceed) || (trustResult == kSecTrustResultUnspecified),
        exit, err = kIntegrityErr);

#if (TARGET_OS_IPHONE)
    publicKeyRef = SecTrustCopyPublicKey(trustRef);
    require_action(publicKeyRef, exit, err = kUnknownErr);
#else
    err = SecCertificateCopyPublicKey(leafCertificateRef, &publicKeyRef);
    require_noerr(err, exit);
#endif

    SHA1(inDataPtr, inDataLen, digest);
    err = _SecKeyVerifySignedSHA1(publicKeyRef, digest, inSignaturePtr, inSignatureLen);
    CFRelease(publicKeyRef);
    require_noerr_quiet(err, exit);

// Verify that the certificate hasn't been revoked.

#if (TARGET_OS_IPHONE)
    snData = SecCertificateCopySerialNumber(leafCertificateRef);
#else
    snData = SecCertificateCopySerialNumber(leafCertificateRef, NULL);
#endif
    check(snData);
    if (snData) {
        err = _VerifySerialNumber(CFDataGetBytePtr(snData), (size_t)CFDataGetLength(snData));
        CFRelease(snData);
        require_noerr_quiet(err, exit);
    }

exit:
    CFReleaseNullSafe(trustRef);
    CFReleaseNullSafe(tempCFArray);
    CFReleaseNullSafe(trustedCertificateArray);
    CFReleaseNullSafe(certificateArray);
    return (err);
}

//===========================================================================================================================

static OSStatus
_SecCertificateCreateWithBytes(
    const void* inPtr,
    size_t inLen,
    SecCertificateRef* outCertificate)
{
    OSStatus err;
    SecCertificateRef certificateRef;
    CFDataRef data;

    data = CFDataCreate(NULL, (const uint8_t*)inPtr, (CFIndex)inLen);
    require_action(data, exit, err = kNoMemoryErr);

    certificateRef = SecCertificateCreateWithData(NULL, data);
    CFRelease(data);
    require_action(certificateRef, exit, err = kMalformedErr);

    *outCertificate = certificateRef;
    err = kNoErr;

exit:
    return (err);
}

#if (TARGET_OS_IPHONE)
//===========================================================================================================================

static OSStatus
_SecKeyVerifySignedSHA1(
    SecKeyRef inPublicKey,
    const uint8_t* inSHA1,
    const uint8_t* inSignaturePtr,
    size_t inSignatureLen)
{
    return (SecKeyRawVerify(inPublicKey, kSecPaddingPKCS1SHA1, inSHA1, SHA_DIGEST_LENGTH, inSignaturePtr, inSignatureLen));
}
#endif

#if (!TARGET_OS_IPHONE)
//===========================================================================================================================

static OSStatus
_SecKeyVerifySignedSHA1(
    SecKeyRef inPublicKey,
    const uint8_t* inSHA1,
    const uint8_t* inSignaturePtr,
    size_t inSignatureLen)
{
    OSStatus err;
    SecTransformRef transform;
    CFDataRef tempData;
    CFTypeRef result;

    transform = SecVerifyTransformCreate(inPublicKey, NULL, NULL);
    require_action(transform, exit, err = kUnknownErr);

    SecTransformSetAttribute(transform, kSecInputIsAttributeName, kSecInputIsDigest, NULL);
    SecTransformSetAttribute(transform, kSecDigestTypeAttribute, kSecDigestSHA1, NULL);

    tempData = CFDataCreate(NULL, inSHA1, SHA_DIGEST_LENGTH);
    require_action(tempData, exit, err = kNoMemoryErr);
    SecTransformSetAttribute(transform, kSecTransformInputAttributeName, tempData, NULL);
    CFRelease(tempData);

    tempData = CFDataCreate(NULL, inSignaturePtr, (CFIndex)inSignatureLen);
    require_action(tempData, exit, err = kNoMemoryErr);
    SecTransformSetAttribute(transform, kSecSignatureAttributeName, tempData, NULL);
    CFRelease(tempData);

    result = SecTransformExecute(transform, NULL);
    err = (result == kCFBooleanTrue) ? kNoErr : kAuthenticationErr;
    if (result)
        CFRelease(result);

exit:
    if (transform)
        CFRelease(transform);
    return (err);
}
#endif

//===========================================================================================================================

static OSStatus _VerifySerialNumber(const uint8_t* inSNPtr, size_t inSNLen)
{
    OSStatus err;

    mfic_ulog(kLogLevelInfo, "MFi certificate serial number: %.3H\n", inSNPtr, (int)inSNLen, 128);

    if (MemEqual(inSNPtr, inSNLen, "\x33\x33\xAA\x12\x12\x19\xAA\x07\xAA\x00\x06\xAA\x00\x00\x01", 15)) {
        mfic_ulog(kLogLevelInfo, "Allowing MFi test certificate serial number\n");
        err = kNoErr;
        goto exit;
    }

#if (TARGET_OS_IPHONE)
    require_action_quiet(SOFT_LINK_HAS_FUNCTION(IAPAuthentication, MFiVerifyCertificateSerialNumber), exit, err = kNoErr;
                         mfic_ulog(kLogLevelError, "Skipping MFi certificate serial number check (no func)\n"));
#else
    require_action_quiet(SOFT_LINK_HAS_FUNCTION(MFiAuthentication, MFiVerifyCertificateSerialNumber), exit, err = kNoErr;
                         mfic_ulog(kLogLevelError, "Skipping MFi certificate serial number check (no func)\n"));
#endif

    err = MFiVerifyCertificateSerialNumber(inSNPtr, inSNLen);
    require_noerr_action_quiet(err, exit,
        mfic_ulog(kLogLevelInfo, "### Bad MFi certificate SN: %#m\n", err));

exit:
    return (err);
}

#if 0
#pragma mark -
#endif

#if (!EXCLUDE_UNIT_TESTS)

#include "StringUtils.h"

//===========================================================================================================================

typedef struct
{
    const char* cert;
    const char* challenge;
    const char* signature;
    Boolean shouldPass;

} MFiClientPlatformTestVector;

static const MFiClientPlatformTestVector kMFiClientPlatformTestVectors[] = {
    // Test 1: AirPort Express (white, AppleTV-looking one). Apple Inc. certificate chain.
    {
        "3082038a06092a864886f70d010702a082037b308203770201013100300b0609"
        "2a864886f70d010701a082035d3082035930820241a003020102020f0101aa11"
        "0407aa00aa0024aa007052300d06092a864886f70d0101050500308183310b30"
        "0906035504061302555331133011060355040a130a4170706c6520496e632e31"
        "263024060355040b131d4170706c652043657274696669636174696f6e204175"
        "74686f72697479313730350603550403132e4170706c652069506f6420416363"
        "6573736f726965732043657274696669636174696f6e20417574686f72697479"
        "301e170d3131303430373031313331385a170d3139303430373031313331385a"
        "3070310b300906035504061302555331133011060355040a130a4170706c6520"
        "496e632e311f301d060355040b13164170706c652069506f6420416363657373"
        "6f72696573312b3029060355040314224950415f303130314141313130343037"
        "41413030414130303234414130303730353230819f300d06092a864886f70d01"
        "0101050003818d0030818902818100ad38e83497c98e77b855351de17565fa49"
        "989590a90c339de0e843f1a36625607bf9194047fd5df8c1b8cb5b6a5b55b149"
        "bd218baa5dcb24cd1e936b6736593d2046723080bf7407c729236d058ff60ae5"
        "4cf257f33ba49b3f039eb4d5f7a84ba6b6bffe9198a0d47600720e087dce78f3"
        "3baa05edda87200a315fbaaf317db50203010001a360305e300e0603551d0f01"
        "01ff0404030203b8300c0603551d130101ff04023000301d0603551d0e041604"
        "14aca964d23f65c089503a6c635a6fe63ed19ec193301f0603551d2304183016"
        "8014ff4b1a439af51996ab18002b61c9ee409d8ec704300d06092a864886f70d"
        "010105050003820101001188bf69635c9ba31b8528c26f4130e04320f23527f8"
        "a9f27f815270a1042a748ab1954305ffff90b2a2d8604baf0fd0ac7cac3cc4ce"
        "2d9934b2cac163bac867677d3ade902949b34e89058f9c94d1980203f4474c36"
        "df5da7e5e16385f6bd38759361bff1cc8b1fabbf4db8c749eb6ce252e8b75874"
        "cdf3e586faa3797314dfcc4f6e45a349293e5b029e812e4283447f74b8f5c3d6"
        "84fea68fd048da9ee41ef3f16207023b5af437b4f91fecfa6a4137f8cba38bfa"
        "5487d0f6982655ff387205f16fb4015d8fb171c9ab82f40e406fbd60b8384425"
        "f10290a4784c0d9bdc1eab70d56740391c88fe25a66d14437b3f04dd8ca51f48"
        "f91ee317adf403717454a1003100",

        "4de65bcbd0163a4d2368b278bf0620479c093cde119bc2872c1c8202e93aa15a"
        "8dc467eebcd324286ab204490d1c42cd710e9e0e95da6fdd13c74b72fe0cbf70",

        "01da02f42995b31b28fce5d8e478391d8bcfc226c687b0b955a601e806a0ed39"
        "02d89bb5e60d0bb7e8f66cfd942e21b61b97aa944b29e0d3c096d098550307ba"
        "427f7ac884ef096b6d0ba10556dfcf89007185b5a453882017559f9bef353b50"
        "e292f04f46b570310eb584ae137cb337c31a5c2180bde2d89a23cdb46150e84f",

        true },

    // Test 2: Zeppelin Air. Older Apple Computer Inc. certificate chain.
    {
        "308203ad06092a864886f70d010702a082039e3082039a0201013100300b0609"
        "2a864886f70d010701a08203803082037c30820264a003020102020f3434aa09"
        "0126aa06aa0008aa004543300d06092a864886f70d0101050500308192310b30"
        "09060355040613025553311d301b060355040a13144170706c6520436f6d7075"
        "7465722c20496e632e312d302b060355040b13244170706c6520436f6d707574"
        "657220436572746966696361746520417574686f726974793135303306035504"
        "03132c4170706c652069506f64204163636573736f7269657320436572746966"
        "696361746520417574686f72697479301e170d3039303132363135333932355a"
        "170d3137303132363135333932355a308183310b300906035504061302555331"
        "1d301b060355040a13144170706c6520436f6d70757465722c20496e632e3128"
        "3026060355040b131f4170706c6520436f6d70757465722069506f6420416363"
        "6573736f72696573312b3029060355040314224950415f333433344141303930"
        "31323641413036414130303038414130303435343330819f300d06092a864886"
        "f70d010101050003818d0030818902818100d74f2bd46b70553ec7d67f769376"
        "b61136d9fd66ccf9be87e706d3f232e970e4ce62b733b683d38a112963054a0f"
        "2a51a372179b9e27c3ad79ab3a27a835b0873699a5daa48760fd9346cef654b1"
        "69b3fb641875477bca1edef042b9705275ff1fee4f7ca50c552eb330828b4255"
        "9b0da9b445bb7784bc18deaf7edb64c9a8870203010001a360305e300e060355"
        "1d0f0101ff0404030203b8300c0603551d130101ff04023000301d0603551d0e"
        "0416041443e4f0e1387b7aaae852a561cd1b4de30534f5f5301f0603551d2304"
        "1830168014c9aa846b06b876e2964fe72702d72e3bdaf7b018300d06092a8648"
        "86f70d010105050003820101009544f46b77798189119f98b28bff7fb2490819"
        "69a12f0689491ae69711a6048be512d57fa3ed3d0f4cc03c86023332ef05e905"
        "be05d02b83a856cee05fde2b948439306e4c5d4ab012a5c585728aad4d6a02a7"
        "3d85a9a5861a18cc17aef7dc0cf799f89e2c477aba4d3f17c02ebefd662179fc"
        "b3127336bdc6be3612ac22796db9d37c0bbfd185544d7e7b516f9b25b64ce3be"
        "523cc30eda14978290984592e670a1f212b08f7e0ea708e025743f2244bf7116"
        "eee4808dcb5e81afa19fd6568cf79d9c9a4838c7bba5bf0a3bf9b8e7eb3af393"
        "0b42901d419922929adda7974882b9fbcf17dc5d3c69650677fabe3863ce2308"
        "b121c7bb0f6adfdea488690de5a1003100",

        "9c2f082a205691f87dc5ca79f458da50980fd288192c57b53773ec0734bdba55"
        "463d78da6f88b9b8ce0238df717811a4a66e1b6a96f1d6980670cdcc8dcf016c",

        "2cd37262d23f0fbf188c204c50c82fd8e1e4ee04a0c015aec5ff022cfb104cb6"
        "87cf94f7656035f7b29b0b37dcfd5281b16c5b3286d0e7f242292f115db00040"
        "e7bc56b4d2666bffb089753291fb0562b7356126f4ffd4e214dcbfbf950d2b1f"
        "cd749bacf767cb4e9e21304130d772f330b46194ae38f22995c0589f9154db44",

        true },

    // Test 3: Bad signature.
    {
        "308203ad06092a864886f70d010702a082039e3082039a0201013100300b0609"
        "2a864886f70d010701a08203803082037c30820264a003020102020f3434aa09"
        "0126aa06aa0008aa004543300d06092a864886f70d0101050500308192310b30"
        "09060355040613025553311d301b060355040a13144170706c6520436f6d7075"
        "7465722c20496e632e312d302b060355040b13244170706c6520436f6d707574"
        "657220436572746966696361746520417574686f726974793135303306035504"
        "03132c4170706c652069506f64204163636573736f7269657320436572746966"
        "696361746520417574686f72697479301e170d3039303132363135333932355a"
        "170d3137303132363135333932355a308183310b300906035504061302555331"
        "1d301b060355040a13144170706c6520436f6d70757465722c20496e632e3128"
        "3026060355040b131f4170706c6520436f6d70757465722069506f6420416363"
        "6573736f72696573312b3029060355040314224950415f333433344141303930"
        "31323641413036414130303038414130303435343330819f300d06092a864886"
        "f70d010101050003818d0030818902818100d74f2bd46b70553ec7d67f769376"
        "b61136d9fd66ccf9be87e706d3f232e970e4ce62b733b683d38a112963054a0f"
        "2a51a372179b9e27c3ad79ab3a27a835b0873699a5daa48760fd9346cef654b1"
        "69b3fb641875477bca1edef042b9705275ff1fee4f7ca50c552eb330828b4255"
        "9b0da9b445bb7784bc18deaf7edb64c9a8870203010001a360305e300e060355"
        "1d0f0101ff0404030203b8300c0603551d130101ff04023000301d0603551d0e"
        "0416041443e4f0e1387b7aaae852a561cd1b4de30534f5f5301f0603551d2304"
        "1830168014c9aa846b06b876e2964fe72702d72e3bdaf7b018300d06092a8648"
        "86f70d010105050003820101009544f46b77798189119f98b28bff7fb2490819"
        "69a12f0689491ae69711a6048be512d57fa3ed3d0f4cc03c86023332ef05e905"
        "be05d02b83a856cee05fde2b948439306e4c5d4ab012a5c585728aad4d6a02a7"
        "3d85a9a5861a18cc17aef7dc0cf799f89e2c477aba4d3f17c02ebefd662179fc"
        "b3127336bdc6be3612ac22796db9d37c0bbfd185544d7e7b516f9b25b64ce3be"
        "523cc30eda14978290984592e670a1f212b08f7e0ea708e025743f2244bf7116"
        "eee4808dcb5e81afa19fd6568cf79d9c9a4838c7bba5bf0a3bf9b8e7eb3af393"
        "0b42901d419922929adda7974882b9fbcf17dc5d3c69650677fabe3863ce2308"
        "b121c7bb0f6adfdea488690de5a1003100",

        "9c2f082a205691f87dc5ca79f458da50980fd288192c57b53773ec0734bdba55"
        "463d78da6f88b9b8ce0238df717811a4a66e1b6a96f1d6980670cdcc8dcf016c",

        "2cd37262d23f0fbf188c204c50c82fd8e1e4ee04a0c015abc5ff022cfb104cb6"
        "87cf94f7656035f7b29b0b37dcfd5281b16c5b3286d0e7f242292f115db00040"
        "e7bc56b4d2666bffb089753291fb0562b7356126f4ffd4e214dcbfbf950d2b1f"
        "cd749bacf767cb4e9e21304130d772f330b46194ae38f22995c0589f9154db44",

        false },

    // Test 4: Bad challenge
    {
        "3082038a06092a864886f70d010702a082037b308203770201013100300b0609"
        "2a864886f70d010701a082035d3082035930820241a003020102020f0101aa11"
        "0407aa00aa0024aa007052300d06092a864886f70d0101050500308183310b30"
        "0906035504061302555331133011060355040a130a4170706c6520496e632e31"
        "263024060355040b131d4170706c652043657274696669636174696f6e204175"
        "74686f72697479313730350603550403132e4170706c652069506f6420416363"
        "6573736f726965732043657274696669636174696f6e20417574686f72697479"
        "301e170d3131303430373031313331385a170d3139303430373031313331385a"
        "3070310b300906035504061302555331133011060355040a130a4170706c6520"
        "496e632e311f301d060355040b13164170706c652069506f6420416363657373"
        "6f72696573312b3029060355040314224950415f303130314141313130343037"
        "41413030414130303234414130303730353230819f300d06092a864886f70d01"
        "0101050003818d0030818902818100ad38e83497c98e77b855351de17565fa49"
        "989590a90c339de0e843f1a36625607bf9194047fd5df8c1b8cb5b6a5b55b149"
        "bd218baa5dcb24cd1e936b6736593d2046723080bf7407c729236d058ff60ae5"
        "4cf257f33ba49b3f039eb4d5f7a84ba6b6bffe9198a0d47600720e087dce78f3"
        "3baa05edda87200a315fbaaf317db50203010001a360305e300e0603551d0f01"
        "01ff0404030203b8300c0603551d130101ff04023000301d0603551d0e041604"
        "14aca964d23f65c089503a6c635a6fe63ed19ec193301f0603551d2304183016"
        "8014ff4b1a439af51996ab18002b61c9ee409d8ec704300d06092a864886f70d"
        "010105050003820101001188bf69635c9ba31b8528c26f4130e04320f23527f8"
        "a9f27f815270a1042a748ab1954305ffff90b2a2d8604baf0fd0ac7cac3cc4ce"
        "2d9934b2cac163bac867677d3ade902949b34e89058f9c94d1980203f4474c36"
        "df5da7e5e16385f6bd38759361bff1cc8b1fabbf4db8c749eb6ce252e8b75874"
        "cdf3e586faa3797314dfcc4f6e45a349293e5b029e812e4283447f74b8f5c3d6"
        "84fea68fd048da9ee41ef3f16207023b5af437b4f91fecfa6a4137f8cba38bfa"
        "5487d0f6982655ff387205f16fb4015d8fb171c9ab82f40e406fbd60b8384425"
        "f10290a4784c0d9bdc1eab70d56740391c88fe25a66d14437b3f04dd8ca51f48"
        "f91ee317adf403717454a1003100",

        "4de65bcbd0163a4d2368b278bf0620479c093cde119bc2872c1c8202e93aa15a"
        "8dc467eebcd324286ab214490d1c42cd710e9e0e95da6fdd13c74b72fe0cbf70",

        "01da02f42995b31b28fce5d8e478391d8bcfc226c687b0b955a601e806a0ed39"
        "02d89bb5e60d0bb7e8f66cfd942e21b61b97aa944b29e0d3c096d098550307ba"
        "427f7ac884ef096b6d0ba10556dfcf89007185b5a453882017559f9bef353b50"
        "e292f04f46b570310eb584ae137cb337c31a5c2180bde2d89a23cdb46150e84f",

        false },

    // Test 5: Bad certificate
    {
        "3082038a06092a864886f70d010702a082037b308203770201013100300b0609"
        "2a864886f70d010701a082035d3082035930820241a003020102020f0101aa11"
        "0407aa00aa0024aa007052300d06092a864886f70d0101050500308183310b30"
        "0906035504061302555331133011060355040a130a4170706c6520496e632e31"
        "263024060355040b131d4170706c652043657274696669636174696f6e204175"
        "74686f72697479313730350603550403132e4170706c652069506f6420416363"
        "6573736f726965732043657274696669636174696f6e20417574686f72697479"
        "301e170d3131303430373031313331385a170d3139303430373031313331385a"
        "3070310b300906035504061302555331133011060355040a130a4170706c6520"
        "496e632e311f301d060355040b13164170706c652069506f6420416363657373"
        "6f72696573312b3029060355040314224950415f303130314141313130343037"
        "41413030414130303234514130303730353230819f300d06092a864886f70d01"
        "0101050003818d0030818902818100ad38e83497c98e77b855351de17565fa49"
        "989590a90c339de0e843f1a36625607bf9194047fd5df8c1b8cb5b6a5b55b149"
        "bd218baa5dcb24cd1e936b6736593d2046723080bf7407c729236d058ff60ae5"
        "4cf257f33ba49b3f039eb4d5f7a84ba6b6bffe9198a0d47600720e087dce78f3"
        "3baa05edda87200a315fbaaf317db50203010001a360305e300e0603551d0f01"
        "01ff0404030203b8300c0603551d130101ff04023000301d0603551d0e041604"
        "14aca964d23f65c089503a6c635a6fe63ed19ec193301f0603551d2304183016"
        "8014ff4b1a439af51996ab18002b61c9ee409d8ec704300d06092a864886f70d"
        "010105050003820101001188bf69635c9ba31b8528c26f4130e04320f23527f8"
        "a9f27f815270a1042a748ab1954305ffff90b2a2d8604baf0fd0ac7cac3cc4ce"
        "2d9934b2cac163bac867677d3ade902949b34e89058f9c94d1980203f4474c36"
        "df5da7e5e16385f6bd38759361bff1cc8b1fabbf4db8c749eb6ce252e8b75874"
        "cdf3e586faa3797314dfcc4f6e45a349293e5b029e812e4283447f74b8f5c3d6"
        "84fea68fd048da9ee41ef3f16207023b5af437b4f91fecfa6a4137f8cba38bfa"
        "5487d0f6982655ff387205f16fb4015d8fb171c9ab82f40e406fbd60b8384425"
        "f10290a4784c0d9bdc1eab70d56740391c88fe25a66d14437b3f04dd8ca51f48"
        "f91ee317adf403717454a1003100",

        "4de65bcbd0163a4d2368b278bf0620479c093cde119bc2872c1c8202e93aa15a"
        "8dc467eebcd324286ab204490d1c42cd710e9e0e95da6fdd13c74b72fe0cbf70",

        "01da02f42995b31b28fce5d8e478391d8bcfc226c687b0b955a601e806a0ed39"
        "02d89bb5e60d0bb7e8f66cfd942e21b61b97aa944b29e0d3c096d098550307ba"
        "427f7ac884ef096b6d0ba10556dfcf89007185b5a453882017559f9bef353b50"
        "e292f04f46b570310eb584ae137cb337c31a5c2180bde2d89a23cdb46150e84f",

        false },

    // Test 6: Expired certificate (but otherwise valid)
    {
        "308203ad06092a864886f70d010702a082039e3082039a0201013100300b0609"
        "2a864886f70d010701a08203803082037c30820264a003020102020f3333aa06"
        "0531aa00aa0001aa000001300d06092a864886f70d0101050500308192310b30"
        "09060355040613025553311d301b060355040a13144170706c6520436f6d7075"
        "7465722c20496e632e312d302b060355040b13244170706c6520436f6d707574"
        "657220436572746966696361746520417574686f726974793135303306035504"
        "03132c4170706c652069506f64204163636573736f7269657320436572746966"
        "696361746520417574686f72697479301e170d3036303533313230343632395a"
        "170d3134303533313230343632395a308183310b300906035504061302555331"
        "1d301b060355040a13144170706c6520436f6d70757465722c20496e632e3128"
        "3026060355040b131f4170706c6520436f6d70757465722069506f6420416363"
        "6573736f72696573312b3029060355040314224950415f333333334141303630"
        "35333141413030414130303031414130303030303130819f300d06092a864886"
        "f70d010101050003818d0030818902818100d29e426dd02a4bcec85a8ab53c6f"
        "b014dba2952d831e1314d48bdafe14c227081186b46b5b555c54389a1eed3215"
        "d123517bd0f7c687490f83279726ff2654f572080c5f042c8c15f1d8d0021ddc"
        "102f7c9584e14be39f331c46950403da26d3c05e0245469ec03ccdfbb5abaedf"
        "ae45c6c7d8e32735bf7235addeb9f0e4f20f0203010001a360305e300e060355"
        "1d0f0101ff0404030203b8300c0603551d130101ff04023000301d0603551d0e"
        "04160414e6f0ed49e024715bfda0a7aff23f0fdc603a3cab301f0603551d2304"
        "1830168014c9aa846b06b876e2964fe72702d72e3bdaf7b018300d06092a8648"
        "86f70d0101050500038201010002a816b650b38cafb00445240d63ce62b65648"
        "8ea75d3de7ee53a5defc55aa940b926a555b9317513f08a7779b7b8a034cd887"
        "e47737406b56861ab43aafa9675d23f0fa4b8c9c71a07cb0c38ff06fb3b485e1"
        "ef2ddc4a4ec2ae85550e392e1a2a9df4dc8085885444bac6cff519b7b9d2de6e"
        "645f8395bda195cfb1432dd84b7997e46fefa4fe93f1ed529ddfa9f3be2d86a7"
        "d9aec12220e821eb73d85a17340f82320db28ec34754faa5fb7d2b1981a8aa65"
        "3383156310868b7a32aeb47953576fe8bd4bc3b337cec1292d890eb5b2033fb4"
        "68d16910ed9f45272def8065959c77fae63e03563c2ca492f3f91f32a19ca1e6"
        "1c2994eaad1e1b36f7e608e91aa1003100",

        "97b961565255ea6b95229aa6cd8706dfd141b7e5e6584d56008f1e4fd99308b0",

        "6e7f374ac85ea3cddec3d1815b291dd8a1bf24cc1d537a515dda3c1053c28a90"
        "06dcd214b5167d2a429347e5f0a0f1eae414c3b4c2c6217768584e7692a9c2a0"
        "21171a530db8bfc623f90d81d3a31cfa59b9f9235f47cc0da637b51478b51c86"
        "495ad260e69be7a6d2fbf03f1d3ce357845042d177760ea61ca0c2296ea3fe4e",

        false },
};

OSStatus MFiClientPlatform_Test(void);
OSStatus MFiClientPlatform_Test(void)
{
    OSStatus err;
    const MFiClientPlatformTestVector* tv;
    size_t i, n;
    uint8_t* challengePtr = NULL;
    size_t challengeLen;
    uint8_t* certPtr = NULL;
    size_t certLen;
    uint8_t* signaturePtr = NULL;
    size_t signatureLen;

    // AuthV2 serial number.

    err = _VerifySerialNumber((const uint8_t*)"\x33\x33\xAA\x12\x12\x19\xAA\x07\xAA\x00\x06\xAA\x00\x00\x01", 15);
    require_noerr(err, exit);

    // AuthV3 serial number.

    err = _VerifySerialNumber((const uint8_t*)"\x26\x24\x81\x02\x0D\x40\x84\x00\xF7\x65\xF9\x73\x00\x01\x3F\xF9", 16);
    require_noerr(err, exit);

    // Signatures

    n = countof(kMFiClientPlatformTestVectors);
    for (i = 0; i < n; ++i) {
        tv = &kMFiClientPlatformTestVectors[i];
        err = HexToDataCopy(tv->challenge, kSizeCString, kHexToData_DefaultFlags, &challengePtr, &challengeLen, NULL);
        require_noerr(err, exit);
        err = HexToDataCopy(tv->cert, kSizeCString, kHexToData_DefaultFlags, &certPtr, &certLen, NULL);
        require_noerr(err, exit);
        err = HexToDataCopy(tv->signature, kSizeCString, kHexToData_DefaultFlags, &signaturePtr, &signatureLen, NULL);
        require_noerr(err, exit);

        err = MFiPlatform_VerifySignature(challengePtr, challengeLen, signaturePtr, signatureLen, certPtr, certLen);
        if (tv->shouldPass)
            require_noerr(err, exit);
        else
            require_action(err != kNoErr, exit, err = -1);

        ForgetMem(&challengePtr);
        ForgetMem(&certPtr);
        ForgetMem(&signaturePtr);
    }
    err = kNoErr;

exit:
    FreeNullSafe(challengePtr);
    FreeNullSafe(certPtr);
    FreeNullSafe(signaturePtr);
    printf("MFiClientPlatform_Test: %s\n", !err ? "PASSED" : "FAILED");
    return (err);
}
#endif
