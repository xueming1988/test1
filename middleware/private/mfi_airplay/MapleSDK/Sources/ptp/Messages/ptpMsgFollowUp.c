/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpMsgFollowUp.h"
#include "Common/ptpMemoryPool.h"
#include "ptpClockContext.h"
#include "ptpClockQuality.h"
#include "ptpMsgCommon.h"
#include "ptpNetworkPort.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void* ptpRefMsgFollowUpAllocateMemFromPool(size_t size);
void ptpRefMsgFollowUpDeallocateMemFromPool(void* ptr);

//
// scaledNs
//

void ptpScaledNsCtor(ScaledNsRef ns)
{
    assert(ns);
    ns->_nanosecondsMsb = 0;
    ns->_nanosecondsLsb = 0;
    ns->_fractionalNanoseconds = 0;
}

void ptpScaledNsDtor(ScaledNsRef ns)
{
    if (!ns)
        return;

    // empty
}

// Overloads the operator = for this class
void ptpScaledNsAssign(ScaledNsRef ns, const ScaledNsRef other)
{
    assert(ns && other);
    ns->_nanosecondsMsb = other->_nanosecondsMsb;
    ns->_nanosecondsLsb = other->_nanosecondsLsb;
    ns->_fractionalNanoseconds = other->_fractionalNanoseconds;
}

void ptpScaledNsFromNetworkOrder(ScaledNsRef ns)
{
    assert(ns);

    ns->_nanosecondsMsb = ptpNtohsu(ns->_nanosecondsMsb);
    ns->_nanosecondsLsb = ptpNtohllu(ns->_nanosecondsLsb);
    ns->_fractionalNanoseconds = ptpNtohsu(ns->_fractionalNanoseconds);
}

void ptpScaledNsToNetworkOrder(ScaledNsRef ns)
{
    assert(ns);

    ns->_nanosecondsMsb = ptpHtonsu(ns->_nanosecondsMsb);
    ns->_nanosecondsLsb = ptpHtonllu(ns->_nanosecondsLsb);
    ns->_fractionalNanoseconds = ptpHtonsu(ns->_fractionalNanoseconds);
}

// Set the lowest 64bits from the scaledNs object
void ptpScaledNsSetNanosecondsLsb(ScaledNsRef ns, uint64_t nanosecondsLsb)
{
    assert(ns);
    ns->_nanosecondsLsb = nanosecondsLsb;
}

// Set the highest 32bits of the scaledNs object
void ptpScaledNsSetNanosecondsMsb(ScaledNsRef ns, uint16_t nanosecondsMsb)
{
    assert(ns);
    ns->_nanosecondsMsb = nanosecondsMsb;
}

// Set the fractional nanoseconds
void ptpScaledNsSetFractionalNs(ScaledNsRef ns, uint16_t fractionalNs)
{
    assert(ns);
    ns->_fractionalNanoseconds = fractionalNs;
}

//
// FollowUpTlv
//

DEFINE_REFCOUNTED_OBJECT(FollowUpTlv, NULL, NULL, ptpFollowUpTlvCtor, ptpFollowUpTlvDtor)

// Builds the FollowUpTLV interface
void ptpFollowUpTlvCtor(FollowUpTlvRef tlv)
{
    assert(tlv);

    tlv->_tlvType = 0x3;
    tlv->_lengthField = 28;
    tlv->_organizationId[0] = (uint8_t)'\x00';
    tlv->_organizationId[1] = (uint8_t)'\x80';
    tlv->_organizationId[2] = (uint8_t)'\xC2';
    tlv->_organizationSubTypeMs = 0;
    tlv->_organizationSubTypeLs = 1;
    tlv->_cumulativeScaledRateOffset = 0;
    tlv->_gmTimeBaseIndicator = 0;
    tlv->_scaledLastGmFreqChange = 0;
}

