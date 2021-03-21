/*****************************************************************************
 **
**  Name:           app_avk.c
**
**  Description:    Bluetooth Manager application
**
**  Copyright (c) 2009-2015, Broadcom Corp., All Rights Reserved.
**  Broadcom Bluetooth Core. Proprietary and confidential.
**
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "pthread.h"

#include "buildcfg.h"

#include "bsa_api.h"
#include "bsa_avk_api.h"

#include "gki.h"
#include "uipc.h"

#include "app_xml_utils.h"
#include "app_disc.h"
#include "app_utils.h"
#include "app_wav.h"

#include "app_manager.h"
#include "a2d_m24.h"
#include "app_avk.h"
#include "wm_util.h"

#ifdef WMRMPLAY_ENABLE
#include "wmrm_api.h"
static wmrm_snd_t g_wmrm_sound = 0;
wmrm_sound_param g_wmrm_param = {0};
static int g_wmrm_playing = 0;
static long long g_wmrm_start_tick = 0;
static long long g_wmrm_render_sample = 0;
static long long g_wmrm_last_tick = 0;
static int g_skew_offset = 0;
static int g_wmrm_used = 0;
#endif
#include "alsa/asoundlib.h"
#define APP_AVK_ASLA_DEV "default"
#include "wm_fadecache.h"
static FadeCache g_BT_fadeCache;
int g_BT_fc_enable = 1;

#include "linkplay_resample_interface.h"
af_instance_t *g_resample_instance = NULL;
af_data_t g_af_data;
#ifdef SAMPLING_RATE_48K
const int g_default_out_sample_rate = 48000; // the alsa default sample rate
#else
const int g_default_out_sample_rate = 44100; // the alsa default sample rate
#endif

extern WIIMU_CONTEXT *g_wiimu_shm;
extern int g_sec_sp_cfm_req;
/*
* Defines
*/
pthread_mutex_t alsa_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t aac_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t volume_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t operation_mutex = PTHREAD_MUTEX_INITIALIZER;

//static int bt_music_ready_count = 0;
static long long bt_music_pcm_ts = -1;

#define TONE_DETECTION_COUNT (44100 * 3)

#define APP_AVK_SOUND_FILE "test_avk"

#ifndef BSA_AVK_SECURITY
#define BSA_AVK_SECURITY BSA_SEC_AUTHORIZATION
#endif
#ifndef BSA_AVK_FEATURES
#define BSA_AVK_FEATURES                                                                           \
    (BSA_AVK_FEAT_RCCT | BSA_AVK_FEAT_RCTG | BSA_AVK_FEAT_VENDOR | BSA_AVK_FEAT_METADATA |         \
     BSA_AVK_FEAT_BROWSE | (1 << BSA_AVK_FEAT_DELAY_RPT))
#endif

#ifndef BT_AVK_DELAY_MS    /* not define in proj.inc */
#define BT_AVK_DELAY_MS 30 /* ms */
#endif

#ifndef BSA_AVK_DUMP_RX_DATA
#define BSA_AVK_DUMP_RX_DATA FALSE
#endif

#ifndef BSA_AVK_AAC_SUPPORTED
#define BSA_AVK_AAC_SUPPORTED TRUE
#endif
#include "aacdecoder_lib.h"
#define DECODE_BUF_LEN (128 * 1024)

typedef struct {
    HANDLE_AACDECODER aac_handle;
    BOOLEAN has_aac_handle; // True if aac_handle is valid
    INT_PCM *decode_buf;
    //   decoded_data_callback_t decode_callback;
} tA2DP_AAC_DECODER_CB;

static tA2DP_AAC_DECODER_CB a2dp_aac_decoder_cb;

/* bitmask of events that BSA client is interested in registering for notifications */
tBSA_AVK_REG_NOTIFICATIONS reg_notifications =
    (1 << (BSA_AVK_RC_EVT_PLAY_STATUS_CHANGE - 1)) | (1 << (BSA_AVK_RC_EVT_TRACK_CHANGE - 1)) |
    (1 << (BSA_AVK_RC_EVT_TRACK_REACHED_END - 1)) |
    (1 << (BSA_AVK_RC_EVT_TRACK_REACHED_START - 1)) |
    //  (1 << (BSA_AVK_RC_EVT_PLAY_POS_CHANGED - 1)) |
    (1 << (BSA_AVK_RC_EVT_BATTERY_STATUS_CHANGE - 1)) |
    (1 << (BSA_AVK_RC_EVT_SYSTEM_STATUS_CHANGE - 1)) |
    (1 << (BSA_AVK_RC_EVT_APP_SETTING_CHANGE - 1)) |
    (1 << (BSA_AVK_RC_EVT_NOW_PLAYING_CHANGE - 1)) |
    (1 << (BSA_AVK_RC_EVT_AVAL_PLAYERS_CHANGE - 1)) |
    (1 << (BSA_AVK_RC_EVT_ADDR_PLAYER_CHANGE - 1)) |
    (1 << (BSA_AVK_RC_EVT_UIDS_CHANGE - 1)) |
    (1 << (BSA_AVK_RC_EVT_VOLUME_CHANGE - 1));

/*
* global variable
*/

tAPP_AVK_CB app_avk_cb;

#ifdef PCM_ALSA
static char *alsa_device = "default"; /* ALSA playback device */
#endif

/*
* Local functions
*/
static void app_avk_close_wave_file(tAPP_AVK_CONNECTION *connection);
static void app_avk_create_wave_file(void);
static void app_avk_uipc_cback(BT_HDR *p_msg);

static tAvkCallback *s_pCallback = NULL;
static tAPP_AVK_CONNECTION *pStreamingConn = NULL;
static long long g_pcm_sample_total_count = 0;
static int g_start_pcm_threshold = 0;
static int g_pcm_max_buffer_size = 0;

#ifndef A2DP_PCM_SKEW_DRS_DISABLE
#define PCM_SKEW_DRS
#endif
#define MaxSkewAdjustRate                                                                          \
    441 // Max number of samples to insert/remove per second for skew compensation,
//static unsigned char g_skewAdjustBuffer[DECODE_BUF_LEN];
//static unsigned char g_pcmTempBuffer[DECODE_BUF_LEN];
static tBSA_SEC_AUTH g_avk_sec_mask = BSA_SEC_AUTHENTICATION; // need passkey for pairing

#ifdef PCM_SKEW_DRS
#include "lputil_skew.h"
void *g_pSkewIns = NULL;
#endif

volatile int g_cacheTimeAverage = 0;

static short lcg_rand(void)
{
    static unsigned long lcg_prev = 12345;
    lcg_prev = lcg_prev * 69069 + 3;
    return lcg_prev & 0xffff;
}

static inline short dithered_vol(short sample, unsigned int fix_volume)
{
    static short rand_a, rand_b;
    long out;
	
	if(fix_volume == 0x10000)
	{
		return sample;
	}
	
    out = (long)sample * fix_volume;
    if(fix_volume < 0x10000) {
        rand_b = rand_a;
        rand_a = lcg_rand();
        out += rand_a;
        out -= rand_b;
    }

    return out >> 16;
}
#define DITHER_MAX_VALUE 0x10000
#define DITHER_MIN_VALUE 0x0
struct param_range {
        long long min;
        long long max;
        long long step;
};
enum {
    DIRECTION_UNKNOWN = -1,
    DIRECTION_FADE_IN,
    DIRECTION_FADE_OUT,
};

static const float vol_min_db = -50.0;
static const float vol_mid_db = -20.0;
static const float vol_max_db = 0.0;
static const int vol_mid_point = 50;  // volume_range.max / 2

static struct param_range volume_range = { 0, 100, 1 };
static struct param_range volume_db_range = { -60 * 256, 0, 0 };  // volume_min_db
static float volume_level_to_decibel(int volume) {
	if (volume < volume_range.min) volume = volume_range.min;
	if (volume > volume_range.max) volume = volume_range.max;
	if (volume < volume_range.max / 2) {
		return vol_min_db
			+ (vol_mid_db - vol_min_db) / vol_mid_point * volume;
	}
	else {
		const int range = volume_range.max - vol_mid_point;
		return vol_mid_db
			+ ((vol_max_db - vol_mid_db) / range
			   * (volume - vol_mid_point));
	}
}

static  int g_snd_vol_cur = 0;
static  int g_snd_vol_cur_save = 0;
static  const int g_mix_vol_max = 100;
static  const int g_mix_vol_min = 0;
static  int g_snd_vol_gain = 0;
static  unsigned int g_fadeframe_counts = 0;
static  int g_fadeint_eachframe = 0;
static  int g_fadeout_eachframe = 10;
static  int g_direction = 0;
static int g_mute_flag = 0;
FILE *fdump = NULL;

static void CalculateFixedVolume( int vol_dec)
{

    int fade_each_frame = g_fadeint_eachframe;
    if(vol_dec)
    {
	 fade_each_frame = g_fadeout_eachframe;
    }

     if(g_fadeframe_counts++%fade_each_frame == 0)
     {
       if(vol_dec)
       {
	     g_snd_vol_cur--;
       }else
       {
	    g_snd_vol_cur++;
       }
	   
	if (g_snd_vol_cur <=  g_mix_vol_min)
	{
		g_snd_vol_cur  = g_mix_vol_min;
		if(g_snd_vol_cur == 0)g_snd_vol_gain = 0;
	}
	else if(g_snd_vol_cur >=  g_mix_vol_max)
		 g_snd_vol_cur = g_mix_vol_max;
	
	if(g_snd_vol_cur_save != g_snd_vol_cur)
	{
	 const float decibel = volume_level_to_decibel(g_snd_vol_cur);
	 const double fraction = exp(decibel / 20 * log(10));
	 g_snd_vol_gain =  DITHER_MAX_VALUE * fraction;
	 g_snd_vol_cur_save= g_snd_vol_cur;
	}
     }

}
static void pcm_fade_by_direct(signed short *dst, unsigned int framesize)
{

    int vol_dec = 0;
    int mute_flag = g_mute_flag;
	signed short *dstsave = dst;
	unsigned int framesizesave = framesize;


    if (mute_flag ||g_direction == DIRECTION_FADE_OUT) {
        vol_dec = 1;

    } else if (g_direction == DIRECTION_FADE_IN) {
        vol_dec = 0;
    }

        for (;;) {
            CalculateFixedVolume(vol_dec);
            if (!*dst) {
                ;
            } else {
                *dst = dithered_vol(*dst, g_snd_vol_gain);
            }
            if (!--framesize)
                break;
            dst++;
        }
//	if(g_snd_vol_gain  != DITHER_MAX_VALUE && g_snd_vol_gain != DITHER_MIN_VALUE)
//			  wiimu_log(1, 0, 0, 0, "vol %d  \r\n",g_snd_vol_gain);
	//	{
	
	//	if(fdump)
	//	{
	//		fwrite(dstsave,1,framesizesave*2,fdump);
		//	wiimu_log(1, 0, 0, 0, "vol %d  \r\n",g_snd_vol_gain);
	//	}
	//}
	//	if(g_snd_vol_gain == 0 && fdump)
	//	{
	//		fclose(fdump);
	//		fdump = NULL;
	//	}
	
}
/*******************************************************************************
 **
** Function         app_avk_get_label
**
** Description      get transaction label (used to distinguish transactions)
**
** Returns          UINT8
**
*******************************************************************************/
UINT8 app_avk_get_label()
{
    if (app_avk_cb.label >= 15)
        app_avk_cb.label = 0;
    return app_avk_cb.label++;
}

void app_avk_close_alsa(void)
{
    pthread_mutex_lock(&alsa_mutex);
#ifdef WMRMPLAY_ENABLE
    if(WMUtil_check_wmrm_used(&g_wmrm_used))
    {
    if (g_wmrm_sound)
        wmrm_sound_deinit(g_wmrm_sound);
    g_wmrm_sound = 0;
    g_wmrm_playing = 0;
    }
    else
#endif
    {
    if (app_avk_cb.alsa_handle) {
        APP_DEBUG0("app_avk_close_alsa ++++");
        if (g_BT_fc_enable && g_BT_fadeCache.cache_run) {
            fadeCache_uninit(&g_BT_fadeCache, 1);
        }
        /* If ALSA PCM driver was already open => close it */
        snd_pcm_close(app_avk_cb.alsa_handle);
        app_avk_cb.alsa_handle = NULL;
        APP_DEBUG0("app_avk_close_alsa  ----");
    }
    }
    pthread_mutex_unlock(&alsa_mutex);
}

static void app_avk_create_wave_file(void)
{
    int file_index = 0;
    int fd;
    char filename[200];

#ifdef PCM_ALSA
    return;
#endif

    /* If a wav file is currently open => close it */
    if (app_avk_cb.fd != -1) {
        tAPP_AVK_CONNECTION dummy;
        memset(&dummy, 0, sizeof(tAPP_AVK_CONNECTION));
        app_avk_close_wave_file(&dummy);
    }

    do {
        snprintf(filename, sizeof(filename), "%s-%d.wav", APP_AVK_SOUND_FILE, file_index);
        filename[sizeof(filename) - 1] = '\0';
        fd = app_wav_create_file(filename, O_EXCL);
        file_index++;
    } while (fd < 0);

    app_avk_cb.fd = fd;
}

UINT16  app_avk_aac_get_sample_rate(UINT16 index)
{
    UINT32 ret = 0;
    index = index & A2D_M24_IE_SAMP_FREQ_MSK ;

	switch(index)
	{
        case A2D_M24_IE_SAMP_FREQ_8:
            ret = 8000;
            break;
        
        case A2D_M24_IE_SAMP_FREQ_11:
            ret = 11000;
            break;
        
        case A2D_M24_IE_SAMP_FREQ_12:
            ret = 12000;
            break;
        
        case A2D_M24_IE_SAMP_FREQ_16:
            ret = 16000;
            break;
        
        case A2D_M24_IE_SAMP_FREQ_22:
            ret = 22050;
            break;
        
        case A2D_M24_IE_SAMP_FREQ_24:
            ret = 24000;
            break;
        
        case A2D_M24_IE_SAMP_FREQ_32:
            ret = 32000;
            break;
        
        case A2D_M24_IE_SAMP_FREQ_44:
            ret = 44100;
            break;
        case A2D_M24_IE_SAMP_FREQ_48:
            ret = 48000;
            break;
        case A2D_M24_IE_SAMP_FREQ_64:
            ret = 64000;
            break;
        case A2D_M24_IE_SAMP_FREQ_88:
            ret = 88000;
            break;
        case A2D_M24_IE_SAMP_FREQ_96:
            ret = 96000;
            break;
        default: 
            printf("%s: can't find the sample rate, use default 44.1khz\n", __func__ );
            ret = 44100;
	}
    return ret;
}


UINT8  app_avk_aac_get_channel_num(UINT8 index)
{
    UINT32 ret = 0;
    index = index & A2D_M24_IE_CHNL_MSK ;
    
    switch(index)
    {
        case A2D_M24_IE_CHNL_1:
            ret = 1;
            break;
        case A2D_M24_IE_CHNL_2:
            ret = 2;
            break;
        default: 
            APP_ERROR1("%s: can't find the channel num, use default 2\n", __func__ );		
            ret = 2;
    }
    return ret;
}

static void app_avk_aac_create_decoder(tBSA_AVK_MSG *p_data, tAPP_AVK_CONNECTION *connection)
{
	if(!a2dp_aac_decoder_cb.has_aac_handle)
	{
	  a2dp_aac_decoder_cb.aac_handle = aacDecoder_Open(TT_MP4_LATM_MCP1, 1 /* nrOfLayers */);
	  a2dp_aac_decoder_cb.has_aac_handle = TRUE;
	  a2dp_aac_decoder_cb.decode_buf = malloc(sizeof(a2dp_aac_decoder_cb.decode_buf[0]) * DECODE_BUF_LEN);
	}
	connection->sample_rate = app_avk_aac_get_sample_rate(p_data->start.media_receiving.cfg.aac.samp_freq);
	connection->num_channel = app_avk_aac_get_channel_num(p_data->start.media_receiving.cfg.aac.chnl);  
	connection->bit_per_sample = 16;
	printf("Sampling rate:%d, number of channel:%d Sampling rate:%d, number of channel:%d  \n",
		   connection->sample_rate, connection->num_channel,
		   p_data->start.media_receiving.cfg.aac.samp_freq,
		   p_data->start.media_receiving.cfg.aac.chnl);
}

static void app_avk_aac_destroy_decoder()
{
    if (a2dp_aac_decoder_cb.has_aac_handle) {
        aacDecoder_Close(a2dp_aac_decoder_cb.aac_handle);
        free(a2dp_aac_decoder_cb.decode_buf);
    }
    memset(&a2dp_aac_decoder_cb, 0, sizeof(a2dp_aac_decoder_cb));
}

