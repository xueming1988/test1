/*
	Copyright (C) 2009-2019 Apple Inc. All Rights Reserved.
	
	Protocol:		Station-to-Station <http://en.wikipedia.org/wiki/Station-to-Station_protocol>.
	Key Exchange:	Curve25519 Elliptic-Curve Diffie-Hellman <http://cr.yp.to/ecdh.html>.
	Signing:		RSA <http://en.wikipedia.org/wiki/RSA#Signing_messages>.
	Encryption:		AES-128 in countermode <http://en.wikipedia.org/wiki/Advanced_Encryption_Standard>.
*/

#include "MFiSAP.h"

#include "AESUtils.h"
#include "CFUtils.h"
#include "CryptoHashUtils.h"
#include "DebugServices.h"
#include "RandomNumberUtils.h"
#include "SystemUtils.h"
#include "TickUtils.h"

#include CURVE25519_HEADER
#include SHA_HEADER

#if (TARGET_OS_MACOSX && !COMMON_SERVICES_NO_CORE_SERVICES)
#include <Security/SecPolicyPriv.h>
#include <Security/Security.h>
#endif
#if (TARGET_OS_IPHONE && !COMMON_SERVICES_NO_CORE_SERVICES)
#include <Security/SecCertificatePriv.h>
#include <Security/SecCMS.h>
#endif

//===========================================================================================================================
//	Constants
//===========================================================================================================================

// MFiSAPState

typedef uint8_t MFiSAPState;
#define kMFiSAPState_Invalid 0 // Not initialized.
#define kMFiSAPState_Init 1 // Initialized and ready for either client or server.
#define kMFiSAPState_ClientM1 2 // Client ready to generate M1.
#define kMFiSAPState_ClientM2 3 // Client ready to process server's M2.
#define kMFiSAPState_ClientDone 4 // Client successfully completed exchange process.
#define kMFiSAPState_ServerM1 5 // Server ready to process client's M1 and generate server's M2.
#define kMFiSAPState_ServerDone 6 // Server successfully completed exchange process.

// Misc

#define kMFiSAP_AES_IV_SaltPtr "AES-IV"
#define kMFiSAP_AES_IV_SaltLen (sizeof(kMFiSAP_AES_IV_SaltPtr) - 1)

#define kMFiSAP_AES_KEY_SaltPtr "AES-KEY"
#define kMFiSAP_AES_KEY_SaltLen (sizeof(kMFiSAP_AES_KEY_SaltPtr) - 1)

#define kMFiSAP_AESKeyLen 16
#define kMFiSAP_ECDHKeyLen 32
#define kMFiSAP_VersionLen 1

//===========================================================================================================================
//	Structures
//===========================================================================================================================

struct MFiSAP {
    MFiSAPState state;
    uint8_t version;
#if (MFI_SAP_ENABLE_CLIENT)
    uint8_t ecdhSK[kMFiSAP_ECDHKeyLen];
    uint8_t ecdhPK[kMFiSAP_ECDHKeyLen];
#endif
    uint8_t sharedSecret[kMFiSAP_ECDHKeyLen];
    uint8_t* certPtr;
    size_t certLen;
    AES_CTR_Context aesCtx;
    Boolean aesValid;
};

//===========================================================================================================================
//	Globals
//===========================================================================================================================

#if (MFI_SAP_ENABLE_SERVER)
static uint64_t gMFiSAP_LastTicks = 0;
static unsigned int gMFiSAP_ThrottleCounter = 0;
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

#if (MFI_SAP_ENABLE_CLIENT)
static OSStatus
_MFiSAP_Exchange_ClientM1(
    MFiSAPRef inRef,
    uint8_t** outOutputPtr,
    size_t* outOutputLen);
static OSStatus
_MFiSAP_Exchange_ClientM2(
    MFiSAPRef inRef,
    const uint8_t* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen);
#endif

#if (MFI_SAP_ENABLE_SERVER)
static OSStatus
_MFiSAP_Exchange_ServerM1(
    MFiSAPRef inRef,
    const uint8_t* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen);
#endif

//===========================================================================================================================
//	MFiSAP_Create
//===========================================================================================================================

