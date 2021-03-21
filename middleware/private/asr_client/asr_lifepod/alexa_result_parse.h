#ifndef __ALEXA_RESULT_PARSE_H__
#define __ALEXA_RESULT_PARSE_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef YES
#define YES (1)
#endif

#ifndef NO
#define NO (0)
#endif

int alexa_result_json_parse(char *message, int is_next);

int alexa_unauthor_json_parse(char *message);

void alexa_clear_authinfo(void);

#ifdef __cplusplus
}
#endif

#endif
