#ifndef deviceCommonDef_h
#define deviceCommonDef_h

#ifndef __cplusplus
#define EXTERN_C
typedef unsigned char bool;
enum { false = 0, true = 1 };
#else
#define EXTERN_C extern "C"
#endif

#ifdef _UWP
#define DEPRECATED_API
#define DEVICE_CALL __cdecl
#if defined(DEVICE_INTERFACE_EXPORT)
#define DEVICE_API extern "C" __declspec(dllexport)
#else
#define DEVICE_API extern "C" __declspec(dllimport)
#endif
#elif defined(__APPLE__)
#define DEVICE_API __attribute__((visibility("default"))) extern "C"
#define DEVICE_CALL
#define DEPRECATED_API __attribute__((deprecated))
#elif defined(__ANDROID__) || defined(__linux__)
#if __GNUC__ >= 4
#ifdef __cplusplus
#define DEVICE_API extern "C" __attribute__((visibility("default")))
#else
#define DEVICE_API __attribute__((visibility("default")))
#endif
#else
#ifdef __cplusplus
#define DEVICE_API extern "C"
#else
#define DEVICE_API
#endif
#endif

#define DEVICE_CALL
#define DEPRECATED_API __attribute__((deprecated))
#else
#define DEVICE_API EXTERN_C __attribute__((visibility("default")))
#define DEVICE_CALL
#define DEPRECATED_API __attribute__((deprecated))
#endif

#ifndef SDK_MAIN_VERSION
#define SDK_MAIN_VERSION 9
#define SDK_SUB_VERSION 9
#define SDK_FIX_VERSION 9
#define SDK_BUILD_NUMBER 9
#endif

/**
 * @brief device global errcode define
 */
enum device_code {
    /** global error, begin */
    error_null = 0,                    /// success，成功
    error_failed = 1,                  /// failed，失败
    error_re_init = 5002,              /// re-init device xwsdk，重复初始化SDK
    error_init_path_invalid = 5003,    /// xwsdk can not access this path，路径不可达
    error_account_invalid = 5004,      /// account invalid，非法的账号信息
    error_param_invalid = 5005,        /// param invalid，参数非法
    error_sdk_uninit = 5006,           /// xwsdk not inited，SDK未初始化
    error_memory_not_enough = 5007,    /// input char[] size not enough，内存不足
    error_account_not_register = 5008, /// account register not success，登录未成功
    error_not_impl = 5009,
    error_logic = 5010,
    error_permission = 5011,
    error_sleep_mode = 5012,
    /** global error, end */

    /** login errcode */
    err_login_device_illegal = -9, /// 非法的设备，这种情况下SDK不会重试，用户需要自己uninit SDK
    err_login_invalid_sign = -5,  /// licence错误
    err_login_not_register = -17, /// 设备未预注册
    /** login errcode end */

    /** network and auth errcode */
    err_network_timeout = 33,      /// 网络超时
    err_auth_session_timeout = 35, /// 长时间未使用，登录态过期，下次请求会恢复正常
    /** network and auth end */

    /** xiaowei err, begin */
    xiaowei_err_call_too_frequently = 28, /// 调用的太频繁了

