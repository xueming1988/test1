#ifndef __ALEXA_JSON_COMMON_H__
#define __ALEXA_JSON_COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "json.h"

/*
 * events type
 */
typedef enum {
    E_SPEECHRECOGNIZER,
    E_ALERTS,
    E_AUDIOPLAYER,
    E_PLAYBACKCTROLLER,
    E_SPEAKER,
    E_SPEECHSYNTHESIZER,
    E_SYSTEM,
    E_SETTING,
    E_BLUETOOTH,
    E_EMPLAYER,
    E_GUI,
    E_UNLNOWN,
} e_event_type_t;

/*
Player Activity Description
IDLE    Nothing was playing, no enqueued items.
PLAYING Stream was playing.
BUFFER_UNDERRUN Buffer underrun
FINISHED    Stream was finished playing.
STOPPED Stream was interrupted.
*/

typedef enum {
    E_ALEXA_PLAY_NONE,
    E_ALEXA_PLAY_IDLE,
    E_ALEXA_PLAY_PLAYING,
    E_ALEXA_PLAY_PAUSED,
    E_ALEXA_PLAY_BUFFER_UNDERRUN,
    E_ALEXA_PLAY_FINISHED,
    E_ALEXA_PLAY_STOPPED,
} E_ALEXA_PLAY_T;

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
#define NAMESPACE_NOTIFICATIONS ("Notifications")
#define NAMESPACE_TEMPLATERUNTIME ("TemplateRuntime")
#define NAMESPACE_Bluetooth ("Bluetooth")
#define NAMESPACE_ExternalMediaPlayer ("ExternalMediaPlayer")
#define NAMESPACE_Alexa ("Alexa")
#define NAMESPACE_SIPUSERAGENT ("SipUserAgent")
#define NAMESPACE_SIPCLIENT ("SipClient")
#define NAME_SIPCLIENTSTATE ("SipClientState")
#define NAMESPACE_DONOTDISTURB ("Alexa.DoNotDisturb")

#ifdef AVS_MRM_ENABLE

/*
** AVS MRM name
*/
#define NAMESPACE_MRM_DEVICE_DISCOVERY ("DeviceDiscovery")

#endif

#ifdef AVS_MRM_ENABLE

/*
** AVS MRM name
*/
#define NAMESPACE_MRM_DEVICE_DISCOVERY ("DeviceDiscovery")

#endif

/*weiqiang.huang add for Input Controller (v3.0) -2018-07-30 -- begin*/
#define NAMESPACE_INPUTCONTROLLER ("Alexa.InputController")
#define NAME_SELECTINPUT ("SelectInput")
#define KEY_INPUT ("input")
/*weiqiang.huang add for Input Controller (v3.0) -2018-07-30 -- bnd*/

/*add by weiqiang.huang for MAR-385 Equalizer controller 2018-08-30 -begin*/
#define NAMESPACE_EQUALIZERCONTROLLER ("EqualizerController")
#define NAME_SET_BANDS ("SetBands")
#define NAME_ADJUST_BANDS ("AdjustBands")
#define NAME_RESET_BANDS ("ResetBands")
#define NAME_SET_MODE ("SetMode")
#define NAME_EQUALIZER_CHANGED ("EqualizerChanged")
#define NAME_BANDS ("bands")
#define NAME_MODE ("mode")
#define NAME_NAME ("name")
#define NAME_LEVEL ("level")
#define NAME_LEVELDELTA ("levelDelta")
#define NAME_LEVELIRECTION ("levelDirection")
#define NAME_TREBLE ("TREBLE")
#define NAME_MIDRANGE ("MIDRANGE")
#define NAME_BASS ("BASS")
/*add by weiqiang.huang for MAR-385 Equalizer controller 2018-08-30 -end*/

#define PAYLOAD_wakeword ("wakeword")
#define PAYLOAD_initiator ("initiator")
#define PAYLOAD_wakeWordIndices ("wakeWordIndices")
#define PAYLOAD_startIndexInSamples ("startIndexInSamples")
#define PAYLOAD_endIndexInSamples ("endIndexInSamples")

