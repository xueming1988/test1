#include <stdio.h>
#include <stdlib.h>

#include "json.h"
#include "asr_json_common.h"
#include "asr_player.h"
#include "common_flags.h"

#include "clova_dnd.h"
#include "clova_device.h"
#include "clova_device_control.h"

static int power_get_actions(json_object *power)
{
    json_object *actions = NULL;
    int ret = -1;

    if (power == NULL) {
        goto EXIT;
    }

    actions = json_object_new_array();
    if (actions == NULL) {
        goto EXIT;
    }

#if 0 // Donut didnot support power turn on/off
    ret = json_object_array_add(actions, JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_NAME_TURN_OFF));
    if(ret) {
        goto EXIT;
    }

    ret = json_object_array_add(actions, JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_NAME_TURN_ON));
    if(ret) {
        goto EXIT;
    }
#endif

    json_object_object_add(power, CLOVA_PAYLOAD_NAME_ACTIONS, actions);
    ret = 0;

EXIT:

    if (ret) {
        if (actions != NULL) {
            json_object_put(actions);
        }
    }

    return ret;
}

static int power_get_state(json_object *power)
{
    char *state = NULL;
    int ret = -1;

    if (power == NULL) {
        goto EXIT;
    }

    /* get state */
    ret = 0;

    json_object_object_add(power, CLOVA_PAYLOAD_NAME_STATE,
                           JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_VALUE_ACTIVE));

EXIT:

    return ret;
}

static int get_power_state(json_object *json_payload)
{
    json_object *power = NULL;
    int ret = -1;

    if (json_payload == NULL) {
        goto EXIT;
    }

    power = json_object_new_object();
    if (power == NULL) {
        goto EXIT;
    }

    /* actions */
    ret = power_get_actions(power);
    if (ret) {
        goto EXIT;
    }

    /* others */
    ret = power_get_state(power);
    if (ret) {
        goto EXIT;
    }

    json_object_object_add(json_payload, CLOVA_PAYLOAD_NAME_POWER, power);

EXIT:

    if (ret) {
        if (power != NULL) {
            json_object_put(power);
        }
    }

    return ret;
}

static int wifi_get_actions(json_object *wifi)
{
    json_object *actions = NULL;
    int ret = -1;

    if (wifi == NULL) {
        goto EXIT;
    }

    actions = json_object_new_array();
    if (actions == NULL) {
        goto EXIT;
    }

    ret = json_object_array_add(actions, JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_NAME_TURN_OFF));
    if (ret) {
        goto EXIT;
    }

    ret = json_object_array_add(actions, JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_NAME_TURN_ON));
    if (ret) {
        goto EXIT;
    }

    ret = 0;
    json_object_object_add(wifi, CLOVA_PAYLOAD_NAME_ACTIONS, actions);

EXIT:

    if (ret) {
        if (actions != NULL) {
            json_object_put(actions);
        }
    }

    return ret;
}

static int wifi_get_state(json_object *wifi)
{
    char *state = NULL;
    char *wifi_name = NULL;
    json_object *networks = NULL;
    json_object *wifi_item = NULL;
    int ret = -1;

    if (wifi == NULL) {
        goto EXIT;
    }

    networks = json_object_new_array();
    if (networks == NULL) {
        goto EXIT;
    }

    wifi_item = json_object_new_object();
    if (wifi_item == NULL) {
        goto EXIT;
    }

    wifi_name = get_wifi_ssid();
    json_object_object_add(wifi_item, CLOVA_PAYLOAD_NAME_WIFI_NAME,
                           JSON_OBJECT_NEW_STRING(wifi_name));
    json_object_object_add(wifi_item, CLOVA_PAYLOAD_NAME_WIFI_CONNECTED,
                           json_object_new_boolean(1));

    ret = json_object_array_add(networks, wifi_item);
    if (ret) {
        goto EXIT;
    }

    /* get state */
    ret = 0;

    json_object_object_add(wifi, CLOVA_PAYLOAD_NAME_NETWORKS, networks);
    json_object_object_add(wifi, CLOVA_PAYLOAD_NAME_STATE, JSON_OBJECT_NEW_STRING("on"));

EXIT:

    if (ret != 0) {
        if (networks) {
            json_object_put(networks);
        }
        if (wifi_item) {
            json_object_put(wifi_item);
        }
    }

    return ret;
}

/*
"wifi": {
    "actions": [],
    "networks": [{
        "name": "Meeting_Room",
        "connected": true
    }],
    "state": "on"
},
*/
static int get_wifi_state(json_object *json_payload)
{
    json_object *wifi = NULL;
    int ret = -1;

    if (json_payload == NULL) {
        goto EXIT;
    }

    wifi = json_object_new_object();
    if (wifi == NULL) {
        goto EXIT;
    }

    /* actions */
    ret = wifi_get_actions(wifi);
    if (ret) {
        goto EXIT;
    }

    /* others */
    ret = wifi_get_state(wifi);
    if (ret) {
        goto EXIT;
    }

    json_object_object_add(json_payload, CLOVA_PAYLOAD_NAME_WIFI, wifi);

EXIT:

    if (ret) {
        if (wifi != NULL) {
            json_object_put(wifi);
        }
    }

    return ret;
}

