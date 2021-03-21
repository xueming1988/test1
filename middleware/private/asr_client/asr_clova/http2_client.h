
#ifndef __HTTP2_CLIENT_H__
#define __HTTP2_CLIENT_H__

#include <nghttp2/nghttp2.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include <pthread.h>

#define HOST_URL ("prod-ni-cic.clova.ai")
#define HOST_PORT (443)
#define HOST_PORT_STR ("443")

#define HTTP_GET ("GET")
#define HTTP_POST ("POST")
#define HEADER_HTTP2_0 ("HTTP/2.0")

typedef struct _http2_conn_s {
    uint16_t want_io;
    int32_t timeout_cnt;
    int32_t ssl_fd;
    SSL *ssl;
    SSL_CTX *ssl_ctx;
    nghttp2_session *session;
    int continued;
    unsigned long long last_request_id;
} http2_conn_t;

typedef size_t(request_cb_t)(void *ptr, size_t size, size_t nmemb, void *ctx);

typedef struct _http2_request_s {
    int32_t http_ret;
    int32_t body_left;
    int32_t body_len;
    char *method;
    char *path;
    char *authorization;
    char *user_agent;
    char *content_type;
    char *body;
    request_cb_t *upload_cb;
    void *upload_data;
    request_cb_t *recv_head_cb;
    void *recv_head;
    request_cb_t *recv_body_cb;
    void *recv_body;
    unsigned long long request_id;
} http2_request_t;

#ifdef __cplusplus
extern "C" {
#endif

http2_conn_t *http2_conn_create(char *host, uint16_t port, long conn_tmout);

int http2_conn_destroy(http2_conn_t **conn);

int http2_conn_keep_alive(http2_conn_t *conn, http2_request_t *request, int timeout);

int http2_conn_cancel_upload(void);

int http2_conn_exit(void);

int http2_conn_do_request(http2_conn_t *conn, http2_request_t *request, int timeout);

#ifdef __cplusplus
}
#endif

#endif
