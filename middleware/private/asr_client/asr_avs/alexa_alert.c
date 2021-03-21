/*----------------------------------------------------------------------------
 * Name:    alexa_alert.c
 * Purpose:
 * Version:
 * Note(s):
 *----------------------------------------------------------------------------
 *
 * Copyright (c) 2016 wiimu. All rights reserved.
 *----------------------------------------------------------------------------
 * History:
 *         Revision          2016/05/26        Feng Yong
 *----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>

#include "json.h"
#include "wm_util.h"
#include "alexa_debug.h"
#include "alexa_result_parse.h"
#include "alexa_request_json.h"

#if defined(GE_SECURITY)
#define TOTAL_ALARM 255
#else
#define TOTAL_ALARM 30
#endif

#define ALERT_TOKEN_SIZE 200
#define ALERT_TYPE_SIZE 20

#define EXPIRED_TIME_30MINS (30 * 60)

#define ALARM_TYPE "ALARM"
#define TIMER_TYPE "TIMER"
#define REMINDER_TYPE "REMINDER"

#define CMD_ALERT_SET "ALERT+SET"
#define CMD_ALERT_START "ALERT+START"
#define CMD_ALERT_STOP "ALERT+STOP"
#define CMD_ALERT_DEL "ALERT+DEL"
#define CMD_ALERT_FORE "ALERT+FORE"
#define CMD_ALERT_BACK "ALERT+BACK"
#define CMD_ALERT_STATUS_GET "ALERT+STATUS+GET"
#define CMD_ALERT_ALL_GET "ALERT+ALL+GET"

#define ACK_ALERT_DELETE "ALERT+DELETE+ACK"

#define ALERT_TOKEN_HEAD "alert_token="
#define ALERT_TYPE_HEAD "alert_type="
#define ALERT_TIME_HEAD "alert_time="

#define ALERT_NAMED_TIMER_AND_REMINDER_HEAD "alert_NamedTimerAndReminder="

#define ALERT_ID_HEAD "alert_id="
#define ALERT_STATUS_HEAD "alert_status="
#define ALERT_REPEAT_HEAD "alert_repeat="

#define ALERT_COMMON_TONECOUNTSHM "toneCountSHM="
#define ALERT_COMMON_TONEIDSHM "toneIdSHM="
#define ALERT_COMMON_PREWAKESHM "preWakeSHM="
#define ALERT_COMMON_VOLUMESHM "volumeSHM="
#define ALERT_ALL_HEAD "alert_all:"
#define ALERT_ACTIVE_HEAD "alert_active:"
#define ALERT_NUM "alert_num="
#define ALERT_COMMON "alert_a"
#define CMD_ALERT_LOCAL_DEL "ALERT+LOCAL+DEL"

#define STANDARD_TIMER_FORMAT "2016-06-06T06:36:33+0000"

#define ALERT_SHM_FTOK_PATH "/etc"
#define ALERT_SHM_ID 32429
#define ALERT_SEMPHORE_ID 32312
#define ALERT_SHM_SIZE TOTAL_ALARM * 512

extern WIIMU_CONTEXT *g_wiimu_shm;

// only access during init, need not lock and extern
static ALARM_NODE_T *alarm_head = NULL;
static int num_of_alarm = 0;
static int num_of_timer = 0;

char *alert_shm = NULL;

static char *alert_shared_memory_create(int shm_size);
static int alarm_list_init(void);
static int alert_all_send(ALARM_NODE_T *palarm_head, char *psend_buf);
static int alarm_list_insert(ALARM_NODE_T *alarm_head, char *ptoken, char *ptype, char *pschtime);

/********************************************************************
*   Function:   alarm_list_insert
*   Description:
*       Insert a node to alarm list
*   Arguments:
*       ALARM_NODE_T *palarm_head   the head of alarm list
*       char *ptoken        the pointer of token insert
*       char *pschtime      the pointer of pschtime inster
*   Return:
*           int        0:succeed or else failed
*   History:
*           Revision    2016/06/04      Feng Yong
*********************************************************************/

