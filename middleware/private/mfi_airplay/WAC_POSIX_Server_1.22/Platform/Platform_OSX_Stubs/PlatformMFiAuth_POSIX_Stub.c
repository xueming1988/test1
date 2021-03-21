/*
    File:    PlatformMFiAuth_POSIX_Stub.c
    Package: WACServer
    Version: WACServer-1.14

    Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
    capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
    Apple software is governed by and subject to the terms and conditions of your MFi License,
    including, but not limited to, the restrictions specified in the provision entitled ?ùPublic
    Software?? and is further subject to your agreement to the following additional terms, and your
    agreement that the use, installation, modification or redistribution of this Apple software
    constitutes acceptance of these additional terms. If you do not agree with these additional terms,
    please do not use, install, modify or redistribute this Apple software.

    Subject to all of these terms and in?consideration of your agreement to abide by them, Apple grants
    you, for as long as you are a current and in good-standing MFi Licensee, a personal, non-exclusive
    license, under Apple's copyrights in this original Apple software (the "Apple Software"), to use,
    reproduce, and modify the Apple Software in source form, and to use, reproduce, modify, and
    redistribute the Apple Software, with or without modifications, in binary form. While you may not
    redistribute the Apple Software in source form, should you redistribute the Apple Software in binary
    form, you must retain this notice and the following text and disclaimers in all such redistributions
    of the Apple Software. Neither the name, trademarks, service marks, or logos of Apple Inc. may be
    used to endorse or promote products derived from the Apple Software without specific prior written
    permission from Apple. Except as expressly stated in this notice, no other rights or licenses,
    express or implied, are granted by Apple herein, including but not limited to any patent rights that
    may be infringed by your derivative works or by other works in which the Apple Software may be
    incorporated.

    Unless you explicitly state otherwise, if you provide any ideas, suggestions, recommendations, bug
    fixes or enhancements to Apple in connection with this software (?úFeedback??, you hereby grant to
    Apple a non-exclusive, fully paid-up, perpetual, irrevocable, worldwide license to make, use,
    reproduce, incorporate, modify, display, perform, sell, make or have made derivative works of,
    distribute (directly or indirectly) and sublicense, such Feedback in connection with Apple products
    and services. Providing this Feedback is voluntary, but if you do provide Feedback to Apple, you
    acknowledge and agree that Apple may exercise the license granted above without the payment of
    royalties or further consideration to Participant.

    The Apple Software is provided by Apple on an "AS IS" basis. APPLE MAKES NO WARRANTIES, EXPRESS OR
    IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY
    AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR
    IN COMBINATION WITH YOUR PRODUCTS.

    IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION
    AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
    (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

    Copyright (C) 2013 Apple Inc. All Rights Reserved.
*/

#include "PlatformLogging.h"
#include "PlatformMFiAuth.h"

#include "Debug.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <linux/i2c-dev.h>
#include "SHAUtils.h"

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


#define kMFiAuthRetryDelayMics                  5000 // 5 ms.
#define kMFiAuthReg_ErrorCode                   0x05
#define kMFiAuthReg_AuthControlStatus           0x10
#define kMFiAuthFlagError                       0x80
#define kMFiAuthControl_GenerateSignature       1
#define kMFiAuthReg_SignatureSize               0x11
#define kMFiAuthReg_SignatureData               0x12
#define kMFiAuthReg_ChallengeSize               0x20
#define kMFiAuthReg_ChallengeData               0x21
#define kMFiAuthReg_DeviceCertificateSize       0x30
#define kMFiAuthReg_DeviceCertificateData1      0x31 // Note: auto-increments so next read is Data2, Data3, etc.
#define free_compat( PTR ) free( (PTR) )
#define ForgetCustom( X, DELETER )      do { if( *(X) ) { DELETER( *(X) ); *(X) = NULL; } } while( 0 )
#define ForgetMem( X ) ForgetCustom( X, free_compat )
OSStatus PlatformMFiAuthCopyCertificate(uint8_t **outCertificatePtr, size_t *outCertificateLen);
int MFiisFileExist(const char *file_name);
int MFigetNumberFromFile(const char *Filename);
extern uint64_t    UpTicks(void);
uint64_t    SecondsToUpTicks(uint64_t x);
extern uint64_t    UpTicksPerSecond(void);
//===========================================================================================================================
//  Prototypes
//===========================================================================================================================

static OSStatus _DoI2C(int inFD, uint8_t inRegister, const uint8_t     *inWritePtr, size_t inWriteLen,
                       uint8_t *inReadBuf, size_t   inReadLen);
