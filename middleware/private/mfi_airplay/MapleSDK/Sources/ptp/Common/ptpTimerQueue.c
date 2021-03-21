/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpTimerQueue.h"
#include "ptpList.h"
#include "ptpLog.h"
#include "ptpMemoryPool.h"
#include "ptpRefCountedObject.h"

#if !TARGET_OS_BRIDGECO
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/timex.h>
#endif

#include <assert.h>
#include <errno.h>
#include <limits.h>

#if (!defined(timespecsub))
// a helper macro to subtract two timespec arguments
#define timespecsub(a, b, result)                        \
    do {                                                 \
        (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;    \
        (result)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec; \
        if ((result)->tv_nsec < 0) {                     \
            --(result)->tv_sec;                          \
            (result)->tv_nsec += 1000000000;             \
        }                                                \
    } while (0)
#endif

// context related to every single timer execution
typedef struct {
    void* _event;
    TimerQueueHandler _callback;
    TimerQueueHandler _destroyCallback;
    int _eventType;
    struct timespec _triggerTime;
} TqAction;

typedef TqAction* TqActionRef;

void ptpTqActionCtor(TqActionRef ctx);
void ptpTqActionDtor(TqActionRef ctx);
void* ptpRefTqActionAllocateMemFromPool(size_t size);
void ptpRefTqActionDeallocateMemFromPool(void* ptr);
void ptpListTqActionObjectDtor(TqActionRef action);
void ptpTimerQueueAction(TqActionRef arg);

// TqAction object constructor
void ptpTqActionCtor(TqActionRef ctx)
{
    assert(ctx);

    ctx->_event = NULL;
    ctx->_callback = NULL;
    ctx->_destroyCallback = NULL;
    ctx->_eventType = 0;
}

// TqAction object destructor
void ptpTqActionDtor(TqActionRef ctx)
{
    assert(ctx);

    if (ctx->_destroyCallback && ctx->_event) {
        ctx->_destroyCallback(ctx->_event);
    }
}

static MemoryPoolRef _ptpTqActionMemPool = NULL;

// callback to allocate required amount of memory
void* ptpRefTqActionAllocateMemFromPool(size_t size)
{
    // create memory pool object
    ptpMemPoolCreateOnce(&_ptpTqActionMemPool, size, NULL, NULL);

    // allocate one element from the memory pool
    return ptpMemPoolAllocate(_ptpTqActionMemPool);
}

// callback to release memory allocated via ptpRefAllocateMem callback
void ptpRefTqActionDeallocateMemFromPool(void* ptr)
{
    assert(_ptpTqActionMemPool);

    // release one element to the memory pool
    if (ptr)
        ptpMemPoolDeallocate(_ptpTqActionMemPool, ptr);
}

DECLARE_REFCOUNTED_OBJECT(TqAction)
DEFINE_REFCOUNTED_OBJECT(TqAction, ptpRefTqActionAllocateMemFromPool, ptpRefTqActionDeallocateMemFromPool, ptpTqActionCtor, ptpTqActionDtor)

void ptpListTqActionObjectDtor(TqActionRef action)
{
    ptpRefTqActionRelease(action);
}

// linked list of TqAction  objects
DECLARE_LIST(TqAction)
DEFINE_LIST(TqAction, ptpListTqActionObjectDtor)

struct ptpTimerQueue_t {

    // memory pool for timer queue actions
    ListTqActionRef _timerQueueList;

    int _key;
    Boolean _stop;
    PtpMutexRef _lock;

    // thread where the timer runs
    PtpThreadRef _queueThread;

    // synchronization condition, mutex
    pthread_cond_t _condition;
    pthread_mutex_t _conditionLock;
    Boolean _conditionNotified;
};

static ThreadExitCode ptpTimerQueueHandler(void* arg);

TimerQueueRef ptpTimerQueueCreate(void)
{
    TimerQueueRef tq = (TimerQueueRef)malloc(sizeof(struct ptpTimerQueue_t));

    if (!tq)
        return NULL;

    tq->_timerQueueList = ptpListTqActionCreate();

    if (!tq->_timerQueueList) {
        free(tq);
        return NULL;
    }

    tq->_key = 0;
    tq->_stop = false;
    tq->_conditionNotified = false;

    // initialize condition and matching mutex
    pthread_mutex_init(&(tq->_conditionLock), NULL);
    pthread_cond_init(&(tq->_condition), NULL);

    tq->_lock = ptpMutexCreate(NULL);

    // run timer queue thread
    tq->_queueThread = ptpThreadCreate();

    if (!tq->_queueThread || !ptpThreadStart(tq->_queueThread, ptpTimerQueueHandler, tq)) {
        PTP_LOG_CRITICAL("unable to start timer queue thread!");
    }

    return tq;
}

void ptpTimerQueueDestroy(TimerQueueRef tq)
{
    if (!tq)
        return;

    ptpTimerQueueDeinitialize(tq);

    pthread_mutex_destroy(&(tq->_conditionLock));
    pthread_cond_destroy(&(tq->_condition));
    ptpListTqActionDestroy(tq->_timerQueueList);

    ptpMutexDestroy(tq->_lock);

    // destroy action arg memory pool
    ptpMemPoolDestroy(_ptpTqActionMemPool);
    _ptpTqActionMemPool = NULL;

    free(tq);
}

static void ptpTimerQueueSignal(TimerQueueRef tq)
{
    assert(tq);

    //
    // signal the condition - this will unblock waiting thread
    //

    pthread_mutex_lock(&(tq->_conditionLock));
    tq->_conditionNotified = true;
    pthread_cond_signal(&(tq->_condition));
    pthread_mutex_unlock(&(tq->_conditionLock));
}

Boolean ptpTimerQueueDeinitialize(TimerQueueRef tq)
{
    assert(tq);

    // if we have already been stopped, leave
    if (tq->_stop)
        return false;

    // this will break the background loop
    tq->_stop = true;

    // unblock waiting queue
    ptpTimerQueueSignal(tq);

    // and clear queued timer events
    if (ptpMutexLock(tq->_lock) == eMutexOk) {

        // clear all items from the queue
        ptpListTqActionClear(tq->_timerQueueList);
        ptpMutexUnlock(tq->_lock);
    }

    ptpThreadJoin(tq->_queueThread, NULL);
    ptpThreadDestroy(tq->_queueThread);
    return true;
}

// execute timer callback
void ptpTimerQueueAction(TqActionRef arg)
{
    assert(arg && arg->_callback && arg->_event);
    arg->_callback(arg->_event);
}

//
// timer queue runloop
//

static ThreadExitCode ptpTimerQueueHandler(void* arg)
{
    TimerQueueRef ctx = (TimerQueueRef)arg;

    // run timer runloop until it is stopped from the outside
    while (!ctx->_stop) {

        // get the current system time
        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);

        // time when the next event will expire
        struct timespec timeout;
        timeout.tv_sec = now.tv_sec + 100;
        timeout.tv_nsec = now.tv_nsec;

        //
        // when we got here it means, that the timer queue got changed.
        // let's analyze it and execute elapsed timers
        //

        // lock access to the queue
        ptpMutexLock(ctx->_lock);

        // get the first timer
        while (!ctx->_stop && ptpListTqActionSize(ctx->_timerQueueList)) {

            // retrieve reference to action from list
            TqActionRef actionContext = ptpListTqActionFront(ctx->_timerQueueList);
            ptpRefTqActionRetain(actionContext);

            // get the current system time
            clock_gettime(CLOCK_REALTIME, &now);

            // did it expire? (difference is negative or below 1ms)
            struct timespec diff;
            timespecsub(&(actionContext->_triggerTime), &now, &diff);

            if ((diff.tv_sec < 0) || (!diff.tv_sec && (diff.tv_nsec < 1000000))) {

                PTP_LOG_VERBOSE("executing event: %d", actionContext->_eventType);

                //
                // yes, it did expire
                //

                // pop the item from the list
                ptpListTqActionPopFront(ctx->_timerQueueList);

                // unlock access to the queue so no action called in ctx->timerQueueAction()
                // can cause deadlock
                ptpMutexUnlock(ctx->_lock);

                // execute callback
                ptpTimerQueueAction(actionContext);

                // release smart pointer
                ptpRefTqActionRelease(actionContext);

                // next timeout in 100s
                timeout.tv_sec = now.tv_sec + 100;
                timeout.tv_nsec = now.tv_nsec;

                // re-lock the queue
                ptpMutexLock(ctx->_lock);

            } else {

                PTP_LOG_VERBOSE("nearest event %d in %d.%03d s", actionContext->_eventType, diff.tv_sec, diff.tv_nsec / 1000000);

                // when will the next event expire?
                timeout.tv_sec = actionContext->_triggerTime.tv_sec;
                timeout.tv_nsec = actionContext->_triggerTime.tv_nsec;

                // release smart pointer
                ptpRefTqActionRelease(actionContext);
                break;
            }
        }

        // unlock access to the queue
        ptpMutexUnlock(ctx->_lock);

        //
        // wait until we are unblocked from the outside (timer queue changes)
        // or the timeout expires
        //

        pthread_mutex_lock(&ctx->_conditionLock);
        while (!ctx->_conditionNotified) {

            struct timespec startTime;
            clock_gettime(CLOCK_REALTIME, &startTime);

            if (timeout.tv_nsec >= kNanosecondsPerSecond) {
                timeout.tv_sec += (timeout.tv_nsec / kNanosecondsPerSecond);
                timeout.tv_nsec = (timeout.tv_nsec % kNanosecondsPerSecond);
            } else if (timeout.tv_nsec < 0) {
                timeout.tv_nsec = 0;
            }

            int err = pthread_cond_timedwait(&ctx->_condition, &ctx->_conditionLock, &timeout);

            // if the timeout expired, leave
            if (err == ETIMEDOUT) {

                struct timespec diff;
                timespecsub(&timeout, &startTime, &diff);
                PTP_LOG_VERBOSE("cond timed out after %d.%03d s", diff.tv_sec, diff.tv_nsec / 1000000);
                break;
            }
        }

        ctx->_conditionNotified = false;
        pthread_mutex_unlock(&ctx->_conditionLock);
    }

    return threadExitOk;
}

