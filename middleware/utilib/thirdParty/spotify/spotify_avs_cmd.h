
#ifndef __SPOTIFY_AVS_CMD_H__
#define __SPOTIFY_AVS_CMD_H__

#ifdef __cplusplus
extern "C" {
#endif

// Need to free the string after used
char *avs_set_player_state(char *endpointId, char *username, int loggedIn, int isGuest,
                           int launched, int active);

// Need to free the string after used
char *avs_set_media(char *playbackSource, char *playbackSourceId, char *trackName, char *trackId,
                    char *trackNumber, char *artist, char *artistId, char *album, char *albumId,
                    char *coverId, long durationInMilliseconds);

#ifdef __cplusplus
}
#endif

#endif
