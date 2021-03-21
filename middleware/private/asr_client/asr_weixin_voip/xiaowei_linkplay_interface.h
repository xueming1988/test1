#ifndef __XIAOWEI_INTERFACE_H__
#define __XIAOWEI_INTERFACE_H__
#include "xiaoweiAudioType.h"
#include "wm_util.h"
extern char txqq_licence[TXQQ_DEFAULT_LICENSE_LEN];
extern char txqq_guid[TXQQ_DEFAULT_GUID_LEN];
extern char txqq_product_id[TXQQ_DEFAULT_PID_LEN];
extern char txqq_appid[TXQQ_DEFAULT_APPID_LEN];
extern char txqq_key_ver[TXQQ_DEFAULT_KEY_VER_LEN];

#if defined __cplusplus || defined __cplusplus__
extern "C" {
#endif
enum {
    e_query_idle = 0,
    e_query_record_start,
    e_query_tts_start,
    e_query_tts_cancel,
    e_query_tts_end,
};

enum {
    e_wechatcall_idle = 0,
    e_wechatcall_request,
    e_wechatcall_invited,
    e_wechatcall_talking,
    e_wechatcall_end,
};

#define JSON_GET_INT_FROM_OBJECT(object, key)                                                      \
    ((object == NULL) ? 0                                                                          \
                      : ((json_object_object_get((object), (key))) == NULL)                        \
                            ? 0                                                                    \
                            : (json_object_get_int(json_object_object_get((object), (key)))))

#define JSON_GET_STRING_FROM_OBJECT(object, key)                                                   \
    ((object == NULL) ? NULL                                                                       \
                      : ((json_object_object_get((object), (key)) == NULL)                         \
                             ? NULL                                                                \
                             : (json_object_get_string(json_object_object_get((object), (key))))))
void signal_handle(void);
void wiimu_shm_init(void);
void system_global_init(void);
void setLogFunc(void);
void set_query_status(int status);
int get_query_status(void);
void set_duplex_mode(int duplex_mode);
int get_duplex_mode(void);
void set_hfp_status(int hfp_status);
int get_hfp_status(void);

void set_asr_led(int status);
void asr_set_audio_resume(void);
void set_tts_download_finished(void);
int xiaowei_wakeup_engine_init(void);
int xiaowei_socket_thread_init(void);
long get_play_pos(void);
int xiaowei_url_player_set_play_url(char *id, char *content, int type, unsigned int offset);
int xiaowei_alarm_thread_init(void);
void xiaowei_wait_login(void);
void xiaowei_wait_internet(void);
int alarm_handle(XIAOWEI_PARAM_RESPONSE *_response);
int wechat_call_name_handle(XIAOWEI_PARAM_RESPONSE *_response);
int global_control_handle(XIAOWEI_PARAM_RESPONSE *_response);
void wakeup_context_handle(XIAOWEI_PARAM_CONTEXT *_context);
void weixin_bind_ticket_callback(int errCode, const char *ticket, unsigned int expireTime,
                                 unsigned int expire_in);
unsigned int get_xiaowei_ota_flag(void);
void xiaowei_player_interrupt_enter(void);
void xiaowei_player_interrupt_exit(void);
int all_ota_disabled(void);
int force_ota_disabled(void);
void do_wechatcall(void);
int WechatcallRingPlayThread(void);
void onWakeUp_context(void);
void wechatcall_uninittimer_handle(void);
void xiaowei_wechatcall_recv_func(void);
int wechatcall_settimer_handle(int delayms);
int xiaowei_set_music_quality(int quality);
void OnSilence(void);
unsigned long long get_xiaowei_tencent_version(void);

#if defined __cplusplus || defined __cplusplus__
}
#endif
#endif
