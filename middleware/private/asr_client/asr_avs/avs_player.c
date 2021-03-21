/*************************************************************************
    > File Name: url_player.c
    > Author:
    > Mail:
    > Created Time: Tue 21 Nov 2017 02:35:49 AM EST
 ************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "wm_util.h"
#include "lp_list.h"

#include "cmd_q.h"

#include "alexa_cmd.h"
#include "avs_player.h"
#include "cid_player.h"
#include "wiimu_player_ctrol.h"

#include "alexa_debug.h"
#include "alexa_json_common.h"
#include "alexa_request_json.h"
#include "alexa_bluetooth.h"
#include "alexa_focus_mgmt.h"
#include "alexa_input_controller.h"

#ifdef EN_AVS_COMMS
#include "alexa_comms.h"
#endif

extern WIIMU_CONTEXT *g_wiimu_shm;
extern share_context *g_mplayer_shmem;
extern void ASR_SetRecording_TimeOut(int time_out);

#define os_calloc(n, size) calloc((n), (size))

#define os_free(ptr) ((ptr != NULL) ? free(ptr) : (void)ptr)

#define os_strdup(ptr) ((ptr != NULL) ? strdup(ptr) : NULL)

#define os_power_on_tick() tickSincePowerOn()

#define URL_DEBUG(fmt, ...)                                                                        \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d][URL Player] " fmt,                                  \
                (int)(os_power_on_tick() / 3600000), (int)(os_power_on_tick() % 3600000 / 60000),  \
                (int)(os_power_on_tick() % 60000 / 1000), (int)(os_power_on_tick() % 1000),        \
                ##__VA_ARGS__);                                                                    \
    } while (0)

enum {
    TTS_STATE_IDLE,
    TTS_STATE_INIT,
    TTS_STATE_PLAYING,
};

enum {
    REPLACE_ALL,
    REPLACE_ENQUEUED,
    ENQUEUE,
    NONE,
};

enum {
    PLAY_STATE_IDLE,
    PLAY_STATE_PLAYING,
    PLAY_STATE_PAUSED,
    PLAY_STATE_BUFFER_UNDERRUN,
    PLAY_STATE_FINISHED,
    PLAY_STATE_STOPPED,
    PLAY_NONE,
};

#define IS_PLAY_BOOK(type)                                                                         \
    (type == TYPE_PLAY_AUDIBLEBOOK || type == TYPE_PLAY_KINDLEBOOK ||                              \
     type == TYPE_PLAY_DAILYBRIEFING)

#define URL_INVALID(audio_url)                                                                     \
    ((StrSubStr(audio_url, IVALIAD_TOKEN) || StrSubStr(audio_url, BAD_HOST)))

// For fixed avs certification, make the offset about 100 ms deviation
#define OFFSET_DEVIATION(x, y) (((x) + 100) > (y))

/*
 * @type: end_queue, replace all, replace endqueue
 *      end_queue:
 *      replace all: (replace  all and and play right now)
 *      replace endqueue:
 * @stream_id: the stream_id of music
 * @url: the url which to play
 * @pos: the pos where to play
 * @play_report_delay: the pos where to play
 * @play_report_interval: the pos where to play
 */
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

static wiimu_player_t *g_wiimu_player = NULL;

static play_list_item_t *wiimu_play_item_new(alexa_url_t *url)
{
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

            if (URL_INVALID(list_item->url.url)) {
                list_item->url.url_invalid = 1;
            } else {
                list_item->url.url_invalid = 0;
            }
        }
    }

    if (!list_item) {
        URL_DEBUG("-------- wiimu_play_item_new failed --------\n");
    }

    return list_item;
}

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

static void wiimu_clear_play_list(wiimu_player_t *wiimu_player)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    play_list_item_t *list_item = NULL;

    if (wiimu_player) {
        pthread_mutex_lock(&wiimu_player->play_list_lock);
        if (!lp_list_empty(&wiimu_player->play_list)) {
            lp_list_for_each_safe(pos, npos, &wiimu_player->play_list)
            {
                list_item = lp_list_entry(pos, play_list_item_t, list);
                if (list_item) {
                    lp_list_del(&list_item->list);
                    wiimu_play_item_free(&list_item);
                }
            }
        }
        pthread_mutex_unlock(&wiimu_player->play_list_lock);
    }
}

