/*
	Copyright (C) 2011-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "HTTPMessage.h"

#include "CommonServices.h"
#include "HTTPUtils.h"
#include "NetUtils.h"

#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

#if (TARGET_OS_POSIX)
#include <sys/stat.h>
#include <unistd.h>
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

#define kFileWriteBufferLen (1 * kBytesPerMegaByte)

static void _HTTPMessageGetTypeID(void* inContext);
static void _HTTPMessageFinalize(CFTypeRef inCF);

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static dispatch_once_t gHTTPMessageInitOnce = 0;
static CFTypeID gHTTPMessageTypeID = _kCFRuntimeNotATypeID;
static const CFRuntimeClass kHTTPMessageClass = {
    0, // version
    "HTTPMessage", // className
    NULL, // init
    NULL, // copy
    _HTTPMessageFinalize, // finalize
    NULL, // equal -- NULL means pointer equality.
    NULL, // hash  -- NULL means pointer hash.
    NULL, // copyFormattingDesc
    NULL, // copyDebugDesc
    NULL, // reclaim
    NULL // refcount
};

//===========================================================================================================================
//	HTTPMessageGetTypeID
//===========================================================================================================================

CFTypeID HTTPMessageGetTypeID(void)
{
    dispatch_once_f(&gHTTPMessageInitOnce, NULL, _HTTPMessageGetTypeID);
    return (gHTTPMessageTypeID);
}

static void _HTTPMessageGetTypeID(void* inContext)
{
    (void)inContext;

    gHTTPMessageTypeID = _CFRuntimeRegisterClass(&kHTTPMessageClass);
    check(gHTTPMessageTypeID != _kCFRuntimeNotATypeID);
}

//===========================================================================================================================
//	HTTPMessageCreate
//===========================================================================================================================

OSStatus HTTPMessageCreate(HTTPMessageRef* outMessage)
{
    OSStatus err;
    HTTPMessageRef me;
    size_t extraLen;

    extraLen = sizeof(*me) - sizeof(me->base);
    me = (HTTPMessageRef)_CFRuntimeCreateInstance(NULL, HTTPMessageGetTypeID(), (CFIndex)extraLen, NULL);
    require_action(me, exit, err = kNoMemoryErr);
    memset(((uint8_t*)me) + sizeof(me->base), 0, extraLen);

    me->maxBodyLen = kHTTPDefaultMaxBodyLen;
    HTTPMessageReset(me);

    *outMessage = me;
    me = NULL;
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	_HTTPMessageFinalize
//===========================================================================================================================

static void _HTTPMessageFinalize(CFTypeRef inCF)
{
    HTTPMessageRef const me = (HTTPMessageRef)inCF;

    HTTPMessageReset(me);
    ForgetMem(&me->requestHeader);
    ForgetPtrLen(&me->requestBodyPtr, &me->requestBodyLen);
}

//===========================================================================================================================
//	HTTPMessageReset
//===========================================================================================================================

void HTTPMessageReset(HTTPMessageRef inMsg)
{
    inMsg->header.len = 0;
    inMsg->headerRead = false;
    inMsg->bodyPtr = inMsg->smallBodyBuf;
    inMsg->bodyLen = 0;
    inMsg->bodyOffset = 0;
    ForgetMem(&inMsg->bigBodyBuf);
    inMsg->timeoutNanos = kHTTPNoTimeout;
}

//===========================================================================================================================
//	HTTPMessageReadMessage
//===========================================================================================================================

OSStatus HTTPMessageReadMessage(HTTPMessageRef inMsg, NetTransportRead_f inRead_f, void* inRead_ctx)
{
    HTTPHeader* const hdr = &inMsg->header;
    OSStatus err;
    size_t len;

    if (!inMsg->headerRead) {
        err = HTTPReadHeader(hdr, inRead_f, inRead_ctx);
        require_noerr_quiet(err, exit);
        inMsg->headerRead = true;

        require_action(hdr->contentLength <= inMsg->maxBodyLen, exit, err = kSizeErr);
        err = HTTPMessageSetBodyLength(inMsg, (size_t)hdr->contentLength);
        require_noerr(err, exit);
    }
    len = hdr->extraDataLen;
    if (len > 0) {
        len = Min(len, inMsg->bodyLen);
        memmove(inMsg->bodyPtr, hdr->extraDataPtr, len);
        hdr->extraDataPtr += len;
        hdr->extraDataLen -= len;
        inMsg->bodyOffset += len;
    }

    len = inMsg->bodyOffset;
    if (len < inMsg->bodyLen) {
        err = inRead_f(inMsg->bodyPtr + len, inMsg->bodyLen - len, &len, inRead_ctx);
        require_noerr_quiet(err, exit);
        inMsg->bodyOffset += len;
    }
    err = (inMsg->bodyOffset == inMsg->bodyLen) ? kNoErr : EWOULDBLOCK;

exit:
    return (err);
}

//===========================================================================================================================
//	HTTPMessageWriteMessage
//===========================================================================================================================

OSStatus HTTPMessageWriteMessage(HTTPMessageRef inMsg, NetTransportWriteV_f inWriteV_f, void* inWriteV_ctx)
{
    OSStatus err;

    err = inWriteV_f(&inMsg->iop, &inMsg->ion, inWriteV_ctx);
    require_noerr_quiet(err, exit);

exit:
    return (err);
}

//===========================================================================================================================
//	HTTPMessageSetBody
//===========================================================================================================================

OSStatus HTTPMessageSetBody(HTTPMessageRef inMsg, const char* inContentType, const void* inData, size_t inLen)
{
    OSStatus err;

    err = inMsg->header.firstErr;
    require_noerr(err, exit);

    err = HTTPMessageSetBodyLength(inMsg, inLen);
    require_noerr(err, exit);
    if (inData && (inData != inMsg->bodyPtr)) // Handle inData pointing to the buffer.
    {
        memmove(inMsg->bodyPtr, inData, inLen); // memmove in case inData is in the middle of the buffer.
    }

    HTTPHeader_SetField(&inMsg->header, kHTTPHeader_ContentLength, "%zu", inLen);
    if (inContentType)
        HTTPHeader_SetField(&inMsg->header, kHTTPHeader_ContentType, inContentType);

exit:
    if (err && !inMsg->header.firstErr)
        inMsg->header.firstErr = err;
    return (err);
}

//===========================================================================================================================
//	HTTPMessageSetBodyPtr
//===========================================================================================================================

OSStatus HTTPMessageSetBodyPtr(HTTPMessageRef inMsg, const char* inContentType, const void* inData, size_t inLen)
{
    OSStatus err;

    err = inMsg->header.firstErr;
    require_noerr(err, exit);

    ForgetMem(&inMsg->bigBodyBuf);
    inMsg->bigBodyBuf = (uint8_t*)inData;
    inMsg->bodyPtr = inMsg->bigBodyBuf;
    inMsg->bodyLen = inLen;

    HTTPHeader_AddFieldF(&inMsg->header, kHTTPHeader_ContentLength, "%zu", inLen);
    if (inContentType)
        HTTPHeader_AddField(&inMsg->header, kHTTPHeader_ContentType, inContentType);

exit:
    return (err);
}

//===========================================================================================================================
//	HTTPMessageSetBodyLength
//===========================================================================================================================

OSStatus HTTPMessageSetBodyLength(HTTPMessageRef inMsg, size_t inLen)
{
    OSStatus err;

    ForgetMem(&inMsg->bigBodyBuf);
    if (inLen <= sizeof(inMsg->smallBodyBuf)) {
        inMsg->bodyPtr = inMsg->smallBodyBuf;
    } else {
        inMsg->bigBodyBuf = (uint8_t*)malloc(inLen);
        require_action(inMsg->bigBodyBuf, exit, err = kNoMemoryErr);
        inMsg->bodyPtr = inMsg->bigBodyBuf;
    }
    inMsg->bodyLen = inLen;
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	HTTPMessageGetOrCopyFormVariable
//===========================================================================================================================

OSStatus
HTTPMessageGetOrCopyFormVariable(
    HTTPMessageRef inMsg,
    const char* inName,
    const char** outValuePtr,
    size_t* outValueLen,
    char** outValueStorage)
{
    OSStatus err;
    const char* ptr;
    const char* end;

    ptr = inMsg->header.url.queryPtr;
    end = ptr + inMsg->header.url.queryLen;
    err = URLGetOrCopyVariable(ptr, end, inName, outValuePtr, outValueLen, outValueStorage, NULL);
    if (err) {
        ptr = (const char*)inMsg->bodyPtr;
        end = ptr + inMsg->bodyLen;
        err = URLGetOrCopyVariable(ptr, end, inName, outValuePtr, outValueLen, outValueStorage, NULL);
    }
    return (err);
}
