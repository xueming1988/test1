/*
 * ALSA 0.9.x-1.x audio output driver
 *
 * Copyright (C) 2004 Alex Beregszaszi
 *
 * modified for real ALSA 0.9.0 support by Zsolt Barat <joy@streamminister.de>
 * additional AC-3 passthrough support by Andy Lo A Foe <andy@alsaplayer.org>
 * 08/22/2002 iec958-init rewritten and merged with common init, zsolt
 * 04/13/2004 merged with ao_alsa1.x, fixes provided by Jindrich Makovicka
 * 04/25/2004 printfs converted to mp_msg, Zsolt.
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <alloca.h>
#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>

#include "config.h"
#include "subopt-helper.h"
#include "mixer.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "audio_out.h"
#include "audio_out_internal.h"
#include "libaf/af_format.h"

#include <pthread.h>
#include <sys/ioctl.h>      /* ? */
//#include <sys/param.h>        /* ? */
#include <sys/time.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "wm_util.h"
#ifndef OPENWRT_AVS
#include "wm_fadecache.h"
#endif

#ifdef WMRMPLAY_ENABLE
#include "wmrm_api.h"
wmrm_snd_t g_wmrm_sound = 0;
wmrm_sound_param g_param = {0};
int g_wmrm_used = 0;
#endif

#define MV_AUDIO_UPDATE 1
#define AUDIO_SYNC_TH 0.025
#define THRE_COUNT_OVERRUN    10
#define THRE_COUNT_UNDERRUN   5

extern WIIMU_CONTEXT *g_wiimu_shm;
//wiimu >>>
#define MAX_SOFTVOL             0x10000
static int fadein_vol;
static int fadein_multiSample;
int fadein_startPoint = 0;
int fadein_time = 0;
int formatTransed = 0;
extern int g_stream_type;
extern void updateDmixConf(int rate, snd_pcm_format_t format);
//wiimu <<<

static const ao_info_t info = {
    "ALSA-0.9.x-1.x audio output",
    "alsa",
    "Alex Beregszaszi, Zsolt Barat <joy@streamminister.de>",
    "under development"
};

LIBAO_EXTERN(alsa)

static snd_pcm_t *alsa_handler = 0;
static snd_pcm_format_t alsa_format;
static snd_pcm_hw_params_t *alsa_hwparams;
static snd_pcm_sw_params_t *alsa_swparams;

static size_t bytes_per_sample = 4;

#ifndef OPENWRT_AVS
FadeCache g_fadeCache;
int g_fc_enable = 1;
int g_fc_nofade = 0;
#else
int g_fc_enable = 0;
int g_fc_nofade = 1;
int snd_pcm_info_set(snd_pcm_t *pcm, int info_index)
{
	return 0;
}
#endif

extern char *shmem_mplayer;
static int g_paused_by_asr = 0;

#ifdef NAVER_LINE
static volatile int need_focus = 0;
static void set_focus(void)
{
    int snd_type = -1;

    if (g_wiimu_shm && g_wiimu_shm->asr_ongoing == 0 && g_wiimu_shm->alert_status == 0) {
        if (need_focus == 0) {
            need_focus = 1;
            snd_type = focus_manage_stream_type_map(g_stream_type);
            if(snd_type == SNDSRC_ALERT) {
                focus_manage_set(g_wiimu_shm, e_focus_alert);
            } else if(snd_type == SNDSRC_ALEXA_TTS) {
                focus_manage_set(g_wiimu_shm, e_focus_dialogue);
            } else if(snd_type == SNDSRC_NOTIFY) {
                //focus_manage_set(g_wiimu_shm, e_focus_notification);
            } else {
                focus_manage_set(g_wiimu_shm, e_focus_content);
            }
            fprintf(stderr, "%d set_focus snd_type = %d asr_ongoing=%d alert_status = %d\n",
                    __LINE__, snd_type, g_wiimu_shm->asr_ongoing, g_wiimu_shm->alert_status);
        }
    } else {
        if (need_focus == 0) {
            snd_type = focus_manage_stream_type_map(g_stream_type);
            if(snd_type == SNDSRC_ALERT) {
                need_focus = 1;
                focus_manage_set(g_wiimu_shm, e_focus_alert);
            } else if(snd_type == SNDSRC_ALEXA_TTS) {
                need_focus = 1;
                focus_manage_set(g_wiimu_shm, e_focus_dialogue);
            } else if(snd_type == SNDSRC_NOTIFY) {
                need_focus = 1;
                //focus_manage_set(g_wiimu_shm, e_focus_notification);
            }
            if (need_focus == 1) {
                fprintf(stderr, "%d set_focus snd_type = %d asr_ongoing=%d alert_status = %d\n",
                        __LINE__, snd_type, g_wiimu_shm->asr_ongoing, g_wiimu_shm->alert_status);
            }
        }
    }
}

