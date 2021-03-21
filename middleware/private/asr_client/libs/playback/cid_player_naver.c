/*************************************************************************
    > File Name: cid_player_naver.c
    > Author: Keying.Zhao
    > Mail: Keying.Zhao@linkplay.com
    > Created Time: Tue 21 Nov 2017 02:35:55 AM EST
    > brief:
    >   A player for mp3/pcm buffer stream, url/file stream
 ************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <alsa/asoundlib.h>

#include "cid_player_naver.h"

#include "cmd_q.h"
#include "cache_buffer.h"
#include "mp3_decode.h"
#include "alsa_device.h"
#include "common_flags.h"

#include "lp_list.h"
#include "wm_util.h"
#include "wiimu_player_ctrol.h"

#define os_power_on_tick() tickSincePowerOn()

#ifndef StrEq
#define StrEq(str1, str2)                                                                          \
    ((str1 && str2)                                                                                \
         ? ((strlen((char *)str1) == strlen((char *)str2)) && !strcmp((char *)str1, (char *)str2)) \
         : 0)
#endif

#ifndef StrInclude
#define StrInclude(str1, str2)                                                                     \
    ((str1 && str2) ? (!strncmp((char *)str1, (char *)str2, strlen((char *)str2))) : 0)
#endif

#ifndef StrSubStr
#define StrSubStr(str1, str2) ((str1 && str2) ? (strstr((char *)str1, (char *)str2) != NULL) : 0)
#endif

#define MAX_CACHE_SIZE (200)

#define CACHE_READ (0)
#define CACHE_WRITE (1)
#define CACHE_PLAY (2)

#define WAV_RIFF COMPOSE_ID('R', 'I', 'F', 'F')
#define WAV_WAVE COMPOSE_ID('W', 'A', 'V', 'E')
#define WAV_FMT COMPOSE_ID('f', 'm', 't', ' ')
#define WAV_DATA COMPOSE_ID('d', 'a', 't', 'a')

#define WAV_FMT_PCM 0x0001

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define COMPOSE_ID(a, b, c, d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))
#define LE_SHORT(v) (v)
#define LE_INT(v) (v)
#define BE_SHORT(v) bswap_16(v)
#define BE_INT(v) bswap_32(v)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define COMPOSE_ID(a, b, c, d) ((d) | ((c) << 8) | ((b) << 16) | ((a) << 24))
#define LE_SHORT(v) bswap_16(v)
#define LE_INT(v) bswap_32(v)
#define BE_SHORT(v) (v)
#define BE_INT(v) (v)
#else
#error "Wrong endian"
#endif

#define realloc_buffer_space(buffer, len, blimit)                                                  \
    if (len > blimit) {                                                                            \
        blimit = len;                                                                              \
        buffer = realloc(buffer, blimit);                                                          \
    }

#define CID_DEBUG(fmt, ...)                                                                        \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d][CID_PLAYER] " fmt,                                  \
                (int)(os_power_on_tick() / 3600000), (int)(os_power_on_tick() % 3600000 / 60000),  \
                (int)(os_power_on_tick() % 60000 / 1000), (int)(os_power_on_tick() % 1000),        \
                ##__VA_ARGS__);                                                                    \
    } while (0)

typedef enum {
    e_cmd_none = -1,
    e_cmd_stop,
    e_cmd_pause,

    e_cmd_max,
} e_cmd_type_t;

typedef enum {
    e_state_idle,
    e_state_pause,
    e_state_stopped,
    e_state_playing,
} e_status_t;

typedef struct _cid_item_s {
    int is_active;
    char *token;
    char *url;
    cache_buffer_t *cache_buffer;
    struct lp_list_head cid_item;
} cid_item_t;

struct _cid_player_s {
    // for sound card info
    int fix_volume;
    int is_running;
    int play_inited;
    int focus;

    // for status
    int is_force_stop;
    char *cur_token;
    char *dialog_id;
    pthread_mutex_t state_lock;

    pthread_t thread_id;

    // for play buffer
    struct lp_list_head item_list;
    pthread_mutex_t list_lock;

    cmd_q_t *play_cmd;
    alsa_device_t *alsa_dev;
    mp3_decode_t *mp3_dec;
    cid_notify_cb_t *notify_cb;
    void *ctx;
    WIIMU_CONTEXT *wiimu_shm;
    unsigned long long request_id;
};

typedef struct {
    unsigned int magic;  /* 'RIFF' */
    unsigned int length; /* filelen */
    unsigned int type;   /* 'WAVE' */
} WaveHeader;

typedef struct {
    unsigned short format; /* see WAV_FMT_* */
    unsigned short channels;
    unsigned int sample_fq; /* frequence of sample */
    unsigned int byte_p_sec;
    unsigned short byte_p_spl; /* samplesize; 1 or 2 bytes */
    unsigned short bit_p_spl;  /* 8, 16 or 32 bit */
} WaveFmtBody;

typedef struct {
    unsigned short channel;
    unsigned int sample;
    int format;
} WaveInfo;

