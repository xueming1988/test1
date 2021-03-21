/*************************************************************************
    > File Name: cid_player.c
    > Author: Keying.Zhao
    > Mail: Keying.Zhao@linkplay.com
    > Created Time: Tue 21 Nov 2017 02:35:55 AM EST
    > brief:
    >   A player for mp3/pcm buffer stream, url/file stream
 ************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include <alsa/asoundlib.h>

#include "cmd_q.h"
#include "os_define.h"
#include "cid_player.h"
#include "cache_buffer.h"
#include "mp3_decode.h"
#include "alsa_device.h"

#ifdef X86
// DO Nothing
#else
#include "lp_list.h"
#include "wm_util.h"

extern WIIMU_CONTEXT *g_wiimu_shm;
#endif

enum {
    e_cache_read = 0,
    e_cache_write = 2,
    e_cache_play = 3,
};

typedef enum {
    e_state_idle,
    e_state_pause,
    e_state_stopped,
    e_state_playing,
} e_status_t;

typedef struct _cid_item_s {
    int is_active;
    cid_stream_t cid_stream;
    cache_buffer_t *cache_buffer;
    struct lp_list_head cid_item;
} cid_item_t;

typedef struct _cid_cmd_s {
    e_cmd_type_t cmd_type;
    void *data;
} cid_cmd_t;

struct _cid_player_s {
    // for sound card info
    int fix_volume;
    int is_running;
    int play_inited;

    // for status
    int is_force_stop;
    e_status_t play_state;

    pthread_mutex_t state_lock;
    pthread_t thread_id;

    // for play buffer
    struct lp_list_head item_list;
    pthread_mutex_t list_lock;

    cid_item_t *current_item;
    cmd_q_t *play_cmd;
    alsa_device_t *alsa_dev;
    mp3_decode_t *mp3_dec;
    FILE *file_ptr;
    cid_notify_cb_t *notify_cb;
    void *notify_ctx;
#ifdef X86
// DO Nothing
#else
    WIIMU_CONTEXT *wiimu_shm;
#endif
};

static cid_item_t *cid_player_get_item(cid_player_t *self, char *item_id, int flags);

static int device_init(cid_player_t *self, int fmt, unsigned int *samplerate,
                       unsigned short channels)
{
    int ret = -1;

    os_assert(self && self->current_item && samplerate, "check input error\n");
    if (self->alsa_dev && self->play_inited == 0) {
        if (channels == 1) {
            channels = 2;
        }
        ret = alsa_device_init(self->alsa_dev, fmt, samplerate, channels);
        if (ret == 0) {
            self->play_inited = 1;
            os_trace("[TICK]=========== show_first_write_ts at %lld ===========\n",
                     os_power_on_tick());
#if defined(MARSHALL_STANMORE_AVS_TBD_BETA) || defined(MARSHALL_ACTON_AVS_TBD_BETA) ||             \
    defined(PURE_RPC)
            int count = 25;
            // usleep(50000);
            if (self->wiimu_shm->device_status == DEV_STANDBY) {
                printf("Start to wait for the MCU resume status before TTS\n");
                while (count--) {
                    if (g_wiimu_shm->device_status != DEV_STANDBY)
                        break;
                    usleep(100000);
                }
                printf("End to wait for the MCU resume status %d before TTS\n",
                       g_wiimu_shm->device_status);
            }
#endif
#if defined(HARMAN)
            alsa_device_play_silence(self->alsa_dev, 200);
#else
            alsa_device_play_silence(self->alsa_dev, 50);
#endif
        }
    }

    return ret;
}

static int device_uninit(cid_player_t *self)
{
    int ret = 0;

    os_assert(self, "input error");
    if (self->alsa_dev) {
        self->play_inited = 0;
        alsa_device_uninit(self->alsa_dev);
        self->alsa_dev = NULL;
    }

    return ret;
}

static int get_volume(cid_player_t *self)
{
    int volume = 0x10000;

    os_assert(self, "input error");
    if (self->alsa_dev) {
        volume = self->fix_volume;
    }

    return volume;
}

// NOTICE: If read data return < 0; the man loop will break, else the loop will keep going
static int read_data(cid_player_t *self, char *data, int len)
{
    int read_len = -1;
    e_cache_state_t state = e_can_read;
    cid_item_t *item_ptr = NULL;

    os_assert(self && self->current_item && data, "check input error\n");
    if (len > 0) {
        item_ptr = cid_player_get_item(self, self->current_item->cid_stream.token, e_cache_read);
        if (item_ptr && item_ptr->cache_buffer) {
            read_len = cache_buffer_read(item_ptr->cache_buffer, data, len, &state);
            if (read_len == 0 && e_can_read != state) {
                read_len = -1;
                if (self->notify_cb) {
                    self->notify_cb(e_notify_near_finished, &self->current_item->cid_stream,
                                    self->notify_ctx);
                }
            }
        } else {
            os_trace("didnot find a buffer to read tick=%lld\n", os_power_on_tick());
        }
    }

    return read_len;
}

static int play(cid_player_t *self, unsigned short *output_ptr, int len)
{
    int err = -1;

    os_assert(self && output_ptr, "check input error\n");
    if (self->alsa_dev) {
        if (self->is_force_stop == 0) {
#ifdef X86
#else
            /*add by weiqiang.huang for CER-24 2017-0713 -- begin*/
            // while alart ringing, play dircetive streaming should be paused
            // just write scilence data into sound card
            if (self->wiimu_shm->alert_status && self->current_item &&
                self->current_item->cid_stream.stream_type == e_stream_play) {
                unsigned short *silent_buf =
                    (unsigned short *)calloc(1, 8192 * sizeof(unsigned short));
                while (self->wiimu_shm->alert_status) {
                    err = alsa_device_play_pcm(self->alsa_dev, silent_buf, 8192);
                    if (err == 0) {
                        usleep(20000);
                    }
                }
                if (silent_buf) {
                    free(silent_buf);
                }
            }
