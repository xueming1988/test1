#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "json.h"

#include "asr_debug.h"
#include "asr_cmd.h"
#include "asr_player.h"
#include "asr_connect.h"
#include "asr_light_effects.h"
#include "asr_session.h"
#include "common_flags.h"

#include "lguplus_result_code.h"
#include "lguplus_json_common.h"
#include "lguplus_event_queue.h"
#include "lguplus_alert.h"

char *g_service_name = NULL;

static void stop_recognize(void)
{
    if (get_asr_talk_start() == 0) {
        char buff[64] = {0};
        snprintf(buff, sizeof(buff), TLK_STOP_N, 0);
        NOTIFY_CMD(ASR_TTS, buff);
        asr_light_effects_set(e_effect_idle);
        common_focus_manage_clear(e_focus_recognize);
    }
}

static int handle_stop_directive(void)
{
    NOTIFY_CMD(ASR_TTS, VAD_FINISHED);
    common_focus_manage_clear(e_focus_recognize);

    return 0;
}

static int handle_default_result_directive(json_object *json_header, json_object *json_body)
{
    json_object *json_result = NULL;
    json_object *json_tts = NULL;
    json_object *json_nlu = NULL;
    json_object *json_slot = NULL;
    char *transactionId = NULL;
    char *messageId = NULL;
    char *skill = NULL;
    char *intent = NULL;
    char *volumeVal = NULL;
    char *muteStatus = NULL;
    int volume = 0;
    int ret = -1;

    if (json_header && json_body) {
        json_result = json_object_object_get(json_body, LGUP_DEFAULT_RESULT);
        if (json_result) {
            json_tts = json_object_object_get(json_result, LGUP_TTS);
            if (json_tts) {
                transactionId = JSON_GET_STRING_FROM_OBJECT(json_header, LGUP_TRANSACTION_ID);
                messageId = JSON_GET_STRING_FROM_OBJECT(json_header, LGUP_MESSAGE_ID);

                if (transactionId && messageId) {
                    lguplus_event_queue_add_body(LGUP_SPEECHSYNTHESIZE, LGUP_STATUS, messageId,
                                                 transactionId, "{\"status\":\"START\"}");
                    lguplus_event_queue_add(LGUP_SPEECHSYNTHESIZE, LGUP_SPEECHSYNTHESIZE_M,
                                            messageId, transactionId);
                }
            }

            json_nlu = json_object_object_get(json_result, LGUP_NLU);
            if (json_nlu) {
                skill = JSON_GET_STRING_FROM_OBJECT(json_nlu, LGUP_SKILL);
                intent = JSON_GET_STRING_FROM_OBJECT(json_nlu, LGUP_INTENT);
                if (StrEq(skill, LGUP_VOLUME_M)) {
                    if (StrEq(intent, LGUP_SET_VOLUME)) {
                        json_slot = json_object_object_get(json_nlu, LGUP_SLOT);
                        if (json_slot) {
                            volumeVal = JSON_GET_STRING_FROM_OBJECT(json_slot, LGUP_VOLUME_VAL);
                            if (volumeVal) {
                                volume = atoi(volumeVal);
                                volume = volume * 100 / 15;
                                ret = asr_player_set_volume(0, volume);
                            }
                        }
                    } else if (StrEq(intent, LGUP_SET_MUTE)) {
                        json_slot = json_object_object_get(json_nlu, LGUP_SLOT);
                        if (json_slot) {
                            muteStatus = JSON_GET_STRING_FROM_OBJECT(json_slot, LGUP_MUTE_STATUS);
                            if (StrEq(muteStatus, "on")) {
                                ret = asr_player_set_volume(1, 1);
                            } else if (StrEq(muteStatus, "off")) {
                                ret = asr_player_set_volume(1, 0);
                            }
                        }
                    } else if (StrEq(intent, LGUP_VOLUME_UP)) {
                        volume = asr_player_get_volume(0);
                        volume += 7;
                        ret = asr_player_set_volume(0, volume);
                    } else if (StrEq(intent, LGUP_VOLUME_DOWN)) {
                        volume = asr_player_get_volume(0);
                        volume -= 7;
                        ret = asr_player_set_volume(0, volume);
                    }
                } else if (StrEq(skill, "genie")) {
                    if (StrEq(intent, "nextMusic")) {
                        asr_player_set_next();
                    } else if (StrEq(intent, "prevMusic")) {
                        asr_player_set_previous();
                    } else if (StrEq(intent, "resumeMusic")) {
                        asr_player_set_resume();
                    } else if (StrEq(intent, "playMusic")) {
                        asr_player_set_resume();
                    }
                }
            }
        }
    }

    return ret;
}

