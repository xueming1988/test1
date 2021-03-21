#ifndef __TX_AI_AUDIO_IO_H__
#define __TX_AI_AUDIO_IO_H__

#include "TXSDKCommonDef.h"
#include "TXDataPoint.h"
#include "TXAIAudioPlayer.h"

CXX_EXTERN_BEGIN


//SYS-APPID range:
//8DAB4796-FA37-4114-0000-000000000000 ~ 8DAB4796-FA37-4114-FFFF-FFFFFFFFFFFF
#define AIAUDIO_APPID_GENERAL   "00000000-0000-0000-0000-000000000000"
//全局指令,通用控制
#define AIAUDIO_APPID_GLOBAL    "8dab4796-fa37-4114-0000-7637fa2b0000"
//唤醒app
#define AIAUDIO_APPID_WAKEUP    "8dab4796-fa37-4114-0000-7637fa2b0001"

//QQ msg
#define AIAUDIO_APPID_QMSG      "8dab4796-fa37-4114-0001-7637fa2b0000"
//QQ电话
#define AIAUDIO_APPID_QCALL     "8dab4796-fa37-4114-0001-7637fa2b0001"
#define APPNAME_QQCALL  "通讯-QQ通话"
//微信电话
#define AIAUDIO_APPID_WCALL     "8dab4796-fa37-4114-0001-7637fa2b0002"

//语音聊天,发送QQ消息
#define AIAUDIO_APPID_VOICECHAT "8dab4796-fa37-4114-0002-7637fa2b0001"
#define APPNAME_QQMSG   "通讯-QQ消息"

//腾讯视频
#define AIAUDIO_APPID_TV        "8dab4796-fa37-4114-0026-7637fa2b0001"
//导航
#define AIAUDIO_APPID_NAV        "8dab4796-fa37-4114-0026-7637fa2b0002"

//音乐电子书等大资源
#define AIAUDIO_APPID_MUSIC     "8dab4796-fa37-4114-0011-7637fa2b0001"
#define AIAUDIO_APPID_EBOOK     "8dab4796-fa37-4114-0011-7637fa2b0002"

// FM
#define AIAUDIO_APPID_FM     "8dab4796-fa37-4114-0024-7637fa2b0001"

#define AIAUDIO_APPID_NEWS    "8dab4796-fa37-4114-0019-7637fa2b0001"

//小资源
//闹钟
#define AIAUDIO_APPID_CLOCK     "8dab4796-fa37-4114-0012-7637fa2b0001"

//消息盒子
#define AIAUDIO_APPID_MSGBOX    "8dab4796-fa37-4114-0012-7637fa2b0002"
//天气
#define AIAUDIO_APPID_WEATHER   "8dab4796-fa37-4114-0012-7637fa2b0003"

// 闹钟触发场景
#define AIAUDIO_APPID_TRIGGER_CLOCK "8dab4796-fa37-4114-0036-7637fa2b0001"


/**
 *
 * 智能语音相关接口
 *
 */

#define MAX_AI_AUDIO_BUF_LENGHT  6400
#define MIN_AI_AUDIO_BUF_LENGHT  64

//音频类型
enum ai_audio_format {
    ai_audio_format_pcm  = 1,
    ai_audio_format_speex  = 4,
    ai_audio_format_amr  = 5,
    ai_audio_format_silk = 6,
    ai_audio_format_pcm_without_silk = 7,
};

//tx_ai_audio_callback.on_state回调的event
enum tx_ai_audio_state {
    tx_ai_audio_state_idle                  = 0,    // 空闲
    tx_ai_audio_state_request_start         = 1,    // 语音请求开始
    tx_ai_audio_state_request_wait          = 2,    // 语音请求等待响应
    tx_ai_audio_state_response              = 3,    // 语音请求收到响应
    tx_ai_audio_state_playing               = 4,    // 正在播放
    tx_ai_audio_state_msgbox_changed        = 5,    // 消息盒子数据变化
    tx_ai_audio_state_msg_start             = 11,   // 开始一条消息播放
    tx_ai_audio_state_msg_stop              = 12,   // 结束一条消息播放
    tx_ai_audio_state_fetch_device_basic_info              = 13,   // 用户询问设备的基础信息
    tx_ai_audio_state_msgproxy_stat         = 20,   // 消息代收状态通知
    tx_ai_audio_state_msg_send              = 21,   // 消息发送通知
    tx_ai_audio_state_playlist_updated      = 22,   // 播放列表更新（用于音乐）
    tx_ai_audio_state_new_pic_received      = 23,   // 收到图片(用于小程序上传图片)
    tx_ai_audio_state_qqcall_event          = 24,   // qqcall状态改变通知(数值对tx_ai_audio_qqcall_state)
};

