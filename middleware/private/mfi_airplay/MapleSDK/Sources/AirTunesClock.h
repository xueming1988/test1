/*
	Copyright (C) 2007-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef __AirTunesClock_h__
#define __AirTunesClock_h__

#include "CommonServices.h"
#include CF_HEADER
#include <math.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark == Constants and Types ==
#endif

// -71970 to -71979
enum {
    kAirTunesClockError_AllocationFailed = -71970,
    kAirTunesClockError_InvalidParameter = -71971,
    kAirTunesClockError_UnsupportedProperty = -71972,
    kAirTunesClockError_InvalidState = -71973,
    kAirTunesClockError_InternalError = -71974,
    kAirTunesClockError_UnsupportedType = -71975,
    kAirTunesClockError_TimelineIDMismatch = -71976,
};

#if 0
#pragma mark == API ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		AirTunesClockClass
 @abstract	The type of clock.
 */
typedef enum {
    kAirTunesClockInvalid = 0,
    kAirTunesClockNTP,
    kAirTunesClockPTP
} AirTunesClockClass;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		AirTunesClockRef
	@abstract	Private structure representing internals for AirTunesClock.
*/
typedef struct AirTunesClockPrivate* AirTunesClockRef;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		AirTunesTime
	@abstract	Structure representing time for AirTunes.
*/

#define AirTunesTimeFlag_Invalid (1 << 0)
#define AirTunesTimeFlag_StateErr (1 << 1)

typedef struct
{
    uint64_t timelineID;
    int32_t secs; //! Number of seconds since 1970-01-01 00:00:00 (Unix time).
    uint64_t frac; //! Fraction of a second in units of 1/2^64.
    uint32_t flags; // AirTunesTimeFlag_Invalid or 0.
} AirTunesTime;

typedef struct
{
    uint8_t priority1;
    uint8_t priority2;
    uint64_t clientDeviceID;
} AirTunesPTPClockSettings;

typedef struct
{
    AirTunesPTPClockSettings ptp;
} AirTunesClockSettings;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesClock_Class
 @abstract	Return the type of the clock.
 */
AirTunesClockClass AirTunesClock_Class(AirTunesClockRef inClock);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesClock_Description
 @abstract	Return a string describing the clock.
 */
const char* AirTunesClock_Description(AirTunesClockRef inClock);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesClock_Create
 @abstract	Create a new instance of a lock/timing engine.
 */
OSStatus AirTunesClock_Create(CFStringRef inTimingProtocol, AirTunesClockSettings* settings, AirTunesClockRef* outRef);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesNTPClock_Create
	@abstract	Create an new instance of a NTP clock/timing engine.
*/
OSStatus AirTunesNTPClock_Create(AirTunesClockRef* outRef);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesClock_Finalize
	@abstract	Finalizes the clock/timing engine.
*/

OSStatus AirTunesClock_Finalize(AirTunesClockRef inClock);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesClock_Adjust
	@abstract	Starts the clock adjustment and discipline process based on the specified clock offset.
	
	@result		true if the clock was stepped. false if slewing.
*/

Boolean AirTunesClock_Adjust(AirTunesClockRef inClock, int64_t inOffsetNanoseconds, Boolean inReset);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesClock_GetSynchronizedNTPTime
	@abstract	Gets the current time, synchronized to the source clock as an NTP timestamp.
*/

uint64_t AirTunesClock_GetSynchronizedNTPTime(AirTunesClockRef inClock);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesClock_GetSynchronizedTime
	@abstract	Gets the current time, synchronized to the source clock.
*/

void AirTunesClock_GetSynchronizedTime(AirTunesClockRef inClock, AirTunesTime* outTime);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesClock_GetSynchronizedTimeNearUpTicks
	@abstract	Gets an estimate of the synchronized time near the specified UpTicks.
*/

void AirTunesClock_GetSynchronizedTimeNearUpTicks(AirTunesClockRef inClock, AirTunesTime* outTime, uint64_t inTicks);

//---------------------------------------------------------------------------------------------------------------------------
/*!    @function    AirTunesClock_kConvertNetworkTimeToUpTicks
 */
OSStatus AirTunesClock_ConvertNetworkTimeToUpTicks(AirTunesClockRef inClock, AirTunesTime inNetworkTime, uint64_t* outUpTicks);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesClock_GetUpTicksNearSynchronizedNTPTime
	@abstract	Gets an estimate of the local UpTicks() near an NTP timestamp on the synchronized timeline.
*/

uint64_t AirTunesClock_GetUpTicksNearSynchronizedNTPTime(AirTunesClockRef inClock, uint64_t inNTPTime);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesClock_GetUpTicksNearSynchronizedNTPTimeMid32
	@abstract	Gets an estimate of the local UpTicks() near the middle 32 bits of an NTP timestamp on the synchronized timeline.
*/

uint64_t AirTunesClock_GetUpTicksNearSynchronizedNTPTimeMid32(AirTunesClockRef inClock, uint32_t inNTPMid32);