WaveInfo *g_wav = NULL;

static cid_item_t *cid_player_get_item(cid_player_t *self, char *item_id, int flags);

static int device_init(cid_player_t *self, int fmt, unsigned int *samplerate,
                       unsigned short channels)
{
    int ret = -1;

    if (self) {
        if (self->play_inited == 0) {
            if (channels == 1) {
                channels = 2;
            }

            if (self->wiimu_shm && self->wiimu_shm->asr_ongoing == 0 && !self->focus) {
                self->focus = 1;
                focus_manage_set(self->wiimu_shm, e_focus_dialogue);
            }

            ret = alsa_device_init(self->alsa_dev, fmt, samplerate, channels);
            if (ret == 0) {
                if (self->notify_cb) {
                    self->notify_cb(e_notify_started, e_cid_mp3, self->cur_token, self->ctx);
                }
                self->play_inited = 1;
                CID_DEBUG("[TICK]=========== show_first_write_ts at %lld ===========\n",
                          os_power_on_tick());
                if (self->wiimu_shm) {
                    if (self->wiimu_shm->asr_pause_request == 0) {
                        self->wiimu_shm->asr_pause_request = 1;
                    }
                    self->wiimu_shm->alexa_response_status = 1;
                }
            }
        }
    }

    return ret;
}

static int device_uninit(cid_player_t *self)
{
    int ret = 0;

    if (self && self->alsa_dev) {
        self->play_inited = 0;
        alsa_device_uninit(self->alsa_dev);
        self->alsa_dev = NULL;
    }

    return ret;
}

static int get_volume(cid_player_t *self)
{
    int volume = 0x10000;

    if (self && self->alsa_dev) {
        volume = self->fix_volume;
    }

    return volume;
}

static int read_data(cid_player_t *self, char *data, int len)
{
    int read_len = -1;
    e_cache_state_t state = e_can_read;
    cid_item_t *item_ptr = NULL;

    if (self && data && len > 0) {
        item_ptr = cid_player_get_item(self, self->cur_token, CACHE_READ);
        if (item_ptr && item_ptr->cache_buffer) {
            read_len = cache_buffer_read(item_ptr->cache_buffer, data, len, &state);
            if (read_len == 0 && e_can_read != state) {
                read_len = -1;
                if (self->notify_cb) {
                    self->notify_cb(e_notify_near_finished, e_cid_mp3, self->cur_token, self->ctx);
                }
            }
        } else {
            CID_DEBUG("did not find a buffer to read tick=%lld\n", os_power_on_tick());
        }
    }

    return read_len;
}

static int play(cid_player_t *self, unsigned short *output_ptr, int len)
{
    int err = -1;

    if (self && self->alsa_dev) {
        if (self->is_force_stop == 0) {
            err = alsa_device_play_pcm(self->alsa_dev, output_ptr, len);
        }
    } else {
        CID_DEBUG("alsa_dev not open and init tick=%lld\n", os_power_on_tick());
    }

    return err;
}

static int cid_player_list_empty(cid_player_t *self)
{
    int ret = -1;

    if (self) {
        pthread_mutex_lock(&self->list_lock);
        if (!lp_list_empty(&self->item_list)) {
            ret = 0;
        }
        pthread_mutex_unlock(&self->list_lock);
    }

    return ret;
}

static void cid_player_free_item(cid_item_t *item)
{
    if (item) {
        if (item->token) {
            free(item->token);
            item->token = NULL;
        }
        if (item->url) {
            free(item->url);
            item->url = NULL;
        }
        if (item->cache_buffer) {
            cache_buffer_destroy(&item->cache_buffer);
            item->cache_buffer = NULL;
        }
        free(item);
        CID_DEBUG("@@@@@@@@@@@@@@@@@@@@@@@ cid play free one item @@@@@@@@@@@@@@@@@@@@@@@\n");
    }
}

static void cid_player_del_item(cid_player_t *self, cid_item_t *item)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    cid_item_t *item_ptr = NULL;

    if (self) {
        pthread_mutex_lock(&self->list_lock);
        if (!lp_list_empty(&self->item_list)) {
            lp_list_for_each_safe(pos, npos, &self->item_list)
            {
                item_ptr = lp_list_entry(pos, cid_item_t, cid_item);
                if (item != NULL) {
                    if (item == item_ptr) {
                        lp_list_del(&item_ptr->cid_item);
                        cid_player_free_item(item_ptr);
                        break;
                    }
                } else {
                    if (item_ptr) {
                        lp_list_del(&item_ptr->cid_item);
                        cid_player_free_item(item_ptr);
                    }
                }
            }
        }
        CID_DEBUG("@@@@@@@@@@@@@@@@@@@@@@@ cid play list number is %d @@@@@@@@@@@@@@@@@@@@@@@\n",
                  lp_list_number(&self->item_list));
        pthread_mutex_unlock(&self->list_lock);
    }
}

