
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <pthread.h>

#include "asr_bluetooth.h"
#include "asr_bluetooth_cmd.h"

#include "asr_cfg.h"
#include "asr_debug.h"
#include "asr_player.h"
#include "asr_json_common.h"
#include "asr_request_json.h"

#include "wm_util.h"
#include "lp_list.h"

#include "common_flags.h"

#include "clova_device.h"
#include "clova_clova.h"
#include "clova_device_control.h"
#include "clova_speech_synthesizer.h"

// The speeker act as server
#define PROFILE_SOURCE ("A2DP-SOURCE") // A2DP-SOURCE
// The speeker act as client
#define PROFILE_SINK ("A2DP-SINK") // A2DP-SINK
#define PROFILE_AVRCP ("AVRCP")
#define PROFILE_VER1_0 ("1.0")
#define PROFILE_VER2_0 ("2.0")

#define Key_ret ("ret")
#define Key_name ("name")
#define Key_devices ("devices")
#define Key_address ("address")
#define Key_version ("version")
#define Key_connected ("connected")
#define Key_role ("role")
#define Key_profileName ("profileName")
#define Key_friendlyName ("friendlyName")
#define Key_uniqueDeviceId ("uniqueDeviceId")
#define Key_supportedProfiles ("supportedProfiles")
#define Key_truncatedMacAddress ("truncatedMacAddress")

#define Val_sink ("sink")
#define Val_scan ("scan")
#define Val_delete ("delete")
#define Val_status ("status")
#define Val_btplay ("btplay")
#define Val_source ("source")
#define Val_sound_path ("sound_path")
#define Val_disconnect ("disconnect")
#define Val_connectbyid ("connectbyid")
#define Val_getpaired_list ("getpaired_list")
#define Val_connectbyprofile ("connectbyprofile")
#define Val_enable_discoverable ("enable_discoverable")
#define Val_disable_discoverable ("disable_discoverable")
#define Val_diconnect_bt ("btavkdisconnect")
#define Val_cleanbthistory ("cleanbthistory")

#define Val_metadata ("update_metadata")
#define Key_title ("title")
#define Key_artist ("artist")
#define Key_album ("album")

#define Val_defdevice ("my phone")
#define Val_unknown_ver ("1.0")
#define Val_unknown_text ("알 수 없는")

#define MAX_PAIRED_LIST (5)

#define GET_CONNECT_TTS ("에 연결되었습니다.")
//#define IS_CONNECTTED               ("%s 에 연결 되어 있습니다.")
#define GET_DISCONNECT_TTS ("%s 기기와 연결이 해제 되었습니다.")
#define DISCONNECT_TXT ("기기와 연결이 해제 되었습니다.")
#define SINK_GUIDE_TXT ("다음부터는 \"내폰에 연결해줘\" 라고 말씀해보세요.")
#define SOURCE_GUIDE_TXT ("다음부터는 \"블루투스 스피커에 연결해줘\" 라고 말씀해보세요.")
#define CONNECT_FAILED                                                                                                \
    ("%s 에 연결할 수 없습니다. 연결할 기기의 블루투스가 켜져있는지 확인하시고 다시 " \
     "시도해주세요.")
#define MAINTAIN_CONNECTION_TXT                                                                                                        \
    ("이미 %s 에 연결되어 있습니다. 다른 기기와 연결하시려면 클로바 앱에서 연결할 기기를 선택해 " \
     "주세요.")
#define GET_START_PAIR_TTS                                                                                                    \
    ("블루투스를 연결할 준비가 되었습니다. 연결할 기기의 블루투스 설정목록에서 %s 항목을 " \
     "선택하시거나, 클로바 앱에서 연결할 블루투스 기기를 선택해 주세요.")
#define START_PAIR_TXT ("블루투스를 연결할 준비가 되었습니다.")
#define NO_DEVICE_CONNECTED ("연결된 기기가 없습니다.")
//#define NOT_SUPPORT_FUNCTION        ("블루투스에서는 지원되지 않는 기능입니다.")
//#define NO_DEVICE_TO_CONNECT        ("연결할 기기를 찾을 수 없습니다.")

#define Check_Version(version) (((version) && strlen((version)) > 0) ? (version) : Val_unknown_ver)

enum {
    e_role_none = -1,
    e_role_sink,
    e_role_source,
};

typedef enum {
    e_add_scan_list,           // 0
    e_update_paired_list,      // 1
    e_delete_item_paired_list, // 2
    e_add_item_paired_list,    // 3
    e_update_active_device,    // 4
    e_free_active_device,      // 5
};

enum {
    e_off,
    e_on,
};

typedef struct {
    int is_connected;
    char *uuid;
    char *friendly_name;
    char *mac_address;
    char *act_role;
    struct lp_list_head ble_item;
} ble_device_t;

typedef struct {
    int status;
    int pairing_status;
    int scanning_status;
    int connecting_status;
    int start_connect;
    int play_status;
    int need_disconnect;
    char *title;
    char *artist;
    char *album;
    char *request_txt;
    pthread_mutex_t state_lock;
    ble_device_t active_dev;
    struct lp_list_head paired_list;
    struct lp_list_head scan_list;
} asr_ble_t;

static asr_ble_t g_asr_ble = {0};
static int asr_ble_del_a_dev(struct lp_list_head *dev_list, char *uuid);

static int asr_ble_scan(json_object *js_data)
{
    int ret = -1;
    char *cmd = NULL;
    char *params = NULL;
    long timeout_ms = 0;

    if (js_data) {
        json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
        if (js_payload) {
            ASR_PRINT(ASR_DEBUG_INFO, "Bluetooth Process(directive scan) recv. +++++++\n");
            pthread_mutex_lock(&g_asr_ble.state_lock);
            if (g_asr_ble.start_connect == e_off) {
                Create_Bluetooth_Scan_Params(params, timeout_ms);
                if (params) {
                    Create_Bluetooth_Cmd(cmd, Val_scan, params);
                    free(params);
                }
                if (cmd) {
                    Bluetooth_Cmd_Send(cmd, strlen(cmd), 0);
                    ret = asr_ble_del_a_dev(&g_asr_ble.scan_list, NULL);
                    g_asr_ble.scanning_status = e_on;
                    free(cmd);
                    ret = 0;
                }
            }
            pthread_mutex_unlock(&g_asr_ble.state_lock);
        }
    }

    return ret;
}

static int asr_ble_delete(json_object *js_data)
{
    int ret = -1;
    char *address = NULL;
    char *role = NULL;
    char *cmd = NULL;
    char *params = NULL;

    if (js_data) {
        json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
        if (js_payload) {
            ASR_PRINT(ASR_DEBUG_INFO, "Bluetooth Process(directive delete) recv. +++++++\n");
            address = JSON_GET_STRING_FROM_OBJECT(js_payload, CLOVA_PAYLOAD_NAME_ADDRESS);
            role = JSON_GET_STRING_FROM_OBJECT(js_payload, CLOVA_PAYLOAD_NAME_ROLE);
            if (address && role) {
                Create_Bluetooth_Comm_Params(params, address, role);
                if (params) {
                    Create_Bluetooth_Cmd(cmd, Val_delete, params);
                    free(params);
                }
                if (cmd) {
                    Bluetooth_Cmd_Send(cmd, strlen(cmd), 0);
                    free(cmd);
                    ret = 0;
                    pthread_mutex_lock(&g_asr_ble.state_lock);
                    g_asr_ble.connecting_status = e_off;
                    g_asr_ble.pairing_status = e_off;
                    g_asr_ble.scanning_status = e_off;
                    g_asr_ble.start_connect = e_off;
                    pthread_mutex_unlock(&g_asr_ble.state_lock);
                }
            }
        }
    }

    return ret;
}

static int asr_ble_play(json_object *js_data)
{
    int ret = -1;
    char *address = NULL;
    char *role = NULL;
    char *cmd = NULL;

    if (js_data) {
        json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
        if (js_payload) {
            ASR_PRINT(ASR_DEBUG_INFO, "Bluetooth Process(directive bt play) recv. +++++++\n");
            Create_Bluetooth_Cmd(cmd, Val_btplay, Val_Obj(NULL));
            if (cmd) {
                Bluetooth_Cmd_Send(cmd, strlen(cmd), 0);
                free(cmd);
                ret = 0;
            }
        }
    }

    return ret;
}

