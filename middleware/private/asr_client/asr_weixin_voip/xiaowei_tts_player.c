#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <curl/curl.h>
#include <sys/shm.h>
#include <errno.h>
#include "wm_util.h"
#include "lp_list.h"
#include "wiimu_player_ctrol.h"
#include "cid_player.h"
#include "xiaowei_tts_player.h"
#include "xiaowei_linkplay_interface.h"

share_context *g_mplayer_shmem = NULL;
extern WIIMU_CONTEXT *g_wiimu_shm;

//#define TTS_URL_DEBUG 1
#ifdef TTS_URL_DEBUG
FILE *ftts_data = 0;
#endif

#define ALEXA_DEBUG_NONE 0x0000
#define ALEXA_DEBUG_INFO 0x0001
#define ALEXA_DEBUG_TRACE 0x0002
#define ALEXA_DEBUG_ERR 0x0004
#define ALEXA_DEBUG_ALL 0xFFFF

typedef struct _alexa_url_s {
    int is_dialog;
    int audible_book;
    int type;
    int need_next;
    int url_invalid;

    int report_delay;
    int started;
    int stopped;
    int finished;
    int failed;

    char *stream_id;
    char *expect_stream_id;
    char *stream_item;
    char *url;
    long play_pos;
    long play_report_delay;
    long play_report_interval;

} alexa_url_t;

typedef struct _tts_resource_s {
    char *tts_dialog_id;
    char *tts_url;
    pthread_t tts_url_thread;
} tts_resource_t;

typedef struct _wiimu_player_s {
    int is_running;
    int play_stoped;
    int cmd_paused;

    pthread_t play_pid;
    pthread_mutex_t play_list_lock;
    struct lp_list_head play_list;

    cid_player_t *cid_player;
    player_ctrl_t *player_ctrl;

    pthread_mutex_t state_lock;
    char *speak_token;
    int speak_state;
    char *play_token;
    int play_state;
    tts_resource_t tts_resource;
#ifdef VOIP_SUPPORT
    char *voip_dialog_id;
#endif
    int talk_continue;
    int need_resume;
    long long play_pos;
} wiimu_player_t;

typedef struct _play_list_item_s {
    int played;
    alexa_url_t url;
    struct lp_list_head list;
} play_list_item_t;

#define PLAYER_MANAGER_DEBUG(fmt, ...)                                                             \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d][Player Manager] " fmt,                              \
                (int)(os_power_on_tick() / 3600000), (int)(os_power_on_tick() % 3600000 / 60000),  \
                (int)(os_power_on_tick() % 60000 / 1000), (int)(os_power_on_tick() % 1000),        \
                ##__VA_ARGS__);                                                                    \
    } while (0)

#define os_power_on_tick() tickSincePowerOn()

#define os_calloc(n, size) calloc((n), (size))
#define os_strdup(ptr) ((ptr != NULL) ? strdup(ptr) : NULL)

#define URL_DEBUG(fmt, ...)                                                                        \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d][URL Player] " fmt,                                  \
                (int)(os_power_on_tick() / 3600000), (int)(os_power_on_tick() % 3600000 / 60000),  \
                (int)(os_power_on_tick() % 60000 / 1000), (int)(os_power_on_tick() % 1000),        \
                ##__VA_ARGS__);                                                                    \
    } while (0)

#define os_free(ptr) ((ptr != NULL) ? free(ptr) : (void)ptr)

// For fixed avs certification, make the offset about 100 ms deviation
#define OFFSET_DEVIATION(x, y) (((x) + 100) > (y))

wiimu_player_t *g_wiimu_player = NULL;
#ifdef VOIP_SUPPORT
wiimu_player_t *g_voip_player = NULL;
#endif

static void wiimu_play_item_free(play_list_item_t **list_item_p)
{
    play_list_item_t *list_item = *list_item_p;
    if (list_item) {
        os_free(list_item->url.url);
        os_free(list_item->url.stream_id);
        os_free(list_item->url.expect_stream_id);
        os_free(list_item->url.stream_item);
        os_free(list_item);
        *list_item_p = NULL;
    }
}

// xiaowei tts player handle functions

