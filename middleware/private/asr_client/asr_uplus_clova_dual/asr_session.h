#ifndef __ASR_SESSION_H__
#define __ASR_SESSION_H__

#include "asr_debug.h"

#define ASR_CLOVA (0)
#define ASR_LGUPLUS (1)

#ifdef __cplusplus
extern "C" {
#endif

int asr_session_init(void);

int asr_session_uninit(void);

int asr_session_start(int req_type);

int asr_session_disconncet(int asr_type);

int asr_session_ping(int asr_type);

/*
 * init the upload ringbuffer
 * retrurn writen size
 */
void asr_session_record_buffer_reset(void);

/*
 * write record data from record moudle
 * retrurn writen size
 */
int asr_session_record_buffer_write(char *data, size_t size);

/*
 * stop write record data when finished speek
 */
int asr_session_record_buffer_end(void);

/*
 * stop write record data by other
 */
int asr_session_record_buffer_stop(void);

/*
 *  return 0 : not login
 */
int asr_session_check_login(void);

#ifdef __cplusplus
}
#endif

#endif
