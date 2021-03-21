
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "wm_util.h"
#include "ccaptureGeneral.h"

#include "asr_debug.h"
#include "asr_request_json.h"
#include "asr_json_common.h"

#include "asr_notification.h"

#include "clova_event.h"
#include "clova_context.h"
#include "clova_audio_player.h"
#include "clova_device_control.h"
#include "clova_playback_controller.h"
#include "clova_speech_synthesizer.h"
#include "clova_settings.h"
#include "clova_clova.h"
#include "clova_update.h"
#if defined(LGUPLUS_IPTV_ENABLE)
#include "iptv_msg.h"
#endif

#ifndef RAND_STR_LEN
#define RAND_STR_LEN (36)
#endif

#define Debug_func_line printf("------(%s:%d)-------\n", __FUNCTION__, __LINE__)

#define update_key_val(json_data, key, set_val)                                                    \
    do {                                                                                           \
        json_object_object_del((json_data), (key));                                                \
        json_object_object_add((json_data), (key), (set_val));                                     \
    } while (0)

static char g_recg_dialog_id[RAND_STR_LEN + 1] = {0};
static pthread_mutex_t g_state_lock = PTHREAD_MUTEX_INITIALIZER;

/*"21abbca2-4db3-461a-bd77-2ab0251e84f5"*/
static char *rand_string_36(char *str)
{
    int i = 0;

    if (!str) {
        return NULL;
    }

    srand((unsigned int)NsSincePowerOn());

    for (i = 0; i < RAND_STR_LEN; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            str[i] = '-';
        } else {
            if (rand() % 2) {
                str[i] = (char)(rand() % 9 + '0');
            } else {
                str[i] = (char)(rand() % ('z' - 'a' + 1) + 'a');
            }
        }
    }

    str[RAND_STR_LEN] = '\0';

    return str;
}

/*struct json_object* json_object_new_array(void);*/
/*
 * Add an alert to the alert list
 * Input :
 *         @alert_list: alert list
 *         @token: alert token
 *         @type: alert type
 *         @scheduled_time: alert scheduled_time
 *
 * Output:
 *         @0: ok
 *         @other: error
 */
int asr_alerts_list_add(json_object *alert_list, char *token, char *type, char *scheduled_time)
{
    int ret = -1;

    if (alert_list) {
        json_object *val = json_object_new_object();
        if (val) {
            json_object_object_add(val, PAYLOAD_TOKEN, JSON_OBJECT_NEW_STRING(token));
            json_object_object_add(val, PAYLOAD_TYPE, JSON_OBJECT_NEW_STRING(type));
            json_object_object_add(val, PAYLOAD_SCHEDULEDTIME,
                                   JSON_OBJECT_NEW_STRING(scheduled_time));
            ret = json_object_array_add(alert_list, val);
            return ret;
        } else {
            ASR_PRINT(ASR_DEBUG_ERR, "json_object_new_object failed\n");
        }
    }

    ASR_PRINT(ASR_DEBUG_ERR, "input error failed\n");

    return -1;
}

/*
"payload": {
    "allAlerts": [{
        "token": "{{STRING}}",
        "type": "{{STRING}}",
        "scheduledTime": "{{STRING}}"
    }],
    "activeAlerts": [{
        "token": "{{STRING}}",
        "type": "{{STRING}}",
        "scheduledTime": "{{STRING}}"
    }]
}
*/
/*
 * Create an Alert paylod include all alerts and active alerts
 * Input :
 *         @all_alerts_list: all alert lists
 *         @active_alerts_list: the active alert list
 *
 * Output: an json
 */
json_object *asr_alerts_payload(json_object *all_alerts_list, json_object *active_alerts_list)
{
    json_object *obj_payload = json_object_new_object();
    if (obj_payload) {
        json_object_object_add(obj_payload, PAYLOAD_ALLALERTS,
                               all_alerts_list ? all_alerts_list : json_object_new_array());
        json_object_object_add(obj_payload, PAYLOAD_ACTIVEALERTS,
                               active_alerts_list ? active_alerts_list : json_object_new_array());
        return obj_payload;
    }
    ASR_PRINT(ASR_DEBUG_ERR, "json_object_new_object failed\n");

    return NULL;
}

