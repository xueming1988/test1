#ifndef __CLOVA_SETTINGS__
#define __CLOVA_SETTINGS__

/* Namespace */
#define CLOVA_HEADER_NAMESPACE_SETTINGS "Settings"

/* Directive part */
/* name */
#define CLOVA_HEADER_NAME_EXPECT_REPORT "ExpectReport"
#define CLOVA_HAEDER_NAME_UPDATE "Update"
#define CLOVA_HEADER_NAME_SYNCHRONIZE "Synchronize"

/* Event part */
/* Name */
#define CLOVA_HEADER_NAME_REQUEST_SYNCHRONIZATION "RequestSynchronization"
#define CLOVA_HEADER_NAME_REPORT "Report"
#define CLOVA_HEADER_NAME_REQUEST_STORE_SETTINGS "RequestStoreSettings"

/* Payload */
#define CLOVA_PAYLOAD_NAME_DEVICE_ID "deviceId"
#define CLOVA_PAYLOAD_NAME_SYNC_WITH_STORAGE "syncWithStorage"
#define CLOVA_PAYLOAD_NAME_CONFIGURATION "configuration"
#define CLOVA_PAYLOAD_NAME_LOCATION "location"
#define CLOVA_PAYLOAD_NAME_LATITUDE "latitude"
#define CLOVA_PAYLOAD_NAME_LONGITUDE "longitude"
#define CLOVA_PAYLOAD_NAME_ADDRESS "address"
#define CLOVA_PAYLOAD_NAME_VOICE_TYPE "voiceType"

/* Else */
#define CLOVA_SETTINGS_CONFIG_BK "/etc/clova_settings.conf"
#define CLOVA_SETTINGS_CONFIG "/vendor/wiimu/clova_settings.conf"

#define KR_ALARM_DOLCE "kr/o_alarm_01.mp3"
#define KR_ALARM_LEGATO "kr/o_alarm_02.mp3"
#define KR_ALARM_MARGATO "kr/o_alarm_03.mp3"

#define KEYWORD_PROMPT_HEY_CLOVA "이젠 Hey Clova 라고 불러주세요."
#define KEYWORD_PROMPT_CLOVA "이젠 Clova 라고 불러주세요."
#define KEYWORD_PROMPT_JESSICA "이젠 Jessica 라고 불러주세요."

#define KEYWORD_BIT 0x01
#define VOICETYPE_BIT 0x02
#define DND_BIT 0x04
#define BT_BIT 0X08
#define DEVICEINFO_BIT 0x10
#define ALARM_BIT 0x20
#define LOCATION_BIT 0x40
#define SILENCE_BIT 0x80
#define STORE_BIT 0x100

enum { KeywordList, KeywordCurrent, KeywordAttendingSoundEnable, KeywordNum };

enum { VoiceTypeList, VoiceTypeCurrent, VoiceTypeNum };

enum { DNDEnable, DNDStartTime, DNDEndTime, DNDNowEndTime, DNDNum };

enum { BtTwoWayPathMode, BtNum };

enum {
    DeviceInfoName,
    DeviceInfoDefinedName,
    DeviceInfoWifiMac,
    DeviceInfoSerial,
    DeviceInfoFirmware,
    DeviceInfoNum
};

enum {
    AlarmSoundList,
    AlarmSoundCurrent,
    AlarmVolumeMax,
    AlarmVolumeMin,
    AlarmVolumeCurrent,
    AlarmSoundChangeCompleted,
    AlarmNum
};

enum {
    LocationInfoLatitude,
    LocationInfoLongitude,
    LocationInfoRefreshedAt,
    LocationInfoAddress,
    LocationInfoNum
};

enum { NetworkReset, NetworkResetCancel, NetworkResetNum };

enum { DeviceReset, DeviceUnbind, DeviceNum };

typedef struct {
    int index;
    char *key;
    char *value;
    char *changeKey;
    int changed;
} settings_cfg;

#endif
