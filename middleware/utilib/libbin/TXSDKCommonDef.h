#ifndef __TX_SDK_COMMON_DEF_H__
#define __TX_SDK_COMMON_DEF_H__

#define SDK_API                     __attribute__((visibility("default")))

#ifndef __cplusplus
#   define bool                     _Bool
#   define true                     1
#   define false                    0
#   define CXX_EXTERN_BEGIN
#   define CXX_EXTERN_END
#   define C_EXTERN                 extern
#else
#   define _Bool                    bool
#   define CXX_EXTERN_BEGIN         extern "C" {
#   define CXX_EXTERN_END           }
#   define C_EXTERN
#endif


CXX_EXTERN_BEGIN

//全局错误码表
enum error_code
{
    err_null                                = 0x00000000,       //success
    err_failed                              = 0x00000001,       //failed
    err_unknown                             = 0x00000002,       //未知错误
    err_invalid_param                       = 0x00000003,       //参数非法
    err_buffer_notenough                    = 0x00000004,       //buffer长度不足
    err_mem_alloc                           = 0x00000005,       //分配内存失败
    err_internal                            = 0x00000006,       //内部错误
    err_device_inited                       = 0x00000007,       //设备已经初始化过了
    err_av_service_started                  = 0x00000008,       //av_service已经启动了
    err_invalid_device_info                 = 0x00000009,       //非法的设备信息
    err_invalid_serial_number               = 0x0000000A,       //(10)      非法的设备序列号
    err_invalid_fs_handler                  = 0x0000000B,       //(11)      非法的读写回调
    err_invalid_device_notify               = 0x0000000C,       //(12)      非法的设备通知回调
    err_invalid_av_callback                 = 0x0000000D,       //(13)      非法的音视频回调
    err_invalid_system_path                 = 0x0000000E,       //(14)      非法的system_path
    err_invalid_app_path                    = 0x0000000F,       //(15)      非法的app_path
    err_invalid_temp_path                   = 0x00000010,       //(16)      非法的temp_path
    err_not_impl                            = 0x00000011,       //(17)      未实现
    err_fetching                            = 0x00000012,       //(18)      正在向后台获取数据中
    err_fetching_buff_not_enough            = 0x00000013,       //(19)      正在向后台获取数据中 & buffer长度不足
    err_off_line                            = 0x00000014,       //(20)      当前设备处于离线状态
    err_invalid_device_name                 = 0x00000015,       //(21)      设备名没填，或者长度超过32byte
    err_invalid_os_platform                 = 0x00000016,       //(22)      系统信息没填，或者长度超过32byte
    err_invalid_license                     = 0x00000017,       //(23)      license没填，或者长度超过150byte
    err_invalid_server_pub_key              = 0x00000018,       //(24)      server_pub_key没填，或者长度超过120byte
    err_invalid_product_version             = 0x00000019,       //(25)      product_version非法
    err_invalid_product_id                  = 0x0000001A,       //(26)      product_id非法
    err_connect_failed                      = 0x0000001B,       //socket connect失败
    err_call_too_frequently                 = 0x0000001C,       //(28)      调用的太频繁了
    err_sys_path_access_permission          = 0x0000001D,       //(29)      system_path没有读写权限
    err_invalid_network_type				= 0x0000001E,		//(30)		初始化时传入的网络类型非法
    err_invalid_run_mode					= 0x0000001F,		//(31)      初始化时传入的SDK运行模式非法
    err_lanav_service_started               = 0x00000020,       //(32)		lanav_service已经启动了
    err_invalid_lanav_callback              = 0x00000021,       //(33)      非法的局域网音视频回调

    err_login_failed                        = 0x00010001,       //(65537)   登录失败
    err_login_invalid_deviceinfo            = 0x00010002,       //(65538)   设备信息非法
    err_login_connect_failed                = 0x00010003,       //(65539)   连接Server失败
    err_login_timeout                       = 0x00010004,       //(65540)   登录超时
    err_login_eraseinfo                     = 0x00010005,       //(65541)   擦除设备信息
    err_login_servererror                   = 0x00010006,       //(65542)   登录Server返回错误
 
    err_msg_sendfailed                      = 0x00020001,       //(131073)  消息发送失败
    err_msg_sendtimeout                     = 0x00020002,       //(131074)  消息发送超时
 
    err_av_unlogin                          = 0x00030001,       //(196609)  未登录的情况下启动音视频服务
 
    err_ft_transfer_failed                  = 0x00040001,       //(262145)  文件传输(发送/接收)失败
    err_ft_file_too_large                   = 0x00040002,       //(262146)  发送的文件太大
    err_ft_upload_failed_key_empty          = 0x00040003,       // 小文件通道上传完毕后，后台没有返回key

    err_ai_audio_parse_req                  = 10000,       // 请求解包错误
    err_ai_audio_empty_voice_data           = 10001,       // 空的语音数据
    err_ai_audio_voice_to_text              = 10002,       // 语音转文本失败或结果为空
    err_ai_audio_text_analysis              = 10003,       // 语义分析失败
    err_ai_audio_text_to_voice              = 10004,       // 文本转语音失败
    err_ai_audio_sp_error                   = 10005,       // 资源类场景错误
    err_ai_audio_invalid_devi               = 10006,       // 被限制接入的设备
    err_ai_audio_vad_too_long               = 10007,       // 语音上行数据时间超长，只能30s
    err_ai_audio_vad_time_out               = 10008,       // 语音上行数据,5s没说话，静音检测超时
    err_ai_audio_change_scene               = 10009,       // 多轮对话中场景切换了
    err_ai_audio_not_match_skill            = 10010,       // 没有命中任何skill
    err_ai_audio_not_find_loginInfo         = 10011,       // 没有找到授权的登录态信息
    
    err_ai_audio_music_not_login_user       = 10012,       // 未登录用户，点播歌曲
    err_ai_audio_music_ask_vip_content      = 10013,       // 登录用户非会员触发付费内容
    err_ai_audio_music_not_find             = 10014,       // 某歌曲没有找到
    err_ai_audio_music_not_has_law          = 10015,       // 某歌曲没有版权
    err_ai_audio_music_not_find_result      = 10016,       // 找不到音乐资源

    err_ai_audio_qqmsg_proxy_err            = 10017,        //开启消息代收错误码
    
    err_ai_audio_system                     = 11000,       // 后台系统错误
    err_ai_audio_no_such_person             = 11030,        // 没有找到联系人，不是好友或代收好友
    err_ai_audio_is_not_understand          = 20001,        // 没有明白你的意思
    //...
};

CXX_EXTERN_END


#endif // __TX_SDK_COMMON_DEF_H__
