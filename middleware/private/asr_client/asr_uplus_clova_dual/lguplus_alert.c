
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"

#include "lguplus_json_common.h"
#include "lguplus_alert.h"

struct lp_list_head lguplus_alert_list;

static void lguplus_read_alert_list(void)
{
    FILE *fp = NULL;
    char buff[256] = {0};
    char *ptr = NULL;
    LGUPLUS_ALERT_T *alert = NULL;

    fp = fopen(LGUP_ALERT_FILE, "r");
    if (fp) {
        while (fgets(buff, sizeof(buff), fp)) {

            ptr = strchr(buff, ':');
            if (ptr) {
                *ptr = '\0';
                ptr += 2;
                *ptr = '\0';
                ptr--;

                alert = (LGUPLUS_ALERT_T *)calloc(1, sizeof(LGUPLUS_ALERT_T));
                if (alert) {
                    alert->alertId = strdup(buff);
                    alert->activeYn = strdup(ptr);

                    lp_list_add_tail(&alert->list, &lguplus_alert_list);
                }
            }

            memset(buff, 0, sizeof(buff));
        }
    }

    if (fp)
        fclose(fp);
}

static void lguplus_write_alert(LGUPLUS_ALERT_T *alert)
{
    FILE *fp = NULL;

    fp = fopen(LGUP_ALERT_FILE, "a");
    if (fp) {
        fwrite(alert->alertId, 1, strlen(alert->alertId), fp);
        fwrite(":", 1, strlen(":"), fp);
        fwrite(alert->activeYn, 1, strlen(alert->activeYn), fp);
        fwrite("\n", 1, strlen("\n"), fp);

        fclose(fp);
    }
}

static void lguplus_write_all_alert(void)
{
    FILE *fp = NULL;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    LGUPLUS_ALERT_T *list_item = NULL;

    fp = fopen(LGUP_ALERT_FILE, "w");
    if (fp) {
        if (!lp_list_empty(&lguplus_alert_list)) {
            lp_list_for_each_safe(pos, npos, &lguplus_alert_list)
            {
                list_item = lp_list_entry(pos, LGUPLUS_ALERT_T, list);
                lguplus_write_alert(list_item);
            }
        }

        fclose(fp);
    }
}

static char *lguplus_create_official_schedule(const char *date, const char *time)
{
    char *schedule = NULL;
    char year[8] = {0};
    char month[8] = {0};
    char day[8] = {0};
    char hour[8] = {0};
    char minute[8] = {0};
    char second[8] = {0};
    char *tmp = NULL;

    if (date && time) {
        tmp = date;
        strncpy(year, tmp, 4);

        tmp += 4;
        strncpy(month, tmp, 2);

        tmp += 2;
        strncpy(day, tmp, 2);

        tmp = time;
        strncpy(hour, tmp, 2);

        tmp += 2;
        strncpy(minute, tmp, 2);

        strncpy(second, "00", 2);

        asprintf(&schedule, "%4s-%2s-%2sT%2s:%2s:%s+09:00", year, month, day, hour, minute, second);
    }

    return schedule;
}

static void lguplus_alert_response(const int success, const char *schema, const char *method,
                                   const char *token, const char *entire)
{
    json_object *js_body = NULL;
    char *result = NULL;
    char *body = NULL;

    if (schema && method) {
        js_body = json_object_new_object();
        if (js_body) {
            if (!success) {
                result = "Y";
            } else {
                result = "N";
            }

            if (token)
                json_object_object_add(js_body, LGUP_ALERT_ALERT_ID, json_object_new_string(token));
            if (entire)
                json_object_object_add(js_body, LGUP_ALERT_IS_ENTIRE,
                                       json_object_new_string(entire));
            json_object_object_add(js_body, LGUP_ALERT_RESULT_STATUS,
                                   json_object_new_string(result));

            body = json_object_to_json_string(js_body);
            lguplus_event_queue_add_body(schema, method, NULL, NULL, body);

            json_object_put(js_body);
        }
    }
}

