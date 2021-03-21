/*
 * This program is written to show how to use nghttp2 API in C and
 * intentionally made simple.
 */

#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <semaphore.h>

#include "wm_util.h"
#include "lp_list.h"
#include "http2_client.h"

#define NGHTTP2_FIFO ("/tmp/nghttp2_fifo")
#define MAX_TIMEOU_CNT (2)
#define REQUEST_INSERT ("insert")
#define CONN_QUIT ("quit")
#define CONN_PING ("ping")

#ifdef LTE_MODULE
#define SOCKET_KEEPIDLE (60 * 28)
#define SOCKET_KEEPINTVL (60)
#else
#define SOCKET_KEEPIDLE (60)
#define SOCKET_KEEPINTVL (60)
#endif

#ifndef USED_CERT
#define USED_CERT
#endif

#ifdef USED_CERT
#define CERT_FILE ("/etc/ssl/certs/ca-certificates.crt")
#define KEY_FILE ("/system/workdir/misc/key.pem")
#endif

#define FILE_EXSIT(file) ((file) ? ((access(file, 0)) == 0) : 0)

enum { IO_NONE, WANT_READ, WANT_WRITE };

#define MAKE_NV(NAME, VALUE)                                                                       \
    {                                                                                              \
        (uint8_t *)NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, sizeof(VALUE) - 1,                    \
            NGHTTP2_NV_FLAG_NONE                                                                   \
    }

#define MAKE_NV_CS(NAME, VALUE)                                                                    \
    {                                                                                              \
        (uint8_t *)NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, strlen(VALUE), NGHTTP2_NV_FLAG_NONE   \
    }

#ifndef WAIT_TIME_OUT
#define WAIT_TIME_OUT (1)
#endif

enum {
    REQUEST_IDLE,
    REQUEST_ADDED,
    REQUEST_EXIT,
    REQUEST_NONE,
};

typedef struct _req_item_s {
    int status;
    int32_t stream_id;
    nghttp2_data_provider data_prd;
    pthread_mutex_t req_mutex;
    sem_t req_sem;
    http2_request_t *request;
    struct lp_list_head list;
    long long last_time_ms;
    long long time_out_ms;
} req_item_t;

static int g_ssl_inited = 0;
static LP_LIST_HEAD(g_request_list);
static pthread_mutex_t g_mutex_request_list = PTHREAD_MUTEX_INITIALIZER;
#define request_list_lock() pthread_mutex_lock(&g_mutex_request_list)
#define request_list_unlock() pthread_mutex_unlock(&g_mutex_request_list)
extern WIIMU_CONTEXT *g_wiimu_shm;

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#if (NGHTTP2_VERSION_NUM >= 0x010900)
/* nghttp2_session_callbacks_set_error_callback is present in nghttp2 1.9.0 or
   later */
#define NGHTTP2_HAS_ERROR_CALLBACK 1
#else
#define nghttp2_session_callbacks_set_error_callback(x, y)
#endif

#define NGHTTP2_DEBUG_TRACE
#if defined(NGHTTP2_DEBUG_INFO)
#define Debug_Info(fmt, ...)                                                                       \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d][%s:%d]" fmt, (int)(tickSincePowerOn() / 3600000),   \
                (int)(tickSincePowerOn() % 3600000 / 60000),                                       \
                (int)(tickSincePowerOn() % 60000 / 1000), (int)(tickSincePowerOn() % 1000),        \
                __func__, __LINE__, ##__VA_ARGS__);                                                \
    } while (0)
#else
#define Debug_Info(fmt, ...)
#endif

#if defined(NGHTTP2_DEBUG_TRACE) || defined(NGHTTP2_DEBUG_INFO)
#define Debug_Trace(fmt, ...)                                                                      \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d][%s:%d]" fmt, (int)(tickSincePowerOn() / 3600000),   \
                (int)(tickSincePowerOn() % 3600000 / 60000),                                       \
                (int)(tickSincePowerOn() % 60000 / 1000), (int)(tickSincePowerOn() % 1000),        \
                __func__, __LINE__, ##__VA_ARGS__);                                                \
    } while (0)
#else
#define Debug_Trace(fmt, ...)
#endif

#if defined(NGHTTP2_DEBUG_TRACE) || defined(NGHTTP2_DEBUG_INFO) || defined(NGHTTP2_DEBUG_ERROR)
#define Debug_Error(fmt, ...)                                                                      \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d][%s:%d]" fmt, (int)(tickSincePowerOn() / 3600000),   \
                (int)(tickSincePowerOn() % 3600000 / 60000),                                       \
                (int)(tickSincePowerOn() % 60000 / 1000), (int)(tickSincePowerOn() % 1000),        \
                __func__, __LINE__, ##__VA_ARGS__);                                                \
    } while (0)
#else
#define Debug_Error(fmt, ...)
#endif

static int submit_request(http2_conn_t *conn, req_item_t *req);

static int create_local_sock(void)
{
    int server_fd = -1;
    int res = -1;
    int flags = 0;
    int rv = 0;

    // start server side
    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(struct sockaddr_un));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, NGHTTP2_FIFO, sizeof(server_addr.sun_path) - 1);
    unlink(NGHTTP2_FIFO);
    server_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (server_fd < 0) {
        Debug_Error("Fail to create socket: error(%s)\n", strerror(errno));
        return res;
    }
    res = bind(server_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un));
    if (res) {
        Debug_Error("Fail to start hub server: error(%s)\n", strerror(errno));
        close(server_fd);
        return res;
    }

    return server_fd;
}

static int local_sock_send(char *buf, size_t len)
{
    int cliet_fd = -1;
    int size = -1;

    // initialize client size
    struct sockaddr_un client_addr;

    client_addr.sun_family = AF_UNIX;
    strncpy(client_addr.sun_path, NGHTTP2_FIFO, sizeof(client_addr.sun_path) - 1);
    cliet_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (cliet_fd == -1) {
        Debug_Error("Fail to create socket: error(%s)\n", strerror(errno));
        return -1;
    }

    size =
        sendto(cliet_fd, buf, len, 0, (struct sockaddr *)&client_addr, sizeof(struct sockaddr_un));
    if (size <= 0) {
        Debug_Error("Fail to send: error(%s)\n", strerror(errno));
        size = -1;
    }

    close(cliet_fd);
    return size;
}

static void remove_request(req_item_t **req_item)
{
    req_item_t *req = *req_item;

    if (req) {
        sem_destroy(&req->req_sem);
        pthread_mutex_destroy(&req->req_mutex);

        free(req);
        *req_item = NULL;
    }
}

static req_item_t *create_request(http2_request_t *req, int time_out)
{
    req_item_t *req_item = NULL;

    if (!req) {
        Debug_Error("multi handle is NULL\n");
        return NULL;
    }

    // need to free!!!!
    Debug_Info("start ++++++++++\n");

    req_item = (req_item_t *)calloc(1, sizeof(req_item_t));
    if (req_item) {
        req_item->status = REQUEST_IDLE;
        req_item->last_time_ms = 0;
        req_item->time_out_ms = (time_out > 0) ? ((long long)time_out * 1000) : 0;
        req_item->request = req;
        pthread_mutex_init(&req_item->req_mutex, NULL);
        sem_init(&req_item->req_sem, 0, 0);
    } else {
        Debug_Error("create an event error(no mem)\n");
        return NULL;
    }

    Debug_Info("end -----------\n");

    return req_item;
}