/*
"header": {
    "namespace": "SpeechRecognizer",
    "name": "Recognize",
    "messageId": "{{STRING}}",
    "dialogRequestId": "{{STRING}}"
},
*/
/*
 * Create event header
 * Input :
 *         @type: the type of event
 *         @name: the name of event
 *         @message_id: the message_id of event
 *         @dialog_request_id: the dialog_request_id of event
 *
 * Output: an json
 */
static json_object *asr_event_header(char *name_space, char *name, int req_from)
{
    json_object *obj_header = NULL;
    char message_id[RAND_STR_LEN + 1] = {0};
    char dialog_id[RAND_STR_LEN + 1] = {0};

    if (!name_space || !name) {
        ASR_PRINT(ASR_DEBUG_ERR, "input error\n");
        return NULL;
    }

    obj_header = json_object_new_object();
    if (obj_header) {
        json_object_object_add(obj_header, KEY_NAMESPACE, JSON_OBJECT_NEW_STRING(name_space));
        json_object_object_add(obj_header, KEY_NAME, JSON_OBJECT_NEW_STRING(name));
        json_object_object_add(obj_header, KEY_MESSAGEID,
                               JSON_OBJECT_NEW_STRING(rand_string_36(message_id)));
        if (StrEq(name, NAME_RECOGNIZE)) {
            if (req_from == 2) {
                pthread_mutex_lock(&g_state_lock);
                strncpy(dialog_id, g_recg_dialog_id, sizeof(g_recg_dialog_id) - 1);
                pthread_mutex_unlock(&g_state_lock);
                if (strlen(dialog_id) > 0) {
                    json_object_object_add(obj_header, KEY_DIALOGREQUESTID,
                                           JSON_OBJECT_NEW_STRING(dialog_id));
                } else {
                    json_object_object_add(obj_header, KEY_DIALOGREQUESTID,
                                           JSON_OBJECT_NEW_STRING(rand_string_36(dialog_id)));
                }
            } else {
                json_object_object_add(obj_header, KEY_DIALOGREQUESTID,
                                       JSON_OBJECT_NEW_STRING(rand_string_36(dialog_id)));
                pthread_mutex_lock(&g_state_lock);
                memset(g_recg_dialog_id, 0, sizeof(g_recg_dialog_id));
                strncpy(g_recg_dialog_id, dialog_id, sizeof(g_recg_dialog_id) - 1);
                pthread_mutex_unlock(&g_state_lock);
            }
        }

        return obj_header;
    }

    ASR_PRINT(ASR_DEBUG_ERR, "json_object_new_object failed\n");

    return NULL;
}

/*
"header": {
    "namespace": ".......",
    "name": "........"
}
"payload":{
}
*/
/*
 * Create an Context
 * Input :
 *         @context_list: the context
 *         @name_space: the event namespace
 *         @name: the event name
 *         @payload: the event payload
 *
 * Output:
 *         @0: ok
 *         @other: error
 */
int asr_context_list_add(json_object *context_list, char *name_space, char *name,
                         json_object *payload)
{
    int ret = -1;
    if (context_list) {
        json_object *header = json_object_new_object();
        json_object *val = json_object_new_object();
        if (header && val) {
            json_object_object_add(header, KEY_NAMESPACE, JSON_OBJECT_NEW_STRING(name_space));
            json_object_object_add(header, KEY_NAME, JSON_OBJECT_NEW_STRING(name));
            json_object_object_add(val, KEY_HEADER, header);
            json_object_object_add(val, KEY_PAYLOAD, payload ? payload : json_object_new_object());
            ret = json_object_array_add(context_list, val);

            return ret;
        } else {
            if (header) {
                json_object_put(header);
            }
            if (val) {
                json_object_put(val);
            }
            ASR_PRINT(ASR_DEBUG_ERR, "json_object_new_object failed\n");
        }
    }

    ASR_PRINT(ASR_DEBUG_ERR, "input error failed\n");

    return -1;
}