    /** xiaowei dialog error */
    xiaowei_err_ai_audio_parse_req = 10000,        /// 请求解包错误
    xiaowei_err_ai_audio_empty_voice_data = 10001, /// 空的语音数据
    xiaowei_err_ai_audio_voice_to_text = 10002,    /// 语音转文本失败或结果为空
    xiaowei_err_ai_audio_text_analysis = 10003,    /// 语义分析失败
    xiaowei_err_ai_audio_text_to_voice = 10004,    /// 文本转语音失败
    xiaowei_err_ai_audio_resource_error = 10005,   /// 资源无法获取
    xiaowei_err_ai_audio_invalid_dev = 10006,      /// 被限制接入的设备
    xiaowei_err_ai_audio_vad_too_long = 10007,     /// 语音上行数据时间超长，只能60s
    xiaowei_err_ai_audio_vad_time_out = 10008, /// 语音上行数据,5s没说话，静音检测超时
    xiaowei_err_ai_audio_change_scene = 10009,        /// 多轮对话中场景切换了
    xiaowei_err_ai_audio_not_match_skill = 10010,     /// 没有命中任何skill
    xiaowei_err_ai_audio_wait_result_timeout = 10017, /// 说完话后等待小微反应超时
    xiaowei_err_ai_audio_end_timeout = 10018,         /// 尾部静音检测超时
    xiaowei_err_ai_audio_is_not_understand = 20001, /// 没有明白你的意思，NLP工作正常，但无法理解
    xiaowei_err_ai_audio_not_support = 20002, /// 小微明白意思，但是目前不支持这个操作
    xiaowei_err_ai_audio_internal_err = 20003, /// 后台内部处理流程错误，例如走到了奇怪的分支
    xiaowei_err_ai_audio_parameter_err = 20004, /// 请求携带的参数非法
    xiaowei_err_ai_audio_auth_err = 20005,      /// 登录态错误，未授权
    xiaowei_err_ai_audio_req_logic_err = 20006, /// 请求逻辑错误，如设置一个昨天的闹钟
    xiaowei_err_ai_audio_illegal = 20007,       /// 请求不和谐，命中黄反等敏感问题
    xiaowei_err_ai_audio_too_frequently = 21031, /// 频繁调用被加入黑名单

    /** Music related */
    xiaowei_music_err_music_parameter = -100000,      /// 参数错误
    xiaowei_music_err_not_login = -100032,            /// 未登录点歌
    xiaowei_music_err_not_vip = -100033,              /// 付费歌曲，非会员不能播放
    xiaowei_music_err_not_find_singer_song = -100034, /// 以歌手-歌曲方式点歌，未找到结果
    xiaowei_music_err_singer_song_copyright = -100035, /// 因歌曲没有版权不能播放
    xiaowei_music_err_nothing_find = -100036,          /// 未查询到歌曲
    xiaowei_music_err_region = -100037,                /// 因海外地区不能播放
    xiaowei_music_err_need_buy = -100038,             /// 因歌曲为数字专辑歌曲不能播放
    xiaowei_music_err_unplayable_other = -100039,     /// 其他原因不能播放
    xiaowei_music_err_change_singer = -100040,        /// 换歌手失败
    xiaowei_music_err_account_verify = -100041,       /// 账户信息验证错误
    xiaowei_music_err_sys_parameter = -200001,        /// 参数错误
    xiaowei_music_err_sys_pre_get = -200010,          /// 预拉取请求音乐失败
    xiaowei_music_err_sys_pick_para = -200012,        /// 点播参数错误
    xiaowei_music_err_sys_pick_id_err = -200013,      /// 点播ID不正确
    xiaowei_music_err_sys_pick_failed = -200015,      /// 点播请求音乐失败
    xiaowei_music_err_sys_play_list_failed = -200017, /// 查询列表请求音乐失败
    xiaowei_music_err_sys_item_detail_para = -200021, /// 查详情参数错误
    xiaowei_music_err_sys_item_id_err = -200022,      /// 查详情ID不正确
    xiaowei_music_err_sys_play_list_no_more = -200030,  /// 查询列表没更多了
    xiaowei_music_err_sys_pre_get_parameter = -200036,  /// 预拉取参数错误
    xiaowei_music_err_sys_play_list_id = -200044,       /// 查询列表参照id不存在
    xiaowei_music_err_sys_pre_get_no_more = -200049,    /// 预拉取没更多了
    xiaowei_music_err_sys_delete_item_para = -200051,   /// 列表中删除元素参数错误
    xiaowei_music_err_sys_delete_item_failed = -200052, /// 列表中删除元素操作失败
    xiaowei_music_err_sys_no_history_list = -200053,    /// 不存在历史列表
    xiaowei_music_err_sys_clear_play_list = -200054,    /// 解绑清除数据操作失败
    xiaowei_music_err_sys_get_history_list = -200055,   /// 查询历史列表请求音乐失败
    xiaowei_music_err_sys_favorite_param = -200060,     /// 操作收藏没带res_id

    /** xiaowei err, end */
};

#endif /* deviceCommonDef_h */
