/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_MUTEX_H_
#define _PTP_MUTEX_H_

#include "ptpConstants.h"
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mutex operation result
 */
typedef enum {
    eMutexOk,
    eMutexFail
} MutexResult;

/**
 * @brief Recursive mutex implementation
 */
typedef struct ptpMutex_t* PtpMutexRef;

typedef struct ptpMutex_t {
    pthread_mutex_t _mutex;
    Boolean _free;
#if !defined(PTHREAD_MUTEX_RECURSIVE)
    int32_t _threadId32;
    int _lockCounter;
#endif
} ptpMutex_t;

/**
 * @brief Allocate new mutex object or configure an existing one
 * @param mutex reference to existing mutex object or NULL if such object has to be created
 * @return reference to mutex object
 */
PtpMutexRef ptpMutexCreate(PtpMutexRef mutex);

/**
 * @brief Destroy mutex object allocated via ptpMutexCreate()
 * @param mutex reference to mutex object
 */
void ptpMutexDestroy(PtpMutexRef mutex);

/**
 * @brief Lock mutex
 * @param mutex reference to mutex object
 * @return operation result
 */
MutexResult ptpMutexLock(PtpMutexRef mutex);

/**
 * @brief Unlock mutex
 * @param mutex reference to mutex object
 * @return operation result
 */
MutexResult ptpMutexUnlock(PtpMutexRef mutex);

/**
 * @brief Try to lock mutex if possible
 * @param mutex reference to mutex object
 * @return operation result
 */
MutexResult ptpMutexTrylock(PtpMutexRef mutex);

#ifdef __cplusplus
}
#endif

#endif // _PTP_MUTEX_H_