/*
"context": {
},
"event":{
}
*/
/*
 *  Create an request Context json
 * Input :
 *         @context_list:  the all context
 *         @event:  the event
 *
 * Output: an json
 */
json_object *asr_context(json_object *context_list, json_object *event)
{
    json_object *obj = json_object_new_object();
    if (obj) {
        json_object_object_add(obj, KEY_CONTEXT,
                               context_list ? context_list : json_object_new_array());
        json_object_object_add(obj, KEY_EVENT, event ? event : json_object_new_object());

        return obj;
    }

    ASR_PRINT(ASR_DEBUG_ERR, "json_object_new_object failed\n");

    return NULL;
}

/*
  *  create an json about the event request body
  */
static json_object *asr_event_create(json_object *cmd_js, int req_type)
{
    json_object *json_event_header = NULL;
    json_object *json_event_payload = NULL;
    json_object *json_event = NULL;
    json_object *json_cur_state = NULL;
    json_object *json_error = NULL;
    json_object *json_event_payload1 = NULL;
    json_object *json_event_payload2 = NULL;
    json_object *json_event_payload3 = NULL;
    int event_type = -1;
    char *event_name = NULL;
    char *name_space = NULL;
    json_object *cmd_params = NULL;

    if (!cmd_js) {
        ASR_PRINT(ASR_DEBUG_ERR, "input error\n");
        return NULL;
    }

    cmd_params = json_object_object_get(cmd_js, KEY_params);

    event_name = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_NAME);
    name_space = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_NAMESPACE);
    json_event_header = asr_event_header(name_space, event_name, req_type);
    json_event_payload = json_object_new_object();

    if (StrEq(name_space, NAMESPACE_SPEECHRECOGNIZER)) {
        if (StrEq(event_name, NAME_RECOGNIZE)) {
            json_object_object_add(json_event_payload, PAYLOAD_PROFILE,
                                   JSON_OBJECT_NEW_STRING(PROFILE_CLOSE_TALK));

            json_object_object_add(json_event_payload, PAYLOAD_FORMAT,
                                   JSON_OBJECT_NEW_STRING(FORMAT_AUDIO_L16));

            int extension = clova_get_extension_state();
            if (extension) {
                json_object_object_add(json_event_payload, CLOVA_PAYLOAD_LANG_NAME,
                                       JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_LANG_VALUE_EN));
            } else {
                json_object_object_add(json_event_payload, CLOVA_PAYLOAD_LANG_NAME,
                                       JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_LANG_VALUE_KO));
            }

            if (req_type == 2) {
                int explicit = clova_get_explicit();
                json_object_object_add(json_event_payload, CLOVA_PAYLOAD_EXPLICIT_NAME,
                                       json_object_new_boolean(explicit));

                char *speechId = clova_get_speechId();
                json_object_object_add(json_event_payload, CLOVA_PAYLOAD_SPEECHID,
                                       JSON_OBJECT_NEW_STRING(speechId));
                free(speechId);
            }

            if (req_type == 1) {
                json_object *json_initiator = json_object_new_object();
                if (json_initiator) {
                    json_object_object_add(json_initiator, CLOVA_PAYLOAD_INPUT_SOURCE,
                                           JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_SELF));

                    json_object_object_add(json_initiator, CLOVA_PAYLOAD_TYPE,
                                           JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_WAKEWORD));

                    json_object *json_payload = json_object_new_object();
                    if (json_payload) {
                        char *uuid = get_device_id();
                        json_object_object_add(json_payload, CLOVA_PAYLOAD_DEVICE_UUID,
                                               JSON_OBJECT_NEW_STRING(uuid));

                        json_object *json_wakeWord = json_object_new_object();
                        if (json_wakeWord) {
                            json_object_object_add(json_wakeWord, CLOVA_PAYLOAD_CONFIDENCE,
                                                   json_object_new_double(0.812312));

                            int keyword_index = get_keyword_type();
                            char *keyword;
                            switch (keyword_index) {
                            case 0:
                                keyword = CLOVA_PAYLOAD_HEY_CLOVA;
                                break;
                            case 1:
                                keyword = CLOVA_PAYLOAD_CLOVA;
                                break;
                            case 2:
                                keyword = CLOVA_PAYLOAD_SELIYA;
                                break;
                            case 3:
                                keyword = CLOVA_PAYLOAD_JESIKA;
                                break;
                            case 4:
                                keyword = CLOVA_PAYLOAD_JJANGGUYA;
                                break;
                            case 5:
                                keyword = CLOVA_PAYLOAD_PINOCCHIO;
                                break;
                            case 6:
                                keyword = CLOVA_PAYLOAD_HI_LG;
                                break;
                            default:
                                keyword = CLOVA_PAYLOAD_HEY_CLOVA;
                                break;
                            }
                            json_object_object_add(json_wakeWord, CLOVA_PAYLOAD_NAME,
                                                   JSON_OBJECT_NEW_STRING(keyword));

                            json_object *json_indices = json_object_new_object();
                            if (json_indices) {
                                json_object_object_add(
                                    json_indices, CLOVA_PAYLOAD_START_INDEX_IN_SAMPLES,
                                    json_object_new_int(asr_get_keyword_begin_index()));

                                json_object_object_add(
                                    json_indices, CLOVA_PAYLOAD_END_INDEX_IN_SAMPLES,
                                    json_object_new_int(asr_get_keyword_end_index(1)));

                                json_object_object_add(json_wakeWord, CLOVA_PAYLOAD_INDICES,
                                                       json_indices);
                            }

                            json_object_object_add(json_payload, CLOVA_PAYLOAD_WAKE_WORD,
                                                   json_wakeWord);
                        }

                        json_object_object_add(json_initiator, CLOVA_PAYLOAD_PAYLOAD, json_payload);
                    }

                    json_object_object_add(json_event_payload, CLOVA_PAYLOAD_INITIATOR,
                                           json_initiator);
                }
            }
        }
    } else if (StrEq(name_space, NAMESPACE_ALERTS)) {
        if (!StrEq(event_name, NAME_ALERT_REQUEST_SYNC)) {
            char *token = JSON_GET_STRING_FROM_OBJECT(cmd_params, PAYLOAD_TOKEN);
            char *type = JSON_GET_STRING_FROM_OBJECT(cmd_params, PAYLOAD_TYPE);

            if (token)
                json_object_object_add(json_event_payload, PAYLOAD_TOKEN,
                                       JSON_OBJECT_NEW_STRING(token));

            json_object_object_add(json_event_payload, PAYLOAD_TYPE, JSON_OBJECT_NEW_STRING(type));
        }
    } else if (StrEq(name_space, NAMESPACE_AUDIOPLAYER)) {
        char *token = JSON_GET_STRING_FROM_OBJECT(cmd_params, KEY_stream_id);
        long offset_ms = JSON_GET_LONG_FROM_OBJECT(cmd_params, KEY_offset);
        long duration_ms = JSON_GET_LONG_FROM_OBJECT(cmd_params, KEY_duratioin);

        if (StrEq(event_name, CLOVA_HEADER_NAME_STREAM_REQUESTED)) {
            char *audioItemId =
                JSON_GET_STRING_FROM_OBJECT(cmd_params, CLOVA_PAYLOAD_NAME_AUDIO_ITEM_ID);
            json_object *audioStream = json_object_object_dup(
                json_object_object_get(cmd_params, CLOVA_PAYLOAD_NAME_AUDIO_STREAM));
            if (audioStream) {
                json_object_object_add(json_event_payload, CLOVA_PAYLOAD_NAME_AUDIO_ITEM_ID,
                                       JSON_OBJECT_NEW_STRING(audioItemId));
                json_object_object_add(json_event_payload, CLOVA_PAYLOAD_NAME_AUDIO_STREAM,
                                       audioStream);
            }
        } else if (StrEq(event_name, CLOVA_HEADER_NAME_REPORT_PLAYBACK_STATE)) {
            char *playerActivity =
                JSON_GET_STRING_FROM_OBJECT(cmd_params, CLOVA_PAYLOAD_NAME_PLAYER_ACTIVITY);
            char *repeatMode =
                JSON_GET_STRING_FROM_OBJECT(cmd_params, CLOVA_PAYLOAD_NAME_REPEATMODE);

            json_object_object_add(json_event_payload, CLOVA_PAYLOAD_NAME_PLAYER_ACTIVITY,
                                   JSON_OBJECT_NEW_STRING(playerActivity));

            json_object_object_add(json_event_payload, CLOVA_PAYLOAD_NAME_REPEATMODE,
                                   JSON_OBJECT_NEW_STRING(repeatMode));

            if (!StrEq(playerActivity, PLAYER_IDLE)) {
                char *token = JSON_GET_STRING_FROM_OBJECT(cmd_params, CLOVA_PAYLOAD_NAME_TOKEN);
                long offsetInMilliseconds = JSON_GET_LONG_FROM_OBJECT(
                    cmd_params, CLOVA_PAYLOAD_NAME_OFFSET_IN_MILLISECONDS);
                long totalInMilliseconds = JSON_GET_LONG_FROM_OBJECT(
                    cmd_params, CLOVA_PAYLOAD_NAME_TOTAL_IN_MILLISECONDES);
                json_object *stream = json_object_object_dup(
                    json_object_object_get(cmd_params, CLOVA_PAYLOAD_NAME_STREAM));
                if (stream) {
                    json_object_object_add(json_event_payload, CLOVA_PAYLOAD_NAME_TOKEN,
                                           JSON_OBJECT_NEW_STRING(token));
                    json_object_object_add(json_event_payload, CLOVA_PAYLOAD_NAME_STREAM, stream);
                    json_object_object_add(json_event_payload,
                                           CLOVA_PAYLOAD_NAME_OFFSET_IN_MILLISECONDS,
                                           json_object_new_int64(offsetInMilliseconds));
                    json_object_object_add(json_event_payload,
                                           CLOVA_PAYLOAD_NAME_TOTAL_IN_MILLISECONDES,
                                           json_object_new_int64(totalInMilliseconds));
                }
            }
        } else {
            json_object_object_add(json_event_payload, PAYLOAD_TOKEN,
                                   JSON_OBJECT_NEW_STRING(token));
            json_object_object_add(json_event_payload, PAYLOAD_OFFSET,
                                   json_object_new_int64(offset_ms));
        }
    } else if (StrEq(name_space, NAMESPACE_PLAYBACKCTROLLER)) {
        if (StrEq(event_name, CLOVA_HEADER_NAME_SET_REPEAT_MODE_COMMAND_ISSUED)) {
            json_object_object_add(json_event_payload, CLOVA_PAYLOAD_NAME_REPEATMODE,
                                   JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_VALUE_NONE));
        } else if (StrEq(event_name, NAME_PLAYCOMMANDISSUED)) {
            if (cmd_params) {
                if (json_event_payload) {
                    json_object_put(json_event_payload);
                }
                json_event_payload = json_object_object_dup(cmd_params);
            }
        }
    } else if (StrEq(name_space, NAMESPACE_SPEAKER)) {
        int volume = JSON_GET_INT_FROM_OBJECT(cmd_params, PAYLOAD_VOLUME);
        int is_muted = JSON_GET_INT_FROM_OBJECT(cmd_params, PAYLOAD_MUTED);

        json_object_object_add(json_event_payload, PAYLOAD_VOLUME, json_object_new_int64(volume));
        json_object_object_add(json_event_payload, PAYLOAD_MUTED,
                               json_object_new_boolean(is_muted));
    } else if (StrEq(name_space, NAMESPACE_SPEECHSYNTHESIZER)) {
        if (StrEq(event_name, CLOVA_HEADER_NAME_REQUEST)) {
            char *text = JSON_GET_STRING_FROM_OBJECT(cmd_params, CLOVA_PAYLOAD_NAME_TEXT);
            char *lang = JSON_GET_STRING_FROM_OBJECT(cmd_params, CLOVA_PAYLOAD_NAME_LANG);

            json_object_object_add(json_event_payload, CLOVA_PAYLOAD_NAME_TEXT,
                                   JSON_OBJECT_NEW_STRING(text));
            json_object_object_add(json_event_payload, CLOVA_PAYLOAD_NAME_LANG,
                                   JSON_OBJECT_NEW_STRING(lang));
        } else {
            char *token = JSON_GET_STRING_FROM_OBJECT(cmd_params, PAYLOAD_TOKEN);
            json_object_object_add(json_event_payload, PAYLOAD_TOKEN,
                                   JSON_OBJECT_NEW_STRING(token));
        }
    } else if (StrEq(name_space, CLOVA_HEADER_NAMESPACE_DEVICE_CONTROL)) {
        if (StrEq(event_name, CLOVA_HEADER_NAME_ACTION_EXECUTED) ||
            StrEq(event_name, CLOVA_HEADER_NAME_ACTION_FAILED)) {
            char *target = JSON_GET_STRING_FROM_OBJECT(cmd_params, CLOVA_PAYLOAD_NAME_TARGET);
            char *command = JSON_GET_STRING_FROM_OBJECT(cmd_params, CLOVA_PAYLOAD_NAME_COMMAND);

            json_object_object_add(json_event_payload, CLOVA_PAYLOAD_NAME_TARGET,
                                   JSON_OBJECT_NEW_STRING(target));
            json_object_object_add(json_event_payload, CLOVA_PAYLOAD_NAME_COMMAND,
                                   JSON_OBJECT_NEW_STRING(command));
        }
    } else if (StrEq(name_space, CLOVA_HEADER_NAMESPACE_SETTINGS)) {
        if (StrEq(event_name, CLOVA_HEADER_NAME_REPORT)) {
            clova_get_report_settings(json_event_payload);
        } else if (StrEq(event_name, CLOVA_HEADER_NAME_REQUEST_STORE_SETTINGS)) {
            clova_get_store_settings(json_event_payload);
            clova_get_location_info_for_settings(json_event_payload);
            clova_get_voice_type(json_event_payload);
        } else if (StrEq(event_name, CLOVA_HEADER_NAME_REQUEST_SYNCHRONIZATION)) {
            if (cmd_params) {
                int syncWithStorage =
                    JSON_GET_BOOL_FROM_OBJECT(cmd_params, CLOVA_PAYLOAD_NAME_SYNC_WITH_STORAGE);
                json_object_object_add(json_event_payload, CLOVA_PAYLOAD_NAME_SYNC_WITH_STORAGE,
                                       json_object_new_boolean(syncWithStorage));
            }
        }
    } else if (StrEq(name_space, CLOVA_HEADER_CLOVA_NAMESPACE)) {
        if (StrEq(event_name, CLOVA_HEADER_PROCESSDELEGATEDEVENT)) {
            char *delegationId =
                JSON_GET_STRING_FROM_OBJECT(cmd_params, CLOVA_PAYLOAD_VAL_DELEGATIONID);
            json_object_object_add(json_event_payload, CLOVA_PAYLOAD_VAL_DELEGATIONID,
                                   JSON_OBJECT_NEW_STRING(delegationId));
        }
    } else if (StrEq(name_space, CLOVA_NAMESPACE_SYSTME)) {
        if (StrEq(event_name, CLOVA_NAME_REPORT_CLIENT_STATE)) {
            json_object *update =
                json_object_object_dup(json_object_object_get(cmd_params, CLOVA_PAYLOAD_UPDATE));
            if (update) {
                json_object_object_add(json_event_payload, CLOVA_PAYLOAD_UPDATE, update);
            }
        } else if (StrEq(event_name, CLOVA_NAME_REPORT_UPDATE_READY_STATE)) {
            json_object *state =
                json_object_object_dup(json_object_object_get(cmd_params, CLOVA_PAYLOAD_STATE));
            if (state) {
                json_object_object_add(json_event_payload, CLOVA_PAYLOAD_STATE, state);
            }
            json_object *ready =
                json_object_object_dup(json_object_object_get(cmd_params, CLOVA_PAYLOAD_READY));
            if (ready) {
                json_object_object_add(json_event_payload, CLOVA_PAYLOAD_READY, ready);
            }
        }
#if defined(LGUPLUS_IPTV_ENABLE)
    } else if (StrEq(name_space, IPTV_HEADER_NAMESPACE_CDK)) {
        if (StrEq(event_name, IPTV_HEADER_NAME_SEND_MESSAGE)) {
            if (cmd_params) {
                if (json_event_payload) {
                    json_object_put(json_event_payload);
                }
                json_event_payload = json_object_object_dup(cmd_params);
            }
        }
#endif
    }

    json_event = json_object_new_object();
    if (json_event && json_event_header && json_event_payload) {
        json_object_object_add(json_event, KEY_HEADER, json_event_header);
        json_object_object_add(json_event, KEY_PAYLOAD, json_event_payload);
    } else {
        if (json_event_header) {
            json_object_put(json_event_header);
        }
        if (json_event_payload) {
            json_object_put(json_event_payload);
        }
    }

    return json_event;
}

