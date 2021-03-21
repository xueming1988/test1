#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include "pthread.h"

#include "ccaptureGeneral.h"
#include "wm_util.h"
#include "alexa_cmd.h"

#ifdef EN_AVS_COMMS
#include "alexa_comms.h"
#endif

#include "alexa.h"
#include "asr_state.h"

//#define DEBUG_DUMP_ASR
#ifdef DEBUG_DUMP_ASR
FILE *fpdump = NULL;
#endif

#ifndef VAD_SUPPORT
uint32_t record_bytes;
#else
extern uint32_t record_bytes;
#endif

extern WIIMU_CONTEXT *g_wiimu_shm;
static long long g_start_time = 0;
static long long g_need_skip = 0;
static long long g_recording_time_out = 8000;
static pthread_mutex_t g_mutex_alexa = PTHREAD_MUTEX_INITIALIZER;

int CCaptCosume_Asr_Alexa(char *buf, int size, void *priv);
int CCaptCosume_initAsr_Alexa(void *priv);
int CCaptCosume_deinitAsr_Alexa(void *priv);
void ASR_Start_Alexa(void);
void ASR_Stop_Alexa(void);

CAPTURE_COMUSER Cap_Cosume_ASR_Alexa = {
    .enable = 0,
    .cCapture_dataCosume = CCaptCosume_Asr_Alexa,
    .cCapture_init = CCaptCosume_initAsr_Alexa,
    .cCapture_deinit = CCaptCosume_deinitAsr_Alexa,
    .id = AUDIO_ID_DEFAULT, // If want to get the wake up word, set to 1
};

static void record_finished(int stop)
{
    if (Cap_Cosume_ASR_Alexa.enable) {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDTHINKING",
                                 strlen("GNOTIFY=MCULEDTHINKING"), NULL, NULL, 0);
        SetAudioInputProcessorObserverState(RECOGNIZING);
    }

    Cap_Cosume_ASR_Alexa.enable = 0;
    g_start_time = 0;
    record_bytes = 0;
    g_need_skip = 1;
    if (!g_wiimu_shm->asr_test_mode) {
        if (stop) {
            AlexaStopWriteData();
        } else {
            AlexaFinishWriteData();
        }
    }
    alexa_dump_info.record_end_ts = tickSincePowerOn();
    alexa_dump_info.vad_ts = alexa_dump_info.record_end_ts;
}

#include "vad.c"

int get_period_ms(void)
{
    if (far_field_judge(g_wiimu_shm))
        return PEROID_MS_FAR;
    else
        return PEROID_MS;
}

static int get_vad_start_ms(void)
{
    if (far_field_judge(g_wiimu_shm))
        return VAD_SKIP_START_MS_FAR_FIELD;
    else
        return VAD_SKIP_START_MS;
}

static int get_upload_skip_ms(void)
{
    if (far_field_judge(g_wiimu_shm)) {
        if ((g_wiimu_shm->asr_have_prompt == 0) ||
            ASR_IS_WAKEUP_WORD(alexa_dump_info.asr_test_flag)) {
            return 0;
        } else {
            return 300;
        }
    } else {
        return 200; // 400;
    }
}

static int get_min_samples(void)
{
    if (far_field_judge(g_wiimu_shm)) {
        return VOICE_COUNT_MIN * CHUNK_TIME_MS + PEROID_MS + 300; // get_upload_skip_ms();
    } else {
        return VOICE_COUNT_MIN * CHUNK_TIME_MS + PEROID_MS;
    }
}

static int get_voice_min_ms(void)
{
    if (far_field_judge(g_wiimu_shm)) {
        if ((g_wiimu_shm->asr_have_prompt == 0) ||
            ASR_IS_WAKEUP_WORD(alexa_dump_info.asr_test_flag)) {
            return 600;
        } else {
            return VOICE_COUNT_MIN * CHUNK_TIME_MS + get_upload_skip_ms();
        }
    } else {
        return 0;
    }
}

static int get_silece_val(void)
{
    if (far_field_judge(g_wiimu_shm)) {
        return SILENCE_THRESOLD_FAR;
    }

    return SILENCE_THRESOLD;
}

