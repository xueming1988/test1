/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpInterface.h"

#include "AtomicUtils.h"
#include "Common/ptpList.h"
#include "Common/ptpMutex.h"
#include "Common/ptpPlatform.h"
#include "Common/ptpThreadSafeCallback.h"
#include "ptpClockContext.h"
#include "ptpNetworkInterface.h"
#include "ptpNetworkPort.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PHY_DELAY_GB_TX_I20 184 // 1G delay
#define PHY_DELAY_GB_RX_I20 382 // 1G delay
#define PHY_DELAY_MB_TX_I20 1044 // 100M delay
#define PHY_DELAY_MB_RX_I20 2133 // 100M delay

typedef struct {

    PtpMutexRef _mutex;
    ClockCtxRef _pClock;
    NetworkPortRef _port;
    char* _ifname;
    PtpTimeData _data;
    uint8_t _priority1;
    uint8_t _priority2;
    ListPeripheralDelayRef _etherPhyDelay;
    uint64_t _instanceId;

} PTPInterface;

typedef PTPInterface* PTPInterfaceRef;

DECLARE_LIST(PTPInterface);

static void ptpListPTPInterfaceObjectDtor(PTPInterfaceRef itf)
{
    // no deallocation here
    (void)itf;
}

DEFINE_LIST(PTPInterface, ptpListPTPInterfaceObjectDtor);

/**
 * @brief static reference to the list of active interfaces
 */
static ListPTPInterfaceRef g_interfaces = NULL;

/**
 * @brief PTPInterface constructor
 * @param itf reference to PTPInterface object
 */
void ptpInterfaceCtor(PTPInterfaceRef itf, uint64_t instanceId);
Boolean ptpInterfaceStart(PTPInterfaceRef itf);
void ptpInterfaceStop(PTPInterfaceRef itf);
Boolean ptpInterfaceInitialize(PTPInterfaceRef itf, const char* interfaceName, uint8_t priority1, uint8_t priority2);
void ptpInterfaceDeinitialize(PTPInterfaceRef itf);
PtpMutexRef ptpInterfacesMutexInstance(void);
Boolean ptpInterfacesReleaseInstance(uint64_t instanceId);
PTPInterfaceRef ptpInterfacesAcquireInstance(uint64_t instanceId);
void ptpInterfaceDtor(PTPInterfaceRef itf);
void ptpInterfaceRefresh(PTPInterfaceRef itf, PtpTimeDataRef data);
void ptpInterfaceAddPeer(PTPInterfaceRef itf, const char* hostName);
void ptpInterfaceDeletePeer(PTPInterfaceRef itf, const char* hostName);
void ptpUpdate(uint64_t instanceId, PtpTimeDataRef data);
void ptpReset(uint64_t instanceId, uint64_t masterClockId);

static void ptpInterfacesMutexInitializerCallback(void* ctx)
{
    PtpMutexRef* cs = (PtpMutexRef*)ctx;

    if (cs && !(*cs)) {
        *cs = ptpMutexCreate(NULL);
    }
}

/**
 * @brief acquires reference to PTPInterface singleton
 * @return PTPInterface singleton
 */
PtpMutexRef ptpInterfacesMutexInstance(void)
{
    // atomically create interfaces mutex object
    static PtpMutexRef cs = NULL;

    // do a quick atomic check whether the interface has already been initialized (it must be non zero)
    if (atomic_add_and_fetch_32((int32_t*)&cs, 0) != 0)
        return cs;

    // interace pointer is zero, execute thread safe callback to allocate it:
    ptpThreadSafeCallback(ptpInterfacesMutexInitializerCallback, &cs);
    return cs;
}

/**
 * @brief acquire valid reference to PTPInterface object for given instance id
 * @param instanceId PTPInterface instance identifier
 * @return PTPInterface singleton
 */
