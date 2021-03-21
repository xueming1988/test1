#ifdef LINKPLAY
/*
** Audio Playing
*/
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include "wm_util.h"
#include "AudioPlay.h"
#include "AudioUtils.h"

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>


//===========================================================================================================================
//  Logging
//===========================================================================================================================

ulog_define(AudioPlay, kLogLevelNotice, kLogFlags_Default, "AudioPlay", NULL);
#define as_dlog(LEVEL, ...) dlogc(&log_category_from_name(AudioPlay), (LEVEL), __VA_ARGS__)
#define as_ulog(LEVEL, ...) ulog(&log_category_from_name(AudioPlay), (LEVEL), __VA_ARGS__)

#ifdef RESAMPLE_BEFORE_ALSA
#include "samplerate.h"

struct rate_src g_resample = {0};
char dst_resampled_buffer[MAX_RESAMPLE_PERIOD_SIZE * 2 * 2];

static int read_samplerate_from_dmixer_conf(void)
{
    int samplerate = 0;
    FILE *fdmix = fopen("/tmp/alsa/dmixer.conf", "rt");
    if (fdmix) {
        char line[256];
        char *p;
        while (NULL != (p = fgets(line, 255, fdmix))) {
            p = strstr(p, "rate");
            if (p) {
                sscanf(p, "rate %d", &samplerate);
            }
        }
        fclose(fdmix);
    }
    return samplerate;
}


int init_resampler_before_alsa(int *p_samplerate)
{
    int samplerate_out = 0;
    int samplerate_in = *p_samplerate;
     
    if (g_resample.state)
        src_delete(g_resample.state);
    
    g_resample.state = 0;

    samplerate_out = read_samplerate_from_dmixer_conf();

    if (samplerate_in != samplerate_out) {
        int err;
        g_resample.channels = NUM_CHANNELS;
        g_resample.converter = SRC_SINC_MEDIUM_QUALITY;
        // SRC_SINC_FASTEST
        // SRC_SINC_BEST_QUALITY
        // SRC_SINC_MEDIUM_QUALITY
        // SRC_ZERO_ORDER_HOLD
        // SRC_LINEAR
        g_resample.state = src_new(g_resample.converter, g_resample.channels, &err);
        g_resample.ratio = (double)samplerate_out / (double)samplerate_in;

        if (g_resample.src_buf)
            free(g_resample.src_buf);
        g_resample.src_buf = malloc(sizeof(float) * g_resample.channels * MAX_RESAMPLE_PERIOD_SIZE);
        if (g_resample.dst_buf)
            free(g_resample.dst_buf);
        g_resample.dst_buf = malloc(sizeof(float) * g_resample.channels * MAX_RESAMPLE_PERIOD_SIZE);
        g_resample.data.data_in = g_resample.src_buf;
        g_resample.data.data_out = g_resample.dst_buf;
        g_resample.data.src_ratio = g_resample.ratio;
        g_resample.data.end_of_input = 0;

        *p_samplerate = samplerate_out;
        
        as_ulog(kLogLevelNotice, "enable resample before alsa (rate %d to %d)\n", samplerate_in, samplerate_out);
    }

    return 0;
}

int get_extra_resample_delay(snd_pcm_sframes_t *InOutAvailable, snd_pcm_sframes_t *InOutDelay)
{
    if (g_resample.state) {
        *InOutAvailable = (int)(*InOutAvailable / g_resample.ratio) - 1;
        *InOutDelay = (int)(*InOutDelay / g_resample.ratio) + 1;
    }

    return 0;
}


int destory_resample(void)
{
    if (g_resample.state)
        src_delete(g_resample.state);

    g_resample.state = 0;

    if (g_resample.src_buf)
        free(g_resample.src_buf);

    if (g_resample.dst_buf)
        free(g_resample.dst_buf);

    g_resample.src_buf = 0;
    g_resample.dst_buf = 0;
    
    return 0;
}

int do_resample_before_alsa(uint8_t **InOutBuffer, long *InOutframes)
{
    if (g_resample.state) {
        g_resample.data.input_frames = *InOutframes;
        g_resample.data.output_frames = MAX_RESAMPLE_PERIOD_SIZE;
        g_resample.data.end_of_input = 0;

        src_short_to_float_array((short *)(*InOutBuffer), g_resample.src_buf, (*InOutframes) * g_resample.channels);
        src_process(g_resample.state, &g_resample.data);
        src_float_to_short_array(g_resample.dst_buf, (short *)dst_resampled_buffer,
                                 g_resample.data.output_frames_gen * g_resample.channels);
        *InOutBuffer = (uint8_t *)dst_resampled_buffer;
        *InOutframes = g_resample.data.output_frames_gen;
    }

    return 0;
}