static int handle_stt_directive(json_object *json_body)
{
    json_object *json_stt;
    char *text = NULL;

    if (!json_body)
        return -1;

    json_stt = json_object_object_get(json_body, LGUP_STT);
    if (!json_stt)
        return -1;

    text = JSON_GET_STRING_FROM_OBJECT(json_stt, LGUP_TEXT);

    return 0;
}

static int handle_tts_directive(json_object *json_body)
{
    json_object *json_tts;

    if (!json_body)
        return -1;

    json_tts = json_object_object_get(json_body, LGUP_TTS);
    if (!json_tts)
        return -1;

    return 0;
}

static int handle_metaDelivery_directive(json_object *json_body)
{
    json_object *json_metaDelivery;

    if (!json_body)
        return -1;

    json_metaDelivery = json_object_object_get(json_body, LGUP_META_DELIVERY);
    if (!json_metaDelivery)
        return -1;

    return 0;
}

static int handle_speech_synthesize_directive(void) { return 0; }

static int handle_play_directive(json_object *json_body)
{
    json_object *json_play = NULL;
    json_object *json_play_list = NULL;
    json_object *json_array = NULL;
    json_object *json_item = NULL;
    json_object *json_meta = NULL;
    char *play_behavior = NULL;
    char *report_interval = NULL;
    char *item_id = NULL;
    char *url = NULL;
    char *serviceName = NULL;
    char *category_id = NULL;
    int play_index = 0;
    int play_list_count = 0;
    int genie_online = -1;
    int i;

    if (!json_body)
        return -1;

    json_play = json_object_object_get(json_body, LGUP_PLAY);
    if (!json_play)
        return -1;

    play_behavior = JSON_GET_STRING_FROM_OBJECT(json_play, LGUP_PLAY_BEHAVIOR);
    report_interval = JSON_GET_STRING_FROM_OBJECT(json_play, LGUP_REPORT_INTERVAL);
    serviceName = JSON_GET_STRING_FROM_OBJECT(json_play, LGUP_PLAY_SERVICE_NAME);
    play_index = JSON_GET_INT_FROM_OBJECT(json_play, LGUP_PLAY_INDEX);
    play_list_count = JSON_GET_INT_FROM_OBJECT(json_play, LGUP_PLAY_LIST_COUNT);
    if (serviceName) {
        if (g_service_name) {
            free(g_service_name);
        }
        g_service_name = strdup(serviceName);
    }

    json_play_list = json_object_object_get(json_play, LGUP_PLAYLIST);

    if (play_list_count == 1) {
        json_array = json_object_array_get_idx(json_play_list, 0);
        json_item = json_object_object_get(json_array, LGUP_ITEM);

        item_id = JSON_GET_STRING_FROM_OBJECT(json_item, LGUP_ITEMID);
        url = JSON_GET_STRING_FROM_OBJECT(json_item, LGUP_URL);
        if (!url && StrEq(g_service_name, "genie")) {
            genie_online = lguplus_check_genie_login();
            if (genie_online) {
                lguplus_genie_server_login();
            }
            url = lguplus_genie_get_url(item_id);
        } else if (!report_interval && StrEq(g_service_name, "RADIO")) {
            return -1;
        }

        lguplus_play_audio(play_behavior, report_interval, item_id, url);
    } else if (play_list_count > 1) {
        for (i = 0; i < play_list_count; i++) {
            json_array = json_object_array_get_idx(json_play_list, i);
            json_item = json_object_object_get(json_array, LGUP_ITEM);

            item_id = JSON_GET_STRING_FROM_OBJECT(json_item, LGUP_ITEMID);
            url = JSON_GET_STRING_FROM_OBJECT(json_item, LGUP_URL);

            if (i == 0) {
                lguplus_play_audio(play_behavior, report_interval, item_id, url);
            } else {
                lguplus_play_audio("ENQUEUE", report_interval, item_id, url);
            }
        }
    }

    return 0;
}

