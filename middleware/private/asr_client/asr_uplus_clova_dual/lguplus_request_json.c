
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "json.h"
#include "asr_debug.h"
#include "asr_authorization.h"
#include "asr_connect.h"
#include "asr_json_common.h"

#include "lguplus_json_common.h"
#include "lguplus_request_json.h"
#include "lguplus_event_queue.h"

extern char *g_service_name;

char *lguplus_create_cid_token(void)
{
    time_t time1;
    struct tm *time2;
    char time3[18] = {0};
    char *cid_token = NULL;

    time1 = time(NULL);
    time2 = localtime(&time1);
    srand((unsigned int)NsSincePowerOn());

    snprintf(time3, sizeof(time3), TIME_PATTERN, time2->tm_year + 1900, time2->tm_mon + 1,
             time2->tm_mday, time2->tm_hour + 9, time2->tm_min, time2->tm_sec, rand() % 100);
    asprintf(&cid_token, CID_TOKEN_PATTERN, time3, "SVC_001");

    return cid_token;
}

int lguplus_create_transactionId(char *transactionId, int tr_len, char *messageId, int mes_len,
                                 char *device_token)
{
    time_t time1;
    struct tm *time2;
    char time3[18] = {0};

    if (!transactionId || !messageId || !device_token)
        return -1;

    time1 = time(NULL);
    time2 = localtime(&time1);
    srand((unsigned int)NsSincePowerOn());

    snprintf(time3, sizeof(time3), TIME_PATTERN, time2->tm_year + 1900, time2->tm_mon + 1,
             time2->tm_mday, time2->tm_hour + 9, time2->tm_min, time2->tm_sec, rand() % 100);
    snprintf(transactionId, tr_len, TRANSACTIONID_PATTERN, time3, "SVC_001", device_token);
    snprintf(messageId, mes_len, MESSAGEID_PATTERN, time3, "SVC_001", device_token);

    return 0;
}

/*
currentState
"audioPlayer":
{
"header": {
"schema": "audioPlayer",
"method": "state"
},
"state": {
"offset": "12345",
"itemId": "A12345",
"activity": "STOP"
}
}
*/
static int lguplus_current_state(json_object *request_json)
{
    int ret = -1;
    json_object *state_json = NULL;

    if (!request_json) {
        return ret;
    }

    state_json = json_object_new_object();
    if (!state_json) {
        return ret;
    }

    ret = lguplus_audio_state(state_json);
    {
        json_object *alert_state = json_object_new_object();
        json_object *header = json_object_new_object();
        json_object *state = json_object_new_object();
        if (alert_state && header && state) {
            json_object_object_add(header, "schema", JSON_OBJECT_NEW_STRING("Alert"));
            json_object_object_add(header, "method", JSON_OBJECT_NEW_STRING("state"));

            json_object *now_alerts = json_object_new_array();
            json_object_object_add(state, "nowAlert", now_alerts);

            json_object_object_add(alert_state, "header", header);
            json_object_object_add(alert_state, "state", state);
        }
        json_object_object_add(state_json, "alert", alert_state);
    }
    {
        json_object *devicetime_state = json_object_new_object();
        json_object *header = json_object_new_object();
        json_object *state = json_object_new_object();
        time_t tt;
        tt = time(&tt);
        if (devicetime_state && header && state) {
            json_object_object_add(header, "schema", JSON_OBJECT_NEW_STRING("DeviceTime"));
            json_object_object_add(header, "method", JSON_OBJECT_NEW_STRING("state"));

            char seconds[128] = {0};
            snprintf(seconds, sizeof(seconds), "%d", tt);

            char nanos[128] = {0};
            snprintf(nanos, sizeof(nanos), "%lld", tickSincePowerOn() * 1000);

            json_object_object_add(state, "seconds", JSON_OBJECT_NEW_STRING(seconds));
            json_object_object_add(state, "nanos", JSON_OBJECT_NEW_STRING(nanos));
            json_object_object_add(state, "utcTimeZone", JSON_OBJECT_NEW_STRING("GMT+9:00"));

            json_object_object_add(devicetime_state, "header", header);
            json_object_object_add(devicetime_state, "state", state);
        }
        json_object_object_add(state_json, "deviceTime", devicetime_state);
    }
    {
        json_object *additional_state = json_object_new_object();
        json_object *state = json_object_new_array();
        if (additional_state && state) {
            // json_object *battery = json_object_new_object();
            // json_object_object_add(battery, "key", JSON_OBJECT_NEW_STRING("BATTERY"));
            // json_object_object_add(battery, "value", JSON_OBJECT_NEW_STRING("100"));

            if (g_service_name) {
                json_object *audio = json_object_new_object();
                json_object_object_add(audio, "key", JSON_OBJECT_NEW_STRING("audioPlayerType"));
                json_object_object_add(audio, "value", JSON_OBJECT_NEW_STRING(g_service_name));
                json_object_array_add(state, audio);
            }

            // json_object *id = json_object_new_object();
            // json_object_object_add(id, "key", JSON_OBJECT_NEW_STRING("preTransactionid"));
            // json_object_object_add(id, "value", JSON_OBJECT_NEW_STRING(""));

            // json_object_array_add(state, battery);
            // json_object_array_add(state, id);

            json_object_object_add(additional_state, "state", state);
        }
        json_object_object_add(state_json, "additional", additional_state);
    }
    json_object_object_add(request_json, LGUPLUS_CUR_STATE, state_json);

    return ret;
}

