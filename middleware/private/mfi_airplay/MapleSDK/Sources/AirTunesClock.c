/*
	Copyright (C) 2007-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "AirTunesClock.h"
#include "AirPlayCommon.h"
#include "AirPlayNetworkAddress.h"
#include "AirTunesClockPriv.h"
#include "AirTunesPTPClock.h"

#include "CFUtils.h"
#include "DebugServices.h"
#include "ThreadUtils.h"
#include "TickUtils.h"
#include <time.h>

//===========================================================================================================================
//	Private
//===========================================================================================================================

// 64-bit fixed-pointed math (32.32).

typedef int64_t Fixed64;

#define Fixed64_Add(X, Y) ((X) += (Y))
#define Fixed64_Sub(X, Y) ((X) -= (Y))
#define Fixed64_RightShift(X, N)  \
    do {                          \
        if ((X) < 0)              \
            (X) = -(-(X) >> (N)); \
        else                      \
            (X) = (X) >> (N);     \
                                  \
    } while (0)

#define Fixed64_Multiply(X, Y) ((X) *= (Y))
#define Fixed64_Clear(X) ((X) = 0)
#define Fixed64_GetInteger(X) (((X) < 0) ? (-(-(X) >> 32)) : ((X) >> 32))
#define Fixed64_SetInteger(X, Y) ((X) = ((uint64_t)(Y)) << 32)

// Phase Lock Loop (PLL) constants.

#define kAirTunesClock_MaxPhase 128000000 // Max phase error (nanoseconds).
#define kAirTunesClock_MaxFrequency 500000 // Max frequence error (nanoseconds per second).
#define kAirTunesClock_PLLShift 4 // PLL loop gain (bit shift value).

typedef struct
{
    struct AirTunesClockPrivate header; // An AirTunesClock always starts with this struct
    AirTunesTime epochTime;
    AirTunesTime upTime;
    AirTunesTime lastTime;
    uint64_t frequency;
    uint64_t scale;
    uint32_t lastCount;
    int32_t lastOffset; // Last time offset (nanoseconds).
    int32_t lastAdjustTime; // Time at last adjustment (seconds).
    Fixed64 offset; // Time offset (nanoseconds).
    Fixed64 frequencyOffset; // Frequency offset (nanoseconds per second).
    int32_t second; // Current second.
    pthread_t thread;
    pthread_t* threadPtr;
    pthread_mutex_t lock;
    pthread_mutex_t* lockPtr;
    Boolean running;
} AirTunesNTPClock;

typedef struct {
    AirTunesClockRef clock;
    CFStringRef interface;
} ClockInterfacePair;

#define removePeers(peers, clock) applyPeers(peers, removePeer, clock)
#define addPeers(peers, clock) applyPeers(peers, addPeer, clock)

#if 0
#pragma mark == Logging ==
#endif

ulog_define(AirTunesClock, kLogLevelNotice, kLogFlags_Default, "AirTunesClock", NULL);
#define atcl_ucat() &log_category_from_name(AirTunesClock)
#define atcl_ulog(LEVEL, ...) ulog(atcl_ucat(), (LEVEL), __VA_ARGS__)

// Prototypes

DEBUG_STATIC void _AirTunesClock_Tick(AirTunesNTPClock* inClock);
DEBUG_STATIC void* _AirTunesClock_Thread(void* inArg);

DEBUG_STATIC OSStatus AirTunesNTPClock_Finalize(AirTunesClockRef inClock);
DEBUG_STATIC Boolean AirTunesNTPClock_Adjust(AirTunesClockRef inClock, int64_t inOffsetNanoseconds, Boolean inReset);
DEBUG_STATIC void AirTunesNTPClock_GetSynchronizedTime(AirTunesClockRef inClock, AirTunesTime* outTime);
DEBUG_STATIC void AirTunesNTPClock_GetSynchronizedTimeNearUpTicks(AirTunesClockRef inClock, AirTunesTime* outTime, uint64_t inTicks);
DEBUG_STATIC OSStatus AirTunesNTPClock_ConvertNetworkTimeToUpTicks(AirTunesClockRef inClock, AirTunesTime inNetworkTime, uint64_t* outUpTicks);
DEBUG_STATIC OSStatus AirTunesNTPClock_AddPeer(AirTunesClockRef inClock, CFStringRef inInterfaceName, const void* inSockAddr);
DEBUG_STATIC OSStatus AirTunesNTPClock_RemovePeer(AirTunesClockRef inClock, CFStringRef inInterfaceName, const void* inSockAddr);

static void applyPeers(CFArrayRef peers, CFArrayApplierFunction peerFunc, AirTunesClockRef inClock);
static void removePeer(const void* value, void* context);
static void addPeer(const void* value, void* context);

// Globals

//===========================================================================================================================
//	AirTunesNTPClock_Create
//===========================================================================================================================

OSStatus AirTunesNTPClock_Create(AirTunesClockRef* outRef)
{
    OSStatus err;
    AirTunesNTPClock* ntp;

    ntp = (AirTunesNTPClock*)calloc(1, sizeof(*ntp));
    require_action(ntp, exit, err = kNoMemoryErr);

    ntp->header.class = kAirTunesClockNTP;
    ntp->header.vtable.finalize = AirTunesNTPClock_Finalize;
    ntp->header.vtable.adjust = AirTunesNTPClock_Adjust;
    ntp->header.vtable.getNetworkTime = AirTunesNTPClock_GetSynchronizedTime;
    ntp->header.vtable.upTicksToNetworkTime = AirTunesNTPClock_GetSynchronizedTimeNearUpTicks;
    ntp->header.vtable.netTimeToUpTicks = AirTunesNTPClock_ConvertNetworkTimeToUpTicks;
    ntp->header.vtable.addPeer = AirTunesNTPClock_AddPeer;
    ntp->header.vtable.removePeer = AirTunesNTPClock_RemovePeer;

    ntp->epochTime.secs = 0;
    ntp->epochTime.frac = 0;

    ntp->upTime.secs = 0;
    ntp->upTime.frac = 0;

    ntp->lastTime.secs = 0;
    ntp->lastTime.frac = 0;

    ntp->frequency = UpTicksPerSecond();
    ntp->scale = UINT64_C(0xFFFFFFFFFFFFFFFF) / ntp->frequency;

    ntp->lastCount = 0;
    ntp->lastOffset = 0;
    ntp->lastAdjustTime = 0;

    Fixed64_Clear(ntp->offset);
    Fixed64_Clear(ntp->frequencyOffset);

    ntp->second = 1;

    err = pthread_mutex_init(&ntp->lock, NULL);
    require_noerr(err, exit);
    ntp->lockPtr = &ntp->lock;

    // Signal to Terminate the thread.
    ntp->running = true;
    err = pthread_create(&ntp->thread, NULL, _AirTunesClock_Thread, ntp);
    require_noerr(err, exit);
    ntp->threadPtr = &ntp->thread;

    *outRef = &ntp->header;
    ntp = NULL;
    err = kNoErr;

exit:
    if (ntp)
        AirTunesClock_Finalize(&ntp->header);
    return (err);
}

//===========================================================================================================================
//	AirTunesClock_Class
//===========================================================================================================================

AirTunesClockClass AirTunesClock_Class(AirTunesClockRef inClock)
{
    return inClock ? inClock->class : kAirTunesClockInvalid;
}

//===========================================================================================================================
//	AirTunesClock_Description
//===========================================================================================================================

const char* AirTunesClock_Description(AirTunesClockRef inClock)
{
    const char* desc;

    AirTunesClockClass clockClass = AirTunesClock_Class(inClock);
    switch (clockClass) {
    case kAirTunesClockNTP:
        desc = "NTP";
        break;

    case kAirTunesClockPTP:
        desc = "PTP";
        break;

    case kAirTunesClockInvalid:
    default:
        desc = "";
        break;
    }

    return desc;
}

//===========================================================================================================================
//	AirTunesClock_Create
//===========================================================================================================================

OSStatus AirTunesClock_Create(CFStringRef inTimingProtocol, AirTunesClockSettings* settings, AirTunesClockRef* outRef)
{
    OSStatus err = kNoErr;

    if (CFEqual(CFSTR(kAirPlayTimingProtocol_PTP), inTimingProtocol)) {
        err = AirTunesPTPClock_Create(&settings->ptp, outRef);
        require_noerr(err, exit);
    } else {
        err = AirTunesNTPClock_Create(outRef);
    }
exit:
    return err;
}

//===========================================================================================================================
//	AirTunesClock_Finalize
//===========================================================================================================================

OSStatus AirTunesClock_Finalize(AirTunesClockRef inClock)
{
    OSStatus err = kNoErr;

    if (inClock) {
        AirTunesClock_SetPeerList(inClock, NULL);
        err = inClock->vtable.finalize(inClock);
        ;
    }

    return err;
}

//===========================================================================================================================
//	AirTunesClock_Adjust
//===========================================================================================================================

Boolean AirTunesClock_Adjust(AirTunesClockRef inClock, int64_t inOffsetNanoseconds, Boolean inReset)
{
    Boolean didAdjust;

    require_action_quiet(inClock && inClock->vtable.adjust, exit, didAdjust = false);
    didAdjust = inClock->vtable.adjust(inClock, inOffsetNanoseconds, inReset);
exit:
    return didAdjust;
}

//===========================================================================================================================
//	AirTunesClock_GetSynchronizedTime
//===========================================================================================================================

void AirTunesClock_GetSynchronizedTime(AirTunesClockRef inClock, AirTunesTime* outTime)
{

    require_action_quiet(inClock && inClock->vtable.getNetworkTime, exit, AirTunesTime_Invalidate(outTime, AirTunesTimeFlag_Invalid));
    inClock->vtable.getNetworkTime(inClock, outTime);
exit:
    return;
}

//===========================================================================================================================
//	AirTunesClock_GetSynchronizedTimeNearUpTicks
//===========================================================================================================================

void AirTunesClock_GetSynchronizedTimeNearUpTicks(AirTunesClockRef inClock, AirTunesTime* outTime, uint64_t inTicks)
{
    require_action_quiet(inClock && inClock->vtable.upTicksToNetworkTime, exit, AirTunesTime_Invalidate(outTime, AirTunesTimeFlag_Invalid));

    inClock->vtable.upTicksToNetworkTime(inClock, outTime, inTicks);

exit:
    return;
}

//===========================================================================================================================
//    AirTunesClock_ConvertNetworkTimeToUpTicks
//===========================================================================================================================

OSStatus AirTunesClock_ConvertNetworkTimeToUpTicks(AirTunesClockRef inClock, AirTunesTime inNetworkTime, uint64_t* outUpTicks)
{
    OSStatus err;

    require_action_quiet(inClock && inClock->vtable.netTimeToUpTicks, exit, err = kNotPreparedErr);
    err = inClock->vtable.netTimeToUpTicks(inClock, inNetworkTime, outUpTicks);
exit:
    return err;
}

//===========================================================================================================================
//    AirTunesClock_SetPeerList
//===========================================================================================================================
OSStatus AirTunesClock_SetPeerList(AirTunesClockRef inClock, CFArrayRef peers)
{
    OSStatus err = kNoErr;

    require_action_quiet(inClock, exit, err = kNotPreparedErr);

    // Remove any peers that are in our current peer list but not in the new peer list.
    CFArrayRef peersToRemove = CFArrayCreateDifference(inClock->peers, peers);
    removePeers(peersToRemove, inClock);
    CFReleaseNullSafe(peersToRemove);

    // Add any peers that are in the new peer list but not in the current peer list
    CFArrayRef peersToAdd = CFArrayCreateDifference(peers, inClock->peers);
    addPeers(peersToAdd, inClock);
    CFReleaseNullSafe(peersToAdd);

    // Remember the current peer list
    ReplaceCF(&inClock->peers, peers);

exit:
    return err;
}

static void applyPeers(CFArrayRef peers, CFArrayApplierFunction peerFunc, AirTunesClockRef inClock)
{
    if (peers) {
        CFArrayApplyFunction(peers, CFRangeMake(0, CFArrayGetCount(peers)), peerFunc, inClock);
    }
}

static void removePeer(const void* value, void* context)
{
    OSStatus err = kNoErr;
    AirTunesClockRef clock = (AirTunesClockRef)context;
    AirPlayNetworkAddressRef address = (AirPlayNetworkAddressRef)value;
    sockaddr_ip sockAddr = AirPlayNetworkAddressGetSocketAddr(address);
    CFStringRef interface = AirPlayNetworkAddressGetInterface(address);

    err = clock->vtable.removePeer(clock, interface, &sockAddr);
    if (err) {
        atcl_ulog(kLogLevelWarning, "%s failed: %d\n", __func__, err);
    }
}

static void addPeer(const void* value, void* context)
{
    OSStatus err = kNoErr;
    AirTunesClockRef clock = (AirTunesClockRef)context;
    AirPlayNetworkAddressRef address = (AirPlayNetworkAddressRef)value;
    sockaddr_ip sockAddr = AirPlayNetworkAddressGetSocketAddr(address);
    CFStringRef interface = AirPlayNetworkAddressGetInterface(address);

    err = clock->vtable.addPeer(clock, interface, &sockAddr);
    if (err) {
        atcl_ulog(kLogLevelWarning, "%s failed: %d\n", __func__, err);
    }
}

//===========================================================================================================================
//	AirTunesClock_GetSynchronizedNTPTime
//===========================================================================================================================

uint64_t AirTunesClock_GetSynchronizedNTPTime(AirTunesClockRef inClock)
{
    AirTunesTime t;

    AirTunesClock_GetSynchronizedTime(inClock, &t);
    return (AirTunesTime_ToNTP(&t));
}

//===========================================================================================================================
//	AirTunesClock_GetUpTicksNearSynchronizedNTPTime
//===========================================================================================================================

uint64_t AirTunesClock_GetUpTicksNearSynchronizedNTPTime(AirTunesClockRef inClock, uint64_t inNTPTime)
{
    uint64_t nowNTP;
    uint64_t ticks;

    nowNTP = AirTunesClock_GetSynchronizedNTPTime(inClock);
    if (inNTPTime >= nowNTP)
        ticks = UpTicks() + NTPtoUpTicks(inNTPTime - nowNTP);
    else
        ticks = UpTicks() - NTPtoUpTicks(nowNTP - inNTPTime);
    return (ticks);
}

//===========================================================================================================================
//	AirTunesClock_GetUpTicksNearSynchronizedNTPTimeMid32
//===========================================================================================================================

uint64_t AirTunesClock_GetUpTicksNearSynchronizedNTPTimeMid32(AirTunesClockRef inClock, uint32_t inNTPMid32)
{
    uint64_t ntpA, ntpB;
    uint64_t ticks;

    ntpA = AirTunesClock_GetSynchronizedNTPTime(inClock);
    ntpB = (ntpA & UINT64_C(0xFFFF000000000000)) | (((uint64_t)inNTPMid32) << 16);
    if (ntpB >= ntpA)
        ticks = UpTicks() + NTPtoUpTicks(ntpB - ntpA);
    else
        ticks = UpTicks() - NTPtoUpTicks(ntpA - ntpB);
    return (ticks);
}

//===========================================================================================================================
//	AirTunesNTPClock_Finalize
//===========================================================================================================================

OSStatus AirTunesNTPClock_Finalize(AirTunesClockRef inClock)
{
    OSStatus err;

    DEBUG_USE_ONLY(err);

    if (inClock) {
        AirTunesNTPClock* ntp = (AirTunesNTPClock*)inClock;
        ntp->running = false;
        if (ntp->threadPtr) {
            err = pthread_join(ntp->thread, NULL);
            check_noerr(err);
            ntp->threadPtr = NULL;
        }

        if (ntp->lockPtr) {
            err = pthread_mutex_destroy(ntp->lockPtr);
            check_noerr(err);
            ntp->lockPtr = NULL;
        }
        free(inClock);
    }
    return (kNoErr);
}

//===========================================================================================================================
//	AirTunesNTPClock_Adjust
//===========================================================================================================================

Boolean AirTunesNTPClock_Adjust(AirTunesClockRef inClock, int64_t inOffsetNanoseconds, Boolean inReset)
{
    AirTunesNTPClock* ntpClock = (AirTunesNTPClock*)inClock;

    if (inReset || ((inOffsetNanoseconds < -kAirTunesClock_MaxPhase) || (inOffsetNanoseconds > kAirTunesClock_MaxPhase))) {
        AirTunesTime at = { 0, 0, 0, 0 };
        uint64_t offset;

        pthread_mutex_lock(ntpClock->lockPtr);
        if (inOffsetNanoseconds < 0) {
            offset = (uint64_t)-inOffsetNanoseconds;
            at.secs = (int32_t)(offset / 1000000000);
            at.frac = (offset % 1000000000) * (UINT64_C(0xFFFFFFFFFFFFFFFF) / 1000000000);
            AirTunesTime_Sub(&ntpClock->epochTime, &at);
        } else {
            offset = (uint64_t)inOffsetNanoseconds;
            at.secs = (int32_t)(offset / 1000000000);
            at.frac = (offset % 1000000000) * (UINT64_C(0xFFFFFFFFFFFFFFFF) / 1000000000);
            AirTunesTime_Add(&ntpClock->epochTime, &at);
        }
        pthread_mutex_unlock(ntpClock->lockPtr);

        _AirTunesClock_Tick(ntpClock);
        inReset = true;
    } else {
        pthread_mutex_lock(ntpClock->lockPtr);

        // Use a phase-lock loop (PLL) to update the time and frequency offset estimates.

        ntpClock->lastOffset = (int32_t)inOffsetNanoseconds;
        Fixed64_SetInteger(ntpClock->offset, ntpClock->lastOffset);

        if (ntpClock->lastAdjustTime != 0) {
            int32_t mtemp;
            Fixed64 ftemp;

            mtemp = ntpClock->second - ntpClock->lastAdjustTime;
            Fixed64_SetInteger(ftemp, ntpClock->lastOffset);
            Fixed64_RightShift(ftemp, (kAirTunesClock_PLLShift + 2) << 1);
            Fixed64_Multiply(ftemp, mtemp);
            Fixed64_Add(ntpClock->frequencyOffset, ftemp);

            if (Fixed64_GetInteger(ntpClock->frequencyOffset) > kAirTunesClock_MaxFrequency) {
                Fixed64_SetInteger(ntpClock->frequencyOffset, kAirTunesClock_MaxFrequency);
            } else if (Fixed64_GetInteger(ntpClock->frequencyOffset) < -kAirTunesClock_MaxFrequency) {
                Fixed64_SetInteger(ntpClock->frequencyOffset, -kAirTunesClock_MaxFrequency);
            }
        }
        ntpClock->lastAdjustTime = ntpClock->second;
        pthread_mutex_unlock(ntpClock->lockPtr);
    }
    return (inReset);
}

//===========================================================================================================================
//	AirTunesNTPClock_GetSynchronizedTime
//===========================================================================================================================

void AirTunesNTPClock_GetSynchronizedTime(AirTunesClockRef inClock, AirTunesTime* outTime)
{
    AirTunesNTPClock* ntpClock = (AirTunesNTPClock*)inClock;

    pthread_mutex_lock(ntpClock->lockPtr);
    *outTime = ntpClock->upTime;
    AirTunesTime_AddFrac(outTime, ntpClock->scale * (UpTicks32() - ntpClock->lastCount));
    AirTunesTime_Add(outTime, &ntpClock->epochTime);
    pthread_mutex_unlock(ntpClock->lockPtr);
}

//===========================================================================================================================
//	AirTunesNTPClock_GetSynchronizedTimeNearUpTicks
//===========================================================================================================================

void AirTunesNTPClock_GetSynchronizedTimeNearUpTicks(AirTunesClockRef inClock, AirTunesTime* outTime, uint64_t inTicks)
{
    uint64_t nowTicks;
    uint32_t nowTicks32;
    uint64_t deltaTicks;
    AirTunesTime deltaTime = { 0, 0, 0, 0 };
    uint64_t scale;
    Boolean future;
    AirTunesNTPClock* ntpClock = (AirTunesNTPClock*)inClock;

    pthread_mutex_lock(ntpClock->lockPtr);
    nowTicks = UpTicks();
    nowTicks32 = ((uint32_t)(nowTicks & UINT32_C(0xFFFFFFFF)));
    future = (inTicks > nowTicks);
    deltaTicks = future ? (inTicks - nowTicks) : (nowTicks - inTicks);

    *outTime = ntpClock->upTime;
    AirTunesTime_AddFrac(outTime, ntpClock->scale * (nowTicks32 - ntpClock->lastCount));
    AirTunesTime_Add(outTime, &ntpClock->epochTime);
    scale = ntpClock->scale;
    pthread_mutex_unlock(ntpClock->lockPtr);

    deltaTime.secs = (int32_t)(deltaTicks / ntpClock->frequency); // Note: unscaled, but delta expected to be < 1 sec.
    deltaTime.frac = (deltaTicks % ntpClock->frequency) * scale;
    if (future)
        AirTunesTime_Add(outTime, &deltaTime);
    else
        AirTunesTime_Sub(outTime, &deltaTime);
}

//===========================================================================================================================
//    AirTunesNTPClock_ConvertNetworkTimeToUpTicks
//===========================================================================================================================

OSStatus AirTunesNTPClock_ConvertNetworkTimeToUpTicks(AirTunesClockRef inClock, AirTunesTime inNetworkTime, uint64_t* outUpTicks)
{
    uint64_t ntpTime = AirTunesTime_ToNTP(&inNetworkTime);
    *outUpTicks = AirTunesClock_GetUpTicksNearSynchronizedNTPTime(inClock, ntpTime);

    return (kNoErr);
}

OSStatus AirTunesNTPClock_AddPeer(AirTunesClockRef inClock, CFStringRef inInterfaceName, const void* inSockAddr)
{
    (void)inClock;
    (void)inInterfaceName;
    (void)inSockAddr;
    return kNoErr;
}

OSStatus AirTunesNTPClock_RemovePeer(AirTunesClockRef inClock, CFStringRef inInterfaceName, const void* inSockAddr)
{
    (void)inClock;
    (void)inInterfaceName;
    (void)inSockAddr;
    return kNoErr;
}

//===========================================================================================================================
//	_AirTunesClock_Tick
//===========================================================================================================================

DEBUG_STATIC void _AirTunesClock_Tick(AirTunesNTPClock* inClock)
{
    uint32_t count;
    uint32_t delta;
    AirTunesTime at = { 0, 0, 0, 0 };

    pthread_mutex_lock(inClock->lockPtr);

    // Update the current uptime from the delta between now and the last update.

    count = UpTicks32();
    delta = count - inClock->lastCount;
    inClock->lastCount = count;
    AirTunesTime_AddFrac(&inClock->upTime, inClock->scale * delta);

    // Perform NTP adjustments each second.

    at = inClock->upTime;
    AirTunesTime_Add(&at, &inClock->epochTime);
    if (at.secs > inClock->lastTime.secs) {
        Fixed64 tickAdjust;

        tickAdjust = inClock->offset;
        Fixed64_RightShift(tickAdjust, kAirTunesClock_PLLShift);
        Fixed64_Sub(inClock->offset, tickAdjust);
        Fixed64_Add(tickAdjust, inClock->frequencyOffset);

        // Recalculate the scaling factor. We want the number of 1/2^64 fractions of a second per period of
        // the hardware counter, taking into account the adjustment factor which the NTP PLL processing
        // provides us with. The adjustment is nanoseconds per second with 32 bit binary fraction and we want
        // 64 bit binary fraction of second:
        //
        //		x = a * 2^32 / 10^9 = a * 4.294967296
        //
        // The range of adjustment is +/- 5000PPM so inside a 64 bit integer we can only multiply by about
        // 850 without overflowing, that leaves no suitably precise fractions for multiply before divide.
        // Divide before multiply with a fraction of 2199/512 results in a systematic undercompensation of
        // 10PPM. On a 5000 PPM adjustment this is a 0.05PPM error. This is acceptable.

        uint64_t scale;

        scale = UINT64_C(1) << 63;
        scale += (tickAdjust / 1024) * 2199;
        scale /= inClock->frequency;
        inClock->scale = scale * 2;
    }

    inClock->second = at.secs;
    inClock->lastTime = at;

    pthread_mutex_unlock(inClock->lockPtr);
}

//===========================================================================================================================
//	_AirTunesClock_Thread
//===========================================================================================================================

DEBUG_STATIC void* _AirTunesClock_Thread(void* inArg)
{
    AirTunesNTPClock* const me = (AirTunesNTPClock*)inArg;

    pthread_setname_np_compat("AirPlayClock");

    while (me->running) {
        _AirTunesClock_Tick(me);
        usleep(10000);
    }
    return (NULL);
}