/*
{
    "header": {
        "schema": "Audio",
        "nethod": "playStop",
        "transactionId": "TR_0000000000000001",
        "messageId": "MSG_000000000001"
    },
    "deviceControl": {
        "beep": "",
        "led": ""
    },
    "body": {
        "playStop": {
            "stopType": "STOP"
        }
    }
}
*/
static int handle_play_stop_directive(json_object *json_body)
{
    int ret = -1;
    json_object *json_play_stop = NULL;
    char *stop_type = NULL;

    if (!json_body)
        return -1;

    json_play_stop = json_object_object_get(json_body, LGUP_PLAY_STOP);
    if (!json_play_stop)
        return -1;

    stop_type = JSON_GET_STRING_FROM_OBJECT(json_play_stop, LGUP_PLAY_STOP_TYPE);
    if (StrEq(stop_type, LGUP_TYPE_STOP)) {
        ret = asr_player_set_pause();
    } else if (StrEq(stop_type, LGUP_TYPE_PAUSED)) {
        ret = asr_player_set_pause();
    }

    return ret;
}

static int handle_play_resume_directive(json_object *json_body)
{
    int ret = -1;
    json_object *json_play_resume = NULL;

    if (!json_body)
        return -1;

    json_play_resume = json_object_object_get(json_body, LGUP_PLAY_RESUME);
    if (!json_play_resume)
        return -1;

    return ret;
}

static int handle_play_clear_queue_directive(json_object *json_body)
{
    int ret = -1;
    json_object *json_play_clear_queue = NULL;
    char *clearMode = NULL;

    if (!json_body)
        return -1;

    json_play_clear_queue = json_object_object_get(json_body, LGUP_PLAY_CLEAR_QUEUE);
    if (!json_play_clear_queue)
        return -1;

    clearMode = JSON_GET_STRING_FROM_OBJECT(json_play_clear_queue, LGUP_PLAY_CLEAR_MODE);
    if (StrEq(clearMode, LGUP_TYPE_CLEAR_ENQUE)) {

    } else if (StrEq(clearMode, LGUP_TYPE_CLEAR_ALL)) {
    }

    return ret;
}

static int handle_alert_add_directive(json_object *json_body)
{
    json_object *json_add = NULL;
    json_object *alarmList = NULL;
    json_object *alarm_item = NULL;
    LGUPLUS_ALERT_T alert_info;
    int array_item = 0;
    int i = 0;

    if (json_body) {
        json_add = json_object_object_get(json_body, LGUP_ALERT_ADD);
        if (json_add) {
            alarmList = json_object_object_get(json_add, LGUP_ALERT_ALARM_LIST);
            array_item = json_object_array_length(alarmList);
            for (; i < array_item; i++) {
                alarm_item = json_object_array_get_idx(alarmList, i);
                if (alarm_item) {
                    alert_info.alertId =
                        JSON_GET_STRING_FROM_OBJECT(alarm_item, LGUP_ALERT_ALERT_ID);
                    alert_info.alarmTitle =
                        JSON_GET_STRING_FROM_OBJECT(alarm_item, LGUP_ALERT_ALARM_TITLE);
                    alert_info.activeYn =
                        JSON_GET_STRING_FROM_OBJECT(alarm_item, LGUP_ALERT_ACTIVE_YN);
                    alert_info.volume = JSON_GET_STRING_FROM_OBJECT(alarm_item, LGUP_ALERT_VOLUME);
                    alert_info.setDate =
                        JSON_GET_STRING_FROM_OBJECT(alarm_item, LGUP_ALERT_SET_DATA);
                    alert_info.setTime =
                        JSON_GET_STRING_FROM_OBJECT(alarm_item, LGUP_ALERT_SET_TIME);
                    alert_info.repeatYn =
                        JSON_GET_STRING_FROM_OBJECT(alarm_item, LGUP_ALERT_REPEAT_YN);
                    alert_info.repeat = JSON_GET_STRING_FROM_OBJECT(alarm_item, LGUP_ALERT_REPEAT);

                    lguplus_add_alert(&alert_info);
                }
            }
        }
    }

    return 0;
}

