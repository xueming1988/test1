/**
 * Copyright (c) 2013-2014 Spotify AB
 *
 * This file is part of the Spotify Embedded SDK examples suite.
 */
#include <alsa/asoundlib.h>
#include <pthread.h>

#include "spotify_embedded.h"
#include "audio.h"
#include "wm_util.h"
#include "wm_fadecache.h"

#define ALSA_DEBUG(fmt, ...)                                                                       \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d][Spotify ESDK][ALSA] " fmt,                          \
                (int)(tickSincePowerOn() / 3600000), (int)(tickSincePowerOn() % 3600000 / 60000),  \
                (int)(tickSincePowerOn() % 60000 / 1000), (int)(tickSincePowerOn() % 1000),        \
                ##__VA_ARGS__);                                                                    \
    } while (0)

extern WIIMU_CONTEXT *g_wiimu_shm;

void wm_cal_gpio_toggle(void);

static struct SpSampleFormat current_sample_format = {0};

static pthread_mutex_t alsa_mutex = PTHREAD_MUTEX_INITIALIZER;

#ifdef WMRMPLAY_ENABLE
#include "wmrm_api.h"
wmrm_snd_t g_wmrm_sound = 0;
wmrm_sound_param g_param = {0};
extern int g_spotify_active;
static int spotify_play_flag = 0;
#else
static snd_pcm_t *alsa_handler = 0;
static snd_pcm_hw_params_t *alsa_hwparams;
const char *alsa_device_name = "default";
FadeCache g_spotify_fc;
#endif

static size_t bytes_per_sample = 4;
static size_t outburst = 0;

static int first_data = 0;
static int g_pause = 0;

typedef struct {
    short sample; // Signed 16-bit sample
} SAMPLE_16_MONO;

#define MAXCHANNELS 2

static unsigned int g_ConvBaseSampleRate = 44100;
static long g_ConvPos;
static unsigned int g_ConvBaseSampleRateInverse;
static unsigned int g_ConvClientRate;
static int m_PrevSamp[MAXCHANNELS];
static int m_CurrSamp[MAXCHANNELS];
static unsigned char *g_PcmCovertionBuffer = NULL;
static unsigned char *g_PcmLastConv = NULL;

static unsigned char *PcmRateConvertionM16(unsigned char *pcmSource, int len,
                                           unsigned char *pcmDest);
#if defined(AUDIO_SRC_OUTPUT)
extern void updateDmixConfDefault(void);
#endif
/*
    plays 'len' bytes of 'data'
    returns: number of bytes played
    modified last at 29.06.02 by jp
    thanxs for marius <marius@rospot.com> for giving us the light ;)
*/
static int play(void *data, int len)
{
    snd_pcm_sframes_t res = 0;

    if (len == 0)
        return 0;

#ifdef WMRMPLAY_ENABLE
    if (g_wmrm_sound) {
        res = wmrm_sound_write(g_wmrm_sound, data, len);
        if (res >= 0)
            res /= bytes_per_sample;
    }
    return res < 0 ? 0 : res;
#else
    int num_frames;
    snd_pcm_sframes_t result = 0;

    if (alsa_handler) {
        num_frames = len / bytes_per_sample;
        result = fadeCache_play(&g_spotify_fc, bytes_per_sample, num_frames, data);
    }
    return res < 0 ? res : result;
#endif
}

SpError alsa_audio_deinit(void)
{
    long long time = 0;
    time = tickSincePowerOn();
    pthread_mutex_lock(&alsa_mutex);
#ifdef WMRMPLAY_ENABLE
    if (g_wmrm_sound)
        wmrm_sound_deinit(g_wmrm_sound);
    g_wmrm_sound = 0;
#else
    if (alsa_handler && g_spotify_fc.cache_run) {
        fadeCache_uninit(&g_spotify_fc, 1);
    }
    if (alsa_handler) {
        snd_pcm_close(alsa_handler);
    }
    alsa_handler = NULL;
#endif
    if (g_PcmCovertionBuffer)
        free(g_PcmCovertionBuffer);

    g_PcmCovertionBuffer = NULL;
    memset(&current_sample_format, 0x0, sizeof(current_sample_format));
    first_data = 0;
    pthread_mutex_unlock(&alsa_mutex);
    ALSA_DEBUG("alsa uninit useT(%lld)-- \n", tickSincePowerOn() - time);

    return kSpErrorOk;
}

