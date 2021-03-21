
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "alexa_request_json.h"
#include "alexa_debug.h"
#include "alexa_emplayer.h"
#include "alexa_bluetooth.h"
#include "alexa_json_common.h"
#include "alexa_notification.h"
#include "alexa_alert.h"
#include "alexa_focus_mgmt.h"

#include "wm_util.h"

#ifdef EN_AVS_COMMS
#include "alexa_comms.h"
#endif

#ifdef AVS_MRM_ENABLE
#include "alexa_mrm.h"
#endif

#ifndef RAND_STR_LEN
#define RAND_STR_LEN (36)
#endif

extern WIIMU_CONTEXT *g_wiimu_shm;

#define Debug_func_line printf("------(%s:%d)-------\n", __FUNCTION__, __LINE__)

#define update_key_val(json_data, key, set_val)                                                    \
    do {                                                                                           \
        json_object_object_del((json_data), (key));                                                \
        json_object_object_add((json_data), (key), (set_val));                                     \
    } while (0)

volatile int64_t g_start_sample = 0;
volatile int64_t g_end_sample = 0;
static char g_recg_dialog_id[RAND_STR_LEN + 1] = {0};
json_object *g_initiator_js = NULL;
static pthread_mutex_t g_state_lock = PTHREAD_MUTEX_INITIALIZER;

/*"21abbca2-4db3-461a-bd77-2ab0251e84f5"*/
static char *alexa_rand_string_36(char *str)
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

void alexa_set_recognize_info(int64_t start_sample, int64_t end_sample)
{
    pthread_mutex_lock(&g_state_lock);
    // The startIndexInSamples will always be 8,000 samples
    g_start_sample = 8000;
    g_end_sample = g_start_sample + (end_sample - start_sample);
    pthread_mutex_unlock(&g_state_lock);
}

int alexa_set_initiator(json_object *initiator_js)
{
    int ret = -1;
    const char *js_str = NULL;

    pthread_mutex_lock(&g_state_lock);
    if (g_initiator_js) {
        json_object_put(g_initiator_js);
        g_initiator_js = NULL;
    }
    if (initiator_js) {
        js_str = json_object_to_json_string(initiator_js);
        if (js_str) {
            g_initiator_js = json_tokener_parse(js_str);
            ret = g_initiator_js ? 0 : -1;
        }
    }
    pthread_mutex_unlock(&g_state_lock);

    return ret;
}

#if 0
static json_object *alexa_get_initiator(void)
{
    const char *js_str = NULL;
    json_object *initiator_js = NULL;

    pthread_mutex_lock(&g_state_lock);
    if (g_initiator_js) {
        js_str = json_object_to_json_string(g_initiator_js);
        if (js_str) {
            initiator_js = json_tokener_parse(js_str);
        }
        json_object_put(g_initiator_js);
        g_initiator_js = NULL;
    }
    pthread_mutex_unlock(&g_state_lock);

    if (initiator_js) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "2 initiator payload is %s\n",
                    json_object_to_json_string(initiator_js));
    }

    return initiator_js;
}
#endif

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
int alexa_alerts_list_add(json_object *alert_list, char *token, char *type, char *scheduled_time)
{
    if (alert_list) {
        json_object *val = json_object_new_object();
        if (val) {
            json_object_object_add(val, PAYLOAD_TOKEN, JSON_OBJECT_NEW_STRING(token));
            json_object_object_add(val, PAYLOAD_TYPE, JSON_OBJECT_NEW_STRING(type));
            json_object_object_add(val, PAYLOAD_SCHEDULEDTIME,
                                   JSON_OBJECT_NEW_STRING(scheduled_time));
            json_object_array_add(alert_list, val);
            return 0;
        } else {
            ALEXA_PRINT(ALEXA_DEBUG_ERR, "json_object_new_object failed\n");
        }
    }

    ALEXA_PRINT(ALEXA_DEBUG_ERR, "input error failed\n");

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
json_object *alexa_alerts_payload(json_object *all_alerts_list, json_object *active_alerts_list)
{
    json_object *obj_payload = json_object_new_object();
    if (obj_payload) {
        json_object_object_add(obj_payload, PAYLOAD_ALLALERTS,
                               all_alerts_list ? all_alerts_list : json_object_new_array());
        json_object_object_add(obj_payload, PAYLOAD_ACTIVEALERTS,
                               active_alerts_list ? active_alerts_list : json_object_new_array());
        return obj_payload;
    }
    ALEXA_PRINT(ALEXA_DEBUG_ERR, "json_object_new_object failed\n");

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
json_object *alexa_event_header(char *name_space, char *name)
{
    json_object *obj_header = NULL;
    char message_id[RAND_STR_LEN + 1] = {0};
    char dialog_id[RAND_STR_LEN + 1] = {0};

    if (!name_space || !name) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "input error\n");
        return NULL;
    }

    obj_header = json_object_new_object();
    if (obj_header) {
        json_object_object_add(obj_header, KEY_NAMESPACE, JSON_OBJECT_NEW_STRING(name_space));
        json_object_object_add(obj_header, KEY_NAME, JSON_OBJECT_NEW_STRING(name));
        json_object_object_add(obj_header, KEY_MESSAGEID,
                               JSON_OBJECT_NEW_STRING(alexa_rand_string_36(message_id)));
        if (StrEq(name, NAME_RECOGNIZE)) {
            pthread_mutex_lock(&g_state_lock);
            if (strlen(g_recg_dialog_id) > 0) {
                json_object_object_add(obj_header, KEY_DIALOGREQUESTID,
                                       json_object_new_string(g_recg_dialog_id));
                memset((void *)g_recg_dialog_id, 0, sizeof(g_recg_dialog_id));
            } else {
                json_object_object_add(obj_header, KEY_DIALOGREQUESTID,
                                       JSON_OBJECT_NEW_STRING(alexa_rand_string_36(dialog_id)));
            }
            pthread_mutex_unlock(&g_state_lock);
        } else if (StrEq(name, NAME_REPORTECHOSPATIALPERCEPTIONDATA)) {
            pthread_mutex_lock(&g_state_lock);
            json_object_object_add(obj_header, KEY_DIALOGREQUESTID,
                                   JSON_OBJECT_NEW_STRING(alexa_rand_string_36(g_recg_dialog_id)));
            pthread_mutex_unlock(&g_state_lock);
        }
        return obj_header;
    }

    ALEXA_PRINT(ALEXA_DEBUG_ERR, "json_object_new_object failed\n");

    return NULL;
}

