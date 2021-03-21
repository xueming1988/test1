/*************************************************************************
    > File Name: mp3_decode.h
    > Author:
    > Mail:
    > Created Time: Tue 21 Nov 2017 02:35:55 AM EST
 ************************************************************************/

#ifndef __MP3_DECODE_H__
#define __MP3_DECODE_H__

#include <stdio.h>
#include <string.h>

#define MP3_DEC_RET_OK (0)
#define MP3_DEC_RET_WARN (-1)
#define MP3_DEC_RET_NETSLOW (-2)
#define MP3_DEC_RET_EXIT (-3)

typedef struct _mp3_decode_s mp3_decode_t;

typedef struct mp3_player_ctx_s {
    int (*get_volume)(void *player_ctx);
    int (*device_init)(void *player_ctx, int fmt, unsigned int *samplerate,
                       unsigned short channels);
    int (*device_uninit)(void *player_ctx);
    int (*read_data)(void *player_ctx, char *dst, size_t len);
    int (*play)(void *player_ctx, unsigned short *pcm_buff, size_t len);
    void *player_ctx;
} mp3_player_ctx_t;

#ifdef __cplusplus
extern "C" {
#endif

mp3_decode_t *decode_init(mp3_player_ctx_t ctx);

int decode_uninit(mp3_decode_t *self);

int decode_start(mp3_decode_t *self, int *dec_size);

int decode_play(mp3_decode_t *self);

#ifdef __cplusplus
}
#endif

#endif