static int alarm_list_insert(ALARM_NODE_T *alarm_head, char *ptoken, char *ptype, char *pschtime)
{
    ALARM_NODE_T *tmp_node = NULL;
    // time_t t = 0;
    // time_t now_t = time(NULL);
    // printf("### %s line:%d ###\n",__FUNCTION__,__LINE__);

    if (NULL == alarm_head)
        return -1;

    tmp_node = (ALARM_NODE_T *)malloc(sizeof(ALARM_NODE_T));
    if (NULL == tmp_node)
        return -1;

    tmp_node->ptoken = (char *)malloc(strlen(ptoken) + 1);
    if (NULL == tmp_node->ptoken)
        goto err1;

    tmp_node->ptype = (char *)malloc(strlen(ptype) + 1);
    if (NULL == tmp_node->ptype)
        goto err2;

    tmp_node->pschtime = (char *)malloc(strlen(pschtime) + 1);
    if (NULL == tmp_node->pschtime)
        goto err3;

    tmp_node->next = NULL;

    memcpy(tmp_node->ptoken, ptoken, strlen(ptoken));
    tmp_node->ptoken[strlen(ptoken)] = '\0';
    memcpy(tmp_node->ptype, ptype, strlen(ptype));
    tmp_node->ptype[strlen(ptype)] = '\0';
    memcpy(tmp_node->pschtime, pschtime, strlen(pschtime));
    tmp_node->pschtime[strlen(pschtime)] = '\0';
    tmp_node->play_start_ts = 0;

    // insert new note to list
    if (NULL == alarm_head->next) {
        alarm_head->next = tmp_node;
    } else {
        tmp_node->next = alarm_head->next;
        alarm_head->next = tmp_node;
    }

    return 0;

err3:
    free(tmp_node->ptype);
err2:
    free(tmp_node->ptoken);
err1:
    free(tmp_node);

    return -1;
}

static int alert_all_send(ALARM_NODE_T *palarm_head, char *psend_buf)
{
    int num_of_alert = 0;
    char tmp_buf[1024] = {0};

    // printf("### %s line: %d ###\n",__FUNCTION__,__LINE__);
    if (NULL == alarm_head)
        return -1;

    num_of_alert = num_of_alarm + num_of_timer;
    *psend_buf = '0';
    psend_buf = psend_buf + 1; // the first byte for voice status
    snprintf(psend_buf, ALERT_SHM_SIZE, ALERT_NUM "%2d", num_of_alert);
    psend_buf = psend_buf + strlen(ALERT_NUM); // skip ALERT_NUM
    psend_buf = psend_buf + 2;
    memset(psend_buf, 0, ALERT_SHM_SIZE - 3 - strlen(ALERT_NUM));
    if (0 == num_of_alert)
        return -1;

    palarm_head = palarm_head->next;
    while (NULL != palarm_head) {
        memset(tmp_buf, 0, sizeof(ALERT_TOKEN_SIZE));
        snprintf(tmp_buf, sizeof(tmp_buf),
                 ALERT_ALL_HEAD ALERT_TOKEN_HEAD "%s" ALERT_TYPE_HEAD "%s" ALERT_TIME_HEAD "%s",
                 palarm_head->ptoken, palarm_head->ptype, palarm_head->pschtime);
        strcat(psend_buf, tmp_buf);
        palarm_head = palarm_head->next;
    }

    return 0;
}

/********************************************************************
*   Function:   alarm_list_init
*   Description:
*       INIT alarm list
*   Arguments:
*       None
*   Return:
*           int        0:succeed or else failed
*   History:
*           Revision    2016/06/04      Feng Yong
*********************************************************************/

