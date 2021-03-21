// Copyright © 2020 Tencent. All rights reserved.

#include "xiaowei_sdk_interface.h"
#include <assert.h>
#include <string.h>
#include <fcntl.h>

#include "wm_context.h"
#include "wm_util.h"
#include "wm_db.h"
#include "xiaowei_linkplay_interface.h"
#include "xiaowei_tts_player.h"
#include "url_player_xiaowei.h"

#include "voipSDK.h"

int waitForLoginComplete = 0;
extern WIIMU_CONTEXT *g_wiimu_shm;
XIAOWEI_PARAM_STATE g_xiaowei_report_state;
extern int g_xiaowei_wakeup_check_debug;
extern url_player_t *g_url_player;
extern int g_sampleleninms;
extern int g_silence_ota_query;

namespace xiaowei_sdk_interface
{
#ifdef A98_ALEXA_BELKIN
void get_qqid_info(void)
{
    int fp = -1;
    char buffer[256] = {0};
    unsigned int readsize = 0;

    system("echo deviceid>/sys/class/unifykeys/name");
    fp = open("/sys/class/unifykeys/read", O_RDONLY);
    if (fp > 0) {
        readsize = read(fp, buffer, 256);
        sscanf(buffer, "SN:%s LICENSE:%s", txqq_guid, txqq_licence);
        printf("%s:read sn/license info:readsize=%d,buffer=%s,guid=%s,license=%s\n", __func__,
               readsize, buffer, txqq_guid, txqq_licence);

        close(fp);
    } else {
        printf("%s:open unifykeys file fail,fp=%d\n", __func__, fp);
    }
    strcpy(txqq_product_id, "1732");
    strcpy(txqq_appid, "ilinkapp_06000000d6ffcb");
    strcpy(txqq_key_ver, "2");
}
#else
void get_qqid_info(void)
{
    char buf[256] = {0};
    if (wm_db_get_ex(FACOTRY_NVRAM, (char *)"txqq_pid", buf) >= 0) {
        strcpy(txqq_product_id, buf);
    }
    if (wm_db_get_ex(FACOTRY_NVRAM, (char *)"txqq_license", buf) >= 0) {
        strcpy(txqq_licence, buf);
    }
    if (wm_db_get_ex(FACOTRY_NVRAM, (char *)"txqq_guid", buf) >= 0) {
        strcpy(txqq_guid, buf);
    }
    if (wm_db_get_ex(FACOTRY_NVRAM, (char *)"txqq_appid", buf) >= 0) {
        strcpy(txqq_appid, buf);
    }
    if (wm_db_get_ex(FACOTRY_NVRAM, (char *)"txqq_key_ver", buf) >= 0) {
        strcpy(txqq_key_ver, buf);
    }
    strcpy(g_wiimu_shm->txqq_product_id, txqq_product_id);
}
#endif
bool onNetDelayCallback(const char *voice_id, unsigned long long time)
{
    // printf("NetDelayCallback,not use\n");
    return true;
}

bool onRequestCallback(const char *voice_id, XIAOWEI_EVENT event, const char *state_info,
                       const char *extend_info, unsigned int extend_info_len)
{
// printf("onRequestCallback:: state_info=%s\n", (char *)state_info);
#ifdef A98_PHILIPST
    if ((g_wiimu_shm->play_mode != PLAYER_MODE_LINEIN))
#endif
    {
        if (0 == get_hfp_status()) {
            if (event == xiaowei_event_on_idle) {
                wiimu_log(1, 0, 0, 0, (char *)"onRequestCallback:: dialog end \n");
                if (1 == get_duplex_mode()) {
                    set_query_status(e_query_idle);
                    asr_set_audio_resume();
                    if (!g_wiimu_shm->wechatcall_status) {
                        set_asr_led(1);
                    } else if (e_wechatcall_talking == g_wiimu_shm->wechatcall_status) {
                        set_asr_led(2);
                    }
                } else if (g_wiimu_shm->wechatcall_status) {
                    set_asr_led(2);
                }
                // set_asr_led(1);
            }
            if (event == xiaowei_event_on_request_start) {
                wiimu_log(1, 0, 0, 0, (char *)"onRequestCallback:: request start \n");
                set_asr_led(2);
            }
            if (event == xiaowei_event_on_speak) {
                printf((char *)"onRequestCallback:: on speak \n");
            }
            if (event == xiaowei_event_on_silent) {
                wiimu_log(1, 0, 0, 0, (char *)"onRequestCallback:: on silent \n");
                OnSilence();
                set_asr_led(3); // thinking
            }
            if (event == xiaowei_event_on_recognize) {
                // process asr result
                // string asrResult = extend_info;
                if (extend_info)
                    printf((char *)"onRequestCallback:: get asr result: %s \n", extend_info);
            }
            if (event == xiaowei_event_on_response) {
                // get final result
                XIAOWEI_PARAM_RESPONSE *response = (XIAOWEI_PARAM_RESPONSE *)state_info;
                if (!response) {
                    wiimu_log(1, 0, 0, 0, (char *)"%s:response is null,continue next handle\n",
                              __func__);
                    return true;
                }
                wiimu_log(1, 0, 0, 0,
                          (char *)"***************************************** Debug resposne "
                                  "***********************************\n");
                wiimu_log(
                    1, 0, 0, 0, (char *)"********basic result: errcode = %d, request_text = %s,  "
                                        "groupSize = %d *****\n",
                    response->error_code, response->request_text, response->resource_groups_size);

                if (response->skill_info.id && response->skill_info.name) {
                    wiimu_log(1, 0, 0, 0, (char *)"%s:skillinfo name=%s,id=%s\n", __func__,
                              response->skill_info.name, response->skill_info.id);
                    if (g_xiaowei_report_state.skill_info.name &&
                        g_xiaowei_report_state.skill_info.id) {
                        // xiaowei_local_state_report(&g_xiaowei_report_state);
                    }
                }
                if (response->skill_info.id) {
                    if (strncmp(response->skill_info.id, DEF_XIAOWEI_SKILL_ID_ALARM,
                                strlen(DEF_XIAOWEI_SKILL_ID_ALARM)) == 0) {
                        alarm_handle(response);
                    } else if (strncmp(response->skill_info.id, DEF_XIAOWEI_SKILL_ID_VOIP,
                                       strlen(DEF_XIAOWEI_SKILL_ID_VOIP)) == 0) {
                        wechat_call_name_handle(response);
                    } else {
                        if (g_wiimu_shm->wechatcall_status) {
                            asr_set_audio_resume();
                            return true;
                        } else {
                            if (response->is_recovery) {
                                if (g_xiaowei_report_state.skill_info.id)
                                    free((void *)g_xiaowei_report_state.skill_info.id);
                                if (g_xiaowei_report_state.skill_info.name)
                                    free((void *)g_xiaowei_report_state.skill_info.name);
                                g_xiaowei_report_state.skill_info.id =
                                    strdup(response->skill_info.id);
                                g_xiaowei_report_state.skill_info.name =
                                    strdup(response->skill_info.name);
                                g_xiaowei_report_state.skill_info.type = response->skill_info.type;
                            }
                        }
                        // wechat call handle
                    }
                }
                wiimu_log(1, 0, 0, 0, (char *)"------grp_size=%d,more_playlist=%d,more_playlist_up="
                                              "%d,has_history=%d,is_"
                                              "recovery=%d,is_notify=%d,"
                                              "wakeup=%d,play_behave=%d,resource_type=%d\n",
                          response->resource_groups_size, response->has_more_playlist,
                          response->has_more_playlist_up, response->has_history_playlist,
                          response->is_recovery, response->is_notify, response->wakeup_flag,
                          response->play_behavior, response->resource_list_type);

                if (g_xiaowei_wakeup_check_debug) {
                    if (xiaowei_wakeup_flag_fail == response->wakeup_flag) {
                        long long ts = tickSincePowerOn();
                        int hour = ts / 3600000;
                        int min = ts % 3600000 / 60000;
                        int sec = ts % 60000 / 1000;
                        int ms = ts % 1000;
                        char *xiaowei_wakeup_check_fail = NULL;
                        char mv_cmd[128] = {0};

                        wiimu_log(1, 0, 0, 0,
                                  (char *)"%s,find xiaowei cloud check wake up word fail\n",
                                  __func__);
                        asprintf(&xiaowei_wakeup_check_fail,
                                 "wakeup_check_fail_%02d_%02d_%02d_%03d.pcm\n", hour, min, sec, ms);
                        if (xiaowei_wakeup_check_fail) {
                            wiimu_log(1, 0, 0, 0, (char *)"%s:xiaowei_wakeup_check_fail=%s\n",
                                      __func__, xiaowei_wakeup_check_fail);
                            sprintf(mv_cmd, "mv -f /tmp/web/asr_upload.pcm /tmp/web/%s &",
                                    xiaowei_wakeup_check_fail);
                            wiimu_log(1, 0, 0, 0, (char *)"%s:mv_cmd=%s\n", __func__, mv_cmd);
                            system(mv_cmd);
                            free(xiaowei_wakeup_check_fail);
                        }
                    }
                }

                int playlist_replaceall_done_flag = 0;
                for (int i = 0; i < (int)response->resource_groups_size; i++) {
                    for (int j = 0; j < (int)response->resource_groups[i].resources_size; j++) {
                        if (response->resource_groups[i]
                                .resources[j]
                                .id /*&& response->resource_groups[i].resources[j].format == 0*/)
                            wiimu_log(1, 0, 0, 0,
                                      (char *)"###In group %d, resource %d, id = %s, type "
                                              "= %d , content = %s, "
                                              "playcount = %d, extendBuf = %s \n",
                                      i, j, response->resource_groups[i].resources[j].id,
                                      response->resource_groups[i].resources[j].format,
                                      response->resource_groups[i].resources[j].content,
                                      response->resource_groups[i].resources[j].play_count,
                                      response->resource_groups[i].resources[j].extend_buffer);
                        if ((response->resource_groups[i].resources[j].content) &&
                            (response->resource_groups[i].resources[j].id)) {
                            if (response->resource_groups[i].resources[j].format ==
                                    xiaowei_resource_tts_url &&
                                (i == 0)) {
                                set_asr_led(4);
                                xiaowei_play_tts_url(
                                    (char *)response->resource_groups[i].resources[j].content,
                                    (char *)response->resource_groups[i].resources[j].id);
                            } else if (g_wiimu_shm->wechatcall_status) {
                                // wechatcall status is on,do nothing
                            } else {
                                if (response->resource_list_type == xiaowei_playlist_type_default) {
                                    if ((response->resource_groups[i].resources[j].format ==
                                         xiaowei_resource_url) ||
                                        (response->resource_groups[i].resources[j].format ==
                                         xiaowei_resource_tts_url)) {
                                        if ((xiaowei_playlist_replace_all ==
                                             response->play_behavior) &&
                                            (playlist_replaceall_done_flag == 0)) {
                                            playlist_replaceall_done_flag = 1;
                                            if (response->skill_info.id) {
                                                if (strncmp(response->skill_info.id,
                                                            DEF_XIAOWEI_SKILL_ID_NEWS,
                                                            strlen(DEF_XIAOWEI_SKILL_ID_NEWS)) ==
                                                        0 ||
                                                    strncmp(
                                                        response->skill_info.id,
                                                        DEF_XIAOWEI_SKILL_ID_WECHAT_SHARE_RESOURCE,
                                                        strlen(
                                                            DEF_XIAOWEI_SKILL_ID_WECHAT_SHARE_RESOURCE)) ==
                                                        0) {
                                                    g_xiaowei_report_state.play_mode =
                                                        (XIAOWEI_PLAY_MODE)
                                                            play_mode_sequential_no_loop; // no loop
                                                                                          // mode
                                                    set_player_mode(g_url_player,
                                                                    e_mode_playlist_sequential);
                                                } else {
                                                    g_xiaowei_report_state
                                                        .play_mode = (XIAOWEI_PLAY_MODE)
                                                        play_mode_sequential_loop_playlist; // default
                                                                                            // mode
                                                    set_player_mode(g_url_player,
                                                                    e_mode_playlist_loop);
                                                    url_player_set_more_playlist(
                                                        g_url_player,
                                                        (int)(response->has_more_playlist));
                                                    url_player_set_history_playlist(
                                                        g_url_player,
                                                        (int)(response->has_history_playlist));
                                                }
                                            }
                                            xiaowei_url_player_set_play_url(
                                                (char *)response->resource_groups[i]
                                                    .resources[j]
                                                    .id,
                                                (char *)response->resource_groups[i]
                                                    .resources[j]
                                                    .content,
                                                e_replace_all, 0);
                                        } else {
                                            xiaowei_url_player_set_play_url(
                                                (char *)response->resource_groups[i]
                                                    .resources[j]
                                                    .id,
                                                (char *)response->resource_groups[i]
                                                    .resources[j]
                                                    .content,
                                                e_endqueue, 0);
                                        }
                                    } else {
                                        wiimu_log(1, 0, 0, 0,
                                                  (char *)"%s:unsupported resource type=%d\n",
                                                  __func__,
                                                  response->resource_groups[i].resources[j].format);
                                    }
                                } else if (response->resource_list_type ==
                                           xiaowei_playlist_type_history) {
                                    xiaowei_url_player_set_play_url(
                                        (char *)response->resource_groups[i].resources[j].id,
                                        (char *)response->resource_groups[i].resources[j].content,
                                        e_add_history, 0);
                                }
                            }
                        }
                    }
                }
                playlist_replaceall_done_flag = 0;
                global_control_handle(response);
                printf("action type: %d \n", (response->play_behavior));
                if (response) {
                    // you need to process the result here async
                    if (response->context.id != NULL && strlen(response->context.id) >= 16) {
                        printf("onRequestCallback:: session not end, should start another voice "
                               "request "
                               "with this session id = %s \n",
                               response->context.id);
                        if (0 == get_duplex_mode()) {
                            g_wiimu_shm->continue_query_mode = 1;
                            wakeup_context_handle(&(response->context));
                        } else {
                            wiimu_log(
                                1, 0, 0, 0,
                                (char *)"%s:when in duplex mode,will ignore continue_query_mode\n",
                                __func__);
                        }
                    } else if (response->control_id != xiaowei_cmd_report_state) {
                        g_wiimu_shm->continue_query_mode = 0;
                    }
                }
                if (response->resource_groups_size == 0) {
                    if (xiaowei_wakeup_flag_suc_continue == response->wakeup_flag) {
                        // do nothing,this is for wake up check response
                    } else if (response->control_id == xiaowei_cmd_report_state) {
                        // do nothing,this is for global control get play state
                    } else {
                        wiimu_log(1, 0, 0, 0, (char *)"%s:no resource group\n", __func__);
                        if ((response->control_id == xiaowei_cmd_pause) ||
                            (response->control_id == xiaowei_cmd_play_prev) ||
                            (response->control_id == xiaowei_cmd_play_next)) {
                            usleep(200000);
                        }
                        if (get_duplex_mode() == 0) {
                            set_query_status(e_query_idle);
                            asr_set_audio_resume();
                            if (0 == g_wiimu_shm->wechatcall_status) {
                                set_asr_led(1);
                            }
                        }
                    }
                }
            }

            if (event == xiaowei_event_on_response_intermediate) {
                printf("onRequestCallback:: this is not for use \n");
            }
        }
    }
    return true;
}

void loginComplete(int error_code)
{
    int ret = 0;
    printf((char *)"login ok , err code = %d,ezlink_ble_setup=%d\n", error_code,
           g_wiimu_shm->ezlink_ble_setup);
    if (!error_code) {
        if (g_wiimu_shm->ezlink_ble_setup && g_wiimu_shm->xiaowei_login_flag == 0) {
            printf("------%s:before get ticket\n", __func__);
            ret = device_get_bind_ticket(weixin_bind_ticket_callback);
            printf("------%s:after get ticket\n", __func__);
            if (ret) {
                wiimu_log(1, 0, 0, 0, (char *)"%s:device_get_bind_ticket errorcode=%d\n", __func__,
                          ret);
            }
        }
        g_wiimu_shm->xiaowei_login_flag = 1;
    }
    if (9 == error_code) {
        wiimu_log(1, 0, 0, 0, (char *)"%s:device login err,device init fail,so uninit device and "
                                      "try device init again\n",
                  __func__);
        device_uninit();
        XiaoweiLoginInit();
    }
}

void online_net_status_changed(int status)
{
    wiimu_log(1, 0, 0, 0, (char *)"%s:status=%d\n", __func__, status);
    if (0 == status)
        g_wiimu_shm->xiaowei_online_status = 0;
    else {
        g_wiimu_shm->xiaowei_online_status = 1;
    }
}

void autoAcceptContactRequest(DEVICE_BINDER_INFO info)
{
    printf("autoAcceptContactRequest info.username = %s", info.username);
    // device_handle_binder_verify_request(info.username, true);
}

void onBinderListChange(int error_code, const DEVICE_BINDER_INFO *pBinderList, int nCount)
{
    printf("onBinderListChange count = %d\n", nCount);
    for (int i = 0; i < nCount; i++) {
        printf("binder[%d], username = %s, nickname = %s, remark = %s, exportid = %s\n", i,
               pBinderList[i].username, pBinderList[i].nickname, pBinderList[i].remark,
               pBinderList[i].export_id);
    }
}
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void XiaoweiLoginInit()
{
    int ret = 0;
    /* init param */
    DEVICE_INFO device_info = {0};
    DEVICE_INIT_PATH path = {0};
    DEVICE_NOTIFY notify = {0};
    unsigned long long xiaowei_tencent_ver = 1;
    xiaowei_tencent_ver = get_xiaowei_tencent_version();
    wiimu_log(1, 0, 0, 0, (char *)"%s:xiaowei_tencent_ver=%llu\n", __func__, xiaowei_tencent_ver);
    /* device_info, please replace them to yours */
    device_info.device_license = txqq_licence;
    device_info.device_serial_number = txqq_guid;
    device_info.xiaowei_appid = txqq_appid;
    device_info.product_id = atoi(txqq_product_id);
    device_info.key_version = atoi(txqq_key_ver);
    device_info.user_hardware_version = xiaowei_tencent_ver;
    device_info.run_mode = 0;

    // notify
    notify.on_login_complete = loginComplete;
    notify.on_net_status_changed = online_net_status_changed;
    notify.on_binder_list_change = onBinderListChange;
    // contact test
    notify.on_binder_verify = autoAcceptContactRequest;

    path.system_path = (char *)"/vendor/weixin";
    path.system_path_capacity = 1024 * 1024;
    printf("-----test5\n");
    printf("-----test6\n");
    ret = device_init(&device_info, &notify, &path);
    printf("----device_init=%d,g_wiimu_shm->xiaowei_login_flag=%d\n", ret,
           g_wiimu_shm->xiaowei_login_flag);
    g_wiimu_shm->xiaowei_login_flag = 0;
}
#ifdef __cplusplus
}
#endif /* __cplusplus */