/*
 * name for SpeechRecognizer
 */
#define NAME_RECOGNIZE ("Recognize")
#define NAME_REPORTECHOSPATIALPERCEPTIONDATA ("ReportEchoSpatialPerceptionData")
#define NAME_RecognizerState ("RecognizerState")
#define NAME_EXPECTSPEECH ("ExpectSpeech")
#define NAME_EXPECTSPEECHTIMEOUT ("ExpectSpeechTimedOut")
#define PAYLOAD_LISTONTIMEOUT ("timeoutInMilliseconds")

#define Val_VOICE_RECORDING ("VOICE_RECORDING")
#define Val_EXPECT_SPEECH_PROMPT ("EXPECT_SPEECH_PROMPT")

#define WAKE_WORD ("ALEXA")

/*
 * name for Comms
 */
#define NAME_SIPUSERAGENTSTATE ("SipUserAgentState")
#define NAME_INVITE ("Invite")
#define NAME_INVITERECEIVED ("InviteReceived")
#define NAME_INBOUNDRINGINGSTARTED ("InboundRingingStarted")
#define NAME_OUTBOUNDLOCALRINGINGSTARTED ("OutboundLocalRingingStarted")
#define NAME_OUTBOUNDPROVIDERRINGINGSTARTED ("OutboundProviderRingingStarted")
#define NAME_CALLACTIVATED ("CallActivated")
#define NAME_CALLSTOPPED ("CallStopped")
#define NAME_CallError ("CallError")
#define NAME_CallCancel ("CallCancel")

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
#define NAME_ALERT_STOPPED ("AlertStopped")

#define NAME_ALERT_FOREGROUND ("AlertEnteredForeground")
#define NAME_ALERT_BACKGROUND ("AlertEnteredBackground")

/*added by weiqiang.huang for AVS alert V1.3 2018-07-30 -begin*/
/*Alert version 1.3*/
#define NAME_DELETE_ALERTS ("DeleteAlerts")
#define NAME_DELETE_ALERTS_SUCCEEDED ("DeleteAlertsSucceeded")
#define NAME_DELETE_ALERTS_FAILED ("DeleteAlertsFailed")
#define NAME_ALERT_SET_VOLUME ("SetVolume")
#define NAME_ALERT_ADJUST_VOLUME ("AdjustVolume")
#define NAME_ALERT_VOLUME_CHANGED ("VolumeChanged")
#define PAYLOAD_TOKENS ("tokens")
/*added by weiqiang.huang for AVS alert V1.3 2018-07-30 -end*/

/*
 * name for AudioPlayer events
 */
#define NAME_PLAY ("Play")
#define NAME_PLAYSTARTED ("PlaybackStarted")
#define NAME_PLAYNEARLYFIN ("PlaybackNearlyFinished")
#define NAME_PLAYREPORTDELAY ("ProgressReportDelayElapsed")
#define NAME_PLAYREPORTINTERVAL ("ProgressReportIntervalElapsed")
#define NAME_PLAYSTUTTERSTARTED ("PlaybackStutterStarted")
#define NAME_PLAYSTUTTERFIN ("PlaybackStutterFinished")
#define NAME_PLAYFINISHED ("PlaybackFinished")
#define NAME_PLAYFAILED ("PlaybackFailed")

#define NAME_STOP ("Stop")
#define NAME_STOPPED ("PlaybackStopped")
#define NAME_PAUSED ("PlaybackPaused")
#define NAME_RESUMED ("PlaybackResumed")

#define NAME_CLEARQUEUE ("ClearQueue")
#define NAME_QUEUECLEARED ("PlaybackQueueCleared")
#define NAME_STREAMMETADATA ("StreamMetadataExtracted")

/*
 * name for PlaybackController
 */
#define NAME_PLAYCOMMANDISSUED ("PlayCommandIssued")
#define NAME_PAUSECOMMANDISSUED ("PauseCommandIssued")
#define NAME_NEXTCOMMANDISSUED ("NextCommandIssued")
#define NAME_PREVIOUSCOMMANDISSUED ("PreviousCommandIssued")

