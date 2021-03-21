
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include <pthread.h>

#include "ccaptureGeneral.h"
#include "wm_util.h"
#include "xiaowei_linkplay_interface.h"

#ifdef VOIP_SUPPORT

extern WIIMU_CONTEXT *g_wiimu_shm;
static pthread_mutex_t mutex_asrvoip_op = PTHREAD_MUTEX_INITIALIZER;

int CCaptCosume_VOIP(char *buf, int size, void *priv);
int CCaptCosume_deinitVOIP(void *priv);
int CCaptCosume_initVOIP(void *priv);

CAPTURE_COMUSER Cap_Cosume_VOIP = {
    .id = AUDIO_ID_BT_HFP,
    .enable = 0,
    .cCapture_dataCosume = CCaptCosume_VOIP,
    .cCapture_init = CCaptCosume_initVOIP,
    .cCapture_deinit = CCaptCosume_deinitVOIP,
};

//#define DEBUG_DUMP_VOIP
#ifdef DEBUG_DUMP_VOIP
FILE *fpdump = NULL;
#endif

int CCaptCosume_VOIP(char *buf, int size, void *priv)
{
    int ret = 0;
    long pcmCount = 0;
    long pcmSize = 0;

    if (Cap_Cosume_VOIP.enable == 0) {
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

        pthread_mutex_lock(&mutex_asrvoip_op);
        if (Cap_Cosume_VOIP.enable) {
            xiaoweiWechatcallWriteRecordData(&buf[pcmCount], len);
        } else {
            ret = -1;
        }

        if (ret != 0) {
            pthread_mutex_unlock(&mutex_asrvoip_op);
            // usleep(30000);
            return size;
        }

#ifdef DEBUG_DUMP_ASR
        if (fpdump) {
            fwrite(&buf[pcmCount], 1, len, fpdump);
        }
#endif

        pthread_mutex_unlock(&mutex_asrvoip_op);
        pcmCount += (long)len;
        pcmSize -= (long)len;
        // usleep(5000);
    }

    // usleep(50000);

    return size;
}

int CCaptCosume_initVOIP(void *priv)
{
#ifdef DEBUG_DUMP_ASR
    system("rm -rf /tmp/cap_pcm.dump");
    sync();

    char filename[128];
    snprintf(filename, sizeof(filename), "/tmp/cap_pcm.dump", (int)tickSincePowerOn());
    fpdump = fopen(filename, "w+");
#endif
    return -1;
}

int CCaptCosume_deinitVOIP(void *priv)
{
#ifdef DEBUG_DUMP_ASR
    if (fpdump) {
        fclose(fpdump);
        fpdump = NULL;
    }
#endif
    return 0;
}

void ASR_Start_VOIP(void)
{
    printf("ASR_Start_VOIP ++++++ at %d\n", (int)tickSincePowerOn());
    // pthread_mutex_lock(&mutex_asrserver_op);
    CCaptRegister(&Cap_Cosume_VOIP);
    // pthread_mutex_unlock(&mutex_asrserver_op);
    printf("ASR_Start_VOIP ------ at %d\n", (int)tickSincePowerOn());
}

void ASR_Stop_VOIP(void)
{
    printf("ASR_Stop_VOIP ++++++ at %d\n", (int)tickSincePowerOn());
    pthread_mutex_lock(&mutex_asrvoip_op);
    Cap_Cosume_VOIP.enable = 0;
    pthread_mutex_unlock(&mutex_asrvoip_op);
    CCaptUnregister(&Cap_Cosume_VOIP);
    printf("ASR_Stop_VOIP ------ at %d\n", (int)tickSincePowerOn());
}

void ASR_Enable_VOIP(void)
{
    printf("ASR_Enable_VOIP ++++++ at %d\n", (int)tickSincePowerOn());
    pthread_mutex_lock(&mutex_asrvoip_op);
    CCaptCosume_deinitVOIP(&Cap_Cosume_VOIP);
    CCaptCosume_initVOIP(&Cap_Cosume_VOIP);
    Cap_Cosume_VOIP.enable = 1;
    pthread_mutex_unlock(&mutex_asrvoip_op);
    printf("ASR_Enable_VOIP--- at %d\n", (int)tickSincePowerOn());
}

void ASR_Disable_VOIP(void)
{
    printf("ASR_Disable_VOIP ++++++ at %d\n", (int)tickSincePowerOn());
    pthread_mutex_lock(&mutex_asrvoip_op);
    Cap_Cosume_VOIP.enable = 0;
    CCaptCosume_deinitVOIP(&Cap_Cosume_VOIP);
    pthread_mutex_unlock(&mutex_asrvoip_op);
    printf("ASR_Disable_VOIP ------ at %d\n", (int)tickSincePowerOn());
}

int ASR_Finished_VOIP(void) { return 0; }

#endif
