/*
	Copyright (C) 2013-2019 Apple Inc. All Rights Reserved.
	
	Darwin software-based platform plugin for MFi-SAP authentication/encryption.
	This is only for testing since it requires embedding a private key, which isn't secure.
*/

#include "MFiSAP.h"

#include "DebugServices.h"
#include "HTTPClient.h"
#include "MiscUtils.h"
#include "StringUtils.h"

#if (COMMON_SERVICES_NO_CORE_SERVICES)
#include <CommonCrypto/CommonRSACryptor.h>
#elif (TARGET_OS_IPHONE)
#include <Security/SecRSAKey.h>
#elif (TARGET_OS_MACOSX)
#include <Security/Security.h>
#endif

//===========================================================================================================================
//	Public Certificate (PKCS#7 message containing the certificate)
//===========================================================================================================================

// Public cert generated specifically for this.

static const uint8_t kPublicCertificate[] = {
    0x30, 0x82, 0x03, 0xad, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x02, 0xa0,
    0x82, 0x03, 0x9e, 0x30, 0x82, 0x03, 0x9a, 0x02, 0x01, 0x01, 0x31, 0x00, 0x30, 0x0b, 0x06, 0x09,
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01, 0xa0, 0x82, 0x03, 0x80, 0x30, 0x82, 0x03,
    0x7c, 0x30, 0x82, 0x02, 0x64, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x0f, 0x33, 0x33, 0xaa, 0x12,
    0x12, 0x19, 0xaa, 0x07, 0xaa, 0x00, 0x06, 0xaa, 0x00, 0x00, 0x01, 0x30, 0x0d, 0x06, 0x09, 0x2a,
    0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00, 0x30, 0x81, 0x92, 0x31, 0x0b, 0x30,
    0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x1d, 0x30, 0x1b, 0x06, 0x03,
    0x55, 0x04, 0x0a, 0x13, 0x14, 0x41, 0x70, 0x70, 0x6c, 0x65, 0x20, 0x43, 0x6f, 0x6d, 0x70, 0x75,
    0x74, 0x65, 0x72, 0x2c, 0x20, 0x49, 0x6e, 0x63, 0x2e, 0x31, 0x2d, 0x30, 0x2b, 0x06, 0x03, 0x55,
    0x04, 0x0b, 0x13, 0x24, 0x41, 0x70, 0x70, 0x6c, 0x65, 0x20, 0x43, 0x6f, 0x6d, 0x70, 0x75, 0x74,
    0x65, 0x72, 0x20, 0x43, 0x65, 0x72, 0x74, 0x69, 0x66, 0x69, 0x63, 0x61, 0x74, 0x65, 0x20, 0x41,
    0x75, 0x74, 0x68, 0x6f, 0x72, 0x69, 0x74, 0x79, 0x31, 0x35, 0x30, 0x33, 0x06, 0x03, 0x55, 0x04,
    0x03, 0x13, 0x2c, 0x41, 0x70, 0x70, 0x6c, 0x65, 0x20, 0x69, 0x50, 0x6f, 0x64, 0x20, 0x41, 0x63,
    0x63, 0x65, 0x73, 0x73, 0x6f, 0x72, 0x69, 0x65, 0x73, 0x20, 0x43, 0x65, 0x72, 0x74, 0x69, 0x66,
    0x69, 0x63, 0x61, 0x74, 0x65, 0x20, 0x41, 0x75, 0x74, 0x68, 0x6f, 0x72, 0x69, 0x74, 0x79, 0x30,
    0x1e, 0x17, 0x0d, 0x31, 0x32, 0x31, 0x32, 0x31, 0x39, 0x32, 0x30, 0x31, 0x34, 0x34, 0x38, 0x5a,
    0x17, 0x0d, 0x32, 0x30, 0x31, 0x32, 0x31, 0x39, 0x32, 0x30, 0x31, 0x34, 0x34, 0x38, 0x5a, 0x30,
    0x81, 0x83, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31,
    0x1d, 0x30, 0x1b, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x13, 0x14, 0x41, 0x70, 0x70, 0x6c, 0x65, 0x20,
    0x43, 0x6f, 0x6d, 0x70, 0x75, 0x74, 0x65, 0x72, 0x2c, 0x20, 0x49, 0x6e, 0x63, 0x2e, 0x31, 0x28,
    0x30, 0x26, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13, 0x1f, 0x41, 0x70, 0x70, 0x6c, 0x65, 0x20, 0x43,
    0x6f, 0x6d, 0x70, 0x75, 0x74, 0x65, 0x72, 0x20, 0x69, 0x50, 0x6f, 0x64, 0x20, 0x41, 0x63, 0x63,
    0x65, 0x73, 0x73, 0x6f, 0x72, 0x69, 0x65, 0x73, 0x31, 0x2b, 0x30, 0x29, 0x06, 0x03, 0x55, 0x04,
    0x03, 0x14, 0x22, 0x49, 0x50, 0x41, 0x5f, 0x33, 0x33, 0x33, 0x33, 0x41, 0x41, 0x31, 0x32, 0x31,
    0x32, 0x31, 0x39, 0x41, 0x41, 0x30, 0x37, 0x41, 0x41, 0x30, 0x30, 0x30, 0x36, 0x41, 0x41, 0x30,
    0x30, 0x30, 0x30, 0x30, 0x31, 0x30, 0x81, 0x9f, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
    0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x81, 0x8d, 0x00, 0x30, 0x81, 0x89, 0x02, 0x81,
    0x81, 0x00, 0xdf, 0x32, 0x0f, 0x50, 0xef, 0xa1, 0x5a, 0xd1, 0x3c, 0xd5, 0x21, 0xda, 0x01, 0x22,
    0x6a, 0xd4, 0xf4, 0x3f, 0x99, 0x85, 0x97, 0x0e, 0x7e, 0x3a, 0xab, 0x91, 0x8e, 0x8e, 0xf3, 0xda,
    0xbf, 0x78, 0xe6, 0xf9, 0xcc, 0x12, 0x26, 0x53, 0xaf, 0x77, 0x9f, 0x32, 0x84, 0xd2, 0x28, 0xb2,
    0x58, 0xca, 0xe2, 0xfb, 0x8e, 0x35, 0x19, 0xe7, 0x15, 0x0d, 0xfe, 0x98, 0xa5, 0x4d, 0x8b, 0xe1,
    0x69, 0x48, 0xbc, 0x8e, 0x85, 0x16, 0x1a, 0x99, 0x89, 0x00, 0xf6, 0x8e, 0xe5, 0x18, 0x28, 0xfd,
    0xe4, 0x9d, 0x26, 0xb3, 0xaf, 0x30, 0xa4, 0x94, 0x9c, 0x49, 0x0c, 0x4f, 0xa4, 0x7c, 0xb6, 0x0e,
    0xbc, 0xa1, 0x66, 0x4f, 0x26, 0x52, 0xb9, 0xd6, 0xb2, 0x22, 0x69, 0x19, 0xa4, 0x53, 0x7d, 0x88,
    0x8c, 0x21, 0x12, 0x58, 0xb1, 0xe1, 0xc2, 0x89, 0x59, 0xd9, 0x36, 0x7e, 0x8b, 0x73, 0x7b, 0x0b,
    0x81, 0xdb, 0x02, 0x03, 0x01, 0x00, 0x01, 0xa3, 0x60, 0x30, 0x5e, 0x30, 0x0e, 0x06, 0x03, 0x55,
    0x1d, 0x0f, 0x01, 0x01, 0xff, 0x04, 0x04, 0x03, 0x02, 0x03, 0xb8, 0x30, 0x0c, 0x06, 0x03, 0x55,
    0x1d, 0x13, 0x01, 0x01, 0xff, 0x04, 0x02, 0x30, 0x00, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x1d, 0x0e,
    0x04, 0x16, 0x04, 0x14, 0xdd, 0x7c, 0x4f, 0xe5, 0xda, 0xe3, 0xe5, 0x95, 0xea, 0x88, 0x51, 0xb9,
    0xf3, 0xcf, 0x1a, 0x56, 0x2c, 0x42, 0x40, 0xab, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x1d, 0x23, 0x04,
    0x18, 0x30, 0x16, 0x80, 0x14, 0xc9, 0xaa, 0x84, 0x6b, 0x06, 0xb8, 0x76, 0xe2, 0x96, 0x4f, 0xe7,
    0x27, 0x02, 0xd7, 0x2e, 0x3b, 0xda, 0xf7, 0xb0, 0x18, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48,
    0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00, 0x03, 0x82, 0x01, 0x01, 0x00, 0x11, 0xcd, 0x90,
    0x1a, 0xcb, 0x45, 0xa2, 0x64, 0xfc, 0x16, 0x87, 0xc3, 0xb2, 0x1c, 0x00, 0xeb, 0x06, 0xae, 0x94,
    0xe4, 0x06, 0x40, 0xf9, 0x4b, 0x49, 0x8a, 0xa0, 0x15, 0xd4, 0xb0, 0xca, 0xff, 0x8f, 0xc9, 0xdd,
    0x52, 0xd5, 0x8e, 0xb7, 0xc4, 0x45, 0x5c, 0xad, 0x54, 0x86, 0x0c, 0x6a, 0x22, 0x08, 0xb2, 0x36,
    0xd3, 0xc6, 0x02, 0xf5, 0xdf, 0x0f, 0xd4, 0xac, 0x39, 0x98, 0x28, 0x18, 0xbc, 0xc7, 0xd7, 0x92,
    0x91, 0x70, 0x2a, 0xf2, 0x86, 0xa1, 0x52, 0x25, 0x6d, 0x6b, 0xab, 0xc3, 0xf3, 0x94, 0x7e, 0x6a,
    0x98, 0x14, 0x35, 0x0c, 0xf9, 0x7c, 0xc0, 0xf2, 0x03, 0x55, 0x74, 0xdf, 0xf1, 0x58, 0x3a, 0x74,
    0x41, 0x24, 0x9e, 0x44, 0xb8, 0xbf, 0x19, 0x7f, 0x4f, 0x45, 0xac, 0x56, 0x42, 0xb0, 0x42, 0xbe,
    0x00, 0xa4, 0x7e, 0x35, 0xff, 0x3c, 0x23, 0x77, 0xec, 0x36, 0xa4, 0xab, 0x7e, 0x54, 0x3f, 0x3f,
    0xee, 0xd0, 0x58, 0x5b, 0x35, 0xcb, 0x36, 0x5c, 0x80, 0x7b, 0xc3, 0xcb, 0x65, 0x71, 0x52, 0x3c,
    0x90, 0x72, 0xc6, 0x72, 0x58, 0xd3, 0x57, 0x6c, 0x33, 0xff, 0x8d, 0xda, 0x4d, 0xb0, 0x4c, 0x88,
    0x9d, 0x91, 0x41, 0x8b, 0xc6, 0xa7, 0x86, 0x63, 0x6b, 0xc2, 0x44, 0xa4, 0x7d, 0x08, 0x8f, 0xc0,
    0xa4, 0x29, 0xd6, 0xdf, 0xe6, 0x9f, 0x3d, 0xec, 0xe9, 0x63, 0x77, 0xc0, 0x64, 0xed, 0x93, 0x07,
    0x92, 0x0a, 0x7e, 0x9c, 0xc3, 0x8b, 0x21, 0x20, 0xf0, 0xf2, 0xbc, 0xa6, 0x68, 0x3d, 0xe7, 0x8b,
    0x0d, 0x10, 0xc2, 0x75, 0x3a, 0x18, 0xe8, 0x04, 0x92, 0xdb, 0xb9, 0x45, 0x85, 0x2a, 0x9a, 0xb7,
    0x9d, 0xf7, 0x53, 0xaf, 0xea, 0xc3, 0x44, 0xea, 0x6a, 0xef, 0xb5, 0xda, 0x4d, 0x2a, 0x11, 0x0d,
    0x84, 0xcb, 0x86, 0x0c, 0xd4, 0xaa, 0xc9, 0x47, 0xab, 0x04, 0xfd, 0x70, 0xac, 0xa1, 0x00, 0x31,
    0x00
};

