/*
	Copyright (C) 2013-2019 Apple Inc. All Rights Reserved.
	
	Linux platform plugin for MFi-SAP authentication/encryption.
	
	This defaults to using i2c bus /dev/i2c-1 with an MFi auth IC device address of 0x11 (RST pulled high).
	These can be overridden in the makefile with the following:
	
	CFLAGS += -DMFI_AUTH_DEVICE_PATH=\"/dev/i2c-0\"		# MFi auth IC on i2c bus /dev/i2c-0.
	CFLAGS += -DMFI_AUTH_DEVICE_ADDRESS=0x10			# MFi auth IC at address 0x10 (RST pulled low).
*/
#include "MFiSAP.h"

#include <fcntl.h>
#include <linux/i2c-dev.h>
//#include <linux/i2c.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include "CommonServices.h"
#include "CryptoHashUtils.h"
#include "DebugServices.h"
#include "SHAUtils.h"
#include "TickUtils.h"

//===========================================================================================================================

#if (defined(MFI_AUTH_DEVICE_PATH))
#define kMFiAuthDevicePath MFI_AUTH_DEVICE_PATH
#else
#define kMFiAuthDevicePath "/dev/i2c-1"
#endif

#if (defined(MFI_AUTH_DEVICE_ADDRESS))
#define kMFiAuthDeviceAddress MFI_AUTH_DEVICE_ADDRESS
#else
#define kMFiAuthDeviceAddress 0x11
#endif

#define kMFiAuthRetryDelayMics 5000 // 5 ms.

#define kMFiAuthRegisterProtocolMajorVersion 0x02
#define kMFiAuthRegisterControl 0x10
#define kMFiAuthRegisterChallengeResponseDataLength 0x11
#define kMFiAuthRegisterChallengeResponseData 0x12
#define kMFiAuthRegisterChallengeDataLength 0x20
#define kMFiAuthRegisterChallengeData 0x21
#define kMFiAuthRegisterAccessoryCertificateDataLength 0x30
#define kMFiAuthRegisterAccessoryCertificateData 0x31

enum {
    kMFiControlValueNone = 0x0,
    kMFiControlValueGenerateChallangeResponse = 0x1
};

ulog_define(MFIAuth, kLogLevelNotice, kLogFlags_Default, "MFIAuth", NULL);
#define mfiauth_ucat() &log_category_from_name(MFIAuth)
#define mfiauth_ulog(LEVEL, ...) ulog(mfiauth_ucat(), (LEVEL), __VA_ARGS__)
#define mfiauth_dlog(LEVEL, ...) dlogc(mfiauth_ucat(), (LEVEL), __VA_ARGS__)

//===========================================================================================================================
static int _UseAuth3(int inFD);
static OSStatus
_DoI2C(
    int inFD,
    uint8_t inRegister,
    const uint8_t* inWritePtr,
    size_t inWriteLen,
    uint8_t* inReadBuf,
    size_t inReadLen);

//===========================================================================================================================
static uint8_t* gMFiCertificatePtr = NULL;
static size_t gMFiCertificateLen = 0;

//===========================================================================================================================
OSStatus MFiPlatform_Initialize(void)
{
    // Cache the certificate at startup since the certificate doesn't change and this saves ~200 ms each time.

    MFiPlatform_CopyCertificate(&gMFiCertificatePtr, &gMFiCertificateLen);
    return (kNoErr);
}

//===========================================================================================================================
void MFiPlatform_Finalize(void)
{
    ForgetMem(&gMFiCertificatePtr);
    gMFiCertificateLen = 0;
}

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
    uint8_t digestBuf[128];
    const void* digestPtr;
    size_t digestLen;
    int fd = -1;
    uint8_t buf[2 + 32];
    size_t signatureLen;
    uint8_t* signaturePtr = NULL;

    mfiauth_ulog(kLogLevelWarning, "MFi auth create signature, dev [%s] addr [0x%x]\n",kMFiAuthDevicePath,kMFiAuthDeviceAddress);
    
    // [1] open the device with read/write permissions
    fd = open(kMFiAuthDevicePath, O_RDWR);
    err = map_fd_creation_errno(fd);
    require_noerr(err, exit);

    // [2] Put the device in slave mode
    err = ioctl(fd, I2C_SLAVE, kMFiAuthDeviceAddress);
    err = map_global_noerr_errno(err);
    require_noerr(err, exit);

    // [3] generate the Hash if needed depending on Auth version
    if (inFlags & kMFiAuthFlagsAlreadyHashed) {
        digestLen = inDataLen;
        digestPtr = inDataPtr;
    } else if (_UseAuth3(fd)) {
        SHA256(inDataPtr, inDataLen, digestBuf);
        digestLen = SHA256_DIGEST_LENGTH;
        digestPtr = digestBuf;
    } else {
        SHA1(inDataPtr, inDataLen, digestBuf);
        digestLen = SHA_DIGEST_LENGTH;
        digestPtr = digestBuf;
    }

    // [4] Write the data to sign. Please note the difference between Auth 3 vs. 2
    if (_UseAuth3(fd)) {
        // Note: Auth 3.0 does not need the number of bytes in the buffer
        err = _DoI2C(fd, kMFiAuthRegisterChallengeData, digestPtr, digestLen, NULL, 0);
        require_noerr(err, exit);
    } else {
        // Note: writes to the size register auto-increment to the data register that follows it in Auth 2.0
        buf[0] = (uint8_t)((digestLen >> 8) & 0xFF);
        buf[1] = (uint8_t)(digestLen & 0xFF);
        memcpy(&buf[2], digestPtr, digestLen);

        err = _DoI2C(fd, kMFiAuthRegisterChallengeDataLength, buf, 2 + digestLen, NULL, 0);
        require_noerr(err, exit);
    }

    // [5]    When written to Authentication Control and Status register controls the start
    //        of the Auth 3.0 CP processes
    //        0x1 mens 'Start new challenge' response generation process.
    buf[0] = kMFiControlValueGenerateChallangeResponse;
    err = _DoI2C(fd, kMFiAuthRegisterControl, buf, 1, NULL, 0);
    require_noerr(err, exit);

    // [6]    The Challenge Response Data Length register holds the length in bytes of the
    //        results of the most recent challenge response generation process. If a
    //        completed challenge response is stored in the output buffer of the Auth 3.0 CP,
    //        this register will return a value of 64. In all other cases it will return a
    //        value of zero.
    err = _DoI2C(fd, kMFiAuthRegisterChallengeResponseDataLength, NULL, 0, buf, 2);
    signatureLen = (buf[0] << 8) | buf[1];
    require_action(signatureLen > 0, exit, err = kSizeErr);

    // [7]    Allocate the memory to read the data into.
    signaturePtr = (uint8_t*)malloc(signatureLen);
    require_action(signaturePtr, exit, err = kNoMemoryErr);

    // [8]    If the Auth 3.0 CP has computed a challenge response, the challenge response
    //        itself will be returned in this register
    err = _DoI2C(fd, kMFiAuthRegisterChallengeResponseData, NULL, 0, signaturePtr, signatureLen);
    require_noerr(err, exit);

    //mfiauth_ulog(kLogLevelWarning, "MFi auth created signature:\n%.2H\n", signaturePtr, (int)signatureLen, (int)signatureLen);
    mfiauth_ulog(kLogLevelWarning, "MFi auth created signature ok\n");
    
    *outSignaturePtr = signaturePtr;
    *outSignatureLen = signatureLen;

    signaturePtr = NULL;
exit:
    ForgetMem(&signaturePtr);
    ForgetFD(&fd);

    if (err)
        mfiauth_ulog(kLogLevelWarning, "### MFi auth create signature failed: %#m\n", err);

    return (err);
}

