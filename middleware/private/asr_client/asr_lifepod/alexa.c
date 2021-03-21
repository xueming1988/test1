#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>

#include "caiman.h"
#include "json.h"
#include "wm_util.h"

#include "cmd_q.h"
#include "cxdish_ctrol.h"
#include "ring_buffer.h"

#include "tick_timer.h"

#include "alexa.h"
#include "alexa_cfg.h"
#include "alexa_cmd.h"

#include "alexa_alert.h"
#include "alexa_debug.h"
#include "alexa_system.h"

#include "alexa_arbitration.h"
#include "alexa_authorization.h"
#include "alexa_bluetooth.h"
#include "alexa_emplayer.h"
#include "alexa_json_common.h"
#include "alexa_notification.h"
#include "alexa_result_parse.h"
#include "alexa_focus_mgmt.h"

#include "asr_event.h"
#include "avs_player.h"
#include "http2_client.h"
#include "asr_state.h"
#include "avs_publish_api.h"
#include "spotify_capability.h"

#ifdef EN_AVS_COMMS
#include "alexa_comms.h"
#endif

#ifdef AVS_MRM_ENABLE
#include "alexa_mrm.h"
#endif

#if !(defined(PLATFORM_AMLOGIC) || defined(PLATFORM_INGENIC) || defined(OPENWRT_AVS))
#include "nvram.h"
#endif

extern void switch_content_to_background(int val);
extern long long ASR_GetRecording_TimeOut(void);

extern int g_alexa_req_from;
unsigned int g_engr_voice = 0;
unsigned int g_engr_noise = 0;

static volatile int g_retry_cnt = 0;
extern int g_internet_access;
extern WIIMU_CONTEXT *g_wiimu_shm;

share_context *g_mplayer_shmem = NULL;
static ring_buffer_t *g_ring_buffer = NULL;

pthread_t avs_publish_api_pid = 1; // add by weiqiang.huang for STAB-264 2018-08-13

//---------------------------------------------------------
volatile static int g_is_cancel = 0;

const int g_retry_table[6] = {
    500,   // Retry 1:   0.50s
    1000,  // Retry 2:  1.00s
    3000,  // Retry 3:  3.00s
    5000,  // Retry 4:  5.00s
    10000, // Retry 5: 10.00s
    20000, // Retry 6: 20.00s
};

static int event_remove(void *reasion, void *cmd_dst);

static void alexa_need_change_boost(void)
{
    // if (CLOSE_TALK != AlexaProfileType()) {
    //  NOTIFY_CMD(ASR_TTS, CHANGE_BOOST);
    // }
}

static void alexa_need_talk_stop(int error_code)
{
    char buff[64] = {0};

    if (g_wiimu_shm->asr_ongoing == 0) {
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "talk to stop %d\n", error_code);
        snprintf(buff, sizeof(buff), TLK_STOP_N, error_code);
        NOTIFY_CMD(ASR_TTS, buff);
    }
}

#ifdef AVS_ARBITRATION
static int alexa_send_ReportESPData_event(unsigned int voiceEnergy, unsigned int ambientEnergy)
{
    int ret = -1;
    char *params = NULL;
    char *state_json = NULL;
    char *cmd_js_str = NULL;

    state_json = avs_player_state(0);
    Create_ESP_Params(params, voiceEnergy, ambientEnergy);
    if (params) {
        Create_Cmd(cmd_js_str, 0, NAMESPACE_SPEECHRECOGNIZER, NAME_REPORTECHOSPATIALPERCEPTIONDATA,
                   params);
        free(params);
    }

    ret = asr_event_exec(state_json, cmd_js_str, NULL, 10);

    if (state_json) {
        free(state_json);
    }

    if (cmd_js_str) {
        free(cmd_js_str);
    }

    return ret;
}

static int alexa_get_ReportESPDate(unsigned int *engr_voice, unsigned int *engr_noise)
{
    int ret = 0;

    ret = alexa_arbitration(ALEXA_WAKEWORD_FILE, engr_voice, engr_noise);
    if (ret < 0) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "alexa_get_ReportESPDate error\n");
    }
    return ret;
}
#endif

static int alexa_error_code(int http_ret)
{
    int ret = -1;

    switch (http_ret) {
    case E_ERROR_UNAUTH:
        ret = E_ERROR_OFFLINE;
        break;
    case E_ERROR_INIT:
    case E_ERROR_OFFLINE:
        ret = E_ERROR_OFFLINE;
        break;
    case E_ERROR_TIMEOUT:
        ret = E_ERROR_OFFLINE;
        break;
    case E_HTTPRET_200:
        ret = 0;
        break;
    case E_HTTPRET_204:
        ret = ERR_ASR_PROPMT_STOP;
        break;
    case E_HTTPRET_400:
        ret = ERR_ASR_HAVE_PROMBLEM;
        break;
    case E_HTTPRET_429:
        ret = ERR_ASR_HAVE_PROMBLEM;
        break;
    default:
        ret = ERR_ASR_HAVE_PROMBLEM;
        break;
    }

    return ret;
}

static int alexa_send_recognizer(void)
{
    int ret = -1;
    int time_out = 30;
    long long record_time_out = 0;
    char *state_json = NULL;
    char *cmd_js_str = NULL;

    // alexa_mode_switch_set(3);
    if (g_alexa_req_from == 2) {
        record_time_out = ASR_GetRecording_TimeOut();
        if (record_time_out == 40000) {
            time_out = 50;
        }
    }
    state_json = avs_player_state(g_alexa_req_from);
    Create_Cmd(cmd_js_str, 0, NAMESPACE_SPEECHRECOGNIZER, NAME_RECOGNIZE, Val_Obj(0));
    ret = asr_event_exec(state_json, cmd_js_str, (void *)g_ring_buffer, time_out);

    if (avs_player_get_state(e_asr_status)) {
        ret = ERR_ASR_CONTINUE;
    } else {
        ret = alexa_error_code(ret);
        avs_player_buffer_resume();
    }

    // alexa_mode_switch_set(2);
    if (state_json) {
        free(state_json);
    }

    if (cmd_js_str) {
        free(cmd_js_str);
    }

    return ret;
}

static int alexa_send_event(char *cmd_js_str)
{
    int ret = E_ERROR_INIT;
    char *state_json = NULL;

    if (!cmd_js_str) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "cmd_js_str is NULL\n");
        return ret;
    }

    state_json = avs_player_state(0);
    if (!g_wiimu_shm->slave_notified) {
        ret = asr_event_exec(state_json, cmd_js_str, NULL, 30);
    }

    if (state_json) {
        free(state_json);
    }

    return ret;
}

/*
 * The thread used to send the record data to AVS server
 */
void *alexa_rcognize_proc(void *arg)
{
    int ret = 0;

    g_wiimu_shm->asr_ongoing = 0;
    // alexa_need_to_alert_background();

    ret = alexa_send_recognizer();
    if (ret != ERR_ASR_CONTINUE && g_wiimu_shm->asr_ongoing == 0 && ret != E_ERROR_OFFLINE) {
        if (ret == ERR_ASR_USER_LOGIN_FAIL) {
#ifdef S11_EVB_EUFY_DOT
            NOTIFY_CMD(IO_GUARD, Alexa_LostInternet);
#else
            NOTIFY_CMD(IO_GUARD, TTS_ERROR);
#endif
            alexa_need_talk_stop(ERR_ASR_USER_LOGIN_FAIL);
        } else {
            int cid_playing = avs_player_get_state(e_tts_player);
            if (cid_playing == 0) {
                NOTIFY_CMD(IO_GUARD, TTS_IDLE);
                SetAudioInputProcessorObserverState(IDLE);
            }
            if (ret != 0) {
                alexa_need_talk_stop(ret);
            } else {
                alexa_need_talk_stop(0);
            }
#ifdef EN_AVS_COMMS
            NOTIFY_CMD(ALEXA_CMD_FD, EVENT_RESUME_RINGTONE);
#endif
        }
        alexa_need_change_boost();
    } else if (ret == E_ERROR_OFFLINE) {
        // wait for listening status end
        usleep(500000);
        alexa_need_talk_stop(0);
        NOTIFY_CMD(IO_GUARD, TTS_IDLE);
        SetAudioInputProcessorObserverState(IDLE);
    } else {
        int cid_playing = avs_talkcontinue_get_status();
        if (cid_playing == 1) {
            NOTIFY_CMD(ASR_TTS, TLK_CONTINUE);
        }
    }

//?? when to alert forground? should not here
// alexa_need_to_alert_forceground();
#ifdef EN_AVS_COMMS
    avs_comms_notify(CALL_MUTE_CTRL_0);
#endif

    g_wiimu_shm->asr_ongoing = 0;

    return 0; // pthread_exit(0);
}

