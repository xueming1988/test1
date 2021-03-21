/*************************************************************************
    > File Name: url_player.h
    > Author:
    > Mail:
    > Created Time: Tue 21 Nov 2017 02:35:55 AM EST
 ************************************************************************/

#ifndef _URL_PLAYER_H
#define _URL_PLAYER_H

#include <stdio.h>
#include <string.h>
#if defined(UPLUS_AIIF)
#include "json.h"
#endif

typedef enum {
    e_notify_play_none = -1,
    e_notify_get_url,
    e_notify_play_started,
    e_notify_play_stopped,
    e_notify_play_paused,
    e_notify_play_resumed,
    e_notify_play_near_finished,
    e_notify_play_finished,
    e_notify_play_error,
    e_notify_play_delay_passed,
    e_notify_play_interval_passed,
    e_notify_play_position_passed,
    e_notify_play_end,
    e_notify_play_need_more,
    e_notify_play_need_history,

    e_notify_play_max,
} e_url_notify_t;

typedef enum {
    e_replace_none = -1,
    e_replace_all,
    e_replace_endqueue,
    e_endqueue,
    e_add_history,

    e_replace_max,
} e_behavior_t;

typedef enum {
    e_player_cmd_none = -1,
    e_player_cmd_stop,
    e_player_cmd_pause,
    e_player_cmd_resume,
    e_player_cmd_clear_queue,
    e_player_cmd_next,
    e_player_cmd_prev,
    e_player_cmd_replay,
    e_player_cmd_seekto,
    e_player_cmd_set_index,

    e_player_cmd_max,
} e_player_cmd_t;

/* the type of the value need set and get */
typedef enum {
    e_value_none = -1,
    e_value_mute,
    e_value_volume,
    e_value_repeat,

    e_value_max,
} e_value_t;

/* for e_value_repeat */
enum {
    e_repeat_none = -1,
    e_repeat_on,
    e_repeat_off,

    e_repeat_max,
};

/* for e_value_mute */
enum {
    e_mute_none = -1,
    e_mute_on,
    e_mute_off,

    e_mute_max,
};

/* for e_value_state  */
enum {
    e_idle,
    e_playing,
    e_paused,
    e_stopped,
};

enum {
    e_mode_playlist_sequential = 0,
    e_mode_playlist_loop,
    e_mode_playlist_random = 3,
    e_mode_single_loop,
};

/* for client_type */
enum {
    e_client_clova,
    e_client_lguplus,

    e_client_max,
};

typedef struct _url_stream_s {
    int ignore_ongoing_speak;
    int client_type;
    int new_dialog;
    e_behavior_t behavior;
    char *stream_id;
    char *stream_item;
    char *url;
    char *stream_data;         // only save the point
    long play_pos;             // start of the stream
    long play_report_delay;    // start of the stream
    long play_report_interval; // start of the stream
    long play_report_position; // start of the stream
    long long duration;        // duration of the stream
    long long cur_pos;         // first time is euqal with start of the stream
#if defined UPLUS_AIIF
    json_object *audio_stream;
    int url_getting;
#endif
} url_stream_t;

typedef struct _stream_state_s {
    int client_type;
    int state;
    int volume;
    int muted;
    int repeat;
    char *stream_id;
    char *stream_data;  // only save the point
    long long duration; // duration of the stream
    long long cur_pos;  // first time is euqal with start of the stream
} stream_state_t;

typedef struct _url_player_s url_player_t;
typedef int(url_notify_cb_t)(e_url_notify_t notify, url_stream_t *url_stream, void *ctx);

#ifdef __cplusplus
extern "C" {
#endif

url_player_t *url_player_create(url_notify_cb_t *notify_cb, void *ctx);

int url_player_destroy(url_player_t **self_p);

/*
 * insert and update a stream
 */
int url_player_insert_stream(url_player_t *self, url_stream_t *url_item);

int url_player_update_stream(url_player_t *self, char *stream_item, char *url, long report_delay,
                             long report_interval, long report_position);

int url_player_set_cmd(url_player_t *self, e_player_cmd_t cmd, long long param);

int url_player_get_value(url_player_t *self, e_value_t value_type, int *val);

int url_player_set_value(url_player_t *self, e_value_t value_type, int val);

stream_state_t *url_player_get_state(url_player_t *self, int client_type);

void url_player_free_state(stream_state_t *stream_state);

int url_player_start_play_url(url_player_t *self, int type, const char *url,
                              long long play_offset_ms);

void url_player_stop_play_url(url_player_t *self, int type);

int url_player_get_play_url_state(url_player_t *self, int type);

void set_player_mode(url_player_t *self, int play_mode);

void url_player_set_more_playlist(url_player_t *self, int need_more_playlist);

void url_player_set_history_playlist(url_player_t *self, int need_history_playlist);

int url_player_get_more_playlist(url_player_t *self);

int url_player_get_history_playlist(url_player_t *self);

int url_player_get_play_status(url_player_t *self);

int url_player_get_playid_index(url_player_t *self, char *play_id);

int url_player_refresh_playid_url(url_player_t *self, char *play_id, char *new_url);

#ifdef __cplusplus
}
#endif

#endif
