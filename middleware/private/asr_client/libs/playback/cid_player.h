/*************************************************************************
    > File Name: cid_player.h
    > Author: Keying.Zhao
    > Mail: Keying.Zhao@linkplay.com
    > Created Time: Tue 21 Nov 2017 02:35:55 AM EST
    > brief:
    >   A player for mp3/pcm buffer stream, url/file stream
 ************************************************************************/

#ifndef __CID_PLAYER_H__
#define __CID_PLAYER_H__

#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  brief: enum for the playback state
 *
 */
typedef enum {
    /* for init state */
    e_notify_none = -1,
    /* play started state */
    e_notify_started,
    /* play stopped state */
    e_notify_stopped,
    /* play near finished state */
    e_notify_near_finished,
    /* play finished state */
    e_notify_finished,
    /* play end state, that means all items play finished */
    e_notify_end,

    /* max notify support */
    e_notify_max,
} cid_notify_t;

/*
 *  brief: enum for the playback type
 *
 */
typedef enum {
    /* cid type init */
    e_type_none = -1,
    /* buffer for mp3 */
    e_type_buffer_mp3,
    /* buffer for pcm */
    e_type_buffer_pcm,
    /* url */
    e_type_url,
    /* file */
    e_type_file,

    /* max play type support */
    e_type_max,
} cid_type_t;

/*
 *  brief: enum for the playback type
 *
 */
typedef enum {
    /* stream type init */
    e_stream_none = -1,
    /* SPEAK */
    e_stream_speak,
    /* PLAY */
    e_stream_play,
    /* Notify */
    e_stream_notify,
    /* Alert */
    e_stream_alert,

    /* max stream type support */
    e_stream_max,
} stream_type_t;

/*
 *  brief: enum for the end queue behavior
 *
 */
typedef enum {
    /* behavior init value */
    e_replace_none = -1,
    /* replace all */
    e_replace_all,
    /* replace the last one */
    e_replace_endqueue,
    /* behavior add the end */
    e_endqueue,

    /* behavior max type */
    e_replace_max,
} e_behavior_t;

/*
 *  brief: the cmd enum
 *
 */
typedef enum {
    /* cmd init value */
    e_cmd_none = -1,
    /* stop current play */
    e_cmd_stop,
    /* clear all queue */
    e_cmd_clear_queue,
    /* change to play other music source: BT,Spotify,linein ... */
    e_cmd_source_change,
    /* TODO: pasue current play */
    e_cmd_pause,
    /* TODO: resume current play */
    e_cmd_resume,
    /* TODO: next play */
    e_cmd_next,
    /* TODO: previous play */
    e_cmd_previous,

    /* cmd max type */
    e_cmd_max,
} e_cmd_type_t;

/*
 *  brief: the type define about the cid_metadata_t
 *  member:
 *      @behavior:
 *          The behavior of end queue
 *      @cid_type:
 *          The type of play
 *      @stream_type:
 *          The type of the cid stream, it can be Speak, Play, File
 *      @token:
 *          The point to the cid item's token
 *      @uri:
 *          The uri of the item, can be url or file path
 *              @    http/https: the item is url mp3
 *              @    file: the item is file
 *              @     pcm: the item is pcm buffer
 *              @ default: the item is mp3 buffer
 */
typedef struct _cid_stream_s {
    int next_cached;
    e_behavior_t behavior;
    cid_type_t cid_type;
    stream_type_t stream_type;
    char *token;
    char *uri;
    char *dialog_id;
} cid_stream_t;

/*
 *  brief: the type define about the cid_player_t
 */
typedef struct _cid_player_s cid_player_t;

/*
 *  brief:
 *      type define about the cid_player's notify callback
 *  params:
 *      @notify:
 *          The type of notify
 *      @cid_stream:
 *          The point to the cid_stream
 *      @ctx:
 *          The context of which set by cid_player_create function
 *
 */
typedef int(cid_notify_cb_t)(const cid_notify_t notify, const cid_stream_t *cid_stream,
                             void *notify_ctx);

/*
 *  brief: create the cid_player object
 *  input:
 *      @notify_cb:
 *          The callback about the cid player state notify
 *      @ctx:
 *          The context which will pass through by callback
 *
 *  output:
 *      The point of cid_player_t object
 */
cid_player_t *cid_player_create(cid_notify_cb_t *notify_cb, void *notify_ctx);

/*
 *  brief: free the cid_player object
 *  input:
 *      @self_p:
 *          The point of the cid_player object
 *  output:
 *
 */
void cid_player_destroy(cid_player_t **self_p);

/*
 *  brief:
 *      Insert a item to the cid_player
 *  input:
 *      @self:
 *          The object of the cid_player
 *      @cid_stream:
 *          The cid stream metadata
 *  output:
 *      @ 0   sucess
 *      @ !0  failed
 */
int cid_player_add_item(cid_player_t *self, const cid_stream_t cid_stream);

/*
 *  brief:
 *      Write the buffer to the cid_player's cache buffer, only for buffer item
 *  input:
 *      @self:
 *          The object of the cid_player
 *      @content_id:
 *          The content id for the item, which is map to item's token
 *      @data:
 *          The buffer want to write
 *      @len:
 *          The length of buffer want to write
 *  output:
 *      @ 0   sucess
 *      @ !0  failed
 */
int cid_player_write_data(cid_player_t *self, char *content_id, char *data, int len);

/*
 *  brief:
 *      The buffer is EOF, set the cache buffer state is finished
 *  input:
 *      @self:
 *          The object of the cid_player
 *      @content_id:
 *          The content id for the item, which is map to item's token
 *  output:
 *      @ 0   sucess
 *      @ !0  failed
 */
int cid_player_cache_finished(cid_player_t *self, char *content_id);

/*
 *  brief:
 *      Stop current player
 *  input:
 *      @self:
 *          The object of the cid_player
 *      @cmd:
 *          The cmd to the player
 *  output:
 *      @ 0   sucess
 *      @ !0  failed
 */
int cid_player_set_cmd(cid_player_t *self, e_cmd_type_t cmd);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_PROCESSOR_H */
