#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include "pthread.h"
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <stddef.h>
#include <errno.h>

#include "soft_pre_process.h"

typedef struct {
    int sock[2];
    int blocking;
    char *path; // socket name
} SOCKET_CONTEXT, *PSOCKET_CONTEXT;

int mic_channel_test_mode = 0;

void ASR_Finished_Alexa(void) {}
void ASR_Enable_Alexa(void) {}
void ASR_Disable_Alexa(void) {}
void ASR_Start_Alexa(void) {}
void ASR_Stop_Alexa(void) {}
int far_field_judge(void *input) { return 1; }
long long tickSincePowerOn(void)
{
    long long sec;
    long long msec;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    sec = ts.tv_sec;
    msec = (ts.tv_nsec + 500000) / 1000000;
    return sec * 1000 + msec;
}

void wiimu_log(int level, int type, int index, long value, char *format, ...) {}

int UnixSocketServer_Init(PSOCKET_CONTEXT pSock)
{
    int s, len, val;
    struct sockaddr_un local;
    signal(SIGPIPE, SIG_IGN);

    pSock->sock[0] = -1;
    pSock->sock[1] = -1;
    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        printf("socket open fail!!\n");
        return -1;
    }

    val = fcntl(s, F_GETFD);
    val |= FD_CLOEXEC;
    fcntl(s, F_SETFD, val);

    local.sun_family = AF_UNIX;
    strncpy(local.sun_path, pSock->path, sizeof(local.sun_path) - 1);
    unlink(local.sun_path);
    len = strlen(local.sun_path) + sizeof(local.sun_family);
    if (bind(s, (struct sockaddr *)&local, len) == -1) {
        printf("bind fail!!\n");
        close(s);
        return -1;
    }

    if (listen(s, 5) == -1) {
        printf("listen fail!! \n");
        close(s);
        return -1;
    }

    pSock->sock[0] = s;
    return pSock->sock[0];
}

void UnixSocketServer_deInit(PSOCKET_CONTEXT pSock)
{
    if (pSock->sock[0] < 0)
        return;
    close(pSock->sock[0]);
    pSock->sock[0] = -1;
}

int UnixSocketServer_accept(PSOCKET_CONTEXT pSock)
{
    // int retval;
    struct timeval tv;
    socklen_t t;
    struct sockaddr_un remote;
    // fd_set rfds;

    if (pSock->sock[0] < 0)
        return -1;

    tv.tv_sec = 3;
    tv.tv_usec = 0;

    // FD_ZERO(&rfds);
    // FD_SET(pSock->sock[0], &rfds);

    if (pSock->blocking) {

        // retval = select(pSock->sock[0]+1, &rfds, NULL, NULL, NULL);
    } else {
        // retval = select(pSock->sock[0]+1, &rfds, NULL, NULL, &tv);

        setsockopt(pSock->sock[0], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    //  if (retval == -1){
    //      return -1;
    //  }
    //  else if (retval > 0){
    //
    // }
    //  else {
    //      return 0;
    //   }
    //
    t = sizeof(struct sockaddr_un);
    if ((pSock->sock[1] = accept(pSock->sock[0], (struct sockaddr *)&remote, &t)) == -1) {
        if (errno == EAGAIN) {
            return 0;
        }
        return -1;
    }

    return pSock->sock[1];
}

void UnixSocketServer_close(PSOCKET_CONTEXT pSock)
{
    if (pSock->sock[1] > 0)
        close(pSock->sock[1]);
    pSock->sock[1] = -1;
}

int UnixSocketServer_read(PSOCKET_CONTEXT pSock, char *buffer, int bufferSize)
{
    fd_set rdSet;
    struct timeval tv;
    int nRet;
    if (pSock->sock[1] < 0)
        return -1;

    FD_ZERO(&rdSet);
    FD_SET(pSock->sock[1], &rdSet);
    if (!pSock->blocking)
        tv.tv_sec = 3;
    else
        tv.tv_sec = 8;
    tv.tv_usec = 0;

    nRet = select(pSock->sock[1] + 1, &rdSet, NULL, NULL, &tv);
    if (nRet <= 0) {
        printf("UnixSocketServer_read select error\n");
        return -1;
    }

    return recv(pSock->sock[1], buffer, bufferSize, 0);
}

int UnixSocketClient_Init(PSOCKET_CONTEXT pSock)
{
    int s, len, retry = 3;
    struct sockaddr_un remote;
    struct timeval tv;
    pSock->sock[0] = 0;
    pSock->sock[1] = 0;
    signal(SIGPIPE, SIG_IGN);

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        printf("UnixSocketClient fail!!\n");
        return -1;
    }

    remote.sun_family = AF_UNIX;
    strncpy(remote.sun_path, pSock->path, sizeof(remote.sun_path) - 1);
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);

    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    while (retry--) {
        if (connect(s, (struct sockaddr *)&remote, len) == -1) {
            usleep(40);
            continue;
        } else {
            pSock->sock[0] = s;
            return pSock->sock[0];
        }
    }

    close(s);
    return -1;
}

void UnixSocketClient_DeInit(PSOCKET_CONTEXT pSock)
{
    if (pSock->sock[0] < 0)
        return;
    close(pSock->sock[0]);
    pSock->sock[0] = -1;
}

int UnixSocketClient_read(PSOCKET_CONTEXT pSock, char *buffer, int bufferSize)
{
    fd_set rdSet;
    struct timeval tv;
    int nRet;
    if (pSock->sock[0] < 0)
        return -1;

    FD_ZERO(&rdSet);
    FD_SET(pSock->sock[0], &rdSet);
    if (!pSock->blocking)
        tv.tv_sec = 3;
    else
        tv.tv_sec = 15;
    tv.tv_usec = 0;

    nRet = select(pSock->sock[0] + 1, &rdSet, NULL, NULL, &tv);
    if (nRet <= 0) {
        printf("UnixSocketClient_read select error\n");
        return -1;
    }

    return recv(pSock->sock[0], buffer, bufferSize, 0);
}

int UnixSocketClient_write(PSOCKET_CONTEXT pSock, char *buffer, int size)
{
    if (pSock->sock[0] < 0)
        return -1;
    return send(pSock->sock[0], buffer, size, 0);
}

int SocketClientReadWriteMsg(const char *path, char *pIn, int pInlen, char *pOut, int *outlen,
                             int blocking)
{
    int ret;
    SOCKET_CONTEXT msg_socket;

    msg_socket.path = (char *)path;
    msg_socket.blocking = blocking;
    if (outlen)
        *outlen = 0;
    if (pOut)
        pOut[0] = 0;

    ret = UnixSocketClient_Init(&msg_socket);
    if (ret <= 0) {
        printf("Critcal>>>>>>>>>>>>>>>>>>>>>>>> UnixSocketClient_Init %s pIn(%s) ret=%d \r\n", path,
               pIn, ret);
        return -1;
    }

    ret = UnixSocketClient_write(&msg_socket, pIn, pInlen);

    if (ret != pInlen) {
        printf("Critcal>>>>>>>>>>>>>>>>>>>>>>>> UnixSocketClient_writeto %s, pIn(%s) ret = %d \r\n",
               path, pIn, ret);
        UnixSocketClient_DeInit(&msg_socket);
        return -1;
    }

    if (pOut != NULL && outlen != NULL) {
        ret = UnixSocketClient_read(&msg_socket, pOut, 8192);
        if (ret >= 0)
            *outlen = ret;
        else
            printf("UnixSocketClient_readfrom %s pIn(%s) ret %d\n", path, pIn, ret);
    }

    UnixSocketClient_DeInit(&msg_socket);
    return ret;
}