static int alarm_list_init(void)
{
    int cnt_alarm = 0;
    int cnt_timer = 0;
    char tmp_token_buf[ALERT_TOKEN_SIZE] = {0};
    char tmp_type_buf[ALERT_TYPE_SIZE] = {0};
    char tmp_pschtime_buf[50] = {0};

    FILE *fp;
    printf("### %s line:%d ###\n", __FUNCTION__, __LINE__);

    if ((fp = fopen(ALARM_FILE, "a+")) == NULL)
        return -1;

    // Init the head-node
    alarm_head = (ALARM_NODE_T *)malloc(sizeof(ALARM_NODE_T));
    if (!alarm_head) {
        fclose(fp);
        return -1;
    }
    memset((void *)alarm_head, 0, sizeof(ALARM_NODE_T));

    while (1) {
        memset(tmp_token_buf, 0, sizeof(tmp_token_buf));
        memset(tmp_type_buf, 0, sizeof(tmp_type_buf));
        memset(tmp_pschtime_buf, 0, sizeof(tmp_pschtime_buf));
        if (fgets(tmp_token_buf, sizeof(tmp_token_buf), fp) == NULL) { // get token
            if (feof(fp) != 0) {                                       // get the end of file
                tmp_token_buf[strlen(tmp_token_buf)] = '\0';
                if (strlen(tmp_token_buf) > 0) {
                    if (tmp_token_buf[strlen(tmp_token_buf) - 1] == '\n')
                        tmp_token_buf[strlen(tmp_token_buf) - 1] = '\0';
                }
                break;
            } else {
                goto invalid_file;
            }
        }
        tmp_token_buf[strlen(tmp_token_buf)] = '\0';
        if (strlen(tmp_token_buf) > 0) {
            if (tmp_token_buf[strlen(tmp_token_buf) - 1] == '\n')
                tmp_token_buf[strlen(tmp_token_buf) - 1] = '\0';
        }

        if (fgets(tmp_type_buf, sizeof(tmp_type_buf), fp) == NULL) { // get typoe
            if (feof(fp) != 0) {                                     // get the end of file
                tmp_type_buf[strlen(tmp_type_buf)] = '\0';
                if (strlen(tmp_type_buf) > 0) {
                    if (tmp_type_buf[strlen(tmp_type_buf) - 1] == '\n')
                        tmp_type_buf[strlen(tmp_type_buf) - 1] = '\0';
                }
                break;
            } else {
                goto invalid_file;
            }
        }
        tmp_type_buf[strlen(tmp_type_buf)] = '\0';
        if (strlen(tmp_type_buf) > 0) {
            if (tmp_type_buf[strlen(tmp_type_buf) - 1] == '\n')
                tmp_type_buf[strlen(tmp_type_buf) - 1] = '\0';
        }

        if (fgets(tmp_pschtime_buf, sizeof(tmp_pschtime_buf), fp) == NULL) { // get time
            if (feof(fp) != 0) {                                             // get the end of file
                tmp_pschtime_buf[strlen(tmp_pschtime_buf)] = '\0';
                if (strlen(tmp_pschtime_buf) > 0) {
                    if (tmp_pschtime_buf[strlen(tmp_pschtime_buf) - 1] == '\n')
                        tmp_pschtime_buf[strlen(tmp_pschtime_buf) - 1] = '\0';
                }
                break;
            } else {
                goto invalid_file;
            }
        }
        tmp_pschtime_buf[strlen(tmp_pschtime_buf)] = '\0';
        if (strlen(tmp_pschtime_buf) > 0) {
            if (tmp_pschtime_buf[strlen(tmp_pschtime_buf) - 1] == '\n')
                tmp_pschtime_buf[strlen(tmp_pschtime_buf) - 1] = '\0';
        }

        // TODO: need to check the file in case it is broken, just check alarm type here
        if (strlen(tmp_token_buf) == 0 || strlen(tmp_type_buf) == 0 ||
            strlen(tmp_pschtime_buf) == 0) {
            goto invalid_file;
        }
        if (0 == alarm_list_insert(alarm_head, tmp_token_buf, tmp_type_buf, tmp_pschtime_buf)) {
            if (0 == strncmp(tmp_type_buf, ALARM_TYPE, strlen(ALARM_TYPE))) {
                cnt_alarm++;
            } else {
                cnt_timer++;
            }
        }
    }

    num_of_alarm = cnt_alarm;
    num_of_timer = cnt_timer;
    fclose(fp);

    return 0;

invalid_file:
    fclose(fp);
    return -1;
}

/********************************************************************
*   Function:   alert_shared_memory_create
*   Description:
*       Create size-settd shared memory
*   Arguments:
*       int shm_size       size of shared memory
*   Return:
*           char *         NULL:failed to get shared memory
*                       or else succeed
*   History:
*           Revision    2016/06/12      Feng Yong
*********************************************************************/

