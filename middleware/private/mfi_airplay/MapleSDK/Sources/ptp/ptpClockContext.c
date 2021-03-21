/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpClockContext.h"
#include "Common/ptpLog.h"
#include "Common/ptpMutex.h"
#include "Common/ptpPlatform.h"
#include "Common/ptpTimerQueue.h"
#include "Common/ptpUtils.h"
#include "Messages/ptpMsgAnnounce.h"
#include "ptpNetworkPort.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// Scheduled event object
//

typedef struct {

    /**
	 * @brief owner of the event descriptor
	 */
    ClockCtxRef _owner;

    /**
	 * @brief linked port object
	 */
    NetworkPortRef _port;

    /**
	 * @brief event type
	 */
    PtpEvent _event;

} EventDescriptor;

void ptpEventDescriptorCtor(void* ptr);
void ptpEventDescriptorDtor(void* ptr);

// EventDescriptor object constructor
void ptpEventDescriptorCtor(void* ptr)
{
    EventDescriptor* ctx = (EventDescriptor*)ptr;
    assert(ptr);

    ctx->_owner = NULL;
    ctx->_port = NULL;
    ctx->_event = evNoEvent;
}

// EventDescriptor object destructor
void ptpEventDescriptorDtor(void* ptr)
{
    if (!ptr)
        return;

    // not used at the moment
}

DECLARE_MEMPOOL(EventDescriptor)
DEFINE_MEMPOOL(EventDescriptor, ptpEventDescriptorCtor, ptpEventDescriptorDtor)

struct ClockCtx {

    NetworkPortRef _port;
    ClockIdentity _clockIdentity;
    ClockQuality _clockQuality;
    ClockIdentity _grandmasterClockIdentity;
    ClockQuality _grandmasterClockQuality;
    Timestamp _oneWayDelay;
    NanosecondTs _prevMasterTime;
    NanosecondTs _prevSyncArrivalTime;
    NanosecondTs _masterClockOffset;
    int _remoteClockOffsetMeasurements;
    NanosecondTs _remoteClockOffsetHistory[REMOTE_CLOCK_FILTER_SIZE];

    TimerQueueRef _timerQueue;

    /**
	 * @brief transmitted FollowUp TLV object
	 */
    FollowUpTlvRef _followUpTx;

    /**
	 * @brief received FollowUp TLV object
	 */
    FollowUpTlvRef _followUpRx;
    PtpMutexRef _clockIdentityMutex;

    int16_t _currentUtcOffset;
    Boolean _localMasterFreqOffsetInit;
    uint64_t _instanceId;
    uint8_t _priority1;
    uint8_t _priority2;
    uint16_t _stepsRemoved;
    uint8_t _domainNumber;
    uint8_t _grandmasterPriority1;
    uint8_t _grandmasterPriority2;
    uint8_t _timeSource;
};

// imported ptpInterface API
extern void ptpReset(uint64_t instanceId, uint64_t masterClockId);
extern void ptpUpdate(uint64_t instanceId, PtpTimeDataRef data);

ClockCtxRef ptpClockCtxCreate(void)
{
    ClockCtxRef clock = (ClockCtxRef)malloc(sizeof(struct ClockCtx));

    if (!clock)
        return NULL;

    return clock;
}

void ptpClockCtxDestroy(ClockCtxRef clock)
{
    if (!clock)
        return;

    ptpClockCtxDtor(clock);
    free(clock);
}

void ptpClockCtxCtor(ClockCtxRef clock, uint64_t instanceId, uint8_t priority1, uint8_t priority2)
{
    assert(clock);

    clock->_instanceId = instanceId;
    clock->_priority1 = priority1;
    clock->_priority2 = priority2;
    clock->_stepsRemoved = 0;
    clock->_port = NULL;
    clock->_clockQuality._accuracy = 254;
    clock->_clockQuality._class = 248;
    clock->_clockQuality._offsetScaledLogVariance = 0x436A;
    clock->_timeSource = 160;
    clock->_domainNumber = 0;
    clock->_currentUtcOffset = 0;
    clock->_localMasterFreqOffsetInit = false;
    clock->_masterClockOffset = 0;

    clock->_remoteClockOffsetMeasurements = 0;
    memset(clock->_remoteClockOffsetHistory, 0, sizeof(clock->_remoteClockOffsetHistory));

    ptpClockIdentityCtor(&(clock->_grandmasterClockIdentity));

    clock->_clockIdentityMutex = ptpMutexCreate(NULL);

    // This should be done LAST!! to pass fully initialized clock object
    clock->_timerQueue = ptpTimerQueueCreate();
    clock->_followUpTx = ptpRefFollowUpTlvAlloc();
    clock->_followUpRx = ptpRefFollowUpTlvAlloc();
}

