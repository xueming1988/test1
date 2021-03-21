/*

*/

#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#include "json.h"

#include "asr_cmd.h"
#include "asr_debug.h"
#include "asr_session.h"
#include "asr_json_common.h"

#include "asr_alert.h"
#include "asr_player.h"
#include "asr_notification.h"
#include "asr_result_parse.h"

#include "clova_playback_controller.h"
#include "clova_device_control.h"
#include "clova_context.h"
#include "clova_clova.h"
#include "clova_settings.h"

extern int g_keeprecording;

static int clova_system_parse_directive(json_object *js_obj)
{
    int ret = -1;
    char *name = NULL;
    char *name_space = NULL;
    char *dialog_id = NULL;
    json_object *js_header = NULL;
    json_object *js_payload = NULL;

    if (js_obj) {
        js_header = json_object_object_get(js_obj, KEY_HEADER);
        if (js_header) {
            name_space = JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            name = JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            dialog_id = JSON_GET_STRING_FROM_OBJECT(js_header, KEY_DIALOGREQUESTID);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
        }

        js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            ASR_PRINT(ASR_DEBUG_ERR, "%s\n", json_object_to_json_string(js_payload));
            if (StrEq(name, "Exception")) {
                int code = JSON_GET_INT_FROM_OBJECT(js_payload, "code");
                if (code == 412) {
                    asr_session_disconncet(ASR_CLOVA);
                } else if (code == 400) {
                    // do nothing
                } else {
                    ASR_PRINT(ASR_DEBUG_ERR, "no method to deal with error code %d\n", code);
                }
            }
        }
    }

    return ret;
}

static int asr_parse_set_alert(json_object *js_obj)
{
    int ret = -1;

    if (js_obj) {
        char *json_string = json_object_to_json_string(js_obj);
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            char *token = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_TOKEN);
            char *type = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_TYPE);
            char *time = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_SCHEDULEDTIME);
            json_object *temp1 = json_object_object_get(js_payload, PAYLOAD_Assets);

            if (token && type && time) {
                if (temp1) {
                    ret = alert_set_with_json(token, type, time, json_string);
                } else {
                    ret = alert_set(token, type, time);
                }
                if (ret == 0) {
                    clova_alerts_general_cmd(NAME_SETALERT_SUCCEEDED, token, type);
                } else {
                    clova_alerts_general_cmd(NAME_SETALERT_FAILED, token, type);
                }
            }
        }
    }

    return ret;
}

static int asr_parse_del_alert(json_object *js_obj)
{
    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            char *token = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_TOKEN);
            char *type = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_TYPE);
            if (token && type) {
                int ret = alert_stop(token);
                if (ret == 0) {
                    clova_alerts_general_cmd(NAME_DELETE_ALERT_SUCCEEDED, token, type);
                } else {
                    clova_alerts_general_cmd(NAME_DELETE_ALERT_FAILED, token, type);
                }
            }
        }

        return 0;
    }

    return -1;
}

static int asr_parse_stop_alert(json_object *js_obj)
{
    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            char *token = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_TOKEN);
            char *type = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_TYPE);
            if (token && type) {
                alert_stop(token);
                clova_alerts_general_cmd(NAME_ALERT_STOPPED, token, type);
            }
        }

        return 0;
    }

    return -1;
}

