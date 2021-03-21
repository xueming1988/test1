
#ifndef __ALEXA_CMD_H__
#define __ALEXA_CMD_H__

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "mv_message.h"

#define ALEXA_CMD_FD ("/tmp/ALEXA_EVENT")
#define LIFEPOD_CMD_FD ("/tmp/LIFEPOD_EVENT")
#define IO_GUARD ("/tmp/RequestIOGuard")
#define AMAZON_TTS ("/tmp/AMAZON_ASRTTS")
#define ASR_TTS ("/tmp/RequestASRTTS")
#define NET_GUARD ("/tmp/RequestNetguard")
#define COMMS_PIPE ("/tmp/ALEXA_COMMS")
#define A01Controller ("/tmp/Requesta01controller")
#ifdef AVS_IOT_CTRL
#define AWS_CLIENT ("/tmp/request_aws_iot_client")
#endif

// { for socket return
#define CMD_OK ("OK")
#define CMD_FAILED ("Failed")
#define CMD_INIT_CFGOK ("InitCfgOK")
#define CMD_NO_TOKEN ("InitCfgOK no token")
#define CMD_CFG_FAILED ("Save cfg failed")

// }

// { for ASRTTS

#define ASR_RECO_START ("amazonRecognizeStart")
#define ASR_RECO_STOP ("amazonRecognizeStop")
#define ASR_INIT_CFG ("InitAlexaAuthCfg")
#define ASR_IS_LOGIN ("isAlexaLogin")
#define ASR_LOGIN ("setAmazonAccessToken:")
#define ASR_LOGOUT ("alexaLogOut")
#define ASR_POWEROFF ("alexaPowerOff")
#define ASR_DISCONNECT ("asrDisconnect")
#define ASR_SET_TOKEN_PARAM ("setTokenParams:")
#define ASR_SET_ACCESS_TOKEN ("setLifepodAccessToken:")
#define ASR_AMAZON_OAUTHCODE ("setAmazonOAuthCode:")
#define ASR_GET_PCO_SKILL_STATUS ("getPcoSkillStatus:")
#define ASR_PCO_ENABLE_SKILL ("enableAlexaSkill:")
#define ASR_PCO_DISABLE_SKILL ("disablePcoSkill:")

#define LFPD_ASR_RECO_START ("lifepodRecognizeStart")
#define ASR_SET_LIFEPOD_AUTHCODE ("setLifepodAuthCode:")

// }

// { for support the old comannd
#define EVENT_ALERT_START ("alexaAlertStart")
#define EVENT_ALERT_STOP ("alexaAlertStop")
#define EVENT_ALERT_LOCAL_DELETE ("alexaDelAlertLocal")
#define EVENT_ALERT_FORCEGROUND ("alexaAlertForceground")
#define EVENT_ALERT_BACKGROUND ("AlertEnteredBackground")
#define EVENT_VOLUME_CHANGED ("alexaVolumeChanged")
#define EVENT_MUTE_CHANGED ("alexaMuteChanged")
#define EVENT_PLAY_NEXT ("alexaPlayNext")
#define EVENT_PLAY_PREV ("alexaPlayPrev")
#define EVENT_PLAY_PAUSE ("alexaPlayPause")
#define EVENT_PLAY_PLAY ("alexaPlayPlay")
#define EVENT_SET_ALERT_OK ("alexaSetAlertSucessed")
#define EVENT_SET_ALERT_ERR ("alexaSetAlertError")
#define EVENT_DEL_ALERT_OK ("alexaDelAlertSucessed")
#define EVENT_DEL_ALERT_ERR ("alexaDelAlertError")

#define EVENT_BOOK_PUASE ("alexaBookPause")
#define EVENT_BOOK_PLAY ("alexaBookPlay")

#define EVENT_SET_ENDPOINT ("alexaSetEndPoint")
#define EVENT_LOCAL_UPDATE ("alexaLocalUpdate")

#define EVENT_LAN_GET ("alexaLanGet")
#define EVENT_LANLIST_GET ("alexaLanListGet")

// process report related
#define EVENT_REPORT_DELAY ("alexaReportDelay")
#define EVENT_REPORT_INTERVAL ("alexaReportInterval")

// user
#define EVENT_ALEXA_EXIT ("alexaExit")

#define EVENT_STATE_UPDATE ("alexaStateUpdate")
#define EVENT_SIPCLIENT ("EventSipClient")

#define EVENT_CLEAR_NEXT ("alexaClearNext")
#define EVENT_CLEAR_CMD_QUEUE ("alexaClearCmdQueue")

#define EVENT_RESUME_RINGTONE ("resume_ring_tone")

//#define EVENT_BUFF_LEN                  (512)
#define PLAY_POS_LEN (64)

// play event cmd: {command:pos:token}
#define GET_PLAY_POS ("%*[^:]:%[^:]")

