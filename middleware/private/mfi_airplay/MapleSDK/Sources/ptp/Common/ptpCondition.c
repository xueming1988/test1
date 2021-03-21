/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpCondition.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !TARGET_OS_BRIDGECO
#include <pthread.h>
#endif

struct ptpCondition_t {
    pthread_cond_t _portReadySignal;
    pthread_mutex_t _portMutex;
    Boolean _conditionNotified;
};

PtpConditionRef ptpConditionCreate(void)
{
    PtpConditionRef condition = malloc(sizeof(struct ptpCondition_t));

    if (!condition)
        return NULL;

    condition->_conditionNotified = false;

    pthread_cond_init(&(condition->_portReadySignal), NULL);
    pthread_mutex_init(&(condition->_portMutex), NULL);
    return condition;
}

void ptpConditionDestroy(PtpConditionRef condition)
{
    assert(condition);
    pthread_cond_destroy(&(condition->_portReadySignal));
    pthread_mutex_destroy(&(condition->_portMutex));
    free(condition);
}

Boolean ptpConditionWait(PtpConditionRef condition)
{
    assert(condition);
    pthread_mutex_lock(&(condition->_portMutex));

    while (!condition->_conditionNotified) {
        int err = pthread_cond_wait(&(condition->_portReadySignal), &(condition->_portMutex));

        // if the timeout expired, leave
        if (err == ETIMEDOUT) {
            break;
        }
    }

    condition->_conditionNotified = false;
    pthread_mutex_unlock(&(condition->_portMutex));
    return true;
}

Boolean ptpConditionSignal(PtpConditionRef condition)
{
    assert(condition);

    pthread_mutex_lock(&(condition->_portMutex));
    condition->_conditionNotified = true;
    pthread_cond_signal(&(condition->_portReadySignal));
    pthread_mutex_unlock(&(condition->_portMutex));
    return true;
}
