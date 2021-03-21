
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>

#include <sys/ipc.h>
#include <sys/shm.h>

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
#include "asr_connect.h"
#include "asr_system.h"
#include "asr_player.h"
#include "asr_session.h"
#include "asr_bluetooth.h"
#include "asr_notification.h"
#include "asr_result_parse.h"
#include "asr_authorization.h"
#include "asr_light_effects.h"

#include "lguplus_event_queue.h"
#include "lguplus_json_common.h"

#include "url_player.h"

#define _GNU_SOURCE

enum {
    e_state_none = 0,
    e_state_authed,

    e_state_max,
};

enum {
    e_fd_cmd = 0,
    e_fd_clova,
    e_fd_lguplus,

    e_fd_max,
};

typedef struct _asr_info_s {
    int fd_asr;
    int last_recognize;
    int state;
    asr_connect_t *asr_connect;
} asr_info_t;

typedef struct _asr_session_s {
    int is_running;
    pthread_t asr_event_pid;
    pthread_t asr_cmd_pid;
    ring_buffer_t *ring_buffer;

    int fd_cmd;
    int timeou_cnt;
    int inactive_timer;
    struct pollfd pollfds[e_fd_max];
    asr_info_t asr_info[e_asr_max];
    cmd_q_t *evnet_queue[e_asr_max];
} asr_session_t;

static asr_session_t asr_session = {};

extern int g_asr_req_from;
extern int h2_reestablish;

void asr_logout()
{
    asr_player_force_stop();
    asr_authorization_logout(1);
    asr_set_default_language(0);
    asr_clear_alert_cfg();
    asr_notificaion_clear();
    set_asr_talk_start(0);
}

int asr_session_get_inactive_time(void) { return asr_session.inactive_timer; }

int asr_session_get_auth(asr_session_t *asr_session, int asr_type)
{
    int ret = -1;
    char *assess_token = NULL;

    if (!asr_session) {
        return ret;
    }

    if (asr_type == e_asr_clova) {
        assess_token = asr_authorization_access_token();
        if (assess_token) {
            free(assess_token);
            asr_session->asr_info[asr_type].state = e_state_authed;
            ret = 0;
        }
    } else if (asr_type == e_asr_lguplus) {
        assess_token = lguplus_get_platform_token();
        if (assess_token) {
            free(assess_token);
            asr_session->asr_info[asr_type].state = e_state_authed;
            ret = 0;
        }
    }

    return ret;
}

int asr_session_connect_create(asr_session_t *asr_session, int asr_type)
{
    int ret = -1;
    int fd = -1;
    char *end_point = NULL;
    asr_connect_t *asr_connect = NULL;

    if (!asr_session || asr_type <= e_asr_none || asr_type >= e_asr_max) {
        return ret;
    }

    asr_connect = asr_session->asr_info[asr_type].asr_connect;
    if (asr_connect == NULL) {
        asr_connect = asr_connect_create(asr_type, asr_session->ring_buffer);
        if (asr_connect) {
            if (asr_type == e_asr_clova) {
                end_point = asr_get_endpoint();
            } else if (asr_type == e_asr_lguplus) {
                end_point = lguplus_get_endpoint();
            }
            fd = asr_connect_init(asr_connect, end_point, 443, 10);
            if (fd >= 0) {
                asr_session->asr_info[asr_type].fd_asr = fd;
                asr_session->asr_info[asr_type].asr_connect = asr_connect;
                if (asr_type == e_asr_lguplus) {
                    char *cmd_json = NULL;
                    Create_Event(cmd_json, LGUP_SYSTEM, LGUP_SYNCHRONIZE, DEFAULT_STR, DEFAULT_STR,
                                 DEFAULT_BODY);
                    if (cmd_json) {
                        asr_connect_submit_request(asr_connect, e_downchannel, NULL, cmd_json, 0);
                        free(cmd_json);
                    }
                } else if (asr_type == e_asr_clova) {
                    asr_connect_submit_request(asr_connect, e_downchannel, NULL, NULL, 0);
                }
            } else {
                asr_session_connect_destroy(asr_session, asr_type);
            }
        } else {
            asr_session_connect_destroy(asr_session, asr_type);
        }
    }

    return ret;
}

int asr_session_connect_destroy(asr_session_t *asr_session, int asr_type)
{
    int ret = -1;
    asr_connect_t *asr_connect = NULL;

    if (!asr_session || asr_type <= e_asr_none || asr_type >= e_asr_max) {
        return ret;
    }

    asr_connect = asr_session->asr_info[asr_type].asr_connect;
    if (asr_connect) {
        asr_connect_destroy(&asr_connect, asr_type);
        asr_session->asr_info[asr_type].asr_connect = NULL;
        asr_session->asr_info[asr_type].fd_asr = -1;
        asr_session->asr_info[asr_type].state = e_state_none;
        ret = 0;
    }

    return ret;
}