/*add by weiqiang.huang for CER-24 2017-0713 -- end*/
#endif
            err = alsa_device_play_pcm(self->alsa_dev, output_ptr, len);
        }
    }

    return err;
}

static int cid_player_list_empty(cid_player_t *self)
{
    int ret = -1;

    os_assert(self, "self is NULL\n");
    pthread_mutex_lock(&self->list_lock);
    if (!lp_list_empty(&self->item_list)) {
        ret = 0;
    }
    pthread_mutex_unlock(&self->list_lock);

    return ret;
}

static void cid_player_free_item(cid_item_t **p_item)
{
    cid_item_t *item = *p_item;
    if (item) {
        os_free(item->cid_stream.dialog_id);
        os_free(item->cid_stream.token);
        os_free(item->cid_stream.uri);
        if (item->cache_buffer) {
            cache_buffer_destroy(&item->cache_buffer);
            item->cache_buffer = NULL;
        }
        os_free(item);
        *p_item = NULL;
    }
}

static void cid_player_del_item(cid_player_t *self, cid_item_t *item)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    cid_item_t *item_ptr = NULL;

    os_assert(self, "self is NULL\n");
    pthread_mutex_lock(&self->list_lock);
    if (!lp_list_empty(&self->item_list)) {
        lp_list_for_each_safe(pos, npos, &self->item_list)
        {
            item_ptr = lp_list_entry(pos, cid_item_t, cid_item);
            if (item != NULL) {
                if (item == item_ptr) {
                    lp_list_del(&item_ptr->cid_item);
                    cid_player_free_item(&item_ptr);
                    break;
                }
            } else {
                if (item_ptr) {
                    lp_list_del(&item_ptr->cid_item);
                    cid_player_free_item(&item_ptr);
                }
            }
        }
    }
    pthread_mutex_unlock(&self->list_lock);
}

static int cid_player_clear_item(cid_player_t *self)
{
    int ret = -1;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    cid_item_t *item_ptr = NULL;

    os_assert(self, "self is NULL\n");
    pthread_mutex_lock(&self->list_lock);
    if (!lp_list_empty(&self->item_list)) {
        lp_list_for_each_safe(pos, npos, &self->item_list)
        {
            item_ptr = lp_list_entry(pos, cid_item_t, cid_item);
            if (item_ptr && item_ptr->is_active == 0) {
                lp_list_del(&item_ptr->cid_item);
                cid_player_free_item(&item_ptr);
            }
            ret = 0;
        }
    }
    pthread_mutex_unlock(&self->list_lock);

    return ret;
}