static int cid_player_clear_item(cid_player_t *self)
{
    int ret = -1;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    cid_item_t *item_ptr = NULL;

    if (self) {
        pthread_mutex_lock(&self->list_lock);
        if (!lp_list_empty(&self->item_list)) {
            lp_list_for_each_safe(pos, npos, &self->item_list)
            {
                item_ptr = lp_list_entry(pos, cid_item_t, cid_item);
                if (item_ptr && item_ptr->is_active == 0) {
                    lp_list_del(&item_ptr->cid_item);
                    cid_player_free_item(item_ptr);
                }
                ret = 0;
            }
        }
        pthread_mutex_unlock(&self->list_lock);
    }

    return ret;
}

static cid_item_t *cid_player_get_item(cid_player_t *self, char *item_id, int flags)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    cid_item_t *item_ptr = NULL;

    if (self) {
        pthread_mutex_lock(&self->list_lock);
        if (!lp_list_empty(&self->item_list)) {
            lp_list_for_each_safe(pos, npos, &self->item_list)
            {
                item_ptr = lp_list_entry(pos, cid_item_t, cid_item);
                if (item_ptr) {
                    if (CACHE_WRITE == flags && StrSubStr(item_ptr->url, item_id)) {
                        break;
                    } else if (CACHE_READ == flags && StrEq(item_ptr->token, item_id) &&
                               item_ptr->is_active) {
                        break;
                    } else if (CACHE_PLAY == flags) {
                        item_ptr->is_active = 1;
                        break;
                    } else {
                        item_ptr = NULL;
                    }
                }
            }
        }
        pthread_mutex_unlock(&self->list_lock);
    }

    return item_ptr;
}

static int cid_player_mp3_play(cid_player_t *self, cid_item_t **item, e_status_t *play_state,
                               int is_force_stop)
{
    int ret = 0;
    cid_item_t *buffer_item = *item;

    if (!self || !buffer_item || !play_state) {
        ret = -1;
        goto Exit;
    }

    if (e_state_playing == *play_state) {
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
            int dec_size = -1;
            int rv = decode_start(self->mp3_dec, &dec_size);
            if (rv == MP3_DEC_RET_EXIT) {
                CID_DEBUG("----- mad_frame_decode error e_tts_stop ------\n");
                *play_state = e_state_stopped;
            } else if (rv == MP3_DEC_RET_NETSLOW) {
                // need some sleep
                usleep(20000);
            } else if (rv == MP3_DEC_RET_OK) {
                if (self && self->alsa_dev == NULL) {
                    self->alsa_dev = alsa_device_open(DEFAULT_DEV);
                }
                // play audio frame
                if (decode_play(self->mp3_dec) < 0) {
                    CID_DEBUG(
                        "decode_play eror tick=%lld self->is_force_stop=%d is_force_stop=%d\n",
                        os_power_on_tick(), self->is_force_stop, is_force_stop);
                    device_uninit(self);
                } else {
                    usleep(5000);
                }
                if (self->is_force_stop || is_force_stop) {
                    *play_state = e_state_stopped;
                }
            }
        } else {
            *play_state = e_state_stopped;
        }

    } else if (e_state_stopped == *play_state) {
        // error, exit play
        if (buffer_item) {
            cid_player_del_item(self, buffer_item);
            *item = NULL;
        }
        if (self->mp3_dec) {
            decode_uninit(self->mp3_dec);
            self->mp3_dec = NULL;
            if (self->notify_cb) {
                if (self->is_force_stop || is_force_stop) {
                    self->notify_cb(e_notify_stopped, e_cid_mp3, self->cur_token, self->ctx);
                } else {
                    self->notify_cb(e_notify_finished, e_cid_mp3, self->cur_token, self->ctx);
                }
                if (0 != cid_player_list_empty(self)) {
                    if (self->wiimu_shm) {
                        self->wiimu_shm->alexa_response_status = 0;
                        self->wiimu_shm->asr_pause_request = 0;
                    }
                    usleep(50000);
                    self->notify_cb(e_notify_end, e_cid_mp3, NULL, self->ctx);
                    self->focus = 0;
                    if ((self->wiimu_shm->snd_enable[1] == SNDSRC_ALERT) &&
                        self->wiimu_shm->alert_status) {
                        apply_alert_volume();
                    }
                    focus_manage_clear(self->wiimu_shm, e_focus_dialogue);
                    asr_notification_resume();
                }
            }
        }
        if (self->cur_token) {
            free(self->cur_token);
            self->cur_token = NULL;
        }
        CID_DEBUG("----- CID tts play ending ------\n");
        *play_state = e_state_idle;
    } else {
        usleep(20000);
    }

Exit:

    return ret;
}

