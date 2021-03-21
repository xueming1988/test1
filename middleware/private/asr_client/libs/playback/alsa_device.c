/*************************************************************************
    > File Name: alsa_device.c
    > Author:
    > Mail:
    > Created Time: Tue 21 Nov 2017 02:35:55 AM EST
 ************************************************************************/

#include <stdio.h>
#include <string.h>
#include <alsa/asoundlib.h>

#include "alsa_device.h"

#ifdef X86
// DO Nothing
#else
#include "wm_util.h"
#ifdef NAVER_LINE
#include "common_flags.h"
#else
extern WIIMU_CONTEXT *g_wiimu_shm;
#endif
#endif

#define os_power_on_tick() tickSincePowerOn()

#define ALSA_DEVICE_DEBUG(fmt, ...)                                                                \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d][ALSA_DEV] " fmt,                                    \
                (int)(os_power_on_tick() / 3600000), (int)(os_power_on_tick() % 3600000 / 60000),  \
                (int)(os_power_on_tick() % 60000 / 1000), (int)(os_power_on_tick() % 1000),        \
                ##__VA_ARGS__);                                                                    \
    } while (0)

struct _alsa_device_s {
    // for sound card info
    int fmt;
    int sample_rate;
    int channels;
    int fix_volume;
    snd_pcm_t *snd_handle;
#ifdef X86
// DO Nothing
#else
    WIIMU_CONTEXT *wiimu_shm;
#endif
};

#if defined(A98_EATON)
extern int g_prompt_on;
#endif

alsa_device_t *alsa_device_open(const char *device_name)
{
    int ret = -1;
    alsa_device_t *self = NULL;

    self = (alsa_device_t *)calloc(1, sizeof(alsa_device_t));
    if (self) {
        // device = "default";
        ALSA_DEVICE_DEBUG("open device start ++++\n");
        ret = snd_pcm_open(&self->snd_handle, device_name, SND_PCM_STREAM_PLAYBACK, 0);
        if (ret != 0) {
            ALSA_DEVICE_DEBUG("open PCM device failed  %s !\n", snd_strerror(ret));
            free(self);
            self = NULL;
        } else {
// set the sound card to noblock to play
#if defined(X86)
// DO Nothing
#else
#if defined(A98) || defined(A88) || defined(A76)
            snd_pcm_info_set(self->snd_handle, SNDSRC_ALEXA_TTS);
#endif
#ifdef NAVER_LINE
            self->wiimu_shm = (WIIMU_CONTEXT *)common_flags_get();
#else
            self->wiimu_shm = g_wiimu_shm;
#endif
#endif
            snd_pcm_nonblock(self->snd_handle, 0);
            ALSA_DEVICE_DEBUG("open device success\n");
        }
    }
#if defined(A98_EATON)
    {
        int counter = 4 * 100;
        while (g_prompt_on && counter-- > 0) {
            usleep(10000);
        }
    }
#endif

    return self;
}

