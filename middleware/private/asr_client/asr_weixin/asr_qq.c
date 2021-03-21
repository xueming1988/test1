
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include <pthread.h>

#include "ccaptureGeneral.h"
#include "wm_util.h"
#include "caiman.h"
#include "xiaowei_interface.h"

extern WIIMU_CONTEXT *g_wiimu_shm;

static pthread_mutex_t mutex_asrserver_op = PTHREAD_MUTEX_INITIALIZER;

int CCaptCosume_Asr(char *buf, int size, void *priv);
int CCaptCosume_deinitAsr(void *priv);
int CCaptCosume_initAsr(void *priv);

CAPTURE_COMUSER Cap_Cosume_ASR = {
    .id = 1,
    .enable = 0,
    .cCapture_dataCosume = CCaptCosume_Asr,
    .cCapture_init = CCaptCosume_initAsr,
    .cCapture_deinit = CCaptCosume_deinitAsr,
};

#define DEBUG_DUMP_ASR
#ifdef DEBUG_DUMP_ASR
FILE *fpdump = NULL;
#endif

extern int g_record_asr_input;
int CCaptCosume_Asr(char *buf, int size, void *priv)
{
    int ret = 0;
    long pcmCount = 0;
    long pcmSize = 0;

    if (Cap_Cosume_ASR.enable == 0) {
        // usleep(30000);
        return size;
    }
    if (g_wiimu_shm->privacy_mode) {
        return size;
    }

    pcmSize = size;

    while (pcmSize) {
        unsigned int len = 4096;
        if (pcmSize < 4096) {
            len = pcmSize;
        }

        pthread_mutex_lock(&mutex_asrserver_op);
        if (Cap_Cosume_ASR.enable) {
            if (g_record_asr_input) {
                FILE *fdump = fopen("/tmp/dump_asr.pcm", "ab+");
                if (fdump) {
                    fwrite((buf + pcmCount), len, 1, fdump);
                    fclose(fdump);
                }
            }

            xiaoweiWriteRecordData(&buf[pcmCount], len);
        } else {
            ret = -1;
        }

        if (ret != 0) {
            pthread_mutex_unlock(&mutex_asrserver_op);
            // usleep(30000);
            return size;
        }

#ifdef DEBUG_DUMP_ASR
        if (fpdump) {
            fwrite(&buf[pcmCount], 1, len, fpdump);
        }
#endif

        pthread_mutex_unlock(&mutex_asrserver_op);
        pcmCount += (long)len;
        pcmSize -= (long)len;
        // usleep(5000);
    }

    // usleep(50000);

    return size;
}

int CCaptCosume_initAsr(void *priv)
{
#ifdef DEBUG_DUMP_ASR
    system("rm -rf /tmp/cap_pcm.dump");
    system("rm -rf /tmp/asr.result");
    sync();

    char filename[128];
    snprintf(filename, sizeof(filename), "/tmp/cap_pcm.dump", (int)tickSincePowerOn());
    fpdump = fopen(filename, "w+");
#endif
    return -1;
}

int CCaptCosume_deinitAsr(void *priv)
{
#ifdef DEBUG_DUMP_ASR
    if (fpdump) {
        fclose(fpdump);
        fpdump = NULL;
    }
#endif
    return 0;
}

void ASR_Start_QQ(void)
{
    printf("ASR_Start_QQ ++++++ at %d\n", (int)tickSincePowerOn());
    // pthread_mutex_lock(&mutex_asrserver_op);
    CCaptRegister(&Cap_Cosume_ASR);
    // pthread_mutex_unlock(&mutex_asrserver_op);
    printf("ASR_Start_QQ ------ at %d\n", (int)tickSincePowerOn());
}

void ASR_Stop_QQ(void)
{
    printf("ASR_Stop_QQ ++++++ at %d\n", (int)tickSincePowerOn());
    pthread_mutex_lock(&mutex_asrserver_op);
    Cap_Cosume_ASR.enable = 0;
    pthread_mutex_unlock(&mutex_asrserver_op);
    CCaptUnregister(&Cap_Cosume_ASR);
    printf("ASR_Stop_QQ ------ at %d\n", (int)tickSincePowerOn());
}

void ASR_Enable_QQ(void)
{
    printf("ASR_Enable_QQ ++++++ at %d\n", (int)tickSincePowerOn());
    CCaptFlush();
    pthread_mutex_lock(&mutex_asrserver_op);
    CCaptCosume_deinitAsr(&Cap_Cosume_ASR);
    CCaptCosume_initAsr(&Cap_Cosume_ASR);
    Cap_Cosume_ASR.enable = 1;
    pthread_mutex_unlock(&mutex_asrserver_op);
    printf("ASR_Enable_QQ--- at %d\n", (int)tickSincePowerOn());
}

void ASR_Disable_QQ(void)
{
    printf("ASR_Disable_QQ ++++++ at %d\n", (int)tickSincePowerOn());
    pthread_mutex_lock(&mutex_asrserver_op);
    Cap_Cosume_ASR.enable = 0;
    CCaptCosume_deinitAsr(&Cap_Cosume_ASR);
    pthread_mutex_unlock(&mutex_asrserver_op);
    printf("ASR_Disable_QQ ------ at %d\n", (int)tickSincePowerOn());
}

int ASR_Finished_QQ(void) { return 0; }
