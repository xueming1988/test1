/*
    File:    PlatformMFiServerNetBSD.c
    Package: AirPlayAudioPOSIXReceiver
    Version: AirPlay_Audio_POSIX_Receiver_211.1.p8
    
    Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
    capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
    Apple software is governed by and subject to the terms and conditions of your MFi License,
    including, but not limited to, the restrictions specified in the provision entitled ”Public
    Software”, and is further subject to your agreement to the following additional terms, and your
    agreement that the use, installation, modification or redistribution of this Apple software
    constitutes acceptance of these additional terms. If you do not agree with these additional terms,
    please do not use, install, modify or redistribute this Apple software.
    
    Subject to all of these terms and in consideration of your agreement to abide by them, Apple grants
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
    fixes or enhancements to Apple in connection with this software (“Feedback”), you hereby grant to
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
    
    Copyright (C) 2012-2013 Apple Inc. All Rights Reserved.

    This defaults to using i2c device /dev/iic1 with an MFi auth IC device address of 0x11.
    These can be overridden in the makefile with the following:
    
    CFLAGS += -DMFI_AUTH_DEVICE_PATH=\"/dev/iic0\"      # MFi auth IC on i2c bus /dev/iic0.
    CFLAGS += -DMFI_AUTH_DEVICE_ADDRESS=0x10            # MFi auth IC at address 0x10.

*/

#include "MFiSAP.h"
#include "CommonServices.h"
#include "TickUtils.h"

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <dev/i2c/i2c_io.h>


//===========================================================================================================================
//  Constants
//===========================================================================================================================

#if( defined( MFI_AUTH_DEVICE_PATH ) )
    #define kMFiAuthDevicePath                  MFI_AUTH_DEVICE_PATH
#else
    #define kMFiAuthDevicePath                  "/dev/iic1"
#endif

#if( defined( MFI_AUTH_DEVICE_ADDRESS ) )
    #define kMFiAuthDeviceAddress               MFI_AUTH_DEVICE_ADDRESS
#else
    #define kMFiAuthDeviceAddress               0x11
#endif

#define kMFiAuthRetryDelayMics                  5000 // 5 ms.
#define kMFiAuthChallengeLen                    20


#define kMFiAuthReg_Error                       0x05
#define kMFiAuthReg_AuthControlStatus           0x10
    #define kMFiAuthFlagError                       0x80
    #define kMFiAuthControl_GenerateSignature       1
#define kMFiAuthReg_SignatureSize               0x11
#define kMFiAuthReg_SignatureData               0x12
#define kMFiAuthReg_ChallengeSize               0x20
#define kMFiAuthReg_ChallengeData               0x21
#define kMFiAuthReg_DeviceCertificateSize       0x30
#define kMFiAuthReg_DeviceCertificateData1      0x31 // Note: auto-increments so next read is Data2, Data3, etc.


//===========================================================================================================================
//  Macros
//===========================================================================================================================

/*
 * NetBSD i2c driver has the following bug which prevents it from detecting write error/NAK.  This leads ioctl 
 * to return success even when there has been an error on the i2c bus. 
 *
 *   gnats.netbsd.org/48932: Raspberry Pi i2c  ioctl returns success even though i2c slave device NACKed
 *
 * An additional i2c write is required to ensure that the write request succeeds. This conditional can be disabled
 * once the NetBSD Pi PR is fixed in a future Pi NetBSD image.
 *
 */
#define PI_NETBSD_I2C_DRIVER_BUG_PR48932_PRESENT


//===========================================================================================================================
//  Globals
//===========================================================================================================================

static uint8_t *        gMFiCertificatePtr  = NULL;
static size_t           gMFiCertificateLen  = 0;
static int              gMFiAuthFD = -1;


//===========================================================================================================================
//  Prototypes
//===========================================================================================================================

static OSStatus     iic_write_bytes( int inFD, uint8_t inRegister, uint8_t *inBufPtr, size_t inBufLen );
static OSStatus     iic_read_bytes( int inFD, uint8_t inRegister, uint8_t *readBufPtr, size_t totalBytesToRead );
static OSStatus     doI2CWrite( int fd, i2c_addr_t addr, uint8_t *cmdPtr, size_t cmdSize, uint8_t *bufPtr, size_t bufSize );
static int          doI2CRead( int fd, i2c_addr_t addr, uint8_t *cmdbufp, size_t cmd_size, uint8_t *bufp, size_t buf_size );

#ifdef ENABLE_DEBUG
static uint8_t      readErrorRegister( int fd );
static void         dumpBuf(  uint8_t *bufPtr, size_t bufLen, char *str );
static void         queryCurrentChallenge( int fd );
#endif



