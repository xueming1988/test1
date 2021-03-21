
#ifndef __ASR_NOTIFICAION_H__
#define __ASR_NOTIFICAION_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct json_object json_object;

int asr_notificaion_init(void);

void asr_notificaion_uninit(void);

int asr_notificaion_clear(void);

int asr_notificaion_parse_directive(json_object *js_obj);

int asr_notificaion_add_local_notify(char *js_str);

void asr_notification_resume(void);

#ifdef __cplusplus
}
#endif

#endif
