#ifndef xiaoweiAudioType_h
#define xiaoweiAudioType_h

#include "deviceCommonDef.h"

/**
 * xiaowei interface date structure
 */

/** char_data_len requirement in xiaowei_request() */
#define XIAOWEI_BUF_MAX_LENGTH 6400
#define XIAOWEI_BUF_MIN_LENGTH 64

/** skillID defines */
#define DEF_XIAOWEI_SKILL_ID_UNKNOWN "8dab4796-fa37-4114-ffff-ffffffffffff" /// skill name: 未知
#define DEF_XIAOWEI_SKILL_ID_ANY "8dab4796-fa37-ffff-ffff-ffffffffffff"     /// skill name: 任意

#define DEF_XIAOWEI_SKILL_ID_MUSIC "8dab4796-fa37-4114-0011-7637fa2b0001" /// skill name: 音乐
#define DEF_XIAOWEI_SKILL_ID_FM "8dab4796-fa37-4114-0024-7637fa2b0001"    /// skill name: FM-*
#define DEF_XIAOWEI_SKILL_ID_WEATHER "8dab4796-fa37-4114-0012-7637fa2b0003" /// skill name: 天气服务
#define DEF_XIAOWEI_SKILL_ID_NEWS "8dab4796-fa37-4114-0019-7637fa2b0001" /// skill name: 新闻
#define DEF_XIAOWEI_SKILL_ID_WIKI "8dab4796-fa37-4114-0020-7637fa2b0001" /// skill name: 百科
#define DEF_XIAOWEI_SKILL_ID_HISTORY                                                               \
    "8dab4796-fa37-4114-0027-7637fa2b0001" /// skill name: 历史上的今天
#define DEF_XIAOWEI_SKILL_ID_DATETIME                                                              \
    "8dab4796-fa37-4114-0028-7637fa2b0001" /// skill name: 当前时间
#define DEF_XIAOWEI_SKILL_ID_CALC "8dab4796-fa37-4114-0018-7637fa2b0001" /// skill name: 计算器
#define DEF_XIAOWEI_SKILL_ID_TRANSLATE "8dab4796-fa37-4114-0030-7637fa2b0001" /// skill name: 翻译
#define DEF_XIAOWEI_SKILL_ID_CHAT "8dab4796-fa37-4114-0029-7637fa2b0001"      /// skill name: 闲聊
#define DEF_XIAOWEI_SKILL_ID_IOTCTRL                                                               \
    "8dab4796-fa37-4114-0016-7637fa2b0001" /// skill name: 物联-物联设备控制
#define DEF_XIAOWEI_SKILL_ID_ALARM "8dab4796-fa37-4114-0012-7637fa2b0001" /// skill name: 提醒类
#define DEF_XIAOWEI_SKILL_ID_VOIP "8dab4796-fa37-4114-0001-7637fa2b0002" /// skill name: 视频通话
#define DEF_XIAOWEI_SKILL_ID_VOIP_NO_VEDIO                                                         \
    "8dab4796-fa37-4114-0002-7637fa2b0001" /// skill name: 音频通话
#define DEF_XIAOWEI_SKILL_ID_WECHAT_MSG                                                            \
    "8dab4796-fa37-4114-0002-7637fa2b0002" /// skill name: 通讯-微信消息
#define DEF_XIAOWEI_SKILL_ID_NAVIGATE "8dab4796-fa37-4114-0015-7637fa2b0001" /// skill name: 导航
#define DEF_XIAOWEI_SKILL_ID_VOD "8dab4796-fa37-4114-0026-7637fa2b0001"      /// skill name: 视频
#define DEF_XIAOWEI_SKILL_ID_TRIGGER_ALARM                                                         \
    "8dab4796-fa37-4114-0036-7637fa2b0001" /// skill name: 闹钟触发场景
#define DEF_XIAOWEI_SKILL_ID_KTV "8dab4796-fa37-4114-0040-7637fa2b0001"    /// skill name: K歌
#define DEF_XIAOWEI_SKILL_ID_GLOBAL "8dab4796-fa37-4114-0000-7637fa2b0000" /// skill name: 通用控制
#define DEF_XIAOWEI_SKILL_ID_IOT_MASTER_DEVICE                                                     \
    "8dab4796-fa37-4114-ffff-000000000000" /// skill name:IOT主控
#define DEF_XIAOWEI_SKILL_ID_IOT_CONTROLLER_DEVICE                                                 \
    "8dab4796-fa37-4114-0037-7637fa2b0001" /// skill name:设备被控
#define DEF_XIAOWEI_SKILL_ID_STOCK "8dab4796-fa37-4114-0033-7637fa2b0001" /// skill name:股票

/** FM extend */
#define DEF_XIAOWEI_SKILL_ID_FM_HOT_PRO                                                            \
    "8dab4796-fa37-4114-0024-7637fa2b0002" /// skill name:热门节目
