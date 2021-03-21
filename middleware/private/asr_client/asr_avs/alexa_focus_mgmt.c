
#include <stdio.h>

#include "wm_util.h"
#include "alexa_json_common.h"
#include "alexa_request_json.h"
#include "alexa_focus_mgmt.h"

#define FOCUS_DEBUG(fmt, ...)                                                                      \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d][FOCUS_DEBUG] " fmt,                                 \
                (int)(tickSincePowerOn() / 3600000), (int)(tickSincePowerOn() % 3600000 / 60000),  \
                (int)(tickSincePowerOn() % 60000 / 1000), (int)(tickSincePowerOn() % 1000),        \
                ##__VA_ARGS__);                                                                    \
    } while (0)

static const char *channels2str[] = {
    "null",   "dialog", "communications", "alert", "content",
    "focused" // visual -> focused
};

static const char *interface2str[] = {"null",      "SpeechSynthesizer", "SipUserAgent",
                                      "Alerts",    "AudioPlayer",       "ExternalMediaPlayer",
                                      "Bluetooth", "TemplateRuntime"};

static focus_mgmt_info_t focus_mgmt_infos[MAX_INTERFACE_COUNT + 1];
static int activities_cnt = 0;
static focus_mgmt_if_t g_last_active_content = 0;

static focus_mgmt_chn_t get_channel_type(focus_mgmt_if_t ifs)
{
    focus_mgmt_chn_t channel = 0;

    switch (ifs) {
    case SpeechSynthesizer:
        channel = dialog;
        break;
    case SipUserAgent:
        channel = communications;
        break;
    case Alerts:
        channel = alert;
        break;
    case AudioPlayer:
    case Bluetooth:
    case ExternalMediaPlayer:
        channel = content;
        break;
    case TemplateRuntime:
        channel = visual;
        break;
    }

    return channel;
}

static focus_mgmt_activity_t get_activity_type(focus_mgmt_if_t ifs)
{
    focus_mgmt_activity_t activity = 0;

    switch (ifs) {
    case SpeechSynthesizer:
    case SipUserAgent:
    case Alerts:
    case AudioPlayer:
    case Bluetooth:
    case ExternalMediaPlayer:
        activity = audio_activity;
        break;
    case TemplateRuntime:
        activity = visual_activity;
        break;
    }

    return activity;
}

static bool is_audio_activity_exist(void)
{
    int i = 0;
    bool exist = false;

    while ((focus_mgmt_infos[i].activity_type != audio_activity) && (i < activities_cnt))
        i++;
    if (i < activities_cnt) {
        exist = true;
    } else {
        exist = false;
    }

    return exist;
}

static bool is_visual_activity_exist(void)
{
    int i = 0;
    bool exist = false;

    while ((focus_mgmt_infos[i].activity_type != visual_activity) && (i < activities_cnt))
        i++;
    if (i < activities_cnt) {
        exist = true;
    } else {
        exist = false;
    }

    return exist;
}

static void display_focus_mgmt_infos(int show_all)
{
    int i = 0;

    FOCUS_DEBUG("CHERYL DISPLAY FOCUS INFOS: \n");
    for (i = 0; i < activities_cnt; i++) {
        if (show_all) {
            FOCUS_DEBUG("focus_mgmt_infos[%d] ifs = %s is_active = %s last_focus_time_stamp = %lld "
                        "idle_time_in_ms = %lld\n",
                        i, interface2str[focus_mgmt_infos[i].ifs],
                        focus_mgmt_infos[i].is_active ? "ACTIVE" : "INACTIVE",
                        focus_mgmt_infos[i].last_focus_time_stamp,
                        focus_mgmt_infos[i].idle_time_in_ms);
        } else {
            if (focus_mgmt_infos[i].is_active) {
                FOCUS_DEBUG(
                    "focus_mgmt_infos[%d] ifs = %s is_active = %s last_focus_time_stamp = %lld "
                    "idle_time_in_ms = %lld\n",
                    i, interface2str[focus_mgmt_infos[i].ifs],
                    focus_mgmt_infos[i].is_active ? "ACTIVE" : "INACTIVE",
                    focus_mgmt_infos[i].last_focus_time_stamp, focus_mgmt_infos[i].idle_time_in_ms);
            }
        }
    }
}

static void update_activity_infos(void)
{
    int i = 0;
    for (i = 0; i < activities_cnt; i++) {
        if (focus_mgmt_infos[i].is_active) {
            focus_mgmt_infos[i].idle_time_in_ms = 0;
        } else {
            focus_mgmt_infos[i].idle_time_in_ms =
                tickSincePowerOn() - focus_mgmt_infos[i].last_focus_time_stamp;
        }
    }
    display_focus_mgmt_infos(1);
}

