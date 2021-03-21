
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "wm_util.h"
#include "json.h"
#include "alexa_json_common.h"

#ifdef EN_AVS_COMMS
#include "alexa_comms.h"
#endif

#include "wiimu_player_ctrol.h"

extern WIIMU_CONTEXT *g_wiimu_shm;
extern share_context *g_mplayer_shmem;

#define os_power_on_tick() tickSincePowerOn()

#define STREAM_TOKEN ("token")
#define STREAM_ITEM ("item")
#define STREAM_URL ("url")
#define STREAM_OFFSET ("offset")

#define WIIMU_PLAYER_CMD ("/tmp/Requesta01controller")
#define WIIMU_ALEXA_PLAY ("setPlayerCmd:alexa:play:%ld:%s")
#define WIIMU_ALEXA_PLAY_NEW ("setPlayerCmd:alexa:play:%s")
#define WIIMU_ALEXA_STOP ("setPlayerCmd:alexa:stop")
#define WIIMU_ALEXA_PAUSE ("setPlayerCmd:alexa:pauseEx")
#define WIIMU_ALEXA_RESUME ("setPlayerCmd:alexa:resume")
#define WIIMU_ALEXA_MUTE_0 ("setPlayerCmd:alexa:mute:0")
#define WIIMU_ALEXA_MUTE_1 ("setPlayerCmd:alexa:mute:1")
#define WIIMU_ALEXA_VOL_SET ("setPlayerCmd:alexa:vol:%d")

#if 0
#define WIIMU_DEBUG(fmt, ...)                                                                      \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d][Wiimu Player] " fmt,                                \
                (int)(os_power_on_tick() / 3600000), (int)(os_power_on_tick() % 3600000 / 60000),  \
                (int)(os_power_on_tick() % 60000 / 1000), (int)(os_power_on_tick() % 1000),        \
                ##__VA_ARGS__);                                                                    \
    } while (0)
#else
#define WIIMU_DEBUG(fmt, ...)                                                                      \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d] " fmt, (int)(os_power_on_tick() / 3600000),         \
                (int)(os_power_on_tick() % 3600000 / 60000),                                       \
                (int)(os_power_on_tick() % 60000 / 1000), (int)(os_power_on_tick() % 1000),        \
                ##__VA_ARGS__);                                                                    \
    } while (0)
#endif

player_ctrl_t *wiimu_player_ctrl_init(void)
{
    player_ctrl_t *self = (player_ctrl_t *)calloc(1, sizeof(player_ctrl_t));
    if (self) {
        if (g_wiimu_shm) {
            self->muted = g_wiimu_shm->mute;
            self->volume = g_wiimu_shm->volume;
            self->play_state = (int)g_wiimu_shm->play_status;
        }
        if (g_mplayer_shmem) {
            self->imuzop_state = g_mplayer_shmem->status;
        }
        self->stream_id = NULL;
        self->position = 0;
        self->duration = 0;
        self->error_code = 0;
        pthread_mutex_init(&self->ctrl_lock, NULL);

        return self;
    }

    return NULL;
}

int wiimu_player_ctrl_uninit(player_ctrl_t **self)
{
    player_ctrl_t *player_ctrl = *self;
    if (player_ctrl) {
        pthread_mutex_lock(&player_ctrl->ctrl_lock);
        if (player_ctrl->stream_id) {
            free(player_ctrl->stream_id);
            player_ctrl->stream_id = NULL;
            WIIMU_DEBUG("!!!!!!!clear the playback token\n");
        }
        pthread_mutex_unlock(&player_ctrl->ctrl_lock);

        pthread_mutex_destroy(&player_ctrl->ctrl_lock);
        free(player_ctrl);
        // g_mplayer_shmem = NULL;
        return 0;
    }

    return -1;
}