extern int mfi_i2c_lock(void);
extern void mfi_i2c_unlock(void);

//---------------------------------------------------------------------------------------------------------------------------
/*! @brief		Flags for controlling MFi auth behavior.
*/
typedef uint32_t MFiAuthFlags;
#define kMFiAuthFlagsNone 0 // No flags.
#define kMFiAuthFlagsAlreadyHashed (1U << 0) // Data is already hashed so don't hash it again.


extern OSStatus MFiPlatform_CreateSignatureEx(
    MFiAuthFlags inFlags,
    const void* inDataPtr,
    size_t inDataLen,
    uint8_t** outSignaturePtr,
    size_t* outSignatureLen);


//===========================================================================================================================
//  Globals
//===========================================================================================================================

uint8_t        *gMFiCertificatePtr  = NULL;
size_t          gMFiCertificateLen  = 0;

OSStatus PlatformMFiAuthInitialize(void)
{
    PlatformMFiAuthCopyCertificate(&gMFiCertificatePtr, &gMFiCertificateLen);
    return kNoErr;
}

void PlatformMFiAuthFinalize(void)
{
    ForgetMem(&gMFiCertificatePtr);
    gMFiCertificateLen = 0;
    return;
}
#define kMFiAuthRegisterProtocolMajorVersion 0x02

#define kLogLevelUninitialized -1 //! PRIVATE: only used internally for tracking LogCategory state.
#define kLogLevelMask 0x000000FF
#define kLogLevelAll 0 //! Intended for setting as a level to enable all logging. Not for passing to ulog.
#define kLogLevelMin 1 //! Intended for passing to ulog as the lowest level. Not for setting as a level.
#define kLogLevelChatty 10
#define kLogLevelVerbose 20 //! Similar to LOG_DEBUG
#define kLogLevelTrace 30
#define kLogLevelInfo 40 //! Similar to LOG_INFO
#define kLogLevelNotice 50 //! Similar to LOG_NOTICE
#define kLogLevelWarning 60 //! Similar to LOG_WARNING
#define kLogLevelAssert 70
#define kLogLevelRequire 80
#define kLogLevelError 90 //! Similar to LOG_ERR
#define kLogLevelCritical 100 //! Similar to LOG_CRIT
#define kLogLevelAlert 110 //! Similar to LOG_ALERT
#define kLogLevelEmergency 120 //! Similar to LOG_EMERG
#define kLogLevelTragic 130
#define kLogLevelMax 0x000000FE //! Intended for passing to ulog. Not for setting as a level.
#define kLogLevelOff 0x000000FF //! Only for use when setting a log level. Not for passing to ulog.

#define kLogLevelFlagMask 0xFFFF0000
#define kLogLevelFlagStackTrace 0x00010000 //! Print a stack trace.
#define kLogLevelFlagDebugBreak 0x00020000 //! Break into a debugger (if a debugger is present, ignored otherwise).
#define kLogLevelFlagForceConsole 0x00040000 //! Open /dev/console, print, and close each time to force output.
#define kLogLevelFlagContinuation 0x00080000 //! Use with ulog. Indicates it's part of a previous ulog call.
#define kLogLevelFlagFunction 0x00100000 //! Use with ulog. Prints the name of the function calling ulog.
#define kLogLevelFlagCrashReport 0x00200000 //! Use with ulog. Forces a crash report with stackshot, etc.
#define kLogLevelFlagDontRateLimit 0x00400000 //! Use with ulog. Bypasses normal rate limiting.
#define kLogLevelFlagSensitive 0x00800000 //! Use with ulog. Only logs if safe (i.e. os_log_sensitive).


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

#define dlog(LEVEL, ...)                                                \
do {                                                                \
    if (unlikely(((LEVEL)&kLogLevelMask) >= kLogLevelInfo)) { \
        printf(__VA_ARGS__);                                        \
    }                                                               \
                                                                    \
} while (0)


#define TARGET_HAVE_FDREF 1

typedef int FDRef;

//#define IsValidFD(X) ((X) >= 0)
#define kInvalidFD -1
#define CloseFD(X) close(X)
#define ReadFD(FD, PTR, LEN) read(FD, PTR, LEN)
#define WriteFD(FD, PTR, LEN) write(FD, PTR, LEN)

#if (DEBUG)
#define check_noerr(ERR)                                                               \
    do {                                                                               \
        OSStatus localErr;                                                             \
                                                                                       \
        localErr = (OSStatus)(ERR);                                                    \
        if (unlikely(localErr != 0)) {                                                 \
            debug_print_assert(localErr, NULL, NULL, __FILE__, __LINE__, __ROUTINE__); \
        }                                                                              \
                                                                                       \
    } while (0)