static int app_avk_aac_decode_pcm(char *p_in_buffer, int in_len, char **pp_out_buffer, int *out_len)
{

    UINT bufferSize = in_len;
    UINT bytesValid = in_len;
    UINT total_len = 0;
	
    if (!a2dp_aac_decoder_cb.has_aac_handle) {
        APP_ERROR1("%s: a2dp_aac_decoder_cb.has_aac_handle is NULL\n", __func__);
        return FALSE;
    }
	
    while (bytesValid > 0) {
        AAC_DECODER_ERROR err =
            aacDecoder_Fill(a2dp_aac_decoder_cb.aac_handle, &p_in_buffer, &bufferSize, &bytesValid);
        if (err != AAC_DEC_OK) {
            return FALSE;
        }

        while (1) {
            err = aacDecoder_DecodeFrame(a2dp_aac_decoder_cb.aac_handle,
                                        (char *)a2dp_aac_decoder_cb.decode_buf + total_len,
                                         DECODE_BUF_LEN - total_len, 0 /* flags */);
            if (err == AAC_DEC_NOT_ENOUGH_BITS) {
                break;
            }
            if (err != AAC_DEC_OK) {
                APP_ERROR1("%s: aacDecoder_Fill failed: %x bytesValid %d ", __func__, err,
                           bytesValid);
                break;
            }

            CStreamInfo *info = aacDecoder_GetStreamInfo(a2dp_aac_decoder_cb.aac_handle);
            if (!info || info->sampleRate <= 0) {
                APP_ERROR1("%s: Invalid stream info", __func__);
                break;
            }

            size_t frame_len =
                info->frameSize * info->numChannels * sizeof(a2dp_aac_decoder_cb.decode_buf[0]);
            total_len += frame_len;
	    
        }
    }
    *pp_out_buffer = a2dp_aac_decoder_cb.decode_buf;
    *out_len = total_len;
	
    return TRUE;
}
/*******************************************************************************
 **
** Function         app_avk_create_aac_file
**
** Description     create a new wave file
**
** Returns          void
**
*******************************************************************************/
static void app_avk_create_aac_file(void)
{
    int file_index = 0;
    int fd;
    char filename[200];

    /* If a wav file is currently open => close it */
    if (app_avk_cb.fd != -1) {
        //        close(app_avk_cb.fd);
        //        app_avk_cb.fd = -1;
        return;
    }

    do {
        int flags = O_EXCL | O_RDWR | O_CREAT | O_TRUNC;

        snprintf(filename, sizeof(filename), "%s-%d.aac", APP_AVK_SOUND_FILE, file_index);
        filename[sizeof(filename) - 1] = '\0';

        /* Create file in read/write mode, reset the length field */
        fd = open(filename, flags, 0666);
        if (fd < 0) {
            APP_ERROR1("open(%s) failed: %d", filename, errno);
        }

        file_index++;
    } while ((fd < 0) && (file_index < 100));

    app_avk_cb.fd_codec_type = BSA_AVK_CODEC_M24;
    app_avk_cb.fd = fd;
}

/*******************************************************************************
 **
** Function         app_avk_close_wave_file
**
** Description     proper header and close the wave file
**
** Returns          void
**
*******************************************************************************/
static void app_avk_close_wave_file(tAPP_AVK_CONNECTION *connection)
{
    tAPP_WAV_FILE_FORMAT format;

#ifdef PCM_ALSA
    return;
#endif

    if (app_avk_cb.fd == -1) {
        printf("app_avk_close_wave_file: no file to close\n");
        return;
    }

    format.bits_per_sample = connection->bit_per_sample;
    format.nb_channels = connection->num_channel;
    format.sample_rate = connection->sample_rate;

    app_wav_close_file(app_avk_cb.fd, &format);

    app_avk_cb.fd = -1;
}

/*******************************************************************************
**
** Function         app_avk_end
**
** Description      This function is used to close AV
**
** Returns          void
**
*******************************************************************************/
void app_avk_end(void)
{
    tBSA_AVK_DISABLE disable_param;
    tBSA_AVK_DEREGISTER deregister_param;

    /* deregister avk */
    BSA_AvkDeregisterInit(&deregister_param);
    BSA_AvkDeregister(&deregister_param);

    /* disable avk */
    BSA_AvkDisableInit(&disable_param);
    BSA_AvkDisable(&disable_param);
}

/*******************************************************************************
**
** Function         app_avk_handle_start
**
** Description      This function is the AVK start handler function
**
** Returns          void
**
*******************************************************************************/
static int g_last_format = 0;
static int g_last_channel = 0;
static int g_last_sample_rate = 0;
static int g_last_bitspersample = 0;
static int g_last_periodsize = 0;

int avk_snd_pcm_set_params(snd_pcm_t *pcm, snd_pcm_format_t format, snd_pcm_access_t access,
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
    buffer_size = period_size * 8;
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
//} else {
#if 0
/* standard configuration buffer_time -> periods */
err = snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
if(err < 0) {
    SNDERR("Unable to get buffer size for %s: %s", s, snd_strerror(err));
    return err;
}
err = snd_pcm_hw_params_get_buffer_time(params, &latency, NULL);
if(err < 0) {
    SNDERR("Unable to get buffer time (latency) for %s: %s", s, snd_strerror(err));
    return err;
}
/* set the period time */
period_time = latency / 4;
err = snd_pcm_hw_params_set_period_time_near(pcm, params, &period_time, NULL);
if(err < 0) {
    SNDERR("Unable to set period time %i for %s: %s", period_time, s, snd_strerror(err));
    return err;
}
err = snd_pcm_hw_params_get_period_size(params, &period_size, NULL);
if(err < 0) {
    SNDERR("Unable to get period size for %s: %s", s, snd_strerror(err));
    return err;
}
}

#endif

    /* write the parameters to device */
    printf("buffer_size %d \r\n", buffer_size);
    printf("period_size %d \r\n", period_size);
    printf("latency %d \r\n", latency);
    g_pcm_max_buffer_size = buffer_size;
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
    err = snd_pcm_sw_params_set_start_threshold(pcm, swparams, 4 * period_size);
    if (err < 0) {
        SNDERR("Unable to set start threshold mode for %s: %s", s, snd_strerror(err));
        return err;
    }
    g_start_pcm_threshold = 4 * period_size;
    printf("g_start_pcm_threshold %d \r\n", g_start_pcm_threshold);
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

static void reset_alsa_device(void)
{
    pthread_mutex_lock(&alsa_mutex);

    if (app_avk_cb.alsa_handle) {
        snd_pcm_close(app_avk_cb.alsa_handle);
        app_avk_cb.alsa_handle = NULL;
    }

    APP_DEBUG0("ALSA driver open ++++");
    int status = snd_pcm_open(&(app_avk_cb.alsa_handle), alsa_device, SND_PCM_STREAM_PLAYBACK,
                              SND_PCM_NONBLOCK);
    if (status < 0) {
        APP_ERROR1("snd_pcm_open failed: %s", snd_strerror(status));
    } else {
        /* Configure ALSA driver with PCM parameters */
        APP_DEBUG0("ALSA driver set params ++++");

        snd_pcm_info_set(app_avk_cb.alsa_handle, SNDSRC_BLUETOOTH);
        status = avk_snd_pcm_set_params(app_avk_cb.alsa_handle, g_last_format,
                                        SND_PCM_ACCESS_RW_INTERLEAVED, g_last_channel,
                                        g_default_out_sample_rate, 1, 50000); /* 0.05sec */
        if (status < 0) {
            APP_ERROR1("snd_pcm_set_params failed: %s", snd_strerror(status));
        }
        g_BT_fadeCache.alsa_handler = app_avk_cb.alsa_handle;
    }
    pthread_mutex_unlock(&alsa_mutex);
}

static void app_avk_handle_start(tBSA_AVK_MSG *p_data, tAPP_AVK_CONNECTION *connection)
{
    char *avk_format_display[12] = {"SBC", "M12", "M24", "??", "ATRAC", "PCM", "APT-X", "SEC"};
#ifdef PCM_ALSA
    int status;
    snd_pcm_format_t format;
#endif

    //   if(p_data->start.media_receiving.format < (sizeof(avk_format_display) /
    //   sizeof(avk_format_display[0]))) {
    //       APP_INFO1("AVK start: format %s",
    //       avk_format_display[p_data->start.media_receiving.format]);
    //   } else {
    //       APP_INFO1("AVK start: format code (%u) unknown", p_data->start.media_receiving.format);
    //   }

    connection->format = p_data->start.media_receiving.format;
	g_cacheTimeAverage = 0;

    if (connection->format == BSA_AVK_CODEC_PCM) {
        //    app_avk_create_wave_file();/* create and open a wave file */
        connection->sample_rate = p_data->start.media_receiving.cfg.pcm.sampling_freq;
        connection->num_channel = p_data->start.media_receiving.cfg.pcm.num_channel;
        connection->bit_per_sample = p_data->start.media_receiving.cfg.pcm.bit_per_sample;
        wiimu_log(1, 0, 0, 0, "BLUETOOTH Start for BSA_AVK_CODEC_PCM");
    } else if (connection->format == BSA_AVK_CODEC_M24) {
    	pthread_mutex_lock(&aac_mutex);
        app_avk_aac_create_decoder(p_data, connection);
	pthread_mutex_unlock(&aac_mutex);
        wiimu_log(1, 0, 0, 0, "BLUETOOTH Start for BSA_AVK_CODEC_M24");
    }
    if (connection->bit_per_sample == 8)
        format = SND_PCM_FORMAT_U8;
    else
        format = SND_PCM_FORMAT_S16_LE;
//   printf("Sampling rate:%d, number of channel:%d bit per sample:%d\n",
//          p_data->start.media_receiving.cfg.pcm.sampling_freq,
//          p_data->start.media_receiving.cfg.pcm.num_channel,
//          p_data->start.media_receiving.cfg.pcm.bit_per_sample);

#ifdef WMRMPLAY_ENABLE
    WMUtil_decide_wmrm_use(&g_wmrm_used, g_wiimu_shm, connection->sample_rate);
    if(WMUtil_check_wmrm_used(&g_wmrm_used))
    {
    if (format != SND_PCM_FORMAT_S16_LE) {
        g_wmrm_param.format = 0;
        wiimu_log(1, 0, 0, 0, "BLUETOOTH Start error: we only support SND_PCM_FORMAT_S16_LE");
        return;
    }
    if (g_wmrm_param.sample_rate == connection->sample_rate &&
        g_wmrm_param.sample_channel == connection->num_channel) {
        printf("BSA_AVK_CODEC_PCM the same param, do nothing\n");
        g_pcm_sample_total_count = 0;
        // g_wiimu_shm->bt_a2dp_playing_count = 0;
        return;
    }
    g_wmrm_param.format = format;
    g_wmrm_param.sample_rate = connection->sample_rate;
    g_wmrm_param.sample_channel = connection->num_channel;
    app_avk_close_alsa();

    // g_wiimu_shm->bt_a2dp_playing_count = 0;
    wiimu_log(1, 0, 0, 0, "BLUETOOTH Start for WMRM ready");
    }
    else
#endif
    {
    pthread_mutex_lock(&alsa_mutex);
    if (g_BT_fc_enable && app_avk_cb.alsa_handle) {
        if (!g_BT_fadeCache.cache_run) {
            fadeCache_init(&g_BT_fadeCache, app_avk_cb.alsa_handle, SNDSRC_BLUETOOTH, g_wiimu_shm,
                           g_last_sample_rate, g_last_format,
                           g_last_periodsize * g_last_channel * g_last_bitspersample / 8,
                           g_last_channel * g_last_bitspersample / 8);
            g_BT_fadeCache.reset_alsa_device = reset_alsa_device;
	    g_BT_fadeCache.pcm_fade_hook = pcm_fade_by_direct;
        }
    }

    if (g_last_format == format && g_last_sample_rate == connection->sample_rate &&
        g_last_channel == connection->num_channel &&
        g_last_bitspersample == connection->bit_per_sample && app_avk_cb.alsa_handle) {
        printf("BSA_AVK_CODEC_PCM the same param, do nothing\n");
        g_pcm_sample_total_count = 0;
#ifdef PCM_SKEW_DRS
	 if(g_pSkewIns)lp_skew_PIDReset(g_pSkewIns);
#endif
        pthread_mutex_unlock(&alsa_mutex);
        return;
    }
    if (g_BT_fc_enable && g_BT_fadeCache.cache_run) {
        fadeCache_uninit(&g_BT_fadeCache, 1);
    }
    // usleep(50000);
    /* If ALSA PCM driver was already open => close it */
    if (app_avk_cb.alsa_handle != NULL) {
        APP_DEBUG0("ALSA driver close ++++");
        snd_pcm_close(app_avk_cb.alsa_handle);
        app_avk_cb.alsa_handle = NULL;
        APP_DEBUG0("ALSA driver close ----");
    }

    if (g_resample_instance)
        linkplay_resample_uninit(g_resample_instance);
    g_resample_instance = NULL;
#ifdef PCM_SKEW_DRS
   if(g_pSkewIns)
   	lp_skew_deinit(g_pSkewIns);
   g_pSkewIns = NULL;
#endif

    /* Open ALSA driver */
    APP_DEBUG0("ALSA driver open ++++");
    status = snd_pcm_open(&(app_avk_cb.alsa_handle), alsa_device, SND_PCM_STREAM_PLAYBACK,
                          SND_PCM_NONBLOCK);
    if (status < 0) {
        APP_ERROR1("snd_pcm_open failed: %s", snd_strerror(status));
    } else {
        /* Configure ALSA driver with PCM parameters */
        APP_DEBUG0("ALSA driver set params ++++");

        snd_pcm_info_set(app_avk_cb.alsa_handle, SNDSRC_BLUETOOTH);
        // WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 0, SNDSRC_BLUETOOTH);
        if (g_default_out_sample_rate != connection->sample_rate) {
            char resample_params[64];
            snprintf(resample_params, sizeof(resample_params), "%d:16:0:5:0.0",
                     g_default_out_sample_rate);
            printf("linkplay_resample_instance_create %s \r\n", resample_params);
            g_resample_instance = linkplay_resample_instance_create(resample_params);
            if (g_resample_instance) {
                g_af_data.rate = connection->sample_rate;
                g_af_data.nch = 2;
                g_af_data.bps = 2;
                linkplay_set_resample_format(&g_af_data, g_resample_instance);
            }
        }
  
#ifdef PCM_SKEW_DRS
	printf("app avk DRS skew init %d \r\n", connection->sample_rate);
	g_pSkewIns = lp_skew_init(2, 4, connection->sample_rate);
#endif
        status = avk_snd_pcm_set_params(app_avk_cb.alsa_handle, format,
                                        SND_PCM_ACCESS_RW_INTERLEAVED, connection->num_channel,
                                        g_default_out_sample_rate, 1, 50000); /* 0.05sec */
        if (status < 0) {
            APP_ERROR1("snd_pcm_set_params failed: %s", snd_strerror(status));
        } else {
            g_last_format = format;
            g_last_channel = connection->num_channel;
            g_last_bitspersample = connection->bit_per_sample;
            g_last_sample_rate = connection->sample_rate;
            //  g_avail_buffer_size=snd_pcm_avail_update(app_avk_cb.alsa_handle);

            if (g_BT_fc_enable) {
                fadeCache_init(&g_BT_fadeCache, app_avk_cb.alsa_handle, SNDSRC_BLUETOOTH,
                               g_wiimu_shm, g_last_sample_rate, g_last_format,
                               g_last_periodsize * g_last_channel * g_last_bitspersample / 8,
                               g_last_channel * g_last_bitspersample / 8);
                g_BT_fadeCache.reset_alsa_device = reset_alsa_device;
		  g_BT_fadeCache.pcm_fade_hook = pcm_fade_by_direct;
            }
        }

        // snd_pcm_info_set(app_avk_cb.alsa_handle , SNDSRC_BLUETOOTH);
    }
    pthread_mutex_unlock(&alsa_mutex);
    APP_DEBUG0("ALSA driver open ----");
    }
}

