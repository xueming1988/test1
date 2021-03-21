/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpNetworkPort.h"
#include "Common/ptpList.h"
#include "Common/ptpLog.h"
#include "Common/ptpMutex.h"
#include "ptpClockContext.h"
#include "ptpConstants.h"
#include "ptpForeignClock.h"
#include "ptpNetworkAccessContext.h"
#include "ptpNetworkInterface.h"

#include <assert.h>
#include <math.h>
#include <string.h>

/**
 * @brief Maps PortIdentity object to the matching link layer address
 */
typedef struct {
    PortIdentity _portIdentity;
    LinkLayerAddress _address;
} PortIdentityAddr;

typedef PortIdentityAddr* PortIdentityAddrRef;
DECLARE_LIST(PortIdentityAddr)

void ptpPortIdentityAddrCtor(PortIdentityAddrRef addr);
void ptpPortIdentityAddrDtor(PortIdentityAddrRef addr);
void ptpListPortIdentityAddrObjectDtor(PortIdentityAddrRef identity);

void ptpPortIdentityAddrCtor(PortIdentityAddrRef addr)
{
    assert(addr);

    // construct PortIdentity object
    ptpPortIdentityCtor(&(addr->_portIdentity));
}

void ptpPortIdentityAddrDtor(PortIdentityAddrRef addr)
{
    if (!addr)
        return;

    // empty
}

DECLARE_MEMPOOL(PortIdentityAddr)
DEFINE_MEMPOOL(PortIdentityAddr, ptpPortIdentityAddrCtor, ptpPortIdentityAddrDtor)

static PortIdentityAddrRef ptpPortIdentityAddrCreate(void)
{
    return ptpMemPoolPortIdentityAddrAllocate();
}

static void ptpPortIdentityAddrDestroy(PortIdentityAddrRef mapping)
{
    ptpMemPoolPortIdentityAddrDeallocate(mapping);
}

void ptpListPortIdentityAddrObjectDtor(PortIdentityAddrRef identity)
{
    ptpPortIdentityAddrDestroy(identity);
}

// linked list of PortIdentity smart objects
DEFINE_LIST(PortIdentityAddr, ptpListPortIdentityAddrObjectDtor)

typedef struct NetworkPort {

    ClockCtxRef _clock;
    PortIdentity _portIdentity;
    PortState _portState;
    MacAddress _localAddress;
    ListForeignClockRef _foreignMasters;
    ForeignClockRef _selectedMaster;
    ClockDatasetRef _selectedMasterDataset;
    NetworkAccessContextRef _networkAccessContext;

    // link speed in kb/sec
    ListPeripheralDelayRef _peripheralDelay;
    uint32_t _linkSpeed;
    TimestampRef _txDelay;
    TimestampRef _rxDelay;

    PtpMutexRef _portTxLock;
    PtpMutexRef _processingLock;
    PtpMutexRef _sendNodeListLock;

    int8_t _logMeanSyncInterval;
    int8_t _logMeanAnnounceInterval;
    int32_t _syncIntervalTimeoutExpireCnt;
    NanosecondTs _linkDelay;

    // the number of successfull synchronizations (or 0 in master mode)
    uint32_t _syncCount;

    uint16_t _announceSequenceId;
    uint16_t _signalSequenceId;
    uint16_t _syncSequenceId;
    uint16_t _delayReqSequenceId;

    // time base indicator to verify received follow up message
    uint16_t _lastGmTimeBaseIndicator;

    LastTimestamps _lastTimestamps;
    NanosecondTs _lastLocalTime;
    NanosecondTs _lastRemoteTime;
    int _badDiffCount;
    int _goodSyncCount;
    int _maxBadDiffCount;
    int _successDiffCount;

    MsgSyncRef _currentSync[PTP_MSG_HISTORY_LEN];
    MsgFollowUpRef _currentFollowUp[PTP_MSG_HISTORY_LEN];
    MsgDelayReqRef _currentDelayRequest[PTP_MSG_HISTORY_LEN];

    ListPortIdentityAddrRef _portIdentityAddrList;
    ListLinkLayerAddressRef _unicastSendNodeList;
} NetworkPort;

static void ptpListNetworkPortObjectDtor(NetworkPortRef port)
{
    // no deallocation here
    (void)port;
}

DEFINE_LIST(NetworkPort, ptpListNetworkPortObjectDtor);

void ptpNetPortProcessMessage(NetworkPortRef port, LinkLayerAddressRef remote, uint8_t* buf, size_t length, Timestamp ingressTime, NetResult rrecv)
{
    MsgCommonRef msg;
    assert(port);

    if (eNetSucceed == rrecv) {

        //
        // compensate peripheral RX delay
        //

        ptpMutexLock(port->_sendNodeListLock);
        if (port->_rxDelay) {
            ptpTimestampSub(&ingressTime, &ingressTime, port->_rxDelay);
        }

        //
        // filter out any messages that don't belong to us:
        //

        ListLinkLayerAddressIterator it;
        Boolean addressValid = false;
        for (it = ptpListLinkLayerAddressFirst(port->_unicastSendNodeList); it != NULL; it = ptpListLinkLayerAddressNext(it)) {

            LinkLayerAddressRef address = ptpListLinkLayerAddressIteratorValue(it);

            if (ptpLinkLayerAddressEqualsTo(address, remote)) {
                addressValid = true;
                break;
            }
        }

        ptpMutexUnlock(port->_sendNodeListLock);

        if (!addressValid) {
#if PTP_LOG_VERBOSE_ON
            char remoteStr[ADDRESS_STRING_LENGTH] = { 0 };
            ptpLinkLayerAddressString(remote, remoteStr, sizeof(remoteStr));
            PTP_LOG_VERBOSE("Received message from unknown address: %s, discarding", remoteStr);
#endif
            return;
        }

        //
        // retrieve expected clock identity
        //

        ClockIdentity expectedClockIdentity;
        ptpClockIdentityCtor(&expectedClockIdentity);
        bool expectedClockIdentityValid = false;

        // in slave state we will filter out any messages that don't match expected Clock identity
        if (ePortStateSlave == port->_portState) {
            ptpClockIdentityAssign(&expectedClockIdentity, ptpClockCtxGetGrandmasterClockIdentity(port->_clock));
            expectedClockIdentityValid = true;
        }

        //
        // create message out of received payload
        //

        msg = ptpMsgCommonBuildPTPMessage(buf, length, remote, port, expectedClockIdentityValid ? &expectedClockIdentity : NULL, &ingressTime);

        if (msg) {
            // message has valid clock identity and/or it is an Announce message. Let's process it:
            ptpMsgCommonProcessMessage(msg, &ingressTime, port);
        }

    } else if (rrecv == eNetFatal) {

        PTP_LOG_ERROR("read from network interface failed");
        ptpNetPortProcessEvent(port, evFailed);
    }
}

NetworkPortRef ptpNetPortCreate()
{
    NetworkPortRef port = (NetworkPortRef)malloc(sizeof(struct NetworkPort));

    if (!port)
        return NULL;

    return port;
}

void ptpNetPortDestroy(NetworkPortRef port)
{
    if (!port)
        return;

    ptpNetPortDtor(port);
    free(port);
}

void ptpNetPortCtor(NetworkPortRef port, const char* ifname, ClockCtxRef clockContext, const ListPeripheralDelayRef phyDelayList)
{
    assert(port && ifname);

    port->_clock = clockContext;
    port->_peripheralDelay = phyDelayList;
    port->_portTxLock = ptpMutexCreate(NULL);
    port->_processingLock = ptpMutexCreate(NULL);
    port->_sendNodeListLock = ptpMutexCreate(NULL);
    port->_portIdentityAddrList = ptpListPortIdentityAddrCreate();
    port->_unicastSendNodeList = ptpListLinkLayerAddressCreate();

    port->_txDelay = NULL;
    port->_rxDelay = NULL;
    port->_linkSpeed = INVALID_LINKSPEED;
    port->_linkDelay = INVALID_LINKDELAY;
    port->_logMeanSyncInterval = 0;
    port->_logMeanAnnounceInterval = 0;
    port->_syncIntervalTimeoutExpireCnt = 0;
    port->_syncCount = 0;
    port->_announceSequenceId = 0;
    port->_signalSequenceId = 0;
    port->_syncSequenceId = 0;
    port->_delayReqSequenceId = 0;
    port->_lastGmTimeBaseIndicator = 0;
    port->_lastLocalTime = 0;
    port->_lastRemoteTime = 0;
    port->_badDiffCount = 0;
    port->_goodSyncCount = 0;
    port->_maxBadDiffCount = PTP_MAX_BAD_DIFF_INITIAL_CNT;
    port->_successDiffCount = 0;
    port->_portState = ePortStateInit;

    int i;
    for (i = 0; i < PTP_MSG_HISTORY_LEN; i++) {
        port->_currentSync[i] = NULL;
        port->_currentFollowUp[i] = ptpRefMsgFollowUpAlloc();
        port->_currentDelayRequest[i] = ptpRefMsgDelayReqAlloc();
    }

    // assign ourself to the clock object
    ptpClockCtxSetPort(port->_clock, port);

    // initialize network access context
    port->_networkAccessContext = ptpNetworkAccessContextCreate(port, ifname, &(port->_localAddress));

    // and assign local address of the network interface to clock identity
    ptpClockCtxAssignClockIdentity(port->_clock, &(port->_localAddress));

    uint32_t speed;
    if (ptpNetworkAccessContextGetLinkSpeed(port->_networkAccessContext, &speed)) {
        ptpNetPortSetLinkSpeed(port, speed);
    }

    // create list of foreign masters
    port->_foreignMasters = ptpListForeignClockCreate();
    port->_selectedMaster = NULL;
    port->_selectedMasterDataset = NULL;

    // create port identity
    ptpPortIdentityCtor(&(port->_portIdentity));
    ptpPortIdentitySetPortNumber(&(port->_portIdentity), 0x8000); // sru: port should be starting from 0x8000, according to aPTP spec for End-to-End UDP links
    ptpPortIdentityAssignClockIdentity(&(port->_portIdentity), ptpClockCtxGetClockIdentity(port->_clock));

    ptpNetPortSetSyncInterval(port, SYNC_INTERVAL_INITIAL_LOG);
    ptpNetPortSetSyncCount(port, 0);
}

void ptpNetPortDtor(NetworkPortRef port)
{
    if (!port)
        return;

    // shutdown interface
    ptpNetworkAccessContextRelease(port->_networkAccessContext, port);
    port->_networkAccessContext = NULL;

    if (port->_foreignMasters) {
        ptpListForeignClockDestroy(port->_foreignMasters);
    }

    if (port->_selectedMasterDataset) {
        ptpClockDatasetDestroy(port->_selectedMasterDataset);
        port->_selectedMasterDataset = NULL;
    }

    ptpListPortIdentityAddrDestroy(port->_portIdentityAddrList);

    ptpMutexDestroy(port->_portTxLock);
    ptpMutexDestroy(port->_processingLock);
    ptpMutexDestroy(port->_sendNodeListLock);

    int i;
    for (i = 0; i < PTP_MSG_HISTORY_LEN; i++) {
        if (port->_currentSync[i]) {
            ptpRefMsgSyncRelease(port->_currentSync[i]);
            port->_currentSync[i] = NULL;
        }

        if (port->_currentFollowUp[i]) {
            ptpRefMsgFollowUpRelease(port->_currentFollowUp[i]);
            port->_currentFollowUp[i] = NULL;
        }

        if (port->_currentDelayRequest[i]) {
            ptpRefMsgDelayReqRelease(port->_currentDelayRequest[i]);
            port->_currentDelayRequest[i] = NULL;
        }
    }

    // explicit destructor call
    ptpListLinkLayerAddressDestroy(port->_unicastSendNodeList);
}

// Gets the link delay information.
NanosecondTs ptpNetPortGetLinkDelay(NetworkPortRef port)
{
    return port->_linkDelay > 0LL ? port->_linkDelay : 0LL;
}

void ptpNetPortSetLinkDelay(NetworkPortRef port, NanosecondTs delay)
{
    port->_linkDelay = delay;
    PTP_LOG_DEBUG("Setting Link delay\t: %lf ms", NANOSECONDS_TO_MILLISECONDS(delay));
}

ClockCtxRef ptpNetPortGetClock(NetworkPortRef port)
{
    return port->_clock;
}

static void ptpNetPortStartSyncIntervalTimer(NetworkPortRef port, uint64_t waitTime)
{
    ptpMutexLock(port->_processingLock);

    // reschedule SYNC timeout timer
    PTP_LOG_VERBOSE("Rescheduling evScheduledSyncTimeout timeout in %llu ms", waitTime / 1000000);
    ptpClockCtxUnscheduledEvent(port->_clock, evScheduledSyncTimeout);
    ptpClockCtxScheduleEvent(port->_clock, port, evScheduledSyncTimeout, waitTime);

    ptpMutexUnlock(port->_processingLock);
}

static void ptpNetPortStopSyncIntervalTimer(NetworkPortRef port)
{
    ptpMutexLock(port->_processingLock);
    ptpClockCtxUnscheduledEvent(port->_clock, evScheduledSyncTimeout);
    ptpMutexUnlock(port->_processingLock);
}

static void ptpNetPortStartAnnounceIntervalTimer(NetworkPortRef port, uint64_t waitTime)
{
    ptpMutexLock(port->_processingLock);

    // reschedule ANNOUNCE timeout timer
    PTP_LOG_VERBOSE("Rescheduling evScheduledAnnounceTimeout timeout in %llu ms", waitTime / 1000000);
    ptpClockCtxUnscheduledEvent(port->_clock, evScheduledAnnounceTimeout);
    ptpClockCtxScheduleEvent(port->_clock, port, evScheduledAnnounceTimeout, waitTime);

    ptpMutexUnlock(port->_processingLock);
}

static void ptpNetPortStopAnnounceIntervalTimer(NetworkPortRef port)
{
    ptpMutexLock(port->_processingLock);
    ptpClockCtxUnscheduledEvent(port->_clock, evScheduledAnnounceTimeout);
    ptpMutexUnlock(port->_processingLock);
}

static void ptpNetPortStartAnnounce(NetworkPortRef port)
{
    ptpNetPortStartAnnounceIntervalTimer(port, 16000000);
}

