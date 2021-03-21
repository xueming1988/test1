#ifndef _CAPTURE_ALSA_H__
#define _CAPTURE_ALSA_H__

#include <alsa/asoundlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    snd_pcm_t *handle;
    snd_pcm_stream_t stream_type; // SND_PCM_STREAM_CAPTURE
    snd_pcm_format_t format;
    snd_pcm_access_t access;
    unsigned int channels;
    unsigned int rate;
    unsigned int latency_ms;
    unsigned int bits_per_sample;
    int block_mode;
    char *device_name;
} wm_alsa_context_t;

int capture_alsa_open(wm_alsa_context_t *alsa_context);
void capture_alsa_close(wm_alsa_context_t *alsa_context);
int capture_alsa_read(wm_alsa_context_t *alsa_context, void *buf, int len);
int capture_alsa_recovery(wm_alsa_context_t *alsa_context, int err, int timeout_ms);
void capture_alsa_dump_config(wm_alsa_context_t *alsa_context);

wm_alsa_context_t *alsa_context_create(char *config_file);
void alsa_context_destroy(wm_alsa_context_t *context);

#ifdef __cplusplus
}
#endif

#endif