/*
"header": {
}
"payload":{
}
*/
/*
 * Create an event data
 * Input :
 *         @header: the header of event
 *         @payload: the payload of event
 *
 * Output: an json
 */
json_object *alexa_event_body(json_object *header, json_object *payload)
{
    json_object *obj_event = json_object_new_object();
    if (obj_event) {
        json_object_object_add(obj_event, KEY_HEADER, header ? header : json_object_new_object());
        json_object_object_add(obj_event, KEY_PAYLOAD,
                               payload ? payload : json_object_new_object());

        return obj_event;
    }

    ALEXA_PRINT(ALEXA_DEBUG_ERR, "json_object_new_object failed\n");

    return NULL;
}

#if defined(IHOME_ALEXA)

/*struct json_object* json_object_new_array(void);*/
/*
 * Add an alert to the alert list
 * Input :
 *           @alert_list:alert_list
 *         @id: ID of alert
 *         @time: alert current time
 *         @type: timer or alarm
 *         @status: active or inactive
 *       @repeat:everyday or everymonday ...
 *
 * Output:
 *         @0: ok
 *         @other: error
 *
 * objest format as below
 *  { "allAlarm": [
 *  { "ID":"0", "Time":"10:20AM", "type":"timer", "status":"active" "repeat":"EveryDay" },
 *  { "ID":"1", "Time":"10:19AM", "type":"alarm", "status":"active" "repeat":"" } ],
 *  "toneCount":"4", "toneId":"0", "preWake":"1", "volume":"40", }
 */

int AlexaRequestAppListAdd(json_object *alert_list, char *id, char *time, char *type, char *status,
                           char *repeat)
{
    if (alert_list) {
        json_object *val = json_object_new_object();
        if (val) {
            json_object_object_add(val, ALAMR_ID_APP, JSON_OBJECT_NEW_STRING(id));
            json_object_object_add(val, ALARM_TIME_APP, JSON_OBJECT_NEW_STRING(time));
            json_object_object_add(val, ALARM_TYPE_APP, JSON_OBJECT_NEW_STRING(type));
            json_object_object_add(val, ALAMR_STATUS_APP, JSON_OBJECT_NEW_STRING(status));
            json_object_object_add(val, ALARM_REPEAT_APP, JSON_OBJECT_NEW_STRING(repeat));
            json_object_array_add(alert_list, val);
            return 0;
        } else {
            ALEXA_PRINT(ALEXA_DEBUG_ERR, "json_object_new_object failed\n");
        }
    }

    ALEXA_PRINT(ALEXA_DEBUG_ERR, "input error failed\n");

    return -1;
}

/*
 * Create for app alerts include all timers and alarms
 * Input :
 *         @all_alerts_list: all alert lists object
 *         @toneCount: the active alert list
 *         @toneId: the active alert list
 *         @preWake: the active alert list
 *         @volume: the active alert list
 *
 * Output: a json
 */