Boolean ptpTimerQueueAddEvent(TimerQueueRef tq, long micros, int type, TimerQueueHandler func, TimerQueueHandler destroyFunc, void* arg)
{
    assert(tq);

    // lock access to the queue
    ptpMutexLock(tq->_lock);

    // if the queue has been stopped in the meantime, do not queue any event
    if (tq->_stop) {

        ptpMutexUnlock(tq->_lock);
        return false;
    }

    TqActionRef scheduledAction = ptpRefTqActionAlloc();

    scheduledAction->_event = arg;
    scheduledAction->_callback = func;
    scheduledAction->_destroyCallback = destroyFunc;
    scheduledAction->_eventType = type;

    // retrieve current time
    struct timespec currTime;
    clock_gettime(CLOCK_REALTIME, &currTime);

    // calculate trigger time
    scheduledAction->_triggerTime.tv_sec = currTime.tv_sec + micros / 1000000;
    scheduledAction->_triggerTime.tv_nsec = (long)currTime.tv_nsec + (micros % 1000000) * 1000;

    // check overflow
    if (scheduledAction->_triggerTime.tv_nsec > 1000000000) {

        scheduledAction->_triggerTime.tv_nsec -= 1000000000;
        scheduledAction->_triggerTime.tv_sec++;
    }

    //
    // insertion sort to the list
    //

    Boolean added = false;
    ListIteratorRef it;

    for (it = ptpListTqActionFirst(tq->_timerQueueList); it != NULL; it = ptpListTqActionNext(it)) {

        // has the current item higher trigger time than the currently added item?
        struct timespec diff;

        // acquire action
        TqActionRef actionContext = ptpListTqActionIteratorValue(it);
        ptpRefTqActionRetain(actionContext);

        struct timespec* currTriggerTime = &(actionContext->_triggerTime);
        timespecsub(currTriggerTime, &(scheduledAction->_triggerTime), &diff);

        // release smart object
        ptpRefTqActionRelease(actionContext);

        if (diff.tv_sec >= 0) {

            // then add new item to the position BEFORE iterator
            ptpRefTqActionRetain(scheduledAction);
            ptpListTqActionInsert(tq->_timerQueueList, it, scheduledAction);
            added = true;
            break;
        }
    }

    if (!added) {
        ptpRefTqActionRetain(scheduledAction);
        ptpListTqActionPushBack(tq->_timerQueueList, scheduledAction);
    }

    ptpRefTqActionRelease(scheduledAction);
    PTP_LOG_VERBOSE("scheduling event: %d in %d.%d s", type, micros / 1000000, micros / 1000);

    // unlock access to the queue
    ptpMutexUnlock(tq->_lock);

    // unblock waiting queue
    ptpTimerQueueSignal(tq);
    return true;
}

