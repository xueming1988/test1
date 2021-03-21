#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/time.h>

#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "deviceSDK.h"
#include "xiaoweiSDK.h"
#include "voipSDK.h"
#include "wm_util.h"
#include "caiman.h"
#include "ring_buffer.h"
#include "wiimu_player_ctrol.h"
#include "xiaowei_player.h"
#include "xiaowei_interface.h"
#define MAX_VPLAYER 32
#define ASR_UPLOAD_DEBUG 1
#define XIAOWEI_ASR_UPLOAD_TIMEOUT 30
#define XIAOWEI_SILENT_TIMEOUT 500
#define XIAOWEI_SPEAK_TIMEOUT 5000

XIAOWEI_PARAM_CONTEXT context = {0};
static ring_buffer_t *g_ring_buffer = NULL;
extern WIIMU_CONTEXT *g_wiimu_shm;

static pthread_mutex_t query_status_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_query_status = e_query_idle;

void set_query_status(int status)
{
    pthread_mutex_lock(&query_status_mutex);
    g_query_status = status;
    pthread_mutex_unlock(&query_status_mutex);
}

int get_query_status(void)
{
    int tmp_query_status;
    pthread_mutex_lock(&query_status_mutex);
    tmp_query_status = g_query_status;
    pthread_mutex_unlock(&query_status_mutex);
    return tmp_query_status;
}

void set_asr_led(int status)
{
    if (status == 1)
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDIDLE",
                                 strlen("GNOTIFY=MCULEDIDLE"), NULL, NULL, 0);
    else if (status == 2)
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDLISTENING",
                                 strlen("GNOTIFY=MCULEDLISTENING"), NULL, NULL, 0);
    else if (status == 3)
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDTHINKING",
                                 strlen("GNOTIFY=MCULEDTHINKING"), NULL, NULL, 0);
    else if (status == 4)
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDPLAYTTS",
                                 strlen("GNOTIFY=MCULEDPLAYTTS"), NULL, NULL, 0);
    else if (status == 5)
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDRECOERROR",
                                 strlen("GNOTIFY=MCULEDRECOERROR"), NULL, NULL, 0);
    else if (status == 6)
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=PRIVACY_ENA",
                                 strlen("GNOTIFY=PRIVACY_ENA"), NULL, NULL,
                                 0); // Notify privacy mode enable to MCU
    else if (status == 7)
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=PRIVACY_DIS",
                                 strlen("GNOTIFY=PRIVACY_DIS"), NULL, NULL,
                                 0); // Notify privacy mode disable to MCU
    else if (status == 8)
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDENNOTI",
                                 strlen("GNOTIFY=MCULEDENNOTI"), NULL, NULL, 0);
    else if (status == 9) {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDDISNOTI",
                                 strlen("GNOTIFY=MCULEDDISNOTI"), NULL, NULL, 0);
    }
}

pthread_t g_wakeup_thread;
char voice_id[33] = {0};
// vPlayerList vPlayer_list[MAX_VPLAYER] = {0};
static player_ctrl_t *active_player = NULL;
int g_first_wakeup = 0;

static int pid_random(void)
{
    struct timeval tm;
    gettimeofday(&tm, NULL);
    srand(tm.tv_sec + tm.tv_usec);
    return (int)(rand());
}
#if 0
int create_vplayer(int pid)
{
    int i = 0;

    for(i = 0; i < MAX_VPLAYER; i++) {
        if(vPlayer_list[i].player == NULL) {
            //pid = pid_random();
            vPlayer_list[i].pid = pid;
            player_ctrl_t *resplayer = wiimu_player_ctrl_init();
            resplayer->pid = pid;
            vPlayer_list[i].player = (void *)resplayer;
            wiimu_log(1, 0, 0, 0, "***%s: pid = %d***", __func__, pid);
            break;
        }
    }

    return pid;
}