int32_t self_vad_process_run(uint8_t *data, int32_t len, int ignore_invalid_start_samples)
{
    if (g_self_vad_init == 0) {
        printf("[%s:%d] self_process doesn't inited\n", __FUNCTION__, __LINE__);
        return -1;
    }

    int32_t ret = 0;
    len = len / 2;
    ret = self_vad_process((int16_t *)data, &len, ignore_invalid_start_samples, get_min_samples(),
                           get_voice_min_ms());
    if (ret > 0) {
        self_vad_uninit();
        record_finished(0);
        SocketClientReadWriteMsg("/tmp/RequestASRTTS", "VAD_FINISH", strlen("VAD_FINISH"), NULL,
                                 NULL, 0);
        // wiimu_log(1,0,0,0, "[TICK]====== vad finished at %lld ====== ", tickSincePowerOn());
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "[TICK]====== vad finished at %lld ======\n",
                    tickSincePowerOn());
        return 0;
    }

    return ret;
}

/* captcosume callback, the record data */
int CCaptCosume_Asr_Alexa(char *buf, int size, void *priv)
{
    int enable = 0;
    long long start_time = 0;
    long long check_time = 0;
    int vad_offset = 0;
    long long cur_time = tickSincePowerOn();
    int write_size = size;
    char *write_buf = buf;
    int need_skip = 0;

    pthread_mutex_lock(&g_mutex_alexa);
    enable = Cap_Cosume_ASR_Alexa.enable;
    start_time = g_start_time;
    need_skip = g_need_skip;

    if (enable == 0 || start_time == 0) {
        pthread_mutex_unlock(&g_mutex_alexa);
        return size;
    }

#ifdef DEBUG_DUMP_ASR
    if (fpdump) {
        fwrite(buf, 1, size, fpdump);
        // fflush(fpdump);
    }
#endif

    if (record_bytes == 0) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "[TICK]====== first write record data at %lld ====== \n",
                    tickSincePowerOn());
    }

    record_bytes += size;
    if ((CLOSE_TALK == AlexaProfileType()) || !ASR_IS_SERVER_VAD(alexa_dump_info.asr_test_flag)) {
        vad_offset = MS_TO_BYTES(BYTES_TO_MS(record_bytes) - get_vad_start_ms());
        // self_vad_init(get_silece_val());
        // do not upload the data before the end of start_prompt
        if (far_field_judge(g_wiimu_shm) &&
            BYTES_TO_MS(record_bytes - size) <= get_upload_skip_ms()) {
            write_size = 0;
            if (BYTES_TO_MS(record_bytes) > get_upload_skip_ms()) {
                int tmp_size = record_bytes - MS_TO_BYTES(get_upload_skip_ms());
                write_buf = write_buf + size - tmp_size;
                write_size = tmp_size;
            }
        } else if (!(g_wiimu_shm->priv_cap2 & (1 << CAP2_FUNC_DISABLE_PLAY_CAP_SIM)) &&
                   !(far_field_judge(g_wiimu_shm)) &&
                   BYTES_TO_MS(record_bytes) <= get_upload_skip_ms()) {
            printf("ignore record_bytes=%d(%d ms)\n", record_bytes, BYTES_TO_MS(record_bytes));
            write_size = 0;
        }
    }

    if (!g_wiimu_shm->asr_test_mode && write_size > 0 && write_buf) {
        AlexaWriteRecordData(write_buf, write_size);
    }

    if ((CLOSE_TALK == AlexaProfileType()) || !ASR_IS_SERVER_VAD(alexa_dump_info.asr_test_flag)) {
        if (g_wiimu_shm->priv_cap2 & (1 << CAP2_FUNC_DISABLE_PLAY_CAP_SIM)) {
            self_vad_process_run((uint8_t *)buf, size, 1);
        } else {
            // if (start_time > 0 && (start_time + VAD_SKIP_START_MS < cur_time)) {
            if (BYTES_TO_MS(record_bytes) >= get_vad_start_ms()) {
                if ((vad_offset < size) && need_skip) {
                    self_vad_process_run((uint8_t *)buf + vad_offset, size - vad_offset, 0);
                } else {
                    self_vad_process_run((uint8_t *)buf, size, 0);
                }
            }
        }
    }

    if (CLOSE_TALK == AlexaProfileType()) {
        if (!ASR_IS_SERVER_VAD(alexa_dump_info.asr_test_flag)) {
            check_time = start_time + get_voice_timeout_ms();
        } else {
            check_time = start_time + 6000;
        }
    } else {
        check_time = start_time + g_recording_time_out;
    }

    cur_time = tickSincePowerOn();
    if (start_time > 0 && (check_time <= cur_time)) {
        if (!ASR_IS_SERVER_VAD(alexa_dump_info.asr_test_flag)) {
            self_vad_uninit();
            cur_time = tickSincePowerOn();
            printf("[%s:%d] send RequestASRTTS talk stop timeout(voice_detect %d) cur_time is %lld "
                   "delta=%lld ms\n",
                   __FUNCTION__, __LINE__, get_voice_count(), cur_time, (cur_time - start_time));
        }
        record_finished(0);
        g_wiimu_shm->times_vad_time_out++;
        SocketClientReadWriteMsg("/tmp/RequestASRTTS", "VAD_TIMEOUT", strlen("VAD_TIMEOUT"), NULL,
                                 NULL, 0);
        ALEXA_PRINT(ALEXA_DEBUG_ERR,
                    "[TICK]====== vad timeout at  %lld start_time=%lld check_time=%lld ======\n",
                    tickSincePowerOn(), start_time, check_time);
    }
    pthread_mutex_unlock(&g_mutex_alexa);

    return size;
}

