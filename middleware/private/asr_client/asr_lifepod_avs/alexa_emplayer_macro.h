
#ifndef __ALEXA_EMPLAYER_MACRO_H__
#define __ALEXA_EMPLAYER_MACRO_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define Key_name ("name")
#define Key_code ("code")
#define Key_type ("type")
#define Key_tiny ("tiny")
#define Key_full ("full")
#define Key_small ("small")
#define Key_large ("large")
#define Key_album ("album")
#define Key_state ("state")
#define Key_value ("value")
#define Key_cause ("cause")
#define Key_fatal ("fatal")
#define Key_index ("index")
#define Key_media ("media")
#define Key_medium ("medium")
#define Key_artist ("artist")
#define Key_header ("header")
#define Key_active ("active")
#define Key_change ("change")
#define Key_repeat ("repeat")
#define Key_enabled ("enabled")
#define Key_shuffle ("shuffle")
#define Key_context ("context")
#define Key_payload ("payload")
#define Key_isGuest ("isGuest")
#define Key_players ("players")
#define Key_albumId ("albumId")
#define Key_trackId ("trackId")
#define Key_coverId ("coverId")
#define Key_favorite ("favorite")
#define Key_launched ("launched")
#define Key_loggedIn ("loggedIn")
#define Key_username ("username")
#define Key_userName ("userName")
#define Key_playerId ("playerId")
#define Key_artistId ("artistId")
#define Key_coverUrls ("coverUrls")
#define Key_namespace ("namespace")
#define Key_trackName ("trackName")
#define Key_mediaType ("mediaType")
#define Key_directive ("directive")
#define Key_eventName ("eventName")
#define Key_errorName ("errorName")
#define Key_endpointId ("endpointId")
#define Key_properties ("properties")
#define Key_forceLogin ("forceLogin")
#define Key_description ("description")
#define Key_accessToken ("accessToken")
#define Key_trackNumber ("trackNumber")
#define Key_timeOfSample ("timeOfSample")
#define Key_playerInFocus ("playerInFocus")
#define Key_mediaProvider ("mediaProvider")
#define Key_payloadVersion ("payloadVersion")
#define Key_playbackSource ("playbackSource")
#define Key_playbackSourceId ("playbackSourceId")
#define Key_correlationToken ("correlationToken")
#define Key_supportedOperations ("supportedOperations")
#define Key_positionMilliseconds ("positionMilliseconds")
#define Key_playbackContextToken ("playbackContextToken")
#define Key_offsetInMilliseconds ("offsetInMilliseconds")
#define Key_durationInMilliseconds ("durationInMilliseconds")
#define Key_deltaPositionMilliseconds ("deltaPositionMilliseconds")
#define Key_uncertaintyInMilliseconds ("uncertaintyInMilliseconds")
#define Key_tokenTimeout ("tokenRefreshIntervalInMilliseconds")

// { for Transport Control
#define Val_Play ("Play")
#define Val_Stop ("Stop")
#define Val_Next ("Next")
#define Val_Pause ("Pause")
#define Val_Rewind ("Rewind")
#define Val_Favorite ("Favorite")
#define Val_Previous ("Previous")
#define Val_StartOver ("StartOver")
#define Val_Unfavorite ("Unfavorite")
#define Val_UnFavorite ("UnFavorite")
#define Val_FastForward ("FastForward")
#define Val_EnableRepeat ("EnableRepeat")
#define Val_EnableShuffle ("EnableShuffle")
#define Val_DisableRepeat ("DisableRepeat")
#define Val_DisableShuffle ("DisableShuffle")
#define Val_SetSeekPosition ("SetSeekPosition")
#define Val_EnableRepeatOne ("EnableRepeatOne")
#define Val_AdjustSeekPosition ("AdjustSeekPosition")

// }