#define DEF_XIAOWEI_SKILL_ID_FM_TRA_PRO                                                            \
    "8dab4796-fa37-4114-0024-7637fa2b0003" /// skill name:传统电台
#define DEF_XIAOWEI_SKILL_ID_FM_NOVEL "8dab4796-fa37-4114-0024-7637fa2b0004" /// skill name:电子书
#define DEF_XIAOWEI_SKILL_ID_FM_STORY "8dab4796-fa37-4114-0024-7637fa2b0005"   /// skill name:故事
#define DEF_XIAOWEI_SKILL_ID_FM_JOKE "8dab4796-fa37-4114-0024-7637fa2b0006"    /// skill name:笑话
#define DEF_XIAOWEI_SKILL_ID_FM_NEWS "8dab4796-fa37-4114-0024-7637fa2b0007"    /// skill name:新闻
#define DEF_XIAOWEI_SKILL_ID_FM_PINGSHU "8dab4796-fa37-4114-0024-7637fa2b0008" /// skill name:评书
#define DEF_XIAOWEI_SKILL_ID_FM_XIANGSHENG                                                         \
    "8dab4796-fa37-4114-0024-7637fa2b0009"                                /// skill name:相声
#define DEF_XIAOWEI_SKILL_OPEN_APP "8dab4796-fa37-4114-0042-7637fa2b0003" /// skill name:打开应用
#define DEF_XIAOWEI_SKILL_ID_WECHAT_SHARE_RESOURCE                                                 \
    "8dab4796-fa37-4114-1000-7637fa2b0001" // SKILL NAME 微信分享

/** @brief Event define in on_request_callback(...) */
typedef enum _xiaowei_event {
    /** Idle. Triggered after a round of requests.*/
    xiaowei_event_on_idle = 0,

    /** Start. Triggered at the beginning of a round of requests
       (xiaowei_request.context.voice_request_begin == true). */
    xiaowei_event_on_request_start = 1,

    /** On speak. Triggered when a valid audio input detected by the cloud for the
     * first time. > Usually the first ASR result(on_recognize event with
     * recognized text length >0) will follow this event. > Once this event
     * triggered, the xiaowei_err_ai_audio_vad_time_out will not be triggered,
     * which threshold = xiaowei_request.context.speak_timeout > Only triggered in
     * voice request.
     */
    xiaowei_event_on_speak = 2,

    /** On silent. Only triggered when the (xiaowei_request.context.request_param
     * & xiaowei_param_local_vad == 0) > This event will triggered when the
     * on_speak event already triggered and there is no new ASR result for a
     * while(xiaowei_request.context.silent_timeout) > Once this event triggered,
     * you need to stop the audio recording and stop calling
     * xiaowei_request(calling will be droped and return errcode). > Only
     * triggered in voice request.
     */
    xiaowei_event_on_silent = 3,

    /** On Recognize. get the ASR realtime result in extend_info(json type).
     *  > Only triggered in voice request.
     */
    xiaowei_event_on_recognize = 4,

    /** On response. Get the final results and resources in this event (even
     * errcode != 0) > XIAOWEI_PARAM_RESPONSE *response = (XIAOWEI_PARAM_RESPONSE
     * *)state_info; > extend_info is an optional information in some case. >
     * Usually only one response, but multi-response is also possible. And after
     * all response returned, on_idle will be triggered.
     */
    xiaowei_event_on_response = 5,

    /** Deprecated. TTS resource pushed by service. */
    xiaowei_event_on_tts = 6,

    /** Temp result, usually with only a TTS resource, such like "xiaowei is
       searching the song for you". */
    xiaowei_event_on_response_intermediate = 7,
} XIAOWEI_EVENT;

/** @brief Device property define in XIAOWEI_PARAM_CONTEXT.request_param
 *  > Bit flags.
 *  @code
 *  XIAOWEI_PARAM_CONTEXT context;
 *  context.request_param = (xiaowei_param_local_vad | xiaowei_param_gps);
 */
typedef enum _xiaowei_param_property {
    /** Use local vad detection. In this mode, xiaowei server will not detect vad,
       in other words, xiaowei_event_on_silent will never be triggered. */
    xiaowei_param_local_vad = 0x0000000000000001,

    /** GPS is available in this device. Once this flag is set and
     * XIAOWEI_GPS_CALLBACK is set by xiaowei_set_gps_callback(...), xiaoweiSDK
     * will try to get the GPS info in this callback function during request. This
     * process will block the request until the XIAOWEI_GPS_CALLBACK returns. So,
     * make sure the callback function is fast enough, usually you should cache
     * GPS info and read cache in this callback function, rather ran call GPS
     * driver interface here.
     */
    xiaowei_param_gps = 0x0000000000000002,

    /** Do not need TTS resource. If you want to use 3-rd party TTS, set this
       flag. */
    xiaowei_param_no_tts = 0x0000000000000004,

    /** Xiaowei official TTS package is embedded in your device, coming soon*/
    xiaowei_param_local_tts = 0x0000000000000008,

    /** Only vad detection in this request, No ASR and NLP
     *  This function is used for voice message sending
     */
    xiaowei_param_only_vad = 0x0000000000000010,
} XIAOWEI_PARAM_PROPERTY;

