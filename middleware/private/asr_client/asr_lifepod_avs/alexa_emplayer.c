
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>

#include <time.h>
#include <pthread.h>

#include "alexa_debug.h"
#include "alexa_json_common.h"
#include "alexa_request_json.h"
#include "alexa_focus_mgmt.h"
#include "alexa_emplayer.h"
#include "alexa_emplayer_macro.h"
#include "alexa.h"

#include "wm_util.h"

extern WIIMU_CONTEXT *g_wiimu_shm;

#define update_key_val(json_data, key, set_val)                                                    \
    do {                                                                                           \
        json_object_object_del((json_data), (key));                                                \
        json_object_object_add((json_data), (key), (set_val));                                     \
    } while (0)

typedef struct _emplayer_state_s {
    pthread_mutex_t state_lock;
    char *correlation_token;
    char *play_id;
    json_object *player_state_json;
    json_object *playback_state_json;
    long long refresh_time;
} emplayer_state_t;

static emplayer_state_t g_emplayer_state;

int get_media_player_state(void)
{
    int ret = -1;
    char *media = NULL;
    char *state = NULL;
    char *players = NULL;
    char *player_state = NULL;
    char *playback_state = NULL;
    char *playback_payload = NULL;

    Create_ExternalMediaPlayerState(player_state, Val_Str(0), Val_Str(0), Val_Bool(0), Val_Bool(0),
                                    Val_Bool(0), Val_Bool(0));
    if (player_state) {
        pthread_mutex_lock(&g_emplayer_state.state_lock);
        if (g_emplayer_state.player_state_json) {
            json_object_put(g_emplayer_state.player_state_json);
            g_emplayer_state.player_state_json = NULL;
        }
        g_emplayer_state.player_state_json = json_tokener_parse(player_state);
        pthread_mutex_unlock(&g_emplayer_state.state_lock);
        free(player_state);
        player_state = NULL;
        ret = 0;
    }
    Create_PlaybackState_State(state, Val_state(0), Val_Array(0), Val_Media(0), 0L, Val_shuffle(0),
                               Val_repeat(0), Val_favorite(0));
    Create_PlaybackState_Media(media, Val_Str(0), Val_Str(0), Val_Str(0), Val_Str(0), Val_Str(0),
                               Val_Str(0), Val_Str(0), Val_Str(0), Val_Str(0), Val_Str(0), 0L);
    Create_PlaybackState_State(players, Val_state(0), Val_Array(0), Val_Media(media), 0L,
                               Val_shuffle(0), Val_repeat(0), Val_favorite(0));
    if (state && players) {
        Create_PlaybackState_Payload(playback_payload, state, players);
    }

    Create_PlaybackState(playback_state, Val_Obj(playback_payload));
    if (playback_state) {
        pthread_mutex_lock(&g_emplayer_state.state_lock);
        if (g_emplayer_state.playback_state_json) {
            json_object_put(g_emplayer_state.playback_state_json);
            g_emplayer_state.playback_state_json = NULL;
        }
        g_emplayer_state.playback_state_json = json_tokener_parse(playback_state);
        pthread_mutex_unlock(&g_emplayer_state.state_lock);
        free(playback_state);
        playback_state = NULL;
        ret = 0;
    }

    if (playback_payload) {
        free(playback_payload);
    }
    if (state) {
        free(state);
    }
    if (players) {
        free(players);
    }
    if (media) {
        free(media);
    }

    return ret;
}

int avs_emplayer_init(void)
{
    int ret = -1;

    memset(&g_emplayer_state, 0x0, sizeof(emplayer_state_t));
    pthread_mutex_init(&g_emplayer_state.state_lock, NULL);

    ret = get_media_player_state();
    if (ret != 0) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "get_media_player_state failed\n");
    }

    g_emplayer_state.play_id = strdup(Val_Spotify_ESDK);

    return ret;
}

int avs_emplayer_uninit(void)
{
    pthread_mutex_lock(&g_emplayer_state.state_lock);
    if (g_emplayer_state.correlation_token) {
        free(g_emplayer_state.correlation_token);
        g_emplayer_state.correlation_token = NULL;
    }
    if (g_emplayer_state.play_id) {
        free(g_emplayer_state.play_id);
        g_emplayer_state.play_id = NULL;
    }
    if (g_emplayer_state.player_state_json) {
        json_object_put(g_emplayer_state.player_state_json);
        g_emplayer_state.player_state_json = NULL;
    }
    if (g_emplayer_state.playback_state_json) {
        json_object_put(g_emplayer_state.playback_state_json);
        g_emplayer_state.playback_state_json = NULL;
    }
    pthread_mutex_unlock(&g_emplayer_state.state_lock);

    return 0;
}