//===========================================================================================================================
//  MFiPlatform_Initialize
//===========================================================================================================================

OSStatus    MFiPlatform_Initialize( void )
{
    OSStatus    err;

    // Open the iic device

    gMFiAuthFD = open(kMFiAuthDevicePath, O_RDWR);
    require_action( gMFiAuthFD != -1, exit, err = kOpenErr );

    // Cache the certificate at startup since the certificate doesn't change and this saves ~200 ms each time.
    
    err = MFiPlatform_CopyCertificate( &gMFiCertificatePtr, &gMFiCertificateLen );

exit:
    if ( err )  printf( "MFi Platform Initialize failed %d\n", err );
    return( err );
}

//===========================================================================================================================
//  MFiPlatform_Finalize
//===========================================================================================================================

void    MFiPlatform_Finalize( void )
{
    // close the iic device
    close(gMFiAuthFD);

    // cleanup cached certificate if any

    ForgetMem( &gMFiCertificatePtr );
    gMFiCertificateLen = 0;

    return;
}


//===========================================================================================================================
//  MFiPlatform_CreateSignature
//===========================================================================================================================

OSStatus
    MFiPlatform_CreateSignature( 
        const void *    inDigestPtr, 
        size_t          inDigestLen, 
        uint8_t **      outSignaturePtr,
        size_t *        outSignatureLen )
{
    OSStatus        err;
    uint8_t         buf[ 32 ];
    size_t          signatureLen;
    uint8_t *       signaturePtr;


    require_action( inDigestPtr && ( inDigestLen > 0 ), exit, err = kParamErr );
    require_action( outSignaturePtr && outSignatureLen, exit, err = kParamErr );
    require_action( inDigestLen == kMFiAuthChallengeLen, exit, err = kSizeErr );

    //
    // Write our Challenge Size & Data 
    //

    buf[ 0 ] = (uint8_t)( ( inDigestLen >> 8 ) & 0xFF );
    buf[ 1 ] = (uint8_t)(   inDigestLen        & 0xFF );
    memcpy( &buf[ 2 ], inDigestPtr, inDigestLen );
    err = iic_write_bytes( gMFiAuthFD, kMFiAuthReg_ChallengeSize, buf, inDigestLen + 2 );
    require_noerr( err, exit );


    //
    // Generate the signature.
    //
    
    buf[ 0 ] = kMFiAuthControl_GenerateSignature;
    err = iic_write_bytes( gMFiAuthFD, kMFiAuthReg_AuthControlStatus, buf, 1 );
    require_noerr( err, exit );


    //
    // Wait a bit after create signature - or else the control status get will return 0xff for some reason
    //
    usleep( kMFiAuthRetryDelayMics );


    //
    // Read Control Status for the signature generation
    //
    
    err = iic_read_bytes( gMFiAuthFD, kMFiAuthReg_AuthControlStatus, buf, 1);
    require_noerr( err, exit );
    require_action( !( buf[ 0 ] & kMFiAuthFlagError ), exit, err = kUnknownErr );
    

    //
    // Read the signature Size
    //
    
    buf[0] = 0;
    buf[1] = 0;
    err = iic_read_bytes( gMFiAuthFD, kMFiAuthReg_SignatureSize, buf, 2);
    require_noerr( err, exit );
    signatureLen = ( buf[ 0 ] << 8 ) | buf[ 1 ];
    require_action( signatureLen > 0, exit, err = kSizeErr );
    
    signaturePtr = (uint8_t *) malloc( signatureLen );
    require_action( signaturePtr, exit, err = kNoMemoryErr );
    


    //
    // Read the signature.
    //
    
    err = iic_read_bytes( gMFiAuthFD, kMFiAuthReg_SignatureData, signaturePtr, signatureLen);
    if( err ) free( signaturePtr );
    require_noerr( err, exit );
    

    printf( "MFi auth created signature:%zu\n", signatureLen );
    *outSignaturePtr = signaturePtr;
    *outSignatureLen = signatureLen;

exit:
    if ( err )      printf( "### MFi auth create signature failed: %d\n", err );
    return( err );
}


//===========================================================================================================================
//  MFiPlatform_CopyCertificate
//===========================================================================================================================