static int cid_player_url_play(cid_player_t *self, cid_item_t **item, e_status_t *play_state,
                               int is_force_stop)
{
    int ret = 0;
    cid_item_t *buffer_item = *item;

    if (!self || !buffer_item || !play_state) {
        ret = -1;
        goto Exit;
    }

    if (e_state_playing == *play_state) {
        ret = wiimu_player_get_play_url_state(e_stream_type_speak);
        if (ret == e_status_ex_exit || ret == -1 || self->is_force_stop || is_force_stop) {
            *play_state = e_state_stopped;
        } else {
            if (self->play_inited == 0) {
                self->play_inited = 1;
                if (self->notify_cb) {
                    self->notify_cb(e_notify_started, e_cid_url, self->cur_token, self->ctx);
                }
            }
            *play_state = e_state_playing;
            usleep(10000);
        }
    } else if (e_state_stopped == *play_state) {
        wiimu_player_stop_play_url(e_stream_type_speak);
        // error, exit play
        if (buffer_item) {
            cid_player_del_item(self, buffer_item);
            *item = NULL;
        }
        self->play_inited = 0;
        if (self->notify_cb) {
            if (self->is_force_stop || is_force_stop) {
                self->notify_cb(e_notify_stopped, e_cid_url, self->cur_token, self->ctx);
            } else {
                self->notify_cb(e_notify_finished, e_cid_url, self->cur_token, self->ctx);
            }
            self->focus = 0;
            if (0 != cid_player_list_empty(self)) {
                if (self->wiimu_shm) {
                    self->wiimu_shm->alexa_response_status = 0;
                    self->wiimu_shm->asr_pause_request = 0;
                }
                self->notify_cb(e_notify_end, e_cid_url, NULL, self->ctx);
                if ((self->wiimu_shm->snd_enable[1] == SNDSRC_ALERT) &&
                    self->wiimu_shm->alert_status) {
                    apply_alert_volume();
                }
                focus_manage_clear(self->wiimu_shm, e_focus_dialogue);
            }
        }

        if (self->cur_token) {
            free(self->cur_token);
            self->cur_token = NULL;
        }
        CID_DEBUG("----- URL tts play ending ------\n");
        *play_state = e_state_idle;
    } else {
        usleep(20000);
    }

Exit:

    return ret;
}