static void ptpNetPortBecomeMaster(NetworkPortRef port)
{
    assert(port);

    PTP_LOG_EXCEPTION("Switching to Master");
    ptpNetPortSetState(port, ePortStateMaster);

    // Stop announce receipt timeout timer
    ptpNetPortStopAnnounceReceiptTimer(port);

    // Stop evSyncReceiptTimeout timeout timer
    ptpNetPortStopSyncReceiptTimer(port);
    ptpNetPortStartAnnounce(port);

    // schedule announce messages
    ptpNetPortStartSyncIntervalTimer(port, (uint64_t)(pow((double)2, SYNC_INTERVAL_INITIAL_LOG) * 1000000000.0));

    // we must clear selected master
    port->_selectedMaster = NULL;

    ptpMutexLock(port->_processingLock);
    if (port->_selectedMasterDataset) {
        ptpClockDatasetDestroy(port->_selectedMasterDataset);
        port->_selectedMasterDataset = NULL;
    }
    ptpMutexUnlock(port->_processingLock);

    ClockIdentityRef clockIdentity = ptpClockCtxGetClockIdentity(port->_clock);
    ptpClockCtxAssignGrandmasterClockIdentity(port->_clock, clockIdentity);

    ptpClockCtxIpcReset(port->_clock, ptpClockIdentityGet(clockIdentity));
    ptpClockCtxUpdateFollowUpTx(port->_clock);
}

static void ptpNetPortBecomeSlave(NetworkPortRef port)
{
    assert(port);

    PTP_LOG_EXCEPTION("Switching to Slave");
    ptpNetPortSetState(port, ePortStateSlave);

    // stop scheduled announce messages
    ptpNetPortStopAnnounceIntervalTimer(port);

    // stop scheduled sync messages
    ptpNetPortStopSyncIntervalTimer(port);

    // Start announce receipt timeout timer
    ptpNetPortStartAnnounceReceiptTimer(port, (uint64_t)(ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER * (uint64_t)(pow((double)2, ptpNetPortGetAnnounceInterval(port)) * 1000000000.0)));
    ptpClockCtxUpdateFollowUpTx(port->_clock);
}

/**
 * @brief ptpNetPortSetOptimalState
 * @param port reference to the NetworkPort object
 * @param state new recommended port state
 * @param masterChanged flag indicating whether the master identity has just been changed
 */
static void ptpNetPortSetOptimalState(NetworkPortRef port, PortState state, Boolean masterChanged)
{
    Boolean resetSync = false;
    assert(port);

    switch (state) {

    case ePortStateMaster:
        if (ptpNetPortGetState(port) != ePortStateMaster) {
            ptpNetPortBecomeMaster(port);
            resetSync = true;
        }
        break;

    case ePortStateSlave:
        if (ptpNetPortGetState(port) != ePortStateSlave) {
            ptpNetPortBecomeSlave(port);
            resetSync = true;
        }
        break;

    default:
        PTP_LOG_ERROR("Invalid state change requested by call to 1588Port::ptpNetPortSetOptimalState()");
        break;
    }

    if (masterChanged) {
        PTP_LOG_STATUS("Changed master! Resetting clock history");
        ptpClockCtxUpdateFollowUpTx(port->_clock);
        resetSync = true;
    }

    if (resetSync)
        port->_syncCount = 0;

    return;
}

void ptpNetPortStartSyncReceiptTimer(NetworkPortRef port, uint64_t waitTime)
{
    ptpMutexLock(port->_processingLock);

    PTP_LOG_VERBOSE("Rescheduling evSyncReceiptTimeout timeout in %llu ms", waitTime / 1000000);
    ptpClockCtxUnscheduledEvent(port->_clock, evSyncReceiptTimeout);
    ptpClockCtxScheduleEvent(port->_clock, port, evSyncReceiptTimeout, waitTime);

    ptpMutexUnlock(port->_processingLock);
}

void ptpNetPortStopSyncReceiptTimer(NetworkPortRef port)
{
    ptpMutexLock(port->_processingLock);

    PTP_LOG_VERBOSE("Stopping evSyncReceiptTimeout timeout");
    ptpClockCtxUnscheduledEvent(port->_clock, evSyncReceiptTimeout);

    ptpMutexUnlock(port->_processingLock);
}

void ptpNetPortStartAnnounceReceiptTimer(NetworkPortRef port, long long unsigned int waitTime)
{
    ptpMutexLock(port->_processingLock);

    PTP_LOG_VERBOSE("Rescheduling ANNOUNCE RECEIPT timeout in %llu ms", waitTime / 1000000);
    ptpClockCtxUnscheduledEvent(port->_clock, evAnnounceReceiptTimeout);
    ptpClockCtxScheduleEvent(port->_clock, port, evAnnounceReceiptTimeout, waitTime);

    ptpMutexUnlock(port->_processingLock);
}

void ptpNetPortStopAnnounceReceiptTimer(NetworkPortRef port)
{
    ptpMutexLock(port->_processingLock);

    PTP_LOG_VERBOSE("Stopping ANNOUNCE RECEIPT timeout");
    ptpClockCtxUnscheduledEvent(port->_clock, evAnnounceReceiptTimeout);
    ptpMutexUnlock(port->_processingLock);
}

static Boolean ptpNetPortCalculateBestForeignMaster(NetworkPortRef port)
{
    ForeignClockRef bestMaster = port->_selectedMaster;

    // go through the list of all know foreign masters
    ListForeignClockIterator iter;
    for (iter = ptpListForeignClockFirst(port->_foreignMasters); iter != NULL; iter = ptpListForeignClockNext(iter)) {

        // get foreign master object
        ForeignClockRef foreignMaster = ptpListForeignClockIteratorValue(iter);

        // prune all messages there
        ptpForeignClockPruneOldMessages(foreignMaster);

        // if the master doesn't have enough announce messages, ignore it
#ifdef LINKPLAY
        if (ptpForeignClockAnnounceMsgSize(foreignMaster) <= 0){
            continue;
        }
#else
        if (!ptpForeignClockHasEnoughMessages(foreignMaster)){
            continue;
        }
#endif

        // if it is the same as currently selected master, ignore it
        if (foreignMaster == bestMaster){
            continue;
        }

        // find the best master
        if (!bestMaster) {
            bestMaster = foreignMaster;
        } else if (ptpForeignClockIsBetterThan(foreignMaster, bestMaster)) {
            bestMaster = foreignMaster;
        } else {
            // the new master is 'worse' than best master. Let's clear it up
            ptpForeignClockClear(foreignMaster);
        }
    }

    if (port->_selectedMaster != bestMaster) {

        ClockIdentityRef oldClockIdentity = port->_selectedMaster ? ptpPortIdentityGetClockIdentity(ptpForeignClockGetPortIdentity(port->_selectedMaster)) : NULL;
        ClockIdentityRef newClockIdentity = bestMaster ? ptpPortIdentityGetClockIdentity(ptpForeignClockGetPortIdentity(bestMaster)) : NULL;

        PTP_LOG_STATUS("%s:%d, Best foreign master changed from %016llx to %016llx",__func__, __LINE__,
            oldClockIdentity ? ptpClockIdentityGetUInt64(oldClockIdentity) : 0LL,
            newClockIdentity ? ptpClockIdentityGetUInt64(newClockIdentity) : 0LL);

        port->_selectedMaster = bestMaster;

        // atomically upate the best foreign master dataset
        ptpMutexLock(port->_processingLock);
        if (port->_selectedMasterDataset) {
            ptpClockDatasetDestroy(port->_selectedMasterDataset);
        }
        port->_selectedMasterDataset = ptpForeignClockGetDataset(port->_selectedMaster);
        ptpMutexUnlock(port->_processingLock);
        return true;
    }

    return false;
}


