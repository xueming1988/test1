#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <curl/curl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "soft_pre_process.h"
#include "soft_prep_rel_interface.h"
#ifdef ENABLE_RECORD_SERVER
#include "AudioCaptureServer.h"
#endif

#if defined(SOFT_PREP_DSPC_MODULE) || defined(SOFT_PREP_DSPC_V2_MODULE)
#define SOFT_PREP_REL_LIB_PROVIDER "softPrep.d."
#else
#define SOFT_PREP_REL_LIB_PROVIDER "softPrep.h."
#endif
#define SOFT_PREP_REL_LIB_VER       SOFT_PREP_REL_LIB_PROVIDER"1.16.1"

SOFT_PRE_PROCESS *g_soft_prep_rel = NULL;
static pthread_mutex_t mutex_soft_prep_rel = PTHREAD_MUTEX_INITIALIZER;

#if defined(SOFT_PREP_DSPC_MODULE) || defined(SOFT_PREP_DSPC_V2_MODULE)
extern SOFT_PRE_PROCESS Soft_prep_dspc;
#endif
#if defined(SOFT_PREP_HOBOT_MODULE)
extern SOFT_PRE_PROCESS Soft_prep_hobot;
#endif
extern void *Soft_Prep_evtSocket_start(void *arg);
pthread_t evt_thread;

#ifdef ENABLE_RECORD_SERVER
void *Soft_Prep_comuser_capture_server = NULL;
int evt_run = 0;

void Soft_Prep_StartRecordServer(int nChannel)
{
    struct _stCaptureServerInfo *pInfo;

    if(Soft_Prep_comuser_capture_server != NULL) {
        printf("server is running, cann't running again\n");
        return;
    }

    printf("record server will start, channel is %d\n", nChannel);
    pInfo = (struct _stCaptureServerInfo *)malloc(sizeof(struct _stCaptureServerInfo));
    memset(pInfo, 0, sizeof(struct _stCaptureServerInfo));
    Soft_Prep_comuser_capture_server = (void *)pInfo;
    pInfo->nServerPort = 7513;
    // Soft_Prep_comuser_capture_server.pre_process = (nChannel == 1) ? 0 : 1;
    CaptureServerInit(Soft_Prep_comuser_capture_server);

    pInfo->nSampleRate = 16000;
    pInfo->nChannel = nChannel;
    pInfo->nBits = 16;
    pInfo->nPrepSampleRate = 48000;
    pInfo->nPrepChannel = 8;

#if defined(SOFT_PREP_MODULE)
#if defined(SOFT_PREP_DSPC_MODULE) || defined(SOFT_PREP_DSPC_V2_MODULE)
    pInfo->nPrepBits = 32;
#elif defined(SOFT_PREP_HOBOT_MODULE)
    pInfo->nPrepBits = 16;
#else
    printf("No Soft PreProcess defined\n");
#endif
    if(g_soft_prep_rel && g_soft_prep_rel->cSoft_PreP_setRecordCallback) {
        g_soft_prep_rel->cSoft_PreP_setRecordCallback(CaptureServerPreSaveData,
                &pInfo->stPrepParam);
    }
#endif
}

void Soft_Prep_StopRecordServer(void)
{
    if(Soft_Prep_comuser_capture_server) {
#if defined(SOFT_HOBOT_MODULE)
        if(g_soft_hobot_process && g_soft_hobot_process->cSoft_hobot_setRecordCallback) {
            g_soft_hobot_process->cSoft_hobot_setRecordCallback(NULL, NULL);
        }
#endif

#if defined(SOFT_PREP_MODULE)
        if(g_soft_prep_rel && g_soft_prep_rel->cSoft_PreP_setRecordCallback) {
            g_soft_prep_rel->cSoft_PreP_setRecordCallback(NULL, NULL);
        }
#endif
        CaptureServerDeinit(Soft_Prep_comuser_capture_server);

        if(Soft_Prep_comuser_capture_server != NULL)
            free(Soft_Prep_comuser_capture_server);

        Soft_Prep_comuser_capture_server = NULL;
    }
}

