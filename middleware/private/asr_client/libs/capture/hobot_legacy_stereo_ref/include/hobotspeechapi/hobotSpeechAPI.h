//
// Created by 余泓 on 2017/7/19.
//
// Copyright (c) 2016 - 2022. Horizon Robotics. All rights reserved.
//
// The material in this file is confidential and contains trade secrets
// of Horizon Robotics Inc. This is proprietary information owned by
// Horizon Robotics Inc. No part of this work may be disclosed,
// reproduced, copied, transmitted, or used in any way for any purpose,
// without the express written permission of Horizon Robotics Inc.
//
#include "hobotdefine.h"

#ifndef HOBOTSPEECHAPI_H
#define HOBOTSPEECHAPI_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 获取SDK的版本号，包含语音前端和后端的版本信息
 * @return 版本号
 * @note 例如 1.0.1.11.044 前三位是SDK构建版本号 第四位语音前端算法，第五位是语音后端算法的版本
 */
const char *GetVersion();

/**
 * @brief 获取当前授权状态，包括授权起止时间，授权对象，授权功能等
 * @param licensePath 存放授权文件路径
 * @return 授权状态，失败返回错误信息
 * @todo 这个需要跟图像一起商量如何使用，暂不实现
 */
const char *GetLicenseInfo(const char *licensePath);

/**
 * @brief 设置log级别
 * @return 成功返回HOBOT_OK，失败返回错误码
 * @note 此接口只对内部开放
 */
HOBOT_ERROR_CODE ConfigLogLevel(HOBOT_LOG_LEVEL level);

/**
 * @brief 获取当前log级别
 * @return log级别
 * @note 此接口只对内部开放
 */
HOBOT_LOG_LEVEL CheckLogLevel();

/**
 * 初始化操作这里只初始化前端 以及语音后端的模型地址等
 * @param licensePath     证书文件路径
 * @param actvieBuff      激活信息
 * @param resPath         资源文件地址
 * @return
 */
HOBOT_ERROR_CODE Init(char *licensePath, char *activeBuff, const char *resPath);
/**
 * 创建服务
 * @param graphName
 * @param speechMode
 * @return
 */
HobotSpeechHandler CreateSpeechEngine(char *graphName, HOBOT_SPEECH_MODE speechMode);

/**
 * @brief 开启语音引擎，进行唤醒和识别服务
 * @param hanler    相对的引擎类别
 * @return
 */
int StartEngine(HobotSpeechHandler handler);

/**
 * 停止引擎识别
 * @param hanler
 * @return
 * @todo 暂时不用使用，可以使用{@CancleEngine}来代替
 */
int StopEngine(HobotSpeechHandler handler);

/**
 * 设置语音识别的参数
 * @param param_type            参数类型
 * @param value_size            参数大小
 * @param value                 参数的值
 * @return
 */
HOBOT_ERROR_CODE SetParams(HOBOT_SPEECH_PARAM_TYPE param_type, unsigned long value_size,
                           void *value);

/**
 *
 * @brief 取消识别 会停止识别引擎
 * @param handler
 * @return
 */
int CancelEngine(HobotSpeechHandler handler);

/**
 * 停止引擎识别
 * @param hanler
 * @return
 * @todo 暂时不用使用，可以使用{@CancleEngine}来代替
 */
int FinishEngine(HobotSpeechHandler handler);

int SyncFeedAudioData(char *audio_data, int32_t audio_len);

/**
 * @brief 销毁引擎
 * @param handler
 * @return
 */
int DestoryEngine(HobotSpeechHandler handler);

/**
 * @brief 设置Asr状态,需要上层应用调度,唤醒时激活，送完云端数据后，需要重新设为不激活状态
 * @param active_asr 0代表不激活Asr，1代表激活Asr
 * @return
 */
int SetAsrState(bool active_asr);

/**
 * @brief 触发REF信号与MIC信号的赔偿
 * @return
 */
int TrigerAudioCompensate();

/**
 * @brief 切换配置文件, 需要上层应用调用
 * @param config 配置文件的名字
 * @return
 */
HOBOT_ERROR_CODE SwitchConfig(const char *config);

/**
 *  @brief 调用之前一定销毁所有创建出来的语音句柄
 * @return
 */
int Finalize();

/**
 *  @brief 唤醒时调用此方法，刷新状态
 *  @return
 */
int FlushState();

#ifdef __cplusplus
}
#endif
#endif // HOBOTSPEECHAPI_H