void ptpClockCtxDtor(ClockCtxRef clock)
{
    if (!clock)
        return;

    if (clock->_timerQueue)
        ptpTimerQueueDestroy(clock->_timerQueue);

    ptpRefFollowUpTlvRelease(clock->_followUpTx);
    ptpRefFollowUpTlvRelease(clock->_followUpRx);

    ptpMutexDestroy(clock->_clockIdentityMutex);
}

void ptpClockCtxDeinitialize(ClockCtxRef clock)
{
    assert(clock);

    if (clock->_timerQueue) {
        ptpTimerQueueDeinitialize(clock->_timerQueue);
    }
}

void ptpClockCtxGetTime(TimestampRef timestamp)
{
    struct timespec ts;
    assert(timestamp);

    clock_gettime(CLOCK_PTP_GET_TIME, &ts);
    ptpTimestampInitFromTsCtor(timestamp, &ts);
}

static void ptpClockCtxTimerQueueHandler(void* arg)
{
    EventDescriptor* eventDescriptor = (EventDescriptor*)arg;

    if (eventDescriptor) {
        ptpNetPortProcessEvent(eventDescriptor->_port, eventDescriptor->_event);
    }
}

static void ptpClockCtxTimerQueueDestroyEvent(void* arg)
{
    EventDescriptor* eventDescriptor = (EventDescriptor*)arg;

    if (eventDescriptor) {
        ptpMemPoolEventDescriptorDeallocate(eventDescriptor);
    }
}

static void ptpClockCtxAddEventTimer(ClockCtxRef clock, NetworkPortRef port, PtpEvent e, uint64_t time_ns)
{
    assert(clock && port);

    EventDescriptor* eventDescriptor = ptpMemPoolEventDescriptorAllocate();

    eventDescriptor->_owner = clock;
    eventDescriptor->_event = e;
    eventDescriptor->_port = port;

    ptpTimerQueueAddEvent(clock->_timerQueue, (unsigned)(time_ns / 1000), (int)e, &ptpClockCtxTimerQueueHandler, &ptpClockCtxTimerQueueDestroyEvent, eventDescriptor);
}

void ptpClockCtxScheduleEvent(ClockCtxRef clock, NetworkPortRef port, PtpEvent e, uint64_t time_ns)
{
    assert(clock && port);
    ptpClockCtxAddEventTimer(clock, port, e, time_ns);
}

Boolean ptpClockCtxIsEventScheduled(ClockCtxRef clock, PtpEvent event)
{
    assert(clock && port);
    return ptpTimerQueueIsEventScheduled(clock->_timerQueue, (int)event);
}

void ptpClockCtxUnscheduledEvent(ClockCtxRef clock, PtpEvent event)
{
    assert(clock);
    ptpTimerQueueCancelEvent(clock->_timerQueue, (int)event);
}

double ptpClockCtxCalcmasterToLocalRatio(ClockCtxRef clock, const NanosecondTs masterTime, const NanosecondTs syncArrivalTime)
{
    NanosecondTs deltaSyncTime;
    NanosecondTs deltaMasterTime;
    NanosecondTs freqRatio;

    assert(clock);

    // if this is our first measurement, keep current values
    if (!clock->_localMasterFreqOffsetInit) {

        clock->_prevSyncArrivalTime = syncArrivalTime;
        clock->_prevMasterTime = masterTime;
        clock->_localMasterFreqOffsetInit = true;
        return 1.0;
    }

    // delta between two consecutive sync times
    deltaSyncTime = syncArrivalTime - clock->_prevSyncArrivalTime;

    // delta between two consecutive master times
    deltaMasterTime = masterTime - clock->_prevMasterTime;

    // calculate ratio between master time (remote) and sync time (local)
    if (deltaSyncTime != 0.0) {
        freqRatio = ((NanosecondTs)deltaMasterTime) / deltaSyncTime;
    } else {
        freqRatio = 1.0;
    }

    // if we got negative time jump:
    if (masterTime < clock->_prevMasterTime) {

        PTP_LOG_STATUS("Negative time jump detected - inter_master_time: %lld, inter_sync_time: %lld, incorrect ppt_offset: %lf", deltaMasterTime, deltaSyncTime, freqRatio);

        // we will recalculate it next time
        clock->_localMasterFreqOffsetInit = false;
        return NEGATIVE_TIME_JUMP;
    }

    // keep new values
    clock->_prevSyncArrivalTime = syncArrivalTime;
    clock->_prevMasterTime = masterTime;
    return freqRatio;
}

