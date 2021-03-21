
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "wm_util.h"
#include "wm_context.h"
#include "volume_type.h"
#include "focus_manage.h"

#include "json.h"
#include "asr_cmd.h"
#include "asr_cfg.h"
#include "asr_json_common.h"
#include "asr_bluetooth.h"
#include "asr_player.h"

#include "clova_clova.h"
#include "clova_context.h"
#include "clova_dnd.h"
#include "clova_settings.h"
#include "clova_device.h"
#include "clova_speech_synthesizer.h"

static pthread_mutex_t g_settings_lock = PTHREAD_MUTEX_INITIALIZER;

#define settings_lock()                                                                            \
    do {                                                                                           \
        printf("[%s:%d] settings lock\n", __FUNCTION__, __LINE__);                                 \
        pthread_mutex_lock(&g_settings_lock);                                                      \
    } while (0)

#define settings_unlock()                                                                          \
    do {                                                                                           \
        printf("[%s:%d] settings unlock\n", __FUNCTION__, __LINE__);                               \
        pthread_mutex_unlock(&g_settings_lock);                                                    \
    } while (0)

settings_cfg g_keyword[KeywordNum] = {{KeywordList, "KeywordList", NULL, NULL, 0},
                                      {KeywordCurrent, "KeywordCurrent", NULL, "KeywordChange", 0},
                                      {KeywordAttendingSoundEnable, "KeywordAttendingSoundEnable",
                                       NULL, "KeywordAttendingSoundChange", 0}};

settings_cfg g_voiceType[VoiceTypeNum] = {
    {VoiceTypeList, "VoiceTypeList", NULL, NULL, 0},
    {VoiceTypeCurrent, "VoiceTypeCurrent", NULL, "VoiceTypeChange", 0}};

settings_cfg g_DND[DNDNum] = {{DNDEnable, "DNDEnable", NULL, "DNDEnableChange", 0},
                              {DNDStartTime, "DNDStartTime", NULL, "DNDStartTimeChange", 0},
                              {DNDEndTime, "DNDEndTime", NULL, "DNDEndTimeChange", 0},
                              {DNDNowEndTime, "DNDNowEndTime", NULL, "DNDNowEndTimeChange", 0}};

settings_cfg g_deviceInfo[DeviceInfoNum] = {
    {DeviceInfoName, "DeviceInfoName", NULL, NULL, 0},
    {DeviceInfoDefinedName, "DeviceInfoDefinedName", NULL, "DeviceInfoDefinedNameChange", 0},
    {DeviceInfoWifiMac, "DeviceInfoWifiMac", NULL, NULL, 0},
    {DeviceInfoSerial, "DeviceInfoSerial", NULL, NULL, 0},
    {DeviceInfoFirmware, "DeviceInfoFirmware", NULL, NULL, 0}};

settings_cfg g_alarm[AlarmNum] = {
    {AlarmSoundList, "AlarmSoundList", NULL, NULL, 0},
    {AlarmSoundCurrent, "AlarmSoundCurrent", NULL, "AlarmSoundChange", 0},
    {AlarmVolumeMax, "AlarmVolumeMax", NULL, NULL, 0},
    {AlarmVolumeMin, "AlarmVolumeMin", NULL, NULL, 0},
    {AlarmVolumeCurrent, "AlarmVolumeCurrent", NULL, "AlarmVolumeCurrentChange", 0},
    {AlarmSoundChangeCompleted, "AlarmSoundChangeCompleted", NULL, NULL, 0}};

settings_cfg g_locationInfo[LocationInfoNum] = {
    {LocationInfoLatitude, "LocationInfoLatitude", NULL, "LocationInfoLatitudeChange", 0},
    {LocationInfoLongitude, "LocationInfoLongitude", NULL, "LocationInfoLongitudeChange", 0},
    {LocationInfoRefreshedAt, "LocationInfoRefreshedAt", NULL, "LocationInfoRefreshedAtChange", 0},
    {LocationInfoAddress, "LocationInfoAddress", NULL, "LocationInfoAddressChange", 0}};

settings_cfg g_network[NetworkResetNum] = {
    {NetworkReset, "NetworkReset", NULL, NULL, 0},
    {NetworkResetCancel, "NetworkResetCancel", NULL, NULL, 0}};

settings_cfg g_device[DeviceNum] = {{DeviceReset, "DeviceReset", NULL, NULL, 0},
                                    {DeviceUnbind, "DeviceUnbind", NULL, NULL, 0}};