static inline void ptpNetPortDumpClockDS(ClockDatasetRef dataSetA)
{

    if (!dataSetA)
        return ;

    //Prevent Warning
    uint64_t *pID = (uint64_t*)(dataSetA->_identity._id);
    uint64_t clockID = ntoh64(*pID);

    PTP_LOG_INFO("Clock DS -> gmID:%016llx priority1:%3d qualityClass:%3d qualityAccuracy:%3d logVariance:%3d priority2:%3d stepsRemoved:%d",
        clockID,
        dataSetA->_priority1,
        dataSetA->_quality._class,
        dataSetA->_quality._accuracy,
        dataSetA->_quality._offsetScaledLogVariance,
        dataSetA->_priority2,
        dataSetA->_stepsRemoved);
}

static void ptpNetPortBestMasterClockDecision(NetworkPortRef port)
{
    assert(port);

    ClockDatasetRef clockDs, bestForeignDs;
    ClockCtxRef clock = ptpNetPortGetClock(port);

    // get our clock default dataset
    clockDs = ptpClockCtxDefaultDataset(clock);

    // atomically retrieve dataset of the best foreign clock
    ptpMutexLock(port->_processingLock);
    bestForeignDs = ptpClockDatasetCreateCopy(port->_selectedMasterDataset);
    ptpMutexUnlock(port->_processingLock);

    if (ptpClockDatasetIsBetterThan(clockDs, bestForeignDs)) {

        ptpNetPortDumpClockDS(clockDs);
        ptpNetPortDumpClockDS(bestForeignDs);

        PTP_LOG_EXCEPTION("Our clock %016llx is better than announced clock %016llx, %s master mode",
            ptpClockIdentityGetUInt64(&clockDs->_identity),
            bestForeignDs ? ptpClockIdentityGetUInt64(&bestForeignDs->_identity) : 0LL,
            (ptpNetPortGetState(port) != ePortStateMaster) ? "switching to" : "staying in");

        // jump into master mode
        ptpNetPortSetOptimalState(port, ePortStateMaster, true);

    } else {

        Boolean changedExternalMaster = ptpClockIdentityNotEqualTo(ptpClockCtxGetGrandmasterClockIdentity(clock), &(bestForeignDs->_identity));

        if(changedExternalMaster){
            PTP_LOG_EXCEPTION("%s:%d Clock DS:", __func__, __LINE__);
            ptpNetPortDumpClockDS(clockDs);
            ptpNetPortDumpClockDS(bestForeignDs);

            if(ptpClockDatasetIsBetterThan(clockDs, bestForeignDs)){
                PTP_LOG_EXCEPTION("%s:%d clock %016llx is better than foreign clock %016llx, Why do we need switch ??\n", __func__, __LINE__, ptpClockIdentityGetUInt64(&clockDs->_identity),
                    bestForeignDs ? ptpClockIdentityGetUInt64(&bestForeignDs->_identity) : 0LL);
            }

            PTP_LOG_EXCEPTION("%s slave mode (master was %s):  %016llx ->  %016llx", (ptpNetPortGetState(port) != ePortStateSlave) ? "Switching to" : "Staying in", (changedExternalMaster ? "changed" : "NOT changed"),
                ptpClockIdentityGetUInt64(ptpClockCtxGetGrandmasterClockIdentity(clock)),
                ptpClockIdentityGetUInt64(&bestForeignDs->_identity));

            // switch/stay in slave mode
            ptpNetPortSetOptimalState(port, ePortStateSlave, changedExternalMaster);
            
            // keep master clock settings
            ptpClockCtxAssignGrandmasterClockIdentity(clock, &(bestForeignDs->_identity));
            
            ptpClockCtxSetGrandmasterPriority1(clock, bestForeignDs->_priority1);
            ptpClockCtxSetGrandmasterPriority2(clock, bestForeignDs->_priority2);
            ptpClockCtxAssignGrandmasterClockQuality(clock, &(bestForeignDs->_quality));
            ptpClockCtxSetGrandmasterStepsRemoved(clock, bestForeignDs->_stepsRemoved);
        }
    }

    ptpClockDatasetDestroy(clockDs);
    ptpClockDatasetDestroy(bestForeignDs);
}

static Boolean ptpNetPortProcessSyncOrAnnounceTimeout(NetworkPortRef port, PtpEvent e)
{
    assert(port);

    // Nothing to do
    if (ptpClockCtxGetPriority1(port->_clock) == 255)
        return true;

    // Restart timer
    if (e == evAnnounceReceiptTimeout) {
        PTP_LOG_EXCEPTION("%s:%d Announce receipt timeout.", __func__, __LINE__);

        // announce timeout -> restart it
        ptpNetPortStartAnnounceReceiptTimer(port, (uint64_t)(ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER * (uint64_t)(pow((double)2, ptpNetPortGetAnnounceInterval(port)) * PTP_AVERAGE_MSG_RECV_INTERVAL)));
    } else {
        PTP_LOG_EXCEPTION("%s:%d Sync receipt timeout.", __func__, __LINE__);

        // sync timeout -> restart it
        ptpNetPortStartSyncReceiptTimer(port, (uint64_t)(SYNC_RECEIPT_TIMEOUT_MULTIPLIER * ((double)pow((double)2, ptpNetPortGetSyncInterval(port)) * PTP_AVERAGE_MSG_RECV_INTERVAL)));
    }

    if (ptpNetPortGetState(port) == ePortStateMaster)
        return true;

    ptpClockCtxAssignGrandmasterClockIdentity(port->_clock, ptpClockCtxGetClockIdentity(port->_clock));
    ptpClockCtxSetGrandmasterPriority1(port->_clock, ptpClockCtxGetPriority1(port->_clock));
    ptpClockCtxSetGrandmasterPriority2(port->_clock, ptpClockCtxGetPriority2(port->_clock));
    ptpClockCtxAssignGrandmasterClockQuality(port->_clock, ptpClockCtxGetClockQuality(port->_clock));

    // switch us to master
    ptpNetPortBecomeMaster(port);
    ptpNetPortStartSyncIntervalTimer(port, 16000000);
    ptpNetPortStartAnnounce(port);

    return true;
}