static int battery_get_actions(json_object *power)
{
    json_object *actions = NULL;
    int ret = -1;

    if (power == NULL) {
        goto EXIT;
    }

    actions = json_object_new_array();
    if (actions == NULL) {
        goto EXIT;
    }

    ret = 0;
    json_object_object_add(power, CLOVA_PAYLOAD_NAME_ACTIONS, actions);

EXIT:

    if (ret) {
        if (actions != NULL) {
            json_object_put(actions);
        }
    }

    return ret;
}

static int battery_get_state(json_object *battery)
{
    char *state = NULL;
    int ret = -1;

    if (battery == NULL) {
        goto EXIT;
    }

    /* get state */
    ret = 0;

    json_object_object_add(battery, CLOVA_PAYLOAD_NAME_VALUE, json_object_new_int(100));
    json_object_object_add(battery, CLOVA_PAYLOAD_VALUE_CHARGING, json_object_new_boolean(1));

EXIT:

    return ret;
}

static int get_battery_state(json_object *json_payload)
{
    json_object *battery = NULL;
    int ret = -1;

    if (json_payload == NULL) {
        goto EXIT;
    }

    battery = json_object_new_object();
    if (battery == NULL) {
        goto EXIT;
    }

    /* actions */
    ret = battery_get_actions(battery);
    if (ret) {
        goto EXIT;
    }

    /* others */
    ret = battery_get_state(battery);
    if (ret) {
        goto EXIT;
    }

    ret = 0;
    json_object_object_add(json_payload, CLOVA_PAYLOAD_NAME_BATTERY, battery);

EXIT:

    if (ret) {
        if (battery != NULL) {
            json_object_put(battery);
        }
    }

    return ret;
}

static int volume_get_actions(json_object *volume)
{
    json_object *actions = NULL;
    int ret = -1;

    if (volume == NULL) {
        goto EXIT;
    }

    actions = json_object_new_array();
    if (actions == NULL) {
        goto EXIT;
    }

    ret = json_object_array_add(actions, JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_NAME_DECREASE));
    if (ret) {
        goto EXIT;
    }

    ret = json_object_array_add(actions, JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_NAME_INCREASE));
    if (ret) {
        goto EXIT;
    }

    ret = json_object_array_add(actions, JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_NAME_SET_VALUE));
    if (ret) {
        goto EXIT;
    }

    json_object_object_add(volume, CLOVA_PAYLOAD_NAME_ACTIONS, actions);

EXIT:

    if (ret) {
        if (actions != NULL) {
            json_object_put(actions);
        }
    }

    return ret;
}

static int volume_get_others(json_object *volume)
{
    char *state = NULL;
    int ret = -1;
    int cur_volume = 0;

    if (volume == NULL) {
        goto EXIT;
    }

    /* get state */
    ret = 0;

    json_object_object_add(volume, CLOVA_PAYLOAD_NAME_MIN, json_object_new_int(0));
    json_object_object_add(volume, CLOVA_PAYLOAD_NAME_MAX, json_object_new_int(15));
    json_object_object_add(volume, CLOVA_PAYLOAD_NAME_WARNING, json_object_new_int(12));

    cur_volume = asr_player_get_volume(0);
    cur_volume = (cur_volume * 15) / 100 + (((cur_volume * 15) % 100 > 50) ? 1 : 0);
    json_object_object_add(volume, CLOVA_PAYLOAD_NAME_VALUE, json_object_new_int(cur_volume));

    int mute = asr_player_get_volume(1);
    json_object_object_add(volume, CLOVA_PAYLOAD_NAME_MUTE, json_object_new_boolean(mute));

EXIT:

    return ret;
}

static int get_volume_state(json_object *json_payload)
{
    json_object *volume = NULL;
    int ret = -1;

    if (json_payload == NULL) {
        goto EXIT;
    }

    volume = json_object_new_object();
    if (volume == NULL) {
        goto EXIT;
    }

    /* actions */
    ret = volume_get_actions(volume);
    if (ret) {
        goto EXIT;
    }

    /* others */
    ret = volume_get_others(volume);
    if (ret) {
        goto EXIT;
    }

    json_object_object_add(json_payload, CLOVA_PAYLOAD_NAME_VOLUME, volume);

EXIT:

    if (ret) {
        if (volume != NULL) {
            json_object_put(volume);
        }
    }

    return ret;
}

static int dnd_get_actions(json_object *dnd_state)
{
    json_object *actions = NULL;
    int ret = -1;

    if (dnd_state == NULL) {
        goto EXIT;
    }

    actions = json_object_new_array();
    if (actions == NULL) {
        goto EXIT;
    }

    ret = json_object_array_add(actions, JSON_OBJECT_NEW_STRING(CLOVA_HEADER_NAME_SETDND));
    if (ret) {
        goto EXIT;
    }

    ret = json_object_array_add(actions, JSON_OBJECT_NEW_STRING(CLOVA_HEADER_NAME_CANCELDND));
    if (ret) {
        goto EXIT;
    }

    ret = json_object_array_add(actions, JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_NAME_SET_VALUE));
    if (ret) {
        goto EXIT;
    }

    json_object_object_add(dnd_state, CLOVA_PAYLOAD_NAME_ACTIONS, actions);