static void debug_session_status(http2_conn_t *connection, req_item_t *req_item, int flags)
{
    if (connection->session) {
        int want_read = nghttp2_session_want_read(connection->session);
        int want_write = nghttp2_session_want_write(connection->session);
        int queue_size = nghttp2_session_get_outbound_queue_size(connection->session);
        int32_t last_stream_id = nghttp2_session_get_last_proc_stream_id(connection->session);
        int32_t stream_effect_recv_len = nghttp2_session_get_stream_effective_recv_data_length(
            connection->session, req_item->stream_id);
        int32_t stream_local_window_size = nghttp2_session_get_stream_effective_local_window_size(
            connection->session, req_item->stream_id);
        int32_t effect_recv_len =
            nghttp2_session_get_effective_recv_data_length(connection->session);
        int32_t local_window_size =
            nghttp2_session_get_effective_local_window_size(connection->session);
        int32_t stream_remote_window_size =
            nghttp2_session_get_stream_remote_window_size(connection->session, req_item->stream_id);
        int32_t remote_window_size = nghttp2_session_get_remote_window_size(connection->session);

        Debug_Trace("streamid_id(%d) is_time_out(%d) status(%d)"
                    "want_write(%d)/want_read(%d) want_io(%d)"
                    "queue_size(%d) last_stream_id(%d) (%d %d %d %d %d %d)\n",
                    req_item->stream_id, flags, req_item->status, want_write, want_read,
                    connection->want_io, queue_size, last_stream_id, stream_effect_recv_len,
                    stream_local_window_size, effect_recv_len, local_window_size,
                    stream_remote_window_size, remote_window_size);
    }
}

static int add_request(http2_conn_t *connection)
{
    int ret = 0;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    req_item_t *req_item = NULL;

    Debug_Info("start ++++++++++\n");

    request_list_lock();
    if (!lp_list_empty(&g_request_list)) {
        lp_list_for_each_safe(pos, npos, &g_request_list)
        {
            req_item = lp_list_entry(pos, req_item_t, list);
            if (req_item) {
                pthread_mutex_lock(&req_item->req_mutex);
                if (req_item->request && req_item->status == REQUEST_IDLE) {
                    if (nghttp2_session_check_request_allowed(connection->session)) {
                        submit_request(connection, req_item);
                        debug_session_status(connection, req_item, 0);
                        // Debug_Trace("insert stream_id(%d)\n", req_item->stream_id);
                        req_item->last_time_ms = tickSincePowerOn();
                        req_item->status = REQUEST_ADDED;
                        ret = 0;
                    } else {
                        Debug_Error("nghttp2_session_check_request_allowed failed\n");
                        ret = -1;
                    }
                    pthread_mutex_unlock(&req_item->req_mutex);
                    break;
                }
                pthread_mutex_unlock(&req_item->req_mutex);
            }
        }
    }
    request_list_unlock();

    Debug_Info("end ---------------\n");

    return ret;
}

static void clear_request(void)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    req_item_t *req_item = NULL;

    Debug_Info("start ++++++++++\n");

    request_list_lock();
    if (!lp_list_empty(&g_request_list)) {
        lp_list_for_each_safe(pos, npos, &g_request_list)
        {
            req_item = lp_list_entry(pos, req_item_t, list);
            if ((req_item && req_item->status >= REQUEST_ADDED) ||
                (req_item && req_item->request && req_item->request->upload_cb)) {
                sem_post(&req_item->req_sem);
            }
        }
    }
    request_list_unlock();

    Debug_Info("end ----------\n");
}

static void request_exit(req_item_t *req_item)
{
    if (req_item) {
        sem_post(&req_item->req_sem);
    }
}

static req_item_t *need_remove_request(http2_conn_t *connection, nghttp2_session *session,
                                       int32_t stream_id, int force_remove)
{
    int found = 0;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    req_item_t *req_item = NULL;
    req_item_t *req_item_find = NULL;

    Debug_Info("start ++++++++++\n");

    if (stream_id < 0 || !connection || !session) {
        Debug_Error("stream_id = %d connection = %p session = %p\n", stream_id, connection,
                    session);
        return NULL;
    }

    req_item = (req_item_t *)nghttp2_session_get_stream_user_data(session, stream_id);
    if (req_item && stream_id > 1) {
        request_list_lock();
        if (!lp_list_empty(&g_request_list)) {
            lp_list_for_each_safe(pos, npos, &g_request_list)
            {
                req_item_find = lp_list_entry(pos, req_item_t, list);
                if (req_item_find && req_item_find->stream_id == stream_id &&
                    req_item == req_item_find) {
                    found = 1;
                    break;
                }
            }
        }
        request_list_unlock();

        if (found == 0) {
            Debug_Trace("the stream_id = %d is already removed\n", stream_id);
            nghttp2_session_set_stream_user_data(session, stream_id, NULL);
            return NULL;
        }
    }

    if (req_item && force_remove == 1) {
        Debug_Trace("exit stream_id = %d\n", stream_id);
        nghttp2_session_set_stream_user_data(session, stream_id, NULL);
        request_exit(req_item);

        return NULL;
    }

    // if(req_item) {
    //    req_item->last_time_ms = tickSincePowerOn();
    //}

    Debug_Info("end ----------\n");

    return req_item;
}

static int check_all_timeout(http2_conn_t *connection, int remove_old, int flags)
{
    int ret = 0;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    req_item_t *req_item = NULL;
    long long time_now = 0;

    if (!connection) {
        Debug_Trace("params err : connection is NULL\n");
        return -1;
    }

    time_now = tickSincePowerOn();
    request_list_lock();
    if (!lp_list_empty(&g_request_list)) {
        lp_list_for_each_safe(pos, npos, &g_request_list)
        {
            req_item = lp_list_entry(pos, req_item_t, list);
            if (req_item && req_item->status < REQUEST_EXIT && req_item->time_out_ms > 0 &&
                req_item->stream_id > 0) {
                if (time_now - req_item->last_time_ms > req_item->time_out_ms) {
                    debug_session_status(connection, req_item, flags);
                    connection->timeout_cnt++;
                    sem_post(&req_item->req_sem);
                }
            } else if (req_item && req_item->status == REQUEST_EXIT) {
                if (0 == remove_old) {
                    Debug_Trace("remove event_request(%p) streamid id(%d) status(%d)\n", req_item,
                                req_item->stream_id, req_item->status);
                    if (connection->session && req_item->stream_id > 0) {
                        ret = nghttp2_submit_rst_stream(connection->session, NGHTTP2_FLAG_NONE,
                                                        req_item->stream_id, NGHTTP2_STREAM_CLOSED);
                        Debug_Info("nghttp2_submit_rst_stream stream_id = %d %s\n",
                                   req_item->stream_id, (ret == 0) ? "OK" : "Failed");
                        void *data = nghttp2_session_get_stream_user_data(connection->session,
                                                                          req_item->stream_id);
                        if (data) {
                            ret = nghttp2_session_set_stream_user_data(connection->session,
                                                                       req_item->stream_id, NULL);
                            Debug_Info("nghttp2_session_set_stream_user_data %s\n",
                                       (ret == 0) ? "OK" : "Failed");
                        }
                    }
                }
                lp_list_del(&req_item->list);
                remove_request(&req_item);
            }
        }
    }
    request_list_unlock();

    return ret;
}

/*
 * The implementation of nghttp2_send_callback type. Here we write
 * |data| with size |length| to the network and return the number of
 * bytes actually written. See the documentation of
 * nghttp2_send_callback for the details.
 */
static ssize_t send_callback(nghttp2_session *session, const uint8_t *data, size_t length,
                             int flags, void *user_data)
{
    int rv = 0;
    http2_conn_t *connection = (http2_conn_t *)user_data;

    if (connection) {
        connection->want_io = IO_NONE;
        ERR_clear_error();
        rv = SSL_write(connection->ssl, data, (int)length);
        if (rv <= 0) {
            int err = SSL_get_error(connection->ssl, rv);
            if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ) {
                connection->want_io = (err == SSL_ERROR_WANT_READ ? WANT_READ : WANT_WRITE);
                rv = NGHTTP2_ERR_WOULDBLOCK;
            } else {
                Debug_Trace("SSL_write error rv=%d err=%d errno(%d) errnostr(%s)\n", rv, err, errno,
                            strerror(errno));
                rv = NGHTTP2_ERR_CALLBACK_FAILURE;
            }
        }
    }
    return rv;
}

/*
 * The implementation of nghttp2_recv_callback type. Here we read data
 * from the network and write them in |buf|. The capacity of |buf| is
 * |length| bytes. Returns the number of bytes stored in |buf|. See
 * the documentation of nghttp2_recv_callback for the details.
 */
