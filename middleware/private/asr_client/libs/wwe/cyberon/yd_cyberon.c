#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include "pthread.h"
#include "ccaptureGeneral.h"
#include "wm_util.h"

#if defined(YD_CYBERON_MODULE)
#include "CSpotterSDKApi.h"
#include "CSpotterSDKApi_Const.h"

static void *g_cspotter_handler = NULL;
static long g_result_counts = 0;
static pthread_mutex_t mutex_YDserver_op = PTHREAD_MUTEX_INITIALIZER;

extern int asr_waked;
extern int g_ntp_synced;
extern WIIMU_CONTEXT *g_wiimu_shm;

#define RECOGNIZER_FRAME_SIZE 8192

#define CYBERON_RES_LIB_PATH "/system/workdir/lib/libNINJA.so"
#define CYBERON_RES_BIN_PATH "/system/workdir/lib/Xiaodu_Adapt.bin"

int CCaptCosume_YD_cyberon(char *buf, int size, void *priv);
int CCaptCosume_initYD_cyberon(void *priv);
int CCaptCosume_deinitYD_cyberon(void *priv);

CAPTURE_COMUSER Cap_Cosume_AIYD_cyberon = {
    .enable = 0,
    .cCapture_dataCosume = CCaptCosume_YD_cyberon,
    .cCapture_init = CCaptCosume_initYD_cyberon,
    .cCapture_deinit = CCaptCosume_deinitYD_cyberon,
    .id = 2,
};

int CCaptCosume_YD_cyberon(char *buf, int size, void *priv)
{
    int pcmCount = 0;
    int pcmSize = 0;
    int rs = 0;

    if (!Cap_Cosume_AIYD_cyberon.enable || !g_cspotter_handler) {
        // usleep(15000);
        return size;
    }

    pcmSize = size;

    while (g_cspotter_handler && pcmSize) {
        int len = RECOGNIZER_FRAME_SIZE;
        if (pcmSize < len)
            len = pcmSize;

        pthread_mutex_lock(&mutex_YDserver_op);
        rs = CSpotter_AddSample(g_cspotter_handler, (short *)(buf + pcmCount), len / 2);
        if (rs == CSPOTTER_SUCCESS) {
            char result[1024];

            if (CSpotter_GetUTF8Result(g_cspotter_handler, NULL, result) >= 0) {
                printf("YD_cyberon get wakeup result [%d]: %s \n", g_result_counts, result);
                g_result_counts++;
            }

            SocketClientReadWriteMsg("/tmp/RequestASRTTS", "talkstart:1", strlen("talkstart:1"),
                                     NULL, NULL, 0);
#if !defined(BAIDU_DUMI) && !defined(TENCENT)
            asr_waked = 1;
#endif
            CSpotter_Reset(g_cspotter_handler);
        }

        pcmCount += len;
        pcmSize -= len;
        pthread_mutex_unlock(&mutex_YDserver_op);
    }

    return size;
}

int CCaptCosume_initYD_cyberon(void *priv)
{
    int rs = 0;

    printf("AIYD cyberon init ++\n");
    pthread_mutex_lock(&mutex_YDserver_op);

    if (g_cspotter_handler != NULL) {
        CSpotter_Release(g_cspotter_handler);
        g_cspotter_handler = NULL;
    }

    if (!g_ntp_synced) {
        system("date \"2017-08-01 00:00\"");
    }

    g_cspotter_handler =
        CSpotter_InitWithFiles(CYBERON_RES_LIB_PATH, CYBERON_RES_BIN_PATH, NULL, &rs);
    if (g_cspotter_handler == NULL) {
        printf("YD_cyberon fail to initialize CSpotter! rs = %d\n", rs);
        pthread_mutex_unlock(&mutex_YDserver_op);
        return rs;
    } else {
        printf("YD_cyberon g_cspotter_handler = 0x%x, rs = %d!\n", g_cspotter_handler, rs);
    }

    rs = CSpotter_Reset(g_cspotter_handler);
    if (rs != CSPOTTER_SUCCESS) {
        printf("YD_cyberon fail to start recognition <%d>!\n", rs);
    }

    pthread_mutex_unlock(&mutex_YDserver_op);
    printf("AIYD cyberon init --\n");

    return rs;
}

int CCaptCosume_deinitYD_cyberon(void *priv)
{
    printf("AIYD cyberon deinit ++\n");
    pthread_mutex_lock(&mutex_YDserver_op);

    if (g_cspotter_handler != NULL) {
        CSpotter_Release(g_cspotter_handler);
        g_cspotter_handler = NULL;
    }

    pthread_mutex_unlock(&mutex_YDserver_op);
    printf("AIYD cyberon deinit --\n");

    return 0;
}

void AIYD_Start_cyberon(void)
{
    printf("AIYD cyberon start ++\n");
    CCaptCosume_initYD_cyberon(&Cap_Cosume_AIYD_cyberon);
    CCaptRegister(&Cap_Cosume_AIYD_cyberon);
    printf("AIYD cyberon start --\n");
}

void AIYD_Stop_cyberon(void)
{
    printf("AIYD cyberon stop ++ \n");
    CCaptCosume_deinitYD_cyberon(&Cap_Cosume_AIYD_cyberon);
    CCaptUnregister(&Cap_Cosume_AIYD_cyberon);
    printf("AIYD cyberon stop -- \n");
}

void AIYD_enable_cyberon(void)
{
    printf("AIYD cyberon enable ++\n");
    pthread_mutex_lock(&mutex_YDserver_op);
    Cap_Cosume_AIYD_cyberon.enable = 1;
    pthread_mutex_unlock(&mutex_YDserver_op);
    printf("AIYD cyberon enable --\n");
}

void AIYD_disable_cyberon(void)
{
    printf("AIYD cyberon disable ++\n");
    pthread_mutex_lock(&mutex_YDserver_op);
    Cap_Cosume_AIYD_cyberon.enable = 0;
    pthread_mutex_unlock(&mutex_YDserver_op);
    printf("AIYD cyberon disable --\n");
}

#endif