Boolean ptpTimerQueueIsEventScheduled(TimerQueueRef tq, int type)
{
    Boolean ret = false;
    assert(tq);

    // lock access to the queue
    ptpMutexLock(tq->_lock);

    // if the queue has been stopped in the meantime, do not remove any event
    if (tq->_stop) {

        ptpMutexUnlock(tq->_lock);
        return false;
    }

    ListIteratorRef it;
    for (it = ptpListTqActionFirst(tq->_timerQueueList); it != NULL;) {

        // acquire action object
        TqActionRef actionContext = ptpListTqActionIteratorValue(it);
        ptpRefTqActionRetain(actionContext);

        if (actionContext->_eventType == type) {
            // we have found a match
            ret = true;

            // dereference smart object
            ptpRefTqActionRelease(actionContext);

            // and exit the search loop
            break;
        } else {
            it = ptpListTqActionNext(it);
        }

        // dereference smart object
        ptpRefTqActionRelease(actionContext);
    }

    // unlock access to the queue
    ptpMutexUnlock(tq->_lock);
    return ret;
}

Boolean ptpTimerQueueCancelEvent(TimerQueueRef tq, int type)
{
    assert(tq);

    // lock access to the queue
    ptpMutexLock(tq->_lock);

    // if the queue has been stopped in the meantime, do not remove any event
    if (tq->_stop) {

        ptpMutexUnlock(tq->_lock);
        return false;
    }

    ListIteratorRef it;
    for (it = ptpListTqActionFirst(tq->_timerQueueList); it != NULL;) {

        // acquire action object
        TqActionRef actionContext = ptpListTqActionIteratorValue(it);
        ptpRefTqActionRetain(actionContext);

        if (actionContext->_eventType == type) {

            PTP_LOG_VERBOSE("canceling event: %d", type);
            it = ptpListTqActionErase(tq->_timerQueueList, it);

        } else {
            it = ptpListTqActionNext(it);
        }

        // dereference smart object
        ptpRefTqActionRelease(actionContext);
    }

    // unlock access to the queue
    ptpMutexUnlock(tq->_lock);

    // unblock waiting queue
    ptpTimerQueueSignal(tq);
    return true;
}