static int wiimu_play_check_expect_token(wiimu_player_t *wiimu_player, alexa_url_t *new_url)
{
    int ret = 0;
    int number = 0;
    int cnt = 0;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    play_list_item_t *list_item = NULL;

    if (wiimu_player && new_url) {
        pthread_mutex_lock(&wiimu_player->play_list_lock);
        number = lp_list_number(&wiimu_player->play_list);
        if (number > 0) {
            lp_list_for_each_safe(pos, npos, &wiimu_player->play_list)
            {
                cnt++;
                list_item = lp_list_entry(pos, play_list_item_t, list);
                if (number == cnt) {
                    break;
                }
            }
            URL_DEBUG("--------expect_stream_id=%s stream_id=%s--------\n",
                      new_url->expect_stream_id, list_item->url.stream_id);
            if (list_item && new_url->expect_stream_id &&
                !StrEq(new_url->expect_stream_id, list_item->url.stream_id)) {
                ret = 1;
            }
        }
        pthread_mutex_unlock(&wiimu_player->play_list_lock);
    }

    return ret;
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
            wiimu_player_ctrl_set_uri(wiimu_player->player_ctrl, list_item->url.stream_id,
                                      list_item->url.stream_item, list_item->url.url,
                                      list_item->url.play_pos, list_item->url.audible_book,
                                      list_item->url.is_dialog);
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
                        alexa_audio_cmd(0, NAME_PLAYFAILED, list_item->url.stream_id,
                                        (long)list_item->url.play_pos, 0,
                                        wiimu_player->player_ctrl->error_code);
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
                                alexa_audio_cmd(0, NAME_PLAYNEARLYFIN, list_item->url.stream_id,
                                                (long)wiimu_player->play_pos, 0, 0);
                            }
                            alexa_audio_cmd(0, NAME_PLAYFINISHED, list_item->url.stream_id,
                                            (long)wiimu_player->play_pos, 0, 0);
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
                    if (!URL_INVALID(list_item->url.url)) {
                        URL_DEBUG("--------play started-------- (%d %d %lld %lld, %ld) (%d %d) "
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
                        alexa_audio_cmd(0, NAME_PLAYSTARTED, list_item->url.stream_id,
                                        (long)list_item->url.play_pos, 0, 0);
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
                                    "(%d %d) (%d %d %ld)\n",
                                    wiimu_player->player_ctrl->error_code,
                                    wiimu_player->player_ctrl->play_state, wiimu_player->play_pos,
                                    wiimu_player->player_ctrl->position, list_item->url.play_pos,
                                    g_mplayer_shmem->err_count, g_mplayer_shmem->http_error_code,
                                    g_mplayer_shmem->status, g_wiimu_shm->play_status, less_delay);
                                // TODO:
                                // https://developer.amazon.com/docs/alexa-voice-service/audioplayer.html
                                // TODO:
                                // https://developer.amazon.com/docs/alexa-voice-service/audioplayer-overview.html
                                alexa_audio_cmd(0, NAME_PLAYREPORTDELAY, list_item->url.stream_id,
                                                (long)wiimu_player->play_pos, 0, 0);
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
                                if (list_item->url.url &&
                                    StrSubStr(list_item->url.url, AVS_TUNE_RADIO_PREFIX)) {
                                    ALEXA_PRINT(
                                        ALEXA_DEBUG_TRACE, "tunein offset %lld player %ld \n",
                                        next_interval + list_item->url.play_pos, current_play_pos);
                                    current_play_pos = next_interval + list_item->url.play_pos;
                                }
                                if (next_interval > 0) {
                                    URL_DEBUG(
                                        "-------- play delay interval report -------- (%d %d %lld "
                                        "%lld, %ld) (%d %d) (%d %d %ld)\n",
                                        wiimu_player->player_ctrl->error_code,
                                        wiimu_player->player_ctrl->play_state,
                                        wiimu_player->play_pos, wiimu_player->player_ctrl->position,
                                        list_item->url.play_pos, g_mplayer_shmem->err_count,
                                        g_mplayer_shmem->http_error_code, g_mplayer_shmem->status,
                                        g_wiimu_shm->play_status, less_interval);
                                    alexa_audio_cmd(0, NAME_PLAYREPORTINTERVAL,
                                                    list_item->url.stream_id, current_play_pos, 0,
                                                    0);
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

int cid_notify_cb(const cid_notify_t notify, const cid_stream_t *cid_stream, void *notify_ctx)
{
    wiimu_player_t *player_manager = (wiimu_player_t *)notify_ctx;
    URL_DEBUG("--------notify=%d --------\n", notify);

    if (cid_stream && player_manager) {
        pthread_mutex_lock(&player_manager->state_lock);
        if (cid_stream->stream_type == e_stream_speak) {
            if (e_notify_started == notify) {
                os_free(player_manager->speak_token);
                player_manager->speak_token = os_strdup(cid_stream->token);
                player_manager->speak_state = TTS_STATE_PLAYING;
                focus_mgmt_activity_status_set(SpeechSynthesizer, FOCUS);
            } else {
                focus_mgmt_activity_status_set(SpeechSynthesizer, UNFOCUS);
                if (e_notify_finished == notify) {
                    player_manager->speak_state = TTS_STATE_IDLE;
                }
            }
            player_manager->play_state = PLAY_STATE_IDLE;
        } else if (cid_stream->stream_type == e_stream_play) {
            if (e_notify_started == notify) {
                os_free(player_manager->play_token);
                player_manager->play_token = os_strdup(cid_stream->token);
                player_manager->play_state = PLAY_STATE_PLAYING;
            } else if (e_notify_finished == notify) {
                player_manager->need_resume = 0;
                player_manager->play_state = PLAY_STATE_FINISHED;
            }
            player_manager->speak_state = TTS_STATE_IDLE;
        }
        pthread_mutex_unlock(&player_manager->state_lock);
    }

    if (e_notify_end == notify) {
        if (player_manager) {
            pthread_mutex_lock(&player_manager->state_lock);
            if (player_manager->talk_continue) {
                player_manager->talk_continue = 0;
                NOTIFY_CMD(ASR_TTS, TLK_CONTINUE);
            } else {
#ifdef EN_AVS_BLE
                avs_bluetooth_do_cmd(1);
#endif
                avs_input_controller_do_cmd();
                if (g_wiimu_shm->asr_session_start == 0) {
                    NOTIFY_CMD(IO_GUARD, TTS_IDLE);
                    NOTIFY_CMD(ASR_TTS, FocusManage_TTSEnd);
                }
            }
            player_manager->speak_state = TTS_STATE_IDLE;
            pthread_mutex_unlock(&player_manager->state_lock);
        }
    } else if (e_notify_started == notify) {
        NOTIFY_CMD(IO_GUARD, PROMPT_START);
        if (g_wiimu_shm->asr_session_start == 0) {
            NOTIFY_CMD(IO_GUARD, TTS_IS_PLAYING);
            NOTIFY_CMD(ASR_TTS, FocusManage_TTSBegin);
        }
        if (cid_stream && cid_stream->token) {
            if (cid_stream->stream_type == e_stream_speak) {
                alexa_speech_cmd(0, NAME_SPEECHSTARTED, cid_stream->token);
            } else if (cid_stream->stream_type == e_stream_play) {
                alexa_audio_cmd(0, NAME_PLAYSTARTED, cid_stream->token, 0, 0, 0);
            }
        }
    } else if (e_notify_stopped == notify) {
        if (cid_stream && cid_stream->token && cid_stream->stream_type == e_stream_play) {
            NOTIFY_CMD(IO_GUARD, PROMPT_END);
            pthread_mutex_lock(&player_manager->state_lock);
            player_manager->play_state = PLAY_STATE_STOPPED;
#if 0
            alexa_audio_cmd(0, NAME_STOPPED, cid_stream->token, 0, 0, 0);
#endif
            pthread_mutex_unlock(&player_manager->state_lock);
        }
    } else if (e_notify_near_finished == notify) {
        if (cid_stream && cid_stream->token && cid_stream->stream_type == e_stream_play) {
            alexa_audio_cmd(0, NAME_PLAYNEARLYFIN, cid_stream->token, 0, 0, 0);
        }
    } else if (e_notify_finished == notify) {
        NOTIFY_CMD(IO_GUARD, PROMPT_END);
        if (cid_stream && cid_stream->token) {
            if (cid_stream->stream_type == e_stream_speak) {
                alexa_speech_cmd(0, NAME_SPEECHFINISHED, cid_stream->token);
            } else if (cid_stream->stream_type == e_stream_play) {
                alexa_audio_cmd(0, NAME_PLAYFINISHED, cid_stream->token, 0, 0, 0);
            }
        }
    }

    return 0;
}

/*
 * Init the player
 * Input :
 *
 * Output:
 *      return the player wiimu_player point
 */
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
        ret = pthread_create(&wiimplayer->play_pid, NULL, wiimu_player_proc, (void *)wiimplayer);
        if (ret != 0) {
            goto Failed;
        }
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

/*
 * uninit the player
 * Input :
 *         @wiimu_player: the player wiimu_player
 *
 * Output:
 *         0 sucess
 *         other failed
 */
int wiimu_player_uninit(wiimu_player_t **wiimu_player_p)
{
    wiimu_player_t *wiimu_player = *wiimu_player_p;

    if (wiimu_player) {
        wiimu_clear_play_list(wiimu_player);
        wiimu_player->play_stoped = 1;
        wiimu_player->is_running = 0;

        if (wiimu_player->play_pid > 0) {
            pthread_join(wiimu_player->play_pid, NULL);
        }

        if (wiimu_player->cid_player) {
            cid_player_destroy(&wiimu_player->cid_player);
            wiimu_player->cid_player = NULL;
        }
        wiimu_player_ctrl_uninit(&wiimu_player->player_ctrl);
        pthread_mutex_destroy(&wiimu_player->play_list_lock);
        os_free(wiimu_player);
    }

    return 0;
}

/*
 * stop/pause current music
 * Input :
 *         @wiimu_player: the player wiimu_player
 *         @is_stop:
 *                   1 : stop the music
 *                   0 : pause the music
 *
 * Output:
 *         the position where stoped/paused
 */
int wiimu_player_stop(wiimu_player_t *wiimu_player, int is_stop)
{
    int ret = -1;

    if (wiimu_player) {
        if (is_stop) {
            wiimu_player->play_stoped = 1;
            ret = wiimu_player_ctrl_stop(wiimu_player->player_ctrl, 1);
        } else {
            ret = wiimu_player_ctrl_pasue(wiimu_player->player_ctrl);
        }
    }

    if (ret == 0 && wiimu_player->player_ctrl) {
        pthread_mutex_lock(&wiimu_player->player_ctrl->ctrl_lock);
        if (!URL_INVALID(wiimu_player->player_ctrl->stream_url)) {
            alexa_audio_cmd(0, NAME_STOPPED, wiimu_player->player_ctrl->stream_id,
                            (long)wiimu_player->player_ctrl->position, 0, 0);
        }
        pthread_mutex_unlock(&wiimu_player->player_ctrl->ctrl_lock);
    }

    return ret;
}

/*
 * add a url to player
 * Input :
 *         @wiimu_player: the player wiimu_player
 *         @url: the alexa url type
 *
 * Output:
 *         0 sucess
 *         other failed
 */
int wiimu_player_play(wiimu_player_t *wiimu_player, alexa_url_t *url)
{
    int ret = -1;
    play_list_item_t *list_item = NULL;

    if (wiimu_player && url) {
        ret = wiimu_play_check_expect_token(wiimu_player, url);
        if (ret == 0) {
            list_item = wiimu_play_item_new(url);
            if (list_item) {
                if (list_item->url.type != ENQUEUE) {
                    if (list_item->url.type == REPLACE_ALL) {
#ifdef A98_FENGHUO
                        switch_mode_set_keep_wifi_mode(g_wiimu_shm, 1);
#endif
                        wiimu_clear_play_list(wiimu_player);
                        wiimu_player->play_stoped = 1;
                        if (wiimu_player_ctrl_is_new(wiimu_player->player_ctrl, url->stream_id,
                                                     url->url, url->play_pos, url->audible_book)) {
                            wiimu_player_stop(wiimu_player, 1);
                        }
                        // cid_player_set_cmd(wiimu_player->cid_player, e_cmd_clear_queue);
                        NOTIFY_CMD(ALEXA_CMD_FD, EVENT_CLEAR_NEXT);
                        URL_DEBUG("++++++++ clear play ++++++++\n");
                    } else if (list_item->url.type == REPLACE_ENQUEUED) {
                        wiimu_clear_play_list(wiimu_player);
                    }
                }
                URL_DEBUG("type=%d pos=%ld report_delay=%ld "
                          "report_interval=%ld\nstream_id:%.800s\nurl=%.800s\n",
                          url->type, url->play_pos, url->play_report_delay,
                          url->play_report_interval, url->stream_id, url->url);
                pthread_mutex_lock(&wiimu_player->play_list_lock);
                lp_list_add_tail(&list_item->list, &wiimu_player->play_list);
                pthread_mutex_unlock(&wiimu_player->play_list_lock);
                ret = 0;
            }
        }
    }

    return ret;
}

/*
 * get current position
 * Input :
 *         @wiimu_player: the player wiimu_player
 *
 * Output:
 *         0 sucess
 *         other failed
 */
long long wiimu_player_get_pos(wiimu_player_t *wiimu_player)
{
    if (wiimu_player) {
        URL_DEBUG("current pos is %lld\n", wiimu_player->play_pos);
        return wiimu_player->play_pos;
    }

    return 0;
}

/*
 * get the current volume/mute
 * Input :
 *         @wiimu_player: the player wiimu_player
 *         @get_mute:  0 : get volume  1 : get mute
 *
 * Output:
 *          when set_mute = 0 return the current volume(0 ~ 100)
 *          when set_mute = 1 return the current mute(0/1)
 */
int wiimu_player_get_vol(wiimu_player_t *wiimu_player, int get_mute)
{
    if (wiimu_player) {
        return wiimu_player->player_ctrl->volume;
    }

    return 0;
}

/*
 * set volume/mute
 * Input :
 *         @wiimu_player: the player wiimu_player
 *         @set_mute:  0 : set volume  1 : set mute
 *         @val:  when set_mute = 0 the val is (0 ~ 100), other val is (0/1)
 *
 * Output:
 *         0 sucess
 *         other failed
 */
int wiimu_player_set_vol(wiimu_player_t *wiimu_player, int set_mute, int val)
{
    int ret = -1;
    URL_DEBUG("wiimu_player_set_vol set_mute=%d val=%d\n", set_mute, val);

    if (wiimu_player) {
        ret = wiimu_player_ctrl_set_volume(wiimu_player->player_ctrl, set_mute, val);
        usleep(100000);
    }

    return ret;
}

/*
 * Init the mp3 player
 * Input :
 *         @wiimu_player: the player wiimu_player
 *         @stream_id:
 *
 * Output:
 *         the size of buffer player
 */
int wiimu_player_start_buffer(wiimu_player_t *wiimu_player, const char *stream_id, const char *url,
                              int type, int is_next)
{
    cid_stream_t cid_stream;

    if (wiimu_player && wiimu_player->cid_player) {
        cid_stream.behavior = e_endqueue;
        cid_stream.cid_type = e_type_buffer_mp3;
        cid_stream.stream_type = type;
        cid_stream.token = (char *)stream_id;
        cid_stream.next_cached = is_next;
        cid_stream.uri = (char *)url + strlen("cid:");
        cid_stream.dialog_id = NULL;
        cid_player_add_item(wiimu_player->cid_player, cid_stream);
    }

    return 0;
}

/*
 * stop/finish play buffer
 * Input :
 *         @wiimu_player: the player wiimu_player
 *         @flag: 0 : finish play  1: stop play
 *
 * Output:
 *         0 sucess
 *         other failed
 */
int wiimu_player_end_buffer(wiimu_player_t *wiimu_player, const char *stream_id, int type)
{
    if (wiimu_player && wiimu_player->cid_player) {
        if (type == e_asr_stop) {
            cid_player_set_cmd(wiimu_player->cid_player, e_cmd_stop);
        } else if (type == e_asr_finished) {
            cid_player_cache_finished(wiimu_player->cid_player, (char *)stream_id);
        }
    }

    return 0;
}

int avs_player_init(void)
{
    if (g_wiimu_player == NULL) {
        g_wiimu_player = wiimu_player_init();
    }

    return 0;
}

int avs_player_uninit(void)
{
    if (g_wiimu_player) {
        wiimu_player_uninit(&g_wiimu_player);
        g_wiimu_player = NULL;
    }

    return 0;
}

static int alexa_get_play_type(char *token)
{
    int play_type = TYPE_PLAY_NONE;
    if (token) {
        if (StrInclude(token, ALEXA_PLAY_MUSIC)) {
            play_type = TYPE_PLAY_MUSIC;
        } else if (StrInclude(token, ALEXA_PLAY_AUDIBLEBOOK)) {
            play_type = TYPE_PLAY_AUDIBLEBOOK;
        } else if (StrInclude(token, ALEXA_PLAY_KINDLEBOOK)) {
            // set this to TYPE_PLAY_AUDIBLEBOOK, when press the talk button, send playstoped event
            // to avs like audiblebook
            play_type = TYPE_PLAY_KINDLEBOOK;
        } else if (StrInclude(token, ALEXA_PLAY_SPORTS)) {
            play_type = TYPE_PLAY_SPORTS;
        } else if (StrInclude(token, ALEXA_PLAY_DAILYBRIEFING)) {
            play_type = TYPE_PLAY_DAILYBRIEFING;
        } else if (StrInclude(token, ALEXA_PLAY_CINEMASHOWTIME)) {
            play_type = TYPE_PLAY_CINEMASHOWTIME;
        } else if (StrInclude(token, ALEXA_PLAY_TODO_BROWSE)) {
            play_type = TYPE_PLAY_TODO_LIST;
        } else if (StrInclude(token, ALEXA_PLAY_SHOPPING_LIST)) {
            play_type = TYPE_PLAY_SHOPPING_LIST;
        }
    }

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "------- what we play is %d-------\n", play_type);
    return play_type;
}

static int avs_player_get_play_behavior(char *play_behavior)
{
    int behavior = NONE;

    if (play_behavior) {
        if (StrEq(play_behavior, PLAYBEHAVIOR_REPLACE_ALL)) {
            behavior = REPLACE_ALL;
        } else if (StrEq(play_behavior, PLAYBEHAVIOR_REPLACE_ENQUEUED)) {
            behavior = REPLACE_ENQUEUED;
        } else if (StrEq(play_behavior, PLAYBEHAVIOR_ENQUEUE)) {
            behavior = ENQUEUE;
        }
    }

    return behavior;
}

/*
Play Directive
{
    "directive": {
        "header": {
            "namespace": "AudioPlayer",
            "name": "Play",
            "messageId": "{{STRING}}",
            "dialogRequestId": "{{STRING}}"
        },
        "payload": {
            "playBehavior": "{{STRING}}",
            "audioItem": {
                "audioItemId": "{{STRING}}",
                "stream": {
                    "url": "{{STRING}}",
                    "streamFormat" "AUDIO_MPEG"
                    "offsetInMilliseconds": {{LONG}},
                    "expiryTime": "{{STRING}}",
                    "progressReport": {
                        "progressReportDelayInMilliseconds": {{LONG}},
                        "progressReportIntervalInMilliseconds": {{LONG}}
                    },
                    "token": "{{STRING}}",
                    "expectedPreviousToken": "{{STRING}}"
                }
            }
        }
    }
}
*/
static int avs_player_parse_play(json_object *js_obj, int is_next)
{
    int play_type = 0;
    char *dialog_id = NULL;
    alexa_url_t new_url = {0};

    focus_mgmt_activity_status_set(AudioPlayer, FOCUS);
    focus_mgmt_activity_status_set(ExternalMediaPlayer, UNFOCUS);

    if (js_obj) {
        json_object *js_header = json_object_object_get(js_obj, KEY_HEADER);
        if (js_header) {
            dialog_id = JSON_GET_STRING_FROM_OBJECT(js_header, KEY_DIALOGREQUESTID);
            if (dialog_id) {
                new_url.is_dialog = 1;
            }
        }

        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            char *play_behavior = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_PLAYBEHAVIOR);
            if (play_behavior) {
                new_url.type = avs_player_get_play_behavior(play_behavior);
                ALEXA_PRINT(ALEXA_DEBUG_INFO, "play_behavior (%d)\n", new_url.type);
            }
            json_object *js_audio_item = json_object_object_get(js_payload, PAYLOAD_AUDIOITEM);
            if (js_audio_item) {
                new_url.stream_item =
                    JSON_GET_STRING_FROM_OBJECT(js_audio_item, PAYLOAD_AUDIOITEM_ID);
                json_object *js_audio_stream =
                    json_object_object_get(js_audio_item, PAYLOAD_AUDIOITEM_STREAM);
                if (js_audio_stream) {
                    JSON_GET_STRING_FROM_OBJECT(js_audio_stream, PAYLOAD_STREAM_FORMAT);
                    JSON_GET_STRING_FROM_OBJECT(js_audio_stream, PAYLOAD_STREAM_EXP_TIME);
                    new_url.expect_stream_id =
                        JSON_GET_STRING_FROM_OBJECT(js_audio_stream, PAYLOAD_STREAM_EXP_TOKEN);
                    new_url.stream_id =
                        JSON_GET_STRING_FROM_OBJECT(js_audio_stream, PAYLOAD_STREAM_TOKEN);
                    play_type = alexa_get_play_type(new_url.stream_id);
                    if (play_type == TYPE_PLAY_AUDIBLEBOOK) {
                        new_url.audible_book = 1;
                    } else {
                        new_url.audible_book = 0;
                    }
                    new_url.play_pos =
                        JSON_GET_LONG_FROM_OBJECT(js_audio_stream, PAYLOAD_STREAM_OFFSET_MS);
                    new_url.url = JSON_GET_STRING_FROM_OBJECT(js_audio_stream, PAYLOAD_STREAM_URL);
                    json_object *js_stream_progress =
                        json_object_object_get(js_audio_stream, PAYLOAD_STREAM_PROGRESS);
                    if (js_stream_progress) {
                        new_url.play_report_delay = JSON_GET_LONG_FROM_OBJECT(
                            js_stream_progress, PAYLOAD_PROGRESS_DELAY_MS);
                        // Fixed AVS issue AVSPARTNER -13834 : Extra ProgressReportDelayElapsed sent
                        if (new_url.play_report_delay + 100 < new_url.play_pos) {
                            new_url.play_report_delay = 0;
                        }
                        new_url.play_report_interval = JSON_GET_LONG_FROM_OBJECT(
                            js_stream_progress, PAYLOAD_PROGRESS_INTERVAL_MS);
                    }

                    if (new_url.url && new_url.stream_id && strlen(new_url.stream_id) > 0 &&
                        strlen(new_url.url) > 0) {
                        if (StrInclude(new_url.url, "http")) {
                            wiimu_player_play(g_wiimu_player, &new_url);
                        } else if (StrInclude(new_url.url, "cid:")) {
                            URL_DEBUG("------------ we need to play an tts buffer ------\n");
                            // TODO: need to write the buffer to tts player
                            if (new_url.type == REPLACE_ALL) {
                                wiimu_clear_play_list(g_wiimu_player);
                                g_wiimu_player->play_stoped = 1;
                                wiimu_player_stop(g_wiimu_player, 1);
                                URL_DEBUG(
                                    "------------ REPLACE_ALL so clear the play list ------\n");
                            }
                            wiimu_player_start_buffer(g_wiimu_player, new_url.stream_id,
                                                      new_url.url, e_stream_play, is_next);
                            pthread_mutex_lock(&g_wiimu_player->player_ctrl->ctrl_lock);
                            int imuzop_state = g_wiimu_player->player_ctrl->imuzop_state;
                            pthread_mutex_unlock(&g_wiimu_player->player_ctrl->ctrl_lock);
                            if (imuzop_state <= 0 || new_url.type == REPLACE_ALL) {
                                URL_DEBUG("++++++++ mplayer_state=%d ++++++++\n", imuzop_state);
                                // TODO: start play the next cache tts
                            }
                        }
                        pthread_mutex_lock(&g_wiimu_player->state_lock);
                        g_wiimu_player->cmd_paused = 0;
                        pthread_mutex_unlock(&g_wiimu_player->state_lock);
                    } else {
                        URL_DEBUG("parse the play directive error\n");
                    }
                }
            }
        }

        return 0;
    }

    return -1;
}

