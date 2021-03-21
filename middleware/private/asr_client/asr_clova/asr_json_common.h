
#ifndef __ASR_JSON_COMMON_H__
#define __ASR_JSON_COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"

#define PLAYER_IDLE ("IDLE")
#define PLAYER_PLAYING ("PLAYING")
#define PLAYER_PAUSED ("PAUSED")
#define PLAYER_UNDERRUN ("BUFFER_UNDERRUN")
#define PLAYER_FINISHED ("FINISHED")
#define PLAYER_STOPPED ("STOPPED")

/*
 * events namespace type
 */
#define NAMESPACE_SPEECHRECOGNIZER ("SpeechRecognizer")
#define NAMESPACE_ALERTS ("Alerts")
#define NAMESPACE_AUDIOPLAYER ("AudioPlayer")
#define NAMESPACE_PLAYBACKCTROLLER ("PlaybackController")
#define NAMESPACE_SPEAKER ("Speaker")
#define NAMESPACE_SPEECHSYNTHESIZER ("SpeechSynthesizer")
#define NAMESPACE_SYSTEM ("System")
#define NAMESPACE_SETTINGS ("Settings")
#define NAMESPACE_NOTIFIER ("Notifier")

#define PAYLOAD_wakeword ("wakeword")
#define PAYLOAD_initiator ("initiator")
#define PAYLOAD_wakeWordIndices ("wakeWordIndices")
#define PAYLOAD_startIndexInSamples ("startIndexInSamples")
#define PAYLOAD_endIndexInSamples ("endIndexInSamples")

/*
 * name for SpeechRecognizer
 */
#define NAME_RECOGNIZE ("Recognize")
#define NAME_RecognizerState ("RecognizerState")
#define NAME_EXPECTSPEECH ("ExpectSpeech")
#define NAME_SHOWRECOGNIZEDTEXT ("ShowRecognizedText")
#define NAME_EXPECTSPEECHTIMEOUT ("ExpectSpeechTimedOut")
#define PAYLOAD_LISTONTIMEOUT ("timeoutInMilliseconds")

#define KEY_initiator ("initiator")
#define KEY_inputSource ("inputSource")
#define KEY_deviceUUID ("deviceUUID")
#define KEY_wakeWord ("wakeWord")
#define KEY_confidence ("confidence")
#define KEY_indices ("indices")
#define KEY_startIndexInSamples ("startIndexInSamples")
#define KEY_endIndexInSamples ("endIndexInSamples")

/*
 * name for Alerts
 */
#define NAME_SETALERT ("SetAlert")
#define NAME_SETALERT_SUCCEEDED ("SetAlertSucceeded")
#define NAME_SETALERT_FAILED ("SetAlertFailed")

#define NAME_DELETE_ALERT ("DeleteAlert")
#define NAME_DELETE_ALERT_SUCCEEDED ("DeleteAlertSucceeded")
#define NAME_DELETE_ALERT_FAILED ("DeleteAlertFailed")

#define NAME_ALERT_STARTED ("AlertStarted")

#define NAME_STOP_ALERT ("StopAlert")
#define NAME_ALERT_STOPPED ("AlertStopped")

#define NAME_ALERT_REQUEST_SYNC ("RequestSynchronizeAlert")
#define NAME_ALERT_SYNC ("SynchronizeAlert")

#define NAME_CLEAR_ALERT ("ClearAlert")
#define NAME_CLEAR_ALERT_FAILED ("ClearAlertFailed")
#define NAME_CLEAR_ALERT_SUCCEEDED ("ClearAlertSucceeded")

/*
 * name for AudioPlayer events
 */
#define NAME_PLAY ("Play")
#define NAME_STOP ("Stop")
#define NAME_CLEARQUEUE ("ClearQueue")

/*
 * name for PlaybackController
 */
#define NAME_PLAYCOMMANDISSUED ("PlayCommandIssued")
#define NAME_RESUMECOMMANDISSUED ("ResumeCommandIssued")
#define NAME_PAUSECOMMANDISSUED ("PauseCommandIssued")
#define NAME_NEXTCOMMANDISSUED ("NextCommandIssued")
#define NAME_PREVIOUSCOMMANDISSUED ("PreviousCommandIssued")

/*
 * name for SpeechSynthesizer
 */
#define NAME_SPEAK ("Speak")
#define NAME_SPEECHSTARTED ("SpeechStarted")
#define NAME_SPEECHFINISHED ("SpeechFinished")
#define NAME_SPEECHSTOPPED ("SpeechStopped")

/*
 * name for System
 */
#define NAME_UserInactivityReport ("UserInactivityReport")
#define NAME_ResetUserInactivity ("ResetUserInactivity")
#define NAME_ExceptionEncountered ("ExceptionEncountered")
#define NAME_Exception ("Exception")
#define NAME_SetEndpoint ("SetEndpoint")

