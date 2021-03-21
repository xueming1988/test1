

#ifndef __SPOTIFY_AVS_MACRO_H__
#define __SPOTIFY_AVS_MACRO_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"

#define Key_index ("index")
#define Key_online ("online")
#define Key_action ("action")
#define Key_offsetms ("offsetms")
#define Key_userName ("userName")
#define Key_forceLogin ("forceLogin")
#define Key_accessToken ("accessToken")
#define Key_refreshTime ("refreshTime")

#define Key_correlationToken ("correlationToken")
#define Key_playbackContextToken ("playbackContextToken")

// { for local cmd

#define Key_Cmd ("cmd")
#define Key_Cmd_type ("cmd_type")
#define Key_Cmd_params ("params")

#define Key_Event ("event")
#define Key_Event_type ("event_type")
#define Key_Event_params ("params")

#define Key_endpointId ("endpointId")
#define Key_username ("username")
#define Key_loggedIn ("loggedIn")
#define Key_active ("active")
#define Key_isGuest ("isGuest")
#define Key_launched ("launched")

#define Key_playbackSource ("playbackSource")
#define Key_playbackSourceId ("playbackSourceId")
#define Key_trackName ("trackName")
#define Key_trackId ("trackId")
#define Key_trackNumber ("trackNumber")
#define Key_artist ("artist")
#define Key_artistId ("artistId")
#define Key_album ("album")
#define Key_albumId ("albumId")
#define Key_coverUrls ("coverUrls")
#define Key_coverId ("coverId")
#define Key_mediaProvider ("mediaProvider")
#define Key_mediaType ("mediaType")
#define Key_durationInMilliseconds ("durationInMilliseconds")

#define Key_tiny ("tiny")
#define Key_small ("small")
#define Key_medium ("medium")
#define Key_large ("large")
#define Key_full ("full")

#define Val_TRACK ("TRACK")
#define Val_Spotify ("Spotify")

// }

#define Cmd_Play ("Play")
#define Cmd_Login ("Login")
#define Cmd_Logout ("Logout")
#define Cmd_online ("online")
#define Cmd_TransportControl ("TransportControl")

#define Val_emplayer ("emplayer")

#define Event_Login ("Login")
#define Event_Logout ("Logout")
#define Event_PlayerEvent ("PlayerEvent")
#define Event_PlayerError ("PlayerError")
#define Event_RequestToken ("RequestToken")
#define Event_ChangeReport ("ChangeReport")

#define Val_Play ("Play")
#define Val_Stop ("Stop")
#define Val_Next ("Next")
#define Val_Pause ("Pause")
#define Val_Rewind ("Rewind")
#define Val_Previous ("Previous")
#define Val_Favorite ("Favorite")
#define Val_StartOver ("StartOver")
#define Val_UnFavorite ("UnFavorite")
#define Val_FastForward ("FastForward")
#define Val_EnableRepeat ("EnableRepeat")
#define Val_EnableShuffle ("EnableShuffle")
#define Val_DisableRepeat ("DisableRepeat")
#define Val_DisableShuffle ("DisableShuffle")
#define Val_SetSeekPosition ("SetSeekPosition")
#define Val_EnableRepeatOne ("EnableRepeatOne")
#define Val_AdjustSeekPosition ("AdjustSeekPosition")

#define Val_Seek ("Seek")
#define Val_NoNext ("NoNext")
#define Val_RepeatOn ("RepeatOn")
#define Val_RepeatOff ("RepeatOff")
#define Val_PlayingAd ("PlayingAd")
#define Val_Reconnect ("Reconnect")
#define Val_ShuffleOn ("ShuffleOn")
#define Val_NoContext ("NoContext")
#define Val_NoPrevious ("NoPrevious")
#define Val_ShuffleOff ("ShuffleOff")
#define Val_AudioFlush ("AudioFlush")
#define Val_Disconnect ("Disconnect")
#define Val_LostPremium ("LostPremium")
#define Val_TrackChanged ("TrackChanged")
#define Val_BecameActive ("BecameActive")
#define Val_BecameInactive ("BecameInactive")
#define Val_PlaybackFailed ("PlaybackFailed")
#define Val_BecamePremium ("BecamePremium")
#define Val_TemporaryError ("TemporaryError")
#define Val_PlaybackStarted ("PlaybackStarted")
#define Val_PlaybackStopped ("PlaybackStopped")
#define Val_PlaybackFinished ("PlaybackFinished")

