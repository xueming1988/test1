
#ifndef __ASR_ALERT_H__
#define __ASR_ALERT_H__

#ifdef __cplusplus
extern "C" {
#endif

int alert_start(void);
int alert_get_all_alert(json_object *pjson_head, json_object *pjson_active_head);
int alert_set(char *ptoken, char *ptype, char *pschtime);
int alert_set_with_json(char *ptoken, char *ptype, char *pschtime, char *json_string);
int alert_stop(char *ptoken);
int alert_clear(char *ptype);
void alert_stop_current(void);

int asr_clear_alert_cfg(void);

#ifdef __cplusplus
}
#endif

#endif
