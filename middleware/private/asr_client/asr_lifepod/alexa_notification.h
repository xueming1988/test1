
#ifndef __AVS_NOTIFICAION_H__
#define __AVS_NOTIFICAION_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct json_object json_object;

int avs_notificaion_init(void);

void avs_notificaion_uninit(void);

int avs_notificaion_clear(void);

int avs_notificaion_parse_directive(json_object *js_obj);

int avs_notificaion_state(json_object *json_context_list);

#ifdef __cplusplus
}
#endif

#endif
