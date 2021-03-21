
#include "curl_https.h"
#include "lp_list.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#ifndef CURLPIPE_MULTIPLEX
/* This little trick will just make sure that we don't enable pipelining for
   libcurls old enough to not have this symbol. It is _not_ defined to zero in
   a recent libcurl header. */
#define CURLPIPE_MULTIPLEX 0
#endif

#define SKIP_PEER_VERIFICATION
#define SKIP_HOSTNAME_VERIFICATION

struct _curl_https_s {
    int enble_pipe;
    CURL *curl_handle;
    struct curl_slist *multi_header;
    struct curl_httppost *multi_post;
    curl_result_t *result;
    long https_ret;
};

#if !defined(ASR_DCS_MODULE)
static int g_curl_global_init = 0;
#endif
pthread_mutex_t g_mutex_multihandle = PTHREAD_MUTEX_INITIALIZER;
CURLM *g_multi_handle = NULL;
#define ALEXA_EVENT_FIFO ("/tmp/event_fifo")

LP_LIST_HEAD(g_event_list);
pthread_mutex_t g_mutex_event_list = PTHREAD_MUTEX_INITIALIZER;

typedef struct _event_s {
    struct lp_list_head list;
    curl_https_t *curl_https;
    pthread_mutex_t event_mutex;
    pthread_cond_t event_cond;
    int added;
} event_t;

event_t *curl_add_event_handle(void)
{
    CURLMcode res = CURLM_OK;
    struct lp_list_head *pos = NULL;
    event_t *avs_event = NULL;

    if (!g_multi_handle) {
        printf("[%s:%d] multi handle is NULL\n", __FUNCTION__, __LINE__);
        return NULL;
    }

    pthread_mutex_lock(&g_mutex_event_list);
    if (!lp_list_empty(&g_event_list)) {
        lp_list_for_each(pos, &g_event_list)
        {
            avs_event = lp_list_entry(pos, event_t, list);
            if (avs_event && avs_event->curl_https && avs_event->curl_https->curl_handle &&
                avs_event->added == 0) {
                res = curl_multi_add_handle(g_multi_handle, avs_event->curl_https->curl_handle);
                if (res != CURLM_OK) {
                    printf("[%s:%d] curl_multi_add_handle failed\n", __FUNCTION__, __LINE__);
                }
                // if (avs_event->curl_https->enble_pipe) {
                //    curl_multi_setopt(g_multi_handle, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
                //}
                /* We do HTTP/2 so let's stick to one connection per host */
                // curl_multi_setopt(g_multi_handle, CURLMOPT_MAX_HOST_CONNECTIONS, 1L);
                avs_event->added = 1;
                break;
            }
        }
    }
    pthread_mutex_unlock(&g_mutex_event_list);

    return avs_event;
}

int curl_create_fifo(int mode)
{
    int alexa_fifo = -1;

    if ((access(ALEXA_EVENT_FIFO, F_OK)) != 0) {
        unlink(ALEXA_EVENT_FIFO);
        alexa_fifo = mkfifo(ALEXA_EVENT_FIFO, O_CREAT | 0666);
        if (alexa_fifo < 0) {
            wiimu_log(1, 0, 0, 0, "Fail to create FIFO: error(%s)\n", strerror(errno));
            goto EXIT;
        }
        printf("[%s:%d] access \n", __FUNCTION__, __LINE__);
    }

    alexa_fifo = open(ALEXA_EVENT_FIFO, mode);
    if (alexa_fifo < 0) {
        printf("[%s:%d] g_alexa_fifo < 0 error\n", __FUNCTION__, __LINE__);
    }

EXIT:
    return alexa_fifo;
}

int curl_create_sock(void)
{
    int server_fd = -1;
    int res = -1;

    // start server side
    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(struct sockaddr_un));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, ALEXA_EVENT_FIFO, 107);
    unlink(ALEXA_EVENT_FIFO);
    server_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (server_fd < 0) {
        wiimu_log(1, 0, 0, 0, "Fail to create socket: error(%s)\n", strerror(errno));
        return res;
    }
    res = bind(server_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_un));
    if (res) {
        perror("Fail to start hub server.\n");
        perror(strerror(errno));
        close(server_fd);
        return res;
    }

    return server_fd;
}