#define Val_ALEXA ("Alexa")
#define Val_Login ("Login")
#define Val_Logout ("Logout")
#define Val_PlayerEvent ("PlayerEvent")
#define Val_PlayerError ("PlayerError")
#define Val_RequestToken ("RequestToken")
#define Val_ChangeReport ("ChangeReport")
#define Val_PlaybackState ("playbackState")
#define Val_ExternalMediaPlayer ("ExternalMediaPlayer")
#define Val_AlexaSeekController ("Alexa.SeekController")
#define Val_AlexaPlaylistController ("Alexa.PlaylistController")
#define Val_AlexaPlaybackController ("Alexa.PlaybackController")
#define Val_ExternalMediaPlayerState ("ExternalMediaPlayerState")
#define Val_AlexaPlaybackStateReporter ("Alexa.PlaybackStateReporter")
#define Val_ExternalMediaPlayerMusicItem ("ExternalMediaPlayerMusicItem")

// { for PlayerEvent.eventName
#define Val_TemporaryError ("TemporaryError")
#define Val_Disconnect ("Disconnect")
#define Val_Reconnect ("Reconnect")
#define Val_BecamePremium ("BecamePremium")
#define Val_LostPremium ("LostPremium")
#define Val_PlaybackStarted ("PlaybackStarted")
#define Val_PlaybackStopped ("PlaybackStopped")
#define Val_TrackChanged ("TrackChanged")
#define Val_BecameActive ("BecameActive")
#define Val_BecameInactive ("BecameInactive")
#define Val_PlaybackFailed ("PlaybackFailed")
#define Val_AudioFlush ("AudioFlush")
#define Val_PlaybackFinished ("PlaybackFinished")
#define Val_PlayingAd ("PlayingAd")
#define Val_Seek ("Seek")
#define Val_NoContext ("NoContext")
#define Val_NoPrevious ("NoPrevious")
#define Val_NoNext ("NoNext")

// }

// "AmazonMusic"
// "TRACK",
// "PLAYING"
// "2017-02-03T16:20:50.52Z"

#define Val_TRACK ("TRACK")
#define Val_RATED ("RATED")
#define Val_PAUSED ("PAUSED")
#define Val_PLAYING ("PLAYING")
#define Val_SHUFFLED ("SHUFFLED")
#define Val_REPEATED ("REPEATED")
#define Val_NOT_RATED ("NOT_RATED")
#define Val_AmazonMusic ("AmazonMusic")
#define Val_Spotify_ESDK ("Spotify:ESDK")
#define Val_NOT_SHUFFLED ("NOT_SHUFFLED")
#define Val_NOT_REPEATED ("NOT_REPEATED")
#define Val_APP_INTERACTION ("APP_INTERACTION")

#define Cmd_Play ("Play")
#define Cmd_Login ("Login")
#define Cmd_Logout ("Logout")
#define Cmd_online ("online")
#define Cmd_TransportControl ("TransportControl")

#define Event_Login ("Login")
#define Event_Logout ("Logout")
#define Event_PlayerEvent ("PlayerEvent")
#define Event_PlayerError ("PlayerError")
#define Event_RequestToken ("RequestToken")
#define Event_ChangeReport ("ChangeReport")

#define Val_online ("online")
#define Val_action ("action")
#define Val_offsetms ("offsetms")
#define Val_userName ("userName")
#define Val_refreshTime ("refreshTime")

#define Val_avsLogin ("avsLogin")
#define Val_needUpdateState ("needUpdateState")
#define Val_playerState ("playerState")
#define Val_playbackState ("playbackState")

#ifndef Val_Obj
#define Val_Obj(obj) ((obj) ? (obj) : ("{}"))
#endif

#ifndef Val_Media
#define Val_Media(obj) ((obj) ? (obj) : ("NULL"))
#endif

#ifndef Val_Array
#define Val_Array(array) ((array) ? (array) : ("[]"))
#endif

#ifndef Val_Bool
#define Val_Bool(val) ((val) ? ("true") : ("false"))
#endif

#ifndef Val_favorite
#define Val_favorite(val) ((val) ? ("RATED") : ("NOT_RATED"))
#endif

#ifndef Val_state
#define Val_state(state) ((state) ? ("PLAYING") : ("PAUSED"))
#endif

#ifndef Val_shuffle
#define Val_shuffle(val) ((val) ? ("SHUFFLED") : ("NOT_SHUFFLED"))
#endif

