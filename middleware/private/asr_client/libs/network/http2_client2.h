
#ifndef __HTTP2_CLIENT_H2__
#define __HTTP2_CLIENT_H2__

#ifdef __cplusplus
extern "C" {
#endif

http2_conn_t *http2_conn_create2(char *host, uint16_t port, long conn_tmout);

int http2_conn_destroy2(http2_conn_t **conn);

int http2_conn_keep_alive2(http2_conn_t *conn, http2_request_t *request, int timeout);

int http2_conn_cancel_upload2(void);

int http2_conn_ping2(void);

int http2_conn_exit2(void);

int http2_conn_do_request2(http2_conn_t *conn, http2_request_t *request, int timeout);

#ifdef __cplusplus
}
#endif

#endif
