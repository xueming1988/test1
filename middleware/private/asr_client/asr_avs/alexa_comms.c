#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/un.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/wait.h>
#include <semaphore.h>

#include "avs_player.h"
#include "alexa_comms.h"
#include "alexa_comms_macro.h"
#include "alexa_cmd.h"
#include "alexa_debug.h"
#include "alexa_json_common.h"
#include "alexa_request_json.h"
#include "download_file.h"
#include "wm_util.h"
#include "alexa_comms_client.h"
#include "alexa_products.h"
#include "alexa_focus_mgmt.h"
#include "avs_player.h"

#define SOCKET_IOGUARD ("/tmp/RequestIOGuard")
#define ONLINE_STATE_0 ("report_alexa_conn_state:0")
#define ONLINE_STATE_1 ("report_alexa_conn_state:1")
#define INTERNET_STATE_0 ("report_network_conn_state:0")
#define INTERNET_STATE_1 ("report_network_conn_state:1")

#define DOWNLOAD_PATH ("/tmp/comms_ringtone.mp3")
#define NULL_PATH ("/tmp/null.mp3")

enum SipUserAgentState {
    ANY = 0,
    UNREGISTERED,
    IDLE,
    TRYING,
    OUTBOUND_LOCAL_RINGING,
    OUTBOUND_PROVIDER_RINGING,
    ACTIVE,
    INVITED,
    INBOUND_RINGING,
    STOPPING,
};

typedef struct {
    int ring_stop;
    int is_incall;
    char *ringtone_src;
    bool url_downloaded;
    bool ringtone_loop;
    enum ringtone_way ringtone_way;
    enum SipUserAgentState sua_state;
    const char *led_pattern;
    json_object *comms_state;
    pthread_mutex_t state_lock;
    pthread_t ringtone_proc_tid;
    sem_t req_sem;
#ifdef COMMS_API_V3
    sem_t comms_cxt_update_sem;
#endif
} avs_comms_state_t;

static int g_have_asr_comms = 1;
static avs_comms_state_t g_comms_state;
extern void cxdish_call_boost(int flags);
extern WIIMU_CONTEXT *g_wiimu_shm;
extern share_context *g_mplayer_shmem;

#define CALL_STATES_CHANGE_NUM 19

typedef struct call_state_ux_s {
    enum SipUserAgentState pre_state;
    enum SipUserAgentState cur_state;
    const char *led_pattern;
    enum ringtone_way ringtone_way;
    const char *ringtone_src;
    bool ringtone_loop;
} call_state_ux_t;

#ifdef COMMS_API_V3
#define OUTBOUND_LOCAL_RINGING_WAY RINGTONE_URL
#else
#define OUTBOUND_LOCAL_RINGING_WAY RINGTONE_FILE
#endif

#define NOTIFY_MSG(socket, cmd, block)                                                             \
    ((NULL != cmd) ? SocketClientReadWriteMsg(socket, (char *)(cmd), strlen((char *)(cmd)), NULL,  \
                                              NULL, block)                                         \
                   : -1)

