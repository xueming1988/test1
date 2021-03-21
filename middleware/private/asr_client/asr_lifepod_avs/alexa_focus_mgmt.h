#ifndef __ALEXA_FOCUS_MGMT_H__
#define __ALEXA_FOCUS_MGMT_H__

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FOCUS true
#define UNFOCUS false

typedef struct json_object json_object;

typedef enum { audio_activity = 1, visual_activity } focus_mgmt_activity_t;

typedef enum { dialog = 1, communications, alert, content, visual } focus_mgmt_chn_t;

typedef enum {
    SpeechSynthesizer = 1,
    SipUserAgent,
    Alerts,
    AudioPlayer,
    ExternalMediaPlayer,
    Bluetooth,
    TemplateRuntime
} focus_mgmt_if_t;

typedef struct {
    focus_mgmt_activity_t activity_type;
    focus_mgmt_chn_t channel;
    focus_mgmt_if_t ifs; // interface
    long long idle_time_in_ms;
    long long last_focus_time_stamp;
    bool is_active;
} focus_mgmt_info_t;

#define MAX_INTERFACE_COUNT 7

void focus_mgmt_context_create(json_object *js_context_list);

void focus_mgmt_activity_status_set(focus_mgmt_if_t ifs, bool active);

void focus_mgmt_init(void);

#ifdef __cplusplus
}
#endif

#endif