static ssize_t recv_callback(nghttp2_session *session, uint8_t *buf, size_t length, int flags,
                             void *user_data)
{
    int rv = 0;
    http2_conn_t *connection = (http2_conn_t *)user_data;

    if (connection) {
        connection->want_io = IO_NONE;
        ERR_clear_error();
        memset(buf, 0x0, length);
        rv = SSL_read(connection->ssl, buf, (int)length);
        if (rv < 0) {
            int err = SSL_get_error(connection->ssl, rv);
            if (err == SSL_ERROR_WANT_WRITE || err == SSL_ERROR_WANT_READ) {
                connection->want_io = (err == SSL_ERROR_WANT_READ ? WANT_READ : WANT_WRITE);
                rv = NGHTTP2_ERR_WOULDBLOCK;
            } else {
                Debug_Trace("SSL_read error rv=%d err=%d errno(%d) errnostr(%s)\n", rv, err, errno,
                            strerror(errno));
                rv = NGHTTP2_ERR_CALLBACK_FAILURE;
            }
        } else if (rv == 0) {
            rv = NGHTTP2_ERR_EOF;
        }
    }

    return rv;
}

static int on_frame_send_callback(nghttp2_session *session, const nghttp2_frame *frame,
                                  void *user_data)
{
    size_t i = 0;
    nghttp2_nv *nva = NULL;
    http2_conn_t *connection = (http2_conn_t *)user_data;

    need_remove_request(connection, session, frame->hd.stream_id, 0);
    switch (frame->hd.type) {
    case NGHTTP2_HEADERS:
        nva = frame->headers.nva;
        Debug_Info("C ----------------------------> S (HEADERS)\n");
        for (i = 0; i < frame->headers.nvlen; ++i) {
            Debug_Info("%s: %s\n", nva[i].name, nva[i].value);
        }
        break;
    case NGHTTP2_RST_STREAM:
        Debug_Info("C ----------------------------> S (RST_STREAM) stream_id(%u)\n",
                   frame->hd.stream_id);
        break;
    case NGHTTP2_GOAWAY:
        Debug_Trace("C ----------------------------> S (GOAWAY) stream_id(%u)\n",
                    frame->hd.stream_id);
        break;
    case NGHTTP2_PING:
        Debug_Trace("C ----------------------------> S (PING) stream_id(%u)\n",
                    frame->hd.stream_id);
        break;
    case NGHTTP2_WINDOW_UPDATE:
        Debug_Info("C ----------------------------> S (WINDOW UPDATE) stream_id(%u)\n",
                   frame->hd.stream_id);
        break;
    default:
        Debug_Info("Got frame type %x for stream %u!\n", frame->hd.type, frame->hd.stream_id);
        break;
    }

    return 0;
}

static int on_frame_recv_callback(nghttp2_session *session, const nghttp2_frame *frame,
                                  void *user_data)
{
    size_t i = 0;
    int rv = 0;
    nghttp2_nv *nva = NULL;
    http2_conn_t *connection = (http2_conn_t *)user_data;
    req_item_t *req_item = NULL;

    need_remove_request(connection, session, frame->hd.stream_id, 0);
    switch (frame->hd.type) {
    case NGHTTP2_HEADERS:
        if (frame->headers.cat == NGHTTP2_HCAT_RESPONSE) {
            nva = frame->headers.nva;
            Debug_Info("C <---------------------------- S (HEADERS) nvlen %d\n",
                       frame->headers.nvlen);
            for (i = 0; i < frame->headers.nvlen; ++i) {
                Debug_Info("%s: %s\n", nva[i].name, nva[i].value);
            }
        }
        break;
    case NGHTTP2_RST_STREAM:
        Debug_Info("C <---------------------------- S (RST_STREAM) stream_id(%u)\n",
                   frame->hd.stream_id);
        break;
    case NGHTTP2_GOAWAY:
        Debug_Trace("C <---------------------------- S (GOAWAY) stream_id(%u)\n",
                    frame->hd.stream_id);
        return NGHTTP2_ERR_EOF;
    case NGHTTP2_SETTINGS:
        /* stream ID zero is for connection-oriented stuff */
        if (frame->hd.stream_id == 0) {
            Debug_Info("Got SETTINGS\n");
            int max_concurrent_streams = nghttp2_session_get_remote_settings(
                session, NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS);
            int enable_push =
                nghttp2_session_get_remote_settings(session, NGHTTP2_SETTINGS_ENABLE_PUSH);
            int window_size =
                nghttp2_session_get_remote_settings(session, NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE);
            int frame_size =
                nghttp2_session_get_remote_settings(session, NGHTTP2_SETTINGS_MAX_FRAME_SIZE);
            Debug_Info("frame->hd.stream_id == %d\n", frame->hd.stream_id);
            Debug_Info("MAX_CONCURRENT_STREAMS == %d\n", max_concurrent_streams);
            Debug_Info("ENABLE_PUSH == %s\n", enable_push ? "TRUE" : "false");
            Debug_Info("NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE == %d\n", window_size);
            Debug_Info("NGHTTP2_SETTINGS_MAX_FRAME_SIZE == %d\n", frame_size);
        }
        break;
    case NGHTTP2_PING:
        Debug_Trace("C <---------------------------- S (PING) stream_id(%u)\n",
                    frame->hd.stream_id);
        connection->ping_time = 0;
        break;
    case NGHTTP2_WINDOW_UPDATE:
        Debug_Info("C <---------------------------- S (WINDOW UPDATE) stream_id(%u)\n",
                   frame->hd.stream_id);
        break;
    default:
        Debug_Info("Got frame type %x for stream %u!\n", frame->hd.type, frame->hd.stream_id);
        break;
    }

    return 0;
}

/*
 * The implementation of nghttp2_on_stream_close_callback type. We use
 * this function to know the response is fully received. Since we just
 * fetch 1 resource in this program, after reception of the response,
 * we submit GOAWAY and close the session.
 */