int curl_sock_send(char *buf, size_t len)
{
    int cliet_fd = -1;
    int size = -1;

    // initialize client size
    struct sockaddr_un client_addr;

    client_addr.sun_family = AF_UNIX;
    strncpy(client_addr.sun_path, ALEXA_EVENT_FIFO, 107);
    cliet_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (cliet_fd == -1) {
        perror("Fail to create socket.");
        perror(strerror(errno));
        return -1;
    }

    size =
        sendto(cliet_fd, buf, len, 0, (struct sockaddr *)&client_addr, sizeof(struct sockaddr_un));

    close(cliet_fd);

    return size;
}

event_t *curl_create_event(curl_https_t *_this)
{
    event_t *avs_event = NULL;
    int write_fd = -1;

    if (!_this) {
        printf("[%s:%d] multi handle is NULL\n", __FUNCTION__, __LINE__);
        return NULL;
    }

    // need to free!!!!
    avs_event = (event_t *)calloc(1, sizeof(event_t));
    if (avs_event) {
        avs_event->curl_https = _this;
        pthread_mutex_init(&avs_event->event_mutex, NULL);
        pthread_cond_init(&avs_event->event_cond, NULL);
        pthread_mutex_lock(&g_mutex_event_list);
        lp_list_add_tail(&avs_event->list, &g_event_list);
        pthread_mutex_unlock(&g_mutex_event_list);

#if 0
        write_fd = curl_create_fifo(O_WRONLY | O_NONBLOCK);
        printf("[%s:%d] write_fd (%d)\n", __FUNCTION__, __LINE__, write_fd);
        printf("[%s:%d] write_fd (%d)\n", __FUNCTION__, __LINE__, write(write_fd, "insert", strlen("insert")));
        //write(write_fd, "insert", strlen("insert"));
        close(write_fd);
#else
        curl_sock_send("insert", strlen("insert"));
#endif
    } else {
        printf("[%s:%d] create an event error\n", __FUNCTION__, __LINE__);
        return NULL;
    }

    return avs_event;
}

#if defined(ASR_DCS_MODULE)
/* curl_global_init should call at the beginning of the process and only being called for one time
 * during the life of the process */
int CurlHttpsGlobalInit(void)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);

    return 0;
}
#endif

curl_https_t *CurlHttpsInit(void)
{
    curl_https_t *_this = (curl_https_t *)calloc(1, sizeof(curl_https_t));
    if (!_this) {
        printf("[%s:%d] malloc failed\n", __FUNCTION__, __LINE__);
        goto EXIT;
    } else {
#if !defined(ASR_DCS_MODULE)
        if (g_curl_global_init == 0) {
            curl_global_cleanup();
            curl_global_init(CURL_GLOBAL_DEFAULT);
            g_curl_global_init = 1;
        }
#endif

        if (g_multi_handle == NULL) {
            pthread_mutex_lock(&g_mutex_multihandle);
            g_multi_handle = curl_multi_init();
            pthread_mutex_unlock(&g_mutex_multihandle);
        }

        _this->curl_handle = curl_easy_init();
        if (!_this->curl_handle) {
            free(_this);
            printf("[%s:%d] curl_easy_init failed\n", __FUNCTION__, __LINE__);
            goto EXIT;
        }
        // printf("[%s:%d] curl_handle=0x%p\n", __FUNCTION__, __LINE__, _this->curl_handle);

        _this->enble_pipe = 0;
        _this->multi_header = NULL;
        _this->multi_post = NULL;

        _this->result = calloc(1, sizeof(curl_result_t));
        if (_this->result) {
            membuffer_init(&_this->result->result_header);
            membuffer_init(&_this->result->result_body);
        } else {
            printf("[%s:%d] result malloc failed\n", __FUNCTION__, __LINE__);
        }

        return _this;
    }

EXIT:
    return NULL;
}

