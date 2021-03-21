/*
 * Copyright 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * You may not use this file except in compliance with the terms and conditions set forth in the accompanying LICENSE.TXT file.
 *
 * THESE MATERIALS ARE PROVIDED ON AN "AS IS" BASIS.  AMAZON SPECIFICALLY DISCLAIMS, WITH RESPECT TO THESE MATERIALS, ALL
 * WARRANTIES, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, AND NON-INFRINGEMENT.
 */

#include "./VAD_class.h"

// value to increment for each vad feature outputting high
const Word32 VADClass::k_ENSEMBLE_VAD_INCREMENT = 8192; //2^13*1.0f;

// One over number of VAD features used to calculate average ensemble VAD outputs
const Word32 VADClass::k_ONE_OVER_TOTAL_VAD_FEATURES  = 2731; // 2^13/3.0f;

const Word32 VADClass::k_ENERGY_THRESH = 200000; // 500000 ~= -70 dBFS Square Wave

VADClass::~VADClass(){

    delete m_XCORRHandle;
    delete[] m_Feature;

}

VADClass::VADClass(const unsigned int audioFrameLength){

    m_frameLength   = audioFrameLength;
    Word16 threshVal  = 0;

    //creating new instances with prescribed framelength

    // here there are 3: centroid, energy, clarity
    m_XCORRHandle   = new XCORRClass (m_frameLength);

    // NUM_SIG_FEAT is the number of signal features used
    // we need an instance of MinPeakClass for each signal feature
    // Speech level estimates and SNR estimates are taken to be features as well
    m_Feature       = new MinPeakClass [NUM_SIG_FEAT];

    // Set parameters dependent on frameSize
    m_Feature[CENTROID_INDEX].setFrameSize(m_frameLength);
    m_Feature[CLARITY_INDEX].setFrameSize(m_frameLength);
    m_Feature[ENERGY_INDEX].setFrameSize(m_frameLength);

    // This is for Centroid
    threshVal = MinPeakClass::k_THR_FCENT;

    // setting the threshold using the turnThreshKnob() method
    m_Feature[CENTROID_INDEX].turnThreshKnob(threshVal);

    // This is for frameClarity
    threshVal = MinPeakClass::k_THR_CLARITY; // raw value not logscale

    // setting the threshold using the turnThreshKnob() method
    m_Feature[CLARITY_INDEX].turnThreshKnob(threshVal);

    // This is for frameEnergy
    threshVal = MinPeakClass::k_THR_ENERGY;

    // setting the threshold using the turnThreshKnob() method
    m_Feature[ENERGY_INDEX].turnThreshKnob(threshVal);

    // initializing values
    m_freqCentroid = 0;

    m_frameClarity = 0;

    m_frameEnergy  = 0;

    // average of VAD outputs of features
    m_ensembleVAD  = 0;

}


void VADClass::blkReset(void){

    // initializing values
    m_freqCentroid = 0;

    m_frameClarity = 0;

    m_frameEnergy  = 0;

    // average of VAD outputs of features
    m_ensembleVAD  = 0;

    for (unsigned int k = 0; k < MinPeakClass::getNumFeatures();k++)
    {
        m_Feature[k].reset(); // this also resets minPeakClass's FLAGBuffers;
    }

}

void VADClass::process( Word16 * xInFr, bool& decision, Word64& currentFrameEnergy) {

    // This is the main process() entry method for the VAD
    // It currently uses 3 features: frequency centroid, frame clarity and frame energy

    //SAMPLE *xInFr = signal->getSamplePointer(channel);
    //resetting values for current frame
    m_ensembleVAD  = 0;
    decision       = false;

    m_Debug_1 = 0;
    m_Debug_2 = 0;

    // calculating features Clarity, Energy and Frequency Centroid in  "calcCorrFeatures"
    // note that these are now frequency constrained values
    m_XCORRHandle->calcCorrFeatures(xInFr);

    // The results of calcCorrFeatures are real values stored in  in private fields
    // these are acessed into members of VADClass using the get....() method

    // getting Values
    m_freqCentroid = m_XCORRHandle->getFreqCentroid(); //0

    // getting Values
    m_frameClarity =  m_XCORRHandle->getClarity();      //1

    // getting Values
    m_frameEnergy = m_XCORRHandle->getFrameEnergy();   //2

    // the above values contain raw signal features
    // these are EACH then smoothed in the update() method
    // of the minpeakclass shown below

    // Updating Values
    m_Feature[CENTROID_INDEX].update(m_freqCentroid); //0

    // Updating Values
    m_Feature[CLARITY_INDEX].update(m_frameClarity);  //1

    // Updating Values
    m_Feature[ENERGY_INDEX].update(m_frameEnergy);    //2


    // Calculating average of VAD outputs of each feature

    // increment if centroid VAD is high
    if ( m_Feature[CENTROID_INDEX].getVADState()) {

        m_ensembleVAD = Word32(SAT32s(m_ensembleVAD + VADClass::k_ENSEMBLE_VAD_INCREMENT));

    }

    // increment if clarity VAD is high
    if ( m_Feature[CLARITY_INDEX].getVADState()) {

        m_ensembleVAD = Word32(SAT32s(m_ensembleVAD + VADClass::k_ENSEMBLE_VAD_INCREMENT));

    }

    // increment if energy VAD is high
    if ( m_Feature[ENERGY_INDEX].getVADState()) {

        m_ensembleVAD = Word32(SAT32s(m_ensembleVAD + VADClass::k_ENSEMBLE_VAD_INCREMENT));

    }

    // ensemble average
    m_ensembleVAD = Word32(Word64(m_ensembleVAD * VADClass::k_ONE_OVER_TOTAL_VAD_FEATURES)>>13);

    // Using the average VAD output to track SNR for each feature
    m_Feature[CENTROID_INDEX].trackSignal2Noise(m_ensembleVAD);

    m_Feature[CLARITY_INDEX].trackSignal2Noise(m_ensembleVAD);

    m_Feature[ENERGY_INDEX].trackSignal2Noise(m_ensembleVAD);

    currentFrameEnergy = m_frameEnergy;

    // Calculating final decsision, if surely speech then decision = true
    // note the decision = false at the very begining of the process() function
    if (m_ensembleVAD >= MinPeakClass::k_ENSEMBLE_VAD_HIGH_VALUE && m_frameEnergy > VADClass::k_ENERGY_THRESH) {

        decision = true;

    }

}
