/*
	Copyright (C) 2012-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
 */

#include "CFCompat.h"
#include "CommonServices.h"
#include "MiscUtils.h"
#include "RandomNumberUtils.h"
#include "StringUtils.h"
#include <pthread.h>

#include "AirPlayReceiverLogRequest.h"

static pthread_mutex_t gLoggingMessagesLock = PTHREAD_MUTEX_INITIALIZER;
static MirroredRingBuffer gLoggingMessagesRingBuffer;

static void _LogCallback(LogPrintFContext* inPFContext, const char* inStr, size_t inLen, void* inContext);

#if (AIRPLAY_LOG_REQUEST)

static void _createLogsDataBuffer(void* inArg);
static OSStatus _logsCreate(AirPlayReceiverServerRef inServer, AirPlayReceiverLogsRef* outLogs);
static void _logFinalize(CFTypeRef inCF);
static CFTypeID _logsGetTypeID(void);
static void _logsTypeIDLogs(void* inContext);

static HTTPStatus _requestProcessLogsInitiate(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest);
static HTTPStatus _requestProcessLogsRetrieve(AirPlayReceiverConnectionRef inCnx);

static dispatch_once_t gAirPlayReceiverLogsInitOnce = 0;
static CFTypeID gAirPlayReceiverLogsTypeID = _kCFRuntimeNotATypeID;

static const CFRuntimeClass kAirPlayReceiverLogsClass = {
    0, // version
    "AirPlayReceiverLogs", // className
    NULL, // init
    NULL, // copy
    _logFinalize, // finalize
    NULL, // equal -- NULL means pointer equality.
    NULL, // hash  -- NULL means pointer hash.
    NULL, // copyFormattingDesc
    NULL, // copyDebugDesc
    NULL, // reclaim
    NULL // refcount
};
#endif // AIRPLAY_LOG_REQUEST

void AirPlayReceiverLogRequestInit(void)
{
    MirroredRingBufferInit(&gLoggingMessagesRingBuffer, AIRPLAY_LOG_BUFFER_SIZE, true);
    LogSetOutputCallback(NULL, 2, _LogCallback, &gLoggingMessagesRingBuffer);
}

//===========================================================================================================================
//	AirPlayReceiverLogRequestGetLogs
//===========================================================================================================================

HTTPStatus AirPlayReceiverLogRequestGetLogs(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    HTTPStatus status;
#if (AIRPLAY_LOG_REQUEST)
    OSStatus err;
    int matchCount;
    const char* idValue;
    size_t idValueLen;
    unsigned int requestID;

    err = HTTPMessageGetOrCopyFormVariable(inRequest, "id", &idValue, &idValueLen, NULL);
    if (err == kNoErr) {
        matchCount = SNScanF(idValue, idValueLen, "%u", &requestID);
        require_action(matchCount > 0, exit, status = kHTTPStatus_BadRequest);

        require_action(requestID == inCnx->logs->requestID, exit, status = kHTTPStatus_NotFound);
        status = _requestProcessLogsRetrieve(inCnx);

        ForgetCF(&inCnx->logs);
    } else {
        status = _requestProcessLogsInitiate(inCnx, inRequest);
    }
exit:
#else
    (void)inCnx;
    (void)inRequest;

    status = kHTTPStatus_NotFound;
#endif
    return (status);
}

#if (AIRPLAY_LOG_REQUEST)

//===========================================================================================================================
//	_createLogsDataBuffer
//===========================================================================================================================

