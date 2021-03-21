//
// Created by cuixuecheng on 2017/11/27.
//
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <alsa/asoundlib.h>
#include "circ_buff.h"
#include <chrono>
#include "hobotdefine.h"
#include "hobotSpeechAPI.h"
#include "pthread.h"
#include <mutex>
#include <future>
#include "soft_prep_hobot_func.h"
#include "wm_api.h"

#include "wm_util.h"
extern WIIMU_CONTEXT *g_wiimu_shm;

#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>

const int DEAL_IN_LEN = 480;
const int DEAL_OUT_LEN = 160;

// channel num
const int CHANNELS = 8;
const int PER_BYTES = 2;
// const int CHUNK_SIZE = 8190;
const int CHUNK_SIZE = 1024;
const int SAMPLE_RATE = 48000;
const int ALLOCATE_ARR_SIZE = CHUNK_SIZE * CHANNELS * PER_BYTES; // bytes
const int MONO_ARR_SIZE = ALLOCATE_ARR_SIZE / 8;

extern "C" {
#include "libresample.h"
#include "resample_defs.h"
#include <time.h>
#include "lpcfg.h"

extern int Soft_prep_hobot_getID(char *ID, size_t len1, char *token, size_t len2);
extern int Soft_prep_hobot_setToken(char *token);

static char authcode[1024] = {0};
static char hobot_id[1024] = {0};
static char hisf_cfg[256] = {0};
static int auth_print_once = 0;

// std::thread test_thread;
std::thread record_thread;
std::thread leftResample_thread;
std::thread rightResample_thread;
#if defined(HOBOT_LEGACY_STEREO_REF)
std::thread refResample_thread[2]; // for stereo channels
void refResample(int index);
#else
std::thread refResample_thread;
void refResample();
#endif

// void * do_pcm_test(void * priv) ;
void recordTest();
void leftResample();
void rightResample();

FILE *record_file;
FILE *mid_file;
FILE *dule_file;

// 44.1k source
FILE *src_file = NULL;

// processed
FILE *ch5_srcFile = NULL;
static FILE *ch2_outFile = NULL;

#define RECORD_FILE "/tmp/web/record_file.pcm"
#define MID_FILE "/tmp/web/mid_file.pcm"
#define DULE_FILE "/tmp/web/dule_file.pcm"
#define SRC_FILE "/tmp/web/src_file.pcm"

#define PCM_441K_2CH "PCM_48K_3CH"
#define PCM_16K_5CH_INPUT "PCM_16K_3CH_INPUT"
#define PCM_16K_2CH_OUTPUT "PCM_16K_2CH_OUTPUT"

static int record_status = 0;

std::mutex _mtx;

std::string _save_audio_path = "";
bool stopRecord = false;

HobotSpeechHandler _wake_handle;
HobotSpeechHandler _asr_handle;

pthread_t _cancel_tid;

int _wakeup_count = 0;

// origin audio data
tCircularBuffer *_left_circ_buffer;
tCircularBuffer *_right_circ_buffer;
tCircularBuffer *_left_process_circ_buffer;
tCircularBuffer *_right_process_circ_buffer;
#if defined(HOBOT_LEGACY_STEREO_REF)
tCircularBuffer *_ref_left_circ_buffer;
tCircularBuffer *_ref_right_circ_buffer;
tCircularBuffer *_ref_left_process_circ_buffer;
tCircularBuffer *_ref_right_process_circ_buffer;
#define HOBOT_CHANNEL_NUM 5
#else
tCircularBuffer *_ref_circ_buffer;
tCircularBuffer *_ref_process_circ_buffer;
#define HOBOT_CHANNEL_NUM 5
#endif

static Soft_prep_hobot_wrapper *g_handle = NULL;

bool mic_run_flag;

int enableHobotMicrophoneData(int value)
{
    printf("enableStreamingMicrophoneData\n");
    mic_run_flag = value;
    return 0;
}

void cancel()
{

    // cancel wake engine
    std::lock_guard<std::mutex> _lock(_mtx);

    std::cout << "*************************************"
              << "wake up count " << ++_wakeup_count << "*************************************"
              << std::endl;

    int ret = CancelEngine(_wake_handle);

    while (true) {

        if (ret != 0) {
            ret = CancelEngine(_wake_handle);
        }

        break;
    }

    StartEngine(_wake_handle);
    return;
}

//语音状态回调
void hobot_event_callback(HOBOT_REC_EVENT_TYPE event_type)
{
    switch (event_type) {
    case VOICE_START:
        // detect voice start point
        break;
    case VOICE_END:
        // detect voice end point
        break;
    default:
        break;
    }
}

void hobot_result_callback(const char *result, bool islast)
{
    // wake up success
    // if wake up success,then cancel engine and start engine on asr mode
    std::cout << "*************************************" << result
              << "*************************************" << std::endl;
    std::thread cancel_thread(cancel);
    cancel_thread.detach();
}

// origin audio data callback
int32_t hobot_audio_callback(char *audio_data, int32_t buff_len)
{
#if defined(HOBOT_LEGACY_STEREO_REF)
    int32_t datalen = buff_len / 5; // this should match IN_WAVE # in .ini file.
    char *ref_left_buffer = new char[datalen];
    char *ref_right_buffer = new char[datalen];
    short *realRefLeftBuffer = nullptr;
    short *realRefRightBuffer = nullptr;
#else
    int32_t datalen = buff_len / 5;
    char *refBuffer = new char[datalen];
    short *realRefBuffer = nullptr;
#endif

    char *leftBuffer = new char[datalen];
    char *rightBuffer = new char[datalen];
    short *realRightBuffer = nullptr;
    short *realLeftBuffer = nullptr;

    short *realBuffer = new short[buff_len / 2];
    short *recordBuffer = new short[buff_len / 2];
    // 2channel data get
    short *mic2chBuffer = new short[buff_len / 2];

    int32_t readlen = 0;
    while (true) {
        if (BufferSizeFilled(_left_process_circ_buffer, 0) >= 512 &&
            BufferSizeFilled(_right_process_circ_buffer, 0) >= 512 &&
#if defined(HOBOT_LEGACY_STEREO_REF)
            BufferSizeFilled(_ref_left_process_circ_buffer, 0) >= 512 &&
            BufferSizeFilled(_ref_right_process_circ_buffer, 0) >= 512) {
#else
            BufferSizeFilled(_ref_process_circ_buffer, 0) >= 512) {
#endif
            /* Has enough data. Now start to process */
            ReadBuffer(_left_process_circ_buffer, leftBuffer, 512);
            ReadBuffer(_right_process_circ_buffer, rightBuffer, 512);
#if defined(HOBOT_LEGACY_STEREO_REF)
            ReadBuffer(_ref_left_process_circ_buffer, ref_left_buffer, 512);
            ReadBuffer(_ref_right_process_circ_buffer, ref_right_buffer, 512);
            realRefLeftBuffer = reinterpret_cast<short *>(ref_left_buffer);
            realRefRightBuffer = reinterpret_cast<short *>(ref_right_buffer);

#else
            ReadBuffer(_ref_process_circ_buffer, refBuffer, 512);
            realRefBuffer = reinterpret_cast<short *>(refBuffer);
#endif
            realLeftBuffer = reinterpret_cast<short *>(leftBuffer);
            realRightBuffer = reinterpret_cast<short *>(rightBuffer);

            for (int i = 0; i < 256; i++) {
/* callback data buffer's layout should match hobot_config.ini
 * MIC_ORDER = {0, 1}
 * AEC_REF_CHANS = {...}
 * IN_WAVE_CHANS = 4 # mic# + ref#
 */
#if defined(HOBOT_LEGACY_STEREO_REF)
                /* Current layout:
                 * MIC_ORDER = {0, 1}
                 * AEC_REF_CHANS = {3, 4}
                 * IN_WAVE_CHANS = 5 # 2MIC + STEREO REF
                 */
                realBuffer[i * 5 + 0] = realLeftBuffer[i];
                realBuffer[i * 5 + 1] = realRightBuffer[i];
                realBuffer[i * 5 + 2] = realRightBuffer[i];
                realBuffer[i * 5 + 3] = realRefLeftBuffer[i];
                realBuffer[i * 5 + 4] = realRefRightBuffer[i];

                recordBuffer[i * 4 + 0] = realLeftBuffer[i];
                recordBuffer[i * 4 + 1] = realRightBuffer[i];
                recordBuffer[i * 4 + 2] = realRefLeftBuffer[i];
                recordBuffer[i * 4 + 3] = realRefRightBuffer[i];

#else
                /* very old layout format */
                realBuffer[i * 5 + 0] = realLeftBuffer[i];
                realBuffer[i * 5 + 1] = 0;
                realBuffer[i * 5 + 2] = 0;
                realBuffer[i * 5 + 3] = realRightBuffer[i];
                realBuffer[i * 5 + 4] = realRefBuffer[i];

                recordBuffer[i * 3 + 0] = realLeftBuffer[i];
                recordBuffer[i * 3 + 1] = realRightBuffer[i];
                recordBuffer[i * 3 + 2] = realRefBuffer[i];
#endif

                mic2chBuffer[i * 2 + 0] = realLeftBuffer[i];
                mic2chBuffer[i * 2 + 1] = realRightBuffer[i];
            }
            break;
        } else {
            usleep(20000);
            // printf("hobot_audio_callback  %d %d\n", BufferSizeFilled(_left_process_circ_buffer,
            // 0), buff_len);
            continue;
        }
    }
    memcpy(audio_data, realBuffer, buff_len);

    if (record_status) { // begine to record
        if (ch5_srcFile) {
            // fwrite(realBuffer, 1, buff_len , ch5_srcFile);
            fwrite(recordBuffer, 2, 512 * 3 / 2, ch5_srcFile);
            fflush(ch5_srcFile);
        }
    }

#if defined(ENABLE_RECORD_SERVER)
    if (g_handle && g_handle->record_callback && mic_run_flag) {
#if defined(HOBOT_LEGACY_STEREO_REF)
        g_handle->record_callback((char *)recordBuffer, 512 * 4, g_handle->recPriv,
                                  1); // STREAM_AFTER_RESAMPLE_INDEX
#else
        g_handle->record_callback((char *)recordBuffer, 512 * 3, g_handle->recPriv,
                                  1); // STREAM_AFTER_RESAMPLE_INDEX
#endif
    }
#endif

    if (g_handle && g_handle->micrawCallback && mic_run_flag) {
        g_handle->micrawCallback(g_handle->micrawPriv, (char *)mic2chBuffer, 512 * 2);
    }

    delete[] leftBuffer;
    delete[] rightBuffer;
#if defined(HOBOT_LEGACY_STEREO_REF)
    delete[] ref_left_buffer;
    delete[] ref_right_buffer;
#else
    delete[] refBuffer;
#endif

    delete[] realBuffer;
    delete[] recordBuffer;
    delete[] mic2chBuffer;
    return buff_len;
}

// 16k 16bit 1ch to 16k 16bit 2ch
int convert_to_2ch(short *src, short *des, int len)
{
    int i = 0;
    while (i < len) {
        *des = *src;
        *(des + 1) = *src;
        src += 1;
        des += 2;
        i++;
    }
    return 0;
}

int mid_len = 0;
int dule_len = 0;

// output audio data callback
// if wake up success, please input the audio data to amazon server for asr recognition
void hobot_enhance_audio_callback(char *audio_data, int32_t buff_len)
{

#if 1

// char des_buf[32768] = {0};
#if 0

        //########file record
        if(mid_len < 16000 * 2 * 5) { //5s record
            fwrite(audio_data, 1, buff_len, mid_file);
            mid_len += buff_len;
            fflush(mid_file);
        } else {
            if(mid_file) {
                fflush(mid_file);
                fclose(mid_file);
                mid_file = NULL;
            }
        }
#endif
    // printf("buff_len = %d######\n",buff_len);
    // convert 16k 16bit 1ch to 16k 16bit 2ch
    // convert_to_2ch((short *)audio_data,(short *)des_buf,buff_len/2);

    if (record_status) { // begine to record
        if (ch2_outFile) {
            fwrite(audio_data, 1, buff_len, ch2_outFile);
        }
        fflush(ch2_outFile);
    }

    if (g_handle && g_handle->callback && mic_run_flag) {
        g_handle->callback(g_handle->fillPriv, audio_data, buff_len);

#if 0
            //########file record
            if(dule_len < 16000 * 2 * 2 * 5) { //5s record
                fwrite(audio_data, 1, buff_len, dule_file);
                dule_len += (buff_len);
                fflush(dule_file);
            } else {
                if(dule_file) {
                    fflush(dule_file);
                    fclose(dule_file);
                    dule_file = NULL;
                }
            }
#endif
    }
#endif
}

// authentication
int hobot_auth_cb(int code)
{
    if (code == 1 && auth_print_once == 0) {
        std::cout << "hobot auth is " << code << std::endl;
        // std::cout << "return tokon:" << authcode << std::endl;
        Soft_prep_hobot_setToken(authcode);
        auth_print_once = 1;
    }
    return 0;
}

static snd_pcm_hw_params_t *record_hw_params = 0;
static snd_pcm_sw_params_t *record_sw_params = 0;

static int set_capture_params(snd_pcm_t *handle)
{
    int err;
    int sampling_rate = SAMPLE_RATE;
    snd_pcm_uframes_t chunk_size = CHUNK_SIZE;
    unsigned int buffer_time = 0;
    unsigned int period_time = 0;
    snd_pcm_uframes_t buffer_size;

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
    err = snd_pcm_hw_params_set_channels(handle, record_hw_params, CHANNELS);
    if (err < 0) {
        printf("Channels count non available\n");
        return -1;
    }
    err = snd_pcm_hw_params_set_rate_near(handle, record_hw_params, (unsigned int *)&sampling_rate,
                                          0);
    if (err < 0) {
        printf("sampling_rate set faile %d\n", sampling_rate);
    }

    err = snd_pcm_hw_params_get_buffer_time_max(record_hw_params, &buffer_time, 0);
    printf("buffer_time %d\n", buffer_time);

    if (buffer_time > 500000)
        buffer_time = 500000;
    period_time = buffer_time / 4;

    err = snd_pcm_hw_params_set_buffer_time_near(handle, record_hw_params, &buffer_time, 0);
    if (err < 0) {
        printf("buffer_time set faile %d\n", buffer_time);
    }
    err = snd_pcm_hw_params_set_period_time_near(handle, record_hw_params, &period_time, 0);
    if (err < 0) {
        printf("period_time set faile %d\n", period_time);
    }

    err = snd_pcm_hw_params(handle, record_hw_params);

    snd_pcm_hw_params_get_period_size(record_hw_params, &chunk_size, 0);
    snd_pcm_hw_params_get_buffer_size(record_hw_params, &buffer_size);
    if (chunk_size == buffer_size) {
        printf("Can't use period equal to buffer size (%lu == %lu)\n", chunk_size, buffer_size);
    }

    err = snd_pcm_nonblock(handle, 1);
    if (err < 0) {
        printf("capture nonblock setting error: %s\n", snd_strerror(err));
        return -1;
    }

    return 0;
}

#if 0
    static int set_capture_params(snd_pcm_t *handle)
    {
        int err;
        int sampling_rate = 44100;
        unsigned int buffer_time = 0;
        unsigned int period_time = 0;
        snd_pcm_uframes_t chunk_size = 8192;
        snd_pcm_uframes_t buffer_size;

        snd_pcm_hw_params_alloca(&record_hw_params);
        snd_pcm_sw_params_alloca(&record_sw_params);
        err = snd_pcm_hw_params_any(handle, record_hw_params);
        if(err < 0) {
            printf("Broken configuration for this PCM: no configurations available\n");
        }
        err = snd_pcm_hw_params_set_access(handle, record_hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
        if(err < 0) {
            printf("Access type not available\n");
            return -1;
        }
        err = snd_pcm_hw_params_set_format(handle, record_hw_params, SND_PCM_FORMAT_S16_LE);
        if(err < 0) {
            printf("Sample format non available\n");
            return -1;
        }
        err = snd_pcm_hw_params_set_channels(handle, record_hw_params, 2);
        if(err < 0) {
            printf("Channels count non available\n");
            return -1;
        }
        err = snd_pcm_hw_params_set_rate_near(handle, record_hw_params, (unsigned int *) &sampling_rate, 0);
        if(err < 0) {
            printf("sampling_rate set faile %d\n", sampling_rate);
        }

        err = snd_pcm_hw_params_get_buffer_time_max(record_hw_params, &buffer_time, 0);
        printf("buffer_time %d\n", buffer_time);

        if(buffer_time > 500000) buffer_time = 500000;
        period_time = buffer_time / 4;

        err = snd_pcm_hw_params_set_buffer_time_near(handle, record_hw_params, &buffer_time, 0);
        if(err < 0) {
            printf("buffer_time set faile %d\n", buffer_time);
        }
        err = snd_pcm_hw_params_set_period_time_near(handle, record_hw_params, &period_time, 0);
        if(err < 0) {
            printf("period_time set faile %d\n", period_time);
        }

        err = snd_pcm_hw_params(handle, record_hw_params);

        snd_pcm_hw_params_get_period_size(record_hw_params, &chunk_size, 0);
        snd_pcm_hw_params_get_buffer_size(record_hw_params, &buffer_size);
        if(chunk_size == buffer_size) {
            printf("Can't use period equal to buffer size (%lu == %lu)\n", chunk_size, buffer_size);
        }

        err = snd_pcm_nonblock(handle, 1);
        if(err < 0) {
            printf("capture nonblock setting error: %s\n", snd_strerror(err));
            return -1;
        }
        return 0;
    }

#endif

void recordTest()
{
    char capdev[32] = {0};
    if (g_handle && g_handle->cap_dev == SOFT_PREP_CAP_TDMC) {
#if defined(ALSA_DSNOOP_ENABLE)
        snprintf(capdev, sizeof(capdev), "%s", "input_tdm");
#else  // defined(ALSA_DSNOOP_ENABLE)
        snprintf(capdev, sizeof(capdev), "%s", "hw:0,1");
#endif // defined(ALSA_DSNOOP_ENABLE)
    } else {
#if defined(ALSA_DSNOOP_ENABLE)
        snprintf(capdev, sizeof(capdev), "%s", "input_pdm");
#else  // defined(ALSA_DSNOOP_ENABLE)
        snprintf(capdev, sizeof(capdev), "%s", "hw:0,2");
#endif // defined(ALSA_DSNOOP_ENABLE)
    }

    StartEngine(_wake_handle);

    snd_pcm_t *alsa_record_handle = 0;

#if defined(A98_FRIENDSMINI_ALEXA)
    while (g_wiimu_shm->loopback_flag != 1)
        usleep(5000);
#endif

    int err = snd_pcm_open(&alsa_record_handle, capdev, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0 || alsa_record_handle == 0) {
        printf("capture open error: %s", snd_strerror(err));
        return;
    }

    if (set_capture_params(alsa_record_handle) < 0) {
        printf("set_capture_params error !!!!\n");
        snd_pcm_close(alsa_record_handle);
        return;
    }

    char *capture_data = (char *)malloc(ALLOCATE_ARR_SIZE);

    short *origin_data = nullptr;
    short *leftBuffer = new short[MONO_ARR_SIZE / 2];
    memset(leftBuffer, 0, MONO_ARR_SIZE / 2 * sizeof(short));

    short *rightBuffer = new short[MONO_ARR_SIZE / 2];
    memset(rightBuffer, 0, MONO_ARR_SIZE / 2 * sizeof(short));

#if defined(HOBOT_LEGACY_STEREO_REF)
    short *refLeftBuffer = new short[MONO_ARR_SIZE / 2];
    short *refRightBuffer = new short[MONO_ARR_SIZE / 2];
    memset(refLeftBuffer, 0, MONO_ARR_SIZE / 2 * sizeof(short));
    memset(refRightBuffer, 0, MONO_ARR_SIZE / 2 * sizeof(short));
#else
    short *refBuffer = new short[MONO_ARR_SIZE / 2];
    memset(refBuffer, 0, MONO_ARR_SIZE / 2 * sizeof(short));
#endif

    // 3channel data get
    short *srcBuffer = new short[MONO_ARR_SIZE / 2 * 3];
    memset(srcBuffer, 0, MONO_ARR_SIZE / 2 * sizeof(short) * 3);

    // for bt stream out
    short *refOriBuffer = new short[MONO_ARR_SIZE / 2 * 2];
    memset(refOriBuffer, 0, MONO_ARR_SIZE / 2 * sizeof(short) * 2);

#if defined(ENABLE_RECORD_SERVER)
    // 8channel record data buffer
    short *recBuffer = new short[MONO_ARR_SIZE / 2 * 8];
    memset(recBuffer, 0, MONO_ARR_SIZE / 2 * sizeof(short) * 8);
#endif

    int res;
    int write_len = 0;
    while (!stopRecord) {
        write_len = CHUNK_SIZE;
        err = snd_pcm_readi(alsa_record_handle, capture_data, write_len);
        if (err == 0)
            usleep(1000);
        if (err == -EAGAIN) {
            usleep(16000);
            printf("Eagain\n");
        } else if (err == -EPIPE) {
            snd_pcm_status_t *status;
            snd_pcm_status_alloca(&status);
            printf("capture EPIPE\n");
            if ((res = snd_pcm_status(alsa_record_handle, status)) < 0) {
                printf("status error: %s", snd_strerror(res));
                break;
            }

            if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
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
        } else if (err == -ESTRPIPE) {
            printf("capture ESTRPIPE\n");
            while ((res = snd_pcm_resume(alsa_record_handle)) == -EAGAIN)
                sleep(1);
            if (res < 0) {
                if ((res = snd_pcm_prepare(alsa_record_handle)) < 0) {
                    printf("capture suspend: prepare error: %s", snd_strerror(res));
                    break;
                }
            }
        } else if (err < 0) {
            printf("capture error: %s", snd_strerror(err));
            break;
        }

        if (err > 0) {
            int tempLen = 0;

#if defined(ENABLE_RECORD_SERVER)
            int recCount = 0;
            int rec_ch = 0;
            stMiscParam *recParam = NULL;
            if (g_handle)
                recParam = (stMiscParam *)g_handle->recPriv;
#endif
            // tempLen = write_len < (err * 16) ? write_len : (err * 16);
            tempLen = err;
            origin_data = reinterpret_cast<short *>(capture_data);

            for (int i = 0; i < tempLen * 16 / 2 / 8; ++i) {
                int ref_cal;
#if defined(A98HARMAN_NAVER) ||                                                                    \
    defined(A98_LGUPLUS_PORTABLE) // temp setting for A98_LGUPLUS_PORTABLE
                leftBuffer[i] = origin_data[8 * i + 4];
                rightBuffer[i] = origin_data[8 * i + 5];
#else
                leftBuffer[i] = origin_data[8 * i + 0];
                rightBuffer[i] = origin_data[8 * i + 1];
#endif

#if !defined(A98_EATON)
#if defined(HOBOT_LEGACY_STEREO_REF)
                refLeftBuffer[i] = origin_data[8 * i + 6];
                refRightBuffer[i] = origin_data[8 * i + 7];
#else
                                      /* mix stereo ref into mono.
                                       * mono ref = (left + right) / 2
                                       */
                ref_cal = (int)origin_data[8 * i + 6];
                ref_cal += (int)origin_data[8 * i + 7];
                ref_cal /= 2;
                refBuffer[i] = (short)ref_cal;
#endif
#endif // HOBOT_LEGACY_STEREO_REF
                refOriBuffer[i * 2 + 0] = origin_data[8 * i + 6];
                refOriBuffer[i * 2 + 1] = origin_data[8 * i + 7];

#if defined(A98HARMAN_NAVER) ||                                                                    \
    defined(A98_LGUPLUS_PORTABLE) // temp setting for A98_LGUPLUS_PORTABLE
                srcBuffer[i * 3 + 0] = origin_data[8 * i + 4];
                srcBuffer[i * 3 + 1] = origin_data[8 * i + 5];
#else
                srcBuffer[i * 3 + 0] = origin_data[8 * i + 0];
                srcBuffer[i * 3 + 1] = origin_data[8 * i + 1];
#endif
                srcBuffer[i * 3 + 2] = origin_data[8 * i + 6];

#if defined(ENABLE_RECORD_SERVER)
                if (g_handle && g_handle->record_callback && mic_run_flag) {
                    for (rec_ch = 0; rec_ch < 8; rec_ch++) {
                        if (1 << rec_ch & recParam->nPrepBitMask) {
                            recBuffer[recCount++] = origin_data[8 * i + rec_ch];
                        }
                    }
                }
#endif
            }
            usleep(10000);
#ifdef BLUETOOTH_SRC_LOOP_CAPTURE_ENABLE
            int bt_stream_enable = 0;
            int hw_pcm_enable = 0;

            static int btav_res = -1;

            int tfd = open("/sys/class/tdm_state/amp_table", O_RDONLY);
            if (tfd >= 0) {
                char mute_flag[4] = {0};
                if (read(tfd, (void *)mute_flag, 4) > 0) {
                    bt_stream_enable = !atoi(mute_flag);
                }
                close(tfd);
            }

            tfd = open("/sys/class/tdm_state/pcm_table", O_RDONLY);
            if (tfd >= 0) {
                char pcm_state[4] = {0};
                if (read(tfd, (void *)pcm_state, 4) > 0) {
                    hw_pcm_enable = atoi(pcm_state);
                }
                close(tfd);
            }

            if (bt_stream_enable && hw_pcm_enable &&
                (access("/tmp/loop_capture_pcm", F_OK) != -1)) {
                if (btav_res == -1) {
                    btav_res = open("/tmp/loop_capture_pcm", O_WRONLY | O_NONBLOCK);
                }
                if (btav_res >= 0) {
                    int ret = write(btav_res, refOriBuffer, err * 4);
                    if (ret < 0) {
                        close(btav_res);
                        btav_res = -1;
                    }
                }
            }
#endif
#if defined(ENABLE_RECORD_SERVER)
            if (g_handle && g_handle->record_callback && mic_run_flag) {
                g_handle->record_callback((char *)recBuffer, recCount * 2, g_handle->recPriv, 0);
            }
#endif
            if (record_status) { // begine to records
                if (src_file) {
                    // fwrite(capture_data, 1, err * 16 , src_file); //8channel
                    fwrite(srcBuffer, 2, tempLen * 16 / 2 / 8 * 3, src_file); // 1channel
                    fflush(src_file);
                }
            }

            FillBuffer(_left_circ_buffer, reinterpret_cast<char *>(leftBuffer), tempLen * 16 / 8);
            FillBuffer(_right_circ_buffer, reinterpret_cast<char *>(rightBuffer), tempLen * 16 / 8);
#if defined(HOBOT_LEGACY_STEREO_REF)
            FillBuffer(_ref_left_circ_buffer, reinterpret_cast<char *>(refLeftBuffer),
                       tempLen * 16 / 8);
            FillBuffer(_ref_right_circ_buffer, reinterpret_cast<char *>(refRightBuffer),
                       tempLen * 16 / 8);

#else
            FillBuffer(_ref_circ_buffer, reinterpret_cast<char *>(refBuffer), tempLen * 16 / 8);
#endif

#ifdef SAVEFILE
            fwrite(capture_data, 1, tempLen * 16, fileDescriptor._raw_fp);
            fflush(fileDescriptor._raw_fp);
#endif
        }
    }

    snd_pcm_close(alsa_record_handle);
    delete[] leftBuffer;
    delete[] rightBuffer;
#if defined(HOBOT_LEGACY_STEREO_REF)
    delete[] refLeftBuffer;
    delete[] refRightBuffer;
#else
    delete[] refBuffer;
#endif
    delete[] srcBuffer;
#if defined(ENABLE_RECORD_SERVER)
    delete[] recBuffer;
#endif
    delete[] refOriBuffer;
    free(capture_data);
}

#if 0
//record audio
    void recordTest()
    {
        StartEngine(_wake_handle);


        record_file = fopen(RECORD_FILE, "wb+");
        mid_file = fopen(MID_FILE, "wb+");
        dule_file = fopen(DULE_FILE, "wb+");

        int record_len = 0;
#if 1

        snd_pcm_t *alsa_record_handle = 0;
        std::cout << "recordTest enter" << std::endl;
        int err = snd_pcm_open(&alsa_record_handle, "hw:0,0", SND_PCM_STREAM_CAPTURE, 0);
        if(err < 0 || alsa_record_handle == 0) {
            printf("capture open error: %s", snd_strerror(err));
            return;
        }



        if(set_capture_params(alsa_record_handle) < 0) {
            snd_pcm_close(alsa_record_handle);
            return;
        }
        char *capture_data = (char *) malloc(32768);

        short *leftChannel = nullptr;
        short *rightChannel = nullptr;
        short *allChannel = nullptr;

        int res;
        std::cout << "capture begin #######" << std::endl;
        int write_len = 0;
        while(!stopRecord) {
            write_len = 8192;
            err = snd_pcm_readi(alsa_record_handle, capture_data, write_len);
            if(err == -EAGAIN) {
                usleep(16000);
            } else if(err == -EPIPE) {
                snd_pcm_status_t *status;
                snd_pcm_status_alloca(&status);
                printf("capture EPIPE\n");
                if((res = snd_pcm_status(alsa_record_handle, status)) < 0) {
                    printf("status error: %s", snd_strerror(res));
                    break;
                }

                if(snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
                    printf("Capture underrun !!!\n");
                    if((res = snd_pcm_prepare(alsa_record_handle)) < 0) {
                        printf("capture xrun: prepare error: %s", snd_strerror(res));
                        break;
                    }
                } else if(snd_pcm_status_get_state(status) == SND_PCM_STATE_DRAINING) {
                    if((res = snd_pcm_prepare(alsa_record_handle)) < 0) {
                        printf("capture xrun(DRAINING): prepare error: %s", snd_strerror(res));
                        break;
                    }
                }
            } else if(err == -ESTRPIPE) {
                printf("capture ESTRPIPE\n");
                while((res = snd_pcm_resume(alsa_record_handle)) == -EAGAIN)
                    sleep(1);
                if(res < 0) {
                    if((res = snd_pcm_prepare(alsa_record_handle)) < 0) {
                        printf("capture suspend: prepare error: %s", snd_strerror(res));
                        break;
                    }
                }
            } else if(err < 0) {
                printf("capture error: %s", snd_strerror(err));
                break;
            }
            //printf("write_len = %d,err=%d,err*4 = %d #####\n",write_len,err,err *4 );
            if(err > 0) {
                int tempLen = 0;
                //tempLen = write_len < (err * 4) ? write_len : (err * 4);
                tempLen = write_len < (err * 4) ? write_len : (err * 4);

#if 0
                //########file record
                if(record_len < 44100 * 2 * 2 * 5) { //5s record
                    fwrite(capture_data, 1, err * 4, record_file);
                    record_len += (err * 4);
                    fflush(record_file);
                } else {
                    if(record_file) {
                        fflush(record_file);
                        fclose(record_file);
                        record_file = NULL;
                    }
                }
#endif
                //usleep(100);
                usleep(10000);

                if(record_status) { //begine to record
                    if(src_file) {
                        fwrite(capture_data, 1, err * 4, src_file);
                        fflush(src_file);
                    }
                }

#if 1

                allChannel = reinterpret_cast<short *>(capture_data);
                leftChannel = new short[tempLen * 4 / 2 / 2];
                memset(leftChannel, 0, tempLen * 4 / 2 / 2 * sizeof(short));

                rightChannel = new short[tempLen * 4 / 2 / 2];
                memset(rightChannel, 0, tempLen * 4 / 2 / 2 * sizeof(short));

                for(int i = 0; i < tempLen * 4 / 2 / 2; i++) {
                    leftChannel[i] = allChannel[2 * i + 0];
                    rightChannel[i] = allChannel[2 * i + 1];
                }

                FillBuffer(_left_circ_buffer, reinterpret_cast<char *>(leftChannel), tempLen * 4 / 2);
                FillBuffer(_right_circ_buffer, reinterpret_cast<char *>(rightChannel), tempLen * 4 / 2);


                delete[]leftChannel;
                delete[]rightChannel;
                delete[] refBuffer;
#endif
            }

        }

        snd_pcm_close(alsa_record_handle);
        free(capture_data);

#endif
    }
#endif

#if defined(HOBOT_LEGACY_STEREO_REF)
void refResample(int ref_index)
{
    void *handle = nullptr;
    Float factor = 16000.0f / 48000;

    tCircularBuffer *inBuffer = NULL;
    tCircularBuffer *outBuffer = NULL;

    if (ref_index == 0) {
        std::cout << "ref left resample" << std::endl;
        inBuffer = _ref_left_circ_buffer;
        outBuffer = _ref_left_process_circ_buffer;
    } else {
        std::cout << "ref right resample" << std::endl;
        inBuffer = _ref_right_circ_buffer;
        outBuffer = _ref_right_process_circ_buffer;
    }

    handle = resample_open(1, factor, factor);
    long readlen = 0;

    int lastFlag = 0, inBufferUsed;
    short *read_in = nullptr;

    auto write_out = new short[DEAL_OUT_LEN];
    memset(write_out, 0, DEAL_OUT_LEN * sizeof(short));

    auto deal_in = new float[DEAL_IN_LEN];
    memset(deal_in, 0, DEAL_IN_LEN * sizeof(float));

    auto deal_out = new float[DEAL_OUT_LEN];
    memset(deal_out, 0, DEAL_OUT_LEN * sizeof(float));

    auto tempInBuffer = new char[DEAL_IN_LEN * 2];
    memset(tempInBuffer, 0, DEAL_IN_LEN * 2);

    char *tempOutBuffer = nullptr;

    while (!stopRecord) {
        if (BufferSizeFilled(inBuffer, 0) >= DEAL_IN_LEN * 2) {
            readlen = ReadBuffer(inBuffer, tempInBuffer, DEAL_IN_LEN * 2);
        } else {
            usleep(16 * 1000);
            continue;
        }

        read_in = reinterpret_cast<short *>(tempInBuffer);

        for (int i = 0; i < readlen / 2; i++) {
            deal_in[i] = read_in[i];
        }

        if (handle) {
            int o = resample_process(handle, factor, deal_in, DEAL_IN_LEN, lastFlag, &inBufferUsed,
                                     deal_out, MIN((int)(readlen * factor + 0.5f), DEAL_OUT_LEN));
            for (int i = 0; i < o; i++) {
                write_out[i] = static_cast<short>(deal_out[i]);
            }
            tempOutBuffer = reinterpret_cast<char *>(write_out);
            FillBuffer(outBuffer, tempOutBuffer, o * 2);
        }
    }

    delete[] write_out;
    delete[] deal_in;
    delete[] deal_out;
    delete[] tempInBuffer;
    resample_close(handle);
    inBuffer = NULL;
    outBuffer = NULL;
}

#else

void refResample()
{
    std::cout << "ref resample" << std::endl;
    void *handle = nullptr;
    Float factor = 16000.0f / 48000;

    handle = resample_open(1, factor, factor);
    long readlen = 0;

    int lastFlag = 0, inBufferUsed;
    short *read_in = nullptr;

    auto write_out = new short[DEAL_OUT_LEN];
    memset(write_out, 0, DEAL_OUT_LEN * sizeof(short));

    auto deal_in = new float[DEAL_IN_LEN];
    memset(deal_in, 0, DEAL_IN_LEN * sizeof(float));

    auto deal_out = new float[DEAL_OUT_LEN];
    memset(deal_out, 0, DEAL_OUT_LEN * sizeof(float));

    auto tempInBuffer = new char[DEAL_IN_LEN * 2];
    memset(tempInBuffer, 0, DEAL_IN_LEN * 2);

    char *tempOutBuffer = nullptr;

    while (!stopRecord) {
        if (BufferSizeFilled(_ref_circ_buffer, 0) >= DEAL_IN_LEN * 2) {
            readlen = ReadBuffer(_ref_circ_buffer, tempInBuffer, DEAL_IN_LEN * 2);
        } else {
            usleep(16 * 1000);
            continue;
        }

        read_in = reinterpret_cast<short *>(tempInBuffer);

        for (int i = 0; i < readlen / 2; i++) {
            deal_in[i] = read_in[i];
        }

        if (handle) {
            int o = resample_process(handle, factor, deal_in, DEAL_IN_LEN, lastFlag, &inBufferUsed,
                                     deal_out, MIN((int)(readlen * factor + 0.5f), DEAL_OUT_LEN));
            for (int i = 0; i < o; i++) {
                write_out[i] = static_cast<short>(deal_out[i]);
            }
            tempOutBuffer = reinterpret_cast<char *>(write_out);
            FillBuffer(_ref_process_circ_buffer, tempOutBuffer, o * 2);
        }
    }

    delete[] write_out;
    delete[] deal_in;
    delete[] deal_out;
    delete[] tempInBuffer;
    resample_close(handle);
}
#endif

// left channel resample
void leftResample()
{
#if 1
    std::cout << "left resample" << std::endl;
    void *handle = nullptr;
    Float factor = 16000.0f / 48000;

    handle = resample_open(1, factor, factor);
    long readlen = 0;

    int lastFlag = 0, inBufferUsed;
    short *read_in = nullptr;

    auto write_out = new short[DEAL_OUT_LEN];
    memset(write_out, 0, DEAL_OUT_LEN * sizeof(short));

    auto deal_in = new float[DEAL_IN_LEN];
    memset(deal_in, 0, DEAL_IN_LEN * sizeof(float));

    auto deal_out = new float[DEAL_OUT_LEN];
    memset(deal_out, 0, DEAL_OUT_LEN * sizeof(float));

    auto tempInBuffer = new char[DEAL_IN_LEN * 2];
    memset(tempInBuffer, 0, DEAL_IN_LEN * 2);

    char *tempOutBuffer = nullptr;

    while (!stopRecord) {

        if (BufferSizeFilled(_left_circ_buffer, 0) >= DEAL_IN_LEN * 2) {
            readlen = ReadBuffer(_left_circ_buffer, tempInBuffer, DEAL_IN_LEN * 2);
        } else {
            usleep(16 * 1000);
            continue;
        }
        read_in = reinterpret_cast<short *>(tempInBuffer);

        for (int i = 0; i < DEAL_IN_LEN; i++) {
            deal_in[i] = read_in[i];
        }

        if (handle) {
            int o = resample_process(handle, factor, deal_in, DEAL_IN_LEN, lastFlag, &inBufferUsed,
                                     deal_out, MIN((int)(readlen * factor + 0.5f), DEAL_OUT_LEN));
            for (int i = 0; i < o; i++) {
                write_out[i] = static_cast<short>(deal_out[i]);
            }
            //              std::cout << "readlen is " << readlen << ", o is " << o << std::endl;
            tempOutBuffer = reinterpret_cast<char *>(write_out);
            FillBuffer(_left_process_circ_buffer, tempOutBuffer, o * 2);
            //              fwrite(tempOutBuffer, 1, o * 2, _left_fp);
            //              fflush(_left_fp);
        }
    }
    // printf("leftResample crash###########################\n");
    delete[] write_out;
    delete[] deal_in;
    delete[] deal_out;
    delete[] tempInBuffer;
    resample_close(handle);
#endif
}

// right channel resample
void rightResample()
{

#if 1
    std::cout << "right resample" << std::endl;
    void *handle = nullptr;
    Float factor = 16000.0f / 48000;

    handle = resample_open(1, factor, factor);
    long readlen = 0;

    int lastFlag = 0, inBufferUsed;
    short *read_in = nullptr;

    auto write_out = new short[DEAL_OUT_LEN];
    memset(write_out, 0, DEAL_OUT_LEN * sizeof(short));

    auto deal_in = new float[DEAL_IN_LEN];
    memset(deal_in, 0, DEAL_IN_LEN * sizeof(float));

    auto deal_out = new float[DEAL_OUT_LEN];
    memset(deal_out, 0, DEAL_OUT_LEN * sizeof(float));

    auto tempInBuffer = new char[DEAL_IN_LEN * 2];
    memset(tempInBuffer, 0, DEAL_IN_LEN * 2);

    char *tempOutBuffer = nullptr;

    while (!stopRecord) {

        if (BufferSizeFilled(_right_circ_buffer, 0) >= DEAL_IN_LEN * 2) {
            readlen = ReadBuffer(_right_circ_buffer, tempInBuffer, DEAL_IN_LEN * 2);
        } else {
            usleep(16 * 1000);
            continue;
        }
        read_in = reinterpret_cast<short *>(tempInBuffer);

        for (int i = 0; i < DEAL_IN_LEN; i++) {
            deal_in[i] = read_in[i];
        }

        if (handle) {
            int o = resample_process(handle, factor, deal_in, DEAL_IN_LEN, lastFlag, &inBufferUsed,
                                     deal_out, MIN((int)(readlen * factor + 0.5f), DEAL_OUT_LEN));
            for (int i = 0; i < o; i++) {
                write_out[i] = static_cast<short>(deal_out[i]);
            }
            //              std::cout << "readlen is " << readlen << ", o is " << o << std::endl;
            tempOutBuffer = reinterpret_cast<char *>(write_out);
            FillBuffer(_right_process_circ_buffer, tempOutBuffer, o * 2);
            //              fwrite(tempOutBuffer, 1, o * 2, _right_fp);
            //              fflush(_right_fp);
        }
    }
    // printf("rightResample crash###########################\n");
    delete[] write_out;
    delete[] deal_in;
    delete[] deal_out;
    delete[] tempInBuffer;
    resample_close(handle);
#endif
}

#define LPCFG_HOBOT_LICENSE_SN "audioFrontEnd.hobotConfig.sn"
#define LPCFG_HOBOT_LICENSE_DIR "audioFrontEnd.hobotConfig.licenseDirPath"
#define LPCFG_HOBOT_HISF_CFG "audioFrontEnd.hobotConfig.hisfPath"
#define DEFAULT_HOBOT_LICENSE_DIR "/vendor/"
#define DEFAULT_HOBOT_HISF_CFG "/system/workdir/lib/hisf_config.ini"

static int update_hobot_cfg(char *license_dir, int license_dir_len, char *hisf_cfg,
                            int hisf_cfg_len, char *sn, int sn_len)
{
    char *hobot_license_sn = NULL;
    char *hobot_license_dir = NULL;
    char *hobot_hisf_cfg = NULL;

    if (!license_dir || !hisf_cfg || !sn) {
        return -1;
    }

    /* this is to make compatible with old API and bitanswer authorization,
     * since parameters passed to Init() is different than the one using license.txt.
     * lpcfg_init called in main();
     */
    if (lpcfg_read_string(LPCFG_HOBOT_LICENSE_SN, &hobot_license_sn) == 0) {
        memset(sn, 0x0, sn_len);
        snprintf(sn, sn_len, "%s", hobot_license_sn);

        memset(license_dir, 0x0, license_dir_len);
        if (lpcfg_read_string(LPCFG_HOBOT_LICENSE_DIR, &hobot_license_dir) == 0) {
            snprintf(license_dir, license_dir_len, "%s", hobot_license_dir);
        } else {
            // use default one
            snprintf(license_dir, license_dir_len, "%s", DEFAULT_HOBOT_LICENSE_DIR);
        }

        memset(hisf_cfg, 0x0, hisf_cfg_len);
        if (lpcfg_read_string(LPCFG_HOBOT_HISF_CFG, &hobot_hisf_cfg) == 0) {
            snprintf(hisf_cfg, hisf_cfg_len, "%s", hobot_hisf_cfg);
        } else {
            // use default one
            snprintf(hisf_cfg, hisf_cfg_len, "%s", DEFAULT_HOBOT_HISF_CFG);
        }
    }

    return 0;
}

/**
 * init sdk setting
 */
int initHobotSdk()
{
    printf("%s %d++++\n", __FUNCTION__, __LINE__);
    HOBOT_ERROR_CODE code = HOBOT_ERROR;
    auth_print_once = 0;

    // set log level
    // code = ConfigLogLevel(HOBOT_LOG_DEBUG);
    code = ConfigLogLevel(HOBOT_LOG_NULL);
    if (code != HOBOT_OK) {
        std::cout << "config log level error" << std::endl;
    }

    // set the number of mic
    code = SetParams(HOBOT_SPEECH_PARAM_CHANNEL_NUM, sizeof(int), (void *)HOBOT_CHANNEL_NUM);
    if (code != HOBOT_OK) {
        std::cout << "set mic num error" << std::endl;
    }

    // set input audio callback
    code =
        SetParams(HOBOT_SPEECH_PARAM_AUDIO_DATA_CB, sizeof(void *), (void *)hobot_audio_callback);
    if (code != HOBOT_OK) {
        std::cout << "set audio data callback" << std::endl;
    }

    // set output audio callback
    code = SetParams(HOBOT_SPEECH_PARAM_AUDIO_ENHANCE_CB, sizeof(void *),
                     (void *)hobot_enhance_audio_callback);
    if (code != HOBOT_OK) {
        std::cout << "set enhance audio data callback" << std::endl;
    }

    code = SetParams(HOBOT_SPEECH_PARAM_AUTH_CB, sizeof(void *), (void *)hobot_auth_cb);
    code = SetParams(HOBOT_SPEECH_PARAM_USE_EXTERNAL_DECORDER, sizeof(bool), (void *)true);
#if 0
        //set event callback
        code = SetParams(HOBOT_SPEECH_PARAM_EVENT_CB, sizeof(void *), (void *)(hobot_event_callback));
        if(code != HOBOT_OK) {
            std::cout << "set event callback" << std::endl;
        }

        //set wake up success callback
        code = SetParams(HOBOT_SPEECH_PARAM_RESULT_CB, sizeof(void *), (void *)(hobot_result_callback));
        if(code != HOBOT_OK) {
            std::cout << "set result callback" << std::endl;
        }
#endif

    //    SetParams(HOBOT_SPEECH_PARAM_USE_EXTERNAL_DECORDER, sizeof(bool), (void *) true);

    if (!_save_audio_path.empty()) {
        code = SetParams(HOBOT_SPEECH_PARAM_LOG_SAVE_PATH, sizeof(char *),
                         (void *)_save_audio_path.c_str());

        if (code != HOBOT_OK) {
            std::cout << "set log save path" << std::endl;
        }
    }

    std::cout << "start init" << std::endl;
    Soft_prep_hobot_getID(hobot_id, sizeof(hobot_id), authcode, sizeof(authcode));
    // Set legacy value for hisf_cfg.
    snprintf(hisf_cfg, sizeof(hisf_cfg), "%s", "/system/workdir/lib");
    // update config from lpcfg.json if any.
    update_hobot_cfg(hobot_id, sizeof(hobot_id), hisf_cfg, sizeof(hisf_cfg), authcode,
                     sizeof(authcode));
#ifdef NAVER_LINE
    if (access("/tmp/hisf_config.ini", 0) != 0) {
        system("cp -rf /system/workdir/lib/hisf_config.ini /tmp/hisf_config.ini");
    }
    code = Init(hobot_id, authcode, "/tmp/hisf_config.ini");
#else
    printf("HOBOT info: '%s', '%s', '%s'\n", hobot_id, authcode, hisf_cfg);
    code = Init(hobot_id, authcode, hisf_cfg);
#endif

    if (code != HOBOT_OK) {
        std::cout << "sdk init error" << std::endl;
        return -1;
    }

    // create speech handle, include wake handle and asr handle
    _wake_handle = CreateSpeechEngine("", WAKE);
    //    _asr_handle = CreateSpeechEngine("", ASR);
    printf("%s %d----\n", __FUNCTION__, __LINE__);

    return 0;
}

void *hobot_initialize(SOFT_PREP_INIT_PARAM input_param)
{
    /* for testing
    Soft_prep_hobot_wrapper * handle = (Soft_prep_hobot_wrapper
    *)malloc(sizeof(Soft_prep_hobot_wrapper));
    if(!handle)
    {
        printf("malloc handle failed\n");
        return 0;
    }

    memset(handle, 0, sizeof(Soft_prep_hobot_wrapper));
    test_thread = std::thread (&do_pcm_read);
    */

    std::cout << "hobot_initialize +++" << std::endl;
    Soft_prep_hobot_wrapper *handle =
        (Soft_prep_hobot_wrapper *)malloc(sizeof(Soft_prep_hobot_wrapper));
    if (!handle) {
        printf("malloc handle failed\n");
        return NULL;
    }
    memset(handle, 0, sizeof(Soft_prep_hobot_wrapper));
    handle->cap_dev = input_param.cap_dev;

#if defined(HOBOT_LEGACY_STEREO_REF)
    _ref_left_circ_buffer = CreateCircularBuffer(256 * 1024);
    FillZeroes(_ref_left_circ_buffer);

    _ref_right_circ_buffer = CreateCircularBuffer(256 * 1024);
    FillZeroes(_ref_right_circ_buffer);

    _ref_left_process_circ_buffer = CreateCircularBuffer(256 * 1024);
    FillZeroes(_ref_left_process_circ_buffer);

    _ref_right_process_circ_buffer = CreateCircularBuffer(256 * 1024);
    FillZeroes(_ref_right_process_circ_buffer);

#else
    _ref_circ_buffer = CreateCircularBuffer(256 * 1024);
    FillZeroes(_ref_circ_buffer);

    _ref_process_circ_buffer = CreateCircularBuffer(256 * 1024);
    FillZeroes(_ref_process_circ_buffer);
#endif

    _left_circ_buffer = CreateCircularBuffer(256 * 1024);
    FillZeroes(_left_circ_buffer);

    _right_circ_buffer = CreateCircularBuffer(256 * 1024);
    FillZeroes(_right_circ_buffer);

    _left_process_circ_buffer = CreateCircularBuffer(256 * 1024);
    FillZeroes(_left_process_circ_buffer);

    _right_process_circ_buffer = CreateCircularBuffer(256 * 1024);
    FillZeroes(_right_process_circ_buffer);

#if 0
        std::string origin_audio_file = _save_audio_path + "/" + "hrsc_raw_441k.pcm";
        _origin_fp = fopen(origin_audio_file.c_str(), "wb");


        if(!_origin_fp) {
            std::cout << "create origin file failed" << std::endl;
            return EXIT_FAILURE;
        }
#endif

    if (initHobotSdk()) { // return not 0

        Soft_prep_hobot_wrapper *wrapper = (Soft_prep_hobot_wrapper *)handle;
        free(wrapper);

        return NULL;
    }

    std::cout << "create file finish" << std::endl;
    std::cout << "start open recorder" << std::endl;

    if (handle) {
        std::cout << "handle is init" << std::endl;
        g_handle = handle;
    } else
        std::cout << "handle is not init" << std::endl;

    record_thread = std::thread(&recordTest);
#if defined(HOBOT_LEGACY_STEREO_REF)
    refResample_thread[0] = std::thread(&refResample, 0); // ref left
    refResample_thread[1] = std::thread(&refResample, 1); // ref right
#else
    refResample_thread = std::thread(&refResample);
#endif

    leftResample_thread = std::thread(&leftResample);
    rightResample_thread = std::thread(&rightResample);

    // StartEngine(_wake_handle);
    std::cout << "hobot_version:" << GetVersion() << std::endl;
    std::cout << "hobot_initialize return" << std::endl;
    return (void *)handle;
}

void free_all_circ_buffers()
{
    DestroyCircularBuffer(_left_circ_buffer);
    DestroyCircularBuffer(_right_circ_buffer);
    DestroyCircularBuffer(_left_process_circ_buffer);
    DestroyCircularBuffer(_right_process_circ_buffer);
#if defined(HOBOT_LEGACY_STEREO_REF)
    DestroyCircularBuffer(_ref_left_circ_buffer);
    DestroyCircularBuffer(_ref_right_circ_buffer);
    DestroyCircularBuffer(_ref_left_process_circ_buffer);
    DestroyCircularBuffer(_ref_right_process_circ_buffer);
#else
    DestroyCircularBuffer(_ref_circ_buffer);
    DestroyCircularBuffer(_ref_process_circ_buffer);

#endif
}

int hobot_deInitialize(void *handle)
{
    if (!stopRecord) {
        stopRecord = 1;
        record_thread.join();
#if defined(HOBOT_LEGACY_STEREO_REF)
        refResample_thread[0].join();
        refResample_thread[1].join();
#else
        refResample_thread.join();
#endif
        leftResample_thread.join();
        rightResample_thread.join();
    }
    free_all_circ_buffers();
    Soft_prep_hobot_wrapper *wrapper = (Soft_prep_hobot_wrapper *)handle;
    free(wrapper);

    auth_print_once = 0;

    return 0;
}

int hobot_reinit_cfg(void *handle)
{
    std::cout << "hobot_reinit_cfg" << std::endl;

    if (_wake_handle) {
#ifdef NAVER_LINE
        if (access("/tmp/hisf_reconfig.ini", 0) == 0) {
            std::cout << "reinit with hisf_reconfig.ini" << std::endl;
            system("cp -rf /tmp/hisf_reconfig.ini /tmp/hisf_config.ini");
            system("rm -rf /tmp/hisf_reconfig.ini");
            SwitchConfig("/tmp/hisf_config.ini");
        }
#endif
        return 0;
    }

    return -1;
}

int hobot_setFillCallback(int handle, soft_prep_capture_CBACK *callback_func, void *opaque)
{
    Soft_prep_hobot_wrapper *wrapper = (Soft_prep_hobot_wrapper *)handle;
    wrapper->callback = callback_func;
    wrapper->fillPriv = opaque;
    return 0;
}

int hobot_setMicrawCallback(int handle, soft_prep_micraw_CBACK *callback_func, void *opaque)
{
    Soft_prep_hobot_wrapper *wrapper = (Soft_prep_hobot_wrapper *)handle;
    wrapper->micrawCallback = callback_func;
    wrapper->micrawPriv = opaque;
    return 0;
}

#if defined(ENABLE_RECORD_SERVER)
int hobot_setRecordCallBack(int handle, soft_prep_record_CBACK *callback_func, void *recPriv)
{
    Soft_prep_hobot_wrapper *wrapper = (Soft_prep_hobot_wrapper *)handle;
    wrapper->record_callback = callback_func;
    wrapper->recPriv = recPriv;
    return 0;
}
#endif

int hotot_delele_file(char *destfile)
{
    char buf[128] = {0};
    if (access(destfile, 0) == 0) {
        printf("rm %s\n", destfile);
        snprintf(buf, sizeof(buf), "rm -rf %s", destfile);
        system(buf);
    }
    return 0;
}

int hobot_flushState(void)
{
    int ret = 0;
    ret = FlushState();
    return 0;
}

// int status,int source,char path)//status: 0 stop 1 start    source:0 44.1k  1:16kprocessed  path
int hobot_source_record(int status, int source, char *path)
{
    time_t timep;
    struct tm *p;

    time(&timep);
    p = localtime(&timep);

    char buf[128] = {0};
    if (status) { // start record
#if 0
            if(access(SRC_FILE, 0) == 0) {
                printf("rm %s\n", SRC_FILE);
                snprintf(buf, sizeof(buf), "rm -rf %s", SRC_FILE);
                system(buf);
            }
#endif
        if (source == 0x100) { // pre-process pcm data
            memset(buf, 0, sizeof(buf));
            snprintf(buf, sizeof(buf), "%s/hobot_pre.pcm", path);
            printf("file:%s\n", buf);
            hotot_delele_file(buf);
            if (src_file == NULL) {
                src_file = fopen(buf, "wb+");
                if (src_file == NULL)
                    return 0;
            }

            record_status = 1;
        } else if (source == 0) { // 44.1k
            memset(buf, 0, sizeof(buf));
            snprintf(buf, sizeof(buf), "%shobot_%s.pcm", path, PCM_441K_2CH);
            // snprintf(buf, sizeof(buf), "%shobot_%s_%d-%d-%d_%d_%d_%d.pcm", path, PCM_441K_2CH,
            // (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday,
            //        (p->tm_hour + 12), p->tm_min, p->tm_sec);
            printf("file:%s\n", buf);
            hotot_delele_file(buf);
            if (src_file == NULL) {
                src_file = fopen(buf, "wb+");
                if (src_file == NULL)
                    return 0;
            }
        } else {
            // snprintf(buf, sizeof(buf), "%shobot_%s.pcm", path, PCM_441K_2CH);
            /*
                        snprintf(buf, sizeof(buf), "%shobot_%s_%d-%d-%d_%d_%d_%d.pcm", path,
            PCM_441K_2CH, (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday,
                                (p->tm_hour + 12), p->tm_min, p->tm_sec);
                        printf("file:%s\n", buf);
                        hotot_delele_file(buf);
                        if(src_file == NULL) {
                            src_file = fopen(buf, "wb+");
                            if(src_file == NULL)
                                return 0;
                        }
            */
            memset(buf, 0, sizeof(buf));
            // snprintf(buf, sizeof(buf), "%shobot_%s.pcm", path, PCM_16K_5CH_INPUT);
            snprintf(buf, sizeof(buf), "%shobot_%s_%d-%d-%d_%d_%d_%d.pcm", path, PCM_16K_5CH_INPUT,
                     (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday, (p->tm_hour + 12), p->tm_min,
                     p->tm_sec);
            printf("file:%s\n", buf);
            hotot_delele_file(buf);
            if (ch5_srcFile == NULL) {
                ch5_srcFile = fopen(buf, "wb+");
                if (ch5_srcFile == NULL)
                    return 0;
            }
            memset(buf, 0, sizeof(buf));
            // snprintf(buf, sizeof(buf), "%shobot_%s.pcm", path, PCM_16K_2CH_OUTPUT);
            snprintf(buf, sizeof(buf), "%shobot_%s_%d-%d-%d_%d_%d_%d.pcm", path, PCM_16K_2CH_OUTPUT,
                     (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday, (p->tm_hour + 12), p->tm_min,
                     p->tm_sec);
            printf("file:%s\n", buf);
            hotot_delele_file(buf);
            if (ch2_outFile == NULL) {
                ch2_outFile = fopen(buf, "wb+");
                if (ch2_outFile == NULL) {
                    if (ch5_srcFile)
                        fclose(ch5_srcFile);
                    return 0;
                }
            }
        }
        record_status = 1;
    } else { // start record
        record_status = 0;
        if (src_file) {
            fclose(src_file);
            src_file = NULL;
        }
        if (ch5_srcFile) {
            fclose(ch5_srcFile);
            ch5_srcFile = NULL;
        }
        if (ch2_outFile) {
            fclose(ch2_outFile);
            ch2_outFile = NULL;
        }
    }
    return 0;
}
}