OSStatus MFiSAP_Create(MFiSAPRef* outRef, uint8_t inVersion)
{
    OSStatus err;
    MFiSAPRef obj;

    require_action(inVersion == kMFiSAPVersion1, exit, err = kVersionErr);

    obj = (MFiSAPRef)calloc(1, sizeof(*obj));
    require_action(obj, exit, err = kNoMemoryErr);

    obj->state = kMFiSAPState_Init;
    obj->version = inVersion;

    *outRef = obj;
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	MFiSAP_Delete
//===========================================================================================================================

void MFiSAP_Delete(MFiSAPRef inRef)
{
    AES_CTR_Forget(&inRef->aesCtx, &inRef->aesValid);
    ForgetPtrLen(&inRef->certPtr, &inRef->certLen);
    MemZeroSecure(inRef, sizeof(*inRef));
    free(inRef);
}

//===========================================================================================================================
//	MFiSAP_CopyCertificate
//===========================================================================================================================

OSStatus MFiSAP_CopyCertificate(MFiSAPRef inRef, uint8_t** outCertPtr, size_t* outCertLen)
{
    OSStatus err;
    uint8_t* ptr;

    require_action_quiet(inRef->state == kMFiSAPState_ClientDone, exit, err = kStateErr);
    require_action_quiet(inRef->certPtr, exit, err = kNotFoundErr);
    require_action_quiet(inRef->certLen > 0, exit, err = kSizeErr);

    ptr = (uint8_t*)malloc(inRef->certLen);
    require_action(ptr, exit, err = kNoMemoryErr);
    memcpy(ptr, inRef->certPtr, inRef->certLen);

    *outCertPtr = ptr;
    *outCertLen = inRef->certLen;
    err = kNoErr;

exit:
    return (err);
}

#if (TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES)
//===========================================================================================================================
//	MFiSAP_CopyCertificateSerialNumber
//===========================================================================================================================
#define IsAppleInternalBuild() 0

OSStatus MFiSAP_CopyCertificateSerialNumber(MFiSAPRef inRef, uint8_t** outSNPtr, size_t* outSNLen)
{
    OSStatus err;
    CFDataRef certData;
    CFArrayRef certArray = NULL;
    SecCertificateRef cert;
    CFDataRef snData;
    uint8_t* snPtr;
    size_t snLen;

    require_action_quiet(inRef->state == kMFiSAPState_ClientDone, exit, err = kStateErr);
    require_action_quiet(inRef->certPtr, exit, err = kNotFoundErr);
    require_action_quiet(inRef->certLen > 0, exit, err = kSizeErr);

    certData = CFDataCreate(NULL, inRef->certPtr, (CFIndex)inRef->certLen);
    require_action(certData, exit, err = kNoMemoryErr);

#if (TARGET_OS_IPHONE)
    certArray = SecCMSCertificatesOnlyMessageCopyCertificates(certData);
    CFRelease(certData);
    require_action(certArray, exit, err = kMalformedErr);
#else
    {
        SecExternalFormat format;

        format = kSecFormatPKCS7;
        err = SecItemImport(certData, NULL, &format, NULL, 0, NULL, NULL, &certArray);
        CFRelease(certData);
        require_noerr(err, exit);
    }
#endif
    require_action(CFArrayGetCount(certArray) > 0, exit, err = kCountErr);
    cert = (SecCertificateRef)CFArrayGetValueAtIndex(certArray, 0);
    require_action(CFGetTypeID(cert) == SecCertificateGetTypeID(), exit, err = kTypeErr);

    snData = SecCertificateCopySerialNumberData(cert, NULL);
    require_action(snData, exit, err = kNotFoundErr);

    snPtr = CFCopyData(snData, &snLen, &err);
    CFRelease(snData);
    require_noerr(err, exit);

    // If it's not an AuthV3 serial number (16 byte) then work around iAP masking the high bit off the device class byte
    // by doing the same masking. iAP should really be changed to not do this in the first place <radar:26457309>.

    if ((snLen != 16) && (snLen > 7) && IsAppleInternalBuild()) {
        snPtr[7] &= 0x7F;
    }

    *outSNPtr = snPtr;
    *outSNLen = snLen;
    err = kNoErr;

exit:
    CFReleaseNullSafe(certArray);
    return (err);
}
#endif // TARGET_OS_DARWIN

//===========================================================================================================================
//	MFiSAP_Exchange
//===========================================================================================================================

OSStatus
MFiSAP_Exchange(
    MFiSAPRef inRef,
    const uint8_t* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone)
{
    OSStatus err;
    Boolean done = false;

    if (inRef->state == kMFiSAPState_Init)
        inRef->state = inInputPtr ? kMFiSAPState_ServerM1 : kMFiSAPState_ClientM1;
    switch (inRef->state) {
#if (MFI_SAP_ENABLE_CLIENT)
    case kMFiSAPState_ClientM1:
        err = _MFiSAP_Exchange_ClientM1(inRef, outOutputPtr, outOutputLen);
        require_noerr(err, exit);
        break;

    case kMFiSAPState_ClientM2:
        err = _MFiSAP_Exchange_ClientM2(inRef, inInputPtr, inInputLen, outOutputPtr, outOutputLen);
        require_noerr(err, exit);
        done = true;
        break;
#endif

#if (MFI_SAP_ENABLE_SERVER)
    case kMFiSAPState_ServerM1:
        err = _MFiSAP_Exchange_ServerM1(inRef, inInputPtr, inInputLen, outOutputPtr, outOutputLen);
        require_noerr(err, exit);
        done = true;
        break;
#endif

    default:
        dlogassert("bad state %d\n", inRef->state);
        err = kStateErr;
        goto exit;
    }
    ++inRef->state;
    *outDone = done;

exit:
    if (err)
        inRef->state = kMFiSAPState_Invalid; // Invalidate on errors to prevent retries.
    return (err);
}

#if (MFI_SAP_ENABLE_CLIENT)
//===========================================================================================================================
//	_MFiSAP_Exchange_ClientM1
//
//	Client: Generate client start message.
//===========================================================================================================================

static OSStatus
_MFiSAP_Exchange_ClientM1(
    MFiSAPRef inRef,
    uint8_t** outOutputPtr,
    size_t* outOutputLen)
{
    OSStatus err;
    size_t len;
    uint8_t* buf;
    uint8_t* dst;

    // Generate a random ECDH key pair.

    err = RandomBytes(inRef->ecdhSK, sizeof(inRef->ecdhSK));
    require_noerr(err, exit);
    CryptoHKDF(kCryptoHashDescriptor_SHA512, inRef->ecdhSK, sizeof(inRef->ecdhSK),
        "MFiSAP-ECDH-Salt", sizeof_string("MFiSAP-ECDH-Salt"),
        "MFiSAP-ECDH-Info", sizeof_string("MFiSAP-ECDH-Info"),
        sizeof(inRef->ecdhSK), inRef->ecdhSK);
    curve25519(inRef->ecdhPK, inRef->ecdhSK, NULL);

    // Return <1:version> <32:our ECDH public key>.

    len = kMFiSAP_VersionLen + kMFiSAP_ECDHKeyLen;
    buf = (uint8_t*)malloc(len);
    require_action(buf, exit, err = kNoMemoryErr);
    dst = buf;

    *dst++ = inRef->version;
    memcpy(dst, inRef->ecdhPK, sizeof(inRef->ecdhPK));
    dst += sizeof(inRef->ecdhPK);

    check(dst == (buf + len));
    *outOutputPtr = buf;
    *outOutputLen = len;

exit:
    return (err);
}

//===========================================================================================================================
//	_MFiSAP_Exchange_ClientM2
//
//	Client: verify server's response and generate M3.
//===========================================================================================================================

static OSStatus
_MFiSAP_Exchange_ClientM2(
    MFiSAPRef inRef,
    const uint8_t* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen)
{
    OSStatus err;
    const uint8_t* inputEnd;
    const uint8_t* peerPK;
    size_t certificateLen;
    const uint8_t* certificatePtr;
    size_t signatureLen;
    const uint8_t* signaturePtr;
    uint8_t* signatureBuf = NULL;
    SHA_CTX sha1Ctx;
    uint8_t aesKey[20]; // Only 16 bytes needed for AES, but 20 bytes needed to store full SHA-1 hash.
    uint8_t aesIV[20];
    uint8_t challenge[2 * kMFiSAP_ECDHKeyLen];

    // Validate input and setup ptrs. Input data must be in the following format:
    //
    //		<32:server's ECDH public key>
    //		<4:big endian certificate length>
    //		<N:certificate data>
    //		<4:big endian signature length>
    //		<N:signature data>

    inputEnd = inInputPtr + inInputLen;
    require_action(inputEnd > inInputPtr, exit, err = kSizeErr); // Detect bad length causing ptr wrap.

    require_action((inputEnd - inInputPtr) >= kMFiSAP_ECDHKeyLen, exit, err = kSizeErr);
    peerPK = inInputPtr;
    inInputPtr += kMFiSAP_ECDHKeyLen;

    require_action((inputEnd - inInputPtr) >= 4, exit, err = kSizeErr);
    certificateLen = ReadBig32(inInputPtr);
    inInputPtr += 4;
    require_action(certificateLen > 0, exit, err = kSizeErr);
    require_action(((size_t)(inputEnd - inInputPtr)) >= certificateLen, exit, err = kSizeErr);
    certificatePtr = inInputPtr;
    inInputPtr += certificateLen;

    require_action((inputEnd - inInputPtr) >= 4, exit, err = kSizeErr);
    signatureLen = ReadBig32(inInputPtr);
    inInputPtr += 4;
    require_action(signatureLen > 0, exit, err = kSizeErr);
    require_action(((size_t)(inputEnd - inInputPtr)) >= signatureLen, exit, err = kSizeErr);
    signaturePtr = inInputPtr;
    inInputPtr += signatureLen;

    require_action(inInputPtr == inputEnd, exit, err = kSizeErr);

    // Use our private key and the server's public key to generate the shared secret.
    // Hash the shared secret with salt and truncate to form the AES key and IV.

    curve25519(inRef->sharedSecret, inRef->ecdhSK, peerPK);
    require_action(Curve25519ValidSharedSecret(inRef->sharedSecret), exit, err = kMalformedErr);
    SHA1_Init(&sha1Ctx);
    SHA1_Update(&sha1Ctx, kMFiSAP_AES_KEY_SaltPtr, kMFiSAP_AES_KEY_SaltLen);
    SHA1_Update(&sha1Ctx, inRef->sharedSecret, sizeof(inRef->sharedSecret));
    SHA1_Final(aesKey, &sha1Ctx);

    SHA1_Init(&sha1Ctx);
    SHA1_Update(&sha1Ctx, kMFiSAP_AES_IV_SaltPtr, kMFiSAP_AES_IV_SaltLen);
    SHA1_Update(&sha1Ctx, inRef->sharedSecret, sizeof(inRef->sharedSecret));
    SHA1_Final(aesIV, &sha1Ctx);

    // Decrypt the signature with the AES key and IV.

    signatureBuf = (uint8_t*)malloc(signatureLen);
    require_action(signatureBuf, exit, err = kNoMemoryErr);
    memcpy(signatureBuf, signaturePtr, signatureLen);

    err = AES_CTR_Init(&inRef->aesCtx, aesKey, aesIV);
    require_noerr(err, exit);
    err = AES_CTR_Update(&inRef->aesCtx, signaturePtr, signatureLen, signatureBuf);
    if (err)
        AES_CTR_Final(&inRef->aesCtx);
    require_noerr(err, exit);
    inRef->aesValid = true;

    // Verify the signature of <32:server's ECDH public key> <32:our ECDH public key> was signed by an MFi auth chip.

    memcpy(challenge, peerPK, kMFiSAP_ECDHKeyLen);
    memcpy(challenge + kMFiSAP_ECDHKeyLen, inRef->ecdhPK, sizeof(inRef->ecdhPK));
    err = MFiPlatform_VerifySignature(challenge, sizeof(challenge), signatureBuf, signatureLen,
        certificatePtr, certificateLen);
    require_noerr(err, exit);

    // Save off the certificate for callers that may want it.

    ForgetPtrLen(&inRef->certPtr, &inRef->certLen);
    inRef->certPtr = (uint8_t*)malloc(certificateLen);
    check(inRef->certPtr);
    if (inRef->certPtr) {
        memcpy(inRef->certPtr, certificatePtr, certificateLen);
        inRef->certLen = certificateLen;
    }

    // Return an empty response (which is not intended to be sent to the server).

    *outOutputPtr = NULL;
    *outOutputLen = 0;

exit:
    FreeNullSafe(signatureBuf);
    return (err);
}
#endif // MFI_SAP_ENABLE_CLIENT

#if (MFI_SAP_ENABLE_SERVER)
//===========================================================================================================================
//	_MFiSAP_Exchange_ServerM1
//
//	Server: process client's M1 and generate M2.
//===========================================================================================================================

static OSStatus
_MFiSAP_Exchange_ServerM1(
    MFiSAPRef inRef,
    const uint8_t* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen)
{
    OSStatus err;
    const uint8_t* inputEnd;
    const uint8_t* peerPK;
    uint8_t ourSK[kMFiSAP_ECDHKeyLen];
    uint8_t ourPK[kMFiSAP_ECDHKeyLen];
    SHA_CTX sha1Ctx;
    uint8_t* signaturePtr = NULL;
    size_t signatureLen;
    uint8_t* certificatePtr = NULL;
    size_t certificateLen;
    uint8_t aesKey[20]; // Only 16 bytes needed for AES, but 20 bytes needed to store full SHA-1 hash.
    uint8_t aesIV[20];
    uint8_t dataToSign[2 * kMFiSAP_ECDHKeyLen];
    uint8_t* buf;
    uint8_t* dst;
    size_t len;

    // Throttle requests to no more than 1 per second and linearly back off to 4 seconds max.

    if ((UpTicks() - gMFiSAP_LastTicks) < UpTicksPerSecond()) {
        if (gMFiSAP_ThrottleCounter < 4)
            ++gMFiSAP_ThrottleCounter;
        SleepForUpTicks(gMFiSAP_ThrottleCounter * UpTicksPerSecond());
    } else {
        gMFiSAP_ThrottleCounter = 0;
    }
    gMFiSAP_LastTicks = UpTicks();

    // Validate inputs. Input data must be: <1:version> <32:client's ECDH public key>.

    inputEnd = inInputPtr + inInputLen;
    require_action(inputEnd > inInputPtr, exit, err = kSizeErr); // Detect bad length causing ptr wrap.

    require_action((inputEnd - inInputPtr) >= kMFiSAP_VersionLen, exit, err = kSizeErr);
    inRef->version = *inInputPtr++;
    require_action(inRef->version == kMFiSAPVersion1, exit, err = kVersionErr);

    require_action((inputEnd - inInputPtr) >= kMFiSAP_ECDHKeyLen, exit, err = kSizeErr);
    peerPK = inInputPtr;
    inInputPtr += kMFiSAP_ECDHKeyLen;

    require_action(inInputPtr == inputEnd, exit, err = kSizeErr);

    // Generate a random ECDH key pair.

    err = RandomBytes(ourSK, sizeof(ourSK));
    require_noerr(err, exit);
    CryptoHKDF(kCryptoHashDescriptor_SHA512, ourSK, sizeof(ourSK),
        "MFiSAP-ECDH-Salt", sizeof_string("MFiSAP-ECDH-Salt"),
        "MFiSAP-ECDH-Info", sizeof_string("MFiSAP-ECDH-Info"),
        sizeof(ourSK), ourSK);
    curve25519(ourPK, ourSK, NULL);

    // Use our private key and the client's public key to generate the shared secret.
    // Hash the shared secret with salt and truncate to form the AES key and IV.

    curve25519(inRef->sharedSecret, ourSK, peerPK);
    require_action(Curve25519ValidSharedSecret(inRef->sharedSecret), exit, err = kMalformedErr);
    SHA1_Init(&sha1Ctx);
    SHA1_Update(&sha1Ctx, kMFiSAP_AES_KEY_SaltPtr, kMFiSAP_AES_KEY_SaltLen);
    SHA1_Update(&sha1Ctx, inRef->sharedSecret, sizeof(inRef->sharedSecret));
    SHA1_Final(aesKey, &sha1Ctx);

    SHA1_Init(&sha1Ctx);
    SHA1_Update(&sha1Ctx, kMFiSAP_AES_IV_SaltPtr, kMFiSAP_AES_IV_SaltLen);
    SHA1_Update(&sha1Ctx, inRef->sharedSecret, sizeof(inRef->sharedSecret));
    SHA1_Final(aesIV, &sha1Ctx);

    // Use the auth chip to sign a hash of <32:our ECDH public key> <32:client's ECDH public key>.
    // And copy the auth chip's certificate so the client can verify the signature.

    memcpy(&dataToSign[0], ourPK, sizeof(ourPK));
    memcpy(&dataToSign[32], peerPK, kMFiSAP_ECDHKeyLen);
    err = MFiPlatform_CreateSignatureEx(kMFiAuthFlagsNone, dataToSign, 2 * kMFiSAP_ECDHKeyLen, &signaturePtr, &signatureLen);
    require_noerr(err, exit);

    err = MFiPlatform_CopyCertificate(&certificatePtr, &certificateLen);
    require_noerr(err, exit);

    // Encrypt the signature with the AES key and IV.

    err = AES_CTR_Init(&inRef->aesCtx, aesKey, aesIV);
    require_noerr(err, exit);
    err = AES_CTR_Update(&inRef->aesCtx, signaturePtr, signatureLen, signaturePtr);
    if (err)
        AES_CTR_Final(&inRef->aesCtx);
    require_noerr(err, exit);
    inRef->aesValid = true;

    // Return the response:
    //
    //		<32:our ECDH public key>
    //		<4:big endian certificate length>
    //		<N:certificate data>
    //		<4:big endian signature length>
    //		<N:encrypted signature data>

    len = kMFiSAP_ECDHKeyLen + 4 + certificateLen + 4 + signatureLen;
    buf = (uint8_t*)malloc(len);
    require_action(buf, exit, err = kNoMemoryErr);
    dst = buf;
    memcpy(dst, ourPK, sizeof(ourPK));
    dst += sizeof(ourPK);
    WriteBig32(dst, certificateLen);
    dst += 4;
    memcpy(dst, certificatePtr, certificateLen);
    dst += certificateLen;
    WriteBig32(dst, signatureLen);
    dst += 4;
    memcpy(dst, signaturePtr, signatureLen);
    dst += signatureLen;

    check(dst == (buf + len));
    *outOutputPtr = buf;
    *outOutputLen = (size_t)(dst - buf);

exit:
    FreeNullSafe(certificatePtr);
    FreeNullSafe(signaturePtr);
    return (err);
}
#endif // MFI_SAP_ENABLE_SERVER

//===========================================================================================================================
//	MFiSAP_Encrypt
//===========================================================================================================================

OSStatus MFiSAP_Encrypt(MFiSAPRef inRef, const void* inPlaintext, size_t inLen, void* inCiphertextBuf)
{
    OSStatus err;

    require_action((inRef->state == kMFiSAPState_ClientDone) || (inRef->state == kMFiSAPState_ServerDone),
        exit, err = kStateErr);

    err = AES_CTR_Update(&inRef->aesCtx, inPlaintext, inLen, inCiphertextBuf);
    require_noerr(err, exit);

exit:
    return (err);
}

//===========================================================================================================================
//	MFiSAP_Decrypt
//===========================================================================================================================

OSStatus MFiSAP_Decrypt(MFiSAPRef inRef, const void* inCiphertext, size_t inLen, void* inPlaintextBuf)
{
    OSStatus err;

    require_action((inRef->state == kMFiSAPState_ClientDone) || (inRef->state == kMFiSAPState_ServerDone),
        exit, err = kStateErr);

    err = AES_CTR_Update(&inRef->aesCtx, inCiphertext, inLen, inPlaintextBuf);
    require_noerr(err, exit);

exit:
    return (err);
}

#if 0
#pragma mark -
#endif

#if (MFI_SAP_ENABLE_CLIENT)
//===========================================================================================================================
//	MFiClient_Test
//===========================================================================================================================

// Signature of a SHA-1 digest of "Test message". Should verify against kExampleMFiAuthCertificate.

#define kExampleMFiMessage "Test message"

static const uint8_t kExampleMFiAuthSignature[] = {
    0x14, 0x22, 0x96, 0x74, 0x15, 0xC5, 0xE9, 0x1A, 0xC0, 0x11, 0xF5, 0xCA, 0x5F, 0x15, 0x86, 0x5D,
    0x92, 0xCE, 0x5F, 0x49, 0x6F, 0x4B, 0xBC, 0x0A, 0x78, 0x4D, 0x96, 0xBF, 0xF3, 0x95, 0x59, 0xD1,
    0x63, 0xE8, 0x7F, 0xDB, 0xF4, 0xC3, 0x14, 0x10, 0x5C, 0x8C, 0x19, 0x9A, 0xA4, 0x4A, 0x5D, 0x33,
    0xA6, 0x39, 0x17, 0x45, 0x31, 0x56, 0x84, 0x96, 0x2D, 0xE8, 0xA9, 0x72, 0x2A, 0xA0, 0xBF, 0x20,
    0xF2, 0x00, 0x89, 0x09, 0xF8, 0xE8, 0xCC, 0x17, 0x7E, 0xAC, 0x61, 0x25, 0xC2, 0x1D, 0x74, 0xDF,
    0x2E, 0x25, 0xA9, 0x74, 0x1A, 0xAC, 0xCE, 0xAA, 0x33, 0x2C, 0xFA, 0xD8, 0xBB, 0x70, 0xD6, 0xBA,
    0x27, 0x8B, 0x06, 0x54, 0xA9, 0x29, 0x5C, 0x0F, 0x6E, 0x50, 0xA3, 0xC6, 0x2B, 0xBC, 0xC9, 0x7B,
    0x31, 0x7B, 0x0F, 0x04, 0x0B, 0x67, 0xE9, 0xB4, 0x65, 0x58, 0xAA, 0x55, 0x39, 0x79, 0x65, 0xBC
};

// Certificate from a real MFi Auth chip (each one has a unique certificate).

static const uint8_t kExampleMFiAuthCertificate[] = {
    0x30, 0x82, 0x03, 0xAD, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x07, 0x02, 0xA0,
    0x82, 0x03, 0x9E, 0x30, 0x82, 0x03, 0x9A, 0x02, 0x01, 0x01, 0x31, 0x00, 0x30, 0x0B, 0x06, 0x09,
    0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x07, 0x01, 0xA0, 0x82, 0x03, 0x80, 0x30, 0x82, 0x03,
    0x7C, 0x30, 0x82, 0x02, 0x64, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x0F, 0x33, 0x33, 0xAA, 0x07,
    0x10, 0x16, 0xAA, 0x06, 0xAA, 0x00, 0x10, 0xAA, 0x01, 0x03, 0x68, 0x30, 0x0D, 0x06, 0x09, 0x2A,
    0x86, 0x48, 0x86, 0xF7, 0x0D, 0x01, 0x01, 0x05, 0x05, 0x00, 0x30, 0x81, 0x92, 0x31, 0x0B, 0x30,
    0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x1D, 0x30, 0x1B, 0x06, 0x03,
    0x55, 0x04, 0x0A, 0x13, 0x14, 0x41, 0x70, 0x70, 0x6C, 0x65, 0x20, 0x43, 0x6F, 0x6D, 0x70, 0x75,
    0x74, 0x65, 0x72, 0x2C, 0x20, 0x49, 0x6E, 0x63, 0x2E, 0x31, 0x2D, 0x30, 0x2B, 0x06, 0x03, 0x55,
    0x04, 0x0B, 0x13, 0x24, 0x41, 0x70, 0x70, 0x6C, 0x65, 0x20, 0x43, 0x6F, 0x6D, 0x70, 0x75, 0x74,
    0x65, 0x72, 0x20, 0x43, 0x65, 0x72, 0x74, 0x69, 0x66, 0x69, 0x63, 0x61, 0x74, 0x65, 0x20, 0x41,
    0x75, 0x74, 0x68, 0x6F, 0x72, 0x69, 0x74, 0x79, 0x31, 0x35, 0x30, 0x33, 0x06, 0x03, 0x55, 0x04,
    0x03, 0x13, 0x2C, 0x41, 0x70, 0x70, 0x6C, 0x65, 0x20, 0x69, 0x50, 0x6F, 0x64, 0x20, 0x41, 0x63,
    0x63, 0x65, 0x73, 0x73, 0x6F, 0x72, 0x69, 0x65, 0x73, 0x20, 0x43, 0x65, 0x72, 0x74, 0x69, 0x66,
    0x69, 0x63, 0x61, 0x74, 0x65, 0x20, 0x41, 0x75, 0x74, 0x68, 0x6F, 0x72, 0x69, 0x74, 0x79, 0x30,
    0x1E, 0x17, 0x0D, 0x30, 0x37, 0x31, 0x30, 0x31, 0x36, 0x30, 0x30, 0x33, 0x33, 0x34, 0x39, 0x5A,
    0x17, 0x0D, 0x31, 0x35, 0x31, 0x30, 0x31, 0x36, 0x30, 0x30, 0x33, 0x33, 0x34, 0x39, 0x5A, 0x30,
    0x81, 0x83, 0x31, 0x0B, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31,
    0x1D, 0x30, 0x1B, 0x06, 0x03, 0x55, 0x04, 0x0A, 0x13, 0x14, 0x41, 0x70, 0x70, 0x6C, 0x65, 0x20,
    0x43, 0x6F, 0x6D, 0x70, 0x75, 0x74, 0x65, 0x72, 0x2C, 0x20, 0x49, 0x6E, 0x63, 0x2E, 0x31, 0x28,
    0x30, 0x26, 0x06, 0x03, 0x55, 0x04, 0x0B, 0x13, 0x1F, 0x41, 0x70, 0x70, 0x6C, 0x65, 0x20, 0x43,
    0x6F, 0x6D, 0x70, 0x75, 0x74, 0x65, 0x72, 0x20, 0x69, 0x50, 0x6F, 0x64, 0x20, 0x41, 0x63, 0x63,
    0x65, 0x73, 0x73, 0x6F, 0x72, 0x69, 0x65, 0x73, 0x31, 0x2B, 0x30, 0x29, 0x06, 0x03, 0x55, 0x04,
    0x03, 0x14, 0x22, 0x49, 0x50, 0x41, 0x5F, 0x33, 0x33, 0x33, 0x33, 0x41, 0x41, 0x30, 0x37, 0x31,
    0x30, 0x31, 0x36, 0x41, 0x41, 0x30, 0x36, 0x41, 0x41, 0x30, 0x30, 0x31, 0x30, 0x41, 0x41, 0x30,
    0x31, 0x30, 0x33, 0x36, 0x38, 0x30, 0x81, 0x9F, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48, 0x86,
    0xF7, 0x0D, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x81, 0x8D, 0x00, 0x30, 0x81, 0x89, 0x02, 0x81,
    0x81, 0x00, 0xAE, 0x58, 0x33, 0xBA, 0x7B, 0xD2, 0x05, 0xC3, 0xD1, 0xD1, 0x69, 0xBA, 0xDB, 0x6C,
    0x40, 0xE3, 0x2F, 0x4D, 0x46, 0xB8, 0xD9, 0x88, 0x81, 0x49, 0x03, 0xB1, 0xDD, 0x17, 0xCA, 0x65,
    0x49, 0xF0, 0x1A, 0x65, 0x43, 0x48, 0x4E, 0xAE, 0xBC, 0xA7, 0xED, 0x4C, 0x52, 0x13, 0x5C, 0x26,
    0x6A, 0x2F, 0x66, 0x30, 0xE3, 0x84, 0xBD, 0x69, 0x2E, 0x20, 0x16, 0x7B, 0x50, 0x48, 0xB1, 0x68,
    0xB9, 0x34, 0xC4, 0x65, 0xDA, 0x3A, 0x02, 0x16, 0x18, 0x78, 0x52, 0x52, 0x1F, 0x03, 0x5A, 0xA0,
    0xB3, 0x1F, 0xAD, 0x39, 0x00, 0x42, 0x0C, 0xF0, 0x78, 0xA9, 0x4F, 0x3E, 0xD9, 0x31, 0xCB, 0x92,
    0x6C, 0x04, 0xA9, 0x73, 0x4B, 0xE5, 0x4C, 0x96, 0xBE, 0xF2, 0x60, 0x4E, 0xD1, 0x0E, 0x9F, 0xD3,
    0x35, 0x0B, 0x57, 0xA6, 0x18, 0xC2, 0xDC, 0xDA, 0x32, 0xD9, 0x1C, 0x1A, 0x1E, 0x2D, 0x24, 0xC3,
    0x68, 0x69, 0x02, 0x03, 0x01, 0x00, 0x01, 0xA3, 0x60, 0x30, 0x5E, 0x30, 0x0E, 0x06, 0x03, 0x55,
    0x1D, 0x0F, 0x01, 0x01, 0xFF, 0x04, 0x04, 0x03, 0x02, 0x03, 0xB8, 0x30, 0x0C, 0x06, 0x03, 0x55,
    0x1D, 0x13, 0x01, 0x01, 0xFF, 0x04, 0x02, 0x30, 0x00, 0x30, 0x1D, 0x06, 0x03, 0x55, 0x1D, 0x0E,
    0x04, 0x16, 0x04, 0x14, 0x39, 0x44, 0x47, 0x64, 0x0E, 0x32, 0x97, 0x35, 0xCA, 0x2B, 0x88, 0x12,
    0x8F, 0x5E, 0x9E, 0xD2, 0xCF, 0xB6, 0x5C, 0xB9, 0x30, 0x1F, 0x06, 0x03, 0x55, 0x1D, 0x23, 0x04,
    0x18, 0x30, 0x16, 0x80, 0x14, 0xC9, 0xAA, 0x84, 0x6B, 0x06, 0xB8, 0x76, 0xE2, 0x96, 0x4F, 0xE7,
    0x27, 0x02, 0xD7, 0x2E, 0x3B, 0xDA, 0xF7, 0xB0, 0x18, 0x30, 0x0D, 0x06, 0x09, 0x2A, 0x86, 0x48,
    0x86, 0xF7, 0x0D, 0x01, 0x01, 0x05, 0x05, 0x00, 0x03, 0x82, 0x01, 0x01, 0x00, 0x3A, 0x80, 0xBA,
    0x96, 0x83, 0xFA, 0x3D, 0x09, 0x8F, 0xEA, 0xC7, 0xEB, 0x26, 0x77, 0xD5, 0x14, 0x73, 0x8E, 0x9A,
    0x00, 0xE6, 0x75, 0x3F, 0x99, 0x0C, 0x68, 0xEB, 0xD6, 0xF3, 0xC7, 0x3A, 0xA7, 0xC7, 0x06, 0x39,
    0x5E, 0xBE, 0x2B, 0xA6, 0x07, 0xA8, 0x95, 0x3E, 0x2A, 0xCD, 0x1C, 0x02, 0x22, 0xD8, 0xC5, 0xC2,
    0x07, 0x2E, 0x66, 0xE6, 0xD9, 0x27, 0x07, 0xE8, 0x5D, 0x43, 0x5D, 0x17, 0x28, 0xAD, 0xE3, 0x7C,
    0x46, 0x6D, 0x9A, 0x48, 0x3A, 0xAD, 0x8A, 0x02, 0x01, 0x3B, 0x6A, 0xE1, 0x8C, 0x3A, 0x19, 0xD4,
    0xE2, 0x73, 0x78, 0x07, 0x21, 0x79, 0x7E, 0xC6, 0x13, 0x5D, 0x2F, 0xAD, 0x4E, 0x40, 0xED, 0xA1,
    0x58, 0xB1, 0x0D, 0x61, 0x40, 0x24, 0xE9, 0x42, 0xFA, 0x6A, 0xFD, 0xB8, 0x82, 0xAA, 0x59, 0x8B,
    0xC3, 0x5D, 0xFC, 0x22, 0xF2, 0x1F, 0xF9, 0xDA, 0xC2, 0xA0, 0x80, 0xFE, 0x2D, 0x35, 0x8A, 0xF7,
    0x20, 0x59, 0xC1, 0xA0, 0x28, 0x1A, 0x88, 0xF2, 0x07, 0xCF, 0x15, 0x31, 0x4F, 0x72, 0xD1, 0x09,
    0x61, 0x9A, 0x8C, 0xD4, 0xFA, 0xF7, 0x02, 0x15, 0xA0, 0x5B, 0xB8, 0xD8, 0x1F, 0xF3, 0x29, 0xB8,
    0x11, 0xF9, 0xD3, 0x24, 0xC8, 0x06, 0x66, 0xD3, 0xC0, 0x89, 0x42, 0x66, 0xDB, 0x50, 0xC5, 0xAC,
    0xE0, 0xEF, 0x83, 0x4B, 0x99, 0x81, 0xBD, 0x93, 0xEC, 0xD9, 0x28, 0x57, 0x8F, 0x49, 0xF2, 0x8F,
    0x6E, 0x12, 0x46, 0x64, 0xFB, 0xB1, 0x63, 0xFA, 0x5E, 0xCD, 0xB4, 0x56, 0xF4, 0xBD, 0x26, 0x8C,
    0x3B, 0x1E, 0xFB, 0xED, 0x07, 0x31, 0xAB, 0x57, 0x0F, 0x92, 0x82, 0x5A, 0x39, 0xF0, 0x31, 0x2E,
    0xE0, 0x83, 0xE3, 0xF6, 0xAC, 0xA3, 0x29, 0xC0, 0x6E, 0x7E, 0x84, 0x5B, 0x6C, 0x86, 0x10, 0x54,
    0x25, 0x33, 0x10, 0x8F, 0x3F, 0x65, 0x6D, 0xDC, 0x79, 0x94, 0x8D, 0xCC, 0x6C, 0xA1, 0x00, 0x31,
    0x00
};

OSStatus MFiClient_Test(void);
OSStatus MFiClient_Test(void)
{
    OSStatus err;

    err = MFiPlatform_VerifySignature(
        kExampleMFiMessage, strlen(kExampleMFiMessage),
        kExampleMFiAuthSignature, sizeof(kExampleMFiAuthSignature),
        kExampleMFiAuthCertificate, sizeof(kExampleMFiAuthCertificate));
    require_noerr(err, exit);

exit:
    printf("MFiClient_Test: %s\n", !err ? "PASSED" : "FAILED");
    return (err);
}
#endif // MFI_SAP_ENABLE_CLIENT

#if (!EXCLUDE_UNIT_TESTS && DEBUG && MFI_SAP_ENABLE_CLIENT && MFI_SAP_ENABLE_SERVER)
//===========================================================================================================================
//	MFiSAP_Test
//===========================================================================================================================

OSStatus MFiSAP_Test(void)
{
    OSStatus err;
    MFiSAPRef clientSAP = NULL;
    Boolean clientDone;
    size_t len;
    uint8_t* inputPtr;
    uint8_t* outputPtr;
    MFiSAPRef serverSAP = NULL;
    Boolean serverDone;
    size_t i;
    uint8_t plaintext[64];
    uint8_t temptext[64];
    uint8_t ciphertext[64];

    err = MFiSAP_Create(&clientSAP, kMFiSAPVersion1);
    require_noerr(err, exit);

    err = MFiSAP_CopyCertificate(clientSAP, &outputPtr, &len);
    require_action(err != kNoErr, exit, err = -1);

    err = MFiSAP_Create(&serverSAP, kMFiSAPVersion1);
    require_noerr(err, exit);

    len = 0;
    inputPtr = NULL;
    outputPtr = NULL;
    clientDone = false;
    serverDone = false;
    for (i = 0;; ++i) {
        inputPtr = outputPtr;
        err = MFiSAP_Exchange(clientSAP, inputPtr, len, &outputPtr, &len, &clientDone);
        FreeNullSafe(inputPtr);
        require_noerr(err, exit);
        if (clientDone)
            break;
        dlog(kLogLevelMax, "Client %d:\n%1.1H", i, outputPtr, len, len);

        inputPtr = outputPtr;
        err = MFiSAP_Exchange(serverSAP, inputPtr, len, &outputPtr, &len, &serverDone);
        FreeNullSafe(inputPtr);
        require_noerr(err, exit);
        dlog(kLogLevelMax, "Server %d:\n%1.1H", i, outputPtr, len, len);
    }
    require_action(clientDone && serverDone, exit, err = -1);

    outputPtr = NULL;
    len = 0;
    err = MFiSAP_CopyCertificate(clientSAP, &outputPtr, &len);
    require_noerr(err, exit);
    err = (outputPtr && (len > 0)) ? kNoErr : -1;
    ForgetMem(&outputPtr);
    require_noerr(err, exit);

    outputPtr = NULL;
    len = 0;
    err = MFiSAP_CopyCertificateSerialNumber(clientSAP, &outputPtr, &len);
    require_noerr(err, exit);
    err = (outputPtr && (len > 0)) ? kNoErr : -1;
    ForgetMem(&outputPtr);
    require_noerr(err, exit);

    // Client encrypt -> Server decrypt.

    for (i = 0; i < sizeof(plaintext); ++i)
        plaintext[i] = (uint8_t)(i & 0xFF);
    err = MFiSAP_Encrypt(clientSAP, plaintext, sizeof(plaintext), ciphertext);
    require_noerr(err, exit);
    require_action(memcmp(plaintext, ciphertext, sizeof(plaintext)) != 0, exit, err = -1);
    dlog(kLogLevelMax, "Client ciphertext:\n%1.1H", ciphertext, sizeof(plaintext), sizeof(plaintext));

    memset(temptext, 0, sizeof(temptext));
    err = MFiSAP_Decrypt(serverSAP, ciphertext, sizeof(ciphertext), temptext);
    require_noerr(err, exit);
    require_action(memcmp(temptext, plaintext, sizeof(plaintext)) == 0, exit, err = -1);
    dlog(kLogLevelMax, "Server plaintext:\n%1.1H", temptext, sizeof(temptext), sizeof(temptext));

    // Server encrypt -> Client decrypt.

    for (i = 0; i < sizeof(plaintext); ++i)
        plaintext[i] = (uint8_t)(i & 0xFF);
    err = MFiSAP_Encrypt(serverSAP, plaintext, sizeof(plaintext), ciphertext);
    require_noerr(err, exit);
    require_action(memcmp(plaintext, ciphertext, sizeof(plaintext)) != 0, exit, err = -1);
    dlog(kLogLevelMax, "Server ciphertext:\n%1.1H", ciphertext, sizeof(plaintext), sizeof(plaintext));

    memset(temptext, 0, sizeof(temptext));
    err = MFiSAP_Decrypt(clientSAP, ciphertext, sizeof(ciphertext), temptext);
    require_noerr(err, exit);
    require_action(memcmp(temptext, plaintext, sizeof(plaintext)) == 0, exit, err = -1);
    dlog(kLogLevelMax, "Client plaintext:\n%1.1H", temptext, sizeof(temptext), sizeof(temptext));

exit:
    MFiSAP_Forget(&clientSAP);
    MFiSAP_Forget(&serverSAP);
    printf("MFiSAP_Test: %s\n", !err ? "PASSED" : "FAILED");
    return (err);
}
#endif // DEBUG && MFI_SAP_ENABLE_CLIENT && MFI_SAP_ENABLE_SERVER
