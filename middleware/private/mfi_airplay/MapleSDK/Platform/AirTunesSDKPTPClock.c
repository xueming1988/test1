/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
 */

#include "AirTunesClock.h"
#include "AirTunesClockPriv.h"
#include "AirTunesPTPClock.h"
#include "CFUtils.h"
#include "Common/ptpLog.h"
#include "DebugServices.h"
#include "TickUtils.h"
#include "ptpInterface.h"
#include <arpa/inet.h>
#include <fcntl.h> //	For O_* constants
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h> //	For getnameinfo
#include <pthread.h>
#include <string.h> //	For strcpy and strcat
#include <sys/mman.h>
#include <sys/stat.h> //	For mode constants
#include <sys/types.h>
#include <unistd.h>

#include <sys/ioctl.h>

// Debugging
ulog_define(AirTunesSDKPTPClock, kLogLevelNotice, kLogFlags_Default, "SDKPTPClock", NULL);
#define atc_ucat() &log_category_from_name(AirTunesSDKPTPClock)
#define atc_ulog(LEVEL, ...) ulog(atc_ucat(), (LEVEL), __VA_ARGS__)

#define kDeviceSessionCount CFSTR("sessions")
#define kDevicePeers CFSTR("peers")
#define kDevicePTPRunning CFSTR("ptp running")

typedef struct {
    pthread_mutex_t mutex; //    protect access to this data structure.
    int retainCount;
    CFMutableDictionaryRef devicesDictionary;
} AirTunesPTPClockSharedData;

typedef struct
{
    struct AirTunesClockPrivate header; // An AirTunesClock always starts with this
    AirTunesPTPClockSettings ptpSettings;
} AirTunesPTPClock;

DEBUG_STATIC OSStatus AirTunesPTPClock_Finalize(AirTunesClockRef inClock);
DEBUG_STATIC Boolean AirTunesPTPClock_Adjust(AirTunesClockRef inClock, int64_t inOffsetNanoseconds, Boolean inReset);
DEBUG_STATIC void AirTunesPTPClock_GetNetworkTime(AirTunesClockRef inClock, AirTunesTime* outTime);
DEBUG_STATIC void AirTunesPTPClock_UpTicksToNetworkTime(AirTunesClockRef inClock, AirTunesTime* outTime, uint64_t inTicks);
DEBUG_STATIC OSStatus AirTunesPTPClock_ConvertNetworkTimeToUpTicks(AirTunesClockRef inClock, AirTunesTime inNetworkTime, uint64_t* outUpTicks);
DEBUG_STATIC OSStatus AirTunesPTPClock_AddPeer(AirTunesClockRef inClock, CFStringRef inInterfaceName, const void* inSockAddr);
DEBUG_STATIC OSStatus AirTunesPTPClock_RemovePeer(AirTunesClockRef ptp, CFStringRef inInterfaceName, const void* inSockAddr);

#if 0
#pragma mark -
#endif
// detect currently active network interface
static void AirTunesPTPClock_GetSharedDataPtrOnce(void* inCtx)
{
    AirTunesPTPClockSharedData** ptp = (AirTunesPTPClockSharedData**)inCtx;

    AirTunesPTPClockSharedData* ptr = calloc(1, sizeof(AirTunesPTPClockSharedData));

    pthread_mutex_init(&ptr->mutex, NULL);
    ptr->devicesDictionary = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    ptpLogInit();

    *ptp = ptr;
}

static AirTunesPTPClockSharedData* AirTunesPTPClock_GetSharedDataPtr(void)
{
    static dispatch_once_t serviceInitOnce = 0;
    static AirTunesPTPClockSharedData* sharedDataPtr = NULL;

    dispatch_once_f(&serviceInitOnce, &sharedDataPtr, AirTunesPTPClock_GetSharedDataPtrOnce);

    return sharedDataPtr;
}

