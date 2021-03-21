#include <stddef.h>
#include <stdint.h>
#include <opus/opus.h>

#include "wm_util.h"
#include "opus_encoder.h"

/// Audio sample rate: 16kHz
#define SAMPLE_RATE 16000

/// OPUS bitrate: 32kbps CBR
#define BIT_RATE 32000

/// OPUS frame length: 20ms
#define FRAME_LENGTH 20

/// PCM frame size
#define FRAME_SIZE ((SAMPLE_RATE / 1000) * FRAME_LENGTH)

/// Max frame size
#define MAX_FRAME_SIZE (FRAME_SIZE * 2)

static OpusEncoder *g_opus_enc = NULL;

static int opus_config(void)
{
    int err;

    /* use 32k bit rate */
    err = opus_encoder_ctl(g_opus_enc, OPUS_SET_BITRATE(BIT_RATE));
    if (err != OPUS_OK) {
        wiimu_log(1, 0, 0, 0, "Failed to set bitrate to 32kbps");
        return -1;
    }

    /* CBR Only */
    err = opus_encoder_ctl(g_opus_enc, OPUS_SET_VBR(0));
    if (err != OPUS_OK) {
        wiimu_log(1, 0, 0, 0, "Failed to set hard-CBR");
        return -1;
    }

    /* 20ms framesize */
    err = opus_encoder_ctl(g_opus_enc, OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_20_MS));
    if (err != OPUS_OK) {
        wiimu_log(1, 0, 0, 0, "Failed to set frame size to 20ms");
        return -1;
    }

    return 0;
}

int opus_start(void)
{
    int err;
    g_opus_enc = opus_encoder_create(SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP, &err);
    if (!g_opus_enc || err != OPUS_OK) {
        wiimu_log(1, 0, 0, 0, "%s: Failed to create OpusEncoder", __FUNCTION__);
        return -1;
    }
    if (opus_config() < 0) {
        opus_encoder_destroy(g_opus_enc);
        return -1;
    }
    return 0;
}

int opus_process_samples(char *samples, int size, int (*func)(char *, size_t))
{
    opus_int16 *pcm = (opus_int16 *)samples;
    int pcm_size = size / sizeof(opus_int16);
    opus_int16 *pcm_end = pcm + pcm_size;
    int encoded_bytes = 0;
    unsigned char encoded_packet[MAX_FRAME_SIZE];

    while (pcm + FRAME_SIZE <= pcm_end) {
        encoded_bytes = opus_encode(g_opus_enc, pcm, FRAME_SIZE, encoded_packet, MAX_FRAME_SIZE);
        if (encoded_bytes < 0) {
            wiimu_log(1, 0, 0, 0, "Failed to encode PCM audio");
            return -1;
        }
        func((char *)encoded_packet, encoded_bytes);
        pcm += FRAME_SIZE;
    }

    return 0;
}

void opus_close(void)
{
    if (g_opus_enc) {
        opus_encoder_destroy(g_opus_enc);
        g_opus_enc = NULL;
    }
}
