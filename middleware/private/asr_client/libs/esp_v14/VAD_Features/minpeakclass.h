/*
 * Copyright 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * You may not use this file except in compliance with the terms and conditions set forth in the accompanying LICENSE.TXT file.
 *
 * THESE MATERIALS ARE PROVIDED ON AN "AS IS" BASIS.  AMAZON SPECIFICALLY DISCLAIMS, WITH RESPECT TO THESE MATERIALS, ALL
 * WARRANTIES, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, AND NON-INFRINGEMENT.
 */

#ifndef MINPEAKCLASS_H
#define MINPEAKCLASS_H

#define MIN_VAL(a,b) (((a)<(b))?(a):(b))
#define MAX_VAL(a,b) (((a)>(b))?(a):(b))

#include "../common/fixed_pt.h"
#include "./flagbuffer.h"

class MinPeakClass: public FLAGBuffer
{
public:
    Word64 m_Debug_1;
    Word64 m_Debug_2;
    Word64 m_Debug_3;

private:

    // raw Averaging time constant
    static const Word16 k_ALPHA;
    static const Word16 k_ONE_MINUS_ALPHA;

    // Attack time constant
    static const Word16 k_ATTACK;
    static const Word16 k_ONE_MINUS_ATTACK;

    // Release time constant
    static const Word16 k_MIN_RELEASE;
    static const Word16 k_ONE_MINUS_MIN_RELEASE;

    // Attack time constant for peak tracking
    static const Word16 k_PEAK_ATTACK;
    static const Word16 k_ONE_MINUS_PEAK_ATTACK;


    // Release time constant for peak tracking
    static const Word16 k_PEAK_RELEASE;
    static const Word16 k_ONE_MINUS_PEAK_RELEASE;


    //Attack and Release TCs for ensemble estimation
    // fast update
    static const Word16 k_EST_FAST_ATTACK;
    static const Word16 k_ONE_MINUS_EST_FAST_ATTACK;
    // slow update
    static const Word16 k_EST_SLOW_ATTACK;
    static const Word16 k_ONE_MINUS_EST_SLOW_ATTACK;


    // Attack time constant for threshold attack
    static const Word16 k_THR_ATTACK;
    static const Word16 k_ONE_MINUS_THR_ATTACK;




    // Number of frames to wait after last attack

    static const unsigned int k_MAX_FRAMES_AFTER_LAST_ATTACK;

    // Frames to wait for initially
    static const unsigned int k_WAIT_FOR_FRAMES;

    // Number of features
    static unsigned int m_numFeatures;



    Word32 m_minTmp;
    Word32 m_maxTmp;

    REAL m_minTmpFlt;
    REAL m_maxTmpFlt;

    // min value
    Word64  m_minVal;

    // peak value
    Word64 m_maxVal;

    // smoothed value
    Word64 m_Val;

    // raw value
    Word64 m_rawVal;

    // Ensemble speech, noise  and SNR estimation
    Word64 m_estSpeechLevel;
    Word64 m_estNoiseLevel;
    Word64 m_estSNR;

    //val when last min attack happened
    Word64 m_lastMinAttackVal;

    //tunable value
    Word16 m_adjThresholdKnob;

    // initial threshold
    Word64 initThreshold;

    // contains estimated signal value for speech
    Word64 m_speechThreshold;

    // contains estimated signal value for noise
    Word64 m_noiseThreshold;


    // m_numAbove used for initial convergence
    unsigned int m_numAbove;

    // used w/ lastMinVal to adapt for floor changes
    unsigned int m_numFramesSinceLastAttack;




    //interim VAD decision of feature
        bool m_vadFlag;

    // checks for first frame for initvals
        bool m_isFirstFrame;


    // private function for updating values
    void updateValue(Word64 currentVal);





public:

    // constructor
    MinPeakClass();

    //destructor
    ~MinPeakClass();




    // sets initial value

    void setInitVal();

    void setInitVal(Word64 init_val);



    // updates value calculated for current frame
    void update(Word64 currentVal);

    // sets threshold value
    void turnThreshKnob(Word16 val);

    // reset of this class
    void reset();

    // tracks signal to noise ratio for given feature
    void trackSignal2Noise(Word32 ensembleVAD);


    // returns interim noise threshold
    Word64 getNoiseThreshold();

    // return output estimated speech level
    Word64 getEstSpeechLevel();

    // return output estimated noise level
    Word64 getEstNoiseLevel();

    // return output estimated SNR
    Word64 getEstSignal2Noise();


    // get number of instances of minpeakclass
    static unsigned int getNumFeatures();


    // return VAD state
    bool getVADState();



    // lowest floor value
    static const Word64 k_LOWEST_FLOOR;

    //tuned threshold for CLARITY feature
    static const Word16 k_THR_CLARITY;

    // tuned threshold for Freq Centroid
    static const Word16 k_THR_FCENT;

    // tuned threshold for Energy
    static const Word16 k_THR_ENERGY;

    //initial threshold
    static const Word16 k_INIT_THR;

    // high value for average of ensemble vad outputs
    static const Word32 k_ENSEMBLE_VAD_HIGH_VALUE;

    // low value for average of ensemble vad outputs
    static const Word32 k_ENSEMBLE_VAD_LOW_VALUE;


};


#endif // MINPEAKCLASS_H