/** @brief Device availability, some availability may not available in some
 * case, such as in voip calling, the microphone is not for xiaowei. > Report
 * state by xiaowei_report_state(...).
 */
typedef enum _xiaowei_availability {
    /** Default, everything are available */
    xiaowei_availability_default = 0x0000000000000000,

    /** Microphone is not available */
    xiaowei_availability_mic_not_available = 0x0000000000000001,

    /** Speaker is not available */
    xiaowei_availability_speaker_not_available = 0x0000000000000002,

    /** Do not disturb mode. In this mode, xiaowei will not initiate speak with
       users. */
    xiaowei_availability_initiative_speak = 0x0000000000000004,

    /** Remote control(xiaowei APP and xiaowei mini APP) will be disabled. */
    xiaowei_availability_disable_remote_control = 0x0000000000000008,
} XIAOWEI_AVAILABILITY;

/** @brief Resource format define*/
typedef enum _xiaowei_resource_format {
    xiaowei_resource_url = 0,          /// URL resource, your player need to support MP3,
                                       /// AAC, m4a, FLAC and m3u8 type.
    xiaowei_resource_text = 1,         /// Text resource, like a story. You can display
                                       /// it directly or translate it to TTS to play.
    xiaowei_resource_tts_id = 2,       /// Deprecated, TTS resource's ID (real resource
                                       /// will be pushed by PCM packages with opus
                                       /// compress (xiaowei_event_on_tts). */
    xiaowei_resource_tts_url = 3,      /// TTS URL, MP3 chuck, may need advanced player support.
    xiaowei_resource_location = 4,     /// Location resource
    xiaowei_resource_command = 5,      /// Deprecated
    xiaowei_resource_intent = 6,       /// Intent resource, special customization.
    xiaowei_resource_remind = 7,       /// Remind resource, local impl, @ #define XIAOWEI_REMIND_*
    xiaowei_resource_image_share = 10, /// Share image*
    xiaowei_resource_file_share = 11,  /// Share file*
    xiaowei_resource_poi_share = 12,   /// Share poi*
    xiaowei_resource_video_share = 13, /// Share video*
    xiaowei_resource_url_share = 14,   /// Share url*
    xiaowei_resource_local = 89,       /// Local resource (local official TTS package)
    xiaowei_resource_unknown = 99,     /// Unknown
} XIAOWEI_RESOURCE_FORMAT;

/** @brief xiaowei_resource_remind define */
#define XIAOWEI_REMIND_DEFAULT "remind_default"
#define XIAOWEI_REMIND_ALARM "remind_alarm"
#define XIAOWEI_REMIND_VOIP "remind_voip"

/** @brief Resource list action type. You can get resources in response, this
 * type shows how to insert these resources into the existing playlist (current
 * list or history list).
 */
typedef enum _xiaowei_playlist_action {
    xiaowei_playlist_replace_all = 0, /// Replacing playlist. So that the current
                                      /// playing resource should be dropped.
    xiaowei_playlist_enqueue_front =
        1, /// Insert into the front of playlist, keep playing current resource.
    xiaowei_playlist_enqueue_back =
        2, /// Insert into the end of playlist, keep playing current resource.
    xiaowei_playlist_replace_enqueue = 3, /// Keep playing current resource, and
                                          /// replacing detail info in playlist,
                                          /// trigger UI refresh.
    xiaowei_playlist_no_action =
        4, /// Keep playing current resource, refreshing media URL in playlist.
    xiaowei_playlist_remove = 6, /// Removing those resources in playlist, if
                                 /// current playing resource is removed, switch
                                 /// to next resource immediately.
} XIAOWEI_PLAYLIST_ACTION;

/** @brief Play list type. */
typedef enum _xiaowei_playlist_type {
    xiaowei_playlist_type_default = 0, /// Default list (current list)/
    xiaowei_playlist_type_history = 1, /// History list.
} XIAOWEI_PLAYLIST_TYPE;