json_object *AlexaAlertForsApp(json_object *all_alerts_list, char *toneCount, char *toneId,
                               char *preWake, char *volume)
{
    json_object *obj_payload = json_object_new_object();
    if (obj_payload) {
        json_object_object_add(obj_payload, ALAMR_ALL_ALERM_APP,
                               all_alerts_list ? all_alerts_list : json_object_new_array());
        json_object_object_add(obj_payload, ALARM_TONECOUNT_APP, JSON_OBJECT_NEW_STRING(toneCount));
        json_object_object_add(obj_payload, ALARM_TONEID_APP, JSON_OBJECT_NEW_STRING(toneId));
        json_object_object_add(obj_payload, ALAMR_PREWAKE_APP, JSON_OBJECT_NEW_STRING(preWake));
        json_object_object_add(obj_payload, ALARM_VOLUME_APP, JSON_OBJECT_NEW_STRING(volume));
        return obj_payload;
    }
    ALEXA_PRINT(ALEXA_DEBUG_ERR, "json_object_new_object failed\n");

    return NULL;
}
#endif

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
int alexa_context_list_add(json_object *context_list, char *name_space, char *name,
                           json_object *payload)
{
    if (context_list) {
        json_object *header = json_object_new_object();
        json_object *val = json_object_new_object();
        if (header && val) {
            json_object_object_add(header, KEY_NAMESPACE, JSON_OBJECT_NEW_STRING(name_space));
            json_object_object_add(header, KEY_NAME, JSON_OBJECT_NEW_STRING(name));
            json_object_object_add(val, KEY_HEADER, header);
            json_object_object_add(val, KEY_PAYLOAD, payload ? payload : json_object_new_object());
            json_object_array_add(context_list, val);

            return 0;
        } else {
            if (header) {
                json_object_put(header);
            }
            if (val) {
                json_object_put(val);
            }
            ALEXA_PRINT(ALEXA_DEBUG_ERR, "json_object_new_object failed\n");
        }
    }

    ALEXA_PRINT(ALEXA_DEBUG_ERR, "input error failed\n");

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
json_object *alexa_context(json_object *context_list, json_object *event)
{
    json_object *obj = json_object_new_object();
    if (obj) {
        json_object_object_add(obj, KEY_CONTEXT,
                               context_list ? context_list : json_object_new_array());
        json_object_object_add(obj, KEY_EVENT, event ? event : json_object_new_object());

        return obj;
    }

    ALEXA_PRINT(ALEXA_DEBUG_ERR, "json_object_new_object failed\n");

    return NULL;
}

static int alexa_need_context(char *name_space, char *event_name)
{
    int ret = 0;

    if ((StrEq(name_space, NAMESPACE_Bluetooth)) || (StrEq(event_name, NAME_RECOGNIZE)) ||
        (StrEq(event_name, NAME_SYNCHORNIZESTATE)) ||
        (StrEq(name_space, NAMESPACE_PLAYBACKCTROLLER)) ||
        (StrEq(name_space, NAMESPACE_ExternalMediaPlayer) || StrEq(name_space, NAMESPACE_Alexa))) {
        ret = 1;
    }

    return ret;
}

static json_object *json_object_object_dup(json_object *src_js)
{
    json_object *obj_dup = NULL;
    const char *str_str = NULL;

    if (src_js) {
        str_str = json_object_to_json_string(src_js);
        obj_dup = json_tokener_parse(str_str);
    }

    return obj_dup;
}

/*
  *  create an json about the event request body
  */
static json_object *alexa_event_create(json_object *cmd_js, int req_type)
{
    json_object *json_event_header = NULL;
    json_object *json_event_payload = NULL;
    json_object *json_event = NULL;
    json_object *json_cur_state = NULL;
    json_object *json_error = NULL;
    // json_object *json_event_payload1 = NULL;
    // json_object *json_event_payload2 = NULL;
    // json_object *json_event_payload3 = NULL;
    char *event_name = NULL;
    char *name_space = NULL;
    json_object *cmd_params = NULL;

    if (!cmd_js) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "input error\n");
        return NULL;
    }

    json_event_payload = json_object_new_object();
    if (!json_event_payload) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "json_object_new_object failed\n");
        return NULL;
    }

    cmd_params = json_object_object_get(cmd_js, KEY_params);

    event_name = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_NAME);
    name_space = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_NAMESPACE);
    json_event_header = alexa_event_header(name_space, event_name);

    if (StrEq(name_space, NAMESPACE_SPEECHRECOGNIZER)) {
        if (StrEq(event_name, NAME_RECOGNIZE)) {
            // char *initiator_type = NULL;
            int profile = AlexaProfileType();
            if (FAR_FIELD == profile) {
                // initiator_type = Initiator_WAKEWORD;
                json_object_object_add(json_event_payload, PAYLOAD_PROFILE,
                                       JSON_OBJECT_NEW_STRING(PROFILE_FAR_FIELD));
            } else if (NEAR_FIELD == profile) {
                // initiator_type = Initiator_TAP;
                json_object_object_add(json_event_payload, PAYLOAD_PROFILE,
                                       JSON_OBJECT_NEW_STRING(PROFILE_NEAR_FIELD));
            } else {
                // initiator_type = Initiator_TAP;
                json_object_object_add(json_event_payload, PAYLOAD_PROFILE,
                                       JSON_OBJECT_NEW_STRING(PROFILE_CLOSE_TALK));
            }
            json_object_object_add(json_event_payload, PAYLOAD_FORMAT,
                                   JSON_OBJECT_NEW_STRING(FORMAT_AUDIO_L16));
#if defined(AVS_CLOUD_BASED_WWV)
            // if not talk continue so clear the initiator
            if (req_type != 2) {
                alexa_set_initiator(NULL);
            }

            // talk continue
            json_event_payload3 = alexa_get_initiator();
            if (json_event_payload3 || req_type == 2) {
                if (json_event_payload3) {
                    json_object_object_add(json_event_payload, PAYLOAD_initiator,
                                           json_event_payload3);
                }
            } else {
                json_event_payload3 = json_object_new_object();
                // push to talk must be TAP
                if (req_type == 0 && FAR_FIELD == profile) {
                    initiator_type = Initiator_TAP;
                    // wake up
                } else if (req_type == 1 && FAR_FIELD == profile) {
                    initiator_type = Initiator_WAKEWORD;
                    json_event_payload1 = json_object_new_object();
                    pthread_mutex_lock(&g_state_lock);
                    json_object_object_add(
                        json_event_payload1, PAYLOAD_startIndexInSamples,
                        json_object_new_int64(
                            (g_start_sample > 0 ? g_start_sample : 8000))); // 300ms
                    json_object_object_add(
                        json_event_payload1, PAYLOAD_endIndexInSamples,
                        json_object_new_int64((g_end_sample > 0 ? g_end_sample : 19200))); // 1300ms
                    pthread_mutex_unlock(&g_state_lock);
                    json_event_payload2 = json_object_new_object();
                    json_object_object_add(json_event_payload2, PAYLOAD_wakeWordIndices,
                                           json_event_payload1);
                    json_object_object_add(json_event_payload3, KEY_PAYLOAD, json_event_payload2);
                    // talk continue, never goes here
                } else if (req_type == 2 && FAR_FIELD == profile) {
                    initiator_type = Initiator_TAP;
                    // intercom alexa
                } else if (req_type == 3) {
                    initiator_type = Initiator_PRESS_HOLD;
                }

                json_object_object_add(json_event_payload3, PAYLOAD_TYPE,
                                       JSON_OBJECT_NEW_STRING(initiator_type));
                json_object_object_add(json_event_payload, PAYLOAD_initiator, json_event_payload3);
            }
#endif
        } else if (StrEq(event_name, NAME_REPORTECHOSPATIALPERCEPTIONDATA)) {
            unsigned int engr_voice =
                (unsigned int)JSON_GET_INT_FROM_OBJECT(cmd_params, PAYLOAD_VOICEENERGY);
            unsigned int engr_noise =
                (unsigned int)JSON_GET_INT_FROM_OBJECT(cmd_params, PAYLOAD_AMBIENTENERGY);
            json_object_object_add(json_event_payload, PAYLOAD_VOICEENERGY,
                                   json_object_new_int(engr_voice));
            json_object_object_add(json_event_payload, PAYLOAD_AMBIENTENERGY,
                                   json_object_new_int(engr_noise));
        }
    } else if (StrEq(name_space, NAMESPACE_ALERTS)) {
        /*modify by weiqiang.huang for AVS alert V1.3 2018-07-30 -- begin*/
        if (StrEq(event_name, NAME_DELETE_ALERTS_SUCCEEDED) ||
            StrEq(event_name, NAME_DELETE_ALERTS_FAILED)) {
            json_object *tokens = json_object_object_dup(cmd_params);
            json_object_object_add(json_event_payload, PAYLOAD_TOKENS, tokens);
        } else if (StrEq(event_name, NAME_ALERT_VOLUME_CHANGED)) {
            int volume = JSON_GET_INT_FROM_OBJECT(cmd_params, PAYLOAD_VOLUME);
            json_object_object_add(json_event_payload, PAYLOAD_VOLUME,
                                   json_object_new_int64(volume));
        } else {
            char *token = JSON_GET_STRING_FROM_OBJECT(cmd_params, PAYLOAD_TOKEN);
            json_object_object_add(json_event_payload, PAYLOAD_TOKEN,
                                   JSON_OBJECT_NEW_STRING(token));
        }
        /*modify by weiqiang.huang for AVS alert V1.3 2018-07-30 -- end*/

        /*add by weiqiang.huang for MAR-385 Equalizer controller 2018-08-30 -begin*/
    } else if (StrEq(name_space, NAMESPACE_EQUALIZERCONTROLLER)) {
        name_space = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_NAMESPACE);
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Equalizer cmd param string: %s\n",
                    json_object_to_json_string(cmd_params));
        if (StrEq(event_name, NAME_EQUALIZER_CHANGED)) {
            json_object *js_bands = json_object_object_get(cmd_params, NAME_BANDS);
            json_object *js_mode = json_object_object_get(cmd_params, NAME_MODE);

            if (js_bands) {
                json_object_object_add(
                    json_event_payload, NAME_BANDS,
                    JSON_OBJECT_NEW_STRING(json_object_to_json_string(js_bands)));
            }

            if (js_mode) {
                json_object_object_add(json_event_payload, NAME_BANDS,
                                       JSON_OBJECT_NEW_STRING(json_object_to_json_string(js_mode)));
            }
        }
        /*add by weiqiang.huang for MAR-385 Equalizer controller 2018-08-30 -end*/
    } else if (StrEq(name_space, NAMESPACE_AUDIOPLAYER)) {
        char *token = JSON_GET_STRING_FROM_OBJECT(cmd_params, KEY_stream_id);
        long offset_ms = JSON_GET_LONG_FROM_OBJECT(cmd_params, KEY_offset);
        long duration_ms = JSON_GET_LONG_FROM_OBJECT(cmd_params, KEY_duratioin);

        if (StrEq(event_name, NAME_PLAYSTUTTERFIN)) {
            json_object_object_add(json_event_payload, PAYLOAD_TOKEN,
                                   JSON_OBJECT_NEW_STRING(token));
            json_object_object_add(json_event_payload, PAYLOAD_OFFSET,
                                   json_object_new_int64(offset_ms));
            json_object_object_add(json_event_payload, PAYLOAD_DURATION,
                                   json_object_new_int64(duration_ms));

        } else if (StrEq(event_name, NAME_PLAYFAILED)) {
            char *error_type = NULL;
            char *error_msg = NULL;

            json_object_object_add(json_event_payload, PAYLOAD_TOKEN,
                                   JSON_OBJECT_NEW_STRING(token));

            json_cur_state = json_object_new_object();
            json_object_object_add(json_cur_state, PAYLOAD_TOKEN, JSON_OBJECT_NEW_STRING(NULL));
            json_object_object_add(json_cur_state, PAYLOAD_OFFSET, json_object_new_int64(0));
            json_object_object_add(json_cur_state, PAYLOAD_PLAYACTIVE,
                                   JSON_OBJECT_NEW_STRING(PLAYER_IDLE));

            json_object_object_add(json_event_payload, PAYLOAD_CURPLAYBACKSTATE, json_cur_state);

            json_error = json_object_new_object();
            long error_num = JSON_GET_LONG_FROM_OBJECT(cmd_params, KEY_error_num);
            if (error_num > 404) {
                error_type = MEDIA_ERROR_INTERNAL_SERVER_ERROR;
                error_msg = MEDIA_ERROR_INTERNAL_SERVER_ERROR_MESSAGE;
            } else if (error_num >= 400 && error_num <= 404) {
                error_type = MEDIA_ERROR_INTERNAL_DEVICE_ERROR;
                error_msg = MEDIA_ERROR_INTERNAL_DEVICE_ERROR_MESSAGE;
            } else if (error_num == 204) {
                error_type = MEDIA_ERROR_INTERNAL_SERVER_ERROR;
                error_msg = MEDIA_ERROR_INTERNAL_SERVER_ERROR_MESSAGE;
            } else {
                error_type = MEDIA_ERROR_INTERNAL_DEVICE_ERROR;
                error_msg = MEDIA_ERROR_INTERNAL_DEVICE_ERROR_MESSAGE;
            }
            json_object_object_add(json_error, ERROR_TYPE, JSON_OBJECT_NEW_STRING(error_type));
            json_object_object_add(json_error, ERROR_MESSAGE, JSON_OBJECT_NEW_STRING(error_msg));

            json_object_object_add(json_event_payload, PAYLOAD_ERROR, json_error);
        } else if (StrEq(event_name, NAME_QUEUECLEARED)) {
            /* NULL payload*/
        } else if (StrEq(event_name, NAME_STREAMMETADATA)) {
            /*
                        "payload": {
                            "token": "{{STRING}}",
                            "metadata": {
                                "{{STRING}}": "{{STRING}}",
                                "{{STRING}}": {{BOOLEAN}}
                                "{{STRING}}": "{{STRING NUMBER}}"
                            }
                        }
                    */
        } else {
            json_object_object_add(json_event_payload, PAYLOAD_TOKEN,
                                   JSON_OBJECT_NEW_STRING(token));
            json_object_object_add(json_event_payload, PAYLOAD_OFFSET,
                                   json_object_new_int64(offset_ms));
        }
    } else if (StrEq(name_space, NAMESPACE_PLAYBACKCTROLLER)) {

    } else if (StrEq(name_space, NAMESPACE_SPEAKER)) {
        int volume = JSON_GET_INT_FROM_OBJECT(cmd_params, PAYLOAD_VOLUME);
        int is_muted = JSON_GET_INT_FROM_OBJECT(cmd_params, PAYLOAD_MUTED);

        json_object_object_add(json_event_payload, PAYLOAD_VOLUME, json_object_new_int64(volume));
        json_object_object_add(json_event_payload, PAYLOAD_MUTED,
                               json_object_new_boolean(is_muted));
    } else if (StrEq(name_space, NAMESPACE_SPEECHSYNTHESIZER)) {
        char *token = JSON_GET_STRING_FROM_OBJECT(cmd_params, PAYLOAD_TOKEN);
        json_object_object_add(json_event_payload, PAYLOAD_TOKEN, JSON_OBJECT_NEW_STRING(token));
    } else if (StrEq(name_space, NAMESPACE_SYSTEM)) {
        json_object *err_msg_js = NULL;
        const char *err_msg_str = NULL;
        long long inactive_ts = (long long)JSON_GET_LONG_FROM_OBJECT(cmd_params, KEY_inactive_ts);

        err_msg_js = json_object_object_get(cmd_params, KEY_err_msg);
        if (err_msg_js) {
            err_msg_str = json_object_to_json_string(err_msg_js);
        }
        if (StrEq(event_name, NAME_UserInactivityReport)) {
            long long now_ts = tickSincePowerOn();
            json_object_object_add(json_event_payload, PAYLOAD_UserInactivityReport,
                                   json_object_new_int64((now_ts - inactive_ts) / 1000));
        } else if (StrEq(event_name, NAME_ExceptionEncountered)) {
            // the token is about the unknown directive's name
            json_object_object_add(json_event_payload, PAYLOAD_UNKNOWN_DIRECTIVE,
                                   JSON_OBJECT_NEW_STRING(err_msg_str));

            json_error = json_object_new_object();
            json_object_object_add(json_error, ERROR_TYPE,
                                   JSON_OBJECT_NEW_STRING(SYSTEM_ERROR_UNEXPECTED));
            json_object_object_add(json_error, ERROR_MESSAGE,
                                   JSON_OBJECT_NEW_STRING(SYSTEM_ERROR_UNEXPECTED_INFO));

            json_object_object_add(json_event_payload, PAYLOAD_ERROR, json_error);
        } else if (StrEq(event_name, NAME_Exception)) {
            json_object_object_add(json_event_payload, PAYLOAD_CODE,
                                   JSON_OBJECT_NEW_STRING(CODE_INVALID_REQUEST_EXCEPTION));
            json_object_object_add(json_event_payload, PAYLOAD_DESCRIPTION,
                                   JSON_OBJECT_NEW_STRING(DESC_INVALID_REQUEST_EXCEPTION));

        } else if (StrEq(event_name, NAME_SoftwareInfo)) {
            fprintf(stderr, "the firmware is release at %s\n", g_wiimu_shm->build_date);
            json_object_object_add(json_event_payload, PAYLOAD_firmwareVersion,
                                   JSON_OBJECT_NEW_STRING(g_wiimu_shm->build_date));
        }
    } else if (StrEq(name_space, NAMESPACE_SETTINGS)) {
        char *language = JSON_GET_STRING_FROM_OBJECT(cmd_params, KEY_language);
        if (StrEq(event_name, NAME_SettingUpdated)) {
            json_object *settings = json_object_new_array();
            if (settings) {
                json_object *local = json_object_new_object();
                if (local) {
                    json_object_object_add(local, SETTING_KEY, JSON_OBJECT_NEW_STRING(KEY_LOCALE));
                    json_object_object_add(local, SETTING_VALUE, JSON_OBJECT_NEW_STRING(language));
                    json_object_array_add(settings, local);
                }
                json_object_object_add(json_event_payload, PAYLOAD_SETTINGS, settings);
            }
        }
    } else {
        if (json_event_payload) {
            json_object_put(json_event_payload);
            json_event_payload = NULL;
        }

        json_event_payload = json_object_object_dup(cmd_params);
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

/*
 * create an context which need by request
 * see https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/reference/context
 */
json_object *alexa_create_context(json_object *state_js, json_object *cmd_js, int req_type)
{
    json_object *playback_state = NULL;
    json_object *volume_state = NULL;
    json_object *speech_state = NULL;
    json_object *json_context_list = NULL;
    json_object *json_all_alert_list = NULL;
    json_object *json_active_alert_list = NULL;
    json_object *recognizer_state = NULL;
    char *event_name = NULL;

    if (!state_js) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "input error\n");
        return NULL;
    }

    json_context_list = json_object_new_array();
    if (json_context_list) {
        // PlaybackState
        playback_state =
            json_object_object_dup(json_object_object_get(state_js, NAME_PLAYBACKSTATE));
        alexa_context_list_add(json_context_list, NAMESPACE_AUDIOPLAYER, NAME_PLAYBACKSTATE,
                               playback_state);

        // AlertsState
        json_all_alert_list = json_object_new_array();
        json_active_alert_list = json_object_new_array();
        alert_get_all_alert(json_all_alert_list, json_active_alert_list);
        alexa_context_list_add(json_context_list, NAMESPACE_ALERTS, NAME_ALERTSTATE,
                               alexa_alerts_payload(json_all_alert_list, json_active_alert_list));

        // VolumeState
        volume_state = json_object_object_dup(json_object_object_get(state_js, NAME_VOLUMESTATE));
        alexa_context_list_add(json_context_list, NAMESPACE_SPEAKER, NAME_VOLUMESTATE,
                               volume_state);

        // SpeechState
        speech_state = json_object_object_dup(json_object_object_get(state_js, NAME_SPEECHSTATE));
        alexa_context_list_add(json_context_list, NAMESPACE_SPEECHSYNTHESIZER, NAME_SPEECHSTATE,
                               speech_state);

        // RecognizerState
        int profile = AlexaProfileType();
        if (profile == NEAR_FIELD || profile == FAR_FIELD) {
            ALEXA_PRINT(ALEXA_DEBUG_INFO, "RecognizerState state %lld\n", tickSincePowerOn());
            recognizer_state = json_object_new_object();
            json_object_object_add(recognizer_state, PAYLOAD_WAKEWORD,
                                   JSON_OBJECT_NEW_STRING(WAKE_WORD));
            alexa_context_list_add(json_context_list, NAMESPACE_SPEECHRECOGNIZER,
                                   NAME_RECOGNIZERSTATE, recognizer_state);
        }

        // NotificationState
        avs_notificaion_state(json_context_list);
#ifdef EN_AVS_BLE
        // BluetoothState
        avs_bluetooth_state(json_context_list);
#endif
// SipClientState
#ifdef EN_AVS_COMMS
        avs_comms_state_cxt(json_context_list);
#endif
        event_name = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_NAME);
        avs_emplayer_state(json_context_list, event_name);
        focus_mgmt_context_create(json_context_list);