Boolean ptpNetPortProcessEvent(NetworkPortRef port, PtpEvent e)
{
    Boolean ret = true;

    switch (e) {

    case evInitializing: {

        PTP_LOG_DEBUG("Received Initializing event");

        // If port has been configured as master or slave, run media
        // specific configuration. If it hasn't been configured
        // start listening for announce messages
        if (ptpClockCtxGetPriority1(port->_clock) == 255 || port->_portState == ePortStateSlave) {
            ptpNetPortBecomeSlave(port);
        } else if (port->_portState == ePortStateMaster) {
            ptpNetPortBecomeMaster(port);
        } else {
            ptpClockCtxScheduleEvent(port->_clock, port, evAnnounceReceiptTimeout, (uint64_t)(ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER * pow(2.0, ptpNetPortGetAnnounceInterval(port)) * 1000000000.0));
        }

        if (!ptpNetworkAccessContextStartProcessing(port->_networkAccessContext)) {
            PTP_LOG_ERROR("Error creating event port thread");
            ret = false;
            break;
        }

        ret = true;
    } break;

    case evLinkIsUp: {
        PTP_LOG_STATUS("Link is up");

        if (ptpClockCtxGetPriority1(port->_clock) == 255 || ptpNetPortGetState(port) == ePortStateSlave) {
            ptpNetPortBecomeSlave(port);
        } else if (ptpNetPortGetState(port) == ePortStateMaster) {
            ptpNetPortBecomeMaster(port);
        } else {
            ptpNetPortStartAnnounceReceiptTimer(port, (uint64_t)(ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER * pow(2.0, ptpNetPortGetAnnounceInterval(port)) * 1000000000.0));
        }

        ret = true;
    } break;

    case evLinkIsDown: {

        PTP_LOG_STATUS("Link is down");
        ret = true;
    } break;

    case evStateChange:
        ptpNetPortBestMasterClockDecision(port);
        ret = true;
        break;

    case evAnnounceReceiptTimeout:
    case evSyncReceiptTimeout: {

        if (e == evAnnounceReceiptTimeout) {

            PTP_LOG_INFO("evAnnounceReceiptTimeout occured");
        } else if (e == evSyncReceiptTimeout) {

            PTP_LOG_INFO("evSyncReceiptTimeout occured");
        }

        ret = ptpNetPortProcessSyncOrAnnounceTimeout(port, e);
    } break;

    case evScheduledAnnounceTimeout: {

        PTP_LOG_DEBUG("evScheduledAnnounceTimeout occured");

        ret = true;

        if (port->_portState != ePortStateSlave) {

            //
            // Send an announce message
            //

            MsgAnnounceRef annc = ptpRefMsgAnnounceAlloc();
            ptpMsgAnnouncePortCtor(annc, port);
            ptpMsgCommonAssignPortIdentity(ptpMsgAnnounceGetBase(annc), ptpNetPortGetPortIdentity(port));

            ptpNetPortLockTxLock(port);
            ptpMsgAnnounceSendPort(annc, port, NULL);
            ptpNetPortUnlockTxLock(port);
            ptpRefMsgAnnounceRelease(annc);

            ptpNetPortStartAnnounceIntervalTimer(port, 1000000000);

            //
            // ... immediately followed by signalling message
            //

            MsgSignallingRef sign = ptpRefMsgSignallingAlloc();
            ptpMsgSignallingPortCtor(sign, port);
            ptpMsgCommonAssignPortIdentity(ptpMsgSignallingGetBase(sign), ptpNetPortGetPortIdentity(port));

            uint8_t interval = 0; // 0 => 1 second
            ptpMsgSignallingSetintervals(sign, interval, (uint8_t)ptpNetPortGetSyncInterval(port), SIGNALLING_INTERVAL_NO_CHANGE);

            ptpNetPortLockTxLock(port);
            ptpMsgSignallingSendPort(sign, port, NULL);
            ptpNetPortUnlockTxLock(port);
            ptpRefMsgSignallingRelease(sign);

            ret = true;
        }
    } break;

    case evScheduledSyncTimeout: {

        ret = true;
        PTP_LOG_DEBUG("evScheduledSyncTimeout occured, count = %d", port->_syncIntervalTimeoutExpireCnt);

        if (port->_syncIntervalTimeoutExpireCnt > 2) {

            // Send a sync message and then a followup to broadcast
            MsgSyncRef sync = ptpRefMsgSyncAlloc();
            ptpMsgSyncPortCtor(sync, port);
            ptpMsgCommonAssignPortIdentity(ptpMsgSyncGetBase(sync), ptpNetPortGetPortIdentity(port));

            ptpNetPortLockTxLock(port);
            Boolean txSucceeded = ptpMsgSyncSendPort(sync, port, NULL);
            ptpNetPortUnlockTxLock(port);

            if (txSucceeded) {

                TimestampRef syncTimestamp = ptpMsgSyncGetOriginTimestamp(sync);

                // allocate follow up message
                MsgFollowUpRef followUp = ptpRefMsgFollowUpAlloc();
                ptpMsgFollowUpPortCtor(followUp, port);

                ptpMsgFollowUpSetClockSourceTime(followUp, ptpClockCtxGetFollowUpTlvTx(port->_clock));
                ptpMsgCommonAssignPortIdentity(ptpMsgFollowUpGetBase(followUp), ptpNetPortGetPortIdentity(port));
                ptpMsgCommonSetSequenceId(ptpMsgFollowUpGetBase(followUp), ptpMsgCommonGetSequenceId(ptpMsgSyncGetBase(sync)));
                ptpMsgFollowUpSetPreciseOriginTimestamp(followUp, syncTimestamp);

                ptpNetPortLockTxLock(port);
                ptpMsgFollowUpSendPort(followUp, port, NULL);
                ptpNetPortUnlockTxLock(port);

                // release followup object
                ptpRefMsgFollowUpRelease(followUp);

            } else {
                PTP_LOG_ERROR("Failed to send Sync message!");
            }

            ret = true;

            uint64_t waitTime = ((uint64_t)(pow((double)2, ptpNetPortGetSyncInterval(port)) * 1000000000.0));
            ptpNetPortStartSyncIntervalTimer(port, waitTime);

            // release sync object
            ptpRefMsgSyncRelease(sync);

        } else {

            port->_syncIntervalTimeoutExpireCnt++;
            uint64_t waitTime = ((uint64_t)(pow((double)2, ptpNetPortGetSyncInterval(port)) * 1000000000.0));
            ptpNetPortStartSyncIntervalTimer(port, waitTime);
        }
    } break;

    case evFailed:
        PTP_LOG_ERROR("Received Failed event");
        break;

    default:
        PTP_LOG_ERROR("Unhandled event type %d", e);
        ret = false;
        break;
    }

    return ret;
}

TimestampRef ptpNetPortGetTxPhyDelay(NetworkPortRef port, uint32_t linkSpeed)
{
    ListPeripheralDelayIterator it;

    for (it = ptpListPeripheralDelayFirst(port->_peripheralDelay); it != NULL; it = ptpListPeripheralDelayNext(it)) {
        PeripheralDelayRef delay = ptpListPeripheralDelayIteratorValue(it);

        if (ptpPeripheralDelayGetLinkSpeed(delay) == linkSpeed) {
            TimestampRef ts = ptpTimestampCreate();
            ptpTimestampInitCtor(ts, ptpPeripheralDelayGetTx(delay), 0, 0);
            return ts;
        }
    }

    return NULL;
}

TimestampRef ptpNetPortGetRxPhyDelay(NetworkPortRef port, uint32_t linkSpeed)
{
    ListPeripheralDelayIterator it;

    for (it = ptpListPeripheralDelayFirst(port->_peripheralDelay); it != NULL; it = ptpListPeripheralDelayNext(it)) {
        PeripheralDelayRef delay = ptpListPeripheralDelayIteratorValue(it);

        if (ptpPeripheralDelayGetLinkSpeed(delay) == linkSpeed) {
            TimestampRef ts = ptpTimestampCreate();
            ptpTimestampInitCtor(ts, ptpPeripheralDelayGetRx(delay), 0, 0);
            return ts;
        }
    }

    return NULL;
}