/*
Stop Directive
{
    "directive": {
        "header": {
            "namespace": "AudioPlayer",
            "name": "Stop",
            "messageId": "{{STRING}}",
            "dialogRequestId": "{{STRING}}"
        },
        "payload": {
        }
    }
}
*/
static int avs_player_parse_stop(json_object *js_obj)
{
    int ret = -1;
    int flag = 0;

    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            ret = wiimu_player_stop(g_wiimu_player, 0);
            if (ret != 0) {
                cid_player_set_cmd(g_wiimu_player->cid_player, e_cmd_stop);
                pthread_mutex_lock(&g_wiimu_player->state_lock);
                if (g_wiimu_player->cmd_paused == 0 && g_wiimu_player->play_token) {
                    alexa_audio_cmd(0, NAME_STOPPED, g_wiimu_player->play_token, 0, 0, 0);
                    g_wiimu_player->cmd_paused = 1;
                    g_wiimu_player->need_resume = 0;
                    g_wiimu_player->play_state = PLAY_STATE_STOPPED;
                    flag = 1;
                }
                pthread_mutex_unlock(&g_wiimu_player->state_lock);
                NOTIFY_CMD(ALEXA_CMD_FD, EVENT_CLEAR_NEXT);
                if (g_wiimu_player && g_wiimu_player->player_ctrl && flag == 0) {
                    pthread_mutex_lock(&g_wiimu_player->player_ctrl->ctrl_lock);
                    if (g_wiimu_player->cmd_paused == 0) {
                        if (!URL_INVALID(g_wiimu_player->player_ctrl->stream_url)) {
                            alexa_audio_cmd(0, NAME_STOPPED, g_wiimu_player->player_ctrl->stream_id,
                                            (long)g_wiimu_player->player_ctrl->position, 0, 0);
                        }
                    }
                    pthread_mutex_unlock(&g_wiimu_player->player_ctrl->ctrl_lock);
                }
            }
            pthread_mutex_lock(&g_wiimu_player->state_lock);
            g_wiimu_player->cmd_paused = 1;
            pthread_mutex_unlock(&g_wiimu_player->state_lock);
        }

        return 0;
    }

    return -1;
}