static void resume_focus(void)
{
    int snd_type = -1;

    if (need_focus == 0) {
        need_focus = 1;
        focus_manage_set(g_wiimu_shm, e_focus_content);
        fprintf(stderr, "%d set_focus snd_type = %d\n", __LINE__, snd_type);
    }
}

static void clear_focus(void)
{
    fprintf(stderr, "clear_focus OK need_focus = %d\n", need_focus);
    need_focus = 0;
    int snd_type = focus_manage_stream_type_map(g_stream_type);
    if(snd_type == SNDSRC_ALERT) {
        focus_manage_clear(g_wiimu_shm, e_focus_alert);
    } else if(snd_type == SNDSRC_ALEXA_TTS) {
        focus_manage_clear(g_wiimu_shm, e_focus_dialogue);
    } else if(snd_type == SNDSRC_NOTIFY) {
        //focus_manage_clear(g_wiimu_shm, e_focus_notification);
    } else {
        focus_manage_clear(g_wiimu_shm, e_focus_content);
    }
}
#endif

//#define DUMP_PCM_FILE
#ifdef DUMP_PCM_FILE
#define PCM_FILE "/tmp/mnt/test.pcm"
int append_pcm_data_to_file(char *path, char *buf, int len)
{
    int ret = 0;
    FILE *fp = fopen(path, "a");
    if(fp == NULL) {
        fprintf(stderr, "%s open %s failed\n", __FUNCTION__, path);
        exit(-1);
    }

    ret = fwrite((void *)buf, len, 1, fp);
    if(ret != 1) {
        fprintf(stderr, "%s fwrite %s ret=%d\n", __FUNCTION__, path, ret);
        exit(-1);
    }
    fclose(fp);
    //usleep((len/4)*1000*10/441);
    return len / 4;
}
#endif

static void alsa_error_handler(const char *file, int line, const char *function,
                               int err, const char *format, ...)
{
    char tmp[0xc00];
    va_list va;

    va_start(va, format);
    vsnprintf(tmp, sizeof tmp, format, va);
    va_end(va);

    if(err) {
        fprintf(stderr, "[ALSA sync] -------------------------alsa error: %s:%i:(%s) %s: %s\n", file, line, function, tmp,
                snd_strerror(err));
    } else {
        fprintf(stderr, "[ALSA sync] alsa-lib: %s:%i:(%s) %s\n", file, line, function, tmp);
    }
}

/* to set/get/query special features/parameters */
static int control(int cmd, void *arg)
{
    switch(cmd) {
        case AOCONTROL_QUERY_FORMAT:
            return CONTROL_TRUE;
        case AOCONTROL_GET_VOLUME:
        case AOCONTROL_SET_VOLUME: {
            ao_control_vol_t *vol = (ao_control_vol_t *)arg;

            int err;
            snd_mixer_t *handle;
            snd_mixer_elem_t *elem;
            snd_mixer_selem_id_t *sid;

            char *mix_name = "PCM";
            char *card = "default";
            int mix_index = 0;

            long pmin, pmax;
            long get_vol, set_vol;
            float f_multi;

            if(AF_FORMAT_IS_AC3(ao_data.format) || AF_FORMAT_IS_IEC61937(ao_data.format))
                return CONTROL_TRUE;

            if(mixer_channel) {
                char *test_mix_index;

                mix_name = strdup(mixer_channel);
                if((test_mix_index = strchr(mix_name, ','))) {
                    *test_mix_index = 0;
                    test_mix_index++;
                    mix_index = strtol(test_mix_index, &test_mix_index, 0);

                    if(*test_mix_index) {
                        mix_index = 0 ;
                    }
                }
            }
            if(mixer_device) card = mixer_device;

            //allocate simple id
            snd_mixer_selem_id_alloca(&sid);

            //sets simple-mixer index and name
            snd_mixer_selem_id_set_index(sid, mix_index);
            snd_mixer_selem_id_set_name(sid, mix_name);

            if(mixer_channel) {
                free(mix_name);
                mix_name = NULL;
            }

            if((err = snd_mixer_open(&handle, 0)) < 0) {
                snd_mixer_selem_id_free(sid);
                return CONTROL_ERROR;
            }

            if((err = snd_mixer_attach(handle, card)) < 0) {
                snd_mixer_selem_id_free(sid);
                snd_mixer_close(handle);
                return CONTROL_ERROR;
            }

            if((err = snd_mixer_selem_register(handle, NULL, NULL)) < 0) {
                snd_mixer_selem_id_free(sid);
                snd_mixer_close(handle);
                return CONTROL_ERROR;
            }
            err = snd_mixer_load(handle);
            if(err < 0) {
                snd_mixer_selem_id_free(sid);
                snd_mixer_close(handle);
                return CONTROL_ERROR;
            }

            elem = snd_mixer_find_selem(handle, sid);
            if(!elem) {
                snd_mixer_selem_id_free(sid);
                snd_mixer_close(handle);
                return CONTROL_ERROR;
            }

            snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);
            f_multi = (100 / (float)(pmax - pmin));

            if(cmd == AOCONTROL_SET_VOLUME) {
                set_vol = vol->left / f_multi + pmin + 0.5;

                //setting channels
                if((err = snd_mixer_selem_set_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, set_vol)) < 0) {
                    snd_mixer_selem_id_free(sid);
                    snd_mixer_close(handle);
                    return CONTROL_ERROR;
                }

                set_vol = vol->right / f_multi + pmin + 0.5;

                if((err = snd_mixer_selem_set_playback_volume(elem, SND_MIXER_SCHN_FRONT_RIGHT, set_vol)) < 0) {
                    snd_mixer_selem_id_free(sid);
                    snd_mixer_close(handle);
                    return CONTROL_ERROR;
                }

                if(snd_mixer_selem_has_playback_switch(elem)) {
                    int lmute = (vol->left == 0.0);
                    int rmute = (vol->right == 0.0);
                    if(snd_mixer_selem_has_playback_switch_joined(elem)) {
                        lmute = rmute = lmute && rmute;
                    } else {
                        snd_mixer_selem_set_playback_switch(elem, SND_MIXER_SCHN_FRONT_RIGHT, !rmute);
                    }
                    snd_mixer_selem_set_playback_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, !lmute);
                }
            } else {
                snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &get_vol);
                vol->left = (get_vol - pmin) * f_multi;
                snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_FRONT_RIGHT, &get_vol);
                vol->right = (get_vol - pmin) * f_multi;
            }
            snd_mixer_selem_id_free(sid);
            snd_mixer_close(handle);
            return CONTROL_OK;
        }

    } //end switch
    return CONTROL_UNKNOWN;
}

