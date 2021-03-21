#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <assert.h>
#include <sys/poll.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <netinet/in.h>
#include <linux/socket.h>
#include <asm/byteorder.h>
#include <stdint.h>
#include "pthread.h"
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h> /* ioctl */
#include <semaphore.h>
#include <linux/tcp.h>

#define __AUDIOCAPTURESERVER_C__
#include "AudioCaptureServer.h"

#define MAX_CACHE_BUFFER_SIZE (16 * 1024 * 1024)

#define DEBUG_INFO(format, ...) // printf(format, ##__VA_ARGS__)
#define DEBUG_INFO_ALWAYS(format, ...) printf(format, ##__VA_ARGS__)
#define DEBUG_ERROR(format, ...) printf(format, ##__VA_ARGS__)

#define MAX(a, b) ((a > b) ? a : b)

#define CaptureDropBuf(pNode)                                                                      \
    {                                                                                              \
        if (pNode->pBuf != NULL)                                                                   \
            free(pNode->pBuf);                                                                     \
        lp_list_del(&pNode->node);                                                                 \
        free(pNode);                                                                               \
    }

static void CapturServerFlush(struct lp_list_head *pHead)
{
    struct _stCaptureBufNode *pNode, *TmpNode;

    lp_list_for_each_entry_safe(pNode, TmpNode, pHead, node) { CaptureDropBuf(pNode); }
}

static unsigned int CaptureServerCalculateCheckSum(char *pBuf, unsigned int size)
{
    unsigned int sum = 0;
    int nAlignByte = sizeof(unsigned int);
    char *p;

    DEBUG_INFO("capture server calculate checksum size: %d\n", size);
    if (pBuf == NULL || size == 0)
        return 0;

    if (size % nAlignByte != 0) {
        memcpy((char *)&sum, pBuf + size - size % nAlignByte, size % nAlignByte);
    } else {
        sum = 0;
    }

    p = pBuf;
    while (p < (pBuf + size - (size % nAlignByte))) {
        sum += *(unsigned int *)p;
        p += nAlignByte;
    }

    return sum;
}

static int CaptureServerSendCmd(void *pContext, unsigned int cmd, int nPacketIndex, char *pExtra,
                                int size)
{
    struct _stCaptureServerInfo *pInfo = (struct _stCaptureServerInfo *)pContext;
    struct _stPacket packet;
    int ret = -1;

    packet.nMagic = CMD_HEADER_MAGIC;
    packet.cmd = cmd;
    packet.index = nPacketIndex;
    packet.size = size;
    packet.checksum = CaptureServerCalculateCheckSum(pExtra, packet.size);

    if (pInfo->hClientSocket != NULL) {
        ret = send(pInfo->hClientSocket, (char *)&packet, sizeof(struct _stPacket), 0);
        if (ret != -1 && pExtra != NULL && size > 0)
            ret = send(pInfo->hClientSocket, pExtra, packet.size, 0);
    }

    return ret;
}