static int delete_specified_voice_type(const char *list, const char *key)
{
    char *ptr, *p1, *p2;
    int len;

    if (!list || !key) {
        return -1;
    }

    ptr = list;

    p1 = p2 = strstr(ptr, key);
    if (p1) {
        while (*--p1 != '{') {
            // do nothing
        }

        ++p2;
        while ((*p2 != '{') && *p2 != ']') {
            ++p2;
        }
        if (*p2 == ']') {
            --p1;
        }

        len = strlen(p2) + 1;

        memmove(p1, p2, len);
    }

    return 0;
}

static int update_single_cfg(settings_cfg *param, int num, char *value)
{
    if (!param || !value || !strlen(value))
        return -1;

    if (param[num].value) {
        free(param[num].value);
        param[num].value = NULL;
    }
    param[num].value = strdup(value);

    return 0;
}

static int handle_network_settings(char *key)
{
    int ret = -1;

    if (StrEq(key, "NetworkReset")) {
        asr_player_set_pause();
        asr_clear_alert_cfg();
        notify_enter_oobe_mode(0);
    } else if (StrEq(key, "NetworkResetCancel")) {
        asr_clear_alert_cfg();
        clova_alerts_request_sync_cmd(NAME_ALERT_REQUEST_SYNC);
        notify_quit_oobe_mode();
    }

    return ret;
}

static void init_dnd_info(void)
{
    int ret = -1;

    ret = clova_set_dnd_expire_time(g_DND[DNDNowEndTime].value);
    if (ret != 0) {
        if (g_DND[DNDNowEndTime].value) {
            free(g_DND[DNDNowEndTime].value);
            g_DND[DNDNowEndTime].value = NULL;
        }
    } else {
        asr_config_set("dnd_now_end_time", g_DND[DNDNowEndTime].value);
        asr_config_sync();
    }

    clova_set_dnd_info(g_DND[DNDEnable].value, g_DND[DNDStartTime].value, g_DND[DNDEndTime].value);
}

static int clear_changed_flag(settings_cfg *param, int num)
{
    int i;

    if (!param || num <= 0)
        return -1;

    for (i = 0; i < num; i++) {
        if (param[i].changed) {
            param[i].changed = 0;
        }
    }

    return 0;
}

static int check_changed(int change_flags)
{
    if (change_flags & KEYWORD_BIT) {
        if (g_keyword[KeywordAttendingSoundEnable].changed) {
            char *tmp = g_keyword[KeywordAttendingSoundEnable].value;
            if (StrEq(tmp, "true")) {
                set_prompt_setting_from_settings(1);
            } else if (StrEq(tmp, "false")) {
                set_prompt_setting_from_settings(0);
            }
        }

        if (g_keyword[KeywordCurrent].changed) {
            char *tmp = g_keyword[KeywordCurrent].value;
            int index = 0;
            if (StrEq(tmp, "KR01")) {
                index = 0;
            } else if (StrEq(tmp, "KR02")) {
                index = 1;
            } else if (StrEq(tmp, "KR03")) {
                index = 2;
            } else if (StrEq(tmp, "KR04")) {
                index = 3;
            } else if (StrEq(tmp, "KR05")) {
                index = 4;
            } else if (StrEq(tmp, "KR06")) {
                index = 5;
            } else if (StrEq(tmp, "KR07")) {
                index = 6;
            }

            notify_switch_keyword_type(index);

            tmp = NULL;
            switch (index) {
            case 0:
                tmp = KEYWORD_PROMPT_HEY_CLOVA;
                break;
            case 1:
                tmp = KEYWORD_PROMPT_CLOVA;
                break;
            case 2:
                tmp = KEYWORD_PROMPT_SELIYA;
                break;
            case 3:
                tmp = KEYWORD_PROMPT_JESSICA;
                break;
            case 4:
                tmp = KEYWORD_PROMPT_JJANGGUYA;
                break;
            case 5:
                tmp = KEYWORD_PROMPT_PINOCCHIO;
                break;
            case 6:
                tmp = KEYWORD_PROMPT_HI_LG;
                break;
            default:
                tmp = KEYWORD_PROMPT_HEY_CLOVA;
                break;
            }

            if (!(change_flags & SILENCE_BIT))
                clova_speech_request_cmd(tmp, CLOVA_PAYLOAD_VALUE_KO);
        }

        clear_changed_flag(g_keyword, KeywordNum);
    }

    if (change_flags & VOICETYPE_BIT) {
        if (g_voiceType[VoiceTypeCurrent].changed) {
            char *tmp = g_voiceType[VoiceTypeCurrent].value;
            if (tmp && strstr(tmp, "V00")) {
                set_local_tone_from_settings(tmp);
                clova_set_voice_type(tmp);
            }
        }

        clear_changed_flag(g_voiceType, VoiceTypeNum);
    }

    if (change_flags & DND_BIT) {
        clear_changed_flag(g_DND, DNDNum);
        init_dnd_info();
    }

    if (change_flags & BT_BIT) {
        if (get_bt_sound_path()) {
            asr_bluetooth_sound_path_change(0);
        } else {
            asr_bluetooth_sound_path_change(1);
        }
    }

    if (change_flags & DEVICEINFO_BIT) {
        clear_changed_flag(g_deviceInfo, DeviceInfoNum);
    }

    if (change_flags & ALARM_BIT) {
        if (g_alarm[AlarmSoundCurrent].changed) {
            if (get_asr_alert_status() && !(change_flags & SILENCE_BIT)) {
                char buff[64] = {0};
                snprintf(buff, sizeof(buff), "/tmp/file_player_%d", e_file_alert);
                SocketClientReadWriteMsg(ALERT_SOCKET_NODE, "ALERT+LOCAL+DEL",
                                         strlen("ALERT+LOCAL+DEL"), NULL, NULL, 0);
                killPidbyName(buff);
            }
            set_alarm_sound(g_alarm[AlarmSoundCurrent].value);
            if (!(change_flags & SILENCE_BIT)) {
                int alarm_type = get_alarm_sound();
                char *alarm_path = alarm_type == 1 ? KR_ALARM_DOLCE : alarm_type == 2
                                                                          ? KR_ALARM_LEGATO
                                                                          : KR_ALARM_MARGATO;
                apply_alert_volume();
                usleep(100000);
                common_focus_manage_set(e_focus_alert);
                file_play_with_path(alarm_path, 0, 0, e_focus_alert);
            }
        }

        if (g_alarm[AlarmVolumeCurrent].changed) {
            set_alert_volume(g_alarm[AlarmVolumeCurrent].value);
            if (!(change_flags & SILENCE_BIT)) {
                apply_alert_volume();
                volume_pormpt_action(VOLUME_KEY_VOLUME);
            }
            usleep(400000);
            restore_system_volume(0);
        }

        if (g_alarm[AlarmSoundChangeCompleted].changed) {
            if (!(change_flags & SILENCE_BIT)) {
                char buff[64] = {0};
                snprintf(buff, sizeof(buff), "/tmp/file_player_%d", e_file_alert);
                if (!get_asr_alert_status() && GetPidbyName(buff)) {
                    killPidbyName(buff);
                    common_focus_manage_clear(e_focus_alert);
                }
                restore_system_volume(1);
            }
        }
        clear_changed_flag(g_alarm, AlarmNum);
    }

    if (change_flags & LOCATION_BIT) {
        clear_changed_flag(g_locationInfo, LocationInfoNum);
    }

    if (change_flags & STORE_BIT) {
        clova_settings_request_store_event();
    }

    return 0;
}

static int handle_device_settings(char *key)
{
    int ret = -1;
    int change_flags = 0;

    if (StrEq(key, "DeviceUnbind")) {
        update_single_cfg(g_keyword, KeywordCurrent, "KR01");
        update_single_cfg(g_voiceType, VoiceTypeCurrent, "V001");
        g_keyword[KeywordCurrent].changed = 1;
        g_voiceType[VoiceTypeCurrent].changed = 1;
        change_flags |= KEYWORD_BIT;
        change_flags |= VOICETYPE_BIT;
        change_flags |= SILENCE_BIT;
        check_changed(change_flags);
        asr_player_force_stop(1);
        asr_clear_auth_info(1);
        restore_volume_to_factory_settings();
        notify_enter_oobe_mode(-1);
        notify_clear_wifi_history();
        notify_disconnect_bt();
        asr_bluetooth_reset();
        asr_clear_alert_cfg();
        asr_config_set("dnd_now_end_time", "");
        asr_config_sync();
    } else if (StrEq(key, "DeviceReset")) {
        update_single_cfg(g_device, DeviceReset, "true");
        clova_settings_report_event();
        notify_do_factory_reset();
    }

    return ret;
}

static int parse_single_cfg(json_object *json_body, settings_cfg *l_cfg, int num)
{
    settings_cfg *param = l_cfg;
    char *tmp = NULL;
    int i;

    for (i = 0; i < num; i++) {
        tmp = JSON_GET_STRING_FROM_OBJECT(json_body, param[i].key);
        if (tmp && strlen(tmp)) {
            if (param[i].value) {
                free(param[i].value);
                param[i].value = NULL;
            }
            param[i].value = strdup(tmp);
        }
    }

    return 0;
}

static int parse_cfg(char *body)
{
    json_object *json_settings = NULL;
    json_object *json_keyword = NULL;
    json_object *json_voiceType = NULL;
    json_object *json_DND = NULL;
    json_object *json_bt = NULL;
    json_object *json_deviceInfo = NULL;
    json_object *json_alarm = NULL;
    json_object *json_locationInfo = NULL;
    json_object *json_device = NULL;
    int change_flags = 0;

    json_settings = json_tokener_parse(body);

    if (json_settings) {
        json_keyword = json_object_object_get(json_settings, "Keyword");
        if (json_keyword) {
            parse_single_cfg(json_keyword, g_keyword, KeywordNum);
        }

        json_voiceType = json_object_object_get(json_settings, "VoiceType");
        if (json_voiceType) {
            parse_single_cfg(json_voiceType, g_voiceType, VoiceTypeNum);
        }

        json_DND = json_object_object_get(json_settings, "DND");
        if (json_DND) {
            parse_single_cfg(json_DND, g_DND, DNDNum);
        }

        json_deviceInfo = json_object_object_get(json_settings, "DeviceInfo");
        if (json_deviceInfo) {
            parse_single_cfg(json_deviceInfo, g_deviceInfo, DeviceInfoNum);
        }

        json_alarm = json_object_object_get(json_settings, "Alarm");
        if (json_alarm) {
            parse_single_cfg(json_alarm, g_alarm, AlarmNum);
            set_alert_volume(g_alarm[AlarmVolumeCurrent].value);
            set_alarm_sound(g_alarm[AlarmSoundCurrent].value);
        }

        json_locationInfo = json_object_object_get(json_settings, "Location");
        if (json_locationInfo) {
            parse_single_cfg(json_locationInfo, g_locationInfo, LocationInfoNum);
        }

        json_device = json_object_object_get(json_settings, "Device");
        if (json_device) {
            parse_single_cfg(json_device, g_device, DeviceNum);
        }

        json_object_put(json_settings);
    }

    return 0;
}

static int read_cfg(void)
{
    FILE *fp = NULL;
    char *buff = NULL;
    size_t len = -1;
    int size = 0;

    fp = fopen(CLOVA_SETTINGS_CONFIG, "rb+");
    if (!fp)
        return -1;

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0) {
        fclose(fp);
        return -1;
    }

    buff = (char *)calloc(1, size + 1);
    if (!buff) {
        fclose(fp);
        return -1;
    }

    len = fread(buff, 1, size, fp);

    fclose(fp);

    parse_cfg(buff);

    if (buff)
        free(buff);

    return 0;
}

static int init_deviceInfo()
{
    int i;
    int change_flags = 0;
    char *name;

    for (i = 0; i < DeviceInfoNum; i++) {
        change_flags |= DEVICEINFO_BIT;
        if (i == DeviceInfoName) {
            if ((name = get_bletooth_name()))
                asprintf(&g_deviceInfo[i].value, "%s", name);
        } else if (i == DeviceInfoDefinedName) {
            if (g_deviceInfo[i].value) {
                if (!strlen(g_deviceInfo[i].value)) {
                    free(g_deviceInfo[i].value);
                    g_deviceInfo[i].value = NULL;
                    g_deviceInfo[i].value = strdup(g_deviceInfo[DeviceInfoName].value);
                }
            } else {
                g_deviceInfo[i].value = strdup(g_deviceInfo[DeviceInfoName].value);
            }
        } else if (i == DeviceInfoWifiMac) {
            unsigned char *wifi_mac = get_wifi_mac_addr();
            if (wifi_mac) {
                asprintf(&g_deviceInfo[i].value, "%02X:%02X:%02X:%02X:%02X:%02X", wifi_mac[0],
                         wifi_mac[1], wifi_mac[2], wifi_mac[3], wifi_mac[4], wifi_mac[5]);
            }
        } else if (i == DeviceInfoSerial) {
            char *sn = get_device_sn();
            if (sn) {
                asprintf(&g_deviceInfo[i].value, "%s", sn);
            }
        } else if (i == DeviceInfoFirmware) {
            char *version = get_device_firmware_version();
            if (version) {
                if (strstr(version, "Linkplay.")) {
                    version += strlen("Linkplay.");
                }
                asprintf(&g_deviceInfo[i].value, "%s", version);
            }
        }
    }

    if (change_flags != 0) {
        check_changed(change_flags);
    }

    return 0;
}

static int update_local_cfg(settings_cfg *param, int num, char *key, char *value)
{
    int i;

    if (!param || !key || num <= 0)
        return -1;

    for (i = 0; i < num; i++) {
        if ((StrEq(param[i].key, key) || StrEq(param[i].changeKey, key)) && !param[i].changed) {
            if (param[i].value) {
                free(param[i].value);
                param[i].value = NULL;
            }
            param[i].value = strdup(value ? value : "");
            param[i].changed = 1;
            break;
        }
    }

    return 0;
}

