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
#include <signal.h>
#include <curl/curl.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <resolv.h>

#include "json.h"
#include "wm_db.h"
#include "wm_util.h"
#include "xiaowei_linkplay_interface.h"
#include "ring_buffer.h"
#include "caiman.h"
#include "asr_tts.h"
#include "url_player_xiaowei.h"
#include "voipSDK.h"
#ifdef BT_HFP_ENABLE
#include "playback_bt_hfp.h"
#endif

int g_xiaowei_talkstart_test = 0;
#define XIAOWEI_TALKSTART_VOICE 1

#ifdef XIAOWEI_BETA
#define TXQQ_DEFAULT_LICENSE                                                                       \
    "MEQCIAWqyl7iTNpl7IvybF2+cAgKWvHpFpOqE42P40SE3QYRAiBXzxMEhyy0nihcYBOiHVrvfMpimfRBx8d+i1IQE/"   \
    "7Pgw=="
#define TXQQ_DEFAULT_GUID "53H94987A0000001"
#define TXQQ_DEFAULT_PID "691"
#define TXQQ_DEFAULT_APPID "ilinkapp_060000df9fcdc9"
#define TXQQ_DEFAULT_KEY_VER "1"

char txqq_licence[TXQQ_DEFAULT_LICENSE_LEN] = TXQQ_DEFAULT_LICENSE;
char txqq_guid[TXQQ_DEFAULT_GUID_LEN] = TXQQ_DEFAULT_GUID;
char txqq_product_id[TXQQ_DEFAULT_PID_LEN] = TXQQ_DEFAULT_PID;
char txqq_appid[TXQQ_DEFAULT_APPID_LEN] = TXQQ_DEFAULT_APPID;
char txqq_key_ver[TXQQ_DEFAULT_KEY_VER_LEN] = TXQQ_DEFAULT_KEY_VER;

#else
char txqq_licence[TXQQ_DEFAULT_LICENSE_LEN] = {0};
char txqq_guid[TXQQ_DEFAULT_GUID_LEN] = {0};
char txqq_product_id[TXQQ_DEFAULT_PID_LEN] = {0};
char txqq_appid[TXQQ_DEFAULT_APPID_LEN] = {0};
char txqq_key_ver[TXQQ_DEFAULT_KEY_VER_LEN] = {0};
#endif

#define WECHATCALL_BUF_SIZE (32 * 100)
static pthread_mutex_t query_status_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t duplex_mode_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t xiaowei_player_resume_status_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t hfp_status_mutex = PTHREAD_MUTEX_INITIALIZER;

static int g_query_status = e_query_idle;
static int g_duplex_mode = 0;
static int g_xiaowei_player_need_resume = 0;
static int g_hfp_status = 0;
WIIMU_CONTEXT *g_wiimu_shm = 0;
static ring_buffer_t *g_ring_buffer = NULL;
#ifdef VOIP_SUPPORT
static ring_buffer_t *g_wechatcall_ring_buffer = NULL;
#endif
asr_dump_t asr_dump_info;
int g_xiaowei_wakeup_check_debug = 0;
extern int ignore_reset_trigger;

int g_wechatcall_status = 0;
extern XIAOWEI_PARAM_STATE g_xiaowei_report_state;
XIAOWEI_PARAM_CONTEXT context = {0};
int g_first_wakeup = 0;
pthread_t g_wakeup_thread;
pthread_t g_wechatcall_send_thread;
pthread_t g_wechatcall_recv_thread;
pthread_t g_wechatcall_ring_play_thread;

char voice_id[33] = {0};
#define XIAOWEI_ASR_UPLOAD_TIMEOUT 30
#define XIAOWEI_SILENT_TIMEOUT 500
#define XIAOWEI_SPEAK_TIMEOUT 5000
#define XIAOWEI_DUPLEX_SPEAK_TIMEOUT 60000
int g_record_asr_input = 0;
extern share_context *g_mplayer_shmem;
url_player_t *g_url_player = NULL;
int g_silence_ota_query = 0;
static int g_asr_disable = 0;
static int g_record_testing = 0;
static sem_t g_wechatcall_recvdata_sem;

#define os_calloc(n, size) calloc((n), (size))
#define os_power_on_tick() tickSincePowerOn()

#define PLAYER_MANAGER_DEBUG(fmt, ...)                                                             \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d][Player Manager] " fmt,                              \
                (int)(os_power_on_tick() / 3600000), (int)(os_power_on_tick() % 3600000 / 60000),  \
                (int)(os_power_on_tick() % 60000 / 1000), (int)(os_power_on_tick() % 1000),        \
                ##__VA_ARGS__);                                                                    \
    } while (0)

enum {
    e_asr_start,
    e_asr_stop,
    e_asr_finished,
};

extern void XiaoweiLoginInit();

void _XIAOWEI_ON_REQUEST_CMD_CALLBACK(const char *voice_id, int xiaowei_err_code, const char *json);

void sigroutine(int dunno)
{
    printf("weixin asr_tts get a signal -- %d \n", dunno);
    switch (dunno) {
    case SIGHUP:
    case SIGINT:
    case SIGQUIT:
    case SIGTERM:
        exit(1);
    default:
        printf("xiaowei get a default signal %d\n", dunno);
        break;
    }

    return;
}

void signal_handle(void)
{
    signal(SIGINT, sigroutine);
    signal(SIGTERM, sigroutine);
    signal(SIGKILL, sigroutine);
    signal(SIGALRM, sigroutine);
    signal(SIGQUIT, sigroutine);
}

void wiimu_shm_init(void) { g_wiimu_shm = WiimuContextGet(); }

void system_global_init(void)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    system("mkdir -p /vendor/weixin");
    g_wiimu_shm->asr_ongoing = 0;
    WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 100, SNDSRC_ASR);

    if (lpcfg_init() != 0) {
        printf("Fatal: asr_tts failed to init lpcfg\n");
        return -1;
    }
}

void AudioCapReset(int enable)
{
    if (enable) {
        CAIMan_deinit();
        CAIMan_YD_stop();
        CAIMan_ASR_stop();

        CAIMan_init();
        CAIMan_YD_start();
        CAIMan_ASR_start();
        CAIMan_YD_enable();
    } else {
        CAIMan_deinit();
        CAIMan_YD_stop();
        CAIMan_ASR_stop();
    }
    g_wiimu_shm->asr_ongoing = 0;
    g_asr_disable = 0;
}

int asr_config_get(char *param, char *value) { return wm_db_get(param, value); }

int asr_config_set(char *param, char *value) { return wm_db_set(param, value); }

int asr_config_set_sync(char *param, char *value) { return wm_db_set_commit(param, value); }

void asr_config_sync(void) { wm_db_commit(); }

int userLog(int level, const char *tag, const char *filename, int line, const char *funcname,
            const char *data)
{
#if 0
    if (level >= g_xiaowei_debug_level) {
        wiimu_log(1, 0, 0, 0,
                  "level = %d, tag = %s, filename = %s, line = %d, funcname = %s, data = %s\n",
                  level, tag, filename, line, funcname, data);
    }
#endif
    return 1;
}

void setLogFunc(void)
{
    char tmp_buf[128] = {0};
    int tmp_debug_level = 4;
    wm_db_get(NVRAM_ASR_XIAOWEI_LOG_LEVEL, tmp_buf);
    if (tmp_buf && strlen(tmp_buf) > 0) {
        if (tmp_buf[0] == '1') {
            tmp_debug_level = 1;
        } else if (tmp_buf[0] == '2') {
            tmp_debug_level = 2;
        } else if (tmp_buf[0] == '3') {
            tmp_debug_level = 3;
        } else if (tmp_buf[0] == '4') {
            tmp_debug_level = 4;
        } else if (tmp_buf[0] == '5') {
            tmp_debug_level = 5;
        } else {
            tmp_debug_level = 4;
        }
    }
    device_set_log_func(userLog, tmp_debug_level, true, false);
}

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

void set_duplex_mode(int duplex_mode)
{
    pthread_mutex_lock(&duplex_mode_mutex);
    g_duplex_mode = duplex_mode;
    pthread_mutex_unlock(&duplex_mode_mutex);
}

int get_duplex_mode(void)
{
    int tmp_duplex_mode;
    pthread_mutex_lock(&duplex_mode_mutex);
    tmp_duplex_mode = g_duplex_mode;
    pthread_mutex_unlock(&duplex_mode_mutex);
    return tmp_duplex_mode;
}

void set_hfp_status(int hfp_status)
{
    pthread_mutex_lock(&hfp_status_mutex);
    g_hfp_status = hfp_status;
    pthread_mutex_unlock(&hfp_status_mutex);
}

int get_hfp_status(void)
{
    int tmp_hfp_status;
    pthread_mutex_lock(&hfp_status_mutex);
    tmp_hfp_status = g_hfp_status;
    pthread_mutex_unlock(&hfp_status_mutex);
    return tmp_hfp_status;
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

void switch_content_to_background(int val)
{
    g_wiimu_shm->asr_status = val ? 1 : 0;

    if (val) {
        WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 4, SNDSRC_ASR);
    } else {
        WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 100, SNDSRC_ASR);
    }
}

void asr_set_audio_resume(void)
{
    switch_content_to_background(0);
    g_wiimu_shm->asr_ongoing = 0;
    WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 100, SNDSRC_ASR);
}

void set_tts_download_finished(void) { set_query_status(e_query_idle); }

void xiaoweiRingBufferInit(void) { g_ring_buffer = RingBufferCreate(0); }
#ifdef VOIP_SUPPORT
void xiaoweiWechatcallRingBufferInit(void) { g_wechatcall_ring_buffer = RingBufferCreate(0); }
#endif
void asr_set_audio_silence(void) { WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 0, SNDSRC_ASR); }

static int do_talkstart_prepare(int req_from)
{
    g_wiimu_shm->asr_ongoing = 1;

    asr_set_audio_silence();

    return 0;
}

void block_voice_prompt(char *path, int block)
{
    char file_vol[256];
    int volume = 100;
    int tmp_volume = g_wiimu_shm->volume;
    char *is_start_voice = NULL;

    // printf(">>>>init value=%d\n",g_wiimu_shm->volume);
    if (g_wiimu_shm->max_prompt_volume && g_wiimu_shm->volume >= g_wiimu_shm->max_prompt_volume)
        volume = 100 * g_wiimu_shm->max_prompt_volume / g_wiimu_shm->volume;
    strcpy(file_vol, path);
    if (!volume_prompt_exist(path)) {
        strcpy(file_vol, "common/ring2.mp3");
    }

    if (far_field_judge(g_wiimu_shm)) {
        volume = g_wiimu_shm->volume;
        if (volume < 50)
            volume = 100;
        else
            volume = 100 - (volume * 50) / 100;
        volume_pormpt_direct(file_vol, block, volume);
    } else if (volume == 100) {
        volume_pormpt_direct(file_vol, block, 0);
    } else {
        volume_pormpt_direct(file_vol, block, volume);
    }
}

// ALARM handle
#define XIAOWEI_ALARM_COUNT_MAX 20
static pthread_cond_t g_alarm_active_condition;
static pthread_mutex_t mutex_alarm;

void xiaowei_start_alarm(void)
{
    g_wiimu_shm->xiaowei_alarm_start = 1;
    pthread_mutex_lock(&mutex_alarm);
    pthread_cond_signal(&g_alarm_active_condition);
    pthread_mutex_unlock(&mutex_alarm);
}

void xiaowei_stop_alarm(void)
{
    g_wiimu_shm->xiaowei_alarm_start = 0;
    if (GetPidbyName("smplayer")) {
        killPidbyName("smplayer");
    }
    usleep(200000);
}