static char *alert_shared_memory_create(int shm_size)
{
    key_t sock_key;
    char *shmem = NULL;

    sock_key = ftok(ALERT_SHM_FTOK_PATH, 's');
    if (sock_key <= 0) {
        if (sock_key == 0)
            sock_key = (key_t)ALERT_SHM_ID;
        else {
            return NULL;
        }
    }

    int shmid = shmget(sock_key, shm_size, 0666 | IPC_CREAT | IPC_EXCL);
    if (shmid != -1) {
        shmem = shmat(shmid, (const void *)0, 0);
    } else {
        if (EEXIST == errno || EINVAL == errno) {
            shmid = shmget(sock_key, 0, 0);
            if (shmid != -1)
                shmem = shmat(shmid, (const void *)0, 0);
        }
    }
    if (shmem == 0) {
        return NULL;
    }

    return shmem;
}

int alert_init(void)
{
    alert_shm = alert_shared_memory_create(ALERT_SHM_SIZE);

    if (NULL == alert_shm)
        printf("failed to get shared memory\n");
    else
        memset((void *)alert_shm, 0, ALERT_SHM_SIZE);

    if (-1 == alarm_list_init()) {
        printf("alarm_list_init failed\n");
    }

    // refresh shared memory
    focus_process_lock();
    alert_all_send(alarm_head, alert_shm);
    focus_process_unlock();

    // alert_list_destroy(alarm_head);

    return 0;
}

int alert_get_all_alert(json_object *pjson_head, json_object *pjson_active_head)
{
    int ret = -1;
    char *recv_buf = NULL;
    int buf_len = TOTAL_ALARM * 512;
    char num_buf[3] = {0};
    int num_of_alert = 0;
    int cnt = 0;
    char *precv_buf = NULL;
    char time_buf[30] = {0};
    int active_flag = 0;
#if defined(IHOME_ALEXA)
    char *tmp_alert_common = NULL;
#endif

    recv_buf = calloc(1, buf_len);
    if (!recv_buf) {
        ret = -1;
        goto exit;
    }

    focus_process_lock();
    memcpy(recv_buf, alert_shm, ALERT_SHM_SIZE);
    focus_process_unlock();

    char *num = strstr(recv_buf, ALERT_NUM);
    if (NULL == num) {
        ret = -1;
        goto exit;
    }
    num += strlen(ALERT_NUM);
    strncat(num_buf, num, 2);
    num_of_alert = atoi(num_buf);
    if (0 == num_of_alert) {
        ret = -1;
        goto exit;
    }

    precv_buf = recv_buf;
    while (cnt < num_of_alert) {
        memset(time_buf, 0, sizeof(time_buf));
        // the first string is "alert_all:" or "alert_active:"
        char *pactive = strstr(precv_buf, ALERT_COMMON);
        char *ptoken = strstr(precv_buf, ALERT_TOKEN_HEAD);
        char *ptype = strstr(precv_buf, ALERT_TYPE_HEAD);
        char *pschtime = strstr(precv_buf, ALERT_TIME_HEAD);
#if defined(IHOME_ALEXA)
        char *pID = strstr(precv_buf, ALERT_ID_HEAD);
        char *pstatus = strstr(precv_buf, ALERT_STATUS_HEAD);
        char *prepeat = strstr(precv_buf, ALERT_REPEAT_HEAD);
        char *mark_end = NULL;
#endif
        if (pactive && (0 == strncmp(pactive, ALERT_ACTIVE_HEAD, strlen(ALERT_ACTIVE_HEAD))))
            active_flag = 1;
        else
            active_flag = 0;

#if defined(IHOME_ALEXA)
        if (ptoken && ptype && pschtime && prepeat)
#else
        if (ptoken && ptype && pschtime)
#endif
        {
            ptoken += strlen(ALERT_TOKEN_HEAD);
            precv_buf[ptype - precv_buf] = '\0';
            ptype += strlen(ALERT_TYPE_HEAD);
            precv_buf[pschtime - precv_buf] = '\0';
            pschtime += strlen(ALERT_TIME_HEAD);
            strncat(time_buf, pschtime, strlen(STANDARD_TIMER_FORMAT));
            precv_buf = pschtime + strlen(STANDARD_TIMER_FORMAT);
#if defined(IHOME_ALEXA)
            precv_buf[pID - precv_buf] = '\0';

            pID += strlen(ALERT_ID_HEAD);
            precv_buf[pstatus - precv_buf] = '\0';
            pstatus += strlen(ALERT_STATUS_HEAD);
            precv_buf[prepeat - precv_buf] = '\0';

            prepeat += strlen(ALERT_REPEAT_HEAD);
            mark_end = strstr(prepeat, ALERT_COMMON);
            if (mark_end) {
                strncpy(time_buf, prepeat, mark_end - prepeat);
            } else {
                mark_end = strstr(prepeat, ALERT_COMMON_TONECOUNTSHM);
                if (mark_end)
                    strncpy(time_buf, prepeat, mark_end - prepeat);
            }
            precv_buf = prepeat;
            tmp_alert_common = prepeat;
#endif
        } else {
            ret = -1;
            goto exit;
        }
#if defined(IHOME_ALEXA)
        if (active_flag)
            alexa_alerts_list_add(pjson_active_head, ptoken, ptype, pschtime);

        alexa_alerts_list_add(pjson_head, ptoken, ptype, pschtime);
#else
        if (active_flag)
            alexa_alerts_list_add(pjson_active_head, ptoken, ptype, time_buf);

        alexa_alerts_list_add(pjson_head, ptoken, ptype, time_buf);

#endif
        cnt++;
    }

exit:
    if (recv_buf) {
        free(recv_buf);
    }

    return ret;
}