static int asr_parse_sync_alert(json_object *js_obj)
{
    if (js_obj) {
        char *json_string = NULL;
        json_object *json_body = NULL;

        json_body = json_object_new_object();

        json_object *js_header = json_object_object_get(js_obj, KEY_HEADER);
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (json_body && js_header && js_payload) {
            js_header = json_object_object_dup(js_header);
            json_object_object_add(json_body, KEY_HEADER, js_header);

            json_object *array = json_object_object_get(js_payload, PAYLOAD_ALLALERTS);
            int num = json_object_array_length(array);
            int i;

            for (i = 0; i < num; i++) {
                json_object *js_array = json_object_array_get_idx(array, i);
                char *token = JSON_GET_STRING_FROM_OBJECT(js_array, PAYLOAD_TOKEN);
                char *type = JSON_GET_STRING_FROM_OBJECT(js_array, PAYLOAD_TYPE);
                char *time = JSON_GET_STRING_FROM_OBJECT(js_array, PAYLOAD_SCHEDULEDTIME);
                json_object *temp1 = json_object_object_get(js_array, PAYLOAD_Assets);

                js_array = json_object_object_dup(js_array);
                json_object_object_add(json_body, KEY_PAYLOAD, js_array);
                json_string = json_object_to_json_string(json_body);

                if (token && type && time) {
                    int ret = 0;
                    if (temp1) {
                        ret = alert_set_with_json(token, type, time, json_string);
                    } else {
                        ret = alert_set(token, type, time);
                    }
                }

                json_object_object_del(json_body, KEY_PAYLOAD);
            }

            json_object_object_del(json_body, KEY_HEADER);
        }
        if (json_body) {
            json_object_put(json_body);
        }

        return 0;
    }

    return -1;
}

static int asr_parse_clear_alert(json_object *js_obj)
{
    int ret;

    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            char *type = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_TYPE);
            if (type) {
                ret = alert_clear(type);
                if (ret == 0) {
                    clova_alerts_general_cmd(NAME_CLEAR_ALERT_SUCCEEDED, NULL, type);
                } else {
                    clova_alerts_general_cmd(NAME_CLEAR_ALERT_FAILED, NULL, type);
                }
            }
        }

        return 0;
    }

    return -1;
}

static int asr_parse_stop_capture(json_object *js_obj)
{
    if (js_obj) {
        NOTIFY_CMD(ASR_TTS, VAD_FINISHED);

        return 0;
    }

    return -1;
}

static int asr_parse_keep_recording(json_object *js_obj)
{
    if (js_obj) {
        g_keeprecording = 1;

        return 0;
    }

    return -1;
}

static int asr_parse_confirm_wakeup(json_object *js_obj)
{
    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            int success = JSON_GET_INT_FROM_OBJECT(js_payload, PAYLOAD_SUCCESS);
            if (!success) {
                NOTIFY_CMD(ASR_TTS, VAD_FINISHED);
            }
        }

        return 0;
    }

    return -1;
}

static int asr_parse_text_show(json_object *js_obj)
{
    if (js_obj) {
        return 0;
    }

    return -1;
}

