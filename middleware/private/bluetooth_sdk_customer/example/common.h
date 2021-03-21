#ifndef __COMMON_H__
#define __COMMON_H__

#include "json.h"

#define JSON_GET_INT_FROM_OBJECT(object, key)                                                      \
    ((object == NULL) ? 0                                                                          \
                      : ((json_object_object_get((object), (key))) == NULL)                        \
                            ? 0                                                                    \
                            : (json_object_get_int(json_object_object_get((object), (key)))))

#define JSON_GET_INT64_FROM_OBJECT(object, key)                                                    \
    ((object == NULL) ? 0                                                                          \
                      : ((json_object_object_get((object), (key))) == NULL)                        \
                            ? 0                                                                    \
                            : (json_object_get_int64(json_object_object_get((object), (key)))))

#define JSON_GET_STRING_FROM_OBJECT(object, key)                                                   \
    ((object == NULL) ? NULL                                                                       \
                      : ((json_object_object_get((object), (key)) == NULL)                         \
                             ? NULL                                                                \
                             : (json_object_get_string(json_object_object_get((object), (key))))))

#define JSON_GET_DOUBLE_FROM_OBJECT(object, key)                                                   \
    ((object == NULL) ? 0.0                                                                        \
                      : ((json_object_object_get((object), (key)) == NULL)                         \
                             ? 0.0                                                                 \
                             : (json_object_get_double(json_object_object_get((object), (key))))))

#define JSON_GET_BOOLEAN_FROM_OBJECT(object, key)                                                  \
    ((object == NULL)                                                                              \
         ? 0                                                                                       \
         : ((json_object_object_get((object), (key)) == NULL)                                      \
                ? 0                                                                                \
                : (json_object_get_boolean(json_object_object_get((object), (key))))))

inline static int string_to_bt_address(char *addr, BD_ADDR *bt_addr)
{
    int tmp[BD_ADDR_LEN];

    sscanf(addr, "%02x:%02x:%02x:%02x:%02x:%02x", &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4],
           &tmp[5]);

    (*bt_addr)[0] = tmp[0];
    (*bt_addr)[1] = tmp[1];
    (*bt_addr)[2] = tmp[2];
    (*bt_addr)[3] = tmp[3];
    (*bt_addr)[4] = tmp[4];
    (*bt_addr)[5] = tmp[5];

    return 0;
}

inline static int bt_address_to_string(char *addr, BD_ADDR *bt_addr)
{
    sprintf(addr, "%02x:%02x:%02x:%02x:%02x:%02x", (*bt_addr)[0], (*bt_addr)[1], (*bt_addr)[2],
            (*bt_addr)[3], (*bt_addr)[4], (*bt_addr)[5]);

    return 0;
}

#endif
