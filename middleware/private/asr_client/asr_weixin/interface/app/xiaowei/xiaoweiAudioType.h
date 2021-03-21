//
//  xiaoweiAudioType.h
//  iLinkSDK
//
//  Created by cppwang(王缘) on 2019/4/14.
//  Copyright © 2019年 Tencent. All rights reserved.
//

#ifndef xiaoweiAudioType_h
#define xiaoweiAudioType_h

#include "deviceCommonDef.h"
typedef unsigned char bool;
enum { false = 0, true = 1 };

CXX_EXTERN_BEGIN_DEVICE

/**
 *
 * 小微通道层接口
 *
 */

/// XIAOWEI_request单次音频数据最大/小长度
#define XIAOWEI_BUF_MAX_LENGTH 6400
#define XIAOWEI_BUF_MIN_LENGTH 64

//技能定义 skillID
#define DEF_XIAOWEI_SKILL_ID_UNKNOWN "8dab4796-fa37-4114-ffff-ffffffffffff"

#define DEF_XIAOWEI_SKILL_ID_MUSIC "8dab4796-fa37-4114-0011-7637fa2b0001" // skill name: 音乐
#define DEF_XIAOWEI_SKILL_ID_FM "8dab4796-fa37-4114-0024-7637fa2b0001"    // skill name:
// FM-笑话/FM-电台/FM-小说/FM-相声/FM-评书/FM-故事/FM-杂烩
#define DEF_XIAOWEI_SKILL_ID_WEATHER "8dab4796-fa37-4114-0012-7637fa2b0003" // skill name: 天气服务
#define DEF_XIAOWEI_SKILL_ID_NEWS "8dab4796-fa37-4114-0019-7637fa2b0001"    // skill name: 新闻
#define DEF_XIAOWEI_SKILL_ID_WIKI "8dab4796-fa37-4114-0020-7637fa2b0001"    // skill name: 百科
#define DEF_XIAOWEI_SKILL_ID_HISTORY                                                               \
    "8dab4796-fa37-4114-0027-7637fa2b0001" /// skill name: 历史上的今天
#define DEF_XIAOWEI_SKILL_ID_DATETIME "8dab4796-fa37-4114-0028-7637fa2b0001" // skill name: 当前时间
#define DEF_XIAOWEI_SKILL_ID_CALC "8dab4796-fa37-4114-0018-7637fa2b0001" // skill name: 计算器
#define DEF_XIAOWEI_SKILL_ID_TRANSLATE "8dab4796-fa37-4114-0030-7637fa2b0001" // skill name: 翻译
#define DEF_XIAOWEI_SKILL_ID_CHAT "8dab4796-fa37-4114-0029-7637fa2b0001"      // skill name: 闲聊
#define DEF_XIAOWEI_SKILL_ID_IOTCTRL                                                               \
    "8dab4796-fa37-4114-0016-7637fa2b0001" /// skill name: 物联-物联设备控制
#define DEF_XIAOWEI_SKILL_ID_ALARM "8dab4796-fa37-4114-0012-7637fa2b0001" // skill name: 提醒类
#define DEF_XIAOWEI_SKILL_ID_VOIP "8dab4796-fa37-4114-0001-7637fa2b0001" // skill name: 视频通话
#define DEF_XIAOWEI_SKILL_ID_VOIP_NO_VEDIO                                                         \
    "8dab4796-fa37-4114-0002-7637fa2b0001" /// skill name: 音频通话
#define DEF_XIAOWEI_SKILL_ID_WECHAT_MSG                                                            \
    "8dab4796-fa37-4114-0002-7637fa2b0002" /// skill name: 通讯-微信消息
#define DEF_XIAOWEI_SKILL_ID_NAVIGATE "8dab4796-fa37-4114-0015-7637fa2b0001" // skill name: 导航
#define DEF_XIAOWEI_SKILL_ID_VOD "8dab4796-fa37-4114-0026-7637fa2b0001"      // skill name: 视频
#define DEF_XIAOWEI_SKILL_ID_TRIGGER_ALARM                                                         \
    "8dab4796-fa37-4114-0036-7637fa2b0001" /// skill name: 闹钟触发场景
