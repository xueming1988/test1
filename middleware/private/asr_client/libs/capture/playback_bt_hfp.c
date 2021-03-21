/*************************************************************************
    > File Name: playback_bt_hfp.c
    > Author:
    > Mail:
    > Created Time: jun 1  2019 11:23:39
 ************************************************************************/

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include <semaphore.h>

#include "wm_util.h"
#include "ccaptureGeneral.h"
#include "playback_bt_hfp.h"

int CCaptCosume_bt_hfp_copy(char *buf, int buf_size, void *priv);
int CCaptCosume_bt_hfp_playback_init(void *priv);
int CCaptCosume_bt_hfp_playback_deinit(void *priv);

extern WIIMU_CONTEXT *g_wiimu_shm;
int g_call_status = BT_HFP_CALL_END;
unsigned int g_bt_play_period_bytes = 0;
static char *g_hfp_audio_buf = NULL;
static int g_hfp_audio_size = 4096;

CAPTURE_COMUSER bt_HFP_Cosume_Playback = {
    .enable = 0,
    .pre_process = 0,
    .id = AUDIO_ID_BT_HFP,
    .cCapture_dataCosume = CCaptCosume_bt_hfp_copy,
    .cCapture_init = CCaptCosume_bt_hfp_playback_init,
    .cCapture_deinit = CCaptCosume_bt_hfp_playback_deinit,
    .priv = 0, /*  void* actually (snd_pcm_t*)  */
};

static int _hfp_capture_pre_send(char *src, int src_len)
{
    short *p_src = (short *)src;
    short *p_dst = (short *)g_hfp_audio_buf;
    int len = src_len / 2;
    int i = 0;

    if ((src_len * 2) > g_hfp_audio_size) {
        free(g_hfp_audio_buf);
        g_hfp_audio_buf = (char *)malloc(src_len * 2);
        g_hfp_audio_size = src_len * 2;
    }
    while (len--) {
        *p_dst = *p_src;
        *(p_dst + 1) = *p_src;
        p_src += 1;
        p_dst += 2;
    }
    return src_len * 2;
}

int CCaptCosume_bt_hfp_copy(char *buf, int buf_size, void *priv)
{
    int ret = 0;
    int write_len = 0;
    snd_pcm_sframes_t frames_to_write = 0;
    snd_pcm_t *p_handle = (snd_pcm_t *)priv;
    char *p_buffer = buf;
    int size = buf_size;
    unsigned int size_of_frame = BT_ALSA_CHANNEL * 16 / 8;

    if (!p_handle || g_call_status != BT_HFP_CALL_STARTUP) {
        return 0;
    }
    if (g_wiimu_shm->privacy_mode)
        return size;
    // wiimu_log(1, 0, 0, 0, "%s: enable %d, size %d", __func__, bt_HFP_Cosume_Playback.enable,
    // size);
    if (bt_HFP_Cosume_Playback.id == AUDIO_ID_BT_HFP) {
        size = _hfp_capture_pre_send(buf, buf_size);
        p_buffer = g_hfp_audio_buf;
        // wiimu_log(1, 0, 0, 0, "%s: src len %d, dst len %d", __func__, buf_size, size);
    }
    while (bt_HFP_Cosume_Playback.enable && size > 0) {
        write_len = g_bt_play_period_bytes <= size ? g_bt_play_period_bytes : size;
        frames_to_write = write_len / size_of_frame;
        if (frames_to_write <= 0) {
            break;
        }

        // wiimu_log(1, 0, 0, 0, "%s: frames_to_write %d", __func__, frames_to_write);
        ret = snd_pcm_writei(p_handle, p_buffer, frames_to_write);
        if (ret == -EPIPE) {
            wiimu_log(1, 0, 0, 0, "%s -EPIPE: -%d", __func__, EPIPE);
            if ((ret = snd_pcm_prepare(p_handle)) < 0) {
                wiimu_log(1, 0, 0, 0, "%s snd_pcm_prepare error: %s", __func__, snd_strerror(ret));
            }
        } else if (ret < 0) {
            // wiimu_log(1, 0, 0, 0, "%s snd_pcm_writei frames: %ld, actual: %ld", __func__,
            // frames_to_write,
            // ret);
            ret = snd_pcm_recover(p_handle, ret, 0);
            // Still an error, need to exit.
            if (ret < 0) {
                wiimu_log(1, 0, 0, 0, "%s Error occured while sending to phone: %s", __func__,
                          snd_strerror(ret));
                break;
            }
        } else if ((ret > 0) && (ret < frames_to_write)) {
            wiimu_log(1, 0, 0, 0, "%s Short write (expected %li, wrote %li)", __func__,
                      (long)frames_to_write, ret);
        }
        if (ret > 0) {
            // wiimu_log(1, 0, 0, 0, "%s: ret %d", __func__, ret);
            size = size - ret * size_of_frame;
            p_buffer = p_buffer + ret * size_of_frame;
        }
    }
    // wiimu_log(1, 0, 0, 0, "%s: exit size %d", __func__, size);

    return size;
}