enum tx_ai_audio_qqcall_state {
    tx_ai_audio_qqcall_state_idle       = 0, //空闲状态(无通话或挂断后的状态)
    tx_ai_audio_qqcall_state_invite     = 1, //收到邀请未接听状态
    tx_ai_audio_qqcall_state_calling    = 2, //通话中
    tx_ai_audio_qqcall_state_callout    = 3, //发起呼叫中未被接听状态
};

// state for tx_ai_audio_state_msg_send
enum tx_ai_audio_msg_send_state {
    tx_ai_audio_msg_send_idle = 0,  // 空闲
    tx_ai_audio_msg_send_ing = 1,   // 正在发送消息
    tx_ai_audio_msg_send_end = 2,   // 消息发送完成 errcode 0:成功 非0:失败
};

enum tx_ai_audio_msgproxy_option {
    tx_ai_audio_msgproxy_option_open = 1,  //代收消息开启操作结果通知
    tx_ai_audio_msgproxy_option_close = 2, //代收消息关闭操作结果通知
    tx_ai_audio_msgproxy_option_state = 3, //代收消息状态更新
};

//本地通用控制id
enum ai_audio_local_common_control_id {
    ai_audio_resume  = 700003,
    ai_audio_pause = 700004,
    ai_audio_prev  = 700005,
    ai_audio_next  = 700006,
    ai_audio_playmode_random = 700103,
    ai_audio_playmode_single_loop = 700113,
    ai_audio_playmode_order = 700104,
    ai_audio_gain_audio_focus = 700131,
    ai_audio_lost_audio_focus = 700132,
    ai_audio_active_app = 700133,
    ai_audio_playmode_loop = 700137,
    ai_audio_error_feedback = 700141,

    ai_audio_audiomsg_record = 710150,
    ai_audio_audiomsg_send = 710151,
    ai_audio_audiomsg_cancel = 710152,
    ai_audio_audiomsg_record_novad = 710153,    //发语音消息无静音检测

    ai_audio_set_dev_config = 710200,   //配置sdk属性，设置config

    ai_audio_location_send = 711062,
    ai_audio_qqmsgproxy_close = 711063, //关闭代收的本地命令
    ai_audio_qqmsgproxy_open = 711064,  //开启代收的本地命令
    
    ai_audio_local_qqcall_hangup = 711021,  //本地接口挂断通话
    ai_audio_local_qqcall_accept = 711022,  //本地接口接听通话
    ai_audio_local_qqcall_reject = 711023,  //本地接口拒绝接听
    
};

#define AI_VOLUME_GET_VALUE(val, diff_percent, diff_val, abs_percent, abs_val) { diff_val = (signed char)(val); diff_percent = (signed char)((val >> 8)); abs_val = (signed char)((val >> 16)); abs_percent = (signed char)(val >> 24);}
#define AI_VOLUME_SET_VALUE(val, diff_percent, diff_val, abs_percent, abs_val) { val = (((abs_percent << 24)) |((abs_val << 16)) | ((diff_percent << 8)) | diff_val & 0xFF);}

//tx_ai_audio_callback.on_control回调的ctrlcode
enum tx_ai_audio_control_code {
    tx_ai_audio_control_code_volume             = 1,    //调节全局音量, on_control中的value为音量值,范围-100 ~ -100
    tx_ai_audio_control_code_startrecord        = 2,    //开始录音
    tx_ai_audio_control_code_endrecord          = 3,    //结束录音
    tx_ai_audio_control_code_volume_silence     = 4,    //调节全局静音,0静音 1取消静音
    tx_ai_audio_control_code_upload_log         = 5,    //上报日志
    tx_ai_audio_control_code_get_screen_words   = 6,    //获取屏幕词表
    tx_ai_audio_control_code_get_location       = 7,    //获取当前位置
};