static int handle_alert_delete_directive(json_object *json_body)
{
    json_object *json_delete = NULL;
    char *alertId = NULL;
    char *isEntire = NULL;

    if (json_body) {
        json_delete = json_object_object_get(json_body, LGUP_ALERT_DELETE);
        if (json_delete) {
            alertId = JSON_GET_STRING_FROM_OBJECT(json_delete, LGUP_ALERT_ALERT_ID);
            isEntire = JSON_GET_STRING_FROM_OBJECT(json_delete, LGUP_ALERT_IS_ENTIRE);

            lguplus_delete_alert(alertId, isEntire);
        }
    }

    return 0;
}

static int handle_alert_update_directive(json_object *json_body)
{
    json_object *json_delete = NULL;
    char *isEntire = NULL;
    char *alertId = NULL;
    char *activeYn = NULL;

    if (json_body) {
        json_delete = json_object_object_get(json_body, LGUP_ALERT_UPDATE);
        if (json_delete) {
            isEntire = JSON_GET_STRING_FROM_OBJECT(json_delete, LGUP_ALERT_IS_ENTIRE);
            alertId = JSON_GET_STRING_FROM_OBJECT(json_delete, LGUP_ALERT_ALERT_ID);
            activeYn = JSON_GET_STRING_FROM_OBJECT(json_delete, LGUP_ALERT_ACTIVE_YN);

            lguplus_update_alert(isEntire, alertId, activeYn);
        }
    }

    return 0;
}

static int handle_change_volume_directive(json_object *json_body)
{
    json_object *json_changeVolume = NULL;
    char *volume = NULL;
    char *volumeType = NULL;

    if (!json_body)
        return -1;

    json_changeVolume = json_object_object_get(json_body, LGUP_CHANGE_VOLUME);
    if (!json_changeVolume)
        return -1;

    volume = JSON_GET_STRING_FROM_OBJECT(json_changeVolume, LGUP_VOLUME_M);
    volumeType = JSON_GET_STRING_FROM_OBJECT(json_changeVolume, LGUP_VOLUME_TYPE);

    return 0;
}

static int handle_mute_directive(json_object *json_body)
{
    json_object *json_recording = NULL;
    char *time = NULL;

    if (!json_body)
        return -1;

    json_recording = json_object_object_get(json_body, LGUP_RECORDING);
    if (!json_recording)
        return -1;

    time = JSON_GET_STRING_FROM_OBJECT(json_recording, LGUP_TIME);

    return 0;
}

static int handle_recording_directive(json_object *json_body)
{
    json_object *json_mute = NULL;
    char *status = NULL;

    if (!json_body)
        return -1;

    json_mute = json_object_object_get(json_body, LGUP_MUTE);
    if (!json_mute)
        return -1;

    status = JSON_GET_STRING_FROM_OBJECT(json_mute, LGUP_STATUS);

    return 0;
}

static int handle_setting_directive(json_object *json_body)
{
    json_object *json_setting = NULL;
    json_object *json_reset_data = NULL;
    json_object *json_tmp = NULL;
    char *all_reset = NULL;
    char *key = NULL;
    char *value = NULL;
    int total_index = 0;
    int i = 0;
    int ret = -1;

    if (!json_body)
        return ret;

    json_setting = json_object_object_get(json_body, LGUP_SETTING_M);
    if (json_setting) {
        all_reset = JSON_GET_STRING_FROM_OBJECT(json_setting, LGUP_ALL_RESET);
        if (StrEq(all_reset, "Y")) {

        } else if (StrEq(all_reset, "N")) {
            json_reset_data = json_object_object_get(json_setting, LGUP_RESET_DATA);
            if (json_reset_data) {
                total_index = json_object_array_length(json_reset_data);
                for (; i < total_index; i++) {
                    json_tmp = json_object_array_get_idx(json_reset_data, i);
                    if (json_tmp) {
                        key = JSON_GET_STRING_FROM_OBJECT(json_tmp, LGUP_KEY);
                        value = JSON_GET_STRING_FROM_OBJECT(json_tmp, LGUP_VALUE);
                        ret = lguplus_setting_apply(key, value);
                    }
                }
            }
        }
    }

    return ret;
}

