#ifndef _XIAOWEI_PLAYER_H
#define _XIAOWEI_PLAYER_H

#include <stdio.h>
#include <string.h>

#if defined __cplusplus || defined __cplusplus__
extern "C" {
#endif
enum {
    e_asr_start,
    e_asr_stop,
    e_asr_finished,
};

enum {
    PLAY_STATE_IDLE,
    PLAY_STATE_PLAYING,
    PLAY_STATE_PAUSED,
    PLAY_STATE_BUFFER_UNDERRUN,
    PLAY_STATE_FINISHED,
    PLAY_STATE_STOPPED,
    PLAY_NONE,
};

int xiaowei_play_tts_url(char *tts_url, char *id);
int xiaowei_player_init(void);
int xiaowei_url_player_init(void);
int xiaowei_tts_player_init(void);
#ifdef VOIP_SUPPORT
int wechatcall_pcm_play_finish(void);
#endif

#if defined __cplusplus || defined __cplusplus__
}
#endif

#endif
