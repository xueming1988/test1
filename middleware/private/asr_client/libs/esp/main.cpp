/*
 * Copyright 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * You may not use this file except in compliance with the terms and conditions set forth in the accompanying LICENSE.TXT file.
 *
 * THESE MATERIALS ARE PROVIDED ON AN "AS IS" BASIS.  AMAZON SPECIFICALLY DISCLAIMS, WITH RESPECT TO THESE MATERIALS, ALL
 * WARRANTIES, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, AND NON-INFRINGEMENT.
 */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include "./VAD_Features/VAD_class.h"
#include "./DA_Metrics/FrameEnergyComputing.h"

using namespace std;

int main(int argc, char *argv[])
{

    // Initialization
    int freqSamp = 16000; // Supports only 16000
    int frameSize_ms = 16; // Supports only 8ms, 15 ms and 16ms

    // Pick one of the below Input Files
    //FILE * infile = fopen("MixDown.pcm","rb"); // Raw Mixdown file
    //FILE * infile = fopen("Noise.pcm","rb");   // Noise only
    FILE * infile = fopen("Mic_16bit.pcm","rb"); // Raw mixdown recorded on Alexa enabled device

    cout << "The expected input file is 16 bits 1 Channel Signed PCM file" << endl;
    cout << "The Main.cpp function creates and calls the two instances - VADClass and FrameEnergyClass" << endl;

    // Create object pointers
    VADClass *m_VAD;
    FrameEnergyClass *m_FrameEnergyCompute;

    // Local Variables
    bool GVAD = false;
    Word64 currentFrameEnergy = 0;
    unsigned int frameNum = 0;
    int frameSize;
    frameSize = (freqSamp/1000) * frameSize_ms;
    short procBuff[frameSize];

    // Setup objects based on frameSize
    m_VAD = new VADClass (frameSize);
    m_FrameEnergyCompute = new FrameEnergyClass (frameSize);

    // Create output csv file
    FILE * energyCsv = fopen("energyVal.csv", "w+");
    fprintf(energyCsv, "Frame Number, VoiceEnergy, AmbientEnergy, VADFlag, CurrentFrameEnergy\n");

    // Process file in frameSize chunks
    cout << "Processing begins..." << endl;
    while (!feof(infile))
    {
        fread(procBuff,2,frameSize,infile);

        frameNum++;

        // Call VAD Process
        m_VAD->process(procBuff, GVAD, currentFrameEnergy);

        // Call Frame Energy Compute Process
        m_FrameEnergyCompute->process(GVAD, currentFrameEnergy);

        // m_FrameEnergyCompute->getVoicedEnergy() returns WakeWord Energy
        // m_FrameEnergyCompute->getAmbientEnergy() returns Ambient Energy
        fprintf(energyCsv, "%u, %d, %d, %d, %lld\n", frameNum, m_FrameEnergyCompute->getVoicedEnergy(), m_FrameEnergyCompute->getAmbientEnergy(), GVAD, currentFrameEnergy);
    }
    if(infile)
        fclose(infile);
    if(energyCsv)
        fclose(energyCsv);
    cout << "Processing Completed!" << endl;
    return 0;
}
