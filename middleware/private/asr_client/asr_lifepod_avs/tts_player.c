
#include <mad.h>
#include <pthread.h>
#include <semaphore.h>
#include <alsa/asoundlib.h>

#include "tts_player.h"

#include "cmd_q.h"
#include "wm_util.h"
#include "cxdish_ctrol.h"

#include "alexa.h"
#include "alexa_cmd.h"
#include "alexa_debug.h"
#include "alexa_json_common.h"
#include "ring_buffer.h"
#include "asr_state.h"

#ifdef AVS_FOCUS_MGMT
#include "alexa_focus_mgmt.h"
#endif

#if defined(SOFT_EQ_DSPC)
#define ALSA_TTS_DEVICE ("postout_softvol")
#else
#define ALSA_TTS_DEVICE ("default")
#endif

#undef RING_BUFFER_LEN
#define RING_BUFFER_LEN (256 * 1024)

#define OUTPUT_BUFFER_SIZE (8192)
#define INPUT_BUFFER_SIZE (8192)

extern WIIMU_CONTEXT *g_wiimu_shm;
extern long long tts_end_tick;

#define os_power_on_tick() tickSincePowerOn()

#if 0
#define TTS_DEBUG(fmt, ...)                                                                        \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d][TTS Player %s:%d] " fmt,                            \
                (int)(os_power_on_tick() / 3600000), (int)(os_power_on_tick() % 3600000 / 60000),  \
                (int)(os_power_on_tick() % 60000 / 1000), (int)(os_power_on_tick() % 1000),        \
                __FUNCTION__, __LINE__, ##__VA_ARGS__);                                            \
    } while (0)
#else
#define TTS_DEBUG(fmt, ...)                                                                        \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d][TTSPlayer] " fmt,                                   \
                (int)(os_power_on_tick() / 3600000), (int)(os_power_on_tick() % 3600000 / 60000),  \
                (int)(os_power_on_tick() % 60000 / 1000), (int)(os_power_on_tick() % 1000),        \
                ##__VA_ARGS__);                                                                    \
    } while (0)
#endif

const char *g_cmd_str[] = {
    "IDLE", "PLAY", "STOP", "PAUSE",
};

enum {
    e_cmd_idle,
    e_cmd_play,
    e_cmd_stop,
    e_cmd_pause,
};

struct _tts_player_s {
    // for sound card info
    int fmt;
    int sample_rate;
    int channels;
    int fix_volume;
    snd_pcm_t *snd_handle;

    // for status
    int play_status;
    int is_running;
    int is_force_stop;
    int first_pcm_play;
    int tlk_started;
    int cache_finish[2];
    int tlk_continue;
    int write_index;
    int read_index;
    int buf_cached;
    pthread_mutex_t state_lock;

    pthread_t thread_id;

    // for play buffer
    ring_buffer_t *play_buffer[2];
    cmd_q_t *play_cmd;

    int type;
    int is_next_cache;
    int cid_continue;
    char *cur_token[2];
    char *back_token;

    int play_paused;
    int alsa_inited;

    unsigned char fadeBuf[2][8192];
    unsigned int fadeDataSize[2];
    int fadeNextIndex;
    int fadeCached;
};

typedef struct _mp3_decode_s {
    struct mad_stream stream;
    struct mad_frame frame;
    struct mad_synth synth;
    char input_buf[INPUT_BUFFER_SIZE];
} mp3_decode_t;

static int ttsplayer_read_data(tts_player_t *self, char *data, int len);
static int device_close(tts_player_t *self);

static short lcg_rand(void)
{
    static unsigned long lcg_prev = 12345;
    lcg_prev = lcg_prev * 69069 + 3;
    return lcg_prev & 0xffff;
}

static inline short dithered_vol(short sample, int fix_volume)
{
    static short rand_a, rand_b;
    long out;

    out = (long)sample * fix_volume;
    if (fix_volume < 0x10000) {
        rand_b = rand_a;
        rand_a = lcg_rand();
        out += rand_a;
        out -= rand_b;
    }

    return out >> 16;
}

static int scale(mad_fixed_t sample)
{
    sample += (1L << (MAD_F_FRACBITS - 16));
    if (sample >= MAD_F_ONE)
        sample = MAD_F_ONE - 1;
    else if (sample < -MAD_F_ONE)
        sample = -MAD_F_ONE;

    return sample >> (MAD_F_FRACBITS + 1 - 16);
}

