#ifndef __XIAOWEI_INTERFACE_H__
#define __XIAOWEI_INTERFACE_H__

#include "json.h"
#include "xiaoweiAudioType.h"

#define JSON_GET_INT_FROM_OBJECT(object, key)                                                      \
    ((object == NULL) ? 0                                                                          \
                      : ((json_object_object_get((object), (key))) == NULL)                        \
                            ? 0                                                                    \
                            : (json_object_get_int(json_object_object_get((object), (key)))))

#define JSON_GET_INT64_FROM_OBJECT(object, key)                                                    \
    ((object == NULL) ? 0                                                                          \
                      : ((json_object_object_get((object), (key))) == NULL)                        \
                            ? 0                                                                    \
                            : (json_object_get_int64(json_object_object_get((object), (key)))))

#define JSON_GET_STRING_FROM_OBJECT(object, key)                                                   \
    ((object == NULL) ? NULL                                                                       \
                      : ((json_object_object_get((object), (key)) == NULL)                         \
                             ? NULL                                                                \
                             : (json_object_get_string(json_object_object_get((object), (key))))))

enum {
    e_query_idle = 0,
    e_query_record_start,
    e_query_tts_start,
    e_query_tts_cancel,
};
void set_asr_led(int status);
void onWakeUp(int req_from);
int weixin_xiaowei_push_rec(char *audio_data, int data_length);
void xiaoweiInitRecordData(void);
int xiaoweiWriteRecordData(char *data, size_t size);
int xiaoweiReadRecordData_continue(void);
int xiaoweiReadRecordData(char *buf, unsigned int len);
int xiaoweiStopWriteData(void);
void xiaoweiRingBufferInit(void);
void OnSilence(void);
int OnCancel(void);
void set_tts_download_finished(void);
int alarm_handle(XIAOWEI_PARAM_RESPONSE *_response);
long get_play_pos(void);
void wakeup_context_handle(XIAOWEI_PARAM_CONTEXT *_context);
void onWakeUp_context(void);

#endif