/*
 *  info: get the a item
 *  input:
 *      @self:
 *          the current object
 *      @item_id:
 *          the id for the item
 *      @flags:
 *          the status buffer want to get
 *
 */
static cid_item_t *cid_player_get_item(cid_player_t *self, char *item_id, int flags)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    cid_item_t *item_ptr = NULL;

    os_assert(self, "self is NULL\n");
    pthread_mutex_lock(&self->list_lock);
    if (!lp_list_empty(&self->item_list)) {
        lp_list_for_each_safe(pos, npos, &self->item_list)
        {
            item_ptr = lp_list_entry(pos, cid_item_t, cid_item);
            if (item_ptr) {
                if (e_cache_write == flags && (os_sub_str(item_ptr->cid_stream.token, item_id) ||
                                               os_sub_str(item_id, item_ptr->cid_stream.uri))) {
                    break;
                } else if (e_cache_read == flags &&
                           (os_sub_str(item_ptr->cid_stream.token, item_id) ||
                            os_sub_str(item_id, item_ptr->cid_stream.uri)) &&
                           item_ptr->is_active) {
                    break;
                } else if (e_cache_play == flags) {
                    item_ptr->is_active = 1;
                    break;
                } else {
                    item_ptr = NULL;
                }
            }
        }
    }
    pthread_mutex_unlock(&self->list_lock);

    return item_ptr;
}

static int cid_player_buffer_play(cid_player_t *self, int is_force_stop)
{
    int ret = 0;

    os_assert(self && self->current_item, "check input error\n");
    if (e_state_playing == self->play_state) {
        // init decode
        if (!self->mp3_dec) {
            mp3_player_ctx_t mp3_ctx = {
                .get_volume = get_volume,
                .device_init = device_init,
                .device_uninit = device_uninit,
                .read_data = read_data,
                .play = play,
                .player_ctx = self,
            };
            self->mp3_dec = decode_init(mp3_ctx);
        }

        if (self->mp3_dec) {
            // decode buffer
            int dec_size = 0;
            int rv = decode_start(self->mp3_dec, &dec_size);
            if (rv == MP3_DEC_RET_EXIT) {
                os_trace("----- decode_start error e_tts_stop ------\n");
                self->play_state = e_state_stopped;
            } else if (rv == MP3_DEC_RET_NETSLOW) {
                // must sleep some times
                usleep(5000);
            } else if (rv == MP3_DEC_RET_WARN) {
                // decode warning, read next data to decode
                if (dec_size < 0) {
                    os_trace("----- decode_start error dec_size < 0 ------\n");
                    self->play_state = e_state_stopped;
                }
            } else if (rv == MP3_DEC_RET_OK) {
                // play audio frame
                if (self && self->alsa_dev == NULL) {
                    self->alsa_dev = alsa_device_open(DEFAULT_DEV);
                }
                if (decode_play(self->mp3_dec) < 0) {
                    os_trace("decode_play eror is_force_stop = %d tick=%lld\n", self->is_force_stop,
                             os_power_on_tick());
                    device_uninit(self);
                } else {
                    // usleep(5000);
                }
            }
        } else {
            self->play_state = e_state_stopped;
        }

    } else if (e_state_stopped == self->play_state) {
        // error, exit play

        if (self->mp3_dec) {
            decode_uninit(self->mp3_dec);
            self->mp3_dec = NULL;
#ifdef X86
#else
            if (self->wiimu_shm) {
                self->wiimu_shm->asr_status = 0;
                self->wiimu_shm->alexa_response_status = 0;
            }
#endif
            if (self->notify_cb) {
                if (is_force_stop == 0) {
                    self->notify_cb(e_notify_finished, &self->current_item->cid_stream,
                                    self->notify_ctx);
                } else {
                    if (self->current_item->cid_stream.stream_type == e_stream_play) {
                        self->notify_cb(e_notify_stopped, &self->current_item->cid_stream,
                                        self->notify_ctx);
                    } else if (self->current_item->cid_stream.stream_type == e_stream_speak) {
                        self->notify_cb(e_notify_finished, NULL, self->notify_ctx);
                    }
                }
            }
        }

        if (self->current_item) {
            cid_player_del_item(self, self->current_item);
            self->current_item = NULL;
        }
        if (self->notify_cb && 0 != cid_player_list_empty(self)) {
            self->notify_cb(e_notify_end, NULL, self->notify_ctx);
            usleep(50000);
#ifdef X86
#else
            if (self->wiimu_shm && self->wiimu_shm->asr_pause_request) {
                self->wiimu_shm->asr_pause_request = 0;
            }
#endif
        }
        os_trace("----- CID tts play ending ------\n");
        self->play_state = e_state_idle;
    } else {
        usleep(20000);
    }

Exit:

    return ret;
}

