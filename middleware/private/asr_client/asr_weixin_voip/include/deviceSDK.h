#ifndef deviceSDK_h
#define deviceSDK_h

#include "deviceStructDefine.h"

/**
 * @brief Initialize the device SDK. A long connection to the cloud will be
 * built, and device registration will be launched asynchronously. Waiting for
 * notifies if you get success return(return 0). Non-zero returns imply serious
 * errors. During the init progressing, use {@see device_check_status} to check
 * current status.
 *
 * @param info All params are required.
 * @param notify device notify callback
 * @param init_path device SDK will write data in this path, do not delete or
 * modify any file under this path.
 * @param auto_start_service_flag auto start service after init success. Set 0
 * to disable auto start. {@see AUTO_START_BIT}.
 *
 * @note DO NOT call any other API, except device_check_status, untill get
 * success return.
 *
 * @return {@see device_error_code}.
 */
DEVICE_API int device_init(DEVICE_INFO *info, DEVICE_NOTIFY *notify, DEVICE_INIT_PATH *init_path);

/**
 * @brief Check the device SDK status.
 * @retval 0 SDK is uninitialized
 * @retval 1 SDK initialization in progress
 * @retval 2 SDK initialize failed
 * @retval 3 SDK initialize success
 */
DEVICE_API int device_check_status(void);

/**
 * @brief Check the device account status.
 * @retval 0 logout
 * @retval 1 logining
 * @retval 2 login
 * @retval 3 unregister
 */
DEVICE_API int device_get_account_status(void);

/**
 * @brief Get device xwsdk version.
 * @param main_ver ...The SDK will fill in the values of these four pointers
 * @return {@see device_error_code}
 */
DEVICE_API int device_get_sdk_version(unsigned int *main_ver, unsigned int *sub_ver,
                                      unsigned int *fix_ver, unsigned int *build_no);

/**
 * @brief Uninit device SDK, stop all Apps.
 * @return {@see device_error_code}
 */
DEVICE_API int device_uninit(void);

/**
 * @brief refresh dynamic qr code for bind device, get new qr code in {@see
 * on_qr_code_refresh}
 * @return {@see device_error_code}
 */
DEVICE_API int device_refresh_dynamic_qr_code();

/**
 * @brief get ticket for bind device,
 * @param callback get the qr code local path in callback function
 * @return {@see device_error_code}
 */
DEVICE_API int device_get_bind_ticket(bind_ticket_callback callback);

/**
 * @brief Get device uin, call this after device login ok
 * @return 0: failed, other:uin
 */
DEVICE_API unsigned long long device_get_uin(void);

/**
 * @brief Get device id, call this after device login ok
 * @return nullptr: failed, other:device id
 */
DEVICE_API const char *device_get_id();
/*
 * @brief set log callback function
 * @param log_level 0~5, 0=verbose,1=debug,2=info,3=warn,4=error 5=failtal
 * @param log_print wether print the log to console, set true to print.(if
 * device_log_func is not null, device_log_func return 0 will not print.)
 * @param local_file_write set true to write log to local. If device_log_func is
 * set, device_log_func return 1 to write.
 * @return {@see device_error_code}
 */
DEVICE_API int device_set_log_func(device_log_func log_func, int log_level, bool log_print,
                                   bool local_file_write);

/**
 * @brief Upload local log file to server.
 * @param file_path make sure SDK has access of it
 * @return {@see device_error_code}
 */
DEVICE_API int device_upload_sdk_log(const char *file_path);

/**
 * @brief get binder list
 * @param p_binder_list device SDK will write binder info to this location if
 the size of *pBinderList is enough
 * @param n_count [in]The size of *pBinderList, Too small size will result in
 incorrect return. [out]Real binder number.
 * @return {@see device_error_code}
 * @code  DEVICE_BINDER_INFO binders[5] = {0};
          int count = 5;
          int result = device_get_binder_list(binders, &count);
          if(result == error_memory_not_enough && count >0)
          {
             DEVICE_BINDER_INFO bindersAgain[count] = {0};
             device_get_binder_list(binders, &count);
          }
 *
 */
DEVICE_API int device_get_binder_list(DEVICE_BINDER_INFO *p_binder_list, int *n_count);

/**
 * @brief erase all binders
 * @return {@see device_error_code}
 */
DEVICE_API int device_erase_all_binders(void);

