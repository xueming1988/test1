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
#include <stdarg.h>
#include <alsa/asoundlib.h>
#include <chrono>
#include <future>
#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>

#include "circ_buff.h"
#include "pthread.h"
#include "wm_api.h"
#include "wm_util.h"

#include "hrsc_sdk.h"
#include "soft_prep_hobot_xiaowei_func.h"

extern WIIMU_CONTEXT *g_wiimu_shm;
extern int asr_waked;
const int DEAL_IN_LEN = 480;
const int DEAL_OUT_LEN = 160;

// channel num
const int CHANNELS = 8;
const int PER_BYTES = 2;
const int CHUNK_SIZE = 1024;
const int SAMPLE_RATE = 48000;
const int ALLOCATE_ARR_SIZE = CHUNK_SIZE * CHANNELS * PER_BYTES; // bytes
const int MONO_ARR_SIZE = ALLOCATE_ARR_SIZE / 8;

extern "C" {
#include "libresample.h"
#include "resample_defs.h"
#include <time.h>

// std::thread test_thread;
std::thread record_thread;
std::thread process_thread;
std::thread refLeftResample_thread;
std::thread refRightResample_thread;
std::thread leftResample_thread;
std::thread rightResample_thread;

// To mix asr and voip data into one stream.
std::thread process_data_mixer_thread;

// void * do_pcm_test(void * priv) ;
void recordTest();
void refLeftResample();
void refRightResample();
void leftResample();
void rightResample();
void hobot_enhance_audio_callback(char *audio_data, int32_t buff_len);

FILE *record_file;
FILE *mid_file;
FILE *dule_file;

// 44.1k source
FILE *src_file = NULL;

// processed
FILE *ch4_srcFile = NULL;
FILE *ch2_outFile = NULL;

// FIXME:DEBUG
#if defined(SECURITY_DEBUGPRINT)
static FILE *voip_file = NULL;
static FILE *mixer_file = NULL;
#endif

#define RECORD_FILE "/tmp/web/record_file.pcm"
#define MID_FILE "/tmp/web/mid_file.pcm"
#define DULE_FILE "/tmp/web/dule_file.pcm"
#define SRC_FILE "/tmp/web/src_file.pcm"
#define HRSC_CFG_FILE_PATH "/system/workdir/lib/hrsc/"

#define PCM_441K_2CH "PCM_48K_3CH"
#define PCM_16K_5CH_INPUT "PCM_16K_3CH_INPUT"
#define PCM_16K_2CH_OUTPUT "PCM_16K_2CH_OUTPUT"

static int record_status = 0;

std::mutex _mtx;

std::string _save_audio_path = "";
volatile bool stopRecord = false;

hrsc_effect_config_t hrsc_cfg;

#define ASR_UPLOAD_DATA_DEBUG 0
#if ASR_UPLOAD_DATA_DEBUG
int g_wakeup_flag = 0;
#endif

pthread_t _cancel_tid;

int _wakeup_count = 0;

// origin audio data
tCircularBuffer *_ref_left_circ_buffer;
tCircularBuffer *_ref_right_circ_buffer;
tCircularBuffer *_left_circ_buffer;
tCircularBuffer *_right_circ_buffer;
tCircularBuffer *_ref_left_process_circ_buffer;
tCircularBuffer *_ref_right_process_circ_buffer;
tCircularBuffer *_right_process_circ_buffer;
tCircularBuffer *_left_process_circ_buffer;

// cache buffer:
tCircularBuffer *hobot_asr_data_cache;
tCircularBuffer *hobot_voip_data_cache;

static Soft_prep_hobot_wrapper *g_handle = NULL;

bool mic_run_flag;

int enableHobotMicrophoneData(int value)
{
    printf("enableStreamingMicrophoneData\n");
    mic_run_flag = value;
    return 0;
}

static void hobot_log_callback(hrsc_log_lvl_t lvl, const char *tag, const char *format, ...)
{
    char buffer[1024] = {0};
    va_list args;

    if (lvl > HRSC_LOG_LVL_DEBUG) {
        return;
    }

    va_start(args, format);
    vsnprintf(buffer, 1023, format, args);
    va_end(args);
    printf("%s", buffer);
}

static void hobot_event_callback(const void *cookie, hrsc_event_t event)
{
    wiimu_log(1, 0, 0, 0, "%s %d cookie=%p, event=%d\n", __FUNCTION__, __LINE__, cookie, event);
}

static void hobot_wakeup_data_callback(const void *cookie, const hrsc_callback_data_t *data,
                                       int keyword_index)
{
    ++_wakeup_count;
    if (NULL == data) {
        std::cout << __FUNCTION__ << ": error: data=NULL" << std::endl;
        return;
    }
    std::cout << __FUNCTION__ << " " << __LINE__ << ": ";
    std::cout << "cookie=" << cookie << ", ";
    std::cout << "data.size=" << data->buffer.size << ", ";
    std::cout << "data.type=" << data->type << ", ";
    std::cout << "data.score=" << data->score << ", ";
    std::cout << "data.angle=" << data->angle << ", ";
    std::cout << "data.timestamp=" << data->buffer.start_time << ", ";
    std::cout << "keyword_index=" << keyword_index << ", ";
    std::cout << "wakeup_count=" << _wakeup_count << std::endl;
#if ASR_UPLOAD_DATA_DEBUG
    g_wakeup_flag = 1;
#endif
    SocketClientReadWriteMsg("/tmp/RequestASRTTS", "talkstart:1", strlen("talkstart:1"), NULL, NULL,
                             0);
#if defined(ASR_WAKEUP_TEXT_UPLOAD)
    asr_waked = 1;
#endif
}

static void hobot_asr_data_callback(const void *cookie, const hrsc_callback_data_t *data)
{
    /*static unsigned int l = 0;
    printf("%s %d cookie=%p, data=%p, type=%d, size=%d\n", __FUNCTION__, __LINE__, cookie, data,
           data->type, l += data->buffer.size);
    if (data->type == HRSC_CALLBACK_DATA_TAIL)
        l = 0;
    */
}

static void hobot_processed_data_callback(const void *cookie, const hrsc_callback_data_t *data)
{
    // Fill processed data into ring buffer for cache
    FillBuffer(hobot_asr_data_cache, (char *)data->buffer.raw, data->buffer.size);
}

static void hobot_voip_data_callback(const void *cookie, const hrsc_callback_data_t *data)
{
    // Fill processed data into ring buffer for cache
    FillBuffer(hobot_voip_data_cache, (char *)data->buffer.raw, data->buffer.size);
// FIXME:DEBUG
#if defined(SECURITY_DEBUGPRINT)
    if (access("/tmp/save_voip_data", 0) == 0) {
        fwrite((char *)data->buffer.raw, 1, data->buffer.size, voip_file);
        fflush(voip_file);
    }
#endif
}

static void hobot_secure_auth_callback(int code)
{
    std::cout << "auth code is: " << code << std::endl;
    std::cout << ((1 == code) ? "auth success" : "failed auth") << std::endl;
}

// origin audio data callback
int32_t hobot_audio_callback(char *audio_data, int32_t buff_len)
{
    int32_t datalen = buff_len / 4;
    char *leftBuffer = new char[datalen];
    char *rightBuffer = new char[datalen];
    char *refLeftBuffer = new char[datalen];
    char *refRightBuffer = new char[datalen];

    short *realLeftBuffer = nullptr;
    short *realRightBuffer = nullptr;
    short *realRefLeftBuffer = nullptr;
    short *realRefRightBuffer = nullptr;

    short *realBuffer = new short[buff_len / 2];
    // 2channel data get
    short *mic2chBuffer = new short[buff_len / 2];

    while (true) {
        if (BufferSizeFilled(_left_process_circ_buffer, 0) >= datalen &&
            BufferSizeFilled(_right_process_circ_buffer, 0) >= datalen &&
            BufferSizeFilled(_ref_left_process_circ_buffer, 0) >= datalen &&
            BufferSizeFilled(_ref_right_process_circ_buffer, 0) >= datalen) {
            ReadBuffer(_left_process_circ_buffer, leftBuffer, datalen);
            ReadBuffer(_right_process_circ_buffer, rightBuffer, datalen);
            ReadBuffer(_ref_left_process_circ_buffer, refLeftBuffer, datalen);
            ReadBuffer(_ref_right_process_circ_buffer, refRightBuffer, datalen);
            realLeftBuffer = reinterpret_cast<short *>(leftBuffer);
            realRightBuffer = reinterpret_cast<short *>(rightBuffer);
            realRefLeftBuffer = reinterpret_cast<short *>(refLeftBuffer);
            realRefRightBuffer = reinterpret_cast<short *>(refRightBuffer);

            for (int i = 0; i < datalen / 2; i++) {
                realBuffer[i * 4 + 0] = realLeftBuffer[i];
                realBuffer[i * 4 + 1] = realRightBuffer[i];
                realBuffer[i * 4 + 2] = realRefLeftBuffer[i];
                realBuffer[i * 4 + 3] = realRefRightBuffer[i];

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
        if (ch4_srcFile) {
            fwrite(realBuffer, 1, buff_len, ch4_srcFile);
            fflush(ch4_srcFile);
        }
    }

    if (g_handle && g_handle->micrawCallback && mic_run_flag) {
        g_handle->micrawCallback(g_handle->micrawPriv, (char *)mic2chBuffer, datalen * 2);
    }

    delete[] leftBuffer;
    delete[] rightBuffer;
    delete[] refLeftBuffer;
    delete[] refRightBuffer;

    delete[] realBuffer;
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
#if ASR_UPLOAD_DATA_DEBUG
    if (g_wakeup_flag == 1) {
        g_wakeup_flag = 0;
        short *p = reinterpret_cast<short *>(audio_data);
        std::cout << buff_len << " |";
        for (int i = 0; i < 8; ++i) {
            std::cout << " " << p[i];
        }
        std::cout << std::endl;
    }
#endif

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

void recordTest()
{
    char capdev[32] = {0};
    if (g_handle && g_handle->cap_dev == SOFT_PREP_CAP_TDMC) {
        snprintf(capdev, sizeof(capdev), "%s", "hw:0,1");
    } else {
        snprintf(capdev, sizeof(capdev), "%s", "hw:0,2");
    }

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

    short *refLeftBuffer = new short[MONO_ARR_SIZE / 2];
    memset(refLeftBuffer, 0, MONO_ARR_SIZE / 2 * sizeof(short));

    short *refRightBuffer = new short[MONO_ARR_SIZE / 2];
    memset(refRightBuffer, 0, MONO_ARR_SIZE / 2 * sizeof(short));

    // 4channel data get
    short *srcBuffer = new short[MONO_ARR_SIZE / 2 * 4];
    memset(srcBuffer, 0, MONO_ARR_SIZE / 2 * sizeof(short) * 4);

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

#if defined(A98HARMAN_NAVER) ||                                                                    \
    defined(A98_LGUPLUS_PORTABLE) // temp setting for A98_LGUPLUS_PORTABLE
                leftBuffer[i] = origin_data[8 * i + 4];
                rightBuffer[i] = origin_data[8 * i + 5];
#else
                leftBuffer[i] = origin_data[8 * i + 0];
                rightBuffer[i] = origin_data[8 * i + 1];
#endif

                refOriBuffer[i * 2 + 0] = origin_data[8 * i + 6];
                refOriBuffer[i * 2 + 1] = origin_data[8 * i + 7];
                refLeftBuffer[i] = origin_data[8 * i + 6];
                refRightBuffer[i] = origin_data[8 * i + 7];

#if defined(A98HARMAN_NAVER) ||                                                                    \
    defined(A98_LGUPLUS_PORTABLE) // temp setting for A98_LGUPLUS_PORTABLE
                srcBuffer[i * 4 + 0] = origin_data[8 * i + 4];
                srcBuffer[i * 4 + 1] = origin_data[8 * i + 5];
#else
                srcBuffer[i * 4 + 0] = origin_data[8 * i + 0];
                srcBuffer[i * 4 + 1] = origin_data[8 * i + 1];
#endif
                srcBuffer[i * 4 + 2] = origin_data[8 * i + 6];
                srcBuffer[i * 4 + 3] = origin_data[8 * i + 7];

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
                    fwrite(srcBuffer, 2, tempLen * 16 / 2 / 8 * 4, src_file); // 1channel
                    fflush(src_file);
                }
            }

            FillBuffer(_left_circ_buffer, reinterpret_cast<char *>(leftBuffer), tempLen * 16 / 8);
            FillBuffer(_right_circ_buffer, reinterpret_cast<char *>(rightBuffer), tempLen * 16 / 8);
            FillBuffer(_ref_left_circ_buffer, reinterpret_cast<char *>(refLeftBuffer),
                       tempLen * 16 / 8);
            FillBuffer(_ref_right_circ_buffer, reinterpret_cast<char *>(refRightBuffer),
                       tempLen * 16 / 8);

#ifdef SAVEFILE
            fwrite(capture_data, 1, tempLen * 16, fileDescriptor._raw_fp);
            fflush(fileDescriptor._raw_fp);
#endif
        }
    }

    snd_pcm_close(alsa_record_handle);
    delete[] leftBuffer;
    delete[] rightBuffer;
    delete[] refLeftBuffer;
    delete[] refRightBuffer;
    delete[] srcBuffer;
#if defined(ENABLE_RECORD_SERVER)
    delete[] recBuffer;
#endif
    delete[] refOriBuffer;
    free(capture_data);
}

