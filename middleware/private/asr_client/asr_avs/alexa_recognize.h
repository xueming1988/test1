
/**
 *
 *
 *
 *
 *
 */

#ifndef __ALEXA_RECOGNIZE_H__
#define __ALEXA_RECOGNIZE_H__

#include <stdio.h>
#include <string.h>

typedef struct json_object json_object;

enum {
    e_tlk_idle,
    e_tlk_start,
    e_tlk_end,
};

#ifdef __cplusplus
extern "C" {
#endif

int avs_recognize_init(void);

int avs_recognize_uninit(void);

int avs_recognize_parse_directive(json_object *js_obj);

int avs_recognize_state(json_object *json_context_list);

int avs_recognize_get_state(void);

int avs_recognize_set_state(int flags);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_PROCESSOR_H */