static const call_state_ux_t call_states[CALL_STATES_CHANGE_NUM] = {
    {ANY, TRYING, LED_CALL_CONNECTING, RINGTONE_NA, RING_SRC_CALL_NA, RING_NO_LOOP},
    {ANY, OUTBOUND_LOCAL_RINGING, LED_CALL_CONNECTING, OUTBOUND_LOCAL_RINGING_WAY,
     RING_SRC_CALL_OUTBOUND, RING_DO_LOOP},
    {ANY, OUTBOUND_PROVIDER_RINGING, LED_CALL_CONNECTING, RINGTONE_SUA, RING_SRC_CALL_NA,
     RING_NO_LOOP},
    {ANY, INBOUND_RINGING, LED_CALL_INBOUND_RINGING, RINGTONE_URL, RING_SRC_CALL_URL, RING_NO_LOOP},
    {ACTIVE, STOPPING, LED_CALL_DISCONNECTED, RINGTONE_FILE, RING_SRC_CALL_DISCONNECTED,
     RING_NO_LOOP},
    {ACTIVE, IDLE, LED_CALL_DISCONNECTED, RINGTONE_FILE, RING_SRC_CALL_DISCONNECTED, RING_NO_LOOP},
    {INVITED, ACTIVE, LED_CALL_CONNECTED, RINGTONE_FILE, RING_SRC_DROP_IN_CONNECTED, RING_NO_LOOP},
    {OUTBOUND_PROVIDER_RINGING, ACTIVE, LED_CALL_CONNECTED, RINGTONE_NA, RING_SRC_CALL_NA,
     RING_NO_LOOP},
    {TRYING, ACTIVE, LED_CALL_CONNECTED, RINGTONE_FILE, RING_SRC_CALL_CONNECTED, RING_NO_LOOP},
    {INBOUND_RINGING, ACTIVE, LED_CALL_CONNECTED, RINGTONE_FILE, RING_SRC_CALL_CONNECTED,
     RING_NO_LOOP},
    {OUTBOUND_LOCAL_RINGING, ACTIVE, LED_CALL_CONNECTED, RINGTONE_FILE, RING_SRC_CALL_CONNECTED,
     RING_NO_LOOP},
    {TRYING, STOPPING, LED_CALL_QUIT, RINGTONE_FILE, RING_SRC_CALL_DISCONNECTED, RING_NO_LOOP},
    {INBOUND_RINGING, STOPPING, LED_CALL_QUIT, RINGTONE_FILE, RING_SRC_CALL_DISCONNECTED,
     RING_NO_LOOP},
    {OUTBOUND_LOCAL_RINGING, STOPPING, LED_CALL_QUIT, RINGTONE_FILE, RING_SRC_CALL_DISCONNECTED,
     RING_NO_LOOP},
    {OUTBOUND_PROVIDER_RINGING, STOPPING, LED_CALL_QUIT, RINGTONE_FILE, RING_SRC_CALL_DISCONNECTED,
     RING_NO_LOOP},
    {TRYING, IDLE, LED_CALL_QUIT, RINGTONE_FILE, RING_SRC_CALL_DISCONNECTED, RING_NO_LOOP},
    {INBOUND_RINGING, IDLE, LED_CALL_QUIT, RINGTONE_FILE, RING_SRC_CALL_DISCONNECTED, RING_NO_LOOP},
    {OUTBOUND_LOCAL_RINGING, IDLE, LED_CALL_QUIT, RINGTONE_FILE, RING_SRC_CALL_DISCONNECTED,
     RING_NO_LOOP},
    {OUTBOUND_PROVIDER_RINGING, IDLE, LED_CALL_QUIT, RINGTONE_FILE, RING_SRC_CALL_DISCONNECTED,
     RING_NO_LOOP}};

static void start_call(int flags)
{
    if (flags) {
        printf("!!!!!!!!![%s:%d] call on\n", __FUNCTION__, __LINE__);
        WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 0, SNDSRC_AIRPLAY, 1);
    } else {
        printf("!!!!!!!!![%s:%d] call off\n", __FUNCTION__, __LINE__);
        WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 1, SNDSRC_AIRPLAY, 1);
    }
}

bool realloc_str(char **point, char *value)
{
    bool ret = 0;
    if (*point) {
        free(*point);
        *point = NULL;
    }
    if (value) {
        *point = strdup(value);
        ret = 1;
    }
    return ret;
}

void comms_destory_ringtone_proc(void)
{
    pthread_mutex_lock(&g_comms_state.state_lock);
    g_comms_state.ring_stop = 1;
    pthread_mutex_unlock(&g_comms_state.state_lock);

    if (g_comms_state.ringtone_proc_tid > 0) {
        wiimu_log(1, 0, 0, 0, "comms_destory_ringtone_proc pthread_join begin, pid = %d stop(%d)\n",
                  g_comms_state.ringtone_proc_tid, g_comms_state.ring_stop);
        struct timespec time_wait;
        struct timeval now;
        gettimeofday(&now, NULL);
        time_wait.tv_nsec = now.tv_usec * 1000;
        time_wait.tv_sec = now.tv_sec + 1;
        int ret = sem_timedwait(&g_comms_state.req_sem, &time_wait);
        /* Check what happened */
        if (ret == -1) {
            if (errno == ETIMEDOUT) {
                wiimu_log(1, 0, 0, 0, "sem_timedwait timed out\n");
            } else {
                wiimu_log(1, 0, 0, 0, "sem_timedwait");
            }
        }
        killPidbyName((char *)"ringtone_smplayer");
        pthread_join(g_comms_state.ringtone_proc_tid, NULL);
        wiimu_log(1, 0, 0, 0,
                  "comms_destory_ringtone_proc pthread_join finish, pid = %d stop(%d)\n",
                  g_comms_state.ringtone_proc_tid, g_comms_state.ring_stop);
        g_comms_state.ringtone_proc_tid = 0;
    }
    killPidbyName((char *)"ringtone_smplayer");
}

