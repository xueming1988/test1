
#ifndef __ASR_EVENT_H__
#define __ASR_EVENT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "http2_client.h"
#include "multi_partparser.h"

/*
  *  Judge cmd_js_str is cmd_type/new cmd or not
  *  retrun:
  *        < 0: not an json cmd
  *        = 0: is an json cmd
  *        = 1: is the cmd_type cmd
  */
int asr_event_cmd_type(char *cmd_js_str, char *cmd_type);

int asr_event_exec(char *state_json, char *cmd_js_str, void *upload_data, int time_out);

int asr_event_down_channel(void);

int asr_event_ping(void);

#ifdef __cplusplus
}
#endif

#endif
