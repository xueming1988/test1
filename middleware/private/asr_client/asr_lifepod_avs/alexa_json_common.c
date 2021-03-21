#include <stdio.h>
#include <stdbool.h>

#include "alexa_json_common.h"
/*
 * {
 *      "stream_id:%s,
 *      "offset\":%ld," \
 *      "duratioin:%ld,
 *      "error_num:%d
 * }
 */
int create_audioplayer_params(char **buff, char *stream_id, long off_set, long duratioin,
                              int error_num)
{
    json_object *cmd_params = NULL;

    if (off_set < 0) {
        fprintf(stderr, "%s invalid offset %ld < 0, set to 0\n", __FUNCTION__, off_set);
        off_set = 0;
    }

    cmd_params = json_object_new_object();
    if (cmd_params) {
        json_object_object_add(cmd_params, KEY_stream_id, JSON_OBJECT_NEW_STRING(stream_id));

        json_object *offset_js = json_object_new_int64(off_set);
        if (offset_js) {
            json_object_object_add(cmd_params, KEY_offset, offset_js);
        }

        json_object *duratioin_js = json_object_new_int64(duratioin);
        if (duratioin_js) {
            json_object_object_add(cmd_params, KEY_duratioin, duratioin_js);
        }

        json_object *error_num_js = json_object_new_int64(error_num);
        if (error_num_js) {
            json_object_object_add(cmd_params, KEY_error_num, error_num_js);
        }
        char *cmd_params_str = (char *)json_object_to_json_string(cmd_params);
        if (cmd_params_str) {
            *buff = strdup(cmd_params_str);
        }

        json_object_put(cmd_params);
    }

    return -1;
}

/*
 * {
 *      "key":%s,
 * }
 */
int create_string_params(char **buff, const char *key_name, const char *val_string)
{
    json_object *cmd_params = NULL;

    if (key_name && val_string) {
        cmd_params = json_object_new_object();
        if (cmd_params) {
            json_object_object_add(cmd_params, key_name, JSON_OBJECT_NEW_STRING(val_string));

            char *cmd_params_str = (char *)json_object_to_json_string(cmd_params);
            if (cmd_params_str) {
                *buff = strdup(cmd_params_str);
            }

            json_object_put(cmd_params);
        }
    }

    return -1;
}

/*{
 *   "inactive_ts":%ld,
 *   "err_msg":%s
 * }
 */
int create_boolean_params(char **buff, const char *key_name, bool val_bool)
{
    json_object *cmd_params = NULL;

    if (key_name) {
        cmd_params = json_object_new_object();
        if (cmd_params) {
            json_object_object_add(cmd_params, key_name, JSON_OBJECT_NEW_BOOLEAN(val_bool));

            char *cmd_params_str = (char *)json_object_to_json_string(cmd_params);
            if (cmd_params_str) {
                *buff = strdup(cmd_params_str);
            }

            json_object_put(cmd_params);
        }
    }

    return -1;
}
int create_system_params(char **buff, long inactive_ts, char *err_msg)
{
    json_object *cmd_params = NULL;
    json_object *err_msg_js = NULL;

    cmd_params = json_object_new_object();
    if (cmd_params) {
        json_object *inactive_ts_js = json_object_new_int64(inactive_ts);
        if (inactive_ts_js) {
            json_object_object_add(cmd_params, KEY_inactive_ts, inactive_ts_js);
        }

        if (err_msg) {
            err_msg_js = json_tokener_parse(err_msg);
            if (err_msg_js) {
                json_object_object_add(cmd_params, KEY_err_msg, err_msg_js);
            }
        }

        char *cmd_params_str = (char *)json_object_to_json_string(cmd_params);
        if (cmd_params_str) {
            *buff = strdup(cmd_params_str);
        }

        json_object_put(cmd_params);
    }

    return -1;
}

int create_avs_state(char **buff, int request_type, char *play_token, long offet, char *play_state,
                     int vol, int muted, char *speech_token, long offet1, char *play_state1)
{
    json_object *cmd_params = NULL;
    json_object *play_back_js = NULL;
    json_object *volume_js = NULL;
    json_object *speech_js = NULL;

    cmd_params = json_object_new_object();
    if (cmd_params) {
        json_object *request_type_js = json_object_new_int64(request_type);
        if (request_type_js) {
            json_object_object_add(cmd_params, KEY_RequestType, request_type_js);
        }

        play_back_js = json_object_new_object();
        if (play_back_js) {
            json_object_object_add(play_back_js, KEY_token, JSON_OBJECT_NEW_STRING(play_token));

            json_object *offet_js = json_object_new_int64(offet);
            if (offet_js) {
                json_object_object_add(play_back_js, KEY_offsetInMilliseconds, offet_js);
            }

            json_object_object_add(play_back_js, KEY_playerActivity,
                                   JSON_OBJECT_NEW_STRING(play_state));

            json_object_object_add(cmd_params, KEY_PlaybackState, play_back_js);
        }

        volume_js = json_object_new_object();
        if (volume_js) {
            json_object *vol_js = json_object_new_int64(vol);
            if (vol_js) {
                json_object_object_add(volume_js, KEY_volume, vol_js);
            }

            json_object *muted_js = json_object_new_boolean(muted);
            if (muted_js) {
                json_object_object_add(volume_js, KEY_muted, muted_js);
            }

            json_object_object_add(cmd_params, KEY_VolumeState, volume_js);
        }

        if (speech_token) {
            speech_js = json_object_new_object();
            if (speech_js) {
                json_object_object_add(speech_js, KEY_token, JSON_OBJECT_NEW_STRING(speech_token));

                json_object *offet1_js = json_object_new_int64(offet1);
                if (offet1_js) {
                    json_object_object_add(speech_js, KEY_offsetInMilliseconds, offet1_js);
                }

                json_object_object_add(speech_js, KEY_playerActivity,
                                       JSON_OBJECT_NEW_STRING(play_state1));

                json_object_object_add(cmd_params, KEY_SpeechState, speech_js);
            }
        }

        char *cmd_params_str = (char *)json_object_to_json_string(cmd_params);
        if (cmd_params_str) {
            *buff = strdup(cmd_params_str);
        }

        json_object_put(cmd_params);
    }

    return -1;
}

