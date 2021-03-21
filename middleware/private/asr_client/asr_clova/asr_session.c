
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>

#include "caiman.h"
#include "json.h"
#include "wm_util.h"

#include "cmd_q.h"
#include "ring_buffer.h"
#include "tick_timer.h"

#include "asr_cmd.h"
#include "asr_debug.h"
#include "asr_json_common.h"

#include "asr_cfg.h"
#include "asr_alert.h"
#include "asr_event.h"
#include "asr_system.h"
#include "asr_player.h"
#include "asr_session.h"
#include "asr_bluetooth.h"
#include "asr_notification.h"
#include "asr_result_parse.h"
#include "asr_authorization.h"
#include "asr_light_effects.h"
#include "clova_speech_recognizer.h"
#include "clova_device_control.h"
#if defined(LGUPLUS_IPTV_ENABLE)
#include "iptv_msg.h"
#endif

extern int g_downchannel_ready;
static ring_buffer_t *g_ring_buffer = NULL;
static sem_t g_ready_sem;

static void asr_need_talk_stop(int error_code)
{
    char buff[64] = {0};

    if (get_asr_talk_start() == 0) {
        ASR_PRINT(ASR_DEBUG_TRACE, "talk to stop %d\n", error_code);
        snprintf(buff, sizeof(buff), TLK_STOP_N, error_code);
        NOTIFY_CMD(ASR_TTS, buff);
    }
}

static int asr_send_recognize(int req_from)
{
    int ret = -1;
    char *state_json = NULL;
    char *cmd_js_str = NULL;

    state_json = asr_player_state(req_from);
    Create_Cmd(cmd_js_str, NAMESPACE_SPEECHRECOGNIZER, NAME_RECOGNIZE, Val_Obj(NULL));
    ret = asr_event_exec(state_json, cmd_js_str, (void *)g_ring_buffer, 35);

    if (asr_player_get_state(e_asr_status)) {
        ret = ERR_ASR_CONTINUE;
    } else if (!asr_player_get_state(e_tts_player)) {
        asr_notification_resume();
    }
    common_focus_manage_clear(e_focus_recognize);

    if (state_json) {
        free(state_json);
    }

    if (cmd_js_str) {
        free(cmd_js_str);
    }

    return ret;
}

static int asr_send_event(char *cmd_js_str)
{
    int ret = -1;
    char *state_json = NULL;

    if (!cmd_js_str) {
        ASR_PRINT(ASR_DEBUG_ERR, "cmd_js_str is NULL\n");
        return -1;
    }

    state_json = asr_player_state(0);
    ret = asr_event_exec(state_json, cmd_js_str, NULL, 30);

    if (state_json) {
        free(state_json);
    }

    return ret;
}

void *asr_rcognize_proc(void *arg)
{
    int ret = 0;
    int req_from = *(int *)arg;

    sem_post(&g_ready_sem);
    ret = asr_send_recognize(req_from);
    if (ret != ERR_ASR_CONTINUE && get_asr_talk_start() == 0) {
        if (ret != 0) {
            asr_light_effects_set(e_effect_listening_off);
            asr_need_talk_stop(ret);
        } else {
            asr_need_talk_stop(0);
        }
    } else if (ret == ERR_ASR_EXCEPTION || ret == -1 || ret == -3 || ret == -2) {
        asr_need_talk_stop(ret);
        if (get_asr_talk_start() == 0)
            asr_light_effects_set(
                e_effect_listening_off); // if new recognition is started, do not turn off led
    }

    return NULL;
}

/*
 * Create the rcognize request thread
 */
static int asr_recognize_create(pthread_t *pid, int req_from)
{
    int ret = 0;
    static int data = 0;
    pthread_attr_t attr;
    struct sched_param param;

    data = req_from;
    ASR_PRINT(ASR_DEBUG_TRACE, "+++++start(%lld)\n", tickSincePowerOn());

    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    param.sched_priority = 80;
    pthread_attr_setschedparam(&attr, &param);

    ret = pthread_create(pid, &attr, asr_rcognize_proc, (void *)&data);
    if (ret) {
        ASR_PRINT(ASR_DEBUG_ERR, "asr_rcognize_proc failed\n");
        set_asr_talk_start(0);
        set_session_state(0);
        asr_need_talk_stop(ERR_ASR_PROPMT_STOP);
        *pid = 0;
    }
    pthread_attr_destroy(&attr);
    sem_wait(&g_ready_sem);
    ASR_PRINT(ASR_DEBUG_TRACE, "-----end pid(%u)(%lld)\n", *pid, tickSincePowerOn());

    return ret;
}

