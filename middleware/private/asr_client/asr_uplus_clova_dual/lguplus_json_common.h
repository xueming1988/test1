#ifndef __LGUPLUS_JSON_COMMON__
#define __LGUPLUS_JSON_COMMON__

#define LGUP_HEADER ("header")
#define LGUP_BODY ("body")
#define LGUP_SCHEMA ("schema")
#define LGUP_METHOD ("method")
#define LGUP_TRANSACTION_ID ("transactionId")
#define LGUP_MESSAGE_ID ("messageId")
#define LGUPLUS_RESULTCODE ("resultCode")

#define LGUPLUS_CUR_STATE ("currentState")

#define LGUP_RESULT ("Result")
#define LGUP_SPEECH_SYNTHESIZE ("SpeechSynthesize")
#define LGUP_AUDIO ("Audio")
#define LGUP_ALERT ("Alert")
#define LGUP_VOLUME ("Volume")
#define LGUP_VOICE_RECORDING ("VoiceRecording")
#define LGUP_SETTING ("Setting")
#define LGUP_BUTTON ("Button")

#define LGUP_STOP ("stop")
#define LGUP_DEFAULT_RESULT ("defaultResult")
#define LGUP_NEXT_RECOGNIZE ("nextRecognize")
#define LGUP_STT ("stt")
#define LGUP_TTS ("tts")
#define LGUP_NLU ("nlu")
#define LGUP_SKILL ("skill")
#define LGUP_INTENT ("intent")
#define LGUP_SLOT ("slot")
#define LGUP_META_DELIVERY ("metaDelivery")
#define LGUP_PLAY ("play")
#define LGUP_PLAY_BEHAVIOR ("playBehavior")
#define LGUP_PLAY_SERVICE_NAME ("serviceName")
#define LGUP_REPORT_INTERVAL ("progressReportInterval")
#define LGUP_PLAY_INDEX ("playIndex")
#define LGUP_PLAY_LIST_COUNT ("playListCount")
#define LGUP_PLAYLIST ("playList")
#define LGUP_ITEM ("item")
#define LGUP_META_INFO ("metaInfo")
#define LGUP_ITEMID ("itemId")
#define LGUP_URL ("url")
#define LGUP_TYPE ("type")
#define LGUP_CATEGORY_ID ("categoryId")
#define LGUP_ANSWER_WORD_TYPE ("answerWordType")

#define LGUP_PLAY_STARTED ("playBackStarted")
#define LGUP_PLAY_PROGRESS_REPORT ("progressStatusReport")
#define LGUP_PLAY_FINISHED ("playFinished")
#define LGUP_PLAY_FAILED ("playFailed")
#define LGUP_PLAY_STOP ("playStop")
#define LGUP_PLAY_RESUME ("playResume")
#define LGUP_PLAY_PAUSED ("playPaused")
#define LGUP_PLAY_RESUMED ("playResumed")
#define LGUP_PLAY_CLEAR_QUEUE ("queueClear")
#define LGUP_PLAY_STOP_TYPE ("stopType")
#define LGUP_PLAY_CLEAR_MODE ("clearMode")

#define LGUP_TYPE_STOP ("STOP")
#define LGUP_TYPE_PAUSED ("PAUSED")
#define LGUP_TYPE_CLEAR_ALL ("CLEAR_ALL")
#define LGUP_TYPE_CLEAR_ENQUE ("CLEAR_ENQUE")

#define LGUP_ALERT_ADD ("add")
#define LGUP_ALERT_DELETE ("delete")
#define LGUP_ALERT_UPDATE ("update")
#define LGUP_ALERT_ALARM_LIST ("alarmList")
#define LGUP_ALERT_ALERT_ID ("alertId")
#define LGUP_ALERT_ALARM_TITLE ("alarmTitle")
#define LGUP_ALERT_ACTIVE_YN ("activeYn")
#define LGUP_ALERT_VOLUME ("volume")
#define LGUP_ALERT_SET_DATA ("setDate")
#define LGUP_ALERT_SET_TIME ("setTime")
#define LGUP_ALERT_REPEAT_YN ("repeatYn")
#define LGUP_ALERT_REPEAT ("repeat")
#define LGUP_ALERT_IS_ENTIRE ("isEntire")
#define LGUP_ALERT_RESULT_STATUS ("resultStatus")
#define LGUP_ALERT_STATUS ("status")

#define LGUP_CHANGE_VOLUME ("changeVolume")
#define LGUP_MUTE ("mute")
#define LGUP_VOLUME_M ("volume")
#define LGUP_SET_VOLUME ("setVolume")
#define LGUP_SET_MUTE ("setMute")
#define LGUP_VOLUME_UP ("increaseVolume")
#define LGUP_VOLUME_DOWN ("decreaseVolume")
#define LGUP_VOLUME_VAL ("volumeVal")
#define LGUP_MUTE_STATUS ("muteStatus")
#define LGUP_VOLUME_TYPE ("volumeType")