/*
{
    "header": {
        "schema": "Recognize",
        "method": "stop",
        "transactionId": "TR_20180723120048097_SVC_001_f880a2a177f74d8da2867f29983ea329",
        "messageId": "MSG_20180723120048097_SVC_001_f880a2a177f74d8da2867f29983ea329"
    },
    "deviceControl": {},
    "body": {
        "stop": {
            "status": "NO_RESULT"
        }
    }
}
*/
static int parse_recongize_series_message(char *method, json_object *json_body)
{
    if (!method || !json_body)
        return -1;

    if (StrEq(method, LGUP_STOP)) {
        json_object *json_status = json_object_object_get(json_body, LGUP_STOP);
        char *status = NULL;
        if (json_status) {
            status = JSON_GET_STRING_FROM_OBJECT(json_status, LGUP_STATUS);
        }

        if (StrEq(status, "OK")) {
            handle_stop_directive();
            asr_light_effects_set(e_effect_listening_off);
        } else {
            stop_recognize();
        }
    } else if (StrEq(method, LGUP_NEXT_RECOGNIZE)) {
        json_object *json_nextRecognize = json_object_object_get(json_body, LGUP_NEXT_RECOGNIZE);
        if (json_nextRecognize) {
            char *language = JSON_GET_STRING_FROM_OBJECT(json_nextRecognize, LGUP_LANGUAGE);
            char *type = JSON_GET_STRING_FROM_OBJECT(json_nextRecognize, LGUP_TYPE);
            char *answerWordType =
                JSON_GET_STRING_FROM_OBJECT(json_nextRecognize, LGUP_ANSWER_WORD_TYPE);

            lguplus_do_talkcontinue();
        }
    }

    return 0;
}

static int parse_result_series_message(char *method, json_object *json_header,
                                       json_object *json_body)
{
    if (!method || !json_header)
        return -1;

    if (StrEq(method, LGUP_DEFAULT_RESULT)) {
        handle_default_result_directive(json_header, json_body);
    } else if (StrEq(method, LGUP_STT)) {
        handle_stt_directive(json_body);
    } else if (StrEq(method, LGUP_TTS)) {
        handle_tts_directive(json_body);
    } else if (StrEq(method, LGUP_META_DELIVERY)) {
        handle_metaDelivery_directive(json_body);
    }

    return 0;
}

static int parse_speech_synthesize_series_message(char *method)
{
    if (!method)
        return -1;

    if (StrEq(method, LGUP_SPEECHSYNTHESIZE_M)) {
        handle_speech_synthesize_directive();
    }

    return 0;
}

static int parse_audio_series_message(char *method, json_object *json_body)
{
    if (!method || !json_body)
        return -1;

    if (StrEq(method, LGUP_PLAY)) {
        handle_play_directive(json_body);
    } else if (StrEq(method, LGUP_PLAY_STOP)) {
        handle_play_stop_directive(json_body);
    } else if (StrEq(method, LGUP_PLAY_RESUME)) {
        handle_play_resume_directive(json_body);
    } else if (StrEq(method, LGUP_PLAY_CLEAR_QUEUE)) {
        handle_play_clear_queue_directive(json_body);
    }

    return 0;
}

static int parse_alert_series_message(char *method, json_object *json_body)
{
    if (!method || !json_body)
        return -1;

    if (StrEq(method, LGUP_ALERT_ADD)) {
        handle_alert_add_directive(json_body);
    } else if (StrEq(method, LGUP_ALERT_DELETE)) {
        handle_alert_delete_directive(json_body);
    } else if (StrEq(method, LGUP_ALERT_UPDATE)) {
        handle_alert_update_directive(json_body);
    }

    return 0;
}

static int parse_volume_series_message(char *method, json_object *json_body)
{
    if (!method || !json_body)
        return -1;

    if (StrEq(method, LGUP_CHANGE_VOLUME)) {
        handle_change_volume_directive(json_body);
    } else if (StrEq(method, LGUP_MUTE)) {
        handle_mute_directive(json_body);
    }

    return 0;
}

static int parse_voice_recording_series_message(char *method, json_object *json_body)
{
    if (!method || !json_body)
        return -1;

    if (StrEq(method, LGUP_RECORDING)) {
        handle_recording_directive(json_body);
    }

    return 0;
}

static int parse_setting_series_message(char *method, json_object *json_body)
{
    if (!method || !json_body)
        return -1;

    if (StrEq(method, LGUP_SETTING_M)) {
        handle_setting_directive(json_body);
    }

    return 0;
}