OSStatus    MFiPlatform_CopyCertificate( uint8_t **outCertificatePtr, size_t *outCertificateLen )
{
    OSStatus        err;
    size_t          certificateLen;
    uint8_t *       certificatePtr;
    uint8_t         buf[ 2 ];

    printf( "MFi auth copy certificate\n" );
    
    require_action( ( outCertificatePtr && outCertificateLen ), exit, err = kParamErr );

    // If the certificate has already been cached then return that as an optimization since it doesn't change.
    
    if( gMFiCertificateLen > 0 )
    {
        certificatePtr = (uint8_t *) malloc( gMFiCertificateLen );
        require_action( certificatePtr, exit, err = kNoMemoryErr );
        memcpy( certificatePtr, gMFiCertificatePtr, gMFiCertificateLen );
        
        *outCertificatePtr = certificatePtr;
        *outCertificateLen = gMFiCertificateLen;
        err = kNoErr;
        goto exit;
    }
    

    // Retrieve Certificate Length - 2 bytes
    err = iic_read_bytes( gMFiAuthFD, kMFiAuthReg_DeviceCertificateSize, buf, 2);
    require_noerr( err, exit );
    certificateLen = ( buf[ 0 ] << 8 ) | buf[ 1 ];
    require_action( certificateLen > 0, exit, err = kSizeErr );

    certificatePtr = (uint8_t *) malloc( certificateLen );
    require_action( certificatePtr, exit, err = kNoMemoryErr );


    // Retrieve Certificate - Note: reads from the data1 register auto-increment to data2, data3, etc. registers that follow it.
    err = iic_read_bytes( gMFiAuthFD, kMFiAuthReg_DeviceCertificateData1, certificatePtr, certificateLen );
    if( err ) free( certificatePtr );
    require_noerr( err, exit );

    printf( "MFi auth copy certificate done: %zu bytes\n", certificateLen );
    *outCertificatePtr = certificatePtr;
    *outCertificateLen = certificateLen;
    
exit:
    if( err )       printf( "### MFi auth copy certificate failed: %d\n", err );
    return( err );
}


//===========================================================================================================================
//  iic_write_bytes
//===========================================================================================================================

static OSStatus
iic_write_bytes(int inFD, uint8_t inRegister, uint8_t *inBufPtr, size_t inBufLen)
{
    int         err, tries;
    uint8_t     buf[ 1 + inBufLen ];
    uint64_t    deadline;
    size_t      len;
    
    
    // Gather the register and data so it can be done as a single write transaction.
    
    buf[ 0 ] = inRegister;
    if ( inBufLen ) memcpy( &buf[ 1 ], inBufPtr, inBufLen );
    len = 1 + inBufLen;

#ifdef PI_NETBSD_I2C_DRIVER_BUG_PR48932_PRESENT
    err = doI2CWrite(inFD, kMFiAuthDeviceAddress, buf, len, NULL, 0);
    usleep(1);
    // ignoring err. Deliberate fall through.
#endif

    deadline = UpTicks() + SecondsToUpTicks( 2 );
    
    for( tries = 0; ; ++tries )
    {
        err = doI2CWrite(inFD, kMFiAuthDeviceAddress, buf, len, NULL, 0);
        if( !err ) break;
        
        usleep( kMFiAuthRetryDelayMics );
        require_action( UpTicks() < deadline, exit, err = kTimeoutErr );
    }

exit:
    if ( tries ) printf("%s() - retries=%d writeLen=%zu\n", __FUNCTION__, tries, len);
    if ( err)    printf("%s() failed inRegister=%d inBufLen=%zu\n", __FUNCTION__, inRegister, inBufLen);

    return( err );
}



//===========================================================================================================================
//  iic_read_bytes
//===========================================================================================================================

