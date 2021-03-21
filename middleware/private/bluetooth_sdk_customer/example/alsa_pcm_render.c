#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "alsa/asoundlib.h"
#include "linkplay_bluetooth_interface.h"
#include "linkplay_resample_interface.h"

#define RING_BUFFER_LEN (1024 * 1024)

typedef struct _ring_buffer_s {
    unsigned char *buf;
    size_t buf_len;
    volatile size_t write_pos;
    volatile size_t read_pos;
    volatile size_t readable_len;
    pthread_mutex_t lock;
    int fin_flags;
} ring_buffer_t;
pthread_t thread;
static ring_buffer_t *g_ring_buffer = NULL;
static int g_close_thread = 0;
static void *write_thread(void *arg);
int audio_render_pcm_write_dataIntel(UINT8 *p_buffer, int frames);

ring_buffer_t *RingBufferCreate(size_t len)
{
    printf("[%s:%d] ring_buffer_create start\n", __FUNCTION__, __LINE__);

    if (len <= 0) {
        len = RING_BUFFER_LEN;
    }

    ring_buffer_t *ring_buffer = (ring_buffer_t *)calloc(1, sizeof(ring_buffer_t));
    if (ring_buffer) {
        ring_buffer->buf = (char *)calloc(1, len);
        if (!ring_buffer->buf) {
            goto EXIT;
        }
        ring_buffer->buf_len = len;
        ring_buffer->write_pos = 0;
        ring_buffer->read_pos = 0;
        ring_buffer->readable_len = 0;
        ring_buffer->fin_flags = 0;
        pthread_mutex_init(&ring_buffer->lock, NULL);

        return ring_buffer;
    }

EXIT:
    if (ring_buffer) {
        free(ring_buffer);
    }
    printf("[%s:%d] ring_buffer_create error cause by calloc failed\n", __FUNCTION__, __LINE__);
    return NULL;
}

void RingBufferDestroy(ring_buffer_t **_this)
{
    printf("[%s:%d] ring_buffer_destroy start\n", __FUNCTION__, __LINE__);
    if (_this) {
        ring_buffer_t *ring_buffer = *_this;
        if (ring_buffer) {
            if (ring_buffer->buf) {
                free(ring_buffer->buf);
            }
            pthread_mutex_destroy(&ring_buffer->lock);
            free(ring_buffer);
        }
    } else {
        printf("[%s:%d] input error\n", __FUNCTION__, __LINE__);
    }
}

void RingBufferReset(ring_buffer_t *_this)
{
    // printf("[%s:%d] circle_buffer_reset start\n", __FUNCTION__, __LINE__);

    if (_this) {
        pthread_mutex_lock(&_this->lock);
        _this->write_pos = 0;
        _this->read_pos = 0;
        _this->readable_len = 0;
        _this->fin_flags = 0;
        pthread_mutex_unlock(&_this->lock);
    } else {
        printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);
    }
}

size_t RingBufferWrite(ring_buffer_t *_this, char *data, size_t len)
{
    size_t write_len = 0;

    if (_this) {
        pthread_mutex_lock(&_this->lock);
        write_len = _this->buf_len - _this->readable_len;
        write_len = write_len > len ? len : write_len;
        if (write_len) {
            write_len = (_this->buf_len - _this->write_pos) > write_len
                            ? write_len
                            : (_this->buf_len - _this->write_pos);

            memcpy(_this->buf + _this->write_pos, data, write_len);
            _this->write_pos = (_this->write_pos + write_len) % _this->buf_len;
            _this->readable_len += write_len;
            pthread_mutex_unlock(&_this->lock);
            //   printf("[%d:%d] ring buffer \n", write_len ,_this->buf_len );

            return write_len;
        } else {
            printf("[%s:%d] ring buffer is full\n", __FUNCTION__, __LINE__);
            pthread_mutex_unlock(&_this->lock);
            return 0;
        }
    }

    printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);
    return -1;
}