static int asr_ble_startpairing(json_object *js_data)
{
    int ret = -1;
    char *cmd = NULL;
    char *params = NULL;

    if (js_data) {
        json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
        if (js_payload) {
            Create_Bluetooth_EnterDiscoverableMode_Params(params, 0);
            if (params) {
                Create_Bluetooth_Cmd(cmd, Val_enable_discoverable, params);
                free(params);
            }
            if (cmd) {
                Bluetooth_Cmd_Send(cmd, strlen(cmd), 0);
                free(cmd);
                ret = 0;
            }
        }
    }

    return ret;
}

static int asr_ble_stoppairing(json_object *js_data)
{
    int ret = -1;
    char *cmd = NULL;

    if (js_data) {
        json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
        if (js_payload) {
            ASR_PRINT(ASR_DEBUG_INFO,
                      "Bluetooth Process(directive ExitDiscoverableMode) recv. +++++++\n");
            Create_Bluetooth_Cmd(cmd, Val_disable_discoverable, Val_Obj(NULL));
            if (cmd) {
                Bluetooth_Cmd_Send(cmd, strlen(cmd), 0);
                free(cmd);
                ret = 0;
                pthread_mutex_lock(&g_asr_ble.state_lock);
                g_asr_ble.pairing_status = e_off;
                pthread_mutex_unlock(&g_asr_ble.state_lock);
            }
        }

        return ret;
    }

    return ret;
}

static int asr_ble_notify_connect(char *address, char *role)
{
    int ret = -1;
    char *cmd = NULL;
    char *params = NULL;

    ASR_PRINT(ASR_DEBUG_INFO, "recv address=%s role=%s +++++++", address, role);
    Create_Bluetooth_Comm_Params(params, Val_Str(address), Val_Str(role));
    if (params) {
        Create_Bluetooth_Cmd(cmd, Val_connectbyid, params);
        free(params);
    }
    if (cmd) {
        Bluetooth_Cmd_Send(cmd, strlen(cmd), 0);
        free(cmd);
        ret = 0;
        pthread_mutex_lock(&g_asr_ble.state_lock);
        g_asr_ble.connecting_status = e_on;
        g_asr_ble.scanning_status = e_off;
        g_asr_ble.pairing_status = e_off;
        g_asr_ble.start_connect = e_on;
        pthread_mutex_unlock(&g_asr_ble.state_lock);
    }

    return ret;
}

static int asr_ble_notify_disconnect(char *address, char *role)
{
    int ret = -1;
    char *cmd = NULL;
    char *params = NULL;

    ASR_PRINT(ASR_DEBUG_INFO, "recv address=%s role=%s+++++++\n", address, role);
    Create_Bluetooth_Comm_Params(params, Val_Str(address), Val_Str(role));
    if (params) {
        Create_Bluetooth_Cmd(cmd, Val_disconnect, params);
        free(params);
    }
    if (cmd) {
        Bluetooth_Cmd_Send(cmd, strlen(cmd), 0);
        free(cmd);
        ret = 0;
        pthread_mutex_lock(&g_asr_ble.state_lock);
        g_asr_ble.connecting_status = e_off;
        g_asr_ble.scanning_status = e_off;
        g_asr_ble.start_connect = e_off;
        pthread_mutex_unlock(&g_asr_ble.state_lock);
    }

    return ret;
}

static int asr_ble_connect_id(json_object *js_data)
{
    int ret = -1;
    int need_tts = 0;
    char *name = NULL;
    char *address = NULL;
    char *role = NULL;
    char *local_role = NULL;
    int connected = -1;
    char *request_txt = NULL;

    if (js_data) {
        json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
        if (js_payload) {
            name = JSON_GET_STRING_FROM_OBJECT(js_payload, CLOVA_PAYLOAD_NAME_NAME);
            address = JSON_GET_STRING_FROM_OBJECT(js_payload, CLOVA_PAYLOAD_NAME_ADDRESS);
            role = JSON_GET_STRING_FROM_OBJECT(js_payload, CLOVA_PAYLOAD_NAME_ROLE);
            connected = JSON_GET_BOOL_FROM_OBJECT(js_payload, CLOVA_PAYLOAD_NAME_CONNECTED);

            if (StrEq(role, Val_sink)) {
                local_role = PROFILE_SOURCE;
            } else if (StrEq(role, Val_source)) {
                local_role = PROFILE_SINK;
            }
            pthread_mutex_lock(&g_asr_ble.state_lock);
            if (!address || (address && strlen(address) == 0)) {
                if ((!role) || (g_asr_ble.active_dev.uuid &&
                                StrEq(local_role, g_asr_ble.active_dev.act_role))) {
                    if (g_asr_ble.active_dev.uuid && g_asr_ble.active_dev.friendly_name) {
                        need_tts = 1;
                    }
                }
            }
            if (need_tts) {
                if (g_asr_ble.active_dev.friendly_name) {
                    asprintf(&request_txt, MAINTAIN_CONNECTION_TXT,
                             g_asr_ble.active_dev.friendly_name);
                    if (request_txt) {
                        clova_speech_request_cmd(request_txt, CLOVA_PAYLOAD_VALUE_KO);
                        free(request_txt);
                        ret = 0;
                        g_asr_ble.start_connect = e_off;
                    }
                } else {
                    ASR_PRINT(ASR_DEBUG_INFO, "!!!!!!!!!! It cannot happened !!!!!!!!!!!!\n");
                }
            }
            pthread_mutex_unlock(&g_asr_ble.state_lock);
            if (need_tts == 0) {
                ret = asr_ble_notify_connect(address, role);
            }
        }

        return ret;
    }

    return ret;
}

static int asr_ble_connect_pin_code(json_object *js_data)
{
    int ret = -1;
    char *pin_code = NULL;

    if (js_data) {
        json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
        if (js_payload) {
            pin_code = JSON_GET_STRING_FROM_OBJECT(js_payload, CLOVA_PAYLOAD_NAME_PIN_CODE);
            if (pin_code) {
                ASR_PRINT(ASR_DEBUG_INFO, "recv pin_code=%s +++++++\n", pin_code);
            }
        }

        return ret;
    }

    return ret;
}

static int asr_ble_disconnect(json_object *js_data)
{
    int ret = -1;
    char *name = NULL;
    char *address = NULL;
    char *role = NULL;
    int connected = -1;
    char *cmd = NULL;
    char *params = NULL;
    int no_device_connect = 0;

    if (js_data) {
        json_object *js_payload = json_object_object_get(js_data, KEY_PAYLOAD);
        if (js_payload) {
            name = JSON_GET_STRING_FROM_OBJECT(js_payload, CLOVA_PAYLOAD_NAME_NAME);
            address = JSON_GET_STRING_FROM_OBJECT(js_payload, CLOVA_PAYLOAD_NAME_ADDRESS);
            role = JSON_GET_STRING_FROM_OBJECT(js_payload, CLOVA_PAYLOAD_NAME_ROLE);
            connected = JSON_GET_BOOL_FROM_OBJECT(js_payload, CLOVA_PAYLOAD_NAME_CONNECTED);

            pthread_mutex_lock(&g_asr_ble.state_lock);
            if (g_asr_ble.pairing_status == e_off && !g_asr_ble.active_dev.uuid) {
                if ((!role && !address) ||
                    ((role && strlen(role) == 0) && (address && strlen(address) == 0))) {
                    clova_speech_request_cmd(NO_DEVICE_CONNECTED, CLOVA_PAYLOAD_VALUE_KO);
                    no_device_connect = 1;
                }
            }
            pthread_mutex_unlock(&g_asr_ble.state_lock);
            if (no_device_connect == 0) {
                ret = asr_ble_notify_disconnect(address, role);
            }
        }
    }

    return ret;
}

void asr_ble_getpaired_list(void)
{
    char *cmd = NULL;

    ASR_PRINT(ASR_DEBUG_INFO, "Bluetooth Process(getpaired_list). +++++++\n");
    Create_Bluetooth_Cmd(cmd, Val_getpaired_list, Val_Obj(0));
    if (cmd) {
        Bluetooth_Cmd_Send(cmd, strlen(cmd), 0);
        free(cmd);
    }
}

