#ifndef _CCAPTUREGENERAL_H
#define _CCAPTUREGENERAL_H

#include "lp_list.h"

enum {
    AUDIO_ID_DEFAULT =
        0, // audio data is 16000hz-16bit-1ch data(does not include wake-word after triggered)
    AUDIO_ID_ASR_FAR_FIELD =
        1,              // audio data is 16000hz-16bit-1ch data(include wake-word after triggered)
    AUDIO_ID_WWE = 2,   // audio data is 16000hz-16bit-1ch data
    AUDIO_ID_DEBUG = 3, // audio data include 16000hz-16bit-2ch data after resample
    AUDIO_ID_WWE_DUAL_CH = 4, // audio data include 16000hz-16bit-2ch data after resample
    AUDIO_ID_BT_HFP = 5,      // audio data inclue 1600khz 3rd channel data after resample
};

enum {
    OUTPUT_CHANNEL_BOTH = 0,
    OUTPUT_CHANNEL_LEFT = 1,
    OUTPUT_CHANNEL_RIGHT = 2,
    OUTPUT_CHANNEL_3RD = 3,
    OUTPUT_CHANNEL_4TH = 4,
};

typedef struct _CAPTURE_COMUSER {
    struct lp_list_head list;
    int enable;
    int (*cCapture_dataCosume)(char *buffer, int bufSize, void *privdata);
    int (*cCapture_init)(void *privdata);
    int (*cCapture_deinit)(void *privdata);
    void *priv;
    int pre_process; // 1 for audio data before resample
    int id;
} CAPTURE_COMUSER;

long asr_get_keyword_begin_index(void);
long asr_get_keyword_end_index(int);
int IsCCaptStart(void);
void StartRecordServer(int nChannel);
void StopRecordServer(void);
int CCaptWakeUpFlushState(void);
void CCaptRegister(CAPTURE_COMUSER *input);
void CCaptUnregister(CAPTURE_COMUSER *input);
void CCaptFlush(void);

// Accuracy for linear interpolation
#define VAD_PROFILE_PATH "/tmp/vad_profile"

#endif
