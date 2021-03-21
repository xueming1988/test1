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
#include "xiaowei_player.h"
#include "xiaowei_interface.h"
share_context *g_mplayer_shmem = NULL;
//#define TTS_URL_DEBUG 1
#ifdef TTS_URL_DEBUG
FILE *ftts_data = 0;
#endif

#define ALEXA_DEBUG_NONE 0x0000
#define ALEXA_DEBUG_INFO 0x0001
#define ALEXA_DEBUG_TRACE 0x0002
#define ALEXA_DEBUG_ERR 0x0004
#define ALEXA_DEBUG_ALL 0xFFFF

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
    char *dialog_id;

    int talk_continue;
    int need_resume;
    long long play_pos;
} wiimu_player_t;

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

typedef struct _play_list_item_s {
    int played;
    alexa_url_t url;
    struct lp_list_head list;
} play_list_item_t;

extern WIIMU_CONTEXT *g_wiimu_shm;
static pthread_mutex_t http_callback_cancel_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_cancel_http_callback = 0;

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

static wiimu_player_t *g_wiimu_player = NULL;

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

void *wiimu_player_proc(void *arg)
{
    wiimu_player_t *wiimu_player = (wiimu_player_t *)arg;

    URL_DEBUG("--------wiimu_player_proc--------\n");
    while (wiimu_player && wiimu_player->is_running) {
        struct lp_list_head *pos = NULL;
        struct lp_list_head *npos = NULL;
        play_list_item_t *list_item = NULL;

        pthread_mutex_lock(&wiimu_player->play_list_lock);
        if (!lp_list_empty(&wiimu_player->play_list)) {
            lp_list_for_each_safe(pos, npos, &wiimu_player->play_list)
            {
                play_list_item_t *list_item_ptr = lp_list_entry(pos, play_list_item_t, list);
                if (list_item_ptr->played == 0) {
                    list_item_ptr->played = 1;
                    // lp_list_del(&list_item->list);
                    list_item = wiimu_play_item_new(&list_item_ptr->url);
                    URL_DEBUG("--------going to start play--------\n");
                    break;
                } else {
                    list_item_ptr = NULL;
                    list_item = NULL;
                }
            }
        }
        pthread_mutex_unlock(&wiimu_player->play_list_lock);

        wiimu_player->play_stoped = 0;
        if (list_item && list_item->url.url && list_item->url.stream_id) {
            wiimu_player->play_pos = list_item->url.play_pos;
#if 0
            wiimu_player_ctrl_set_uri(wiimu_player->player_ctrl, list_item->url.stream_id, \
                                      list_item->url.stream_item, list_item->url.url, list_item->url.play_pos, \
                                      list_item->url.audible_book, list_item->url.is_dialog);
#endif
            long long cur_time = 0;
            long long interval_time = 0; // send playpos etc. to VAS
            long long delay_time = 0;

            int index_interval = 0;
            long less_interval = 0;
            long less_delay = 0;
            long long interval_ms = 0;

            while (wiimu_player->is_running) {
                cur_time = tickSincePowerOn();
                if (g_wiimu_shm->alexa_response_status == 1) {
                    usleep(10000);
                    // If tts is playing delay the report time, because the position is paused by
                    // tts
                    if (delay_time > 0) {
                        delay_time += 10;
                    }
                    if (interval_time > 0) {
                        interval_time += 10;
                    }
                    if (wiimu_player->play_stoped == 1) {
                        wiimu_player->play_stoped = 0;
                        URL_DEBUG("--------1 play stoped--------\n");
                        break;
                    } else {
                        continue;
                    }
                }

                int ret = wiimu_player_ctrl_update_info(wiimu_player->player_ctrl);
                if (wiimu_player->play_stoped == 1) {
                    wiimu_player->play_stoped = 0;
                    URL_DEBUG("--------2 play stoped--------\n");
                    break;
                }
                if ((wiimu_player->player_ctrl->error_code != 0 || ret == 1)) {
                    URL_DEBUG("--------error_code(%d) ret(%d) state(%d) pos(%lld)--------\n",
                              wiimu_player->player_ctrl->error_code, ret,
                              wiimu_player->player_ctrl->play_state, wiimu_player->play_pos);
                    if (list_item->url.failed == 0 &&
                        (wiimu_player->player_ctrl->error_code > 0 ||
                         wiimu_player->play_pos <= list_item->url.play_pos + 10)) {
                        // TODO: send a playback error event to avs
                        list_item->url.failed = 1;
                        URL_DEBUG("--------play failed-------- (%d %d) (%d %d) (%d %d) (%d %d)\n",
                                  wiimu_player->player_ctrl->error_code,
                                  wiimu_player->player_ctrl->play_state,
                                  (int)wiimu_player->play_pos, (int)list_item->url.play_pos,
                                  g_mplayer_shmem->err_count, g_mplayer_shmem->http_error_code,
                                  g_mplayer_shmem->status, g_wiimu_shm->play_status);
                        // alexa_audio_cmd(0, NAME_PLAYFAILED, list_item->url.stream_id, \
                                      //  (long)list_item->url.play_pos, 0, wiimu_player->player_ctrl->error_code);
                    } else if (list_item->url.finished == 0) {
                        // TODO: send a playback finished event to avs
                        list_item->url.finished = 1;
                        URL_DEBUG("--------play finished-------- (%d %d %d) (%d %d) (%d %d)\n",
                                  wiimu_player->player_ctrl->error_code,
                                  wiimu_player->player_ctrl->play_state,
                                  (int)wiimu_player->play_pos, g_mplayer_shmem->err_count,
                                  g_mplayer_shmem->http_error_code, g_mplayer_shmem->status,
                                  g_wiimu_shm->play_status);
                        int call_active = 0;
#ifdef EN_AVS_COMMS
                        call_active = avs_comms_is_avtive();
#endif
                        if (!call_active) {
                            if (list_item->url.need_next == 1) {
                                list_item->url.need_next = 0;
                                // alexa_audio_cmd(0, NAME_PLAYNEARLYFIN, list_item->url.stream_id, \
                                               // (long)wiimu_player->play_pos, 0, 0);
                            }
                            // alexa_audio_cmd(0, NAME_PLAYFINISHED, list_item->url.stream_id,
                            // (long)wiimu_player->play_pos, 0, 0);
                        } else {
                            URL_DEBUG("-------- the call is active --------\n");
                        }
                    }
                    break;
                }

                if (wiimu_player->player_ctrl->play_state == player_playing &&
                    list_item->url.started == 0 && list_item->url.failed == 0 &&
                    list_item->url.finished == 0) {
                    list_item->url.started = 1;
                    cur_time = tickSincePowerOn();
                    interval_time = cur_time - (long long)list_item->url.play_pos;
                    if (list_item->url.report_delay == 1) {
                        delay_time = cur_time + (long long)(list_item->url.play_report_delay -
                                                            list_item->url.play_pos);
                        delay_time -= wiimu_player->player_ctrl->position;
                        URL_DEBUG("-------- play delay report cur_time[%lld] delay_time[%lld] end "
                                  "--------\n",
                                  cur_time, delay_time);
                    } else {
                        delay_time = 0;
                    }
                    index_interval = 1;
                    less_interval = 0;
                    less_delay = 0;
                    // TODO: send a play started event to avs server
                    {
                        URL_DEBUG("--------play started-------- (%d %d %lld %lld, %lld) (%d %d) "
                                  "(%d %d)\n",
                                  wiimu_player->player_ctrl->error_code,
                                  wiimu_player->player_ctrl->play_state, wiimu_player->play_pos,
                                  wiimu_player->player_ctrl->position, list_item->url.play_pos,
                                  g_mplayer_shmem->err_count, g_mplayer_shmem->http_error_code,
                                  g_mplayer_shmem->status, g_wiimu_shm->play_status);
                        pthread_mutex_lock(&wiimu_player->state_lock);
                        os_free(wiimu_player->play_token);
                        wiimu_player->play_token = NULL;
                        pthread_mutex_unlock(&wiimu_player->state_lock);
                        //alexa_audio_cmd(0, NAME_PLAYSTARTED, list_item->url.stream_id, \
                                        //(long)list_item->url.play_pos, 0, 0);
                        URL_DEBUG("-------- play started end --------\n");
                    }
                } else if (wiimu_player->player_ctrl->play_state == player_playing) {
                    // sync the play position
                    wiimu_player->play_pos = wiimu_player->player_ctrl->position;
                    // Send the delay report event
                    if (0 < delay_time && 0 < list_item->url.play_report_delay) {
                        cur_time = tickSincePowerOn();
                        if (cur_time >= delay_time) {
                            less_delay = cur_time - delay_time;
                            // Wait 2000 ms for the offset arrive at play_report_delay
                            if (OFFSET_DEVIATION(wiimu_player->play_pos,
                                                 list_item->url.play_report_delay)) {
                                delay_time = 0;
                                less_delay = 0;
                                list_item->url.report_delay = 0;
                                URL_DEBUG(
                                    "-------- play delay report -------- (%d %d %lld %lld, %ld) "
                                    "(%d %d) (%d %d %lld)\n",
                                    wiimu_player->player_ctrl->error_code,
                                    wiimu_player->player_ctrl->play_state, wiimu_player->play_pos,
                                    wiimu_player->player_ctrl->position, list_item->url.play_pos,
                                    g_mplayer_shmem->err_count, g_mplayer_shmem->http_error_code,
                                    g_mplayer_shmem->status, g_wiimu_shm->play_status, less_delay);
                                // TODO:
                                // https://developer.amazon.com/docs/alexa-voice-service/audioplayer.html
                                // TODO:
                                // https://developer.amazon.com/docs/alexa-voice-service/audioplayer-overview.html
                                // alexa_audio_cmd(0, NAME_PLAYREPORTDELAY, list_item->url.stream_id, \
                                                //(long)wiimu_player->play_pos, 0, 0);
                                URL_DEBUG("-------- play delay report end --------\n");
                            }
                        }
                    }
#if 0
                    // Send the PlaybackNearFinished after start play about 3000 ms
                    if(list_item->url.need_next == 1 && list_item->url.started == 1) {
                        if(!StrInclude(list_item->url.stream_id, ALEXA_PLAY_MESSAGE)) {
                            long long next_time = 0;
                            if(list_item->url.play_report_delay < 3000) {
                                if(list_item->url.play_pos < list_item->url.play_report_delay) {
                                    next_time = (long long)list_item->url.play_report_delay + 2000;
                                } else {
                                    next_time = (long long)list_item->url.play_pos + 1000;
                                }
                            } else {
                                next_time = (long long)list_item->url.play_pos + 2000;
                            }
                            if(OFFSET_DEVIATION(wiimu_player->play_pos, next_time)) {
                                list_item->url.need_next = 0;
                                alexa_audio_cmd(0, NAME_PLAYNEARLYFIN, list_item->url.stream_id, \
                                                (long)wiimu_player->play_pos, 0, 0);
                            }
                        }
                    }
#endif
                    interval_ms = list_item->url.play_report_interval;
                    if (0 < interval_ms) {
                        cur_time = tickSincePowerOn();
                        if (cur_time >= (interval_ms + interval_time)) {
                            less_interval = cur_time - (interval_ms + interval_time);
                            long long next_interval =
                                (index_interval * interval_ms - list_item->url.play_pos);
                            // Wait 2000 ms for the offset arrive at next_interval
                            if (OFFSET_DEVIATION(wiimu_player->play_pos, next_interval) ||
                                (less_interval >= 2000)) {
                                long current_play_pos = (long)wiimu_player->play_pos;
                                interval_time += interval_ms;
                                index_interval++;
                                less_interval = 0;
// workaround for tunein certification issue
#if 0
                                if(list_item->url.url && StrSubStr(list_item->url.url, AVS_TUNE_RADIO_PREFIX)) {
                                    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "tunein offset %lld player %ld \n",
                                                next_interval + list_item->url.play_pos, current_play_pos);
                                    current_play_pos = next_interval + list_item->url.play_pos;
                                }
#endif
                                if (next_interval > 0) {
                                    URL_DEBUG(
                                        "-------- play delay interval report -------- (%d %d %lld "
                                        "%lld, %ld) (%d %d) (%d %d %lld)\n",
                                        wiimu_player->player_ctrl->error_code,
                                        wiimu_player->player_ctrl->play_state,
                                        wiimu_player->play_pos, wiimu_player->player_ctrl->position,
                                        list_item->url.play_pos, g_mplayer_shmem->err_count,
                                        g_mplayer_shmem->http_error_code, g_mplayer_shmem->status,
                                        g_wiimu_shm->play_status, less_interval);
                                    //alexa_audio_cmd(0, NAME_PLAYREPORTINTERVAL, list_item->url.stream_id, \
                                                    //current_play_pos, 0, 0);
                                    URL_DEBUG("-------- play delay interval report end --------\n");
                                }
                            }
                        }
                    }
                } else if (wiimu_player->player_ctrl->play_state == player_pausing) {
                    usleep(10000);
                    continue;
                } else if (wiimu_player->player_ctrl->play_state == player_loading) {
                    usleep(10000);
                    continue;
                } else if (wiimu_player->player_ctrl->play_state == player_stopped) {
                    usleep(10000);
                    URL_DEBUG("--------play stoped 2--------\n");
                    break;
                }

                usleep(10000);

                // just let the play status set to playing, which after send avs playstarted event
                if (wiimu_player->play_pos == list_item->url.play_pos) {
                    wiimu_player->play_pos = list_item->url.play_pos + 10;
                }
            }
        } else {
            usleep(10000);
        }

        if (list_item) {
            pthread_mutex_lock(&wiimu_player->play_list_lock);
            if (!lp_list_empty(&wiimu_player->play_list)) {
                lp_list_for_each_safe(pos, npos, &wiimu_player->play_list)
                {
                    play_list_item_t *list_item_tmp = lp_list_entry(pos, play_list_item_t, list);
                    if (list_item_tmp && list_item_tmp->played) {
                        lp_list_del(&list_item_tmp->list);
                        wiimu_play_item_free(&list_item_tmp);
                        break;
                    }
                }
            }
            pthread_mutex_unlock(&wiimu_player->play_list_lock);
            wiimu_play_item_free(&list_item);
        }
    }

    return NULL;
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
    if (notify == e_notify_started) {
        asr_set_audio_resume();
        WMUtil_Snd_Ctrl_SetProcessFadeStep(g_wiimu_shm, 20, SNDSRC_ALEXA_TTS);
        WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 0, SNDSRC_ALEXA_TTS, 1);
        WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 1, SNDSRC_ALEXA_TTS);
        WMUtil_Snd_Ctrl_SetProcessSelfVol(g_wiimu_shm, 100, SNDSRC_ALEXA_TTS);
        xiaowei_mute_tts_handle();
    } else if (notify == e_notify_finished) {
        if (!g_wiimu_shm->xiaowei_alarm_start) {
            set_asr_led(1);
        }
        WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 1, SNDSRC_ALEXA_TTS, 1);
        WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 100, SNDSRC_ALEXA_TTS);
        if (g_wiimu_shm->continue_query_mode == 1) {
            printf("%s:translate mode\n", __func__);
            onWakeUp_context();
        }
        xiaowei_control_handle();
    }
}