/*
 * Create the rcognize request thread
 */
static int alexa_recognize_create(pthread_t *pid)
{
    int ret = 0;
    pthread_attr_t attr;
    struct sched_param param;

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "+++++start(%lld)\n", tickSincePowerOn());

    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    param.sched_priority = 80;
    pthread_attr_setschedparam(&attr, &param);

    ret = pthread_create(pid, &attr, alexa_rcognize_proc, NULL);
    if (ret) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "alexa_rcognize_proc failed\n");
        g_wiimu_shm->asr_ongoing = 0;
        alexa_need_talk_stop(ERR_ASR_PROPMT_STOP);
        *pid = 0;
    }
    pthread_attr_destroy(&attr);

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "-----end pid(%lu)(%lld)\n", *pid, tickSincePowerOn());

    return ret;
}

/*
 * Stop the rcognize request thread
 */
static int alexa_recognize_destroy(pthread_t *pid)
{
    int ret = -1;
    ALEXA_PRINT(ALEXA_DEBUG_INFO, "start\n");

    avs_player_buffer_stop(NULL, e_asr_start);
    if (*pid > 0) {
        ret = http2_conn_cancel_upload();
        if (ret != 0) {
            g_is_cancel = 1;
        }
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "+++++join(%lu) start(%lld)\n", *pid, tickSincePowerOn());
        pthread_join(*pid, NULL);
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "-----join end(%lld)\n", tickSincePowerOn());
        g_is_cancel = 0;
        *pid = 0;
    }

    avs_system_update_inactive_timer();
    return ret;
}

static void alexa_start_request(pthread_t *pid, int req_from)
{
    alexa_recognize_destroy(pid);

// stop curl readcallback that upload record data
#ifdef AVS_ARBITRATION
    int is_incall = 0;
#ifdef EN_AVS_COMMS
    is_incall = avs_comms_is_avtive();
#endif
    if (AWAKE_BY_WAKEWORD == req_from && !is_incall) {
        long long start_esp = tickSincePowerOn();
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "+++++ esp start at (%lld)\n", tickSincePowerOn());
        if (0 == alexa_get_ReportESPDate(&g_engr_voice, &g_engr_noise)) {
            ALEXA_PRINT(ALEXA_DEBUG_TRACE, "~~~~~ esp resut useT(%lld)\n",
                        tickSincePowerOn() - start_esp);
            /* add for arbitration api , by bujuan.ji */
            alexa_send_ReportESPData_event(g_engr_voice, g_engr_noise);
        }
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "+++++ esp end useT(%lld)\n",
                    tickSincePowerOn() - start_esp);
    }
#endif

    // start the recognize request
    alexa_recognize_create(pid);
}

void alexa_logout(pthread_t *pid)
{
    alexa_recognize_destroy(pid);

    avs_player_force_stop(1);
    LifepodAuthorizationLogOut(1);
    // asr_set_default_language(0);

    // if access_token exist
    if ((access("/vendor/wiimu/alexa_alarm", 0)) == 0) {
        // begin
        if ((access("/vendor/wiimu/alexa_alarm_NamedTimerAndReminder", 0)) == 0) {
            system("rm -rf /vendor/wiimu/alexa_alarm_NamedTimerAndReminder & sync");
        }
        if ((access("/tmp/NamedTimerAndReminder_Audio", 0)) == 0) {
            system("rm -rf /tmp/NamedTimerAndReminder_Audio & sync");
        }
        // end

        system("rm -rf /vendor/wiimu/alexa_alarm & sync");
        killPidbyName("alexa_alert");
        alert_reset();
        system("alexa_alert &");
    }
    avs_notificaion_clear();

    if (PLAYER_MODE_IS_ALEXA(g_wiimu_shm->play_mode) ||
        PLAYER_MODE_IS_SPOTIFY(g_wiimu_shm->play_mode)) {
#ifdef PURE_RPC
        NOTIFY_CMD(A01Controller, ALEXA_PLAY_LOGOUT); /* pure need clear metadata */
#else
        NOTIFY_CMD(A01Controller, ALEXA_PLAY_STOP);
#endif
    }

    // if (CLOSE_TALK != AlexaProfileType()) {
    //     cxdish_change_boost();
    //}
    switch_content_to_background(0);
    http2_conn_exit();
    g_wiimu_shm->asr_ongoing = 0;

    SocketClientReadWriteMsg("/tmp/RequestNetguard", "GNOTIFY=alexaLogOut",
                             strlen("GNOTIFY=alexaLogOut"), NULL, NULL, 0);

#if 0
    /*add by weiqiang.huang for STAB-264 2018-08-13 --begin*/
    //reset publish API record flag, should re-publish api again when re-login
#if !(defined(PLATFORM_AMLOGIC) || defined(PLATFORM_INGENIC) || defined(OPENWRT_AVS))
    nvram_set(RT2860_NVRAM, AVS_PUBLISH_API_STATUS, "0");
#else
    wm_db_set(AVS_PUBLISH_API_STATUS, "0");
#endif
    /*add by weiqiang.huang for STAB-264 2018-08-13 --end*/
#endif
    avs_emplayer_notify_logout();
}