json_object *asr_create_context(json_object *state_js, json_object *cmd_js, int req_type)
{
    int ret = -1;
    char *name = NULL;
    json_object *playback_state = NULL;
    json_object *volume_state = NULL;
    json_object *speech_state = NULL;
    json_object *json_context_list = NULL;
    json_object *json_all_alert_list = NULL;
    json_object *json_active_alert_list = NULL;
    json_object *recognizer_state = NULL;

    ASR_PRINT(ASR_DEBUG_TRACE, "+++++ start at %lld\n", tickSincePowerOn());

    if (!state_js) {
        ASR_PRINT(ASR_DEBUG_ERR, "input error\n");
        return NULL;
    }

    name = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_NAME);

    json_context_list = json_object_new_array();
    if (json_context_list) {
        // PlaybackState
        playback_state =
            json_object_object_dup(json_object_object_get(state_js, NAME_PLAYBACKSTATE));
        if (StrEq(name, CLOVA_HEADER_NAME_PLAY_STARTED)) {
            json_object_object_add(playback_state, CLOVA_PAYLOAD_NAME_OFFSET_IN_MILLISECONDS,
                                   json_object_new_int64(0));
        }
        ret = asr_context_list_add(json_context_list, NAMESPACE_AUDIOPLAYER, NAME_PLAYBACKSTATE,
                                   playback_state);
        if (ret != 0) {
            if (playback_state) {
                json_object_put(playback_state);
            }
        }

        // AlertsState
        json_all_alert_list = json_object_new_array();
        json_active_alert_list = json_object_new_array();
        alert_get_all_alert(json_all_alert_list, json_active_alert_list);
        ret = asr_context_list_add(json_context_list, NAMESPACE_ALERTS, NAME_ALERTSTATE,
                                   asr_alerts_payload(json_all_alert_list, json_active_alert_list));
        if (ret != 0) {
            if (json_all_alert_list) {
                json_object_put(json_all_alert_list);
            }
            if (json_active_alert_list) {
                json_object_put(json_active_alert_list);
            }
        }

        // VolumeState
        volume_state = json_object_object_dup(json_object_object_get(state_js, NAME_VOLUMESTATE));
        ret = asr_context_list_add(json_context_list, NAMESPACE_SPEAKER, NAME_VOLUMESTATE,
                                   volume_state);
        if (ret != 0) {
            if (volume_state) {
                json_object_put(volume_state);
            }
        }

        // Device.DeviceState
        clova_get_device_status(json_context_list);

        // Device.Display
        clova_device_display_state(json_context_list);

        // Clova.Location
        clova_clova_location_state(json_context_list);

        // Clova.SavedPlace
        clova_clova_saved_place_state(json_context_list);

#if defined(LGUPLUS_IPTV_ENABLE)
        // CDK.Applications
        clova_cdk_applications_state(json_context_list);
#endif

        // SpeechSynthesizer.SpeechState
        speech_state = json_object_object_dup(
            json_object_object_get(state_js, CLOVA_HEADER_SPEECH_STATE_NAME));
        ret = asr_context_list_add(json_context_list, CLOVA_HEADER_SPEECHSYNTHESIZER_NAMESPACE,
                                   CLOVA_HEADER_SPEECH_STATE_NAME, speech_state);

        if (ret != 0) {
            if (speech_state) {
                json_object_put(speech_state);
            }
        }

        // Clova.FreetalkState
        clova_clova_extension_state(json_context_list);
    }

    ASR_PRINT(ASR_DEBUG_TRACE, "----- end at %lld\n", tickSincePowerOn());

    return json_context_list;
}

