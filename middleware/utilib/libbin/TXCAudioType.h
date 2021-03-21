#ifndef __TX_CLOUD_AUDIO_TYPE_H__
#define __TX_CLOUD_AUDIO_TYPE_H__


#include "TXSDKCommonDef.h"


CXX_EXTERN_BEGIN

/**
 *
 * 语音云通道层接口
 *
 */
#define TXCA_BUF_MAX_LENGHT  6400

//txc_audio_callback.on_state回调的event
typedef enum _txca_event {
    txca_event_on_idle               = 0,      // 空闲
    txca_event_on_request_start      = 1,      // 请求开始
    txca_event_on_speak              = 2,      // 检测到说话
    txca_event_on_silent             = 3,      // 检测到静音(only@device has not txca_device_local_vad)
    txca_event_on_recognize          = 4,      // 识别文本实时返回
    txca_event_on_response           = 5,      // 请求收到响应
    txca_event_on_tts                = 6,
    txca_event_on_net_delay          = 7,
} txca_event;

typedef enum _txca_device_property {
    txca_device_local_vad            = 0x0000000000000001,  // use local vad
    txca_device_gps                  = 0x0000000000000002,
    txca_device_wifi_ap              = 0x0000000000000004,
    txca_device_local_tts            = 0x0000000000000008,  // use local tts
    txca_device_dump_silk            = 0x0000000000000010,  // dump silk
    txca_device_disable_audio_encode = 0x0000000000000020,  // disable audio input encode
} txca_device_property;

typedef enum _txca_resource_format {
    txca_resource_tts                = 0,         //TTS播放(附带文本信息)
    txca_resource_url                = 1,         //url资源
    txca_resource_notify             = 2,         //提醒类
    txca_resource_text               = 3,         //纯文本(无TTS)
    txca_resource_command            = 4,         //指令类型
    txca_resource_intent             = 5,         //语义类型
} txca_resource_format;

typedef enum _txca_playlist_action {
    txca_playlist_replace_all        = 0,         //中断当前播放，替换列表
    txca_playlist_enqueue            = 1,         //拼接到列表队尾
    txca_playlist_replace_enqueue    = 2,         //不中断当前播放的资源，替换列表
} txca_playlist_action;

typedef enum _txca_chat_type {
    txca_chat_via_voice              = 0,         //语音请求
    txca_chat_via_text               = 1,         //文本请求
    txca_chat_only_tts               = 2,         //tts请求
} txca_chat_type;

typedef enum _txca_playstate {
    txca_playstate_preload           = 0,         // 预加载
    txca_playstate_resume            = 1,         // 继续
    txca_playstate_paused            = 2,         // 暂停
    txca_playstate_stopped           = 3,         // 一首歌播放完毕
    txca_playstate_finished          = 4,         // 歌单播放结束，停止播放了
    txca_playstate_idle              = 5,
    txca_playstate_start             = 6,         // 一首歌开始播放
} txca_playstate;

typedef enum _txca_account_type {
    txca_account_null                = 0,
    txca_account_qq                  = 1,         // QQ
    txca_account_wx                  = 2,         // 微信
    txca_account_3rd                 = 3,         // 3rd
} txca_account_type;

//account data
typedef struct _txca_param_account
{
    unsigned int            type;                   // txca_account_type
    const char*             account;                // if type is QQ/WX, must set value to token and appid
    const char*             token;                  // if type is 3rd, token is null, and must bind/unbind device by cgi
    const char*             appid;

    char*                   buffer;                 // account buffer
    unsigned int            buffer_len;             // account buffer length
} TXCA_PARAM_ACCOUNT;

//设备能力集合，比如使用本地vad、有gps、可以做wifiap等
typedef struct _txca_param_device
{
    unsigned long long      properties;             // devices prop as txca_device_property
    char*                   extend_buffer;
    unsigned int            extend_buffer_len;
} TXCA_PARAM_DEVICE;

//场景信息
typedef struct _txca_param_app
{
    const char*             app_name;               // app name
    const char*             app_id;                 // app ID
    int                     app_type;
} TXCA_PARAM_APP;

// 播放资源
typedef struct _txca_param_resource
{
    txca_resource_format    format;                 // url/tts/notify/text/command
    char*                   id;                     // resource's id, if format == text or notify, id is null
    char*                   content;                // url(url/notify) or text(tts/text)
    char*                   extend_buffer;          // json
} TXCA_PARAM_RESOURCE;

//上下文
typedef struct _txca_param_context
{
    const char*             id;                     //context id
    unsigned int            speak_timeout;          //wait speak timeout time(ms)
    unsigned int            silent_timeout;         //speak to silent timeout time(ms)
    bool                    voice_request_begin;
    bool                    voice_request_end;
    bool                    enable_V2A;             //enable visible to answer
    char**                  words_list;             //screen words
    unsigned int            list_count;             //words cont
} TXCA_PARAM_CONTEXT;

// 语音云返回数据
typedef struct _txca_param_response
{
    TXCA_PARAM_APP          app_info;               //场景信息

    unsigned int            error_code;             //请求错误码

    char                    voice_id[33];           //请求id
    TXCA_PARAM_CONTEXT      context;                //上下文信息

    const char*             request_text;           //ASR结果文本
    const char*             answer_text;            // answer文本
    unsigned int            response_type;          //用于信息展示的json数据type
    const char*             response_data;          //用于信息展示的json数据

    unsigned int            resources_size;         //资源列表size
    TXCA_PARAM_RESOURCE*    resources;              //资源列表
    bool                    has_more_playlist;      //是否可以加载更多
    bool                    is_recovery;            //是否可以恢复播放
    txca_playlist_action    play_behavior;          //列表拼接类型
} TXCA_PARAM_RESPONSE;

//state for report
typedef struct _txca_param_state
{
    TXCA_PARAM_APP          app_info;               //场景信息
    
    unsigned int            play_state;             //txca_playstate
    const char*             play_id;
    const char*             play_content;
    unsigned long long      play_offset;
    unsigned int            play_mode;
} TXCA_PARAM_STATE;

CXX_EXTERN_END

#endif // __TX_CLOUD_AUDIO_TYPE_H__
