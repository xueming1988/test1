/*
 * Copyright (C) Linkplay Technology - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * <huaijing.wang@linkplay.com>, june. 2020
 *
 * Linkplay skew  source files
 *
 */

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <stdint.h>


#include "cramstadasrc.h"
typedef unsigned char uchar;

// ==== MIN / MAX ====
//---------------------------------------------------------------------------------------------------------------------------
/*! @function   Min
    @abstract   Returns the lesser of X and Y.
*/
#if (!defined(Min))
#define Min(X, Y) (((X) < (Y)) ? (X) : (Y))
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*! @function   Max
    @abstract   Returns the greater of X and Y.
*/
#if (!defined(Max))
#define Max(X, Y) (((X) > (Y)) ? (X) : (Y))
#endif

#define Clamp(x, a, b) Max((a), Min((b), (x)))

typedef struct {
    double pGain;      // Proportional Gain.
    double iState;     // Integrator state.
    double iMin, iMax; // Integrator limits.
    double iGain;      // Integrator gain (always less than 1).
    double dState;     // Differentiator state.
    double dpGain;     // Differentiator filter gain = 1 - pole.
    double dGain;      // Derivative gain.

} PIDContext;

struct _lp_skew {
    struct CRamstadASRC resampler; // resampler state.
    PIDContext PIDController;
    int bytes_per_sample;
    int channel;
    int sampleRate;
    uchar *skewAdjustBuffer; // Temporary buffer for doing skew compensation.
    int skewAdjustBufferSize;
};

void lp_skew_PIDInit(void *p,
                     double pGain,
                     double iGain,
                     double dGain,
                     double dPole,
                     double iMin,
                     double iMax)
{
    struct _lp_skew *skew = (struct _lp_skew *)p;
    PIDContext *inPID = &skew->PIDController;

    inPID->iState = 0.0;
    inPID->iMax = iMax;
    inPID->iMin = iMin;
    inPID->iGain = iGain;

    inPID->dState = 0.0;
    inPID->dpGain = 1.0 - dPole;
    inPID->dGain = dGain * (1.0 - dPole);

    inPID->pGain = pGain;
}

//===========================================================================================================================
//	PIDUpdate
//===========================================================================================================================

double lp_skew_PIDUpdate(void *p, double input)
{
    struct _lp_skew *skew = (struct _lp_skew *)p;
    PIDContext *inPID = &skew->PIDController;

    double output;
    double dTemp;
    double pTerm;

    pTerm = input * inPID->pGain; // Proportional.

    // Update the integrator state and limit it.

    inPID->iState = inPID->iState + (inPID->iGain * input);
    output = inPID->iState + pTerm;
    if (output > inPID->iMax) {
        inPID->iState = inPID->iMax - pTerm;
        output = inPID->iMax;
    } else if (output < inPID->iMin) {
        inPID->iState = inPID->iMin - pTerm;
        output = inPID->iMin;
    }

    // Update the differentiator state.

    dTemp = input - inPID->dState;
    inPID->dState += inPID->dpGain * dTemp;
    output += dTemp * inPID->dGain;
    return (output);
}
void lp_skew_PIDReset(void *p)
{
    struct _lp_skew *skew = (struct _lp_skew *)p;
    PIDContext *inPID = &skew->PIDController;

    inPID->iState = 0;
    inPID->dState = 0;
}
void *lp_skew_init(int channel, int bytes_per_sample, int sampleRate)
{
    struct _lp_skew *skew = (struct _lp_skew *)malloc(sizeof(struct _lp_skew));
    CRamstadASRC_init(&skew->resampler);
    skew->channel = channel;
    skew->bytes_per_sample = bytes_per_sample;
    skew->sampleRate = sampleRate;
    skew->skewAdjustBufferSize = 0;
    skew->skewAdjustBuffer = NULL;
    lp_skew_PIDInit(skew, 0.7, 0.2, 1.2, 0.97, -5, 5); // init default PID

    return skew;
}

void lp_skew_deinit(void *p)
{
    struct _lp_skew *skew = (struct _lp_skew *)p;
    if (skew->skewAdjustBuffer)
        free(skew->skewAdjustBuffer);
    free(skew);
}

int lp_do_skew(void *p, uchar *data, int samples, int skewAdjustSample, uchar **outdata)
{
    struct _lp_skew *skew = (struct _lp_skew *)p;

    int numInputSamples = samples;
    int numOutputSamples = samples;
    // compute rate of input samples to output samples.
    if (skewAdjustSample) {
        int bytes_per_sample = skew->bytes_per_sample;
        double sampleRate = (double)skew->sampleRate;
        // 1000 seconds to consume all skew. That is what the add/drop code appears to be doing.
        // equivalent to ((sampleRate) +/- kAirTunesMaxSkewAdjustRate(sampleRate)) / (sampleRate).
        double rate = 1. + (.001 * skewAdjustSample) / sampleRate;

        rate = Clamp(rate, .99, 1.01);
        CRamstadASRC_setRate(&skew->resampler, rate); // set the resampler rate

        // calculate the number of output samples that can be produced from the input.
        numInputSamples = samples;
        numOutputSamples =
            CRamstadASRC_outputSamplesForInputSamples(&skew->resampler, numInputSamples, NULL);
        //       printf("numInputSamples %d numOutputSamples %d
        //       \r\n",numInputSamples,numOutputSamples);
        if (skew->skewAdjustBufferSize < numOutputSamples * bytes_per_sample) {
            skew->skewAdjustBufferSize = numOutputSamples * bytes_per_sample;
            skew->skewAdjustBuffer =
                (uchar *)realloc(skew->skewAdjustBuffer, skew->skewAdjustBufferSize);
        }
        // get pointers to the interleaved channels.
        const short *inL = (short *)data;
        const short *inR = inL + 1;
        short *outL = (short *)skew->skewAdjustBuffer;
        short *outR = outL + 1;

        // do the resampling.
        CRamstadASRC_processStereoInt16(
            &skew->resampler, inL, inR, outL, outR, numInputSamples, numOutputSamples, 2, 2);
        *outdata = skew->skewAdjustBuffer;
    }
    return numOutputSamples;
}
