#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>

#include "common.h"
#include <pthread.h>
#include "wm_context.h"

#define NUM_CHANNELS 2

static pthread_mutex_t alsa_mutex = PTHREAD_MUTEX_INITIALIZER;

static snd_pcm_t *alsa_handle = NULL;
static snd_pcm_hw_params_t *alsa_params = NULL;
//extern FILE* debug;
//extern int debug;
//#define debug stderr
//#ifdef MULTIROOM_MODULE_ENABLE
#ifdef  AUDIO_SRC_OUTPUT
extern void updateDmixConfDefault(void);
#endif

struct _pcm_info {
	unsigned int device;		/* RO/WR (control): device number */
	unsigned int subdevice;		/* RO/WR (control): subdevice number */
	int stream;			/* RO/WR (control): stream direction */
	int card;			/* R: card number */
	unsigned char id[64];		/* ID (user selectable) */
	unsigned char name[80];		/* name of this device */
	unsigned char subname[32];	/* subdevice name */
	int dev_class;			/* SNDRV_PCM_CLASS_* */
	int dev_subclass;		/* SNDRV_PCM_SUBCLASS_* */
	unsigned int subdevices_count;
	unsigned int subdevices_avail;
	unsigned char sync[16];	/* hardware synchronization ID */
	unsigned char reserved[64];	/* reserved for future... */
};
long long GetDelay(void)
{
	struct _pcm_info* info;
	long long elapse = 0;
	unsigned long long reserve = 0;
	snd_pcm_sframes_t delay = 0;

    pthread_mutex_lock(&alsa_mutex);
	if(alsa_handle)
	{
		snd_pcm_delay(alsa_handle, &delay);

		if(0 == snd_pcm_info_malloc( (snd_pcm_info_t**)(void *)&info))
		{
			int err;
		   	if ((err = snd_pcm_info(alsa_handle, (snd_pcm_info_t*)info)) < 0)
			{
				debug(1,"[ALSA sync] snd_pcm_info error\n");
		   	}
		   	else
		   	{
				memcpy(&reserve, info->reserved, 8);
		   	}
		   	snd_pcm_info_free( (snd_pcm_info_t*)info);
		}
	}
    pthread_mutex_unlock(&alsa_mutex);
	//if(reserve)
	//{
		//reserve = (reserve * 44100) / 1000 * 4;
		//if( reserve > 32768) reserve = 0;
	//}
	if( reserve > 186) reserve = 0;
	else reserve = 186 - reserve;
	elapse = delay * 1000 / 44100 + (long long)reserve;
	return elapse;
}
//#endif

void audio_set_driver(char* driver) {
  //  debug(1, "audio_set_driver: not supported with alsa\n");
}

void audio_set_device_name(char* device_name) {
  //  debug(1, "audio_set_device_name: not supported with alsa\n");
}

void audio_set_device_id(char* device_id) {
  //  debug(1, "audio_set_device_id: not supported with alsa\n");
}

char* audio_get_driver(void)
{
    return NULL;
}

char* audio_get_device_name(void)
{
    return NULL;
}

char* audio_get_device_id(void)
{
    return NULL;
}
void audio_reset(void)
{
    pthread_mutex_lock(&alsa_mutex);
    if (alsa_handle)
    {
		snd_pcm_drop(alsa_handle);
		snd_pcm_prepare(alsa_handle);
    }
	pthread_mutex_unlock(&alsa_mutex);
}

void audio_play(char* outbuf, int samples, void* priv_data)
{
    pthread_mutex_lock(&alsa_mutex);
    if (alsa_handle)
    {
        int err = snd_pcm_writei(alsa_handle, outbuf, samples);
        if (err < 0)
            err = snd_pcm_recover(alsa_handle, err, 0);
    }
	pthread_mutex_unlock(&alsa_mutex);
}

void audio_flush(void)
{
    pthread_mutex_lock(&alsa_mutex);
    if (alsa_handle)
    {
		snd_pcm_drop(alsa_handle);
		snd_pcm_prepare(alsa_handle);
		snd_pcm_start(alsa_handle);
    }
	pthread_mutex_unlock(&alsa_mutex);

}

void* audio_init(int sampling_rate)
{
	int rc, dir = 0;
//    snd_pcm_uframes_t frames = 32;
    pthread_mutex_lock(&alsa_mutex);
#ifdef  AUDIO_SRC_OUTPUT
    updateDmixConfDefault();
#endif
	rc = snd_pcm_open(&alsa_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (rc < 0)
	{
        debug(1, "unable to open pcm device: %s\n", snd_strerror(rc));
        die("alsa initialization failed");
	}
    snd_pcm_info_set(alsa_handle, SNDSRC_AIRPLAY);
	snd_pcm_hw_params_alloca(&alsa_params);
    snd_pcm_hw_params_any(alsa_handle, alsa_params);
    snd_pcm_hw_params_set_access(alsa_handle, alsa_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(alsa_handle, alsa_params, SND_PCM_FORMAT_S16);
    snd_pcm_hw_params_set_channels(alsa_handle, alsa_params, NUM_CHANNELS);
    snd_pcm_hw_params_set_rate_near(alsa_handle, alsa_params, (unsigned int *)&sampling_rate, &dir);
    //snd_pcm_hw_params_set_period_size_near(alsa_handle, alsa_params, &frames, &dir);
	rc = snd_pcm_hw_params(alsa_handle, alsa_params);
	if (rc < 0)
	{
        debug(1, "unable to set hw parameters: %s\n", snd_strerror(rc));
        die("alsa initialization failed");
	}


    snd_pcm_sw_params_t *sw_params;
    //snd_pcm_hw_params_get_buffer_size (alsa_params, &frames);
	snd_pcm_sw_params_alloca (&sw_params);
	snd_pcm_sw_params_current (alsa_handle, sw_params);
    snd_pcm_sw_params_set_start_threshold(alsa_handle, sw_params, 32);
	snd_pcm_start(alsa_handle);
    pthread_mutex_unlock(&alsa_mutex);
    return NULL;
}

void audio_deinit(void)
{
    pthread_mutex_lock(&alsa_mutex);
    if (alsa_handle)
	{
        snd_pcm_drain(alsa_handle);
        snd_pcm_close(alsa_handle);
        alsa_handle = NULL;
        debug(1, "de-init alsa done!=%d\n", (int)alsa_handle);
    }
    pthread_mutex_unlock(&alsa_mutex);
}
