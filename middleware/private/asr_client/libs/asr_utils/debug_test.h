
/**
 *
 *
 *
 *
 *
 */

#ifndef __DEBUG_TEST_H__
#define __DEBUG_TEST_H__

#include <stdio.h>
#include <string.h>
#include "wm_api.h"

#ifdef __cplusplus
extern "C" {
#endif

int debug_mode_on(void);

int debug_cmd_parse(SOCKET_CONTEXT *msg_socket, char *recv_buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif
