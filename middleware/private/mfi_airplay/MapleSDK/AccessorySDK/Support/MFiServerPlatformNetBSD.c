/*
	Copyright (C) 2010-2019 Apple Inc. All Rights Reserved.
*/

#include "MFiSAP.h"

#include <fcntl.h>
#include <unistd.h>

#include <dev/i2c/i2c_io.h>
#include <sys/ioctl.h>

#include "CryptoHashUtils.h"
#include "DebugServices.h"
#include "SHAUtils.h"

//===========================================================================================================================
//	Mozart Constants
//===========================================================================================================================

#define kMozartOverallMaxTries 3 // Number of times to try a logical group of I2C transactions.
#define kMozartIndividualMaxTries 20 // Number of times to try a single I2C transaction.
#define kMozartRetryDelayMics 10000 // 10 ms.

#define kMozartDeviceAddress 0x11 // Should make this configurable to handle RST=0 for 0x10 addresses.

#define kMozartReg_DeviceVersion 0x00
#define kMozartReg_FirmwareVersion 0x01
#define kMozartReg_AuthProtocolMajorVersion 0x02
#define kMozartReg_AuthProtocolMinorVersion 0x03
#define kMozartReg_DeviceID 0x04
#define kMozartReg_ErrorCode 0x05
#define kMozartError_NoError 0x00
#define kMozartError_InvalidRegisterForRead 0x01
#define kMozartError_InvalidRegisterForWrite 0x02
#define kMozartError_InvalidSignatureLength 0x03
#define kMozartError_InvalidChallengeLength 0x04
#define kMozartError_InvalidCertificateLength 0x05
#define kMozartError_SignatureGeneration 0x06
#define kMozartError_ChallengeGeneration 0x07
#define kMozartError_SignatureVerification 0x08
#define kMozartError_CertificateVerification 0x09
#define kMozartError_InvalidProcessControl 0x0A
#define kMozartError_OutOfSequence 0x0B
#define kMozartError_PublicEncryption 0x0C
#define kMozartError_PrivateDecryption 0x0D

#define kMozartReg_AuthControlStatus 0x10
#define kMozartAuthErrSet 0x80

#define kMozartAuthStatusShift 4
#define kMozartAuthStatusMask 0x3
#define kMozartAuthStatus_NotValid 0
#define kMozartAuthStatus_DeviceSignatureGenerated 1
#define kMozartAuthStatus_ChallengeGenerated 2
#define kMozartAuthStatus_HostSignatureVerified 3
#define kMozartAuthStatus_HostCertificateValidated 4
#define kMozartAuthStatus_HostCertificateValidatedOID 5
#define kMozartAuthStatus_PublicEncryptSuccess 6
#define kMozartAuthStatus_PrivateDecryptSuccess 7
#define MozartAuthStatusToOSStatus(X) \
    (((X)&kMozartAuthErrSet) ? (1000 + (((X) >> kMozartAuthStatusShift) & kMozartAuthStatusMask)) : kNoErr)

#define kMozartAuthControlShift 0
#define kMozartAuthControlMask 0xF
#define kMozartAuthControl_NoEffect 0
#define kMozartAuthControl_GenerateSignature 1
#define kMozartAuthControl_GenerateChallenge 2
#define kMozartAuthControl_VerifySignature 3
#define kMozartAuthControl_VerifyCertificate 4
#define kMozartAuthControl_NoOperation 5
#define kMozartAuthControl_PublicEncrypt 6
#define kMozartAuthControl_PrivateDecrypt 7

#define kMozartReg_SignatureSize 0x11
#define kMozartReg_SignatureData 0x12
#define kMozartReg_ChallengeSize 0x20
#define kMozartReg_ChallengeData 0x21
#define kMozartReg_DeviceCertificateSize 0x30
#define kMozartReg_DeviceCertificateData1 0x31
#define kMozartReg_DeviceCertificateData2 0x32
#define kMozartReg_DeviceCertificateData3 0x33
#define kMozartReg_DeviceCertificateData4 0x34
#define kMozartReg_DeviceCertificateData5 0x35
#define kMozartReg_DeviceCertificateData6 0x36
#define kMozartReg_DeviceCertificateData7 0x37
#define kMozartReg_DeviceCertificateData8 0x38
#define kMozartReg_DeviceCertificateData9 0x39
#define kMozartReg_DeviceCertificateData10 0x3A
#define kMozartReg_SelfTestControlStatus 0x40
#define kMozartReg_SleepModeActivationDelayMs 0x41
#define kMozartReg_CurrentLimitationMA 0x42
#define kMozartReg_SecurityEventCounter 0x4D
#define kMozartReg_CertificateSerialNumber 0x4E
#define kMozartReg_CoprocessorUID 0x4F
#define kMozartReg_HostCertificateSize 0x50
#define kMozartReg_HostCertificateData1 0x51
#define kMozartReg_HostCertificateData2 0x52
#define kMozartReg_HostCertificateData3 0x53
#define kMozartReg_HostCertificateData4 0x54
#define kMozartReg_HostCertificateData5 0x55
#define kMozartReg_HostCertificateData6 0x56
#define kMozartReg_HostCertificateData7 0x57
#define kMozartReg_HostCertificateData8 0x58

