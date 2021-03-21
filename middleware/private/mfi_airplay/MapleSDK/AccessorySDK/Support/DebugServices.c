/*
	Copyright (C) 1997-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#if 0
#pragma mark == Includes ==
#endif

//===========================================================================================================================
//	Includes
//===========================================================================================================================

// Microsoft deprecated standard C APIs like fopen so disable those warnings because the replacement APIs are not portable.

#if (!defined(_CRT_SECURE_NO_DEPRECATE))
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#include "DebugServices.h"
#include "CommonServices.h"

#if (TARGET_HAS_STD_C_LIB)
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#endif

#if (TARGET_OS_DARWIN)
#if (TARGET_KERNEL)
#include <libkern/OSDebug.h>
#include <mach/mach_vm.h>
#include <mach/vm_map.h>
#else
#include <dlfcn.h>
#include <execinfo.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/sysctl.h>

#if (!COMMON_SERVICES_NO_CORE_SERVICES)
#include <CoreAudio/CoreAudio.h>
#include <dispatch/dispatch.h>
#include <xpc/xpc.h>
#endif
#if (!TARGET_IPHONE_SIMULATOR && !COMMON_SERVICES_NO_CORE_SERVICES)
#include <CoreSymbolication/CoreSymbolication.h>
#endif
#endif
#include <sys/time.h>
#endif

#if (TARGET_OS_NETBSD && TARGET_KERNEL)
#include <machine/db_machdep.h>

#include <ddb/db_interface.h>
#endif

#if (TARGET_OS_LINUX && !TARGET_OS_ANDROID)
#include <execinfo.h>
#endif

#if (TARGET_OS_POSIX)
#include <sys/types.h>

#include <fcntl.h>
#include <net/if.h>
#if (TARGET_OS_BSD)
#include <net/if_dl.h>
#endif
#include <pthread.h>
#include <signal.h>
#include <syslog.h>
#if (TARGET_OS_ANDROID)
#include <linux/sysctl.h>
#else
#include <sys/sysctl.h>
#endif
#include <sys/stat.h>
#include <sys/time.h>
#if (!TARGET_OS_QNX)
#include <sys/user.h>
#endif
#include <unistd.h>
#endif

#if (TARGET_OS_QNX)
#include <backtrace.h>
#include <devctl.h>
#include <sys/neutrino.h>
#include <sys/procfs.h>
#endif

#if (TARGET_OS_VXWORKS)
#include "config.h"

#include <arch/ppc/vxPpcLib.h>
#include <dbgLib.h>
#include <intLib.h>
#include <msgQLib.h>
#include <netinet/in.h>
#include <netShow.h>
#include <rebootLib.h>
#include <semLib.h>
#include <sysLib.h>
#include <taskLib.h>
#include <taskHookLib.h>
#include <tickLib.h>
#include <vxLib.h>
#include <usrLib.h>
#endif

#if (TARGET_OS_WINDOWS && !TARGET_OS_WINDOWS_CE)
#include <direct.h>
#include <fcntl.h>
#include <io.h>

#include <Dbghelp.h>
#endif

#if (DEBUG_CF_OBJECTS_ENABLED)
#include CF_HEADER
#endif

#if (!DEBUG_SERVICES_LITE)
#include "PrintFUtils.h"
#include "StringUtils.h"
#include "TickUtils.h"
#endif

#if (COMPILER_ARM_REALVIEW)
// Disable RealView "warning #111-D: statement is unreachable" for debug code.
// Disable RealView "warning #236-D: controlling expression is constant" for debug code.

#pragma diag_suppress 111, 236
#endif

#if 0
#pragma mark == Globals ==
#endif

//===========================================================================================================================
//	Globals
//===========================================================================================================================

#if ((TARGET_OS_DARWIN && !TARGET_OS_DARWIN_KERNEL) || TARGET_OS_WINDOWS)
#define kDebugDefaultBreakLevel kLogLevelAssert
#else
#define kDebugDefaultBreakLevel kLogLevelOff // Don't enter the debugger by default on embedded platforms.
#endif

#define kDebugDefaultStackTraceLevel kLogLevelAssert

#if (LOGUTILS_ENABLED)
ulog_define(DebugServicesAssert, kLogLevelAll, kLogFlags_PrintTime, "", NULL);
ulog_define(DebugServicesBreak, kDebugDefaultBreakLevel, kLogFlags_PrintTime, "", NULL);
ulog_define(DebugServicesLogging, kLogLevelInfo + 1, kLogFlags_PrintTime, "", NULL);
ulog_define(DebugServicesStackTrace, kDebugDefaultStackTraceLevel, kLogFlags_PrintTime, "", NULL);
#else
LogLevel gDebugServicesLevel = kLogLevelInfo + 1;
#endif

#if (!LOGUTILS_ENABLED)
#define dbs_ulog(LEVEL, ...) printf(__VA_ARGS__)
#elif (TARGET_HAS_C99_VA_ARGS)
#define dbs_ulog(LEVEL, ...) ulog(&log_category_from_name(DebugServicesLogging), (LEVEL), __VA_ARGS__)
#elif (TARGET_HAS_GNU_VA_ARGS)
#define dbs_ulog(LEVEL, ARGS...) ulog(&log_category_from_name(DebugServicesLogging), (LEVEL), ##ARGS)
#else
#define dbs_ulog DebugLog_C89 // No VA_ARG macros so we have to do it from a real function.
#endif

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	DebugLog_C89
//===========================================================================================================================

#if (!TARGET_HAS_VA_ARG_MACROS)
int DebugLog_C89(const char* inFunction, LogLevel inLevel, const char* inFormat, ...)
{
    if (log_category_ptr_enabled(&log_category_from_name(DebugServicesLogging), inLevel)) {
        int n;
        va_list args;

        va_start(args, inFormat);
        n = LogPrintV(&log_category_from_name(DebugServicesLogging), inFunction, inLevel, inFormat, args);
        va_end(args);
        return (n);
    }
    return (0);
}
#endif

#if (DEBUG_SERVICES_LITE)
//===========================================================================================================================
//	DebugPrintAssert
//===========================================================================================================================

void DebugPrintAssert(
    DebugAssertFlags inFlags,
    OSStatus inErrorCode,
    const char* inAssertString,
    const char* inFilename,
    long inLineNumber,
    const char* inFunction,
    const char* inMessageFormat,
    ...)
{
    va_list args;
    const char* ptr;
    char c;

    // Strip off any parent folder path in the filename.

    if (inFilename) {
        for (ptr = inFilename; (c = *ptr) != '\0'; ++ptr) {
            if ((c == '/') || (c == '\\')) {
                inFilename = ptr + 1;
            }
        }
    }

    // Print out the assert.

    if (!inFilename)
        inFilename = "";
    if (!inFunction)
        inFunction = "";

    va_start(args, inMessageFormat);
    if (inErrorCode != 0) {
        printf("### [ASSERT] %s:%ld \"%s\", %d %s\n", inFilename, inLineNumber, inFunction, (int)inErrorCode,
            inMessageFormat ? inMessageFormat : "");
    } else {
        printf("### [ASSERT] %s:%ld \"%s\", \"%s\" %s\n",
            inFilename, inLineNumber, inFunction, inAssertString ? inAssertString : "",
            inMessageFormat ? inMessageFormat : "");
    }
    va_end(args);

    // If this is a panic assert, try to stop.

    if (DebugIsDebuggerPresent()) {
        DebugEnterDebugger(false);
    } else if (inFlags & kDebugAssertFlagsPanic) {
#if (TARGET_OS_VXWORKS)
        taskSuspend(taskIdSelf());
#elif (TARGET_OS_WINDOWS)
        SuspendThread(GetCurrentThread());
#else
        for (;;) {
        }
#endif
    }
}
#else
//===========================================================================================================================
//	DebugPrintAssert
//===========================================================================================================================

void DebugPrintAssert(
    DebugAssertFlags inFlags,
    OSStatus inErrorCode,
    const char* inAssertString,
    const char* inFilename,
    long inLineNumber,
    const char* inFunction,
    const char* inMessageFormat,
    ...)
{
    va_list args;
    const char* ptr;
    char c;
    char* stackTrace = NULL;

    if (!log_category_enabled(&log_category_from_name(DebugServicesAssert), kLogLevelAssert)) {
        return;
    }

    // Strip off any parent folder path in the filename.

    if (inFilename) {
        for (ptr = inFilename; (c = *ptr) != '\0'; ++ptr) {
            if ((c == '/') || (c == '\\')) {
                inFilename = ptr + 1;
            }
        }
    }

    // Print out the assert.

    if (!inFilename)
        inFilename = "";
    if (!inFunction)
        inFunction = "";

#if ((TARGET_OS_DARWIN && !TARGET_KERNEL) || TARGET_OS_LINUX)
    if ((inFlags & kDebugAssertFlagsPanic) || log_category_enabled(&log_category_from_name(DebugServicesStackTrace), kLogLevelAssert)) {
        stackTrace = DebugCopyStackTrace(NULL);
    }
#endif

    va_start(args, inMessageFormat);
    if (inErrorCode != 0) {
        dbs_ulog(kLogLevelAssert, "### [ASSERT] %s:%ld \"%###s\", %#m %V\n%s",
            inFilename, inLineNumber, inFunction,
            inErrorCode, inMessageFormat ? inMessageFormat : "", &args, stackTrace ? stackTrace : "");
    } else {
        dbs_ulog(kLogLevelAssert, "### [ASSERT] %s:%ld \"%###s\", \"%s\" %V\n%s",
            inFilename, inLineNumber, inFunction, inAssertString ? inAssertString : "",
            inMessageFormat ? inMessageFormat : "", &args, stackTrace ? stackTrace : "");
    }
    va_end(args);

    // If this is a panic assert, try to stop.

    if (inFlags & kDebugAssertFlagsPanic) {
        if (DebugIsDebuggerPresent()) {
            DebugEnterDebugger(true);
        } else {
#if (TARGET_OS_VXWORKS)
            taskSuspend(taskIdSelf());
#elif (TARGET_OS_WINDOWS)
            SuspendThread(GetCurrentThread());
#else
            for (;;) {
            }
#endif
        }
    } else {
        if (log_category_enabled(&log_category_from_name(DebugServicesBreak), kLogLevelAssert) && DebugIsDebuggerPresent()) {
            DebugEnterDebugger(false);
        }
    }
    FreeNullSafe(stackTrace);
}
#endif // DEBUG_SERVICES_LITE

#if (DEBUG || DEBUG_EXPORT_ERROR_STRINGS)
//===========================================================================================================================
//	DebugErrors
//===========================================================================================================================

#define CaseErrorString(X, STR) \
    {                           \
        (X), (STR)              \
    }
#define CaseErrorStringify(X) \
    {                         \
        (X), #X               \
    }
#define CaseErrorStringifyHardCode(VALUE, X) \
    {                                        \
        (OSStatus)(VALUE), #X                \
    }
#if (TARGET_RT_64_BIT)
#define CaseEFIErrorString(X, STR)                            \
    { (OSStatus)((X) | UINT64_C(0x8000000000000000)), #STR }, \
    {                                                         \
        (OSStatus)((X) | 0x80000000U), #STR                   \
    }
#define CaseEFIError2String(X, STR)                           \
    { (OSStatus)((X) | UINT64_C(0xC000000000000000)), #STR }, \
    {                                                         \
        (OSStatus)((X) | 0xC0000000U), #STR                   \
    }
#else
#define CaseEFIErrorString(X, STR)          \
    {                                       \
        (OSStatus)((X) | 0x80000000U), #STR \
    }
#define CaseEFIError2String(X, STR)         \
    {                                       \
        (OSStatus)((X) | 0xC0000000U), #STR \
    }
#endif
#define CaseEnd() \
    {             \
        0, NULL   \
    }

typedef struct
{
    OSStatus err;
    const char* str;

} DebugErrorEntry;

static const DebugErrorEntry kDebugErrors[] = {
    // General Errors

    CaseErrorString(0, "noErr"),
    CaseErrorString(-1, "EPERM / generic-error"),

    // Common Services Errors

    CaseErrorStringify(kUnknownErr),
    CaseErrorStringify(kOptionErr),
    CaseErrorStringify(kSelectorErr),
    CaseErrorStringify(kExecutionStateErr),
    CaseErrorStringify(kPathErr),
    CaseErrorStringify(kParamErr),
    CaseErrorStringify(kUserRequiredErr),
    CaseErrorStringify(kCommandErr),
    CaseErrorStringify(kIDErr),
    CaseErrorStringify(kStateErr),
    CaseErrorStringify(kRangeErr),
    CaseErrorStringify(kRequestErr),
    CaseErrorStringify(kResponseErr),
    CaseErrorStringify(kChecksumErr),
    CaseErrorStringify(kNotHandledErr),
    CaseErrorStringify(kVersionErr),
    CaseErrorStringify(kSignatureErr),
    CaseErrorStringify(kFormatErr),
    CaseErrorStringify(kNotInitializedErr),
    CaseErrorStringify(kAlreadyInitializedErr),
    CaseErrorStringify(kNotInUseErr),
    CaseErrorStringify(kAlreadyInUseErr),
    CaseErrorStringify(kTimeoutErr),
    CaseErrorStringify(kCanceledErr),
    CaseErrorStringify(kAlreadyCanceledErr),
    CaseErrorStringify(kCannotCancelErr),
    CaseErrorStringify(kDeletedErr),
    CaseErrorStringify(kNotFoundErr),
    CaseErrorStringify(kNoMemoryErr),
    CaseErrorStringify(kNoResourcesErr),
    CaseErrorStringify(kDuplicateErr),
    CaseErrorStringify(kImmutableErr),
    CaseErrorStringify(kUnsupportedDataErr),
    CaseErrorStringify(kIntegrityErr),
    CaseErrorStringify(kIncompatibleErr),
    CaseErrorStringify(kUnsupportedErr),
    CaseErrorStringify(kUnexpectedErr),
    CaseErrorStringify(kValueErr),
    CaseErrorStringify(kNotReadableErr),
    CaseErrorStringify(kNotWritableErr),
    CaseErrorStringify(kBadReferenceErr),
    CaseErrorStringify(kFlagErr),
    CaseErrorStringify(kMalformedErr),
    CaseErrorStringify(kSizeErr),
    CaseErrorStringify(kNameErr),
    CaseErrorStringify(kNotPreparedErr),
    CaseErrorStringify(kReadErr),
    CaseErrorStringify(kWriteErr),
    CaseErrorStringify(kMismatchErr),
    CaseErrorStringify(kDateErr),
    CaseErrorStringify(kUnderrunErr),
    CaseErrorStringify(kOverrunErr),
    CaseErrorStringify(kEndingErr),
    CaseErrorStringify(kConnectionErr),
    CaseErrorStringify(kAuthenticationErr),
    CaseErrorStringify(kOpenErr),
    CaseErrorStringify(kTypeErr),
    CaseErrorStringify(kSkipErr),
    CaseErrorStringify(kNoAckErr),
    CaseErrorStringify(kCollisionErr),
    CaseErrorStringify(kBackoffErr),
    CaseErrorStringify(kAddressErr),
    CaseErrorStringify(kInternalErr),
    CaseErrorStringify(kNoSpaceErr),
    CaseErrorStringify(kCountErr),
    CaseErrorStringify(kEndOfDataErr),
    CaseErrorStringify(kWouldBlockErr),
    CaseErrorStringify(kLookErr),
    CaseErrorStringify(kSecurityRequiredErr),
    CaseErrorStringify(kOrderErr),
    CaseErrorStringify(kUpgradeErr),
    CaseErrorStringify(kAsyncNoErr),
    CaseErrorStringify(kDeprecatedErr),
    CaseErrorStringify(kPermissionErr),
    CaseErrorStringify(kReadWouldBlockErr),
    CaseErrorStringify(kWriteWouldBlockErr),

    // Bonjour Errors

    CaseErrorStringifyHardCode(-65537, kDNSServiceErr_Unknown),
    CaseErrorStringifyHardCode(-65538, kDNSServiceErr_NoSuchName),
    CaseErrorStringifyHardCode(-65539, kDNSServiceErr_NoMemory),
    CaseErrorStringifyHardCode(-65540, kDNSServiceErr_BadParam),
    CaseErrorStringifyHardCode(-65541, kDNSServiceErr_BadReference),
    CaseErrorStringifyHardCode(-65542, kDNSServiceErr_BadState),
    CaseErrorStringifyHardCode(-65543, kDNSServiceErr_BadFlags),
    CaseErrorStringifyHardCode(-65544, kDNSServiceErr_Unsupported),
    CaseErrorStringifyHardCode(-65545, kDNSServiceErr_NotInitialized),
    CaseErrorStringifyHardCode(-65546, mStatus_NoCache),
    CaseErrorStringifyHardCode(-65547, kDNSServiceErr_AlreadyRegistered),
    CaseErrorStringifyHardCode(-65548, kDNSServiceErr_NameConflict),
    CaseErrorStringifyHardCode(-65549, kDNSServiceErr_Invalid),
    CaseErrorStringifyHardCode(-65550, kDNSServiceErr_Firewall),
    CaseErrorStringifyHardCode(-65551, kDNSServiceErr_Incompatible),
    CaseErrorStringifyHardCode(-65552, kDNSServiceErr_BadInterfaceIndex),
    CaseErrorStringifyHardCode(-65553, kDNSServiceErr_Refused),
    CaseErrorStringifyHardCode(-65554, kDNSServiceErr_NoSuchRecord),
    CaseErrorStringifyHardCode(-65555, kDNSServiceErr_NoAuth),
    CaseErrorStringifyHardCode(-65556, kDNSServiceErr_NoSuchKey),
    CaseErrorStringifyHardCode(-65557, kDNSServiceErr_NATTraversal),
    CaseErrorStringifyHardCode(-65558, kDNSServiceErr_DoubleNAT),
    CaseErrorStringifyHardCode(-65559, kDNSServiceErr_BadTime),
    CaseErrorStringifyHardCode(-65560, kDNSServiceErr_BadSig),
    CaseErrorStringifyHardCode(-65561, kDNSServiceErr_BadKey),
    CaseErrorStringifyHardCode(-65562, kDNSServiceErr_Transient),
    CaseErrorStringifyHardCode(-65563, kDNSServiceErr_ServiceNotRunning),
    CaseErrorStringifyHardCode(-65564, kDNSServiceErr_NATPortMappingUnsupported),
    CaseErrorStringifyHardCode(-65565, kDNSServiceErr_NATPortMappingDisabled),
    CaseErrorStringifyHardCode(-65566, kDNSServiceErr_NoRouter),
    CaseErrorStringifyHardCode(-65567, kDNSServiceErr_PollingMode),
    CaseErrorStringifyHardCode(-65568, kDNSServiceErr_Timeout),

    CaseErrorStringifyHardCode(-65787, mStatus_ConnPending),
    CaseErrorStringifyHardCode(-65788, mStatus_ConnFailed),
    CaseErrorStringifyHardCode(-65789, mStatus_ConnEstablished),
    CaseErrorStringifyHardCode(-65790, mStatus_GrowCache),
    CaseErrorStringifyHardCode(-65791, mStatus_ConfigChanged),
    CaseErrorStringifyHardCode(-65792, mStatus_MemFree),

#if (TARGET_OS_DARWIN)

    // HomeKit errors.

    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(1), HMErrorCodeAlreadyExists),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(2), HMErrorCodeNotFound),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(3), HMErrorCodeInvalidParameter),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(4), HMErrorCodeAccessoryNotReachable),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(5), HMErrorCodeReadOnlyCharacteristic),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(6), HMErrorCodeWriteOnlyCharacteristic),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(7), HMErrorCodeNotificationNotSupported),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(8), HMErrorCodeOperationTimedOut),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(9), HMErrorCodeAccessoryPoweredOff),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(10), HMErrorCodeAccessDenied),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(11), HMErrorCodeObjectAssociatedToAnotherHome),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(12), HMErrorCodeObjectNotAssociatedToAnyHome),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(13), HMErrorCodeObjectAlreadyAssociatedToHome),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(14), HMErrorCodeAccessoryIsBusy),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(15), HMErrorCodeOperationInProgress),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(16), HMErrorCodeAccessoryOutOfResources),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(17), HMErrorCodeInsufficientPrivileges),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(18), HMErrorCodeAccessoryPairingFailed),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(19), HMErrorCodeInvalidDataFormatSpecified),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(20), HMErrorCodeNilParameter),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(21), HMErrorCodeUnconfiguredParameter),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(22), HMErrorCodeInvalidClass),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(23), HMErrorCodeOperationCancelled),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(24), HMErrorCodeRoomForHomeCannotBeInZone),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(25), HMErrorCodeNoActionsInActionSet),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(26), HMErrorCodeNoRegisteredActionSets),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(27), HMErrorCodeMissingParameter),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(28), HMErrorCodeFireDateInPast),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(29), HMErrorCodeRoomForHomeCannotBeUpdated),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(30), HMErrorCodeActionInAnotherActionSet),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(31), HMErrorCodeObjectWithSimilarNameExistsInHome),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(32), HMErrorCodeHomeWithSimilarNameExists),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(33), HMErrorCodeRenameWithSimilarName),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(34), HMErrorCodeCannotRemoveNonBridgeAccessory),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(35), HMErrorCodeNameContainsProhibitedCharacters),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(36), HMErrorCodeNameDoesNotStartWithValidCharacters),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(37), HMErrorCodeUserIDNotEmailAddress),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(38), HMErrorCodeUserDeclinedAddingUser),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(39), HMErrorCodeUserDeclinedRemovingUser),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(40), HMErrorCodeUserDeclinedInvite),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(41), HMErrorCodeUserManagementFailed),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(42), HMErrorCodeRecurrenceTooSmall),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(43), HMErrorCodeInvalidValueType),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(44), HMErrorCodeValueLowerThanMinimum),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(45), HMErrorCodeValueHigherThanMaximum),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(46), HMErrorCodeStringLongerThanMaximum),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(47), HMErrorCodeHomeAccessNotAuthorized),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(48), HMErrorCodeOperationNotSupported),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(49), HMErrorCodeMaximumObjectLimitReached),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(50), HMErrorCodeAccessorySentInvalidResponse),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(51), HMErrorCodeStringShorterThanMinimum),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(52), HMErrorCodeGenericError),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(53), HMErrorCodeSecurityFailure),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(54), HMErrorCodeCommunicationFailure),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(55), HMErrorCodeMessageAuthenticationFailed),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(56), HMErrorCodeInvalidMessageSize),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(57), HMErrorCodeAccessoryDiscoveryFailed),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(58), HMErrorCodeClientRequestError),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(59), HMErrorCodeAccessoryResponseError),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(60), HMErrorCodeNameDoesNotEndWithValidCharacters),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(61), HMErrorCodeAccessoryIsBlocked),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(62), HMErrorCodeInvalidAssociatedServiceType),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(63), HMErrorCodeActionSetExecutionFailed),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(64), HMErrorCodeActionSetExecutionPartialSuccess),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(65), HMErrorCodeActionSetExecutionInProgress),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(66), HMErrorCodeAccessoryOutOfCompliance),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(67), HMErrorCodeDataResetFailure),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(68), HMErrorCodeNotificationAlreadyEnabled),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(69), HMErrorCodeRecurrenceMustBeOnSpecifiedBoundaries),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(70), HMErrorCodeDateMustBeOnSpecifiedBoundaries),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(71), HMErrorCodeCannotActivateTriggerTooFarInFuture),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(72), HMErrorCodeRecurrenceTooLarge),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(73), HMErrorCodeReadWritePartialSuccess),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(74), HMErrorCodeReadWriteFailure),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(75), HMErrorCodeNotSignedIntoiCloud),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(76), HMErrorCodeKeychainSyncNotEnabled),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(77), HMErrorCodeCloudDataSyncInProgress),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(78), HMErrorCodeNetworkUnavailable),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(79), HMErrorCodeAddAccessoryFailed),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(80), HMErrorCodeMissingEntitlement),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(81), HMErrorCodeCannotUnblockNonBridgeAccessory),
    CaseErrorStringifyHardCode(HomeKitErrorToOSStatus(82), HMErrorCodeDeviceLocked),
#endif

    // HTTP 2.0 errors

    CaseErrorStringifyHardCode(HTTP2ErrorToOSStatus(kHTTP2Error_ProtocolError), kHTTP2Error_ProtocolError),
    CaseErrorStringifyHardCode(HTTP2ErrorToOSStatus(kHTTP2Error_InternalError), kHTTP2Error_InternalError),
    CaseErrorStringifyHardCode(HTTP2ErrorToOSStatus(kHTTP2Error_FlowControlError), kHTTP2Error_FlowControlError),
    CaseErrorStringifyHardCode(HTTP2ErrorToOSStatus(kHTTP2Error_SettingsTimeout), kHTTP2Error_SettingsTimeout),
    CaseErrorStringifyHardCode(HTTP2ErrorToOSStatus(kHTTP2Error_StreamClosed), kHTTP2Error_StreamClosed),
    CaseErrorStringifyHardCode(HTTP2ErrorToOSStatus(kHTTP2Error_FrameSizeError), kHTTP2Error_FrameSizeError),
    CaseErrorStringifyHardCode(HTTP2ErrorToOSStatus(kHTTP2Error_RefusedStream), kHTTP2Error_RefusedStream),
    CaseErrorStringifyHardCode(HTTP2ErrorToOSStatus(kHTTP2Error_Cancel), kHTTP2Error_Cancel),
    CaseErrorStringifyHardCode(HTTP2ErrorToOSStatus(kHTTP2Error_CompressionError), kHTTP2Error_CompressionError),
    CaseErrorStringifyHardCode(HTTP2ErrorToOSStatus(kHTTP2Error_ConnectError), kHTTP2Error_ConnectError),
    CaseErrorStringifyHardCode(HTTP2ErrorToOSStatus(kHTTP2Error_EnhanceYourCalm), kHTTP2Error_EnhanceYourCalm),
    CaseErrorStringifyHardCode(HTTP2ErrorToOSStatus(kHTTP2Error_InadequateSecurity), kHTTP2Error_InadequateSecurity),
    CaseErrorStringifyHardCode(HTTP2ErrorToOSStatus(kHTTP2Error_HTTP1Required), kHTTP2Error_HTTP1Required),

#if (TARGET_OS_DARWIN)

    // Misc errors

    CaseErrorStringifyHardCode(1100, BOOTSTRAP_NOT_PRIVILEGED),
    CaseErrorStringifyHardCode(1101, BOOTSTRAP_NAME_IN_USE),
    CaseErrorStringifyHardCode(1102, BOOTSTRAP_UNKNOWN_SERVICE),
    CaseErrorStringifyHardCode(1103, BOOTSTRAP_SERVICE_ACTIVE),
    CaseErrorStringifyHardCode(1104, BOOTSTRAP_BAD_COUNT),
    CaseErrorStringifyHardCode(1105, BOOTSTRAP_NO_MEMORY),
    CaseErrorStringifyHardCode(1106, BOOTSTRAP_NO_CHILDREN),

    CaseErrorStringifyHardCode(107, EBADBUNDLE),
    CaseErrorStringifyHardCode(108, EBADPATH),
    CaseErrorStringifyHardCode(109, EBADPLIST),
    CaseErrorStringifyHardCode(110, EBADLABEL),
    CaseErrorStringifyHardCode(111, EBADPROGRAM),
    CaseErrorStringifyHardCode(112, ENODOMAIN),
    CaseErrorStringifyHardCode(113, ENOSERVICE),
    CaseErrorStringifyHardCode(114, EBADUSER),
    CaseErrorStringifyHardCode(115, EBADGROUP),
    CaseErrorStringifyHardCode(116, E2BIMPL),
    CaseErrorStringifyHardCode(117, EUSAGE),
    CaseErrorStringifyHardCode(118, EBADRESP),
    CaseErrorStringifyHardCode(119, EDISABLED),
    CaseErrorStringifyHardCode(120, EWRONGIPC),
    CaseErrorStringifyHardCode(121, ESKIPPED),
    CaseErrorStringifyHardCode(122, EOWNERSHIP),
    CaseErrorStringifyHardCode(123, EWHITELISTED),
    CaseErrorStringifyHardCode(124, EIMMELTING),
    CaseErrorStringifyHardCode(125, EBADDOMAIN),
    CaseErrorStringifyHardCode(126, EDEPRECATED),
    CaseErrorStringifyHardCode(127, ENOTSYSTEM),
    CaseErrorStringifyHardCode(128, ENOTBUNDLE),
    CaseErrorStringifyHardCode(129, ESUPERSEDED),
    CaseErrorStringifyHardCode(130, ERANDOM),
    CaseErrorStringifyHardCode(131, ENOOOO),
    CaseErrorStringifyHardCode(132, ERACE),
    CaseErrorStringifyHardCode(133, EMANY),
    CaseErrorStringifyHardCode(134, EWRONGSESSION),
    CaseErrorStringifyHardCode(135, EUNMANAGED),
    CaseErrorStringifyHardCode(136, ESINGLETON),
    CaseErrorStringifyHardCode(137, EBADSERVICE),
    CaseErrorStringifyHardCode(138, EWRONGHARDWARE),
    CaseErrorStringifyHardCode(139, ECANNOTEXEC),
    CaseErrorStringifyHardCode(140, EBADNAME),
    CaseErrorStringifyHardCode(141, EREENTRANT),
    CaseErrorStringifyHardCode(142, ENOTDEVELOPMENT),
    CaseErrorStringifyHardCode(143, ECACHED),
    CaseErrorStringifyHardCode(144, ENOTENTITLED),
    CaseErrorStringifyHardCode(145, EHIDDEN),
    CaseErrorStringifyHardCode(146, ENOTDEMAND),
    CaseErrorStringifyHardCode(147, ESERVICEEXTERN),
    CaseErrorStringifyHardCode(148, ENOTCACHED),
    CaseErrorStringifyHardCode(149, ENOTRESOLVED),
    CaseErrorStringifyHardCode(150, EROOTLESS),
    CaseErrorStringifyHardCode(151, ETRYUSER),
    CaseErrorStringifyHardCode(152, EWRONGBOOT),
    CaseErrorStringifyHardCode(153, EWTF),

    // Misc COM errors

    CaseErrorStringifyHardCode(UINT32_C(0x8000FFFF), E_UNEXPECTED),
    CaseErrorStringifyHardCode(UINT32_C(0x80000001), E_NOTIMPL),
    CaseErrorStringifyHardCode(UINT32_C(0x80000002), E_OUTOFMEMORY),
    CaseErrorStringifyHardCode(UINT32_C(0x80000003), E_INVALIDARG),
    CaseErrorStringifyHardCode(UINT32_C(0x80000004), E_NOINTERFACE),
    CaseErrorStringifyHardCode(UINT32_C(0x80000005), E_POINTER),
    CaseErrorStringifyHardCode(UINT32_C(0x80000006), E_HANDLE),
    CaseErrorStringifyHardCode(UINT32_C(0x80000007), E_ABORT),
    CaseErrorStringifyHardCode(UINT32_C(0x80000008), E_FAIL),
    CaseErrorStringifyHardCode(UINT32_C(0x80000009), E_ACCESSDENIED),

    CaseErrorString(-50, "ENETDOWN / paramErr"),
    CaseErrorStringifyHardCode(-108, memFullErr),

    // Security framwork errors

    CaseErrorStringifyHardCode(-25240, errSecACLNotSimple),
    CaseErrorStringifyHardCode(-25241, errSecPolicyNotFound),
    CaseErrorStringifyHardCode(-25242, errSecInvalidTrustSetting),
    CaseErrorStringifyHardCode(-25243, errSecNoAccessForItem),
    CaseErrorStringifyHardCode(-25244, errSecInvalidOwnerEdit),
    CaseErrorStringifyHardCode(-25245, errSecTrustNotAvailable),
    CaseErrorStringifyHardCode(-25256, errSecUnsupportedFormat),
    CaseErrorStringifyHardCode(-25257, errSecUnknownFormat),
    CaseErrorStringifyHardCode(-25258, errSecKeyIsSensitive),
    CaseErrorStringifyHardCode(-25259, errSecMultiplePrivKeys),
    CaseErrorStringifyHardCode(-25260, errSecPassphraseRequired),
    CaseErrorStringifyHardCode(-25261, errSecInvalidPasswordRef),
    CaseErrorStringifyHardCode(-25262, errSecInvalidTrustSettings),
    CaseErrorStringifyHardCode(-25263, errSecNoTrustSettings),
    CaseErrorStringifyHardCode(-25264, errSecPkcs12VerifyFailure),
    CaseErrorStringifyHardCode(-25291, errSecNotAvailable),
    CaseErrorStringifyHardCode(-25292, errSecReadOnly),
    CaseErrorStringifyHardCode(-25293, errSecAuthFailed),
    CaseErrorStringifyHardCode(-25294, errSecNoSuchKeychain),
    CaseErrorStringifyHardCode(-25295, errSecInvalidKeychain),
    CaseErrorStringifyHardCode(-25296, errSecDuplicateKeychain),
    CaseErrorStringifyHardCode(-25297, errSecDuplicateCallback),
    CaseErrorStringifyHardCode(-25298, errSecInvalidCallback),
    CaseErrorStringifyHardCode(-25299, errSecDuplicateItem),
    CaseErrorStringifyHardCode(-25300, errSecItemNotFound),
    CaseErrorStringifyHardCode(-25301, errSecBufferTooSmall),
    CaseErrorStringifyHardCode(-25302, errSecDataTooLarge),
    CaseErrorStringifyHardCode(-25303, errSecNoSuchAttr),
    CaseErrorStringifyHardCode(-25304, errSecInvalidItemRef),
    CaseErrorStringifyHardCode(-25305, errSecInvalidSearchRef),
    CaseErrorStringifyHardCode(-25306, errSecNoSuchClass),
    CaseErrorStringifyHardCode(-25307, errSecNoDefaultKeychain),
    CaseErrorStringifyHardCode(-25308, errSecInteractionNotAllowed),
    CaseErrorStringifyHardCode(-25309, errSecReadOnlyAttr),
    CaseErrorStringifyHardCode(-25310, errSecWrongSecVersion),
    CaseErrorStringifyHardCode(-25311, errSecKeySizeNotAllowed),
    CaseErrorStringifyHardCode(-25312, errSecNoStorageModule),
    CaseErrorStringifyHardCode(-25313, errSecNoCertificateModule),
    CaseErrorStringifyHardCode(-25314, errSecNoPolicyModule),
    CaseErrorStringifyHardCode(-25315, errSecInteractionRequired),
    CaseErrorStringifyHardCode(-25316, errSecDataNotAvailable),
    CaseErrorStringifyHardCode(-25317, errSecDataNotModifiable),
    CaseErrorStringifyHardCode(-25318, errSecCreateChainFailed),
    CaseErrorStringifyHardCode(-25319, errSecInvalidPrefsDomain),
    CaseErrorStringifyHardCode(-25320, errSecInDarkWake),
    CaseErrorStringifyHardCode(-25327, errSecMPSignatureInvalid),
    CaseErrorStringifyHardCode(-25328, errSecOTRTooOld),
    CaseErrorStringifyHardCode(-25329, errSecOTRIDTooNew),
    CaseErrorStringifyHardCode(-26267, errSecNotSigner),
    CaseErrorStringifyHardCode(-26270, errSecPolicyDenied),
    CaseErrorStringifyHardCode(-26275, errSecDecode),
    CaseErrorStringifyHardCode(-26276, errSecInternal),
    CaseErrorStringifyHardCode(-34017, errSecWaitForCallback),
    CaseErrorStringifyHardCode(-34018, errSecMissingEntitlement),
    CaseErrorStringifyHardCode(-34019, errSecUpgradePending),
    CaseErrorStringifyHardCode(-67024, errSecCSDbCorrupt),
    CaseErrorStringifyHardCode(-67025, errSecCSOutdated),
    CaseErrorStringifyHardCode(-67026, errSecCSFileHardQuarantined),
    CaseErrorStringifyHardCode(-67027, errSecCSNoMatches),
    CaseErrorStringifyHardCode(-67028, errSecCSBadBundleFormat),
    CaseErrorStringifyHardCode(-67029, errSecCSNoMainExecutable),
    CaseErrorStringifyHardCode(-67030, errSecCSInfoPlistFailed),
    CaseErrorStringifyHardCode(-67031, errSecCSHostProtocolInvalidAttribute),
    CaseErrorStringifyHardCode(-67032, errSecCSDBAccess),
    CaseErrorStringifyHardCode(-67033, errSecCSDBDenied),
    CaseErrorStringifyHardCode(-67034, errSecCSStaticCodeChanged),
    CaseErrorStringifyHardCode(-67035, errSecCSHostProtocolInvalidHash),
    CaseErrorStringifyHardCode(-67036, errSecCSCMSTooLarge),
    CaseErrorStringifyHardCode(-67037, errSecCSNotSupported),
    CaseErrorStringifyHardCode(-67039, errSecCSHostProtocolUnrelated),
    CaseErrorStringifyHardCode(-67040, errSecCSHostProtocolStateError),
    CaseErrorStringifyHardCode(-67041, errSecCSHostProtocolNotProxy),
    CaseErrorStringifyHardCode(-67042, errSecCSHostProtocolDedicationError),
    CaseErrorStringifyHardCode(-67043, errSecCSHostProtocolContradiction),
    CaseErrorStringifyHardCode(-67044, errSecCSHostProtocolRelativePath),
    CaseErrorStringifyHardCode(-67045, errSecCSSignatureInvalid),
    CaseErrorStringifyHardCode(-67046, errSecCSNotAHost),
    CaseErrorStringifyHardCode(-67047, errSecCSHostReject),
    CaseErrorStringifyHardCode(-67048, errSecCSInternalError),
    CaseErrorStringifyHardCode(-67049, errSecCSBadObjectFormat),
    CaseErrorStringifyHardCode(-67050, errSecCSReqFailed),
    CaseErrorStringifyHardCode(-67051, errSecCSReqUnsupported),
    CaseErrorStringifyHardCode(-67052, errSecCSReqInvalid),
    CaseErrorStringifyHardCode(-67053, errSecCSResourceRulesInvalid),
    CaseErrorStringifyHardCode(-67054, errSecCSBadResource),
    CaseErrorStringifyHardCode(-67055, errSecCSResourcesInvalid),
    CaseErrorStringifyHardCode(-67056, errSecCSResourcesNotFound),
    CaseErrorStringifyHardCode(-67057, errSecCSResourcesNotSealed),
    CaseErrorStringifyHardCode(-67058, errSecCSBadDictionaryFormat),
    CaseErrorStringifyHardCode(-67059, errSecCSSignatureUnsupported),
    CaseErrorStringifyHardCode(-67060, errSecCSSignatureNotVerifiable),
    CaseErrorStringifyHardCode(-67061, errSecCSSignatureFailed),
    CaseErrorStringifyHardCode(-67062, errSecCSUnsigned),
    CaseErrorStringifyHardCode(-67063, errSecCSGuestInvalid),
    CaseErrorStringifyHardCode(-67064, errSecCSMultipleGuests),
    CaseErrorStringifyHardCode(-67065, errSecCSNoSuchCode),
    CaseErrorStringifyHardCode(-67066, errSecCSInvalidAttributeValues),
    CaseErrorStringifyHardCode(-67067, errSecCSUnsupportedGuestAttributes),
    CaseErrorStringifyHardCode(-67068, errSecCSStaticCodeNotFound),
    CaseErrorStringifyHardCode(-67069, errSecCSObjectRequired),
    CaseErrorStringifyHardCode(-67070, errSecCSInvalidFlags),
    CaseErrorStringifyHardCode(-67071, errSecCSInvalidObjectRef),
    CaseErrorStringifyHardCode(-67072, errSecCSUnimplemented),
    CaseErrorStringifyHardCode(-67585, errSecServiceNotAvailable),
    CaseErrorStringifyHardCode(-67586, errSecInsufficientClientID),
    CaseErrorStringifyHardCode(-67587, errSecDeviceReset),
    CaseErrorStringifyHardCode(-67588, errSecDeviceFailed),
    CaseErrorStringifyHardCode(-67589, errSecAppleAddAppACLSubject),
    CaseErrorStringifyHardCode(-67590, errSecApplePublicKeyIncomplete),
    CaseErrorStringifyHardCode(-67591, errSecAppleSignatureMismatch),
    CaseErrorStringifyHardCode(-67592, errSecAppleInvalidKeyStartDate),
    CaseErrorStringifyHardCode(-67593, errSecAppleInvalidKeyEndDate),
    CaseErrorStringifyHardCode(-67594, errSecConversionError),
    CaseErrorStringifyHardCode(-67595, errSecAppleSSLv2Rollback),
    CaseErrorStringifyHardCode(-67596, errSecQuotaExceeded),
    CaseErrorStringifyHardCode(-67597, errSecFileTooBig),
    CaseErrorStringifyHardCode(-67598, errSecInvalidDatabaseBlob),
    CaseErrorStringifyHardCode(-67599, errSecInvalidKeyBlob),
    CaseErrorStringifyHardCode(-67600, errSecIncompatibleDatabaseBlob),
    CaseErrorStringifyHardCode(-67601, errSecIncompatibleKeyBlob),
    CaseErrorStringifyHardCode(-67602, errSecHostNameMismatch),
    CaseErrorStringifyHardCode(-67603, errSecUnknownCriticalExtensionFlag),
    CaseErrorStringifyHardCode(-67604, errSecNoBasicConstraints),
    CaseErrorStringifyHardCode(-67605, errSecNoBasicConstraintsCA),
    CaseErrorStringifyHardCode(-67606, errSecInvalidAuthorityKeyID),
    CaseErrorStringifyHardCode(-67607, errSecInvalidSubjectKeyID),
    CaseErrorStringifyHardCode(-67608, errSecInvalidKeyUsageForPolicy),
    CaseErrorStringifyHardCode(-67609, errSecInvalidExtendedKeyUsage),
    CaseErrorStringifyHardCode(-67610, errSecInvalidIDLinkage),
    CaseErrorStringifyHardCode(-67611, errSecPathLengthConstraintExceeded),
    CaseErrorStringifyHardCode(-67612, errSecInvalidRoot),
    CaseErrorStringifyHardCode(-67613, errSecCRLExpired),
    CaseErrorStringifyHardCode(-67614, errSecCRLNotValidYet),
    CaseErrorStringifyHardCode(-67615, errSecCRLNotFound),
    CaseErrorStringifyHardCode(-67616, errSecCRLServerDown),
    CaseErrorStringifyHardCode(-67617, errSecCRLBadURI),
    CaseErrorStringifyHardCode(-67618, errSecUnknownCertExtension),
    CaseErrorStringifyHardCode(-67619, errSecUnknownCRLExtension),
    CaseErrorStringifyHardCode(-67620, errSecCRLNotTrusted),
    CaseErrorStringifyHardCode(-67621, errSecCRLPolicyFailed),
    CaseErrorStringifyHardCode(-67622, errSecIDPFailure),
    CaseErrorStringifyHardCode(-67623, errSecSMIMEEmailAddressesNotFound),
    CaseErrorStringifyHardCode(-67624, errSecSMIMEBadExtendedKeyUsage),
    CaseErrorStringifyHardCode(-67625, errSecSMIMEBadKeyUsage),
    CaseErrorStringifyHardCode(-67626, errSecSMIMEKeyUsageNotCritical),
    CaseErrorStringifyHardCode(-67627, errSecSMIMENoEmailAddress),
    CaseErrorStringifyHardCode(-67628, errSecSMIMESubjAltNameNotCritical),
    CaseErrorStringifyHardCode(-67629, errSecSSLBadExtendedKeyUsage),
    CaseErrorStringifyHardCode(-67630, errSecOCSPBadResponse),
    CaseErrorStringifyHardCode(-67631, errSecOCSPBadRequest),
    CaseErrorStringifyHardCode(-67632, errSecOCSPUnavailable),
    CaseErrorStringifyHardCode(-67633, errSecOCSPStatusUnrecognized),
    CaseErrorStringifyHardCode(-67634, errSecEndOfData),
    CaseErrorStringifyHardCode(-67635, errSecIncompleteCertRevocationCheck),
    CaseErrorStringifyHardCode(-67636, errSecNetworkFailure),
    CaseErrorStringifyHardCode(-67637, errSecOCSPNotTrustedToAnchor),
    CaseErrorStringifyHardCode(-67638, errSecRecordModified),
    CaseErrorStringifyHardCode(-67639, errSecOCSPSignatureError),
    CaseErrorStringifyHardCode(-67640, errSecOCSPNoSigner),
    CaseErrorStringifyHardCode(-67641, errSecOCSPResponderMalformedReq),
    CaseErrorStringifyHardCode(-67642, errSecOCSPResponderInternalError),
    CaseErrorStringifyHardCode(-67643, errSecOCSPResponderTryLater),
    CaseErrorStringifyHardCode(-67644, errSecOCSPResponderSignatureRequired),
    CaseErrorStringifyHardCode(-67645, errSecOCSPResponderUnauthorized),
    CaseErrorStringifyHardCode(-67646, errSecOCSPResponseNonceMismatch),
    CaseErrorStringifyHardCode(-67647, errSecCodeSigningBadCertChainLength),
    CaseErrorStringifyHardCode(-67648, errSecCodeSigningNoBasicConstraints),
    CaseErrorStringifyHardCode(-67649, errSecCodeSigningBadPathLengthConstraint),
    CaseErrorStringifyHardCode(-67650, errSecCodeSigningNoExtendedKeyUsage),
    CaseErrorStringifyHardCode(-67651, errSecCodeSigningDevelopment),
    CaseErrorStringifyHardCode(-67652, errSecResourceSignBadCertChainLength),
    CaseErrorStringifyHardCode(-67653, errSecResourceSignBadExtKeyUsage),
    CaseErrorStringifyHardCode(-67654, errSecTrustSettingDeny),
    CaseErrorStringifyHardCode(-67655, errSecInvalidSubjectName),
    CaseErrorStringifyHardCode(-67656, errSecUnknownQualifiedCertStatement),
    CaseErrorStringifyHardCode(-67657, errSecMobileMeRequestQueued),
    CaseErrorStringifyHardCode(-67658, errSecMobileMeRequestRedirected),
    CaseErrorStringifyHardCode(-67659, errSecMobileMeServerError),
    CaseErrorStringifyHardCode(-67660, errSecMobileMeServerNotAvailable),
    CaseErrorStringifyHardCode(-67661, errSecMobileMeServerAlreadyExists),
    CaseErrorStringifyHardCode(-67662, errSecMobileMeServerServiceErr),
    CaseErrorStringifyHardCode(-67663, errSecMobileMeRequestAlreadyPending),
    CaseErrorStringifyHardCode(-67664, errSecMobileMeNoRequestPending),
    CaseErrorStringifyHardCode(-67665, errSecMobileMeCSRVerifyFailure),
    CaseErrorStringifyHardCode(-67666, errSecMobileMeFailedConsistencyCheck),
    CaseErrorStringifyHardCode(-67667, errSecNotInitialized),
    CaseErrorStringifyHardCode(-67668, errSecInvalidHandleUsage),
    CaseErrorStringifyHardCode(-67669, errSecPVCReferentNotFound),
    CaseErrorStringifyHardCode(-67670, errSecFunctionIntegrityFail),
    CaseErrorStringifyHardCode(-67671, errSecInternalError),
    CaseErrorStringifyHardCode(-67672, errSecMemoryError),
    CaseErrorStringifyHardCode(-67673, errSecInvalidData),
    CaseErrorStringifyHardCode(-67674, errSecMDSError),
    CaseErrorStringifyHardCode(-67675, errSecInvalidPointer),
    CaseErrorStringifyHardCode(-67676, errSecSelfCheckFailed),
    CaseErrorStringifyHardCode(-67677, errSecFunctionFailed),
    CaseErrorStringifyHardCode(-67678, errSecModuleManifestVerifyFailed),
    CaseErrorStringifyHardCode(-67679, errSecInvalidGUID),
    CaseErrorStringifyHardCode(-67680, errSecInvalidHandle),
    CaseErrorStringifyHardCode(-67681, errSecInvalidDBList),
    CaseErrorStringifyHardCode(-67682, errSecInvalidPassthroughID),
    CaseErrorStringifyHardCode(-67683, errSecInvalidNetworkAddress),
    CaseErrorStringifyHardCode(-67684, errSecCRLAlreadySigned),
    CaseErrorStringifyHardCode(-67685, errSecInvalidNumberOfFields),
    CaseErrorStringifyHardCode(-67686, errSecVerificationFailure),
    CaseErrorStringifyHardCode(-67687, errSecUnknownTag),
    CaseErrorStringifyHardCode(-67688, errSecInvalidSignature),
    CaseErrorStringifyHardCode(-67689, errSecInvalidName),
    CaseErrorStringifyHardCode(-67690, errSecInvalidCertificateRef),
    CaseErrorStringifyHardCode(-67691, errSecInvalidCertificateGroup),
    CaseErrorStringifyHardCode(-67692, errSecTagNotFound),
    CaseErrorStringifyHardCode(-67693, errSecInvalidQuery),
    CaseErrorStringifyHardCode(-67694, errSecInvalidValue),
    CaseErrorStringifyHardCode(-67695, errSecCallbackFailed),
    CaseErrorStringifyHardCode(-67696, errSecACLDeleteFailed),
    CaseErrorStringifyHardCode(-67697, errSecACLReplaceFailed),
    CaseErrorStringifyHardCode(-67698, errSecACLAddFailed),
    CaseErrorStringifyHardCode(-67699, errSecACLChangeFailed),
    CaseErrorStringifyHardCode(-67700, errSecInvalidAccessCredentials),
    CaseErrorStringifyHardCode(-67701, errSecInvalidRecord),
    CaseErrorStringifyHardCode(-67702, errSecInvalidACL),
    CaseErrorStringifyHardCode(-67703, errSecInvalidSampleValue),
    CaseErrorStringifyHardCode(-67704, errSecIncompatibleVersion),
    CaseErrorStringifyHardCode(-67705, errSecPrivilegeNotGranted),
    CaseErrorStringifyHardCode(-67706, errSecInvalidScope),
    CaseErrorStringifyHardCode(-67707, errSecPVCAlreadyConfigured),
    CaseErrorStringifyHardCode(-67708, errSecInvalidPVC),
    CaseErrorStringifyHardCode(-67709, errSecEMMLoadFailed),
    CaseErrorStringifyHardCode(-67710, errSecEMMUnloadFailed),
    CaseErrorStringifyHardCode(-67711, errSecAddinLoadFailed),
    CaseErrorStringifyHardCode(-67712, errSecInvalidKeyRef),
    CaseErrorStringifyHardCode(-67713, errSecInvalidKeyHierarchy),
    CaseErrorStringifyHardCode(-67714, errSecAddinUnloadFailed),
    CaseErrorStringifyHardCode(-67715, errSecLibraryReferenceNotFound),
    CaseErrorStringifyHardCode(-67716, errSecInvalidAddinFunctionTable),
    CaseErrorStringifyHardCode(-67717, errSecInvalidServiceMask),
    CaseErrorStringifyHardCode(-67718, errSecModuleNotLoaded),
    CaseErrorStringifyHardCode(-67719, errSecInvalidSubServiceID),
    CaseErrorStringifyHardCode(-67720, errSecAttributeNotInContext),
    CaseErrorStringifyHardCode(-67721, errSecModuleManagerInitializeFailed),
    CaseErrorStringifyHardCode(-67722, errSecModuleManagerNotFound),
    CaseErrorStringifyHardCode(-67723, errSecEventNotificationCallbackNotFound),
    CaseErrorStringifyHardCode(-67724, errSecInputLengthError),
    CaseErrorStringifyHardCode(-67725, errSecOutputLengthError),
    CaseErrorStringifyHardCode(-67726, errSecPrivilegeNotSupported),
    CaseErrorStringifyHardCode(-67727, errSecDeviceError),
    CaseErrorStringifyHardCode(-67728, errSecAttachHandleBusy),
    CaseErrorStringifyHardCode(-67729, errSecNotLoggedIn),
    CaseErrorStringifyHardCode(-67730, errSecAlgorithmMismatch),
    CaseErrorStringifyHardCode(-67731, errSecKeyUsageIncorrect),
    CaseErrorStringifyHardCode(-67732, errSecKeyBlobTypeIncorrect),
    CaseErrorStringifyHardCode(-67733, errSecKeyHeaderInconsistent),
    CaseErrorStringifyHardCode(-67734, errSecUnsupportedKeyFormat),
    CaseErrorStringifyHardCode(-67735, errSecUnsupportedKeySize),
    CaseErrorStringifyHardCode(-67736, errSecInvalidKeyUsageMask),
    CaseErrorStringifyHardCode(-67737, errSecUnsupportedKeyUsageMask),
    CaseErrorStringifyHardCode(-67738, errSecInvalidKeyAttributeMask),
    CaseErrorStringifyHardCode(-67739, errSecUnsupportedKeyAttributeMask),
    CaseErrorStringifyHardCode(-67740, errSecInvalidKeyLabel),
    CaseErrorStringifyHardCode(-67741, errSecUnsupportedKeyLabel),
    CaseErrorStringifyHardCode(-67742, errSecInvalidKeyFormat),
    CaseErrorStringifyHardCode(-67743, errSecUnsupportedVectorOfBuffers),
    CaseErrorStringifyHardCode(-67744, errSecInvalidInputVector),
    CaseErrorStringifyHardCode(-67745, errSecInvalidOutputVector),
    CaseErrorStringifyHardCode(-67746, errSecInvalidContext),
    CaseErrorStringifyHardCode(-67747, errSecInvalidAlgorithm),
    CaseErrorStringifyHardCode(-67748, errSecInvalidAttributeKey),
    CaseErrorStringifyHardCode(-67749, errSecMissingAttributeKey),
    CaseErrorStringifyHardCode(-67750, errSecInvalidAttributeInitVector),
    CaseErrorStringifyHardCode(-67751, errSecMissingAttributeInitVector),
    CaseErrorStringifyHardCode(-67752, errSecInvalidAttributeSalt),
    CaseErrorStringifyHardCode(-67753, errSecMissingAttributeSalt),
    CaseErrorStringifyHardCode(-67754, errSecInvalidAttributePadding),
    CaseErrorStringifyHardCode(-67755, errSecMissingAttributePadding),
    CaseErrorStringifyHardCode(-67756, errSecInvalidAttributeRandom),
    CaseErrorStringifyHardCode(-67757, errSecMissingAttributeRandom),
    CaseErrorStringifyHardCode(-67758, errSecInvalidAttributeSeed),
    CaseErrorStringifyHardCode(-67759, errSecMissingAttributeSeed),
    CaseErrorStringifyHardCode(-67760, errSecInvalidAttributePassphrase),
    CaseErrorStringifyHardCode(-67761, errSecMissingAttributePassphrase),
    CaseErrorStringifyHardCode(-67762, errSecInvalidAttributeKeyLength),
    CaseErrorStringifyHardCode(-67763, errSecMissingAttributeKeyLength),
    CaseErrorStringifyHardCode(-67764, errSecInvalidAttributeBlockSize),
    CaseErrorStringifyHardCode(-67765, errSecMissingAttributeBlockSize),
    CaseErrorStringifyHardCode(-67766, errSecInvalidAttributeOutputSize),
    CaseErrorStringifyHardCode(-67767, errSecMissingAttributeOutputSize),
    CaseErrorStringifyHardCode(-67768, errSecInvalidAttributeRounds),
    CaseErrorStringifyHardCode(-67769, errSecMissingAttributeRounds),
    CaseErrorStringifyHardCode(-67770, errSecInvalidAlgorithmParms),
    CaseErrorStringifyHardCode(-67771, errSecMissingAlgorithmParms),
    CaseErrorStringifyHardCode(-67772, errSecInvalidAttributeLabel),
    CaseErrorStringifyHardCode(-67773, errSecMissingAttributeLabel),
    CaseErrorStringifyHardCode(-67774, errSecInvalidAttributeKeyType),
    CaseErrorStringifyHardCode(-67775, errSecMissingAttributeKeyType),
    CaseErrorStringifyHardCode(-67776, errSecInvalidAttributeMode),
    CaseErrorStringifyHardCode(-67777, errSecMissingAttributeMode),
    CaseErrorStringifyHardCode(-67778, errSecInvalidAttributeEffectiveBits),
    CaseErrorStringifyHardCode(-67779, errSecMissingAttributeEffectiveBits),
    CaseErrorStringifyHardCode(-67780, errSecInvalidAttributeStartDate),
    CaseErrorStringifyHardCode(-67781, errSecMissingAttributeStartDate),
    CaseErrorStringifyHardCode(-67782, errSecInvalidAttributeEndDate),
    CaseErrorStringifyHardCode(-67783, errSecMissingAttributeEndDate),
    CaseErrorStringifyHardCode(-67784, errSecInvalidAttributeVersion),
    CaseErrorStringifyHardCode(-67785, errSecMissingAttributeVersion),
    CaseErrorStringifyHardCode(-67786, errSecInvalidAttributePrime),
    CaseErrorStringifyHardCode(-67787, errSecMissingAttributePrime),
    CaseErrorStringifyHardCode(-67788, errSecInvalidAttributeBase),
    CaseErrorStringifyHardCode(-67789, errSecMissingAttributeBase),
    CaseErrorStringifyHardCode(-67790, errSecInvalidAttributeSubprime),
    CaseErrorStringifyHardCode(-67791, errSecMissingAttributeSubprime),
    CaseErrorStringifyHardCode(-67792, errSecInvalidAttributeIterationCount),
    CaseErrorStringifyHardCode(-67793, errSecMissingAttributeIterationCount),
    CaseErrorStringifyHardCode(-67794, errSecInvalidAttributeDLDBHandle),
    CaseErrorStringifyHardCode(-67795, errSecMissingAttributeDLDBHandle),
    CaseErrorStringifyHardCode(-67796, errSecInvalidAttributeAccessCredentials),
    CaseErrorStringifyHardCode(-67797, errSecMissingAttributeAccessCredentials),
    CaseErrorStringifyHardCode(-67798, errSecInvalidAttributePublicKeyFormat),
    CaseErrorStringifyHardCode(-67799, errSecMissingAttributePublicKeyFormat),
    CaseErrorStringifyHardCode(-67800, errSecInvalidAttributePrivateKeyFormat),
    CaseErrorStringifyHardCode(-67801, errSecMissingAttributePrivateKeyFormat),
    CaseErrorStringifyHardCode(-67802, errSecInvalidAttributeSymmetricKeyFormat),
    CaseErrorStringifyHardCode(-67803, errSecMissingAttributeSymmetricKeyFormat),
    CaseErrorStringifyHardCode(-67804, errSecInvalidAttributeWrappedKeyFormat),
    CaseErrorStringifyHardCode(-67805, errSecMissingAttributeWrappedKeyFormat),
    CaseErrorStringifyHardCode(-67806, errSecStagedOperationInProgress),
    CaseErrorStringifyHardCode(-67807, errSecStagedOperationNotStarted),
    CaseErrorStringifyHardCode(-67808, errSecVerifyFailed),
    CaseErrorStringifyHardCode(-67809, errSecQuerySizeUnknown),
    CaseErrorStringifyHardCode(-67810, errSecBlockSizeMismatch),
    CaseErrorStringifyHardCode(-67811, errSecPublicKeyInconsistent),
    CaseErrorStringifyHardCode(-67812, errSecDeviceVerifyFailed),
    CaseErrorStringifyHardCode(-67813, errSecInvalidLoginName),
    CaseErrorStringifyHardCode(-67814, errSecAlreadyLoggedIn),
    CaseErrorStringifyHardCode(-67815, errSecInvalidDigestAlgorithm),
    CaseErrorStringifyHardCode(-67816, errSecInvalidCRLGroup),
    CaseErrorStringifyHardCode(-67817, errSecCertificateCannotOperate),
    CaseErrorStringifyHardCode(-67818, errSecCertificateExpired),
    CaseErrorStringifyHardCode(-67819, errSecCertificateNotValidYet),
    CaseErrorStringifyHardCode(-67820, errSecCertificateRevoked),
    CaseErrorStringifyHardCode(-67821, errSecCertificateSuspended),
    CaseErrorStringifyHardCode(-67822, errSecInsufficientCredentials),
    CaseErrorStringifyHardCode(-67823, errSecInvalidAction),
    CaseErrorStringifyHardCode(-67824, errSecInvalidAuthority),
    CaseErrorStringifyHardCode(-67825, errSecVerifyActionFailed),
    CaseErrorStringifyHardCode(-67826, errSecInvalidCertAuthority),
    CaseErrorStringifyHardCode(-67827, errSecInvaldCRLAuthority),
    CaseErrorStringifyHardCode(-67828, errSecInvalidCRLEncoding),
    CaseErrorStringifyHardCode(-67829, errSecInvalidCRLType),
    CaseErrorStringifyHardCode(-67830, errSecInvalidCRL),
    CaseErrorStringifyHardCode(-67831, errSecInvalidFormType),
    CaseErrorStringifyHardCode(-67832, errSecInvalidID),
    CaseErrorStringifyHardCode(-67833, errSecInvalidIdentifier),
    CaseErrorStringifyHardCode(-67834, errSecInvalidIndex),
    CaseErrorStringifyHardCode(-67835, errSecInvalidPolicyIdentifiers),
    CaseErrorStringifyHardCode(-67836, errSecInvalidTimeString),
    CaseErrorStringifyHardCode(-67837, errSecInvalidReason),
    CaseErrorStringifyHardCode(-67838, errSecInvalidRequestInputs),
    CaseErrorStringifyHardCode(-67839, errSecInvalidResponseVector),
    CaseErrorStringifyHardCode(-67840, errSecInvalidStopOnPolicy),
    CaseErrorStringifyHardCode(-67841, errSecInvalidTuple),
    CaseErrorStringifyHardCode(-67842, errSecMultipleValuesUnsupported),
    CaseErrorStringifyHardCode(-67843, errSecNotTrusted),
    CaseErrorStringifyHardCode(-67844, errSecNoDefaultAuthority),
    CaseErrorStringifyHardCode(-67845, errSecRejectedForm),
    CaseErrorStringifyHardCode(-67846, errSecRequestLost),
    CaseErrorStringifyHardCode(-67847, errSecRequestRejected),
    CaseErrorStringifyHardCode(-67848, errSecUnsupportedAddressType),
    CaseErrorStringifyHardCode(-67849, errSecUnsupportedService),
    CaseErrorStringifyHardCode(-67850, errSecInvalidTupleGroup),
    CaseErrorStringifyHardCode(-67851, errSecInvalidBaseACLs),
    CaseErrorStringifyHardCode(-67852, errSecInvalidTupleCredendtials),
    CaseErrorStringifyHardCode(-67853, errSecInvalidEncoding),
    CaseErrorStringifyHardCode(-67854, errSecInvalidValidityPeriod),
    CaseErrorStringifyHardCode(-67855, errSecInvalidRequestor),
    CaseErrorStringifyHardCode(-67856, errSecRequestDescriptor),
    CaseErrorStringifyHardCode(-67857, errSecInvalidBundleInfo),
    CaseErrorStringifyHardCode(-67858, errSecInvalidCRLIndex),
    CaseErrorStringifyHardCode(-67859, errSecNoFieldValues),
    CaseErrorStringifyHardCode(-67860, errSecUnsupportedFieldFormat),
    CaseErrorStringifyHardCode(-67861, errSecUnsupportedIndexInfo),
    CaseErrorStringifyHardCode(-67862, errSecUnsupportedLocality),
    CaseErrorStringifyHardCode(-67863, errSecUnsupportedNumAttributes),
    CaseErrorStringifyHardCode(-67864, errSecUnsupportedNumIndexes),
    CaseErrorStringifyHardCode(-67865, errSecUnsupportedNumRecordTypes),
    CaseErrorStringifyHardCode(-67866, errSecFieldSpecifiedMultiple),
    CaseErrorStringifyHardCode(-67867, errSecIncompatibleFieldFormat),
    CaseErrorStringifyHardCode(-67868, errSecInvalidParsingModule),
    CaseErrorStringifyHardCode(-67869, errSecDatabaseLocked),
    CaseErrorStringifyHardCode(-67870, errSecDatastoreIsOpen),
    CaseErrorStringifyHardCode(-67871, errSecMissingValue),
    CaseErrorStringifyHardCode(-67872, errSecUnsupportedQueryLimits),
    CaseErrorStringifyHardCode(-67873, errSecUnsupportedNumSelectionPreds),
    CaseErrorStringifyHardCode(-67874, errSecUnsupportedOperator),
    CaseErrorStringifyHardCode(-67875, errSecInvalidDBLocation),
    CaseErrorStringifyHardCode(-67876, errSecInvalidAccessRequest),
    CaseErrorStringifyHardCode(-67877, errSecInvalidIndexInfo),
    CaseErrorStringifyHardCode(-67878, errSecInvalidNewOwner),
    CaseErrorStringifyHardCode(-67879, errSecInvalidModifyMode),
    CaseErrorStringifyHardCode(-67880, errSecMissingRequiredExtension),
    CaseErrorStringifyHardCode(-67881, errSecExtendedKeyUsageNotCritical),
    CaseErrorStringifyHardCode(-67882, errSecTimestampMissing),
    CaseErrorStringifyHardCode(-67883, errSecTimestampInvalid),
    CaseErrorStringifyHardCode(-67884, errSecTimestampNotTrusted),
    CaseErrorStringifyHardCode(-67885, errSecTimestampServiceNotAvailable),
    CaseErrorStringifyHardCode(-67886, errSecTimestampBadAlg),
    CaseErrorStringifyHardCode(-67887, errSecTimestampBadRequest),
    CaseErrorStringifyHardCode(-67888, errSecTimestampBadDataFormat),
    CaseErrorStringifyHardCode(-67889, errSecTimestampTimeNotAvailable),
    CaseErrorStringifyHardCode(-67890, errSecTimestampUnacceptedPolicy),
    CaseErrorStringifyHardCode(-67891, errSecTimestampUnacceptedExtension),
    CaseErrorStringifyHardCode(-67892, errSecTimestampAddInfoNotAvailable),
    CaseErrorStringifyHardCode(-67893, errSecTimestampSystemFailure),
    CaseErrorStringifyHardCode(-67894, errSecSigningTimeMissing),
    CaseErrorStringifyHardCode(-67895, errSecTimestampRejection),
    CaseErrorStringifyHardCode(-67896, errSecTimestampWaiting),
    CaseErrorStringifyHardCode(-67897, errSecTimestampRevocationWarning),
    CaseErrorStringifyHardCode(-67898, errSecTimestampRevocationNotification),

// Security framework SEC_ERROR errors.

#define SEC_ERROR_Stringify(OFFSET, NAME)   \
    {                                       \
        (OSStatus)(-8192 + (OFFSET)), #NAME \
    }

    SEC_ERROR_Stringify(0, SEC_ERROR_IO),
    SEC_ERROR_Stringify(1, SEC_ERROR_LIBRARY_FAILURE),
    SEC_ERROR_Stringify(2, SEC_ERROR_BAD_DATA),
    SEC_ERROR_Stringify(3, SEC_ERROR_OUTPUT_LEN),
    SEC_ERROR_Stringify(4, SEC_ERROR_INPUT_LEN),
    SEC_ERROR_Stringify(5, SEC_ERROR_INVALID_ARGS),
    SEC_ERROR_Stringify(6, SEC_ERROR_INVALID_ALGORITHM),
    SEC_ERROR_Stringify(7, SEC_ERROR_INVALID_AVA),
    SEC_ERROR_Stringify(8, SEC_ERROR_INVALID_TIME),
    SEC_ERROR_Stringify(9, SEC_ERROR_BAD_DER),
    SEC_ERROR_Stringify(10, SEC_ERROR_BAD_SIGNATURE),
    SEC_ERROR_Stringify(11, SEC_ERROR_EXPIRED_CERTIFICATE),
    SEC_ERROR_Stringify(12, SEC_ERROR_REVOKED_CERTIFICATE),
    SEC_ERROR_Stringify(13, SEC_ERROR_UNKNOWN_ISSUER),
    SEC_ERROR_Stringify(14, SEC_ERROR_BAD_KEY),
    SEC_ERROR_Stringify(15, SEC_ERROR_BAD_PASSWORD),
    SEC_ERROR_Stringify(16, SEC_ERROR_RETRY_PASSWORD),
    SEC_ERROR_Stringify(17, SEC_ERROR_NO_NODELOCK),
    SEC_ERROR_Stringify(18, SEC_ERROR_BAD_DATABASE),
    SEC_ERROR_Stringify(19, SEC_ERROR_NO_MEMORY),
    SEC_ERROR_Stringify(20, SEC_ERROR_UNTRUSTED_ISSUER),
    SEC_ERROR_Stringify(21, SEC_ERROR_UNTRUSTED_CERT),
    SEC_ERROR_Stringify(22, SEC_ERROR_DUPLICATE_CERT),
    SEC_ERROR_Stringify(23, SEC_ERROR_DUPLICATE_CERT_NAME),
    SEC_ERROR_Stringify(24, SEC_ERROR_ADDING_CERT),
    SEC_ERROR_Stringify(25, SEC_ERROR_FILING_KEY),
    SEC_ERROR_Stringify(26, SEC_ERROR_NO_KEY),
    SEC_ERROR_Stringify(27, SEC_ERROR_CERT_VALID),
    SEC_ERROR_Stringify(28, SEC_ERROR_CERT_NOT_VALID),
    SEC_ERROR_Stringify(29, SEC_ERROR_CERT_NO_RESPONSE),
    SEC_ERROR_Stringify(30, SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE),
    SEC_ERROR_Stringify(31, SEC_ERROR_CRL_EXPIRED),
    SEC_ERROR_Stringify(32, SEC_ERROR_CRL_BAD_SIGNATURE),
    SEC_ERROR_Stringify(33, SEC_ERROR_CRL_INVALID),
    SEC_ERROR_Stringify(34, SEC_ERROR_EXTENSION_VALUE_INVALID),
    SEC_ERROR_Stringify(35, SEC_ERROR_EXTENSION_NOT_FOUND),
    SEC_ERROR_Stringify(36, SEC_ERROR_CA_CERT_INVALID),
    SEC_ERROR_Stringify(37, SEC_ERROR_PATH_LEN_CONSTRAINT_INVALID),
    SEC_ERROR_Stringify(38, SEC_ERROR_CERT_USAGES_INVALID),
    SEC_ERROR_Stringify(39, SEC_INTERNAL_ONLY),
    SEC_ERROR_Stringify(40, SEC_ERROR_INVALID_KEY),
    SEC_ERROR_Stringify(41, SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION),
    SEC_ERROR_Stringify(42, SEC_ERROR_OLD_CRL),
    SEC_ERROR_Stringify(43, SEC_ERROR_NO_EMAIL_CERT),
    SEC_ERROR_Stringify(44, SEC_ERROR_NO_RECIPIENT_CERTS_QUERY),
    SEC_ERROR_Stringify(45, SEC_ERROR_NOT_A_RECIPIENT),
    SEC_ERROR_Stringify(46, SEC_ERROR_PKCS7_KEYALG_MISMATCH),
    SEC_ERROR_Stringify(47, SEC_ERROR_PKCS7_BAD_SIGNATURE),
    SEC_ERROR_Stringify(48, SEC_ERROR_UNSUPPORTED_KEYALG),
    SEC_ERROR_Stringify(49, SEC_ERROR_DECRYPTION_DISALLOWED),
    SEC_ERROR_Stringify(50, XP_SEC_FORTEZZA_BAD_CARD),
    SEC_ERROR_Stringify(51, XP_SEC_FORTEZZA_NO_CARD),
    SEC_ERROR_Stringify(52, XP_SEC_FORTEZZA_NONE_SELECTED),
    SEC_ERROR_Stringify(53, XP_SEC_FORTEZZA_MORE_INFO),
    SEC_ERROR_Stringify(54, XP_SEC_FORTEZZA_PERSON_NOT_FOUND),
    SEC_ERROR_Stringify(55, XP_SEC_FORTEZZA_NO_MORE_INFO),
    SEC_ERROR_Stringify(56, XP_SEC_FORTEZZA_BAD_PIN),
    SEC_ERROR_Stringify(57, XP_SEC_FORTEZZA_PERSON_ERROR),
    SEC_ERROR_Stringify(58, SEC_ERROR_NO_KRL),
    SEC_ERROR_Stringify(59, SEC_ERROR_KRL_EXPIRED),
    SEC_ERROR_Stringify(60, SEC_ERROR_KRL_BAD_SIGNATURE),
    SEC_ERROR_Stringify(61, SEC_ERROR_REVOKED_KEY),
    SEC_ERROR_Stringify(62, SEC_ERROR_KRL_INVALID),
    SEC_ERROR_Stringify(63, SEC_ERROR_NEED_RANDOM),
    SEC_ERROR_Stringify(64, SEC_ERROR_NO_MODULE),
    SEC_ERROR_Stringify(65, SEC_ERROR_NO_TOKEN),
    SEC_ERROR_Stringify(66, SEC_ERROR_READ_ONLY),
    SEC_ERROR_Stringify(67, SEC_ERROR_NO_SLOT_SELECTED),
    SEC_ERROR_Stringify(68, SEC_ERROR_CERT_NICKNAME_COLLISION),
    SEC_ERROR_Stringify(69, SEC_ERROR_KEY_NICKNAME_COLLISION),
    SEC_ERROR_Stringify(70, SEC_ERROR_SAFE_NOT_CREATED),
    SEC_ERROR_Stringify(71, SEC_ERROR_BAGGAGE_NOT_CREATED),
    SEC_ERROR_Stringify(72, XP_JAVA_REMOVE_PRINCIPAL_ERROR),
    SEC_ERROR_Stringify(73, XP_JAVA_DELETE_PRIVILEGE_ERROR),
    SEC_ERROR_Stringify(74, XP_JAVA_CERT_NOT_EXISTS_ERROR),
    SEC_ERROR_Stringify(75, SEC_ERROR_BAD_EXPORT_ALGORITHM),
    SEC_ERROR_Stringify(76, SEC_ERROR_EXPORTING_CERTIFICATES),
    SEC_ERROR_Stringify(77, SEC_ERROR_IMPORTING_CERTIFICATES),
    SEC_ERROR_Stringify(78, SEC_ERROR_PKCS12_DECODING_PFX),
    SEC_ERROR_Stringify(79, SEC_ERROR_PKCS12_INVALID_MAC),
    SEC_ERROR_Stringify(80, SEC_ERROR_PKCS12_UNSUPPORTED_MAC_ALGORITHM),
    SEC_ERROR_Stringify(81, SEC_ERROR_PKCS12_UNSUPPORTED_TRANSPORT_MODE),
    SEC_ERROR_Stringify(82, SEC_ERROR_PKCS12_CORRUPT_PFX_STRUCTURE),
    SEC_ERROR_Stringify(83, SEC_ERROR_PKCS12_UNSUPPORTED_PBE_ALGORITHM),
    SEC_ERROR_Stringify(84, SEC_ERROR_PKCS12_UNSUPPORTED_VERSION),
    SEC_ERROR_Stringify(85, SEC_ERROR_PKCS12_PRIVACY_PASSWORD_INCORRECT),
    SEC_ERROR_Stringify(86, SEC_ERROR_PKCS12_CERT_COLLISION),
    SEC_ERROR_Stringify(87, SEC_ERROR_USER_CANCELLED),
    SEC_ERROR_Stringify(88, SEC_ERROR_PKCS12_DUPLICATE_DATA),
    SEC_ERROR_Stringify(89, SEC_ERROR_MESSAGE_SEND_ABORTED),
    SEC_ERROR_Stringify(90, SEC_ERROR_INADEQUATE_KEY_USAGE),
    SEC_ERROR_Stringify(91, SEC_ERROR_INADEQUATE_CERT_TYPE),
    SEC_ERROR_Stringify(92, SEC_ERROR_CERT_ADDR_MISMATCH),
    SEC_ERROR_Stringify(93, SEC_ERROR_PKCS12_UNABLE_TO_IMPORT_KEY),
    SEC_ERROR_Stringify(94, SEC_ERROR_PKCS12_IMPORTING_CERT_CHAIN),
    SEC_ERROR_Stringify(95, SEC_ERROR_PKCS12_UNABLE_TO_LOCATE_OBJECT_BY_NAME),
    SEC_ERROR_Stringify(96, SEC_ERROR_PKCS12_UNABLE_TO_EXPORT_KEY),
    SEC_ERROR_Stringify(97, SEC_ERROR_PKCS12_UNABLE_TO_WRITE),
    SEC_ERROR_Stringify(98, SEC_ERROR_PKCS12_UNABLE_TO_READ),
    SEC_ERROR_Stringify(99, SEC_ERROR_PKCS12_KEY_DATABASE_NOT_INITIALIZED),
    SEC_ERROR_Stringify(100, SEC_ERROR_KEYGEN_FAIL),
    SEC_ERROR_Stringify(101, SEC_ERROR_INVALID_PASSWORD),
    SEC_ERROR_Stringify(102, SEC_ERROR_RETRY_OLD_PASSWORD),
    SEC_ERROR_Stringify(103, SEC_ERROR_BAD_NICKNAME),
    SEC_ERROR_Stringify(104, SEC_ERROR_NOT_FORTEZZA_ISSUER),
    SEC_ERROR_Stringify(105, SEC_ERROR_CANNOT_MOVE_SENSITIVE_KEY),
    SEC_ERROR_Stringify(106, SEC_ERROR_JS_INVALID_MODULE_NAME),
    SEC_ERROR_Stringify(107, SEC_ERROR_JS_INVALID_DLL),
    SEC_ERROR_Stringify(108, SEC_ERROR_JS_ADD_MOD_FAILURE),
    SEC_ERROR_Stringify(109, SEC_ERROR_JS_DEL_MOD_FAILURE),
    SEC_ERROR_Stringify(110, SEC_ERROR_OLD_KRL),
    SEC_ERROR_Stringify(111, SEC_ERROR_CKL_CONFLICT),
    SEC_ERROR_Stringify(112, SEC_ERROR_CERT_NOT_IN_NAME_SPACE),
    SEC_ERROR_Stringify(113, SEC_ERROR_KRL_NOT_YET_VALID),
    SEC_ERROR_Stringify(114, SEC_ERROR_CRL_NOT_YET_VALID),
    SEC_ERROR_Stringify(115, SEC_ERROR_UNKNOWN_CERT),
    SEC_ERROR_Stringify(116, SEC_ERROR_UNKNOWN_SIGNER),
    SEC_ERROR_Stringify(117, SEC_ERROR_CERT_BAD_ACCESS_LOCATION),
    SEC_ERROR_Stringify(118, SEC_ERROR_OCSP_UNKNOWN_RESPONSE_TYPE),
    SEC_ERROR_Stringify(119, SEC_ERROR_OCSP_BAD_HTTP_RESPONSE),
    SEC_ERROR_Stringify(120, SEC_ERROR_OCSP_MALFORMED_REQUEST),
    SEC_ERROR_Stringify(121, SEC_ERROR_OCSP_SERVER_ERROR),
    SEC_ERROR_Stringify(122, SEC_ERROR_OCSP_TRY_SERVER_LATER),
    SEC_ERROR_Stringify(123, SEC_ERROR_OCSP_REQUEST_NEEDS_SIG),
    SEC_ERROR_Stringify(124, SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST),
    SEC_ERROR_Stringify(125, SEC_ERROR_OCSP_UNKNOWN_RESPONSE_STATUS),
    SEC_ERROR_Stringify(126, SEC_ERROR_OCSP_UNKNOWN_CERT),
    SEC_ERROR_Stringify(127, SEC_ERROR_OCSP_NOT_ENABLED),
    SEC_ERROR_Stringify(128, SEC_ERROR_OCSP_NO_DEFAULT_RESPONDER),
    SEC_ERROR_Stringify(129, SEC_ERROR_OCSP_MALFORMED_RESPONSE),
    SEC_ERROR_Stringify(130, SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE),
    SEC_ERROR_Stringify(131, SEC_ERROR_OCSP_FUTURE_RESPONSE),
    SEC_ERROR_Stringify(132, SEC_ERROR_OCSP_OLD_RESPONSE),
    SEC_ERROR_Stringify(133, SEC_ERROR_DIGEST_NOT_FOUND),
    SEC_ERROR_Stringify(134, SEC_ERROR_UNSUPPORTED_MESSAGE_TYPE),
    SEC_ERROR_Stringify(135, SEC_ERROR_MODULE_STUCK),
    SEC_ERROR_Stringify(136, SEC_ERROR_BAD_TEMPLATE),
    SEC_ERROR_Stringify(137, SEC_ERROR_CRL_NOT_FOUND),
    SEC_ERROR_Stringify(138, SEC_ERROR_REUSED_ISSUER_AND_SERIAL),
    SEC_ERROR_Stringify(139, SEC_ERROR_BUSY),
    SEC_ERROR_Stringify(140, SEC_ERROR_NO_USER_INTERACTION),

#undef SEC_ERROR_Stringify

    // SecureTransport errors

    CaseErrorStringifyHardCode(-9800, errSSLProtocol),
    CaseErrorStringifyHardCode(-9801, errSSLNegotiation),
    CaseErrorStringifyHardCode(-9802, errSSLFatalAlert),
    CaseErrorStringifyHardCode(-9803, errSSLWouldBlock),
    CaseErrorStringifyHardCode(-9804, errSSLSessionNotFound),
    CaseErrorStringifyHardCode(-9805, errSSLClosedGraceful),
    CaseErrorStringifyHardCode(-9806, errSSLClosedAbort),
    CaseErrorStringifyHardCode(-9807, errSSLXCertChainInvalid),
    CaseErrorStringifyHardCode(-9808, errSSLBadCert),
    CaseErrorStringifyHardCode(-9809, errSSLCrypto),
    CaseErrorStringifyHardCode(-9810, errSSLInternal),
    CaseErrorStringifyHardCode(-9811, errSSLModuleAttach),
    CaseErrorStringifyHardCode(-9812, errSSLUnknownRootCert),
    CaseErrorStringifyHardCode(-9813, errSSLNoRootCert),
    CaseErrorStringifyHardCode(-9814, errSSLCertExpired),
    CaseErrorStringifyHardCode(-9815, errSSLCertNotYetValid),
    CaseErrorStringifyHardCode(-9816, errSSLClosedNoNotify),
    CaseErrorStringifyHardCode(-9817, errSSLBufferOverflow),
    CaseErrorStringifyHardCode(-9818, errSSLBadCipherSuite),
    CaseErrorStringifyHardCode(-9819, errSSLPeerUnexpectedMsg),
    CaseErrorStringifyHardCode(-9820, errSSLPeerBadRecordMac),
    CaseErrorStringifyHardCode(-9821, errSSLPeerDecryptionFail),
    CaseErrorStringifyHardCode(-9822, errSSLPeerRecordOverflow),
    CaseErrorStringifyHardCode(-9823, errSSLPeerDecompressFail),
    CaseErrorStringifyHardCode(-9824, errSSLPeerHandshakeFail),
    CaseErrorStringifyHardCode(-9825, errSSLPeerBadCert),
    CaseErrorStringifyHardCode(-9826, errSSLPeerUnsupportedCert),
    CaseErrorStringifyHardCode(-9827, errSSLPeerCertRevoked),
    CaseErrorStringifyHardCode(-9828, errSSLPeerCertExpired),
    CaseErrorStringifyHardCode(-9829, errSSLPeerCertUnknown),
    CaseErrorStringifyHardCode(-9830, errSSLIllegalParam),
    CaseErrorStringifyHardCode(-9831, errSSLPeerUnknownCA),
    CaseErrorStringifyHardCode(-9832, errSSLPeerAccessDenied),
    CaseErrorStringifyHardCode(-9833, errSSLPeerDecodeError),
    CaseErrorStringifyHardCode(-9834, errSSLPeerDecryptError),
    CaseErrorStringifyHardCode(-9835, errSSLPeerExportRestriction),
    CaseErrorStringifyHardCode(-9836, errSSLPeerProtocolVersion),
    CaseErrorStringifyHardCode(-9837, errSSLPeerInsufficientSecurity),
    CaseErrorStringifyHardCode(-9838, errSSLPeerInternalError),
    CaseErrorStringifyHardCode(-9839, errSSLPeerUserCancelled),
    CaseErrorStringifyHardCode(-9840, errSSLPeerNoRenegotiation),
    CaseErrorStringifyHardCode(-9841, errSSLPeerAuthCompleted),
    CaseErrorStringifyHardCode(-9842, errSSLClientCertRequested),
    CaseErrorStringifyHardCode(-9843, errSSLHostNameMismatch),
    CaseErrorStringifyHardCode(-9844, errSSLConnectionRefused),
    CaseErrorStringifyHardCode(-9845, errSSLDecryptionFail),
    CaseErrorStringifyHardCode(-9846, errSSLBadRecordMac),
    CaseErrorStringifyHardCode(-9847, errSSLRecordOverflow),
    CaseErrorStringifyHardCode(-9848, errSSLBadConfiguration),
    CaseErrorStringifyHardCode(-9849, errSSLUnexpectedRecord),

#endif // TARGET_OS_DARWIN

#if (KEYCHAIN_LITE_ENABLED && !TARGET_OS_DARWIN)
    CaseErrorStringifyHardCode(-25293, errSecAuthFailed),
    CaseErrorStringifyHardCode(-25299, errSecDuplicateItem),
    CaseErrorStringifyHardCode(-25300, errSecItemNotFound),
#endif

// errno

#ifdef EACCES
    CaseErrorStringify(EACCES),
    CaseErrorStringifyHardCode(-EACCES, EACCES),
#endif

#ifdef EADDRINUSE
    CaseErrorStringify(EADDRINUSE),
    CaseErrorStringifyHardCode(-EADDRINUSE, EADDRINUSE),
#endif

#ifdef EADDRNOTAVAIL
    CaseErrorStringify(EADDRNOTAVAIL),
    CaseErrorStringifyHardCode(-EADDRNOTAVAIL, EADDRNOTAVAIL),
#endif

#ifdef EADV
    CaseErrorStringify(EADV),
    CaseErrorStringifyHardCode(-EADV, EADV),
#endif

#ifdef EAFNOSUPPORT
    CaseErrorStringify(EAFNOSUPPORT),
    CaseErrorStringifyHardCode(-EAFNOSUPPORT, EAFNOSUPPORT),
#endif

#ifdef EALREADY
#if (!defined(EBUSY) || (EALREADY != EBUSY))
    CaseErrorStringify(EALREADY),
    CaseErrorStringifyHardCode(-EALREADY, EALREADY),
#endif
#endif

#ifdef EAUTH
    CaseErrorStringify(EAUTH),
    CaseErrorStringifyHardCode(-EAUTH, EAUTH),
#endif

#ifdef EBADARCH
    CaseErrorStringify(EBADARCH),
    CaseErrorStringifyHardCode(-EBADARCH, EBADARCH),
#endif

#ifdef EBADE
    CaseErrorStringify(EBADE),
    CaseErrorStringifyHardCode(-EBADE, EBADE),
#endif

#ifdef EBADEXEC
    CaseErrorStringify(EBADEXEC),
    CaseErrorStringifyHardCode(-EBADEXEC, EBADEXEC),
#endif

#ifdef EBADF
    CaseErrorStringify(EBADF),
    CaseErrorStringifyHardCode(-EBADF, EBADF),
#endif

#ifdef EBADFD
    CaseErrorStringify(EBADFD),
    CaseErrorStringifyHardCode(-EBADFD, EBADFD),
#endif

#ifdef EBADFSYS
    CaseErrorStringify(EBADFSYS),
    CaseErrorStringifyHardCode(-EBADFSYS, EBADFSYS),
#endif

#ifdef EBADMACHO
    CaseErrorStringify(EBADMACHO),
    CaseErrorStringifyHardCode(-EBADMACHO, EBADMACHO),
#endif

#ifdef EBADMSG
    CaseErrorStringify(EBADMSG),
    CaseErrorStringifyHardCode(-EBADMSG, EBADMSG),
#endif

#ifdef EBADR
    CaseErrorStringify(EBADR),
    CaseErrorStringifyHardCode(-EBADR, EBADR),
#endif

#ifdef EBADRPC
    CaseErrorStringify(EBADRPC),
    CaseErrorStringifyHardCode(-EBADRPC, EBADRPC),
#endif

#ifdef EBADRQC
    CaseErrorStringify(EBADRQC),
    CaseErrorStringifyHardCode(-EBADRQC, EBADRQC),
#endif

#ifdef EBADSLT
    CaseErrorStringify(EBADSLT),
    CaseErrorStringifyHardCode(-EBADSLT, EBADSLT),
#endif

#ifdef EBFONT
    CaseErrorStringify(EBFONT),
    CaseErrorStringifyHardCode(-EBFONT, EBFONT),
#endif

#ifdef EBUSY
    CaseErrorStringify(EBUSY),
    CaseErrorStringifyHardCode(-EBUSY, EBUSY),
#endif

#ifdef ECANCELED
    CaseErrorStringify(ECANCELED),
    CaseErrorStringifyHardCode(-ECANCELED, ECANCELED),
#endif

#ifdef ECHILD
    CaseErrorStringify(ECHILD),
    CaseErrorStringifyHardCode(-ECHILD, ECHILD),
#endif

#ifdef ECHRNG
    CaseErrorStringify(ECHRNG),
    CaseErrorStringifyHardCode(-ECHRNG, ECHRNG),
#endif

#ifdef ECOMM
    CaseErrorStringify(ECOMM),
    CaseErrorStringifyHardCode(-ECOMM, ECOMM),
#endif

#ifdef ECONNABORTED
    CaseErrorStringify(ECONNABORTED),
    CaseErrorStringifyHardCode(-ECONNABORTED, ECONNABORTED),
#endif

#ifdef ECONNREFUSED
    CaseErrorStringify(ECONNREFUSED),
    CaseErrorStringifyHardCode(-ECONNREFUSED, ECONNREFUSED),
#endif

#ifdef ECONNRESET
    CaseErrorStringify(ECONNRESET),
    CaseErrorStringifyHardCode(-ECONNRESET, ECONNRESET),
#endif

#ifdef ECTRLTERM
    CaseErrorStringify(ECTRLTERM),
    CaseErrorStringifyHardCode(-ECTRLTERM, ECTRLTERM),
#endif

#ifdef EDEADLK
    CaseErrorStringify(EDEADLK),
    CaseErrorStringifyHardCode(-EDEADLK, EDEADLK),
#endif

#ifdef EDEADLOCK
    CaseErrorStringify(EDEADLOCK),
    CaseErrorStringifyHardCode(-EDEADLOCK, EDEADLOCK),
#endif

#ifdef EDESTADDRREQ
    CaseErrorStringify(EDESTADDRREQ),
    CaseErrorStringifyHardCode(-EDESTADDRREQ, EDESTADDRREQ),
#endif

#ifdef EDEVERR
    CaseErrorStringify(EDEVERR),
    CaseErrorStringifyHardCode(-EDEVERR, EDEVERR),
#endif

#ifdef EDOM
    CaseErrorStringify(EDOM),
    CaseErrorStringifyHardCode(-EDOM, EDOM),
#endif

#ifdef EDQUOT
    CaseErrorStringify(EDQUOT),
    CaseErrorStringifyHardCode(-EDQUOT, EDQUOT),
#endif

#ifdef EENDIAN
    CaseErrorStringify(EENDIAN),
    CaseErrorStringifyHardCode(-EENDIAN, EENDIAN),
#endif

#ifdef EEXIST
    CaseErrorStringify(EEXIST),
    CaseErrorStringifyHardCode(-EEXIST, EEXIST),
#endif

#ifdef EFAULT
    CaseErrorStringify(EFAULT),
    CaseErrorStringifyHardCode(-EFAULT, EFAULT),
#endif

#ifdef EFBIG
    CaseErrorStringify(EFBIG),
    CaseErrorStringifyHardCode(-EFBIG, EFBIG),
#endif

#ifdef EFPOS
    CaseErrorStringify(EFPOS),
    CaseErrorStringifyHardCode(-EFPOS, EFPOS),
#endif

#ifdef EFTYPE
    CaseErrorStringify(EFTYPE),
    CaseErrorStringifyHardCode(-EFTYPE, EFTYPE),
#endif

#ifdef EHAVEOOB
    CaseErrorStringify(EHAVEOOB),
    CaseErrorStringifyHardCode(-EHAVEOOB, EHAVEOOB),
#endif

#ifdef EHOSTDOWN
    CaseErrorStringify(EHOSTDOWN),
    CaseErrorStringifyHardCode(-EHOSTDOWN, EHOSTDOWN),
#endif

#ifdef EHOSTUNREACH
    CaseErrorStringify(EHOSTUNREACH),
    CaseErrorStringifyHardCode(-EHOSTUNREACH, EHOSTUNREACH),
#endif

#ifdef EIDRM
    CaseErrorStringify(EIDRM),
    CaseErrorStringifyHardCode(-EIDRM, EIDRM),
#endif

#ifdef EIEIO
    CaseErrorStringify(EIEIO),
    CaseErrorStringifyHardCode(-EIEIO, EIEIO),
#endif

#ifdef EILSEQ
    CaseErrorStringify(EILSEQ),
    CaseErrorStringifyHardCode(-EILSEQ, EILSEQ),
#endif

#ifdef EINPROGRESS
    CaseErrorStringify(EINPROGRESS),
    CaseErrorStringifyHardCode(-EINPROGRESS, EINPROGRESS),
#endif

#ifdef EINTR
    CaseErrorStringify(EINTR),
    CaseErrorStringifyHardCode(-EINTR, EINTR),
#endif

#ifdef EINVAL
    CaseErrorStringify(EINVAL),
    CaseErrorStringifyHardCode(-EINVAL, EINVAL),
#endif

#ifdef EIO
    CaseErrorStringify(EIO),
    CaseErrorStringifyHardCode(-EIO, EIO),
#endif

#ifdef EISCONN
    CaseErrorStringify(EISCONN),
    CaseErrorStringifyHardCode(-EISCONN, EISCONN),
#endif

#ifdef EISDIR
    CaseErrorStringify(EISDIR),
    CaseErrorStringifyHardCode(-EISDIR, EISDIR),
#endif

#ifdef EL2HLT
    CaseErrorStringify(EL2HLT),
    CaseErrorStringifyHardCode(-EL2HLT, EL2HLT),
#endif

#ifdef EL2NSYNC
    CaseErrorStringify(EL2NSYNC),
    CaseErrorStringifyHardCode(-EL2NSYNC, EL2NSYNC),
#endif

#ifdef EL3HLT
    CaseErrorStringify(EL3HLT),
    CaseErrorStringifyHardCode(-EL3HLT, EL3HLT),
#endif

#ifdef EL3RST
    CaseErrorStringify(EL3RST),
    CaseErrorStringifyHardCode(-EL3RST, EL3RST),
#endif

#ifdef ELIBACC
    CaseErrorStringify(ELIBACC),
    CaseErrorStringifyHardCode(-ELIBACC, ELIBACC),
#endif

#ifdef ELIBBAD
    CaseErrorStringify(ELIBBAD),
    CaseErrorStringifyHardCode(-ELIBBAD, ELIBBAD),
#endif

#ifdef ELIBEXEC
    CaseErrorStringify(ELIBEXEC),
    CaseErrorStringifyHardCode(-ELIBEXEC, ELIBEXEC),
#endif

#ifdef ELIBMAX
    CaseErrorStringify(ELIBMAX),
    CaseErrorStringifyHardCode(-ELIBMAX, ELIBMAX),
#endif

#ifdef ELIBSCN
    CaseErrorStringify(ELIBSCN),
    CaseErrorStringifyHardCode(-ELIBSCN, ELIBSCN),
#endif

#ifdef ELNRNG
    CaseErrorStringify(ELNRNG),
    CaseErrorStringifyHardCode(-ELNRNG, ELNRNG),
#endif

#ifdef ELOOP
    CaseErrorStringify(ELOOP),
    CaseErrorStringifyHardCode(-ELOOP, ELOOP),
#endif

#ifdef ELOWER
    CaseErrorStringify(ELOWER),
    CaseErrorStringifyHardCode(-ELOWER, ELOWER),
#endif

#ifdef EMFILE
    CaseErrorStringify(EMFILE),
    CaseErrorStringifyHardCode(-EMFILE, EMFILE),
#endif

#ifdef EMLINK
    CaseErrorStringify(EMLINK),
    CaseErrorStringifyHardCode(-EMLINK, EMLINK),
#endif

#ifdef EMORE
    CaseErrorStringify(EMORE),
    CaseErrorStringifyHardCode(-EMORE, EMORE),
#endif

#ifdef EMSGSIZE
    CaseErrorStringify(EMSGSIZE),
    CaseErrorStringifyHardCode(-EMSGSIZE, EMSGSIZE),
#endif

#ifdef EMULTIHOP
    CaseErrorStringify(EMULTIHOP),
    CaseErrorStringifyHardCode(-EMULTIHOP, EMULTIHOP),
#endif

#ifdef ENAMETOOLONG
    CaseErrorStringify(ENAMETOOLONG),
    CaseErrorStringifyHardCode(-ENAMETOOLONG, ENAMETOOLONG),
#endif

#ifdef ENEEDAUTH
    CaseErrorStringify(ENEEDAUTH),
    CaseErrorStringifyHardCode(-ENEEDAUTH, ENEEDAUTH),
#endif

#ifdef ENETDOWN
    CaseErrorStringify(ENETDOWN),
    CaseErrorStringifyHardCode(-ENETDOWN, ENETDOWN),
#endif

#ifdef ENETRESET
    CaseErrorStringify(ENETRESET),
    CaseErrorStringifyHardCode(-ENETRESET, ENETRESET),
#endif

#ifdef ENETUNREACH
    CaseErrorStringify(ENETUNREACH),
    CaseErrorStringifyHardCode(-ENETUNREACH, ENETUNREACH),
#endif

#ifdef ENFILE
    CaseErrorStringify(ENFILE),
    CaseErrorStringifyHardCode(-ENFILE, ENFILE),
#endif

#ifdef ENOANO
    CaseErrorStringify(ENOANO),
    CaseErrorStringifyHardCode(-ENOANO, ENOANO),
#endif

#ifdef ENOATTR
    CaseErrorStringify(ENOATTR),
    CaseErrorStringifyHardCode(-ENOATTR, ENOATTR),
#endif

#ifdef ENOBUFS
    CaseErrorStringify(ENOBUFS),
    CaseErrorStringifyHardCode(-ENOBUFS, ENOBUFS),
#endif

#ifdef ENOCSI
    CaseErrorStringify(ENOCSI),
    CaseErrorStringifyHardCode(-ENOCSI, ENOCSI),
#endif

#ifdef ENODATA
    CaseErrorStringify(ENODATA),
    CaseErrorStringifyHardCode(-ENODATA, ENODATA),
#endif

#ifdef ENODEV
    CaseErrorStringify(ENODEV),
    CaseErrorStringifyHardCode(-ENODEV, ENODEV),
#endif

#ifdef ENOENT
    CaseErrorStringify(ENOENT),
    CaseErrorStringifyHardCode(-ENOENT, ENOENT),
#endif

#ifdef ENOEXEC
    CaseErrorStringify(ENOEXEC),
    CaseErrorStringifyHardCode(-ENOEXEC, ENOEXEC),
#endif

#ifdef ENOLCK
    CaseErrorStringify(ENOLCK),
    CaseErrorStringifyHardCode(-ENOLCK, ENOLCK),
#endif

#ifdef ENOLIC
    CaseErrorStringify(ENOLIC),
    CaseErrorStringifyHardCode(-ENOLIC, ENOLIC),
#endif

#ifdef ENOLINK
    CaseErrorStringify(ENOLINK),
    CaseErrorStringifyHardCode(-ENOLINK, ENOLINK),
#endif

#ifdef ENOMEM
    CaseErrorStringify(ENOMEM),
    CaseErrorStringifyHardCode(-ENOMEM, ENOMEM),
#endif

#ifdef ENOMSG
    CaseErrorStringify(ENOMSG),
    CaseErrorStringifyHardCode(-ENOMSG, ENOMSG),
#endif

#ifdef ENONDP
    CaseErrorStringify(ENONDP),
    CaseErrorStringifyHardCode(-ENONDP, ENONDP),
#endif

#ifdef ENONET
    CaseErrorStringify(ENONET),
    CaseErrorStringifyHardCode(-ENONET, ENONET),
#endif

#ifdef ENOPKG
    CaseErrorStringify(ENOPKG),
    CaseErrorStringifyHardCode(-ENOPKG, ENOPKG),
#endif

#ifdef ENOPOLICY
    CaseErrorStringify(ENOPOLICY),
    CaseErrorStringifyHardCode(-ENOPOLICY, ENOPOLICY),
#endif

#ifdef ENOPROTOOPT
    CaseErrorStringify(ENOPROTOOPT),
    CaseErrorStringifyHardCode(-ENOPROTOOPT, ENOPROTOOPT),
#endif

#ifdef ENOREMOTE
    CaseErrorStringify(ENOREMOTE),
    CaseErrorStringifyHardCode(-ENOREMOTE, ENOREMOTE),
#endif

#ifdef ENOSPC
    CaseErrorStringify(ENOSPC),
    CaseErrorStringifyHardCode(-ENOSPC, ENOSPC),
#endif

#ifdef ENOSR
    CaseErrorStringify(ENOSR),
    CaseErrorStringifyHardCode(-ENOSR, ENOSR),
#endif

#ifdef ENOSTR
    CaseErrorStringify(ENOSTR),
    CaseErrorStringifyHardCode(-ENOSTR, ENOSTR),
#endif

#ifdef ENOSYS
    CaseErrorStringify(ENOSYS),
    CaseErrorStringifyHardCode(-ENOSYS, ENOSYS),
#endif

#ifdef ENOTBLK
    CaseErrorStringify(ENOTBLK),
    CaseErrorStringifyHardCode(-ENOTBLK, ENOTBLK),
#endif

#ifdef ENOTCONN
    CaseErrorStringify(ENOTCONN),
    CaseErrorStringifyHardCode(-ENOTCONN, ENOTCONN),
#endif

#ifdef ENOTDIR
    CaseErrorStringify(ENOTDIR),
    CaseErrorStringifyHardCode(-ENOTDIR, ENOTDIR),
#endif

#ifdef ENOTEMPTY
    CaseErrorStringify(ENOTEMPTY),
    CaseErrorStringifyHardCode(-ENOTEMPTY, ENOTEMPTY),
#endif

#ifdef ENOTRECOVERABLE
    CaseErrorStringify(ENOTRECOVERABLE),
    CaseErrorStringifyHardCode(-ENOTRECOVERABLE, ENOTRECOVERABLE),
#endif

#ifdef ENOTSOCK
    CaseErrorStringify(ENOTSOCK),
    CaseErrorStringifyHardCode(-ENOTSOCK, ENOTSOCK),
#endif

#ifdef ENOTSUP
    CaseErrorStringify(ENOTSUP),
    CaseErrorStringifyHardCode(-ENOTSUP, ENOTSUP),
#endif

#ifdef ENOTTY
    CaseErrorStringify(ENOTTY),
    CaseErrorStringifyHardCode(-ENOTTY, ENOTTY),
#endif

#ifdef ENOTUNIQ
    CaseErrorStringify(ENOTUNIQ),
    CaseErrorStringifyHardCode(-ENOTUNIQ, ENOTUNIQ),
#endif

#ifdef ENXIO
    CaseErrorStringify(ENXIO),
    CaseErrorStringifyHardCode(-ENXIO, ENXIO),
#endif

#ifdef EOPNOTSUPP
    CaseErrorStringify(EOPNOTSUPP),
    CaseErrorStringifyHardCode(-EOPNOTSUPP, EOPNOTSUPP),
#endif

#ifdef EOVERFLOW
    CaseErrorStringify(EOVERFLOW),
    CaseErrorStringifyHardCode(-EOVERFLOW, EOVERFLOW),
#endif

#ifdef EOWNERDEAD
    CaseErrorStringify(EOWNERDEAD),
    CaseErrorStringifyHardCode(-EOWNERDEAD, EOWNERDEAD),
#endif

#ifdef EPERM
    CaseErrorStringify(EPERM),
    CaseErrorStringifyHardCode(-EPERM, EPERM),
#endif

#ifdef EPFNOSUPPORT
    CaseErrorStringify(EPFNOSUPPORT),
    CaseErrorStringifyHardCode(-EPFNOSUPPORT, EPFNOSUPPORT),
#endif

#ifdef EPIPE
    CaseErrorStringify(EPIPE),
    CaseErrorStringifyHardCode(-EPIPE, EPIPE),
#endif

#ifdef EPROCLIM
    CaseErrorStringify(EPROCLIM),
    CaseErrorStringifyHardCode(-EPROCLIM, EPROCLIM),
#endif

#ifdef EPROCUNAVAIL
    CaseErrorStringify(EPROCUNAVAIL),
    CaseErrorStringifyHardCode(-EPROCUNAVAIL, EPROCUNAVAIL),
#endif

#ifdef EPROGMISMATCH
    CaseErrorStringify(EPROGMISMATCH),
    CaseErrorStringifyHardCode(-EPROGMISMATCH, EPROGMISMATCH),
#endif

#ifdef EPROGUNAVAIL
    CaseErrorStringify(EPROGUNAVAIL),
    CaseErrorStringifyHardCode(-EPROGUNAVAIL, EPROGUNAVAIL),
#endif

#ifdef EPROTO
    CaseErrorStringify(EPROTO),
    CaseErrorStringifyHardCode(-EPROTO, EPROTO),
#endif

#ifdef EPROTONOSUPPORT
    CaseErrorStringify(EPROTONOSUPPORT),
    CaseErrorStringifyHardCode(-EPROTONOSUPPORT, EPROTONOSUPPORT),
#endif

#ifdef EPROTOTYPE
    CaseErrorStringify(EPROTOTYPE),
    CaseErrorStringifyHardCode(-EPROTOTYPE, EPROTOTYPE),
#endif

#ifdef EPWROFF
    CaseErrorStringify(EPWROFF),
    CaseErrorStringifyHardCode(-EPWROFF, EPWROFF),
#endif

#ifdef EQFULL
    CaseErrorStringify(EQFULL),
    CaseErrorStringifyHardCode(-EQFULL, EQFULL),
#endif

#ifdef ERANGE
    CaseErrorStringify(ERANGE),
    CaseErrorStringifyHardCode(-ERANGE, ERANGE),
#endif

#ifdef EREMCHG
    CaseErrorStringify(EREMCHG),
    CaseErrorStringifyHardCode(-EREMCHG, EREMCHG),
#endif

#ifdef EREMOTE
    CaseErrorStringify(EREMOTE),
    CaseErrorStringifyHardCode(-EREMOTE, EREMOTE),
#endif

#ifdef ERESTART
    CaseErrorStringify(ERESTART),
    CaseErrorStringifyHardCode(-ERESTART, ERESTART),
#endif

#ifdef EROFS
    CaseErrorStringify(EROFS),
    CaseErrorStringifyHardCode(-EROFS, EROFS),
#endif

#ifdef ERPCMISMATCH
    CaseErrorStringify(ERPCMISMATCH),
    CaseErrorStringifyHardCode(-ERPCMISMATCH, ERPCMISMATCH),
#endif

#ifdef ESHLIBVERS
    CaseErrorStringify(ESHLIBVERS),
    CaseErrorStringifyHardCode(-ESHLIBVERS, ESHLIBVERS),
#endif

#ifdef ESHUTDOWN
    CaseErrorStringify(ESHUTDOWN),
    CaseErrorStringifyHardCode(-ESHUTDOWN, ESHUTDOWN),
#endif

#ifdef ESOCKTNOSUPPORT
    CaseErrorStringify(ESOCKTNOSUPPORT),
    CaseErrorStringifyHardCode(-ESOCKTNOSUPPORT, ESOCKTNOSUPPORT),
#endif

#ifdef ESPIPE
    CaseErrorStringify(ESPIPE),
    CaseErrorStringifyHardCode(-ESPIPE, ESPIPE),
#endif

#ifdef ESRCH
    CaseErrorStringify(ESRCH),
    CaseErrorStringifyHardCode(-ESRCH, ESRCH),
#endif

#ifdef ESRMNT
    CaseErrorStringify(ESRMNT),
    CaseErrorStringifyHardCode(-ESRMNT, ESRMNT),
#endif

#ifdef ESRVRFAULT
    CaseErrorStringify(ESRVRFAULT),
    CaseErrorStringifyHardCode(-ESRVRFAULT, ESRVRFAULT),
#endif

#ifdef ESTALE
    CaseErrorStringify(ESTALE),
    CaseErrorStringifyHardCode(-ESTALE, ESTALE),
#endif

#ifdef ESTRPIPE
    CaseErrorStringify(ESTRPIPE),
    CaseErrorStringifyHardCode(-ESTRPIPE, ESTRPIPE),
#endif

#ifdef ETIME
    CaseErrorStringify(ETIME),
    CaseErrorStringifyHardCode(-ETIME, ETIME),
#endif

#ifdef ETIMEDOUT
    CaseErrorStringify(ETIMEDOUT),
    CaseErrorStringifyHardCode(-ETIMEDOUT, ETIMEDOUT),
#endif

#ifdef ETOOMANYREFS
    CaseErrorStringify(ETOOMANYREFS),
    CaseErrorStringifyHardCode(-ETOOMANYREFS, ETOOMANYREFS),
#endif

#ifdef ETXTBSY
    CaseErrorStringify(ETXTBSY),
    CaseErrorStringifyHardCode(-ETXTBSY, ETXTBSY),
#endif

#ifdef EUNATCH
    CaseErrorStringify(EUNATCH),
    CaseErrorStringifyHardCode(-EUNATCH, EUNATCH),
#endif

#ifdef EUSERS
    CaseErrorStringify(EUSERS),
    CaseErrorStringifyHardCode(-EUSERS, EUSERS),
#endif

#ifdef EWOULDBLOCK
    CaseErrorStringify(EWOULDBLOCK),
    CaseErrorStringifyHardCode(-EWOULDBLOCK, EWOULDBLOCK),
#endif

#ifdef EXDEV
    CaseErrorStringify(EXDEV),
    CaseErrorStringifyHardCode(-EXDEV, EXDEV),
#endif

#ifdef EXFULL
    CaseErrorStringify(EXFULL),
    CaseErrorStringifyHardCode(-EXFULL, EXFULL),
#endif

// signals

#ifdef SIGHUP
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGHUP), SIGHUP),
#endif

#ifdef SIGINT
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGINT), SIGINT),
#endif

#ifdef SIGQUIT
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGQUIT), SIGQUIT),
#endif

#ifdef SIGILL
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGILL), SIGILL),
#endif

#ifdef SIGTRAP
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGTRAP), SIGTRAP),
#endif

#ifdef SIGABRT
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGABRT), SIGABRT),
#endif

#ifdef SIGPOLL
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGPOLL), SIGPOLL),
#endif

#ifdef SIGEMT
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGEMT), SIGEMT),
#endif

#ifdef SIGFPE
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGFPE), SIGFPE),
#endif

#ifdef SIGKILL
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGKILL), SIGKILL),
#endif

#ifdef SIGBUS
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGBUS), SIGBUS),
#endif

#ifdef SIGSEGV
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGSEGV), SIGSEGV),
#endif

#ifdef SIGSYS
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGSYS), SIGSYS),
#endif

#ifdef SIGPIPE
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGPIPE), SIGPIPE),
#endif

#ifdef SIGALRM
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGALRM), SIGALRM),
#endif

#ifdef SIGTERM
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGTERM), SIGTERM),
#endif

#ifdef SIGURG
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGURG), SIGURG),
#endif

#ifdef SIGSTOP
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGSTOP), SIGSTOP),
#endif

#ifdef SIGTSTP
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGTSTP), SIGTSTP),
#endif

#ifdef SIGCONT
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGCONT), SIGCONT),
#endif

#ifdef SIGCHLD
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGCHLD), SIGCHLD),
#endif

#ifdef SIGTTIN
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGTTIN), SIGTTIN),
#endif

#ifdef SIGTTOU
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGTTOU), SIGTTOU),
#endif

#ifdef SIGIO
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGIO), SIGIO),
#endif

#ifdef SIGXCPU
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGXCPU), SIGXCPU),
#endif

#ifdef SIGXFSZ
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGXFSZ), SIGXFSZ),
#endif

#ifdef SIGVTALRM
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGVTALRM), SIGVTALRM),
#endif

#ifdef SIGPROF
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGPROF), SIGPROF),
#endif

#ifdef SIGWINCH
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGWINCH), SIGWINCH),
#endif

#ifdef SIGINFO
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGINFO), SIGINFO),
#endif

#ifdef SIGUSR1
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGUSR1), SIGUSR1),
#endif

#ifdef SIGUSR2
    CaseErrorStringifyHardCode(POSIXSignalToOSStatus(SIGUSR2), SIGUSR2),
#endif

    CaseEnd()
};

//===========================================================================================================================
//	DebugGetErrorString
//===========================================================================================================================

#if (TARGET_OS_MACOSX)
typedef const char* (*GetMacOSStatusErrorString_f)(OSStatus inStatus);
#endif

const char* DebugGetErrorString(OSStatus inErrorCode, char* inBuffer, size_t inBufferSize)
{
    const DebugErrorEntry* e;
    const char* s;
    char* dst;
    char* end;
#if (TARGET_OS_WINDOWS && !TARGET_OS_WINDOWS_CE)
    char buf[256];
#endif

    // Check for HTTP status codes and the HTTP range for OSStatus.

    if ((inErrorCode >= 100) && (inErrorCode <= 599)) {
        s = HTTPGetReasonPhrase(inErrorCode);
        if (*s != '\0')
            goto gotIt;
    } else if ((inErrorCode >= 200100) && (inErrorCode <= 200599)) {
        s = HTTPGetReasonPhrase(inErrorCode - 200000);
        if (*s != '\0')
            goto gotIt;
    }

    // Search our own table of error strings. If not found, fall back to other methods.

    for (e = kDebugErrors; ((s = e->str) != NULL) && (e->err != inErrorCode); ++e) {
    }
    if (s)
        goto gotIt;

#if (TARGET_OS_WINDOWS && !TARGET_OS_WINDOWS_CE)
    // If on Windows, try FormatMessage.

    if (inBuffer && (inBufferSize > 0)) {
        DWORD n;

        n = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, (DWORD)inErrorCode,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, sizeof(buf), NULL);
        if (n > 0) {
            // Remove any trailing CR's or LF's since some messages have them.

            while ((n > 0) && isspace_safe(buf[n - 1]))
                buf[--n] = '\0';
            s = buf;
        }
    }

#elif (TARGET_OS_MACOSX && !COMMON_SERVICES_NO_CORE_SERVICES)
    // If on the Mac, try GetMacOSStatusErrorString.
    {
        static dispatch_once_t sOnce = 0;
        static GetMacOSStatusErrorString_f sGetMacOSStatusErrorString_f = NULL;

        dispatch_once(&sOnce,
            ^{
                void* dlh;

                dlh = dlopen("/System/Library/Frameworks/CoreServices.framework/CoreServices", RTLD_LAZY | RTLD_LOCAL);
                if (dlh) {
                    sGetMacOSStatusErrorString_f = (GetMacOSStatusErrorString_f)(uintptr_t)dlsym(dlh, "GetMacOSStatusErrorString");
                }
            });
        if (sGetMacOSStatusErrorString_f) {
            s = sGetMacOSStatusErrorString_f(inErrorCode);
            if (s && (*s == '\0'))
                s = NULL;
        }
    }
#endif

    // If we still haven't found a string, try the ANSI C strerror.

    if (!s) {
#if (TARGET_HAS_STD_C_LIB && !TARGET_OS_WINDOWS_CE)
        s = strerror(inErrorCode);
#endif
        if (!s)
            s = "<< NO ERROR STRING >>";
    }

// Copy the string to the output buffer. If no buffer is supplied or it is empty, return an empty string.

gotIt:
    if (inBuffer && (inBufferSize > 0)) {
        dst = inBuffer;
        end = dst + (inBufferSize - 1);
        while (((end - dst) > 0) && (*s != '\0'))
            *dst++ = *s++;
        *dst = '\0';
        s = inBuffer;
    }
    return (s);
}

//===========================================================================================================================
//	DebugGetNextError
//===========================================================================================================================

OSStatus DebugGetNextError(size_t inIndex, OSStatus* outErr)
{
    if (inIndex < (countof(kDebugErrors) - 1)) {
        *outErr = kDebugErrors[inIndex].err;
        return (kNoErr);
    }
    return (kRangeErr);
}
#endif // DEBUG || DEBUG_EXPORT_ERROR_STRINGS

#if (TARGET_OS_DARWIN && !TARGET_IPHONE_SIMULATOR && !TARGET_KERNEL && !COMMON_SERVICES_NO_CORE_SERVICES)
//===========================================================================================================================
//	DebugCopyStackTrace
//===========================================================================================================================

typedef CSSymbolicatorRef (*CSSymbolicatorCreateWithTask_f)(task_t task);
typedef CSSourceInfoRef (*CSSymbolicatorGetSourceInfoWithAddressAtTime_f)(CSSymbolicatorRef, mach_vm_address_t, CSMachineTime);
typedef const char* (*CSSourceInfoGetFilename_f)(CSSourceInfoRef);
typedef uint32_t (*CSSourceInfoGetLineNumber_f)(CSSourceInfoRef);
typedef CSSymbolRef (*CSSymbolicatorGetSymbolWithAddressAtTime_f)(CSSymbolicatorRef, mach_vm_address_t, CSMachineTime);
typedef const char* (*CSSymbolGetName_f)(CSSymbolRef);
typedef CSSymbolOwnerRef (*CSSymbolGetSymbolOwner_f)(CSSymbolRef);
typedef const char* (*CSSymbolOwnerGetName_f)(CSSymbolOwnerRef);
typedef bool (*CSIsNull_f)(CSTypeRef);
typedef void (*CSRelease_f)(CSTypeRef);

char* DebugCopyStackTrace(OSStatus* outErr)
{
    static dispatch_once_t sInitOnce = 0;
    static void* sCoreSymbolicationLib = NULL;
    static CSSymbolicatorCreateWithTask_f sCSSymbolicatorCreateWithTask = NULL;
    static CSSymbolicatorGetSourceInfoWithAddressAtTime_f sCSSymbolicatorGetSourceInfoWithAddressAtTime = NULL;
    static CSSourceInfoGetFilename_f sCSSourceInfoGetFilename = NULL;
    static CSSourceInfoGetLineNumber_f sCSSourceInfoGetLineNumber = NULL;
    static CSSymbolicatorGetSymbolWithAddressAtTime_f sCSSymbolicatorGetSymbolWithAddressAtTime = NULL;
    static CSSymbolGetName_f sCSSymbolGetName = NULL;
    static CSSymbolGetSymbolOwner_f sCSSymbolGetSymbolOwner = NULL;
    static CSSymbolOwnerGetName_f sCSSymbolOwnerGetName = NULL;
    static CSIsNull_f sCSIsNull = NULL;
    static CSRelease_f sCSRelease = NULL;

    char* result = NULL;
    char* trace = NULL;
    CSSymbolicatorRef symbolicator = kCSNull;
    Boolean hasCoreSym = false;
    OSStatus err;
    void* stack[64];
    int j, i, n;
    uintptr_t addr;
    CSSourceInfoRef sourceInfo;
    CSSymbolRef symbol;
    CSSymbolOwnerRef owner;
    const char* ownerName;
    const char* filename;
    const char* function;
    uint32_t lineNum;
    Boolean hasExtra;

    dispatch_once(&sInitOnce,
        ^{
            sCoreSymbolicationLib = dlopen("/System/Library/PrivateFrameworks/CoreSymbolication.framework/CoreSymbolication",
                RTLD_LAZY | RTLD_LOCAL);
            if (sCoreSymbolicationLib) {
                sCSSymbolicatorCreateWithTask = (CSSymbolicatorCreateWithTask_f)(uintptr_t)
                    dlsym(sCoreSymbolicationLib, "CSSymbolicatorCreateWithTask");

                sCSSymbolicatorGetSourceInfoWithAddressAtTime = (CSSymbolicatorGetSourceInfoWithAddressAtTime_f)(uintptr_t)
                    dlsym(sCoreSymbolicationLib, "CSSymbolicatorGetSourceInfoWithAddressAtTime");

                sCSSourceInfoGetFilename = (CSSourceInfoGetFilename_f)(uintptr_t)
                    dlsym(sCoreSymbolicationLib, "CSSourceInfoGetFilename");

                sCSSourceInfoGetLineNumber = (CSSourceInfoGetLineNumber_f)(uintptr_t)
                    dlsym(sCoreSymbolicationLib, "CSSourceInfoGetLineNumber");

                sCSSymbolicatorGetSymbolWithAddressAtTime = (CSSymbolicatorGetSymbolWithAddressAtTime_f)(uintptr_t)
                    dlsym(sCoreSymbolicationLib, "CSSymbolicatorGetSymbolWithAddressAtTime");

                sCSSymbolGetName = (CSSymbolGetName_f)(uintptr_t)dlsym(sCoreSymbolicationLib, "CSSymbolGetName");
                sCSSymbolGetSymbolOwner = (CSSymbolGetSymbolOwner_f)(uintptr_t)dlsym(sCoreSymbolicationLib, "CSSymbolGetSymbolOwner");
                sCSSymbolOwnerGetName = (CSSymbolOwnerGetName_f)(uintptr_t)dlsym(sCoreSymbolicationLib, "CSSymbolOwnerGetName");
                sCSIsNull = (CSIsNull_f)(uintptr_t)dlsym(sCoreSymbolicationLib, "CSIsNull");
                sCSRelease = (CSRelease_f)(uintptr_t)dlsym(sCoreSymbolicationLib, "CSRelease");
            }
        });
    if (sCSSymbolicatorCreateWithTask && sCSSymbolicatorGetSourceInfoWithAddressAtTime && sCSSourceInfoGetFilename && sCSSourceInfoGetLineNumber && sCSSymbolicatorGetSymbolWithAddressAtTime && sCSSymbolGetName && sCSSymbolGetSymbolOwner && sCSSymbolOwnerGetName && sCSIsNull && sCSRelease) {
        hasCoreSym = true;
    }
    if (hasCoreSym) {
        symbolicator = sCSSymbolicatorCreateWithTask(mach_task_self());
        if (sCSIsNull(symbolicator)) {
            hasCoreSym = false;
        }
    }

    // Print frames in reverse order (parent to child), but exclude the last one (this function).

    AppendPrintF(&trace, ""); // Empty string so it's non-null in case there are no frames.
    n = backtrace(stack, (int)countof(stack));
    j = 2;
    for (i = j + 1; i < (n - 1); ++i) {
        addr = (uintptr_t)stack[n - i];
        ownerName = NULL;
        filename = NULL;
        function = NULL;
        lineNum = 0;

        if (hasCoreSym) {
            sourceInfo = sCSSymbolicatorGetSourceInfoWithAddressAtTime(symbolicator, addr, kCSNow);
            if (!sCSIsNull(sourceInfo)) {
                filename = sCSSourceInfoGetFilename(sourceInfo);
                lineNum = sCSSourceInfoGetLineNumber(sourceInfo);
            }

            symbol = sCSSymbolicatorGetSymbolWithAddressAtTime(symbolicator, addr, kCSNow);
            if (!sCSIsNull(symbol)) {
                owner = sCSSymbolGetSymbolOwner(symbol);
                if (!sCSIsNull(owner)) {
                    ownerName = sCSSymbolOwnerGetName(owner);
                }
                function = sCSSymbolGetName(symbol);
            }
        }

        hasExtra = ownerName || filename;
        AppendPrintF(&trace, "%2d%*s%p   %###s%s", i - j, i - 1, "", addr, function ? function : "", hasExtra ? " " : "\n");
        if (hasExtra) {
            AppendPrintF(&trace, "(%s%s%s%s%?u)\n", ownerName ? ownerName : "", (ownerName && filename) ? ", " : "",
                filename ? filename : "", filename ? ":" : "", filename != NULL, lineNum);
        }
    }

    result = trace;
    trace = NULL;
    err = result ? kNoErr : kUnknownErr;

    if (sCSIsNull && !sCSIsNull(symbolicator) && sCSRelease)
        sCSRelease(symbolicator);
    if (outErr)
        *outErr = err;
    return (result);
}
#endif // TARGET_OS_DARWIN && !TARGET_IPHONE_SIMULATOR && !TARGET_KERNEL

//===========================================================================================================================
//	DebugCopyStackTrace
//===========================================================================================================================

#if (TARGET_IPHONE_SIMULATOR || TARGET_OS_LINUX || (TARGET_OS_DARWIN && COMMON_SERVICES_NO_CORE_SERVICES))
char* DebugCopyStackTrace(OSStatus* outErr)
{
    char* trace = NULL;
#if (!TARGET_OS_ANDROID)
    void* stack[64];
    char** symbols = NULL;
    int i, j, n;

    n = backtrace(stack, (int)countof(stack));
    if (n > 0)
        symbols = backtrace_symbols(stack, n);

    // Print frames in reverse order (parent to child), but exclude the last one (this function).

    AppendPrintF(&trace, ""); // Empty string so it's non-null in case there are no frames.
    if (symbols) {
        for (i = 0; i < (n - 1); ++i) {
            j = (n - i) - 1;
            AppendPrintF(&trace, "%2d %*s %p  %s\n", i + 1, i, "", stack[j], symbols[j]);
        }
    }

    FreeNullSafe(symbols);
#endif
    if (outErr)
        *outErr = trace ? kNoErr : kUnknownErr;
    return (trace);
}
#endif

//===========================================================================================================================
//	DebugCopyStackTrace
//===========================================================================================================================

#if (TARGET_OS_QNX)
char* DebugCopyStackTrace(OSStatus* outErr)
{
    char* trace = NULL;
    bt_memmap_t map;
    bt_addr_t stack[64];
    int i, j, n, nn;
    char symbol[128];

    bt_load_memmap(&bt_acc_self, &map);
    n = bt_get_backtrace(&bt_acc_self, stack, (int)countof(stack));
    AppendPrintF(&trace, ""); // Empty string so it's non-null in case there are no frames.
    for (i = 0; i < n; ++i) {
        j = (n - i) - 1;
        *symbol = '\0';
        nn = bt_sprnf_addrs(&map, &stack[j], 1, "%a (%f)", symbol, sizeof(symbol), NULL);
        if (nn != 1) {
            symbol[0] = '?';
            symbol[1] = '\0';
        }
        AppendPrintF(&trace, "%2d %*s %p  %s\n", i + 1, i, "", stack[j], symbol);
    }
    bt_unload_memmap(&map);

    if (outErr)
        *outErr = trace ? kNoErr : kUnknownErr;
    return (trace);
}
#endif

#if (!TARGET_OS_WINDOWS)
//===========================================================================================================================
//	DebugStackTrace
//===========================================================================================================================

OSStatus DebugStackTrace(LogLevel inLevel)
{
    OSStatus err;

#if (LOGUTILS_ENABLED)
    if (!log_category_enabled(&log_category_from_name(DebugServicesStackTrace), inLevel)) {
        err = kRangeErr;
        goto exit;
    }
#endif

#if ((TARGET_OS_DARWIN && !TARGET_KERNEL) || TARGET_OS_LINUX || TARGET_OS_QNX)
    {
        char* trace;

        trace = DebugCopyStackTrace(&err);
        require_noerr_quiet(err, exit);

        dbs_ulog(inLevel, "\n%s", trace);
        free(trace);
        err = kNoErr;
    }
#elif (TARGET_OS_DARWIN && TARGET_KERNEL)
    {
        void* stack[32];
        unsigned int i, n, j;

        // Print frames in reverse order (parent to child).

        n = OSBacktrace(stack, 32);
        for (i = 0; i < n; ++i) {
            j = (n - i) - 1;
            dbs_ulog(inLevel, "%2d %*s %p\n", j, i, "", stack[j]);
        }
        dbs_ulog(inLevel, "\n");
        err = kNoErr;
    }
#elif (TARGET_OS_NETBSD && TARGET_KERNEL)

    db_stack_trace_print((db_expr_t)__builtin_frame_address(0), TRUE, 65535, "u", printf);
    err = kNoErr;

#elif (TARGET_OS_NETBSD && !TARGET_KERNEL)
    {
        int debugNow;

        debugNow = 2; // 2 means "print a stack trace".
        // err = sysctlbyname( "ddb.debug_now", NULL, NULL, &debugNow, sizeof( debugNow ) );
        abort();
        err = kNoErr;
    }
#else
    (void)inLevel;

    dbs_ulog(kLogLevelError, "### stack tracing not supported on this platform\n");
    err = kUnsupportedErr;
#endif
    goto exit;

exit:
    return (err);
}
#endif // !TARGET_OS_WINDOWS

#if (TARGET_OS_WINDOWS)
//===========================================================================================================================
//	DebugStackTrace
//===========================================================================================================================

typedef BOOL(__stdcall* StackWalk64Func)(
    __in DWORD inMachineType,
    __in HANDLE inProcess,
    __in HANDLE inThread,
    __inout LPSTACKFRAME64 ioStackFrame,
    __inout PVOID ioCurrentRecord,
    __in_opt PREAD_PROCESS_MEMORY_ROUTINE64 inReadMemoryRoutine,
    __in_opt PFUNCTION_TABLE_ACCESS_ROUTINE64 inFunctionTableAccessRoutine,
    __in_opt PGET_MODULE_BASE_ROUTINE64 inGetModuleBaseRoutine,
    __in_opt PTRANSLATE_ADDRESS_ROUTINE64 inTranslateAddress);

typedef DWORD64(__stdcall* SymGetModuleBase64Func)(
    __in HANDLE inProcess,
    __in DWORD64 inAddr);

typedef BOOL(__stdcall* SymFromAddrFunc)(
    HANDLE inProcess,
    DWORD64 inAddress,
    PDWORD64 inDisplacement,
    PSYMBOL_INFO inSymbol);

typedef PVOID(__stdcall* SymFunctionTableAccess64Func)(
    __in HANDLE inProcess,
    __in DWORD64 inAddrBase);

typedef BOOL(__stdcall* SymInitializeFunc)(
    __in HANDLE inProcess,
    __in_opt PCSTR oinUserSearchPath,
    __in BOOL inInvadeProcess);

typedef DWORD(__stdcall* SymSetOptionsFunc)(__in DWORD inSymOptions);

typedef struct
{
    DWORD64 addr;
    char name[128];

} StackLevel;

static Boolean gDbgHelpInitialized;
static CRITICAL_SECTION gDbgHelpLock;
static LONG gDbgHelpLockState = 0;
static HMODULE gDbgHelpDLL = NULL;
static StackWalk64Func gStachWalkFunc = NULL;
static SymGetModuleBase64Func gSymGetModuleBase64Func = NULL;
static SymFromAddrFunc gSymFromAddrFunc = NULL;
static SymFunctionTableAccess64Func gSymFunctionTableAccess64Func = NULL;
static SymInitializeFunc gSymInitializeFunc = NULL;
static SymSetOptionsFunc gSymSetOptionsFunc = NULL;

OSStatus DebugStackTrace(LogLevel inLevel)
{
    OSStatus err;
    StackLevel stack[64];
    StackLevel* levelPtr;
    CONTEXT currentContext;
    HANDLE currentProcess;
    HANDLE currentThread;
    DWORD machineType;
    STACKFRAME64 stackFrame;
    size_t i, n, j;
    BOOL good;
    size_t len;

    // Skip if specified level is below the current level.

    if (!log_category_enabled(&log_category_from_name(DebugServicesStackTrace), inLevel)) {
        err = kRangeErr;
        goto exit2;
    }

    currentProcess = GetCurrentProcess();
    currentThread = GetCurrentThread();
    RtlCaptureContext(&currentContext);

    InitializeCriticalSectionOnce(&gDbgHelpLock, &gDbgHelpLockState);
    EnterCriticalSection(&gDbgHelpLock);

    // Load symbols dynamically since they are in Dbghelp.dll, which may not always be available.

    if (!gDbgHelpInitialized) {
        gDbgHelpInitialized = true; // Mark initialized early for easier cleanup. Lock prevents other threads.

        gDbgHelpDLL = LoadLibrary(TEXT("Dbghelp"));
        if (gDbgHelpDLL) {
            gStachWalkFunc = (StackWalk64Func)(uintptr_t)GetProcAddress(gDbgHelpDLL, "StackWalk64");
            require_action_quiet(gStachWalkFunc, exit, err = kUnsupportedErr);

            gSymGetModuleBase64Func = (SymGetModuleBase64Func)(uintptr_t)GetProcAddress(gDbgHelpDLL,
                "SymGetModuleBase64");
            require_action_quiet(gSymGetModuleBase64Func, exit, err = kUnsupportedErr);

            gSymFromAddrFunc = (SymFromAddrFunc)(uintptr_t)GetProcAddress(gDbgHelpDLL, "SymFromAddr");
            require_action_quiet(gSymFromAddrFunc, exit, err = kUnsupportedErr);

            gSymFunctionTableAccess64Func = (SymFunctionTableAccess64Func)(uintptr_t)GetProcAddress(gDbgHelpDLL,
                "SymFunctionTableAccess64");
            require_action_quiet(gSymFunctionTableAccess64Func, exit, err = kUnsupportedErr);

            gSymInitializeFunc = (SymInitializeFunc)(uintptr_t)GetProcAddress(gDbgHelpDLL, "SymInitialize");
            require_action_quiet(gSymInitializeFunc, exit, err = kUnsupportedErr);

            gSymSetOptionsFunc = (SymSetOptionsFunc)(uintptr_t)GetProcAddress(gDbgHelpDLL, "SymSetOptions");
            require_action_quiet(gSymSetOptionsFunc, exit, err = kUnsupportedErr);
        }

        gSymSetOptionsFunc(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
        good = gSymInitializeFunc(currentProcess, NULL, TRUE);
        err = map_global_value_errno(good, good);
        require_noerr_quiet(err, exit);
    }

    // Setup the info needed by StackWalk64.

    memset(&stackFrame, 0, sizeof(stackFrame));
#ifdef _M_IX86
    machineType = IMAGE_FILE_MACHINE_I386;
    stackFrame.AddrPC.Offset = currentContext.Eip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = currentContext.Ebp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = currentContext.Esp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#elif _M_X64
    machineType = IMAGE_FILE_MACHINE_AMD64;
    stackFrame.AddrPC.Offset = currentContext.Rip;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = currentContext.Rsp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = currentContext.Rsp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#elif _M_IA64
    machineType = IMAGE_FILE_MACHINE_IA64;
    stackFrame.AddrPC.Offset = currentContext.StIIP;
    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Offset = currentContext.IntSp;
    stackFrame.AddrFrame.Mode = AddrModeFlat;
    stackFrame.AddrBStore.Offset = currentContext.RsBSP;
    stackFrame.AddrBStore.Mode = AddrModeFlat;
    stackFrame.AddrStack.Offset = currentContext.IntSp;
    stackFrame.AddrStack.Mode = AddrModeFlat;
#else
#error "unsupported platform"
#endif

    // Walk up the stack and save off the PC in each frame.

    for (n = 0; n < countof(stack); ++n) {
        good = gStachWalkFunc(machineType, currentProcess, currentThread, &stackFrame, &currentContext,
            NULL, gSymFunctionTableAccess64Func, gSymGetModuleBase64Func, NULL);
        if (!good)
            break;

        if (stackFrame.AddrPC.Offset != 0) {
            stack[n].addr = stackFrame.AddrPC.Offset;
        } else {
            break;
        }
    }

    // Get the function name at each level.

    for (i = 0; i < n; ++i) {
        SYMBOL_INFO_PACKAGE symbolInfo;
        DWORD64 displacement;

        j = (n - i) - 1;
        levelPtr = &stack[j];
        memset(&symbolInfo.si, 0, sizeof(symbolInfo.si));
        symbolInfo.si.SizeOfStruct = sizeof(symbolInfo.si);
        symbolInfo.si.MaxNameLen = (ULONG)sizeof(symbolInfo.name);
        good = gSymFromAddrFunc(currentProcess, levelPtr->addr, &displacement, &symbolInfo.si);
        if (good) {
            len = strlen(symbolInfo.si.Name);
            len = Min(len, sizeof(levelPtr->name) - 1);
            memcpy(levelPtr->name, symbolInfo.si.Name, len);
            levelPtr->name[len] = '\0';
        } else {
            levelPtr->name[0] = '\0';
        }
    }

    LeaveCriticalSection(&gDbgHelpLock);

    // Skip unimportant functions.

    for (; n > 0; --n) {
        levelPtr = &stack[n - 1];
        if (strcmp(levelPtr->name, "RtlInitializeExceptionChain") == 0)
            continue;
        if (strcmp(levelPtr->name, "BaseThreadInitThunk") == 0)
            continue;
        if (strcmp(levelPtr->name, "mainCRTStartup") == 0)
            continue;
        if (strcmp(levelPtr->name, "__tmainCRTStartup") == 0)
            continue;
        if (strcmp(levelPtr->name, "wWinMainCRTStartup") == 0)
            continue;
        break;
    }

    // Print each function from top (first called) to bottom (deepest).

    for (i = 0; i < n; ++i) {
        j = (n - i) - 1;
        levelPtr = &stack[j];
        if (*levelPtr->name != '\0') {
            dbs_ulog(inLevel, "%2d %*s %s\n", j, (int)i, "", levelPtr->name);
        } else {
            dbs_ulog(inLevel, "%2d %*s %p ???\n", j, (int)i, "", (uintptr_t)levelPtr->addr);
        }
    }
    err = kNoErr;
    goto exit2;

exit:
    if (err && gDbgHelpDLL) {
        FreeLibrary(gDbgHelpDLL);
        gDbgHelpDLL = NULL;
    }
    LeaveCriticalSection(&gDbgHelpLock);

exit2:
    return (err);
}
#endif // TARGET_OS_WINDOWS

//===========================================================================================================================
//	DebugValidPtr
//===========================================================================================================================

#if (TARGET_OS_DARWIN_KERNEL)
extern vm_map_t kernel_map;
#define mach_task_self() kernel_map
#endif

int DebugValidPtr(uintptr_t inPtr, size_t inSize, int inRead, int inWrite, int inExecute)
{
#if (TARGET_MACH)
    kern_return_t err;
    vm_prot_t protectionMask;
    mach_vm_address_t regionPtr;
    mach_vm_address_t regionEnd;
    mach_vm_size_t regionLen;
    vm_region_basic_info_data_64_t info;
    vm_region_info_t infoPtr;
    mach_msg_type_number_t count;
    mach_port_t name;

    require_action_quiet((inPtr + inSize) >= inPtr, exit, err = kRangeErr);

    protectionMask = 0;
    if (inRead)
        protectionMask |= VM_PROT_READ;
    if (inWrite)
        protectionMask |= VM_PROT_WRITE;
    if (inExecute)
        protectionMask |= VM_PROT_EXECUTE;

    regionPtr = inPtr;
    regionLen = inSize;
    regionEnd = regionPtr + regionLen;
    infoPtr = (vm_region_info_t)&info;
    for (; regionPtr < regionEnd; regionPtr += regionLen) {
        count = VM_REGION_BASIC_INFO_COUNT_64;
        err = mach_vm_region(mach_task_self(), &regionPtr, &regionLen, VM_REGION_BASIC_INFO, infoPtr, &count, &name);
        require_noerr_quiet(err, exit);
        require_action_quiet((info.protection & protectionMask) == protectionMask, exit, err = kSecurityRequiredErr);
    }
    err = kNoErr;

exit:
    return (err == kNoErr);

#elif (TARGET_OS_WINDOWS)

    if ((inRead && IsBadReadPtr((const void*)inPtr, inSize)) || (inWrite && IsBadWritePtr((void*)inPtr, inSize)) || (inExecute && IsBadCodePtr((FARPROC)inPtr))) {
        return (0);
    }
    return (1);

#else
    (void)inPtr; // Unused
    (void)inSize; // Unused
    (void)inRead; // Unused
    (void)inWrite; // Unused
    (void)inExecute; // Unused

    return (1);
#endif
}

//===========================================================================================================================
//	DebugIsDebuggerPresent
//===========================================================================================================================

int DebugIsDebuggerPresent(void)
{
#if (TARGET_OS_DARWIN && !TARGET_OS_DARWIN_KERNEL)

    int mib[4];
    struct kinfo_proc info;
    size_t size;

    // Check if the process'es P_TRACED flag is set.

    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = getpid();
    size = sizeof(info);
    info.kp_proc.p_flag = 0;
    sysctl(mib, 4, &info, &size, NULL, 0);
    return ((info.kp_proc.p_flag & P_TRACED) == P_TRACED);

#elif (TARGET_OS_DARWIN_KERNEL)
#if (DEBUG)
    return (1);
#else
    return (0);
#endif
#elif (TARGET_OS_WINDOWS && !TARGET_OS_WINDOWS_CE)
    return (IsDebuggerPresent());
#elif (TARGET_OS_NETBSD && !TARGET_KERNEL)
    return 0; // return( sysctlbyname( "ddb.debug_now", NULL, NULL, NULL, 0 ) == 0 );
#elif (TARGET_OS_NETBSD && TARGET_KERNEL && DEBUG)
    return (1);
#else
    return (0);
#endif
}

//===========================================================================================================================
//	DebugEnterDebugger
//===========================================================================================================================

void DebugEnterDebugger(Boolean inForce)
{
#if (TARGET_OS_DARWIN && !TARGET_KERNEL)
    if (!inForce) {
        const char* var;

        var = getenv("USERBREAK");
        if (var && (*var == '1')) {
            inForce = true;
        }
    }
    if (inForce)
        __builtin_debugtrap();
#elif (TARGET_OS_DARWIN && TARGET_OS_DARWIN_KERNEL)
    (void)inForce;

    Debugger("DebugServices");
#elif (TARGET_OS_WINDOWS)
    (void)inForce;

    __debugbreak();
#elif (TARGET_OS_NETBSD && !TARGET_KERNEL)
    int debugNow;

    (void)inForce;

    debugNow = 1;
    // sysctlbyname( "ddb.debug_now", NULL, NULL, &debugNow, sizeof( debugNow ) );
    abort();
#elif (TARGET_OS_QNX)
    (void)inForce;

    DebugBreak();
#elif (TARGET_OS_NETBSD && TARGET_KERNEL)
    (void)inForce;

    Debugger();
#else
    (void)inForce;

// $$$ TO DO: Think of something better to do in the unknown target case.
#endif
}

#if (GCD_ENABLED)
//===========================================================================================================================
//	DebugIsCurrentDispatchQueue
//===========================================================================================================================

Boolean DebugIsCurrentDispatchQueue(dispatch_queue_t inQueue)
{
// dispatch_get_current_queue is deprecated so disable deprecation warnings around its usage.
// This function is only intended for debugging code, but if dispatch_get_current_queue is ever removed, it'll go away.

#if (COMPILER_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
    return (dispatch_get_current_queue() == inQueue);
#if (COMPILER_CLANG)
#pragma clang diagnostic pop
#endif
}
#endif

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	HTTPGetReasonPhrase
//===========================================================================================================================

#define CASE_REASON_PHRASE(NUM, STR) \
    case NUM:                        \
        reasonPhrase = STR;          \
        break

const char* HTTPGetReasonPhrase(int inStatusCode)
{
    const char* reasonPhrase;

    switch (inStatusCode) {
        // Information 1xx

        CASE_REASON_PHRASE(100, "Continue");
        CASE_REASON_PHRASE(101, "Switching Protocols");
        CASE_REASON_PHRASE(102, "Processing");
        CASE_REASON_PHRASE(103, "Checkpoint");

        // Successfull 2xx

        CASE_REASON_PHRASE(200, "OK");
        CASE_REASON_PHRASE(201, "Created");
        CASE_REASON_PHRASE(202, "Accepted");
        CASE_REASON_PHRASE(203, "Non-Authoritative Information");
        CASE_REASON_PHRASE(204, "No Content");
        CASE_REASON_PHRASE(205, "Reset Content");
        CASE_REASON_PHRASE(206, "Partial Content");
        CASE_REASON_PHRASE(207, "Multi-Status");
        CASE_REASON_PHRASE(208, "Already Reported");
        CASE_REASON_PHRASE(210, "Content Different");
        CASE_REASON_PHRASE(226, "IM Used");
        CASE_REASON_PHRASE(250, "Low on Storage Space");

        // Redirection 3xx

        CASE_REASON_PHRASE(300, "Multiple Choices");
        CASE_REASON_PHRASE(301, "Moved Permanently");
        CASE_REASON_PHRASE(302, "Found");
        CASE_REASON_PHRASE(303, "See Other");
        CASE_REASON_PHRASE(304, "Not Modified");
        CASE_REASON_PHRASE(305, "Use Proxy");
        CASE_REASON_PHRASE(306, "Switch Proxy"); // No longer used.
        CASE_REASON_PHRASE(307, "Temporary Redirect");
        CASE_REASON_PHRASE(308, "Permanent Redirect");
        CASE_REASON_PHRASE(330, "Moved Location");
        CASE_REASON_PHRASE(350, "Going Away");
        CASE_REASON_PHRASE(351, "Load Balancing");

        // Client Error 4xx

        CASE_REASON_PHRASE(400, "Bad Request");
        CASE_REASON_PHRASE(401, "Unauthorized");
        CASE_REASON_PHRASE(402, "Payment Required");
        CASE_REASON_PHRASE(403, "Forbidden");
        CASE_REASON_PHRASE(404, "Not Found");
        CASE_REASON_PHRASE(405, "Method Not Allowed");
        CASE_REASON_PHRASE(406, "Not Acceptable");
        CASE_REASON_PHRASE(407, "Proxy Authentication Required");
        CASE_REASON_PHRASE(408, "Request Timeout");
        CASE_REASON_PHRASE(409, "Conflict");
        CASE_REASON_PHRASE(410, "Gone");
        CASE_REASON_PHRASE(411, "Length Required");
        CASE_REASON_PHRASE(412, "Precondition Failed");
        CASE_REASON_PHRASE(413, "Request Entity Too Large");
        CASE_REASON_PHRASE(414, "Request URI Too Long");
        CASE_REASON_PHRASE(415, "Unsupported Media Type");
        CASE_REASON_PHRASE(416, "Requested Range Not Satisfiable");
        CASE_REASON_PHRASE(417, "Expectation Failed");
        CASE_REASON_PHRASE(418, "I'm a teapot");
        CASE_REASON_PHRASE(419, "Authentication Timeout");
        CASE_REASON_PHRASE(420, "Enhance Your Calm");
        CASE_REASON_PHRASE(421, "Not Authoritative");
        CASE_REASON_PHRASE(422, "Unprocessable Entity");
        CASE_REASON_PHRASE(423, "Expectation Failed");
        CASE_REASON_PHRASE(424, "Failed Dependency");
        CASE_REASON_PHRASE(425, "Unordered Collection");
        CASE_REASON_PHRASE(426, "Upgrade Required");
        CASE_REASON_PHRASE(428, "Precondition Required");
        CASE_REASON_PHRASE(429, "Too Many Requests");
        CASE_REASON_PHRASE(431, "Request Header Fields Too Large");
        CASE_REASON_PHRASE(440, "Login Timeout");
        CASE_REASON_PHRASE(444, "No Response");
        CASE_REASON_PHRASE(449, "Retry With");
        CASE_REASON_PHRASE(450, "Blocked by Parental Controls");
        CASE_REASON_PHRASE(451, "Parameter Not Understood");
        CASE_REASON_PHRASE(452, "Conference Not Found");
        CASE_REASON_PHRASE(453, "Not Enough Bandwidth");
        CASE_REASON_PHRASE(454, "Session Not Found");
        CASE_REASON_PHRASE(455, "Method Not Valid In This State");
        CASE_REASON_PHRASE(456, "Header Field Not Valid");
        CASE_REASON_PHRASE(457, "Invalid Range");
        CASE_REASON_PHRASE(458, "Parameter Is Read-Only");
        CASE_REASON_PHRASE(459, "Aggregate Operation Not Allowed");
        CASE_REASON_PHRASE(460, "Only Aggregate Operation Allowed");
        CASE_REASON_PHRASE(461, "Unsupported Transport");
        CASE_REASON_PHRASE(462, "Destination Unreachable");
        CASE_REASON_PHRASE(463, "Destination Prohibited");
        CASE_REASON_PHRASE(464, "Data Transport Not Ready Yet");
        CASE_REASON_PHRASE(465, "Notification Reason Unknown");
        CASE_REASON_PHRASE(466, "Key Management Error");
        CASE_REASON_PHRASE(470, "Connection Authorization Required");
        CASE_REASON_PHRASE(471, "Connection Credentials not accepted");
        CASE_REASON_PHRASE(472, "Failure to establish secure connection");
        CASE_REASON_PHRASE(475, "Invalid collblob");
        CASE_REASON_PHRASE(499, "Client Closed Request");

        // Server Error 5xx

        CASE_REASON_PHRASE(500, "Internal Server Error");
        CASE_REASON_PHRASE(501, "Not Implemented");
        CASE_REASON_PHRASE(502, "Bad Gateway");
        CASE_REASON_PHRASE(503, "Service Unavailable");
        CASE_REASON_PHRASE(504, "Gateway Timeout");
        CASE_REASON_PHRASE(505, "Version Not Supported");
        CASE_REASON_PHRASE(506, "Variant Also Negotiates");
        CASE_REASON_PHRASE(507, "Insufficient Storage");
        CASE_REASON_PHRASE(508, "Loop Detected");
        CASE_REASON_PHRASE(509, "Bandwidth Limit Exceeded");
        CASE_REASON_PHRASE(510, "Not Extended");
        CASE_REASON_PHRASE(511, "Network Authentication Required");
        CASE_REASON_PHRASE(520, "Origin Error");
        CASE_REASON_PHRASE(522, "Connection timed out");
        CASE_REASON_PHRASE(523, "Proxy Declined Request");
        CASE_REASON_PHRASE(524, "Timeout occurred");
        CASE_REASON_PHRASE(551, "Option Not Supported");
        CASE_REASON_PHRASE(553, "Proxy Unavailable");
        CASE_REASON_PHRASE(598, "Network Read Timeout");
        CASE_REASON_PHRASE(599, "Network Connect Timeout");

    default:
        reasonPhrase = "";
        break;
    }
    return (reasonPhrase);
}

//===========================================================================================================================
//	ReportCriticalError
//===========================================================================================================================

void ReportCriticalError(const char* inReason, uint32_t inExceptionCode, Boolean inCrashLog)
{
    (void)inReason;
    (void)inExceptionCode;
    (void)inCrashLog;

    // TO DO: Figure out what to do on other platforms.
}

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if (!DEBUG_SERVICES_LITE && !EXCLUDE_UNIT_TESTS)
//===========================================================================================================================
//	DebugServicesTest
//===========================================================================================================================

OSStatus DebugServicesTest(void)
{
    OSStatus err;
    LogLevel oldDebugBreakLevel;
#if (TARGET_MACH || TARGET_OS_WINDOWS)
    uint8_t data[] = {
        0x11, 0x22, 0x33, 0x44,
        0x55, 0x66,
        0x77, 0x88, 0x99, 0xAA,
        0xBB, 0xCC, 0xDD,
        0xEE,
        0xFF,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
        0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0,
        0x11, 0x21, 0x31, 0x41, 0x51, 0x61, 0x71, 0x81, 0x91, 0xA1
    };
#endif

    oldDebugBreakLevel = log_category_from_name(DebugServicesBreak).level;
    log_category_from_name(DebugServicesBreak).level = kLogLevelOff;

    // check's

    check(0 && "SHOULD SEE: check");
    check(1 && "SHOULD *NOT* SEE: check (valid)");
    check_string(0, "SHOULD SEE: check_string");
    check_string(1, "SHOULD *NOT* SEE: check_string (valid)");
    check_noerr(-123);
    check_noerr(10038);
    check_noerr(22);
    check_noerr(0);
    check_noerr_string(-6712, "SHOULD SEE: check_noerr_string");
    check_noerr_string(0, "SHOULD *NOT* SEE: check_noerr_string (valid)");
    check_ptr_overlap(("SHOULD *NOT* SEE" != NULL) ? 10 : 0, 10, 22, 10);
    check_ptr_overlap(("SHOULD SEE" != NULL) ? 10 : 0, 10, 5, 10);
    check_ptr_overlap(("SHOULD SEE" != NULL) ? 10 : 0, 10, 12, 6);
    check_ptr_overlap(("SHOULD SEE" != NULL) ? 12 : 0, 6, 10, 10);
    check_ptr_overlap(("SHOULD SEE" != NULL) ? 12 : 0, 10, 10, 10);
    check_ptr_overlap(("SHOULD *NOT* SEE" != NULL) ? 22 : 0, 10, 10, 10);
    check_ptr_overlap(("SHOULD *NOT* SEE" != NULL) ? 10 : 0, 10, 20, 10);
    check_ptr_overlap(("SHOULD *NOT* SEE" != NULL) ? 20 : 0, 10, 10, 10);

    // require's

    require(0 && "SHOULD SEE", require1);
    {
        err = kResponseErr;
        goto exit;
    }
require1:
    require(1 && "SHOULD *NOT* SEE", require2);
    goto require2Good;
require2 : {
    err = kResponseErr;
    goto exit;
}
require2Good:
    require_string(0 && "SHOULD SEE", require3, "SHOULD SEE: require_string");
    {
        err = kResponseErr;
        goto exit;
    }
require3:
    require_string(1 && "SHOULD *NOT* SEE", require4, "SHOULD *NOT* SEE: require_string (valid)");
    goto require4Good;
require4 : {
    err = kResponseErr;
    goto exit;
}
require4Good:
    require_quiet(0 && "SHOULD SEE", require5);
    {
        err = kResponseErr;
        goto exit;
    }
require5:
    require_quiet(1 && "SHOULD *NOT* SEE", require6);
    goto require6Good;
require6 : {
    err = kResponseErr;
    goto exit;
}
require6Good:
    require_noerr(-1, require7);
    {
        err = kResponseErr;
        goto exit;
    }
require7:
    require_noerr(0, require8);
    goto require8Good;
require8 : {
    err = kResponseErr;
    goto exit;
}
require8Good:
    require_noerr_string(-2, require9, "SHOULD SEE: require_noerr_string");
    {
        err = kResponseErr;
        goto exit;
    }
require9:
    require_noerr_string(0, require10, "SHOULD *NOT* SEE: require_noerr_string (valid)");
    goto require10Good;
require10 : {
    err = kResponseErr;
    goto exit;
}
require10Good:
    require_noerr_action_string(-3, require11, dbs_ulog(kLogLevelMax, "SHOULD SEE: action 1 (expected)\n"), "require_noerr_action_string");
    {
        err = kResponseErr;
        goto exit;
    }
require11:
    require_noerr_action_string(0, require12, dbs_ulog(kLogLevelMax, "SHOULD *NOT* SEE: action 2\n"), "require_noerr_action_string (valid)");
    goto require12Good;
require12 : {
    err = kResponseErr;
    goto exit;
}
require12Good:
    require_noerr_quiet(-4, require13);
    {
        err = kResponseErr;
        goto exit;
    }
require13:
    require_noerr_quiet(0, require14);
    goto require14Good;
require14 : {
    err = kResponseErr;
    goto exit;
}
require14Good:
    require_noerr_action(-5, require15, dbs_ulog(kLogLevelMax, "SHOULD SEE: action 3 (expected)\n"));
    {
        err = kResponseErr;
        goto exit;
    }
require15:
    require_noerr_action(0, require16, dbs_ulog(kLogLevelMax, "SHOULD *NOT* SEE: action 4\n"));
    goto require16Good;
require16 : {
    err = kResponseErr;
    goto exit;
}
require16Good:
    require_noerr_action_quiet(-4, require17, dbs_ulog(kLogLevelMax, "SHOULD SEE: action 5 (expected)\n"));
    {
        err = kResponseErr;
        goto exit;
    }
require17:
    require_noerr_action_quiet(0, require18, dbs_ulog(kLogLevelMax, "SHOULD *NOT* SEE: action 6\n"));
    goto require18Good;
require18 : {
    err = kResponseErr;
    goto exit;
}
require18Good:
    require_action(0 && "SHOULD SEE", require19, dbs_ulog(kLogLevelMax, "SHOULD SEE: action 7 (expected)\n"));
    {
        err = kResponseErr;
        goto exit;
    }
require19:
    require_action(1 && "SHOULD *NOT* SEE", require20, dbs_ulog(kLogLevelMax, "SHOULD *NOT* SEE: action 8\n"));
    goto require20Good;
require20 : {
    err = kResponseErr;
    goto exit;
}
require20Good:
    require_action_quiet(0, require21, dbs_ulog(kLogLevelMax, "SHOULD SEE: action 9 (expected)\n"));
    {
        err = kResponseErr;
        goto exit;
    }
require21:
    require_action_quiet(1, require22, dbs_ulog(kLogLevelMax, "SHOULD *NOT* SEE: action 10\n"));
    goto require22Good;
require22 : {
    err = kResponseErr;
    goto exit;
}
require22Good:
    require_action_string(0, require23, dbs_ulog(kLogLevelMax, "SHOULD SEE: action 11 (expected)\n"), "SHOULD SEE: require_action_string");
    {
        err = kResponseErr;
        goto exit;
    }
require23:
    require_action_string(1, require24, dbs_ulog(kLogLevelMax, "SHOULD *NOT* SEE: action 12\n"), "SHOULD *NOT* SEE: require_action_string");
    goto require24Good;
require24 : {
    err = kResponseErr;
    goto exit;
}
require24Good:

    // debug_string

    debug_string("debug_string");

    // dlog's

    dlog(kLogLevelNotice, "dlog\n");
    dlog(kLogLevelNotice, "dlog integer: %d\n", 123);
    dlog(kLogLevelNotice, "dlog string:  \"%s\"\n", "test string");

// DebugValidPtr

#if (TARGET_MACH || TARGET_OS_WINDOWS)
    require_action(!DebugValidPtr(0x00000000, 1, 1, 0, 0), exit, err = kResponseErr);
    require_action(!DebugValidPtr(0x00000000, 1, 0, 1, 0), exit, err = kResponseErr);
    require_action(!DebugValidPtr(0x00000000, 1, 0, 0, 1), exit, err = kResponseErr);
    require_action(DebugValidPtr((uintptr_t)data, 1, 1, 1, 0), exit, err = kResponseErr);
    require_action(DebugValidPtr((uintptr_t)data, sizeof(data), 1, 1, 0), exit, err = kResponseErr);
    require_action(!DebugValidPtr(0xFFFFFFF0U, 0xFFFF, 1, 1, 0), exit, err = kResponseErr);

    require_action(!debug_valid_ptr_r(0x00000000, 1), exit, err = kResponseErr);
    require_action(!debug_valid_ptr_w(0x00000000, 1), exit, err = kResponseErr);
    require_action(!debug_valid_ptr_e(0x00000000, 1), exit, err = kResponseErr);
    require_action(debug_valid_ptr_rw(data, 1), exit, err = kResponseErr);
    require_action(debug_valid_ptr_rw(data, sizeof(data)), exit, err = kResponseErr);
    require_action(!debug_valid_ptr_rw(0xFFFFFFF0U, 0xFFFF), exit, err = kResponseErr);
#endif

    err = kNoErr;

exit:
    log_category_from_name(DebugServicesBreak).level = oldDebugBreakLevel;
    printf("DebugServicesTest: %s\n", !err ? "PASSED" : "FAILED");
    return (err);
}
#endif // !DEBUG_SERVICES_LITE && !EXCLUDE_UNIT_TESTS