void *alexa_comand_proc(void *arg)
{
    int recv_num;
    char *recv_buf = (char *)malloc(65536);

    int ret;
    pthread_t record_upload_pid = 0;

    SOCKET_CONTEXT msg_socket;
    msg_socket.path = "/tmp/AMAZON_ASRTTS";
    msg_socket.blocking = 1;

    cmd_q_t *evnet_cmd_q = (cmd_q_t *)arg;
    if (!recv_buf) {
        fprintf(stderr, "%s malloc failed\n", __FUNCTION__);
        exit(-1);
    }

RE_INIT:
    ret = UnixSocketServer_Init(&msg_socket);
    if (ret <= 0) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "ALEXA_ASR UnixSocketServer_Init fail\n");
        sleep(5);
        goto RE_INIT;
    }

    while (1) {
        ret = UnixSocketServer_accept(&msg_socket);
        if (ret <= 0) {
            ALEXA_PRINT(ALEXA_DEBUG_ERR, "ALEXA_ASR UnixSocketServer_accept fail\n");
            UnixSocketServer_deInit(&msg_socket);
            sleep(5);
            goto RE_INIT;
        } else {
            memset(recv_buf, 0x0, 65536);

            recv_num = UnixSocketServer_read(&msg_socket, recv_buf, 65536);
            if (recv_num <= 0) {
                ALEXA_PRINT(ALEXA_DEBUG_ERR, "ALEXA_ASR recv failed\r\n");
                if (recv_num < 0) {
                    UnixSocketServer_close(&msg_socket);
                    UnixSocketServer_deInit(&msg_socket);
                    sleep(5);
                    goto RE_INIT;
                }
            } else {
                recv_buf[recv_num] = '\0';
                if (!StrInclude(recv_buf, ASR_LOGIN) && !StrInclude(recv_buf, ASR_SET_ACCESS_TOKEN))
                    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "ALEXA_ASR start at %lld recv_buf %s ++++++\n",
                                tickSincePowerOn(), recv_buf);
                if (StrInclude(recv_buf, ASR_RECO_START)) {
                    int req_from = atoi(recv_buf + strlen(ASR_RECO_START) + 1);
                    cmd_q_remove(evnet_cmd_q, (void *)NAME_PLAYFAILED, event_remove);
                    alexa_start_request(&record_upload_pid, req_from);
                } else if (StrInclude(recv_buf, ASR_RECO_STOP)) {
                    if (record_upload_pid > 0) {
                        alexa_recognize_destroy(&record_upload_pid);
                        g_wiimu_shm->asr_session_start = 0;
                        NOTIFY_CMD(IO_GUARD, TTS_IDLE);
                        SetAudioInputProcessorObserverState(IDLE);
                        alexa_need_talk_stop(ERR_ASR_PROPMT_STOP);
                    }
                    g_wiimu_shm->asr_ongoing = 0;
                } else if (StrInclude(recv_buf, ASR_IS_LOGIN)) {
                    int login = 0;
                    if ((access(LIFEPOD_TOKEN_PATH, F_OK)) == 0) {
                        FILE *_file = fopen(LIFEPOD_TOKEN_PATH, "rb");
                        if (_file) {
                            fseek(_file, 0, SEEK_END);
                            if (ftell(_file) > 10)
                                login = 1;
                            fclose(_file);
                        }
                    }
                    if (login)
                        UnixSocketServer_write(&msg_socket, "1", 1);
                    else
                        UnixSocketServer_write(&msg_socket, "0", 1);
#ifdef LIFEPOD_CLOUD
                } else if (StrInclude(recv_buf, ASR_SET_ACCESS_TOKEN)) {
                    char *cmd = recv_buf + strlen(ASR_SET_ACCESS_TOKEN);
                    if (cmd && strlen(cmd) > 0) {
                        LifepodAuthorizationLogin(cmd);
                    }
#endif
#ifdef ASR_3PDA_ENABLE
                } else if (StrInclude(recv_buf, ASR_AMAZON_OAUTHCODE)) {
                    char *cmd = recv_buf + strlen(ASR_AMAZON_OAUTHCODE);
                    if (cmd && strlen(cmd) > 0) {
                        alexa_logout(&record_upload_pid);
                        printf("cmd is %s \r\n", cmd);
                        ret = AlexaLogInWithParam(cmd);
                        if (ret == 0) {
                            UnixSocketServer_write(&msg_socket, CMD_OK, strlen(CMD_OK));
                        } else if (ret == -2) {
                            UnixSocketServer_write(&msg_socket, CMD_CFG_FAILED,
                                                   strlen(CMD_CFG_FAILED));
                        } else {
                            UnixSocketServer_write(&msg_socket, CMD_FAILED, strlen(CMD_FAILED));
                        }
                    } else {
                        UnixSocketServer_write(&msg_socket, CMD_FAILED, strlen(CMD_FAILED));
                    }
                } else if (StrInclude(recv_buf, ASR_PCO_ENABLE_SKILL)) {
                    char *cmd = recv_buf + strlen(ASR_PCO_ENABLE_SKILL);
                    if (cmd && strlen(cmd) > 0) {
                        printf("cmd is %s \r\n", cmd);
                        ret = pco_enable_skill(cmd);
                        if (ret == 0) {
                            UnixSocketServer_write(&msg_socket, CMD_OK, strlen(CMD_OK));
                        } else if (ret == -2) {
                            UnixSocketServer_write(&msg_socket, CMD_CFG_FAILED,
                                                   strlen(CMD_CFG_FAILED));
                        } else {
                            UnixSocketServer_write(&msg_socket, CMD_FAILED, strlen(CMD_FAILED));
                        }
                    } else {
                        UnixSocketServer_write(&msg_socket, CMD_FAILED, strlen(CMD_FAILED));
                    }
                } else if (StrInclude(recv_buf, ASR_GET_PCO_SKILL_STATUS)) {
                    char *cmd = recv_buf + strlen(ASR_GET_PCO_SKILL_STATUS);
                    if (cmd && strlen(cmd) > 0) {
                        printf("cmd is %s \r\n", cmd);
                        ret = pco_get_skill_enable_status(cmd);
                        if (ret == 0) {
                            UnixSocketServer_write(&msg_socket, CMD_OK, strlen(CMD_OK));
                        } else if (ret == -2) {
                            UnixSocketServer_write(&msg_socket, CMD_CFG_FAILED,
                                                   strlen(CMD_CFG_FAILED));
                        } else {
                            UnixSocketServer_write(&msg_socket, CMD_FAILED, strlen(CMD_FAILED));
                        }
                    } else {
                        UnixSocketServer_write(&msg_socket, CMD_FAILED, strlen(CMD_FAILED));
                    }
                } else if (StrInclude(recv_buf, ASR_PCO_DISABLE_SKILL)) {
                    char *cmd = recv_buf + strlen(ASR_PCO_DISABLE_SKILL);
                    if (cmd && strlen(cmd) > 0) {
                        printf("cmd is %s \r\n", cmd);
                        ret = pco_disable_skill(cmd);
                        if (ret == 0) {
                            UnixSocketServer_write(&msg_socket, CMD_OK, strlen(CMD_OK));
                        } else if (ret == -2) {
                            UnixSocketServer_write(&msg_socket, CMD_CFG_FAILED,
                                                   strlen(CMD_CFG_FAILED));
                        } else {
                            UnixSocketServer_write(&msg_socket, CMD_FAILED, strlen(CMD_FAILED));
                        }
                    } else {
                        UnixSocketServer_write(&msg_socket, CMD_FAILED, strlen(CMD_FAILED));
                    }
#endif
                } else if (StrInclude(recv_buf, ASR_SET_TOKEN_PARAM)) {
#ifdef S11_EVB_EUFY_DOT
                    g_wiimu_shm->eufy_alexa_settoken = 0;
#endif
                    // init the amazon accesstoken, read from the file ALEXA_TOKEN_PATH
                    char *cmd = recv_buf + strlen(ASR_SET_TOKEN_PARAM);
                    if (cmd && strlen(cmd) > 0) {
                        alexa_logout(&record_upload_pid);
                        printf("cmd is %s \r\n", cmd);
                        ret = LifepodLogInWithAuthcode(cmd);
                        if (ret == 0) {
#ifdef A98_SPRINT
                            SocketClientReadWriteMsg("/tmp/RequestASRTTS", "talksetEnableMic",
                                                     strlen("talksetEnableMic"), NULL, NULL, 0);
#endif
#ifdef S11_EVB_EUFY_DOT
                            g_wiimu_shm->eufy_alexa_settoken = 1;
#endif
#ifdef AVS_MRM_ENABLE
                            if (!AlexaDisableMrm()) {
                                /* notify mrm to update registration state */
                                SocketClientReadWriteMsg(
                                    "/tmp/alexa_mrm", "GNOTIFY=AlexaRegistrationStatus:1",
                                    strlen("GNOTIFY=AlexaRegistrationStatus:1"), NULL, NULL, 1);
                            }
#endif
                            UnixSocketServer_write(&msg_socket, CMD_OK, strlen(CMD_OK));
                            /*add by weiqiang.huang for STAB-264 2018-08-13 -- begin*/
                            if (avs_publish_api_pid == 0) {
                                // release supported API version to AVS server
                                pthread_create(&avs_publish_api_pid, NULL,
                                               alexa_publish_supported_api_proc, NULL);
                                ALEXA_PRINT(ALEXA_DEBUG_TRACE,
                                            "publish api thread created : %ld \n",
                                            avs_publish_api_pid);
                            } else {
                                ALEXA_PRINT(ALEXA_DEBUG_TRACE,
                                            "publish api thread already exist: %ld \n",
                                            avs_publish_api_pid);
                            }
                        } else if (ret == -2) {
                            UnixSocketServer_write(&msg_socket, CMD_CFG_FAILED,
                                                   strlen(CMD_CFG_FAILED));
                        } else {
                            UnixSocketServer_write(&msg_socket, CMD_FAILED, strlen(CMD_FAILED));
                        }
                    } else {
                        UnixSocketServer_write(&msg_socket, CMD_FAILED, strlen(CMD_FAILED));
                    }
                } else if (StrInclude(recv_buf, ASR_MODE_SWITCH)) {
                    avs_player_buffer_stop(NULL, e_asr_stop);
                    avs_player_force_stop(1);
                } else if (StrInclude(recv_buf, ASR_LOGOUT)) {
                    avs_system_clear_soft_version_flag();
                    avs_player_buffer_stop(NULL, e_asr_stop);
                    if (record_upload_pid > 0) {
                        NOTIFY_CMD(IO_GUARD, TTS_IDLE);
                        SetAudioInputProcessorObserverState(IDLE);
                        alexa_need_talk_stop(ERR_ASR_PROPMT_STOP);
                    }
                    alexa_logout(&record_upload_pid);
#ifdef PURE_RPC /* pure avs.user */
                    SocketClientReadWriteMsg(RPC_LOCAL_SOCKET_PARTH, "GNOTIFY=USER_STAT:0",
                                             strlen("GNOTIFY=USER_STAT:0"), NULL, NULL, 0);
#endif
#ifdef EN_AVS_COMMS
                    avs_comms_notify(REPORT_ALEXA_LOGOUT);
#endif
#ifdef A98_SPRINT
                    SocketClientReadWriteMsg("/tmp/RequestASRTTS", "talksetDisableMic",
                                             strlen("talksetDisableMic"), NULL, NULL, 0);
#endif
                    UnixSocketServer_write(&msg_socket, CMD_OK, strlen(CMD_OK));

#ifdef AVS_MRM_ENABLE
                    if (!AlexaDisableMrm()) {
                        /* notify mrm to update registration state */
                        SocketClientReadWriteMsg(
                            "/tmp/alexa_mrm", "GNOTIFY=AlexaRegistrationStatus:0",
                            strlen("GNOTIFY=AlexaRegistrationStatus:0"), NULL, NULL, 1);
                    }
#endif
                } else if (StrInclude(recv_buf, ASR_POWEROFF)) {
                    avs_player_buffer_stop(NULL, e_asr_stop);
                    avs_player_force_stop(1);
                    switch_content_to_background(0);
                    LifepodAuthorizationLogOut(0);
                    avs_system_clear_soft_version_flag();
                    http2_conn_exit();
                    g_wiimu_shm->asr_ongoing = 0;
                    g_wiimu_shm->asr_pause_request = 0;
                    UnixSocketServer_write(&msg_socket, CMD_OK, strlen(CMD_OK));
                } else if (StrInclude(recv_buf, ASR_DISCONNECT)) {
                    http2_conn_exit();
                    UnixSocketServer_write(&msg_socket, CMD_OK, strlen(CMD_OK));
                }
// else if (StrInclude(recv_buf, CHANGE_BOOST))
//{
//    if (CLOSE_TALK != AlexaProfileType()) {
//        cxdish_change_boost();
//    }
//}
#ifdef EN_AVS_COMMS
                else if (StrInclude(recv_buf, "comms_set_alert_background")) {
                    // alexa_need_to_alert_background();
                } else if (StrInclude(recv_buf, "comms_set_alert_foreground")) {
                    // alexa_need_to_alert_forceground();
                } else if (StrInclude(recv_buf, CALL_CHECK_NET)) {
                    if (g_internet_access) {
                        avs_comms_notify(CALL_NET_STATE_1);
                    } else {
                        avs_comms_notify(CALL_NET_STATE_0);
                    }
                } else if (StrInclude(recv_buf, CALL_CHECK_CONN)) {
                    if (alexa_dump_info.on_line) {
                        avs_comms_notify(CALL_CONN_STATE_1);
                    } else {
                        avs_comms_notify(CALL_CONN_STATE_0);
                    }
                }
#endif
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "ALEXA_ASR end at %lld ------\n",
                            tickSincePowerOn());
            }

            UnixSocketServer_close(&msg_socket);
        }
    }

    pthread_exit(0);
}