int asr_session_add_recognize(asr_session_t *asr_session, int asr_type, int req_from)
{
    int ret = -1;
    char *state_json = NULL;
    char *cmd_js_str = NULL;
    int last_recognize = -1;
    asr_connect_t *asr_connect = NULL;

    if (!asr_session || asr_type <= e_asr_none || asr_type >= e_asr_max) {
        return ret;
    }

    asr_connect = asr_session->asr_info[asr_type].asr_connect;
    if (asr_type == e_asr_clova) {
        asr_queue_clear_speech();
        if (asr_connect) {
            last_recognize = asr_session->asr_info[asr_type].last_recognize;
            if (last_recognize > 0) {
                asr_connect_cancel_stream(asr_connect, last_recognize);
                asr_session->asr_info[asr_type].last_recognize = -1;
            }
            Create_Cmd(cmd_js_str, NAMESPACE_SPEECHRECOGNIZER, NAME_RECOGNIZE, Val_Obj(0));
            if (cmd_js_str) {
                state_json = asr_player_state(req_from);
                if (state_json) {
                    set_asr_talk_start(0);
                    last_recognize = asr_connect_submit_request(asr_connect, e_recognize,
                                                                state_json, cmd_js_str, 30);
                    asr_session->asr_info[asr_type].last_recognize = last_recognize;
                    ret = 0;
                }
            }
        }
    } else if (asr_type == e_asr_lguplus) {
        if (asr_connect) {
            Create_Event(cmd_js_str, LGUP_RECOGNIZE, LGUP_WAKEUP, DEFAULT_STR, DEFAULT_STR,
                         DEFAULT_BODY);
            if (cmd_js_str) {
                set_asr_talk_start(0);
                lguplus_event_queue_clear_recognize();
                asr_connect_submit_request(asr_connect, e_recognize, NULL, cmd_js_str, 30);
                lguplus_event_queue_add(LGUP_RECOGNIZE, LGUP_SPEECHRECOGNIZE, "", "");
                ret = 0;
            }
        }
    }

    if (cmd_js_str) {
        free(cmd_js_str);
    }
    if (state_json) {
        free(state_json);
    }

    return ret;
}

int asr_session_add_ping(asr_session_t *asr_session, int asr_type)
{
    int ret = -1;
    asr_connect_t *asr_connect = NULL;

    if (!asr_session || asr_type <= e_asr_none || asr_type >= e_asr_max) {
        return ret;
    }

    asr_connect = asr_session->asr_info[asr_type].asr_connect;
    if (asr_connect) {
        if (asr_type == e_asr_lguplus) {
            char *cmd_json = NULL;
            Create_Event(cmd_json, LGUP_HEARTBEAT, LGUP_PING, DEFAULT_STR, DEFAULT_STR,
                         DEFAULT_BODY);
            if (cmd_json) {
                asr_connect_submit_request(asr_connect, e_ping, NULL, cmd_json, 10);
                free(cmd_json);
            }
            ret = 0;
        } else if (asr_type == e_asr_clova) {
            asr_connect_submit_request(asr_connect, e_ping, NULL, NULL, 10);
            ret = 0;
        }
    }

    return ret;
}

int asr_session_add_event(asr_session_t *asr_session, int asr_type)
{
    int ret = -1;
    char *cmd_js_str = NULL;
    char *state_json = NULL;
    int can_do_event = -1;
    asr_connect_t *asr_connect = NULL;

    if (!asr_session || asr_type <= e_asr_none || asr_type >= e_asr_max) {
        return ret;
    }

    asr_connect = asr_session->asr_info[asr_type].asr_connect;
    if (asr_connect) {
        can_do_event = asr_connect_can_do_request(asr_connect);
        if (can_do_event) {
            cmd_js_str = (char *)cmd_q_pop(asr_session->evnet_queue[asr_type]);
            if (asr_type == e_asr_lguplus) {
                if (cmd_js_str) {
                    asr_connect_submit_request(asr_connect, e_event, NULL, cmd_js_str, 30);
                    ret = 0;
                }
            } else if (asr_type == e_asr_clova) {
                if (cmd_js_str) {
                    state_json = asr_player_state(0);
                    if (state_json) {
                        asr_connect_submit_request(asr_connect, e_event, state_json, cmd_js_str,
                                                   30);
                        ret = 0;
                    }
                }
            }
        }
    }

    if (cmd_js_str) {
        free(cmd_js_str);
    }
    if (state_json) {
        free(state_json);
    }

    return ret;
}