/*
 * name for Speaker
 */
#define NAME_SETVOLUME ("SetVolume")
#define NAME_ADJUSTVOLUME ("AdjustVolume")
#define NAME_VOLUMECHANGED ("VolumeChanged")
#define NAME_SETMUTE ("SetMute")
#define NAME_MUTECHANGED ("MuteChanged")

/*
 * name for SpeechSynthesizer
 */
#define NAME_SPEAK ("Speak")
#define NAME_SPEECHSTARTED ("SpeechStarted")
#define NAME_SPEECHFINISHED ("SpeechFinished")

/*
 * name for System
 */
#define NAME_SYNCHORNIZESTATE ("SynchronizeState")
#define NAME_UserInactivityReport ("UserInactivityReport")
#define NAME_ResetUserInactivity ("ResetUserInactivity")
#define NAME_ExceptionEncountered ("ExceptionEncountered")
#define NAME_Exception ("Exception")
#define NAME_SetEndpoint ("SetEndpoint")
#define NAME_SoftwareInfo ("SoftwareInfo")
#define NAME_ReportSoftwareInfo ("ReportSoftwareInfo")
/*
 * Name for Notification
 */
#define NAME_IndicatorState ("IndicatorState")
#define NAME_SetNotifications ("SetIndicator")
#define NAME_ClearNotifications ("ClearIndicator")

/*
 * name for Settings
 */
#define NAME_SettingUpdated ("SettingsUpdated")

/*
 * name for context
 */
#define NAME_PLAYBACKSTATE ("PlaybackState")
#define NAME_ALERTSTATE ("AlertsState")
#define NAME_VOLUMESTATE ("VolumeState")
#define NAME_SPEECHSTATE ("SpeechState")
#define NAME_STOPCAPTURE ("StopCapture")
#define NAME_RECOGNIZERSTATE ("RecognizerState")

#define KEY_NAME ("name")
#define KEY_NAMESPACE ("namespace")

/*
 * name for  do not disturb
 */
#define NAME_REPORTDONOTDISTURB ("ReportDoNotDisturb")

/* avs mrm */
#define KEY_URI ("uri")

#define KEY_MESSAGEID ("messageId")
#define KEY_DIALOGREQUESTID ("dialogRequestId")

#define KEY_HEADER ("header")
#define KEY_PAYLOAD ("payload")

#define KEY_CONTEXT ("context")
#define KEY_EVENT ("event")

#define KEY_DIRECTIVE ("directive")

// DEFINITION FOR APP
#define ALAMR_ID_APP ("ID")         // alarm1 alarm2...
#define ALARM_TIME_APP ("Time")     // time of alarm
#define ALARM_TYPE_APP ("type")     // type of alert
#define ALAMR_STATUS_APP ("status") // acitve or inavtive
#define ALARM_REPEAT_APP ("repeat") // repeat: yes or no

#define ALARM_TONECOUNT_APP ("tonecount") // count of alarm
#define ALARM_TONEID_APP ("toneid")       // toneid of alarm
#define ALAMR_PREWAKE_APP ("prewake")     // prewake or inavtive
#define ALARM_VOLUME_APP ("volume")       // volume: 0~100
#define ALAMR_ALL_ALERM_APP ("allAlarm")

/*
 * PAYLOAD  keys
 */
#define PAYLOAD_WAKEWORD ("wakeword")

#define PAYLOAD_TOKEN ("token")
#define PAYLOAD_PLAYACTIVE ("playerActivity")
#define PAYLOAD_PROFILE ("profile")
// https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/content/designing-for-the-alexa-voice-service
#define PROFILE_CLOSE_TALK ("CLOSE_TALK")
#define PROFILE_NEAR_FIELD ("NEAR_FIELD")
#define PROFILE_FAR_FIELD ("FAR_FIELD")
#define PAYLOAD_FORMAT ("format")
#define FORMAT_AUDIO_L16 ("AUDIO_L16_RATE_16000_CHANNELS_1")
#define PAYLOAD_URL ("url")
#define PAYLOAD_CAPTION ("caption")
#define PAYLOAD_ENABLED ("enabled")