/*
    open & setup audio device
    return: 1=success 0=fail
*/
static int init(int rate_hz, int channels, int format, int flags)
{
#ifdef WMRMPLAY_ENABLE
    WMUtil_decide_wmrm_use(&g_wmrm_used, g_wiimu_shm, rate_hz);
    printf("%d %s:rate:%d,g_wmrm_used:%d\n",__LINE__,__FUNCTION__,rate_hz,g_wmrm_used);
    if(WMUtil_check_wmrm_used(&g_wmrm_used))
    {
    ao_data.samplerate = rate_hz;
    ao_data.format = format;
    ao_data.channels = channels;
    ao_data.bps = ao_data.samplerate * bytes_per_sample;
    ao_data.buffersize = 2048 * bytes_per_sample;
    ao_data.outburst = 1024 * bytes_per_sample;
	g_param.block = 1;
	g_param.format =SND_PCM_FORMAT_S16_LE; 
	g_param.stream_id = stream_id_linkplay;
	g_param.sample_rate = rate_hz;
	g_param.sample_channel = channels;
	g_param.play_leadtime = g_wiimu_shm->slave_num?800:200; //less time, the slave may lag when the network is not good enough
	g_param.ctrl_leadtime = 50;
	g_wmrm_sound = wmrm_sound_init(&g_param);
#ifdef  AUDIO_SRC_OUTPUT
        updateDmixConf(rate_hz, SND_PCM_FORMAT_S16_LE);
#endif
    }
    else
#endif
    {
    int rc, dir = 0;
    snd_pcm_uframes_t bufsize;
    int sampling_rate = rate_hz;
    snd_pcm_uframes_t frames = 32;
    snd_lib_error_set_handler(alsa_error_handler);
    char deviceName[20] = {0};
    //static snd_output_t *logger;
    //rc = snd_output_stdio_attach(&logger, stderr, 0);

    ao_data.samplerate = rate_hz;
    ao_data.format = format;
    ao_data.channels = channels;
    ao_data.bps = ao_data.samplerate * bytes_per_sample;

#ifdef  AUDIO_SRC_OUTPUT
    bytes_per_sample = af_fmt2bits(ao_data.format) / 8;
    bytes_per_sample *= ao_data.channels;
    formatTransed = 0;
    //sprintf(deviceName, "hw");
    sprintf(deviceName, "default");
    switch(format) {
        case AF_FORMAT_S8:
            alsa_format = SND_PCM_FORMAT_S8;
            break;
        case AF_FORMAT_U8:
            alsa_format = SND_PCM_FORMAT_U8;
            break;
        case AF_FORMAT_U16_LE:
            alsa_format = SND_PCM_FORMAT_U16_LE;
            break;
        case AF_FORMAT_U16_BE:
            alsa_format = SND_PCM_FORMAT_U16_BE;
            break;
        case AF_FORMAT_AC3_LE:
        case AF_FORMAT_S16_LE:
        case AF_FORMAT_IEC61937_LE:
            alsa_format = SND_PCM_FORMAT_S16_LE;
            break;
        case AF_FORMAT_AC3_BE:
        case AF_FORMAT_S16_BE:
        case AF_FORMAT_IEC61937_BE:
            alsa_format = SND_PCM_FORMAT_S16_BE;
            break;
        case AF_FORMAT_U32_LE:
            alsa_format = SND_PCM_FORMAT_U32_LE;
            break;
        case AF_FORMAT_U32_BE:
            alsa_format = SND_PCM_FORMAT_U32_BE;
            break;
        case AF_FORMAT_S32_LE:
            fprintf(stderr, "AF_FORMAT_S32_LE transfer to SND_PCM_FORMAT_S24_LE\n");
            formatTransed = 2;
            alsa_format = SND_PCM_FORMAT_S24_LE;
            break;
        case AF_FORMAT_S32_BE:
            alsa_format = SND_PCM_FORMAT_S32_BE;
            break;
        case AF_FORMAT_U24_LE:
            alsa_format = SND_PCM_FORMAT_U24_3LE;
            break;
        case AF_FORMAT_U24_BE:
            alsa_format = SND_PCM_FORMAT_U24_3BE;
            break;
        case AF_FORMAT_S24_LE:
            alsa_format = SND_PCM_FORMAT_S24_3LE;
            break;
        case AF_FORMAT_S24_BE:
            alsa_format = SND_PCM_FORMAT_S24_3BE;
            break;
        case AF_FORMAT_FLOAT_LE:
            alsa_format = SND_PCM_FORMAT_FLOAT_LE;
            break;
        case AF_FORMAT_FLOAT_BE:
            alsa_format = SND_PCM_FORMAT_FLOAT_BE;
            break;
        case AF_FORMAT_MU_LAW:
            alsa_format = SND_PCM_FORMAT_MU_LAW;
            break;
        case AF_FORMAT_A_LAW:
            alsa_format = SND_PCM_FORMAT_A_LAW;
            break;

        default:
            alsa_format = SND_PCM_FORMAT_MPEG; //? default should be -1
            break;
    }
    updateDmixConf(ao_data.samplerate, alsa_format);
#else
#if defined(OPENWRT_AVS) && defined(PLATFORM_QUALCOMM)
    sprintf(deviceName, "plug:dmix");
#else
    sprintf(deviceName, "default");
#endif
    alsa_format = SND_PCM_FORMAT_S16;
#endif

    rc = snd_pcm_open(&alsa_handler, deviceName, SND_PCM_STREAM_PLAYBACK, 0);
    if(rc < 0) {
        fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
        alsa_handler = 0;
        return 0;
    }

#ifdef NAVER_LINE
    int snd_type = focus_manage_stream_type_map(g_stream_type);
    if(snd_type > 0) {
        snd_pcm_info_set(alsa_handler, snd_type);
    } else {
        snd_pcm_info_set(alsa_handler, SNDSRC_MPLAYER);
    }
#else
#if !defined(SAMPLING_RATE_176400)      //this is 32bit out, do not use alsa fade feature
    snd_pcm_info_set(alsa_handler, SNDSRC_MPLAYER);
#endif
#endif

#ifndef OPENWRT_AVS
    snd_pcm_fade_vol(alsa_handler, 0); //set maplay fade in
#endif
    snd_pcm_nonblock(alsa_handler, 0);

    snd_pcm_hw_params_alloca(&alsa_hwparams);
    snd_pcm_hw_params_any(alsa_handler, alsa_hwparams);
    snd_pcm_hw_params_set_access(alsa_handler, alsa_hwparams, SND_PCM_ACCESS_RW_INTERLEAVED);
    rc = snd_pcm_hw_params_set_format(alsa_handler, alsa_hwparams, alsa_format);
    if(rc < 0) {
        fprintf(stderr, "Sample format non available");
        return 0;
    }
    snd_pcm_hw_params_set_channels(alsa_handler, alsa_hwparams, channels);
    snd_pcm_hw_params_set_rate_near(alsa_handler, alsa_hwparams, (unsigned int *)&sampling_rate, &dir);
    //snd_pcm_hw_params_set_period_size_near(alsa_handler, alsa_hwparams, &frames, &dir);
#ifdef  AUDIO_SRC_OUTPUT
    {
        unsigned buffer_time = 0, buffer_frames = 0;
        unsigned period_time = 0, period_frames = 0;
        if(buffer_time == 0 && buffer_frames == 0) {
            rc = snd_pcm_hw_params_get_buffer_time_max(alsa_hwparams,
                    &buffer_time, 0);
        }
        if(period_time == 0 && period_frames == 0) {
            if(buffer_time > 0)
                period_time = buffer_time / 4;
            else
                period_frames = buffer_frames / 4;
        }

        if(period_time > 0) {
            rc = snd_pcm_hw_params_set_period_time_near(alsa_handler, alsa_hwparams,
                    &period_time, 0);
        } else {
            rc = snd_pcm_hw_params_set_period_size_near(alsa_handler, alsa_hwparams,
                    &period_frames, 0);
        }
        if(buffer_time > 0) {
            rc = snd_pcm_hw_params_set_buffer_time_near(alsa_handler, alsa_hwparams,
                    &buffer_time, 0);
        } else {
            rc = snd_pcm_hw_params_set_buffer_size_near(alsa_handler, alsa_hwparams,
                    &buffer_frames);
        }
    }
#endif
    rc = snd_pcm_hw_params(alsa_handler, alsa_hwparams);
    if(rc < 0) {
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
        snd_pcm_close(alsa_handler);
        alsa_hwparams = 0;
        alsa_handler = 0;
        return 0;
    }

    snd_pcm_hw_params_get_buffer_size(alsa_hwparams, &bufsize);
    ao_data.buffersize = bufsize * bytes_per_sample;

    snd_pcm_hw_params_get_period_size(alsa_hwparams, &frames, NULL);
    ao_data.outburst = frames * bytes_per_sample;
    printf("ao_data.outburst = %d\n", ao_data.outburst);
    //snd_pcm_hw_params_dump(alsa_hwparams, logger);

    snd_pcm_sw_params_alloca(&alsa_swparams);
    snd_pcm_sw_params_current(alsa_handler, alsa_swparams);
#ifdef  AUDIO_SRC_OUTPUT
    snd_pcm_sw_params_set_start_threshold(alsa_handler, alsa_swparams, bufsize);
#else
    //snd_pcm_sw_params_set_start_threshold(alsa_handler, alsa_swparams, 32);
#endif
    //alsa_can_pause = snd_pcm_hw_params_can_pause(alsa_hwparams);

    wiimu_log(1, 0, 0, 0, "[TICK][ALSA init] channel=%d, rate=%d, burst=%d, bufsize=%d at %lld",
              channels, sampling_rate, frames, bufsize, tickSincePowerOn());

	if(g_fc_enable) {
#ifndef OPENWRT_AVS
#ifdef NAVER_LINE
        if(snd_type > 0) {
            fadeCache_init(&g_fadeCache, alsa_handler, snd_type, g_wiimu_shm, ao_data.samplerate, alsa_format, ao_data.outburst,
                           bytes_per_sample);
        } else {
            fadeCache_init(&g_fadeCache, alsa_handler, SNDSRC_MPLAYER, g_wiimu_shm, ao_data.samplerate, alsa_format,
                           ao_data.outburst, bytes_per_sample);
        }
#else
        fadeCache_init(&g_fadeCache, alsa_handler, SNDSRC_MPLAYER, g_wiimu_shm, ao_data.samplerate, alsa_format,
                       ao_data.outburst, bytes_per_sample);
#endif
    }
    else
    {
#ifndef NAVER_LINE
		WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 0, SNDSRC_MPLAYER, 0);
#endif
    }
#endif

    if(shmem_mplayer) {
        share_context *pshare = (share_context *)shmem_mplayer;
        int fadein_deltaVol = (MAX_SOFTVOL - fadein_startPoint) ;
        long long fadein_deltaCount = ((long long)fadein_time * ao_data.samplerate * ao_data.channels) / 1000;

        if(!(fadein_deltaVol && fadein_deltaCount)) {
            pshare->fadein_lefttime = 0;
            pshare->fixed_volume = MAX_SOFTVOL;
            fadein_vol = 0;
            fadein_multiSample = 0;
        } else if(fadein_deltaVol > fadein_deltaCount) {
            fadein_vol = fadein_deltaVol / fadein_deltaCount;
            fadein_multiSample = 1;
            pshare->fadein_lefttime = fadein_time;
            pshare->fixed_volume = fadein_startPoint;
        } else {
            fadein_multiSample = fadein_deltaCount / fadein_deltaVol;
            fadein_vol = 1;
            pshare->fadein_lefttime = fadein_time;
            pshare->fixed_volume = fadein_startPoint;
        }
    }
    //snd_pcm_dump(alsa_handler, logger);

#ifdef NAVER_LINE
    if(!g_fc_enable) {
        usleep(100000);
        fprintf(stderr, "[focus_manage] imuzop start play without fade\n");
        if(snd_type > 0) {
        	if (snd_type != SNDSRC_NOTIFY) {
        		WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 0, snd_type, 1);
        	}
        } else {
            WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 0, SNDSRC_MPLAYER, 1);
        }
        usleep(150000);         //sleep to make sure PA turn on
    }
    set_focus();