/** @brief responseData type define */
typedef enum _xiaowei_response_type {
    xiaowei_response_type_unknown = 0,
    xiaowei_response_type_card = 1,               /// card
    xiaowei_response_type_weather = 2,            /// weather: json
    xiaowei_response_type_game = 3,               /// game
    xiaowei_response_type_clock = 4,              /// reminder: json
    xiaowei_response_type_english = 5,            /// english reading/media: json
    xiaowei_response_type_local_skill = 6,        /// local skill: intent and slot info
    xiaowei_response_type_msgbox = 7,             /// message box: json
    xiaowei_response_type_fetch_device_info = 8,  /// fetch device info: json
    xiaowei_response_type_news = 9,               /// news: json
    xiaowei_response_type_wiki = 10,              /// wiki: json
    xiaowei_response_type_mini_app = 11,          /// miniapp: json
    xiaowei_response_type_voip = 12,              /// voip
    xiaowei_response_type_iot_device_list = 13,   /// IOT device list
    xiaowei_response_type_iot_org_request = 14,   /// IOT request text
    xiaowei_response_type_translate = 15,         /// translate: json
    xiaowei_response_type_stock = 16,             /// stock json
    xiaowei_response_type_degnoise = 17,          /// blue-tooth degnoise: json
    xiaowei_response_type_mini_program = 18,      /// voice control of mini program
    xiaowei_response_type_photo = 19,             /// photo: json
    xiaowei_response_type_xm_login = 20,          /// xm login: json
    xiaowei_response_type_xm_qr_code_status = 21, /// xm query qr status: json
    xiaowei_response_type_xm_logout = 22,         /// xm logout: json
    xiaowei_response_type_open_app = 23,          /// open local app: json
    xiaowei_response_type_clock_trig = 24,        /// clock trigger: json
} XIAOWEI_RESPONSE_TYPE;

/** @brief Request type in xiaowei_request(...) */
typedef enum _xiaowei_chat_type {
    /** Voice request. In this mode, you need to call xiaowei_request many times,
     * each time takes some voice data (64 < char_data_len < 6400). Only for the
     * first time, context.voice_request_begin = true, and only for the last time,
     * context.voice_request_end = true (if you use local vad, cloud vad detection
     * no need for the end flag). In this mode, chat_data should be 16bit, 16KHz,
     * single channel, pcm type.
     */
    xiaowei_chat_via_voice = 0,

    /** Text request. In this mode, chat_data is the utf-8 type request text. */
    xiaowei_chat_via_text = 1,

    /** TTS request. In this mode, chat_data is the utf-8 type TTS text. */
    xiaowei_chat_only_tts = 2,

    /** Intent request. Not support at present. */
    xiaowei_chat_via_intent = 3,

    /** Wakeup check request. In this mode, wakeup_type must be
     * xiaowei_wakeup_type_cloud. This is for the device with low performance of
     * embedded wakeup module (low accuracy). The cloud will check your wakeup
     * words. See more detail in XIAOWEI_WAKEUP_FLAG You need to upload all audio
     * data, including wake-up voice data.
     */
    xiaowei_chat_via_wakeup_check = 4,
} XIAOWEI_CHAT_TYPE;

/** Deprecated */
typedef enum _xiaowei_account_type {
    xiaowei_account_null = 0,
    xiaowei_account_qq = 1,
    xiaowei_account_wx = 2,
    xiaowei_account_3rd = 3,
} XIAOWEI_ACCOUNT_TYPE;

/** @brief Wake-up flag. During the wake-up check process, you will recieve
 * multiple responses (xiaowei_event_on_response). Decode the result and check
 * the flag in XIAOWEI_PARAM_RESPONSE.wakeup_flag
 */
typedef enum _xiaowei_wakeup_flag {
    /** This response is not for a wake-up check request. */
    xiaowei_wakeup_flag_no = 0,

    /** Wake-up failed */
    xiaowei_wakeup_flag_fail = 1,

    /** @deprecated use xiaowei_wakeup_flag_suc_continue and
       xiaowei_wakeup_flag_suc_rsp instead */
    /** Wake-up success, and no other words said by user (only said wake-up
       words). */
    xiaowei_wakeup_flag_suc = 2,

    /** Wake-up success, and NLP analysis finish. You can get final response and
     * resource in this time. The user said the wake-up words and continued to say
     * the follow-up requirements without gap can trigger this derectly. Or, this
     * is usually be triggered after multi xiaowei_wakeup_flag_suc_continue event.
     */
    xiaowei_wakeup_flag_suc_rsp = 3,

    /** Wake-up words is detected, and do not known if any other input (user is
     * keep saying, silent event not triggered). This event may trigger
     * multi-times, and after that, there are two possibilities:
     * xiaowei_wakeup_flag_suc or xiaowei_wakeup_flag_suc_rsp
     */
    xiaowei_wakeup_flag_suc_continue = 4,
} XIAOWEI_WAKEUP_FLAG;

/** @brief Deprecated. Voice data format, only pcm support now */
typedef enum _xiaowei_audio_data_format {
    xiaowei_audio_data_pcm = 0,
    xiaowei_audio_data_silk = 1,
    xiaowei_audio_data_opus = 2,
} XIAOWEI_AUDIO_DATA_FORMAT;

/** @brief  Wake-up type. In xiaowei_request(...), you must set this value in
 * context.wakeup_type */