static int on_stream_close_callback(nghttp2_session *session, int32_t stream_id,
                                    uint32_t error_code, void *user_data)
{
    int rv = 0;
    http2_conn_t *connection = (http2_conn_t *)user_data;
    req_item_t *req_item = NULL;

#if defined(TENCENT)
    if (2 >= stream_id) {
#else
    if (0 == stream_id) {
#endif
        Debug_Error("stream_id = %d error_code = %d; The ssl connect will be colsed at %lld \n",
                    stream_id, error_code, tickSincePowerOn());
        rv = nghttp2_session_terminate_session(session, NGHTTP2_NO_ERROR);
        if (rv != 0) {
            Debug_Error("%s\n", nghttp2_strerror(rv));
        }
    } else {
        Debug_Error("Close stream_id = %d error_code = %d at %lld \n", stream_id, error_code,
                    tickSincePowerOn());
        req_item = need_remove_request(connection, session, stream_id, 1);
        if (NULL == req_item) {
            return 0;
        }
    }

    return rv;
}

/*
 * The implementation of nghttp2_on_data_chunk_recv_callback type. We
 * use this function to print the received response body.
 */
static int on_data_chunk_recv_callback(nghttp2_session *session, uint8_t flags, int32_t stream_id,
                                       const uint8_t *data, size_t len, void *user_data)
{
    int rv = 0;
    req_item_t *req_item = NULL;
    http2_conn_t *connection = (http2_conn_t *)user_data;

    req_item = need_remove_request(connection, session, stream_id, 0);
    if (NULL == req_item) {
        Debug_Error("request stream_id(%d) have been removed?????\n", stream_id);
        return 0;
    }

    pthread_mutex_lock(&req_item->req_mutex);
    if (req_item && req_item->request && req_item->request->recv_body_cb &&
        req_item->request->recv_body) {
        Debug_Info("C <---------------------------- S (DATA chunk)\n %lu bytes\n",
                   (unsigned long int)len);
        req_item->request->recv_body_cb(data, 1, len, req_item->request->recv_body);
    }
    pthread_mutex_unlock(&req_item->req_mutex);

    return 0;
}

static int on_invalid_frame_recv(nghttp2_session *session, const nghttp2_frame *frame,
                                 int lib_error_code, void *user_data)
{
    http2_conn_t *connection = (http2_conn_t *)user_data;

    Debug_Trace("[TICK]=====stream_id(%d) error=%d:%s\n", frame->hd.stream_id, lib_error_code,
                nghttp2_strerror(lib_error_code));

#if 0
    /* Issue RST_STREAM so that stream does not hang around. */
    if(connection && connection->session && frame->hd.stream_id > 1) {
        nghttp2_stream *stream = NULL;
        stream = nghttp2_session_find_stream(connection->session, frame->hd.stream_id);
        nghttp2_stream_proto_state state = nghttp2_stream_get_state(stream);
        if(state > NGHTTP2_STREAM_STATE_OPEN) {
            //nghttp2_submit_rst_stream(connection->session, NGHTTP2_FLAG_NONE, \
            //                      frame->hd.stream_id, NGHTTP2_STREAM_CLOSED);
            need_remove_request(connection, connection->session, \
                                frame->hd.stream_id, 1);
        }
    }
#endif

    return 0;
}

static int before_frame_send(nghttp2_session *session, const nghttp2_frame *frame, void *user_data)
{
    int rv = 0;
    http2_conn_t *connection = (http2_conn_t *)user_data;
    req_item_t *req_item = NULL;

    req_item = need_remove_request(connection, session, frame->hd.stream_id, 0);
    if (NULL == req_item) {
        if (frame->hd.stream_id > 0) {
            Debug_Error("request stream_id(%d) have been removed\n", frame->hd.stream_id);
        }
        return 0;
    }

    Debug_Error("stream_id(%d) before send\n", frame->hd.stream_id);

    return 0;
}

static int on_frame_not_send(nghttp2_session *session, const nghttp2_frame *frame,
                             int lib_error_code, void *user_data)
{
    http2_conn_t *connection = (http2_conn_t *)user_data;

    Debug_Trace("[TICK]=====stream_id(%d) error=%d:%s\n", frame->hd.stream_id, lib_error_code,
                nghttp2_strerror(lib_error_code));

#if 0
    /* Issue RST_STREAM so that stream does not hang around. */
    if(connection && connection->session && frame->hd.stream_id > 1) {
        nghttp2_stream *stream = NULL;
        stream = nghttp2_session_find_stream(connection->session, frame->hd.stream_id);
        nghttp2_stream_proto_state state = nghttp2_stream_get_state(stream);
        if(state > NGHTTP2_STREAM_STATE_OPEN) {
            //nghttp2_submit_rst_stream(connection->session, NGHTTP2_FLAG_NONE, \
            //                      frame->hd.stream_id, NGHTTP2_STREAM_CLOSED);
            need_remove_request(connection, connection->session, \
                                frame->hd.stream_id, 1);
        }
    }
#endif

    return 0;
}

static int on_begin_headers(nghttp2_session *session, const nghttp2_frame *frame, void *user_data)
{
    int rv = 0;
    http2_conn_t *connection = (http2_conn_t *)user_data;
    req_item_t *req_item = NULL;

    req_item = need_remove_request(connection, session, frame->hd.stream_id, 0);
    if (NULL == req_item) {
        Debug_Error("request stream_id(%d) have been removed?????\n", frame->hd.stream_id);
        return 0;
    }

    //Debug_Error("[TICK]=====stream_id(%d) %lld\n", \
    //    frame->hd.stream_id, tickSincePowerOn());

    if (frame->hd.type != NGHTTP2_HEADERS) {
        return 0;
    }

    return 0;
}

/* Decode HTTP status code.  Returns -1 if no valid status code was
   decoded. */
static int decode_status_code(const uint8_t *value, size_t len)
{
    int i = 0;
    int res = -1;

    if (len != 3) {
        return -1;
    }

    res = 0;
    for (i = 0; i < 3; ++i) {
        char c = value[i];
        if (c < '0' || c > '9') {
            return -1;
        }
        res *= 10;
        res += c - '0';
    }

    return res;
}

/* frame->hd.type is either NGHTTP2_HEADERS or NGHTTP2_PUSH_PROMISE */
static int on_header(nghttp2_session *session, const nghttp2_frame *frame, const uint8_t *name,
                     size_t namelen, const uint8_t *value, size_t valuelen, uint8_t flags,
                     void *userp)
{
    int rv = 0;
    req_item_t *req_item = NULL;
    int32_t stream_id = frame->hd.stream_id;
    http2_conn_t *connection = (http2_conn_t *)userp;
    (void)flags;

    req_item = need_remove_request(connection, session, frame->hd.stream_id, 0);
    if (NULL == req_item) {
        Debug_Error("request stream_id(%d) have been removed?????\n", frame->hd.stream_id);
        return 0;
    }

    /* Store received PUSH_PROMISE headers to be used when the subsequent
     PUSH_PROMISE callback comes */
    if (frame->hd.type == NGHTTP2_PUSH_PROMISE) {
        Debug_Error("NGHTTP2_PUSH_PROMISE\n");
        return 0;
    }

    if (namelen == sizeof(":status") - 1 && memcmp(":status", name, namelen) == 0) {
        /* nghttp2 guarantees :status is received first and only once, and
           value is 3 digits status code, and decode_status_code always
           succeeds. */
        pthread_mutex_lock(&req_item->req_mutex);
        if (req_item->request && req_item->request->recv_head_cb && req_item->request->recv_head) {
            req_item->request->http_ret = decode_status_code(value, valuelen);
            //Debug_Trace("[TICK]=====HTTP/2 %03d stream_id(%d) %lld ====== \n", \
            //    req_item->request->http_ret, stream_id, tickSincePowerOn());
            if ((req_item->request->http_ret == 200) || (req_item->request->http_ret == 204)) {
                if (connection && connection->timeout_cnt > 0) {
                    Debug_Trace("want_io(%d) timeout_cnt(%d) connection(%p)\n", connection->want_io,
                                connection->timeout_cnt, connection);
                    connection->timeout_cnt = 0;
                }
            }
            // "HTTP/2.0 200"
            char buf[64] = {0};
            snprintf(buf, sizeof(buf), "HTTP/2.0 %d\r\n", req_item->request->http_ret);
            req_item->request->recv_head_cb(buf, 1, strlen(buf), req_item->request->recv_head);
        }
        pthread_mutex_unlock(&req_item->req_mutex);
        return 0;
    } else {
        /* This is trailer fields. */
        /* 3 is for ":" and "\r\n". */
        uint32_t n = (uint32_t)(namelen + valuelen + 3);
        Debug_Info("h2 trailer: %.*s: %.*s\n", namelen, name, valuelen, value);
        pthread_mutex_lock(&req_item->req_mutex);
        if (req_item->request && req_item->request->recv_head_cb && req_item->request->recv_head) {
            char *buf = NULL;
            asprintf(&buf, "%s:%s\r\n", name, value);
            if (buf) {
                req_item->request->recv_head_cb(buf, 1, strlen(buf), req_item->request->recv_head);
                free(buf);
            }
        }
        pthread_mutex_unlock(&req_item->req_mutex);
        return 0;
    }

    Debug_Info("h2 header: %.*s: %.*s\n", namelen, name, valuelen, value);

    return 0; /* 0 is successful */
}

#ifdef NGHTTP2_HAS_ERROR_CALLBACK
static int error_callback(nghttp2_session *session, const char *msg, size_t len, void *userp)
{
    (void)userp;
    (void)session;
    Debug_Error("http2 error: %.*s\n", len, msg);
    return 0;
}
#endif

static ssize_t read_length_callback(nghttp2_session *session, uint8_t frame_type, int32_t stream_id,
                                    int32_t session_remote_window_size,
                                    int32_t stream_remote_window_size,
                                    uint32_t remote_max_frame_size, void *user_data)
{
    int read_length = 16384;
    req_item_t *req_item = NULL;
    http2_conn_t *connection = (http2_conn_t *)user_data;

    if (connection && session && stream_id > 1) {
        req_item = need_remove_request(connection, session, stream_id, 0);
        if (req_item) {
            pthread_mutex_lock(&req_item->req_mutex);
            if (req_item->request && req_item->request->body_left == 0 &&
                req_item->request->upload_cb && req_item->request->upload_data) {

                read_length = 320;
                if (session_remote_window_size < 2 * read_length ||
                    stream_remote_window_size < 2 * read_length ||
                    remote_max_frame_size < 2 * read_length) {
                    read_length = MIN(session_remote_window_size, stream_remote_window_size);
                    read_length = MIN(read_length, remote_max_frame_size);
                    Debug_Info("read_length = %d\n", read_length);
                }
            }
            pthread_mutex_unlock(&req_item->req_mutex);
        }
    }

    Debug_Info("session_remote_window_size = %d stream_remote_window_size = %d "
               "remote_max_frame_size = %d\n",
               session_remote_window_size, stream_remote_window_size, remote_max_frame_size);

    return read_length;
    // return 16384;
}

static ssize_t data_source_read_callback(nghttp2_session *session, int32_t stream_id, uint8_t *buf,
                                         size_t length, uint32_t *data_flags,
                                         nghttp2_data_source *source, void *user_data)
{
    int rv = 0;
    size_t nread = 0;
    req_item_t *req_item = NULL;
    http2_conn_t *connection = (http2_conn_t *)user_data;

    req_item = need_remove_request(connection, session, stream_id, 0);
    if (NULL == req_item) {
        Debug_Trace("request have been removed! length = %d, stream_id = %d\n", length, stream_id);
        return 0;
    }

    Debug_Info("---------------0 data_source_read_callback: length %zu bytes stream %u\n", length,
               stream_id);

    pthread_mutex_lock(&req_item->req_mutex);
    if (req_item && req_item->request && source) {
        nread = MIN(req_item->request->body_len, length);
        if (nread > 0) {
            Debug_Info("source->ptr: %sEND\n", source->ptr);
            memcpy(buf, source->ptr, nread);
            source->ptr = (uint8_t *)source->ptr + nread;
            req_item->request->body_len -= nread;
            req_item->request->body_left -= nread;
            Debug_Info("source->ptr: nread %zu bytes stream %u\n", nread, stream_id);
            pthread_mutex_unlock(&req_item->req_mutex);
            return nread;
        }
        if (req_item->request->body_left == 0) {
            if (req_item->request->upload_cb && req_item->request->upload_data) {
                nread =
                    req_item->request->upload_cb(buf, 1, length, req_item->request->upload_data);
                if (nread == 0) {
                    Debug_Info("data_source_read_callback: data_flags is set 1\n");
                    *data_flags = NGHTTP2_DATA_FLAG_EOF;
                }
                Debug_Info(
                    ">>>>>>>>>>>>>>>1 data_source_read_callback: returns %zu bytes stream %u\n",
                    nread, stream_id);
                pthread_mutex_unlock(&req_item->req_mutex);
                return nread;
            } else {
                Debug_Info("data_source_read_callback: data_flags is set 2\n");
                *data_flags = NGHTTP2_DATA_FLAG_EOF;
            }
        } else if (nread == 0) {
            pthread_mutex_unlock(&req_item->req_mutex);
            return NGHTTP2_ERR_DEFERRED;
        }
    } else {
        Debug_Info("data_source_read_callback: NGHTTP2_ERR_CALLBACK_FAILURE\n");
        pthread_mutex_unlock(&req_item->req_mutex);
        return NGHTTP2_ERR_CALLBACK_FAILURE;
    }
    pthread_mutex_unlock(&req_item->req_mutex);

    Debug_Info("++++++++++++++2 data_source_read_callback: returns %zu bytes stream %u\n", nread,
               stream_id);

    return nread;
}

/*
 * Setup callback functions. nghttp2 API offers many callback
 * functions, but most of them are optional. The send_callback is
 * always required. Since we use nghttp2_session_recv(), the
 * recv_callback is also required.
 */
static void setup_nghttp2_callbacks(nghttp2_session_callbacks *callbacks)
{
    /* nghttp2_send_callback */
    nghttp2_session_callbacks_set_send_callback(callbacks, send_callback);

    /* nghttp2_recv_callback */
    nghttp2_session_callbacks_set_recv_callback(callbacks, recv_callback);

    /* nghttp2_on_frame_send_callback */
    nghttp2_session_callbacks_set_on_frame_send_callback(callbacks, on_frame_send_callback);

    /* nghttp2_on_frame_recv_callback */
    nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks, on_frame_recv_callback);

    /* nghttp2_on_stream_close_callback */
    nghttp2_session_callbacks_set_on_stream_close_callback(callbacks, on_stream_close_callback);

    nghttp2_session_callbacks_set_data_source_read_length_callback(callbacks, read_length_callback);

    /* nghttp2_on_data_chunk_recv_callback */
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(callbacks,
                                                              on_data_chunk_recv_callback);

    /* nghttp2_on_invalid_frame_recv_callback */
    nghttp2_session_callbacks_set_on_invalid_frame_recv_callback(callbacks, on_invalid_frame_recv);

    /* nghttp2_before_frame_send_callback */
    nghttp2_session_callbacks_set_before_frame_send_callback(callbacks, before_frame_send);

    /* nghttp2_on_frame_not_send_callback */
    nghttp2_session_callbacks_set_on_frame_not_send_callback(callbacks, on_frame_not_send);

    /* nghttp2_on_begin_headers_callback */
    nghttp2_session_callbacks_set_on_begin_headers_callback(callbacks, on_begin_headers);

    /* nghttp2_on_header_callback */
    nghttp2_session_callbacks_set_on_header_callback(callbacks, on_header);

    /* nghttp2_on_error_callback */
    nghttp2_session_callbacks_set_error_callback(callbacks, error_callback);
}

