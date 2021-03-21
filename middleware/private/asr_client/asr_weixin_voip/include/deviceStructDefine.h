#ifndef deviceStructDefine_h
#define deviceStructDefine_h

#include "deviceCommonDef.h"
#include "voipSDK.h"
#include "xiaoweiSDK.h"

/****************************** Define begin
 * ***********************************/

/**
 * @brief Device info for init device SDK
 */
typedef struct _device_info {
    /*!< Device system type. Select from these Enum {"Android", "Linux",
     * "Windows", "IOS", "Unknown"} */
    const char *os_platform;
    /*!< The following 4 params should be obtained from the device platform */
    const char *device_serial_number;
    const char *device_license;
    int product_id;
    int key_version;
    /*!< Hardware version for OTA reference */
    unsigned long long user_hardware_version;
    /*!< Debug flag, 0:default  1:debug mode */
    unsigned int run_mode;
    /*!< should be obtained from the device platform */
    const char *xiaowei_appid;
    /*!<cert file path for https download. Linux device do not need set this. Set
     * nullptr to use default file. */
    const char *ca_path;
} DEVICE_INFO;

/**
 * @brief Auto start service after init success,
 * @code
 * for example, if you want to auto start xiaowei & voip service:
 *     int autoFlag = auto_start_xiaowei | auto_start_voip;
 *     device_init(..., auto_start_service_mask = autoFlag);
 */
typedef enum _auto_start_service_bit {
    auto_start_none = 0x0,
    auto_start_xiaowei = 0x1,
    auto_start_voip = 0x2,
    auto_start_miniapp = 0x4,
    auto_start_wechat_message = 0x8,
} AUTO_START_BIT;

/**
 * @brief device binder info
 */
typedef struct _device_binder_info {
    /*!< device ID */
    char username[1024];
    /*!< WeChat nick name */
    char nickname[256];
    /*!< Binder head image url */
    char head_img[1024];
    /*!< Binder remark */
    char remark[256];
    /*!< 0:femal 1:male other:unknow */
    unsigned int sex;

    /*!< Internal use, user not need this */
    char export_id[1024];

    /*!< 0: normal contact, 1: manager */
    int type;
} DEVICE_BINDER_INFO;

/**
 * @brief device base notify for init device SDK
 */
typedef struct _device_notify {
    /**
     * @brief Network change notify
     * @param status 1:online 0:offline
     */
    void (*on_net_status_changed)(int status);

    /**
     * @brief Login complete notify
     * @param status 0:success other:failed
     */
    void (*on_login_complete)(int error_code);

    /**
     * @brief StartLogout complete notify
     * @param status 0:success other:failed
     */
    void (*on_logout_complete)(int error_code);

    /**
     * @brief device nickname change notify, may from app
     * @param status 0:success other:failed
     * @param nickname new nickname
     * @param nickname_length new nickname length
     */
    void (*on_nickname_update)(int error_code, const char *nickname, int nickname_length);

    /**
     * @brief device head change notify, may from app
     * @param status 0:success other:failed
     * @param nickname new head url
     * @param nickname_length new head url length
     */
    void (*on_avatar_update)(int error_code, const char *avatar_url, int url_length);

    /**
     * @brief contacts list change notify
     * @param status 0:success other:failed
     * @param p_binder_list contacts
     * @param n_count contact number
     */
    void (*on_binder_list_change)(int error_code, const DEVICE_BINDER_INFO *p_binder_list,
                                  int n_count);

    /**
     * @brief someone want to bind this device. If your device doesn't have a
     * screen to show his info, it is not recommended to handle this notify.
     *        Handle this notify by device_handle_binder_verify_request
     * @param info this person's info
     */
    void (*on_binder_verify)(DEVICE_BINDER_INFO info);

    /**
     * @brief qr code refreshed
     * @param errCode 0 = no error
     * @param qr_code_url qr_code_url
     * @param expire_time qr code expireTime
     */
    void (*on_qr_code_refresh)(int err_code, const char *qr_code_path, unsigned int expire_time,
                               unsigned int expire_in);

    /**
     * @brief qr code has been scan by someone
     * @param nickname nickname of scan user
     * @param avatar_url avatar url scan user
     * @param scan_time scan time of this qr code
     */
    void (*on_qr_code_scan)(const char *nickname, const char *avatar_url, unsigned int scan_time);

} DEVICE_NOTIFY;