typedef enum _xiaowei_wakeup_type {
    /** Local wake-up. There will be no more processing flow, cloud will
       processing audio data directly. */
    xiaowei_wakeup_type_local = 0,

    /** Deprecated. Use xiaowei_wakeup_type_local_with_text instead.
     */
    xiaowei_wakeup_type_cloud = 1,

    /** Local wake-up with text. Some devices have difficulty splitting wake-up
     * data and request voice data. This mode saved it. In this mode, you need to
     * upload all voice data and set xiaowei_request(...).context.wakeup_words.
     * So that the cloud will help you splitting them and remove the wake-up voice
     * data in your request.
     */
    xiaowei_wakeup_type_local_with_text = 2,

    /** Local wake-up with free mode. In this mode, the cloud will keeping
       analysis the request until you cancel this request. */
    xiaowei_wakeup_type_local_with_free = 3,
} XIAOWEI_WAKEUP_TYPE;

/** @brief Wake-up profile, far or near. Affect on ASR recognize */
typedef enum _xiaowei_wakeup_profile {
    xiaowei_wakeup_profile_far = 0,  /// > 1m
    xiaowei_wakeup_profile_near = 1, /// < 1m
} XIAOWEI_WAKEUP_PROFILE;

/** @brief Command ID define. It corresponds to
 * XIAOWEI_PARAM_RESPONSE.control_id > In some command, there will be
 * control_value
 */
typedef enum _xiaowei_device_cmd {
    /** volume and microphone */
    xiaowei_cmd_vol_add = 1000000,    /// volume add {control_value}, -100 < controlValue < 100
    xiaowei_cmd_vol_set_to = 1000001, /// volume set to {control_value}, 0 < controlValue < 100
    xiaowei_cmd_mute = 1000002,       /// speaker mute
    xiaowei_cmd_unmute = 1000003,     /// speaker unmute
    xiaowei_cmd_close_mic = 1000004,  /// microphone muteplay_state_idle
    xiaowei_cmd_open_mic = 1000005,   /// microphone unmute

    /** play control */
    xiaowei_cmd_play = 1000010,           /// play/resume
    xiaowei_cmd_stop = 1000011,           /// stop(the media can not resume, only can restart)
    xiaowei_cmd_pause = 1000012,          /// pause
    xiaowei_cmd_replay = 1000013,         /// restart current media
    xiaowei_cmd_play_prev = 1000014,      /// play previous
    xiaowei_cmd_play_next = 1000015,      /// play next
    xiaowei_cmd_play_res_id = 1000016,    /// play resource, {control_value} is the resource_id
    xiaowei_cmd_offset_add = 1000017,     /// seek {control_value}, unit ms. positive
                                          /// value is forward and negative value
                                          /// means rewind
    xiaowei_cmd_offset_set = 1000018,     /// seek to {control_value}, unit ms.
    xiaowei_cmd_set_play_mode = 1000019,  /// set play mode to {control_id}. 0:
                                          /// sequential 1: playlist loop 2:
                                          /// random 3: random loop 4: single loop
    xiaowei_cmd_set_play_speed = 1000030, /// set play speed

    /** device control */
    xiaowei_cmd_bright_add = 1000050, /// brightness add {control_value}, -100 < controlValue < 100
    xiaowei_cmd_bright_set = 1000051, /// brightness set to {control_value}, 0 < controlValue < 100
    xiaowei_cmd_report_state = 1000100,       /// report current state ASAP
    xiaowei_cmd_shutdown = 1000101,           /// shutdown
    xiaowei_cmd_reboot = 1000102,             /// reboot
    xiaowei_cmd_bluetooth_mode_on = 1000103,  /// enter Bluetooth mode
    xiaowei_cmd_bluetooth_mode_off = 1000104, /// exit Bluetooth mode
    xiaowei_cmd_duplex_mode_on = 1000105,     /// enter duplex mode
    xiaowei_cmd_duplex_mode_off = 1000106,    /// exit duplex mode
    xiaowei_cmd_upload_log = 1000107,         /// need to upload log

    /** resource */
    xiaowei_cmd_favorite = 1100010,    /// set {control_value}(resource_id) favorite
    xiaowei_cmd_un_favorite = 1100011, /// reset {control_value}(resource_id) favorite

    /** skill related */
    xiaowei_cmd_v2a = 1200001, /// V2A matched, controlValue is json {"match_word":"xxx"}

} XIAOWEI_DEVICE_CMD;

/** Deprecated */
typedef struct _xiaowei_param_account {
    unsigned int type;
    const char *account;
    const char *token;
    const char *appid;
    char *buffer;
    unsigned int buffer_len;
} XIAOWEI_PARAM_ACCOUNT;

/** Skill info */
typedef struct _xiaowei_param_skill {
    const char *name; /// skill name
    const char *id;   /// skill ID (define at the top of this file)
    int type;         /// skill type. 0:internal  1:local  2:3rd
} XIAOWEI_PARAM_SKILL;

/** resource info */
typedef struct _xiaowei_param_resource {
    XIAOWEI_RESOURCE_FORMAT format; /// resource format
    const char *id;                 /// resource ID
    const char *content;            /// resource content. If format is URL, this is the URL,
                                    /// else if format is text, this is the text.
    int play_count;                 /// resource play count, usually TTS resource is 1. -1 means
                                    /// no limit.
    unsigned long long offset;      /// play offset, unit ms.
    const char *extend_buffer;      /// extend info, json type
} XIAOWEI_PARAM_RESOURCE;

