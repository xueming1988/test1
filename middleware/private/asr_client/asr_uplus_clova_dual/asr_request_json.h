
#ifndef __ASR_REQUEST_JSON_H__
#define __ASR_REQUEST_JSON_H__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

int asr_alerts_list_add(json_object *alert_list, char *token, char *type, char *scheduled_time);

char *asr_create_event_body(char *state_json, char *cmd_json);

int asr_context_list_add(json_object *context_list, char *name_space, char *name,
                         json_object *payload);

#ifdef __cplusplus
}
#endif

#endif
