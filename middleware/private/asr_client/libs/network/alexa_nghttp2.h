#ifndef __ALEXA_NGHTTP2_H__
#define __ALEXA_NGHTTP2_H__

#include <nghttp2/nghttp2.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include <pthread.h>

/*
:method = POST
:scheme = https
:path = /{{API version}}/events
authorization = Bearer {{YOUR_ACCESS_TOKEN}}
content-type = multipart/form-data; boundary={{BOUNDARY_TERM_HERE}}

--{{BOUNDARY_TERM_HERE}}
Content-Disposition: form-data; name="metadata"
Content-Type: application/json; charset=UTF-8

{
    "context": [

    ],
    "event": {
        "header": {
            "namespace": "System",
            "name": "SynchronizeState",
            "messageId": "{{STRING}}"
        },
        "payload": {
        }
    }
}

--{{BOUNDARY_TERM_HERE}}--

*/

#ifdef BAIDU_DUMI

#if defined(ASR_DCS_MODULE)
#define ALEXA_PING_PATH ("/dcs/v1/ping")
#define ALEXA_EVENTS_PATH ("/dcs/v1/events")
#define ALEXA_DIRECTIVES_PATH ("/dcs/v1/directives")
#else
#define ALEXA_PING_PATH ("/ping")
#define ALEXA_EVENTS_PATH ("/dcs/avs-compatible-v20160207/events")
#define ALEXA_DIRECTIVES_PATH ("/dcs/avs-compatible-v20160207/directives")
#endif

#define ALEXA_HOST_URL ("dueros-h2.baidu.com")

#elif defined(TENCENT)

#define ALEXA_PING_PATH ("/ping")
#define ALEXA_EVENTS_PATH ("/v20160207/events")
#define ALEXA_DIRECTIVES_PATH ("/v20160207/directives")
#define ALEXA_HOST_URL ("tvs.html5.qq.com")

#else

#define ALEXA_PING_PATH ("/ping")
#define ALEXA_EVENTS_PATH ("/v20160207/events")

#define ALEXA_MRM_EVENTS_PATH ("/v20160207/cluster/events")

#define ALEXA_DIRECTIVES_PATH ("/v20160207/directives")
#define ALEXA_HOST_URL ("avs-alexa-na.amazon.com")

#endif

#define ALEXA_HOST_PORT (443)
#define ALEXA_HOST_PORT_STR ("443")

#if defined(ASR_DCS_MODULE)
#define ALEXA_EVENT_CONTENTTYPE ("multipart/form-data; boundary=this-is-a-boundary")
#else
#define ALEXA_EVENT_CONTENTTYPE ("multipart/form-data; boundary=--abcdefg123456")
#endif

#define ALEXA_GET ("GET")
#define ALEXA_POST ("POST")

#if defined(ASR_DCS_MODULE)
#define ALEXA_EVENT_PART                                                                           \
    ("--this-is-a-boundary\r\nContent-Disposition: form-data; name=\"metadata\"\r\nContent-Type: " \
     "application/json; charset=UTF-8\r\n\r\n%s\r\n--this-is-a-boundary--")
#define ALEXA_RECON_PART                                                                           \
    ("--this-is-a-boundary\r\nContent-Disposition: form-data; name=\"metadata\"\r\nContent-Type: " \
     "application/json; "                                                                          \
     "charset=UTF-8\r\n\r\n%s\r\n\r\n--this-is-a-boundary\r\nContent-Disposition: form-data; "     \
     "name=\"audio\"\r\nContent-Type: application/octet-stream\r\n\r\n")
#define ALEXA_RECON_PART_END ("\r\n--this-is-a-boundary--")
#define ALEXA_PING_PART ("PING")
#else
#define ALEXA_EVENT_PART                                                                           \
    ("----abcdefg123456\r\nContent-Disposition: form-data; name=\"metadata\"\r\nContent-Type: "    \
     "application/json; charset=UTF-8\r\n\r\n%s\r\n----abcdefg123456--\r\n")
#define ALEXA_RECON_PART                                                                           \
    ("----abcdefg123456\r\nContent-Disposition: form-data; name=\"metadata\"\r\nContent-Type: "    \
     "application/json; charset=UTF-8\r\n\r\n%s\r\n\r\n----abcdefg123456\r\nContent-Disposition: " \
     "form-data; name=\"audio\"\r\nContent-Type: application/octet-stream\r\n\r\n")
#define ALEXA_RECON_PART_END ("\r\n----abcdefg123456--")
#endif

#define ALEXA_MAX_TIMEOU_CNT (5)

typedef struct _alexa_conn_s {
    uint16_t want_io;
    int32_t timeout_cnt;
    int32_t ssl_fd;
    SSL *ssl;
    SSL_CTX *ssl_ctx;
    nghttp2_session *session;
} alexa_conn_t;

typedef size_t(request_cb_t)(void *ptr, size_t size, size_t nmemb, void *ctx);

typedef size_t(upload_cb_t)(void *ptr, void *con_ctx, size_t size, void *ctx);

typedef struct _alexa_request_s {
    int32_t http_ret;
    int32_t body_left;
    int32_t body_len;
    char *method;
    char *path;
    char *authorization;
    char *content_type;
    char *body;
    upload_cb_t *upload_cb;
    void *upload_data;
    request_cb_t *recv_head_cb;
    void *recv_head;
    request_cb_t *recv_body_cb;
    void *recv_body;
} alexa_request_t;

#ifdef __cplusplus
extern "C" {
#endif

alexa_conn_t *AlexaNghttp2Create(char *host, uint16_t port, long conn_tmout);

int AlexaNghttp2Destroy(alexa_conn_t **conn);

void AlexaNghttp2CheckRead(alexa_conn_t *conn, int timeout_ms);

int AlexaNghttp2Run(alexa_conn_t *conn, alexa_request_t *request, int timeout);

int AlexaNghttp2SendPing(void);

int AlexaNghttp2Cancel(void);

int AlexaNghttp2Exit(void);

int AlexaNghttp2WaitFin(alexa_conn_t *conn, alexa_request_t *request, int timeout);

#ifdef __cplusplus
}
#endif

#endif