static Boolean ptpNetPortAddForeignMaster(NetworkPortRef port, MsgAnnounceRef announce)
{
    ForeignClockRef foundMaster = NULL;

    // go through the list of all known foreign masters
    ListForeignClockIterator iter;
    for (iter = ptpListForeignClockFirst(port->_foreignMasters); iter != NULL; iter = ptpListForeignClockNext(iter)) {

        // get foreign master object
        ForeignClockRef foreignMaster = ptpListForeignClockIteratorValue(iter);

        // does it match our announce object?
        if (ptpForeignClockMatchesAnnounce(foreignMaster, announce)) {

            // yes, it does. So let's use it
            foundMaster = foreignMaster;
            break;
        }
    }

    // is this master known already?
    if (!foundMaster) {

        // log new foreign master
        ClockIdentityRef msgSenderId = ptpPortIdentityGetClockIdentity(ptpMsgAnnounceGetPortIdentity(announce));
        ClockIdentityRef msgGrandMasterClockIdentity = ptpMsgAnnounceCopyGrandmasterClockIdentity(announce);
        PTP_LOG_INFO("New foreign master:%016llx gmID:%016llx",
            ptpClockIdentityGetUInt64(msgSenderId),
            ptpClockIdentityGetUInt64(msgGrandMasterClockIdentity));
        ptpRefClockIdentityRelease(msgGrandMasterClockIdentity);

        // create new foreign master object
        foundMaster = ptpForeignClockCreate(port, announce);

        // add it to the list
        ptpListForeignClockPushBack(port->_foreignMasters, foundMaster);
        return true;
    }

    // process new announce
    return ptpForeignClockInsertAnnounce(foundMaster, announce);
}

static Boolean ptpNetPortUpdateCurrentMaster(NetworkPortRef port, MsgAnnounceRef announce)
{
    // if the received announce doesn't match our best clock, add/process it to the list of masters
    if (!port->_selectedMaster || !ptpForeignClockMatchesAnnounce(port->_selectedMaster, announce)) {
        return ptpNetPortAddForeignMaster(port, announce);
    }

    return ptpForeignClockInsertAnnounce(port->_selectedMaster, announce);
}

void ptpNetPortAnnounceReceived(NetworkPortRef port, MsgAnnounceRef announce)
{
    Boolean ret = false;
    assert(port && announce);

    switch (ptpNetPortGetState(port)) {
    case ePortStateInit:
        break;

    case ePortStateMaster:
        ret = ptpNetPortAddForeignMaster(port, announce);
        break;

    case ePortStateSlave:
        ret = ptpNetPortUpdateCurrentMaster(port, announce);
        break;

    default:
        break;
    }

    // calculate the best foreign master
    ret |= ptpNetPortCalculateBestForeignMaster(port);

    // shall we generate a new event?
    if (ret) {

        // and schedule state change event which will handle master/slave state change
        if (!ptpClockCtxIsEventScheduled(port->_clock, evStateChange))
            ptpClockCtxScheduleEvent(port->_clock, port, evStateChange, 16000000);
    }
}

void ptpNetPortSend(NetworkPortRef port, uint8_t* buf, size_t size, PortIdentityRef destIdentity, TimestampRef txTimestamp, Boolean isEvent, Boolean sendToAllUnicast)
{
    NetResult ret = eNetSucceed;
    LinkLayerAddress dest;
    ptpLinkLayerAddressCtor(&dest);

    uint16_t portNum = isEvent ? EVENT_PORT : GENERAL_PORT;

    ptpNetPortMapSocketAddr(port, destIdentity, &dest);
    ptpLinkLayerAddressSetPort(&dest, portNum);

    ptpMutexLock(port->_sendNodeListLock);

    if ((ePortStateMaster == port->_portState) && ptpListLinkLayerAddressSize(port->_unicastSendNodeList) && sendToAllUnicast) {

        Boolean foundDest = false;
        NetResult status;
        ListLinkLayerAddressIterator it;

        //
        // Use the unicast send node list if it is present
        //

        for (it = ptpListLinkLayerAddressFirst(port->_unicastSendNodeList); it != NULL; it = ptpListLinkLayerAddressNext(it)) {

            char addrStr[ADDRESS_STRING_LENGTH] = { 0 };

            LinkLayerAddress address;

            ptpLinkLayerAddressCtor(&address);
            ptpLinkLayerAddressAssign(&address, ptpListLinkLayerAddressIteratorValue(it));
            ptpLinkLayerAddressSetPort(&address, portNum);
            ptpLinkLayerAddressString(&address, addrStr, sizeof(addrStr));

            PTP_LOG_VERBOSE("Sending PTP packet to address: %s, port:%d", addrStr, portNum);
            status = ptpNetworkAccessContextSend(port->_networkAccessContext, &address, buf, size, txTimestamp, isEvent);
            ret = (status != eNetSucceed) ? status : ret;

            foundDest = ptpLinkLayerAddressEqualsTo(&dest, &address);
            ptpLinkLayerAddressDtor(&address);
        }

        if (!foundDest && ptpLinkLayerAddressIsValid(&dest)) {
            status = ptpNetworkAccessContextSend(port->_networkAccessContext, &dest, (uint8_t*)buf, size, txTimestamp, isEvent);
            ret = (status != eNetSucceed) ? status : ret;
        }

    } else {

        if (ptpLinkLayerAddressIsValid(&dest)) {

            char destStr[ADDRESS_STRING_LENGTH] = { 0 };
            ptpLinkLayerAddressString(&dest, destStr, sizeof(destStr));
            PTP_LOG_VERBOSE("Sending PTP packet to address: %s, port:%d", destStr, portNum);
            ret = ptpNetworkAccessContextSend(port->_networkAccessContext, &dest, (uint8_t*)buf, size, txTimestamp, isEvent);
        }
    }

    if (ret != eNetSucceed) {
        PTP_LOG_ERROR("ptpNetPortSend() failed");
    } else {
        // compensate peripheral TX delay
        if (txTimestamp && port->_txDelay) {
            ptpTimestampAdd(txTimestamp, txTimestamp, port->_txDelay);
        }
    }

    ptpMutexUnlock(port->_sendNodeListLock);
}

PortState ptpNetPortGetState(NetworkPortRef port)
{
    assert(port);
    return port->_portState;
}

void ptpNetPortSetState(NetworkPortRef port, PortState state)
{
    assert(port);

    if (state == port->_portState)
        return;

    switch (state) {

    case ePortStateInit:
        PTP_LOG_STATUS("setPortState: setting port state to: initializing");
        break;

    case ePortStateMaster:
        PTP_LOG_STATUS("setPortState: setting port state to: master");
        break;

    case ePortStateSlave:
        PTP_LOG_STATUS("setPortState: setting port state to: slave");
        break;
    }

    port->_portState = state;
}

char* ptpNetPortIdentityToAddressStr(NetworkPortRef port, PortIdentityRef destIdentity, char* addrStr, size_t addrStrLen)
{
    assert(port && addrStr && addrStrLen);

    if (!destIdentity) {
        strncpy(addrStr, "all addresses", addrStrLen);
        return addrStr;
    }

    LinkLayerAddress remote;
    ptpNetPortMapSocketAddr(port, destIdentity, &remote);
    ptpLinkLayerAddressString(&remote, addrStr, addrStrLen);
    return addrStr;
}

void ptpNetPortMapSocketAddr(NetworkPortRef port, PortIdentityRef destIdentity, LinkLayerAddressRef remote)
{
    assert(port);
    if (!destIdentity)
        return;

    ptpMutexLock(port->_processingLock);

    ListPortIdentityAddrIterator it;
    for (it = ptpListPortIdentityAddrFirst(port->_portIdentityAddrList); it != NULL; it = ptpListPortIdentityAddrNext(it)) {

        PortIdentityAddrRef mapping = ptpListPortIdentityAddrIteratorValue(it);
        if (ptpPortIdentityEqualsTo(&(mapping->_portIdentity), destIdentity)) {
            ptpLinkLayerAddressAssign(remote, &(mapping->_address));
            break;
        }
    }

    ptpMutexUnlock(port->_processingLock);
}