/*
*   json payload format:
*   "payload": {
*         "dialog": {
*         "interface": "{{STRING}}",
*         "idleTimeInMilliseconds": {{LONG}}
*         },
*         "communications": {
*         "interface": "{{STRING}}",
*         "idleTimeInMilliseconds": {{LONG}}
*         }
*   }
*
**/

static json_object *create_json_payload(focus_mgmt_activity_t activity_type)
{
    int i = 0;

    json_object *payload = json_object_new_object();
    if (!payload)
        goto ERR;

    for (i = 0; i < activities_cnt; i++) {
        if ((focus_mgmt_infos[i].activity_type != activity_type) ||
            (g_last_active_content != focus_mgmt_infos[i].ifs &&
             (focus_mgmt_infos[i].channel == content)))
            continue;

        json_object *attrs = json_object_new_object();
        if (!attrs)
            goto ERR;

        json_object_object_add(attrs, KEY_INTERFACE,
                               JSON_OBJECT_NEW_STRING(interface2str[focus_mgmt_infos[i].ifs]));
        if (activity_type == audio_activity) {
            json_object_object_add(attrs, KEY_IDLETIMEINMILLISECONDS,
                                   json_object_new_int64(focus_mgmt_infos[i].idle_time_in_ms));
        } // todo
        json_object_object_add(payload, channels2str[focus_mgmt_infos[i].channel], attrs);
    }

    return payload;
ERR:
    if (payload)
        json_object_put(payload);

    FOCUS_DEBUG("create focus management json payload error!\n");
    return NULL;
}

void focus_mgmt_context_create(json_object *js_context_list)
{

    if (NULL == js_context_list)
        goto ERR;

    update_activity_infos();

    if (is_audio_activity_exist()) {
        json_object *payload = create_json_payload(audio_activity);
        if (NULL == payload)
            goto ERR;
        alexa_context_list_add(js_context_list, NAMESPACE_AUDIOACTIVEETRACKER, NAME_ACTIVITYSTATE,
                               payload);
    }

    if (is_visual_activity_exist()) {
        json_object *payload = create_json_payload(visual_activity);
        if (NULL == payload)
            goto ERR;
        alexa_context_list_add(js_context_list, NAMESPACE_VISUALACTIVEETRACKER, NAME_ACTIVITYSTATE,
                               payload);
    }

    return;
ERR:
    FOCUS_DEBUG("create focus management context error!\n");
}

void focus_mgmt_activity_status_set(focus_mgmt_if_t ifs, bool is_active)
{
    int i = 0;
    int last_active_content = -1;
    FOCUS_DEBUG("CHERYL focus_mgmt_activity_status_set %s to %s\n", interface2str[ifs],
                is_active ? "ACTIVE" : "INACTIVE");
    while ((i < activities_cnt) && (focus_mgmt_infos[i].ifs != ifs))
        i++;

    if (i < activities_cnt) {
        focus_mgmt_infos[i].is_active = is_active;
        if (is_active) {
            focus_mgmt_infos[i].idle_time_in_ms = 0;
            if (focus_mgmt_infos[i].channel == content) {
                last_active_content = i;
            }
        } else {
            focus_mgmt_infos[i].last_focus_time_stamp = tickSincePowerOn();
            focus_mgmt_infos[i].idle_time_in_ms = 1;
        }
    } else { // this interface has not been added
        focus_mgmt_infos[activities_cnt].ifs = ifs;
        focus_mgmt_infos[activities_cnt].channel = get_channel_type(ifs);
        focus_mgmt_infos[activities_cnt].activity_type = get_activity_type(ifs);
        focus_mgmt_infos[activities_cnt].is_active = is_active;
        if (is_active && focus_mgmt_infos[activities_cnt].channel == content) {
            last_active_content = activities_cnt;
        }
        activities_cnt++;
    }

    if (is_active && last_active_content >= 0) {
        g_last_active_content = focus_mgmt_infos[last_active_content].ifs;
    }
    if (is_active) {
        FOCUS_DEBUG("CHERYL last_active_content %d g_last_active_content = %d\n",
                    last_active_content, g_last_active_content);
    }

    display_focus_mgmt_infos(0);
}

void focus_mgmt_init() { memset(focus_mgmt_infos, 0, sizeof(focus_mgmt_infos)); }