// tx_ai_audio_callback.on_rsp回调的rsp_type
enum tx_ai_audio_rsp_type {
    tx_ai_audio_rsp_play_detail_info          = 0,   // 播放类详情信息
    tx_ai_audio_rsp_extend_info               = 1,   // 通用详情信息
    tx_ai_audio_rsp_device_config_info        = 2,   // 设备配置信息
    tx_ai_audio_rsp_device_diss_list          = 3,   // 歌单(或其他集合类资源)列表信息
    tx_ai_audio_rsp_device_play_list          = 4,   // 某个歌单下的资源列表
    tx_ai_audio_rsp_get_device_alarm_list     = 5,   // 设备闹钟列表
    tx_ai_audio_rsp_update_device_alarm       = 6,   // 闹钟新增/修改
    tx_ai_audio_rsp_trigger_timing_skill      = 7,   // 请求触发定时播放Skill
    tx_ai_audio_rsp_get_more_list      = 8,   // 预拉取
    tx_ai_audio_rsp_set_login_status      = 9,   // 设置登录态
    tx_ai_audio_rsp_get_login_status      = 10,   // 查询登录态
    tx_ai_audio_rsp_get_music_vip_info      = 11,   // 音乐vip信息
    tx_ai_audio_rsp_get_pic_list = 12,      //拉取小程序图片列表
    tx_ai_audio_rsp_upload_call_name = 13,  //上传联系人
    tx_ai_audio_rsp_delete_call_name = 14,  //删除联系人
};

enum tx_ai_audio_msg_control_code {
    tx_ai_audio_msg_control_code_queryall   = 0,
    tx_ai_audio_msg_control_code_play       = 1,
    tx_ai_audio_msg_control_code_stop       = 2,
    tx_ai_audio_msg_control_code_readed     = 3,    //标识消息已读
};

enum tx_ai_audio_words_type
{
    tx_ai_audio_words_type_realtime = 0,    //实时词表 (一次请求响应有效)
    tx_ai_audio_words_type_contacts = 1,    //联系人词表 (需要主动清除)
};

// 闹钟操作类型
typedef enum _tx_ai_audio_alarm_opt_type
{
    tx_ai_audio_add_alarm    = 1,    // 新增闹钟
    tx_ai_audio_update_alarm = 2,    // 修改闹钟
    tx_ai_audio_delete_alarm = 3,    // 删除闹钟
} tx_ai_audio_alarm_opt_type;

//语音上行参数
typedef struct _tx_ai_audio_encode_param
{
    unsigned int  audio_format;                     //ai_audio_format
    unsigned int  samples_per_sec;                  //每秒采样率(单位k),建议设置为16
    unsigned int  sample_bits;                      //sample位数, 固定为16， 录音须采用short数据类型
} tx_ai_audio_encode_param;

//语音状态数据
typedef struct _tx_ai_audio_event_info
{
    const char*             app_name;               //AppName
    const char*             app_id;                 //AppID

    const char*             voice_id;               //请求id
    bool                    has_context;            //是否有下文

    const char*             text_question;          //语音识别结果文本
    const char*             text_answer;            //语音处理结果文本

    const char*             app_info;               //App状态数据-用于扩展
    unsigned int            app_info_type;          //App状态数据类型
    unsigned long           app_info_lenght;        //App状态数据长度
    unsigned int            error_code;             //请求错误码
    unsigned int            wakeup_flag;             //云端唤醒校验
    unsigned int            rsp_type;             //响应类型
} tx_ai_audio_event_info;

// 语音通用请求响应数据
typedef struct _tx_ai_audio_rsp_app_info
{
    const char*    app_name;                        //场景名
    const char*    app_id;                          //场景id

    const char*    rsp_app_info;                    //响应数据
    unsigned long  rsp_app_info_length;             //响应数据长度
    
    const char*    voice_id;                        //请求id
    
} tx_ai_audio_rsp_app_info;