/*
ClearQueue Directive
{
    "directive": {
        "header": {
            "namespace": "AudioPlayer",
            "name": "ClearQueue",
            "messageId": "{{STRING}}",
            "dialogRequestId": "{{STRING}}"
        },
        "payload": {
            "clearBehavior": "{{STRING}}"
        }
    }
}
*/
static int avs_player_parse_clear_queue(json_object *js_obj)
{
    int ret = -1;

    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            char *clear_behavior = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_CLEARBEHAVIOR);
            if (clear_behavior) {
                URL_DEBUG("clear_behavior %s\n", clear_behavior);
                wiimu_clear_play_list(g_wiimu_player);
                if (StrEq(clear_behavior, PAYLOAD_CLEAR_ALL)) {
                    wiimu_player_stop(g_wiimu_player, 1);
                    pthread_mutex_lock(&g_wiimu_player->state_lock);
                    // if (g_wiimu_player->talk_continue == 0) {
                    //    cid_player_set_cmd(g_wiimu_player->cid_player, e_cmd_clear_queue);
                    //}
                    g_wiimu_player->cmd_paused = 0;
                    pthread_mutex_unlock(&g_wiimu_player->state_lock);
                }
                if (g_wiimu_shm->asr_pause_request == 1) {
                    g_wiimu_shm->asr_pause_request = 0;
                }
            }
            alexa_audio_cmd(0, NAME_QUEUECLEARED, NAME_QUEUECLEARED, 0, 0, 0);
            ret = 0;
        }
    }

    return ret;
}

