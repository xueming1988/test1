
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
// #include <errno.h>
#include <sys/un.h>

#include <pthread.h>

#include "alexa_bluetooth.h"

#include "alexa_debug.h"
#include "alexa_json_common.h"
#include "alexa_request_json.h"
#include "asr_config.h"
#include "alexa_focus_mgmt.h"
#include "alexa_cfg.h"

#include "wm_util.h"
#include "lp_list.h"

#if !(defined(PLATFORM_AMLOGIC) || defined(PLATFORM_INGENIC) || defined(OPENWRT_AVS))
#include "nvram.h"
#endif

#include "alexa_bluetooth_Interface.h"

#define BT_BLE_SERVER (1 << 0)
#define BT_BLE_CLIENT (1 << 1)
#define BT_A2DP_SYNK (1 << 2)
#define BT_A2DP_SOURCE (1 << 3)

// The speeker act as server
#define PROFILE_SOURCE ("A2DP-SOURCE") // A2DP-SOURCE

// The speeker act as client
#define PROFILE_SINK ("A2DP-SINK") // A2DP-SINK

#define PROFILE_VER2_0 ("1.0")

#define PROFILE_AVRCP ("AVRCP")
#define PROFILE_VER1_0 ("1.0")

#define Key_ret ("ret")
#define Key_uuid ("uuid")
#define Key_name ("name")
#define Key_devices ("devices")
#define Key_hasmore ("hasmore")
#define Key_version ("version")
#define Key_duration ("duration")
#define Key_timeout_ms ("timeout_ms")
#define Key_profileName ("profileName")
#define Key_friendlyName ("friendlyName")
#define Key_uniqueDeviceId ("uniqueDeviceId")
#define Key_supportedProfiles ("supportedProfiles")
#define Key_truncatedMacAddress ("truncatedMacAddress")

#define Val_scan ("scan")
#define Val_pair ("pair")
#define Val_play ("play")
#define Val_stop ("stop")
#define Val_next ("next")
#define Val_status ("status")
#define Val_unpair ("unpair")
#define Val_previous ("previous")
#define Val_disconnect ("disconnect")
#define Val_connectbyid ("connectbyid")
#define Val_media_control ("media_control")
#define Val_getpaired_list ("getpaired_list")
#define Val_connectbyprofile ("connectbyprofile")
#define Val_enable_discoverable ("enable_discoverable")
#define Val_disable_discoverable ("disable_discoverable")

#define Val_role ("role")
#define Val_sink ("sink")
#define Val_source ("source")

#define Val_requester_device ("DEVICE")
#define Val_requester_cloud ("CLOUD")

#define Val_defdevice ("my phone")
#define Val_unknown_ver ("1.0")

#define Check_Version(version) (((version) && strlen((version)) > 0) ? (version) : Val_unknown_ver)

#define LAST_PAIED_BLE_MAC ("last_paired_ble_mac")
#define LAST_PAIED_BLE_DEVNAME ("last_paired_ble_devname")

#define BLE_CONNECTED ("common/bluetooth_connected.mp3")
#define BLE_DISCONNECTED ("common/bluetooth_disconnected.mp3")

#define update_key_val(json_data, key, set_val)                                                    \
    do {                                                                                           \
        json_object_object_del((json_data), (key));                                                \
        json_object_object_add((json_data), (key), (set_val));                                     \
    } while (0)

extern void block_voice_prompt(char *path, int block);

// ACTIVE, INACTIVE, PAUSED
enum {
    e_streaming_inactive,
    e_streaming_active,
    e_streaming_paused,
};

enum {
    e_add_scan_list,           // 0
    e_update_paired_list,      // 1
    e_delete_item_paired_list, // 2
    e_add_item_paired_list,    // 3
    e_update_active_device,    // 4
    e_free_active_device,      // 5
};

typedef struct {
    char *name;
    char *version;
    struct lp_list_head profile_item;
} ble_profile_t;

typedef struct {
    char *uuid;
    char *friendly_name;
    char *mac_address;
    char *self_role;
    struct lp_list_head profile_list;
    struct lp_list_head ble_item;
} ble_device_t;

typedef struct {
    int need_silence;
    int is_conn_id;
    int is_cloud;
    int is_enable_find;
    int stream_state;
    int discoverable_time;
    pthread_mutex_t state_lock;
    ble_device_t active_dev;
    struct lp_list_head paired_list;
    struct lp_list_head scan_list;
    char *cmd_cache;
} avs_ble_t;

static avs_ble_t g_avs_ble;
extern WIIMU_CONTEXT *g_wiimu_shm;

static ble_device_t *avs_ble_find_dev_by_uuid(struct lp_list_head *dev_list, char *uuid);

#ifndef DISABLE_BT_PROMPT_PLAY
static void avs_ble_prompt(char *path)
{
    // the prompt is played by mcu, so we needn't to play it
    if (path) {
        block_voice_prompt(path, 1);
    }
}
#endif

static void avs_switch_ble_mode(void) {}

void avs_bluetooth_do_cmd(int flag)
{
    pthread_mutex_lock(&g_avs_ble.state_lock);
    if (g_avs_ble.cmd_cache) {
        if (flag) {
            avs_switch_ble_mode();
        }
        BluetoothAPI_Directive_Send(g_avs_ble.cmd_cache, strlen(g_avs_ble.cmd_cache), 0);
        free(g_avs_ble.cmd_cache);
        g_avs_ble.cmd_cache = NULL;
    }
    pthread_mutex_unlock(&g_avs_ble.state_lock);
}

static int avs_ble_cfg_save(char *uuid, char *friendly_name, int want_clear)
{
    int ret = -1;

    if (want_clear == 0) {
        if (uuid && strlen(uuid) > 0) {
            asr_config_set(LAST_PAIED_BLE_MAC, uuid);
            ret = 0;
        }
        if (friendly_name && strlen(friendly_name) > 0) {
            asr_config_set(LAST_PAIED_BLE_DEVNAME, friendly_name);
            ret = 0;
        }
    } else if (want_clear == 1) {
        asr_config_set(LAST_PAIED_BLE_MAC, "");
        asr_config_set(LAST_PAIED_BLE_DEVNAME, "");
        ret = 0;
    }
    if (ret == 0) {
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "uuid=%s friendly_name=%s\n", uuid, friendly_name);
        asr_config_sync();
    }

    return ret;
}

/*
{
  "directive": {
    "header": {
      "namespace": "Bluetooth",
      "name": "ScanDevices",
      "messageId": "{{STRING}}"
    },
    "payload": {
    }
  }
}
*/
static int avs_ble_scan(json_object *js_data)
{
    if (js_data) {
        json_object *js_header = json_object_object_get(js_data, KEY_HEADER);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
        }

        json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
        if (js_payload) {
            // JinDongjie Begin
            wiimu_log(1, 0, 0, 0, "Bluetooth Process(directive scan) recv. +++++++");

            char *cmd = NULL;
            char *params = NULL;
            long timeout_ms = 0;
            Create_Internal_BluetoothAPI_Directive_Scan_Params(params, timeout_ms);
            if (params) {
                Create_Internal_BluetoothAPI_Directive_Cmd(cmd, Cmd_Scan, params);
                free(params);
            }
            if (cmd) {
                BluetoothAPI_Directive_Send(cmd, strlen(cmd), 0);
                free(cmd);
            }
            // JinDongjie End
        }

        return 0;
    }

    return -1;
}

/*
{
  "directive": {
    "header": {
      "namespace": "Bluetooth",
      "name": "EnterDiscoverableMode",
      "messageId": "{{STRING}}"
    },
    "payload": {
      "durationInSeconds": {{LONG}}
    }
  }
}
*/
static int avs_ble_startdiscovery(json_object *js_data)
{
    if (js_data) {
        json_object *js_header = json_object_object_get(js_data, KEY_HEADER);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
        }

        json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
        if (js_payload) {
            long durationInSeconds =
                JSON_GET_LONG_FROM_OBJECT(js_payload, PAYLOAD_durationInSeconds);

            // JinDongjie Begin
            wiimu_log(1, 0, 0, 0, "Bluetooth Process(directive EnterDiscoverableMode) recv "
                                  "durationInSeconds=%ld +++++++",
                      durationInSeconds);

            char *cmd = NULL;
            char *params = NULL;
            Create_Internal_BluetoothAPI_Directive_EnterDiscoverableMode_Params(params,
                                                                                durationInSeconds);
            if (params) {
                Create_Internal_BluetoothAPI_Directive_Cmd(cmd, Cmd_EnterDiscoverableMode, params);
                free(params);
            }
            if (cmd) {
                BluetoothAPI_Directive_Send(cmd, strlen(cmd), 0);
                free(cmd);
                pthread_mutex_lock(&g_avs_ble.state_lock);
                g_avs_ble.is_enable_find = 1;
                pthread_mutex_unlock(&g_avs_ble.state_lock);
            }
            // JinDongjie End
        }

        return 0;
    }

    return -1;
}

/*
{
  "directive": {
    "header": {
      "namespace": "Bluetooth",
      "name": "ExitDiscoverableMode",
      "messageId": "{{STRING}}"
    },
    "payload": {
    }
  }
}
*/
static int avs_ble_enddiscovery(json_object *js_data)
{
    if (js_data) {
        json_object *js_header = json_object_object_get(js_data, KEY_HEADER);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
        }

        json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
        if (js_payload) {
            // JinDongjie Begin
            wiimu_log(1, 0, 0, 0,
                      "Bluetooth Process(directive ExitDiscoverableMode) recv. +++++++");

            char *cmd = NULL;
            Create_Internal_BluetoothAPI_Directive_Cmd(cmd, Cmd_ExitDiscoverableMode, Val_Obj(0));
            if (cmd) {
                BluetoothAPI_Directive_Send(cmd, strlen(cmd), 0);
                free(cmd);
                pthread_mutex_lock(&g_avs_ble.state_lock);
                g_avs_ble.is_enable_find = 1;
                pthread_mutex_unlock(&g_avs_ble.state_lock);
            }
            // JinDongjie End
        }

        return 0;
    }

    return -1;
}

