
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <json.h>

#include "wm_util.h"
#include "tick_timer.h"

#include "asr_debug.h"
#include "asr_system.h"
#include "asr_session.h"
#include "asr_json_common.h"
#include "clova_device_control.h"
#include "clova_timer.h"
#include "clova_settings.h"
#include "clova_dnd.h"
#include "common_flags.h"

#define HOUR_IN_SECOND_MS(n) ((n)*60 * 60 * 1000L)
#define PING_TIME (5 * 60 * 1000L)
#define LGUP_PING_TIME (3 * 60 * 1000L)
#define LGUP_INACTIVE_PERIOD (60 * 1000L)
#define os_power_on_tick() tickSincePowerOn()

enum {
    job_none,

    job_ping,
    job_ping_lgup,
    job_report,
    job_dnd_set,
    job_dnd_clear,
    job_dnd_expire,
    job_inactive_report_lgup,

    job_max,
};

typedef struct _asr_system_s {
    pthread_mutex_t state_lock;
    tick_timer_t *tick_timer;
} asr_system_t;

static asr_system_t g_asr_system;

static int start_clova_ping(void *ctx)
{
    int ret = -1;

    ret = asr_session_ping(ASR_CLOVA);

    return ret;
}

static int start_lguplus_ping(void *ctx)
{
    int ret = -1;

    ret = asr_session_ping(ASR_LGUPLUS);

    return ret;
}

static int start_lguplus_inactive_report(void *ctx)
{
    int ret = -1;

    ret = asr_session_inactive_report();

    return ret;
}

static int start_control_report(void *ctx)
{
    int ret = -1;
    int internet = get_internet_flags();
    int asr_online = get_asr_online();

    if (asr_online && internet == 1) {
        ret = clova_device_control_report_cmd(CLOVA_HEADER_NAME_REPORT_STATE);
    }

    return ret;
}

int asr_system_init(void)
{
    int ret = -1;

    memset(&g_asr_system, 0x0, sizeof(asr_system_t));
    pthread_mutex_init(&g_asr_system.state_lock, NULL);
    pthread_mutex_lock(&g_asr_system.state_lock);
    g_asr_system.tick_timer = tick_timer_create();
    pthread_mutex_unlock(&g_asr_system.state_lock);

    return 0;
}

int start_set_dnd(void *ctx)
{
    clova_set_dnd_on(1);
    return 0;
}

int start_clear_dnd(void *ctx)
{
    clova_set_dnd_on(0);
    return 0;
}

int dnd_expire(void *ctx)
{
    clova_cfg_update_dnd_expire_time("");
    clova_set_dnd_on(0);
    return 0;
}

int asr_system_uninit(void)
{
    clova_settings_uninit();
    pthread_mutex_lock(&g_asr_system.state_lock);
    if (g_asr_system.tick_timer) {
        tick_timer_destroy(&g_asr_system.tick_timer);
        g_asr_system.tick_timer = NULL;
    }
    pthread_mutex_unlock(&g_asr_system.state_lock);
    pthread_mutex_destroy(&g_asr_system.state_lock);

    return 0;
}

int asr_system_add_ping_timer(int ping_type)
{
    tick_job_t ping_job = {0};
    ping_job.need_repeat = 1;
    if (ping_type == ASR_CLOVA) {
        ping_job.job_id = job_ping;
        ping_job.start_time = PING_TIME + os_power_on_tick();
        ping_job.period_time = PING_TIME;
        ping_job.do_job = start_clova_ping;
    } else {
        ping_job.job_id = job_ping_lgup;
        ping_job.start_time = LGUP_PING_TIME + os_power_on_tick();
        ping_job.period_time = LGUP_PING_TIME;
        ping_job.do_job = start_lguplus_ping;
    }
    ping_job.job_ctx = NULL;

    return tick_timer_add_item(g_asr_system.tick_timer, &ping_job);
}

int asr_system_add_inactive_timer(void)
{
    tick_job_t inactive_report_job = {0};
    inactive_report_job.need_repeat = 1;
    inactive_report_job.job_id = job_inactive_report_lgup;
    inactive_report_job.start_time = LGUP_INACTIVE_PERIOD + os_power_on_tick();
    inactive_report_job.period_time = LGUP_INACTIVE_PERIOD;
    inactive_report_job.do_job = start_lguplus_inactive_report;

    inactive_report_job.job_ctx = NULL;

    return tick_timer_add_item(g_asr_system.tick_timer, &inactive_report_job);
}

int asr_system_add_report_state(int duration, int interval)
{
    tick_job_t report_job = {0};
    report_job.job_id = job_report;
    report_job.need_repeat = 1;
    report_job.start_time = (ms_tick_t)duration * 1000 + os_power_on_tick();
    report_job.period_time = (ms_tick_t)interval * 1000;
    report_job.do_job = start_control_report;
    report_job.job_ctx = NULL;

    return tick_timer_add_item(g_asr_system.tick_timer, &report_job);
}

int asr_system_add_dnd_start(int start_time)
{
    tick_job_t dnd_start_job = {0};
    dnd_start_job.job_id = job_dnd_set;
    dnd_start_job.need_repeat = 1;
    dnd_start_job.start_time = (ms_tick_t)start_time * 1000 + os_power_on_tick();
    dnd_start_job.period_time = (ms_tick_t)HOUR_IN_SECOND_MS(24);
    dnd_start_job.do_job = start_set_dnd;
    dnd_start_job.job_ctx = NULL;

    return tick_timer_add_item(g_asr_system.tick_timer, &dnd_start_job);
}

int asr_system_add_dnd_end(int end_time)
{
    tick_job_t dnd_end_job = {0};
    dnd_end_job.job_id = job_dnd_clear;
    dnd_end_job.need_repeat = 1;
    dnd_end_job.start_time = (ms_tick_t)end_time * 1000 + os_power_on_tick();
    dnd_end_job.period_time = (ms_tick_t)HOUR_IN_SECOND_MS(24);
    dnd_end_job.do_job = start_clear_dnd;
    dnd_end_job.job_ctx = NULL;

    return tick_timer_add_item(g_asr_system.tick_timer, &dnd_end_job);
}

int asr_system_add_dnd_expire(int expire_time)
{
    tick_job_t dnd_expire_job = {0};
    dnd_expire_job.job_id = job_dnd_expire;
    dnd_expire_job.need_repeat = 0;
    dnd_expire_job.start_time = (ms_tick_t)expire_time * 1000 + os_power_on_tick();
    dnd_expire_job.period_time = 0;
    dnd_expire_job.do_job = dnd_expire;
    dnd_expire_job.job_ctx = NULL;

    return tick_timer_add_item(g_asr_system.tick_timer, &dnd_expire_job);
}