static int cid_player_pcm_init(cid_player_t *self)
{
    int ret = -1;
#ifdef ASR_WEIXIN_MODULE
    unsigned int samplerate = 16000;
#else
    unsigned int samplerate = 22050;
#endif

    os_assert(self, "check input is error\n");
    if (self->alsa_dev) {
        ret = alsa_device_init(self->alsa_dev, SND_PCM_FORMAT_S16_LE, &samplerate, 1);
        if (ret == 0) {
            self->play_inited = 1;
            os_trace("[TICK]=========== show_first_write_ts at %lld ===========\n",
                     os_power_on_tick());
            alsa_device_play_silence(self->alsa_dev, 450);
        }
    }

    return ret;
}

static int cid_player_play_pcm(cid_player_t *self)
{
    int read_len = -1;
    char buffer[8192] = {0};
    e_cache_state_t state = e_can_read;
    cid_item_t *item_ptr = NULL;

    os_assert(self && self->current_item && self->current_item->cid_stream.token,
              "check input is error\n");
    item_ptr = cid_player_get_item(self, self->current_item->cid_stream.token, e_cache_read);
    if (item_ptr && item_ptr->cache_buffer) {
        read_len = cache_buffer_read(item_ptr->cache_buffer, buffer, sizeof(buffer), &state);
        if (read_len == 0 && e_can_read != state) {
            read_len = -1;
            if (self->notify_cb) {
                self->notify_cb(e_notify_near_finished, &self->current_item->cid_stream,
                                self->notify_ctx);
            }
        } else {
            if (read_len > 0) {
                play(self, (unsigned short *)buffer, read_len / 2);
            }
        }
    } else {
        os_trace("didnot find a buffer to read tick=%lld\n", os_power_on_tick());
    }

    return read_len;
}

static int cid_player_pcm_play(cid_player_t *self, int is_force_stop)
{
    int ret = 0;

    os_assert(self && self->current_item, "check input is error\n");
    if (e_state_playing == self->play_state) {
        if (self->alsa_dev == NULL) {
            // play audio frame
            self->alsa_dev = alsa_device_open(DEFAULT_DEV);
        }
        if (self->alsa_dev) {
            if (self->play_inited == 0) {
                ret = cid_player_pcm_init(self);
            }
            if (self->play_inited == 1) {
                ret = cid_player_play_pcm(self);
                if (ret == 0) {
                    usleep(5000);
                }
            }
            if (ret == -1) {
                self->play_state = e_state_stopped;
                usleep(5000);
            }
        } else {
            os_trace("alsa_device_open eror tick=%lld\n", os_power_on_tick());
            self->play_state = e_state_stopped;
            usleep(5000);
        }
    } else if (e_state_stopped == self->play_state) {
        // error, exit play
        self->play_inited = 0;
        if (self->alsa_dev) {
            alsa_device_uninit(self->alsa_dev);
            self->alsa_dev = NULL;
#ifdef X86
#else
            if (self->wiimu_shm) {
                self->wiimu_shm->asr_status = 0;
                self->wiimu_shm->alexa_response_status = 0;
            }
#endif
            if (self->notify_cb) {
                if (is_force_stop == 0) {
                    self->notify_cb(e_notify_finished, &self->current_item->cid_stream,
                                    self->notify_ctx);
                } else {
                    self->notify_cb(e_notify_finished, NULL, self->notify_ctx);
                }
            }
        }
        if (self->current_item) {
            cid_player_del_item(self, self->current_item);
            self->current_item = NULL;
        }
        if (self->notify_cb && 0 != cid_player_list_empty(self)) {
            self->notify_cb(e_notify_end, NULL, self->notify_ctx);
#ifdef X86
#else
            if (self->wiimu_shm && self->wiimu_shm->asr_pause_request) {
                self->wiimu_shm->asr_pause_request = 0;
            }
#endif
        }
        os_trace("----- CID tts play ending ------\n");
        self->play_state = e_state_idle;
    } else {
        usleep(20000);
    }

Exit:

    return ret;
}

void *cid_player_run(void *args)
{
    cid_player_t *self = (cid_player_t *)args;

    os_assert(self, "self is NULL\n");
    self->is_running = 1;
    while (self->is_running == 1) {
        int is_force_stop = 0;
        int *cmd_type = NULL;

        if (NULL == self->current_item) {
            device_uninit(self);
            self->current_item = cid_player_get_item(self, NULL, e_cache_play);
            if (self->current_item && self->current_item->cid_stream.token) {
                cmd_q_clear(self->play_cmd); // new cid item comes,cler before cmd queue
                self->play_state = e_state_playing;
                if (self->is_force_stop) {
                    self->is_force_stop = 0;
                }
                if (self->current_item->cid_stream.cid_type == e_type_buffer_mp3 ||
                    self->current_item->cid_stream.cid_type == e_type_buffer_pcm) {
                    if (self->notify_cb) {
                        self->notify_cb(e_notify_started, &self->current_item->cid_stream,
                                        self->notify_ctx);
                    }
#ifdef X86
// DO Nothing
#else
                    if (self->wiimu_shm) {
                        if (self->wiimu_shm->asr_pause_request == 0) {
                            self->wiimu_shm->asr_pause_request = 1;
                        }
                        self->wiimu_shm->alexa_response_status = 1;
                    }
#endif
                }
            }
        }

        cmd_type = (int *)cmd_q_pop(self->play_cmd);
        if (cmd_type) {
            pthread_mutex_lock(&self->state_lock);
            if (e_cmd_stop == *cmd_type) {
                if (self->current_item != NULL) {
                    self->play_state = e_state_stopped;
                    if (self->is_force_stop) {
                        self->is_force_stop = 0;
                        is_force_stop = 1;
                    }
                    if (self->current_item->cid_stream.cid_type == e_type_url) {
                    }
                }
            } else if (e_cmd_pause == *cmd_type) {
                if (self->is_force_stop) {
                    self->is_force_stop = 0;
                }
                self->play_state = e_state_pause;
            }
            pthread_mutex_unlock(&self->state_lock);
            os_free(cmd_type);
        }

        if (self->current_item) {
            if (self->current_item->cid_stream.cid_type == e_type_buffer_mp3) {
                cid_player_buffer_play(self, is_force_stop);
            } else if (self->current_item->cid_stream.cid_type == e_type_url) {
            } else if (self->current_item->cid_stream.cid_type == e_type_file) {
                // TODO: file play
            } else if (self->current_item->cid_stream.cid_type == e_type_buffer_pcm) {
                cid_player_pcm_play(self, is_force_stop);
            } else {
                usleep(20000);
            }
        } else {
            usleep(20000);
        }
    }

    if (self->mp3_dec) {
        decode_uninit(self->mp3_dec);
    }
    device_uninit(self);
#ifdef X86
#else
    if (self->wiimu_shm) {
        self->wiimu_shm->alexa_response_status = 0;
    }
#endif
    os_trace("Audio Processor stopped !\n");

    return NULL;
}

static int cid_player_init(cid_player_t *self)
{
    int ret = -1;
    pthread_attr_t attr;
    struct sched_param param;

    os_assert(self, "self is NULL\n");
    pthread_mutex_init(&self->state_lock, NULL);
    pthread_mutex_init(&self->list_lock, NULL);

    self->is_running = 1;
    self->fix_volume = 0x10000;

    self->alsa_dev = NULL;
    self->mp3_dec = NULL;
    LP_INIT_LIST_HEAD(&self->item_list);

    self->play_cmd = cmd_q_init(NULL);

    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    param.sched_priority = 90;
    pthread_attr_setschedparam(&attr, &param);

    ret = pthread_create(&self->thread_id, &attr, cid_player_run, self);
    if (ret != 0) {
        os_trace("cid_player_run create faield!\n");
        self->thread_id = 0;
    }
    pthread_attr_destroy(&attr);

    return ret;
}

