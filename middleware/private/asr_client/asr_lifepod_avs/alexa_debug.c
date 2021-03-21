
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "alexa_debug.h"
#include "wm_util.h"

unsigned int g_alexa_flags = ALEXA_DEBUG_NONE;
extern WIIMU_CONTEXT *g_wiimu_shm;

const char *current_time(void)
{
    static char time_str[100];
    memset(&time_str, 0x0, sizeof(time_str));

#ifdef UTC_TIME
    time_t now_time = 0;
    struct tm *local_time = NULL;

    now_time = time(NULL);
    local_time = localtime(&now_time);
    local_time->tm_year = local_time->tm_year + 1900;
    local_time->tm_mon = local_time->tm_mon + 1;
    snprintf(time_str, sizeof(time_str), "%04d-%02d-%02d %02d:%02d:%02d ~ %02d:%02d:%02d.%03d",
             local_time->tm_year, local_time->tm_mon, local_time->tm_mday, local_time->tm_hour,
             local_time->tm_min, local_time->tm_sec, (int)(tickSincePowerOn() / 3600000),
             (int)(tickSincePowerOn() % 3600000 / 60000), (int)(tickSincePowerOn() % 60000 / 1000),
             (int)(tickSincePowerOn() % 1000));
#else
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d.%03d", (int)(tickSincePowerOn() / 3600000),
             (int)(tickSincePowerOn() % 3600000 / 60000), (int)(tickSincePowerOn() % 60000 / 1000),
             (int)(tickSincePowerOn() % 1000));
#endif

    return (const char *)time_str;
}

void AlexaSetDebugLevel(int level) { g_alexa_flags = level; }

int AlexaProfileType(void)
{

    if (far_field_judge(g_wiimu_shm)) {
        return FAR_FIELD;
    } else {
        return CLOSE_TALK;
    }
}
