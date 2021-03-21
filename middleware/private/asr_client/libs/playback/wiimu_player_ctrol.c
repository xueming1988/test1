
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

#include "json.h"
#include "wiimu_player_ctrol.h"

#ifdef NAVER_CLOVA
#include "common_flags.h"
#else
extern WIIMU_CONTEXT *g_wiimu_shm;
#endif

// this is for asprintf
#define _GNU_SOURCE

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
#define WIIMU_ALEXA_VOL_ADJ ("setPlayerCmd:alexa:vol%c")
#define WIIMU_PAUSE ("setPlayerCmd:pauseEx")
#define WIIMU_RESUME ("setPlayerCmd:resume")
#define WIIMU_SEEK ("setPlayerCmd:seek:%lld")
#ifdef ASR_WEIXIN_MODULE
#define WIIMU_PLAYER_WIIMU_NEXT ("setPlayerCmd:next")
#define WIIMU_PLAYER_WIIMU_PREV ("setPlayerCmd:prev")
#define WIIMU_PLAYER_WIIMU_PAUSE ("setPlayerCmd:pause")
#define WIIMU_PLAYER_WIIMU_RESUME ("setPlayerCmd:resume")
#endif
#define Format_Cmd                                                                                 \
    ("GNOTIFY=BLUETOOTH_DIRECTIVE:{"                                                               \
     "\"cmd_type\":\"bluetooth\","                                                                 \
     "\"cmd\":\"%s\","                                                                             \
     "\"params\":%s"                                                                               \
     "}")

#define Create_Ble_Cmd(buff, cmd, parmas)                                                          \
    do {                                                                                           \
        asprintf(&(buff), Format_Cmd, (cmd), (parmas));                                            \
    } while (0)

#define Format_Action                                                                              \
    ("{"                                                                                           \
     "\"action\":\"%s\""                                                                           \
     "}")

#define Create_Ble_Cmd_Params(buff, action)                                                        \
    do {                                                                                           \
        asprintf(&(buff), Format_Action, action);                                                  \
    } while (0)

#define ASR_BluetoothAPI ("/tmp/bt_manager")

#define Ble_Cmd_Send(buf, buf_len, blocked)                                                        \
    do {                                                                                           \
        SocketClientReadWriteMsg(ASR_BluetoothAPI, buf, buf_len, NULL, NULL, blocked);             \
    } while (0)

#define Cmd_MediaControl ("media_control")
#define Action_next ("next")
#define Action_previous ("previous")

#define JSON_OBJECT_NEW_STRING(str)                                                                \
    ((str) ? json_object_new_string((char *)(str)) : json_object_new_string(""))

#define WIIMU_DEBUG(fmt, ...)                                                                      \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d] " fmt, (int)(os_power_on_tick() / 3600000),         \
                (int)(os_power_on_tick() % 3600000 / 60000),                                       \
                (int)(os_power_on_tick() % 60000 / 1000), (int)(os_power_on_tick() % 1000),        \
                ##__VA_ARGS__);                                                                    \
    } while (0)

player_ctrl_t *wiimu_player_ctrl_init(void)
{
    player_ctrl_t *self = (player_ctrl_t *)calloc(1, sizeof(player_ctrl_t));
    if (self) {
        /*
            * If mplayer shmem not create, then create it. else get it
            */
        int err = 0;
        int shmid = shmget(34387, 4096, 0666 | IPC_CREAT | IPC_EXCL);
        err = errno;
        if (shmid != -1) {
            self->mplayer_shmem = (share_context *)shmat(shmid, (const void *)0, 0);
        } else {
            WIIMU_DEBUG("shmget error %d \r\n", errno);
            if (EEXIST == err) {
                shmid = shmget(34387, 0, 0);
                if (shmid != -1) {
                    self->mplayer_shmem = (share_context *)shmat(shmid, (const void *)0, 0);
                } else {
                    free(self);
                    return NULL;
                }
            }
        }

#ifdef NAVER_CLOVA
        self->wiimu_shm = (WIIMU_CONTEXT *)common_flags_get();
#else
        self->wiimu_shm = g_wiimu_shm;
#endif

        if (self->wiimu_shm) {
            self->muted = self->wiimu_shm->mute;
            self->volume = self->wiimu_shm->volume;
            self->play_state = (int)self->wiimu_shm->play_status;
        } else {
            WIIMU_DEBUG("Comm Data havenot init\r\n");
        }
        self->imuzop_state = self->mplayer_shmem->status;
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
        }
        if (player_ctrl->stream_url) {
            free(player_ctrl->stream_url);
            player_ctrl->stream_url = NULL;
        }
        if (player_ctrl->stream_data) {
            free(player_ctrl->stream_data);
            player_ctrl->stream_data = NULL;
        }
        pthread_mutex_unlock(&player_ctrl->ctrl_lock);

        pthread_mutex_destroy(&player_ctrl->ctrl_lock);
        player_ctrl->mplayer_shmem = NULL;
        free(player_ctrl);
        *self = NULL;
        return 0;
    }

    return -1;
}

