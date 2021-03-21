/*
 * Copyright 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * You may not use this file except in compliance with the terms and conditions set forth in the accompanying LICENSE.TXT file.
 *
 * THESE MATERIALS ARE PROVIDED ON AN "AS IS" BASIS.  AMAZON SPECIFICALLY DISCLAIMS, WITH RESPECT TO THESE MATERIALS, ALL
 * WARRANTIES, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, AND NON-INFRINGEMENT.
 */

#ifndef FRAMEENERGYCLASS_H
#define FRAMEENERGYCLASS_H
#include "./FrameEnergyComputing.h"
#include "../common/fixed_pt.h"

#define     BUFFER_IDX_WRAP(X, Y)    (((X+1) >= (Y) ? (0) : (X+1)))

class FrameEnergyClass
{

private:
    Word64    m_frameLen;

    // device arbitration estimates
    Word64    m_egyEstVoice;
    Word64    m_egyEstAmbient;
    unsigned int m_HangCounter;
    unsigned int m_HangCounterAmbient;

    // Buffer and pointers for Ambient energies to provide look ahead of VAD
    Word64    *m_egyEstAmbientBuff;
    Word64    m_egyEstAmbientPrevVal;
    unsigned int  m_buffWriteIdx;
    unsigned int  m_buffReadIdx;

    // Hold off interval for ambient
    unsigned int m_hangIntervalAmbient;

    // Look ahead frames for ambient noise. Value >=1
    unsigned int m_ambientBuffLength;

    Word16 m_hangInterval;

    // Smoothing coef for voiced energy - slow decay
    Word16 m_lpfBetaVoice;

    // Smoothing coef for voiced energy - fast decay
    Word16 m_lpfBetaVoiceDecay;

    // Smoothing coef for background noise
    Word16 m_lpfBetaNoise;

public:

    // constructor with framelength passed
    FrameEnergyClass(const unsigned int monoAudioFrameLength);

    ~FrameEnergyClass();

    // process function
    void process(const bool vadFlag, const Word64 frameEnergy);

    // block reset function
    void blkReset(void);

    // Get Ambient Energy
    int getAmbientEnergy() const;

    // Get Voiced Energy
    int getVoicedEnergy() const;

    // Set parameters for noise energy look back and hang constants
    void setAmbientBuffParam(const unsigned int numHangFramesAmbient, const unsigned int numLookBackFramesAmbient);

};

#endif // FRAMEENERGYCLASS_H