/*
SetVolume Directive
{
    "directive": {
        "header": {
            "namespace": "Speaker",
            "name": "SetVolume",
            "messageId": "{{STRING}}",
            "dialogRequestId": "{{STRING}}"
        },
        "payload": {
            "volume": {{LONG}}
    }
}
*/
static int avs_player_parse_set_vol(json_object *js_obj)
{
    int ret = -1;
    int is_muted = 0;
    long volume_val = 0;

    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            volume_val = JSON_GET_LONG_FROM_OBJECT(js_payload, PAYLOAD_VOLUME);
            if (g_wiimu_player && g_wiimu_player->player_ctrl) {
                pthread_mutex_lock(&g_wiimu_player->player_ctrl->ctrl_lock);
                is_muted = g_wiimu_player->player_ctrl->muted;
                pthread_mutex_unlock(&g_wiimu_player->player_ctrl->ctrl_lock);
                ret = wiimu_player_set_vol(g_wiimu_player, 0, (int)volume_val);
                if (ret == 0) {
                    alexa_speeker_cmd(0, NAME_VOLUMECHANGED, (int)volume_val, is_muted);
                }
            }
        }
    }

    return ret;
}

/*
AdjustVolume Directive
{
    "directive": {
        "header": {
            "namespace": "Speaker",
            "name": "AdjustVolume",
            "messageId": "{{STRING}}",
            "dialogRequestId": "{{STRING}}"
        },
        "payload": {
            "volume": {{LONG}}
        }
    }
}*/
static int avs_player_parse_adjust_vol(json_object *js_obj)
{
    int ret = -1;
    int is_muted = 0;
    int volume = 0;
    long volume_val = 0;

    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            volume_val = JSON_GET_LONG_FROM_OBJECT(js_payload, PAYLOAD_VOLUME);
            if (g_wiimu_player && g_wiimu_player->player_ctrl) {
                pthread_mutex_lock(&g_wiimu_player->player_ctrl->ctrl_lock);
                is_muted = g_wiimu_player->player_ctrl->muted;
                volume = g_wiimu_player->player_ctrl->volume;
                pthread_mutex_unlock(&g_wiimu_player->player_ctrl->ctrl_lock);

#if defined(HARMAN)
                int hwVol = 0;
#if defined(HARMAN_ALLURE_PORTABLE)
                unsigned char volume_set_dsp[11] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
#elif defined(HARMAN_ALLURE_ASTRA)
                unsigned char volume_set_dsp[11] = {0, 1, 10, 20, 32, 42, 52, 65, 72, 82, 100};
#else
                unsigned char volume_set_dsp[11] = {0, 1, 10, 20, 30, 40, 50, 60, 70, 82, 100};
#endif
                for (hwVol = 0; hwVol < 11; hwVol++) {
                    if (volume_set_dsp[hwVol] >= volume_val) {
                        if (volume_val > volume)
                            volume_val = hwVol * 10;
                        break;
                    }
                }
#endif
                volume_val += volume;

                if (volume_val >= 100) {
                    volume_val = 100;
                }
                if (volume_val <= 0) {
                    volume_val = 0;
                }
                ret = wiimu_player_set_vol(g_wiimu_player, 0, (int)volume_val);
                if (ret == 0) {
                    alexa_speeker_cmd(0, NAME_VOLUMECHANGED, (int)volume_val, is_muted);
                }
            }
        }

        return ret;
    }

    return ret;
}

/*
SetMute Directive
{
    "directive": {
        "header": {
            "namespace": "Speaker",
            "name": "SetMute",
            "messageId": "{{STRING}}",
            "dialogRequestId": "{{STRING}}"
        },
        "payload": {
            "mute": {{BOOLEAN}}
        }
    }
}
*/
static int avs_player_parse_set_mute(json_object *js_obj)
{
    int ret = -1;
    int is_muted = 0;
    int volume = 0;
    char buff[128];

    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            json_bool muted = JSON_GET_BOOL_FROM_OBJECT(js_payload, PAYLOAD_MUTE);
            is_muted = (muted ? 1 : 0);
            if (g_wiimu_player && g_wiimu_player->player_ctrl) {
                pthread_mutex_lock(&g_wiimu_player->player_ctrl->ctrl_lock);
                volume = g_wiimu_player->player_ctrl->volume;
                pthread_mutex_unlock(&g_wiimu_player->player_ctrl->ctrl_lock);
                ret = wiimu_player_set_vol(g_wiimu_player, 1, (int)is_muted);
                if (ret == 0) {
                    if (is_muted == 0 && volume < VOLUME_VALUE_OF_INIT) {
                        // set value as TAP did
                        volume = VOLUME_VALUE_OF_INIT;
                        snprintf(buff, sizeof(buff), "setPlayerCmd:alexa:vol:%d", volume);
                        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "%s\n", buff);
                        SocketClientReadWriteMsg("/tmp/Requesta01controller", buff, strlen(buff), 0,
                                                 0, 0);
                        pthread_mutex_lock(&g_wiimu_player->player_ctrl->ctrl_lock);
                        g_wiimu_player->player_ctrl->volume = volume;
                        g_wiimu_player->player_ctrl->muted = g_wiimu_shm->mute;
                        pthread_mutex_unlock(&g_wiimu_player->player_ctrl->ctrl_lock);
                    }
                    alexa_speeker_cmd(0, NAME_MUTECHANGED, (int)volume, is_muted);
                }
            }
        }

        return ret;
    }

    return ret;
}

/*
Speak Directive
{
    "directive": {
        "header": {
            "namespace": "SpeechSynthesizer",
            "name": "Speak",
            "messageId": "{{STRING}}",
            "dialogRequestId": "{{STRING}}"
        },
        "payload": {
            "url": "{{STRING}}",
            "format": "{{STRING}}",
            "token": "{{STRING}}"
        }
    }
}
*/
static int avs_player_parse_speak(json_object *js_obj)
{
    int ret = -1;

    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_URL);
            JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_FORMAT);
            char *caption = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_CAPTION);
            char *token = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_TOKEN);
            char *url = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_URL);
            if (token && url) {
                wiimu_player_start_buffer(g_wiimu_player, token, url, e_stream_speak, 0);
                pthread_mutex_lock(&g_wiimu_player->state_lock);
                g_wiimu_player->speak_state = TTS_STATE_INIT;
                pthread_mutex_unlock(&g_wiimu_player->state_lock);
            }
            if (caption) {
                char *buf = NULL;
                asprintf(&buf, "nextinvalid:%s", caption);
                if (buf) {
                    SocketClientReadWriteMsg("/tmp/Requesta01controller", buf, strlen(buf), 0, 0,
                                             0);
                    os_free(buf);
                }
            }
            ret = 0;
        }
    }

    return ret;
}

/*
SpeechRecognizer Directive:
ExpectSpeech Directive
{
  "directive": {
        "header": {
            "namespace": "SpeechRecognizer",
            "name": "ExpectSpeech",
            "messageId": "{{STRING}}",
            "dialogRequestId": "{{STRING}}"
        },
        "payload": {
            "timeoutInMilliseconds": {{LONG}}
        }
    }
}
*/
static int avs_player_parse_expect_speech(json_object *js_obj)
{
    int is_voice_recording = 0;

    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            // JSON_GET_LONG_FROM_OBJECT(js_payload, PAYLOAD_TIMEOUT_MS);
            json_object *js_initiator = json_object_object_get(js_payload, PAYLOAD_initiator);
            if (js_initiator) {
                alexa_set_initiator(js_initiator);
                char *type = JSON_GET_STRING_FROM_OBJECT(js_initiator, PAYLOAD_TYPE);
                if (StrEq(type, Val_VOICE_RECORDING)) {
                    is_voice_recording = 1;
                }
            }
        }
        if (is_voice_recording) {
            ASR_SetRecording_TimeOut(40000);
        } else {
            ASR_SetRecording_TimeOut(8000);
        }
        pthread_mutex_lock(&g_wiimu_player->state_lock);
        g_wiimu_player->talk_continue = 1;
        g_wiimu_player->need_resume = 0;
        pthread_mutex_unlock(&g_wiimu_player->state_lock);

        return 0;
    }

    return -1;
}

// parse directive
int avs_player_parse_directvie(json_object *js_data, int is_next)
{
    int ret = -1;
    char *name = NULL;
    char *name_space = NULL;

    if (js_data) {
        json_object *js_header = json_object_object_get(js_data, KEY_HEADER);
        if (js_header) {
            name_space = JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            name = JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_DIALOGREQUESTID);
        }

        if (StrEq(name_space, NAMESPACE_AUDIOPLAYER)) {
            if (StrEq(name, NAME_PLAY)) {
                ret = avs_player_parse_play(js_data, is_next);
            } else if (StrEq(name, NAME_STOP)) {
                ret = avs_player_parse_stop(js_data);
            } else if (StrEq(name, NAME_CLEARQUEUE)) {
                ret = avs_player_parse_clear_queue(js_data);
            }
        } else if (StrEq(name_space, NAMESPACE_SPEECHSYNTHESIZER)) {
            if (StrEq(name, NAME_SPEAK)) {
                ret = avs_player_parse_speak(js_data);
            }
        } else if (StrEq(name_space, NAMESPACE_SPEAKER)) {
            if (StrEq(name, NAME_SETVOLUME)) {
                ret = avs_player_parse_set_vol(js_data);
            } else if (StrEq(name, NAME_ADJUSTVOLUME)) {
                ret = avs_player_parse_adjust_vol(js_data);
            } else if (StrEq(name, NAME_SETMUTE)) {
                ret = avs_player_parse_set_mute(js_data);
            }
        } else if (StrEq(name_space, NAMESPACE_SPEECHRECOGNIZER)) {
            if (StrEq(name, NAME_EXPECTSPEECH)) {
                avs_player_parse_expect_speech(js_data);
            }
        }
    }

    return ret;
}