static int sync_local_cfg(settings_cfg *param, int num, json_object *json_configuration)
{
    int i;
    char *tmp = NULL;

    if (!param || num <= 0 || !json_configuration)
        return -1;

    for (i = 0; i < num; i++) {
        tmp = JSON_GET_STRING_FROM_OBJECT(json_configuration, param[i].key);
        if (tmp && strlen(tmp)) {
            if (param[i].value) {
                free(param[i].value);
                param[i].value = NULL;
            }
            param[i].value = strdup(tmp);
            param[i].changed = 1;
        }
    }

    return 0;
}

static int sync_single_local_cfg(settings_cfg *param, int index, json_object *json_configuration)
{
    char *tmp = NULL;

    if (!param || !json_configuration)
        return -1;

    tmp = JSON_GET_STRING_FROM_OBJECT(json_configuration, param[index].key);
    if (tmp && strlen(tmp)) {
        if (param[index].value) {
            free(param[index].value);
            param[index].value = NULL;
        }
        param[index].value = strdup(tmp);
        param[index].changed = 1;
    }

    return 0;
}

static int parse_synchronize_directive(json_object *json_payload)
{
    json_object *json_configuration = NULL;
    json_object *json_location = NULL;
    char *tmp = NULL;
    char buf[64] = {0};
    int change_flags = 0;

    if (json_payload) {
        json_configuration = json_object_object_get(json_payload, CLOVA_PAYLOAD_NAME_CONFIGURATION);
        if (json_configuration) {
            if (json_object_object_length(json_configuration)) {
                sync_single_local_cfg(g_keyword, KeywordCurrent, json_configuration);
                sync_single_local_cfg(g_keyword, KeywordAttendingSoundEnable, json_configuration);
                sync_single_local_cfg(g_voiceType, VoiceTypeCurrent, json_configuration);
                sync_local_cfg(g_DND, DNDNum, json_configuration);
                sync_local_cfg(g_deviceInfo, DeviceInfoNum, json_configuration);
                sync_single_local_cfg(g_alarm, AlarmSoundCurrent, json_configuration);
                sync_single_local_cfg(g_alarm, AlarmVolumeCurrent, json_configuration);
                sync_local_cfg(g_locationInfo, LocationInfoNum, json_configuration);
            } else {
                g_keyword[KeywordCurrent].changed = 1;
                g_keyword[KeywordAttendingSoundEnable].changed = 1;
                g_voiceType[VoiceTypeCurrent].changed = 1;
                g_DND[DNDEnable].changed = 1;
                g_DND[DNDStartTime].changed = 1;
                g_DND[DNDEndTime].changed = 1;
                g_alarm[AlarmSoundCurrent].changed = 1;
                g_alarm[AlarmVolumeCurrent].changed = 1;
            }

            if (!strstr(g_keyword[KeywordList].value, g_keyword[KeywordCurrent].value)) {
                free(g_keyword[KeywordCurrent].value);
                g_keyword[KeywordCurrent].value = strdup("KR01");
            }

            if (!strstr(g_voiceType[VoiceTypeList].value, g_voiceType[VoiceTypeCurrent].value)) {
                free(g_voiceType[VoiceTypeCurrent].value);
                g_voiceType[VoiceTypeCurrent].value = strdup("V001");
            }

            if (!strstr(g_alarm[AlarmSoundList].value, g_alarm[AlarmSoundCurrent].value)) {
                free(g_alarm[AlarmSoundCurrent].value);
                g_alarm[AlarmSoundCurrent].value = strdup("AlarmSound1");
            }

            asr_config_get("dnd_now_end_time", buf, sizeof(buf));
            update_single_cfg(g_DND, DNDNowEndTime, buf);

            change_flags = 0xff;
            change_flags &= ~BT_BIT;
            check_changed(change_flags);
        }

        return 0;
    }

    return -1;
}