void *alexa_down_channel_proc(void *arg)
{
    // Verify that when the device is unable to connect to the AVS webservice,
    // the device does not try to reconnect more than 50 times in 10 minutes,
    // using an appropriate exponential backoff method.
    int err = 0;
    struct timeval tv;
    int retry_delay_ms = 0;

    while (1) {
        char *alexa_access_token = NULL;

        alexa_dump_info.on_line = 0;
        alexa_access_token = LifepodGetAccessToken();
        if (alexa_access_token) {
            int ret = asr_event_down_channel();
#ifdef EN_AVS_COMMS
            avs_comms_state_change(e_state_online, e_off);
#endif
#ifdef AVS_MRM_ENABLE
            if (!AlexaDisableMrm()) {
                /* notify avs mrm to update avs connection */
                SocketClientReadWriteMsg("/tmp/alexa_mrm", "GNOTIFY=AlexaConnectionStatus:0",
                                         strlen("GNOTIFY=AlexaConnectionStatus:0"), NULL, NULL, 1);
            }
#endif
            if (ret != 0) {
                if (g_retry_cnt > 5) {
                    g_retry_cnt = 0;
                }
                retry_delay_ms = g_retry_table[g_retry_cnt];
                tv.tv_sec = retry_delay_ms / 1000;
                tv.tv_usec = retry_delay_ms % 1000 * 1000;
                g_retry_cnt++;
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "retry_time_s = %d retry_cnt=%d\n", retry_delay_ms,
                            g_retry_cnt);
            } else {
                g_retry_cnt = 0;
                retry_delay_ms = 1000;
                tv.tv_sec = retry_delay_ms / 1000;
                tv.tv_usec = retry_delay_ms % 1000 * 1000;
            }
        } else {
            g_retry_cnt = 0;
            retry_delay_ms = 1000;
            tv.tv_sec = retry_delay_ms / 1000;
            tv.tv_usec = retry_delay_ms % 1000 * 1000;
        }

        alexa_dump_info.on_line = 0;
        g_wiimu_shm->alexa_online = 0;
        if (alexa_access_token) {
            free(alexa_access_token);
        }

        do {
            err = select(0, NULL, NULL, NULL, &tv);
        } while (err < 0 && errno == EINTR);
    }

    return 0; // pthread_exit(0);
}

int alexa_get_tts_status(void)
{
    int tts_playing = 0;
    int tts_continue = 0;

    tts_continue = avs_player_get_state(e_asr_status);
    tts_playing = avs_player_get_state(e_tts_player);
    if (tts_continue || tts_playing) {
        return 1;
    }

    return g_wiimu_shm->TTSPlayStatus &&
           tts_playing; /*modify by weiqiang.huang for CER-24 2017-0713 */
}

void alexa_book_ctrol(int pause_flag) { avs_player_book_ctrol(pause_flag); }

static int event_remove(void *reasion, void *cmd_dst)
{
    int ret = -1;

    if (cmd_dst && reasion) {
        ret = asr_event_findby_name((char *)cmd_dst, (char *)reasion);
    }

    return ret;
}

static int cmd_add_alert_event(cmd_q_t *evnet_cmd_q, char *name, char *alert_token)
{
    int ret = -1;

    if (evnet_cmd_q && name && alert_token) {
        if (StrEq(name, NAME_ALERT_STARTED)) {
            // todo bad place
            focus_mgmt_activity_status_set(Alerts, FOCUS);
        } else if (StrEq(name, NAME_ALERT_STOPPED)) {
            focus_mgmt_activity_status_set(Alerts, UNFOCUS);
        }
        char *local_cmd = create_alerts_cmd(0, name, alert_token);
        if (local_cmd) {
            cmd_q_add_tail(evnet_cmd_q, local_cmd, strlen(local_cmd));
            free(local_cmd);
            ret = 0;
        }
    }

    return ret;
}

static int cmd_add_speeker_event(cmd_q_t *evnet_cmd_q, char *name, int volume, int is_muted)
{
    int ret = -1;

    if (evnet_cmd_q && name) {
        char *local_cmd = create_speeker_cmd(0, name, volume, is_muted);
        if (local_cmd) {
            cmd_q_remove(evnet_cmd_q, (void *)NAME_VOLUMECHANGED, event_remove);
            cmd_q_add_tail(evnet_cmd_q, local_cmd, strlen(local_cmd));
            free(local_cmd);
            ret = 0;
        }
    }

    return ret;
}

