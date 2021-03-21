
#include <stdio.h>
#include <string.h>

#include "asr_cmd.h"
#include "asr_json_common.h"

#include "clova_speech_synthesizer.h"
#include "clova_audio_player.h"
#include "clova_playback_controller.h"
#include "clova_settings.h"
#include "clova_device_control.h"

json_object *json_object_object_dup(json_object *src_js)
{
    json_object *obj_dup = NULL;
    char *str_str = NULL;

    if (src_js) {
        str_str = (char *)json_object_to_json_string(src_js);
        if (str_str) {
            obj_dup = json_tokener_parse(str_str);
        }
    }

    return obj_dup;
}

static int create_audio_general_params(char **buff, char *stream_id, long off_set)
{
    json_object *cmd_params = NULL;

    cmd_params = json_object_new_object();
    if (cmd_params) {
        json_object_object_add(cmd_params, KEY_stream_id, JSON_OBJECT_NEW_STRING(stream_id));

        json_object *offset_js = json_object_new_int64(off_set);
        if (offset_js) {
            json_object_object_add(cmd_params, KEY_offset, offset_js);
        }

        char *cmd_params_str = (char *)json_object_to_json_string(cmd_params);
        if (cmd_params_str) {
            *buff = strdup(cmd_params_str);
        }

        json_object_put(cmd_params);
    }

    return -1;
}

int create_audio_report_state_params(char **buff, long long duration, long play_offset_ms,
                                     char *play_state, char *repeat_mode, json_object *stream,
                                     char *token)
{
    int ret = -1;
    json_object *cmd_params = NULL;

    cmd_params = json_object_new_object();
    if (cmd_params) {
        json_object_object_add(cmd_params, CLOVA_PAYLOAD_NAME_PLAYER_ACTIVITY,
                               JSON_OBJECT_NEW_STRING(play_state));

        json_object_object_add(cmd_params, CLOVA_PAYLOAD_NAME_REPEATMODE,
                               JSON_OBJECT_NEW_STRING(repeat_mode));

        if (!StrEq(play_state, PLAYER_IDLE)) {
            json_object_object_add(cmd_params, CLOVA_PAYLOAD_NAME_OFFSET_IN_MILLISECONDS,
                                   json_object_new_int64(play_offset_ms));
            if (stream) {
                json_object *stream_js = json_object_object_dup(stream);
                if (stream_js) {
                    json_object_object_add(cmd_params, CLOVA_PAYLOAD_NAME_STREAM, stream_js);
                }
            } else {
                printf("the stream is NULL\n");
            }
            json_object_object_add(cmd_params, CLOVA_PAYLOAD_NAME_TOKEN,
                                   JSON_OBJECT_NEW_STRING(token));

            json_object_object_add(cmd_params, CLOVA_PAYLOAD_NAME_TOTAL_IN_MILLISECONDES,
                                   json_object_new_int64(duration));
        }

        char *cmd_params_str = (char *)json_object_to_json_string(cmd_params);
        if (cmd_params_str) {
            *buff = strdup(cmd_params_str);
        }

        json_object_put(cmd_params);
        ret = 0;
    }

    return ret;
}

static int create_stream_request_params(char **buff, char *audioItemId, json_object *audioStream)
{
    int ret = -1;
    json_object *cmd_params = NULL;

    cmd_params = json_object_new_object();
    if (cmd_params) {
        json_object_object_add(cmd_params, CLOVA_PAYLOAD_NAME_AUDIO_ITEM_ID,
                               JSON_OBJECT_NEW_STRING(audioItemId));

        if (audioStream) {
            json_object *stream_js = json_object_object_dup(audioStream);
            if (stream_js) {
                json_object_object_add(cmd_params, CLOVA_PAYLOAD_NAME_AUDIO_STREAM, stream_js);
            }
        }
        char *cmd_params_str = (char *)json_object_to_json_string(cmd_params);
        if (cmd_params_str) {
            *buff = strdup(cmd_params_str);
        }

        json_object_put(cmd_params);
        ret = 0;
    }

    return ret;
}