/*
 * Stop the rcognize request thread
 */
static int asr_recognize_destroy(pthread_t *pid)
{
    int ret = -1;
    ASR_PRINT(ASR_DEBUG_INFO, "start\n");

    asr_player_buffer_stop(NULL, e_asr_start);
    if (*pid > 0) {
        http2_conn_cancel_upload();
        ASR_PRINT(ASR_DEBUG_TRACE, "+++++join(%u) start(%lld)\n", *pid, tickSincePowerOn());
        pthread_join(*pid, NULL);
        ASR_PRINT(ASR_DEBUG_TRACE, "-----join end(%lld)\n", tickSincePowerOn());
        *pid = 0;
    }

    return ret;
}

static void asr_start_request(pthread_t *pid, int req_from)
{
    asr_recognize_destroy(pid);
    // start the recognize request
    asr_recognize_create(pid, req_from);
}

static void asr_stop_recoginze(pthread_t *pid)
{
    if (*pid > 0) {
        set_asr_talk_start(0);
        set_session_state(0);
        asr_recognize_destroy(pid);
        asr_light_effects_set(e_effect_listening_off);
        asr_need_talk_stop(ERR_ASR_PROPMT_STOP);
    }
}

void asr_logout(pthread_t *pid)
{
    asr_recognize_destroy(pid);
    asr_player_force_stop(1);
    asr_authorization_logout(1);
    asr_set_default_language(0);
    asr_clear_alert_cfg();
    asr_notificaion_clear();
    http2_conn_exit();
    set_asr_talk_start(0);
    set_session_state(0);
}

// TODO: how to use the same connection
void *asr_down_channel_proc(void *arg)
{
    int is_running = 1;
    while (is_running) {
        char *asr_access_token = NULL;

        set_asr_online(0);
        asr_access_token = asr_authorization_access_token();
        if (get_internet_flags() == 1 && asr_access_token && get_ota_block_status()) {
            // each time we create directive channel, remove local settings and then synchronize
            // them with server
            if (!get_asr_alert_status())
                asr_clear_alert_cfg();
            clova_settings_uninit();
            clova_settings_init();

            check_ota_state();
            asr_queue_clear_alert();
            asr_event_down_channel();
            asr_light_effects_set(e_effect_listening_off); // if downstream disconnected, turn off
                                                           // led in case that old recognition
                                                           // didn't do it
        }
        set_asr_online(0);
        set_asr_talk_start(0);
        set_session_state(0);
        if (asr_access_token) {
            free(asr_access_token);
        }

        sleep(1);
    }

    return NULL;
}

void *asr_msg_parse(void *ctx)
{
    cmd_q_t *evnet_cmd_q = (cmd_q_t *)ctx;
    int to_cnt = 0;
    int is_running = 1;

    while (is_running) {
        char *cmd = NULL;
        if (get_asr_online() && g_downchannel_ready) {
            cmd = (char *)cmd_q_pop(evnet_cmd_q);
        }
        if (cmd) {
            ASR_PRINT(ASR_DEBUG_INFO, "cmd:\n%s\n", cmd);
            int ret = asr_send_event(cmd);
            if (ret == -2 && get_asr_online() == 0) {
                ASR_PRINT(ASR_DEBUG_ERR, "!!!!!!!downchannel offline need retry\n");
                cmd_q_add(evnet_cmd_q, cmd, strlen(cmd));
                to_cnt = 0;
            } else {
                to_cnt = 0;
            }
            cmd_free(&cmd);
            usleep(5000);
        } else {
            usleep(20000);
        }
    }

    return NULL;
}

