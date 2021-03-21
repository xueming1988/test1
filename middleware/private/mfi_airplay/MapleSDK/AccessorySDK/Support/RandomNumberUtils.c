/*
	Copyright (C) 2001-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "RandomNumberUtils.h"

#include "CommonServices.h"
#include "DebugServices.h"

#if (TARGET_HAS_STD_C_LIB)
#include <stdlib.h>
#endif

#if (TARGET_HAS_MOCANA_SSL)
#include "Networking/SSL/mtypes.h"

#include "Networking/SSL/merrors.h"
#include "Networking/SSL/random.h"
#endif

#if (TARGET_PLATFORM_WICED && defined(_STM32x_))
#include "stm32f2xx_rng.h"
#endif

#if (TARGET_OS_LINUX || (TARGET_OS_QNX && TARGET_NO_OPENSSL))
#include <fcntl.h>
#include <pthread.h>
#endif

#if (TARGET_OS_QNX && !TARGET_NO_OPENSSL)
#include <openssl/rand.h>
#endif

#if (TARGET_OS_DARWIN)
//===========================================================================================================================
//	RandomBytes
//===========================================================================================================================

OSStatus RandomBytes(void* inBuffer, size_t inByteCount)
{
    return (CCRandomGenerateBytes(inBuffer, inByteCount));
}
#endif

#if (TARGET_HAS_MOCANA_SSL)
//===========================================================================================================================
//	RandomBytes
//===========================================================================================================================

static randomContext* gRandomContext = NULL;

OSStatus RandomBytes(void* inBuffer, size_t inByteCount)
{
    uint8_t* const buf = (uint8_t*)inBuffer;
    OSStatus err;

    if (gRandomContext == NULL) {
        err = RANDOM_acquireContext(&gRandomContext);
        require_noerr(err, exit);
    }

    err = RANDOM_numberGenerator(gRandomContext, buf, (sbyte4)inByteCount);
    require_noerr(err, exit);

exit:
    return (err);
}
#endif

#if (TARGET_PLATFORM_WICED && defined(__STM32F2xx_RNG_H))
//===========================================================================================================================
//	RandomBytes
//
//	STM32F2xx implementation.
//===========================================================================================================================

OSStatus RandomBytes(void* inBuffer, size_t inByteCount)
{
    uint8_t* const buf = (uint8_t*)inBuffer;
    size_t offset;
    size_t len;
    uint32_t r;

    // Enable RNG clock source and peripheral.

    RCC_AHB2PeriphClockCmd(RCC_AHB2Periph_RNG, ENABLE);
    RNG_Cmd(ENABLE);

    for (offset = 0; (inByteCount - offset) >= 4;) {
        while (RNG_GetFlagStatus(RNG_FLAG_DRDY) == RESET) {
        } // Wait for RNG to be ready.
        r = RNG_GetRandomNumber();
        buf[offset++] = (uint8_t)((r >> 24) & 0xFF);
        buf[offset++] = (uint8_t)((r >> 16) & 0xFF);
        buf[offset++] = (uint8_t)((r >> 8) & 0xFF);
        buf[offset++] = (uint8_t)(r & 0xFF);
    }

    len = inByteCount - offset;
    if (len == 3) {
        while (RNG_GetFlagStatus(RNG_FLAG_DRDY) == RESET) {
        } // Wait for RNG to be ready.
        r = RNG_GetRandomNumber();
        buf[offset++] = (uint8_t)((r >> 24) & 0xFF);
        buf[offset++] = (uint8_t)((r >> 16) & 0xFF);
        buf[offset] = (uint8_t)((r >> 8) & 0xFF);
    } else if (len == 2) {
        while (RNG_GetFlagStatus(RNG_FLAG_DRDY) == RESET) {
        } // Wait for RNG to be ready.
        r = RNG_GetRandomNumber();
        buf[offset++] = (uint8_t)((r >> 24) & 0xFF);
        buf[offset] = (uint8_t)((r >> 16) & 0xFF);
    } else if (len == 1) {
        while (RNG_GetFlagStatus(RNG_FLAG_DRDY) == RESET) {
        } // Wait for RNG to be ready.
        r = RNG_GetRandomNumber();
        buf[offset] = (uint8_t)((r >> 24) & 0xFF);
    }
    return (kNoErr);
}
#endif

#if (TARGET_OS_POSIX && !TARGET_OS_DARWIN && !TARGET_OS_LINUX && !TARGET_OS_QNX)
//===========================================================================================================================
//	RandomBytes
//===========================================================================================================================

OSStatus RandomBytes(void* inBuffer, size_t inByteCount)
{
    uint8_t* const buf = (uint8_t*)inBuffer;
    size_t offset;
    size_t len;
    uint32_t r;

    for (offset = 0; (inByteCount - offset) >= 4;) {
        r = arc4random();
        buf[offset++] = (uint8_t)((r >> 24) & 0xFF);
        buf[offset++] = (uint8_t)((r >> 16) & 0xFF);
        buf[offset++] = (uint8_t)((r >> 8) & 0xFF);
        buf[offset++] = (uint8_t)(r & 0xFF);
    }

    len = inByteCount - offset;
    if (len == 3) {
        r = arc4random();
        buf[offset++] = (uint8_t)((r >> 24) & 0xFF);
        buf[offset++] = (uint8_t)((r >> 16) & 0xFF);
        buf[offset] = (uint8_t)((r >> 8) & 0xFF);
    } else if (len == 2) {
        r = arc4random();
        buf[offset++] = (uint8_t)((r >> 24) & 0xFF);
        buf[offset] = (uint8_t)((r >> 16) & 0xFF);
    } else if (len == 1) {
        r = arc4random();
        buf[offset] = (uint8_t)((r >> 24) & 0xFF);
    }
    return (kNoErr);
}
#endif

#if (TARGET_OS_LINUX || (TARGET_OS_QNX && TARGET_NO_OPENSSL))
//===========================================================================================================================
//	RandomBytes
//===========================================================================================================================

OSStatus RandomBytes(void* inBuffer, size_t inByteCount)
{
    static pthread_mutex_t sRandomLock = PTHREAD_MUTEX_INITIALIZER;
    static int sRandomFD = -1;
    uint8_t* dst;
    ssize_t n;

    pthread_mutex_lock(&sRandomLock);
    while (sRandomFD < 0) {
        sRandomFD = open("/dev/urandom", O_RDONLY);
        if (sRandomFD < 0) {
            dlogassert("open urandom error: %#m", errno);
            sleep(1);
            continue;
        }
        break;
    }
    pthread_mutex_unlock(&sRandomLock);

    dst = (uint8_t*)inBuffer;
    while (inByteCount > 0) {
        n = read(sRandomFD, dst, inByteCount);
        if (n < 0) {
            dlogassert("read urandom error: %#m", errno);
            sleep(1);
            continue;
        }
        dst += n;
        inByteCount -= n;
    }
    return (kNoErr);
}
#endif

#if (TARGET_OS_QNX && !TARGET_NO_OPENSSL)
//===========================================================================================================================
//	RandomBytes
//===========================================================================================================================

OSStatus RandomBytes(void* inBuffer, size_t inByteCount)
{
    return (RAND_bytes((unsigned char*)inBuffer, (int)inByteCount) ? kNoErr : kUnknownErr);
}
#endif

#if (TARGET_OS_WINDOWS)
//===========================================================================================================================
//	RandomBytes
//===========================================================================================================================

#define RtlGenRandom SystemFunction036
#ifdef __cplusplus
extern "C"
#endif
    BOOLEAN NTAPI
    RtlGenRandom(PVOID RandomBuffer, ULONG RandomBufferLength);

OSStatus RandomBytes(void* inBuffer, size_t inByteCount)
{
    OSStatus err;

    err = RtlGenRandom(inBuffer, (ULONG)inByteCount) ? kNoErr : kUnknownErr;
    check_noerr(err);
    return (err);
}
#endif

//===========================================================================================================================
//	RandomString
//===========================================================================================================================

char* RandomString(const char* inCharSet, size_t inCharSetSize, size_t inMinChars, size_t inMaxChars, char* outString)
{
    uint32_t r;
    char* ptr;
    char* end;

    check(inMinChars <= inMaxChars);

    RandomBytes(&r, sizeof(r));
    ptr = outString;
    end = ptr + (inMinChars + (r % ((inMaxChars - inMinChars) + 1)));
    while (ptr < end) {
        RandomBytes(&r, sizeof(r));
        *ptr++ = inCharSet[r % inCharSetSize];
    }
    *ptr = '\0';
    return (outString);
}

#if 0
#pragma mark -
#endif

#if (!EXCLUDE_UNIT_TESTS)

//===========================================================================================================================
//	RandomNumberUtilsTest
//===========================================================================================================================

OSStatus RandomNumberUtilsTest(void)
{
    OSStatus err;
    uint8_t buf[16];
    uint8_t buf2[16];
    size_t i, n;

    // Make sure at least 1/2 of the bytes were changed.

    memset(buf, 0, sizeof(buf));
    err = RandomBytes(buf, sizeof(buf));
    require_noerr(err, exit);
    n = 0;
    for (i = 0; i < countof(buf); ++i) {
        if (buf[i] != 0)
            ++n;
    }
    require(n >= (countof(buf) / 2), exit);

    // Make sure it doesn't return the same bytes twice.

    for (i = 0; i < countof(buf); ++i)
        buf2[i] = buf[i];
    err = RandomBytes(buf2, sizeof(buf2));
    require_noerr(err, exit);
    require_action(memcmp(buf, buf2, sizeof(buf)) != 0, exit, err = kMismatchErr);

exit:
    return err;
}
#endif // !EXCLUDE_UNIT_TESTS