static int create_get_sem(void)
{
    key_t sem_key = 10000;
    int ret = 0;
    int semid = 0;
    ret = semget(sem_key, 1, 0666 | IPC_CREAT);
    if (ret < 0) {
        wiimu_log(1, 0, 0, 0, "sem semget error\n");
    } else {
        semid = ret;
    }

    return semid;
}

static void set_sem(int value)
{
    int semid = create_get_sem();
    if (semid > 0) {
        semctl(semid, 0, SETVAL, value);
    } else {
        wiimu_log(1, 0, 0, 0, "Set sem value error\n");
    }
}

static int g_smplayer_wait = SMPLAYER_NO_WAIT;
static int g_smplayer_volume = SMPLAYER_KEEP_VOLUME;
static void *download_ringtone(void *arg)
{
    char *ring_url = NULL;
    avs_comms_state_t *comms_state = (avs_comms_state_t *)arg;

    pthread_detach(pthread_self());
    if (comms_state) {
        pthread_mutex_lock(&comms_state->state_lock);
        if (comms_state->ringtone_src) {
            ring_url = strdup(comms_state->ringtone_src);
        }
        pthread_mutex_unlock(&comms_state->state_lock);

        if (ring_url) {
            wiimu_log(1, 0, 0, 0, "debug: comms Ringtone , download mp3, url = %s\n", ring_url);
            g_smplayer_wait = SMPLAYER_WAIT;
            curl_download(ring_url, NULL, NULL, DOWNLOAD_PATH);
            set_sem(1);
            g_smplayer_wait = SMPLAYER_NO_WAIT;
            wiimu_log(1, 0, 0, 0, "debug: comms Ringtone , download mp3 finished.\n");
            free(ring_url);
        } else {
            wiimu_log(1, 0, 0, 0, "ring_url is NULL. \n");
        }
    } else {
        wiimu_log(1, 0, 0, 0, "comms_state is NULL. \n");
    }

    return NULL;
}

static void *play_ringtone_proc(void *arg)
{
    int ret = 0;
    pthread_t pid = 0;
    char *ring_url = NULL;
    char cmd[1024] = {0};
    char ringtone_path[1024] = {0};
    enum ringtone_way ringtone_way;
    bool ringtone_loop = 0;
    bool ring_stop = 0;
    bool url_downloaded = 0;
    avs_comms_state_t *comms_state = (avs_comms_state_t *)arg;

    if (comms_state) {
        sem_post(&comms_state->req_sem);
        pthread_mutex_lock(&comms_state->state_lock);
        ring_stop = comms_state->ring_stop;
        ringtone_way = comms_state->ringtone_way;
        ringtone_loop = comms_state->ringtone_loop;
        url_downloaded = comms_state->url_downloaded;
        const char *led_pattern = comms_state->led_pattern;

        if (RINGTONE_FILE == ringtone_way) {
            snprintf(ringtone_path, sizeof(ringtone_path),
                     "/system/workdir/misc/Voice-prompt/common/%s", comms_state->ringtone_src);
            snprintf(cmd, sizeof(cmd), "ringtone_smplayer %s", ringtone_path);
        } else if (RINGTONE_URL == ringtone_way) {
            ring_url = strdup(comms_state->ringtone_src);
            snprintf(ringtone_path, sizeof(ringtone_path), "%s", DOWNLOAD_PATH);
        } else {
            snprintf(ringtone_path, sizeof(ringtone_path), "%s", NULL_PATH);
        }
        pthread_mutex_unlock(&comms_state->state_lock);

        if (ring_url) {
            if (!url_downloaded) {
                remove(DOWNLOAD_PATH);
                set_sem(0);
                ret = pthread_create(&pid, NULL, download_ringtone, (void *)comms_state);
                if (ret) {
                    ALEXA_PRINT(ALEXA_DEBUG_ERR, "create download_ringtone thread failed \n");
                } else {
                    pthread_mutex_lock(&comms_state->state_lock);
                    comms_state->url_downloaded = true;
                    pthread_mutex_unlock(&comms_state->state_lock);
                }
            }
            FILE *fp;
            int size = -1;
            int n = 0;
            while (size <= 0) {
                fp = fopen(ringtone_path, "rb");
                if (fp) {
                    fseek(fp, 0, SEEK_END);
                    size = ftell(fp);
                    fclose(fp);
                }
                usleep(100000);
                pthread_mutex_lock(&comms_state->state_lock);
                ring_stop = comms_state->ring_stop;
                pthread_mutex_unlock(&comms_state->state_lock);
                if (++n >= 100 || ring_stop) {
                    ALEXA_PRINT(ALEXA_DEBUG_ERR, "get rongtone time out n=%d\n", n);
                    break;
                }
            }
            snprintf(cmd, sizeof(cmd), "ringtone_smplayer %s %d %d", ringtone_path,
                     g_smplayer_volume, g_smplayer_wait);
            free(ring_url);
        }

        ret = NOTIFY_MSG(SOCKET_IOGUARD, led_pattern, 0);
        if (ret < 0) {
            ALEXA_PRINT(ALEXA_DEBUG_ERR, "start led pattern %s failed!\n", led_pattern);
        }

        wiimu_log(1, 0, 0, 0, (char *)"ringtone_path %s ring_stop = %d\r\n", ringtone_path,
                  ring_stop);

        if (!strncmp(ringtone_path, NULL_PATH, strlen(NULL_PATH))) {
            wiimu_log(1, 0, 0, 0, "No ringtone need to be played by us\n");
        } else if (access((char *)ringtone_path, 0) == 0) {
            do {
                pthread_mutex_lock(&comms_state->state_lock);
                ring_stop = comms_state->ring_stop;
                pthread_mutex_unlock(&comms_state->state_lock);

                if (!ring_stop) {
                    killPidbyName((char *)"ringtone_smplayer");
                    int result = system(cmd);
                    wiimu_log(1, 0, 0, 0, "system return is %d WIFEXITED(%d)\n", result,
                              WIFEXITED(result));
                }
            } while (ringtone_loop && !ring_stop);
        } else {
            ALEXA_PRINT(ALEXA_DEBUG_ERR, "file %s can not access!\n", ringtone_path);
        }
        wiimu_log(1, 0, 0, 0, "play_ringtone_proc finish: %s\n", cmd);
    }

    return NULL;
}