int xiaowei_alarm_get_remind(char *remind_voice_id, char *remind_clock_id)
{
    char tmp_param[64] = {0};
    int ret = 0;

    if (NULL == remind_voice_id || NULL == remind_clock_id) {
        return 1;
    }
    wiimu_log(1, 0, 0, 0, "%s,voice_id=%s,clock_id=%s ++++++", __func__, remind_voice_id,
              remind_clock_id);
    snprintf(tmp_param, 64, "{\"clock_id\":%ld}", atol(remind_clock_id));

    ret = xiaowei_request_cmd(remind_voice_id, "ALARM", "get_timing_task", tmp_param,
                              _XIAOWEI_ON_REQUEST_CMD_CALLBACK);
    // CleanSilentOTAFlag(g_wiimu_shm);
    wiimu_log(1, 0, 0, 0, "%s ---tmp_param=%s---", __func__, tmp_param);
}

void *xiaowei_alarm_handle_thread(void *arg)
{
    int ret = 0;
    int alarm_count = 0;
    while (1) {
        pthread_mutex_lock(&mutex_alarm);
        ret = pthread_cond_wait(&g_alarm_active_condition, &mutex_alarm);
        pthread_mutex_unlock(&mutex_alarm);
        set_asr_led(4);
        xiaowei_player_interrupt_enter();
        while ((alarm_count < XIAOWEI_ALARM_COUNT_MAX) && (g_wiimu_shm->xiaowei_alarm_start)) {
            {
                // printf("%s:internet=%d\n",__func__,g_wiimu_shm->internet);
                block_voice_prompt("common/alarm_sound.mp3", 1);
            }
            alarm_count++;
        }
        xiaowei_player_interrupt_exit();
        if (g_wiimu_shm->wechatcall_status == e_wechatcall_talking) {
            set_asr_led(2);
        } else if (g_wiimu_shm->wechatcall_status == e_wechatcall_idle) {
            set_asr_led(1);
        }
        printf("%s:exit alarm loop\n", __func__);
        g_wiimu_shm->xiaowei_alarm_start = 0;
        alarm_count = 0;
        // set_asr_led(9);
    }
}

// alarm handle end

void xiaoweiInitRecordData(void) { RingBufferReset(g_ring_buffer); }

#ifdef VOIP_SUPPORT
void xiaoweiWechatcallInitRecordData(void) { RingBufferReset(g_wechatcall_ring_buffer); }
#endif

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
#ifdef VOIP_SUPPORT
int xiaoweiWechatcallReadRecordData(char *buf, unsigned int len)
{
    ring_buffer_t *ring_buffer = g_wechatcall_ring_buffer;
    int read_len = 0;
    int readable_len = RingBufferReadableLen(ring_buffer);
    int finished = RingBufferGetFinished(ring_buffer);
    while (!finished && (readable_len < len)) {
        readable_len = RingBufferReadableLen(ring_buffer);
        finished = RingBufferGetFinished(ring_buffer);
        usleep(2000);
        if (g_wiimu_shm->privacy_mode) {
            wiimu_log(1, 0, 0, 0, "%s:mic is muted,exit\n", __func__);
            break;
        }
    }

    read_len = RingBufferRead(ring_buffer, buf, len);
    if (read_len == len) {
#ifdef WECHATCALL_DEBUG
        wiimu_log(1, 0, 0, 0, "%s:readlen=%d is ok\n", __func__, read_len);
#endif
    } else {
        printf("%s:readlen=%d is something wrong\n", __func__, read_len);
    }
    return read_len;
}
#endif
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