static json_object *asr_ble_get_a_device(int paired_list, ble_device_t *ble_dev)
{
    int flags = 0;
    json_object *ble_dev_json = NULL;
    json_bool is_connected = 0;
    json_object *is_connected_js = NULL;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;

    ASR_PRINT(ASR_DEBUG_INFO, "asr_ble_get_a_device start\n");

    if (ble_dev && ble_dev->uuid) {
        ble_dev_json = json_object_new_object();
        if (ble_dev_json) {
            json_object_object_add(ble_dev_json, Key_address,
                                   JSON_OBJECT_NEW_STRING(ble_dev->uuid));
            json_object_object_add(ble_dev_json, Key_name,
                                   JSON_OBJECT_NEW_STRING(ble_dev->friendly_name));
            if (paired_list) {
                if (StrEq(g_asr_ble.active_dev.uuid, ble_dev->uuid)) {
                    is_connected = 1;
                }
                is_connected_js = json_object_new_boolean(is_connected);
                if (is_connected_js) {
                    json_object_object_add(ble_dev_json, Key_connected, is_connected_js);
                }
            }
            if (StrEq(ble_dev->act_role, PROFILE_SOURCE)) {
                json_object_object_add(ble_dev_json, Key_role, JSON_OBJECT_NEW_STRING(Val_sink));
                flags = 1;
            } else if (StrEq(ble_dev->act_role, PROFILE_SINK)) {
                json_object_object_add(ble_dev_json, Key_role, JSON_OBJECT_NEW_STRING(Val_source));
                flags = 1;
            }
            if (flags == 0) {
                json_object_object_add(ble_dev_json, Key_role, JSON_OBJECT_NEW_STRING(Val_source));
            }
            return ble_dev_json;
        }
    }

    ASR_PRINT(ASR_DEBUG_INFO, "asr_ble_get_a_device end\n");

    return NULL;
}

static int asr_ble_get_dev_list(int paired_list, struct lp_list_head *dev_list,
                                json_object *js_dev_list)
{
    int ret = -1;

    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    ble_device_t *ble_dev = NULL;
    json_object *ble_dev_json = NULL;

    ASR_PRINT(ASR_DEBUG_INFO, "asr_ble_get_dev_list start\n");

    if (!lp_list_empty(dev_list) && js_dev_list) {
        lp_list_for_each_safe(pos, npos, dev_list)
        {
            ble_dev = lp_list_entry(pos, ble_device_t, ble_item);
            if (ble_dev) {
                ble_dev_json = asr_ble_get_a_device(paired_list, ble_dev);
                if (ble_dev_json) {
                    ret = json_object_array_add(js_dev_list, ble_dev_json);
                }
            }
        }
    }

    ASR_PRINT(ASR_DEBUG_INFO, "asr_ble_get_dev_list end\n");

    return ret;
}

static int asr_ble_free_a_dev(ble_device_t *device, int flags)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;

    ASR_PRINT(ASR_DEBUG_INFO, "asr_ble_free_a_dev start\n");
    if (device) {
        if (device->act_role) {
            free(device->act_role);
            device->act_role = NULL;
        }
        if (device->uuid) {
            free(device->uuid);
            device->uuid = NULL;
        }
        if (device->friendly_name) {
            free(device->friendly_name);
            device->friendly_name = NULL;
        }
        if (device->mac_address) {
            free(device->mac_address);
            device->mac_address = NULL;
        }
        if (flags) {
            free(device);
            device = NULL;
        }
        ASR_PRINT(ASR_DEBUG_INFO, "asr_ble_free_a_dev end\n");
        return 0;
    }
    ASR_PRINT(ASR_DEBUG_ERR, "asr_ble_free_a_dev end\n");

    return -1;
}

static int asr_ble_del_a_dev(struct lp_list_head *dev_list, char *uuid)
{
    int ret = -1;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    ble_device_t *device_item = NULL;

    if (!dev_list) {
        ASR_PRINT(ASR_DEBUG_ERR, "json_cmd_parmas is NULL\n");
        return -1;
    }

    ASR_PRINT(ASR_DEBUG_INFO, "asr_ble_del_a_dev start\n");
    if (!lp_list_empty(dev_list)) {
        lp_list_for_each_safe(pos, npos, dev_list)
        {
            device_item = lp_list_entry(pos, ble_device_t, ble_item);
            if (device_item) {
                if (uuid) {
                    if (StrEq(device_item->uuid, uuid)) {
                        lp_list_del(&device_item->ble_item);
                        asr_ble_free_a_dev(device_item, 1);
                        ret = 0;
                        break;
                    }
                } else {
                    lp_list_del(&device_item->ble_item);
                    asr_ble_free_a_dev(device_item, 1);
                    ret = 0;
                }
            }
        }
    }

    ASR_PRINT(ASR_DEBUG_INFO, "asr_ble_del_a_dev end\n");

    return ret;
}

static int asr_ble_add_a_dev(int need_profile, struct lp_list_head *dev_list,
                             json_object *js_device)
{
    int ret = -1;
    int i = 0;
    int item_num = 0;
    int is_same = -1;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    char *uuid = NULL;
    char *friendly_name = NULL;
    char *mac_address = NULL;
    char *role_name = NULL;
    ble_device_t *device_item = NULL;
    json_object *supportedProfiles = NULL;

    if (!js_device || !dev_list) {
        ASR_PRINT(ASR_DEBUG_ERR, "json_cmd_parmas is NULL\n");
        return -1;
    }

    ASR_PRINT(ASR_DEBUG_INFO, "asr_ble_add_a_dev start\n");
    uuid = JSON_GET_STRING_FROM_OBJECT(js_device, Key_uniqueDeviceId);
    friendly_name = JSON_GET_STRING_FROM_OBJECT(js_device, Key_friendlyName);
    mac_address = JSON_GET_STRING_FROM_OBJECT(js_device, Key_truncatedMacAddress);
    role_name = JSON_GET_STRING_FROM_OBJECT(js_device, Key_profileName);

    if (NULL == uuid || strlen(uuid) <= 0) {
        ASR_PRINT(ASR_DEBUG_ERR, "uuid is invalid\n");
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
                    lp_list_del(&device_item->ble_item);
                    break;
                }
            }
        }
    }

    if (is_same != 0) {
        // add new information
        device_item = (ble_device_t *)calloc(1, sizeof(ble_device_t));
        if (device_item) {
            if (uuid) {
                device_item->uuid = strdup(uuid);
            }
            if (mac_address) {
                device_item->mac_address = strdup(mac_address);
            }
        }
    }
    if (device_item) {
        if (friendly_name) {
            if (device_item->friendly_name == NULL) {
                device_item->friendly_name = strdup(friendly_name);
            } else {
                if (strlen(friendly_name) > 0) {
                    free(device_item->friendly_name);
                    device_item->friendly_name = strdup(friendly_name);
                }
            }
        }
        if (need_profile == 1) {
            if (role_name) {
                if (device_item->act_role) {
                    free(device_item->act_role);
                }
                if (StrEq(role_name, Val_sink)) {
                    device_item->act_role = strdup(PROFILE_SOURCE);
                } else if (StrEq(role_name, Val_source)) {
                    device_item->act_role = strdup(PROFILE_SINK);
                }
            } else {
                json_object *supportedProfiles =
                    json_object_object_get(js_device, Key_supportedProfiles);
                if (supportedProfiles) {
                    item_num = json_object_array_length(supportedProfiles);
                    for (i = 0; i < item_num; i++) {
                        json_object *js_profile = json_object_array_get_idx(supportedProfiles, i);
                        if (js_profile) {
                            char *act_role = JSON_GET_STRING_FROM_OBJECT(js_profile, Key_name);
                            if (StrEq(act_role, PROFILE_SOURCE) || StrEq(act_role, PROFILE_SINK)) {
                                if (device_item->act_role) {
                                    free(device_item->act_role);
                                }
                                device_item->act_role = strdup(act_role);
                                break;
                            }
                        }
                    }
                }
            }
        }
        lp_list_add(&device_item->ble_item, dev_list);
        ret = 0;
    } else {
        ASR_PRINT(ASR_DEBUG_ERR, "device_item is NULL\n");
    }

    ASR_PRINT(ASR_DEBUG_INFO, "asr_ble_add_a_dev end\n");

    return ret;
}