#else
#define check_noerr(ERR) (void)(ERR)
#endif

#define ForgetFD(X)                                            \
    do {                                                       \
        if (IsValidFD(*(X))) {                                 \
            OSStatus ForgetFDErr;                              \
                                                               \
            ForgetFDErr = CloseFD(*(X));                       \
            ForgetFDErr = map_global_noerr_errno(ForgetFDErr); \
            check_noerr(ForgetFDErr);                          \
            *(X) = kInvalidFD;                                 \
        }                                                      \
                                                               \
    } while (0)



//===========================================================================================================================
static int _UseAuth3(int inFD)
{
    static int gUseAuth3 = -1;

    if (gUseAuth3 == -1) {
        uint8_t buf[1];
        OSStatus err = _DoI2C(inFD, kMFiAuthRegisterProtocolMajorVersion, NULL, 0, buf, 1);
        if (err == kNoErr) {
            gUseAuth3 = (buf[0] == 0x03 ? 1 : 0);
            dlog(kLogLevelError, "Using MFi Auth v.%d chip\n", buf[0]);
        }
    }

    return gUseAuth3;
}


//===========================================================================================================================
OSStatus MFiPlatform_CreateSignatureEx(
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

    dlog(kLogLevelVerbose, "MFi auth create signature\n");

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

 //   dlog(kLogLevelVerbose, "MFi auth created signature:\n%.2H\n", signaturePtr, (int)signatureLen, (int)signatureLen);
    *outSignaturePtr = signaturePtr;
    *outSignatureLen = signatureLen;

    signaturePtr = NULL;
exit:
    ForgetMem(&signaturePtr);
    ForgetFD(&fd);

    if (err)
      //  dlog(kLogLevelWarning, "### MFi auth create signature failed: %#m\n", err);
	printf("### MFi auth create signature failed: %d\n", err);

    return (err);
}

//===========================================================================================================================
OSStatus
PlatformMFiAuthCreateSignature(
	const void* inDigestPtr,
	size_t inDigestLen,
	uint8_t** outSignaturePtr,
	size_t* outSignatureLen)
{
	return (MFiPlatform_CreateSignatureEx(kMFiAuthFlagsAlreadyHashed, inDigestPtr, inDigestLen,
		outSignaturePtr, outSignatureLen));
}

#if 0
OSStatus PlatformMFiAuthCreateSignature(const void *inDigestPtr, size_t     inDigestLen,  uint8_t    **outSignaturePtr,
                                        size_t     *outSignatureLen)
{
    OSStatus        err = kNoErr;
    size_t          signatureLen;
    uint8_t        *signaturePtr;
    int                 fd;
    uint8_t         buf[ 32 ];


    //printf("MFi auth create signature\n" );

#if defined(MFI_AIRPLAY_MODULE_ENABLE)
    mfi_i2c_lock();
#endif

    fd = open(kMFiAuthDevicePath, O_RDWR);
    err = map_fd_creation_errno(fd);
    require_noerr(err, exit);
    /*
        (void) inDigestPtr;
        (void) inDigestLen;
        uint8_t * testPtr = (uint8_t *) inDigestPtr;
    */

    err = ioctl(fd, I2C_SLAVE, kMFiAuthDeviceAddress);
    err = map_global_noerr_errno(err);
    require_noerr(err, exit);


    // Write the data to sign.
    // Note: writes to the size register auto-increment to the data register that follows it.



    require_action(inDigestLen == 20, exit, err = kSizeErr);
    buf[ 0 ] = (uint8_t)((inDigestLen >> 8) & 0xFF);
    buf[ 1 ] = (uint8_t)(inDigestLen        & 0xFF);



    memcpy(&buf[ 2 ], inDigestPtr, inDigestLen);

    err = _DoI2C(fd, kMFiAuthReg_ChallengeSize, buf, 2 + inDigestLen, NULL, 0);
    require_noerr(err, exit);
    /*
    err = _DoI2C( fd, kMFiAuthReg_ErrorCode, NULL, 0, buf, 1 );
    printf("kMFiAuthReg_Challengedata error code 0x%x \n", buf[ 0 ] );
    */
    // Generate the signature.

    buf[ 0 ] = kMFiAuthControl_GenerateSignature;
    err = _DoI2C(fd, kMFiAuthReg_AuthControlStatus, buf, 1, NULL, 0);
    require_noerr(err, exit);

    /*
        err = _DoI2C( fd, kMFiAuthReg_ErrorCode, NULL, 0, buf, 1 );
    printf("write kMFiAuthReg_AuthControlStatus error code 0x%x \n", buf[ 0 ]   );
    */

    err = _DoI2C(fd, kMFiAuthReg_AuthControlStatus, NULL, 0, buf, 1);
    require_noerr(err, exit);
    require_action(!(buf[ 0 ] & kMFiAuthFlagError), exit, err = kUnknownErr);
    /*
        err = _DoI2C( fd, kMFiAuthReg_ErrorCode, NULL, 0, buf, 1 );
    printf("read kMFiAuthReg_AuthControlStatus error code 0x%x \n", buf[ 0 ]    );
    */


    // Read the signature.

    err = _DoI2C(fd, kMFiAuthReg_SignatureSize, NULL, 0, buf, 2);
    require_noerr(err, exit);
    signatureLen = (buf[ 0 ] << 8) | buf[ 1 ];
    require_action(signatureLen > 0, exit, err = kSizeErr);

    signaturePtr = (uint8_t *) malloc(signatureLen);
    require_action(signaturePtr, exit, err = kNoMemoryErr);

    err = _DoI2C(fd, kMFiAuthReg_SignatureData, NULL, 0, signaturePtr, signatureLen);
    if(err) free(signaturePtr);
    require_noerr(err, exit);

    printf("MFi auth created signature:%p, len=%d\n", signaturePtr, (int) signatureLen);
    *outSignaturePtr = signaturePtr;
    *outSignatureLen = signatureLen;

exit:
    if(fd >= 0)    close(fd);
    if(err)        printf("### MFi auth create signature failed: %d\n", err);

#if defined(MFI_AIRPLAY_MODULE_ENABLE)
    mfi_i2c_unlock();
#endif

    return(err);
}
#endif

OSStatus PlatformMFiAuthCopyCertificate(uint8_t **outCertificatePtr, size_t *outCertificateLen)
{
    OSStatus        err;
    size_t          certificateLen;
    uint8_t        *certificatePtr;
    int             fd = -1;
    uint8_t         buf[ 4 ];
    //int i = 0;
    printf("MFi auth copy certificate gMFiCertificateLen=%d\n", gMFiCertificateLen);

#if defined(MFI_AIRPLAY_MODULE_ENABLE)
    mfi_i2c_lock();
#endif

    /* If the certificate has already been cached then return that as an optimization since it doesn't change.*/

    if(gMFiCertificateLen > 0) {
        certificatePtr = (uint8_t *) malloc(gMFiCertificateLen);
        require_action(certificatePtr, exit, err = kNoMemoryErr);
        memcpy(certificatePtr, gMFiCertificatePtr, gMFiCertificateLen);

        *outCertificatePtr = certificatePtr;
        *outCertificateLen = gMFiCertificateLen;
        err = kNoErr;
        goto exit;
    }

#if 0
    do {
        fd = open(kMFiAuthDevicePath, O_RDWR);
        if(fd < 0) {
            system("mknod /dev/cp c 250 0");
            printf("!!!!open %s failed, retry %d\n", kMFiAuthDevicePath, i);
            sleep(1);
        } else
            break;
    } while(i++ < 10);

    printf("11 fd=%d, kMFiAuthDevicePath=%s\n", fd, kMFiAuthDevicePath);
    err = map_fd_creation_errno(fd);
    require_noerr(err, exit);
    printf("fd=%d, kMFiAuthDevicePath=%s\n", fd, kMFiAuthDevicePath);
#endif

    fd = open(kMFiAuthDevicePath, O_RDWR);
    err = map_fd_creation_errno(fd);
    require_noerr(err, exit);

    err = ioctl(fd, I2C_SLAVE, kMFiAuthDeviceAddress);
    err = map_global_noerr_errno(err);
    require_noerr(err, exit);

#if 0 // by liaods for test
    for(i = 0; i < 0x3; ++i) {
        _DoI2C(fd, i, NULL, 0, buf, 1);
        printf("i=%x, val=0x%.2x\n", i, buf[0]);
    }
    i = 4;
    _DoI2C(fd, i, NULL, 0, buf, 4);
    printf("i=%x, val=0x%.8x\n", i, *(unsigned int *)&buf[0]);

    i = 5;
    _DoI2C(fd, i, NULL, 0, buf, 1);
    printf("i=%x, val=0x%.2x\n", i, buf[0]);

    i = 0x10;
    _DoI2C(fd, i, NULL, 0, buf, 1);
    printf("i=%x, val=0x%.2x\n", i, buf[0]);

    i = 0x11;
    _DoI2C(fd, i, NULL, 0, buf, 2);
    printf("i=%x, val=%d\n", i, ((buf[0] << 8) | buf[1]));

    i = 0x20;
    _DoI2C(fd, i, NULL, 0, buf, 2);
    printf("i=%x, val=%d\n", i, ((buf[0] << 8) | buf[1]));

    i = 0x30;
    _DoI2C(fd, i, NULL, 0, buf, 2);
    printf("i=%x, val=%d\n", i, ((buf[0] << 8) | buf[1]));
#endif

    err = _DoI2C(fd, kMFiAuthReg_DeviceCertificateSize, NULL, 0, buf, 2);
    require_noerr(err, exit);

    certificateLen = (buf[ 0 ] << 8) | buf[ 1 ];
    printf("certificateLen=%d\n", certificateLen);
    require_action(certificateLen > 0, exit, err = kSizeErr);

    certificatePtr = (uint8_t *) malloc(certificateLen);
    require_action(certificatePtr, exit, err = kNoMemoryErr);
    // Note: reads from the data1 register auto-increment to data2, data3, etc. registers that follow it.
    err = _DoI2C(fd, kMFiAuthReg_DeviceCertificateData1, NULL, 0, certificatePtr, certificateLen);
    if(err) free(certificatePtr);
    require_noerr(err, exit);


    printf("MFi auth copy certificate done: %d bytes\n", certificateLen);
    *outCertificatePtr = certificatePtr;
    *outCertificateLen = certificateLen;

exit:
    if(fd >= 0)    close(fd);
    if(err)        printf("### MFi auth copy certificate failed: %d\n", err);

#if defined(MFI_AIRPLAY_MODULE_ENABLE)
    mfi_i2c_unlock();
#endif

    return(err);
}

