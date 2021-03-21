
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/tcp.h>

#include <errno.h>

#include <signal.h>

#include "intercom_record.h"

//#define INTERCOM_RECV_PORT          19100
#define INTERCOM_RECV_PORT 18100
#define MAX_RECORD_TIME 40 // seconds
#define RECORD_BITS 16
#define RECORD_SAMPLE_RATE 16000
#define RECORD_CHANNELS 1
#define MAX_PCM_LEN (RECORD_SAMPLE_RATE * RECORD_BITS * RECORD_CHANNELS * MAX_RECORD_TIME / 8 + 32)

#define INTERCOM_PREFIX "seq="
#define START_PREFIX "seq=START&"
#define START_ALEXA_PREFIX "seq=ALEXA&"
#define END_PREFIX "END&"
#define DROP_PREFIX "DROP&"
#define DATA_PREFIX_LEN 22 // "seq=1234&len=12345678&"

#define ParsePrefix(data, prefix) (!strncmp(data, prefix, strlen(prefix)))

/*
seq=START&
seq=1&len=xxxx&....
seq=2&len=xxxx&....
....
seq=END&
seq=DROP&

BUSY&
FREE&
PLAY&
*/
enum {
    STATUS_WAITING_START = 0,
    STATUS_WAITING_DATA = 1,
    STATUS_WAITING_PLAY = 2,
};

typedef struct {
    int is_runnning;
    unsigned short port;
    int listen_fd;
    int intercom_status;
    int client_fd;
    int client_status;
    pthread_mutex_t mutex;
    pthread_t recv_tid;
} intercom_record_t;

static intercom_record_t g_record_ctx;

extern int AlexaWriteRecordData(char *data, size_t size);
extern int AlexaStopWriteData(void);
extern void switch_content_to_background(int val);

#if 0
static void debug(intercom_record_t *intercom_if)
{
    printf("debug start:\n");
    if(intercom_if) {
        printf("intercom_if       = 0x%x\n", intercom_if);
        printf("port              = %d\n", (int)intercom_if->port);
        printf("listen_fd         = %d\n", intercom_if->listen_fd);
        printf("client_fd         = %d\n", intercom_if->client_fd);
        printf("intercom_status   = %d\n", intercom_if->intercom_status);
        printf("client_status     = %d\n", intercom_if->client_status);
    }
    printf("debug end\n");
}
#else
#define debug(intercom_if)
#endif

#ifdef TMP_RECORD
static FILE *g_record_intercom = NULL;
#endif

static int parse_buf(intercom_record_t *intercom_if, char *buf, int len)
{
    int stat = 0;
    int left = len;
    char *p = NULL;

    if (left <= 0 || !buf || !intercom_if) {
        return 0;
    }

    stat = intercom_if->client_status;

    /* start new transfer */
    if (STATUS_WAITING_START == stat) {
        p = strstr(buf, START_PREFIX);
        if (p) {
            printf("intercom data start\n");
            intercom_if->client_status = STATUS_WAITING_DATA;
            p += strlen(START_PREFIX);
            left = len - strlen(START_PREFIX);
            return parse_buf(intercom_if, p, left);
        }

        p = strstr(buf, START_ALEXA_PREFIX);
        if (p) {
            printf("intercom alexa start\n");
            intercom_if->client_status = STATUS_WAITING_DATA;
            p += strlen(START_ALEXA_PREFIX);
            left = len - strlen(START_ALEXA_PREFIX);
            return parse_buf(intercom_if, p, left);
        }

        p = strstr(buf, "seq=0001&");
        if (p) {
            printf("%s %d status=%d, header not found [%10s]\n", __FUNCTION__, __LINE__, stat, buf);
            printf("however, got first seq number\n");
            // client do not send start ?

            left = len - (p - buf);
            intercom_if->client_status = STATUS_WAITING_DATA;
            return parse_buf(intercom_if, p, left);
        }

        p = strstr(buf, "seq=A001&");
        if (p) {
            printf("%s %d status=%d, intercom alexa header not found [%10s]\n", __FUNCTION__,
                   __LINE__, stat, buf);
            printf("however, got first seq number\n");
            // client do not send start ?

            left = len - (p - buf);
            intercom_if->client_status = STATUS_WAITING_DATA;
            return parse_buf(intercom_if, p, left);
        }
        return len;
    }
    /* data packet */
    if (STATUS_WAITING_DATA == stat) {

        /* data end */
        if (ParsePrefix(buf, END_PREFIX)) {
            printf("intercom data end\n");
            AlexaWriteRecordData(buf, left - strlen(END_PREFIX));
#ifdef TMP_RECORD
            if (g_record_intercom) {
                fwrite(buf, 1, left - strlen(END_PREFIX), g_record_intercom);
            }
#endif
            intercom_if->client_status = STATUS_WAITING_PLAY;
            return 0;
        }

        /* cancel data */
        if (ParsePrefix(buf, DROP_PREFIX)) {
            printf("intercom data cancel\n");
            AlexaWriteRecordData(buf, left - strlen(DROP_PREFIX));
#ifdef TMP_RECORD
            if (g_record_intercom) {
                fwrite(buf, 1, left - strlen(DROP_PREFIX), g_record_intercom);
            }
#endif
            intercom_if->client_status = STATUS_WAITING_PLAY;
            return 0;
        }

        p = strstr(buf, INTERCOM_PREFIX);
        if (!p) {
            AlexaWriteRecordData(buf, left);
#ifdef TMP_RECORD
            if (g_record_intercom) {
                fwrite(buf, 1, left, g_record_intercom);
            }
#endif
            printf("%s %d status=%d, header not found\n", __FUNCTION__, __LINE__, stat);
            return -1;
        }

        // p += strlen(INTERCOM_PREFIX);
        left -= strlen(INTERCOM_PREFIX);
        if (left <= 0) {
            printf("%s %d status=%d, data not found\n", __FUNCTION__, __LINE__, stat);
            return 0;
        }

        if (left < DATA_PREFIX_LEN) {
            printf("%s %d left=%d\n", __FUNCTION__, __LINE__, left);
            return 0;
        }

        int seq = 0;
        int seq_len = 0;
        if (sscanf(buf, "seq=%d&len=%d&", &seq, &seq_len) != 2) {
            printf("got seq and len failed\n");
            return -1;
        }

        printf("seq=%d, len=%d, left=%d\n", seq, seq_len, left);

        if (seq > 0 && seq_len > 0) {
            AlexaWriteRecordData(buf + DATA_PREFIX_LEN, left - DATA_PREFIX_LEN);
#ifdef TMP_RECORD
            if (g_record_intercom) {
                fwrite(buf + DATA_PREFIX_LEN, 1, left - DATA_PREFIX_LEN, g_record_intercom);
            }
#endif
        }
    }
    return 0;
}

