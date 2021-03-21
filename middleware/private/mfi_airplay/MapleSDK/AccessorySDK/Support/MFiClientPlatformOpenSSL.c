/*
	Copyright (C) 2010-2019 Apple Inc. All Rights Reserved.
*/

#include <stdlib.h>
#include <string.h>

#include "CommonServices.h"
#include "DebugServices.h"
#include "MFiCertificates.h"
#include "MFiSAP.h"

#include CF_HEADER
#include SHA_HEADER

#include <openssl/objects.h>
#include <openssl/opensslv.h>
#include <openssl/pkcs7.h>
#include <openssl/x509.h>

#if (OPENSSL_VERSION_NUMBER >= 0x00908000) // 0.9.8 and later use const.
#define d2i_X509_compat(PX, IO_PTR, LEN) d2i_X509((PX), (IO_PTR), (LEN))
#define d2i_PKCS7_compat(PX, IO_PTR, LEN) d2i_PKCS7((PX), (IO_PTR), (LEN))
#define EVP_VerifyFinal_compat(CTX, SIGBUF, SIGLEN, KEY) \
    EVP_VerifyFinal((CTX), (SIGBUF), (SIGLEN), (KEY))
#else
#define d2i_X509_compat(PX, IO_PTR, LEN) d2i_X509((PX), (unsigned char**)(IO_PTR), (LEN))
#define d2i_PKCS7_compat(PX, IO_PTR, LEN) d2i_PKCS7((PX), (unsigned char**)(IO_PTR), (LEN))
#define EVP_VerifyFinal_compat(CTX, SIGBUF, SIGLEN, KEY) \
    EVP_VerifyFinal((CTX), (unsigned char*)(SIGBUF), (SIGLEN), (KEY))
#endif

// If we're building in iTunes then we don't have the real DebugServices so map it to something similar.
// This relies on BUILDING_ITUNES_APP being defined because iTunes defines it to 1 or 0 even if the
// code is not being used to build the app (e.g. when building the AppleTV library). When non-iTunes
// code is being built, BUILDING_ITUNES_APP won't be defined at all so it'll skip this section.

#include "DebugUtilities.h"

static OSStatus _MFiPlatform_VerifySerialNumber(const uint8_t* inSNPtr, size_t inSNLen);

#if (COMPILER_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

//===========================================================================================================================
//	MFiPlatform_VerifySignature
//===========================================================================================================================

static OSStatus
_MFiPlatform_VerifySignatureEx(
    const void* inDataPtr,
    size_t inDataLen,
    const void* inSignaturePtr,
    size_t inSignatureLen,
    const void* inCertificatePtr,
    size_t inCertificateLen,
    const void* inRootCertificatePtr,
    size_t inRootCertificateLen,
    const void* inSubCACertificatePtr,
    size_t inSubCACertificateLen);

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

    mfic_ulog(kLogLevelVerbose, "MFi certificate:\n%1.2H\n", inCertificatePtr, (int)inCertificateLen, (int)inCertificateLen);
    mfic_ulog(kLogLevelVerbose, "MFi signature:\n%1.2H\n", inSignaturePtr, (int)inSignatureLen, (int)inSignatureLen);

    err = _MFiPlatform_VerifySignatureEx(inDataPtr, inDataLen, inSignaturePtr, inSignatureLen,
        inCertificatePtr, inCertificateLen, kAppleRootCA1, kAppleRootCA1Len, kiPodAccessoryCA1, kiPodAccessoryCA1Len);
    if (err) {
        err = _MFiPlatform_VerifySignatureEx(inDataPtr, inDataLen, inSignaturePtr, inSignatureLen,
            inCertificatePtr, inCertificateLen, kAppleRootCA2, kAppleRootCA2Len, kiPodAccessoryCA2, kiPodAccessoryCA2Len);
    }
    return (err);
}

