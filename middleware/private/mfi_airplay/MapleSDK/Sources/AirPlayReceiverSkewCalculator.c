/*
 Copyright (C) 2018 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
 */

#include "CommonServices.h"
#include "DebugServices.h"
#include "TimeUtils.h"

#include "AirPlayReceiverSkewCalculator.h"

extern Boolean kPerformSkewAdjustRightAway;

ulog_define(AirPlayReceiverSkew, kLogLevelNotice, kLogFlags_Default, "AirPlaySkew", NULL);
#define aps_ucat() &log_category_from_name(AirPlayReceiverSkew)
#define aps_ulog(LEVEL, ...) ulog(aps_ucat(), (LEVEL), __VA_ARGS__)

static double
audioSessionBufferedSkewCalculation_getFracsOfDouble(double inDouble)
{
    double dummyIntPart = 0;
    return (modf(inDouble, &dummyIntPart));
}

static uint32_t
audioSessionBufferedSkewCalculation_doubleToUInt32(
    double inDoubleVal)
{
    int64_t int64Val = (int64_t)inDoubleVal;
    uint64_t uint64Val = (uint64_t)int64Val;
    uint32_t uint32Val = (uint32_t)uint64Val;

    return uint32Val;
}

static double
audioSessionBufferedSkewCalculation_uint32ToDouble(
    uint32_t inUInt32Val)
{
    int64_t int64Val = (int64_t)inUInt32Val;
    double doubleVal = (double)int64Val;

    return doubleVal;
}

