#ifndef DOUG_OS_RECOGNIZERTYPES_H
#define DOUG_OS_RECOGNIZERTYPES_H

#include <string>
#define RECOGNIZER_FRAME_SIZE 320

enum RecognizerResultStatus
{
    SUCCESS,NETWORKING_ERROR,NETWORKING_TIMEOUT,NONE,HAVENOT_STARTED,INTERNAL_ERROR
};

enum RecognizerStartResult
{
    START_SUCCESS,ALREADY_STARTED,UUID_INVALID
};

/*
 * 语音识别领域
 */
enum RecognizerDomain
{
	DOMAIN_MUSIC = 0,	// default music
	DOMAIN_CONTROL_TTS, // 1 控制指令 等待播控 tts url
	DOMAIN_OTHER		// 2 暂时没用
};

enum TaskStatus {
	TASK_END = 0,	// 本輪對話結束 default
	TASK_WAITING	// 等待用戶回答  1
};

/*
 * 语音识别alink_status枚举，与本地提示音相关
 */
enum RecognizerAlinkStatus
{
    RECOGNIZER_ALINK_STATUS_SUCCESS = 0,             // 1000
    RECOGNIZER_ALINK_STATUS_NO_SEARCHING_RESULT,     //8011, 8013   “抱歉，没有搜索到您要的内容”
    RECOGNIZER_ALINK_STATUS_NOT_CLEAR,               //8012         “没听清，请再试一次”
    RECOGNIZER_ALINK_STATUS_SERVER_ERROR,            //8009, 8010   “服务忙，请稍后再试”
    RECOGNIZER_ALINK_STATUS_UNEXPECTED               //暂时不用提示音
};

struct RecognizerResult
{
    RecognizerResultStatus status;
    int alinkStatus;	// 原来是透传服务端返回的状态码，改成包装后的状态值 ，RecognizerAlinkStatus类型， 从rtos同步 2016/12/16
    					// 原来返回的是alink错误码 + 透传服务端应答码
    					// enum ALINK_CODE
						//{
						//	ALINK_OK = 0,
						//	ALINK_ERR_MEM,	//1
						//	ALINK_ERR_JSON,	//2
						//	ALINK_ERR_WS_NULL,	//3
						//	ALINK_ERR_FAIL,		// 4
						//	ALINK_SERVICE_FAIL,	// 5
						//	ALINK_FAIL_NUM	//6
						//};

    int domain;			// RecognizerDomain 类型
    int task_status;	// 远场多轮对话是否结束标记位 TaskStatus类型
    int should_restore_player_status; //控制播放器是否处理完语音识别事件之后恢复之前的状态，0是不恢复；1是恢复。   从rtos同步  2016/12/16
    int no_wait_tts;	// 2016/12/21  临时增加,用于解决"继续播放",需要等5秒才会继续播放问题
    					// 目前只在Music领域 resume意图为1,其他情况都为默认值0
    std::string asrResult;
    std::string uuid;
    std::string extraMsg;
};

#endif