//===========================================================================================================================
//	Internals
//===========================================================================================================================

DEBUG_STATIC OSStatus Mozart_CopyDeviceCertificate(uint8_t** outCertificatePtr, size_t* outCertificateLen);
DEBUG_STATIC OSStatus
Mozart_CreateSignature(
    const void* inDigestPtr,
    size_t inDigestLen,
    uint8_t** outSignaturePtr,
    size_t* outSignatureLen);
DEBUG_STATIC OSStatus Mozart_SetHostCertificate(const void* inCertPtr, size_t inCertLen);

DEBUG_STATIC OSStatus
__Mozart_DoI2CIOCommand(
    uint8_t inControlCode,
    const void* inSrcPtr,
    size_t inSrcLen,
    void* inDstBuf,
    size_t inDstMaxLen,
    size_t* outDstLen);
DEBUG_STATIC OSStatus
__Mozart_DoI2CCommand(
    int inFD,
    uint8_t inRegister,
    const uint8_t* inWritePtr,
    size_t inWriteLen,
    uint8_t* inReadBuf,
    size_t inReadLen);
DEBUG_STATIC OSStatus
__Mozart_DoI2CTransaction(
    int inFD,
    i2c_op_t inOp,
    const uint8_t* inCmdPtr,
    size_t inCmdLen,
    uint8_t* inBufPtr,
    size_t inBufLen,
    int* outActualTries);

static uint8_t* gMFiCertificatePtr = NULL;
static size_t gMFiCertificateLen = 0;

//===========================================================================================================================
//	MFiPlatform_Initialize
//===========================================================================================================================

OSStatus MFiPlatform_Initialize(void)
{
    OSStatus err;

    // Cache the certificate at startup so users don't have to wait when they try to play to us.

    err = MFiPlatform_CopyCertificate(NULL, NULL);
    check_noerr(err);

    return (kNoErr);
}

//===========================================================================================================================
//	MFiPlatform_Finalize
//===========================================================================================================================

void MFiPlatform_Finalize(void)
{
    ForgetMem(&gMFiCertificatePtr);
    gMFiCertificateLen = 0;
}

//===========================================================================================================================
//	MFiPlatform_CreateSignature
//===========================================================================================================================

OSStatus
MFiPlatform_CreateSignature(
    const void* inDataPtr,
    size_t inDataLen,
    uint8_t** outSignaturePtr,
    size_t* outSignatureLen)
{
    return (MFiPlatform_CreateSignatureEx(kMFiAuthFlagsAlreadyHashed, inDataPtr, inDataLen,
        outSignaturePtr, outSignatureLen));
}

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
    size_t signatureLen;
    uint8_t* signaturePtr = NULL;

    if (inFlags & kMFiAuthFlagsAlreadyHashed) {
        digestLen = inDataLen;
        digestPtr = inDataPtr;
    } else {
        SHA1(inDataPtr, inDataLen, digestBuf);
        digestLen = SHA_DIGEST_LENGTH;
        digestPtr = digestBuf;
    }

    return (Mozart_CreateSignature(digestPtr, digestLen, outSignaturePtr, outSignatureLen));
}

//===========================================================================================================================
//	MFiPlatform_CopyCertificate
//===========================================================================================================================

OSStatus MFiPlatform_CopyCertificate(uint8_t** outCertificatePtr, size_t* outCertificateLen)
{
    OSStatus err;
    uint8_t* certificateBuf;

    // If the certificate isn't cached, get it from the hardware and cache it.

    if (!gMFiCertificatePtr) {
        err = Mozart_CopyDeviceCertificate(&gMFiCertificatePtr, &gMFiCertificateLen);
        require_noerr(err, exit);
    }

    // Copy the cached certificate.

    if (outCertificatePtr) {
        certificateBuf = (uint8_t*)malloc(gMFiCertificateLen);
        require_action(certificateBuf, exit, err = kNoMemoryErr);
        memcpy(certificateBuf, gMFiCertificatePtr, gMFiCertificateLen);

        *outCertificatePtr = certificateBuf;
    }
    if (outCertificateLen) {
        *outCertificateLen = gMFiCertificateLen;
    }
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	MFiPlatform_RSAPublicEncrypt
//===========================================================================================================================

OSStatus
MFiPlatform_RSAPublicEncrypt(
    const void* inSrcPtr,
    size_t inSrcLen,
    void* inDstBuf,
    size_t inDstMaxLen,
    size_t* outDstLen)
{
    Mozart_SetHostCertificate(gMFiCertificatePtr, gMFiCertificateLen);
    return (__Mozart_DoI2CIOCommand(kMozartAuthControl_PublicEncrypt, inSrcPtr, inSrcLen, inDstBuf, inDstMaxLen, outDstLen));
}

//===========================================================================================================================
//	MFiPlatform_RSAPrivateDecrypt
//===========================================================================================================================

OSStatus
MFiPlatform_RSAPrivateDecrypt(
    const void* inSrcPtr,
    size_t inSrcLen,
    void* inDstBuf,
    size_t inDstMaxLen,
    size_t* outDstLen)
{
    return (__Mozart_DoI2CIOCommand(kMozartAuthControl_PrivateDecrypt, inSrcPtr, inSrcLen, inDstBuf, inDstMaxLen, outDstLen));
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	Mozart_CopyDeviceCertificate
//===========================================================================================================================

DEBUG_STATIC OSStatus Mozart_CopyDeviceCertificate(uint8_t** outCertificatePtr, size_t* outCertificateLen)
{
    OSStatus err;
    int fd;
    uint8_t tempBuf[2];
    size_t certificateLen;
    uint8_t* certificatePtr;

    certificatePtr = NULL;

    fd = open("/dev/iic0", O_RDWR);
    err = map_global_value_errno(fd >= 0, fd);
    require_noerr(err, exit);

    err = __Mozart_DoI2CCommand(fd, kMozartReg_DeviceCertificateSize, NULL, 0, tempBuf, 2);
    require_noerr(err, exit);
    certificateLen = (tempBuf[0] << 8) | tempBuf[1];
    require_action(certificateLen > 0, exit, err = kSizeErr);

    certificatePtr = (uint8_t*)malloc(certificateLen + 1);
    require_action(certificatePtr, exit, err = kNoMemoryErr);

    // Note: reads from the data1 register auto-increment to other data registers that follow it.

    err = __Mozart_DoI2CCommand(fd, kMozartReg_DeviceCertificateData1, NULL, 0, certificatePtr, certificateLen);
    require_noerr(err, exit);

    *outCertificatePtr = certificatePtr;
    *outCertificateLen = certificateLen;
    certificatePtr = NULL;

exit:
    if (certificatePtr)
        free(certificatePtr);
    if (fd >= 0)
        close(fd);
    return (err);
}

//===========================================================================================================================
//	Mozart_CreateSignature
//===========================================================================================================================

DEBUG_STATIC OSStatus
Mozart_CreateSignature(
    const void* inDigestPtr,
    size_t inDigestLen,
    uint8_t** outSignaturePtr,
    size_t* outSignatureLen)
{
    OSStatus err;
    int fd;
    uint8_t tempBuf[128];
    size_t signatureLen;
    uint8_t* signaturePtr;

    signaturePtr = NULL;

    fd = open("/dev/iic0", O_RDWR);
    err = map_global_value_errno(fd >= 0, fd);
    require_noerr(err, exit);

    // Write the digest to the device.
    // Note: writes to the size register auto-increment to the data register that follows it.

    require_action(inDigestLen == 20, exit, err = kSizeErr);

    tempBuf[0] = (uint8_t)((inDigestLen >> 8) & 0xFF);
    tempBuf[1] = (uint8_t)(inDigestLen & 0xFF);
    memcpy(&tempBuf[2], inDigestPtr, inDigestLen);
    err = __Mozart_DoI2CCommand(fd, kMozartReg_ChallengeSize, tempBuf, 2 + inDigestLen, NULL, 0);
    require_noerr(err, exit);

    // Generate the signature.

    tempBuf[0] = kMozartAuthControl_GenerateSignature;
    err = __Mozart_DoI2CCommand(fd, kMozartReg_AuthControlStatus, tempBuf, 1, NULL, 0);
    require_noerr(err, exit);

    err = __Mozart_DoI2CCommand(fd, kMozartReg_AuthControlStatus, NULL, 0, tempBuf, 1);
    require_noerr(err, exit);
    err = MozartAuthStatusToOSStatus(tempBuf[0]);
    require_noerr(err, exit);

    // Read the signature.

    err = __Mozart_DoI2CCommand(fd, kMozartReg_SignatureSize, NULL, 0, tempBuf, 2);
    require_noerr(err, exit);
    signatureLen = (tempBuf[0] << 8) | tempBuf[1];
    require_action(signatureLen > 0, exit, err = kSizeErr);

    signaturePtr = (uint8_t*)malloc(signatureLen + 1);
    require_action(signaturePtr, exit, err = kNoMemoryErr);

    err = __Mozart_DoI2CCommand(fd, kMozartReg_SignatureData, NULL, 0, signaturePtr, signatureLen);
    require_noerr(err, exit);

    *outSignaturePtr = signaturePtr;
    *outSignatureLen = signatureLen;
    signaturePtr = NULL;

exit:
    if (signaturePtr)
        free(signaturePtr);
    if (fd >= 0)
        close(fd);
    return (err);
}

//===========================================================================================================================
//	Mozart_SetHostCertificate
//===========================================================================================================================

DEBUG_STATIC OSStatus Mozart_SetHostCertificate(const void* inCertPtr, size_t inCertLen)
{
    OSStatus err;
    int fd;
    uint8_t tempBuf[2];
    const uint8_t* certPtr;
    size_t len;
    uint8_t reg;

    fd = open("/dev/iic0", O_RDWR);
    err = map_global_value_errno(fd >= 0, fd);
    require_noerr(err, exit);

    // Write the digest to the device.
    // Note: writes to the data1 register auto-increment to the data registers that follow it.

    require_action(inCertLen <= 0xFFFF, exit, err = kSizeErr);
    tempBuf[0] = (uint8_t)((inCertLen >> 8) & 0xFF);
    tempBuf[1] = (uint8_t)(inCertLen & 0xFF);
    err = __Mozart_DoI2CCommand(fd, kMozartReg_HostCertificateSize, tempBuf, 2, NULL, 0);
    require_noerr(err, exit);

    reg = kMozartReg_HostCertificateData1;
    for (certPtr = (const uint8_t*)inCertPtr; inCertLen > 0; certPtr += len, inCertLen -= len) {
        len = Min(inCertLen, 128);
        err = __Mozart_DoI2CCommand(fd, reg++, certPtr, len, NULL, 0);
        require_noerr(err, exit);
    }

    // Validate the certificate to cause the device to extract the public key.

    tempBuf[0] = kMozartAuthControl_VerifyCertificate;
    err = __Mozart_DoI2CCommand(fd, kMozartReg_AuthControlStatus, tempBuf, 1, NULL, 0);
    require_noerr(err, exit);

    err = __Mozart_DoI2CCommand(fd, kMozartReg_AuthControlStatus, NULL, 0, tempBuf, 1);
    require_noerr(err, exit);

// Note: we ignore the status code because we expect it to fail.

exit:
    if (fd >= 0)
        close(fd);
    return (err);
}

//===========================================================================================================================
//	__Mozart_DoI2CIOCommand
//===========================================================================================================================

DEBUG_STATIC OSStatus
__Mozart_DoI2CIOCommand(
    uint8_t inControlCode,
    const void* inSrcPtr,
    size_t inSrcLen,
    void* inDstBuf,
    size_t inDstMaxLen,
    size_t* outDstLen)
{
    OSStatus err;
    int fd;
    uint8_t tempBuf[130];
    size_t len;

    fd = open("/dev/iic0", O_RDWR);
    err = map_global_value_errno(fd >= 0, fd);
    require_noerr(err, exit);

    // Write the input data to the device.
    // Note: writes to the size register auto-increment to the data register that follows it.

    require_action(inSrcLen == 128, exit, err = kSizeErr);
    tempBuf[0] = (uint8_t)((inSrcLen >> 8) & 0xFF);
    tempBuf[1] = (uint8_t)(inSrcLen & 0xFF);
    memcpy(&tempBuf[2], inSrcPtr, inSrcLen);
    err = __Mozart_DoI2CCommand(fd, kMozartReg_ChallengeSize, tempBuf, 2 + inSrcLen, NULL, 0);
    require_noerr(err, exit);

    // Process the data.

    err = __Mozart_DoI2CCommand(fd, kMozartReg_AuthControlStatus, &inControlCode, 1, NULL, 0);
    require_noerr(err, exit);

    err = __Mozart_DoI2CCommand(fd, kMozartReg_AuthControlStatus, NULL, 0, tempBuf, 1);
    require_noerr(err, exit);
    err = MozartAuthStatusToOSStatus(tempBuf[0]);
    require_noerr(err, exit);

    // Read the response.

    err = __Mozart_DoI2CCommand(fd, kMozartReg_SignatureSize, NULL, 0, tempBuf, 2);
    require_noerr(err, exit);
    len = (tempBuf[0] << 8) | tempBuf[1];
    require_action((len > 0) && (len <= inDstMaxLen), exit, err = kSizeErr);

    err = __Mozart_DoI2CCommand(fd, kMozartReg_SignatureData, NULL, 0, inDstBuf, len);
    require_noerr(err, exit);

    *outDstLen = len;

exit:
    if (fd >= 0)
        close(fd);
    return (err);
}

//===========================================================================================================================
//	__Mozart_DoI2CCommand
//===========================================================================================================================

DEBUG_STATIC OSStatus
__Mozart_DoI2CCommand(
    int inFD,
    uint8_t inRegister,
    const uint8_t* inWritePtr,
    size_t inWriteLen,
    uint8_t* inReadBuf,
    size_t inReadLen)
{
    OSStatus err;
    int tries, individualTries;

    for (tries = 1; tries <= kMozartOverallMaxTries; ++tries) {
        if (inReadBuf) {
            // Mozart can't handle combined transactions so do a separate write then a read.

            err = __Mozart_DoI2CTransaction(inFD, I2C_OP_WRITE_WITH_STOP, NULL, 0, &inRegister, 1, &individualTries);
            if (err) {
                dlogassert("Set register to 0x%02X failed: %#m", inRegister, err);
                continue;
            }
            if (individualTries > 1)
                dlog(kLogLevelNotice, "*** Mozart took %d tries to set register to 0x%02X\n",
                    individualTries, inRegister);

            err = __Mozart_DoI2CTransaction(inFD, I2C_OP_READ_WITH_STOP, NULL, 0, inReadBuf, inReadLen, &individualTries);
            if (err) {
                dlogassert("Read %zu bytes from register 0x%02X failed: %#m", inReadLen, inRegister, err);
                continue;
            }
            if (individualTries > 1)
                dlog(kLogLevelNotice, "*** Mozart took %d tries to read %zu bytes from register 0x%02X\n",
                    individualTries, inReadLen, inRegister);
        } else {
            err = __Mozart_DoI2CTransaction(inFD, I2C_OP_WRITE_WITH_STOP, &inRegister, 1,
                (uint8_t*)inWritePtr, inWriteLen, &individualTries);
            if (err) {
                dlogassert("Write %zu bytes to register 0x%02X failed: %#m", inWriteLen, inRegister, err);
                continue;
            }
            if (individualTries > 1)
                dlog(kLogLevelNotice, "*** Mozart took %d tries to write %zu bytes to register 0x%02X\n",
                    individualTries, inWriteLen, inRegister);
        }
        break;
    }
    require_noerr(err, exit);

exit:
    return (err);
}

//===========================================================================================================================
//	__Mozart_DoI2CTransaction
//===========================================================================================================================

DEBUG_STATIC OSStatus
__Mozart_DoI2CTransaction(
    int inFD,
    i2c_op_t inOp,
    const uint8_t* inCmdPtr,
    size_t inCmdLen,
    uint8_t* inBufPtr,
    size_t inBufLen,
    int* outActualTries)
{
    OSStatus err;
    int tries;
    i2c_ioctl_exec_t io;

    for (tries = 1; tries <= kMozartIndividualMaxTries; ++tries) {
        io.iie_op = inOp;
        io.iie_addr = kMozartDeviceAddress;
        io.iie_cmd = inCmdPtr;
        io.iie_cmdlen = inCmdLen;
        io.iie_buf = inBufPtr;
        io.iie_buflen = inBufLen;

        err = ioctl(inFD, I2C_IOCTL_EXEC, &io);
        err = map_global_noerr_errno(err);
        if (err) {
            usleep(kMozartRetryDelayMics);
            continue;
        }

        if (outActualTries)
            *outActualTries = tries;
        break;
    }
    return (err);
}