/*
{
  "directive": {
    "header": {
      "namespace": "Bluetooth",
      "name": "PairDevice",
      "messageId": "{{STRING}}"
    },
    "payload": {
      "device": {
        "uniqueDeviceId": "{{STRING}}"
      }
    }
  }
}
*/
static int avs_ble_pair(json_object *js_data)
{
    if (js_data) {
        json_object *js_header = json_object_object_get(js_data, KEY_HEADER);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
        }

        json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
        if (js_payload) {
            json_object *js_device = json_object_object_get(js_payload, PAYLOAD_device);
            if (js_device) {
                char *uuid = JSON_GET_STRING_FROM_OBJECT(js_device, PAYLOAD_uniqueDeviceId);
                if (uuid) {
                    char *role = NULL;
                    char *cmd = NULL;
                    char *params = NULL;
                    pthread_mutex_lock(&g_avs_ble.state_lock);
                    g_avs_ble.is_cloud = 1;
                    ble_device_t *ble_dev = avs_ble_find_dev_by_uuid(&g_avs_ble.scan_list, uuid);
                    // JinDongjie Begin
                    if (ble_dev && ble_dev->self_role) {
                        role = ble_dev->self_role;
                    } else {
                        role = "";
                    }
                    wiimu_log(1, 0, 0, 0,
                              "Bluetooth Process(directive pair) recv uuid=%s:%s +++++++", uuid,
                              role);
                    Create_Internal_BluetoothAPI_Directive_ConnectById_Params(params, uuid, role);
                    pthread_mutex_unlock(&g_avs_ble.state_lock);
                    if (params) {
                        Create_Internal_BluetoothAPI_Directive_Cmd(cmd, Cmd_Pair, params);
                        free(params);
                    }
                    if (cmd) {
                        BluetoothAPI_Directive_Send(cmd, strlen(cmd), 0);
                        free(cmd);
                    }
                    // JinDongjie End
                }
            }
        }

        return 0;
    }

    return -1;
}

/*
{
  "directive": {
    "header": {
      "namespace": "Bluetooth",
      "name": "UnpairDevice",
      "messageId": "{{STRING}}"
    },
    "payload": {
      "device": {
        "uniqueDeviceId": "{{STRING}}"
      }
    }
  }
}
*/
static int avs_ble_unpair(json_object *js_data)
{
    if (js_data) {
        json_object *js_header = json_object_object_get(js_data, KEY_HEADER);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
        }

        json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
        if (js_payload) {
            json_object *js_device = json_object_object_get(js_payload, PAYLOAD_device);
            if (js_device) {
                char *uuid = JSON_GET_STRING_FROM_OBJECT(js_device, PAYLOAD_uniqueDeviceId);
                if (uuid) {
                    // JinDongjie Begin
                    wiimu_log(1, 0, 0, 0,
                              "Bluetooth Process(directive unpair) recv uuid=%s +++++++", uuid);

                    char *cmd = NULL;
                    char *params = NULL;
                    Create_Internal_BluetoothAPI_Directive_UNPAIR_Params(params, uuid);
                    if (params) {
                        Create_Internal_BluetoothAPI_Directive_Cmd(cmd, Cmd_UnPair, params);
                        free(params);
                    }
                    if (cmd) {
                        BluetoothAPI_Directive_Send(cmd, strlen(cmd), 0);
                        free(cmd);
                    }
                    // JinDongjie End
                }
            }
        }

        return 0;
    }

    return -1;
}

/*
{
  "directive": {
    "header": {
      "namespace": "Bluetooth",
      "name": "ConnectByDeviceId",
      "messageId": "{{STRING}}"
    },
    "payload": {
      "device": {
        "uniqueDeviceId": "{{STRING}}"
      }
    }
  }
}
*/
static int avs_ble_connect_id(json_object *js_data)
{
    if (js_data) {
        json_object *js_header = json_object_object_get(js_data, KEY_HEADER);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
        }

        json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
        if (js_payload) {
            json_object *js_device = json_object_object_get(js_payload, PAYLOAD_device);
            if (js_device) {
                char *role = NULL;
                char *uuid = JSON_GET_STRING_FROM_OBJECT(js_device, PAYLOAD_uniqueDeviceId);
                if (uuid) {
                    // JinDongjie Begin
                    char *cmd = NULL;
                    char *params = NULL;

                    pthread_mutex_lock(&g_avs_ble.state_lock);
                    ble_device_t *ble_dev = avs_ble_find_dev_by_uuid(&g_avs_ble.scan_list, uuid);
                    // JinDongjie Begin
                    if (ble_dev && ble_dev->self_role) {
                        role = ble_dev->self_role;
                    } else {
                        role = "";
                    }
                    wiimu_log(1, 0, 0, 0,
                              "Bluetooth Process(directive ConnectById) recv uuid=%s:%s +++++++",
                              uuid, role);
                    Create_Internal_BluetoothAPI_Directive_ConnectById_Params(params, uuid, role);
                    pthread_mutex_unlock(&g_avs_ble.state_lock);
                    if (params) {
                        Create_Internal_BluetoothAPI_Directive_Cmd(cmd, Cmd_ConnectById, params);
                        free(params);
                    }
                    if (cmd) {
#if 0
                        BluetoothAPI_Directive_Send(cmd, strlen(cmd), 0);
                        free(cmd);
                        pthread_mutex_lock(&g_avs_ble.state_lock);
                        g_avs_ble.is_cloud = 1;
                        g_avs_ble.is_conn_id = 1;
                        pthread_mutex_unlock(&g_avs_ble.state_lock);
#else
                        pthread_mutex_lock(&g_avs_ble.state_lock);
                        if (g_avs_ble.cmd_cache) {
                            free(g_avs_ble.cmd_cache);
                        }
                        g_avs_ble.cmd_cache = cmd;
                        g_avs_ble.is_cloud = 1;
                        g_avs_ble.is_conn_id = 1;
                        pthread_mutex_unlock(&g_avs_ble.state_lock);
#endif
                    }
                    // JinDongjie End
                }
            }
        }

        return 0;
    }

    return -1;
}

/*
{
  "directive": {
    "header": {
      "namespace": "Bluetooth",
      "name": "ConnectByProfile",
      "messageId": "{{STRING}}"
    },
    "payload": {
      "profile": {
        "name": "{{STRING}}",
        "version": "{{STRING}}"
      }
    }
  }
}
*/
static int avs_ble_connect_profile(json_object *js_data)
{
    if (js_data) {
        json_object *js_header = json_object_object_get(js_data, KEY_HEADER);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
        }

        json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
        if (js_payload) {
            json_object *js_profile = json_object_object_get(js_payload, PAYLOAD_PROFILE);
            if (js_profile) {
                char *name = JSON_GET_STRING_FROM_OBJECT(js_profile, KEY_NAME);
                char *version = JSON_GET_STRING_FROM_OBJECT(js_profile, PAYLOAD_version);
                if (name) {
                    ALEXA_PRINT(ALEXA_DEBUG_ERR, "name is %s version is %s\n", name, version);
                    char *cmd = NULL;
                    char *params = NULL;
#ifdef AMLOGIC_A113
                    char *role = NULL;
                    if (StrEq(name, PROFILE_SOURCE)) {
                        role = Val_sink;
                    } else if (StrEq(name, PROFILE_SINK)) {
                        role = Val_source;
                    } else if (StrEq(name, "A2DP")) {
                        role = "";
                    } else {
                        role = Val_sink;
                    }
                    if (role) {
                        Create_Internal_BluetoothAPI_Directive_ConnectById_Params(params, "", role);
                        if (params) {
                            Create_Internal_BluetoothAPI_Directive_Cmd(cmd, Cmd_ConnectById,
                                                                       params);
                            free(params);
                        }
                    }
#else
                    char last_connected[128] = {0};
                    asr_config_get(LAST_PAIED_BLE_MAC, last_connected, sizeof(last_connected));
                    if (last_connected || (last_connected && strlen(last_connected) > 0) ||
                        (version && strlen(version) == 0)) {
                        version = last_connected;
                        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "MAC=%s\n", version);
                    }
                    Create_Internal_BluetoothAPI_Directive_ConnectByProfile_Params(
                        params, Val_Str(name), Val_Str(version));
                    if (params) {
                        Create_Internal_BluetoothAPI_Directive_Cmd(cmd, Cmd_ConnectByProfile,
                                                                   params);
                        free(params);
                    }
#endif
                    if (cmd) {
#if 0
                        BluetoothAPI_Directive_Send(cmd, strlen(cmd), 0);
                        free(cmd);
                        pthread_mutex_lock(&g_avs_ble.state_lock);
                        g_avs_ble.is_cloud = 1;
                        g_avs_ble.is_conn_id = 0;
                        pthread_mutex_unlock(&g_avs_ble.state_lock);
#else
                        pthread_mutex_lock(&g_avs_ble.state_lock);
                        if (g_avs_ble.cmd_cache) {
                            free(g_avs_ble.cmd_cache);
                        }
                        g_avs_ble.cmd_cache = cmd;
                        g_avs_ble.is_cloud = 1;
                        g_avs_ble.is_conn_id = 0;
                        pthread_mutex_unlock(&g_avs_ble.state_lock);
#endif
                    }
                }
            }
        }

        return 0;
    }

    return -1;
}

/*
{
  "directive": {
    "header": {
      "namespace": "Bluetooth",
      "name": "ConnectByDeviceId",
      "messageId": "{{STRING}}"
    },
    "payload": {
      "device": {
        "uniqueDeviceId": "{{STRING}}"
      }
    }
  }
}
*/
static int avs_ble_disconnect(json_object *js_data)
{
    if (js_data) {
        json_object *js_header = json_object_object_get(js_data, KEY_HEADER);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
        }

        json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
        if (js_payload) {
            json_object *js_device = json_object_object_get(js_payload, PAYLOAD_device);
            if (js_device) {
                char *uuid = JSON_GET_STRING_FROM_OBJECT(js_device, PAYLOAD_uniqueDeviceId);
                if (uuid) {
                    // JinDongjie Begin
                    wiimu_log(1, 0, 0, 0,
                              "Bluetooth Process(directive ConnectById) recv uuid=%s +++++++",
                              uuid);

                    char *cmd = NULL;
                    char *params = NULL;
                    Create_Internal_BluetoothAPI_Directive_Disconnect_Params(params, uuid);
                    if (params) {
                        Create_Internal_BluetoothAPI_Directive_Cmd(cmd, Cmd_Disconnect, params);
                        free(params);
                    }
                    if (cmd) {
                        BluetoothAPI_Directive_Send(cmd, strlen(cmd), 0);
                        free(cmd);
                        pthread_mutex_lock(&g_avs_ble.state_lock);
                        g_avs_ble.is_cloud = 1;
                        pthread_mutex_unlock(&g_avs_ble.state_lock);
                    }
                    // JinDongjie End
                }
            }
        }

        return 0;
    }

    return -1;
}