void XiaoweiEngineStart()
{
    /* xiaowei service */
    XIAOWEI_CALLBACK xiaoweiCB = {0};
    xiaoweiCB.on_request_callback = onRequestCallback;
    xiaoweiCB.on_net_delay_callback = onNetDelayCallback;
    XIAOWEI_NOTIFY xiaoweoNotify = {0};
    printf("init xiaowei service result = %d \n",
           xiaowei_service_start(&xiaoweiCB, &xiaoweoNotify));
    xiaowei_report_volume(g_wiimu_shm->volume);
    g_xiaowei_report_state.volume = g_wiimu_shm->volume;
}

void XiaoweiTextReq(const std::string &text)
{
    char voice_id[33] = {0};
    XIAOWEI_PARAM_CONTEXT context = {0};
    context.voice_request_begin = true;
    xiaowei_request(voice_id, xiaowei_chat_via_text, text.c_str(),
                    static_cast<unsigned int>(text.length()), &context);
}

void XiaoweiCommonReq(const std::string &cmd, const std::string &sub_cmd, const std::string &param)
{
    char vid[33] = {0};
    xiaowei_request_cmd(vid, cmd.c_str(), sub_cmd.c_str(), param.c_str(),
                        [](const char *voice_id, int xiaowei_err_code, const char *json) {
                            printf("get cmd response\n");
                        });
}

