/*
	Copyright (C) 2016-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "OPACKUtils.h"
#include "CFUtils.h"
#include "DebugServices.h"

//===========================================================================================================================

// Ranges

#define kOPACKMaxDepth 32 // Max object hierarchy depth when decoding to avoid infinite recursion.
#define kOPACKSmallArrayMax (kOPACKTagSmallArrayMax - kOPACKTagSmallArrayMin)
#define kOPACKSmallDataMax (kOPACKTagSmallDataMax - kOPACKTagSmallDataMin)
#define kOPACKSmallDictionaryMax (kOPACKTagSmallDictionaryMax - kOPACKTagSmallDictionaryMin)
#define kOPACKSmallIntegerMin -1
#define kOPACKSmallIntegerMax (kOPACKTagSmallIntMax - kOPACKTagSmallInt0)
#define kOPACKSmallStringMax (kOPACKTagSmallStringMax - kOPACKTagSmallStringMin)
#define kOPACKSmallUIDMin 0
#define kOPACKSmallUIDMax (kOPACKTagSmallUIDMax - kOPACKTagSmallUIDMin)

// Tags

#define kOPACKTagInvalid 0x00 // Reserved to avoid common errors with zero'd memory.

#define kOPACKTagTrue 0x01 // Boolean true.
#define kOPACKTagFalse 0x02 // Boolean false.
#define kOPACKTagTerminator 0x03 // Terminator for variable-item objects.
#define kOPACKTagNull 0x04 // Null object.
#define kOPACKTagUUID 0x05 // <16:big endian UUID>.
#define kOPACKTagDate 0x06 // <8:little endian Float64 seconds since 2001-01-01 00:00:00>.

#define kOPACKTagSmallIntMin 0x07 // -1
#define kOPACKTagSmallInt0 0x08 // 0
#define kOPACKTagSmallIntMax 0x2F // 39
#define kOPACKTagInt8Bit 0x30 // <1:value>.
#define kOPACKTagInt16Bit 0x31 // <2:little endian value>.
#define kOPACKTagInt32Bit 0x32 // <4:little endian value>.
#define kOPACKTagInt64Bit 0x33 // <8:little endian value>.
#define kOPACKTagInt128Bit 0x34
#define kOPACKTagFloat32 0x35 // <4:little endian Float32>.
#define kOPACKTagFloat64 0x36 // <8:little endian Float64>.

#define kOPACKTagSmallStringMin 0x40 // <0:UTF-8 bytes>.
#define kOPACKTagSmallStringMax 0x60 // <32:UTF-8 bytes>.
#define kOPACKTagString8Bit 0x61 // <1:length> <n:UTF-8 bytes>.
#define kOPACKTagString16Bit 0x62 // <2:little endian length> <n:UTF-8 bytes>.
#define kOPACKTagString32Bit 0x63 // <4:little endian length> <n:UTF-8 bytes>.
#define kOPACKTagString64Bit 0x64 // <8:little endian length> <n:UTF-8 bytes>.
#define kOPACKTagStringNullTerm 0x6F // <n:UTF-8 bytes> <1:NUL>.

#define kOPACKTagSmallDataMin 0x70 // <n:bytes>.
#define kOPACKTagSmallDataMax 0x90 // <n:bytes>.
#define kOPACKTagData8Bit 0x91 // <1:length> <n:bytes>.
#define kOPACKTagData16Bit 0x92 // <2:little endian length> <n:bytes>.
#define kOPACKTagData32Bit 0x93 // <4:little endian length> <n:bytes>.
#define kOPACKTagData64Bit 0x94 // <8:little endian length> <n:bytes>.
#define kOPACKTagDataChunks 0x9F // [data object 1] [data object 2] ... [data object n] <1:terminator tag>.

#define kOPACKTagSmallUIDMin 0xA0 // 0
#define kOPACKTagSmallUIDMax 0xC0 // 32
#define kOPACKTagUID8Bit 0xC1 // <1:UID integer>.
#define kOPACKTagUID16Bit 0xC2 // <2:little endian UID integer>.
#define kOPACKTagUID24Bit 0xC3 // <3:little endian UID integer>.
#define kOPACKTagUID32Bit 0xC4 // <4:little endian UID integer>.

#define kOPACKTagSmallArrayMin 0xD0 // 0-item array.
// 0xD1-0xDC for 1-13 item arrays.
#define kOPACKTagSmallArrayMax 0xDE // <object 1> <object 2> ... <object 14>.
#define kOPACKTagArray 0xDF // [object 1] [object 2] ... [object n] <1:terminator tag>.

#define kOPACKTagSmallDictionaryMin 0xE0 // 0-entry dictionary.
// 0xD1-0xDC for 1-13 entry dictionaries.
#define kOPACKTagSmallDictionaryMax 0xEE // <key 1><value 1> <key 2><value 2> ... <key 14> <value 14>.
#define kOPACKTagDictionary 0xEF // [key 1][value 1] [key 2][value 2] ... [key n][value n] <1:terminator tag>.

//===========================================================================================================================

check_compile_time(sizeof(CFIndex) == sizeof(ssize_t));

typedef struct
{
    OPACKWrite_f callback;
    void* context;
    CFMutableDictionaryRef uniqueDict;
    uintptr_t uniqueCount;

} OPACKEncodeContext;

typedef struct
{
    OPACKEncodeContext* writeCtx;
    CFDictionaryRef dict;
    OSStatus err;

} OPACKDictionaryApplierContext;

typedef struct
{
    CFDataRef data;
    OPACKFlags flags;
    const uint8_t* base;
    CFMutableDictionaryRef uniqueDict;
    uintptr_t uniqueCount;
    int32_t depth;

} OPACKDecodeContext;

//===========================================================================================================================

#define _OPACKEnsureInitialized() dispatch_once_f(&gOPACKInitalizeOnce, NULL, _OPACKInitializeOnce)
static void _OPACKInitializeOnce(void* inArg);
static Boolean _OPACKObjectsExactlyEqual(const void* a, const void* b);

// Encoder

static OSStatus _OPACKEncoderAppendCallback(const uint8_t* inData, size_t inLen, OPACKFlags inFlags, void* inContext);
static OSStatus _OPACKEncodeObject(OPACKEncodeContext* ctx, CFTypeRef inObj);
static OSStatus _OPACKEncodeArray(OPACKEncodeContext* ctx, CFArrayRef inArray);
static OSStatus _OPACKEncodeData(OPACKEncodeContext* ctx, CFDataRef inData);
static OSStatus _OPACKEncodeDate(OPACKEncodeContext* ctx, CFDateRef inDate);
static OSStatus _OPACKEncodeDictionary(OPACKEncodeContext* ctx, CFDictionaryRef inDictionary);
static void _OPACKEncodeDictionaryApplier(const void* inKey, const void* inValue, void* inContext);
static OSStatus _OPACKEncodeNumber(OPACKEncodeContext* ctx, CFNumberRef inNumber);
static OSStatus _OPACKEncodeString(OPACKEncodeContext* ctx, CFStringRef inString);
static OSStatus _OPACKEncodeUID(OPACKEncodeContext* ctx, CFTypeRef inObj, Boolean* outSkip);
static OSStatus _OPACKEncodeUUID(OPACKEncodeContext* ctx, CFTypeRef inUUID, bool inNS);
#define _OPACKEncodeBytes(CTX, PTR, LEN) ctx->callback((const uint8_t*)(PTR), (LEN), kOPACKFlag_None, (CTX)->context)

// Decoder

CF_RETURNS_RETAINED
static CFTypeRef _OPACKDecodeBytes(CFDataRef inData, const void* inPtr, size_t inLen, OPACKFlags inFlags, OSStatus* outErr);
CF_RETURNS_RETAINED
static CFTypeRef
_OPACKDecodeObject(
    OPACKDecodeContext* ctx,
    const uint8_t* inSrc,
    const uint8_t* const inEnd,
    const uint8_t** outSrc,
    OSStatus* outErr);
CF_RETURNS_RETAINED
static CFTypeRef
_OPACKDecodeArray(
    OPACKDecodeContext* ctx,
    const uint8_t* inSrc,
    const uint8_t* const inEnd,
    uint8_t inTag,
    const uint8_t** outSrc,
    OSStatus* outErr);
CF_RETURNS_RETAINED
static CFTypeRef
_OPACKDecodeData(
    OPACKDecodeContext* ctx,
    const uint8_t* inSrc,
    const uint8_t* const inEnd,
    uint8_t inTag,
    const uint8_t** outSrc,
    OSStatus* outErr);
CF_RETURNS_RETAINED
static CFTypeRef
_OPACKDecodeDataChunks(
    OPACKDecodeContext* ctx,
    const uint8_t* inSrc,
    const uint8_t* const inEnd,
    const uint8_t** outSrc,
    OSStatus* outErr);
CF_RETURNS_RETAINED
static CFTypeRef
_OPACKDecodeDictionary(
    OPACKDecodeContext* ctx,
    const uint8_t* inSrc,
    const uint8_t* const inEnd,
    uint8_t inTag,
    const uint8_t** outSrc,
    OSStatus* outErr);
CF_RETURNS_RETAINED
static CFTypeRef
_OPACKDecodeNumber(
    OPACKDecodeContext* ctx,
    const uint8_t* inSrc,
    const uint8_t* const inEnd,
    uint8_t inTag,
    const uint8_t** outSrc,
    OSStatus* outErr);
CF_RETURNS_RETAINED
static CFTypeRef
_OPACKDecodeSmallString(
    OPACKDecodeContext* ctx,
    const uint8_t* inSrc,
    const uint8_t* const inEnd,
    size_t inLen,
    const uint8_t** outSrc,
    OSStatus* outErr);
CF_RETURNS_RETAINED
static CFTypeRef
_OPACKDecodeString(
    OPACKDecodeContext* ctx,
    const uint8_t* inSrc,
    const uint8_t* const inEnd,
    uint8_t inTag,
    const uint8_t** outSrc,
    OSStatus* outErr);
CF_RETURNS_RETAINED
static CFTypeRef
_OPACKDecodeUID(
    OPACKDecodeContext* ctx,
    const uint8_t* inSrc,
    const uint8_t* const inEnd,
    uint8_t inTag,
    const uint8_t** outSrc,
    OSStatus* outErr);

//===========================================================================================================================

static dispatch_once_t gOPACKInitalizeOnce = 0;

static CFTypeID gCFArrayType = 0;
static CFTypeID gCFBooleanType = 0;
static CFTypeID gCFDataType = 0;
static CFTypeID gCFDateType = 0;
static CFTypeID gCFDictionaryType = 0;
static CFTypeID gCFNumberType = 0;
static CFTypeID gCFStringType = 0;
static CFTypeID gCFUUIDType = 0;

static CFNumberRef gCFNumbers[(kOPACKSmallIntegerMax - kOPACKSmallIntegerMin) + 1];

//===========================================================================================================================

static void _OPACKInitializeOnce(void* inArg)
{
    size_t i;

    (void)inArg;

    gCFArrayType = CFArrayGetTypeID();
    gCFBooleanType = CFBooleanGetTypeID();
    gCFDataType = CFDataGetTypeID();
    gCFDateType = CFDateGetTypeID();
    gCFDictionaryType = CFDictionaryGetTypeID();
    gCFNumberType = CFNumberGetTypeID();
    gCFStringType = CFStringGetTypeID();
    gCFUUIDType = CFUUIDGetTypeID();

    // Pre-allocate small numbers to avoid needing to create them on every decode.

    for (i = 0; i < countof(gCFNumbers); ++i) {
        int8_t x;

        x = (int8_t)(kOPACKSmallIntegerMin + ((int8_t)i));
        gCFNumbers[i] = CFNumberCreate(NULL, kCFNumberSInt8Type, &x);
    }
}

//===========================================================================================================================
//	This is needed because we need exact matches to avoid lossy roundtrip conversions:
//
//	1. CFEqual will treat kCFBooleanFalse == CFNumber( 0 ) and kCFBooleanTrue == CFNumber( 1 ).
//	2. CFEqual will treat CFNumber( 1.0 ) == CFNumber( 1 ).

static Boolean _OPACKObjectsExactlyEqual(const void* a, const void* b)
{
    CFTypeID const aType = CFGetTypeID(a);
    CFTypeID const bType = CFGetTypeID(b);

    if ((aType == bType) && CFEqual(a, b)) {
        if (aType != gCFNumberType) {
            return (true);
        }
        if (CFNumberIsFloatType((CFNumberRef)a) == CFNumberIsFloatType((CFNumberRef)b)) {
            return (true);
        }
    }
    return (false);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================

CFDataRef OPACKEncoderCreateData(CFTypeRef inObj, OPACKFlags inFlags, OSStatus* outErr)
{
    return (OPACKEncoderCreateDataMutable(inObj, inFlags, outErr));
}

//===========================================================================================================================

CFMutableDataRef OPACKEncoderCreateDataMutable(CFTypeRef inObj, OPACKFlags inFlags, OSStatus* outErr)
{
    CFMutableDataRef result = NULL;
    CFMutableDataRef output;
    OSStatus err;

    output = CFDataCreateMutable(NULL, 0);
    require_action(output, exit, err = kNoMemoryErr);

    err = OPACKEncodeObject(inObj, inFlags, _OPACKEncoderAppendCallback, output);
    require_noerr_quiet(err, exit);

    result = output;
    output = NULL;

exit:
    CFReleaseNullSafe(output);
    if (outErr)
        *outErr = err;
    return (result);
}

//===========================================================================================================================

static OSStatus _OPACKEncoderAppendCallback(const uint8_t* inData, size_t inLen, OPACKFlags inFlags, void* inContext)
{
    CFMutableDataRef const output = (CFMutableDataRef)inContext;

    (void)inFlags;

    CFDataAppendBytes(output, inData, (CFIndex)inLen);
    return (kNoErr);
}

//===========================================================================================================================

OSStatus OPACKEncodeObject(CFTypeRef inObj, OPACKFlags inFlags, OPACKWrite_f inCallback, void* inContext)
{
    OSStatus err;
    OPACKEncodeContext ctx = { inCallback, inContext, NULL, 0 };

    _OPACKEnsureInitialized();
    if (!(inFlags & kOPACKFlag_Dups)) {
        CFDictionaryKeyCallBacks dictCallbacks;

        dictCallbacks = kCFTypeDictionaryKeyCallBacks;
        dictCallbacks.equal = _OPACKObjectsExactlyEqual;
        ctx.uniqueDict = CFDictionaryCreateMutable(NULL, 0, &dictCallbacks, NULL);
        require_action(ctx.uniqueDict, exit, err = kNoMemoryErr);
    }
    err = _OPACKEncodeObject(&ctx, inObj);

exit:
    CFReleaseNullSafe(ctx.uniqueDict);
    return (err);
}

//===========================================================================================================================

static OSStatus _OPACKEncodeObject(OPACKEncodeContext* ctx, CFTypeRef inObj)
{
    CFTypeID const type = CFGetTypeID(inObj);
    OSStatus err;

    // Note: these are ordered based on how frequently they occur in real world use.

    if (0) {
    }

    // String

    else if (type == gCFStringType) {
        err = _OPACKEncodeString(ctx, (CFStringRef)inObj);
        require_noerr_quiet(err, exit);
    }

    // Number

    else if (type == gCFNumberType) {
        err = _OPACKEncodeNumber(ctx, (CFNumberRef)inObj);
        require_noerr_quiet(err, exit);
    }

    // Boolean

    else if (type == gCFBooleanType) {
        uint8_t u8;

        u8 = (inObj == kCFBooleanTrue) ? kOPACKTagTrue : kOPACKTagFalse;
        err = _OPACKEncodeBytes(ctx, &u8, 1);
        require_noerr_quiet(err, exit);
    }

    // Dictionary

    else if (type == gCFDictionaryType) {
        err = _OPACKEncodeDictionary(ctx, (CFDictionaryRef)inObj);
        require_noerr_quiet(err, exit);
    }

    // Array

    else if (type == gCFArrayType) {
        err = _OPACKEncodeArray(ctx, (CFArrayRef)inObj);
        require_noerr_quiet(err, exit);
    }

    // Data

    else if (type == gCFDataType) {
        err = _OPACKEncodeData(ctx, (CFDataRef)inObj);
        require_noerr_quiet(err, exit);
    }

    // UUID
    // Note: NSUUID may return the same CFTypeID as CFUUID, but they're not toll-free bridged so check for NSUUID first.

    else if (type == gCFUUIDType) {
        err = _OPACKEncodeUUID(ctx, inObj, false);
        require_noerr_quiet(err, exit);
    }

    // Null

    else if (inObj == ((CFTypeRef)kCFNull)) {
        uint8_t u8;

        u8 = kOPACKTagNull;
        err = _OPACKEncodeBytes(ctx, &u8, 1);
        require_noerr_quiet(err, exit);
    }

    // Date

    else if (type == gCFDateType) {
        err = _OPACKEncodeDate(ctx, (CFDateRef)inObj);
        require_noerr_quiet(err, exit);
    }

    // Unknown

    else {
        err = kUnsupportedErr;
        goto exit;
    }

exit:
    return (err);
}

//===========================================================================================================================

static OSStatus _OPACKEncodeArray(OPACKEncodeContext* ctx, CFArrayRef inArray)
{
    OSStatus err;
    CFIndex i, n;
    uint8_t u8;

    // If the count is small enough, embed the count in the tag. Otherwise, use a single terminator object at the end.
    // This makes it so small arrays have 1 byte of overhead and large arrays have 2 bytes of overhead.

    n = CFArrayGetCount(inArray);
    u8 = (n <= kOPACKSmallArrayMax) ? ((uint8_t)(kOPACKTagSmallArrayMin + n)) : kOPACKTagArray;
    err = _OPACKEncodeBytes(ctx, &u8, 1);
    require_noerr_quiet(err, exit);
    for (i = 0; i < n; ++i) {
        err = _OPACKEncodeObject(ctx, CFArrayGetValueAtIndex(inArray, i));
        require_noerr_quiet(err, exit);
    }
    if (u8 == kOPACKTagArray) {
        u8 = kOPACKTagTerminator;
        err = _OPACKEncodeBytes(ctx, &u8, 1);
        require_noerr_quiet(err, exit);
    }

exit:
    return (err);
}

//===========================================================================================================================

static OSStatus _OPACKEncodeData(OPACKEncodeContext* ctx, CFDataRef inData)
{
    OSStatus err;
    uint8_t buf[9];
    size_t n, len;

    // If data is at least 1 byte and de-duping is enabled then encode the UID of a previous dup.
    // The > 1 byte check avoids consuming a UID slot when using a UID wouldn't save any space.

    n = (size_t)CFDataGetLength(inData);
    if ((n > 1) && ctx->uniqueDict) {
        Boolean skip = false;

        err = _OPACKEncodeUID(ctx, inData, &skip);
        require_noerr(err, exit);
        require_quiet(!skip, exit);
    }

    // Encode the length of the data. Embed it in the tag for small lengths. Otherwise, use the minimum number of bytes.

    if (n <= kOPACKSmallDataMax) {
        buf[0] = (uint8_t)(kOPACKTagSmallDataMin + n);
        len = 1;
    } else if (n <= 0xFF) {
        buf[0] = kOPACKTagData8Bit;
        buf[1] = (uint8_t)n;
        len = 2;
    } else if (n <= 0xFFFF) {
        buf[0] = kOPACKTagData16Bit;
        WriteLittle16(&buf[1], n);
        len = 3;
    } else if (n <= UINT32_C(0xFFFFFFFF)) {
        buf[0] = kOPACKTagData32Bit;
        WriteLittle32(&buf[1], n);
        len = 5;
    } else {
        buf[0] = kOPACKTagData64Bit;
        WriteLittle64(&buf[1], (uint64_t)n);
        len = 9;
    }
    err = _OPACKEncodeBytes(ctx, buf, len);
    require_noerr_quiet(err, exit);

    // Encode the data.

    if (n > 0) {
        err = _OPACKEncodeBytes(ctx, CFDataGetBytePtr(inData), (size_t)n);
        require_noerr_quiet(err, exit);
    }

exit:
    return (err);
}

//===========================================================================================================================

static OSStatus _OPACKEncodeDate(OPACKEncodeContext* ctx, CFDateRef inDate)
{
    OSStatus err;
    double d;
    uint8_t buf[9];

    if (ctx->uniqueDict) {
        Boolean skip = false;

        err = _OPACKEncodeUID(ctx, inDate, &skip);
        require_noerr(err, exit);
        require_quiet(!skip, exit);
    }

    buf[0] = kOPACKTagDate;
    d = CFDateGetAbsoluteTime(inDate);
    WriteLittleFloat64(&buf[1], d);
    err = _OPACKEncodeBytes(ctx, buf, 9);
    require_noerr_quiet(err, exit);

exit:
    return (err);
}

//===========================================================================================================================

static OSStatus _OPACKEncodeDictionary(OPACKEncodeContext* ctx, CFDictionaryRef inDictionary)
{
    OSStatus err;
    CFIndex n;
    uint8_t u8;

    // If the count is small enough, embed the count in the tag. Otherwise, use a single terminator object at the end.
    // This makes it so small dictionaries have 1 byte of overhead and large dictionaries have 2 bytes of overhead.

    n = CFDictionaryGetCount(inDictionary);
    u8 = (n <= kOPACKSmallDictionaryMax) ? ((uint8_t)(kOPACKTagSmallDictionaryMin + n)) : kOPACKTagDictionary;
    err = _OPACKEncodeBytes(ctx, &u8, 1);
    require_noerr_quiet(err, exit);
    if (n > 0) {
        OPACKDictionaryApplierContext applierCtx;

        applierCtx.writeCtx = ctx;
        applierCtx.err = kNoErr;
        CFDictionaryApplyFunction(inDictionary, _OPACKEncodeDictionaryApplier, &applierCtx);
        require_noerr_action_quiet(applierCtx.err, exit, err = applierCtx.err);
    }
    if (u8 == kOPACKTagDictionary) {
        u8 = kOPACKTagTerminator;
        err = _OPACKEncodeBytes(ctx, &u8, 1);
        require_noerr_quiet(err, exit);
    }

exit:
    return (err);
}

//===========================================================================================================================

static void _OPACKEncodeDictionaryApplier(const void* inKey, const void* inValue, void* inContext)
{
    OPACKDictionaryApplierContext* const ctx = (OPACKDictionaryApplierContext*)inContext;
    OSStatus err;

    if (ctx->err)
        return;

    err = _OPACKEncodeObject(ctx->writeCtx, inKey);
    require_noerr_quiet(err, exit);

    err = _OPACKEncodeObject(ctx->writeCtx, inValue);
    require_noerr_quiet(err, exit);

exit:
    if (err)
        ctx->err = err;
}

//===========================================================================================================================

static OSStatus _OPACKEncodeNumber(OPACKEncodeContext* ctx, CFNumberRef inNumber)
{
    OSStatus err;
    uint8_t buf[9];
    size_t len;

    if (CFNumberIsFloatType(inNumber)) {
        if (ctx->uniqueDict) {
            Boolean skip = false;

            err = _OPACKEncodeUID(ctx, inNumber, &skip);
            require_noerr(err, exit);
            require_quiet(!skip, exit);
        }
        if (CFNumberGetByteSize(inNumber) <= 4) {
            Float32 f32;

            CFNumberGetValue(inNumber, kCFNumberFloat32Type, &f32);
            buf[0] = kOPACKTagFloat32;
            WriteLittleFloat32(&buf[1], f32);
            len = 5;
        } else {
            Float64 f64;

            CFNumberGetValue(inNumber, kCFNumberFloat64Type, &f64);
            buf[0] = kOPACKTagFloat64;
            WriteLittleFloat64(&buf[1], f64);
            len = 9;
        }
    } else {
        int64_t s64;

        // If number would take more than 1 byte (i.e. not small) and de-duping is enabled then encode the UID of a
        // previous dup. This check avoids consuming a UID when it wouldn't save any space.

        CFNumberGetValue(inNumber, kCFNumberSInt64Type, &s64);
        if (ctx->uniqueDict && ((s64 < kOPACKSmallIntegerMin) || (s64 > kOPACKSmallIntegerMax))) {
            Boolean skip = false;

            err = _OPACKEncodeUID(ctx, inNumber, &skip);
            require_noerr(err, exit);
            require_quiet(!skip, exit);
        }

        // Encode the integer in the least amount of space. This checks if the value is the same after truncating to
        // 8 bit then 16 bit then 32 bit, stopping at the first one that matches. Otherwise, it uses the full 64 bits.

        if (((int64_t)(int8_t)s64) == s64) {
            int8_t s8;

            s8 = (int8_t)s64;
            if ((s8 >= kOPACKSmallIntegerMin) && (s8 <= kOPACKSmallIntegerMax)) {
                buf[0] = (uint8_t)(kOPACKTagSmallIntMin + (s8 - kOPACKSmallIntegerMin));
                len = 1;
            } else {
                buf[0] = kOPACKTagInt8Bit;
                buf[1] = (uint8_t)s8;
                len = 2;
            }
        } else if (((int64_t)(int16_t)s64) == s64) {
            int16_t s16;

            s16 = (int16_t)s64;
            buf[0] = kOPACKTagInt16Bit;
            WriteLittle16(&buf[1], s16);
            len = 3;
        } else if (((int64_t)(int32_t)s64) == s64) {
            int32_t s32;

            s32 = (int32_t)s64;
            buf[0] = kOPACKTagInt32Bit;
            WriteLittle32(&buf[1], s32);
            len = 5;
        } else {
            buf[0] = kOPACKTagInt64Bit;
            WriteLittle64(&buf[1], s64);
            len = 9;
        }
    }
    err = _OPACKEncodeBytes(ctx, buf, len);
    require_noerr(err, exit);

exit:
    return (err);
}

//===========================================================================================================================

static OSStatus _OPACKEncodeString(OPACKEncodeContext* ctx, CFStringRef inString)
{
    OSStatus err;
    const char* cptr;
    char* cbuf = NULL;
    size_t len, len2;
    uint8_t buf[9];

    // If the string is non-empty and de-duping is enabled then encode the UID of a previous dup.
    // This check avoids consuming a UID when it wouldn't save any space.

    if (ctx->uniqueDict && (CFStringGetLength(inString) > 0)) {
        Boolean skip = false;

        err = _OPACKEncodeUID(ctx, inString, &skip);
        require_noerr_quiet(err, exit);
        require_quiet(!skip, exit);
    }

    // Encode the string as UTF-8. It's prefixed by the length, encoded in as few bytes as possible.

    err = CFStringGetOrCopyCStringUTF8(inString, &cptr, &cbuf, &len);
    require_noerr(err, exit);
    if (len <= kOPACKSmallStringMax) {
        buf[0] = (uint8_t)(kOPACKTagSmallStringMin + len);
        len2 = 1;
    } else if (len <= 0xFF) {
        buf[0] = kOPACKTagString8Bit;
        buf[1] = (uint8_t)len;
        len2 = 2;
    } else if (len <= 0xFFFF) {
        buf[0] = kOPACKTagString16Bit;
        WriteLittle16(&buf[1], len);
        len2 = 3;
    } else if (len <= UINT32_C(0xFFFFFFFF)) {
        buf[0] = kOPACKTagString32Bit;
        WriteLittle32(&buf[1], len);
        len2 = 5;
    } else {
        buf[0] = kOPACKTagString64Bit;
        WriteLittle64(&buf[1], (uint64_t)len);
        len2 = 9;
    }
    err = _OPACKEncodeBytes(ctx, buf, len2);
    require_noerr(err, exit);
    if (len > 0) {
        err = _OPACKEncodeBytes(ctx, cptr, len);
        require_noerr(err, exit);
    }

exit:
    FreeNullSafe(cbuf);
    return (err);
}

//===========================================================================================================================

static OSStatus _OPACKEncodeUID(OPACKEncodeContext* ctx, CFTypeRef inObj, Boolean* outSkip)
{
    OSStatus err;
    uintptr_t uid;
    uint8_t buf[9];
    size_t len;

    // Look up the object in our dictionary of unique objects. If we don't find a match then add it to the dictionary
    // using count of unique objects as the value. This becomes a map of objects indexed by their order of appearance.

    uid = (uintptr_t)CFDictionaryGetValue(ctx->uniqueDict, inObj);
    if (uid == 0) {
        CFDictionaryAddValue(ctx->uniqueDict, inObj, (const void*)++ctx->uniqueCount);
        *outSkip = false;
        err = kNoErr;
        goto exit;
    }

    // Match found. Encode the UID in the fewest number of bytes. Report that we should skip encoding the full object
    // because the UID of an equivalent object will be encoded in the stream instead.

    uid -= 1;
    if (uid <= kOPACKSmallUIDMax) {
        buf[0] = (uint8_t)(kOPACKTagSmallUIDMin + uid);
        len = 1;
    } else if (uid <= 0xFF) {
        buf[0] = kOPACKTagUID8Bit;
        buf[1] = (uint8_t)uid;
        len = 2;
    } else if (uid <= 0xFFFF) {
        buf[0] = kOPACKTagUID16Bit;
        WriteLittle16(&buf[1], uid);
        len = 3;
    } else if (uid <= 0xFFFFFF) {
        buf[0] = kOPACKTagUID24Bit;
        WriteLittle24(&buf[1], uid);
        len = 4;
    } else if (uid <= UINT32_C(0xFFFFFFFF)) {
        buf[0] = kOPACKTagUID32Bit;
        WriteLittle32(&buf[1], uid);
        len = 5;
    } else {
        err = kRangeErr;
        goto exit;
    }
    err = _OPACKEncodeBytes(ctx, buf, len);
    require_noerr(err, exit);
    *outSkip = true;

exit:
    return (err);
}

//===========================================================================================================================

static OSStatus _OPACKEncodeUUID(OPACKEncodeContext* ctx, CFTypeRef inUUID, bool inNS)
{
    OSStatus err;
    struct
    {
        uint8_t type;
        union {
            CFUUIDBytes cfUUIDBytes;
            uint8_t nsUUIDBytes[16];

        } u;

    } encodedUUID;

    check_compile_time_code(sizeof(encodedUUID) == 17);

    if (ctx->uniqueDict) {
        Boolean skip = false;

        err = _OPACKEncodeUID(ctx, inUUID, &skip);
        require_noerr_quiet(err, exit);
        require_quiet(!skip, exit);
    }

    encodedUUID.type = kOPACKTagUUID;
    (void)inNS;
    encodedUUID.u.cfUUIDBytes = CFUUIDGetUUIDBytes((CFUUIDRef)inUUID);
    err = _OPACKEncodeBytes(ctx, &encodedUUID, 17);
    require_noerr(err, exit);

exit:
    return (err);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================

CFTypeRef OPACKDecodeBytes(const void* inPtr, size_t inLen, OPACKFlags inFlags, OSStatus* outErr)
{
    return (_OPACKDecodeBytes(NULL, inPtr, inLen, inFlags, outErr));
}

//===========================================================================================================================

CFTypeRef OPACKDecodeData(CFDataRef inData, OPACKFlags inFlags, OSStatus* outErr)
{
    return (_OPACKDecodeBytes(inData, CFDataGetBytePtr(inData), (size_t)CFDataGetLength(inData), inFlags, outErr));
}

//===========================================================================================================================

static CFTypeRef _OPACKDecodeBytes(CFDataRef inData, const void* inPtr, size_t inLen, OPACKFlags inFlags, OSStatus* outErr)
{
    CFTypeRef result = NULL;
    OSStatus err;
    OPACKDecodeContext ctx;
    const uint8_t* src;

    _OPACKEnsureInitialized();

    ctx.data = inData;
    ctx.flags = inFlags;
    ctx.base = (const uint8_t*)inPtr;
    ctx.uniqueDict = NULL;
    ctx.uniqueCount = 0;
    ctx.depth = 0;

    if (!(inFlags & kOPACKFlag_Dups)) {
        CFDictionaryValueCallBacks dictCallbacks;

        dictCallbacks = kCFTypeDictionaryValueCallBacks;
        dictCallbacks.equal = _OPACKObjectsExactlyEqual;
        ctx.uniqueDict = CFDictionaryCreateMutable(NULL, 0, NULL, &dictCallbacks);
        require_action(ctx.uniqueDict, exit, err = kNoMemoryErr);
    }

    result = _OPACKDecodeObject(&ctx, ctx.base, ctx.base + inLen, &src, &err);
    require_noerr_quiet(err, exit);
    require_action_quiet(result, exit, err = kMalformedErr);

exit:
    CFReleaseNullSafe(ctx.uniqueDict);
    if (outErr)
        *outErr = err;
    return (result);
}

//===========================================================================================================================

static CFTypeRef
_OPACKDecodeObject(
    OPACKDecodeContext* ctx,
    const uint8_t* inSrc,
    const uint8_t* const inEnd,
    const uint8_t** outSrc,
    OSStatus* outErr)
{
    CFTypeRef result = NULL;
    OSStatus err;
    uint8_t b;

    require_action_quiet((inEnd - inSrc) >= 1, exit, err = kUnderrunErr);
    b = *inSrc++;

    // Note: these are ordered based on how frequently they occur in real world use.

    if (0) {
    }

    // SmallString

    else if ((b >= kOPACKTagSmallStringMin) && (b <= kOPACKTagSmallStringMax)) {
        result = _OPACKDecodeSmallString(ctx, inSrc, inEnd, b - kOPACKTagSmallStringMin, &inSrc, &err);
        require_noerr_quiet(err, exit);
    }

    // SmallInteger

    else if ((b >= kOPACKTagSmallIntMin) && (b <= kOPACKTagSmallIntMax)) {
        result = gCFNumbers[b - kOPACKTagSmallIntMin];
        require_action(result, exit, err = kInternalErr);
        CFRetain(result);
    }

    // String

    else if ((b >= kOPACKTagString8Bit) && (b <= kOPACKTagStringNullTerm)) {
        result = _OPACKDecodeString(ctx, inSrc, inEnd, b, &inSrc, &err);
        require_noerr_quiet(err, exit);
    }

    // Number

    else if ((b >= kOPACKTagInt8Bit) && (b <= kOPACKTagFloat64)) {
        result = _OPACKDecodeNumber(ctx, inSrc, inEnd, b, &inSrc, &err);
        require_noerr_quiet(err, exit);
    }

    // Misc

    else if (b == kOPACKTagTrue)
        result = kCFBooleanTrue;
    else if (b == kOPACKTagFalse)
        result = kCFBooleanFalse;
    else if (b == kOPACKTagNull)
        result = kCFNull;
    else if (b == kOPACKTagTerminator)
        result = NULL;

    // UID

    else if ((b >= kOPACKTagSmallUIDMin) && (b <= kOPACKTagUID32Bit)) {
        result = _OPACKDecodeUID(ctx, inSrc, inEnd, b, &inSrc, &err);
        require_noerr_quiet(err, exit);
    }

    // Dictionary

    else if ((b >= kOPACKTagSmallDictionaryMin) && (b <= kOPACKTagDictionary)) {
        result = _OPACKDecodeDictionary(ctx, inSrc, inEnd, b, &inSrc, &err);
        require_noerr_quiet(err, exit);
    }

    // Array

    else if ((b >= kOPACKTagSmallArrayMin) && (b <= kOPACKTagArray)) {
        result = _OPACKDecodeArray(ctx, inSrc, inEnd, b, &inSrc, &err);
        require_noerr_quiet(err, exit);
    }

    // SmallData

    else if ((b >= kOPACKTagSmallDataMin) && (b <= kOPACKTagSmallDataMax)) {
        b -= kOPACKTagSmallDataMin;
        require_action_quiet(((size_t)(inEnd - inSrc)) >= b, exit, err = kUnderrunErr);
        result = CFDataCreate(NULL, inSrc, (CFIndex)b);
        require_action(result, exit, err = kNoMemoryErr);
        inSrc += b;
        if ((b > 1) && ctx->uniqueDict) {
            CFDictionaryAddValue(ctx->uniqueDict, (const void*)ctx->uniqueCount++, result);
        }
    }

    // Data

    else if ((b >= kOPACKTagData8Bit) && (b <= kOPACKTagData64Bit)) {
        result = _OPACKDecodeData(ctx, inSrc, inEnd, b, &inSrc, &err);
        require_noerr_quiet(err, exit);
    } else if (b == kOPACKTagDataChunks) {
        result = _OPACKDecodeDataChunks(ctx, inSrc, inEnd, &inSrc, &err);
        require_noerr_quiet(err, exit);
    }

    // UUID

    else if (b == kOPACKTagUUID) {
        require_action_quiet((inEnd - inSrc) >= 16, exit, err = kUnderrunErr);
        {
            result = CFUUIDCreateFromUUIDBytes(NULL, *((const CFUUIDBytes*)inSrc));
        }
        require_action(result, exit, err = kNoMemoryErr);
        inSrc += 16;
        if (ctx->uniqueDict) {
            CFDictionaryAddValue(ctx->uniqueDict, (const void*)ctx->uniqueCount++, result);
        }
    }

    // Date

    else if (b == kOPACKTagDate) {
        require_action_quiet((inEnd - inSrc) >= 8, exit, err = kUnderrunErr);
        result = CFDateCreate(NULL, ReadLittleFloat64(inSrc));
        require_action(result, exit, err = kNoMemoryErr);
        inSrc += 8;
        if (ctx->uniqueDict) {
            CFDictionaryAddValue(ctx->uniqueDict, (const void*)ctx->uniqueCount++, result);
        }
    }

    // Unknown

    else {
        err = kUnsupportedErr;
        goto exit;
    }

    *outSrc = inSrc;
    err = kNoErr;

exit:
    if (outErr)
        *outErr = err;
    return (result);
}

//===========================================================================================================================

static CFTypeRef
_OPACKDecodeArray(
    OPACKDecodeContext* ctx,
    const uint8_t* inSrc,
    const uint8_t* const inEnd,
    uint8_t inTag,
    const uint8_t** outSrc,
    OSStatus* outErr)
{
    CFTypeRef result = NULL;
    CFMutableArrayRef array = NULL;
    CFTypeRef obj = NULL;
    OSStatus err;

    array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    require_action(array, exit, err = kNoMemoryErr);

    // If the tag contains the length then decode exactly that number of objects.
    // Otherwise, the array is terminated by reading a terminator object (_OPACKDecodeObject returns NULL).
    // This also does depth checking to protect against accidental or malicious arrays that are too deep.

    if ((inTag >= kOPACKTagSmallArrayMin) && (inTag <= kOPACKTagSmallArrayMax)) {
        size_t i, n;

        n = inTag - kOPACKTagSmallArrayMin;
        for (i = 0; i < n; ++i) {
            require_action_quiet(ctx->depth < kOPACKMaxDepth, exit, err = kOverrunErr);
            ++ctx->depth;
            obj = _OPACKDecodeObject(ctx, inSrc, inEnd, &inSrc, &err);
            --ctx->depth;
            require_noerr_quiet(err, exit);
            require_action_quiet(obj, exit, err = kMalformedErr);
            CFArrayAppendValue(array, obj);
            CFRelease(obj);
            obj = NULL;
        }
    } else {
        for (;;) {
            require_action_quiet(ctx->depth < kOPACKMaxDepth, exit, err = kOverrunErr);
            ++ctx->depth;
            obj = _OPACKDecodeObject(ctx, inSrc, inEnd, &inSrc, &err);
            --ctx->depth;
            require_noerr_quiet(err, exit);
            if (!obj)
                break;
            CFArrayAppendValue(array, obj);
            CFRelease(obj);
            obj = NULL;
        }
    }
    result = array;
    array = NULL;
    err = kNoErr;

exit:
    CFReleaseNullSafe(obj);
    CFReleaseNullSafe(array);
    *outSrc = inSrc;
    *outErr = err;
    return (result);
}

//===========================================================================================================================

static CFTypeRef
_OPACKDecodeData(
    OPACKDecodeContext* ctx,
    const uint8_t* inSrc,
    const uint8_t* const inEnd,
    uint8_t inTag,
    const uint8_t** outSrc,
    OSStatus* outErr)
{
    CFTypeRef result = NULL;
    OSStatus err;
    size_t len;

    switch (inTag) {
    case kOPACKTagData8Bit:
        require_action_quiet((inEnd - inSrc) >= 1, exit, err = kUnderrunErr);
        len = *inSrc++;
        break;

    case kOPACKTagData16Bit:
        require_action_quiet((inEnd - inSrc) >= 2, exit, err = kUnderrunErr);
        len = ReadLittle16(inSrc);
        inSrc += 2;
        break;

    case kOPACKTagData32Bit:
        require_action_quiet((inEnd - inSrc) >= 4, exit, err = kUnderrunErr);
        len = ReadLittle32(inSrc);
        inSrc += 4;
        require_action_quiet(len <= SSIZE_MAX, exit, err = kSizeErr);
        break;

    case kOPACKTagData64Bit: {
        uint64_t u64;

        require_action_quiet((inEnd - inSrc) >= 8, exit, err = kUnderrunErr);
        u64 = ReadLittle64(inSrc);
        inSrc += 8;
        require_action_quiet(u64 <= SSIZE_MAX, exit, err = kSizeErr);
        len = (size_t)u64;
        break;
    }

    default:
        err = kInternalErr;
        goto exit;
    }
    require_action_quiet(((size_t)(inEnd - inSrc)) >= len, exit, err = kUnderrunErr);
    result = CFDataCreate(NULL, inSrc, (CFIndex)len);
    require_action(result, exit, err = kNoMemoryErr);
    inSrc += len;
    if ((len > 1) && ctx->uniqueDict) {
        CFDictionaryAddValue(ctx->uniqueDict, (const void*)ctx->uniqueCount++, result);
    }
    err = kNoErr;

exit:
    *outSrc = inSrc;
    *outErr = err;
    return (result);
}

//===========================================================================================================================

static CFTypeRef
_OPACKDecodeDataChunks(
    OPACKDecodeContext* ctx,
    const uint8_t* inSrc,
    const uint8_t* const inEnd,
    const uint8_t** outSrc,
    OSStatus* outErr)
{
    CFTypeRef result = NULL;
    uint8_t* buf = NULL;
    size_t totalLen = 0;
    OSStatus err;

    for (;;) {
        uint8_t tag;
        size_t len;
        uint8_t* tempBuf;
        size_t tempLen;

        // Decode the length of the chunk. The length is embedded in the tag for small lengths.
        // Otherwise, the length is encoded in the bytes that follow the length, before the data.

        require_action_quiet((inEnd - inSrc) >= 1, exit, err = kUnderrunErr);
        tag = *inSrc++;
        if ((tag >= kOPACKTagSmallDataMin) && (tag <= kOPACKTagSmallDataMax)) {
            len = tag - kOPACKTagSmallDataMin;
        } else {
            switch (tag) {
            case kOPACKTagData8Bit:
                require_action_quiet((inEnd - inSrc) >= 1, exit, err = kUnderrunErr);
                len = *inSrc++;
                break;

            case kOPACKTagData16Bit:
                require_action_quiet((inEnd - inSrc) >= 2, exit, err = kUnderrunErr);
                len = ReadLittle16(inSrc);
                inSrc += 2;
                break;

            case kOPACKTagData32Bit:
                require_action_quiet((inEnd - inSrc) >= 4, exit, err = kUnderrunErr);
                len = ReadLittle32(inSrc);
                inSrc += 4;
                require_action_quiet(len <= SSIZE_MAX, exit, err = kSizeErr);
                break;

            case kOPACKTagData64Bit: {
                uint64_t u64;

                require_action_quiet((inEnd - inSrc) >= 8, exit, err = kUnderrunErr);
                u64 = ReadLittle64(inSrc);
                inSrc += 8;
                require_action_quiet(u64 <= SSIZE_MAX, exit, err = kSizeErr);
                len = (size_t)u64;
                break;
            }

            case kOPACKTagTerminator:
                goto end;

            default:
                err = kMalformedErr;
                goto exit;
            }
        }
        if (len == 0)
            continue;

        // Resize the buffer to hold the new length after the new chunk is added. Then append the new chunk data.

        require_action_quiet(((size_t)(inEnd - inSrc)) >= len, exit, err = kUnderrunErr);
        tempLen = totalLen + len;
        require_action_quiet(tempLen >= len, exit, err = kRangeErr); // Detect overflow.
        tempBuf = (uint8_t*)realloc(buf, tempLen);
        require_action(tempBuf, exit, err = kNoMemoryErr);
        buf = tempBuf;
        memcpy(&buf[totalLen], inSrc, len);
        inSrc += len;
        totalLen = tempLen;
    }

end:
    // Create a data object to wrap the malloc'd memory. This avoids needing to re-allocate and re-copy the data.
    // If there's no buffer than there wasn't any data so it only needs an empty data object for it.

    require_action_quiet(totalLen <= SSIZE_MAX, exit, err = kSizeErr);
    if (buf)
        result = CFDataCreateWithBytesNoCopy(NULL, buf, (CFIndex)totalLen, kCFAllocatorMalloc);
    else
        result = CFDataCreate(NULL, (const uint8_t*)"", 0);
    require_action(result, exit, err = kNoMemoryErr);
    buf = NULL;
    if ((totalLen > 1) && ctx->uniqueDict) {
        CFDictionaryAddValue(ctx->uniqueDict, (const void*)ctx->uniqueCount++, result);
    }
    err = kNoErr;

exit:
    FreeNullSafe(buf);
    *outSrc = inSrc;
    *outErr = err;
    return (result);
}

//===========================================================================================================================

static CFTypeRef
_OPACKDecodeDictionary(
    OPACKDecodeContext* ctx,
    const uint8_t* inSrc,
    const uint8_t* const inEnd,
    uint8_t inTag,
    const uint8_t** outSrc,
    OSStatus* outErr)
{
    CFTypeRef result = NULL;
    CFMutableDictionaryRef dict = NULL;
    CFTypeRef key = NULL;
    CFTypeRef value = NULL;
    OSStatus err;

    dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(dict, exit, err = kNoMemoryErr);

    // If the tag contains the length then decode exactly that number of key/value pairs of objects.
    // Otherwise, the dictionary is terminated by reading a single terminator object (_OPACKDecodeObject returns NULL).
    // This also does depth checking to protect against accidental or malicious dictionaries that are too deep.

    if ((inTag >= kOPACKTagSmallDictionaryMin) && (inTag <= kOPACKTagSmallDictionaryMax)) {
        size_t i, n;

        n = inTag - kOPACKTagSmallDictionaryMin;
        for (i = 0; i < n; ++i) {
            require_action_quiet(ctx->depth < kOPACKMaxDepth, exit, err = kOverrunErr);
            ++ctx->depth;
            key = _OPACKDecodeObject(ctx, inSrc, inEnd, &inSrc, &err);
            --ctx->depth;
            require_noerr_quiet(err, exit);
            require_action_quiet(key, exit, err = kMalformedErr);

            ++ctx->depth;
            value = _OPACKDecodeObject(ctx, inSrc, inEnd, &inSrc, &err);
            --ctx->depth;
            require_noerr_quiet(err, exit);
            require_action_quiet(value, exit, err = kMalformedErr);

            CFDictionaryAddValue(dict, key, value);
            CFRelease(key);
            key = NULL;
            CFRelease(value);
            value = NULL;
        }
    } else {
        for (;;) {
            require_action_quiet(ctx->depth < kOPACKMaxDepth, exit, err = kOverrunErr);
            ++ctx->depth;
            key = _OPACKDecodeObject(ctx, inSrc, inEnd, &inSrc, &err);
            --ctx->depth;
            require_noerr_quiet(err, exit);
            if (!key)
                break;

            ++ctx->depth;
            value = _OPACKDecodeObject(ctx, inSrc, inEnd, &inSrc, &err);
            --ctx->depth;
            require_noerr_quiet(err, exit);
            require_action_quiet(value, exit, err = kMalformedErr);

            CFDictionaryAddValue(dict, key, value);
            CFRelease(key);
            key = NULL;
            CFRelease(value);
            value = NULL;
        }
    }
    result = dict;
    dict = NULL;
    err = kNoErr;

exit:
    CFReleaseNullSafe(key);
    CFReleaseNullSafe(value);
    CFReleaseNullSafe(dict);
    *outSrc = inSrc;
    *outErr = err;
    return (result);
}

//===========================================================================================================================

static CFTypeRef
_OPACKDecodeNumber(
    OPACKDecodeContext* ctx,
    const uint8_t* inSrc,
    const uint8_t* const inEnd,
    uint8_t inTag,
    const uint8_t** outSrc,
    OSStatus* outErr)
{
    CFTypeRef result = NULL;
    OSStatus err;
    Value64 val;
    CFNumberType type;
    Boolean unique = false;

    // Decode the size of the integer. If the number is in a range that wouldn't normally fit into a single byte
    // then it's also marked for check if it's unique to allow it to be track for subsequent de-duping.

    switch (inTag) {
    case kOPACKTagInt8Bit:
        require_action_quiet((inEnd - inSrc) >= 1, exit, err = kUnderrunErr);
        val.s8[0] = (int8_t)*inSrc++;
        type = kCFNumberSInt8Type;
        if ((val.s8[0] < kOPACKSmallIntegerMin) || (val.s8[0] > kOPACKSmallIntegerMax))
            unique = true;
        break;

    case kOPACKTagInt16Bit:
        require_action_quiet((inEnd - inSrc) >= 2, exit, err = kUnderrunErr);
        val.s16[0] = (int16_t)ReadLittle16(inSrc);
        inSrc += 2;
        type = kCFNumberSInt16Type;
        if ((val.s16[0] < kOPACKSmallIntegerMin) || (val.s16[0] > kOPACKSmallIntegerMax))
            unique = true;
        break;

    case kOPACKTagInt32Bit:
        require_action_quiet((inEnd - inSrc) >= 4, exit, err = kUnderrunErr);
        val.s32[0] = (int32_t)ReadLittle32(inSrc);
        inSrc += 4;
        type = kCFNumberSInt32Type;
        if ((val.s32[0] < kOPACKSmallIntegerMin) || (val.s32[0] > kOPACKSmallIntegerMax))
            unique = true;
        break;

    case kOPACKTagInt64Bit:
        require_action_quiet((inEnd - inSrc) >= 8, exit, err = kUnderrunErr);
        val.s64 = (int64_t)ReadLittle64(inSrc);
        inSrc += 8;
        type = kCFNumberSInt64Type;
        if ((val.s64 < kOPACKSmallIntegerMin) || (val.s64 > kOPACKSmallIntegerMax))
            unique = true;
        break;

    case kOPACKTagFloat32:
        require_action_quiet((inEnd - inSrc) >= 4, exit, err = kUnderrunErr);
        val.f32[0] = ReadLittleFloat32(inSrc);
        inSrc += 4;
        type = kCFNumberFloat32Type;
        unique = true;
        break;

    case kOPACKTagFloat64:
        require_action_quiet((inEnd - inSrc) >= 8, exit, err = kUnderrunErr);
        val.f64 = ReadLittleFloat64(inSrc);
        inSrc += 8;
        type = kCFNumberFloat64Type;
        unique = true;
        break;

    default:
        err = kInternalErr;
        goto exit;
    }
    result = CFNumberCreate(NULL, type, &val);
    require_action(result, exit, err = kNoMemoryErr);
    if (unique && ctx->uniqueDict) {
        CFDictionaryAddValue(ctx->uniqueDict, (const void*)ctx->uniqueCount++, result);
    }
    err = kNoErr;

exit:
    *outSrc = inSrc;
    *outErr = err;
    return (result);
}

//===========================================================================================================================

static CFTypeRef
_OPACKDecodeSmallString(
    OPACKDecodeContext* ctx,
    const uint8_t* inSrc,
    const uint8_t* const inEnd,
    size_t inLen,
    const uint8_t** outSrc,
    OSStatus* outErr)
{
    CFTypeRef result = NULL;
    OSStatus err;

    if (inLen > 0) {
        require_action_quiet(((size_t)(inEnd - inSrc)) >= inLen, exit, err = kUnderrunErr);
        result = CFStringCreateWithBytes(NULL, inSrc, (CFIndex)inLen, kCFStringEncodingUTF8, false);
        require_action(result, exit, err = kNoMemoryErr);
        inSrc += inLen;
        if (ctx->uniqueDict) {
            CFDictionaryAddValue(ctx->uniqueDict, (const void*)ctx->uniqueCount++, result);
        }
    } else {
        result = CFSTR("");
    }
    err = kNoErr;

exit:
    *outSrc = inSrc;
    *outErr = err;
    return (result);
}

//===========================================================================================================================

static CFTypeRef
_OPACKDecodeString(
    OPACKDecodeContext* ctx,
    const uint8_t* inSrc,
    const uint8_t* const inEnd,
    uint8_t inTag,
    const uint8_t** outSrc,
    OSStatus* outErr)
{
    CFTypeRef result = NULL;
    OSStatus err;
    size_t len, trailLen = 0;

    // Decode the string length as the number of UTF-8 bytes.

    switch (inTag) {
    case kOPACKTagString8Bit:
        require_action_quiet((inEnd - inSrc) >= 1, exit, err = kUnderrunErr);
        len = Read8(inSrc);
        inSrc += 1;
        break;

    case kOPACKTagString16Bit:
        require_action_quiet((inEnd - inSrc) >= 2, exit, err = kUnderrunErr);
        len = ReadLittle16(inSrc);
        inSrc += 2;
        break;

    case kOPACKTagString32Bit:
        require_action_quiet((inEnd - inSrc) >= 4, exit, err = kUnderrunErr);
        len = ReadLittle32(inSrc);
        inSrc += 4;
        require_action_quiet(len <= SSIZE_MAX, exit, err = kSizeErr);
        break;

    case kOPACKTagString64Bit: {
        uint64_t u64;

        require_action_quiet((inEnd - inSrc) >= 8, exit, err = kUnderrunErr);
        u64 = ReadLittle64(inSrc);
        inSrc += 8;
        require_action_quiet(u64 <= SSIZE_MAX, exit, err = kSizeErr);
        len = (size_t)u64;
        break;
    }

    case kOPACKTagStringNullTerm: {
        const uint8_t* ptr;

        // The string is null terminated rather than being length prefixed so walk to the end of the string
        // to determine its length while also ensuring that we don't read beyond the bounds of the input.

        for (ptr = inSrc; (ptr < inEnd) && (*ptr != 0); ++ptr)
            ++ptr;
        require_action_quiet(ptr < inEnd, exit, err = kUnderrunErr);
        len = (size_t)(ptr - inSrc);
        require_action_quiet(len <= SSIZE_MAX, exit, err = kSizeErr);
        trailLen = 1;
        break;
    }

    default:
        err = kInternalErr;
        goto exit;
    }
    require_action_quiet(((size_t)(inEnd - inSrc)) >= len, exit, err = kUnderrunErr);
    result = CFStringCreateWithBytes(NULL, inSrc, (CFIndex)len, kCFStringEncodingUTF8, false);
    require_action(result, exit, err = kNoMemoryErr);
    inSrc += (len + trailLen);
    if ((len > 0) && ctx->uniqueDict) {
        CFDictionaryAddValue(ctx->uniqueDict, (const void*)ctx->uniqueCount++, result);
    }
    err = kNoErr;

exit:
    *outSrc = inSrc;
    *outErr = err;
    return (result);
}

//===========================================================================================================================

static CFTypeRef
_OPACKDecodeUID(
    OPACKDecodeContext* ctx,
    const uint8_t* inSrc,
    const uint8_t* const inEnd,
    uint8_t inTag,
    const uint8_t** outSrc,
    OSStatus* outErr)
{
    CFTypeRef result = NULL;
    OSStatus err;
    uintptr_t uid;

    if ((inTag >= kOPACKTagSmallUIDMin) && (inTag <= kOPACKTagSmallUIDMax)) {
        uid = inTag - kOPACKTagSmallUIDMin;
    } else {
        switch (inTag) {
        case kOPACKTagUID8Bit:
            require_action_quiet((inEnd - inSrc) >= 1, exit, err = kUnderrunErr);
            uid = *inSrc++;
            break;

        case kOPACKTagUID16Bit:
            require_action_quiet((inEnd - inSrc) >= 2, exit, err = kUnderrunErr);
            uid = ReadLittle16(inSrc);
            inSrc += 2;
            break;

        case kOPACKTagUID24Bit:
            require_action_quiet((inEnd - inSrc) >= 4, exit, err = kUnderrunErr);
            uid = ReadLittle24(inSrc);
            inSrc += 3;
            break;

        case kOPACKTagUID32Bit:
            require_action_quiet((inEnd - inSrc) >= 4, exit, err = kUnderrunErr);
            uid = ReadLittle32(inSrc);
            inSrc += 4;
            break;

        default:
            err = kInternalErr;
            goto exit;
        }
    }
    require_action_quiet(ctx->uniqueDict, exit, err = kUnsupportedDataErr);
    result = CFDictionaryGetValue(ctx->uniqueDict, (const void*)uid);
    require_action_quiet(result, exit, err = kIDErr);
    CFRetain(result);
    err = kNoErr;

exit:
    *outSrc = inSrc;
    *outErr = err;
    return (result);
}