char *wiimu_player_create_play_cmd(char *token, char *stream_item, char *url, long offset)
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
                              char *stream_data, long position)
{
    int need_new = 0;

    if (self && stream_id && url && self->wiimu_shm) {
        pthread_mutex_lock(&self->ctrl_lock);
        if (!self->stream_id || position == 0) {
            need_new = 1;
        } else if (strncmp(stream_id, self->stream_id, strlen(stream_id))) {
            need_new = 1;
        }
        // TODO: fixed this, is for audiobook/kindlebook
        if ((((self->position - position > 10000) || self->position < position)) ||
            (self->wiimu_shm->play_status >= player_init &&
             self->wiimu_shm->play_status <= player_stopped) ||
            (self->wiimu_shm->play_status != player_pausing)) {
            need_new = 1;
        }
        pthread_mutex_unlock(&self->ctrl_lock);

        if (need_new) {
            pthread_mutex_lock(&self->ctrl_lock);
            if (self->stream_id) {
                free(self->stream_id);
            }
            if (self->stream_url) {
                free(self->stream_url);
            }
            self->stream_url = strdup(url);
            self->stream_id = strdup(stream_id);
            if (stream_data) {
                self->stream_data = strdup(stream_data);
            } else {
                free(self->stream_data);
                self->stream_data = NULL;
            }
            self->position = position;
            self->error_code = 0;
            self->imuzop_state = 0;
            pthread_mutex_unlock(&self->ctrl_lock);

            WIIMU_DEBUG("[MESSAGE TICK %lld] ~~~~ imuzop_state -1 -> 0 ~~~~\n", os_power_on_tick());
            // WIIMU_DEBUG("streamUrl:%s, offset:%ld\n", url, postion);
            char *buf = NULL;
            // Attention: the long long must used %lld !!!!!!!
            buf = wiimu_player_create_play_cmd(stream_id, stream_item, url, position);
            if (buf) {
                // WIIMU_DEBUG("%.100s\n", buf);
                SocketClientReadWriteMsg(WIIMU_PLAYER_CMD, buf, strlen(buf), 0, 0, 0);
                free(buf);
            } else {
                WIIMU_DEBUG("setPlayerCmd ERROR: asprintf return failed\n");
            }
        } else {
            if (self->wiimu_shm->play_status == player_pausing) {
                WIIMU_DEBUG("-------------- play resumeed --------------\n");
                SocketClientReadWriteMsg(WIIMU_PLAYER_CMD, WIIMU_ALEXA_RESUME,
                                         strlen(WIIMU_ALEXA_RESUME), 0, 0, 0);
            } else {
                WIIMU_DEBUG("-------------- play failed --------------\n");
            }
        }

        return 0;
    }

    return -1;
}

int wiimu_player_ctrl_seek(player_ctrl_t *self, long long postion)
{
    int ret = -1;
    char buf[32] = {0};

    if (self && self->wiimu_shm) {
        // TODO:
        if (PLAYER_MODE_IS_ALEXA(self->wiimu_shm->play_mode)) {
            snprintf(buf, sizeof(buf), WIIMU_SEEK, (postion > 0) ? postion : 0);
            SocketClientReadWriteMsg(WIIMU_PLAYER_CMD, buf, strlen(buf), NULL, 0, 1);
            WIIMU_DEBUG("-------------- self->wiimu_shm->play_status(%d) --------------\n",
                        self->wiimu_shm->play_status);
        } else if (self->wiimu_shm && self->wiimu_shm->play_mode == PLAYER_MODE_BT) {
            // TODO: ????? bluetooth support seek ????
            WIIMU_DEBUG("-------------- TODO for bluetooth seek --------------\n",
                        self->wiimu_shm->play_status);
        }
    }

    return ret;
}