static void comms_show_ux(void)
{
    int ret = 0;
    comms_destory_ringtone_proc();
    pthread_mutex_lock(&g_comms_state.state_lock);
    g_comms_state.ring_stop = 0;
    pthread_mutex_unlock(&g_comms_state.state_lock);

    wiimu_log(1, 0, 0, 0, "%s ,ringtone src = %s\n", __func__, g_comms_state.ringtone_src);
    ret = pthread_create(&g_comms_state.ringtone_proc_tid, NULL, play_ringtone_proc,
                         (void *)&g_comms_state);
    if (ret) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "create loop_ringtone_proc thread failed \n");
        g_comms_state.ringtone_proc_tid = 0;
    }
}

#if defined(SUPPORT_VOIP_SWITCH) && SUPPORT_VOIP_SWITCH
void switch_dsp_mode(enum dsp_mode new_mode)
{
    static enum dsp_mode local_dsp_mode = DSP_MODE_ASR; // DEFAULT
    int ret = 0;
    if (new_mode == local_dsp_mode) {
        wiimu_log(1, 0, 0, 0, "COMMS switch dsp mode , same as perious\n");
        return;
    }
    if (DSP_MODE_VOIP == new_mode)
        ret = cxdish_switch_mode_voip();
    else
        ret = cxdish_switch_mode_asr();

    if (ret < 0)
        wiimu_log(1, 0, 0, 0, "COMMS switch dsp mode to %s error!\n",
                  new_mode == DSP_MODE_ASR ? "ASR" : "VOIP");
    else
        wiimu_log(1, 0, 0, 0, "COMMS switch dsp mode to %s successful!\n",
                  new_mode == DSP_MODE_ASR ? "ASR" : "VOIP");

    local_dsp_mode = new_mode;
}
#endif

static void call_connect(void)
{
#if defined(SUPPORT_VOIP_SWITCH) && SUPPORT_VOIP_SWITCH
    switch_dsp_mode(DSP_MODE_VOIP);
#endif
}

static void call_disconnect(void)
{
#if defined(SUPPORT_VOIP_SWITCH) && SUPPORT_VOIP_SWITCH
    switch_dsp_mode(DSP_MODE_ASR);
#endif
}

