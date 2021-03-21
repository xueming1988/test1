
#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <alsa/asoundlib.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>

#include "caiman.h"
#include "wm_util.h"
#include "ccaptureGeneral.h"

#include "record_resample.h"
#include "playback_record.h"
#include "save_record_file.h"
#include "lpcfg.h"

#ifdef USE_LINKPLAY_DSNOOP
#include "shm_record_comms.h"
MY_SNOOP *asr_dsnoop = NULL;
#endif

#if !defined(ASR_DCS_MODULE)
#include "../../asr_avs/alexa.h"
#else
#include "alexa_debug.h"
#endif

#ifdef ENABLE_RECORD_SERVER
#include "AudioCaptureServer.h"
#endif

#ifdef ASR_DCS_SHARP_MODULE
#include "shm_record_sharp.h"
FOXCOM_SNOOP *gFoxcom_snoop = NULL;
#endif

#ifdef SOFT_PREP_MODULE
#include "soft_pre_process.h"
#endif

#define SAMPLE_RATE_FAR (USB_CAPTURE_SAMPLERATE)
int g_bit_per_sample = 16;
char g_cap_dev[32];
int g_cap_rate = 44100;
int g_cap_channel = 2;
const static int g_cap_resample_rate = 16000;

#ifdef AVS_CLOUD_BASED_WWV
#define MAX_WAKE_WORD_MS (1400) // 1400ms
#else
#define MAX_WAKE_WORD_MS (1200) // 1200ms
#endif

#if defined(NAVER_LINE)
#define EMPTY_BUFFER_BEFORE_KEYWORD (300) // 300ms
#endif

#define MAX_WAKE_WORD (SAMPLE_RATE_FAR * 2 * MAX_WAKE_WORD_MS / 1000)

extern WIIMU_CONTEXT *g_wiimu_shm;
extern CAPTURE_COMUSER Cosume_Playback;
#if defined(SOFT_PREP_MODULE)
extern SOFT_PRE_PROCESS *g_soft_pre_process;
#endif

static resample_context_t resample_info[3];     //[0] for left ch, [1] for right ch, [2] for comms
static resample_context_t resample_info_ext[2]; //[0] for 3rd ch, [1] for 4th ch
static char wake_word_buf[MAX_WAKE_WORD];
static char esp_buf[MAX_WAKE_WORD];
static int wake_word_offset = 0;
static int esp_word_offset = 0;
int asr_waked = 0;
int amazon_detectionCallback_waked = 0;
int ignore_reset_trigger = 0;
static char *wake_word_dropped_buf = NULL;
static int wake_word_dropped_len = 0;
unsigned int trigger_delayed_bytes;

#if defined(NAVER_LINE) || defined(ASR_XIAOWEI_MODULE) || defined(ASR_WEIXIN_MODULE)
int GetAudioInputProcessorObserverState(void *handle) { return 0; }
#else
extern int GetAudioInputProcessorObserverState(void *handle);
#endif

#if defined(NAVER_LINE)
long asr_get_keyword_begin_index(void)
{
    long keyword_begin_index = 0;
    int random;

    srand((int)time(NULL));
    random = rand() % 100;

    keyword_begin_index = (EMPTY_BUFFER_BEFORE_KEYWORD - random) * SAMPLE_RATE_FAR / 1000;

    wiimu_log(1, 0, 0, 0, "%s %ld, max buffer %ld", __FUNCTION__, keyword_begin_index,
              MAX_WAKE_WORD);
    return keyword_begin_index;
}

long asr_get_keyword_end_index(int index)
{
    long keyword_end_index = 0;
    int random;
    int len;

    srand((int)time(NULL));
    random = rand() % 100;

    if (index == 1)
        len = g_wiimu_shm->trigger1_len_ms;
    else if (index == 2)
        len = g_wiimu_shm->trigger2_len_ms;

    keyword_end_index = (len + EMPTY_BUFFER_BEFORE_KEYWORD + random) * SAMPLE_RATE_FAR / 1000;

    wiimu_log(1, 0, 0, 0, "%s %ld", __FUNCTION__, keyword_end_index);
    return keyword_end_index;
}
#endif

#if defined(UPLOAD_PRERECORD)
static char *pre_record_word_buf = NULL;
static int pre_record_word_size = 0;
int pre_record_word_load(char *pre_record_file)
{
    int ret = -1;
    int size = 0;
    if (access(pre_record_file, 0) == 0) {
        size = get_fileSize(pre_record_file);
        printf("pre_record file size:%d\n", size);
        pre_record_word_buf = malloc(size);
        if (!pre_record_word_buf) {
            printf("Error: malloc memory failed in %s\n", __func__);
            return ret;
        }

        FILE *fre = fopen(pre_record_file, "r");
        if (fre) {
            pre_record_word_size = fread(pre_record_word_buf, 1, size, fre);
            printf("read pre_record:%d\n", pre_record_word_size);
            fclose(fre);
            ret = pre_record_word_size;
        }
    } else {
        printf("Error: pre_record_file:%s not found!\n", pre_record_file);
    }
    return ret;
}

#endif

static void save_wake_word(char *buf, int len)
{
    int len1;
    int len2 = 0;
    char *p = buf;

    if (!far_field_judge(g_wiimu_shm))
        return;

// save the data will be overwritten (after waked but not consumed)
#if defined(AVS_CLOUD_BASED_WWV) || defined(ASR_WAKEUP_TEXT_UPLOAD)
    if (asr_waked && len > 0) {
        if (!wake_word_dropped_buf)
            wake_word_dropped_buf = malloc(len);
        else {
            char *p = realloc((void *)wake_word_dropped_buf, wake_word_dropped_len + len);
            wake_word_dropped_buf = p;
        }

        len1 = len;
        if (wake_word_offset + len > MAX_WAKE_WORD) {
            len1 = MAX_WAKE_WORD - wake_word_offset;
            len2 = len - len1;
        }

        if (len1 > 0)
            memcpy(wake_word_dropped_buf + wake_word_dropped_len, &wake_word_buf[wake_word_offset],
                   len1);

        if (len2 > 0)
            memcpy(wake_word_dropped_buf + wake_word_dropped_len, &wake_word_buf[0], len2);
        wake_word_dropped_len += len;
        if (wake_word_dropped_len >= MAX_WAKE_WORD * 5) {
            wake_word_dropped_len = 0;
            free(wake_word_dropped_buf);
            wake_word_dropped_buf = 0;
        }
    } else {
        wake_word_dropped_len = 0;
        free(wake_word_dropped_buf);
        wake_word_dropped_buf = 0;
    }

    len2 = 0;
#endif

    if (len > MAX_WAKE_WORD) {
        p = buf + (len - MAX_WAKE_WORD);
        len = MAX_WAKE_WORD;
    }
    len1 = len;
    if (wake_word_offset + len > MAX_WAKE_WORD) {
        len1 = MAX_WAKE_WORD - wake_word_offset;
        len2 = len - len1;
    }

    if (len1 > 0) {
        memcpy(&wake_word_buf[wake_word_offset], p, len1);
        wake_word_offset = (wake_word_offset + len1) % MAX_WAKE_WORD;
        p += len1;
    }

    if (len2 > 0) {
        memcpy(&wake_word_buf[wake_word_offset], p, len2);
        wake_word_offset = (wake_word_offset + len2) % MAX_WAKE_WORD;
    }
}

void dump_wake_word(char *path)
{
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC);
    if (fd >= 0) {
        int len = MAX_WAKE_WORD - wake_word_offset;
        int len2 = 0;
        if (len == 0)
            len = MAX_WAKE_WORD;
        else
            len2 = MAX_WAKE_WORD - len;
        if (len > 0)
            write(fd, &wake_word_buf[wake_word_offset], len);

        if (len2 > 0)
            write(fd, &wake_word_buf[0], len2);
        close(fd);
        wiimu_log(1, 0, 0, 0, "%s save %s succeed", __FUNCTION__, path);
    }
}