void ptpFollowUpTlvDtor(FollowUpTlvRef tlv)
{
    if (!tlv)
        return;

    // empty
}

void ptpFollowUpTlvAssign(FollowUpTlvRef tlv, const FollowUpTlvRef other)
{
    assert(tlv && other);
    memcpy(tlv, other, sizeof(*tlv));
}

// convert FollowUpTlv to/from network order
void ptpFollowUpTlvFromNetworkOrder(FollowUpTlvRef tlv)
{
    assert(tlv);

    tlv->_tlvType = ptpNtohsu(tlv->_tlvType);
    tlv->_lengthField = ptpNtohsu(tlv->_lengthField);
    tlv->_organizationSubTypeLs = ptpNtohsu(tlv->_organizationSubTypeLs);
    tlv->_cumulativeScaledRateOffset = ptpNtohls(tlv->_cumulativeScaledRateOffset);
    tlv->_gmTimeBaseIndicator = ptpNtohsu(tlv->_gmTimeBaseIndicator);
    tlv->_scaledLastGmFreqChange = ptpNtohls(tlv->_scaledLastGmFreqChange);

    ptpScaledNsFromNetworkOrder(&(tlv->_scaledLastGmPhaseChange));
}

void ptpFollowUpTlvToNetworkOrder(FollowUpTlvRef tlv)
{
    assert(tlv);

    tlv->_tlvType = ptpHtonsu(tlv->_tlvType);
    tlv->_lengthField = ptpHtonsu(tlv->_lengthField);
    tlv->_organizationSubTypeLs = ptpHtonsu(tlv->_organizationSubTypeLs);
    tlv->_cumulativeScaledRateOffset = ptpHtonls(tlv->_cumulativeScaledRateOffset);
    tlv->_gmTimeBaseIndicator = ptpHtonsu(tlv->_gmTimeBaseIndicator);
    tlv->_scaledLastGmFreqChange = ptpHtonls(tlv->_scaledLastGmFreqChange);

    ptpScaledNsToNetworkOrder(&(tlv->_scaledLastGmPhaseChange));
}

// Gets the cummulative scaledRateOffset
int32_t ptpFollowUpTlvGetRateOffset(FollowUpTlvRef tlv)
{
    assert(tlv);
    return tlv->_cumulativeScaledRateOffset;
}

// Gets the gmTimeBaseIndicator
uint16_t ptpFollowUpTlvGetGmTimeBaseIndicator(FollowUpTlvRef tlv)
{
    assert(tlv);
    return tlv->_gmTimeBaseIndicator;
}

// Updates the scaledLastGmFreqChanged private member
void ptpFollowUpTlvSetScaledLastGmFreqChange(FollowUpTlvRef tlv, int32_t val)
{
    assert(tlv);
    tlv->_scaledLastGmFreqChange = ptpHtonls(val);
}

// Gets the current scaledLastGmFreqChanged value
int32_t ptpFollowUpTlvGetScaledLastGmFreqChange(FollowUpTlvRef tlv)
{
    assert(tlv);
    return tlv->_scaledLastGmFreqChange;
}

// Sets the gmTimeBaseIndicator private member
void ptpFollowUpTlvSetGMTimeBaseIndicator(FollowUpTlvRef tlv, uint16_t tbi)
{
    assert(tlv);
    tlv->_gmTimeBaseIndicator = tbi;
}

// Incremets the Time Base Indicator member
void ptpFollowUpTlvIncrementGMTimeBaseIndicator(FollowUpTlvRef tlv)
{
    assert(tlv);
    tlv->_gmTimeBaseIndicator++;
}

// Gets the current gmTimeBaseIndicator value
uint16_t ptpFollowUpTlvGetGMTimeBaseIndicator(FollowUpTlvRef tlv)
{
    assert(tlv);
    return tlv->_gmTimeBaseIndicator;
}

