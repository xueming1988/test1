
#ifndef __ALEXA_BLUETOOTH_H__
#define __ALEXA_BLUETOOTH_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct json_object json_object;

int avs_bluetooth_init(void);

int avs_bluetooth_uninit(void);

void avs_bluetooth_getpaired_list(void);

int avs_bluetooth_set_inactivae(void);

int avs_bluetooth_need_slience(void);

// parse directive
int avs_bluetooth_parse_directive(json_object *js_data);

void avs_bluetooth_do_cmd(int flag);

// parse the cmd from bluetooth thread to avs event
// TODO: need free after used
char *avs_bluetooth_parse_cmd(char *json_cmd_str);

int avs_bluetooth_state(json_object *js_context_list);

json_object *avs_bluetooth_event_create(json_object *json_cmd);

#ifdef __cplusplus
}
#endif

#endif