void destroy_vplayer(int pid)
{
    int i = 0;

    for(i = 0; i < MAX_VPLAYER; i++) {
        if(vPlayer_list[i].pid == pid) {
            player_ctrl_t *resplayer = (player_ctrl_t *)vPlayer_list[i].player;
            if(resplayer) {
                wiimu_player_ctrl_uninit(&resplayer);
            }
            vPlayer_list[i].player = NULL;
            vPlayer_list[i].pid = -1;
            wiimu_log(1, 0, 0, 0, "***%s: type = %d, pid = %d***", __func__, vPlayer_list[i].type, pid);
            break;
        }
    }

    return;
}

void *get_vplayer(int pid)
{
    int i = 0;
    void *vplayer = NULL;

    for(i = 0; i < MAX_VPLAYER; i++) {
        if((vPlayer_list[i].pid == pid) && vPlayer_list[i].player) {
            vplayer = vPlayer_list[i].player;
            //wiimu_log(1,0,0,0,"***%s: pid = %d***", __func__, pid);
            break;
        }
    }

    return vplayer;
}
#endif
void OnSilence(void)
{
    if (get_query_status() == e_query_record_start) {
        memset(&context, 0, sizeof(context));
        printf("OnSilence stop wakeup\n");
    }
}

int OnCancel(void)
{
    int tmp_record_cancel = 0;
    wiimu_log(1, 0, 0, 0, "%s:query status=%d\n", __func__, get_query_status());

    if (get_query_status() == e_query_record_start) {
        wiimu_log(1, 0, 0, 0, "%s:record start,so cancel record handle\n", __func__);
        xiaowei_request_cancel(voice_id);
        memset(&context, 0, sizeof(context));
        set_query_status(e_query_idle);
        tmp_record_cancel = 1;
    } else if (get_query_status() == e_query_tts_start) {
        wiimu_log(1, 0, 0, 0, "%s:tts start,so cancel tts handle\n", __func__);
        cancel_http_callback();
        WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 1, SNDSRC_ALEXA_TTS, 1);
        WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 100, SNDSRC_ALEXA_TTS);
    }
    avs_player_buffer_stop(NULL, e_asr_stop);
    if (g_wakeup_thread != 0) {
        pthread_join(g_wakeup_thread, NULL);
        g_wakeup_thread = 0;
        printf("OnCancel a_thread\n");
    }
    return tmp_record_cancel;
}

void xiaoweiRingBufferInit(void) { g_ring_buffer = RingBufferCreate(0); }

void xiaoweiInitRecordData(void) { RingBufferReset(g_ring_buffer); }

int xiaoweiStopWriteData(void) { return RingBufferSetStop(g_ring_buffer, 1); }

int xiaoweiWriteRecordData(char *data, size_t size)
{
    size_t less_len = size;
    while (less_len > 0) {
        int write_len = RingBufferWrite(g_ring_buffer, data, less_len);
        if (write_len > 0) {
            less_len -= write_len;
            data += write_len;
        } else if (write_len <= 0) {
            wiimu_log(1, 0, 0, 0, "RingBufferWrite return %d\n", write_len);
            return less_len;
        }

        if (write_len == 0)
            usleep(5000);
    }

    return size;
}

int xiaoweiReadRecordData_continue(void)
{
    ring_buffer_t *ring_buffer = g_ring_buffer;
    int readable_len = RingBufferReadableLen(ring_buffer);
    int finished = RingBufferGetFinished(ring_buffer);
    // printf("%s:finished=%d,readable_len=%d\n",__func__,finished,readable_len);
    if (finished && (readable_len == 0)) {
        return 0;
    } else {
        return 1;
    }
}

int xiaoweiReadRecordData(char *buf, unsigned int len)
{
    ring_buffer_t *ring_buffer = g_ring_buffer;
    int read_len = 0;
    int readable_len = RingBufferReadableLen(ring_buffer);
    int finished = RingBufferGetFinished(ring_buffer);
    if (!finished && (readable_len <= MIN_TX_AI_FILL_DATA_LEN)) {
        // printf("%s:wait for data in\n",__func__);
        usleep(2000);
        return 0;
    }
    read_len = RingBufferRead(ring_buffer, buf, len);
    if (read_len > 0) {
        return read_len;
    } else {
        usleep(1000);
        return 0;
    }
}

