
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#include "json.h"

#include "asr_debug.h"
#include "asr_system.h"
#include "asr_json_common.h"

#include "clova_dnd.h"
#include "clova_device.h"
#include "common_flags.h"

typedef struct _clova_dnd_s {
    int tm_zone;
    int state;
    int dnd_on;
    time_t start_time_s;
    time_t end_time_s;
    time_t expire_time_s;
    char *start_time;
    char *end_time;
    char *expire_time;
} clova_dnd_t;

static clova_dnd_t g_clova_dnd = {0};
#define HOUR_IN_SECONDS(n) ((n)*60 * 60)
static pthread_mutex_t g_dnd_state_lock = PTHREAD_MUTEX_INITIALIZER;
#define dnd_state_lock() pthread_mutex_lock(&g_dnd_state_lock)
#define dnd_state_unlock() pthread_mutex_unlock(&g_dnd_state_lock)

int clova_dnd_init(void)
{
    dnd_state_lock();
    memset(&g_clova_dnd, 0x0, sizeof(clova_dnd_t));
    g_clova_dnd.state = 1;
    dnd_state_unlock();

    return 0;
}

void clova_dnd_uninit(void)
{
    dnd_state_lock();
    if (g_clova_dnd.start_time) {
        free(g_clova_dnd.start_time);
        g_clova_dnd.start_time = NULL;
    }
    if (g_clova_dnd.end_time) {
        free(g_clova_dnd.end_time);
        g_clova_dnd.end_time = NULL;
    }
    if (g_clova_dnd.expire_time) {
        free(g_clova_dnd.expire_time);
        g_clova_dnd.expire_time = NULL;
    }
    dnd_state_unlock();
}

int clova_get_dnd_on(void)
{
    int dnd_on = 0;

    dnd_state_lock();
    dnd_on = g_clova_dnd.dnd_on;
    dnd_state_unlock();

    return dnd_on;
}

int clova_set_dnd_on(int dnd_on)
{
    int dnd_expire = 0;
    int dnd_scheduled = 0;
    time_t tt;
    tt = time(&tt);

    dnd_state_lock();
    if (dnd_on) {
        printf("!!!!!!!!!!!!!!!!! time is arrived, set the dnd on !!!!!!!!!!!!!!!\n");
        if (g_clova_dnd.state == 1 && g_clova_dnd.dnd_on != 1) {
            g_clova_dnd.dnd_on = 1;
            set_dnd_mode(1);
        }
    } else {
        printf("!!!!!!!!!!!!!!!!! time is arrived, clear the dnd on !!!!!!!!!!!!!!!\n");
        // Check for disable dnd mode
        if (g_clova_dnd.expire_time_s != 0 && g_clova_dnd.expire_time_s <= tt) {
            g_clova_dnd.expire_time_s = 0;
            if (g_clova_dnd.expire_time) {
                free(g_clova_dnd.expire_time);
            }
            g_clova_dnd.expire_time = NULL;
            dnd_expire = 1;
            // printf("------- 1 expire_time clear ---------\n");
        }
        if (g_clova_dnd.state == 1 &&
            (tt >= g_clova_dnd.end_time_s || tt < g_clova_dnd.start_time_s)) {
            dnd_scheduled = 1;
            // printf("------- 2 not in dnd scheduled time ---------\n");
        }
        if ((dnd_scheduled == 1 || g_clova_dnd.state == 0) &&
            (dnd_expire == 1 || g_clova_dnd.expire_time_s <= 0)) {
            if (g_clova_dnd.dnd_on != 0) {
                g_clova_dnd.dnd_on = 0;
                set_dnd_mode(0);
                dnd_expire = 1;
                dnd_scheduled = 1;
                // printf("------- 3 DND clear ---------\n");
            }
        }
        // update the start time and the end time for next day
        if (g_clova_dnd.state == 1 && (tt >= g_clova_dnd.end_time_s)) {
            g_clova_dnd.start_time_s += HOUR_IN_SECONDS(24);
            g_clova_dnd.end_time_s += HOUR_IN_SECONDS(24);
            // printf("------- 4 start time and end time update ---------\n");
            debug_printf();
        }
    }
    dnd_state_unlock();

    if (dnd_scheduled == 1 && dnd_expire == 1) {
        cmd_add_normal_event(NAMESPACE_NOTIFIER, NAME_RequestIndicator, NULL);
    }

    return dnd_on;
}