#endif

snd_pcm_t *alsa_handle = NULL;
pthread_mutex_t airplay_client_mutex = PTHREAD_MUTEX_INITIALIZER;
int airplay_client = 0;
extern WIIMU_CONTEXT* g_wiimu_shm;

static int g_airplay_volume = 0;

extern AirplayPrivControlContext gAirplayPrivCtrlCtx;

static uint8_t SilenceAudioData[MAX_AUDIO_OUTPUT_BUFF_SIZE] = {0};
static int silenceInMs = SILENCE_AUDIO_WRITE_IN_MS;

static int audio_alsa_init(int sampling_rate)
{
    int rc, dir = 0;
    snd_pcm_hw_params_t *alsa_params = NULL;
    snd_pcm_uframes_t threshold_size = 0, buffer_size = 0, period_size = 0;
    snd_pcm_sw_params_t *sw_params = NULL;

#ifdef RESAMPLE_BEFORE_ALSA
    init_resampler_before_alsa(&sampling_rate);
#endif

    rc = snd_pcm_open(&alsa_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0)
    {
        as_ulog(kLogLevelNotice, "unable to open pcm device: %s\n", snd_strerror(rc));
        return rc;
    }

#if !defined(PLATFORM_INGENIC)
    snd_pcm_info_set(alsa_handle, SNDSRC_AIRPLAY);
#endif

    WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 9, SNDSRC_AIRPLAY);
    WMUtil_Snd_Ctrl_SetProcessFadeStep(g_wiimu_shm, 20, SNDSRC_AIRPLAY);
    WMUtil_Snd_Ctrl_SetProcessSelfVol(g_wiimu_shm, 100, SNDSRC_AIRPLAY);
    WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 0, SNDSRC_AIRPLAY, 1);

    snd_pcm_hw_params_alloca(&alsa_params);
    snd_pcm_hw_params_any(alsa_handle, alsa_params);
    snd_pcm_hw_params_set_access(alsa_handle, alsa_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(alsa_handle, alsa_params, SND_PCM_FORMAT_S16);
    snd_pcm_hw_params_set_channels(alsa_handle, alsa_params, NUM_CHANNELS);

    as_ulog(kLogLevelNotice, "set sample rate to %d\n", sampling_rate);
    snd_pcm_hw_params_set_rate_near(alsa_handle, alsa_params, (unsigned int *)&sampling_rate, &dir);
    gAirplayPrivCtrlCtx.SampleRate = sampling_rate;
    
    period_size = CONFIG_AUDIO_PERIOD_SIZE;
    
    rc = snd_pcm_hw_params_set_period_size_near(alsa_handle, alsa_params, &period_size, &dir);
    if (rc < 0)
    {
        as_ulog( kLogLevelNotice, "unable to get period size for playback: %s\n", snd_strerror(rc));
        return rc;
    }

    buffer_size = CONFIG_AUDIO_BUFFER_SIZE;
    snd_pcm_hw_params_set_buffer_size_near(alsa_handle, alsa_params, &buffer_size);

    rc = snd_pcm_hw_params(alsa_handle, alsa_params);
    if (rc < 0)
    {
        as_ulog( kLogLevelNotice, "unable to set hw params: %s\n", snd_strerror(rc));
        return rc;
    }

    rc = snd_pcm_sw_params_malloc(&sw_params);
    if (rc < 0)
    {
        as_ulog( kLogLevelNotice, "unable to alloc sw params: %s\n", snd_strerror(rc));
        return rc;
    }

    snd_pcm_sw_params_current(alsa_handle, sw_params);
    
    threshold_size = CONFIG_AUDIO_THRESHOLD_SIZE;
    rc = snd_pcm_sw_params_set_start_threshold(alsa_handle, sw_params, threshold_size);
    if (rc < 0)
    {
        as_ulog( kLogLevelNotice, "unable to set threshold: %s\n", snd_strerror(rc));
        return rc;
    }
        
    snd_pcm_sw_params(alsa_handle, sw_params);

    if (sw_params != NULL)
    {
        snd_pcm_sw_params_free(sw_params);
        sw_params = NULL;
    }

    snd_pcm_prepare(alsa_handle);
    
    as_ulog( kLogLevelNotice, "alsa audio params init ok.\n");

    return 0;
}