static void comms_state_updated(json_object *params_json)
{
    bool r_ok = 0;
    if (!params_json) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "PARAMS IS NULL!\n");
        return;
    }

    int i = 0;
    enum SipUserAgentState pre_state = ANY;
    enum SipUserAgentState cur_state = ANY;
    char *pre_state_s = JSON_GET_STRING_FROM_OBJECT(params_json, Key_pre_state);
    char *cur_state_s = JSON_GET_STRING_FROM_OBJECT(params_json, Key_cur_state);
    if (pre_state_s)
        pre_state = atoi(pre_state_s);
    if (cur_state_s)
        cur_state = atoi(cur_state_s);

    pthread_mutex_lock(&g_comms_state.state_lock);
    g_comms_state.sua_state = cur_state;
    pthread_mutex_unlock(&g_comms_state.state_lock);

    for (i = 0; i < CALL_STATES_CHANGE_NUM; i++) {
        if ((cur_state == call_states[i].cur_state) &&
            ((pre_state == call_states[i].pre_state) || (ANY == call_states[i].pre_state))) {
            break;
        }
    }

    if (cur_state == ACTIVE && pre_state != ACTIVE)
        call_connect();
    else if (cur_state != ACTIVE && pre_state == ACTIVE)
        call_disconnect();

    if (i >= CALL_STATES_CHANGE_NUM) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "STATE CHANGE DO NOT NEED TO SHOW UX\n");
        return;
    }

    pthread_mutex_lock(&g_comms_state.state_lock);
    g_comms_state.led_pattern = call_states[i].led_pattern;
    g_comms_state.ringtone_way = call_states[i].ringtone_way;
    g_comms_state.ringtone_loop = call_states[i].ringtone_loop;
    g_comms_state.url_downloaded = false;
    if (RINGTONE_URL == g_comms_state.ringtone_way) {
        char *url = JSON_GET_STRING_FROM_OBJECT(params_json, Key_url);
        if (NULL == url || strlen(url) < 1) {
            ALEXA_PRINT(ALEXA_DEBUG_ERR, "NEED URL , URL IS NULL, PLAY FILE\n");
            g_comms_state.ringtone_way = RINGTONE_FILE;
            if (OUTBOUND_LOCAL_RINGING == cur_state)
                r_ok = realloc_str(&g_comms_state.ringtone_src, RING_SRC_CALL_OUTBOUND);
            else
                r_ok =
                    realloc_str(&g_comms_state.ringtone_src, RING_SRC_CALL_INCOMING_RINGTONE_INTRO);
        } else
            r_ok = realloc_str(&g_comms_state.ringtone_src, url);
    } else {
        r_ok = realloc_str(&g_comms_state.ringtone_src, (char *)call_states[i].ringtone_src);
    }
    pthread_mutex_unlock(&g_comms_state.state_lock);

    if (!r_ok) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "ringtone src get error!\n");
        return;
    }

    comms_show_ux();
}

static void comms_channel_change(json_object *params_json)
{
    if (params_json) {
        // char * channel_name = JSON_GET_STRING_FROM_OBJECT(params_json, Key_channel_name);
        json_bool channel_on = JSON_GET_BOOL_FROM_OBJECT(params_json, Key_channel_on);

        if (channel_on) {
            focus_mgmt_activity_status_set(SipUserAgent, FOCUS);
            pthread_mutex_lock(&g_comms_state.state_lock);
            g_comms_state.is_incall = 1;
            pthread_mutex_unlock(&g_comms_state.state_lock);
            NOTIFY_CMD(ASR_TTS, FocusManage_COMMSSTART);
            if (g_wiimu_shm->mute == 1 || g_wiimu_shm->volume < MINI_VOLUME_OF_COMMS) {
                avs_player_need_unmute(MINI_VOLUME_OF_COMMS);
            }

            start_call(1);
        } else {
            focus_mgmt_activity_status_set(SipUserAgent, UNFOCUS);
            pthread_mutex_lock(&g_comms_state.state_lock);
            g_comms_state.is_incall = 0;
            pthread_mutex_unlock(&g_comms_state.state_lock);
            NOTIFY_CMD(ASR_TTS, FocusManage_COMMSEND);

            start_call(0);
        }
    }
}

void avs_comms_stop_ux(void)
{
    pthread_mutex_lock(&g_comms_state.state_lock);
    g_comms_state.ring_stop = 1;
    pthread_mutex_unlock(&g_comms_state.state_lock);
}

