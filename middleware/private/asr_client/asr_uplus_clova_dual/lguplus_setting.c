
#include <stdio.h>
#include <stdlib.h>

#include "asr_session.h"
#include "asr_debug.h"
#include "asr_json_common.h"
#include "lguplus_setting.h"

static int wakeup_word_apply(const char *value) { return 0; }

static int ping_period_apply(const char *value) { return 0; }

static int aiif_endpoint_apply(const char *value) { return 0; }

static int inactive_interval_apply(const char *value) { return 0; }

static int firmware_update_start(const char *value) { return 0; }

static int location_information_report(const char *value) { return 0; }

static int sleep_mode_apply(const char *value)
{
    int ret;

    ret = asr_session_disconncet(ASR_LGUPLUS);

    return ret;
}

int lguplus_setting_apply(const char *key, const char *value)
{
    int ret = -1;

    if (!key || !value)
        return ret;

    ASR_PRINT(ASR_DEBUG_TRACE, "key = %s value = %s\n", key, value);

    if (StrEq(key, WAKEUP_WORD)) {
        ret = wakeup_word_apply(value);
    } else if (StrEq(key, PING_PERIOD)) {
        ret = ping_period_apply(value);
    } else if (StrEq(key, AIIF_ENDPOINT)) {
        ret = aiif_endpoint_apply(value);
    } else if (StrEq(key, INACTIVE_INTERVAL)) {
        ret = inactive_interval_apply(value);
    } else if (StrEq(key, FIRMWARE_UPDATE)) {
        ret = firmware_update_start(value);
    } else if (StrEq(key, UPLOAD_LOCATION_INFOMATION)) {
        ret = location_information_report(value);
    } else if (StrEq(key, SLEEP_MODE)) {
        ret = sleep_mode_apply(value);
    }

    return ret;
}