static int parse_update_directive(json_object *json_payload)
{
    json_object *json_configuration = NULL;
    char *value = NULL;
    int change_flags = 0;

    if (!json_payload)
        return -1;

    json_configuration = json_object_object_get(json_payload, CLOVA_PAYLOAD_NAME_CONFIGURATION);
    if (!json_configuration)
        return -1;

    json_object_object_foreach(json_configuration, l_key, l_value)
    {
        value = JSON_GET_STRING_FROM_OBJECT(json_configuration, l_key);
        if (StrInclude(l_key, "Keyword")) {
            update_local_cfg(g_keyword, KeywordNum, l_key, value);
            change_flags |= KEYWORD_BIT;
        } else if (StrInclude(l_key, "VoiceType")) {
            update_local_cfg(g_voiceType, VoiceTypeNum, l_key, value);
            change_flags |= VOICETYPE_BIT;
        } else if (StrInclude(l_key, "DND")) {
            update_local_cfg(g_DND, DNDNum, l_key, value);
            change_flags |= DND_BIT;
        } else if (StrInclude(l_key, "DeviceInfo")) {
            update_local_cfg(g_deviceInfo, DeviceInfoNum, l_key, value);
            change_flags |= DEVICEINFO_BIT;
        } else if (StrInclude(l_key, "Alarm")) {
            update_local_cfg(g_alarm, AlarmNum, l_key, value);
            change_flags |= ALARM_BIT;
        } else if (StrInclude(l_key, "LocationInfo")) {
            update_local_cfg(g_locationInfo, LocationInfoNum, l_key, value);
            change_flags |= LOCATION_BIT;
        } else if (StrInclude(l_key, "Network")) {
            handle_network_settings(l_key);
            return -2;
        } else if (StrInclude(l_key, "Device")) {
            handle_device_settings(l_key);
            return -2;
        } else if (StrInclude(l_key, "BtTwoWayPathMode")) {
            change_flags |= BT_BIT;
        }
    }

    change_flags |= STORE_BIT;

    if (change_flags != 0) {
        check_changed(change_flags);
    }

    return 0;
}

static int parse_update_version_spec_directive(json_object *json_payload)
{
    char *client_name;
    char *device_version;
    char *spec_version;
    char *name;
    char *key;
    char *default_key;
    char *tmp;
    json_object *json_local_voice_type;
    json_object *json_spec;
    json_object *json_voice_type;
    json_object *json_default;
    json_object *json_list;
    json_object *json_list_item;
    int change_flags = 0;
    int list_num;
    int i;

    if (!json_payload) {
        return -1;
    }

    json_local_voice_type = json_tokener_parse(g_voiceType[VoiceTypeList].value);
    if (!json_local_voice_type) {
        return -1;
    }

    client_name = JSON_GET_STRING_FROM_OBJECT(json_payload, CLOVA_PAYLOAD_NAME_CLIENT_NAME);
    device_version = JSON_GET_STRING_FROM_OBJECT(json_payload, CLOVA_PAYLOAD_NAME_DEVICE_VERSION);
    spec_version = JSON_GET_STRING_FROM_OBJECT(json_payload, CLOVA_PAYLOAD_NAME_SPEC_VERSION);

    json_spec = json_object_object_get(json_payload, CLOVA_PAYLOAD_NAME_SPEC);
    if (json_spec) {
        json_voice_type = json_object_object_get(json_spec, CLOVA_PAYLOAD_NAME_VOICE_TYPE);
        if (json_voice_type) {
            name = JSON_GET_STRING_FROM_OBJECT(json_voice_type, CLOVA_PAYLOAD_NAME_NAME);
            json_default = json_object_object_get(json_voice_type, CLOVA_PAYLOAD_NAME_DEFAULT);
            json_list = json_object_object_get(json_voice_type, CLOVA_PAYLOAD_NAME_LIST);
            if (json_default) {
                default_key = JSON_GET_STRING_FROM_OBJECT(json_default, CLOVA_PAYLOAD_NAME_KEY);
            }
            if (json_list) {
                tmp = json_object_to_json_string(json_list);
                list_num = json_object_array_length(json_local_voice_type);
                for (i = 0; i < list_num; i++) {
                    json_list_item = json_object_array_get_idx(json_local_voice_type, i);
                    if (json_list_item) {
                        key = JSON_GET_STRING_FROM_OBJECT(json_list_item, CLOVA_PAYLOAD_NAME_KEY);
                        if (!StrSubStr(tmp, key)) {
                            delete_specified_voice_type(g_voiceType[VoiceTypeList].value, key);
                            if (StrEq(key, g_voiceType[VoiceTypeCurrent].value)) {
                                update_local_cfg(g_voiceType, VoiceTypeNum,
                                                 g_voiceType[VoiceTypeCurrent].key, default_key);
                                change_flags |= VOICETYPE_BIT;
                                change_flags |= STORE_BIT;
                                change_flags |= SILENCE_BIT;
                            }
                        }
                    }
                }
            }
        }
    }

    if (change_flags) {
        check_changed(change_flags);
    }

    json_object_put(json_local_voice_type);

    return 0;
}

void clova_get_location_info_for_context(json_object *json_location)
{
    if (json_location) {
        settings_lock();
        json_object_object_add(json_location, CLOVA_PAYLOAD_LATITUDE_NAME,
                               JSON_OBJECT_NEW_STRING(g_locationInfo[LocationInfoLatitude].value));
        json_object_object_add(json_location, CLOVA_PAYLOAD_LONGITUDE_NAME,
                               JSON_OBJECT_NEW_STRING(g_locationInfo[LocationInfoLongitude].value));
        json_object_object_add(
            json_location, CLOVA_PAYLOAD_REFRESHEDAT_NAME,
            JSON_OBJECT_NEW_STRING(g_locationInfo[LocationInfoRefreshedAt].value));
        settings_unlock();
    }
}