enum tx_ai_audio_msg_type {
    tx_ai_audio_msg_type_invalid                =0,
    tx_ai_audio_msg_type_base_audio             =1,
    tx_ai_audio_msg_type_base_text              =2,
    tx_ai_audio_msg_type_im_text                =3,
    tx_ai_audio_msg_type_im_audio               =4,
    tx_ai_audio_msg_type_im_location            =5,
    tx_ai_audio_msg_type_im_image               =6,
    tx_ai_audio_msg_type_im_video               =7,
    tx_ai_audio_msg_type_im_music               =8,
};

//msg info
typedef struct _tx_ai_audio_msginfo
{
    //see @tx_ai_audio_msg_type
    unsigned int            msg_type;
    unsigned long           msg_id;
    unsigned long long      peer_id;
    long                    timestamp;
    bool                    readed;
    unsigned int            direction;  //0:收到的消息 1:发送的消息

    //message detail info.
    const char*             msg_detail;
} tx_ai_audio_msginfo;

typedef struct _tx_ai_audio_location
{
    double          longitude;          // 经度
    double          latitude;           // 纬度
    const char*     description;        // 位置描述
} tx_ai_audio_location;

// c2c msg struct define
typedef struct _tx_ai_cc_msg
{
    const char*                 business_name; // 业务名：ai.external.xiaowei   ai.internal
    
    const char*                 msg;
    unsigned int                msg_len;
} tx_ai_cc_msg;

//AIAudio主回调
typedef struct _tx_ai_audio_callback
{
    void (*on_state)(int event, tx_ai_audio_event_info* info);                  //SDK状态回调
    void (*on_control)(int ctrlcode, int value);                                //SDK控制回调
    void (*on_rsp)(int errCode, int rsp_type, tx_ai_audio_rsp_app_info* info);  //通用请求回调
    void (*on_cc_msg_notify)(unsigned long long from, tx_ai_cc_msg* msg);
    void (*on_send_cc_msg_result)(unsigned int cookie, unsigned long long to, int err_code);
    void (*on_wakeup)();// 云端唤醒成功
    int  (*on_get_vol)();
    void (*on_net_delay)(unsigned long long ms, const char* voiceId);// 语音请求延迟

} tx_ai_audio_callback;

typedef struct _tx_ai_device_state
{
    const char*             app_name;
    const char*             app_id;
    
    unsigned int            play_state; //ai_audio_playstate
    const char*             play_url;
    const char*             play_id;
    unsigned long long      play_offset;
    unsigned int            play_mode;
} tx_ai_device_state;

enum tx_ai_audio_playlist_op_type {
    tx_ai_audio_playlist_op_type_delete     = 1,    // 删除
};

// 语音服务的参数
typedef struct _tx_ai_audio_service_param
{
    bool IsNotUsePlayManager;                     //不使用播放管理
    bool DumpSilk;                              // 保存silk文件到/sdcard/tencent/xiaowei/voice/send
} tx_ai_audio_service_param;

// 语音请求的参数
typedef struct _tx_ai_audio_request_param
{
    bool reset;                     // 重置
    bool just_vad;                  // 只做vad
    unsigned int wakeup_mode;        // 0 本地唤醒 1 云端校验唤醒
    unsigned int silence_time;       // 静音尾点时间
    unsigned int timeout_time;       // 不说话超时时间
    bool local_vad;                // 使用本地静音检测
    unsigned int profile_type;   // 0 远场 1 近场
    const char*             wakeup_text;// 唤醒词
} tx_ai_audio_request_param;

/**
 * 接口说明：用于配置AI Audio服务参数，需要在tx_ai_audio_service_start后调用
 * 参数说明：player 用于声音播放的播放管理器接口
 * 参数说明：recordinfo 用于语音流上行的参数
 * 返回值  ：错误码（见全局错误码表）
 */
SDK_API int tx_ai_audio_configservice(tx_ai_audio_player* player, tx_ai_audio_encode_param* recordinfo);