#ifdef AVS_MRM_ENABLE
        avs_mrm_cluster_context_state(json_context_list);
#endif
    }
    ALEXA_PRINT(ALEXA_DEBUG_INFO, "event request %lld\n", tickSincePowerOn());

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
char *alexa_create_event_body(char *state_json, char *cmd_json, char **outURI)
{
    int need_contex = 0;
    char *post_body = NULL;
    json_object *state_js = NULL;
    json_object *cmd_js = NULL;
    json_object *context = NULL;
    json_object *s_event = NULL;
    json_object *event = NULL;
    int req_type = -1;
    char *name_space = NULL;
    char *event_name = NULL;
    char *tmp_uri = NULL;

    if (!state_json || !cmd_json) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "event request %lld\n", tickSincePowerOn());
    }

    state_js = json_tokener_parse(state_json);
    if (!state_js) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "state_js is not a json:\n%s\n", state_json);
    }

    cmd_js = json_tokener_parse(cmd_json);
    if (!cmd_js) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "cmd_js is not a json:\n%s\n", cmd_json);
        goto Exit;
    }

    req_type = JSON_GET_INT_FROM_OBJECT(state_js, KEY_RequestType);
    name_space = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_NAMESPACE);
    event_name = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_NAME);
    tmp_uri = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_URI);

    if (!event_name || !name_space) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "event_name not found\n");
        goto Exit;
    }

    if (tmp_uri && outURI) {
        *outURI = strdup(tmp_uri);
    }

    need_contex = alexa_need_context(name_space, event_name);

    if (tmp_uri && outURI) {
        *outURI = strdup(tmp_uri);
        need_contex = 1;
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, " fetched Mrm URI [%s]\n", *outURI);
    }

    // ALEXA_PRINT(ALEXA_DEBUG_TRACE, " need_contex [%d]\n", need_contex);

    if (need_contex) {
        context = alexa_create_context(state_js, cmd_js, req_type);
    }