int asr_session_check_timeout(asr_session_t *asr_session)
{
    int ret = 0;

    if (asr_session) {
        ret = asr_connect_check_time_out(asr_session->asr_info[e_asr_clova].asr_connect);
        if (ret != 0) {
            asr_session_connect_destroy(asr_session, e_asr_clova);
        }
        ret = asr_connect_check_time_out(asr_session->asr_info[e_asr_lguplus].asr_connect);
        if (ret != 0) {
            asr_session_connect_destroy(asr_session, e_asr_lguplus);
        }
    }

    return ret;
}

void asr_session_io_ctrol(asr_session_t *asr_session)
{
    int i = 0;
    int io_state = e_io_none;

    if (asr_session) {
        for (i = 0; i < e_fd_max; i++) {
            asr_session->pollfds[i].fd = -1;
            asr_session->pollfds[i].events = 0;
        }
        if (asr_session->fd_cmd >= 0) {
            asr_session->pollfds[e_fd_cmd].fd = asr_session->fd_cmd;
            asr_session->pollfds[e_fd_cmd].events |= POLLIN;
        }
        if (asr_session->asr_info[e_asr_clova].fd_asr >= 0) {
            asr_session->pollfds[e_fd_clova].fd = asr_session->asr_info[e_asr_clova].fd_asr;
            if (asr_session->asr_info[e_asr_clova].asr_connect) {
                io_state = asr_connect_io_state(asr_session->asr_info[e_asr_clova].asr_connect);
            }
            if (io_state & e_io_read) {
                asr_session->pollfds[e_fd_clova].events |= POLLIN;
            }
            if (io_state & e_io_write) {
                asr_session->pollfds[e_fd_clova].events |= POLLOUT;
            }
            asr_session->pollfds[e_fd_clova].events |= POLLHUP;
            asr_session->pollfds[e_fd_clova].events |= POLLERR;
        }
        if (asr_session->asr_info[e_asr_lguplus].fd_asr >= 0) {
            asr_session->pollfds[e_fd_lguplus].fd = asr_session->asr_info[e_asr_lguplus].fd_asr;
            if (asr_session->asr_info[e_asr_lguplus].asr_connect) {
                io_state = asr_connect_io_state(asr_session->asr_info[e_asr_lguplus].asr_connect);
            }
            if (io_state & e_io_read) {
                asr_session->pollfds[e_fd_lguplus].events |= POLLIN;
            }
            if (io_state & e_io_write) {
                asr_session->pollfds[e_fd_lguplus].events |= POLLOUT;
            }
            asr_session->pollfds[e_fd_lguplus].events |= POLLHUP;
            asr_session->pollfds[e_fd_lguplus].events |= POLLERR;
        }
    }
}

