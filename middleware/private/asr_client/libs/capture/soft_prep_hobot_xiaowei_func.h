#ifndef __SOFT_PREP_HOBOT_FUNC_H__
#define __SOFT_PREP_HOBOT_FUNC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "soft_pre_process.h"

typedef struct _Soft_prep_hobot_wrapper {
    soft_prep_capture_CBACK *callback;
#if defined(ENABLE_RECORD_SERVER)
    soft_prep_record_CBACK *record_callback;
    void *recPriv;
#endif
    soft_prep_micraw_CBACK *micrawCallback;
    void *fillPriv;
    void *micrawPriv;
    SOFT_PREP_CAP_DEVICE cap_dev;
} Soft_prep_hobot_wrapper;

int enableHobotMicrophoneData(int value);

void *hobot_initialize(SOFT_PREP_INIT_PARAM input_param);

int hobot_deInitialize(void *handle);

int hobot_setFillCallback(int handle, soft_prep_capture_CBACK *callback_func, void *opaque);

int hobot_source_record(int status, int source, char *path);

int hobot_flushState(void);

#if defined(ENABLE_RECORD_SERVER)
int hobot_setRecordCallBack(int handle, soft_prep_record_CBACK *callback_func, void *recPriv);
#endif

#ifdef __cplusplus
}
#endif

#endif //__SOFT_PREP_DSPC_FUNC_H__