static int bt_hfp_set_pcm_para(snd_pcm_t *handle, unsigned int *p_rate, unsigned int channel_num,
                               unsigned int divided_period)
{
    int err;
    snd_pcm_hw_params_t *params;
    snd_pcm_sw_params_t *swparams;
    unsigned int buffer_time, period_time;
    snd_pcm_uframes_t buffer_frames;
    int period_frames = 0;
    int byte_per_frame = 2 * 1;

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_sw_params_alloca(&swparams);
    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0) {
        wiimu_log(1, 0, 0, 0, "%s Can not configure this PCM device", __func__);
        return -1;
    }
    err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        wiimu_log(1, 0, 0, 0, "%s: Failed to snd_pcm_hw_params_set_access", __func__);
        return -1;
    }
    err = snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
        wiimu_log(1, 0, 0, 0, "%s: Failed to set SND_PCM_FORMAT_S16_LE", __func__);
        return -1;
    }
    err = snd_pcm_hw_params_set_channels(handle, params, channel_num);
    if (err < 0) {
        wiimu_log(1, 0, 0, 0, "%s: Failed to APP_HS_CHANNEL=%d", __func__, channel_num);
        return -1;
    }
    wiimu_log(1, 0, 0, 0, "%s: snd_pcm_hw_params_set_rate_near=%d", __func__, *p_rate);
    err = snd_pcm_hw_params_set_rate_near(handle, params, p_rate, 0);
    if (err < 0) {
        wiimu_log(1, 0, 0, 0, "%s: Failed to set PCM device to sample rate =%d", __func__, *p_rate);
        return -1;
    }
    wiimu_log(1, 0, 0, 0, "%s: bt_capture_rate =%d", __func__, *p_rate);

    snd_pcm_hw_params_get_buffer_time_max(params, &buffer_time, 0);
    if (buffer_time > 500000)
        buffer_time = 500000;
    period_time = buffer_time / divided_period;
    err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, 0);
    if (err < 0) {
        wiimu_log(1, 0, 0, 0, "%s: Failed to set PCM device to buffer time=%d", __func__,
                  buffer_time);
        return -1;
    }

    err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, 0);
    if (err < 0) {
        wiimu_log(1, 0, 0, 0, "%s: Failed to set PCM device to period time =%d", __func__,
                  period_time);
        return -1;
    }
    wiimu_log(1, 0, 0, 0, "%s: buffer_time=%d period_time=%d", __func__, buffer_time, period_time);

    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
        wiimu_log(1, 0, 0, 0, "%s: unable to set hw parameters", __func__);
        return -1;
    }
    err = snd_pcm_get_params(handle, &buffer_frames, &period_frames);
    if (buffer_frames <= period_frames) {
        wiimu_log(1, 0, 0, 0, "%s: bt_capture_rate buffer_frames<=period_frames", __func__);
        return -1;
    }
    wiimu_log(1, 0, 0, 0, "%s: bt_handle_capture buffer_frames=%d period_frame=%d", __func__,
              buffer_frames, period_frames);
    if (err < 0) {
        wiimu_log(1, 0, 0, 0, "%s: bt_handle_capture snd_pcm_get_params fault!", __func__);
        return -1;
    }
    snd_pcm_sw_params_current(handle, swparams);
    err = snd_pcm_sw_params_set_start_threshold(handle, swparams, period_frames);
    if (err < 0) {
        wiimu_log(1, 0, 0, 0, "%s: snd_pcm_sw_params_set_start_threshold error!", __func__);
        return -1;
    }
    err = snd_pcm_sw_params(handle, swparams);
    if (err < 0) {
        wiimu_log(1, 0, 0, 0, "%s: snd_pcm_sw_params error!", __func__);
        return -1;
    }
    g_bt_play_period_bytes = period_frames * byte_per_frame;
    snd_pcm_hw_params_get_channels(params, &err);
    wiimu_log(1, 0, 0, 0, "%s: get  channel=%d, g_bt_play_period_bytes %d", __func__, err,
              g_bt_play_period_bytes);
    return 0;
}