/** Resource group */
typedef struct _xiaowei_param_res_group {
    XIAOWEI_PARAM_RESOURCE *resources; /// resources
    unsigned int resources_size;       /// size of resources
} XIAOWEI_PARAM_RES_GROUP;

/** Context. Every xiaowei_request(...) need to take this. Also, you will get
 * this in response (on_request_callback) Usually, a new context should be
 * created before you calling xiaowei_request(...) for the first time in a
 * round of request.
 */
typedef struct _xiaowei_param_context {
    /** Context ID.
     *  If you enter multiple round conversations, you will receive a context by
     * on_request_callback(event == xiaowei_event_on_response). Take out the ID in
     * this context and set it into the next round's context. Otherwise, set this
     * id null.
     */
    const char *id;

    /** Speak timeout, unit ms. If xiaowei_event_on_speak is not triggered within
     * this time, an error will be generated and this round of request will be
     * ended by SDK (xiaowei_event_on_idle will be triggered) Set this 5000 is
     * recommended. In multiple round conversations, we suggest you set this by
     * the last return's context. Only valid in voice request mode.
     */
    unsigned int speak_timeout;

    /** Silent timeout, unit micro seconds. Only valid when (this.request_param &
     * xiaowei_param_local_vad != 0) and xiaowei_event_on_speak has been
     * triggered. xiaowei_event_on_silent will be triggered if no new ASR result
     * refresh within this time. Only valid in voice request mode.
     */
    unsigned int silent_timeout;

    /** Must be true at, and only at the first calling of xiaowei_request(...) in
     * a round of request. If this flag is true, a new round of request will be
     * created by SDK. Set this true when the chat_type is not
     * xiaowei_chat_via_voice
     */
    bool voice_request_begin;

    /** If you use local vad (xiaowei_param_local_vad), set this true at the last
     * calling of xiaowei_request(...). No voice data length limitation at this
     * time because it will be ignore at all. Otherwise, set this false.
     */
    bool voice_request_end;

    /** @See XIAOWEI_WAKEUP_PROFILE */
    XIAOWEI_WAKEUP_PROFILE wakeup_profile;

    /** @See XIAOWEI_WAKEUP_TYPE */
    XIAOWEI_WAKEUP_TYPE wakeup_type;

    /** @See XIAOWEI_WAKEUP_TYPE */
    const char *wakeup_word;

    /** @See XIAOWEI_PARAM_PROPERTY */
    unsigned long long request_param;

    /** Local official TTS package version */
    int local_tts_version;

    /** If you take this in your request, only declared skill will be hit by NLP
     */
    XIAOWEI_PARAM_SKILL skill_info;
} XIAOWEI_PARAM_CONTEXT;

/** @brief Response data at on_request_callback(...),
 *  > XIAOWEI_PARAM_RESPONSE *response = (XIAOWEI_PARAM_RESPONSE *)state_info;
 */
typedef struct _xiaowei_param_response {
    /** Skill hit in this request */
    XIAOWEI_PARAM_SKILL skill_info;

    /** Skill info before this request (current skill) */
    XIAOWEI_PARAM_SKILL last_skill_info;

    /** errocde, see deviceCommonDef.h */
    int error_code;

    /** voice id of the request */
    char voice_id[33];

    /** context for this request */
    XIAOWEI_PARAM_CONTEXT context;

    /** ASR final result */
    const char *request_text;

    /** Json type for response_data @see {XIAOWEI_RESPONSE_TYPE} */
    unsigned int response_type;

    /** Extra json data in some case */
    const char *response_data;

    /** Extra intent data in some case */
    const char *intent_info;

    /** size of resource_groups */
    unsigned int resource_groups_size;

    /** resource group, get resources in this. Notice this is a pointer. Resources
     * will be released when the callback function returns. Holding them by your
     * self.
     */
    XIAOWEI_PARAM_RES_GROUP *resource_groups;

    /** If true, means you can pull more resources by another request to add more
     * resource at the end of playlist. */
    bool has_more_playlist;

    /** If true, means this response has history play list to load */
    bool has_history_playlist;

    /** If true, means in this response, the resources can be resumed after pause
     * or being interrupted. Otherwise you need to drop the resource once being
     * interrupted. */
    bool is_recovery;

    /** If true, means in this response, the resources are notify information.
     * Usually you can reduce the volume of the currently playing resources and
     * play them. */
    bool is_notify;

    /** @see XIAOWEI_WAKEUP_FLAG */
    unsigned int wakeup_flag;

    /** Play list action(operation) */
    XIAOWEI_PLAYLIST_ACTION play_behavior;

    /** Play list type, current list or history list */
    XIAOWEI_PLAYLIST_TYPE resource_list_type;

    /** Answer text */
    const char *response_text;

    /** Control ID, such as PAUSE, PLAY, NEXT..., @see XIAOWEI_DEVICE_CMD */
    unsigned int control_id;

    /** Control value for the control ID */
    const char *control_value;

    /** If true, means you can pull more resources by another request to add more
     * resource at the front of playlist. */
    bool has_more_playlist_up;
} XIAOWEI_PARAM_RESPONSE;