#if defined(IHOME_ALEXA)

int alert_get_for_app(SOCKET_CONTEXT *pmsg_socket, char *pbuf)
{
    int ret = -1;
    json_object *json_all_alert_list = NULL;
    json_object *json_alert_send = NULL;
    char *recv_buf = NULL;
    char num_buf[3] = {0};
    int num_of_alert = 0;
    int cnt = 0;
    char *precv_buf = NULL;
    char time_buf[30] = {0};
    int active_flag = 0;
    char *tmp_alert_common = NULL;
    char *tmp_json_string = NULL;

    recv_buf = (char *)calloc(1, TOTAL_ALARM * 512);
    if (!recv_buf) {
        ret = -1;
        goto exit;
    }

    json_all_alert_list = json_object_new_array();
    if (!json_all_alert_list) {
        ret = -1;
        goto exit;
    }

    focus_process_lock();
    memcpy(recv_buf, alert_shm, ALERT_SHM_SIZE);
    focus_process_unlock();

    char *num = strstr(recv_buf, ALERT_NUM);
    /*modify by Weiqiang.huang for STAB-219 2018-06-22 -- begin*/
    if (NULL == num) {
        ret = -1;
        goto exit;
    }
    /*modify by Weiqiang.huang for STAB-219 2018-06-22 -- end*/
    num += strlen(ALERT_NUM);
    strncat(num_buf, num, 2);
    num_of_alert = atoi(num_buf);

    precv_buf = recv_buf;
    tmp_alert_common = recv_buf;
    while (cnt < num_of_alert) {
        // memset(time_buf,0,sizeof(time_buf));
        char *pactive = strstr(precv_buf, ALERT_COMMON);
        // char *ptoken = strstr(precv_buf, ALERT_TOKEN_HEAD);
        char *ptype = strstr(precv_buf, ALERT_TYPE_HEAD);
        char *pschtime = strstr(precv_buf, ALERT_TIME_HEAD);
        char *pID = strstr(precv_buf, ALERT_ID_HEAD);
        char *pstatus = strstr(precv_buf, ALERT_STATUS_HEAD);
        char *prepeat = strstr(precv_buf, ALERT_REPEAT_HEAD);
        char *mark_end = NULL;
        if (0 == strncmp(pactive, ALERT_ALL_HEAD, strlen(ALERT_ALL_HEAD)))
            active_flag = 0;
        else
            active_flag = 1;

        if (ptype && pschtime && prepeat) {
            if (0 == strncmp(pactive, ALERT_ALL_HEAD, strlen(ALERT_ALL_HEAD)))
                pactive += strlen(ALERT_ALL_HEAD);
            else
                pactive += strlen(ALERT_ACTIVE_HEAD);
            ptype += strlen(ALERT_TYPE_HEAD);
            precv_buf[pschtime - precv_buf] = '\0';
            pschtime += strlen(ALERT_TIME_HEAD);
            precv_buf[pID - precv_buf] = '\0';

            pID += strlen(ALERT_ID_HEAD);
            precv_buf[pstatus - precv_buf] = '\0';
            pstatus += strlen(ALERT_STATUS_HEAD);
            precv_buf[prepeat - precv_buf] = '\0';

            prepeat += strlen(ALERT_REPEAT_HEAD);
            mark_end = strstr(prepeat, ALERT_COMMON);
            if (mark_end) {
                strncpy(time_buf, prepeat, mark_end - prepeat);
            } else {
                mark_end = strstr(prepeat, ALERT_COMMON_TONECOUNTSHM);
                if (mark_end)
                    strncpy(time_buf, prepeat, mark_end - prepeat);
            }
            precv_buf = prepeat;
            tmp_alert_common = prepeat;
            AlexaRequestAppListAdd(json_all_alert_list, pID, pschtime, ptype, pstatus, time_buf);
        } else {
            break;
        }
        cnt++;
    }

    // get alert common  status
    char *ptonecount = strstr(tmp_alert_common, ALERT_COMMON_TONECOUNTSHM);
    char *ptoneid = strstr(tmp_alert_common, ALERT_COMMON_TONEIDSHM);
    char *pprewake = strstr(tmp_alert_common, ALERT_COMMON_PREWAKESHM);
    char *pvolume = strstr(tmp_alert_common, ALERT_COMMON_VOLUMESHM);

    if (ptonecount && ptoneid && pprewake && pvolume) {
        ptonecount += strlen(ALERT_COMMON_TONECOUNTSHM);
        precv_buf[ptoneid - tmp_alert_common] = '\0';
        ptoneid += strlen(ALERT_COMMON_TONEIDSHM);
        precv_buf[pprewake - tmp_alert_common] = '\0';
        pprewake += strlen(ALERT_COMMON_PREWAKESHM);
        precv_buf[pvolume - tmp_alert_common] = '\0';
        pvolume += strlen(ALERT_COMMON_VOLUMESHM);
        json_alert_send =
            AlexaAlertForsApp(json_all_alert_list, ptonecount, ptoneid, pprewake, pvolume);
        tmp_json_string = ((char *)json_object_to_json_string(json_alert_send));

        memcpy(pbuf, tmp_json_string, strlen(tmp_json_string));
    } else {
        printf("alert_get_for_app error\n");
    }

exit:
    /*modify by Weiqiang.huang for STAB-219 2018-06-22 -- begin*/
    if (json_alert_send) {
        json_object_put(json_alert_send);
    } else if (json_all_alert_list) {
        json_object_put(json_all_alert_list);
    }
    /*modify by Weiqiang.huang for STAB-219 2018-06-22 -- end*/
    if (recv_buf) {
        free(recv_buf);
    }
    return 0;
}