// ota callback functions
DEVICE_OTA_RESULT ota_callback;

void on_ota_check_result(int err_code, const char *bussiness_id, bool new_version,
                         OTA_UPDATE_INFO *update_info, OTA_PACKAGE_INFO *package_info)
{
    wiimu_log(1, 0, 0, 0, (char *)"------%s--err=%d,buss_id=%s,new_version=%d----\n", __func__,
              err_code, bussiness_id, new_version);
    if (update_info) {
        wiimu_log(1, 0, 0, 0,
                  (char *)"updateinfo:deviversion=%d,extra_info=%s,update_level=%d,update_time="
                          "%llu,update_tips=%s,version=%llu,version_name=%s\n",
                  update_info->dev_flag, update_info->extra_info, update_info->update_level,
                  update_info->update_time, update_info->update_tips, update_info->version,
                  update_info->version_name);
    }
    if (package_info) {
        if (package_info->package_url && strlen(package_info->package_url) > 0) {
            fprintf(stderr, (char *)"%s packageinfo:hash=%s,hashtype=%d,package size=%llu,url=%s\n",
                    __func__, package_info->package_hash, package_info->package_hash_type,
                    package_info->package_size, package_info->package_url);
            char buf[1024] = {0};
            if (update_info && update_info->extra_info) {
                if (all_ota_disabled() == 0) {
                    if ((force_ota_disabled() == 0) || (g_silence_ota_query == 1)) {
                        if (g_silence_ota_query) {
                            g_silence_ota_query = 0;
                        }
                        if (strncmp(update_info->extra_info, g_wiimu_shm->firmware_ver,
                                    strlen(g_wiimu_shm->firmware_ver))) {
                            printf((char *)"%s:before send ota cmd\n", __func__);
                            if (1) {
                                sprintf(buf, "StartUpdate:%s", package_info->package_url);
                            } else {
                                sprintf(buf, "SetAllInOneUpdateUrl:%s", package_info->package_url);
                            }
                            SocketClientReadWriteMsg("/tmp/a01remoteupdate", buf, strlen(buf), NULL,
                                                     NULL, 0);
                        }
                    }
                }
            }
        }
    }
}