int avs_emplayer_parse_play(json_object *js_data)
{
    if (js_data) {
        char *corr_token = NULL;
        json_object *js_header = json_object_object_get(js_data, Key_header);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, Key_namespace);
            JSON_GET_STRING_FROM_OBJECT(js_header, Key_name);
            corr_token = JSON_GET_STRING_FROM_OBJECT(js_header, Key_correlationToken);
            if (corr_token) {
                pthread_mutex_lock(&g_emplayer_state.state_lock);
                if (g_emplayer_state.correlation_token) {
                    free(g_emplayer_state.correlation_token);
                }
                g_emplayer_state.correlation_token = strdup(corr_token);
                pthread_mutex_unlock(&g_emplayer_state.state_lock);
            }
        }
        json_object *js_payload = json_object_object_get(js_data, Key_payload);
        if (js_payload) {
            char *context_token = JSON_GET_STRING_FROM_OBJECT(js_payload, Key_playbackContextToken);
            char *play_id = JSON_GET_STRING_FROM_OBJECT(js_payload, Key_playerId);
            long index = JSON_GET_LONG_FROM_OBJECT(js_payload, Key_index);
            long offsetms = JSON_GET_LONG_FROM_OBJECT(js_payload, Key_offsetInMilliseconds);
            if (play_id) {
                pthread_mutex_lock(&g_emplayer_state.state_lock);
                if (g_emplayer_state.play_id) {
                    free(g_emplayer_state.play_id);
                }
                g_emplayer_state.play_id = strdup(play_id);
                pthread_mutex_unlock(&g_emplayer_state.state_lock);
            }
            if (context_token && play_id) {
                char *cmd = NULL;
                char *params = NULL;
                Create_Emplayer_Play_Params(params, Val_Str(corr_token), Val_Str(context_token),
                                            index, offsetms);
                if (params) {
                    Create_Emplayer_Cmd(cmd, Cmd_Play, params);
                    free(params);
                }
                if (cmd) {
                    focus_mgmt_activity_status_set(ExternalMediaPlayer, FOCUS);
                    Spotify_Notify(cmd, strlen(cmd), 0);
                    free(cmd);
                }
            }
        }

        return 0;
    }

    return -1;
}

int avs_emplayer_parse_login(json_object *js_data)
{
    if (js_data) {
        json_object *js_header = json_object_object_get(js_data, Key_header);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, Key_namespace);
            JSON_GET_STRING_FROM_OBJECT(js_header, Key_name);
        }

        json_object *js_payload = json_object_object_get(js_data, Key_payload);
        if (js_payload) {
            char *access_token = JSON_GET_STRING_FROM_OBJECT(js_payload, Key_accessToken);
            char *user_name = JSON_GET_STRING_FROM_OBJECT(js_payload, Key_username);
            char *play_id = JSON_GET_STRING_FROM_OBJECT(js_payload, Key_playerId);
            long refreshTime = JSON_GET_LONG_FROM_OBJECT(js_payload, Key_tokenTimeout);
            json_bool fore_login = JSON_GET_BOOL_FROM_OBJECT(js_payload, Key_forceLogin);
            if (access_token && play_id) {
                char *cmd = NULL;
                char *params = NULL;
                Create_Emplayer_Login_Params(params, Val_Str(access_token), Val_Str(user_name),
                                             refreshTime, Val_Bool(fore_login));
                if (params) {
                    Create_Emplayer_Cmd(cmd, Cmd_Login, params);
                    free(params);
                }
                if (cmd) {
                    Spotify_Notify(cmd, strlen(cmd), 0);
                    free(cmd);
                }
            }
        }

        return 0;
    }

    return -1;
}

int avs_emplayer_parse_logout(json_object *js_data)
{
    if (js_data) {
        json_object *js_header = json_object_object_get(js_data, Key_header);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, Key_namespace);
            JSON_GET_STRING_FROM_OBJECT(js_header, Key_name);
        }

        json_object *js_payload = json_object_object_get(js_data, Key_payload);
        if (js_payload) {
            char *cmd = NULL;
            Create_Emplayer_Cmd(cmd, Cmd_Logout, Val_Obj(0));
            if (cmd) {
                Spotify_Notify(cmd, strlen(cmd), 0);
                free(cmd);
            }
        }
        return 0;
    }

    return -1;
}

