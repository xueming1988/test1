
#ifndef __ALEXA_GUI_H__
#define __ALEXA_GUI_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct json_object json_object;

#ifndef bool
typedef int bool;
#endif

int avs_gui_init(void);

int avs_gui_uninit(void);

// judge that directive is gui or not, if is gui directive return 0;
int avs_gui_directive_check(char *name_space);

// parse directive
int avs_gui_parse_directive(json_object *js_data);

// parse the cmd from other thread to avs event
// TODO: need free after used
char *avs_gui_parse_cmd(char *json_cmd_str);

int avs_gui_state(json_object *js_context_list, char *event_name);

json_object *avs_gui_event_create(json_object *json_cmd);

#ifdef __cplusplus
}
#endif

#endif