void on_ota_download_progress(const char *url, double current, double total)
{
    wiimu_log(1, 0, 0, 0, (char *)"------%s------\n", __func__);
}

void on_ota_download_error(const char *url, int err_code)
{
    wiimu_log(1, 0, 0, 0, (char *)"------%s------\n", __func__);
}

void on_ota_download_complete(const char *url, const char *file_path)
{
    wiimu_log(1, 0, 0, 0, (char *)"------%s------\n", __func__);
}

void xiaowei_ota_config_init(void)
{
    ota_callback.on_ota_check_result = on_ota_check_result;
    ota_callback.on_ota_download_complete = on_ota_download_complete;
    ota_callback.on_ota_download_error = on_ota_download_error;
    ota_callback.on_ota_download_progress = on_ota_download_progress;
    device_config_ota(&ota_callback);
}

void xiaowei_query_ota(void)
{
    int res = 0;
    res = device_query_ota_update(NULL, 1, 3, get_xiaowei_ota_flag());
    wiimu_log(1, 0, 0, 0, (char *)"device_query_ota_update return=%d\n", res);
}

void xiaowei_get_sdk_ver(void)
{
    /* get sdk version */
    unsigned int main_ver;
    unsigned int sub_ver;
    unsigned int fix_ver;
    unsigned int build_no;
    device_get_sdk_version(&main_ver, &sub_ver, &fix_ver, &build_no);
    printf((char *)"\ndevice sdk ver = %d.%d.%d.%d\n", main_ver, sub_ver, fix_ver, build_no);
}