/*
 * Callback function for TLS NPN. Since this program only supports
 * HTTP/2 protocol, if server does not offer HTTP/2 the nghttp2
 * library supports, we terminate program.
 */
static int select_next_proto_cb(SSL *ssl, unsigned char **out, unsigned char *outlen,
                                const unsigned char *in, unsigned int inlen, void *arg)
{
    int rv;
    (void)ssl;
    (void)arg;

    /* nghttp2_select_next_protocol() selects HTTP/2 protocol the
     nghttp2 library supports. */
    rv = nghttp2_select_next_protocol(out, outlen, in, inlen);
    if (rv <= 0) {
        Debug_Info("Server did not advertise HTTP/2 protocol");
    }
    return SSL_TLSEXT_ERR_OK;
}

static int make_non_block(int fd)
{
    int flags = 0;
    int rv = 0;

    while ((flags = fcntl(fd, F_GETFL, 0)) == -1 && errno == EINTR)
        ;
    if (flags == -1) {
        Debug_Error("fcntl error(%s)\n", strerror(errno));
        return -1;
    }

    while ((rv = fcntl(fd, F_SETFL, flags | O_NONBLOCK)) == -1 && errno == EINTR)
        ;
    if (rv == -1) {
        Debug_Error("fcntl error(%s)\n", strerror(errno));
        return -1;
    }

    return 0;
}

static int make_block(int fd)
{
    int flags = 0;
    int rv = 0;

    while ((flags = fcntl(fd, F_GETFL, 0)) == -1 && errno == EINTR)
        ;
    if (flags == -1) {
        Debug_Error("fcntl error(%s)\n", strerror(errno));
        return 0;
    }

    while ((rv = fcntl(fd, F_SETFL, flags & ~O_NONBLOCK)) == -1 && errno == EINTR)
        ;
    if (rv == -1) {
        Debug_Error("fcntl error(%s)\n", strerror(errno));
        return -1;
    }

    return 0;
}

static int set_tcp_nodelay(int fd)
{
    int val = 1;
    int rv = 0;
    int get_val = 0;
    socklen_t val_len = 0;

    getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&get_val, (socklen_t *)&val_len);
    Debug_Error("setsockopt TCP_NODELAY get_val(%d)\n", get_val);

    rv = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &val, (socklen_t)sizeof(val));
    if (rv == -1) {
        Debug_Error("setsockopt error(%s)\n", strerror(errno));
        return -1;
    }

    return 0;
}

