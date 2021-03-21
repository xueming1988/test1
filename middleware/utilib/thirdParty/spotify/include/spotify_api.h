#ifndef _SPOTIFY_API_H_
#define _SPOTIFY_API_H_

typedef enum {
    NoAction = 0,
    QuitAction = 1,
    PlayPauseAction = 2,
    PlayAction = 3,
    PauseAction = 4,
    NextAction = 5,
    PreviousAction = 6,
    VolumeSetAction = 7,
    SeekAction = 8,
    ShowNowPlayingAction = 9,
    PlaybackAction = 10,
    ShuffleAction = 11,
    RepeatAction = 12,
    BitrateAction = 13,
    StopAction = 14,
    ConnenctAction = 15,
    SetNameAction = 16,
    PlayPresetAction = 17,
    SavePresetAction = 18,
    logoutAction = 19,
    AddPlaylistAction = 20,
    PlayPlaylistAction = 21,
    AvsSpotifyAction = 22,
    RelativeSeekAction = 23,
} SpotifyAction;

typedef enum {
    sp_nofity_volume = 0,
    sp_notify_active,
    sp_notify_playstatus,
    sp_notify_postion,
    sp_notify_metadata,
    sp_nofity_loopmode,
    sp_nofity_playctrl,
    sp_nofity_avs,
    sp_nofity_login,
    sp_nofity_ble,
} SpotifyNotify;

struct SpMetadataOut {
    char playback_source[256];
    char playback_source_uri[128];
    char track[256];
    char track_uri[128];
    char artist[256];
    char album[256];
    char album_cover_uri[256];
    uint32_t duration_ms;
    uint32_t playback_restrictions;
};

typedef int (*SpNotifyUserCallBack)(int cmd, unsigned int params1, unsigned int param2);

int SpotifyConnectInit(const char *unique_id, const char *dname,
                       SpNotifyUserCallBack pSpNotifycallbacks, int device_type_input,
                       char *modelname, char *brandname, char *modeldisplayname,
                       char *branddisplayname);
int SpotifyUserCmd(SpotifyAction spCmd, unsigned int params);
int SpotifyForceStop(void);

#endif