/*******************************************************************************
 **
** Function         app_avk_cback
**
** Description      This function is the AVK callback function
**
** Returns          void
**
*******************************************************************************/
static void app_avk_cback(tBSA_AVK_EVT event, tBSA_AVK_MSG *p_data)
{
    tAPP_AVK_CONNECTION *connection = NULL;
    switch (event) {
    case BSA_AVK_OPEN_EVT:
        APP_DEBUG1("BSA_AVK_OPEN_EVT status 0x%x, ccb_handle %d", p_data->open.status,
                   p_data->open.ccb_handle);

        if (p_data->open.status == BSA_SUCCESS) {
            connection = app_avk_add_connection(p_data->open.bd_addr);

            if (connection == NULL) {
                APP_DEBUG0("BSA_AVK_OPEN_EVT cannot allocate connection cb");
                break;
            }

            connection->ccb_handle = p_data->open.ccb_handle;
            connection->is_open = TRUE;
            connection->is_streaming_open = FALSE;
            printf("AVK connected to device index %d \n", connection->index);
        }
            app_avk_cb.index = app_avk_get_index_by_ad_addr(p_data->open.bd_addr);
            app_avk_cb.open_pending = FALSE;
            app_avk_send_delay_report(BT_AVK_DELAY_MS*10);  
	     app_avk_cb.has_volume_change_ = 1;

        printf("AVK connected to device %02X:%02X:%02X:%02X:%02X:%02X\n", p_data->open.bd_addr[0],
               p_data->open.bd_addr[1], p_data->open.bd_addr[2], p_data->open.bd_addr[3],
               p_data->open.bd_addr[4], p_data->open.bd_addr[5]);
        break;

    case BSA_AVK_CLOSE_EVT:
        /* Close event, reason BTA_AVK_CLOSE_STR_CLOSED or BTA_AVK_CLOSE_CONN_LOSS*/
        APP_DEBUG1(
            "BSA_AVK_CLOSE_EVT status 0x%x, handle %d, bd_addr: %02X:%02X:%02X:%02X:%02X:%02X",
            p_data->close.status, p_data->close.ccb_handle, p_data->close.bd_addr[0],
            p_data->close.bd_addr[1], p_data->close.bd_addr[2], p_data->close.bd_addr[3],
            p_data->close.bd_addr[4], p_data->close.bd_addr[5]);

#ifdef WMRMPLAY_ENABLE
        if(WMUtil_check_wmrm_used(&g_wmrm_used))
        {
        app_avk_close_alsa();
        }
        else
#endif
        {
        if (g_BT_fadeCache.cache_run) {
            fadeCache_eof(&g_BT_fadeCache);
           fadeCache_uninit(&g_BT_fadeCache, 0);
        }
        }
        connection = app_avk_find_connection_by_bd_addr(p_data->close.bd_addr);
        if (connection == NULL) {
            connection = app_avk_find_connection_by_index(
                app_avk_get_index_by_ad_addr(p_data->close.bd_addr));
            if (connection == NULL ||
                bdcmp(connection->bda_connected, p_data->close.bd_addr) != 0) {
                APP_DEBUG1("BSA_AVK_CLOSE_EVT unknown handle %d", p_data->close.ccb_handle);
                break;
            }
        }

        connection->is_open = FALSE;
        connection->is_rc_open = FALSE;
        connection->rc_handle = 0xFF;
        app_avk_cb.open_pending = FALSE;
        if (connection->is_started == TRUE) {
            connection->is_started = FALSE;

                if(connection->format == BSA_AVK_CODEC_M24) {
                    close(app_avk_cb.fd);
                    app_avk_cb.fd  = -1;
                } else
                    app_avk_close_wave_file(connection);
            }
            app_avk_reset_connection(p_data->close.bd_addr);
			
	pthread_mutex_lock(&aac_mutex);
        app_avk_aac_destroy_decoder();
    	pthread_mutex_unlock(&aac_mutex);
	//fixed iOS14 bug
	app_mgr_sec_disconnect(p_data->close.bd_addr);

            break;

        case BSA_AVK_START_EVT:
            APP_DEBUG1("BSA_AVK_START_EVT status 0x%x, streaming %d, discarded %d, ccb_handle %d, bd_addr: %02X:%02X:%02X:%02X:%02X:%02X", p_data->start.status, p_data->start.streaming, p_data->start.discarded, p_data->start.ccb_handle, p_data->start.bd_addr[0], p_data->start.bd_addr[1], p_data->start.bd_addr[2], p_data->start.bd_addr[3], p_data->start.bd_addr[4], p_data->start.bd_addr[5]);
          //  APP_DEBUG1("BSA_AVK_START_EVT media_reciving format: %d, pcm num_channel %d, sampling_freq %d, bit_per_sample %d", p_data->start.media_receiving.format, p_data->start.media_receiving.cfg.pcm.num_channel, p_data->start.media_receiving.cfg.pcm.sampling_freq, p_data->start.media_receiving.cfg.pcm.bit_per_sample);

            if (app_hs_is_calling()) {
                APP_INFO0("BSA_AVK_START_EVT app hs is calling, ignore");
                return;
            }
            {
                g_wiimu_shm->bt_connected_ts = tickSincePowerOn();
                if(p_data->start.streaming == FALSE) {
                    connection = app_avk_find_connection_by_av_handle(p_data->start.ccb_handle);
                    if(connection == NULL) {
                        return;
                    }
                    connection->is_streaming_open = TRUE;
                    return;
                } else {
                    /* We got START_EVT for a new device that is streaming but server discards the data
                        		because another stream is already active */
                    if(p_data->start.discarded) {
                        connection = app_avk_find_connection_by_bd_addr(p_data->start.bd_addr);
                        if(connection) {
                         //   connection->is_started = TRUE;
                            connection->is_streaming_open = TRUE;
                        }
                        return;
                    }
                    pStreamingConn = NULL;
                    connection = app_avk_find_connection_by_bd_addr(p_data->start.bd_addr);

                    if(connection == NULL || (connection->is_started)) {
			event = BSA_CUSTOMER_AVK_RESUME;
                       // return;
                    } else {
                        app_avk_pause_other_streams(p_data->start.bd_addr);
                        connection->is_started = TRUE;
                        if(g_wiimu_shm->bt_play_status == player_init)
                          g_wiimu_shm->bt_play_status = player_stopping;
                        app_avk_handle_start(p_data, connection);
                    connection->is_streaming_open = TRUE;
                    pStreamingConn = connection;
                }
            }
        }
        break;

        case BSA_AVK_STOP_EVT:
            APP_DEBUG1("BSA_AVK_STOP_EVT streaming: %d  Suspended: %d, status: %d, ccb_handle: %d, bd_addr: %02X:%02X:%02X:%02X:%02X:%02X", p_data->stop.streaming, p_data->stop.suspended, p_data->stop.status, p_data->stop.ccb_handle, p_data->stop.bd_addr[0], p_data->stop.bd_addr[1], p_data->stop.bd_addr[2], p_data->stop.bd_addr[3], p_data->stop.bd_addr[4], p_data->stop.bd_addr[5]);

            
            if(p_data->stop.suspended) {
                connection = app_avk_find_connection_by_bd_addr(p_data->stop.bd_addr);
                if(connection == NULL) {
                    connection = app_avk_find_connection_by_index(app_avk_get_index_by_ad_addr(p_data->stop.bd_addr));
                    if(connection == NULL || bdcmp(p_data->stop.bd_addr, connection->bda_connected) != 0) {
                        APP_ERROR0("BSA_AVK_STOP_EVT can't find stop bd_addr");
                        connection = NULL;
                    }
                }
                if(connection) {
                    connection->is_streaming_open = FALSE;
                    connection->is_started = FALSE;
                }
            } else if(p_data->stop.streaming == FALSE) {
                connection = app_avk_find_connection_by_index(app_avk_get_index_by_ad_addr(p_data->stop.bd_addr));
                if(connection == NULL) {
                    connection = app_avk_find_connection_by_av_handle(p_data->stop.ccb_handle);
                    if(connection == NULL || bdcmp(p_data->stop.bd_addr, connection->bda_connected) != 0) {
                        APP_ERROR0("BSA_AVK_STOP_EVT can't find stop bd_addr");
                        connection = NULL;
                    }
                }
                if(connection) {
                    connection->is_streaming_open = FALSE;
                    connection->is_started = FALSE;
                }
            } else {
                connection = app_avk_find_connection_by_index(app_avk_get_index_by_ad_addr(p_data->stop.bd_addr));
                if(connection == NULL) {
                    connection = app_avk_find_streaming_connection();
                    if(connection == NULL || bdcmp(p_data->stop.bd_addr, connection->bda_connected) != 0) {
                        APP_ERROR0("BSA_AVK_STOP_EVT can't find stop bd_addr");
                        connection = NULL;
                    }
                }
                if(connection) {
                    connection->is_started = FALSE;
    
                    if(connection->format == BSA_AVK_CODEC_M24) {
                        close(app_avk_cb.fd);
                        app_avk_cb.fd  = -1;
                    } else {
                        app_avk_close_wave_file(connection);
                    }
                }
            }
            if(app_avk_find_streaming_connection() == NULL) {
		 //    app_avk_cb.audio_play_flag = 0;
#ifdef WMRMPLAY_ENABLE
        if(WMUtil_check_wmrm_used(&g_wmrm_used))
        {
            app_avk_close_alsa();
        }
        else
#endif
        {
            if (g_BT_fc_enable && g_BT_fadeCache.cache_run) {
                fadeCache_eof(&g_BT_fadeCache);
               fadeCache_uninit(&g_BT_fadeCache, 0);
            //} else {
            //    WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 1, SNDSRC_BLUETOOTH, 0);
            }
        }
        }

        break;

    case BSA_AVK_RC_OPEN_EVT:

            if(p_data->rc_open.status == BSA_SUCCESS) {
                APP_DEBUG1("BSA_AVK_RC_OPEN_EVT status: %d, peer_version: %d, bd_addr: %02X:%02X:%02X:%02X:%02X:%02X", p_data->rc_open.status, p_data->rc_open.peer_version, p_data->rc_open.bd_addr[0], p_data->rc_open.bd_addr[1], p_data->rc_open.bd_addr[2], p_data->rc_open.bd_addr[3], p_data->rc_open.bd_addr[4], p_data->rc_open.bd_addr[5]);

                connection = app_avk_add_connection(p_data->rc_open.bd_addr);
                if(connection == NULL) {
                    APP_DEBUG0("BSA_AVK_RC_OPEN_EVT could not allocate connection");
                    break;
                }
                APP_DEBUG1("BSA_AVK_RC_OPEN_EVT peer_feature=0x%x rc_handle=%d", p_data->rc_open.peer_features,
                           p_data->rc_open.rc_handle);
                g_wiimu_shm->bt_connected_ts = tickSincePowerOn();
				app_avk_cb.rc_handle = p_data->rc_open.rc_handle;
                connection->rc_handle = p_data->rc_open.rc_handle;
                connection->peer_features =  p_data->rc_open.peer_features;
                connection->peer_version = p_data->rc_open.peer_version;
                connection->is_rc_open = TRUE;

                char cmd[100] = {0};
                sprintf(cmd, "GNOTIFY=VOLUMECHANGE%d", g_wiimu_shm->volume);
                NotifyMessage(BT_MANAGER_SOCKET, cmd);
                app_avk_rc_get_play_status_command(app_avk_cb.rc_handle);
            }
            break;

        case BSA_AVK_RC_PEER_OPEN_EVT:
            APP_DEBUG1("BSA_AVK_RC_PEER_OPEN_EVT status %d, peer_version: %d, index %d, bd_addr: %02X:%02X:%02X:%02X:%02X:%02X", p_data->rc_open.status, p_data->rc_open.peer_version, app_avk_cb.index, p_data->rc_open.bd_addr[0], p_data->rc_open.bd_addr[1], p_data->rc_open.bd_addr[2], p_data->rc_open.bd_addr[3], p_data->rc_open.bd_addr[4], p_data->rc_open.bd_addr[5]);
           
            //connection = app_avk_find_connection_by_rc_handle(p_data->rc_open.rc_handle);
            connection = app_avk_find_connection_by_index(app_avk_cb.index);
            if(connection == NULL) {
                APP_DEBUG1("BSA_AVK_RC_PEER_OPEN_EVT could not find connection handle %d", p_data->rc_open.rc_handle);
                break;
            }

            APP_DEBUG1("BSA_AVK_RC_PEER_OPEN_EVT peer_feature=0x%x rc_handle=%d", p_data->rc_open.peer_features,
                       p_data->rc_open.rc_handle);
			app_avk_cb.rc_handle = p_data->rc_open.rc_handle;
            connection->peer_features =  p_data->rc_open.peer_features;
            connection->peer_version = p_data->rc_open.peer_version;
            connection->is_rc_open = TRUE;
            break;

        case BSA_AVK_RC_CLOSE_EVT:
            APP_DEBUG1("BSA_AVK_RC_CLOSE_EVT rc_handle %d, status %d", p_data->rc_close.rc_handle, p_data->rc_close.status);
          
            connection = app_avk_find_connection_by_rc_handle(p_data->rc_close.rc_handle);
            if(connection == NULL) {
                break;
            }
            APP_DEBUG1("BSA_AVK_RC_CLOSE_EVT bd_addr: %02X:%02X:%02X:%02X:%02X:%02X", connection->bda_connected[0], connection->bda_connected[1], connection->bda_connected[2], connection->bda_connected[3], connection->bda_connected[4], connection->bda_connected[5]);

        connection->is_rc_open = FALSE;
        break;
		
    case BSA_AVK_VENDOR_CMD_EVT:
        APP_DEBUG0("BSA_AVK_VENDOR_CMD_EVT");
        APP_DEBUG1(" code:0x%x", p_data->vendor_cmd.code);
        APP_DEBUG1(" company_id:0x%x", p_data->vendor_cmd.company_id);
        APP_DEBUG1(" label:0x%x", p_data->vendor_cmd.label);
        APP_DEBUG1(" len:0x%x", p_data->vendor_cmd.len);
#if (defined(BSA_AVK_DUMP_RX_DATA) && (BSA_AVK_DUMP_RX_DATA == TRUE))
        APP_DUMP("A2DP Data", p_data->vendor_cmd.data, p_data->vendor_cmd.len);
#endif
        break;

    case BSA_AVK_VENDOR_RSP_EVT:
        APP_DEBUG0("BSA_AVK_VENDOR_RSP_EVT");
        APP_DEBUG1(" code:0x%x", p_data->vendor_rsp.code);
        APP_DEBUG1(" company_id:0x%x", p_data->vendor_rsp.company_id);
        APP_DEBUG1(" label:0x%x", p_data->vendor_rsp.label);
        APP_DEBUG1(" len:0x%x", p_data->vendor_rsp.len);
#if (defined(BSA_AVK_DUMP_RX_DATA) && (BSA_AVK_DUMP_RX_DATA == TRUE))
        APP_DUMP("A2DP Data", p_data->vendor_rsp.data, p_data->vendor_rsp.len);
#endif
        break;

    case BSA_AVK_CP_INFO_EVT:
        APP_DEBUG0("BSA_AVK_CP_INFO_EVT");
        if (p_data->cp_info.id == BSA_AVK_CP_SCMS_T_ID) {
            switch (p_data->cp_info.info.scmst_flag) {
            case BSA_AVK_CP_SCMS_COPY_NEVER:
                APP_INFO1(" content protection:0x%x - COPY NEVER", p_data->cp_info.info.scmst_flag);
                break;
            case BSA_AVK_CP_SCMS_COPY_ONCE:
                APP_INFO1(" content protection:0x%x - COPY ONCE", p_data->cp_info.info.scmst_flag);
                break;
            case BSA_AVK_CP_SCMS_COPY_FREE:
            case (BSA_AVK_CP_SCMS_COPY_FREE + 1):
                APP_INFO1(" content protection:0x%x - COPY FREE", p_data->cp_info.info.scmst_flag);
                break;
            default:
                APP_ERROR1(" content protection:0x%x - UNKNOWN VALUE",
                           p_data->cp_info.info.scmst_flag);
                break;
            }
        } else {
            APP_INFO0(" no content protection");
        }
        break;

    case BSA_AVK_REGISTER_NOTIFICATION_EVT:
        //  APP_INFO1("BSA_AVK_REGISTER_NOTIFICATION_EVT pdu %d. status=%d, opcode=%d, eid=%d ctype
        //  = %d",
        //            p_data->reg_notif.rsp.pdu,
        //            p_data->reg_notif.rsp.status,
        //            p_data->reg_notif.rsp.opcode,
        //            p_data->reg_notif.rsp.event_id,
        //            p_data->reg_notif.rsp.ctype
        //           );
        if (p_data->reg_notif.rsp.ctype == AVRC_RSP_INTERIM) {
            // skip AVRC RSP INTERIM
            return;
        }

        if (p_data->reg_notif.rsp.event_id == AVRC_EVT_PLAY_STATUS_CHANGE) {
            //  APP_INFO1("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_PLAY_STATUS_CHANGE
            //  play_status=%d, handle %d",
            //           (int)p_data->reg_notif.rsp.param.play_status, p_data->reg_notif.handle);
            // if (p_data->reg_notif.rsp.opcode == AVRC_RSP_CHANGED) {
            //	APP_INFO1("+++ Response CHANGED, playstatus: %d",
            //p_data->reg_notif.rsp.param.play_status);
            //} else if (p_data->reg_notif.rsp.opcode == AVRC_RSP_INTERIM) {
            //	APP_DEBUG1("++++ Response INTERIM, playstatus: %d",
            //p_data->reg_notif.rsp.param.play_status);
            //	break;
            //}
            switch (p_data->reg_notif.rsp.param.play_status) {
            case AVRC_PLAYSTATE_STOPPED:
                APP_INFO0("AVRC_Stopped!");
                app_avk_rc_get_play_status_command(app_avk_cb.rc_handle);
                break;
            case AVRC_PLAYSTATE_PLAYING:
                APP_INFO0("AVRC_Playing!");
                app_avk_rc_get_play_status_command(app_avk_cb.rc_handle);
                break;
            case AVRC_PLAYSTATE_PAUSED:
	        // app_avk_play_fade_operation(FADE_OP_PAUSE);	 
		  pthread_mutex_lock(&operation_mutex);
	         g_direction = DIRECTION_FADE_OUT;
		  pthread_mutex_unlock(&operation_mutex);
                APP_INFO0("AVRC_Paused!");
                app_avk_rc_get_play_status_command(app_avk_cb.rc_handle);
                break;
            case AVRC_PLAYSTATE_FWD_SEEK:
                APP_INFO0("AVRC_Fwd_seek!");
                break;
            case AVRC_PLAYSTATE_REV_SEEK:
                APP_INFO0("AVRC_Rev_seek!");
                break;
            case AVRC_PLAYSTATE_ERROR:
                APP_INFO0("avrcp play_error!");
                break;
            }
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_TRACK_CHANGE) {
            //    APP_INFO0("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_TRACK_CHANGE");
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_TRACK_REACHED_START) {
            //     APP_INFO0("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_TRACK_REACHED_START");
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_TRACK_REACHED_END) {
            //      APP_INFO0("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_TRACK_REACHED_END");
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_PLAY_POS_CHANGED) {
            //       APP_INFO1("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_PLAY_POS_CHANGED
            //       pos=%d",
            //                  (int)p_data->reg_notif.rsp.param.play_pos);
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_BATTERY_STATUS_CHANGE) {
            //        APP_INFO1("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_BATTERY_STATUS_CHANGE
            //        bat=%d",
            //                  (int)p_data->reg_notif.rsp.param.battery_status);
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_SYSTEM_STATUS_CHANGE) {
            //          APP_INFO1("   BSA_AVK_REGISTER_NOTIFICATION_EVT
            //          AVRC_EVT_SYSTEM_STATUS_CHANGE  sys=%d",
            //                  (int)p_data->reg_notif.rsp.param.system_status);
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_APP_SETTING_CHANGE) {
            //        APP_INFO0("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_APP_SETTING_CHANGE");
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_NOW_PLAYING_CHANGE) {
            //        APP_INFO0("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_NOW_PLAYING_CHANGE");
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_AVAL_PLAYERS_CHANGE) {
            //       APP_INFO0("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_AVAL_PLAYERS_CHANGE");
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_ADDR_PLAYER_CHANGE) {
            //         APP_INFO0("   BSA_AVK_REGISTER_NOTIFICATION_EVT
            //         AVRC_EVT_ADDR_PLAYER_CHANGE");
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_UIDS_CHANGE) {
            //        APP_INFO1("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_UIDS_CHANGE
            //        uid_counter=%d",
            //                  (int)p_data->reg_notif.rsp.param.uid_counter);
        } else if (p_data->reg_notif.rsp.event_id == AVRC_EVT_VOLUME_CHANGE) {
            //        APP_INFO1("   BSA_AVK_REGISTER_NOTIFICATION_EVT AVRC_EVT_VOLUME_CHANGE
            //        vol=%d",
            //                   (int)p_data->reg_notif.rsp.param.volume);
        }

        if (AVRC_STS_NO_ERROR == p_data->reg_notif.rsp.status &&
            p_data->reg_notif.rsp.event_id == AVRC_EVT_PLAY_STATUS_CHANGE) {
            connection = app_avk_find_connection_by_rc_handle(p_data->reg_notif.handle);
            if (connection != NULL) {
                connection->playstate = p_data->reg_notif.rsp.param.play_status;
            }
        }
        break;
    case BSA_AVK_LIST_PLAYER_APP_ATTR_EVT:
        APP_INFO0("BSA_AVK_LIST_PLAYER_APP_ATTR_EVT");
        break;
    case BSA_AVK_LIST_PLAYER_APP_VALUES_EVT:
        APP_INFO0("BSA_AVK_LIST_PLAYER_APP_VALUES_EVT");
        break;
    case BSA_AVK_SET_PLAYER_APP_VALUE_EVT:
        APP_INFO0("BSA_AVK_SET_PLAYER_APP_VALUE_EVT");
        break;
    case BSA_AVK_GET_PLAYER_APP_VALUE_EVT:
        APP_INFO0("BSA_AVK_GET_PLAYER_APP_VALUE_EVT");
        break;
    case BSA_AVK_GET_ELEM_ATTR_EVT:
        APP_INFO0("BSA_AVK_GET_ELEM_ATTR_EVT");
        break;
    case BSA_AVK_GET_PLAY_STATUS_EVT:
        APP_INFO0("BSA_AVK_GET_PLAY_STATUS_EVT");
       // if (p_data->get_play_status.rsp.play_status == AVRC_PLAYSTATE_PLAYING) {
      //  } else if (p_data->get_play_status.rsp.play_status == AVRC_PLAYSTATE_PAUSED) {
       // } else if (p_data->get_play_status.rsp.play_status == AVRC_PLAYSTATE_STOPPED) {
      //  }
     //   break;
    case BSA_AVK_SET_ADDRESSED_PLAYER_EVT:
        APP_INFO0("BSA_AVK_SET_ADDRESSED_PLAYER_EVT");
        break;
    case BSA_AVK_SET_BROWSED_PLAYER_EVT:
        APP_INFO0("BSA_AVK_SET_BROWSED_PLAYER_EVT");
        break;
    case BSA_AVK_GET_FOLDER_ITEMS_EVT:
        APP_INFO0("BSA_AVK_GET_FOLDER_ITEMS_EVT");
        break;
    case BSA_AVK_CHANGE_PATH_EVT:
        APP_INFO0("BSA_AVK_CHANGE_PATH_EVT");
        break;
    case BSA_AVK_GET_ITEM_ATTR_EVT:
        APP_INFO0("BSA_AVK_GET_ITEM_ATTR_EVT");
        break;
    case BSA_AVK_PLAY_ITEM_EVT:
        APP_INFO0("BSA_AVK_PLAY_ITEM_EVT");
        break;
    case BSA_AVK_ADD_TO_NOW_PLAYING_EVT:
        APP_INFO0("BSA_AVK_ADD_TO_NOW_PLAYING_EVT");
        break;

    case BSA_AVK_SET_ABS_VOL_CMD_EVT: {
        connection = app_avk_find_connection_by_rc_handle(p_data->abs_volume.handle);
        if ((connection != NULL) && connection->m_bAbsVolumeSupported) {
            /* Peer requested change in volume. Make the change and send response with new system
             * volume. BSA is TG role in AVK */
             pthread_mutex_lock(&volume_mutex);
            app_avk_cb.volume =  p_data->abs_volume.abs_volume_cmd.volume;
	     pthread_mutex_unlock(&volume_mutex);
            app_avk_set_abs_vol_rsp(p_data->abs_volume.abs_volume_cmd.volume, connection->rc_handle,
                                    p_data->abs_volume.label);
            APP_DEBUG1("BSA_AVK_SET_ABS_VOL_CMD_EVT %d Transaction Label %d connection "
                       "volchangelabel %d \r\n",
                       p_data->abs_volume.abs_volume_cmd.volume, p_data->abs_volume.label,
                       p_data->abs_volume.label);
        } else {
            APP_ERROR1("not changing volume connection 0x%x m_bAbsVolumeSupported %d", connection,
                       connection->m_bAbsVolumeSupported);
        }
    } break;

    case BSA_AVK_REG_NOTIFICATION_CMD_EVT: {
        APP_DEBUG0("BSA_AVK_REG_NOTIFICATION_CMD_EVT ");
        if (p_data->reg_notif_cmd.reg_notif_cmd.event_id == AVRC_EVT_VOLUME_CHANGE) {
		pthread_mutex_lock(&volume_mutex);
            connection = app_avk_find_connection_by_rc_handle(p_data->reg_notif_cmd.handle);
            if (connection != NULL) {
                     connection->m_bAbsVolumeSupported = TRUE;
			
	                 printf("app_avk_cb.has_volume_change_ %d app_avk_cb.volume %d volChangeLabel %d \r\n",app_avk_cb.has_volume_change_,app_avk_cb.volume,p_data->reg_notif_cmd.label);		 
			if(app_avk_cb.has_volume_change_)
			{
				app_avk_reg_notfn_rsp(app_avk_cb.volume, connection->rc_handle,p_data->reg_notif_cmd.label, 
					p_data->reg_notif_cmd.reg_notif_cmd.event_id,
										BTA_AV_RSP_CHANGED);
				app_avk_cb.has_volume_change_ = 0;		
			}else
			{
	                connection->volChangeLabel = p_data->reg_notif_cmd.label;
	                /* Peer requested registration for vol change event. Send response with current
	                 * system volume. BSA is TG role in AVK */
	                app_avk_reg_notfn_rsp(
	                    app_avk_cb.volume, p_data->reg_notif_cmd.handle, p_data->reg_notif_cmd.label,
	                    p_data->reg_notif_cmd.reg_notif_cmd.event_id, BTA_AV_RSP_INTERIM);
			}
            }
		pthread_mutex_unlock(&volume_mutex);
        }
    } break;

    default:
        APP_ERROR1("Unsupported event %d", event);
        break;
    }

    /* forward the callback to registered applications */
    if (s_pCallback)
        s_pCallback(event, p_data);
}