NanosecondTs ptpClockCtxMasterOffset(ClockCtxRef clock)
{
    assert(clock);
    return clock->_masterClockOffset;
}

// filter received master/local clock offset and return filtered value
static NanosecondTs ptpClockCtxProcessMasterClockOffset(ClockCtxRef clock, const NanosecondTs masterClockOffset, Boolean* offsetValid)
{
    int i;
    assert(clock && offsetValid);

    // by default, returned offset is not valid, until we don't have enoough measurements
    *offsetValid = false;

    // move history buffer up
    for (i = REMOTE_CLOCK_FILTER_SIZE - 1; i > 0; i--) {
        clock->_remoteClockOffsetHistory[i] = clock->_remoteClockOffsetHistory[i - 1];
    }

    // store current value
    clock->_remoteClockOffsetHistory[0] = masterClockOffset;

    // count the number of previous measurements
    clock->_remoteClockOffsetMeasurements++;

    if (clock->_remoteClockOffsetMeasurements >= REMOTE_CLOCK_FILTER_SIZE) {
        clock->_remoteClockOffsetMeasurements = REMOTE_CLOCK_FILTER_SIZE;

        // we have enough measurements. The returned offset is valid
        *offsetValid = true;
    }

    // create a copy of measurement history
    NanosecondTs sortedRemoteClockOffsetHistory[REMOTE_CLOCK_FILTER_SIZE];
    memcpy(sortedRemoteClockOffsetHistory, clock->_remoteClockOffsetHistory, sizeof(clock->_remoteClockOffsetHistory));

    // sort the array
    quickSortDouble(sortedRemoteClockOffsetHistory, 0, clock->_remoteClockOffsetMeasurements);

    // and pick the median value
    clock->_masterClockOffset = sortedRemoteClockOffsetHistory[clock->_remoteClockOffsetMeasurements / 2];

    if (*offsetValid == false){
        PTP_LOG_INFO("Remote clock offset measured [%d] masterClockOffset [%lf] ns", clock->_remoteClockOffsetMeasurements, NANOSECONDS_TO_MILLISECONDS(clock->_masterClockOffset));
    }

    // if we have more than 3 measurements and not valid offset flag...
    if (!(*offsetValid) && (clock->_remoteClockOffsetMeasurements >= REMOTE_CLOCK_SYNC_MIN_COUNT)) {

        // compare upper and lower measurement around median value (as we have 3 measurements at least,
        // these value do exist)
        NanosecondTs lowerOffset = sortedRemoteClockOffsetHistory[clock->_remoteClockOffsetMeasurements / 2 - 1];
        NanosecondTs upperOffset = sortedRemoteClockOffsetHistory[clock->_remoteClockOffsetMeasurements / 2 + 1];

        // if the difference between lower and upper value is less than 1ms, we can accept the offset
        NanosecondTs lowerDiff = clock->_masterClockOffset - lowerOffset;
        NanosecondTs upperDiff = upperOffset - clock->_masterClockOffset;

        if ((lowerDiff <= REMOTE_CLOCK_DIFF_TOLERANCE) && (upperDiff <= REMOTE_CLOCK_DIFF_TOLERANCE)) {
            PTP_LOG_VERBOSE("Lower and upper diff is below %lf ms, offset is valid", REMOTE_CLOCK_DIFF_TOLERANCE / 1000000.0);
            *offsetValid = true;
        } else {
            PTP_LOG_ERROR("Lower diff: %lf ms, upper diff: %lf ms (offset is not valid)", NANOSECONDS_TO_MILLISECONDS(lowerDiff), NANOSECONDS_TO_MILLISECONDS(upperDiff));
        }
    }

    // return median offset
    return clock->_masterClockOffset;
}

