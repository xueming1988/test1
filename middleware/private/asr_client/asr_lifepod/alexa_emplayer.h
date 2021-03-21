
#ifndef __ALEXA_EMPLAYER_H__
#define __ALEXA_EMPLAYER_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct json_object json_object;

int avs_emplayer_init(void);

int avs_emplayer_uninit(void);

// judge that directive is emplayer or not, if is emplayer directive return 0;
int avs_emplayer_directive_check(char *name_space);

// parse directive
int avs_emplayer_parse_directive(json_object *js_data);

int avs_emplayer_notify_logout(void);

#ifdef __cplusplus
}
#endif

#endif
