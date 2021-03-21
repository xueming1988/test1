#include <stdio.h>
#include <stdint.h>
#include "pthread.h"
#include <signal.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "wm_util.h"
#include "ccaptureGeneral.h"
#include "hobot_xiaowei_wwe.h"

extern int CCaptCosume_Asr_Weixin(char *buf, int size, void *priv);
extern int CCaptCosume_initAsr_Weixin(void *priv);
extern int CCaptCosume_deinitAsr_Weixin(void *priv);
extern pthread_mutex_t mutex_YDserver_op;

CAPTURE_COMUSER Cap_Cosume_ASR_Weixin = {
    .enable = 0,
    .cCapture_dataCosume = CCaptCosume_Asr_Weixin,
    .cCapture_init = CCaptCosume_initAsr_Weixin,
    .cCapture_deinit = CCaptCosume_deinitAsr_Weixin,
    .id = AUDIO_ID_WWE,
};

void ASR_Start_Weixin(void)
{
    printf("%s+++ at %d\n", __FUNCTION__, (int)tickSincePowerOn());
    CCaptCosume_initAsr_Weixin(&Cap_Cosume_ASR_Weixin);
    CCaptRegister(&Cap_Cosume_ASR_Weixin);
    printf("%s--- at %d\n", __FUNCTION__, (int)tickSincePowerOn());
}

void ASR_Stop_Weixin(void)
{
    printf("%s+++ at %d\n", __FUNCTION__, (int)tickSincePowerOn());
    Cap_Cosume_ASR_Weixin.enable = 0;
    CCaptUnregister(&Cap_Cosume_ASR_Weixin);
    CCaptCosume_deinitAsr_Weixin(&Cap_Cosume_ASR_Weixin);
    printf("%s--- at %d\n", __FUNCTION__, (int)tickSincePowerOn());
}

void ASR_Enable_Weixin(void)
{
    printf("%s+++ at %d\n", __FUNCTION__, (int)tickSincePowerOn());
    pthread_mutex_lock(&mutex_YDserver_op);
    Cap_Cosume_ASR_Weixin.enable = 1;
    pthread_mutex_unlock(&mutex_YDserver_op);
    printf("%s--- at %d\n", __FUNCTION__, (int)tickSincePowerOn());
}

void ASR_Disable_Weixin(void)
{
    printf("%s+++ at %d\n", __FUNCTION__, (int)tickSincePowerOn());
    pthread_mutex_lock(&mutex_YDserver_op);
    Cap_Cosume_ASR_Weixin.enable = 0;
    pthread_mutex_unlock(&mutex_YDserver_op);
    printf("%s--- at %d\n", __FUNCTION__, (int)tickSincePowerOn());
}

int get_asr_Weixin_enable(void) { return Cap_Cosume_ASR_Weixin.enable; }