#define PAYLOAD_startIndexInSamples ("startIndexInSamples")
#define PAYLOAD_endIndexInSamples ("endIndexInSamples")
#define PAYLOAD_wakeWordIndices ("wakeWordIndices")
#define PAYLOAD_initiator ("initiator")

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
#define PAYLOAD_STREAM_OFFSET_MS (PAYLOAD_OFFSET)
#define PAYLOAD_STREAM_EXP_TIME ("expiryTime")
#define PAYLOAD_STREAM_PROGRESS ("progressReport")
#define PAYLOAD_PROGRESS_DELAY_MS ("progressReportDelayInMilliseconds")
#define PAYLOAD_PROGRESS_INTERVAL_MS ("progressReportIntervalInMilliseconds")
#define PAYLOAD_STREAM_TOKEN (PAYLOAD_TOKEN)
#define PAYLOAD_STREAM_EXP_TOKEN ("expectedPreviousToken")
#define PAYLOAD_ENDPOINT ("endpoint")
#define PAYLOAD_SETTINGS ("settings")
#define PAYLOAD_firmwareVersion ("firmwareVersion")

#define PAYLOAD_VisualIndicator ("persistVisualIndicator")
#define PAYLOAD_AudioIndicator ("playAudioIndicator")
#define PAYLOAD_Asset ("asset")
#define PAYLOAD_Assets ("assets")
#define PAYLOAD_AssetId ("assetId")
#define PAYLOAD_Asset_url ("url")
#define PAYLOAD_assetPlayOrder ("assetPlayOrder")
#define PAYLOAD_loopCount ("loopCount")
#define PAYLOAD_loopPauseInMilliSeconds ("loopPauseInMilliSeconds")

#define PAYLOAD_VOICEENERGY ("voiceEnergy")
#define PAYLOAD_AMBIENTENERGY ("ambientEnergy")

/******************* string for focus management *****************************/
#define NAME_ACTIVITYSTATE ("ActivityState")
#define NAMESPACE_AUDIOACTIVEETRACKER ("AudioActivityTracker")
#define NAMESPACE_VISUALACTIVEETRACKER ("VisualActivityTracker")
#define PAYLOAD_DIALOG ("dialog")
#define PAYLOAD_COMMUNICATIONS ("communications")
#define PAYLOAD_ALERT ("alert")
#define PAYLOAD_CONTENT ("content")
#define PAYLOAD_FOCUSED ("focused")
#define KEY_INTERFACE ("interface")
#define KEY_IDLETIMEINMILLISECONDS ("idleTimeInMilliseconds")
/*****************************************************************************/

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
    LANGUAGE_FR,
    LANGUAGE_UNKNOWN,
};

#define LOCALE_US "en-US"
#define LOCALE_UK "en-GB"
#define LOCALE_DE "de-DE"
#define LOCALE_IN "en-IN"
#define LOCALE_JP "ja-JP"
#define LOCALE_CA "en-CA"
#define LOCALE_FR "fr-FR"
#define LOCALE_IT "it-IT"
#define LOCALE_ES "es-ES"
#define LOCALE_MX "mx-ES"
#define LOCALE_CA_FR "fr-CA"
#define LOCALE_PT_BR "pt-BR"

#define LOCALE_LIST                                                                                \
    (LOCALE_US ":" LOCALE_UK ":" LOCALE_DE ":" LOCALE_IN ":" LOCALE_JP ":" LOCALE_CA ":" LOCALE_FR \
               ":" LOCALE_IT ":" LOCALE_ES ":" LOCALE_MX ":" LOCALE_CA_FR ":" LOCALE_PT_BR)

// ERROR INFO
#define MEDIA_ERROR_UNKNOWN ("MEDIA_ERROR_UNKNOWN")
#define MEDIA_ERROR_UNKNOWN_MESSAGE ("An unknown error occurred.")

#define MEDIA_ERROR_INVALID_REQUEST ("MEDIA_ERROR_INVALID_REQUEST")
#define MEDIA_ERROR_INVALID_REQUEST_MESSAGE                                                        \
    ("The server recognized the request as being malformed.")