static int cmd_add_playback_event(cmd_q_t *evnet_cmd_q, char *name)
{
    int ret = -1;

    if (evnet_cmd_q && name) {
        char *local_cmd = create_playback_cmd(0, name);
        if (local_cmd) {
            cmd_q_add_tail(evnet_cmd_q, local_cmd, strlen(local_cmd));
            free(local_cmd);
            ret = 0;
        }
    }

    return ret;
}

static int cmd_add_system_cmd(cmd_q_t *evnet_cmd_q, char *name, char *location)
{
    int ret = -1;

    if (evnet_cmd_q && name) {
        char *local_cmd = create_system_cmd(0, name, 0, location, NULL);
        if (local_cmd) {
            cmd_q_add_tail(evnet_cmd_q, local_cmd, strlen(local_cmd));
            free(local_cmd);
            ret = 0;
        }
    }

    return ret;
}

void *avs_msg_parse(void *ctx)
{
    cmd_q_t *evnet_cmd_q = (cmd_q_t *)ctx;
    int rv = 0;
    int to_cnt = 0;
    int last_send = 0;

    while (1) {
        char *cmd = NULL;
        if (alexa_dump_info.on_line == 1) {
            cmd = (char *)cmd_q_pop(evnet_cmd_q);
        }
        if (cmd) {
            // ALEXA_PRINT(ALEXA_DEBUG_ERR, "cmd:\n%s\n", cmd);
            int ret = alexa_send_event(cmd);
            if (ret >= E_HTTPRET_200 && ret < E_HTTPRET_300) {
                int flags = avs_system_get_changeendpoint();
                if (ret == E_HTTPRET_204 || flags == 0) {
                    rv = asr_event_findby_name(cmd, NAME_SYNCHORNIZESTATE);
                    if (rv == 0) {
                        asr_event_sync_state();
                    }
                }
                to_cnt = 0;
                last_send = 0;
            } else {
                if (ret >= E_HTTPRET_400 && ret < E_HTTPRET_500) {
                    if (ret == E_HTTPRET_403) {
                        cmd_q_clear(evnet_cmd_q);
                        switch_content_to_background(0);
                        LifepodAuthorizationLogOut(0);
                        avs_system_clear_soft_version_flag();
                        http2_conn_exit();
                        g_wiimu_shm->asr_ongoing = 0;
                        g_wiimu_shm->asr_pause_request = 0;
                    }
                } else if (ret >= E_HTTPRET_500) {
                    // TODO:
                }
                if (ret == E_ERROR_TIMEOUT || ret == E_ERROR_OFFLINE) {
                    rv = asr_event_findby_name(cmd, NAME_PLAYNEARLYFIN);
                    if (rv == 0) {
                        to_cnt++;
                        if (last_send == 0) {
                            cmd_q_add(evnet_cmd_q, cmd, strlen(cmd));
                        } else {
                            last_send = 0;
                        }
                        if (to_cnt >= 2) {
                            last_send = 1;
                            to_cnt = 0;
                            ALEXA_PRINT(ALEXA_DEBUG_ERR, "!!!!!!!try two times to get next URL\n");
                        }
                    } else {
                        to_cnt = 0;
                        last_send = 0;
                    }
                } else {
                    to_cnt = 0;
                    last_send = 0;
                }
                usleep(20000);
            }
            free(cmd);
        } else {
            usleep(20000);
        }
    }
}

static void alexa_remove_old_state_event(cmd_q_t *evnet_cmd_q, char *recv_buf)
{
    int rv = -1;

    rv = asr_event_findby_name(recv_buf, NAME_STOPPED);
    if (rv == 0) {
        cmd_q_remove(evnet_cmd_q, (void *)NAME_PLAYREPORTDELAY, event_remove);
        cmd_q_remove(evnet_cmd_q, (void *)NAME_PLAYREPORTINTERVAL, event_remove);
        focus_mgmt_activity_status_set(AudioPlayer, UNFOCUS);
    }
    rv = asr_event_findby_name(recv_buf, NAME_PLAYSTARTED);
    if (rv == 0) {
        cmd_q_remove(evnet_cmd_q, (void *)NAME_PLAYREPORTDELAY, event_remove);
        cmd_q_remove(evnet_cmd_q, (void *)NAME_PLAYREPORTINTERVAL, event_remove);
        focus_mgmt_activity_status_set(AudioPlayer, FOCUS);
    }
    rv = asr_event_findby_name(recv_buf, NAME_REPORTDONOTDISTURB);
    if (rv == 0) {
        cmd_q_remove(evnet_cmd_q, (void *)NAME_REPORTDONOTDISTURB, event_remove);
    }
    rv = asr_event_findby_name(recv_buf, NAME_SYNCHORNIZESTATE);
    if (rv == 0) {
        cmd_q_remove(evnet_cmd_q, (void *)NAME_SYNCHORNIZESTATE, event_remove);
    }
    rv = asr_event_findby_name(recv_buf, NAME_SettingUpdated);
    if (rv == 0) {
        cmd_q_remove(evnet_cmd_q, (void *)NAME_SettingUpdated, event_remove);
    }
    rv = asr_event_findby_name(recv_buf, NAME_VOLUMECHANGED);
    if (rv == 0) {
        cmd_q_remove(evnet_cmd_q, (void *)NAME_VOLUMECHANGED, event_remove);
    }
}