int alert_common_for_app(char *cmd)
{
    int tmp_ret = 0;
    tmp_ret = SocketClientReadWriteMsg(ALERT_SOCKET_NODE, cmd, strlen(cmd), NULL, NULL, 1);
    return 0;
}

#endif

int alert_set(char *ptoken, char *ptype, char *pschtime)
{
    char buf[2048] = {0};
    char ret[32] = {0};
    int outlen = 0;
    snprintf(buf, sizeof(buf),
             CMD_ALERT_SET ALERT_TOKEN_HEAD "%s" ALERT_TYPE_HEAD "%s" ALERT_TIME_HEAD "%s", ptoken,
             ptype, pschtime);
    SocketClientReadWriteMsg(ALERT_SOCKET_NODE, buf, strlen(buf), ret, &outlen, 1);
    if (outlen > 0 && strcmp("OK", ret) == 0)
        return 0;

    return -1;
}

int alert_set_ForNamedTimerAndReminder(char *ptoken, char *ptype, char *pschtime,
                                       const char *json_string)
{
    // char buf[2048] = {0};
    char *buf = NULL;
    char ret[32] = {0};
    int outlen = 0;

    asprintf(&buf, CMD_ALERT_SET ALERT_TOKEN_HEAD "%s" ALERT_TYPE_HEAD "%s" ALERT_TIME_HEAD
                                                  "%s" ALERT_NAMED_TIMER_AND_REMINDER_HEAD "%s",
             ptoken, ptype, pschtime, json_string);

    if (!buf) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR,
                    "In fuc 'alert_set_ForNamedTimerAndReminder_Json', buf malloc failed!\n");
        return -1;
    }

    // ALEXA_PRINT(ALEXA_DEBUG_TRACE, "sizeof(json_string)=%d, strlen(json_string)=%d,
    // strlen(buf)=%d, sizeof(buf)=%d, buf=%s\n", sizeof(json_string), strlen(json_string),
    // strlen(buf), sizeof(buf), buf);

    SocketClientReadWriteMsg(ALERT_SOCKET_NODE, buf, strlen(buf), ret, &outlen, 1);
    if (buf) {
        free(buf);
        buf = NULL;
    }

    if (outlen > 0 && strcmp("OK", ret) == 0)
        return 0;

    return -1;
}

