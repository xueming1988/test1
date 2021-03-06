/*
	Copyright (C) 1997-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

// Microsoft deprecated standard C APIs like fopen so disable those warnings because the replacement APIs are not portable.

#if (!defined(_CRT_SECURE_NO_DEPRECATE))
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include "PrintFUtils.h"

#include "CommonServices.h"
#include "DebugServices.h"
#include "RandomNumberUtils.h"
#include "StringUtils.h"

#if (TARGET_HAS_STD_C_LIB)
#include <time.h>
#endif

#if (TARGET_OS_DARWIN)
#if (!TARGET_KERNEL && !COMMON_SERVICES_NO_CORE_SERVICES)
#include <CoreAudio/CoreAudio.h>
#endif
#endif
#if (TARGET_OS_POSIX)
#include <net/if.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>

#include "MiscUtils.h"
#include "TimeUtils.h"
#endif

#if (DEBUG_CF_OBJECTS_ENABLED)
#include "CFUtils.h"
#endif

#include LIBDISPATCH_HEADER

//===========================================================================================================================
//	Constants
//===========================================================================================================================

// Enables limited %f floating-point output.

#if (!defined(PRINTF_ENABLE_FLOATING_POINT))
#if (!TARGET_OS_WINDOWS_KERNEL)
#define PRINTF_ENABLE_FLOATING_POINT 1
#else
#define PRINTF_ENABLE_FLOATING_POINT 0
#endif
#endif

//===========================================================================================================================
//	Structures
//===========================================================================================================================

static const PrintFFormat kPrintFFormatDefault = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

typedef int (*PrintFCallBack)(const char* inStr, size_t inSize, PrintFContext* inContext);

struct PrintFContext {
    PrintFCallBack callback;
    char* str;
    size_t usedSize;
    size_t reservedSize;

    PrintFUserCallBack userCallBack;
    void* userContext;
};

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static int PrintFWriteAddr(const uint8_t* inAddr, PrintFFormat* inFormat, char* outStr);
static int PrintFWriteBits(uint64_t inX, PrintFFormat* inFormat, char* outStr);
#if (DEBUG_CF_OBJECTS_ENABLED)
static int PrintFWriteCFObject(PrintFContext* inContext, PrintFFormat* inFormat, CFTypeRef inObj, char* inBuffer);
#endif
static int
PrintFWriteHex(
    PrintFContext* inContext,
    PrintFFormat* inFormat,
    int inIndent,
    const void* inData,
    size_t inSize,
    size_t inMaxSize);
static int PrintFWriteHexOneLine(PrintFContext* inContext, PrintFFormat* inFormat, const uint8_t* inData, size_t inSize);
static int PrintFWriteHexByteStream(PrintFContext* inContext, Boolean inUppercase, const uint8_t* inData, size_t inSize);
static int PrintFWriteMultiLineText(PrintFContext* inContext, PrintFFormat* inFormat, const char* inStr, size_t inLen);
static int PrintFWriteString(const char* inStr, PrintFFormat* inFormat, char* inBuf, const char** outStrPtr);
static int PrintFWriteText(PrintFContext* inContext, PrintFFormat* inFormat, const char* inText, size_t inSize);
static int PrintFWriteUnicodeString(const uint8_t* inStr, PrintFFormat* inFormat, char* inBuf);

#define print_indent(CONTEXT, N) PrintFCore((CONTEXT), "%*s", (int)((N)*4), "")

static int PrintFCallBackFixedString(const char* inStr, size_t inSize, PrintFContext* inContext);
static int PrintFCallBackAllocatedString(const char* inStr, size_t inSize, PrintFContext* inContext);
static int PrintFCallBackUserCallBack(const char* inStr, size_t inSize, PrintFContext* inContext);

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	SNPrintF
//===========================================================================================================================

int SNPrintF(void* inBuf, size_t inMaxLen, const char* inFormat, ...)
{
    int n;
    va_list args;

    va_start(args, inFormat);
    n = VSNPrintF(inBuf, inMaxLen, inFormat, args);
    va_end(args);
    return (n);
}

//===========================================================================================================================
//	VSNPrintF
//===========================================================================================================================

int VSNPrintF(void* inBuf, size_t inMaxLen, const char* inFormat, va_list inArgs)
{
    int n;
    PrintFContext context;

    context.callback = PrintFCallBackFixedString;
    context.str = (char*)inBuf;
    context.usedSize = 0;
    context.reservedSize = (inMaxLen > 0) ? inMaxLen - 1 : 0;

    n = PrintFCoreVAList(&context, inFormat, inArgs);
    if (inMaxLen > 0)
        context.str[context.usedSize] = '\0';
    return (n);
}

//===========================================================================================================================
//	SNPrintF_Add
//===========================================================================================================================

#if (!defined(SNPRINTF_USE_ASSERTS))
#define SNPRINTF_USE_ASSERTS 1
#endif

OSStatus SNPrintF_Add(char** ioPtr, char* inEnd, const char* inFormat, ...)
{
    char* const ptr = *ioPtr;
    size_t len;
    int n;
    va_list args;

    len = (size_t)(inEnd - ptr);
#if (SNPRINTF_USE_ASSERTS)
    require_action(len > 0, exit, n = kNoSpaceErr);
#else
    require_action_quiet(len > 0, exit, n = kNoSpaceErr);
#endif

    va_start(args, inFormat);
    n = VSNPrintF(ptr, len, inFormat, args);
    va_end(args);
#if (SNPRINTF_USE_ASSERTS)
    require(n >= 0, exit);
#else
    require_quiet(n >= 0, exit);
#endif
    if (n >= ((int)len)) {
#if (SNPRINTF_USE_ASSERTS)
        dlogassert("Add '%s' format failed due to lack of space (%d vs %zu)", inFormat, n, len);
#endif
        *ioPtr = inEnd;
        n = kOverrunErr;
        goto exit;
    }
    *ioPtr = ptr + n;
    n = kNoErr;

exit:
    return (n);
}

//===========================================================================================================================
//	AppendPrintF
//===========================================================================================================================

int AppendPrintF(char** ioStr, const char* inFormat, ...)
{
    int n;
    va_list args;
    char* tempStr;

    va_start(args, inFormat);
    n = ASPrintF(&tempStr, "%s%V", *ioStr ? *ioStr : "", inFormat, &args);
    va_end(args);
    require_quiet(n >= 0, exit);

    if (*ioStr)
        free(*ioStr);
    *ioStr = tempStr;

exit:
    return (n);
}

//===========================================================================================================================
//	ASPrintF
//===========================================================================================================================

int ASPrintF(char** outStr, const char* inFormat, ...)
{
    int n;
    va_list args;

    va_start(args, inFormat);
    n = VASPrintF(outStr, inFormat, args);
    va_end(args);
    return (n);
}

//===========================================================================================================================
//	VASPrintF
//===========================================================================================================================

int VASPrintF(char** outStr, const char* inFormat, va_list inArgs)
{
    int n;
    PrintFContext context;
    int tmp;

    context.callback = PrintFCallBackAllocatedString;
    context.str = NULL;
    context.usedSize = 0;
    context.reservedSize = 0;

    n = PrintFCoreVAList(&context, inFormat, inArgs);
    if (n >= 0) {
        tmp = context.callback("", 1, &context);
        if (tmp < 0)
            n = tmp;
    }
    if (n >= 0)
        *outStr = context.str;
    else if (context.str)
        free(context.str);
    return (n);
}

//===========================================================================================================================
//	CPrintF
//===========================================================================================================================

int CPrintF(PrintFUserCallBack inCallBack, void* inContext, const char* inFormat, ...)
{
    int n;
    va_list args;

    va_start(args, inFormat);
    n = VCPrintF(inCallBack, inContext, inFormat, args);
    va_end(args);
    return (n);
}

//===========================================================================================================================
//	VCPrintF
//===========================================================================================================================

int VCPrintF(PrintFUserCallBack inCallBack, void* inContext, const char* inFormat, va_list inArgs)
{
    int n;
    PrintFContext context;
    int tmp;

    context.callback = PrintFCallBackUserCallBack;
    context.str = NULL;
    context.usedSize = 0;
    context.reservedSize = 0;
    context.userCallBack = inCallBack;
    context.userContext = inContext;

    n = PrintFCoreVAList(&context, inFormat, inArgs);
    if (n >= 0) {
        tmp = context.callback("", 0, &context);
        if (tmp < 0)
            n = tmp;
    }
    return (n);
}

#if (TARGET_HAS_C_LIB_IO)
//===========================================================================================================================
//	FPrintF
//===========================================================================================================================

int FPrintF(FILE* inFile, const char* inFormat, ...)
{
    int n;
    va_list args;

    va_start(args, inFormat);
    n = VFPrintF(inFile, inFormat, args);
    va_end(args);

    return (n);
}

//===========================================================================================================================
//	VFPrintF
//===========================================================================================================================

static int FPrintFCallBack(const char* inStr, size_t inSize, void* inContext);

int VFPrintF(FILE* inFile, const char* inFormat, va_list inArgs)
{
    return (VCPrintF(FPrintFCallBack, inFile, inFormat, inArgs));
}

//===========================================================================================================================
//	FPrintFCallBack
//===========================================================================================================================

static int FPrintFCallBack(const char* inStr, size_t inSize, void* inContext)
{
    FILE* const file = (FILE*)inContext;

    if (file)
        fwrite(inStr, 1, inSize, file);
    return ((int)inSize);
}
#endif // TARGET_HAS_C_LIB_IO

//===========================================================================================================================
//	MemPrintF
//===========================================================================================================================

int MemPrintF(void* inBuf, size_t inMaxLen, const char* inFormat, ...)
{
    int n;
    va_list args;

    va_start(args, inFormat);
    n = VMemPrintF(inBuf, inMaxLen, inFormat, args);
    va_end(args);
    return (n);
}

//===========================================================================================================================
//	VMemPrintF
//===========================================================================================================================

int VMemPrintF(void* inBuf, size_t inMaxLen, const char* inFormat, va_list inArgs)
{
    int n;
    PrintFContext context;

    context.callback = PrintFCallBackFixedString;
    context.str = (char*)inBuf;
    context.usedSize = 0;
    context.reservedSize = inMaxLen;

    n = PrintFCoreVAList(&context, inFormat, inArgs);
    return (n);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	PrintFCore
//===========================================================================================================================

int PrintFCore(PrintFContext* inContext, const char* inFormat, ...)
{
    int n;
    va_list args;

    va_start(args, inFormat);
    n = PrintFCoreVAList(inContext, inFormat, args);
    va_end(args);
    return (n);
}

//===========================================================================================================================
//	PrintFCoreVAList
//===========================================================================================================================

#define kPrintFBufSize 300 // Enough space for a 256-byte domain name and some error text.

#define PrintFIsPrintable(C) (((C) >= 0x20) && ((C) < 0x7F))
#define PrintFMakePrintable(C) ((char)(PrintFIsPrintable((C)) ? (C) : (C) ? '^' : '.'))

int PrintFCoreVAList(PrintFContext* inContext, const char* inFormat, va_list inArgs)
{
    int total = 0;
    const char* fmt = inFormat;
    PrintFVAList vaArgs;
    const char* src;
    int err;
    char buf[kPrintFBufSize];
    char* p;
    int c;
    PrintFFormat F;
    int i;
    const char* digits;
    const char* s;
    const uint8_t* a;
    int n;
    size_t size;
    size_t sizeMax;
    uint64_t x;
    unsigned int base;
    uint32_t remain;
    unsigned int extra;

#if (defined(va_copy))
    va_copy(vaArgs.args, inArgs);
#else
    vaArgs.args = inArgs; // Not portable and only works on va_list's that are pointers and not arrays.
#endif

    for (c = *fmt;; c = *++fmt) {
        // Non-conversion characters are copied directly to the output.

        src = fmt;
        while ((c != '\0') && (c != '%'))
            c = *++fmt;
        if (fmt != src) {
            i = (int)(fmt - src);
            err = inContext->callback(src, (size_t)i, inContext);
            if (err < 0)
                goto error;
            total += i;
        }
        if (c == '\0')
            break;

        F = kPrintFFormatDefault;

        // Flags

        for (;;) {
            c = *++fmt;
            if (c == '-')
                F.leftJustify = 1;
            else if (c == '+')
                F.forceSign = 1;
            else if (c == ' ')
                F.sign = ' ';
            else if (c == '#')
                F.altForm += 1;
            else if (c == '0')
                F.zeroPad = 1;
            else if (c == '\'')
                F.group += 1;
            else if (c == '?')
                F.suppress = !va_arg(vaArgs.args, int);
            else
                break;
        }

        // Field Width

        if (c == '*') {
            i = va_arg(vaArgs.args, int);
            if (i < 0) {
                i = -i;
                F.leftJustify = 1;
            }
            F.fieldWidth = (unsigned int)i;
            c = *++fmt;
        } else {
            for (; (c >= '0') && (c <= '9'); c = *++fmt) {
                F.fieldWidth = (10 * F.fieldWidth) + (c - '0');
            }
        }

        // Precision

        if (c == '.') {
            c = *++fmt;
            if (c == '*') {
                F.precision = va_arg(vaArgs.args, unsigned int);
                c = *++fmt;
            } else {
                for (; (c >= '0') && (c <= '9'); c = *++fmt) {
                    F.precision = (10 * F.precision) + (c - '0');
                }
            }
            F.havePrecision = 1;
        }
        if (F.leftJustify)
            F.zeroPad = 0;

        // Length modifiers

        for (;;) {
            if (c == 'h') {
                ++F.hSize;
                c = *++fmt;
            } else if (c == 'l') {
                ++F.lSize;
                c = *++fmt;
            } else if (c == 'j') {
                if (F.hSize || F.lSize) {
                    err = -1;
                    goto error;
                }
                // Disable unreachable code warnings because unreachable paths are reachable on some architectures.
                begin_unreachable_code_paths() if (sizeof(intmax_t) == sizeof(long)) F.lSize = 1;
                else if (sizeof(intmax_t) == sizeof(int64_t)) F.lSize = 2;
                else F.lSize = 0;
                end_unreachable_code_paths()
                    c
                    = *++fmt;
                break;
            } else if (c == 'z') {
                if (F.hSize || F.lSize) {
                    err = -1;
                    goto error;
                };
                // Disable unreachable code warnings because unreachable paths are reachable on some architectures.
                begin_unreachable_code_paths() if (sizeof(size_t) == sizeof(long)) F.lSize = 1;
                else if (sizeof(size_t) == sizeof(int64_t)) F.lSize = 2;
                else F.lSize = 0;
                end_unreachable_code_paths()
                    c
                    = *++fmt;
                break;
            } else if (c == 't') {
                if (F.hSize || F.lSize) {
                    err = -1;
                    goto error;
                };
                // Disable unreachable code warnings because unreachable paths are reachable on some architectures.
                begin_unreachable_code_paths() if (sizeof(ptrdiff_t) == sizeof(long)) F.lSize = 1;
                else if (sizeof(ptrdiff_t) == sizeof(int64_t)) F.lSize = 2;
                else F.lSize = 0;
                end_unreachable_code_paths()
                    c
                    = *++fmt;
                break;
            } else
                break;
        }
        if (F.hSize > 2) {
            err = -1;
            goto error;
        };
        if (F.lSize > 2) {
            err = -1;
            goto error;
        };
        if (F.hSize && F.lSize) {
            err = -1;
            goto error;
        };

        // Conversions

        digits = kHexDigitsUppercase;
        switch (c) {
        // %d, %i, %u, %o, %b, %x, %X, %p: Number

        case 'd':
        case 'i':
            base = 10;
            goto canBeSigned;
        case 'u':
            base = 10;
            goto notSigned;
        case 'o':
            base = 8;
            goto notSigned;
        case 'b':
            base = 2;
            goto notSigned;
        case 'x':
            digits = kHexDigitsLowercase;
            FALLTHROUGH;
        case 'X':
            base = 16;
            goto notSigned;
        case 'p':
            x = (uintptr_t)va_arg(vaArgs.args, void*);
            F.precision = sizeof(void*) * 2;
            F.havePrecision = 1;
            F.altForm = 1;
            F.sign = 0;
            base = 16;
            c = 'x';
            goto number;

        canBeSigned:
            if (F.lSize == 1)
                x = (uint64_t)va_arg(vaArgs.args, long);
            else if (F.lSize == 2)
                x = (uint64_t)va_arg(vaArgs.args, int64_t);
            else
                x = (uint64_t)va_arg(vaArgs.args, int);
            if (F.hSize == 1)
                x = (uint64_t)(short)(x & 0xFFFF);
            else if (F.hSize == 2)
                x = (uint64_t)(signed char)(x & 0xFF);
            if ((int64_t)x < 0) {
                x = (uint64_t)(-(int64_t)x);
                F.sign = '-';
            } else if (F.forceSign)
                F.sign = '+';
            goto number;

        notSigned:
            if (F.lSize == 1)
                x = va_arg(vaArgs.args, unsigned long);
            else if (F.lSize == 2)
                x = va_arg(vaArgs.args, uint64_t);
            else
                x = va_arg(vaArgs.args, unsigned int);
            if (F.hSize == 1)
                x = (unsigned short)(x & 0xFFFF);
            else if (F.hSize == 2)
                x = (unsigned char)(x & 0xFF);
            F.sign = 0;
            goto number;

        number:
            if (F.suppress)
                continue;
            if ((base == 2) && (F.altForm > 1)) {
                i = PrintFWriteBits(x, &F, buf);
                s = buf;
            } else {
                if (!F.havePrecision) {
                    if (F.zeroPad) {
                        extra = 0;
                        if (F.altForm) {
                            if (base == 8)
                                extra += 1; // Make room for the leading "0".
                            else if (base != 10)
                                extra += 2; // Make room for the leading "0x", "0b", etc.
                        }
                        if (F.sign)
                            extra += 1; // Make room for the leading "+" or "-".
                        F.precision = (F.fieldWidth > extra) ? (F.fieldWidth - extra) : 0;
                    }
                    if (F.precision < 1)
                        F.precision = 1;
                }
                if (F.precision > (sizeof(buf) - 1))
                    F.precision = sizeof(buf) - 1;

                p = buf + sizeof(buf);
                i = 0;
                if (F.group) {
                    n = 0;
                    for (;;) {
                        Divide64x32(x, base, remain);
                        *--p = digits[remain];
                        ++i;
                        ++n;
                        if (!x)
                            break;
                        if ((n % 3) == 0) {
                            *--p = ',';
                            ++i;
                        }
                    }
                } else {
                    while (x) {
                        Divide64x32(x, base, remain);
                        *--p = digits[remain];
                        ++i;
                    }
                }
                for (; i < (int)F.precision; ++i)
                    *--p = '0';
                if (F.altForm) {
                    if (base == 8) {
                        *--p = '0';
                        i += 1;
                    } else if (base != 10) {
                        *--p = (char)c;
                        *--p = '0';
                        i += 2;
                    }
                }
                if (F.sign) {
                    *--p = F.sign;
                    ++i;
                }
                s = p;
            }
            break;

#if (PRINTF_ENABLE_FLOATING_POINT)
        case 'f': // %f: Floating point
        {
            char fpFormat[9];
            double dx;

            i = 0;
            fpFormat[i++] = '%';
            if (F.forceSign)
                fpFormat[i++] = '+';
            if (F.altForm)
                fpFormat[i++] = '#';
            if (F.zeroPad)
                fpFormat[i++] = '0';
            fpFormat[i++] = '*';
            if (F.havePrecision) {
                fpFormat[i++] = '.';
                fpFormat[i++] = '*';
            }
            fpFormat[i++] = 'f';
            fpFormat[i] = '\0';

            i = (int)F.fieldWidth;
            if (F.leftJustify)
                i = -i;
            dx = va_arg(vaArgs.args, double);
            if (F.suppress)
                continue;
            if (F.havePrecision)
                i = snprintf(buf, sizeof(buf), fpFormat, i, (int)F.precision, dx);
            else
                i = snprintf(buf, sizeof(buf), fpFormat, i, dx);
            if (i < 0) {
                err = i;
                goto error;
            }
            s = buf;
            break;
        }
#endif

        case 's': // %s: String
            src = va_arg(vaArgs.args, const char*);
            if (F.suppress)
                continue;
            if (!src && (!F.havePrecision || (F.precision != 0))) {
                s = "<<NULL>>";
                i = 8;
                break;
            }
            if (F.group && F.havePrecision) {
                if (F.precision >= 2)
                    F.precision -= 2;
                else {
                    F.precision = 0;
                    F.group = '\0';
                }
            }
            i = PrintFWriteString(src, &F, buf, &s);
            if (F.group == 1) {
                F.prefix = '\'';
                F.suffix = '\'';
            } else if (F.group == 2) {
                F.prefix = '"';
                F.suffix = '"';
            }
            break;

        case 'S': // %S: Unicode String
            a = va_arg(vaArgs.args, uint8_t*);
            if (F.suppress)
                continue;
            if (!a && (!F.havePrecision || (F.precision != 0))) {
                s = "<<NULL>>";
                i = 8;
                break;
            }
            if (F.group && F.havePrecision) {
                if (F.precision >= 2)
                    F.precision -= 2;
                else {
                    F.precision = 0;
                    F.group = '\0';
                }
            }
            i = PrintFWriteUnicodeString(a, &F, buf);
            s = buf;
            if (F.group == 1) {
                F.prefix = '\'';
                F.suffix = '\'';
            } else if (F.group == 2) {
                F.prefix = '"';
                F.suffix = '"';
            }
            break;

        case '@': // %@: Cocoa/CoreFoundation Object
            a = va_arg(vaArgs.args, uint8_t*);
            if (F.suppress)
                continue;

#if (DEBUG_CF_OBJECTS_ENABLED)
            {
                CFTypeRef cfObj;

                cfObj = (CFTypeRef)a;
                if (!cfObj)
                    cfObj = CFSTR("<<NULL>>");

                if (F.group && F.havePrecision) {
                    if (F.precision >= 2)
                        F.precision -= 2;
                    else {
                        F.precision = 0;
                        F.group = '\0';
                    }
                }
                if (F.group == 1) {
                    F.prefix = '\'';
                    F.suffix = '\'';
                } else if (F.group == 2) {
                    F.prefix = '"';
                    F.suffix = '"';
                }

                err = PrintFWriteCFObject(inContext, &F, cfObj, buf);
                if (err < 0)
                    goto error;
                total += err;
                continue;
            }
#else
            i = SNPrintF(buf, sizeof(buf), "<<%%@=%p WITH CF OBJECTS DISABLED>>", a);
            s = buf;
            break;
#endif

        case 'm': // %m: Error Message
        {
            OSStatus errCode;

            errCode = va_arg(vaArgs.args, OSStatus);
            if (F.suppress)
                continue;
            if (F.altForm) {
                if (PrintFIsPrintable((errCode >> 24) & 0xFF) && PrintFIsPrintable((errCode >> 16) & 0xFF) && PrintFIsPrintable((errCode >> 8) & 0xFF) && PrintFIsPrintable(errCode & 0xFF)) {
                    if (F.altForm == 2) {
                        i = SNPrintF(buf, sizeof(buf), "%-11d    0x%08X    '%C'    ",
                            (int)errCode, (unsigned int)errCode, (uint32_t)errCode);
                    } else {
                        i = SNPrintF(buf, sizeof(buf), "%d/0x%X/'%C' ",
                            (int)errCode, (unsigned int)errCode, (uint32_t)errCode);
                    }
                } else {
                    if (F.altForm == 2) {
                        i = SNPrintF(buf, sizeof(buf), "%-11d    0x%08X    '^^^^'    ",
                            (int)errCode, (unsigned int)errCode, (uint32_t)errCode);
                    } else {
                        i = SNPrintF(buf, sizeof(buf), "%d/0x%X ",
                            (int)errCode, (unsigned int)errCode, (uint32_t)errCode);
                    }
                }
            } else {
#if (DEBUG || DEBUG_EXPORT_ERROR_STRINGS)
                i = 0;
#else
                i = SNPrintF(buf, sizeof(buf), "%d/0x%X ", (int)errCode, (unsigned int)errCode);
#endif
            }
#if (DEBUG || DEBUG_EXPORT_ERROR_STRINGS)
            DebugGetErrorString(errCode, &buf[i], sizeof(buf) - ((size_t)i));
#endif
            s = buf;
            for (i = 0; s[i]; ++i) {
            }
            break;
        }

        case 'H': // %H: Hex Dump
            a = va_arg(vaArgs.args, uint8_t*);
            size = (size_t)va_arg(vaArgs.args, int);
            sizeMax = (size_t)va_arg(vaArgs.args, int);
            if (F.suppress)
                continue;
            if (a || (size == 0)) {
                if (size == kSizeCString)
                    size = strlen((const char*)a);
                if (F.precision == 0)
                    err = PrintFWriteHexOneLine(inContext, &F, a, Min(size, sizeMax));
                else if (F.precision == 1)
                    err = PrintFWriteHex(inContext, &F, (int)F.fieldWidth, a, size, sizeMax);
                else if (F.precision == 2) {
                    if (size <= 0)
                        err = PrintFCore(inContext, "(0 bytes)\n");
                    else if (size <= 16)
                        err = PrintFWriteHex(inContext, &F, 0, a, size, sizeMax);
                    else {
                        err = PrintFCore(inContext, "\n");
                        if (err < 0)
                            goto error;

                        err = PrintFWriteHex(inContext, &F, (int)F.fieldWidth, a, size, sizeMax);
                    }
                } else if (F.precision == 3)
                    err = PrintFWriteHexByteStream(inContext, false, a, Min(size, sizeMax));
                else if (F.precision == 4)
                    err = PrintFWriteHexByteStream(inContext, true, a, Min(size, sizeMax));
                else
                    err = PrintFCore(inContext, "<< BAD %%H PRECISION >>");
                if (err < 0)
                    goto error;
                total += err;
            } else {
                err = PrintFCore(inContext, "<<NULL %zu/%zu>>", size, sizeMax);
                if (err < 0)
                    goto error;
                total += err;
            }
            continue;

        case 'c': // %c: Character
            c = va_arg(vaArgs.args, int);
            if (F.suppress)
                continue;
            if (F.group) {
                buf[0] = '\'';
                buf[1] = PrintFMakePrintable(c);
                buf[2] = '\'';
                i = 3;
            } else {
                buf[0] = (char)c;
                i = 1;
            }
            s = buf;
            break;

        case 'C': // %C: FourCharCode
            x = va_arg(vaArgs.args, uint32_t);
            if (F.suppress)
                continue;
            i = 0;
            if (F.group)
                buf[i++] = '\'';
            buf[i] = (char)((x >> 24) & 0xFF);
            buf[i] = PrintFMakePrintable(buf[i]);
            ++i;
            buf[i] = (char)((x >> 16) & 0xFF);
            buf[i] = PrintFMakePrintable(buf[i]);
            ++i;
            buf[i] = (char)((x >> 8) & 0xFF);
            buf[i] = PrintFMakePrintable(buf[i]);
            ++i;
            buf[i] = (char)(x & 0xFF);
            buf[i] = PrintFMakePrintable(buf[i]);
            ++i;
            if (F.group)
                buf[i++] = '\'';
            s = buf;
            break;

        case 'a': // %a: Address
            a = va_arg(vaArgs.args, const uint8_t*);
            if (F.suppress)
                continue;
            if (!a) {
                s = "<<NULL>>";
                i = 8;
                break;
            }
            i = PrintFWriteAddr(a, &F, buf);
            s = buf;
            break;

        case 'N': // %N Now (date/time string).
            if (F.suppress)
                continue;
#if (TARGET_OS_POSIX)
            {
                struct timeval now;
                time_t nowTT;
                struct tm* nowTM;
                char dateTimeStr[24];
                char amPMStr[8];

                gettimeofday(&now, NULL);
                nowTT = now.tv_sec;
                nowTM = localtime(&nowTT);
                if (F.altForm)
                    strftime(dateTimeStr, sizeof(dateTimeStr), "%Y-%m-%d_%I-%M-%S", nowTM);
                else
                    strftime(dateTimeStr, sizeof(dateTimeStr), "%Y-%m-%d %I:%M:%S", nowTM);
                strftime(amPMStr, sizeof(amPMStr), "%p", nowTM);
                i = SNPrintF(buf, sizeof(buf), "%s.%06u%c%s", dateTimeStr, now.tv_usec, F.altForm ? '-' : ' ', amPMStr);
                s = buf;
            }
#elif (TARGET_HAS_STD_C_LIB)
            {
                time_t now;
                struct tm* nowTM;

                buf[0] = '\0';
                now = time(NULL);
                nowTM = localtime(&now);
                if (F.altForm)
                    i = (int)strftime(buf, sizeof(buf), "%Y-%m-%d_%I-%M-%S-%p", nowTM);
                else
                    i = (int)strftime(buf, sizeof(buf), "%Y-%m-%d %I:%M:%S %p", nowTM);
                s = buf;
            }
#elif (TARGET_OS_DARWIN_KERNEL || (TARGET_OS_NETBSD && TARGET_KERNEL))
            {
                struct timeval now;
                int64_t secs;
                int year, month, day, hour, minute, second;

                microtime(&now);
                secs = now.tv_sec + (INT64_C(719163) * kSecondsPerDay); // Seconds to 1970-01-01 00:00:00.
                SecondsToYMD_HMS(secs, &year, &month, &day, &hour, &minute, &second);
                if (F.altForm) {
                    i = SNPrintF(buf, sizeof(buf), "%04d-%02d-%02d_%02d-%02d-%02d.%06u-%s",
                        year, month, day, Hour24ToHour12(hour), minute, second, now.tv_usec, Hour24ToAMPM(hour));
                } else {
                    i = SNPrintF(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%06u %s",
                        year, month, day, Hour24ToHour12(hour), minute, second, now.tv_usec, Hour24ToAMPM(hour));
                }
                s = buf;
            }
#else
            s = "<<NO TIME>>";
            i = 11;
#endif
            break;

        case 'U': // %U: UUID
            a = va_arg(vaArgs.args, const uint8_t*);
            if (F.suppress)
                continue;
            if (!a) {
                s = "<<NULL>>";
                i = 8;
                break;
            }

            // Note: Windows and EFI treat some sections as 32-bit and 16-bit little endian values and those are the
            // most common UUID's so default to that, but allow %#U to print big-endian UUIDs.

            if (F.altForm == 0) {
                i = SNPrintF(buf, sizeof(buf), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                    a[3], a[2], a[1], a[0], a[5], a[4], a[7], a[6],
                    a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15]);
            } else {
                i = SNPrintF(buf, sizeof(buf), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                    a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7],
                    a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15]);
            }
            s = buf;
            break;

        case 'V': // %V: Nested PrintF format string and va_list.
        {
            const char* nestedFormat;
            va_list* nestedArgs;

            nestedFormat = va_arg(vaArgs.args, const char*);
            nestedArgs = va_arg(vaArgs.args, va_list*);
            if (F.suppress)
                continue;
            if (!nestedFormat || !nestedArgs) {
                s = "<<NULL>>";
                i = 8;
                break;
            }

            err = PrintFCoreVAList(inContext, nestedFormat, *nestedArgs);
            if (err < 0)
                goto error;
            total += err;
            continue;
        }

        case 'n': // %n: Receive the number of characters written so far.
            p = va_arg(vaArgs.args, char*);
            if (F.hSize == 1)
                *((short*)p) = (short)total;
            else if (F.hSize == 2)
                *((char*)p) = (char)total;
            else if (F.lSize == 1)
                *((long*)p) = (long)total;
            else if (F.lSize == 2)
                *((int64_t*)p) = (long)total;
            else
                *((int*)p) = total;
            continue;

        case '%': // %%: Literal %
            if (F.suppress)
                continue;
            buf[0] = '%';
            i = 1;
            s = buf;
            break;

        case '{': // %{<extension>}
        {
            const char* extensionPtr;
            size_t extensionLen;

            extensionPtr = ++fmt;
            while ((c != '\0') && (c != '}'))
                c = *++fmt;
            extensionLen = (size_t)(fmt - extensionPtr);

            // %{text}: Multi-line text (with optional indenting).

            if (strnicmpx(extensionPtr, extensionLen, "text") == 0) {
                s = va_arg(vaArgs.args, const char*);
                size = va_arg(vaArgs.args, size_t);
                if (F.suppress)
                    continue;
                if (size == kSizeCString)
                    size = strlen(s);

                err = PrintFWriteMultiLineText(inContext, &F, s, size);
                if (err < 0)
                    goto error;
                total += err;
                continue;
            }

            // Unknown extension.

            i = SNPrintF(buf, sizeof(buf), "<<UNKNOWN PRINTF EXTENSION '%.*s'>>", (int)extensionLen, extensionPtr);
            s = buf;
            break;
        }

        default:
            i = SNPrintF(buf, sizeof(buf), "<<UNKNOWN FORMAT CONVERSION CODE %%%c>>", c);
            s = buf;
            break;
        }

        // Print the text with the correct padding, etc.

        err = PrintFWriteText(inContext, &F, s, (size_t)i);
        if (err < 0)
            goto error;
        total += err;
    }

    return (total);

error:
    return (err);
}

//===========================================================================================================================
//	PrintFWriteAddr
//===========================================================================================================================

typedef struct
{
    int32_t type;
    union {
        uint8_t v4[4];
        uint8_t v6[16];

    } ip;

} mDNSAddrCompat;

static int PrintFWriteAddr(const uint8_t* inAddr, PrintFFormat* inFormat, char* outStr)
{
    int n;
    const uint8_t* a;
    PrintFFormat* F;

    a = inAddr;
    F = inFormat;
    if ((F->altForm == 1) && (F->precision == 4)) // %#.4a - IPv4 address in host byte order
    {
#if (TARGET_RT_BIG_ENDIAN)
        n = SNPrintF(outStr, kPrintFBufSize, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
#else
        n = SNPrintF(outStr, kPrintFBufSize, "%u.%u.%u.%u", a[3], a[2], a[1], a[0]);
#endif
    } else if ((F->altForm == 1) && (F->precision == 6)) // %#.6a - MAC address from host order uint64_t *.
    {
#if (TARGET_RT_BIG_ENDIAN)
        n = SNPrintF(outStr, kPrintFBufSize, "%02X:%02X:%02X:%02X:%02X:%02X",
            a[2], a[3], a[4], a[5], a[6], a[7]);
#else
        n = SNPrintF(outStr, kPrintFBufSize, "%02X:%02X:%02X:%02X:%02X:%02X",
            a[5], a[4], a[3], a[2], a[1], a[0]);
#endif
    } else if (F->altForm == 1) // %#a: mDNSAddr
    {
        mDNSAddrCompat* ip;

        ip = (mDNSAddrCompat*)inAddr;
        if (ip->type == 4) {
            a = ip->ip.v4;
            n = SNPrintF(outStr, kPrintFBufSize, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
        } else if (ip->type == 6) {
            IPv6AddressToCString(ip->ip.v6, 0, 0, -1, outStr, 0);
            n = (int)strlen(outStr);
        } else {
            n = SNPrintF(outStr, kPrintFBufSize, "%s", "<< ERROR: %#a used with unsupported type: %d >>", ip->type);
        }
    } else if (F->altForm == 2) // %##a: sockaddr
    {
#if (defined(AF_INET))
        int family;

        family = ((const struct sockaddr*)inAddr)->sa_family;
        if (family == AF_INET) {
            const struct sockaddr_in* const sa4 = (const struct sockaddr_in*)a;

            IPv4AddressToCString(ntoh32(sa4->sin_addr.s_addr), (int)ntoh16(sa4->sin_port), outStr);
            n = (int)strlen(outStr);
        }
#if (defined(AF_INET6))
        else if (family == AF_INET6) {
            const struct sockaddr_in6* const sa6 = (const struct sockaddr_in6*)a;

            IPv6AddressToCString(sa6->sin6_addr.s6_addr, sa6->sin6_scope_id, (int)ntoh16(sa6->sin6_port), -1, outStr, 0);
            n = (int)strlen(outStr);
        }
#endif
#if (defined(AF_LINK) && defined(LLADDR))
        else if (family == AF_LINK) {
            const struct sockaddr_dl* const sdl = (const struct sockaddr_dl*)a;

            a = (const uint8_t*)LLADDR(sdl);
            if (sdl->sdl_alen == 6) {
                n = SNPrintF(outStr, kPrintFBufSize, "%02X:%02X:%02X:%02X:%02X:%02X",
                    a[0], a[1], a[2], a[3], a[4], a[5]);
            } else {
                n = SNPrintF(outStr, kPrintFBufSize, "<< AF_LINK %H >>", a, sdl->sdl_alen, sdl->sdl_alen);
            }
        }
#endif
        else if (family == AF_UNSPEC) {
            n = SNPrintF(outStr, kPrintFBufSize, "<< AF_UNSPEC >>");
        } else {
            n = SNPrintF(outStr, kPrintFBufSize, "<< ERROR: %%##a used with unknown family: %d >>", family);
        }
#else
        n = SNPrintF(outStr, kPrintFBufSize, "%s", "<< ERROR: %##a used without socket support >>");
#endif
    } else {
        switch (F->precision) {
        case 2:
            n = SNPrintF(outStr, kPrintFBufSize, "%u.%u.%u.%u", a[0] >> 4, a[0] & 0xF, a[1] >> 4, a[1] & 0xF);
            break;

        case 4:
            n = SNPrintF(outStr, kPrintFBufSize, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
            break;

        case 6:
            n = SNPrintF(outStr, kPrintFBufSize, "%02X:%02X:%02X:%02X:%02X:%02X",
                a[0], a[1], a[2], a[3], a[4], a[5]);
            break;

        case 8:
            n = SNPrintF(outStr, kPrintFBufSize, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
            break;

        case 16:
            IPv6AddressToCString(a, 0, 0, -1, outStr, 0);
            n = (int)strlen(outStr);
            break;

        default:
            n = SNPrintF(outStr, kPrintFBufSize, "%s",
                "<< ERROR: Must specify address size (i.e. %.4a=IPv4, %.6a=Enet, %.8a=Fibre, %.16a=IPv6) >>");
            break;
        }
    }
    return (n);
}

//===========================================================================================================================
//	PrintFWriteBits
//===========================================================================================================================

static int PrintFWriteBits(uint64_t inX, PrintFFormat* inFormat, char* outStr)
{
#if (TYPE_LONGLONG_NATIVE)
    static const uint64_t kBit0 = 1;
    uint64_t x = inX;
#else
    static const unsigned long kBit0 = 1;
    unsigned long x = (unsigned long)inX;
#endif
    int maxBit;
    int bit;
    char* dst;
    char* lim;

    dst = outStr;
    lim = dst + kPrintFBufSize;
    if (!inFormat->havePrecision) {
        if (inFormat->hSize == 1)
            inFormat->precision = 8 * sizeof(short);
        else if (inFormat->hSize == 2)
            inFormat->precision = 8 * sizeof(char);
        else if (inFormat->lSize == 1)
            inFormat->precision = 8 * sizeof(long);
        else if (inFormat->lSize == 2)
            inFormat->precision = 8 * sizeof(int64_t);
        else
            inFormat->precision = 8 * sizeof(int);
    }
    if (inFormat->precision > (sizeof(kBit0) * 8)) {
        SNPrintF_Add(&dst, lim, "ERROR: << precision must be 0-%d >>", (sizeof(kBit0) * 8));
    } else {
        if (inFormat->precision < 1)
            inFormat->precision = 1;
        maxBit = (int)(inFormat->precision - 1);
        if (inFormat->altForm == 2) {
            for (bit = maxBit; bit >= 0; --bit) {
                if (x & (kBit0 << bit)) {
                    SNPrintF_Add(&dst, lim, "%s%d", (dst != outStr) ? " " : "", bit);
                }
            }
        } else {
            for (bit = 0; bit <= maxBit; ++bit) {
                if (x & (kBit0 << (maxBit - bit))) {
                    SNPrintF_Add(&dst, lim, "%s%d", (dst != outStr) ? " " : "", bit);
                }
            }
        }
    }
    return ((int)(dst - outStr));
}

#if (DEBUG_CF_OBJECTS_ENABLED)

//===========================================================================================================================
//	PrintFWriteCFObject
//===========================================================================================================================

typedef struct
{
    PrintFContext* context;
    PrintFFormat* format;
    int indent;
    int total; // Note: temporary total for a recursive operation.
    OSStatus error;

} PrintFWriteCFObjectContext;

static int PrintFWriteCFObjectLevel(PrintFWriteCFObjectContext* inContext, CFTypeRef inObj, Boolean inPrintingArray);
static void PrintFWriteCFObjectApplier(const void* inKey, const void* inValue, void* inContext);

static int PrintFWriteCFObject(PrintFContext* inContext, PrintFFormat* inFormat, CFTypeRef inObj, char* inBuffer)
{
    int total;
    CFTypeID typeID;
    const char* s;
    int n;

    typeID = CFGetTypeID(inObj);

    // Boolean

    if (typeID == CFBooleanGetTypeID()) {
        if (((CFBooleanRef)inObj) == kCFBooleanTrue) {
            s = "true";
            n = 4;
        } else {
            s = "false";
            n = 5;
        }
        total = PrintFWriteText(inContext, inFormat, s, (size_t)n);
    }

    // Number

    else if (typeID == CFNumberGetTypeID()) {
#if (!CFLITE_ENABLED || CFL_FLOATING_POINT_NUMBERS)
        if (CFNumberIsFloatType((CFNumberRef)inObj)) {
            double dval = 0;

            CFNumberGetValue((CFNumberRef)inObj, kCFNumberDoubleType, &dval);
            n = SNPrintF(inBuffer, kPrintFBufSize, "%f", dval);
        } else
#endif
        {
            int64_t s64 = 0;

            CFNumberGetValue((CFNumberRef)inObj, kCFNumberSInt64Type, &s64);
            n = SNPrintF(inBuffer, kPrintFBufSize, "%lld", s64);
        }
        total = PrintFWriteText(inContext, inFormat, inBuffer, (size_t)n);
    }

    // String

    else if (typeID == CFStringGetTypeID()) {
        CFStringRef const cfStr = (CFStringRef)inObj;
        CFIndex cfLen;
        size_t size;

        cfLen = CFStringGetLength(cfStr);
        size = (size_t)CFStringGetMaximumSizeForEncoding(cfLen, kCFStringEncodingUTF8);
        if (size > 0) {
            char* cStr;
            CFRange range;
            CFIndex i;

            cStr = (char*)malloc(size);
            require_action_quiet(cStr, exit, total = kNoMemoryErr);

            i = 0;
            range = CFRangeMake(0, cfLen);
            CFStringGetBytes(cfStr, range, kCFStringEncodingUTF8, '^', false, (UInt8*)cStr, (CFIndex)size, &i);

            // Restrict the string length to the precision, but don't truncate in the middle of a UTF-8 character.

            if (inFormat->havePrecision && (i > (CFIndex)inFormat->precision)) {
                for (i = (int)inFormat->precision; (i > 0) && ((cStr[i] & 0xC0) == 0x80); --i) {
                }
            }

            total = PrintFWriteText(inContext, inFormat, cStr, (size_t)i);
            free(cStr);
        } else {
            // Note: this is needed because there may be field widths, etc. to fill.

            total = PrintFWriteText(inContext, inFormat, "", 0);
        }
    }

    // Null

    else if (typeID == CFNullGetTypeID()) {
        total = PrintFWriteText(inContext, inFormat, "Null", 4);
    }

#if (TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES)
    // URL

    else if (typeID == CFURLGetTypeID()) {
        CFStringRef cfStr;

        cfStr = CFURLGetString((CFURLRef)inObj);
        require_action_quiet(cfStr, exit, total = kUnknownErr);
        total = PrintFWriteCFObject(inContext, inFormat, cfStr, inBuffer);
    }

    // UUID

    else if (typeID == CFUUIDGetTypeID()) {
        CFUUIDBytes bytes;

        bytes = CFUUIDGetUUIDBytes((CFUUIDRef)inObj);
        n = SNPrintF(inBuffer, kPrintFBufSize, "%#U", &bytes);
        total = PrintFWriteText(inContext, inFormat, inBuffer, (size_t)n);
    }
#endif

    // Other

    else {
        PrintFWriteCFObjectContext cfContext;

        cfContext.context = inContext;
        cfContext.format = inFormat;
        cfContext.indent = (int)inFormat->fieldWidth;
        cfContext.error = kNoErr;

        total = PrintFWriteCFObjectLevel(&cfContext, inObj, false);
        require_quiet(total >= 0, exit);

        if ((typeID == CFArrayGetTypeID()) || (typeID == CFDictionaryGetTypeID())) {
            n = inContext->callback("\n", 1, inContext);
            require_action_quiet(n > 0, exit, total = n);
            total += n;
        }
    }

exit:
    return (total);
}

//===========================================================================================================================
//	PrintFWriteCFObjectLevel
//===========================================================================================================================

static int PrintFWriteCFObjectLevel(PrintFWriteCFObjectContext* inContext, CFTypeRef inObj, Boolean inPrintingArray)
{
    int total = 0;
    OSStatus err;
    CFTypeID typeID;
    CFIndex i, n;
    CFTypeRef obj;
    size_t size;
    char buf[4];

    typeID = CFGetTypeID(inObj);

    // Array

    if (typeID == CFArrayGetTypeID()) {
        err = print_indent(inContext->context, inContext->indent);
        require_action_quiet(err >= 0, exit, total = err);
        total += err;

        n = CFArrayGetCount((CFArrayRef)inObj);
        if (n > 0) {
            err = inContext->context->callback("[\n", 2, inContext->context);
            require_action_quiet(err >= 0, exit, total = err);
            total += 2;

            for (i = 0; i < n;) {
                obj = CFArrayGetValueAtIndex((CFArrayRef)inObj, i);

                ++inContext->indent;
                err = PrintFWriteCFObjectLevel(inContext, obj, true);
                --inContext->indent;
                require_action_quiet(err >= 0, exit, total = err);
                total += err;

                ++i;
                size = 0;
                if (i < n)
                    buf[size++] = ',';
                buf[size++] = '\n';
                err = inContext->context->callback(buf, size, inContext->context);
                require_action_quiet(err >= 0, exit, total = err);
                total += 1;
            }

            err = print_indent(inContext->context, inContext->indent);
            require_action_quiet(err >= 0, exit, total = err);
            total += err;

            err = inContext->context->callback("]", 1, inContext->context);
            require_action_quiet(err >= 0, exit, total = err);
            total += 1;
        } else {
            err = inContext->context->callback("[]", 2, inContext->context);
            require_action_quiet(err >= 0, exit, total = err);
            total += 2;
        }
    }

    // Boolean

    else if (typeID == CFBooleanGetTypeID()) {
        const char* boolStr;

        err = print_indent(inContext->context, inContext->indent);
        require_action_quiet(err >= 0, exit, total = err);
        total += err;

        if (((CFBooleanRef)inObj) == kCFBooleanTrue) {
            boolStr = "true";
            size = 4;
        } else {
            boolStr = "false";
            size = 5;
        }

        err = inContext->context->callback(boolStr, size, inContext->context);
        require_action_quiet(err >= 0, exit, total = err);
        total += (int)size;
    }

    // Data

    else if (typeID == CFDataGetTypeID()) {
        int oldIndent;

        oldIndent = inContext->indent;
        size = (size_t)CFDataGetLength((CFDataRef)inObj);
        if ((size <= 16) && !inPrintingArray) {
            inContext->indent = 0;
        } else {
            err = inContext->context->callback("\n", 1, inContext->context);
            require_action_quiet(err >= 0, exit, total = err);
            total += 1;

            inContext->indent = oldIndent + 1;
        }

        err = PrintFWriteHex(inContext->context, inContext->format, inContext->indent,
            CFDataGetBytePtr((CFDataRef)inObj), size,
            inContext->format->havePrecision ? inContext->format->precision : size);
        require_action_quiet(err >= 0, exit, total = err);
        total += err;

        inContext->indent = oldIndent;
    }

    // Date

    else if (typeID == CFDateGetTypeID()) {
        int year, month, day, hour, minute, second, micros;

        err = print_indent(inContext->context, inContext->indent);
        require_action_quiet(err >= 0, exit, total = err);
        total += err;

        CFDateGetComponents((CFDateRef)inObj, &year, &month, &day, &hour, &minute, &second, &micros);
        err = PrintFCore(inContext->context, "%04d-%02d-%02d %02d:%02d:%02d.%06d",
            year, month, day, hour, minute, second, micros);
        require_action_quiet(err >= 0, exit, total = err);
        total += err;
    }

    // Dictionary

    else if (typeID == CFDictionaryGetTypeID()) {
        err = print_indent(inContext->context, inContext->indent);
        require_action_quiet(err >= 0, exit, total = err);
        total += err;

        if (CFDictionaryGetCount((CFDictionaryRef)inObj) > 0) {
            err = inContext->context->callback("{\n", 2, inContext->context);
            require_action_quiet(err >= 0, exit, total = err);
            total += 2;

            ++inContext->indent;

            inContext->total = total;
            CFDictionaryApplyFunction((CFDictionaryRef)inObj, PrintFWriteCFObjectApplier, inContext);
            require_action_quiet(inContext->error >= 0, exit, total = inContext->error);
            total = inContext->total;

            --inContext->indent;

            err = print_indent(inContext->context, inContext->indent);
            require_action_quiet(err >= 0, exit, total = err);
            total += err;

            err = inContext->context->callback("}", 1, inContext->context);
            require_action_quiet(err >= 0, exit, total = err);
            total += 1;
        } else {
            err = inContext->context->callback("{}", 2, inContext->context);
            require_action_quiet(err >= 0, exit, total = err);
            total += 2;
        }
    }

    // Number

    else if (typeID == CFNumberGetTypeID()) {
        err = print_indent(inContext->context, inContext->indent);
        require_action_quiet(err >= 0, exit, total = err);
        total += err;

#if (!CFLITE_ENABLED || CFL_FLOATING_POINT_NUMBERS)
        if (CFNumberIsFloatType((CFNumberRef)inObj)) {
            double dval = 0;

            CFNumberGetValue((CFNumberRef)inObj, kCFNumberDoubleType, &dval);
            err = PrintFCore(inContext->context, "%f", dval);
        } else
#endif
        {
            int64_t s64 = 0;

            CFNumberGetValue((CFNumberRef)inObj, kCFNumberSInt64Type, &s64);
            err = PrintFCore(inContext->context, "%lld", s64);
        }
        require_action_quiet(err >= 0, exit, total = err);
        total += err;
    }

    // String

    else if (typeID == CFStringGetTypeID()) {
        CFStringRef const cfStr = (CFStringRef)inObj;

        err = print_indent(inContext->context, inContext->indent);
        require_action_quiet(err >= 0, exit, total = err);
        total += err;

        err = inContext->context->callback("\"", 1, inContext->context);
        require_action_quiet(err >= 0, exit, total = err);
        total += 1;

        n = CFStringGetLength(cfStr);
        size = (size_t)CFStringGetMaximumSizeForEncoding(n, kCFStringEncodingUTF8);
        if (size > 0) {
            char* cStr;
            CFRange range;
            CFIndex converted;

            cStr = (char*)malloc(size);
            require_action_quiet(cStr, exit, total = kNoMemoryErr);

            converted = 0;
            range = CFRangeMake(0, n);
            CFStringGetBytes(cfStr, range, kCFStringEncodingUTF8, '^', false, (UInt8*)cStr, (CFIndex)size, &converted);

            err = inContext->context->callback(cStr, (size_t)converted, inContext->context);
            free(cStr);
            require_action_quiet(err >= 0, exit, total = err);
            total += (int)converted;
        }

        err = inContext->context->callback("\"", 1, inContext->context);
        require_action_quiet(err >= 0, exit, total = err);
        total += 1;
    }

    // Null

    else if (typeID == CFNullGetTypeID()) {
        err = print_indent(inContext->context, inContext->indent);
        require_action_quiet(err >= 0, exit, total = err);
        total += err;

        err = inContext->context->callback("Null", 4, inContext->context);
        require_action_quiet(err >= 0, exit, total = err);
        total += 4;
    }

#if (TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES)
    // URL

    else if (typeID == CFURLGetTypeID()) {
        CFStringRef cfStr;

        cfStr = CFURLGetString((CFURLRef)inObj);
        require_action_quiet(cfStr, exit, total = kUnknownErr);
        err = PrintFWriteCFObjectLevel(inContext, cfStr, inPrintingArray);
        require_action_quiet(err >= 0, exit, total = err);
        total += err;
    }

    // UUID

    else if (typeID == CFUUIDGetTypeID()) {
        CFUUIDBytes bytes;

        err = print_indent(inContext->context, inContext->indent);
        require_action_quiet(err >= 0, exit, total = err);
        total += err;

        bytes = CFUUIDGetUUIDBytes((CFUUIDRef)inObj);
        err = PrintFCore(inContext->context, "%#U", &bytes);
        require_action_quiet(err >= 0, exit, total = err);
        total += err;
    }
#endif

    // Unknown

    else {
        err = print_indent(inContext->context, inContext->indent);
        require_action_quiet(err >= 0, exit, total = err);
        total += err;

#if (TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES)
        {
            CFStringRef desc;

            desc = CFCopyDescription(inObj);
            if (desc) {
                err = PrintFCore(inContext->context, "%@", desc);
                CFRelease(desc);
                require_action_quiet(err >= 0, exit, total = err);
                total += err;
                goto exit;
            }
        }
#endif

        err = PrintFCore(inContext->context, "<<UNKNOWN CF OBJECT TYPE: %d>>", (int)typeID);
        require_action_quiet(err >= 0, exit, total = err);
        total += err;
    }

exit:
    return (total);
}

//===========================================================================================================================
//	PrintFWriteCFObjectApplier
//===========================================================================================================================

static void PrintFWriteCFObjectApplier(const void* inKey, const void* inValue, void* inContext)
{
    int total;
    PrintFWriteCFObjectContext* const context = (PrintFWriteCFObjectContext*)inContext;
    CFTypeRef const value = (CFTypeRef)inValue;
    OSStatus err;
    CFTypeID typeID;

    if (context->error)
        return;

    // Print the key.

    err = PrintFWriteCFObjectLevel(context, (CFTypeRef)inKey, false);
    require_action_quiet(err >= 0, exit, total = err);
    total = err;

    err = context->context->callback(" : ", 3, context->context);
    require_action_quiet(err >= 0, exit, total = err);
    total += 3;

    // Print the value based on its type.

    typeID = CFGetTypeID(value);
    if (typeID == CFArrayGetTypeID()) {
        if (CFArrayGetCount((CFArrayRef)inValue) > 0) {
            err = context->context->callback("\n", 1, context->context);
            require_action_quiet(err >= 0, exit, total = err);
            total += 1;

            err = PrintFWriteCFObjectLevel(context, value, true);
            require_action_quiet(err >= 0, exit, total = err);
            total += err;

            err = context->context->callback("\n", 1, context->context);
            require_action_quiet(err >= 0, exit, total = err);
            total += 1;
        } else {
            err = context->context->callback("[]\n", 3, context->context);
            require_action_quiet(err >= 0, exit, total = err);
            total += 3;
        }
    } else if (typeID == CFDictionaryGetTypeID()) {
        if (CFDictionaryGetCount((CFDictionaryRef)inValue) > 0) {
            err = context->context->callback("\n", 1, context->context);
            require_action_quiet(err >= 0, exit, total = err);
            total += 1;

            err = PrintFWriteCFObjectLevel(context, value, false);
            require_action_quiet(err >= 0, exit, total = err);
            total += err;

            err = context->context->callback("\n", 1, context->context);
            require_action_quiet(err >= 0, exit, total = err);
            total += 1;
        } else {
            err = context->context->callback("{}\n", 3, context->context);
            require_action_quiet(err >= 0, exit, total = err);
            total += 3;
        }
    } else if ((typeID == CFDataGetTypeID()) && (context->format->altForm != 2)) {
        err = PrintFWriteCFObjectLevel(context, value, false);
        require_action_quiet(err >= 0, exit, total = err);
        total += err;
    } else {
        int oldIndent;

        oldIndent = context->indent;
        context->indent = 0;

        err = PrintFWriteCFObjectLevel(context, value, false);
        require_action_quiet(err >= 0, exit, total = err);
        total += err;

        context->indent = oldIndent;

        err = context->context->callback("\n", 1, context->context);
        require_action_quiet(err >= 0, exit, total = err);
        total += 1;
    }

exit:
    context->total += total;
    if (err < 0)
        context->error = err;
}

#endif // DEBUG_CF_OBJECTS_ENABLED

//===========================================================================================================================
//	PrintFWriteHex
//===========================================================================================================================

static int
PrintFWriteHex(
    PrintFContext* inContext,
    PrintFFormat* inFormat,
    int inIndent,
    const void* inData,
    size_t inSize,
    size_t inMaxSize)
{
    int total = 0;
    int err;
    const uint8_t* start;
    const uint8_t* ptr;
    size_t size;
    uint8_t hex1[64];
    uint8_t hex2[64];
    uint8_t* currHexPtr;
    uint8_t* prevHexPtr;
    uint8_t* tempHexPtr;
    int dupCount;
    size_t dupSize;

    currHexPtr = hex1;
    prevHexPtr = hex2;
    dupCount = 0;
    dupSize = 0;
    start = (const uint8_t*)inData;
    ptr = start;
    size = (inSize > inMaxSize) ? inMaxSize : inSize;

    for (;;) {
        size_t chunkSize;
        uint8_t ascii[64];
        uint8_t* s;
        uint8_t c;
        size_t i;

        // Build a hex string (space every 4 bytes) and pad with space to fill the full 16-byte range.

        chunkSize = Min(size, 16);
        s = currHexPtr;
        for (i = 0; i < 16; ++i) {
            if ((i > 0) && ((i % 4) == 0))
                *s++ = ' ';
            if (i < chunkSize) {
                *s++ = (uint8_t)kHexDigitsLowercase[ptr[i] >> 4];
                *s++ = (uint8_t)kHexDigitsLowercase[ptr[i] & 0xF];
            } else {
                *s++ = ' ';
                *s++ = ' ';
            }
        }
        *s++ = '\0';
        check(((size_t)(s - currHexPtr)) < sizeof(hex1));

        // Build a string with the ASCII version of the data (replaces non-printable characters with '^').
        // Pads the string with spaces to fill the full 16 byte range (so it lines up).

        s = ascii;
        for (i = 0; i < 16; ++i) {
            if (i < chunkSize) {
                c = ptr[i];
                if (!PrintFIsPrintable(c)) {
                    c = '^';
                }
            } else {
                c = ' ';
            }
            *s++ = c;
        }
        *s++ = '\0';
        check(((size_t)(s - ascii)) < sizeof(ascii));

        // Print the data.

        if (inSize <= 16) {
            err = print_indent(inContext, inIndent);
            require_action_quiet(err >= 0, exit, total = err);
            total += err;

            err = PrintFCore(inContext, "%s |%s| (%zu bytes)\n", currHexPtr, ascii, inSize);
            require_action_quiet(err >= 0, exit, total = err);
            total += err;
        } else if (ptr == start) {
            err = print_indent(inContext, inIndent);
            require_action_quiet(err >= 0, exit, total = err);
            total += err;

            err = PrintFCore(inContext, "+%04X: %s |%s| (%zu bytes)\n", (int)(ptr - start), currHexPtr, ascii, inSize);
            require_action_quiet(err >= 0, exit, total = err);
            total += err;
        } else if ((inFormat->group > 0) && (memcmp(currHexPtr, prevHexPtr, 32) == 0)) {
            dupCount += 1;
            dupSize += chunkSize;
        } else {
            if (dupCount > 0) {
                err = print_indent(inContext, inIndent);
                require_action_quiet(err >= 0, exit, total = err);
                total += err;

                err = PrintFCore(inContext, "* (%zu more identical bytes, %zu total)\n", dupSize, dupSize + 16);
                require_action_quiet(err >= 0, exit, total = err);
                total += err;

                dupCount = 0;
                dupSize = 0;
            }

            err = print_indent(inContext, inIndent);
            require_action_quiet(err >= 0, exit, total = err);
            total += err;

            err = PrintFCore(inContext, "+%04X: %s |%s|\n", (int)(ptr - start), currHexPtr, ascii);
            require_action_quiet(err >= 0, exit, total = err);
            total += err;
        }

        tempHexPtr = prevHexPtr;
        prevHexPtr = currHexPtr;
        currHexPtr = tempHexPtr;

        ptr += chunkSize;
        size -= chunkSize;
        if (size <= 0)
            break;
    }

    if (dupCount > 0) {
        err = print_indent(inContext, inIndent);
        require_action_quiet(err >= 0, exit, total = err);
        total += err;

        err = PrintFCore(inContext, "* (%zu more identical bytes, %zu total)\n", dupSize, dupSize + 16);
        require_action_quiet(err >= 0, exit, total = err);
        total += err;
    }
    if (inSize > inMaxSize) {
        err = print_indent(inContext, inIndent);
        require_action_quiet(err >= 0, exit, total = err);
        total += err;

        err = PrintFCore(inContext, "... %zu more bytes ...\n", inSize - inMaxSize);
        require_action_quiet(err >= 0, exit, total = err);
        total += err;
    }

exit:
    return (total);
}

//===========================================================================================================================
//	PrintFWriteHexOneLine
//===========================================================================================================================

static int PrintFWriteHexOneLine(PrintFContext* inContext, PrintFFormat* inFormat, const uint8_t* inData, size_t inSize)
{
    int total = 0;
    int err;
    size_t i;
    size_t j;
    uint8_t b;
    char hex[3];
    char c;

    require_quiet(inSize > 0, exit);

    // Print each byte as hex.

    if (inFormat->altForm != 2) {
        for (i = 0; i < inSize; ++i) {
            j = 0;
            if (i != 0)
                hex[j++] = ' ';
            b = inData[i];
            hex[j++] = kHexDigitsLowercase[(b >> 4) & 0x0F];
            hex[j++] = kHexDigitsLowercase[b & 0x0F];
            err = inContext->callback(hex, j, inContext);
            require_action_quiet(err >= 0, exit, total = err);
            total += (int)j;
        }
    }

    // Print each byte as ASCII if requested.

    if (inFormat->altForm > 0) {
        if (total > 0) {
            err = inContext->callback(" |", 2, inContext);
            require_action_quiet(err >= 0, exit, total = err);
            total += 2;
        } else {
            err = inContext->callback("|", 1, inContext);
            require_action_quiet(err >= 0, exit, total = err);
            total += 1;
        }
        for (i = 0; i < inSize; ++i) {
            c = (char)inData[i];
            if ((c < 0x20) || (c >= 0x7F))
                c = '^';

            err = inContext->callback(&c, 1, inContext);
            require_action_quiet(err >= 0, exit, total = err);
            total += 1;
        }

        err = inContext->callback("|", 1, inContext);
        require_action_quiet(err >= 0, exit, total = err);
        total += 1;
    }

exit:
    return (total);
}

//===========================================================================================================================
//	PrintFWriteHexByteStream
//===========================================================================================================================

static int PrintFWriteHexByteStream(PrintFContext* inContext, Boolean inUppercase, const uint8_t* inData, size_t inSize)
{
    const char* const digits = inUppercase ? kHexDigitsUppercase : kHexDigitsLowercase;
    int total = 0;
    int err;
    const uint8_t* src;
    const uint8_t* end;
    char buf[64];
    char* dst;
    char* lim;
    size_t len;

    src = inData;
    end = src + inSize;
    dst = buf;
    lim = dst + sizeof(buf);

    while (src < end) {
        uint8_t b;

        if (dst == lim) {
            len = (size_t)(dst - buf);
            err = inContext->callback(buf, len, inContext);
            require_action_quiet(err >= 0, exit, total = err);
            total += (int)len;

            dst = buf;
        }

        b = *src++;
        *dst++ = digits[(b >> 4) & 0x0F];
        *dst++ = digits[b & 0x0F];
    }
    if (dst != buf) {
        len = (size_t)(dst - buf);
        err = inContext->callback(buf, len, inContext);
        require_action_quiet(err >= 0, exit, total = err);
        total += (int)len;
    }

exit:
    return (total);
}

//===========================================================================================================================
//	PrintFWriteMultiLineText
//===========================================================================================================================

static int PrintFWriteMultiLineText(PrintFContext* inContext, PrintFFormat* inFormat, const char* inStr, size_t inLen)
{
    int total = 0, err;
    const char* line;
    const char* end;
    const char* eol;
    const char* next;
    unsigned int i, n;
    size_t len;

    for (line = inStr, end = line + inLen; line < end; line = next) {
        for (eol = line; (eol < end) && (*eol != '\r') && (*eol != '\n'); ++eol) {
        }
        if (eol < end) {
            if ((eol[0] == '\r') && (((eol + 1) < end) && (eol[1] == '\n'))) {
                next = eol + 2;
            } else {
                next = eol + 1;
            }
        } else {
            next = eol;
        }
        if ((line < eol) && (*line != '\r') && (*line != '\n')) {
            n = inFormat->fieldWidth;
            for (i = 0; i < n; ++i) {
                err = inContext->callback("    ", 4, inContext);
                require_action_quiet(err >= 0, exit, total = err);
                total += 4;
            }
        }
        if (line < eol) {
            len = (size_t)(eol - line);
            err = inContext->callback(line, len, inContext);
            require_action_quiet(err >= 0, exit, total = err);
            total += (int)len;
        }
        if (eol < end) {
            err = inContext->callback("\n", 1, inContext);
            require_action_quiet(err >= 0, exit, total = err);
            total += 1;
        }
    }

exit:
    return (total);
}

//===========================================================================================================================
//	PrintFWriteString
//===========================================================================================================================

static int PrintFWriteString(const char* inStr, PrintFFormat* inFormat, char* inBuf, const char** outStrPtr)
{
    int i;
    const char* s;
    PrintFFormat* F;
    int c;

    s = inStr;
    F = inFormat;
    if (F->altForm == 0) // %s: C-string
    {
        i = 0;
        if (F->havePrecision) {
            int j;

            while ((i < (int)F->precision) && (s[i] != '\0'))
                ++i;

            // Make sure we don't truncate in the middle of a UTF-8 character.
            // If the last character is part of a multi-byte UTF-8 character, back up to the start of it.
            // If the actual count of UTF-8 characters matches the encoded UTF-8 count, add it back.

            c = 0;
            j = 0;
            while ((i > 0) && ((c = s[i - 1]) & 0x80)) {
                ++j;
                --i;
                if ((c & 0xC0) != 0x80)
                    break;
            }
            if ((j > 1) && (j <= 6)) {
                int test;
                int mask;

                test = (0xFF << (8 - j)) & 0xFF;
                mask = test | (1 << ((8 - j) - 1));
                if ((c & mask) == test)
                    i += j;
            }
        } else {
            while (s[i] != '\0')
                ++i;
        }
    } else if (F->altForm == 1) // %#s: Pascal-string
    {
        i = *s++;
    } else if (F->altForm == 2) // %##s: DNS label-sequence name
    {
        const uint8_t* a;
        char* dst;
        char* lim;

        a = (const uint8_t*)s;
        dst = inBuf;
        lim = dst + kPrintFBufSize;
        if (*a == 0)
            *dst++ = '.'; // Special case for root DNS name.
        while (*a) {
            if (*a > 63) {
                SNPrintF_Add(&dst, lim, "<<INVALID DNS LABEL LENGTH %u>>", *a);
                break;
            }
            if ((dst + *a) >= &inBuf[254]) {
                SNPrintF_Add(&dst, lim, "<<DNS NAME TOO LONG>>");
                break;
            }
            SNPrintF_Add(&dst, lim, "%#s.", a);
            a += (1 + *a);
        }
        i = (int)(dst - inBuf);
        s = inBuf;
    } else if (F->altForm == 3) // %###s: Cleansed function name (i.e. isolate just the [<class>::]<function> part).
    {
        const char* functionStart;

        // This needs to handle function names with the following forms:
        //
        // main
        // main(int, const char **)
        // int main(int, const char **)
        // MyClass::operator()
        // MyClass::operator'()'
        // const char * MyClass::MyFunction(const char *) const
        // void *MyClass::MyFunction(const char *) const
        // +[MyClass MyMethod]
        // -[MyClass MyMethod:andParam2:]

        functionStart = inStr;
        if ((*functionStart == '+') || (*functionStart == '-')) // Objective-C class or instance method.
        {
            s = functionStart + strlen(functionStart);
        } else {
            for (s = inStr; ((c = *s) != '\0') && (c != ':'); ++s) {
                if (c == ' ')
                    functionStart = s + 1;
            }
            if (c == ':')
                c = *(++s);
            if (c == ':')
                ++s;
            else {
                // Non-C++ function so re-do the search for a C function name.

                functionStart = inStr;
                for (s = inStr; ((c = *s) != '\0') && (c != '('); ++s) {
                    if (c == ' ')
                        functionStart = s + 1;
                }
            }
            for (; ((c = *s) != '\0') && (c != ' ') && (c != '('); ++s) {
            }
            if ((s[0] == '(') && (s[1] == ')') && (s[2] == '('))
                s += 2;
            else if ((s[0] == '(') && (s[1] == ')') && (s[2] == '\''))
                s += 3;
            if ((functionStart < s) && (*functionStart == '*'))
                ++functionStart;
        }
        i = (int)(s - functionStart);
        s = functionStart;
    } else {
        i = SNPrintF(inBuf, kPrintFBufSize, "<< ERROR: %%s with too many #'s (%d) >>", F->altForm);
        s = inBuf;
    }

    // Make sure we don't truncate in the middle of a UTF-8 character.

    if (F->havePrecision && (i > (int)F->precision)) {
        for (i = (int)F->precision; (i > 0) && ((s[i] & 0xC0) == 0x80); --i) {
        }
    }
    *outStrPtr = s;
    return (i);
}

//===========================================================================================================================
//	PrintFWriteText
//===========================================================================================================================

static int PrintFWriteText(PrintFContext* inContext, PrintFFormat* inFormat, const char* inText, size_t inSize)
{
    int total = 0;
    int err;
    int n;

    n = (int)inSize;
    if (inFormat->prefix != '\0')
        n += 1;
    if (inFormat->suffix != '\0')
        n += 1;

    // Pad on the left.

    if (!inFormat->leftJustify && (n < (int)inFormat->fieldWidth)) {
        do {
            err = inContext->callback(" ", 1, inContext);
            if (err < 0)
                goto error;
            total += 1;

        } while (n < (int)--inFormat->fieldWidth);
    }

    // Write the prefix (if any).

    if (inFormat->prefix != '\0') {
        err = inContext->callback(&inFormat->prefix, 1, inContext);
        if (err < 0)
            goto error;
        total += 1;
    }

    // Write the actual text.

    err = inContext->callback(inText, inSize, inContext);
    if (err < 0)
        goto error;
    total += (int)inSize;

    // Write the suffix (if any).

    if (inFormat->suffix != '\0') {
        err = inContext->callback(&inFormat->suffix, 1, inContext);
        if (err < 0)
            goto error;
        total += 1;
    }

    // Pad on the right.

    for (; n < (int)inFormat->fieldWidth; ++n) {
        err = inContext->callback(" ", 1, inContext);
        if (err < 0)
            goto error;
        total += 1;
    }

    return (total);

error:
    return (err);
}

//===========================================================================================================================
//	PrintFWriteUnicodeString
//===========================================================================================================================

static int PrintFWriteUnicodeString(const uint8_t* inStr, PrintFFormat* inFormat, char* inBuf)
{
    int i;
    const uint8_t* a;
    const uint16_t* u;
    PrintFFormat* F;
    char* p;
    char* q;
    int endianIndex;

    a = inStr;
    F = inFormat;
    if (!F->havePrecision || (F->precision > 0)) {
        if ((a[0] == 0xFE) && (a[1] == 0xFF)) {
            F->altForm = 1;
            a += 2;
            --F->precision;
        } // Big Endian
        else if ((a[0] == 0xFF) && (a[1] == 0xFE)) {
            F->altForm = 2;
            a += 2;
            --F->precision;
        } // Little Endian
    }
    u = (const uint16_t*)a;
    p = inBuf;
    q = p + kPrintFBufSize;
    switch (F->altForm) {
    case 0: // Host Endian
        for (i = 0; (!F->havePrecision || (i < (int)F->precision)) && u[i] && ((q - p) > 0); ++i) {
            *p++ = PrintFMakePrintable(u[i]);
        }
        break;

    case 1: // Big Endian
    case 2: // Little Endian
        endianIndex = 1 - (F->altForm - 1);
        for (i = 0; (!F->havePrecision || (i < (int)F->precision)) && u[i] && ((q - p) > 0); ++i) {
            *p++ = PrintFMakePrintable(a[endianIndex]);
            a += 2;
        }
        break;

    default:
        i = SNPrintF(inBuf, kPrintFBufSize, "<< ERROR: %%S with too many #'s (%d) >>", F->altForm);
        break;
    }
    return (i);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	PrintFCallBackFixedString
//===========================================================================================================================

static int PrintFCallBackFixedString(const char* inStr, size_t inSize, PrintFContext* inContext)
{
    size_t n;

    // If the string is too long, truncate it, but don't truncate in the middle of a UTF-8 character.

    n = inContext->reservedSize - inContext->usedSize;
    if (inSize > n) {
        while ((n > 0) && ((inStr[n] & 0xC0) == 0x80))
            --n;
        inSize = n;
    }

    // Copy the string (excluding any null terminator).

    if (inSize > 0)
        memcpy(inContext->str + inContext->usedSize, inStr, inSize);
    inContext->usedSize += inSize;
    return ((int)inSize);
}

//===========================================================================================================================
//	PrintFCallBackAllocatedString
//===========================================================================================================================

static int PrintFCallBackAllocatedString(const char* inStr, size_t inSize, PrintFContext* inContext)
{
    int result;
    size_t n;

    // If there's not enough room in the buffer, resize it. Amortize allocations by rounding the size up.

    n = inContext->usedSize + inSize;
    if (n > inContext->reservedSize) {
        char* tmp;

        if (n < 256)
            n = 256;
        else
            n = (n + 1023) & ~1023U;

#if (TARGET_NO_REALLOC)
        tmp = (char*)malloc(n);
        require_action(tmp, exit, result = kNoMemoryErr);
        memcpy(tmp, inContext->str, inContext->usedSize);

        free(inContext->str);
        inContext->str = tmp;
        inContext->reservedSize = n;
#else
        tmp = (char*)realloc(inContext->str, n);
        require_action(tmp, exit, result = kNoMemoryErr);
        inContext->str = tmp;
        inContext->reservedSize = n;
#endif
    }

    // Copy the string (excluding any null terminator).

    memcpy(inContext->str + inContext->usedSize, inStr, inSize);
    inContext->usedSize += inSize;
    result = (int)inSize;

exit:
    return (result);
}

//===========================================================================================================================
//	PrintFCallBackUserCallBack
//===========================================================================================================================

static int PrintFCallBackUserCallBack(const char* inStr, size_t inSize, PrintFContext* inContext)
{
    return (inContext->userCallBack(inStr, inSize, inContext->userContext));
}

#if 0
#pragma mark -
#endif

#if (!defined(PRINTF_UTILS_PRINT_TEST))
#if (DEBUG_FPRINTF_ENABLED)
#define PRINTF_UTILS_PRINT_TEST 0
#endif
#endif

#if (!EXCLUDE_UNIT_TESTS)
//===========================================================================================================================
//	PrintFUtils_Test
//===========================================================================================================================

static OSStatus _PrintFTestString(const char* inMatch, const char* inFormat, ...);
static OSStatus _PrintFTestVAList(const char* inMatch, const char* inFormat, ...);

#define PFTEST1(MATCH, FORMAT, PARAM)                        \
    do {                                                     \
        err = _PrintFTestString((MATCH), (FORMAT), (PARAM)); \
        require_noerr(err, exit);                            \
                                                             \
    } while (0)

#define PFTEST2(MATCH, FORMAT, PARAM1, PARAM2)                          \
    do {                                                                \
        err = _PrintFTestString((MATCH), (FORMAT), (PARAM1), (PARAM2)); \
        require_noerr(err, exit);                                       \
                                                                        \
    } while (0)

OSStatus PrintFUtils_Test(void)
{
    OSStatus err;
    int n;
    char buf[512];
    const char* src;
    const char* end;
    char* dst;
    char* lim;

    // Field Width and Precision Tests.

    PFTEST1(":hello, world:", ":%s:", "hello, world");
    PFTEST1(":hello, world:", ":%10s:", "hello, world");
    PFTEST1(":hello, wor:", ":%.10s:", "hello, world");
    PFTEST1(":hello, world:", ":%-10s:", "hello, world");
    PFTEST1(":hello, world:", ":%.15s:", "hello, world");
    PFTEST1(":hello, world   :", ":%-15s:", "hello, world");
    PFTEST1(":   hello, world:", ":%15s:", "hello, world");
    PFTEST1(":     hello, wor:", ":%15.10s:", "hello, world");
    PFTEST1(":hello, wor     :", ":%-15.10s:", "hello, world");

    PFTEST1(":'hello, world':", ":%'s:", "hello, world");
    PFTEST1(":\"hello, world\":", ":%''s:", "hello, world");
    PFTEST1(":'hello, wor':", ":%'.12s:", "hello, world");
    PFTEST1(":   'hello, world':", ":%'17s:", "hello, world");
    PFTEST1(":'h':", ":%'.3s:", "hello, world");
    PFTEST1(":'':", ":%'.2s:", "hello, world");
    PFTEST1("::", ":%'.1s:", "hello, world");

    // Number Tests.

    PFTEST1("1234", "%d", 1234);
    PFTEST1("1234", "%#d", 1234);
    PFTEST1("0", "%'d", 0);
    PFTEST1("1", "%'d", 1);
    PFTEST1("12", "%'d", 12);
    PFTEST1("123", "%'d", 123);
    PFTEST1("1,234", "%'d", 1234);
    PFTEST1("12,345", "%'d", 12345);
    PFTEST1("123,456", "%'d", 123456);
    PFTEST1("1,234,567", "%'d", 1234567);
    PFTEST1("-1", "%'d", -1);
    PFTEST1("-12", "%'d", -12);
    PFTEST1("-123", "%'d", -123);
    PFTEST1("-1,234", "%'d", -1234);
    PFTEST1("-12,345", "%'d", -12345);
    PFTEST1("-123,456", "%'d", -123456);
    PFTEST1("-1,234,567", "%'d", -1234567);
    PFTEST1("1234", "%i", 1234);
    PFTEST1("1234", "%u", 1234);
    PFTEST1("2322", "%o", 1234);
    PFTEST1("02322", "%#o", 1234);
    PFTEST1("0777", "%#2o", 0777);
    PFTEST1("12AB", "%X", 0x12AB);
    PFTEST1("12ab", "%x", 0x12AB);
    PFTEST1("0x12ab", "%#x", 0x12AB);
    PFTEST1("0x1", "%#01x", 0x1);
    PFTEST1("0x01", "%#04x", 0x1);
    PFTEST1("1001010101011", "%b", 0x12AB);
    PFTEST1(" 1234", "%5d", 1234);
    PFTEST1("1234 ", "%-5d", 1234);
    PFTEST1("2147483647", "%ld", 2147483647L);
    PFTEST1("4294967295", "%lu", (unsigned long)UINT32_C(4294967295));
    PFTEST1("9223372036854775807", "%lld", INT64_C(9223372036854775807));
    PFTEST1("-9223372036854775807", "%lld", INT64_C(-9223372036854775807));
    PFTEST1("18446744073709551615", "%llu", UINT64_C(18446744073709551615));
    PFTEST1("-46", "%hhd", 1234);
    PFTEST1("210", "%hhu", 1234);
    PFTEST1("12345678", "%jX", (intmax_t)0x12345678);
    PFTEST1("12345678", "%zX", (size_t)0x12345678);
    PFTEST1("305419896", "%zd", (size_t)0x12345678);
    PFTEST1("12345678", "%tX", (ptrdiff_t)0x12345678);
    PFTEST1("1111011011110110111101101111011", "%lb", (unsigned long)0x7B7B7B7B);

#if (PRINTF_ENABLE_FLOATING_POINT)
    PFTEST1("123.234000", "%f", 123.234);
    PFTEST1("123.23", "%.2f", 123.234);
    PFTEST1(" 123.2340000000", "%15.10f", 123.234);
    PFTEST1("123.2340000000 ", "%-15.10f", 123.234);
#endif

    // Bit Number Tests.

    PFTEST1("12 9 5 4 2", "%##b", 0x1234);
    PFTEST1("19 22 26 27 29", "%###b", 0x1234);
    PFTEST1("3 6 10 11 13", "%###hb", 0x1234);
    PFTEST1("4 1", "%##.8lb", UINT32_C(0x12));
    PFTEST1("59 62", "%###llb", UINT64_C(0x12));
    PFTEST1("4 1", "%##.8llb", UINT64_C(0x12));
    PFTEST1("3 6", "%###.8llb", UINT64_C(0x12));
    PFTEST1("", "%###.0llb", UINT64_C(0x12));
    PFTEST1("ERROR: << precision must be 0-64 >>", "%###.128llb", UINT64_C(0x12));
    PFTEST1("31 0", "%##b", 0x80000001U);
    PFTEST1("0 31", "%###b", 0x80000001U);
    PFTEST1("31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0", "%##b", 0xFFFFFFFFU);
    PFTEST1("0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31", "%###b", 0xFFFFFFFFU);
    PFTEST1("4 1 0     ", "%-##10b", 0x13);
    PFTEST1("     4 1 0", "%##10b", 0x13);

    // String Tests.

    PFTEST1("test", "%#s", "\04test");
    PFTEST1("www.apple.com.", "%##s", "\03www\05apple\03com");

    // Address Tests.

    PFTEST1("1.2.3.4", "%.2a", "\x12\x34");
    PFTEST1("17.205.123.5", "%.4a", "\x11\xCD\x7B\x05");
    PFTEST1("00:11:22:AA:BB:CC", "%.6a", "\x00\x11\x22\xAA\xBB\xCC");
    PFTEST1("00:11:22:AA:BB:CC:56:78", "%.8a", "\x00\x11\x22\xAA\xBB\xCC\x56\x78");
    PFTEST1("102:304:506:708:90a:b0c:d0e:f10", "%.16a", "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10");

    {
        mDNSAddrCompat maddr;

        memset(&maddr, 0, sizeof(maddr));
        maddr.type = 4;
        maddr.ip.v4[0] = 127;
        maddr.ip.v4[1] = 0;
        maddr.ip.v4[2] = 0;
        maddr.ip.v4[3] = 1;
        PFTEST1("127.0.0.1", "%#a", &maddr);

        memset(&maddr, 0, sizeof(maddr));
        maddr.type = 6;
        maddr.ip.v6[0] = 0xFE;
        maddr.ip.v6[1] = 0x80;
        maddr.ip.v6[15] = 0x01;
        PFTEST1("fe80::1", "%#a", &maddr);
    }

#if (defined(AF_INET))
    {
        struct sockaddr_in sa4;

        memset(&sa4, 0, sizeof(sa4));
        SIN_LEN_SET(&sa4);
        sa4.sin_family = AF_INET;
        sa4.sin_port = hton16(80);
        sa4.sin_addr.s_addr = hton32(UINT32_C(0x7f000001));

        PFTEST1("127.0.0.1:80", "%##a", &sa4);
    }
#endif

#if (defined(AF_INET6))
    {
        struct sockaddr_in6 sa6;

        memset(&sa6, 0, sizeof(sa6));
        SIN6_LEN_SET(&sa6);
        sa6.sin6_family = AF_INET6;
        sa6.sin6_port = hton16(80);
        sa6.sin6_addr.s6_addr[0] = 0xFE;
        sa6.sin6_addr.s6_addr[1] = 0x80;
        sa6.sin6_addr.s6_addr[15] = 0x01;
        sa6.sin6_scope_id = 2;

#if (TARGET_OS_POSIX)
        {
            char ifname[IF_NAMESIZE];

            SNPrintF(buf, sizeof(buf), "[fe80::1%%%s]:80", if_indextoname(sa6.sin6_scope_id, ifname));
            PFTEST1(buf, "%##a", &sa6);
        }
#else
        PFTEST1("[fe80::1%2]:80", "%##a", &sa6);
#endif

        memset(&sa6, 0, sizeof(sa6));
        SIN6_LEN_SET(&sa6);
        sa6.sin6_family = AF_INET6;
        sa6.sin6_port = hton16(80);
        sa6.sin6_addr.s6_addr[10] = 0xFF; // ::ffff:<32-bit IPv4 address>
        sa6.sin6_addr.s6_addr[11] = 0xFF;
        memcpy(&sa6.sin6_addr.s6_addr[12], "\xE8\x05\x0F\x49", 4); // IPv4 address is in the low 32 bits of the IPv6 address.

        PFTEST1("[::ffff:232.5.15.73]:80", "%##a", &sa6);
    }
#endif

    // Unicode Tests.

    PFTEST2("tes", "%.*s", 4, "tes");
    PFTEST2("test", "%.*s", 4, "test");
    PFTEST2("test", "%.*s", 4, "testing");
    PFTEST2("te\xC3\xA9", "%.*s", 4, "te\xC3\xA9");
    PFTEST2("te\xC3\xA9", "%.*s", 4, "te\xC3\xA9ing");
    PFTEST2("tes", "%.*s", 4, "tes\xC3\xA9ing");
    PFTEST2("t\xE3\x82\xBA", "%.*s", 4, "t\xE3\x82\xBA");
    PFTEST2("t\xE3\x82\xBA", "%.*s", 4, "t\xE3\x82\xBAing");
    PFTEST2("te", "%.*s", 4, "te\xE3\x82\xBA");
    PFTEST2("te", "%.*s", 4, "te\xE3\x82\xBAing");
    PFTEST2("te\xC3\xA9\xE3\x82\xBA", "%.*s", 7, "te\xC3\xA9\xE3\x82\xBAing");
    PFTEST2("te\xC3\xA9", "%.*s", 6, "te\xC3\xA9\xE3\x82\xBAing");
    PFTEST2("te\xC3\xA9", "%.*s", 5, "te\xC3\xA9\xE3\x82\xBAing");
#if (TARGET_RT_BIG_ENDIAN)
    PFTEST1("abcd", "%S", "\x00"
                          "a"
                          "\x00"
                          "b"
                          "\x00"
                          "c"
                          "\x00"
                          "d"
                          "\x00"
                          "\x00");
#else
    PFTEST1("abcd", "%S", "a"
                          "\x00"
                          "b"
                          "\x00"
                          "c"
                          "\x00"
                          "d"
                          "\x00"
                          "\x00"
                          "\x00");
#endif
    PFTEST1("abcd", "%S",
        "\xFE\xFF"
        "\x00"
        "a"
        "\x00"
        "b"
        "\x00"
        "c"
        "\x00"
        "d"
        "\x00"
        "\x00"); // Big Endian BOM
    PFTEST1("abcd", "%S",
        "\xFF\xFE"
        "a"
        "\x00"
        "b"
        "\x00"
        "c"
        "\x00"
        "d"
        "\x00"
        "\x00"
        "\x00"); // Little Endian BOM
    PFTEST1("abcd", "%#S", "\x00"
                           "a"
                           "\x00"
                           "b"
                           "\x00"
                           "c"
                           "\x00"
                           "d"
                           "\x00"
                           "\x00"); // Big Endian
    PFTEST1("abcd", "%##S", "a"
                            "\x00"
                            "b"
                            "\x00"
                            "c"
                            "\x00"
                            "d"
                            "\x00"
                            "\x00"
                            "\x00"); // Little Endian
    PFTEST2("abc", "%.*S",
        4, "\xFE\xFF"
           "\x00"
           "a"
           "\x00"
           "b"
           "\x00"
           "c"
           "\x00"
           "d"); // Big Endian BOM
    PFTEST2("abc", "%.*S",
        4, "\xFF\xFE"
           "a"
           "\x00"
           "b"
           "\x00"
           "c"
           "\x00"
           "d"
           "\x00"); // Little Endian BOM
#if (TARGET_RT_BIG_ENDIAN)
    PFTEST2("abc", "%.*S", 3, "\x00"
                              "a"
                              "\x00"
                              "b"
                              "\x00"
                              "c"
                              "\x00"
                              "d");
#else
    PFTEST2("abc", "%.*S", 3, "a"
                              "\x00"
                              "b"
                              "\x00"
                              "c"
                              "\x00"
                              "d"
                              "\x00");
#endif
    PFTEST2("abc", "%#.*S", 3, "\x00"
                               "a"
                               "\x00"
                               "b"
                               "\x00"
                               "c"
                               "\x00"
                               "d"); // Big Endian
    PFTEST2("abc", "%##.*S", 3, "a"
                                "\x00"
                                "b"
                                "\x00"
                                "c"
                                "\x00"
                                "d"
                                "\x00"); // Little Endian

    // Other Tests.

    PFTEST1("a", "%c", 'a');
    PFTEST1("'a'", "%'c", 'a');
    PFTEST1("AbCd", "%C", 0x41624364 /* 'AbCd' */);
    PFTEST1("'AbCd'", "%'C", 0x41624364 /* 'AbCd' */);
    PFTEST1("6ba7b810-9dad-11d1-80b4-00c04fd430c8", "%U", "\x10\xb8\xa7\x6b"
                                                          "\xad\x9d"
                                                          "\xd1\x11"
                                                          "\x80\xb4"
                                                          "\x00\xc0\x4f\xd4\x30\xc8");
    PFTEST1("10b8a76b-ad9d-d111-80b4-00c04fd430c8", "%#U", "\x10\xb8\xa7\x6b"
                                                           "\xad\x9d"
                                                           "\xd1\x11"
                                                           "\x80\xb4"
                                                           "\x00\xc0\x4f\xd4\x30\xc8");