#define DEF_XIAOWEI_SKILL_ID_KTV "8dab4796-fa37-4114-0040-7637fa2b0001"    // skill name: K歌
#define DEF_XIAOWEI_SKILL_ID_GLOBAL "8dab4796-fa37-4114-0000-7637fa2b0000" // skill name: 通用控制
#define DEF_XIAOWEI_SKILL_ID_IOT_MASTER_DEVICE "8dab4796-fa37-4114-ffff-000000000000" // IOT主控
#define DEF_XIAOWEI_SKILL_ID_IOT_CONTROLLER_DEVICE "8dab4796-fa37-4114-0037-7637fa2b0001" //设备被控
#define DEF_XIAOWEI_SKILL_ID_STOCK "8dab4796-fa37-4114-0033-7637fa2b0001" /// skill name:股票

// FM扩展类
#define DEF_XIAOWEI_SKILL_ID_FM_HOT_PRO "8dab4796-fa37-4114-0024-7637fa2b0002"    // 热门节目
#define DEF_XIAOWEI_SKILL_ID_FM_TRA_PRO "8dab4796-fa37-4114-0024-7637fa2b0003"    // 传统电台
#define DEF_XIAOWEI_SKILL_ID_FM_NOVEL "8dab4796-fa37-4114-0024-7637fa2b0004"      // 电子书
#define DEF_XIAOWEI_SKILL_ID_FM_STORY "8dab4796-fa37-4114-0024-7637fa2b0005"      // 故事
#define DEF_XIAOWEI_SKILL_ID_FM_JOKE "8dab4796-fa37-4114-0024-7637fa2b0006"       // 笑话
#define DEF_XIAOWEI_SKILL_ID_FM_NEWS "8dab4796-fa37-4114-0024-7637fa2b0007"       // 新闻
#define DEF_XIAOWEI_SKILL_ID_FM_PINGSHU "8dab4796-fa37-4114-0024-7637fa2b0008"    // 评书
#define DEF_XIAOWEI_SKILL_ID_FM_XIANGSHENG "8dab4796-fa37-4114-0024-7637fa2b0009" // 相声
#define DEF_XIAOWEI_SKILL_ID_WECHAT_SHARE_RESOURCE                                                 \
    "8dab4796-fa37-4114-1000-7637fa2b0001" // SKILL NAME 微信分享

// 语音请求回调on_request_callback接口相关事件定义
typedef enum _xiaowei_event {
    xiaowei_event_on_idle = 0,          // 空闲
    xiaowei_event_on_request_start = 1, // 请求开始
    xiaowei_event_on_speak = 2,         // 检测到说话
    xiaowei_event_on_silent = 3,    // 检测到静音(only@device has not xiaowei_device_local_vad)
    xiaowei_event_on_recognize = 4, // 识别文本实时返回
    xiaowei_event_on_response = 5,  // 请求收到响应
    xiaowei_event_on_tts = 6,       // 小微后台推送的TTS信息
    xiaowei_event_on_response_intermediate = 7, // 中间结果响应
} XIAOWEI_EVENT;

// 硬件设备支持属性定义
typedef enum _xiaowei_param_property {
    xiaowei_param_local_vad = 0x0000000000000001, // 使用本地静音检测

    xiaowei_param_gps = 0x0000000000000002,
    xiaowei_param_no_tts = 0x0000000000000004, // 无需云端下发TTS，使用纯本地TTS
    xiaowei_param_local_tts = 0x0000000000000008,

    xiaowei_param_only_vad = 0x0000000000000010, // 只做后台静音检测
} XIAOWEI_PARAM_PROPERTY;

//设备状态位
typedef enum _xiaowei_availability {
    xiaowei_availability_default = 0x0000000000000000, // default，所有功能都正常
    xiaowei_availability_mic_not_available = 0x0000000000000001,

    xiaowei_availability_speaker_not_available = 0x0000000000000002,

    xiaowei_availability_initiative_speak = 0x0000000000000004,

    xiaowei_availability_disable_remote_control = 0x0000000000000008,
} XIAOWEI_AVAILABILITY;