#else
    if(!g_fc_enable) {
        usleep(100000);
        WMUtil_Snd_Ctrl_SetProcessSelfVol(g_wiimu_shm, 100, SNDSRC_MPLAYER);
        usleep(150000);
		//sleep to make sure PA turn on
    }
#endif
    }
    g_paused_by_asr = 0;
    return 1;
}


/* close audio device */
static void uninit(int immed)
{
    wiimu_log(1, 0, 0, 0, "[ALSA sync] alsa uninit++, immed=%d at %d", immed, (int)tickSincePowerOn());
#ifdef WMRMPLAY_ENABLE
    if(WMUtil_check_wmrm_used(&g_wmrm_used))
    {
	if(g_wmrm_sound) {
		wmrm_sound_deinit(g_wmrm_sound);
		g_wmrm_sound = 0;
	}
    }
    else
#endif	
    {

	if(g_fc_enable) {
#ifndef OPENWRT_AVS
        fadeCache_uninit(&g_fadeCache, immed);
#endif
    }

    if(alsa_handler) {
        //int delay = 0;
        //snd_pcm_delay(alsa_handler, &delay);
        //fprintf(stderr, "[ALSA sync] alsa uninit++, delay is %d at %d\n", delay, (int)tickSincePowerOn());
        //usleep(200*1000);
        if(!immed) { //!immed)
            fprintf(stderr, "[ALSA sync] alsa uninit++, drain \n");
            snd_pcm_drain(alsa_handler);
        } else {
            fprintf(stderr, "[ALSA sync] alsa uninit++, drop \n");
            usleep(50000);
            snd_pcm_drop(alsa_handler);
            snd_pcm_prepare(alsa_handler);
        }

#ifdef NAVER_LINE
        clear_focus();
#else
        WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 1, SNDSRC_MPLAYER, 0);
#endif
        usleep(50000);
        snd_pcm_close(alsa_handler);
    }
    alsa_handler = NULL;
    }
    wiimu_log(1, 0, 0, 0, "[ALSA sync] alsa uninit----");
}