#define Val_Obj(obj) ((obj) ? (obj) : ("{}"))
#define Val_Media(media) ((media) ? (media) : ("NULL"))
#define Val_Urls(urls) ((urls) ? (urls) : ("NULL"))
#define Val_Array(array) ((strlen(array) > 0) ? (array) : ("[]"))
#define Val_Bool(val) ((val) ? ("true") : ("false"))
#define Val_favorite(val) ((val) ? ("RATED") : ("NOT_RATED"))
#define Val_state(state) ((state) ? ("PLAYING") : ("PAUSED"))
#define Val_shuffle(val) ((val) ? ("SHUFFLED") : ("NOT_SHUFFLED"))
#define Val_repeat(val) ((val) ? ("REPEATED") : ("NOT_REPEATED"))
#define Val_Str(str) ((str && strlen(str) > 0) ? (str) : (""))

#ifndef StrEq
#define StrEq(str1, str2)                                                                          \
    ((str1 && str2) ? ((strlen(str1) == strlen(str2)) && !strcmp(str1, str2)) : 0)
#endif

#ifndef StrInclude
#define StrInclude(str1, str2) ((str1 && str2) ? (!strncmp(str1, str2, strlen(str2))) : 0)
#endif

#define Format_BLE_Seek_Notify "{\"seek_changed\":{\"seeked_to\":%d,\"type\":\"spotify\"}}"

#define Create_BLE_Cmd(buff, cmd)                                                                  \
    do {                                                                                           \
        asprintf(&(buff), Format_BLE_Seek_Notify, (cmd));                                          \
    } while (0)

#define Format_Spotify_Notify                                                                      \
    ("{"                                                                                           \
     "\"event_type\":\"emplayer\","                                                                \
     "\"event\":\"%s\","                                                                           \
     "\"params\":%s"                                                                               \
     "}")

#define Create_Emplayer_Cmd(buff, cmd, parmas)                                                     \
    do {                                                                                           \
        asprintf(&(buff), Format_Spotify_Notify, (cmd), (parmas));                                 \
    } while (0)

#define Format_PlayerEvent_Params                                                                  \
    ("{"                                                                                           \
     "\"correlationToken\":\"%s\","                                                                \
     "\"eventName\":\"%s\","                                                                       \
     "\"needUpdateState\":%s,"                                                                     \
     "\"playerState\":%s,"                                                                         \
     "\"playbackState\":%s"                                                                        \
     "}")

#define Create_PlayerEvent_Params(buff, correlationToken, eventName, needUpdateState, playerState, \
                                  playbackState)                                                   \
    do {                                                                                           \
        asprintf(&(buff), Format_PlayerEvent_Params, (correlationToken), (eventName),              \
                 (needUpdateState), (playerState), (playbackState));                               \
    } while (0)

#define Format_PlayerError_Params                                                                  \
    ("{"                                                                                           \
     "\"correlationToken\":\"%s\","                                                                \
     "\"errorName\":\"%s\","                                                                       \
     "\"code\":%u,"                                                                                \
     "\"description\":\"%s\","                                                                     \
     "\"fatal\":%s"                                                                                \
     "}")

#define Create_PlayerError_Params(buff, correlationToken, errorName, code, description, fatal)     \
    do {                                                                                           \
        asprintf(&(buff), Format_PlayerError_Params, (correlationToken), (errorName), (code),      \
                 (description), (fatal));                                                          \
    } while (0)

#define Format_Comm_Params                                                                         \
    ("{"                                                                                           \
     "\"needUpdateState\":%s,"                                                                     \
     "\"playerState\":%s,"                                                                         \
     "\"playbackState\":%s"                                                                        \
     "}")

#define Create_Comm_Params(buff, needUpdateState, playerState, playbackState)                      \
    do {                                                                                           \
        asprintf(&(buff), Format_Comm_Params, (needUpdateState), (playerState), (playbackState));  \
    } while (0)

#define Format_Login_Params                                                                        \
    ("{"                                                                                           \
     "\"avsLogin\":%s,"                                                                            \
     "\"needUpdateState\":%s,"                                                                     \
     "\"playerState\":%s,"                                                                         \
     "\"playbackState\":%s"                                                                        \
     "}")

#define Create_Login_Params(buff, avsLogin, needUpdateState, playerState, playbackState)           \
    do {                                                                                           \
        asprintf(&(buff), Format_Login_Params, (avsLogin), (needUpdateState), (playerState),       \
                 (playbackState));                                                                 \
    } while (0)

#define Format_Player_State                                                                        \
    ("{"                                                                                           \
     "\"endpointId\":\"%s\","                                                                      \
     "\"username\":\"%s\","                                                                        \
     "\"loggedIn\":%s,"                                                                            \
     "\"isGuest\":%s,"                                                                             \
     "\"launched\":%s,"                                                                            \
     "\"active\":%s"                                                                               \
     "}")

