/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_THREAD_SAFE_CALLBACK_H_
#define _PTP_THREAD_SAFE_CALLBACK_H_

#include "ptpMutex.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Thread safe callback prototype
 */
typedef void (*ThreadSafeCallback)(void*);

/**
 * @brief Calls provided callback function in a thread-safe way
 * @param callback callback function safely called without a risk of race condition
 * @param arg argument passed to callback function
 */
void ptpThreadSafeCallback(ThreadSafeCallback callback, void* arg);

#ifdef __cplusplus
}
#endif

#endif // _PTP_THREAD_SAFE_CALLBACK_H_
