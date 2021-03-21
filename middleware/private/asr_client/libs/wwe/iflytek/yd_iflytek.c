
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <linux/autoconf.h> //kernel config
#include "pthread.h"
#include "json.h"

#include "qisr.h"
#include "msp_cmn.h"
#include "msp_errors.h"
#include "ccaptureGeneral.h"
#include "asr_tts.h"
#include "wm_util.h"

#if defined(YD_IFLYTEK_MODULE)
extern int g_iflytek_login;
extern WIIMU_CONTEXT *g_wiimu_shm;

//#define DEBUG_DUMP_YD

char *YD_sessionID = 0;
int msp_yd_audio_status;
int msp_yd_score = 0;
// int m_YD_thread_run = 0;
int m_YD_ep_err_count = 0;
static pthread_mutex_t mutex_YDserver_op = PTHREAD_MUTEX_INITIALIZER;

#ifdef DEBUG_DUMP_YD
FILE *fpYdDump = NULL;
#endif

int CCaptCosume_YD_iflytek(char *buf, int size, void *priv);
int CCaptCosume_initYD_iflytek(void *priv);
int CCaptCosume_deinitYD_iflytek(void *priv);

CAPTURE_COMUSER Cap_Cosume_AIYD_iflytek = {
    .enable = 0,
    .cCapture_dataCosume = CCaptCosume_YD_iflytek,
    .cCapture_init = CCaptCosume_initYD_iflytek,
    .cCapture_deinit = CCaptCosume_deinitYD_iflytek,
};

int CCaptCosume_YD_iflytek(char *buf, int size, void *priv)
{
    long pcmCount = 0;
    long pcmSize = 0;
    int ret = 0;

    if (!Cap_Cosume_AIYD_iflytek.enable || !YD_sessionID || CheckPlayerStatus(g_wiimu_shm)) {
        usleep(15000);
        return size;
    }

    pcmSize = size;

    while (YD_sessionID && pcmSize) {
        unsigned int len = 6400;
        if (pcmSize < 6400)
            len = pcmSize;
        // printf("CCaptCosume_YD_iflytek %d++\n", pcmSize);
        pthread_mutex_lock(&mutex_YDserver_op);
        ret = QIVWAudioWrite(YD_sessionID, (const void *)&buf[pcmCount], len, msp_yd_audio_status);
        msp_yd_audio_status = MSP_AUDIO_SAMPLE_CONTINUE;
        pthread_mutex_unlock(&mutex_YDserver_op);

#ifdef DEBUG_DUMP_YD
        if (fpYdDump) {
            fwrite(&buf[pcmCount], 1, len, fpYdDump);
        }
#endif
        // printf("CCaptCosume_YD_iflytek--\n");
        if (ret != 0) {
            // printf("CCaptCosume_YD_iflytek write error\n");
            CCaptCosume_deinitYD_iflytek(&Cap_Cosume_AIYD_iflytek);
            CCaptCosume_initYD_iflytek(&Cap_Cosume_AIYD_iflytek);
            break;
        }

        pcmCount += (long)len;
        pcmSize -= (long)len;
        usleep(15000);
    }

    return size;
}