#if (!defined(MFI_HARD_CODED_PRIVATE_KEY))
#define MFI_HARD_CODED_PRIVATE_KEY 0
#endif
#if (MFI_HARD_CODED_PRIVATE_KEY)
//===========================================================================================================================
//	Private Key
//===========================================================================================================================

// You can replace this with a real key for private testing, but don't check it in or give out binaries with it.

static const uint8_t kPrivateKey[] = {
    0x00
};
#endif

//===========================================================================================================================
//	Internals
//===========================================================================================================================

static uint8_t* _CopyKey(size_t* outLen, OSStatus* outErr);

#if (COMMON_SERVICES_NO_CORE_SERVICES)
static OSStatus
_DoSign_CommonCrypto(
    const uint8_t* inSKPtr,
    size_t inSKLen,
    const void* inSHA1,
    uint8_t** outSigPtr,
    size_t* outSigLen);
#elif (TARGET_OS_IPHONE)
static OSStatus
_DoSign_SecKeyRawSign(
    const uint8_t* inSKPtr,
    size_t inSKLen,
    const void* inSHA1,
    uint8_t** outSigPtr,
    size_t* outSigLen);
#elif (TARGET_OS_MACOSX)
static OSStatus
_DoSign_SecTransform(
    const uint8_t* inSKPtr,
    size_t inSKLen,
    const void* inSHA1,
    uint8_t** outSigPtr,
    size_t* outSigLen);
