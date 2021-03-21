#ifndef __SOFT_PREP_DSPC_FUNC_V2_H__
#define __SOFT_PREP_DSPC_FUNC_V2_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "soft_pre_process.h"

typedef struct _Soft_prep_dspc_wrapper {
    soft_prep_capture_CBACK *fill_callback;
    soft_prep_wakeup_CBACK *wakeup_callback;
    soft_prep_ObserverState_CBACK *ObserverState_callback;
#if defined(ENABLE_RECORD_SERVER)
    soft_prep_record_CBACK *record_callback;
    void *recPriv;
#endif
    soft_prep_micraw_CBACK *micrawCallback;
    void *micrawPriv;
    void *fillPriv;
    void *wakeupPriv;
    void *observerPriv;
    int mic_numb;
} Soft_prep_dspc_wrapper;

int enableStreamingMicrophoneData(void *handle, int value);
int enableWWE(void *handle, int value);

void *initialize(SOFT_PREP_INIT_PARAM input);

int deInitialize(void *handle);

int setFillCallback(int handle, soft_prep_capture_CBACK *callback_func, void *opaque);

int setWakeupCallBack(int handle, soft_prep_wakeup_CBACK *callback_func, void *opaque);
int setObserverStateCallBack(int handle, soft_prep_ObserverState_CBACK *callback_func,
                             void *opaque);

#if defined(ENABLE_RECORD_SERVER)
int setRecordCallBack(int handle, soft_prep_record_CBACK *callback_func, void *recPriv);
#endif
int setMicrawCallback(int handle, soft_prep_micraw_CBACK *callback_func, void *opaque);

#ifdef __cplusplus
}
#endif

#endif //__SOFT_PREP_DSPC_FUNC_V2_H__