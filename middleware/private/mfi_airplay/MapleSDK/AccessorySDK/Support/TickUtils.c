/*
	Copyright (C) 2001-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "TickUtils.h"

#include "CommonServices.h"
#include "DebugServices.h"

#if (TARGET_MACH)
#include <mach/mach_time.h>
#endif
#if (TARGET_OS_NETBSD && !TARGET_KERNEL)
#include <sys/sysctl.h>
#endif
#if (TARGET_OS_POSIX)
#include <sys/time.h>
#endif
#if (TARGET_OS_WINDOWS)
#pragma warning(disable : 4201) // Disable "nonstandard extension used : nameless struct/union" warning for Microsoft headers.
#include "Mmsystem.h"
#pragma warning(default : 4201) // Re-enable "nonstandard extension used : nameless struct/union" after Microsoft headers.
#pragma comment(lib, "Winmm.lib")
#endif

#include LIBDISPATCH_HEADER

#if (TARGET_MACH)

#if 0
#pragma mark == Darwin ==
#endif

//===========================================================================================================================
//	UpTicksPerSecond
//===========================================================================================================================

static void _UpTicksPerSecondInit(void* inContext);

uint64_t UpTicksPerSecond(void)
{
    static dispatch_once_t sOnce = 0;
    static uint64_t sTicksPerSecond = 0;

    dispatch_once_f(&sOnce, &sTicksPerSecond, _UpTicksPerSecondInit);
    return (sTicksPerSecond);
}

static void _UpTicksPerSecondInit(void* inContext)
{
    uint64_t* const ticksPerSecondPtr = (uint64_t*)inContext;
    kern_return_t err;
    struct mach_timebase_info info;
    uint64_t ticksPerSecond;

    err = mach_timebase_info(&info);
    check_noerr(err);
    check(info.numer != 0);
    check(info.denom != 0);
    if (!err && (info.numer != 0) && (info.denom != 0)) {
        ticksPerSecond = info.denom;
        ticksPerSecond *= 1000000000;
        ticksPerSecond /= info.numer;
    } else {
        ticksPerSecond = 1000000000; // No valid info so assume nanoseconds.
    }
    *ticksPerSecondPtr = ticksPerSecond;
}

#endif // TARGET_MACH

#if (TARGET_OS_FREEBSD || TARGET_OS_LINUX || TARGET_OS_QNX)

#if 0
#pragma mark -
#pragma mark == FreeBSD/Linux/QNX ==
#endif

//===========================================================================================================================
//	UpTicks
//===========================================================================================================================

uint64_t UpTicks(void)
{
    uint64_t nanos;
    struct timespec ts;

    ts.tv_sec = 0;
    ts.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    nanos = ts.tv_sec;
    nanos *= 1000000000;
    nanos += ts.tv_nsec;
    return (nanos);
}
#endif

#if (TARGET_OS_NETBSD && !TARGET_KERNEL)

#if 0
#pragma mark -
#pragma mark == NetBSD ==
#endif

//===========================================================================================================================
//	UpTicksPerSecond
//===========================================================================================================================

uint64_t UpTicksPerSecond(void)
{
    static Boolean sInitialized = false;
    static uint64_t sTicksPerSecond = 0;

    if (!sInitialized) {
        OSStatus err;
        uint64_t ticksPerSecond;
        size_t len;

        len = sizeof(ticksPerSecond);
        err = sysctlbyname("kern.upticksrate", &ticksPerSecond, &len, NULL, 0);
        err = map_global_noerr_errno(err);
        if (err) {
            ticksPerSecond = 166666600;
            dlogassert("no kern.upticksrate (%#m), defaulting to %llu", err, ticksPerSecond);
        }
        sTicksPerSecond = ticksPerSecond;
        sInitialized = true;
    }
    return (sTicksPerSecond);
}
#endif

#if (TARGET_OS_WINDOWS)

#if 0
#pragma mark -
#pragma mark == Windows ==
#endif

//===========================================================================================================================
//	UpTicks
//===========================================================================================================================

uint64_t UpTicks(void)
{
    static Boolean sInitialized = false;
    static Boolean sHasHighResTimer = false;
    LARGE_INTEGER t;

    if (!sInitialized) {
        if (QueryPerformanceCounter(&t)) {
            sHasHighResTimer = true;
        }
        sInitialized = true;
    }
    if (sHasHighResTimer) {
        LARGE_INTEGER t;

        if (QueryPerformanceCounter(&t)) {
            return ((uint64_t)t.QuadPart);
        } else {
            dlogassert("QueryPerformanceCounter() failed when it worked earlier: %#m", GetLastError());
        }
    }
    return ((uint64_t)timeGetTime());
}

//===========================================================================================================================
//	UpTicksPerSecond
//===========================================================================================================================

uint64_t UpTicksPerSecond(void)
{
    static Boolean sInitialized = false;
    static uint64_t sTicksPerSecond = 1000; // Default to milliseconds (what timeGetTime returns) if init fails.

    if (!sInitialized) {
        LARGE_INTEGER freq;

        if (QueryPerformanceFrequency(&freq) && (freq.QuadPart != 0)) {
            sTicksPerSecond = (uint64_t)freq.QuadPart;
        } else {
            dlog(kLogLevelNotice, "no high-res timer: %#m\n", GetLastError());
        }
        sInitialized = true;
    }
    return (sTicksPerSecond);
}

#endif // TARGET_OS_WINDOWS

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	SleepForUpTicks
//===========================================================================================================================

#if (TARGET_OS_POSIX)
void SleepForUpTicks(uint64_t inTicks)
{
    uint64_t ticksPerSec, ticks, deadline;
    struct timespec ts;
    OSStatus err;

    DEBUG_USE_ONLY(err);

    ticksPerSec = UpTicksPerSecond();
    ticks = UpTicks();
    deadline = ticks + inTicks;
    for (; ticks < deadline; ticks = UpTicks()) {
        ticks = deadline - ticks;
        ts.tv_sec = (int32_t)(ticks / ticksPerSec);
        ts.tv_nsec = (int32_t)(((ticks % ticksPerSec) * kNanosecondsPerSecond) / ticksPerSec);
        err = nanosleep(&ts, NULL);
        err = map_global_noerr_errno(err);
        if (err == EINTR)
            continue;
        check_noerr(err);
    }
}
#endif

#if (TARGET_OS_WINDOWS)
void SleepForUpTicks(uint64_t inTicks)
{
    Sleep((DWORD)((inTicks * kMillisecondsPerSecond) / UpTicksPerSecond()));
}
#endif

//===========================================================================================================================
//	SleepUntilUpTicks
//===========================================================================================================================

#if (TickUtils_HAS_SLEEP_PRIMITIVE)
void SleepUntilUpTicks(uint64_t inTicks)
{
    uint64_t ticks;

    if (inTicks > 0) {
        ticks = UpTicks();
        if (ticks < inTicks) {
            SleepForUpTicks(inTicks - ticks);
        }
    }
}
#endif

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	UpTicksTo*
//===========================================================================================================================

uint64_t UpTicksToSeconds(uint64_t inTicks)
{
    static double sMultiplier = 0;

    if (sMultiplier == 0)
        sMultiplier = 1.0 / ((double)UpTicksPerSecond());
    return ((uint64_t)(((double)inTicks) * sMultiplier));
}

double UpTicksToSecondsF(uint64_t inTicks)
{
    static double sMultiplier = 0;

    if (sMultiplier == 0)
        sMultiplier = 1.0 / ((double)UpTicksPerSecond());
    return (((double)inTicks) * sMultiplier);
}

uint64_t UpTicksToMilliseconds(uint64_t inTicks)
{
    static double sMultiplier = 0;

    if (sMultiplier == 0)
        sMultiplier = ((double)kMillisecondsPerSecond) / ((double)UpTicksPerSecond());
    return ((uint64_t)(((double)inTicks) * sMultiplier));
}

uint64_t UpTicksToMicroseconds(uint64_t inTicks)
{
    static double sMultiplier = 0;

    if (sMultiplier == 0)
        sMultiplier = ((double)kMicrosecondsPerSecond) / ((double)UpTicksPerSecond());
    return ((uint64_t)(((double)inTicks) * sMultiplier));
}

uint64_t UpTicksToNanoseconds(uint64_t inTicks)
{
    static double sMultiplier = 0;

    if (sMultiplier == 0)
        sMultiplier = ((double)kNanosecondsPerSecond) / ((double)UpTicksPerSecond());
    return ((uint64_t)(((double)inTicks) * sMultiplier));
}

uint64_t UpTicksToNTP(uint64_t inTicks)
{
    uint64_t ticksPerSec;

    ticksPerSec = UpTicksPerSecond();
    return (((inTicks / ticksPerSec) << 32) | (((inTicks % ticksPerSec) << 32) / ticksPerSec));
}

#if (!TARGET_OS_THREADX)
//===========================================================================================================================
//    UpTicksToTimeValTimeout
//===========================================================================================================================

struct timeval* UpTicksToTimeValTimeout(uint64_t inDeadlineTicks, struct timeval* inTimeVal)
{
    uint64_t ticks;
    uint64_t mics;

    if (inDeadlineTicks != kUpTicksForever) {
        ticks = UpTicks();
        if (inDeadlineTicks > ticks) {
            mics = UpTicksToMicroseconds(inDeadlineTicks - ticks);
            inTimeVal->tv_sec = (int)(mics / 1000000);
            inTimeVal->tv_usec = (int)(mics % 1000000);
        } else {
            inTimeVal->tv_sec = 0;
            inTimeVal->tv_usec = 0;
        }
        return (inTimeVal);
    }
    return (NULL);
}
#endif

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	*ToUpTicks
//===========================================================================================================================

uint64_t SecondsToUpTicks(uint64_t x)
{
    static uint64_t sMultiplier = 0;

    if (sMultiplier == 0)
        sMultiplier = UpTicksPerSecond();
    return (x * sMultiplier);
}

uint64_t SecondsToUpTicksF(double x)
{
    static double sMultiplier = 0;

    if (sMultiplier == 0)
        sMultiplier = (double)UpTicksPerSecond();
    return ((uint64_t)(x * sMultiplier));
}

uint64_t MillisecondsToUpTicks(uint64_t x)
{
    static double sMultiplier = 0;

    if (sMultiplier == 0)
        sMultiplier = ((double)UpTicksPerSecond()) / ((double)kMillisecondsPerSecond);
    return ((uint64_t)(((double)x) * sMultiplier));
}

uint64_t MicrosecondsToUpTicks(uint64_t x)
{
    static double sMultiplier = 0;

    if (sMultiplier == 0)
        sMultiplier = ((double)UpTicksPerSecond()) / ((double)kMicrosecondsPerSecond);
    return ((uint64_t)(((double)x) * sMultiplier));
}

uint64_t NanosecondsToUpTicks(uint64_t x)
{
    static double sMultiplier = 0;

    if (sMultiplier == 0)
        sMultiplier = ((double)UpTicksPerSecond()) / ((double)kNanosecondsPerSecond);
    return ((uint64_t)(((double)x) * sMultiplier));
}

uint64_t NTPtoUpTicks(uint64_t inNTP)
{
    uint64_t ticksPerSec, ticks;

    ticksPerSec = UpTicksPerSecond();
    ticks = (inNTP >> 32) * ticksPerSec;
    ticks += (((inNTP & UINT32_C(0xFFFFFFFF)) * ticksPerSec) >> 32);
    return (ticks);
}

#if 0
#pragma mark -
#pragma mark == Tests ==
#endif

#if (!EXCLUDE_UNIT_TESTS)

//===========================================================================================================================
//	TickUtilsTest
//===========================================================================================================================

OSStatus TickUtilsTest(void)
{
    OSStatus err = kNoErr;
    uint64_t ticks, u64;

// UpTicksPerSecond

#if (TARGET_OS_DARWIN && (TARGET_CPU_X86 || TARGET_CPU_X86_64))
    require_action(UpTicksPerSecond() == 1000000000, exit, err = kMismatchErr);
#elif (TARGET_OS_DARWIN && (TARGET_CPU_ARM || TARGET_CPU_ARM64))
    require_action(UpTicksPerSecond() == 24000000, exit, err = kMismatchErr);
#else
    require_action(UpTicksPerSecond() >= 1000, exit, err = kMismatchErr);
#endif

    // UpTicks

    require_action(UpTicks() > 0, exit, err = kValueErr);
    require_action(UpSeconds() > 0, exit, err = kValueErr);
    require_action(UpMilliseconds() > 0, exit, err = kValueErr);
    require_action(UpMicroseconds() > 0, exit, err = kValueErr);
    require_action(UpNanoseconds() > 0, exit, err = kValueErr);
    require_action(UpNTP() > 0, exit, err = kValueErr);

    // UpTicksToSeconds

    require_action(UpTicksToSeconds(0) == 0, exit, err = kValueErr);
    require_action(UpTicksToSecondsF(0) == 0.0, exit, err = kValueErr);
    require_action(UpTicksToSeconds(UpTicksPerSecond() / 2) == 0, exit, err = kValueErr);
    require_action(UpTicksToSecondsF(UpTicksPerSecond() / 2) == 0.5, exit, err = kValueErr);
    require_action(UpTicksToSeconds(UpTicksPerSecond()) == 1, exit, err = kValueErr);
    require_action(UpTicksToSecondsF(UpTicksPerSecond()) == 1.0, exit, err = kValueErr);
    require_action(UpTicksToSecondsF(UpTicksPerSecond() + (UpTicksPerSecond() / 2)) == 1.5, exit, err = kValueErr);
    require_action(UpTicksToSeconds(UpTicksPerSecond() * 2) == 2, exit, err = kValueErr);

    // UpTicksToMilliseconds

    require_action(UpTicksToMilliseconds(0) == 0, exit, err = kValueErr);
    require_action(UpTicksToMilliseconds(UpTicksPerSecond() / 2) == 500, exit, err = kValueErr);
    require_action(UpTicksToMilliseconds(UpTicksPerSecond()) == 1000, exit, err = kValueErr);
    require_action(UpTicksToMilliseconds(UpTicksPerSecond() + (UpTicksPerSecond() / 2)) == 1500, exit, err = kValueErr);
    require_action(UpTicksToMilliseconds(UpTicksPerSecond() * 2) == 2000, exit, err = kValueErr);

    // UpTicksToMicroseconds

    require_action(UpTicksToMicroseconds(0) == 0, exit, err = kValueErr);
    require_action(UpTicksToMicroseconds(UpTicksPerSecond() / 2) == 500000, exit, err = kValueErr);
    require_action(UpTicksToMicroseconds(UpTicksPerSecond()) == 1000000, exit, err = kValueErr);
    require_action(UpTicksToMicroseconds(UpTicksPerSecond() + (UpTicksPerSecond() / 2)) == 1500000, exit, err = kValueErr);
    require_action(UpTicksToMicroseconds(UpTicksPerSecond() * 2) == 2000000, exit, err = kValueErr);

    // UpTicksToNanoseconds

    require_action(UpTicksToNanoseconds(0) == 0, exit, err = kValueErr);
    require_action(UpTicksToNanoseconds(UpTicksPerSecond() / 2) == 500000000, exit, err = kValueErr);
    require_action(UpTicksToNanoseconds(UpTicksPerSecond()) == 1000000000, exit, err = kValueErr);
    require_action(UpTicksToNanoseconds(UpTicksPerSecond() + (UpTicksPerSecond() / 2)) == 1500000000, exit, err = kValueErr);
    require_action(UpTicksToNanoseconds(UpTicksPerSecond() * 2) == 2000000000, exit, err = kValueErr);

    // UpTicksToNTP

    require_action(UpTicksToNTP(0) == 0, exit, err = kValueErr);
    require_action(UpTicksToNTP(UpTicksPerSecond() / 2) == 0x80000000, exit, err = kValueErr);
    require_action(UpTicksToNTP(UpTicksPerSecond()) == 0x0000000100000000, exit, err = kValueErr);
    require_action(UpTicksToNTP(UpTicksPerSecond() + (UpTicksPerSecond() / 2)) == 0x0000000180000000, exit, err = kValueErr);

    // SecondsToUpTicks

    require_action(SecondsToUpTicks(0) == 0, exit, err = kValueErr);
    require_action(SecondsToUpTicksF(0.0) == 0, exit, err = kValueErr);
    require_action(SecondsToUpTicks(1) == UpTicksPerSecond(), exit, err = kValueErr);
    require_action(SecondsToUpTicksF(1.0) == UpTicksPerSecond(), exit, err = kValueErr);
    require_action(SecondsToUpTicksF(1.5) == (UpTicksPerSecond() + (UpTicksPerSecond() / 2)), exit, err = kValueErr);
    require_action(SecondsToUpTicks(2) == (UpTicksPerSecond() * 2), exit, err = kValueErr);
    require_action(SecondsToUpTicksF(2.0) == (UpTicksPerSecond() * 2), exit, err = kValueErr);

    // MillisecondsToUpTicks

    require_action(MillisecondsToUpTicks(0) == 0, exit, err = kValueErr);
    require_action(MillisecondsToUpTicks(500) == (UpTicksPerSecond() / 2), exit, err = kValueErr);
    require_action(MillisecondsToUpTicks(1000) == UpTicksPerSecond(), exit, err = kValueErr);
    require_action(MillisecondsToUpTicks(1500) == (UpTicksPerSecond() + (UpTicksPerSecond() / 2)), exit, err = kValueErr);
    require_action(MillisecondsToUpTicks(2000) == (UpTicksPerSecond() * 2), exit, err = kValueErr);

    // MicrosecondsToUpTicks

    require_action(MicrosecondsToUpTicks(0) == 0, exit, err = kValueErr);
    require_action(MicrosecondsToUpTicks(500000) == (UpTicksPerSecond() / 2), exit, err = kValueErr);
    require_action(MicrosecondsToUpTicks(1000000) == UpTicksPerSecond(), exit, err = kValueErr);
    require_action(MicrosecondsToUpTicks(1500000) == (UpTicksPerSecond() + (UpTicksPerSecond() / 2)), exit, err = kValueErr);
    require_action(MicrosecondsToUpTicks(2000000) == (UpTicksPerSecond() * 2), exit, err = kValueErr);

    // NanosecondsToUpTicks

    require_action(NanosecondsToUpTicks(0) == 0, exit, err = kValueErr);
    require_action(NanosecondsToUpTicks(500000000) == (UpTicksPerSecond() / 2), exit, err = kValueErr);
    require_action(NanosecondsToUpTicks(1000000000) == UpTicksPerSecond(), exit, err = kValueErr);
    require_action(NanosecondsToUpTicks(1500000000) == (UpTicksPerSecond() + (UpTicksPerSecond() / 2)), exit, err = kValueErr);
    require_action(NanosecondsToUpTicks(2000000000) == (UpTicksPerSecond() * 2), exit, err = kValueErr);

    // NTPtoUpTicks

    require_action(NTPtoUpTicks(0) == 0, exit, err = kValueErr);
    require_action(NTPtoUpTicks(0x80000000) == (UpTicksPerSecond() / 2), exit, err = kValueErr);
    require_action(NTPtoUpTicks(0x0000000100000000) == UpTicksPerSecond(), exit, err = kValueErr);
    require_action(NTPtoUpTicks(0x0000000180000000) == (UpTicksPerSecond() + (UpTicksPerSecond() / 2)), exit, err = kValueErr);

    // SleepForUpTicks

    ticks = UpTicks();
    SleepForUpTicks(MillisecondsToUpTicks(100));
    u64 = UpTicksToMilliseconds(UpTicks() - ticks);
    require_action((u64 >= 99) && (u64 <= 200), exit, err = kValueErr);

    // SleepUntilUpTicks

    ticks = UpTicks();
    SleepUntilUpTicks(UpTicks() + MillisecondsToUpTicks(100));
    u64 = UpTicksToMilliseconds(UpTicks() - ticks);
    require_action((u64 >= 99) && (u64 <= 200), exit, err = kValueErr);

exit:
    return err;
}

#endif // !EXCLUDE_UNIT_TESTS