static void removePeers(const void* key, const void* value, void* context)
{
    (void)value;

    char name[NI_MAXHOST] = { 0 };
    if (CFStringGetCString((CFStringRef)key, name, sizeof(name), kCFStringEncodingUTF8)) {
        ptpRemovePeer(CFGetInt64(context, NULL), name);
        atc_ulog(kLogLevelNotice, "Device at ip address %s was removed from the group [%@]\n", name, (CFNumberRef)context);
    }
}

static void AirTunesPTPClock_Retain(AirTunesPTPClock* ptp)
{
    AirTunesPTPClockSharedData* data = AirTunesPTPClock_GetSharedDataPtr();
    CFNumberRef n = NULL;
    CFMutableDictionaryRef deviceDict = NULL;
    CFMutableDictionaryRef newDevice = NULL;
    CFMutableDictionaryRef newPeers = NULL;

    pthread_mutex_lock(&data->mutex);
    data->retainCount++;

    if (data->retainCount == 1) {
        ptpLogConfigure(PTP_LOG_LVL_INFO);
    }

    n = CFNumberCreateInt64(ptp->ptpSettings.clientDeviceID);
    require(n, skip);

    deviceDict = (CFMutableDictionaryRef)CFDictionaryGetValue(data->devicesDictionary, n);
    if (deviceDict == NULL) {
        newDevice = CFDictionaryCreateMutable(NULL, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        newPeers = CFDictionaryCreateMutable(NULL, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

        CFDictionarySetInt64(newDevice, kDeviceSessionCount, 1);
        CFDictionarySetValue(newDevice, kDevicePeers, newPeers);
        CFDictionarySetBoolean(newDevice, kDevicePTPRunning, false);

        CFDictionarySetValue(data->devicesDictionary, n, newDevice);
    } else {
        CFDictionarySetInt64(deviceDict, kDeviceSessionCount, CFDictionaryGetInt64(deviceDict, kDeviceSessionCount, NULL) + 1);
    }
skip:
    pthread_mutex_unlock(&data->mutex);

    CFReleaseNullSafe(n);
    CFReleaseNullSafe(newDevice);
    CFReleaseNullSafe(newPeers);
}

static void AirTunesPTPClock_Release(AirTunesPTPClock* ptp)
{
    AirTunesPTPClockSharedData* data = AirTunesPTPClock_GetSharedDataPtr();
    CFNumberRef n = NULL;

    pthread_mutex_lock(&data->mutex);

    n = CFNumberCreateInt64(ptp->ptpSettings.clientDeviceID);
    require(n, skip);

    CFMutableDictionaryRef deviceDict = (CFMutableDictionaryRef)CFDictionaryGetValue(data->devicesDictionary, n);
    require(deviceDict, skip);

    int64_t sessions = CFDictionaryGetInt64(deviceDict, kDeviceSessionCount, NULL) - 1;

    if (sessions == 0) {
        CFMutableDictionaryRef peers = (CFMutableDictionaryRef)CFDictionaryGetValue(deviceDict, kDevicePeers);
        require(deviceDict, skip);

        CFDictionaryApplyFunction(peers, removePeers, (void*)n);

        if (CFDictionaryGetBoolean(deviceDict, kDevicePTPRunning, NULL))
            ptpStop(ptp->ptpSettings.clientDeviceID);

        CFDictionaryRemoveValue(data->devicesDictionary, n);
    } else if (sessions > 0) {
        CFDictionarySetInt64(deviceDict, kDeviceSessionCount, sessions);
    }
skip:
    data->retainCount--;
    if (data->retainCount == 0) {
        CFDictionaryRemoveAllValues(data->devicesDictionary);
    }
    pthread_mutex_unlock(&data->mutex);

    CFReleaseNullSafe(n);
}

static char* AirTunesPTPClock_CopyActiveNetworkInterface(void)
{
    struct ifaddrs *ifaddr, *ifa;

    // Get a linked list of network interfaces
    if (getifaddrs(&ifaddr) == -1) {
        atc_ulog(kLogLevelError, "*%s(): error getting linked list of interfaces", __FUNCTION__);
        return NULL;
    }

    // Walk through the linked list and find the active one
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {

        // No loopback interfaces or not running
        if (ifa->ifa_addr != NULL && !(ifa->ifa_flags & IFF_LOOPBACK) && (ifa->ifa_flags & IFF_RUNNING)) {

            // check if this interface has a valid ip address
            int inSock = socket(AF_INET, SOCK_DGRAM, 0);

            struct ifreq ifr;
            memset(&ifr, 0, sizeof(ifr));
            strncpy(ifr.ifr_name, ifa->ifa_name, sizeof(ifr.ifr_name));

            // read interface address
            int err = ioctl(inSock, SIOCGIFADDR, &ifr);
            close_compat(inSock);

            // if we could not retrieve address, skip this interface
            if (err)
                continue;

            // clone interface name and return it
            char* ifName = strdup(ifa->ifa_name);
            return ifName;
        }
    }

    return NULL;
}

static OSStatus AirTunesPTPClock_Start(AirTunesPTPClock* ptp, const char* inInterfaceName)
{
    OSStatus err = kParamErr;
    CFNumberRef n = NULL;
    CFMutableDictionaryRef deviceDict = NULL;

    AirTunesPTPClockSharedData* data = AirTunesPTPClock_GetSharedDataPtr();

    pthread_mutex_lock(&data->mutex);

    n = CFNumberCreateInt64(ptp->ptpSettings.clientDeviceID);
    require(n, skip);

    deviceDict = (CFMutableDictionaryRef)CFDictionaryGetValue(data->devicesDictionary, n);
    require(deviceDict, skip);

    if (!CFDictionaryGetBoolean(deviceDict, kDevicePTPRunning, NULL)) {
        char* interfaceName = (char*)inInterfaceName;

        // find out the name of active network interface
        if (interfaceName == NULL)
            interfaceName = AirTunesPTPClock_CopyActiveNetworkInterface();

        if (interfaceName) {
            err = ptpStart(interfaceName, ptp->ptpSettings.clientDeviceID, ptp->ptpSettings.priority1, ptp->ptpSettings.priority2) ? kNoErr : kParamErr;
            if (err == kNoErr) {
                CFDictionarySetBoolean(deviceDict, kDevicePTPRunning, true);

                PtpTimeData ptpTimeData;
                ptpRefresh(ptp->ptpSettings.clientDeviceID, &ptpTimeData);
            }

            if (interfaceName != inInterfaceName)
                free(interfaceName);
        }
    }

    err = (CFDictionaryGetBoolean(deviceDict, kDevicePTPRunning, NULL) ? kNoErr : err);
skip:

    pthread_mutex_unlock(&data->mutex);

    CFReleaseNullSafe(n);

    return err;
}

static void AirTunesPTPClock_GetTimeData(const AirTunesPTPClock* ptp, PtpTimeData* ptpTimeData)
{
    CFNumberRef n = NULL;
    CFMutableDictionaryRef deviceDict = NULL;

    AirTunesPTPClockSharedData* data = AirTunesPTPClock_GetSharedDataPtr();

    pthread_mutex_lock(&data->mutex);

    n = CFNumberCreateInt64(ptp->ptpSettings.clientDeviceID);
    require(n, skip);

    deviceDict = (CFMutableDictionaryRef)CFDictionaryGetValue(data->devicesDictionary, n);
    require(deviceDict, skip);

    if (CFDictionaryGetBoolean(deviceDict, kDevicePTPRunning, NULL)) {
        ptpRefresh(ptp->ptpSettings.clientDeviceID, ptpTimeData);
    } else {
        atc_ulog(kLogLevelError, "ptp->ptpSettings.ptpRunning == 0", __FUNCTION__);
        memset(ptpTimeData, 0, sizeof(PtpTimeData));
    }
skip:
    pthread_mutex_unlock(&data->mutex);

    CFReleaseNullSafe(n);
}

static OSStatus AirTunesPTPClock_AddressToName(const struct sockaddr* inSockAddr, char name[NI_MAXHOST])
{
    OSStatus err = kNoErr;
    socklen_t sockLen = 0;

    if (inSockAddr->sa_family == AF_INET)
        sockLen = sizeof(struct sockaddr_in);
#ifdef AF_INET6
    else if (inSockAddr->sa_family == AF_INET6)
        sockLen = sizeof(struct sockaddr_in6);
#endif //AF_INET6

    require_action(sockLen, exit, err = kParamErr);
    err = getnameinfo(inSockAddr, sockLen, name, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
    require_noerr_quiet(err, exit);

exit:
    return err;
}

//===========================================================================================================================
//	AirTunesPTPClock_Create
//===========================================================================================================================

OSStatus AirTunesPTPClock_Create(AirTunesPTPClockSettings* ptpSettings, AirTunesClockRef* outRef)
{
    OSStatus err = kNoErr;
    AirTunesPTPClock* ptp = (AirTunesPTPClock*)calloc(1, sizeof(AirTunesPTPClock));
    require_action(ptp, exit, err = kNoMemoryErr);

    ptp->header.class = kAirTunesClockPTP;
    ptp->header.vtable.finalize = AirTunesPTPClock_Finalize;
    ptp->header.vtable.adjust = AirTunesPTPClock_Adjust;
    ptp->header.vtable.getNetworkTime = AirTunesPTPClock_GetNetworkTime;
    ptp->header.vtable.upTicksToNetworkTime = AirTunesPTPClock_UpTicksToNetworkTime;
    ptp->header.vtable.netTimeToUpTicks = AirTunesPTPClock_ConvertNetworkTimeToUpTicks;
    ptp->header.vtable.addPeer = AirTunesPTPClock_AddPeer;
    ptp->header.vtable.removePeer = AirTunesPTPClock_RemovePeer;
    ptp->ptpSettings = *ptpSettings;

    AirTunesPTPClock_Retain(ptp);

    *outRef = &ptp->header;
exit:
    // Clean up on error
    if (err && ptp) {
        AirTunesClock_Finalize(&ptp->header);
        *outRef = NULL;
    }

    return (err);
}

//===========================================================================================================================
//	AirTunesPTPClock_Finalize
//===========================================================================================================================

DEBUG_STATIC OSStatus AirTunesPTPClock_Finalize(AirTunesClockRef inClock)
{
    OSStatus err = kNoErr;

    atc_ulog(kLogLevelTrace, "AirTunesPTPClock_Finalize\n");

    if (inClock) {
        AirTunesPTPClock* ptp = (AirTunesPTPClock*)inClock;

        AirTunesPTPClock_Release(ptp);

        free(inClock);
        inClock = NULL;
    }
    return (err);
}

//===========================================================================================================================
//	AirTunesPTPClock_Adjust
//===========================================================================================================================

DEBUG_STATIC Boolean AirTunesPTPClock_Adjust(AirTunesClockRef inClock, int64_t inOffsetNanoseconds, Boolean inReset)
{
    // Can't adjust PTP clock.
    (void)inClock;
    (void)inOffsetNanoseconds;
    return (inReset);
}

//===========================================================================================================================
//	PTPClockGetTimeInNano
//===========================================================================================================================

static OSStatus PTPClockGetTimeInNano(const AirTunesPTPClock* ptp, uint64_t* remoteTimeNs, uint64_t* systemTicks, uint64_t* clockID)
{
    (void)ptp;

    // Get the PTP data in the shared memory.
    // We do it before the system time is measured as gptpRefresh() call may
    // take longer time due to mutex locking
    PtpTimeData ptpTimeData;
    AirTunesPTPClock_GetTimeData(ptp, &ptpTimeData);

    // get current ticks
    if (systemTicks) {
        *systemTicks = UpTicks();
    }

    // get clock ID
    *clockID = ptpTimeData._masterClockId;

    // local clock time adjusted by PTP
    struct timespec ts;
    clock_gettime(CLOCK_PTP_GET_TIME, &ts);
    uint64_t clockNs = (uint64_t)ts.tv_sec * 1e9 + ts.tv_nsec;
    *remoteTimeNs = clockNs - ptpTimeData._masterToLocalPhase;

    return kNoErr;
}

//===========================================================================================================================
//	AirTunesPTPClock_GetNetworkTime
//===========================================================================================================================

DEBUG_STATIC void AirTunesPTPClock_GetNetworkTime(AirTunesClockRef inClock, AirTunesTime* outTime)
{
    AirTunesPTPClock* ptp = (AirTunesPTPClock*)inClock;

    uint64_t currentNetworkTimeInNs = 0;
    uint64_t systemTicks = 0;

    if (PTPClockGetTimeInNano(ptp, &currentNetworkTimeInNs, &systemTicks, &outTime->timelineID) == kNoErr) {
        AirTunesTime_FromNanoseconds(outTime, currentNetworkTimeInNs);
    }

    //	If we failed to get the PTP data, just leave the "outTime" content as is
}

//===========================================================================================================================
//	AirTunesPTPClock_GetSynchronizedTimeNearUpTicks
//===========================================================================================================================

DEBUG_STATIC void AirTunesPTPClock_UpTicksToNetworkTime(AirTunesClockRef inClock, AirTunesTime* outTime, uint64_t inTicks)
{
    AirTunesPTPClock* ptp = (AirTunesPTPClock*)inClock;
    uint64_t nowNetNanos = 0;
    uint64_t nowTicks = 0;
    OSStatus err;

    if (((err = PTPClockGetTimeInNano(ptp, &nowNetNanos, &nowTicks, &outTime->timelineID)) != 0) || (outTime->timelineID == 0))
        AirTunesTime_Invalidate(outTime, err == kExecutionStateErr ? AirTunesTimeFlag_StateErr : AirTunesTimeFlag_Invalid);
    else {
        if (inTicks >= nowTicks) {
            nowNetNanos = nowNetNanos + UpTicksToNanoseconds(inTicks - nowTicks);
        } else {
            nowNetNanos = nowNetNanos - UpTicksToNanoseconds(nowTicks - inTicks);
        }
        AirTunesTime_FromNanoseconds(outTime, nowNetNanos);
    }
}

//===========================================================================================================================
//    AirTunesPTPClock_ConvertNetworkTimeToUpTicks
//===========================================================================================================================
OSStatus AirTunesPTPClock_ConvertNetworkTimeToUpTicks(AirTunesClockRef inClock, AirTunesTime inNetworkTime, uint64_t* outTicks)
{
    AirTunesPTPClock* ptp = (AirTunesPTPClock*)inClock;

    uint64_t nowNano = 0;
    uint64_t nowTicks = 0;
    uint64_t clockID = 0;

    OSStatus err = PTPClockGetTimeInNano(ptp, &nowNano, &nowTicks, &clockID);

    if (!err && (clockID != inNetworkTime.timelineID)) {
        atc_ulog(kLogLevelTrace, "timeline id doesn't match! (received: %#llx, expected: %#llx)\n", clockID, inNetworkTime.timelineID);
        err = kAirTunesClockError_TimelineIDMismatch;
    }

    if (!err) {
        uint64_t inNano = AirTunesTime_ToNanoseconds(&inNetworkTime);
        if (inNano >= nowNano) {
            *outTicks = nowTicks + NanosecondsToUpTicks(inNano - nowNano);
        } else {
            *outTicks = nowTicks - NanosecondsToUpTicks(nowNano - inNano);
        }
    }

    return err;
}
//===========================================================================================================================
//    AirTunesPTPClock_RemovePeer
//===========================================================================================================================
OSStatus AirTunesPTPClock_RemovePeer(AirTunesClockRef inClock, CFStringRef inInterfaceName, const void* inSockAddr)
{
    (void)inInterfaceName;

    OSStatus err = kNoErr;
    AirTunesPTPClock* ptp = (AirTunesPTPClock*)inClock;
    CFNumberRef clientDeviceID = NULL;
    CFMutableDictionaryRef deviceDict = NULL;

    AirTunesPTPClockSharedData* data = AirTunesPTPClock_GetSharedDataPtr();

    pthread_mutex_lock(&data->mutex);

    clientDeviceID = CFNumberCreateInt64(ptp->ptpSettings.clientDeviceID);
    require(clientDeviceID, exit);

    deviceDict = (CFMutableDictionaryRef)CFDictionaryGetValue(data->devicesDictionary, clientDeviceID);
    require(deviceDict, exit);

    if (CFDictionaryGetBoolean(deviceDict, kDevicePTPRunning, NULL)) {
        CFStringRef address = NULL;
        char name[NI_MAXHOST] = { 0 };

        err = AirTunesPTPClock_AddressToName(inSockAddr, name);
        require_noerr_quiet(err, skip);

        address = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
        require_quiet(address, skip);

        CFMutableDictionaryRef peers = (CFMutableDictionaryRef)CFDictionaryGetValue(deviceDict, kDevicePeers);
        require(peers, skip);

        int64_t count = CFDictionaryGetInt64(peers, address, NULL) - 1;
        if (count <= 0) {
            CFDictionaryRemoveValue(peers, address);
            ptpRemovePeer(ptp->ptpSettings.clientDeviceID, name);
            atc_ulog(kLogLevelNotice, "Device at ip address %s was removed from the group [%@]\n", name, clientDeviceID);
        } else {
            CFDictionarySetInt64(peers, address, count);
        }
    skip:
        CFReleaseNullSafe(address);
    }

exit:
    pthread_mutex_unlock(&data->mutex);

    CFReleaseNullSafe(clientDeviceID);

    return err;
}

//===========================================================================================================================
//	AirTunesPTPClock_AddPeer
//===========================================================================================================================
DEBUG_STATIC OSStatus AirTunesPTPClock_AddPeer(AirTunesClockRef inClock, CFStringRef inInterfaceName, const void* inSockAddr)
{
    char* interfaceName = NULL;
    OSStatus err = kNoErr;
    CFNumberRef clientDeviceID = NULL;
    CFMutableDictionaryRef deviceDict = NULL;
    AirTunesPTPClock* ptp = (AirTunesPTPClock*)inClock;

    err = CFStringCopyUTF8CString(inInterfaceName, &interfaceName);
    require_noerr(err, exit);

    err = AirTunesPTPClock_Start(ptp, interfaceName);
    require_noerr(err, exit);

    AirTunesPTPClockSharedData* data = AirTunesPTPClock_GetSharedDataPtr();
    pthread_mutex_lock(&data->mutex);

    clientDeviceID = CFNumberCreateInt64(ptp->ptpSettings.clientDeviceID);
    require(clientDeviceID, exit);

    deviceDict = (CFMutableDictionaryRef)CFDictionaryGetValue(data->devicesDictionary, clientDeviceID);
    require(deviceDict, exit);

    if (CFDictionaryGetBoolean(deviceDict, kDevicePTPRunning, NULL)) {
        char name[NI_MAXHOST] = { 0 };
        CFStringRef address = NULL;

        err = AirTunesPTPClock_AddressToName(inSockAddr, name);
        require_noerr_quiet(err, skip);

        address = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
        require_action_quiet(address, skip, err = kNoMemoryErr);

        CFMutableDictionaryRef peers = (CFMutableDictionaryRef)CFDictionaryGetValue(deviceDict, kDevicePeers);
        require(peers, skip);

        CFDictionarySetInt64(peers, address, CFDictionaryGetInt64(peers, address, NULL) + 1);
        if (CFDictionaryGetInt64(peers, address, NULL) == 1) {
            ptpAddPeer(ptp->ptpSettings.clientDeviceID, name);
            atc_ulog(kLogLevelNotice, "Device at ip address %s [if:%@] was added to the group [%@]\n", name, inInterfaceName, clientDeviceID);
        }
    skip:
        CFReleaseNullSafe(address);
    }
    pthread_mutex_unlock(&data->mutex);
exit:
    CFReleaseNullSafe(clientDeviceID);
    ForgetMem(&interfaceName);

    return err;
}
