/*
	Copyright (C) 2007-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "URLUtils.h"

#include "CommonServices.h"
#include "DebugServices.h"
#include "StringUtils.h"

#if (TARGET_HAS_STD_C_LIB)
#include <stddef.h>
#include <string.h>
#endif

#if 0
#pragma mark == Encoding/Decoding ==
#endif

//===========================================================================================================================
//	URLDecodeEx
//===========================================================================================================================

OSStatus
URLDecodeEx(
    const void* inEncodedData,
    size_t inEncodedSize,
    void* inDecodedDataBuffer,
    size_t inDecodedDataBufferSize,
    size_t* outDecodedSize,
    int* outChanges)
{
    OSStatus err;
    const uint8_t* src;
    const uint8_t* end;
    uint8_t* dst;
    uint8_t* lim;
    uint8_t c1;
    uint8_t c2;
    int changes;

    if (inEncodedSize == kSizeCString)
        inEncodedSize = strlen((const char*)inEncodedData);
    src = (const uint8_t*)inEncodedData;
    end = src + inEncodedSize;
    dst = (uint8_t*)inDecodedDataBuffer;
    lim = dst + inDecodedDataBufferSize;

    changes = 0;
    while (src < end) {
        c1 = *src++;
        if (c1 == '%') {
            require_action_quiet((end - src) >= 2, exit, err = kUnderrunErr);

            c1 = *src++;
            if ((c1 >= '0') && (c1 <= '9'))
                c1 = c1 - '0';
            else if ((c1 >= 'a') && (c1 <= 'f'))
                c1 = 10 + (c1 - 'a');
            else if ((c1 >= 'A') && (c1 <= 'F'))
                c1 = 10 + (c1 - 'A');
            else {
                err = kMalformedErr;
                goto exit;
            }

            c2 = *src++;
            if ((c2 >= '0') && (c2 <= '9'))
                c2 = c2 - '0';
            else if ((c2 >= 'a') && (c2 <= 'f'))
                c2 = 10 + (c2 - 'a');
            else if ((c2 >= 'A') && (c2 <= 'F'))
                c2 = 10 + (c2 - 'A');
            else {
                err = kMalformedErr;
                goto exit;
            }

            c1 = (uint8_t)((c1 << 4) | c2);
            ++changes;
        } else if (c1 == '+') {
            c1 = ' '; // Some application/x-www-form-urlencoded may use '+' instead of %20 for spaces.
            ++changes;
        }

        if (inDecodedDataBuffer) {
            require_action_quiet(dst < lim, exit, err = kOverrunErr);
            *dst = c1;
        }
        ++dst;
    }
    err = kNoErr;

exit:
    if (outDecodedSize)
        *outDecodedSize = (size_t)(dst - ((uint8_t*)inDecodedDataBuffer));
    if (outChanges)
        *outChanges = changes;
    return (err);
}

//===========================================================================================================================
//	URLDecodeCopy
//===========================================================================================================================

OSStatus URLDecodeCopy(const void* inEncodedData, size_t inEncodedSize, void* outDecodedPtr, size_t* outDecodedSize)
{
    OSStatus err;
    size_t decodedSize;
    char* decodedData;

    decodedData = NULL;

    if (inEncodedSize == kSizeCString)
        inEncodedSize = strlen((const char*)inEncodedData);

    err = URLDecode(inEncodedData, inEncodedSize, NULL, 0, &decodedSize);
    require_noerr_quiet(err, exit);

    decodedData = (char*)malloc(decodedSize + 1);
    require_action(decodedData, exit, err = kNoMemoryErr);

    err = URLDecode(inEncodedData, inEncodedSize, decodedData, decodedSize, &decodedSize);
    require_noerr_quiet(err, exit);
    decodedData[decodedSize] = '\0';

    *((void**)outDecodedPtr) = decodedData;
    if (outDecodedSize)
        *outDecodedSize = decodedSize;
    decodedData = NULL;

exit:
    if (decodedData)
        free(decodedData);
    return (err);
}

#if 0
#pragma mark -
#pragma mark == Misc ==
#endif

//===========================================================================================================================
//	URLParseComponents
//===========================================================================================================================

OSStatus URLParseComponents(const char* inSrc, const char* inEnd, URLComponents* outComponents, const char** outSrc)
{
    const char* ptr;
    const char* schemePtr;
    const char* schemeEnd;
    const char* userPtr;
    const char* userEnd;
    const char* passwordPtr;
    const char* passwordEnd;
    const char* hostPtr;
    const char* hostEnd;
    const char* pathPtr;
    const char* pathEnd;
    const char* queryPtr;
    const char* queryEnd;
    const char* fragmentPtr;
    const char* fragmentEnd;
    char c;

    /*
		URL breakdown from RFC 3986.
	
		 foo://example.com:8042/over/there?name=ferret#nose
		 \_/   \______________/\_________/ \_________/ \__/
		  |           |            |            |        |
		scheme    authority       path        query   fragment
		  |   _____________________|__
		 / \ /                        \ 
		 urn:example:animal:ferret:nose
	*/

    if (inEnd == NULL)
        inEnd = inSrc + strlen(inSrc);

    // Parse an optional scheme (the "ftp" in "ftp://tom:secret@abc.com/test?x#y").

    schemePtr = NULL;
    schemeEnd = NULL;

    c = '\0';
    ptr = inSrc;
    while ((ptr < inEnd) && ((c = *ptr) != ':') && (c != '/') && (c != '?') && (c != '#'))
        ++ptr;
    if (c == ':') {
        schemePtr = inSrc;
        schemeEnd = ptr;
        inSrc = ptr + 1;
    }

    // Parse an optional authority (the "tom:secret@abc.com" in "ftp://tom:secret@abc.com/test?x#y").

    userPtr = NULL;
    userEnd = NULL;
    passwordPtr = NULL;
    passwordEnd = NULL;
    hostPtr = NULL;
    hostEnd = NULL;

    if (((inEnd - inSrc) >= 2) && (inSrc[0] == '/') && (inSrc[1] == '/')) {
        const char* authorityPtr;
        const char* authorityEnd;
        const char* userInfoPtr;
        const char* userInfoEnd;

        inSrc += 2;
        authorityPtr = inSrc;
        while ((inSrc < inEnd) && ((c = *inSrc) != '/') && (c != '?') && (c != '#'))
            ++inSrc;
        authorityEnd = inSrc;

        // Parse an optional userinfo (the "tom:secret" in the above URL).

        userInfoPtr = authorityPtr;
        userInfoEnd = userInfoPtr;
        while ((userInfoEnd < authorityEnd) && (*userInfoEnd != '@'))
            ++userInfoEnd;
        if (userInfoEnd < authorityEnd) {
            // Parse the username (the "tom" in the above URL).

            userPtr = userInfoPtr;
            userEnd = userPtr;
            while ((userEnd < userInfoEnd) && (*userEnd != ':'))
                ++userEnd;
            if (userEnd < userInfoEnd) {
                // The rest is password/auth info. Note: passwords in URLs are deprecated (see RFC 3986 section 3.2.1).

                passwordPtr = userEnd + 1;
                passwordEnd = userInfoEnd;
            }

            // The host is the rest of the authority (the "abc.com" in "ftp://tom:secret@abc.com/test?x#y").

            hostPtr = userInfoEnd + 1;
            hostEnd = authorityEnd;
        } else {
            // The host is the entire authority (the "abc.com" in "ftp://tom:secret@abc.com/test?x#y").

            hostPtr = authorityPtr;
            hostEnd = authorityEnd;
        }
    }

    // Parse the path (the "/test" in "ftp://tom:secret@abc.com/test?x#y").

    c = '\0';
    pathPtr = inSrc;
    while ((inSrc < inEnd) && ((c = *inSrc) != '?') && (c != '#'))
        ++inSrc;
    pathEnd = inSrc;

    // Parse an optional query (the "x" in "ftp://tom:secret@abc.com/test?x#y").

    queryPtr = NULL;
    queryEnd = NULL;
    if (c == '?') {
        queryPtr = ++inSrc;
        while ((inSrc < inEnd) && ((c = *inSrc) != '#'))
            ++inSrc;
        queryEnd = inSrc;
    }

    // Parse an optional fragment  (the "y" in "ftp://tom:secret@abc.com/test?x#y").

    fragmentPtr = NULL;
    fragmentEnd = NULL;
    if (c == '#') {
        fragmentPtr = ++inSrc;
        fragmentEnd = inEnd;
        inSrc = inEnd;
    }

    outComponents->schemePtr = schemePtr;
    outComponents->schemeLen = (size_t)(schemeEnd - schemePtr);
    outComponents->userPtr = userPtr;
    outComponents->userLen = (size_t)(userEnd - userPtr);
    outComponents->passwordPtr = passwordPtr;
    outComponents->passwordLen = (size_t)(passwordEnd - passwordPtr);
    outComponents->hostPtr = hostPtr;
    outComponents->hostLen = (size_t)(hostEnd - hostPtr);
    outComponents->pathPtr = pathPtr;
    outComponents->pathLen = (size_t)(pathEnd - pathPtr);
    outComponents->queryPtr = queryPtr;
    outComponents->queryLen = (size_t)(queryEnd - queryPtr);
    outComponents->fragmentPtr = fragmentPtr;
    outComponents->fragmentLen = (size_t)(fragmentEnd - fragmentPtr);
    outComponents->segmentPtr = ((pathPtr < pathEnd) && (*pathPtr == '/')) ? (pathPtr + 1) : pathPtr;
    outComponents->segmentEnd = pathEnd;
    if (outSrc)
        *outSrc = inSrc;
    return (kNoErr);
}

