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

using namespace std;

int main(int argc, char *argv[])
{
    VADClass *m_VAD;
    bool m_GVAD;

    int frameSize = 128;
    short procBuff[frameSize];
    const unsigned int nSamples = 128;
    unsigned int nChannel = 1;

    m_VAD = new VADClass (nSamples);

    FILE * infile = fopen("MixDown.pcm","rb");
    //FILE * infile = fopen("Noise.pcm","rb");
    FILE * energyCsv = fopen("energyVal.csv", "w+");

    while (!feof(infile))
    {
        fread(procBuff,2,frameSize,infile);

        m_VAD->process(procBuff, 0, m_GVAD);

        fprintf(energyCsv, "%d", m_GVAD);
    }

    if( infile )
        fclose(infile);

    if( energyCsv )
        fclose(energyCsv);
    cout << "Processing Completed!" << endl;
    return 0;
}