#ifndef Val_repeat
#define Val_repeat(val) ((val) ? ("REPEATED") : ("NOT_REPEATED"))
#endif

#ifndef Val_Str
#define Val_Str(str) ((str && strlen(str) > 0) ? (str) : (""))
#endif

#define Format_ExternalMediaPlayerState                                                            \
    ("{"                                                                                           \
     "\"header\":{"                                                                                \
     "\"namespace\":\"ExternalMediaPlayer\","                                                      \
     "\"name\":\"ExternalMediaPlayerState\""                                                       \
     "},"                                                                                          \
     "\"payload\":{"                                                                               \
     "\"playerInFocus\":\"Spotify:ESDK\","                                                         \
     "\"players\":["                                                                               \
     "{"                                                                                           \
     "\"playerId\":\"Spotify:ESDK\","                                                              \
     "\"endpointId\":\"%s\","                                                                      \
     "\"username\":\"%s\","                                                                        \
     "\"loggedIn\":%s,"                                                                            \
     "\"isGuest\":%s,"                                                                             \
     "\"launched\":%s,"                                                                            \
     "\"active\":%s"                                                                               \
     "}"                                                                                           \
     "]"                                                                                           \
     "}"                                                                                           \
     "}")

#define Create_ExternalMediaPlayerState(buff, endpointId, username, loggedIn, isGuest, launched,   \
                                        active)                                                    \
    do {                                                                                           \
        asprintf(&(buff), Format_ExternalMediaPlayerState, (endpointId), (username), (loggedIn),   \
                 (isGuest), (launched), (active));                                                 \
    } while (0)

#define Format_PlaybackState_Val_Media                                                             \
    ("{"                                                                                           \
     "\"type\":\"ExternalMediaPlayerMusicItem\","                                                  \
     "\"value\":%s"                                                                                \
     "}")

#define Create_PlaybackState_Val_Media(buff, media)                                                \
    do {                                                                                           \
        asprintf(&(buff), Format_PlaybackState_Val_Media, (media));                                \
    } while (0)

#define Format_PlaybackState_Media                                                                 \
    ("{"                                                                                           \
     "\"type\":\"ExternalMediaPlayerMusicItem\","                                                  \
     "\"value\":{"                                                                                 \
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
     "}"                                                                                           \
     "}")

#define Create_PlaybackState_Media(buff, playbackSource, playbackSourceId, trackName, trackId,     \
                                   trackNumber, artist, artistId, album, albumId, coverId,         \
                                   durationInMilliseconds)                                         \
    do {                                                                                           \
        asprintf(&(buff), Format_PlaybackState_Media, (playbackSource), (playbackSourceId),        \
                 (trackName), (trackId), (trackNumber), (artist), (artistId), (album), (albumId),  \
                 (coverId), (durationInMilliseconds));                                             \
    } while (0)

#define Format_PlayBackState_State                                                                 \
    ("\"state\":\"%s\","                                                                           \
     "\"supportedOperations\":%s,"                                                                 \
     "\"media\":%s,"                                                                               \
     "\"positionMilliseconds\":%ld,"                                                               \
     "\"shuffle\":\"%s\","                                                                         \
     "\"repeat\":\"%s\","                                                                          \
     "\"favorite\":\"%s\"")

#define Create_PlaybackState_State(buff, state, supportedOperations, media, positionMilliseconds,  \
                                   shuffle, repeat, favorite)                                      \
    do {                                                                                           \
        asprintf(&(buff), Format_PlayBackState_State, (state), (supportedOperations), (media),     \
                 (positionMilliseconds), (shuffle), (repeat), (favorite));                         \
    } while (0)

#define Format_PlaybackState_Payload                                                               \
    ("{"                                                                                           \
     "%s,"                                                                                         \
     "\"players\":["                                                                               \
     "{"                                                                                           \
     "\"playerId\":\"Spotify:ESDK\","                                                              \
     "%s"                                                                                          \
     "}"                                                                                           \
     "]"                                                                                           \
     "}")

