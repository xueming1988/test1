
#ifndef __ALEXA_EVENT_H__
#define __ALEXA_EVENT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "membuffer.h"
#include "alexa_nghttp2.h"

#ifdef EN_AVS_ANNOUNCEMENT
#include "avs_multipart.h"

/// Size of CLRF in chars
#define LEADING_CRLF_CHAR_SIZE (2)
/// ASCII value of CR
#define CARRIAGE_RETURN_ASCII (13)
/// ASCII value of LF
#define LINE_FEED_ASCII (10)

#endif

typedef struct _request_result_s {
    int is_talk;
    int is_next;
    int is_first_chunk;
    int data_flag;
    int octet_index;
    int recv_head;
    int content_type;
    membuffer result_header;
    membuffer result_body;
#ifdef EN_AVS_ANNOUNCEMENT
    char *json_cache;
    avs_multi_parse_t *multi_paser;
#endif
} request_result_t;

#define TTS_START (0)    // to start the tts player
#define TTS_PLAYING (1)  // to do something when tts is playing
#define TTS_FINISHED (2) // to stop the tts player
#define TTS_STOPED (3)   // to do something when tts is stoped

typedef int(result_exec_cb_t)(int type, int http_ret, void *result, void *result_ctx);

// calloc out need to free it
char *alexa_event_get_boundary(char *header);

int alexa_check_audio_event(char *cmd_js_str);

int alexa_check_event_name(char *cmd_js_str, char *event_name);

/*
  *  Judge cmd_js_str is cmd_type/new cmd or not
  *  retrun:
  *        < 0: not an json cmd
  *        = 0: is an json cmd
  *        = 1: is the cmd_type cmd
  */
int alexa_check_cmd(char *cmd_js_str, char *cmd_type);

int alexa_do_event(char *state_json, char *cmd_js_str, request_cb_t *head_cb, request_cb_t *data_cb,
                   upload_cb_t *upload_cb, void *upload_data, result_exec_cb_t *result_exec,
                   void *result_ctx, int time_out);

int alexa_down_channel(request_cb_t *head_cb, request_cb_t *data_cb);

#ifdef __cplusplus
}
#endif

#endif
