/*************************************************************************
    > File Name: url_player.c
    > Author:
    > Mail:
    > Created Time: June 14 Nov 2018 15:20:49 AM EST
 ************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "wm_util.h"
#include "lp_list.h"

#include "url_player_naver.h"
#include "wiimu_player_ctrol.h"
#include "common_flags.h"

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
typedef struct _stream_item_s {
    int need_dump;
    int get_url;
    int started;
    int stopped;
    int paused;
    int finished;
    int failed;
    url_stream_t url_stream;
} stream_item_t;

typedef struct _stream_list_item_s {
    int index;
    stream_item_t stream_item;
    struct lp_list_head list;
} stream_list_item_t;

struct _url_player_s {
    int is_running;
    int play_stoped;
    int notify2app;
    int clear;
    unsigned long long play_req_id;

    pthread_t play_pid;
    int total_num;
    int next_index;
    pthread_mutex_t play_list_lock;
    struct lp_list_head play_list;
    player_ctrl_t *player_ctrl;
    url_notify_cb_t *notify_cb;
    void *ctx;
    WIIMU_CONTEXT *wiimu_shm;
};

static void url_player_clear_play_list(url_player_t *self)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    stream_list_item_t *list_item = NULL;

    if (self) {
        pthread_mutex_lock(&self->play_list_lock);
        if (!lp_list_empty(&self->play_list)) {
            lp_list_for_each_safe(pos, npos, &self->play_list)
            {
                list_item = lp_list_entry(pos, stream_list_item_t, list);
                if (list_item) {
                    lp_list_del(&list_item->list);
                    os_free(list_item->stream_item.url_stream.url);
                    os_free(list_item->stream_item.url_stream.stream_id);
                    os_free(list_item->stream_item.url_stream.stream_item);
                    os_free(list_item->stream_item.url_stream.stream_data);
                    os_free(list_item);
                }
            }
            URL_DEBUG("url play list number is %d\n", lp_list_number(&self->play_list));
        }
        self->total_num = 0;
        self->next_index = 0;
        pthread_mutex_unlock(&self->play_list_lock);
    }
}

static int url_player_notify(url_player_t *self, e_url_notify_t notify, url_stream_t *url_stream)
{
    int ret = -1;

    if (self && self->notify_cb && url_stream) {
        ret = self->notify_cb(notify, url_stream, self->ctx);
    }

    return ret;
}

static stream_list_item_t *url_player_list_item_new(url_stream_t *url_item)
{
    stream_list_item_t *list_item = NULL;

    if (url_item) {
        list_item = os_calloc(1, sizeof(stream_list_item_t));
        if (list_item) {
            memset(list_item, 0, sizeof(stream_list_item_t));
            list_item->stream_item.url_stream.ignore_ongoing_speak = url_item->ignore_ongoing_speak;
            list_item->stream_item.url_stream.behavior = url_item->behavior;
            list_item->stream_item.url_stream.stream_id = os_strdup(url_item->stream_id);
            list_item->stream_item.url_stream.stream_item = os_strdup(url_item->stream_item);
            list_item->stream_item.url_stream.url = os_strdup(url_item->url);
            list_item->stream_item.need_dump = url_item->url ? 0 : 1;
            list_item->stream_item.url_stream.new_dialog = url_item->new_dialog;
            list_item->stream_item.url_stream.play_pos = url_item->play_pos;
            list_item->stream_item.url_stream.play_report_delay = url_item->play_report_delay;
            list_item->stream_item.url_stream.play_report_interval = url_item->play_report_interval;
            list_item->stream_item.url_stream.play_report_position = url_item->play_report_position;
            list_item->stream_item.url_stream.stream_data = os_strdup(url_item->stream_data);
            list_item->stream_item.url_stream.duration = url_item->duration;
            list_item->stream_item.url_stream.cur_pos = url_item->play_pos;
        }
    }

    return list_item;
}

static void url_player_list_item_free(stream_list_item_t *list_item)
{
    if (list_item) {
        os_free(list_item->stream_item.url_stream.url);
        os_free(list_item->stream_item.url_stream.stream_id);
        os_free(list_item->stream_item.url_stream.stream_item);
        os_free(list_item->stream_item.url_stream.stream_data);
        os_free(list_item);
    }
}

void *url_player_proc(void *arg)
{
    url_player_t *self = (url_player_t *)arg;

    long long cur_time = 0;

    long long interval_time = 0;
    long long delay_time = 0;
    long long position_time = 0;

    long long interval_ms = 0;
    long long delay_ms = 0;
    long long position_ms = 0;

    int index_interval = 0;
    long long next_interval = 0;

    int genie_online = -1;
    char *genie_url = NULL;

    URL_DEBUG("--------wiimu_player_proc--------\n");
    while (self && self->is_running) {
        struct lp_list_head *pos = NULL;
        struct lp_list_head *npos = NULL;
        stream_list_item_t *list_item = NULL;

        pthread_mutex_lock(&self->play_list_lock);
        if (!lp_list_empty(&self->play_list)) {
            lp_list_for_each_safe(pos, npos, &self->play_list)
            {
                list_item = lp_list_entry(pos, stream_list_item_t, list);
                if (list_item && list_item->stream_item.url_stream.url) {
                    if (self->wiimu_shm && self->wiimu_shm->alexa_response_status == 1 &&
                        !list_item->stream_item.url_stream.ignore_ongoing_speak) {
                        list_item = NULL;
                        break;
                    }

                    if (list_item->stream_item.need_dump == 0) {
                        if (list_item->index == self->next_index) {
                            self->next_index++;
                            list_item =
                                url_player_list_item_new(&list_item->stream_item.url_stream);
                            break;
                        } else {
                            list_item = NULL;
                        }
                    } else {
                        lp_list_del(&list_item->list);
                        break;
                    }
                } else if (list_item && list_item->stream_item.url_stream.url == NULL &&
                           list_item->stream_item.get_url == 0) {
                    if (self->wiimu_shm && self->wiimu_shm->alexa_response_status == 0) {
                        url_player_notify(self, e_notify_get_url,
                                          &list_item->stream_item.url_stream);
                        list_item->stream_item.get_url = 1;
                    }
                    list_item = NULL;
                } else {
                    list_item = NULL;
                }
            }
        }
        pthread_mutex_unlock(&self->play_list_lock);

        self->play_stoped = 0;
        if (list_item && list_item->stream_item.url_stream.url &&
            list_item->stream_item.url_stream.stream_id) {
            URL_DEBUG("--------start play index/total=%d/%d new_dialog=%d--------\n",
                      list_item->index, self->total_num,
                      list_item->stream_item.url_stream.new_dialog);
            list_item->stream_item.started = 0;
            list_item->stream_item.stopped = 0;
            list_item->stream_item.paused = 0;
            list_item->stream_item.finished = 0;
            list_item->stream_item.failed = 0;

            wiimu_player_ctrl_set_uri(self->player_ctrl,
                                      list_item->stream_item.url_stream.stream_id,
                                      list_item->stream_item.url_stream.stream_item,
                                      list_item->stream_item.url_stream.url,
                                      list_item->stream_item.url_stream.stream_data,
                                      list_item->stream_item.url_stream.play_pos);
            while (self->is_running) {
                cur_time = os_power_on_tick();
                int ret = wiimu_player_ctrl_update_info(self->player_ctrl);
                if (self->play_stoped == 1) {
                    self->play_stoped = 0;
                    URL_DEBUG("--------1 play stoped 1--------\n");
                    if (list_item->stream_item.started == 1 && list_item->stream_item.failed == 0 &&
                        list_item->stream_item.finished == 0 &&
                        list_item->stream_item.stopped == 0) {
                        list_item->stream_item.stopped = 1;
                        url_player_notify(self, e_notify_play_stopped,
                                          &list_item->stream_item.url_stream);
                    }
                    break;
                }

                if ((self->player_ctrl->error_code != 0 || ret == 1)) {
                    URL_DEBUG("--------error_code(%d) ret(%d) state(%d) pos(%lld)--------\n",
                              self->player_ctrl->error_code, ret, self->player_ctrl->play_state,
                              list_item->stream_item.url_stream.cur_pos);
                    if (list_item->stream_item.failed == 0 &&
                        (self->player_ctrl->error_code > 0 ||
                         list_item->stream_item.url_stream.cur_pos <=
                             list_item->stream_item.url_stream.play_pos + 10)) {
                        // TODO: send a playback error event to avs
                        list_item->stream_item.failed = 1;
                        URL_DEBUG(
                            "--------play failed-------- (%d %d) (%lld %ld) (%d %d) (%d %d)\n",
                            self->player_ctrl->error_code, self->player_ctrl->play_state,
                            list_item->stream_item.url_stream.cur_pos,
                            list_item->stream_item.url_stream.play_pos,
                            self->player_ctrl->mplayer_shmem->err_count,
                            self->player_ctrl->mplayer_shmem->http_error_code,
                            self->player_ctrl->mplayer_shmem->status, self->wiimu_shm->play_status);
                        url_player_notify(self, e_notify_play_error,
                                          &list_item->stream_item.url_stream);
                    } else if (list_item->stream_item.finished == 0) {
                        // TODO: send a playback finished event to avs
                        list_item->stream_item.finished = 1;
                        URL_DEBUG("--------play finished-------- (%d %d %lld) (%d %d) (%d %d)\n",
                                  self->player_ctrl->error_code, self->player_ctrl->play_state,
                                  list_item->stream_item.url_stream.cur_pos,
                                  self->player_ctrl->mplayer_shmem->err_count,
                                  self->player_ctrl->mplayer_shmem->http_error_code,
                                  self->player_ctrl->mplayer_shmem->status,
                                  self->wiimu_shm->play_status);
                        if (get_device_status() == DEV_REBOOTING)
                            url_player_notify(self, e_notify_play_stopped,
                                              &list_item->stream_item.url_stream);
                        else
                            url_player_notify(self, e_notify_play_finished,
                                              &list_item->stream_item.url_stream);
                    }
                    break;
                }

                if (self->player_ctrl->play_state == player_playing &&
                    list_item->stream_item.started == 0 && list_item->stream_item.failed == 0 &&
                    list_item->stream_item.finished == 0) {
                    list_item->stream_item.started = 1;
                    interval_time = position_time = delay_time = os_power_on_tick();
                    index_interval = 1;
                    delay_ms = list_item->stream_item.url_stream.play_report_delay;
                    position_ms = list_item->stream_item.url_stream.play_report_position;
                    interval_ms = list_item->stream_item.url_stream.play_report_interval;
                    // TODO: send a play started event to avs server
                    URL_DEBUG("--------play started-------- (%d %d %lld) (%d %d) (%d %d)\n",
                              self->player_ctrl->error_code, self->player_ctrl->play_state,
                              list_item->stream_item.url_stream.cur_pos,
                              self->player_ctrl->mplayer_shmem->err_count,
                              self->player_ctrl->mplayer_shmem->http_error_code,
                              self->player_ctrl->mplayer_shmem->status,
                              self->wiimu_shm->play_status);
                    if (self->wiimu_shm &&
                        list_item->stream_item.url_stream.behavior == e_replace_all) {
                        focus_manage_set(self->wiimu_shm, e_focus_content);
                    }
                    url_player_notify(self, e_notify_play_started,
                                      &list_item->stream_item.url_stream);

                } else if (self->player_ctrl->play_state == player_playing) {

                    if (list_item->stream_item.started == 1 && list_item->stream_item.failed == 0 &&
                        list_item->stream_item.finished == 0 &&
                        list_item->stream_item.paused == 1) {
                        list_item->stream_item.paused = 0;
                        url_player_notify(self, e_notify_play_resumed,
                                          &list_item->stream_item.url_stream);
                    }

                    // sync the play position
                    list_item->stream_item.url_stream.cur_pos = self->player_ctrl->position;
                    if (0 < delay_ms && 0 < delay_time) {
                        cur_time = os_power_on_tick();
                        if (cur_time >= (delay_ms + delay_time)) {
                            if (list_item->stream_item.url_stream.cur_pos >=
                                (delay_ms + list_item->stream_item.url_stream.play_pos)) {
                                delay_time = 0;
                                url_player_notify(self, e_notify_play_delay_passed,
                                                  &list_item->stream_item.url_stream);
                            }
                        }
                    }

                    if (0 < position_ms && 0 < position_time) {
                        cur_time = os_power_on_tick();
                        if (cur_time >= (position_ms + position_time)) {
                            if (list_item->stream_item.url_stream.cur_pos >=
                                (position_ms + list_item->stream_item.url_stream.play_pos)) {
                                position_time = 0;
                                url_player_notify(self, e_notify_play_position_passed,
                                                  &list_item->stream_item.url_stream);
                            }
                        }
                    }

                    if (0 < interval_ms) {
                        cur_time = os_power_on_tick();
                        if (cur_time >= (interval_ms + interval_time)) {
                            next_interval = (index_interval * interval_ms +
                                             list_item->stream_item.url_stream.play_pos);
                            if (list_item->stream_item.url_stream.cur_pos >= next_interval) {
                                interval_time += interval_ms;
                                index_interval++;
                                url_player_notify(self, e_notify_play_interval_passed,
                                                  &list_item->stream_item.url_stream);
                            }
                        }
                    }
                } else if (self->player_ctrl->play_state == player_pausing) {
                    if (list_item->stream_item.started == 1 && list_item->stream_item.failed == 0 &&
                        list_item->stream_item.finished == 0 &&
                        list_item->stream_item.paused == 0 && list_item->stream_item.stopped == 0) {
                        list_item->stream_item.paused = 1;
                        url_player_notify(self, e_notify_play_paused,
                                          &list_item->stream_item.url_stream);
                    }
                    usleep(10000);
                    continue;
                } else if (self->player_ctrl->play_state == player_loading) {
                    usleep(10000);
                    continue;
                } else if (self->player_ctrl->play_state == player_stopped) {
                    usleep(10000);
                    URL_DEBUG("--------2 play stoped 2--------\n");
                    break;
                }

                usleep(10000);

                // just let the play status set to playing, which after send avs playstarted event
                if (list_item->stream_item.url_stream.cur_pos ==
                    list_item->stream_item.url_stream.play_pos) {
                    list_item->stream_item.url_stream.cur_pos =
                        list_item->stream_item.url_stream.play_pos + 10;
                }
            }
        } else {
            usleep(10000);
        }
        if (list_item) {
            url_player_list_item_free(list_item);
        }
    }

    return NULL;
}

/*
 * add a url to player
 * Input :
 *         @self: the player self
 *         @url: the alexa url type
 *
 * Output:
 *         0 sucess
 *         other failed
 */