static int cid_player_pcm_init(cid_player_t *self)
{
    int ret = -1;
    if (self) {
        unsigned int samplerate = 22050;
        ret = alsa_device_init(self->alsa_dev, SND_PCM_FORMAT_S16_LE, &samplerate, 1);
        if (ret == 0) {
            if (self->notify_cb) {
                self->notify_cb(e_notify_started, e_cid_pcm, self->cur_token, self->ctx);
            }
            self->play_inited = 1;
            CID_DEBUG("[TICK]=========== show_first_write_ts at %lld ===========\n",
                      os_power_on_tick());
            if (self->wiimu_shm) {
                if (self->wiimu_shm->asr_pause_request == 0) {
                    self->wiimu_shm->asr_pause_request = 1;
                }
                self->wiimu_shm->alexa_response_status = 1;
            }
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

    if (self) {
        item_ptr = cid_player_get_item(self, self->cur_token, CACHE_READ);
        if (item_ptr && item_ptr->cache_buffer) {
            read_len = cache_buffer_read(item_ptr->cache_buffer, buffer, sizeof(buffer), &state);
            if (read_len == 0 && e_can_read != state) {
                read_len = -1;
                if (self->notify_cb) {
                    self->notify_cb(e_notify_near_finished, e_cid_pcm, self->cur_token, self->ctx);
                }
            } else {
                if (read_len > 0) {
                    play(self, (unsigned short *)buffer, read_len / 2);
                }
            }
        } else {
            CID_DEBUG("did not find a buffer to read tick=%lld\n", os_power_on_tick());
        }
    }

    return read_len;
}

static int cid_player_pcm_play(cid_player_t *self, cid_item_t **item, e_status_t *play_state,
                               int is_force_stop)
{
    int ret = 0;
    cid_item_t *buffer_item = *item;

    if (!self || !buffer_item || !play_state) {
        ret = -1;
        goto Exit;
    }

    if (e_state_playing == *play_state) {
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
            if (ret == -1 || self->is_force_stop || is_force_stop) {
                *play_state = e_state_stopped;
                usleep(5000);
            }
        } else {
            CID_DEBUG("alsa_device_open eror tick=%lld\n", os_power_on_tick());
            *play_state = e_state_stopped;
            usleep(5000);
        }
    } else if (e_state_stopped == *play_state) {
        // error, exit play
        if (buffer_item) {
            cid_player_del_item(self, buffer_item);
            *item = NULL;
        }
        self->play_inited = 0;
        if (self->alsa_dev) {
            alsa_device_uninit(self->alsa_dev);
            self->alsa_dev = NULL;
            if (self->notify_cb) {
                if (self->is_force_stop || is_force_stop) {
                    self->notify_cb(e_notify_stopped, e_cid_pcm, self->cur_token, self->ctx);
                } else {
                    self->notify_cb(e_notify_finished, e_cid_pcm, self->cur_token, self->ctx);
                }
                if (0 != cid_player_list_empty(self)) {
                    if (self->wiimu_shm) {
                        self->wiimu_shm->alexa_response_status = 0;
                        self->wiimu_shm->asr_pause_request = 0;
                    }
                    self->notify_cb(e_notify_end, e_cid_pcm, NULL, self->ctx);
                    focus_manage_clear(self->wiimu_shm, e_focus_dialogue);
                }
            }
        }
        if (self->cur_token) {
            free(self->cur_token);
            self->cur_token = NULL;
        }
        CID_DEBUG("----- CID tts play ending ------\n");
        *play_state = e_state_idle;
    } else {
        usleep(20000);
    }

Exit:

    return ret;
}

static void MonoToStereo(char *buffer, unsigned int len)
{
    int i;

    for (i = len / 2 - 1; i > 0; i--) {
        buffer[4 * i] = buffer[2 * i];
        buffer[4 * i + 1] = buffer[2 * i + 1];
    }

    for (i = 0; i < len / 2; i++) {
        buffer[4 * i + 2] = buffer[4 * i];
        buffer[4 * i + 3] = buffer[4 * i + 1];
    }
}

static int cid_player_wav_info(cid_player_t *self, WaveInfo *wav_info)
{
    int ret = -1;
    int read_len = -1;
    unsigned int blimit = 0;
    unsigned int *tmp;
    char *buffer = NULL;
    e_cache_state_t state = e_can_read;
    cid_item_t *item_ptr = NULL;
    WaveHeader *header = NULL;
    WaveFmtBody *fmt = NULL;

    CID_DEBUG("%s ++\n", __func__);
    item_ptr = cid_player_get_item(self, self->cur_token, CACHE_READ);
    if (self && wav_info && item_ptr && item_ptr->cache_buffer) {
        buffer = (char *)calloc(1, sizeof(WaveHeader));
        if (!buffer) {
            CID_DEBUG("malloc error at line %d\n", __LINE__);
            goto EXIT;
        }

        blimit = sizeof(WaveHeader);
        read_len = cache_buffer_read(item_ptr->cache_buffer, buffer, sizeof(WaveHeader), &state);
        if (read_len == sizeof(WaveHeader)) {
            header = (WaveHeader *)buffer;
            if (header->magic != WAV_RIFF || header->type != WAV_WAVE) {
                CID_DEBUG("wav header error!\n");
                goto EXIT;
            }

            CID_DEBUG("find header!\n");
            realloc_buffer_space(buffer, sizeof(unsigned int), blimit);
            if (!buffer) {
                CID_DEBUG("malloc error at line %d\n", __LINE__);
                goto EXIT;
            }
        } else {
            CID_DEBUG("read data error at line %d\n", __LINE__);
            goto EXIT;
        }

        while (1) {
            read_len =
                cache_buffer_read(item_ptr->cache_buffer, buffer, sizeof(unsigned int), &state);
            if (read_len == sizeof(unsigned int)) {
                tmp = (unsigned int *)buffer;
                if (*tmp == WAV_FMT) {
                    CID_DEBUG("find fmt!\n");
                    read_len = cache_buffer_read(item_ptr->cache_buffer, buffer,
                                                 sizeof(unsigned int), &state);
                    if (read_len == sizeof(unsigned int)) {
                        break;
                    } else {
                        CID_DEBUG("read data error at line %d\n", __LINE__);
                        goto EXIT;
                    }
                }
            } else {
                CID_DEBUG("read data error at line %d\n", __LINE__);
                goto EXIT;
            }
        }

        realloc_buffer_space(buffer, sizeof(WaveFmtBody), blimit);
        if (!buffer) {
            CID_DEBUG("malloc error at line %d\n", __LINE__);
            goto EXIT;
        }

        CID_DEBUG("parse fmt!\n");
        read_len = cache_buffer_read(item_ptr->cache_buffer, buffer, sizeof(WaveFmtBody), &state);
        if (read_len == sizeof(WaveFmtBody)) {
            fmt = (WaveFmtBody *)buffer;
            if (LE_SHORT(fmt->format) != WAV_FMT_PCM) {
                CID_DEBUG("is not pcm data!\n");
                goto EXIT;
            }
            wav_info->channel = LE_SHORT(fmt->channels);
            switch (LE_SHORT(fmt->byte_p_spl)) {
            case 16:
                wav_info->format = SND_PCM_FORMAT_S16_LE;
            default:
                wav_info->format = SND_PCM_FORMAT_S16_LE;
            }
            wav_info->sample = LE_INT(fmt->sample_fq);
        } else {
            CID_DEBUG("read data error at line %d\n", __LINE__);
            goto EXIT;
        }

        realloc_buffer_space(buffer, sizeof(unsigned int), blimit);
        if (!buffer) {
            CID_DEBUG("malloc error at line %d\n", __LINE__);
            goto EXIT;
        }

        while (1) {
            read_len =
                cache_buffer_read(item_ptr->cache_buffer, buffer, sizeof(unsigned int), &state);
            if (read_len == sizeof(unsigned int)) {
                tmp = (unsigned int *)buffer;
                if (*tmp == WAV_DATA) {
                    CID_DEBUG("find data!\n");
                    read_len = cache_buffer_read(item_ptr->cache_buffer, buffer,
                                                 sizeof(unsigned int), &state);
                    if (read_len == sizeof(unsigned int)) {
                        ret = 0;
                        break;
                    } else {
                        CID_DEBUG("read data error at line %d\n", __LINE__);
                        goto EXIT;
                    }
                }
            } else {
                CID_DEBUG("read data error at line %d\n", __LINE__);
                goto EXIT;
            }
        }
    }

EXIT:

    if (buffer) {
        free(buffer);
        buffer = NULL;
    }

    CID_DEBUG("%s --\n", __func__);

    return ret;
}

static int cid_player_wav_init(cid_player_t *self)
{
    int ret = -1;

    if (self) {
        if (self->wiimu_shm && self->wiimu_shm->asr_ongoing == 0 && !self->focus) {
            self->focus = 1;
            focus_manage_set(self->wiimu_shm, e_focus_dialogue);
        }

        if (!g_wav) {
            g_wav = (WaveInfo *)calloc(1, sizeof(WaveInfo));
        }

        if (g_wav) {
            ret = cid_player_wav_info(self, g_wav);
            if (!ret) {
                g_wav->channel = g_wav->channel == 2 ? g_wav->channel : 2;
                ret =
                    alsa_device_init(self->alsa_dev, g_wav->format, &g_wav->sample, g_wav->channel);
                if (!ret) {
                    if (self->notify_cb) {
                        self->notify_cb(e_notify_started, e_cid_wav, self->cur_token, self->ctx);
                    }
                    self->play_inited = 1;
                    CID_DEBUG("[TICK]=========== show_first_write_ts at %lld ===========\n",
                              os_power_on_tick());
                    if (self->wiimu_shm) {
                        if (self->wiimu_shm->asr_pause_request == 0) {
                            self->wiimu_shm->asr_pause_request = 1;
                        }
                        self->wiimu_shm->alexa_response_status = 1;
                    }
                    alsa_device_play_silence(self->alsa_dev, 450);
                }
            }
        }
    }

    return ret;
}

static int cid_player_play_wav(cid_player_t *self)
{
    int read_len = -1;
    int tmp;
    char buffer[8192] = {0};
    e_cache_state_t state = e_can_read;
    cid_item_t *item_ptr = NULL;

    if (self) {
        item_ptr = cid_player_get_item(self, self->cur_token, CACHE_READ);
        if (item_ptr && item_ptr->cache_buffer) {
            read_len =
                cache_buffer_read(item_ptr->cache_buffer, buffer, sizeof(buffer) / 2, &state);
            if (read_len == 0 && e_can_read != state) {
                read_len = -1;
                if (self->notify_cb) {
                    self->notify_cb(e_notify_near_finished, e_cid_wav, self->cur_token, self->ctx);
                }
            } else {
                if (read_len > 0) {
                    MonoToStereo(buffer, read_len);
                    tmp = g_wav->channel * g_wav->format;
                    play(self, (unsigned short *)buffer, read_len * 2 / tmp);
                }
            }
        } else {
            CID_DEBUG("did not find a buffer to read tick=%lld\n", os_power_on_tick());
        }
    }

    return read_len;
}

static int cid_player_wav_play(cid_player_t *self, cid_item_t **item, e_status_t *play_state,
                               int is_force_stop)
{
    int ret = 0;
    cid_item_t *buffer_item = *item;

    if (!self || !buffer_item || !play_state) {
        ret = -1;
        goto Exit;
    }

    if (e_state_playing == *play_state) {
        if (self->alsa_dev == NULL) {
            self->alsa_dev = alsa_device_open(DEFAULT_DEV);
        }
        if (self->alsa_dev) {
            if (self->play_inited == 0) {
                ret = cid_player_wav_init(self);
            }
            if (self->play_inited == 1) {
                ret = cid_player_play_wav(self);
                if (ret == 0) {
                    usleep(5000);
                }
            }
            if (ret == -1 || self->is_force_stop || is_force_stop) {
                *play_state = e_state_stopped;
                usleep(5000);
            }
        } else {
            CID_DEBUG("alsa_device_open eror tick=%lld\n", os_power_on_tick());
            *play_state = e_state_stopped;
            usleep(5000);
        }
    } else if (e_state_stopped == *play_state) {
        // error, exit play
        if (buffer_item) {
            cid_player_del_item(self, buffer_item);
            *item = NULL;
        }
        self->play_inited = 0;
        if (self->alsa_dev) {
            alsa_device_uninit(self->alsa_dev);
            self->alsa_dev = NULL;
            if (self->notify_cb) {
                if (self->is_force_stop || is_force_stop) {
                    self->notify_cb(e_notify_stopped, e_cid_wav, self->cur_token, self->ctx);
                } else {
                    self->notify_cb(e_notify_finished, e_cid_wav, self->cur_token, self->ctx);
                }
                if (0 != cid_player_list_empty(self)) {
                    if (self->wiimu_shm) {
                        self->wiimu_shm->alexa_response_status = 0;
                        self->wiimu_shm->asr_pause_request = 0;
                    }
                    self->notify_cb(e_notify_end, e_cid_wav, NULL, self->ctx);
                    self->focus = 0;
                    focus_manage_clear(self->wiimu_shm, e_focus_dialogue);
                }
            }
        }
        if (self->cur_token) {
            free(self->cur_token);
            self->cur_token = NULL;
        }
        CID_DEBUG("----- CID tts play ending ------\n");
        *play_state = e_state_idle;
    } else {
        usleep(20000);
    }

Exit:

    return ret;
}

void *cid_player_run(void *args)
{
    cid_player_t *self = (cid_player_t *)args;
    if (self) {
        int cid_type = 0;
        cid_item_t *item = NULL;
        self->is_running = 1;
        e_status_t play_state = e_state_idle;

        while (self->is_running == 1) {
            int is_force_stop = 0;
            int *cmd_type = NULL;

            if (NULL == item) {
                item = cid_player_get_item(self, NULL, CACHE_PLAY);
                if (item && item->token) {
                    if (self->cur_token) {
                        free(self->cur_token);
                        self->cur_token = NULL;
                    }
                    self->cur_token = strdup(item->token);
                    play_state = e_state_playing;
                    if (self->is_force_stop) {
                        self->is_force_stop = 0;
                    }
                    if (StrInclude(item->url, "http") || StrInclude(item->url, "HTTP")) {
                        cid_type = e_cid_url;
                        wiimu_player_start_play_url(e_stream_type_speak, item->url, 0);
                        usleep(10000);
                    } else if (StrInclude(item->url, "cid")) {
                        cid_type = cache_buffer_get_type(item->cache_buffer);
                    }
                }
            }

            cmd_type = (int *)cmd_q_pop(self->play_cmd);
            if (cmd_type) {
                pthread_mutex_lock(&self->state_lock);
                if (e_cmd_stop == *cmd_type) {
                    if (item != NULL) {
                        play_state = e_state_stopped;
                        if (self->is_force_stop) {
                            self->is_force_stop = 0;
                            is_force_stop = 1;
                        }
                    }
                } else if (e_cmd_pause == *cmd_type) {
                    if (self->is_force_stop) {
                        self->is_force_stop = 0;
                    }
                    play_state = e_state_pause;
                }
                pthread_mutex_unlock(&self->state_lock);
                free(cmd_type);
            }

            if (item) {
                if (cid_type == e_cid_mp3) {
                    cid_player_mp3_play(self, &item, &play_state, is_force_stop);
                } else if (cid_type == e_cid_url) {
                    cid_player_url_play(self, &item, &play_state, is_force_stop);
                } else if (cid_type == e_cid_wav) {
                    cid_player_wav_play(self, &item, &play_state, is_force_stop);
                } else {
                    cid_player_del_item(self, item);
                    item = NULL;
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
        if (self->wiimu_shm) {
            self->wiimu_shm->alexa_response_status = 0;
        }
        CID_DEBUG("Audio Processor stopped !\n");
    }

    return NULL;
}

static int cid_player_init(cid_player_t *self)
{
    int ret = -1;
    pthread_attr_t attr;
    struct sched_param param;

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
        CID_DEBUG("cid_player_run create faield!\n");
        self->thread_id = 0;
    }
    pthread_attr_destroy(&attr);

    return ret;
}

cid_player_t *cid_player_create(cid_notify_cb_t *notify_cb, void *ctx)
{
    int ret = -1;

    cid_player_t *self = (cid_player_t *)calloc(1, sizeof(cid_player_t));
    if (self) {
        self->notify_cb = notify_cb;
        self->ctx = ctx;
        self->wiimu_shm = (WIIMU_CONTEXT *)common_flags_get();
        ret = cid_player_init(self);
        if (ret != 0) {
            cid_player_destroy(&self);
            self = NULL;
        }
    }

    return self;
}

void cid_player_destroy(cid_player_t **self_p)
{
    cid_player_t *self = *self_p;

    if (self) {
        cid_player_stop(self, 1, 0);
        self->is_running = 0;
        if (self->thread_id > 0) {
            pthread_join(self->thread_id, NULL);
        }
        cmd_q_uninit(&self->play_cmd);
        cid_player_del_item(self, NULL);
        pthread_mutex_lock(&self->state_lock);
        if (self->cur_token) {
            free(self->cur_token);
            self->cur_token = NULL;
        }
        if (self->dialog_id) {
            free(self->dialog_id);
            self->dialog_id = NULL;
        }
        pthread_mutex_unlock(&self->state_lock);
        pthread_mutex_destroy(&self->state_lock);
        free(self);
        *self_p = NULL;
    }
}

int cid_player_add_item(cid_player_t *self, char *dialog_id, char *token, char *url,
                        unsigned long long request_id)
{
    int ret = -1;
    int need_clear = 0;
    int url_play = 0;
    cid_item_t *cid_item = NULL;

    if (self) {
        pthread_mutex_lock(&self->state_lock);
        CID_DEBUG("request_id=%llu old_request_id=%llu\n", request_id, self->request_id);
        if (request_id != self->request_id) {
            need_clear = 1;
        }
        self->request_id = request_id;
        pthread_mutex_unlock(&self->state_lock);
        if (need_clear) {
            ret = cid_player_clear_item(self);
            if (ret != 0 || (self->wiimu_shm && self->wiimu_shm->alexa_response_status == 0)) {
                if (self->wiimu_shm) {
                    CID_DEBUG("alexa_response_status=%d\n", self->wiimu_shm->alexa_response_status);
                }
                need_clear = 0;
            }
        }
    }

    if (self && token && url) {
        pthread_mutex_lock(&self->list_lock);
        if (lp_list_number(&self->item_list) > MAX_CACHE_SIZE) {
            pthread_mutex_unlock(&self->list_lock);
            CID_DEBUG("!!!!!!! The cid queue's size is > %d !!!!!!!!\n", MAX_CACHE_SIZE);
            goto exit;
        }
        pthread_mutex_unlock(&self->list_lock);

        cid_item = (cid_item_t *)calloc(1, sizeof(cid_item_t));
        if (cid_item) {
            if (StrInclude(url, "http") || StrInclude(url, "HTTP")) {
                ret = 0;
            } else {
                cid_item->cache_buffer = cache_buffer_create(CACHE_BUFFER_LEN, CACHE_BUFFER_STEP);
                if (cid_item->cache_buffer) {
                    ret = 0;
                } else {
                    ret = -1;
                }
            }
            if (ret == 0) {
                cid_item->token = strdup(token);
                cid_item->url = strdup(url);
                CID_DEBUG("token=%s url=%s need_clear=%d\n", cid_item->token, cid_item->url,
                          need_clear);
                if (need_clear) {
                    pthread_mutex_lock(&self->state_lock);
                    self->is_force_stop = 1;
                    if (self->wiimu_shm && self->wiimu_shm->alexa_response_status &&
                        self->cur_token) {
                        wiimu_player_stop_play_url(e_stream_type_speak);
                    }
                    pthread_mutex_unlock(&self->state_lock);
                } else {
                    cmd_q_clear(self->play_cmd);
                }
                if (self->wiimu_shm) {
                    self->wiimu_shm->alexa_response_status = 1;
                }
                pthread_mutex_lock(&self->list_lock);
                lp_list_add_tail(&cid_item->cid_item, &self->item_list);
                pthread_mutex_unlock(&self->list_lock);
            } else {
                free(cid_item);
            }
        }
    }

exit:

    if (0 != ret) {
        CID_DEBUG("add item failed\n");
    }

    return ret;
}

int cid_player_write_data(cid_player_t *self, char *content_id, char *data, int len)
{
    int write_len = 0;
    cid_item_t *item = NULL;

    if (self && content_id && data && len > 0) {
        item = cid_player_get_item(self, content_id, CACHE_WRITE);
        if (item && item->cache_buffer) {
            write_len = cache_buffer_write(item->cache_buffer, data, len);
        } else {
            CID_DEBUG("did not find a buffer to write tick=%lld\n", os_power_on_tick());
        }
    }

    return write_len;
}

int cid_player_cache_finished(cid_player_t *self, char *content_id)
{
    int ret = 0;
    cid_item_t *item = NULL;

    if (self && content_id) {
        item = cid_player_get_item(self, content_id, CACHE_WRITE);
        if (item && item->cache_buffer) {
            ret = cache_buffer_set_state(item->cache_buffer, e_cache_finished);
        } else {
            CID_DEBUG("did not find a buffer to write tick=%lld\n", os_power_on_tick());
        }
    }

    return ret;
}

int cid_player_stop(cid_player_t *self, int clear_all, unsigned long long request_id)
{
    int need_exec = 0;
    int ret = -1;
    int cmd = -1;
    CID_DEBUG("cid_player_stop, clear_all=%d request_id=%llu, tick=%lld\n", clear_all, request_id,
              os_power_on_tick());

    if (self) {
        pthread_mutex_lock(&self->state_lock);
        if (request_id != self->request_id) {
            need_exec = 1;
        }
        if (need_exec) {
            self->is_force_stop = 1;
        }
        pthread_mutex_unlock(&self->state_lock);
        if (need_exec) {
            cmd = e_cmd_stop;
            if (0 == clear_all) {
                ret = 0;
            } else {
                ret = cid_player_clear_item(self);
                if (self->wiimu_shm && self->wiimu_shm->alexa_response_status) {
                    ret = 0;
                }
            }
            if (cmd > e_cmd_none) {
                cmd_q_add_tail(self->play_cmd, &cmd, sizeof(int));
            }
        }
    }

    return ret;
}

// This function will get mplayer quit state, only for Naver SpeechSynthesizer.Speak with url.
int cid_player_quit_state(cid_player_t *self)
{
    if (self)
        return self->wiimu_shm->imuzo_error_ex[e_stream_type_speak];

    return 0;
}