static int device_open(tts_player_t *self)
{
    int ret = -1;

    if (self && self->snd_handle == NULL) {
        TTS_DEBUG("open device start ++++\n");
        ret = snd_pcm_open(&self->snd_handle, ALSA_TTS_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
        if (ret != 0) {
            TTS_DEBUG("open PCM device failed  %s !\n", snd_strerror(ret));
            self->snd_handle = NULL;
        } else {
            // set the sound card to noblock to play
            snd_pcm_info_set(self->snd_handle, SNDSRC_ALEXA_TTS);
            snd_pcm_nonblock(self->snd_handle, 0);
            // self->fadeNextIndex = 0;
            // self->fadeCached = 0;
            // memset(self->fadeBuf, 0, sizeof(self->fadeBuf));
            // memset(self->fadeDataSize, 0, sizeof(self->fadeDataSize));
            self->alsa_inited = 0;
            TTS_DEBUG("open device success\n");
        }
    }

    return ret;
}

static int device_init(tts_player_t *self, int fmt, unsigned int *samplerate,
                       unsigned short channels)
{
    int ret = 0;
    int dir = 0;
    snd_pcm_uframes_t frames = 2048;
    snd_pcm_hw_params_t *pcm_params = NULL;
    snd_pcm_sw_params_t *pcm_params_sw = NULL;

#if defined(HARMAN)
    // HM Plugin reinit will cause pop issue when rate is different
    if (g_wiimu_shm->alexa_response_status && self->sample_rate != *samplerate) {
        device_close(self);
        device_open(self);
        if (self->snd_handle == NULL)
            return -1;
    }
#endif

    if (self && self->snd_handle) {
        if (channels == 1) {
            channels = 2;
        }
        if (self->fmt == fmt && self->sample_rate == (int)*samplerate &&
            self->channels == (int)channels && self->alsa_inited) {
            return 0;
        }

        TTS_DEBUG("device_init begin at channels(%d) ticks=%lld\n", channels, os_power_on_tick());
        snd_pcm_hw_params_alloca(&pcm_params);
        ret = snd_pcm_hw_params_any(self->snd_handle, pcm_params);
        if (ret < 0) {
            TTS_DEBUG("snd_pcm_hw_params_any error: %s !\n", snd_strerror(ret));
            return ret;
        }

        ret = snd_pcm_hw_params_set_access(self->snd_handle, pcm_params,
                                           SND_PCM_ACCESS_RW_INTERLEAVED);
        if (ret < 0) {
            TTS_DEBUG("snd_pcm_params_set_access error: %s !\n", snd_strerror(ret));
            return ret;
        }

        ret = snd_pcm_hw_params_set_format(self->snd_handle, pcm_params, (snd_pcm_format_t)fmt);
        if (ret < 0) {
            TTS_DEBUG("snd_pcm_params_set_format error: %s !\n", snd_strerror(ret));
            return ret;
        }
        self->fmt = fmt;

        ret = snd_pcm_hw_params_set_channels(self->snd_handle, pcm_params, channels);
        if (ret < 0) {
            TTS_DEBUG("snd_pcm_params_set_channels error: %s !\n", snd_strerror(ret));
            return ret;
        }
        self->channels = channels;

        ret = snd_pcm_hw_params_set_rate_near(self->snd_handle, pcm_params, samplerate, &dir);
        if (ret < 0) {
            TTS_DEBUG("snd_pcm_hw_params_set_rate_near error: %s !\n", snd_strerror(ret));
            return ret;
        }
        self->sample_rate = *samplerate;

        ret = snd_pcm_hw_params(self->snd_handle, pcm_params);
        if (ret < 0) {
            TTS_DEBUG("snd_pcm_hw_params error: %s !\n", snd_strerror(ret));
            return ret;
        }

        snd_pcm_sw_params_alloca(&pcm_params_sw);
        /* set software parameters */
        ret = snd_pcm_sw_params_current(self->snd_handle, pcm_params_sw);
        if (ret < 0) {
            TTS_DEBUG("unable to determine current software params: %s !", snd_strerror(ret));
        }

        ret = snd_pcm_hw_params_get_period_size(pcm_params, &frames, NULL);
        if (ret < 0) {
            TTS_DEBUG("unable set start threshold: %s !", snd_strerror(ret));
        }

        ret = snd_pcm_sw_params_set_start_threshold(self->snd_handle, pcm_params_sw, frames * 2);
        if (ret < 0) {
            TTS_DEBUG("unable set start threshold: %s !", snd_strerror(ret));
        }

        ret = snd_pcm_sw_params_set_silence_size(self->snd_handle, pcm_params_sw, 4);
        if (ret < 0) {
            TTS_DEBUG("unable set start threshold: %s !", snd_strerror(ret));
        }

        ret = snd_pcm_sw_params(self->snd_handle, pcm_params_sw);
        if (ret < 0) {
            TTS_DEBUG("unable set sw params: %s !", snd_strerror(ret));
        }
        // snd_pcm_mix_vol(self->snd_handle, 0, 0, 0);
        snd_pcm_start(self->snd_handle);

        self->alsa_inited = 1;
        TTS_DEBUG("device_init end at ticks=%lld\n", os_power_on_tick());
    }

    return 0;
}

static int device_close(tts_player_t *self)
{
    int ret = 0;

    if (self && self->snd_handle) {
        TTS_DEBUG("device_close start at ticks=%lld\n", os_power_on_tick());
        snd_pcm_drain(self->snd_handle);
        // snd_pcm_mix_vol(self->snd_handle, 0, 0, 0);
        snd_pcm_close(self->snd_handle);
        TTS_DEBUG("device_close at end ticks=%lld\n", os_power_on_tick());
        self->snd_handle = NULL;
    }

    return ret;
}

static int device_clear_buf(tts_player_t *self)
{
    int ret = 0;

    if (self && self->snd_handle) {
        snd_pcm_drop(self->snd_handle);
        snd_pcm_prepare(self->snd_handle);
    }
    return ret;
}

void play_silence(tts_player_t *self, int time_ms, int sample_rate)
{
    int err = 0;
    int period_size = 8192;
    int silent_size = time_ms * sample_rate * 4 / 1000;
    int frames = silent_size / 4;
    int oneshot = 0;
    int offset = 0;
    char *silent = (char *)malloc(silent_size);

    memset(silent, 0, silent_size);

    while (frames > 0 && self->is_force_stop == 0 && self->tlk_started == 0) {
        if (frames > period_size) {
            oneshot = period_size;
        } else {
            oneshot = frames;
        }
        err = snd_pcm_writei(self->snd_handle, silent + offset, oneshot);
        if (-EAGAIN == err) {
            snd_pcm_wait(self->snd_handle, 1000);
            continue;
        } else if (err < 0) {
            err = snd_pcm_recover(self->snd_handle, err, 0);
            err = 0;
        } else {
            frames -= err;
            offset += err * 4;
        }
    }

    free(silent);
}

static int play_pcm(tts_player_t *self, unsigned short *output_ptr, int len)
{
    int err = 0;
    int err_cnt = 0;
    int is_message = 0;
    int silence_len = 50;
    int less_len = 0;
    int first_pcm_play = 0;

    if (self && self->snd_handle) {
        pthread_mutex_lock(&self->state_lock);
        if (self->first_pcm_play == 1) {
            self->first_pcm_play = 0;
            first_pcm_play = 1;
            if (g_wiimu_shm->asr_pause_request == 0 && self->is_next_cache == 0) {
                g_wiimu_shm->asr_pause_request = 1;
            }
        }
        if (StrInclude(self->cur_token[e_mp3_cid], ALEXA_PLAY_MESSAGE) && self->type == e_mp3_cid) {
            is_message = 1;
        }
        pthread_mutex_unlock(&self->state_lock);
        if (first_pcm_play == 1) {
            device_clear_buf(self);
            TTS_DEBUG("[TICK]=========== play_pcm first_write_ts at %lld ===========\n",
                      os_power_on_tick());
            NOTIFY_CMD(ASR_TTS, FocusManage_TTSBegin);

            WMUtil_Snd_Ctrl_SetProcessFadeFlag(g_wiimu_shm, 0, SNDSRC_ALEXA_TTS);
            // WMUtil_Snd_Ctrl_SetProcessFadeStep(g_wiimu_shm, 20, SNDSRC_ALEXA_TTS);
            WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 0, SNDSRC_ALEXA_TTS, 1);
            WMUtil_Snd_Ctrl_SetProcessSelfVol(g_wiimu_shm, 100, SNDSRC_ALEXA_TTS);

            g_wiimu_shm->alexa_response_status = 1;
            // if (CLOSE_TALK != AlexaProfileType()) {
            //  NOTIFY_CMD(AMAZON_TTS, CHANGE_BOOST);
            //}
            NOTIFY_CMD(IO_GUARD, TTS_IS_PLAYING);
#if defined(MARSHALL_STANMORE_AVS_TBD_BETA) || defined(MARSHALL_ACTON_AVS_TBD_BETA) ||             \
    defined(PURE_RPC)
            int count = 25;
            usleep(50000);
            if (g_wiimu_shm->device_status == DEV_STANDBY) {
                printf("Start to wait for the MCU resume status before TTS\n");
                while (count--) {
                    if (g_wiimu_shm->device_status != DEV_STANDBY)
                        break;
                    usleep(100000);
                }
                printf("End to wait for the MCU resume status %d before TTS\n",
                       g_wiimu_shm->device_status);
            }
#else
            if (self->sample_rate > 0) {
                play_silence(self, 50, self->sample_rate);
            }
#endif
        }

        less_len = len;
        while (less_len > 0 && self->is_force_stop == 0 && self->tlk_started == 0) {
            if (first_pcm_play) { // walk around for popo noise
                int silent_len = 1600;
                if (less_len < silent_len)
                    silent_len = less_len;
                memset(output_ptr, 0, silent_len);
                printf("To walking around BigPop, set %d pcm to 0\n", silent_len);
            }
            if (g_wiimu_shm->silence_ota)
                memset(output_ptr, 0, less_len * 2);
            err = snd_pcm_writei(self->snd_handle, output_ptr, less_len);
            if (err < 0) {
                err = snd_pcm_recover(self->snd_handle, err, 0);
                err = 0;
                ++err_cnt;
            } else if (err > 0) {
                less_len -= err;
                output_ptr = (unsigned short *)((char *)output_ptr + err * 2);
                err_cnt = 0;
            } else {
                ++err_cnt;
                usleep(20000);
            }
            if (err_cnt > 50) {
                TTS_DEBUG("play pcm error err(%d)\n", err);
                err = -1;
                break;
            }
        }
    }

    return err;
}