/*******************************************************************************
 **
** Function         app_avk_open
**
** Description      Example of function to open AVK connection
**
** Returns          void
**
*******************************************************************************/
int app_avk_open_by_addr(BD_ADDR *bd_addr_in)
{
    tBSA_STATUS status;
    int choice;
    BOOLEAN connect = FALSE;
    BD_ADDR bd_addr;
    UINT8 *p_name;
    tBSA_AVK_OPEN open_param;
    tAPP_AVK_CONNECTION *connection = NULL;
#if defined(DEWALT_BEHAVE)
    int try_count = 150;
#else
    int try_count = 250;
#endif
    int last_active = 0;

    if (app_avk_cb.open_pending) {
        APP_ERROR0("already trying to connect");
        return ALREADY_CONNECTING;
    }

    bdcpy(bd_addr, *bd_addr_in);
    printf("%s %d \r\n", __FUNCTION__, __LINE__);

    //    xml_db_lock();
    //    if(app_read_xml_remote_devices() == -1) {
    //        xml_db_unlock();
    //        return BT_DEVICES_NO_HISTORY;
    //    }
    //	printf("%s %d \r\n",__FUNCTION__,__LINE__);

    //    for(; choice < APP_NUM_ELEMENTS(app_xml_remote_devices_db); choice++) {
    //        if((app_xml_remote_devices_db[choice].in_use == TRUE)  &&
    //			(!bdcmp(app_xml_remote_devices_db[choice].bd_addr,bd_addr)))
    //		 {
    //                p_name = app_xml_remote_devices_db[choice].name;
    //                connect = TRUE;
    //                break;
    //         }
    //    }
    //    xml_db_unlock();
    //	printf("%s %d \r\n",__FUNCTION__,__LINE__);
    connect = TRUE;

    if (connect != FALSE) {
        /* Open AVK stream */
        // printf("Connecting to AV device:%s \n", p_name);

        app_avk_cb.open_pending = TRUE;

        BSA_AvkOpenInit(&open_param);
        memcpy((char *)(open_param.bd_addr), bd_addr, sizeof(BD_ADDR));

        open_param.sec_mask = g_avk_sec_mask;
        status = BSA_AvkOpen(&open_param);
        if (status != BSA_SUCCESS) {
            APP_ERROR1("Unable to connect to device %02X:%02X:%02X:%02X:%02X:%02X with status %d",
                       open_param.bd_addr[0], open_param.bd_addr[1], open_param.bd_addr[2],
                       open_param.bd_addr[3], open_param.bd_addr[4], open_param.bd_addr[5], status);

            app_avk_cb.open_pending = FALSE;
        } else {
            /* this is an active wait for demo purpose */
            APP_DEBUG0("waiting for AV connection to open\n");

            while (avk_is_open_pending() == TRUE && (try_count-- > 0 || g_sec_sp_cfm_req)) {
                if (g_sec_sp_cfm_req)
#if defined(DEWALT_BEHAVE)
                    try_count = 150;
#else
                    try_count = 250;
#endif
                usleep(20000);
            }

            connection = app_avk_find_connection_by_bd_addr(open_param.bd_addr);
            if (connection == NULL || connection->is_open == FALSE) {
                APP_ERROR0("failure opening AV connection  \n");
                return BT_CONNECTED_FAILED;
            } else {
                /* XML Database update should be done upon reception of AV OPEN event */
                // APP_DEBUG1("Connected to AV device:%s", p_name);
                /* Read the Remote device xml file to have a fresh view */
                xml_db_lock();
                app_read_xml_remote_devices();

                /* Add AV service for this devices in XML database */
                app_xml_add_trusted_services_db(
                    app_xml_remote_devices_db, APP_NUM_ELEMENTS(app_xml_remote_devices_db), bd_addr,
                    BSA_A2DP_SERVICE_MASK | BSA_AVRCP_SERVICE_MASK, PROFILE_TYPE_A2DP_SOURCE);

                //  app_xml_update_name_db(app_xml_remote_devices_db,
                //                         APP_NUM_ELEMENTS(app_xml_remote_devices_db), bd_addr,
                //                         p_name);

                /* Update database => write to disk */
                if (app_write_xml_remote_devices() < 0) {
                    APP_ERROR0("Failed to store remote devices database");
                }
                xml_db_unlock();

                return BT_CONNECTED_SUCCESSED;
            }
        }
    }

    return BT_HISTORY_NOT_FOUND;
}

/*******************************************************************************
 **
** Function         app_avk_open
**
** Description      Example of function to open AVK connection
**
** Returns          void
**
*******************************************************************************/
int app_avk_open(int input)
{
    tBSA_STATUS status;
    int choice;
    BOOLEAN connect = FALSE;
    BD_ADDR bd_addr;
    UINT8 *p_name;
    tBSA_AVK_OPEN open_param;
    tAPP_AVK_CONNECTION *connection = NULL;
    int try_count = 250;
    int last_active = 0;

    if (app_avk_cb.open_pending) {
        APP_ERROR0("already trying to connect");
        return -1;
    }
    choice = input;

    if (choice == -1) {
        last_active = 1;
        choice = 0;
    }

    /* Read the XML file which contains all the bonded devices */
    xml_db_lock();
    if (app_read_xml_remote_devices() == -1) {
        xml_db_unlock();
        return -2;
    }

    for (; choice < APP_NUM_ELEMENTS(app_xml_remote_devices_db); choice++) {
        if (app_xml_remote_devices_db[choice].in_use != FALSE) {
            if (last_active && app_xml_remote_devices_db[choice].last_active) {
                bdcpy(bd_addr, app_xml_remote_devices_db[choice].bd_addr);
                p_name = app_xml_remote_devices_db[choice].name;
                connect = TRUE;
                break;
            } else if (!last_active) {
                bdcpy(bd_addr, app_xml_remote_devices_db[choice].bd_addr);
                p_name = app_xml_remote_devices_db[choice].name;
                connect = TRUE;
                break;
            }
        } else {
            // APP_ERROR0("Device entry not in use");
        }
    }

    // else
    //{
    //    APP_ERROR0("Unsupported device number");
    //}

    xml_db_unlock();

    if (connect != FALSE) {
        /* Open AVK stream */
        printf("Connecting to AV device:%s \n", p_name);

        app_avk_cb.open_pending = TRUE;

        BSA_AvkOpenInit(&open_param);
        memcpy((char *)(open_param.bd_addr), bd_addr, sizeof(BD_ADDR));

        open_param.sec_mask = g_avk_sec_mask;
        status = BSA_AvkOpen(&open_param);
        if (status != BSA_SUCCESS) {
            APP_ERROR1("Unable to connect to device %02X:%02X:%02X:%02X:%02X:%02X with status %d",
                       open_param.bd_addr[0], open_param.bd_addr[1], open_param.bd_addr[2],
                       open_param.bd_addr[3], open_param.bd_addr[4], open_param.bd_addr[5], status);

            app_avk_cb.open_pending = FALSE;
        } else {
            /* this is an active wait for demo purpose */
            printf("waiting for AV connection to open\n");

            while (avk_is_open_pending() == TRUE && try_count--) {
                usleep(20000);
            }

            connection = app_avk_find_connection_by_bd_addr(open_param.bd_addr);
            if (connection == NULL || connection->is_open == FALSE) {
                printf("failure opening AV connection  \n");
                return -1;
            } else {
                /* XML Database update should be done upon reception of AV OPEN event */
                APP_DEBUG1("Connected to AV device:%s", p_name);
                /* Read the Remote device xml file to have a fresh view */
                xml_db_lock();
                app_read_xml_remote_devices();

                /* Add AV service for this devices in XML database */
                app_xml_add_trusted_services_db(
                    app_xml_remote_devices_db, APP_NUM_ELEMENTS(app_xml_remote_devices_db), bd_addr,
                    BSA_A2DP_SERVICE_MASK | BSA_AVRCP_SERVICE_MASK, PROFILE_TYPE_A2DP_SOURCE);

                app_xml_update_name_db(app_xml_remote_devices_db,
                                       APP_NUM_ELEMENTS(app_xml_remote_devices_db), bd_addr,
                                       p_name);

                /* Update database => write to disk */
                if (app_write_xml_remote_devices() < 0) {
                    APP_ERROR0("Failed to store remote devices database");
                }
                xml_db_unlock();

                return (input == -1) ? 0 : input;
            }
        }
    }

    return -3;
}