static int asr_ble_update_active(json_object *js_device)
{
    int ret = -1;
    int i = 0;
    int item_num = 0;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    char *uuid = NULL;
    char *friendly_name = NULL;
    char *mac_address = NULL;
    ble_device_t *device_item = NULL;
    ble_device_t *new_device = NULL;
    json_object *supportedProfiles = NULL;

    if (!js_device) {
        ASR_PRINT(ASR_DEBUG_ERR, "json_cmd_parmas is NULL\n");
        return -1;
    }

    ASR_PRINT(ASR_DEBUG_INFO, "asr_ble_update_active start\n");
    uuid = JSON_GET_STRING_FROM_OBJECT(js_device, Key_uniqueDeviceId);
    friendly_name = JSON_GET_STRING_FROM_OBJECT(js_device, Key_friendlyName);
    mac_address = JSON_GET_STRING_FROM_OBJECT(js_device, Key_truncatedMacAddress);

    if (!lp_list_empty(&g_asr_ble.paired_list) && uuid) {
        lp_list_for_each_safe(pos, npos, &g_asr_ble.paired_list)
        {
            device_item = lp_list_entry(pos, ble_device_t, ble_item);
            if (device_item) {
                if (StrEq(device_item->uuid, uuid)) {
                    asr_ble_free_a_dev(&g_asr_ble.active_dev, 0);
                    if (device_item->uuid) {
                        g_asr_ble.active_dev.uuid = strdup(device_item->uuid);
                    }
                    if (device_item->friendly_name) {
                        g_asr_ble.active_dev.friendly_name = strdup(device_item->friendly_name);
                    }
                    if (device_item->mac_address) {
                        g_asr_ble.active_dev.mac_address = strdup(device_item->mac_address);
                    }
                    if (device_item->act_role) {
                        g_asr_ble.active_dev.act_role = strdup(device_item->act_role);
                    }
                    ret = 0;
                    break;
                }
            }
        }
    }
    ASR_PRINT(ASR_DEBUG_INFO, "asr_ble_update_active end\n");

    return ret;
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
void asr_ble_list_add_item(struct lp_list_head *dev_list, char *name, char *uuid, char *profile)
{
    int ret = 0;

    if (uuid && name) {
        json_object *json_cmd_parmas = json_object_new_object();
        if (json_cmd_parmas) {
            json_object_object_add(json_cmd_parmas, Key_friendlyName, JSON_OBJECT_NEW_STRING(name));
            json_object_object_add(json_cmd_parmas, Key_uniqueDeviceId,
                                   JSON_OBJECT_NEW_STRING(uuid));

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

            ret = asr_ble_add_a_dev(1, dev_list, json_cmd_parmas);
            json_object_put(json_cmd_parmas);
        }
    }
}

int asr_ble_parse_list(struct lp_list_head *dev_list, json_object *json_parmas)
{
    int ret = -1;
    int item_num = 0;
    int index = 0;
    json_object *js_list = NULL;

    if (json_parmas && dev_list) {
        js_list = json_object_object_get(json_parmas, "list");
        if (js_list) {
            item_num = json_object_array_length(js_list);
            for (index = item_num - 1; index >= 0; index--) {
                json_object *device_item = json_object_array_get_idx(js_list, index);
                if (device_item) {
                    char *name = JSON_GET_STRING_FROM_OBJECT(device_item, "name");
                    char *uuid = JSON_GET_STRING_FROM_OBJECT(device_item, "ad");
                    char *role = JSON_GET_STRING_FROM_OBJECT(device_item, "role");
                    if (uuid && strlen(uuid) > 0 && name && strlen(name) > 0 && role &&
                        strlen(role) > 0) {
                        if (StrEq(role, "Audio Source")) {
                            role = PROFILE_SOURCE;
                        } else if (StrEq(role, "Audio Sink")) {
                            role = PROFILE_SINK;
                        }
                        asr_ble_list_add_item(dev_list, name, uuid, role);
                    }
                }
            }
            ret = 0;
        }
    }

    return ret;
}

static int asr_ble_list_max_check(struct lp_list_head *dev_list, int max_cnt)
{
    int need_remove = 0;
    int sink_cnt = 0;
    int source_cnt = 0;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    ble_device_t *device_item = NULL;

    if (!lp_list_empty(dev_list)) {
        // remove the latest one
        lp_list_for_each_safe(pos, npos, dev_list)
        {
            device_item = lp_list_entry(pos, ble_device_t, ble_item);
            if (device_item) {
                if (StrEq(device_item->act_role, PROFILE_SOURCE)) {
                    sink_cnt++;
                    if (sink_cnt > max_cnt) {
                        lp_list_del(&device_item->ble_item);
                        asr_ble_free_a_dev(device_item, 1);
                        sink_cnt--;
                    }
                } else if (StrEq(device_item->act_role, PROFILE_SINK)) {
                    source_cnt++;
                    if (source_cnt > max_cnt) {
                        lp_list_del(&device_item->ble_item);
                        asr_ble_free_a_dev(device_item, 1);
                        source_cnt--;
                    }
                }
            }
        }
    }

    return 0;
}

void asr_bluetooth_get_tts(int tts_type)
{
    int need_tts = 1;

    pthread_mutex_lock(&g_asr_ble.state_lock);
    if (!g_asr_ble.request_txt) {
        need_tts = 0;
    }
    if (tts_type == e_tts_connect) {
        if (!StrSubStr(g_asr_ble.request_txt, GET_CONNECT_TTS)) {
            need_tts = 0;
        }
    } else if (tts_type == e_tts_disconnect) {
        if (!StrSubStr(g_asr_ble.request_txt, DISCONNECT_TXT)) {
            need_tts = 0;
        }
    } else if (tts_type == e_tts_pairing) {
        if (!StrSubStr(g_asr_ble.request_txt, START_PAIR_TXT)) {
            need_tts = 0;
        }
    }
    if (need_tts) {
        asr_player_buffer_stop(NULL, e_asr_stop);
        clova_speech_request_cmd(g_asr_ble.request_txt, CLOVA_PAYLOAD_VALUE_KO);
        free(g_asr_ble.request_txt);
        g_asr_ble.request_txt = NULL;
    }
    pthread_mutex_unlock(&g_asr_ble.state_lock);
}

void asr_bluetooth_clear_tts(void)
{
    pthread_mutex_lock(&g_asr_ble.state_lock);
    if (g_asr_ble.request_txt) {
        free(g_asr_ble.request_txt);
        g_asr_ble.request_txt = NULL;
    }
    pthread_mutex_unlock(&g_asr_ble.state_lock);
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
static int asr_bluetooth_state_update(int flags, json_object *json_cmd_parmas)
{
    int ret = -1;
    json_object *devices = NULL;

    if (!json_cmd_parmas) {
        ASR_PRINT(ASR_DEBUG_ERR, "json_cmd_parmas is NULL\n");
        return -1;
    }
    ASR_PRINT(ASR_DEBUG_INFO, "asr_bluetooth_state_update start\n");
    if (flags == e_add_scan_list) {
        int scan_status = JSON_GET_INT_FROM_OBJECT(json_cmd_parmas, "scan_status");
        devices = json_object_object_get(json_cmd_parmas, Key_devices);
        if (devices) {
            int item_num = json_object_array_length(devices);
            int i = 0;
            for (i = 0; i < item_num; i++) {
                json_object *device_item = json_object_array_get_idx(devices, i);
                if (device_item) {
                    pthread_mutex_lock(&g_asr_ble.state_lock);
                    ret = asr_ble_add_a_dev(0, &g_asr_ble.scan_list, device_item);
                    pthread_mutex_unlock(&g_asr_ble.state_lock);
                }
            }
        } else {
            pthread_mutex_lock(&g_asr_ble.state_lock);
            if (scan_status == 4) {
                g_asr_ble.scanning_status = e_off;
                g_asr_ble.start_connect = e_off;
            }
            ret = asr_ble_parse_list(&g_asr_ble.scan_list, json_cmd_parmas);
            pthread_mutex_unlock(&g_asr_ble.state_lock);
        }
        if (scan_status == 4) {
            ret = 1;
        }

    } else if (flags == e_update_paired_list) {
        devices = json_object_object_get(json_cmd_parmas, Key_devices);
        if (devices) {
            int item_num = json_object_array_length(devices);
            int i = 0;
            for (i = item_num; i >= 0; i--) {
                json_object *device_item = json_object_array_get_idx(devices, i);
                if (device_item) {
                    pthread_mutex_lock(&g_asr_ble.state_lock);
                    ret = asr_ble_add_a_dev(1, &g_asr_ble.paired_list, device_item);
                    pthread_mutex_unlock(&g_asr_ble.state_lock);
                }
            }
        } else {
            pthread_mutex_lock(&g_asr_ble.state_lock);
            ret = asr_ble_parse_list(&g_asr_ble.paired_list, json_cmd_parmas);
            pthread_mutex_unlock(&g_asr_ble.state_lock);
        }
    } else if (flags == e_delete_item_paired_list) {
        char *uuid = JSON_GET_STRING_FROM_OBJECT(json_cmd_parmas, Key_uniqueDeviceId);
        if (uuid) {
            pthread_mutex_lock(&g_asr_ble.state_lock);
            ret = asr_ble_del_a_dev(&g_asr_ble.paired_list, uuid);
            pthread_mutex_unlock(&g_asr_ble.state_lock);
        }
    } else if (flags == e_add_item_paired_list) {
        pthread_mutex_lock(&g_asr_ble.state_lock);
        ret = asr_ble_add_a_dev(1, &g_asr_ble.paired_list, json_cmd_parmas);
        asr_ble_list_max_check(&g_asr_ble.paired_list, MAX_PAIRED_LIST);
        pthread_mutex_unlock(&g_asr_ble.state_lock);
    } else if (flags == e_update_active_device) {
        pthread_mutex_lock(&g_asr_ble.state_lock);
        g_asr_ble.status = e_on;
        g_asr_ble.scanning_status = e_off;
        g_asr_ble.connecting_status = e_off;
        asr_ble_update_active(json_cmd_parmas);
        pthread_mutex_unlock(&g_asr_ble.state_lock);
    } else if (flags == e_free_active_device) {
        char *uuid = JSON_GET_STRING_FROM_OBJECT(json_cmd_parmas, Key_uniqueDeviceId);
        ASR_PRINT(ASR_DEBUG_INFO, "uuid = %s\n", uuid);
        if (StrEq(uuid, g_asr_ble.active_dev.uuid)) {
            pthread_mutex_lock(&g_asr_ble.state_lock);
            asr_ble_free_a_dev(&g_asr_ble.active_dev, 0);
            pthread_mutex_unlock(&g_asr_ble.state_lock);
        }
    }
    ASR_PRINT(ASR_DEBUG_INFO, "asr_bluetooth_state_update end\n");

    return ret;
}

// To free the string after used
static char *clova_name_btname_rule(char *src_name)
{
    int dst_len = 0;
    char *dst_name = NULL;
    char *tmp_src = src_name;
    char *tmp_dst = NULL;
    int is_num = 0;
    int num_cnt = 0;
    int need_insert = 0;

    if (!src_name || (src_name && strlen(src_name) <= 0)) {
        ASR_PRINT(ASR_DEBUG_ERR, "clova_name_btname_rule name is null\n");
        goto Exit;
    }

    dst_len = 2 * strlen(src_name);
    dst_name = calloc(1, dst_len);
    if (!dst_name) {
        ASR_PRINT(ASR_DEBUG_ERR, "dst_name calloc failed\n");
        goto Exit;
    }

    tmp_dst = dst_name;
    while (*tmp_src) {
        need_insert = 0;
        if (isdigit(*tmp_src)) {
            is_num = 1;
            num_cnt++;
        } else {
            if (is_num == 1) {
                need_insert = 1;
            }
            num_cnt = 0;
            is_num = 0;
        }

        if (is_num == 1 && num_cnt >= 2) {
            need_insert = 1;
        }

        if (need_insert) {
            *tmp_dst = ',';
            tmp_dst++;
        }
        *tmp_dst = *tmp_src;
        tmp_dst++;
        tmp_src++;
    }
    dst_name[dst_len - 1] = '\0';
    ASR_PRINT(ASR_DEBUG_INFO, "src_name=%s dst_name=%s\n", src_name, dst_name);

Exit:

    return dst_name;
}

static int asr_bluetooth_check_need_tts(int success, char *event, char *address, char *name,
                                        char *role_name)
{
    int ret = success;
    int role = e_role_none;
    int first_flag = 0;
    char *dst_name = NULL;

    if (get_net_work_reset_flag() || get_internet_flags() == 0) {
        ASR_PRINT(ASR_DEBUG_ERR,
                  "net work reset mode or the internet is error, ignore to get the tts\n");
        pthread_mutex_lock(&g_asr_ble.state_lock);
        g_asr_ble.need_disconnect = 0;
        if (g_asr_ble.request_txt) {
            free(g_asr_ble.request_txt);
            g_asr_ble.request_txt = NULL;
        }
        pthread_mutex_unlock(&g_asr_ble.state_lock);
        goto Exit;
    }

    if (StrEq(role_name, Val_source)) {
        role = e_role_source;
    } else if (StrEq(role_name, Val_sink)) {
        role = e_role_sink;
    }

    pthread_mutex_lock(&g_asr_ble.state_lock);
    g_asr_ble.need_disconnect = 0;
    if (StrEq(event, Val_disconnect)) {
        if (g_asr_ble.request_txt) {
            free(g_asr_ble.request_txt);
            g_asr_ble.request_txt = NULL;
        }
        if (g_asr_ble.start_connect == e_on) {
            ret = 0;
        } else if (StrEq(g_asr_ble.active_dev.uuid, address)) {
            ret = 1;
        } else {
            ret = 0;
        }
        if (ret) {
            dst_name = clova_name_btname_rule(name);
            if (dst_name) {
                asprintf(&g_asr_ble.request_txt, GET_DISCONNECT_TTS, dst_name);
            }
        }
    } else if (StrEq(event, Val_connectbyid)) {
        if (g_asr_ble.request_txt) {
            free(g_asr_ble.request_txt);
            g_asr_ble.request_txt = NULL;
        }
        if (lp_list_empty(&g_asr_ble.paired_list)) {
            first_flag = 1;
        }
        if (ret) {
            if (first_flag) {
                if (role == e_role_source) {
                    dst_name = clova_name_btname_rule(name);
                    if (dst_name) {
                        asprintf(&g_asr_ble.request_txt, "%s %s %s", dst_name, GET_CONNECT_TTS,
                                 SOURCE_GUIDE_TXT);
                    }
                } else if (role == e_role_sink) {
                    dst_name = clova_name_btname_rule(name);
                    if (dst_name) {
                        asprintf(&g_asr_ble.request_txt, "%s %s %s", dst_name, GET_CONNECT_TTS,
                                 SINK_GUIDE_TXT);
                    }
                }
            } else {
                dst_name = clova_name_btname_rule(name);
                if (dst_name) {
                    asprintf(&g_asr_ble.request_txt, "%s %s", dst_name, GET_CONNECT_TTS);
                }
            }
        } else {
            dst_name = clova_name_btname_rule(name);
            if (dst_name) {
                asprintf(&g_asr_ble.request_txt, CONNECT_FAILED, dst_name);
            }
        }
        g_asr_ble.start_connect = e_off;
    } else if (StrEq(event, Val_enable_discoverable)) {
        if (ret) {
            if (g_asr_ble.request_txt) {
                free(g_asr_ble.request_txt);
                g_asr_ble.request_txt = NULL;
            }
            char *src_name = get_bletooth_name();
            dst_name = clova_name_btname_rule(src_name);
            if (dst_name) {
                asprintf(&g_asr_ble.request_txt, GET_START_PAIR_TTS, dst_name);
            }
        }
    } else if (StrEq(event, Val_delete)) {
        printf("active uuid = %s address = %s\n", g_asr_ble.active_dev.uuid, address);
        if (g_asr_ble.request_txt) {
            free(g_asr_ble.request_txt);
            g_asr_ble.request_txt = NULL;
        }
        if (StrEq(g_asr_ble.active_dev.uuid, address) &&
            StrEq(g_asr_ble.active_dev.act_role, PROFILE_SOURCE)) {
            dst_name = clova_name_btname_rule(g_asr_ble.active_dev.friendly_name);
            if (dst_name) {
                asprintf(&g_asr_ble.request_txt, GET_DISCONNECT_TTS, dst_name);
            }
        }
    }
    pthread_mutex_unlock(&g_asr_ble.state_lock);

Exit:
    if (dst_name) {
        free(dst_name);
    }
    return 0;
}

static int asr_bluetooth_update_metadata(json_object *json_cmd_parmas)
{
    int ret = -1;
    char *title = NULL;
    char *artist = NULL;
    char *album = NULL;

    if (json_cmd_parmas) {
        title = JSON_GET_STRING_FROM_OBJECT(json_cmd_parmas, Key_title);
        artist = JSON_GET_STRING_FROM_OBJECT(json_cmd_parmas, Key_artist);
        album = JSON_GET_STRING_FROM_OBJECT(json_cmd_parmas, Key_album);

        pthread_mutex_lock(&g_asr_ble.state_lock);
        if (g_asr_ble.title) {
            free(g_asr_ble.title);
            g_asr_ble.title = NULL;
        }
        if (title) {
            g_asr_ble.title = strdup(title);
        }
        if (g_asr_ble.artist) {
            free(g_asr_ble.artist);
            g_asr_ble.artist = NULL;
        }
        if (artist) {
            g_asr_ble.artist = strdup(artist);
        }
        if (g_asr_ble.album) {
            free(g_asr_ble.album);
            g_asr_ble.album = NULL;
        }
        if (album) {
            g_asr_ble.album = strdup(album);
        }
        pthread_mutex_unlock(&g_asr_ble.state_lock);

        ret = 0;
    }

    return ret;
}

/*
 * parse the cmd from bluetooth thread to asr client
 *
 */
// TODO: need free after used
char *asr_bluetooth_parse_cmd(char *json_cmd_str)
{
    json_bool ret = 0;
    json_object *json_cmd = NULL;
    json_object *json_cmd_parmas = NULL;
    char *event_type = NULL;
    char *event = NULL;
    char *cmd_data = NULL;
    char *event_name = NULL;

    if (!json_cmd_str) {
        ASR_PRINT(ASR_DEBUG_ERR, "input is NULL\n");
        return NULL;
    }

    json_cmd = json_tokener_parse(json_cmd_str);
    if (!json_cmd) {
        ASR_PRINT(ASR_DEBUG_ERR, "input is not a json\n");
        return NULL;
    }

    event_type = JSON_GET_STRING_FROM_OBJECT(json_cmd, Key_Event_type);
    if (!event_type) {
        ASR_PRINT(ASR_DEBUG_ERR, "cmd none name found\n");
        goto Exit;
    }

    ASR_PRINT(ASR_DEBUG_INFO, "cmd json is \n%s\n", json_cmd_str);

    if (StrEq(event_type, Val_bluetooth)) {
        event = JSON_GET_STRING_FROM_OBJECT(json_cmd, Key_Event);
        if (event) {
            json_cmd_parmas = json_object_object_get(json_cmd, Key_Event_params);
            if (!json_cmd_parmas) {
                ASR_PRINT(ASR_DEBUG_ERR, "json_cmd_parmas is not found\n");
            }
            ret = JSON_GET_BOOL_FROM_OBJECT(json_cmd_parmas, Key_ret);
            char *address = JSON_GET_STRING_FROM_OBJECT(json_cmd_parmas, Key_uniqueDeviceId);
            char *name = JSON_GET_STRING_FROM_OBJECT(json_cmd_parmas, Key_friendlyName);
            char *role_name = JSON_GET_STRING_FROM_OBJECT(json_cmd_parmas, Key_profileName);

            asr_bluetooth_check_need_tts(ret, event, address, name, role_name);
            if (StrEq(event, Val_scan)) {
                asr_bluetooth_state_update(e_add_scan_list, json_cmd_parmas);
                ret = 1;
            } else if (StrEq(event, Val_disconnect)) {
                if (ret) {
                    asr_bluetooth_state_update(e_free_active_device, json_cmd_parmas);
                }
            } else if (StrEq(event, Val_connectbyid)) {
                if (ret) {
                    asr_bluetooth_state_update(e_add_item_paired_list, json_cmd_parmas);
                    asr_bluetooth_state_update(e_update_active_device, json_cmd_parmas);
                } else {
                    pthread_mutex_lock(&g_asr_ble.state_lock);
                    asr_ble_free_a_dev(&g_asr_ble.active_dev, 0);
                    pthread_mutex_unlock(&g_asr_ble.state_lock);
                }
            } else if (StrEq(event, Val_getpaired_list)) {
                asr_bluetooth_state_update(e_update_paired_list, json_cmd_parmas);
            } else if (StrEq(event, Val_enable_discoverable)) {
                if (ret) {
                    pthread_mutex_lock(&g_asr_ble.state_lock);
                    g_asr_ble.pairing_status = e_on;
                    g_asr_ble.scanning_status = e_off;
                    g_asr_ble.connecting_status = e_off;
                    g_asr_ble.start_connect = e_off;
                    pthread_mutex_unlock(&g_asr_ble.state_lock);
                } else {
                    pthread_mutex_lock(&g_asr_ble.state_lock);
                    g_asr_ble.pairing_status = e_off;
                    g_asr_ble.start_connect = e_off;
                    pthread_mutex_unlock(&g_asr_ble.state_lock);
                }
            } else if (StrEq(event, Val_disable_discoverable)) {
                pthread_mutex_lock(&g_asr_ble.state_lock);
                g_asr_ble.pairing_status = e_off;
                g_asr_ble.start_connect = e_off;
                pthread_mutex_unlock(&g_asr_ble.state_lock);
            } else if (StrEq(event, Val_connectbyprofile)) {
                pthread_mutex_lock(&g_asr_ble.state_lock);
                g_asr_ble.connecting_status = e_off;
                g_asr_ble.start_connect = e_off;
                pthread_mutex_unlock(&g_asr_ble.state_lock);
                if (ret) {
                    asr_bluetooth_state_update(e_add_item_paired_list, json_cmd_parmas);
                    asr_bluetooth_state_update(e_update_active_device, json_cmd_parmas);
                }
            } else if (StrEq(event, Val_delete)) {
                if (ret) {
                    asr_bluetooth_state_update(e_delete_item_paired_list, json_cmd_parmas);
                    pthread_mutex_lock(&g_asr_ble.state_lock);
                    if (StrEq(g_asr_ble.active_dev.uuid, address) &&
                        StrEq(g_asr_ble.active_dev.act_role, PROFILE_SOURCE)) {
                        asr_ble_free_a_dev(&g_asr_ble.active_dev, 0);
                    }
                    pthread_mutex_unlock(&g_asr_ble.state_lock);
                }
            } else if (StrEq(event, Val_status)) {
                pthread_mutex_lock(&g_asr_ble.state_lock);
                if (g_asr_ble.active_dev.uuid) {
                    if (ret) {
                        g_asr_ble.play_status = e_on;
                    } else {
                        g_asr_ble.play_status = e_off;
                    }
                    ret = 1;
                }
                pthread_mutex_unlock(&g_asr_ble.state_lock);
            } else if (StrEq(event, Val_metadata)) {
                ret = asr_bluetooth_update_metadata(json_cmd_parmas);
            }
        }
    }

    if (ret) {
        cmd_data = create_device_control_report_cmd(CLOVA_HEADER_NAME_REPORT_STATE);
    } else {
        if (StrEq(event, Val_connectbyid)) {
            asr_bluetooth_get_tts(0);
        }
    }
Exit:
    if (json_cmd) {
        json_object_put(json_cmd);
    }

    return cmd_data;
}

static int asr_bluetooth_get_actions(json_object *bluetooth)
{
    json_object *actions = NULL;
    int ret = -1;

    if (bluetooth == NULL) {
        goto EXIT;
    }

    actions = json_object_new_array();
    if (actions == NULL) {
        goto EXIT;
    }

    ret = json_object_array_add(actions, JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_NAME_BT_CONNECT));
    if (ret) {
        goto EXIT;
    }

    ret = json_object_array_add(actions, JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_NAME_BT_DISCONNECT));
    if (ret) {
        goto EXIT;
    }

    ret =
        json_object_array_add(actions, JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_NAME_BT_START_PAIRING));
    if (ret) {
        goto EXIT;
    }

    ret =
        json_object_array_add(actions, JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_NAME_BT_STOP_PAIRING));
    if (ret) {
        goto EXIT;
    }

    ret = json_object_array_add(actions, JSON_OBJECT_NEW_STRING(CLOVA_HEADER_NAME_BT_RESCAN));
    if (ret) {
        goto EXIT;
    }

    ret = json_object_array_add(actions, JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_NAME_BT_DELETE));
    if (ret) {
        goto EXIT;
    }

    ret = json_object_array_add(actions, JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_NAME_BT_PLAY));
    if (ret) {
        goto EXIT;
    }