int avs_emplayer_notify_online(int online)
{
    char *cmd = NULL;
    char *params = NULL;

#ifdef EN_AVS_SPOTIFY
    Create_Emplayer_Online_Params(params, Val_Bool(online));
    if (params) {
        Create_Emplayer_Cmd(cmd, Cmd_online, params);
        free(params);
    }
    if (cmd) {
        Spotify_Notify(cmd, strlen(cmd), 0);
        free(cmd);
        return 0;
    }
#endif

    return -1;
}

int avs_emplayer_notify_action(char *action, long offsetms)
{
    char *cmd = NULL;
    char *params = NULL;

    Create_Emplayer_TransportControl_Params(params, action, offsetms);
    if (params) {
        Create_Emplayer_Cmd(cmd, Cmd_TransportControl, params);
        free(params);
    }
    if (cmd) {
        // alexa_mode_switch_set(1);
        Spotify_Notify(cmd, strlen(cmd), 0);
        free(cmd);
        return 0;
    }

    return -1;
}

int avs_emplayer_directive_check(char *name_space)
{
    int ret = -1;

    if (StrEq(name_space, Val_ExternalMediaPlayer) ||
        StrEq(name_space, Val_AlexaPlaybackController) ||
        StrEq(name_space, Val_AlexaPlaylistController) ||
        StrEq(name_space, Val_AlexaSeekController)) {
        ret = 0;
    }

    return ret;
}

// parse directive
int avs_emplayer_parse_directive(json_object *js_data)
{
    int ret = 0;

#ifdef EN_AVS_SPOTIFY
    if (js_data) {
        json_object *json_header = json_object_object_get(js_data, Key_header);
        if (json_header) {
            char *name_space = JSON_GET_STRING_FROM_OBJECT(json_header, Key_namespace);
            char *name = JSON_GET_STRING_FROM_OBJECT(json_header, Key_name);
            if (name && name_space) {
                if (StrEq(name_space, Val_ExternalMediaPlayer)) {
                    if (StrEq(name, Val_Play)) {
                        avs_emplayer_parse_play(js_data);

                    } else if (StrEq(name, Val_Login)) {
                        avs_emplayer_parse_login(js_data);

                    } else if (StrEq(name, Val_Logout)) {
                        avs_emplayer_parse_logout(js_data);

                    } else {
                        ret = -1;
                    }
                } else if (StrEq(name_space, Val_AlexaPlaybackController)) {
                    ret = avs_emplayer_notify_action(name, 0);

                } else if (StrEq(name_space, Val_AlexaPlaylistController)) {
                    ret = avs_emplayer_notify_action(name, 0);

                } else if (StrEq(name_space, Val_AlexaSeekController)) {
                    json_object *payload = NULL;
                    long offset_ms = 0;
                    payload = json_object_object_get(js_data, KEY_PAYLOAD);
                    if (StrEq(name, Val_AdjustSeekPosition)) {
                        offset_ms =
                            JSON_GET_LONG_FROM_OBJECT(payload, Key_deltaPositionMilliseconds);
                        avs_emplayer_notify_action(name, offset_ms);
                    } else if (StrEq(name, Val_SetSeekPosition)) {
                        offset_ms = JSON_GET_LONG_FROM_OBJECT(payload, Key_positionMilliseconds);
                        avs_emplayer_notify_action(name, offset_ms);
                    } else {
                        ret = -1;
                    }
                }
            }
        }
    }
#endif
    if (ret != 0) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "prase message error\n");
    }

    return ret;
}