static void audio_pause(void)
{
    wiimu_log(1, 0, 0, 0, "[ALSA sync] alsa pause ++");
#ifdef WMRMPLAY_ENABLE
    if(WMUtil_check_wmrm_used(&g_wmrm_used))
    {
	if(g_wmrm_sound)
		wmrm_sound_control(g_wmrm_sound, MRM_SOUND_PLAYBACK_PAUSE);
    }
    else
#endif	
    {
    if(g_fc_enable) {
#ifdef NAVER_CLOVA
        snd_pcm_drop(alsa_handler);
#endif
#ifndef OPENWRT_AVS
        wiimu_log(1, 0, 0, 0, "[ALSA sync] fade pasue at %lld\n", tickSincePowerOn());
        fadeCache_pause(&g_fadeCache);
#endif
    } else {
        wiimu_log(1, 0, 0, 0, "[ALSA sync] default pasue at %lld\n", tickSincePowerOn());
        snd_pcm_drop(alsa_handler);
        WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 1, SNDSRC_MPLAYER, 0);
    }
#ifdef NAVER_LINE
    clear_focus();
#endif
    }

    wiimu_log(1, 0, 0, 0, "[ALSA sync] alsa pause at %d--", (int)master_get_ntp_time());
}

static void audio_resume(void)
{
    wiimu_log(1, 0, 0, 0, "[ALSA sync] alsa resume ++");
#ifdef WMRMPLAY_ENABLE
    if(WMUtil_check_wmrm_used(&g_wmrm_used))
    {
	if(g_wmrm_sound)
		wmrm_sound_control(g_wmrm_sound, MRM_SOUND_PLAYBACK_RESUME);
    }
    else
#endif	
    {
    //WMUtil_Snd_Ctrl_SetProcessVol(g_wiimu_shm, 100, SNDSRC_MPLAYER);
	if(g_fc_enable) {
#ifndef OPENWRT_AVS
        fadeCache_resume(&g_fadeCache);
#endif
    } else {
        snd_pcm_prepare(alsa_handler);
        WMUtil_Snd_Ctrl_SetProcessMute(g_wiimu_shm, 0, SNDSRC_MPLAYER, 0);
    }
#ifdef NAVER_LINE
    resume_focus();
#endif
    }
    g_paused_by_asr = 0;
    wiimu_log(1, 0, 0, 0, "[ALSA sync] alsa resume at %d --", (int)master_get_ntp_time());
}