static void save_esp_wake_word(char *buf, int len)
{
    int len1;
    int len2 = 0;
    char *p = buf;

    if (!(g_wiimu_shm->priv_cap1 & (1 << CAP1_USB_RECORDER)))
        return;
    if (len > MAX_WAKE_WORD) {
        p = buf + (len - MAX_WAKE_WORD);
        len = MAX_WAKE_WORD;
    }
    len1 = len;
    if (esp_word_offset + len > MAX_WAKE_WORD) {
        len1 = MAX_WAKE_WORD - esp_word_offset;
        len2 = len - len1;
    }

    if (len1 > 0) {
        memcpy(&esp_buf[esp_word_offset], p, len1);
        esp_word_offset = (esp_word_offset + len1) % MAX_WAKE_WORD;
        p += len1;
    }

    if (len2 > 0) {
        memcpy(&esp_buf[esp_word_offset], p, len2);
        esp_word_offset = (esp_word_offset + len2) % MAX_WAKE_WORD;
    }
}

//#define DEBUG_FILE
//#define DEBUG_DUMP_CAP

//#ifdef USB_CAPDSP_MODULE
// static int CAPTURE_SHARE_BUF_SIZE = 131072;
//#else
// static int CAPTURE_SHARE_BUF_SIZE = 176400;
//#endif

// far feild : 2 channel, 16000 rates, cache 3s record data
#if CXDISH_6CH_SAMPLE_RATE
#define CAPTURE_BUF_LENGTH (SAMPLE_RATE_FAR * 2 * 2) //(128000 + 64000)
#else
#if MAX_RECORD_TIME_IN_MS
#define CAPTURE_BUF_LENGTH ((MAX_RECORD_TIME_IN_MS)*32 + 10240)
#else
#define CAPTURE_BUF_LENGTH (128000 + 64000)
#endif
#endif
static int CAPTURE_SHARE_BUF_SIZE = CAPTURE_BUF_LENGTH;

static int g_capture_filled = 0;
static char *g_temp_buffer = 0;

LP_LIST_HEAD(list_capture_cosumer);

static char *cap_cache_buf = 0;
static volatile int cap_cache_write = 0;
static volatile int cap_cache_read = 0;
static volatile int cap_cache_len = 0;
static volatile int cap_cache_flush = 1;

static char *work_buf = NULL;
static int work_buf_size = 0;
static char *downSample_buf_left = NULL;
static char *downSample_buf_right = NULL;
static char *downSample_buf_3rd = NULL;
static char *downSample_buf_4th = NULL;
static char *downSample_buf_comms = NULL;

static int CCap_capture_start = 0;
static pthread_t CCap_capture_thread = 0;
static pthread_t CCap_cosume_thread = 0;
static pthread_mutex_t mutex_ccap = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_ccap_list = PTHREAD_MUTEX_INITIALIZER;

static snd_pcm_hw_params_t *record_hw_params = 0;
static snd_pcm_sw_params_t *record_sw_params = 0;

static int alsa_cap_ch = 2;

#if defined(ASR_DCS_MODULE)
static int cxdish_Version_update(void)
{
    FILE *fp;
    char tmp_buf[128] = {0};

    system("cxdish fw-version > /tmp/cxdish_ver");

    if ((fp = fopen("/tmp/cxdish_ver", "r")) == NULL) {
        return -1;
    }

    if (fgets(tmp_buf, 64, fp) == NULL) {
        if (feof(fp) != 0) { // get the end of file
            printf("get the end of file\n");
        } else {
            fclose(fp);
            return -1;
        }
    }
    fclose(fp);

    if ((access("/tmp/cxdish_ver", 0)) == 0)
        system("rm -rf /tmp/cxdish_ver");
    memset(g_wiimu_shm->asr_dsp_fw_ver, 0, sizeof(g_wiimu_shm->asr_dsp_fw_ver));
    strncpy(g_wiimu_shm->asr_dsp_fw_ver, tmp_buf,
            (strlen(tmp_buf) > sizeof(g_wiimu_shm->asr_dsp_fw_ver))
                ? sizeof(g_wiimu_shm->asr_dsp_fw_ver)
                : strlen(tmp_buf));
    return 0;
}
#endif

static int set_capture_params(snd_pcm_t *handle, int sampling_rate)
{
    int err;
    int channels = 2;
    // size_t n;
    int rate = sampling_rate;
    int buffer_time = 0;
    int period_time = 0;
    snd_pcm_uframes_t chunk_size = 2048;
    snd_pcm_uframes_t buffer_size;

    // snd_pcm_uframes_t start_threshold, stop_threshold;
    snd_pcm_hw_params_alloca(&record_hw_params);
    snd_pcm_sw_params_alloca(&record_sw_params);
    err = snd_pcm_hw_params_any(handle, record_hw_params);
    if (err < 0) {
        printf("Broken configuration for this PCM: no configurations available\n");
    }
    err = snd_pcm_hw_params_set_access(handle, record_hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        printf("Access type not available\n");
        return -1;
    }
    err = snd_pcm_hw_params_set_format(handle, record_hw_params, SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
        printf("Sample format non available\n");
        return -1;
    }

    err = snd_pcm_hw_params_test_channels(handle, record_hw_params, channels);
    if (err < 0) {
        printf("Test Set Channel to %d not available\n", channels);

        channels = 1;
        err = snd_pcm_hw_params_test_channels(handle, record_hw_params, channels);
        if (err < 0) {
            printf("Test Set Channel to %d not available\n", channels);
            return -1;
        }
    }

    err = snd_pcm_hw_params_set_channels(handle, record_hw_params, channels);
    if (err < 0) {
        printf("Channels count non available\n");
        return -1;
    }
    alsa_cap_ch = channels;
    g_cap_channel = alsa_cap_ch;

    err = snd_pcm_hw_params_set_rate_near(handle, record_hw_params, (unsigned int *)&rate, 0);
    if (err < 0) {
        printf("sample rate set faile %d\n", rate);
    }

    err = snd_pcm_hw_params_get_buffer_time_max(record_hw_params, &buffer_time, 0);
    // printf("buffer_time %d\n", buffer_time);

    if (buffer_time > 500000)
        buffer_time = 500000;
    period_time = buffer_time / 4;
    if (g_cap_rate <= USB_CAPTURE_SAMPLERATE) {
        if (buffer_time > 200000)
            buffer_time = 200000;
        period_time = buffer_time / 4;
    }
    err = snd_pcm_hw_params_set_buffer_time_near(handle, record_hw_params, &buffer_time, 0);
    if (err < 0) {
        printf("buffer_time set faile %d\n", buffer_time);
    }
    err = snd_pcm_hw_params_set_period_time_near(handle, record_hw_params, &period_time, 0);
    if (err < 0) {
        printf("period_time set faile %d\n", period_time);
    }

    err = snd_pcm_hw_params(handle, record_hw_params);
    if (err < 0) {
        printf("snd_pcm_hw_params set faile err=%d\n", err);
    }

    snd_pcm_hw_params_get_period_size(record_hw_params, &chunk_size, 0);
    snd_pcm_hw_params_get_buffer_size(record_hw_params, &buffer_size);
    if (chunk_size == buffer_size) {
        printf("Can't use period equal to buffer size (%lu == %lu)\n", chunk_size, buffer_size);
    }

    snd_pcm_hw_params_get_channels(record_hw_params, &channels);
    printf("capture config channel = %d\n", channels);

// snd_pcm_sw_params_current(handle, record_sw_params);
// err = snd_pcm_sw_params_set_avail_min(handle, record_sw_params, chunk_size);
// if (err < 0)
//  printf("chunk_size non available\n");

/* round up to closest transfer boundary */
// err = snd_pcm_sw_params_set_start_threshold(handle, swparams, buffer_size);
// err = snd_pcm_sw_params_set_stop_threshold(handle, swparams, stop_threshold);

// if (snd_pcm_sw_params(handle, record_sw_params) < 0) {
//  printf("unable to install sw params\n");
//}
#ifndef EN_AVS_COMMS
    err = snd_pcm_nonblock(handle, 1);
#endif
    if (err < 0) {
        printf("capture nonblock setting error: %s\n", snd_strerror(err));
        return -1;
    }

    printf("capture rate = %d/%d, chunk=%d, buffer=%d, period=%d, buffer_time=%d\n", rate,
           sampling_rate, chunk_size, buffer_size, period_time, buffer_time);
    return 0;
}

