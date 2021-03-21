#ifndef _SOFT_PRE_PROCESS_H
#define _SOFT_PRE_PROCESS_H

#include "soft_prep_rel_define.h"

#define MICRAW_FORMAT_CHG 1

#if defined(ENABLE_RECORD_SERVER)
typedef int(soft_prep_record_CBACK)(char *buf, int size, void *priv, int nStreamIndex);
#endif

typedef enum {
    SOFT_PREP_METHOD_CALLBACK =
        1, // soft pre processer fill record data through callback registed by comsumer
    SOFT_PREP_METHOD_READ =
        1, // soft pre processer cache the record data. comsumer read data through read API
} SOFT_PREP_METHOD;

typedef struct _SOFT_PREP_INIT_PARAM {
    int mic_numb;                 // number of mics
    SOFT_PREP_CAP_DEVICE cap_dev; // capture device
    const char *configFile;       // config file path
} SOFT_PREP_INIT_PARAM;

typedef struct _SOFT_PRE_PROCESS {
    // init
    int (*cSoft_PreP_init)(SOFT_PREP_INIT_PARAM input_param);

    // deinit
    int (*cSoft_PreP_unInit)(void);

    // reset
    int (*cSoft_PreP_reset)(void);

    // report output audio parameter
    WAVE_PARAM (*cSoft_PreP_getAudioParam)(void);

    // report mic raw audio parameter
    WAVE_PARAM (*cSoft_PreP_getMicRawAudioParam)(void);

    // report capability
    int (*cSoft_PreP_getCapability)(void);

    // comsumer read record data
    //  return: data filled size in byte
    int (*cSoft_PreP_readData)(char *buffer, int bufSize);

    // comsumer regitster a callback to let soft pre processer fill record data
    int (*cSoft_PreP_setFillCallback)(soft_prep_capture_CBACK *, void *opaque);

    // comsumer regitster a callback to let soft pre processer trigge a event of wake up
    int (*cSoft_PreP_setWakeupCallback)(soft_prep_wakeup_CBACK *, void *opaque);

    // comsumer regitster a callback to let soft pre processer get alexa state
    int (*cSoft_PreP_setObserverStateCallback)(soft_prep_ObserverState_CBACK *, void *opaque);

    // comsumer regitster a callback to let soft pre processer fill mic raw data
    int (*cSoft_PreP_setMicrawCallback)(soft_prep_micraw_CBACK *, void *opaque);

    // enable / disable record data
    int (*cSoft_PreP_enable)(int);

    // enable / disable wake word engine
    int (*cSoft_PreP_WWE_enable)(int);

    // flush wakeup state
    int (*cSoft_PreP_flushState)(void);

#if defined(ENABLE_RECORD_SERVER)
    int (*cSoft_PreP_setRecordCallback)(soft_prep_record_CBACK *, void *recPriv);
#endif

    // int status,int source,char path)//status: 0 stop 1 start    source:0 44.1k  1:16kprocessed
    // path
    int (*cSoft_PreP_privRecord)(int, int, char *);

    SOFT_PREP_METHOD method;
    soft_prep_capture_CBACK *soft_prep_fillCallback;
    void *priv;
#if defined(ENABLE_RECORD_SERVER)
    soft_prep_record_CBACK *soft_prep_recCallback;
    void *recordPriv;
#endif
    int enable;
    WAVE_PARAM audio_param;
    WAVE_PARAM Micraw_audio_param;
    int capability;
} SOFT_PRE_PROCESS;

#endif //_SOFT_PRE_PROCESS_H
