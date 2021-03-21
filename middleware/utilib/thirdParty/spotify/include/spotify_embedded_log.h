/**
 * \file spotify_embedded_log.h
 * \brief The public Spotify Embedded API
 * \copyright Copyright 2016 - 2020 Spotify AB. All rights reserved.
 */

#ifndef _SPOTIFY_EMBEDDED_LOG_H
#define _SPOTIFY_EMBEDDED_LOG_H
#include "spotify_embedded.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief GCC printf format extension */
#ifdef __GNUC__
#define SP_EMB_FORMAT(F, A) __attribute__((format(printf, F, A)))
#else
#define SP_EMB_FORMAT(F, A)
#endif

/**
 * \defgroup Log Logging
 *
 * \brief Functionality related to eSDK logging
 */

/**
 * \addtogroup Log
 * @{
 */

/**
 * \brief Log levels
 *
 * These are the log levels for assigned to log messages emitted by the eSDK.
 *
 * \see SpLogSetLevel
 */
typedef enum {
    /** \brief Indicates a severe abnormal condition and program termination */
    SP_LOG_LEVEL_FATAL = 1,
    /** \brief Indicates an abnormal condition that could result in
     * degraded functionality */
    SP_LOG_LEVEL_ERROR = 2,
    /** \brief Indicates a condition that probably has some impact on
     * execution or performance */
    SP_LOG_LEVEL_WARNING = 3,
    /** \brief Informational message */
    SP_LOG_LEVEL_INFO = 4,
    /** \brief Debug message */
    SP_LOG_LEVEL_DEBUG = 5,
    /** \brief Trace message */
    SP_LOG_LEVEL_TRACE = 6,
} SpLogLevel;

/**
 * \brief Trace object
 *
 * The trace object is an abstraction of a specific trace message category and
 * an associated log level. The level can be changed freely for individual
 * trace objects.
 */
struct SpLogTraceObject {
    /** \brief The current debug level for this trace object */
    SpLogLevel level;
    /** \brief The trace category name describing this trace object */
    const char *name;
};

/** \brief Default trace log level */
#ifndef SP_LOG_DEFAULT_LEVEL
#define SP_LOG_DEFAULT_LEVEL SP_LOG_LEVEL_INFO
#endif

/**
 * \brief Macro to define a trace object
 * \param obj Name of the trace object
 */
#define SP_LOG_DEFINE_TRACE_OBJ(obj)                                                               \
    struct SpLogTraceObject trace_obj_##obj = {SP_LOG_DEFAULT_LEVEL, #obj}

/**
 * \brief Macro to declare a previously defined trace object
 * \param obj Name of the trace object
 */
#define SP_LOG_DECLARE_TRACE_OBJ(obj) extern struct SpLogTraceObject trace_obj_##obj

/**
 * \brief Macro to register a previously defined trace object
 * \param obj Name of the trace object
 */
#define SP_LOG_REGISTER_TRACE_OBJ(obj) SpLogRegisterTraceObject(&trace_obj_##obj)

/**
 * \brief Register a defined trace object with eSDK
 *
 * The registration is needed for @ref SpLogSetLevel() to be able to locate the
 * trace object and change the log level on it.
 * \param[in] obj Pointer to trace object
 * \return Return kSpErrorOk on success, or error code on failure.
 */
SP_EMB_PUBLIC
SpError SpLogRegisterTraceObject(struct SpLogTraceObject *obj);

/**
 * \brief Output a debug message via eSDK
 *
 * \param[in] obj Pointer to trace object
 * \param[in] level The log level associated this this message
 * \param[in] file The source file from where the call is made, typically \c __FILE__.
 * \param[in] func The function name from where the call is made, typically \c __func__.
 * \param[in] line The line number from where the call is made, typically \c __LINE__.
 * \param[in] format A printf style format string followed by arguments.
 */
SP_EMB_PUBLIC SP_EMB_FORMAT(6, 7) void SpLog(const struct SpLogTraceObject *obj, SpLogLevel level,
                                             const char *file, const char *func, int line,
                                             const char *format, ...);

/**
 * \brief Macro that outputs a trace message
 *
 * \param obj Trace object name
 * \param lvl Trace level for the message
 * \param file Source file name
 * \param func Calling function name
 * \param line Line number in the source file
 */
