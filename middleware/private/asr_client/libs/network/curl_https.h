#ifndef __CURL_HTTPS_H__
#define __CURL_HTTPS_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <curl/curl.h>
#include <pthread.h>
#include "membuffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _curl_https_s curl_https_t;

typedef struct _curl_result_s {
    int data_flag;
    int recv_head;
    membuffer result_header;
    membuffer result_body;
} curl_result_t;

#if defined(ASR_DCS_MODULE)
int CurlHttpsGlobalInit(void);
#endif

curl_https_t *CurlHttpsInit(void);

int CurlHttpsUnInit(curl_https_t **_this);

CURLcode CurlHttpsRun(curl_https_t *_this, int retry_count);

CURLMcode CurlHttpsMultiRun(curl_https_t *_this);

CURLcode CurlHttpsWaitFinished(curl_https_t *_this);

int CurlHttpsSetMethod(curl_https_t *_this, char *method);

int CurlHttpsSetVerSion(curl_https_t *_this, long version);
int CurlHttpsSetPipeWait(curl_https_t *_this, long enable);

int CurlHttpsSetDepends(curl_https_t *_this, curl_https_t *_depends);

int CurlHttpsSetUrl(curl_https_t *_this, char *url);

int CurlHttpsSetSpeedLimit(curl_https_t *_this, long speed_time_sec, long speed_bytes);

int CurlHttpsSetGet(curl_https_t *_this, long useget);

int CurlHttpsSetVerbose(curl_https_t *_this, long onoff);

int CurlHttpsSetTimeOut(curl_https_t *_this, long time_out);

int CurlHttpsSetHeader(curl_https_t *_this, struct curl_slist *header);

int CurlHttpsSetBody(curl_https_t *_this, char *post);
int CurlHttpsSetFormBody(curl_https_t *_this, struct curl_httppost *formpost);

int CurlHttpsSetReadCallBack(curl_https_t *_this, void *read_cb, void *data);

int CurlHttpsSetHeaderCallBack(curl_https_t *_this, void *header_cb, void *data);

int CurlHttpsSetWriteCallBack(curl_https_t *_this, void *write_cb, void *data);

int CurlHttpsSetProgressCallBack(curl_https_t *_this, void *progress_cb, void *data);

int CurlHttpsGetHttpRetCode(curl_https_t *_this);

curl_result_t *CurlHttpsGetResult(curl_https_t *_this);

int CurlHttpsCancel(void);

#ifdef __cplusplus
}
#endif

#endif