#ifdef ENABLE_RECORD_SERVER
CAPTURE_COMUSER comuser_capture_server = {
    .enable = 0,
    .pre_process = 0,
    .id = AUDIO_ID_DEBUG, // after process, but keep left & right channel
    .cCapture_dataCosume = CaptureServerPostSaveData,
    .cCapture_init = CaptureServerInit,
    .cCapture_deinit = CaptureServerDeinit,
    .priv = NULL,
};

void StartRecordServer(int nChannel)
{
    struct _stCaptureServerInfo *pInfo;

    if (comuser_capture_server.priv != NULL) {
        printf("server is running, cann't running again\n");
        return;
    }

    printf("record server will start, channel is %d\n", nChannel);
    comuser_capture_server.enable = 1;
    pInfo = (struct _stCaptureServerInfo *)malloc(sizeof(struct _stCaptureServerInfo));
    memset(pInfo, 0, sizeof(struct _stCaptureServerInfo));
    comuser_capture_server.priv = (void *)pInfo;
    pInfo->nServerPort = 7513;
    // comuser_capture_server.pre_process = (nChannel == 1) ? 0 : 1;
    comuser_capture_server.cCapture_init(comuser_capture_server.priv);

#ifdef USB_CAPDSP_MODULE
    pInfo->nSampleRate = SAMPLE_RATE_FAR;
#else
    if (g_wiimu_shm->priv_cap1 & (1 << CAP1_USB_RECORDER)) {
        pInfo->nSampleRate = SAMPLE_RATE_FAR;
    } else {
        pInfo->nSampleRate = 44100;
    }
#endif
    pInfo->nSampleRate = 16000;
    pInfo->nChannel = nChannel;
    pInfo->nBits = 16;
    pInfo->nPrepSampleRate = 48000;
    pInfo->nPrepChannel = 8;

#if defined(SOFT_PREP_MODULE)
#if defined(SOFT_PREP_DSPC_MODULE) || defined(SOFT_PREP_DSPC_V2_MODULE)
    pInfo->nPrepBits = 32;
#elif defined(SOFT_PREP_HOBOT_MODULE)
    pInfo->nPrepBits = 16;
#else
    printf("No Soft PreProcess defined\n");
#endif
    if (g_soft_pre_process && g_soft_pre_process->cSoft_PreP_setRecordCallback) {
        g_soft_pre_process->cSoft_PreP_setRecordCallback(CaptureServerStreamSaveData,
                                                         &pInfo->stPrepParam);
    }
#endif

    CCaptRegister(&comuser_capture_server);
}

void StopRecordServer(void)
{
    if (comuser_capture_server.priv) {
#if defined(SOFT_PREP_MODULE)
        if (g_soft_pre_process && g_soft_pre_process->cSoft_PreP_setRecordCallback) {
            g_soft_pre_process->cSoft_PreP_setRecordCallback(NULL, NULL);
        }
#endif
        CCaptUnregister(&comuser_capture_server);
        comuser_capture_server.enable = 0;
        comuser_capture_server.cCapture_deinit(comuser_capture_server.priv);

        if (comuser_capture_server.priv != NULL)
            free(comuser_capture_server.priv);

        comuser_capture_server.priv = NULL;
    }
}
#endif

static int g_capture_thread_exist = 0;

