
#ifndef __LGUPLUS_EVENT_QUEUE_H__
#define __LGUPLUS_EVENT_QUEUE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd_q.h"

#define KEY_BODY ("body")
#define KEY_SCHEMA ("schema")
#define KEY_METHOD ("method")
#define KEY_MESSAGE_ID ("message_id")
#define KEY_TRANSACTION_ID ("transaction_id")

#define KEY_STATUS ("status")
#define DEFAULT_STR ("")
#define DEFAULT_BODY ("{}")

#define Format_Event                                                                               \
    ("{"                                                                                           \
     "\"schema\":\"%s\","                                                                          \
     "\"method\":\"%s\","                                                                          \
     "\"message_id\":\"%s\","                                                                      \
     "\"transaction_id\":\"%s\","                                                                  \
     "\"body\":%s"                                                                                 \
     "}")

/*
"PlayStop": {
"StreamItemId": "A12345",
"offsetPlayIntime ":" 57000 "
}
*/

#define Format_AudioBody                                                                           \
    ("{"                                                                                           \
     "\"%s\":"                                                                                     \
     "{"                                                                                           \
     "\"streamItemId\":\"%s\","                                                                    \
     "\"playOffsetIntime\":\"%ld\""                                                                \
     "}"                                                                                           \
     "}")

#define Create_Event(buff, schema, method, message_id, transaction_id, params)                     \
    do {                                                                                           \
        asprintf(&(buff), Format_Event, schema, method, message_id, transaction_id, params);       \
    } while (0)

#define Create_AudioBody(buff, method, audio_token, position)                                      \
    do {                                                                                           \
        asprintf(&(buff), Format_AudioBody, method, audio_token, position);                        \
    } while (0)

#ifdef __cplusplus
extern "C" {
#endif

cmd_q_t *lguplus_event_queue_init(void);

int lguplus_event_queue_uninit(void);

void lguplus_event_queue_clear_recognize(void);

int lguplus_event_queue_add(char *schema, char *method, char *message_id, char *transaction_id);

int lguplus_event_queue_add_body(char *schema, char *method, char *message_id, char *transaction_id,
                                 char *body);

int lguplus_event_queue_audio(char *method, char *audio_token, long position);

#ifdef __cplusplus
}
#endif

#endif