int alert_status_get(void)
{
    char tmp_buf[2] = {0};

    memset(tmp_buf, 0, sizeof(tmp_buf));
    focus_process_lock();
    memcpy(tmp_buf, alert_shm, 1);
    focus_process_unlock();
    if (atoi(tmp_buf) > 0)
        return 1;
    else
        return 0;
}

void alert_start(char *ptoken)
{
    char buf[1024];
    snprintf(buf, sizeof(buf), CMD_ALERT_START ALERT_TOKEN_HEAD "%s", ptoken);
    SocketClientReadWriteMsg(ALERT_SOCKET_NODE, buf, strlen(buf), NULL, NULL, 0);
}

void alert_stop(char *ptoken)
{
    char buf[1024];
    snprintf(buf, sizeof(buf), CMD_ALERT_STOP ALERT_TOKEN_HEAD "%s", ptoken);
    SocketClientReadWriteMsg(ALERT_SOCKET_NODE, buf, strlen(buf), NULL, NULL, 0);
}

void alert_delete(const char *ptoken)
{
    char buf[1024];
    snprintf(buf, sizeof(buf), CMD_ALERT_DEL ALERT_TOKEN_HEAD "%s", ptoken);
    SocketClientReadWriteMsg(ALERT_SOCKET_NODE, buf, strlen(buf), NULL, NULL, 0);
}

void alert_local_delete(void)
{
    char buf[1024];
    snprintf(buf, sizeof(buf), CMD_ALERT_LOCAL_DEL);
    SocketClientReadWriteMsg(ALERT_SOCKET_NODE, buf, strlen(buf), NULL, NULL, 0);
}

void alert_enter_foreground(char *ptoken)
{
    char buf[1024];
    snprintf(buf, sizeof(buf), CMD_ALERT_FORE ALERT_TOKEN_HEAD "%s", ptoken);
    SocketClientReadWriteMsg(ALERT_SOCKET_NODE, buf, strlen(buf), NULL, NULL, 0);
}

void alert_enter_background(char *ptoken)
{
    char buf[1024];
    snprintf(buf, sizeof(buf), CMD_ALERT_BACK ALERT_TOKEN_HEAD "%s", ptoken);
    SocketClientReadWriteMsg(ALERT_SOCKET_NODE, buf, strlen(buf), NULL, NULL, 0);
}

void alert_delete_ACK(char *ptoken)
{
    char buf[1024];
    snprintf(buf, sizeof(buf), ACK_ALERT_DELETE ALERT_TOKEN_HEAD "%s", ptoken);
    SocketClientReadWriteMsg(ALERT_SOCKET_NODE, buf, strlen(buf), NULL, NULL, 0);
}

void alert_reset(void)
{
    focus_process_lock();
    if (alert_shm) {
        memset(alert_shm, 0x0, ALERT_SHM_SIZE);
    }
    focus_process_unlock();
}
