/*
    Copyright (C) 2007-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/
#include "SDPUtils.h"
#include "CommonServices.h"
#include "DebugServices.h"
#include "StringUtils.h"
#include <ctype.h>

//===========================================================================================================================
//    SDPFindAttribute
//===========================================================================================================================
OSStatus
SDPFindAttribute(
    const char* inSrc,
    const char* inEnd,
    const char* inAttribute,
    const char** outValuePtr,
    size_t* outValueLen,
    const char** outNextPtr)
{
    OSStatus err;
    char type;
    const char* valuePtr;
    size_t valueLen;

    while ((err = SDPGetNext(inSrc, inEnd, &type, &valuePtr, &valueLen, &inSrc)) == kNoErr) {
        if (type == 'a') {
            const char* ptr;
            const char* end;

            ptr = valuePtr;
            end = ptr + valueLen;
            while ((ptr < end) && (*ptr != ':'))
                ++ptr;
            if ((ptr < end) && (strncmpx(valuePtr, (size_t)(ptr - valuePtr), inAttribute) == 0)) {
                ptr += 1;
                *outValuePtr = ptr;
                *outValueLen = (size_t)(end - ptr);
                break;
            }
        }
    }
    if (outNextPtr)
        *outNextPtr = inSrc;
    return (err);
}
//===========================================================================================================================
//    SDPFindMediaSection
//===========================================================================================================================
OSStatus
SDPFindMediaSection(
    const char* inSrc,
    const char* inEnd,
    const char** outMediaSectionPtr,
    const char** outMediaSectionEnd,
    const char** outMediaValuePtr,
    size_t* outMediaValueLen,
    const char** outNextPtr)
{
    OSStatus err;
    char type;
    const char* valuePtr;
    size_t valueLen;
    const char* mediaSectionPtr;
    const char* mediaValuePtr;
    size_t mediaValueLen;

    // Find a media section.

    mediaSectionPtr = NULL;
    mediaValuePtr = NULL;
    mediaValueLen = 0;
    while (SDPGetNext(inSrc, inEnd, &type, &valuePtr, &valueLen, &inSrc) == kNoErr) {
        if (type == 'm') {
            mediaSectionPtr = valuePtr - 2; // Skip back to "m=".
            mediaValuePtr = valuePtr;
            mediaValueLen = valueLen;
            break;
        }
    }
    require_action_quiet(mediaSectionPtr, exit, err = kNotFoundErr);

    // Find the end of the media section.

    while (SDPGetNext(inSrc, inEnd, &type, &valuePtr, &valueLen, &inSrc) == kNoErr) {
        if (type == 'm') {
            inSrc = valuePtr - 2; // Skip back to "m=" of the next section.
            break;
        }
    }

    if (outMediaSectionPtr)
        *outMediaSectionPtr = mediaSectionPtr;
    if (outMediaSectionEnd)
        *outMediaSectionEnd = inSrc;
    if (outMediaValuePtr)
        *outMediaValuePtr = mediaValuePtr;
    if (outMediaValueLen)
        *outMediaValueLen = mediaValueLen;
    if (outNextPtr)
        *outNextPtr = inSrc;
    err = kNoErr;

exit:
    return (err);
}
//===========================================================================================================================
//    SDPFindMediaSectionEx
//===========================================================================================================================
OSStatus
SDPFindMediaSectionEx(
    const char* inSrc,
    const char* inEnd,
    const char** outMediaTypePtr,
    size_t* outMediaTypeLen,
    const char** outPortPtr,
    size_t* outPortLen,
    const char** outProtocolPtr,
    size_t* outProtocolLen,
    const char** outFormatPtr,
    size_t* outFormatLen,
    const char** outSubSectionsPtr,
    size_t* outSubSectionsLen,
    const char** outSrc)
{
    OSStatus err;
    char type;
    const char* valuePtr;
    size_t valueLen;
    const char* mediaValuePtr;
    size_t mediaValueLen;
    const char* subSectionPtr;
    const char* ptr;
    const char* end;
    const char* token;

    // Find a media section. Media sections start with "m=" and end with another media section or the end of data.
    // The following is an example of two media sections, "audio" and "video".
    //
    //        m=audio 21010 RTP/AVP 5\r\n
    //        c=IN IP4 224.0.1.11/127\r\n
    //        a=ptime:40\r\n
    //        m=video 61010 RTP/AVP 31\r\n
    //        c=IN IP4 224.0.1.12/127\r\n

    while ((err = SDPGetNext(inSrc, inEnd, &type, &mediaValuePtr, &mediaValueLen, &inSrc)) == kNoErr) {
        if (type == 'm') {
            break;
        }
    }
    require_noerr_quiet(err, exit);
    subSectionPtr = inSrc;

    // Find the end of the media section. Media sections end with another media section or the end of data.

    while (SDPGetNext(inSrc, inEnd, &type, &valuePtr, &valueLen, &inSrc) == kNoErr) {
        if (type == 'm') {
            inSrc = valuePtr - 2; // Skip back to "m=" of the next section.
            break;
        }
    }

    if (outSubSectionsPtr)
        *outSubSectionsPtr = subSectionPtr;
    if (outSubSectionsLen)
        *outSubSectionsLen = (size_t)(inSrc - subSectionPtr);
    if (outSrc)
        *outSrc = inSrc;

    // Parse details out of the media line (after the m= prefix). It has the following format:
    //
    //        <media> <port> <proto> <fmt> ...
    //
    //        "video 49170/2 RTP/AVP 31"

    ptr = mediaValuePtr;
    end = ptr + mediaValueLen;

    // Media Type

    while ((ptr < end) && isspace_safe(*ptr))
        ++ptr;
    token = ptr;
    while ((ptr < end) && !isspace_safe(*ptr))
        ++ptr;
    if (outMediaTypePtr)
        *outMediaTypePtr = token;
    if (outMediaTypeLen)
        *outMediaTypeLen = (size_t)(ptr - token);

    // Port

    while ((ptr < end) && isspace_safe(*ptr))
        ++ptr;
    token = ptr;
    while ((ptr < end) && !isspace_safe(*ptr))
        ++ptr;
    if (outPortPtr)
        *outPortPtr = token;
    if (outPortLen)
        *outPortLen = (size_t)(ptr - token);

    // Protocol

    while ((ptr < end) && isspace_safe(*ptr))
        ++ptr;
    token = ptr;
    while ((ptr < end) && !isspace_safe(*ptr))
        ++ptr;
    if (outProtocolPtr)
        *outProtocolPtr = token;
    if (outProtocolLen)
        *outProtocolLen = (size_t)(ptr - token);

    // Format

    while ((ptr < end) && isspace_safe(*ptr))
        ++ptr;
    while ((ptr < end) && isspace_safe(end[-1]))
        --end;
    if (outFormatPtr)
        *outFormatPtr = ptr;
    if (outFormatLen)
        *outFormatLen = (size_t)(end - ptr);

exit:
    return (err);
}
//===========================================================================================================================
//    SDPFindParameter
//===========================================================================================================================
OSStatus
SDPFindParameter(
    const char* inSrc,
    const char* inEnd,
    const char* inName,
    const char** outValuePtr,
    size_t* outValueLen,
    const char** outNextPtr)
{
    const char* namePtr;
    size_t nameLen;
    const char* valuePtr;
    size_t valueLen;

    while (SDPGetNextParameter(inSrc, inEnd, &namePtr, &nameLen, &valuePtr, &valueLen, &inSrc) == kNoErr) {
        if (strncmpx(namePtr, nameLen, inName) == 0) {
            if (outValuePtr)
                *outValuePtr = valuePtr;
            if (outValueLen)
                *outValueLen = valueLen;
            if (outNextPtr)
                *outNextPtr = inSrc;
            return (kNoErr);
        }
    }
    if (outNextPtr)
        *outNextPtr = inSrc;
    return (kNotFoundErr);
}
//===========================================================================================================================
//    SDPFindSessionSection
//===========================================================================================================================
OSStatus
SDPFindSessionSection(
    const char* inSrc,
    const char* inEnd,
    const char** outSectionPtr,
    const char** outSectionEnd,
    const char** outNextPtr)
{
    const char* sessionPtr;
    char type;
    const char* valuePtr;
    size_t valueLen;

    // SDP session sections start at the beginning of the text and go until the next section (or the end of text).

    sessionPtr = inSrc;
    while (SDPGetNext(inSrc, inEnd, &type, &valuePtr, &valueLen, &inSrc) == kNoErr) {
        if (type == 'm') {
            inSrc = valuePtr - 2; // Skip to just before "m=" of the next section.
            break;
        }
    }
    if (outSectionPtr)
        *outSectionPtr = sessionPtr;
    if (outSectionEnd)
        *outSectionEnd = inSrc;
    if (outNextPtr)
        *outNextPtr = inSrc;
    return (kNoErr);
}
//===========================================================================================================================
//    SDPFindAttribute
//===========================================================================================================================
OSStatus
SDPFindType(
    const char* inSrc,
    const char* inEnd,
    char inType,
    const char** outValuePtr,
    size_t* outValueLen,
    const char** outNextPtr)
{
    OSStatus err;
    char type;
    const char* valuePtr;
    size_t valueLen;

    while ((err = SDPGetNext(inSrc, inEnd, &type, &valuePtr, &valueLen, &inSrc)) == kNoErr) {
        if (type == inType) {
            if (outValuePtr)
                *outValuePtr = valuePtr;
            if (outValueLen)
                *outValueLen = valueLen;
            break;
        }
    }
    if (outNextPtr)
        *outNextPtr = inSrc;
    return (err);
}
//===========================================================================================================================
//    SDPGetNext
//===========================================================================================================================
OSStatus
SDPGetNext(
    const char* inSrc,
    const char* inEnd,
    char* outType,
    const char** outValuePtr,
    size_t* outValueLen,
    const char** outNextPtr)
{
    OSStatus err;
    char type;
    const char* val;
    size_t len;
    char c;

    require_action_quiet(inSrc < inEnd, exit, err = kNotFoundErr);

    // Parse a <type>=<value> line (e.g. "v=0\r\n").

    require_action((inEnd - inSrc) >= 2, exit, err = kSizeErr);
    type = inSrc[0];
    require_action(inSrc[1] == '=', exit, err = kMalformedErr);
    inSrc += 2;

    for (val = inSrc; (inSrc < inEnd) && ((c = *inSrc) != '\r') && (c != '\n'); ++inSrc) {
    }
    len = (size_t)(inSrc - val);

    while ((inSrc < inEnd) && (((c = *inSrc) == '\r') || (c == '\n')))
        ++inSrc;

    // Return results.

    *outType = type;
    *outValuePtr = val;
    *outValueLen = len;
    *outNextPtr = inSrc;
    err = kNoErr;

exit:
    return (err);
}
//===========================================================================================================================
//    SDPGetNextAttribute
//===========================================================================================================================
OSStatus
SDPGetNextAttribute(
    const char* inSrc,
    const char* inEnd,
    const char** outNamePtr,
    size_t* outNameLen,
    const char** outValuePtr,
    size_t* outValueLen,
    const char** outNextPtr)
{
    OSStatus err;
    char type;
    const char* src;
    const char* ptr;
    const char* end;
    size_t len;

    while ((err = SDPGetNext(inSrc, inEnd, &type, &src, &len, &inSrc)) == kNoErr) {
        if (type == 'a') {
            break;
        }
    }
    require_noerr_quiet(err, exit);

    ptr = src;
    end = src + len;
    while ((src < end) && (*src != ':'))
        ++src;

    if (outNamePtr)
        *outNamePtr = ptr;
    if (outNameLen)
        *outNameLen = (size_t)(src - ptr);

    if (src < end)
        ++src;
    if (outValuePtr)
        *outValuePtr = src;
    if (outValueLen)
        *outValueLen = (size_t)(end - src);

exit:
    if (outNextPtr)
        *outNextPtr = inSrc;
    return (err);
}
//===========================================================================================================================
//    SDPGetNextParameter
//===========================================================================================================================
OSStatus
SDPGetNextParameter(
    const char* inSrc,
    const char* inEnd,
    const char** outNamePtr,
    size_t* outNameLen,
    const char** outValuePtr,
    size_t* outValueLen,
    const char** outNextPtr)
{
    const char* ptr;

    while ((inSrc < inEnd) && isspace_safe(*inSrc))
        ++inSrc;
    if (inSrc == inEnd)
        return (kNotFoundErr);

    for (ptr = inSrc; (inSrc < inEnd) && (*inSrc != '='); ++inSrc) {
    }
    if (outNamePtr)
        *outNamePtr = ptr;
    if (outNameLen)
        *outNameLen = (size_t)(inSrc - ptr);
    if (inSrc < inEnd)
        ++inSrc;

    for (ptr = inSrc; (inSrc < inEnd) && (*inSrc != ';'); ++inSrc) {
    }
    if (outValuePtr)
        *outValuePtr = ptr;
    if (outValueLen)
        *outValueLen = (size_t)(inSrc - ptr);

    if (outNextPtr)
        *outNextPtr = (inSrc < inEnd) ? (inSrc + 1) : inSrc;
    return (kNoErr);
}
//===========================================================================================================================
//    SDPScanFAttribute
//===========================================================================================================================
int SDPScanFAttribute(
    const char* inSrc,
    const char* inEnd,
    const char** outSrc,
    const char* inAttribute,
    const char* inFormat,
    ...)
{
    int n;
    va_list args;

    va_start(args, inFormat);
    n = SDPScanFAttributeV(inSrc, inEnd, outSrc, inAttribute, inFormat, args);
    va_end(args);
    return (n);
}
//===========================================================================================================================
//    SDPScanFAttributeV
//===========================================================================================================================
int SDPScanFAttributeV(
    const char* inSrc,
    const char* inEnd,
    const char** outSrc,
    const char* inAttribute,
    const char* inFormat,
    va_list inArgs)
{
    int n;
    OSStatus err;
    const char* valuePtr;
    size_t valueLen;

    n = 0;
    err = SDPFindAttribute(inSrc, inEnd, inAttribute, &valuePtr, &valueLen, outSrc);
    require_noerr_quiet(err, exit);

    n = VSNScanF(valuePtr, valueLen, inFormat, inArgs);

exit:
    return (n);
}
#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif
#if (DEBUG && !EXCLUDE_UNIT_TESTS)
//===========================================================================================================================
//    SDPUtils_Test
//===========================================================================================================================
OSStatus SDPUtils_Test(void)
{
    OSStatus err;
    const char* src;
    const char* end;
    const char* ptr;
    size_t len;
    int i, n;
    char type;
    const char* namePtr;
    size_t nameLen;
    const char* valuePtr;
    size_t valueLen;
    const char* mediaSectionPtr;
    const char* mediaSectionEnd;
    const char* mediaTypePtr;
    size_t mediaTypeLen;
    const char* portPtr;
    size_t portLen;
    const char* protocolPtr;
    size_t protocolLen;
    const char* formatPtr;
    size_t formatLen;
    const char* sectionPtr;
    const char* sectionEnd;
    size_t sectionLen;

    src = "v=0\r\n"
          "o=iTunes 3208725422 0 IN IP4 24.1.2.49\r\n"
          "s=iTunes\r\n"
          "c=IN IP4 24.1.2.50\r\n"
          "t=0 0\r\n"
          "a=abc:1234\r\n"
          "a=xyz:5678\r\n";
    end = src + strlen(src);

    sectionPtr = NULL;
    sectionEnd = NULL;
    err = SDPFindSessionSection(src, end, &sectionPtr, &sectionEnd, &ptr);
    require_noerr(err, exit);
    require_action(strncmp_prefix(sectionPtr, (size_t)(sectionEnd - sectionPtr), "v=0\r\n") == 0, exit, err = -1);
    require_action(sectionEnd == end, exit, err = -1);
    require_action(ptr == end, exit, err = -1);

    err = SDPGetNextAttribute(sectionPtr, sectionEnd, &namePtr, &nameLen, &valuePtr, &valueLen, &sectionPtr);
    require_noerr(err, exit);
    require_action(strncmpx(namePtr, nameLen, "abc") == 0, exit, err = -1);
    require_action(strncmpx(valuePtr, valueLen, "1234") == 0, exit, err = -1);

    err = SDPGetNextAttribute(sectionPtr, sectionEnd, &namePtr, &nameLen, &valuePtr, &valueLen, &sectionPtr);
    require_noerr(err, exit);
    require_action(strncmpx(namePtr, nameLen, "xyz") == 0, exit, err = -1);
    require_action(strncmpx(valuePtr, valueLen, "5678") == 0, exit, err = -1);

    err = SDPGetNextAttribute(sectionPtr, sectionEnd, &namePtr, &nameLen, &valuePtr, &valueLen, &sectionPtr);
    require_action(err != kNoErr, exit, err = -1);

    src = "v=0\r\n"
          "o=iTunes 3208725422 0 IN IP4 24.1.2.49\r\n"
          "s=iTunes\r\n"
          "c=IN IP4 24.1.2.50\r\n"
          "t=0 0\r\n"
          "m=audio 0 RTP/AVP 96\r\n"
          "a=rtpmap:96 AppleLossless\r\n"
          "a=fmtp:96 352 0 16 40 10 14 2 255 0 0 44100\r\n"
          "a=rsaaeskey:QZhfrCYoVv9zy3vFe+Gpmg2rH07cDhcJ743gs9/YcffLpm+N1YEhw5HJAAvBy+v6KXr4jEAHc5WISooOUKpocSEfIT35h6C1dZMTROKrNdgT0dOFGlraTrIqeKxq3/jdqhtuy/3J684xpUCvhFV8N4Fgi5Ds6gB+W2Nl95wOGn1uhFwkczdZCQvE0gcZnqBV4DQc+ls1pg2C8oFoIdeq6NjR0Y3HUTNV6jqMeQGBgRzsBQlFGDhbnQUHFHD3s3iGIFwT0NCLNUQCzyoviAfpnJA1ISXN2+7Gi614UEX04KuvKYrdCy9sE7Mz6qNRDu5lfbJueDuzhyc/eIGNpnyprQ\r\n"
          "a=aesiv:UwYG++Ge6wzmMgYdMjfqEg\r\n";
    end = src + strlen(src);

    sectionPtr = NULL;
    sectionEnd = NULL;
    err = SDPFindSessionSection(src, end, &sectionPtr, &sectionEnd, &ptr);
    require_noerr(err, exit);
    require_action(strncmp_prefix(sectionPtr, (size_t)(sectionEnd - sectionPtr), "v=0\r\n") == 0, exit, err = -1);
    require_action(strncmp_prefix(sectionEnd, (size_t)(end - sectionPtr), "m=audio") == 0, exit, err = -1);
    require_action(strncmp_prefix(ptr, (size_t)(end - ptr), "m=audio") == 0, exit, err = -1);

    mediaSectionPtr = NULL;
    mediaSectionEnd = NULL;
    err = SDPFindMediaSection(src, end, &mediaSectionPtr, &mediaSectionEnd, &valuePtr, &valueLen, &ptr);
    require_noerr(err, exit);

    err = SDPFindAttribute(mediaSectionPtr, mediaSectionEnd, "rtpmap", &valuePtr, &valueLen, NULL);
    require_noerr(err, exit);
    require_action(strncmpx(valuePtr, valueLen, "96 AppleLossless") == 0, exit, err = kResponseErr);
    require_action(ptr == end, exit, err = kResponseErr);

    i = 0;
    for (ptr = src; SDPGetNext(ptr, end, &type, &valuePtr, &valueLen, &ptr) == kNoErr;) {
        ++i;
        if (i == 2) {
            require_action(type == 'o', exit, err = kResponseErr);
            require_action(strncmpx(valuePtr, valueLen, "iTunes 3208725422 0 IN IP4 24.1.2.49") == 0, exit, err = kResponseErr);
        }
        if (i == 10) {
            require_action(type == 'a', exit, err = kResponseErr);
            require_action(strncmpx(valuePtr, valueLen, "aesiv:UwYG++Ge6wzmMgYdMjfqEg") == 0, exit, err = kResponseErr);
        }
    }
    require_action(i == 10, exit, err = kResponseErr);

    err = SDPFindAttribute(src, end, "rtpmap", &valuePtr, &valueLen, NULL);
    require_noerr(err, exit);
    require_action(strncmpx(valuePtr, valueLen, "96 AppleLossless") == 0, exit, err = kResponseErr);

    // 2

    src = "m=audio 49230 RTP/AVP 96\r\n"
          "a=rtpmap:96 mpeg4-generic/48000/6\r\n"
          "a=fmtp:96 streamtype=5; profile-level-id=16; mode=AAC-hbr; config=11B0; sizeLength=13; indexLength=3;"
          "indexDeltaLength=3; constantDuration=1024\r\n";
    end = src + strlen(src);

    err = SDPFindAttribute(src, end, "fmtp", &valuePtr, &valueLen, NULL);
    require_noerr(err, exit);
    require_action(strncmp_prefix(valuePtr, valueLen, "96 streamtype") == 0, exit, err = kResponseErr);
    valuePtr += 3;
    valueLen -= 3;

    err = SDPFindParameter(valuePtr, valuePtr + valueLen, "config", &ptr, &len, NULL);
    require_noerr(err, exit);
    require_action(strncmpx(ptr, len, "11B0") == 0, exit, err = -1);

    // 3

    src = "m=audio 21010 RTP/AVP 5\r\n"
          "c=IN IP4 224.0.1.11/127\r\n"
          "a=ptime:40\r\n"
          "m=video 61010/2 RTP/AVP 31 high\r\n"
          "c=IN IP4 224.0.1.12/127\r\n";
    end = src + strlen(src);

    err = SDPFindMediaSectionEx(src, end, &mediaTypePtr, &mediaTypeLen, &portPtr, &portLen, &protocolPtr, &protocolLen,
        &formatPtr, &formatLen, &sectionPtr, &sectionLen, &src);
    require_noerr(err, exit);
    require_action(strncmpx(mediaTypePtr, mediaTypeLen, "audio") == 0, exit, err = -1);
    require_action(strncmpx(portPtr, portLen, "21010") == 0, exit, err = -1);
    require_action(strncmpx(protocolPtr, protocolLen, "RTP/AVP") == 0, exit, err = -1);
    require_action(strncmpx(formatPtr, formatLen, "5") == 0, exit, err = -1);
    require_action(strncmp_prefix(sectionPtr, sectionLen, "c=IN IP4 224.0.1.11/127") == 0, exit, err = -1);
    require_action((sectionPtr + sectionLen) == src, exit, err = -1);
    require_action(strncmp_prefix(src, (size_t)(end - src), "m=video") == 0, exit, err = -1);

    err = SDPFindMediaSectionEx(src, end, &mediaTypePtr, &mediaTypeLen, &portPtr, &portLen, &protocolPtr, &protocolLen,
        &formatPtr, &formatLen, &sectionPtr, &sectionLen, &src);
    require_noerr(err, exit);
    require_action(strncmpx(mediaTypePtr, mediaTypeLen, "video") == 0, exit, err = -1);
    require_action(strncmpx(portPtr, portLen, "61010/2") == 0, exit, err = -1);
    require_action(strncmpx(protocolPtr, protocolLen, "RTP/AVP") == 0, exit, err = -1);
    require_action(strncmpx(formatPtr, formatLen, "31 high") == 0, exit, err = -1);
    require_action(strncmp_prefix(sectionPtr, sectionLen, "c=IN IP4 224.0.1.12/127") == 0, exit, err = -1);
    require_action((sectionPtr + sectionLen) == src, exit, err = -1);
    require_action(src == end, exit, err = -1);

    err = SDPFindMediaSectionEx(src, end, &mediaTypePtr, &mediaTypeLen, &portPtr, &portLen, &protocolPtr, &protocolLen,
        &formatPtr, &formatLen, &sectionPtr, &sectionLen, &src);
    require_action(err != kNoErr, exit, err = -1);

    // 4

    src = "m=audio 21010 RTP/AVP 5\r\n"
          "c=IN IP4 224.0.1.11/127\r\n"
          "a=ptime:40\r\n"
          "m=video 61010/2 RTP/AVP 31 high\r\n"
          "c=IN IP4 224.0.1.12/127\r\n";
    end = src + strlen(src);

    n = SDPScanFAttribute(src, end, NULL, "ptime", "%d", &i);
    require_action(n == 1, exit, err = -1);
    require_action(i == 40, exit, err = -1);

    err = kNoErr;

exit:
    dlog(kLogLevelMax, "\n%###s: %s\n", __ROUTINE__, !err ? "PASSED" : "FAILED");
    return (err);
}
#endif // DEBUG && !EXCLUDE_UNIT_TESTS
