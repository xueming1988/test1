#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include "pthread.h"
#include "ccaptureGeneral.h"
#include "wm_util.h"

#if defined(YD_BAIDU_MODULE)
#include "ytv_asr_tiny_interface.h"

YTV_ASR_CONTAINER_POINTER pContainer = NULL;
YTV_ASR_ENGINE_POINTER pEngineInstance = NULL;

#define RECOGNIZER_FRAME_SIZE 320
#define ENGINE_BUFF_SIZE (100 * 1024)
static char pMemoryBufferForEngine[ENGINE_BUFF_SIZE];
static pthread_mutex_t mutex_YDserver_op = PTHREAD_MUTEX_INITIALIZER;

extern int asr_waked;
extern WIIMU_CONTEXT *g_wiimu_shm;

int CCaptCosume_YD_baidu(char *buf, int size, void *priv);
int CCaptCosume_initYD_baidu(void *priv);
int CCaptCosume_deinitYD_baidu(void *priv);

CAPTURE_COMUSER Cap_Cosume_AIYD_baidu = {
    .enable = 0,
    .cCapture_dataCosume = CCaptCosume_YD_baidu,
    .cCapture_init = CCaptCosume_initYD_baidu,
    .cCapture_deinit = CCaptCosume_deinitYD_baidu,
    .id = 2,
};

int CCaptCosume_YD_baidu(char *buf, int size, void *priv)
{
    long pcmCount = 0;
    long pcmSize = 0;
    char cFlag_VAD = 0;
    char *strResult;
    int N_MAX = 10, pOriginalWordIndex[10], nResultNumber;

    if (!Cap_Cosume_AIYD_baidu.enable || !pContainer || !pEngineInstance) {
        // usleep(15000);
        return size;
    }

    pcmSize = size;

    while (pContainer && pEngineInstance && pcmSize) {
        unsigned int len = RECOGNIZER_FRAME_SIZE;
        if (pcmSize < len)
            len = pcmSize;

        // if (pcmSize < len){
        //	pthread_mutex_lock(&mutex_YDserver_op);
        //	ytv_asr_tiny_run_one_step(pEngineInstance);
        //	pthread_mutex_unlock(&mutex_YDserver_op);
        //	return 0;
        //}else
        {
            pthread_mutex_lock(&mutex_YDserver_op);
            ytv_asr_tiny_run_one_step(pEngineInstance);
            ytv_asr_tiny_put_pcm_data(pEngineInstance, (short *)(buf + pcmCount), len / 2,
                                      &cFlag_VAD);

            if ('S' == cFlag_VAD) {
                printf("YD Baidu VAD Start!\n");
            }

            if ('Y' == cFlag_VAD) {
                printf("YD Baidu VAD_end!\n");
                ytv_asr_tiny_notify_data_end(pEngineInstance);

                ytv_asr_tiny_get_result(pEngineInstance, N_MAX, &nResultNumber, pOriginalWordIndex);
                if (nResultNumber > 0 && g_wiimu_shm->asr_ongoing == 0) {
                    strResult =
                        ytv_asr_tiny_get_one_command_gbk(pEngineInstance, pOriginalWordIndex[0]);
                    printf("YD_Baidu Recogintion_result:%s \r\n", strResult);
                    SocketClientReadWriteMsg("/tmp/RequestASRTTS", "talkstart:1",
                                             strlen("talkstart:1"), NULL, NULL, 0);
#if !defined(BAIDU_DUMI) && !defined(TENCENT)
                    asr_waked = 1;
#endif
                } else {
                    printf("YD_Baidu Recogintion_no_results!\n");
                }

                ytv_asr_tiny_reset_engine(pEngineInstance);
            }

            pcmCount += (long)len;
            pcmSize -= (long)len;
            pthread_mutex_unlock(&mutex_YDserver_op);
        }
    }

    return size;
}

int CCaptCosume_initYD_baidu(void *priv)
{
    int ret = 0;
    char strArgOne[128], strArgTwo[128];
    int TARGET_LANGUAGE = 0, YTV_ASR_TINY_LANG_ID_MANDARIN_IN = 1;

    printf("AIYD baidu init ++\n");
    pthread_mutex_lock(&mutex_YDserver_op);

    if (pEngineInstance) {
        printf("ytv_asr_tiny_free_engine...\n");
        ytv_asr_tiny_free_engine(pEngineInstance);
        pEngineInstance = NULL;
    }

    if (pContainer) {
        printf("ytv_asr_tiny_free_container...\n");
        ytv_asr_tiny_free_container(pContainer);
        pContainer = NULL;
    }

    // create asr engine
    strncpy(strArgOne, "reserved_one", sizeof(strArgOne) - 1);
    strncpy(strArgTwo, "reserved_two", sizeof(strArgTwo) - 1);
    TARGET_LANGUAGE = YTV_ASR_TINY_LANG_ID_MANDARIN_IN;

    pContainer = ytv_asr_tiny_create_container(NULL, strArgOne, TARGET_LANGUAGE, strArgTwo);
    if (pContainer) {
        ytv_asr_tiny_create_engine(pContainer, pMemoryBufferForEngine, ENGINE_BUFF_SIZE,
                                   &pEngineInstance);
        if (pEngineInstance) {
            printf("ERROR ytv_asr_tiny_create_engine %d\n", ret);
        }
    } else {
        printf("ERROR ytv_asr_tiny_create_container \n");
    }

    pthread_mutex_unlock(&mutex_YDserver_op);
    printf("AIYD baidu init --\n");
    return -1;
}

int CCaptCosume_deinitYD_baidu(void *priv)
{
    printf("AIYD baidu deinit ++\n");
    pthread_mutex_lock(&mutex_YDserver_op);

    if (pEngineInstance) {
        printf("ytv_asr_tiny_free_engine...\n");
        ytv_asr_tiny_free_engine(pEngineInstance);
        pEngineInstance = NULL;
    }

    if (pContainer) {
        printf("ytv_asr_tiny_free_container...\n");
        ytv_asr_tiny_free_container(pContainer);
        pContainer = NULL;
    }

    pthread_mutex_unlock(&mutex_YDserver_op);
    printf("AIYD baidu deinit --\n");
    return 0;
}

void AIYD_Start_baidu(void)
{
    printf("AIYD baidu start ++\n");
    CCaptCosume_initYD_baidu(&Cap_Cosume_AIYD_baidu);
    CCaptRegister(&Cap_Cosume_AIYD_baidu);
    printf("AIYD baidu start --\n");
}

void AIYD_Stop_baidu(void)
{
    printf("AIYD baidu stop ++ \n");
    CCaptCosume_deinitYD_baidu(&Cap_Cosume_AIYD_baidu);
    CCaptUnregister(&Cap_Cosume_AIYD_baidu);
    printf("AIYD baidu stop -- \n");
}

void AIYD_enable_baidu(void)
{
    printf("AIYD baidu enable ++\n");
    pthread_mutex_lock(&mutex_YDserver_op);
    Cap_Cosume_AIYD_baidu.enable = 1;
    pthread_mutex_unlock(&mutex_YDserver_op);
    printf("AIYD baidu enable --\n");
}

void AIYD_disable_baidu(void)
{
    printf("AIYD baidu disable ++\n");
    pthread_mutex_lock(&mutex_YDserver_op);
    Cap_Cosume_AIYD_baidu.enable = 0;
    pthread_mutex_unlock(&mutex_YDserver_op);
    printf("AIYD baidu disable --\n");
}

#endif
