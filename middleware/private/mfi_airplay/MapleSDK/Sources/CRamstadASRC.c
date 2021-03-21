/*
	Copyright (C) 2018 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "CRamstadASRC.h"
#include "CommonServices.h"
#include <pthread.h>
#include <string.h>
#if (TARGET_HAS_STD_C_LIB)
#include <math.h>
#endif

#define kNumStages 8
#define kFracTableBits 8
#define kNumFracOffsets (1 << kFracTableBits)
#define fracIndexMask (kNumFracOffsets - 1)
#define fracShift (31 - kFracTableBits)
#define fracMask ((1L << fracShift) - 1)
#define kTwoToThe31 2147483648.0
#define CLAMP(X, LO, HI) MAX(LO, MIN(HI, X))

enum {
    kReal = 0,
    kImag,
};

static const double kRamstadASRCPoles16[kNumStages][2] = {
    { -0.010580231272200, 1.015854368884259 },
    { -0.034631443416635, 1.004707604377318 },
    { -0.067784519454247, 0.978934260136594 },
    { -0.117153641693031, 0.930592990799070 },
    { -0.189578684805366, 0.845511562425358 },
    { -0.286310245434279, 0.702340823339251 },
    { -0.391794780477086, 0.478435699044166 },
    { -0.465881840945613, 0.171899068040505 },
};

static const double kRamstadASRCResidues16[kNumStages][2] = {
    { -0.000188058111223, 0.014718756821565 },
    { -0.052403953725020, -0.033640521289266 },
    { 0.151898343298679, -0.070433625501297 },
    { -0.068070248949139, 0.348152704659309 },
    { -0.403800558560144, -0.506399577019729 },
    { 1.041756966732024, 0.114617671749445 },
    { -1.225494722624338, 0.844596719548321 },
    { 0.556296006676991, -1.710362968124224 },
};

struct CRamstadKernel {
    float mFrontEndCoeffs[16];
    float mBackEndCoeffs[16 * (kNumFracOffsets + 1)];
};

static struct CRamstadKernel gCRamstadKernel;

static void ramstadFrontEndStereo16Pole(float L, float R, float const* a1, float const* a2, float* w1, float* w2)
{
    // a1 and a2 are the same for both channels.
    const int n = kNumStages;
    int i;
    for (i = 0; i < n; ++i) {
        float w0 = L + a1[i] * w1[i] + a2[i] * w2[i];
        w2[i] = w1[i];
        w1[i] = w0;

        w0 = R + a1[i] * w1[i + n] + a2[i] * w2[i + n];
        w2[i + n] = w1[i + n];
        w1[i + n] = w0;
    }
}

static void ramstadBackEndStereo16Pole(float const* b1, float const* b2, float* w1, float* w2, float* L, float* R)
{
    float Lsum = 0.f;
    float Rsum = 0.f;
    const int n = kNumStages;
    int i;
    for (i = 0; i < n; ++i) {
        Lsum += b1[i] * w1[i] + b2[i] * w2[i];
        Rsum += b1[i] * w1[i + n] + b2[i] * w2[i + n];
    }
    *L = Lsum;
    *R = Rsum;
}

int CRamstadASRC_processStereoInt16(struct CRamstadASRC* r,
    int16_t const* inL,
    int16_t const* inR,
    int16_t* outL,
    int16_t* outR,
    int inNumInputSamples,
    int inNumOutputSamples,
    int inInputStride,
    int inOutputStride)
{
    const int wholeTickRate = r->mWholeTickRate;
    const int fracTickRate = r->mFracTickRate;
    int fracTicks = r->mFracTicks;
    int numInputsBeforeOutput = r->mNumInputsBeforeOutput;
    int inputConsumed = 0;

    const float* frontEndCoeffs = gCRamstadKernel.mFrontEndCoeffs;
    const float* backEndCoeffs = gCRamstadKernel.mBackEndCoeffs;
    float* w1 = r->mW1;
    float* w2 = r->mW2;

    const int n = kNumStages;
    const float* a1 = frontEndCoeffs;
    const float* a2 = a1 + n;
    int i;
    for (i = 0; i < inNumOutputSamples; ++i) {
        int j;
        for (j = 0; j < numInputsBeforeOutput; ++j) {
            ramstadFrontEndStereo16Pole((float)*inL, (float)*inR, a1, a2, w1, w2);
            inL += inInputStride;
            inR += inInputStride;
        }
        inputConsumed += numInputsBeforeOutput;

        const int index = (fracTicks >> fracShift) & fracIndexMask;
        const int index1 = index + 1;
        float frac = (double)(fracTicks & fracMask) * r->mFracScale;

        const float* b1a = backEndCoeffs + index * 2 * n;
        const float* b2a = b1a + n;
        const float* b1b = backEndCoeffs + index1 * 2 * n;
        const float* b2b = b1b + n;

        float outLA, outLB, outRA, outRB;
        ramstadBackEndStereo16Pole(b1a, b2a, w1, w2, &outLA, &outRA);
        ramstadBackEndStereo16Pole(b1b, b2b, w1, w2, &outLB, &outRB);
        int outL_i = (int)floor(outLA + frac * (outLB - outLA) + .5f);
        int outR_i = (int)floor(outRA + frac * (outRB - outRA) + .5f);
        *outL = (int16_t)CLAMP(outL_i, -32768, 32767);
        *outR = (int16_t)CLAMP(outR_i, -32768, 32767);
        outL += inOutputStride;
        outR += inOutputStride;

        fracTicks += fracTickRate; //	Undefined behavior, rely on an overflow to generate a negative value
        numInputsBeforeOutput = wholeTickRate;
        if (fracTicks < 0) {
            ++numInputsBeforeOutput;
            fracTicks &= INT_MAX; //	Get its absolute value
        }
    }
    int unconsumedInput = inNumInputSamples - inputConsumed;
    if (unconsumedInput) {
        int excessInput = MIN(unconsumedInput, numInputsBeforeOutput); // this is the input that can be fed into the front end filter.
        int j;
        for (j = 0; j < excessInput; ++j) {
            ramstadFrontEndStereo16Pole((float)*inL, (float)*inR, a1, a2, w1, w2);
            inL += inInputStride;
            inR += inInputStride;
        }
        numInputsBeforeOutput -= excessInput;
        inputConsumed += excessInput;
    }

    r->mFracTicks = fracTicks;
    r->mNumInputsBeforeOutput = numInputsBeforeOutput;
    return inputConsumed;
}

struct RamstadIntermediateCoeffs {
    double zeroMag2[8];
    double zeroAngle[8];
    double poleReal[8];
    double poleImag[8];
    double poleRealExp[8];
};

static void ramstadPrecalculateCoeffs(
    double passbandWidth,
    struct RamstadIntermediateCoeffs* ric,
    float* a1,
    float* a2)
{
    double aRamstadASRCImpulseInvariantTransformFactor = M_PI * passbandWidth;
    int i;

    for (i = 0; i < kNumStages; i++) {
        double poleRealT = kRamstadASRCPoles16[i][kReal] * aRamstadASRCImpulseInvariantTransformFactor;
        double poleImagT = kRamstadASRCPoles16[i][kImag] * aRamstadASRCImpulseInvariantTransformFactor;

        a1[i] = 2.0 * exp(poleRealT) * cos(poleImagT);
        a2[i] = -exp(2.0 * poleRealT);

        double zeroRealT = kRamstadASRCResidues16[i][kReal] * aRamstadASRCImpulseInvariantTransformFactor;
        double zeroImagT = kRamstadASRCResidues16[i][kImag] * aRamstadASRCImpulseInvariantTransformFactor;

        ric->zeroMag2[i] = 2.0 * hypot(zeroRealT, zeroImagT);
        ric->zeroAngle[i] = atan2(zeroImagT, zeroRealT);
        ric->poleReal[i] = poleRealT;
        ric->poleRealExp[i] = exp(poleRealT);
        ric->poleImag[i] = poleImagT;
    }
}

static void ramstadBackEndCoeffs(
    double alpha,
    struct RamstadIntermediateCoeffs const* ric,
    float* b1,
    float* b2)
{
    int i;
    for (i = 0; i < kNumStages; ++i) {
        double K = (ric->zeroMag2[i] * exp(ric->poleReal[i] * alpha));
        double D = cos(ric->zeroAngle[i] + (ric->poleImag[i] * alpha));
        double E = -ric->poleRealExp[i] * cos(ric->zeroAngle[i] - ric->poleImag[i] + (ric->poleImag[i] * alpha));
        b1[i] = K * D;
        b2[i] = K * E;

        //printf("alpha %12.6f  stage %2d   b1 %12.6f   b2 %12.6f   re %12.6f   im %12.6f   mag %12.6f ang %12.6f exp %12.6f\n", alpha, i, K*D, K*E, ric->poleReal[i], ric->poleImag[i], ric->zeroMag2[i], ric->zeroAngle[i], ric->poleRealExp[i]);
    }
}

static void CRamstadKernel_init(void)
{
    double maxRateDeviation = .001;
    double filterScale = 1. - maxRateDeviation;
    double passbandWidth = 0.90702947845805 * filterScale;

    struct RamstadIntermediateCoeffs ric;

    int numOffsetsFrac = kNumFracOffsets + 1;
    double alphaStep = 1. / (double)kNumFracOffsets;

    memset(&gCRamstadKernel, 0, sizeof(struct CRamstadKernel));

    const int n = kNumStages;
    float* a1 = gCRamstadKernel.mFrontEndCoeffs;
    float* a2 = a1 + n;
    ramstadPrecalculateCoeffs(passbandWidth, &ric, a1, a2);

    float* b1 = gCRamstadKernel.mBackEndCoeffs;
    float* b2 = b1 + n;
    double alpha = 0.;
    int i;
    for (i = 0; i < numOffsetsFrac; ++i) {
        ramstadBackEndCoeffs(alpha, &ric, b1, b2);
        b1 += 2 * n;
        b2 += 2 * n;
        alpha += alphaStep;
    }
}

void CRamstadASRC_setRate(struct CRamstadASRC* r, double rate)
{
    double wholeRate = floor(rate);
    double fracRate = rate - wholeRate;
    r->mWholeTickRate = (int)wholeRate;
    r->mFracTickRate = (int)floor(kTwoToThe31 * fracRate);
    r->mTickRate64 = ((int64_t)r->mWholeTickRate << 31) + (int64_t)r->mFracTickRate;
}

struct CRamstadASRC* CRamstadASRC_init(struct CRamstadASRC* r)
{
    memset(r, 0, sizeof(struct CRamstadASRC));

    r->mFracScale = pow(2., -fracShift);
    //r->mFracTicks = 0; done by memset
    r->mNumInputsBeforeOutput = 1;

    CRamstadASRC_setRate(r, 1.);

    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, CRamstadKernel_init);

    return r;
}

int CRamstadASRC_inputSamplesForOutputSamples(struct CRamstadASRC* r, int inNumOutputSamples)
{
    if (inNumOutputSamples <= 0)
        return 0;

    return r->mNumInputsBeforeOutput + (int)(((inNumOutputSamples - 1) * r->mTickRate64 + (int64_t)r->mFracTicks) >> 31);
}

int CRamstadASRC_outputSamplesForInputSamples(struct CRamstadASRC* r, int inNumInputSamples, int* outActualInputNeeded)
{
    if (inNumInputSamples < r->mNumInputsBeforeOutput) {
        if (outActualInputNeeded)
            *outActualInputNeeded = 0;
        return 0;
    }

    // we can only estimate here because ceil(a/b) is not an exact inverse of floor(a*b).
    int outputProduced;
    int samplesAvailable = inNumInputSamples - r->mNumInputsBeforeOutput;

    int64_t ticksAvailable = ((int64_t)samplesAvailable << 31) + (int64_t)r->mFracTicks;
    outputProduced = 1 + (int)((ticksAvailable + r->mTickRate64 - 1) / r->mTickRate64);

    // we may have overestimated, so need to check against inputSamplesForOutputSamples.
    while (outputProduced > 0) {
        int inputNeeded = CRamstadASRC_inputSamplesForOutputSamples(r, outputProduced);
        if (inputNeeded <= inNumInputSamples) {
            if (outActualInputNeeded)
                *outActualInputNeeded = inputNeeded;
            return outputProduced;
        }
        --outputProduced;
    }
    return outputProduced;
}