void clova_get_location_info_for_settings(json_object *json_payload)
{
    json_object *json_location = NULL;

    json_location = json_object_new_object();
    if (json_location) {
        settings_lock();
        json_object_object_add(json_location, CLOVA_PAYLOAD_NAME_LATITUDE,
                               JSON_OBJECT_NEW_STRING(g_locationInfo[LocationInfoLatitude].value));
        json_object_object_add(json_location, CLOVA_PAYLOAD_NAME_LONGITUDE,
                               JSON_OBJECT_NEW_STRING(g_locationInfo[LocationInfoLongitude].value));
        json_object_object_add(json_location, CLOVA_PAYLOAD_NAME_ADDRESS,
                               JSON_OBJECT_NEW_STRING(g_locationInfo[LocationInfoAddress].value));
        settings_unlock();
        json_object_object_add(json_payload, CLOVA_PAYLOAD_NAME_LOCATION, json_location);
    }
}

void clova_get_voice_type(json_object *json_payload)
{
    settings_lock();
    json_object_object_add(json_payload, CLOVA_PAYLOAD_NAME_VOICE_TYPE,
                           JSON_OBJECT_NEW_STRING(g_voiceType[VoiceTypeCurrent].value));
    settings_unlock();
}

static int clova_get_single_settings(json_object *json_configuration, settings_cfg *param, int num)
{
    int i;

    for (i = 0; i < num; i++) {
        json_object_object_add(json_configuration, param[i].key,
                               JSON_OBJECT_NEW_STRING(param[i].value));
    }

    return 0;
}

static int clova_get_dnd_settings(json_object *json_configuration, settings_cfg *param, int num)
{
    int i;

    for (i = 0; i < num; i++) {
        if (i != DNDNowEndTime) {
            json_object_object_add(json_configuration, param[i].key,
                                   JSON_OBJECT_NEW_STRING(param[i].value));
        }
    }

    return 0;
}

int clova_get_store_settings(json_object *json_payload)
{
    json_object *json_configuration = NULL;

    json_configuration = json_object_new_object();
    if (!json_configuration)
        return -1;

    settings_lock();
    clova_get_single_settings(json_configuration, g_keyword, KeywordNum);
    clova_get_single_settings(json_configuration, g_voiceType, VoiceTypeNum);
    clova_get_dnd_settings(json_configuration, g_DND, DNDNum);

    json_object_object_add(json_configuration, g_deviceInfo[DeviceInfoDefinedName].key,
                           JSON_OBJECT_NEW_STRING(g_deviceInfo[DeviceInfoDefinedName].value));

    json_object_object_add(json_configuration, g_alarm[AlarmSoundList].key,
                           JSON_OBJECT_NEW_STRING(g_alarm[AlarmSoundList].value));
    json_object_object_add(json_configuration, g_alarm[AlarmSoundCurrent].key,
                           JSON_OBJECT_NEW_STRING(g_alarm[AlarmSoundCurrent].value));
    json_object_object_add(json_configuration, g_alarm[AlarmVolumeMax].key,
                           JSON_OBJECT_NEW_STRING(g_alarm[AlarmVolumeMax].value));
    json_object_object_add(json_configuration, g_alarm[AlarmVolumeCurrent].key,
                           JSON_OBJECT_NEW_STRING(g_alarm[AlarmVolumeCurrent].value));

    clova_get_single_settings(json_configuration, g_locationInfo, LocationInfoNum);

    json_object_object_add(json_payload, CLOVA_PAYLOAD_NAME_CONFIGURATION, json_configuration);
    settings_unlock();

    return 0;
}

int clova_get_report_settings(json_object *json_payload)
{
    char *sound_path = NULL;
    json_object *json_configuration = NULL;

    json_configuration = json_object_new_object();
    if (!json_configuration)
        return -1;

    sound_path = !get_bt_sound_path() ? "true" : "false";

    settings_lock();
    clova_get_single_settings(json_configuration, g_keyword, KeywordNum);
    clova_get_single_settings(json_configuration, g_voiceType, VoiceTypeNum);
    clova_get_single_settings(json_configuration, g_DND, DNDNum);
    clova_get_single_settings(json_configuration, g_deviceInfo, DeviceInfoNum);

    json_object_object_add(json_configuration, g_alarm[AlarmSoundList].key,
                           JSON_OBJECT_NEW_STRING(g_alarm[AlarmSoundList].value));
    json_object_object_add(json_configuration, g_alarm[AlarmSoundCurrent].key,
                           JSON_OBJECT_NEW_STRING(g_alarm[AlarmSoundCurrent].value));
    json_object_object_add(json_configuration, g_alarm[AlarmVolumeMax].key,
                           JSON_OBJECT_NEW_STRING(g_alarm[AlarmVolumeMax].value));
    json_object_object_add(json_configuration, g_alarm[AlarmVolumeCurrent].key,
                           JSON_OBJECT_NEW_STRING(g_alarm[AlarmVolumeCurrent].value));

    clova_get_single_settings(json_configuration, g_locationInfo, LocationInfoNum);

    json_object_object_add(json_configuration, "BtTwoWayPathMode",
                           JSON_OBJECT_NEW_STRING(sound_path));
    json_object_object_add(json_configuration, g_device[DeviceReset].key,
                           JSON_OBJECT_NEW_STRING(g_device[DeviceReset].value));

    json_object_object_add(json_payload, CLOVA_PAYLOAD_NAME_CONFIGURATION, json_configuration);
    settings_unlock();

    return 0;
}