#if 0 // Donut didnot support BT turn on/off
    ret = json_object_array_add(actions, JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_NAME_TURN_OFF));
    if(ret) {
        goto EXIT;
    }

    ret = json_object_array_add(actions, JSON_OBJECT_NEW_STRING(CLOVA_PAYLOAD_NAME_TURN_ON));
    if(ret) {
        goto EXIT;
    }
#endif

    ret = 0;
    json_object_object_add(bluetooth, CLOVA_PAYLOAD_NAME_ACTIONS, actions);

EXIT:

    if (ret) {
        if (actions != NULL) {
            json_object_put(actions);
        }
    }

    return ret;
}

static int asr_bluetooth_get_btlist(json_object *bluetooth)
{
    json_object *btlist = NULL;
    int ret = -1;

    if (bluetooth == NULL) {
        goto EXIT;
    }

    btlist = json_object_new_array();
    if (btlist == NULL) {
        goto EXIT;
    }

    /* get btlist */
    pthread_mutex_lock(&g_asr_ble.state_lock);
    asr_ble_list_max_check(&g_asr_ble.paired_list, MAX_PAIRED_LIST);
    asr_ble_get_dev_list(1, &g_asr_ble.paired_list, btlist);
    pthread_mutex_unlock(&g_asr_ble.state_lock);

    ret = 0;
    json_object_object_add(bluetooth, CLOVA_PAYLOAD_NAME_BT_LIST, btlist);

EXIT:

    if (ret) {
        if (btlist != NULL) {
            json_object_put(btlist);
        }
    }

    return ret;
}