void ptpClockCtxSyncComplete(ClockCtxRef clock, NanosecondTs masterToLocalPhase, NanosecondTs masterToLocalRatio, NanosecondTs syncArrivalTime, unsigned syncCount, PortState portState, uint64_t masterClockId)
{
    PtpTimeData newCtx;
    assert(clock);

    // process master clock offset and get the filtered value back
    Boolean offsetValid = false;
    masterToLocalPhase = ptpClockCtxProcessMasterClockOffset(clock, masterToLocalPhase, &offsetValid);

    // update master offset (only if offset is valid)
    if (offsetValid) {

        ptpClockIdentityGetBytes(&(clock->_clockIdentity), newCtx._localClockId);
        newCtx._portNumber = ptpPortIdentityGetPortNumber(ptpNetPortGetPortIdentity(clock->_port));
        newCtx._grandmasterDomainNumber = clock->_domainNumber;
        newCtx._priority1 = clock->_priority1;
        newCtx._priority2 = clock->_priority2;
        newCtx._clockClass = clock->_clockQuality._class;
        newCtx._clockAccuracy = clock->_clockQuality._accuracy;
        newCtx._offsetScaledLogVariance = clock->_clockQuality._offsetScaledLogVariance;
        newCtx._domainNumber = clock->_domainNumber;

        newCtx._logSyncInterval = ptpNetPortGetSyncInterval(clock->_port);
        newCtx._logAnnounceInterval = ptpNetPortGetAnnounceInterval(clock->_port);

        newCtx._masterClockId = masterClockId;
        newCtx._syncArrivalTime = syncArrivalTime;
        newCtx._masterToLocalPhase = masterToLocalPhase;
        newCtx._masterToLocalRatio = masterToLocalRatio;
        newCtx._syncCount = syncCount;
        newCtx._portState = portState;

        // report new PTP state
        ptpUpdate(clock->_instanceId, &newCtx);
    } else {
        PTP_LOG_ERROR("%s: offset is invalid for gm %016llx", __func__, masterClockId);
    }
}

void ptpClockCtxAssignClockIdentity(ClockCtxRef clock, const MacAddressRef addr)
{
    assert(clock);
    ptpClockIdentityAssignAddress(&(clock->_clockIdentity), addr);
}

ClockIdentityRef ptpClockCtxGetGrandmasterClockIdentity(ClockCtxRef clock)
{
    assert(clock);
    return &(clock->_grandmasterClockIdentity);
}

void ptpClockCtxAssignGrandmasterClockIdentity(ClockCtxRef clock, ClockIdentityRef id)
{
    assert(clock);

    if (ptpClockIdentityNotEqualTo(id, &(clock->_grandmasterClockIdentity))) {
        PTP_LOG_STATUS("New Grandmaster \"%016llx\" (previous \"%016llx\")", ptpClockIdentityGetUInt64(id), ptpClockIdentityGetUInt64(&(clock->_grandmasterClockIdentity)));
        ptpClockIdentityAssign(&(clock->_grandmasterClockIdentity), id);
    }
}

ClockQualityRef ptpClockCtxGetGrandmasterClockQuality(ClockCtxRef clock)
{
    assert(clock);
    return &(clock->_grandmasterClockQuality);
}

void ptpClockCtxAssignGrandmasterClockQuality(ClockCtxRef clock, ClockQualityRef clockQuality)
{
    assert(clock);
    ptpClockQualityAssign(&(clock->_grandmasterClockQuality), clockQuality);
}

ClockQualityRef ptpClockCtxGetClockQuality(ClockCtxRef clock)
{
    assert(clock);
    return &(clock->_clockQuality);
}

unsigned char ptpClockCtxGetGrandmasterPriority1(ClockCtxRef clock)
{
    assert(clock);
    return clock->_grandmasterPriority1;
}

unsigned char ptpClockCtxGetGrandmasterPriority2(ClockCtxRef clock)
{
    assert(clock);
    return clock->_grandmasterPriority2;
}