// https://audiblecdn-vh.akamaihd.net/i/295890/audiblewords/content/bk/adbl/016592/V$000001$V/aax/bk_adbl_016592_22_32.mp4/master.m3u8?
//  start=4308&end=6361&hdnea=st=1546937768~exp=1546937828~acl=/i/295890/audiblewords/content/bk/adbl/016592/V$000001$V/aax/
//  bk_adbl_016592_22_32.mp4/*~data=4308,6361~hmac=87ab8e811fabc992b856b3585f1deed649acad8186bcb4111668dff5bc171713
static int wiimu_player_is_new_audible_book(char *new_url, char *old_url)
{
    int ret = -1;
    int cmp_string_len = 0;
    // Find a better way to fixed this
    char *end_string = "&hdnea";
    char *cmp_string = NULL;
    char *tmp_string = NULL;

    if (new_url && old_url) {
        tmp_string = strstr(old_url, end_string);
        if (tmp_string) {
            cmp_string_len = tmp_string - old_url + strlen(end_string);
            cmp_string = calloc(1, cmp_string_len + 1);
            if (cmp_string) {
                strncpy(cmp_string, old_url, cmp_string_len);
                cmp_string[cmp_string_len] = '\0';
                WIIMU_DEBUG("~~~~ cmp_string(%s)  ~~~~\n", cmp_string);
                if (strstr(new_url, cmp_string)) {
                    ret = 0;
                }
            }
        }
    }

    if (cmp_string) {
        free(cmp_string);
    }

    return ret;
}

int wiimu_player_ctrl_is_new(player_ctrl_t *self, char *stream_id, char *stream_url, long position,
                             int audible_book)
{
    int need_new = 0;

    if (stream_id) {
        pthread_mutex_lock(&self->ctrl_lock);
        if (!self->stream_id) {
            need_new = 1;
        } else if (audible_book == 1) {
            // This is not a better way to fixed audible book pause to resume to slow
            if (strncmp(stream_id, self->stream_id, strlen(stream_id)) &&
                wiimu_player_is_new_audible_book(stream_url, self->stream_url)) {
                need_new = 1;
            } else if (g_wiimu_shm->play_status == player_pausing &&
                       ((self->position - (long long)position) > 28000 ||
                        ((long long)position - self->position) > 28000)) {
                need_new = 1;
                // TODO: fixed this, is for audiobook
            } else if ((g_wiimu_shm->play_status >= player_init &&
                        g_wiimu_shm->play_status <= player_stopped) ||
                       (g_wiimu_shm->play_status == player_playing)) {
                need_new = 1;
            }
        } else if (strncmp(stream_id, self->stream_id, strlen(stream_id))) {
            need_new = 1;
        } else if (!strncmp(stream_id, self->stream_id, strlen(stream_id)) && audible_book == 0) {
            // This is for seek from Alexa app
            if ((self->position - (long long)position > 5000) ||
                (long long)position - self->position > 5000 || position == 0) {
                need_new = 1;
            }
        }
        pthread_mutex_unlock(&self->ctrl_lock);
    }

    return need_new;
}

char *alexa_create_play_cmd(char *token, char *stream_item, char *url, long offset)
{
    char *buf = NULL;
    json_object *json_obj = json_object_new_object();

    if (json_obj) {
        json_object_object_add(json_obj, STREAM_TOKEN, JSON_OBJECT_NEW_STRING(token));
        json_object_object_add(json_obj, STREAM_ITEM, JSON_OBJECT_NEW_STRING(stream_item));
        json_object_object_add(json_obj, STREAM_URL, JSON_OBJECT_NEW_STRING(url));
        json_object_object_add(json_obj, STREAM_OFFSET, json_object_new_int64(offset));
        asprintf(&buf, WIIMU_ALEXA_PLAY_NEW, (char *)json_object_to_json_string(json_obj));
        if (!buf) {
            WIIMU_DEBUG("-------------- ctreate play cmd failed --------------\n");
        }
        json_object_put(json_obj);
    }

    return buf;
}