/** @brief
 设备当前的播放状态，用户应该在状态变化之后及时上报，只需要上报大资源，即打断后能够恢复的资源，例如音乐，FM。无需上报小资源，例如TTS播放。这里列举一下常用场景的上报：
 1.
 切歌：报2次，首先第一首歌abort、然后第二首歌start(如果可以的话，先报preload，再start)。
 2. 播放过程中唤醒了开始语音请求(这时候正常音乐是要暂停的)：上报paused
 3.
 操作2点了一首新歌，并且要准备播放了：先报刚才那首暂停的歌abort，再报新歌start
 4.
 操作2问了一下天气或者闲聊：首先无需任何上报，音箱播放天气或者闲聊的TTS，对话结束之后正常逻辑应该恢复播放音乐，然后这时候上报resume。
 5. 该播放的东西播完了，无事可做：上报idle
 6. 一首歌自然播放完毕，开始播放下一首：上报第一首stop，再上报第二首start
 7. 所有的URL资源，如果可以的话，start之前先报preload
 8.
 播放列表资源全部播放完毕，退出播放，上报finished，注意携带skill_id。此时如果自动去播放别的了，就该报什么报什么，如果什么也不播放了，就上报idle。

 The current playing state of the device. The device should report ASAP while
 the state changes. Only resume-able resources(resources that can be recovered
 after interruption) need to be reported, such as music and FM. No need to
 report nonrecoverable resources, such as TTS playback. Here are some common
 sample to report:
 1. switch song: report twice. Firstly, report the replaced song
 play_state_abort. Then report the new song play_state_start(It's better if you
 can report preload first).
 2. Interrupted by wakeup and voice request(the playing need to pause
 automatically): report play_state_paused.
 3. If 2 asked for playing a new song:Report this paused song
 play_state_abort，then report new song play_state_start(It's better if you can
 report preload first).
 4. If 2 asked for the weather or chat：Firstly, no need for any report, just
 playing the weather or chat TTS, Then the previous song resume automatically
 and report it play_state_resume.
 5. All resource play finish, nothing todo, report play_state_idle.
 6. A song play finish and play next song automatically: Report the first one
 play_state_stop, then report the next one play_state_start(It's better if you
 can report preload first).
 7. All URL resource, if possible, report preload before start.
 8. Switch to a new skill and play list refreshed, report the previous
 play_state_abort and play_state_finished, Then report new one. Pay attention to
 report the correct skill ID.
 */
typedef enum _xiaowei_res_player_state {
    play_state_preload = 0,  /// prepare or loading the resource, in this state,
                             /// play offset should be 0
    play_state_start = 1,    /// start playing, in this state, play offset should be 0
    play_state_paused = 2,   /// paused, can continue playing
    play_state_stopped = 3,  /// stopped, usually for the resource playing
                             /// finish(automatically), if manually, use
                             /// play_state_abort
    play_state_finished = 4, /// all resource played, nothing to play
    play_state_idle = 5,     /// device idle, not at any skill
    play_state_resume = 6,   /// resume
    play_state_abort = 11,   /// abort, switch songs manually
    play_state_err = 12,     /// play error
} XIAOWEI_PLAY_STATE;

/** @brief resource type, different type has different content */
typedef enum _xiaowei_state_info_res_content_Type {
    report_res_type_url = 0,     /// content is URL
    report_res_type_text = 1,    /// content is text
    report_res_type_tts_id = 2,  /// content is TTS resource ID, deprecated
    report_res_type_tts_url = 3, /// content is TTS URL, do not report state for TTS playing
} XIAOWEI_REPORT_RES_TYPE;

/** @brief play mode */
typedef enum _xiaowei_res_play_mode {
    play_mode_sequential_no_loop = 0, /// sequential, finish playing while all resources are played
    play_mode_sequential_loop_playlist = 1, /// sequential loop
    play_mode_shuffle_no_loop =
        2, /// shuffle playlist, finish playing while all resources are played
    play_mode_shuffle_loop_playlist = 3, /// shuffle playlist, loopy
    play_mode_loop_single = 4,           /// single loop
} XIAOWEI_PLAY_MODE;