static void processThread()
{
    char data[2560] = "";
    hrsc_audio_buffer_t hrsc_input_buffer;
    hrsc_input_buffer.start_time = 0;
    hrsc_input_buffer.size = 2560;
    hrsc_input_buffer.raw = data;
    while (!stopRecord) {
        if (BufferSizeFilled(_left_process_circ_buffer, 0) <= 640 ||
            BufferSizeFilled(_right_process_circ_buffer, 0) <= 640 ||
            BufferSizeFilled(_ref_left_process_circ_buffer, 0) <= 640 ||
            BufferSizeFilled(_ref_right_process_circ_buffer, 0) <= 640) {
            usleep(10000);
            continue;
        }
        hobot_audio_callback(data, 2560);
        hrsc_process(&hrsc_input_buffer);
    }
}

// To mix asr and voip data into one stream
static void processDataMixerThread()
{
    int size = 640;
    char *asr_data = new char[size];
    char *voip_data = new char[size];

    short *asr_data_ptr = nullptr;
    short *voip_data_ptr = nullptr;

    short *mixer_buffer = new short[size];
    if (!mixer_buffer) {
        printf("HOBOT: failed to allocate mixer buffer\n");
        exit(1);
    }
    memset(mixer_buffer, 0, 640);
    memset(voip_data, 0, size);
    printf("HOBOT: %s starts...\n", __FUNCTION__);
    while (!stopRecord) {
        if (BufferSizeFilled(hobot_asr_data_cache, 0) <= size) {
            usleep(10000);
            continue;
        }

#if defined(VOIP_SUPPORT)
        if (BufferSizeFilled(hobot_voip_data_cache, 0) <= size) {
            usleep(10000);
            continue;
        }
        ReadBuffer(hobot_voip_data_cache, voip_data, size);
#endif

        // now we have asr data and voip data. mix them.
        ReadBuffer(hobot_asr_data_cache, asr_data, size);

        asr_data_ptr = reinterpret_cast<short *>(asr_data);
        voip_data_ptr = reinterpret_cast<short *>(voip_data);

        for (int i = 0; i < size / 2; ++i) {
            mixer_buffer[i * 2 + 0] = asr_data_ptr[i];
            mixer_buffer[i * 2 + 1] =
                voip_data_ptr[i]; // If enable voip, fill real voip data, otherwise, fill 0s.
        }

        // stream is ready to send out.
        hobot_enhance_audio_callback((char *)mixer_buffer, size * 2);

#if defined(SECURITY_DEBUGPRINT)
        // FIXME: debug
        if (access("/tmp/save_mixer_data", 0) == 0) {
            fwrite((char *)mixer_buffer, 1, size * 2, mixer_file);
            fflush(mixer_file);
        }
#endif
    }

    printf("HOBOT: %s quits...\n", __FUNCTION__);

    delete[] asr_data;
    delete[] voip_data;
    delete[] mixer_buffer;
}

