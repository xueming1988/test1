/*
 * Copyright 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * You may not use this file except in compliance with the terms and conditions set forth in the accompanying LICENSE.TXT file.
 *
 * THESE MATERIALS ARE PROVIDED ON AN "AS IS" BASIS.  AMAZON SPECIFICALLY DISCLAIMS, WITH RESPECT TO THESE MATERIALS, ALL
 * WARRANTIES, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, AND NON-INFRINGEMENT.
 */

#include "./FrameEnergyComputing.h"


FrameEnergyClass::~FrameEnergyClass(){
    delete m_egyEstAmbientBuff;
}

FrameEnergyClass::FrameEnergyClass(const unsigned int audioFrameLength){

    m_frameLen   = audioFrameLength;
    unsigned int numHangFramesAmbient = 0;
    unsigned int numLookBackFramesAmbient = 0;

    // frame length dependent parameters
    if (m_frameLen == 256){

        //for fix point computation, below float number won't be used
        //double coeff_m_voicedEnergy = 0.01875f;// = 1229/2^16
        //double coeff_voice_hang = 0.09375f;// = 96/2^10
        //double coeff_m_ambientEnergy = 0.01125f;// = 737/2^16

        // number of frames elapsed before voiced energy is reduced
        m_hangInterval  = 25;

        // Smoothing coef for voiced energy - slow decay
        m_lpfBetaVoice = 1311;//1311

        // Smoothing coef for voiced energy - fast decay
        m_lpfBetaVoiceDecay = 102;//102

        // Smoothing coef for background noise
        m_lpfBetaNoise = 786;//786

        numHangFramesAmbient = 9;//12
        numLookBackFramesAmbient = 5;//8

        setAmbientBuffParam( numHangFramesAmbient, numLookBackFramesAmbient);
    }
    else if (m_frameLen == 240){

        //for fix point computation, below float number won't be used
        //double coeff_m_voicedEnergy = 0.01875f;// = 1229/2^16
        //double coeff_voice_hang = 0.09375f;// = 96/2^10
        //double coeff_m_ambientEnergy = 0.01125f;// = 737/2^16

        // number of frames elapsed before voiced energy is reduced
        m_hangInterval  = 27;

        // Smoothing coef for voiced energy - slow decay
        m_lpfBetaVoice = 1229;//1229

        // Smoothing coef for voiced energy - fast decay
        m_lpfBetaVoiceDecay = 96;//96

        // Smoothing coef for background noise
        m_lpfBetaNoise = 737;//737

        numHangFramesAmbient = 10;//12
        numLookBackFramesAmbient = 5;//8

        setAmbientBuffParam( numHangFramesAmbient, numLookBackFramesAmbient);
    }
    else { // Use default as 128

        //for fix point computation, below float number won't be used
        //double coeff_m_voicedEnergy = 0.01875f * 8/15;// = 0.01f  = 655/2^16
        //double coeff_voice_hang = 0.09375f * 8/15;// = 0.05f = 51/2^10
        //double coeff_m_ambientEnergy = 0.01125f * 8/15;// = 0.006f = 393/2^16

        // number of frames elapsed before voiced energy is reduced
        m_hangInterval  = 50;

        // Smoothing coef for voiced energy - slow decay
        m_lpfBetaVoice = 655;

        // Smoothing coef for voiced energy - fast decay
        m_lpfBetaVoiceDecay = 51;

        // Smoothing coef for background noise
        m_lpfBetaNoise = 393;

        numHangFramesAmbient = 18; //24;
        numLookBackFramesAmbient = 10; //16;

        setAmbientBuffParam( numHangFramesAmbient, numLookBackFramesAmbient);
    }

    // device arbitration estimates
    m_egyEstVoice   = 0;
    m_egyEstAmbient = 0;
    m_HangCounter   = 0;
}