#define SP_LOG(obj, lvl, file, func, line, ...)                                                    \
    do {                                                                                           \
        if (trace_obj_##obj.level >= lvl) {                                                        \
            SpLog(&(trace_obj_##obj), lvl, file, func, line, __VA_ARGS__);                         \
        }                                                                                          \
    } while (0)

/** \brief Emit trace message with @ref SP_LOG_LEVEL_FATAL */
#define SpLogFatal(obj, ...)                                                                       \
    SP_LOG(obj, SP_LOG_LEVEL_FATAL, __FILE__, __func__, __LINE__, __VA_ARGS__)

/** \brief Emit trace message with @ref SP_LOG_LEVEL_ERROR */
#define SpLogError(obj, ...)                                                                       \
    SP_LOG(obj, SP_LOG_LEVEL_ERROR, __FILE__, __func__, __LINE__, __VA_ARGS__)

/** \brief Emit trace message with @ref SP_LOG_LEVEL_WARNING */
#define SpLogWarning(obj, ...)                                                                     \
    SP_LOG(obj, SP_LOG_LEVEL_WARNING, __FILE__, __func__, __LINE__, __VA_ARGS__)

/** \brief Emit trace message with @ref SP_LOG_LEVEL_INFO */
#define SpLogInfo(obj, ...)                                                                        \
    SP_LOG(obj, SP_LOG_LEVEL_INFO, __FILE__, __func__, __LINE__, __VA_ARGS__)

/** \brief Emit trace message with @ref SP_LOG_LEVEL_DEBUG */
#define SpLogDebug(obj, ...)                                                                       \
    SP_LOG(obj, SP_LOG_LEVEL_DEBUG, __FILE__, __func__, __LINE__, __VA_ARGS__)

/** \brief Emit trace message with @ref SP_LOG_LEVEL_TRACE */
#define SpLogTrace(obj, ...)                                                                       \
    SP_LOG(obj, SP_LOG_LEVEL_TRACE, __FILE__, __func__, __LINE__, __VA_ARGS__)

/**
 * \brief Control the current logging level
 *
 * Control the logging level for a particular category. If the current
 * configured log level is greater than or equal to the level associated with
 * the message, the log message will be passed to the debug log callback. Each
 * log message is also associated with a \a category. The category is a string
 * that is included in the log message output. Each individual category has a
 * separate current log level.
 *
 * The log messages produced by the eSDK are formatted as: `<LEVEL> <CATEGORY>
 * <MESSAGE>` where `<LEVEL>` is a single letter indicating the log level
 * (E=Error, I=Info, etc.), `<CATEGORY>` is a short string that identifies the
 * log category, for example 'api' and `<MESSAGE>` is the actual debug message.
 *
 * As an example, a debug output string with level set to @ref
 * SP_LOG_LEVEL_ERROR (letter "E") and the category `"api"` could look
 * something like this (the timestamp is not actually part of the debug message
 * from eSDK, but added by the DebugMessageCallback. Thus, the format may not
 * exactly match the example below).
 *
 * 12:01:02.000 E api Invalid parameter passed to SpInit()
 *
 * Example:
 *
 *     // Disable all log messages...
 *     SpLogSetLevel(NULL, 0);
 *     // ...and then enable DEBUG level messages from category "audio"
 *     SpLogSetLevel(SP_LOG_LEVEL_DEBUG, "audio")
 *
 * \param[in] category Identifies the log category for which to set the logging
 * level. If NULL is passed in this parameter, the \a level will be applied to
 * all categories.
 * \param[in] level The log level defined by \ref SpLogLevel
 *
 * \return Returns an error code if the \a category is not a valid log
 * category.
 */
SP_EMB_PUBLIC SpError SpLogSetLevel(const char *category, SpLogLevel level);

/**
 * \brief Get registered trace objects and log levels
 *
 * The information about trace objects and levels is represented as a
 * string containing comma separated tuples `<level>:<trace obj>`, e.g.
 * "4:app,4:esdk,4:audio".
 *
 * \param[in] buffer A buffer to copy the string to.
 * \param[in] buffer_len Length of the buffer
 *
 * \return Returns an error code if buffer is not big enough.
 */
SP_EMB_PUBLIC SpError SpLogGetLevels(char *buffer, int buffer_len);

/**
 * @}
 */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _SPOTIFY_EMBEDDED_LOG_H */
