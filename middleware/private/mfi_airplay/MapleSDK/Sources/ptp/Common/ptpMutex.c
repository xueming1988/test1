/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpMutex.h"
#include "AtomicUtils.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !TARGET_OS_BRIDGECO
#include <pthread.h>
#endif

#define THREAD_ID_HASH32(threadID) ((uint32_t)(threadID))

PtpMutexRef ptpMutexCreate(PtpMutexRef mutex)
{
    int lock_c;

    if (mutex == NULL) {
        if ((mutex = malloc(sizeof(struct ptpMutex_t))) == NULL)
            return NULL;
        mutex->_free = true;
    } else {
        mutex->_free = false;
    }

#if defined(PTHREAD_MUTEX_RECURSIVE)
    pthread_mutexattr_t _mta;

    pthread_mutexattr_init(&_mta);
    pthread_mutexattr_settype(&_mta, PTHREAD_MUTEX_RECURSIVE);
    lock_c = pthread_mutex_init(&mutex->_mutex, &_mta);
    pthread_mutexattr_destroy(&_mta);
#else
    mutex->_threadId32 = 0;
    mutex->_lockCounter = 0;
    lock_c = pthread_mutex_init(&mutex->_mutex, NULL);
#endif
    if (lock_c != 0) {
        fprintf(stderr, "PtpMutex initialization failed - %s", strerror(errno));
        if (mutex->_free) {
            free(mutex);
        }
        return NULL;
    }

    return mutex;
}

void ptpMutexDestroy(PtpMutexRef mutex)
{
    assert(mutex);
    pthread_mutex_destroy(&(mutex->_mutex));
    if (mutex->_free) {
        free(mutex);
    }
}

MutexResult ptpMutexLock(PtpMutexRef mutex)
{
    int lock_c;
    assert(mutex);

#if !defined(PTHREAD_MUTEX_RECURSIVE)
    // get id of our thread
    pthread_t self = pthread_self();
    int32_t self32 = THREAD_ID_HASH32(self);

    // was our thread locked before?
    if (atomic_val_compare_and_swap_32(&mutex->_threadId32, self32, self32) == self32) {

        // then increase lock counter and leave
        mutex->_lockCounter++;
        return eMutexOk;
    }
#endif

    lock_c = pthread_mutex_lock(&(mutex->_mutex));

    if (lock_c != 0) {

        fprintf(stderr, "PtpMutex: lock failed %d\n", lock_c);
        return eMutexFail;
    }

#if !defined(PTHREAD_MUTEX_RECURSIVE)
    // set the thread id
    atomic_val_compare_and_swap_32(&mutex->_threadId32, 0, self32);
#endif

    return eMutexOk;
}

MutexResult ptpMutexTrylock(PtpMutexRef mutex)
{
    int lock_c;
    assert(mutex);

    lock_c = pthread_mutex_trylock(&(mutex->_mutex));
    return (lock_c != 0 ? eMutexFail : eMutexOk);
}

MutexResult ptpMutexUnlock(PtpMutexRef mutex)
{
    int lock_c;

    assert(mutex);

#if !defined(PTHREAD_MUTEX_RECURSIVE)
    // get id of our thread
    pthread_t self = pthread_self();
    int32_t self32 = THREAD_ID_HASH32(self);

    // was our thread locked?
    if (atomic_val_compare_and_swap_32(&mutex->_threadId32, self32, self32) == self32) {

        // then decrease lock counter and leave if it was not zero already
        if (mutex->_lockCounter) {
            mutex->_lockCounter--;
            return eMutexOk;
        }
    }

    // clear the thread id
    atomic_val_compare_and_swap_32(&mutex->_threadId32, self32, 0);
#endif

    // we can release the mutex
    lock_c = pthread_mutex_unlock(&(mutex->_mutex));

    if (lock_c != 0) {
        fprintf(stderr, "PtpMutex: unlock failed %d\n", lock_c);
        return eMutexFail;
    }
    return eMutexOk;
}