// Sets the scaledLastGmPhaseChange private member
void ptpFollowUpTlvAssignScaledLastGmPhaseChange(FollowUpTlvRef tlv, ScaledNsRef pc)
{
    assert(tlv);
    ptpScaledNsAssign(&(tlv->_scaledLastGmPhaseChange), pc);
}

// Gets the scaledLastGmPhaseChange private member value
ScaledNsRef ptpFollowUpTlvGetScaledLastGmPhaseChange(FollowUpTlvRef tlv)
{
    assert(tlv);
    return &(tlv->_scaledLastGmPhaseChange);
}

//
// MsgFollowUp
//

#define BASE_CLASS_REF(ptr) (&(ptr->_base))

static MemoryPoolRef _ptpMsgFollowUpMemPool = NULL;

// callback to allocate required amount of memory
void* ptpRefMsgFollowUpAllocateMemFromPool(size_t size)
{
    // create memory pool object
    ptpMemPoolCreateOnce(&_ptpMsgFollowUpMemPool, size, NULL, NULL);

    // allocate one element from the memory pool
    return ptpMemPoolAllocate(_ptpMsgFollowUpMemPool);
}

// callback to release memory allocated via ptpRefAllocateMem callback
void ptpRefMsgFollowUpDeallocateMemFromPool(void* ptr)
{
    assert(_ptpMsgFollowUpMemPool);

    // release one element to the memory pool
    if (ptr)
        ptpMemPoolDeallocate(_ptpMsgFollowUpMemPool, ptr);
}

static void ptpMsgFollowUpCtor(MsgFollowUpRef msg);
DEFINE_REFCOUNTED_OBJECT(MsgFollowUp, ptpRefMsgFollowUpAllocateMemFromPool, ptpRefMsgFollowUpDeallocateMemFromPool, ptpMsgFollowUpCtor, ptpMsgFollowUpDtor)

static void ptpMsgFollowUpCtor(MsgFollowUpRef msg)
{
    assert(msg);

    ptpMsgCommonCtor(BASE_CLASS_REF(msg));
    BASE_CLASS_REF(msg)->_messageType = PTP_FOLLOWUP_MESSAGE;
    BASE_CLASS_REF(msg)->_control = FOLLOWUP;
    BASE_CLASS_REF(msg)->_messageLength = sizeof(MsgFollowUp);
}

// Default constructor. Creates PTPMessageSync from given payload
void ptpMsgFollowUpPayloadCtor(MsgFollowUpRef msg, const uint8_t* payload, size_t payloadSize)
{
    assert(msg && payload);

    if (payloadSize < sizeof(MsgFollowUp))
        return;

    memcpy(msg, payload, sizeof(MsgFollowUp));
    ptpMsgCommonPayloadCtor(BASE_CLASS_REF(msg), payload, payloadSize);
    ptpTimestampFromNetworkOrder(&(msg->_preciseOriginTimestamp));
    ptpFollowUpTlvFromNetworkOrder(&(msg->_tlv));
}

// Default constructor. Creates PTPMessageSync
void ptpMsgFollowUpPortCtor(MsgFollowUpRef msg, NetworkPortRef port)
{
    assert(msg && port);
    ptpMsgFollowUpCtor(msg);

    BASE_CLASS_REF(msg)->_messageType = PTP_FOLLOWUP_MESSAGE;
    BASE_CLASS_REF(msg)->_control = FOLLOWUP;
    BASE_CLASS_REF(msg)->_logMeanMsgInterval = ptpNetPortGetSyncInterval(port);
    BASE_CLASS_REF(msg)->_messageLength = sizeof(MsgFollowUp);

    ptpTimestampCtor(&(msg->_preciseOriginTimestamp));
    ptpFollowUpTlvCtor(&(msg->_tlv));
}