static int url_player_insert_item(url_player_t *self, url_stream_t *url_item)
{
    int ret = -1;
    stream_list_item_t *list_item = NULL;

    if (self && url_item) {
        list_item = url_player_list_item_new(url_item);
        if (list_item) {
            if (url_item->behavior != e_endqueue) {
                if (url_item->behavior == e_replace_all) {
                    self->play_stoped = 1;
                    if (self->clear) {
                        sleep(1);
                    }
                    url_player_clear_play_list(self);
                    wiimu_player_ctrl_stop(self->player_ctrl);
                    URL_DEBUG("++++++++ clear play ++++++++\n");
                } else if (url_item->behavior == e_replace_endqueue) {
                    url_player_clear_play_list(self);
                }
            }
            pthread_mutex_lock(&self->play_list_lock);
            self->total_num++;
            list_item->index = self->total_num - 1;
            URL_DEBUG("total_num=%d next_index=%d behavior=%d pos=%ld"
                      "report_delay=%ld report_interval=%ld\nstream_id:%s\nurl=%s\n",
                      self->total_num, self->next_index, url_item->behavior, url_item->play_pos,
                      url_item->play_report_delay, url_item->play_report_interval,
                      url_item->stream_id, url_item->url);
            lp_list_add_tail(&list_item->list, &self->play_list);
            pthread_mutex_unlock(&self->play_list_lock);
        }
        ret = 0;
    }

    return ret;
}

