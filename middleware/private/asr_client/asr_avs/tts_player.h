/**
 *
 *
 *
 *
 *
 */

#ifndef __TTS_PLAYER_H__
#define __TTS_PLAYER_H__

#include <stdio.h>
#include <string.h>

typedef struct _tts_player_s tts_player_t;

enum {
    e_mp3_tts,
    e_mp3_cid,
};

enum {
    e_ctrl_stop,
    e_ctrl_finished,
    e_ctrl_continue,
    e_ctrl_pause,
};

enum {
    e_status_idle,
    e_status_pause,
    e_status_stopped,
    e_status_playing,
    e_status_interrupt,
};

#ifdef __cplusplus
extern "C" {
#endif

tts_player_t *ttsplayer_create(void);

void ttsplayer_destroy(tts_player_t **self_p);

/*
 * type: e_mp3_tts/e_mp3_cid
 */
int ttsplayer_start(tts_player_t *self, const char *token, int type, int is_next);

/*
 * e_ctrl_stop: stop current tts
 * e_ctrl_finished: tts cache finished
 * e_ctrl_continue: need talk continue and cache finished
 * e_ctrl_pause: pause currtent play
 */
void ttsplayer_stop(tts_player_t *self, int ctrl_cmd);

int ttsplayer_write_data(tts_player_t *self, char *data, int len);

/*
 * type: e_mp3_tts/e_mp3_cid
 *
 */
// TODO: the token need to free
int ttsplayer_get_state(tts_player_t *self, char **token, int type);

/*
 * stop current cid play
 */
int ttsplayer_clear_cid(tts_player_t *self, int flags);

/*
 * set the cid can play
 */
int ttsplayer_can_play_cid(tts_player_t *self);

/*
 * get the asr need continue or not
 *
 */
int ttsplayer_get_continue(tts_player_t *self);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_PROCESSOR_H */
