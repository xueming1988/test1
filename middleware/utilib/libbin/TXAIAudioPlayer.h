#ifndef __TX_AI_AUDIO_PLAYER_H__
#define __TX_AI_AUDIO_PLAYER_H__

#include "TXSDKCommonDef.h"
#include "TXDataPoint.h"

CXX_EXTERN_BEGIN

//tx_ai_audio_player.controll回调的ctrlcode
enum tx_ai_audio_player_control_code {
    tx_ai_audio_player_control_code_stop        =0,          //停止
    tx_ai_audio_player_control_code_start       =1,          //开始, value=0 narmal start, value=1 restart current
    tx_ai_audio_player_control_code_pause       =2,          //暂停
    tx_ai_audio_player_control_code_resume      =3,          //继续
    tx_ai_audio_player_control_code_next        =4,          //下一首
    tx_ai_audio_player_control_code_prev        =5,          //上一首
    tx_ai_audio_player_control_code_volume      =6,          //调节播放器音量，value from 0 to 100
    //tx_ai_audio_player_control_code_querystate  =7,        //查询状态，收到后需要调用tx_ai_audio_player_statechange同步状态给SDK
};

//tx_ai_audio_player_statechange的state
enum tx_ai_audio_player_state {
    tx_ai_audio_player_state_start      = 1,            //开始播放: 新播放一个资源
    tx_ai_audio_player_state_abort      = 2,            //退出播放: 终止播放, 调用tx_ai_audio_player_control_code_stop后触发
    tx_ai_audio_player_state_complete   = 3,            //完成播放: 完成一首, 包括调用下一首上一首等
    tx_ai_audio_player_state_pause      = 4,            //暂停播放: 暂停, 调用tx_ai_audio_player_control_code_pause后触发
    tx_ai_audio_player_state_continue   = 5,            //继续播放: 继续播放, 调用tx_ai_audio_player_control_code_resume后触发
    tx_ai_audio_player_state_err        = 6,            //播放错误: 包括无法下载url，资源异常等
    tx_ai_audio_player_state_seek       = 7,            //通知sdk播放器进行了定位操作。要使用tx_ai_audio_player_statechangeEx接口，data给offset的值（秒）
};

/**
 * 播放资源类型
 */
enum ai_audio_content_type {
    ai_audio_content_type_url       = 0,
    ai_audio_content_type_text      = 1,
    ai_audio_content_type_tts       = 2,
    ai_audio_content_type_file      = 3,
    ai_audio_content_type_location  = 4,    //位置信息
    ai_audio_content_type_other     = 99,
};

/**
 * 播放器类型
 */
enum ai_audio_player_type {
    ai_audio_player_tts             = 0,
    ai_audio_player_res             = 1,
};

/**
 * call tx_ai_audio_player.create返回playerId 用于控制播放器
 * call tx_ai_audio_player.control(pid, tx_ai_audio_player_control_code_start, 0); 开始播放， 播放器等待 play_url 或tts_push_start. 其间收到其他类型资源忽略
 * call tx_ai_audio_player.play_res(content, type); type: url, localfile, android_assert, etc.
 * callback tx_ai_audio_player_statechange(pid, tx_ai_audio_player_state_start); 播放开始(包括下载开始)
 * ...
 * ...
 * ...
 * callback tx_ai_audio_player_statechange(pid, tx_ai_audio_player_state_complate); 播放结束
 * call tx_ai_audio_player.play_res(content, type); type: url, localfile, android_assert, etc.
 * ...
 * ...
 * ...
 */

//播放器回调
typedef struct _tx_ai_audio_player
{
    /**
      * 创建播放器
      * @param type 播放器类型，请参考 ai_audio_player_type
      * @return 播放器标识 pid
      */
    int (*create)(int type);
    /**
      * 销毁播放器
      * @param pid 播放器标识
      */
    void (*destory)(int pid);
    /**
      * 播放器控制
      *
      * @param pid      播放器标识
      * @param ctrlcode 控制码，请参考tx_ai_audio_player_control_code
      * @param value    控制值
      */
    void (*controll)(int pid, const int ctrlcode, int value);
    /**
      * 播放资源
      *
      * @param pid      播放器标识
      * @param playId   播放资源标识
      * @param type     播放资源类型，请参考ai_audio_content_type
      * @param content  播放资源内容
      */
    void (*play_res)(int pid, const char *playId, int type, const char *content);
    /**
      * TTS流准备开始推送
      *
      * @param pid       播放器标识
      * @param sample    opus采样率
      * @param channel   声道
      * @param pcmSample pcm采样率
      * @param playId    该条TTS资源对应的资源Id
      */
    void (*tts_push_start)(int pid, int sample, int channel, int pcmSample, const char* playId);
    /**
      * TTS流推送中
      *
      * @param pid       播放器标识
      * @param seq       序列号
      * @param data      TTS数据流
      * @param len       TTS数据长度
      * @param playId    该条TTS资源对应的资源Id
      */
    void (*tts_push)(int pid, int seq, const char * data, int len, const char* playId);
    /**
      * TTS流推送结束
      *
      * @param pid       播放器标识
      * @param playId    该条TTS资源对应的资源Id
      **/
    void (*tts_push_end)(int pid, const char* playId);
    /**
      * 播放资源扩展回调
      *
      * @param pid      播放器标识
      * @param playId   播放资源标识
      * @param type     播放资源类型，请参考ai_audio_content_type
      * @param content  播放资源内容
      * @param offset
      */
    void (*play_res_ex)(int pid, const char *playId, int type, const char *content, unsigned int offset);
    
    /**
      * 播放控制扩展回调
      * @param pid      播放器标识
      * @param ctrlcode 控制码，请参考tx_ai_audio_player_control_code
      * @param value    控制值
      */
    void (*controll_ex)(int pid, const int ctrlCode, int value);
} tx_ai_audio_player;

// 播放器状态回调
typedef struct _tx_ai_audio_player_event
{
    void (*on_play)(int pid, const char* playId, bool fromUser); // fromUser 表示为用户主动操作导致切歌，如果这个时候UI不在前台，需要显示出来
    void (*on_resume)(int pid);
    void (*on_pause)(int pid);
    void (*on_stop)(int pid);
    void (*on_set_play_mode)(int playMode);
    void (*on_shared)(const char* playId, unsigned long long to);
    void (*on_keep)(const char* playId);
    void (*on_un_keep)(const char* playId);
    void (*on_delete)(int pid, const char* playId);
} tx_ai_audio_player_event;

/**
 * 接口说明：同步音乐播放状态，在音乐播放状态变化时调用
 * 参数说明：pid 播放器进程id
 * 参数说明：state 播放器状态tx_ai_audio_player_state
 * 返回值  ：错误码（见全局错误码表）
 */
SDK_API int tx_ai_audio_player_statechange(int pid, int state);
/**
 * 接口说明：同tx_ai_audio_player_statechange，目前添加了tx_ai_audio_player_state_seekTo的支持
 * 参数说明：pid 播放器进程id
 * 参数说明：state 播放器状态tx_ai_audio_player_state
 * 参数说  ：data 额外参数。当state==tx_ai_audio_player_state_seekTo时，data为定位位置的值（字符串表示的时间，秒为单位）
 * 返回值  ：错误码（见全局错误码表）
 */
SDK_API int tx_ai_audio_player_statechangeEx(int pid, int state, const char* data);

CXX_EXTERN_END

#endif // __TX_AI_AUDIO_PLAYER_H__
