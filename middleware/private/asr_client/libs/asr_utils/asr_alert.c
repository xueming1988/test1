
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <unistd.h>

#include "json.h"
#include "wm_util.h"
#include "asr_alert.h"
#include "common_flags.h"

#define TOTAL_ALARM 1000

#define ALERT_TOKEN_SIZE 200
#define ALERT_TYPE_SIZE 20

#define ALARM_TYPE "ALARM"
#define TIMER_TYPE "TIMER"
#define REMINDER_TYPE "REMINDER"

#define CMD_ALERT_SET "ALERT+SET"
#define CMD_ALERT_START "ALERT+START"
#define CMD_ALERT_STOP "ALERT+STOP"
#define CMD_ALERT_DEL "ALERT+DEL"
#define CMD_ALERT_CLEAR "ALERT+CLEAR"
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
#define ALARM_FILE_NAMED_TIMER_AND_REMINDER "/vendor/wiimu/clova_alarm_NamedTimerAndReminder"

#define ALERT_SHM_ID 32429
#define ALERT_SEMPHORE_ID 32312
#define ALERT_SHM_SIZE TOTAL_ALARM * 512

char *alert_shm = NULL;

extern void killPidbyName(char *sPidString);
extern int asr_alerts_list_add(json_object *alert_list, char *token, char *type,
                               char *scheduled_time);
extern int asprintf(char **strp, const char *fmt, ...);

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

int alert_start(void)
{
    asr_clear_alert_cfg();

    alert_shm = alert_shared_memory_create(ALERT_SHM_SIZE);

    if (NULL == alert_shm)
        printf("failed to get shared memory\n");

    return 0;
}

int alert_get_all_alert(json_object *pjson_head, json_object *pjson_active_head)
{
    int ret = -1;
    int len = TOTAL_ALARM * 512;
    char *recv_buf = NULL;
    char num_buf[3] = {0};
    int num_of_alert = 0;
    int cnt = 0;
    char *precv_buf = NULL;
    char time_buf[30] = {0};
    int active_flag = 0;

    if (len <= 0) {
        printf("alert_get_all_alert len <= 0\n");
        return -1;
    }

    recv_buf = calloc(1, len);
    if (!recv_buf) {
        printf("calloc failed\n");
        goto exit;
    }

    focus_process_lock();
    memcpy(recv_buf, alert_shm, ALERT_SHM_SIZE);
    focus_process_unlock();

    char *num = strstr(recv_buf, ALERT_NUM);
    if (NULL == num) {
        printf("num is NULL\n");
        goto exit;
    }

    num += strlen(ALERT_NUM);
    strncat(num_buf, num, 2);
    num_of_alert = atoi(num_buf);
    if (0 == num_of_alert) {
        printf("num_of_alert is 0\n");
        goto exit;
    }

    precv_buf = recv_buf;
    while (cnt < num_of_alert) {
        memset(time_buf, 0, sizeof(time_buf));
        char *pactive = strstr(precv_buf, ALERT_ACTIVE_HEAD);
        char *ptoken = strstr(precv_buf, ALERT_TOKEN_HEAD);
        char *ptype = strstr(precv_buf, ALERT_TYPE_HEAD);
        char *pschtime = strstr(precv_buf, ALERT_TIME_HEAD);
        if (pactive && (pactive < ptoken))
            active_flag = 1;
        else
            active_flag = 0;

        if (ptoken && ptype && pschtime) {
            ptoken += strlen(ALERT_TOKEN_HEAD);
            precv_buf[ptype - precv_buf] = '\0';
            ptype += strlen(ALERT_TYPE_HEAD);
            precv_buf[pschtime - precv_buf] = '\0';
            pschtime += strlen(ALERT_TIME_HEAD);
            strncat(time_buf, pschtime, strlen(STANDARD_TIMER_FORMAT));
            precv_buf = pschtime + strlen(STANDARD_TIMER_FORMAT);
        } else {
            goto exit;
        }
        if (active_flag)
            asr_alerts_list_add(pjson_active_head, ptoken, ptype, time_buf);

        asr_alerts_list_add(pjson_head, ptoken, ptype, time_buf);
        cnt++;
    }
    ret = 0;

exit:
    if (recv_buf) {
        free(recv_buf);
    }

    return ret;
}

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

int alert_set_with_json(char *ptoken, char *ptype, char *pschtime, char *json_string)
{
    char *buf = NULL;
    char ret[32] = {0};
    int outlen = 0;

    asprintf(&buf, CMD_ALERT_SET ALERT_TOKEN_HEAD "%s" ALERT_TYPE_HEAD "%s" ALERT_TIME_HEAD
                                                  "%s" ALERT_NAMED_TIMER_AND_REMINDER_HEAD "%s",
             ptoken, ptype, pschtime, json_string);

    if (!buf) {
        printf("In fuc 'alert_set_ForNamedTimerAndReminder_Json', buf malloc failed!\n");
        return -1;
    }

    SocketClientReadWriteMsg(ALERT_SOCKET_NODE, buf, strlen(buf), ret, &outlen, 1);
    if (buf) {
        free(buf);
        buf = NULL;
    }

    if (outlen > 0 && strcmp("OK", ret) == 0)
        return 0;

    return -1;
}

int alert_stop(char *ptoken)
{
    char buf[1024];
    snprintf(buf, sizeof(buf), CMD_ALERT_STOP ALERT_TOKEN_HEAD "%s", ptoken);
    SocketClientReadWriteMsg(ALERT_SOCKET_NODE, buf, strlen(buf), NULL, NULL, 0);
    return 0;
}

int alert_clear(char *ptype)
{
    char buf[1024];
    char ret[32] = {0};
    int outlen = 0;

    snprintf(buf, sizeof(buf), CMD_ALERT_CLEAR ALERT_TYPE_HEAD "%s", ptype);
    SocketClientReadWriteMsg(ALERT_SOCKET_NODE, buf, strlen(buf), ret, &outlen, 1);
    if (outlen > 0 && strcmp("OK", ret) == 0)
        return 0;

    return -1;
}

void alert_stop_current(void)
{
    char buff[64] = {0};

    if (get_asr_alert_status()) {
        SocketClientReadWriteMsg(ALERT_SOCKET_NODE, "ALERT+LOCAL+DEL", strlen("ALERT+LOCAL+DEL"),
                                 NULL, NULL, 0);
        snprintf(buff, sizeof(buff), "/tmp/file_player_%d", e_file_alert);
        killPidbyName(buff);
    }
}

int asr_clear_alert_cfg(void)
{
    char buf[256];

    if ((access(ALARM_FILE_NAMED_TIMER_AND_REMINDER, 0)) == 0) {
        snprintf(buf, sizeof(buf), "rm -fr %s", ALARM_FILE_NAMED_TIMER_AND_REMINDER);
        system(buf);
    }

    if ((access(NAMED_TIMER_AND_REMINDER_AUDIO_PATH, 0)) == 0) {
        snprintf(buf, sizeof(buf), "rm -fr %s", NAMED_TIMER_AND_REMINDER_AUDIO_PATH);
        system(buf);
    }

    if ((access(ALARM_FILE, 0)) == 0) {
        snprintf(buf, sizeof(buf), "rm -rf %s && sync", ALARM_FILE);
        system(buf);
    }

    killPidbyName("clova_alert");
    focus_process_lock();
    if (alert_shm) {
        memset(alert_shm, 0x0, ALERT_SHM_SIZE);
    }
    focus_process_unlock();
    system("clova_alert &");

    return 0;
}
