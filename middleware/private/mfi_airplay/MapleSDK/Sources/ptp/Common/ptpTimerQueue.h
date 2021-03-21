/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_TIMER_QUEUE_H_
#define _PTP_TIMER_QUEUE_H_

#include "ptpMutex.h"
#include "ptpThread.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Timer queue handler callback type
 */
typedef void (*TimerQueueHandler)(void*);

typedef struct ptpTimerQueue_t TimerQueue;
typedef TimerQueue* TimerQueueRef;

/**
 * @brief allocates memory for a single instance of TimerQueue object
 * @return dynamically allocated TimerQueue object
 */
TimerQueueRef ptpTimerQueueCreate(void);

/**
 * @brief Destroy TimerQueue object
 * @param tq reference to TimerQueue object
 */
void ptpTimerQueueDestroy(TimerQueueRef tq);

/**
 * @brief Deinitialize timer queue but don't destroy it
 * @param tq reference to TimerQueue object
 * @return true if succeeded, false otherwise
 */
Boolean ptpTimerQueueDeinitialize(TimerQueueRef tq);

/**
 * @brief Add an event to the timer queue
 * @param tq reference to TimerQueue object
 * @param micros execution timeout in microseconds
 * @param type event type
 * @param func event callback function
 * @param destroyFunc event destructor
 * @param object event context
 * @return true if succeeded, false otherwise
 */
Boolean ptpTimerQueueAddEvent(TimerQueueRef tq, long micros, int type, TimerQueueHandler func, TimerQueueHandler destroyFunc, void* object);

/**
 * @brief Check if the event of given type is scheduled for execution
 * @param tq reference to TimerQueue object
 * @param type event type
 * @return true if the event is scheduled, false otherwise
 */
Boolean ptpTimerQueueIsEventScheduled(TimerQueueRef tq, int type);

/**
 * @brief Remove an event from the timer queue
 * @param tq reference to TimerQueue object
 * @param type type of event that must be removed
 * @return true if succeeded, false otherwise
 */
Boolean ptpTimerQueueCancelEvent(TimerQueueRef tq, int type);

#ifdef __cplusplus
}
#endif

#endif // _PTP_TIMER_QUEUE_H_
