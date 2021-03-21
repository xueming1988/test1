/*
	Copyright (C) 2015 Apple Inc. All Rights Reserved.
*/

#include <stdlib.h>
#include <string.h>

#include "CommonServices.h"
#include "DebugServices.h"
#include "MFiCertificates.h"
#include "MFiSAP.h"

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
    (void)inDataPtr;
    (void)inDataLen;
    (void)inSignaturePtr;
    (void)inSignatureLen;
    (void)inCertificatePtr;
    (void)inCertificateLen;
    (void)inRootCertificatePtr;
    (void)inRootCertificateLen;
    (void)inSubCACertificatePtr;
    (void)inSubCACertificateLen;

    return (kUnsupportedErr);
}
