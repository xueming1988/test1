
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "wm_context.h"
#include "volume_type.h"

#include "json.h"
#include "asr_cmd.h"
#include "asr_cfg.h"
#include "asr_json_common.h"
#include "asr_bluetooth.h"

#include "clova_clova.h"
#include "clova_context.h"
#include "clova_dnd.h"
#include "clova_settings.h"
#include "clova_device.h"
#include "clova_speech_synthesizer.h"

static json_object *g_json_settings = NULL;
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

settings_cfg g_bt[BtNum] = {
    {BtTwoWayPathMode, "BtTwoWayPathMode", NULL, "BtTwoWayPathModeChange", 0}};

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
    {AlarmSoundChangeCompleted, "AlarmSoundChangeCompleted", NULL, 0}};

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

static int mapping_alarm_sound(const char *psound)
{
    int alarm_sound = 0;
    char *ptr = NULL;

    if (!psound)
        return alarm_sound;

    ptr = strstr(psound, "AlarmSound");
    if (ptr) {
        ptr += strlen("AlarmSound");
        alarm_sound = atoi(ptr);
    }

    return alarm_sound;
}

static int handle_network_settings(char *key)
{
    int ret = -1;

    if (StrEq(key, "NetworkReset")) {
        asr_clear_alert_cfg();
        notify_enter_oobe_mode(0);
    } else if (StrEq(key, "NetworkResetCancel")) {
        asr_clear_alert_cfg();
        clova_alerts_request_sync_cmd(NAME_ALERT_REQUEST_SYNC);
        notify_quit_oobe_mode();
    }

    return ret;
}