int CurlHttpsUnInit(curl_https_t **_this)
{
    if (_this) {
        if (*_this) {
            curl_https_t *curl_https = *_this;

            if (curl_https->multi_header) {
                curl_slist_free_all(curl_https->multi_header);
                curl_https->multi_header = NULL;
            }

            if (curl_https->multi_post) {
                curl_formfree(curl_https->multi_post);
                curl_https->multi_post = NULL;
            }

            if (curl_https->result) {
                membuffer_destroy(&curl_https->result->result_header);
                membuffer_destroy(&curl_https->result->result_body);
                free(curl_https->result);
                curl_https->result = NULL;
            }

            free(curl_https);
        } else {
            printf("[%s:%d] *this is NULL \n", __FUNCTION__, __LINE__);
        }
    }

    return 0;
}

CURLcode CurlHttpsRun(curl_https_t *_this, int retry_count)
{
    if (_this) {
        CURLcode res;
        int tryCount = 0;

        if (retry_count < 0) {
            tryCount = 0;
        } else {
            tryCount = retry_count;
        }
    retry:
        /* Perform the request, res will get the return code */
        res = curl_easy_perform(_this->curl_handle);
        /* Check for errors */
        if (res != CURLE_OK) {
            printf("CurlHttpsRun curl_easy_perform() failed: %d,%s\r\n", res,
                   curl_easy_strerror(res));
            if (CURLE_OPERATION_TIMEOUTED == res || CURLE_COULDNT_RESOLVE_HOST == res ||
                CURLE_COULDNT_CONNECT == res) {
                if (tryCount--)
                    goto retry;
            }
        }

        curl_easy_getinfo(_this->curl_handle, CURLINFO_RESPONSE_CODE, &_this->https_ret);
        printf("[%s:%d] response code is %ld\n", __FUNCTION__, __LINE__, _this->https_ret);
        // printf("[%s:%d] curl_handle=0x%p\n", __FUNCTION__, __LINE__, _this->curl_handle);
        curl_easy_cleanup(_this->curl_handle);

        return res;
    } else {
        printf("[%s:%d] this is NULL \n", __FUNCTION__, __LINE__);
        return -1;
    }

    return 0;
}