void avs_comms_resume_ux(void)
{
    enum SipUserAgentState sua_state = ANY;

    pthread_mutex_lock(&g_comms_state.state_lock);
    sua_state = g_comms_state.sua_state;
    pthread_mutex_unlock(&g_comms_state.state_lock);

    wiimu_log(1, 0, 0, 0, "CHERYL DEBUG g_comms_state.sua_state = %d\n", sua_state);
    if ((sua_state == OUTBOUND_LOCAL_RINGING) || (sua_state == OUTBOUND_PROVIDER_RINGING) ||
        (sua_state == INBOUND_RINGING)) {
        ALEXA_PRINT(ALEXA_DEBUG_INFO, "comms need to resume ux\n");
        comms_show_ux();
    }
}

void avs_comms_state_change(int type, int status)
{
    int ret = 0;

    if (g_have_asr_comms) {
        if (e_state_online == type) {
            ret = NOTIFY_MSG(COMMS_PIPE, (status ? ONLINE_STATE_1 : ONLINE_STATE_0), 0);
        } else if (e_state_internet == type) {
            ret = NOTIFY_MSG(COMMS_PIPE, (status ? INTERNET_STATE_1 : INTERNET_STATE_0), 0);
        }
    }
    if (!ret) {
        printf("avs_comms_state_change failed\n");
    }
}

void avs_comms_notify(char *notify_data)
{
    int ret = 0;

    if (g_have_asr_comms) {
        ret = NOTIFY_MSG(COMMS_PIPE, notify_data, 0);
    }
    if (!ret) {
        printf("avs_comms_state_change failed\n");
    }
}

int avs_comms_init(void)
{
    int ret = 0;
    memset(&g_comms_state, 0x0, sizeof(avs_comms_state_t));
    pthread_mutex_init(&g_comms_state.state_lock, NULL);
    sem_init(&g_comms_state.req_sem, 0, 0);
#ifdef COMMS_API_V3
    sem_init(&g_comms_state.comms_cxt_update_sem, 0, 0);
#endif
    return ret;
}

int avs_comms_uninit(void)
{
    int ret = 0;
    comms_destory_ringtone_proc();
    pthread_mutex_lock(&g_comms_state.state_lock);
    if (g_comms_state.comms_state) {
        json_object_put(g_comms_state.comms_state);
        g_comms_state.comms_state = NULL;
    }

    pthread_mutex_unlock(&g_comms_state.state_lock);
    pthread_mutex_destroy(&g_comms_state.state_lock);
    sem_destroy(&g_comms_state.req_sem);
#ifdef COMMS_API_V3
    sem_destroy(&g_comms_state.comms_cxt_update_sem);
#endif
    return ret;
}

// parse comms directive

int avs_comms_parse_directive(json_object *js_data)
{
    int ret = -1;
    if (js_data) {
        json_object *json_header = json_object_object_get(js_data, KEY_HEADER);
        if (json_header) {
            char *name = JSON_GET_STRING_FROM_OBJECT(json_header, KEY_NAME);
            // When receive the Stop directive, donot resume the Ring tone
            if (StrEq(name, Key_Stop) || StrEq(name, Key_Accept)) {
                pthread_mutex_lock(&g_comms_state.state_lock);
                if ((g_comms_state.sua_state == OUTBOUND_LOCAL_RINGING) ||
                    (g_comms_state.sua_state == INBOUND_RINGING)) {
                    g_comms_state.ringtone_way = RINGTONE_NA;
                    printf("comms DEBUG donnot resume the old ringtone\n");
                }
                pthread_mutex_unlock(&g_comms_state.state_lock);
            }
            json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
            if (name) {
                char *tmp_buf = NULL;
                asprintf(&tmp_buf, "handle_call_directive:NAME=%sPAYLOAD=%s", name,
                         (char *)json_object_to_json_string(js_payload));

                if (tmp_buf) {
                    if (g_have_asr_comms) {
                        ret = NOTIFY_MSG(COMMS_PIPE, tmp_buf, 0);
                    }
                    free(tmp_buf);
                }
            }
        }
    }

    if (ret < 0) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "prase message error\n");
    }

    return ret;
}

// parse the cmd from bluetooth thread to avs event
// TODO: need free after used