static int CaptureServerExcuteCmd(void *pContext, unsigned int cmd, int nPacketIndex, char *pExtra,
                                  int size)
{
    struct _stCaptureServerInfo *pInfo = (struct _stCaptureServerInfo *)pContext;
    struct _stCaptureBufNode *pDraftNode, *pTmpNode;

    DEBUG_INFO("capture server excute cmd %d(size: %d)\n", cmd, size);

    switch (cmd) {
    case CMD_RESEND_PACKET: {
        int nResponseCmd = *(unsigned int *)pExtra;
        if (pInfo->nCaptureStatus != CAPTURE_STATUS_RUNNING) {
            break;
        }

        pthread_mutex_lock(&pInfo->mutex);
        lp_list_for_each_entry_safe(pDraftNode, pTmpNode, &pInfo->list_junk, node)
        {
            if (nPacketIndex == pDraftNode->nIndex) {
                lp_list_del(&pDraftNode->node);
                lp_list_add(&pDraftNode->node, &pInfo->list_data);
                pInfo->nJunkPacketCacheSize -= pDraftNode->nSize;
                pInfo->nPacketCacheSize += pDraftNode->nSize;
                break;
            }
        }
        pthread_mutex_unlock(&pInfo->mutex);
        DEBUG_INFO_ALWAYS("record server resend %d's packet\n", nPacketIndex);
    } break;

    case CMD_AUDIO_PARAM: {
        unsigned int buf[8 + 3 * SUPPORT_MAX_STREAMS];
        if (size == 0 || pExtra == NULL)
            pInfo->nClientVersion = 0;
        else {
            pInfo->nClientVersion = *(unsigned int *)pExtra;
        }
        DEBUG_INFO_ALWAYS("Capture Client version: %d\n", pInfo->nClientVersion);
        buf[0] = pInfo->nSampleRate;
        buf[1] = pInfo->nBits;
        buf[2] = pInfo->nChannel;
        buf[3] = pInfo->nPrepSampleRate;
        buf[4] = pInfo->nPrepBits;
        buf[5] = pInfo->nPrepChannel;
        buf[6] = pInfo->nVersion;

        CaptureServerSendCmd(pContext, CMD_AUDIO_PARAM, 0, &buf, 6 * sizeof(unsigned int));
        // support streams
        buf[7] = SUPPORT_MAX_STREAMS;
        for (int i = 0; i < SUPPORT_MAX_STREAMS; i++) {
            buf[8 + i] = pInfo->nStreamsSupportedChannelMap[i];
            buf[8 + SUPPORT_MAX_STREAMS + i] = pInfo->nStreamsSupportedSamplerates[i];
            buf[8 + (2 * SUPPORT_MAX_STREAMS) + i] = pInfo->nStreamsSupportedBits[i];
            DEBUG_INFO_ALWAYS("streams %d:\n"
                              "\tchannel: %x\n"
                              "\tsamplerate: %d\n"
                              "\tbits: %d\n",
                              i, pInfo->nStreamsSupportedChannelMap[i],
                              pInfo->nStreamsSupportedSamplerates[i],
                              pInfo->nStreamsSupportedBits[i]);
        }
        CaptureServerSendCmd(pContext, CMD_AUDIO_PARAM, 0, &buf,
                             (8 + 3 * SUPPORT_MAX_STREAMS) * sizeof(unsigned int));
        DEBUG_INFO_ALWAYS("record server send capture parameters\n");
    } break;

    case CMD_MISC_PARAM: {
        struct _stMiscParam *inputCmd = (struct _stMiscParam *)pExtra;
        memcpy(&pInfo->stPrepParam, inputCmd, sizeof(struct _stMiscParam));
        pInfo->stPrepParam.nReserved = (unsigned int)pInfo;
        printf("Tool setup:\n    nCompress:%x\n    nPrepCapture:%x\n    nPrepBitMask:%x\n",
               pInfo->stPrepParam.nCompress, pInfo->stPrepParam.nPrepCapture,
               pInfo->stPrepParam.nPrepBitMask);

        CaptureServerSendCmd(pContext, CMD_MISC_PARAM, 0, NULL, 0);
        DEBUG_INFO_ALWAYS("record server send misc parameters\n");
    } break;

    case CMD_START_CAPTURE:
        if (pInfo->bConnected)
            pInfo->nCaptureStatus = CAPTURE_STATUS_RUNNING;
        CaptureServerSendCmd(pContext, CMD_START_CAPTURE, 0, NULL, 0);
        DEBUG_INFO_ALWAYS("record server start capture\n");
        break;

    case CMD_PAUSE_CATPURE:
        if (pInfo->bConnected)
            pInfo->nCaptureStatus = CAPTURE_STATUS_PAUSE;
        CaptureServerSendCmd(pContext, CMD_PAUSE_CATPURE, 0, NULL, 0);
        DEBUG_INFO_ALWAYS("record server pause capture\n");
        break;

    case CMD_STOP_CATPURE:
        pInfo->nCaptureStatus = CAPTURE_STATUS_STOP;
        pthread_mutex_lock(&pInfo->mutex);
        pInfo->nPacketIndex = 0;
        pInfo->nPacketCacheSize = 0;
        pInfo->nJunkPacketCacheSize = 0;
        CapturServerFlush(&pInfo->list_data);
        CapturServerFlush(&pInfo->list_junk);
        pthread_mutex_unlock(&pInfo->mutex);
        CaptureServerSendCmd(pContext, CAPTURE_STATUS_STOP, 0, NULL, 0);
        DEBUG_INFO_ALWAYS("record server stop capture\n");
        break;

    case CMD_SET_STREAMS: {
        unsigned int *pStreamsCap = pExtra;
        if (pStreamsCap[0] > SUPPORT_MAX_STREAMS) {
            DEBUG_ERROR("Streams number from client is too large");
            pStreamsCap[0] = SUPPORT_MAX_STREAMS;
        }
        for (int i = 0; i < pStreamsCap[0]; i++) {
            pInfo->nStreamsChannelMap[i] =
                pStreamsCap[i + 1] & pInfo->nStreamsSupportedChannelMap[i];
            DEBUG_INFO_ALWAYS("actual streams %d:\n"
                              "\tchannel: %x\n",
                              i, pInfo->nStreamsChannelMap[i]);
        }
        if (pInfo->nStreamsChannelMap[STREAM_MIC_BYPASS_INDEX] != 0)
            pInfo->stPrepParam.nPrepCapture = 1;
        else
            pInfo->stPrepParam.nPrepCapture = 0;

        pInfo->stPrepParam.nPrepBitMask = pInfo->nStreamsChannelMap[STREAM_MIC_BYPASS_INDEX];
        pInfo->stPrepParam.nReserved = (unsigned int)pInfo;
    } break;

    case CMD_RESPONSE_OK: {
        int nResponseCmd = *(unsigned int *)pExtra;
        if (nResponseCmd == CMD_SEND_DATA) {
            pthread_mutex_lock(&pInfo->mutex);
            lp_list_for_each_entry_safe(pDraftNode, pTmpNode, &pInfo->list_junk, node)
            {
                if (nPacketIndex == pDraftNode->nIndex) {
                    DEBUG_INFO("record server response ok to %d's packet\n", nPacketIndex);
                    pInfo->nJunkPacketCacheSize -= pDraftNode->nSize;
                    CaptureDropBuf(pDraftNode);
                    break;
                }
            }
            pthread_mutex_unlock(&pInfo->mutex);
        }
    } break;
    }

    return 0;
}