url_player_t *url_player_create(url_notify_cb_t *notify_cb, void *ctx)
{
    int ret = -1;
    url_player_t *self = NULL;

    self = os_calloc(1, sizeof(url_player_t));
    if (self) {
        self->is_running = 1;
        self->wiimu_shm = (WIIMU_CONTEXT *)common_flags_get();
        self->player_ctrl = wiimu_player_ctrl_init();
        if (self->player_ctrl == NULL) {
            goto Failed;
        }
        self->notify_cb = notify_cb;
        self->ctx = ctx;
        pthread_mutex_init(&self->play_list_lock, NULL);
        LP_INIT_LIST_HEAD(&self->play_list);

        ret = pthread_create(&self->play_pid, NULL, url_player_proc, (void *)self);
        if (ret != 0) {
            goto Failed;
        }
        return self;
    }

Failed:

    if (self) {
        pthread_mutex_destroy(&self->play_list_lock);
        wiimu_player_ctrl_uninit(&self->player_ctrl);
        os_free(self);
    }

    return NULL;
}

int url_player_destroy(url_player_t **self_p)
{
    url_player_t *self = *self_p;

    if (self) {
        self->is_running = 0;

        if (self->play_pid > 0) {
            pthread_join(self->play_pid, NULL);
        }
        url_player_clear_play_list(self);
        pthread_mutex_destroy(&self->play_list_lock);
        wiimu_player_ctrl_uninit(&self->player_ctrl);
        os_free(self);

        *self_p = NULL;
    }

    return 0;
}

