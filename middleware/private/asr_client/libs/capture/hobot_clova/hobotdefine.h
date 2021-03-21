//
// Copyright (c) 2016 - 2022. Horizon Robotics. All rights reserved.
//
// The material in this file is confidential and contains trade secrets
// of Horizon Robotics Inc. This is proprietary information owned by
// Horizon Robotics Inc. No part of this work may be disclosed,
// reproduced, copied, transmitted, or used in any way for any purpose,
// without the express written permission of Horizon Robotics Inc.
//

#ifndef HOBOTSPEECHAPI_HOBOTDEFINE_H
#define HOBOTSPEECHAPI_HOBOTDEFINE_H

#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

/**
 * @brief hobot speech handler 用来区分不同的语音业务比如ASR有ASR的handler 而WAKE 有WAKE的handler
 */
typedef long HobotSpeechHandler;

/**
 * @brief 语音模式
 * @note 目前只有命令词识别和唤醒模式，后面新的模式会逐渐加入
 */
typedef enum HOBOT_SPEECH_MODE {
    ASR = 0x11,
    WAKE = 0x14,
} HOBOT_SPEECH_MODE;

/**
 * @brief 错误码
 * @note 现阶段所有错误先返回HOBOT_ERROR，会逐渐完善
 */
typedef enum HOBOT_ERROR_CODE {
    HOBOT_OK = 0,
    HOBOT_ERROR = 1,
    HOBOT_ERROR_INVALID_PARAM,
} HOBOT_ERROR_CODE;

/**
 * @brief 音频位数
 * @note 目前支持16位、24位、32位，默认16位
 */
typedef enum HOBOT_SPEECH_ECODING {
    PCM_16BIT = 0,
    PCM_24BIT = 1,
    PCM_32BIT = 2,
} HOBOT_SPEECH_ECODING;

/**
 * @brief log级别
 * @note HOBOT_LOG_NULL用来做错误判别
 *  此部分只对内部开放
 */
typedef enum HOBOT_LOG_LEVEL {
    HOBOT_LOG_VERBOSE,
    HOBOT_LOG_DEBUG,
    HOBOT_LOG_INFO,
    HOBOT_LOG_WARN,
    HOBOT_LOG_ERROR,
    HOBOT_LOG_ASSERT,
    HOBOT_LOG_NULL
} HOBOT_LOG_LEVEL;

/**
 * @brief 语音识别状态类型
 */
typedef enum HOBOT_REC_EVENT_TYPE {
    VOICE_START = 0,
    VOICE_END,
    VOICE_TOO_LONG,
    ENGINE_STARTED,
    ENGINE_STOPPED
} HOBOT_REC_EVENT_TYPE;

/**
 *  @brief SDK 参数设置
 */
typedef enum HOBOT_SPEECH_PARAM_TYPE {
    HOBOT_SPEECH_PARAM_AUDIO_DATA_CB,    // 设置音频读取回调
    HOBOT_SPEECH_PARAM_AUDIO_ENHANCE_CB, // 设置获取增强后的音频接口
    HOBOT_SPEECH_PARAM_CHANNEL_NUM,      // 设置mic个数
    HOBOT_SPEECH_PARAM_RESULT_CB,        // 设置识别结果回调接口
    HOBOT_SPEECH_PARAM_EVENT_CB,         // 设置识别事件回调接口
    HOBOT_SPEECH_PARAM_AUTH_CB,          // 设置认证结果回调接口
    HOBOT_SPEECH_PARAM_USE_DOA, // 设置DOA值，在命令词识别中可以设置自动生效
    HOBOT_SPEECH_PARAM_FRONT_BF_AZIMUTH, // 使用外部波束
    HOBOT_SPEECH_PARAM_LOG_SAVE_PATH,    // 设置日志文件保存路径指到文件夹
    HOBOT_SPEECH_PARAM_UPDATE_HISF_SYNC, // 设置hisf调用同步更新, 主要用于离线测试
    HOBOT_SPEECH_PARAM_USE_EXTERNAL_DECORDER // 使用外部解码器
} HOBOT_SPEECH_PARAM_TYPE;

typedef int (*hobot_auth_callback)(int code);

typedef int32_t (*hobot_audio_data_callback)(char *audio_data, int32_t buff_len);

typedef void (*hobot_enhance_audio_data_callback)(char *enhance_audio_dadta, int32_t data_len);

typedef void (*hobot_rec_result_callback)(const char *result, bool islast);

typedef void (*hobot_rec_event_callback)(HOBOT_REC_EVENT_TYPE event_type);

#endif // HOBOTSPEECHAPI_HOBOTDEFINE_H
