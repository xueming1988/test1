
#ifndef __ALEXA_DEBUG_H__
#define __ALEXA_DEBUG_H__

#include <wm_util.h>

#define ALEXA_DEBUG 1

#define ALEXA_DEBUG_NONE 0x0000
#define ALEXA_DEBUG_INFO 0x0001
#define ALEXA_DEBUG_TRACE 0x0002
#define ALEXA_DEBUG_ERR 0x0004
#define ALEXA_DEBUG_ALL 0xFFFF

#ifndef TICK_POWER_ON
#define UTC_TIME
#endif

#if ALEXA_DEBUG

extern unsigned int g_alexa_flags;
#define COLOR_RED "\033[1;31m"
#define COLOR_GRN "\033[1;32m"
#define COLOR_GRY "\033[1;30m"
#define COLOR_NOR "\033[0m"

#define ALEXA_PRINT(flag, fmt, ...)                                                                \
    do {                                                                                           \
        if (g_alexa_flags & flag) {                                                                \
            if (flag == ALEXA_DEBUG_ERR) {                                                         \
                fprintf(stderr, "[%02d:%02d:%02d.%03d][%s:%d]" fmt,                                \
                        (int)(tickSincePowerOn() / 3600000),                                       \
                        (int)(tickSincePowerOn() % 3600000 / 60000),                               \
                        (int)(tickSincePowerOn() % 60000 / 1000),                                  \
                        (int)(tickSincePowerOn() % 1000), __func__, __LINE__, ##__VA_ARGS__);      \
            } else if (flag == ALEXA_DEBUG_TRACE) {                                                \
                fprintf(stderr, "[%02d:%02d:%02d.%03d][%s:%d]" fmt,                                \
                        (int)(tickSincePowerOn() / 3600000),                                       \
                        (int)(tickSincePowerOn() % 3600000 / 60000),                               \
                        (int)(tickSincePowerOn() % 60000 / 1000),                                  \
                        (int)(tickSincePowerOn() % 1000), __func__, __LINE__, ##__VA_ARGS__);      \
            } else {                                                                               \
                fprintf(stderr, "[%02d:%02d:%02d.%03d][%s:%d]" fmt,                                \
                        (int)(tickSincePowerOn() / 3600000),                                       \
                        (int)(tickSincePowerOn() % 3600000 / 60000),                               \
                        (int)(tickSincePowerOn() % 60000 / 1000),                                  \
                        (int)(tickSincePowerOn() % 1000), __func__, __LINE__, ##__VA_ARGS__);      \
            }                                                                                      \
        }                                                                                          \
    } while (0)

#define ALEXA_LOGCHECK(flag, fmt, ...)                                                             \
    do {                                                                                           \
        fprintf(stderr, "[%s]" fmt, current_time(), ##__VA_ARGS__);                                \
    } while (0)

#else

#define ALEXA_PRINT(flag, fmt, ...)
#define ALEXA_LOGCHECK(flag, fmt, ...)

#endif

#define PRINT_LINE() fprintf(stderr, "[%s:%d]\n", __func__, __LINE__)

#ifdef __cplusplus
extern "C" {
#endif

const char *current_time(void);

/*
 * Set the debug level
 * Input :
 *         @ level: debug level
 * Output:
 */
void AlexaSetDebugLevel(int level);

enum {
    CLOSE_TALK,
    NEAR_FIELD,
    FAR_FIELD,
};

/*
 * Check the device is far feild moudle or not
 * Input :
 *         @
 * Output:
 *           0 : close talk moudle
 *           1 : near feild moudle
 *           2 : far feild moudle
 */
int AlexaProfileType(void);

#ifdef __cplusplus
}
#endif

#endif // __ALEXA_DEBUG_H__