//===========================================================================================================================
//  _DoI2C
//===========================================================================================================================

uint64_t    SecondsToUpTicks(uint64_t x)
{
    static uint64_t     sMultiplier = 0;

    if(sMultiplier == 0) sMultiplier = UpTicksPerSecond();
    return(x * sMultiplier);
}

static OSStatus
_DoI2C(
    int             inFD,
    uint8_t         inRegister,
    const uint8_t  *inWritePtr,
    size_t          inWriteLen,
    uint8_t        *inReadBuf,
    size_t          inReadLen)
{
    OSStatus        err;
    uint64_t        deadline;
    uint64_t        wait_time = 1ULL;
    int             tries;
    ssize_t         n;
    uint8_t         buf[ 1 + inWriteLen ];
    size_t          len;

    deadline = UpTicks() + SecondsToUpTicks(wait_time);
    if(inReadBuf) {
        // Combined mode transactions are not supported so set the register and do a separate read.

        for(tries = 1; ; ++tries) {
            n = write(inFD, &inRegister, 1);
            err = map_global_value_errno(n == 1, n);

            if(!err) break;

            //printf("### MFi auth set register 0x%02X failed (try %d): %d, n=%d\n", inRegister, tries, err, n);
            usleep(kMFiAuthRetryDelayMics);
            require_action(UpTicks() < deadline, exit, err = kUnknownErr);
        }
        for(tries = 1; ; ++tries) {

            n = read(inFD, inReadBuf, inReadLen);
            err = map_global_value_errno(n == ((ssize_t) inReadLen), n);

            if(!err) break;
            printf("### MFi auth read register 0x%02X, %zu bytes failed (try %d): %d, n=%d\n",
                   inRegister, inReadLen, tries, err, n);

            usleep(kMFiAuthRetryDelayMics);
            require_action(UpTicks() < deadline, exit, err = kUnknownErr);
        }
    } else {
        // Gather the register and data so it can be done as a single write transaction.

        buf[ 0 ] = inRegister;
        memcpy(&buf[ 1 ], inWritePtr, inWriteLen);
        len = 1 + inWriteLen;

        for(tries = 1; ; ++tries) {
            n = write(inFD, buf, len);  //buf: 0->register 1,2->inDigestLen 3...~->inDigestData
            err = map_global_value_errno(n == ((ssize_t) len), n);
            if(!err) break;

            printf("### MFi auth write register 0x%02X, %zu bytes failed (try %d): %d\n",
                   inRegister, inWriteLen, tries, err);
            usleep(kMFiAuthRetryDelayMics);
            require_action(UpTicks() < deadline, exit, err = kUnknownErr);
        }
    }

exit:
    if(err) printf("### MFi auth register 0x%02X failed after %d tries: %d\n", inRegister, tries, err);
    return(err);
}