#if (DEBUG || DEBUG_EXPORT_ERROR_STRINGS)
    PFTEST1("noErr", "%m", 0);
    PFTEST1("kUnknownErr", "%m", kUnknownErr);
    PFTEST1("-6700/0xFFFFE5D4 kUnknownErr", "%#m", kUnknownErr);
#endif

    err = _PrintFTestString(
        "6b a7 b8 10 9d ad 11 d1 80 b4 00 c0 4f d4 30 c8",
        "%H",
        "\x6b\xa7\xb8\x10\x9d\xad\x11\xd1\x80\xb4\x00\xc0\x4f\xd4\x30\xc8", 16, 16);
    require_noerr(err, exit);

    err = _PrintFTestString(
        "6b a7 b8 10 9d ad 11 d1 80 b4 00 c0 4f d4 30 c8 "
        "6b a7 b8 10 9d ad 11 d1 80 b4 00 c0 4f d4 30 c8",
        "%H",
        "\x6b\xa7\xb8\x10\x9d\xad\x11\xd1\x80\xb4\x00\xc0\x4f\xd4\x30\xc8"
        "\x6b\xa7\xb8\x10\x9d\xad\x11\xd1\x80\xb4\x00\xc0\x4f\xd4\x30\xc8",
        32, 32);
    require_noerr(err, exit);

    err = _PrintFTestString("6b a7", "%H", "\x6b\xa7", 2, 2);
    require_noerr(err, exit);

    err = _PrintFTestString("0123456789abcdef", "%.3H", "\x01\x23\x45\x67\x89\xab\xcd\xef", 8, 8);
    require_noerr(err, exit);

    err = _PrintFTestString("0123456789ABCDEF", "%.4H", "\x01\x23\x45\x67\x89\xAB\xCD\xEF", 8, 8);
    require_noerr(err, exit);