#define Create_Player_State(buff, endpointId, username, loggedIn, isGuest, launched, active)       \
    do {                                                                                           \
        asprintf(&(buff), Format_Player_State, (endpointId), (username), (loggedIn), (isGuest),    \
                 (launched), (active));                                                            \
    } while (0)

#define Format_Playback_Media                                                                      \
    ("{"                                                                                           \
     "\"playbackSource\":\"%s\","                                                                  \
     "\"playbackSourceId\":\"%s\","                                                                \
     "\"trackName\":\"%s\","                                                                       \
     "\"trackId\":\"%s\","                                                                         \
     "\"trackNumber\":\"%s\","                                                                     \
     "\"artist\":\"%s\","                                                                          \
     "\"artistId\":\"%s\","                                                                        \
     "\"album\":\"%s\","                                                                           \
     "\"albumId\":\"%s\","                                                                         \
     "\"coverUrls\":\"NULL\","                                                                     \
     "\"coverId\":\"%s\","                                                                         \
     "\"mediaProvider\":\"Spotify\","                                                              \
     "\"mediaType\":\"TRACK\","                                                                    \
     "\"durationInMilliseconds\":%ld"                                                              \
     "}")

#define Create_Playback_Media(buff, playbackSource, playbackSourceId, trackName, trackId,          \
                              trackNumber, artist, artistId, album, albumId, coverId,              \
                              durationInMilliseconds)                                              \
    do {                                                                                           \
        asprintf(&(buff), Format_Playback_Media, (playbackSource), (playbackSourceId),             \
                 (trackName), (trackId), (trackNumber), (artist), (artistId), (album), (albumId),  \
                 (coverId), (durationInMilliseconds));                                             \
    } while (0)

#define Format_Playback_State                                                                      \
    ("{"                                                                                           \
     "\"state\":\"%s\","                                                                           \
     "\"supportedOperations\":%s,"                                                                 \
     "\"media\":%s,"                                                                               \
     "\"positionMilliseconds\":%u,"                                                                \
     "\"shuffle\":\"%s\","                                                                         \
     "\"repeat\":\"%s\","                                                                          \
     "\"favorite\":\"%s\""                                                                         \
     "}")

#define Create_Playback_State(buff, state, supportedOperations, media, positionMilliseconds,       \
                              shuffle, repeat, favorite)                                           \
    do {                                                                                           \
        asprintf(&(buff), Format_Playback_State, (state), (supportedOperations), (media),          \
                 (positionMilliseconds), (shuffle), (repeat), (favorite));                         \
    } while (0)

#define Format_Playback_Urls(tiny, small, medium, large, full)                                     \
    ("{"                                                                                           \
     "\"tiny\":\"%s\","                                                                            \
     "\"small\":\"%s\","                                                                           \
     "\"medium\":\"%s\","                                                                          \
     "\"large\":\"%s\","                                                                           \
     "\"full\":\"%s\""                                                                             \
     "}")

#define Create_Playback_Urls(buff, tiny, small, medium, large, full)                               \
    do {                                                                                           \
        asprintf(&(buff), Format_Playback_Urls, (tiny), (small), (medium), (large), (full));       \
    } while (0)

#define JSON_GET_INT_FROM_OBJECT(object, key)                                                      \
    ((object == NULL) ? 0 : ((json_object_object_get((object), (key))) == NULL)                    \
                                ? 0                                                                \
                                : (json_object_get_int(json_object_object_get((object), (key)))))

#define JSON_GET_LONG_FROM_OBJECT(object, key)                                                     \
    ((object == NULL) ? 0                                                                          \
                      : ((json_object_object_get((object), (key))) == NULL)                        \
                            ? 0                                                                    \
                            : (json_object_get_int64(json_object_object_get((object), (key)))))

#define JSON_GET_BOOL_FROM_OBJECT(object, key)                                                     \
    ((object == NULL) ? 0                                                                          \
                      : ((json_object_object_get((object), (key))) == NULL)                        \
                            ? 0                                                                    \
                            : (json_object_get_boolean(json_object_object_get((object), (key)))))

#define JSON_GET_STRING_FROM_OBJECT(object, key)                                                   \
    ((object == NULL)                                                                              \
         ? NULL                                                                                    \
         : ((json_object_object_get((object), (key)) == NULL)                                      \
                ? NULL                                                                             \
                : ((char *)json_object_get_string(json_object_object_get((object), (key))))))

#define JSON_OBJECT_NEW_STRING(str)                                                                \
    ((str) ? json_object_new_string((char *)(str)) : json_object_new_string(""))

#endif