static int asr_bluetooth_get_scanlist(json_object *bluetooth)
{
    json_object *scanlist = NULL;
    int ret = -1;

    if (bluetooth == NULL) {
        goto EXIT;
    }

    scanlist = json_object_new_array();
    if (scanlist == NULL) {
        goto EXIT;
    }

    /* get scanlist */
    pthread_mutex_lock(&g_asr_ble.state_lock);
    asr_ble_get_dev_list(0, &g_asr_ble.scan_list, scanlist);
    pthread_mutex_unlock(&g_asr_ble.state_lock);

    ret = 0;
    json_object_object_add(bluetooth, CLOVA_PAYLOAD_NAME_SCAN_LIST, scanlist);

EXIT:

    if (ret) {
        if (scanlist != NULL) {
            json_object_put(scanlist);
        }
    }

    return ret;
}

static int asr_bluetooth_get_status(json_object *bluetooth)
{
    char *state = NULL;
    char *pairing_state = NULL;
    char *scanning_state = NULL;
    char *connecting_state = NULL;
    char *player_state = CLOVA_PAYLOAD_NAME_STATE_PAUSED;
    json_object *playerinfo = NULL;

    int ret = -1;

    if (bluetooth == NULL) {
        goto EXIT;
    }

    playerinfo = json_object_new_object();
    if (playerinfo == NULL) {
        goto EXIT;
    }

    /* get state */
    pthread_mutex_lock(&g_asr_ble.state_lock);
    state = g_asr_ble.status ? CLOVA_PAYLOAD_NAME_ON : CLOVA_PAYLOAD_NAME_OFF;
    pairing_state = g_asr_ble.pairing_status ? CLOVA_PAYLOAD_NAME_ON : CLOVA_PAYLOAD_NAME_OFF;
    scanning_state = g_asr_ble.scanning_status ? CLOVA_PAYLOAD_NAME_ON : CLOVA_PAYLOAD_NAME_OFF;
    connecting_state = g_asr_ble.connecting_status ? CLOVA_PAYLOAD_NAME_ON : CLOVA_PAYLOAD_NAME_OFF;

    json_object_object_add(bluetooth, CLOVA_PAYLOAD_NAME_STATE, JSON_OBJECT_NEW_STRING(state));
    json_object_object_add(bluetooth, CLOVA_PAYLOAD_NAME_PAIRING,
                           JSON_OBJECT_NEW_STRING(pairing_state));
    json_object_object_add(bluetooth, CLOVA_PAYLOAD_NAME_SCANNING,
                           JSON_OBJECT_NEW_STRING(scanning_state));
    json_object_object_add(bluetooth, CLOVA_PAYLOAD_NAME_CONNECTING,
                           JSON_OBJECT_NEW_STRING(connecting_state));

    if (g_asr_ble.active_dev.uuid && StrEq(g_asr_ble.active_dev.act_role, PROFILE_SOURCE)) {
        if (!g_asr_ble.artist || (g_asr_ble.artist && strlen(g_asr_ble.artist) == 0)) {
            json_object_object_add(playerinfo, CLOVA_PAYLOAD_NAME_ARTISTNAME,
                                   JSON_OBJECT_NEW_STRING(Val_unknown_text));
        } else {
            json_object_object_add(playerinfo, CLOVA_PAYLOAD_NAME_ARTISTNAME,
                                   JSON_OBJECT_NEW_STRING(g_asr_ble.artist));
        }
        if (!g_asr_ble.title || (g_asr_ble.title && strlen(g_asr_ble.title) == 0)) {
            json_object_object_add(playerinfo, CLOVA_PAYLOAD_NAME_TRACKTITLE,
                                   JSON_OBJECT_NEW_STRING(Val_unknown_text));
        } else {
            json_object_object_add(playerinfo, CLOVA_PAYLOAD_NAME_TRACKTITLE,
                                   JSON_OBJECT_NEW_STRING(g_asr_ble.title));
        }
        if (!g_asr_ble.album || (g_asr_ble.album && strlen(g_asr_ble.album) == 0)) {
            json_object_object_add(playerinfo, CLOVA_PAYLOAD_NAME_ALBUMTITLE,
                                   JSON_OBJECT_NEW_STRING(Val_unknown_text));
        } else {
            json_object_object_add(playerinfo, CLOVA_PAYLOAD_NAME_ALBUMTITLE,
                                   JSON_OBJECT_NEW_STRING(g_asr_ble.album));
        }
        if (check_is_bt_play_mode()) {
            player_state = g_asr_ble.play_status ? CLOVA_PAYLOAD_NAME_STATE_PLAYING
                                                 : CLOVA_PAYLOAD_NAME_STATE_PAUSED;
        } else {
            player_state = CLOVA_PAYLOAD_NAME_STATE_STOPPED;
        }
        json_object_object_add(playerinfo, CLOVA_PAYLOAD_NAME_STATE,
                               JSON_OBJECT_NEW_STRING(player_state));
        json_object_object_add(bluetooth, CLOVA_PAYLOAD_NAME_PLAYERINFO, playerinfo);
    } else if (playerinfo) {
        json_object_put(playerinfo);
    }
    pthread_mutex_unlock(&g_asr_ble.state_lock);

    ret = 0;

EXIT:

    return ret;
}