#ifdef EN_AVS_BLE
    if (StrEq(name_space, NAMESPACE_Bluetooth)) {
        // For ble event, the event_data event cmd
        event = avs_bluetooth_event_create(cmd_js);
    } else
#endif
        if (StrEq(name_space, NAMESPACE_ExternalMediaPlayer) ||
            StrEq(name_space, NAMESPACE_Alexa)) {
        event = avs_emplayer_event_create(cmd_js);
    }
#ifdef EN_AVS_COMMS
    else if (StrEq(name_space, NAMESPACE_SIPUSERAGENT)) {
        event = avs_comms_event_create(cmd_js);
    }
#endif
    else {
        // event_data means event name
        event = alexa_event_create(cmd_js, req_type);
    }

    if (event) {
        s_event = json_object_new_object();
        if (s_event) {
            if (need_contex) {
                json_object_object_add(s_event, KEY_CONTEXT,
                                       context ? context : json_object_new_array());
            }
            json_object_object_add(s_event, KEY_EVENT, event);
            post_body = strdup(json_object_to_json_string(s_event));
            json_object_put(s_event);
        } else {
            if (context) {
                json_object_put(context);
            }
            if (event) {
                json_object_put(event);
            }
        }
    } else {
        if (context) {
            json_object_put(context);
        }
    }

    ALEXA_LOGCHECK(ALEXA_DEBUG_TRACE, "[LOG_CHECK (%s)]%s\r\n", event_name, post_body);