EXIT:

    if (ret) {
        if (actions != NULL) {
            json_object_put(actions);
        }
    }

    return ret;
}

static int dnd_get_others(json_object *dnd_state)
{
    int ret = -1;

    if (dnd_state == NULL) {
        goto EXIT;
    }

    ret = clova_get_dnd_state(dnd_state);

EXIT:

    return ret;
}

static int get_dnd_state(json_object *json_payload)
{
    json_object *dnd_state = NULL;
    int ret = -1;

    if (json_payload == NULL) {
        goto EXIT;
    }

    dnd_state = json_object_new_object();
    if (dnd_state == NULL) {
        goto EXIT;
    }

    /* actions */
    ret = dnd_get_actions(dnd_state);
    if (ret) {
        goto EXIT;
    }

    /* others */
    ret = dnd_get_others(dnd_state);
    if (ret) {
        goto EXIT;
    }

    json_object_object_add(json_payload, CLOVA_PAYLOAD_NAME_DND, dnd_state);

EXIT:

    if (ret) {
        if (dnd_state != NULL) {
            json_object_put(dnd_state);
        }
    }

    return ret;
}

static int microphone_get_actions(json_object *microphone)
{
    json_object *actions = NULL;
    int ret = -1;

    if (microphone == NULL) {
        goto EXIT;
    }

    actions = json_object_new_array();
    if (actions == NULL) {
        goto EXIT;
    }

    ret = json_object_array_add(actions, JSON_OBJECT_NEW_STRING(CLOVA_HEADER_NAME_TURN_OFF));
    if (ret) {
        goto EXIT;
    }

    json_object_object_add(microphone, CLOVA_PAYLOAD_NAME_ACTIONS, actions);

EXIT:

    if (ret) {
        if (actions != NULL) {
            json_object_put(actions);
        }
    }

    return ret;
}

static int microphone_get_others(json_object *microphone)
{
    char *state = NULL;
    int ret = -1;

    if (microphone == NULL) {
        goto EXIT;
    }

    /* get state */
    ret = asr_get_privacy_mode();
    if (ret) {
        state = CLOVA_PAYLOAD_NAME_OFF;
    } else {
        state = CLOVA_PAYLOAD_NAME_ON;
    }

    ret = 0;

    json_object_object_add(microphone, CLOVA_PAYLOAD_NAME_STATE, JSON_OBJECT_NEW_STRING(state));

EXIT:

    return ret;
}

static int get_microphone_state(json_object *json_payload)
{
    json_object *microphone_state = NULL;
    int ret = -1;

    if (json_payload == NULL) {
        goto EXIT;
    }

    microphone_state = json_object_new_object();
    if (microphone_state == NULL) {
        goto EXIT;
    }

    /* actions */
    ret = microphone_get_actions(microphone_state);
    if (ret) {
        goto EXIT;
    }

    /* others */
    ret = microphone_get_others(microphone_state);
    if (ret) {
        goto EXIT;
    }

    json_object_object_add(json_payload, CLOVA_PAYLOAD_NAME_MICROPHONE, microphone_state);

EXIT:

    if (ret) {
        if (microphone_state != NULL) {
            json_object_put(microphone_state);
        }
    }

    return ret;
}

int clova_get_device_status(json_object *context_list)
{
    json_object *device_state_payload = NULL;
    int ret = -1;

    if (context_list == NULL) {
        goto EXIT;
    }

    device_state_payload = json_object_new_object();
    if (device_state_payload == NULL) {
        goto EXIT;
    }

    /* volume */
    ret = get_volume_state(device_state_payload);
    if (ret) {
        goto EXIT;
    }

    /* dnd */
    ret = get_dnd_state(device_state_payload);
    if (ret) {
        goto EXIT;
    }

    /* bluetooth */
    ret = asr_bluetooth_state(device_state_payload);
    if (ret) {
        goto EXIT;
    }

    /* power */
    ret = get_power_state(device_state_payload);
    if (ret) {
        goto EXIT;
    }

    /* wifi */
    ret = get_wifi_state(device_state_payload);
    if (ret) {
        goto EXIT;
    }

#if 0 // Donut doesn't have battery
    /* battery */
    ret = get_battery_state(device_state_payload);
    if(ret) {
        goto EXIT;
    }
#endif

    /* microphone */
    ret = get_microphone_state(device_state_payload);
    if (ret) {
        goto EXIT;
    }

    ret = asr_context_list_add(context_list, CLOVA_HEADER_NAMESPACE_DEVICE,
                               CLOVA_HEADER_NAME_DEVICE_STATE, device_state_payload);

EXIT:

    if (ret) {
        if (device_state_payload != NULL) {
            json_object_put(device_state_payload);
        }
    }

    return ret;
}
