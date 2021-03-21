
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <json.h>

#include "alexa_debug.h"
#include "alexa_cmd.h"
#include "alexa_api2.h"
#include "alexa_json_common.h"
#include "alexa_authorization.h"
#include "http2_client.h"

#include "wm_util.h"
#include "tick_timer.h"

#include "alexa_system.h"

#define os_power_on_tick() tickSincePowerOn()

extern WIIMU_CONTEXT *g_wiimu_shm;

#define SYSTEM_DEBUG(fmt, ...)                                                                     \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d][system] " fmt, (int)(os_power_on_tick() / 3600000), \
                (int)(os_power_on_tick() % 3600000 / 60000),                                       \
                (int)(os_power_on_tick() % 60000 / 1000), (int)(os_power_on_tick() % 1000),        \
                ##__VA_ARGS__);                                                                    \
    } while (0)

typedef struct _avs_system_s {
    int is_first_boot;
    int is_speech_timeout;
    int need_change_endpoint;
    pthread_mutex_t state_lock;
    tick_timer_t *tick_timer;
} avs_system_t;

static avs_system_t g_avs_system;

extern void asr_set_audio_resume(void);

static int start_ping(void *ctx)
{
    http2_conn_ping();
    return 0;
}

static int start_inactive_report(void *ctx)
{
    long long last_inactive_ts = 0;
    char *alexa_access_token = AlexaGetAccessToken();

    pthread_mutex_lock(&g_avs_system.state_lock);
    last_inactive_ts = g_wiimu_shm->client_last_inactive_ts;
    pthread_mutex_unlock(&g_avs_system.state_lock);

    if (last_inactive_ts > 0 && alexa_access_token) {
        alexa_system_cmd(0, NAME_UserInactivityReport, last_inactive_ts, NULL, NULL);
    }

#ifdef AVS_MRM_ENABLE
    if (!AlexaDisableMrm()) {
        /* need to send a notify to WHA library and WHA lib will decide if it needs to calculate the
        * cluster idle time */
        SocketClientReadWriteMsg("/tmp/alexa_mrm", "AlexaIdleStateChange",
                                 strlen("AlexaIdleStateChange"), NULL, NULL, 1);
    }
#endif

    if (alexa_access_token) {
        free(alexa_access_token);
    }

    return 0;
}

static int expect_speech_timed_out(void *ctx)
{
    char *buffer = NULL;

    pthread_mutex_lock(&g_avs_system.state_lock);
    if (g_avs_system.is_speech_timeout == 1) {
        SYSTEM_DEBUG("speech time out, clear the flags\n");
        asr_set_audio_resume();
        Create_Cmd(buffer, 0, NAMESPACE_SPEECHRECOGNIZER, NAME_EXPECTSPEECHTIMEOUT, Val_Obj(NULL));
        if (buffer) {
            NOTIFY_CMD(ALEXA_CMD_FD, buffer);
            free(buffer);
        }
    }
    pthread_mutex_unlock(&g_avs_system.state_lock);

    return 0;
}

int avs_system_init(void)
{
    memset(&g_avs_system, 0x0, sizeof(avs_system_t));
    pthread_mutex_init(&g_avs_system.state_lock, NULL);
    pthread_mutex_lock(&g_avs_system.state_lock);
    g_avs_system.tick_timer = tick_timer_create();
    g_wiimu_shm->client_last_inactive_ts = os_power_on_tick();
    pthread_mutex_unlock(&g_avs_system.state_lock);

    return 0;
}

int avs_system_uninit(void)
{
    pthread_mutex_lock(&g_avs_system.state_lock);
    if (g_avs_system.tick_timer) {
        tick_timer_destroy(&g_avs_system.tick_timer);
        g_avs_system.tick_timer = NULL;
    }
    pthread_mutex_unlock(&g_avs_system.state_lock);
    pthread_mutex_destroy(&g_avs_system.state_lock);

    return 0;
}

int avs_system_add_ping_timer(void)
{
    tick_job_t ping_job = {0};
    ping_job.job_id = 1;
    ping_job.need_repeat = 1;
    ping_job.start_time = os_power_on_tick();
    ping_job.end_time = PING_TIME + os_power_on_tick();
    ping_job.do_job = start_ping;
    ping_job.job_ctx = NULL;

    return tick_timer_add_item(g_avs_system.tick_timer, ping_job);
}

int avs_system_update_inactive_timer(void)
{
    tick_job_t ping_job = {0};
    ping_job.job_id = 2;
    ping_job.need_repeat = 1;
    ping_job.start_time = os_power_on_tick();
    ping_job.end_time = INATIVE_TIME + os_power_on_tick();
    ping_job.do_job = start_inactive_report;
    ping_job.job_ctx = NULL;

#ifdef AVS_MRM_ENABLE
    if (!AlexaDisableMrm()) {
        /* need to send a notify to WHA library and WHA lib will decide if it needs to calculate the
        * cluster idle time */
        SocketClientReadWriteMsg("/tmp/alexa_mrm", "AlexaIdleStateChange",
                                 strlen("AlexaIdleStateChange"), NULL, NULL, 1);
    }
#endif

    pthread_mutex_lock(&g_avs_system.state_lock);
    g_wiimu_shm->client_last_inactive_ts = os_power_on_tick();
    pthread_mutex_unlock(&g_avs_system.state_lock);

    return tick_timer_add_item(g_avs_system.tick_timer, ping_job);
}