/*
 * insert and update a stream
 */
int url_player_insert_stream(url_player_t *self, url_stream_t *url_item)
{
    int ret = -1;

    if (self && url_item) {
        ret = url_player_insert_item(self, url_item);
    }

    return ret;
}

int url_player_update_stream(url_player_t *self, char *stream_item, char *url, long report_delay,
                             long report_interval, long report_position)
{
    int ret = -1;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    stream_list_item_t *list_item = NULL;

    URL_DEBUG("++++++++ update stream report_delay = %ld report_interval = %ld report_position = "
              "%ld ++++++++\n",
              report_delay, report_interval, report_position);

    if (self && stream_item && url) {
        pthread_mutex_lock(&self->play_list_lock);
        if (!lp_list_empty(&self->play_list)) {
            lp_list_for_each_safe(pos, npos, &self->play_list)
            {
                list_item = lp_list_entry(pos, stream_list_item_t, list);
                if (!strncmp(list_item->stream_item.url_stream.stream_item, stream_item,
                             strlen(stream_item))) {
                    if (list_item->stream_item.url_stream.url == NULL) {
                        list_item->stream_item.url_stream.url = strdup(url);
                        if (report_delay >= 0) {
                            list_item->stream_item.url_stream.play_report_delay = report_delay;
                        }
                        if (report_interval >= 0) {
                            list_item->stream_item.url_stream.play_report_interval =
                                report_interval;
                        }
                        if (report_position > 0) {
                            list_item->stream_item.url_stream.play_report_position =
                                report_position;
                        }
                        ret = 0;
                        break;
                    }
                }
            }
        }
        pthread_mutex_unlock(&self->play_list_lock);
    }
    URL_DEBUG("++++++++ update stream end ++++++++\n");

    return ret;
}

