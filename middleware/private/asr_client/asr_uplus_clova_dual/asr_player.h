/*************************************************************************
    > File Name: asr_player.h
    > Author:
    > Mail:
    > Created Time: Tue 21 Nov 2017 02:35:55 AM EST
 ************************************************************************/

#ifndef _ASR_PLAYER_H
#define _ASR_PLAYER_H

#include <stdio.h>
#include <string.h>

typedef struct json_object json_object;

// { status type
enum {
    e_tts_player,
    e_asr_status,
};

enum {
    e_asr_start,
    e_asr_stop,
    e_asr_finished,
};

// }

#ifdef __cplusplus
extern "C" {
#endif

int asr_player_init(void);

int asr_player_uninit(void);

// parse directive
int asr_player_parse_directvie(json_object *js_data, int down_cmd);

/*
 * include SpeechState PlaybackState VolumeState
 */
// TODO: need free after used
char *asr_player_state(int request_type);

void asr_player_get_audio_info(char **params);

int asr_player_force_stop(void);

int asr_player_buffer_start_pcm(char *content_id);

// e_asr_stop: stop current tts
// e_asr_finished: tts cache finished
int asr_player_buffer_stop(char *content_id, int type);

int asr_player_buffer_write_data(char *content_id, char *data, int len);

int asr_player_get_client_type(void);

int asr_player_get_state(int type);

int asr_player_need_unmute(void);

int asr_player_get_volume(int get_mute);

int asr_player_set_volume(int set_mute, int val);

int asr_player_set_repeat_mode(char *mode);

int asr_player_set_pause(void);

int asr_player_set_resume(void);

int asr_player_set_replay(void);

int asr_player_set_stop(void);

int asr_player_set_next(void);

int asr_player_set_previous(void);

int asr_player_start_play_url(int type, const char *url, long long play_offset_ms);

void asr_player_stop_play_url(int type);

int asr_player_get_play_url_state(int type);

int lguplus_play_audio(char *play_behavior, char *report_interval, char *item_id, char *url);

int lguplus_audio_state(json_object *js_state);

#ifdef __cplusplus
}
#endif

#endif