/*******************************************************************************
 **
** Function         app_avk_close
**
** Description      Function to close AVK connection
**
** Returns          void
**
*******************************************************************************/
void app_avk_close(BD_ADDR bda)
{
    tBSA_STATUS status;
    tBSA_AVK_CLOSE bsa_avk_close_param;
    tAPP_AVK_CONNECTION *connection = NULL;

    /* Close AVK connection */
    BSA_AvkCloseInit(&bsa_avk_close_param);

    connection = app_avk_find_connection_by_bd_addr(bda);

    if (connection == NULL) {
        APP_ERROR0("Unable to Close AVK connection , invalid BDA");
        return;
    }

    bsa_avk_close_param.ccb_handle = connection->ccb_handle;
    bsa_avk_close_param.rc_handle = connection->rc_handle;

    status = BSA_AvkClose(&bsa_avk_close_param);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Close AVK connection with status %d", status);
    } else {
        app_avk_reset_connection(bda);
    }
}

/*******************************************************************************
 **
** Function         app_avk_stop_all
**
** Description      Function to stop all AVK connections
**
** Returns          void
**
*******************************************************************************/
void app_avk_stop_all()
{
    int index;
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if (app_avk_cb.connections[index].in_use == TRUE) {
            app_avk_play_stop(&(app_avk_cb.connections[index]));
        }
    }
}

/*******************************************************************************
 **
** Function         app_avk_close_all
**
** Description      Function to close all AVK connections
**
** Returns          void
**
*******************************************************************************/
void app_avk_close_all()
{
    int index;
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if (app_avk_cb.connections[index].in_use == TRUE) {
            app_avk_close(app_avk_cb.connections[index].bda_connected);
        }
    }
}

/*******************************************************************************
**
** Function         app_avk_open_rc
**
** Description      Function to opens avrc controller connection. AVK should be open before opening
*rc.
**
** Returns          void
**
*******************************************************************************/
void app_avk_open_rc(BD_ADDR bd_addr)
{
    tBSA_STATUS status;
    tBSA_AVK_OPEN bsa_avk_open_param;

    /* open avrc connection */
    BSA_AvkOpenRcInit(&bsa_avk_open_param);
    bdcpy(bsa_avk_open_param.bd_addr, bd_addr);
    status = BSA_AvkOpenRc(&bsa_avk_open_param);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to open arvc connection with status %d", status);
    }
}

/*******************************************************************************
**
** Function         app_avk_close_rc
**
** Description      Function to closes avrc controller connection.
**
** Returns          void
**
*******************************************************************************/
void app_avk_close_rc(UINT8 rc_handle)
{
    tBSA_STATUS status;
    tBSA_AVK_CLOSE bsa_avk_close_param;

    /* close avrc connection */
    BSA_AvkCloseRcInit(&bsa_avk_close_param);

    bsa_avk_close_param.rc_handle = rc_handle;

    status = BSA_AvkCloseRc(&bsa_avk_close_param);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to close arvc connection with status %d", status);
    }
}

/*******************************************************************************
 **
** Function        app_avk_start
**
** Description     Example of function to start an av stream
**
** Returns         void
**
*******************************************************************************/
void app_avk_start(void)
{
    tBSA_STATUS status;
    tBSA_AVK_START req;

    /* Close AVK connection */
    BSA_AvkStartInit(&req);
    status = BSA_AvkStart(&req);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to start AVK connection with status %d", status);
    }
}

/*******************************************************************************
 **
** Function        app_avk_stop
**
** Description     Example of function to stop an av stream
**
** Returns         void
**
*******************************************************************************/
void app_avk_stop(BOOLEAN suspend)
{
    tBSA_STATUS status;
    tBSA_AVK_STOP req;

    /* Close AVK connection */
    BSA_AvkStopInit(&req);
    req.pause = suspend; /* suspend or local stop*/

    status = BSA_AvkStop(&req);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to stop AVK connection with status %d", status);
    }
}

/*******************************************************************************
**
** Function         app_avk_close_str
**
** Description      Function to close an A2DP Steam connection
**
** Returns          void
**
*******************************************************************************/
void app_avk_close_str(UINT8 ccb_handle)
{
    tBSA_STATUS status;
    tBSA_AVK_CLOSE_STR bsa_avk_close_str_param;

    /* close avrc connection */
    BSA_AvkCloseStrInit(&bsa_avk_close_str_param);

    bsa_avk_close_str_param.ccb_handle = ccb_handle;

    status = BSA_AvkCloseStr(&bsa_avk_close_str_param);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to close arvc connection with status %d", status);
    }
}

/*******************************************************************************
**
** Function        app_avk_register
**
** Description     Example of function to register an avk endpoint
**
** Returns         void
**
*******************************************************************************/
int app_avk_register(int b_support_aac_decode)
{
    tBSA_STATUS status;
    tBSA_AVK_REGISTER bsa_avk_register_param;

    /* register an audio AVK end point */
    BSA_AvkRegisterInit(&bsa_avk_register_param);

    bsa_avk_register_param.media_sup_format.audio.pcm_supported = TRUE;
    bsa_avk_register_param.media_sup_format.audio.sec_supported = TRUE;
    bsa_avk_register_param.media_sup_format.audio.aac_supported = FALSE;
#if (defined(BSA_AVK_AAC_SUPPORTED) && (BSA_AVK_AAC_SUPPORTED == TRUE))
    /* Enable AAC support in the app */
    if (b_support_aac_decode) {
        bsa_avk_register_param.media_sup_format.audio.aac_supported = TRUE;
    }
#endif
    strncpy(bsa_avk_register_param.service_name, "BSA Audio Service", BSA_AVK_SERVICE_NAME_LEN_MAX);

    bsa_avk_register_param.reg_notifications = reg_notifications;

    status = BSA_AvkRegister(&bsa_avk_register_param);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to register an AV sink with status %d", status);
        return -1;
    }
    /* Save UIPC channel */
    app_avk_cb.uipc_audio_channel = bsa_avk_register_param.uipc_channel;

    /* open the UIPC channel to receive the pcm */
    if (UIPC_Open(bsa_avk_register_param.uipc_channel, app_avk_uipc_cback) == FALSE) {
        APP_ERROR0("Unable to open UIPC channel");
        return -1;
    }

    app_avk_cb.fd = -1;

    return 0;
}

/*******************************************************************************
 **
** Function        app_avk_deregister
**
** Description     Example of function to deregister an avk endpoint
**
** Returns         void
**
*******************************************************************************/
void app_avk_deregister(void)
{
    tBSA_STATUS status;
    tBSA_AVK_DEREGISTER req;

    /* register an audio AVK end point */
    BSA_AvkDeregisterInit(&req);
    status = BSA_AvkDeregister(&req);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to deregister an AV sink with status %d", status);
    }

#ifdef TODO
    if (app_avk_cb.format == BSA_AVK_CODEC_M24) {
        close(app_avk_cb.fd);
        app_avk_cb.fd = -1;
    } else
        app_avk_close_wave_file();
#endif

    /* close the UIPC channel receiving the pcm */
    UIPC_Close(app_avk_cb.uipc_audio_channel);
    app_avk_cb.uipc_audio_channel = UIPC_CH_ID_BAD;
}
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;

#define SAVE_PCM_AUDIO_DATA 0
#define PCM_AUDIO_DATA_PATH "/mnt/usbdisk/btaudiox"

#if SAVE_PCM_AUDIO_DATA
static int audio_file_count = 0;
static int pcm_data_count = 0;
static int pcm_data_fd = -1;
static int save_pcm_data(char *in_buf, int in_len)
{
    char audio_file[128];

    if (pcm_data_fd < 0) {
        struct stat st;
        memset((void *)audio_file, 0, 128);
        sprintf(audio_file, "%s%d", PCM_AUDIO_DATA_PATH,
                audio_file_count); /* check directory: /mnt/sdcard/audioX */
        if (stat(audio_file, &st) < 0) {
            return 0;
        }
        sprintf(audio_file, "%s%d/audio%d.pcm", PCM_AUDIO_DATA_PATH, audio_file_count,
                audio_file_count);
        pcm_data_fd = open(audio_file, O_RDWR | O_CREAT | O_TRUNC, 0666);
        APP_INFO1("open %s: pcm_data_fd=%d\n", audio_file, pcm_data_fd);
        if (pcm_data_fd < 0) {
            printf("open %s error: %s\n", audio_file, strerror(errno));
            return 0;
        }
    }
    if (pcm_data_fd > 0) {
        pcm_data_count += in_len;
        if (write(pcm_data_fd, in_buf, in_len) < 0) {
            printf("write audio buf error: %s\n", strerror(errno));
            /* if(errno == EFBIG){ */
            /* 	close(pcm_data_fd); */
            /* 	pcm_data_fd = -1; */ /* } */
        }
    }

    if (pcm_data_count > (44100 * 2 * 2 * 30)) { /* 30s 44.1KHz */
        close(pcm_data_fd);
        APP_INFO1("close audio_file: pcm_data_fd=%d\n", pcm_data_fd);
        pcm_data_fd = -1;
        audio_file_count++;
        pcm_data_count = 0;
    }
    return 0;
}
#endif

/*******************************************************************************
**
** Function        app_avk_uipc_cback
**
** Description     uipc audio call back function.
**
** Parameters      pointer on a buffer containing PCM sample with a BT_HDR header
**
** Returns          void
**
*******************************************************************************/
static void app_avk_uipc_cback(BT_HDR *p_msg)
{
#ifdef PCM_ALSA
    snd_pcm_sframes_t alsa_frames;
    snd_pcm_sframes_t alsa_frames_to_send;
#endif
    UINT8 *p_buffer;
    int dummy;
    tAPP_AVK_CONNECTION *connection = NULL;
    int avail;
    int ret = 0;
    int    buffer_len = 0;
    if(p_msg == NULL) {
        APP_ERROR0("app_avk_uipc_cback p_msg is NULL ?!");
        return;
    }

    p_buffer = ((UINT8 *)(p_msg + 1)) + p_msg->offset;
    buffer_len = p_msg->len;

    connection = app_avk_find_streaming_connection();

    if (!connection) {
        APP_ERROR0("app_avk_uipc_cback unable to find connection!!!!!!");
        GKI_freebuf(p_msg);
	 g_cacheTimeAverage = 0;
        return;
    }

    if (connection->format == BSA_AVK_CODEC_M24) {
	pthread_mutex_lock(&aac_mutex);
        ret = app_avk_aac_decode_pcm(p_buffer, buffer_len, &p_buffer, &buffer_len);
	pthread_mutex_unlock(&aac_mutex);
	if(ret != TRUE)
            goto end;

    } else if (connection->format == BSA_AVK_CODEC_PCM &&
               app_avk_cb.fd_codec_type != BSA_AVK_CODEC_M24) {
        /* Invoke AAC decoder here for the current buffer.
            decode the AAC file that is created */
        if(app_avk_cb.fd != -1) {
            APP_DEBUG1("app_avk_uipc_cback Writing Data 0x%x, len = %d", p_buffer,buffer_len);
            dummy = write(app_avk_cb.fd, p_buffer, buffer_len);
            (void)dummy;
        }

#if (defined(BSA_AVK_DUMP_RX_DATA) && (BSA_AVK_DUMP_RX_DATA == TRUE))
        APP_DUMP("A2DP Data", p_buffer, buffer_len);
#endif
    }

    if (connection->num_channel == 0) {
        APP_ERROR0("connection->num_channel == 0");
        GKI_freebuf(p_msg);
        return;
    }
#ifdef WMRMPLAY_ENABLE
    if(WMUtil_check_wmrm_used(&g_wmrm_used))
    {
	if(p_buffer && buffer_len && g_wmrm_param.format == SND_PCM_FORMAT_S16_LE) {
        pthread_mutex_lock(&alsa_mutex);
        if (g_wiimu_shm->play_mode == PLAYER_MODE_BT && !g_wmrm_sound &&
            g_wmrm_param.format == SND_PCM_FORMAT_S16_LE) {
            g_wmrm_param.stream_id = stream_id_bluetooth;
            g_wmrm_param.play_leadtime = 800;
            g_wmrm_param.ctrl_leadtime = 50;
            g_wmrm_param.block = 1;
            g_wmrm_param.realtime_streaming = 1;
            g_wmrm_start_tick = 0;
            g_wmrm_render_sample = 0;
            g_wmrm_sound = wmrm_sound_init(&g_wmrm_param);
	    g_wmrm_playing = 1;
        }
        if (g_wiimu_shm->play_mode == PLAYER_MODE_BT && g_wmrm_sound && g_wmrm_playing) {
            if (g_wmrm_start_tick == 0) {
                g_wmrm_start_tick = tickSincePowerOn();
                g_wmrm_last_tick = tickSincePowerOn();
                g_wmrm_render_sample = 0;
                g_pcm_sample_total_count = 0;
            } else {
                long long cur_tick = tickSincePowerOn();
                g_wmrm_render_sample += buffer_len;
                g_pcm_sample_total_count =  g_wmrm_render_sample / connection->num_channel / 2;
                if(cur_tick >= g_wmrm_last_tick + 5000) {
                    int delay = wmrm_sound_get_delay(g_wmrm_sound);
                    printf("Bluetooth playback delay=%d\n", delay);

                    int offset =
                        (int)((cur_tick - g_wmrm_last_tick) * 441 / 10 - g_pcm_sample_total_count);
                    if (offset < 400 && offset > -400)
                        g_skew_offset += offset;
                    printf("Bluetooth playback samplerate=%d, total_offset=%d\n",
                           (int)(g_pcm_sample_total_count * 1000 / (cur_tick - g_wmrm_last_tick)),
                           g_skew_offset);
                    g_wmrm_last_tick = cur_tick;
                    g_wmrm_render_sample = 0;
                }
            }

            wmrm_sound_write(g_wmrm_sound, p_buffer, buffer_len);
        }
        pthread_mutex_unlock(&alsa_mutex);
    }
    }
    else
#endif
    {
    pthread_mutex_lock(&alsa_mutex);
    if(app_avk_cb.alsa_handle != NULL && p_buffer && buffer_len) {
        /* Compute number of PCM samples (contained in p_msg->len bytes) */
        alsa_frames_to_send = buffer_len / connection->num_channel;
        if(connection->bit_per_sample == 16)
            alsa_frames_to_send /= 2;

        g_pcm_sample_total_count += alsa_frames_to_send;
#ifdef PCM_SKEW_DRS
        if(g_BT_fc_enable&& (STATUS_RUN==g_BT_fadeCache.status || STATUS_RESUME ==g_BT_fadeCache.status)) {
            if(g_pcm_sample_total_count >  g_last_sample_rate * FADECACHE_CTRL_DELAY_BT) {
                unsigned char *pdata = p_buffer;
                int datalen = buffer_len;
		  snd_pcm_sframes_t delay = 0;
		  int cachedTime = fadeCache_get_delay(&g_BT_fadeCache, 0) * 1000;
		  if (snd_pcm_delay(g_BT_fadeCache.alsa_handler, &delay) == 0)
		  {
			cachedTime += (float)delay *1000 /(float)g_BT_fadeCache.alsa_samplerate;
		  }
		  int need_skew = 0;
		  int udiff = 0;
		  if(g_cacheTimeAverage == 0)g_cacheTimeAverage = cachedTime;
#define Mod32_GT(A, B) ((int32_t)(((uint32_t)(A)) - ((uint32_t)(B))) > 0)
#define Mod32_Diff(A, B) (Mod32_LT((A), (B)) ? ((B) - (A)) : ((A) - (B)))
#define Mod32_LT(A, B) ((int32_t)(((uint32_t)(A)) - ((uint32_t)(B))) < 0)
                udiff = Mod32_Diff(cachedTime,g_cacheTimeAverage);

		  if (Mod32_GT(cachedTime, g_cacheTimeAverage))
			 g_cacheTimeAverage += (udiff / 16);
		  else
			 g_cacheTimeAverage -= (udiff / 16);
		 int deltaSamples =(g_cacheTimeAverage-FADECACHE_CTRL_DELAY_BT)*44100/1000;
		 
		 if(deltaSamples < 2203 && deltaSamples > -2203)
		  {
	                int rtpSkewAdjust =
	                    (int)(1000.0 *
	                          lp_skew_PIDUpdate(g_pSkewIns, deltaSamples));
			 if(rtpSkewAdjust != 0)
			 {
	                int skewlen = lp_do_skew(g_pSkewIns,
	                                     pdata,
	                                     datalen / 4,
	                                     rtpSkewAdjust,
	                                     &pdata);
	                skewlen *= 4;
	              // if( skewlen-datalen != 0)
			//	wiimu_log(1, LOG_TYPE_TS, 12, 2,"deltaSamples %d skew %d sample ,cache remain %d ms g_cacheTimeAverage %d ms\r\n",
			//					  	deltaSamples,(skewlen-datalen)/4,cachedTime,g_cacheTimeAverage);	

	                  datalen = skewlen;
			 }
		  }else
		  {
			  lp_skew_PIDReset(g_pSkewIns);
			  if(g_cacheTimeAverage > 1000)
			  {
				if (g_BT_fadeCache.cache_run)
           				 fadeCache_alsa_reset(&g_BT_fadeCache);
				g_cacheTimeAverage = 0;
			  }
		  }
		   alsa_frames_to_send = datalen/4;
		   p_buffer = pdata;
            }
        }
#endif
        int i;

		if(app_avk_cb.audio_play_flag == 1)
		{
#if SAVE_PCM_AUDIO_DATA					
		save_pcm_data(p_buffer, alsa_frames_to_send*4);
#endif
		if(g_resample_instance) {
	            af_data_t *outdata;
	            g_af_data.audio = p_buffer;
	            g_af_data.len = alsa_frames_to_send*4;
	            outdata = linkPlay_resample(&g_af_data, g_resample_instance);
	            if(outdata) {
	                p_buffer = outdata->audio;
	                alsa_frames_to_send = outdata->len/4;
	            }
	    }
        if(g_BT_fc_enable && g_BT_fadeCache.cache_run) {
		 int cachedlength  = fadeCache_get_data(&g_BT_fadeCache);
		 if(cachedlength > FADECACHE_SIZE/2)
		 {
                    wiimu_log(1, LOG_TYPE_TS, 12, 2, "cachedlength %d \r\n",
                              cachedlength);		
		 }
                 //pcm_fade_by_direct(p_buffer, alsa_frames_to_send*2);
                 fadeCache_play(&g_BT_fadeCache, g_last_channel * g_last_bitspersample / 8,
                               alsa_frames_to_send, p_buffer);
            } else if (app_avk_cb.alsa_handle) {
                alsa_frames = snd_pcm_writei(app_avk_cb.alsa_handle, p_buffer, alsa_frames_to_send);
                if (alsa_frames < 0) {
                    alsa_frames = snd_pcm_recover(app_avk_cb.alsa_handle, alsa_frames, 0);
                    g_pcm_sample_total_count = 0;
                }
            }
        }
    } else {
        APP_DEBUG0("app_avk_uipc_cback snd_pcm_writei failed FINALLY !!\n");
    }
    pthread_mutex_unlock(&alsa_mutex);
    }

end:
    GKI_freebuf(p_msg);
}