int url_player_set_cmd(url_player_t *self, e_player_cmd_t cmd)
{
    int ret = -1;

    if (self && self->player_ctrl) {
        switch (cmd) {
        case e_player_cmd_stop: {
            self->play_stoped = 1;
            ret = wiimu_player_ctrl_stop(self->player_ctrl);
        } break;
        case e_player_cmd_pause: {
            ret = wiimu_player_ctrl_pasue(self->player_ctrl);
        } break;
        case e_player_cmd_resume: {
            ret = wiimu_player_ctrl_resume(self->player_ctrl);
        } break;
        case e_player_cmd_clear_queue: {
            url_player_clear_play_list(self);
            if (self->wiimu_shm && self->wiimu_shm->asr_pause_request == 1) {
                self->wiimu_shm->asr_pause_request = 0;
            }
            pthread_mutex_lock(&self->player_ctrl->ctrl_lock);
            if (self->player_ctrl->stream_id) {
                os_free(self->player_ctrl->stream_id);
                self->player_ctrl->stream_id = NULL;
                os_free(self->player_ctrl->stream_data);
                self->player_ctrl->stream_data = NULL;
            }
            pthread_mutex_unlock(&self->player_ctrl->ctrl_lock);
            ret = 0;
        } break;
        case e_player_cmd_next: {
            if (self->wiimu_shm && self->wiimu_shm->play_mode == PLAYER_MODE_BT)
                wiimu_player_ble_next();
        } break;
        case e_player_cmd_prev: {
            if (self->wiimu_shm && self->wiimu_shm->play_mode == PLAYER_MODE_BT)
                wiimu_player_ble_previous();
        } break;
        case e_player_cmd_replay: {
            ret = wiimu_player_ctrl_seek(self->player_ctrl, 0);
        } break;
        case e_player_cmd_none:
        case e_player_cmd_max:
        default: {
            ret = -1;
        } break;
        }
    }

    return ret;
}

int url_player_get_value(url_player_t *self, e_value_t value_type, int *val)
{
    int ret = -1;
    int volume = 0;
    int mute = 0;

    if (self && val) {
        switch (value_type) {
        case e_value_mute: {
            if (self->player_ctrl) {
                ret = wiimu_player_ctrl_get_volume(self->player_ctrl, &volume, &mute);
                *val = mute;
            }
        } break;
        case e_value_volume: {
            if (self->player_ctrl) {
                ret = wiimu_player_ctrl_get_volume(self->player_ctrl, &volume, &mute);
                *val = volume;
            }
        } break;
        case e_value_repeat: {
            if (self->player_ctrl) {
                pthread_mutex_lock(&self->player_ctrl->ctrl_lock);
                *val = self->player_ctrl->repeat_val;
                pthread_mutex_unlock(&self->player_ctrl->ctrl_lock);
                ret = 0;
            }
        } break;
        case e_value_none:
        case e_value_max:
        default: {
            ret = -1;
        } break;
        }
    }

    return ret;
}