/*
{
  "directive": {
    "header": {
      "namespace": "Bluetooth",
      "name": "Play",
      "messageId": "{{STRING}}"
    },
    "payload": {
    }
  }
}
*/
static int avs_ble_play(json_object *js_data)
{
    if (js_data) {
        json_object *js_header = json_object_object_get(js_data, KEY_HEADER);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
        }

        json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
        if (js_payload) {
            // { JinDongjie Begin
            wiimu_log(1, 0, 0, 0, "Bluetooth Process(directive Play) recv. +++++++");
            // avs_switch_ble_mode();
            char *cmd = NULL;
            char *params = NULL;
            Create_Internal_BluetoothAPI_Directive_MediaControl_Params(params, Val_play);
            if (params) {
                Create_Internal_BluetoothAPI_Directive_Cmd(cmd, Cmd_MediaControl, params);
                free(params);
            }
#if 0
            if(cmd) {
                BluetoothAPI_Directive_Send(cmd, strlen(cmd), 0);
                free(cmd);
            }
#else
            pthread_mutex_lock(&g_avs_ble.state_lock);
            if (g_avs_ble.cmd_cache) {
                free(g_avs_ble.cmd_cache);
            }
            g_avs_ble.cmd_cache = cmd;
            pthread_mutex_unlock(&g_avs_ble.state_lock);
#endif
            // } JinDongjie End
        }

        return 0;
    }

    return -1;
}

/*
{
  "directive": {
    "header": {
      "namespace": "Bluetooth",
      "name": "Stop",
      "messageId": "{{STRING}}"
    },
    "payload": {
    }
  }
}
*/
static int avs_ble_stop(json_object *js_data)
{
    if (js_data) {
        json_object *js_header = json_object_object_get(js_data, KEY_HEADER);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
        }

        json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
        if (js_payload) {
            // JinDongjie Begin
            wiimu_log(1, 0, 0, 0, "Bluetooth Process(directive Stop) recv. +++++++");
            avs_switch_ble_mode();

            char *cmd = NULL;
            char *params = NULL;
            Create_Internal_BluetoothAPI_Directive_MediaControl_Params(params, Val_stop);
            if (params) {
                Create_Internal_BluetoothAPI_Directive_Cmd(cmd, Cmd_MediaControl, params);
                free(params);
            }
            if (cmd) {
                BluetoothAPI_Directive_Send(cmd, strlen(cmd), 0);
                free(cmd);
            }
            // JinDongjie End
            pthread_mutex_lock(&g_avs_ble.state_lock);
            if (g_avs_ble.stream_state != e_streaming_inactive)
                g_avs_ble.stream_state = e_streaming_paused;
            pthread_mutex_unlock(&g_avs_ble.state_lock);
        }

        return 0;
    }

    return -1;
}

/*
{
  "directive": {
    "header": {
      "namespace": "Bluetooth",
      "name": "Next",
      "messageId": "{{STRING}}"
    },
    "payload": {
    }
  }
}
*/
static int avs_ble_next(json_object *js_data)
{
    if (js_data) {
        json_object *js_header = json_object_object_get(js_data, KEY_HEADER);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
        }

        json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
        if (js_payload) {
            // JinDongjie Begin
            wiimu_log(1, 0, 0, 0, "Bluetooth Process(directive Next) recv. +++++++");
            avs_switch_ble_mode();

            char *cmd = NULL;
            char *params = NULL;
            Create_Internal_BluetoothAPI_Directive_MediaControl_Params(params, Val_next);
            if (params) {
                Create_Internal_BluetoothAPI_Directive_Cmd(cmd, Cmd_MediaControl, params);
                free(params);
            }
            if (cmd) {
                BluetoothAPI_Directive_Send(cmd, strlen(cmd), 0);
                free(cmd);
            }
            // JinDongjie End
        }

        return 0;
    }

    return -1;
}

/*
{
  "directive": {
    "header": {
      "namespace": "Bluetooth",
      "name": "Previous",
      "messageId": "{{STRING}}"
    },
    "payload": {
    }
  }
}
*/
static int avs_ble_prevoius(json_object *js_data)
{
    if (js_data) {
        json_object *js_header = json_object_object_get(js_data, KEY_HEADER);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
        }

        json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
        if (js_payload) {
            // JinDongjie Begin
            wiimu_log(1, 0, 0, 0, "Bluetooth Process(directive Previous) recv. +++++++");
            avs_switch_ble_mode();

            char *cmd = NULL;
            char *params = NULL;
            Create_Internal_BluetoothAPI_Directive_MediaControl_Params(params, Val_previous);
            if (params) {
                Create_Internal_BluetoothAPI_Directive_Cmd(cmd, Cmd_MediaControl, params);
                free(params);
            }
            if (cmd) {
                BluetoothAPI_Directive_Send(cmd, strlen(cmd), 0);
                free(cmd);
            }
            // JinDongjie End
        }

        return 0;
    }

    return -1;
}

// Jin Begin
void avs_bluetooth_getpaired_list(void)
{
    wiimu_log(1, 0, 0, 0, "Bluetooth Process(getpaired_list). +++++++");

    char *cmd = NULL;
    Create_Internal_BluetoothAPI_Directive_Cmd(cmd, Cmd_getpaired_list, Val_Obj(0));
    if (cmd) {
        BluetoothAPI_Directive_Send(cmd, strlen(cmd), 0);
        free(cmd);
    }
}
// Jin End

static json_object *avs_ble_get_a_profile(ble_profile_t *ble_profile)
{
    json_object *profile_json = NULL;

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_ble_get_a_profile start\n");

    if (ble_profile) {
        profile_json = json_object_new_object();
        if (profile_json) {
            json_object_object_add(profile_json, PAYLOAD_name,
                                   JSON_OBJECT_NEW_STRING(ble_profile->name));
            json_object_object_add(profile_json, PAYLOAD_version,
                                   JSON_OBJECT_NEW_STRING(ble_profile->version));
            return profile_json;
        }
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_ble_get_a_profile end\n");

    return NULL;
}

static json_object *avs_ble_get_profile_list(ble_device_t *ble_dev)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    json_object *ble_dev_profile_list = NULL;
    json_object *ble_dev_profile_json = NULL;
    ble_profile_t *ble_profile = NULL;

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_ble_get_profile_list start\n");
    ble_dev_profile_list = json_object_new_array();
    if (!lp_list_empty(&ble_dev->profile_list) && ble_dev_profile_list) {
        lp_list_for_each_safe(pos, npos, &ble_dev->profile_list)
        {
            ble_profile = lp_list_entry(pos, ble_profile_t, profile_item);
            if (ble_profile) {
                ble_dev_profile_json = avs_ble_get_a_profile(ble_profile);
                if (ble_dev_profile_json) {
                    json_object_array_add(ble_dev_profile_list, ble_dev_profile_json);
                }
            }
        }
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_ble_get_profile_list end\n");
    return ble_dev_profile_list;
}

static json_object *avs_ble_get_a_device(int need_profile, ble_device_t *ble_dev)
{
    json_object *ble_dev_json = NULL;
    json_object *ble_dev_profile_list = NULL;

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_ble_get_a_device start\n");

    if (ble_dev && ble_dev->uuid) {
        ble_dev_json = json_object_new_object();
        if (ble_dev_json) {
            json_object_object_add(ble_dev_json, PAYLOAD_uniqueDeviceId,
                                   JSON_OBJECT_NEW_STRING(ble_dev->uuid));
            json_object_object_add(ble_dev_json, PAYLOAD_friendlyName,
                                   JSON_OBJECT_NEW_STRING(ble_dev->friendly_name));
            if (need_profile == 0) {
                json_object_object_add(ble_dev_json, PAYLOAD_truncatedMacAddress,
                                       JSON_OBJECT_NEW_STRING(ble_dev->mac_address));
            } else if (need_profile == 1) {
                ble_dev_profile_list = avs_ble_get_profile_list(ble_dev);
                if (ble_dev_profile_list) {
                    json_object_object_add(ble_dev_json, PAYLOAD_supportedProfiles,
                                           ble_dev_profile_list);
                }
            }
            return ble_dev_json;
        }
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_ble_get_a_device end\n");

    return NULL;
}

static int avs_ble_get_dev_list(int need_profile, struct lp_list_head *dev_list,
                                json_object *js_dev_list)
{
    int ret = -1;

    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    ble_device_t *ble_dev = NULL;
    json_object *ble_dev_json = NULL;

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_ble_get_dev_list start\n");

    if (!lp_list_empty(dev_list) && js_dev_list) {
        lp_list_for_each_safe(pos, npos, dev_list)
        {
            ble_dev = lp_list_entry(pos, ble_device_t, ble_item);
            if (ble_dev) {
                ble_dev_json = avs_ble_get_a_device(need_profile, ble_dev);
                if (ble_dev_json) {
                    json_object_array_add(js_dev_list, ble_dev_json);
                    ret = 0;
                }
            }
        }
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_ble_get_dev_list end\n");

    return ret;
}

static int avs_ble_free_a_profile(ble_profile_t **profile_ptr)
{
    ble_profile_t *profile = *profile_ptr;

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_ble_free_a_profile start\n");
    if (profile) {
        if (profile->name) {
            free(profile->name);
            profile->name = NULL;
        }
        if (profile->version) {
            free(profile->version);
            profile->version = NULL;
        }
        free(profile);

        ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_ble_free_a_profile end\n");

        return 0;
    }
    ALEXA_PRINT(ALEXA_DEBUG_ERR, "avs_ble_free_a_profile end\n");

    return -1;
}

static int avs_ble_add_a_profile(struct lp_list_head *profile_list, char *name, char *version)
{
    int ret = -1;
    int is_same = 0;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    ble_profile_t *profile = NULL;

    if (!profile_list || !name) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "profile_list is NULL\n");
        return -1;
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_ble_add_a_profile start\n");

    if (!lp_list_empty(profile_list)) {
        lp_list_for_each_safe(pos, npos, profile_list)
        {
            profile = lp_list_entry(pos, ble_profile_t, profile_item);
            if (profile) {
                if (StrCaseEq(profile->name, name)) {
                    is_same = 1;
                    break;
                }
            }
        }
    }
    if (is_same == 0) {
        profile = (ble_profile_t *)calloc(1, sizeof(ble_profile_t));
        if (profile) {
            profile->name = strdup(name);
            profile->version = strdup(Check_Version(version));
            lp_list_add_tail(&profile->profile_item, profile_list);
            ret = 0;
        } else {
            ALEXA_PRINT(ALEXA_DEBUG_ERR, "out of mem\n");
        }
    } else if (profile) {
        if (profile->version) {
            free(profile->version);
        }
        profile->version = strdup(Check_Version(version));
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_ble_add_a_profile end\n");

    return ret;
}

static int avs_ble_free_a_dev(ble_device_t *device, int flags)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    ble_profile_t *profile = NULL;

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_ble_free_a_dev start\n");
    if (device) {
        if (!lp_list_empty(&device->profile_list)) {
            lp_list_for_each_safe(pos, npos, &device->profile_list)
            {
                profile = lp_list_entry(pos, ble_profile_t, profile_item);
                if (profile) {
                    lp_list_del(&profile->profile_item);
                    avs_ble_free_a_profile(&profile);
                }
            }
        }
        if (device->uuid) {
            free(device->uuid);
            device->uuid = NULL;
        }
        if (device->friendly_name) {
            free(device->friendly_name);
            device->friendly_name = NULL;
        }
        if (device->self_role) {
            free(device->self_role);
            device->self_role = NULL;
        }
        if (device->mac_address) {
            free(device->mac_address);
            device->mac_address = NULL;
        }
        if (flags) {
            free(device);
        }
        ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_ble_free_a_dev end\n");
        return 0;
    }
    ALEXA_PRINT(ALEXA_DEBUG_ERR, "avs_ble_free_a_dev end\n");

    return -1;
}

static int avs_ble_del_a_dev(struct lp_list_head *dev_list, json_object *json_cmd_parmas)
{
    int ret = -1;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    char *uuid = NULL;
    char *friendly_name = NULL;
    ble_device_t *device_item = NULL;

    if (!json_cmd_parmas || !dev_list) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "json_cmd_parmas is NULL\n");
        return -1;
    }

    ALEXA_PRINT(ALEXA_DEBUG_ERR, "avs_ble_del_a_dev start\n");
    uuid = JSON_GET_STRING_FROM_OBJECT(json_cmd_parmas, Key_uniqueDeviceId);
    friendly_name = JSON_GET_STRING_FROM_OBJECT(json_cmd_parmas, Key_friendlyName);
    if (!lp_list_empty(dev_list) && uuid) {
        lp_list_for_each_safe(pos, npos, dev_list)
        {
            device_item = lp_list_entry(pos, ble_device_t, ble_item);
            if (device_item) {
                if (StrCaseEq(device_item->uuid, uuid)) {
                    if ((friendly_name == NULL || (friendly_name && strlen(friendly_name) == 0)) &&
                        (device_item->friendly_name && strlen(device_item->friendly_name) > 0)) {
                        json_object_object_add(json_cmd_parmas, Key_friendlyName,
                                               JSON_OBJECT_NEW_STRING(device_item->friendly_name));
                    }
                    lp_list_del(&device_item->ble_item);
                    avs_ble_free_a_dev(device_item, 1);
                    ret = 0;
                    break;
                }
            }
        }
    }

    ALEXA_PRINT(ALEXA_DEBUG_ERR, "avs_ble_del_a_dev end\n");

    return ret;
}