char *avs_comms_parse_cmd(char *json_cmd_str)
{
    json_object *json_cmd = NULL;
    json_object *json_cmd_parmas = NULL;
    char *cmd_params_str = NULL;
    char *event_type = NULL;
    char *event = NULL;
    char *avs_cmd = NULL;
    char *event_name = NULL;
    if (!json_cmd_str) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "input is NULL\n");
        goto Exit;
    }
    json_cmd = json_tokener_parse(json_cmd_str);
    if (!json_cmd) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "input is not a json\n");
        goto Exit;
    }

    event_type = JSON_GET_STRING_FROM_OBJECT(json_cmd, Key_Event_type);
    if (!event_type) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "cmd none name found\n");
        goto Exit;
    }

    if (StrEq(event_type, Type_SipPhone)) {
        event = JSON_GET_STRING_FROM_OBJECT(json_cmd, Key_Event);

        if (event) {
            json_cmd_parmas = json_object_object_get(json_cmd, Key_Event_params);

            if (json_cmd_parmas) {
                cmd_params_str = (char *)json_object_to_json_string(json_cmd_parmas);
            }

            if (StrEq(event, Event_State_Updated)) {
                comms_state_updated(json_cmd_parmas);
                event_name = NULL;
            }

            else if (StrEq(event, Event_Send_Events)) {
                event_name = JSON_GET_STRING_FROM_OBJECT(json_cmd_parmas, Key_event_name);
                json_object *payload = json_object_object_get(json_cmd_parmas, Key_payload);
                if (payload) {
                    cmd_params_str = (char *)json_object_to_json_string(payload);
                } else {
                    cmd_params_str = NULL;
                }
            } else if (StrEq(event, Event_Get_State_Cxt)) {
                json_object *state = json_object_object_get(json_cmd_parmas, Key_state);
                if (state) {
                    pthread_mutex_lock(&g_comms_state.state_lock);
                    char *state_str = (char *)json_object_to_json_string(state);
                    if (state_str) {
                        if (g_comms_state.comms_state) {
                            json_object_put(g_comms_state.comms_state);
                            g_comms_state.comms_state = NULL;
                        }
                        g_comms_state.comms_state = json_tokener_parse(state_str);
                    }
                    pthread_mutex_unlock(&g_comms_state.state_lock);
// avs_cmd = create_system_cmd(0, NAME_SYNCHORNIZESTATE, 0, NULL);
#ifdef COMMS_API_V3
                    sem_post(&g_comms_state.comms_cxt_update_sem);
#endif
                }
                event_name = NULL;

            } else if (StrEq(event, Event_Channel_Focus)) {
                comms_channel_change(json_cmd_parmas);
                event_name = NULL;
            }
        }
        if (event_name) {
            Create_Cmd(avs_cmd, 0, NAMESPACE_SIPUSERAGENT, event_name, Val_Obj(cmd_params_str));
        }
    }
Exit:
    if (json_cmd) {
        json_object_put(json_cmd);
    }

    return avs_cmd;
}

static int avs_comms_get_state(void)
{
    char buf[8192];
    int len = 0;
    json_object *json = NULL;

    memset(buf, 0, sizeof(buf));

    if (SocketClientReadWriteMsg(COMMS_PIPE, "update_status_context",
                                 strlen("update_status_context"), buf, &len, 0) < 0) {
        wiimu_log(1, 0, 0, 0, "update_status_context error!\n");
        return -1;
    } else {
        wiimu_log(1, 0, 0, 0, "update_status_context response: %s\n", buf);
        json = json_tokener_parse(buf);
        if (!json) {
            ALEXA_PRINT(ALEXA_DEBUG_ERR, "input is not a json\n");
            return -1;
        }

        pthread_mutex_lock(&g_comms_state.state_lock);
        if (g_comms_state.comms_state) {
            json_object_put(g_comms_state.comms_state);
            g_comms_state.comms_state = NULL;
        }
        g_comms_state.comms_state = json;
        pthread_mutex_unlock(&g_comms_state.state_lock);
    }
    return 0;
}

void avs_comms_state_cxt(json_object *js_context_list)
{
    if (g_have_asr_comms == 0) {
        return;
    }

#ifdef COMMS_API_V3
    if (avs_comms_get_state() < 0)
        return;
#endif

    //
    if (js_context_list) {
        pthread_mutex_lock(&g_comms_state.state_lock);
        if (g_comms_state.comms_state) {
            char *comms_state_str = (char *)json_object_to_json_string(g_comms_state.comms_state);
            if (comms_state_str) {
                json_object *tmp_js = json_tokener_parse(comms_state_str);
                if (tmp_js) {
                    alexa_context_list_add(js_context_list, NAMESPACE_SIPUSERAGENT,
                                           NAME_SIPUSERAGENTSTATE, tmp_js);
                }
            }
        }
        pthread_mutex_unlock(&g_comms_state.state_lock);
    }
}