static OSStatus
_MFiPlatform_VerifySignatureEx(
    const void* inDataPtr,
    size_t inDataLen,
    const void* inSignaturePtr,
    size_t inSignatureLen,
    const void* inCertificatePtr,
    size_t inCertificateLen,
    const void* inRootCertificatePtr,
    size_t inRootCertificateLen,
    const void* inSubCACertificatePtr,
    size_t inSubCACertificateLen)
{
    OSStatus err;
    X509_STORE* certStore;
    const uint8_t* ptr;
    const uint8_t* end;
    PKCS7* p7 = NULL;
    int type;
    STACK_OF(X509) * certStack;
    X509* cert;
    X509_STORE_CTX* storeContext = NULL;
    const EVP_MD* evpMD;
    EVP_MD_CTX evpContext;
    Boolean evpInitialized = false;
    EVP_PKEY* publicKey;
    ASN1_INTEGER* sn;

    certStore = X509_STORE_new();
    require_action(certStore, exit, err = kUnknownErr);

    // Add the sub-CA certificate.

    ptr = inSubCACertificatePtr;
    end = ptr + inSubCACertificateLen;
    cert = d2i_X509_compat(NULL, &ptr, (int)(end - ptr));
    require_action(cert, exit, err = kUnknownErr);
    require_action(ptr == end, exit, err = kUnknownErr);

    err = (X509_STORE_add_cert(certStore, cert) == 1) ? kNoErr : kUnknownErr;
    X509_free(cert);
    require_noerr(err, exit);

    // Add the root certificate.

    ptr = inRootCertificatePtr;
    end = ptr + inRootCertificateLen;
    cert = d2i_X509_compat(NULL, &ptr, (int)(end - ptr));
    require_action(cert, exit, err = kUnknownErr);
    require_action(ptr == end, exit, err = kUnknownErr);

    err = (X509_STORE_add_cert(certStore, cert) == 1) ? kNoErr : kUnknownErr;
    X509_free(cert);
    require_noerr(err, exit);

    // Verify the certificate.

    ptr = inCertificatePtr;
    end = ptr + inCertificateLen;
    p7 = d2i_PKCS7_compat(NULL, &ptr, (int)(end - ptr));
    require_action(p7, exit, err = kUnknownErr);
    require_action(ptr == end, exit, err = kUnknownErr);

    type = OBJ_obj2nid(p7->type);
    if (type == NID_pkcs7_signed)
        certStack = p7->d.sign->cert;
    else if (type == NID_pkcs7_signedAndEnveloped)
        certStack = p7->d.signed_and_enveloped->cert;
    else {
        dlogassert("bad PKCS7 type %d", type);
        err = kTypeErr;
        goto exit;
    }

    require_action(sk_X509_num(certStack) > 0, exit, err = kCountErr);
    cert = sk_X509_value(certStack, 0);
    require_action(cert, exit, err = kUnknownErr);

    storeContext = X509_STORE_CTX_new();
    require_action(storeContext, exit, err = kUnknownErr);

    err = (X509_STORE_CTX_init(storeContext, certStore, cert, NULL) == 1) ? kNoErr : kUnknownErr;
    require_noerr(err, exit);

    err = (X509_verify_cert(storeContext) == 1) ? kNoErr : kUnknownErr;
    if (err)
        err = X509_STORE_CTX_get_error(storeContext);
    require_noerr_quiet(err, exit);

    // Verify the signature.

    evpMD = (inSignatureLen == kMFiSignatureLengthPreAuthV3) ? EVP_sha1() : EVP_sha256();
    err = (EVP_VerifyInit(&evpContext, evpMD) == 1) ? kNoErr : kUnknownErr;
    require_noerr(err, exit);
    evpInitialized = true;

    err = (EVP_VerifyUpdate(&evpContext, inDataPtr, inDataLen) == 1) ? kNoErr : kUnknownErr;
    require_noerr(err, exit);

    publicKey = X509_get_pubkey(cert);
    require_action(publicKey, exit, err = kUnknownErr);

    err = (EVP_VerifyFinal_compat(&evpContext, inSignaturePtr, (unsigned int)inSignatureLen,
               publicKey)
              == 1)
        ? kNoErr
        : kUnknownErr;
    EVP_PKEY_free(publicKey);
    require_noerr_quiet(err, exit);

exit:
    if (evpInitialized)
        EVP_MD_CTX_cleanup(&evpContext);
    if (storeContext)
        X509_STORE_CTX_free(storeContext);
    if (p7)
        PKCS7_free(p7);
    if (certStore)
        X509_STORE_free(certStore);
    return (err);
}

//===========================================================================================================================
//	_MFiPlatform_VerifySerialNumber
//===========================================================================================================================

static OSStatus _MFiPlatform_VerifySerialNumber(const uint8_t* inSNPtr, size_t inSNLen)
{
    OSStatus err;

    mfic_ulog(kLogLevelInfo, "MFi certificate serial number: %.3H\n", inSNPtr, (int)inSNLen, 128);
    if (MFiIsRevokedCert(inSNPtr, inSNLen)) {
        mfic_ulog(kLogLevelInfo, "### Revoked MFi certificate SN: %.3H\n", inSNPtr, (int)inSNLen, 128);
        err = kAuthenticationErr;
        goto exit;
    }
    if (inSNLen == 16) // AuthV3 serial number...treat as valid until there's a definition of invalid serial numbers.
    {
        err = kNoErr;
        goto exit;
    }
    if ((inSNLen <= 7) || (inSNPtr[7] > 0x0A)) {
        mfic_ulog(kLogLevelInfo, "### Bad MFi certificate SN class: %.3H\n", inSNPtr, (int)inSNLen, 128);
        err = kAuthenticationErr;
        goto exit;
    }
    err = kNoErr;

exit:
    return (err);
}

#if (COMPILER_CLANG)
#pragma clang diagnostic pop
#endif