static int avs_ble_add_a_dev(int need_profile, struct lp_list_head *dev_list,
                             json_object *js_device)
{
    int ret = -1;
    int i = 0;
    int item_num = 0;
    int is_same = -1;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    char *uuid = NULL;
    char *self_role = NULL;
    char *friendly_name = NULL;
    char *mac_address = NULL;
    char *profile_name = NULL;
    ble_device_t *device_item = NULL;

    if (!js_device || !dev_list) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "json_cmd_parmas is NULL\n");
        return -1;
    }

    ALEXA_PRINT(ALEXA_DEBUG_ERR, "avs_ble_add_a_dev start\n");
    uuid = JSON_GET_STRING_FROM_OBJECT(js_device, Key_uniqueDeviceId);
    friendly_name = JSON_GET_STRING_FROM_OBJECT(js_device, Key_friendlyName);
    mac_address = JSON_GET_STRING_FROM_OBJECT(js_device, Key_truncatedMacAddress);
    self_role = JSON_GET_STRING_FROM_OBJECT(js_device, Val_role);
    profile_name = JSON_GET_STRING_FROM_OBJECT(js_device, Key_profileName);

    if (NULL == uuid || strlen(uuid) <= 0) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "uuid is invalid\n");
        return -1;
    }

    // if exist, update value
    if (!lp_list_empty(dev_list) && uuid) {
        lp_list_for_each_safe(pos, npos, dev_list)
        {
            device_item = lp_list_entry(pos, ble_device_t, ble_item);
            if (device_item) {
                if (StrCaseEq(device_item->uuid, uuid)) {
                    is_same = 0;
                    break;
                }
            }
        }
    }

    if (is_same != 0) {
        // add new information
        device_item = (ble_device_t *)calloc(1, sizeof(ble_device_t));
        if (device_item) {
            LP_INIT_LIST_HEAD(&device_item->profile_list);
            if (uuid) {
                device_item->uuid = strdup(uuid);
            }
            if (mac_address) {
                device_item->mac_address = strdup(mac_address);
            }
            if (self_role) {
                device_item->self_role = strdup(self_role);
            }
        }
    }
    if (device_item) {
        if (friendly_name) {
            if (device_item->friendly_name == NULL) {
                device_item->friendly_name = strdup(friendly_name);
            } else {
                free(device_item->friendly_name);
                device_item->friendly_name = strdup(friendly_name);
            }
        }
        if (need_profile == 1) {
            json_object *supportedProfiles =
                json_object_object_get(js_device, Key_supportedProfiles);
            if (supportedProfiles) {
                item_num = json_object_array_length(supportedProfiles);
                for (i = 0; i < item_num; i++) {
                    json_object *js_profile = json_object_array_get_idx(supportedProfiles, i);
                    if (js_profile) {
                        avs_ble_add_a_profile(&device_item->profile_list,
                                              JSON_GET_STRING_FROM_OBJECT(js_profile, Key_name),
                                              JSON_GET_STRING_FROM_OBJECT(js_profile, Key_version));
                    }
                }
                avs_ble_add_a_profile(&device_item->profile_list, PROFILE_AVRCP, PROFILE_VER1_0);
            } else if (profile_name) {
                if (StrEq(profile_name, Val_sink)) {
                    avs_ble_add_a_profile(&device_item->profile_list, PROFILE_SOURCE,
                                          PROFILE_VER2_0);
                } else if (StrEq(profile_name, Val_source)) {
                    avs_ble_add_a_profile(&device_item->profile_list, PROFILE_SINK, PROFILE_VER2_0);
                }
            }
#if !defined(ENABLE_BT_SOURCE)
            else {
                avs_ble_add_a_profile(&device_item->profile_list, PROFILE_SOURCE, PROFILE_VER2_0);
                avs_ble_add_a_profile(&device_item->profile_list, PROFILE_AVRCP, PROFILE_VER1_0);
            }
#endif
        }
        if (is_same != 0) {
            lp_list_add_tail(&device_item->ble_item, dev_list);
        }
        ret = 0;
    } else {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "device_item is NULL\n");
    }

    ALEXA_PRINT(ALEXA_DEBUG_ERR, "avs_ble_add_a_dev end\n");

    return ret;
}

static int avs_ble_update_active_profile(struct lp_list_head *profile_list1,
                                         struct lp_list_head *profile_list2)
{
    int ret = -1;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;

    if (!profile_list1 || !profile_list2) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "input is error\n");
        return -1;
    }

    if (!lp_list_empty(profile_list1)) {
        lp_list_for_each_safe(pos, npos, profile_list1)
        {
            ble_profile_t *ble_profile = lp_list_entry(pos, ble_profile_t, profile_item);
            if (ble_profile) {
                ret = avs_ble_add_a_profile(profile_list2, ble_profile->name, ble_profile->version);
            }
        }
    }

    avs_ble_add_a_profile(profile_list2, PROFILE_AVRCP, PROFILE_VER1_0);

    return ret;
}

static int avs_ble_update_active(json_object *js_device)
{
    int ret = -1;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    char *uuid = NULL;
    char *friendly_name = NULL;
    ble_device_t *device_item = NULL;

    if (!js_device) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "json_cmd_parmas is NULL\n");
        return -1;
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_ble_update_active start\n");
    uuid = JSON_GET_STRING_FROM_OBJECT(js_device, Key_uniqueDeviceId);
    friendly_name = JSON_GET_STRING_FROM_OBJECT(js_device, Key_friendlyName);

    if (!lp_list_empty(&g_avs_ble.paired_list) && uuid) {
        lp_list_for_each_safe(pos, npos, &g_avs_ble.paired_list)
        {
            device_item = lp_list_entry(pos, ble_device_t, ble_item);
            if (device_item) {
                if (StrEq(device_item->friendly_name, friendly_name) ||
                    StrEq(device_item->uuid, uuid)) {
                    avs_ble_free_a_dev(&g_avs_ble.active_dev, 0);
                    if (device_item->uuid) {
                        g_avs_ble.active_dev.uuid = strdup(device_item->uuid);
                    }
                    if (device_item->friendly_name) {
                        g_avs_ble.active_dev.friendly_name = strdup(device_item->friendly_name);
                    }
                    if (device_item->mac_address) {
                        g_avs_ble.active_dev.mac_address = strdup(device_item->mac_address);
                    }
                    avs_ble_update_active_profile(&device_item->profile_list,
                                                  &g_avs_ble.active_dev.profile_list);
                    ret = 0;
                    break;
                }
            }
        }
    }
    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_ble_update_active end\n");

    return ret;
}

