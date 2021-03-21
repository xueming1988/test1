#ifndef __ALEXA_COMMS_H__
#define __ALEXA_COMMS_H__

#include <stdio.h>
#include <string.h>
#include "alexa_request_json.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { e_state_online = 0, e_state_internet };

enum {
    e_off = 0,
    e_on = 1,
};

enum ringtone_way { RINGTONE_NA = 0, RINGTONE_FILE, RINGTONE_URL, RINGTONE_SUA };

enum dsp_mode {
    DSP_MODE_ASR = 0, // default mode
    DSP_MODE_VOIP
};

int avs_comms_init(void);

int avs_comms_uninit(void);

// parse directive
int avs_comms_parse_directive(json_object *js_data);

// parse the cmd from bluetooth thread to avs event
// TODO: need free after used
char *avs_comms_parse_cmd(char *json_cmd_str);

void avs_comms_state_cxt(json_object *js_context_list);

void avs_comms_state_change(int type, int status);

void avs_comms_notify(char *notify_data);

void avs_comms_stop_ux(void);

void avs_comms_resume_ux(void);

json_object *avs_comms_event_create(json_object *json_cmd);

int avs_comms_is_avtive(void);

void start_comms_monitor(void);

#ifdef __cplusplus
}
#endif

#endif
