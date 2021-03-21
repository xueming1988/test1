#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include "pthread.h"

#include "ccaptureGeneral.h"

#include "asr_session.h"
#include "asr_light_effects.h"
#include "common_flags.h"

//#define DEBUG_DUMP_ASR
#ifdef DEBUG_DUMP_ASR
FILE *fpdump = NULL;
#define ASR_RECORD_FILE "/tmp/web/asr_record.pcm"
#endif

extern int g_asr_req_from;

uint32_t record_bytes = 0;
static long long g_start_time = 0;
static pthread_mutex_t g_mutex_record = PTHREAD_MUTEX_INITIALIZER;

int CCaptCosume_Asr_Record(char *buf, int size, void *priv);
int CCaptCosume_initAsr_Record(void *priv);
int CCaptCosume_deinitAsr_Record(void *priv);
void ASR_Start_Record(void);
void ASR_Stop_Record(void);

CAPTURE_COMUSER Cap_Cosume_ASR_Record = {
    .enable = 0,
    .cCapture_dataCosume = CCaptCosume_Asr_Record,
    .cCapture_init = CCaptCosume_initAsr_Record,
    .cCapture_deinit = CCaptCosume_deinitAsr_Record,
    .id = AUDIO_ID_DEFAULT, // If want to get the wake up word, set to 1
};

static void record_finished(int stop)
{
    if (Cap_Cosume_ASR_Record.enable) {
        if (g_asr_req_from == 0 || g_asr_req_from == 1) {
            asr_light_effects_set(e_effect_thinking);
        }
    }

    Cap_Cosume_ASR_Record.enable = 0;
    g_start_time = 0;
    record_bytes = 0;
#if defined(LGUPLUS_IPTV_ENABLE)
    if (g_asr_req_from == 0 || g_asr_req_from == 1) {
        if (stop) {
            asr_session_record_buffer_stop();
        } else {
            asr_session_record_buffer_end();
        }
    }
#else
    if (stop) {
        asr_session_record_buffer_stop();
    } else {
        asr_session_record_buffer_end();
    }
#endif
}

/* captcosume callback, the record data */
int CCaptCosume_Asr_Record(char *buf, int size, void *priv)
{
    int enable = 0;
    long long start_time = 0;
    long long check_time = 0;
    int vad_offset = 0;
    long long cur_time = tickSincePowerOn();
    int write_size = size;
    char *write_buf = buf;

    pthread_mutex_lock(&g_mutex_record);
    enable = Cap_Cosume_ASR_Record.enable;
    start_time = g_start_time;

    if (enable == 0 || start_time == 0) {
        pthread_mutex_unlock(&g_mutex_record);
        return size;
    }

#ifdef DEBUG_DUMP_ASR
    if (fpdump) {
        fwrite(buf, 1, size, fpdump);
        // fflush(fpdump);
    }
#endif

    if (record_bytes == 0) {
        wiimu_log(1, 0, 0, 0, "[TICK]====== first write record data at %lld\n", tickSincePowerOn());
    }

    record_bytes += size;
    if (write_size > 0 && write_buf) {
#if defined(LGUPLUS_IPTV_ENABLE)
        if (g_asr_req_from == 0 || g_asr_req_from == 1)
            asr_session_record_buffer_write(write_buf, write_size);
        else if (g_asr_req_from == 4)
            iptv_pcm_write(write_buf, write_size);
#else
        asr_session_record_buffer_write(write_buf, write_size);
#endif
    }

#if MAX_RECORD_TIME_IN_MS
    check_time = start_time + ((MAX_RECORD_TIME_IN_MS < 10000) ? 10000 : MAX_RECORD_TIME_IN_MS);
#else
    check_time = start_time + 10000;
#endif

    cur_time = tickSincePowerOn();
    if (start_time > 0 && (check_time <= cur_time)) {
        record_finished(0);
#if defined(LGUPLUS_IPTV_ENABLE)
        if (g_asr_req_from == 0 || g_asr_req_from == 1) {
            update_vad_timeout_count();
        }
#else
        update_vad_timeout_count();
#endif
        SocketClientReadWriteMsg("/tmp/RequestASRTTS", "VAD_TIMEOUT", strlen("VAD_TIMEOUT"), NULL,
                                 NULL, 0);
        wiimu_log(1, 0, 0, 0, "[TICK]====== vad timeout at  %lld start_time=%lld check_time=%lld\n",
                  tickSincePowerOn(), start_time, check_time);
    }
    pthread_mutex_unlock(&g_mutex_record);

    return size;
}

int CCaptCosume_initAsr_Record(void *priv)
{
#ifdef DEBUG_DUMP_ASR
    fpdump = fopen(ASR_RECORD_FILE, "wb+");
#endif

    return 0;
}

int CCaptCosume_deinitAsr_Record(void *priv)
{
#ifdef DEBUG_DUMP_ASR
    if (fpdump) {
        fclose(fpdump);
        fpdump = NULL;
    }
#endif
    return 0;
}

void ASR_Start_Record(void)
{
    wiimu_log(1, 0, 0, 0, "ASR_Start_Record+++");
    g_start_time = tickSincePowerOn();
    record_bytes = 0;
    CCaptRegister(&Cap_Cosume_ASR_Record);
    wiimu_log(1, 0, 0, 0, "ASR_Start_Record---");
}

void ASR_Stop_Record(void)
{
    wiimu_log(1, 0, 0, 0, "ASR_Stop_Record+++");
    Cap_Cosume_ASR_Record.enable = 0;
    CCaptUnregister(&Cap_Cosume_ASR_Record);
    wiimu_log(1, 0, 0, 0, "ASR_Stop_Record---");
}

void ASR_Enable_Record(int talk_from)
{
    wiimu_log(1, 0, 0, 0, "ASR_Enable_Record+++");
    asr_light_effects_set(e_effect_listening);
    pthread_mutex_lock(&g_mutex_record);
    if (talk_from != 3) {
        CCaptCosume_deinitAsr_Record(&Cap_Cosume_ASR_Record);
        CCaptCosume_initAsr_Record(&Cap_Cosume_ASR_Record);
        CCaptFlush();
        Cap_Cosume_ASR_Record.enable = 1;
        record_bytes = 0;
        g_start_time = tickSincePowerOn();
        Cap_Cosume_ASR_Record.id = AUDIO_ID_ASR_FAR_FIELD;
    }
    pthread_mutex_unlock(&g_mutex_record);
    wiimu_log(1, 0, 0, 0, "ASR_Enable_Record---");
}

void ASR_Disable_Record(void)
{
    wiimu_log(1, 0, 0, 0, "ASR_Disable_Record+++");

    pthread_mutex_lock(&g_mutex_record);
    g_start_time = 0;
    record_bytes = 0;
    if (Cap_Cosume_ASR_Record.enable == 1) {
        asr_session_record_buffer_stop();
    }
    Cap_Cosume_ASR_Record.enable = 0;
    CCaptCosume_deinitAsr_Record(&Cap_Cosume_ASR_Record);
    pthread_mutex_unlock(&g_mutex_record);

    wiimu_log(1, 0, 0, 0, "ASR_Disable_Record---");
}

void ASR_Finished_Record(void)
{
    wiimu_log(1, 0, 0, 0, "ASR_Finished_Record+++");

    pthread_mutex_lock(&g_mutex_record);
    if (1 == Cap_Cosume_ASR_Record.enable) {
        record_finished(1);
    }
    pthread_mutex_unlock(&g_mutex_record);

    wiimu_log(1, 0, 0, 0, "ASR_Finished_Record---");
}