int avs_emplayer_update_state(json_object *player_state_js, json_object *playback_state_js)
{
    int ret = -1;
    char *state = NULL;
    char *media = NULL;
    char *players = NULL;
    char *player_state = NULL;
    char *playback_state = NULL;
    char *playback_payload = NULL;

    if (player_state_js) {
        char *endpointId = JSON_GET_STRING_FROM_OBJECT(player_state_js, Key_endpointId);
        char *username = JSON_GET_STRING_FROM_OBJECT(player_state_js, Key_username);
        json_bool loggedIn = JSON_GET_BOOL_FROM_OBJECT(player_state_js, Key_loggedIn);
        json_bool isGuest = JSON_GET_BOOL_FROM_OBJECT(player_state_js, Key_isGuest);
        json_bool launched = JSON_GET_BOOL_FROM_OBJECT(player_state_js, Key_launched);
        json_bool active = JSON_GET_BOOL_FROM_OBJECT(player_state_js, Key_active);
        Create_ExternalMediaPlayerState(player_state, Val_Str(endpointId), Val_Str(username),
                                        Val_Bool(loggedIn), Val_Bool(isGuest), Val_Bool(launched),
                                        Val_Bool(active));
        if (player_state) {
            pthread_mutex_lock(&g_emplayer_state.state_lock);
            if (g_emplayer_state.player_state_json) {
                json_object_put(g_emplayer_state.player_state_json);
                g_emplayer_state.player_state_json = NULL;
            }
            g_emplayer_state.player_state_json = json_tokener_parse(player_state);
            pthread_mutex_unlock(&g_emplayer_state.state_lock);
            ret = 0;
        }
    }

    if (playback_state_js) {
        char *state_str = JSON_GET_STRING_FROM_OBJECT(playback_state_js, Key_state);
        json_object *support_ops_js =
            json_object_object_get(playback_state_js, Key_supportedOperations);
        const char *support_ops_str = NULL;
        json_object *media_js = json_object_object_get(playback_state_js, Key_media);
        const char *media_str = NULL;
        char *shuffle_str = JSON_GET_STRING_FROM_OBJECT(playback_state_js, Key_shuffle);
        char *repeat_str = JSON_GET_STRING_FROM_OBJECT(playback_state_js, Key_repeat);
        char *favorite_str = JSON_GET_STRING_FROM_OBJECT(playback_state_js, Key_favorite);
        long position_ms = JSON_GET_LONG_FROM_OBJECT(playback_state_js, Key_positionMilliseconds);

        if (support_ops_js) {
            support_ops_str = json_object_to_json_string(support_ops_js);
        }
        if (media_js) {
            media_str = json_object_to_json_string(media_js);
            Create_PlaybackState_Val_Media(media, Val_Obj(media_str));
        }
        Create_PlaybackState_State(state, Val_Str(state_str), Val_Array(support_ops_str),
                                   Val_Media(0), position_ms, Val_Str(shuffle_str),
                                   Val_Str(repeat_str), Val_Str(favorite_str));
        Create_PlaybackState_State(players, Val_Str(state_str), Val_Array(support_ops_str),
                                   Val_Media(media), position_ms, Val_Str(shuffle_str),
                                   Val_Str(repeat_str), Val_Str(favorite_str));
        if (state && players) {
            Create_PlaybackState_Payload(playback_payload, state, players);
        }
        Create_PlaybackState(playback_state, Val_Obj(playback_payload));
        if (playback_state) {
            pthread_mutex_lock(&g_emplayer_state.state_lock);
            if (g_emplayer_state.playback_state_json) {
                json_object_put(g_emplayer_state.playback_state_json);
                g_emplayer_state.playback_state_json = NULL;
            }
            g_emplayer_state.playback_state_json = json_tokener_parse(playback_state);
            pthread_mutex_unlock(&g_emplayer_state.state_lock);
            ret = 0;
        }
    }

    if (player_state) {
        free(player_state);
    }
    if (playback_state) {
        free(playback_state);
    }
    if (media) {
        free(media);
    }
    if (playback_payload) {
        free(playback_payload);
    }
    if (state) {
        free(state);
    }
    if (players) {
        free(players);
    }

    return ret;
}

