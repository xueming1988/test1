#include "caiman.h"
#include <stdio.h>
#include "ccaptureGeneral.h"

int CAIMan_VAD_finished(void)
{
    ASR_Finished_Vad();
    return 0;
}

int CAIMan_ASR_Vad_start(void)
{
    ASR_Start_Vad();
    return 0;
}

int CAIMan_ASR_Vad_stop(void)
{
    ASR_Stop_Vad();
    return 0;
}

int CAIMan_ASR_Vad_enable(int talk_from)
{
    ASR_Enable_Vad(talk_from);
    return 0;
}

int CAIMan_ASR_Vad_disable(void)
{
    ASR_Disable_Vad();
    return 0;
}