uint16_t ptpClockCtxGetGrandmasterStepsRemoved(ClockCtxRef clock)
{
    assert(clock);
    return clock->_stepsRemoved;
}

void ptpClockCtxSetGrandmasterPriority1(ClockCtxRef clock, unsigned char priority1)
{
    assert(clock);
    clock->_grandmasterPriority1 = priority1;
}

void ptpClockCtxSetGrandmasterPriority2(ClockCtxRef clock, unsigned char priority2)
{
    assert(clock);
    clock->_grandmasterPriority2 = priority2;
}

void ptpClockCtxSetGrandmasterStepsRemoved(ClockCtxRef clock, uint16_t stepsRemoved)
{
    assert(clock);
    clock->_stepsRemoved = stepsRemoved;
}

int16_t ptpClockCtxGetCurrentUtcOffset(ClockCtxRef clock)
{
    assert(clock);
    return clock->_currentUtcOffset;
}

uint8_t ptpClockCtxGetTimeSource(ClockCtxRef clock)
{
    assert(clock);
    return clock->_timeSource;
}

unsigned char ptpClockCtxGetPriority1(ClockCtxRef clock)
{
    assert(clock);
    return clock->_priority1;
}

void ptpClockCtxSetPriority1(ClockCtxRef clock, unsigned char value)
{
    assert(clock);
    clock->_priority1 = value;
}

unsigned char ptpClockCtxGetPriority2(ClockCtxRef clock)
{
    assert(clock);
    return clock->_priority2;
}

FollowUpTlvRef ptpClockCtxGetFollowUpTlvTx(ClockCtxRef clock)
{
    assert(clock);
    return clock->_followUpTx;
}

FollowUpTlvRef ptpClockCtxGetFollowUTlvRx(ClockCtxRef clock)
{
    assert(clock);
    return clock->_followUpRx;
}

void ptpClockCtxUpdateFollowUpTx(ClockCtxRef clock)
{
    assert(clock);

    // clear history of remote clock measurements
    clock->_remoteClockOffsetMeasurements = 0;
    memset(clock->_remoteClockOffsetHistory, 0, sizeof(clock->_remoteClockOffsetHistory));

    ptpFollowUpTlvIncrementGMTimeBaseIndicator(clock->_followUpTx);
    ptpFollowUpTlvSetScaledLastGmFreqChange(clock->_followUpTx, ptpFollowUpTlvGetScaledLastGmFreqChange(clock->_followUpRx));
    ptpFollowUpTlvAssignScaledLastGmPhaseChange(clock->_followUpTx, ptpFollowUpTlvGetScaledLastGmPhaseChange(clock->_followUpRx));
}

void ptpClockCtxSetPort(ClockCtxRef clock, NetworkPortRef port)
{
    assert(clock);

    if (clock->_port != port) {
        clock->_port = port;
    }
}

ClockIdentityRef ptpClockCtxGetClockIdentity(ClockCtxRef clock)
{
    assert(clock);
    return &(clock->_clockIdentity);
}

void ptpClockCtxIpcReset(ClockCtxRef clock, uint64_t masterClockId)
{
    (void)clock;

    assert(clock);
    ptpReset(clock->_instanceId, masterClockId);
}

ClockDatasetRef ptpClockCtxDefaultDataset(ClockCtxRef clock)
{
    assert(clock);

    ClockDatasetRef ds = ptpClockDatasetCreate();
    ds->_priority1 = clock->_priority1;
    ds->_priority2 = clock->_priority2;
    ds->_stepsRemoved = clock->_stepsRemoved;
    ptpClockIdentityAssign(&(ds->_identity), &(clock->_clockIdentity));
    ptpClockQualityAssign(&(ds->_quality), &(clock->_clockQuality));

    ptpClockIdentityAssign(ptpPortIdentityGetClockIdentity(&(ds->_sender)), &(clock->_clockIdentity));
    ptpPortIdentitySetPortNumber(&(ds->_sender), 0);

    ptpClockIdentityAssign(ptpPortIdentityGetClockIdentity(&(ds->_receiver)), &(clock->_clockIdentity));
    ptpPortIdentitySetPortNumber(&(ds->_receiver), 0);
    return ds;
}