// parse the commmand from spotify thread
char *avs_emplayer_parse_cmd(char *json_cmd_str)
{
    json_object *json_cmd = NULL;
    json_object *json_cmd_parmas = NULL;
    const char *cmd_params_str = NULL;
    char *event_type = NULL;
    char *event = NULL;
    char *avs_cmd = NULL;

#ifdef EN_AVS_SPOTIFY
    if (!json_cmd_str) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "input is NULL\n");
        return NULL;
    }

    json_cmd = json_tokener_parse(json_cmd_str);
    if (!json_cmd) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "input is not a json\n");
        return NULL;
    }

    event_type = JSON_GET_STRING_FROM_OBJECT(json_cmd, Key_Event_type);
    if (!event_type) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "cmd none name found\n");
        goto Exit;
    }

    if (StrEq(event_type, Val_emplayer)) {
        event = JSON_GET_STRING_FROM_OBJECT(json_cmd, Key_Event);
        if (event) {
            json_cmd_parmas = json_object_object_get(json_cmd, Key_Event_params);
            if (json_cmd_parmas) {
                cmd_params_str = json_object_to_json_string(json_cmd_parmas);
            }

            json_bool need_update = JSON_GET_BOOL_FROM_OBJECT(json_cmd_parmas, Val_needUpdateState);
            if (need_update) {
                json_object *player_state =
                    json_object_object_get(json_cmd_parmas, Val_playerState);
                json_object *playback_state =
                    json_object_object_get(json_cmd_parmas, Val_playbackState);
                avs_emplayer_update_state(player_state, playback_state);
            }
            if (StrEq(event, Event_ChangeReport)) {
                Create_Cmd(avs_cmd, 0, Val_ALEXA, Event_ChangeReport, Val_Obj(cmd_params_str));

            } else if (StrEq(event, Event_RequestToken)) {
                Create_Cmd(avs_cmd, 0, Val_ExternalMediaPlayer, Event_RequestToken,
                           Val_Obj(cmd_params_str));

            } else if (StrEq(event, Event_PlayerEvent)) {
                Create_Cmd(avs_cmd, 0, Val_ExternalMediaPlayer, Event_PlayerEvent,
                           Val_Obj(cmd_params_str));
                char *eventName = JSON_GET_STRING_FROM_OBJECT(json_cmd_parmas, Key_eventName);
                if (StrEq(eventName, Val_PlaybackStarted)) {
                    focus_mgmt_activity_status_set(ExternalMediaPlayer, FOCUS);
                } else if (StrEq(eventName, Val_PlaybackStopped)) {
                    focus_mgmt_activity_status_set(ExternalMediaPlayer, UNFOCUS);
                }

            } else if (StrEq(event, Event_Login)) {
                json_bool avs_login = JSON_GET_BOOL_FROM_OBJECT(json_cmd_parmas, Val_avsLogin);
                if (!avs_login) {
                    Create_Cmd(avs_cmd, 0, Val_ExternalMediaPlayer, Event_Login,
                               Val_Obj(cmd_params_str));
                }

            } else if (StrEq(event, Event_Logout)) {
                Create_Cmd(avs_cmd, 0, Val_ExternalMediaPlayer, Event_Logout,
                           Val_Obj(cmd_params_str));

            } else if (StrEq(event, Event_PlayerError)) {
                Create_Cmd(avs_cmd, 0, Val_ExternalMediaPlayer, Event_PlayerError,
                           Val_Obj(cmd_params_str));
            }
        }
    }
#endif

Exit:
    if (json_cmd) {
        json_object_put(json_cmd);
    }

    return avs_cmd;
}

/*
 * Create event header
 * Input :
 *         @name_space: the type of event
 *         @name: the name of event
 *
 * Output: an json
 */
json_object *avs_emplayer_json_header(char *name_space, char *name, int event)
{
    json_object *obj_header = NULL;

    if (!name_space || !name) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "input error\n");
        return NULL;
    }

    obj_header = alexa_event_header(name_space, name);

    if ((StrEq(name, Val_PlayerEvent) || StrEq(name, Val_PlayerError)) && obj_header && event) {
        pthread_mutex_lock(&g_emplayer_state.state_lock);
        json_object_object_add(obj_header, Key_correlationToken,
                               JSON_OBJECT_NEW_STRING(g_emplayer_state.correlation_token));
        pthread_mutex_unlock(&g_emplayer_state.state_lock);
    }
    if (!obj_header) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "json_object_new_object failed\n");
    }

    return obj_header;
}