static void CaptureServerDumpBuf(char *pBuf, int size)
{
    int i;

    for (i = 0; i < size; i++) {
        printf("%02X", (unsigned char)*(pBuf + i));
        if ((i + 1) % 16 == 0) {
            printf("\n");
        } else {
            printf(" ");
        }
    }

    printf("\n");
}

static void CaptureServerParseClientCmd(void *arg)
{
    struct _stCaptureServerInfo *pInfo = (struct _stCaptureServerInfo *)arg;
    char *pHead = pInfo->pRecvBuf;
    char *pEnd = pInfo->pWritePos - 4;
    char *p;
    struct _stPacket *pPacket;
    int nHeaderSize = sizeof(struct _stPacket);

    if ((pInfo->pWritePos - pInfo->pRecvBuf) < nHeaderSize) {
        return;
    }

    // CaptureServerDumpBuf(pInfo->pRecvBuf, pInfo->pWritePos - pInfo->pRecvBuf);
    // find header
    while (pHead < pEnd) {
        if (*(unsigned int *)pHead == CMD_HEADER_MAGIC) {
            DEBUG_INFO("found header at %d\n", pHead - pInfo->pRecvBuf);
            if ((int)(pInfo->pWritePos - pHead) < nHeaderSize) {
                break;
            }

            pPacket = (struct _stPacket *)pHead;
            DEBUG_INFO("Header info:\n");
            DEBUG_INFO("\tsize: %d\n", pPacket->size);
            DEBUG_INFO("\tcmd: %d\n", pPacket->cmd);
            DEBUG_INFO("\tindex: %d\n", pPacket->index);
            DEBUG_INFO("\tchecksum: %d\n", pPacket->checksum);

            if (pPacket->size >= 4096) {
                pHead += 4;
                continue;
            }

            if ((pInfo->pWritePos - pHead) >= (pPacket->size + nHeaderSize)) {
                if (pPacket->checksum ==
                    CaptureServerCalculateCheckSum(pHead + nHeaderSize, pPacket->size)) {
                    CaptureServerExcuteCmd(pInfo, pPacket->cmd, pPacket->index, pHead + nHeaderSize,
                                           pPacket->size);
                    pHead = pHead + nHeaderSize + pPacket->size;
                    continue;
                }
            } else {
                DEBUG_INFO("buf size is smaller than packet size\n");
                break;
            }

            pHead += 4;
        } else {
            pHead++;
        }
    }

    p = pInfo->pRecvBuf;
    while (pHead < pInfo->pWritePos) {
        *(p++) = *(pHead++);
    }
    pInfo->pWritePos = p;
    // CaptureServerDumpBuf(pInfo->pRecvBuf, pInfo->pWritePos - pInfo->pRecvBuf);
}