void cancel_http_callback(void)
{
    pthread_mutex_lock(&http_callback_cancel_mutex);
    g_cancel_http_callback = 1;
    pthread_mutex_unlock(&http_callback_cancel_mutex);
}

void set_http_callback_flag(int flag)
{
    pthread_mutex_lock(&http_callback_cancel_mutex);
    g_cancel_http_callback = flag;
    pthread_mutex_unlock(&http_callback_cancel_mutex);
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

void *wiimu_player_init(void)
{
    int ret = -1;
    wiimu_player_t *wiimplayer = NULL;

    wiimplayer = os_calloc(1, sizeof(wiimu_player_t));
    if (wiimplayer) {
        wiimplayer->is_running = 1;
        wiimplayer->cid_player = cid_player_create(cid_notify_cb, wiimplayer);
        if (!wiimplayer->cid_player) {
            goto Failed;
        }
        wiimplayer->player_ctrl = wiimu_player_ctrl_init();
        if (!wiimplayer->player_ctrl) {
            goto Failed;
        }
        pthread_mutex_init(&wiimplayer->play_list_lock, NULL);
        pthread_mutex_init(&wiimplayer->state_lock, NULL);

        LP_INIT_LIST_HEAD(&wiimplayer->play_list);
        wiimplayer->player_ctrl->volume = g_wiimu_shm->volume;
        wiimplayer->player_ctrl->muted = g_wiimu_shm->mute;
        /*ret = pthread_create(&wiimplayer->play_pid, NULL, wiimu_player_proc, (void *)wiimplayer);
        if (ret != 0) {
            goto Failed;
        }*/
        return wiimplayer;
    }

Failed:

    if (wiimplayer) {
        if (wiimplayer->cid_player) {
            cid_player_destroy(&wiimplayer->cid_player);
            wiimplayer->cid_player = NULL;
        }
        pthread_mutex_destroy(&wiimplayer->play_list_lock);
        wiimu_player_ctrl_uninit(&wiimplayer->player_ctrl);
        os_free(wiimplayer);
    }

    return NULL;
}

size_t tts_url_data_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
    int ret = 0;

    // this is only used for http handle exit,return 0 will exit curl_easy_peform block handle
    pthread_mutex_lock(&http_callback_cancel_mutex);
    if (g_cancel_http_callback == 1) {
        wiimu_log(1, 0, 0, 0, "%s:cancel tts httpcallback and exit \n", __func__);
        pthread_mutex_unlock(&http_callback_cancel_mutex);
        set_query_status(e_query_tts_cancel);
        return 0;
    } else {
        pthread_mutex_unlock(&http_callback_cancel_mutex);
    }
    if (g_wiimu_player && g_wiimu_player->cid_player) {
#ifdef TTS_URL_DEBUG
        fwrite(ptr, 1, size * nmemb, ftts_data);
#endif
        // wiimu_log(1, 0, 0, 0, "------%s:data_len=%d,g_id=%s\n", __func__, size * nmemb,
        // g_wiimu_player->dialog_id);
        ret = cid_player_write_data(g_wiimu_player->cid_player, g_wiimu_player->dialog_id, ptr,
                                    size * nmemb);
    }
    return size * nmemb;
}
#define HTTPS_CA_FILE ("/etc/ssl/certs/ca-certificates.crt")