// 资源格式定义
typedef enum _xiaowei_resource_format {
    xiaowei_resource_url = 0,      // url资源
    xiaowei_resource_text = 1,     // 纯文本(无TTS)
    xiaowei_resource_tts_id = 2,   // TTS播放(推送PCM包)
    xiaowei_resource_tts_url = 3,  // TTS URL
    xiaowei_resource_location = 4, // 位置
    xiaowei_resource_command = 5,  // 指令类型，废弃
    xiaowei_resource_intent = 6,   // 语义类型
    xiaowei_resource_remind = 7,   // 提醒类资源
    xiaowei_resource_local = 89,   // 本地资源
    xiaowei_resource_unknown = 99, // 未知类型
} XIAOWEI_RESOURCE_FORMAT;

// 资源播放类型操作定义
typedef enum _xiaowei_playlist_action {
    xiaowei_playlist_replace_all = 0,     // 中断当前播放，替换列表
    xiaowei_playlist_enqueue_front = 1,   // 拼接到列表队头
    xiaowei_playlist_enqueue_back = 2,    // 拼接到列表队尾
    xiaowei_playlist_replace_enqueue = 3, // 不中断当前播放的资源，替换列表的详情
    xiaowei_playlist_no_action = 4, // 不中断播放，更新列表中某些播放资源的url和quality字段信息
    xiaowei_playlist_remove = 6, // 从播放列表中移除这些资源
} XIAOWEI_PLAYLIST_ACTION;

// 资源播放类型操作定义
typedef enum _xiaowei_playlist_type {
    xiaowei_playlist_type_default = 0, // 默认的列表，当前列表。
    xiaowei_playlist_type_history = 1, // 这个场景的历史列表
} XIAOWEI_PLAYLIST_TYPE;

/** @brief responseData type define */
typedef enum _xiaowei_response_type {
    xiaowei_response_type_unknow = 0,
    xiaowei_response_type_card = 1,              /// card
    xiaowei_response_type_weather = 2,           /// weather: json
    xiaowei_response_type_game = 3,              /// game
    xiaowei_response_type_clock = 4,             /// reminder: json
    xiaowei_response_type_english = 5,           /// english reading/media: json
    xiaowei_response_type_local_skill = 6,       /// local skill: intent and slot info
    xiaowei_response_type_msgbox = 7,            /// message box: json
    xiaowei_response_type_fetch_device_info = 8, /// fetch device info: json
    xiaowei_response_type_news = 9,              /// news: json
    xiaowei_response_type_wiki = 10,             /// wiki: json
    xiaowei_response_type_mini_app = 11,         /// miniapp: json
    xiaowei_response_type_voip = 12,             /// voip
    xiaowei_response_type_iot_device_list = 13,  /// IOT device list
    xiaowei_response_type_iot_org_request = 14,  /// IOT request text
    xiaowei_response_type_tanslate = 15,         /// translate: json
    // 16 to 30 are reserved
} XIAOWEI_RESPONSE_TYPE;

// 语音请求类型
typedef enum _xiaowei_chat_type {
    xiaowei_chat_via_voice = 0,        // 语音请求
    xiaowei_chat_via_text = 1,         // 文本请求
    xiaowei_chat_only_tts = 2,         // tts请求
    xiaowei_chat_via_intent = 3,       // 意图请求 不再支持意图请求
    xiaowei_chat_via_wakeup_check = 4, // 唤醒校验请求
} XIAOWEI_CHAT_TYPE;

// 使用自有 App 绑定小微设备的账号类型
typedef enum _xiaowei_account_type {
    xiaowei_account_null = 0, // 默认值，使用小微 App 时使用
    xiaowei_account_qq = 1,   // QQ 账户登录
    xiaowei_account_wx = 2,   // 微信账户绑定登录
    xiaowei_account_3rd = 3,  // 第三方账户云接口绑定
} XIAOWEI_ACCOUNT_TYPE;

// 云端校验唤醒返回的标识位
typedef enum _xiaowei_wakeup_flag {
    xiaowei_wakeup_flag_no = 0,   // 不是云端校验唤醒的结果
    xiaowei_wakeup_flag_fail = 1, // 唤醒校验失败
    xiaowei_wakeup_flag_suc = 2,

    xiaowei_wakeup_flag_suc_rsp = 3, // 成功唤醒并且收到了最终响应，废弃，最终响应直接展示。
    xiaowei_wakeup_flag_suc_continue = 4, // 成功唤醒并且还需要继续传声音，还不知道会不会连续说话
} XIAOWEI_WAKEUP_FLAG;