static int set_tcp_keep_alive(int fd)
{
    int rv = 0;
    int keep_live = 1;
    int keep_idle = SOCKET_KEEPIDLE;
    int keep_interval = SOCKET_KEEPINTVL;

    rv = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void *)&keep_live, (socklen_t)sizeof(keep_live));
    if (rv == -1) {
        Debug_Error("setsockopt SO_KEEPALIVE error(%s)\n", strerror(errno));
        return -1;
    }
    rv = setsockopt(fd, SOL_TCP, TCP_KEEPIDLE, (void *)&keep_idle, (socklen_t)sizeof(keep_idle));
    if (rv == -1) {
        Debug_Error("setsockopt TCP_KEEPIDLE error(%s)\n", strerror(errno));
        return -1;
    }
    rv = setsockopt(fd, SOL_TCP, TCP_KEEPINTVL, (void *)&keep_interval,
                    (socklen_t)sizeof(keep_interval));
    if (rv == -1) {
        Debug_Error("setsockopt TCP_KEEPINTVL error(%s)\n", strerror(errno));
        return -1;
    }
}

static int set_tcp_sendbuf(int fd, int send_len)
{
    int val = 1;
    int rv = 0;

    if (send_len > 0) {
        getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (const char *)&send_len, sizeof(int));

        socklen_t send_buflen = 0;
        socklen_t len = sizeof(send_buflen);
        getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&send_buflen, &len);
        Debug_Trace("default,sendbuf:%d\n", send_buflen);

        send_buflen = send_len;
        rv = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (void *)&send_buflen, len);
        if (rv == -1) {
            Debug_Error("setsockopt error(%s)\n", strerror(errno));
            return -1;
        }
    }

    return 0;
}

/*
 * Connects to the host |host| and port |port|.  This function returns
 * the file descriptor of the client socket.
 */
static int connect_to(const char *host, uint16_t port, int conn_tmout)
{
    int fd = -1;
    int rv = 0;
    struct addrinfo hints;
    char service[NI_MAXSERV];
    struct addrinfo *res = NULL;
    struct addrinfo *rp = NULL;
    struct sockaddr_in *host_ip;
    struct timeval timeo = {0, 0};
    fd_set rset, wset;

    if (!host) {
        Debug_Error("host is NULL\n");
        return -1;
    }

    snprintf(service, sizeof(service), "%u", port);
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    Debug_Trace("+++++start at %lld\n", tickSincePowerOn());
    rv = getaddrinfo(host, service, &hints, &res);
    Debug_Trace("-----end at %lld\n", tickSincePowerOn());
    if (rv != 0) {
        Debug_Error("getaddrinfo:host:%s error(%s)\n", host, gai_strerror(rv));
        return -1;
    }
    for (rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1) {
            continue;
        }

        if (conn_tmout > 0) {
            make_non_block(fd);
            if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
                Debug_Info("connect nonblock connected \n");
                break;
            }
            if (errno != EINPROGRESS) {
                Debug_Error("nonblock connect error(%s)\n", strerror(errno));
                close(fd);
                fd = -1;
                break;
            }

            FD_ZERO(&rset);
            FD_SET(fd, &rset);
            wset = rset;
            timeo.tv_sec = conn_tmout;
            timeo.tv_usec = 0;
            rv = select(fd + 1, &rset, &wset, NULL, &timeo);
            if (rv == -1) {
                Debug_Error("select: error\n");
                close(fd);
                fd = -1;
                break;
            } else if (rv == 0) {
                Debug_Trace("select: nonblock connect time out %lld \n", tickSincePowerOn());
                close(fd);
                fd = -1;
                break;
            } else {
                Debug_Info("select: nonblock connect \n", __FUNCTION__, __LINE__);
                break;
            }
        } else {
            while ((rv = connect(fd, rp->ai_addr, rp->ai_addrlen)) == -1 && errno == EINTR)
                ;
            if (rv == 0) {
                Debug_Info("block connected \n");
                break;
            } else {
                close(fd);
                fd = -1;
                break;
            }
        }
    }

    if (res) {
        freeaddrinfo(res);
    }
    return fd;
}

/*
 * Setup SSL/TLS context.
 */
static void init_ssl_ctx(SSL_CTX *ssl_ctx)
{
    /* Disable SSLv2 and enable all workarounds for buggy servers */
    SSL_CTX_set_options(ssl_ctx, SSL_OP_ALL | SSL_OP_NO_SSLv2);
    SSL_CTX_set_mode(ssl_ctx, SSL_MODE_AUTO_RETRY);
    SSL_CTX_set_mode(ssl_ctx, SSL_MODE_RELEASE_BUFFERS);
    /* Set NPN callback */
    SSL_CTX_set_next_proto_select_cb(ssl_ctx, select_next_proto_cb, NULL);

#ifdef USED_CERT
    if (FILE_EXSIT(CERT_FILE)) {
        SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, NULL);
        SSL_CTX_load_verify_locations(ssl_ctx, CERT_FILE, NULL);
        SSL_CTX_use_certificate_file(ssl_ctx, CERT_FILE, SSL_FILETYPE_PEM);
        // SSL_CTX_use_PrivateKey_file(ssl_ctx, KEY_FILE, SSL_FILETYPE_PEM);
        // SSL_CTX_check_private_key(ssl_ctx);
    } else {
        Debug_Error("!!!!!!! %s is not exsit, connect to server without cert files\n", CERT_FILE);
    }
#endif
}

static int ssl_handshake(SSL *ssl, int fd)
{
    int rv = 0;

    if (fd > 0) {
        make_block(fd);
    }

    SSL_set_connect_state(ssl);
    if (SSL_set_fd(ssl, fd) == 0 || fd < 0) {
        Debug_Error("SSL_set_fd %s\n", ERR_error_string(ERR_get_error(), NULL));
        return -1;
    }
    ERR_clear_error();
    rv = SSL_connect(ssl);
    /* 1  is fine
        * 0  is "not successful but was shut down controlled"
        * <0 is "handshake was not successful, because a fatal error occurred"
        */
    if (rv <= 0) {
        Debug_Error("SSL_connect %s", ERR_error_string(ERR_get_error(), NULL));
        return -1;
    }

#ifdef USED_CERT
    if (FILE_EXSIT(CERT_FILE)) {
        int lerr = SSL_get_verify_result(ssl);
        if (lerr != X509_V_OK) {
            Debug_Error("SSL certificate problem: %s\n", X509_verify_cert_error_string(lerr));
            return -1;
        } else {
            Debug_Error("SSL certificate OK: %s\n", X509_verify_cert_error_string(lerr));
        }
    }
#endif

    return 0;
}

static int ssl_handshake_timeout(SSL *ssl, int fd, int to_s)
{
    int rv = -1;
    int err = -1;
    int err_cnt = 0;
    struct pollfd pfd;
    int events = POLLIN | POLLOUT | POLLERR;

    SSL_set_connect_state(ssl);
    if (SSL_set_fd(ssl, fd) == 0 || fd < 0) {
        Debug_Error("SSL_set_fd %s\n", ERR_error_string(ERR_get_error(), NULL));
        return -1;
    }

    ERR_clear_error();
    while ((rv = SSL_do_handshake(ssl)) != 1) {
        err = SSL_get_error(ssl, rv);
        if (err == SSL_ERROR_WANT_WRITE) {
            events |= POLLOUT;
            events &= ~POLLIN;
        } else if (err == SSL_ERROR_WANT_READ) {
            events |= POLLIN;
            events &= ~POLLOUT;
        } else {
            err_cnt++;
            Debug_Error("SSL_do_handshake return %d error %d errno %d msg %s\n", rv, err, errno,
                        strerror(errno));
            if (err_cnt < 3) {
                sleep(1);
                ERR_clear_error();
                continue;
            } else {
                return -1;
            }
        }
        pfd.fd = fd;
        pfd.events = events;
        rv = poll(&pfd, 1, to_s * 1000);
        if (rv == 0) {
            Debug_Error("SSL_do_handshake timeout return %d error %d errno %d msg %s\n", rv, err,
                        errno, strerror(errno));
            return -1;
        }
    }

#ifdef USED_CERT
    if (FILE_EXSIT(CERT_FILE)) {
        int lerr = SSL_get_verify_result(ssl);
        if (lerr != X509_V_OK) {
            Debug_Error("SSL certificate problem: %s\n", X509_verify_cert_error_string(lerr));
            return -1;
        } else {
            Debug_Error("~~~~~SSL certificate OK: %s ~~~~~\n", X509_verify_cert_error_string(lerr));
        }
    }
#endif

    return 0;
}