typedef struct _device_init_path {
    /** device will write config file, data and logs in this path. An illegal path
     * will lead to system_err while calling device_init() */
    const char *system_path;

    /** device data capacity limitation. The old data will be overwrited when the
     data size reaches the threshold. The capacity must no less than
     10M(system_path_capacity = 1024*1024*10), Set 0 to disable capacity
     limitation
     */
    unsigned long long system_path_capacity;

} DEVICE_INIT_PATH;

/**
 * @brief xiaowei custom log callback, call {@see device_set_log_func} before
 * init device SDK.
 * @return SDK will do nothing if reutrn 0. Return 1 to write local log file by
 * SDK automaticly. Then xiaowei cloud can pull that log by internal command. We
 * suggest that return 1 if log level is warn or error(3 or 4). Local log file
 * will be auto cleaned to keep total size less than your
 * setting(system_path_capacity).
 */
typedef int (*device_log_func)(int level, const char *tag, const char *filename, int line,
                               const char *funcname, const char *data);

/**
 * @brief callback for {@see device_get_public_qr_code}.
 */
typedef void (*bind_ticket_callback)(int err_code, const char *ticket, unsigned int expire_time,
                                     unsigned int expire_in);

/**
 * @brief callback for {@see device_unregister}.
 * @retval err_code 0 is success
 */
typedef void (*unregister_callback)(int err_code);

/**
 * @brief device profile
 */
typedef struct _device_profile {
    char default_device_name[256];
    char default_device_head[1024];
    char device_name[256];
    char device_head[1024];

} DEVICE_PROFILE;

/** OTA */
typedef struct _device_ota_update_info {
    /*!< update setting time, unit second */
    unsigned long long update_time;
    /*!< UNKNOWN = 0, SOFTWARE = 1, FIRMWARE = 2, HARDWARE = 3, only SOFTWARE
     * support currently*/
    unsigned int update_type;
    /*!< latest version */
    unsigned long long version;
    /*!< update suggestion level */
    unsigned int update_level;
    /*!< current dev flag (=device_query_ota_update.debug_flag)*/
    unsigned int dev_flag;
    /*!< new version name */
    const char *version_name;
    /*!< new version update tips */
    const char *update_tips;
    /*!< new version update extra info */
    const char *extra_info;
} OTA_UPDATE_INFO;

typedef struct _device_ota_package_info {
    unsigned long long package_size;
    const char *package_url;
    unsigned int package_hash_type; /*!< MD5 = 1, SHA1 = 2, SHA256 = 3 */
    const char *package_hash;
} OTA_PACKAGE_INFO;

/**
 * @brief device OTA result
 */
typedef struct _device_ota_callback {
    /** ota check result, if new_version == false, update_info and package_info
     * may == nullptr*/
    void (*on_ota_check_result)(int err_code, const char *business_id, bool new_version,
                                OTA_UPDATE_INFO *update_info, OTA_PACKAGE_INFO *package_info);

    /** download progress, for UI using*/
    void (*on_ota_download_progress)(const char *url, double current, double total);

    /** error_code > 0 see CURL. else -1,hash error;-2 unknown hash type. */
    void (*on_ota_download_error)(const char *url, int err_code);

    /** download complete*/
    void (*on_ota_download_complete)(const char *url, const char *file_path);
} DEVICE_OTA_RESULT;

#endif /* deviceStructDefine_h */