/* stop playing and empty buffers (for seeking/pause) */
static void reset(void)
{
    wiimu_log(1, 0, 0, 0, "[ALSA sync] alsa reset ++");

#ifdef WMRMPLAY_ENABLE
    if(WMUtil_check_wmrm_used(&g_wmrm_used))
    {
		if(g_wmrm_sound)
			wmrm_sound_control(g_wmrm_sound, MRM_SOUND_PLAYBACK_FLUSH);
    }
    else
#endif	
    {

	if(g_fc_enable) {
#ifndef OPENWRT_AVS
        fadeCache_alsa_reset(&g_fadeCache);
#endif
    } else {
        snd_pcm_drop(alsa_handler);
        snd_pcm_prepare(alsa_handler);
    }
    }
    g_paused_by_asr = 0;
    wiimu_log(1, 0, 0, 0, "[ALSA sync] alsa reset at %d--", (int)master_get_ntp_time());
}

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

    out = (long)sample * fix_volume;
    if(fix_volume < MAX_SOFTVOL) {
        rand_b = rand_a;
        rand_a = lcg_rand();
        out += rand_a;
        out -= rand_b;
    }

    return out >> 16;
}

static int fadein(void *data, int len)
{
    unsigned int num_frames = len / bytes_per_sample;
    unsigned int tsize = num_frames * ao_data.channels;
    int leftTime;
    short *dst = data;
    static int multiCount = 0;

    if(shmem_mplayer) {
        share_context *pshare = (share_context *)shmem_mplayer;

        if(pshare->fixed_volume < MAX_SOFTVOL) {
            for(;;) {
                if(fadein_multiSample) {
                    multiCount ++;
                    if(multiCount >= fadein_multiSample) {
                        multiCount = 0;
                        pshare->fixed_volume += fadein_vol;
                    }
                } else {
                    pshare->fixed_volume += fadein_vol;
                }

                if(pshare->fixed_volume > MAX_SOFTVOL)
                    pshare->fixed_volume = MAX_SOFTVOL;

                if(!*dst) {
                    ;
                } else {
                    *dst = dithered_vol(*dst, pshare->fixed_volume);
                }
                if(!--tsize)
                    break;
                dst++;
            }
        }

        leftTime = pshare->fadein_lefttime - num_frames * 1000 / ao_data.samplerate;

        pshare->fadein_lefttime = leftTime > 0 ? leftTime : 0;
    }
}