#define Create_PlaybackState_Payload(buff, state, players)                                         \
    do {                                                                                           \
        asprintf(&(buff), Format_PlaybackState_Payload, (state), (players));                       \
    } while (0)

#define Format_PlaybackState                                                                       \
    ("{"                                                                                           \
     "\"header\":{"                                                                                \
     "\"namespace\":\"Alexa.PlaybackStateReporter\","                                              \
     "\"name\":\"playbackState\""                                                                  \
     "},"                                                                                          \
     "\"payload\":%s"                                                                              \
     "}")

#define Create_PlaybackState(buff, payload)                                                        \
    do {                                                                                           \
        asprintf(&(buff), Format_PlaybackState, (payload));                                        \
    } while (0)

#if 0
#define Format_ChangeReport_Properties                                                             \
    ("{"                                                                                           \
     "\"namespace\":\"PlaybackStateReporter\","                                                    \
     "\"name\":\"playbackState\","                                                                 \
     "\"value\":%s,"                                                                               \
     "\"timeOfSample\":\"%s\","                                                                    \
     "\"uncertaintyInMilliseconds\":%ld"                                                           \
     "}")

#define Create_ChangeReport_Properties(buff, value, timeOfSample, uncertaintyInMilliseconds)       \
    do {                                                                                           \
        asprintf(&(buff), Format_ChangeReport_Properties, (value), (timeOfSample),                 \
                 (uncertaintyInMilliseconds));                                                     \
    } while (0)
#endif

#define Format_Emplayer_Cmd                                                                        \
    ("{"                                                                                           \
     "\"cmd_type\":\"emplayer\","                                                                  \
     "\"cmd\":\"%s\","                                                                             \
     "\"params\":%s"                                                                               \
     "}")

#define Create_Emplayer_Cmd(buff, cmd, parmas)                                                     \
    do {                                                                                           \
        asprintf(&(buff), Format_Emplayer_Cmd, (cmd), (parmas));                                   \
    } while (0)

#define Format_Emplayer_Online                                                                     \
    ("{"                                                                                           \
     "\"online\":%s"                                                                               \
     "}")

#define Create_Emplayer_Online_Params(buff, online)                                                \
    do {                                                                                           \
        asprintf(&(buff), Format_Emplayer_Online, online);                                         \
    } while (0)

#define Format_Emplayer_Play                                                                       \
    ("{"                                                                                           \
     "\"correlationToken\":\"%s\","                                                                \
     "\"playbackContextToken\":\"%s\","                                                            \
     "\"index\":%ld,"                                                                              \
     "\"offsetms\":%ld"                                                                            \
     "}")

#define Create_Emplayer_Play_Params(buff, correlationToken, playbackContextToken, index, offsetms) \
    do {                                                                                           \
        asprintf(&(buff), Format_Emplayer_Play, correlationToken, playbackContextToken, index,     \
                 offsetms);                                                                        \
    } while (0)

#define Format_Emplayer_Login                                                                      \
    ("{"                                                                                           \
     "\"accessToken\":\"%s\","                                                                     \
     "\"userName\":\"%s\","                                                                        \
     "\"refreshTime\":%ld,"                                                                        \
     "\"forceLogin\":%s"                                                                           \
     "}")

#define Create_Emplayer_Login_Params(buff, accessToken, userName, refreshTime, forceLogin)         \
    do {                                                                                           \
        asprintf(&(buff), Format_Emplayer_Login, accessToken, userName, refreshTime, forceLogin);  \
    } while (0)

#define Format_Emplayer_TransportControl                                                           \
    ("{"                                                                                           \
     "\"action\":\"%s\","                                                                          \
     "\"offsetms\":%ld"                                                                            \
     "}")

#define Create_Emplayer_TransportControl_Params(buff, action, offsetms)                            \
    do {                                                                                           \
        asprintf(&(buff), Format_Emplayer_TransportControl, action, offsetms);                     \
    } while (0)

#define AVS_SPOTIFY ("/tmp/avs_spotify")
#define Spotify_Notify(buf, buf_len, blocked)                                                      \
    SocketClientReadWriteMsg(AVS_SPOTIFY, buf, buf_len, NULL, NULL, blocked)

#endif