static ble_device_t *avs_ble_find_dev_by_uuid(struct lp_list_head *dev_list, char *uuid)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    ble_device_t *device_item = NULL;

    ALEXA_PRINT(ALEXA_DEBUG_ERR, "avs_ble_find_dev_by_uuid start\n");
    if (uuid && (dev_list && !lp_list_empty(dev_list))) {
        // remove the latest one
        lp_list_for_each_safe(pos, npos, dev_list)
        {
            device_item = lp_list_entry(pos, ble_device_t, ble_item);
            if (device_item) {
                if (StrCaseEq(device_item->uuid, uuid)) {
                    ALEXA_PRINT(ALEXA_DEBUG_ERR, "avs_ble_find_dev_by_uuid friendly_name(%s)\n",
                                device_item->friendly_name);
                    break;
                } else {
                    device_item = NULL;
                }
            }
        }
    }
    ALEXA_PRINT(ALEXA_DEBUG_ERR, "avs_ble_find_dev_by_uuid end\n");

    return device_item;
}

/*
 {
     "num": 4,
     "scan_status": 4,
     "list": [{
         "name": "Redmi",
         "ad": "80-35-c1-57-97-ba",
         "ct": 0
     }, {
         "name": "",
         "ad": "00-00-00-00-00-00",
         "ct": 0
     }, {
         "name": "JBL Charge 3",
         "ad": "b8-d5-0b-a8-a3-7d",
         "ct": 1
     }, {
         "name": "o$RHYMEET_R1",
         "ad": "01-00-00-00-fc-58",
         "ct": 0
     }]
 }
 */
int avs_ble_list_add_item(struct lp_list_head *dev_list, char *name, char *uuid, char *profile)
{
    int ret = -1;
    char *self_role = NULL;

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "avs_ble_list_add_item start\n");
    if (uuid && name) {
        json_object *json_cmd_parmas = json_object_new_object();
        if (json_cmd_parmas) {
            json_object_object_add(json_cmd_parmas, Key_friendlyName, JSON_OBJECT_NEW_STRING(name));
            json_object_object_add(json_cmd_parmas, Key_uniqueDeviceId,
                                   JSON_OBJECT_NEW_STRING(uuid));
            json_object_object_add(json_cmd_parmas, Key_truncatedMacAddress,
                                   JSON_OBJECT_NEW_STRING(uuid));
            if (StrEq(profile, PROFILE_SOURCE)) {
                self_role = Val_sink;
            } else if (StrEq(profile, PROFILE_SINK)) {
                self_role = Val_source;
            } else {
                self_role = Val_source;
            }
            if (self_role) {
                json_object_object_add(json_cmd_parmas, Val_role,
                                       JSON_OBJECT_NEW_STRING(self_role));
            }
            json_object *json_profile_list = json_object_new_array();
            if (json_profile_list) {
                json_object *json_profile = json_object_new_object();
                if (json_profile) {
                    json_object_object_add(json_profile, Key_name, JSON_OBJECT_NEW_STRING(profile));
                    json_object_object_add(json_profile, Key_version,
                                           JSON_OBJECT_NEW_STRING(Val_unknown_ver));
                    ret = json_object_array_add(json_profile_list, json_profile);
                }
                json_object_object_add(json_cmd_parmas, Key_supportedProfiles, json_profile_list);
            }

            ret = avs_ble_add_a_dev(1, dev_list, json_cmd_parmas);
            json_object_put(json_cmd_parmas);
        }
    }
    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "avs_ble_list_add_item end\n");

    return ret;
}

int avs_ble_parse_list(struct lp_list_head *dev_list, json_object *json_parmas)
{
    int ret = 0;
    int item_num = 0;
    int index = 0;
    json_object *js_list = NULL;

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "avs_ble_parse_list start\n");
    if (json_parmas && dev_list) {
        js_list = json_object_object_get(json_parmas, "list");
        if (js_list) {
            item_num = json_object_array_length(js_list);
            for (index = item_num - 1; index >= 0; index--) {
                json_object *device_item = json_object_array_get_idx(js_list, index);
                if (device_item) {
                    char *name = JSON_GET_STRING_FROM_OBJECT(device_item, "name");
                    char *uuid = JSON_GET_STRING_FROM_OBJECT(device_item, "ad");
                    char *profile = JSON_GET_STRING_FROM_OBJECT(device_item, "role");
                    if (uuid && strlen(uuid) > 0 && name && strlen(name) > 0 && profile &&
                        strlen(profile) > 0) {
                        if (StrEq(profile, "Audio Source")) {
                            profile = PROFILE_SOURCE;
                        } else if (StrEq(profile, "Audio Sink")) {
                            profile = PROFILE_SINK;
                        }
                        ret = avs_ble_list_add_item(dev_list, name, uuid, profile);
                    }
                }
            }
        }
    }
    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "avs_ble_parse_list end\n");

    return ret;
}

/*
 * flags:
 *      0: add scan list
 *      1: update paired list
 *      2: delete item paired list
 *      3: add a item paired list
 *      4: update active device
 *      5: free active device
 */
static int avs_bluetooth_state_update(int flags, json_object *json_cmd_parmas)
{
    int ret = -1;
    json_object *devices = NULL;

    if (!json_cmd_parmas) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "json_cmd_parmas is NULL\n");
        return -1;
    }
    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_bluetooth_state_update start\n");
    devices = json_object_object_get(json_cmd_parmas, Key_devices);
    if (flags == e_add_scan_list) {
        devices = json_object_object_get(json_cmd_parmas, Key_devices);
        if (devices) {
            int item_num = json_object_array_length(devices);
            int i = 0;
            for (i = 0; i < item_num; i++) {
                json_object *device_item = json_object_array_get_idx(devices, i);
                if (device_item) {
                    pthread_mutex_lock(&g_avs_ble.state_lock);
                    ret = avs_ble_add_a_dev(0, &g_avs_ble.scan_list, device_item);
                    pthread_mutex_unlock(&g_avs_ble.state_lock);
                }
            }
        } else {
            int scan_status = JSON_GET_INT_FROM_OBJECT(json_cmd_parmas, "scan_status");
            pthread_mutex_lock(&g_avs_ble.state_lock);
            ret = avs_ble_parse_list(&g_avs_ble.scan_list, json_cmd_parmas);
            pthread_mutex_unlock(&g_avs_ble.state_lock);
            if (scan_status == 4) {
                ret = 1;
            }
        }

    } else if (flags == e_update_paired_list) {
        if (devices) {
            int item_num = json_object_array_length(devices);
            int i = 0;
            for (i = 0; i < item_num; i++) {
                json_object *device_item = json_object_array_get_idx(devices, i);
                if (device_item) {
                    pthread_mutex_lock(&g_avs_ble.state_lock);
                    ret = avs_ble_add_a_dev(1, &g_avs_ble.paired_list, device_item);
                    pthread_mutex_unlock(&g_avs_ble.state_lock);
                }
            }
        } else {
            pthread_mutex_lock(&g_avs_ble.state_lock);
            ret = avs_ble_parse_list(&g_avs_ble.paired_list, json_cmd_parmas);
            pthread_mutex_unlock(&g_avs_ble.state_lock);
        }

    } else if (flags == e_delete_item_paired_list) {
        pthread_mutex_lock(&g_avs_ble.state_lock);
        ret = avs_ble_del_a_dev(&g_avs_ble.paired_list, json_cmd_parmas);
        pthread_mutex_unlock(&g_avs_ble.state_lock);

    } else if (flags == e_add_item_paired_list) {
        pthread_mutex_lock(&g_avs_ble.state_lock);
        ret = avs_ble_add_a_dev(1, &g_avs_ble.paired_list, json_cmd_parmas);
        pthread_mutex_unlock(&g_avs_ble.state_lock);

    } else if (flags == e_update_active_device) {
        pthread_mutex_lock(&g_avs_ble.state_lock);
        avs_ble_update_active(json_cmd_parmas);
        pthread_mutex_unlock(&g_avs_ble.state_lock);

    } else if (flags == e_free_active_device) {
        pthread_mutex_lock(&g_avs_ble.state_lock);
        g_avs_ble.stream_state = e_streaming_inactive;
        avs_ble_free_a_dev(&g_avs_ble.active_dev, 0);
        pthread_mutex_unlock(&g_avs_ble.state_lock);
    }
    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_bluetooth_state_update end\n");

    return ret;
}