// 音频数据编码格式
typedef enum _xiaowei_audio_data_format {
    xiaowei_audio_data_pcm = 0,
    xiaowei_audio_data_silk = 1,
    xiaowei_audio_data_opus = 2,
} XIAOWEI_AUDIO_DATA_FORMAT;

// 唤醒类型
typedef enum _xiaowei_wakeup_type {
    xiaowei_wakeup_type_local = 0,           // 本地唤醒
    xiaowei_wakeup_type_cloud = 1,           // 云端校验唤醒
    xiaowei_wakeup_type_local_with_text = 2, // 本地唤醒带唤醒词文本
    xiaowei_wakeup_type_local_with_free = 3, // 本地免唤醒
} XIAOWEI_WAKEUP_TYPE;

// 唤醒场景定义
typedef enum _xiaowei_wakeup_profile {
    xiaowei_wakeup_profile_far = 0,  // 远场
    xiaowei_wakeup_profile_near = 1, // 近场，例如遥控器
} XIAOWEI_WAKEUP_PROFILE;

/** @brief Command ID define. It corresponds to XIAOWEI_PARAM_RESPONSE.control_id
 *  > In some command, there will be control_value
 */
//控制指令
typedef enum _xiaowei_device_cmd {
    //声音相关
    xiaowei_cmd_vol_add = 1000000, //音量增加，具体值为controlValue，单位为百分数，-100~+100
    xiaowei_cmd_vol_set_to = 1000001, //设置音量到某个位置
    xiaowei_cmd_mute = 1000002,       //音箱静音
    xiaowei_cmd_unmute = 1000003,     //取消音箱静音
    xiaowei_cmd_close_mic = 1000004,  //麦克风关闭
    xiaowei_cmd_open_mic = 1000005,   //麦克风打开

    //播放控制相关
    xiaowei_cmd_play = 1000010, //播放/恢复播放
    xiaowei_cmd_stop = 1000011, //停止(停止播放之后就无法再恢复播放了，再次播放就从头开始播)
    xiaowei_cmd_pause = 1000012,       //暂停
    xiaowei_cmd_replay = 1000013,      //从头播放该资源
    xiaowei_cmd_play_prev = 1000014,   //上一首
    xiaowei_cmd_play_next = 1000015,   //下一首
    xiaowei_cmd_play_res_id = 1000016, //播放某个id的资源，id在controlValue里
    xiaowei_cmd_offset_add = 1000017,  /// seek {control_value}, unit ms. positive value is forward
                                       /// and negative value means rewind
    xiaowei_cmd_offset_set = 1000018,  //跳转到某一播放时刻
    xiaowei_cmd_set_play_mode = 1000019, /// set play mode to {control_id}. 0: sequential 1:
                                         /// playlist loop 2: random 3: single loop
                                         // 4:单曲循环

    // notify
    xiaowei_cmd_bright_add = 1000050, /// brightness add {control_value}, -100 < controlValue < 100
    xiaowei_cmd_bright_set = 1000051, /// brightness set to {control_value}, 0 < controlValue < 100
    xiaowei_cmd_report_state = 1000100, //后台需要设备尽快上报当前的播放状态，
    xiaowei_cmd_shutdown = 1000101,     //关机
    xiaowei_cmd_reboot = 1000102,       //重启
    xiaowei_cmd_bluetooth_mode_on = 1000103,  /// enter Bluetooth mode
    xiaowei_cmd_bluetooth_mode_off = 1000104, /// exit Bluetooth mode
    xiaowei_cmd_duplex_mode_on = 1000105,     /// enter duplex mode
    xiaowei_cmd_duplex_mode_off = 1000106,    /// exit duplex mode
    xiaowei_cmd_upload_log = 1000107,         /// need to upload log
    // favorite
    xiaowei_cmd_favorite = 1100010,
    xiaowei_cmd_un_favorite = 1100011,
    /** skill related */
    xiaowei_cmd_v2a = 1200001, /// V2A matched, controlValue is json {"match_word":"xxx"}

} XIAOWEI_DEVICE_CMD;