// 2016-06-07T14:16:15+000
// Maybe time zone should be handled(string after +)
// We change the time zone to 0, because our system using 0 time zone
static time_t clova_time_string_to_time(char *str)
{
    time_t ret = 0;
    time_t tt;
    struct tm t;
    struct tm *cur;
    int hour = 0;
    int minute = 0;
    int err = 0;

    if (!str || strlen(str) < 5) {
        return 0;
    }

    if (strlen(str) > 20) {
        int eot = 0;
        int year = 0;
        int month = 0;
        int second = 0;
        int day = 0;
        char symbol = '+';

        err = sscanf(str, "%d-%d-%dT%d:%d:%d%c%2d", &year, &month, &day, &hour, &minute, &second,
                     &symbol, &eot);
        (void)err;
        memset((void *)&t, 0, sizeof(t));
        t.tm_sec = second;
        t.tm_min = minute;
        t.tm_hour = hour;
        t.tm_mday = day;
        t.tm_mon = month - 1;
        t.tm_year = year - 1900;
        t.tm_wday = -1;
        t.tm_yday = -1;
        t.tm_isdst = -1;
        ret = mktime(&t);

        if (symbol == '+') {
            ret -= HOUR_IN_SECONDS(eot);
        } else {
            ret += HOUR_IN_SECONDS(eot);
        }
        if (eot > 0 && g_clova_dnd.tm_zone != eot) {
            if (symbol == '+') {
                g_clova_dnd.tm_zone = eot;
            } else {
                g_clova_dnd.tm_zone = -eot;
            }
        }
    } else {
        time(&tt);
        cur = localtime(&tt);
        sscanf(str, "%d:%d", &hour, &minute);
        memset((void *)&t, 0, sizeof(t));
        t.tm_sec = 0;
        t.tm_min = minute;
        t.tm_hour = hour;
        t.tm_mday = cur->tm_mday;
        t.tm_mon = cur->tm_mon;
        t.tm_year = cur->tm_year;
        t.tm_wday = -1;
        t.tm_yday = -1;
        t.tm_isdst = -1;
        ret = mktime(&t);

        if (g_clova_dnd.tm_zone != 0) {
            ret -= HOUR_IN_SECONDS(g_clova_dnd.tm_zone);
        } else {
            ret -= HOUR_IN_SECONDS(9);
        }
    }

    return ret;
}

void update_dnd_info_time(void)
{
    time_t tt;
    tt = time(&tt);

    if (g_clova_dnd.end_time_s > g_clova_dnd.start_time_s) {
        if (tt > g_clova_dnd.end_time_s) {
            g_clova_dnd.start_time_s += HOUR_IN_SECONDS(24);
            g_clova_dnd.end_time_s += HOUR_IN_SECONDS(24);
        }
    } else {
        if (g_clova_dnd.start_time_s > tt) {
            g_clova_dnd.start_time_s -= HOUR_IN_SECONDS(24);
        }

        if (tt > g_clova_dnd.end_time_s) {
            g_clova_dnd.end_time_s += HOUR_IN_SECONDS(24);
        }

        if (g_clova_dnd.end_time_s - g_clova_dnd.start_time_s > HOUR_IN_SECONDS(24)) {
            g_clova_dnd.start_time_s += HOUR_IN_SECONDS(24);
        }
    }
}