static int play_tts(tts_player_t *self, mp3_decode_t *mp3_dec)
{
    int fmt = 0;
    unsigned int nchannels, nsamples, n;
    mad_fixed_t const *left_ch, *right_ch;
    unsigned char output_buf[OUTPUT_BUFFER_SIZE] = {0};
    unsigned short *output_ptr = NULL;

    if (!self || !self->snd_handle || !mp3_dec) {
        TTS_DEBUG("input is NULL\n");
        return -1;
    }

    nchannels = mp3_dec->synth.pcm.channels;
    n = nsamples = mp3_dec->synth.pcm.length;
    left_ch = mp3_dec->synth.pcm.samples[0];
    right_ch = mp3_dec->synth.pcm.samples[1];

    fmt = SND_PCM_FORMAT_S16_LE;
    device_init(self, fmt, &mp3_dec->synth.pcm.samplerate, mp3_dec->synth.pcm.channels);

    output_ptr = (unsigned short *)output_buf;
    while (nsamples--) {
        signed int sample;
        sample = scale(*left_ch++);
        *(output_ptr++) = dithered_vol(sample & 0xffff, self->fix_volume);

        if (nchannels == 2) {
            sample = scale(*right_ch++);
            *(output_ptr++) = dithered_vol(sample & 0xffff, self->fix_volume);
        } else if (nchannels == 1) {
            *(output_ptr++) = dithered_vol(sample & 0xffff, self->fix_volume);
        }
    }

    return play_pcm(self, (unsigned short *)output_buf, n);
}

static mp3_decode_t *decode_init(void)
{
    mp3_decode_t *mp3_dec = calloc(1, sizeof(mp3_decode_t));
    if (mp3_dec) {
        mad_stream_init(&mp3_dec->stream);
        mad_frame_init(&mp3_dec->frame);
        mad_synth_init(&mp3_dec->synth);
        memset(mp3_dec->input_buf, 0x0, sizeof(mp3_dec->input_buf));
        return mp3_dec;
    }

    return NULL;
}