//  使用自有 App 绑定小微设备的账号相关信息
typedef struct _xiaowei_param_account {
    unsigned int type; // 请参考XIAOWEI_ACCOUNT_TYPE
    const char *account;
    const char *token; // 账户token，如果type是QQ/WX登录表示accessToken，其他类型传空即可
    const char *appid; // 账户名和token对应的appid，在QQ/WX登录时使用，其他类型传空即可
    char *buffer;            // 账号扩展参数
    unsigned int buffer_len; // 账号扩展参数长度
} XIAOWEI_PARAM_ACCOUNT;

// 场景信息，后续优化只用一个ID就行
typedef struct _xiaowei_param_skill {
    const char *name; // 场景名
    const char *id;   // 场景ID
    int type;         // 场景类型，如内置、第三方等
} XIAOWEI_PARAM_SKILL;

// 播放资源结构定义
typedef struct _xiaowei_param_resource {
    XIAOWEI_RESOURCE_FORMAT format; // 资源格式定义
    char *id;                       // 资源ID
    char *content;                  //  资源内容
    int play_count;                 // 资源播放次数, 如果 value == -1, means no limit
    unsigned long long offset;      // URL播放资源
    char *extend_buffer;            // json格式的资源描述信息
} XIAOWEI_PARAM_RESOURCE;

// 播放资源集合
typedef struct _xiaowei_param_res_group {
    XIAOWEI_PARAM_RESOURCE *resources; // 一个集合中包含的播放资源
    unsigned int resources_size;       // 资源个数
} XIAOWEI_PARAM_RES_GROUP;

// 上下文
typedef struct _xiaowei_param_context {
    const char *id;              // 上下文 ID，多轮会话情况下需要带上该ID
    unsigned int speak_timeout;  // 等待用户说话的超时时间 单位ms
    unsigned int silent_timeout; // 用户说话的静音尾点时间 单位：ms
    bool voice_request_begin;    // 声音请求的首包标志，首包时，必须为true
    bool voice_request_end;      // 当使用本地VAD时，声音尾包置为true
    XIAOWEI_WAKEUP_PROFILE wakeup_profile; // 唤醒场景，远场or近场
    XIAOWEI_WAKEUP_TYPE wakeup_type;       // 唤醒类型
    const char *wakeup_word;               // 唤醒词文本
    unsigned long long request_param;      // 请求参数 XIAOWEI_PARAM_PROPERTY
    int local_tts_version;                 //本地TTS版本，
    XIAOWEI_PARAM_SKILL skill_info;        //若携带skill_id，则只会命中当前skill的事件
} XIAOWEI_PARAM_CONTEXT;

// 语音云返回数据
typedef struct _xiaowei_param_response {
    XIAOWEI_PARAM_SKILL skill_info;      // 场景信息
    XIAOWEI_PARAM_SKILL last_skill_info; // 之前的场景信息

    int error_code; // 请求错误码

    char voice_id[33];             // 请求id
    XIAOWEI_PARAM_CONTEXT context; // 上下文信息

    const char *request_text;   // ASR结果文本
    unsigned int response_type; // 用于信息展示的json数据type
    const char *response_data;  // 用于信息展示的json数据
    const char *intent_info;    // 额外暴露给用户的intentInfo

    unsigned int resource_groups_size;        // 资源集合列表size
    XIAOWEI_PARAM_RES_GROUP *resource_groups; // 资源集合列表
    bool has_more_playlist;                   // 是否可以加载更多
    bool has_history_playlist;                // 是否可以加载历史记录
    bool is_recovery;                         // 是否可以恢复播放
    bool is_notify;                           // 是通知
    unsigned int wakeup_flag;                 // 请参考XIAOWEI_WAKEUP_FLAG
                                              // 云端校验唤醒请求带下来的结果，0表示非该类结果，1表示唤醒失败，2表示唤醒成功并且未连续说话，3表示说的指令唤醒词，4可能为中间结果，表示唤醒成功了，还在继续检测连续说话或者已经在连续说话了
    XIAOWEI_PLAYLIST_ACTION play_behavior;    // 列表拼接类型
    XIAOWEI_PLAYLIST_TYPE resource_list_type; // 资源列表类型，可能为当前列表、历史列表等类型
    const char *response_text;                // 回答文本

    /** Control ID, such as PAUSE, PLAY, NEXT..., @see XIAOWEI_DEVICE_CMD */
    unsigned int control_id;
    const char *control_value; //控制value
    bool has_more_playlist_up; // 向上是否可以加载更多
} XIAOWEI_PARAM_RESPONSE;