void refLeftResample()
{
    std::cout << "ref left resample" << std::endl;
    void *handle = nullptr;
    volatile Float factor = 16000.0f / 48000;

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

        if (BufferSizeFilled(_ref_left_circ_buffer, 0) >= DEAL_IN_LEN * 2) {
            readlen = ReadBuffer(_ref_left_circ_buffer, tempInBuffer, DEAL_IN_LEN * 2);
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
            FillBuffer(_ref_left_process_circ_buffer, tempOutBuffer, o * 2);
        }
    }

    delete[] write_out;
    delete[] deal_in;
    delete[] deal_out;
    delete[] tempInBuffer;
    resample_close(handle);
}

void refRightResample()
{
    std::cout << "ref right resample" << std::endl;
    void *handle = nullptr;
    volatile Float factor = 16000.0f / 48000;

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

        if (BufferSizeFilled(_ref_right_circ_buffer, 0) >= DEAL_IN_LEN * 2) {
            readlen = ReadBuffer(_ref_right_circ_buffer, tempInBuffer, DEAL_IN_LEN * 2);
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
            FillBuffer(_ref_right_process_circ_buffer, tempOutBuffer, o * 2);
        }
    }

    delete[] write_out;
    delete[] deal_in;
    delete[] deal_out;
    delete[] tempInBuffer;
    resample_close(handle);
}