json_object *avs_comms_event_create(json_object *json_cmd)
{
    int ret = -1;
    char *cmd_name = NULL;
    json_object *json_event_header = NULL;
    json_object *json_event_payload = NULL;
    json_object *json_event = NULL;
    json_object *json_cmd_parms = NULL;

    if (!json_cmd) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "input error\n");
        goto Exit;
    }

    cmd_name = JSON_GET_STRING_FROM_OBJECT(json_cmd, KEY_NAME);
    if (!cmd_name) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "cmd none name found\n");
        goto Exit;
    }

    json_event_header = alexa_event_header(NAMESPACE_SIPUSERAGENT, cmd_name);
    json_cmd_parms = json_object_object_get(json_cmd, KEY_params);
    if (json_cmd_parms) {
        char *cmd_params_str = (char *)json_object_to_json_string(json_cmd_parms);
        if (cmd_params_str) {
            json_event_payload = json_tokener_parse(cmd_params_str);
        }
    }

    if (json_event_header && json_event_payload) {
        json_event = alexa_event_body(json_event_header, json_event_payload);
        if (json_event) {
            ret = 0;
        }
    }

Exit:
    if (ret != 0) {
        if (json_event_header) {
            json_object_put(json_event_header);
        }
        if (json_event_payload) {
            json_object_put(json_event_payload);
        }
        json_event = NULL;
    }

    return json_event;
}

int avs_comms_is_avtive(void)
{
    int is_active = 0;
    pthread_mutex_lock(&g_comms_state.state_lock);
    is_active = g_comms_state.is_incall;
    pthread_mutex_unlock(&g_comms_state.state_lock);

    return is_active;
}

bool avs_get_project_DTID(char *DTID)
{
    bool ret = false;
#if defined(ALEXA_DEFAULT_DEVICE_NAME) && defined(ALEXA_DEFAULT_DTID)
    strncpy(DTID, ALEXA_DEFAULT_DTID, strlen(ALEXA_DEFAULT_DTID));
    ret = true;
#else
    AlexaDeviceInfo device_info;
    device_info.clientId = NULL;
    device_info.clientSecret = NULL;
    device_info.deviceName = NULL;
    device_info.serverUrl = NULL;
    device_info.projectName = NULL;
    device_info.DTID = NULL;

    get_alexa_device_info(ALEXA_PRODUCTS_FILE_PATH, g_wiimu_shm->project_name, &device_info);
    if (device_info.DTID) {
        strncpy(DTID, device_info.DTID, strlen(device_info.DTID));
        DTID[strlen(device_info.DTID)] = '\0';
        ret = true;
    } else {
        wiimu_log(1, 0, 0, 0, "COMMS DTID for %s does not exits!!", device_info.deviceName);
        ret = false;
    }
    free_alexa_device_info(&device_info);
#endif

    printf("CHERYL DEBUG,avs_get_project_DTID DTID = %s\n", DTID);
    return ret;
}

static void *comms_process_monitor(void *arg)
{
    int pid;
    int status = 0;
    char DTID[64] = "";

    // SET DTID
    memset(DTID, 0x00, sizeof(DTID));
    if (!avs_get_project_DTID(DTID)) {
        g_have_asr_comms = 0;
        wiimu_log(1, 0, 0, 0, "%s ,get DTID failed,COMMS does not start!\n", __FUNCTION__);
        return (void *)-1;
    }

    while (1) {
        wiimu_log(1, 0, 0, 0, "comms_process_monitor device_status=%d!",
                  g_wiimu_shm->device_status);
        if (g_wiimu_shm->device_status == DEV_BURNING ||
            g_wiimu_shm->device_status == DEV_REBOOTING) {
            sleep(5);
            continue;
        }

        pid = fork();
        if (pid < 0) {
            perror("comms_process_monitor comms create failed\n");
            exit(-1);
        } else if (pid == 0) {
            execlp("asr_comms", "asr_comms", DTID, NULL);
            exit(-1);
        } else {
            wiimu_log(1, 0, 0, 0, "asr_comms fork %d\n", (int)pid);
            int quit_pid = waitpid(pid, &status, 0);
            wiimu_log(1, 0, 0, 0, "asr_comms child quit %d, status=%x\n", quit_pid, status);
        }
        sleep(3);
    }
    return (void *)NULL;
}

void start_comms_monitor(void)
{
    pthread_t tid;
    pthread_create(&tid, NULL, comms_process_monitor, NULL);
    wiimu_log(1, 0, 0, 0, "%s\n", __FUNCTION__);
}