int wiimu_player_ctrl_set_uri(player_ctrl_t *self, char *stream_id, char *stream_item, char *url,
                              long position, int audible_book, int is_dialog)
{
    int ret = -1;
    char *buf = NULL;
    int need_new = 0;

    if (self && stream_id && url) {
        pthread_mutex_lock(&self->ctrl_lock);
        if (!self->stream_id || position == 0 || self->provider == PROVIDER_PANDORA) {
            need_new = 1;
        } else if (audible_book == 1) {
            if ((strncmp(stream_id, self->stream_id, strlen(stream_id)) &&
                 wiimu_player_is_new_audible_book(url, self->stream_url)) &&
                is_dialog) {
                need_new = 1;
            } else if (g_wiimu_shm->play_status == player_pausing &&
                       ((self->position - (long long)position) > 28000 ||
                        ((long long)position - self->position) > 28000)) {
                need_new = 1;
                // TODO: fixed this, is for audiobook
            } else if ((g_wiimu_shm->play_status >= player_init &&
                        g_wiimu_shm->play_status <= player_stopped) ||
                       (g_wiimu_shm->play_status == player_playing)) {
                need_new = 1;
            }
        } else if (strncmp(stream_id, self->stream_id, strlen(stream_id))) {
            need_new = 1;
        } else if (!strncmp(stream_id, self->stream_id, strlen(stream_id)) && audible_book == 0) {
            // This is for seek from Alexa app
            if ((self->position - (long long)position > 10000) ||
                (long long)position - self->position > 10000) {
                need_new = 1;
            }
        }
        if (self->stream_id) {
            free(self->stream_id);
        }
        if (self->stream_url) {
            free(self->stream_url);
        }
        self->stream_url = strdup(url);
        self->stream_id = strdup(stream_id);

        if (need_new) {
            WIIMU_DEBUG("[MESSAGE TICK %lld] ~~~~ imuzop_state -1 -> 0 self.pos(%lld) "
                        "position(%ld) play_status(%d)  ~~~~\n",
                        os_power_on_tick(), self->position, position, g_wiimu_shm->play_status);
            buf = alexa_create_play_cmd(stream_id, stream_item, url, position);
            if (buf) {
                SocketClientReadWriteMsg(WIIMU_PLAYER_CMD, buf, strlen(buf), 0, 0, 0);
                free(buf);
                ret = 0;
            } else {
                WIIMU_DEBUG("setPlayerCmd ERROR: asprintf return failed\n");
            }
            self->position = position;
            self->error_code = 0;
            self->imuzop_state = 0;
        } else {
            if (g_wiimu_shm->play_status == player_pausing) {
                WIIMU_DEBUG("-------------- play resumeed --------------\n");
                SocketClientReadWriteMsg(WIIMU_PLAYER_CMD, WIIMU_ALEXA_RESUME,
                                         strlen(WIIMU_ALEXA_RESUME), 0, 0, 0);
                ret = 0;
            } else {
                WIIMU_DEBUG("-------------- play failed --------------\n");
            }
        }

        self->provider = 0;
        pthread_mutex_unlock(&self->ctrl_lock);

        return ret;
    }

    return ret;
}

int wiimu_player_ctrl_seek(player_ctrl_t *self, long long postion)
{
    if (self) {
        // TODO:
        return 0;
    }

    return -1;
}

