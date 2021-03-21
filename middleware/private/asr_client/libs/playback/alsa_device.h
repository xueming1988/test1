/*************************************************************************
    > File Name: alsa_device.h
    > Author:
    > Mail:
    > Created Time: Tue 21 Nov 2017 02:35:55 AM EST
 ************************************************************************/

#ifndef __ALSA_DEVICE_H__
#define __ALSA_DEVICE_H__

#if defined(SOFT_EQ_DSPC)
#define DEFAULT_DEV ("postout_softvol")
#else
#define DEFAULT_DEV ("default")
#endif

typedef struct _alsa_device_s alsa_device_t;

#ifdef __cplusplus
extern "C" {
#endif

alsa_device_t *alsa_device_open(const char *device_name);

int alsa_device_init(alsa_device_t *self, int fmt, unsigned int *samplerate,
                     unsigned short channels);

int alsa_device_uninit(alsa_device_t *self);

int alsa_device_play_silence(alsa_device_t *self, int time_ms);

int alsa_device_play_pcm(alsa_device_t *self, unsigned short *output_ptr, int len);

#ifdef __cplusplus
}
#endif

#endif
