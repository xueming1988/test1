
#ifndef __ASR_SYSTEM_H__
#define __ASR_SYSTEM_H__

#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

int asr_system_init(void);

int asr_system_uninit(void);

int asr_system_add_ping_timer(int ping_type);

int asr_system_add_report_state(int duration, int interval);

int asr_system_add_dnd_start(int start_time);

int asr_system_add_dnd_end(int end_time);

int asr_system_add_dnd_expire(int expire_time);

#ifdef __cplusplus
}
#endif

#endif
