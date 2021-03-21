#include <stdio.h>
#include <stdint.h>
#include "pthread.h"
#include <signal.h>
#include <sys/time.h>
#include "wm_util.h"
#include "ccaptureGeneral.h"

#if defined(YD_AMAZON_MODULE)
#if defined(AMAZON_DOUBLE_TRIGGER)

extern WIIMU_CONTEXT *g_wiimu_shm;
extern pthread_mutex_t mutex_YDserver_op;

extern int CCaptCosume_Asr_Amazon_double(char *buf, int size, void *priv);
extern int CCaptCosume_deinitAsr_Amazon_double(void *priv);
extern int CCaptCosume_initAsr_Amazon_double(void *priv);

CAPTURE_COMUSER Cap_Cosume_ASR_Amazon_double = {
    .enable = 0,
    .cCapture_dataCosume = CCaptCosume_Asr_Amazon_double,
    .cCapture_init = CCaptCosume_initAsr_Amazon_double,
    .cCapture_deinit = CCaptCosume_deinitAsr_Amazon_double,
    .id = AUDIO_ID_WWE_DUAL_CH,
};

void ASR_Start_Amazon_double(void)
{
    printf("%s+++ at %d\n", __FUNCTION__, (int)tickSincePowerOn());
    CCaptCosume_initAsr_Amazon_double(&Cap_Cosume_ASR_Amazon_double);
    CCaptRegister(&Cap_Cosume_ASR_Amazon_double);
    printf("%s--- at %d\n", __FUNCTION__, (int)tickSincePowerOn());
}

void ASR_Stop_Amazon_double(void)
{
    printf("%s+++ at %d\n", __FUNCTION__, (int)tickSincePowerOn());
    Cap_Cosume_ASR_Amazon_double.enable = 0;
    CCaptUnregister(&Cap_Cosume_ASR_Amazon_double);
    CCaptCosume_deinitAsr_Amazon_double(&Cap_Cosume_ASR_Amazon_double);
    printf("%s--- at %d\n", __FUNCTION__, (int)tickSincePowerOn());
}

void ASR_Enable_Amazon_double(void)
{
    printf("%s+++ at %d\n", __FUNCTION__, (int)tickSincePowerOn());
    pthread_mutex_lock(&mutex_YDserver_op);
    Cap_Cosume_ASR_Amazon_double.enable = 1;
    pthread_mutex_unlock(&mutex_YDserver_op);
    printf("%s--- at %d\n", __FUNCTION__, (int)tickSincePowerOn());
}

void ASR_Disable_Amazon_double(void)
{
    printf("%s+++ at %d\n", __FUNCTION__, (int)tickSincePowerOn());
    pthread_mutex_lock(&mutex_YDserver_op);
    Cap_Cosume_ASR_Amazon_double.enable = 0;
    pthread_mutex_unlock(&mutex_YDserver_op);
    CCaptCosume_deinitAsr_Amazon(&Cap_Cosume_ASR_Amazon_double);
    printf("%s--- at %d\n", __FUNCTION__, (int)tickSincePowerOn());
}

int get_asr_amazon_enable_double(void) { return Cap_Cosume_ASR_Amazon_double.enable; }

#endif
#endif