void *alsa_audio_init(const struct SpSampleFormat *sample_format)
{
    snd_pcm_uframes_t frames = 1024;

    pthread_mutex_lock(&alsa_mutex);
#ifdef WMRMPLAY_ENABLE
    g_param.block = 0;
    g_param.format = SND_PCM_FORMAT_S16_LE;
    g_param.stream_id = stream_id_spotify;
    g_param.sample_rate = g_ConvBaseSampleRate;
    g_param.sample_channel = MAXCHANNELS;
    g_param.play_leadtime = g_wiimu_shm->slave_num?800:200; //less time the slave may lag when the network is not good and stable enough
#if defined(SPTOFY_CERTIFICATION)
    g_param.play_leadtime = 200;
#endif
    g_param.ctrl_leadtime = 50;
    if (g_wmrm_sound)
        wmrm_sound_deinit(g_wmrm_sound);
    g_wmrm_sound = wmrm_sound_init(&g_param);
    outburst = frames * bytes_per_sample;
#else
    int rc, dir = 0;
#ifdef AUDIO_SRC_OUTPUT
    updateDmixConfDefault();
#endif
    rc = snd_pcm_open(&alsa_handler, alsa_device_name, SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        ALSA_DEBUG("unable to open pcm device: %s\n", snd_strerror(rc));
    }
#if !defined(SAMPLING_RATE_176400) // this is 32bit out, do not use alsa fade feature
    snd_pcm_info_set(alsa_handler, SNDSRC_SPOTIFY);
#endif

    snd_pcm_hw_params_alloca(&alsa_hwparams);
    snd_pcm_hw_params_any(alsa_handler, alsa_hwparams);
    snd_pcm_hw_params_set_access(alsa_handler, alsa_hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(alsa_handler, alsa_hwparams, SND_PCM_FORMAT_S16);
    snd_pcm_hw_params_set_channels(alsa_handler, alsa_hwparams, MAXCHANNELS);
    snd_pcm_hw_params_set_rate_near(alsa_handler, alsa_hwparams,
                                    (unsigned int *)&g_ConvBaseSampleRate, &dir);
    snd_pcm_hw_params_set_period_size(alsa_handler, alsa_hwparams, frames, dir);
    rc = snd_pcm_hw_params(alsa_handler, alsa_hwparams);
    if (rc < 0) {
        ALSA_DEBUG("unable to set hw parameters: %s\n", snd_strerror(rc));
    }

    snd_pcm_hw_params_get_period_size(alsa_hwparams, &frames, NULL);
    snd_pcm_hw_params_set_buffer_size(alsa_handler, alsa_hwparams, 8192);
    outburst = frames * bytes_per_sample;
    ALSA_DEBUG("outburst %d \r\n", outburst);

    snd_pcm_sw_params_t *sw_params;
    snd_pcm_hw_params_get_buffer_size(alsa_hwparams, &frames);
    printf("frames %lu \r\n", frames);
    snd_pcm_sw_params_alloca(&sw_params);
    snd_pcm_sw_params_current(alsa_handler, sw_params);
    snd_pcm_sw_params_set_start_threshold(alsa_handler, sw_params, 2048);

    if (alsa_handler) {
        fadeCache_init(&g_spotify_fc, alsa_handler, SNDSRC_SPOTIFY, g_wiimu_shm,
                       g_ConvBaseSampleRate, SND_PCM_FORMAT_S16, outburst, bytes_per_sample);
    }
#endif
    pthread_mutex_unlock(&alsa_mutex);
    ALSA_DEBUG("alsa_audio_init --\n");

    return kSpErrorOk;
}

size_t alsa_audio_data(const int16_t *samples, size_t sample_count,
                       const struct SpSampleFormat *sample_format, uint32_t *samples_buffered,
                       void *context)
{
    uint32_t buffered;
    size_t accepted = 0;
    int i;
    void *data;
    int len;

    if (g_pause || (samples == NULL) || (sample_count == 1) || (sample_format == NULL)) {
        return 0;
    }

    if (sample_count == 0)
        goto end;

    // Re-initialize ALSA if the audio format changed
    if (!first_data) {
        ALSA_DEBUG("first on_audio_data at %lld ms \r\n", tickSincePowerOn());
        wm_cal_gpio_toggle();
        first_data = 1;
    }
#ifdef WMRMPLAY_ENABLE
    pthread_mutex_lock(&alsa_mutex);
    if (!g_wmrm_sound && g_spotify_active) {
        fprintf(stderr, "spotify alsa_audio_data init, input samplerate=%d, input channel=%d \n",
                sample_format->sample_rate, sample_format->channels);
        bytes_per_sample = 4;
        g_param.block = 0;
        g_param.format = SND_PCM_FORMAT_S16_LE;
        g_param.stream_id = stream_id_spotify;
        g_param.sample_rate = g_ConvBaseSampleRate;
        g_param.sample_channel = MAXCHANNELS;
        g_param.play_leadtime = 400;
#if defined(SPTOFY_CERTIFICATION)
        g_param.play_leadtime = 200;
#endif
        g_param.ctrl_leadtime = 50;
        g_wmrm_sound = wmrm_sound_init(&g_param);
    }
    pthread_mutex_unlock(&alsa_mutex);

    if (g_wmrm_sound) {
        int delay = wmrm_sound_get_delay(g_wmrm_sound);
        if (delay > 25000)
            goto end;
    }

#else
    snd_pcm_sframes_t delay;
    if (alsa_handler == NULL) {
        alsa_audio_deinit();
        if (alsa_audio_init(sample_format))
            goto end;
        bytes_per_sample = 4;
    }
#endif

    if (memcmp(sample_format, &current_sample_format, sizeof(struct SpSampleFormat)) != 0) {
        current_sample_format = *sample_format;
        if (current_sample_format.channels != 2) {
            g_ConvBaseSampleRateInverse =
                ((unsigned int)((((long long)1 << 32) / g_ConvBaseSampleRate)));
            g_ConvPos = -(long)g_ConvBaseSampleRate;
            g_ConvClientRate = current_sample_format.sample_rate;
            for (i = 0; i < MAXCHANNELS; i++) {
                m_PrevSamp[i] = 0;
                m_CurrSamp[i] = 0;
            }
            fprintf(
                stderr,
                "spotify alsa_audio_data format changed, input samplerate=%d, input channel=%d \n",
                sample_format->sample_rate, sample_format->channels);
            if (g_PcmCovertionBuffer)
                free(g_PcmCovertionBuffer);
            g_PcmCovertionBuffer = NULL;
            g_PcmCovertionBuffer = (unsigned char *)malloc(32 * 1024);
        }
    }

    if (current_sample_format.channels == 1) {
        g_PcmLastConv =
            PcmRateConvertionM16((unsigned char *)samples, sample_count * 2, g_PcmCovertionBuffer);
        data = g_PcmCovertionBuffer;
        len = g_PcmLastConv - g_PcmCovertionBuffer;
    } else {
        data = (void *)samples;
        len = sample_count * 2;
    }
    if (g_wiimu_shm->asr_pause_request) {
        accepted = 0;
    } else {
        accepted = play(data, len);
#ifdef WMRMPLAY_ENABLE
        spotify_play_flag = 1;
#endif
        if (current_sample_format.channels != 1) {
            accepted *= 2;
        }
    }

end:
#ifdef WMRMPLAY_ENABLE
    buffered = 0;
    if (g_wmrm_sound) {
        static int history_delay = 0;
        int delay = wmrm_sound_get_delay(g_wmrm_sound);
        buffered = delay * g_ConvBaseSampleRate / 1000;
        buffered *= 2;

        static long long last_ticks = 0;
        long long cur_ticks = tickSincePowerOn();
        if (cur_ticks > last_ticks + 2000 && delay < 400) {
            if (spotify_play_flag == 0 && history_delay == delay) {
                buffered = 0;
                alsa_audio_deinit();
            }
            fprintf(stderr, "spotify delay %d, buffered=%d, accepted=%d, history_delay=%d\n", delay, buffered, accepted, history_delay);
            last_ticks = cur_ticks;
        }
        if (spotify_play_flag == 1) {
            spotify_play_flag = 0;
        }
        history_delay = delay;
    }
    *samples_buffered = buffered;
#else
    // Note: this calculation is incorrect if the audio format changed between mono
    // and stereo.  A real integration should track sample count accurately.
    if (alsa_handler && snd_pcm_delay(alsa_handler, &delay) == 0)
        buffered = delay * 2;
    else
        buffered = 0;

    if (alsa_handler) {
        *samples_buffered = buffered + fadeCache_get_data(&g_spotify_fc) / 2;
    }
#endif

    return accepted;
}

void alsa_audio_pause(int pause)
{
    pthread_mutex_lock(&alsa_mutex);
    g_pause = pause;
    if (pause) {
#ifdef WMRMPLAY_ENABLE
        if (g_wmrm_sound)
            wmrm_sound_control(g_wmrm_sound, MRM_SOUND_PLAYBACK_PAUSE);
#else
        if (alsa_handler) {
            fadeCache_pause(&g_spotify_fc);
        }
#endif
    } else {
#ifdef WMRMPLAY_ENABLE
        if (g_wmrm_sound)
            wmrm_sound_control(g_wmrm_sound, MRM_SOUND_PLAYBACK_RESUME);
#else
        if (alsa_handler) {
            fadeCache_resume(&g_spotify_fc);
        }
#endif
    }
    pthread_mutex_unlock(&alsa_mutex);
}

SpError alsa_audio_flush(void)
{
    ALSA_DEBUG("alsa reset ++\n");
    pthread_mutex_lock(&alsa_mutex);

#ifdef WMRMPLAY_ENABLE
    if (g_wmrm_sound)
        wmrm_sound_control(g_wmrm_sound, MRM_SOUND_PLAYBACK_FLUSH);
#else

    if (alsa_handler) {
        fadeCache_alsa_reset(&g_spotify_fc);
    }
#endif

    pthread_mutex_unlock(&alsa_mutex);
    ALSA_DEBUG("alsa reset at %d--\n", (int)master_get_ntp_time());

    return kSpErrorOk;
}
SpError alsa_audio_flush_fade(void)
{
    first_data = 0;

    wm_cal_gpio_toggle();
    alsa_audio_flush();

    return kSpErrorOk;
}

void SpExampleAudioGetCallbacks(struct SpExampleAudioCallbacks *callbacks)
{
    if (!callbacks)
        return;

    callbacks->audio_data = alsa_audio_data;
    callbacks->audio_pause = alsa_audio_pause;
    callbacks->audio_flush = alsa_audio_flush_fade;
    callbacks->audio_deinit = alsa_audio_deinit;
}

static unsigned char *PcmRateConvertionM16(unsigned char *pcmSource, int len,
                                           unsigned char *pcmDest)
{
    long CurrPos = g_ConvPos;
    unsigned int ClientRate = g_ConvClientRate;
    unsigned int BaseRate = g_ConvBaseSampleRate;
    unsigned int BaseRateInv = g_ConvBaseSampleRateInverse;

    int CurrSamp0 = m_CurrSamp[0];
    int PrevSamp0 = m_PrevSamp[0];
    unsigned char *pCurrData = pcmSource;
    unsigned char *pCurrDataEnd = pcmSource + len;

    while (1) {
        while (CurrPos < 0) {
            if (pCurrData >= pCurrDataEnd) {
                goto Exit;
            }

            CurrPos += BaseRate;

            PrevSamp0 = CurrSamp0;

            SAMPLE_16_MONO *pSampleSrc = (SAMPLE_16_MONO *)pCurrData;
            CurrSamp0 = (long)pSampleSrc->sample;
            pCurrData += 2;
        }

        // Calculate ratio between samples as a 17.15 fraction
        // (Only use 15 bits to avoid overflow on next multiply)
        long Ratio;
        Ratio = (CurrPos * BaseRateInv) >> 17;

        CurrPos -= ClientRate;

        long OutSamp0;

        // Calc difference between samples. Note OutSamp0 is a 17-bit signed number now.
        OutSamp0 = PrevSamp0 - CurrSamp0;

        // Now interpolate
        OutSamp0 = (OutSamp0 * Ratio) >> 15;

        // Add to previous number
        OutSamp0 += CurrSamp0;

        long OutSamp1;
        OutSamp1 = OutSamp0;

        ((short *)pcmDest)[0] = (short)OutSamp0;
        ((short *)pcmDest)[1] = (short)OutSamp1;
        pcmDest += 2 * sizeof(short);
    }

Exit:
    g_ConvPos = CurrPos;
    m_PrevSamp[0] = PrevSamp0;
    m_CurrSamp[0] = CurrSamp0;
    return pcmDest;
}