OSStatus
AirPlayReceiverSkewCalculation(AirPlayReceiverSkewCalculationContext* inContext,
    AirTunesTime inNetworkTime,
    uint32_t inLocalSampleTime)
{
    OSStatus err = kNoErr;
    AirTunesTime networkTimeElapsedSinceAnchor = { 0, 0, 0, 0 }; // Network Time that has elapsed since anchorTime
    double rtpSamplesWeShouldHaveConsumedDouble = 0; // RTP Samples we should have consuemd over that period. double to limit rounding errors.
    uint32_t rtpSamplesWeShouldHaveConsumedU32 = 0; // RTP Samples we should have consuemd over that period.
    uint32_t localRTPTimeU32 = 0; // RTP TimeStamp of where this local device is
    uint32_t rtpTimeWhereWeShouldBeU32 = 0; // RTP TimeStamp of where this local device should be if it was true to the network clock
    double rtpTimeWhereWeShouldBeFracs = 0; // Fraction of the RTP TimeStamp of where this local device should be if it was true to the network clock
    uint32_t howManySamplesWeAreOffU32 = 0; // Difference between rtpTimeWhereWeShouldBe and localRTPTime
    double samplesToSkewDouble = 0; // Samples the platform should skew to get synchronized with network time
    uint32_t newRTPOffsetActive = 0;
    Boolean stutterCreditPending = false;

    //networkTimeElapsedSinceAnchor = APSNetworkTime_Sub( inNetworkTime, inContext->input.anchorNetworkTime );
    networkTimeElapsedSinceAnchor = inNetworkTime;
    AirTunesTime_Sub(&networkTimeElapsedSinceAnchor, &inContext->input.anchorNetworkTime);

    // [1] Calculate how many samples should have been consumed over that period

    //rtpSamplesWeShouldHaveConsumedDouble = APSNetworkTime_ToFP( &networkTimeElapsedSinceAnchor ) * inContext->input.sampleRate;
    rtpSamplesWeShouldHaveConsumedDouble = AirTunesTime_ToFP(&networkTimeElapsedSinceAnchor) * inContext->input.sampleRate;

    rtpSamplesWeShouldHaveConsumedU32 = audioSessionBufferedSkewCalculation_doubleToUInt32(rtpSamplesWeShouldHaveConsumedDouble);

    // [2] Where should we be in RTP Time?

    rtpTimeWhereWeShouldBeU32 = inContext->input.anchorRTPTime + rtpSamplesWeShouldHaveConsumedU32;
    rtpTimeWhereWeShouldBeFracs = inContext->input.anchorRTPTimeFracs + audioSessionBufferedSkewCalculation_getFracsOfDouble(rtpSamplesWeShouldHaveConsumedDouble);
    if (rtpTimeWhereWeShouldBeFracs > 1) {
        rtpTimeWhereWeShouldBeU32 += 1;
        rtpTimeWhereWeShouldBeFracs -= 1.;
    } else if (rtpTimeWhereWeShouldBeFracs < 0) {
        rtpTimeWhereWeShouldBeU32 -= 1;
        rtpTimeWhereWeShouldBeFracs += 1.;
    }
    check(0 <= rtpTimeWhereWeShouldBeFracs);
    check(rtpTimeWhereWeShouldBeFracs < 1.);

    // [3] But the audio hardware is actually at inLocalSampleTime. How many samples off is it?

    localRTPTimeU32 = inLocalSampleTime - inContext->input.rtpOffsetActive;

    howManySamplesWeAreOffU32 = Mod32_Diff(rtpTimeWhereWeShouldBeU32, localRTPTimeU32); //rb
    aps_ulog(kLogLevelTrace, "Process SetRate Skew: (localRTP = %u) - (shouldBeRTP = %u) == (sampleOff = %u)\n",
        localRTPTimeU32, rtpTimeWhereWeShouldBeU32, howManySamplesWeAreOffU32);

    if (howManySamplesWeAreOffU32 >= inContext->input.rtpOffsetResetThreshold) {
        inContext->output.rtpExcessiveDriftCount = inContext->input.rtpExcessiveDriftCount + 1;
    } else {
        inContext->output.rtpExcessiveDriftCount = 0;
    }

    Boolean isDriftCountExceed = (inContext->output.rtpExcessiveDriftCount >= inContext->input.rtpMaxExcessiveDriftCount);

    if (isDriftCountExceed || inContext->input.forceOffsetRecalculation || kPerformSkewAdjustRightAway) {               
        inContext->output.rtpExcessiveDriftCount = 0;

        aps_ulog(kLogLevelNotice, "SkewNow:%u isForce:%u isDriftCountExceed:%u\n", kPerformSkewAdjustRightAway, inContext->input.forceOffsetRecalculation, isDriftCountExceed);
    
        // [3a] If we're way off, slam the rtpOffset to the offset and stop skewing

        newRTPOffsetActive = inLocalSampleTime - rtpTimeWhereWeShouldBeU32;

        if(isDriftCountExceed && (!kPerformSkewAdjustRightAway) && (!inContext->input.forceOffsetRecalculation)) {
            aps_ulog(kLogLevelNotice, "Too many samples to skew: %u\n", howManySamplesWeAreOffU32);
        }
        
        kPerformSkewAdjustRightAway = false;//reset flag
        aps_ulog(kLogLevelNotice, "Update rtpOffsetActive: %u (%u - %u)\n",
            newRTPOffsetActive, inLocalSampleTime, rtpTimeWhereWeShouldBeU32);

        samplesToSkewDouble = 0.;
        stutterCreditPending = true;
    } else {
        // [3b] Else, proceed with the skew...
        // Note that since we're using a double we won't wrap at 32-bit boundaries, but this is ok for the samplesToSkew
        // calculation.

        double localRTPTimeDouble = 0;
        double anchorRTPDouble = 0;
        double rtpTimeWhereWeShouldBeDouble = 0;
        double howManySamplesWeAreBehindDouble = 0;

        localRTPTimeDouble = audioSessionBufferedSkewCalculation_uint32ToDouble(localRTPTimeU32);
        anchorRTPDouble = audioSessionBufferedSkewCalculation_uint32ToDouble(inContext->input.anchorRTPTime);

        rtpTimeWhereWeShouldBeDouble = fmod(anchorRTPDouble + inContext->input.anchorRTPTimeFracs + rtpSamplesWeShouldHaveConsumedDouble, ((uint64_t)1) << 32);
        howManySamplesWeAreBehindDouble = rtpTimeWhereWeShouldBeDouble - localRTPTimeDouble;

        samplesToSkewDouble = howManySamplesWeAreBehindDouble;

        aps_ulog(kLogLevelTrace, "Samples to Skew: %1.3f\n", samplesToSkewDouble);

        newRTPOffsetActive = inContext->input.rtpOffsetActive;
    }

    // [4] Set our output variables

    inContext->output.stutterCreditPending = stutterCreditPending;
    inContext->output.rtpOffsetActive = newRTPOffsetActive;
    inContext->output.rtpTimeWhereWeShouldBeU32 = rtpTimeWhereWeShouldBeU32;
    inContext->output.rtpTimeWhereWeShouldBeFracs = rtpTimeWhereWeShouldBeFracs;
    inContext->output.samplesToSkewDouble = samplesToSkewDouble;

    return (err);
}