#endif

#if (!COMMON_SERVICES_NO_CORE_SERVICES)
static OSStatus _MFiProxyCopySignature(uint8_t** outCertificatePtr, size_t* outCertificateLen);
static OSStatus
_MFiProxyCreateSignature(
    const void* inDigestPtr,
    size_t inDigestLen,
    uint8_t** outSignaturePtr,
    size_t* outSignatureLen);
#endif

//===========================================================================================================================
//	MFiPlatform_Initialize
//===========================================================================================================================

OSStatus MFiPlatform_Initialize(void)
{
    return (kNoErr);
}

//===========================================================================================================================
//	MFiPlatform_Finalize
//===========================================================================================================================

void MFiPlatform_Finalize(void)
{
}

//===========================================================================================================================
//	MFiPlatform_CreateSignature
//===========================================================================================================================

OSStatus
MFiPlatform_CreateSignature(
    const void* inDigestPtr,
    size_t inDigestLen,
    uint8_t** outSignaturePtr,
    size_t* outSignatureLen)
{
    return (MFiPlatform_CreateSignatureEx(kMFiAuthFlagsAlreadyHashed, inDigestPtr, inDigestLen,
        outSignaturePtr, outSignatureLen));
}

//===========================================================================================================================

OSStatus
MFiPlatform_CreateSignatureEx(
    MFiAuthFlags inFlags,
    const void* inDataPtr,
    size_t inDataLen,
    uint8_t** outSignaturePtr,
    size_t* outSignatureLen)
{
    OSStatus err;
    uint8_t digestBuf[32];
    const void* digestPtr;
    size_t digestLen;
    uint8_t* skPtr = NULL;
    size_t skLen;

    if (inFlags & kMFiAuthFlagsAlreadyHashed) {
        digestPtr = inDataPtr;
        digestLen = inDataLen;
    } else {
#if (MFI_SAP_SERVER_AUTHV3)
        SHA256(inDataPtr, inDataLen, digestBuf);
        digestLen = SHA256_DIGEST_LENGTH;
#else
        SHA1(inDataPtr, inDataLen, digestBuf);
        digestLen = SHA_DIGEST_LENGTH;
#endif
        digestPtr = digestBuf;
    }

    skPtr = _CopyKey(&skLen, &err);
#if (!COMMON_SERVICES_NO_CORE_SERVICES)
    if (!skPtr) {
        err = _MFiProxyCreateSignature(digestPtr, digestLen, outSignaturePtr, outSignatureLen);
        require_noerr(err, exit);
        goto exit;
    }
#else
    require_action_quiet(skPtr, exit, err = kUnsupportedErr);
#endif

#if (COMMON_SERVICES_NO_CORE_SERVICES)
    err = _DoSign_CommonCrypto(skPtr, skLen, digestPtr, outSignaturePtr, outSignatureLen);
    require_noerr(err, exit);
#elif (TARGET_OS_IPHONE)
    err = _DoSign_SecKeyRawSign(skPtr, skLen, digestPtr, outSignaturePtr, outSignatureLen);
    require_noerr(err, exit);
#elif (TARGET_OS_MACOSX)
    err = _DoSign_SecTransform(skPtr, skLen, digestPtr, outSignaturePtr, outSignatureLen);
    require_noerr(err, exit);
#else
#error "Unsupported platform"
#endif

exit:
    FreeNullSafe(skPtr);
    return (err);
}

