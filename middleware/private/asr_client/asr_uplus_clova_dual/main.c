#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <dirent.h>

#include "wm_util.h"
#include "caiman.h"
#include "ccaptureGeneral.h"

#include "asr_cmd.h"
#include "asr_session.h"
#include "asr_alert.h"
#include "asr_cfg.h"
#include "asr_authorization.h"
#include "asr_light_effects.h"
#include "debug_test.h"
#include "common_flags.h"

#include "asr_json_common.h"
#include "clova_clova.h"
#include "clova_speech_synthesizer.h"

int g_asr_req_from = 0;
static int g_internet_access = 0; // debug 1
extern int ignore_reset_trigger;
int g_keeprecording = 0;

static int do_talkstop(int errcode, int req_from);
static ASR_ERROR_CODE do_talkstart_check(int req_from);
static int do_talkstart(int req_from);
static int do_talkstart_prepare(int req_from);
static int do_talkcontinue(int errCode, int req_from);

void check_ota_state(void)
{
    char old_ver[64] = {0};
    char *cur_ver = NULL;

    wm_db_get("lguplus_ota_version", old_ver);
    cur_ver = get_device_firmware_version();

    if (cur_ver && strlen(old_ver)) {
        if (StrEq(cur_ver, old_ver)) {
            volume_pormpt_action(VOLUME_FW_08);
            notify_ota_state(0);
        } else {
            volume_pormpt_action(VOLUME_FW_09);
            notify_ota_state(1);
        }
    }

    wm_db_set_commit("lguplus_ota_version", "");
}

void do_record_reset(int enable)
{
    if (enable) {
        CAIMan_deinit();
        CAIMan_YD_stop();
        CAIMan_ASR_stop();

        CAIMan_init();
        CAIMan_YD_start();
        CAIMan_ASR_start();
        CAIMan_YD_enable();
    } else {
        CAIMan_deinit();
        CAIMan_YD_stop();
        CAIMan_ASR_stop();
    }
    set_asr_talk_start(0);
}

