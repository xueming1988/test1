/*
 * Copyright 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * You may not use this file except in compliance with the terms and conditions set forth in the
 * accompanying LICENSE.TXT file.
 *
 * THESE MATERIALS ARE PROVIDED ON AN "AS IS" BASIS.  AMAZON SPECIFICALLY DISCLAIMS, WITH RESPECT TO
 * THESE MATERIALS, ALL
 * WARRANTIES, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR
 * PURPOSE, AND NON-INFRINGEMENT.
 */

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include "VAD_class.h"

using namespace std;

int vad_process_pcmfile(char *vadin_name, char *vadout_name, const unsigned int frameSize)
{
    printf("vad_process_pcm_file\n");
    int ret = -1;
    VADClass *m_VAD;
    bool m_GVAD = 0;
    short procBuff[frameSize];
    const unsigned int nSamples = frameSize;
    size_t rsize = 0;

    m_VAD = new VADClass(nSamples);
    FILE *infile = fopen(vadin_name, "rb");
    FILE *outfile = fopen(vadout_name, "w+");

    if (!m_VAD || !infile || !outfile) {
        goto Exit;
    }

    while (!feof(infile)) {
        rsize = fread(procBuff, 2, frameSize, infile);
        if (rsize <= 0) {
            break;
        }
        m_VAD->process(procBuff, 0, m_GVAD);

        fprintf(outfile, "%d\n", m_GVAD);
    }
    ret = 0;

Exit:
    if (infile) {
        fclose(infile);
    }

    if (outfile) {
        fclose(outfile);
    }

    if (m_VAD) {
        delete m_VAD;
    }
    cout << "vad_process_pcmfile Completed!" << endl;

    return ret;
}

#ifdef VAD_TEST
int main(int argc, char *argv[])
{
    if (argc < 4) {
        cout << "Usage: vad_test [pcmfile] [outfile] [framesizeinsamples]\n" << endl;
        return -1;
    }

    vad_process_pcmfile(argv[1], argv[2], atoi(argv[3]));
}
#endif
