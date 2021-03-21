#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <alsa/asoundlib.h>
#include "capture_alsa.h"
#define GET_SAMPLE_SIZE_BY_FORMAT(x) (snd_pcm_format_physical_width((x)) >> 3)
#define _LOG_TAG "[AFE/ALSA]"

/*
 * format:
 * access: SND_PCM_ACCESS_RW_INTERLEAVED
 * channels: SND_PCM_FORMAT_S32_LE
 * sample rate: 48000
 * latency:
 */
static int set_capture_params(snd_pcm_t *handle, snd_pcm_format_t format, snd_pcm_access_t access,
                              unsigned int channels, unsigned int rate, unsigned int latency)
{
    int err;
    int sampling_rate = rate;
    snd_pcm_uframes_t chunk_size;
    unsigned int buffer_time = latency;
    unsigned int max_buffer_time = 0;
    unsigned int period_time = 0;
    snd_pcm_uframes_t buffer_size;
    snd_pcm_hw_params_t *record_hw_params = 0;

    snd_pcm_hw_params_alloca(&record_hw_params);
    err = snd_pcm_hw_params_any(handle, record_hw_params);
    if (err < 0) {
        printf("%s Broken configuration for this PCM: no configurations available\n", _LOG_TAG);
    }
    err = snd_pcm_hw_params_set_access(handle, record_hw_params, access);
    if (err < 0) {
        printf("%s Access type not available\n", _LOG_TAG);
        return -1;
    }
    err = snd_pcm_hw_params_set_format(handle, record_hw_params, format);
    if (err < 0) {
        printf("%s Sample format non available\n", _LOG_TAG);
        return -1;
    }
    err = snd_pcm_hw_params_set_channels(handle, record_hw_params, channels);
    if (err < 0) {
        printf("%s Channels count non available\n", _LOG_TAG);
        return -1;
    }
    err = snd_pcm_hw_params_set_rate_near(handle, record_hw_params, (unsigned int *)&sampling_rate,
                                          0);
    if (err < 0) {
        printf("%s sampling_rate set faile %d\n", _LOG_TAG, sampling_rate);
    }
    err = snd_pcm_hw_params_get_buffer_time_max(record_hw_params, &max_buffer_time, 0);

    if (buffer_time > max_buffer_time)
        buffer_time = max_buffer_time;
    period_time = buffer_time / 4;

    err = snd_pcm_hw_params_set_buffer_time_near(handle, record_hw_params, &buffer_time, 0);
    if (err < 0) {
        printf("%s buffer_time set faile %d\n", _LOG_TAG, buffer_time);
    }
    err = snd_pcm_hw_params_set_period_time_near(handle, record_hw_params, &period_time, 0);
    if (err < 0) {
        printf("%s period_time set faile %d\n", _LOG_TAG, period_time);
    }

    err = snd_pcm_hw_params(handle, record_hw_params);
    if (err < 0) {
        printf("%s Unable to set HW parameters: %s\n", _LOG_TAG, snd_strerror(err));
        return -1;
    }
    snd_pcm_hw_params_get_period_size(record_hw_params, &chunk_size, 0);
    snd_pcm_hw_params_get_buffer_size(record_hw_params, &buffer_size);

    if (chunk_size == buffer_size) {
        printf("%s Can't use period equal to buffer size (%lu == %lu)\n", _LOG_TAG, chunk_size,
               buffer_size);
    }

    printf("%s buffer_time %d period_time %d max_buffer_time %d\n", _LOG_TAG, buffer_time,
           period_time, max_buffer_time);
    return 0;
}

static int alsa_set_params(wm_alsa_context_t *alsa_context)
{
    if (alsa_context && alsa_context->handle) {
        return set_capture_params(alsa_context->handle, alsa_context->format, alsa_context->access,
                                  alsa_context->channels, alsa_context->rate,
                                  alsa_context->latency_ms * 1000);
    }
    return -1;
}

int capture_alsa_open(wm_alsa_context_t *alsa_context)
{
    int ret = 0;
    if (alsa_context && alsa_context->device_name) {
        ret = snd_pcm_open(&alsa_context->handle, alsa_context->device_name,
                           alsa_context->stream_type,
                           alsa_context->block_mode ? 0 : SND_PCM_NONBLOCK);
        if (!ret) {
            if (!alsa_set_params(alsa_context))
                return 0;
        }
    }
    return -1;
}

void capture_alsa_close(wm_alsa_context_t *alsa_context)
{
    if (alsa_context && alsa_context->handle) {
        snd_pcm_close(alsa_context->handle);
        alsa_context->handle = NULL;
    }
}