/**
 * 接口说明：用于配置AI Audio服务参数，需要在tx_ai_audio_service_start后调用
 * 参数说明：player 用于声音播放的播放管理器接口
 * 参数说明：recordinfo 用于语音流上行的参数
 * 参数说明：player_event_callback 播放事件的回调 开始播放某一首歌、暂停、恢复、收藏、取消收藏、设置播放模式、分享、播放完毕
 * 返回值  ：错误码（见全局错误码表）
 */
SDK_API int tx_ai_audio_configservice_with_player_callback(tx_ai_audio_player * player, tx_ai_audio_encode_param * recordinfo, tx_ai_audio_player_event * player_event_callback);
/**
 * 接口说明：Start AI Audio相关服务，该服务需要登录成功后才能调用，否则会有错误码返回
 * 参数说明：callback AIAudio服务回调接口
 * 返回值  ：错误码（见全局错误码表）
 */
SDK_API int tx_ai_audio_service_start(tx_ai_audio_callback *callback);

/**
* 接口说明：Stop AI Audio相关服务
* 返回值  ：错误码（见全局错误码表）
*/
SDK_API int tx_ai_audio_service_stop();

/**
* 接口说明：开始语音请求(同时只会有一个请求)
* 返回值  ：错误码（见全局错误码表）
*/
SDK_API int tx_ai_audio_request_start(tx_ai_audio_request_param* param);

/**
* 接口说明：停止语音请求
* 返回值  ：错误码（见全局错误码表）
*/
SDK_API int tx_ai_audio_request_cancel();

/**
 * 向语音请求填充音频数据
 * param：arg0 音频数据 单声道数据
 * param：length  arg0 数据长度
 * param：pcm_length 音频数据原始长度(PCM), 若不做压缩则等于length
 * param：status only for Android SDK，始终为0
 * 返回值  ：错误码（见全局错误码表）
 */
SDK_API int tx_ai_audio_request_fill_data(const void* audio_data, int data_length, int pcm_length, int status);

/**
 * 接口说明：开始文字请求(同时只会有一个请求)
 * param：text 要请求的文字(类似语音说话的内容，如："我要听音乐")
 * 返回值  ：错误码（见全局错误码表）
 */
SDK_API int tx_ai_audio_request_Text(char* voice_id, const char* text, tx_ai_device_state* state, bool play);

/**
 * 接口说明：请求TTS播放(同时只会有一个请求)
 * param：text 要播放的文本
 * 返回值  ：错误码（见全局错误码表）
 */
SDK_API int tx_ai_audio_request_TTS(const char* text);

/**
 * 接口说明：用于加载播放类详情信息
 * param：play_id_list playid数组
 * param：list_size    playid数组长度
 * 返回值：错误码（见全局错误码表）
 */
SDK_API int tx_ai_audio_get_play_detail_info(char *voice_id, char **play_id_list, int list_size, char* app_name);

/**
 * 接口说明：用于预加载播放列表
 * 返回值：错误码（见全局错误码表）
 */
SDK_API int tx_ai_audio_get_more_play_list();

/**
 * 接口说明：用于播放指定playid的资源
 * 返回值：错误码（见全局错误码表）
 */
SDK_API int tx_ai_audio_play_by_id(const char* play_id);

/**
 * 接口说明：用于本地的通用控制 
 * propid: 指令id，定义为 ai_audio_local_common_control_id
 * propValue: 附加值，没有可为空
 * 返回值：错误码（见全局错误码表）
 */
SDK_API int tx_ai_audio_local_common_control(unsigned int propid, const char * propValue);

/**
 * 接口说明：用于切换当前app, 目前仅可用于切换到音乐， fm等类型
 * app_name: 需要切换到的app name
 * app_id: 需要切换到的app id, 优先使用
 * resume: 是否自动播放之前暂停的资源(音乐等)
 * 返回值: 错误码(见全局错误码表)
 */
SDK_API int tx_ai_audio_active_app(const char * app_name, const char * app_id, bool resume);

/**
 * 接口说明：查询消息详情
 * msg_id_list: 消息id list
 * msg_count: 消息id list的size
 * info_list: 消息详情 list
 * 返回值：错误码（见全局错误码表）
 */