//===========================================================================================================================
//	MFiPlatform_CopyCertificate
//===========================================================================================================================

OSStatus MFiPlatform_CopyCertificate(uint8_t** outCertificatePtr, size_t* outCertificateLen)
{
    OSStatus err;
    uint8_t* skPtr;
    size_t skLen;
    uint8_t* ptr;

    skPtr = _CopyKey(&skLen, &err);
#if (!COMMON_SERVICES_NO_CORE_SERVICES)
    if (!skPtr) {
        err = _MFiProxyCopySignature(outCertificatePtr, outCertificateLen);
        require_noerr(err, exit);
        goto exit;
    }
#else
    require_action_quiet(skPtr, exit, err = kUnsupportedErr);
#endif

    if (outCertificatePtr) {
        ptr = (uint8_t*)malloc(sizeof(kPublicCertificate));
        require_action(ptr, exit, err = kNoMemoryErr);
        memcpy(ptr, kPublicCertificate, sizeof(kPublicCertificate));
        *outCertificatePtr = ptr;
    }
    if (outCertificateLen)
        *outCertificateLen = sizeof(kPublicCertificate);
    err = kNoErr;

exit:
    FreeNullSafe(skPtr);
    return (err);
}

//===========================================================================================================================
//	_CopyKey
//===========================================================================================================================

