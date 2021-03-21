#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include "pthread.h"
#include "wm_util.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "hrsc_sdk.h"

#define HRSC_CFG_FILE_PATH "/system/workdir/lib/hrsc/"
#define HRSC_AUTH_INFO_PATH "/vendor/"
#define RECOGNIZER_FRAME_SIZE 512

pthread_mutex_t mutex_YDserver_op = PTHREAD_MUTEX_INITIALIZER;

extern int asr_waked;
extern WIIMU_CONTEXT *g_wiimu_shm;
extern int get_asr_Weixin_enable(void);

int CCaptCosume_Asr_Weixin(char *buf, int size, void *priv);
int CCaptCosume_initAsr_Weixin(void *priv);
int CCaptCosume_deinitAsr_Weixin(void *priv);

static HrscEffectConfig effect_cfg;
static void *g_hrsc_handle = NULL;
static size_t wkp_count = 0;

// FIXME: for debug
#if defined(SECURITY_DEBUGPRINT)
static FILE *wakeup_fp = NULL;
static FILE *input_fp = NULL;
#endif

static void hrsc_auth_callback(const void *cookie, const int code)
{
    if (1 == code) {
        printf("hobot auth success\n");
    }
}

static void hrsc_event_callback(const void *cookie, HrscEventType event)
{
    switch (event) {
    case kHrscEventWkpNormal:
    case kHrscEventWkpOneshot: {
        SocketClientReadWriteMsg((char *)"/tmp/RequestASRTTS", (char *)"talkstart:1",
                                 strlen("talkstart:1"), NULL, NULL, 0);
#if defined(ASR_WAKEUP_TEXT_UPLOAD)
        asr_waked = 1;
#endif
        printf("hrsc: wakeup success, wkp count is %Zu\n", ++wkp_count);
        break;
    }
    case kHrscEventSDKStart:
        printf("hrsc sdk started, version = %s\n", HrscGetVersion());
        break;
    case kHrscEventSDKEnd:
        printf("hrsc sdk ended\n");
        break;
    default:
        break;
    }
}

static void hrsc_wakeup_data_callback(const void *cookie, const HrscCallbackData *data,
                                      const int keyword_index)
{
// SocketClientReadWriteMsg((char*)"/tmp/RequestASRTTS", (char*)"talkstart:1",
// strlen("talkstart:1"), NULL, NULL,
//                          0);
// FIXME:
// #if defined(ASR_WAKEUP_TEXT_UPLOAD)
//     asr_waked = 1;
// #endif

// FIXME: DEBUG: save wake word to file
#if defined(SECURITY_DEBUGPRINT)
    if (wakeup_fp)
        fclose(wakeup_fp);
    wakeup_fp = fopen("/tmp/wkp_record.pcm", "wb+");
    if (wakeup_fp) {
        fwrite(data->audio_buffer.audio_data, 1, data->audio_buffer.size, wakeup_fp);
        fflush(wakeup_fp);
    }
#endif
}

static int initHobotSDK()
{
    effect_cfg.input_cfg.audio_channels = 1;
    effect_cfg.input_cfg.sample_rate = 16000;
    effect_cfg.input_cfg.audio_format = kHrscAudioFormatPcm16Bit;

    effect_cfg.output_cfg.audio_channels = 1;
    effect_cfg.output_cfg.sample_rate = 16000;
    effect_cfg.output_cfg.audio_format = kHrscAudioFormatPcm16Bit;

    effect_cfg.priv = &effect_cfg;
    effect_cfg.asr_timeout = 5000;
    effect_cfg.cfg_file_path = HRSC_CFG_FILE_PATH;
    effect_cfg.custom_wakeup_word = NULL;
    effect_cfg.vad_timeout = 5000;
    effect_cfg.ref_ch_index = 2;
    effect_cfg.target_score = 0;
    effect_cfg.support_command_word = 0;
    effect_cfg.wakeup_prefix = 200;
    effect_cfg.wakeup_suffix = 200;
    effect_cfg.output_name = HRSC_AUTH_INFO_PATH; // for bitanswer info of authorization.

    effect_cfg.HrscEventCallback = hrsc_event_callback;
    effect_cfg.HrscWakeupDataCallback = hrsc_wakeup_data_callback;
    effect_cfg.HrscAuthCallback = hrsc_auth_callback;

    g_hrsc_handle = HrscInit(&effect_cfg);
    if (g_hrsc_handle == NULL) {
        printf("hrsc init error.\n");
        goto err;
    }
    printf("hrsc init success!\n");

    if (HRSC_CODE_SUCCESS != HrscStart(g_hrsc_handle)) {
        printf("hrsc start error.\n");
        HrscRelease(&g_hrsc_handle);
        g_hrsc_handle = NULL;
        goto err;
    }
    printf("hrsc start success!\n");

    return 0;
err:
    return -1;
}

static int destroyHobotSDK()
{
    if (g_hrsc_handle) {
        printf("Destroy hobot SDK.\n");
        HrscStop(g_hrsc_handle);
        HrscRelease(&g_hrsc_handle);
        g_hrsc_handle = NULL;
    }

    return 0;
}

int CCaptCosume_Asr_Weixin(char *buf, int size, void *priv)
{
    int pcmCount = 0;
    int pcmSize = size;
    int process_len = 0;
    HrscAudioBuffer hrsc_buffer;

    if (!get_asr_Weixin_enable() || !g_hrsc_handle) {
        return size;
    }

    while (g_hrsc_handle && pcmSize) {
        process_len = (pcmSize < RECOGNIZER_FRAME_SIZE) ? pcmSize : RECOGNIZER_FRAME_SIZE;

        pthread_mutex_lock(&mutex_YDserver_op);
        hrsc_buffer.audio_data = buf + pcmCount;
        hrsc_buffer.size = process_len;
        /* proccess data len's upper limit is 2 seconds */
        HrscProcess(g_hrsc_handle, &hrsc_buffer);

        pcmSize -= process_len;
        pcmCount += process_len;
        pthread_mutex_unlock(&mutex_YDserver_op);
    }

// FIXME: save input data for debug
#if defined(SECURITY_DEBUGPRINT)
    if (access("/tmp/xiaowei_debug", 0) == 0) {
        if (input_fp) {
            fwrite(buf, 1, size, input_fp);
            fflush(input_fp);
        }
    }
#endif

    return size;
}

int CCaptCosume_initAsr_Weixin(void *priv)
{
    pthread_mutex_lock(&mutex_YDserver_op);

    if (g_hrsc_handle) {
        destroyHobotSDK();
    }

    if (initHobotSDK() < 0) {
        return -1;
    }

// FIXME:
#if defined(SECURITY_DEBUGPRINT)
    if (input_fp)
        fclose(input_fp);
    input_fp = fopen("/tmp/input_record.pcm", "wb+");
#endif

    pthread_mutex_unlock(&mutex_YDserver_op);

    return 0;
}

int CCaptCosume_deinitAsr_Weixin(void *priv)
{
    pthread_mutex_lock(&mutex_YDserver_op);
    destroyHobotSDK();

// FIXME:
#if defined(SECURITY_DEBUGPRINT)
    if (input_fp) {
        fclose(input_fp);
    }
#endif
    pthread_mutex_unlock(&mutex_YDserver_op);
    return 0;
}

#ifdef __cplusplus
}
#endif