// }

#ifdef PURE_RPC
#define ALEXA_PLAY_LOGOUT ("setPlayerCmd:alexa:logout")
#endif
#define ALEXA_PLAY_STOP ("setPlayerCmd:alexa:stop")
#define ALEXA_PLAY_PAUSEEX ("setPlayerCmd:pauseEx")
#define CAP_RESET ("CAP_RESET")

#define PROMPT_START ("GNOTIFY=volume_prompt_start")
#define PROMPT_END ("GNOTIFY=volume_prompt_end")
#define TTS_IDLE ("GNOTIFY=MCULEDIDLE")
#define TTS_IS_PLAYING ("GNOTIFY=MCULEDPLAYTTS")
#define TTS_ERROR ("GNOTIFY=MCULEDRECOERROR")
#define CHANGE_BOOST ("alexaChangeBoost")

// TODO: should change project related string
#ifdef S11_EVB_EUFY_DOT
#define Alexa_LoginFailed ("EufyAlexaLoginFail")
#define Alexa_LoginOK ("EufyAlexaLoginSuccess")
#define Alexa_Logout ("EufyAlexaLogOutNotify")
#define Alexa_LostInternet ("GNOTIFY=EufyAlexaInternetLost")
#else
#define Alexa_LoginFailed ("AlexaLoginFail")
#define Alexa_LoginOK ("AlexaLoginSuccess")
#define Alexa_Logout ("AlexaLogOutNotify")
#define Alexa_LostInternet ("GNOTIFY=AlexaInternetLost")
#endif

#define CALL_MUTE_CTRL_1 ("call_mute_ctrl:1")
#define CALL_MUTE_CTRL_0 ("call_mute_ctrl:0")
#define CALL_NET_STATE_1 ("report_network_conn_state:1")
#define CALL_NET_STATE_0 ("report_network_conn_state:0")
#define CALL_CONN_STATE_1 ("report_alexa_conn_state:1")
#define CALL_CONN_STATE_0 ("report_alexa_conn_state:0")
#define CALL_CHECK_NET ("comms_check_network_conn_stat")
#define CALL_CHECK_CONN ("comms_check_alexa_conn_stat")

#define REPORT_ALEXA_LOGIN ("report_alexa_login")
#define REPORT_ALEXA_LOGOUT ("report_alexa_logout")

#define FocusManage_TTSBegin ("FocusManage:TTSBegin")
#define FocusManage_TTSEnd ("FocusManage:TTSEnd")
#define FocusManage_COMMSSTART ("FocusManage:COMMSSTART")
#define FocusManage_COMMSEND ("FocusManage:COMMSEND")

#define IO_FOCUS_CALL_0 ("GNOTIFY=FOCUSCALL:0")
#define IO_FOCUS_CALL_1 ("GNOTIFY=FOCUSCALL:1")

#define TLK_STOP_N ("talkstop:%d")
#define TLK_CONTINUE ("talkcontinue:-1013")

#define VAD_FINISHED ("VAD_FINISH")
#define AVS_UNAUTH ("unauthorize_res_exception")
#define AVS_LOGOUT ("alexaLogOut")

#define NOTIFY_CMD(socket, cmd)                                                                    \
    do {                                                                                           \
        if (strlen(cmd) > 0) {                                                                     \
            SocketClientReadWriteMsg(socket, (char *)(cmd), strlen((char *)(cmd)), NULL, NULL, 0); \
        }                                                                                          \
    } while (0)

#ifdef __cplusplus
extern "C" {
#endif

int alexa_audio_cmd(int cmd_id, char *cmd_name, char *token, long pos, long duration, int err_num);

int alexa_playback_cmd(int cmd_id, char *cmd_name);

int alexa_speech_cmd(int cmd_id, char *cmd_name, char *token);

int alexa_speeker_cmd(int cmd_id, char *cmd_name, int volume, int muted);

int alexa_alerts_cmd(int cmd_id, char *cmd_name, const char *token);

int alexa_system_cmd(int cmd_id, char *cmd_name, long inactive_ts, char *err_msg);
int lifepod_system_cmd(int cmd_id, char *cmd_name, long inactive_ts, char *err_msg);

int alexa_settings_cmd(int cmd_id, char *cmd_name, char *language);

int alexa_string_cmd(char *cmd_prefix, char *string);

/*add by weiqiang.huang for MAR-385 Equalizer controller 2018-08-30 -begin*/
int alexa_equalizer_controller_cmd(int cmd_id, char *cmd_name, const char *payload_param);
/*add by weiqiang.huang for MAR-385 Equalizer controller 2018-08-30 -end*/

int alexa_donotdisturb_cmd(int cmd_id, char *cmd_name, bool enabled);

#ifdef __cplusplus
}
#endif

#endif
