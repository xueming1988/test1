
#ifndef __ALEXA_REQUEST_JSON_H__
#define __ALEXA_REQUEST_JSON_H__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

void alexa_set_recognize_info(int64_t start_sample, int64_t end_sample);

int alexa_set_initiator(json_object *initiator_js);

/*
"header": {
    "namespace": "SpeechRecognizer",
    "name": "Recognize",
    "messageId": "{{STRING}}",
    "dialogRequestId": "{{STRING}}"
},
*/
/*
 * Create event header
 * Input :
 *         @type: the type of event
 *         @name: the name of event
 *         @message_id: the message_id of event
 *         @dialog_request_id: the dialog_request_id of event
 *
 * Output: an json
 */
json_object *alexa_event_header(char *name_space, char *name);

char *alexa_create_event_body(char *state_json, char *cmd_json, char **outURI);

int alexa_context_list_add(json_object *context_list, char *name_space, char *name,
                           json_object *payload);

json_object *alexa_event_body(json_object *header, json_object *payload);

int alexa_alerts_list_add(json_object *alert_list, char *token, char *type, char *scheduled_time);

#ifdef __cplusplus
}
#endif

#endif