Boolean ptpNetPortAddSockAddrMap(NetworkPortRef port, PortIdentityRef destIdentity, LinkLayerAddressRef remote)
{
    assert(port);
    Boolean shallAdd = true;

    if (!destIdentity)
        return false;

    ptpMutexLock(port->_processingLock);

    ListPortIdentityAddrIterator it;
    for (it = ptpListPortIdentityAddrFirst(port->_portIdentityAddrList); it != NULL; it = ptpListPortIdentityAddrNext(it)) {

        PortIdentityAddrRef mapping = ptpListPortIdentityAddrIteratorValue(it);
        if (ptpPortIdentityEqualsTo(&(mapping->_portIdentity), destIdentity)) {
            mapping->_address = *remote;
            shallAdd = false;
            break;
        }
    }

    if (shallAdd) {

        // create new port identity ->address mapping object
        PortIdentityAddrRef mapping = ptpPortIdentityAddrCreate();
        mapping->_address = *remote;
        ptpPortIdentityAssign(&mapping->_portIdentity, destIdentity);
        ptpListPortIdentityAddrPushBack(port->_portIdentityAddrList, mapping);
    }

    ptpMutexUnlock(port->_processingLock);
    return shallAdd;
}

void ptpNetPortSetCurrentSync(NetworkPortRef port, MsgSyncRef msg)
{
    assert(port);
    ptpMutexLock(port->_processingLock);
    ptpRefMsgSyncRelease(port->_currentSync[0]);
    ptpRefMsgSyncRetain(msg);

    int i;
    for (i = 0; i < PTP_MSG_HISTORY_LEN - 1; i++) {
        port->_currentSync[i] = port->_currentSync[i + 1];
    }

    port->_currentSync[PTP_MSG_HISTORY_LEN - 1] = msg;
    ptpMutexUnlock(port->_processingLock);
}

MsgSyncRef ptpNetPortGetCurrentSync(NetworkPortRef port, PortIdentityRef expectedPortIdent, const uint16_t expectedSyncId)
{
    assert(port);

    int i;
    for (i = 0; i < PTP_MSG_HISTORY_LEN; i++) {
        PortIdentityRef syncPort = port->_currentSync[i] ? ptpMsgCommonGetPortIdentity(ptpMsgSyncGetBase(port->_currentSync[i])) : NULL;
        if (syncPort && (ptpPortIdentityEqualsTo(expectedPortIdent, syncPort)) && (ptpMsgCommonGetSequenceId(ptpMsgSyncGetBase(port->_currentSync[i])) == expectedSyncId))
            return port->_currentSync[i];
    }

    return NULL;
}

void ptpNetPortSetCurrentFollowUp(NetworkPortRef port, MsgFollowUpRef msg)
{
    assert(port);
    ptpMutexLock(port->_processingLock);
    ptpRefMsgFollowUpRelease(port->_currentFollowUp[0]);
    ptpRefMsgFollowUpRetain(msg);

    int i;
    for (i = 0; i < PTP_MSG_HISTORY_LEN - 1; i++) {
        port->_currentFollowUp[i] = port->_currentFollowUp[i + 1];
    }

    port->_currentFollowUp[PTP_MSG_HISTORY_LEN - 1] = msg;
    ptpMutexUnlock(port->_processingLock);
}

MsgFollowUpRef ptpNetPortGetCurrentFollowUp(NetworkPortRef port, PortIdentityRef expectedPortIdent, const uint16_t expectedSyncId)
{
    assert(port);

    int i;
    for (i = 0; i < PTP_MSG_HISTORY_LEN; i++) {
        PortIdentityRef followUpPort = port->_currentFollowUp[i] ? ptpMsgCommonGetPortIdentity(ptpMsgFollowUpGetBase(port->_currentFollowUp[i])) : NULL;
        if (followUpPort && (ptpPortIdentityEqualsTo(expectedPortIdent, followUpPort)) && (ptpMsgCommonGetSequenceId(ptpMsgFollowUpGetBase(port->_currentFollowUp[i])) == expectedSyncId))
            return port->_currentFollowUp[i];
    }

    return NULL;
}

void ptpNetPortSetCurrentDelayReq(NetworkPortRef port, MsgDelayReqRef msg)
{
    assert(port);
    ptpMutexLock(port->_processingLock);

    if (port->_currentDelayRequest[0]) {
        ptpRefMsgDelayReqRelease(port->_currentDelayRequest[0]);
    }
    ptpRefMsgDelayReqRetain(msg);

    int i;
    for (i = 0; i < PTP_MSG_HISTORY_LEN - 1; i++) {
        port->_currentDelayRequest[i] = port->_currentDelayRequest[i + 1];
    }

    port->_currentDelayRequest[PTP_MSG_HISTORY_LEN - 1] = msg;
    ptpMutexUnlock(port->_processingLock);
}

MsgDelayReqRef ptpNetPortGetCurrentDelayReq(NetworkPortRef port, const uint16_t expectedSyncId)
{
    assert(port);

    int i;
    for (i = 0; i < PTP_MSG_HISTORY_LEN; i++) {
        if (port->_currentDelayRequest[i] && (ptpMsgCommonGetSequenceId(ptpMsgDelayReqGetBase(port->_currentDelayRequest[i])) == expectedSyncId))
            return port->_currentDelayRequest[i];
    }

    return NULL;
}

PortIdentityRef ptpNetPortGetPortIdentity(NetworkPortRef port)
{
    assert(port);
    return &(port->_portIdentity);
}

PtpMutexRef ptpNetPortGetProcessingLock(NetworkPortRef port)
{
    assert(port);
    return port->_processingLock;
}

void ptpNetPortSetSyncCount(NetworkPortRef port, unsigned int cnt)
{
    assert(port);
    port->_syncCount = cnt;
}

// Increments sync count
void ptpNetPortIncSyncCount(NetworkPortRef port)
{
    assert(port);
    port->_syncCount++;
}

// Gets current sync count value
uint32_t ptpNetPortGetSyncCount(NetworkPortRef port)
{
    assert(port);
    return port->_syncCount;
}

// Increments announce sequence id and returns Next announce sequence id.
uint16_t ptpNetPortGetNextAnnounceSequenceId(NetworkPortRef port)
{
    assert(port);
    return port->_announceSequenceId++;
}

// Increments signal sequence id and returns Next signal sequence id.
uint16_t ptpNetPortGetNextSignalSequenceId(NetworkPortRef port)
{
    assert(port);
    return port->_signalSequenceId++;
}

// Increments sync sequence ID and returns Next synce sequence id.
uint16_t ptpNetPortGetNextSyncSequenceId(NetworkPortRef port)
{
    assert(port);
    return port->_syncSequenceId++;
}

// Increments Delay sequence ID and returns Next Delay sequence id.
uint16_t ptpNetPortGetNextDelayReqSequenceId(NetworkPortRef port)
{
    assert(port);
    return ++port->_delayReqSequenceId;
}