static void audio_alsa_deinit(void)
{
    if (alsa_handle)
    {
        snd_pcm_drain(alsa_handle);
        snd_pcm_close(alsa_handle);
        alsa_handle = NULL;

        WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 100, SNDSRC_AIRPLAY);
        WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 1, SNDSRC_AIRPLAY, 1);

        as_ulog( kLogLevelNotice, "alsa audio params deinit ok.\n");
    }

#ifdef RESAMPLE_BEFORE_ALSA // not define in A31
    destory_resample();
#endif

}


#if 0
static inline int alsa_error_recovery(int err)
{
    int i = 0;
    if (err == -EPIPE)
    {
        /* FIXME: underrun length detection */
        //printf("underrun, restarting...\n");
        /* output buffer underrun */
        err = snd_pcm_prepare(alsa_handle);
        if (err < 0)
        {
            return err;
        }
        usleep(1000);
    }
    else if (err == -ESTRPIPE)
    {
        /* application was suspended, wait until suspend flag clears */
        printf("underrun, snd_pcm_resume\n");
        while ((err = snd_pcm_resume(alsa_handle)) == -EAGAIN)
        {
            usleep(1000);
            if (++i >= 3)
                break;
        }

        if (err < 0)
        {
            /* unable to wake up pcm device, restart it */
            err = snd_pcm_prepare(alsa_handle);
            if (err < 0)
                return err;
        }
        return 0;
    }

    /* error isn't recoverable */
    return err;
}

int audio_play(uint8_t *outbuf, int samples, void* priv_data)
{
    int len = samples;
    uint8_t *ptr = (uint8_t *) outbuf;
    int stop = 0;
    (void)priv_data;

    if (!alsa_handle)
        return -1;

    while (len > 0)
    {
        int err = snd_pcm_writei(alsa_handle, ptr, len);
        if (err < 0)
        {
            //printf("!!%s snd_pcm_writei return %d, try to recover\n", __FUNCTION__, err);
            pthread_mutex_lock(&airplay_client_mutex);
            stop = (alsa_need_to_wait || !alsa_is_ready) ? 1 : 0;
            pthread_mutex_unlock(&airplay_client_mutex);

            if (stop)
            {
                printf("%s alsa_need_to_wait=%d, alsa_is_ready=%d\n",
                       __FUNCTION__,  alsa_need_to_wait, alsa_is_ready);
                return -2;
            }

            if (err == -EAGAIN || err == -EINTR)
                continue;

            err = alsa_error_recovery(err);

            if (err < 0)
            {
                //printf("write error: %s\n", snd_strerror(err));
                return err;
            }
            else
            {
                continue;
            }
        }

        len -= err;
        ptr += (err << 2);
    }
    return 0;
}
#endif

static int create_alsa_pipe(void)
{
    int err = 0;

    err = audio_alsa_init(DEFAULT_SPEED);

    if (err < 0)
    {
        as_ulog( kLogLevelNotice, "audio_init error: %s %s, %d\n", snd_strerror(err), __FUNCTION__, __LINE__);
        return err;
    }

    return 1;
}


int audio_client_init(void)
{
    pthread_mutex_lock(&airplay_client_mutex);

    if (airplay_client == 0)
    {
        create_alsa_pipe();
    }
    else
    {
        as_ulog( kLogLevelNotice, "audio stream already created !\n");
    }

    airplay_client += 1;/* inc client num */

    pthread_mutex_unlock(&airplay_client_mutex);

    as_ulog( kLogLevelNotice, "audio client initiated. \n");

    return 0;
}