// left channel resample
void leftResample()
{
#if 1
    std::cout << "left resample" << std::endl;
    void *handle = nullptr;
    volatile Float factor = 16000.0f / 48000;

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
    volatile Float factor = 16000.0f / 48000;

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

static int enable_hobot_voip_data_output()
{
    // enable VOIP data by default.
    // TODO: make it configurable, or on demand.
    int ret = 0;
    hrsc_paras_t voip_switch;
    int *voip_on = new int[1];
    *voip_on = 1;
    voip_switch.type = HRSC_PARAS_TYPE_VOIP_DATA_SWITCH;
    voip_switch.value = (void *)voip_on;
    ret = hrsc_set_paras(&voip_switch);
    voip_switch.value = NULL;
    delete[] voip_on;
    return ret;
}

/**
 * init sdk setting
 */
int initHobotSdk()
{
    std::cout << __FUNCTION__ << " " << __LINE__ << " ++++" << std::endl;
    hrsc_cfg.inputCfg.samplingRate = 16000;
    hrsc_cfg.inputCfg.channels = 4;
    hrsc_cfg.inputCfg.format = HRSC_AUDIO_FORMAT_PCM_16_BIT;
    hrsc_cfg.outputCfg.samplingRate = 16000;
    hrsc_cfg.outputCfg.channels = 2;
    hrsc_cfg.outputCfg.format = HRSC_AUDIO_FORMAT_PCM_16_BIT;

    hrsc_cfg.ref_ch_index = 2;
    hrsc_cfg.wakeup_prefix = 200;
    hrsc_cfg.wakeup_suffix = 200;
    hrsc_cfg.wait_asr_timeout = 3000;

    hrsc_cfg.vad_timeout = 400;
    hrsc_cfg.vad_switch = 0;
    hrsc_cfg.wakeup_data_switch = 1;
    hrsc_cfg.processed_data_switch = 1;

    hrsc_cfg.target_score = 0.3;
    hrsc_cfg.effect_mode = HRSC_EFFECT_MODE_ASR;
    hrsc_cfg.cfg_file_path = HRSC_CFG_FILE_PATH;
    hrsc_cfg.priv = (void *)&hrsc_cfg;
    std::cout << "cookie=" << hrsc_cfg.priv << std::endl;

    hrsc_cfg.log_callback = hobot_log_callback;
    hrsc_cfg.event_callback = hobot_event_callback;
    hrsc_cfg.wakeup_data_callback = hobot_wakeup_data_callback;
    hrsc_cfg.asr_data_callback = hobot_asr_data_callback;
    hrsc_cfg.voip_data_callback = hobot_voip_data_callback;
    hrsc_cfg.processed_data_callback = hobot_processed_data_callback;
    hrsc_cfg.secure_auth_callback = hobot_secure_auth_callback;

    hrsc_cfg.licenseStr = "";
    hrsc_cfg.activeBuff = new char[1024];

    std::cout << "call hrsc_init .... \n" << std::endl;
    if (0 != hrsc_init((const hrsc_effect_config_t *)&hrsc_cfg)) {
        std::cout << "hrsc_init error" << std::endl;
        return -1;
    }

#if defined(VOIP_SUPPORT)
    if (enable_hobot_voip_data_output() == 0) {
        std::cout << "hobot: enable voip data callback..." << std::endl;
    }
#endif

    std::cout << "end hrsc_init .... \n" << std::endl;
    if (0 != hrsc_start()) {
        hrsc_release();
        std::cout << "hrsc_start error" << std::endl;
        return -1;
    }

#if defined(A98_LUXMAN)
    // support compensation between mic and reference signal.
    hrsc_trigger_audio_compensate();
    std::cout << "enable hrsc audio compensate" << std::endl;
#endif

    std::cout << __FUNCTION__ << " " << __LINE__ << " ----" << std::endl;

#if defined(SECURITY_DEBUGPRINT)
    // FIXME: add debug
    if (voip_file != 0) {
        fclose(voip_file);
    }
    voip_file = fopen("/tmp/xiaowei_voip_debug.pcm", "wb+");

    if (mixer_file != 0) {
        fclose(mixer_file);
    }
    mixer_file = fopen("/tmp/xiaowei_mixer_debug.pcm", "wb+");
#endif

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
        std::cout << "malloc handle failed" << std::endl;
        return NULL;
    }
    memset(handle, 0, sizeof(Soft_prep_hobot_wrapper));
    handle->cap_dev = input_param.cap_dev;

    _ref_left_circ_buffer = CreateCircularBuffer(256 * 1024);
    FillZeroes(_ref_left_circ_buffer);

    _ref_right_circ_buffer = CreateCircularBuffer(256 * 1024);
    FillZeroes(_ref_right_circ_buffer);

    _left_circ_buffer = CreateCircularBuffer(256 * 1024);
    FillZeroes(_left_circ_buffer);

    _right_circ_buffer = CreateCircularBuffer(256 * 1024);
    FillZeroes(_right_circ_buffer);

    _ref_left_process_circ_buffer = CreateCircularBuffer(256 * 1024);
    FillZeroes(_ref_left_process_circ_buffer);

    _ref_right_process_circ_buffer = CreateCircularBuffer(256 * 1024);
    FillZeroes(_ref_right_process_circ_buffer);

    _left_process_circ_buffer = CreateCircularBuffer(256 * 1024);
    FillZeroes(_left_process_circ_buffer);

    _right_process_circ_buffer = CreateCircularBuffer(256 * 1024);
    FillZeroes(_right_process_circ_buffer);

    hobot_asr_data_cache = CreateCircularBuffer(256 * 1024);
    FillZeroes(hobot_asr_data_cache);

    hobot_voip_data_cache = CreateCircularBuffer(256 * 1024);
    FillZeroes(hobot_asr_data_cache);

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

    process_thread = std::thread(&processThread);
    record_thread = std::thread(&recordTest);
    refLeftResample_thread = std::thread(&refLeftResample);
    refRightResample_thread = std::thread(&refRightResample);
    leftResample_thread = std::thread(&leftResample);
    rightResample_thread = std::thread(&rightResample);

    process_data_mixer_thread = std::thread(&processDataMixerThread);

    std::cout << "hobot_version:" << hrsc_get_version_info() << std::endl;
    std::cout << "hobot_initialize return" << std::endl;
    return (void *)handle;
}