#define LGUP_RECORDING ("recording")
#define LGUP_TIME ("time")

#define LGUP_SETTING_M ("setting")
#define LGUP_ALL_RESET ("allReset")
#define LGUP_RESET_DATA ("resetData")
#define LGUP_KEY ("key")
#define LGUP_VALUE ("value")

#define LGUP_BUTTON_NEXT ("nextCommand")
#define LGUP_BUTTON_PREV ("previousCommand")
#define LGUP_BUTTON_PAUSE ("pauseCommand")
#define LGUP_BUTTON_PLAY ("playCommand")

#define LGUP_INACTIVE_TIME ("inActiveTime")

/*
IDLE: Standby
PLAY: During playback,
BUF_UNDER: buffer underrun
FINISHED: end of playback
STOP: Stop playback
PAUSE: Pause
*/
#define LGUP_STATE_IDLE ("IDLE")
#define LGUP_STATE_PLAY ("PLAY")
#define LGUP_STATE_STOP ("STOP")
#define LGUP_STATE_PAUSE ("PAUSE")
#define LGUP_STATE_BUF_UNDER ("BUF_UNDER")
#define LGUP_STATE_FINISHED ("FINISHED")

#define LGUP_CLIENTINFO ("clientInfo")
#define LGUP_DEVICETOKEN ("deviceToken")
#define LGUP_ACCESSINFO ("accessInfo")
#define LGUP_OSINFO ("osInfo")
#define LGUP_NETWORKINFO ("networkInfo")
#define LGUP_DEVICEMODEL ("deviceModel")
#define LGUP_CARRIER ("carrier")
#define LGUP_APNAME ("apName")
#define LGUP_DEVVERINFO ("devVerInfo")
#define LGUP_FLOWTYPE ("flowType")
#define LGUP_LANGUAGE ("language")
#define LGUP_LOCATION ("location")
#define LGUP_LOGNITUDE ("longitude")
#define LGUP_LATITUDE ("latitude")
#define LGUP_TEXT ("text")

/* Schema */
#define LGUP_SYSTEM ("System")
#define LGUP_RECOGNIZE ("Recognize")
#define LGUP_HEARTBEAT ("Heartbeat")
#define LGUP_SPEECHSYNTHESIZE ("SpeechSynthesize")

/* Method */
#define LGUP_SYNCHRONIZE ("synchronize")
#define LGUP_WAKEUP ("wakeup")
#define LGUP_PING ("ping")
#define LGUP_SPEECHRECOGNIZE ("speechRecognize")
#define LGUP_TEXTRECOGNIZE ("textRecognize")
#define LGUP_NEXT_RECOGNIZE_EXCEPTION ("nextRecognizeException")
#define LGUP_CONTEXT_EXIT ("contextExit")
#define LGUP_SPEECHSYNTHESIZE_M ("speechSynthesize")
#define LGUP_STATUS ("status")

/* Others */
#define TIME_PATTERN ("%d%02d%02d%02d%02d%02d%03d")
#define TRANSACTIONID_PATTERN ("TR_%s_%s_%s")
#define MESSAGEID_PATTERN ("MSG_%s_%s_%s")
#define VAL_DEVICE_TOKEN ("DEVICE_TOKEN_%s")
#define CID_TOKEN_PATTERN ("LGUPLUS_%s_%s")

typedef struct {
    char *buffer;
    int len;
    int read_len;
    int less_len;
} lguplus_record_buffer_t;

#ifndef JSON_GET_STRING_FROM_OBJECT
#define JSON_GET_STRING_FROM_OBJECT(object, key)                                                   \
    ((object == NULL)                                                                              \
         ? NULL                                                                                    \
         : ((json_object_object_get((object), (key)) == NULL)                                      \
                ? NULL                                                                             \
                : ((char *)json_object_get_string(json_object_object_get((object), (key))))))
#endif

#ifndef JSON_GET_INT_FROM_OBJECT
#define JSON_GET_INT_FROM_OBJECT(object, key)                                                      \
    ((object == NULL) ? 0 : ((json_object_object_get((object), (key))) == NULL)                    \
                                ? 0                                                                \
                                : (json_object_get_int(json_object_object_get((object), (key)))))
#endif

#ifndef StrEq
#define StrEq(str1, str2)                                                                          \
    ((str1 && str2)                                                                                \
         ? ((strlen((char *)str1) == strlen((char *)str2)) && !strcmp((char *)str1, (char *)str2)) \
         : 0)
#endif

#endif
