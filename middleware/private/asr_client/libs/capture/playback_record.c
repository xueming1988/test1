/*************************************************************************
    > File Name: playback_record.c
    > Author:
    > Mail:
    > Created Time: Sat 12 May 2018 11:23:39 PM EDT
 ************************************************************************/

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <alsa/asoundlib.h>

#include "wm_util.h"
#include "ccaptureGeneral.h"
#include "playback_record.h"

#if CXDISH_6CH_SAMPLE_RATE
#undef USB_CAPTURE_SAMPLERATE
#define USB_CAPTURE_SAMPLERATE CXDISH_6CH_SAMPLE_RATE
#endif

#define SAMPLE_RATE_FAR (USB_CAPTURE_SAMPLERATE)

extern WIIMU_CONTEXT *g_wiimu_shm;

static snd_pcm_t *alsa_playback_handle = NULL;
static int g_playback_testing = 0;
static snd_pcm_hw_params_t *playback_hw_params = NULL;
static snd_pcm_sw_params_t *playback_sw_params = NULL;

CAPTURE_COMUSER Cosume_Playback = {
    .enable = 0,
    .cCapture_dataCosume = CCaptCosume_Playback,
    .cCapture_init = CCaptCosume_initPlayback,
    .cCapture_deinit = CCaptCosume_deinitPlayback,
};

static int set_playback_params(snd_pcm_t *handle)
{
    int err;
    int dir = 0;
    int sampling_rate = 44100;
    int chunk = 8192;
    int bufsize = 32768;
    snd_pcm_uframes_t frames = 32;

#ifdef USB_CAPDSP_MODULE
    sampling_rate = SAMPLE_RATE_FAR;
#else
    if (g_wiimu_shm->priv_cap1 & (1 << CAP1_USB_RECORDER)) {
        sampling_rate = SAMPLE_RATE_FAR;
    }
#endif

    snd_pcm_hw_params_alloca(&playback_hw_params);
    snd_pcm_sw_params_alloca(&playback_sw_params);
    snd_pcm_hw_params_any(handle, playback_hw_params);
    snd_pcm_hw_params_set_access(handle, playback_hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, playback_hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, playback_hw_params, 2);
    snd_pcm_hw_params_set_rate_near(handle, playback_hw_params, (unsigned int *)&sampling_rate,
                                    &dir);
    snd_pcm_hw_params_set_period_size_near(handle, playback_hw_params, &frames, &dir);

    err = snd_pcm_hw_params(handle, playback_hw_params);
    if (err < 0) {
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(err));
        return -1;
    }

    snd_pcm_hw_params_get_buffer_size(playback_hw_params, &frames);
    snd_pcm_sw_params_current(handle, playback_sw_params);

    snd_pcm_hw_params_get_buffer_size(playback_hw_params, &chunk);
    snd_pcm_hw_params_get_period_size(playback_hw_params, &bufsize, NULL);
    snd_pcm_sw_params_set_start_threshold(handle, playback_sw_params, bufsize);
    printf("capture playback chunk=%d, bufsize=%d\n", chunk, bufsize);

    err = snd_pcm_nonblock(handle, 1);
    if (err < 0) {
        printf("capture playback nonblock setting error: %s\n", snd_strerror(err));
        return -1;
    }

    return 0;
}

int CCaptCosume_Playback(char *buf, int size, void *priv)
{
    int pcm_size = size / 4;
    int pcm_write = 0;
    int res = 0;
    int err = 0;

    if (Cosume_Playback.enable == 1) {
        if (alsa_playback_handle == NULL) {
            err = snd_pcm_open(&alsa_playback_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
            if (err < 0 || alsa_playback_handle == NULL) {
                printf("capture playback open error: %s", snd_strerror(err));
                return size;
            }

            if (set_playback_params(alsa_playback_handle) < 0) {
                printf("====%s %d set_playback_params fail====\n", __FUNCTION__, __LINE__);
                snd_pcm_close(alsa_playback_handle);
                alsa_playback_handle = NULL;
                return size;
            }
        }

        while (pcm_size) {
            int avail = snd_pcm_avail_update(alsa_playback_handle);
            int len = pcm_size;
            if (len > avail)
                len = avail;
            if (avail <= 0)
                usleep(10000); // printf("CCaptCosume_Playback avail = %d\n", avail);
            else {
                res = snd_pcm_writei(alsa_playback_handle, buf + pcm_write, len);
                if (res < 0) {
                    res = snd_pcm_recover(alsa_playback_handle, res, 0);
                    res = 0;
                }

                if (res > 0) {
                    pcm_size -= len;
                    pcm_write += len * 4;
                }
            }
        }
    }

    return size;
}

int CCaptCosume_initPlayback(void *priv)
{
    g_playback_testing = 0;
    Cosume_Playback.enable = 1;
    return 0;
}

int CCaptCosume_deinitPlayback(void *priv)
{
    Cosume_Playback.enable = 0;
    g_playback_testing = 0;
    return 0;
}

void StartRecordTest(void) { CCaptCosume_initPlayback(&Cosume_Playback); }

void StopRecordTest(void) { CCaptCosume_deinitPlayback(&Cosume_Playback); }

void ClosePlaybackHandle(void)
{
    if (alsa_playback_handle) {
        printf("====%s %s %d ====\n", __FILE__, __FUNCTION__, __LINE__);
        snd_pcm_drain(alsa_playback_handle);
        snd_pcm_close(alsa_playback_handle);
        alsa_playback_handle = NULL;
    }
}

void Notify_IOGuard(void)
{
    if (g_playback_testing == 0) {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCUTLKOFF",
                                 strlen("GNOTIFY=MCUTLKOFF"), NULL, NULL, 0);
        g_playback_testing = 1;
    } else if (g_playback_testing == 1) {
        g_playback_testing = 0;
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCUTLKON",
                                 strlen("GNOTIFY=MCUTLKON"), NULL, NULL, 0);
    }
}