static void *CaptureServerSendThread(void *arg)
{
    struct _stCaptureServerInfo *pInfo = (struct _stCaptureServerInfo *)arg;
    struct _stCaptureBufNode *pNode, *pTmpNode;
    struct _stPacket packet = {0};

    DEBUG_INFO_ALWAYS("record server send thread running\n");

    // first send the audio capture parameter to client
    while (!pInfo->bSendThreadStop) {
        if (lp_list_empty(&pInfo->list_data)) {
            // sem_wait(&pInfo->send_sem);
            usleep(100000);
            continue;
        }

        pthread_mutex_lock(&pInfo->mutex);
        pNode = lp_list_entry(pInfo->list_data.next, struct _stCaptureBufNode, node);
        lp_list_del(&pNode->node);
        pInfo->nPacketCacheSize -= pNode->nSize;
        pthread_mutex_unlock(&pInfo->mutex);

        DEBUG_INFO("packet index:%d, size:%d, stream:%d\n", pNode->nIndex, pNode->nSize,
                   pNode->nStreamIndex);
        if (pInfo->nClientVersion == 0 && pNode->nStreamIndex != 0) {
            DEBUG_INFO("client version is old, skip the other stream\n");
            free(pNode->pBuf);
            free(pNode);
            continue;
        }

        if (pNode->nStreamIndex == STREAM_MIC_BYPASS_INDEX)
            CaptureServerSendCmd(pInfo, CMD_SEND_DATA, pNode->nIndex, pNode->pBuf, pNode->nSize);
        else if (pInfo->nVersion == 0 && pNode->nStreamIndex == STREAM_AFTER_RESAMPLE_INDEX)
            CaptureServerSendCmd(pInfo, CMD_SEND_DATA, pNode->nIndex, pNode->pBuf, pNode->nSize);
        else
            CaptureServerSendCmd(pInfo, (CMD_STREAMS_DATA0 + pNode->nStreamIndex - 1),
                                 pNode->nIndex, pNode->pBuf, pNode->nSize);

        pthread_mutex_lock(&pInfo->mutex);
        if (pInfo->nJunkPacketCacheSize > MAX_CACHE_BUFFER_SIZE) {
            if (lp_list_empty(&pInfo->list_junk)) {
                DEBUG_ERROR(
                    "record server has issue, junk list is empty, but want to release a buf\n");
            } else {
                pTmpNode = lp_list_entry(pInfo->list_junk.next, struct _stCaptureBufNode, node);
                DEBUG_INFO_ALWAYS("junk list is too more, drop %d's packet\n", pTmpNode->nIndex);
                pInfo->nJunkPacketCacheSize -= pTmpNode->nSize;
                CaptureDropBuf(pTmpNode);
            }
        }
        lp_list_add_tail(&pNode->node, &pInfo->list_junk);
        pInfo->nJunkPacketCacheSize += pNode->nSize;
        pthread_mutex_unlock(&pInfo->mutex);
        // DEBUG_INFO("packet had sent\n");
    }

    DEBUG_INFO_ALWAYS("record server send thread exit\n");

    return NULL;
}