int url_player_set_value(url_player_t *self, e_value_t value_type, int val)
{
    int ret = -1;

    if (self) {
        switch (value_type) {
        case e_value_mute: {
            if (self->player_ctrl) {
                ret = wiimu_player_ctrl_set_volume(self->player_ctrl, 1, val);
            }
        } break;
        case e_value_volume: {
            if (self->player_ctrl) {
                ret = wiimu_player_ctrl_set_volume(self->player_ctrl, 0, val);
            }
        } break;
        case e_value_repeat: {
            if (self->player_ctrl) {
                pthread_mutex_lock(&self->player_ctrl->ctrl_lock);
                self->player_ctrl->repeat_val = val;
                pthread_mutex_unlock(&self->player_ctrl->ctrl_lock);
                ret = 0;
            }
        } break;
        case e_value_none:
        case e_value_max:
        default: {
            ret = -1;
        } break;
        }
    }

    return ret;
}

int url_player_adjust_volume(url_player_t *self, int vol_change)
{
    int ret = -1;

    if (self)
        ret = wiimu_player_ctrl_adjust_volume(self->player_ctrl, vol_change);

    return ret;
}

stream_state_t *url_player_get_state(url_player_t *self)
{
    int state = 0;
    int volume = 0;
    int mute = 0;
    stream_state_t *stream_state = NULL;

    if (self && self->player_ctrl) {
        stream_state = os_calloc(1, sizeof(stream_state_t));
        if (stream_state) {
            wiimu_player_ctrl_get_volume(self->player_ctrl, &volume, &mute);
            stream_state->muted = mute;
            stream_state->volume = volume;
            pthread_mutex_lock(&self->player_ctrl->ctrl_lock);
            switch (self->player_ctrl->play_state) {
            case player_stopping:
            case player_stopped: {
                state = e_stopped;
            } break;
            case player_pausing: {
                state = e_paused;
            } break;
            case player_loading:
            case player_playing: {
                state = e_playing;
            } break;
            case player_init:
            default:
                if (self->player_ctrl->stream_id) {
                    state = e_stopped;
                } else {
                    state = e_idle;
                }
                break;
            }
            if (!self->player_ctrl->stream_id) {
                state = e_idle;
            }
            stream_state->state = state;
            if (state != e_idle) {
                stream_state->stream_id = os_strdup(self->player_ctrl->stream_id);
                stream_state->stream_data = os_strdup(self->player_ctrl->stream_data);
                stream_state->duration = self->player_ctrl->duration;
                stream_state->cur_pos = self->player_ctrl->position;
            }
            stream_state->repeat = self->player_ctrl->repeat_val;
            pthread_mutex_unlock(&self->player_ctrl->ctrl_lock);
        }
    }

    return stream_state;
}

void url_player_free_state(stream_state_t *stream_state)
{
    if (stream_state) {
        os_free(stream_state->stream_id);
        os_free(stream_state->stream_data);
        os_free(stream_state);
    }
}

int url_player_start_play_url(url_player_t *self, int type, const char *url,
                              long long play_offset_ms)
{
    int ret = -1;

    if (self) {
        ret = wiimu_player_start_play_url(type, url, play_offset_ms);
    }

    return ret;
}

void url_player_stop_play_url(url_player_t *self, int type)
{
    if (self) {
        wiimu_player_stop_play_url(type);
    }
}

int url_player_get_play_url_state(url_player_t *self, int type)
{
    int ret = -1;

    if (self) {
        ret = wiimu_player_get_play_url_state(type);
    }

    return ret;
}

int url_player_set_notify2app(url_player_t *self, int value)
{
    if (self)
        self->notify2app = value;

    return 0;
}

int url_player_get_notify2app(url_player_t *self)
{
    if (self)
        return self->notify2app;

    return 0;
}

int url_player_set_clear(url_player_t *self, int value)
{
    if (self)
        self->clear = value;

    return 0;
}

int url_player_get_clear(url_player_t *self)
{
    if (self)
        return self->clear;

    return 0;
}

void url_player_set_request_id(url_player_t *self, unsigned long long id)
{
    if (self)
        self->play_req_id = id;
}

unsigned long long url_player_get_request_id(url_player_t *self)
{
    if (self)
        return self->play_req_id;

    return 0;
}