void *alexa_msg_recv(void *ctx)
{
    int ret = 0;
    int init_ok = 0;
    int recv_num = 0;
    char *recv_buf = (char *)malloc(65536);
    char alert_active_token[512] = {0};
    size_t alert_token_len = 0;

    cmd_q_t *evnet_cmd_q = (cmd_q_t *)ctx;

    SOCKET_CONTEXT msg_socket;
    msg_socket.path = (char *)ALEXA_CMD_FD;
    msg_socket.blocking = 1;
    if (!recv_buf) {
        fprintf(stderr, "%s malloc failed\n", __FUNCTION__);
        exit(-1);
    }
RE_INIT:
    ret = UnixSocketServer_Init(&msg_socket);
    if (ret <= 0) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "ALEXA_EVENT UnixSocketServer_Init fail\n");
        sleep(5);
        goto RE_INIT;
    }

    if (init_ok == 0) {
        init_ok = 1;
#ifdef EN_AVS_BLE
        avs_bluetooth_getpaired_list();
#endif
    }

    while (1) {
        ret = UnixSocketServer_accept(&msg_socket);
        if (ret <= 0) {
            ALEXA_PRINT(ALEXA_DEBUG_ERR, "ALEXA_EVENT UnixSocketServer_accept fail, ret=%d\n", ret);
            UnixSocketServer_deInit(&msg_socket);
            sleep(5);
            goto RE_INIT;
        } else {
            memset(recv_buf, 0x0, 65536);

            recv_num = UnixSocketServer_read(&msg_socket, recv_buf, 65536);
            if (recv_num <= 0) {
                ALEXA_PRINT(ALEXA_DEBUG_ERR, "ALEXA_EVENT recv failed\r\n");
                UnixSocketServer_close(&msg_socket);
                UnixSocketServer_deInit(&msg_socket);
                sleep(5);
                goto RE_INIT;
            } else {
                recv_buf[recv_num] = '\0';
                // ALEXA_PRINT(ALEXA_DEBUG_TRACE, "recv_buf:\n%s\n", recv_buf);
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "ALEXA_EVENT %.80s at %lld ++++++++\n", recv_buf,
                            tickSincePowerOn());

                if (0 == asr_event_cmd_type(recv_buf, NULL)) {
                    // before insert an new event, check if it need to remove some old events
                    if (1 == asr_event_cmd_type(recv_buf, (char *)Val_emplayer)) {
#ifdef EN_AVS_BLE
                    } else if (1 == asr_event_cmd_type(recv_buf, (char *)Val_bluetooth)) {

                        int rv = asr_event_findby_name(recv_buf, NAME_ScanDevicesUpdated);
                        if (rv == 0) {
                            cmd_q_remove(evnet_cmd_q, (void *)NAME_ScanDevicesUpdated,
                                         event_remove);
                        }
                        char *local_cmd = avs_bluetooth_parse_cmd(recv_buf);
                        if (local_cmd) {
                            if (g_wiimu_shm->alexa_online == 1) {
                                cmd_q_add_tail(evnet_cmd_q, local_cmd, strlen(local_cmd));
                                free(local_cmd);
                            }
                        }
                        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "bluetooth_cmd:\n%s\n", recv_buf);
#endif
#ifdef AVS_MRM_ENABLE
                    } else if (1 == asr_event_cmd_type(recv_buf, Val_avs_mrm)) {
                        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "avs_mrm_cmd:\n%.100s\n", recv_buf);
                        avs_mrm_parse_cmd(recv_buf);
#endif
#ifdef EN_AVS_COMMS
                    } else if (1 == asr_event_cmd_type(recv_buf, (char *)Val_sip_phone)) {
                        char *local_cmd = avs_comms_parse_cmd(recv_buf);
                        if (local_cmd) {
                            cmd_q_add_tail(evnet_cmd_q, local_cmd, strlen(local_cmd));
                            free(local_cmd);
                        }
#endif
                    } else {
                        // ALEXA_PRINT(ALEXA_DEBUG_TRACE, "parse the message OK\n");

                        alexa_remove_old_state_event(evnet_cmd_q, recv_buf);
                        if (!asr_event_findby_name(recv_buf, NAME_SYNCHORNIZESTATE)) {
                            cmd_q_add(evnet_cmd_q, recv_buf, strlen(recv_buf));
                        } else {
                            cmd_q_add_tail(evnet_cmd_q, recv_buf, strlen(recv_buf));
                        }
                    }
                } else {
                    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "the old cmd is: %s ------\n", recv_buf);
                    if (StrInclude(recv_buf, EVENT_ALERT_START)) {
                        alert_token_len = strlen(recv_buf) - strlen(EVENT_ALERT_START) - 1 /*:*/;
                        if (alert_token_len > 0) {
                            char *alert_token =
                                (char *)recv_buf + strlen(EVENT_ALERT_START) + 1 /*:*/;
                            memset(alert_active_token, 0x0, sizeof(alert_active_token));
                            if (alert_token_len > sizeof(alert_active_token)) {
                                alert_token_len = sizeof(alert_active_token);
                            }
                            strncpy(alert_active_token, alert_token, alert_token_len);

                            cmd_add_alert_event(evnet_cmd_q, NAME_ALERT_STARTED, alert_token);

                            if (alexa_get_tts_status()) {
                                cmd_add_alert_event(evnet_cmd_q, NAME_ALERT_BACKGROUND,
                                                    alert_token);
                            } else {
                                cmd_add_alert_event(evnet_cmd_q, NAME_ALERT_FOREGROUND,
                                                    alert_token);
                            }
                        }
                        /* {alexaAlertStop} stop current alert by say "delete alert / stop alert" or
                         * stop by APP */
                    } else if (StrInclude(recv_buf, EVENT_ALERT_STOP)) {
                        size_t token_len = strlen(recv_buf) - strlen(EVENT_ALERT_STOP) - 1 /*:*/;
                        if (token_len > 0) {
                            char *alert_token =
                                (char *)recv_buf + strlen(EVENT_ALERT_STOP) + 1 /*:*/;
                            cmd_add_alert_event(evnet_cmd_q, NAME_ALERT_STOPPED, alert_token);
                        }
                        /* {alexaSetAlertSucessed} when set alert ok must send alert set sucess
                         * event to AVS server */
                    } else if (StrInclude(recv_buf, EVENT_SET_ALERT_OK)) {
                        size_t token_len = strlen(recv_buf) - strlen(EVENT_SET_ALERT_OK) - 1 /*:*/;
                        if (token_len > 0) {
                            char *alert_token = recv_buf + strlen(EVENT_SET_ALERT_OK) + 1 /*:*/;
                            cmd_add_alert_event(evnet_cmd_q, NAME_SETALERT_SUCCEEDED, alert_token);
                        }

                        /* {alexaDelAlertSucessed} when del alert ok must send alert del sucess
                         * event to AVS server */
                    } else if (StrInclude(recv_buf, EVENT_SET_ALERT_ERR)) {
                        size_t token_len = strlen(recv_buf) - strlen(EVENT_SET_ALERT_ERR) - 1 /*:*/;
                        if (token_len > 0) {
                            char *alert_token = recv_buf + strlen(EVENT_SET_ALERT_ERR) + 1 /*:*/;
                            cmd_add_alert_event(evnet_cmd_q, NAME_SETALERT_FAILED, alert_token);
                        }
                    } else if (StrInclude(recv_buf, EVENT_DEL_ALERT_OK)) {
                        size_t token_len = strlen(recv_buf) - strlen(EVENT_DEL_ALERT_OK) - 1 /*:*/;
                        if (token_len > 0) {
                            char *alert_token = recv_buf + strlen(EVENT_DEL_ALERT_OK) + 1 /*:*/;
                            cmd_add_alert_event(evnet_cmd_q, NAME_DELETE_ALERT_SUCCEEDED,
                                                alert_token);
                        }
                        /* {alexaAlertForceground} when a alert start you need send an event to tell
                         * AVS server the
                         * alert is in forceground */
                    } else if (StrInclude(recv_buf, EVENT_ALERT_FORCEGROUND)) {
                        if (alert_token_len > 0) {
                            cmd_add_alert_event(evnet_cmd_q, NAME_ALERT_FOREGROUND,
                                                alert_active_token);
                        }
                        /* {alexaAlertForceground} when you have to upload record data to server you
                         * must tell AVS
                         * server the alert is in backround */
                    } else if (StrInclude(recv_buf, EVENT_ALERT_BACKGROUND)) {
                        if (alert_token_len > 0) {
                            cmd_add_alert_event(evnet_cmd_q, NAME_ALERT_BACKGROUND,
                                                alert_active_token);
                        }

                        /* {alexaDelAlertLocal} when del alert ok must send alert del sucess event
                         * to AVS server */
                    } else if (StrInclude(recv_buf, EVENT_ALERT_LOCAL_DELETE)) {
                        alert_token_len =
                            strlen(recv_buf) - strlen(EVENT_ALERT_LOCAL_DELETE) - 1 /*:*/;
                        if (alert_token_len > 0) {
                            memset(alert_active_token, 0, sizeof(alert_active_token));
                            strncpy(alert_active_token,
                                    recv_buf + strlen(EVENT_ALERT_LOCAL_DELETE) + 1 /*:*/,
                                    alert_token_len);
                            cmd_add_alert_event(evnet_cmd_q, NAME_ALERT_STOPPED,
                                                alert_active_token);
                            cmd_add_alert_event(evnet_cmd_q, NAME_DELETE_ALERT_SUCCEEDED,
                                                alert_active_token);
#ifdef EN_AVS_COMMS
                            avs_comms_resume_ux();
#endif
                        }

                        /* {alexaVolumeChanged} when the volume changed, must send event to AVS
                         * server */
                    } else if (StrInclude(recv_buf, EVENT_VOLUME_CHANGED)) {
                        int volume = 0;
                        if (strlen(recv_buf) != strlen(EVENT_VOLUME_CHANGED)) {
                            volume = atoi(recv_buf + strlen(EVENT_VOLUME_CHANGED) + 1 /*:*/);
                        } else {
                            volume = g_wiimu_shm->volume;
                        }

#ifdef AVS_MRM_ENABLE
                        if (!AlexaDisableMrm()) {
                            /* notify avs mrm of volume change */
                            SocketClientReadWriteMsg("/tmp/alexa_mrm", "VolumeChange",
                                                     strlen("VolumeChange"), NULL, NULL, 1);
                        }
#endif
                        avs_system_update_inactive_timer();
                        cmd_add_speeker_event(evnet_cmd_q, NAME_VOLUMECHANGED, volume, 0);
                        /* {alexaMuteChanged} when the mute changed, must send event to AVS server
                         */
                    } else if (StrInclude(recv_buf, EVENT_MUTE_CHANGED)) {
                        int volume = 0;
                        int muted = 0;
                        if (strlen(recv_buf) != strlen(EVENT_MUTE_CHANGED)) {
                            muted = atoi(recv_buf + strlen(EVENT_MUTE_CHANGED) + 1 /*:*/);
                        } else {
                            muted = g_wiimu_shm->mute;
                        }
                        volume = g_wiimu_shm->volume;
                        // Fix pandora not stopped for 8 hours playing issue, server send unmute
                        // directive after
                        // playback failed avs_system_update_inactive_timer();
                        cmd_add_speeker_event(evnet_cmd_q, NAME_MUTECHANGED, volume, muted);
                        /* {alexaPlayNext} send play next event, by the next buttom is pushed */
                    } else if (StrInclude(recv_buf, EVENT_PLAY_NEXT)) {
                        avs_system_update_inactive_timer();
                        cmd_add_playback_event(evnet_cmd_q, NAME_NEXTCOMMANDISSUED);

                        /* {alexaPlayPrev} send play prev event, by the prev buttom is pushed */
                    } else if (StrInclude(recv_buf, EVENT_PLAY_PREV)) {
                        avs_system_update_inactive_timer();
                        cmd_add_playback_event(evnet_cmd_q, NAME_PREVIOUSCOMMANDISSUED);
                    }
                    /* {alexaPlayPause} send play pause event, by the pause buttom is pushed */
                    else if (StrInclude(recv_buf, EVENT_PLAY_PAUSE)) {
                        avs_system_update_inactive_timer();
                        // cmd_add_playback_event(evnet_cmd_q, NAME_PAUSECOMMANDISSUED);
                        avs_player_ctrol(1);

                        /* {alexaPlayPlay} send play play event, by the pause buttom is pushed */
                    } else if (StrInclude(recv_buf, EVENT_PLAY_PLAY)) {
                        avs_system_update_inactive_timer();
                        cmd_add_playback_event(evnet_cmd_q, NAME_PLAYCOMMANDISSUED);
                    } else if (StrInclude(recv_buf, EVENT_LOCAL_UPDATE)) {
                        int ret = -1;
                        size_t message_len =
                            strlen(recv_buf) - strlen(EVENT_LOCAL_UPDATE) - 1 /*:*/;
                        if (message_len > 0) {
                            char *location = (char *)(recv_buf + strlen(EVENT_LOCAL_UPDATE) + 1);
                            ret = asr_set_language(location);
                            if (1 == alexa_dump_info.on_line) {
                                ret =
                                    cmd_add_system_cmd(evnet_cmd_q, NAME_LocalesChanged, location);
                                if (0 != ret) {
                                    if (strcmp(g_wiimu_shm->language, "Ger_de") == 0 ||
                                        strcmp(g_wiimu_shm->language, "de-DE") == 0)
                                        asr_set_language(LOCALE_DE);
                                    else if (strcmp(g_wiimu_shm->language, "en_uk") == 0 ||
                                             strcmp(g_wiimu_shm->language, "en-GB") == 0)
                                        asr_set_language(LOCALE_UK);
                                    else if (strcmp(g_wiimu_shm->language, "en_in") == 0 ||
                                             strcmp(g_wiimu_shm->language, "en-IN") == 0)
                                        asr_set_language(LOCALE_IN);
                                    else if (strcmp(g_wiimu_shm->language, "en_ca") == 0 ||
                                             strcmp(g_wiimu_shm->language, "en-CA") == 0)
                                        asr_set_language(LOCALE_CA);
                                    else if (strcmp(g_wiimu_shm->language, "en_ir") == 0 ||
                                             strcmp(g_wiimu_shm->language, "en-IR") == 0)
                                        asr_set_language(LOCALE_UK);
                                    else if (strcmp(g_wiimu_shm->language, "ja_jp") == 0 ||
                                             strcmp(g_wiimu_shm->language, "ja-JP") == 0) {
                                        asr_set_language(LOCALE_JP);
                                    } else if (strcmp(g_wiimu_shm->language, "french") == 0 ||
                                               strcmp(g_wiimu_shm->language, "fr-FR") == 0)
                                        asr_set_language(LOCALE_FR);
                                    else if (strcmp(g_wiimu_shm->language, "italian") == 0 ||
                                             strcmp(g_wiimu_shm->language, "it-IT") == 0)
                                        asr_set_language(LOCALE_IT);
                                    else if (strcmp(g_wiimu_shm->language, "spanish") == 0 ||
                                             strcmp(g_wiimu_shm->language, "es-ES") == 0)
                                        asr_set_language(LOCALE_ES);
                                    else if (strcmp(g_wiimu_shm->language, "es_mx") == 0 ||
                                             strcmp(g_wiimu_shm->language, "mx-ES") == 0)
                                        asr_set_language(LOCALE_MX);
                                    else if (strcmp(g_wiimu_shm->language, "fr_ca") == 0 ||
                                             strcmp(g_wiimu_shm->language, "fr-CA") == 0)
                                        asr_set_language(LOCALE_CA_FR);
                                    else if (strcmp(g_wiimu_shm->language, "portuguese") == 0 ||
                                             strcmp(g_wiimu_shm->language, "pt-BR") == 0)
                                        asr_set_language(LOCALE_PT_BR);
                                    else
                                        asr_set_language(LOCALE_US);
                                }
                                asr_set_default_language(((0 == ret) ? 1 : 0));
                            } else {
                                asr_set_default_language(0);
                            }
                        }
                        if (0 == ret) {
                            UnixSocketServer_write(&msg_socket, CMD_OK, strlen(CMD_OK));
                            NOTIFY_CMD(ASR_TTS, CAP_RESET);
                        } else {
                            UnixSocketServer_write(&msg_socket, CMD_FAILED, strlen(CMD_FAILED));
                        }
                    }
                    // Normal command, not the avs events
                    else if (StrInclude(recv_buf, EVENT_ALEXA_EXIT)) {
                        avs_player_force_stop(0);
                        switch_content_to_background(0);
                        // alexa_need_change_boost();
                        g_wiimu_shm->asr_ongoing = 0;

                    } else if (StrInclude(recv_buf, EVENT_LAN_GET)) {
                        char *buf = asr_get_language();
                        if (buf && strlen(buf) > 0) {
                            UnixSocketServer_write(&msg_socket, buf, strlen(buf));
                        } else {
                            UnixSocketServer_write(&msg_socket, CMD_FAILED, strlen(CMD_FAILED));
                        }

                    } else if (StrInclude(recv_buf, EVENT_CLEAR_CMD_QUEUE)) {
                        cmd_q_clear(evnet_cmd_q);
                    } else if (StrInclude(recv_buf, EVENT_LANLIST_GET)) {
                        UnixSocketServer_write(&msg_socket, LOCALE_LIST, strlen(LOCALE_LIST));
                    } else if (StrInclude(recv_buf, EVENT_SET_ENDPOINT)) {
                        int ret = -1;
                        char *end_point = NULL;
                        size_t point_len = strlen(recv_buf) - strlen(EVENT_SET_ENDPOINT) - 1 /*:*/;
                        if (point_len > 0) {
                            end_point = recv_buf + strlen(EVENT_SET_ENDPOINT) + 1;
                            cmd_q_clear(evnet_cmd_q);
                            ret = asr_set_endpoint(end_point);
                            if (0 == ret) {
                                avs_system_clear_soft_version_flag();
                                http2_conn_exit();
                                UnixSocketServer_write(&msg_socket, CMD_OK, strlen(CMD_OK));
                                killPidbyName((char *)"asr_comms");
                            } else {
                                UnixSocketServer_write(&msg_socket, CMD_FAILED, strlen(CMD_FAILED));
                            }
                        }

                    } else if (StrInclude(recv_buf, EVENT_CLEAR_NEXT)) {
                        cmd_q_remove(evnet_cmd_q, (void *)NAME_PLAYSTARTED, event_remove);
                        cmd_q_remove(evnet_cmd_q, (void *)NAME_PLAYNEARLYFIN, event_remove);
                        cmd_q_remove(evnet_cmd_q, (void *)NAME_PLAYFINISHED, event_remove);
                        cmd_q_remove(evnet_cmd_q, (void *)NAME_PLAYREPORTDELAY, event_remove);
                        cmd_q_remove(evnet_cmd_q, (void *)NAME_PLAYREPORTINTERVAL, event_remove);
                    } else if (StrInclude(recv_buf, EVENT_RESUME_RINGTONE)) {
#ifdef EN_AVS_COMMS
                        avs_comms_resume_ux();
#endif
                    } else {
                        ALEXA_PRINT(ALEXA_DEBUG_ERR, "unknow cmd\n");
                    }
                }
                ALEXA_PRINT(ALEXA_DEBUG_ERR, "ALEXA_EVENT at %lld ------\n", tickSincePowerOn());
                UnixSocketServer_close(&msg_socket);
            }
        }
    }
}

