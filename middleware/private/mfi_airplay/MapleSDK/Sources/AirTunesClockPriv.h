/*
	Copyright (C) 2007-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
 */
#ifndef AirTunesClockPriv_h
#define AirTunesClockPriv_h

typedef OSStatus (*AirTunesClock_Finialize_f)(AirTunesClockRef inClock);
typedef Boolean (*AirTunesClock_Adjust_f)(AirTunesClockRef inClock, int64_t inOffsetNanoseconds, Boolean inReset);
typedef void (*AirTunesClock_GetSynchronizedNetworkTime_f)(AirTunesClockRef inClock, AirTunesTime* outTime);
typedef void (*AirTunesClock_ConvertUpTicksToNetworkTime_f)(AirTunesClockRef inClock, AirTunesTime* outTime, uint64_t inTicks);
typedef OSStatus (*AirTunesClock_ConvertNetworkTimeToUpTicks_f)(AirTunesClockRef inClock, AirTunesTime inNetworkTime, uint64_t* outUpTicks);
typedef OSStatus (*AirTunesClock_AddPeer_f)(AirTunesClockRef inClock, CFStringRef inInterfaceName, const void* inSockAddr);
typedef OSStatus (*AirTunesClock_RemovePeer_f)(AirTunesClockRef inClock, CFStringRef inInterfaceName, const void* inSockAddr);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		AirTunesClock_vtable
	@abstract	Structure representing functions to be implemented by an AirTunesClock.
 */
typedef struct
{
    AirTunesClock_Finialize_f finalize;
    AirTunesClock_Adjust_f adjust;
    AirTunesClock_GetSynchronizedNetworkTime_f getNetworkTime;
    AirTunesClock_ConvertUpTicksToNetworkTime_f upTicksToNetworkTime;
    AirTunesClock_ConvertNetworkTimeToUpTicks_f netTimeToUpTicks;
    AirTunesClock_AddPeer_f addPeer;
    AirTunesClock_RemovePeer_f removePeer;
} AirTunesClock_vtable;

//---------------------------------------------------------------------------------------------------------------------------
/*!	@struct		AirTunesClockPrivate
	@abstract	The common header for of an AirTunesClock. Implementations can add data after this header.
 */
struct AirTunesClockPrivate {
    AirTunesClockClass class;
    AirTunesClock_vtable vtable;
    CFArrayRef peers;
};

#endif /* AirTunesClockPriv_h */