void FrameEnergyClass::process(const bool vadFlag, const Word64 frameEnergy)
{

    // Decrement the hangcounter until it reaches zero
    if(m_HangCounter>0)
        m_HangCounter--;

    // Decrement the hangcounter for ambient until it reaches zero
    if(m_HangCounterAmbient>0)
        m_HangCounterAmbient--;

    // When zero, voiced energy is reset
    if(m_HangCounter==0)
    {
        m_egyEstVoice = m_egyEstVoice - ((m_lpfBetaVoiceDecay * m_egyEstVoice) >> 10);
    }

    if (vadFlag)
    {
        // When VAD is true, update voiced energy measure
        m_HangCounter = m_hangInterval;

        m_HangCounterAmbient = m_hangIntervalAmbient;

        //m_egyEstVoice = m_egyEstVoice - ((m_lpfBetaVoice * (m_egyEstVoice - frameEnergy)) >> 16);

        m_egyEstVoice = (( m_lpfBetaVoice * frameEnergy ) >> 16) + m_egyEstVoice - (( m_lpfBetaVoice * m_egyEstVoice ) >> 16);
    }
    else
    {
        // When VAD is false, update ambient energy measure
        if(m_HangCounter==0)
        {
            //m_egyEstVoice = m_egyEstVoice - ((m_lpfBetaVoice * (m_egyEstVoice - frameEnergy)) >> 16);

            m_egyEstVoice = (( m_lpfBetaVoice * frameEnergy ) >> 16) + m_egyEstVoice - (( m_lpfBetaVoice * m_egyEstVoice ) >> 16);
        }

        // Hold before updating noise immediately after the VAD drops
        if(m_HangCounterAmbient == 0)
        {

            m_egyEstAmbient = m_egyEstAmbient - ((m_lpfBetaNoise * (m_egyEstAmbient - frameEnergy)) >> 16);

            //m_egyEstAmbient = (( m_lpfBetaNoise * frameEnergy ) >> 16) + m_egyEstAmbient - (( m_lpfBetaNoise * m_egyEstAmbient ) >> 16);

            m_egyEstAmbientBuff[m_buffWriteIdx] = m_egyEstAmbient;

            m_buffWriteIdx = BUFFER_IDX_WRAP(m_buffWriteIdx, m_ambientBuffLength);

            m_buffReadIdx = BUFFER_IDX_WRAP(m_buffReadIdx, m_ambientBuffLength);

            m_egyEstAmbientPrevVal = m_egyEstAmbientBuff[m_buffReadIdx];
        }

        // During Ambient Hang period, update the ambient buffer with previous value
        else
        {
            m_egyEstAmbientBuff[m_buffWriteIdx] = m_egyEstAmbientPrevVal;

            m_buffWriteIdx = BUFFER_IDX_WRAP(m_buffWriteIdx, m_ambientBuffLength);

            m_buffReadIdx = BUFFER_IDX_WRAP(m_buffReadIdx, m_ambientBuffLength);

            m_egyEstAmbientBuff[m_buffReadIdx] = m_egyEstAmbientPrevVal; // holding the same look back value

            m_egyEstAmbient = m_egyEstAmbientPrevVal; // Override the current energy estimate with the read index to avoid glitch
        }
    }
}

void FrameEnergyClass::blkReset()
{

    m_egyEstAmbientPrevVal = 0;
    m_HangCounterAmbient = m_hangIntervalAmbient;

    // Circular buffer for VAD lookahead for Ambient energy
    m_buffWriteIdx = m_ambientBuffLength - 1;
    m_buffReadIdx = 0;
    for (unsigned int k = 0; k < m_ambientBuffLength; k++)
        m_egyEstAmbientBuff[k] = 0;
}

// Set parameters for noise energy look back and hang constants
void FrameEnergyClass::setAmbientBuffParam(const unsigned int numHangFramesAmbient, const unsigned int numLookBackFramesAmbient)
{
    m_ambientBuffLength = numLookBackFramesAmbient;
    m_hangIntervalAmbient = numHangFramesAmbient;
    m_HangCounterAmbient = m_hangIntervalAmbient;
    m_egyEstAmbientPrevVal = 0;

    m_egyEstAmbientBuff = new Word64 [m_ambientBuffLength];

    // Circular buffer for VAD lookahead for Ambient energy
    m_buffWriteIdx = m_ambientBuffLength - 1;
    m_buffReadIdx = 0;
    for (unsigned int k = 0; k < m_ambientBuffLength; k++)
        m_egyEstAmbientBuff[k] = 0;
}

// Get Ambient Energy
int FrameEnergyClass::getAmbientEnergy() const
{
    return SAT32s(m_egyEstAmbientBuff[m_buffReadIdx]);
}

// Get Voiced Energy
int FrameEnergyClass::getVoicedEnergy() const
{
    return SAT32s(m_egyEstVoice);
}