json_object *avs_emplayer_event_payload(char *cmd_name, json_object *cmd_parms)
{
    int ret = -1;
    json_object *json_event_payload = NULL;

    if (!cmd_name) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "cmd_name is NULL\n");
    }

    json_event_payload = json_object_new_object();
    if (!json_event_payload) {
        goto Exit;
    }

    ret = 0;
    if (StrEq(cmd_name, Val_RequestToken)) {
        pthread_mutex_lock(&g_emplayer_state.state_lock);
        json_object_object_add(json_event_payload, Key_playerId,
                               JSON_OBJECT_NEW_STRING(g_emplayer_state.play_id));
        pthread_mutex_unlock(&g_emplayer_state.state_lock);

    } else if (StrEq(cmd_name, Val_Login)) {
        pthread_mutex_lock(&g_emplayer_state.state_lock);
        json_object_object_add(json_event_payload, Key_playerId,
                               JSON_OBJECT_NEW_STRING(g_emplayer_state.play_id));
        pthread_mutex_unlock(&g_emplayer_state.state_lock);

    } else if (StrEq(cmd_name, Val_Logout)) {
        pthread_mutex_lock(&g_emplayer_state.state_lock);
        json_object_object_add(json_event_payload, Key_playerId,
                               JSON_OBJECT_NEW_STRING(g_emplayer_state.play_id));
        pthread_mutex_unlock(&g_emplayer_state.state_lock);

    } else if (StrEq(cmd_name, Val_PlayerEvent)) {
        char *eventName = JSON_GET_STRING_FROM_OBJECT(cmd_parms, Key_eventName);
        json_object_object_add(json_event_payload, Key_eventName,
                               JSON_OBJECT_NEW_STRING(eventName));
        pthread_mutex_lock(&g_emplayer_state.state_lock);
        json_object_object_add(json_event_payload, Key_playerId,
                               JSON_OBJECT_NEW_STRING(g_emplayer_state.play_id));
        pthread_mutex_unlock(&g_emplayer_state.state_lock);

    } else if (StrEq(cmd_name, Val_PlayerError)) {
        char *error_name = JSON_GET_STRING_FROM_OBJECT(cmd_parms, Key_errorName);
        long error_no = JSON_GET_LONG_FROM_OBJECT(cmd_parms, Key_code);
        char *error_des = JSON_GET_STRING_FROM_OBJECT(cmd_parms, Key_description);
        json_bool error_fatel = JSON_GET_BOOL_FROM_OBJECT(cmd_parms, Key_fatal);

        json_object_object_add(json_event_payload, Key_errorName,
                               JSON_OBJECT_NEW_STRING(error_name));
        json_object_object_add(json_event_payload, Key_code, json_object_new_int64(error_no));
        json_object_object_add(json_event_payload, Key_description,
                               JSON_OBJECT_NEW_STRING(error_des));
        json_object_object_add(json_event_payload, Key_fatal, json_object_new_boolean(error_fatel));

        pthread_mutex_lock(&g_emplayer_state.state_lock);
        json_object_object_add(json_event_payload, Key_playerId,
                               JSON_OBJECT_NEW_STRING(g_emplayer_state.play_id));
        pthread_mutex_unlock(&g_emplayer_state.state_lock);
    } else if (StrEq(cmd_name, Val_ChangeReport)) {
        json_object *json_change = json_object_new_object();
        if (json_change) {
            json_object *json_cause = json_object_new_object();
            if (json_cause) {
                json_object_object_add(json_cause, Key_type,
                                       JSON_OBJECT_NEW_STRING(Val_APP_INTERACTION));
                json_object_object_add(json_change, Key_cause, json_cause);
            }
            json_object *json_properties = json_object_new_array();
            if (json_properties) {
                json_object *json_item = json_object_new_object();
                if (json_item) {
                    json_object_object_add(json_item, Key_namespace,
                                           JSON_OBJECT_NEW_STRING(Val_AlexaPlaybackStateReporter));
                    json_object_object_add(json_item, Key_name,
                                           JSON_OBJECT_NEW_STRING(Val_PlaybackState));
                    pthread_mutex_lock(&g_emplayer_state.state_lock);
                    json_object *json_payload = NULL;
                    json_payload =
                        json_object_object_get(g_emplayer_state.playback_state_json, Key_payload);
                    if (json_payload) {
                        const char *json_val_str = json_object_to_json_string(json_payload);
                        json_object *json_val = json_tokener_parse(json_val_str);
                        if (json_val) {
                            json_object_object_add(json_item, Key_value, json_val);
                        }
                    }
                    pthread_mutex_unlock(&g_emplayer_state.state_lock);

                    time_t now = time(NULL);
                    struct tm *tm_now;
                    struct tm ptm = {0};
                    // "2017-02-03T16:20:50.52Z"
                    tm_now = localtime_r(&now, &ptm);
                    char time_sam[24] = {0};
                    snprintf(time_sam, sizeof(time_sam), "%04d-%02d-%02dT%02d:%02d:%02d.00Z",
                             tm_now->tm_year + 1900, tm_now->tm_mon + 1, tm_now->tm_mday,
                             tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
                    json_object_object_add(json_item, Key_timeOfSample,
                                           json_object_new_string(time_sam));
                    json_object_object_add(json_item, Key_uncertaintyInMilliseconds,
                                           json_object_new_int64(0));
                    json_object_array_add(json_properties, json_item);
                }
                json_object_object_add(json_change, Key_properties, json_properties);
            }
            json_object_object_add(json_event_payload, Key_change, json_change);
        }
    } else {
        ret = -1;
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "unknown name\n");
    }

