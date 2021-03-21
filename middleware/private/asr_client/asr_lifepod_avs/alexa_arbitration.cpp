#if ESP_V14
#include "alexa_arbitration_v14.cpp"
#include "vad_process.cpp"
#else

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#ifndef ENERGY_TEST
#include "wm_util.h"
#include "alexa_debug.h"
#endif
#include "alexa_arbitration.h"
#include "VAD_class.h"
#include "FrameEnergyComputing.h"
using namespace std;

#define DEBUG_OUTPUT
#define SAMPLE_RETA 16000
#define FRAME_LEN_MS 16

int alexa_arbitration(char *vadin_name, int *energy_voice, int *energy_noise)
{
    // Initialization
    int freqSamp = SAMPLE_RETA;      // Supports only 16000
    int frameSize_ms = FRAME_LEN_MS; // Supports only 8 or 15 ms  or 16ms
    int frameSize = 0;
    frameSize = (freqSamp / 1000) * frameSize_ms;

    // Create object pointers
    VADClass *m_VAD;
    FrameEnergyClass *m_FrameEnergyCompute;

    // Local Variables
    bool GVAD = false;
    Word64 currentFrameEnergy = 0;
    unsigned int frameNum = 0;
    short procBuff[frameSize];
    size_t rsize;

    m_VAD = new VADClass(frameSize);
    m_FrameEnergyCompute = new FrameEnergyClass(frameSize);

    FILE *infile = fopen(vadin_name, "rb");
#ifdef DEBUG_OUTPUT
    FILE *energyCsv = fopen(ESP_DEBUG_OUTPUT_FILE, "w+");
    FILE *vadout = fopen(ESP_DEBUG_VADOUT_FILE, "w+");
#endif

#ifndef ENERGY_TEST
    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "alexa_arbitration enter (%lld)\n", tickSincePowerOn());
#endif

    while (infile && !feof(infile)) {
        rsize = fread(procBuff, 2, frameSize, infile);
        if (rsize <= 0) {
            break;
        }
        frameNum++;
        m_VAD->process(procBuff, GVAD, currentFrameEnergy);
        m_FrameEnergyCompute->process(GVAD, currentFrameEnergy);

#ifdef DEBUG_OUTPUT
        fprintf(vadout, "%d\n", GVAD);
        fprintf(energyCsv, "%u, %d, %d, %d, %lld\n", frameNum,
                m_FrameEnergyCompute->getVoicedEnergy(), m_FrameEnergyCompute->getAmbientEnergy(),
                GVAD, currentFrameEnergy);
#endif
    }

    *energy_voice = m_FrameEnergyCompute->getVoicedEnergy();
    *energy_noise = m_FrameEnergyCompute->getAmbientEnergy();
#ifndef ENERGY_TEST
    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "alexa_arbitration finish process(%lld)\n", tickSincePowerOn());
#endif
    if (infile)
        fclose(infile);

#ifdef DEBUG_OUTPUT
    if (vadout)
        fclose(vadout);
    if (energyCsv)
        fclose(energyCsv);
#endif

    if (m_VAD) {
        delete m_VAD;
        m_VAD = NULL;
    }

    if (m_FrameEnergyCompute) {
        delete m_FrameEnergyCompute;
        m_VAD = NULL;
    }

    return 0;
}

#ifdef ENERGY_TEST
int main(int argc, char *argv[])
{
    if (argc < 2) {
        printf("Usage: energy_test [pcmfile] \n");
        return -1;
    }
    int engr_voice = 0;
    int engr_noise = 0;

    alexa_arbitration(argv[1], &engr_voice, &engr_noise);
    printf("alexa_arbitration finish process\n,engr_voice = %d, engr_noise = %d\n", engr_voice,
           engr_noise);
}
#endif
#endif