void *CCaptCosumeThread(void *arg)
{
    int err;
    int i = 0;
    char *voip_channel;

    printf("ASR consume begin pid=%d\n", getpid());
#ifdef USE_LINKPLAY_DSNOOP
    asr_dsnoop = ShmCreateforComms();
    ShmRecordInit(asr_dsnoop, 16000);
#endif

    if (lpcfg_read_string("audioFrontEnd.voipChannel", &voip_channel) != 0) {
        fprintf(stderr, "can not get audioFrontEnd.voipChannel from lpcfg\n");
        return NULL;
    }
    printf("audioFrontEnd.voipChannel = %s\n", voip_channel);

    while (CCap_capture_start) {
        int res = 0;
        int read_len = 0;
        int postSize = 0;
        int post_size_3rd = 0;
        int post_size_4th = 0;
        int avail;
        struct lp_list_head *pos, *n;
        CAPTURE_COMUSER *cap_cosumer_entry = NULL;
        static int left_post_size = 0;
        static int downSample_buf_size = 0;
        if (Cosume_Playback.enable == 1) {
            if (g_capture_filled) {
                Notify_IOGuard();
                pthread_mutex_lock(&mutex_ccap);
                read_len = (CAPTURE_SHARE_BUF_SIZE - cap_cache_read) > cap_cache_len
                               ? cap_cache_len
                               : (CAPTURE_SHARE_BUF_SIZE - cap_cache_read);
                read_len = read_len > 65536 ? 65536 : read_len;
                if (read_len) {
                    memcpy(g_temp_buffer, cap_cache_buf + cap_cache_read, read_len);
                    cap_cache_len -= read_len;
                    cap_cache_read += read_len;
                    cap_cache_read %= CAPTURE_SHARE_BUF_SIZE;
                }
                pthread_mutex_unlock(&mutex_ccap);

                if (read_len) {
                    CCaptCosume_Playback(g_temp_buffer, read_len, &Cosume_Playback);
                } else if (!cap_cache_len) {
                    usleep(20000);
                }
            }

            if (!cap_cache_len) {
                g_capture_filled = 0;
                Notify_IOGuard();
            }
        } else {
            ClosePlaybackHandle();
            if (!lp_list_empty(&list_capture_cosumer)) {
                pthread_mutex_lock(&mutex_ccap);
                read_len = (CAPTURE_SHARE_BUF_SIZE - cap_cache_read) > cap_cache_len
                               ? cap_cache_len
                               : (CAPTURE_SHARE_BUF_SIZE - cap_cache_read);
                read_len = read_len > 65536 ? 65536 : read_len;
                if (g_cap_rate <= USB_CAPTURE_SAMPLERATE)
                    read_len = read_len > 24000 ? 24000 : read_len;
#if defined(YD_AMAZON_MODULE)
                // read_len = (read_len/640)*640;
                // read_len =  read_len > 9600? 9600:read_len;
                //             640 * 32, 28, 24, 20, 16, 8, 4, 2, 1
                //             160 sample * 2ch * 2byte
                int align_640[] = {20480, 17920, 15360, 12800, 10240, 5120, 2560, 1280, 640};
                for (i = 0; i < 9; ++i) {
                    if (read_len >= align_640[i] * g_cap_channel / 2) {
                        read_len = align_640[i] * g_cap_channel / 2;
                        break;
                    }
                }
                if (read_len < 640 * g_cap_channel / 2) {
                    read_len = 0;
                }
#endif

                // printf("****%s:reladlen=%d\n",__func__,read_len);
                if (read_len > 0) {
                    // long long start_ts = tickSincePowerOn();
                    memcpy(g_temp_buffer, cap_cache_buf + cap_cache_read, read_len);
                    cap_cache_len -= read_len;
                    cap_cache_read += read_len;
                    cap_cache_read %= CAPTURE_SHARE_BUF_SIZE;
                }
                pthread_mutex_unlock(&mutex_ccap);
                if (read_len > 0) {
                    // step 0, for those pre-process cosumer, directly pass data
                    pthread_mutex_lock(&mutex_ccap_list);
                    if (!lp_list_empty(&list_capture_cosumer)) {
                        lp_list_for_each(pos, &list_capture_cosumer)
                        {
                            cap_cosumer_entry = lp_list_entry(pos, CAPTURE_COMUSER, list);
                            if (cap_cosumer_entry->enable &&
                                cap_cosumer_entry->cCapture_dataCosume &&
                                cap_cosumer_entry->pre_process) {
                                cap_cosumer_entry->cCapture_dataCosume(g_temp_buffer, read_len,
                                                                       cap_cosumer_entry->priv);
                            }
                        }
                    }
                    pthread_mutex_unlock(&mutex_ccap_list);
                    // step 1, downsample if any capture cosumer registe
                    if (!downSample_buf_left) {
                        printf("CCaptCosumeThread malloc size to %d\n", read_len);
                        downSample_buf_left = malloc(read_len);
                        downSample_buf_right = malloc(read_len);
                        downSample_buf_3rd = malloc(read_len);
                        downSample_buf_4th = malloc(read_len);
                        downSample_buf_comms = malloc(read_len);
                        downSample_buf_size = read_len;
                    } else if (read_len > downSample_buf_size) {
                        printf("CCaptCosumeThread re-alloc size to %d\n", read_len);
                        free(downSample_buf_left);
                        free(downSample_buf_right);
                        free(downSample_buf_3rd);
                        free(downSample_buf_4th);
                        free(downSample_buf_comms);
                        downSample_buf_left = malloc(read_len);
                        downSample_buf_right = malloc(read_len);
                        downSample_buf_3rd = malloc(read_len);
                        downSample_buf_4th = malloc(read_len);
                        downSample_buf_comms = malloc(read_len);
                        downSample_buf_size = read_len;
                    }

                    CCapt_postprocess(&resample_info[0], &work_buf, &work_buf_size,
                                      &resample_info[0].g_lin_pol, g_temp_buffer, read_len,
                                      downSample_buf_left, &left_post_size, g_cap_channel,
                                      OUTPUT_CHANNEL_LEFT);

                    CCapt_postprocess(&resample_info[1], &work_buf, &work_buf_size,
                                      &resample_info[1].g_lin_pol, g_temp_buffer, read_len,
                                      downSample_buf_right, &postSize, g_cap_channel,
                                      OUTPUT_CHANNEL_RIGHT);

                    if (g_cap_channel > 2) {
                        CCapt_postprocess(&resample_info_ext[0], &work_buf, &work_buf_size,
                                          &resample_info_ext[0].g_lin_pol, g_temp_buffer, read_len,
                                          downSample_buf_3rd, &post_size_3rd, g_cap_channel,
                                          OUTPUT_CHANNEL_3RD);

                        CCapt_postprocess(&resample_info_ext[1], &work_buf, &work_buf_size,
                                          &resample_info_ext[1].g_lin_pol, g_temp_buffer, read_len,
                                          downSample_buf_4th, &post_size_4th, g_cap_channel,
                                          OUTPUT_CHANNEL_4TH);
                    }

                    // save recent 1200ms data for esp (right channel)
                    save_esp_wake_word(downSample_buf_right, postSize);

                    // save recent 800ms data for wake word
                    if (g_wiimu_shm->asr_ch_trigger == OUTPUT_CHANNEL_LEFT)
                        save_wake_word(downSample_buf_left, postSize);
                    else
                        save_wake_word(downSample_buf_right, postSize);
#ifdef USE_LINKPLAY_DSNOOP
                    if (asr_dsnoop->sample_rate == g_cap_resample_rate) {
                        if (STR_CMP(voip_channel, "right") == 0)
                            ShmRecordWrite(asr_dsnoop, downSample_buf_right, left_post_size);
                        else if (STR_CMP(voip_channel, "left") == 0)
                            ShmRecordWrite(asr_dsnoop, downSample_buf_left, left_post_size);
                        else if (g_cap_channel > 2) {
                            if (STR_CMP(voip_channel, "3rd") == 0)
                                ShmRecordWrite(asr_dsnoop, downSample_buf_3rd, left_post_size);
                            else if (STR_CMP(voip_channel, "4th") == 0)
                                ShmRecordWrite(asr_dsnoop, downSample_buf_4th, left_post_size);
                        } else
                            break;
                    } else {
                        int comms_post_size = 0;
                        if (!resample_info[2].inited) {
                            CCapt_resample_init(&resample_info[2], g_cap_rate,
                                                asr_dsnoop->sample_rate,
                                                &resample_info[2].g_lin_pol);
                            printf("resample comms data to %d\n", asr_dsnoop->sample_rate);
                            resample_info[2].inited = 1;
                        }

                        int channel;
                        if (STR_CMP(voip_channel, "right") == 0)
                            channel = OUTPUT_CHANNEL_RIGHT;
                        else if (STR_CMP(voip_channel, "left") == 0)
                            channel = OUTPUT_CHANNEL_LEFT;
                        else if (g_cap_channel > 2) {
                            if (STR_CMP(voip_channel, "3rd") == 0)
                                channel = OUTPUT_CHANNEL_3RD;
                            else if (STR_CMP(voip_channel, "4th") == 0)
                                channel = OUTPUT_CHANNEL_4TH;
                        } else
                            break;

                        CCapt_postprocess(&resample_info[2], &work_buf, &work_buf_size,
                                          &resample_info[2].g_lin_pol, g_temp_buffer, read_len,
                                          downSample_buf_comms, &comms_post_size, g_cap_channel,
                                          channel);

                        ShmRecordWrite(asr_dsnoop, downSample_buf_comms, comms_post_size);
                    }
#endif

                    // step 2, pass the downsample to cosumer
                    pthread_mutex_lock(&mutex_ccap_list);
                    int need_upload_wake_word = asr_waked;
#if defined(UPLOAD_PRERECORD)
                    // for push to talk key
                    if (g_wiimu_shm->push_talk_upload_word) {
                        need_upload_wake_word = 1;
                    }
#endif
                    if (!lp_list_empty(&list_capture_cosumer)) {
                        lp_list_for_each(pos, &list_capture_cosumer)
                        {
                            cap_cosumer_entry = lp_list_entry(pos, CAPTURE_COMUSER, list);
                            // save two channel data after processed for record tool
                            if (cap_cosumer_entry->enable &&
                                cap_cosumer_entry->cCapture_dataCosume &&
                                cap_cosumer_entry->id == AUDIO_ID_DEBUG) {
                                int16_t *p_buffer_left_right = NULL;
                                int16_t *p_tmp_buffer = NULL;
                                int16_t *p_left = (int16_t *)downSample_buf_left;
                                int16_t *p_right = (int16_t *)downSample_buf_right;
                                int16_t *p_3rd = (int16_t *)downSample_buf_3rd;
                                int16_t *p_4th = (int16_t *)downSample_buf_4th;
                                static int total_post_len = 0;
                                if (postSize > 0) {
                                    p_buffer_left_right = (int16_t *)malloc(postSize * 4);
                                    memset((void *)p_buffer_left_right, 0, postSize * 4);
                                    if (p_buffer_left_right) {
                                        int i;
                                        p_tmp_buffer = p_buffer_left_right;
                                        for (i = 0; i < postSize / 2; ++i) {
                                            *p_tmp_buffer++ = *p_left++;
                                            *p_tmp_buffer++ = *p_right++;
                                            *p_tmp_buffer++ = *p_3rd++;
                                            *p_tmp_buffer++ = *p_4th++;
                                        }
                                        total_post_len += postSize * 4;
                                        cap_cosumer_entry->cCapture_dataCosume(
                                            p_buffer_left_right, postSize * 4,
                                            cap_cosumer_entry->priv);
                                        free(p_buffer_left_right);
                                    }
                                }
                            } else if (cap_cosumer_entry->enable &&
                                       cap_cosumer_entry->cCapture_dataCosume &&
                                       !cap_cosumer_entry->pre_process) {
                                if (cap_cosumer_entry->id == AUDIO_ID_ASR_FAR_FIELD &&
                                    need_upload_wake_word) {
#if defined(EMPTY_BUFFER_BEFORE_KEYWORD)
                                    int trigger_len;
                                    int len_needed;
                                    if (g_wiimu_shm->trigger1_len_ms)
                                        trigger_len = g_wiimu_shm->trigger1_len_ms;
                                    else if (g_wiimu_shm->trigger2_len_ms)
                                        trigger_len = g_wiimu_shm->trigger2_len_ms;
                                    else
                                        trigger_len =
                                            MAX_WAKE_WORD_MS - EMPTY_BUFFER_BEFORE_KEYWORD;
                                    len_needed = (trigger_len + EMPTY_BUFFER_BEFORE_KEYWORD) *
                                                 SAMPLE_RATE_FAR * 2 / 1000;
#else
                                    int len_needed = MAX_WAKE_WORD;
#endif
                                    int len = MAX_WAKE_WORD - wake_word_offset;
                                    int len2 = 0;

                                    if (len == 0)
                                        len = MAX_WAKE_WORD;
                                    else
                                        len2 = MAX_WAKE_WORD - len;

                                    if (wake_word_dropped_len > 0 || trigger_delayed_bytes > 0)
                                        wiimu_log(1, 0, 0, 0, "wake_word_pre_buffer_len = %d "
                                                              "trigger_delayed_bytes=%d",
                                                  wake_word_dropped_len, trigger_delayed_bytes);

#if defined(UPLOAD_PRERECORD)
                                    if (g_wiimu_shm->last_trigger_index == ASR_WWE_TRIG2) {
                                        // patch case, upload pre-record word
                                        if (pre_record_word_buf) {
                                            cap_cosumer_entry->cCapture_dataCosume(
                                                pre_record_word_buf, pre_record_word_size,
                                                cap_cosumer_entry->priv);
                                        }

                                        // upload the post-record after Wake-word
                                        if (trigger_delayed_bytes > 0 ||
                                            (wake_word_dropped_len > 0 && wake_word_dropped_buf)) {
                                            // if wake_word_buf was totally overwritten
                                            // upload wake_word_dropped_buf(wake_word_dropped_len -
                                            // MAX_WAKE_WORD) and whole wake_word_buf
                                            if (wake_word_dropped_len > MAX_WAKE_WORD) {
                                                cap_cosumer_entry->cCapture_dataCosume(
                                                    (wake_word_dropped_buf + MAX_WAKE_WORD),
                                                    (wake_word_dropped_len - MAX_WAKE_WORD),
                                                    cap_cosumer_entry->priv);
                                                len_needed = MAX_WAKE_WORD;
                                            }
                                            // only upload the data after Wake-word (
                                            // wake_word_dropped_len )
                                            else {
                                                len_needed =
                                                    wake_word_dropped_len + trigger_delayed_bytes;
                                            }

                                            if (len2 >= len_needed) {
                                                cap_cosumer_entry->cCapture_dataCosume(
                                                    &wake_word_buf[len2 - len_needed], len_needed,
                                                    cap_cosumer_entry->priv);
                                            } else {
                                                cap_cosumer_entry->cCapture_dataCosume(
                                                    &wake_word_buf[wake_word_offset +
                                                                   (len - (len_needed - len2))],
                                                    len_needed - len2, cap_cosumer_entry->priv);
                                                cap_cosumer_entry->cCapture_dataCosume(
                                                    &wake_word_buf[0], len2,
                                                    cap_cosumer_entry->priv);
                                            }
                                        }
                                    } else
                                        trigger_delayed_bytes = 0;
#endif

#if defined(UPLOAD_PRERECORD)
                                    if (g_wiimu_shm->last_trigger_index != ASR_WWE_TRIG2) {
#endif

                                        if (wake_word_dropped_len > 0 && wake_word_dropped_buf)
                                            cap_cosumer_entry->cCapture_dataCosume(
                                                wake_word_dropped_buf, wake_word_dropped_len,
                                                cap_cosumer_entry->priv);

                                        if (len2 >= len_needed) {
                                            cap_cosumer_entry->cCapture_dataCosume(
                                                &wake_word_buf[len2 - len_needed], len_needed,
                                                cap_cosumer_entry->priv);
                                        } else {
                                            cap_cosumer_entry->cCapture_dataCosume(
                                                &wake_word_buf[wake_word_offset +
                                                               (len - (len_needed - len2))],
                                                len_needed - len2, cap_cosumer_entry->priv);
                                            cap_cosumer_entry->cCapture_dataCosume(
                                                &wake_word_buf[0], len2, cap_cosumer_entry->priv);
                                        }
#if defined(UPLOAD_PRERECORD)
                                    }
#endif

                                    asr_waked = 0;
                                    g_wiimu_shm->push_talk_upload_word = 0;
                                } else {
                                    // trigger
                                    if (cap_cosumer_entry->id == AUDIO_ID_WWE) {
#if defined(SOFT_PREP_DSPC_MODULE) || defined(SOFT_PREP_HOBOT_MODULE)
#if defined(ASR_XIAOWEI_MODULE) || defined(ASR_WEIXIN_MODULE)
                                        if ((!g_wiimu_shm->privacy_mode) &&
                                            (!g_wiimu_shm->disable_consume))
#else
                                        if (!g_wiimu_shm->privacy_mode)
#endif
                                            cap_cosumer_entry->cCapture_dataCosume(
                                                downSample_buf_left, postSize,
                                                cap_cosumer_entry->priv);
#elif defined(SOFT_PREP_DSPC_V2_MODULE)
                                        if (!g_wiimu_shm->privacy_mode)
                                            cap_cosumer_entry->cCapture_dataCosume(
                                                downSample_buf_right, postSize,
                                                cap_cosumer_entry->priv);
#else
                                        if (!g_wiimu_shm->privacy_mode) {
                                            if (g_wiimu_shm->asr_ch_trigger ==
                                                OUTPUT_CHANNEL_LEFT) {
                                                cap_cosumer_entry->cCapture_dataCosume(
                                                    downSample_buf_left, postSize,
                                                    cap_cosumer_entry->priv);
                                            } else {
                                                cap_cosumer_entry->cCapture_dataCosume(
                                                    downSample_buf_right, postSize,
                                                    cap_cosumer_entry->priv);
                                            }
                                        }
#endif
#ifdef AVS_ARBITRATION
                                        if (amazon_detectionCallback_waked) {
                                            FILE *fpwake = NULL;
                                            fpwake = fopen(ALEXA_WAKEWORD_FILE, "wb");
                                            if (fpwake) {
                                                // printf("cheryl debug ,post Size = %d, write file
                                                // %s\n",postSize,ALEXA_WAKEWORD_FILE);
                                                int len = MAX_WAKE_WORD - wake_word_offset;
                                                int len2 = 0;
                                                if (len == 0)
                                                    len = MAX_WAKE_WORD;
                                                else
                                                    len2 = MAX_WAKE_WORD - len;
                                                if (len > 0) {
                                                    fwrite(&wake_word_buf[wake_word_offset], 1, len,
                                                           fpwake);
                                                }
                                                if (len2 > 0) {
                                                    fwrite(&wake_word_buf[0], 1, len2, fpwake);
                                                }
                                                fclose(fpwake);
                                            }
                                            printf("AVS_ARBITRATION wakeup callback\n");
                                            SocketClientReadWriteMsg(
                                                "/tmp/RequestASRTTS", (char *)"talkstart:1",
                                                strlen("talkstart:1"), NULL, NULL, 0);
                                            amazon_detectionCallback_waked = 0;
                                        }
#endif
                                    } else if (cap_cosumer_entry->id == AUDIO_ID_WWE_DUAL_CH) {
#if defined(HOBOT_DUAL_CHANNEL_TRIGGER) && defined(SOFT_PREP_HOBOT_MODULE)
                                        if (!g_wiimu_shm->privacy_mode)
                                            cap_cosumer_entry->cCapture_dataCosume(
                                                downSample_buf_right, postSize,
                                                cap_cosumer_entry->priv);
#elif defined(WWE_DOUBLE_TRIGGER)
#if defined(ASR_XIAOWEI_MODULE) || defined(ASR_WEIXIN_MODULE)
                                        if ((!g_wiimu_shm->privacy_mode) &&
                                            (!g_wiimu_shm->disable_consume))
#else
                                        if (!g_wiimu_shm->privacy_mode)
#endif
                                            cap_cosumer_entry->cCapture_dataCosume(
                                                downSample_buf_3rd, post_size_3rd,
                                                cap_cosumer_entry->priv);
#endif
                                    } else if (cap_cosumer_entry->id == AUDIO_ID_BT_HFP) {
                                        if (!g_wiimu_shm->privacy_mode) {
                                            if (g_cap_channel > 2) {
                                                cap_cosumer_entry->cCapture_dataCosume(
                                                    downSample_buf_4th, post_size_4th,
                                                    cap_cosumer_entry->priv);
                                            } else {
                                                if (STR_CMP(voip_channel, "right") == 0) {
                                                    cap_cosumer_entry->cCapture_dataCosume(
                                                        downSample_buf_right, postSize,
                                                        cap_cosumer_entry->priv);
                                                } else {
                                                    cap_cosumer_entry->cCapture_dataCosume(
                                                        downSample_buf_left, postSize,
                                                        cap_cosumer_entry->priv);
                                                }
                                            }
                                        }
                                    } else {
                                        if (g_wiimu_shm->asr_ch_asr == OUTPUT_CHANNEL_LEFT) {
                                            if (g_wiimu_shm->privacy_mode)
                                                memset((void *)downSample_buf_left, 0, postSize);
                                            if (!need_upload_wake_word && asr_waked == 0)
                                                cap_cosumer_entry->cCapture_dataCosume(
                                                    downSample_buf_left, postSize,
                                                    cap_cosumer_entry->priv);
                                        } else {
                                            if (g_wiimu_shm->privacy_mode)
                                                memset((void *)downSample_buf_right, 0, postSize);
                                            cap_cosumer_entry->cCapture_dataCosume(
                                                downSample_buf_right, postSize,
                                                cap_cosumer_entry->priv);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    pthread_mutex_unlock(&mutex_ccap_list);
                } else if (!cap_cache_len) {
                    usleep(20000);
                } else {
                    usleep(1000);
                }
            } else {
                pthread_mutex_lock(&mutex_ccap);
                cap_cache_len = 0;
                cap_cache_read = 0;
                cap_cache_write = 0;
                pthread_mutex_unlock(&mutex_ccap);
                usleep(20000);
            }
        }
    }

    printf("ASR consume thread quit\n");

    return 0;
}
#ifdef DEBUG_FILE
FILE *fpread = NULL;
#endif

#ifdef DEBUG_DUMP_CAP
FILE *fpcap = 0;
static int g_isBegin = 0;
#endif

static void force_recover()
{
    int i = 10;
    // system("echo AXX+STA+000 > /dev/ttyS0");
    system("touch /tmp/cap_recover_flag");
    // SocketClientReadWriteMsg("/tmp/Requesta01controller", "setPlayerCmd:stop",
    // strlen("setPlayerCmd:stop"),NULL,NULL,0);
    // WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 100, SNDSRC_SMPLAYER);
    // WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 1, SNDSRC_SMPLAYER);
    // killPidbyName("smplayer");
    while (i-- > 0) {
        sleep(1);
        printf("******%s wait audio stop i=%d ******\n", __FUNCTION__, i);
        if (access("/tmp/cap_recover_flag", R_OK))
            break;
    }
}

static int input_output_error_detected = 0;
static int input_output_errors = 0;
void *CCaptCaptureThread(void *arg)
{
    int i = 0;
    int res;
    int errs = 0;
    int fd = -1;
    int dir = 0;
    int chunk = 8192;
    int chunk_len = 8192;
    int bufsize = 32768;
    snd_pcm_t *alsa_record_handle = 0;
    char *capture_data = NULL;
    int bytes_per_frame = 0;
    ignore_reset_trigger = 1;

    while (0 != g_wiimu_shm->device_status) {
        i++;
        if (i > 30)
            break;
        sleep(1);
    }

#if defined(HARMAN)
    cxdish_reset();
#endif
    wiimu_log(1, 0, 0, 0, "pid=%d capture device:%s, sample rate:%d has_mptv=%d ver=%d has_cap1=%d "
                          "cap1=0x%.8x at %lld\n",
              getpid(), g_cap_dev, g_cap_rate, g_wiimu_shm->priv_has_mcu_protocol_ver,
              g_wiimu_shm->priv_mcu_protocol_ver, g_wiimu_shm->priv_cap_has_cap1,
              g_wiimu_shm->priv_cap1, tickSincePowerOn());

    printf("[ALSA Open] Audio Capture \n");
    int err = snd_pcm_open(&alsa_record_handle, g_cap_dev, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0 || alsa_record_handle == 0) {
        ++g_wiimu_shm->times_open_failed;
        printf("capture open error: %s", snd_strerror(err));
        goto open_fail;
        // return NULL;
    }
    ignore_reset_trigger = 0;

    if (set_capture_params(alsa_record_handle, g_cap_rate) < 0) {
        printf("====%s %s set_capture_params failed ====\n", __FUNCTION__, __LINE__);
        snd_pcm_close(alsa_record_handle);
        // return NULL;
        goto open_fail;
    }

    bytes_per_frame = (snd_pcm_format_physical_width(SND_PCM_FORMAT_S16_LE) * alsa_cap_ch) / 8;
    printf("capture bytes_per_frame:%d \r\n", bytes_per_frame);

    capture_data = (char *)malloc(8192 * bytes_per_frame);
    g_capture_thread_exist = 1;
    while (CCap_capture_start) {
        // test code for capture reset
        if (1 && access("/tmp/cap_reset_flag", R_OK) == 0) {
            printf("capture reset flag found, do cap reset\n");
            remove("/tmp/cap_reset_flag");
            break;
        }

        if (access("/tmp/cap_recover_flag", R_OK) == 0) {
            printf("cap_recover_flag found\n");
            force_recover();
            break;
        }
        if (g_cap_rate <= USB_CAPTURE_SAMPLERATE)
            chunk_len = 1024;
#ifdef EN_AVS_COMMS
        // changed for capture latency
        chunk_len = 256;
#endif

        err = snd_pcm_readi(alsa_record_handle, capture_data, chunk_len);
        // if mute, do not write the data to cache
        if (g_wiimu_shm && g_wiimu_shm->privacy_mode) {
            if (err > 0)
                memset(capture_data, 0, err * bytes_per_frame);
        }

        if (access("/tmp/eio_flag_set", R_OK) == 0) {
            printf("eio_flag_set found\n");

            fd = remove("/tmp/eio_flag_set");
            if (fd < 0) {
                wiimu_log(1, 0, 0, 0, "Fail to remove file: error(%s)\n", strerror(errno));
            }
            err = -EIO;
        }

        if (err == -EAGAIN || (err > 0 && (size_t)err < chunk_len)) {
#ifndef EN_AVS_COMMS
// printf("capture EAGAIN, err=%d\n", err);
#ifdef CONFIG_SND_I2S_MASTER
            usleep(400000);
#else
            usleep(80000);
#endif
#else
            snd_pcm_wait(alsa_record_handle, 10);
#endif
            errs++;
            if (errs > 100) {
                printf("===%s %d EAGAIN errs=%d at %lld\n", __FUNCTION__, __LINE__, errs,
                       tickSincePowerOn());
                break;
            }
        } else if (err == -EPIPE) {
            snd_pcm_status_t *status;
            snd_pcm_status_alloca(&status);
            if (errs > 1)
                printf("capture EPIPE errs=%d at %lld\n", errs, tickSincePowerOn());
            if ((res = snd_pcm_status(alsa_record_handle, status)) < 0) {
                printf("status error: %s", snd_strerror(res));
                break;
            }
            if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
                if (errs > 1)
                    printf("Capture underrun !!!\n");
                if ((res = snd_pcm_prepare(alsa_record_handle)) < 0) {
                    printf("capture xrun: prepare error: %s", snd_strerror(res));
                    break;
                }
            } else if (snd_pcm_status_get_state(status) == SND_PCM_STATE_DRAINING) {
                if ((res = snd_pcm_prepare(alsa_record_handle)) < 0) {
                    printf("capture xrun(DRAINING): prepare error: %s", snd_strerror(res));
                    break;
                }
            }

            errs++;
            if (errs > 100) {
                printf("===%s %d EPIPE errs=%d at %lld\n", __FUNCTION__, __LINE__, errs,
                       tickSincePowerOn());
                break;
            }
        } else if (err == -ESTRPIPE) {
            printf("capture ESTRPIPE errs=%d\n", errs);
            while ((res = snd_pcm_resume(alsa_record_handle)) == -EAGAIN)
                sleep(1); /* wait until suspend flag is released */
            if (res < 0) {
                if ((res = snd_pcm_prepare(alsa_record_handle)) < 0) {
                    printf("capture suspend: prepare error: %s", snd_strerror(res));
                    break;
                }
            }
            errs++;
            if (errs > 100) {
                printf("===%s %d ESTRPIPE errs=%d at %lld\n", __FUNCTION__, __LINE__, errs,
                       tickSincePowerOn());
                break;
            }
        } else if (err < 0) {
            ++g_wiimu_shm->times_capture_failed;
            printf("capture error: %s errors=%d input_output_error_detected=%d at %lld\n",
                   snd_strerror(err), ++input_output_errors, input_output_error_detected,
                   tickSincePowerOn());
            if (input_output_error_detected++) {
                printf("\n\n!!!!!Capture recover failed at %lld, should restart rootApp???\n\n",
                       tickSincePowerOn());
                force_recover();
            }
            break;
        }

#ifdef DEBUG_FILE
        if (fpread) {
            if (!feof(fpread)) {
                res = fread(capture_data, 1, 8192 * bytes_per_frame, fpread);
                if (res < 8192 * bytes_per_frame) {
                    memset(capture_data + res, 0, 8192 * bytes_per_frame - res);
                }

            } else {
                memset(capture_data, 0, 8192 * bytes_per_frame);
                fseek(fpread, 0, SEEK_SET);
            }
        }
#endif

        if (err > 0) { // && cap_cache_flush==0)
            errs = 0;
            char *pdata = capture_data;
            if (input_output_error_detected) {
                input_output_error_detected = 0;
                // system("smplayer /system/workdir/misc/Voice-prompt/common/alexa_alarm.mp3");
            }
#ifdef DEBUG_DUMP_CAP
            if (!g_isBegin) {
                printf("capture first read return: %d at %d\n", err, (int)tickSincePowerOn());
                g_isBegin = 1;
            }
            fwrite(pdata, 1, err * bytes_per_frame, fpcap);
#endif

            if (Cosume_Playback.enable == 1) {
                pthread_mutex_lock(&mutex_ccap);
                if (CAPTURE_SHARE_BUF_SIZE != 65536 * 8) {
                    CAPTURE_SHARE_BUF_SIZE = 65536 * 8;
                    free(cap_cache_buf);
                    cap_cache_buf = malloc(CAPTURE_SHARE_BUF_SIZE);
                    cap_cache_len = 0;
                    cap_cache_write = 0;
                    cap_cache_read = 0;
                }
                pthread_mutex_unlock(&mutex_ccap);

                if (g_capture_filled) {
                    usleep(100000);
                    continue;
                }
            } else {
                pthread_mutex_lock(&mutex_ccap);
                if (CAPTURE_SHARE_BUF_SIZE != CAPTURE_BUF_LENGTH) {
                    CAPTURE_SHARE_BUF_SIZE = CAPTURE_BUF_LENGTH;
                    free(cap_cache_buf);
                    cap_cache_buf = malloc(CAPTURE_SHARE_BUF_SIZE);
                    g_capture_filled = 0;
                    cap_cache_len = 0;
                    cap_cache_write = 0;
                    cap_cache_read = 0;
                }
                pthread_mutex_unlock(&mutex_ccap);
            }

            do {
                int write_len = 0;
                pthread_mutex_lock(&mutex_ccap);
                write_len = CAPTURE_SHARE_BUF_SIZE - cap_cache_len;
                pthread_mutex_unlock(&mutex_ccap);

                // if(cap_cache_len >  CAPTURE_SHARE_BUF_SIZE/2)
                //  printf("cap_cache_len %d\n");
                if (write_len) {
                    pthread_mutex_lock(&mutex_ccap);
                    write_len =
                        write_len < (err * bytes_per_frame) ? write_len : (err * bytes_per_frame);
                    write_len = (CAPTURE_SHARE_BUF_SIZE - cap_cache_write) > write_len
                                    ? write_len
                                    : (CAPTURE_SHARE_BUF_SIZE - cap_cache_write);
                    memcpy(cap_cache_buf + cap_cache_write, pdata, write_len);
                    cap_cache_write += write_len;
                    cap_cache_write %= CAPTURE_SHARE_BUF_SIZE;
                    cap_cache_len += write_len;
                    pthread_mutex_unlock(&mutex_ccap);

                    pdata += write_len;
                    err -= write_len / bytes_per_frame;
                } else {
                    printf("Dropped %d capture buffer, cap_cache_len = %d \n",
                           err * bytes_per_frame, cap_cache_len);
                    usleep(1000);
                    break;
                }
#ifndef EN_AVS_COMMS
                usleep(10000);
#endif
            } while (err > 0);
        }

        pthread_mutex_lock(&mutex_ccap);
        if (Cosume_Playback.enable == 1 && cap_cache_len >= CAPTURE_SHARE_BUF_SIZE - 65536) {
            g_capture_filled = 1;
        }
        pthread_mutex_unlock(&mutex_ccap);
        /*
        pthread_mutex_lock(&mutex_ccap);
        if(cap_cache_flush)
        {
            cap_cache_len = 0;
            cap_cache_write = 0;
            cap_cache_read = 0;
            cap_cache_flush = 0;
            printf("flush the capture buffer\n");
        }
        pthread_mutex_unlock(&mutex_ccap);*/
    }
    printf("====%s %d befreo snd_pcm_close at %lld====\n", __FUNCTION__, __LINE__,
           tickSincePowerOn());
    snd_pcm_close(alsa_record_handle);
open_fail:
    if (capture_data)
        free(capture_data);
    printf("CCaptCaptureThread-- capture thread quit err=%d at %lld\n", err, tickSincePowerOn());
    if (CCap_capture_start)
        SocketClientReadWriteMsg("/tmp/RequestASRTTS", "CAP_RESET", strlen("CAP_RESET"), NULL, NULL,
                                 0);
    g_capture_thread_exist = 0;
    CCap_capture_start = 0;

    // pthread_detach(pthread_self());
    // pthread_exit(0);
    return 0;
}

void CCaptRegister(CAPTURE_COMUSER *input)
{
    pthread_mutex_lock(&mutex_ccap_list);
    lp_list_add_tail(&input->list, &list_capture_cosumer);
// input->enable = 1;
#ifdef DEBUG_FILE
    fpread = fopen("/tmp/wfc.pcm", "r");
    fseek(fpread, 0, SEEK_SET);
#endif
    pthread_mutex_unlock(&mutex_ccap_list);
}

void CCaptUnregister(CAPTURE_COMUSER *input)
{
    // int running;
    pthread_mutex_lock(&mutex_ccap_list);
    lp_list_del(&input->list);
    // running = input->enable;
    // input->enable = 0;
    pthread_mutex_unlock(&mutex_ccap_list);
// if(CCap_capture_start && input->cCapture_deinit)
//{
//    input->cCapture_deinit(NULL);
//}
#ifdef DEBUG_FILE
    if (fpread) {
        fclose(fpread);
        fpread = NULL;
    }
#endif
}

void CCaptFlush(void)
{
    pthread_mutex_lock(&mutex_ccap);
    cap_cache_len = 0;
    cap_cache_write = 0;
    cap_cache_read = 0;
    pthread_mutex_unlock(&mutex_ccap);
}

int CCaptWakeUpFlushState(void)
{
    int ret = 1;
#if defined(SOFT_PREP_MODULE)
    if (g_soft_pre_process && g_soft_pre_process->cSoft_PreP_flushState) {
        ret = g_soft_pre_process->cSoft_PreP_flushState();
    }
#endif
    return ret;
}

#if defined(SOFT_PREP_MODULE)

int CCapt_Soft_prep_wakeup(int number)
{
    printf("wakeup callback ++\n");
    SocketClientReadWriteMsg("/tmp/RequestASRTTS", "talkstart:1", strlen("talkstart:1"), NULL, NULL,
                             0);
#ifdef BAIDU_DUMI
    asr_waked = 0;
#else
    asr_waked = 1;
#endif
    printf("wakeup callback --\n");
    return 0;
}

int CCapt_soft_prep_fill(void *opaque, char *buffer, int dataLen)
{
    int err = dataLen;
    // printf("%s ++ %x %d\n", __func__, buffer, dataLen);
    WM_CAL_START(CCapt_soft_prep_fill, 2);

    if (err > 0) { // && cap_cache_flush==0)
        char *pdata = buffer;
        if (input_output_error_detected) {
            input_output_error_detected = 0;
            // system("smplayer /system/workdir/misc/Voice-prompt/common/alexa_alarm.mp3");
        }
#ifdef DEBUG_DUMP_CAP
        if (!g_isBegin) {
            printf("capture first read return: %d at %d\n", err, (int)tickSincePowerOn());
            g_isBegin = 1;
        }
        fwrite(pdata, 1, err, fpcap);
#endif

        do {
            int write_len = 0;
            pthread_mutex_lock(&mutex_ccap);
            write_len = CAPTURE_SHARE_BUF_SIZE - cap_cache_len;
            pthread_mutex_unlock(&mutex_ccap);

            if (write_len) {
                pthread_mutex_lock(&mutex_ccap);
                write_len = write_len < err ? write_len : err;
                write_len = (CAPTURE_SHARE_BUF_SIZE - cap_cache_write) > write_len
                                ? write_len
                                : (CAPTURE_SHARE_BUF_SIZE - cap_cache_write);
                memcpy(cap_cache_buf + cap_cache_write, pdata, write_len);
                cap_cache_write += write_len;
                cap_cache_write %= CAPTURE_SHARE_BUF_SIZE;
                cap_cache_len += write_len;
                pthread_mutex_unlock(&mutex_ccap);

                pdata += write_len;
                err -= write_len;
            } else {
                printf("Dropped %d capture buffer, cap_cache_len = %d \n", err, cap_cache_len);
                usleep(1000);
                break;
            }

            if (err > 0) {
                usleep(1000);
            }
        } while (err > 0);
    }

    pthread_mutex_lock(&mutex_ccap);
    if (Cosume_Playback.enable == 1 && cap_cache_len >= CAPTURE_SHARE_BUF_SIZE - 65536) {
        g_capture_filled = 1;
    }
    pthread_mutex_unlock(&mutex_ccap);

    WM_CAL_END(CCapt_soft_prep_fill, 2);
    return dataLen;
}
#endif

void CCaptStart(void)
{
    int ret = 0;
    int inited = 0;

    pthread_mutex_lock(&mutex_ccap);

#ifdef DEBUG_DUMP_CAP
    system("mkdir -p /tmp/opt/temp/cap");
    system("rm -rf /tmp/opt/temp/cap/cap.pcm");
    g_isBegin = 0;
    fpcap = fopen("/tmp/opt/temp/cap/cap.pcm", "w+");
#endif

    inited = CCap_capture_start;
    CCap_capture_start = 1;
    pthread_mutex_unlock(&mutex_ccap);

    CCaptFlush();

#if defined(UPLOAD_PRERECORD)
    pre_record_word_load("/system/workdir/misc/pre_record_word.pcm");
#endif

    // config usb recorder by mcu cap1, and mcu protocol version must > 001
    memset(g_cap_dev, 0x0, sizeof(g_cap_dev));
    strncpy(g_cap_dev, "default", sizeof(g_cap_dev) - 1);

    if ((g_wiimu_shm->priv_has_mcu_protocol_ver && (g_wiimu_shm->priv_mcu_protocol_ver > 1)) &&
        g_wiimu_shm->priv_cap_has_cap1) {
        if (g_wiimu_shm->priv_cap1 & (1 << CAP1_USB_RECORDER)) {
            g_cap_rate = USB_CAPTURE_SAMPLERATE;
            strncpy(g_cap_dev, USB_CAPTURE_DEVICE, sizeof(g_cap_dev) - 1);
        }
    }

#if defined(OPENWRT_AVS)
    g_cap_rate = 44100;
    strncpy(g_cap_dev, "hw:1,0", sizeof(g_cap_dev) - 1);
#else
#ifdef USB_CAPDSP_MODULE
    g_cap_rate = USB_CAPTURE_SAMPLERATE;
    strncpy(g_cap_dev, USB_CAPTURE_DEVICE, sizeof(g_cap_dev) - 1);
#else
    g_cap_rate = AMLOGIC_I2S_CAPTURE_SAMPLERATE;
    strncpy(g_cap_dev, AMLOGIC_I2S_CAPTURE_DEVICE, sizeof(g_cap_dev) - 1);
#endif
#endif

#if defined(SOFT_PREP_MODULE)
    if (g_soft_pre_process && g_soft_pre_process->cSoft_PreP_getAudioParam) {
        WAVE_PARAM audio_param = g_soft_pre_process->cSoft_PreP_getAudioParam();
        g_cap_rate = audio_param.SampleRate;
        g_bit_per_sample = audio_param.BitsPerSample;
        g_cap_channel = audio_param.NumChannels;
        printf("---------- soft pre processer will output samplerate=%d, bits=%d, Channel=%d "
               "------------",
               g_cap_rate, g_bit_per_sample, g_cap_channel);
    } else {
        printf("---------- soft pre processer has no audio parameter? ------------");
    }
#endif

    if (!inited) {
        CCapt_resample_init(&resample_info[0], g_cap_rate, g_cap_resample_rate,
                            &resample_info[0].g_lin_pol);
        CCapt_resample_init(&resample_info[1], g_cap_rate, g_cap_resample_rate,
                            &resample_info[1].g_lin_pol);
        CCapt_resample_init(&resample_info_ext[0], g_cap_rate, g_cap_resample_rate,
                            &resample_info_ext[0].g_lin_pol);
        CCapt_resample_init(&resample_info_ext[1], g_cap_rate, g_cap_resample_rate,
                            &resample_info_ext[1].g_lin_pol);

        cap_cache_buf = malloc(CAPTURE_SHARE_BUF_SIZE);
        g_temp_buffer = malloc(CAPTURE_SHARE_BUF_SIZE);
        if (!cap_cache_buf) {
            printf("alloc capture share buffer error!!!\n");
            return;
        }

#if defined(SOFT_PREP_MODULE)
        if (g_soft_pre_process && g_soft_pre_process->cSoft_PreP_setWakeupCallback) {
            g_soft_pre_process->cSoft_PreP_setWakeupCallback(CCapt_Soft_prep_wakeup, NULL);
        }
        if (g_soft_pre_process && g_soft_pre_process->cSoft_PreP_setObserverStateCallback) {
            g_soft_pre_process->cSoft_PreP_setObserverStateCallback(
                GetAudioInputProcessorObserverState, NULL);
        }

        if (g_soft_pre_process && g_soft_pre_process->cSoft_PreP_setFillCallback &&
            g_soft_pre_process->method == SOFT_PREP_METHOD_CALLBACK) {
            g_soft_pre_process->cSoft_PreP_setFillCallback(CCapt_soft_prep_fill, NULL);
            g_capture_thread_exist = 1;
        } else
#endif
            pthread_create(&CCap_capture_thread, NULL, CCaptCaptureThread, NULL);

        // usleep(10000);
        pthread_create(&CCap_cosume_thread, NULL, CCaptCosumeThread, NULL);
    }

    // CCaptRegister(&Cosume_File);
}

void CCaptStop(void)
{
    pthread_mutex_lock(&mutex_ccap);
    g_capture_thread_exist = 0;
    CCap_capture_start = 0;
    pthread_mutex_unlock(&mutex_ccap);

    if (CCap_cosume_thread > 0) {
        pthread_join(CCap_cosume_thread, NULL);
        CCap_cosume_thread = 0;
    }
    if (CCap_capture_thread > 0) {
        pthread_join(CCap_capture_thread, NULL);
        CCap_capture_thread = 0;
    }

    if (cap_cache_buf)
        free(cap_cache_buf);
    cap_cache_buf = 0;

    if (g_temp_buffer)
        free(g_temp_buffer);
    g_temp_buffer = 0;

#ifdef DEBUG_DUMP_CAP
    if (fpcap)
        fclose(fpcap);
#endif
#if defined(SOFT_PREP_MODULE)
    if (g_soft_pre_process && g_soft_pre_process->cSoft_PreP_setFillCallback &&
        g_soft_pre_process->method == SOFT_PREP_METHOD_CALLBACK) {
        g_soft_pre_process->cSoft_PreP_setFillCallback(NULL, NULL);
    }
#endif

    if (work_buf) {
        free(work_buf);
        work_buf = NULL;
    }
    if (downSample_buf_left) {
        free(downSample_buf_left);
        downSample_buf_left = NULL;
    }
    if (downSample_buf_right) {
        free(downSample_buf_right);
        downSample_buf_right = NULL;
    }
    if (downSample_buf_comms) {
        free((downSample_buf_comms));
        (downSample_buf_comms) = NULL;
    }
#if defined(UPLOAD_PRERECORD)
    if (pre_record_word_buf) {
        free(pre_record_word_buf);
        pre_record_word_buf = NULL;
    }
#endif
}

int IsCCaptStart(void) { return (CCap_capture_start & g_capture_thread_exist); }