Exit:
    if (ret != 0) {
        if (json_event_payload) {
            json_object_put(json_event_payload);
            json_event_payload = NULL;
        }
    }

    return json_event_payload;
}

int avs_emplayer_state(json_object *js_context_list, char *event_name)
{
    int ret = -1;
    const char *player_state_str = NULL;
    const char *playback_state_str = NULL;

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_emplayer_state start\n");

#ifdef EN_AVS_SPOTIFY
    if (js_context_list) {
        pthread_mutex_lock(&g_emplayer_state.state_lock);
        if (g_emplayer_state.player_state_json) {
            // update position
            long long play_pos = (long long)get_play_pos();
            if (!PLAYER_MODE_IS_SPOTIFY(g_wiimu_shm->play_mode) || play_pos < 0) {
                play_pos = 0;
            }
            json_object *json_payload =
                json_object_object_get(g_emplayer_state.playback_state_json, Key_payload);
            if (json_payload) {
                update_key_val(json_payload, Key_positionMilliseconds,
                               json_object_new_int64(play_pos));
                json_object *json_players = json_object_object_get(json_payload, Key_players);
                if (json_players) {
                    json_object *player_obj = json_object_array_get_idx(json_players, 0);
                    if (player_obj) {
                        json_object *json_players_pos =
                            json_object_object_get(player_obj, Key_positionMilliseconds);
                        if (json_players_pos)
                            update_key_val(player_obj, Key_positionMilliseconds,
                                           json_object_new_int64(play_pos));
                    }
                }
            }

            player_state_str = json_object_to_json_string(g_emplayer_state.player_state_json);
            if (player_state_str) {
                json_object *tmp = json_tokener_parse(player_state_str);
                if (tmp) {
                    json_object_array_add(js_context_list, tmp);
                }
            }
        }
        if (!StrEq(event_name, Val_ChangeReport) && g_emplayer_state.playback_state_json) {
            playback_state_str = json_object_to_json_string(g_emplayer_state.playback_state_json);
            if (playback_state_str) {
                json_object *tmp = json_tokener_parse(playback_state_str);
                if (tmp) {
                    json_object_array_add(js_context_list, tmp);
                }
            }
        }
        pthread_mutex_unlock(&g_emplayer_state.state_lock);
        ret = 0;
    }
#endif

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_emplayer_state end\n");

    return ret;
}

/*
  * in json_cmd format:
  * {
  *     "name": "",
  *     "params": {
  *
  *     }
  * }
  * out:
  *
  */
json_object *avs_emplayer_event_create(json_object *json_cmd)
{
    int ret = -1;
    char *cmd_name = NULL;
    json_object *json_event_header = NULL;
    json_object *json_event_payload = NULL;
    json_object *json_event = NULL;
    json_object *json_cmd_parms = NULL;

#ifdef EN_AVS_SPOTIFY
    if (!json_cmd) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "input error\n");
        goto Exit;
    }

    cmd_name = JSON_GET_STRING_FROM_OBJECT(json_cmd, Key_name);
    if (!cmd_name) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "cmd none name found\n");
        goto Exit;
    }

    if (StrEq(cmd_name, Val_ChangeReport)) {
        json_event_header = avs_emplayer_json_header(Val_ALEXA, cmd_name, 0);
    } else {
        json_event_header = avs_emplayer_json_header(Val_ExternalMediaPlayer, cmd_name, 1);
    }
    json_cmd_parms = json_object_object_get(json_cmd, KEY_params);
    json_event_payload = avs_emplayer_event_payload(cmd_name, json_cmd_parms);

    if (json_event_header && json_event_payload) {
        json_event = alexa_event_body(json_event_header, json_event_payload);
        if (json_event) {
            ret = 0;
        }
    }
#endif

Exit:
    if (ret != 0) {
        if (json_event_header) {
            json_object_put(json_event_header);
        }
        if (json_event_payload) {
            json_object_put(json_event_payload);
        }
        json_event = NULL;
    }

    return json_event;
}
