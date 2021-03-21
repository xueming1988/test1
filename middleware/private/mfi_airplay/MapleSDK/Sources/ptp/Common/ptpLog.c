/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpLog.h"
#include "ptpCondition.h"
#include "ptpList.h"
#include "ptpMemoryPool.h"
#include "ptpMutex.h"
#include "ptpPlatform.h"
#include "ptpRefCountedObject.h"
#include "ptpThread.h"
#include "ptpThreadSafeCallback.h"

#include "AtomicUtils.h"
#include "DebugServices.h"
#include <assert.h>

static PTP_LOG_LEVEL g_ptpLogLevel = PTP_LOG_LVL_DEBUG;

// use AirPlay debug subsystem
static ulog_define(Ptp, kLogLevelVerbose, kLogFlags_Default, "PTP", NULL);
#define ptp_ulog(LEVEL, ...) ulog(&log_category_from_name(Ptp), (LEVEL), __VA_ARGS__)
#define FILE_NAME(path) (strrchr(path, '/') ? strrchr(path, '/') + 1 : path)

//
// configure logging
//
void ptpLogConfigure(const PTP_LOG_LEVEL level)
{
    g_ptpLogLevel = level;
}

#ifndef PTP_USE_DEDICATED_LOGGING_THREAD

void ptpLogInit(void)
{
}

void ptpLogDeinit(void)
{
}

void ptpLog(PTP_LOG_LEVEL level, const char* tag, const char* path, int line, const char* fmt, ...)
{
    if (level > g_ptpLogLevel) {
        return;
    }

    int logLevel;
    char _text[1024];

    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(_text, sizeof(_text) - 1, fmt, args);
    if (n > 0)
        _text[n] = 0;

    switch (level) {
    case PTP_LOG_LVL_CRITICAL:
        logLevel = kLogLevelCritical;
        break;
    case PTP_LOG_LVL_ERROR:
        logLevel = kLogLevelError;
        break;
    case PTP_LOG_LVL_EXCEPTION:
        logLevel = kLogLevelAssert;
        break;
    case PTP_LOG_LVL_WARNING:
        logLevel = kLogLevelWarning;
        break;
    case PTP_LOG_LVL_INFO:
        logLevel = kLogLevelNotice;
        break;
    case PTP_LOG_LVL_STATUS:
        logLevel = kLogLevelInfo;
        break;
    case PTP_LOG_LVL_DEBUG:
        logLevel = kLogLevelTrace;
        break;
    case PTP_LOG_LVL_VERBOSE:
        logLevel = kLogLevelVerbose;
        break;
    default:
        logLevel = kLogLevelChatty;
        break;
    }

#ifdef VERBOSE_LOG
    ptp_ulog(logLevel, "%s: (%.14s:%4u) %s\n", tag, FILE_NAME(path), line, _text);
#else
    (void)line;
    (void)path;
    ptp_ulog(logLevel, "%s: %s\n", tag, _text);
#endif
}

#else // PTP_USE_DEDICATED_LOGGING_THREAD

#define MAX_LOG_TAG_LENGTH 8
#define MAX_LOG_PATH_LENGTH 14
#define MAX_LOG_LINE_LENGTH 1024
#define MAX_QUEUED_LOG_ENTRIES 128

//
// One log line element
//

typedef struct {
    PTP_LOG_LEVEL _level;
#ifdef VERBOSE_LOG
    int _line;
    char _path[MAX_LOG_PATH_LENGTH];
#endif
    char _tag[MAX_LOG_TAG_LENGTH];
    char _text[MAX_LOG_LINE_LENGTH];
} LogEntry;

typedef LogEntry* LogEntryRef;

static MemoryPoolRef _ptpLogEntryMemPool = NULL;