/**
 * @brief erase binders by name
 * @param binder_name binder username to be erased.
 * @return {@see device_error_code}
 */
DEVICE_API int device_erase_binders(const char *binder_name);

/**
 * @brief get all bind request
 * @param p_binder_verify_list {@see device_get_binder_list for refrence}
 * @param n_count [in]size of pBinderListVerifyList, [out]real count
 * @return {@see device_error_code}
 */
DEVICE_API int device_get_all_unverify_bind_request(DEVICE_BINDER_INFO *p_binder_verify_list,
                                                    int *n_count);

/**
 * @brief handle binder verify request
 * @param username get by device_get_all_unverify_bind_request
 * @param is_accept true:accept, false:reject
 * @return {@see device_error_code}
 */
DEVICE_API int device_handle_binder_verify_request(const char *username, bool is_accept);

/**
 * @brief change binder's alias(remark)
 * @param username get by device_get_all_unverify_bind_request
 * @param alias new alias
 * @return {@see device_error_code}
 */
DEVICE_API int device_update_binder_alias(const char *username, const char *alias);

/**
 * @brief change device nickname
 * @param nickname new device nickname
 * @return {@see device_error_code}
 */
DEVICE_API int device_update_device_nickname(const char *nickname);

/**
 * @brief change device avatar(head)
 * @param jpg_filePath your head image file path, square picture, jpg type.
 * 512x512 or 256x256 is recommended
 * @return {@see device_error_code}
 */
DEVICE_API int device_update_device_avatar(const char *jpg_filePath);

/**
 * @brief get device profile
 * @param profile [input] create an empty profile, and xwsdk will fill it.
 * @return {@see device_error_code}
 */
DEVICE_API int device_get_device_profile(DEVICE_PROFILE *profile);

/**
 * @brief Unregister device. Usually use this during factory rest process.
 * This function will remove current device info in the backend, and then the
 * SDK will be forced logout due to the authorization failed. A new uin will be
 * created in the next login. Do not kill or logout SDK until the callback
 * coming. Get the result to make sure unregister success.
 * @param callback get unregister result in the callback, 0 is OK.
 * @return {@see device_error_code}
 */
DEVICE_API int device_unregister(unregister_callback callback);

/**
 * @brief upload voice-link result.
 * @param ticket get by device_get_bind_ticket(...)
 * @param token get from voice-link callback. XW_VOICELINK_PARAM.token
 * @return {@see device_error_code}
 */
DEVICE_API int device_upload_voice_link_result(const char *ticket, const char *token);

/** OTA */

/**
 * @brief init OTA module(call this after login complete if you need OTA)
 * @param callback {@link DEVICE_OTA_RESULT}
 * @return {@link device_error_code}
 */
DEVICE_API int device_config_ota(DEVICE_OTA_RESULT *call_back);

/**
 * @brief query OTA update info, then get the result in
 DEVICE_OTA_RESULT.on_ota_check_result
 * @param business_id A unique ID for the update business, same ID in another
 query will cancel current query. Get the business_id in xiaowei website during
 the register progress.
 * @param channel Update channel, you can use multiple channels for different
 update strategy.
 * @param ota_type
          enum UpdateType {
              UNKNOWN = 0,
              SOFTWARE = 1,
              FIRMWARE = 2,
              HARDWARE = 3,
          };
 * @param debug_flag 1 : normal 2 : debug
 *
 * @return {@link device_error_code}
*/
DEVICE_API int device_query_ota_update(const char *business_id, unsigned int channel,
                                       unsigned int ota_type, unsigned int debug_flag);

/**
 * @brief Start ota download. You also can use this interface to download other
 resources.
 * @param url Download resource url(get the url in
 DEVICE_OTA_RESULT.on_ota_check_result)
 * @param download_dir Dir to download the file. Like ".", "/download"
 * @param hash_type
 *        enum HashType {
             MD5 = 1,
             SHA1 = 2,
             SHA256 = 3,
          };
 * @param hash_value Hash value
 * @return {@link device_error_code}
*/
DEVICE_API int device_ota_download(const char *url, const char *download_dir,
                                   unsigned int hash_type, const char *hash_value);

/**
 * @brief Stop download task, will NOT delete current downloading chache file.
 * @param url Download task url, use nullptr to stop all download
 * @return {@link device_error_code}
 */
DEVICE_API int device_ota_stop_download(const char *url);

#endif /* deviceSDK_h */
