/*
 Copyright (C) 2018 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
 */

#ifndef AirPlayReceiverSkewCalculator_h
#define AirPlayReceiverSkewCalculator_h

#include "AirTunesClock.h"
#include "CommonServices.h"

typedef struct
{
    struct {
        Boolean forceOffsetRecalculation;
        uint32_t rtpOffsetResetThreshold; // Threshold of samples to skew after which the RTP offset is reset.
        AirTunesTime anchorNetworkTime; // AnchorTime in NetworkTime
        uint32_t anchorRTPTime; // AnchorTime in RTPTime
        double anchorRTPTimeFracs; // Fractions of the samples of the above
        uint32_t sampleRate;
        uint32_t rtpOffsetActive;
        int rtpExcessiveDriftCount;
        int rtpMaxExcessiveDriftCount;
    } input;

    struct {
        uint32_t rtpOffsetActive;
        uint32_t rtpTimeWhereWeShouldBeU32;
        double rtpTimeWhereWeShouldBeFracs;
        double samplesToSkewDouble;
        int rtpExcessiveDriftCount;
        Boolean stutterCreditPending;
    } output;

} AirPlayReceiverSkewCalculationContext;

OSStatus
AirPlayReceiverSkewCalculation(
    AirPlayReceiverSkewCalculationContext* inContext,
    AirTunesTime inNetworkTime,
    uint32_t inLocalSampleTime);

#endif /* AirPlayReceiverSkewCalculator_h */