/*******************************************************************************
**
** Function         app_avk_init
**
** Description      Init Manager application
**
** Parameters       Application callback (if null, default will be used)
**
** Returns          0 if successful, error code otherwise
**
*******************************************************************************/
int app_avk_init(tAvkCallback pcb /* = NULL */)
{
    tBSA_AVK_ENABLE bsa_avk_enable_param;
    tBSA_STATUS status;

    /* register callback */
    s_pCallback = pcb;

    /* Initialize the control structure */
    memset(&app_avk_cb, 0, sizeof(app_avk_cb));
    app_avk_cb.uipc_audio_channel = UIPC_CH_ID_BAD;

    app_avk_cb.fd = -1;
	app_avk_cb.open_pending = FALSE;
	 app_avk_cb.waiting_volume_callback_label_ = -1;
    /* set sytem vol at 50% */
    app_avk_cb.volume = (UINT8)((BSA_MAX_ABS_VOLUME - BSA_MIN_ABS_VOLUME) >> 1);
    app_avk_cb.play_state = AVRC_PLAYSTATE_STOPPED;
    g_fadeout_eachframe =(g_default_out_sample_rate *2/(1000/100) )/(volume_range.max-volume_range.min);
    g_fadeint_eachframe =(g_default_out_sample_rate *2/(1000/1000) )/(volume_range.max-volume_range.min);
   printf("g_fadeout_eachframe %d g_fadeint_eachframe %d \r\n",g_fadeout_eachframe,g_fadeint_eachframe);
   
    /* get hold on the AVK resource, synchronous mode */
    BSA_AvkEnableInit(&bsa_avk_enable_param);

    bsa_avk_enable_param.sec_mask = BSA_AVK_SECURITY;
    bsa_avk_enable_param.features = BSA_AVK_FEATURES;
    bsa_avk_enable_param.p_cback = app_avk_cback;

    status = BSA_AvkEnable(&bsa_avk_enable_param);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to enable an AV sink with status %d", status);

        if (BSA_ERROR_SRV_AVK_AV_REGISTERED == status) {
            APP_ERROR0("AV was registered before AVK, AVK should be registered before AV");
        }

        return -1;
    }

    return BSA_SUCCESS;
}

/*******************************************************************************
**
** Function         app_avk_rc_send_command
**
** Description      Example of Send a RemoteControl command
**
** Returns          void
**
*******************************************************************************/
void app_avk_rc_send_command(UINT8 key_state, UINT8 command, UINT8 rc_handle)
{
    int status;
    tBSA_AVK_REM_CMD bsa_avk_rc_cmd;

    /* Send remote control command */
    status = BSA_AvkRemoteCmdInit(&bsa_avk_rc_cmd);
    bsa_avk_rc_cmd.rc_handle = rc_handle;
    bsa_avk_rc_cmd.key_state = (tBSA_AVK_STATE)key_state;
    bsa_avk_rc_cmd.rc_id = (tBSA_AVK_RC)command;
    bsa_avk_rc_cmd.label = app_avk_get_label();

    status = BSA_AvkRemoteCmd(&bsa_avk_rc_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send AV RC Command %d, status %d", command, status);
    }
}

/*******************************************************************************
**
** Function         app_avk_rc_send_click
**
** Description      Send press and release states of a command
**
** Returns          void
**
*******************************************************************************/
void app_avk_rc_send_click(UINT8 command, UINT8 rc_handle)
{
    app_avk_rc_send_command(BSA_AV_STATE_PRESS, command, rc_handle);
    GKI_delay(300);
    app_avk_rc_send_command(BSA_AV_STATE_RELEASE, command, rc_handle);
}

/*******************************************************************************
**
** Function         app_avk_play_start
**
** Description      Example of start steam play
**
** Returns          void
**
*******************************************************************************/
void app_avk_play_start(tAPP_AVK_CONNECTION *connection)
{
#if 0
tAPP_AVK_CONNECTION *connection = NULL;
connection = app_avk_find_connection_by_rc_handle(rc_handle);
if(connection == NULL) {
    APP_DEBUG1("app_avk_play_start could not find connection handle %d", rc_handle);
    return;
}
#endif
    if (connection) {
        APP_DEBUG1("connection peer_features 0x%x, is_rc_open %d", connection->peer_features,
                   connection->is_rc_open);
        if ((connection->peer_features & BTA_RC_FEAT_CONTROL) && connection->is_rc_open) {
            app_avk_rc_send_click(BSA_AVK_RC_PLAY, connection->rc_handle);
        } else {
            //  APP_ERROR1("Unable to send AVRC command, is support RCTG %d, is rc open %d",
            //             (connection->peer_features & BTA_RC_FEAT_CONTROL),
            //             connection->is_rc_open);
            app_avk_start();
        }
    } else {
        APP_ERROR0("unknow connection");
    }
}

/*******************************************************************************
 **
** Function         app_avk_play_stop
**
** Description      Example of stop steam play
**
** Returns          void
**
*******************************************************************************/
void app_avk_play_stop(tAPP_AVK_CONNECTION *connection)
{
#if 0
tAPP_AVK_CONNECTION *connection = NULL;
connection = app_avk_find_connection_by_rc_handle(rc_handle);
if(connection == NULL) {
    APP_DEBUG1("app_avk_play_stop could not find connection handle %d", rc_handle);
    return;
}
#endif
    if (connection) {
        APP_DEBUG1("connection peer_features 0x%x, is_rc_open %d", connection->peer_features,
                   connection->is_rc_open);
        if ((connection->peer_features & BTA_RC_FEAT_CONTROL) && connection->is_rc_open) {
            app_avk_rc_send_click(BSA_AVK_RC_STOP, connection->rc_handle);
        } else {
            //  APP_ERROR1("Unable to send AVRC command, is support RCTG %d, is rc open %d",
            //             (connection->peer_features & BTA_RC_FEAT_CONTROL),
            //             connection->is_rc_open);
            app_avk_stop(TRUE);
        }
    } else {
        APP_ERROR0("unknow connection");
    }
}

/*******************************************************************************
 **
** Function         app_avk_play_pause
**
** Description      Example of pause steam pause
**
** Returns          void
**
*******************************************************************************/
void app_avk_play_pause(tAPP_AVK_CONNECTION *connection)
{
#if 0
tAPP_AVK_CONNECTION *connection = NULL;
connection = app_avk_find_connection_by_rc_handle(rc_handle);
if(connection == NULL) {
    APP_DEBUG1("app_avk_play_pause could not find connection handle %d", rc_handle);
    return;
}
#endif
    if (connection) {
        APP_DEBUG1("connection peer_features 0x%x, is_rc_open %d", connection->peer_features,
                   connection->is_rc_open);
        if ((connection->peer_features & BTA_RC_FEAT_CONTROL) && connection->is_rc_open) {
            app_avk_rc_send_click(BSA_AVK_RC_PAUSE, connection->rc_handle);
        } else {
            // APP_ERROR1("Unable to send AVRC command, is support RCTG %d, is rc open %d",
            //            (connection->peer_features & BTA_RC_FEAT_CONTROL),
            //            connection->is_rc_open);
            app_avk_stop(TRUE);
        }
    } else {
        APP_ERROR0("unknow connection");
    }
}

/*******************************************************************************
 **
** Function         app_avk_play_next_track
**
** Description      Example of play next track
**
** Returns          void
**
*******************************************************************************/
void app_avk_play_next_track(tAPP_AVK_CONNECTION *connection)
{
#if 0
tAPP_AVK_CONNECTION *connection = NULL;
connection = app_avk_find_connection_by_rc_handle(rc_handle);
if(connection == NULL) {
    APP_DEBUG1("could not find connection handle %d", rc_handle);
    return;
}
#endif
    if (connection) {
        APP_DEBUG1("connection peer_features 0x%x, is_rc_open %d", connection->peer_features,
                   connection->is_rc_open);
        if ((connection->peer_features & BTA_RC_FEAT_CONTROL) && connection->is_rc_open) {
            app_avk_rc_send_click(BSA_AVK_RC_FORWARD, connection->rc_handle);
        } else {
            APP_ERROR1("Unable to send AVRC command, is support RCTG %d, is rc open %d",
                       (connection->peer_features & BTA_RC_FEAT_CONTROL), connection->is_rc_open);
        }
    } else {
        APP_ERROR0("unknow connection");
    }
}

/*******************************************************************************
 **
** Function         app_avk_play_previous_track
**
** Description      Example of play previous track
**
** Returns          void
**
*******************************************************************************/
void app_avk_play_previous_track(tAPP_AVK_CONNECTION *connection)
{
#if 0
tAPP_AVK_CONNECTION *connection = NULL;
connection = app_avk_find_connection_by_rc_handle(rc_handle);
if(connection == NULL) {
    APP_DEBUG1("could not find connection handle %d", rc_handle);
    return;
}
#endif
    if (connection) {
        APP_DEBUG1("connection peer_features 0x%x, is_rc_open %d", connection->peer_features,
                   connection->is_rc_open);
        if ((connection->peer_features & BTA_RC_FEAT_CONTROL) && connection->is_rc_open) {
            app_avk_rc_send_click(BSA_AVK_RC_BACKWARD, connection->rc_handle);
        } else {
            APP_ERROR1("Unable to send AVRC command, is support RCTG %d, is rc open %d",
                       (connection->peer_features & BTA_RC_FEAT_CONTROL), connection->is_rc_open);
        }
    } else {
        APP_ERROR0("unknow connection");
    }
}

/*******************************************************************************
 **
** Function         app_avk_rc_cmd
**
** Description      Example of function to open AVK connection
**
** Returns          void
**
*******************************************************************************/
void app_avk_rc_cmd(UINT8 rc_handle)
{
    int choice;

    do {
        printf("Bluetooth AVK AVRC CMD menu:\n");
        printf("    0 play\n");
        printf("    1 stop\n");
        printf("    2 pause\n");
        printf("    3 forward\n");
        printf("    4 backward\n");
        printf("    5 rewind key_press\n");
        printf("    6 rewind key_release\n");
        printf("    7 fast forward key_press\n");
        printf("    8 fast forward key_release\n");

        printf("    99 exit\n");

        choice = app_get_choice("Select source");

        switch (choice) {
        case 0:
            app_avk_rc_send_click(BSA_AVK_RC_PLAY, rc_handle);
            break;

        case 1:
            app_avk_rc_send_click(BSA_AVK_RC_STOP, rc_handle);
            break;

        case 2:
            app_avk_rc_send_click(BSA_AVK_RC_PAUSE, rc_handle);
            break;

        case 3:
            app_avk_rc_send_click(BSA_AVK_RC_FORWARD, rc_handle);
            break;

        case 4:
            app_avk_rc_send_click(BSA_AVK_RC_BACKWARD, rc_handle);
            break;

        case 5:
            app_avk_rc_send_command(BSA_AV_STATE_PRESS, BSA_AVK_RC_REWIND, rc_handle);
            break;

        case 6:
            app_avk_rc_send_command(BSA_AV_STATE_RELEASE, BSA_AVK_RC_REWIND, rc_handle);
            break;

        case 7:
            app_avk_rc_send_command(BSA_AV_STATE_PRESS, BSA_AVK_RC_FAST_FOR, rc_handle);
            break;

        case 8:
            app_avk_rc_send_command(BSA_AV_STATE_RELEASE, BSA_AVK_RC_FAST_FOR, rc_handle);
            break;

        default:
            APP_ERROR0("Unsupported choice");
            break;
        }
    } while (choice != 99);
}

/*******************************************************************************
 **
** Function         app_avk_send_get_element_attributes_cmd
**
** Description      Example of function to send Vendor Dependent Command
**
** Returns          void
**
*******************************************************************************/
void app_avk_send_get_element_attributes_cmd(UINT8 rc_handle)
{
    int status;
    tBSA_AVK_VEN_CMD bsa_avk_vendor_cmd;

    /* Send remote control command */
    status = BSA_AvkVendorCmdInit(&bsa_avk_vendor_cmd);

    bsa_avk_vendor_cmd.rc_handle = rc_handle;
    bsa_avk_vendor_cmd.ctype = BSA_AVK_CMD_STATUS;
    bsa_avk_vendor_cmd.data[0] = BSA_AVK_RC_VD_GET_ELEMENT_ATTR;
    bsa_avk_vendor_cmd.data[1] = 0;    /* reserved & packet type */
    bsa_avk_vendor_cmd.data[2] = 0;    /* length high*/
    bsa_avk_vendor_cmd.data[3] = 0x11; /* length low*/

    /* data 4 ~ 11 are 0, which means "identifier 0x0 PLAYING" */

    bsa_avk_vendor_cmd.data[12] = 2; /* num of attributes */

    /* data 13 ~ 16 are 0x1, which means "attribute ID 1 : Title of media" */
    bsa_avk_vendor_cmd.data[16] = 0x1;

    /* data 17 ~ 20 are 0x7, which means "attribute ID 2 : Playing Time" */
    bsa_avk_vendor_cmd.data[20] = 0x7;

    bsa_avk_vendor_cmd.length = 21;
    bsa_avk_vendor_cmd.label = app_avk_get_label(); /* Just used to distinguish commands */

    status = BSA_AvkVendorCmd(&bsa_avk_vendor_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send AV Vendor Command %d", status);
    }
}

/*******************************************************************************
**
** Function         avk_is_open_pending
**
** Description      Check if AVK Open is pending
**
** Parameters
**
** Returns          TRUE if open is pending, FALSE otherwise
**
*******************************************************************************/
BOOLEAN avk_is_open_pending() { return app_avk_cb.open_pending; }