// Destroys the PTPMessageAnnounce interface
void ptpMsgFollowUpDtor(MsgFollowUpRef msg)
{
    if (!msg)
        return;

    ptpTimestampDtor(&(msg->_preciseOriginTimestamp));
    ptpFollowUpTlvDtor(&(msg->_tlv));
    ptpMsgCommonDtor(BASE_CLASS_REF(msg));
}

// Get base class object reference
MsgCommonRef ptpMsgFollowUpGetBase(MsgFollowUpRef msg)
{
    assert(msg);
    return BASE_CLASS_REF(msg);
}

void ptpMsgFollowUpAssign(MsgFollowUpRef msg, const MsgFollowUpRef other)
{
    assert(msg && other);

    if (msg != other) {
        ptpMsgCommonAssign(BASE_CLASS_REF(msg), BASE_CLASS_REF(other));
        ptpTimestampAssign(&(msg->_preciseOriginTimestamp), &(other->_preciseOriginTimestamp));
        ptpFollowUpTlvAssign(&(msg->_tlv), &(other->_tlv));
    }
}

// Assembles PTPMessageFollowUp message on the EtherPort payload
Boolean ptpMsgFollowUpSendPort(MsgFollowUpRef msg, NetworkPortRef port, PortIdentityRef destIdentity)
{
    assert(msg && port);

    // Compute correction field.
    // _preciseOriginTimestamp is set to a time when matching sync request was sent, now we will calculate
    // a delay between this timestamp and the current time (when we know for sure that this packet has been sent)
    // it would be better to take this time from the network driver but for sake of simplicity, this is not yet implemented
    Timestamp egressTime;
    ptpClockCtxGetTime(&egressTime);
    double residenceTime = TIMESTAMP_TO_NANOSECONDS(&egressTime) - TIMESTAMP_TO_NANOSECONDS(&(msg->_preciseOriginTimestamp));
    BASE_CLASS_REF(msg)->_correctionField = ((int64_t)residenceTime) << 16;

    // output message object
    MsgFollowUp outMsg;
    ptpMsgFollowUpAssign(&outMsg, msg);
    ptpMsgCommonBuildHeader(BASE_CLASS_REF(msg), (uint8_t*)&outMsg);
    ptpTimestampToNetworkOrder(&(outMsg._preciseOriginTimestamp));
    ptpFollowUpTlvToNetworkOrder(&(outMsg._tlv));

    PTP_LOG_VERBOSE("Follow-Up Time: %lf s", TIMESTAMP_TO_SECONDS(&(msg->_preciseOriginTimestamp)));

    Timestamp timestamp;
    ptpNetPortSend(port, (uint8_t*)&outMsg, sizeof(outMsg), destIdentity, &timestamp, false, true);

    char addrStr[ADDRESS_STRING_LENGTH] = { 0 };
    PTP_LOG_DEBUG("==================================================================");
    PTP_LOG_STATUS("Sent FollowUp message (seq = %d, tx time = %lf s) to %s", BASE_CLASS_REF(msg)->_sequenceId, TIMESTAMP_TO_SECONDS(&timestamp), ptpNetPortIdentityToAddressStr(port, destIdentity, addrStr, sizeof(addrStr)));
    PTP_LOG_DEBUG("==================================================================");

    return true;
}