//===========================================================================================================================
OSStatus MFiPlatform_CopyCertificate(uint8_t** outCertificatePtr, size_t* outCertificateLen)
{
    OSStatus err;
    size_t certificateLen;
    uint8_t* certificatePtr;
    int fd = -1;
    uint8_t buf[2];

    mfiauth_ulog(kLogLevelTrace, "MFi auth copy certificate, dev [%s] addr [0x%x]\n",kMFiAuthDevicePath,kMFiAuthDeviceAddress);

    // If the certificate has already been cached then return that as an optimization since it doesn't change.

    if (gMFiCertificateLen > 0) {
        certificatePtr = (uint8_t*)malloc(gMFiCertificateLen);
        require_action(certificatePtr, exit, err = kNoMemoryErr);
        memcpy(certificatePtr, gMFiCertificatePtr, gMFiCertificateLen);

        *outCertificatePtr = certificatePtr;
        *outCertificateLen = gMFiCertificateLen;
        err = kNoErr;
        mfiauth_ulog(kLogLevelTrace, "[%s:%d] MFi auth copy certificate ok\n", __func__, __LINE__);
        goto exit;
    }

    fd = open(kMFiAuthDevicePath, O_RDWR);
    err = map_fd_creation_errno(fd);
    require_noerr(err, exit);

    err = ioctl(fd, I2C_SLAVE, kMFiAuthDeviceAddress);
    err = map_global_noerr_errno(err);
    require_noerr(err, exit);

    err = _DoI2C(fd, kMFiAuthRegisterAccessoryCertificateDataLength, NULL, 0, buf, 2);
    require_noerr(err, exit);
    certificateLen = (buf[0] << 8) | buf[1];
    require_action(certificateLen > 0, exit, err = kSizeErr);

    certificatePtr = (uint8_t*)malloc(certificateLen);
    require_action(certificatePtr, exit, err = kNoMemoryErr);

    // Note: reads from the data1 register auto-increment to data2, data3, etc. registers that follow it.

    err = _DoI2C(fd, kMFiAuthRegisterAccessoryCertificateData, NULL, 0, certificatePtr, certificateLen);
    if (err)
        free(certificatePtr);
    require_noerr(err, exit);

    //mfiauth_ulog(kLogLevelVerbose, "MFi auth copy certificate done:\n%.2H\n", certificatePtr, (int)certificateLen, (int)certificateLen);
    mfiauth_ulog(kLogLevelTrace, "[%s:%d] MFi auth copy certificate ok\n", __func__, __LINE__);
    
    *outCertificatePtr = certificatePtr;
    *outCertificateLen = certificateLen;

exit:
    if (fd >= 0)
        close(fd);
    if (err)
    {
        mfiauth_ulog(kLogLevelWarning, "### MFi auth copy certificate failed: %#m\n", err);
    }
    
    return (err);
}

//===========================================================================================================================
static int _UseAuth3(int inFD)
{
    static int gUseAuth3 = -1;

    if (gUseAuth3 == -1) {
        uint8_t buf[1];
        OSStatus err = _DoI2C(inFD, kMFiAuthRegisterProtocolMajorVersion, NULL, 0, buf, 1);
        if (err == kNoErr) {
            gUseAuth3 = (buf[0] == 0x03 ? 1 : 0);
            mfiauth_ulog(kLogLevelWarning, "Using MFi Auth v.%d chip\n", buf[0]);
        }
    }

    return gUseAuth3;
}

static OSStatus
_DoI2C(
    int inFD,
    uint8_t inRegister,
    const uint8_t* inWritePtr,
    size_t inWriteLen,
    uint8_t* inReadBuf,
    size_t inReadLen)
{
    OSStatus err;
    uint64_t deadline;
    int tries;
    ssize_t n;
    uint8_t buf[1 + inWriteLen];
    size_t len;

    deadline = UpTicks() + SecondsToUpTicks(2);
    if (inReadBuf) {
        // Combined mode transactions are not supported so set the register and do a separate read.

        for (tries = 1;; ++tries) {
            n = write(inFD, &inRegister, 1);
            err = map_global_value_errno(n == 1, n);
            if (!err)
                break;

            mfiauth_ulog(kLogLevelVerbose, "### MFi auth set register 0x%02X failed (try %d): %#m\n", inRegister, tries, err);
            usleep(kMFiAuthRetryDelayMics);
            require_action(UpTicks() < deadline, exit, err = kTimeoutErr);
        }
        for (tries = 1;; ++tries) {
            n = read(inFD, inReadBuf, inReadLen);
            err = map_global_value_errno(n == ((ssize_t)inReadLen), n);
            if (!err)
                break;

            mfiauth_ulog(kLogLevelVerbose, "### MFi auth read register 0x%02X, %zu bytes failed (try %d): %#m\n",
                inRegister, inReadLen, tries, err);
            usleep(kMFiAuthRetryDelayMics);
            require_action(UpTicks() < deadline, exit, err = kTimeoutErr);
        }
    } else {
        // Gather the register and data so it can be done as a single write transaction.

        buf[0] = inRegister;
        memcpy(&buf[1], inWritePtr, inWriteLen);
        len = 1 + inWriteLen;

        for (tries = 1;; ++tries) {
            n = write(inFD, buf, len);
            err = map_global_value_errno(n == ((ssize_t)len), n);
            if (!err)
                break;

            mfiauth_ulog(kLogLevelVerbose, "### MFi auth write register 0x%02X, %zu bytes failed (try %d): %#m\n",
                inRegister, inWriteLen, tries, err);
            usleep(kMFiAuthRetryDelayMics);
            require_action(UpTicks() < deadline, exit, err = kTimeoutErr);
        }
    }

exit:
    if (err)
        mfiauth_ulog(kLogLevelWarning, "### MFi auth register 0x%02X failed after %d tries: %#m\n", inRegister, tries, err);
    return (err);
}