int asr_bluetooth_state(json_object *json_payload)
{
    json_object *bluetooth = NULL;
    int ret = -1;

    if (json_payload == NULL) {
        goto EXIT;
    }

    bluetooth = json_object_new_object();
    if (bluetooth == NULL) {
        goto EXIT;
    }

    /* actions */
    ret = asr_bluetooth_get_actions(bluetooth);
    if (ret) {
        goto EXIT;
    }

    /* btlist */
    ret = asr_bluetooth_get_btlist(bluetooth);
    if (ret) {
        goto EXIT;
    }

    /* scanlist */
    ret = asr_bluetooth_get_scanlist(bluetooth);
    if (ret) {
        goto EXIT;
    }

    /* state */
    ret = asr_bluetooth_get_status(bluetooth);
    if (ret) {
        goto EXIT;
    }

    ret = 0;
    json_object_object_add(json_payload, CLOVA_PAYLOAD_NAME_BLUETOOTH, bluetooth);

EXIT:

    if (ret) {
        if (bluetooth) {
            json_object_put(bluetooth);
        }
    }

    return ret;
}

int asr_bluetooth_init(void)
{
    int ret = 0;

    memset(&g_asr_ble, 0x0, sizeof(asr_ble_t));

    pthread_mutex_init(&g_asr_ble.state_lock, NULL);
    pthread_mutex_lock(&g_asr_ble.state_lock);
    LP_INIT_LIST_HEAD(&g_asr_ble.paired_list);
    LP_INIT_LIST_HEAD(&g_asr_ble.scan_list);
    g_asr_ble.status = e_on;
    pthread_mutex_unlock(&g_asr_ble.state_lock);

    return ret;
}

int asr_bluetooth_uninit(void)
{
    // TODO: free the malloc data, free paired list scan list
    pthread_mutex_lock(&g_asr_ble.state_lock);
    if (g_asr_ble.title) {
        free(g_asr_ble.title);
        g_asr_ble.title = NULL;
    }
    if (g_asr_ble.artist) {
        free(g_asr_ble.artist);
        g_asr_ble.artist = NULL;
    }
    if (g_asr_ble.album) {
        free(g_asr_ble.album);
        g_asr_ble.album = NULL;
    }
    if (g_asr_ble.request_txt) {
        free(g_asr_ble.request_txt);
        g_asr_ble.request_txt = NULL;
    }
    pthread_mutex_unlock(&g_asr_ble.state_lock);

    pthread_mutex_destroy(&g_asr_ble.state_lock);

    return 0;
}

