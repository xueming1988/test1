
#ifndef __WIIMU_PLAYER_CTROL_H__
#define __WIIMU_PLAYER_CTROL_H__

#include <pthread.h>
#include "wm_util.h"

typedef struct _player_ctrl_s {
    int play_state;
    int imuzop_state;
    int volume;
    int muted;
    int repeat_val;
    int error_code;
    pthread_mutex_t ctrl_lock;
    char *stream_id;
    char *stream_url;
    char *stream_data;
    share_context *mplayer_shmem;
    WIIMU_CONTEXT *wiimu_shm;
    long long position;
    long long duration;
} player_ctrl_t;

#ifdef __cplusplus
extern "C" {
#endif

player_ctrl_t *wiimu_player_ctrl_init(void);

int wiimu_player_ctrl_uninit(player_ctrl_t **self);

int wiimu_player_ctrl_set_uri(player_ctrl_t *self, char *stream_id, char *stream_item, char *url,
                              char *stream_data, long position);

int wiimu_player_ctrl_seek(player_ctrl_t *self, long long postion);

int wiimu_player_ctrl_stop(player_ctrl_t *self);

int wiimu_player_ctrl_pasue(player_ctrl_t *self);

int wiimu_player_ctrl_resume(player_ctrl_t *self);

int wiimu_player_ctrl_set_volume(player_ctrl_t *self, int set_mute, int val);

int wiimu_player_ctrl_get_volume(player_ctrl_t *self, int *volume, int *muted);

int wiimu_player_ctrl_update_info(player_ctrl_t *self);

// { for muti player
int wiimu_player_start_play_url(int type, const char *url, long long play_offset_ms);

void wiimu_player_stop_play_url(int type);

int wiimu_player_get_play_url_state(int type);
// }

int wiimu_player_ble_previous(void);

int wiimu_player_ble_next(void);

int wiimu_player_ctrl_is_new(player_ctrl_t *self, char *stream_id);

#ifdef ASR_WEIXIN_MODULE
int wiimu_player_ctrl_wiimu_next(player_ctrl_t *self);

int wiimu_player_ctrl_wiimu_prev(player_ctrl_t *self);

int wiimu_player_ctrl_wiimu_pause(player_ctrl_t *self);

int wiimu_player_ctrl_wiimu_resume(player_ctrl_t *self);

#endif

#ifdef __cplusplus
}
#endif

#endif