int cb_ivw_msg_proc(const char *sessionID, int msg, int param1, int param2, const void *info,
                    void *userData)
{
    if (2 == msg) //唤醒出错消息
    {
        m_YD_ep_err_count++;
        SocketClientReadWriteMsg("/tmp/RequestASRTTS", "CAP_RESET", strlen("CAP_RESET"), NULL, NULL,
                                 0);
        printf("\n\nMSP_IVW_MSG_WAKEUP2 error result = %d\n\n", param1);
    } else if (1 == msg) //唤醒成功消息
    {
        int score = 0;
        int id = -1;
        if (info)
            printf("\n\nMSP_IVW_MSG_WAKEUP1 wakeup result = %s\n\n", (char *)info);
        json_object *new_obj = 0, *sub_obj = 0;
        new_obj = json_tokener_parse(info);
        if (new_obj) {
            sub_obj = json_object_object_get(new_obj, "score");
            if (sub_obj)
                score = json_object_get_int(sub_obj);
            sub_obj = json_object_object_get(new_obj, "id");
            if (sub_obj)
                id = json_object_get_int(sub_obj);

            json_object_put(new_obj);
        }

#if defined(S11_EVB_XIMALAYA)
        if (1)
#else
        if (id == 0)
#endif
        {
            msp_yd_score = score;
            if (score > 10) {
                msp_yd_score = -40;
                SocketClientReadWriteMsg("/tmp/RequestASRTTS", "talkstart:1", strlen("talkstart:1"),
                                         NULL, NULL, 0);
            }
        } else
            msp_yd_score = -40;

        m_YD_ep_err_count = 0;
    } else if (3 == msg) //唤醒成功消息
    {
        if (info) {
            printf("\n\nMSP_IVW_MSG_WAKEUP3 wakeup result = %s (%d - %s)\n\n", (char *)info,
                   msp_yd_score, TTS_WAKEUP);
            if (msp_yd_score != -40 &&
                (strstr(info, TTS_WAKEUP) || (msp_yd_score > 0 && (strstr(info, TTS_WAKEUP_BA1) ||
                                                                   strstr(info, TTS_WAKEUP_BA2) ||
                                                                   strstr(info, TTS_WAKEUP_BA3)) &&
                                              strlen(info) < 5) ||
                 (msp_yd_score > -15 && (strstr(info, TTS_WAKEUP_2) || strstr(info, TTS_WAKEUP_3) ||
                                         strstr(info, TTS_WAKEUP_4)) &&
                  strlen(info) < 6))) {
                msp_yd_score = -40;
                SocketClientReadWriteMsg("/tmp/RequestASRTTS", "talkstart:1", strlen("talkstart:1"),
                                         NULL, NULL, 0);
            } else if (msp_yd_score != -40) {
                SocketClientReadWriteMsg("/tmp/RequestASRTTS", "talkwakeup:3",
                                         strlen("talkwakeup:3"), NULL, NULL, 0);
                msp_yd_score = -40;
            }
        } else if (msp_yd_score != -40) {
            SocketClientReadWriteMsg("/tmp/RequestASRTTS", "talkwakeup:3", strlen("talkwakeup:3"),
                                     NULL, NULL, 0);
            msp_yd_score = -40;
        }
    } else {
        // printf("\n\nMSP_IVW_MSG_WAKEUP4 wakeup result \n\n");
    }

    return 0;
}

int CCaptCosume_initYD_iflytek(void *priv)
{
/// APPID请勿随意改动
#if defined(S11_EVB_XIMALAYA)
    const char *lgi_param =
        "appid = 563b5dfb,engine_start = ivw,ivw_res_path "
        "=fo|/system/workdir/misc/msc/res/ivw/wakeupresource.jet"; //使用唤醒需要在此设置engine_start
                                                                   //= ivw,ivw_res_path =fo|xxx/xx
                                                                   //启动唤醒引擎
    // const char *ssb_param = "ivw_threshold=0:-15;1:50;2:50,sst=wakeup";
    const char *ssb_param =
        "ivw_threshold=0:-35;1:-35;2:-35,sst=oneshot,sub=iat,domain=iat,auf=audio/"
        "L16;rate=16000,aue=speex-wb,ent=sms16k,rst=plain,vad_enable=0,rse=utf8,asr_ptt=false";
#elif defined(JD_RPC)
    const char *lgi_param =
        "appid = 56d3b3d8,engine_start = ivw,ivw_res_path "
        "=fo|/system/workdir/misc/msc/res/ivw/wakeupresource.jet"; //使用唤醒需要在此设置engine_start
                                                                   //= ivw,ivw_res_path =fo|xxx/xx
                                                                   //启动唤醒引擎
#elif defined(S11_EVB_DOSS_TINGSHU)
    const char *lgi_param =
        "appid = 5770e3e3,engine_start = ivw,ivw_res_path "
        "=fo|/system/workdir/misc/msc/res/ivw/wakeupresource.jet"; //使用唤醒需要在此设置engine_start
                                                                   //= ivw,ivw_res_path =fo|xxx/xx
                                                                   //启动唤醒引擎
    // const char *ssb_param = "ivw_threshold=0:-15;1:50;2:50,sst=wakeup";
    const char *ssb_param =
        "ivw_threshold=0:-35;1:50;2:50,sst=oneshot,sub=iat,domain=iat,auf=audio/"
        "L16;rate=16000,aue=speex-wb,ent=sms16k,rst=plain,vad_enable=0,rse=utf8,asr_ptt=false";
#else
    const char *lgi_param =
        "appid = 554b0696,engine_start = ivw,ivw_res_path "
        "=fo|/system/workdir/misc/msc/res/ivw/wakeupresource.jet"; //使用唤醒需要在此设置engine_start
                                                                   //= ivw,ivw_res_path =fo|xxx/xx
                                                                   //启动唤醒引擎
    // const char *ssb_param = "ivw_threshold=0:-15;1:50;2:50,sst=wakeup";
    const char *ssb_param =
        "ivw_threshold=0:-35;1:50;2:50,sst=oneshot,sub=iat,domain=iat,auf=audio/"
        "L16;rate=16000,aue=speex-wb,ent=sms16k,rst=plain,vad_enable=0,rse=utf8,asr_ptt=false";
#endif
    int err_code = MSP_SUCCESS;
    int ret = 0;

#ifdef DEBUG_DUMP_YD
    char filename[128];
    snprintf(filename, sizeof(filename), "/tmp/yd_cap_pcm_%d.pcm", (int)tickSincePowerOn());
    if (!fpYdDump)
        fpYdDump = fopen(filename, "w+");
#endif

    // m_YD_thread_run = 1;
    pthread_mutex_lock(&mutex_YDserver_op);
    m_YD_ep_err_count = 0;

    if (!g_iflytek_login)
        MSPLogin(NULL, NULL, lgi_param);
    // else
    //	ret = MSP_SUCCESS;
    // if ( ret == MSP_SUCCESS )
    //{
    g_iflytek_login = 1;
    YD_sessionID = QIVWSessionBegin(NULL, ssb_param, &err_code);
    if (err_code == MSP_SUCCESS) {
        msp_yd_audio_status = MSP_AUDIO_SAMPLE_FIRST;
        msp_yd_score = -40;
        err_code = QIVWRegisterNotify(YD_sessionID, cb_ivw_msg_proc, NULL);
        if (err_code == MSP_SUCCESS) {
            pthread_mutex_unlock(&mutex_YDserver_op);
            return 0;
        } else {
            // MSPLogout();
            printf("!!!!!!!!QIVWRegisterNotify return code:%d\n", err_code);
        }
    } else {
        // MSPLogout();
        printf("!!!!!!!!QIVWSessionBegin return code:%d\n", err_code);
    }
    //}
    // else
    //{
    //	printf("!!!!!!!!MSPLogin ret:%d\n",ret);
    //
    //}
    // g_iflytek_login = 0;
    if (YD_sessionID)
        QIVWSessionEnd(YD_sessionID, "FAIL");
    YD_sessionID = 0;
    pthread_mutex_unlock(&mutex_YDserver_op);
    return -1;
}