static int decode_uninit(mp3_decode_t *mp3_dec)
{
    if (mp3_dec) {
        mad_synth_finish(&mp3_dec->synth);
        mad_frame_finish(&mp3_dec->frame);
        mad_stream_finish(&mp3_dec->stream);
        free(mp3_dec);
    }

    return 0;
}

static int decode_buffer(tts_player_t *self, mp3_decode_t *mp3_dec, int *dec_size)
{
    int ret = 0;

    if (!self || !mp3_dec) {
        TTS_DEBUG("input is NULL\n");
        return -1;
    }

    // get decode data
    if (mp3_dec->stream.buffer == NULL || mp3_dec->stream.error == MAD_ERROR_BUFLEN) {
        int read_size = 0, remain_size = 0;
        unsigned char *read_start = NULL;

        if (mp3_dec->stream.next_frame != NULL) {
            remain_size = mp3_dec->stream.bufend - mp3_dec->stream.next_frame;
            memmove(mp3_dec->input_buf, mp3_dec->stream.next_frame, remain_size);
            read_start = mp3_dec->input_buf + remain_size;
            read_size = INPUT_BUFFER_SIZE - remain_size;
        } else {
            read_size = INPUT_BUFFER_SIZE;
            read_start = mp3_dec->input_buf;
            remain_size = 0;
        }

        read_size = ttsplayer_read_data(self, read_start, read_size);
        if (read_size == 0) {
            mp3_dec->stream.error = MAD_ERROR_BUFLEN;
            *dec_size = 0;
            ret = -1;
            goto exit;
        } else {
            *dec_size = read_size;
        }

        mad_stream_buffer(&mp3_dec->stream, mp3_dec->input_buf, read_size + remain_size);
        mp3_dec->stream.error = 0;
    }

    mad_stream_sync(&mp3_dec->stream);
    if (mad_frame_decode(&mp3_dec->frame, &mp3_dec->stream)) {
        if (MAD_RECOVERABLE(mp3_dec->stream.error)) {
            if (mp3_dec->stream.error != MAD_ERROR_LOSTSYNC) {
                TTS_DEBUG("mad_frame_decode wran : code 0x%x !\n", mp3_dec->stream.error);
            }
            // TTS_DEBUG("mad_frame_decode wran : code %d !\n", mp3_dec->stream.error);
            ret = -1;
            goto exit;
        } else if (mp3_dec->stream.error == MAD_ERROR_BUFLEN) {
            // TTS_DEBUG("mad_frame_decode under run : code %d !\n");
            ret = -1;
            goto exit;
        } else {
            /// TTS_DEBUG("!!!!!!!mad_frame_decode error!\n");
            ret = -2;
            goto exit;
        }
    }

    mad_synth_frame(&mp3_dec->synth, &mp3_dec->frame);
    ret = 0;

exit:
    return ret;
}

static int circular_buf_reset(tts_player_t *self, int index)
{
    if (self && self->play_buffer[index]) {
        RingBufferReset(self->play_buffer[index]);
    }

    return 0;
}

static int circular_buf_write(tts_player_t *self, int index, char *data, int data_size)
{
    int write_len = 0;

    if (self && self->play_buffer[index] && self->is_force_stop == 0) {
        write_len = RingBufferWrite(self->play_buffer[index], data, data_size);
    }

    return write_len;
}

static int circular_buf_read(tts_player_t *self, int index, char *buf, int read_size)
{
    int read_len = 0;

    if (self && self->play_buffer[index] && self->is_force_stop == 0) {
        read_len = RingBufferRead(self->play_buffer[index], buf, read_size);
    }

    return (read_len > 0) ? read_len : 0;
}

static int ttsplayer_play_cache_next(tts_player_t *self)
{
    int ret = 0;
    int cmd = 0;

    if (self) {
        pthread_mutex_lock(&self->state_lock);
        if (self->buf_cached == 1 && self->is_force_stop == 0) {
            if (self->cur_token[e_mp3_cid]) {
                free(self->cur_token[e_mp3_cid]);
                self->cur_token[e_mp3_cid] = NULL;
            }
            if (self->back_token) {
                self->cur_token[e_mp3_cid] = strdup(self->back_token);
                free(self->back_token);
                self->back_token = NULL;
            }
            self->buf_cached = 0;
            self->type = e_mp3_cid;
            self->read_index = self->write_index;
            cmd = e_cmd_play;
            ret = 1;
            TTS_DEBUG("!!!!!!!! start to play cache buffer !!!!!!!!\n");
        } else {
            self->buf_cached = 0;
        }
        pthread_mutex_unlock(&self->state_lock);
        if (cmd > 0) {
            cmd_q_add_tail(self->play_cmd, &cmd, sizeof(int));
        }
    }

    return ret;
}