static play_list_item_t *wiimu_play_item_new(alexa_url_t *url)
{
    int ret = -1;
    play_list_item_t *list_item = NULL;

    if (url) {
        list_item = os_calloc(1, sizeof(play_list_item_t));
        if (list_item) {
            memset(list_item, 0, sizeof(play_list_item_t));
            list_item->url.type = url->type;
            list_item->url.stream_id = os_strdup(url->stream_id);
            list_item->url.stream_item = os_strdup(url->stream_item);
            list_item->url.url = os_strdup(url->url);
            list_item->url.expect_stream_id = os_strdup(url->expect_stream_id);

            list_item->url.play_pos = url->play_pos;
            list_item->url.play_report_delay = url->play_report_delay;
            if (url->play_report_delay > 0) {
                list_item->url.report_delay = 1;
            }
            list_item->url.play_report_interval = url->play_report_interval;
            list_item->url.audible_book = url->audible_book;
            list_item->url.is_dialog = url->is_dialog;
            list_item->url.need_next = 1;

            list_item->url.url_invalid = 0;
        }
    }

    if (!list_item) {
        URL_DEBUG("-------- wiimu_play_item_new failed --------\n");
    }

    return list_item;
}

#define MIN_START_PROMPT_VOLUME 10

static void xiaowei_mute_tts_handle(void)
{
    if (g_wiimu_shm->mute == 1) {                            // mute status
        if (g_wiimu_shm->volume < MIN_START_PROMPT_VOLUME) { // temporarilly voice up prompt voice
            char buff[128] = {0};
            sprintf(buff, "setPlayerCmd:alexa:mutePrompt:%d", MIN_START_PROMPT_VOLUME);
            SocketClientReadWriteMsg("/tmp/Requesta01controller", buff, strlen(buff), 0, 0, 0);
        } else { // do not voice up prompt voice
            char buff[128] = {0};
            sprintf(buff, "setPlayerCmd:alexa:mutePrompt:%d", g_wiimu_shm->volume);
            SocketClientReadWriteMsg("/tmp/Requesta01controller", buff, strlen(buff), 0, 0, 0);
        }

        return;
    } else if (g_wiimu_shm->volume < MIN_START_PROMPT_VOLUME) { // volume 0 status
        char buff[128] = {0};
        int vol = g_wiimu_shm->volume;
        sprintf(buff, "setPlayerCmd:alexa:mutePrompt:%d", MIN_START_PROMPT_VOLUME);
        SocketClientReadWriteMsg("/tmp/Requesta01controller", buff, strlen(buff), 0, 0, 0);
        return;
    }
}
// some control handle need do after tts played
static void xiaowei_control_handle(void)
{
    if (g_wiimu_shm->xiaowei_control_id == xiaowei_cmd_bluetooth_mode_on) {
#ifdef A98_MARSHALL_STEPIN
        SocketClientReadWriteMsg("/tmp/bt_manager", "btconnectrequest:3",
                                 strlen("btconnectrequest:3"), NULL, NULL, 0);
#else
        SocketClientReadWriteMsg("/tmp/bt_manager", "btavkenterpair:notPlayTone",
                                 strlen("btavkenterpair:notPlayTone"), NULL, NULL, 0);
#endif
    } else if (g_wiimu_shm->xiaowei_control_id == xiaowei_cmd_shutdown) {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=VOICE_REQUEST_SHUTDOWN",
                                 strlen("GNOTIFY=VOICE_REQUEST_SHUTDOWN"), NULL, NULL, 0);
    } else if (g_wiimu_shm->xiaowei_control_id == xiaowei_cmd_reboot) {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=REBOOT", strlen("GNOTIFY=REBOOT"),
                                 NULL, 0, 0);
    }
}