void *asr_msg_parse(void *arg)
{
    int ret = 0;
    int i = 0;
    int poll_cnt = 0;
    long timeout_ms = /*-1*/ 50;
    asr_session_t *asr_session = (asr_session_t *)arg;

    if (asr_session) {
        asr_session->asr_info[e_asr_clova].fd_asr = -1;
        asr_session->asr_info[e_asr_lguplus].fd_asr = -1;
        asr_session->fd_cmd = local_sock_create(ASR_LOCAL_SOCKET);
        if (asr_session->fd_cmd < 0) {
            ASR_PRINT(ASR_DEBUG_ERR, "local_sock_create failed at %lld\n", tickSincePowerOn());
        }
    }

    ASR_PRINT(ASR_DEBUG_INFO, "internet_flags=%d at %lld\n", get_internet_flags(),
              tickSincePowerOn());

    while (asr_session && asr_session->is_running) {
        if (get_internet_flags()) {
            if (asr_session->asr_info[e_asr_clova].state == e_state_none) {
                asr_session_get_auth(asr_session, e_asr_clova);
            }
            if (asr_session->asr_info[e_asr_lguplus].state == e_state_none) {
                asr_session_get_auth(asr_session, e_asr_lguplus);
            }

            if (asr_session->asr_info[e_asr_clova].state == e_state_none &&
                asr_session->asr_info[e_asr_lguplus].state == e_state_none)
                continue;

            if (asr_session->asr_info[e_asr_clova].state == e_state_authed) {
                asr_session_connect_create(asr_session, e_asr_clova);
            }
            if (asr_session->asr_info[e_asr_lguplus].state == e_state_authed) {
                asr_session_connect_create(asr_session, e_asr_lguplus);
            }

            asr_session_io_ctrol(asr_session);
            int nfds = poll(asr_session->pollfds, e_fd_max, timeout_ms);
            if (0 > nfds) {
                break;
            } else if (0 <= nfds) {
                if (asr_session->pollfds[e_fd_clova].revents & (POLLIN)) {
                    ret = asr_connect_read(asr_session->asr_info[e_asr_clova].asr_connect);
                    if (ret < 0) {
                        asr_session_connect_destroy(asr_session, e_asr_clova);
                    } else {
                        continue;
                    }
                }
                if (asr_session->pollfds[e_fd_clova].revents & (POLLOUT)) {
                    ret = asr_connect_write(asr_session->asr_info[e_asr_clova].asr_connect);
                    if (ret < 0) {
                        asr_session_connect_destroy(asr_session, e_asr_clova);
                    }
                }
                if ((asr_session->pollfds[e_fd_clova].revents & POLLHUP) ||
                    (asr_session->pollfds[e_fd_clova].revents & POLLERR)) {
                    ASR_PRINT(ASR_DEBUG_ERR, "poll POLLHUP|POLLERR\n");
                    asr_session_connect_destroy(asr_session, e_asr_clova);
                    continue;
                }
                if (asr_session->pollfds[e_fd_lguplus].revents & (POLLIN)) {
                    ret = asr_connect_read(asr_session->asr_info[e_asr_lguplus].asr_connect);
                    if (ret < 0) {
                        asr_session_connect_destroy(asr_session, e_asr_lguplus);
                    } else {
                        continue;
                    }
                }
                if (asr_session->pollfds[e_fd_lguplus].revents & (POLLOUT)) {
                    ret = asr_connect_write(asr_session->asr_info[e_asr_lguplus].asr_connect);
                    if (ret < 0) {
                        asr_session_connect_destroy(asr_session, e_asr_lguplus);
                    }
                }
                if ((asr_session->pollfds[e_fd_lguplus].revents & POLLHUP) ||
                    (asr_session->pollfds[e_fd_lguplus].revents & POLLERR)) {
                    ASR_PRINT(ASR_DEBUG_ERR, "poll POLLHUP|POLLERR\n");
                    asr_session_connect_destroy(asr_session, e_asr_lguplus);
                    continue;
                }
                if (asr_session->pollfds[e_fd_cmd].revents & (POLLIN)) {
                    char buff[128] = {0};
                    int recv_len = read(asr_session->pollfds[e_fd_cmd].fd, buff, sizeof(buff));
                    ASR_PRINT(ASR_DEBUG_INFO, "buff=%s\n", buff);
                    if (StrInclude(buff, ASR_RECO_START)) {
                        asr_session->inactive_timer = tickSincePowerOn();
                        int req_from = atoi(buff + strlen(ASR_RECO_START) + 1);
                        if (req_from == 1 || (req_from == 2 && g_asr_req_from == 1)) {
                            ret = asr_session_add_recognize(asr_session, e_asr_clova, req_from);
                        } else {
                            ret = asr_session_add_recognize(asr_session, e_asr_lguplus, req_from);
                        }
                        if (ret != 0) {
                            char *buff = NULL;
                            ASR_PRINT(ASR_DEBUG_INFO, "talk to stop %d\n", ERR_ASR_EXCEPTION);
                            asprintf(&buff, TLK_STOP_N, ERR_ASR_EXCEPTION);
                            if (buff) {
                                NOTIFY_CMD(ASR_TTS, buff);
                                free(buff);
                            }
                            common_focus_manage_clear(e_focus_recognize);
                        }
                    } else if (StrInclude(buff, ASR_DIS_CONNECT)) {
                        int asr_type = atoi(buff + strlen(ASR_DIS_CONNECT) + 1);
                        if (asr_type == ASR_CLOVA) {
                            asr_session_connect_destroy(asr_session, e_asr_clova);
                        } else if (asr_type == ASR_LGUPLUS) {
                            asr_session_connect_destroy(asr_session, e_asr_lguplus);
                        }
                    } else if (StrInclude(buff, ASR_PING)) {
                        int asr_type = atoi(buff + strlen(ASR_PING) + 1);
                        if (asr_type == ASR_CLOVA) {
                            asr_session_add_ping(asr_session, e_asr_clova);
                        } else if (asr_type == ASR_LGUPLUS) {
                            asr_session_add_ping(asr_session, e_asr_lguplus);
                        }
                    }
                }
                asr_session_add_event(asr_session, e_asr_clova);
                asr_session_add_event(asr_session, e_asr_lguplus);
                if (0 == nfds) {
                    asr_session_check_timeout(asr_session);
                    asr_session->timeou_cnt++;
                    if (asr_session->timeou_cnt >= 100) {
                        asr_session->timeou_cnt = 0;
                        ASR_PRINT(
                            ASR_DEBUG_INFO, "poll timeout:\n clova.fd_asr=%d lguplus.fd_asr=%d"
                                            "clova_state=%d lguplus_state=%d fd_cmd=%d \n",
                            asr_session->asr_info[e_asr_clova].fd_asr,
                            asr_session->asr_info[e_asr_lguplus].fd_asr,
                            asr_connect_io_state(asr_session->asr_info[e_asr_clova].asr_connect),
                            asr_connect_io_state(asr_session->asr_info[e_asr_lguplus].asr_connect),
                            asr_session->fd_cmd);
                    }
                } else {
                    asr_session->timeou_cnt = 0;
                }
            }
        } else if (asr_session->asr_info[e_asr_clova].state == e_state_authed &&
                   asr_session->asr_info[e_asr_lguplus].state == e_state_authed) {
            asr_session_connect_destroy(asr_session, e_asr_clova);
            asr_session_connect_destroy(asr_session, e_asr_lguplus);
        } else {
            usleep(10000);
        }
    }

    ASR_PRINT(ASR_DEBUG_INFO, "end at %lld\n", tickSincePowerOn());
    // TODO: need to free
}

