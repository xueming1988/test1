
#ifndef __ASR_DEBUG_H__
#define __ASR_DEBUG_H__

#define ASR_DEBUG 1

#define ASR_DEBUG_NONE 0x0000
#define ASR_DEBUG_INFO 0x0001
#define ASR_DEBUG_TRACE 0x0002
#define ASR_DEBUG_ERR 0x0004
#define ASR_DEBUG_ALL 0xFFFF

#define TICK_POWER_ON
#ifndef TICK_POWER_ON
#define UTC_TIME
#endif

#if ASR_DEBUG

#include "wm_util.h"

extern unsigned int g_debug_level;
#define COLOR_RED "\033[1;31m"
#define COLOR_GRN "\033[1;32m"
#define COLOR_GRY "\033[1;30m"
#define COLOR_NOR "\033[0m"

#define ASR_PRINT(flag, fmt, ...)                                                                  \
    do {                                                                                           \
        if (g_debug_level & flag) {                                                                \
            if (flag == ASR_DEBUG_ERR) {                                                           \
                fprintf(stderr, "[%02d:%02d:%02d.%03d][%s:%d]" fmt,                                \
                        (int)(tickSincePowerOn() / 3600000),                                       \
                        (int)(tickSincePowerOn() % 3600000 / 60000),                               \
                        (int)(tickSincePowerOn() % 60000 / 1000),                                  \
                        (int)(tickSincePowerOn() % 1000), __func__, __LINE__, ##__VA_ARGS__);      \
            } else if (flag == ASR_DEBUG_TRACE) {                                                  \
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

#else

#define ASR_PRINT(flag, fmt, ...)

#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Set the debug level
 * Input :
 *         @ level: debug level
 * Output:
 */
void asr_set_debug_level(int level);

#ifdef __cplusplus
}
#endif

#endif