json_object *lguplus_create_json_data(json_object *header, json_object *body)
{
    json_object *data = NULL;

    if (!header || !body)
        return NULL;

    data = json_object_new_object();
    if (!data)
        return NULL;

    json_object_object_add(data, LGUP_HEADER, header);
    lguplus_current_state(data);
    json_object_object_add(data, LGUP_BODY, body);

    return data;
}

json_object *lguplus_create_json_header(char *schema, char *method, char *transactionId,
                                        char *messageId)
{
    json_object *header = NULL;

    if (!schema || !method || !transactionId || !messageId)
        return NULL;

    header = json_object_new_object();
    if (!header)
        return NULL;

    json_object_object_add(header, LGUP_SCHEMA, json_object_new_string(schema));
    json_object_object_add(header, LGUP_METHOD, json_object_new_string(method));
    json_object_object_add(header, LGUP_TRANSACTION_ID, json_object_new_string(transactionId));
    json_object_object_add(header, LGUP_MESSAGE_ID, json_object_new_string(messageId));

    return header;
}

json_object *lguplus_create_inactive_json_body(void)
{
    json_object *body = NULL;
    json_object *inactive_time = NULL;
    int inactive_interval = 0;

    body = json_object_new_object();
    if (!body)
        return NULL;

    inactive_time = json_object_new_object();
    if (!inactive_time) {
        json_object_put(body);
        return NULL;
    }

    inactive_interval = tickSincePowerOn() - asr_session_get_inactive_time();
    inactive_interval = inactive_interval > 0 ? inactive_interval / 1000 : 0;

    json_object_object_add(body, LGUP_INACTIVE_TIME, inactive_time);

    json_object_object_add(inactive_time, LGUP_INACTIVE_TIME,
                           json_object_new_int(inactive_interval));

    return body;
}

json_object *lguplus_create_status_json_body(char *stat)
{
    json_object *body = NULL;
    json_object *status = NULL;

    body = json_object_new_object();
    if (!body)
        return NULL;

    status = json_object_new_object();
    if (!status) {
        json_object_put(body);
        return NULL;
    }

    json_object_object_add(body, LGUP_STATUS, status);

    json_object_object_add(status, LGUP_STATUS, json_object_new_string(stat));

    return body;
}