static void ptpLogEntryCtor(LogEntryRef ctx);
static void ptpLogEntryDtor(LogEntryRef ctx);
static void* ptpLogEntryAllocateMemFromPool(size_t size);
static void ptpLogEntryDeallocateMemFromPool(void* ptr);
static void ptpListLogEntryObjectDtor(LogEntryRef action);
// LogEntry object constructor
static void ptpLogEntryCtor(LogEntryRef ctx)
{
    assert(ctx);

    ctx->_level = PTP_LOG_LVL_ERROR;
#ifdef VERBOSE_LOG
    ctx->_line = 0;
    ctx->_path[0] = 0;
#endif
    ctx->_tag[0] = 0;
    ctx->_text[0] = 0;
}

// LogEntry object destructor
static void ptpLogEntryDtor(LogEntryRef ctx)
{
    assert(ctx);
    (void)ctx;
}

// callback to allocate required amount of memory
static void* ptpLogEntryAllocateMemFromPool(size_t size)
{
    // make sure memory pool object is created
    ptpMemPoolCreateOnce(&_ptpLogEntryMemPool, size, NULL, NULL);

    // allocate one element from the memory pool
    return ptpMemPoolAllocate(_ptpLogEntryMemPool);
}

// callback to release memory allocated via ptpRefAllocateMem callback
void ptpLogEntryDeallocateMemFromPool(void* ptr)
{
    assert(_ptpLogEntryMemPool);

    // release one element to the memory pool
    if (ptr)
        ptpMemPoolDeallocate(_ptpLogEntryMemPool, ptr);
}

DECLARE_REFCOUNTED_OBJECT(LogEntry)
DEFINE_REFCOUNTED_OBJECT(LogEntry, ptpLogEntryAllocateMemFromPool, ptpLogEntryDeallocateMemFromPool, ptpLogEntryCtor, ptpLogEntryDtor)

static void ptpListLogEntryObjectDtor(LogEntryRef action)
{
    ptpRefLogEntryRelease(action);
}

// linked list of LogEntry objects
DECLARE_LIST(LogEntry)
DEFINE_LIST(LogEntry, ptpListLogEntryObjectDtor)

//
// Logging context
//

typedef struct {
    PtpThreadRef _thread;
    PtpMutexRef _cs;
    ListLogEntryRef _logList;
    bool _running;
    PtpConditionRef _event;
} ptpLogCtx;

typedef ptpLogCtx* ptpLogCtxRef;
static void ptpLogCtxCtor(ptpLogCtxRef log);
static void ptpLogCtxDtor(ptpLogCtxRef log);
static ThreadExitCode ptpLogCtxRun(void* arg);

static ThreadExitCode ptpLogCtxRun(void* arg)
{
    ptpLogCtxRef log = (ptpLogCtxRef)arg;
    assert(log);

#if TARGET_OS_BRIDGECO
    // give it a low priority
    unsigned int oldPriority = 0;
    osThreadPriorityChange(osThreadIdentify(), 23, &oldPriority);
#endif

    while (log->_running) {

        ptpMutexLock(log->_cs);

        if (ptpListLogEntrySize(log->_logList)) {

            int32_t logLevel = kLogLevelChatty;

            LogEntryRef logEntry = ptpListLogEntryFront(log->_logList);
            ptpRefLogEntryRetain(logEntry);
            ptpListLogEntryPopFront(log->_logList);
            ptpMutexUnlock(log->_cs);

            // transform PTP log level to AirPlay level
            switch (logEntry->_level) {

            case PTP_LOG_LVL_CRITICAL:
                logLevel = kLogLevelCritical;
                break;
            case PTP_LOG_LVL_ERROR:
                logLevel = kLogLevelError;
                break;

            case PTP_LOG_LVL_EXCEPTION:
                logLevel = kLogLevelAssert;
                break;

            case PTP_LOG_LVL_WARNING:
                logLevel = kLogLevelWarning;
                break;

            case PTP_LOG_LVL_INFO:
                logLevel = kLogLevelNotice;
                break;

            case PTP_LOG_LVL_STATUS:
                logLevel = kLogLevelInfo;
                break;

            case PTP_LOG_LVL_DEBUG:
                logLevel = kLogLevelTrace;
                break;

            case PTP_LOG_LVL_VERBOSE:
                logLevel = kLogLevelVerbose;
                break;
            }

#ifdef VERBOSE_LOG
            ptp_ulog(logLevel, "%s: (%.14s:%4u) %s\n", logEntry->_tag, logEntry->_path, logEntry->_line, logEntry->_text);
#else
            ptp_ulog(logLevel, "%s: %s\n", logEntry->_tag, logEntry->_text);
#endif
            ptpRefLogEntryRelease(logEntry);

        } else {
            ptpMutexUnlock(log->_cs);
            ptpConditionWait(log->_event);
        }
    }

    return threadExitOk;
}

