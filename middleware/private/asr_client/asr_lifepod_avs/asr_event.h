
#ifndef __ASR_EVENT_H__
#define __ASR_EVENT_H__

#define E_ERROR_UNAUTH (-1)
#define E_ERROR_INIT (-2)
#define E_ERROR_OFFLINE (-3)
#define E_ERROR_TIMEOUT (0)

#define E_HTTPRET_200 (200)
#define E_HTTPRET_204 (204)

#define E_HTTPRET_300 (300)

#define E_HTTPRET_400 (400)
// UNAUTH
#define E_HTTPRET_403 (403)
#define E_HTTPRET_429 (429)
#define E_HTTPRET_500 (500)
#define E_HTTPRET_503 (503)

#ifdef __cplusplus
extern "C" {
#endif

int asr_event_findby_name(char *cmd_js_str, char *event_name);

/*
  *  Judge cmd_js_str is cmd_type/new cmd or not
  *  retrun:
  *        < 0: not an json cmd
  *        = 0: is an json cmd
  *        = 1: is the cmd_type cmd
  */
int asr_event_cmd_type(char *cmd_js_str, char *cmd_type);

int asr_event_exec(char *state_json, char *cmd_js_str, void *upload_data, int time_out);
int lifepod_event_exec(char *state_json, char *cmd_js_str, void *upload_data, int time_out);

int asr_event_down_channel(void);
int lifepod_event_down_channel(void);

void asr_event_sync_state(void);

#ifdef __cplusplus
}
#endif

#endif
