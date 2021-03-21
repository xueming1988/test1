/*
 * Copyright 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * You may not use this file except in compliance with the terms and conditions set forth in the accompanying LICENSE.TXT file.
 *
 * THESE MATERIALS ARE PROVIDED ON AN "AS IS" BASIS.  AMAZON SPECIFICALLY DISCLAIMS, WITH RESPECT TO THESE MATERIALS, ALL
 * WARRANTIES, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, AND NON-INFRINGEMENT.
 */

#include "./minpeakclass.h"

//Keeps track of number of MinPeakClass objects
unsigned int MinPeakClass::m_numFeatures = 0;

//Time Constant for the Smoother
const    Word16 MinPeakClass::k_ALPHA            = 28920; //0.882553959f*2^15;
const    Word16 MinPeakClass::k_ONE_MINUS_ALPHA  = 3848; //1.0f  - MinPeakClass::k_ALPHA;

//Time Constant for attack in Min and level
const    Word16 MinPeakClass::k_ATTACK           = 10908; //2^15 * 0.332871079f;
const    Word16 MinPeakClass::k_ONE_MINUS_ATTACK = 21860; //2^15 * 1.0f  -  MinPeakClass::k_ATTACK;

//Time Constant for attack in Peak tracking
const Word16 MinPeakClass::k_PEAK_ATTACK           = 11796; //0.36f * 2^15; //tuned 04-08-14
const Word16 MinPeakClass::k_ONE_MINUS_PEAK_ATTACK = 20972; //2^15* (1.0f  -  MinPeakClass::k_PEAK_ATTACK);

// Time Constant for release in Peak tracking
const    Word16 MinPeakClass::k_PEAK_RELEASE           = 32637; //2^15 * 0.996f;
const    Word16 MinPeakClass::k_ONE_MINUS_PEAK_RELEASE = 131; //2^15 - (1.0f - MinPeakClass::k_PEAK_RELEASE);

// Time Constant for release in Min  tracking
const    Word16 MinPeakClass::k_MIN_RELEASE           = 32653; // 2^15 * 0.996486187f;
const    Word16 MinPeakClass::k_ONE_MINUS_MIN_RELEASE = 115; // 2^15 - (1.0f - MinPeakClass::k_MIN_RELEASE);

// Time Constant for smoothing initial Threshold
const    Word16 MinPeakClass::k_THR_ATTACK           = 32604; // 2^15 * 0.995000000f;
const    Word16 MinPeakClass::k_ONE_MINUS_THR_ATTACK = 164; // 2^15 *(1.0f - MinPeakClass::k_THR_ATTACK);

// Time Constant for smooting ensemble estimation
const Word16 MinPeakClass::k_EST_FAST_ATTACK    = 26214; // 2^15 * 0.8f;
const Word16 MinPeakClass::k_ONE_MINUS_EST_FAST_ATTACK = 6554; // 2^15 * (1.0f - MinPeakClass::k_EST_FAST_ATTACK);

const Word16 MinPeakClass::k_EST_SLOW_ATTACK    = 32440; // 2^15 * 0.99f;
const Word16 MinPeakClass::k_ONE_MINUS_EST_SLOW_ATTACK = 328; // 2^15 * (1.0f - MinPeakClass::k_EST_SLOW_ATTACK);

// Set num of frames to start adapting after extended release
const   unsigned int MinPeakClass::k_MAX_FRAMES_AFTER_LAST_ATTACK = 0;

// Set initial frames to wait for
const   unsigned int MinPeakClass::k_WAIT_FOR_FRAMES              = 0;

// set Thresholds here, tuned on AMPED

const Word16 MinPeakClass::k_THR_FCENT   = 19005; // 2.32f  << 13;

const Word16 MinPeakClass::k_THR_CLARITY = 12124; // 1.48f << 13;

const Word16 MinPeakClass::k_THR_ENERGY  = 17039; // 2.08f << 13;

const Word16 MinPeakClass::k_INIT_THR    = 16384; // 2.0f << 13;

const Word32 MinPeakClass::k_ENSEMBLE_VAD_HIGH_VALUE = 4915; //0.6f << 13;