static int handle_device_settings(char *key)
{
    int ret = -1;

    if (StrEq(key, "DeviceUnbind")) {
        asr_clear_auth_info(1);
        restore_volume_to_factory_settings();
        notify_enter_oobe_mode(-1);
        notify_clear_wifi_history();
        notify_disconnect_bt();
        asr_bluetooth_reset();
        asr_clear_alert_cfg();
        clova_settings_uninit(0);
        clova_settings_init(0);
        asr_config_set("dnd_now_end_time", "");
        asr_config_sync();
    } else if (StrEq(key, "DeviceReset")) {
        update_single_cfg(g_device, DeviceReset, "true");
        clova_settings_report_cmd(CLOVA_HEADER_NAME_REPORT);
        notify_do_factory_reset();
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

static int save_alarm_cfg(char *part_name, settings_cfg *param)
{
    json_object *json_body = NULL;

    if (!part_name || !param || !g_json_settings)
        return -1;

    json_body = json_object_new_object();
    if (!json_body)
        return -1;

    json_object_object_add(json_body, param[AlarmSoundList].key,
                           JSON_OBJECT_NEW_STRING(param[AlarmSoundList].value));
    json_object_object_add(json_body, param[AlarmSoundCurrent].key,
                           JSON_OBJECT_NEW_STRING(param[AlarmSoundCurrent].value));
    json_object_object_add(json_body, param[AlarmVolumeMax].key,
                           JSON_OBJECT_NEW_STRING(param[AlarmVolumeMax].value));
    json_object_object_add(json_body, param[AlarmVolumeCurrent].key,
                           JSON_OBJECT_NEW_STRING(param[AlarmVolumeCurrent].value));

    json_object_object_add(g_json_settings, part_name, json_body);

    return 0;
}

static int save_single_cfg(char *part_name, settings_cfg *param, int index)
{
    json_object *json_body = NULL;

    if (!part_name || !param || index <= 0 || !g_json_settings)
        return -1;

    json_body = json_object_new_object();
    if (!json_body)
        return -1;

    json_object_object_add(json_body, param[index].key, JSON_OBJECT_NEW_STRING(param[index].value));

    json_object_object_add(g_json_settings, part_name, json_body);

    return 0;
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

static int save_cfg(char *part_name, settings_cfg *param, int num)
{
    json_object *json_body = NULL;
    int i;

    if (!part_name || !param || num <= 0 || !g_json_settings)
        return -1;

    json_body = json_object_new_object();
    if (!json_body)
        return -1;

    for (i = 0; i < num; i++) {
        json_object_object_add(json_body, param[i].key, JSON_OBJECT_NEW_STRING(param[i].value));
    }

    json_object_object_add(g_json_settings, part_name, json_body);

    return 0;
}

static int write_cfg(void)
{
    FILE *fp = NULL;
    char *buff = NULL;
    char *val_str = NULL;
    int len = -1;

    if (!g_json_settings) {
        printf("[%s:%d] g_json_settings is NULL\n", __FUNCTION__, __LINE__);
        return -1;
    }

    fp = fopen(CLOVA_SETTINGS_CONFIG, "wb+");
    if (!fp)
        return -1;
    val_str = json_object_to_json_string(g_json_settings);
    if (val_str) {
        buff = strdup(val_str);
    }
    if (!buff) {
        fclose(fp);
        return -1;
    }

    fwrite(buff, 1, strlen(buff), fp);

    // if(buff)
    free(buff);

    fclose(fp);

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
                tmp = KEYWORD_PROMPT_JESSICA;
                break;
            }

            if (!(change_flags & SILENCE_BIT))
                clova_speech_request_cmd(tmp, CLOVA_PAYLOAD_VALUE_KO);
        }

        save_cfg("Keyword", g_keyword, KeywordNum);
        clear_changed_flag(g_keyword, KeywordNum);
    }

    if (change_flags & VOICETYPE_BIT) {
        if (g_voiceType[VoiceTypeCurrent].changed) {
            char *tmp = g_voiceType[VoiceTypeCurrent].value;
            if (tmp && strstr(tmp, "V00")) {
                set_local_tone_from_settings(tmp);
                clova_set_voice_type(tmp);
                save_cfg("VoiceType", g_voiceType, VoiceTypeNum);
            }
        }

        clear_changed_flag(g_voiceType, VoiceTypeNum);
    }

    if (change_flags & DND_BIT) {
        save_cfg("DND", g_DND, DNDNum);
        clear_changed_flag(g_DND, DNDNum);
        init_dnd_info();
    }

    if (change_flags & BT_BIT) {
        save_cfg("Bt", g_bt, BtNum);
        clear_changed_flag(g_bt, BtNum);
    }

    if (change_flags & DEVICEINFO_BIT) {
        save_single_cfg("DeviceInfo", g_deviceInfo, DeviceInfoDefinedName);
        clear_changed_flag(g_deviceInfo, DeviceInfoNum);
    }

    if (change_flags & ALARM_BIT) {
        if (g_alarm[AlarmSoundCurrent].changed) {
            alert_set_sound(g_alarm[AlarmSoundCurrent].value);
            if (!(change_flags & SILENCE_BIT)) {
                int alarm_type = mapping_alarm_sound(g_alarm[AlarmSoundCurrent].value);
                char *alarm_path = alarm_type == 1 ? KR_ALARM_DOLCE : alarm_type == 2
                                                                          ? KR_ALARM_LEGATO
                                                                          : KR_ALARM_MARGATO;
                set_volume_for_alert(g_alarm[AlarmVolumeCurrent].value);
                volume_pormpt_direct(alarm_path, 0, 0);
            }
        }

        if (g_alarm[AlarmVolumeCurrent].changed) {
            if (!(change_flags & SILENCE_BIT)) {
                volume_pormpt_action(VOLUME_KEY_VOLUME);
                set_volume_for_alert(g_alarm[AlarmVolumeCurrent].value);
            }
            alert_set_volume(g_alarm[AlarmVolumeCurrent].value);
            usleep(300000);
            restore_system_volume();
        }

        if (g_alarm[AlarmSoundChangeCompleted].changed) {
            if (!(change_flags & SILENCE_BIT)) {
                if (GetPidbyName("smplayer"))
                    killPidbyName("smplayer");
                restore_system_volume();
            }
        }
        save_alarm_cfg("Alarm", g_alarm);
        clear_changed_flag(g_alarm, AlarmNum);
    }

    if (change_flags & LOCATION_BIT) {
        save_cfg("Location", g_locationInfo, LocationInfoNum);
        clear_changed_flag(g_locationInfo, LocationInfoNum);
    }

    if (change_flags & STORE_BIT) {
        clova_settings_request_store_cmd(CLOVA_HEADER_NAME_REQUEST_STORE_SETTINGS);
    }

    write_cfg();

    return 0;
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
    json_object *json_keyword = NULL;
    json_object *json_voiceType = NULL;
    json_object *json_DND = NULL;
    json_object *json_bt = NULL;
    json_object *json_deviceInfo = NULL;
    json_object *json_alarm = NULL;
    json_object *json_locationInfo = NULL;
    json_object *json_device = NULL;
    int change_flags = 0;

    g_json_settings = json_tokener_parse(body);

    if (g_json_settings) {
        json_keyword = json_object_object_get(g_json_settings, "Keyword");
        if (json_keyword) {
            parse_single_cfg(json_keyword, g_keyword, KeywordNum);
        }

        json_voiceType = json_object_object_get(g_json_settings, "VoiceType");
        if (json_voiceType) {
            parse_single_cfg(json_voiceType, g_voiceType, VoiceTypeNum);
        }

        json_DND = json_object_object_get(g_json_settings, "DND");
        if (json_DND) {
            parse_single_cfg(json_DND, g_DND, DNDNum);
        }

        json_bt = json_object_object_get(g_json_settings, "Bt");
        if (json_bt) {
            parse_single_cfg(json_bt, g_bt, BtNum);
        }

        json_deviceInfo = json_object_object_get(g_json_settings, "DeviceInfo");
        if (json_deviceInfo) {
            parse_single_cfg(json_deviceInfo, g_deviceInfo, DeviceInfoNum);
        }

        json_alarm = json_object_object_get(g_json_settings, "Alarm");
        if (json_alarm) {
            parse_single_cfg(json_alarm, g_alarm, AlarmNum);
            alert_set_volume(g_alarm[AlarmVolumeCurrent].value);
            alert_set_sound(g_alarm[AlarmSoundCurrent].value);
        }

        json_locationInfo = json_object_object_get(g_json_settings, "Location");
        if (json_locationInfo) {
            parse_single_cfg(json_locationInfo, g_locationInfo, LocationInfoNum);
        }

        json_device = json_object_object_get(g_json_settings, "Device");
        if (json_device) {
            parse_single_cfg(json_device, g_device, DeviceNum);
        }
    }

    return 0;
}