/*
 *  Init the Alexa moudle
 */
int AlexaInit(void)
{
    int shmid = 0;
    int ret = 0;
    pthread_t alexa_pid = 0;
    pthread_t alexa_down_channel = 0;
    pthread_t alexa_event_pid = 0;
    pthread_t avs_cmd_q = 0;
    cmd_q_t *evnet_cmd_q = NULL;

    AlexaSetDebugLevel(ALEXA_DEBUG_TRACE | ALEXA_DEBUG_ERR);
    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "start at %lld\n", tickSincePowerOn());

#if defined(EN_AVS_COMMS) && defined(HARMAN)
    cxdish_call_boost(1);
#endif
    /*
     * Init a ring buffer which used to upload record data
     */
    g_ring_buffer = RingBufferCreate(0);
    if (!g_ring_buffer) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "error\n");
        return -1;
    }

    g_is_cancel = 0;

    /*
     * If mplayer shmem not create, then create it. else get it
     */
    shmid = shmget(34387, 4096, 0666 | IPC_CREAT | IPC_EXCL);
    if (shmid != -1) {
        g_mplayer_shmem = (share_context *)shmat(shmid, (const void *)0, 0);
    } else {
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "shmget error %d \n", errno);
        if (EEXIST == errno) {
            shmid = shmget(34387, 0, 0);
            if (shmid != -1) {
                g_mplayer_shmem = (share_context *)shmat(shmid, (const void *)0, 0);
            }
        } else {
            ALEXA_PRINT(ALEXA_DEBUG_TRACE, "shmget error %d \r\n", errno);
        }
    }

    if (!g_mplayer_shmem) {
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "g_mplayer_shmem create failed in asr_tts\r\n");
    } else {
        g_mplayer_shmem->err_count = 0;
    }
    if (g_wiimu_shm->asr_test_mode)
        return 0;
    /*
     * Init to get token and refresh token from amazon server
     */
    ret = LifepodAuthorizationInit();
    if (ret) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "AlexaAuthorizationInit failed\n");
        return -1;
    }

    init_spotify_capability();
    avs_system_init();
    avs_player_init();
    evnet_cmd_q = cmd_q_init(NULL);
    avs_notificaion_init();
