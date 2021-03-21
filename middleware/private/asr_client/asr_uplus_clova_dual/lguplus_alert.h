
#ifndef __LGUP_ALERT_H__
#define __LGUP_ALERT_H__

#include "lp_list.h"

#define YES "Y"
#define NO "N"

#define LGUP_ALERT_FILE ("/vendor/wiimu/lguplus_alert")

#define LGUP_ALARM ("ALARM")

#ifndef StrEq
#define StrEq(str1, str2)                                                                          \
    ((str1 && str2)                                                                                \
         ? ((strlen((char *)str1) == strlen((char *)str2)) && !strcmp((char *)str1, (char *)str2)) \
         : 0)
#endif

typedef struct {
    char *alertId;
    char *alarmTitle;
    char *activeYn;
    char *volume;
    char *setDate;
    char *setTime;
    char *repeatYn;
    char *repeat;

    struct lp_list_head list;
} LGUPLUS_ALERT_T;

#endif