/*
 *  info: create the cid_player object
 *  input:
 *      @notify_cb:
 *          The callback about the cid player state notify
 *      @ctx:
 *          The context which will pass through by callback
 *
 *  output:
 *      The point of cid_player_t object
 */
cid_player_t *cid_player_create(cid_notify_cb_t *notify_cb, void *notify_ctx)
{
    int ret = -1;

    cid_player_t *self = (cid_player_t *)os_calloc(1, sizeof(cid_player_t));
    os_assert(self, "self calloc failed\n");
    self->notify_cb = notify_cb;
    self->notify_ctx = notify_ctx;
#ifdef X86
#else
    self->wiimu_shm = g_wiimu_shm;
#endif
    ret = cid_player_init(self);
    if (ret != 0) {
        cid_player_destroy(&self);
        self = NULL;
    }

    return self;
}

/*
 *  brief: free the cid_player object
 *  input:
 *      @self_p:
 *          The point of the cid_player object
 *  output:
 *
 */
void cid_player_destroy(cid_player_t **self_p)
{
    cid_player_t *self = *self_p;

    if (self) {
        cid_player_set_cmd(self, e_cmd_stop);
        self->is_running = 0;
        if (self->thread_id > 0) {
            pthread_join(self->thread_id, NULL);
        }
        cmd_q_uninit(&self->play_cmd);
        cid_player_del_item(self, NULL);
        pthread_mutex_destroy(&self->state_lock);
        os_free(self);
        *self_p = NULL;
    }
}

/*
 *  brief:
 *      Insert a item to the cid_player
 *  input:
 *      @self:
 *          The object of the cid_player
 *      @cid_stream:
 *          The cid stream metadata
 *  output:
 *      @ 0   sucess
 *      @ !0  failed
 */
int cid_player_add_item(cid_player_t *self, const cid_stream_t cid_stream)
{
    int ret = -1;
    int need_clear = 0;
    cid_item_t *cid_item = NULL;
    int cmd = e_cmd_stop;

    os_assert(self, "input error\n");

    if (cid_stream.behavior == e_replace_all) {
        ret = cid_player_clear_item(self);
#ifdef X86
        need_clear = 1;
#else
        if (ret != 0 && self->wiimu_shm && self->wiimu_shm->alexa_response_status == 0) {
            need_clear = 0;
        } else {
            need_clear = 1;
        }
#endif
    }

    if (cid_stream.token) {
        cid_item = (cid_item_t *)os_calloc(1, sizeof(cid_item_t));
        os_assert(cid_item, "calloc failed\n");
        cid_item->cid_stream.behavior = cid_stream.behavior;
        cid_item->cid_stream.cid_type = cid_stream.cid_type;
        cid_item->cid_stream.stream_type = cid_stream.stream_type;
        cid_item->cid_stream.dialog_id = os_strdup(cid_stream.dialog_id);
        cid_item->cid_stream.uri = os_strdup(cid_stream.uri);
        cid_item->cid_stream.token = os_strdup(cid_stream.token);
        if (cid_stream.cid_type == e_type_buffer_mp3 || cid_stream.cid_type == e_type_buffer_pcm) {
            cid_item->cache_buffer = cache_buffer_create(CACHE_BUFFER_LEN, CACHE_BUFFER_STEP);
            os_assert(cid_item->cache_buffer, "cache_buffer_create failed\n");
        }

        os_trace("token=%.200s uri=%.200s cid_type=%d stream_type=%d need_clear=%d\n",
                 cid_stream.token, cid_stream.uri, cid_stream.cid_type, cid_stream.stream_type,
                 need_clear);
        if (need_clear) {
            cmd_q_add_tail(self->play_cmd, &cmd, sizeof(int));
        } else {
            cmd_q_clear(self->play_cmd);
        }
#ifdef X86
#else
        if (self->wiimu_shm) {
            self->wiimu_shm->alexa_response_status = 1;
        }
#endif
        pthread_mutex_lock(&self->list_lock);
        lp_list_add_tail(&cid_item->cid_item, &self->item_list);
        pthread_mutex_unlock(&self->list_lock);
        ret = 0;
    }

    if (0 != ret) {
        os_trace("add item failed\n");
    }

    return ret;
}