CURLMcode CurlHttpsMultiRun(curl_https_t *_this)
{
    if (_this) {
        if (_this->curl_handle && g_multi_handle) {
            CURLMcode res;
            struct CURLMsg *m;
            int still_running = 0;
            int fifo_fd = -1;

#if 0
            fifo_fd = curl_create_fifo(O_RDONLY | O_NONBLOCK);
            printf("[%s:%d] fifo_fd (%d)\n", __FUNCTION__, __LINE__, fifo_fd);
#else
            fifo_fd = curl_create_sock();
#endif

            res = curl_multi_add_handle(g_multi_handle, _this->curl_handle);
            if (res != CURLM_OK) {
                printf("[%s:%d] curl_multi_add_handle failed\n", __FUNCTION__, __LINE__);
            }
            if (_this->enble_pipe) {
                curl_multi_setopt(g_multi_handle, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
            }
            /* We do HTTP/2 so let's stick to one connection per host */
            curl_multi_setopt(g_multi_handle, CURLMOPT_MAX_HOST_CONNECTIONS, 1L);

            res = curl_multi_perform(g_multi_handle, &still_running);
            if (res != CURLM_OK) {
                printf("[%s:%d] curl_multi_perform failed\n", __FUNCTION__, __LINE__);
            }

            do {
                struct timeval timeout;
                int rc; /* select() return code */

                fd_set fdread;
                fd_set fdwrite;
                fd_set fdexcep;
                int maxfd = -1;

                long curl_timeo = -1;

                FD_ZERO(&fdread);
                FD_ZERO(&fdwrite);
                FD_ZERO(&fdexcep);

                /* set a suitable timeout to play around with */
                timeout.tv_sec = 0;
                timeout.tv_usec = 500 * 1000;

                curl_add_event_handle();

                res = curl_multi_timeout(g_multi_handle, &curl_timeo);
                if (res != CURLM_OK) {
                    printf("[%s:%d] 1 curl_multi_timeout failed\n", __FUNCTION__, __LINE__);
                }

                if (curl_timeo >= 0) {
                    timeout.tv_sec = curl_timeo / 1000;
                    if (timeout.tv_sec > 1)
                        timeout.tv_sec = 1;
                    else
                        timeout.tv_usec = (curl_timeo % 1000) * 1000;
                }

                /* get file descriptors from the transfers */
                res = curl_multi_fdset(g_multi_handle, &fdread, &fdwrite, &fdexcep, &maxfd);
                if (res != CURLM_OK) {
                    fprintf(stderr, "curl_multi_fdset() failed, code %d.\n", res);
                    break;
                }
                FD_SET(fifo_fd, &fdread);
                FD_SET(fifo_fd, &fdexcep);

                maxfd = (maxfd > fifo_fd) ? maxfd : fifo_fd;
                /* On success the value of maxfd is guaranteed to be >= -1. We call
                         select(maxfd + 1, ...); specially in case of (maxfd == -1) there are
                         no fds ready yet so we call select(0, ...) --or Sleep() on Windows--
                         to sleep 100ms, which is the minimum suggested value in the
                         curl_multi_fdset() doc. */

                if (maxfd == -1) {
                    /* Portable sleep for platforms other than Windows. */
                    struct timeval wait = {0, 100 * 1000};
                    rc = select(0, NULL, NULL, NULL, &wait);
                } else {
                    /* Note that on some platforms 'timeout' may be modified by select().
                               If you need access to the original value save a copy beforehand. */
                    rc = select(maxfd + 1, &fdread, &fdwrite, &fdexcep, &timeout);
                }

                switch (rc) {
                case -1:
                    /* select error */
                    break;
                case 0:
                default:
                    if (rc > 0 && FD_ISSET(fifo_fd, &fdread)) {
                        FD_CLR(fifo_fd, &fdread);
                        char buf[32] = {0};
                        read(fifo_fd, buf, sizeof(buf));
                        printf("[%s:%d] read info from fifo: (%s)\n", __FUNCTION__, __LINE__, buf);
                    }
                    curl_add_event_handle();

                    /* timeout or readable/writable sockets */
                    res = curl_multi_perform(g_multi_handle, &still_running);
                    if (res != CURLM_OK) {
                        printf("[%s:%d] 2 curl_multi_perform failed\n", __FUNCTION__, __LINE__);
                    }
                    break;
                }

                do {
                    int msg_cnt = 0;
                    ;
                    //  printf("curl_multi_info_read \r\n");
                    m = curl_multi_info_read(g_multi_handle, &msg_cnt);
                    if (m && (m->msg == CURLMSG_DONE)) {
                        printf("down channel handle 0x%x easy_handle done  0x%x\n",
                               _this->curl_handle, m->easy_handle);
                        if (_this->curl_handle != m->easy_handle) {
                            struct lp_list_head *pos = NULL;
                            event_t *avs_event = NULL;
                            pthread_mutex_lock(&g_mutex_event_list);
                            if (!lp_list_empty(&g_event_list)) {
                                lp_list_for_each(pos, &g_event_list)
                                {
                                    avs_event = lp_list_entry(pos, event_t, list);
                                    if (avs_event && avs_event->curl_https &&
                                        avs_event->curl_https->curl_handle) {
                                        printf("avs event handle 0x%x easy_handle done  0x%x\n",
                                               avs_event->curl_https->curl_handle, m->easy_handle);
                                        if (avs_event->curl_https->curl_handle == m->easy_handle) {
                                            lp_list_del(&avs_event->list);
                                            curl_multi_remove_handle(g_multi_handle,
                                                                     m->easy_handle);
                                            pthread_mutex_lock(&avs_event->event_mutex);
                                            pthread_cond_signal(&avs_event->event_cond);
                                            pthread_mutex_unlock(&avs_event->event_mutex);
                                            break;
                                        }
                                    }
                                }
                            }
                            pthread_mutex_unlock(&g_mutex_event_list);
                        } else {
                            printf("[%s:%d] ------- down channel is exit -----\n", __FUNCTION__,
                                   __LINE__);
                            // curl_easy_reset(_this->curl_handle);
                        }
                    }
                } while (m);
            } while (still_running);

            curl_multi_remove_handle(g_multi_handle, _this->curl_handle);
            curl_easy_getinfo(_this->curl_handle, CURLINFO_RESPONSE_CODE, &_this->https_ret);
            printf("[%s:%d] response code is %ld\n", __FUNCTION__, __LINE__, _this->https_ret);

            curl_easy_cleanup(_this->curl_handle);
            // curl_multi_cleanup(g_multi_handle);
            // g_multi_handle = NULL;

            close(fifo_fd);
            return res;
        } else {
            printf("[%s:%d] please check you param \n", __FUNCTION__, __LINE__);
            return -1;
        }
    } else {
        printf("[%s:%d] this is NULL \n", __FUNCTION__, __LINE__);
        return -1;
    }

    return 0;
}

CURLcode CurlHttpsWaitFinished(curl_https_t *_this)
{
    CURLcode result = CURLE_OK;
    event_t *avs_event = NULL;
    struct timespec time_to_wait;

    if (!_this || !_this->curl_handle || !g_multi_handle) {
        printf("[%s:%d] please check you param \n", __FUNCTION__, __LINE__);
        return -1;
    }

    avs_event = curl_create_event(_this);
    if (avs_event) {
        pthread_mutex_lock(&avs_event->event_mutex);
        // time_to_wait.tv_nsec = 0;
        // time_to_wait.tv_sec = time(NULL) + 30;
        // pthread_cond_timedwait(&avs_event->event_cond, &avs_event->event_mutex, &time_to_wait);
        pthread_cond_wait(&avs_event->event_cond, &avs_event->event_mutex);
        pthread_mutex_unlock(&avs_event->event_mutex);

        pthread_mutex_destroy(&avs_event->event_mutex);
        pthread_cond_destroy(&avs_event->event_cond);
        curl_easy_getinfo(avs_event->curl_https->curl_handle, CURLINFO_RESPONSE_CODE,
                          &avs_event->curl_https->https_ret);
        curl_easy_cleanup(avs_event->curl_https->curl_handle);

        free(avs_event);
    }

    return result;
}

int CurlHttpsSetMethod(curl_https_t *_this, char *method)
{
    if (_this) {
        if (_this->curl_handle && method) {
            curl_easy_setopt(_this->curl_handle, CURLOPT_CUSTOMREQUEST, method);
            curl_easy_setopt(_this->curl_handle, CURLOPT_NOBODY, 1L);
        } else {
            printf("[%s:%d] please check you param \n", __FUNCTION__, __LINE__);
            return -1;
        }
    } else {
        printf("[%s:%d] this is NULL \n", __FUNCTION__, __LINE__);
        return -1;
    }

    return 0;
}

int CurlHttpsSetVerSion(curl_https_t *_this, long version)
{
    if (_this) {
        if (_this->curl_handle) {
            curl_easy_setopt(_this->curl_handle, CURLOPT_HTTP_VERSION, version);
        }
    } else {
        printf("[%s:%d] this is NULL \n", __FUNCTION__, __LINE__);
        return -1;
    }
    return 0;
}

int CurlHttpsSetGet(curl_https_t *_this, long useget)
{
    if (_this) {
        if (_this->curl_handle) {
            curl_easy_setopt(_this->curl_handle, CURLOPT_HTTPGET, useget);
            // curl_easy_setopt(_this->curl_handle, CURLOPT_NOBODY, 1L);
        }
    } else {
        printf("[%s:%d] this is NULL \n", __FUNCTION__, __LINE__);
        return -1;
    }
    return 0;
}

int CurlHttpsSetVerbose(curl_https_t *_this, long onoff)
{
    if (_this) {
        if (_this->curl_handle) {
            curl_easy_setopt(_this->curl_handle, CURLOPT_VERBOSE, onoff);
        }
    } else {
        printf("[%s:%d] this is NULL \n", __FUNCTION__, __LINE__);
        return -1;
    }
    return 0;
}

int CurlHttpsSetTimeOut(curl_https_t *_this, long time_out)
{
    if (_this) {
        if (_this->curl_handle) {
            curl_easy_setopt(_this->curl_handle, CURLOPT_TIMEOUT, time_out);
            curl_easy_setopt(_this->curl_handle, CURLOPT_NOSIGNAL, 1L);
        }
    } else {
        printf("[%s:%d] this is NULL \n", __FUNCTION__, __LINE__);
        return -1;
    }
    return 0;
}

int CurlHttpsSetPipeWait(curl_https_t *_this, long enable)
{
#if (CURLPIPE_MULTIPLEX > 0)
    if (_this) {
        if (_this->curl_handle) {
            /* wait for pipe connection to confirm */
            curl_easy_setopt(_this->curl_handle, CURLOPT_PIPEWAIT, enable);
            if (enable) {
                _this->enble_pipe = 1;
            }
        }
    } else {
        printf("[%s:%d] this is NULL \n", __FUNCTION__, __LINE__);
        return -1;
    }
#else
    printf("[%s:%d] curl is too low\n", __FUNCTION__, __LINE__);
#endif

    return 0;
}

int CurlHttpsSetDepends(curl_https_t *_this, curl_https_t *_depends)
{
#if (CURLPIPE_MULTIPLEX > 0)
    if (_this && _depends) {
        if (_this->curl_handle && _depends->curl_handle) {
            CURLcode res =
                curl_easy_setopt(_this->curl_handle, CURLOPT_STREAM_DEPENDS, _depends->curl_handle);
            printf("[%s:%d] res is %d\n", __FUNCTION__, __LINE__, res);
        }
    } else {
        printf("[%s:%d] this/depends is NULL \n", __FUNCTION__, __LINE__);
        return -1;
    }
#else
    printf("[%s:%d] curl is too low\n", __FUNCTION__, __LINE__);
#endif

    return 0;
}

int CurlHttpsSetUrl(curl_https_t *_this, char *url)
{
    if (_this) {
        if (_this->curl_handle) {
            curl_easy_setopt(_this->curl_handle, CURLOPT_URL, url);
            curl_easy_setopt(_this->curl_handle, CURLOPT_CONNECTTIMEOUT, 10L);
            curl_easy_setopt(_this->curl_handle, CURLOPT_DNS_CACHE_TIMEOUT, 3600);

#ifdef SKIP_PEER_VERIFICATION
            curl_easy_setopt(_this->curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
#endif

#ifdef SKIP_HOSTNAME_VERIFICATION
            curl_easy_setopt(_this->curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
#endif
            curl_easy_setopt(_this->curl_handle, CURLOPT_USERAGENT, "libcurl/" LIBCURL_VERSION);
            curl_easy_setopt(_this->curl_handle, CURLOPT_NOSIGNAL,
                             1L); // must be set, otherwise will crash
        }
    } else {
        printf("[%s:%d] this is NULL \n", __FUNCTION__, __LINE__);
        return -1;
    }

    return 0;
}

int CurlHttpsSetSpeedLimit(curl_https_t *_this, long speed_time_sec, long speed_bytes)
{
    if (_this) {
        curl_easy_setopt(_this->curl_handle, CURLOPT_LOW_SPEED_TIME, speed_time_sec);
        curl_easy_setopt(_this->curl_handle, CURLOPT_LOW_SPEED_LIMIT, speed_bytes);
    } else {
        printf("[%s:%d] this is NULL \n", __FUNCTION__, __LINE__);
        return -1;
    }
    return 0;
}

int CurlHttpsSetHeader(curl_https_t *_this, struct curl_slist *header)
{
    if (_this && header) {
        if (_this->curl_handle) {
            curl_easy_setopt(_this->curl_handle, CURLOPT_HTTPHEADER, header);
            _this->multi_header = header;
        }
    } else {
        printf("[%s:%d] this/header is NULL \n", __FUNCTION__, __LINE__);
        return -1;
    }

    return 0;
}

int CurlHttpsSetBody(curl_https_t *_this, char *post)
{
    if (_this && post) {
        if (_this->curl_handle) {
            curl_easy_setopt(_this->curl_handle, CURLOPT_POST, 1L);
            curl_easy_setopt(_this->curl_handle, CURLOPT_POSTFIELDS, post);
            curl_easy_setopt(_this->curl_handle, CURLOPT_POSTFIELDSIZE, (long)strlen(post));
        }
    } else {
        printf("[%s:%d] this/post is NULL \n", __FUNCTION__, __LINE__);
        return -1;
    }

    return 0;
}

int CurlHttpsSetFormBody(curl_https_t *_this, struct curl_httppost *formpost)
{
    if (_this && formpost) {
        if (_this->curl_handle) {
            curl_easy_setopt(_this->curl_handle, CURLOPT_HTTPPOST, formpost);
            _this->multi_post = formpost;
        }
    } else {
        printf("[%s:%d] this/formpost is NULL \n", __FUNCTION__, __LINE__);
        return -1;
    }

    return 0;
}

int CurlHttpsSetReadCallBack(curl_https_t *_this, void *read_cb, void *data)
{
    if (_this && read_cb) {
        if (_this->curl_handle) {
            // curl_easy_setopt(_this->curl_handle, CURLOPT_EXPECT_100_TIMEOUT_MS, 50);
            curl_easy_setopt(_this->curl_handle, CURLOPT_READFUNCTION, read_cb);
            if (data) {
                curl_easy_setopt(_this->curl_handle, CURLOPT_READDATA, data);
            }
        }
    } else {
        printf("[%s:%d] this is NULL \n", __FUNCTION__, __LINE__);
        return -1;
    }

    return 0;
}

int CurlHttpsSetHeaderCallBack(curl_https_t *_this, void *header_cb, void *data)
{
    if (_this && header_cb) {
        if (_this->curl_handle) {
            curl_easy_setopt(_this->curl_handle, CURLOPT_HEADERFUNCTION, header_cb);
            if (data) {
                curl_easy_setopt(_this->curl_handle, CURLOPT_WRITEHEADER, data);
            } else {
                curl_easy_setopt(_this->curl_handle, CURLOPT_WRITEHEADER, _this->result);
            }
        }
    } else {
        printf("[%s:%d] this is NULL \n", __FUNCTION__, __LINE__);
        return -1;
    }

    return 0;
}

int CurlHttpsSetWriteCallBack(curl_https_t *_this, void *write_cb, void *data)
{
    if (_this && write_cb) {
        if (_this->curl_handle) {
            curl_easy_setopt(_this->curl_handle, CURLOPT_WRITEFUNCTION, write_cb);
            if (data) {
                curl_easy_setopt(_this->curl_handle, CURLOPT_WRITEDATA, data);
            } else {
                curl_easy_setopt(_this->curl_handle, CURLOPT_WRITEDATA, _this->result);
            }
        }
    } else {
        printf("[%s:%d] this is NULL \n", __FUNCTION__, __LINE__);
        return -1;
    }

    return 0;
}

/*
int progress_callback(void *clientp,   curl_off_t dltotal,   curl_off_t dlnow,   curl_off_t ultotal,
curl_off_t ulnow);
CURLcode curl_easy_setopt(CURL *handle, CURLOPT_XFERINFOFUNCTION, progress_callback);
CURLcode curl_easy_setopt(CURL *handle, CURLOPT_NOPROGRESS, long onoff);
*/
int CurlHttpsSetProgressCallBack(curl_https_t *_this, void *progress_cb, void *data)
{
    if (_this && progress_cb) {
        if (_this->curl_handle) {
            curl_easy_setopt(_this->curl_handle, CURLOPT_NOPROGRESS, 0);
            curl_easy_setopt(_this->curl_handle, CURLOPT_XFERINFOFUNCTION, progress_cb);
            if (data) {
                curl_easy_setopt(_this->curl_handle, CURLOPT_XFERINFODATA, data);
            }
        }
    } else {
        printf("[%s:%d] this is NULL \n", __FUNCTION__, __LINE__);
        return -1;
    }

    return 0;
}

curl_result_t *CurlHttpsGetResult(curl_https_t *_this)
{
    if (_this && _this->result) {
        return _this->result;
    } else {
        printf("[%s:%d] this is NULL \n", __FUNCTION__, __LINE__);
    }

    return NULL;
}

int CurlHttpsGetHttpRetCode(curl_https_t *_this)
{
    if (_this) {
        return (int)_this->https_ret;
    } else {
        printf("[%s:%d] this is NULL \n", __FUNCTION__, __LINE__);
    }

    return -1;
}

int CurlHttpsCancel(void)
{
    curl_sock_send("cancel", strlen("cancel"));
    return -1;
}