int cid_notify_cb(const cid_notify_t notify, const cid_stream_t *cid_stream, void *notify_ctx)
{
    printf("-----%s---notify=%d --------\n", __func__, notify);
    if (notify_ctx) {
        if ((wiimu_player_t *)notify_ctx == g_voip_player) {
            wiimu_log(1, 0, 0, 0, "%s:voip player do not handle play callback\n", __func__);
            return 0;
        }
    }
    if (notify == e_notify_started) {
        asr_set_audio_resume();
        WMUtil_Snd_Ctrl_SetProcessFadeStep(g_wiimu_shm, 20, SNDSRC_ALEXA_TTS);
        WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 0, SNDSRC_ALEXA_TTS, 1);
        WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 1, SNDSRC_ALEXA_TTS);
        WMUtil_Snd_Ctrl_SetProcessSelfVol(g_wiimu_shm, 100, SNDSRC_ALEXA_TTS);
        xiaowei_mute_tts_handle();
    } else if (notify == e_notify_finished) {
        if (1 == get_duplex_mode()) {
            if (get_query_status() == e_query_idle) {
                set_asr_led(1);
            } else {
                set_asr_led(2);
            }
        } else if (g_wiimu_shm->wechatcall_status == e_wechatcall_talking) {
            set_asr_led(2);
        } else if (g_wiimu_shm->wechatcall_status == e_wechatcall_invited) {
            set_asr_led(4);
        } else {
            if (!g_wiimu_shm->xiaowei_alarm_start) {
                set_asr_led(1);
            }
        }
        if (g_wiimu_shm->wechatcall_status == e_wechatcall_request) {
            WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 0, SNDSRC_SMPLAYER);
            WechatcallRingPlayThread();
        } else {
            WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 1, SNDSRC_ALEXA_TTS, 1);
            WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 100, SNDSRC_ALEXA_TTS);
        }
        if (g_wiimu_shm->xiaowei_control_id == xiaowei_cmd_duplex_mode_on) {
            set_duplex_mode(1);
            g_wiimu_shm->xiaowei_control_id = 0;
            duplex_talkstart();
        }
        if (0 == get_duplex_mode()) {
            set_query_status(e_query_tts_end);
        }

        if (g_wiimu_shm->continue_query_mode == 1) {
            printf("%s:translate mode\n", __func__);
            onWakeUp_context();
        }
        xiaowei_control_handle();
    }
}

int avs_player_buffer_stop(char *contend_id, int type)
{
    int ret = -1;

    if (g_wiimu_player && g_wiimu_player->cid_player) {
        if (type == e_asr_start) {
            pthread_mutex_lock(&g_wiimu_player->state_lock);
            g_wiimu_player->talk_continue = 0;
            if (g_wiimu_player->play_token && g_wiimu_player->play_state == PLAY_STATE_PLAYING) {
                g_wiimu_player->need_resume = 1;
            }
            pthread_mutex_unlock(&g_wiimu_player->state_lock);
            cid_player_set_cmd(g_wiimu_player->cid_player, e_cmd_clear_queue);
        } else if (type == e_asr_stop) {
            pthread_mutex_lock(&g_wiimu_player->state_lock);
            g_wiimu_player->talk_continue = 0;
            g_wiimu_player->need_resume = 0;
            pthread_mutex_unlock(&g_wiimu_player->state_lock);
            cid_player_set_cmd(g_wiimu_player->cid_player, e_cmd_clear_queue);
        } else if (type == e_asr_finished) {
            cid_player_cache_finished(g_wiimu_player->cid_player, contend_id);
        }
    }

    return 0;
}

size_t tts_url_data_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
    int ret = 0;

    if (g_wiimu_player && g_wiimu_player->cid_player &&
        g_wiimu_player->tts_resource.tts_dialog_id) {
#ifdef TTS_URL_DEBUG
        fwrite(ptr, 1, size * nmemb, ftts_data);
#endif
        // wiimu_log(1, 0, 0, 0, "------%s:data_len=%d,g_id=%s\n", __func__, size * nmemb,
        // g_wiimu_player->dialog_id);
        ret = cid_player_write_data(g_wiimu_player->cid_player,
                                    g_wiimu_player->tts_resource.tts_dialog_id, ptr, size * nmemb);
    }
    return size * nmemb;
}
#define HTTPS_CA_FILE ("/etc/ssl/certs/ca-certificates.crt")