/*
 * Name for Notification
 */
#define NAME_IndicatorState ("IndicatorState")
#define NAME_SetNotifications ("SetIndicator")
#define NAME_ClearNotifications ("ClearIndicator")
#define NAME_Notify ("Notify")
#define NAME_RequestIndicator ("RequestIndicator")

/*
 * name for context
 */
#define NAME_PLAYBACKSTATE ("PlaybackState")
#define NAME_ALERTSTATE ("AlertsState")
#define NAME_VOLUMESTATE ("VolumeState")
#define NAME_SPEECHSTATE ("SpeechState")
#define NAME_STOPCAPTURE ("StopCapture")
#define NAME_KEEPRECORDING ("KeepRecording")
#define NAME_CONFIRMWAKEUP ("ConfirmWakeUp")
#define NAME_RECOGNIZERSTATE ("RecognizerState")

#define KEY_NAME ("name")
#define KEY_NAMESPACE ("namespace")

#define KEY_MESSAGEID ("messageId")
#define KEY_DIALOGREQUESTID ("dialogRequestId")

#define KEY_HEADER ("header")
#define KEY_PAYLOAD ("payload")

#define KEY_CONTEXT ("context")
#define KEY_EVENT ("event")

#define KEY_DIRECTIVE ("directive")

/*
 * PAYLOAD  keys
 */

#define PAYLOAD_TOKEN ("token")
#define PAYLOAD_PLAYACTIVE ("playerActivity")
#define PAYLOAD_PROFILE ("profile")

#define PROFILE_CLOSE_TALK ("CLOSE_TALK")
#define PROFILE_NEAR_FIELD ("NEAR_FIELD")
#define PROFILE_FAR_FIELD ("FAR_FIELD")
#define PAYLOAD_FORMAT ("format")
#define FORMAT_AUDIO_L16 ("AUDIO_L16_RATE_16000_CHANNELS_1")
#define PAYLOAD_URL ("url")
#define PAYLOAD_CAPTION ("caption")

#define Initiator_PRESS_HOLD ("PRESS_AND_HOLD")
#define Initiator_TAP ("TAP")
#define Initiator_WAKEWORD ("WAKEWORD")

#define PAYLOAD_OFFSET ("offsetInMilliseconds")
#define PAYLOAD_DURATION ("stutterDurationInMilliseconds")
#define PAYLOAD_CURPLAYBACKSTATE ("currentPlaybackState")
#define PAYLOAD_UNKNOWN_DIRECTIVE ("unparsedDirective")
#define PAYLOAD_ERROR ("error")
#define ERROR_TYPE ("type")
#define ERROR_MESSAGE ("message")
#define PAYLOAD_CODE ("code")
#define PAYLOAD_DESCRIPTION ("description")
#define PAYLOAD_METADATA ("metadata")
#define PAYLOAD_VOLUME ("volume")
#define PAYLOAD_MUTED ("muted")
#define PAYLOAD_MUTE ("mute")
#define PAYLOAD_UserInactivityReport ("inactiveTimeInSeconds")
#define PAYLOAD_ExceptionEncountered ("unparsedDirective")

#define PAYLOAD_TIMEOUT_MS ("timeoutInMilliseconds")

#define PAYLOAD_TYPE ("type")
/*yyyy-mm-ddThh:mm:ss[.mmm]*/
#define PAYLOAD_SCHEDULEDTIME ("scheduledTime")
#define PAYLOAD_ALLALERTS ("allAlerts")
#define PAYLOAD_ACTIVEALERTS ("activeAlerts")

#define PAYLOAD_PLAYBEHAVIOR ("playBehavior")
#define PLAYBEHAVIOR_REPLACE_ALL ("REPLACE_ALL")
#define PLAYBEHAVIOR_ENQUEUE ("ENQUEUE")
#define PLAYBEHAVIOR_REPLACE_ENQUEUED ("REPLACE_ENQUEUED")

#define PAYLOAD_CLEARBEHAVIOR ("clearBehavior")
#define PAYLOAD_CLEAR_ENQUEUED ("CLEAR_ENQUEUED")
#define PAYLOAD_CLEAR_ALL ("CLEAR_ALL")