static void asr_focus_manage_exec(pthread_t *pid)
{
    int session_state;

    if (clova_get_extension_state()) {
        clova_clear_extension_state();
        asr_player_buffer_stop(NULL, e_asr_stop);
    }
    if (get_asr_talk_start()) {
        session_state = get_session_state();
        if (session_state == 1)
            asr_stop_recoginze(pid);
#if defined(LGUPLUS_IPTV_ENABLE)
        else if (!session_state)
            iptv_cancel_recording();
#endif
    }
}

static void asr_cancel_recognition_and_tts(pthread_t *pid)
{
    asr_player_buffer_stop(NULL, e_asr_stop);

    if (get_asr_talk_start()) {
        asr_stop_recoginze(pid);
    }
}

void *asr_msg_recv(void *ctx)
{
    int ret = 0;
    int recv_num = 0;
    char recv_buf[8 * 1024] = {0};
    int init_ok = 0;
    pthread_t record_upload_pid = 0;

    cmd_q_t *evnet_cmd_q = (cmd_q_t *)ctx;

    SOCKET_CONTEXT msg_socket;
    msg_socket.path = (char *)ASR_EVENT_PIPE;
    msg_socket.blocking = 1;

RE_INIT:
    ret = UnixSocketServer_Init(&msg_socket);
    if (ret <= 0) {
        ASR_PRINT(ASR_DEBUG_ERR, "asr event recv UnixSocketServer_Init fail\n");
        sleep(5);
        goto RE_INIT;
    }

    if (init_ok == 0) {
        init_ok = 1;
        asr_ble_getpaired_list();
    }

    while (1) {
        ret = UnixSocketServer_accept(&msg_socket);
        if (ret <= 0) {
            ASR_PRINT(ASR_DEBUG_ERR, "asr event recv UnixSocketServer_accept fail\n");
            UnixSocketServer_deInit(&msg_socket);
            sleep(5);
            goto RE_INIT;
        } else {
            memset(recv_buf, 0x0, sizeof(recv_buf));

            recv_num = UnixSocketServer_read(&msg_socket, recv_buf, sizeof(recv_buf));
            if (recv_num <= 0) {
                ASR_PRINT(ASR_DEBUG_ERR, "asr event recv failed\r\n");
                UnixSocketServer_close(&msg_socket);
                UnixSocketServer_deInit(&msg_socket);
                sleep(5);
                goto RE_INIT;
            } else {
                recv_buf[recv_num] = '\0';
                // ASR_PRINT(ASR_DEBUG_TRACE, "recv_buf:\n%s\n", recv_buf);
                ASR_PRINT(ASR_DEBUG_TRACE, "asr event recv %s at %lld ++++++++\n", recv_buf,
                          tickSincePowerOn());

                if (0 == asr_event_cmd_type(recv_buf, NULL)) {
                    // before insert an new event, check if it need to remove some old events
                    if (1 == asr_event_cmd_type(recv_buf, (char *)Val_bluetooth)) {
                        int need_tts = 0;
                        char *local_cmd = asr_bluetooth_parse_cmd(recv_buf, &need_tts);
                        if (local_cmd) {
                            cmd_q_add_tail(evnet_cmd_q, local_cmd, strlen(local_cmd));
                            free(local_cmd);
                            if (need_tts)
                                asr_focus_manage_exec(&record_upload_pid);
                        }
                    } else {
                        ASR_PRINT(ASR_DEBUG_INFO, "parse the message OK\n");
                        cmd_q_add_tail(evnet_cmd_q, recv_buf, strlen(recv_buf));
                    }
                } else {
                    if (StrInclude(recv_buf, EVENT_ALERT_START)) {
                        char *alert_token = recv_buf + strlen(EVENT_ALERT_START) + 1;
                        char *alert_type = strchr(alert_token, ':');
                        *alert_type = '\0';
                        alert_type += 1;
                        size_t token_len = alert_type - alert_token - 1 /*:*/;
                        if (token_len > 0) {
                            clova_alerts_general_cmd(NAME_ALERT_STARTED, alert_token, alert_type);
                            asr_focus_manage_exec(&record_upload_pid);
                        }
                    } else if (StrInclude(recv_buf, EVENT_ALERT_STOP)) {
                        char *alert_token = recv_buf + strlen(EVENT_ALERT_STOP) + 1;
                        char *alert_type = strchr(alert_token, ':');
                        *alert_type = '\0';
                        alert_type += 1;
                        size_t token_len = alert_type - alert_token - 1 /*:*/;
                        if (token_len > 0) {
                            clova_alerts_general_cmd(NAME_ALERT_STOPPED, alert_token, alert_type);
                        }

                    } else if (StrInclude(recv_buf, EVENT_DEL_ALERT_OK)) {
                        char *alert_token = recv_buf + strlen(EVENT_DEL_ALERT_OK) + 1;
                        char *alert_type = strchr(alert_token, ':');
                        *alert_type = '\0';
                        alert_type += 1;
                        size_t token_len = alert_type - alert_token - 1 /*:*/;
                        if (token_len > 0) {
                            clova_alerts_general_cmd(NAME_DELETE_ALERT_SUCCEEDED, alert_token,
                                                     alert_type);
                        }
                    } else if (StrInclude(recv_buf, EVENT_ALERT_LOCAL_DELETE)) {
                        int token_len = 0;
                        char *alert_token = recv_buf + strlen(EVENT_ALERT_LOCAL_DELETE) + 1;
                        char *alert_type = strchr(alert_token, ':');
                        *alert_type = '\0';
                        alert_type += 1;
                        token_len = alert_type - alert_token - 1 /*:*/;
                        if (token_len > 0) {
                            clova_alerts_general_cmd(NAME_ALERT_STOPPED, alert_token, alert_type);
                        }
                    } else if (StrInclude(recv_buf, EVENT_PLAY_NEXT)) {
                        cmd_add_playback_event(NAME_NEXTCOMMANDISSUED);
                    } else if (StrInclude(recv_buf, EVENT_PLAY_PREV)) {
                        cmd_add_playback_event(NAME_PREVIOUSCOMMANDISSUED);
                    } else if (StrInclude(recv_buf, EVENT_PLAY_PAUSE)) {
                        char *stop_tts = recv_buf + strlen(EVENT_PLAY_PAUSE) + 1;
                        if (stop_tts && atoi(stop_tts) == 1) {
                            clova_set_explicit(0);
                            clova_clear_extension_state();
                            asr_player_buffer_stop(NULL, e_asr_stop);
                        } else {
                            cmd_add_playback_event(NAME_PAUSECOMMANDISSUED);
                        }
                    } else if (StrInclude(recv_buf, EVENT_PLAY_PLAY)) {
                        cmd_add_playback_event(NAME_PLAYCOMMANDISSUED);
                    } else if (StrInclude(recv_buf, EVENT_MUTE_CHANGED)) {
                        clova_device_control_report_cmd(CLOVA_HEADER_NAME_REPORT_STATE);
                    } else if (StrInclude(recv_buf, EVENT_VOLUME_CHANGED)) {
                        clova_device_control_report_cmd(CLOVA_HEADER_NAME_REPORT_STATE);
#if defined(LGUPLUS_IPTV_ENABLE)
                    } else if (StrInclude(recv_buf, CDK_SEND_MESSAGE)) {
                        char *ptr = recv_buf + strlen(CDK_SEND_MESSAGE) + 1;
                        cmd_add_normal_event(IPTV_HEADER_NAMESPACE_CDK,
                                             IPTV_HEADER_NAME_SEND_MESSAGE, ptr);
#endif
                    } else if (StrInclude(recv_buf, ASR_RECO_START)) {
                        int req_from = atoi(recv_buf + strlen(ASR_RECO_START) + 1);
                        asr_start_request(&record_upload_pid, req_from);
                    } else if (StrInclude(recv_buf, ASR_RECO_STOP)) {
                        asr_stop_recoginze(&record_upload_pid);
                    } else if (StrInclude(recv_buf, ASR_IS_LOGIN)) {
                        int login = asr_session_check_login();
                        if (login)
                            UnixSocketServer_write(&msg_socket, "1", 1);
                        else
                            UnixSocketServer_write(&msg_socket, "0", 1);
                    } else if (StrInclude(recv_buf, ASR_LOGIN)) {

                        // init the asr accesstoken, read from the file
                        char *cmd = recv_buf + strlen(ASR_LOGIN);
                        if (cmd && strlen(cmd) > 0) {
                            asr_logout(&record_upload_pid);
                            ret = asr_authorization_login(cmd);
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
                    } else if (StrInclude(recv_buf, ASR_LOGOUT)) {
                        asr_notificaion_clear();
                        asr_player_buffer_stop(NULL, e_asr_stop);
                        asr_stop_recoginze(&record_upload_pid);
                        asr_logout(&record_upload_pid);
                        UnixSocketServer_write(&msg_socket, CMD_OK, strlen(CMD_OK));
                    } else if (StrInclude(recv_buf, ASR_POWEROFF)) {
                        asr_notificaion_clear();
                        asr_player_buffer_stop(NULL, e_asr_stop);
                        asr_player_force_stop(1);
                        asr_authorization_logout(0);
                        http2_conn_exit();
                        set_asr_talk_start(0);
                        set_session_state(0);
                        UnixSocketServer_write(&msg_socket, CMD_OK, strlen(CMD_OK));
                    } else if (StrInclude(recv_buf, ASR_SET_LAN)) {
                        int ret = -1;
                        size_t message_len = strlen(recv_buf) - strlen(ASR_SET_LAN) - 1 /*:*/;
                        if (message_len > 0) {
                            char *location = (char *)(recv_buf + strlen(ASR_SET_LAN) + 1);
                            ret = asr_set_language(location);
                            if (get_asr_online()) {
                                if (0 != ret) {
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
                    } else if (StrInclude(recv_buf, ASR_EXIT)) {
                        // Normal command, not the asr events
                        asr_player_force_stop(0);
                        set_asr_talk_start(0);
                        set_session_state(0);
                    } else if (StrInclude(recv_buf, ASR_SET_ENDPOINT)) {
                        int ret = -1;
                        char *end_point = NULL;
                        size_t point_len = strlen(recv_buf) - strlen(ASR_SET_ENDPOINT) - 1 /*:*/;
                        if (point_len > 0) {
                            end_point = recv_buf + strlen(ASR_SET_ENDPOINT) + 1;
                            ret = asr_set_endpoint(end_point);
                            if (0 == ret) {
                                http2_conn_exit();
                                UnixSocketServer_write(&msg_socket, CMD_OK, strlen(CMD_OK));
                            } else {
                                UnixSocketServer_write(&msg_socket, CMD_FAILED, strlen(CMD_FAILED));
                            }
                        }
                    } else if (StrInclude(recv_buf, ASR_LOCAL_NOTIFY)) {
                        char *js_str = recv_buf + strlen(ASR_LOCAL_NOTIFY) + 1;
                        if (js_str && strlen(js_str) > 0) {
                            asr_notificaion_add_local_notify(js_str);
                        }
                    } else if (StrInclude(recv_buf, ASR_WPS_START)) {
                        asr_player_set_pause();
                        asr_bluetooth_clear_active();
                        asr_clear_alert_cfg();
                        asr_notificaion_clear();
                        asr_cancel_recognition_and_tts(&record_upload_pid);
                    } else if (StrInclude(recv_buf, ASR_WPS_END)) {
                        clova_alerts_request_sync_cmd(NAME_ALERT_REQUEST_SYNC);
                    } else if (StrInclude(recv_buf, ASR_FREE_TALK_START)) {
                        asr_player_set_pause();
                        asr_notificaion_clear();
                        alert_stop_current();
                    } else if (StrInclude(recv_buf, ASR_STOP_FREE_TALK)) {
                        asr_focus_manage_exec(&record_upload_pid);
                    } else {
                        ASR_PRINT(ASR_DEBUG_ERR, "unknow cmd\n");
                    }
                }
                ASR_PRINT(ASR_DEBUG_ERR, "asr event recv at %lld ------\n", tickSincePowerOn());
                UnixSocketServer_close(&msg_socket);
            }
        }
    }

    return NULL;
}

int asr_session_init(void)
{
    int shmid = 0;
    int ret = 0;
    pthread_t asr_down_channel = 0;
    pthread_t asr_event_pid = 0;
    pthread_t asr_idle = 0;
    pthread_t asr_cmd_q = 0;
    cmd_q_t *evnet_cmd_q = NULL;

    asr_set_debug_level(ASR_DEBUG_TRACE | ASR_DEBUG_ERR);
    ASR_PRINT(ASR_DEBUG_INFO, "start\n");

    /*
     * Init a ring buffer which used to upload record data
     */
    g_ring_buffer = RingBufferCreate(0);
    if (!g_ring_buffer) {
        ASR_PRINT(ASR_DEBUG_ERR, "error\n");
        return -1;
    }

    sem_init(&g_ready_sem, 0, 0);
    /*
    * Init to get token and refresh token from asr server
    */
    ret = asr_authorization_init();
    if (ret) {
        ASR_PRINT(ASR_DEBUG_ERR, "asr_authorization_init failed\n");
        return -1;
    }

    asr_system_init();
    init_update();
    evnet_cmd_q = asr_queue_init();
    asr_player_init();
    asr_notificaion_init();
    asr_bluetooth_init();
    clova_explicit_init();

    /*
     *  create a thread to exec EVENTS that need to send to asr server
     */
    ret = pthread_create(&asr_event_pid, NULL, asr_msg_recv, (void *)evnet_cmd_q);
    if (ret) {
        ASR_PRINT(ASR_DEBUG_ERR, "pthread_create asr_msg_recv failed\n");
        return -1;
    }

    /*
     * create a thread to KEEP the device online and recv Directive from asr send by APP: volume
     * control, play control
     */
    ret = pthread_create(&asr_down_channel, NULL, asr_down_channel_proc, NULL);
    if (ret) {
        ASR_PRINT(ASR_DEBUG_ERR, "pthread_create asr_down_channel_proc failed\n");
        return -1;
    }

    /*
     * create a thread to ping asr server
     */
    ret = pthread_create(&asr_cmd_q, NULL, asr_msg_parse, (void *)evnet_cmd_q);
    if (ret) {
        ASR_PRINT(ASR_DEBUG_ERR, "pthread_create asr_msg_parse failed\n");
        return -1;
    }

    ASR_PRINT(ASR_DEBUG_INFO, "exit\n");

    return 0;
}

int asr_session_uninit(void)
{
    ASR_PRINT(ASR_DEBUG_INFO, "start\n");

    if (g_ring_buffer) {
        RingBufferDestroy(&g_ring_buffer);
        g_ring_buffer = NULL;
    }
    sem_destroy(&g_ready_sem);

    clova_explicit_uninit();
    asr_notificaion_uninit();
    asr_system_uninit();
    asr_player_uninit();
    asr_bluetooth_uninit();
    asr_queue_uninit();

    ASR_PRINT(ASR_DEBUG_INFO, "exit\n");

    return 0;
}

void asr_session_record_buffer_reset(void)
{
    // init the circle buffer for record data
    RingBufferReset(g_ring_buffer);
}

/*
 *   the record thread will call this function to write the record data to
 *       a ring buffer which will upload to asr server
 *   Input : data :  PCM (16000 sample rate, 16 bit, channel 1) record data
 *              size: the data length
 *   retrun : the writen len
 */
int asr_session_record_buffer_write(char *data, size_t size)
{
    size_t less_len = size;
    while (less_len > 0) {
        int write_len = RingBufferWrite(g_ring_buffer, data, less_len);
        if (write_len > 0) {
            less_len -= write_len;
            data += write_len;
        } else if (write_len <= 0) {
            wiimu_log(1, 0, 0, 0, "RingBufferWrite return %d\n", write_len);
            return less_len;
        }
    }
    return size;
}

/*
 * stop write record data when finished speek
 */
int asr_session_record_buffer_end(void) { return RingBufferSetFinished(g_ring_buffer, 1); }

/*
 * stop write record data by other
 */
int asr_session_record_buffer_stop(void) { return RingBufferSetStop(g_ring_buffer, 1); }

/*
 *  return 0 : not login
 */
int asr_session_check_login(void)
{
    int log_in = 0;
    char *refresh_token = asr_authorization_refresh_token();
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