/*
 *  brief:
 *      Write the buffer to the cid_player's cache buffer, only for buffer item
 *  input:
 *      @self:
 *          The object of the cid_player
 *      @content_id:
 *          The content id for the item, which is map to item's token
 *      @data:
 *          The buffer want to write
 *      @len:
 *          The length of buffer want to write
 *  output:
 *      @ 0   sucess
 *      @ !0  failed
 */
int cid_player_write_data(cid_player_t *self, char *content_id, char *data, int len)
{
    int write_len = 0;
    cid_item_t *item = NULL;

    if (self && content_id && data && len > 0) {
        item = cid_player_get_item(self, content_id, e_cache_write);
        if (item && item->cache_buffer) {
            write_len = cache_buffer_write(item->cache_buffer, data, len);
        } else {
            os_trace("didnot find a buffer to write tick=%lld\n", os_power_on_tick());
        }
    }

    return write_len;
}

/*
 *  brief:
 *      The buffer is EOF, set the cache buffer state is finished
 *  input:
 *      @self:
 *          The object of the cid_player
 *      @content_id:
 *          The content id for the item, which is map to item's token
 *  output:
 *      @ 0   sucess
 *      @ !0  failed
 */
int cid_player_cache_finished(cid_player_t *self, char *content_id)
{
    int ret = 0;
    cid_item_t *item = NULL;

    if (self && content_id) {
        item = cid_player_get_item(self, content_id, e_cache_write);
        if (item && item->cache_buffer) {
            ret = cache_buffer_set_state(item->cache_buffer, e_cache_finished);
        } else {
            os_trace("didnot find a buffer to write tick=%lld\n", os_power_on_tick());
        }
    }

    return ret;
}

/*
 *  brief:
 *      Stop current player
 *  input:
 *      @self:
 *          The object of the cid_player
 *      @cmd:
 *          The cmd to the player
 *  output:
 *      @ 0   sucess
 *      @ !0  failed
 */
int cid_player_set_cmd(cid_player_t *self, e_cmd_type_t cmd)
{
    int ret = -1;
    os_trace("cid_player_set_cmd, cmd=%d , tick=%lld\n", cmd, os_power_on_tick());

    os_assert(self, "self is NULL\n");
    switch (cmd) {
    case e_cmd_stop: {
        pthread_mutex_lock(&self->state_lock);
        self->is_force_stop = 1;
        pthread_mutex_unlock(&self->state_lock);
        cmd_q_add_tail(self->play_cmd, &cmd, sizeof(int));
        ret = 0;
    } break;
    case e_cmd_clear_queue: {
        pthread_mutex_lock(&self->state_lock);
        self->is_force_stop = 1;
        pthread_mutex_unlock(&self->state_lock);
        ret = cid_player_clear_item(self);
#ifdef X86
#else
        if (self->wiimu_shm && self->wiimu_shm->alexa_response_status) {
            ret = 0;
        }
#endif
        cmd = e_cmd_stop;
        cmd_q_add_tail(self->play_cmd, &cmd, sizeof(int));
    } break;
    case e_cmd_source_change: {
        int number = 0;
        pthread_mutex_lock(&self->list_lock);
        number = lp_list_number(&self->item_list);
        pthread_mutex_unlock(&self->list_lock);
        if (number > 1) {
            ret = cid_player_clear_item(self);
        }
    } break;
    case e_cmd_pause: {
        // TODO:
    } break;
    case e_cmd_resume: {
        // TODO:
    } break;
    case e_cmd_next: {
        // TODO:
    } break;
    case e_cmd_previous: {
        // TODO:
    } break;
    case e_cmd_none:
    case e_cmd_max:
    default: {

    } break;
    }

    return ret;
}