#define MEDIA_ERROR_SERVICE_UNAVAILABLE ("MEDIA_ERROR_SERVICE_UNAVAILABLE")
#define MEDIA_ERROR_SERVICE_UNAVAILABLE_MESSAGE ("The client was unable to reach the service.")

#define MEDIA_ERROR_INTERNAL_SERVER_ERROR ("MEDIA_ERROR_INTERNAL_SERVER_ERROR")
#define MEDIA_ERROR_INTERNAL_SERVER_ERROR_MESSAGE                                                  \
    ("The server accepted the request, but was unable to process the request as expected.")

#define MEDIA_ERROR_INTERNAL_DEVICE_ERROR ("MEDIA_ERROR_INTERNAL_DEVICE_ERROR")
#define MEDIA_ERROR_INTERNAL_DEVICE_ERROR_MESSAGE ("There was an internal error on the client.")

#define SYSTEM_ERROR_UNEXPECTED ("UNEXPECTED_INFORMATION_RECEIVED")
#define SYSTEM_ERROR_UNEXPECTED_INFO                                                               \
    ("The directive sent to your client was malformed or the payload does not conform to the "     \
     "directive specification.")

#define SYSTEM_ERROR_UNSUPPORTED ("UNSUPPORTED_OPERATION")
#define SYSTEM_ERROR_UNSUPPORTED_INFO                                                              \
    ("The operation specified by the namespace/name in the directive's header are not supported "  \
     "by the client.")

#define SYSTEM_ERROR_INTERNAL_ERROR ("INTERNAL_ERROR")
#define SYSTEM_ERROR_INTERNAL_ERROR_INFO                                                           \
    ("An error occurred while the device was handling the directive and the error does not fall "  \
     "into the specified categories.")

// Code  HTTP Status Code    Description
// INVALID_REQUEST_EXCEPTION 400 The request was malformed.
// UNAUTHORIZED_REQUEST_EXCEPTION    403 The request was not authorized.
// THROTTLING_EXCEPTION  429 Too many requests to the Alexa Voice Service.
// INTERNAL_SERVICE_EXCEPTION    500 Internal service exception.
// N/A   503 The Alexa Voice Service is unavailable.

#define CODE_INVALID_REQUEST_EXCEPTION ("INVALID_REQUEST_EXCEPTION")
#define DESC_INVALID_REQUEST_EXCEPTION ("The request was malformed")

#define CODE_UNAUTHORIZED_REQUEST_EXCEPTION ("UNAUTHORIZED_REQUEST_EXCEPTION")
#define DESC_UNAUTHORIZED_REQUEST_EXCEPTION ("The request was not authorized")

#define CODE_THROTTLING_EXCEPTION ("THROTTLING_EXCEPTION")
#define DESC_THROTTLING_EXCEPTION ("Too many requests to the Alexa Voice Service")

#define CODE_INTERNAL_SERVICE_EXCEPTION ("INTERNAL_SERVICE_EXCEPTION")
#define DESC_INTERNAL_SERVICE_EXCEPTION ("Internal service exception")

#define CODE_NA ("N/A")
#define DESC_NA ("The Alexa Voice Service is unavailable")

#define ALERT_TIMER ("TIMER")
#define ALERT_ALARM ("ALARM")

/* FOR DEVICE DELETE FORM AMAZON WEB */
#define PAYLOAD_UNAUTHORIZED_REQUEST_EXCEPTION ("UNAUTHORIZED_REQUEST_EXCEPTION")
#define ERROR_INVALID_GRANT ("invalid_grant")
#define ERROR_UNAUTHORIZED_CLIENT ("unauthorized_client")
#define ERROR_INVALID_REQUEST ("invalid_request") // add by weiqiang.huang for STAB-275 2018-08-28

//{ for bletooth
#define NAME_BluetoothState ("BluetoothState")