static uint8_t* _CopyKey(size_t* outLen, OSStatus* outErr)
{
    uint8_t* key = NULL;
    OSStatus err;

#if (MFI_HARD_CODED_PRIVATE_KEY)
    // If there's a hard-coded private key, return that.

    key = (uint8_t*)malloc(sizeof(kPrivateKey));
    require_action(key, exit, err = kNoMemoryErr);
    memcpy(key, kPrivateKey, sizeof(kPrivateKey));
    *outLen = sizeof(kPrivateKey);
    err = kNoErr;
#else
    const char* path;
    char* ptr;
    uint8_t* ptr2;
    size_t len;

    // Check for a private key on disk.

    path = getenv("MFI_KEY_PATH");
    if (!path)
        path = "mfi-key.txt";
    err = CopyFileDataByPath(path, &ptr, &len);
    require_noerr_quiet(err, exit);

    err = HexToDataCopy(ptr, len, kHexToData_DefaultFlags, &ptr2, &len, NULL);
    free(ptr);
    require_noerr_quiet(err, exit);
    key = ptr2;
    *outLen = len;
#endif

exit:
    if (outErr)
        *outErr = err;
    return (key);
}

#if (COMMON_SERVICES_NO_CORE_SERVICES)
//===========================================================================================================================
//	_DoSign_CommonCrypto
//===========================================================================================================================

static OSStatus
_DoSign_CommonCrypto(
    const uint8_t* inSKPtr,
    size_t inSKLen,
    const void* inSHA1,
    uint8_t** outSigPtr,
    size_t* outSigLen)
{
    OSStatus err;
    CCRSACryptorRef key;
    uint8_t sig[256];
    size_t len;
    uint8_t* ptr;

    err = CCRSACryptorImport(inSKPtr, inSKLen, &key);
    require_noerr(err, exit);

    len = sizeof(sig);
    err = CCRSACryptorSign(key, ccPKCS1Padding, inSHA1, 20, kCCDigestSHA1, 0, sig, &len);
    CCRSACryptorRelease(key);
    require_noerr(err, exit);
    require_action(len > 0, exit, err = kInternalErr);

    ptr = (uint8_t*)malloc(len);
    require_action(ptr, exit, err = kNoMemoryErr);
    memcpy(ptr, sig, len);
    *outSigPtr = ptr;
    *outSigLen = len;

exit:
    return (err);
}

