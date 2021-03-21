
#ifndef __ASR_CONNECT_H__
#define __ASR_CONNECT_H__

enum {
    e_none = -1,
    e_ping,
    e_downchannel,
    e_recognize,
    e_event,
    e_tts,

    e_max,
};

enum {
    e_io_none,
    e_io_read,
    e_io_write,
};

enum {
    e_asr_none = -1,
    e_asr_clova,
    e_asr_lguplus,

    e_asr_max,
};

typedef struct _ring_buffer_s ring_buffer_t;
typedef struct _asr_connect_s asr_connect_t;

#ifdef __cplusplus
extern "C" {
#endif

asr_connect_t *asr_connect_create(int asr_type, ring_buffer_t *ring_buffer);

void asr_connect_destroy(asr_connect_t **self_p, int asr_type);

int asr_connect_init(asr_connect_t *self, const char *host_url, int port, long conn_time_out);

int asr_connect_io_state(asr_connect_t *self);

int asr_connect_write(asr_connect_t *self);

int asr_connect_read(asr_connect_t *self);

int asr_connect_cancel_stream(asr_connect_t *self, int stream_id);

int asr_connect_can_do_request(asr_connect_t *self);

int asr_connect_submit_request(asr_connect_t *self, int request_type, char *state_json,
                               char *cmd_js_str, int time_out);

int asr_connect_check_time_out(asr_connect_t *self);

#ifdef __cplusplus
}
#endif

#endif
