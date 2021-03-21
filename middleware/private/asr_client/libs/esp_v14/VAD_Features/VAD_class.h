/*
 * Copyright 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * You may not use this file except in compliance with the terms and conditions set forth in the accompanying LICENSE.TXT file.
 *
 * THESE MATERIALS ARE PROVIDED ON AN "AS IS" BASIS.  AMAZON SPECIFICALLY DISCLAIMS, WITH RESPECT TO THESE MATERIALS, ALL
 * WARRANTIES, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, AND NON-INFRINGEMENT.
 */

#ifndef VADCLASS_H
#define VADCLASS_H
#include <string>
#include "flagbuffer.h"
#include "../common/fixed_pt.h"
#include "./xcorr_class.h"
#include "./minpeakclass.h"

enum enumVADFeaturesIndex {CENTROID_INDEX,
                           CLARITY_INDEX,
                           ENERGY_INDEX,
                           NUM_SIG_FEAT };

class VADClass
{
public:
    // ensemble VAD count
    Word32    m_ensembleVAD;
    Word64    m_Debug_1;
    Word64    m_Debug_2;
    Word64    m_Debug_3;

    MinPeakClass   *m_Feature;

private:

    XCORRClass     *m_XCORRHandle;

    // short time frame energy (the lag-zero from autocorrelation)
    Word64    m_frameEnergy;

    // frequency centroid
    Word64    m_freqCentroid;

    // frame clarity
    Word64    m_frameClarity;

    UWord32   m_frameLength;

public:

    // constructor with framelength passed
    VADClass(const unsigned int monoAudioFrameLength);

    ~VADClass();

    // process function
    virtual void process( Word16* signal, const unsigned int channel, bool& decision);

    // VAD block reset function
    virtual void blkReset(void);

    // stores increment value
    static const Word32 k_ENSEMBLE_VAD_INCREMENT;

    // stores total vad features
    static const Word32 k_ONE_OVER_TOTAL_VAD_FEATURES;

    static const Word32 k_ENERGY_THRESH;

};

#endif // VADMODULE_H