/* report define begin */

/*设备当前的播放状态，用户应该在状态变化之后及时上报，只需要上报大资源，即打断后能够恢复的资源，例如音乐，FM。无需上报小资源，例如TTS播放。这里列举一下常用场景的上报：
 1. 切歌：报2次，首先第一首歌stoped、然后第二首歌playing。
 2. 播放过程中唤醒了开始语音请求(这时候正常音乐是要暂停的)：上报paused
 3. 操作2点了一首新歌，并且要准备播放了：先报刚才那首暂停的歌stoped，再报新歌playing
 4.
 操作2问了一下天气或者闲聊：首先无需任何上报，音箱播放天气或者闲聊的TTS，对话结束之后正常逻辑应该恢复播放音乐，然后这时候上报playing。
 5. 该播放的东西播完了，无事可做：上报idle
 */
typedef enum _xiaowei_res_player_state {
    play_state_preload = 0, //预加载
    play_state_start = 1,   //开始播放
    play_state_paused = 2, //播放暂停（处于某个场景下，比如播放音乐，就是单纯的暂停了）
    play_state_stoped = 3, //播放停止（不会再恢复当前资源了，针对一首歌自动播放完毕
    play_state_finished = 4, // 所有资源播放结束，停止播放了
    play_state_idle = 5,     //设备空闲，什么也没干，不处于某个技能下
    play_state_resume = 6,   //播放资源（继续，与暂停对应）
    play_state_abort = 11,   //播放中断，用户主动切歌
    play_state_err = 12,     //播放出错
} XIAOWEI_PLAY_STATE;

//资源类型，当前播放的是什么资源，不同类型的content就不一样
typedef enum _xiaowei_state_info_res_content_Type {
    report_res_type_url = 0,     //对应的content就是这个url
    report_res_type_text = 1,    //对应的content就是这个text
    report_res_type_tts_id = 2,  //对应的content就是这个TTS的ID，目前暂时不用上报
    report_res_type_tts_url = 3, //对应的content就是这个TTS的URL，目前暂时不用上报
} XIAOWEI_REPORT_RES_TYPE;

//设备的播放模式，由用户自己维护，注意随机播放的时候不要随机播放TTS
typedef enum _xiaowei_res_play_mode {
    play_mode_sequential_no_loop = 0, //顺序播放，播完就结束，比如常见的新闻播放之类的
    play_mode_sequential_loop_playlist = 1, //顺序播放，播放列表播完了又重新开始。
    play_mode_shuffle_no_loop = 2,          //随机播放列表中的资源，不循环
    play_mode_shuffle_loop_playlist = 3,    //随机播放列表中的资源，循环
    play_mode_loop_single = 4,              //单曲循环
} XIAOWEI_PLAY_MODE;

/* report define end */

// 上报状态信息
typedef struct _xiaowei_param_state {
    XIAOWEI_PARAM_SKILL skill_info;    // 场景信息
    XIAOWEI_PLAY_STATE play_state;     // 请参考XIAOWEI_PLAY_STATE
    XIAOWEI_AVAILABILITY availability; // 目前设备的可用状态，
    const char *play_id;               // 资源ID
    const char *play_content;          // 资源内容
    unsigned long long play_offset;    // 播放偏移量，单位：s
    XIAOWEI_PLAY_MODE play_mode;       // 播放模式
    XIAOWEI_RESOURCE_FORMAT resource_type;

    // if music, those info are recommended
    const char *resource_name; // e.g. music name
    const char *performer;     // e.g. singer
    const char *collection;    // e.g abulm
    const char *cover_url;

    int volume;     //音量，0~100
    int brightness; //亮度，0~100
} XIAOWEI_PARAM_STATE;

// 音频数据 TTS
typedef struct _xiaowei_param_audio_data {
    const char *id;                   // 资源id
    unsigned int seq;                 // 序号
    unsigned int is_end;              // 最后一包了
    unsigned int pcm_sample_rate;     // pcm采样率
    unsigned int sample_rate;         // 音频数据的(例如:opus)采样率
    unsigned int channel;             // 声道
    XIAOWEI_AUDIO_DATA_FORMAT format; // 格式(例如:opus)
    const char *raw_data;             // 数据内容
    unsigned int raw_data_len;        // 数据长度
} XIAOWEI_PARAM_AUDIO_DATA;

