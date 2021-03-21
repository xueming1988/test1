/*************************************************************************
    > File Name: avs_player.h
    > Author:
    > Mail:
    > Created Time: Tue 21 Nov 2017 02:35:55 AM EST
 ************************************************************************/

#ifndef _AVS_PLAYER_H
#define _AVS_PLAYER_H

#include <stdio.h>
#include <string.h>

typedef struct json_object json_object;

// { status type
enum {
    e_url_player,
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

int avs_player_init(void);

int avs_player_uninit(void);

// parse directive
int avs_player_parse_directvie(json_object *js_data, int is_next);

/*
 * include SpeechState PlaybackState VolumeState
 */
// TODO: need free after used
char *avs_player_state(int request_type);

int avs_player_force_stop(int flags);

int avs_player_buffer_stop(char *contend_id, int type);

int avs_player_buffer_resume(void);

int avs_player_buffer_write_data(char *content_id, char *data, int len);

int avs_player_get_state(int type);

int avs_player_need_unmute(int mini_vol);

void avs_player_book_ctrol(int pause_flag);

int avs_player_audio_invalid(char *stream_id);

int avs_talkcontinue_get_status(void);

void avs_player_set_provider(char *provider);

void avs_player_ctrol(int pause_flag);

#ifdef __cplusplus
}
#endif

#endif