void *Soft_Prep_evtSocket_start(void *arg)
{
    int recv_num;
    char recv_buf[65536];
    int ret;

    printf("%s ++\n", __func__);
    SOCKET_CONTEXT msg_socket;
    msg_socket.path = "/tmp/RequestASRTTS";
    msg_socket.blocking = 1;

RE_INIT:
    ret = UnixSocketServer_Init(&msg_socket);
    if(ret <= 0) {
        printf("Soft_Prep UnixSocketServer_Init fail\n");
        sleep(5);
        goto RE_INIT;
    }

    while(evt_run) {
        ret = UnixSocketServer_accept(&msg_socket);
        if(ret <= 0) {
            printf("Soft_Prep UnixSocketServer_accept fail\n");
            UnixSocketServer_deInit(&msg_socket);
            sleep(5);
            goto RE_INIT;
        } else {
            memset(recv_buf, 0x0, sizeof(recv_buf));
            recv_num = UnixSocketServer_read(&msg_socket, recv_buf, sizeof(recv_buf));
            if(recv_num <= 0) {
                printf("Soft_Prep recv failed\r\n");
                if(recv_num < 0) {
                    UnixSocketServer_close(&msg_socket);
                    UnixSocketServer_deInit(&msg_socket);
                    sleep(5);
                    goto RE_INIT;
                }
            } else {
                recv_buf[recv_num] = '\0';

                printf("Soft_Prep recv_buf %s +++++++++", recv_buf);
                if(strncmp(recv_buf, "startRecordServer", strlen("startRecordServer")) == 0) {
                    int nChannel = 1;
                    char *pChannel = strstr(recv_buf, "startRecordServer:");

                    if(pChannel != NULL) {
                        pChannel += strlen("startRecordServer:");
                        nChannel = atoi(pChannel);
                    }

                    Soft_Prep_StartRecordServer(nChannel);
                } else if(strncmp(recv_buf, "stopRecordServer", strlen("stopRecordServer")) == 0) {
                    Soft_Prep_StopRecordServer();
                }
            }

            UnixSocketServer_close(&msg_socket);
        }
    }
    printf("%s --\n", __func__);
    return NULL;
}
#endif

int Soft_Prep_Init(SOFT_PREP_INIT init_input)
{
    int ret;

    pthread_mutex_lock(&mutex_soft_prep_rel);

#if defined(SOFT_PREP_DSPC_MODULE) || defined(SOFT_PREP_DSPC_V2_MODULE)
    g_soft_prep_rel = &Soft_prep_dspc;
    ret = 0;
#elif defined(SOFT_PREP_HOBOT_MODULE)
    g_soft_prep_rel = &Soft_prep_hobot;
    ret = 0;
#else
    printf("=============Miss Soft PreProcess module definition!============");
#endif

#ifdef ENABLE_RECORD_SERVER
    evt_run = 1;
    pthread_create(&evt_thread, NULL, Soft_Prep_evtSocket_start, NULL);
#endif

    if(g_soft_prep_rel) {
        SOFT_PREP_INIT_PARAM in;
        in.mic_numb = init_input.mic_numb;
        in.cap_dev = init_input.cap_dev;
        in.configFile = init_input.configFile;
        ret = g_soft_prep_rel->cSoft_PreP_init(in);
        if(g_soft_prep_rel->cSoft_PreP_setFillCallback && init_input.captureCallback) {
            g_soft_prep_rel->cSoft_PreP_setFillCallback(init_input.captureCallback,
                    init_input.opaque_capture);
        }
        if(g_soft_prep_rel->cSoft_PreP_setWakeupCallback && init_input.wakeupCallback) {
            g_soft_prep_rel->cSoft_PreP_setWakeupCallback(init_input.wakeupCallback,
                    init_input.opaque_wakeup);
        }
        if(g_soft_prep_rel->cSoft_PreP_setObserverStateCallback &&
                init_input.ObserverStateCallback) {
            g_soft_prep_rel->cSoft_PreP_setObserverStateCallback(init_input.ObserverStateCallback,
                    init_input.opaque_observer);
        }
        if(g_soft_prep_rel->cSoft_PreP_setMicrawCallback && init_input.micrawCallback) {
            g_soft_prep_rel->cSoft_PreP_setMicrawCallback(init_input.micrawCallback,
                    init_input.opaque_micraw);
        }

        printf("soft process library version:%s loaded\n", Soft_Prep_GetVersion());
    }

    pthread_mutex_unlock(&mutex_soft_prep_rel);
    return ret;
}

int Soft_Prep_Deinit(void)
{
    pthread_mutex_lock(&mutex_soft_prep_rel);
    if(g_soft_prep_rel && g_soft_prep_rel->cSoft_PreP_unInit) {
        g_soft_prep_rel->cSoft_PreP_unInit();
    }
    pthread_mutex_unlock(&mutex_soft_prep_rel);
#ifdef ENABLE_RECORD_SERVER
    evt_run = 0;
    SocketClientReadWriteMsg("/tmp/RequestASRTTS", "quitThread", strlen("quitThread"), NULL, NULL,
                             0);
    pthread_join(evt_thread, NULL);
#endif
    usleep(100000);
    return 0;
}