/*
 * Submits the request |req| to the connection |connection|.  This
 * function does not send packets; just append the request to the
 * internal queue in |connection->session|.
 */
static int submit_request(http2_conn_t *conn, req_item_t *req)
{
    int32_t stream_id = -1;
    http2_request_t *request = NULL;

    if (!conn || !conn->session || !req || !req->request) {
        Debug_Error("input error\n");
        return -1;
    }

    request = req->request;

    Debug_Info("req->method is %s\n", request->method);
    if (!strncasecmp(HTTP_GET, request->method, strlen(HTTP_GET))) {
        /* Make sure that the last item is NULL */
        const nghttp2_nv nva[] = {MAKE_NV_CS(":method", request->method),
                                  MAKE_NV_CS(":path", request->path), MAKE_NV(":scheme", "https"),
                                  MAKE_NV_CS("authorization", request->authorization),
                                  MAKE_NV("accept", "*/*")};
        nghttp2_priority_spec priority;
        nghttp2_priority_spec_default_init(&priority);
        priority.weight = NGHTTP2_DEFAULT_WEIGHT + 2;
        stream_id = nghttp2_submit_request(conn->session, &priority, nva,
                                           sizeof(nva) / sizeof(nva[0]), NULL, req);
    } else {
        /* Make sure that the last item is NULL */
        const nghttp2_nv nva[] = {MAKE_NV_CS(":method", request->method),
                                  MAKE_NV_CS(":path", request->path),
                                  MAKE_NV(":scheme", "https"),
                                  MAKE_NV_CS("authorization", request->authorization),
                                  MAKE_NV("accept", "*/*"),
                                  MAKE_NV_CS("content-type", request->content_type)};
        req->data_prd.read_callback = data_source_read_callback;
        req->data_prd.source.ptr = (void *)request->body;

        if (request->upload_cb && request->upload_data) {
            nghttp2_priority_spec priority;
            nghttp2_priority_spec_default_init(&priority);
            priority.stream_id = 1;
            priority.weight = NGHTTP2_DEFAULT_WEIGHT + 1;
            stream_id = nghttp2_submit_request(conn->session, &priority, nva,
                                               sizeof(nva) / sizeof(nva[0]), &req->data_prd, req);
#if (NGHTTP2_VERSION_NUM >= 0x011600)
            int window_size = 16384;
            nghttp2_session_set_local_window_size(conn->session, NGHTTP2_FLAG_NONE, stream_id,
                                                  window_size);
#endif
        } else {
            stream_id = nghttp2_submit_request(conn->session, NULL, nva,
                                               sizeof(nva) / sizeof(nva[0]), &req->data_prd, req);
        }
    }

    if (stream_id < 0) {
        Debug_Error("%s\n", nghttp2_strerror(stream_id));
    }

    req->stream_id = stream_id;
    Debug_Trace("stream_id = %d\n", stream_id);

    return stream_id;
}

/*
 * Update |pollfd| based on the state of |connection|.
 */
static void ctl_poll(struct pollfd *pollfd, http2_conn_t *conn)
{
    if (pollfd && conn) {
        pollfd->events = 0;
        if (nghttp2_session_want_write(conn->session) || conn->want_io == WANT_WRITE) {
            pollfd->events |= POLLOUT;
            conn->want_io = IO_NONE;
        }
        if (nghttp2_session_want_read(conn->session) || conn->want_io == WANT_READ) {
            pollfd->events |= POLLIN;
            conn->want_io = IO_NONE;
        }
        if (pollfd->events == 0) {
            Debug_Trace("poll error\n");
        }
    }
}

http2_conn_t *http2_conn_create(char *host, uint16_t port, long conn_tmout)
{
    int rv = -1;
    http2_conn_t *conn = NULL;
    struct sigaction act;
    nghttp2_session_callbacks *callbacks = NULL;

    conn = (http2_conn_t *)calloc(1, sizeof(http2_conn_t));
    if (conn) {
        memset(&act, 0, sizeof(struct sigaction));
        act.sa_handler = SIG_IGN;
        sigaction(SIGPIPE, &act, 0);

        if (0 == g_ssl_inited) {
#ifndef OPENSSL_IS_BORINGSSL
            OPENSSL_config(NULL);
#endif /* OPENSSL_IS_BORINGSSL */
            SSL_load_error_strings();
            g_ssl_inited = 1;
            SSL_library_init();
        }

        /* Establish connection and setup SSL */
        conn->ssl_fd = connect_to(host, port, (int)conn_tmout);
        if (conn->ssl_fd < 0) {
            Debug_Error("Could not open file descriptor\n");
            goto failed_exit;
        }

        conn->ssl_ctx = SSL_CTX_new(TLS_client_method());
        if (conn->ssl_ctx == NULL) {
            Debug_Error("SSL_CTX_new error(%s)\n", ERR_error_string(ERR_get_error(), NULL));
            goto failed_exit;
        }
        init_ssl_ctx(conn->ssl_ctx);

        conn->ssl = SSL_new(conn->ssl_ctx);
        if (conn->ssl == NULL) {
            Debug_Error("SSL_new error(%s)\n", ERR_error_string(ERR_get_error(), NULL));
            goto failed_exit;
        }

        SSL_set_tlsext_host_name(conn->ssl, host);

        /* To simplify the program, we perform SSL/TLS handshake in blocking I/O. */
        Debug_Info("ssl_handshake start at %lld\n", tickSincePowerOn());
        if (conn_tmout > 0) {
            rv = ssl_handshake_timeout(conn->ssl, conn->ssl_fd, (int)conn_tmout);
        } else {
            rv = ssl_handshake(conn->ssl, conn->ssl_fd);
        }
        if (rv != 0) {
            Debug_Error("ssl_handshake failed\n");
            goto failed_exit;
        }
        Debug_Info("ssl_handshake end at %lld\n", tickSincePowerOn());
        conn->want_io = IO_NONE;

        Debug_Error("SSL/TLS handshake completed\n");

        /* Here make file descriptor non-block */
        make_non_block(conn->ssl_fd);
        set_tcp_nodelay(conn->ssl_fd);
        set_tcp_keep_alive(conn->ssl_fd);

        rv = nghttp2_session_callbacks_new(&callbacks);
        if (rv != 0) {
            Debug_Error("nghttp2_session_callbacks_new");
            goto failed_exit;
        }

        setup_nghttp2_callbacks(callbacks);
        rv = nghttp2_session_client_new(&conn->session, callbacks, conn);
        nghttp2_session_callbacks_del(callbacks);
        if (rv != 0) {
            Debug_Error("nghttp2_session_client_new error: %s\n", nghttp2_strerror(rv));
            goto failed_exit;
        }

        nghttp2_submit_settings(conn->session, NGHTTP2_FLAG_NONE, NULL, 0);

        return conn;
    }

failed_exit:

    if (conn) {
        if (conn->session) {
            nghttp2_session_del(conn->session);
            conn->session = NULL;
        }

        if (conn->ssl) {
            SSL_shutdown(conn->ssl);
            SSL_free(conn->ssl);
            conn->ssl = NULL;
        }

        if (conn->ssl_ctx) {
            SSL_CTX_free(conn->ssl_ctx);
            conn->ssl_ctx = NULL;
        }

        if (conn->ssl_fd >= 0) {
            shutdown(conn->ssl_fd, SHUT_WR);
            close(conn->ssl_fd);
            conn->ssl_fd = -1;
        }

        free(conn);
    }

    return NULL;
}

int http2_conn_destroy(http2_conn_t **conn)
{
    http2_conn_t *asr_conn = *conn;

    if (asr_conn) {

        if (asr_conn->session) {
            nghttp2_session_del(asr_conn->session);
            asr_conn->session = NULL;
        }

        if (asr_conn->ssl) {
            SSL_shutdown(asr_conn->ssl);
            SSL_free(asr_conn->ssl);
            asr_conn->ssl = NULL;
        }

        if (asr_conn->ssl_ctx) {
            SSL_CTX_free(asr_conn->ssl_ctx);
            asr_conn->ssl_ctx = NULL;
        }

        if (asr_conn->ssl_fd >= 0) {
            shutdown(asr_conn->ssl_fd, SHUT_WR);
            close(asr_conn->ssl_fd);
            asr_conn->ssl_fd = -1;
        }

        ERR_clear_error();
        ERR_free_strings();
        ERR_remove_thread_state(NULL);

        clear_request();
        free(asr_conn);
        asr_conn = NULL;
        return 0;
    }

    return -1;
}