void xiaowei_get_uin_info(void)
{
    wiimu_log(1, 0, 0, 0, (char *)"------uin=%llu------\n", device_get_uin());
}

#ifdef VOIP_SUPPORT
// voip callback
VOIP_CALLBACK g_voip_callback;
VOIP_INIT_PARAM g_voip_param;

static void xiaowei_on_voip_invited(const char *fromUsername, int type)
{
    wiimu_log(1, 0, 0, 0, (char *)"---on_voip_invited---fromUsername=%s,type=%d------\n",
              fromUsername, type);
    g_wiimu_shm->wechatcall_status = e_wechatcall_invited;
    WechatcallRingPlayThread();
}

static void xiaowei_on_voip_join_room(int result, bool isMaster)
{
    wiimu_log(1, 0, 0, 0, (char *)"---on_voip_join_room---result=%d,isMaster=%d------\n", result,
              (int)isMaster);
    if (result) {
        wiimu_log(1, 0, 0, 0, (char *)"xiaowei on voip join room fail,should hangup\n");
        g_wiimu_shm->wechatcall_status = e_wechatcall_idle;
        SocketClientReadWriteMsg((char *)"/tmp/RequestASRTTS", (char *)"wechatcall_hangup",
                                 strlen((char *)"wechatcall_hangup"), NULL, NULL, 0);
        set_asr_led(1);
    } else {
        g_wiimu_shm->wechatcall_status = e_wechatcall_request;
    }
}