#define PAYLOAD_alexaDevice ("alexaDevice")
#define PAYLOAD_friendlyName ("friendlyName")
#define PAYLOAD_pairedDevices ("pairedDevices")
#define PAYLOAD_activeDevice ("activeDevice")
#define PAYLOAD_uniqueDeviceId ("uniqueDeviceId")
#define PAYLOAD_supportedProfiles ("supportedProfiles")
#define PAYLOAD_name ("name")
#define PAYLOAD_version ("version")
#define PAYLOAD_streaming ("streaming")

#define STREAMING_ACTIVE ("ACTIVE")
#define STREAMING_INACTIVE ("INACTIVE")
#define STREAMING_PAUSED ("PAUSED")

#define NAME_ScanDevices ("ScanDevices")
#define NAME_ScanDevicesUpdated ("ScanDevicesUpdated")

#define PAYLOAD_discoveredDevices ("discoveredDevices")
#define PAYLOAD_truncatedMacAddress ("truncatedMacAddress")
#define PAYLOAD_hasMore ("hasMore")

#define NAME_ScanDevicesFailed ("ScanDevicesFailed")

#define NAME_EnterDiscoverableMode ("EnterDiscoverableMode")
#define PAYLOAD_durationInSeconds ("durationInSeconds")

#define NAME_EnterDiscoverableModeSucceeded ("EnterDiscoverableModeSucceeded")
#define NAME_EnterDiscoverableModeFailed ("EnterDiscoverableModeFailed")
#define NAME_ExitDiscoverableMode ("ExitDiscoverableMode")

#define NAME_PairDevice ("PairDevice")
#define PAYLOAD_device ("device")

#define NAME_PairDeviceSucceeded ("PairDeviceSucceeded")
#define NAME_PairDeviceFailed ("PairDeviceFailed")
#define NAME_UnpairDevice ("UnpairDevice")
#define NAME_UnpairDeviceSucceeded ("UnpairDeviceSucceeded")
#define NAME_UnpairDeviceFailed ("UnpairDeviceFailed")

#define NAME_ConnectByDeviceId ("ConnectByDeviceId")
#define NAME_ConnectByDeviceIdSucceeded ("ConnectByDeviceIdSucceeded")
#define NAME_ConnectByDeviceIdFailed ("ConnectByDeviceIdFailed")
#define PAYLOAD_requester ("requester")

#define NAME_ConnectByProfile ("ConnectByProfile")
#define NAME_ConnectByProfileSucceeded ("ConnectByProfileSucceeded")
#define NAME_ConnectByProfileFailed ("ConnectByProfileFailed")
#define PAYLOAD_profileName ("profileName")

#define NAME_DisconnectDevice ("DisconnectDevice")
#define NAME_DisconnectDeviceSucceeded ("DisconnectDeviceSucceeded")
#define NAME_DisconnectDeviceFailed ("DisconnectDeviceFailed")

#define NAME_Ble_Play ("Play")
#define NAME_Ble_Stop ("Stop")
#define NAME_Ble_Next ("Next")
#define NAME_Ble_Previous ("Previous")
#define NAME_MediaControlPlaySucceeded ("MediaControlPlaySucceeded")
#define NAME_MediaControlPlayFailed ("MediaControlPlayFailed")
#define NAME_MediaControlStopSucceeded ("MediaControlStopSucceeded")
#define NAME_MediaControlStopFailed ("MediaControlStopFailed")
#define NAME_MediaControlNextSucceeded ("MediaControlNextSucceeded")
#define NAME_MediaControlNextFailed ("MediaControlNextFailed")
#define NAME_MediaControlPreviousSucceeded ("MediaControlPreviousSucceeded")
#define NAME_MediaControlPreviousFailed ("MediaControlPreviousFailed")

//}