char *avs_player_state(int request_type)
{
    char *avs_state = NULL;
    char *audio_token = NULL;
    char *audio_url = NULL;
    char *speech_token = NULL;
    char *speech_state = PLAYER_FINISHED;
    char *play_state = PLAYER_IDLE;
    long play_offset_ms = 0;
    int volume = 0;
    int muted = 0;

    if (g_wiimu_player) {
        // get the TTS's play status
        if (g_wiimu_player->cid_player) {
            pthread_mutex_lock(&g_wiimu_player->state_lock);
            if (g_wiimu_player->speak_token) {
                speech_token = os_strdup(g_wiimu_player->speak_token);
                if (g_wiimu_player->speak_state == TTS_STATE_PLAYING) {
                    speech_state = PLAYER_PLAYING;
                } else {
                    speech_state = PLAYER_FINISHED;
                    // TODO: Not a good way to fixed this, make sure with amazon server, why we set
                    // it to finished
                    // TODO: the server didn't recognize the "Stop" command when "alexa, send
                    // meesage"
                    if (StrInclude(g_wiimu_player->speak_token, ALEXA_PLAY_MESSAGE) &&
                        request_type == 2) {
                        speech_state = PLAYER_PLAYING;
                    }
                }
            }
            if (g_wiimu_player->play_token) {
                audio_token = os_strdup(g_wiimu_player->play_token);
                if (g_wiimu_player->play_state == PLAY_STATE_PLAYING) {
                    play_state = PLAYER_PLAYING;
                } else if (g_wiimu_player->play_state == PLAY_STATE_STOPPED) {
                    play_state = PLAYER_STOPPED;
                } else if (g_wiimu_player->play_state == PLAY_STATE_FINISHED) {
                    play_state = PLAYER_FINISHED;
                } else {
                    play_state = PLAYER_FINISHED;
                }
            }
            pthread_mutex_unlock(&g_wiimu_player->state_lock);
        }
        // get the url's play status
        if (g_wiimu_player->player_ctrl) {
            wiimu_player_ctrl_get_volume(g_wiimu_player->player_ctrl, &volume, &muted);
            if (!audio_token) {
                pthread_mutex_lock(&g_wiimu_player->player_ctrl->ctrl_lock);
                audio_token = os_strdup(g_wiimu_player->player_ctrl->stream_id);
                audio_url = os_strdup(g_wiimu_player->player_ctrl->stream_url);
                switch (g_wiimu_player->player_ctrl->play_state) {
                case player_stopped:
                case player_stopping:
                    play_state = PLAYER_STOPPED;
                    break;
                case player_pausing:
                    play_state = PLAYER_STOPPED;
                    break;
                case player_loading:
                case player_playing:
                    play_state = PLAYER_PLAYING;
                    break;
                case player_init:
                default:
                    if (audio_token) {
                        play_state = PLAYER_STOPPED;
                    } else {
                        play_state = PLAYER_IDLE;
                    }
                    break;
                }
                play_offset_ms = (long)g_wiimu_player->player_ctrl->position;
                pthread_mutex_unlock(&g_wiimu_player->player_ctrl->ctrl_lock);
            }
        }

        // TODO: workaround the pandora invalid token/host urls, don't send some event to avs server
        if ((!audio_token) ||
            ((StrSubStr(audio_url, IVALIAD_TOKEN) || StrSubStr(audio_url, BAD_HOST)) &&
             audio_url)) {
            os_free(audio_token);
            audio_token = NULL;
            play_state = (char *)PLAYER_IDLE;
            play_offset_ms = 0;
        }
    }

    if (play_offset_ms < 0) {
        play_offset_ms = 0;
    }

    create_avs_state(&avs_state, request_type, Val_Str(audio_token), play_offset_ms,
                     Val_Str(play_state), volume, muted, Val_Str(speech_token), 0,
                     Val_Str(speech_state));

    os_free(audio_token);
    os_free(audio_url);
    os_free(speech_token);

    return avs_state;
}

int avs_player_force_stop(int flags)
{
    if (g_wiimu_player) {
        if (g_wiimu_shm->asr_pause_request == 1) {
            g_wiimu_shm->asr_pause_request = 0;
        }
        if (g_wiimu_player->cid_player) {
            if (flags) {
                cid_player_set_cmd(g_wiimu_player->cid_player, e_cmd_clear_queue);
            } else {
                cid_player_set_cmd(g_wiimu_player->cid_player, e_cmd_source_change);
            }
        }
        wiimu_clear_play_list(g_wiimu_player);
        wiimu_player_ctrl_stop(g_wiimu_player->player_ctrl, 0);
        if (g_wiimu_player && g_wiimu_player->player_ctrl &&
            g_wiimu_shm->play_status != player_pausing) {
            pthread_mutex_lock(&g_wiimu_player->player_ctrl->ctrl_lock);
            if (g_wiimu_player->cmd_paused == 0) {
                g_wiimu_player->cmd_paused = 1;
                if (!URL_INVALID(g_wiimu_player->player_ctrl->stream_url)) {
                    alexa_audio_cmd(0, NAME_STOPPED, g_wiimu_player->player_ctrl->stream_id,
                                    (long)g_wiimu_player->player_ctrl->position, 0, 0);
                }
            }
            if (g_wiimu_player->player_ctrl->stream_id && flags) {
                free(g_wiimu_player->player_ctrl->stream_id);
                g_wiimu_player->player_ctrl->stream_id = NULL;
                URL_DEBUG("clear the playback token\n");
            }
            pthread_mutex_unlock(&g_wiimu_player->player_ctrl->ctrl_lock);
        }
        NOTIFY_CMD(ALEXA_CMD_FD, EVENT_CLEAR_NEXT);
    }

    return 0;
}

// e_asr_stop: stop current tts
// e_asr_finished: tts cache finished
int avs_player_buffer_stop(char *contend_id, int type)
{
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
            if (contend_id) {
                cid_player_cache_finished(g_wiimu_player->cid_player, contend_id);
            } else {
                pthread_mutex_lock(&g_wiimu_player->state_lock);
                cid_player_cache_finished(g_wiimu_player->cid_player, g_wiimu_player->speak_token);
                pthread_mutex_unlock(&g_wiimu_player->state_lock);
            }
        }
    }

    return 0;
}

int avs_player_buffer_resume(void)
{
    int ret = -1;

    if (g_wiimu_player && g_wiimu_player->cid_player) {
        pthread_mutex_lock(&g_wiimu_player->state_lock);
        if (g_wiimu_player->need_resume == 1 && g_wiimu_player->cmd_paused == 0) {
            if (g_wiimu_player->play_token) {
                alexa_audio_cmd(0, NAME_PLAYNEARLYFIN, g_wiimu_player->play_token, 0, 0, 0);
                free(g_wiimu_player->play_token);
                g_wiimu_player->play_token = NULL;
                ret = 0;
                URL_DEBUG("----- avs_player_buffer_resume -----\n");
            }
        }
        g_wiimu_player->need_resume = 0;
        pthread_mutex_unlock(&g_wiimu_player->state_lock);

#ifdef EN_AVS_BLE
        if (g_wiimu_player->speak_state != PLAY_STATE_PLAYING) {
            avs_bluetooth_do_cmd(0);
        }
#endif
    }

    return ret;
}

int avs_player_buffer_write_data(char *content_id, char *data, int len)
{
    size_t size = 0;

    if (g_wiimu_player && g_wiimu_player->cid_player) {
        size = cid_player_write_data(g_wiimu_player->cid_player, content_id, data, len);
    }

    return size;
}

