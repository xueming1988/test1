

#ifndef __ASR_LIGHT_EFFECTS_H__
#define __ASR_LIGHT_EFFECTS_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    e_effect_none = -1,
    e_effect_idle,
    e_effect_listening,
    e_effect_listening_off,
    e_effect_thinking,
    e_effect_unthinking,
    e_effect_speeking,
    e_effect_speeking_off,
    e_effect_error,
    e_effect_notify_on,
    e_effect_notify_off,
    e_effect_message_on,
    e_effect_message_off,
    e_effect_ota_on,
    e_effect_ota_off,
    e_effect_music_play,

    e_effect_max,
} e_effect_type_t;

int asr_light_effects_set(e_effect_type_t type);

#ifdef __cplusplus
}
#endif

#endif