/*
 * create an event request json body
 *
 *    {
 *       "event":{
 *           "header": {
 *               "namespace": "event_type",
 *               "name": "{{event_name}}",
 *               "messageId": "",
 *           },
 *           "payload": {
 *                 -token-
 *           }
 *       }
 *    }
 */
char *asr_create_event_body(char *state_json, char *cmd_json)
{
    char *post_body = NULL;
    json_object *state_js = NULL;
    json_object *cmd_js = NULL;
    json_object *context = NULL;
    json_object *s_event = NULL;
    json_object *event = NULL;
    int req_type = -1;
    char *name_space = NULL;
    char *event_name = NULL;

    ASR_PRINT(ASR_DEBUG_TRACE, "+++++ start at %lld\n", tickSincePowerOn());

    if (!state_json || !cmd_json) {
        ASR_PRINT(ASR_DEBUG_ERR, "event request %lld\n", tickSincePowerOn());
        goto Exit;
    }

    state_js = json_tokener_parse(state_json);
    if (!state_js) {
        ASR_PRINT(ASR_DEBUG_ERR, "state_js is not a json:\n%s\n", state_json);
        goto Exit;
    }

    cmd_js = json_tokener_parse(cmd_json);
    if (!cmd_js) {
        ASR_PRINT(ASR_DEBUG_ERR, "cmd_js is not a json:\n%s\n", cmd_json);
        goto Exit;
    }

    req_type = JSON_GET_INT_FROM_OBJECT(state_js, KEY_RequestType);
    name_space = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_NAMESPACE);
    event_name = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_NAME);
    if (!event_name || !name_space) {
        ASR_PRINT(ASR_DEBUG_ERR, "event_name not found\n");
        goto Exit;
    }

    context = asr_create_context(state_js, cmd_js, req_type);

    // event_data means event name
    event = asr_event_create(cmd_js, req_type);
    if (event) {
        s_event = json_object_new_object();
        if (s_event) {
            json_object_object_add(s_event, KEY_CONTEXT,
                                   context ? context : json_object_new_array());
            json_object_object_add(s_event, KEY_EVENT, event);
            char *val_str = json_object_to_json_string(s_event);
            if (val_str) {
                post_body = strdup(val_str);
            }

            if (StrEq(event_name, CLOVA_HEADER_NAME_REPORT)) {
                ASR_PRINT(ASR_DEBUG_TRACE, "[LOG_CHECK (%s)] %s\r\n", event_name, post_body);
            } else {
                ASR_PRINT(ASR_DEBUG_TRACE, "[LOG_CHECK (%s)] %s\r\n", event_name,
                          json_object_to_json_string(event));
            }

            json_object_put(s_event);
        } else {
            if (context) {
                json_object_put(context);
            }
            if (event) {
                json_object_put(event);
            }
        }
    }

Exit:
    if (state_js) {
        json_object_put(state_js);
    }

    if (cmd_js) {
        json_object_put(cmd_js);
    }

    ASR_PRINT(ASR_DEBUG_TRACE, "----- end at %lld\n", tickSincePowerOn());

    return post_body;
}

int asr_check_dialogId_valid(const char *dialog_id) { return StrEq(g_recg_dialog_id, dialog_id); }