PTPInterfaceRef ptpInterfacesAcquireInstance(uint64_t instanceId)
{
    // acquire global mutex
    PtpMutexRef cs = ptpInterfacesMutexInstance();
    PTPInterfaceRef ret = NULL;

    // atomically find/create ptp interface for given instance id
    ptpMutexLock(cs);

    // allocate list of interfaces if necessary
    if (!g_interfaces) {
        g_interfaces = ptpListPTPInterfaceCreate();
    }

    // search for interface with given instance id:
    ListPTPInterfaceIterator it = NULL;
    for (it = ptpListPTPInterfaceFirst(g_interfaces); it != NULL; it = ptpListPTPInterfaceNext(it)) {
        PTPInterfaceRef itf = ptpListPTPInterfaceIteratorValue(it);

        if (itf->_instanceId == instanceId) {
            // we've found a match:
            ret = itf;
            break;
        }
    }

    // if it doesn't exist yet, create it:
    if (!ret) {
        ret = (PTPInterfaceRef)malloc(sizeof(PTPInterface));
        ptpInterfaceCtor(ret, instanceId);

        // and add it to the list of interfaces
        ptpListPTPInterfacePushBack(g_interfaces, ret);
    }
    ptpMutexUnlock(cs);
    return ret;
}

/**
 * @brief Release PTPInterface object for given instance id
 *
 * @param instanceId PTPInterface instance identifier
 * @return Boolean true if succeeded, false otherwise
 */
Boolean ptpInterfacesReleaseInstance(uint64_t instanceId)
{
    // acquire global mutex
    PtpMutexRef cs = ptpInterfacesMutexInstance();
    Boolean ret = false;

    // atomically find/create ptp interface for given instance id
    ptpMutexLock(cs);

    if (!g_interfaces) {
        // singleton is missing, leave
        ptpMutexUnlock(cs);
        return false;
    }

    // search for interface with given instance id:
    ListPTPInterfaceIterator it = NULL;
    for (it = ptpListPTPInterfaceFirst(g_interfaces); it != NULL; it = ptpListPTPInterfaceNext(it)) {
        PTPInterfaceRef itf = ptpListPTPInterfaceIteratorValue(it);

        if (itf->_instanceId == instanceId) {
            // we've found a match - let's delete this instance
            ptpInterfaceDtor(itf);
            free(itf);

            // remove now invalid interface from the list
            ptpListPTPInterfaceErase(g_interfaces, it);
            ret = true;
            break;
        }
    }
    ptpMutexUnlock(cs);
    return ret;
}

void ptpInterfaceCtor(PTPInterfaceRef itf, uint64_t instanceId)
{
    assert(itf);

    itf->_pClock = NULL;
    itf->_port = NULL;
    itf->_ifname = NULL;
    itf->_priority1 = 248;
    itf->_priority2 = 248;
    itf->_etherPhyDelay = NULL;
    itf->_instanceId = instanceId;
    itf->_mutex = ptpMutexCreate(NULL);
}

void ptpInterfaceDtor(PTPInterfaceRef itf)
{
    if (!itf)
        return;

    ptpInterfaceStop(itf);

    ptpMutexDestroy(itf->_mutex);
    itf->_mutex = NULL;
}

Boolean ptpInterfaceInitialize(PTPInterfaceRef itf, const char* interfaceName, uint8_t priority1, uint8_t priority2)
{
    assert(itf);

    // make sure we are deinitialized
    ptpInterfaceDeinitialize(itf);

    // interface name object
    itf->_ifname = strdup(interfaceName);
    itf->_priority1 = priority1;
    itf->_priority2 = priority2;

    // use default values for Phy delay
    PeripheralDelay* delay1G = ptpRefPeripheralDelayAlloc();
    ptpPeripheralDelaySetLinkSpeed(delay1G, LINKSPEED_1GBIT);
    ptpPeripheralDelaySet(delay1G, PHY_DELAY_GB_TX_I20, PHY_DELAY_GB_RX_I20);

    PeripheralDelay* delay100M = ptpRefPeripheralDelayAlloc();
    ptpPeripheralDelaySetLinkSpeed(delay100M, LINKSPEED_100MBIT);
    ptpPeripheralDelaySet(delay100M, PHY_DELAY_MB_TX_I20, PHY_DELAY_MB_RX_I20);

    itf->_etherPhyDelay = ptpListPeripheralDelayCreate();
    ptpListPeripheralDelayPushBack(itf->_etherPhyDelay, delay1G);
    ptpListPeripheralDelayPushBack(itf->_etherPhyDelay, delay100M);

    ptpMutexLock(itf->_mutex);
    memset(&(itf->_data), 0, sizeof(itf->_data));
    itf->_data._masterToLocalPhase = 0;
    itf->_data._masterToLocalRatio = 1;
    itf->_data._masterClockId = 0;
    itf->_data._syncArrivalTime = 0;
    itf->_data._syncCount = 0;
    itf->_data._portState = ePortStateInit;
    ptpMutexUnlock(itf->_mutex);

    // initialize and setup clock
    itf->_pClock = ptpClockCtxCreate();
    ptpClockCtxCtor(itf->_pClock, itf->_instanceId, priority1, priority2);

    // create port instance
    itf->_port = ptpNetPortCreate();
    ptpNetPortCtor(itf->_port, itf->_ifname, itf->_pClock, itf->_etherPhyDelay);
    ptpNetPortSetSyncInterval(itf->_port, SYNC_INTERVAL_INITIAL_LOG);

    // set overridden port state
    ptpNetPortSetState(itf->_port, ePortStateMaster);
    return true;
}