#ifdef VOIP_SUPPORT
int xiaoweiWechatcallWriteRecordData(char *data, size_t size)
{
    size_t less_len = size;
    while (less_len > 0) {

        int write_len = RingBufferWrite(g_wechatcall_ring_buffer, data, less_len);
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
#endif

void xiaowei_voice_request(void *arg)
{
    int ret = -1;
    int filled = 0;
    char buffer[MAX_TX_AI_FILL_DATA_LEN] = {0};
    unsigned int size = 0;
    int upload_timeout_flag = 0;
    int tmp_request_mode = 0;

    if (1 == get_duplex_mode()) {
        context.silent_timeout = XIAOWEI_DUPLEX_SPEAK_TIMEOUT;
        tmp_request_mode = 1;
    } else {
        context.silent_timeout = XIAOWEI_SILENT_TIMEOUT;
    }
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
    while (1) {
        memset(buffer, 0, MAX_TX_AI_FILL_DATA_LEN);
        size = xiaoweiReadRecordData(buffer, MAX_TX_AI_FILL_DATA_LEN);
        if (1 == tmp_request_mode) // duplex mode
        {
            if (get_query_status() == e_query_idle) {
                tmp_request_mode = 0;
                upload_timeout_flag = 0;
                wiimu_log(1, 0, 0, 0, "xiaowei request duplex mode end\n");
                break;
            }
            if (size > MIN_TX_AI_FILL_DATA_LEN) {
                if (g_first_wakeup == 1) {
                    g_first_wakeup = 0;
                    context.voice_request_begin = true;
                    wiimu_log(1, 0, 0, 0, "%s:first packet\n", __func__);
                }
                ret = xiaowei_request(voice_id, xiaowei_chat_via_voice, buffer, size, &context);
            }
        } else // normal mode
        {
            if (time(NULL) - time_begin >= XIAOWEI_ASR_UPLOAD_TIMEOUT) {
                wiimu_log(1, 0, 0, 0, "xiaowei asr data upload timeout\n");
                upload_timeout_flag = 1;
                break;
            }
            if ((get_query_status() == e_query_idle) ||
                ((get_query_status() == e_query_tts_start))) {
                wiimu_log(1, 0, 0, 0, "%s:status=%d\n", __func__, get_query_status());
                upload_timeout_flag = 0;
                break;
            }
            if (size > MIN_TX_AI_FILL_DATA_LEN) {
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

    if (1 == upload_timeout_flag) {
        wiimu_log(1, 0, 0, 0, "%s:voice request thread exit because of request timeout\n",
                  __func__);
        asr_set_audio_resume();
        set_asr_led(1);
    }
    CAIMan_ASR_disable();
    wiimu_log(1, 0, 0, 0, "voice request end");
    // return ret;
}

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
void xiaowei_player_interrupt_enter(void)
{
    int tmp_play_status = 0;
    tmp_play_status = url_player_get_play_status(g_url_player);
    if (1 == tmp_play_status || 2 == tmp_play_status) {
        pthread_mutex_lock(&xiaowei_player_resume_status_mutex);
        if (!g_xiaowei_player_need_resume) {
            g_xiaowei_player_need_resume = 1;
        }
        pthread_mutex_unlock(&xiaowei_player_resume_status_mutex);
        url_player_set_cmd(g_url_player, e_player_cmd_pause, 0);
    }
    OnCancel();
}
void xiaowei_player_interrupt_exit(void)
{
    int tmp_xiaowei_player_need_resume;
    pthread_mutex_lock(&xiaowei_player_resume_status_mutex);
    tmp_xiaowei_player_need_resume = g_xiaowei_player_need_resume;
    if (g_xiaowei_player_need_resume) {
        g_xiaowei_player_need_resume = 0;
    }
    pthread_mutex_unlock(&xiaowei_player_resume_status_mutex);
    if (tmp_xiaowei_player_need_resume) {
        url_player_set_cmd(g_url_player, e_player_cmd_resume, 0);
    }
    asr_set_audio_resume();
}
#ifdef VOIP_SUPPORT
void xiaowei_wechatcall_send_handle(void *arg)
{
    int ret = -1;
    char send_buffer[WECHATCALL_BUF_SIZE] = {0};
    int tmp_send_count = 50;

    int send_len = g_wiimu_shm->wechatcall_sampleleninms * 32;
    wiimu_log(1, 0, 0, 0, "------wechatcall_sampleleninms=%d,samplerate=%d,send_len=%d\n",
              g_wiimu_shm->wechatcall_sampleleninms, g_wiimu_shm->wechatcall_samplate, send_len);
#ifdef WECHATCALL_DEBUG_SEND
    FILE *fupload = fopen("/tmp/web/wechatcall_upload.pcm", "ab+");
#endif

    while (g_wiimu_shm->wechatcall_status == e_wechatcall_talking) {
        if (g_wiimu_shm->wechatcall_sampleleninms > 0) {
            send_len = g_wiimu_shm->wechatcall_sampleleninms * 32;
            if (g_wiimu_shm->privacy_mode) {
                memset(send_buffer, 0, WECHATCALL_BUF_SIZE);

                ret = voip_send_audio_data(send_buffer, send_len, 100);
                // wiimu_log(1, 0, 0, 0, "------send_len=%d,ret=%d\n", send_len,ret);
                usleep(g_wiimu_shm->wechatcall_sampleleninms * 1000);
            } else {
                ret = xiaoweiWechatcallReadRecordData(send_buffer, send_len);
                ret = voip_send_audio_data(send_buffer, send_len, 100);
            }
// wiimu_log(1, 0, 0, 0, "voip_send_audio_data ret=%d\n", ret);
#ifdef WECHATCALL_DEBUG_SEND
            if (fupload) {
                fwrite(send_buffer, send_len, 1, fupload);
            }
#endif
        } else {
            wiimu_log(1, 0, 0, 0, "%s:send_len=%d,wait voip param comes\n", __func__, send_len);
            usleep(100000);
            tmp_send_count--;
            if (tmp_send_count <= 0) {
                g_wiimu_shm->wechatcall_status = e_wechatcall_idle;
                wiimu_log(1, 0, 0, 0, "%s:wechatcall_sampleleninms=%d,param error,exit\n", __func__,
                          g_wiimu_shm->wechatcall_sampleleninms);
                break;
            }
        }
    }
    g_wiimu_shm->wechatcall_sampleleninms = 0;
    // CAIMan_ASR_disable();
    ASR_Disable_VOIP();
    wiimu_log(1, 0, 0, 0, "xiaowei_wechatcall_send_handle end\n");
}

int wechatcall_settimer_handle(int delayms)
{
    struct itimerval tick;

    signal(SIGALRM, xiaowei_wechatcall_recv_func);
    memset(&tick, 0, sizeof(tick));

    // Timeout to run first time
    tick.it_value.tv_sec = 0;
    tick.it_value.tv_usec = delayms * 1000;

    // After first, the Interval time for clock
    tick.it_interval.tv_sec = 0;
    tick.it_interval.tv_usec = delayms * 1000;

    if (setitimer(ITIMER_REAL, &tick, NULL) < 0) {

        printf("Set timer failed!\n");
        return -1;
    }
    return 0;
}

void wechatcall_uninittimer_handle(void)
{
    struct itimerval tick;

    memset(&tick, 0, sizeof(tick));

    if (setitimer(ITIMER_REAL, &tick, NULL) < 0)
        printf("Set timer failed!\n");
    wiimu_log(1, 0, 0, 0, "uninit real timer\n");
}

void xiaowei_wechatcall_recv_func(void) { sem_post(&g_wechatcall_recvdata_sem); }

void xiaowei_wechatcall_recv_handle(void *arg)
{
    int ret = -1;
    char recv_buffer[WECHATCALL_BUF_SIZE] = {0};
    int tmp_recv_count = 50;
    int set_timer_flag = 0;
    int read_len = 0;
    sem_init(&g_wechatcall_recvdata_sem, 0, 0);

    while (g_wiimu_shm->wechatcall_status == e_wechatcall_talking) {
        if (g_wiimu_shm->wechatcall_sampleleninms > 0) {
            if (0 == set_timer_flag) {
                set_timer_flag = 1;
                // wechatcall_uninittimer_handle();
                wechatcall_settimer_handle(g_wiimu_shm->wechatcall_sampleleninms);
            }
            read_len = g_wiimu_shm->wechatcall_sampleleninms * 32;
            sem_wait(&g_wechatcall_recvdata_sem);
            // wiimu_log(1, 0, 0, 0, "voip_get_audio_data before-----\n");
            ret = voip_get_audio_data(recv_buffer, read_len);
            // wiimu_log(1, 0, 0, 0, "voip_get_audio_data after ret=%d,buf[0]=%d++++++\n", ret,
            // recv_buffer[0]);
            wechatcall_pcm_play(recv_buffer, read_len);
        } else {
            wiimu_log(1, 0, 0, 0, "%s:wait voip param comes\n", __func__);
            usleep(100000);
            tmp_recv_count--;
            if (tmp_recv_count <= 0) {
                g_wiimu_shm->wechatcall_status = e_wechatcall_idle;
                wiimu_log(1, 0, 0, 0, "%s:wechatcall_sampleleninms=%d,param error,exit\n", __func__,
                          g_wiimu_shm->wechatcall_sampleleninms);
                break;
            }
        }
    }
    wechatcall_pcm_play_finish();
    wechatcall_uninittimer_handle();
    g_wiimu_shm->wechatcall_sampleleninms = 0;

    wiimu_log(1, 0, 0, 0, "xiaowei_wechatcall_recv_handle end");
}

void xiaowei_wechatcall_playring_handle(void *arg)
{
    int wechatcall_ring_count = 0;
    set_asr_led(4);
    char rep[32];
    NotifyMessageACKBlockIng(BT_MANAGER_SOCKET, "btdisconnectall", rep, 32);
    xiaowei_player_interrupt_enter();
    while ((g_wiimu_shm->wechatcall_status == e_wechatcall_request) ||
           (g_wiimu_shm->wechatcall_status == e_wechatcall_invited)) {
        block_voice_prompt("cn/phonering.mp3", 1);
        wechatcall_ring_count++;
        if (wechatcall_ring_count >= 30) {
            wiimu_log(1, 0, 0, 0, "xiaowei_wechatcall ring count > 30 times\n");
            g_wiimu_shm->wechatcall_status = e_wechatcall_idle;
            voip_hangup();
            xiaowei_player_interrupt_exit();
            break;
        }
    }
    if (g_wiimu_shm->wechatcall_status != e_wechatcall_talking) {
        set_asr_led(1);
    }
    wiimu_log(1, 0, 0, 0, "xiaowei_wechatcall_playring_handle end");
}

int voiceWechatcallThread(void)
{
    int res;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    res = pthread_create(&g_wechatcall_send_thread, &attr, xiaowei_wechatcall_send_handle, NULL);
    if (res) {
        wiimu_log(1, 0, 0, 0, "g_wechatcall_send_thread create fail\n");
    }

    res = pthread_create(&g_wechatcall_recv_thread, &attr, xiaowei_wechatcall_recv_handle, NULL);
    if (res) {
        wiimu_log(1, 0, 0, 0, "g_wechatcall_recv_thread create fail\n");
    }
    return res;
}

int WechatcallRingPlayThread(void)
{
    int res;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    res = pthread_create(&g_wechatcall_ring_play_thread, &attr, xiaowei_wechatcall_playring_handle,
                         NULL);
    if (res) {
        wiimu_log(1, 0, 0, 0, "g_wechatcall_ring_play_thread create fail\n");
    }
    return res;
}

void stopWechatcallThread(void)
{
    // stop wechatcall send thread
    if (g_wechatcall_send_thread != 0) {
        g_wiimu_shm->wechatcall_status = e_wechatcall_idle;
        pthread_join(g_wechatcall_send_thread, NULL);
        g_wechatcall_send_thread = 0;
    }
    // stop wechatcall recv thread
    if (g_wechatcall_recv_thread != 0) {
        g_wiimu_shm->wechatcall_status = e_wechatcall_idle;
        pthread_join(g_wechatcall_recv_thread, NULL);
        g_wechatcall_recv_thread = 0;
    }
}

void do_wechatcall(void)
{

    if (GetPidbyName("smplayer"))
        killPidbyName("smplayer");
    asr_set_audio_resume();
    xiaoweiWechatcallInitRecordData();
    // CAIMan_ASR_enable(0);
    ASR_Enable_VOIP();
    wechatcall_pcm_play_start();
    voiceWechatcallThread();
}

#endif

void onWakeUp(int req_from)
{
    memset(&context, 0, sizeof(context));
    context.wakeup_type = xiaowei_wakeup_type_local;
    {
        if (1 == req_from) {
#if defined(ASR_WAKEUP_TEXT_UPLOAD)
            context.wakeup_type = xiaowei_wakeup_type_local_with_text;
#ifdef A98CLEER_512M
            context.wakeup_word = "小微";
#else
            context.wakeup_word = "小微小微";
#endif
#endif
        }
    }
    if (1 == get_duplex_mode()) // duplex mode
    {
        if (!g_wiimu_shm->wechatcall_status) {
            context.wakeup_type = xiaowei_wakeup_type_local_with_free;
        }
    }
    wiimu_log(1, 0, 0, 0, "%s:wakeup_type=%d\n", __func__, context.wakeup_type);
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
    printf("%s:wakeup_type=%d,profile=%d,id=%s\n", __func__, context.wakeup_type,
           context.wakeup_profile, context.id);
    context.voice_request_begin = true;
    context.wakeup_word = NULL;
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

int OnCancel(void)
{
    int tmp_record_cancel = 0;
    wiimu_log(1, 0, 0, 0, "%s:query status=%d\n", __func__, get_query_status());

    avs_player_buffer_stop(NULL, e_asr_stop);
    if (g_wakeup_thread != 0) {
        pthread_join(g_wakeup_thread, NULL);
        g_wakeup_thread = 0;
        printf("OnCancel a_thread\n");
    }
    return tmp_record_cancel;
}

/*
* return 0 for ok,  others for fail reason
*/
static ASR_ERROR_CODE do_talkstart_check(int req_from)
{
    if (g_asr_disable) {
        return ERR_ASR_MODULE_DISABLED;
    }

    if (g_wiimu_shm->privacy_mode) {
        return ERR_ASR_MODULE_DISABLED;
    }

    if (g_wiimu_shm->internet == 0 ||
        (g_wiimu_shm->netstat != NET_CONN && g_wiimu_shm->Ethernet == 0)) {
        return ERR_ASR_NO_INTERNET;
    }
    if (0 == g_wiimu_shm->xiaowei_online_status || 0 == g_wiimu_shm->xiaowei_login_flag) {
        return ERR_ASR_USER_LOGIN_FAIL;
    }

    if (g_wiimu_shm->slave_notified) {
        return ERR_ASR_DEVICE_IS_SLAVE;
    }
    return 0;
}

static int do_talkstop(int errcode)
{

    printf("do_talkstop %d +++++++\n", errcode);

    if (errcode == ERR_ASR_NO_INTERNET) {
        if (strcmp(g_wiimu_shm->language, "zh_cn") == 0) {
#ifdef MARSHALL_XIAOWEI
            block_voice_prompt("cn/network_trouble.mp3", 1);
            set_asr_led(1);
#else
#ifdef YD_XIAOWEI_HOBOT_WWE_MODULE
            // Disable mic data flow into WWE when playing voice prompt
            // This is a workaround for false acception that
            // "小微尚未联网" triggers wake word engine.
            CAIMan_YD_disable();
#endif // end of YD_XIAOWEI_HOBOT_WWE_MODULE

            block_voice_prompt("cn/network_trouble.mp3", 1);

#ifdef YD_XIAOWEI_HOBOT_WWE_MODULE
            CAIMan_YD_enable();
#endif // end of YD_XIAOWEI_HOBOT_WWE_MODULE

#endif
        } else
            block_voice_prompt("us/internet_lost.mp3", 0);
        asr_set_audio_resume();
    } else if (errcode == ERR_ASR_LISTEN_NONE) {
        if (strcmp(g_wiimu_shm->language, "zh_cn") == 0)
            block_voice_prompt("cn/CN_beg_pardon.mp3", 0);
        else
            block_voice_prompt("us/beg_pardon.mp3", 0);
        asr_set_audio_resume();
    } else if (errcode == ERR_ASR_MODULE_DISABLED) {
        CAIMan_ASR_disable();
        // block_voice_prompt("common/ring2.mp3", 0);
        asr_set_audio_resume();
    } else if (errcode == ERR_ASR_DEVICE_IS_SLAVE) {
        CAIMan_ASR_disable();
        block_voice_prompt("common/ring2.mp3", 0);
        asr_set_audio_resume();
    } else if (errcode == ERR_ASR_NOTHING_PLAY) {
        CAIMan_ASR_disable();
        if (strcmp(g_wiimu_shm->language, "zh_cn") == 0)
            block_voice_prompt("cn/CN_nothing_play.mp3", 0);
        else
            block_voice_prompt("common/ring2.mp3", 0);
        asr_set_audio_resume();
    } else if (errcode == ERR_ASR_BOOK_SEARCH_FAIL) {
        CAIMan_ASR_disable();
        if (strcmp(g_wiimu_shm->language, "zh_cn") == 0) {
            if (volume_prompt_exist("cn/CN_book_search_fail.mp3"))
                block_voice_prompt("cn/CN_book_search_fail.mp3", 0);
            else
                block_voice_prompt("cn/CN_nothing_play.mp3", 0);
        } else
            block_voice_prompt("common/ring2.mp3", 0);
        asr_set_audio_resume();
    } else if (errcode == ERR_ASR_MUSIC_SEARCH_FAIL) {
        CAIMan_ASR_disable();
        if (strcmp(g_wiimu_shm->language, "zh_cn") == 0) {
            if (volume_prompt_exist("cn/CN_music_search_fail.mp3"))
                block_voice_prompt("cn/CN_music_search_fail.mp3", 0);
            else
                block_voice_prompt("cn/CN_nothing_play.mp3", 0);
        } else
            block_voice_prompt("common/ring2.mp3", 0);
        asr_set_audio_resume();
    } else if (errcode == ERR_ASR_SEARCHING_BEGIN) {
        CAIMan_ASR_disable();
        g_wiimu_shm->smplay_skip_silence = 1;
        if (strcmp(g_wiimu_shm->language, "zh_cn") == 0)
            block_voice_prompt("cn/CN_music_searching.mp3", 0);
        else
            block_voice_prompt("common/ring2.mp3", 0);
    } else if (errcode == ERR_ASR_NETWORK_LOW_STOP) {
        CAIMan_ASR_disable();
        if (strcmp(g_wiimu_shm->language, "zh_cn") == 0)
            block_voice_prompt("cn/CN_slow_connection.mp3", 0);
        else
            block_voice_prompt("us/slow_connection.mp3", 0);
        asr_set_audio_resume();
    } else if (errcode == ERR_ASR_USER_LOGIN_FAIL) {
        CAIMan_ASR_disable();
        block_voice_prompt("cn/not_login.mp3", 0);

    } else if (errcode == ERR_ASR_EXCEPTION) {
        CAIMan_ASR_disable();
        block_voice_prompt("common/ring2.mp3", 0);
        asr_set_audio_resume();
    } else if (errcode == ERR_ASR_MUSIC_STOP) {
        CAIMan_ASR_disable();
        // CAIMan_YD_enable();
    } else if (errcode == ERR_ASR_MUSIC_RESUME) {

    } else {
        CAIMan_ASR_disable();
        // asr_set_audio_resume();
    }

    // CAIMan_YD_enable();
    g_wiimu_shm->asr_ongoing = 0;
    // g_wiimu_shm->asr_pause_request = 0;
    // snd_pause_flag_set(0);
    printf("do_talkstop ------\n");

    return 0;
}

static void do_talkstart(int req_from)
{
    int ret;
    int is_cancel_in_record = 0;

    if (get_query_status() == e_query_record_start) {
        set_query_status(e_query_idle);
        wiimu_log(1, 0, 0, 0, "%s:record start,so cancel record handle\n", __func__);
        xiaowei_request_cancel(voice_id);
        memset(&context, 0, sizeof(context));
    } else if (1 == get_duplex_mode()) {
        set_query_status(e_query_idle);
        xiaowei_request_cancel(voice_id);
    }
    printf("do_talkstart at %d\n", (int)tickSincePowerOn());

    if (g_wiimu_shm->asr_trigger_test_on) {
        set_asr_led(5); // error led
        return;
    }

    if (g_xiaowei_talkstart_test) {
#ifdef XIAOWEI_TALKSTART_VOICE
        block_voice_prompt("cn/asr_start.mp3", 0);
#endif
        return;
    }
    ret = do_talkstart_check(req_from);
    if (ret) {
        if (ret != ERR_ASR_MODULE_DISABLED)
            set_asr_led(5); // error led
        do_talkstop(ret);
        // set_asr_led(1); //idle led
        return;
    }
    do_talkstart_prepare(req_from);
    OnCancel();

    set_asr_led(2);
#ifdef XIAOWEI_TALKSTART_VOICE
    if (g_wiimu_shm->wechatcall_status == e_wechatcall_request ||
        g_wiimu_shm->wechatcall_status == e_wechatcall_invited) {
        system("smplayer -f /system/workdir/misc/Voice-prompt/cn/asr_start.mp3 &");
    } else {
        block_voice_prompt("cn/asr_start.mp3", 0);
    }
#endif
    xiaoweiInitRecordData();
    CAIMan_ASR_enable(0);
    // CAIMan_YD_enable();
    onWakeUp(req_from);
    printf("talk to enable at %d\n", (int)tickSincePowerOn());
}

void duplex_talkstart(void)
{
    set_asr_led(2);
    xiaoweiInitRecordData();
    CAIMan_ASR_enable(0);
    // CAIMan_YD_enable();
    onWakeUp(5);
    printf("talk to enable at %d\n", (int)tickSincePowerOn());
}

static int ble_easy_setup_res(char *code)
{
    NotifyMessage(BT_MANAGER_SOCKET, code);
    return 0;
}

void weixin_bind_ticket_callback(int errCode, const char *ticket, unsigned int expireTime,
                                 unsigned int expire_in)
{
    char cmd[64];
    static int ticket_timeout_flag = 0;
    wiimu_log(1, 0, 0, 0, "%s:errorcode=%d,ticket=%s,expiretime=%u,expire_in=%u\n", __func__,
              errCode, ticket, expireTime, expire_in);
    if ((!errCode) && (ticket)) {
        ticket_timeout_flag = 0;
        strncpy(g_wiimu_shm->txqq_token, ticket, sizeof(g_wiimu_shm->txqq_token) - 1);
        // g_wiimu_shm->xiaowei_get_ticket_flag =1;

        snprintf(cmd, sizeof(cmd), "blecode:22:00");
        ble_easy_setup_res(cmd); // NETRES:DHCPOK
    } else if (0 != errCode)     // 35,maybe network is not good,call getticket func again.
    {
        wiimu_log(1, 0, 0, 0, "%s:ticket_timeout_flag=%d\n", __func__, ticket_timeout_flag);
        if (ticket_timeout_flag == 0) {
            SocketClientReadWriteMsg("/tmp/RequestASRTTS", "INTERNET_ACCESS:1",
                                     strlen("INTERNET_ACCESS:1"), NULL, NULL, 0);
        } else {
            ticket_timeout_flag = 0;
            snprintf(cmd, sizeof(cmd), "blenotifyticketfail");
            ble_easy_setup_res(cmd); // NETRES:DHCPOK
        }
        ticket_timeout_flag++;
    }
}

unsigned int get_xiaowei_ota_flag(void)
{
    // set xiaowei query ota server flag
    char tmp_ota_flag_buf[128] = {0};
    unsigned int tmp_xiaowei_ota_flag = 1;
    wm_db_get(NVRAM_ASR_XIAOWEI_OTA_DEBUG_FLAG, tmp_ota_flag_buf);
    if (tmp_ota_flag_buf && strlen(tmp_ota_flag_buf) > 0 && tmp_ota_flag_buf[0] == '2') {
        tmp_xiaowei_ota_flag = 2;
    } else {
#if defined(A98_MARSHALL_STEPIN) || defined(A98_PHILIPST)
        tmp_xiaowei_ota_flag = 2;
#else
        tmp_xiaowei_ota_flag = 1;
#endif
    }
    wiimu_log(1, 0, 0, 0, "%s:tmp_xiaowei_ota_flag=%d\n", __func__, tmp_xiaowei_ota_flag);
    return tmp_xiaowei_ota_flag;
}

void *evtSocket_thread(void *arg)
{
    int recv_num;
    char recv_buf[65536];
    int ret;

    SOCKET_CONTEXT msg_socket;
    msg_socket.path = "/tmp/RequestASRTTS";
    msg_socket.blocking = 1;

RE_INIT:
    ret = UnixSocketServer_Init(&msg_socket);
    if (ret <= 0) {
        g_wiimu_shm->asr_ready = 0;
        printf("ASRTTS UnixSocketServer_Init fail\n");
        sleep(5);
        goto RE_INIT;
    }

    while (1) {
        g_wiimu_shm->asr_ready = 1;
        ret = UnixSocketServer_accept(&msg_socket);
        if (ret <= 0) {
            printf("ASRTTS UnixSocketServer_accept fail\n");
            UnixSocketServer_deInit(&msg_socket);
            sleep(5);
            goto RE_INIT;
        } else {
            memset(recv_buf, 0x0, sizeof(recv_buf));
            recv_num = UnixSocketServer_read(&msg_socket, recv_buf, sizeof(recv_buf));
            if (recv_num <= 0) {
                printf("ASRTTS recv failed\r\n");
                if (recv_num < 0) {
                    UnixSocketServer_close(&msg_socket);
                    UnixSocketServer_deInit(&msg_socket);
                    sleep(5);
                    goto RE_INIT;
                }
            } else {
                recv_buf[recv_num] = '\0';

                wiimu_log(1, 0, 0, 0, "ASRTTS recv_buf %s +++++++++", recv_buf);
                if (strncmp(recv_buf, "CAP_RESET", strlen("CAP_RESET")) == 0) {
                    if (g_wiimu_shm->internet && !g_asr_disable)
                        AudioCapReset(1);
                    else
                        AudioCapReset(0);
                } else if (strncmp(recv_buf, "GNOTIFY=NET_CONNECTED",
                                   strlen("GNOTIFY=NET_CONNECTED")) == 0) {
                } else if (strncmp(recv_buf, "talkstart:", strlen("talkstart:")) == 0) {
                    int req_from = atoi(recv_buf + strlen("talkstart:"));
                    g_wiimu_shm->times_talk_on++;
#ifdef ENABLE_POWER_MONITOR
                    SocketClientReadWriteMsg("/tmp/Requesta01controller", "setSystemActive",
                                             strlen("setSystemActive"), NULL, NULL, 0);
#endif
                    if (g_wiimu_shm->privacy_mode) {
                        continue;
                    }
                    if (get_hfp_status()) {
                        continue;
                    }
// when in wechatcall request/invited/talking status,ignore talkstart event
// if (g_wiimu_shm->wechatcall_status) {
//    continue;
//}
#ifdef A98_PHILIPST
                    if (g_wiimu_shm->play_mode == PLAYER_MODE_LINEIN) {
                        continue;
                    }
#endif

                    CleanSilentOTAFlag(g_wiimu_shm);
                    if (ignore_reset_trigger) {
                        if (ignore_reset_trigger) {
                            ignore_reset_trigger = 0;
                        }
                        wiimu_log(1, 0, 0, 0, "ignore_talk_start ==\n");
                        continue;
                    }
                    wiimu_log(1, 0, 0, 0, "ASRTTS times_talk_on = %d +++++++++",
                              g_wiimu_shm->times_talk_on);
                    if (g_wiimu_shm->xiaowei_alarm_start) {
                        xiaowei_stop_alarm();
                    } else {
                        asr_dump_info.record_start_ts = 0;
                        asr_dump_info.record_end_ts = 0;
                        asr_dump_info.vad_ts = 0;
                        asr_dump_info.http_ret_code = 0;
                        do_talkstart(req_from);
                    }
                } else if (strncmp(recv_buf, "talkstop", strlen("talkstop")) == 0) {
                    set_asr_led(1);
                    OnCancel();
                    do_talkstop(ERR_ASR_NO_UNKNOWN);
                } else if (strncmp(recv_buf, "talkcontinue", strlen("talkcontinue")) == 0) {

                } else if (strncmp(recv_buf, "privacy_enable", strlen("privacy_enable")) == 0 ||
                           strncmp(recv_buf, "talksetPrivacyOn", strlen("talksetPrivacyOn") == 0)) {
                    if (!g_wiimu_shm->privacy_mode)
                        g_wiimu_shm->privacy_mode = 1;
                    SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=PRIVACY_ENA",
                                             strlen("GNOTIFY=PRIVACY_ENA"), NULL, NULL, 0);
                    asr_config_set(NVRAM_ASR_PRIVACY_MODE, "1");
                    asr_config_sync();
                } else if (strncmp(recv_buf, "privacy_disable", strlen("privacy_disable")) == 0 ||
                           strncmp(recv_buf, "talksetPrivacyOff",
                                   strlen("talksetPrivacyOff") == 0)) {
                    if (g_wiimu_shm->privacy_mode)
                        g_wiimu_shm->privacy_mode = 0;
                    SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=PRIVACY_DIS",
                                             strlen("GNOTIFY=PRIVACY_DIS"), NULL, NULL, 0);
                    asr_config_set(NVRAM_ASR_PRIVACY_MODE, "0");
                    asr_config_sync();
                } else if (strncmp(recv_buf, "change_record_path", strlen("change_record_path")) ==
                           0) {
                    int change_flag = atoi(recv_buf + strlen("change_record_path:"));
                    memset(recv_buf, 0, sizeof(recv_buf));
                    if (1 == change_flag) {
                        if (strlen(g_wiimu_shm->mmc_path) > 0) {
                            if (!asr_dump_info.record_to_sd) {
                                asr_dump_info.record_to_sd = 1;
                            }
                            strcat(recv_buf, "record to SD card!");
                        } else {
                            strcat(recv_buf, "Error: none sd card found");
                        }
                    } else {
                        if (asr_dump_info.record_to_sd) {
                            asr_dump_info.record_to_sd = 0;
                        }
                        strcat(recv_buf, "record to tmp!");
                    }
                    UnixSocketServer_write(&msg_socket, recv_buf, strlen(recv_buf));
                } else if (strncmp(recv_buf, "INTERNET_ACCESS:", strlen("INTERNET_ACCESS:")) == 0) {
                    int flag = atoi(recv_buf + strlen("INTERNET_ACCESS:"));
                    int ret = 0;
                    if (flag == 1) {
                        // XiaoweiLoginInit();
                        if (g_wiimu_shm->ezlink_ble_setup && g_wiimu_shm->xiaowei_login_flag) {
                            printf("------%s:before get ticket\n", __func__);
                            ret = device_get_bind_ticket(weixin_bind_ticket_callback);
                            printf("------%s:before get ticket\n", __func__);
                            if (ret) {
                                wiimu_log(1, 0, 0, 0, "%s:device_get_bind_ticket errorcode=%d\n",
                                          __func__, ret);
                            }
                        }
                    }
                } else if (strncmp(recv_buf, "setXiaoweiLogLevel:",
                                   strlen("setXiaoweiLogLevel:")) == 0) {
                    int tmp_debug_level = atoi(recv_buf + strlen("setXiaoweiLogLevel:"));
                    if (tmp_debug_level < 0 || tmp_debug_level > 5) {
                        printf("%s:debug level error value set\n", __func__);
                    } else {
                        if (tmp_debug_level == 1) {
                            asr_config_set(NVRAM_ASR_XIAOWEI_LOG_LEVEL, "1");
                        } else if (tmp_debug_level == 2) {
                            asr_config_set(NVRAM_ASR_XIAOWEI_LOG_LEVEL, "2");
                        } else if (tmp_debug_level == 3) {
                            asr_config_set(NVRAM_ASR_XIAOWEI_LOG_LEVEL, "3");
                        } else if (tmp_debug_level == 4) {
                            asr_config_set(NVRAM_ASR_XIAOWEI_LOG_LEVEL, "4");
                        } else if (tmp_debug_level == 5) {
                            asr_config_set(NVRAM_ASR_XIAOWEI_LOG_LEVEL, "5");
                        } else {
                            asr_config_set(NVRAM_ASR_XIAOWEI_LOG_LEVEL, "4");
                        }
                    }
                } else if (strncmp(recv_buf, "setXiaoweiWakeupCheckDebug:",
                                   strlen("setXiaoweiWakeupCheckDebug:")) == 0) {
                    int tmp_wakeup_check_debug =
                        atoi(recv_buf + strlen("setXiaoweiWakeupCheckDebug:"));
                    if (tmp_wakeup_check_debug < 0) {
                        printf("%s:setXiaoweiWakeupCheckDebug error value set\n", __func__);
                    } else {
                        g_xiaowei_wakeup_check_debug = tmp_wakeup_check_debug;
                    }
                } else if (strncmp(recv_buf, "xiaoweiTalkstartTest:",
                                   strlen("xiaoweiTalkstartTest:")) == 0) {
                    int tmp_talkstart_test = atoi(recv_buf + strlen("xiaoweiTalkstartTest:"));
                    if (tmp_talkstart_test < 0) {
                        printf("%s:xiaoweiTalkstartTest error value set\n", __func__);
                    } else {
                        g_xiaowei_talkstart_test = tmp_talkstart_test;
                    }
                } else if (strncmp(recv_buf, "USER_REQUEST:", strlen("USER_REQUEST:")) == 0) {
                    int asr_disable = atoi(recv_buf + strlen("USER_REQUEST:"));
                    g_asr_disable = asr_disable;
                    if (!g_record_testing) {
                        SocketClientReadWriteMsg("/tmp/RequestASRTTS", "CAP_RESET",
                                                 strlen("CAP_RESET"), NULL, NULL, 0);
                    }
                } else if (strncmp(recv_buf, "recordTest", strlen("recordTest")) == 0) {
                    int flag = atoi(recv_buf + strlen("recordTest:"));
                    g_record_testing = flag;
                    SocketClientReadWriteMsg("/tmp/Requesta01controller", "setPlayerCmd:stop",
                                             strlen("setPlayerCmd:stop"), NULL, NULL, 0);
                    system("killall -9 smplayer");
                    CAIMan_init();
                    if (flag) {
                        g_wiimu_shm->asr_ongoing = 1;
                        StartRecordTest();
                    } else {
                        StopRecordTest();
                        g_wiimu_shm->asr_ongoing = 0;
                    }
                } else if (strncmp(recv_buf, "recordAsrTest", strlen("recordAsrTest")) == 0) {
                    int flag = atoi(recv_buf + strlen("recordAsrTest:"));
                    if (flag == 1) {
                        remove("/tmp/web/dump_asr.pcm");
                        remove("/tmp/web/dump_cap.pcm");
                        remove("/tmp/dump_asr.pcm");
                        remove("/tmp/dump_cap.pcm");
                        sync();
                    }
                    g_record_asr_input = flag;
                    if (flag == 0) {
                        usleep(300000);
                        sync();
                        system("cp /tmp/dump_asr.pcm /tmp/web/dump_asr.pcm");
                        system("cp /tmp/dump_cap.pcm /tmp/web/dump_cap.pcm");
                        sync();
                    }
                } else if (strncmp(recv_buf, "recordFileStart:", strlen("recordFileStart:")) == 0) {
                    char *fileName = (recv_buf + strlen("recordFileStart:"));
                    g_record_testing = 1;

                    if (!IsCCaptStart()) {
                        CAIMan_init();
                    }
                    CCapt_Start_GeneralFile(fileName);
                } else if (strncmp(recv_buf, "recordFileStop", strlen("recordFileStop")) == 0) {
                    char buf[256];
                    memset(buf, 0x0, sizeof(buf));
                    g_record_testing = 0;
                    CCapt_Stop_GeneralFile(buf);
                    UnixSocketServer_write(&msg_socket, buf, strlen(buf));
                } else if (strncmp(recv_buf, "rmLastRecordFile", strlen("rmLastRecordFile")) == 0) {
                    if (strlen(asr_dump_info.last_rec_file) > 0) {
                        char cmdbuff[256] = {0};
                        sprintf(cmdbuff, "rm -rf %s", asr_dump_info.last_rec_file);
                        system(cmdbuff);
                        printf("rm file %s\r\n", cmdbuff);
                    }
                } else if (strncmp(recv_buf, "alarm_get_remind", strlen("alarm_get_remind")) == 0) {
                    char remind_voice_id[64];
                    char *pvoice_start, *pclock_start;
                    pvoice_start = strchr(recv_buf, ':') + 1;
                    pclock_start = strchr(pvoice_start, ':') + 1;
                    strncpy(remind_voice_id, pvoice_start, pclock_start - pvoice_start - 1);
                    remind_voice_id[pclock_start - pvoice_start - 1] = '\0';
                    OnCancel();
                    printf("%s,before xiaowei_alarm_get_remind \n", __func__);
                    xiaowei_alarm_get_remind(remind_voice_id, pclock_start);
                    if (g_wiimu_shm->xiaowei_alarm_start) {
                        xiaowei_stop_alarm();
                    }
                    xiaowei_start_alarm();
                } else if (strncmp(recv_buf, "xiaowei_alarm_stop", strlen("xiaowei_alarm_stop")) ==
                           0) {
                    xiaowei_stop_alarm();
                } else if (strncmp(recv_buf, "PlayEventNotify:", strlen("PlayEventNotify:")) == 0) {
                    char *ptr = recv_buf + strlen("PlayEventNotify:");
                    /*if (ptr && (strncmp(ptr, "forcestop", strlen("forcestop")) == 0)) {
                        g_xiaowei_report_state.play_state = play_state_stoped;
                        xiaowei_local_state_report(&g_xiaowei_report_state);
                    }*/
                    // qq_xiaowei_playevent_notification(ptr);
                } else if (strncmp(recv_buf, "VoiceLinkStart", strlen("VoiceLinkStart")) == 0) {
                    // xiaowei_start_voiceLink();
                } else if (strncmp(recv_buf, "VoiceLinkStop", strlen("VoiceLinkStop")) == 0) {
                    // xiaowei_stop_voiceLink();
                } else if (strncmp(recv_buf, "setFactoryTXQQID:", strlen("setFactoryTXQQID:")) ==
                           0) {
                    char buf[256];
                    char *str = recv_buf + strlen("setFactoryTXQQID:");
                    char *pid = NULL;
                    char *guid = NULL;
                    char *license = NULL;
                    char *appid = NULL;
                    char *key_ver = NULL;
                    int failed = 0;

                    sprintf(buf, "%s", "Failed");

                    pid = str;
                    str = strstr(str, ":");
                    if (str) {
                        str[0] = '\0';
                        str++;
                        guid = str;
                        str = strstr(str, ":");
                        if (str) {
                            str[0] = '\0';
                            str++;
                            appid = str;
                            str = strstr(str, ":");
                            if (str) {
                                str[0] = '\0';
                                str++;
                                license = str;
                                str = strstr(str, ":");
                                if (str) {
                                    str[0] = '\0';
                                    str++;
                                    key_ver = str;
                                    if ((strlen(license) < TXQQ_DEFAULT_LICENSE_LEN) &&
                                        (strlen(guid) < TXQQ_DEFAULT_GUID_LEN) &&
                                        (strlen(appid) < TXQQ_DEFAULT_APPID_LEN) &&
                                        (strlen(pid) < TXQQ_DEFAULT_PID_LEN) &&
                                        (strlen(key_ver) < TXQQ_DEFAULT_KEY_VER_LEN)) {
                                        strcpy(txqq_licence, license);
                                        strcpy(txqq_guid, guid);
                                        strcpy(txqq_product_id, pid);
                                        strcpy(txqq_appid, appid);
                                        strcpy(txqq_key_ver, key_ver);
                                        strcpy(g_wiimu_shm->txqq_guid, txqq_guid);
                                        strcpy(g_wiimu_shm->txqq_product_id, txqq_product_id);
                                        if (wm_db_set_commit_ex(FACOTRY_NVRAM, "txqq_pid", pid) !=
                                            0) {
                                            failed |= 1;
                                        }
                                        if (wm_db_set_commit_ex(FACOTRY_NVRAM, "txqq_license",
                                                                license) != 0) {
                                            failed |= 1;
                                        }
                                        if (wm_db_set_commit_ex(FACOTRY_NVRAM, "txqq_guid", guid) !=
                                            0) {
                                            failed |= 1;
                                        }
                                        if (wm_db_set_commit_ex(FACOTRY_NVRAM, "txqq_appid",
                                                                appid) != 0) {
                                            failed |= 1;
                                        }
                                        if (wm_db_set_commit_ex(FACOTRY_NVRAM, "txqq_key_ver",
                                                                key_ver) != 0) {
                                            failed |= 1;
                                        }
                                    } else {
                                        failed = 1;
                                    }
                                    if (!failed)
                                        sprintf(buf, "%s", "Success");
                                }
                            }
                        }
                    }
                    UnixSocketServer_write(&msg_socket, buf, strlen(buf));
                } else if (strncmp(recv_buf, "getFactoryTXQQID", strlen("getFactoryTXQQID")) == 0) {
                    char buf[512];

                    snprintf(buf, 512, "{\"pid\":\"%s\",\"guid\":\"%s\",\"license\":\"%s\","
                                       "\"appid\":\"%s\",\"key_ver\":\"%s\"}",
                             txqq_product_id, txqq_guid, txqq_licence, txqq_appid, txqq_key_ver);
                    UnixSocketServer_write(&msg_socket, buf, strlen(buf));
                } else if (strncmp(recv_buf, "setTXQQQuality:", strlen("setTXQQQuality:")) == 0) {

                } else if (strncmp(recv_buf, "query_xiaowei_ota_status",
                                   sizeof("query_xiaowei_ota_status")) == 0) {
                    int res = 0;
                    res = device_query_ota_update(NULL, 1, 3, get_xiaowei_ota_flag());
                    g_silence_ota_query = 1;
                    wiimu_log(1, 0, 0, 0, "device_query_ota_update return=%d\n", res);

                } else if (strncmp(recv_buf, "xiaowei_other_mode_state_report",
                                   sizeof("xiaowei_other_mode_state_report")) == 0) {
                    xiaowei_other_mode_state_report(&g_xiaowei_report_state);
                } else if (strncmp(recv_buf, "RESTOREFACTORY", sizeof("RESTOREFACTORY")) == 0) {
                    xiaowei_other_mode_state_report(&g_xiaowei_report_state);
                    url_player_set_cmd(g_url_player, e_player_cmd_stop, 0);
                    device_erase_all_binders();
                } else if (strncmp(recv_buf, "quit_wifi_playing_mode",
                                   sizeof("quit_wifi_playing_mode")) == 0) {
                    xiaowei_other_mode_state_report(&g_xiaowei_report_state);
                    url_player_set_cmd(g_url_player, e_player_cmd_clear_queue, 0);
                } else if (strncmp(recv_buf, "enter_wps_setup_mode",
                                   sizeof("enter_wps_setup_mode")) == 0) {
                    xiaowei_other_mode_state_report(&g_xiaowei_report_state);
                    url_player_set_cmd(g_url_player, e_player_cmd_stop, 0);
                } else if (strncmp(recv_buf, "setDisableForceOTA", strlen("setDisableForceOTA")) ==
                           0) {
                    int flag = atoi(recv_buf + strlen("setDisableForceOTA:"));
                    if (flag) {
                        asr_config_set(NVRAM_ASR_DIS_FORCE_OTA, "1");
                    } else {
                        asr_config_set(NVRAM_ASR_DIS_FORCE_OTA, "0");
                    }
                    asr_config_sync();
                } else if (strncmp(recv_buf, "setDisableAllOTA", strlen("setDisableAllOTA")) == 0) {
                    int flag = atoi(recv_buf + strlen("setDisableAllOTA:"));
                    if (flag) {
                        asr_config_set(NVRAM_ASR_DIS_ALL_OTA, "1");
                    } else {
                        asr_config_set(NVRAM_ASR_DIS_ALL_OTA, "0");
                    }
                    asr_config_sync();
                } else if (strncmp(recv_buf, "setXiaoweiDebugOTA", strlen("setXiaoweiDebugOTA")) ==
                           0) {
                    int flag = atoi(recv_buf + strlen("setXiaoweiDebugOTA:"));
                    if (2 == flag) {
                        asr_config_set(NVRAM_ASR_XIAOWEI_OTA_DEBUG_FLAG, "2");
                    } else {
                        asr_config_set(NVRAM_ASR_XIAOWEI_OTA_DEBUG_FLAG, "0");
                    }
                    asr_config_sync();
                } else if (strncmp(recv_buf, "alexadump", strlen("alexadump")) == 0) {
                    memset(recv_buf, 0, sizeof(recv_buf));
                } else if (strncmp(recv_buf, "NetworkDHCPNewIP", strlen("NetworkDHCPNewIP")) == 0) {
#ifdef AMLOGIC_A113
                    res_init();
#endif
                } else if (strncmp(recv_buf, "wechatcall_name:", strlen("wechatcall_name:")) == 0) {
                    char *user_name = recv_buf + strlen("wechatcall_name:");
                    voip_create_call(user_name, "", 2);
                    printf("----create call,username=%s\n", user_name);
                } else if (strncmp(recv_buf, "wechatcall_start:", strlen("wechatcall_start:")) ==
                           0) {
                    printf("----before start wechat call\n");
                    do_wechatcall();
                } else if (strncmp(recv_buf, "wechatcall_hangup", strlen("wechatcall_hangup")) ==
                           0) {
                    printf("----before hangup wechat call\n");
                    g_wiimu_shm->wechatcall_status = e_wechatcall_idle;
                    voip_hangup();
                    xiaowei_player_interrupt_exit();
                } else if (strncmp(recv_buf, "wechatcall_key_response",
                                   strlen("wechatcall_key_response")) == 0) {
                    if (g_wiimu_shm->wechatcall_status == e_wechatcall_invited) {
                        printf("----before answer wechat call\n");
                        voip_answer();
                    } else if ((g_wiimu_shm->wechatcall_status == e_wechatcall_request) ||
                               (g_wiimu_shm->wechatcall_status == e_wechatcall_talking)) {
                        g_wiimu_shm->wechatcall_status = e_wechatcall_idle;
                        voip_hangup();
                        xiaowei_player_interrupt_exit();
                    }
                } else if (strncmp(recv_buf, "xiaowei_report_state_no_refresh_offset",
                                   strlen("xiaowei_report_state_no_refresh_offset")) == 0) {
                    int ret = 0;
                    ret = xiaowei_report_state(&g_xiaowei_report_state);
                    if (ret) {
                        printf("%s:errcode=%d\n", __func__, ret);
                    }
                } else if (strncmp(recv_buf, "xiaowei_report_state_refresh_offset",
                                   strlen("xiaowei_report_state_refresh_offset")) == 0) {
                    int ret = 0;
                    ret = xiaowei_local_state_report(&g_xiaowei_report_state);
                    if (ret) {
                        printf("%s:errcode=%d\n", __func__, ret);
                    }
                } else if (strncmp(recv_buf, "volume_change_report",
                                   strlen("volume_change_report")) == 0) {
                    xiaowei_report_volume(g_wiimu_shm->volume);
                } else if (strncmp(recv_buf, "QQLocalCtl:", strlen("QQLocalCtl:")) == 0) {
                    char *ptr = strchr(recv_buf, ':') + 1;
                    if (strncmp(ptr, "next", strlen("next")) == 0) {
                        url_player_set_cmd(g_url_player, e_player_cmd_next, 0);
                    } else if (strncmp(ptr, "prev", strlen("prev")) == 0) {
                        url_player_set_cmd(g_url_player, e_player_cmd_prev, 0);
                    }
                }
#ifdef ENABLE_RECORD_SERVER
                else if (strncmp(recv_buf, "startRecordServer", strlen("startRecordServer")) == 0) {
                    int nChannel = 1;
                    char *pChannel = strstr(recv_buf, "startRecordServer:");

                    if (pChannel != NULL) {
                        pChannel += strlen("startRecordServer:");
                        nChannel = atoi(pChannel);
                    }

                    StartRecordServer(nChannel);
                } else if (strncmp(recv_buf, "stopRecordServer", strlen("stopRecordServer")) == 0) {
                    StopRecordServer();
                }
#endif
#ifdef BT_HFP_ENABLE
                else if (strncmp(recv_buf, "bluetooth_hfp_start_play",
                                 strlen("bluetooth_hfp_start_play")) == 0) {
                    if (g_wiimu_shm->wechatcall_status) {
                        g_wiimu_shm->wechatcall_status = e_wechatcall_idle;
                        voip_hangup();
                    }
                    set_hfp_status(1);
                    xiaowei_player_interrupt_enter();
                    bluetooth_hfp_start_play();
                } else if (strncmp(recv_buf, "bluetooth_hfp_stop_play",
                                   strlen("bluetooth_hfp_stop_play")) == 0) {
                    set_hfp_status(0);
                    xiaowei_player_interrupt_exit();
                    bluetooth_hfp_stop_play();
                }
#endif
                wiimu_log(1, 0, 0, 0, "ASRTTS recv_buf------");
            }

            UnixSocketServer_close(&msg_socket);
        }
    }

    pthread_exit("exit udpEvt thread");
}

int xiaowei_wakeup_engine_init(void)
{
#if defined(SOFT_PREP_MODULE)
    if (!CAIMan_soft_prep_init()) {
        printf("Soft PreProcess module init fail\n");
        return -1;
    }
#endif
    xiaoweiRingBufferInit();
#ifdef VOIP_SUPPORT
    xiaoweiWechatcallRingBufferInit();
#endif
    CAIMan_init();
    CAIMan_YD_start();
    CAIMan_ASR_start();
#ifdef VOIP_SUPPORT
    ASR_Start_VOIP();
#endif
    CAIMan_YD_enable();
    set_asr_led(1);

    return 0;
}

int xiaowei_socket_thread_init(void)
{
    int ret;
    pthread_t xiaowei_socket_thread;
    ret = pthread_create(&xiaowei_socket_thread, NULL, evtSocket_thread, NULL);
    if (ret) {
        wiimu_log(1, 0, 0, 0, "%s:asr_tts evtsocket_thread create fail\n", __func__);
        return -1;
    }
    return 0;
}

int xiaowei_alarm_thread_init(void)
{
    int ret;
    pthread_t xiaowei_alarm_thread;
    ret = pthread_create(&xiaowei_alarm_thread, NULL, xiaowei_alarm_handle_thread, NULL);
    if (ret) {
        wiimu_log(1, 0, 0, 0, "%s:xiaowei alarm handle thread create fail\n", __func__);
        return -1;
    }
    return 0;
}

int xiaowei_local_state_report(XIAOWEI_PARAM_STATE *_report_state)
{
    int ret = 0;
    if (_report_state->play_state != play_state_resume)
        _report_state->play_offset = get_play_pos();
    printf("------%s:into,offset=%llu,state=%d,play_id=%s\n", __func__, _report_state->play_offset,
           _report_state->play_state, _report_state->play_id);
    _report_state->volume = g_wiimu_shm->volume;
    ret = xiaowei_report_state(_report_state);
    if (ret) {
        printf("%s:errcode=%d\n", __func__, ret);
    }
}

int xiaowei_other_mode_state_report(XIAOWEI_PARAM_STATE *_report_state)
{
    int ret = 0;
    _report_state->play_state = play_state_idle;
    if (_report_state->skill_info.id) {
        free(_report_state->skill_info.id);
        _report_state->skill_info.id = NULL;
    }
    if (_report_state->skill_info.name) {
        free(_report_state->skill_info.name);
        _report_state->skill_info.name = NULL;
    }
    _report_state->play_offset = 0;
    ret = xiaowei_report_state(_report_state);
    if (ret) {
        printf("%s:errcode=%d\n", __func__, ret);
    }
    return ret;
}

int xiaowei_get_more_playlist(char *res_id, int is_up)
{
    char tmp_param[512] = {0};
    char voice_id[64] = {0};
    int ret = 0;

    if (NULL == res_id) {
        return 1;
    }
    wiimu_log(1, 0, 0, 0, "%s,res_id=%s ++++++\n", __func__, res_id);
    if (g_xiaowei_report_state.skill_info.id && g_xiaowei_report_state.skill_info.name) {
        wiimu_log(1, 0, 0, 0, "%s,g_xiaowei_report_state.skill_info.id=%s,g_xiaowei_report_state."
                              "skill_info.name=%s ++++++\n",
                  __func__, g_xiaowei_report_state.skill_info.id,
                  g_xiaowei_report_state.skill_info.name);
        if (is_up) {
            snprintf(tmp_param, 512, "{\"skill_info\": {\"id\": "
                                     "\"%s\",\"name\": "
                                     "\"%s\"},\"play_id\": \"%s\",\"is_up\": true}",
                     g_xiaowei_report_state.skill_info.id, g_xiaowei_report_state.skill_info.name,
                     res_id);
        } else {
            snprintf(tmp_param, 512, "{\"skill_info\": {\"id\": "
                                     "\"%s\",\"name\": "
                                     "\"%s\"},\"play_id\": \"%s\",\"is_up\": false}",
                     g_xiaowei_report_state.skill_info.id, g_xiaowei_report_state.skill_info.name,
                     res_id);
        }

        ret = xiaowei_request_cmd(voice_id, "PLAY_RESOURCE", "get_more", tmp_param, NULL);
    }
    // CleanSilentOTAFlag(g_wiimu_shm);
    wiimu_log(1, 0, 0, 0, "%s,voidid=%s ---tmp_param=%s---", __func__, voice_id, tmp_param);
    return ret;
}

int xiaowei_get_history_playlist(void)
{
    char tmp_param[512] = {0};
    char voice_id[64] = {0};
    int ret = 0;

    snprintf(tmp_param, 512, "{\"skill_info\": {\"id\": "
                             "\"8dab4796-fa37-4114-0011-7637fa2b0001\",\"name\": "
                             "\"音乐\"},\"play_id\": \"rd001zmy3Y3Evni9\",\"is_up\": false}");
    xiaowei_request_cmd(voice_id, "PLAY_RESOURCE", "get_history", tmp_param, NULL);
    wiimu_log(1, 0, 0, 0, "%s,voidid=%s ---tmp_param=%s---", __func__, voice_id, tmp_param);

    return ret;
}

int url_notify_cb(e_url_notify_t notify, url_stream_t *url_stream, void *ctx)
{
    int ret = 0;
    url_player_t *player_manager = (url_player_t *)ctx;

    if (!url_stream || !player_manager) {
        return -1;
    }

    PLAYER_MANAGER_DEBUG("--------url_notify=%d cur_pos=%lld token=%s--------\n", notify,
                         url_stream->cur_pos, url_stream->stream_id);

    switch (notify) {
    case e_notify_get_url:
        if (g_xiaowei_report_state.play_id)
            free(g_xiaowei_report_state.play_id);
        g_xiaowei_report_state.play_id = strdup(url_stream->stream_id);
        g_xiaowei_report_state.play_state = play_state_preload;
        g_xiaowei_report_state.play_offset = 0; // play started offset 0
        printf("---1---xiaowei_local_state_report:into,offset=%llu,state=%d,play_id=%s\n",
               g_xiaowei_report_state.play_offset, g_xiaowei_report_state.play_state,
               g_xiaowei_report_state.play_id);
        ret = xiaowei_report_state(&g_xiaowei_report_state);
        if (ret) {
            printf("%s:errcode=%d\n", __func__, ret);
        }
        break;
    case e_notify_play_started: {
        g_xiaowei_report_state.play_state = play_state_start;
        g_xiaowei_report_state.play_offset = 0; // play started offset 0
        printf("---1---xiaowei_local_state_report:into,offset=%llu,state=%d,play_id=%s\n",
               g_xiaowei_report_state.play_offset, g_xiaowei_report_state.play_state,
               g_xiaowei_report_state.play_id);
        ret = xiaowei_report_state(&g_xiaowei_report_state);
        if (ret) {
            printf("%s:errcode=%d\n", __func__, ret);
        }
    } break;
    case e_notify_play_stopped: {
        g_xiaowei_report_state.play_state = play_state_abort;
        xiaowei_local_state_report(&g_xiaowei_report_state);
    } break;
    case e_notify_play_paused: {
        g_xiaowei_report_state.play_state = play_state_paused;
        xiaowei_local_state_report(&g_xiaowei_report_state);
    } break;
    case e_notify_play_resumed: {
        g_xiaowei_report_state.play_state = play_state_resume;
        xiaowei_local_state_report(&g_xiaowei_report_state);
    } break;
    case e_notify_play_near_finished: {

    } break;
    case e_notify_play_finished: {
        g_xiaowei_report_state.play_state = play_state_finished;
        xiaowei_local_state_report(&g_xiaowei_report_state);
    } break;
    case e_notify_play_error: {
        g_xiaowei_report_state.play_state = play_state_err;
        xiaowei_refresh_music_id(url_stream->stream_id);
    } break;
    case e_notify_play_delay_passed: {
    } break;
    case e_notify_play_interval_passed: {
    } break;
    case e_notify_play_position_passed: {
    } break;
    case e_notify_play_end: {
    } break;
    case e_notify_play_need_more: {
        if (url_player_get_more_playlist(g_url_player))
            xiaowei_get_more_playlist(url_stream->stream_id, 0);
    } break;
    case e_notify_play_need_history: {
        if (url_player_get_history_playlist(g_url_player))
            xiaowei_get_history_playlist();
    } break;
    default: {
    } break;
    }

    return 0;
}

int xiaowei_url_player_init(void)
{
    g_xiaowei_report_state.play_mode = e_mode_playlist_loop; // default mode
    if (g_url_player == NULL) {
        g_url_player = (url_player_t *)os_calloc(1, sizeof(g_url_player));
        if (g_url_player) {
            g_url_player = url_player_create(url_notify_cb, g_url_player);
        }
    }
    set_player_mode(g_url_player, e_mode_playlist_loop);
    g_xiaowei_report_state.play_mode = play_mode_sequential_loop_playlist; // default mode
    return 0;
}

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

void _XIAOWEI_ON_REQUEST_CMD_CALLBACK(const char *voice_id, int xiaowei_err_code, const char *json)
{
    wiimu_log(1, 0, 0, 0, "%s:voice_id=%s,error_code=%d\n", __func__, voice_id, xiaowei_err_code);
    json_object *response_obj = NULL;
    json_object *res_grp_obj = NULL, *res_res_obj = NULL;
    json_object *tmp_grp = NULL, *tmp_res = NULL;
    int obj_grp_num = 0, obj_res_num = 0;
    int i = 0, j = 0;
    char *content_url = NULL, *_voice_id = NULL;
    json_object *content_obj = NULL;
    int format = 0;
    int play_behavior = 0;
    if (!json) {
        printf("%s,json null\n", __func__);
        goto exit;
    }
    response_obj = json_tokener_parse(json);
    if (!response_obj) {
        printf("%s,response_obj null\n", __func__);
        goto exit;
    }
    play_behavior = JSON_GET_INT_FROM_OBJECT(response_obj, "play_behavior");
    printf("%s:play_behavior=%d\n", __func__, play_behavior);
    res_grp_obj = json_object_object_get(response_obj, "resource_groups");
    if (!res_grp_obj) {
        printf("%s,res_grp_obj null\n", __func__);
        goto exit;
    }

    obj_grp_num = json_object_array_length(res_grp_obj);
    printf("%s,obj_grp_num =%d\n", __func__, obj_grp_num);
    for (i = 0; i < obj_grp_num; i++) {
        tmp_grp = json_object_array_get_idx(res_grp_obj, i);
        if (!tmp_grp) {
            printf("%s,tmp_grp null\n", __func__);
            goto exit;
        }
        res_res_obj = json_object_object_get(tmp_grp, "resources");
        if (!res_res_obj) {
            printf("%s,res_res_obj null\n", __func__);
            goto exit;
        }
        obj_res_num = json_object_array_length(res_res_obj);
        printf("%s,obj_res_num =%d\n", __func__, obj_res_num);
        for (j = 0; j < obj_res_num; j++) {
            tmp_res = json_object_array_get_idx(res_res_obj, j);
            if (tmp_res) {
                content_url = JSON_GET_STRING_FROM_OBJECT(tmp_res, "content");
                format = JSON_GET_INT_FROM_OBJECT(tmp_res, "format");
                _voice_id = JSON_GET_STRING_FROM_OBJECT(tmp_res, "id");
                if (content_url && _voice_id) {
                    printf("%s:content_url=%s,voice_id=%s\n", __func__, content_url, _voice_id);
                    if (play_behavior == 4) {
                        if (strlen(content_url) > 4) // workround for xiaowei refresh url
                                                     // error(sometimes get a url only have one
                                                     // char)
                        {
                            url_player_refresh_playid_url(g_url_player, _voice_id, content_url);
                        } else {
                            wiimu_log(1, 0, 0, 0, "%s:refresh url wrong,try next song\n", __func__);
                            url_player_set_cmd(g_url_player, e_player_cmd_next, 0);
                        }
                    } else {
                        if (format == 3) {
                            set_asr_led(4);
                            xiaowei_play_tts_url(content_url, voice_id);
                        }
                    }
                }
            }
        }
    }
exit:
    if (response_obj) {
        json_object_put(response_obj);
    }
}

int xiaowei_refresh_music_id(char *res_id)
{
    char tmp_voiceid[128];
    char tmp_param[256] = {0};

    snprintf(tmp_param, 256, "{\"skill_info\": {\"id\": "
                             "\"8dab4796-fa37-4114-0011-7637fa2b0001\",\"name\": "
                             "\"音乐\"},\"play_ids\": [\"%s\"]}",
             res_id);
    xiaowei_request_cmd(tmp_voiceid, "PLAY_RESOURCE", "refresh", tmp_param,
                        _XIAOWEI_ON_REQUEST_CMD_CALLBACK);
}

int xiaowei_url_player_set_play_url(char *id, char *content, int type, unsigned int offset)
{
    url_stream_t new_stream = {0};
    new_stream.stream_id = strdup(id);
    new_stream.play_pos = offset;
    new_stream.url = strdup(content);
    new_stream.behavior = type;
    url_player_insert_stream(g_url_player, &new_stream);
}

void xiaowei_wait_login(void)
{
    while (g_wiimu_shm->xiaowei_login_flag == 0)
        usleep(20000);
    wiimu_log(1, 0, 0, 0, (char *)"123g_wiimu_shm->xiaowei_login_flag=%d\n",
              g_wiimu_shm->xiaowei_login_flag);
}

void xiaowei_wait_internet(void)
{
    while (g_wiimu_shm->internet == 0)
        usleep(200000);
    wiimu_log(1, 0, 0, 0, (char *)"%:internet ok,will do xiaowei device_init\n", __func__);
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

int wechat_call_name_handle(XIAOWEI_PARAM_RESPONSE *_response)
{
    int ret = 0;
    if (_response->response_data) {
        json_object *response_data_obj = NULL;
        char *binder_user_name = NULL;
        char tmp_buf[256] = {0};
        wiimu_log(1, 0, 0, 0, (char *)"%s:response_data=%s\n", __func__, _response->response_data);
        response_data_obj = json_tokener_parse(_response->response_data);
        if (response_data_obj) {
            binder_user_name = JSON_GET_STRING_FROM_OBJECT(response_data_obj, "binder_username");
            if (binder_user_name) {
                sprintf(tmp_buf, "wechatcall_name:%s", binder_user_name);
                SocketClientReadWriteMsg("/tmp/RequestASRTTS", tmp_buf, strlen(tmp_buf), NULL, NULL,
                                         0);
            } else {
                wiimu_log(1, 0, 0, 0, (char *)"%s:get binder user name fail\n", __func__);
            }
            json_object_put(response_data_obj);
        } else {
            wiimu_log(1, 0, 0, 0, (char *)"%s:response_data is not a json struct\n", __func__);
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

int global_control_handle(XIAOWEI_PARAM_RESPONSE *_response)
{
    char volume_buf[32] = {0};
    g_wiimu_shm->xiaowei_control_id = 0;
    printf("------%s:control_id=%d,control_value=%s\n", __func__, _response->control_id,
           _response->control_value);
    if (xiaowei_cmd_pause == _response->control_id) {
        url_player_set_cmd(g_url_player, e_player_cmd_pause, 0);
    } else if (xiaowei_cmd_play == _response->control_id) {
        url_player_set_cmd(g_url_player, e_player_cmd_resume, 0);
    } else if (xiaowei_cmd_stop == _response->control_id) {
        url_player_set_cmd(g_url_player, e_player_cmd_stop, 0);
    } else if (xiaowei_cmd_play_prev == _response->control_id) {
        url_player_set_cmd(g_url_player, e_player_cmd_prev, 0);
    } else if (xiaowei_cmd_play_next == _response->control_id) {
        url_player_set_cmd(g_url_player, e_player_cmd_next, 0);
    } else if (xiaowei_cmd_replay == _response->control_id) {
        url_player_set_cmd(g_url_player, e_player_cmd_replay, 0);
    } else if (xiaowei_cmd_play_res_id == _response->control_id) {
        if (_response->control_value) {
            int set_index = -1;
            if (PLAYER_MODE_IS_ALEXA(g_wiimu_shm->play_mode)) {
                printf("%s:before get playlist index by res_id\n", __func__);
                set_index = url_player_get_playid_index(g_url_player, _response->control_value);
                if (set_index != -1) {
                    url_player_set_cmd(g_url_player, e_player_cmd_set_index, set_index);
                }
            }
        }
    } else if (xiaowei_cmd_offset_set == _response->control_id) {
        if (PLAYER_MODE_IS_ALEXA(g_wiimu_shm->play_mode)) {
            if (_response->control_value) {
                long long tmp_position = (long long)atol(_response->control_value);
                if (tmp_position < 0) {
                    tmp_position = 0;
                }
                url_player_set_cmd(g_url_player, e_player_cmd_seekto, tmp_position / 1000);

                int ret = 0;
                g_xiaowei_report_state.play_offset = tmp_position;
                g_xiaowei_report_state.play_state = play_state_resume;
                printf("------%s:into,state=%d,offset=%llu\n", __func__,
                       g_xiaowei_report_state.play_state, g_xiaowei_report_state.play_offset);
                SocketClientReadWriteMsg(
                    "/tmp/RequestASRTTS", "xiaowei_report_state_no_refresh_offset",
                    strlen("xiaowei_report_state_no_refresh_offset"), NULL, NULL, 0);
            }
        }
    } else if (xiaowei_cmd_offset_add == _response->control_id) {
        if (PLAYER_MODE_IS_ALEXA(g_wiimu_shm->play_mode)) {
            if (_response->control_value) {
                long long tmp_position = (long long)atol(_response->control_value);
                long long target_offset = get_play_pos() / 1000 + tmp_position / 1000;
                if (target_offset < 0) {
                    target_offset = 0;
                }
                printf("---%s:get_play_pos=%lld,target_offset=%lld\n", __func__, get_play_pos(),
                       target_offset);
                url_player_set_cmd(g_url_player, e_player_cmd_seekto, target_offset);

                int ret = 0;
                g_xiaowei_report_state.play_offset = target_offset * 1000;
                g_xiaowei_report_state.play_state = play_state_resume;
                printf("------%s:into,state=%d,offset=%llu\n", __func__,
                       g_xiaowei_report_state.play_state, g_xiaowei_report_state.play_offset);
                SocketClientReadWriteMsg(
                    "/tmp/RequestASRTTS", "xiaowei_report_state_no_refresh_offset",
                    strlen("xiaowei_report_state_no_refresh_offset"), NULL, NULL, 0);
            }
        }
    } else if (xiaowei_cmd_mute == _response->control_id) {
        wiimu_player_ctrl_set_mute(1);
    } else if (xiaowei_cmd_unmute == _response->control_id) {
        wiimu_player_ctrl_set_mute(0);
    } else if (xiaowei_cmd_vol_set_to == _response->control_id) {
        int vol_value = 0;
        if (_response->control_value) {
            vol_value = atoi(_response->control_value);
            if (vol_value < 0) {
                vol_value = 0;
            }
            if (vol_value > 100) {
                vol_value = 100;
            }
            sprintf(volume_buf, "setPlayerCmd:vol:%d", vol_value);
            SocketClientReadWriteMsg("/tmp/Requesta01controller", volume_buf, strlen(volume_buf),
                                     NULL, NULL, 0);
        }
    } else if (xiaowei_cmd_vol_add == _response->control_id) {
        int vol_step = 0;
        vol_step = atoi(_response->control_value);
        if (vol_step > 100) {
            vol_step = 100;
        }
        if (vol_step < -100) {
            vol_step = -100;
        }
        if (vol_step >= 0) {
            sprintf(volume_buf, "setPlayerCmd:Vol++%d", vol_step);
        } else {
            vol_step = -vol_step;
            sprintf(volume_buf, "setPlayerCmd:Vol--%d", vol_step);
        }
        SocketClientReadWriteMsg("/tmp/Requesta01controller", volume_buf, strlen(volume_buf), NULL,
                                 NULL, 0);
    } else if (xiaowei_cmd_close_mic == _response->control_id) {
        if (!g_wiimu_shm->privacy_mode)
            g_wiimu_shm->privacy_mode = 1;
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=PRIVACY_ENA",
                                 strlen("GNOTIFY=PRIVACY_ENA"), NULL, NULL, 0);
        asr_config_set(NVRAM_ASR_PRIVACY_MODE, "1");
        asr_config_sync();
    } else if (xiaowei_cmd_open_mic == _response->control_id) {
        if (g_wiimu_shm->privacy_mode)
            g_wiimu_shm->privacy_mode = 0;
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=PRIVACY_DIS",
                                 strlen("GNOTIFY=PRIVACY_DIS"), NULL, NULL, 0);
        asr_config_set(NVRAM_ASR_PRIVACY_MODE, "0");
        asr_config_sync();
    } else if (xiaowei_cmd_set_play_mode == _response->control_id) {
        int tmp_play_mode = 0;
        tmp_play_mode = atoi(_response->control_value);
        g_xiaowei_report_state.play_mode = tmp_play_mode;
        set_player_mode(g_url_player, tmp_play_mode);
        SocketClientReadWriteMsg("/tmp/RequestASRTTS", "xiaowei_report_state_refresh_offset",
                                 strlen("xiaowei_report_state_refresh_offset"), NULL, NULL, 0);
    } else if (xiaowei_cmd_report_state == _response->control_id) {
        // SocketClientReadWriteMsg("/tmp/RequestASRTTS", "xiaowei_report_state_no_refresh_offset",
        // strlen("xiaowei_report_state_no_refresh_offset"),
        // NULL, NULL, 0);
    } else if (xiaowei_cmd_bluetooth_mode_on == _response->control_id) {
        if (_response->resource_groups_size == 0) {
#ifdef A98_MARSHALL_STEPIN
            SocketClientReadWriteMsg("/tmp/bt_manager", "btconnectrequest:3",
                                     strlen("btconnectrequest:3"), NULL, NULL, 0);
#else
            SocketClientReadWriteMsg("/tmp/bt_manager", "btavkenterpair", strlen("btavkenterpair"),
                                     NULL, NULL, 0);
#endif
        } else {
            g_wiimu_shm->xiaowei_control_id = xiaowei_cmd_bluetooth_mode_on;
        }
    } else if (xiaowei_cmd_bluetooth_mode_off == _response->control_id) {
        char rep[32];
        SocketClientReadWriteMsg("/tmp/bt_manager", "btdisconnectall", strlen("btdisconnectall"),
                                 NULL, NULL, 0);
        NotifyMessageACKBlockIng(BT_MANAGER_SOCKET, "btmodeswitch:0", rep, 32);
    } else if (xiaowei_cmd_shutdown == _response->control_id) {
        if (_response->resource_groups_size == 0) {
            SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=VOICE_REQUEST_SHUTDOWN",
                                     strlen("GNOTIFY=VOICE_REQUEST_SHUTDOWN"), NULL, NULL, 0);
        } else {
            g_wiimu_shm->xiaowei_control_id = xiaowei_cmd_shutdown;
        }
    } else if (xiaowei_cmd_reboot == _response->control_id) {
        if (_response->resource_groups_size == 0) {
            SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=REBOOT",
                                     strlen("GNOTIFY=REBOOT"), NULL, 0, 0);
        } else {
            g_wiimu_shm->xiaowei_control_id = xiaowei_cmd_reboot;
        }
    } else if (xiaowei_cmd_duplex_mode_on == _response->control_id) {
        g_wiimu_shm->xiaowei_control_id = xiaowei_cmd_duplex_mode_on;
    } else if (xiaowei_cmd_duplex_mode_off == _response->control_id) {
        set_duplex_mode(0);
        g_wiimu_shm->xiaowei_control_id = xiaowei_cmd_duplex_mode_off;
        set_query_status(e_query_idle);
    }
}

int force_ota_disabled(void)
{
    char tmp_buf[128] = {0};
    wm_db_get(NVRAM_ASR_DIS_FORCE_OTA, tmp_buf);
    if (tmp_buf && strlen((char *)tmp_buf) > 0 && tmp_buf[0] == '1')
        return 1;
    return 0;
}

int all_ota_disabled(void)
{
    char tmp_buf[128] = {0};
    wm_db_get(NVRAM_ASR_DIS_ALL_OTA, tmp_buf);
    if (tmp_buf && strlen(tmp_buf) > 0 && tmp_buf[0] == '1')
        return 1;
    return 0;
}

int xiaowei_set_music_quality(int quality)
{
    char tmp_voiceid[128];
    char tmp_param[256] = {0};

    snprintf(tmp_param, 256, "{\"skill_info\": {\"id\": "
                             "\"8dab4796-fa37-4114-0011-7637fa2b0001\",\"name\": "
                             "\"音乐\"},\"quality_config\": {\"type\": 0,\"quality\": %d}}",
             quality);
    xiaowei_request_cmd(tmp_voiceid, "PLAY_RESOURCE", "quality_config", tmp_param,
                        _XIAOWEI_ON_REQUEST_CMD_CALLBACK);
}

void OnSilence(void)
{
    if (get_query_status() == e_query_record_start) {
        memset(&context, 0, sizeof(context));
        printf("OnSilence stop wakeup\n");
    }
}

unsigned long long get_xiaowei_tencent_version(void)
{
    char *tmp_xiaowei_ver = NULL;
    char *tmp_first_param = NULL;
    char *tmp_second_param = NULL;
    char *tmp_third_param = NULL;
    unsigned long long tmp_first_int = 0;
    unsigned long long tmp_second_int = 0;
    unsigned long long tmp_third_int = 0;
    unsigned long long tmp_xiaowei_tencent_ver = 1;
    tmp_xiaowei_ver = strrchr(g_wiimu_shm->firmware_ver, '.') + 1;
    wiimu_log(1, 0, 0, 0, "%s:tmp_xiaowei_ver=%s,firmware_ver=%s\n", __func__, tmp_xiaowei_ver,
              g_wiimu_shm->firmware_ver);
    if (tmp_xiaowei_ver && (strlen(tmp_xiaowei_ver) == 6)) {
        asprintf(&tmp_first_param, "%c%c", tmp_xiaowei_ver[0], tmp_xiaowei_ver[1]);
        asprintf(&tmp_second_param, "%c%c", tmp_xiaowei_ver[2], tmp_xiaowei_ver[3]);
        asprintf(&tmp_third_param, "%c%c", tmp_xiaowei_ver[4], tmp_xiaowei_ver[5]);
        if (!tmp_first_param || !tmp_second_param || !tmp_third_param) {
            goto get_xiaowei_ver_err;
        }
        wiimu_log(1, 0, 0, 0, "%s:tmp_first_param=%s,tmp_second_param=%s,tmp_third_param=%s\n",
                  __func__, tmp_first_param, tmp_second_param, tmp_third_param);
        tmp_first_int = (unsigned long long)atoi(tmp_first_param);
        if (tmp_third_int > 99 || tmp_first_int < 0) {
            goto get_xiaowei_ver_err;
        }
        tmp_second_int = (unsigned long long)atoi(tmp_second_param);
        if (tmp_second_int > 99 || tmp_second_int < 0) {
            goto get_xiaowei_ver_err;
        }
        tmp_third_int = (unsigned long long)atoi(tmp_third_param);
        if (tmp_third_int > 99 || tmp_third_int < 0) {
            goto get_xiaowei_ver_err;
        }
        // wiimu_log(1, 0, 0, 0,
        // "%s:tmp_first_int=%llu,tmp_second_int=%llu,tmp_third_int=%llu\n",__func__,tmp_first_param,tmp_second_param,tmp_third_param);
        tmp_xiaowei_tencent_ver = (tmp_first_int << 32) + (tmp_second_int << 16) + tmp_third_int;
        wiimu_log(1, 0, 0, 0, "%s:tmp_xiaowei_tencent_ver=%llu\n", __func__,
                  tmp_xiaowei_tencent_ver);
        return tmp_xiaowei_tencent_ver;
    } else {
        goto get_xiaowei_ver_err;
    }
get_xiaowei_ver_err:
    wiimu_log(1, 0, 0, 0, "%s:something wrong,get xiaowei ota version err\n", __func__);
    return 1;
}