/*******************************************************************************
**
** Function         avk_set_open_pending
**
** Description      Set AVK open pending
**
** Parameters
**
** Returns          void
**
*******************************************************************************/
void avk_set_open_pending(BOOLEAN bopenpend) { app_avk_cb.open_pending = bopenpend; }

/*******************************************************************************
**
** Function         avk_is_open
**
** Description      Check if AVK is open
**
** Parameters
**
** Returns          TRUE if AVK is open, FALSE otherwise. Return BDA of the connected device
**
*******************************************************************************/
BOOLEAN avk_is_open(BD_ADDR bda)
{
    int index;
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if ((app_avk_cb.connections[index].in_use == TRUE) &&
            (app_avk_cb.connections[index].is_open == TRUE)) {
            bdcpy(bda, app_avk_cb.connections[index].bda_connected);
            return TRUE;
        }
    }

    return FALSE;
}

/*******************************************************************************
**
** Function         avk_is_rc_open
**
** Description      Check if AVRC is open
**
** Parameters
**
** Returns          TRUE if AVRC is open, FALSE otherwise.
**
*******************************************************************************/
BOOLEAN avk_is_rc_open(BD_ADDR bda)
{
    tAPP_AVK_CONNECTION *connection = app_avk_find_connection_by_bd_addr(bda);

    if (connection == NULL)
        return FALSE;

    return connection->is_rc_open;
}

/*******************************************************************************
**
** Function         app_avk_cancel
**
** Description      Example of cancel connection command
**
** Returns          void
**
*******************************************************************************/
void app_avk_cancel()
{
    int status;
    tBSA_AVK_CANCEL_CMD bsa_avk_cancel_cmd;

    /* Send remote control command */
    status = BSA_AvkCancelCmdInit(&bsa_avk_cancel_cmd);

    status = BSA_AvkCancelCmd(&bsa_avk_cancel_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_cancel_command %d", status);
    }
}

/*******************************************************************************
 **
** Function         app_avk_rc_get_element_attr_command
**
** Description      Example of get_element_attr_command command
**
** Returns          void
**
*******************************************************************************/
void app_avk_rc_get_element_attr_command(UINT8 rc_handle)
{
    int status;
    tBSA_AVK_GET_ELEMENT_ATTR bsa_avk_get_elem_attr_cmd;

    UINT8 attrs[] = {AVRC_MEDIA_ATTR_ID_TITLE,       AVRC_MEDIA_ATTR_ID_ARTIST,
                     AVRC_MEDIA_ATTR_ID_ALBUM,       AVRC_MEDIA_ATTR_ID_TRACK_NUM,
                     AVRC_MEDIA_ATTR_ID_NUM_TRACKS,  AVRC_MEDIA_ATTR_ID_GENRE,
                     AVRC_MEDIA_ATTR_ID_PLAYING_TIME};
    UINT8 num_attr = 7;

    /* Send command */
    status = BSA_AvkGetElementAttrCmdInit(&bsa_avk_get_elem_attr_cmd);
    bsa_avk_get_elem_attr_cmd.rc_handle = rc_handle;
    bsa_avk_get_elem_attr_cmd.label = app_avk_get_label(); /* Just used to distinguish commands */

    bsa_avk_get_elem_attr_cmd.num_attr = num_attr;
    memcpy(bsa_avk_get_elem_attr_cmd.attrs, attrs, sizeof(attrs));

    status = BSA_AvkGetElementAttrCmd(&bsa_avk_get_elem_attr_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_get_element_attr_command %d", status);
    }
}

/*******************************************************************************
 **
** Function         app_avk_rc_list_player_attr_command
**
** Description      Example of app_avk_rc_list_player_attr_command command
**
** Returns          void
**
*******************************************************************************/
void app_avk_rc_list_player_attr_command(UINT8 rc_handle)
{
    int status;
    tBSA_AVK_LIST_PLAYER_ATTR bsa_avk_list_player_attr_cmd;

    /* Send command */
    status = BSA_AvkListPlayerAttrCmdInit(&bsa_avk_list_player_attr_cmd);
    bsa_avk_list_player_attr_cmd.rc_handle = rc_handle;
    bsa_avk_list_player_attr_cmd.label =
        app_avk_get_label(); /* Just used to distinguish commands */

    status = BSA_AvkListPlayerAttrCmd(&bsa_avk_list_player_attr_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_list_player_attr_command %d", status);
    }
}

/*******************************************************************************
 **
** Function         app_avk_rc_list_player_attr_value_command
**
** Description      Example of app_avk_rc_list_player_attr_value_command command
**
** Returns          void
**
*******************************************************************************/
void app_avk_rc_list_player_attr_value_command(UINT8 attr, UINT8 rc_handle)
{
    int status;
    tBSA_AVK_LIST_PLAYER_VALUES bsa_avk_list_player_attr_cmd;

    /* Send command */
    status = BSA_AvkListPlayerValuesCmdInit(&bsa_avk_list_player_attr_cmd);
    bsa_avk_list_player_attr_cmd.rc_handle = rc_handle;
    bsa_avk_list_player_attr_cmd.label =
        app_avk_get_label(); /* Just used to distinguish commands */
    bsa_avk_list_player_attr_cmd.attr = attr;

    status = BSA_AvkListPlayerValuesCmd(&bsa_avk_list_player_attr_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_list_player_attr_command %d", status);
    }
}

/*******************************************************************************
 **
** Function         app_avk_rc_get_player_value_command
**
** Description      Example of get_player_value_command command
**
** Returns          void
**
*******************************************************************************/
void app_avk_rc_get_player_value_command(UINT8 *attrs, UINT8 num_attr, UINT8 rc_handle)
{
    int status;
    tBSA_AVK_GET_PLAYER_VALUE bsa_avk_get_player_val_cmd;

    /* Send command */
    status = BSA_AvkGetPlayerValueCmdInit(&bsa_avk_get_player_val_cmd);
    bsa_avk_get_player_val_cmd.rc_handle = rc_handle;
    bsa_avk_get_player_val_cmd.label = app_avk_get_label(); /* Just used to distinguish commands */
    bsa_avk_get_player_val_cmd.num_attr = num_attr;

    memcpy(bsa_avk_get_player_val_cmd.attrs, attrs, sizeof(UINT8) * num_attr);

    status = BSA_AvkGetPlayerValueCmd(&bsa_avk_get_player_val_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_get_player_value_command %d", status);
    }
}

/*******************************************************************************
 **
** Function         app_avk_rc_set_player_value_command
**
** Description      Example of set_player_value_command command
**
** Returns          void
**
*******************************************************************************/
void app_avk_rc_set_player_value_command(UINT8 num_attr, UINT8 *attr_ids, UINT8 *attr_val,
                                         UINT8 rc_handle)
{
    int status;
    tBSA_AVK_SET_PLAYER_VALUE bsa_avk_set_player_val_cmd;

    /* Send command */
    status = BSA_AvkSetPlayerValueCmdInit(&bsa_avk_set_player_val_cmd);
    bsa_avk_set_player_val_cmd.rc_handle = rc_handle;
    bsa_avk_set_player_val_cmd.label = app_avk_get_label(); /* Just used to distinguish commands */
    bsa_avk_set_player_val_cmd.num_attr = num_attr;

    memcpy(bsa_avk_set_player_val_cmd.player_attr, attr_ids, sizeof(UINT8) * num_attr);
    memcpy(bsa_avk_set_player_val_cmd.player_value, attr_val, sizeof(UINT8) * num_attr);

    status = BSA_AvkSetPlayerValueCmd(&bsa_avk_set_player_val_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_set_player_value_command %d", status);
    }
}

/*******************************************************************************
 **
** Function         app_avk_rc_get_play_status_command
**
** Description      Example of get_play_status
**
** Returns          void
**
*******************************************************************************/
void app_avk_rc_get_play_status_command(UINT8 rc_handle)
{
    int status;
    tBSA_AVK_GET_PLAY_STATUS bsa_avk_play_status_cmd;

    /* Send command */
    status = BSA_AvkGetPlayStatusCmdInit(&bsa_avk_play_status_cmd);
    bsa_avk_play_status_cmd.rc_handle = rc_handle;
    bsa_avk_play_status_cmd.label = app_avk_get_label(); /* Just used to distinguish commands */

    status = BSA_AvkGetPlayStatusCmd(&bsa_avk_play_status_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_get_play_status_command %d", status);
    }
}

/*******************************************************************************
 **
** Function         app_avk_rc_set_browsed_player_command
**
** Description      Example of set_browsed_player
**
** Returns          void
**
*******************************************************************************/
void app_avk_rc_set_browsed_player_command(UINT16 player_id, UINT8 rc_handle)
{
    int status;
    tBSA_AVK_SET_BROWSED_PLAYER bsa_avk_set_browsed_player;

    /* Send command */
    status = BSA_AvkSetBrowsedPlayerCmdInit(&bsa_avk_set_browsed_player);
    bsa_avk_set_browsed_player.player_id = player_id;
    bsa_avk_set_browsed_player.rc_handle = rc_handle;
    bsa_avk_set_browsed_player.label = app_avk_get_label(); /* Just used to distinguish commands */

    status = BSA_AvkSetBrowsedPlayerCmd(&bsa_avk_set_browsed_player);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_set_browsed_player_command %d", status);
    }
}

/*******************************************************************************
 **
** Function         app_avk_rc_set_addressed_player_command
**
** Description      Example of set_addressed_player
**
** Returns          void
**
*******************************************************************************/
void app_avk_rc_set_addressed_player_command(UINT16 player_id, UINT8 rc_handle)
{
    int status;
    tBSA_AVK_SET_ADDR_PLAYER bsa_avk_set_player;

    /* Send command */
    status = BSA_AvkSetAddressedPlayerCmdInit(&bsa_avk_set_player);
    bsa_avk_set_player.player_id = player_id;
    bsa_avk_set_player.rc_handle = rc_handle;
    bsa_avk_set_player.label = app_avk_get_label(); /* Just used to distinguish commands */

    status = BSA_AvkSetAddressedPlayerCmd(&bsa_avk_set_player);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_set_addressed_player_command %d", status);
    }
}

/*******************************************************************************
 **
** Function         app_avk_rc_change_path_command
**
** Description      Example of change_path
**
** Returns          void
**
*******************************************************************************/
void app_avk_rc_change_path_command(UINT16 uid_counter, UINT8 direction, tAVRC_UID folder_uid,
                                    UINT8 rc_handle)
{
    int status;
    tBSA_AVK_CHG_PATH bsa_change_path;

    /* Send command */
    status = BSA_AvkChangePathCmdInit(&bsa_change_path);

    bsa_change_path.rc_handle = rc_handle;
    bsa_change_path.label = app_avk_get_label(); /* Just used to distinguish commands */

    bsa_change_path.uid_counter = uid_counter;
    bsa_change_path.direction = direction;
    memcpy(bsa_change_path.folder_uid, folder_uid, sizeof(tAVRC_UID));

    status = BSA_AvkChangePathCmd(&bsa_change_path);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_change_path_command %d", status);
    }
}

/*******************************************************************************
 **
** Function         app_avk_rc_get_folder_items
**
** Description      Example of get_folder_items
**
** Returns          void
**
*******************************************************************************/
void app_avk_rc_get_folder_items(UINT8 scope, UINT32 start_item, UINT32 end_item, UINT8 rc_handle)
{
    int status;
    tBSA_AVK_GET_FOLDER_ITEMS bsa_get_folder_items;

    UINT32 attrs[] = {AVRC_MEDIA_ATTR_ID_TITLE,       AVRC_MEDIA_ATTR_ID_ARTIST,
                      AVRC_MEDIA_ATTR_ID_ALBUM,       AVRC_MEDIA_ATTR_ID_TRACK_NUM,
                      AVRC_MEDIA_ATTR_ID_NUM_TRACKS,  AVRC_MEDIA_ATTR_ID_GENRE,
                      AVRC_MEDIA_ATTR_ID_PLAYING_TIME};
    UINT8 num_attr = 7;

    /* Send command */
    status = BSA_AvkGetFolderItemsCmdInit(&bsa_get_folder_items);

    bsa_get_folder_items.rc_handle = rc_handle;
    bsa_get_folder_items.label = app_avk_get_label(); /* Just used to distinguish commands */

    bsa_get_folder_items.scope = scope;
    bsa_get_folder_items.start_item = start_item;
    bsa_get_folder_items.end_item = end_item;

    if (AVRC_SCOPE_PLAYER_LIST != scope) {
        bsa_get_folder_items.attr_count = num_attr;
        memcpy(bsa_get_folder_items.attrs, attrs, sizeof(attrs));
    }

    status = BSA_AvkGetFolderItemsCmd(&bsa_get_folder_items);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_get_folder_items %d", status);
    }
}

/*******************************************************************************
 **
** Function         app_avk_rc_get_items_attr
**
** Description      Example of get_items_attr
**
** Returns          void
**
*******************************************************************************/
void app_avk_rc_get_items_attr(UINT8 scope, tAVRC_UID uid, UINT16 uid_counter, UINT8 rc_handle)
{
    int status;
    tBSA_AVK_GET_ITEMS_ATTR bsa_get_items_attr;

    UINT8 attrs[] = {AVRC_MEDIA_ATTR_ID_TITLE,       AVRC_MEDIA_ATTR_ID_ARTIST,
                     AVRC_MEDIA_ATTR_ID_ALBUM,       AVRC_MEDIA_ATTR_ID_TRACK_NUM,
                     AVRC_MEDIA_ATTR_ID_NUM_TRACKS,  AVRC_MEDIA_ATTR_ID_GENRE,
                     AVRC_MEDIA_ATTR_ID_PLAYING_TIME};
    UINT8 num_attr = 7;

    /* Send command */
    status = BSA_AvkGetItemsAttrCmdInit(&bsa_get_items_attr);

    bsa_get_items_attr.rc_handle = rc_handle;
    bsa_get_items_attr.label = app_avk_get_label(); /* Just used to distinguish commands */

    bsa_get_items_attr.scope = scope;
    memcpy(bsa_get_items_attr.uid, uid, sizeof(tAVRC_UID));
    bsa_get_items_attr.uid_counter = uid_counter;
    bsa_get_items_attr.attr_count = num_attr;
    memcpy(bsa_get_items_attr.attrs, attrs, sizeof(attrs));

    status = BSA_AvkGetItemsAttrCmd(&bsa_get_items_attr);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_get_items_attr %d", status);
    }
}

/*******************************************************************************
 **
** Function         app_avk_rc_play_item
**
** Description      Example of play_item
**
** Returns          void
**
*******************************************************************************/
void app_avk_rc_play_item(UINT8 scope, tAVRC_UID uid, UINT16 uid_counter, UINT8 rc_handle)
{
    int status;
    tBSA_AVK_PLAY_ITEM bsa_play_item;

    /* Send command */
    status = BSA_AvkPlayItemCmdInit(&bsa_play_item);

    bsa_play_item.rc_handle = rc_handle;
    bsa_play_item.label = app_avk_get_label(); /* Just used to distinguish commands */

    bsa_play_item.scope = scope;
    memcpy(bsa_play_item.uid, uid, sizeof(tAVRC_UID));
    bsa_play_item.uid_counter = uid_counter;

    status = BSA_AvkPlayItemCmd(&bsa_play_item);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_play_item %d", status);
    }
}

/*******************************************************************************
 **
** Function         app_avk_rc_add_to_play
**
** Description      Example of add_to_play
**
** Returns          void
**
*******************************************************************************/
void app_avk_rc_add_to_play(UINT8 scope, tAVRC_UID uid, UINT16 uid_counter, UINT8 rc_handle)
{
    int status;
    tBSA_AVK_ADD_TO_PLAY bsa_add_play;

    /* Send command */
    status = BSA_AvkAddToPlayCmdInit(&bsa_add_play);

    bsa_add_play.rc_handle = rc_handle;
    bsa_add_play.label = app_avk_get_label(); /* Just used to distinguish commands */

    bsa_add_play.scope = scope;
    memcpy(bsa_add_play.uid, uid, sizeof(tAVRC_UID));
    bsa_add_play.uid_counter = uid_counter;

    status = BSA_AvkAddToPlayCmd(&bsa_add_play);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send app_avk_rc_add_to_play %d", status);
    }
}

