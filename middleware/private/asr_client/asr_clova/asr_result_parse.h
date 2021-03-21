#ifndef __ASR_RESULT_PARSE_H__
#define __ASR_RESULT_PARSE_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

int asr_result_json_parse(char *message, unsigned long long request_id);

int asr_unauthor_json_parse(char *message);

#ifdef __cplusplus
}
#endif

#endif
