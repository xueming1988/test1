/*************************************************************************
    > File Name: cid_player_naver.h
    > Author: Keying.Zhao
    > Mail: Keying.Zhao@linkplay.com
    > Created Time: Tue 21 Nov 2017 02:35:55 AM EST
    > brief:
    >   A player for mp3/pcm buffer stream, url/file stream
 ************************************************************************/

#ifndef __CID_PLAYER_NAVER_H__
#define __CID_PLAYER_NAVER_H__

#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    e_notify_none = -1,
    e_notify_started,
    e_notify_stopped,
    e_notify_near_finished,
    e_notify_finished,
    e_notify_end,

    e_notify_max,
} cid_notify_t;

typedef enum {
    e_cid_none = -1,
    e_cid_mp3,
    e_cid_url,
    e_cid_wav,
    e_cid_pcm,

    e_cid_max,
} cid_type_t;

typedef struct _cid_player_s cid_player_t;
typedef int(cid_notify_cb_t)(cid_notify_t notify, cid_type_t cid_type, void *data, void *ctx);

cid_player_t *cid_player_create(cid_notify_cb_t *notify_cb, void *ctx);

void cid_player_destroy(cid_player_t **self_p);

int cid_player_add_item(cid_player_t *self, char *dialog_id, char *token, char *url,
                        unsigned long long request_id);

int cid_player_write_data(cid_player_t *self, char *content_id, char *data, int len);

int cid_player_cache_finished(cid_player_t *self, char *content_id);

int cid_player_stop(cid_player_t *self, int clear_all, unsigned long long request_id);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_PROCESSOR_H */