void xiaowei_voice_request(void *arg)
{
    int ret = -1;
    int filled = 0;
    char buffer[MAX_TX_AI_FILL_DATA_LEN] = {0};
    unsigned int size = 0;
    int exit_thread_flag = 0;

    context.silent_timeout = XIAOWEI_SILENT_TIMEOUT;
    context.speak_timeout = XIAOWEI_SPEAK_TIMEOUT;
    // context.voice_request_begin = false;
    context.voice_request_end = false;

    memset(voice_id, 0, 33);
    wiimu_log(1, 0, 0, 0, "voice request begin");

    XIAOWEI_PARAM_PROPERTY property = 0;
    context.request_param = property;
#ifdef ASR_UPLOAD_DEBUG
    if (0 == access("/tmp/web/asr_upload.pcm", F_OK)) {
        system("rm /tmp/web/asr_upload.pcm");
    }
#endif

    time_t time_begin = time(NULL);
    while (time(NULL) - time_begin < XIAOWEI_ASR_UPLOAD_TIMEOUT) {
        memset(buffer, 0, MAX_TX_AI_FILL_DATA_LEN);
        size = xiaoweiReadRecordData(buffer, MAX_TX_AI_FILL_DATA_LEN);
        if (size > MIN_TX_AI_FILL_DATA_LEN) {
            if ((get_query_status() == e_query_idle) ||
                ((get_query_status() == e_query_tts_start))) {
                printf("%s:status=%d\n", __func__, get_query_status());
                ret = xiaowei_request(voice_id, xiaowei_chat_via_voice, buffer, size, &context);
                exit_thread_flag = 1;
                break;
            } else {
                if (g_first_wakeup == 1) {
                    g_first_wakeup = 0;
                    context.voice_request_begin = true;
                    printf("%s:first packet\n", __func__);
                }
                ret = xiaowei_request(voice_id, xiaowei_chat_via_voice, buffer, size, &context);
            }
        }
        context.voice_request_begin = false;
#ifdef ASR_UPLOAD_DEBUG
        FILE *fdump = fopen("/tmp/web/asr_upload.pcm", "ab+");
        if (fdump) {
            fwrite(buffer, size, 1, fdump);
            fclose(fdump);
        }
#endif
    }
    if (0 == exit_thread_flag) {
        wiimu_log(1, 0, 0, 0, "%s:voice request thread exit because of request timeout\n",
                  __func__);
        asr_set_audio_resume();
        set_asr_led(1);
    }
    CAIMan_ASR_disable();
    wiimu_log(1, 0, 0, 0, "voice request end");
    // return ret;
}
void set_tts_download_finished(void) { set_query_status(e_query_idle); }

int voiceRequestThread(void)
{
    int res;
    res = pthread_create(&g_wakeup_thread, NULL, xiaowei_voice_request, NULL);
    return res;
}

void stopVoiceRequestThread(void)
{
    if (g_wakeup_thread != 0) {
        pthread_join(g_wakeup_thread, NULL);
        g_wakeup_thread = 0;
    }
}

void onWakeUp(int req_from)
{
    memset(&context, 0, sizeof(context));
    context.wakeup_type = xiaowei_wakeup_type_local;
    if (1 == req_from) {
#if defined(ASR_WAKEUP_TEXT_UPLOAD)
        context.wakeup_type = xiaowei_wakeup_type_local_with_text;
        context.wakeup_word = "小微小微";
#endif
    }
    context.wakeup_profile = xiaowei_wakeup_profile_far;
    context.voice_request_begin = true;
    set_query_status(e_query_record_start);
    g_first_wakeup = 1;
    if (voiceRequestThread()) {
        g_first_wakeup = 0;
        set_asr_led(1);
        set_query_status(e_query_idle);
        CAIMan_ASR_disable();
    }
}