static int lguplus_notify_add_alert(LGUPLUS_ALERT_T *new_alert)
{
    char *token = NULL;
    char *type = NULL;
    char *schedule = NULL;
    int ret = -1;

    if (new_alert) {
        token = new_alert->alertId;
        type = LGUP_ALARM;
        schedule = lguplus_create_official_schedule(new_alert->setDate, new_alert->setTime);

        if (token && type && schedule)
            ret = alert_set(token, type, schedule);

        if (schedule)
            free(schedule);
    }

    return ret;
}

static void destory_alert_item(LGUPLUS_ALERT_T *item)
{
    if (item) {
        if (item->alertId) {
            free(item->alertId);
        }
        if (item->alarmTitle) {
            free(item->alarmTitle);
        }
        if (item->activeYn) {
            free(item->activeYn);
        }
        if (item->volume) {
            free(item->volume);
        }
        if (item->setDate) {
            free(item->setDate);
        }
        if (item->setTime) {
            free(item->setTime);
        }
        if (item->repeatYn) {
            free(item->repeatYn);
        }
        if (item->repeat) {
            free(item->repeat);
        }

        free(item);
    }
}

static void delete_entire_alert(void)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    LGUPLUS_ALERT_T *list_item = NULL;

    if (!lp_list_empty(&lguplus_alert_list)) {
        lp_list_for_each_safe(pos, npos, &lguplus_alert_list)
        {
            list_item = lp_list_entry(pos, LGUPLUS_ALERT_T, list);
            alert_delete(list_item->alertId);

            lp_list_del(&list_item->list);
            destory_alert_item(list_item);
        }
    }

    lguplus_alert_response(0, LGUP_ALERT, LGUP_ALERT_DELETE, NULL, YES);
}

static void delete_single_alert(const char *alertId, int need_clean)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    LGUPLUS_ALERT_T *list_item = NULL;

    if (!lp_list_empty(&lguplus_alert_list)) {
        lp_list_for_each_safe(pos, npos, &lguplus_alert_list)
        {
            list_item = lp_list_entry(pos, LGUPLUS_ALERT_T, list);
            if (StrEq(list_item->alertId, alertId)) {
                alert_delete(list_item->alertId);

                if (need_clean) {
                    lguplus_alert_response(0, LGUP_ALERT, LGUP_ALERT_DELETE, list_item->alertId,
                                           NO);
                    lp_list_del(&list_item->list);
                    destory_alert_item(list_item);
                }
            }
        }
    }
}

static void update_entire_alert(const char *activeYn)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    LGUPLUS_ALERT_T *list_item = NULL;

    if (!lp_list_empty(&lguplus_alert_list)) {
        lp_list_for_each_safe(pos, npos, &lguplus_alert_list)
        {
            list_item = lp_list_entry(pos, LGUPLUS_ALERT_T, list);
            if (StrEq(activeYn, YES)) {
                if (StrEq(list_item->activeYn, NO)) {
                    free(list_item->activeYn);
                    list_item->activeYn = strdup(activeYn);
                    lguplus_notify_add_alert(list_item);
                }
            } else if (StrEq(activeYn, NO)) {
                if (StrEq(list_item->activeYn, YES) || !list_item->activeYn) {
                    if (list_item->activeYn) {
                        free(list_item->activeYn);
                    }
                    list_item->activeYn = strdup(activeYn);
                    delete_single_alert(list_item->alertId, 0);
                }
            }

            lguplus_alert_response(0, LGUP_ALERT, LGUP_ALERT_UPDATE, NULL, YES);
        }
    }
}

