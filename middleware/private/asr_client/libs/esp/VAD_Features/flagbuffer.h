/*
 * Copyright 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * You may not use this file except in compliance with the terms and conditions set forth in the accompanying LICENSE.TXT file.
 *
 * THESE MATERIALS ARE PROVIDED ON AN "AS IS" BASIS.  AMAZON SPECIFICALLY DISCLAIMS, WITH RESPECT TO THESE MATERIALS, ALL
 * WARRANTIES, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, AND NON-INFRINGEMENT.
 */

#ifndef FLAGBUFFER_H
#define FLAGBUFFER_H

// total length of FLAG_BUFFER
#define     FLAG_BUFFER_LENGTH   16

#include "../common/fixed_pt.h"

typedef unsigned short int flagbuffer_t;

class FLAGBuffer
{
private:
    flagbuffer_t    m_FlagBuffer;

    // lowest used edge of FLAG_BUFFER (  0 < val < m_FLAG_BUFFER_LENGTH/2)
    static const flagbuffer_t k_MID_BUFFER_LENGTH;

    // highest used edge of FLAG_BUFFER (  m_FLAG_BUFFER_LENGTH/2 < val < m_FLAG_BUFFER_LENGTH)
    static const flagbuffer_t k_LOW_BUFFER_EDGE;

    // total length of middle part of flagbuffer
    static const flagbuffer_t k_HIGH_BUFFER_EDGE;


    // stores the past m_FLAG_BUFFER_LENGTH values
    Word64   m_ValBuffer[FLAG_BUFFER_LENGTH];

    // stores the value of the mean value of the middle part
    Word64    m_ValBufferMean;

protected:

    // mask value for the entire buffer but LSB
    static const flagbuffer_t k_FLAG_MASK;

    // returns m_FlagBuffer
    flagbuffer_t getFlagBuffer();

     // retruns meanValue
    Word64 getBufferMean();

     // resets everything
    void reset();

public:

    // constructor
    FLAGBuffer();

    // destructor
    ~FLAGBuffer();

    // updates m_FlagBuffer
    void shiftFlag(bool flag);

    // updates m_FlagBuffer
    void shiftFlag(Word64 val, bool flag);

};

#endif // FLAGBUFFER_H
