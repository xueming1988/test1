#ifndef __TX_CLOUD_AUDIO_H__
#define __TX_CLOUD_AUDIO_H__


#include "TXCAudioType.h"


CXX_EXTERN_BEGIN

//语音云服务回调
typedef struct _txca_callback
{
    void (*on_request_callback)(const char* voice_id, txca_event event, char* state_info, char* extend_info, unsigned int extend_info_len);       // 会话请求相关回调
} TXCA_CALLBACK;


/**
 * 接口说明：Start AI Audio相关服务，该服务需要登录成功后才能调用，否则会有错误码返回
 * 参数说明：callback AIAudio服务回调接口
 * 返回值  ：错误码（见全局错误码表）
 */
SDK_API int txca_service_start(TXCA_CALLBACK* callback, TXCA_PARAM_DEVICE* device, TXCA_PARAM_ACCOUNT* account);

/**
* 接口说明：Stop AI Audio相关服务
* 返回值  ：错误码（见全局错误码表）
*/
SDK_API int txca_service_stop();

/**
* 接口说明：开始会话请求(同时只会有一个请求)
* 返回值  ：错误码（见全局错误码表）
*/
SDK_API int txca_request(char* voice_id, txca_chat_type type, const char* chat_data, unsigned int char_data_len, TXCA_PARAM_CONTEXT* context);

/**
* 接口说明：cancel会话请求
* 返回值  ：错误码（见全局错误码表）
*/
SDK_API int txca_request_cancel(const char* voice_id);

/**
* 接口说明：repory play state
* 返回值  ：错误码（见全局错误码表）
*/
SDK_API int txca_report_state(TXCA_PARAM_STATE* state);

///////////////////////////////////////////////////

CXX_EXTERN_END

#endif // __TX_CLOUD_AUDIO_H__