#define PAYLOAD_AUDIOITEM ("audioItem")
#define PAYLOAD_AUDIOITEM_ID ("audioItemId")
#define PAYLOAD_AUDIOITEM_STREAM ("stream")
#define PAYLOAD_STREAM_URL (PAYLOAD_URL)
#define PAYLOAD_STREAM_FORMAT ("streamFormat")
#define PAYLOAD_STREAM_EXP_TIME ("expiryTime")
#define PAYLOAD_STREAM_PROGRESS ("progressReport")
#define PAYLOAD_PROGRESS_DELAY_MS ("progressReportDelayInMilliseconds")
#define PAYLOAD_PROGRESS_INTERVAL_MS ("progressReportIntervalInMilliseconds")
#define PAYLOAD_PROGRESS_POSITION_MS ("progressReportPositionInMilliseconds")
#define PAYLOAD_STREAM_TOKEN (PAYLOAD_TOKEN)
#define PAYLOAD_STREAM_EXP_TOKEN ("expectedPreviousToken")
#define PAYLOAD_ENDPOINT ("endpoint")
#define PAYLOAD_SETTINGS ("settings")
#define PAYLOAD_firmwareVersion ("firmwareVersion")

#define PAYLOAD_SUCCESS ("success")
#define PAYLOAD_new ("new")
#define PAYLOAD_light ("light")

#define PAYLOAD_Asset ("asset")
#define PAYLOAD_Assets ("assets")
#define PAYLOAD_AssetId ("assetId")
#define PAYLOAD_Asset_url ("url")
#define PAYLOAD_assetPlayOrder ("assetPlayOrder")
#define PAYLOAD_loopCount ("loopCount")
#define PAYLOAD_loopPauseInMilliSeconds ("loopPauseInMilliSeconds")

#define PAYLOAD_VOICEENERGY ("voiceEnergy")
#define PAYLOAD_AMBIENTENERGY ("ambientEnergy")

/*
    Clova Autherization Error Message
*/
#define DOWNSTREAM_CREATION_ERROR ("downstream creation error")
#define ACCESS_TOKEN_GET_FAILED ("access_token get failed")

/*
    Notification API
    isEnabled:
        Indicates there are new or pending notifications that have not been
        communicated to the user. Note: Any indicator that has not been cleared is
        considered enabled.

    isVisualIndicatorPersisted:
        persistVisualIndicator value of the last SetIndicator directive received. If
        persistVisualIndicator was true for the last directive received, upon
        reconnecting, isVisualIndicatorPersisted must be true
*/
#define PAYLOAD_IsEnable ("isEnabled")
#define PAYLOAD_IsVisualIndicatorPersisted ("isVisualIndicatorPersisted")

#define SETTING_KEY ("key")
#define SETTING_VALUE ("value")

#define KEY_LOCALE ("locale")

// "(US), en-GB (UK), de-DE (DE)"
enum {
    LANGUAGE_US,
    LANGUAGE_UK,
    LANGUAGE_DE,
    LANGUAGE_IN,
    LANGUAGE_JP,
    LANGUAGE_UNKNOWN,
};

#define LOCALE_US "en-US"
#define LOCALE_UK "en-GB"
#define LOCALE_DE "de-DE"
#define LOCALE_IN "en-IN"
#define LOCALE_JP "ja-JP"
#define LOCALE_CA "en-CA"
#define LOCALE_IR "en-IR"
#define LOCALE_LIST                                                                                \
    (LOCALE_US ":" LOCALE_UK ":" LOCALE_DE ":" LOCALE_IN ":" LOCALE_JP ":" LOCALE_CA ":" LOCALE_IR)

/* FOR DEVICE DELETE FORM ASR WEB */
#define PAYLOAD_UNAUTHORIZED_REQUEST_EXCEPTION ("UNAUTHORIZED_REQUEST_EXCEPTION")
#define ERROR_INVALID_GRANT ("invalid_grant")
#define ERROR_UNAUTHORIZED_CLIENT ("unauthorized_client")

#ifndef StrEq
#define StrEq(str1, str2)                                                                          \
    ((str1 && str2)                                                                                \
         ? ((strlen((char *)str1) == strlen((char *)str2)) && !strcmp((char *)str1, (char *)str2)) \
         : 0)
#endif

#ifndef StrCaseEq
#define StrCaseEq(str1, str2)                                                                      \
    ((str1 && str2) ? ((strlen((char *)str1) == strlen((char *)str2)) &&                           \
                       !strcasecmp((char *)str1, (char *)str2))                                    \
                    : 0)
#endif

#ifndef StrInclude
#define StrInclude(str1, str2)                                                                     \
    ((str1 && str2) ? (!strncmp((char *)str1, (char *)str2, strlen((char *)str2))) : 0)
#endif

#ifndef StrSubStr
#define StrSubStr(str1, str2) ((str1 && str2) ? (strstr((char *)str1, (char *)str2) != NULL) : 0)
#endif

/*
  * json_cmd format:
  * {
  *     "id": "",
  *     "type": "",
  *     "name": "",
  *     "params": {
  *
  *     }
  * }
*/

#ifndef Val_Str
#define Val_Str(str) ((char *)((str) ? (str) : ("")))
#endif
#ifndef Val_Obj
#define Val_Obj(obj) ((char *)((obj) ? (obj) : ("{}")))
#endif
#ifndef Val_Bool
#define Val_Bool(val) ((val) ? ("true") : ("false"))
#endif