int clova_set_dnd_info(char *state, char *start_time, char *end_time)
{
    int ret = -1;
    time_t tt;
    tt = time(&tt);

    if (start_time && end_time) {
        dnd_state_lock();
        if (StrEq(state, "true")) {
            g_clova_dnd.state = 1;
        } else {
            g_clova_dnd.state = 0;
        }
        if (g_clova_dnd.start_time) {
            free(g_clova_dnd.start_time);
        }
        g_clova_dnd.start_time = strdup(start_time);
        g_clova_dnd.start_time_s = clova_time_string_to_time(start_time);

        if (g_clova_dnd.end_time) {
            free(g_clova_dnd.end_time);
        }
        g_clova_dnd.end_time = strdup(end_time);
        g_clova_dnd.end_time_s = clova_time_string_to_time(end_time);
        update_dnd_info_time();
        if (g_clova_dnd.state == 1) {
            asr_system_add_dnd_start(g_clova_dnd.start_time_s - tt);
            asr_system_add_dnd_end(g_clova_dnd.end_time_s - tt);
            printf("dnd start(%d) dnd end(%d)\n", g_clova_dnd.start_time_s - tt,
                   g_clova_dnd.end_time_s - tt);
            if (g_clova_dnd.dnd_on != 1 && tt >= g_clova_dnd.start_time_s &&
                tt < g_clova_dnd.end_time_s) {
                g_clova_dnd.dnd_on = 1;
                set_dnd_mode(1);
            }
        } else {
            if (g_clova_dnd.dnd_on != 0 && tt >= g_clova_dnd.expire_time_s) {
                g_clova_dnd.dnd_on = 0;
                set_dnd_mode(0);
            }
        }
        debug_printf();
        dnd_state_unlock();
        ret = 0;
    }

    return ret;
}

int clova_set_dnd_expire_time(char *expire_time)
{
    int ret = -1;
    time_t tt;
    tt = time(&tt);

    if (expire_time) {
        dnd_state_lock();
        if (g_clova_dnd.expire_time) {
            free(g_clova_dnd.expire_time);
        }
        g_clova_dnd.expire_time = strdup(expire_time);
        g_clova_dnd.expire_time_s = clova_time_string_to_time(expire_time);
        if (g_clova_dnd.expire_time_s != 0) {
            if (g_clova_dnd.dnd_on != 1 && tt < g_clova_dnd.expire_time_s) {
                g_clova_dnd.dnd_on = 1;
                set_dnd_mode(1);
            } else if (tt > g_clova_dnd.expire_time_s) {
                if (g_clova_dnd.expire_time) {
                    free(g_clova_dnd.expire_time);
                    g_clova_dnd.expire_time = NULL;
                    g_clova_dnd.expire_time_s = 0;
                }
            }
        } else {
            if (!(g_clova_dnd.state == 1 &&
                  (g_clova_dnd.end_time_s > tt && tt > g_clova_dnd.start_time_s))) {
                if (g_clova_dnd.dnd_on != 0) {
                    g_clova_dnd.dnd_on = 0;
                    set_dnd_mode(0);
                }
            }
        }
        printf("[%s:%d] expire_time_s(%d) tt(%d)\n", __FUNCTION__, __LINE__,
               g_clova_dnd.expire_time_s, tt);
        if (g_clova_dnd.expire_time_s != 0 && tt < g_clova_dnd.expire_time_s) {
            asr_system_add_dnd_expire(g_clova_dnd.expire_time_s - tt);
        }
        if (g_clova_dnd.expire_time == NULL) {
            ret = -1;
        } else {
            ret = 0;
        }
        dnd_state_unlock();
    }

    return ret;
}

int clova_clear_dnd_expire_time(void)
{
    int ret = -1;
    time_t tt;
    tt = time(&tt);

    ret = 0;
    dnd_state_lock();
    if (g_clova_dnd.expire_time) {
        free(g_clova_dnd.expire_time);
    }
    g_clova_dnd.expire_time = NULL;
    if (!(g_clova_dnd.state == 1 &&
          (g_clova_dnd.end_time_s > tt && tt > g_clova_dnd.start_time_s))) {
        if (g_clova_dnd.dnd_on != 0) {
            g_clova_dnd.dnd_on = 0;
            set_dnd_mode(0);
        }
    }
    g_clova_dnd.expire_time_s = 0;
    dnd_state_unlock();

EXIT:

    return ret;
}

