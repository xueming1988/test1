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
#include <resolv.h>

#include "wm_util.h"
#include "caiman.h"
#include "ccaptureGeneral.h"

#include "asr_cmd.h"
#include "asr_cfg.h"
#include "asr_alert.h"
#include "asr_player.h"
#include "asr_session.h"
#include "asr_json_common.h"
#include "asr_authorization.h"
#include "asr_light_effects.h"

#include "debug_test.h"
#include "common_flags.h"

#include "clova_clova.h"
#include "clova_speech_synthesizer.h"
#include "clova_update.h"

#if defined(LGUPLUS_IPTV_ENABLE)
#include "iptv.h"
#endif

#define CRASH_LOG_PATH ("/tmp/clova")

int g_asr_req_from = 0;
int IptvWakeUpEnable = 1;
extern int ignore_reset_trigger;

static int do_talkstop(int errcode);
static ASR_ERROR_CODE do_talkstart_check(void);
static int do_talkstart(int req_from);
static int do_talkstart_prepare(int req_from);
static int do_talkcontinue(int errCode);

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
    set_session_state(0);
}

int do_cmd_parse(void)
{
    int recv_num;
    char recv_buf[8 * 1024];
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

                    if (get_network_setup_status() || !get_asr_have_token()) {
                        wiimu_log(1, 0, 0, 0,
                                  "ignore the talkstart event in network setup mode (%d %d)\n",
                                  get_network_setup_status(), get_asr_have_token());
                        UnixSocketServer_close(&msg_socket);
                        continue;
                    }

#ifdef ENABLE_POWER_MONITOR
                    SocketClientReadWriteMsg("/tmp/Requesta01controller", "setSystemActive",
                                             strlen("setSystemActive"), NULL, NULL, 0);
#endif
                    if (clova_get_extension_state()) {
                        clova_clear_extension_state();
                        asr_player_buffer_stop(NULL, e_asr_stop);
                    }
                    if (req_from == 0 && (get_asr_talk_start() != 0 || tlk_start_cnt > 0)) {
                        SocketClientReadWriteMsg(ASR_EVENT_PIPE, ASR_RECO_STOP,
                                                 strlen(ASR_RECO_STOP), NULL, 0, 0);
                        tlk_start_cnt = 0;
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
                    do_talkstop(req_from);
                    if (tlk_start_cnt > 0) {
                        tlk_start_cnt = 0;
                        set_asr_talk_start(0);
                    }
                } else if (strncmp(recv_buf, "talkcontinue:", strlen("talkcontinue:")) == 0) {
                    if (tlk_start_cnt == 0) {
                        int errCode = atoi(recv_buf + strlen("talkcontinue:"));
                        int ret = do_talkcontinue(errCode);
                        if (0 == ret) {
                            tlk_start_cnt++;
                        }
                    } else {
                        wiimu_log(1, 0, 0, 0, "tlk_start_cnt is %d\n", tlk_start_cnt);
                    }
                } else if (strncmp(recv_buf, "privacy_enable", strlen("privacy_enable")) == 0 ||
                           strncmp(recv_buf, "talksetPrivacyOn", strlen("talksetPrivacyOn")) == 0) {
                    if (get_asr_have_token()) {
                        volume_pormpt_action(VOLUME_KEY_MUTE);
                        if (tlk_start_cnt > 0) {
                            SocketClientReadWriteMsg(ASR_EVENT_PIPE, ASR_RECO_STOP,
                                                     strlen(ASR_RECO_STOP), NULL, 0, 0);
                            tlk_start_cnt = 0;
                        }
                        asr_set_privacy_mode(e_mode_on);
                    }
                } else if (strncmp(recv_buf, "privacy_disable", strlen("privacy_disable")) == 0 ||
                           strncmp(recv_buf, "talksetPrivacyOff", strlen("talksetPrivacyOff")) ==
                               0) {
                    asr_set_privacy_mode(e_mode_off);

                } else if (strncmp(recv_buf, "VAD_FINISH", strlen("VAD_FINISH")) == 0) {
                    asr_light_effects_set(e_effect_listening_off);
#if defined(LGUPLUS_IPTV_ENABLE)
                    if (g_asr_req_from == 4) {
                        iptv_shm_reset();
                        common_focus_manage_clear(
                            e_focus_recognize); // U+TV quit recognition should clear focus manager
                        asr_notification_resume();
                    }
#endif
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
                    wiimu_log(1, 0, 0, 0, "tlk_start_cnt(%d) asr_talk_star(%d)\n", tlk_start_cnt,
                              get_asr_talk_start());
#if defined(LGUPLUS_IPTV_ENABLE)
                    if (g_asr_req_from == 4) {
                        iptv_shm_reset();
                        asr_notification_resume();
                    }
#endif
                    if (tlk_start_cnt <= 1) {
                        common_focus_manage_clear(e_focus_recognize);
                        asr_light_effects_set(e_effect_listening_off);
                        set_asr_talk_start(0);
                        tlk_start_cnt = 0;
                    } else {
                        tlk_start_cnt--;
                        tlk_start_cnt = tlk_start_cnt > 0 ? tlk_start_cnt : 0;
                    }
                    CAIMan_YD_enable();
                } else if (StrEq(recv_buf, "GetNaverPrepareInfo")) {
                    char *response = asr_get_prepare_info();
                    if (response)
                        UnixSocketServer_write(&msg_socket, response, strlen(response));
                    else
                        UnixSocketServer_write(&msg_socket, "0", strlen("0"));

                    if (response) {
                        free(response);
                    }
                } else if (!strncmp(recv_buf, "SetNaverPrepareInfo",
                                    strlen("SetNaverPrepareInfo"))) {
                    char *tmp = strchr(recv_buf, ':') + 1;
                    asr_set_prepare_info(tmp);
                } else if (!strncmp(recv_buf, "isNetworkOnly", strlen("isNetworkOnly"))) {
                    char *tmp = strchr(recv_buf, ':') + 1;
                    asr_handle_network_only(tmp);
                } else if (StrEq(recv_buf, "GetClovaTokenStatus")) {
                    int ret = asr_check_token_exist();
                    if (!ret)
                        UnixSocketServer_write(&msg_socket, "1", strlen("1"));
                    else
                        UnixSocketServer_write(&msg_socket, "0", strlen("0"));
                } else if (StrInclude(recv_buf, "NaverOTAVer")) {
                    char *tmp = NULL;
                    char *old_ver = NULL;
                    char *new_ver = NULL;

                    tmp = recv_buf + strlen("NaverOTAVer") + 1;
                    old_ver = tmp;
                    new_ver = strchr(tmp, ':');
                    *new_ver++ = '\0';

                    update_OTA_version(old_ver, new_ver);
                } else if (StrInclude(recv_buf, "NaverOTAState")) {
                    char *state = NULL;

                    state = recv_buf + strlen("NaverOTAState") + 1;

                    update_OTA_state(state);
                } else if (StrInclude(recv_buf, "NaverOTAEvent")) {
                    char *tmp = NULL;

                    tmp = recv_buf + strlen("NaverOTAEvent") + 1;
                    if (StrInclude(tmp, "DOWNLOAD")) {
                        int progress = -1;

                        tmp += strlen("DOWNLOAD:");
                        progress = atoi(tmp);
                        update_OTA_process(progress);
                    }
                } else if (StrInclude(recv_buf, "alert_volume")) {
                    char *tmp = NULL;

                    tmp = recv_buf + strlen("alert_volume") + 1;
                    if (StrEq(tmp, "down")) {
                        change_alert_volume_by_button(-1);
                    } else {
                        change_alert_volume_by_button(1);
                    }
                } else if (StrEq(recv_buf, "NetworkDHCPNewIP")) {
#ifdef AMLOGIC_A113
                    res_init();
#endif
                } else if (StrInclude(recv_buf, "IptvWakeUpOnOff")) {
                    char *ptr;

                    ptr = recv_buf + strlen("IptvWakeUpOnOff") + 1;
                    if (StrEq(ptr, "on"))
                        IptvWakeUpEnable = 1;
                    else if (StrEq(ptr, "off"))
                        IptvWakeUpEnable = 0;
                } else if (StrInclude(recv_buf, "UnauthDetect")) {
                    // for Naver, if first ota checking finds unauthorized, notify with this message
                    cic_conn_unauth();
                } else if (StrInclude(recv_buf, "ReportOTAResult")) {
                    report_ota_result();
                } else {
                    debug_cmd_parse(&msg_socket, recv_buf, sizeof(recv_buf));
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
static ASR_ERROR_CODE do_talkstart_check(void)
{
    if (debug_mode_on()) {
        set_asr_talk_start(0);
        return ERR_ASR_MODULE_DISABLED;
    }

    if (asr_get_privacy_mode()) {
        return ERR_ASR_MODULE_DISABLED;
    }

    if (get_internet_flags() == 0) {
        return ERR_ASR_NO_INTERNET;
    }

    // if access_token not exist
    if ((access(ASR_TOKEN_PATH, 0)) != 0) {
        return ERR_ASR_NO_USER_ACCOUNT;
    }

    if (!asr_session_check_login()) {
        return ERR_ASR_USER_LOGIN_FAIL;
    }

    if (get_asr_online() == 0) {
        return ERR_ASR_HAVE_PROMBLEM;
    }

    return 0;
}

static int do_talkstart_prepare(int req_from)
{
    common_focus_manage_set(e_focus_recognize);
    set_asr_talk_start(1);

    // U+TV do not set this flag
    if (req_from != 4)
        set_session_state(1);

    if (req_from != 3) {
        CAIMan_ASR_disable();
#if defined(LGUPLUS_IPTV_ENABLE)
        if (req_from == 0 || req_from == 1)
            asr_session_record_buffer_reset();
        else if (req_from == 4)
            iptv_shm_reset();
#else
        asr_session_record_buffer_reset();
#endif
    }

#if defined(LGUPLUS_IPTV_ENABLE)
    if (req_from == 0 || req_from == 1) {
        char *buf = NULL;
        asprintf(&buf, "%s:%d", ASR_RECO_START, req_from);
        if (buf) {
            SocketClientReadWriteMsg(ASR_EVENT_PIPE, buf, strlen(buf), NULL, NULL, 0);
            free(buf);
            buf = NULL;
        }
    }
#else
    char *buf = NULL;
    asprintf(&buf, "%s:%d", ASR_RECO_START, req_from);
    if (buf) {
        SocketClientReadWriteMsg(ASR_EVENT_PIPE, buf, strlen(buf), NULL, NULL, 0);
        free(buf);
        buf = NULL;
    }
#endif

    return 0;
}

static int do_talkstart(int req_from)
{
    int ret;

#if defined(LGUPLUS_IPTV_ENABLE)
    if (req_from == 4 && !IptvWakeUpEnable)
        return -1;
#endif

    // for clova, when the device is processing force update, do not operate talkstart
    if (check_force_ota())
        return -1;

    update_talk_count();
    ret = do_talkstart_check();
    if (ret) {
        do_talkstop(ret);
        return -1;
    }

    do_talkstart_prepare(req_from);

    if (req_from == 1)
        CCaptWakeUpFlushState();

    if (req_from == 0 || req_from == 1) {
        if (get_asr_have_prompt()) {
            restore_system_volume(1);
            volume_pormpt_action(VOLUME_ATTENDING);
        }
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

#if defined(LGUPLUS_IPTV_ENABLE)
#if defined(PURE_RPC) || defined(COMMON_RPC)
    if (req_from == 4) {
        char buf[256];
        long begin_index;

        begin_index = asr_get_keyword_begin_index();

        snprintf(buf, sizeof(buf), "GNOTIFY=IPTV_WAKEUP:%ld:%ld", begin_index,
                 asr_get_keyword_end_index(2));
        SocketClientReadWriteMsg(RPC_LOCAL_SOCKET_PARTH, buf, strlen(buf), NULL, NULL, 0);
    }
#endif
#endif

    return 0;
}

static int do_talkstop(int errcode)
{
    if (debug_mode_on()) {
        set_asr_talk_start(0);
        return 0;
    }

    wiimu_log(1, 0, 0, 0, "do_talkstop ++++");
    CAIMan_ASR_disable();
    // TODO: change the volume tye for error
    if (errcode == ERR_ASR_NO_INTERNET) {
        volume_pormpt_action(VOLUME_A_ERROR);
        volume_pormpt_action(VOLUME_INTERNET_LOST);
    } else if (errcode == ERR_ASR_HAVE_PROMBLEM) {
        volume_pormpt_action(VOLUME_A_ERROR);
        volume_pormpt_action(VOLUME_INTERNET_LOST);
    }

    CAIMan_YD_enable();
    set_asr_talk_start(0);
    wiimu_log(1, 0, 0, 0, "do_talkstop ----");
    return 0;
}

static int do_talkcontinue(int errCode)
{
    int ret = -1;
    if (debug_mode_on()) {
        set_asr_talk_start(0);
        return -1;
    }

    if (asr_get_privacy_mode()) {
        do_talkstop(-1);
        return -1;
    }

    ret = do_talkstart_check();
    if (ret) {
        do_talkstop(ret);
        return -1;
    }

    if (errCode == ERR_ASR_CONTINUE) {
        common_focus_manage_set(e_focus_recognize);
        CAIMan_ASR_disable();
#if defined(LGUPLUS_IPTV_ENABLE)
        if (g_asr_req_from == 0 || g_asr_req_from == 1) {
            set_session_state(1);
            if (get_asr_have_prompt())
                volume_pormpt_action(VOLUME_ATTENDING);
            asr_session_record_buffer_reset();
        } else if (g_asr_req_from == 4)
            iptv_shm_reset();
#else
        if (get_asr_have_prompt())
            volume_pormpt_action(VOLUME_ATTENDING);
        if (g_asr_req_from != 3) {
            asr_session_record_buffer_reset();
        }
#endif
    } else {
        do_talkstop(-1);
        return -1;
    }

    if (g_asr_req_from != 3) {
        set_asr_talk_start(1);
#if defined(LGUPLUS_IPTV_ENABLE)
        if (g_asr_req_from == 0 || g_asr_req_from == 1)
            SocketClientReadWriteMsg(ASR_EVENT_PIPE, ASR_RECO_CONTINUE, strlen(ASR_RECO_CONTINUE),
                                     NULL, NULL, 0);
#else
        SocketClientReadWriteMsg(ASR_EVENT_PIPE, ASR_RECO_CONTINUE, strlen(ASR_RECO_CONTINUE), NULL,
                                 NULL, 0);
#endif
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

#ifdef LGUPLUS_IPTV_ENABLE
void *IPTVAgentThread(void *asrg)
{
    int pid;
    int status = 0;

    while (1) {
        if ((pid = fork()) < 0) {
            printf("IPTVAgentThread fork error...\n");
        } else if (pid == 0) {
            execlp("iptvagent", "iptvagent", NULL);
            printf("==========SHOULD NOT BE HERE===========\n");
            exit(-1);
        } else {
            printf("[%s-%d], iptvagent forked pid [%d].\n", __func__, __LINE__, pid);

            int quit_pid = waitpid(pid, &status, 0);

            printf("[%s-%d], iptvagent [%d] quit!!\n", __func__, __LINE__, quit_pid);
        }

        sleep(1);
    }
}
#endif

void *ButtonRelease(void *arg)
{
    long long time;

    for (;;) {
        time = tickSincePowerOn();

        if (time >= 60 * 1000) {
            release_button_block();
            break;
        }

        sleep(1);
    }

    pthread_exit(0);
}

int main(int argc, char **argv)
{

    signal(SIGINT, sigroutine);
    signal(SIGTERM, sigroutine);
    signal(SIGKILL, sigroutine);
    signal(SIGALRM, sigroutine);
    signal(SIGQUIT, sigroutine);

#ifdef LGUPLUS_IPTV_ENABLE
    pthread_t iptv_thread = 0;
    if (iptv_is_alive())
        killPidbyName("iptvagent");
    iptv_init(LINKPLAY_PLATFORM);
    pthread_create(&iptv_thread, NULL, IPTVAgentThread, NULL);

    if (!monitor_is_alive())
        system("sh /system/workdir/script/memory_monitor.sh &");
#endif

    pthread_t button_release_thread = 0;
    pthread_create(&button_release_thread, NULL, ButtonRelease, NULL);
    if (button_release_thread)
        pthread_detach(button_release_thread);

    common_flags_init();

    asr_init_cfg();
    alert_start();
    CAIMan_pre_init();
    CAIMan_init();
    CAIMan_YD_start();
    CAIMan_ASR_start();
    CAIMan_YD_enable();
    asr_session_init();

    do_cmd_parse();

    asr_session_uninit();
    common_falgs_uninit();

    return 0;
}
