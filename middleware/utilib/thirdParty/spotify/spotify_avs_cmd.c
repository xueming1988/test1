
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "spotify_avs_cmd.h"
#include "spotify_avs_macro.h"

char *avs_set_player_state(char *endpointId, char *username, int loggedIn, int isGuest,
                           int launched, int active)
{
    char *player_state_str = NULL;
    const char *js_str = NULL;
    json_object *player_state = NULL;

    player_state = json_object_new_object();
    if (player_state) {
        json_object_object_add(player_state, Key_endpointId, JSON_OBJECT_NEW_STRING(endpointId));
        json_object_object_add(player_state, Key_username, JSON_OBJECT_NEW_STRING(username));
        json_object_object_add(player_state, Key_loggedIn, json_object_new_boolean(loggedIn));
        json_object_object_add(player_state, Key_isGuest, json_object_new_boolean(isGuest));
        json_object_object_add(player_state, Key_launched, json_object_new_boolean(launched));
        json_object_object_add(player_state, Key_active, json_object_new_boolean(active));

        js_str = json_object_to_json_string(player_state);
        if (js_str) {
            player_state_str = strdup(js_str);
        }
        json_object_put(player_state);
    }

    return player_state_str;
}

char *avs_set_media(char *playbackSource, char *playbackSourceId, char *trackName, char *trackId,
                    char *trackNumber, char *artist, char *artistId, char *album, char *albumId,
                    char *coverId, long durationInMilliseconds)
{
    char *media_str = NULL;
    const char *js_str = NULL;
    json_object *media = NULL;

    media = json_object_new_object();
    if (media) {
        json_object_object_add(media, Key_playbackSource, JSON_OBJECT_NEW_STRING(playbackSource));
        json_object_object_add(media, Key_playbackSourceId,
                               JSON_OBJECT_NEW_STRING(playbackSourceId));
        json_object_object_add(media, Key_trackName, JSON_OBJECT_NEW_STRING(trackName));
        json_object_object_add(media, Key_trackId, JSON_OBJECT_NEW_STRING(trackId));
        json_object_object_add(media, Key_trackNumber, JSON_OBJECT_NEW_STRING(trackNumber));
        json_object_object_add(media, Key_artist, JSON_OBJECT_NEW_STRING(artist));
        json_object_object_add(media, Key_artistId, JSON_OBJECT_NEW_STRING(artistId));
        json_object_object_add(media, Key_album, JSON_OBJECT_NEW_STRING(album));
        json_object_object_add(media, Key_albumId, JSON_OBJECT_NEW_STRING(albumId));
        json_object_object_add(media, Key_coverId, JSON_OBJECT_NEW_STRING(coverId));
        json_object_object_add(media, Key_mediaProvider, JSON_OBJECT_NEW_STRING(Val_Spotify));
        json_object_object_add(media, Key_mediaType, JSON_OBJECT_NEW_STRING(Val_TRACK));
        json_object_object_add(media, Key_durationInMilliseconds,
                               json_object_new_int64(durationInMilliseconds));

        js_str = json_object_to_json_string(media);
        if (js_str) {
            media_str = strdup(js_str);
        }
        json_object_put(media);
    }

    return media_str;
}