size_t RingBufferRead(ring_buffer_t *_this, char *data, size_t len)
{
    size_t read_len = 0;

    if (_this) {
        pthread_mutex_lock(&_this->lock);
        read_len = (_this->buf_len - _this->read_pos) > _this->readable_len
                       ? _this->readable_len
                       : (_this->buf_len - _this->read_pos);
        read_len = read_len > len ? len : read_len;

        if (read_len > 0) {
            memcpy(data, _this->buf + _this->read_pos, read_len);
            _this->readable_len = _this->readable_len - read_len;
            _this->read_pos = (_this->read_pos + read_len) % _this->buf_len;
            pthread_mutex_unlock(&_this->lock);
            return read_len;
        } else {
            pthread_mutex_unlock(&_this->lock);
            printf("[%s:%d] ring buffer is empty\n", __FUNCTION__, __LINE__);
            return 0;
        }
    }

    printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);
    return -1;
}

size_t RingBufferReadableLen(ring_buffer_t *_this)
{
    if (_this) {
        size_t readable_len = 0;
        pthread_mutex_lock(&_this->lock);
        readable_len = _this->readable_len;
        pthread_mutex_unlock(&_this->lock);
        return readable_len;
    }
    printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);

    return -1;
}

size_t RingBufferLen(ring_buffer_t *_this)
{
    if (_this) {
        size_t buf_len = 0;
        pthread_mutex_lock(&_this->lock);
        buf_len = _this->buf_len;
        pthread_mutex_unlock(&_this->lock);
        return buf_len;
    }
    printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);

    return -1;
}

int RingBufferGetFinished(ring_buffer_t *_this)
{
    if (_this) {
        int finished = 0;
        pthread_mutex_lock(&_this->lock);
        finished = _this->fin_flags;
        pthread_mutex_unlock(&_this->lock);
        return finished;
    }
    printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);

    return -1;
}

static int g_last_format = 0;
static int g_last_channel = 0;
static int g_last_sample_rate = 0;
static int g_last_bitspersample = 0;
static int g_last_periodsize = 0;
static snd_pcm_t *alsa_handle = NULL;
const int g_default_out_sample_rate = 48000; // the alsa default sample rate
af_instance_t *g_resample_instance = NULL;
af_data_t g_af_data;