// Gets the lastGmTimeBaseIndicator
uint16_t ptpNetPortGetLastGmTimeBaseIndicator(NetworkPortRef port)
{
    assert(port);
    return port->_lastGmTimeBaseIndicator;
}

// Sets the lastGmTimeBaseIndicator
void ptpNetPortSetLastGmTimeBaseIndicator(NetworkPortRef port, uint16_t gmTimeBaseIndicator)
{
    assert(port);
    port->_lastGmTimeBaseIndicator = gmTimeBaseIndicator;
}

// Gets the evScheduledSyncTimeout value
signed char ptpNetPortGetSyncInterval(NetworkPortRef port)
{
    assert(port);
    return port->_logMeanSyncInterval;
}

// Sets the evScheduledSyncTimeout value
void ptpNetPortSetSyncInterval(NetworkPortRef port, signed char val)
{
    assert(port);
    port->_logMeanSyncInterval = val;
}

void ptpNetPortDumpUnicastSendNodes(NetworkPortRef port)
{
    assert(port);
    ptpMutexLock(port->_sendNodeListLock);

    ListLinkLayerAddressIterator it;
    for (it = ptpListLinkLayerAddressFirst(port->_unicastSendNodeList); it != NULL; it = ptpListLinkLayerAddressNext(it)) {

        char addrStr[ADDRESS_STRING_LENGTH] = { 0 };
        LinkLayerAddressRef address = ptpListLinkLayerAddressIteratorValue(it);
        ptpLinkLayerAddressString(address, addrStr, sizeof(addrStr));

        PTP_LOG_VERBOSE("UnicastSendNode: %s", addrStr);
    }

    ptpMutexUnlock(port->_sendNodeListLock);
}

void ptpNetPortAddUnicastSendNode(NetworkPortRef port, const char* addrStr)
{
    assert(port && addrStr);

#if TARGET_OS_BRIDGECO
    // get ip address of the main network interface
    struct in_addr itfIp;
    itfIp.s_addr = nets[0]->n_ipaddr;

    // convert it to string
    char* ip = inet_ntoa(itfIp);

    // do not add our own address to the list of unicast nodes!
    if (!stricmp(addrStr, ip)) {
        PTP_LOG_DEBUG("IP address %s matches address of our network interface. It won't be added to the list of unicast addresses!", addrStr);
    } else
#endif
    {
        LinkLayerAddressRef addr = ptpLinkLayerAddressCreate();
        ptpLinkLayerAddressCtorEx(addr, addrStr, 0);

        ptpMutexLock(port->_sendNodeListLock);
        ptpListLinkLayerAddressPushBack(port->_unicastSendNodeList, addr);
        ptpMutexUnlock(port->_sendNodeListLock);
        PTP_LOG_INFO("Node %s added to the list of unicast nodes", addrStr);
    }
}

void ptpNetPortDeleteUnicastSendNode(NetworkPortRef port, const char* addrStr)
{
    assert(port);

    if (!port) {
        return;
    }

    LinkLayerAddress addressToDelete;
    ptpLinkLayerAddressCtorEx(&addressToDelete, addrStr, 0);

    ptpMutexLock(port->_sendNodeListLock);
    ListLinkLayerAddressIterator it;
    for (it = ptpListLinkLayerAddressFirst(port->_unicastSendNodeList); it != NULL; it = ptpListLinkLayerAddressNext(it)) {

        LinkLayerAddressRef address = ptpListLinkLayerAddressIteratorValue(it);

        if (ptpLinkLayerAddressEqualsTo(address, &addressToDelete)) {
            PTP_LOG_INFO("Node %s removed from the list of unicast nodes", addrStr);
            ptpListLinkLayerAddressErase(port->_unicastSendNodeList, it);
            break;
        }
    }
    ptpMutexUnlock(port->_sendNodeListLock);
    ptpLinkLayerAddressDtor(&addressToDelete);
}

LastTimestampsRef ptpNetPortGetLastTimestamps(NetworkPortRef port)
{
    assert(port);
    return &(port->_lastTimestamps);
}

void ptpNetPortSetLastTimestamps(NetworkPortRef port, const LastTimestampsRef timestamps)
{
    assert(port && timestamps);
    memcpy(&(port->_lastTimestamps), timestamps, sizeof(port->_lastTimestamps));
}

NanosecondTs ptpNetPortGetLastLocalTime(NetworkPortRef port)
{
    assert(port);
    return port->_lastLocalTime;
}

void ptpNetPortSetLastLocaltime(NetworkPortRef port, NanosecondTs value)
{
    assert(port);
    port->_lastLocalTime = value;
}

void ptpNetPortSetLastRemoteTime(NetworkPortRef port, NanosecondTs value)
{
    assert(port);
    port->_lastRemoteTime = value;
}

NanosecondTs ptpNetPortGetLastRemoteTime(NetworkPortRef port)
{
    assert(port);
    return port->_lastRemoteTime;
}

int ptpNetPortGetBadDiffCount(NetworkPortRef port)
{
    assert(port);
    return port->_badDiffCount;
}

void ptpNetPortSetBadDiffCount(NetworkPortRef port, int value)
{
    assert(port);
    port->_badDiffCount = value;
}

int ptpNetPortGetGoodSyncCount(NetworkPortRef port)
{
    assert(port);
    return port->_goodSyncCount;
}

void ptpNetPortSetGoodSyncCount(NetworkPortRef port, int value)
{
    assert(port);
    port->_goodSyncCount = value;
}

int ptpNetPortGetMaxBadDiffCount(NetworkPortRef port)
{
    assert(port);
    return port->_maxBadDiffCount;
}

void ptpNetPortSetMaxBadDiffCount(NetworkPortRef port, int value)
{
    assert(port);
    port->_maxBadDiffCount = value;
}

int ptpNetPortGetSuccessDiffCount(NetworkPortRef port)
{
    assert(port);
    return port->_successDiffCount;
}

void ptpNetPortSetSuccessDiffCount(NetworkPortRef port, int value)
{
    assert(port);
    port->_successDiffCount = value;
}

signed char ptpNetPortGetAnnounceInterval(NetworkPortRef port)
{
    assert(port);
    return port->_logMeanAnnounceInterval;
}

void ptpNetPortSetAnnounceInterval(NetworkPortRef port, signed char val)
{
    assert(port);
    port->_logMeanAnnounceInterval = val;
}

void ptpNetPortSetLinkSpeed(NetworkPortRef port, uint32_t linkSpeed)
{
    assert(port);

    ptpMutexLock(port->_sendNodeListLock);
    port->_linkSpeed = linkSpeed;

    // retrieve tx delay for given link speed
    port->_txDelay = ptpNetPortGetTxPhyDelay(port, linkSpeed);

    // retrieve rx delay for given link speed
    port->_rxDelay = ptpNetPortGetRxPhyDelay(port, linkSpeed);
    ptpMutexUnlock(port->_sendNodeListLock);
}

uint32_t ptpNetPortGetLinkSpeed(NetworkPortRef port)
{
    assert(port);
    return port->_linkSpeed;
}

Boolean ptpNetPortLockTxLock(NetworkPortRef port)
{
    assert(port);
    return ptpMutexLock(port->_portTxLock) == eMutexOk ? true : false;
}

Boolean ptpNetPortUnlockTxLock(NetworkPortRef port)
{
    assert(port);
    return ptpMutexUnlock(port->_portTxLock) == eMutexOk ? true : false;
}