static void *CaptureServerListenThread(void *arg)
{
    struct _stCaptureServerInfo *pInfo = (struct _stCaptureServerInfo *)arg;
    int flags;
    int res;
    int server_len, client_len;
    struct sockaddr_in server_address;
    struct sockaddr_in client_address;
    fd_set fdsr;
    fd_set mExceptionFDS;
    struct timeval tv;
    int maxsock;
    int client;
    int received_bytes;
    int nFreeSize;
    char *pTmpBuf;

    pInfo->hListenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (pInfo->hListenSocket < 0) {
        DEBUG_ERROR("cann't create socket\n");
        return 0;
    }

    int on = 1;
    setsockopt(pInfo->hListenSocket, SOL_SOCKET, SO_REUSEADDR, on, sizeof(on));

    memset(&server_address, 0x0, sizeof(struct sockaddr_in));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(pInfo->nServerPort);
    server_len = sizeof(struct sockaddr_in);
    res = bind(pInfo->hListenSocket, (struct sockaddr *)&server_address, server_len);
    if (res < 0) {
        close(pInfo->hListenSocket);
        DEBUG_ERROR("capture server bind failed\n");
        return 0;
    }

    res = listen(pInfo->hListenSocket, 1);
    if (res == -1) {
        close(pInfo->hListenSocket);
        DEBUG_ERROR("capture server listen failed\n");
        return 0;
    }

    // set non-block
    flags = fcntl(pInfo->hListenSocket, F_GETFL, 0);
    fcntl(pInfo->hListenSocket, F_SETFL, flags | O_NONBLOCK);

    DEBUG_INFO_ALWAYS("record server running, listening on %d port\n", pInfo->nServerPort);
    // wait client connection
    maxsock = pInfo->hListenSocket;
    pTmpBuf = malloc(1400);
    while (!pInfo->bListenThreadStop) {
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        FD_ZERO(&fdsr);
        FD_ZERO(&mExceptionFDS);

        maxsock = pInfo->hListenSocket;
        if (pInfo->hListenSocket > 0) {
            FD_SET(pInfo->hListenSocket, &fdsr);
            FD_SET(pInfo->hListenSocket, &mExceptionFDS);
        }
        if (pInfo->hClientSocket > 0) {
            FD_SET(pInfo->hClientSocket, &fdsr);
            FD_SET(pInfo->hClientSocket, &mExceptionFDS);

            if (maxsock < pInfo->hClientSocket)
                maxsock = pInfo->hClientSocket;
        }

        res = select(maxsock + 1, &fdsr, NULL, &mExceptionFDS, &tv);
        if (res < 0) {
            DEBUG_ERROR("capture server select error\n");
            break;
        } else if (res == 0) {
            // timeout
            continue;
        }

        if (pInfo->hClientSocket > 0 && FD_ISSET(pInfo->hClientSocket, &mExceptionFDS)) {
            DEBUG_ERROR("client exceptrions error\n");
        } else if (FD_ISSET(pInfo->hListenSocket, &mExceptionFDS)) {
            DEBUG_ERROR("server exceptrions error\n");
        } else if (FD_ISSET(pInfo->hListenSocket, &fdsr)) {
            client_len = sizeof(client_address);
            client = accept(pInfo->hListenSocket, (struct sockaddr *)&client_address, &client_len);
            if (client <= 0) {
                DEBUG_ERROR("capture server accept error\n");
                continue;
            }

            if (pInfo->bConnected) {
                DEBUG_INFO_ALWAYS("only support one client, ignore this client\n");
                close(client);
                continue;
            }

            int keepalive = 1;
            int keepidle = 1;
            int keepinterval = 1;
            int keepcount = 2;
            setsockopt(client, SOL_SOCKET, SO_KEEPALIVE, (void *)&keepalive, sizeof(keepalive));
            setsockopt(client, IPPROTO_TCP, TCP_KEEPIDLE, (void *)&keepidle, sizeof(keepidle));
            setsockopt(client, IPPROTO_TCP, TCP_KEEPINTVL, (void *)&keepinterval,
                       sizeof(keepinterval));
            setsockopt(client, IPPROTO_TCP, TCP_KEEPCNT, (void *)&keepcount, sizeof(keepcount));

            pInfo->hClientSocket = client;
            pInfo->bSendThreadStop = 0;
            memset(pInfo->nStreamsChannelMap, 0, sizeof(pInfo->nStreamsChannelMap));
            res = pthread_create(&pInfo->hSendThread, NULL, CaptureServerSendThread, pInfo);
            if (res != 0) {
                DEBUG_ERROR("capture server CaptureServerSendThread create error\n");
            }
            pthread_mutex_lock(&pInfo->mutex);
            pInfo->bConnected = 1;
            pthread_mutex_unlock(&pInfo->mutex);
            DEBUG_INFO_ALWAYS("client has connected\n");
        } else if (pInfo->hClientSocket > 0 && FD_ISSET(pInfo->hClientSocket, &fdsr)) {
            received_bytes = recv(pInfo->hClientSocket, pTmpBuf, 1400, 0);
            if (received_bytes <= 0) {
                close(pInfo->hClientSocket);
                pInfo->hClientSocket = NULL;
                pInfo->pWritePos = pInfo->pRecvBuf;

                pInfo->bSendThreadStop = 1;
                if (pInfo->hSendThread)
                    pthread_join(pInfo->hSendThread, 0);
                pInfo->hSendThread = NULL;

                pthread_mutex_lock(&pInfo->mutex);
                pInfo->bConnected = 0;
                pInfo->nPacketIndex = 0;
                pInfo->nPacketCacheSize = 0;
                pInfo->nJunkPacketCacheSize = 0;
                pInfo->nCaptureStatus = CAPTURE_STATUS_STOP;
                CapturServerFlush(&pInfo->list_data);
                CapturServerFlush(&pInfo->list_junk);
                pthread_mutex_unlock(&pInfo->mutex);

                DEBUG_INFO_ALWAYS("client had disconnected\n");
                continue;
            } else {
                nFreeSize = SIZE_RECEIVE_BUF - (pInfo->pWritePos - pInfo->pRecvBuf);
                if (nFreeSize <= received_bytes) {
                    DEBUG_INFO("receive buf overlow, wait...\n");
                    continue;
                }

                memcpy(pInfo->pWritePos, pTmpBuf, received_bytes);
                pInfo->pWritePos += received_bytes;
            }

            CaptureServerParseClientCmd(pInfo);
        }
    }

    free(pTmpBuf);

    close(pInfo->hListenSocket);
    pInfo->hListenSocket = NULL;

    // sem_post(&pInfo->send_sem);
    pInfo->bSendThreadStop = 1;
    if (pInfo->hSendThread)
        pthread_join(pInfo->hSendThread, 0);
    pInfo->hSendThread = NULL;

    CapturServerFlush(&pInfo->list_data);
    CapturServerFlush(&pInfo->list_junk);

    if (pInfo->hClientSocket != NULL)
        close(pInfo->hClientSocket);

    pInfo->hClientSocket = NULL;

    DEBUG_INFO_ALWAYS("record server exit\n");

    return 0;
}