Exit:
    if (state_js) {
        json_object_put(state_js);
    }

    if (cmd_js) {
        json_object_put(cmd_js);
    }

    return post_body;
}

char *lifepod_create_event_body(char *state_json, char *cmd_json, char **outURI)
{
    int need_contex = 0;
    char *post_body = NULL;
    json_object *state_js = NULL;
    json_object *cmd_js = NULL;
    json_object *context = NULL;
    json_object *s_event = NULL;
    json_object *event = NULL;
    int req_type = -1;
    char *name_space = NULL;
    char *event_name = NULL;
    char *tmp_uri = NULL;

    if (!state_json || !cmd_json) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "event request %lld\n", tickSincePowerOn());
    }

    state_js = json_tokener_parse(state_json);
    if (!state_js) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "state_js is not a json:\n%s\n", state_json);
    }

    cmd_js = json_tokener_parse(cmd_json);
    if (!cmd_js) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "cmd_js is not a json:\n%s\n", cmd_json);
        goto Exit;
    }

    req_type = JSON_GET_INT_FROM_OBJECT(state_js, KEY_RequestType);
    name_space = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_NAMESPACE);
    event_name = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_NAME);
    tmp_uri = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_URI);

    if (!event_name || !name_space) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "event_name not found\n");
        goto Exit;
    }

    if (tmp_uri && outURI) {
        *outURI = strdup(tmp_uri);
    }

    need_contex = alexa_need_context(name_space, event_name);

    if (tmp_uri && outURI) {
        *outURI = strdup(tmp_uri);
        need_contex = 1;
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, " fetched Mrm URI [%s]\n", *outURI);
    }

    // ALEXA_PRINT(ALEXA_DEBUG_TRACE, " need_contex [%d]\n", need_contex);

    if (need_contex) {
        context = alexa_create_context(state_js, cmd_js, req_type);
    }