int CCaptCosume_initAsr_Alexa(void *priv)
{
#ifdef DEBUG_DUMP_ASR
    // system("rm -rf "ALEXA_RECORD_FILE);
    if (alexa_dump_info.record_to_sd && strlen(g_wiimu_shm->mmc_path) > 0) {
        struct tm *pTm = gmtime(&alexa_dump_info.last_dialog_ts);
        char timestr[20];
        char record_file[128];

        snprintf(timestr, sizeof(timestr), ALEXA_DATE_FMT, pTm->tm_year + 1900, pTm->tm_mon + 1,
                 pTm->tm_mday, pTm->tm_hour, pTm->tm_min, pTm->tm_sec);

        snprintf(record_file, sizeof(record_file), ALEXA_RECORD_FMT, g_wiimu_shm->mmc_path,
                 timestr);
        fpdump = fopen(record_file, "wb+");
        system("rm -rf " ALEXA_RECORD_FILE);
    } else {
        fpdump = fopen(ALEXA_RECORD_FILE, "wb+");
    }
#endif

    return 0;
}

int CCaptCosume_deinitAsr_Alexa(void *priv)
{
#ifdef DEBUG_DUMP_ASR
    if (fpdump) {
        fclose(fpdump);
        fpdump = NULL;
    }
#endif
    return 0;
}

void ASR_Start_Alexa(void)
{
    wiimu_log(1, 0, 0, 0, "ASR_Start_Amazon+++");
    g_start_time = tickSincePowerOn();
    record_bytes = 0;
    g_need_skip = 1;
    CCaptRegister(&Cap_Cosume_ASR_Alexa);
    wiimu_log(1, 0, 0, 0, "ASR_Start_Amazon---");
}

void ASR_Stop_Alexa(void)
{
    wiimu_log(1, 0, 0, 0, "ASR_Stop_Alexa+++");
    Cap_Cosume_ASR_Alexa.enable = 0;
    g_need_skip = 1;
    CCaptUnregister(&Cap_Cosume_ASR_Alexa);
    wiimu_log(1, 0, 0, 0, "ASR_Stop_Alexa---");
}

void ASR_Enable_Alexa(int talk_from)
{
    wiimu_log(1, 0, 0, 0, "ASR_Enable_Alexa+++");

    SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDLISTENING",
                             strlen("GNOTIFY=MCULEDLISTENING"), NULL, NULL, 0);
    SetAudioInputProcessorObserverState(EXPECTING_SPEECH);
