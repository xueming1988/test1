/*************************************************************************
    > File Name: os_define.h
    > Author: Keying.Zhao
    > Mail: Keying.Zhao@linkplay.com
    > Created Time: Tue 21 Nov 2017 02:35:55 AM EST
    > brief:
    >   A player for mp3/pcm buffer stream, url/file stream
 ************************************************************************/

#ifndef __OS_DEFINE_H__
#define __OS_DEFINE_H__

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "wm_util.h"

#ifndef os_calloc
#define os_calloc(n, size) calloc((n), (size))
#endif

#ifndef os_free
#define os_free(ptr) ((ptr != NULL) ? free(ptr) : (void)ptr)
#endif

#ifndef os_strdup
#define os_strdup(ptr) ((ptr != NULL) ? strdup((ptr)) : NULL)
#endif

#ifndef os_power_on_tick
#define os_power_on_tick() tickSincePowerOn()
#endif

#ifndef os_str_eq
#define os_str_eq(str1, str2)                                                                      \
    ((str1 && str2)                                                                                \
         ? ((strlen((char *)str1) == strlen((char *)str2)) && !strcmp((char *)str1, (char *)str2)) \
         : 0)
#endif

#ifndef os_sub_str
#define os_sub_str(str1, str2) ((str1 && str2) ? (strstr((char *)str1, (char *)str2) != NULL) : 0)
#endif

#define os_debug(fmt, ...)                                                                         \
    do {                                                                                           \
        fprintf(stderr, "[INFO][%02d:%02d:%02d.%03d][%s:%d] " fmt,                                 \
                (int)(os_power_on_tick() / 3600000), (int)(os_power_on_tick() % 3600000 / 60000),  \
                (int)(os_power_on_tick() % 60000 / 1000), (int)(os_power_on_tick() % 1000),        \
                __FUNCTION__, __LINE__, ##__VA_ARGS__);                                            \
    } while (0)

#define os_trace(fmt, ...)                                                                         \
    do {                                                                                           \
        fprintf(stderr, "[TRECE][%02d:%02d:%02d.%03d][%s:%d] " fmt,                                \
                (int)(os_power_on_tick() / 3600000), (int)(os_power_on_tick() % 3600000 / 60000),  \
                (int)(os_power_on_tick() % 60000 / 1000), (int)(os_power_on_tick() % 1000),        \
                __FUNCTION__, __LINE__, ##__VA_ARGS__);                                            \
    } while (0)

#define os_error(fmt, ...)                                                                         \
    do {                                                                                           \
        fprintf(stderr, "[ERROR][%02d:%02d:%02d.%03d][%s:%d] " fmt,                                \
                (int)(os_power_on_tick() / 3600000), (int)(os_power_on_tick() % 3600000 / 60000),  \
                (int)(os_power_on_tick() % 60000 / 1000), (int)(os_power_on_tick() % 1000),        \
                __FUNCTION__, __LINE__, ##__VA_ARGS__);                                            \
    } while (0)

#define os_assert(ptr, str)                                                                        \
    do {                                                                                           \
        if (!(ptr))                                                                                \
            os_error("%s", (str));                                                                 \
        assert((ptr));                                                                             \
    } while (0)

#endif