// parse directive
int asr_bluetooth_parse_directive(json_object *js_data)
{
    int ret = -1;

    if (js_data) {
        json_object *json_header = json_object_object_get(js_data, KEY_HEADER);
        if (json_header) {
            char *name = JSON_GET_STRING_FROM_OBJECT(json_header, KEY_NAME);
            if (name) {
                if (StrEq(name, CLOVA_HEADER_NAME_BT_START_PAIRING)) {
                    ret = asr_ble_startpairing(js_data);
                    if (0 != ret) {
                        clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_FAILED,
                                                        CLOVA_PAYLOAD_NAME_BLUETOOTH,
                                                        CLOVA_HEADER_NAME_BT_START_PAIRING);
                    } else {
                        clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_EXECUTED,
                                                        CLOVA_PAYLOAD_NAME_BLUETOOTH,
                                                        CLOVA_HEADER_NAME_BT_START_PAIRING);
                    }

                } else if (StrEq(name, CLOVA_HEADER_NAME_BT_STOP_PAIRING)) {
                    ret = asr_ble_stoppairing(js_data);
                    if (0 == ret) {
                        clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_EXECUTED,
                                                        CLOVA_PAYLOAD_NAME_BLUETOOTH,
                                                        CLOVA_HEADER_NAME_BT_STOP_PAIRING);

                    } else {
                        clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_FAILED,
                                                        CLOVA_PAYLOAD_NAME_BLUETOOTH,
                                                        CLOVA_HEADER_NAME_BT_STOP_PAIRING);
                    }

                } else if (StrEq(name, CLOVA_HEADER_NAME_BT_CONNECT)) {
                    ret = asr_ble_connect_id(js_data);
                    if (0 != ret) {
                        clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_FAILED,
                                                        CLOVA_PAYLOAD_NAME_BLUETOOTH,
                                                        CLOVA_HEADER_NAME_BT_CONNECT);
                    } else {
                        clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_EXECUTED,
                                                        CLOVA_PAYLOAD_NAME_BLUETOOTH,
                                                        CLOVA_HEADER_NAME_BT_CONNECT);
                    }

                } else if (StrEq(name, CLOVA_HEADER_NAME_BT_CONNECT_BY_PIN_CODE)) {
                    ret = asr_ble_connect_pin_code(js_data);
                    if (0 != ret) {
                        clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_FAILED,
                                                        CLOVA_PAYLOAD_NAME_BLUETOOTH,
                                                        CLOVA_HEADER_NAME_BT_CONNECT_BY_PIN_CODE);
                    } else {
                        clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_EXECUTED,
                                                        CLOVA_PAYLOAD_NAME_BLUETOOTH,
                                                        CLOVA_HEADER_NAME_BT_CONNECT_BY_PIN_CODE);
                    }

                } else if (StrEq(name, CLOVA_HEADER_NAME_BT_DISCONNECT)) {
                    ret = asr_ble_disconnect(js_data);
                    if (0 != ret) {
                        clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_FAILED,
                                                        CLOVA_PAYLOAD_NAME_BLUETOOTH,
                                                        CLOVA_HEADER_NAME_BT_DISCONNECT);
                    } else {
                        clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_EXECUTED,
                                                        CLOVA_PAYLOAD_NAME_BLUETOOTH,
                                                        CLOVA_HEADER_NAME_BT_DISCONNECT);
                    }

                } else if (StrEq(name, CLOVA_HEADER_NAME_BT_RESCAN)) {
                    ret = asr_ble_scan(js_data);
                    if (0 != ret) {
                        clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_FAILED,
                                                        CLOVA_PAYLOAD_NAME_BLUETOOTH,
                                                        CLOVA_HEADER_NAME_BT_RESCAN);
                    } else {
                        clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_EXECUTED,
                                                        CLOVA_PAYLOAD_NAME_BLUETOOTH,
                                                        CLOVA_HEADER_NAME_BT_RESCAN);
                    }

                } else if (StrEq(name, CLOVA_HEADER_NAME_BT_DELETE)) {
                    ret = asr_ble_delete(js_data);
                    if (0 != ret) {
                        clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_FAILED,
                                                        CLOVA_PAYLOAD_NAME_BLUETOOTH,
                                                        CLOVA_HEADER_NAME_BT_DELETE);
                    } else {
                        clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_EXECUTED,
                                                        CLOVA_PAYLOAD_NAME_BLUETOOTH,
                                                        CLOVA_HEADER_NAME_BT_DELETE);
                    }
                    clova_set_bt_sound_path(0);

                } else if (StrEq(name, CLOVA_HEADER_NAME_BT_PLAY)) {
                    ret = asr_ble_play(js_data);
                    if (0 != ret) {
                        clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_FAILED,
                                                        CLOVA_PAYLOAD_NAME_BLUETOOTH,
                                                        CLOVA_HEADER_NAME_BT_PLAY);
                    } else {
                        clova_device_control_action_cmd(CLOVA_HEADER_NAME_ACTION_EXECUTED,
                                                        CLOVA_PAYLOAD_NAME_BLUETOOTH,
                                                        CLOVA_HEADER_NAME_BT_PLAY);
                    }

                } else {
                    ret = -1;
                }
            }
        }
    }

    if (ret != 0) {
        ASR_PRINT(ASR_DEBUG_ERR, "prase message error\n");
    }

    return ret;
}

int asr_bluetooth_switch(int on_ff_status)
{
    pthread_mutex_lock(&g_asr_ble.state_lock);
    g_asr_ble.status = on_ff_status;
    pthread_mutex_unlock(&g_asr_ble.state_lock);

    return 0;
}

int asr_bluetooth_sound_path_change(int path_flags)
{
    int ret = -1;
    char *params = NULL;
    char *cmd = NULL;

    pthread_mutex_lock(&g_asr_ble.state_lock);
    Create_Bluetooth_Sound_Path_Params(params, g_asr_ble.active_dev.uuid, path_flags);
    pthread_mutex_unlock(&g_asr_ble.state_lock);
    if (params) {
        Create_Bluetooth_Cmd(cmd, Val_sound_path, params);
        free(params);
    }
    if (cmd) {
        Bluetooth_Cmd_Send(cmd, strlen(cmd), 0);
        free(cmd);
        ret = 0;
    }

    return ret;
}

void asr_bluetooth_disconnect(int prepare)
{
    pthread_mutex_lock(&g_asr_ble.state_lock);
    if (g_asr_ble.active_dev.uuid && StrEq(g_asr_ble.active_dev.act_role, PROFILE_SOURCE)) {
        asr_ble_free_a_dev(&g_asr_ble.active_dev, 0);
        if (prepare == 1) {
            g_asr_ble.need_disconnect = 1;
        } else {
            g_asr_ble.need_disconnect = 0;
            Bluetooth_Cmd_Send(Val_diconnect_bt, strlen(Val_diconnect_bt), 0);
        }
    } else {
        if (g_asr_ble.need_disconnect == 1) {
            Bluetooth_Cmd_Send(Val_diconnect_bt, strlen(Val_diconnect_bt), 0);
        }
        g_asr_ble.need_disconnect = 0;
    }
    pthread_mutex_unlock(&g_asr_ble.state_lock);
}

void asr_bluetooth_clear_active(void)
{
    int need_report = 0;

    pthread_mutex_lock(&g_asr_ble.state_lock);
    if (g_asr_ble.active_dev.uuid) {
        asr_ble_free_a_dev(&g_asr_ble.active_dev, 0);
        need_report = 1;
    }
    pthread_mutex_unlock(&g_asr_ble.state_lock);

    if (need_report) {
        clova_device_control_report_cmd(CLOVA_HEADER_NAME_REPORT_STATE);
    }
}

void asr_bluetooth_reset(void)
{
    pthread_mutex_lock(&g_asr_ble.state_lock);
    asr_ble_del_a_dev(&g_asr_ble.scan_list, NULL);
    asr_ble_del_a_dev(&g_asr_ble.paired_list, NULL);
    asr_ble_free_a_dev(&g_asr_ble.active_dev, 0);
    Bluetooth_Cmd_Send(Val_cleanbthistory, strlen(Val_cleanbthistory), 0);
    pthread_mutex_unlock(&g_asr_ble.state_lock);
}
