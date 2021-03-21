
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <alsa/asoundlib.h>

#include "pthread.h"
#include "json-c/json.h"

#if defined(SOFT_PREP_HOBOT_MODULE)

#include "ccaptureGeneral.h"
#include "soft_pre_process.h"
#include "soft_prep_hobot_xiaowei_func.h"
#include "wm_util.h"
#include "wm_db.h"

extern WIIMU_CONTEXT *g_wiimu_shm;
SOFT_PRE_PROCESS Soft_prep_hobot;

extern int hobot_reinit_cfg(void *handle);
extern int hobot_setMicrawCallback(int handle, soft_prep_micraw_CBACK *callback_func, void *opaque);

int Soft_prep_hobot_init(SOFT_PREP_INIT_PARAM input_param)
{
    printf("hobot_init#####\n");
    printf("Soft_prep_hobot_init +++\n");
    Soft_prep_hobot.priv = hobot_initialize(input_param);
    Soft_prep_hobot.audio_param.BitsPerSample = 16;
    Soft_prep_hobot.audio_param.SampleRate = 16000;
    Soft_prep_hobot.audio_param.NumChannels = 2; // asr + voip, 2 channels.
    Soft_prep_hobot.capability = SOFP_PREP_CAP_AEC;
    printf("Soft_prep_hobot_init ---\n");

    return (int)Soft_prep_hobot.priv;
}

int Soft_prep_hobot_uninit()
{
    hobot_deInitialize(Soft_prep_hobot.priv);
    Soft_prep_hobot.priv = 0;
    Soft_prep_hobot.soft_prep_fillCallback = 0;
    return 0;
}

int Soft_prep_hobot_reset()
{
    hobot_reinit_cfg(Soft_prep_hobot.priv);
    return 0;
}

int Soft_prep_hobot_enable(int value)
{
    printf("%s ++ %d\n", __FUNCTION__, value);
    enableHobotMicrophoneData(value);
    Soft_prep_hobot.enable = value;
    printf("%s -- %d\n", __FUNCTION__, Soft_prep_hobot.enable);
    return Soft_prep_hobot.enable;
}

WAVE_PARAM Soft_prep_hobot_getAudioParam() { return Soft_prep_hobot.audio_param; }

int Soft_prep_hobot_getCapability() { return Soft_prep_hobot.capability; }

int Soft_prep_hobot_setFillCallback(soft_prep_capture_CBACK input, void *opaque)
{
    Soft_prep_hobot.soft_prep_fillCallback = input;
    hobot_setFillCallback((int)Soft_prep_hobot.priv, input, opaque);
    return 0;
}

int Soft_prep_hobot_setMicrawCallback(soft_prep_micraw_CBACK input, void *opaque)
{
    hobot_setMicrawCallback((int)Soft_prep_hobot.priv, input, opaque);
    return 0;
}

// int status,int source,char path)//status: 0 stop 1 start    source:0 44.1k  1:16kprocessed  path
int Soft_prep_hobot_resource_record(int status, int source, char *path)
{
    hobot_source_record(status, source, path);
    return 0;
}

#if defined(ENABLE_RECORD_SERVER)
int Soft_prep_hobot_setRecordCallback(soft_prep_record_CBACK *input, void *recPriv)
{
    Soft_prep_hobot.soft_prep_recCallback = input;
    Soft_prep_hobot.recordPriv = recPriv;
    hobot_setRecordCallBack((int)Soft_prep_hobot.priv, input, recPriv);
    return 0;
}
#endif

SOFT_PRE_PROCESS Soft_prep_hobot = {
    .cSoft_PreP_init = Soft_prep_hobot_init,
    .cSoft_PreP_unInit = Soft_prep_hobot_uninit,
    .cSoft_PreP_reset = Soft_prep_hobot_reset,
    .cSoft_PreP_enable = Soft_prep_hobot_enable,
    .cSoft_PreP_setFillCallback = Soft_prep_hobot_setFillCallback,
    .cSoft_PreP_setRecordCallback = Soft_prep_hobot_setRecordCallback,
    .cSoft_PreP_setObserverStateCallback = NULL,
    .cSoft_PreP_setMicrawCallback = Soft_prep_hobot_setMicrawCallback,
    .cSoft_PreP_getAudioParam = Soft_prep_hobot_getAudioParam,
    .cSoft_PreP_getCapability = Soft_prep_hobot_getCapability,
    .cSoft_PreP_flushState = NULL,
    .cSoft_PreP_privRecord = Soft_prep_hobot_resource_record,
    .method = SOFT_PREP_METHOD_CALLBACK,
    .enable = 0,
};

#endif