int CCaptCosume_bt_hfp_playback_init(void *priv)
{
    int err = 0;
    unsigned int sample_rate = 16000;
    CAPTURE_COMUSER *p_capture = (CAPTURE_COMUSER *)priv;

    if (p_capture->priv == NULL) {
        err =
            snd_pcm_open((snd_pcm_t **)&(p_capture->priv), BT_ALSA_HW, SND_PCM_STREAM_PLAYBACK, 0);
        if (err < 0 || p_capture->priv == NULL) {
            wiimu_log(1, 0, 0, 0, "capture playback open error: %s", snd_strerror(err));
            if (p_capture->priv) {
                snd_pcm_close((snd_pcm_t *)p_capture->priv);
            }
            return -1;
        } else if (bt_hfp_set_pcm_para((snd_pcm_t *)p_capture->priv, &(sample_rate),
                                       BT_ALSA_CHANNEL, 4) < 0) {
            wiimu_log(1, 0, 0, 0, "====%s set_playback_params fail====", __FUNCTION__);
            snd_pcm_close((snd_pcm_t *)p_capture->priv);
            p_capture->priv = NULL;
            return -1;
        }
    } else {
        wiimu_log(1, 0, 0, 0, "%s alsa_playback_handle  used", __func__);
        return -1;
    }
    wiimu_log(1, 0, 0, 0, "%s: id %d, pre_process %d, channels %d", __func__, p_capture->id,
              p_capture->pre_process, BT_ALSA_CHANNEL);
    g_hfp_audio_buf = (char *)malloc(g_hfp_audio_size);
    return 0;
}

int CCaptCosume_bt_hfp_playback_deinit(void *priv)
{
    CAPTURE_COMUSER *p_capture = (CAPTURE_COMUSER *)priv;

    if (p_capture->priv) {
        snd_pcm_close(p_capture->priv);
        p_capture->priv = NULL;
    }
    if (g_hfp_audio_buf) {
        free(g_hfp_audio_buf);
    }

    return 0;
}

int Is_HFP_Running() { return bt_HFP_Cosume_Playback.enable; }

static void bt_hfp_cap_start(void)
{
    wiimu_log(1, 0, 0, 0, "bt_hfp_cap_start_call+++");
    CCaptRegister(&bt_HFP_Cosume_Playback);
    wiimu_log(1, 0, 0, 0, "bt_hfp_cap_start_call---");
}
static void bt_hfp_cap_stop(void)
{
    wiimu_log(1, 0, 0, 0, "bt_hfp_cap_stop_call+++");
    if (bt_HFP_Cosume_Playback.priv)
        CCaptUnregister(&bt_HFP_Cosume_Playback);
    wiimu_log(1, 0, 0, 0, "bt_hfp_cap_stop_call---");
}

void bluetooth_hfp_start_play(void)
{

    if (bt_HFP_Cosume_Playback.cCapture_init((void *)&(bt_HFP_Cosume_Playback)) < 0) {
        wiimu_log(1, 0, 0, 0, "bluetooth hfp cCapture_init  failed!");
        return;
    }
    bt_hfp_cap_start();

    bt_HFP_Cosume_Playback.enable = 1;
    g_call_status = BT_HFP_CALL_STARTUP;
}

void bluetooth_hfp_stop_play(void)
{
    g_call_status = BT_HFP_CALL_END;
    bt_HFP_Cosume_Playback.enable = 0;

    bt_hfp_cap_stop();
    bt_HFP_Cosume_Playback.cCapture_deinit((void *)&(bt_HFP_Cosume_Playback));
}