void *ttsplayer_run(void *param)
{
    tts_player_t *self = (tts_player_t *)param;
    if (self) {
        int need_speek_end = 0;
        int is_get_next = 0;
        mp3_decode_t *mp3_dec = NULL;

#if defined(MARSHALL_STANMORE_AVS_TBD_BETA) || defined(MARSHALL_ACTON_AVS_TBD_BETA) ||             \
    defined(PURE_RPC)
        device_open(self);
#endif
        self->is_running = 1;
        int play_status = e_status_idle;
        while (self->is_running == 1) {
            int is_force_stop = 0;
            int is_pause = 0;
            int is_tlk_start = 0;
            int *cmd_type = (int *)cmd_q_pop(self->play_cmd);
            if (cmd_type) {
                pthread_mutex_lock(&self->state_lock);
                is_tlk_start = self->tlk_started;
                if (e_cmd_stop == *cmd_type) {
                    play_status = e_status_stopped;
                    if (self->is_force_stop) {
                        TTS_DEBUG("----- TTS playback interrupt----\n");
                        is_force_stop = 1;
                        self->is_force_stop = 0;
                        if (self->play_status == e_status_playing) {
                            self->play_status = e_status_interrupt;
                        } else if (self->play_status == e_status_interrupt) {
                            self->play_status = e_status_stopped;
                        }
                        if (need_speek_end) {
#ifdef AVS_FOCUS_MGMT
                            // TODO: When playing tts, then make a call, and the speech finished
                            // didnot send,
                            // TODO: and the this will cause the focus manage not focus on call
                            focus_mgmt_activity_status_set(SpeechSynthesizer, UNFOCUS);
#endif
                            need_speek_end = 0;
                        }
                    }
                } else if (e_cmd_play == *cmd_type) {
                    if (self->is_force_stop) {
                        self->is_force_stop = 0;
                    }
                    self->first_pcm_play = 1;
                    self->play_paused = 0;
                    play_status = e_status_playing;
                    self->play_status = e_status_playing;
                } else if (e_cmd_pause == *cmd_type) {
                    if (self->is_force_stop) {
                        self->is_force_stop = 0;
                    }
                    is_pause = 1;
                    play_status = e_status_stopped;
                }
                pthread_mutex_unlock(&self->state_lock);
                TTS_DEBUG("----- 0 recv cmd = %s ------\n", g_cmd_str[*cmd_type]);
                free(cmd_type);
            }

            if (self->play_paused) {
                usleep(20000);
                continue;
            }
            if (e_status_playing == play_status) {
                // init decode
                if (!mp3_dec && is_tlk_start == 0) {
                    pthread_mutex_lock(&self->state_lock);
                    self->play_status = e_status_playing;
                    TTS_DEBUG("----- 1 start play tts ------\n");
                    if (self->type == e_mp3_tts && self->cur_token[e_mp3_tts]) {
                        need_speek_end = 1;
                        alexa_speech_cmd(0, NAME_SPEECHSTARTED, self->cur_token[e_mp3_tts]);
                    } else if (self->type == e_mp3_cid && self->cur_token[e_mp3_cid]) {
                        is_get_next = 0;
                        alexa_audio_cmd(0, NAME_PLAYSTARTED, self->cur_token[e_mp3_cid], 0, 0, 0);
                    }
                    pthread_mutex_unlock(&self->state_lock);
                    // NOTIFY_CMD(IO_GUARD, PROMPT_START);
                }
                if (!mp3_dec)
                    mp3_dec = decode_init();

                // decode buffer
                int dec_size = -1;
                int rv = decode_buffer(self, mp3_dec, &dec_size);
                if (rv == -2) {
                    TTS_DEBUG("----- 2 mad_frame_decode error e_tts_stop ------\n");
                    play_status = e_status_stopped;
                    continue;
                } else if (rv == -1) {
                    // need some sleep
                    if (dec_size == 0) {
                        int is_finished = 0;
                        pthread_mutex_lock(&self->state_lock);
                        is_finished = self->cache_finish[self->read_index];
                        self->cache_finish[self->read_index] = 0;
                        if (is_finished) {
                            self->play_status = e_status_idle;
                        }
                        pthread_mutex_unlock(&self->state_lock);
                        if (is_finished) {
                            TTS_DEBUG("[MESSAGE TICK %lld] ----- 4 tts play is_finished "
                                      "write_index(%d) read_index(%d)------\n",
                                      os_power_on_tick(), self->write_index, self->read_index);
                            play_status = e_status_stopped;
                        } else {
                            usleep(20000);
                        }
                    }
                    continue;
                } else if (rv == 0) {
// if (!self->snd_handle) {
//  device_open(self);
//}
#ifdef EN_AVS_COMMS
                    int need_next = 0;
                    pthread_mutex_lock(&self->state_lock);
                    if (StrInclude(self->cur_token[e_mp3_cid], ALEXA_PLAY_MESSAGE)) {
                        if (self->cache_finish[self->read_index] && is_tlk_start == 0 &&
                            is_get_next == 0 && self->type == e_mp3_cid &&
                            self->cur_token[e_mp3_cid]) {
                            need_next = 1;
                            is_get_next = 1;
                            self->cid_continue = 0;
                        }
                    } else {
                        is_get_next = 0;
                    }
                    pthread_mutex_unlock(&self->state_lock);
                    if (need_next == 1) {
                        TTS_DEBUG("[MESSAGE TICK %lld]----- 5 get next to play ------\n",
                                  tickSincePowerOn());
                        alexa_audio_cmd(1, NAME_PLAYNEARLYFIN, self->cur_token[e_mp3_cid], 0, 0, 0);
                    }
#endif
                    // play audio frame
                    if (self && self->snd_handle == NULL) {
                        TTS_DEBUG("!!!!!!!!!self->snd_handle == NULL !\n");
                        device_open(self);
                    }
                    if (play_tts(self, mp3_dec) < 0) {
                        TTS_DEBUG("!!!!!!!!!play_tts(self, mp3_dec)  failed ++\n");
#if !defined(MARSHALL_STANMORE_AVS_TBD_BETA) && !defined(MARSHALL_ACTON_AVS_TBD_BETA) &&           \
    !defined(PURE_RPC)
                        device_close(self);
#else
                        self->alsa_inited = 0;
#endif
                        TTS_DEBUG("!!!!!!!!!play_tts(self, mp3_dec)  failed -- \n");
                    }
                }

            } else if (e_status_stopped == play_status) {
                // error, exit play
                if (mp3_dec) {
                    int need_notify_continue = 0;
                    int first_pcm_play = 0;
                    int is_send_speek_end = 0;
                    decode_uninit(mp3_dec);
                    mp3_dec = NULL;
                    if (self->snd_handle) {
#if !defined(MARSHALL_STANMORE_AVS_TBD_BETA) && !defined(MARSHALL_ACTON_AVS_TBD_BETA) &&           \
    !defined(PURE_RPC)
                        device_close(self);
#else
                        self->alsa_inited = 0;
#endif
                        WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 1, SNDSRC_ALEXA_TTS, 1);
                    }
                    pthread_mutex_lock(&self->state_lock);
                    if (self->tlk_continue == 1) {
                        self->tlk_continue = 0;
                        need_notify_continue = 1;
                    }
                    if (self->first_pcm_play == 1) {
                        first_pcm_play = 1;
                    }
                    if (is_force_stop == 0 && is_pause == 0) {
                        if ((self->type == e_mp3_tts || need_speek_end == 1) &&
                            self->cur_token[e_mp3_tts]) {
                            need_speek_end = 0;
                            is_send_speek_end = 1;
#ifdef AVS_FOCUS_MGMT
                            focus_mgmt_activity_status_set(SpeechSynthesizer, UNFOCUS);
#endif
                            alexa_speech_cmd(need_notify_continue, NAME_SPEECHFINISHED,
                                             self->cur_token[e_mp3_tts]);
                        }
                    }
                    if (is_force_stop == 0 && is_pause == 0 && need_notify_continue == 0) {
                        if ((self->type == e_mp3_cid ||
                             (self->cid_continue == 1 && self->type == e_mp3_tts)) &&
                            self->cur_token[e_mp3_cid]) {
                            self->cid_continue = 0;
                            if (is_get_next == 0) {
                                TTS_DEBUG("----- 6 get next to play ?? ------\n");
                                alexa_audio_cmd(0, NAME_PLAYNEARLYFIN, self->cur_token[e_mp3_cid],
                                                0, 0, 0);
                            }
                            alexa_audio_cmd(0, NAME_PLAYFINISHED, self->cur_token[e_mp3_cid], 0, 0,
                                            0);
                            g_wiimu_shm->asr_pause_request = 0;
                        }
                    }
                    if (self->first_pcm_play == 0) {
                        self->first_pcm_play = 1;
                        g_wiimu_shm->asr_status = 0;
                        g_wiimu_shm->alexa_response_status = 0;
                    }
                    is_get_next = 0;
                    pthread_mutex_unlock(&self->state_lock);
                    g_wiimu_shm->alexa_response_status = 0;
                    NOTIFY_CMD(IO_GUARD, PROMPT_END);
                    if (need_notify_continue == 0 && is_force_stop == 0)
                        NOTIFY_CMD(IO_GUARD, TTS_IDLE);
                    if (first_pcm_play == 0 && is_force_stop == 0 && is_tlk_start == 0 &&
                        need_notify_continue == 0) {
                        SetAudioInputProcessorObserverState(IDLE);
                        // if tts is played, but didnot send Speechend event, then clear the flags
                        if (is_send_speek_end == 0) {
                            g_wiimu_shm->asr_pause_request = 0;
                        }
                    }
                    if (need_notify_continue == 1) {
                        if (is_send_speek_end) {
                            usleep(100000);
                        }
                        NOTIFY_CMD(ASR_TTS, TLK_CONTINUE);
                    }
                    g_wiimu_shm->asr_status = 0;
                }
                pthread_mutex_lock(&self->state_lock);
                if (is_pause == 1 && (self->type == e_mp3_cid && self->cur_token[e_mp3_cid])) {
                    alexa_audio_cmd(0, NAME_STOPPED, self->cur_token[e_mp3_cid], 0, 0, 0);
                    self->play_status = e_status_pause;
                }
                pthread_mutex_unlock(&self->state_lock);
                // g_wiimu_shm->asr_pause_request = 0;
                tts_end_tick = tickSincePowerOn();
                TTS_DEBUG("[MESSAGE TICK %lld]----- 7 tts play ending ------\n",
                          os_power_on_tick());
                play_status = e_status_idle;
#ifdef EN_AVS_COMMS
                if (ttsplayer_play_cache_next(self) == 1) {
                    // TODO: find a good way to fixed this
                    int cnt = 10;
                    while (self->is_force_stop == 0 && cnt--) {
                        usleep(20000);
                    }
                }
#endif
            } else {

                pthread_mutex_lock(&self->state_lock);
                if (self->tlk_continue == 1) {
                    self->tlk_continue = 0;
                    NOTIFY_CMD(ASR_TTS, TLK_CONTINUE);
                }
                pthread_mutex_unlock(&self->state_lock);
                usleep(20000);
            }
        }

        device_close(self);
        g_wiimu_shm->alexa_response_status = 0;
        TTS_DEBUG("Audio Processor stopped !\n");
    }

    return NULL;
}