int do_cmd_parse(void)
{
    int recv_num;
    char recv_buf[65536];
    int ret;
    int tlk_start_cnt = 0;
    int is_background = 0;

    SOCKET_CONTEXT msg_socket;
    msg_socket.path = "/tmp/RequestASRTTS";
    msg_socket.blocking = 1;

RE_INIT:
    ret = UnixSocketServer_Init(&msg_socket);
    if (ret <= 0) {
        printf("ASRTTS UnixSocketServer_Init fail\n");
        sleep(5);
        goto RE_INIT;
    }

    while (1) {
        ret = UnixSocketServer_accept(&msg_socket);
        if (ret <= 0) {
            printf("ASRTTS UnixSocketServer_accept fail\n");
            UnixSocketServer_deInit(&msg_socket);
            sleep(5);
            goto RE_INIT;
        } else {
            memset(recv_buf, 0x0, sizeof(recv_buf));
            recv_num = UnixSocketServer_read(&msg_socket, recv_buf, sizeof(recv_buf));
            if (recv_num <= 0) {
                printf("ASRTTS recv failed\r\n");
                if (recv_num < 0) {
                    UnixSocketServer_close(&msg_socket);
                    UnixSocketServer_deInit(&msg_socket);
                    sleep(5);
                    goto RE_INIT;
                }
            } else {
                recv_buf[recv_num] = '\0';

                wiimu_log(1, 0, 0, 0, "ASRTTS recv_buf %s +++++++++", recv_buf);
                if (strncmp(recv_buf, "CAP_RESET", strlen("CAP_RESET")) == 0) {
                    do_record_reset(1);
                } else if (strncmp(recv_buf, "talkstart:", strlen("talkstart:")) == 0) {
                    wiimu_log(1, 0, 0, 0, "talk_start(%d) privacy_mode(%d)\n", get_asr_talk_start(),
                              asr_get_privacy_mode());
                    int req_from = atoi(recv_buf + strlen("talkstart:"));
#ifdef ENABLE_POWER_MONITOR
                    SocketClientReadWriteMsg("/tmp/Requesta01controller", "setSystemActive",
                                             strlen("setSystemActive"), NULL, NULL, 0);
#endif
                    if (tlk_start_cnt > 0) {
                        UnixSocketServer_close(&msg_socket);
                        continue;
                    }

                    if (req_from == 0 && (get_asr_talk_start() != 0 || tlk_start_cnt > 0)) {
                        clova_clear_extension_state();
                        // TODO: stop current recognize request
                        UnixSocketServer_close(&msg_socket);
                        printf("cancel the last reconigze request\n");
                        continue;
                    }
                    if (get_asr_talk_start() == 0) {
                        if (!asr_get_privacy_mode() || req_from == 3) {
                            if (ignore_reset_trigger && req_from != 3) {
                                ignore_reset_trigger = 0;
                                wiimu_log(1, 0, 0, 0, "ignore_reset_trigger ==\n");
                            } else {
                                set_asr_talk_start(1);
                                int ret = do_talkstart(req_from);
                                if (0 == ret) {
                                    tlk_start_cnt++;
                                }
                            }
                        }
                    }
                } else if (strncmp(recv_buf, "talkstop", strlen("talkstop")) == 0) {
                    int req_from = 0;
                    if (strstr(recv_buf, "talkstop:"))
                        req_from = atoi(recv_buf + strlen("talkstop:"));
                    do_talkstop(req_from, req_from);
                    if (tlk_start_cnt > 0) {
                        tlk_start_cnt = 0;
                        set_asr_talk_start(0);
                    }
                } else if (strncmp(recv_buf, "talkcontinue:", strlen("talkcontinue:")) == 0) {
                    if (tlk_start_cnt == 0) {
                        int errCode = atoi(recv_buf + strlen("talkcontinue:"));
                        int ret = do_talkcontinue(errCode, 1);
                        if (0 == ret) {
                            tlk_start_cnt++;
                        }
                    } else {
                        wiimu_log(1, 0, 0, 0, "tlk_start_cnt is %d\n", tlk_start_cnt);
                    }
                } else if (strncmp(recv_buf, "privacy_enable", strlen("privacy_enable")) == 0 ||
                           strncmp(recv_buf, "talksetPrivacyOn", strlen("talksetPrivacyOn")) == 0) {
                    if (tlk_start_cnt > 0) {
                        // TODO: stop current recognize request
                        // SocketClientReadWriteMsg(ASR_EVENT_PIPE, ASR_RECO_STOP,
                        // strlen(ASR_RECO_STOP), NULL, 0, 0);
                    }
                    if (get_internet_flags())
                        asr_set_privacy_mode(e_mode_on);
                    else
                        volume_pormpt_action(VOLUME_W_06);
                } else if (strncmp(recv_buf, "privacy_disable", strlen("privacy_disable")) == 0 ||
                           strncmp(recv_buf, "talksetPrivacyOff", strlen("talksetPrivacyOff")) ==
                               0) {
                    if (get_internet_flags())
                        asr_set_privacy_mode(e_mode_off);
                    else
                        volume_pormpt_action(VOLUME_W_06);
                } else if (strncmp(recv_buf, "INTERNET_ACCESS:", strlen("INTERNET_ACCESS:")) == 0) {
                    g_internet_access = atoi(recv_buf + strlen("INTERNET_ACCESS:"));
                    set_asr_talk_start(0);
                    tlk_start_cnt = 0;
                } else if (strncmp(recv_buf, "VAD_FINISH", strlen("VAD_FINISH")) == 0) {
                    if (tlk_start_cnt == 1) {
                        CAIMan_ASR_finished();
                        tlk_start_cnt = 0;
                        set_asr_talk_start(0);
                    } else {
                        tlk_start_cnt--;
                        tlk_start_cnt = tlk_start_cnt > 0 ? tlk_start_cnt : 0;
                    }
                    CAIMan_YD_enable();
                } else if (strncmp(recv_buf, "VAD_TIMEOUT", strlen("VAD_TIMEOUT")) == 0) {
                    if (tlk_start_cnt == 1) {
                        tlk_start_cnt = 0;
                        asr_light_effects_set(e_effect_listening_off);
                        set_asr_talk_start(0);
                    } else {
                        tlk_start_cnt--;
                        tlk_start_cnt = tlk_start_cnt > 0 ? tlk_start_cnt : 0;
                    }
                    CAIMan_YD_enable();
                } else if (strncmp(recv_buf, "set_asr_server_url", strlen("set_asr_server_url")) ==
                           0) {
                    char *tmp = recv_buf + strlen("set_asr_server_url") + 1;
                    lguplus_parse_server_url(tmp);
                } else if (strncmp(recv_buf, "set_asr_exaccess_token",
                                   strlen("set_asr_exaccess_token")) == 0) {
                    char *tmp = recv_buf + strlen("set_asr_exaccess_token") + 1;
                    lguplus_parse_aiif_token(tmp);
                } else if (strncmp(recv_buf, "set_asr_access_token",
                                   strlen("set_asr_access_token")) == 0) {
                    char *tmp = recv_buf + strlen("set_asr_access_token") + 1;
                    lguplus_parse_naver_token(tmp);
                } else if (strncmp(recv_buf, "set_new_fw", strlen("set_new_fw")) == 0) {
                    char *tmp = recv_buf + strlen("set_new_fw") + 1;
                    lguplus_parse_fw_url(tmp);
                } else {
                    debug_cmd_parse(&msg_socket, recv_buf);
                }
                wiimu_log(1, 0, 0, 0, "ASRTTS recv_buf------");
            }

            UnixSocketServer_close(&msg_socket);
        }
    }
}