OSStatus AirTunesClock_SetPeerList(AirTunesClockRef inClock, CFArrayRef peers);

#if 0
#pragma mark == Utils ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesTime_AddFrac
	@abstract	Adds a fractional seconds (1/2^64 units) value to a time.
*/

STATIC_INLINE void AirTunesTime_AddFrac(AirTunesTime* inTime, uint64_t inFrac)
{
    uint64_t frac;

    frac = inTime->frac;
    inTime->frac = frac + inFrac;
    if (frac > inTime->frac)
        inTime->secs += 1; // Increment seconds on fraction wrap.
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesTime_Add
	@abstract	Adds one time to another time.
*/

STATIC_INLINE void AirTunesTime_Add(AirTunesTime* inTime, const AirTunesTime* inTimeToAdd)
{
    uint64_t frac;

    frac = inTime->frac;
    inTime->frac = frac + inTimeToAdd->frac;
    if (frac > inTime->frac)
        inTime->secs += 1; // Increment seconds on fraction wrap.
    inTime->secs += inTimeToAdd->secs;
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesTime_Sub
	@abstract	Subtracts one time from another time.
*/

STATIC_INLINE void AirTunesTime_Sub(AirTunesTime* inTime, const AirTunesTime* inTimeToSub)
{
    uint64_t frac;

    frac = inTime->frac;
    inTime->frac = frac - inTimeToSub->frac;
    if (frac < inTime->frac)
        inTime->secs -= 1; // Decrement seconds on fraction wrap.
    inTime->secs -= inTimeToSub->secs;
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesTime_ToFP
	@abstract	Converts an AirTunesTime to a floating-point seconds value.
*/

STATIC_INLINE double AirTunesTime_ToFP(const AirTunesTime* inTime)
{
    return (((double)inTime->secs) + (((double)inTime->frac) * (1.0 / 18446744073709551615.0)));
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesTime_FromFP
	@abstract	Converts a floating-point seconds value to an AirTunesTime.
*/

STATIC_INLINE void AirTunesTime_FromFP(AirTunesTime* inTime, double inFP)
{
    double secs;
    double frac;

    secs = floor(inFP);
    frac = inFP - secs;
    frac *= 18446744073709551615.0;

    inTime->secs = (int32_t)secs;
    inTime->frac = (uint64_t)frac;
    inTime->flags = 0;
}

//---------------------------------------------------------------------------------------------------------------------------
//	@function	AirTunesTime_ToUInt64/FromUInt64
//	@abstract	Converts between an AirTunesTime struct and one uint64_t value.
//				The implementation assumes that the unit is not much smaller than nano-second

STATIC_INLINE uint64_t AirTunesTime_ToUInt64(const AirTunesTime* inTime, uint64_t perSec)
{
    return inTime->secs * perSec + ((perSec * (inTime->frac >> 32)) >> 32);
}

STATIC_INLINE void AirTunesTime_FromUInt64(AirTunesTime* inTime, uint64_t inVal, uint64_t perSec)
{
    double secs = (double)inVal / perSec;
    AirTunesTime_FromFP(inTime, secs);
}

#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC 1000000000ull
#endif
STATIC_INLINE uint64_t AirTunesTime_ToNanoseconds(const AirTunesTime* inTime)
{
    return (AirTunesTime_ToUInt64(inTime, NSEC_PER_SEC));
}

STATIC_INLINE void AirTunesTime_FromNanoseconds(AirTunesTime* inTime, uint64_t inVal)
{
    AirTunesTime_FromUInt64(inTime, inVal, NSEC_PER_SEC);
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesTime_ToNTP
	@abstract	Converts an AirTunesTime to a 64-bit NTP timestamp.
*/

#define AirTunesTime_ToNTP(AT) ((((uint64_t)(AT)->secs) << 32) | ((AT)->frac >> 32))

//---------------------------------------------------------------------------------------------------------------------------
/*! @function    AirTunesTime_Invalidate
    @abstract    Marks the time as invalid.
 */
STATIC_INLINE void AirTunesTime_Invalidate(AirTunesTime* inTime, uint32_t flags)
{
    inTime->timelineID = 0;
    inTime->secs = 0;
    inTime->frac = 0;
    inTime->flags = flags;
}

//---------------------------------------------------------------------------------------------------------------------------
/*! @function    AirTunesTime_IsValid
    @abstract    Return true if the time is valid.
 */
STATIC_INLINE Boolean AirTunesTime_IsValid(const AirTunesTime* inTime)
{
    return !(inTime->flags & AirTunesTimeFlag_Invalid);
}

//---------------------------------------------------------------------------------------------------------------------------
/*! @function    AirTunesTime_IsValid
    @abstract    Return true if the time is valid.
 */
STATIC_INLINE Boolean AirTunesTimelines_Equal(const AirTunesTime time1, const AirTunesTime time2)
{
    return time1.timelineID == time2.timelineID && time1.flags == time2.flags;
}

#ifdef __cplusplus
}
#endif

#endif // __AirTunesClock_h__