static int ttsplayer_init(tts_player_t *self)
{
    int ret = -1;
    pthread_attr_t attr;
    struct sched_param param;

    pthread_mutex_init(&self->state_lock, NULL);

    self->is_running = 0;
    self->first_pcm_play = 1;
    self->fix_volume = 0x10000;

    self->play_cmd = cmd_q_init(NULL);
    self->play_buffer[0] = RingBufferCreate(RING_BUFFER_LEN);
    self->play_buffer[1] = RingBufferCreate(RING_BUFFER_LEN);

    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    param.sched_priority = 90;
    pthread_attr_setschedparam(&attr, &param);

    ret = pthread_create(&self->thread_id, &attr, ttsplayer_run, self);
    if (ret != 0) {
        TTS_DEBUG("ttsplayer_run create faield!\n");
        self->thread_id = 0;
    }
    pthread_attr_destroy(&attr);

    return ret;
}

tts_player_t *ttsplayer_create(void)
{
    int ret = -1;

    tts_player_t *self = (tts_player_t *)calloc(1, sizeof(tts_player_t));
    if (self) {
        ret = ttsplayer_init(self);
        if (ret != 0) {
            ttsplayer_destroy(&self);
            self = NULL;
        }
    }

    return self;
}

void ttsplayer_destroy(tts_player_t **self_p)
{
    tts_player_t *self = *self_p;

    if (self) {
        ttsplayer_stop(self, e_ctrl_stop);
        self->is_running = 0;
        if (self->thread_id > 0) {
            pthread_join(self->thread_id, NULL);
        }
        cmd_q_uninit(&self->play_cmd);
        if (self->play_buffer[0]) {
            RingBufferDestroy(&self->play_buffer[0]);
        }
        if (self->play_buffer[1]) {
            RingBufferDestroy(&self->play_buffer[1]);
        }
        pthread_mutex_lock(&self->state_lock);
        if (self->cur_token[e_mp3_tts]) {
            free(self->cur_token[e_mp3_tts]);
            self->cur_token[e_mp3_tts] = NULL;
        }
        if (self->cur_token[e_mp3_cid]) {
            free(self->cur_token[e_mp3_cid]);
            self->cur_token[e_mp3_cid] = NULL;
        }
        if (self->back_token) {
            free(self->back_token);
            self->back_token = NULL;
        }
        pthread_mutex_unlock(&self->state_lock);
        pthread_mutex_destroy(&self->state_lock);
        free(self);
        *self_p = NULL;
    }
}