/*
* return 0 for ok,  others for fail reason
*/
static ASR_ERROR_CODE do_talkstart_check(int req_from)
{
    if (debug_mode_on()) {
        set_asr_talk_start(0);
        return ERR_ASR_MODULE_DISABLED;
    }

    if (asr_get_privacy_mode()) {
        return ERR_ASR_MODULE_DISABLED;
    }

    if (get_internet_flags() == 0) {
        if (get_network_setup_status())
            volume_pormpt_action(VOLUME_W_06);
        else
            volume_pormpt_action(VOLUME_V_03);
        return ERR_ASR_NO_INTERNET;
    }

    // if access_token not exist
    if ((access(ASR_TOKEN_PATH, 0)) != 0) {
        return ERR_ASR_NO_USER_ACCOUNT;
    }

    if (!asr_session_check_login()) {
        return ERR_ASR_USER_LOGIN_FAIL;
    }

    if ((access(LGUP_TOKEN_PATH, 0)) != 0) {
        return ERR_ASR_NO_USER_ACCOUNT;
    }

    if (!lguplus_check_login()) {
        return ERR_ASR_USER_LOGIN_FAIL;
    }

    if (get_asr_online() == 0) {
        return ERR_ASR_HAVE_PROMBLEM;
    }

    g_keeprecording = 0;

    return 0;
}

static int do_talkstart_prepare(int req_from)
{
    set_asr_talk_start(1);
    common_focus_manage_set(e_focus_recognize);

    if (req_from != 3) {
        CAIMan_ASR_disable();
        asr_session_record_buffer_reset();
    }

    asr_session_start(req_from);

    return 0;
}