int asr_result_json_parse(char *message, int down_cmd)
{
    json_object *json_message = NULL;

    json_message = json_tokener_parse(message);
    if (json_message) {
        json_object *json_directive = json_object_object_get(json_message, KEY_DIRECTIVE);
        if (json_directive) {
            json_object *json_header = json_object_object_get(json_directive, KEY_HEADER);
            if (json_header) {
                char *name_space = JSON_GET_STRING_FROM_OBJECT(json_header, KEY_NAMESPACE);
                char *name = JSON_GET_STRING_FROM_OBJECT(json_header, KEY_NAME);
                if (name && name_space) {
#if defined(UPLUS_DEBUG_LOG)
                    ASR_PRINT(ASR_DEBUG_TRACE, "[CLOVA (%s-%s)] %s\n", name_space, name, message);
#else
                    ASR_PRINT(ASR_DEBUG_TRACE, "[CLOVA (%s-%s)]\n", name_space, name);
#endif
                    if (StrEq(name_space, NAMESPACE_NOTIFIER)) {
                        asr_notificaion_parse_directive(json_directive);
                    } else if (StrEq(name_space, NAMESPACE_AUDIOPLAYER) ||
                               StrEq(name_space, NAMESPACE_SPEECHSYNTHESIZER) ||
                               StrEq(name_space, NAMESPACE_SPEAKER) ||
                               StrEq(name, NAME_EXPECTSPEECH)) {
                        asr_player_parse_directvie(json_directive, down_cmd);
                    } else if (StrEq(name_space, NAMESPACE_SYSTEM)) {
                        clova_system_parse_directive(json_directive);
                    } else if (StrEq(name_space, CLOVA_HEADER_NAMESPACE_PLAYBACK_CONTROLLER)) {
                        clova_parse_playback_controller_directive(json_directive);
                    } else if (StrEq(name_space, CLOVA_HEADER_NAMESPACE_DEVICE_CONTROL)) {
                        clova_parse_device_control_directive(json_directive);
                    } else if (StrEq(name_space, CLOVA_HEADER_CLOVA_NAMESPACE)) {
                        clova_parse_clova_directive(json_directive);
                    } else if (StrEq(name_space, CLOVA_HEADER_NAMESPACE_SETTINGS)) {
                        clova_parse_settings_directive(json_directive);
                    } else if (StrEq(name, NAME_SETALERT)) {
                        asr_parse_set_alert(json_directive);
                    } else if (StrEq(name, NAME_DELETE_ALERT)) {
                        asr_parse_del_alert(json_directive);
                    } else if (StrEq(name, NAME_STOP_ALERT)) {
                        asr_parse_stop_alert(json_directive);
                    } else if (StrEq(name, NAME_ALERT_SYNC)) {
                        asr_parse_sync_alert(json_directive);
                    } else if (StrEq(name, NAME_CLEAR_ALERT)) {
                        asr_parse_clear_alert(json_directive);
                    } else if (StrEq(name, NAME_STOPCAPTURE)) {
                        asr_parse_stop_capture(json_directive);
                    } else if (StrEq(name, NAME_KEEPRECORDING)) {
                        asr_parse_keep_recording(json_directive);
                    } else if (StrEq(name, NAME_CONFIRMWAKEUP)) {
                        asr_parse_confirm_wakeup(json_directive);
                    } else if (StrEq(name, NAME_SHOWRECOGNIZEDTEXT)) {
                        asr_parse_text_show(json_directive);
                    } else {
                        ASR_PRINT(ASR_DEBUG_ERR, "unknown directive (%s:%s)\n", name_space, name);
                    }
                }
            }
        }

        json_object_put(json_message);
        return 0;
    }

    ASR_PRINT(ASR_DEBUG_ERR, "prase message error:\n{\n%s\n}\n", message);

    return -1;
}

int asr_unauthor_json_parse(char *message)
{
    int ret = -1;
    json_object *json_message = NULL;
    ASR_PRINT(ASR_DEBUG_INFO, "asr_unauthor_json_parse ++++\n");

    json_message = json_tokener_parse(message);
    if (json_message) {
        json_object *js_payload = json_object_object_get(json_message, KEY_PAYLOAD);
        if (js_payload) {
            char *res = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_CODE);
            if (res) {
                ASR_PRINT(ASR_DEBUG_INFO, "asr_unauthor_json_parse:%s\n", res);
                if (StrInclude(res, PAYLOAD_UNAUTHORIZED_REQUEST_EXCEPTION)) {
                    ASR_PRINT(ASR_DEBUG_INFO, "into this function++++\n");
                    char recv_buff[128] = {0};
                    int len = 0;
                    SocketClientReadWriteMsg(ASR_EVENT_PIPE, ASR_LOGOUT, strlen(ASR_LOGOUT),
                                             recv_buff, &len, 0);
                    ret = 0;
                }
            }
        } else {
            char *res = JSON_GET_STRING_FROM_OBJECT(json_message, PAYLOAD_ERROR);
            if (res) {
                ASR_PRINT(ASR_DEBUG_INFO, "asr_unauthor_json_parse %s:\n", res);
                if (StrInclude(res, ERROR_INVALID_GRANT) ||
                    StrInclude(res, ERROR_UNAUTHORIZED_CLIENT)) {
                    char recv_buff[128] = {0};
                    int len = 0;
                    SocketClientReadWriteMsg(ASR_EVENT_PIPE, ASR_LOGOUT, strlen(ASR_LOGOUT),
                                             recv_buff, &len, 0);
                    ret = 0;
                }
            }
        }
        json_object_put(json_message);
    }

    ASR_PRINT(ASR_DEBUG_INFO, "asr_unauthor_json_parse ----\n");
    return ret;
}