// parse the cmd from spotify thread to avs event
// TODO: need free after used
char *avs_bluetooth_parse_cmd(char *json_cmd_str)
{
    json_object *json_cmd = NULL;
    json_object *json_cmd_parmas = NULL;
    const char *cmd_params_str = NULL;
    char *event_type = NULL;
    char *event = NULL;
    char *avs_cmd = NULL;
    char *event_name = NULL;
    json_object *parms_js = NULL;

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

    if (StrEq(event_type, Val_bluetooth)) {
        event = JSON_GET_STRING_FROM_OBJECT(json_cmd, Key_Event);
        if (event) {
            json_cmd_parmas = json_object_object_get(json_cmd, Key_Event_params);
            if (json_cmd_parmas) {
                cmd_params_str = json_object_to_json_string(json_cmd_parmas);
            }
            json_bool ret = JSON_GET_BOOL_FROM_OBJECT(json_cmd_parmas, Key_ret);
            if (StrEq(event, Val_scan)) {
                int ret = avs_bluetooth_state_update(e_add_scan_list, json_cmd_parmas);
                if (ret >= 0) {
                    event_name = NAME_ScanDevicesUpdated;
                    parms_js = json_object_new_object();
                    if (parms_js) {
                        json_object_object_add(parms_js, Key_hasmore,
                                               json_object_new_boolean((ret == 1 ? 0 : 1)));
                        cmd_params_str = json_object_to_json_string(parms_js);
                    } else {
                        cmd_params_str = NULL;
                    }
                } else {
                    event_name = NAME_ScanDevicesFailed;
                    cmd_params_str = NULL;
                }

            } else if (StrEq(event, Val_pair)) {
                if (ret) {
                    event_name = NAME_PairDeviceSucceeded;
                    char *uuid = JSON_GET_STRING_FROM_OBJECT(json_cmd_parmas, Key_uniqueDeviceId);
                    pthread_mutex_lock(&g_avs_ble.state_lock);
                    ble_device_t *ble_dev = avs_ble_find_dev_by_uuid(&g_avs_ble.scan_list, uuid);
                    if (ble_dev && ble_dev->friendly_name && strlen(ble_dev->friendly_name) > 0) {
                        update_key_val(json_cmd_parmas, Key_friendlyName,
                                       JSON_OBJECT_NEW_STRING(ble_dev->friendly_name));
                        cmd_params_str = json_object_to_json_string(json_cmd_parmas);
                    }
                    pthread_mutex_unlock(&g_avs_ble.state_lock);
                    // avs_bluetooth_state_update(e_add_item_paired_list, json_cmd_parmas);
                } else {
                    event_name = NAME_PairDeviceFailed;
                    cmd_params_str = NULL;
                }

            } else if (StrEq(event, Val_play)) {
                if (ret) {
                    event_name = NAME_MediaControlPlaySucceeded;
                    pthread_mutex_lock(&g_avs_ble.state_lock);
                    if (g_avs_ble.stream_state != e_streaming_active) {
                        g_avs_ble.stream_state = e_streaming_active;
                    }
                    pthread_mutex_unlock(&g_avs_ble.state_lock);
                    focus_mgmt_activity_status_set(Bluetooth, FOCUS);
                } else {
                    event_name = NAME_MediaControlPlayFailed;
                }
                cmd_params_str = NULL;

            } else if (StrEq(event, Val_stop)) {
                if (ret) {
                    event_name = NAME_MediaControlStopSucceeded;
                    focus_mgmt_activity_status_set(Bluetooth, UNFOCUS);
                    pthread_mutex_lock(&g_avs_ble.state_lock);
                    if (g_avs_ble.stream_state == e_streaming_active) {
                        g_avs_ble.stream_state = e_streaming_paused;
                    }
                    pthread_mutex_unlock(&g_avs_ble.state_lock);
                } else {
                    event_name = NAME_MediaControlStopFailed;
                }
                cmd_params_str = NULL;

            } else if (StrEq(event, Val_next)) {
                if (ret) {
                    event_name = NAME_MediaControlNextSucceeded;
                } else {
                    event_name = NAME_MediaControlNextFailed;
                }
                cmd_params_str = NULL;

            } else if (StrEq(event, Val_unpair)) {
                if (ret) {
                    event_name = NAME_UnpairDeviceSucceeded;
                    char *uuid = JSON_GET_STRING_FROM_OBJECT(json_cmd_parmas, Key_uniqueDeviceId);
                    pthread_mutex_lock(&g_avs_ble.state_lock);
                    ble_device_t *ble_dev = avs_ble_find_dev_by_uuid(&g_avs_ble.paired_list, uuid);
                    if (ble_dev && ble_dev->friendly_name && strlen(ble_dev->friendly_name) > 0) {
                        update_key_val(json_cmd_parmas, Key_friendlyName,
                                       JSON_OBJECT_NEW_STRING(ble_dev->friendly_name));
                        cmd_params_str = json_object_to_json_string(json_cmd_parmas);
                    }
                    pthread_mutex_unlock(&g_avs_ble.state_lock);
                    // avs_bluetooth_state_update(e_free_active_device, json_cmd_parmas);
                    avs_bluetooth_state_update(e_delete_item_paired_list, json_cmd_parmas);
                    // avs_ble_cfg_save(NULL, NULL, 1);
                } else {
                    event_name = NAME_UnpairDeviceFailed;
                    cmd_params_str = NULL;
                }

            } else if (StrEq(event, Val_previous)) {
                if (ret) {
                    event_name = NAME_MediaControlPreviousSucceeded;
                } else {
                    event_name = NAME_MediaControlPreviousFailed;
                }
                cmd_params_str = NULL;

            } else if (StrEq(event, Val_disconnect)) {
                if (ret) {
                    avs_bluetooth_state_update(e_free_active_device, json_cmd_parmas);
                    event_name = NAME_DisconnectDeviceSucceeded;
#if defined(A98_FRIENDSMINI_ALEXA) || defined(A98_DEWALT)
                    printf("%s,mode:%d\n", __func__, g_wiimu_shm->play_mode);
                    if (PLAYER_MODE_BT == g_wiimu_shm->play_mode)
#endif /* defined(A98_FRIENDSMINI_ALEXA) || defined(A98_DEWALT) */
                    {
#ifndef DISABLE_BT_PROMPT_PLAY
                        avs_ble_prompt(BLE_DISCONNECTED);
#endif
                    }
                } else {
                    event_name = NAME_DisconnectDeviceFailed;
                }

            } else if (StrEq(event, Val_connectbyid)) {
                if (ret) {
#if !defined(ENABLE_BT_SOURCE)
                    pthread_mutex_lock(&g_avs_ble.state_lock);
                    if (g_avs_ble.is_cloud == 1 && g_avs_ble.is_conn_id == 0) {
                        event_name = NAME_ConnectByProfileSucceeded;
                    } else {
                        event_name = NAME_ConnectByDeviceIdSucceeded;
                    }
                    pthread_mutex_unlock(&g_avs_ble.state_lock);
#else
                    event_name = NAME_ConnectByDeviceIdSucceeded;
#endif
                    avs_bluetooth_state_update(e_add_item_paired_list, json_cmd_parmas);
                    avs_bluetooth_state_update(e_update_active_device, json_cmd_parmas);
#ifndef DISABLE_BT_PROMPT_PLAY
                    // avs_ble_prompt(BLE_CONNECTED);
                    SocketClientReadWriteMsg("/tmp/volumeprompt", "common_prompt:12",
                                             strlen("common_prompt:12"), NULL, NULL, 0);
#endif
                } else {
                    event_name = NAME_ConnectByDeviceIdFailed;
                }

            } else if (StrEq(event, Val_getpaired_list)) {
                avs_bluetooth_state_update(e_update_paired_list, json_cmd_parmas);

            } else if (StrEq(event, Val_connectbyprofile)) {
                if (ret) {
                    event_name = NAME_ConnectByProfileSucceeded;
                    avs_bluetooth_state_update(e_add_item_paired_list, json_cmd_parmas);
                    avs_bluetooth_state_update(e_update_active_device, json_cmd_parmas);
                    // avs_ble_prompt(BLE_CONNECTED);
                    SocketClientReadWriteMsg("/tmp/volumeprompt", "common_prompt:12",
                                             strlen("common_prompt:12"), NULL, NULL, 0);
                } else {
                    event_name = NAME_ConnectByProfileFailed;
                }

            } else if (StrEq(event, Val_enable_discoverable)) {
                if (ret) {
                    event_name = NAME_EnterDiscoverableModeSucceeded;
                } else {
                    event_name = NAME_EnterDiscoverableModeFailed;
                }
                cmd_params_str = NULL;
                pthread_mutex_lock(&g_avs_ble.state_lock);
                if (g_avs_ble.is_enable_find == 0) {
                    event_name = NULL;
                } else {
                    g_avs_ble.is_enable_find = 0;
                }
                pthread_mutex_unlock(&g_avs_ble.state_lock);

            } else if (StrEq(event, Val_status)) {
                if (ret) {
                    pthread_mutex_lock(&g_avs_ble.state_lock);
                    if (g_avs_ble.stream_state != e_streaming_active) {
                        g_avs_ble.stream_state = e_streaming_active;
                    }
                    pthread_mutex_unlock(&g_avs_ble.state_lock);
                    focus_mgmt_activity_status_set(Bluetooth, FOCUS);
                } else {
                    focus_mgmt_activity_status_set(Bluetooth, UNFOCUS);
                    pthread_mutex_lock(&g_avs_ble.state_lock);
                    if (g_avs_ble.stream_state == e_streaming_active) {
                        g_avs_ble.stream_state = e_streaming_paused;
                    }
                    pthread_mutex_unlock(&g_avs_ble.state_lock);
                }
            } else if (StrEq(event, "StreamStart")) {
                event_name = "StreamingStarted";
            } else if (StrEq(event, "StreamEnd")) {
                event_name = "StreamingEnded";
                pthread_mutex_lock(&g_avs_ble.state_lock);
                g_avs_ble.stream_state = e_streaming_inactive;
                pthread_mutex_unlock(&g_avs_ble.state_lock);
            }

            if (event_name) {
                Create_Cmd(avs_cmd, 0, NAMESPACE_Bluetooth, event_name, Val_Obj(cmd_params_str));
            }
        }
    }

Exit:
    if (parms_js) {
        json_object_put(parms_js);
    }
    if (json_cmd) {
        json_object_put(json_cmd);
    }

    return avs_cmd;
}