/*******************************************************************************
 **
** Function         app_avk_set_abs_vol_rsp
**
** Description      This function sends abs vol response
**
** Returns          None
**
*******************************************************************************/
void app_avk_set_abs_vol_rsp(UINT8 volume, UINT8 rc_handle, UINT8 label)
{
    int status;
    tBSA_AVK_SET_ABS_VOLUME_RSP abs_vol;
    BSA_AvkSetAbsVolumeRspInit(&abs_vol);

    abs_vol.label = label;
    abs_vol.rc_handle = rc_handle;
    abs_vol.volume = volume;
    status = BSA_AvkSetAbsVolumeRsp(&abs_vol);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to send app_avk_set_abs_vol_rsp %d", status);
    }
}
int app_avk_set_abs_vol(UINT8 volume,int index)
{
	int ret = -1;
	tAPP_AVK_CONNECTION *connection = NULL;
	pthread_mutex_lock(&volume_mutex);
	if(app_avk_cb.volume == volume)
	{
		pthread_mutex_unlock(&volume_mutex);
		return 0;
	}
       app_avk_cb.volume = volume;
	  connection = app_avk_find_connection_by_index(index);
	  if(connection) {
	        if(connection->volChangeLabel  >=0)
	        {
		  	app_avk_reg_notfn_rsp(volume, connection->rc_handle, connection->volChangeLabel, AVRC_EVT_VOLUME_CHANGE,
								BTA_AV_RSP_CHANGED);
			
			connection->volChangeLabel = -1;
	        }else
	        {
			app_avk_cb.has_volume_change_ = 1;
	        }
		  ret = 0;
	  } else {
		  printf("Unknown choice:%d\n", index);
		  ret = -1;
	  }
	  pthread_mutex_unlock(&volume_mutex);

	  return ret;


}
/*******************************************************************************
 **
** Function         app_avk_reg_notfn_rsp
**
** Description      This function sends reg notfn response (BSA as TG role in AVK)
**
** Returns          none
**
*******************************************************************************/
void app_avk_reg_notfn_rsp(UINT8 volume, UINT8 rc_handle, UINT8 label, UINT8 event,
                           tBTA_AV_CODE code)
{
    int status;
    tBSA_AVK_REG_NOTIF_RSP reg_notf;
    BSA_AvkRegNotifRspInit(&reg_notf);
  //  app_avk_cb.volume = volume;
    reg_notf.reg_notf.param.volume  = volume;
    reg_notf.reg_notf.event_id      = event;
    reg_notf.rc_handle              = rc_handle;
    reg_notf.label                  = label;
    reg_notf.code                   = code;

    status = BSA_AvkRegNotifRsp(&reg_notf);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to send app_avk_reg_notfn_rsp %d", status);
    }
}
void app_avk_sync_vol(UINT8 volume)
{
    app_avk_cb.volume = volume;
}

/*******************************************************************************
 **
** Function         app_avk_send_get_capabilities
**
** Description      Sample code for attaining capability for events
**
** Returns          void
**
*******************************************************************************/
void app_avk_send_get_capabilities(UINT8 rc_handle)
{
    int status;
    tBSA_AVK_VEN_CMD bsa_avk_vendor_cmd;

    /* Send remote control command */
    status = BSA_AvkVendorCmdInit(&bsa_avk_vendor_cmd);

    bsa_avk_vendor_cmd.rc_handle = rc_handle;
    bsa_avk_vendor_cmd.ctype = BSA_AVK_CMD_STATUS;
    bsa_avk_vendor_cmd.data[0] = BSA_AVK_RC_VD_GET_CAPABILITIES;
    bsa_avk_vendor_cmd.data[1] = 0;    /* reserved & packet type */
    bsa_avk_vendor_cmd.data[2] = 0;    /* length high*/
    bsa_avk_vendor_cmd.data[3] = 1;    /* length low*/
    bsa_avk_vendor_cmd.data[4] = 0x03; /* for event*/

    bsa_avk_vendor_cmd.length = 5;
    bsa_avk_vendor_cmd.label = app_avk_get_label(); /* Just used to distinguish commands */

    status = BSA_AvkVendorCmd(&bsa_avk_vendor_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send AV Vendor Command %d", status);
    }
}

/*******************************************************************************
 **
** Function         app_avk_send_register_notification
**
** Description      Sample code for attaining capability for events
**
** Returns          void
**
*******************************************************************************/
void app_avk_send_register_notification(int evt, UINT8 rc_handle)
{
    int status;
    tBSA_AVK_VEN_CMD bsa_avk_vendor_cmd;

    /* Send remote control command */
    status = BSA_AvkVendorCmdInit(&bsa_avk_vendor_cmd);

    bsa_avk_vendor_cmd.rc_handle = rc_handle;
    bsa_avk_vendor_cmd.ctype = BSA_AVK_CMD_NOTIF;
    bsa_avk_vendor_cmd.data[0] = BSA_AVK_RC_VD_REGISTER_NOTIFICATION;
    bsa_avk_vendor_cmd.data[1] = 0;   /* reserved & packet type */
    bsa_avk_vendor_cmd.data[2] = 0;   /* length high*/
    bsa_avk_vendor_cmd.data[3] = 5;   /* length low*/
    bsa_avk_vendor_cmd.data[4] = evt; /* length low*/

    bsa_avk_vendor_cmd.length = 9;
    bsa_avk_vendor_cmd.label = app_avk_get_label(); /* Just used to distinguish commands */

    status = BSA_AvkVendorCmd(&bsa_avk_vendor_cmd);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send AV Vendor Command %d", status);
    }
}

/*******************************************************************************
 **
** Function         app_avk_add_connection
**
** Description      This function adds a connection
**
** Returns          Pointer to the found structure or NULL
**
*******************************************************************************/
tAPP_AVK_CONNECTION *app_avk_add_connection(BD_ADDR bd_addr)
{
    int index;
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if (bdcmp(app_avk_cb.connections[index].bda_connected, bd_addr) == 0) {
            app_avk_cb.connections[index].in_use = TRUE;
            return &app_avk_cb.connections[index];
        }
    }

    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if (app_avk_cb.connections[index].in_use == FALSE) {
            bdcpy(app_avk_cb.connections[index].bda_connected, bd_addr);
            app_avk_cb.connections[index].in_use = TRUE;
            app_avk_cb.connections[index].index = index;
            return &app_avk_cb.connections[index];
        }
    }

    return NULL;
}
/*******************************************************************************
 **
** Function         app_avk_reset_connection
**
** Description      This function resets a connection
**
**
*******************************************************************************/
void app_avk_init_connection()
{
    int index;
    for(index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
		memset(&app_avk_cb.connections[index],0x0,sizeof(app_avk_cb.connections[index]));
		app_avk_cb.connections[index].index = index;
		app_avk_cb.connections[index].rc_handle = 0xFF;
		app_avk_cb.connections[index].volChangeLabel = -1;
    }
}

/*******************************************************************************
 **
** Function         app_avk_reset_connection
**
** Description      This function resets a connection
**
**
*******************************************************************************/
void app_avk_reset_connection(BD_ADDR bd_addr)
{
    int index;

    printf("app_avk_reset_connection %02x:%02x:%02x:%02x:%02x:%02x \r\n", bd_addr[0], bd_addr[1],
           bd_addr[2], bd_addr[3], bd_addr[4], bd_addr[5]);
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if (bdcmp(app_avk_cb.connections[index].bda_connected, bd_addr) == 0) {
            app_avk_cb.connections[index].in_use = FALSE;

            app_avk_cb.connections[index].m_uiAddressedPlayer = 0;
            app_avk_cb.connections[index].m_uidCounterAddrPlayer = 0;
            app_avk_cb.connections[index].m_uid_counter = 0;
            app_avk_cb.connections[index].m_bDeviceSupportBrowse = FALSE;
            app_avk_cb.connections[index].m_uiBrowsedPlayer = 0;
            app_avk_cb.connections[index].m_iCurrentFolderItemCount = 0;
            app_avk_cb.connections[index].m_bAbsVolumeSupported = FALSE;
            app_avk_cb.connections[index].playstate = 0;
	     app_avk_cb.connections[index].volChangeLabel = -1;
            break;
        }
    }
}

/*******************************************************************************
 **
** Function         app_avk_find_connection_by_av_handle
**
** Description      This function finds the connection structure by its handle
**
** Returns          Pointer to the found structure or NULL
**
*******************************************************************************/
tAPP_AVK_CONNECTION *app_avk_find_connection_by_av_handle(UINT8 handle)
{
    int index;
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if (app_avk_cb.connections[index].ccb_handle == handle)
            return &app_avk_cb.connections[index];
    }
    return NULL;
}

/*******************************************************************************
 **
** Function         app_avk_find_connection_by_rc_handle
**
** Description      This function finds the connection structure by its handle
**
** Returns          Pointer to the found structure or NULL
**
*******************************************************************************/
tAPP_AVK_CONNECTION *app_avk_find_connection_by_rc_handle(UINT8 handle)
{
    int index;
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if (app_avk_cb.connections[index].rc_handle == handle)
            return &app_avk_cb.connections[index];
    }
    return NULL;
}

/*******************************************************************************
 **
** Function         app_avk_find_connection_by_bd_addr
**
** Description      This function finds the connection structure by its handle
**
** Returns          Pointer to the found structure or NULL
**
*******************************************************************************/
tAPP_AVK_CONNECTION *app_avk_find_connection_by_bd_addr(BD_ADDR bd_addr)
{
    int index;
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if ((bdcmp(app_avk_cb.connections[index].bda_connected, bd_addr) == 0) &&
            (app_avk_cb.connections[index].in_use == TRUE))
            return &app_avk_cb.connections[index];
    }
    return NULL;
}

/*******************************************************************************
 **
** Function         app_avk_find_streaming_connection
**
** Description      This function finds the connection structure that is streaming
**
** Returns          Pointer to the found structure or NULL
**
*******************************************************************************/
tAPP_AVK_CONNECTION *app_avk_find_streaming_connection()
{
    int index;

    if (pStreamingConn && pStreamingConn->is_streaming_open && pStreamingConn->is_started)
        return pStreamingConn;

    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if (app_avk_cb.connections[index].is_streaming_open == TRUE &&
            app_avk_cb.connections[index].is_started == TRUE)
            return &app_avk_cb.connections[index];
    }
    return NULL;
}

/*******************************************************************************
 **
** Function         app_avk_num_connections
**
** Description      This function number of connections
**
** Returns          number of connections
**
*******************************************************************************/
int app_avk_num_connections()
{
    int iCount = 0;
    int index;
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if (app_avk_cb.connections[index].in_use == TRUE)
            iCount++;
    }
    return iCount;
}

/*******************************************************************************
 **
** Function         app_avk_find_connection_by_index
**
** Description      This function finds the connection structure by its index
**
** Returns          Pointer to the found structure or NULL
**
*******************************************************************************/
tAPP_AVK_CONNECTION *app_avk_find_connection_by_index(int index)
{
    if (index >= 0 && index < APP_AVK_MAX_CONNECTIONS)
        return &app_avk_cb.connections[index];

    return NULL;
}

/*******************************************************************************
 **
** Function         app_avk_pause_other_streams
**
** Description      This function pauses other streams when a new stream start
**                  is received from device identified by bd_addr
**
** Returns          None
**
*******************************************************************************/
void app_avk_pause_other_streams(BD_ADDR bd_addr)
{
    int index;
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        if ((bdcmp(app_avk_cb.connections[index].bda_connected, bd_addr)) &&
            app_avk_cb.connections[index].in_use &&
            app_avk_cb.connections[index].is_streaming_open) {
            app_avk_cb.connections[index].is_started = FALSE;
        }
    }
}

/*******************************************************************************
 **
** Function         app_avk_send_delay_report
**
** Description      Sample code to send delay report
**
** Returns          void
**
*******************************************************************************/
void app_avk_send_delay_report(UINT16 delay)
{
    tBSA_AVK_DELAY_REPORT report;
    int status;

    APP_DEBUG1("send delay report:%d", delay);

    BSA_AvkDelayReportInit(&report);
    report.delay = delay;
    status = BSA_AvkDelayReport(&report);
    if (status != BSA_SUCCESS) {
        APP_ERROR1("Unable to Send delay report %d", status);
    }
}
int app_avk_get_index_by_ad_addr(BD_ADDR bd_addr)
{
    int connection_index = -1;
    int index;
    APP_DEBUG1("%02x:%02x:%02x:%02x:%02x:%02x \r\n", bd_addr[0], bd_addr[1], bd_addr[2], bd_addr[3],
               bd_addr[4], bd_addr[5]);
    for (index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        APP_DEBUG1("%02x:%02x:%02x:%02x:%02x:%02x \r\n",
                   app_avk_cb.connections[index].bda_connected[0],
                   app_avk_cb.connections[index].bda_connected[1],
                   app_avk_cb.connections[index].bda_connected[2],
                   app_avk_cb.connections[index].bda_connected[3],
                   app_avk_cb.connections[index].bda_connected[4],
                   app_avk_cb.connections[index].bda_connected[5]);
        if (bdcmp(app_avk_cb.connections[index].bda_connected, bd_addr) == 0) {
            connection_index = index;
            break;
        }
    }
    return connection_index;
}
int app_avk_get_playstatus(void)
{
	return app_avk_cb.play_state;
}

void app_avk_play_fade_operation(int op)
{
	printf("app_avk_play_fade_operation %d \n",op);
	
	pthread_mutex_lock(&operation_mutex);

    switch (op) {
    case FADE_OP_STOP:
#ifdef WMRMPLAY_ENABLE
        if(WMUtil_check_wmrm_used(&g_wmrm_used))
        {
        printf("app_avk_play_fade_operation FADE_OP_STOP\n");
        app_avk_close_alsa();
        }
        else
#endif
        {
        if (g_BT_fadeCache.cache_run) {
            fadeCache_eof(&g_BT_fadeCache);
            fadeCache_uninit(&g_BT_fadeCache, 0);
        }
        }
        break;
    case FADE_OP_PAUSE:
	 if(app_avk_cb.play_state != AVRC_PLAYSTATE_PAUSED)
	 {
		if(g_direction != DIRECTION_FADE_OUT) g_snd_vol_cur = g_mix_vol_max;
		g_direction = DIRECTION_FADE_OUT;
		// app_avk_cb.audio_play_flag = 0;
#ifdef WMRMPLAY_ENABLE
        if(WMUtil_check_wmrm_used(&g_wmrm_used))
        {
	        printf("app_avk_play_fade_operation FADE_OP_PAUSE\n");
	        pthread_mutex_lock(&alsa_mutex);
	        if (g_wmrm_sound)
	            wmrm_sound_control(g_wmrm_sound, MRM_SOUND_PLAYBACK_PAUSE);
		g_wmrm_playing  = 0;
	        pthread_mutex_unlock(&alsa_mutex);
        }
        else
#endif
        {
	    // if (g_BT_fadeCache.cache_run)
	    //        fadeCache_pause(&g_BT_fadeCache);
	    }
		app_avk_cb.play_state = AVRC_PLAYSTATE_PAUSED;
	 }
        break;
    case FADE_OP_RESUME:
	if(g_direction != DIRECTION_FADE_IN) 
	{
		g_direction = DIRECTION_FADE_IN;
		g_snd_vol_cur = g_mix_vol_min;
	}
	if(app_avk_cb.play_state != AVRC_PLAYSTATE_PLAYING)
	{
		g_direction = DIRECTION_FADE_IN;
		g_snd_vol_cur = g_mix_vol_min;
	       app_avk_cb.audio_play_flag = 1;
#ifdef WMRMPLAY_ENABLE
        if(WMUtil_check_wmrm_used(&g_wmrm_used))
        {
	        printf("app_avk_play_fade_operation FADE_OP_RESUME\n");
	        pthread_mutex_lock(&alsa_mutex);
		g_wmrm_playing = 1;
	        if (g_wmrm_sound) {
	            //if(wmrm_sound_get_delay(g_wmrm_sound) > 500)
	            	wmrm_sound_control(g_wmrm_sound, MRM_SOUND_PLAYBACK_FLUSH);
	            //else
	            //	wmrm_sound_control(g_wmrm_sound, MRM_SOUND_PLAYBACK_RESUME);
	        }
	        pthread_mutex_unlock(&alsa_mutex);
	    }
	    else
#endif
        {
	        if (g_BT_fadeCache.cache_run)
	            fadeCache_resume(&g_BT_fadeCache);
        }
		app_avk_cb.play_state = AVRC_PLAYSTATE_PLAYING;
	}
        break;
    case FADE_OP_RESET:
#ifdef WMRMPLAY_ENABLE
        if(WMUtil_check_wmrm_used(&g_wmrm_used))
        {
        printf("app_avk_play_fade_operation FADE_OP_RESET\n");
        pthread_mutex_lock(&alsa_mutex);
        if (g_wmrm_sound)
            wmrm_sound_control(g_wmrm_sound, MRM_SOUND_PLAYBACK_FLUSH);
        pthread_mutex_unlock(&alsa_mutex);
        }
        else
#endif
        {
		g_direction = DIRECTION_FADE_IN;
		g_snd_vol_cur = g_mix_vol_min;
//
      //  if (g_BT_fadeCache.cache_run)
        //    fadeCache_alsa_reset(&g_BT_fadeCache);
        }
        break;
    default:
        break;
    }
	
	pthread_mutex_unlock(&operation_mutex);

}