//===========================================================================================================================
//	URLGetOrCopyNextVariable
//===========================================================================================================================

OSStatus
URLGetOrCopyNextVariable(
    const char* inSrc,
    const char* inEnd,
    const char** outNamePtr,
    size_t* outNameLen,
    char** outNameStorage,
    const char** outValuePtr,
    size_t* outValueLen,
    char** outValueStorage,
    const char** outSrc)
{
    OSStatus err;
    const char* namePtr;
    size_t nameLen;
    char* nameStorage;
    const char* valuePtr;
    size_t valueLen;
    char* valueStorage;
    int changes;

    nameStorage = NULL;
    valueStorage = NULL;

    err = URLGetNextVariable(inSrc, inEnd, &namePtr, &nameLen, &valuePtr, &valueLen, outSrc);
    require_noerr_quiet(err, exit);

    err = URLDecodeEx(namePtr, nameLen, NULL, 0, NULL, &changes);
    require_noerr(err, exit);
    if (changes > 0) {
        err = URLDecodeCopy(namePtr, nameLen, &nameStorage, &nameLen);
        require_noerr(err, exit);
        namePtr = nameStorage;
    }

    if (outValueStorage) {
        err = URLDecodeEx(valuePtr, valueLen, NULL, 0, NULL, &changes);
        require_noerr(err, exit);
        if (changes > 0) {
            err = URLDecodeCopy(valuePtr, valueLen, &valueStorage, &valueLen);
            require_noerr(err, exit);
            valuePtr = valueStorage;
        }
    }

    *outNamePtr = namePtr;
    *outNameLen = nameLen;
    *outNameStorage = nameStorage;
    nameStorage = NULL;

    if (outValuePtr)
        *outValuePtr = valuePtr;
    if (outValueLen)
        *outValueLen = valueLen;
    if (outValueStorage) {
        *outValueStorage = valueStorage;
        valueStorage = NULL;
    }

exit:
    if (nameStorage)
        free(nameStorage);
    if (valueStorage)
        free(valueStorage);
    return (err);
}

