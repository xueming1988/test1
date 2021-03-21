
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "json.h"

#include "asr_cfg.h"
#include "asr_debug.h"
#include "asr_system.h"
#include "asr_json_common.h"
#include "asr_bluetooth.h"
#include "asr_player.h"

#include "clova_dnd.h"
#include "clova_device.h"
#include "clova_device_control.h"

static int clova_parse_decrease(json_object *json_payload)
{
    int ret = -1;
    char *target = NULL;

    if (json_payload == NULL) {
        goto EXIT;
    }

    target = JSON_GET_STRING_FROM_OBJECT(json_payload, CLOVA_PAYLOAD_NAME_TARGET);

    if (StrEq(target, CLOVA_PAYLOAD_VALUE_VOLUME)) {
        ret = asr_player_adjust_volume(-1);
        if (ret) {
            clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_FAILED,
                                            CLOVA_PAYLOAD_NAME_VOLUME, CLOVA_HEADER_NAME_DECREASE);
        } else {
            clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_EXECUTED,
                                            CLOVA_PAYLOAD_NAME_VOLUME, CLOVA_HEADER_NAME_DECREASE);
        }
    } else if (StrEq(target, CLOVA_PAYLOAD_VALUE_SCREEN_BRIGHTNESS)) {

    } else if (StrEq(target, CLOVA_PAYLOAD_VALUE_CHANNEL)) {
    }

EXIT:

    return ret;
}

static int clova_parse_expect_report_state(json_object *json_payload)
{
    int ret = -1;
    pthread_t clova_device_control_report_state_tid = 0;
    pthread_attr_t attr;

    if (json_payload == NULL) {
        goto EXIT;
    }

    int durationInSeconds =
        JSON_GET_INT_FROM_OBJECT(json_payload, CLOVA_PAYLOAD_NAME_DURATION_IN_SECONDS);
    int intervalInSeconds =
        JSON_GET_INT_FROM_OBJECT(json_payload, CLOVA_PAYLOAD_NAME_INTERVAL_IN_SECONDS);

    clova_device_control_report_cmd(CLOVA_HEADER_NAME_REPORT_STATE);

    if (durationInSeconds >= 0 && intervalInSeconds > 0) {
        ASR_PRINT(ASR_DEBUG_ERR, "durationInSeconds=%d intervalInSeconds=%d\n", durationInSeconds,
                  intervalInSeconds);
        asr_system_add_report_state(durationInSeconds, intervalInSeconds);
    }

EXIT:

    return ret;
}

static int clova_parse_increase(json_object *json_payload)
{
    int ret = -1;
    char *target = NULL;

    if (json_payload == NULL) {
        goto EXIT;
    }

    target = JSON_GET_STRING_FROM_OBJECT(json_payload, CLOVA_PAYLOAD_NAME_TARGET);

    if (StrEq(target, CLOVA_PAYLOAD_VALUE_VOLUME)) {
        ret = asr_player_adjust_volume(1);
        if (ret) {
            clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_FAILED,
                                            CLOVA_PAYLOAD_NAME_VOLUME, CLOVA_HEADER_NAME_INCREASE);
        } else {
            clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_EXECUTED,
                                            CLOVA_PAYLOAD_NAME_VOLUME, CLOVA_HEADER_NAME_INCREASE);
        }
    } else if (StrEq(target, CLOVA_PAYLOAD_VALUE_SCREEN_BRIGHTNESS)) {

    } else if (StrEq(target, CLOVA_PAYLOAD_VALUE_CHANNEL)) {
    }

EXIT:

    return ret;
}

static int clova_parse_launch_app(json_object *json_payload)
{
    int ret = -1;
    char *target = NULL;

    if (json_payload == NULL) {
        goto EXIT;
    }

    target = JSON_GET_STRING_FROM_OBJECT(json_payload, CLOVA_PAYLOAD_NAME_TARGET);

EXIT:

    return ret;
}

static int clova_parse_open_screen(json_object *json_payload)
{
    int ret = -1;
    char *target = NULL;

    if (json_payload == NULL) {
        goto EXIT;
    }

    target = JSON_GET_STRING_FROM_OBJECT(json_payload, CLOVA_PAYLOAD_NAME_TARGET);

EXIT:

    return ret;
}