const Word32 MinPeakClass::k_ENSEMBLE_VAD_LOW_VALUE  = 2458; //0.3f << 13;

// lowest possible floor value, hard threshold
const Word64 MinPeakClass::k_LOWEST_FLOOR = 0x000001; //10e-7 = 2^-23 ~= 0x00 00 01 in q63;

MinPeakClass::~MinPeakClass() { }

void MinPeakClass::reset()
{

    m_lastMinAttackVal = 0;
    m_numAbove         = 1;

    m_numFramesSinceLastAttack = 0;

    // values for interim
    m_speechThreshold  = 0;
    m_noiseThreshold   = 0;
    m_adjThresholdKnob = 0;

    // ensemble estimation test
    m_estSpeechLevel   = 0.0f;
    m_estNoiseLevel    = 0.0f;
    m_estSNR           = 0.0f;

    m_Val              = 0;
    m_rawVal           = 0;

    m_minVal           = 0;
    m_maxVal           = 0;

    initThreshold      = 0;

    m_vadFlag        = false;
    m_isFirstFrame   = true;

    FLAGBuffer::reset();

}

MinPeakClass::MinPeakClass()
{

    setInitVal();
    MinPeakClass::reset();
    m_numFeatures++;
    m_minTmp = 0;
    m_maxTmp = 0;

    m_minTmpFlt = 0.f;
    m_maxTmpFlt = 0.f;
}

Word64 MinPeakClass::getNoiseThreshold() {

    return m_noiseThreshold;

}

unsigned int MinPeakClass::getNumFeatures(){

    return m_numFeatures;

}

void MinPeakClass::setInitVal(Word64 init_val){

    m_maxVal = init_val;
    m_minVal = init_val;
    m_Val    = init_val;

}

Word64 MinPeakClass::getEstSpeechLevel() {

    return m_estSpeechLevel;

}

Word64 MinPeakClass::getEstNoiseLevel() {

    return m_estNoiseLevel;

}

Word64 MinPeakClass::getEstSignal2Noise() {

    return  m_estSNR;

}

void MinPeakClass::updateValue(Word64 currentVal) {

    // smoothing raw values
    m_Val = ((Word64)k_ALPHA*m_Val >>15) + ((Word64)k_ONE_MINUS_ALPHA*currentVal >>15);

    // Tracking Peak Level
    if (m_rawVal > m_maxVal) {
        m_maxVal = ((Word64)k_PEAK_ATTACK*m_maxVal >> 15) +  ((Word64)k_ONE_MINUS_PEAK_ATTACK*m_rawVal >> 15);
    }
    else {
        m_maxVal = ((Word64)k_PEAK_RELEASE*m_maxVal >> 15) + ((Word64)k_ONE_MINUS_PEAK_RELEASE*m_rawVal >> 15);

    }

    // Tracking Min Level
    if (m_Val  < m_minVal) {

        // BEGIN: Min Attack

        m_minVal = ((Word64)k_ATTACK*m_minVal >> 15)  + ((Word64)k_ONE_MINUS_ATTACK*m_Val >> 15);
        m_lastMinAttackVal = m_Val;
        m_numFramesSinceLastAttack = 0;

        // END: Min Attack

    }
    else {

        // BEGIN:Release happens

        //making sure i have released for more than 1 second

        if (m_numFramesSinceLastAttack < k_MAX_FRAMES_AFTER_LAST_ATTACK) {

            m_numFramesSinceLastAttack++;
        }
        else {
            m_numFramesSinceLastAttack = 0;
            m_noiseThreshold = ((Word64)k_ATTACK*m_noiseThreshold >>15) + ((Word64)k_ONE_MINUS_ATTACK*m_lastMinAttackVal >>15);
            //attack to the last min attack val
        }
        // release Min Val
        m_minVal = ((Word64)k_MIN_RELEASE*m_minVal >> 15) + ((Word64)k_ONE_MINUS_MIN_RELEASE*m_Val>>15);

        //END: Release happens
    }

}