int wiimu_player_ctrl_stop(player_ctrl_t *self, int need_stop)
{
    int len = 0;
    int ret = -1;
    char buf[32] = {0};

    if (self) {
        pthread_mutex_lock(&self->ctrl_lock);
        if (PLAYER_MODE_IS_ALEXA(g_wiimu_shm->play_mode)) {
            if (g_wiimu_shm->play_status != player_pausing && self->imuzop_state >= 0) {
                ret = 0;
            }
            if (self->imuzop_state != -1 && need_stop) {
                SocketClientReadWriteMsg(WIIMU_PLAYER_CMD, WIIMU_ALEXA_STOP,
                                         strlen(WIIMU_ALEXA_STOP), buf, &len, 1);
            }
            WIIMU_DEBUG("----- g_wiimu_shm->play_status(%d) imuzop_state(%d) "
                        "mplayer_shmem->status(%d) -----\n",
                        g_wiimu_shm->play_status, self->imuzop_state, g_mplayer_shmem->status);
        }
        self->imuzop_state = -1;
        pthread_mutex_unlock(&self->ctrl_lock);
    }

    return ret;
}

int wiimu_player_ctrl_resume(player_ctrl_t *self)
{
    int ret = -1;

    if (self) {
        if (PLAYER_MODE_IS_ALEXA(g_wiimu_shm->play_mode)) {
            if (g_wiimu_shm->play_status == player_pausing) {
                ret = 0;
                SocketClientReadWriteMsg(WIIMU_PLAYER_CMD, WIIMU_ALEXA_RESUME,
                                         strlen(WIIMU_ALEXA_RESUME), 0, 0, 0);
            }
        }
    }

    return ret;
}

int wiimu_player_ctrl_pasue(player_ctrl_t *self)
{
    int ret = -1;

    if (self) {
        // if(PLAYER_MODE_IS_ALEXA(g_wiimu_shm->play_mode)) {//remove by weiqiang.huang for MAR-330
        int status = CheckPlayerStatus(g_wiimu_shm);
        if (1 == status || 2 == status) {
            WIIMU_DEBUG("-------------- play will pause, need a better way --------------\n");
            char buf[32] = {0};
            int len = 0;
            SocketClientReadWriteMsg(WIIMU_PLAYER_CMD, WIIMU_ALEXA_PAUSE, strlen(WIIMU_ALEXA_PAUSE),
                                     buf, &len, 1);
            // TODO: find a better way
            int conut = 20;
            // while((!g_wiimu_shm->codec_mcu_mute) &&(conut--)) {
            while (conut--) {
                usleep(20000);
            }
            pthread_mutex_lock(&self->ctrl_lock);
            self->play_state = (int)g_wiimu_shm->play_status;
            WIIMU_DEBUG("-------------- g_wiimu_shm->play_status(%d) --------------\n",
                        g_wiimu_shm->play_status);
            pthread_mutex_unlock(&self->ctrl_lock);
            ret = 0;
        } else {
            WIIMU_DEBUG(
                "-------------- play pause/stop failed(the music is exit) --------------\n");
        }
        //}//remove by weiqiang.huang for MAR-330
    }

    return ret;
}

int wiimu_player_ctrl_set_volume(player_ctrl_t *self, int set_mute, int val)
{
    int ret = -1;

    if (self) {
        if (set_mute) {
            // if (val != g_wiimu_shm->mute && val != self->muted) {
            {
                if (val) {
                    SocketClientReadWriteMsg(WIIMU_PLAYER_CMD, WIIMU_ALEXA_MUTE_1,
                                             strlen(WIIMU_ALEXA_MUTE_1), 0, 0, 0);
                } else {
                    SocketClientReadWriteMsg(WIIMU_PLAYER_CMD, WIIMU_ALEXA_MUTE_0,
                                             strlen(WIIMU_ALEXA_MUTE_0), 0, 0, 0);
                }
                pthread_mutex_lock(&self->ctrl_lock);
                self->muted = val;
                pthread_mutex_unlock(&self->ctrl_lock);
                ret = 0;
            }
        } else {
            if (val != g_wiimu_shm->volume && val != self->volume) {
                if (val >= 100) {
                    val = 100;
                }
                if (val <= 0) {
                    val = 0;
                }
                char buff[128] = {0};
                snprintf(buff, sizeof(buff), WIIMU_ALEXA_VOL_SET, val);
                WIIMU_DEBUG("%s\n", buff);
                SocketClientReadWriteMsg(WIIMU_PLAYER_CMD, buff, strlen(buff), 0, 0, 0);
                pthread_mutex_lock(&self->ctrl_lock);
                self->volume = val;
                pthread_mutex_unlock(&self->ctrl_lock);
                ret = 0;
            }
        }
    }

    return ret;
}