static inline void store24bit(void *data, int pos, unsigned int expanded_value)
{
#if HAVE_BIGENDIAN
    ((unsigned char *)data)[3 * pos] = expanded_value >> 24;
    ((unsigned char *)data)[3 * pos + 1] = expanded_value >> 16;
    ((unsigned char *)data)[3 * pos + 2] = expanded_value >> 8;
#else
    ((unsigned char *)data)[3 * pos] = expanded_value >> 8;
    ((unsigned char *)data)[3 * pos + 1] = expanded_value >> 16;
    ((unsigned char *)data)[3 * pos + 2] = expanded_value >> 24;
#endif
}

/*
    plays 'len' bytes of 'data'
    returns: number of bytes played
    modified last at 29.06.02 by jp
    thanxs for marius <marius@rospot.com> for giving us the light ;)
*/

static int play(void *data, int len, int flags)
{
#ifdef WMRMPLAY_ENABLE
    if(WMUtil_check_wmrm_used(&g_wmrm_used))
    {
	int res = -1;
	if(g_wmrm_sound) {
		res = wmrm_sound_write(g_wmrm_sound, data, len);
		if(res > 0)
			res /= 4;
		else usleep(10000);	
	}
        static long long last_output_ts = 0;
	static int writed = 0;
	long long cur_output_ts = tickSincePowerOn();
	writed += res;
	if(writed >= 4800) {
		//if(last_output_ts + 50 >  cur_output_ts)		
		//	usleep(20000);	
		last_output_ts = cur_output_ts;
		writed = 0;
	}
        return res < 0 ? res : res * bytes_per_sample;
    }
    else
#endif
    {
    int num_frames, i;
    snd_pcm_sframes_t result = 0;
    snd_pcm_sframes_t res = 0;
    char *pdata = (char *)data;
    int byteSample  = bytes_per_sample;

    if(!(flags & AOPLAY_FINAL_CHUNK) && (ao_data.outburst != 0))
        len = len / ao_data.outburst * ao_data.outburst;
    num_frames = len / bytes_per_sample;
    if(!alsa_handler) {
        return 0;
    }
    if(num_frames == 0)
        return 0;

    fadein(data, len);

#ifdef NAVER_LINE
    set_focus();
#endif

	if(formatTransed == 1) {
        int i;
        for(i = 0; i < num_frames * ao_data.channels; i++) {
            store24bit(data, i, ((unsigned int *)data)[i]);
        }
        byteSample = 3 * ao_data.channels;
    } else if(formatTransed == 2) {
        int i;
        for(i = 0; i < num_frames * ao_data.channels; i++) {
            ((unsigned int *)data)[i] = ((unsigned int *)data)[i] >> 8;
        }
    }

#ifndef OPENWRT_AVS
    if(g_fc_enable && g_fadeCache.cache_run) {
        result = fadeCache_play(&g_fadeCache, byteSample, num_frames, pdata);
   }
#endif
    else {
        do {
#ifndef NAVER_LINE
            if(g_wiimu_shm->asr_pause_request) {
                if(g_paused_by_asr == 0) {
                    wiimu_log(1, 0, 0, 0, "mplayer paused by ASR");
                    snd_pcm_drop(alsa_handler);
                }
                g_paused_by_asr = 1;
                usleep(50000);
                return 0;
            } else {
                if(g_paused_by_asr) {
                    wiimu_log(1, 0, 0, 0, "mplayer resume from ASR");
                    snd_pcm_prepare(alsa_handler);
                }
                g_paused_by_asr = 0;
            }
#endif
            res = snd_pcm_writei(alsa_handler, data, num_frames);
            if(res < 0) {
                //fprintf(stderr, "ALSA write data failed: %d\n", (int)res);
                res = snd_pcm_recover(alsa_handler, res, 0);
                res = 0;
            }
            if(res == -EINTR) {
                /* nothing to do */
                res = 0;
            } else if(res == -ESTRPIPE) {
                /* suspend */
            }

#ifdef MV_AUDIO_UPDATE
            if(res > 0) {
                result += res;
                num_frames -= res;
                data = (void *)((char *)data + ((int)res * byteSample));
                continue;
            }
            usleep(10023);
#endif
        } while(num_frames > 0);
    }

    res = result;
    static long long last_output_ts = 0;
    long long cur_output_ts = tickSincePowerOn();

#ifndef OPENWRT_AVS
    if(g_fc_enable && g_fadeCache.cache_run) {
        last_output_ts = fadeCache_play_sleep(&g_fadeCache, cur_output_ts, last_output_ts);
    }
#endif
    else {
        if(last_output_ts + 10 >  cur_output_ts)
            usleep(30050);
        else if(last_output_ts + 30 >  cur_output_ts)
            usleep(15050);
        else if(last_output_ts + 45 >  cur_output_ts)
            usleep(5050);
        last_output_ts = cur_output_ts;
    }
    return res < 0 ? res : res * bytes_per_sample;
    }
}