void *tts_url_data_download(void *arg)
{
    char *url = NULL;
    url = (char *)arg;
    fprintf(stderr, "%s:before tts urltest url=%s\n", __func__, url);
    set_query_status(e_query_tts_start);
    CURL *pstContextCURL = NULL;
    int curl_count = 3;

    while (curl_count) {
        pstContextCURL = curl_easy_init();
        if (pstContextCURL) {
            // Set download URL.
            curl_easy_setopt(pstContextCURL, CURLOPT_URL, url);
            // curl_easy_setopt(pstContextCURL, CURLOPT_CONNECTIONTIMEOUT, 10);
            curl_easy_setopt(pstContextCURL, CURLOPT_TIMEOUT, 100);
            curl_easy_setopt(pstContextCURL, CURLOPT_VERBOSE, 1L);
            // set curl connect timeout
            // curl_easy_setopt(pstContextCURL, CURLOPT_CONNECTTIMEOUT, 5);
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
                if (g_cancel_http_callback == 1) {
                    g_cancel_http_callback = 0;
                    curl_easy_cleanup(pstContextCURL);
                    break;
                } else {
                    fprintf(stderr, "CurlHttpsRun curl_easy_perform() failed: %d,%s,try again\r\n",
                            stStatusCode, curl_easy_strerror(stStatusCode));
                    curl_easy_cleanup(pstContextCURL);
                }
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
    cid_player_cache_finished(g_wiimu_player->cid_player, g_wiimu_player->dialog_id);
    if (get_query_status() == e_query_tts_start)
        set_tts_download_finished();
    if (g_wiimu_player->dialog_id) {
        free(g_wiimu_player->dialog_id);
    }
    fprintf(stderr, "%s:after tts urltest url=%s,status=%d\n", __func__, url, get_query_status());
    if (url) {
        free(url);
    }
}

void start_tts_url_handle(char *url)
{
    int ret = 0;
    char *tmp = NULL;
    pthread_t tts_handle_thread;
    pthread_attr_t tts_handle_attr;
    pthread_attr_init(&tts_handle_attr);
    pthread_attr_setdetachstate(&tts_handle_attr, PTHREAD_CREATE_DETACHED);
    fprintf(stderr, "%s:url=%s\n", __func__, url);
    asprintf(&tmp, "%s", url);
    ret = pthread_create(&tts_handle_thread, &tts_handle_attr, tts_url_data_download, (void *)tmp);
    if (ret != 0) {
        fprintf(stderr, "%s:pthread create fail ret=%d\n", __func__, ret);
    }
}

int wiimu_player_start_buffer(wiimu_player_t *wiimu_player, const char *stream_id, const char *url,
                              int type, int is_next)
{
    cid_stream_t cid_stream;

    if (wiimu_player && wiimu_player->cid_player) {
        cid_stream.behavior = e_endqueue;
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
    char *dialog_id = NULL;
    if (!tts_url || !id) {
        return -1;
    }
    asprintf(&dialog_id, "%s", id);
    wiimu_player_start_buffer(g_wiimu_player, dialog_id, tts_url, e_stream_play, 0);
    g_wiimu_player->dialog_id = dialog_id;
#ifdef TTS_URL_DEBUG
    ftts_data = fopen("/tmp/tts.mp3", "w+");
#endif
    start_tts_url_handle(tts_url);
    wiimu_log(1, 0, 0, 0, "%s,after start tts thread\n", __func__);
#ifdef TTS_URL_DEBUG
    if (ftts_data) {
        fclose(ftts_data);
    }
#endif
}

int xiaowei_player_init(void)
{
    int shmid;
    /*
   * If mplayer shmem not create, then create it. else get it
   */
    shmid = shmget(34387, 4096, 0666 | IPC_CREAT | IPC_EXCL);
    if (shmid != -1) {
        g_mplayer_shmem = (share_context *)shmat(shmid, (const void *)0, 0);
    } else {
        printf("shmget error %d \n", errno);
        if (EEXIST == errno) {
            shmid = shmget(34387, 0, 0);
            if (shmid != -1) {
                g_mplayer_shmem = (share_context *)shmat(shmid, (const void *)0, 0);
            }
        } else {
            printf("shmget error %d \r\n", errno);
        }
    }

    if (!g_mplayer_shmem) {
        printf("g_mplayer_shmem create failed in asr_tts\r\n");
    } else {
        g_mplayer_shmem->err_count = 0;
    }
    g_wiimu_player = wiimu_player_init();
    return 0;
}