void *tts_url_data_download(void *arg)
{
    tts_resource_t *tmp_tts_resource = NULL;
    tmp_tts_resource = (tts_resource_t *)arg;
    if (!arg || !tmp_tts_resource->tts_url) {
        wiimu_log(1, 0, 0, 0, "%s:arg is null,something wrong\n", __func__);
        return;
    }

    fprintf(stderr, "%s:before tts urltest url=%s\n", __func__, tmp_tts_resource->tts_url);
    if (0 == get_duplex_mode()) {
        set_query_status(e_query_tts_start);
    }
    CURL *pstContextCURL = NULL;
    int curl_count = 1;

    while (curl_count) {
        pstContextCURL = curl_easy_init();
        if (pstContextCURL) {
            // Set download URL.
            curl_easy_setopt(pstContextCURL, CURLOPT_URL, tmp_tts_resource->tts_url);
            // curl_easy_setopt(pstContextCURL, CURLOPT_CONNECTIONTIMEOUT, 10);
            curl_easy_setopt(pstContextCURL, CURLOPT_TIMEOUT, 100);
            curl_easy_setopt(pstContextCURL, CURLOPT_VERBOSE, 1L);
            // set curl connect timeout
            curl_easy_setopt(pstContextCURL, CURLOPT_CONNECTTIMEOUT, 5);
            // curl_easy_setopt(pstContextCURL, CURLOPT_USERAGENT, "libcurl/" LIBCURL_VERSION);
            // curl_easy_setopt(pstContextCURL, CURLOPT_SSL_VERIFYPEER, 0L);
            // curl_easy_setopt(pstContextCURL, CURLOPT_SSL_VERIFYHOST, 2L);
            // curl_easy_setopt(pstContextCURL, CURLOPT_CAINFO, HTTPS_CA_FILE);
            // curl_easy_setopt(pstContextCURL, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

            // Set parameter for write callback.
            curl_easy_setopt(pstContextCURL, CURLOPT_WRITEDATA, NULL);
            // curl_easy_setopt(pstContextCURL, CURLOPT_WRITEDATA, NULL);
            // Set writing body to memory.
            curl_easy_setopt(pstContextCURL, CURLOPT_WRITEFUNCTION, tts_url_data_callback);
            // Run
            CURLcode stStatusCode = curl_easy_perform(pstContextCURL);
            if (stStatusCode != CURLE_OK) {
                fprintf(stderr, "CurlHttpsRun curl_easy_perform() failed: %d,%s,try again\r\n",
                        stStatusCode, curl_easy_strerror(stStatusCode));
                curl_easy_cleanup(pstContextCURL);
            } else {
                curl_easy_cleanup(pstContextCURL);
                break;
            }

        } else {
            printf("%s:create curl handle err\n", __func__);
        }
        curl_count--;
        sleep(4);
    }

    // after tts block download,do some xiaowei process
    cid_player_cache_finished(g_wiimu_player->cid_player, tmp_tts_resource->tts_dialog_id);
    if (get_query_status() == e_query_tts_start && (0 == get_duplex_mode()))
        set_tts_download_finished();
    if (tmp_tts_resource->tts_dialog_id) {
        free(tmp_tts_resource->tts_dialog_id);
        tmp_tts_resource->tts_dialog_id = NULL;
    }
    if (tmp_tts_resource->tts_url) {
        free(tmp_tts_resource->tts_url);
        tmp_tts_resource->tts_url = NULL;
    }
    fprintf(stderr, "%s:after tts urltest status=%d\n", __func__, get_query_status());
}

pthread_t start_tts_url_handle(tts_resource_t *_tts_resource)
{
    int ret = 0;
    char *tmp = NULL;
    pthread_t tts_handle_thread;
    pthread_attr_t tts_handle_attr;
    pthread_attr_init(&tts_handle_attr);
    pthread_attr_setdetachstate(&tts_handle_attr, PTHREAD_CREATE_JOINABLE);
    fprintf(stderr, "%s:url=%s\n", __func__, _tts_resource->tts_url);
    ret = pthread_create(&tts_handle_thread, &tts_handle_attr, tts_url_data_download,
                         (void *)_tts_resource);
    if (ret != 0) {
        fprintf(stderr, "%s:pthread create fail ret=%d\n", __func__, ret);
    }
    return tts_handle_thread;
}

int wiimu_player_start_buffer(wiimu_player_t *wiimu_player, const char *stream_id, const char *url,
                              int type, int is_next)
{
    cid_stream_t cid_stream;

    if (wiimu_player && wiimu_player->cid_player) {
        cid_stream.behavior = e_replace_all;
        cid_stream.cid_type = e_type_buffer_mp3;
        cid_stream.stream_type = type;
        cid_stream.token = stream_id;
        cid_stream.next_cached = is_next;
        cid_stream.uri = url;
        cid_stream.dialog_id = NULL;
        cid_player_add_item(wiimu_player->cid_player, cid_stream);
    }

    return 0;
}

int xiaowei_play_tts_url(char *tts_url, char *id)
{
    int ret = 0;
    char *tmp_tts_dialog_id = NULL;
    char *tmp_tts_url = NULL;
    if (!tts_url || !id) {
        return -1;
    }
    if (g_wiimu_player->tts_resource.tts_url_thread) {
        wiimu_log(1, 0, 0, 0, "%s,before join tts_url__thread\n", __func__);
        pthread_join(g_wiimu_player->tts_resource.tts_url_thread, NULL);
        wiimu_log(1, 0, 0, 0, "%s,after join tts_url__thread\n", __func__);
    }
    asprintf(&tmp_tts_dialog_id, "%s", id);
    asprintf(&tmp_tts_url, "%s", tts_url);
    wiimu_player_start_buffer(g_wiimu_player, tmp_tts_dialog_id, tmp_tts_url, e_stream_play, 0);
    g_wiimu_player->tts_resource.tts_dialog_id = tmp_tts_dialog_id;
    g_wiimu_player->tts_resource.tts_url = tmp_tts_url;
#ifdef TTS_URL_DEBUG
    ftts_data = fopen("/tmp/tts.mp3", "w+");
#endif

    g_wiimu_player->tts_resource.tts_url_thread =
        start_tts_url_handle(&(g_wiimu_player->tts_resource));
    wiimu_log(1, 0, 0, 0, "%s,after start tts thread\n", __func__);
#ifdef TTS_URL_DEBUG
    if (ftts_data) {
        fclose(ftts_data);
    }
#endif
}