int audio_client_deinit(void)
{
    pthread_mutex_lock(&airplay_client_mutex);

    as_ulog( kLogLevelNotice, "#### airplay_client=[%d].\n", airplay_client);

    if (airplay_client > 0)
    {
        airplay_client -= 1;/* dec client num */
    }

    if (airplay_client <= 0)
    {
        /*
        * PLATFORM_TO_DO:
        *
        *   - Notify the audio thread to stop rendering.
        *   - Wait for the thread to finish.
        *      + If killing the thread, ensure join is done here.
        *
        *   - Shutdown/reset platform's audio infrastructure
        *   - cleanup buffers etc used for this stream.
        *
        */
        audio_alsa_deinit();
        as_ulog( kLogLevelNotice, "airplay_client is none.\n");
    }
    else
    {
        as_ulog( kLogLevelNotice, "another client came in, deinit current and start new one.\n");

        audio_alsa_deinit();
        
        create_alsa_pipe();
    }

    pthread_mutex_unlock(&airplay_client_mutex);

    return 0;
}


int audio_set_volume(double inVol)
{
    double f = inVol;
    int hwVol;
    char sendBuf[48] = {0};

    hwVol = (f < -30.0) ? 0 : (((f + 30.0) * 100.0 / 30.0) + 0.5);

    if (hwVol > 100)
    {
        hwVol = 100;
    }

    if (g_airplay_volume != hwVol) 
    {
        as_ulog( kLogLevelNotice, "Airplay2 set volume to [%f] db, [%d] percent.\n", f, hwVol);
        g_airplay_volume = hwVol;

        memset((void *)sendBuf, 0, sizeof(sendBuf));
        snprintf(sendBuf, sizeof(sendBuf) - 1, "setPlayerCmd:airplay_vol:%d", hwVol);
        SocketClientReadWriteMsg("/tmp/Requesta01controller", sendBuf, strlen(sendBuf), NULL, NULL, 0);
    }
    
    return 0;
}

int audio_client_get_alsa_handler(snd_pcm_t **pOutHandler)
{
    if (!alsa_handle)
        audio_client_init();
    
    *pOutHandler = alsa_handle;

    return 0;
}


static short lcg_rand(void)
{
    static unsigned long lcg_prev = 12345;
    lcg_prev = lcg_prev * 69069 + 3;
    return lcg_prev & 0xffff;
}

static inline short dithered_vol(short sample)
{
    static short rand_a, rand_b;
    long out;

    out = (long)sample * gAirplayPrivCtrlCtx.RequiredFadeVolume;
    
    if (gAirplayPrivCtrlCtx.RequiredFadeVolume < 0x10000) {
        rand_b = rand_a;
        rand_a = lcg_rand();
        out += rand_a;
        out -= rand_b;
    }

    return out >> 16;
}

#define kFixed1Pt0 0x00010000 // 1.0 in 16.16 fixed format
#define kFixed0Pt5 0x00008000 // 0.5 in 16.16 fixed format...used for rounding.
static void _audio_client_adjust_volume(int16_t* dst, size_t size, int32_t scale)
{
    int16_t* lim = dst + (size / 2);

    if (scale == 0) {
        for (; dst < lim; ++dst)
            *dst = 0;
    } else if (scale != kFixed1Pt0) {
        for (; dst < lim; ++dst)
            *dst = (int16_t)(((*dst * scale) + kFixed0Pt5) >> 16);
    }
}

void audio_client_process_fade_and_silence(uint8_t **InAudioBuffer, long inFrames, size_t bytesPerUnit, int32_t *fixedVolume)
{
    if (gAirplayPrivCtrlCtx.RequestSilence)
    {
        if(gAirplayPrivCtrlCtx.SampleRate > 0)
            silenceInMs -= (inFrames * 1000)/ gAirplayPrivCtrlCtx.SampleRate;
        else
            silenceInMs = 0;
        if (silenceInMs <= 0)
        {
            //reset 
            gAirplayPrivCtrlCtx.RequestSilence = 0;
            silenceInMs = SILENCE_AUDIO_WRITE_IN_MS;
        }

        if (InAudioBuffer)
            *InAudioBuffer = SilenceAudioData;
    }

    if (*InAudioBuffer != SilenceAudioData && gAirplayPrivCtrlCtx.RequestFadeIn && *fixedVolume < 0x00010000){
    
        *fixedVolume += 0x50;
    
        if(*fixedVolume > 0x00010000){
            *fixedVolume = 0x00010000;
            gAirplayPrivCtrlCtx.RequestFadeIn = 0;
        }

        _audio_client_adjust_volume((int16_t*)(*InAudioBuffer), (size_t)(inFrames * bytesPerUnit), *fixedVolume);
    }
}


#endif /* LINKPLAY */