/*
  *  Judge cmd_js_str is cmd_type/new cmd or not
  *  retrun:
  *        < 0: not an json cmd
  *        = 0: is an json cmd
  *        = 1: is the cmd_type cmd
  */
int asr_event_cmd_type(char *cmd_js_str, char *cmd_type)
{
    int ret = -1;
    json_object *json_cmd = NULL;
    char *cmd_params_str = NULL;
    char *event_type = NULL;

    if (!cmd_js_str) {
        ASR_PRINT(ASR_DEBUG_ERR, "input is NULL\n");
        return -1;
    }

    json_cmd = json_tokener_parse(cmd_js_str);
    if (!json_cmd) {
        ASR_PRINT(ASR_DEBUG_INFO, "cmd_js is not a json\n");
        return -1;
    }

    ret = 0;
    event_type = JSON_GET_STRING_FROM_OBJECT(json_cmd, Key_Event_type);
    if (!event_type) {
        ASR_PRINT(ASR_DEBUG_ERR, "cmd none name found\n");
        goto Exit;
    }

    if (StrEq(event_type, cmd_type)) {
        ret = 1;
    }

Exit:
    json_object_put(json_cmd);

    return ret;
}

void *asr_msg_recv(void *arg)
{
    int ret = 0;
    int init_ok = 0;
    int recv_num = 0;
    char recv_buf[65536] = {0};

    asr_session_t *asr_session = (asr_session_t *)arg;

    SOCKET_CONTEXT msg_socket;
    msg_socket.path = (char *)ASR_EVENT_PIPE;
    msg_socket.blocking = 1;

    asr_session->inactive_timer = tickSincePowerOn();

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

    while (asr_session && asr_session->is_running) {
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
                ASR_PRINT(ASR_DEBUG_INFO, "asr event recv %.80s at %lld ++++++++\n", recv_buf,
                          tickSincePowerOn());

                if (0 == asr_event_cmd_type(recv_buf, NULL)) {
                    // before insert an new event, check if it need to remove some old events
                    if (1 == asr_event_cmd_type(recv_buf, (char *)Val_bluetooth)) {
                        char *local_cmd = asr_bluetooth_parse_cmd(recv_buf);
                        if (local_cmd) {
                            cmd_q_add_tail(asr_session->evnet_queue[e_asr_clova], local_cmd,
                                           strlen(local_cmd));
                            free(local_cmd);
                        }
                        ASR_PRINT(ASR_DEBUG_INFO, "bluetooth_cmd:\n%.160s\n", recv_buf);
                    } else {
                        ASR_PRINT(ASR_DEBUG_INFO, "parse the message OK\n");
                        cmd_q_add_tail(asr_session->evnet_queue[e_asr_clova], recv_buf,
                                       strlen(recv_buf));
                    }
                } else {
                    if (StrInclude(recv_buf, EVENT_ALERT_START)) {
                        char *alert_token = recv_buf + strlen(EVENT_ALERT_START) + 1;
                        char *alert_type = strchr(alert_token, ':');
                        *alert_type = '\0';
                        alert_type += 1;

                        ret = lguplus_check_alert_inside(alert_token);
                        if (ret) {
                            cmd_add_alert_event(NAME_ALERT_STARTED, alert_token, alert_type);
                        } else {
                            char body[128] = {0};
                            snprintf(body, sizeof(body),
                                     "{\"alertId\":\"%s\",\"status\":\"START\"}", alert_token);
                            lguplus_event_queue_add_body(LGUP_ALERT, LGUP_ALERT_STATUS, NULL, NULL,
                                                         body);
                        }
                    } else if (StrInclude(recv_buf, EVENT_ALERT_STOP)) {
                        char *alert_token = recv_buf + strlen(EVENT_ALERT_STOP) + 1;
                        char *alert_type = strchr(alert_token, ':');
                        *alert_type = '\0';
                        alert_type += 1;

                        ret = lguplus_check_alert_inside(alert_token);
                        if (ret) {
                            cmd_add_alert_event(NAME_ALERT_STOPPED, alert_token, alert_type);
                        } else {
                            char body[128] = {0};
                            snprintf(body, sizeof(body), "{\"alertId\":\"%s\",\"status\":\"STOP\"}",
                                     alert_token);
                            lguplus_event_queue_add_body(LGUP_ALERT, LGUP_ALERT_STATUS, NULL, NULL,
                                                         body);
                        }

                    } else if (StrInclude(recv_buf, EVENT_DEL_ALERT_OK)) {
                        char *alert_token = recv_buf + strlen(EVENT_DEL_ALERT_OK) + 1;
                        char *alert_type = strchr(alert_token, ':');
                        *alert_type = '\0';
                        alert_type += 1;
                        size_t token_len = alert_type - alert_token - 1 /*:*/;
                        if (token_len > 0) {
                            cmd_add_alert_event(NAME_DELETE_ALERT_SUCCEEDED, alert_token,
                                                alert_type);
                        }
                    } else if (StrInclude(recv_buf, EVENT_ALERT_LOCAL_DELETE)) {
                        char *alert_token = recv_buf + strlen(EVENT_ALERT_LOCAL_DELETE) + 1;
                        char *alert_type = strchr(alert_token, ':');
                        *alert_type = '\0';
                        alert_type += 1;
                        ret = lguplus_check_alert_inside(alert_token);
                        if (ret) {
                            cmd_add_alert_event(NAME_ALERT_STOPPED, alert_token, alert_type);
                        } else {
                            char body[128] = {0};
                            snprintf(body, sizeof(body), "{\"alertId\":\"%s\",\"status\":\"STOP\"}",
                                     alert_token);
                            lguplus_event_queue_add_body(LGUP_ALERT, LGUP_ALERT_STATUS, NULL, NULL,
                                                         body);
                            lguplus_check_alert_del(alert_token);
                        }
                    } else if (StrInclude(recv_buf, EVENT_PLAY_NEXT)) {
                        int client_type = asr_player_get_client_type();
                        if (client_type == e_client_clova) {
                            cmd_add_playback_event(NAME_NEXTCOMMANDISSUED);
                        } else if (client_type == e_client_lguplus) {
                            lguplus_event_queue_add(LGUP_BUTTON, LGUP_BUTTON_NEXT, NULL, NULL);
                        }
                    } else if (StrInclude(recv_buf, EVENT_PLAY_PREV)) {
                        int client_type = asr_player_get_client_type();
                        if (client_type == e_client_clova) {
                            cmd_add_playback_event(NAME_PREVIOUSCOMMANDISSUED);
                        } else if (client_type == e_client_lguplus) {
                            lguplus_event_queue_add(LGUP_BUTTON, LGUP_BUTTON_PREV, NULL, NULL);
                        }
                    } else if (StrInclude(recv_buf, EVENT_PLAY_PAUSE)) {
                        asr_session->inactive_timer = tickSincePowerOn();
                        int client_type = asr_player_get_client_type();
                        if (client_type == e_client_clova) {
                            char *stop_tts = recv_buf + strlen(EVENT_PLAY_PAUSE) + 1;
                            if (stop_tts && atoi(stop_tts) == 1) {
                                clova_set_explicit(0);
                                clova_clear_extension_state();
                                asr_player_buffer_stop(NULL, e_asr_stop);
                            } else {
                                cmd_add_playback_event(NAME_PAUSECOMMANDISSUED);
                            }
                        } else if (client_type == e_client_lguplus) {
                            lguplus_event_queue_add(LGUP_BUTTON, LGUP_BUTTON_PAUSE, NULL, NULL);
                        }
                    } else if (StrInclude(recv_buf, EVENT_PLAY_PLAY)) {
                        asr_session->inactive_timer = tickSincePowerOn();
                        int client_type = asr_player_get_client_type();
                        if (client_type == e_client_clova) {
                            cmd_add_playback_event(NAME_PLAYCOMMANDISSUED);
                        } else if (client_type == e_client_lguplus) {
                            lguplus_event_queue_add(LGUP_BUTTON, LGUP_BUTTON_PLAY, NULL, NULL);
                        }
                    } else if (StrInclude(recv_buf, EVENT_MUTE_CHANGED)) {

                    } else if (StrInclude(recv_buf, EVENT_VOLUME_CHANGED)) {

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
                            asr_logout();
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
                        asr_player_buffer_stop(NULL, e_asr_stop);
                        asr_logout();
                        UnixSocketServer_write(&msg_socket, CMD_OK, strlen(CMD_OK));
                    } else if (StrInclude(recv_buf, ASR_POWEROFF)) {
                        asr_player_buffer_stop(NULL, e_asr_stop);
                        asr_player_force_stop();
                        asr_authorization_logout(0);
                        set_asr_talk_start(0);
                        UnixSocketServer_write(&msg_socket, CMD_OK, strlen(CMD_OK));
                    } else if (StrInclude(recv_buf, ASR_SET_LAN)) {
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
                    }
                    // Normal command, not the asr events
                    else if (StrInclude(recv_buf, ASR_EXIT)) {
                        asr_player_force_stop();
                        set_asr_talk_start(0);
                    } else if (StrInclude(recv_buf, ASR_SET_ENDPOINT)) {
                        char *end_point = NULL;
                        size_t point_len = strlen(recv_buf) - strlen(ASR_SET_ENDPOINT) - 1 /*:*/;
                        if (point_len > 0) {
                            end_point = recv_buf + strlen(ASR_SET_ENDPOINT) + 1;
                            ret = asr_set_endpoint(end_point);
                            if (0 == ret) {
                                UnixSocketServer_write(&msg_socket, CMD_OK, strlen(CMD_OK));
                            } else {
                                UnixSocketServer_write(&msg_socket, CMD_FAILED, strlen(CMD_FAILED));
                            }
                            asr_session_disconncet(ASR_CLOVA);
                        }
                    } else if (StrInclude(recv_buf, ASR_WPS_START)) {
                        h2_reestablish = 1;
                        asr_clear_auth_info(0);
                        asr_session->asr_info[e_asr_clova].state = e_state_none;
                        asr_session->asr_info[e_asr_lguplus].state = e_state_none;
                        asr_session_connect_destroy(asr_session, e_asr_clova);
                        asr_session_connect_destroy(asr_session, e_asr_lguplus);
                    } else if (StrInclude(recv_buf, ASR_WPS_END)) {
                        lguplus_authorization_read_cfg();
                        asr_authorization_read_cfg();
                    } else if (StrInclude(recv_buf, ASR_INACTIVE_REPORT)) {
                        lguplus_event_queue_add(LGUP_SYSTEM, LGUP_INACTIVE_TIME, NULL, NULL);
                    } else {
                        ASR_PRINT(ASR_DEBUG_ERR, "unknow cmd\n");
                    }
                }
                ASR_PRINT(ASR_DEBUG_INFO, "asr event recv at %lld ------\n", tickSincePowerOn());
                UnixSocketServer_close(&msg_socket);
            }
        }
    }
}