int capture_alsa_read(wm_alsa_context_t *alsa_context, void *buf, int len)
{
    int ret = -1;
    if (alsa_context && alsa_context->handle) {
        len = len / (alsa_context->channels * GET_SAMPLE_SIZE_BY_FORMAT(alsa_context->format));
        ret = snd_pcm_readi(alsa_context->handle, buf, len);
        if (ret > 0)
            ret = ret * alsa_context->channels * GET_SAMPLE_SIZE_BY_FORMAT(alsa_context->format);
    }
    return ret;
}

// xrun and suspend recovery
int capture_alsa_recovery(wm_alsa_context_t *alsa_context, int err, int timeout_ms)
{
    printf("%s alsa pcm read recovery\n", _LOG_TAG);
    snd_pcm_status_t *status;
    snd_pcm_t *handle = NULL;

    int ret = -1;
    int time_ms = 0;
    int res = 0;

    if (!alsa_context || !alsa_context->handle) {
        printf("%s ERROR null pointer detected.\n", _LOG_TAG);
        return -1;
    }

    handle = alsa_context->handle;
    snd_pcm_status_alloca(&status);

    if (err == -EAGAIN) {
        snd_pcm_wait(handle, 16);
        return 0;
    } else if (err == -EPIPE) {
        if ((res = snd_pcm_status(handle, status)) < 0) {
            printf("%s status error: %s", _LOG_TAG, snd_strerror(res));
        } else if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
            printf("%s Capture underrun !!!\n", _LOG_TAG);
            if ((res = snd_pcm_prepare(handle)) < 0) {
                printf("%s capture xrun: prepare error: %s\n", _LOG_TAG, snd_strerror(res));
                goto REINIT;
            } else {
                ret = 0;
            }
        } else if (snd_pcm_status_get_state(status) == SND_PCM_STATE_DRAINING) {
            if ((res = snd_pcm_prepare(handle)) < 0) {
                printf("%s capture xrun(DRAINING): prepare error: %s\n", _LOG_TAG,
                       snd_strerror(res));
                goto REINIT;
            } else {
                ret = 0;
            }
        } else {
            printf("%s !!! %d err=%d state=%d\n", _LOG_TAG, __LINE__, err,
                   snd_pcm_status_get_state(status));
        }

    } else if (err == -ESTRPIPE) {
        printf("%s capture ESTRPIPE\n", _LOG_TAG);
        while ((res = snd_pcm_resume(handle)) == -EAGAIN) {
            if (timeout_ms > 0 && time_ms >= timeout_ms)
                break;
            usleep(10000); /* wait until suspend flag is released */
            time_ms += 10;
        }
        if (res < 0) {
            if ((res = snd_pcm_prepare(handle)) < 0) {
                printf("%s capture suspend: prepare error: %s\n", _LOG_TAG, snd_strerror(res));
                goto REINIT;
            } else {
                ret = 0;
            }
        }
    } else {
        printf("%s !!! %d err=%d\n", _LOG_TAG, __LINE__, err);
    }

    printf("%s alsa pcm read recovery succeed!\n", _LOG_TAG);
    return ret;

REINIT:
    capture_alsa_close(alsa_context);
    ret = capture_alsa_open(alsa_context);
    return ret;
}

void capture_alsa_dump_config(wm_alsa_context_t *alsa_context)
{
    if (alsa_context) {
        printf("########## capture_alsa_dump_config ##########\n");
        printf("# device name: %s\n", alsa_context->device_name);
        printf("# rate: %d\n", alsa_context->rate);
        printf("# channel: %d\n", alsa_context->channels);
        printf("# bps: %d\n", alsa_context->bits_per_sample);
        printf("# latency: %d\n", alsa_context->latency_ms);
        printf("# block: %d\n", alsa_context->block_mode);
        printf("##############################################\n");
    }
}

wm_alsa_context_t *alsa_context_create(char *config_file)
{
    wm_alsa_context_t *alsa_context = NULL;
    alsa_context = (wm_alsa_context_t *)malloc(sizeof(wm_alsa_context_t));
    if (alsa_context) {
        memset((void *)alsa_context, 0, sizeof(wm_alsa_context_t));

        /* TODO: load config from lpcfg.json */
        /* TODO: set default config */
    }
    return alsa_context;
}

void alsa_context_destroy(wm_alsa_context_t *context)
{
    if (context) {
        if (context->device_name)
            free(context->device_name);
        capture_alsa_close(context);
        free(context);
        context = NULL;
    }
}