int wiimu_player_ctrl_stop(player_ctrl_t *self)
{
    int len = 0;
    int ret = -1;
    char buf[32] = {0};

    if (self && self->wiimu_shm) {
        pthread_mutex_lock(&self->ctrl_lock);
        if (PLAYER_MODE_IS_ALEXA(self->wiimu_shm->play_mode)) {
            if (self->wiimu_shm->play_status != player_pausing) {
                ret = 0;
            }
            if (self->imuzop_state != -1) {
                SocketClientReadWriteMsg(WIIMU_PLAYER_CMD, WIIMU_ALEXA_STOP,
                                         strlen(WIIMU_ALEXA_STOP), buf, &len, 1);
            }
            WIIMU_DEBUG("-------------- self->wiimu_shm->play_status(%d) --------------\n",
                        self->wiimu_shm->play_status);
        } else if (self->wiimu_shm && self->wiimu_shm->play_mode == PLAYER_MODE_BT) {
            int status = CheckPlayerStatus(self->wiimu_shm);
            if (1 == status || 2 == status) {
                WIIMU_DEBUG("-------------- bluetooth play will pause --------------\n");
                SocketClientReadWriteMsg(WIIMU_PLAYER_CMD, WIIMU_PAUSE, strlen(WIIMU_PAUSE), buf,
                                         &len, 1);
            } else {
                WIIMU_DEBUG("-------------- bluetooth is not playing --------------\n");
            }
        }
        self->imuzop_state = -1;
        pthread_mutex_unlock(&self->ctrl_lock);
    }

    return ret;
}

int wiimu_player_ctrl_resume(player_ctrl_t *self)
{
    int ret = -1;

    if (self && self->wiimu_shm) {
        if (self->wiimu_shm->play_status == player_pausing) {
            if (PLAYER_MODE_IS_ALEXA(self->wiimu_shm->play_mode)) {
                ret = 0;
                SocketClientReadWriteMsg(WIIMU_PLAYER_CMD, WIIMU_ALEXA_RESUME,
                                         strlen(WIIMU_ALEXA_RESUME), 0, 0, 0);
            }
        } else if (self->wiimu_shm->play_mode == PLAYER_MODE_BT) {
            ret = 0;
            NotifyMessage(BT_MANAGER_SOCKET, "a2dp:resume");
        }
    }

    return ret;
}
#ifdef ASR_WEIXIN_MODULE
int wiimu_player_ctrl_wiimu_next(player_ctrl_t *self)
{
    int ret = -1;

    if (self && self->wiimu_shm) {
        ret = 0;
        SocketClientReadWriteMsg(WIIMU_PLAYER_CMD, WIIMU_PLAYER_WIIMU_NEXT,
                                 strlen(WIIMU_PLAYER_WIIMU_NEXT), 0, 0, 0);
    }

    return ret;
}

int wiimu_player_ctrl_wiimu_prev(player_ctrl_t *self)
{
    int ret = -1;

    if (self && self->wiimu_shm) {
        ret = 0;
        SocketClientReadWriteMsg(WIIMU_PLAYER_CMD, WIIMU_PLAYER_WIIMU_PREV,
                                 strlen(WIIMU_PLAYER_WIIMU_PREV), 0, 0, 0);
    }

    return ret;
}

int wiimu_player_ctrl_wiimu_pause(player_ctrl_t *self)
{
    int ret = -1;

    if (self && self->wiimu_shm) {
        ret = 0;
        SocketClientReadWriteMsg(WIIMU_PLAYER_CMD, WIIMU_PLAYER_WIIMU_PAUSE,
                                 strlen(WIIMU_PLAYER_WIIMU_PAUSE), 0, 0, 0);
    }

    return ret;
}

int wiimu_player_ctrl_wiimu_resume(player_ctrl_t *self)
{
    int ret = -1;

    if (self && self->wiimu_shm) {
        ret = 0;
        SocketClientReadWriteMsg(WIIMU_PLAYER_CMD, WIIMU_PLAYER_WIIMU_RESUME,
                                 strlen(WIIMU_PLAYER_WIIMU_RESUME), 0, 0, 0);
    }

    return ret;
}

#endif

