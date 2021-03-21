
#ifndef __ALEXA_EMPLAYER_H__
#define __ALEXA_EMPLAYER_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct json_object json_object;

int avs_emplayer_init(void);

int avs_emplayer_uninit(void);

int avs_emplayer_notify_online(int online);

// judge that directive is emplayer or not, if is emplayer directive return 0;
int avs_emplayer_directive_check(char *name_space);

// parse directive
int avs_emplayer_parse_directive(json_object *js_data);

// parse the cmd from spotify thread to avs event
// TODO: need free after used
char *avs_emplayer_parse_cmd(char *json_cmd_str);

int avs_emplayer_state(json_object *js_context_list, char *event_name);

json_object *avs_emplayer_event_create(json_object *json_cmd);

#ifdef __cplusplus
}
#endif

#endif