static void xiaowei_on_voip_invite_result(int result)
{
    wiimu_log(1, 0, 0, 0, (char *)"---on_voip_invite_result---result=%d------\n", result);
    if (result) {
        wiimu_log(1, 0, 0, 0, (char *)"xiaowei on voip invite fail,should hangup\n");
        g_wiimu_shm->wechatcall_status = e_wechatcall_idle;
        SocketClientReadWriteMsg((char *)"/tmp/RequestASRTTS", (char *)"wechatcall_hangup",
                                 strlen((char *)"wechatcall_hangup"), NULL, NULL, 0);
        set_asr_led(1);
    }
}

static void xiaowei_on_voip_cancel(bool isMaster)
{
    wiimu_log(1, 0, 0, 0, (char *)"---on_voip_cancel---isMaster=%d------\n", (int)isMaster);
    g_wiimu_shm->wechatcall_status = e_wechatcall_idle;
    set_asr_led(1);
}

static void xiaowei_on_voip_talking()
{
    wiimu_log(1, 0, 0, 0, (char *)"---on_voip_talking----\n");
    g_wiimu_shm->wechatcall_status = e_wechatcall_talking;
    set_asr_led(2);
    do_wechatcall();
}

void xiaowei_on_voip_finish()
{
    wiimu_log(1, 0, 0, 0, (char *)"---on_voip_finish----\n");
    g_wiimu_shm->wechatcall_status = e_wechatcall_idle;
    set_asr_led(1);
    xiaowei_player_interrupt_exit();
}