static void _createLogsDataBuffer(void* inArg)
{
    AirPlayReceiverLogsRef inLogs = (AirPlayReceiverLogsRef)inArg;
    OSStatus err;
    DataBuffer* dataBuffer = NULL;
    CFDictionaryRef dict = NULL;
    char path[PATH_MAX];

    require_action(!inLogs->dataBuffer, exit, err = kInternalErr);

    dataBuffer = (DataBuffer*)malloc(sizeof(DataBuffer));
    require_action(dataBuffer, exit, err = kNoMemoryErr);

    DataBuffer_Init(dataBuffer, NULL, 0, 10 * kBytesPerMegaByte);

    (void)path;
    (void)dict;
    pthread_mutex_lock(&gLoggingMessagesLock);
    uint8_t* ptr = MirroredRingBufferGetReadPtr(&gLoggingMessagesRingBuffer);

    err = DataBuffer_Append(dataBuffer, ptr, MirroredRingBufferGetBytesUsed(&gLoggingMessagesRingBuffer));
    pthread_mutex_unlock(&gLoggingMessagesLock);

    inLogs->dataBuffer = dataBuffer;
    dataBuffer = NULL;

exit:
    inLogs->status = err ? kHTTPStatus_InternalServerError : kHTTPStatus_OK;

    // Must set last for synchronization purposes
    inLogs->pending = false;

    if (dataBuffer)
        DataBuffer_Free(dataBuffer);
    check_noerr(err);

    CFRelease(inLogs);
}

//===========================================================================================================================
//	_logsCreate
//===========================================================================================================================

static OSStatus _logsCreate(AirPlayReceiverServerRef inServer, AirPlayReceiverLogsRef* outLogs)
{
    OSStatus err;
    AirPlayReceiverLogsRef me;
    size_t extraLen;

    extraLen = sizeof(*me) - sizeof(me->base);
    me = (AirPlayReceiverLogsRef)_CFRuntimeCreateInstance(NULL, _logsGetTypeID(), (CFIndex)extraLen, NULL);
    require_action(me, exit, err = kNoMemoryErr);
    memset(((uint8_t*)me) + sizeof(me->base), 0, extraLen);

    me->server = inServer;
    CFRetain(inServer);

    *outLogs = me;
    me = NULL;
    err = kNoErr;

exit:
    if (me)
        CFRelease(me);
    return (err);
}

//===========================================================================================================================
//	_logFinalize
//===========================================================================================================================

static void _logFinalize(CFTypeRef inCF)
{
    AirPlayReceiverLogsRef inLogs = (AirPlayReceiverLogsRef)inCF;

    ForgetCF(&inLogs->server);
    if (inLogs->dataBuffer) {
        DataBuffer_Free(inLogs->dataBuffer);
        inLogs->dataBuffer = NULL;
    }
}

//===========================================================================================================================
//	_logsGetTypeID
//===========================================================================================================================

static CFTypeID _logsGetTypeID(void)
{
    dispatch_once_f(&gAirPlayReceiverLogsInitOnce, NULL, _logsTypeIDLogs);
    return (gAirPlayReceiverLogsTypeID);
}

//===========================================================================================================================
//	_logsTypeIDLogs
//===========================================================================================================================

static void _logsTypeIDLogs(void* inContext)
{
    (void)inContext;

    gAirPlayReceiverLogsTypeID = _CFRuntimeRegisterClass(&kAirPlayReceiverLogsClass);
    check(gAirPlayReceiverLogsTypeID != _kCFRuntimeNotATypeID);
}

//===========================================================================================================================
//	_requestProcessLogsInitiate
//===========================================================================================================================
static HTTPStatus _requestProcessLogsInitiate(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    HTTPStatus status;
    OSStatus err;
    const char* headerPtr;
    size_t headerLen;
    Boolean replyAsync;
    HTTPMessageRef response;

    // We only support one request for logs at a time
    require_action_quiet(!inCnx->logs || !inCnx->logs->pending, exit, status = kHTTPStatus_TooManyRequests);

    // Dispose of stale, unretrieved logs
    ForgetCF(&inCnx->logs);

    // Set up new logs
    err = _logsCreate(inCnx->server, &inCnx->logs);
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

    err = HTTPGetHeaderField(inRequest->header.buf, inRequest->header.len, "Prefer", NULL, NULL, &headerPtr, &headerLen, NULL);
    replyAsync = ((err == kNoErr) && (NULL != strnstr(headerPtr, "respond-async", headerLen)));

    if (!replyAsync) {
        CFRetain(inCnx->logs); // Will be released in _createLogsDataBuffer
        _createLogsDataBuffer(inCnx->logs);
        require_action(inCnx->logs->dataBuffer, exit, status = inCnx->logs->status);

        // Retrieve the results immediately since we were called synchronously
        status = _requestProcessLogsRetrieve(inCnx);
    } else {
        RandomBytes(&inCnx->logs->requestID, sizeof(inCnx->logs->requestID));
        if (inCnx->logs->requestID == 0)
            ++inCnx->logs->requestID; // Make sure it isn't zero

        CFRetain(inCnx->logs); // Will be released in _createLogsDataBuffer
        dispatch_async_f(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), inCnx->logs, _createLogsDataBuffer);

        // Send 202 (Accepted) with Retry-After and Location

        response = inCnx->httpCnx->responseMsg;

        err = HTTPHeader_InitResponse(&response->header, inCnx->httpCnx->delegate.httpProtocol, kHTTPStatus_Accepted, NULL);
        require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

        HTTPHeader_AddFieldF(&response->header, "Retry-After", "5"); // Retry in 5 sec
        HTTPHeader_AddFieldF(&response->header, "Location", "/Logs?id=%u", inCnx->logs->requestID);
        response->bodyLen = 0;

        // Return "OK" here to indicate that we prepared the response message ourselves
        status = kHTTPStatus_OK;
    }