/*
sports update:
"amzn1.as-ct.v1.Domain:Application:Sports:Legacy#ACRI#ssml#ACRI#IntroductionPrompt1f8f76b5-9966-4256-9feb-1ba4f7d6d7e2"
flash briefing:
"amzn1.as-ct.v1.Domain:Application:DailyBriefing:TTS#ACRI#url#ACRI#DailyBriefingPrompt.55d8ab40-fdb1-4b7f-b400-b68df8185c97:ChannelItem:0:0"
play music: "amzn1.as-ct.v1.Dee-Domain-Music#ACRI#url#ACRI#77a7b180-c6e4-4e8a-8416-d3317c29f102:1"
moives:
"amzn1.as-ct.v1.Domain:Application:CinemaShowTimes:MovieListing#ACRI#ssml#ACRI#MovieListing#IntroPrompt617c6393-3fe2-4ac4-9a6a-03b318c26643"
audiblebook: "amzn1.as-ct.v1.AudibleClientId#ACRI#url#ACRI#b8b777c6-ee24-424f-800f-6cd77e3e0ace:34"
timer/alerts:
"amzn1.as-ct.v1.Domain:Application:Notifications#ACRI#SetTimerPrompt-42466b47-e87f-4c71-a709-09db6f9985f2"
kindlebook:"amzn1.as-ct.v1.EBottsClientId#ACRI#url#ACRI#213fc9cb-59c9-440e-9224-df1725ba841c:42379"
shopping list:
"amzn1.as-ct.v1.Domain:Application:ShoppingV4:List#ACRI#ssml#ACRI#c8565153-2ab6-4ee8-5477-b17d73697dea55ebac93-6de4-4794-a2c7-9264155beac4"
todo list:
"amzn1.as-ct.v1.Domain:Application:Todo:Browse#ACRI#ssml#ACRI#ListIntroductionWithSizeAndPagingPrompt2d2235a9-ba90-4988-891b-aa84a282e301"
*/
// TODO: if the server have change this token type, we need to change this.
#define ALEXA_PLAY_MUSIC ("amzn1.as-ct.v1.Dee-Domain-Music")
#define ALEXA_PLAY_AUDIBLEBOOK ("amzn1.as-ct.v1.AudibleClientId")
#define ALEXA_PLAY_KINDLEBOOK ("amzn1.as-ct.v1.EBottsClientId")
#define ALEXA_PLAY_SPORTS ("amzn1.as-ct.v1.Domain:Application:Sports")
#define ALEXA_PLAY_DAILYBRIEFING ("amzn1.as-ct.v1.Domain:Application:DailyBriefing")
#define ALEXA_PLAY_CINEMASHOWTIME ("amzn1.as-ct.v1.Domain:Application:CinemaShowTimes")
#define ALEXA_PLAY_NOTIFICATIONS ("amzn1.as-ct.v1.Domain:Application:Notifications")
#define ALEXA_PLAY_TODO_BROWSE ("amzn1.as-ct.v1.Domain:Application:Todo")
#define ALEXA_PLAY_SHOPPING_LIST ("amzn1.as-ct.v1.Domain:Application:ShoppingV4")
#define ALEXA_PLAY_MESSAGE ("amzn1.as-ct.v1.Domain:Application:Communications:Messaging")

#define ALEXA_PLAY_SPORTS_PAUSE                                                                    \
    ("amzn1.as-ct.v1.Domain:Application:Sports:Legacy#ACRI#ssml#ACRI#PausePrompt")
#define ALEXA_PLAY_TODO_BROWSE_PAUSE                                                               \
    ("amzn1.as-ct.v1.Domain:Application:Todo:Browse#ACRI#ssml#ACRI#PausePrompt")
#define ALEXA_PLAY_CINEMASHOWTIME_PAUSE                                                            \
    ("amzn1.as-ct.v1.Domain:Application:CinemaShowTimes:MovieListing#ACRI#ssml#ACRI#PausePrompt")
#define ALEXA_PLAY_SHOPPING_LIST_PAUSE                                                             \
    ("amzn1.as-ct.v1.Domain:Application:ShoppingV4:List#ACRI#ssml#ACRI#PausePrompt")

#define ALEXA_PLAY_CINEMASHOWTIME_ENDPAUSE                                                         \
    ("amzn1.as-ct.v1.Domain:Application:CinemaShowTimes:MovieListing#ACRI#ssml#ACRI#"              \
     "ListNavigationEndOfPagePausePrompt")