int clova_parse_settings_directive(json_object *json_directive)
{
    json_object *json_header = NULL;
    json_object *json_payload = NULL;
    int ret = -1;

    if (!json_directive)
        return ret;

    json_header = json_object_object_get(json_directive, KEY_HEADER);
    json_payload = json_object_object_get(json_directive, KEY_PAYLOAD);

    if (!json_header || !json_payload)
        return ret;

    char *name_space = JSON_GET_STRING_FROM_OBJECT(json_header, KEY_NAMESPACE);
    char *name = JSON_GET_STRING_FROM_OBJECT(json_header, KEY_NAME);

    settings_lock();
    if (StrEq(name, CLOVA_HEADER_NAME_EXPECT_REPORT)) {
        ret = 0;
    } else if (StrEq(name, CLOVA_HAEDER_NAME_UPDATE)) {
        ret = parse_update_directive(json_payload);
    } else if (StrEq(name, CLOVA_HEADER_NAME_SYNCHRONIZE)) {
        ret = parse_synchronize_directive(json_payload);
    } else if (StrEq(name, CLOVA_HEADER_NAME_UPDATE_VERSION_SPEC)) {
        ret = parse_update_version_spec_directive(json_payload);
    }
    settings_unlock();

    if (!ret)
        clova_settings_report_event();

    return ret;
}

int clova_settings_init(void)
{
    settings_lock();

    init_bk_volume();
    clova_dnd_init();
    read_cfg();
    init_deviceInfo();

    settings_unlock();

    return 0;
}

static void clear_settings(settings_cfg *param, int num)
{
    int i;

    for (i = 0; i < num; i++) {
        if (param[i].value) {
            free(param[i].value);
            param[i].value = NULL;
        }
        param[i].changed = 0;
    }
}

static void uninit_settings(void)
{
    clear_settings(g_keyword, KeywordNum);
    clear_settings(g_voiceType, VoiceTypeNum);
    clear_settings(g_DND, DNDNum);
    clear_settings(g_deviceInfo, DeviceInfoNum);
    clear_settings(g_alarm, AlarmNum);
    clear_settings(g_locationInfo, LocationInfoNum);
}

void clova_settings_uninit(void)
{
    settings_lock();

    clova_dnd_uninit();
    uninit_settings();

    settings_unlock();
}

int clova_cfg_update_dnd_expire_time(char *expire_time)
{
    int ret = -1;

    if (expire_time) {
        settings_lock();
        ret = update_local_cfg(g_DND, DNDNum, "DNDNowEndTime", expire_time);
        clova_settings_request_store_event();
        clear_changed_flag(g_DND, DNDNum);
        settings_unlock();
    }

    return ret;
}

void change_alert_volume_by_button(int gap)
{
    char *alert_volume = NULL;
    int tmp = 8;
    int new_alert_volume = 0;

    settings_lock();
    alert_volume = g_alarm[AlarmVolumeCurrent].value;
    if (alert_volume)
        tmp = atoi(alert_volume);

    if (gap == -1) {
        new_alert_volume = --tmp < 0 ? 0 : tmp;
    } else {
        new_alert_volume = ++tmp > 15 ? 15 : tmp;
    }

    if (alert_volume) {
        free(alert_volume);
        alert_volume = NULL;
        g_alarm[AlarmVolumeCurrent].value = NULL;
    }

    alert_volume = (char *)calloc(1, 8);
    if (alert_volume) {
        snprintf(alert_volume, 8, "%d", new_alert_volume);
        g_alarm[AlarmVolumeCurrent].value = alert_volume;

        set_alert_volume(g_alarm[AlarmVolumeCurrent].value);
    }
    settings_unlock();
}

void cic_conn_unauth(void) { handle_device_settings("DeviceUnbind"); }