/** @brief struct for status report */
typedef struct _xiaowei_param_state {
    XIAOWEI_PARAM_SKILL skill_info;        /// current skill info, very important
    XIAOWEI_PLAY_STATE play_state;         /// @see XIAOWEI_PLAY_STATE
    const char *play_id;                   /// current playing or pausing resourceID
    const char *play_content;              /// current resource content
    unsigned long long play_offset;        /// current playing offset, unit ms
    XIAOWEI_PLAY_MODE play_mode;           /// current play mode
    XIAOWEI_RESOURCE_FORMAT resource_type; /// resource type

    /** if music, the flowing infos are recommended */
    const char *resource_name; /// e.g. music name
    const char *performer;     /// e.g. singer
    const char *collection;    /// e.g album
    const char *cover_url;

    int volume;     /// 0~100
    int brightness; /// 0~100
    float play_speed;
} XIAOWEI_PARAM_STATE;

/**
 * @brief scene type for xiaowei_set_device_scene();
 */
typedef struct enum_xiaowei_scene {
    int type;
    const char *info;
} XIAOWEI_SCENE;

/**
 * @brief GPS info for XIAOWEI_GPS_CALLBACK
 */
typedef struct _xiaowei_gps_info {
    bool is_gps_available;       /// set false is GPS info is not available
    double longitude;            /// positive value: East, negative value: West
    double latitude;             /// positive value: North, negative value: South
    double altitude;             /// unit: m
    unsigned long long utc_time; /// unix time stamp, ms
    double speed;                /// m/s
} XIAOWEI_GPS_INFO;

/**
 * @brief User Log reoport for xiaowei_user_report(..)
 */
typedef struct _xiaowei_param_log {
    const char *voice_id;             /// voiceID for this report, optional
    const char *log_data;             /// log data, required
    const char *event;                /// user defined event
    unsigned long long time_stamp_ms; /// unix time stamp, ms
} XIAOWEI_PARAM_LOG_REPORT;

/**
 * @brief words type for xiaowei_set_wordslist();
 */
typedef enum enum_xiaowei_words_type {
    /**
     * V2A list, words_list is the words array. eg. words_list[0] = "确认",
     * words_list[1] = "取消"
     */
    xiaowei_words_command = 0,
    /**
     * photo file name for local photo skill
     */
    xiaowei_photo_files = 4,
} XIAOWEI_WORDS_TYPE;

/**
 * @brief words type for xiaowei_upload_data();
 */
typedef enum enum_xiaowei_upload_data_type {
    /**
     * local app list. type is:
     * {"appName":"","appNickName":["name1","name2"],"appId":"app_id"}
     */
    xiaowei_upload_app_list = 0,
    /**
     * contact list, words_list should be a json for each contact, each json
     stands for a contact,type is:
     * {
         "id": "your unique ID",
         "name": "王缘",
         "remark": "哥哥",
         "info": "something"
        }
     */
    xiaowei_uplaod_contact = 1,
} XIAOWEI_UPLOAD_DATA_TYPE;

/**
 * Callback for xiaowei_request_cmd(...)
 * @param voice_id The voiceID on the request.
 * @param json response in json type
 */
typedef void (*XIAOWEI_ON_REQUEST_CMD_CALLBACK)(const char *voice_id, int xiaowei_err_code,
                                                const char *json);

/**
 * @brief Xiaowei request base callback
 */
typedef struct _xiaowei_callback {
    /**
     * Callback for voice/text request, or event pushed by server(such as pick
     * song by miniapp).
     * @param voice_id The voiceID on the request.
     * @param event {@see XIAOWEI_EVENT}
     * @param state_info auto response = reinterpret_cast<const
     * XIAOWEI_PARAM_RESPONSE*>(state_info);
     * @param extend_info Additional information, skill related.
     */
    bool (*on_request_callback)(const char *voice_id, XIAOWEI_EVENT event, const char *state_info,
                                const char *extend_info, unsigned int extend_info_len);

    /**
     * Network delay for the request.
     */
    bool (*on_net_delay_callback)(const char *voice_id, unsigned long long time);
} XIAOWEI_CALLBACK;

/**
 * @brief XiaoweiDevice Notify
 */
typedef struct _xiaowei_notify {
    /**
     * Device should reboot immediately.
     */
    void (*on_device_reboot)(void);

    /**
     * XiaoWei service stop, will be called after SDK uninit finish.
     */
    void (*on_service_exit)(void);

} XIAOWEI_NOTIFY;
typedef void (*XIAOWEI_CLOUD_TICKET_CALLBACK)(void *context, const char *ticket, int len,
                                              int expire_seconds);

/**
 * @brief GPS call back. Will be called at the begin of a round of request while
 * XIAOWEI_PARAM_PROPERTY.xiaowei_param_gps is enabled. This is a synchronous
 * call, so please make sure it will never block and faster enough. It is
 * recommended that the user set a buffer to store GPS information
 * @param gpsInfo If GPS is not available, set is_gps_available false.
 */
typedef void (*XIAOWEI_GPS_CALLBACK)(XIAOWEI_GPS_INFO *gpsInfo);

/**
 * @brief upload data result for int xiaowei_upload_data(...)
 */
typedef void (*XIAOWEI_UPLOAD_DATA_CALLBACK)(const char *voice_id, int result);

#endif /* xiaoweiAudioType_h */