json_object *lguplus_create_wakeup_json_body(void)
{
    json_object *body = NULL;
    json_object *wakeup = NULL;

    body = json_object_new_object();
    if (!body)
        return NULL;

    wakeup = json_object_new_object();
    if (!wakeup) {
        json_object_put(body);
        return NULL;
    }

    json_object_object_add(body, LGUP_WAKEUP, wakeup);

    return body;
}

json_object *lguplus_create_speechRecognize_json_body(void)
{
    json_object *body = NULL;
    json_object *speechRecognize = NULL;
    json_object *location = NULL;

    body = json_object_new_object();
    if (!body)
        return NULL;

    speechRecognize = json_object_new_object();
    if (!speechRecognize) {
        json_object_put(body);
        return NULL;
    }

    location = json_object_new_object();
    if (!location) {
        json_object_put(body);
        json_object_put(speechRecognize);
        return NULL;
    }

    json_object_object_add(body, LGUP_SPEECHRECOGNIZE, speechRecognize);

    json_object_object_add(speechRecognize, LGUP_LANGUAGE, json_object_new_string("ko-KR"));
    json_object_object_add(speechRecognize, LGUP_LOCATION, location);

    json_object_object_add(location, LGUP_LOGNITUDE, json_object_new_string("126.963538"));
    json_object_object_add(location, LGUP_LATITUDE, json_object_new_string("37.523808"));

    return body;
}

json_object *lguplus_create_textRecognize_json_body(void)
{
    json_object *body = NULL;
    json_object *textRecognize = NULL;
    json_object *location = NULL;

    body = json_object_new_object();
    if (!body)
        return NULL;

    textRecognize = json_object_new_object();
    if (!textRecognize) {
        json_object_put(body);
        return NULL;
    }

    location = json_object_new_object();
    if (!location) {
        json_object_put(body);
        json_object_put(textRecognize);
        return NULL;
    }

    json_object_object_add(body, LGUP_TEXTRECOGNIZE, textRecognize);

    json_object_object_add(textRecognize, LGUP_FLOWTYPE, json_object_new_string("TTS"));
    json_object_object_add(textRecognize, LGUP_TEXT,
                           json_object_new_string("Now Seoul Weather Yeah."));
    json_object_object_add(textRecognize, LGUP_LOCATION, location);

    json_object_object_add(location, LGUP_LOGNITUDE, json_object_new_string("123.1233123"));
    json_object_object_add(location, LGUP_LATITUDE, json_object_new_string("23.1233123"));

    return body;
}

json_object *lguplus_create_nextRecognizeException_json_body(void)
{
    json_object *body = NULL;
    json_object *nextRecognizeException = NULL;

    body = json_object_new_object();
    if (!body)
        return NULL;

    nextRecognizeException = json_object_new_object();
    if (!nextRecognizeException) {
        json_object_put(body);
        return NULL;
    }

    json_object_object_add(body, LGUP_NEXT_RECOGNIZE_EXCEPTION, nextRecognizeException);

    json_object_object_add(nextRecognizeException, LGUP_TYPE, json_object_new_string("TIME_OUT"));

    return body;
}

json_object *lguplus_create_contextExit_json_body(void)
{
    json_object *body = NULL;
    json_object *contextExit = NULL;

    body = json_object_new_object();
    if (!body)
        return NULL;

    contextExit = json_object_new_object();
    if (!contextExit) {
        json_object_put(body);
        return NULL;
    }

    json_object_object_add(body, LGUP_CONTEXT_EXIT, contextExit);

    return body;
}

json_object *lguplus_create_speechSynthesize_json_body(void)
{
    json_object *body = NULL;
    json_object *speechSynthesize = NULL;

    body = json_object_new_object();
    if (!body)
        return NULL;

    speechSynthesize = json_object_new_object();
    if (!speechSynthesize) {
        json_object_put(body);
        return NULL;
    }

    json_object_object_add(body, LGUP_SPEECHSYNTHESIZE_M, speechSynthesize);

    return body;
}