//===========================================================================================================================
//	URLGetNextVariable
//===========================================================================================================================

OSStatus
URLGetNextVariable(
    const char* inSrc,
    const char* inEnd,
    const char** outNamePtr,
    size_t* outNameLen,
    const char** outValuePtr,
    size_t* outValueLen,
    const char** outSrc)
{
    OSStatus err;
    char c;
    const char* namePtr;
    const char* nameEnd;
    const char* valuePtr;
    const char* valueEnd;

    require_action_quiet(inSrc < inEnd, exit, err = kNotFoundErr);

    // Variables are in the form: "name1=value1&name2=value2"

    c = '\0';
    namePtr = inSrc;
    while ((inSrc < inEnd) && ((c = *inSrc) != '=') && (c != '&'))
        ++inSrc;
    nameEnd = inSrc;
    if (inSrc < inEnd)
        ++inSrc;

    if (c == '=') {
        valuePtr = inSrc;
        while ((inSrc < inEnd) && (*inSrc != '&'))
            ++inSrc;
        valueEnd = inSrc;
        if (inSrc < inEnd)
            ++inSrc;
    } else {
        valuePtr = NULL;
        valueEnd = NULL;
    }

    *outNamePtr = namePtr;
    *outNameLen = (size_t)(nameEnd - namePtr);
    *outValuePtr = valuePtr;
    *outValueLen = (size_t)(valueEnd - valuePtr);
    *outSrc = inSrc;
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	URLGetOrCopyVariable
//===========================================================================================================================

OSStatus
URLGetOrCopyVariable(
    const char* inSrc,
    const char* inEnd,
    const char* inName,
    const char** outValuePtr,
    size_t* outValueLen,
    char** outValueStorage,
    const char** outSrc)
{
    OSStatus err;
    const char* namePtr = NULL;
    size_t nameLen = 0;
    char* nameStorage = NULL;
    const char* valuePtr = NULL;
    size_t valueLen;
    char* valueStorage;
    int match;
    int changes;

    while ((err = URLGetOrCopyNextVariable(inSrc, inEnd,
                &namePtr, &nameLen, &nameStorage,
                &valuePtr, &valueLen, NULL,
                &inSrc))
        == kNoErr) {
        match = (strncmpx(namePtr, nameLen, inName) == 0);
        if (nameStorage)
            free(nameStorage);
        if (!match)
            continue;

        valueStorage = NULL;
        if (outValueStorage) {
            err = URLDecodeEx(valuePtr, valueLen, NULL, 0, NULL, &changes);
            require_noerr(err, exit);
            if (changes > 0) {
                err = URLDecodeCopy(valuePtr, valueLen, &valueStorage, &valueLen);
                require_noerr(err, exit);
                valuePtr = valueStorage;
            }
        }

        if (outValuePtr)
            *outValuePtr = valuePtr;
        if (outValueLen)
            *outValueLen = valueLen;
        if (outValueStorage)
            *outValueStorage = valueStorage;
        if (outSrc)
            *outSrc = inSrc;
        return (kNoErr);
    }

exit:
    return (err);
}

//===========================================================================================================================
//	URLGetVariable
//===========================================================================================================================

OSStatus
URLGetVariable(
    const char* inSrc,
    const char* inEnd,
    const char* inName,
    const char** outValuePtr,
    size_t* outValueLen,
    const char** outSrc)
{
    OSStatus err;
    const char* namePtr;
    size_t nameLen;
    const char* valuePtr;
    size_t valueLen;

    namePtr = NULL;
    nameLen = 0;
    valuePtr = NULL;
    valueLen = 0;
    while ((err = URLGetNextVariable(inSrc, inEnd, &namePtr, &nameLen, &valuePtr, &valueLen, &inSrc)) == kNoErr) {
        if (strncmpx(namePtr, nameLen, inName) == 0) {
            if (outValuePtr)
                *outValuePtr = valuePtr;
            if (outValueLen)
                *outValueLen = valueLen;
            if (outSrc)
                *outSrc = inSrc;
            return (kNoErr);
        }
    }
    return (err);
}

//===========================================================================================================================
//    URLGetNextPathSegment
//===========================================================================================================================
OSStatus URLGetNextPathSegment(URLComponents* inComps, const char** outSegmentPtr, size_t* outSegmentLen)
{
    const char* src;
    const char* ptr;
    const char* end;

    src = inComps->segmentPtr;
    end = inComps->segmentEnd;
    for (ptr = src; (ptr < end) && (*ptr != '/'); ++ptr) {
    }

    if (ptr != src) {
        *outSegmentPtr = src;
        *outSegmentLen = (size_t)(ptr - src);
        inComps->segmentPtr = (ptr < end) ? (ptr + 1) : ptr;
        return (kNoErr);
    }
    return (kNotFoundErr);
}

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if (!EXCLUDE_UNIT_TESTS)
//===========================================================================================================================
//	URLUtils_Test
//===========================================================================================================================

OSStatus URLUtils_Test(void)
{
    OSStatus err;
    char buf[256];
    size_t len;
    const char* src;
    const char* end;
    const char* ptr;
    URLComponents urlComps;
    const char* namePtr;
    size_t nameLen;
    char* nameStorage;
    const char* valuePtr;
    size_t valueLen;
    char* valueStorage;
    int i;
    char* str;

    // Decoding

    src = "This%20That";
    len = strlen(src);
    err = URLDecode(src, len, buf, sizeof(buf), &len);
    require_noerr(err, exit);
    require_action(strncmpx(buf, len, "This That") == 0, exit, err = kResponseErr);

    src = "0017F2F790F0%40abc's%20%EF%A3%BFtv";
    len = strlen(src);
    err = URLDecode(src, len, buf, sizeof(buf), &len);
    require_noerr(err, exit);
    require_action(strncmpx(buf, len, "0017F2F790F0@abc's \xEF\xA3\xBFtv") == 0, exit, err = kResponseErr);

    str = NULL;
    err = URLDecodeCopy("0017F2F790F0%40abc's%20%EF%A3%BFtv", kSizeCString, &str, &len);
    require_noerr(err, exit);
    require_action(strcmp(str, "0017F2F790F0@abc's \xEF\xA3\xBFtv") == 0, exit, err = kResponseErr);
    require_action(len == strlen(str), exit, err = kResponseErr);
    free(str);

    // Parsing

    src = "ftp://tom:secret@abc.com/test?x#y";
    end = src + strlen(src);
    err = URLParseComponents(src, end, &urlComps, &ptr);
    require_noerr(err, exit);
    require_action(strncmpx(urlComps.schemePtr, urlComps.schemeLen, "ftp") == 0, exit, err = kResponseErr);
    require_action(strncmpx(urlComps.userPtr, urlComps.userLen, "tom") == 0, exit, err = kResponseErr);
    require_action(strncmpx(urlComps.passwordPtr, urlComps.passwordLen, "secret") == 0, exit, err = kResponseErr);
    require_action(strncmpx(urlComps.hostPtr, urlComps.hostLen, "abc.com") == 0, exit, err = kResponseErr);
    require_action(strncmpx(urlComps.pathPtr, urlComps.pathLen, "/test") == 0, exit, err = kResponseErr);
    require_action(strncmpx(urlComps.queryPtr, urlComps.queryLen, "x") == 0, exit, err = kResponseErr);
    require_action(strncmpx(urlComps.fragmentPtr, urlComps.fragmentLen, "y") == 0, exit, err = kResponseErr);
    require_action(ptr == end, exit, err = kResponseErr);

    src = "http://www.ics.uci.edu/pub/ietf/uri/#Related";
    end = src + strlen(src);
    err = URLParseComponents(src, end, &urlComps, &ptr);
    require_noerr(err, exit);
    require_action(strncmpx(urlComps.schemePtr, urlComps.schemeLen, "http") == 0, exit, err = kResponseErr);
    require_action((urlComps.userPtr == NULL) && (urlComps.userLen == 0), exit, err = kResponseErr);
    require_action((urlComps.passwordPtr == NULL) && (urlComps.passwordLen == 0), exit, err = kResponseErr);
    require_action(strncmpx(urlComps.hostPtr, urlComps.hostLen, "www.ics.uci.edu") == 0, exit, err = kResponseErr);
    require_action(strncmpx(urlComps.pathPtr, urlComps.pathLen, "/pub/ietf/uri/") == 0, exit, err = kResponseErr);
    require_action((urlComps.queryPtr == NULL) && (urlComps.queryLen == 0), exit, err = kResponseErr);
    require_action(strncmpx(urlComps.fragmentPtr, urlComps.fragmentLen, "Related") == 0, exit, err = kResponseErr);
    require_action(ptr == end, exit, err = kResponseErr);

    src = "urn:example:animal:ferret:nose";
    end = src + strlen(src);
    err = URLParseComponents(src, end, &urlComps, &ptr);
    require_noerr(err, exit);
    require_action(strncmpx(urlComps.schemePtr, urlComps.schemeLen, "urn") == 0, exit, err = kResponseErr);
    require_action((urlComps.userPtr == NULL) && (urlComps.userLen == 0), exit, err = kResponseErr);
    require_action((urlComps.passwordPtr == NULL) && (urlComps.passwordLen == 0), exit, err = kResponseErr);
    require_action((urlComps.hostPtr == NULL) && (urlComps.hostLen == 0), exit, err = kResponseErr);
    require_action(strncmpx(urlComps.pathPtr, urlComps.pathLen, "example:animal:ferret:nose") == 0, exit, err = kResponseErr);
    require_action((urlComps.queryPtr == NULL) && (urlComps.queryLen == 0), exit, err = kResponseErr);
    require_action((urlComps.fragmentPtr == NULL) && (urlComps.fragmentLen == 0), exit, err = kResponseErr);
    require_action(ptr == end, exit, err = kResponseErr);

    src = "http://joe:secret@www.host.com:123/folder/page.html?cgi-bin123#cool";
    end = src + strlen(src);
    err = URLParseComponents(src, end, &urlComps, &ptr);
    require_noerr(err, exit);
    require_action(strncmpx(urlComps.schemePtr, urlComps.schemeLen, "http") == 0, exit, err = kResponseErr);
    require_action(strncmpx(urlComps.userPtr, urlComps.userLen, "joe") == 0, exit, err = kResponseErr);
    require_action(strncmpx(urlComps.passwordPtr, urlComps.passwordLen, "secret") == 0, exit, err = kResponseErr);
    require_action(strncmpx(urlComps.hostPtr, urlComps.hostLen, "www.host.com:123") == 0, exit, err = kResponseErr);
    require_action(strncmpx(urlComps.pathPtr, urlComps.pathLen, "/folder/page.html") == 0, exit, err = kResponseErr);
    require_action(strncmpx(urlComps.queryPtr, urlComps.queryLen, "cgi-bin123") == 0, exit, err = kResponseErr);
    require_action(strncmpx(urlComps.fragmentPtr, urlComps.fragmentLen, "cool") == 0, exit, err = kResponseErr);
    require_action(ptr == end, exit, err = kResponseErr);

    src = "*";
    end = src + strlen(src);
    err = URLParseComponents(src, end, &urlComps, &ptr);
    require_noerr(err, exit);
    require_action((urlComps.schemePtr == NULL) && (urlComps.schemeLen == 0), exit, err = kResponseErr);
    require_action((urlComps.userPtr == NULL) && (urlComps.userLen == 0), exit, err = kResponseErr);
    require_action((urlComps.passwordPtr == NULL) && (urlComps.passwordLen == 0), exit, err = kResponseErr);
    require_action((urlComps.hostPtr == NULL) && (urlComps.hostLen == 0), exit, err = kResponseErr);
    require_action(strncmpx(urlComps.pathPtr, urlComps.pathLen, "*") == 0, exit, err = kResponseErr);
    require_action((urlComps.queryPtr == NULL) && (urlComps.queryLen == 0), exit, err = kResponseErr);
    require_action((urlComps.fragmentPtr == NULL) && (urlComps.fragmentLen == 0), exit, err = kResponseErr);
    require_action(ptr == end, exit, err = kResponseErr);

    src = "mailto:frank@wwdcdemo.example.com";
    end = src + strlen(src);
    err = URLParseComponents(src, end, &urlComps, &ptr);
    require_noerr(err, exit);
    require_action(strncmpx(urlComps.schemePtr, urlComps.schemeLen, "mailto") == 0, exit, err = kResponseErr);
    require_action((urlComps.userPtr == NULL) && (urlComps.userLen == 0), exit, err = kResponseErr);
    require_action((urlComps.passwordPtr == NULL) && (urlComps.passwordLen == 0), exit, err = kResponseErr);
    require_action((urlComps.hostPtr == NULL) && (urlComps.hostLen == 0), exit, err = kResponseErr);
    require_action(strncmpx(urlComps.pathPtr, urlComps.pathLen, "frank@wwdcdemo.example.com") == 0, exit, err = kResponseErr);
    require_action((urlComps.queryPtr == NULL) && (urlComps.queryLen == 0), exit, err = kResponseErr);
    require_action((urlComps.fragmentPtr == NULL) && (urlComps.fragmentLen == 0), exit, err = kResponseErr);
    require_action(ptr == end, exit, err = kResponseErr);

    src = "file:///foo;bar";
    end = src + strlen(src);
    err = URLParseComponents(src, end, &urlComps, &ptr);
    require_noerr(err, exit);
    require_action(strncmpx(urlComps.schemePtr, urlComps.schemeLen, "file") == 0, exit, err = kResponseErr);
    require_action((urlComps.userPtr == NULL) && (urlComps.userLen == 0), exit, err = kResponseErr);
    require_action((urlComps.passwordPtr == NULL) && (urlComps.passwordLen == 0), exit, err = kResponseErr);
    require_action(urlComps.hostLen == 0, exit, err = kResponseErr);
    require_action(strncmpx(urlComps.pathPtr, urlComps.pathLen, "/foo;bar") == 0, exit, err = kResponseErr);
    require_action((urlComps.queryPtr == NULL) && (urlComps.queryLen == 0), exit, err = kResponseErr);
    require_action((urlComps.fragmentPtr == NULL) && (urlComps.fragmentLen == 0), exit, err = kResponseErr);
    require_action(ptr == end, exit, err = kResponseErr);

    // Query variables.

    src = "field1=value1&field2=value2&field3=value3";
    end = src + strlen(src);
    i = 0;
    while (URLGetNextVariable(src, end, &namePtr, &nameLen, &valuePtr, &valueLen, &src) == kNoErr) {
        ++i;
        if (i == 1) {
            require_action(strncmpx(namePtr, nameLen, "field1") == 0, exit, err = kResponseErr);
            require_action(strncmpx(valuePtr, valueLen, "value1") == 0, exit, err = kResponseErr);
        } else if (i == 2) {
            require_action(strncmpx(namePtr, nameLen, "field2") == 0, exit, err = kResponseErr);
            require_action(strncmpx(valuePtr, valueLen, "value2") == 0, exit, err = kResponseErr);
        } else if (i == 3) {
            require_action(strncmpx(namePtr, nameLen, "field3") == 0, exit, err = kResponseErr);
            require_action(strncmpx(valuePtr, valueLen, "value3") == 0, exit, err = kResponseErr);
        }
    }
    require_action(i == 3, exit, err = kResponseErr);
    require_action(src == end, exit, err = kResponseErr);

    src = "field1=value1&field2=value2&field3=value3";
    end = src + strlen(src);
    err = URLGetVariable(src, end, "field2", &valuePtr, &valueLen, NULL);
    require_noerr(err, exit);
    require_action(strncmpx(valuePtr, valueLen, "value2") == 0, exit, err = kResponseErr);

    // Encoded Query Variable Iteration.

    src = "field1=value1&field+2=value%202&field%203=value3";
    end = src + strlen(src);
    i = 0;
    while (URLGetOrCopyNextVariable(src, end, &namePtr, &nameLen, &nameStorage,
               &valuePtr, &valueLen, &valueStorage, &src)
        == kNoErr) {
        ++i;
        if (i == 1) {
            require_action(strncmpx(namePtr, nameLen, "field1") == 0, exit, err = kResponseErr);
            require_action(nameStorage == NULL, exit, err = kResponseErr);
            require_action(strncmpx(valuePtr, valueLen, "value1") == 0, exit, err = kResponseErr);
            require_action(valueStorage == NULL, exit, err = kResponseErr);
        } else if (i == 2) {
            require_action(strncmpx(namePtr, nameLen, "field 2") == 0, exit, err = kResponseErr);
            require_action(nameStorage, exit, err = kResponseErr);
            require_action(strncmpx(valuePtr, valueLen, "value 2") == 0, exit, err = kResponseErr);
            require_action(valueStorage, exit, err = kResponseErr);
            free(nameStorage);
            free(valueStorage);
        } else if (i == 3) {
            require_action(strncmpx(namePtr, nameLen, "field 3") == 0, exit, err = kResponseErr);
            require_action(nameStorage, exit, err = kResponseErr);
            require_action(strncmpx(valuePtr, valueLen, "value3") == 0, exit, err = kResponseErr);
            require_action(valueStorage == NULL, exit, err = kResponseErr);
            free(nameStorage);
        }
    }
    require_action(i == 3, exit, err = kResponseErr);
    require_action(src == end, exit, err = kResponseErr);

    // Encoded Query Variable Extraction.

    src = "field1=value1&field+2=value%202&field%203=value3";
    end = src + strlen(src);

    err = URLGetOrCopyVariable(src, end, "field1", &valuePtr, &valueLen, &valueStorage, NULL);
    require_noerr(err, exit);
    require_action(strncmpx(valuePtr, valueLen, "value1") == 0, exit, err = kResponseErr);
    require_action(valueStorage == NULL, exit, err = kResponseErr);

    err = URLGetOrCopyVariable(src, end, "field 2", &valuePtr, &valueLen, &valueStorage, NULL);
    require_noerr(err, exit);
    require_action(strncmpx(valuePtr, valueLen, "value 2") == 0, exit, err = kResponseErr);
    require_action(valueStorage, exit, err = kResponseErr);
    free(valueStorage);

    err = URLGetOrCopyVariable(src, end, "field 3", &valuePtr, &valueLen, &valueStorage, NULL);
    require_noerr(err, exit);
    require_action(strncmpx(valuePtr, valueLen, "value3") == 0, exit, err = kResponseErr);
    require_action(valueStorage == NULL, exit, err = kResponseErr);

    err = URLGetOrCopyVariable(src, end, "field 4", &valuePtr, &valueLen, &valueStorage, NULL);
    require_action(err != kNoErr, exit, err = -1);

    err = kNoErr;

exit:
    printf("URLUtils_Test: %s\n", !err ? "PASSED" : "FAILED");
    return (err);
}
#endif // !EXCLUDE_UNIT_TESTS