static void *record_recv_thread(void *arg)
{
    char buf[16384] = {0};
    int len = 0;
    int ret = 0;
    int role = 0;
    struct timeval tv;
    fd_set rfds;
    intercom_record_t *intercom_if = (intercom_record_t *)arg;

    if (!intercom_if || intercom_if->client_fd < 0) {
        return NULL;
    }

    intercom_if->client_status = STATUS_WAITING_START;

    send(intercom_if->client_fd, "FREE", 4, 0);
    FD_ZERO(&rfds);
    FD_SET(intercom_if->client_fd, &rfds);

    switch_content_to_background(1);
    AlexaInitRecordData();
    SocketClientReadWriteMsg("/tmp/RequestASRTTS", "talkstart:3", strlen("talkstart:3"), 0, 0, 0);

#ifdef TMP_RECORD
    g_record_intercom = fopen("/tmp/web/intercom.pcm", "wb+");
#endif

    while (1) {
        tv.tv_sec = 3;
        tv.tv_usec = 0;

        ret = select(intercom_if->client_fd + 1, &rfds, NULL, NULL, &tv);
        if (ret <= 0) {
            printf("ret=%d, quit since no data received for 2 seconds\n", ret);
            goto quit;
        }

        len = (int)recv(intercom_if->client_fd, buf, sizeof(buf), 0);
        // printf("%s recved %d bytes\n", __FUNCTION__, len);
        if (len <= 0) {
            printf("data recv error errno = %d\n", errno);
            goto quit;
        }

        buf[len] = '\0';
        ret = parse_buf(intercom_if, buf, len);
        if (ret < 0 || STATUS_WAITING_START == intercom_if->client_status) {
            continue;
        }

        if (intercom_if->client_status != STATUS_WAITING_DATA) {
            if (intercom_if->client_status == STATUS_WAITING_PLAY) {
                /* wait master's play command to reduce latency between devices*/
                intercom_if->client_status = STATUS_WAITING_START;
                break;
            }
        }
    }

quit:
    AlexaFinishWriteData();
    SocketClientReadWriteMsg("/tmp/RequestASRTTS", "VAD_FINISH", strlen("VAD_FINISH"), NULL, NULL,
                             0);

#ifdef TMP_RECORD
    if (g_record_intercom) {
        fclose(g_record_intercom);
    }
#endif
    printf("client %d exit\n", intercom_if->client_fd);
    pthread_mutex_lock(&intercom_if->mutex);
    if (intercom_if->client_fd > 0) {
        close(intercom_if->client_fd);
        intercom_if->client_fd = -1;
    }
    intercom_if->intercom_status = 2;
    pthread_mutex_unlock(&intercom_if->mutex);

    return NULL;
}