int asr_session_init(void)
{
    int ret = 0;

    asr_set_debug_level(ASR_DEBUG_TRACE | ASR_DEBUG_ERR);
    ASR_PRINT(ASR_DEBUG_INFO, "start\n");

    memset(&asr_session, 0x0, sizeof(asr_session_t));

    asr_session.ring_buffer = RingBufferCreate(0);
    if (!asr_session.ring_buffer) {
        ASR_PRINT(ASR_DEBUG_ERR, "error\n");
        return -1;
    }

    ret = asr_authorization_init();
    if (ret) {
        ASR_PRINT(ASR_DEBUG_ERR, "asr_authorization_init failed\n");
        return -1;
    }

    asr_system_init();
    asr_player_init();
    asr_notificaion_init();
    asr_bluetooth_init();
    // clova_settings_init();
    lguplus_alert_list_init();

    asr_session.evnet_queue[e_asr_clova] = asr_queue_init();
    asr_session.evnet_queue[e_asr_lguplus] = lguplus_event_queue_init();
    asr_session.is_running = 1;

    /*
     *  create a thread to exec EVENTS that need to send to asr server
     */
    ret = pthread_create(&asr_session.asr_event_pid, NULL, asr_msg_recv, (void *)&asr_session);
    if (ret) {
        ASR_PRINT(ASR_DEBUG_ERR, "pthread_create asr_msg_recv failed\n");
        return -1;
    }

    /*
     * create a thread to ping asr server
     */
    ret = pthread_create(&asr_session.asr_cmd_pid, NULL, asr_msg_parse, (void *)&asr_session);
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
    asr_session.is_running = 0;

    if (asr_session.asr_event_pid > 0) {
        pthread_join(asr_session.asr_event_pid, NULL);
    }
    if (asr_session.asr_cmd_pid > 0) {
        pthread_join(asr_session.asr_cmd_pid, NULL);
    }

    if (asr_session.ring_buffer) {
        RingBufferDestroy(&asr_session.ring_buffer);
        asr_session.ring_buffer = NULL;
    }

    asr_notificaion_uninit();
    asr_system_uninit();
    asr_player_uninit();
    asr_bluetooth_uninit();
    asr_queue_uninit();
    // clova_settings_uninit();
    lguplus_event_queue_uninit();

    ASR_PRINT(ASR_DEBUG_INFO, "exit\n");

    return 0;
}

