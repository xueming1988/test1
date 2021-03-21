/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_LOG_H_
#define _PTP_LOG_H_

#include "ptpConstants.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

//
// compiler flags to disable/enable log levels
//

#ifndef PTP_LOG_CRITICAL_ON
#define PTP_LOG_CRITICAL_ON 1
#endif

#ifndef PTP_LOG_ERROR_ON
#define PTP_LOG_ERROR_ON 1
#endif

#ifndef PTP_LOG_EXCEPTION_ON
#define PTP_LOG_EXCEPTION_ON 1
#endif

#ifndef PTP_LOG_WARNING_ON
#define PTP_LOG_WARNING_ON 1
#endif

#ifndef PTP_LOG_INFO_ON
#define PTP_LOG_INFO_ON 1
#endif

#ifndef PTP_LOG_STATUS_ON
#define PTP_LOG_STATUS_ON 1
#endif

#ifndef PTP_LOG_DEBUG_ON
#define PTP_LOG_DEBUG_ON 1
#endif

#ifndef PTP_LOG_VERBOSE_ON
#define PTP_LOG_VERBOSE_ON 1
#endif

typedef enum {
    PTP_LOG_LVL_CRITICAL,
    PTP_LOG_LVL_ERROR,
    PTP_LOG_LVL_EXCEPTION,
    PTP_LOG_LVL_WARNING,
    PTP_LOG_LVL_INFO,
    PTP_LOG_LVL_STATUS,
    PTP_LOG_LVL_DEBUG,
    PTP_LOG_LVL_VERBOSE,
} PTP_LOG_LEVEL;

/**
 * @brief Initialize logging subsystem
 */
void ptpLogInit(void);

/**
 * @brief Deinitialize logging subsystem
 */
void ptpLogDeinit(void);

/**
 * @brief Add one entry to the log
 * @param level log level of this call
 * @param tag tag string associated with this call
 * @param path caller source code path
 * @param line caller source code line
 * @param fmt formatting tag
 */
void ptpLog(PTP_LOG_LEVEL level, const char* tag, const char* path, int line, const char* fmt, ...);

/**
 * @brief Configure current logging level
 * @param level new value of logging level
 */
void ptpLogConfigure(const PTP_LOG_LEVEL level);

#if PTP_LOG_CRITICAL_ON
#define PTP_LOG_CRITICAL(fmt, ...) ptpLog(PTP_LOG_LVL_CRITICAL, "CRIT ", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define PTP_LOG_CRITICAL(fmt, ...)
#endif

#if PTP_LOG_ERROR_ON
#define PTP_LOG_ERROR(fmt, ...) ptpLog(PTP_LOG_LVL_ERROR, "ERROR", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define PTP_LOG_ERROR(fmt, ...)
#endif

#if PTP_LOG_EXCEPTION_ON
#define PTP_LOG_EXCEPTION(fmt, ...) ptpLog(PTP_LOG_LVL_EXCEPTION, "EXCPT", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define PTP_LOG_EXCEPTION(fmt, ...)
#endif

#if PTP_LOG_WARNING_ON
#define PTP_LOG_WARNING(fmt, ...) ptpLog(PTP_LOG_LVL_WARNING, "WARN ", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define PTP_LOG_WARNING(fmt, ...)
#endif

#if PTP_LOG_INFO_ON
#define PTP_LOG_INFO(fmt, ...) ptpLog(PTP_LOG_LVL_INFO, "INFO ", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define PTP_LOG_INFO(fmt, ...)
#endif

#if PTP_LOG_STATUS_ON
#define PTP_LOG_STATUS(fmt, ...) ptpLog(PTP_LOG_LVL_STATUS, "STAT ", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define PTP_LOG_STATUS(fmt, ...)
#endif

#if PTP_LOG_DEBUG_ON
#define PTP_LOG_DEBUG(fmt, ...) ptpLog(PTP_LOG_LVL_DEBUG, "DBG  ", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define PTP_LOG_DEBUG(fmt, ...)
#endif

#if PTP_LOG_VERBOSE_ON
#define PTP_LOG_VERBOSE(fmt, ...) ptpLog(PTP_LOG_LVL_VERBOSE, "VRBS ", __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define PTP_LOG_VERBOSE(fmt, ...)
#endif

#ifdef __cplusplus
}
#endif

#endif // _PTP_LOG_H_
