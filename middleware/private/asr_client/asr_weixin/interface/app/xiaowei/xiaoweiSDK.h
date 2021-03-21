//
//  xiaoweiSDK.h
//  iLinkSDK
//
//  Created by cppwang(王缘) on 2019/4/14.
//  Copyright © 2019年 Tencent. All rights reserved.
//

#ifndef xiaoweiSDK_h
#define xiaoweiSDK_h

#include "deviceCommonDef.h"
#include "xiaoweiAudioType.h"

CXX_EXTERN_BEGIN_DEVICE

/**
 * @brief
 * 开始小微服务，必须要在智联登录成功后调用，否则返回错误，不是初始化成功后（小微依赖device的登录态）
 * @return 错误码（见全局错误码表xiaowei_error_code）
 */
DEVICE_API int xiaowei_service_start(XIAOWEI_CALLBACK *callback, XIAOWEI_NOTIFY *notify);

/**
 * @brief 发起请求
 * @param voice_id 传入一个指针进来，发送完请求之后获得voiceID,size必须为33
 * @param type 参考XIAOWEI_CHAT_TYPE
 * @param chat_data 语音/文本data
 * @param context 请求需要的context，参考XIAOWEI_PARAM_CONTEXT定义
 * @return 错误码（见全局错误码表xiaowei_error_code）
 * @code
     //文本请求
     char voice_id[33] = {0};
     XIAOWEI_PARAM_CONTEXT context = {0};
     context.silent_timeout = 500;
     context.speak_timeout = 5000;
     context.voice_request_begin = true;
     context.voice_request_end = false;
     const char *text = "我要听林俊杰的歌";
     xiaowei_request(voice_id, xiaowei_chat_via_text, text, (uint32_t)strlen(text), &context);

     //语音请求
     char buf[1024] = your voice data;
     //第一包
     char voice_id[33] = {0};
     XIAOWEI_PARAM_CONTEXT context = {0};
     context.silent_timeout = 500;
     context.speak_timeout = 5000;
     context.voice_request_begin = true;
     context.voice_request_end = false;
     XIAOWEI_PARAM_PROPERTY property =
 xiaowei_param_local_vad;//本地静音检测，要主动发送end包，如果云端静音检测，就当收到onsilent回调之后停止发送请求
     context.request_param = property;
     xiaowei_request(voice_id, xiaowei_chat_via_voice, buf, sizeof(buf), &context);
     //第N包，从第2包开始
     context.voice_request_begin = false;
     buf更新
     xiaowei_request(voice_id, xiaowei_chat_via_voice, buf, sizeof(buf), &context);

     // 中间还有若干包

     //最后一包
     context.voice_request_end = true;
     buf更新(最后一包buf长度可以为0)
     xiaowei_request(voice_id, xiaowei_chat_via_voice, buf, sizeof(buf), &context);

 */
DEVICE_API int xiaowei_request(char *voice_id, XIAOWEI_CHAT_TYPE type, const char *chat_data,
                               unsigned int char_data_len, XIAOWEI_PARAM_CONTEXT *context);

/**
 * @brief 停止小微服务
 * @return 错误码（见全局错误码表xiaowei_error_code）
 */
DEVICE_API int xiaowei_service_stop();

/**
 * @brief 强行取消本次请求,voiceID填空会取消所有请求
 * @return 错误码（见全局错误码表xiaowei_error_code）
 */
DEVICE_API int xiaowei_request_cancel(const char *voice_id);

/**
 * @brief 设置某个技能的登录态，比如音乐技能用QQ音乐
 * @param st 完整填写
 * @return 错误码（见全局错误码表xiaowei_error_code）
 */
DEVICE_API int xiaowei_set_skill_account(char *voice_id, XIAOWEI_PARAM_LOGIN_STATUS *st);

/**
 * @brief 获取某个技能的登录态
 * @param st 填写skillID
 * @return 错误码（见全局错误码表xiaowei_error_code）
 */
DEVICE_API int xiaowei_get_skill_account(char *voice_id, XIAOWEI_PARAM_LOGIN_STATUS *st);

/**
 * @brief 上报当前的播放状态
 * @param state 当前状态
 * @return 错误码（见全局错误码表xiaowei_error_code）
 * @note
     用户应该在状态变化之后及时上报，只需要上报大资源，即打断后能够恢复的资源，例如音乐，FM。无需上报小资源，例如TTS播放。这里列举一下常用场景的上报：
     1.
 切歌：报2次，首先第一首歌abort(主动切歌，offset为abort前一瞬间的位置)或者stop(播放完毕)，然后第二首歌start(offset
 = 0)。
     2. 播放过程中唤醒了开始语音请求(这时候正常音乐是要暂停的)：上报paused
     3. 操作2点了一首新歌，并且要准备播放了：先报刚才那首暂停的歌stoped，再报新歌start
     4.
 操作2问了一下天气或者闲聊：首先无需任何上报，音箱播放天气或者闲聊的TTS，对话结束之后正常逻辑应该恢复播放音乐，然后这时候上报刚才那个歌resume。
     5. 该播放的东西播完了，无事可做：上报idle
 */
DEVICE_API int xiaowei_report_state(XIAOWEI_PARAM_STATE *state);

/**
 * @brief 发起通用的请求，后续除了播放资源之外的所有接口都会往这上面集成
 * @param voice_id
 * 请求的voiceID，用户需要维护，等待callback，将voiceID对上，以便确定是哪一次请求的回调
 * @param cmd todo
 * @param sub_cmd todo
 * @param callback
 * 真正的返回是在callback里的，每次请求都要设置，可以设置不同的callback，也可以设置一个静态的，再在callback里面自己分发
 * @return 错误码（见全局错误码表xiaowei_error_code）
 */