int wiimu_player_wechatcall_start_buffer(wiimu_player_t *wiimu_player, const char *stream_id,
                                         const char *url, int type, int is_next)
{
    cid_stream_t cid_stream;

    if (wiimu_player && wiimu_player->cid_player) {
        cid_stream.behavior = e_endqueue;
        cid_stream.cid_type = e_type_buffer_pcm;
        cid_stream.stream_type = type;
        cid_stream.token = stream_id;
        cid_stream.next_cached = is_next;
        cid_stream.uri = url;
        cid_stream.dialog_id = NULL;
        cid_player_add_item(wiimu_player->cid_player, cid_stream);
    }

    return 0;
}

#ifdef VOIP_SUPPORT
int wechatcall_pcm_play_start(void)
{
    char *dialog_id = NULL;

    asprintf(&dialog_id, "wechatcall pcm");
    wiimu_player_wechatcall_start_buffer(g_voip_player, "wechatcall pcm", "wechatcall pcm",
                                         e_stream_play, 0);

    g_voip_player->voip_dialog_id = dialog_id;
    return 0;
}

int wechatcall_pcm_play(char *buf, int len)
{
    cid_player_write_data(g_voip_player->cid_player, g_voip_player->voip_dialog_id, buf, len);
}

int wechatcall_pcm_play_finish(void)
{
    cid_player_cache_finished(g_voip_player->cid_player, g_voip_player->voip_dialog_id);
    if (g_voip_player->voip_dialog_id) {
        free(g_voip_player->voip_dialog_id);
    }
}

#endif
/*add for xiaowei_url_player handle*/

void *wiimu_player_init(void)
{
    int ret = -1;
    wiimu_player_t *wiimplayer = NULL;

    wiimplayer = os_calloc(1, sizeof(wiimu_player_t));
    if (wiimplayer) {
        wiimplayer->is_running = 1;
        // tts player create
        wiimplayer->cid_player = cid_player_create(cid_notify_cb, wiimplayer);
        if (!wiimplayer->cid_player) {
            goto Failed;
        }

        pthread_mutex_init(&wiimplayer->play_list_lock, NULL);
        pthread_mutex_init(&wiimplayer->state_lock, NULL);

        LP_INIT_LIST_HEAD(&wiimplayer->play_list);

        return wiimplayer;
    }

Failed:

    if (wiimplayer) {
        if (wiimplayer->cid_player) {
            cid_player_destroy(&wiimplayer->cid_player);
            wiimplayer->cid_player = NULL;
        }

        pthread_mutex_destroy(&wiimplayer->play_list_lock);
        os_free(wiimplayer);
    }

    return NULL;
}

void mplayer_shmem_get(share_context **mplayer_shmem)
{
    int shmid;
    /*
   * If mplayer shmem not create, then create it. else get it
   */
    shmid = shmget(34387, 4096, 0666 | IPC_CREAT | IPC_EXCL);
    if (shmid != -1) {
        *mplayer_shmem = (share_context *)shmat(shmid, (const void *)0, 0);
    } else {
        printf("shmget error %d \n", errno);
        if (EEXIST == errno) {
            shmid = shmget(34387, 0, 0);
            if (shmid != -1) {
                *mplayer_shmem = (share_context *)shmat(shmid, (const void *)0, 0);
            }
        } else {
            printf("22shmget error %d \r\n", errno);
        }
    }

    if (!(*mplayer_shmem)) {
        printf("g_mplayer_shmem create failed in asr_tts\r\n");
    } else {
        (*mplayer_shmem)->err_count = 0;
    }
}

int xiaowei_tts_player_init(void)
{

    mplayer_shmem_get(&g_mplayer_shmem);
    g_wiimu_player = wiimu_player_init();
#ifdef VOIP_SUPPORT
    g_voip_player = wiimu_player_init();
#endif
    return 0;
}