json_object *lguplus_create_ping_json_body(void)
{
    json_object *body = NULL;
    json_object *ping = NULL;

    body = json_object_new_object();
    if (!body)
        return NULL;

    ping = json_object_new_object();
    if (!ping) {
        json_object_put(body);
        return NULL;
    }

    json_object_object_add(body, LGUP_PING, ping);

    return body;
}

json_object *lguplus_create_downstream_json_body(char *device_token)
{
    json_object *body = NULL;
    json_object *synchronize = NULL;
    json_object *client_info = NULL;
    char deviceToken[64] = {0};

    if (!device_token)
        return NULL;

    body = json_object_new_object();
    if (!body)
        return NULL;

    synchronize = json_object_new_object();
    if (!synchronize) {
        json_object_put(body);
        return NULL;
    }

    client_info = json_object_new_object();
    if (!client_info) {
        json_object_put(body);
        json_object_put(synchronize);
        return NULL;
    }

    snprintf(deviceToken, sizeof(deviceToken), VAL_DEVICE_TOKEN, device_token);
    json_object_object_add(synchronize, LGUP_DEVICETOKEN, json_object_new_string(deviceToken));

    json_object_object_add(client_info, LGUP_ACCESSINFO, json_object_new_string("SPEAKER"));
    json_object_object_add(client_info, LGUP_OSINFO, json_object_new_string("linux_4.9.68"));
    json_object_object_add(client_info, LGUP_NETWORKINFO, json_object_new_string("WIFI"));
    json_object_object_add(client_info, LGUP_DEVICEMODEL, json_object_new_string("BPS-1000"));
    json_object_object_add(client_info, LGUP_CARRIER, json_object_new_string("L"));
    json_object_object_add(client_info, LGUP_APNAME, json_object_new_string("zolo_router"));
    json_object_object_add(client_info, LGUP_DEVVERINFO, json_object_new_string("00.02.01"));

    json_object_object_add(synchronize, LGUP_CLIENTINFO, client_info);

    json_object_object_add(body, LGUP_SYNCHRONIZE, synchronize);

    return body;
}

json_object *lguplus_create_payload_body(const char *method, json_object *payload)
{
    json_object *body = NULL;

    body = json_object_new_object();
    if (!body)
        return NULL;

    json_object_object_add(body, method, payload);

    return body;
}