// Processes PTP messages
void ptpMsgFollowUpProcess(MsgFollowUpRef msg, NetworkPortRef port)
{
    assert(msg);

    if (ptpPortIdentityEqualsTo(ptpNetPortGetPortIdentity(port), ptpMsgCommonGetPortIdentity(BASE_CLASS_REF(msg)))) {

        PTP_LOG_VERBOSE("Received Follow Up from self...ignoring it.");
        // Ignore messages from self
        return;
    }

    PTP_LOG_VERBOSE("Processing a follow-up message");
    PTP_LOG_VERBOSE("Followup clock id:%016llx (%d)", ptpClockIdentityGetUInt64(ptpPortIdentityGetClockIdentity(&(BASE_CLASS_REF(msg)->_sourcePortIdentity))));

    // Expire any SYNC_RECEIPT timers that exist
    ptpNetPortStopSyncReceiptTimer(port);

    ptpMutexLock(ptpNetPortGetProcessingLock(port));

    // port identity of sync message
    PortIdentityRef syncPort = &(BASE_CLASS_REF(msg)->_sourcePortIdentity);
    MsgSyncRef sync = ptpNetPortGetCurrentSync(port, syncPort, BASE_CLASS_REF(msg)->_sequenceId);
    if (sync == NULL) {
        PTP_LOG_DEBUG("%s: Received Follow Up that doesn't match the Sync message (FUP clock id: %016llx (%d))",
            __func__,
            ptpClockIdentityGetUInt64(ptpPortIdentityGetClockIdentity(syncPort)),
            ptpPortIdentityGetPortNumber(syncPort));
        ptpMutexUnlock(ptpNetPortGetProcessingLock(port));
        return;
    }

    ptpMutexUnlock(ptpNetPortGetProcessingLock(port));

    PTP_LOG_DEBUG("Received matching Follow Up packet (%d)", BASE_CLASS_REF(msg)->_sequenceId);
    ptpMsgFollowUpSendDelayRequest(msg, port);
}

void ptpMsgFollowUpSendDelayRequest(MsgFollowUpRef msg, NetworkPortRef port)
{
    PortState state = ptpNetPortGetState(port);

    // Check if we are in slave mode
    if (ePortStateSlave == state) {
        MsgDelayReqRef req = ptpRefMsgDelayReqAlloc();
        ptpMsgDelayReqPortCtor(req, port);

        Timestamp zeroTime;
        ptpTimestampCtor(&zeroTime);

        ptpMsgDelayReqAssignOriginTimestamp(req, &zeroTime);

        PortIdentityRef reqPortId = ptpNetPortGetPortIdentity(port);
        ptpMsgCommonAssignPortIdentity(ptpMsgDelayReqGetBase(req), reqPortId);
        ptpMsgDelayReqSetMatcingFollowUpSeqId(req, BASE_CLASS_REF(msg)->_sequenceId);

        ptpNetPortLockTxLock(port);
        ptpMsgDelayReqSendPort(req, port, ptpMsgCommonGetPortIdentity(BASE_CLASS_REF(msg)));
        ptpNetPortUnlockTxLock(port);

        ptpNetPortSetCurrentDelayReq(port, req);
        ptpRefMsgDelayReqRelease(req);
    }
}

// Gets the precise origin timestamp value
TimestampRef ptpMsgFollowUpGetPreciseOriginTimestamp(MsgFollowUpRef msg)
{
    assert(msg);
    return &(msg->_preciseOriginTimestamp);
}

// Sets the precis origin timestamp value
void ptpMsgFollowUpSetPreciseOriginTimestamp(MsgFollowUpRef msg, TimestampRef timestamp)
{
    assert(msg);
    ptpTimestampAssign(&(msg->_preciseOriginTimestamp), timestamp);
}

FollowUpTlvRef ptpMsgGetFollowUpTlv(MsgFollowUpRef msg)
{
    assert(msg);
    return &(msg->_tlv);
}

// Sets the clock source time interface (802.1AS 9.2)
void ptpMsgFollowUpSetClockSourceTime(MsgFollowUpRef msg, FollowUpTlvRef fup)
{
    assert(msg);
    ptpFollowUpTlvSetGMTimeBaseIndicator(&(msg->_tlv), ptpFollowUpTlvGetGMTimeBaseIndicator(fup));
    ptpFollowUpTlvSetScaledLastGmFreqChange(&(msg->_tlv), ptpFollowUpTlvGetScaledLastGmFreqChange(fup));
    ptpFollowUpTlvAssignScaledLastGmPhaseChange(&(msg->_tlv), ptpFollowUpTlvGetScaledLastGmPhaseChange(fup));
}