static void ttsplayer_no_cache(tts_player_t *self, const char *token, int type)
{
    int cmd = 0;
    printf("ttsplayer_start, type=%d\n", type);
    if (self && type <= e_mp3_cid && type >= e_mp3_tts) {
        pthread_mutex_lock(&self->state_lock);
        if (self->cur_token[type]) {
            free(self->cur_token[type]);
            self->cur_token[type] = NULL;
        }
        if (token) {
            self->cur_token[type] = strdup(token);
        }
        self->tlk_started = 0;
        if (type == e_mp3_cid) {
            self->type = e_mp3_cid;
            self->is_next_cache = 1;
            if (self->cid_continue == 1) {
                self->cid_continue = 0;
            }
        } else if (type == e_mp3_tts) {
            self->type = e_mp3_tts;
            if (self->is_next_cache == 1) {
                self->is_next_cache = 0;
            }
            if (self->tlk_continue == 1) {
                self->tlk_continue = 0;
            }
        } else {
            self->is_next_cache = 0;
        }
        self->buf_cached = 0;
        circular_buf_reset(self, self->read_index);
        self->cache_finish[self->read_index] = 0;
        // make sure the read_index = write_index
        self->write_index = self->read_index;
        pthread_mutex_unlock(&self->state_lock);
        if (type == e_mp3_tts) {
            cmd = e_cmd_play;
            cmd_q_add_tail(self->play_cmd, &cmd, sizeof(int));
        }
    }
}

void ttsplayer_pause(tts_player_t *self)
{
    printf("ttsplayer_pause\n");
    if (self) {
        pthread_mutex_lock(&self->state_lock);
        self->play_paused = 1;
        pthread_mutex_unlock(&self->state_lock);
    }
}

void ttsplayer_resume(tts_player_t *self)
{
    printf("ttsplayer_resume\n");
    if (self) {
        pthread_mutex_lock(&self->state_lock);
        self->play_paused = 0;
        pthread_mutex_unlock(&self->state_lock);
    }
}

static void ttsplayer_cache_next(tts_player_t *self, const char *token)
{
    if (token) {
        pthread_mutex_lock(&self->state_lock);
        if (self->back_token) {
            free(self->back_token);
            self->back_token = NULL;
        }
        self->back_token = strdup(token);
        if (self->read_index == 0) {
            self->write_index = 1;
        } else {
            self->write_index = 0;
        }
        circular_buf_reset(self, self->write_index);
        self->cache_finish[self->write_index] = 0;
        self->buf_cached = 1;
        TTS_DEBUG(
            "[MESSAGE TICK %lld] ----- 8 tts start cache write_index(%d) read_index(%d)------\n",
            os_power_on_tick(), self->write_index, self->read_index);
        pthread_mutex_unlock(&self->state_lock);
    }
}

int ttsplayer_start(tts_player_t *self, const char *token, int type, int is_next)
{
    int cmd = 0;
    int play_status = 0;

    if (self) {
        pthread_mutex_lock(&self->state_lock);
        play_status = self->play_status;
        pthread_mutex_unlock(&self->state_lock);
        if (is_next == 1 && play_status >= e_status_playing) {
            ttsplayer_cache_next(self, token);
        } else {
            ttsplayer_no_cache(self, token, type);
        }
    }

    return 0;
}

/*
 * e_ctrl_stop: stop current tts
 * e_ctrl_finished: tts cache finished
 * e_ctrl_continue: need talk continue and cache finished
 * e_ctrl_pause: pause currtent play
 */