void ptpInterfaceDeinitialize(PTPInterfaceRef itf)
{
    assert(itf);

    if (itf->_etherPhyDelay) {
        ptpListPeripheralDelayDestroy(itf->_etherPhyDelay);
        itf->_etherPhyDelay = NULL;
    }

    if (itf->_port) {
        ptpNetPortDestroy(itf->_port);
        itf->_port = NULL;
    }

    if (itf->_ifname) {
        free(itf->_ifname);
        itf->_ifname = NULL;
    }

    if (itf->_pClock) {
        ptpClockCtxDeinitialize(itf->_pClock);
        ptpClockCtxDestroy(itf->_pClock);
        itf->_pClock = NULL;
    }
}

Boolean ptpInterfaceStart(PTPInterfaceRef itf)
{
    assert(itf);
    return ptpNetPortProcessEvent(itf->_port, evInitializing);
}

void ptpInterfaceStop(PTPInterfaceRef itf)
{
    assert(itf);
    ptpInterfaceDeinitialize(itf);
}

void ptpInterfaceRefresh(PTPInterfaceRef itf, PtpTimeDataRef data)
{
    assert(itf && data);

    ptpMutexLock(itf->_mutex);
    memcpy(data, &(itf->_data), sizeof(itf->_data));
    ptpMutexUnlock(itf->_mutex);
}

void ptpInterfaceAddPeer(PTPInterfaceRef itf, const char* hostName)
{
    assert(itf);

    if (!hostName || !itf->_port)
        return;

    ptpNetPortAddUnicastSendNode(itf->_port, hostName);
}

void ptpInterfaceDeletePeer(PTPInterfaceRef itf, const char* hostName)
{
    assert(itf);

    if (!hostName || !itf->_port)
        return;

    ptpNetPortDeleteUnicastSendNode(itf->_port, hostName);
}

void ptpUpdate(uint64_t instanceId, PtpTimeDataRef data)
{
    PTPInterfaceRef itf = ptpInterfacesAcquireInstance(instanceId);
    double currentRatio = 0;
    double filteredRatio = 1.0;

    assert(itf && data);

    //
    // Atomically copy PTP context to our internal context
    //

    ptpMutexLock(itf->_mutex);

    // new value of master/local ratio
    currentRatio = data->_masterToLocalRatio;

    // previous filtered value of master/local ratio
    filteredRatio = itf->_data._masterToLocalRatio;

    // copy all other properties
    memcpy(&(itf->_data), data, sizeof(itf->_data));

    //
    // Filter the master/local clock ration using exponential filter
    //

    // the new value should make sense:
    if (currentRatio < 3.0) {

        // if we are  doing a first calculation, let's reset the exponential filter
        if ((filteredRatio < 0.5) || (filteredRatio > 2.0)) {
            PTP_LOG_INFO("Resetting filtered ratio %lf to the new value %lf", filteredRatio, currentRatio);
            filteredRatio = currentRatio;
        }

        // exponential filter
        filteredRatio = (filteredRatio * PTP_CLOCK_RATIO_FILTER_ALPHA) + (currentRatio * PTP_CLOCK_RATIO_FILTER_BETA);
    }

    itf->_data._masterToLocalRatio = filteredRatio;
    ptpMutexUnlock(itf->_mutex);

    //
    // dump current state
    //

    PTP_LOG_VERBOSE("=============================================");
    PTP_LOG_VERBOSE("Clock data update:");
    PTP_LOG_VERBOSE("master/local offset\t:%lf s", NANOSECONDS_TO_SECONDS(itf->_data._masterToLocalPhase));
    PTP_LOG_VERBOSE("master/local rate filt\t:%lf", itf->_data._masterToLocalRatio);
    PTP_LOG_VERBOSE("master/local rate\t:%lf", data->_masterToLocalRatio);

    PTP_LOG_VERBOSE("master clock id\t\t:%" PRIx64, itf->_data._masterClockId);
    PTP_LOG_VERBOSE("ptp domain number\t:%u", (unsigned int)itf->_data._grandmasterDomainNumber);
    PTP_LOG_VERBOSE("clock identity\t:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
        (unsigned int)itf->_data._localClockId[0], (unsigned int)itf->_data._localClockId[1],
        (unsigned int)itf->_data._localClockId[2], (unsigned int)itf->_data._localClockId[3],
        (unsigned int)itf->_data._localClockId[4], (unsigned int)itf->_data._localClockId[5],
        (unsigned int)itf->_data._localClockId[6], (unsigned int)itf->_data._localClockId[7]);

    PTP_LOG_VERBOSE("priority1\t\t:%u", (unsigned int)itf->_data._priority1);
    PTP_LOG_VERBOSE("clock_class\t\t:%u", (unsigned int)itf->_data._clockClass);
    PTP_LOG_VERBOSE("clock_accuracy\t:%u", (unsigned int)itf->_data._clockAccuracy);
    PTP_LOG_VERBOSE("priority2\t\t:%u", (unsigned int)itf->_data._priority2);
    PTP_LOG_VERBOSE("domain_number\t:%u", (unsigned int)itf->_data._domainNumber);
    PTP_LOG_VERBOSE("log sync interval\t:%d", (int)itf->_data._logSyncInterval);
    PTP_LOG_VERBOSE("log announce intrvl\t:%d", (int)itf->_data._logAnnounceInterval);
    PTP_LOG_VERBOSE("port number\t\t:%u", (unsigned int)itf->_data._portNumber);
    PTP_LOG_VERBOSE("sync count\t\t:%u", itf->_data._syncCount);
    PTP_LOG_VERBOSE("Port State\t\t:%s", PortStateStr(itf->_data._portState));
    PTP_LOG_VERBOSE("=============================================");
}