int create_playback_general_params(char **buff, char *device_id)
{
    json_object *cmd_params = NULL;

    cmd_params = json_object_new_object();
    if (cmd_params) {
        if (device_id) {
            json_object_object_add(cmd_params, CLOVA_PAYLOAD_NAME_DEVICE_ID,
                                   JSON_OBJECT_NEW_STRING(device_id));
        }

        char *cmd_params_str = (char *)json_object_to_json_string(cmd_params);
        if (cmd_params_str) {
            *buff = strdup(cmd_params_str);
        }

        json_object_put(cmd_params);
    }

    return -1;
}

int create_settings_request_sync_params(char **buff, char *deviceId, int syncWithStorage)
{
    int ret = -1;
    json_object *cmd_params = NULL;

    cmd_params = json_object_new_object();
    if (cmd_params) {
        if (deviceId) {
            json_object_object_add(cmd_params, CLOVA_PAYLOAD_NAME_DEVICE_ID,
                                   JSON_OBJECT_NEW_STRING(deviceId));
        }

        json_object_object_add(cmd_params, CLOVA_PAYLOAD_NAME_SYNC_WITH_STORAGE,
                               json_object_new_boolean(syncWithStorage));

        char *cmd_params_str = (char *)json_object_to_json_string(cmd_params);
        if (cmd_params_str) {
            *buff = strdup(cmd_params_str);
        }

        json_object_put(cmd_params);
        ret = 0;
    }

    return ret;
}

int create_speech_general_params(char **buff, char *token)
{
    int ret = -1;
    json_object *cmd_params = NULL;

    cmd_params = json_object_new_object();
    if (cmd_params) {
        json_object_object_add(cmd_params, CLOVA_PAYLOAD_NAME_TOKEN, JSON_OBJECT_NEW_STRING(token));

        char *cmd_params_str = (char *)json_object_to_json_string(cmd_params);
        if (cmd_params_str) {
            *buff = strdup(cmd_params_str);
        }

        json_object_put(cmd_params);
        ret = 0;
    }

    return ret;
}

int create_speech_request_params(char **buff, const char *text, char *lang)
{
    int ret = -1;
    json_object *cmd_params = NULL;

    if (text && lang) {
        cmd_params = json_object_new_object();
        if (cmd_params) {
            json_object_object_add(cmd_params, CLOVA_PAYLOAD_NAME_TEXT,
                                   JSON_OBJECT_NEW_STRING(text));

            json_object_object_add(cmd_params, CLOVA_PAYLOAD_NAME_LANG,
                                   JSON_OBJECT_NEW_STRING(lang));

            char *cmd_params_str = (char *)json_object_to_json_string(cmd_params);
            if (cmd_params_str) {
                *buff = strdup(cmd_params_str);
            }

            json_object_put(cmd_params);
            ret = 0;
        }
    }

    return ret;
}

int create_alerts_general_params(char **buff, char *token, char *type)
{
    int ret = -1;
    json_object *cmd_params = NULL;

    cmd_params = json_object_new_object();
    if (cmd_params) {
        if (token)
            json_object_object_add(cmd_params, PAYLOAD_TOKEN, JSON_OBJECT_NEW_STRING(token));

        json_object_object_add(cmd_params, PAYLOAD_TYPE, JSON_OBJECT_NEW_STRING(type));

        char *cmd_params_str = (char *)json_object_to_json_string(cmd_params);
        if (cmd_params_str) {
            *buff = strdup(cmd_params_str);
        }

        json_object_put(cmd_params);
        ret = 0;
    }

    return ret;
}

