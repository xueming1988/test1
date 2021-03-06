/*
	Copyright (C) 2008-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

// Microsoft deprecated standard C APIs like fopen so disable those warnings because the replacement APIs are not portable.

#if (!defined(_CRT_SECURE_NO_DEPRECATE))
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include "DataBufferUtils.h"

#include "CommonServices.h"
#include "DebugServices.h"
#include "PrintFUtils.h"

#if (TARGET_HAS_STD_C_LIB)
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#define kDataBufferFileBufferSize (32 * 1024)

//===========================================================================================================================
//	DataBuffer_Init
//===========================================================================================================================

void DataBuffer_Init(DataBuffer* inDB, void* inStaticBufferPtr, size_t inStaticBufferLen, size_t inMaxGrowLen)
{
    inDB->staticBufferPtr = (uint8_t*)inStaticBufferPtr;
    inDB->staticBufferLen = inStaticBufferLen;
    inDB->maxGrowLen = inMaxGrowLen;
    inDB->bufferPtr = (uint8_t*)inStaticBufferPtr;
    inDB->bufferLen = 0;
    inDB->bufferMaxLen = inStaticBufferLen;
    inDB->malloced = 0;
    inDB->firstErr = kNoErr;
}

//===========================================================================================================================
//	DataBuffer_Free
//===========================================================================================================================

void DataBuffer_Free(DataBuffer* inDB)
{
    if (inDB->malloced && inDB->bufferPtr) {
        free_compat(inDB->bufferPtr);
    }
    inDB->bufferPtr = inDB->staticBufferPtr;
    inDB->bufferLen = 0;
    inDB->bufferMaxLen = inDB->staticBufferLen;
    inDB->malloced = 0;
    inDB->firstErr = kNoErr;
}

//===========================================================================================================================
//	DataBuffer_Disown
//===========================================================================================================================

uint8_t* DataBuffer_Disown(DataBuffer* inDB)
{
    uint8_t* buf;

    buf = DataBuffer_GetPtr(inDB);
    inDB->bufferPtr = NULL;
    inDB->malloced = 0;
    DataBuffer_Free(inDB);
    return (buf);
}

//===========================================================================================================================
//	DataBuffer_Commit
//===========================================================================================================================

OSStatus DataBuffer_Commit(DataBuffer* inDB, uint8_t** outPtr, size_t* outLen)
{
    OSStatus err;

    err = inDB->firstErr;
    require_noerr_string(err, exit, "earlier error occurred");

    inDB->firstErr = kAlreadyInUseErr; // Mark in-use to prevent further changes to it until DataBuffer_Free.

    if (outPtr)
        *outPtr = DataBuffer_GetPtr(inDB);
    if (outLen)
        *outLen = DataBuffer_GetLen(inDB);

exit:
    return (err);
}

//===========================================================================================================================
//	DataBuffer_Detach
//===========================================================================================================================

OSStatus DataBuffer_Detach(DataBuffer* inDB, uint8_t** outPtr, size_t* outLen)
{
    OSStatus err;
    uint8_t* ptr;
    size_t len;

    len = inDB->bufferLen;
    if (inDB->malloced) {
        check(inDB->bufferPtr || (len == 0));
        ptr = inDB->bufferPtr;
    } else {
        ptr = (uint8_t*)malloc_compat((len > 0) ? len : 1); // malloc( 0 ) is undefined so use at least 1.
        require_action(ptr, exit, err = kNoMemoryErr);

        if (len > 0)
            memcpy(ptr, inDB->bufferPtr, len);
    }
    inDB->bufferPtr = inDB->staticBufferPtr;
    inDB->bufferLen = 0;
    inDB->bufferMaxLen = inDB->staticBufferLen;
    inDB->malloced = 0;
    inDB->firstErr = kNoErr;

    *outPtr = ptr;
    *outLen = len;
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	DataBuffer_DetachCString
//===========================================================================================================================

OSStatus DataBuffer_DetachCString(DataBuffer* inDB, char** outStr)
{
    OSStatus err;
    uint8_t* ptr;
    size_t len;

    err = DataBuffer_Append(inDB, "", 1);
    require_noerr(err, exit);

    ptr = NULL;
    err = DataBuffer_Detach(inDB, &ptr, &len);
    require_noerr(err, exit);

    *outStr = (char*)ptr;

exit:
    return (err);
}

//===========================================================================================================================
//	DataBuffer_Replace
//===========================================================================================================================

OSStatus DataBuffer_Replace(DataBuffer* inDB, size_t inOffset, size_t inOldLen, const void* inNewData, size_t inNewLen)
{
    OSStatus err;
    size_t endOffset;
    size_t oldLen;

    err = inDB->firstErr;
    require_noerr(err, exit2);

    if (inNewLen == kSizeCString)
        inNewLen = strlen((const char*)inNewData);
    if (inNewData)
        check_ptr_overlap(inDB->bufferPtr, inDB->bufferLen, inNewData, inNewLen);

    if (inOffset > inDB->bufferLen) {
        endOffset = inOffset + inNewLen;
        require_action(inOldLen == 0, exit, err = kSizeErr);
        require_action(endOffset >= inOffset, exit, err = kSizeErr);

        err = DataBuffer_Resize(inDB, endOffset, NULL);
        require_noerr(err, exit2);
    } else {
        endOffset = inOffset + inOldLen;
        require_action(endOffset >= inOffset, exit, err = kSizeErr);
        require_action(endOffset <= inDB->bufferLen, exit, err = kRangeErr);

        // Shift any data after the data being replaced to make room (growing) or take up slack (shrinking).

        oldLen = inDB->bufferLen;
        if (inNewLen > inOldLen) {
            err = DataBuffer_Grow(inDB, inNewLen - inOldLen, NULL);
            require_noerr(err, exit2);
        }
        if (oldLen != endOffset) {
            memmove(inDB->bufferPtr + inOffset + inNewLen, inDB->bufferPtr + endOffset, oldLen - endOffset);
        }
        if (inNewLen < inOldLen) {
            inDB->bufferLen -= inOldLen - inNewLen;
        }
    }

    // Copy in any new data.

    if (inNewData) {
        memmove(inDB->bufferPtr + inOffset, inNewData, inNewLen);
    }
    goto exit2;

exit:
    inDB->firstErr = err;

exit2:
    return (err);
}

//===========================================================================================================================
//	DataBuffer_Resize
//===========================================================================================================================

OSStatus DataBuffer_Resize(DataBuffer* inDB, size_t inNewLen, void* outPtr)
{
    OSStatus err;
    size_t oldLen;
    size_t newMaxLen;
    uint8_t* newPtr;

    err = inDB->firstErr;
    require_noerr(err, exit2);

    // Grow or shink as needed. If growing larger than the max size of the current buffer, reallocate.

    oldLen = inDB->bufferLen;
    if (inNewLen > oldLen) {
        if (inNewLen > inDB->bufferMaxLen) {
            require_action(inNewLen <= inDB->maxGrowLen, exit, err = kOverrunErr);

            if (inNewLen < 256)
                newMaxLen = 256;
            else if (inNewLen < 4096)
                newMaxLen = 4096;
            else if (inNewLen < 131072)
                newMaxLen = AlignUp(inNewLen, 16384U);
            else
                newMaxLen = AlignUp(inNewLen, 131072U);

            newPtr = (uint8_t*)malloc_compat(newMaxLen);
            require_action(newPtr, exit, err = kNoMemoryErr);

            if (inDB->bufferLen > 0) {
                memcpy(newPtr, inDB->bufferPtr, inDB->bufferLen);
            }
            if (inDB->malloced && inDB->bufferPtr) {
                free_compat(inDB->bufferPtr);
            }
            inDB->bufferMaxLen = newMaxLen;
            inDB->bufferPtr = newPtr;
            inDB->malloced = 1;
        }
        inDB->bufferLen = inNewLen;
        if (outPtr)
            *((void**)outPtr) = inDB->bufferPtr + oldLen;
    } else {
        // $$$ TO DO: Consider shrinking the buffer if it's shrinking by a large amount.

        inDB->bufferLen = inNewLen;
        if (outPtr)
            *((void**)outPtr) = inDB->bufferPtr;
    }
    goto exit2;

exit:
    inDB->firstErr = err;

exit2:
    return (err);
}

//===========================================================================================================================
//	DataBuffer_Shrink
//===========================================================================================================================

OSStatus DataBuffer_Shrink(DataBuffer* inDB, size_t inAmount)
{
    OSStatus err;

    err = inDB->firstErr;
    require_noerr(err, exit2);

    require_action(inAmount <= inDB->bufferLen, exit, err = kSizeErr);
    inDB->bufferLen -= inAmount;
    goto exit2;

exit:
    inDB->firstErr = err;

exit2:
    return (err);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	DataBuffer_Append
//===========================================================================================================================

OSStatus DataBuffer_Append(DataBuffer* inDB, const void* inData, size_t inLen)
{
    OSStatus err;
    uint8_t* ptr;

    if (inLen == kSizeCString)
        inLen = strlen((const char*)inData);
    err = DataBuffer_Grow(inDB, inLen, &ptr);
    require_noerr(err, exit);

    memcpy(ptr, inData, inLen);

exit:
    return (err);
}

//===========================================================================================================================
//	DataBuffer_AppendF
//===========================================================================================================================

static int __DataBuffer_PrintFCallBack(const char* inStr, size_t inLen, void* inContext);

OSStatus DataBuffer_AppendF(DataBuffer* inDB, const char* inFormat, ...)
{
    OSStatus err;
    va_list args;
    int n;

    va_start(args, inFormat);
    n = VCPrintF(__DataBuffer_PrintFCallBack, inDB, inFormat, args);
    va_end(args);
    require_action(n >= 0, exit, err = n);
    err = kNoErr;
exit:
    return (err);
}

static int __DataBuffer_PrintFCallBack(const char* inStr, size_t inLen, void* inContext)
{
    int result;

    result = (int)DataBuffer_Append((DataBuffer*)inContext, inStr, inLen);
    require_noerr(result, exit);
    result = (int)inLen;

exit:
    return (result);
}

//===========================================================================================================================
//	DataBuffer_AppendANSIFile
//===========================================================================================================================

#if (TARGET_HAS_C_LIB_IO)
OSStatus DataBuffer_AppendANSIFile(DataBuffer* inBuffer, FILE* inFile)
{
    OSStatus err;
    uint8_t* buf;
    size_t n;

    buf = (uint8_t*)malloc_compat(kDataBufferFileBufferSize);
    require_action(buf, exit, err = kNoMemoryErr);

    for (;;) {
        n = fread(buf, 1, kDataBufferFileBufferSize, inFile);
        if (n == 0)
            break;

        err = DataBuffer_Append(inBuffer, buf, n);
        require_noerr(err, exit);
    }
    err = kNoErr;

exit:
    if (buf)
        free_compat(buf);
    return (err);
}
#endif

//===========================================================================================================================
//	DataBufferAppendFile
//===========================================================================================================================

#if (TARGET_HAS_C_LIB_IO)
OSStatus DataBuffer_AppendFile(DataBuffer* inBuffer, const char* inPath)
{
    OSStatus err;
    FILE* f;

    f = fopen(inPath, "rb");
    err = map_global_value_errno(f, f);
    require_noerr_quiet(err, exit);

    err = DataBuffer_AppendANSIFile(inBuffer, f);
    fclose(f);
    require_noerr(err, exit);

exit:
    return (err);
}
#endif

//===========================================================================================================================
//	DataBuffer_RunProcessAndAppendOutput
//===========================================================================================================================

#if (TARGET_OS_POSIX)
OSStatus DataBuffer_RunProcessAndAppendOutput(DataBuffer* inBuffer, const char* inCmdLine)
{
    OSStatus err;
    FILE* f;

    f = popen(inCmdLine, "r");
    err = map_global_value_errno(f, f);
    require_noerr_quiet(err, exit);

    err = DataBuffer_AppendANSIFile(inBuffer, f);
    if (f)
        pclose(f);
    require_noerr(err, exit);

exit:
    return (err);
}
#endif

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if (!EXCLUDE_UNIT_TESTS)
//===========================================================================================================================
//	DataBufferUtils_Test
//===========================================================================================================================

OSStatus DataBufferUtils_Test(void)
{
    OSStatus err;
    DataBuffer db;
    uint8_t dbStaticBuf[256];
    uint8_t* ptr;

    // API Test

    DataBuffer_Init(&db, dbStaticBuf, sizeof(dbStaticBuf), SIZE_MAX);

    err = DataBuffer_Append(&db, "test", kSizeCString);
    require_noerr(err, exit);
    require_action(DataBuffer_GetLen(&db) == 4, exit, err = kResponseErr);
    require_action(memcmp(DataBuffer_GetPtr(&db), "test", DataBuffer_GetLen(&db)) == 0, exit, err = kResponseErr);

    err = DataBuffer_Shrink(&db, 1);
    require_noerr(err, exit);
    require_action(DataBuffer_GetLen(&db) == 3, exit, err = kResponseErr);
    require_action(memcmp(DataBuffer_GetPtr(&db), "tes", DataBuffer_GetLen(&db)) == 0, exit, err = kResponseErr);

    err = DataBuffer_Resize(&db, 4, NULL);
    require_noerr(err, exit);
    require_action(DataBuffer_GetLen(&db) == 4, exit, err = kResponseErr);
    require_action(memcmp(DataBuffer_GetPtr(&db), "tes", 3) == 0, exit, err = kResponseErr);
    memcpy(DataBuffer_GetPtr(&db), "TEST", 4);

    err = DataBuffer_Grow(&db, 4, &ptr);
    require_noerr(err, exit);
    require_action(DataBuffer_GetLen(&db) == 8, exit, err = kResponseErr);
    memcpy(ptr, "cool", 4);
    require_action(memcmp(DataBuffer_GetPtr(&db), "TESTcool", 8) == 0, exit, err = kResponseErr);

    memset(DataBuffer_GetPtr(&db), 'z', DataBuffer_GetLen(&db));
    err = DataBuffer_Insert(&db, 4, "test", kSizeCString);
    require_noerr(err, exit);
    require_action(DataBuffer_GetLen(&db) == 12, exit, err = kResponseErr);
    require_action(memcmp(DataBuffer_GetPtr(&db), "zzzztestzzzz", 12) == 0, exit, err = kResponseErr);

    err = DataBuffer_Remove(&db, 2, 4);
    require_noerr(err, exit);
    require_action(DataBuffer_GetLen(&db) == 8, exit, err = kResponseErr);
    require_action(memcmp(DataBuffer_GetPtr(&db), "zzstzzzz", 8) == 0, exit, err = kResponseErr);

    err = DataBuffer_Insert(&db, 8, "test", kSizeCString);
    require_noerr(err, exit);
    require_action(DataBuffer_GetLen(&db) == 12, exit, err = kResponseErr);
    require_action(memcmp(DataBuffer_GetPtr(&db), "zzstzzzztest", 12) == 0, exit, err = kResponseErr);

    err = DataBuffer_Replace(&db, 2, 2, "kewl", kSizeCString);
    require_noerr(err, exit);
    require_action(DataBuffer_GetLen(&db) == 14, exit, err = kResponseErr);
    require_action(memcmp(DataBuffer_GetPtr(&db), "zzkewlzzzztest", 14) == 0, exit, err = kResponseErr);

    err = DataBuffer_Commit(&db, NULL, NULL);
    require_noerr(err, exit);

    DataBuffer_Free(&db);
    require_action(DataBuffer_GetLen(&db) == 0, exit, err = kResponseErr);

    // Formatted Test

    DataBuffer_Init(&db, dbStaticBuf, sizeof(dbStaticBuf), 1024);

    err = DataBuffer_AppendF(&db, "test");
    require_noerr(err, exit);
    require_action(DataBuffer_GetLen(&db) == 4, exit, err = kResponseErr);
    require_action(memcmp(DataBuffer_GetPtr(&db), "test", 4) == 0, exit, err = kResponseErr);

    err = DataBuffer_AppendF(&db, "ing");
    require_noerr(err, exit);
    require_action(DataBuffer_GetLen(&db) == 7, exit, err = kResponseErr);
    require_action(memcmp(DataBuffer_GetPtr(&db), "testing", 7) == 0, exit, err = kResponseErr);

    err = DataBuffer_AppendF(&db, " %d", 123);
    require_noerr(err, exit);
    require_action(DataBuffer_GetLen(&db) == 11, exit, err = kResponseErr);
    require_action(memcmp(DataBuffer_GetPtr(&db), "testing 123", 11) == 0, exit, err = kResponseErr);

    err = DataBuffer_Commit(&db, NULL, NULL);
    require_noerr(err, exit);

    DataBuffer_Free(&db);

    // Replace Test

    DataBuffer_Init(&db, dbStaticBuf, sizeof(dbStaticBuf), 1024);

    err = DataBuffer_Append(&db, "test", 4);
    require_noerr(err, exit);
    require_action(DataBuffer_GetLen(&db) == 4, exit, err = kResponseErr);
    require_action(memcmp(DataBuffer_GetPtr(&db), "test", 4) == 0, exit, err = kResponseErr);

    err = DataBuffer_Replace(&db, 0, 3, "ba", 2);
    require_noerr(err, exit);
    require_action(DataBuffer_GetLen(&db) == 3, exit, err = kResponseErr);
    require_action(memcmp(DataBuffer_GetPtr(&db), "bat", 3) == 0, exit, err = kResponseErr);

    err = DataBuffer_Replace(&db, DataBuffer_GetLen(&db), 0, "ing", 3);
    require_noerr(err, exit);
    require_action(DataBuffer_GetLen(&db) == 6, exit, err = kResponseErr);
    require_action(memcmp(DataBuffer_GetPtr(&db), "bating", 6) == 0, exit, err = kResponseErr);

    err = DataBuffer_Replace(&db, 3, 0, "-xyz-", 5);
    require_noerr(err, exit);
    require_action(DataBuffer_GetLen(&db) == 11, exit, err = kResponseErr);
    require_action(memcmp(DataBuffer_GetPtr(&db), "bat-xyz-ing", 11) == 0, exit, err = kResponseErr);

    err = DataBuffer_Replace(&db, 3, 5, NULL, 0);
    require_noerr(err, exit);
    require_action(DataBuffer_GetLen(&db) == 6, exit, err = kResponseErr);
    require_action(memcmp(DataBuffer_GetPtr(&db), "bating", 6) == 0, exit, err = kResponseErr);

    err = DataBuffer_Replace(&db, 3, 3, NULL, 0);
    require_noerr(err, exit);
    require_action(DataBuffer_GetLen(&db) == 3, exit, err = kResponseErr);
    require_action(memcmp(DataBuffer_GetPtr(&db), "bat", 3) == 0, exit, err = kResponseErr);

    err = DataBuffer_Commit(&db, NULL, NULL);
    require_noerr(err, exit);

    DataBuffer_Free(&db);

    // Dynamic Memory Test

    DataBuffer_Init(&db, dbStaticBuf, 10, SIZE_MAX);

    err = DataBuffer_Append(&db, "test", 4);
    require_noerr(err, exit);
    require_action(DataBuffer_GetLen(&db) == 4, exit, err = kResponseErr);
    require_action(memcmp(DataBuffer_GetPtr(&db), "test", 4) == 0, exit, err = kResponseErr);

    err = DataBuffer_Append(&db, "another", 7);
    require_noerr(err, exit);
    require_action(DataBuffer_GetLen(&db) == 11, exit, err = kResponseErr);
    require_action(memcmp(DataBuffer_GetPtr(&db), "testanother", 11) == 0, exit, err = kResponseErr);

    err = DataBuffer_Grow(&db, 300, NULL);
    require_noerr(err, exit);
    require_action(DataBuffer_GetLen(&db) == 311, exit, err = kResponseErr);
    require_action(memcmp(DataBuffer_GetPtr(&db), "testanother", 11) == 0, exit, err = kResponseErr);

    err = DataBuffer_Commit(&db, NULL, NULL);
    require_noerr(err, exit);

    DataBuffer_Free(&db);

exit:
    printf("DataBufferUtils_Test: %s\n", !err ? "PASSED" : "FAILED");
    return (err);
}
#endif // !EXCLUDE_UNIT_TESTS
