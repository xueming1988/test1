#ifndef LOG_OBSERVER_INTERFACE_H_
#define LOG_OBSERVER_INTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Enum used to specify the severity assigned to a log message.
 */
typedef enum Level {
    /// Log of a event that indicates an unrecoverable error.
    LEVEL_CRITICAL = 0,

    /// Log of an event that indicates an error.
    LEVEL_ERROR = 1,

    /// Log of an event that may indicate a problem.
    LEVEL_WARN = 2,

    /// Logs of normal operations, to be used in release builds.
    LEVEL_INFO = 3,

    /// Debug log levels
    LEVEL_DEBUG = 4,
    LEVEL_TRACE = 5,
    LEVEL_DEBUG_TRACE= 6
} Level;

typedef struct LogObserverInterface {
    /**
    * Observer interface for listening to print logs.
    */
    void (*onLogWrite)(Level logLevel, const char* logMessage);

#ifdef __cplusplus
    bool operator==(const struct LogObserverInterface& other) const {
        return onLogWrite == other.onLogWrite;
    }

    bool isValid() const {
        return onLogWrite != nullptr;
    }
#endif
} LogObserverInterface;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LOG_OBSERVER_INTERFACE_H_
