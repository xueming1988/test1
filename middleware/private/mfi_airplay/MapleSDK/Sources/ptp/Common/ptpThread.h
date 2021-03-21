/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_THREAD_H_
#define _PTP_THREAD_H_

#include "ptpConstants.h"

#if !TARGET_OS_BRIDGECO
#include <pthread.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    threadExitOk,
    threadExitErr
} ThreadExitCode;

/**
 * @brief PTP thread function prototype
 */
typedef ThreadExitCode (*ThreadFunction)(void*);

/**
 * @brief PTP thread handle
 */
typedef struct ptpThread_t* PtpThreadRef;

/**
 * @brief allocates memory for a single instance of PtpThread object
 * @return dynamically allocated PtpThread object
 */
PtpThreadRef ptpThreadCreate(void);

/**
 * @brief Destroy PtpThread object
 * @param thread reference to PtpThread object
 */
void ptpThreadDestroy(PtpThreadRef thread);

/**
 * @brief Check if the thread specified by given PtpThread object exists
 * @param thread reference to PtpThread object
 * @return true if the thread object exists, false otherwise
 */
Boolean ptpThreadExists(PtpThreadRef thread);

/**
 * @brief Start a new thread with the specified callback function
 * @param thread reference to PtpThread object
 * @param function callback function that is executed in the new thread
 * @param arg user specified argument passed to callback function
 * @return true if the thread was started successfully
 */
Boolean ptpThreadStart(PtpThreadRef thread, ThreadFunction function, void* arg);

/**
 * @brief Join given thread and wait until the thread is finished
 * @param thread reference to PtpThread object
 * @param exitCode [out] variable where the exit code of thread is stored
 * @return true if the thread was successfully joined
 */
Boolean ptpThreadJoin(PtpThreadRef thread, ThreadExitCode* exitCode);

/**
 * @brief pauses execution of current thread for a given amount of time
 * @param micros the number of microseconds to sleep
 * @return the actual number of microseconds the current thread for suspended for
 */
unsigned long ptpThreadSleep(uint32_t micros);

#ifdef __cplusplus
}
#endif

#endif