void debug_printf(void)
{
    time_t tt;
    struct tm *cur;
    struct tm *start;
    struct tm *end;
    struct tm *expire;
    time(&tt);

    cur = localtime(&tt);
    printf("currtne time: %4d-%02d-%02d %02d:%02d:%02d time_zone=%d dnd_on=%d\n",
           cur->tm_year + 1900, cur->tm_mon + 1, cur->tm_mday, cur->tm_hour, cur->tm_min,
           cur->tm_sec, g_clova_dnd.tm_zone, g_clova_dnd.dnd_on);

    start = localtime(&g_clova_dnd.start_time_s);
    printf("start time: %4d-%02d-%02d %02d:%02d:%02d time_zone=%d dnd_on=%d\n",
           start->tm_year + 1900, start->tm_mon + 1, start->tm_mday, start->tm_hour, start->tm_min,
           start->tm_sec, g_clova_dnd.tm_zone, g_clova_dnd.dnd_on);

    end = localtime(&g_clova_dnd.end_time_s);
    printf("end time: %4d-%02d-%02d %02d:%02d:%02d time_zone=%d dnd_on=%d\n", end->tm_year + 1900,
           end->tm_mon + 1, end->tm_mday, end->tm_hour, end->tm_min, end->tm_sec,
           g_clova_dnd.tm_zone, g_clova_dnd.dnd_on);

    expire = localtime(&g_clova_dnd.expire_time_s);
    printf("expire time: %4d-%02d-%02d %02d:%02d:%02d time_zone=%d dnd_on=%d\n",
           expire->tm_year + 1900, expire->tm_mon + 1, expire->tm_mday, expire->tm_hour,
           expire->tm_min, expire->tm_sec, g_clova_dnd.tm_zone, g_clova_dnd.dnd_on);
}

int clova_get_dnd_state(json_object *dnd_state)
{
    int ret = -1;
    char *start_time = "23:00";
    char *end_time = "09:00";
    json_object *scheduled = NULL;

    if (dnd_state == NULL) {
        goto EXIT;
    }

    scheduled = json_object_new_object();
    if (scheduled == NULL) {
        goto EXIT;
    }

    dnd_state_lock();
    start_time = g_clova_dnd.start_time;
    if (!start_time) {
        start_time = "23:00";
    }
    end_time = g_clova_dnd.end_time;
    if (!end_time) {
        end_time = "09:00";
    }

    debug_printf();

    json_object_object_add(scheduled, CLOVA_PAYLOAD_DND_SCHEDULED_STATE,
                           JSON_OBJECT_NEW_STRING(g_clova_dnd.state ? "on" : "off"));
    json_object_object_add(scheduled, CLOVA_PAYLOAD_DND_SCHEDULED_START_TIME,
                           JSON_OBJECT_NEW_STRING(start_time));
    json_object_object_add(scheduled, CLOVA_PAYLOAD_DND_SCHEDULED_END_TIME,
                           JSON_OBJECT_NEW_STRING(end_time));
    json_object_object_add(dnd_state, CLOVA_PAYLOAD_DND_SCHEDULED, scheduled);
    json_object_object_add(dnd_state, CLOVA_PAYLOAD_DND_ACTION_EXPIREDAT,
                           JSON_OBJECT_NEW_STRING(g_clova_dnd.expire_time));
    json_object_object_add(dnd_state, CLOVA_PAYLOAD_DND_SCHEDULED_STATE,
                           JSON_OBJECT_NEW_STRING(g_clova_dnd.dnd_on ? "on" : "off"));
    ret = 0;
    dnd_state_unlock();

EXIT:

    return ret;
}