#ifdef EN_AVS_BLE
    avs_bluetooth_init();
#endif
    avs_emplayer_init();
    focus_mgmt_init();
#ifdef EN_AVS_COMMS
    avs_comms_init();
#endif

    /*
     * create a thread to recv comand from others, just recv login command and start recognize
     */
    ret = pthread_create(&alexa_pid, NULL, alexa_comand_proc, (void *)evnet_cmd_q);
    if (ret) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "pthread_create alexa_comand_proc failed\n");
        return -1;
    }

    /*
     *  create a thread to exec EVENTS that need to send to AVS server
     */
    ret = pthread_create(&alexa_event_pid, NULL, alexa_msg_recv, (void *)evnet_cmd_q);
    if (ret) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "pthread_create alexa_msg_recv failed\n");
        return -1;
    }

    /*
     * create a thread to KEEP the device online and recv Directive from AVS send by APP: volume
     * control, play control
     */
    ret = pthread_create(&alexa_down_channel, NULL, alexa_down_channel_proc, NULL);
    if (ret) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "pthread_create alexa_down_channel_proc failed\n");
        return -1;
    }

    /*
     * create a thread to ping AVS server
     */
    ret = pthread_create(&avs_cmd_q, NULL, avs_msg_parse, (void *)evnet_cmd_q);
    if (ret) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "pthread_create avs_msg_parse failed\n");
        return -1;
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "exit\n");

    return 0;
}

/*
 * Uninit the Alexa moudle
 */
int AlexaUnInit(void)
{
    ALEXA_PRINT(ALEXA_DEBUG_INFO, "start\n");

    if (g_ring_buffer) {
        RingBufferDestroy(&g_ring_buffer);
        g_ring_buffer = NULL;
    }

    avs_notificaion_uninit();
#ifdef EN_AVS_BLE
    avs_bluetooth_uninit();
#endif
    avs_emplayer_uninit();
    avs_system_uninit();
    avs_player_uninit();
#ifdef EN_AVS_COMMS
    avs_comms_uninit();
#endif
    ALEXA_PRINT(ALEXA_DEBUG_INFO, "exit\n");

    return 0;
}

void AlexaInitRecordData(void)
{
    // init the circle buffer for record data
    RingBufferReset(g_ring_buffer);
}

/*
 *   the record thread will call this function to write the record data to
 *       a ring buffer which will upload to AVS server
 *   Input : data :  PCM (16000 sample rate, 16 bit, channel 1) record data
 *              size: the data length
 *   retrun : the writen len
 */
int AlexaWriteRecordData(char *data, size_t size)
{
    size_t less_len = size;
    while (less_len > 0) {
        int write_len = RingBufferWrite(g_ring_buffer, data, less_len);
        if (write_len > 0) {
            less_len -= write_len;
            data += write_len;
            if (!alexa_dump_info.record_start_ts) {
                alexa_dump_info.record_start_ts = tickSincePowerOn();
            }
        } else if (write_len <= 0) {
            // wiimu_log(1, 0, 0, 0, "RingBufferWrite return %d\n", write_len);
            return less_len;
        }
    }
    return size;
}

/*
 * stop write record data when finished speek
 */
int AlexaFinishWriteData(void) { return RingBufferSetFinished(g_ring_buffer, 1); }

/*
 * stop write record data by other
 */
int AlexaStopWriteData(void) { return RingBufferSetStop(g_ring_buffer, 1); }

/*
 *  return 0 : not login
 */
int LifepodCheckLogin(void)
{
    int log_in = 0;
    char *refresh_token = LifepodGetRefreshToken();
    if (refresh_token) {
        log_in = 1;
    } else {
        log_in = 0;
    }

    if (refresh_token) {
        free(refresh_token);
    }

    return log_in;
}

long get_play_pos(void)
{
    char *shmem_mplayer = NULL;
    int shmid;
    if (g_mplayer_shmem == NULL) {
        shmid = shmget(34387, 4096, 0666 | IPC_CREAT | IPC_EXCL);
        if (shmid != -1)
            shmem_mplayer = (char *)shmat(shmid, (const void *)0, 0);
        else {
            printf("%s shmget error %d \n", __FUNCTION__, errno);
            if (EEXIST == errno) {
                shmid = shmget(34387, 0, 0);
                if (shmid != -1)
                    shmem_mplayer = (char *)shmat(shmid, (const void *)0, 0);
            }
        }
        if (shmem_mplayer)
            g_mplayer_shmem = (share_context *)shmem_mplayer;
    }

    if (g_mplayer_shmem) {
        if (PLAYER_MODE_IS_SPOTIFY(g_wiimu_shm->play_mode))
            return (long)(g_wiimu_shm->play_pos);
        return (long)(g_mplayer_shmem->offset_pts * 1000.0);
    }
    return 0;
}

void alexa_update_action_time(void) { avs_system_update_inactive_timer(); }