static void update_single_alert(const char *alertId, const char *activeYn)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    LGUPLUS_ALERT_T *list_item = NULL;

    if (!lp_list_empty(&lguplus_alert_list)) {
        lp_list_for_each_safe(pos, npos, &lguplus_alert_list)
        {
            list_item = lp_list_entry(pos, LGUPLUS_ALERT_T, list);
            if (StrEq(list_item->alertId, alertId)) {
                if (StrEq(activeYn, YES)) {
                    if (StrEq(list_item->activeYn, NO)) {
                        free(list_item->activeYn);
                        list_item->activeYn = strdup(activeYn);
                        lguplus_notify_add_alert(list_item);
                    }
                } else if (StrEq(activeYn, NO)) {
                    if (StrEq(list_item->activeYn, YES) || !list_item->activeYn) {
                        if (list_item->activeYn) {
                            free(list_item->activeYn);
                        }
                        list_item->activeYn = strdup(activeYn);
                        delete_single_alert(list_item->alertId, 0);
                    }
                }

                lguplus_alert_response(0, LGUP_ALERT, LGUP_ALERT_UPDATE, alertId, NO);
            }
        }
    }
}

void lguplus_add_alert(LGUPLUS_ALERT_T *new_alert)
{
    LGUPLUS_ALERT_T *insert_alert = NULL;
    int ret = -1;

    if (new_alert) {
        insert_alert = (LGUPLUS_ALERT_T *)calloc(1, sizeof(LGUPLUS_ALERT_T));
        if (insert_alert) {
            if (new_alert->alertId) {
                insert_alert->alertId = strdup(new_alert->alertId);
            }
            if (new_alert->alarmTitle) {
                insert_alert->alarmTitle = strdup(new_alert->alarmTitle);
            }
            if (new_alert->activeYn) {
                insert_alert->activeYn = strdup(new_alert->activeYn);
            }
            if (new_alert->volume) {
                insert_alert->volume = strdup(new_alert->volume);
            }
            if (new_alert->setDate) {
                insert_alert->setDate = strdup(new_alert->setDate);
            }
            if (new_alert->setTime) {
                insert_alert->setTime = strdup(new_alert->setTime);
            }
            if (new_alert->repeatYn) {
                insert_alert->repeatYn = strdup(new_alert->repeatYn);
            }
            if (new_alert->repeat) {
                insert_alert->repeat = strdup(new_alert->repeat);
            }

            lp_list_add_tail(&insert_alert->list, &lguplus_alert_list);

            ret = lguplus_notify_add_alert(insert_alert);

            lguplus_write_alert(insert_alert);
        }
    }

    lguplus_alert_response(ret, LGUP_ALERT, LGUP_ALERT_ADD, insert_alert->alertId, NULL);
}

void lguplus_delete_alert(const char *alertId, const char *isEntire)
{
    if (StrEq(isEntire, YES)) {
        delete_entire_alert();
    } else if (StrEq(isEntire, NO)) {
        if (alertId) {
            delete_single_alert(alertId, 1);
        }
    }

    lguplus_write_all_alert();
}

void lguplus_update_alert(const char *isEntire, const char *alertId, const char *activeYn)
{
    if (StrEq(isEntire, YES)) {
        update_entire_alert(activeYn);
    } else if (StrEq(isEntire, NO)) {
        update_single_alert(alertId, activeYn);
    }

    lguplus_write_all_alert();
}

int lguplus_check_alert_inside(const char *alertId)
{
    int ret = -1;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    LGUPLUS_ALERT_T *list_item = NULL;

    if (!lp_list_empty(&lguplus_alert_list)) {
        lp_list_for_each_safe(pos, npos, &lguplus_alert_list)
        {
            list_item = lp_list_entry(pos, LGUPLUS_ALERT_T, list);
            if (StrEq(list_item->alertId, alertId))
                ret = 0;
        }
    }

    return ret;
}

void lguplus_check_alert_del(const char *alertId)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    LGUPLUS_ALERT_T *list_item = NULL;

    if (!lp_list_empty(&lguplus_alert_list)) {
        lp_list_for_each_safe(pos, npos, &lguplus_alert_list)
        {
            list_item = lp_list_entry(pos, LGUPLUS_ALERT_T, list);
            if (StrEq(list_item->alertId, alertId)) {
                if (!list_item->repeat) {
                    lp_list_del(&list_item->list);
                    destory_alert_item(list_item);
                }
            }
        }
    }

    lguplus_write_all_alert();
}

void lguplus_alert_list_init(void)
{
    LP_INIT_LIST_HEAD(&lguplus_alert_list);
    lguplus_read_alert_list();
}