/*
 * Create AudioPlayer Cmd
 * Input :
 *         @cmd_name:
 *         @token: the token of current play
 *         @pos: the position of current play
 *
 * Output: int
 */
char *create_audio_cmd(int cmd_id, char *cmd_name, char *token, long pos, long duration,
                       int err_num)
{
    char *buff = NULL;
    char *params = NULL;

    if (!token || !cmd_name) {
        return NULL;
    }

    create_audioplayer_params(&params, token, pos, duration, err_num);
    if (params) {
        Create_Cmd(buff, cmd_id, NAMESPACE_AUDIOPLAYER, cmd_name, params);
        free(params);
    }

    return buff;
}

char *create_playback_cmd(int cmd_id, char *cmd_name)
{
    char *buff = NULL;

    if (!cmd_name) {
        return NULL;
    }

    Create_Cmd(buff, cmd_id, NAMESPACE_PLAYBACKCTROLLER, cmd_name, Val_Obj(0));

    return buff;
}

/*
 * Create system Cmd
 * Input :
 *         @cmd_name:
 *         @token:
 *
 * Output: int
 */
char *create_speech_cmd(int cmd_id, char *cmd_name, char *token)
{
    char *buff = NULL;
    char *params = NULL;

    if (!cmd_name) {
        return NULL;
    }

    create_string_params(&params, KEY_token, token);
    if (params) {
        Create_Cmd(buff, cmd_id, NAMESPACE_SPEECHSYNTHESIZER, cmd_name, params);
        free(params);
    }

    return buff;
}

/*
 * Create speaker Cmd
 * Input :
 *         @cmd_name:
 *         @volume:
 *         @muted:
 *
 * Output: int
 */
char *create_speeker_cmd(int cmd_id, char *cmd_name, int volume, int muted)
{
    char *buff = NULL;
    char *params = NULL;

    if (!cmd_name) {
        return NULL;
    }

    Create_Speeker_Params(params, volume, muted);
    if (params) {
        Create_Cmd(buff, cmd_id, NAMESPACE_SPEAKER, cmd_name, params);
        free(params);
    }

    return buff;
}

/*
 * Create alerts Cmd
 * Input :
 *         @cmd_name:
 *         @token:
 *
 * Output: int
 */
char *create_alerts_cmd(int cmd_id, char *cmd_name, const char *token)
{
    char *buff = NULL;
    char *params = NULL;

    if (!cmd_name || !token) {
        return NULL;
    }
    /*modify by weiqiang.huang for AVS alert V1.3 -- begin*/
    if (StrEq(cmd_name, NAME_DELETE_ALERTS_SUCCEEDED) ||
        StrEq(cmd_name, NAME_DELETE_ALERTS_FAILED) || StrEq(cmd_name, NAME_ALERT_VOLUME_CHANGED)) {
        params = strdup(token);
    } else {
        create_string_params(&params, KEY_token, token);
    }
    /*modify by weiqiang.huang for AVS alert V1.3 -- end*/
    if (params) {
        Create_Cmd(buff, cmd_id, NAMESPACE_ALERTS, cmd_name, params);
        free(params);
    }

    return buff;
}
char *create_donotdisturb_cmd(int cmd_id, char *cmd_name, bool enabled)
{
    char *buff = NULL;
    char *params = NULL;

    if (!cmd_name) {
        return NULL;
    }

    create_boolean_params(&params, KEY_enabled, enabled);
    if (params) {
        Create_Cmd(buff, cmd_id, NAMESPACE_DONOTDISTURB, cmd_name, params);
        free(params);
    }

    return buff;
}

/*
 * Create system Cmd
 * Input :
 *         @cmd_name:
 *         @token:
 *
 * Output: int
 */
char *create_system_cmd(int cmd_id, char *cmd_name, long inactive_ts, char *err_msg)
{
    char *buff = NULL;
    char *params = NULL;

    if (!cmd_name) {
        return NULL;
    }

    create_system_params(&params, inactive_ts, Val_Obj(err_msg));
    if (params) {
        Create_Cmd(buff, cmd_id, NAMESPACE_SYSTEM, cmd_name, params);
        free(params);
    }

    return buff;
}

/*
 * {
 *      "language":%s,
 * }
 */
char *create_settings_cmd(int cmd_id, char *cmd_name, char *language)
{
    char *buff = NULL;
    char *params = NULL;

    if (!language) {
        return NULL;
    }

    create_string_params(&params, KEY_language, language);
    if (params) {
        Create_Cmd(buff, cmd_id, NAMESPACE_SETTINGS, cmd_name, params);
        free(params);
    }

    return buff;
}

int directive_check_namespace(char *js_str, char *name_space)
{
    int ret = 0;

    if (js_str) {
        json_object *js_obj = json_tokener_parse(js_str);
        if (js_obj) {
            json_object *json_header = json_object_object_get(js_obj, KEY_HEADER);
            if (json_header) {
                char *_name_sapce = JSON_GET_STRING_FROM_OBJECT(json_header, KEY_NAMESPACE);
                if (StrEq(_name_sapce, name_space)) {
                    ret = 1;
                }
            }
            json_object_put(js_obj);
        }
    }

    return ret;
}