int CCaptCosume_deinitYD_iflytek(void *priv)
{
    pthread_mutex_lock(&mutex_YDserver_op);
    Cap_Cosume_AIYD_iflytek.enable = 0;
    if (NULL != YD_sessionID) {
        msp_yd_audio_status = MSP_AUDIO_SAMPLE_LAST;
        QIVWAudioWrite(YD_sessionID, (const void *)NULL, 0, msp_yd_audio_status);
        QIVWSessionEnd(YD_sessionID, "OK");
    }
    YD_sessionID = 0;
    // MSPLogout();
    // g_iflytek_login = 0;
    pthread_mutex_unlock(&mutex_YDserver_op);

#ifdef DEBUG_DUMP_YD
    if (fpYdDump) {
        fclose(fpYdDump);
        fpYdDump = 0;
    }
#endif
    return 0;
}

void AIYD_Start_iflytek(void)
{
    // if(Cap_Cosume_AIYD_iflytek.enable)
    //{
    //    printf("AIYD iflytke already run, exit\n");
    //    return;
    //}

    printf("AIYD iflytke start\n");
    // if(YD_sessionID)
    //    CCaptUnregister(&Cap_Cosume_AIYD_iflytek);
    CCaptRegister(&Cap_Cosume_AIYD_iflytek);
    // Cap_Cosume_AIYD_iflytek.enable = 1;
    printf("AIYD iflytke start end\n");
}

void AIYD_Stop_iflytek(void)
{
    // if(!Cap_Cosume_AIYD_iflytek.enable)
    //{
    //    printf("AIYD iflytke not running, exit\n");
    //    return;
    //}

    printf("AIYD iflytke stop\n");
    // Cap_Cosume_AIYD_iflytek.enable = 0;
    // if(YD_sessionID)
    CCaptUnregister(&Cap_Cosume_AIYD_iflytek);
    printf("AIYD iflytke end\n");
}

extern void CCaptFlush(void);

void AIYD_enable_iflytek(void)
{
    // CCaptCosume_deinitYD_iflytek(&Cap_Cosume_AIYD_iflytek);
    printf("AIYD enable++\n");
    if (YD_sessionID)
        CCaptCosume_deinitYD_iflytek(&Cap_Cosume_AIYD_iflytek);
    CCaptCosume_initYD_iflytek(&Cap_Cosume_AIYD_iflytek);
    Cap_Cosume_AIYD_iflytek.enable = 1;
    printf("AIYD enable--\n");
}

void AIYD_disable_iflytek(void)
{
    printf("AIYD disable ++\n");
    pthread_mutex_lock(&mutex_YDserver_op);
    Cap_Cosume_AIYD_iflytek.enable = 0;
    pthread_mutex_unlock(&mutex_YDserver_op);

    printf("AIYD disable --\n");
}

#endif
