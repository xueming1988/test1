#ifndef __AUDIOCAPTURESERVER_H__
#define __AUDIOCAPTURESERVER_H__

#ifdef __AUDIOCAPTURESERVER_C__
#define __ACS_DEC__
#else
#define __ACS_DEC__ extern
#endif

#include "lp_list.h"
#include "wm_api.h"

#define SIZE_RECEIVE_BUF 1024
#define CMD_HEADER_MAGIC 0x12345678

#define CAPTURE_SERVER_SERVER_VERSION 0x0001
#define SUPPORT_MAX_STREAMS 5
#define STREAM_MIC_BYPASS_INDEX 0
#define STREAM_AFTER_RESAMPLE_INDEX 1
#define STREAM_AFTER_SOFT_PROCESS_INDEX 2

#define CMD_RESEND_PACKET 1
#define CMD_RESPONSE_OK 2
#define CMD_SEND_DATA 3
#define CMD_AUDIO_PARAM 4
#define CMD_START_CAPTURE 5
#define CMD_PAUSE_CATPURE 6
#define CMD_STOP_CATPURE 7
#define CMD_MISC_PARAM 8
#define CMD_SET_STREAMS 9
#define CMD_STREAMS_DATA0 10
#define CMD_STREAMS_DATA1 11
#define CMD_STREAMS_DATA2 12
#define CMD_STREAMS_DATA3 13
#define CMD_STREAMS_DATA4 14
#define CMD_STREAMS_DATA5 15

#define CAPTURE_STATUS_STOP 0
#define CAPTURE_STATUS_RUNNING 1
#define CAPTURE_STATUS_PAUSE 2

#define CAPTURE_STATUS_STOP 0
#define CAPTURE_STATUS_RUNNING 1
#define CAPTURE_STATUS_PAUSE 2

struct _stPacket {
    unsigned int nMagic;
    unsigned int size;
    unsigned int cmd;
    unsigned int index;
    unsigned int checksum;
};

struct _stCaptureBufNode {
    struct lp_list_head node;
    int nIndex;
    int nSize;
    int nTimeStamp;
    int nStreamIndex;
    char *pBuf;
};

struct _stCaptureServerInfo {
    int nServerPort;
    int nPacketIndex;
    int bConnected;
    int hListenSocket;
    int hClientSocket;
    int bListenThreadStop;
    int bSendThreadStop;
    pthread_t hListenThread;
    pthread_t hSendThread;
    pthread_mutex_t mutex;
    // sem_t send_sem;
    struct lp_list_head list_data;
    struct lp_list_head list_junk;
    char *pRecvBuf;
    char *pWritePos;
    int nSampleRate;
    int nBits;
    int nChannel;
    int nCaptureStatus;
    int nPacketCacheSize;
    int nJunkPacketCacheSize;
    stMiscParam stPrepParam;
    int nPrepSampleRate;
    int nPrepBits;
    int nPrepChannel;
    int nVersion;
    int nClientVersion;
    unsigned char nStreamsSupportedChannelMap[SUPPORT_MAX_STREAMS];
    unsigned char
        nStreamsChannelMap[SUPPORT_MAX_STREAMS]; // one bit indicate one channel, max is 8channels
    unsigned int nStreamsSupportedSamplerates[SUPPORT_MAX_STREAMS];
    unsigned char nStreamsSupportedBits[SUPPORT_MAX_STREAMS];
};

__ACS_DEC__ int CaptureServerInit(void *priv);
__ACS_DEC__ int CaptureServerDeinit(void *priv);
__ACS_DEC__ int CaptureServerSaveData(char *buf, int size, void *priv, int nStreamIndex);
__ACS_DEC__ int CaptureServerPostSaveData(char *buf, int size, void *priv);
__ACS_DEC__ int CaptureServerPreSaveData(char *buf, int size, void *priv);
__ACS_DEC__ int CaptureServerStreamSaveData(char *buf, int size, void *priv, int nStreamIndex);

#endif