int hobot_deInitialize(void *handle)
{
    if (!stopRecord) {
        stopRecord = 1;
        record_thread.join();
        process_thread.join();
        refLeftResample_thread.join();
        refRightResample_thread.join();
        leftResample_thread.join();
        rightResample_thread.join();
    }
    hrsc_stop();
    hrsc_release();

    Soft_prep_hobot_wrapper *wrapper = (Soft_prep_hobot_wrapper *)handle;
    free(wrapper);

    return 0;
}

int hobot_reinit_cfg(void *handle)
{
    std::cout << "hobot_reinit_cfg" << std::endl;

    hrsc_stop();
    hrsc_release();
    std::cout << "call hrsc_init .... \n" << std::endl;
    if (0 != hrsc_init((const hrsc_effect_config_t *)&hrsc_cfg)) {
        std::cout << "hrsc_init error" << std::endl;
        return -1;
    }

    std::cout << "end hrsc_init .... \n" << std::endl;
    if (0 != hrsc_start()) {
        hrsc_release();
        std::cout << "hrsc_start error" << std::endl;
        return -1;
    }

#if defined(A98_LUXMAN)
    // support compensation between mic and reference signal.
    hrsc_trigger_audio_compensate();
    std::cout << "enable hrsc audio compensate" << std::endl;
#endif

    return 0;
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
            if (ch4_srcFile == NULL) {
                ch4_srcFile = fopen(buf, "wb+");
                if (ch4_srcFile == NULL)
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
                    if (ch4_srcFile)
                        fclose(ch4_srcFile);
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
        if (ch4_srcFile) {
            fclose(ch4_srcFile);
            ch4_srcFile = NULL;
        }
        if (ch2_outFile) {
            fclose(ch2_outFile);
            ch2_outFile = NULL;
        }
    }

    return 0;
}
}
