/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpThread.h"
#include "ptpLog.h"
#include "ptpMutex.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !TARGET_OS_BRIDGECO
#include <signal.h>
#endif

struct ptpThreadArg_t {
    ThreadFunction _threadFunc;
    void* _threadArg;
    ThreadExitCode _threadRet;
};

struct ptpThread_t {
    struct ptpThreadArg_t* _argInner;
    pthread_t _threadId;
};

static void* OSThreadCallback(void* input);

static void* OSThreadCallback(void* input)
{
    struct ptpThreadArg_t* arg = (struct ptpThreadArg_t*)input;
    arg->_threadRet = arg->_threadFunc(arg->_threadArg);
    return 0;
}

PtpThreadRef ptpThreadCreate()
{
    PtpThreadRef thread = malloc(sizeof(struct ptpThread_t));

    if (!thread)
        return NULL;

    thread->_threadId = 0;
    thread->_argInner = 0;
    return thread;
}

void ptpThreadDestroy(PtpThreadRef thread)
{
    if (!thread)
        return;

    free(thread);
}

Boolean ptpThreadExists(PtpThreadRef thread)
{
    return (thread != NULL);
}

Boolean ptpThreadStart(PtpThreadRef thread, ThreadFunction function, void* arg)
{
#if !TARGET_OS_BRIDGECO
    sigset_t set;
    sigset_t oset;
#endif

    int err;
    assert(thread);

    thread->_argInner = malloc(sizeof(struct ptpThreadArg_t));
    thread->_argInner->_threadFunc = function;
    thread->_argInner->_threadArg = arg;

#if !TARGET_OS_BRIDGECO
    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    err = pthread_sigmask(SIG_BLOCK, &set, &oset);

    if (err != 0) {
        PTP_LOG_ERROR("Add timer pthread_sigmask(SIG_BLOCK ...)");
        return false;
    }
#endif

    err = pthread_create(&thread->_threadId, NULL, OSThreadCallback, thread->_argInner);

    if (err != 0)
        return false;

#if !TARGET_OS_BRIDGECO
    sigdelset(&oset, SIGALRM);
    err = pthread_sigmask(SIG_SETMASK, &oset, NULL);

    if (err != 0) {
        PTP_LOG_ERROR("Add timer pthread_sigmask(SIG_SETMASK ...)");
        return false;
    }
#endif

    return true;
}

Boolean ptpThreadJoin(PtpThreadRef thread, ThreadExitCode* exitCode)
{
    int err;
    assert(thread);
    err = pthread_join(thread->_threadId, NULL);

    if (err != 0)
        return false;

    if (exitCode) {
        *exitCode = thread->_argInner->_threadRet;
    }

    free(thread->_argInner);
    return true;
}

unsigned long ptpThreadSleep(uint32_t micros)
{
#if TARGET_OS_BRIDGECO
    usleep(micros);
    return micros;
#else
    struct timespec req;
    struct timespec rem;
    req.tv_sec = micros / 1000000;
    req.tv_nsec = micros % 1000000 * 1000;
    int ret = nanosleep(&req, &rem);

    while (ret == -1 && errno == EINTR) {
        req = rem;
        ret = nanosleep(&req, &rem);
    }

    if (ret == -1) {
        fprintf(stderr, "Error calling nanosleep: %s\n", strerror(errno));
        assert(0);
    }

    return micros;
#endif
}