int CaptureServerInit(void *priv)
{
    struct _stCaptureServerInfo *pInfo = (struct _stCaptureServerInfo *)priv;
    int res;

    pInfo->nVersion = CAPTURE_SERVER_SERVER_VERSION;
    memset(pInfo->nStreamsSupportedChannelMap, 0, sizeof(pInfo->nStreamsSupportedChannelMap));
    // init stream for post process(after resample)
    pInfo->nStreamsSupportedChannelMap[0] = 0xFF;   // 8channel
    pInfo->nStreamsSupportedSamplerates[0] = 48000; // 48KHz samplerate
#if defined(SOFT_PREP_DSPC_MODULE) || defined(SOFT_PREP_DSPC_V2_MODULE)
    pInfo->nStreamsSupportedBits[0] = 32;
#else
    pInfo->nStreamsSupportedBits[0] = 16;
#endif

    pInfo->nStreamsSupportedChannelMap[1] =
        0x07; // left channel and right channel, reference channel, 3channel
#if defined(SOFT_PREP_DSPC_MODULE) || defined(SOFT_PREP_DSPC_V2_MODULE)
    pInfo->nStreamsSupportedSamplerates[1] = 48000; // 48KHz samplerate
    pInfo->nStreamsSupportedBits[1] = 32;
#else
    pInfo->nStreamsSupportedSamplerates[1] = 16000; // 16KHz samplerate
    pInfo->nStreamsSupportedBits[1] = 16;
#endif

    pInfo->nStreamsSupportedChannelMap[2] = 0x0F;   // up to 4 channel
    pInfo->nStreamsSupportedSamplerates[2] = 16000; // 16KHz samplerate
    pInfo->nStreamsSupportedBits[2] = 16;

    pthread_mutex_init(&pInfo->mutex, NULL);

    pthread_mutex_lock(&pInfo->mutex);
    pInfo->nPacketCacheSize = 0;
    pInfo->nPacketIndex = 0;
    LP_INIT_LIST_HEAD(&pInfo->list_data);
    LP_INIT_LIST_HEAD(&pInfo->list_junk);
    // sem_init(&pInfo->send_sem, 0, 0);
    pInfo->pRecvBuf = (char *)malloc(1024);
    pInfo->pWritePos = pInfo->pRecvBuf;
    if (pInfo->pRecvBuf == NULL) {
        DEBUG_ERROR("cann't malloc when server init\n");
        pthread_mutex_unlock(&pInfo->mutex);
        return 0;
    }
    pInfo->bListenThreadStop = 0;
    pthread_mutex_unlock(&pInfo->mutex);
    res = pthread_create(&pInfo->hListenThread, NULL, CaptureServerListenThread, pInfo);

    DEBUG_INFO_ALWAYS("Init capture server, addr:%08X\n", pInfo);

    return 0;
}

