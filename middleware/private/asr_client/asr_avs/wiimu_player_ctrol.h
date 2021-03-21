
#ifndef __WIIMU_PLAYER_CTROL_H__
#define __WIIMU_PLAYER_CTROL_H__

#include <pthread.h>

typedef struct _player_ctrl_s {
    int play_state;
    int imuzop_state;
    int volume;
    int muted;
    int error_code;
    int provider;
    pthread_mutex_t ctrl_lock;
    char *stream_id;
    char *stream_url;
    long long position;
    long long duration;
} player_ctrl_t;

enum {
    PROVIDER_PANDORA = 1,
    PROVIDER_IHEARTRADIO,
};

#ifdef __cplusplus
extern "C" {
#endif

player_ctrl_t *wiimu_player_ctrl_init(void);

int wiimu_player_ctrl_uninit(player_ctrl_t **self);

int wiimu_player_ctrl_is_new(player_ctrl_t *self, char *stream_id, char *stream_url, long position,
                             int audible_book);

int wiimu_player_ctrl_set_uri(player_ctrl_t *self, char *stream_id, char *stream_item, char *url,
                              long position, int audible_book, int is_dialog);

int wiimu_player_ctrl_seek(player_ctrl_t *self, long long postion);

int wiimu_player_ctrl_stop(player_ctrl_t *self, int need_stop);

int wiimu_player_ctrl_resume(player_ctrl_t *self);

int wiimu_player_ctrl_set_volume(player_ctrl_t *self, int vol, int muted);

int wiimu_player_ctrl_get_volume(player_ctrl_t *self, int *volume, int *muted);

int wiimu_player_ctrl_update_info(player_ctrl_t *self);

int wiimu_player_ctrl_pasue(player_ctrl_t *self);

#ifdef __cplusplus
}
#endif

#endif
