//
//  asr_core.h
//  BDSASRCore
//
//  Created by baidu on 10/24/14.
//  Copyright (c) 2014 baidu. All rights reserved.
//

#ifndef __BDSASRCore__asr_core__
#define __BDSASRCore__asr_core__

#ifdef __WINDOWS__
#define DLL_IMPLEMENT

#ifdef DLL_IMPLEMENT
#define DLL_API __declspec(dllexport)
#else
#define DLL_API __declspec(dllimport)
#endif

#else
#define DLL_API
#endif

#include "asr_define.h"

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus
    
/**
 * @brief start ASR
 *
 * @return errCode
 */
DLL_API ASR_CORE_ERROR_NUMBER asr_core_run();
    
/**
 * @brief stop ASR
 *
 * @return errCode
 */
DLL_API ASR_CORE_ERROR_NUMBER asr_core_stop();
    
/**
 * @brief set ASR params
 *
 * @param type: key
 * @param value_size: value size 
 * @param value: value
 *
 * @return errCode
 */
DLL_API ASR_CORE_ERROR_NUMBER asr_core_set_param( ASR_PARAM_TYPE type, unsigned long value_size, void* value);
    
/**
 * @brief input audio data to synchronously recognize
 *
 * @param audio_buffer: audio data buffer
 * @param audio_buffer_len: audio data buffer length
 * @param end_flag: if value is true, indicate audio data is eof
 *
 * @return errCode
 */
DLL_API ASR_CORE_ERROR_NUMBER asr_core_synch_recog_input(const char* audio_buffer, uint32_t audio_buffer_len, int end_flag);
    
/**
 * @brief output recognition result
 *
 * @param result: recognition result
 *
 * @return errCode
 */
DLL_API RecognitionResult* asr_core_synch_recog_get_result();
    
/**
 * @brief free the resource alloc from asr_core_synch_recog_get_result
 *
 * @param result: the resource pointer
 *
 */
DLL_API void asr_core_synch_recog_free(RecognitionResult* result);

/**
 * @brief  set Log Level
 *
 * @param level 
 */
DLL_API void asr_core_set_log_level(ASR_LOG_LEVEL level);

/**
 * @brief  get SDK Version Information 
 *
 */
DLL_API const char* asr_core_get_version();
#ifdef __cplusplus
};
#endif // __cplusplus

#endif /* defined(__BDSASRCore__asr_core__) */