static void xiaowei_on_voip_param_change(unsigned int sampleRate, unsigned int sampleLenInms,
                                         unsigned int channels, const char *extends)
{
    wiimu_log(
        1, 0, 0, 0,
        (char *)"---on_voip_param_change----samplerate=%d,sampleleninms=%d,channel=%d,extends=%s\n",
        sampleRate, sampleLenInms, channels, extends);
    if ((sampleLenInms != (unsigned int)g_wiimu_shm->wechatcall_sampleleninms) &&
        (g_wiimu_shm->wechatcall_sampleleninms != 0)) {
        wiimu_log(1, 0, 0, 0, (char *)"---on_voip_param_change----param changed "
                                      "!sampleleninms=%d,wechatcall_sampleleninms=%d\n",
                  sampleLenInms, g_wiimu_shm->wechatcall_sampleleninms);
        wechatcall_uninittimer_handle();
        wechatcall_settimer_handle(sampleLenInms);
    }
    g_wiimu_shm->wechatcall_sampleleninms = sampleLenInms;
    g_wiimu_shm->wechatcall_channel = channels;
    g_wiimu_shm->wechatcall_samplate = sampleRate;
}

static void xiaowei_on_voip_member_change(unsigned int currentNumber, const int *memberIds,
                                          const char **memberUsernames)
{
    wiimu_log(1, 0, 0, 0, (char *)"---on_voip_member_change----\n");
}

static void xiaowei_on_voip_av_change(unsigned int audioEnableNumber, const int *audioEnableIds,
                                      unsigned int videoEnableNumber, const int *videoEnableIds)
{
    wiimu_log(1, 0, 0, 0, (char *)"---on_voip_av_change----\n");
}

void voip_service_init(void)
{
    g_voip_callback.on_voip_av_change = xiaowei_on_voip_av_change;
    g_voip_callback.on_voip_cancel = xiaowei_on_voip_cancel;
    g_voip_callback.on_voip_finish = xiaowei_on_voip_finish;
    g_voip_callback.on_voip_invited = xiaowei_on_voip_invited;
    g_voip_callback.on_voip_invite_result = xiaowei_on_voip_invite_result;
    g_voip_callback.on_voip_join_room = xiaowei_on_voip_join_room;
    g_voip_callback.on_voip_member_change = xiaowei_on_voip_member_change;
    g_voip_callback.on_voip_param_change = xiaowei_on_voip_param_change;
    g_voip_callback.on_voip_talking = xiaowei_on_voip_talking;
    g_voip_param.audio_type = 0;
    g_voip_param.cpu_arch = 1;
    g_voip_param.cpu_core_number = 4;
    g_voip_param.cpu_freq = 1500;
    g_voip_param.def_close_av = 2;
    g_voip_param.def_video_length = 0;
    voip_service_start(&g_voip_callback, &g_voip_param);
}
#endif

} // namespace xw_test