/*
{
    "header": {
        "transactionId": "TR_20180719140954080_SVC_001_f880a2a177f74d8da2867f29983ea329",
        "messageId": "MSG_20180719140954080_SVC_001_f880a2a177f74d8da2867f29983ea329",
        "resultCode": "AIIF20000000",
        "resultMessage": ""
    },
    "body": {}
}
*/
int lguplus_parse_directive_message(int type, char *message)
{
    json_object *json_message = NULL;
    json_object *json_header = NULL;
    json_object *json_body = NULL;
    char *schema = NULL;
    char *method = NULL;
    char *transactionId = NULL;
    char *messageId = NULL;
    char *resultCode = NULL;
    char *eofyn = NULL;

    if (!message)
        return -1;

    json_message = json_tokener_parse(message);
    if (!json_message)
        return -1;

    json_header = json_object_object_get(json_message, LGUP_HEADER);
    if (!json_header) {
        json_object_put(json_message);
        return -1;
    }

    json_body = json_object_object_get(json_message, LGUP_BODY);
    if (!json_body) {
        json_object_put(json_message);
        return -1;
    }

    transactionId = JSON_GET_STRING_FROM_OBJECT(json_header, LGUP_TRANSACTION_ID);
    messageId = JSON_GET_STRING_FROM_OBJECT(json_header, LGUP_MESSAGE_ID);
    resultCode = JSON_GET_STRING_FROM_OBJECT(json_header, LGUPLUS_RESULTCODE);
    schema = JSON_GET_STRING_FROM_OBJECT(json_header, LGUP_SCHEMA);
    method = JSON_GET_STRING_FROM_OBJECT(json_header, LGUP_METHOD);

#if defined(UPLUS_DEBUG_LOG)
    if (resultCode)
        ASR_PRINT(ASR_DEBUG_TRACE, "[UPLUS (%s)] %s\n", resultCode, message);
    else
        ASR_PRINT(ASR_DEBUG_TRACE, "[UPLUS (%s-%s)] %s\n", schema, method, message);
#else
    if (resultCode)
        ASR_PRINT(ASR_DEBUG_TRACE, "[UPLUS (%s)]\n", resultCode);
    else
        ASR_PRINT(ASR_DEBUG_TRACE, "[UPLUS (%s-%s)]\n", schema, method);
#endif

    if (StrEq(schema, LGUP_RECOGNIZE)) {
        parse_recongize_series_message(method, json_body);
    } else if (StrEq(schema, LGUP_RESULT)) {
        parse_result_series_message(method, json_header, json_body);
    } else if (StrEq(schema, LGUP_SPEECH_SYNTHESIZE)) {
        parse_speech_synthesize_series_message(method);
        if (StrEq(method, LGUP_SPEECHSYNTHESIZE_M)) {
            json_object *speechSynthesize =
                json_object_object_get(json_body, LGUP_SPEECHSYNTHESIZE_M);
            if (speechSynthesize) {
                eofyn = JSON_GET_STRING_FROM_OBJECT(speechSynthesize, "eofyn");
                if (StrEq(eofyn, "Y") && messageId && transactionId) {
                    lguplus_event_queue_add_body(LGUP_SPEECHSYNTHESIZE, LGUP_STATUS, messageId,
                                                 transactionId, "{\"status\":\"FINISHED\"}");
                }
            }
        }
    } else if (StrEq(schema, LGUP_AUDIO)) {
        parse_audio_series_message(method, json_body);
    } else if (StrEq(schema, LGUP_ALERT)) {
        parse_alert_series_message(method, json_body);
    } else if (StrEq(schema, LGUP_VOLUME)) {
        parse_volume_series_message(method, json_body);
    } else if (StrEq(schema, LGUP_VOICE_RECORDING)) {
        parse_voice_recording_series_message(method, json_body);
    } else if (StrEq(schema, LGUP_SETTING)) {
        parse_setting_series_message(method, json_body);
    } else {
        if (type == e_recognize) {
            if (StrEq(resultCode, AIIF20000000)) {
                lguplus_event_queue_add(LGUP_RECOGNIZE, LGUP_SPEECHRECOGNIZE, messageId,
                                        transactionId);
            } else if (StrEq(resultCode, AIIF51000104)) {
                stop_recognize();
            }
        } else if (type == e_event) {
            if (StrEq(resultCode, AIIF31000008)) {
                asr_session_disconncet(ASR_LGUPLUS);
            }
        }
    }

    return 0;
}