int asr_session_start(int req_type)
{
    int ret = -1;
    char *buf = NULL;
    asprintf(&buf, "%s:%d", ASR_RECO_START, req_type);
    if (buf) {
        ASR_PRINT(ASR_DEBUG_INFO, "buf=%s\n", buf);
        ret = local_sock_send(ASR_LOCAL_SOCKET, buf, strlen(buf));
        free(buf);
        buf = NULL;
    }

    if (ret <= 0) {
        ASR_PRINT(ASR_DEBUG_ERR, "talk to stop %d\n", ERR_ASR_EXCEPTION);
        asprintf(&buf, TLK_STOP_N, ERR_ASR_EXCEPTION);
        if (buf) {
            NOTIFY_CMD(ASR_TTS, buf);
            free(buf);
        }
        common_focus_manage_clear(e_focus_recognize);
    }

    return ret;
}

int asr_session_disconncet(int asr_type)
{
    int ret = -1;
    char *buf = NULL;
    asprintf(&buf, "%s:%d", ASR_DIS_CONNECT, asr_type);
    if (buf) {
        ASR_PRINT(ASR_DEBUG_INFO, "buf=%s\n", buf);
        ret = local_sock_send(ASR_LOCAL_SOCKET, buf, strlen(buf));
        free(buf);
    }

    return ret;
}