static int do_talkstart(int req_from)
{
    int ret;

    update_talk_count();
    ret = do_talkstart_check(req_from);
    if (ret) {
        printf("===== %s talkstart check failed, ret = %d  =====\n", __FUNCTION__, ret);
        asr_light_effects_set(e_effect_error);
        do_talkstop(ret, req_from);
        return -1;
    }

    do_talkstart_prepare(req_from);

    if (req_from == 1)
        CCaptWakeUpFlushState();

    if (req_from != 3) {
        if (get_asr_have_prompt())
            volume_pormpt_action(VOLUME_AT_00);
    }

    g_asr_req_from = req_from;
    if (req_from != 3) {
        if (!IsCCaptStart()) {
            printf("===== %s capture stopped, restart  =====\n", __FUNCTION__);
            CAIMan_deinit();
            CAIMan_ASR_stop();
            CAIMan_init();
            CAIMan_YD_start();
            CAIMan_ASR_start();
            CAIMan_YD_enable();
        }
    }
    CAIMan_ASR_enable(req_from);
    return 0;
}

static int do_talkstop(int errcode, int req_from)
{
    if (debug_mode_on()) {
        set_asr_talk_start(0);
        return 0;
    }

    wiimu_log(1, 0, 0, 0, "do_talkstop ++++");
    set_asr_talk_start(1);
    CAIMan_ASR_disable();
    // TODO: change the volume tye for error
    if (errcode == ERR_ASR_NO_INTERNET) {
        volume_pormpt_action(VOLUME_INTERNET_LOST);
    } else if (errcode == ERR_ASR_HAVE_PROMBLEM) {
        volume_pormpt_action(VOLUME_INTERNET_LOST);
    }

    g_keeprecording = 0;

    CAIMan_YD_enable();
    set_asr_talk_start(0);
    wiimu_log(1, 0, 0, 0, "do_talkstop ----");
    return 0;
}

static int do_talkcontinue(int errCode, int req_from)
{
    int ret = -1;
    if (debug_mode_on()) {
        set_asr_talk_start(0);
        return -1;
    }

    if (asr_get_privacy_mode()) {
        do_talkstop(-1, req_from);
        return -1;
    }

    ret = do_talkstart_check(errCode);
    if (ret) {
        do_talkstop(ret, req_from);
        return -1;
    }

    if (errCode == ERR_ASR_CONTINUE) {
        common_focus_manage_set(e_focus_recognize);
        CAIMan_ASR_disable();
        if (get_asr_have_prompt())
            volume_pormpt_action(VOLUME_AT_00);
        if (g_asr_req_from != 3) {
            asr_session_record_buffer_reset();
        }
    } else {
        do_talkstop(-1, req_from);
        return -1;
    }

    if (g_asr_req_from != 3) {
        asr_session_start(2);
        if (!IsCCaptStart()) {
            printf("===== %s capture stopped, restart  =====\n", __FUNCTION__);
            CAIMan_deinit();
            CAIMan_ASR_stop();

            CAIMan_init();
            CAIMan_ASR_start();
        }
        CAIMan_ASR_enable(0);
        printf("talk to continue enable at %d\n", (int)tickSincePowerOn());
    }

    return 0;
}

void sigroutine(int dunno)
{
    printf("ASR exception get a signal %d\n", dunno);
    switch (dunno) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
    case SIGQUIT:
        common_falgs_uninit();
        exit(1);
        break;
    default:
        printf("ASR exception get a default signal %d\n", dunno);
        break;
    }

    return;
}

int main(int argc, char **argv)
{
    signal(SIGINT, sigroutine);
    signal(SIGTERM, sigroutine);
    signal(SIGKILL, sigroutine);
    signal(SIGALRM, sigroutine);
    signal(SIGQUIT, sigroutine);

    common_flags_init();
    asr_init_cfg();
    alert_init();
    CAIMan_pre_init();
    CAIMan_init();
    CAIMan_YD_start();
    CAIMan_ASR_start();
    CAIMan_YD_enable();
    asr_session_init();

    check_ota_state();

    do_cmd_parse();

    asr_session_uninit();
    common_falgs_uninit();

    return 0;
}