static int clova_parse_set_value(json_object *json_payload)
{
    int ret = -1;
    char *target = NULL;
    char *value = NULL;
    int index = 0;

    if (json_payload == NULL) {
        goto EXIT;
    }

    target = JSON_GET_STRING_FROM_OBJECT(json_payload, CLOVA_PAYLOAD_NAME_TARGET);
    value = JSON_GET_STRING_FROM_OBJECT(json_payload, CLOVA_PAYLOAD_NAME_VALUE);

    if (value && target) {
        if (StrEq(target, CLOVA_PAYLOAD_VALUE_VOLUME)) {
            unsigned char vol_step[16] = {0,  6,  13, 20, 27, 33, 40, 47,
                                          53, 60, 67, 73, 80, 87, 93, 100};
            index = atoi(value);

            ret = asr_player_set_volume(0, vol_step[index]);
            if (ret) {
                clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_FAILED,
                                                CLOVA_PAYLOAD_NAME_VOLUME,
                                                CLOVA_HEADER_NAME_SET_VALUE);
            } else {
                clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_EXECUTED,
                                                CLOVA_PAYLOAD_NAME_VOLUME,
                                                CLOVA_HEADER_NAME_SET_VALUE);
            }
        } else if (StrEq(target, CLOVA_PAYLOAD_VALUE_SCREEN_BRIGHTNESS)) {

        } else if (StrEq(target, CLOVA_PAYLOAD_VALUE_CHANNEL)) {
        }
    }

EXIT:

    return ret;
}

static int clova_parse_synchronize_state(json_object *json_payload)
{
    int ret = -1;
    char *deviceid = NULL;
    json_object *deviceState = NULL;

    if (json_payload == NULL) {
        goto EXIT;
    }

    deviceid = JSON_GET_STRING_FROM_OBJECT(json_payload, CLOVA_PAYLOAD_NAME_DEVICE_ID);
    deviceState = json_object_object_get(json_payload, CLOVA_PAYLOAD_NAME_DEVICE_STATE);

EXIT:

    return ret;
}

static int clova_parse_turn_off(json_object *json_payload)
{
    int ret = -1;
    char *target = NULL;

    if (json_payload == NULL) {
        goto EXIT;
    }

    target = JSON_GET_STRING_FROM_OBJECT(json_payload, CLOVA_PAYLOAD_NAME_TARGET);

    if (StrEq(target, CLOVA_PAYLOAD_NAME_BLUETOOTH)) {
        // turn off the bt
        ret = asr_bluetooth_switch(0);
        if (ret) {
            clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_FAILED,
                                            CLOVA_PAYLOAD_NAME_BLUETOOTH,
                                            CLOVA_HEADER_NAME_TURN_OFF);
        } else {
            clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_EXECUTED,
                                            CLOVA_PAYLOAD_NAME_BLUETOOTH,
                                            CLOVA_HEADER_NAME_TURN_OFF);
        }
    } else if (StrEq(target, CLOVA_PAYLOAD_NAME_MICROPHONE)) {
        // turn off the microphone
        ret = asr_set_privacy_mode(e_mode_on);
        if (ret) {
            clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_FAILED,
                                            CLOVA_PAYLOAD_NAME_MICROPHONE,
                                            CLOVA_HEADER_NAME_TURN_OFF);
        } else {
            clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_EXECUTED,
                                            CLOVA_PAYLOAD_NAME_MICROPHONE,
                                            CLOVA_HEADER_NAME_TURN_OFF);
        }
    }

EXIT:

    return ret;
}

static int clova_parse_turn_on(json_object *json_payload)
{
    int ret = -1;
    char *target = NULL;

    if (json_payload == NULL) {
        goto EXIT;
    }

    target = JSON_GET_STRING_FROM_OBJECT(json_payload, CLOVA_PAYLOAD_NAME_TARGET);

    if (StrEq(target, CLOVA_PAYLOAD_NAME_BLUETOOTH)) {
        // turn on the bt
        ret = asr_bluetooth_switch(1);
        if (ret) {
            clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_FAILED,
                                            CLOVA_PAYLOAD_NAME_BLUETOOTH,
                                            CLOVA_HEADER_NAME_TURN_ON);
        } else {
            clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_EXECUTED,
                                            CLOVA_PAYLOAD_NAME_BLUETOOTH,
                                            CLOVA_HEADER_NAME_TURN_ON);
        }
    }

EXIT:

    return ret;
}