char *lguplus_create_request_data(char *cmd_json, int *type)
{
    json_object *json_header = NULL;
    json_object *json_body = NULL;
    json_object *json_data = NULL;
    json_object *cmd_js = NULL;
    json_object *cmd_body = NULL;
    char *schema = NULL;
    char *method = NULL;
    char *message_id = NULL;
    char *transaction_id = NULL;
    char message_id_str[128] = {0};
    char transaction_id_str[128] = {0};
    char *data = NULL;
    char *request_data = NULL;
    char *device_token = NULL;

    ASR_PRINT(ASR_DEBUG_INFO, "+++++ start at %lld\n", tickSincePowerOn());

    if (!cmd_json || !type) {
        ASR_PRINT(ASR_DEBUG_ERR, "cmd_json is NULL at %lld\n", tickSincePowerOn());
        goto Exit;
    }

    device_token = lguplus_get_device_token();
    if (!device_token) {
        ASR_PRINT(ASR_DEBUG_ERR, "----- device_token not ready +++ at %lld\n", tickSincePowerOn());
        goto Exit;
    }

    cmd_js = json_tokener_parse(cmd_json);
    if (!cmd_js) {
        ASR_PRINT(ASR_DEBUG_ERR, "cmd_js is not a json:\n%s\n", cmd_json);
        goto Exit;
    }

    schema = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_SCHEMA);
    method = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_METHOD);
    message_id = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_MESSAGE_ID);
    transaction_id = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_TRANSACTION_ID);
    cmd_body = json_object_object_get(cmd_js, KEY_BODY);

    if (!schema || !method) {
        ASR_PRINT(ASR_DEBUG_ERR, "method not found\n");
        goto Exit;
    }

    if (message_id == NULL || transaction_id == NULL || strlen(message_id) == 0 ||
        strlen(transaction_id) == 0) {
        lguplus_create_transactionId(transaction_id_str, sizeof(transaction_id_str), message_id_str,
                                     sizeof(message_id_str), device_token);
        message_id = message_id_str;
        transaction_id = transaction_id_str;
    }

    *type = e_event;
    json_header = lguplus_create_json_header(schema, method, transaction_id, message_id);
    if (StrEq(schema, LGUP_HEARTBEAT)) {
        if (StrEq(method, LGUP_PING)) {
            *type = e_ping;
            json_body = lguplus_create_ping_json_body();
        }
    } else if (StrEq(schema, LGUP_SYSTEM)) {
        if (StrEq(method, LGUP_SYNCHRONIZE)) {
            *type = e_downchannel;
            json_body = lguplus_create_downstream_json_body(device_token);
        } else if (StrEq(method, LGUP_INACTIVE_TIME)) {
            *type = e_event;
            json_body = lguplus_create_inactive_json_body();
        }
    } else if (StrEq(schema, LGUP_RECOGNIZE)) {
        if (StrEq(method, LGUP_SPEECHRECOGNIZE)) {
            *type = e_recognize;
            json_body = lguplus_create_speechRecognize_json_body();
        } else if (StrEq(method, LGUP_WAKEUP)) {
            json_body = lguplus_create_wakeup_json_body();
        } else if (StrEq(method, LGUP_TEXTRECOGNIZE)) {
            json_body = lguplus_create_textRecognize_json_body();
        } else if (StrEq(method, LGUP_NEXT_RECOGNIZE_EXCEPTION)) {
            json_body = lguplus_create_nextRecognizeException_json_body();
        } else if (StrEq(method, LGUP_CONTEXT_EXIT)) {
            json_body = lguplus_create_contextExit_json_body();
        }
    } else if (StrEq(schema, LGUP_SPEECHSYNTHESIZE)) {
        if (StrEq(method, LGUP_SPEECHSYNTHESIZE_M)) {
            *type = e_tts;
            json_body = lguplus_create_speechSynthesize_json_body();
        } else if (StrEq(method, LGUP_STATUS)) {
            if (cmd_body) {
                char *status = JSON_GET_STRING_FROM_OBJECT(cmd_body, KEY_STATUS);
                if (status) {
                    json_body = lguplus_create_status_json_body(status);
                }
            }
        }
    } else if (StrEq(schema, LGUP_AUDIO)) {
        if (cmd_body) {
            json_body = json_object_object_dup(cmd_body);
        }
    } else if (StrEq(schema, LGUP_ALERT)) {
        if (cmd_body) {
            json_body = lguplus_create_payload_body(method, cmd_body);
        }
    } else if (StrEq(schema, LGUP_BUTTON)) {
        if (cmd_body) {
            json_body = lguplus_create_payload_body(method, cmd_body);
        }
    }

    if (json_header && json_body) {
        json_data = lguplus_create_json_data(json_header, json_body);
        if (json_data) {
            data = json_object_to_json_string(json_data);
            if (data) {
                request_data = strdup(data);
            }
        }
    }

#if defined(UPLUS_DEBUG_LOG)
    ASR_PRINT(ASR_DEBUG_TRACE, "[UPLUS (%s-%s)] %s\n", schema, method, request_data);
#else
    ASR_PRINT(ASR_DEBUG_TRACE, "[UPLUS (%s-%s)]\n", schema, method);
#endif

Exit:
    if (device_token) {
        free(device_token);
    }

    if (cmd_js) {
        json_object_put(cmd_js);
    }

    if (json_data) {
        json_object_put(json_data);
    } else {
        if (json_header) {
            json_object_put(json_header);
        }
        if (json_body) {
            json_object_put(json_body);
        }
    }

    return request_data;
}
