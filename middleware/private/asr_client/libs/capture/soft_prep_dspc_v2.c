
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include "pthread.h"
#include "json-c/json.h"
#include <alsa/asoundlib.h>

#if defined(SOFT_PREP_DSPC_V2_MODULE)

#include "ccaptureGeneral.h"
#include "soft_pre_process.h"
#include "soft_prep_dspc_func.h"
#include "wm_util.h"

SOFT_PRE_PROCESS Soft_prep_dspc;

int Soft_prep_dspc_init(SOFT_PREP_INIT_PARAM input_param)
{
    Soft_prep_dspc.priv = initialize(input_param);
    Soft_prep_dspc.audio_param.BitsPerSample = 16;
    Soft_prep_dspc.audio_param.SampleRate = 16000;
#if defined(WWE_DOUBLE_TRIGGER)
    Soft_prep_dspc.audio_param.NumChannels = 4;
#else
    Soft_prep_dspc.audio_param.NumChannels = 2;
#endif
    Soft_prep_dspc.capability = SOFP_PREP_CAP_AEC;
#if defined(MICRAW_FORMAT_CHG)
    Soft_prep_dspc.Micraw_audio_param.BitsPerSample = 16;
    Soft_prep_dspc.Micraw_audio_param.SampleRate = 16000;
#else
    Soft_prep_dspc.Micraw_audio_param.BitsPerSample = 32;
    Soft_prep_dspc.Micraw_audio_param.SampleRate = 48000;
#endif
    Soft_prep_dspc.Micraw_audio_param.NumChannels = input_param.mic_numb;

    return (int)Soft_prep_dspc.priv;
}

int Soft_prep_dspc_uninit()
{
    deInitialize(Soft_prep_dspc.priv);
    Soft_prep_dspc.priv = 0;
    Soft_prep_dspc.soft_prep_fillCallback = 0;
    return 0;
}

WAVE_PARAM Soft_prep_dspc_getAudioParam() { return Soft_prep_dspc.audio_param; }

WAVE_PARAM Soft_prep_dspc_getMicrawAudioParam() { return Soft_prep_dspc.Micraw_audio_param; }

int Soft_prep_dspc_getCapability() { return Soft_prep_dspc.capability; }

int Soft_prep_dspc_enable(int value)
{
    printf("%s ++ %d\n", __func__, value);
    enableStreamingMicrophoneData(Soft_prep_dspc.priv, value);
    Soft_prep_dspc.enable = value;
    printf("%s -- %d\n", __func__, Soft_prep_dspc.enable);
    return Soft_prep_dspc.enable;
}

int Soft_prep_dspc_WWE_enable(int value)
{
    printf("%s ++ %d\n", __func__, value);
    enableWWE(Soft_prep_dspc.priv, value);
    printf("%s -- %d\n", __func__, Soft_prep_dspc.enable);
    return 0;
}

int Soft_prep_dspc_setFillCallback(soft_prep_capture_CBACK *input, void *opaque)
{
    Soft_prep_dspc.soft_prep_fillCallback = input;
    setFillCallback((int)Soft_prep_dspc.priv, input, opaque);
    return 0;
}

int Soft_prep_dspc_setWakeupCallback(soft_prep_wakeup_CBACK *input, void *opaque)
{
    setWakeupCallBack((int)Soft_prep_dspc.priv, input, opaque);
    return 0;
}

int Soft_prep_dspc_setObserverStateCallback(soft_prep_ObserverState_CBACK *input, void *opaque)
{
    setObserverStateCallBack((int)Soft_prep_dspc.priv, input, opaque);
    return 0;
}

int Soft_prep_dspc_setMicrawCallback(soft_prep_micraw_CBACK input, void *opaque)
{
    setMicrawCallback((int)Soft_prep_dspc.priv, input, opaque);
    return 0;
}

#if defined(ENABLE_RECORD_SERVER)
int Soft_prep_dspc_setRecordCallback(soft_prep_record_CBACK *input, void *recPriv)
{
    Soft_prep_dspc.soft_prep_recCallback = input;
    Soft_prep_dspc.recordPriv = recPriv;
    setRecordCallBack((int)Soft_prep_dspc.priv, input, recPriv);
    return 0;
}
#endif

SOFT_PRE_PROCESS Soft_prep_dspc = {
    .cSoft_PreP_init = Soft_prep_dspc_init,
    .cSoft_PreP_unInit = Soft_prep_dspc_uninit,
    .cSoft_PreP_setFillCallback = Soft_prep_dspc_setFillCallback,
    .cSoft_PreP_setRecordCallback = Soft_prep_dspc_setRecordCallback,
    .cSoft_PreP_getAudioParam = Soft_prep_dspc_getAudioParam,
    .cSoft_PreP_getMicRawAudioParam = Soft_prep_dspc_getMicrawAudioParam,
    .cSoft_PreP_getCapability = Soft_prep_dspc_getCapability,
    .cSoft_PreP_setWakeupCallback = Soft_prep_dspc_setWakeupCallback,
    .cSoft_PreP_setObserverStateCallback = Soft_prep_dspc_setObserverStateCallback,
    .cSoft_PreP_setMicrawCallback = Soft_prep_dspc_setMicrawCallback,
    .cSoft_PreP_enable = Soft_prep_dspc_enable,
    .cSoft_PreP_WWE_enable = Soft_prep_dspc_WWE_enable,
    .method = SOFT_PREP_METHOD_CALLBACK,
    .enable = 0,
};

#endif
