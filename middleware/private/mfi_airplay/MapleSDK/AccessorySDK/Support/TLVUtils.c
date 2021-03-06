/*
	Copyright (C) 2008-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "TLVUtils.h"

#include "CommonServices.h"
#include "DataBufferUtils.h"
#include "DebugServices.h"

//===========================================================================================================================
//	TLV8Get
//===========================================================================================================================

OSStatus
TLV8Get(
    const uint8_t* inSrc,
    const uint8_t* inEnd,
    uint8_t inType,
    const uint8_t** outPtr,
    size_t* outLen,
    const uint8_t** outNext)
{
    OSStatus err;
    uint8_t type;
    const uint8_t* ptr;
    size_t len;

    while ((err = TLV8GetNext(inSrc, inEnd, &type, &ptr, &len, &inSrc)) == kNoErr) {
        if (type == inType) {
            if (outPtr)
                *outPtr = ptr;
            if (outLen)
                *outLen = len;
            if (outNext)
                *outNext = inSrc;
            break;
        }
    }
    return (err);
}

//===========================================================================================================================
//	TLV8GetBytes
//===========================================================================================================================

OSStatus
TLV8GetBytes(
    const uint8_t* inSrc,
    const uint8_t* inEnd,
    uint8_t inType,
    size_t inMinLen,
    size_t inMaxLen,
    void* inBuffer,
    size_t* outLen,
    const uint8_t** outNext)
{
    OSStatus err;
    uint8_t* buf = (uint8_t*)inBuffer;
    uint8_t* dst = buf;
    const uint8_t* const lim = buf + inMaxLen;
    const uint8_t* src2;
    uint8_t type;
    const uint8_t* ptr;
    size_t len;

    err = TLV8Get(inSrc, inEnd, inType, &ptr, &len, &inSrc);
    require_noerr_quiet(err, exit);
    require_action_quiet(len <= ((size_t)(lim - dst)), exit, err = kOverrunErr);
    memcpy(dst, ptr, len);
    dst += len;

    for (;;) {
        err = TLV8GetNext(inSrc, inEnd, &type, &ptr, &len, &src2);
        if (err)
            break;
        if (type != inType)
            break;
        inSrc = src2;
        if (len == 0)
            continue;
        require_action_quiet(len <= ((size_t)(lim - dst)), exit, err = kOverrunErr);
        memcpy(dst, ptr, len);
        dst += len;
    }

    len = (size_t)(dst - buf);
    require_action_quiet(len >= inMinLen, exit, err = kUnderrunErr);
    if (outLen)
        *outLen = len;
    if (outNext)
        *outNext = inSrc;
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	TLV8GetSInt64
//===========================================================================================================================

int64_t
TLV8GetSInt64(
    const uint8_t* inSrc,
    const uint8_t* inEnd,
    uint8_t inType,
    OSStatus* outErr,
    const uint8_t** outNext)
{
    int64_t x = 0;
    OSStatus err;
    const uint8_t* ptr;
    size_t len;

    err = TLV8Get(inSrc, inEnd, inType, &ptr, &len, outNext);
    require_noerr_quiet(err, exit);
    x = TLVParseSInt64(ptr, len, &err);

exit:
    if (outErr)
        *outErr = err;
    return (x);
}

//===========================================================================================================================
//	TLV8GetUInt64
//===========================================================================================================================

uint64_t
TLV8GetUInt64(
    const uint8_t* inSrc,
    const uint8_t* inEnd,
    uint8_t inType,
    OSStatus* outErr,
    const uint8_t** outNext)
{
    uint64_t x = 0;
    OSStatus err;
    const uint8_t* ptr;
    size_t len;

    err = TLV8Get(inSrc, inEnd, inType, &ptr, &len, outNext);
    require_noerr_quiet(err, exit);
    x = TLVParseUInt64(ptr, len, &err);

exit:
    if (outErr)
        *outErr = err;
    return (x);
}

//===========================================================================================================================
//	TLV8GetNext
//===========================================================================================================================

OSStatus
TLV8GetNext(
    const uint8_t* inSrc,
    const uint8_t* inEnd,
    uint8_t* outType,
    const uint8_t** outPtr,
    size_t* outLen,
    const uint8_t** outNext)
{
    OSStatus err;
    const uint8_t* ptr;
    size_t len;
    const uint8_t* next;

    // TLV8's have the following format: <1:type> <1:length> <length:data>

    require_action_quiet(inSrc != inEnd, exit, err = kNotFoundErr);
    require_action_quiet(inSrc < inEnd, exit, err = kParamErr);
    len = (size_t)(inEnd - inSrc);
    require_action_quiet(len >= 2, exit, err = kNotFoundErr);

    ptr = inSrc + 2;
    len = inSrc[1];
    next = ptr + len;
    require_action_quiet((next >= ptr) && (next <= inEnd), exit, err = kUnderrunErr);

    *outType = inSrc[0];
    *outPtr = ptr;
    *outLen = len;
    if (outNext)
        *outNext = next;
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	TLV8CopyCoalesced
//===========================================================================================================================

uint8_t*
TLV8CopyCoalesced(
    const uint8_t* inSrc,
    const uint8_t* inEnd,
    uint8_t inType,
    size_t* outLen,
    const uint8_t** outNext,
    OSStatus* outErr)
{
    uint8_t* result = NULL;
    OSStatus err;
    const uint8_t* ptr;
    size_t len;
    uint8_t* storage;
    const uint8_t* next;

    err = TLV8GetOrCopyCoalesced(inSrc, inEnd, inType, &ptr, &len, &storage, &next);
    require_noerr_quiet(err, exit);
    if (storage) {
        result = storage;
    } else {
        result = (uint8_t*)malloc(len ? len : 1); // Use 1 if length is 0 since malloc( 0 ) is undefined.
        require_action(result, exit, err = kNoMemoryErr);
        memcpy(result, ptr, len);
    }
    *outLen = len;
    if (outNext)
        *outNext = inSrc;

exit:
    if (outErr)
        *outErr = err;
    return (result);
}

//===========================================================================================================================
//	TLV8GetOrCopyCoalesced
//===========================================================================================================================

OSStatus
TLV8GetOrCopyCoalesced(
    const uint8_t* inSrc,
    const uint8_t* inEnd,
    uint8_t inType,
    const uint8_t** outPtr,
    size_t* outLen,
    uint8_t** outStorage,
    const uint8_t** outNext)
{
    OSStatus err;
    uint8_t* storage = NULL;
    const uint8_t* resultPtr;
    size_t resultLen;
    const uint8_t* ptr;
    size_t len;
    uint8_t type;
    const uint8_t* src2;
    uint8_t* tempPtr;
    size_t tempLen;

    err = TLV8Get(inSrc, inEnd, inType, &ptr, &len, &inSrc);
    require_noerr_quiet(err, exit);
    resultPtr = ptr;
    resultLen = len;

    for (;;) {
        err = TLV8GetNext(inSrc, inEnd, &type, &ptr, &len, &src2);
        if (err)
            break;
        if (type != inType)
            break;
        inSrc = src2;
        if (len == 0)
            continue;

        if (resultLen > 0) {
            tempLen = resultLen + len;
            tempPtr = (uint8_t*)malloc(tempLen);
            require_action(tempPtr, exit, err = kNoMemoryErr);
            memcpy(tempPtr, resultPtr, resultLen);
            memcpy(tempPtr + resultLen, ptr, len);

            if (storage)
                free(storage);
            storage = tempPtr;
            resultPtr = tempPtr;
            resultLen = tempLen;
        } else {
            resultPtr = ptr;
            resultLen = len;
        }
    }

    *outPtr = resultPtr;
    *outLen = resultLen;
    *outStorage = storage;
    if (outNext)
        *outNext = inSrc;
    storage = NULL;
    err = kNoErr;

exit:
    FreeNullSafe(storage);
    return (err);
}

//===========================================================================================================================

int64_t TLVParseSInt64(const uint8_t* inPtr, size_t inLen, OSStatus* outErr)
{
    int64_t x = 0;
    OSStatus err;

    if (inLen == 1)
        x = (int8_t)*inPtr;
    else if (inLen == 2)
        x = (int16_t)ReadLittle16(inPtr);
    else if (inLen == 4)
        x = (int32_t)ReadLittle32(inPtr);
    else if (inLen == 8)
        x = (int64_t)ReadLittle64(inPtr);
    else {
        err = kSizeErr;
        goto exit;
    }
    err = kNoErr;

exit:
    if (outErr)
        *outErr = err;
    return (x);
}

//===========================================================================================================================

uint64_t TLVParseUInt64(const uint8_t* inPtr, size_t inLen, OSStatus* outErr)
{
    uint64_t x = 0;
    OSStatus err;

    if (inLen == 1)
        x = *inPtr;
    else if (inLen == 2)
        x = ReadLittle16(inPtr);
    else if (inLen == 4)
        x = ReadLittle32(inPtr);
    else if (inLen == 8)
        x = ReadLittle64(inPtr);
    else {
        err = kSizeErr;
        goto exit;
    }
    err = kNoErr;

exit:
    if (outErr)
        *outErr = err;
    return (x);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	TLV8BufferInit
//===========================================================================================================================

void TLV8BufferInit(TLV8Buffer* inBuffer, size_t inMaxLen)
{
    inBuffer->ptr = inBuffer->inlineBuffer;
    inBuffer->len = 0;
    inBuffer->maxLen = inMaxLen;
    inBuffer->mallocedPtr = NULL;
}

//===========================================================================================================================
//	TLV8BufferFree
//===========================================================================================================================

void TLV8BufferFree(TLV8Buffer* inBuffer)
{
    ForgetMem(&inBuffer->mallocedPtr);
}

//===========================================================================================================================
//	TLV8BufferAppend
//===========================================================================================================================

OSStatus TLV8BufferAppend(TLV8Buffer* inBuffer, uint8_t inType, const void* inPtr, size_t inLen)
{
    const uint8_t* src;
    const uint8_t* end;
    OSStatus err;
    size_t len, newLen;
    uint8_t* dst;
    uint8_t* lim;

    DEBUG_USE_ONLY(lim);

    // Calculate the total size of the buffer after the new data is added.
    // This includes the space needed for extra headers if the data needs to be split across multiple items.

    if (inLen == kSizeCString)
        inLen = strlen((const char*)inPtr);
    len = (inLen <= 255) ? (2 + inLen) : ((2 * ((inLen / 255) + ((inLen % 255) ? 1 : 0))) + inLen);
    newLen = inBuffer->len + len;
    require_action(newLen <= inBuffer->maxLen, exit, err = kSizeErr);
    require_action(newLen >= inBuffer->len, exit, err = kOverrunErr);

    if (newLen <= sizeof(inBuffer->inlineBuffer)) {
        dst = &inBuffer->inlineBuffer[inBuffer->len];
        lim = inBuffer->inlineBuffer + newLen;
    } else {
        // Data is too big to fit in the inline buffer so allocate a new buffer to hold everything.

        dst = (uint8_t*)malloc(newLen);
        require_action(dst, exit, err = kNoMemoryErr);
        if (inBuffer->mallocedPtr) {
            memcpy(dst, inBuffer->mallocedPtr, inBuffer->len);
            free(inBuffer->mallocedPtr);
        } else if (inBuffer->len > 0) {
            memcpy(dst, inBuffer->inlineBuffer, inBuffer->len);
        }
        inBuffer->ptr = dst;
        inBuffer->mallocedPtr = dst;
        lim = dst + newLen;
        dst += inBuffer->len;
    }

    // Append the new data. This will split it into 255 byte items as needed.

    src = (const uint8_t*)inPtr;
    end = src + inLen;
    do {
        len = (size_t)(end - src);
        if (len > 255)
            len = 255;
        dst[0] = inType;
        dst[1] = (uint8_t)len;
        if (len > 0)
            memcpy(&dst[2], src, len);
        src += len;
        dst += (2 + len);

    } while (src != end);
    inBuffer->len = (size_t)(dst - inBuffer->ptr);
    check(dst == lim);
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	TLV8BufferAppendSInt64
//===========================================================================================================================

OSStatus TLV8BufferAppendSInt64(TLV8Buffer* inBuffer, uint8_t inType, int64_t x)
{
    uint8_t buf[8];
    size_t len;

    if ((x >= INT8_MIN) && (x <= INT8_MAX)) {
        *buf = (uint8_t)x;
        len = 1;
    } else if ((x >= INT16_MIN) && (x <= INT16_MAX)) {
        WriteLittle16(buf, x);
        len = 2;
    } else if ((x >= INT32_MIN) && (x <= INT32_MAX)) {
        WriteLittle32(buf, x);
        len = 4;
    } else {
        WriteLittle64(buf, x);
        len = 8;
    }
    return (TLV8BufferAppend(inBuffer, inType, buf, len));
}

//===========================================================================================================================
//	TLV8BufferAppendUInt64
//===========================================================================================================================

OSStatus TLV8BufferAppendUInt64(TLV8Buffer* inBuffer, uint8_t inType, uint64_t x)
{
    uint8_t buf[8];
    size_t len;

    if (x <= UINT8_MAX) {
        *buf = (uint8_t)x;
        len = 1;
    } else if (x <= UINT16_MAX) {
        WriteLittle16(buf, x);
        len = 2;
    } else if (x <= UINT32_MAX) {
        WriteLittle32(buf, x);
        len = 4;
    } else {
        WriteLittle64(buf, x);
        len = 8;
    }
    return (TLV8BufferAppend(inBuffer, inType, buf, len));
}

//===========================================================================================================================
//	TLV8BufferDetach
//===========================================================================================================================

OSStatus TLV8BufferDetach(TLV8Buffer* inBuffer, uint8_t** outPtr, size_t* outLen)
{
    OSStatus err;
    uint8_t* ptr;
    size_t len;

    len = inBuffer->len;
    if (inBuffer->mallocedPtr) {
        ptr = inBuffer->mallocedPtr;
    } else {
        ptr = (uint8_t*)malloc(len ? len : 1); // malloc( 1 ) if buffer is empty since malloc( 0 ) is undefined.
        require_action(ptr, exit, err = kNoMemoryErr);
        if (len)
            memcpy(ptr, inBuffer->ptr, len);
    }
    inBuffer->ptr = inBuffer->inlineBuffer;
    inBuffer->len = 0;
    inBuffer->mallocedPtr = NULL;

    *outPtr = ptr;
    *outLen = len;
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	TLV8MaxPayloadBytesForTotalBytes
//===========================================================================================================================

size_t TLV8MaxPayloadBytesForTotalBytes(size_t inN)
{
    size_t n, r;

    if (inN < 2)
        return (0);
    n = inN - (2 * (inN / 257));
    r = inN % 257;
    if (r > 1)
        n -= 2;
    else if (r != 0)
        n -= 1;
    return (n);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	TLV16Get
//===========================================================================================================================

OSStatus
TLV16Get(
    const uint8_t* inSrc,
    const uint8_t* inEnd,
    Boolean inBigEndian,
    uint16_t inType,
    const uint8_t** outPtr,
    size_t* outLen,
    const uint8_t** outNext)
{
    OSStatus err;
    uint16_t type;
    const uint8_t* ptr;
    size_t len;

    while ((err = TLV16GetNext(inSrc, inEnd, inBigEndian, &type, &ptr, &len, &inSrc)) == kNoErr) {
        if (type == inType) {
            *outPtr = ptr;
            *outLen = len;
            break;
        }
    }
    if (outNext)
        *outNext = inSrc;
    return (err);
}

//===========================================================================================================================
//	TLV16GetNext
//===========================================================================================================================

OSStatus
TLV16GetNext(
    const uint8_t* inSrc,
    const uint8_t* inEnd,
    Boolean inBigEndian,
    uint16_t* outType,
    const uint8_t** outPtr,
    size_t* outLen,
    const uint8_t** outNext)
{
    OSStatus err;
    const uint8_t* ptr;
    uint16_t type;
    size_t len;
    const uint8_t* next;

    // 16-bit TLV's have the following format: <2:type> <2:length> <length:data>

    require_action_quiet(inSrc != inEnd, exit, err = kNotFoundErr);
    require_action_quiet(inSrc < inEnd, exit, err = kParamErr);
    len = (size_t)(inEnd - inSrc);
    require_action_quiet(len >= 4, exit, err = kUnderrunErr);

    if (inBigEndian) {
        type = ReadBig16(&inSrc[0]);
        len = ReadBig16(&inSrc[2]);
    } else {
        type = ReadLittle16(&inSrc[0]);
        len = ReadLittle16(&inSrc[2]);
    }
    ptr = inSrc + 4;
    require_action_quiet(((size_t)(inEnd - ptr)) >= len, exit, err = kUnderrunErr);

    next = ptr + len;
    require_action_quiet((next >= ptr) && (next <= inEnd), exit, err = kUnderrunErr);

    *outType = type;
    *outPtr = ptr;
    *outLen = len;
    if (outNext)
        *outNext = next;
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	DataBuffer_AppendTLV16
//===========================================================================================================================

OSStatus DataBuffer_AppendTLV16(DataBuffer* inDB, Boolean inBigEndian, uint16_t inType, const void* inData, size_t inLen)
{
    OSStatus err;
    uint8_t* dst;
    const uint8_t* src;
    const uint8_t* end;

    // 16-bit TLV's have the following format:
    //
    //		<2:type> <2:length> <length:data>

    if (inLen == kSizeCString)
        inLen = strlen((const char*)inData);
    require_action(inLen <= 0xFFFF, exit, err = kSizeErr);

    err = DataBuffer_Grow(inDB, 2 + 2 + inLen, &dst); // Append <2:type> <2:len> <len:value>
    require_noerr(err, exit);

    if (inBigEndian) {
        WriteBig16(dst, inType);
        dst += 2;
        WriteBig16(dst, inLen);
        dst += 2;
    } else {
        WriteLittle16(dst, inType);
        dst += 2;
        WriteLittle16(dst, inLen);
        dst += 2;
    }

    src = (const uint8_t*)inData;
    end = src + inLen;
    while (src < end)
        *dst++ = *src++;
    check(dst == DataBuffer_GetEnd(inDB));

exit:
    if (!inDB->firstErr)
        inDB->firstErr = err;
    return (err);
}

#if 0
#pragma mark -
#endif

#if (!EXCLUDE_UNIT_TESTS)

static OSStatus TLV8Test(void);
static OSStatus TLV16Test(void);

//===========================================================================================================================
//	TLVUtilsTest
//===========================================================================================================================

OSStatus TLVUtilsTest(void)
{
    OSStatus err = TLV8Test();
    if (err)
        return err;
    return TLV16Test();
}

//===========================================================================================================================
//	TLV8Test
//===========================================================================================================================

static OSStatus TLV8Test(void)
{
    OSStatus err;
    const uint8_t* src;
    const uint8_t* src2 = NULL;
    const uint8_t* end;
    uint8_t type = 0;
    const uint8_t* ptr = NULL;
    size_t len = 0;
    size_t i;
    uint8_t* storage = NULL;
    TLV8Buffer tlv8;
    uint8_t* ptr2 = NULL;
    int64_t s64;
    uint8_t buf[32];

    TLV8BufferInit(&tlv8, SIZE_MAX);

// TLV8GetNext

#define kTLV8Test1 ""
    src = (const uint8_t*)kTLV8Test1;
    end = src + sizeof_string(kTLV8Test1);
    err = TLV8GetNext(src, end, &type, &ptr, &len, &src);
    require_action(err == kNotFoundErr, exit, err = kMismatchErr);

#define kTLV8Test2 "\x00"
    src = (const uint8_t*)kTLV8Test2;
    end = src + sizeof_string(kTLV8Test2);
    err = TLV8GetNext(src, end, &type, &ptr, &len, &src);
    require_action(err == kNotFoundErr, exit, err = kMismatchErr);

#define kTLV8Test3 "\x00\x01"
    src = (const uint8_t*)kTLV8Test3;
    end = src + sizeof_string(kTLV8Test3);
    err = TLV8GetNext(src, end, &type, &ptr, &len, &src);
    require(err == kUnderrunErr, exit);

#define kTLV8Test4 "\x11\x00"
    src = (const uint8_t*)kTLV8Test4;
    end = src + sizeof_string(kTLV8Test4);
    err = TLV8GetNext(src, end, &type, &ptr, &len, &src2);
    require_noerr(err, exit);
    require_action(type == 0x11, exit, err = kMismatchErr);
    require_action(ptr == src + 2, exit, err = kMismatchErr);
    require_action(len == 0, exit, err = kMismatchErr);
    require_action(src2 == end, exit, err = kMismatchErr);

#define kTLV8Test5 "\x11\x01\xAA"
    src = (const uint8_t*)kTLV8Test5;
    end = src + sizeof_string(kTLV8Test5);
    src2 = src;
    err = TLV8GetNext(src, end, &type, &ptr, &len, &src2);
    require_noerr(err, exit);
    require_action(type == 0x11, exit, err = kMismatchErr);
    require_action(ptr == src + 2, exit, err = kMismatchErr);
    require_action(len == 1, exit, err = kMismatchErr);
    require_action(memcmp(ptr, "\xAA", len) == 0, exit, err = kMismatchErr);
    require_action(src2 == end, exit, err = kMismatchErr);
    err = TLV8GetNext(src2, end, &type, &ptr, &len, &src2);
    require_action(err == kNotFoundErr, exit, err = kUnexpectedErr);

#define kTLV8Test6 "\x55\x01\x15\x22\x04\xBB\xCC\xDD\xEE"
    src = (const uint8_t*)kTLV8Test6;
    end = src + sizeof_string(kTLV8Test6);
    err = TLV8GetNext(src, end, &type, &ptr, &len, &src2);
    require_noerr(err, exit);
    require_action(type == 0x55, exit, err = kMismatchErr);
    require_action(ptr == src + 2, exit, err = kMismatchErr);
    require_action(len == 1, exit, err = kMismatchErr);
    require_action(memcmp(ptr, "\x15", len) == 0, exit, err = kMismatchErr);
    src = src2;
    err = TLV8GetNext(src, end, &type, &ptr, &len, &src2);
    require_noerr(err, exit);
    require_action(type == 0x22, exit, err = kMismatchErr);
    require_action(ptr == src + 2, exit, err = kMismatchErr);
    require_action(len == 4, exit, err = kMismatchErr);
    require_action(memcmp(ptr, "\xBB\xCC\xDD\xEE", len) == 0, exit, err = kMismatchErr);
    require_action(src2 == end, exit, err = kMismatchErr);

    // TLV8Get

    src = (const uint8_t*)kTLV8Test5;
    end = src + sizeof_string(kTLV8Test5);
    err = TLV8Get(src, end, 0x99, &ptr, &len, &src2);
    require_action(err == kNotFoundErr, exit, err = kMismatchErr);
    err = TLV8Get(src, end, 0x11, &ptr, &len, &src2);
    require_noerr(err, exit);
    require_action(ptr == src + 2, exit, err = kMismatchErr);
    require_action(len == 1, exit, err = kMismatchErr);
    require_action(memcmp(ptr, "\xAA", len) == 0, exit, err = kMismatchErr);
    require_action(src2 == end, exit, err = kMismatchErr);
    err = TLV8Get(src2, end, 0x11, &ptr, &len, &src2);
    require_action(err == kNotFoundErr, exit, err = kMismatchErr);

    src = (const uint8_t*)kTLV8Test6;
    end = src + sizeof_string(kTLV8Test6);
    err = TLV8Get(src, end, 0x99, &ptr, &len, &src2);
    require_action(err == kNotFoundErr, exit, err = kMismatchErr);
    err = TLV8Get(src, end, 0x55, &ptr, &len, &src2);
    require_noerr(err, exit);
    require_action(ptr == src + 2, exit, err = kMismatchErr);
    require_action(len == 1, exit, err = kMismatchErr);
    require_action(memcmp(ptr, "\x15", len) == 0, exit, err = kMismatchErr);
    src = src2;
    err = TLV8Get(src, end, 0x22, &ptr, &len, &src2);
    require_noerr(err, exit);
    require_action(ptr == src + 2, exit, err = kMismatchErr);
    require_action(len == 4, exit, err = kMismatchErr);
    require_action(memcmp(ptr, "\xBB\xCC\xDD\xEE", len) == 0, exit, err = kMismatchErr);
    require_action(src2 == end, exit, err = kMismatchErr);

    // TLV8GetBytes

    src = (const uint8_t*)kTLV8Test5;
    end = src + sizeof_string(kTLV8Test5);
    err = TLV8GetBytes(src, end, 0x99, 0, sizeof(buf), buf, &len, &src2);
    require_action(err == kNotFoundErr, exit, err = kUnexpectedErr);
    err = TLV8GetBytes(src, end, 0x11, 0, 1, buf, &len, &src2);
    require_noerr(err, exit);
    require_action(len == 1, exit, err = kMismatchErr);
    require_action(memcmp(buf, "\xAA", len) == 0, exit, err = kMismatchErr);
    require_action(src2 == end, exit, err = kMismatchErr);
    err = TLV8GetBytes(src2, end, 0x11, 0, 1, buf, &len, &src2);
    require_action(err == kNotFoundErr, exit, err = kMismatchErr);

    src = (const uint8_t*)kTLV8Test6;
    end = src + sizeof_string(kTLV8Test6);
    err = TLV8GetBytes(src, end, 0x99, 0, sizeof(buf), buf, &len, &src2);
    require_action(err == kNotFoundErr, exit, err = kMismatchErr);
    err = TLV8GetBytes(src, end, 0x55, 1, sizeof(buf), buf, &len, &src2);
    require_noerr(err, exit);
    require_action(len == 1, exit, err = kMismatchErr);
    require_action(memcmp(buf, "\x15", len) == 0, exit, err = kMismatchErr);
    src = src2;
    err = TLV8GetBytes(src, end, 0x22, 5, sizeof(buf), buf, &len, &src2);
    require_action(err == kUnderrunErr, exit, err = kMismatchErr);
    err = TLV8GetBytes(src, end, 0x22, 2, sizeof(buf), buf, &len, &src2);
    require_action(len == 4, exit, err = kMismatchErr);
    require_action(memcmp(buf, "\xBB\xCC\xDD\xEE", len) == 0, exit, err = kMismatchErr);
    require_action(src2 == end, exit, err = kMismatchErr);

// TLV8GetOrCopyCoalesced

#define kTLV8Test7 \
    "\x01\x00"     \
    "\x01\x00"     \
    "\x01\x00"
    src = (const uint8_t*)kTLV8Test7;
    end = src + sizeof_string(kTLV8Test7);
    ptr = NULL;
    len = 99;
    storage = NULL;
    err = TLV8GetOrCopyCoalesced(src, end, 0x01, &ptr, &len, &storage, &src2);
    require_noerr(err, exit);
    require_action(ptr, exit, err = kMismatchErr);
    require_action(len == 0, exit, err = kMismatchErr);
    require_action(!storage, exit, err = kMismatchErr);
    require_action(src2 == end, exit, err = kMismatchErr);
    ForgetMem(&storage);

#define kTLV8Test8 \
    "\x01\x01\xAA" \
    "\x01\x00"     \
    "\x01\x00"
    src = (const uint8_t*)kTLV8Test8;
    end = src + sizeof_string(kTLV8Test8);
    ptr = NULL;
    len = 99;
    storage = NULL;
    err = TLV8GetOrCopyCoalesced(src, end, 0x01, &ptr, &len, &storage, &src2);
    require_noerr(err, exit);
    require_action(ptr, exit, err = kMismatchErr);
    require_action(len == 1, exit, err = kMismatchErr);
    require_action(memcmp(ptr, "\xAA", len) == 0, exit, err = kMismatchErr);
    require_action(!storage, exit, err = kMismatchErr);
    require_action(src2 == end, exit, err = kMismatchErr);
    ForgetMem(&storage);

#define kTLV8Test9 \
    "\x01\x00"     \
    "\x01\x01\xAA" \
    "\x01\x00"
    src = (const uint8_t*)kTLV8Test9;
    end = src + sizeof_string(kTLV8Test9);
    ptr = NULL;
    len = 99;
    storage = NULL;
    err = TLV8GetOrCopyCoalesced(src, end, 0x01, &ptr, &len, &storage, &src2);
    require_noerr(err, exit);
    require_action(ptr, exit, err = kMismatchErr);
    require_action(len == 1, exit, err = kMismatchErr);
    require_action(memcmp(ptr, "\xAA", len) == 0, exit, err = kMismatchErr);
    require_action(!storage, exit, err = kMismatchErr);
    require_action(src2 == end, exit, err = kMismatchErr);
    ForgetMem(&storage);

#define kTLV8Test10 \
    "\x01\x00"      \
    "\x01\x01\xAA"  \
    "\x01\x01\xBB"  \
    "\x01\x00"
    src = (const uint8_t*)kTLV8Test10;
    end = src + sizeof_string(kTLV8Test10);
    ptr = NULL;
    len = 99;
    storage = NULL;
    err = TLV8GetOrCopyCoalesced(src, end, 0x01, &ptr, &len, &storage, &src2);
    require_noerr(err, exit);
    require_action(ptr, exit, err = kMismatchErr);
    require_action(len == 2, exit, err = kMismatchErr);
    require_action(memcmp(ptr, "\xAA\xBB", len) == 0, exit, err = kMismatchErr);
    require_action(storage, exit, err = kMismatchErr);
    require_action(src2 == end, exit, err = kMismatchErr);
    ForgetMem(&storage);

#define kTLV8Test11    \
    "\x01\x01\xAA"     \
    "\x01\x02\xBB\xCC" \
    "\x01\x03\xDD\xEE\xFF"
    src = (const uint8_t*)kTLV8Test11;
    end = src + sizeof_string(kTLV8Test11);
    ptr = NULL;
    len = 99;
    storage = NULL;
    err = TLV8GetOrCopyCoalesced(src, end, 0x01, &ptr, &len, &storage, &src2);
    require_noerr(err, exit);
    require_action(ptr, exit, err = kMismatchErr);
    require_action(len == 6, exit, err = kMismatchErr);
    require_action(memcmp(ptr, "\xAA\xBB\xCC\xDD\xEE\xFF", len) == 0, exit, err = kMismatchErr);
    require_action(storage, exit, err = kMismatchErr);
    require_action(src2 == end, exit, err = kMismatchErr);
    ForgetMem(&storage);

#define kTLV8Test12    \
    "\x02\x01\xAA"     \
    "\x03\x02\xBB\xCC" \
    "\x04\x03\xDD\xEE\xFF"
    src = (const uint8_t*)kTLV8Test12;
    end = src + sizeof_string(kTLV8Test12);
    ptr = NULL;
    len = 99;
    storage = NULL;
    err = TLV8GetOrCopyCoalesced(src, end, 0x01, &ptr, &len, &storage, &src2);
    require_action(err == kNotFoundErr, exit, err = kMismatchErr);
    require_action(!ptr, exit, err = kMismatchErr);
    require_action(len == 99, exit, err = kMismatchErr);
    require_action(!storage, exit, err = kMismatchErr);
    ForgetMem(&storage);

#define kTLV8Test13        \
    "\x01\x01\xAA"         \
    "\x01\x02\xBB\xCC"     \
    "\x04\x03\xDD\xEE\xFF" \
    "\x01\x01\x11"
    src = (const uint8_t*)kTLV8Test13;
    end = src + sizeof_string(kTLV8Test13);
    ptr = NULL;
    len = 99;
    storage = NULL;
    err = TLV8GetOrCopyCoalesced(src, end, 0x01, &ptr, &len, &storage, &src2);
    require_noerr(err, exit);
    require_action(ptr, exit, err = kMismatchErr);
    require_action(len == 3, exit, err = kMismatchErr);
    require_action(memcmp(ptr, "\xAA\xBB\xCC", len) == 0, exit, err = kMismatchErr);
    require_action(storage, exit, err = kMismatchErr);
    require_action(src2 == (src + 7), exit, err = kMismatchErr);
    ForgetMem(&storage);
    err = TLV8GetOrCopyCoalesced(src2, end, 0x04, &ptr, &len, &storage, &src2);
    require_noerr(err, exit);
    require_action(ptr == src + 9, exit, err = kMismatchErr);
    require_action(len == 3, exit, err = kMismatchErr);
    require_action(memcmp(ptr, "\xDD\xEE\xFF", len) == 0, exit, err = kMismatchErr);
    require_action(!storage, exit, err = kMismatchErr);
    require_action(src2 == (src + 12), exit, err = kMismatchErr);
    err = TLV8GetOrCopyCoalesced(src2, end, 0x01, &ptr, &len, &storage, &src2);
    require_noerr(err, exit);
    require_action(ptr == src + 14, exit, err = kMismatchErr);
    require_action(len == 1, exit, err = kMismatchErr);
    require_action(memcmp(ptr, "\x11", len) == 0, exit, err = kMismatchErr);
    require_action(!storage, exit, err = kMismatchErr);
    require_action(src2 == end, exit, err = kMismatchErr);
    err = TLV8GetOrCopyCoalesced(src2, end, 0x01, &ptr, &len, &storage, &src2);
    require_action(err != kNoErr, exit, err = kMismatchErr);
    require_action(!storage, exit, err = kMismatchErr);
    require_action(src2 == end, exit, err = kMismatchErr);

    // TLV8Buffer

    TLV8BufferInit(&tlv8, 256);
    err = TLV8BufferAppend(&tlv8, 0x01, "\xAA", 1);
    require_noerr(err, exit);
    err = TLV8BufferAppend(&tlv8, 0x02, "value2", kSizeCString);
    require_noerr(err, exit);
    require_action(TLV8BufferGetLen(&tlv8) == 11, exit, err = kMismatchErr);
    ptr = TLV8BufferGetPtr(&tlv8);
    require_action(ptr == tlv8.inlineBuffer, exit, err = kMismatchErr);
    require_action(!tlv8.mallocedPtr, exit, err = kMismatchErr);
    require_action(memcmp(ptr, "\x01\x01\xAA\x02\x06value2", 11) == 0, exit, err = kMismatchErr);
    TLV8BufferFree(&tlv8);

    TLV8BufferInit(&tlv8, 2000);
    err = TLV8BufferAppend(&tlv8, 0x01, "value1", kSizeCString);
    require_noerr(err, exit);
    err = TLV8BufferAppend(&tlv8, 0x02, "val2", kSizeCString);
    require_noerr(err, exit);
    len = 1093;
    storage = (uint8_t*)malloc(len);
    require_action(storage, exit, err = kNoMemoryErr);
    for (i = 0; i < len; ++i)
        storage[i] = (uint8_t)(i & 0xFF);
    err = TLV8BufferAppend(&tlv8, 0x03, storage, len);
    require_noerr(err, exit);
    ForgetMem(&storage);
    require_action(tlv8.mallocedPtr, exit, err = kMismatchErr);
    ptr = TLV8BufferGetPtr(&tlv8);
    require_action(ptr == tlv8.mallocedPtr, exit, err = kMismatchErr);
    require_action(ptr != tlv8.inlineBuffer, exit, err = kMismatchErr);
    require_action(TLV8BufferGetLen(&tlv8) == 1117, exit, err = kMismatchErr);
    require_action(memcmp(ptr, "\x01\x06value1\x02\x04val2", 14) == 0, exit, err = kMismatchErr);
    ptr = &ptr[14];
    // 0-255
    require_action(*ptr++ == 0x03, exit, err = kMismatchErr);
    require_action(*ptr++ == 255, exit, err = kMismatchErr);
    for (i = 0; i < 255; ++i)
        require_action(*ptr++ == (i & 0xFF), exit, err = kMismatchErr);
    // 255-510
    require_action(*ptr++ == 0x03, exit, err = kMismatchErr);
    require_action(*ptr++ == 255, exit, err = kMismatchErr);
    for (; i < 510; ++i)
        require_action(*ptr++ == (i & 0xFF), exit, err = kMismatchErr);
    // 510-766
    require_action(*ptr++ == 0x03, exit, err = kMismatchErr);
    require_action(*ptr++ == 255, exit, err = kMismatchErr);
    for (; i < 765; ++i)
        require_action(*ptr++ == (i & 0xFF), exit, err = kMismatchErr);
    // 765-1020
    require_action(*ptr++ == 0x03, exit, err = kMismatchErr);
    require_action(*ptr++ == 255, exit, err = kMismatchErr);
    for (; i < 1020; ++i)
        require_action(*ptr++ == (i & 0xFF), exit, err = kMismatchErr);
    // 1020-1093
    require_action(*ptr++ == 0x03, exit, err = kMismatchErr);
    require_action(*ptr++ == 73, exit, err = kMismatchErr);
    for (; i < 1093; ++i)
        require_action(*ptr++ == (i & 0xFF), exit, err = kMismatchErr);
    require_action((size_t)(ptr - TLV8BufferGetPtr(&tlv8)) == 1117, exit, err = kMismatchErr);

    // TLV8BufferDetach

    ptr2 = NULL;
    len = 0;
    err = TLV8BufferDetach(&tlv8, &ptr2, &len);
    require_noerr(err, exit);
    require_action(ptr2, exit, err = kMismatchErr);
    require_action(ptr2 != tlv8.inlineBuffer, exit, err = kMismatchErr);
    require_action(len == 1117, exit, err = kMismatchErr);
    require_action(tlv8.ptr == tlv8.inlineBuffer, exit, err = kMismatchErr);
    require_action(tlv8.len == 0, exit, err = kMismatchErr);
    require_action(tlv8.maxLen == 2000, exit, err = kMismatchErr);
    require_action(!tlv8.mallocedPtr, exit, err = kMismatchErr);
    require_action(memcmp(ptr2, "\x01\x06value1\x02\x04val2", 14) == 0, exit, err = kMismatchErr);
    ptr = &ptr2[14];
    // 0-255
    require_action(*ptr++ == 0x03, exit, err = kMismatchErr);
    require_action(*ptr++ == 255, exit, err = kMismatchErr);
    for (i = 0; i < 255; ++i)
        require_action(*ptr++ == (i & 0xFF), exit, err = kMismatchErr);
    // 255-510
    require_action(*ptr++ == 0x03, exit, err = kMismatchErr);
    require_action(*ptr++ == 255, exit, err = kMismatchErr);
    for (; i < 510; ++i)
        require_action(*ptr++ == (i & 0xFF), exit, err = kMismatchErr);
    // 510-766
    require_action(*ptr++ == 0x03, exit, err = kMismatchErr);
    require_action(*ptr++ == 255, exit, err = kMismatchErr);
    for (; i < 765; ++i)
        require_action(*ptr++ == (i & 0xFF), exit, err = kMismatchErr);
    // 765-1020
    require_action(*ptr++ == 0x03, exit, err = kMismatchErr);
    require_action(*ptr++ == 255, exit, err = kMismatchErr);
    for (; i < 1020; ++i)
        require_action(*ptr++ == (i & 0xFF), exit, err = kMismatchErr);
    // 1020-1093
    require_action(*ptr++ == 0x03, exit, err = kMismatchErr);
    require_action(*ptr++ == 73, exit, err = kMismatchErr);
    for (; i < 1093; ++i)
        require_action(*ptr++ == (i & 0xFF), exit, err = kMismatchErr);
    require_action((size_t)(ptr - ptr2) == 1117, exit, err = kMismatchErr);
    ForgetMem(&ptr2);
    TLV8BufferFree(&tlv8);

    TLV8BufferInit(&tlv8, 256);
    err = TLV8BufferAppend(&tlv8, 0x01, "\xAA", 1);
    require_noerr(err, exit);
    err = TLV8BufferAppend(&tlv8, 0x02, "value2", kSizeCString);
    require_noerr(err, exit);
    err = TLV8BufferDetach(&tlv8, &ptr2, &len);
    require_noerr(err, exit);
    require_action(ptr2, exit, err = kMismatchErr);
    require_action(ptr2 != tlv8.inlineBuffer, exit, err = kMismatchErr);
    require_action(len == 11, exit, err = kMismatchErr);
    require_action(tlv8.ptr == tlv8.inlineBuffer, exit, err = kMismatchErr);
    require_action(tlv8.len == 0, exit, err = kMismatchErr);
    require_action(tlv8.maxLen == 256, exit, err = kMismatchErr);
    require_action(!tlv8.mallocedPtr, exit, err = kMismatchErr);
    require_action(memcmp(ptr2, "\x01\x01\xAA\x02\x06value2", 11) == 0, exit, err = kMismatchErr);
    ForgetMem(&ptr2);
    TLV8BufferFree(&tlv8);

    // Integer tests

    TLV8BufferInit(&tlv8, 256);
    err = TLV8BufferAppendUInt64(&tlv8, 0x01, 0);
    require_noerr(err, exit);
    require_action(tlv8.len == 3, exit, err = kMismatchErr);
    require_action(memcmp(tlv8.ptr, "\x01\x01\x00", tlv8.len) == 0, exit, err = kMismatchErr);
    s64 = TLV8GetSInt64(tlv8.ptr, tlv8.ptr + tlv8.len, 0x01, &err, NULL);
    require_noerr(err, exit);
    require_action(s64 == 0, exit, err = kMismatchErr);

    TLV8BufferInit(&tlv8, 256);
    err = TLV8BufferAppendSInt64(&tlv8, 0x01, -100);
    require_noerr(err, exit);
    require_action(tlv8.len == 3, exit, err = kMismatchErr);
    require_action(memcmp(tlv8.ptr, "\x01\x01\x9C", tlv8.len) == 0, exit, err = kMismatchErr);
    s64 = TLV8GetSInt64(tlv8.ptr, tlv8.ptr + tlv8.len, 0x01, &err, NULL);
    require_noerr(err, exit);
    require_action(s64 == -100, exit, err = kMismatchErr);

    TLV8BufferInit(&tlv8, 256);
    err = TLV8BufferAppendUInt64(&tlv8, 0x01, 250);
    require_noerr(err, exit);
    require_action(tlv8.len == 3, exit, err = kMismatchErr);
    require_action(memcmp(tlv8.ptr, "\x01\x01\xFA", tlv8.len) == 0, exit, err = kMismatchErr);
    s64 = (int64_t)TLV8GetUInt64(tlv8.ptr, tlv8.ptr + tlv8.len, 0x01, &err, NULL);
    require_noerr(err, exit);
    require_action(s64 == 250, exit, err = kMismatchErr);

    TLV8BufferInit(&tlv8, 256);
    err = TLV8BufferAppendSInt64(&tlv8, 0x01, -1000);
    require_noerr(err, exit);
    require_action(tlv8.len == 4, exit, err = kMismatchErr);
    require_action(memcmp(tlv8.ptr, "\x01\x02\x18\xFC", tlv8.len) == 0, exit, err = kMismatchErr);
    s64 = TLV8GetSInt64(tlv8.ptr, tlv8.ptr + tlv8.len, 0x01, &err, NULL);
    require_noerr(err, exit);
    require_action(s64 == -1000, exit, err = kMismatchErr);

    TLV8BufferInit(&tlv8, 256);
    err = TLV8BufferAppendUInt64(&tlv8, 0x01, 60000);
    require_noerr(err, exit);
    require_action(tlv8.len == 4, exit, err = kMismatchErr);
    require_action(memcmp(tlv8.ptr, "\x01\x02\x60\xEA", tlv8.len) == 0, exit, err = kMismatchErr);
    s64 = (int64_t)TLV8GetUInt64(tlv8.ptr, tlv8.ptr + tlv8.len, 0x01, &err, NULL);
    require_noerr(err, exit);
    require_action(s64 == 60000, exit, err = kMismatchErr);

    TLV8BufferInit(&tlv8, 256);
    err = TLV8BufferAppendUInt64(&tlv8, 0x01, 200000);
    require_noerr(err, exit);
    require_action(tlv8.len == 6, exit, err = kMismatchErr);
    require_action(memcmp(tlv8.ptr, "\x01\x04\x40\x0D\x03\x00", tlv8.len) == 0, exit, err = kMismatchErr);
    s64 = (int64_t)TLV8GetUInt64(tlv8.ptr, tlv8.ptr + tlv8.len, 0x01, &err, NULL);
    require_noerr(err, exit);
    require_action(s64 == 200000, exit, err = kMismatchErr);

    // TLV8MaxPayloadBytesForTotalBytes

    require_action(TLV8MaxPayloadBytesForTotalBytes(0) == 0, exit, err = kMismatchErr);
    require_action(TLV8MaxPayloadBytesForTotalBytes(1) == 0, exit, err = kMismatchErr);
    for (i = 2; i <= 257; ++i)
        require_action(TLV8MaxPayloadBytesForTotalBytes(i) == (i - 2), exit, err = kMismatchErr);
    require_action(TLV8MaxPayloadBytesForTotalBytes(258) == 255, exit, err = kMismatchErr);
    require_action(TLV8MaxPayloadBytesForTotalBytes(259) == 255, exit, err = kMismatchErr);
    for (i = 260; i <= 514; ++i)
        require_action(TLV8MaxPayloadBytesForTotalBytes(i) == (i - 4), exit, err = kMismatchErr);
    require_action(TLV8MaxPayloadBytesForTotalBytes(515) == 510, exit, err = kMismatchErr);
    require_action(TLV8MaxPayloadBytesForTotalBytes(516) == 510, exit, err = kMismatchErr);
    require_action(TLV8MaxPayloadBytesForTotalBytes(517) == 511, exit, err = kMismatchErr);
    require_action(TLV8MaxPayloadBytesForTotalBytes(518) == 512, exit, err = kMismatchErr);
    require_action(TLV8MaxPayloadBytesForTotalBytes(519) == 513, exit, err = kMismatchErr);

exit:
    ForgetMem(&storage);
    ForgetMem(&ptr2);
    TLV8BufferFree(&tlv8);
    return err;
}

//===========================================================================================================================
//	TLV16Test
//===========================================================================================================================

static OSStatus TLV16Test(void)
{
    OSStatus err;
    const uint8_t* src = NULL;
    const uint8_t* src2 = NULL;
    const uint8_t* end = NULL;
    uint16_t type = 0;
    const uint8_t* ptr = NULL;
    size_t len = 0;

// TLV16GetNext

#define kTLV16Test1 ""
    src = (const uint8_t*)kTLV16Test1;
    end = src + sizeof_string(kTLV16Test1);
    err = TLV16BEGetNext(src, end, &type, &ptr, &len, &src2);
    require_action(err == kNotFoundErr, exit, err = kMismatchErr);
    err = TLV16LEGetNext(src, end, &type, &ptr, &len, &src2);
    require_action(err == kNotFoundErr, exit, err = kMismatchErr);

#define kTLV16Test2 "\x00"
    src = (const uint8_t*)kTLV16Test2;
    end = src + sizeof_string(kTLV16Test2);
    err = TLV16BEGetNext(src, end, &type, &ptr, &len, &src2);
    require_action(err == kUnderrunErr, exit, err = kMismatchErr);
    err = TLV16LEGetNext(src, end, &type, &ptr, &len, &src2);
    require_action(err == kUnderrunErr, exit, err = kMismatchErr);

#define kTLV16Test3 "\x00\x00"
    src = (const uint8_t*)kTLV16Test3;
    end = src + sizeof_string(kTLV16Test3);
    err = TLV16BEGetNext(src, end, &type, &ptr, &len, &src2);
    require_action(err == kUnderrunErr, exit, err = kMismatchErr);
    err = TLV16LEGetNext(src, end, &type, &ptr, &len, &src2);
    require_action(err == kUnderrunErr, exit, err = kMismatchErr);

#define kTLV16Test4 "\x00\x00\x00"
    src = (const uint8_t*)kTLV16Test4;
    end = src + sizeof_string(kTLV16Test4);
    err = TLV16BEGetNext(src, end, &type, &ptr, &len, &src2);
    require_action(err == kUnderrunErr, exit, err = kMismatchErr);
    err = TLV16LEGetNext(src, end, &type, &ptr, &len, &src2);
    require_action(err == kUnderrunErr, exit, err = kMismatchErr);

#define kTLV16Test5 "\x00\x00\x00\x00"
    src = (const uint8_t*)kTLV16Test5;
    end = src + sizeof_string(kTLV16Test5);
    err = TLV16BEGetNext(src, end, &type, &ptr, &len, &src2);
    require_noerr(err, exit);
    require_action(type == 0, exit, err = kMismatchErr);
    require_action(ptr == (src + 4), exit, err = kMismatchErr);
    require_action(len == 0, exit, err = kMismatchErr);
    err = TLV16LEGetNext(src, end, &type, &ptr, &len, &src2);
    require_noerr(err, exit);
    require_action(type == 0, exit, err = kMismatchErr);
    require_action(ptr == (src + 4), exit, err = kMismatchErr);
    require_action(len == 0, exit, err = kMismatchErr);

#define kTLV16Test6 "\xAA\x11\x00\x04" \
                    "abcd"
    src = (const uint8_t*)kTLV16Test6;
    end = src + sizeof_string(kTLV16Test6);
    err = TLV16BEGetNext(src, end, &type, &ptr, &len, &src2);
    require_noerr(err, exit);
    require_action(type == 0xAA11, exit, err = kMismatchErr);
    require_action(ptr == (src + 4), exit, err = kMismatchErr);
    require_action(len == 4, exit, err = kMismatchErr);
    require_action(memcmp(ptr, "abcd", len) == 0, exit, err = kMismatchErr);

#define kTLV16Test7 "\x11\xAA\x04\x00" \
                    "abcd"
    src = (const uint8_t*)kTLV16Test7;
    end = src + sizeof_string(kTLV16Test7);
    err = TLV16LEGetNext(src, end, &type, &ptr, &len, &src2);
    require_noerr(err, exit);
    require_action(type == 0xAA11, exit, err = kMismatchErr);
    require_action(ptr == (src + 4), exit, err = kMismatchErr);
    require_action(len == 4, exit, err = kMismatchErr);
    require_action(memcmp(ptr, "abcd", len) == 0, exit, err = kMismatchErr);

exit:
    return err;
}

#endif // !EXCLUDE_UNIT_TESTS