int http2_conn_keep_alive(http2_conn_t *conn, http2_request_t *request, int timeout)
{
    int ret = 0;

    if (!conn || !request) {
        Debug_Error("input error\n");
        return -1;
    }

    check_all_timeout(conn, 1, 0);

    req_item_t *req_item = create_request(request, timeout);
    ret = submit_request(conn, req_item);
    if (req_item && ret > 0) {
        int rv = 0;
        int local_sock = -1;
        struct pollfd pollfds[2];
        long timeout_ms = /*-1*/ 50;

        memset(pollfds, 0x0, sizeof(pollfds));
        pollfds[0].fd = conn->ssl_fd;

        local_sock = create_local_sock();
        if (local_sock >= 0) {
            pollfds[1].fd = local_sock;
            pollfds[1].events |= POLLIN;
        }

        /* Event loop */
        while ((nghttp2_session_want_read(conn->session) ||
                nghttp2_session_want_write(conn->session)) &&
               (!conn->continued)) {
            pollfds[1].events |= POLLIN;
            ctl_poll(&pollfds[0], conn);
            int nfds = poll(pollfds, 2, timeout_ms);
            if (0 > nfds) {
                Debug_Error("poll error(%s)\n", strerror(errno));
                break;
            } else if (0 < nfds) {
                check_all_timeout(conn, 0, 0);
                if (pollfds[0].revents & (POLLIN)) {
                    if (nghttp2_session_want_read(conn->session)) {
                        rv = nghttp2_session_recv(conn->session);
                        if (rv != 0) {
                            Debug_Trace(
                                "nghttp2_session_recv error rv(%d) %s errno(%d) errnostr(%s)\n", rv,
                                nghttp2_strerror(rv), errno, strerror(errno));
                            if (NGHTTP2_ERR_EOF != rv) {
                                ret = -1;
                            } else {
                                ret = 0;
                            }
                            break;
                        } else {
                            continue;
                        }
                    }
                }
                if (pollfds[0].revents & (POLLOUT)) {
                    if (nghttp2_session_want_write(conn->session)) {
                        rv = nghttp2_session_send(conn->session);
                        if (rv != 0) {
                            Debug_Error(
                                "nghttp2_session_send error rv(%d) %s errno(%d) errnostr(%s)\n", rv,
                                nghttp2_strerror(rv), errno, strerror(errno));
                            ret = -1;
                            break;
                        }
                    }
                }
                if ((pollfds[0].revents & POLLHUP) || (pollfds[0].revents & POLLERR)) {
                    Debug_Error("ssl connection errno(%d) errnostr(%s)\n", errno, strerror(errno));
                    ret = -1;
                    break;
                }
                if (pollfds[1].revents & (POLLIN | POLLOUT)) {
                    char buf[64] = {0};
                    read(pollfds[1].fd, buf, sizeof(buf));
                    Debug_Info("read info from fifo: (%s)\n", buf);

                    if (!strncmp(buf, REQUEST_INSERT, strlen(REQUEST_INSERT))) {
                        rv = add_request(conn);
                        if (0 != rv) {
                            ret = -1;
                            break;
                        }
                    } else if (!strncmp(buf, CONN_QUIT, strlen(CONN_QUIT))) {
                        ret = 0;
                        break;
                    } else if (!strncmp(buf, CONN_PING, strlen(CONN_PING))) {
                        Debug_Error("Start PING AVS Server\n");
                        nghttp2_submit_ping(conn->session, NGHTTP2_FLAG_NONE, NULL);
                        conn->ping_time = tickSincePowerOn();
                    }
                }
                if ((pollfds[1].revents & POLLHUP) || (pollfds[1].revents & POLLERR)) {
                    Debug_Error("local sock connection errno(%d) errnostr(%s)\n", errno,
                                strerror(errno));
                    ret = -1;
                    break;
                }
            } else if (0 == nfds) {
                // Poll time out
                check_all_timeout(conn, 0, 1);
            }
            long long cur_time = tickSincePowerOn();
            if (conn->ping_time > 0 && cur_time > conn->ping_time + 15000L) {
                conn->ping_time = 0;
                ret = 0;
                Debug_Error("PING avs server failed, rebuilt the connection\n");
                break;
            }
            if (conn->timeout_cnt >= MAX_TIMEOU_CNT) {
                ret = -1;
                break;
            }
        }

        if (0 <= local_sock) {
            close(local_sock);
            unlink(NGHTTP2_FIFO);
        }

        remove_request(&req_item);
        return ret;
    }

    remove_request(&req_item);
    return -1;
}

int http2_conn_exit(void)
{
    int ret = -1;

    ret = local_sock_send(CONN_QUIT, strlen(CONN_QUIT));

    return ret;
}

int http2_conn_ping(void)
{
    int ret = -1;

    ret = local_sock_send(CONN_PING, strlen(CONN_PING));

    return ret;
}

int http2_conn_cancel_upload(void)
{
    int ret = -1;
    int cancel = 0;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    req_item_t *req_item = NULL;

    request_list_lock();
    if (!lp_list_empty(&g_request_list)) {
        lp_list_for_each_safe(pos, npos, &g_request_list)
        {
            req_item = lp_list_entry(pos, req_item_t, list);
            if (req_item) {
                pthread_mutex_lock(&req_item->req_mutex);
                if (req_item->request && req_item->request->upload_cb &&
                    req_item->request->upload_data) {
                    if (req_item->status < REQUEST_EXIT) {
                        Debug_Info("cancel stream_id = %d status = %d\n", req_item->stream_id,
                                   req_item->status);
                        sem_post(&req_item->req_sem);
                        ret = 0;
                    }
                }
                pthread_mutex_unlock(&req_item->req_mutex);
            }
        }
    }
    request_list_unlock();

    return ret;
}

int http2_conn_do_request(http2_conn_t *conn, http2_request_t *request, int timeout)
{
    int ret = 0;
    req_item_t *req_item = NULL;

    if (!conn || !request) {
        Debug_Error("please check you param \n");
        return -1;
    }

    Debug_Trace("start +++++++++++++++++++++++++++ \n");

    req_item = create_request(request, timeout);
    if (req_item) {
        request_list_lock();
        lp_list_add_tail(&req_item->list, &g_request_list);
        request_list_unlock();
        ret = local_sock_send(REQUEST_INSERT, strlen(REQUEST_INSERT));
        if (ret < 0) {
            request_list_lock();
            lp_list_del(&req_item->list);
            request_list_unlock();
            remove_request(&req_item);
        } else {
#if WAIT_TIME_OUT
            struct timespec time_wait;
            struct timeval now;
            gettimeofday(&now, NULL);
            time_wait.tv_nsec = now.tv_usec * 1000;
            time_wait.tv_sec = now.tv_sec + timeout + 2;
            ret = sem_timedwait(&req_item->req_sem, &time_wait);
            /* Check what happened */
            if (ret == -1) {
                if (errno == ETIMEDOUT) {
                    Debug_Trace("stream_id %d sem_timedwait() timed out\n", req_item->stream_id);
                } else {
                    perror("sem_timedwait");
                }
            }
#else
            sem_wait(&req_item->req_sem);
#endif
            Debug_Trace("stream_id(%d) end status(%d) http_ret(%d) ----\n", req_item->stream_id,
                        req_item->status, req_item->request->http_ret);
            pthread_mutex_lock(&req_item->req_mutex);
            if (ret == -1) {
                req_item->request->http_ret = 0;
            }
            req_item->request = NULL;
            pthread_mutex_unlock(&req_item->req_mutex);
            req_item->status = REQUEST_EXIT;
        }
    }

    Debug_Trace("end --------------------------- \n");

    return ret;
}