static int read_cfg(void)
{
    FILE *fp = NULL;
    char *buff = NULL;
    size_t len = -1;
    int size = 0;

    if (access(CLOVA_SETTINGS_CONFIG, F_OK) != 0) {
        char buf[256] = {0};
        snprintf(buf, sizeof(buf), "cp %s %s", CLOVA_SETTINGS_CONFIG_BK, CLOVA_SETTINGS_CONFIG);
        system(buf);
    }

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

static int init_voice_type(void)
{
    clova_set_voice_type(g_voiceType[VoiceTypeCurrent].value);

    return 0;
}

static int init_deviceInfo()
{
    int i;
    int change_flags = 0;

    for (i = 0; i < DeviceInfoNum; i++) {
        change_flags |= DEVICEINFO_BIT;
        if (i == DeviceInfoName) {
            unsigned char *wifi_mac = get_wifi_mac_addr();
            if (wifi_mac) {
                asprintf(&g_deviceInfo[i].value, "CLOVA-DONUT-%X%02X", wifi_mac[4] & 0xf,
                         wifi_mac[5]);
            }
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
                param[i].changed = 1;
            }
            param[i].value = strdup(value ? value : "");
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
                param[i].changed = 1;
            }
            param[i].value = strdup(tmp);
        }
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
            sync_local_cfg(g_keyword, KeywordNum, json_configuration);
            sync_local_cfg(g_voiceType, VoiceTypeNum, json_configuration);
            sync_local_cfg(g_DND, DNDNum, json_configuration);
            sync_local_cfg(g_deviceInfo, DeviceInfoNum, json_configuration);
            sync_local_cfg(g_alarm, AlarmNum, json_configuration);
            sync_local_cfg(g_bt, BtNum, json_configuration);
            sync_local_cfg(g_locationInfo, LocationInfoNum, json_configuration);

            asr_config_get("dnd_now_end_time", buf, sizeof(buf));
            update_single_cfg(g_DND, DNDNowEndTime, buf);

            change_flags = 0xff;
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
            update_local_cfg(g_bt, BtNum, l_key, value);
            change_flags |= BT_BIT;
            if (StrEq(g_bt[BtTwoWayPathMode].value, "false")) {
                asr_bluetooth_sound_path_change(0);
            } else if (StrEq(g_bt[BtTwoWayPathMode].value, "true")) {
                asr_bluetooth_sound_path_change(1);
            }
        }
    }

    change_flags |= STORE_BIT;

    if (change_flags != 0) {
        check_changed(change_flags);
    }

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
    clova_get_single_settings(json_configuration, g_bt, BtNum);

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
    json_object *json_configuration = NULL;

    json_configuration = json_object_new_object();
    if (!json_configuration)
        return -1;

    settings_lock();
    clova_get_single_settings(json_configuration, g_keyword, KeywordNum);
    clova_get_single_settings(json_configuration, g_voiceType, VoiceTypeNum);
    clova_get_single_settings(json_configuration, g_DND, DNDNum);
    clova_get_single_settings(json_configuration, g_bt, BtNum);

    json_object_object_add(json_configuration, g_deviceInfo[DeviceInfoDefinedName].key,
                           JSON_OBJECT_NEW_STRING(g_deviceInfo[DeviceInfoDefinedName].value));
    json_object_object_add(json_configuration, g_deviceInfo[DeviceInfoWifiMac].key,
                           JSON_OBJECT_NEW_STRING(g_deviceInfo[DeviceInfoWifiMac].value));
    json_object_object_add(json_configuration, g_deviceInfo[DeviceInfoSerial].key,
                           JSON_OBJECT_NEW_STRING(g_deviceInfo[DeviceInfoSerial].value));
    json_object_object_add(json_configuration, g_deviceInfo[DeviceInfoFirmware].key,
                           JSON_OBJECT_NEW_STRING(g_deviceInfo[DeviceInfoFirmware].value));

    json_object_object_add(json_configuration, g_alarm[AlarmSoundList].key,
                           JSON_OBJECT_NEW_STRING(g_alarm[AlarmSoundList].value));
    json_object_object_add(json_configuration, g_alarm[AlarmSoundCurrent].key,
                           JSON_OBJECT_NEW_STRING(g_alarm[AlarmSoundCurrent].value));
    json_object_object_add(json_configuration, g_alarm[AlarmVolumeMax].key,
                           JSON_OBJECT_NEW_STRING(g_alarm[AlarmVolumeMax].value));
    json_object_object_add(json_configuration, g_alarm[AlarmVolumeCurrent].key,
                           JSON_OBJECT_NEW_STRING(g_alarm[AlarmVolumeCurrent].value));

    clova_get_single_settings(json_configuration, g_locationInfo, LocationInfoNum);

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
    }
    settings_unlock();

    if (!ret)
        clova_settings_report_cmd(CLOVA_HEADER_NAME_REPORT);

    return ret;
}