#elif (TARGET_OS_IPHONE)
//===========================================================================================================================
//	_DoSign_SecKeyRawSign
//===========================================================================================================================

static OSStatus
_DoSign_SecKeyRawSign(
    const uint8_t* inSKPtr,
    size_t inSKLen,
    const void* inSHA1,
    uint8_t** outSigPtr,
    size_t* outSigLen)
{
    OSStatus err;
    SecKeyRef key;
    size_t sigLen;
    uint8_t* sigPtr;

    key = SecKeyCreateRSAPrivateKey(NULL, inSKPtr, (CFIndex)inSKLen, kSecKeyEncodingPkcs1);
    require_action(key, exit, err = kUnknownErr);

    sigLen = inSKLen;
    sigPtr = (uint8_t*)malloc(inSKLen);
    require_action(sigPtr, exit, err = kNoMemoryErr);

    err = SecKeyRawSign(key, kSecPaddingPKCS1SHA1, (const uint8_t*)inSHA1, 20, sigPtr, &sigLen);
    if (err)
        free(sigPtr);
    require_noerr(err, exit);

    *outSigPtr = sigPtr;
    *outSigLen = sigLen;

exit:
    CFReleaseNullSafe(key);
    return (err);
}

#elif (TARGET_OS_MACOSX)
//===========================================================================================================================
//	_DoSign_SecTransform
//===========================================================================================================================

static OSStatus
_DoSign_SecTransform(
    const uint8_t* inSKPtr,
    size_t inSKLen,
    const void* inSHA1,
    uint8_t** outSigPtr,
    size_t* outSigLen)
{
    OSStatus err;
    CFDataRef keyData;
    SecExternalFormat format;
    CFArrayRef keys = NULL;
    CFArrayRef allowUsage;
    SecKeyRef key;
    SecItemImportExportKeyParameters params;
    SecTransformRef transform = NULL;
    CFDataRef tempData;
    size_t len;
    uint8_t* ptr;

    // Create a SecKey from the raw key data.

    keyData = CFDataCreate(NULL, inSKPtr, (CFIndex)inSKLen);
    require_action(keyData, exit, err = kNoMemoryErr);

    allowUsage = CFArrayCreate(NULL, (const void* []){ kSecAttrCanSign }, 1, &kCFTypeArrayCallBacks);
    require_action(allowUsage, exit, err = kNoMemoryErr);

    format = kSecFormatUnknown;
    memset(&params, 0, sizeof(params));
    params.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
    params.keyUsage = allowUsage;
    err = SecItemImport(keyData, NULL, &format, NULL, 0, &params, NULL, &keys);
    CFRelease(allowUsage);
    require_noerr(err, exit);

    require_action(CFArrayGetCount(keys) > 0, exit, err = kCountErr);
    key = (SecKeyRef)CFArrayGetValueAtIndex(keys, 0);
    require_action(CFIsType(key, SecKey), exit, err = kTypeErr);

    // Sign the data using a transform.

    transform = SecSignTransformCreate(key, NULL);
    require_action(transform, exit, err = kUnknownErr);

    tempData = CFDataCreate(NULL, (const uint8_t*)inSHA1, 20);
    require_action(tempData, exit, err = kUnknownErr);
    SecTransformSetAttribute(transform, kSecTransformInputAttributeName, tempData, NULL);
    CFRelease(tempData);

    SecTransformSetAttribute(transform, kSecInputIsAttributeName, kSecInputIsDigest, NULL);
    SecTransformSetAttribute(transform, kSecDigestTypeAttribute, kSecDigestSHA1, NULL);

    tempData = (CFDataRef)SecTransformExecute(transform, NULL);
    require_action(tempData, exit, err = kUnknownErr);

    len = (size_t)CFDataGetLength(tempData);
    ptr = (uint8_t*)malloc((len > 0) ? len : 1);
    if (!ptr)
        CFRelease(tempData);
    require_action(ptr, exit, err = kNoMemoryErr);
    if (len > 0)
        memcpy(ptr, CFDataGetBytePtr(tempData), len);
    CFRelease(tempData);

    *outSigPtr = ptr;
    *outSigLen = len;

exit:
    CFReleaseNullSafe(transform);
    CFReleaseNullSafe(keys);
    CFReleaseNullSafe(keyData);
    return (err);
}
#endif // TARGET_OS_MACOSX