// 登录状态信息
typedef struct _xiaowei_param_login_status {
    unsigned int type;         // 登录态类型 QQ 1 WX 2, other 3
    const char *app_id;        // 登录的appid
    const char *open_id;       // 登录的openId
    const char *access_token;  // 登录的accessToken
    const char *refresh_token; // 登录的refreshToken
    const char *skill_id;      // 登录的skillId
    //    const char *third_part_extend_info; //第三方的应用自己的登录信息
} XIAOWEI_PARAM_LOGIN_STATUS;

// gpsinfo
typedef struct _xiaowei_gps_info {
    bool is_gps_available;       // gps信息是否可用，
    double longitude;            //经度，正东经，负西经
    double latitude;             //纬度，正北纬，负南纬
    double altitude;             //海拔，单位米
    unsigned long long utc_time; // time，从1970年1月1日到现在经过的毫秒数
    double speed;                // knots 速度 节
} XIAOWEI_GPS_INFO;

//用户上报日志的数据结构，小微出现问题的时候使用
typedef struct _xiaowei_param_log {
    const char *voice_id;             //出现问题时的voiceID
    const char *log_data;             //问题出现时的数据，可以是日志、堆栈等
    const char *event;                //用户自己定义当前的event，
    unsigned long long time_stamp_ms; //问题发生的时间
} XIAOWEI_PARAM_LOG_REPORT;

typedef enum enum_xiaowei_words_type {
    xiaowei_words_command = 0, // 可见可答屏幕词表，常用于屏幕上的按钮
    xiaowei_words_contact = 1, // 联系人词表，设置后，一直有效。重复调用将替换之前设置的联系人词表。
} XIAOWEI_WORDS_TYPE;

/**
 * 设置词表的异步结果回调
 * @param op 操作 0:上传 1:删除
 * @param errCode 0:成功 非0:失败
 */
typedef void (*XIAOWEI_SET_WORDSLIST_CALLBACK)(int op, int errCode);

/**
 * 通用请求的回调
 * @param voice_id 请求时的voiceID
 * @param json 返回的json数据
 */
typedef void (*XIAOWEI_ON_REQUEST_CMD_CALLBACK)(const char *voice_id, int xiaowei_err_code,
                                                const char *json);

/**
 * 小微的基础回调
 */
typedef struct _xiaowei_callback {
    /**
     * 语音/文本请求的回调。也有可能直接来自于后台push（例如app上点歌）
     * @param voice_id 请求时的voiceID
     * @param event {@see XIAOWEI_EVENT}
     * @param state_info
     * 根据event的不同，将其强转为对应的数据类型，tts的时候强转为XIAOWEI_PARAM_AUDIO_DATA，其它时候强转为XIAOWEI_PARAM_RESPONSE
     * @param extend_info 额外的信息，与业务相关
     */
    bool (*on_request_callback)(const char *voice_id, XIAOWEI_EVENT event, const char *state_info,
                                const char *extend_info, unsigned int extend_info_len);

    /**
     * 收到响应后，小微可以实时返回当前网络延时状况，用于评估和展示
     */
    bool (*on_net_delay_callback)(const char *voice_id, unsigned long long time);
} XIAOWEI_CALLBACK;

/**
 * 小微的设备notify
 */
typedef struct _xiaowei_notify {
    /**
     * 后台希望设备能够重启一下，多半是出现严重bug了
     */
    void (*on_device_reboot)(void);

    /**
     * 小微服务退出了
     */
    void (*on_service_exit)(void);

} XIAOWEI_NOTIFY;

/**
 * GPS需求的回调，当小微需要GPS信息时触发，由于同步操作，此函数执行一定不能阻塞，建议用户设置一个buffer存储GPS信息，
 * @param gpsInfo 填写gpsInfo中的所有可用值，若GPS不可用，is_gps_available = false;
 */
typedef void (*XIAOWEI_GPS_CALLBACK)(XIAOWEI_GPS_INFO *gpsInfo);

CXX_EXTERN_END_DEVICE

#endif /* xiaoweiAudioType_h */