#if (DEBUG_CF_OBJECTS_ENABLED)
    {
        CFNumberRef num;
        int64_t s64;

        PFTEST1(":hello, world:", ":%@:", CFSTR("hello, world"));
        PFTEST1(":hello, wor     :", ":%-15.10@:", CFSTR("hello, world"));
        PFTEST1(":   hello, world:", ":%15@:", CFSTR("hello, world"));

        s64 = INT64_C(9223372036854775807);
        num = CFNumberCreate(NULL, kCFNumberSInt64Type, &s64);
        require_action(num, exit, err = kNoMemoryErr);

        err = _PrintFTestString("9223372036854775807", "%@", num);
        CFRelease(num);
        require_noerr(err, exit);
    }
#endif

    PFTEST1("%5", "%%%d", 5);

    err = _PrintFTestString("Test 123 hello, world", "Test %d %s%n", 123, "hello, world", &n);
    require_noerr(err, exit);
    require_action(n == 21, exit, err = kResponseErr);

    err = _PrintFTestVAList("begin 123 test 456 end", "%d test %s%n", 123, "456", &n);
    require_noerr(err, exit);
    require_action(n == 12, exit, err = kResponseErr);

    PFTEST1("a", "%c", 'a');

    err = _PrintFTestString(
        "Line 1", "%{text}",
        "Line 1", kSizeCString);
    require_noerr(err, exit);

    err = _PrintFTestString(
        "Line 1\n"
        "Line 2\n"
        "Line 3",
        "%{text}",
        "Line 1\n"
        "Line 2\n"
        "Line 3",
        kSizeCString);
    require_noerr(err, exit);

    err = _PrintFTestString(
        "Line 1\n"
        "Line 2\n"
        "Line 3\n"
        "\n"
        "\n"
        "\n"
        "Line 7\n",
        "%{text}",
        "Line 1\r"
        "Line 2\r\n"
        "Line 3\n"
        "\n"
        "\r"
        "\r\n"
        "Line 7\n",
        kSizeCString);
    require_noerr(err, exit);

    err = _PrintFTestString(
        "    Line 1\n"
        "    Line 2\n"
        "    Line 3\n"
        "\n"
        "\n"
        "\n"
        "    Line 7\n",
        "%1{text}",
        "Line 1\r"
        "Line 2\r\n"
        "Line 3\n"
        "\n"
        "\r"
        "\r\n"
        "Line 7\n",
        kSizeCString);
    require_noerr(err, exit);

    err = _PrintFTestString(":abc123:", ":%?s%?d%?d%?s:", true, "abc", true, 123, false, 456, false, "xyz");
    require_noerr(err, exit);

    // Bounds Tests.

    memset(buf, 'Z', sizeof(buf));
    n = SNPrintF(buf, 0, "%s", "test");
    require_action(n == 4, exit, err = kResponseErr);
    src = buf;
    end = buf + sizeof(buf);
    while ((src < end) && (*src == 'Z'))
        ++src;
    require_action(src == end, exit, err = kImmutableErr);

    memset(buf, 'Z', sizeof(buf));
    n = SNPrintF(buf, 3, "%s", "test");
    require_action(n == 4, exit, err = kResponseErr);
    require_action(buf[2] == '\0', exit, err = kResponseErr);
    require_action(strcmp(buf, "te") == 0, exit, err = kResponseErr);
    src = buf + 3;
    end = buf + sizeof(buf);
    while ((src < end) && (*src == 'Z'))
        ++src;
    require_action(src == end, exit, err = kImmutableErr);

    memset(buf, 'Z', sizeof(buf));
    n = SNPrintF(buf, 4, "%s", "te\xC3\xA9");
    require_action(n == 4, exit, err = kResponseErr);
    require_action(buf[2] == '\0', exit, err = kResponseErr);
    require_action(strcmp(buf, "te") == 0, exit, err = kResponseErr);
    src = buf + 3;
    end = buf + sizeof(buf);
    while ((src < end) && (*src == 'Z'))
        ++src;
    require_action(src == end, exit, err = kImmutableErr);

    // SNPrintF_Add

    dst = buf;
    lim = dst + 10;
    err = SNPrintF_Add(&dst, lim, "12345");
    require_noerr(err, exit);
    require_action(strcmp(buf, "12345") == 0, exit, err = -1);

    err = SNPrintF_Add(&dst, lim, "67890");
    require_action(err != kNoErr, exit, err = -1);
    require_action(strcmp(buf, "123456789") == 0, exit, err = -1);

    err = SNPrintF_Add(&dst, lim, "ABCDE");
    require_action(err != kNoErr, exit, err = -1);
    require_action(strcmp(buf, "123456789") == 0, exit, err = -1);

    dst = buf;
    lim = dst + 10;
    err = SNPrintF_Add(&dst, lim, "12345");
    require_noerr(err, exit);
    require_action(strcmp(buf, "12345") == 0, exit, err = -1);

    err = SNPrintF_Add(&dst, lim, "6789");
    require_noerr(err, exit);
    require_action(strcmp(buf, "123456789") == 0, exit, err = -1);

    err = SNPrintF_Add(&dst, lim, "ABCDE");
    require_action(err != kNoErr, exit, err = -1);
    require_action(strcmp(buf, "123456789") == 0, exit, err = -1);

    memcpy(buf, "abc", 4);
    dst = buf;
    lim = dst;
    err = SNPrintF_Add(&dst, lim, "12345");
    require_action(err != kNoErr, exit, err = -1);
    require_action(strcmp(lim, "abc") == 0, exit, err = -1);

    err = kNoErr;