int CaptureServerDeinit(void *priv)
{
    struct _stCaptureServerInfo *pInfo = (struct _stCaptureServerInfo *)priv;

    pInfo->bListenThreadStop = 1;
    if (pInfo->hListenThread)
        pthread_join(pInfo->hListenThread, NULL);

    pthread_mutex_lock(&pInfo->mutex);
    CapturServerFlush(&pInfo->list_data);
    CapturServerFlush(&pInfo->list_junk);

    if (pInfo->pRecvBuf != NULL)
        free(pInfo->pRecvBuf);
    pthread_mutex_unlock(&pInfo->mutex);

    pthread_mutex_destroy(&pInfo->mutex);
    DEBUG_INFO_ALWAYS("Deinit capture server\n");

    return 0;
}

int CaptureServerSaveData(char *buf, int size, void *priv, int nStreamIndex)
{
    struct _stCaptureBufNode *pCaptureNode;
    struct _stCaptureServerInfo *pInfo = (struct _stCaptureServerInfo *)priv;

    DEBUG_INFO("add packet, size %d, connect:%d, info:%08X\n", size, pInfo->bConnected, pInfo);

    if (pInfo == NULL || !pInfo->bConnected || pInfo->nCaptureStatus != CAPTURE_STATUS_RUNNING)
        return 0;

    pthread_mutex_lock(&pInfo->mutex);
    if (pInfo->nPacketCacheSize > MAX_CACHE_BUFFER_SIZE) {
        struct _stCaptureBufNode *pNode;
        pNode = lp_list_entry(pInfo->list_data.next, struct _stCaptureBufNode, node);

        DEBUG_ERROR("capture drop buf, index:%d, size:%d\n", pNode->nIndex, pNode->nSize);
        pInfo->nPacketCacheSize -= pNode->nSize;
        CaptureDropBuf(pNode);
    }
    pthread_mutex_unlock(&pInfo->mutex);

    pCaptureNode = (struct _stCaptureBufNode *)malloc(sizeof(struct _stCaptureBufNode));
    if (pCaptureNode == NULL) {
        DEBUG_ERROR("malloc buf node error, not enough memory\n");
        return 0;
    }

    pCaptureNode->pBuf = (char *)malloc(size);
    if (pCaptureNode->pBuf == NULL) {
        DEBUG_ERROR("malloc buf node error, not enough memory\n");
        free(pCaptureNode);
        return 0;
    }

    memcpy(pCaptureNode->pBuf, buf, size);
    pCaptureNode->nSize = size;

    pCaptureNode->nStreamIndex = nStreamIndex;

    pthread_mutex_lock(&pInfo->mutex);
    pCaptureNode->nIndex = (pInfo->nPacketIndex++);
    if (!pInfo->bConnected || pInfo->nCaptureStatus != CAPTURE_STATUS_RUNNING) {
        pthread_mutex_unlock(&pInfo->mutex);
        DEBUG_INFO_ALWAYS("device has disconnect, ignore this packet\n");
        free(pCaptureNode->pBuf);
        free(pCaptureNode);
        return 0;
    }
    pInfo->nPacketCacheSize += size;
    lp_list_add_tail(&pCaptureNode->node, &pInfo->list_data);
    pthread_mutex_unlock(&pInfo->mutex);

    //  DEBUG_INFO_ALWAYS("add %d's packet\n", pCaptureNode->nIndex);
    //  if(pInfo->nPacketCacheSize > 0)
    {
        //  sem_post(&pInfo->send_sem);
    }

    return 0;
}
int CaptureServerPostSaveData(char *buf, int size, void *priv)
{
    struct _stCaptureServerInfo *pInfo = (struct _stCaptureServerInfo *)priv;

    if (pInfo == NULL || (pInfo->nClientVersion == 0 && pInfo->stPrepParam.nPrepCapture) ||
        size == 0)
        return 0;

    if (pInfo->nClientVersion == 1 &&
        pInfo->nStreamsChannelMap[STREAM_AFTER_SOFT_PROCESS_INDEX] == 0)
        return 0;

    return CaptureServerSaveData(
        buf, size, priv, ((pInfo->nClientVersion == 0) ? 0 : STREAM_AFTER_SOFT_PROCESS_INDEX));
}
/*
int CaptureServerPreSaveData(char *buf, int size, void *priv)
{
    stMiscParam *pPrepParam = (stMiscParam *)priv;
    struct _stCaptureServerInfo *pInfo = (struct _stCaptureServerInfo *)pPrepParam->nReserved;

    if(pPrepParam == NULL || (pInfo->nClientVersion == 0 && !pPrepParam->nPrepCapture) || size == 0)
        return 0;

    if(pInfo->nClientVersion == 1 && pInfo->nStreamsChannelMap[STREAM_MIC_BYPASS_INDEX] == 0)
        return 0;

    return CaptureServerSaveData(buf, size, pPrepParam->nReserved, 0);
}
*/
int CaptureServerStreamSaveData(char *buf, int size, void *priv, int nStreamIndex)
{
    stMiscParam *pPrepParam = (stMiscParam *)priv;
    struct _stCaptureServerInfo *pInfo = (struct _stCaptureServerInfo *)pPrepParam->nReserved;

    if (pInfo == NULL || nStreamIndex >= SUPPORT_MAX_STREAMS || size == 0)
        return 0;

    DEBUG_INFO("recv streams %d, size:%d ch:%x\n", nStreamIndex, size,
               pInfo->nStreamsChannelMap[nStreamIndex]);
    if (pInfo->nStreamsChannelMap[nStreamIndex] == 0)
        return 0;

    return CaptureServerSaveData(buf, size, pInfo, nStreamIndex);
}