DEVICE_API int xiaowei_request_cmd(char *voice_id, const char *cmd, const char *sub_cmd,
                                   const char *param, XIAOWEI_ON_REQUEST_CMD_CALLBACK callback);

/**
 * @brief 设置资源的品质（如：音乐的品质）
 * @param quality 品质的值（目前用于音乐资源，0 流畅 1 标准 2 高品质 3
 * 无损）
 * @return 0成功 非0失败
 */
DEVICE_API int xiaowei_resource_set_quality(int quality);

/**
 * @brief 用于预加载播放列表，拉取更多的列表
 * @param voice_id
 * 请求的voiceID，用户需要维护，等待基础的on_request_callback，将voiceID对上，以便确定是哪一次请求的回调
 * @param skill_info 技能信息
 * @param play_id 播放资源ID
 * @param max_list_size 最多想拉多少首，实际返回可能小于该值
 * @param is_up 向上拉还是向下拉
 * @return 错误码（见全局错误码表）
 */
DEVICE_API int xiaowei_resource_get_list(char *voice_id, const XIAOWEI_PARAM_SKILL *skill_info,
                                         const char *play_id, unsigned int max_list_size,
                                         bool is_up);

/**
 * @brief 用于加载播放类详情信息
 * @param voice_id
 * 请求的voiceID，用户需要维护，等待基础的on_request_callback，将voiceID对上，以便确定是哪一次请求的回调
 * @param skill_info 技能信息
 * @param list_play_id 播放资源ID数组
 * @param list_size 资源ID数组的大小
 * @return 错误码（见全局错误码表）
 */
DEVICE_API int xiaowei_resource_get_detail_info(char *voice_id,
                                                const XIAOWEI_PARAM_SKILL *skill_info,
                                                char **list_play_id, int list_size);

/**
 * @brief 用于刷新播放列表的url
 * @param voice_id
 * 请求的voiceID，用户需要维护，等待基础的on_request_callback，将voiceID对上，以便确定是哪一次请求的回调
 * @param skill_info 技能ID，现在只有音乐
 * @param list_play_id 播放资源ID数组
 * @param list_size 资源ID数组的大小
 * @return 错误码（见全局错误码表）
 */
DEVICE_API int xiaowei_resource_refresh_list(char *voice_id, const XIAOWEI_PARAM_SKILL *skill_info,
                                             char **list_play_id, int list_size);

/**
 * @brief 用于设置某个资源是否被收藏
 * @param skill_info 技能信息
 * @param play_id 播放资源的id
 * @param is_favorite 是否要收藏 true表示收藏，false表示取消收藏
 * @return 错误码（见全局错误码表）
 */
DEVICE_API int xiaowei_resource_set_favorite(char *voice_id, const XIAOWEI_PARAM_SKILL *skill_info,
                                             const char *play_id, bool is_favorite);

/**
 * @brief 开启可见可答功能
 * @param enable 0:关闭 1:开启
 * @return 错误码（见全局错误码表）
 */
DEVICE_API int xiaowei_enable_v2a(bool enable);

/**
 * @brief 设置词表
 * @param type 0:可见可答屏幕词表 1:联系人词表
 * @param words_list 词表数组
 * @param list_size 词表数量
 * @param callback 设置词表的结果回调
 * @return 错误码（见全局错误码表）
 */
DEVICE_API int xiaowei_set_wordslist(XIAOWEI_WORDS_TYPE type, char **words_list, int list_size,
                                     XIAOWEI_SET_WORDSLIST_CALLBACK callback);

/**
 * @brief 请求指定形式的TTS，这个接口后续还会修改，
 * @return 错误码（见全局错误码表xiaowei_error_code）
 */
DEVICE_API int xiaowei_request_protocol_tts(char *voice_id, const char *remark,
                                            unsigned long long timestamp,
                                            int cmd); // TODO这个后续应该要改，功能先留在这里

/**
 * @brief
 * 设置GPS回调，设置后在发送请求时设置xiaowei_param_gps，小微则会回调on_gps_callback来获取GPS信息
 * @return 错误码（见全局错误码表xiaowei_error_code）
 */
DEVICE_API int xiaowei_set_gps_callback(XIAOWEI_GPS_CALLBACK callback);

/**
 * @brief 对某次识别结果不满意，可以将这一次voiceID上报后台
 * @param voice_id 不满意的请求voiceID
 * @return 错误码（见全局错误码表xiaowei_error_code）
 */
DEVICE_API int xiaowei_error_feed_back(const char *voice_id);

/**
 * @brief
 * 用户侧发生关键error的时候调用此接口，把问题上报，可以填写一些设备相关的日志和堆栈，一般情况下不要调用。
 * @param log {@see XIAOWEI_PARAM_LOG_REPORT}
 * @return 错误码（见全局错误码表xiaowei_error_code）
 */
DEVICE_API int xiaowei_user_report(XIAOWEI_PARAM_LOG_REPORT *log);

/**
 * @brief fetch iot设备列表，结果会异步推送
 * @return 错误码（见全局错误码表xiaowei_error_code）
 */
DEVICE_API int xiaowei_get_iot_device_list();

/**
 * @brief 设置设备的可用状态
 * @param availability 按位或，具体定义在XIAOWEI_PARAM_AVAILABILITY
 * @return 错误码（见全局错误码表xiaowei_error_code）
 */
DEVICE_API int xiaowei_set_device_availability(int availability);

CXX_EXTERN_END_DEVICE

#endif /* xiaoweiSDK_h */