exit:
    printf("PrintFUtils: %s\n", !err ? "PASSED" : "FAILED");
    return (err);
}

//===========================================================================================================================
//	_PrintFTestString
//===========================================================================================================================

static OSStatus _PrintFTestString(const char* inMatch, const char* inFormat, ...)
{
    OSStatus err;
    size_t size;
    int n;
    va_list args;
    char buf[512];
    const char* src;
    const char* end;

    memset(buf, 'Z', sizeof(buf));
    va_start(args, inFormat);
    n = VSNPrintF(buf, sizeof(buf), inFormat, args);
    va_end(args);

#if (PRINTF_UTILS_PRINT_TEST)
    printf("\"%.*s\"\n", (int)sizeof(buf), buf);
#endif

    size = strlen(inMatch);
    require_action_quiet(n == (int)size, exit, err = kSizeErr);
    require_action_quiet(buf[n] == '\0', exit, err = kOverrunErr);
    require_action_quiet(strcmp(buf, inMatch) == 0, exit, err = kMismatchErr);

    src = &buf[n + 1];
    end = buf + sizeof(buf);
    while ((src < end) && (*src == 'Z'))
        ++src;
    require_action_quiet(src == end, exit, err = kImmutableErr);
    err = kNoErr;

exit:
    if (err)
        printf("### Bad PrintF output:\n'%s'\nExpected:\n'%s'", buf, inMatch);
    return (err);
}

//===========================================================================================================================
//	_PrintFTestVAList
//===========================================================================================================================

static OSStatus _PrintFTestVAList(const char* inMatch, const char* inFormat, ...)
{
    OSStatus err;
    va_list args;

    va_start(args, inFormat);
    err = _PrintFTestString(inMatch, "begin %V end", inFormat, &args);
    va_end(args);
    return (err);
}

#endif // !EXCLUDE_UNIT_TESTS
