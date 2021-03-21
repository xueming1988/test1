#ifndef __ALEXA_H__
#define __ALEXA_H__

#include "alexa_debug.h"
#include "alexa_api2.h"

#ifndef MAX_SOCKET_RECV_BUF_LEN
#define MAX_SOCKET_RECV_BUF_LEN (65536)
#endif

typedef struct {
    volatile int login;
    volatile int on_line;
    volatile int http_ret_code;
    volatile int record_to_sd;
    volatile int asr_test_flag;
    time_t last_dialog_ts;
    volatile long long record_start_ts;
    volatile long long record_end_ts;
    volatile long long vad_ts;
} alexa_dump_t;

#define ASR_TEST_FLAG_NOT_PAUSE (1 << 0) // 0
//#define ASR_TEST_FlAG_HAVE_PROMT        (1<<1)          // 1
#define ASR_TEST_FlAG_I2S_REFRENCE (1 << 2)    // 4
#define ASR_TEST_FlAG_MIC_BYPASS (1 << 3)      // 8
#define ASR_TEST_FlAG_LOCAL_VAD (1 << 4)       // 16
#define ASR_TEST_FlAG_WAKEUP_WORD (1 << 5)     // 32
#define ASR_TEST_FlAG_BG_MUSIC_VOL (0xff << 8) // bit8-15   val * 0x100

#define ASR_IS_NEED_PAUSE(flag) ((flag)&ASR_TEST_FLAG_NOT_PAUSE)
#define ASR_IS_I2S_REFRENCE(flag) ((flag)&ASR_TEST_FlAG_I2S_REFRENCE)
#define ASR_IS_MIC_BYPASS(flag) ((flag)&ASR_TEST_FlAG_MIC_BYPASS)

#if defined(TENCENT) // tencent
#define ASR_IS_SERVER_VAD(flag) (((flag)&ASR_TEST_FlAG_LOCAL_VAD))
#else
#define ASR_IS_SERVER_VAD(flag) (!((flag)&ASR_TEST_FlAG_LOCAL_VAD))
#endif

#define ASR_IS_WAKEUP_WORD(flag) (!((flag)&ASR_TEST_FlAG_WAKEUP_WORD))
#define ASR_BG_MUSIC_VOL(flag) (((flag)&ASR_TEST_FlAG_BG_MUSIC_VOL) >> 8)

extern alexa_dump_t alexa_dump_info;

#define ALEXA_DATE_FMT "%d%.2d%.2d_%.2d_%.2d_%.2d"
#define ALEXA_RECORD_FMT "%s/%s_record.pcm"
#define ALEXA_UPLOAD_FMT "%s/%s_upload.pcm"

#define ALEXA_UPLOAD_FILE "/tmp/web/alexa_upload.pcm"
#define ALEXA_RECORD_FILE "/tmp/web/alexa_record.pcm"
#define ALEXA_ERROR_FILE "/tmp/web/alexa_response.txt"
#define ALEXA_WAKEWORD_FILE "/tmp/web/wake_word.pcm"

#ifdef __cplusplus
extern "C" {
#endif

int AlexaInit(void);

int AlexaUnInit(void);

/*
 * init the upload ringbuffer
 * retrurn writen size
 */
void AlexaInitRecordData(void);

/*
 * write record data from record moudle
 * retrurn writen size
 */
int AlexaWriteRecordData(char *data, size_t size);

/*
 * stop write record data when finished speek
 */
int AlexaFinishWriteData(void);

/*
 * stop write record data by other
 */
int AlexaStopWriteData(void);

/*
 *  return 0 : not login
 */
int AlexaCheckLogin(void);

long get_play_pos(void);

void alexa_update_action_time(void);

int alexa_get_tts_status(void);

void alexa_book_ctrol(int pause_flag);

#ifdef __cplusplus
}
#endif

#endif