#define ALEXA_PLAY_TODO_BROWSE_ENDPAUSE                                                            \
    ("amzn1.as-ct.v1.Domain:Application:Todo:Browse#ACRI#ssml#ACRI#"                               \
     "ListNavigationEndOfPagePausePrompt")

#define ALEXA_PLAY_LIST_END ("#ACRI#ssml#ACRI#ListNavigationEndOfPagePausePrompt")
#define ALEXA_PLAY_LIST_PAUSE ("#ACRI#ssml#ACRI#PausePrompt")

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

#define IVALIAD_TOKEN ("DEVICEMONKEYBADTOKEN")
#define BAD_HOST ("DEVICEMONKEYBADHOST")
// This is not a goodd way to judge the invalid info
#define IVALIAD_TOKEN_TILE ("INVALID AUDIO TOKEN")
#define BAD_URL_TILE ("BAD AUDIO URL")
#define IVALIAD_HOST_TILE ("INVALID HOST URL")

// TODO: this is not the good way to workaround the issue
#define AVS_TUNE_RADIO_PREFIX ("opml.radiotime.com/Tune.ashx")

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
#define KEY_enabled ("enabled")

#define Format_Cmd                                                                                 \
    ("{"                                                                                           \
     "\"id\":%d,"                                                                                  \
     "\"namespace\":\"%s\","                                                                       \
     "\"name\":\"%s\","                                                                            \
     "\"params\":%s"                                                                               \
     "}")

#define Create_Cmd(buff, id, namespace, name, params)                                              \
    do {                                                                                           \
        asprintf(&(buff), Format_Cmd, id, namespace, name, params);                                \
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

#define Format_Speeker_Params                                                                      \
    ("{"                                                                                           \
     "\"volume\":\"%d\","                                                                          \
     "\"muted\":%d"                                                                                \
     "}")

#define Create_Speeker_Params(buff, volume, muted)                                                 \
    do {                                                                                           \
        asprintf(&(buff), Format_Speeker_Params, volume, muted);                                   \
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
#define JSON_OBJECT_NEW_BOOLEAN(bool)                                                              \
    ((bool) ? json_object_new_boolean(true) : json_object_new_boolean(false))

// { for local cmd

#define Key_Cmd ("cmd")
#define Key_Cmd_type ("cmd_type")
#define Key_Cmd_params ("params")

#define Val_emplayer ("emplayer")
#define Val_bluetooth ("bluetooth")
#define Val_sip_phone ("sip_phone")

#ifdef AVS_MRM_ENABLE
#define Val_avs_mrm ("AVS_MRM")
#define Val_avs_mrm_forwardDirective ("ForwardAVSDirective")
#define Val_avs_mrm_updateClusterCntx ("updateClusterContext")
#endif

#ifdef AVS_MRM_ENABLE
#define Val_avs_mrm ("AVS_MRM")
#define Val_avs_mrm_forwardDirective ("ForwardAVSDirective")
#define Val_avs_mrm_updateClusterCntx ("updateClusterContext")
#define Val_avs_mrm_clearClusterCntx ("clearClusterContext")
#endif

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
int create_avs_state(char **buff, int request_type, char *play_token, long offet, char *play_state,
                     int vol, int muted, char *speech_token, long offet1, char *play_state1);

// TODO: need to free the string
char *create_speeker_cmd(int cmd_id, char *cmd_name, int volume, int muted);

char *create_alerts_cmd(int cmd_id, char *cmd_name, const char *token);

char *create_system_cmd(int cmd_id, char *cmd_name, long inactive_ts, char *err_msg);

char *create_settings_cmd(int cmd_id, char *cmd_name, char *language);

char *create_speech_cmd(int cmd_id, char *cmd_name, char *token);

char *create_playback_cmd(int cmd_id, char *cmd_name);

char *create_audio_cmd(int cmd_id, char *cmd_name, char *token, long pos, long duration,
                       int err_num);

int directive_check_namespace(char *js_str, char *name_space);

char *create_donotdisturb_cmd(int cmd_id, char *cmd_name, bool enabled);

#ifdef __cplusplus
}
#endif

#endif