/*
{
  "header": {
    "namespace": "Bluetooth",
    "name": "BluetoothState"
  },
  "payload": {
    "alexaDevice": {
      "friendlyName": "{{STRING}}",
    },
    "pairedDevices": [
      {
        "uniqueDeviceId": "{{STRING}}",
        "friendlyName": "{{STRING}}",
        "supportedProfiles": [
          {
            "name": "{{STRING}}",
            "version": "{{STRING}}"
          },
          {
            ...
          }
        ],
      },
      {
        ...
        ...
      }
    ],
    "activeDevice": {
      "uniqueDeviceId": "{{STRING}}",
      "friendlyName": "{{STRING}}",
      "supportedProfiles": [
        {
          "name": "{{STRING}}",
          "version": "{{STRING}}"
        },
        {
          ...
        }
      ],
      "streaming": "{{STRING}}"
    }
  }
}
*/
int avs_bluetooth_state(json_object *js_context_list)
{
    json_object *ble_state_payload = NULL;
    json_object *ble_alexa_dev = NULL;
    json_object *ble_paired_list = NULL;
    json_object *ble_active_dev = NULL;

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_ble_state start\n");

    if (js_context_list) {
        ble_state_payload = json_object_new_object();
        if (ble_state_payload) {
            ble_alexa_dev = json_object_new_object();
            if (ble_alexa_dev) {
#ifdef A98_EATON
                if (strlen(g_wiimu_shm->device_name) > 0) {
                    json_object_object_add(ble_alexa_dev, PAYLOAD_friendlyName,
                                           JSON_OBJECT_NEW_STRING(g_wiimu_shm->bt_name));
                } else {
                    json_object_object_add(ble_alexa_dev, PAYLOAD_friendlyName,
                                           JSON_OBJECT_NEW_STRING("unknown"));
                }
#else
                if (strlen(g_wiimu_shm->device_name) > 0) {
                    if (StrEq(g_wiimu_shm->project_name, "BUSH_31326")) {
                        json_object_object_add(ble_alexa_dev, PAYLOAD_friendlyName,
                                               JSON_OBJECT_NEW_STRING("B100ALF"));
                    } else if (StrEq(g_wiimu_shm->project_name, "HK_Allure_Portable")) {
                        json_object_object_add(ble_alexa_dev, PAYLOAD_friendlyName,
                                               JSON_OBJECT_NEW_STRING("HK_Allure_Portable"));
                    } else if (StrEq(g_wiimu_shm->project_name, "Astra")) {
                        json_object_object_add(ble_alexa_dev, PAYLOAD_friendlyName,
                                               JSON_OBJECT_NEW_STRING("HK_Astra"));
                    } else if (StrEq(g_wiimu_shm->project_name, "HK_Allure")) {
                        json_object_object_add(ble_alexa_dev, PAYLOAD_friendlyName,
                                               JSON_OBJECT_NEW_STRING("HK_Allure"));
                    } else {
                        json_object_object_add(ble_alexa_dev, PAYLOAD_friendlyName,
                                               JSON_OBJECT_NEW_STRING(g_wiimu_shm->device_name));
                    }
                } else {
                    json_object_object_add(ble_alexa_dev, PAYLOAD_friendlyName,
                                           JSON_OBJECT_NEW_STRING("unknown"));
                }
#endif
                json_object_object_add(ble_state_payload, PAYLOAD_alexaDevice, ble_alexa_dev);
            }

            ble_paired_list = json_object_new_array();
            if (ble_paired_list) {
                pthread_mutex_lock(&g_avs_ble.state_lock);
                avs_ble_get_dev_list(1, &g_avs_ble.paired_list, ble_paired_list);
                pthread_mutex_unlock(&g_avs_ble.state_lock);
                json_object_object_add(ble_state_payload, PAYLOAD_pairedDevices, ble_paired_list);
            }

            pthread_mutex_lock(&g_avs_ble.state_lock);
            ble_active_dev = avs_ble_get_a_device(1, &g_avs_ble.active_dev);
            pthread_mutex_unlock(&g_avs_ble.state_lock);
            if (ble_active_dev) {
                char *streaming_active = NULL;
                pthread_mutex_lock(&g_avs_ble.state_lock);
                if (g_avs_ble.active_dev.uuid && strlen(g_avs_ble.active_dev.uuid) > 0) {
                    if (g_avs_ble.stream_state == e_streaming_active) {
                        streaming_active = STREAMING_ACTIVE;
                    } else if (g_avs_ble.stream_state == e_streaming_inactive) {
                        streaming_active = STREAMING_INACTIVE;
                    } else if (g_avs_ble.stream_state == e_streaming_paused) {
                        if (g_wiimu_shm->play_mode == PLAYER_MODE_BT)
                            streaming_active = STREAMING_PAUSED;
                        else
                            streaming_active = STREAMING_INACTIVE;
                    }
                }
                pthread_mutex_unlock(&g_avs_ble.state_lock);
                json_object_object_add(ble_active_dev, PAYLOAD_streaming,
                                       JSON_OBJECT_NEW_STRING(streaming_active));
                json_object_object_add(ble_state_payload, PAYLOAD_activeDevice, ble_active_dev);
            }
            alexa_context_list_add(js_context_list, NAMESPACE_Bluetooth, NAME_BluetoothState,
                                   ble_state_payload);
        }
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_ble_state end\n");

    return -1;
}

static int avs_ble_update_last_piared_dev(struct lp_list_head *dev_list, char *uuid,
                                          char *friendly_name)
{
    int ret = -1;
    int is_same = -1;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    ble_device_t *device_item = NULL;

    if (!uuid || !dev_list) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "uuid/dev_list is NULL\n");
        return -1;
    }

    ALEXA_PRINT(ALEXA_DEBUG_ERR, "start\n");
    if (!lp_list_empty(dev_list)) {
        lp_list_for_each_safe(pos, npos, dev_list)
        {
            device_item = lp_list_entry(pos, ble_device_t, ble_item);
            if (device_item) {
                if (StrCaseEq(device_item->uuid, uuid)) {
                    is_same = 0;
                    break;
                }
            }
        }
    }

    if (is_same != 0) {
        // add new information
        device_item = (ble_device_t *)calloc(1, sizeof(ble_device_t));
        if (device_item) {
            LP_INIT_LIST_HEAD(&device_item->profile_list);
            if (uuid) {
                device_item->uuid = strdup(uuid);
            }
            if (friendly_name && strlen(friendly_name) > 0) {
                device_item->friendly_name = strdup(friendly_name);
            } else {
                device_item->friendly_name = strdup(Val_defdevice);
            }
            avs_ble_add_a_profile(&device_item->profile_list, PROFILE_SOURCE, PROFILE_VER2_0);
            avs_ble_add_a_profile(&device_item->profile_list, PROFILE_AVRCP, PROFILE_VER1_0);
            lp_list_add_tail(&device_item->ble_item, dev_list);
            ret = 0;
        } else {
            ALEXA_PRINT(ALEXA_DEBUG_ERR, "calloc failed\n");
        }
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "end\n");

    return ret;
}

static int avs_ble_cfg_init(struct lp_list_head *dev_list)
{
    int ret = -1;
    char last_paired_mac[128];
    char last_paired_devname[128];

    // if (dev_list) {
    if (0) {
        asr_config_get(LAST_PAIED_BLE_MAC, last_paired_mac, sizeof(last_paired_mac));
        if (strlen(last_paired_mac) > 0) {
            ALEXA_PRINT(ALEXA_DEBUG_TRACE, "last_paired_mac=%s\n", last_paired_mac);
        }

        asr_config_get(LAST_PAIED_BLE_DEVNAME, last_paired_devname, sizeof(last_paired_devname));
        if (strlen(last_paired_devname) > 0) {
            ALEXA_PRINT(ALEXA_DEBUG_TRACE, "last_paired_devname=%s\n", last_paired_devname);
        }

        if (strlen(last_paired_mac) > 0) {
            ret = avs_ble_update_last_piared_dev(dev_list, last_paired_mac, last_paired_devname);
        }
    }

    return ret;
}

int avs_bluetooth_init(void)
{
    int ret = 0;

    memset(&g_avs_ble, 0x0, sizeof(avs_ble_t));
    LP_INIT_LIST_HEAD(&g_avs_ble.paired_list);
    avs_ble_cfg_init(&g_avs_ble.paired_list);
    LP_INIT_LIST_HEAD(&g_avs_ble.scan_list);
    LP_INIT_LIST_HEAD(&g_avs_ble.active_dev.profile_list);

    pthread_mutex_init(&g_avs_ble.state_lock, NULL);
    return ret;
}

int avs_bluetooth_uninit(void)
{
    // TODO: free the malloc data, free paired list scan list

    return 0;
}

int avs_bluetooth_set_inactivae(void)
{
    char *cmd = NULL;
    char *params = NULL;

    pthread_mutex_lock(&g_avs_ble.state_lock);
    if (g_wiimu_shm->play_mode == PLAYER_MODE_BT && g_avs_ble.stream_state == e_streaming_active) {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=PLAY_AUTO_SWITCH:041",
                                 strlen("GNOTIFY=PLAY_AUTO_SWITCH:041"), NULL, NULL, 0);
        Create_Internal_BluetoothAPI_Directive_MediaControl_Params(params, Val_stop);
        if (params) {
            Create_Internal_BluetoothAPI_Directive_Cmd(cmd, Cmd_MediaControl, params);
            free(params);
        }
        if (cmd) {
            BluetoothAPI_Directive_Send(cmd, strlen(cmd), 0);
            free(cmd);
        }
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=PLAY_AUTO_SWITCH:010",
                                 strlen("GNOTIFY=PLAY_AUTO_SWITCH:010"), NULL, NULL, 0);

        g_avs_ble.stream_state = e_streaming_inactive;
        g_avs_ble.need_silence = 1;
    }
    pthread_mutex_unlock(&g_avs_ble.state_lock);

    return 0;
}

int avs_bluetooth_need_slience(void)
{
    int need_silence = 0;

    pthread_mutex_lock(&g_avs_ble.state_lock);
    need_silence = g_avs_ble.need_silence;
    if (g_avs_ble.need_silence == 1) {
        g_avs_ble.need_silence = 0;
    }
    pthread_mutex_unlock(&g_avs_ble.state_lock);

    return need_silence;
}

/* parse directive  */
int avs_bluetooth_parse_directive(json_object *js_data)
{
    int ret = -1;

    if (js_data) {
        json_object *json_header = json_object_object_get(js_data, KEY_HEADER);
        if (json_header) {
            char *name = JSON_GET_STRING_FROM_OBJECT(json_header, KEY_NAME);
            if (name) {
                if (StrEq(name, NAME_ScanDevices)) {
                    ret = avs_ble_scan(js_data);
                } else if (StrEq(name, NAME_EnterDiscoverableMode)) {
                    ret = avs_ble_startdiscovery(js_data);

                } else if (StrEq(name, NAME_ExitDiscoverableMode)) {
                    ret = avs_ble_enddiscovery(js_data);

                } else if (StrEq(name, NAME_PairDevice)) {
                    ret = avs_ble_pair(js_data);

                } else if (StrEq(name, NAME_UnpairDevice)) {
                    ret = avs_ble_unpair(js_data);

                } else if (StrEq(name, NAME_ConnectByDeviceId)) {
                    ret = avs_ble_connect_id(js_data);

                } else if (StrEq(name, NAME_ConnectByProfile)) {
                    ret = avs_ble_connect_profile(js_data);

                } else if (StrEq(name, NAME_DisconnectDevice)) {
                    ret = avs_ble_disconnect(js_data);

                } else if (StrEq(name, NAME_Ble_Play)) {
                    ret = avs_ble_play(js_data);

                } else if (StrEq(name, NAME_Ble_Stop)) {
                    ret = avs_ble_stop(js_data);

                } else if (StrEq(name, NAME_Ble_Next)) {
                    ret = avs_ble_next(js_data);

                } else if (StrEq(name, NAME_Ble_Previous)) {
                    ret = avs_ble_prevoius(js_data);

                } else {
                    ret = -1;
                }
            }
        }
    }

    if (ret != 0) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "prase message error\n");
    }

    return ret;
}

