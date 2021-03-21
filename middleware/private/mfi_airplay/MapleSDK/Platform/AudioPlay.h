#ifdef LINKPLAY
#ifndef __AUDIO_PLAY_H__
#define __AUDIO_PLAY_H__
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <alsa/asoundlib.h>
#include "Platform_linkplay.h"

#define RESAMPLE_BEFORE_ALSA

typedef unsigned char uint8_t;

#define DEFAULT_SPEED   44100
#define DEFAULT_CHANNEL  2
#define DEFAULT_BITS   16
#define NUM_CHANNELS DEFAULT_CHANNEL

#ifdef RESAMPLE_BEFORE_ALSA

#include "samplerate.h"

struct rate_src {
    double ratio;
    int converter;
    unsigned int channels;
    float *src_buf;
    float *dst_buf;
    SRC_STATE *state;
    SRC_DATA data;
};

#define MAX_RESAMPLE_PERIOD_SIZE 8192

int init_resampler_before_alsa(int *p_samplerate);
int destory_resample(void);
int do_resample_before_alsa(uint8_t **InOutBuffer, long *InOutframes);
int get_extra_resample_delay(snd_pcm_sframes_t *InOutAvailable, snd_pcm_sframes_t *InOutDelay);

#endif


#define CONFIG_AUDIO_PERIOD_SIZE 2048
#define CONFIG_AUDIO_THRESHOLD_SIZE 4096
#define CONFIG_AUDIO_BUFFER_SIZE 8192

#define MAX_AUDIO_OUTPUT_BUFF_SIZE (8192)
#define SILENCE_AUDIO_WRITE_IN_MS 350

int audio_client_init(void);
int audio_client_deinit(void);
int audio_set_volume(double inVol);
int audio_client_get_alsa_handler(snd_pcm_t **pOutHandler);
void audio_client_process_fade_and_silence(uint8_t **InAudioBuffer, long inFrames, size_t bytesPerUnit, int32_t *fixedVolume);

#endif /* __AUDIO_PLAY_H__ */
#endif /* LINKPLAY */