int wiimu_player_ctrl_pasue(player_ctrl_t *self)
{
    int ret = -1;

    if (self && self->wiimu_shm) {
        int status = CheckPlayerStatus(self->wiimu_shm);
        if (1 == status || 2 == status) {
            WIIMU_DEBUG("-------------- play will pause --------------\n");
            char buf[32] = {0};
            int len = 0;
            if (PLAYER_MODE_IS_ALEXA(self->wiimu_shm->play_mode)) {
                SocketClientReadWriteMsg(WIIMU_PLAYER_CMD, WIIMU_ALEXA_PAUSE,
                                         strlen(WIIMU_ALEXA_PAUSE), buf, &len, 1);
            } else if (self->wiimu_shm && self->wiimu_shm->play_mode == PLAYER_MODE_BT) {
                NotifyMessage(BT_MANAGER_SOCKET, "a2dp:pause");
            }
            // TODO: find a better way
            int conut = 20;
            while (conut--) {
                usleep(20000);
            }
            pthread_mutex_lock(&self->ctrl_lock);
            self->play_state = (int)self->wiimu_shm->play_status;
            WIIMU_DEBUG("-------------- self->wiimu_shm->play_status(%d) --------------\n",
                        self->wiimu_shm->play_status);
            pthread_mutex_unlock(&self->ctrl_lock);
            ret = 0;
        } else {
            WIIMU_DEBUG(
                "-------------- play pause/stop failed(the music is exit) --------------\n");
        }
    }

    return ret;
}