json_object *avs_ble_event_payload(char *cmd_name, json_object *cmd_parms)
{
    int ret = 0;
    json_object *json_event_payload = NULL;

    if (!cmd_name) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "cmd_name is NULL\n");
    }

    json_event_payload = json_object_new_object();
    if (!json_event_payload) {
        goto Exit;
    }

    if (StrEq(cmd_name, NAME_ConnectByDeviceIdSucceeded) ||
        StrEq(cmd_name, NAME_ConnectByProfileSucceeded)) {
        char *uuid = JSON_GET_STRING_FROM_OBJECT(cmd_parms, Key_uniqueDeviceId);
        char *friendly_name = JSON_GET_STRING_FROM_OBJECT(cmd_parms, Key_friendlyName);
        avs_ble_cfg_save(uuid, friendly_name, 0);
    }

    if (StrEq(cmd_name, NAME_ScanDevicesUpdated)) {
        json_bool has_more = JSON_GET_BOOL_FROM_OBJECT(cmd_parms, Key_hasmore);

        json_object *devices_list = json_object_new_array();
        if (devices_list) {
            pthread_mutex_lock(&g_avs_ble.state_lock);
            avs_ble_get_dev_list(0, &g_avs_ble.scan_list, devices_list);
            pthread_mutex_unlock(&g_avs_ble.state_lock);
            json_object_object_add(json_event_payload, PAYLOAD_discoveredDevices, devices_list);
        }

        json_object_object_add(json_event_payload, PAYLOAD_hasMore,
                               json_object_new_boolean(has_more));

    } else if (StrEq(cmd_name, NAME_PairDeviceSucceeded) ||
               StrEq(cmd_name, NAME_UnpairDeviceSucceeded)) {
        json_object *device = json_object_new_object();
        if (device) {
            char *uuid = JSON_GET_STRING_FROM_OBJECT(cmd_parms, Key_uniqueDeviceId);
            char *friendly_name = JSON_GET_STRING_FROM_OBJECT(cmd_parms, Key_friendlyName);

            json_object_object_add(device, Key_uniqueDeviceId, JSON_OBJECT_NEW_STRING(uuid));
            json_object_object_add(device, Key_friendlyName, JSON_OBJECT_NEW_STRING(friendly_name));
            json_object_object_add(json_event_payload, PAYLOAD_device, device);
        }

    } else if (StrEq(cmd_name, NAME_ConnectByDeviceIdSucceeded) ||
               StrEq(cmd_name, NAME_ConnectByDeviceIdFailed)) {
        json_object *device = json_object_new_object();
        if (device) {
            char *uuid = JSON_GET_STRING_FROM_OBJECT(cmd_parms, Key_uniqueDeviceId);
            char *friendly_name = JSON_GET_STRING_FROM_OBJECT(cmd_parms, Key_friendlyName);

            json_object_object_add(device, Key_uniqueDeviceId, JSON_OBJECT_NEW_STRING(uuid));
            json_object_object_add(device, Key_friendlyName, JSON_OBJECT_NEW_STRING(friendly_name));
            json_object_object_add(json_event_payload, PAYLOAD_device, device);
            pthread_mutex_lock(&g_avs_ble.state_lock);
            if (g_avs_ble.is_cloud == 1) {
                g_avs_ble.is_cloud = 0;
                json_object_object_add(json_event_payload, PAYLOAD_requester,
                                       JSON_OBJECT_NEW_STRING(Val_requester_cloud));
            } else {
                json_object_object_add(json_event_payload, PAYLOAD_requester,
                                       JSON_OBJECT_NEW_STRING(Val_requester_device));
            }
            pthread_mutex_unlock(&g_avs_ble.state_lock);
        }

    } else if (StrEq(cmd_name, NAME_ConnectByProfileSucceeded)) {
        json_object *device = json_object_new_object();
        if (device) {
            char *uuid = JSON_GET_STRING_FROM_OBJECT(cmd_parms, Key_uniqueDeviceId);
            char *profile = JSON_GET_STRING_FROM_OBJECT(cmd_parms, Key_profileName);
            char *friendly_name = JSON_GET_STRING_FROM_OBJECT(cmd_parms, Key_friendlyName);

            char uui_cfg[256] = {0};
            char friendly_name_cfg[256] = {0};
            if (!uuid || strlen(uuid) == 0) {
                asr_config_get(LAST_PAIED_BLE_MAC, uui_cfg, sizeof(uui_cfg));
                uuid = uui_cfg;
            }

            if (!friendly_name || (friendly_name && strlen(friendly_name) == 0)) {
                asr_config_get(LAST_PAIED_BLE_DEVNAME, friendly_name_cfg,
                               sizeof(friendly_name_cfg));
                friendly_name = friendly_name_cfg;
            }

            if (!profile || (profile && strlen(profile) == 0)) {
                profile = PROFILE_SOURCE;
            } else {
                if (StrEq(profile, Val_source)) {
                    profile = PROFILE_SINK;
                } else if (StrEq(profile, Val_sink)) {
                    profile = PROFILE_SOURCE;
                }
            }

            json_object_object_add(device, Key_uniqueDeviceId, JSON_OBJECT_NEW_STRING(uuid));
            json_object_object_add(device, Key_friendlyName, JSON_OBJECT_NEW_STRING(friendly_name));
            json_object_object_add(json_event_payload, PAYLOAD_device, device);
            pthread_mutex_lock(&g_avs_ble.state_lock);
            if (g_avs_ble.is_cloud == 1) {
                g_avs_ble.is_cloud = 0;
                json_object_object_add(json_event_payload, PAYLOAD_requester,
                                       JSON_OBJECT_NEW_STRING(Val_requester_cloud));
            } else {
                json_object_object_add(json_event_payload, PAYLOAD_requester,
                                       JSON_OBJECT_NEW_STRING(Val_requester_device));
            }
            pthread_mutex_unlock(&g_avs_ble.state_lock);
            json_object_object_add(json_event_payload, Key_profileName,
                                   JSON_OBJECT_NEW_STRING(profile));
        }

    } else if (StrEq(cmd_name, NAME_ConnectByProfileFailed)) {
        char *profile = JSON_GET_STRING_FROM_OBJECT(cmd_parms, Key_profileName);

        pthread_mutex_lock(&g_avs_ble.state_lock);
        if (g_avs_ble.is_cloud == 1) {
            g_avs_ble.is_cloud = 0;
            json_object_object_add(json_event_payload, PAYLOAD_requester,
                                   JSON_OBJECT_NEW_STRING(Val_requester_cloud));
        } else {
            json_object_object_add(json_event_payload, PAYLOAD_requester,
                                   JSON_OBJECT_NEW_STRING(Val_requester_device));
        }
        pthread_mutex_unlock(&g_avs_ble.state_lock);
        json_object_object_add(json_event_payload, Key_profileName,
                               JSON_OBJECT_NEW_STRING(profile));

    } else if (StrEq(cmd_name, NAME_DisconnectDeviceSucceeded) ||
               StrEq(cmd_name, NAME_DisconnectDeviceFailed)) {
        json_object *device = json_object_new_object();
        if (device) {
            char *uuid = JSON_GET_STRING_FROM_OBJECT(cmd_parms, Key_uniqueDeviceId);
            char *friendly_name = JSON_GET_STRING_FROM_OBJECT(cmd_parms, Key_friendlyName);

            json_object_object_add(device, Key_uniqueDeviceId, JSON_OBJECT_NEW_STRING(uuid));
            json_object_object_add(device, Key_friendlyName, JSON_OBJECT_NEW_STRING(friendly_name));
            json_object_object_add(json_event_payload, PAYLOAD_device, device);
            pthread_mutex_lock(&g_avs_ble.state_lock);
            if (g_avs_ble.is_cloud == 1) {
                g_avs_ble.is_cloud = 0;
                json_object_object_add(json_event_payload, PAYLOAD_requester,
                                       JSON_OBJECT_NEW_STRING(Val_requester_cloud));
            } else {
                json_object_object_add(json_event_payload, PAYLOAD_requester,
                                       JSON_OBJECT_NEW_STRING(Val_requester_device));
            }
            pthread_mutex_unlock(&g_avs_ble.state_lock);
        }

    } else if (StrEq(cmd_name, NAME_ScanDevicesFailed) ||
               StrEq(cmd_name, NAME_EnterDiscoverableModeSucceeded) ||
               StrEq(cmd_name, NAME_EnterDiscoverableModeFailed) ||
               StrEq(cmd_name, NAME_PairDeviceFailed) || StrEq(cmd_name, NAME_UnpairDeviceFailed) ||
               StrEq(cmd_name, NAME_MediaControlPlaySucceeded) ||
               StrEq(cmd_name, NAME_MediaControlPlayFailed) ||
               StrEq(cmd_name, NAME_MediaControlStopSucceeded) ||
               StrEq(cmd_name, NAME_MediaControlStopFailed) ||
               StrEq(cmd_name, NAME_MediaControlNextSucceeded) ||
               StrEq(cmd_name, NAME_MediaControlNextFailed) ||
               StrEq(cmd_name, NAME_MediaControlPreviousSucceeded) ||
               StrEq(cmd_name, NAME_MediaControlPreviousFailed)) {
        // empty
        ret = 0;
    } else if (StrEq(cmd_name, "StreamingStarted") || StrEq(cmd_name, "StreamingEnded")) {
        json_object *device = json_object_new_object();
        if (device) {
            char *uuid = JSON_GET_STRING_FROM_OBJECT(cmd_parms, Key_uniqueDeviceId);
            json_object_object_add(device, Key_uniqueDeviceId, JSON_OBJECT_NEW_STRING(uuid));
            json_object_object_add(json_event_payload, PAYLOAD_device, device);
        }
    } else {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "unknown name\n");
        ret = -1;
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
json_object *avs_bluetooth_event_create(json_object *json_cmd)
{
    int ret = -1;
    char *cmd_name = NULL;
    json_object *json_event_header = NULL;
    json_object *json_event_payload = NULL;
    json_object *json_event = NULL;
    json_object *json_cmd_parms = NULL;

    if (!json_cmd) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "input error\n");
        goto Exit;
    }

    cmd_name = JSON_GET_STRING_FROM_OBJECT(json_cmd, KEY_NAME);
    if (!cmd_name) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "cmd none name found\n");
        goto Exit;
    }

    json_event_header = alexa_event_header(NAMESPACE_Bluetooth, cmd_name);
    json_cmd_parms = json_object_object_get(json_cmd, KEY_params);
    json_event_payload = avs_ble_event_payload(cmd_name, json_cmd_parms);

    if (json_event_header && json_event_payload) {
        json_event = alexa_event_body(json_event_header, json_event_payload);
        if (json_event) {
            ret = 0;
        }
    }

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