int create_device_control_action_params(char **buff, char *target, char *command)
{
    int ret = -1;
    json_object *cmd_params = NULL;

    cmd_params = json_object_new_object();
    if (cmd_params) {
        json_object_object_add(cmd_params, CLOVA_PAYLOAD_NAME_TARGET,
                               JSON_OBJECT_NEW_STRING(target));

        json_object_object_add(cmd_params, CLOVA_PAYLOAD_NAME_COMMAND,
                               JSON_OBJECT_NEW_STRING(command));

        char *cmd_params_str = (char *)json_object_to_json_string(cmd_params);
        if (cmd_params_str) {
            *buff = strdup(cmd_params_str);
        }

        json_object_put(cmd_params);
        ret = 0;
    }

    return ret;
}

int create_empty_params(char **buff)
{
    int ret = -1;
    json_object *cmd_params = NULL;

    cmd_params = json_object_new_object();
    if (cmd_params) {
        char *cmd_params_str = (char *)json_object_to_json_string(cmd_params);
        if (cmd_params_str) {
            *buff = strdup(cmd_params_str);
        }

        json_object_put(cmd_params);
        ret = 0;
    }

    return ret;
}

int create_asr_state(char **buff, int request_type, long long duration, long offet,
                     char *play_state, char *repeat_mode, json_object *stream, int vol, int muted,
                     char *speech_token, long offet1, char *play_state1)
{
    int ret = -1;
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
            json_object_object_add(play_back_js, KEY_playerActivity,
                                   JSON_OBJECT_NEW_STRING(play_state));

            if (!StrEq(play_state, PLAYER_IDLE)) {
                json_object *offet_js = json_object_new_int64(offet);
                if (offet_js) {
                    json_object_object_add(play_back_js, KEY_offsetInMilliseconds, offet_js);
                }
                if (stream) {
                    json_object *stream_js = json_object_object_dup(stream);
                    if (stream_js) {
                        json_object_object_add(play_back_js, CLOVA_PAYLOAD_NAME_STREAM, stream_js);
                    }
                } else {
                    printf("the stream is NULL\n");
                }
                json_object *duration_js = json_object_new_int64(duration);
                if (duration_js) {
                    json_object_object_add(play_back_js, "totalInMilliseconds", duration_js);
                }
            }

            json_object_object_add(play_back_js, "repeatMode", JSON_OBJECT_NEW_STRING(repeat_mode));

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
#if 0
                json_object *offet1_js = json_object_new_int64(offet1);
                if(offet1_js) {
                    json_object_object_add(speech_js, KEY_offsetInMilliseconds, offet1_js);
                }
#endif
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
        ret = 0;
    }

    return ret;
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
char *create_audio_general_cmd(char *cmd_name, char *token, long pos)
{
    char *buff = NULL;
    char *params = NULL;

    if (!token || !cmd_name) {
        return NULL;
    }

    create_audio_general_params(&params, token, pos);
    if (params) {
        Create_Cmd(buff, NAMESPACE_AUDIOPLAYER, cmd_name, params);
        free(params);
    }

    return buff;
}

char *create_audio_report_state_cmd(char *cmd_name)
{
    char *buff = NULL;
    char *params = NULL;

    asr_player_get_audio_info(&params);

    if (params) {
        Create_Cmd(buff, NAMESPACE_AUDIOPLAYER, cmd_name, params);
        free(params);
    }

    return buff;
}

char *create_stream_request_cmd(char cmd_name, char *audioItemId, json_object *audioStream)
{
    char *buff = NULL;
    char *params = NULL;

    if (!audioItemId || !audioStream) {
        return NULL;
    }

    create_stream_request_params(&params, audioItemId, audioStream);
    if (params) {
        Create_Cmd(buff, NAMESPACE_AUDIOPLAYER, cmd_name, params);
        free(params);
    }

    return buff;
}

char *create_playback_general_cmd(char *cmd_name, char *device_id)
{
    char *buff = NULL;
    char *params = NULL;

    if (!cmd_name) {
        return NULL;
    }

    create_playback_general_params(&params, device_id);
    if (params) {
        Create_Cmd(buff, NAMESPACE_PLAYBACKCTROLLER, cmd_name, params);
        free(params);
    }

    return buff;
}