void *record_listen_thread(void *arg)
{
    int on = 1;
    int ret = -1;
    pthread_t tid = 0;
    int retry_cnt = 5;
    struct sockaddr_in local_addr;
    intercom_record_t *intercom_if = (intercom_record_t *)arg;

    intercom_if->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (intercom_if->listen_fd < 0) {
        perror("socket");
        return NULL;
    }

    memset((void *)&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(intercom_if->port);

    ret = setsockopt(intercom_if->listen_fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
    if (ret < 0) {
        fprintf(stderr, "-----------------%s: TCP_NODELAY fail-------------\n", __FUNCTION__);
    }
    ret =
        setsockopt(intercom_if->listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&on, sizeof(on));
    if (ret < 0) {
        fprintf(stderr, "-----------------%s: SO_REUSEADDR fail-------------\n", __FUNCTION__);
    }

    while (retry_cnt--) {
        ret = bind(intercom_if->listen_fd, (struct sockaddr *)&local_addr, sizeof(local_addr));
        if (ret < 0) {
            fprintf(stderr, "-----------------%s: fd = %d ret = %d bind fail-------------\n",
                    __FUNCTION__, intercom_if->listen_fd, ret);
            sleep(2);
        } else {
            break;
        }
    }
    if (retry_cnt == 0) {
        goto Exit;
    }

    ret = listen(intercom_if->listen_fd, 1);
    if (ret < 0) {
        fprintf(stderr, "%s: listen fail\n", __FUNCTION__);
        goto Exit;
    }

    intercom_if->is_runnning = 1;
    while (intercom_if->is_runnning) {
        struct sockaddr_in from;
        socklen_t fromlen = sizeof(struct sockaddr_in);
        int client_fd = 0;
        printf("%s %d waiting client login fd = %d\n", __FUNCTION__, __LINE__,
               intercom_if->listen_fd);
        memset((void *)&from, 0, sizeof(from));
        client_fd = accept(intercom_if->listen_fd, (struct sockaddr *)&from, &fromlen);
        if (client_fd < 0) {
            fprintf(stderr, "%s: accept fail errno = %d\n", __FUNCTION__, errno);
            sleep(1);
            continue;
        }
        // printf("%s ret=%d new client %s\n", __FUNCTION__, ret, inet_ntoa(from.sin_addr));

        // do not serve new client if other client do not out (playing or transfering)
        pthread_mutex_lock(&intercom_if->mutex);
        if (intercom_if->intercom_status == 1) {
            send(client_fd, "BUSY", 4, 0);
            close(client_fd);
            fprintf(stderr, "intercom is busy\n");
            pthread_mutex_unlock(&intercom_if->mutex);
            continue;
            /* previous client exited */
        } else if (intercom_if->intercom_status == 2) {
            if (tid > 0) {
                ret = pthread_join(tid, (void **)NULL);
                printf("==%s %d pthread_join ret=%d===\n", __FUNCTION__, __LINE__, ret);
                tid = 0;
            }
        }
        intercom_if->intercom_status = 1;
        intercom_if->client_fd = client_fd;
        printf("client_fd = %d\n", intercom_if->client_fd);
        ret = pthread_create(&tid, NULL, record_recv_thread, (void *)intercom_if);
        if (ret != 0) {
            printf("pthread_create failed(%d)\n", ret);
            intercom_if->intercom_status = 0;
        }
        pthread_mutex_unlock(&intercom_if->mutex);
    }

Exit:
    pthread_mutex_lock(&intercom_if->mutex);
    if (intercom_if->listen_fd > 0) {
        close(intercom_if->listen_fd);
        intercom_if->listen_fd = -1;
    }
    pthread_mutex_unlock(&intercom_if->mutex);

    return NULL;
}

int intercom_init(void)
{
    int ret = 0;

    memset((void *)&g_record_ctx, 0x0, sizeof(intercom_record_t));
    pthread_mutex_init(&g_record_ctx.mutex, NULL);
    g_record_ctx.port = INTERCOM_RECV_PORT;

    ret = pthread_create(&g_record_ctx.recv_tid, NULL, record_listen_thread, (void *)&g_record_ctx);
    if (0 != ret) {
        printf("intercom record failed\n");
        pthread_mutex_destroy(&g_record_ctx.mutex);
        return ret;
    }
    printf("intercom record OK\n");
    return 0;
}

int intercom_uninit(void)
{
    int ret = 0;

    g_record_ctx.is_runnning = 0;
    if (g_record_ctx.recv_tid > 0) {
        pthread_join(g_record_ctx.recv_tid, NULL);
    }

    pthread_mutex_lock(&g_record_ctx.mutex);
    if (g_record_ctx.listen_fd > 0) {
        close(g_record_ctx.listen_fd);
        g_record_ctx.listen_fd = -1;
    }
    if (g_record_ctx.client_fd > 0) {
        close(g_record_ctx.client_fd);
        g_record_ctx.client_fd = -1;
    }
    pthread_mutex_unlock(&g_record_ctx.mutex);

    pthread_mutex_destroy(&g_record_ctx.mutex);

    return 0;
}
