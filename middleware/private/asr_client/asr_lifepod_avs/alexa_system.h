
#ifndef __ALEXA_SYSTEM_H__
#define __ALEXA_SYSTEM_H__

#include <stdio.h>
#include <string.h>

typedef struct json_object json_object;

#ifdef __cplusplus
extern "C" {
#endif

int avs_system_init(void);
int lifepod_system_init(void);

int avs_system_uninit(void);
int lifepod_system_uninit(void);

int avs_system_parse_directive(json_object *js_obj);

int avs_system_add_ping_timer(void);
int lifepod_system_add_ping_timer(void);

int avs_system_update_inactive_timer(void);

int avs_system_set_expect_speech_timed_out(int clear_flags);

int avs_system_update_soft_version(void);

void avs_system_clear_soft_version_flag(void);

int avs_system_get_changeendpoint(void);

int avs_system_set_expect_speech_timed_out(int clear_flags);

#ifdef __cplusplus
}
#endif

#endif
