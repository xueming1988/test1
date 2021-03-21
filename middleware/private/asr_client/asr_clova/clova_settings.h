#ifndef __CLOVA_SETTINGS__
#define __CLOVA_SETTINGS__

/* Namespace */
#define CLOVA_HEADER_NAMESPACE_SETTINGS "Settings"

/* Directive part */
/* name */
#define CLOVA_HEADER_NAME_EXPECT_REPORT "ExpectReport"
#define CLOVA_HAEDER_NAME_UPDATE "Update"
#define CLOVA_HEADER_NAME_SYNCHRONIZE "Synchronize"
#define CLOVA_HEADER_NAME_UPDATE_VERSION_SPEC "UpdateVersionSpec"

/* Event part */
/* Name */
#define CLOVA_HEADER_NAME_REQUEST_SYNCHRONIZATION "RequestSynchronization"
#define CLOVA_HEADER_NAME_REPORT "Report"
#define CLOVA_HEADER_NAME_REQUEST_STORE_SETTINGS "RequestStoreSettings"
#define CLOVA_HEADER_NAME_REQUEST_VERSION_SPEC "RequestVersionSpec"

/* Payload */
#define CLOVA_PAYLOAD_NAME_DEVICE_ID "deviceId"
#define CLOVA_PAYLOAD_NAME_SYNC_WITH_STORAGE "syncWithStorage"
#define CLOVA_PAYLOAD_NAME_CONFIGURATION "configuration"
#define CLOVA_PAYLOAD_NAME_LOCATION "location"
#define CLOVA_PAYLOAD_NAME_LATITUDE "latitude"
#define CLOVA_PAYLOAD_NAME_LONGITUDE "longitude"
#define CLOVA_PAYLOAD_NAME_ADDRESS "address"
#define CLOVA_PAYLOAD_NAME_VOICE_TYPE "voiceType"
#define CLOVA_PAYLOAD_NAME_CLIENT_NAME "clientName"
#define CLOVA_PAYLOAD_NAME_DEVICE_VERSION "deviceVersion"
#define CLOVA_PAYLOAD_NAME_SPEC_VERSION "specVersion"
#define CLOVA_PAYLOAD_NAME_SPEC "spec"
#define CLOVA_PAYLOAD_NAME_NAME "name"
#define CLOVA_PAYLOAD_NAME_DEFAULT "default"
#define CLOVA_PAYLOAD_NAME_LIST "list"
#define CLOVA_PAYLOAD_NAME_KEY "key"

/* Else */
#define CLOVA_SETTINGS_CONFIG "/etc/clova_settings.conf"

#define KR_ALARM_DOLCE "kr/o_alarm_01.mp3"
#define KR_ALARM_LEGATO "kr/o_alarm_02.mp3"
#define KR_ALARM_MARGATO "kr/o_alarm_03.mp3"

// KR01
#define KEYWORD_PROMPT_HEY_CLOVA "이젠 헤이 클로바 라고 불러주세요."
// KR02
#define KEYWORD_PROMPT_CLOVA "이젠 클로바 라고 불러주세요."
// KR03
#define KEYWORD_PROMPT_SELIYA "이젠 샐리야 라고 불러주세요."
// KR04
#define KEYWORD_PROMPT_JESSICA "이젠 제시카 라고 불러주세요."
// KR05
#define KEYWORD_PROMPT_JJANGGUYA "이젠 짱구야 라고 불러주세요."
// KR06
#define KEYWORD_PROMPT_PINOCCHIO "이젠 피노키오 라고 불러주세요."
// KR07
#define KEYWORD_PROMPT_HI_LG "이젠 Hi LG 라고 불러주세요."
// others
#define KEYWORD_PROMPT_DDOKDDOKA "이젠 Ddokddoka 라고 불러주세요."

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