int wiimu_player_ctrl_get_volume(player_ctrl_t *self, int *volume, int *muted)
{
    int ret = -1;
    if (self) {
        pthread_mutex_lock(&self->ctrl_lock);
        *muted = self->muted;
        if (self->muted != g_wiimu_shm->mute) {
            self->muted = g_wiimu_shm->mute;
        }
        if (self->volume != g_wiimu_shm->volume) {
            self->volume = g_wiimu_shm->volume;
        }
        *volume = self->volume;
        pthread_mutex_unlock(&self->ctrl_lock);
        ret = 0;
    }

    return ret;
}

int wiimu_player_ctrl_update_info(player_ctrl_t *self)
{
    int ret = -1;
    if (self && g_wiimu_shm && g_mplayer_shmem) {
        pthread_mutex_lock(&self->ctrl_lock);
        self->muted = g_wiimu_shm->mute;
        self->volume = g_wiimu_shm->volume;
        if (PLAYER_MODE_IS_ALEXA(g_wiimu_shm->play_mode)) {
            if (g_mplayer_shmem->status >= 2 && self->imuzop_state >= 2) {
                self->imuzop_state = -1;
                // self->error_code = g_mplayer_shmem->http_error_code;
                WIIMU_DEBUG("~~~~ imuzop_state 2 -> -1 error(%d)~~~~\n", self->error_code);
#ifdef EN_AVS_COMMS
                int call_active = avs_comms_is_avtive();
                if (call_active) {
                    if (self->stream_id) {
                        free(self->stream_id);
                        self->stream_id = NULL;
                    }
                    self->play_state = 0;
                } else {
                    self->play_state = player_stopped;
                }
#else
                self->play_state = 0;
#endif
                ret = 1;
            } else {
                self->play_state = (int)g_wiimu_shm->play_status;
                if (self->imuzop_state == 0 && g_mplayer_shmem->status == 1 &&
                    self->play_state == player_playing) {
                    self->imuzop_state = 1;
                    WIIMU_DEBUG("~~~~ imuzop_state 0 -> 1 ~~~~\n");
                } else if (self->imuzop_state == 1 && g_mplayer_shmem->status == 2) {
                    self->imuzop_state = 2;
                    WIIMU_DEBUG("~~~~ imuzop_state 1 -> 2 ~~~~\n");
                } else if (g_mplayer_shmem->err_count > 0 && g_mplayer_shmem->status == 2 &&
                           self->imuzop_state == 0 && self->error_code == 0) {
                    self->imuzop_state = 2;
                    self->error_code = 1;
                    WIIMU_DEBUG("~~~~ 1 imuzop play failed error(%d)~~~~\n", self->error_code);
                }
                ret = 0;
            }
        } else {
            self->play_state = (int)g_wiimu_shm->play_status;
            ret = 0;
            // WIIMU_DEBUG("~~~~ not play alexa music ~~~~\n");
        }

        // TODO: how to check playback error
        if (g_mplayer_shmem->http_error_code >= 400 && g_mplayer_shmem->status == 2 &&
            self->imuzop_state >= 0) {
            ret = 0;
            self->error_code = g_mplayer_shmem->http_error_code;
            WIIMU_DEBUG("~~~~ 2 imuzop play failed error(%d) ~~~~\n", self->error_code);
        }

        self->position = (long long)(g_mplayer_shmem->offset_pts * 1000.0);
        self->duration = (long long)(g_mplayer_shmem->duration * 1000.0);
        pthread_mutex_unlock(&self->ctrl_lock);

        return ret;
    }

    return ret;
}
