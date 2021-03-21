/*
	Copyright (C) 2018 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
 */

#ifndef AirTunredPTPClock_h
#define AirTunredPTPClock_h

#include "AirTunesClock.h"
#include "CommonServices.h"

#include CF_HEADER

#if 0
#pragma mark == API ==
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirTunesPTPClock_Create
 @abstract	Create an instance of a PTP clock.
 */
OSStatus AirTunesPTPClock_Create(AirTunesPTPClockSettings* ptpSettings, AirTunesClockRef* outRef);

#endif /* AirTunredPTPClock_h */