int alsa_device_init(alsa_device_t *self, int fmt, unsigned int *samplerate,
                     unsigned short channels)
{
    int ret = 0;
    int dir = 0;
    snd_pcm_uframes_t frames = 2048;
    snd_pcm_hw_params_t *pcm_params = NULL;
    snd_pcm_sw_params_t *pcm_params_sw = NULL;

    if (self && self->snd_handle) {
        if (self->fmt == fmt && self->sample_rate == (int)*samplerate &&
            self->channels == (int)channels) {
            return 0;
        }

        ALSA_DEVICE_DEBUG("device_init begin at fmt(%d) channels(%d) samplerate(%d) ticks=%lld\n",
                          fmt, channels, *samplerate, os_power_on_tick());
        snd_pcm_hw_params_alloca(&pcm_params);
        ret = snd_pcm_hw_params_any(self->snd_handle, pcm_params);
        if (ret < 0) {
            ALSA_DEVICE_DEBUG("snd_pcm_hw_params_any error: %s !\n", snd_strerror(ret));
            return ret;
        }

        ret = snd_pcm_hw_params_set_access(self->snd_handle, pcm_params,
                                           SND_PCM_ACCESS_RW_INTERLEAVED);
        if (ret < 0) {
            ALSA_DEVICE_DEBUG("snd_pcm_params_set_access error: %s !\n", snd_strerror(ret));
            return ret;
        }

        ret = snd_pcm_hw_params_set_format(self->snd_handle, pcm_params, (snd_pcm_format_t)fmt);
        if (ret < 0) {
            ALSA_DEVICE_DEBUG("snd_pcm_params_set_format error: %s !\n", snd_strerror(ret));
            return ret;
        }
        self->fmt = fmt;

        ret = snd_pcm_hw_params_set_channels(self->snd_handle, pcm_params, channels);
        if (ret < 0) {
            ALSA_DEVICE_DEBUG("snd_pcm_params_set_channels error: %s !\n", snd_strerror(ret));
            return ret;
        }
        self->channels = channels;

        ret = snd_pcm_hw_params_set_rate_near(self->snd_handle, pcm_params, samplerate, &dir);
        if (ret < 0) {
            ALSA_DEVICE_DEBUG("snd_pcm_hw_params_set_rate_near error: %s !\n", snd_strerror(ret));
            return ret;
        }
        self->sample_rate = *samplerate;

        ret = snd_pcm_hw_params(self->snd_handle, pcm_params);
        if (ret < 0) {
            ALSA_DEVICE_DEBUG("snd_pcm_hw_params error: %s !\n", snd_strerror(ret));
            return ret;
        }

        snd_pcm_sw_params_alloca(&pcm_params_sw);
        /* set software parameters */
        ret = snd_pcm_sw_params_current(self->snd_handle, pcm_params_sw);
        if (ret < 0) {
            ALSA_DEVICE_DEBUG("unable to determine current software params: %s !",
                              snd_strerror(ret));
        }

        ret = snd_pcm_hw_params_get_period_size(pcm_params, &frames, NULL);
        if (ret < 0) {
            ALSA_DEVICE_DEBUG("unable set start threshold: %s !", snd_strerror(ret));
        }

        ret = snd_pcm_sw_params_set_start_threshold(self->snd_handle, pcm_params_sw, frames * 2);
        if (ret < 0) {
            ALSA_DEVICE_DEBUG("unable set start threshold: %s !", snd_strerror(ret));
        }

        ret = snd_pcm_sw_params_set_silence_size(self->snd_handle, pcm_params_sw, 4);
        if (ret < 0) {
            ALSA_DEVICE_DEBUG("unable set start threshold: %s !", snd_strerror(ret));
        }

        ret = snd_pcm_sw_params(self->snd_handle, pcm_params_sw);
        if (ret < 0) {
            ALSA_DEVICE_DEBUG("unable set sw params: %s !", snd_strerror(ret));
        }
#if defined(A31)
        snd_pcm_mix_vol(self->snd_handle, 0, 0, 0);
#endif
        snd_pcm_start(self->snd_handle);

#ifndef NAVER_LINE
#ifdef X86
// DO Nothing
#else
#if defined(A98) || defined(A88)
        if (self->wiimu_shm) {
            WMUtil_Snd_Ctrl_SetProcessFadeStep(self->wiimu_shm, 20, SNDSRC_ALEXA_TTS);
            WMUtil_Snd_Ctrl_SetProcessMute(self->wiimu_shm, 0, SNDSRC_ALEXA_TTS, 1);
            WMUtil_Snd_Ctrl_SetProcessSelfVol(self->wiimu_shm, 100, SNDSRC_ALEXA_TTS);
        }
#elif defined(A76)
        if (self->wiimu_shm) {
            WMUtil_Snd_Ctrl_SetProcessMute(self->wiimu_shm, 0, SNDSRC_ALEXA_TTS);
        }
#endif
#endif
#endif
        ALSA_DEVICE_DEBUG("device_init end at fmt(%d) channels(%d) samplerate(%d) ticks=%lld\n",
                          fmt, channels, *samplerate, os_power_on_tick());
    }
    return 0;
}

int alsa_device_uninit(alsa_device_t *self)
{
    int ret = 0;

    if (self) {
        if (self->snd_handle) {
            ALSA_DEVICE_DEBUG("device_close start at ticks=%lld\n", os_power_on_tick());
            snd_pcm_drain(self->snd_handle);
#if defined(A31)
            snd_pcm_mix_vol(self->snd_handle, 0, 0, 0);
#endif
            snd_pcm_close(self->snd_handle);
            ALSA_DEVICE_DEBUG("device_close at end ticks=%lld\n", os_power_on_tick());
            self->snd_handle = NULL;
        }

#ifndef NAVER_LINE
#ifdef X86
// DO Nothing
#else
#if defined(A98) || defined(A88)
        if (self->wiimu_shm) {
            WMUtil_Snd_Ctrl_SetProcessMute(self->wiimu_shm, 1, SNDSRC_ALEXA_TTS, 1);
            WMUtil_Snd_Ctrl_SetProcessVol(self->wiimu_shm, 100, SNDSRC_ALEXA_TTS);
        }
#elif defined(A76)
        if (self->wiimu_shm) {
            WMUtil_Snd_Ctrl_SetProcessMute(self->wiimu_shm, 1, SNDSRC_ALEXA_TTS);
        }
#endif
#endif
#endif
        free(self);
    }

    return ret;
}

int alsa_device_play_silence(alsa_device_t *self, int time_ms)
{
    int err = 0;
    int period_size = 8192;
    int silent_size = 0;
    int frames = 0;
    int oneshot = 0;
    int offset = 0;
    char *silent = NULL;

    if (!self) {
        ALSA_DEVICE_DEBUG("%s input error\n", __FUNCTION__);
        return -1;
    }

    silent_size = time_ms * self->sample_rate * 4 / 1000;
    silent = (char *)malloc(silent_size);
    if (!silent) {
        ALSA_DEVICE_DEBUG("%s malloc failed\n", __FUNCTION__);
        return -1;
    }
    memset(silent, 0, silent_size);
    frames = silent_size / 4;
    while (frames > 0) {
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

    return 0;
}

int alsa_device_play_pcm(alsa_device_t *self, unsigned short *output_ptr, int len)
{
    int err = 0;
    int err_cnt = 0;
    int less_len = 0;

    if (self && self->snd_handle) {
        less_len = len;
        while (less_len > 0) {
#ifdef X86
// DO Nothing
#else
            if (self->wiimu_shm && self->wiimu_shm->silence_ota)
                memset(output_ptr, 0, less_len * 2);
#endif
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
                ALSA_DEVICE_DEBUG("play pcm error err(%d)\n", err);
                err = -1;
                break;
            }
        }
    }

    return err;
}
