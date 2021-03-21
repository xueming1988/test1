
#ifndef __ASR_CMD_H__
#define __ASR_CMD_H__

#include <stdio.h>
#include <string.h>

#include "cmd_q.h"
#include "mv_message.h"

#define ASR_TTS ("/tmp/RequestASRTTS")
#define ASR_EVENT_PIPE ("/tmp/asr_event")
#define ASR_LOCAL_SOCKET ("/tmp/local.socket.asr")

// { for socket return
#define CMD_OK ("OK")
#define CMD_FAILED ("Failed")
#define CMD_CFG_FAILED ("Save cfg failed")

// }

// { for ASRTTS

#define ASR_RECO_START ("asrRecognizeStart")
#define ASR_RECO_CONTINUE ("asrRecognizeStart:2")
#define ASR_RECO_STOP ("asrRecognizeStop")
#define ASR_DIS_CONNECT ("asrDisConnect")
#define ASR_PING ("asrPing")
#define ASR_INACTIVE_REPORT ("asrInactiveReport")

#define ASR_IS_LOGIN ("isAsrLogin")
#define ASR_LOGIN ("asrLogin:")
#define ASR_LOGOUT ("asrLogOut")
#define ASR_POWEROFF ("powerOff")
#define ASR_SET_ENDPOINT ("asrSetEndPoint")
#define ASR_SET_LAN ("asrSetLanguage")
#define ASR_EXIT ("asrExit")
#define ASR_LOCAL_NOTIFY ("asrLocalNotify")
#define ASR_WPS_START ("asrWPSStart")
#define ASR_WPS_END ("asrWPSEnd")
#define ASR_FREE_TALK_START ("asrFreeTalkStart")
#define ASR_STOP_FREE_TALK ("asrStopFreeTalk")

// }

// { for support the old comannd

#define EVENT_PLAY_NEXT ("asrNext")
#define EVENT_PLAY_PREV ("asrPrev")
#define EVENT_PLAY_PAUSE ("asrPause")
#define EVENT_PLAY_PLAY ("asrPlay")

#define EVENT_ALERT_START ("asrAlertStart")

#define EVENT_ALERT_STOP ("asrAlertStop")
#define EVENT_ALERT_LOCAL_DELETE ("asrDelAlertLocal")
#define EVENT_DEL_ALERT_OK ("asrDelAlertSucessed")

#define EVENT_SET_ALERT_OK ("asrSetAlertSucessed")
#define EVENT_SET_ALERT_ERR ("asrSetAlertError")

#define EVENT_MUTE_CHANGED ("asrMuteChanged")
#define EVENT_VOLUME_CHANGED ("asrVolumeChanged")

// }

#define CAP_RESET ("CAP_RESET")
#define TLK_STOP_N ("talkstop:%d")
#define TLK_CONTINUE ("talkcontinue:-1013")
#define VAD_FINISHED ("VAD_FINISH")

#define NOTIFY_CMD(socket, cmd)                                                                    \
    do {                                                                                           \
        if ((cmd) && (strlen(cmd) > 0)) {                                                          \
            SocketClientReadWriteMsg(socket, (char *)(cmd), strlen((char *)(cmd)), NULL, NULL, 0); \
        }                                                                                          \
    } while (0)

typedef struct json_object json_object;

#ifdef __cplusplus
extern "C" {
#endif

cmd_q_t *asr_queue_init(void);
int asr_queue_uninit(void);
void asr_queue_clear_audio(void);
void asr_queue_clear_speech(void);
void asr_queue_clear_report_state(void);
int clova_audio_general_cmd(char *cmd_name, char *token, long pos);
int clova_audio_report_state_cmd(char *cmd_name);
int clova_audio_stream_request_cmd(char *cmd_name, char *audioItemId, json_object *audioStream);
int clova_playback_general_cmd(char *cmd_name, char *device_id);
int clova_settings_request_sync_cmd(char *cmd_name, char *deviceId, int syncWithStorage);
int clova_settings_report_cmd(char *cmd_name);
int clova_settings_request_store_cmd(char *cmd_name);
int clova_speech_general_cmd(char *cmd_name, char *token);
int clova_speech_request_cmd(char *text, char *lang);
int clova_alerts_general_cmd(char *cmd_name, char *token, char *type);
int clova_alerts_request_sync_cmd(char *cmd_name);
int clova_device_control_action_cmd(char *name, char *target, char *command);
int clova_device_control_report_cmd(char *name);
int cmd_add_alert_event(char *name, char *alert_token, char *alert_type);
int cmd_add_playback_event(char *name);
int cmd_add_normal_event(char *name_space, char *name, char *payload);

#ifdef __cplusplus
}
#endif

#endif