static void ptpLogCtxCtor(ptpLogCtxRef log)
{
    assert(log);

    log->_cs = ptpMutexCreate(NULL);
    log->_logList = ptpListLogEntryCreate();
    log->_thread = ptpThreadCreate();
    log->_running = true;
    log->_event = ptpConditionCreate();

    ptpThreadStart(log->_thread, ptpLogCtxRun, log);
}

static void ptpLogCtxDtor(ptpLogCtxRef log)
{
    if (!log)
        return;

    log->_running = false;
    ptpConditionSignal(log->_event);
    ptpThreadJoin(log->_thread, NULL);
    ptpThreadDestroy(log->_thread);

    ptpConditionDestroy(log->_event);
    ptpListLogEntryDestroy(log->_logList);
    ptpMutexDestroy(log->_cs);
}

static void ptpLogInitCallback(void* ctx)
{
    ptpLogCtxRef* logCtxRef = (ptpLogCtxRef*)ctx;

    if (logCtxRef && !(*logCtxRef)) {
        *logCtxRef = malloc(sizeof(ptpLogCtx));
        ptpLogCtxCtor(*logCtxRef);
    }
}

static void ptpLogDeinitCallback(void* ctx)
{
    ptpLogCtxRef* logCtxRef = (ptpLogCtxRef*)ctx;

    if (logCtxRef && (*logCtxRef)) {
        ptpLogCtxDtor(*logCtxRef);
        free(*logCtxRef);
        *logCtxRef = NULL;
    }
}

static ptpLogCtxRef _ptpLogCtx = NULL;

void ptpLogInit(void)
{
    // do a quick atomic check whether the log object has already been initialized (it must be non zero)
    if (atomic_add_and_fetch_32((int32_t*)&_ptpLogCtx, 0) != 0)
        return;

    ptpThreadSafeCallback(ptpLogInitCallback, &_ptpLogCtx);
}

void ptpLogDeinit(void)
{
    ptpThreadSafeCallback(ptpLogDeinitCallback, &_ptpLogCtx);
}

//
// configure logging
//

void ptpLog(PTP_LOG_LEVEL level, const char* tag, const char* path, int line, const char* fmt, ...)
{
    (void)path;
    (void)line;

    // make sure logging subsystem is initialized
    ptpLogInit();

    if (level > g_ptpLogLevel) {
        return;
    }

    // allocate new logging entry
    LogEntryRef entry = ptpRefLogEntryAlloc();

    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(entry->_text, sizeof(entry->_text) - 1, fmt, args);
    if (n > 0)
        entry->_text[n] = 0;

    entry->_level = level;
    strncpy(entry->_tag, tag, sizeof(entry->_tag) - 1);

#ifdef VERBOSE_LOG
    entry->_line = line;
    strncpy(entry->_path, FILE_NAME(path), sizeof(entry->_path) - 1);
#endif

    ptpMutexLock(_ptpLogCtx->_cs);

    // if we have too many queued entries, remove the oldest one
    if (ptpListLogEntrySize(_ptpLogCtx->_logList) > MAX_QUEUED_LOG_ENTRIES)
        ptpListLogEntryPopFront(_ptpLogCtx->_logList);

    ptpListLogEntryPushBack(_ptpLogCtx->_logList, entry);

    // add logging entry to the list
    ptpMutexUnlock(_ptpLogCtx->_cs);

    // unblock message processing thread
    ptpConditionSignal(_ptpLogCtx->_event);
}
#endif