#ifdef EN_AVS_BLE
    if (StrEq(name_space, NAMESPACE_Bluetooth)) {
        // For ble event, the event_data event cmd
        event = avs_bluetooth_event_create(cmd_js);
    } else
#endif
        if (StrEq(name_space, NAMESPACE_ExternalMediaPlayer) ||
            StrEq(name_space, NAMESPACE_Alexa)) {
        event = avs_emplayer_event_create(cmd_js);
    }
#ifdef EN_AVS_COMMS
    else if (StrEq(name_space, NAMESPACE_SIPUSERAGENT)) {
        event = avs_comms_event_create(cmd_js);
    }
#endif
    else {
        // event_data means event name
        event = alexa_event_create(cmd_js, req_type);
    }

    if (event) {
        s_event = json_object_new_object();
        if (s_event) {
            if (need_contex) {
                json_object_object_add(s_event, KEY_CONTEXT,
                                       context ? context : json_object_new_array());
            }
            json_object_object_add(s_event, KEY_EVENT, event);
            post_body = strdup(json_object_to_json_string(s_event));
            json_object_put(s_event);
        } else {
            if (context) {
                json_object_put(context);
            }
            if (event) {
                json_object_put(event);
            }
        }
    } else {
        if (context) {
            json_object_put(context);
        }
    }

    ALEXA_LOGCHECK(ALEXA_DEBUG_TRACE, "[LFPD_LOG_CHECK (%s)]%s\r\n", event_name, post_body);

Exit:
    if (state_js) {
        json_object_put(state_js);
    }

    if (cmd_js) {
        json_object_put(cmd_js);
    }

    return post_body;
}