void ptpReset(uint64_t instanceId, uint64_t masterClockId)
{
    PTPInterfaceRef itf = ptpInterfacesAcquireInstance(instanceId);
    assert(itf);

    ptpMutexLock(itf->_mutex);

    itf->_data._masterToLocalPhase = 0;
    itf->_data._masterToLocalRatio = 1;
    itf->_data._syncArrivalTime = 0;
    itf->_data._syncCount = 0;
    itf->_data._portState = ePortStateInit;
    itf->_data._masterClockId = masterClockId;

    ptpMutexUnlock(itf->_mutex);
}

Boolean ptpStart(const char* itfName, uint64_t instanceId, uint8_t priority1, uint8_t priority2)
{
    PTPInterfaceRef itf = ptpInterfacesAcquireInstance(instanceId);
    assert(itf);

    // make sure we are stopped before proceeding
    ptpInterfaceStop(itf);

    if (!ptpInterfaceInitialize(itf, itfName, priority1, priority2))
        return false;

    return ptpInterfaceStart(itf);
}

Boolean ptpStop(uint64_t instanceId)
{
    // stop the interface
    ptpInterfaceStop(ptpInterfacesAcquireInstance(instanceId));

    // and release the interface instance, it is not needed anymore
    return ptpInterfacesReleaseInstance(instanceId);
}

// retrieve current ptp data
void ptpRefresh(uint64_t instanceId, PtpTimeDataRef data)
{
    ptpInterfaceRefresh(ptpInterfacesAcquireInstance(instanceId), data);
}

// retrieve ptp version string
const char* ptpVersion()
{
    static char versionString[] = PTP_VERSION_STRING;
    return versionString;
}

void ptpAddPeer(uint64_t instanceId, const char* hostName)
{
    ptpInterfaceAddPeer(ptpInterfacesAcquireInstance(instanceId), hostName);
}

void ptpRemovePeer(uint64_t instanceId, const char* hostName)
{
    ptpInterfaceDeletePeer(ptpInterfacesAcquireInstance(instanceId), hostName);
}

//
// ptp server/client test
//

void ptpTest(uint64_t instanceId, const char* interfaceName, const char* hostName)
{
    if (!hostName)
        return;

    // if we failed, leave
    if (!interfaceName) {
        return;
    }

#ifdef BUILDTYPE_PC
    if (!ptpStart(interfaceName, instanceId, 250, 248)) {
#else
    if (!ptpStart(interfaceName, instanceId, 248, 248)) {
#endif
        return;
    }

    if (hostName) {
        ptpAddPeer(instanceId, hostName);
    }
}