static OSStatus
iic_read_bytes(int inFD, uint8_t inRegister, uint8_t *readBufPtr, size_t totalBytesToRead)
{
    int             err, tries;
    size_t          readSoFar;
    size_t          bytesRemaining;
    uint64_t        deadline;
    

#define MAX_NETBSD_PI_READ_CHUNK    32  // this is the max that can be read successfully over i2c currently on Pi NetBSD

    deadline = UpTicks() + SecondsToUpTicks( 2 );
    
    for( tries = 0; ; ++tries )
    {
#ifdef PI_NETBSD_I2C_DRIVER_BUG_PR48932_PRESENT
        /*
         * This extra write is needed due to NetBSD Pi i2c driver PR48932.  Ideally, the register value will
         * be written to the i2c bus first as part of the iic_read() below. But to the PR mentioned, the driver
         * does not convey write errror if that write fails. Once the PR is fixed, this extra write here can be
         * removed and the iic_read below should write the register value and then do the read on the i2c bus.
         */
        err = iic_write_bytes( inFD, inRegister, NULL, 0 );
        require( err == kNoErr, exit );
#endif

        readSoFar = 0;

        while ( totalBytesToRead > readSoFar )
        {
            usleep(1);

            bytesRemaining = totalBytesToRead - readSoFar;
            bytesRemaining = ( bytesRemaining >= MAX_NETBSD_PI_READ_CHUNK) ? MAX_NETBSD_PI_READ_CHUNK : bytesRemaining;
            err = doI2CRead( inFD, kMFiAuthDeviceAddress, NULL, 0, readBufPtr + readSoFar, bytesRemaining );
            if( err ) 
            {
                usleep( kMFiAuthRetryDelayMics );
                require_action( UpTicks() < deadline, exit, err = kTimeoutErr );
                break;      // Error. So go back to the top of for loop for next retry
            }

            readSoFar += bytesRemaining;
        }
        if ( readSoFar >= totalBytesToRead ) break; // done reading sucessfully. get out.
    }

    err = kNoErr;

exit:
    if ( tries ) printf("%s() - retries=%d read=%zu totalBytesToRead=%zu\n", __FUNCTION__, tries, readSoFar, totalBytesToRead);
    if ( err )   printf("%s() failed inRegister=%d bytesToRead=%zu bytesRead=%zu\n", 
                __FUNCTION__, inRegister, totalBytesToRead, readSoFar);

    return( err );
}


//===========================================================================================================================
//  doI2CWrite
//===========================================================================================================================

static OSStatus
doI2CWrite(int fd, i2c_addr_t addr, uint8_t *cmdPtr, size_t cmdSize, uint8_t *bufPtr, size_t bufSize)
{
    i2c_ioctl_exec_t iie;
    int              ret;
    OSStatus         err;

    iie.iie_op = I2C_OP_WRITE;
    iie.iie_addr = addr;
    iie.iie_cmd = cmdPtr;
    iie.iie_cmdlen = cmdSize;
    iie.iie_buf = bufPtr;
    iie.iie_buflen = bufSize;

    ret = ioctl(fd, I2C_IOCTL_EXEC, &iie);
    require_action( ret != -1, exit, err = kWriteErr );

    err = kNoErr;

 exit:
    return ( err );
}



//===========================================================================================================================
//  doI2CRead
//===========================================================================================================================

static int
doI2CRead(int fd, i2c_addr_t addr, uint8_t *cmdbufp, size_t cmd_size, uint8_t *bufp, size_t buf_size)
{
    i2c_ioctl_exec_t    iie;
    int                 ret;
    OSStatus            err;

    iie.iie_op = I2C_OP_READ;
    iie.iie_addr = addr;
    iie.iie_cmd = cmdbufp;
    iie.iie_cmdlen = cmd_size;
    iie.iie_buf = bufp;
    iie.iie_buflen = buf_size;

    ret = ioctl(fd, I2C_IOCTL_EXEC, &iie);
    require_action( ret != -1, exit, err = kReadErr );

    err = kNoErr;

 exit:

    return( err );
}



#ifdef ENABLE_DEBUG
//===========================================================================================================================
//  readErrorRegister
//===========================================================================================================================

uint8_t readErrorRegister( int fd )
{
    uint8_t         errorValue;

    // Read Error Register Content 

    errorValue = 0xEF;
    iic_read_bytes( fd, kMFiAuthReg_Error, &errorValue, 1);

    return( errorValue );
}


//===========================================================================================================================
//  dumpBuf
//===========================================================================================================================

void dumpBuf( uint8_t *bufPtr, size_t bufLen, char *str)
{
    size_t i;

    printf("==== %s [%zu] ====\n", str, bufLen);
    for (i=1; i <= bufLen; i++)
    {
        printf("%02x", ((uint8_t *) bufPtr)[i-1]);
        if ((i % 4) == 0) printf(" ");
        if ((i % 48) == 0) printf("\n");
    }
    printf("\n");

    return;
}

//===========================================================================================================================
//  queryCurrentChallenge
//===========================================================================================================================

void queryCurrentChallenge(int fd )
{
    uint8_t     buf[ 32 ];
    size_t      challengeSize;

    //
    // Read challenge size 
    //

    buf[0] = 0xDE;
    buf[1] = 0xAF;
    iic_read_bytes( fd, kMFiAuthReg_ChallengeSize, buf, 2);
    challengeSize = (buf[0] << 8) | buf[1];
    printf("Read challenge size=%zu[%x:%x]\n", challengeSize, buf[0], buf[1]);


    //
    // Read current challenge data before writing our own challenge
    //

    iic_read_bytes( fd, kMFiAuthReg_ChallengeData, buf, challengeSize );

    return;
}
#endif