#ifdef EN_AVS_COMMS
    avs_comms_notify(CALL_MUTE_CTRL_1);
#endif

    pthread_mutex_lock(&g_mutex_alexa);
    if (talk_from != 3) {
        CCaptCosume_deinitAsr_Alexa(&Cap_Cosume_ASR_Alexa);
        CCaptCosume_initAsr_Alexa(&Cap_Cosume_ASR_Alexa);
        CCaptFlush();
        if ((CLOSE_TALK == AlexaProfileType()) ||
            !ASR_IS_SERVER_VAD(alexa_dump_info.asr_test_flag)) {
            self_vad_uninit();
            self_vad_init(get_silece_val());
        }
        Cap_Cosume_ASR_Alexa.enable = 1;
        record_bytes = 0;
        g_start_time = tickSincePowerOn();
        if (CLOSE_TALK != AlexaProfileType()) {
            if (ASR_IS_WAKEUP_WORD(alexa_dump_info.asr_test_flag)) {
                Cap_Cosume_ASR_Alexa.id = AUDIO_ID_ASR_FAR_FIELD;
            } else {
                Cap_Cosume_ASR_Alexa.id = AUDIO_ID_DEFAULT;
            }
        }
    }
    pthread_mutex_unlock(&g_mutex_alexa);
    wiimu_log(1, 0, 0, 0, "ASR_Enable_Alexa---");
    ALEXA_PRINT(ALEXA_DEBUG_ERR, "[TICK]====== enable record data at %lld ====== \n",
                tickSincePowerOn());
}

void ASR_Disable_Alexa(void)
{
    wiimu_log(1, 0, 0, 0, "ASR_Disable_Alexa+++");

    pthread_mutex_lock(&g_mutex_alexa);
    g_start_time = 0;
    record_bytes = 0;
    g_need_skip = 1;
    if (!g_wiimu_shm->asr_test_mode && Cap_Cosume_ASR_Alexa.enable == 1) {
        AlexaStopWriteData();
    }
    Cap_Cosume_ASR_Alexa.enable = 0;
    CCaptCosume_deinitAsr_Alexa(&Cap_Cosume_ASR_Alexa);
    pthread_mutex_unlock(&g_mutex_alexa);

    wiimu_log(1, 0, 0, 0, "ASR_Disable_Alexa---");
}

void ASR_Finished_Alexa(void)
{
    wiimu_log(1, 0, 0, 0, "ASR_Finished_Alexa+++");

    pthread_mutex_lock(&g_mutex_alexa);
    if (1 == Cap_Cosume_ASR_Alexa.enable) {
        record_finished(1);
    }
    pthread_mutex_unlock(&g_mutex_alexa);

    wiimu_log(1, 0, 0, 0, "ASR_Finished_Alexa---");
}

void ASR_SetRecording_TimeOut(int time_out)
{
    wiimu_log(1, 0, 0, 0, "ASR_SetRecording_TimeOut time_out = %d +++", time_out);

    pthread_mutex_lock(&g_mutex_alexa);
    g_recording_time_out = (long long)time_out;
    pthread_mutex_unlock(&g_mutex_alexa);

    wiimu_log(1, 0, 0, 0, "ASR_SetRecording_TimeOut g_recording_time_out = %lld---",
              g_recording_time_out);
}

long long ASR_GetRecording_TimeOut(void)
{
    long long time_out = 0;
    wiimu_log(1, 0, 0, 0, "ASR_GetRecording_TimeOut +++");

    pthread_mutex_lock(&g_mutex_alexa);
    if (g_recording_time_out != 8000 && g_recording_time_out != 40000) {
        g_recording_time_out = 8000;
    }
    time_out = g_recording_time_out;
    pthread_mutex_unlock(&g_mutex_alexa);

    wiimu_log(1, 0, 0, 0, "ASR_GetRecording_TimeOut time_out = %lld---", time_out);
    return time_out;
}