static int _snd_pcm_set_params(snd_pcm_t *pcm, snd_pcm_format_t format, snd_pcm_access_t access,
                               unsigned int channels, unsigned int rate, int soft_resample,
                               unsigned int latency)
{
    snd_pcm_hw_params_t *params;
    snd_pcm_sw_params_t *swparams;
    const char *s = snd_pcm_stream_name(snd_pcm_stream(pcm));
    snd_pcm_uframes_t buffer_size, period_size;
    unsigned int rrate, period_time;
    int err;

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_sw_params_alloca(&swparams);

    assert(pcm);
    /* choose all parameters */
    err = snd_pcm_hw_params_any(pcm, params);
    if (err < 0) {
        SNDERR("Broken configuration for %s: no configurations available", s);
        return err;
    }
    /* set software resampling */
    err = snd_pcm_hw_params_set_rate_resample(pcm, params, soft_resample);
    if (err < 0) {
        SNDERR("Resampling setup failed for %s: %s", s, snd_strerror(err));
        return err;
    }
    /* set the selected read/write format */
    err = snd_pcm_hw_params_set_access(pcm, params, access);
    if (err < 0) {
        SNDERR("Access type not available for %s: %s", s, snd_strerror(err));
        return err;
    }
    /* set the sample format */
    err = snd_pcm_hw_params_set_format(pcm, params, format);
    if (err < 0) {
        SNDERR("Sample format not available for %s: %s", s, snd_strerror(err));
        return err;
    }
    /* set the count of channels */
    err = snd_pcm_hw_params_set_channels(pcm, params, channels);
    if (err < 0) {
        SNDERR("Channels count (%i) not available for %s: %s", channels, s, snd_strerror(err));
        return err;
    }
    /* set the stream rate */
    rrate = rate;
    err = snd_pcm_hw_params_set_rate_near(pcm, params, &rrate, 0);
    if (err < 0) {
        SNDERR("Rate %iHz not available for playback: %s", rate, snd_strerror(err));
        return err;
    }
    if (rrate != rate) {
        SNDERR("Rate doesn't match (requested %iHz, get %iHz)", rate, err);
        return -EINVAL;
    }
    /* set the buffer time */
    // err = snd_pcm_hw_params_set_buffer_time_near(pcm, params, &latency, NULL);
    // printf("error %d \r\n",err);
    // if (err < 0) {
    /* error path -> set period size as first */
    /* set the period time */
    period_time = latency / 4;
    err = snd_pcm_hw_params_set_period_time_near(pcm, params, &period_time, NULL);
    if (err < 0) {
        SNDERR("Unable to set period time %i for %s: %s", period_time, s, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_period_size(params, &period_size, NULL);
    if (err < 0) {
        SNDERR("Unable to get period size for %s: %s", s, snd_strerror(err));
        return err;
    }
    g_last_periodsize = period_size;
    buffer_size = period_size * 16;
    err = snd_pcm_hw_params_set_buffer_size_near(pcm, params, &buffer_size);
    if (err < 0) {
        SNDERR("Unable to set buffer size %lu %s: %s", buffer_size, s, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
    if (err < 0) {
        SNDERR("Unable to get buffer size for %s: %s", s, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_buffer_time(params, &latency, NULL);
    if (err < 0) {
        SNDERR("Unable to get buffer time (latency) for %s: %s", s, snd_strerror(err));
        return err;
    }

    /* write the parameters to device */
    printf("buffer_size %d \r\n", buffer_size);
    printf("period_size %d \r\n", period_size);
    printf("latency %d \r\n", latency);
    err = snd_pcm_hw_params(pcm, params);
    if (err < 0) {
        SNDERR("Unable to set hw params for %s: %s", s, snd_strerror(err));
        return err;
    }

    /* get the current swparams */
    err = snd_pcm_sw_params_current(pcm, swparams);
    if (err < 0) {
        SNDERR("Unable to determine current swparams for %s: %s", s, snd_strerror(err));
        return err;
    }
    /* start the transfer when the buffer is almost full: */
    /* (buffer_size / avail_min) * avail_min */
    err = snd_pcm_sw_params_set_start_threshold(pcm, swparams, 8 * period_size);
    if (err < 0) {
        SNDERR("Unable to set start threshold mode for %s: %s", s, snd_strerror(err));
        return err;
    }
    /* allow the transfer when at least period_size samples can be processed */
    err = snd_pcm_sw_params_set_avail_min(pcm, swparams, period_size);
    if (err < 0) {
        SNDERR("Unable to set avail min for %s: %s", s, snd_strerror(err));
        return err;
    }
    /* write the parameters to the playback device */
    err = snd_pcm_sw_params(pcm, swparams);
    if (err < 0) {
        SNDERR("Unable to set sw params for %s: %s", s, snd_strerror(err));
        return err;
    }
    return 0;
}
void audio_render_pcm_close(void *context, void *pcm_handle)
{
    // snd_pcm_t *alsa_handle =  (snd_pcm_t *)pcm_handle;

    // Implement alsa close
    // Note: in this Demo , not open /close alsa device every time at
    // BSA_AVK_START_EVT/BSA_AVK_STOP_EVT
    //
}

void *audio_render_pcm_open(void *context, UINT8 format, UINT16 sample_rate, UINT8 num_channel,
                            UINT8 bit_per_sample)
{
    int status;
    printf("%s %d \r\n", __FUNCTION__, __LINE__);

    if (g_last_format == format && g_last_sample_rate == sample_rate &&
        g_last_channel == num_channel && g_last_bitspersample == bit_per_sample && alsa_handle) {
        printf("pcm_open the same param, do nothing \r\n");
        return (void *)alsa_handle;
    }
    /* If ALSA PCM driver was already open => close it */
    if (alsa_handle != NULL) {
        printf("ALSA driver close ++++ \r\n");
        snd_pcm_close(alsa_handle);
        alsa_handle = NULL;
        printf("ALSA driver close ---- \r\n");
    }

    if (g_resample_instance)
        linkplay_resample_uninit(g_resample_instance);
    g_resample_instance = NULL;

    /* Open ALSA driver */
    printf("ALSA driver open ++++ \r\n");
    status =
        snd_pcm_open(&alsa_handle, "default", SND_PCM_STREAM_PLAYBACK, 0); // SND_PCM_NONBLOCK);
    if (status < 0) {
        printf("snd_pcm_open failed: %s", snd_strerror(status));
        return NULL;
    } else {
        /* Configure ALSA driver with PCM parameters */
        printf("ALSA driver set params ++++ \r\n");
        status =
            _snd_pcm_set_params(alsa_handle, format, SND_PCM_ACCESS_RW_INTERLEAVED, num_channel,
                                g_default_out_sample_rate, 1, 1000000); /* 0.5sec */

        if (status < 0) {
            printf("snd_pcm_set_params failed: %s  \r\n", snd_strerror(status));
        } else {

            if (g_default_out_sample_rate != sample_rate) {
                char resample_params[64];
                snprintf(resample_params, sizeof(resample_params), "%d:16:0:5:0.0",
                         g_default_out_sample_rate);
                printf("linkplay_resample_instance_create %s \r\n", resample_params);
                g_resample_instance = linkplay_resample_instance_create(resample_params);
                if (g_resample_instance) {
                    g_af_data.rate = sample_rate;
                    g_af_data.nch = num_channel;
                    g_af_data.bps = bit_per_sample / 8;
                    linkplay_set_resample_format(&g_af_data, g_resample_instance);
                }
            }
            g_last_format = format;
            g_last_channel = num_channel;
            g_last_bitspersample = bit_per_sample;
            g_last_sample_rate = sample_rate;
        }
    }
    printf("ALSA driver open ----");
    if (g_ring_buffer) {
        g_close_thread = 1;
        pthread_join(thread, 0);
        RingBufferDestroy(&g_ring_buffer);
        g_ring_buffer = NULL;
    }

    g_ring_buffer = RingBufferCreate(0);
    g_close_thread = 0;
    pthread_create(&thread, NULL, write_thread, NULL);

    return (void *)alsa_handle;
}

static void *write_thread(void *arg)
{
    char tmp[4096];
    while (!g_close_thread) {

        if (RingBufferReadableLen(g_ring_buffer) > 0) {
            int read = RingBufferRead(g_ring_buffer, tmp, 4096);
            audio_render_pcm_write_dataIntel(tmp, read);
        } else {
            usleep(10000);
        }
    }

    return NULL;
}
// FILE *file = NULL;
int audio_render_pcm_write_data(void *context, void *pcm_handle, int connection, UINT8 *p_buffer,
                                int length)
{
    int ret = -1;
    if (alsa_handle)
        ret = RingBufferWrite(g_ring_buffer, p_buffer, length);
    return ret;
}

int audio_render_pcm_write_dataIntel(UINT8 *p_buffer, int length)
{
    snd_pcm_sframes_t alsa_frames;
    int bytesframes = g_last_bitspersample * g_last_channel / 8;
    //  printf("frames %d bytesframes %d \r\n",frames,bytesframes);

    if (alsa_handle != NULL && p_buffer && length > 0) {

        if (g_resample_instance) {
            af_data_t *outdata;
            g_af_data.audio = p_buffer;
            g_af_data.len = length;
            outdata = linkPlay_resample(&g_af_data, g_resample_instance);
            if (outdata) {
                p_buffer = outdata->audio;
                if (outdata->len % bytesframes != 0) {
                    printf("outdata->len %d \r\n", outdata->len);
                }
                alsa_frames = outdata->len / bytesframes;
            }
        }
        //	printf("%s %d %d\r\n",__FUNCTION__,__LINE__,frames);

        alsa_frames = snd_pcm_writei(alsa_handle, p_buffer, alsa_frames);
        //  printf("alsa_frames %d \n", alsa_frames);
        if (alsa_frames < 0) {
            alsa_frames = snd_pcm_recover(alsa_handle, alsa_frames, 0);
        }

        if (alsa_frames < 0) {
            printf(" snd_pcm_writei failed %s\n", snd_strerror(alsa_frames));
        }
    }

    return alsa_frames;
}