/* how many byes are free in the buffer */
static int get_space(void)
{
    int ret;
 #if defined(MV_AUDIO_UPDATE)
    ret = 32768 ;//16384;
#else  
	int byteSample = bytes_per_sample;
    if(formatTransed == 1) {
        byteSample = 3 * ao_data.channels;
    }

    snd_pcm_status_t *status = 0;
    snd_pcm_status_malloc(&status);
    if((ret = snd_pcm_status(alsa_handler, status)) < 0) {
        return 0;
    }

    ret = snd_pcm_status_get_avail(status) * byteSample;
    if(ret > ao_data.buffersize)   // Buffer underrun?
        ret = ao_data.buffersize;

    snd_pcm_status_free(status);
//  fprintf(stderr, "mplayer alsa  get_space %d \n",ret);
#endif
    return ret;
}

/* delay in seconds between first and last sample in buffer */
static float get_delay(void)
{
#ifdef WMRMPLAY_ENABLE
    if(WMUtil_check_wmrm_used(&g_wmrm_used))
    {
	float delay = 0;
	int res = 0;
	if(g_wmrm_sound) {
		res = wmrm_sound_get_delay(g_wmrm_sound);
		delay = (float)res / 1000.0;

	 	wiimu_log(1, LOG_TYPE_TS, 9, 2, "mplayer delay: %.2f \n", delay);
		if(delay >= 0.1)
			delay -= 0.1;
		else
			delay = 0;
	}
	return (float)delay;
    }
    else
#endif	
    {
    if(alsa_handler) {
        snd_pcm_sframes_t delay;
        float cache = 0;

        if(g_fc_enable) {
#ifndef OPENWRT_AVS
            cache = fadeCache_get_delay(&g_fadeCache, ao_data.samplerate);
#endif
        }

        if(snd_pcm_delay(alsa_handler, &delay) < 0) {
            return cache;
        }

        if(delay < 0) {
            /* underrun - move the application pointer forward to catch up */
#if SND_LIB_VERSION >= 0x000901 /* snd_pcm_forward() exists since 0.9.0rc8 */
            snd_pcm_forward(alsa_handler, -delay);
#endif
            delay = 0;
        }

        // fprintf(stderr, "ALSA get_delay: delay:%d, cache:%.3f,ret= %f\n", delay, cache, (float)delay / (float)ao_data.samplerate + cache);
        return (float)delay / (float)ao_data.samplerate + cache;
    } else {
        return 0;
    }
    }
}