#if (!COMMON_SERVICES_NO_CORE_SERVICES)
//===========================================================================================================================
//	_MFiProxyCopySignature
//===========================================================================================================================

static OSStatus _MFiProxyCopySignature(uint8_t** outCertificatePtr, size_t* outCertificateLen)
{
    OSStatus err;
    HTTPClientRef client = NULL;
    dispatch_queue_t queue;
    HTTPMessageRef msg = NULL;
    uint8_t* certPtr;

    err = HTTPClientCreate(&client);
    require_noerr(err, exit);

    err = HTTPClientSetDestination(client, "spc.apple.com", 14000);
    require_noerr(err, exit);

    queue = dispatch_queue_create("MFiProxy", NULL);
    require_action(queue, exit, err = kUnknownErr);
    HTTPClientSetDispatchQueue(client, queue);
    arc_safe_dispatch_release(queue);

    err = HTTPMessageCreate(&msg);
    require_noerr(err, exit);
    HTTPHeader_InitRequest(&msg->header, "GET", "/mfi-certificate", "HTTP/1.1");
    err = HTTPClientSendMessageSync(client, msg);
    if (IsHTTPOSStatus(err))
        err = kNoErr;
    require_noerr(err, exit);

    require_action(msg->bodyLen > 0, exit, err = kSizeErr);
    certPtr = malloc(msg->bodyLen);
    require_action(certPtr, exit, err = kNoMemoryErr);
    memcpy(certPtr, msg->bodyPtr, msg->bodyLen);
    *outCertificatePtr = certPtr;
    *outCertificateLen = msg->bodyLen;

exit:
    HTTPClientForget(&client);
    ForgetCF(&msg);
    return (err);
}

//===========================================================================================================================
//	_MFiProxyCreateSignature
//===========================================================================================================================

static OSStatus
_MFiProxyCreateSignature(
    const void* inDigestPtr,
    size_t inDigestLen,
    uint8_t** outSignaturePtr,
    size_t* outSignatureLen)
{
    OSStatus err;
    HTTPClientRef client = NULL;
    dispatch_queue_t queue;
    HTTPMessageRef msg = NULL;
    uint8_t* signaturePtr;

    err = HTTPClientCreate(&client);
    require_noerr(err, exit);

    err = HTTPClientSetDestination(client, "spc.apple.com", 14000);
    require_noerr(err, exit);

    queue = dispatch_queue_create("MFiProxy", NULL);
    require_action(queue, exit, err = kUnknownErr);
    HTTPClientSetDispatchQueue(client, queue);
    arc_safe_dispatch_release(queue);

    err = HTTPMessageCreate(&msg);
    require_noerr(err, exit);
    HTTPHeader_InitRequest(&msg->header, "POST", "/mfi-create-signature", "HTTP/1.1");
    err = HTTPMessageSetBody(msg, kMIMEType_Binary, inDigestPtr, inDigestLen);
    require_noerr(err, exit);
    err = HTTPClientSendMessageSync(client, msg);
    if (IsHTTPOSStatus(err))
        err = kNoErr;
    require_noerr(err, exit);

    require_action(msg->bodyLen > 0, exit, err = kSizeErr);
    signaturePtr = malloc(msg->bodyLen);
    require_action(signaturePtr, exit, err = kNoMemoryErr);
    memcpy(signaturePtr, msg->bodyPtr, msg->bodyLen);
    *outSignaturePtr = signaturePtr;
    *outSignatureLen = msg->bodyLen;

exit:
    HTTPClientForget(&client);
    ForgetCF(&msg);
    return (err);
}
#endif // !COMMON_SERVICES_NO_CORE_SERVICES