int avs_player_get_state(int type)
{
    int ret = 0;

    if (g_wiimu_player) {
        if (type == e_url_player) {
            if (g_wiimu_player->player_ctrl) {
                wiimu_player_ctrl_update_info(g_wiimu_player->player_ctrl);
                pthread_mutex_lock(&g_wiimu_player->player_ctrl->ctrl_lock);
                if (g_wiimu_player->player_ctrl->play_state >= player_loading) {
                    ret = 1;
                }
                pthread_mutex_unlock(&g_wiimu_player->player_ctrl->ctrl_lock);
            }
        } else if (type == e_tts_player) {
            /*modify by weiqiang.huang for CER-24 2017-0713 -- begin*/
            pthread_mutex_lock(&g_wiimu_player->state_lock);
            if (g_wiimu_player->speak_state != TTS_STATE_IDLE ||
                g_wiimu_player->play_state == PLAY_STATE_PLAYING) {
                ret = 1;
            }
            pthread_mutex_unlock(&g_wiimu_player->state_lock);
            /*modify by weiqiang.huang for CER-24 2017-0713 -- end*/

        } else if (type == e_asr_status) {
            pthread_mutex_lock(&g_wiimu_player->state_lock);
            ret = g_wiimu_player->talk_continue;
            pthread_mutex_unlock(&g_wiimu_player->state_lock);
        }
    }

    return ret;
}

int avs_talkcontinue_get_status()
{
    pthread_mutex_lock(&g_wiimu_player->state_lock);
    if (g_wiimu_player->speak_state == TTS_STATE_IDLE &&
        g_wiimu_player->play_state != PLAY_STATE_PLAYING) {
        if (g_wiimu_player->talk_continue == 1) {
            g_wiimu_player->talk_continue = 0;
            pthread_mutex_unlock(&g_wiimu_player->state_lock);
            return 1;
        }
    }
    pthread_mutex_unlock(&g_wiimu_player->state_lock);
    return 0;
}

int avs_player_need_unmute(int mini_vol)
{
    int is_muted = 0;
    int volume = 0;
    char buff[128] = {0};

    if (g_wiimu_player && g_wiimu_player->player_ctrl) {
        pthread_mutex_lock(&g_wiimu_player->player_ctrl->ctrl_lock);
        is_muted = g_wiimu_player->player_ctrl->muted;
        volume = g_wiimu_player->player_ctrl->volume;
        if (volume != g_wiimu_shm->volume) {
            volume = g_wiimu_shm->volume;
            g_wiimu_player->player_ctrl->volume = volume;
        }
        if (is_muted != g_wiimu_shm->mute) {
            is_muted = g_wiimu_shm->mute;
            g_wiimu_player->player_ctrl->muted = g_wiimu_shm->mute;
        }
        pthread_mutex_unlock(&g_wiimu_player->player_ctrl->ctrl_lock);

        // if mute ,switch to unmute
        if (is_muted) {
            snprintf(buff, sizeof(buff), "setPlayerCmd:alexa:mute:%d", 0);
            SocketClientReadWriteMsg("/tmp/Requesta01controller", buff, strlen(buff), 0, 0, 0);
        }
        if (mini_vol > volume || is_muted) {
            volume = mini_vol;
            snprintf(buff, sizeof(buff), "setPlayerCmd:alexa:vol:%d", volume);
            ALEXA_PRINT(ALEXA_DEBUG_TRACE, "%s\n", buff);
            SocketClientReadWriteMsg("/tmp/Requesta01controller", buff, strlen(buff), 0, 0, 0);
            alexa_speeker_cmd(0, NAME_VOLUMECHANGED, volume, 0);
        }
        pthread_mutex_lock(&g_wiimu_player->player_ctrl->ctrl_lock);
        g_wiimu_player->player_ctrl->volume = volume;
        g_wiimu_player->player_ctrl->muted = g_wiimu_shm->mute;
        pthread_mutex_unlock(&g_wiimu_player->player_ctrl->ctrl_lock);
    }

    return 0;
}

void avs_player_book_ctrol(int pause_flag)
{
// This will cause mplayer cannot receive pause/resume command
#if 0
    int ret = 0;
    int play_type = 0;

    if(g_wiimu_player && g_wiimu_player->player_ctrl && PLAYER_MODE_IS_ALEXA(g_wiimu_shm->play_mode)) {
        pthread_mutex_lock(&g_wiimu_player->player_ctrl->ctrl_lock);
        play_type = alexa_get_play_type(g_wiimu_player->player_ctrl->stream_id);
        pthread_mutex_unlock(&g_wiimu_player->player_ctrl->ctrl_lock);
        if(IS_PLAY_BOOK(play_type)) {
            URL_DEBUG("----- pause_flag = %d -----\n", pause_flag);
            if(pause_flag) {
                g_wiimu_shm->asr_pause_request = 1;
            } else {
                g_wiimu_shm->asr_pause_request = 0;
            }
        }
    }
#endif
}

void avs_player_ctrol(int pause_flag)
{
    if (g_wiimu_player && g_wiimu_player->player_ctrl &&
        PLAYER_MODE_IS_ALEXA(g_wiimu_shm->play_mode)) {
        if (pause_flag) {
            pthread_mutex_lock(&g_wiimu_player->player_ctrl->ctrl_lock);
            if (g_wiimu_player->cmd_paused == 0) {
                alexa_audio_cmd(0, NAME_STOPPED, g_wiimu_player->player_ctrl->stream_id,
                                (long)g_wiimu_player->player_ctrl->position, 0, 0);
                g_wiimu_player->cmd_paused = 1;
            }
            pthread_mutex_unlock(&g_wiimu_player->player_ctrl->ctrl_lock);
        } else {
            pthread_mutex_lock(&g_wiimu_player->player_ctrl->ctrl_lock);
            alexa_audio_cmd(0, NAME_PLAYSTARTED, g_wiimu_player->player_ctrl->stream_id,
                            (long)g_wiimu_player->player_ctrl->position, 0, 0);
            g_wiimu_player->cmd_paused = 0;
            pthread_mutex_unlock(&g_wiimu_player->player_ctrl->ctrl_lock);
        }
    }
}

int avs_player_audio_invalid(char *stream_id)
{
    int ret = 0;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    play_list_item_t *list_item = NULL;

    if (g_wiimu_player && stream_id) {
        pthread_mutex_lock(&g_wiimu_player->play_list_lock);
        if (!lp_list_empty(&g_wiimu_player->play_list)) {
            lp_list_for_each_safe(pos, npos, &g_wiimu_player->play_list)
            {
                list_item = lp_list_entry(pos, play_list_item_t, list);
                if (list_item && list_item->url.stream_id) {
                    if (StrEq(list_item->url.stream_id, stream_id)) {
                        ret = list_item->url.url_invalid;
                        break;
                    }
                }
            }
        }
        pthread_mutex_unlock(&g_wiimu_player->play_list_lock);
    }

    return ret;
}

void avs_player_set_provider(char *provider)
{
    pthread_mutex_lock(&g_wiimu_player->player_ctrl->ctrl_lock);
    if (StrCaseEq(provider, "Pandora"))
        g_wiimu_player->player_ctrl->provider = PROVIDER_PANDORA;
    else if (StrCaseEq(provider, "iHeartRadio Live Radio"))
        g_wiimu_player->player_ctrl->provider = PROVIDER_IHEARTRADIO;
    else
        g_wiimu_player->player_ctrl->provider = 0;
    pthread_mutex_unlock(&g_wiimu_player->player_ctrl->ctrl_lock);
}