int asr_session_ping(int asr_type)
{
    int ret = -1;
    char *buf = NULL;

    if (get_internet_flags()) {
        asprintf(&buf, "%s:%d", ASR_PING, asr_type);
        if (buf) {
            ASR_PRINT(ASR_DEBUG_INFO, "buf=%s\n", buf);
            ret = local_sock_send(ASR_LOCAL_SOCKET, buf, strlen(buf));
            free(buf);
        }
    }

    return ret;
}

int asr_session_inactive_report(void)
{
    int ret = -1;

    if (get_internet_flags()) {
        ret = SocketClientReadWriteMsg(ASR_EVENT_PIPE, ASR_INACTIVE_REPORT,
                                       strlen(ASR_INACTIVE_REPORT), NULL, NULL, 0);
    }

    return ret;
}

void asr_session_record_buffer_reset(void)
{
    // init the circle buffer for record data
    RingBufferReset(asr_session.ring_buffer);
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
        int write_len = RingBufferWrite(asr_session.ring_buffer, data, less_len);
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
int asr_session_record_buffer_end(void)
{
    return RingBufferSetFinished(asr_session.ring_buffer, 1);
}

/*
 * stop write record data by other
 */
int asr_session_record_buffer_stop(void) { return RingBufferSetStop(asr_session.ring_buffer, 1); }

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
        refresh_token = NULL;
    }

    return log_in;
}

int lguplus_check_login(void)
{
    char *device_token = NULL;
    char *platform_token = NULL;
    int log_in = 0;

    device_token = lguplus_get_device_token();
    platform_token = lguplus_get_platform_token();

    if (device_token && platform_token) {
        log_in = 1;
    } else {
        log_in = 0;
    }

    if (device_token) {
        free(device_token);
        device_token = NULL;
    }

    if (platform_token) {
        free(platform_token);
        platform_token = NULL;
    }

    return log_in;
}