int avs_system_set_expect_speech_timed_out(int clear_flags)
{
    if (clear_flags == 0) {
        tick_job_t time_job = {0};
        time_job.job_id = 3;
        time_job.need_repeat = 0;
        time_job.start_time = os_power_on_tick();
        time_job.end_time = 8000 + os_power_on_tick();
        time_job.do_job = expect_speech_timed_out;
        time_job.job_ctx = NULL;
        pthread_mutex_lock(&g_avs_system.state_lock);
        g_avs_system.is_speech_timeout = 1;
        pthread_mutex_unlock(&g_avs_system.state_lock);
        return tick_timer_add_item(g_avs_system.tick_timer, time_job);
    } else {
        pthread_mutex_lock(&g_avs_system.state_lock);
        g_avs_system.is_speech_timeout = 0;
        pthread_mutex_unlock(&g_avs_system.state_lock);
        return tick_timer_del_item(g_avs_system.tick_timer, 3);
    }
}

/*
System:
ResetUserInactivity Directive
{
    "directive": {
        "header": {
            "namespace": "System",
            "name": "ResetUserInactivity",
            "messageId": "{{STRING}}"
        },
        "payload": {
        }
    }
}
*/
static int avs_system_parse_reset_user_inactivity(json_object *js_obj)
{
    if (js_obj) {
        json_object *js_header = json_object_object_get(js_obj, KEY_HEADER);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
        }

        return avs_system_update_inactive_timer();
    }

    return -1;
}

/*
System :
SetEndpoint Directive
{
    "directive": {
        "header": {
            "namespace": "System",
            "name": "SetEndpoint",
            "messageId": "{{STRING}}"
        },
        "payload": {
            "endpoint": "{{STRING}}"
         }
    }
}

The SetEndpoint directive instructs a client to change endpoints when the
following conditions are met:
A user's country settings are not supported by the endpoint. For example, if
a user's current country is set to United Kingdom (UK) in Manage Your
Content and Devices and the client connects to the United States (US) endpoint
, a SetEndpoint directive will be sent instructing the client to connect to
the endpoint that supports UK.
A user changes their country settings (or address). For example, if a user
connected to the US endpoint changes their current country (or address) from
the US to the UK, a SetEndpoint directive will be sent instructing the client
to connect to the endpoint that supports UK.
Important: Failure to switch endpoints may result in a user's inability to
access data associated with a given country.
*/
static int avs_system_parse_set_endpoint(json_object *js_obj)
{
    if (js_obj) {
        json_object *js_header = json_object_object_get(js_obj, KEY_HEADER);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
        }

        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            char *endpoint = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_ENDPOINT);
            if (endpoint) {
                pthread_mutex_lock(&g_avs_system.state_lock);
                g_avs_system.need_change_endpoint = 1;
                pthread_mutex_unlock(&g_avs_system.state_lock);
                SYSTEM_DEBUG("endpoint is %s\n", endpoint);
                char *tmp = strstr(endpoint, "https://");
                if (tmp) {
                    alexa_string_cmd(EVENT_SET_ENDPOINT, tmp + strlen("https://"));
                }
            }
        }

        return 0;
    }

    return -1;
}

/*
    https://developer.amazon.com/docs/alexa-voice-service/system.html#softwareinfo-event
    This directive instructs your product to report current software
        information to Alexa using the SoftwareInfo event.
    {
        "directive": {
            "header": {
                "namespace": "System",
                "name": "ReportSoftwareInfo",
                "messageId": "{{STRING}}"
            },
            "payload": {
            }
        }
    }
*/
static int avs_system_parse_report_soft_version(json_object *js_obj)
{
    if (js_obj) {
        json_object *js_header = json_object_object_get(js_obj, KEY_HEADER);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
        }

        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            //
        }
        alexa_system_cmd(0, NAME_SoftwareInfo, 0, NULL, NULL);

        return 0;
    }

    return -1;
}

int avs_system_parse_directive(json_object *js_obj)
{
    int ret = -1;
    char *name = NULL;

    if (js_obj) {
        json_object *js_header = json_object_object_get(js_obj, KEY_HEADER);
        if (js_header) {
            name = JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_DIALOGREQUESTID);
        }

        if (StrEq(name, NAME_ResetUserInactivity)) {
            ret = avs_system_parse_reset_user_inactivity(js_obj);

        } else if (StrEq(name, NAME_SetEndpoint)) {
            ret = avs_system_parse_set_endpoint(js_obj);

        } else if (StrEq(name, NAME_ReportSoftwareInfo)) {
            ret = avs_system_parse_report_soft_version(js_obj);
        }
    }

    return ret;
}

int avs_system_update_soft_version(void)
{
    int is_first_boot = 0;

    pthread_mutex_lock(&g_avs_system.state_lock);
    is_first_boot = g_avs_system.is_first_boot;
    g_avs_system.is_first_boot = 1;
    pthread_mutex_unlock(&g_avs_system.state_lock);

    if (is_first_boot == 0) {
        alexa_system_cmd(1, NAME_SoftwareInfo, 0, NULL, NULL);
    }

    return 0;
}

void avs_system_clear_soft_version_flag(void)
{
    pthread_mutex_lock(&g_avs_system.state_lock);
    g_avs_system.is_first_boot = 0;
    pthread_mutex_unlock(&g_avs_system.state_lock);
}

int avs_system_get_changeendpoint(void)
{
    int flags = 0;
    pthread_mutex_lock(&g_avs_system.state_lock);
    flags = g_avs_system.need_change_endpoint;
    g_avs_system.need_change_endpoint = 0;
    pthread_mutex_unlock(&g_avs_system.state_lock);

    return flags;
}
