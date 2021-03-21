/*
	Copyright (C) 2018 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef CRamstadASRC_h
#define CRamstadASRC_h

#include <stdint.h>

// The state struct for the resampler.
struct CRamstadASRC {
    int mWholeTickRate;
    int mFracTickRate;
    int mFracTicks;
    int64_t mTickRate64;

    int mNumInputsBeforeOutput;
    double mFracScale;

    float mW1[16];
    float mW2[16];
};

// initialize a resampler state. Can also be used to reset and reuse. Does not allocate, so there is no need for a dispose call.
struct CRamstadASRC* CRamstadASRC_init(struct CRamstadASRC* r);

// set the rate before calling inputSamplesForOutputSamples or outputSamplesForInputSamples
void CRamstadASRC_setRate(struct CRamstadASRC* r,
    double rate); // rate is expressed as input_samples consumed per output_samples produced. It should be very close to 1.0.

// returns the number of input samples that can are needed to produce inNumOutputSamples of output.
int CRamstadASRC_inputSamplesForOutputSamples(struct CRamstadASRC* r,
    int inNumOutputSamples);

// returns the number of output samples that can be produced from inNumInputSamples of input.
int CRamstadASRC_outputSamplesForInputSamples(struct CRamstadASRC* r,
    int inNumInputSamples, // The number of input samples available.
    int* outActualInputNeeded // It is possible that fewer input samples could have been used to return this same amount output. outActualInputNeeded will reflect the minimum number of input samples that could have been passed to return this many output samples.
    );

// process a buffer
int CRamstadASRC_processStereoInt16(struct CRamstadASRC* r, // The state struct for the resampler.
    int16_t const* inL, // left channel input
    int16_t const* inR, // right channel input
    int16_t* outL, // left channel output
    int16_t* outR, // right channel output
    int inNumInputSamples, // number of input samples, as determined from a previous call to inputSamplesForOutputSamples or outputSamplesForInputSamples
    int inNumOutputSamples, // number of output samples, as determined from a previous call to inputSamplesForOutputSamples or outputSamplesForInputSamples
    int inInputStride, // 1 for deinterleaved, 2 for interleaved
    int inOutputStride // 1 for deinterleaved, 2 for interleaved
    );

#endif /* CRamstadSRC_h */