int wiimu_player_ctrl_set_volume(player_ctrl_t *self, int set_mute, int val)
{
    int ret = -1;

    if (self && self->wiimu_shm) {
        if (set_mute) {
            if (val != self->wiimu_shm->mute && val != self->muted) {
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
            if (val != self->wiimu_shm->volume && val != self->volume) {
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

int wiimu_player_ctrl_adjust_volume(player_ctrl_t *self, int vol_change)
{
    char buff[128] = {0};
    char symbol = '+';
    int ret = -1;

    if (self && self->wiimu_shm) {
        if (vol_change == -1)
            symbol = '-';
        else if (vol_change == 1)
            symbol = '+';

        snprintf(buff, sizeof(buff), WIIMU_ALEXA_VOL_ADJ, symbol);
        ret = SocketClientReadWriteMsg(WIIMU_PLAYER_CMD, buff, strlen(buff), 0, 0, 0);

        pthread_mutex_lock(&self->ctrl_lock);
        self->volume = self->wiimu_shm->volume;
        pthread_mutex_unlock(&self->ctrl_lock);
        ret = 0;
    }

    return ret;
}

int wiimu_player_ctrl_get_volume(player_ctrl_t *self, int *volume, int *muted)
{
    int ret = -1;
    if (self && self->wiimu_shm) {
        pthread_mutex_lock(&self->ctrl_lock);
        if (self->muted != self->wiimu_shm->mute) {
            self->muted = self->wiimu_shm->mute;
        }
        if (self->volume != self->wiimu_shm->volume) {
            self->volume = self->wiimu_shm->volume;
        }
        *muted = self->muted;
        *volume = self->volume;
        pthread_mutex_unlock(&self->ctrl_lock);
        ret = 0;
    }

    return ret;
}

int wiimu_player_ctrl_update_info(player_ctrl_t *self)
{
    int ret = -1;
    if (self && self->wiimu_shm && self->mplayer_shmem) {
        pthread_mutex_lock(&self->ctrl_lock);
        self->muted = self->wiimu_shm->mute;
        self->volume = self->wiimu_shm->volume;
        if (PLAYER_MODE_IS_ALEXA(self->wiimu_shm->play_mode)) {
            if (self->mplayer_shmem->status >= 2 && self->imuzop_state >= 2) {
                self->play_state = 0;
                self->imuzop_state = -1;
                WIIMU_DEBUG("~~~~ imuzop_state 2 -> -1 ~~~~\n");
                ret = 1;
            } else {
                self->play_state = (int)self->wiimu_shm->play_status;
                if (self->imuzop_state == 0 && self->mplayer_shmem->status == 1) {
                    self->imuzop_state = 1;
                    WIIMU_DEBUG("~~~~ imuzop_state 0 -> 1 ~~~~\n");
                } else if (self->imuzop_state == 1 && self->mplayer_shmem->status == 2) {
                    self->imuzop_state = 2;
                    WIIMU_DEBUG("~~~~ imuzop_state 1 -> 2 ~~~~\n");
                } else if (self->mplayer_shmem->err_count > 0 && self->mplayer_shmem->status == 2 &&
                           self->imuzop_state == 0 && self->error_code == 0) {
                    self->imuzop_state = 2;
                    self->error_code = 1;
                    WIIMU_DEBUG("~~~~ imuzop play failed ~~~~\n");
                }
                ret = 0;
            }
        } else {
            self->play_state = (int)self->wiimu_shm->play_status;
            ret = 0;
            // WIIMU_DEBUG("~~~~ not play alexa music ~~~~\n");
        }

        // TODO: how to check playback error
        if (self->mplayer_shmem->http_error_code >= 400 && self->mplayer_shmem->status == 2 &&
            self->imuzop_state >= 0) {
            ret = 0;
            self->error_code = self->mplayer_shmem->http_error_code;
            self->mplayer_shmem->http_error_code = 0;
            WIIMU_DEBUG("~~~~ 2 imuzop play failed(%d) ~~~~\n", self->error_code);
        }

        self->position = (long long)(self->mplayer_shmem->offset_pts * 1000.0);
        self->duration = (long long)(self->mplayer_shmem->duration * 1000.0);
        pthread_mutex_unlock(&self->ctrl_lock);

        return ret;
    }

    return ret;
}

#define MAX_URL_SIZE (8192)
#define IMUZO_PATH ("/system/workdir/bin/imuzop")
#define IMUZO_STREAM ("/tmp/stream_%d")

static void format_url(char *url_in, int len_in, char *url_out, int len_out)
{
    strncpy(url_out, "hex://", len_out - 1);
    ascii2hex(url_in, url_out + strlen("hex://"), strlen(url_in), len_out - strlen("hex://"));
}

int wiimu_player_start_play_url(int type, const char *url, long long play_offset_ms)
{
    char stream_imuzop[128] = {0};
    char cmdline[128] = {0};
    char streamtype[128] = {0};
    char option[256] = {0};
    char escape_url[MAX_URL_SIZE] = {0};
    char seekOption[128] = {0};
    long seek_pos = play_offset_ms / 1000;
    int count = 200;

#ifdef NAVER_CLOVA
    WIIMU_CONTEXT *wiimu_shm = (WIIMU_CONTEXT *)common_flags_get();
#else
    WIIMU_CONTEXT *wiimu_shm = g_wiimu_shm;
#endif

    if (!wiimu_shm || type <= e_stream_type_none || type >= e_stream_type_max || !url) {
        WIIMU_DEBUG("input type error %d\n", type);
        return -1;
    }

    if (url && strlen(url) == 0) {
        WIIMU_DEBUG("url is NULL or length is 0\n");
        return -1;
    }

    char *buf = (char *)calloc(1, MAX_URL_SIZE);
    if (buf == NULL) {
        WIIMU_DEBUG("wiimu_player_start_play_url malloc fail  \n");
        return -1;
    }

    snprintf(option, sizeof(option), " ");
    if (seek_pos > 0) {
        printf("seek_offset %ld seconds\n", seek_pos);
        snprintf(seekOption, sizeof(seekOption), " -ss %ld ", seek_pos);
        strcat(option, seekOption);
    }

    if (type == e_stream_type_notify || type == e_stream_type_speak) {
        strcat(option, "-fddis 1");
    }

    memset(stream_imuzop, 0x0, sizeof(stream_imuzop));
    snprintf(stream_imuzop, sizeof(stream_imuzop), "/tmp/stream_%d", type);

    memset(cmdline, 0x0, sizeof(cmdline));
    snprintf(cmdline, sizeof(cmdline), "rm %s; ln -s %s %s 2>/dev/null", stream_imuzop, IMUZO_PATH,
             stream_imuzop);
    system(cmdline);

    snprintf(streamtype, sizeof(streamtype), " -streamtype %d ", type);
    if (strlen(streamtype) > 0) {
        strcat(option, streamtype);
    }
// WIIMU_DEBUG("1 option= %s\n", option);

#if 0
    //workaround for "https://nstr.ai.uplus.co.kr" speak directive cut off issue, so disable fadecache.
    if(strstr(url, ".m3u") != NULL && strstr(url, "https://nstr.ai.uplus.co.kr") != NULL &&
            (type == e_stream_type_speak)) {
        strcat(option, " -fddis 1 ");
    }
#endif

    // workaround for Japanese dHit issue, music.dmkt-sp.jp
    if ((strstr(url, ".m3u") != NULL && !strstr(url, "music.dmkt-sp.jp")) ||
        (strstr(url, ".pls") != NULL) || (strstr(url, ".asx") != NULL) ||
        (strstr(url, "/asx/") != NULL) || (strstr(url, ".ashx") != NULL)) {
        // strcat(option, " -loop 0 -playlist ");
        strcat(option, " -playlist ");
    } else if (strstr(url, "mms://") != NULL) {
        strcat(option, " -cache 524288 ");
    }

    format_url(url, strlen(url), escape_url, sizeof(escape_url));
// WIIMU_DEBUG("2 option= %s\n", option);
#if 0
    snprintf(buf, MAX_URL_SIZE, "%s -af channels=2:2:0:0:0:1 -noquiet -slave %s \"%s\" &", \
             stream_imuzop, option, escape_url);
#else
    snprintf(buf, MAX_URL_SIZE, "%s -noquiet -slave %s \"%s\" &", stream_imuzop, option,
             escape_url);
#endif

    wiimu_shm->imuzo_status_ex[type] = e_status_ex_init;
    WIIMU_DEBUG("%s create %s\n", stream_imuzop, buf);
    system(buf);

    while (wiimu_shm->imuzo_status_ex[type] == e_status_ex_init && count--)
        usleep(10000);

    WIIMU_DEBUG("%s create end at  %lld\n", stream_imuzop, os_power_on_tick());
    free(buf);

    return 0;
}

void wiimu_player_stop_play_url(int type)
{
    char stream_imuzop[32] = {0};

    memset(stream_imuzop, 0x0, sizeof(stream_imuzop));
    snprintf(stream_imuzop, sizeof(stream_imuzop), "/tmp/stream_%d", type);
    killPidbyName(stream_imuzop);
    WIIMU_DEBUG("stop %s\n", stream_imuzop);
}

int wiimu_player_get_play_url_state(int type)
{
    char stream_imuzop[32] = {0};

#ifdef NAVER_CLOVA
    WIIMU_CONTEXT *wiimu_shm = (WIIMU_CONTEXT *)common_flags_get();
#else
    WIIMU_CONTEXT *wiimu_shm = g_wiimu_shm;
#endif

    memset(stream_imuzop, 0x0, sizeof(stream_imuzop));
    snprintf(stream_imuzop, sizeof(stream_imuzop), "/tmp/stream_%d", type);

    if (!wiimu_shm || type <= e_stream_type_none || type >= e_stream_type_max) {
        WIIMU_DEBUG("input type error %d\n", type);
        return -1;
    }

    if (!GetPidbyName(stream_imuzop)) {
        wiimu_shm->imuzo_status_ex[type] = e_status_ex_exit;
        return -1;
    }

    return wiimu_shm->imuzo_status_ex[type];
}

int wiimu_player_ble_next(void)
{
    int ret = -1;
    char *cmd = NULL;
    char *params = NULL;

    Create_Ble_Cmd_Params(params, Action_next);
    if (params) {
        Create_Ble_Cmd(cmd, Cmd_MediaControl, params);
        free(params);
    }
    if (cmd) {
        Ble_Cmd_Send(cmd, strlen(cmd), 0);
        free(cmd);
        ret = 0;
    }

    return ret;
}

int wiimu_player_ble_previous(void)
{
    int ret = -1;
    char *cmd = NULL;
    char *params = NULL;

    Create_Ble_Cmd_Params(params, Action_previous);
    if (params) {
        Create_Ble_Cmd(cmd, Cmd_MediaControl, params);
        free(params);
    }
    if (cmd) {
        Ble_Cmd_Send(cmd, strlen(cmd), 0);
        free(cmd);
        ret = 0;
    }

    return ret;
}

int wiimu_player_ctrl_is_new(player_ctrl_t *self, char *stream_id)
{
    int need_new = 0;

    if (stream_id) {
        pthread_mutex_lock(&self->ctrl_lock);
        if (!self->stream_id) {
            need_new = 1;
        } else if (strncmp(stream_id, self->stream_id, strlen(stream_id))) {
            need_new = 1;
        }
        pthread_mutex_unlock(&self->ctrl_lock);
    }

    return need_new;
}