char *Soft_Prep_GetVersion(void)
{
    return SOFT_PREP_REL_LIB_VER;
}

int Soft_Prep_GetCapability(void)
{
    int ret = 0;
    pthread_mutex_lock(&mutex_soft_prep_rel);
    if(g_soft_prep_rel && g_soft_prep_rel->cSoft_PreP_getCapability) {
        ret = g_soft_prep_rel->cSoft_PreP_getCapability();
    }
    pthread_mutex_unlock(&mutex_soft_prep_rel);

    return ret;
}

WAVE_PARAM Soft_Prep_GetAudioParam(void)
{
    WAVE_PARAM ret;
    memset(&ret, 0, sizeof(WAVE_PARAM));
    pthread_mutex_lock(&mutex_soft_prep_rel);
    if(g_soft_prep_rel && g_soft_prep_rel->cSoft_PreP_getAudioParam) {
        ret = g_soft_prep_rel->cSoft_PreP_getAudioParam();
    }
    pthread_mutex_unlock(&mutex_soft_prep_rel);
    return ret;
}

WAVE_PARAM Soft_Prep_GetMicrawAudioParam(void)
{
    WAVE_PARAM ret;
    memset(&ret, 0, sizeof(WAVE_PARAM));
    pthread_mutex_lock(&mutex_soft_prep_rel);
    if(g_soft_prep_rel && g_soft_prep_rel->cSoft_PreP_getMicRawAudioParam) {
        ret = g_soft_prep_rel->cSoft_PreP_getMicRawAudioParam();
    }
    pthread_mutex_unlock(&mutex_soft_prep_rel);
    return ret;
}

int Soft_prep_flushState(void)
{
    int ret = 0;
    pthread_mutex_lock(&mutex_soft_prep_rel);
    if(g_soft_prep_rel && g_soft_prep_rel->cSoft_PreP_flushState) {
        ret = g_soft_prep_rel->cSoft_PreP_flushState();
    }
    pthread_mutex_unlock(&mutex_soft_prep_rel);

    return ret;
}

int Soft_Prep_StartWWE(void)
{
    int ret = 0;
    int cap;
    cap = Soft_Prep_GetCapability();
    if(!(cap & SOFP_PREP_CAP_WAKEUP)) {
        printf("unsupport Capability of Wake up\n");
        return -1;
    }
    pthread_mutex_lock(&mutex_soft_prep_rel);
    if(g_soft_prep_rel && g_soft_prep_rel->cSoft_PreP_WWE_enable) {
        ret = g_soft_prep_rel->cSoft_PreP_WWE_enable(1);
    }
    pthread_mutex_unlock(&mutex_soft_prep_rel);
    return ret;
}

int Soft_Prep_StopWWE(void)
{
    int ret = 0;
    int cap;
    cap = Soft_Prep_GetCapability();
    if(!(cap & SOFP_PREP_CAP_WAKEUP)) {
        printf("unsupport Capability of Wake up\n");
        return -1;
    }
    pthread_mutex_lock(&mutex_soft_prep_rel);
    if(g_soft_prep_rel && g_soft_prep_rel->cSoft_PreP_WWE_enable) {
        ret = g_soft_prep_rel->cSoft_PreP_WWE_enable(0);
    }
    pthread_mutex_unlock(&mutex_soft_prep_rel);
    return ret;
}

int Soft_Prep_StartCapture(void)
{
    int ret = 0;
    pthread_mutex_lock(&mutex_soft_prep_rel);
    if(g_soft_prep_rel && g_soft_prep_rel->cSoft_PreP_enable) {
        ret = g_soft_prep_rel->cSoft_PreP_enable(1);
    }
    usleep(100000);
    pthread_mutex_unlock(&mutex_soft_prep_rel);
    return ret;
}

int Soft_Prep_StopCapture(void)
{
    int ret = 0;
    pthread_mutex_lock(&mutex_soft_prep_rel);
    if(g_soft_prep_rel && g_soft_prep_rel->cSoft_PreP_enable) {
        ret = g_soft_prep_rel->cSoft_PreP_enable(0);
    }
    pthread_mutex_unlock(&mutex_soft_prep_rel);
    return ret;
}