static int clova_parse_set_dnd(json_object *json_payload)
{
    int ret = -1;
    char *expired_at = NULL;

    if (json_payload == NULL) {
        goto EXIT;
    }

    expired_at = JSON_GET_STRING_FROM_OBJECT(json_payload, CLOVA_PAYLOAD_DND_ACTION_EXPIREDAT);
    if (expired_at) {
        ret = clova_set_dnd_expire_time(expired_at);
        if (ret) {
            clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_FAILED, CLOVA_PAYLOAD_NAME_DND,
                                            CLOVA_HEADER_NAME_SETDND);
        } else {
            clova_cfg_update_dnd_expire_time(expired_at);
            clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_EXECUTED,
                                            CLOVA_PAYLOAD_NAME_DND, CLOVA_HEADER_NAME_SETDND);
        }
    }

EXIT:

    return ret;
}

static int clova_parse_cancel_dnd(json_object *json_payload)
{
    int ret = -1;
    char *target = NULL;

    if (json_payload == NULL) {
        goto EXIT;
    }

    ret = clova_clear_dnd_expire_time();
    if (ret) {
        clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_FAILED, CLOVA_PAYLOAD_NAME_DND,
                                        CLOVA_HEADER_NAME_CANCELDND);
    } else {
        clova_cfg_update_dnd_expire_time("");
        clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_EXECUTED, CLOVA_PAYLOAD_NAME_DND,
                                        CLOVA_HEADER_NAME_CANCELDND);
    }

EXIT:

    return ret;
}

int clova_parse_device_control_directive(json_object *json_directive)
{
    int ret = -1;
    char *name = NULL;
    json_object *json_header = NULL;
    json_object *json_payload = NULL;

    if (json_directive == NULL) {
        goto EXIT;
    }

    json_header = json_object_object_get(json_directive, KEY_HEADER);
    if (json_header == NULL) {
        goto EXIT;
    }

    json_payload = json_object_object_get(json_directive, KEY_PAYLOAD);
    if (json_payload == NULL) {
        goto EXIT;
    }

    name = JSON_GET_STRING_FROM_OBJECT(json_header, KEY_NAME);

    if ((StrEq(name, CLOVA_HEADER_NAME_BT_CONNECT)) ||
        (StrEq(name, CLOVA_HEADER_NAME_BT_CONNECT_BY_PIN_CODE)) ||
        (StrEq(name, CLOVA_HEADER_NAME_BT_DISCONNECT)) ||
        (StrEq(name, CLOVA_HEADER_NAME_BT_START_PAIRING)) ||
        (StrEq(name, CLOVA_HEADER_NAME_BT_STOP_PAIRING)) ||
        (StrEq(name, CLOVA_HEADER_NAME_BT_RESCAN)) || (StrEq(name, CLOVA_HEADER_NAME_BT_DELETE)) ||
        (StrEq(name, CLOVA_HEADER_NAME_BT_PLAY))) {
        ret = asr_bluetooth_parse_directive(json_directive);
    } else if (StrEq(name, CLOVA_HEADER_NAME_DECREASE)) {
        ret = clova_parse_decrease(json_payload);
    } else if (StrEq(name, CLOVA_HEADER_NAME_EXPECT_REPORT_STATE)) {
        ret = clova_parse_expect_report_state(json_payload);
    } else if (StrEq(name, CLOVA_HEADER_NAME_INCREASE)) {
        ret = clova_parse_increase(json_payload);
    } else if (StrEq(name, CLOVA_HEADER_NAME_LAUNCH_APP)) {
        ret = clova_parse_launch_app(json_payload);
    } else if (StrEq(name, CLOVA_HEADER_NAME_OPEN_SCREEN)) {
        ret = clova_parse_open_screen(json_payload);
    } else if (StrEq(name, CLOVA_HEADER_NAME_SET_VALUE)) {
        ret = clova_parse_set_value(json_payload);
    } else if (StrEq(name, CLOVA_HEADER_NAME_SYNCHRONIZE_STATE)) {
        ret = clova_parse_synchronize_state(json_payload);
    } else if (StrEq(name, CLOVA_HEADER_NAME_TURN_OFF)) {
        ret = clova_parse_turn_off(json_payload);
    } else if (StrEq(name, CLOVA_HEADER_NAME_TURN_ON)) {
        ret = clova_parse_turn_on(json_payload);
    } else if (StrEq(name, CLOVA_HEADER_NAME_SETDND)) {
        ret = clova_parse_set_dnd(json_payload);
    } else if (StrEq(name, CLOVA_HEADER_NAME_CANCELDND)) {
        ret = clova_parse_cancel_dnd(json_payload);
    }

EXIT:

    return ret;
}