SDK_API int tx_ai_audio_get_msginfo(unsigned long* msg_id_list, unsigned int msg_count, tx_ai_audio_msginfo* info_list);

/**
 * 接口说明：消息控制接口
 * cmd: 指令id，定义为 tx_ai_audio_msg_control_code
 * msg_id: 消息id
 * 返回值：错误码（见全局错误码表）
 */
SDK_API int tx_ai_audio_set_msg_command(int cmd, unsigned long msg_id);

/**
 * 接口说明：发送CC消息
 * to: 消息接收方tinyId
 * msg: 消息体
 * cookie: 消息发送cookie
 *
 */
SDK_API int tx_ai_audio_send_cc_msg(unsigned long long to, tx_ai_cc_msg* msg, unsigned int* cookie);

/**
 * 接口说明: 开启实时获取屏幕词表，可见可答功能
 * bEnable: true开启 false关闭
 * ret: 0成功 非0失败
 */
SDK_API int tx_ai_audio_enable_realtime_wordslist(bool bEnable);

/**
 * 接口说明: 设置词表
 * type: 词表类型，参见tx_ai_audio_words_type
 * words_list: 词表列表
 * list_size: 词表数量
 * ret: 0成功 非0失败
 */
SDK_API int tx_ai_audio_set_wordslist(int type, char** words_list, int list_size);


enum tx_ai_audio_info
{
    tx_ai_audio_info_msgproxy_state = 10,    //获取代收消息的开关状态
    tx_ai_audio_friend_uin_to_tinyid = 11,   //代收列表uin转换tinyid
};
/**
 * 接口说明: 获取设备的某些状态值
 * type: 获取的数据类型 见tx_ai_audio_info
 * param: 参数可选
 * info: 返回数据的buff-外部申请
 * len: buff长度
 */
SDK_API int tx_ai_audio_get_device_info(int type, const char* param, char* info, int len);

/**
 * 接口说明: 设置location
 * words_list: 词表列表
 * ret: 0成功 非0失败
 */
SDK_API int tx_ai_audio_set_location(tx_ai_audio_location* location);

/**
 * 接口说明: 触发定时Skill任务，请求后台下发播放资源
 * clock_id: 定时Skill任务的标识id
 * ret: 0成功 非0失败
 */
SDK_API int tx_ai_audio_trigger_timing_skill(char* voice_id, const char* clock_id);

/**
 * 接口说明：当闹钟提醒到点时，可以通过该接口请求后台下发闹铃资源，由SDK内部控制播放
 * clock_id: 闹钟提醒的标识id
 * ret: 0成功 非0失败
 */
SDK_API int tx_ai_audio_fire_alarm_event(char* voice_id, const char* clock_id);

/**
 * 接口说明：拉取闹钟/定时Skill任务列表
 * ret: 0成功，非0失败
 */
SDK_API int tx_ai_audio_get_device_alarm_list(char* voice_id);

/**
 * 接口说明：新增/修改闹钟
 * opt_type: 操作类型 请参考tx_ai_audio_alarm_opt_type中的取值
 * clock_info_json: 闹钟信息
 * ret: 0成功 非0失败
 */
SDK_API int tx_ai_audio_update_device_alarm(char* voice_id, int opt_type, const char* alarm_info_json);


SDK_API int tx_ai_audio_set_context_appname(const char* app_name);

/**
 * 接口说明:设置资源的品质（如：音乐的品质），如果当前正在使用音乐app，则会自动更新播放列表
 * quality: 品质的值（目前用于音乐资源，0 流畅，1 标准，2 高，3 无损）
 * ret: 0成功 非0失败
 */
SDK_API int tx_ai_audio_set_quality(int quality);

SDK_API int tx_ai_get_current_appname(char* app_name);

SDK_API int tx_ai_audio_get_pic_list(char* voice_id, unsigned long long offset, unsigned int limit, bool asc);

/**
 * 接口说明：查询消息的总数和未读消息
 * unread：未读消息的个数
 * total：总消息个数
 */
SDK_API int tx_ai_audio_query_msgbox_state(int * unread, int * total);

///////////////////////////////////////////////////

CXX_EXTERN_END

#endif // __TX_AI_AUDIO_IO_H__