char *create_settings_request_sync_cmd(char *cmd_name, char *deviceId, int syncWithStorage)
{
    char *buff = NULL;
    char *params = NULL;

    if (!cmd_name) {
        return NULL;
    }

    create_settings_request_sync_params(&params, deviceId, syncWithStorage);
    if (params) {
        Create_Cmd(buff, CLOVA_HEADER_NAMESPACE_SETTINGS, cmd_name, params);
        free(params);
    }

    return buff;
}

char *create_settings_report_cmd(char *cmd_name)
{
    char *buff = NULL;
    char *params = NULL;

    if (!cmd_name) {
        return NULL;
    }

    create_empty_params(&params);
    if (params) {
        Create_Cmd(buff, CLOVA_HEADER_NAMESPACE_SETTINGS, cmd_name, params);
        free(params);
    }

    return buff;
}

char *create_settings_request_store_cmd(char *cmd_name)
{
    char *buff = NULL;
    char *params = NULL;

    if (!cmd_name) {
        return NULL;
    }

    create_empty_params(&params);
    if (params) {
        Create_Cmd(buff, CLOVA_HEADER_NAMESPACE_SETTINGS, cmd_name, params);
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
char *create_speech_general_cmd(char *cmd_name, char *token)
{
    char *buff = NULL;
    char *params = NULL;

    if (!cmd_name) {
        return NULL;
    }

    create_speech_general_params(&params, token);
    if (params) {
        Create_Cmd(buff, NAMESPACE_SPEECHSYNTHESIZER, cmd_name, params);
        free(params);
    }

    return buff;
}

char *create_speech_request_cmd(char *text, char *lang)
{
    char *buff = NULL;
    char *params = NULL;

    if (!text || !lang) {
        return NULL;
    }

    create_speech_request_params(&params, text, lang);
    if (params) {
        Create_Cmd(buff, CLOVA_HEADER_NAMESPACE_SPEECH_SYNTHESIZER, CLOVA_HEADER_NAME_REQUEST,
                   params);
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
char *create_alerts_general_cmd(char *cmd_name, char *token, char *type)
{
    int ret = -1;
    char *buff = NULL;
    char *params = NULL;

    if (!cmd_name || !type) {
        return NULL;
    }

    create_alerts_general_params(&params, token, type);
    if (params) {
        Create_Cmd(buff, NAMESPACE_ALERTS, cmd_name, params);
        free(params);
    }

    return buff;
}

char *create_alerts_request_sync_cmd(char *cmd_name)
{
    int ret = -1;
    char *buff = NULL;
    char *params = NULL;

    if (!cmd_name) {
        return NULL;
    }

    create_empty_params(&params);
    if (params) {
        Create_Cmd(buff, NAMESPACE_ALERTS, cmd_name, params);
        free(params);
    }

    return buff;
}

char *create_device_control_action_cmd(char *cmd_name, char *target, char *command)
{
    int ret = -1;
    char *buff = NULL;
    char *params = NULL;

    if (!cmd_name || !target || !command) {
        return NULL;
    }

    create_device_control_action_params(&params, target, command);
    if (params) {
        Create_Cmd(buff, CLOVA_HEADER_NAMESPACE_DEVICE_CONTROL, cmd_name, params);
        free(params);
    }

    return buff;
}

char *create_device_control_report_cmd(char *cmd_name)
{
    int ret = -1;
    char *buff = NULL;
    char *params = NULL;

    if (!cmd_name) {
        return NULL;
    }

    create_empty_params(&params);
    if (params) {
        if (StrEq(cmd_name, CLOVA_HEADER_NAME_REPORT_STATE)) {
            asr_queue_clear_report_state();
        }
        Create_Cmd(buff, CLOVA_HEADER_NAMESPACE_DEVICE_CONTROL, cmd_name, params);
        free(params);
    }

    return buff;
}

char *create_cmd(const char *name_space, const char *name, const char *payload)
{
    char *cmd = NULL;

    if (name_space && name && payload) {
        Create_Cmd(cmd, name_space, name, payload);
    }

    return cmd;
}