int clova_settings_init(int is_reset)
{
    if (is_reset) {
        settings_lock();
    }
    init_bk_volume();
    clova_dnd_init();
    read_cfg();
    init_deviceInfo();
    if (is_reset) {
        settings_unlock();
    }

    return 0;
}

static void clear_settings(settings_cfg *param, int num)
{
    int i;

    for (i = 0; i < num; i++) {
        if (param[i].value) {
            free(param[i].value);
            param[i].value = NULL;
            param[i].changed = 0;
        }
    }
}

static void uninit_settings(int is_reset)
{
    if (is_reset == 0) {
        if (access(CLOVA_SETTINGS_CONFIG, F_OK) == 0) {
            char tmp[128] = {0};
            snprintf(tmp, sizeof(tmp), "rm %s; sync", CLOVA_SETTINGS_CONFIG);
            system(tmp);
        }
    }

    clear_settings(g_keyword, KeywordNum);
    clear_settings(g_voiceType, VoiceTypeNum);
    clear_settings(g_DND, DNDNum);
    clear_settings(g_bt, BtNum);
    clear_settings(g_deviceInfo, DeviceInfoNum);
    clear_settings(g_alarm, AlarmNum);
    clear_settings(g_locationInfo, LocationInfoNum);

    json_object_put(g_json_settings);
    g_json_settings = NULL;
}

void clova_settings_uninit(int is_reset)
{
    if (is_reset) {
        settings_lock();
    }
    clova_dnd_uninit();
    uninit_settings(is_reset);
    if (is_reset) {
        settings_unlock();
    }
}

int clova_set_bt_sound_path(int flags)
{
    int ret = -1;

    settings_lock();
    ret = update_local_cfg(g_bt, BtNum, "BtTwoWayPathMode", flags ? "true" : "false");
    clear_changed_flag(g_bt, BtNum);
    settings_unlock();
    return ret;
}

int clova_cfg_update_dnd_expire_time(char *expire_time)
{
    int ret = -1;

    if (expire_time) {
        settings_lock();
        ret = update_local_cfg(g_DND, DNDNum, "DNDNowEndTime", expire_time);
        clova_settings_request_store_cmd(CLOVA_HEADER_NAME_REQUEST_STORE_SETTINGS);
        save_cfg("DND", g_DND, DNDNum);
        write_cfg();
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

        alert_set_volume(g_alarm[AlarmVolumeCurrent].value);
        save_alarm_cfg("Alarm", g_alarm);
        write_cfg();
    }
    settings_unlock();
}