void ttsplayer_stop(tts_player_t *self, int ctrl_cmd)
{
    int ret = -1;
    int cmd = -1;
    TTS_DEBUG("ttsplayer_stop, cmd=%d , tick=%lld\n", ctrl_cmd, os_power_on_tick());

    if (self) {
        pthread_mutex_lock(&self->state_lock);
        if (ctrl_cmd == e_ctrl_stop) {
            self->is_force_stop = 1;
            self->tlk_started = 1;
            self->first_pcm_play = 1;
            if (self->play_status >= e_status_playing && self->type == e_mp3_cid &&
                self->cur_token[e_mp3_cid]) {
                self->cid_continue = 1;
            }
            // stop cache current write buffer, and clear buf_cached flags
            self->cache_finish[self->write_index] = 1;
            self->buf_cached = 0;
            self->tlk_continue = 0;
            cmd = e_cmd_stop;
        } else if (ctrl_cmd == e_ctrl_finished) {
            self->cache_finish[self->write_index] = 1;
            TTS_DEBUG("[MESSAGE TICK %lld] ----- 3 tts is cache finished write_index(%d) "
                      "read_index(%d)------\n",
                      os_power_on_tick(), self->write_index, self->read_index);
        } else if (ctrl_cmd == e_ctrl_continue) {
            self->tlk_continue = 1;
            self->cid_continue = 0;
            self->is_next_cache = 0;
            // clear buf_cached flags
            self->buf_cached = 0;
        } else if (ctrl_cmd == e_ctrl_pause) {
            if (self->type == e_mp3_cid && self->cur_token[e_mp3_cid] &&
                self->play_status >= e_status_playing) {
                self->is_force_stop = 1;
                self->is_next_cache = 0;
                cmd = e_cmd_pause;
            }
            self->cid_continue = 0;
            // stop cache current write buffer, and clear buf_cached flags
            self->cache_finish[self->write_index] = 1;
            self->buf_cached = 0;
        }
        pthread_mutex_unlock(&self->state_lock);
        if (cmd > 0) {
            cmd_q_add_tail(self->play_cmd, &cmd, sizeof(int));
        }
    }
}

int ttsplayer_write_data(tts_player_t *self, char *data, int len)
{
    int write_len = 0;
    int write_index = 0;

    if (self && data && len > 0) {
        pthread_mutex_lock(&self->state_lock);
        write_index = self->write_index;
        pthread_mutex_unlock(&self->state_lock);
        write_len = circular_buf_write(self, write_index, data, len);
    }

    return write_len;
}

int ttsplayer_read_data(tts_player_t *self, char *data, int len)
{
    int read_len = 0;
    int read_index = 0;

    if (self && data && len > 0) {
        pthread_mutex_lock(&self->state_lock);
        read_index = self->read_index;
        pthread_mutex_unlock(&self->state_lock);
        read_len = circular_buf_read(self, read_index, data, len);
    }

    return read_len;
}

/*
 * type: e_mp3_tts/e_mp3_cid
 *
 */
// TODO: the token need to free
int ttsplayer_get_state(tts_player_t *self, char **token, int type)
{
    int play_status = 0;

    if (self) {
        pthread_mutex_lock(&self->state_lock);
        if (!token) {
            play_status = (self->play_status >= e_status_playing) ? 1 : 0;
        } else {
            if (type != self->type && type == e_mp3_cid) {
                if (self->play_status != e_status_stopped) {
                    play_status = self->play_status;
                } else {
                    play_status = e_status_idle;
                }
            } else {
                play_status = self->play_status;
            }
            if (self->cur_token[type]) {
                *token = strdup(self->cur_token[type]);
            }
        }
        pthread_mutex_unlock(&self->state_lock);
    }

    return play_status;
}

/*
 * stop current cid play
 */
int ttsplayer_clear_cid(tts_player_t *self, int flags)
{
    int ret = -1;
    int cmd = -1;

    if (self) {
        pthread_mutex_lock(&self->state_lock);
        self->cid_continue = 0;
        if (self->cur_token[e_mp3_cid]) {
            free(self->cur_token[e_mp3_cid]);
            self->cur_token[e_mp3_cid] = NULL;
        }
        self->buf_cached = 0;
        if (self->type == e_mp3_cid && self->play_status >= e_status_playing && flags) {
            self->is_force_stop = 1;
            cmd = e_cmd_stop;
        }
        pthread_mutex_unlock(&self->state_lock);
        if (cmd > 0) {
            cmd_q_add_tail(self->play_cmd, &cmd, sizeof(int));
            ret = 0;
        }
    }

    return ret;
}

/*
 * set the cid can play
 */
int ttsplayer_can_play_cid(tts_player_t *self)
{
    int ret = -1;
    int cmd = -1;

    if (self) {
        pthread_mutex_lock(&self->state_lock);
        if (self->is_next_cache = 1 && (self->type == e_mp3_cid)) {
            self->is_next_cache = 0;
            cmd = e_cmd_play;
        }
        pthread_mutex_unlock(&self->state_lock);
        if (cmd > 0) {
            cmd_q_add_tail(self->play_cmd, &cmd, sizeof(int));
            ret = 0;
        }
    }

    return ret;
}

/*
 * get the asr need continue or not
 *
 */
int ttsplayer_get_continue(tts_player_t *self)
{
    int tlk_continue = 0;

    if (self) {
        pthread_mutex_lock(&self->state_lock);
        tlk_continue = self->tlk_continue;
        pthread_mutex_unlock(&self->state_lock);
    }

    return tlk_continue;
}

int ttsplayer_clear_talk(tts_player_t *self)
{
    if (self) {
        pthread_mutex_lock(&self->state_lock);
        if (self->tlk_started == 1) {
            self->tlk_started = 0;
        }
        if (self->cid_continue == 1 && self->cur_token[e_mp3_cid]) {
            self->cid_continue = 0;
            TTS_DEBUG("----- resmue get next to play ?? ------\n");
            alexa_audio_cmd(0, NAME_PLAYNEARLYFIN, self->cur_token[e_mp3_cid], 0, 0, 0);
        }
        pthread_mutex_unlock(&self->state_lock);
    }

    return 0;
}
