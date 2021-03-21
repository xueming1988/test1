/*
 * Copyright 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * You may not use this file except in compliance with the terms and conditions set forth in the accompanying LICENSE.TXT file.
 *
 * THESE MATERIALS ARE PROVIDED ON AN "AS IS" BASIS.  AMAZON SPECIFICALLY DISCLAIMS, WITH RESPECT TO THESE MATERIALS, ALL
 * WARRANTIES, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, AND NON-INFRINGEMENT.
 */

#include <string.h>
#include "./flagbuffer.h"

// lowest used edge of FLAG_BUFFER (  0 < val < m_FLAG_BUFFER_LENGTH/2)
const flagbuffer_t FLAGBuffer::k_LOW_BUFFER_EDGE       = 4;

// highest used edge of FLAG_BUFFER (  m_FLAG_BUFFER_LENGTH/2 < val < m_FLAG_BUFFER_LENGTH)
const flagbuffer_t FLAGBuffer::k_HIGH_BUFFER_EDGE      = 12;

// total length of middle part of flagbuffer
const flagbuffer_t FLAGBuffer::k_MID_BUFFER_LENGTH     = FLAGBuffer::k_HIGH_BUFFER_EDGE -  FLAGBuffer::k_LOW_BUFFER_EDGE;


const flagbuffer_t FLAGBuffer::k_FLAG_MASK             = 65535;// 2^16 - 1

FLAGBuffer::FLAGBuffer()
{

    m_ValBufferMean    = 0;

    m_FlagBuffer       = 0; // resetting

    // setting all values to zero
    memset(m_ValBuffer, 0, sizeof(m_ValBuffer));

}

void FLAGBuffer::shiftFlag(bool flag)
{

    if (flag)
    {

        m_FlagBuffer ^=  1; // setting LSB

    }

    m_FlagBuffer <<= 1;    // left-shift by 1 bit

}

void FLAGBuffer::shiftFlag(Word64 val, bool flag)
{

    // initializing
     m_ValBufferMean = 0;

    // shifting left
    for (unsigned int k = 0; k < FLAG_BUFFER_LENGTH - 1;k++)
    {
        m_ValBuffer[k] = m_ValBuffer[k + 1];
    }

    // calculating mean from middle part of valBuffer
    for (unsigned int k = k_LOW_BUFFER_EDGE; k < k_HIGH_BUFFER_EDGE;k++)
    {
        m_ValBufferMean += m_ValBuffer[k];
    }


    m_ValBufferMean = m_ValBufferMean >> 3; // / k_MID_BUFFER_LENGTH;

    // storing current value in to valBuffer
    m_ValBuffer[FLAG_BUFFER_LENGTH - 1] = val;

    // shifting decision flags left
    shiftFlag(flag);
}

FLAGBuffer::~FLAGBuffer()
{

}

Word64 FLAGBuffer::getBufferMean()
{
    return m_ValBufferMean;
}

flagbuffer_t FLAGBuffer::getFlagBuffer()
{
    return m_FlagBuffer;
}

void FLAGBuffer::reset()
{
     m_FlagBuffer = 0; // resetting

     // setting all values to zero
     memset(m_ValBuffer, 0, sizeof(m_ValBuffer));
}