exit:
    return (status);
}

//===========================================================================================================================
//	_requestProcessRetrieveLogs
//===========================================================================================================================
static HTTPStatus _requestProcessLogsRetrieve(AirPlayReceiverConnectionRef inCnx)
{
    HTTPStatus status;
    OSStatus err;
    uint8_t* ptr;
    size_t len;
    HTTPMessageRef response = inCnx->httpCnx->responseMsg;

    require_action(inCnx->logs, exit, status = kHTTPStatus_NotFound);

    if (inCnx->logs->pending) {
        // Send 202 (Accepted) with Retry-After and Location
        err = HTTPHeader_InitResponse(&response->header, inCnx->httpCnx->delegate.httpProtocol, kHTTPStatus_Accepted, NULL);
        require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

        HTTPHeader_AddFieldF(&response->header, "Retry-After", "5"); // Retry in 5 sec
        HTTPHeader_AddFieldF(&response->header, "Location", "/Logs?id=%u", inCnx->logs->requestID);
        response->bodyLen = 0;

        // Return "OK" here to indicate that we prepared the response message ourselves
        status = kHTTPStatus_OK;
    } else {
        if (inCnx->logs->dataBuffer) {
            err = HTTPHeader_InitResponse(&response->header, inCnx->httpCnx->delegate.httpProtocol, kHTTPStatus_OK, NULL);
            require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);
            response->bodyLen = 0;

            err = DataBuffer_Detach(inCnx->logs->dataBuffer, &ptr, &len);
            require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

            err = HTTPMessageSetBodyPtr(response, "application/txt", ptr, len);
            require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);

            DataBuffer_Free(inCnx->logs->dataBuffer);
            inCnx->logs->dataBuffer = NULL;
            inCnx->logs->requestID = 0;

            // Return "OK" here to indicate that we prepared the response message ourselves
            status = kHTTPStatus_OK;
        } else {
            check(inCnx->logs->status != kHTTPStatus_OK);
            status = inCnx->logs->status;
        }

        // Clean up d
        ForgetCF(&inCnx->logs);
    }

exit:
    return (status);
}

#endif // AIRPLAY_LOG_REQUEST

static void _LogCallback(LogPrintFContext* inPFContext, const char* inStr, size_t inLen, void* inContext)
{
    (void)inPFContext;

    size_t len;
    pthread_mutex_lock(&gLoggingMessagesLock);
    MirroredRingBuffer* loggingRingBuffer;
    loggingRingBuffer = (MirroredRingBuffer*)inContext;
    len = MirroredRingBufferGetBytesFree(loggingRingBuffer);
    if (len < inLen) {
        MirroredRingBufferReadAdvance(loggingRingBuffer, inLen - len);
    }
    memcpy(MirroredRingBufferGetWritePtr(loggingRingBuffer), inStr, inLen);
    MirroredRingBufferWriteAdvance(loggingRingBuffer, inLen);
    pthread_mutex_unlock(&gLoggingMessagesLock);
}
