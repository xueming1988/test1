//
//  deviceSDK.h
//  deviceSDK
//
//  Created by cppwang(王缘) on 2019/4/10.
//  Copyright © 2019年 Tencent. All rights reserved.
//

#ifndef deviceSDK_h
#define deviceSDK_h

#include "deviceStructDefine.h"

CXX_EXTERN_BEGIN_DEVICE

/**
 * @brief Initialize the device SDK. A long connection to the cloud will be built, and device
 * registration will be launched asynchronously. Waiting for notifies if you get success
 * return(return 0). Non-zero returns imply serious errors. During the init progressing, use {@see
 * device_check_status} to check current status.
 *
 * @param info All params are required.
 * @param notify device notify callback
 * @param init_path device SDK will write data in this path, do not delete or modify any file under
 * this path.
 * @param auto_start_service_flag auto start service after init success. Set 0 to disable auto
 * start. {@see AUTO_START_BIT}.
 *
 * @note DO NOT call any other API, except device_check_status, untill get success return.
 *
 * @return {@see device_error_code}.
 */
DEVICE_API int device_init(DEVICE_INFO *info, DEVICE_NOTIFY *notify, DEVICE_INIT_PATH *init_path,
                           int auto_start_service_flag, SERVICE_CALLBACK *service_callback);

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
 * @brief Get device sdk version.
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
 * @brief refresh dynamic qr code for bind device, get new qr code in {@see on_qr_code_refresh}
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
 * @brief 3rd-part login, not work at present. Suport in the future
 * @param account
 * @return {@see device_error_code}
 */
// ILink_API int device_set_account_info(DEVICE_PARAM_ACCOUNT *account);

/**
 * @brief 3rd-part login, not work at present. Suport in the future
 * @param log_level 0~5, 0=verbose 5=failtal
 * @param log_print wether print the log to console, set ture to print.
 */
DEVICE_API int device_set_log_func(device_log_func log_func, int log_level, bool log_print,
                                   bool local_file_write);

/**
 * @brief Upload log to server, not work at present. Suport in the future
 */
DEVICE_API int device_upload_sdk_log(void);

/**
 * @brief get binder list
 * @param p_binder_list device SDK will write binder info to this location if the size of
 *pBinderList is enough
 * @param n_count [in]The size of *pBinderList, Too small size will result in incorrect return.
 [out]Real binder number.
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
 * @param jpg_filePath your head image file path, square picture, jpg type. 512x512 or 256x256 is
 * recommended
 * @return {@see device_error_code}
 */
DEVICE_API int device_update_device_avatar(const char *jpg_filePath);

/**
 * @brief get device profile
 * @param profile [input] create an empty profile, and sdk will fill it.
 * @return {@see device_error_code}
 */
DEVICE_API int device_get_device_profile(DEVICE_PROFILE *profile);

/** OTA
 * Extra lib is required
 */
#ifndef __NO_OTA

/**
 * @brief init OTA module(call this after device init finish if you need OTA)
 * @param callback {@link DEVICE_OTA_RESULT}
 * @return {@link device_error_code}
*/
DEVICE_API int device_config_ota(DEVICE_OTA_RESULT *callback);

/**
 * @brief query OTA update info, then get the result in DEVICE_OTA_RESULT.on_ota_check_result
 * @param business_id A unique ID for the update business, same ID in another query will cancel
 current query. Get the business_id in xiaowie website during the register progress.
 * @param channel Update channel, you can use multiple channels for different update strategy.
 * @param ota_type
          enum UpdateType {
              UNKNOWN = 0,
              SOFTWARE = 1,
              FIRMWARE = 2,
              HARDWARE = 3,
          };
 * @param debug_flag 0 = normal, others for debug
 *
 * @return {@link device_error_code}
*/
DEVICE_API int device_query_ota_update(const char *business_id, unsigned int channel,
                                       unsigned int ota_type, unsigned int debug_flag);

/**
 * @brief Start ota download. You also can use this interface to download other resources.
 * @param url Download resource url(get the url in DEVICE_OTA_RESULT.on_ota_check_result)
 * @param download_dir Dir to download the file. Like ".", "/download"
 * @param ca_path If use https download, you may need to provide CA certificate file. Set nullptr to
 use default file.
 * @param hash_type
 *        enum HashType {
             MD5 = 1,
             SHA1 = 2,
             SHA256 = 3,
          };
 * @param hash_value Hash value
 * @return {@link device_error_code}
*/
DEVICE_API int device_ota_download(const char *url, const char *download_dir, const char *ca_path,
                                   unsigned int hash_type, const char *hash_value);

/**
 * @brief Stop download task, will NOT delete current downloading chache file.
 * @param url Download task url, use nullptr to stop all download
 * @return {@link device_error_code}
*/
DEVICE_API int device_ota_stop_download(const char *url);

#endif

CXX_EXTERN_END_DEVICE

#endif /* deviceSDK_h */
