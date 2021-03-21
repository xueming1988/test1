
#ifndef __CLOVA_DND_H__
#define __CLOVA_DND_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct json_object json_object;

int clova_dnd_init(void);

void clova_dnd_uninit(void);

int clova_get_dnd_on(void);

int clova_set_dnd_on(int dnd_on);

int clova_set_dnd_info(char *state, char *start_time, char *end_time);

int clova_set_dnd_expire_time(char *expire_time);

int clova_clear_dnd_expire_time(void);

int clova_get_dnd_state(json_object *dnd_state);

void *send_report(void *arg);

#ifdef __cplusplus
}
#endif

#endif