void MinPeakClass::update(Word64 currentVal) {

    m_rawVal = currentVal;

    if (m_isFirstFrame)
    {
        setInitVal(currentVal);
        m_isFirstFrame = false;
    }
    else
    {
        updateValue(currentVal);

        if (m_numAbove < k_WAIT_FOR_FRAMES)
        {   //accumulation of  initial threshold convergence
            if (m_Val> currentVal)
            {
                m_numAbove++;
            }

            initThreshold     =  ((Word64)k_THR_ATTACK*initThreshold >>15) + ((Word64)k_ONE_MINUS_THR_ATTACK*currentVal >>15);
            m_noiseThreshold  =  initThreshold;
            m_speechThreshold =  ((Word64)k_INIT_THR*initThreshold >> 13); // putting the threshold  dB above noise;
            m_vadFlag = false;
        }
        //initial threshold convergence complete
        else
        {

            if (m_Val >  ((Word64)m_adjThresholdKnob*getNoiseThreshold() >> 13))
			{
                m_vadFlag = true;
            }
            else
            {
                m_vadFlag = false;
            }

        }

    }

    // Note that m_vadFlag must be functional
    //  to have this update rule properly working.

    // updating the flag buffer
    // updating currentVal to the ValBuffer
    shiftFlag(currentVal, m_vadFlag);

    if ((FLAGBuffer::getFlagBuffer() & FLAGBuffer::k_FLAG_MASK) == 0 ){
        // if  previous  frames are inactive, update noise threshold
        m_noiseThreshold = ((Word64)k_THR_ATTACK*m_noiseThreshold >> 15) + ((Word64)k_ONE_MINUS_THR_ATTACK*getBufferMean()>>15);

    }

    if ( (FLAGBuffer::getFlagBuffer() & FLAGBuffer::k_FLAG_MASK) == FLAGBuffer::k_FLAG_MASK ) {
        // if current and previous frames are active
        m_speechThreshold = ((Word64)k_THR_ATTACK*m_speechThreshold >> 15) + ((Word64)k_ONE_MINUS_THR_ATTACK*getBufferMean()>>15);
    }
}


bool MinPeakClass::getVADState() {

    return m_vadFlag;

}

void MinPeakClass::setInitVal(){

    m_minVal = 0;
    m_maxVal = 0;
}

void MinPeakClass::turnThreshKnob(Word16 val) {

    m_adjThresholdKnob = val;

}

void MinPeakClass::trackSignal2Noise(Word32 ensembleVAD) {

    if (ensembleVAD >= k_ENSEMBLE_VAD_HIGH_VALUE){

        // surely speech so update fast
        m_estSpeechLevel = ((Word64)k_EST_FAST_ATTACK*m_estSpeechLevel >> 15) + ((Word64)k_ONE_MINUS_EST_FAST_ATTACK*m_rawVal >> 15);

    }
    else if ((ensembleVAD >= k_ENSEMBLE_VAD_LOW_VALUE) && (ensembleVAD < k_ENSEMBLE_VAD_HIGH_VALUE)) {

         //not sure so update slow
        m_estSpeechLevel = ((Word64)k_EST_SLOW_ATTACK*m_estSpeechLevel >> 15) + ((Word64)k_ONE_MINUS_EST_SLOW_ATTACK*m_rawVal >> 15);
        m_estNoiseLevel  = ((Word64)k_EST_SLOW_ATTACK*m_estNoiseLevel >> 15)  + ((Word64)k_ONE_MINUS_EST_SLOW_ATTACK*m_rawVal >> 15);

    }
    else {
         // surely noise update fast
        m_estNoiseLevel = ((Word64)k_EST_FAST_ATTACK*m_estNoiseLevel >> 15) + ((Word64)k_ONE_MINUS_EST_FAST_ATTACK*m_rawVal >> 15);

    }

    if (m_estNoiseLevel > 0.0f) {

        m_estSNR = (Word64)(m_estSpeechLevel / m_estNoiseLevel);
    }
    else {

        m_estSNR = MinPeakClass::k_LOWEST_FLOOR;
    }

    m_maxTmp = MAX_VAL(m_maxTmp, ensembleVAD);
    m_minTmp = MIN_VAL(m_minTmp, ensembleVAD);

}