void onWakeUp_context(void)
{
    int ret = 0;
    WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 0, SNDSRC_ASR);
    OnCancel();
    set_asr_led(2);
    block_voice_prompt("cn/asr_start.mp3", 0);
    xiaoweiInitRecordData();
    CAIMan_ASR_enable(0);
    CAIMan_YD_enable();
    printf("%s:wakeup_type=%d,profile=%d,id=%s\n", __func__, context.wakeup_type,
           context.wakeup_profile, context.id);
    context.voice_request_begin = true;
    set_query_status(e_query_record_start);
    g_first_wakeup = 1;
    if (voiceRequestThread()) {
        g_first_wakeup = 0;
        set_asr_led(1);
        set_query_status(e_query_idle);
        CAIMan_ASR_disable();
    }
}

void wakeup_context_handle(XIAOWEI_PARAM_CONTEXT *_context)
{
    if (context.id) {
        free(context.id);
    }
    context.id = strdup(_context->id);
    context.wakeup_type = _context->wakeup_type;
    context.wakeup_profile = _context->wakeup_profile;
}

int alarm_handle(XIAOWEI_PARAM_RESPONSE *_response)
{
    json_object *alarm_obj = NULL;
    char *clock_info_str = NULL;
    char *xiaowei_alarm_info = NULL;
    int ret = 0;

    for (int i = 0; i < _response->resource_groups_size; i++) {
        const XIAOWEI_PARAM_RESOURCE *xiaowei_resource = _response->resource_groups[i].resources;
        printf("%s:res_formt=%d,res_id=%s\n", __func__, xiaowei_resource->format,
               xiaowei_resource->id);
    }
    if (_response->response_data) {
        printf("%s:response_type=%d,response_data=%s\n", __func__, _response->response_type,
               _response->response_data);
        alarm_obj = json_tokener_parse(_response->response_data);
        if (alarm_obj) {
            clock_info_str = JSON_GET_STRING_FROM_OBJECT(alarm_obj, "clock_info");
            if (clock_info_str) {
                asprintf(&xiaowei_alarm_info, "setxiaoweialarm:%s:%s", _response->voice_id,
                         clock_info_str);
                if (xiaowei_alarm_info && (strlen(xiaowei_alarm_info) < 2048))
                    SocketClientReadWriteMsg("/tmp/Requesta01controller", xiaowei_alarm_info,
                                             strlen(xiaowei_alarm_info), NULL, NULL, 0);
            }
            json_object_put(alarm_obj);
            if (xiaowei_alarm_info) {
                free(xiaowei_alarm_info);
            }
        }
    }

    return ret;
}

int wiimu_player_ctrl_set_mute(int muted)
{
    if (muted) {
        SocketClientReadWriteMsg("/tmp/Requesta01controller", "setPlayerCmd:mute:1",
                                 strlen("setPlayerCmd:mute:1"), 0, 0, 0);
    } else {
        SocketClientReadWriteMsg("/tmp/Requesta01controller", "setPlayerCmd:mute:0",
                                 strlen("setPlayerCmd:mute:0"), 0, 0, 0);
    }

    return 0;
}

extern share_context *g_mplayer_shmem;
extern WIIMU_CONTEXT *g_wiimu_shm;

long get_play_pos(void)
{
    if (g_mplayer_shmem) {
        wiimu_log(1, 0, 0, 0, "%s pos=%ld ms postion=%ld duration=%d", __FUNCTION__,
                  (long)(g_mplayer_shmem->offset_pts * 1000),
                  (long)(g_mplayer_shmem->postion * 1000), g_wiimu_shm->play_duration);
        return (long)(g_mplayer_shmem->offset_pts * 1000.0) < 0
                   ? 0
                   : (long)(g_mplayer_shmem->offset_pts * 1000.0); /* if offset < 0, return 0 */
    }
    return 0;
}