#define KEY_id ("id")
#define KEY_type ("type")
#define KEY_token ("token")
#define KEY_muted ("muted")
#define KEY_volume ("volume")

#define KEY_RequestType ("RequestType")
#define KEY_SpeechState ("SpeechState")
#define KEY_VolumeState ("VolumeState")
#define KEY_PlaybackState ("PlaybackState")
#define KEY_playerActivity ("playerActivity")
#define KEY_offsetInMilliseconds ("offsetInMilliseconds")

#define KEY_params ("params")
#define KEY_language ("language")

#define KEY_offset ("offset")
#define KEY_duratioin ("duratioin")
#define KEY_stream_id ("stream_id")

#define KEY_inactive_ts ("inactive_ts")
#define KEY_err_msg ("err_msg")
#define KEY_error_num ("error_num")
#define KEY_language ("language")

#define Format_Cmd                                                                                 \
    ("{"                                                                                           \
     "\"namespace\":\"%s\","                                                                       \
     "\"name\":\"%s\","                                                                            \
     "\"params\":%s"                                                                               \
     "}")

#define Create_Cmd(buff, namespace, name, params)                                                  \
    do {                                                                                           \
        asprintf(&(buff), Format_Cmd, namespace, name, params);                                    \
    } while (0)

#define Format_ESP_Params                                                                          \
    ("{"                                                                                           \
     "\"voiceEnergy\":\"%u\","                                                                     \
     "\"ambientEnergy\":%u"                                                                        \
     "}")

#define Create_ESP_Params(buff, voiceEnergy, ambientEnergy)                                        \
    do {                                                                                           \
        asprintf(&(buff), Format_ESP_Params, voiceEnergy, ambientEnergy);                          \
    } while (0)

#define JSON_GET_INT_FROM_OBJECT(object, key)                                                      \
    ((object == NULL) ? 0 : ((json_object_object_get((object), (key))) == NULL)                    \
                                ? 0                                                                \
                                : (json_object_get_int(json_object_object_get((object), (key)))))

#define JSON_GET_LONG_FROM_OBJECT(object, key)                                                     \
    ((object == NULL) ? 0                                                                          \
                      : ((json_object_object_get((object), (key))) == NULL)                        \
                            ? 0                                                                    \
                            : (json_object_get_int64(json_object_object_get((object), (key)))))

#define JSON_GET_BOOL_FROM_OBJECT(object, key)                                                     \
    ((object == NULL) ? 0                                                                          \
                      : ((json_object_object_get((object), (key))) == NULL)                        \
                            ? 0                                                                    \
                            : (json_object_get_boolean(json_object_object_get((object), (key)))))

#define JSON_GET_STRING_FROM_OBJECT(object, key)                                                   \
    ((object == NULL)                                                                              \
         ? NULL                                                                                    \
         : ((json_object_object_get((object), (key)) == NULL)                                      \
                ? NULL                                                                             \
                : ((char *)json_object_get_string(json_object_object_get((object), (key))))))

#define JSON_OBJECT_NEW_STRING(str)                                                                \
    ((str) ? json_object_new_string((char *)(str)) : json_object_new_string(""))

// { for local cmd

#define Key_Cmd ("cmd")
#define Key_Cmd_type ("cmd_type")
#define Key_Cmd_params ("params")

#define Val_emplayer ("emplayer")
#define Val_bluetooth ("bluetooth")

#define Key_Event ("event")
#define Key_Event_type ("event_type")
#define Key_Event_params ("params")

// }

#ifdef __cplusplus
extern "C" {
#endif

/*
 * {
 *   "RequestType":%d,
 *   "PlaybackState": {
 *          "token":"%s",
 *          "offsetInMilliseconds":%ld,
 *          "playerActivity":"%s"
 *      },
 *      "VolumeState":{"
 *          "volume":%ld,"
 *          "muted":%s"
 *      },
 *      "SpeechState":{"
 *          "token":"%s",
 *          "offsetInMilliseconds":%ld,
 *          "playerActivity":"%s"
 *      }
 * }
 */
int create_asr_state(char **buff, int request_type, long long duration, long offet,
                     char *play_state, char *repeat_mode, json_object *stream, int vol, int muted,
                     char *speech_token, long offet1, char *play_state1);

// TODO: need to free the string
char *create_alerts_general_cmd(char *cmd_name, char *token, char *type);

char *create_speech_cmd(int cmd_id, char *cmd_name, char *token);

char *create_playback_general_cmd(char *cmd_name, char *device_id);

char *create_audio_general_cmd(char *cmd_name, char *token, long pos);

json_object *json_object_object_dup(json_object *src_js);

#ifdef __cplusplus
}
#endif

#endif
