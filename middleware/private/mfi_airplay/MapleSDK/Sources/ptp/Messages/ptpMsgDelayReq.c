/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpMsgDelayReq.h"
#include "Common/ptpMemoryPool.h"
#include "ptpClockContext.h"
#include "ptpClockQuality.h"
#include "ptpMsgDelayResp.h"
#include "ptpNetworkPort.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define BASE_CLASS_REF(ptr) (&(ptr->_payload._base))
#define PAYLOAD(ptr) (&((ptr)->_payload))

static MemoryPoolRef _ptpMsgDelayReqMemPool = NULL;

void* ptpRefMsgDelayReqAllocateMemFromPool(size_t size);
void ptpRefMsgDelayReqDeallocateMemFromPool(void* ptr);

// callback to allocate required amount of memory
void* ptpRefMsgDelayReqAllocateMemFromPool(size_t size)
{
    // create memory pool object
    ptpMemPoolCreateOnce(&_ptpMsgDelayReqMemPool, size, NULL, NULL);

    // allocate one element from the memory pool
    return ptpMemPoolAllocate(_ptpMsgDelayReqMemPool);
}

// callback to release memory allocated via ptpRefAllocateMem callback
void ptpRefMsgDelayReqDeallocateMemFromPool(void* ptr)
{
    assert(_ptpMsgDelayReqMemPool);

    // release one element to the memory pool
    if (ptr)
        ptpMemPoolDeallocate(_ptpMsgDelayReqMemPool, ptr);
}

static void ptpMsgDelayReqCtor(MsgDelayReqRef msg);
DEFINE_REFCOUNTED_OBJECT(MsgDelayReq, ptpRefMsgDelayReqAllocateMemFromPool, ptpRefMsgDelayReqDeallocateMemFromPool, ptpMsgDelayReqCtor, ptpMsgDelayReqDtor)

static void ptpMsgDelayReqCtor(MsgDelayReqRef msg)
{
    assert(msg);

    ptpMsgCommonCtor(BASE_CLASS_REF(msg));
    BASE_CLASS_REF(msg)->_messageType = PTP_DELAY_REQ_MESSAGE;
    BASE_CLASS_REF(msg)->_control = DELAY_REQ; // sru: check if it doesn't have to be set to MESSAGE_OTHER
    BASE_CLASS_REF(msg)->_messageLength = sizeof(MsgDelayReqPayload);
    ptpTimestampCtor(&(PAYLOAD(msg)->_originTimestamp));
    ptpTimestampCtor(&(msg->_txTimestamp));
}

// Default constructor. Creates PTPMessageSync from given payload
void ptpMsgDelayReqPayloadCtor(MsgDelayReqRef msg, const uint8_t* payload, size_t payloadSize)
{
    assert(msg && payload);

    if (payloadSize < sizeof(MsgDelayReqPayload))
        return;

    memcpy(PAYLOAD(msg), payload, sizeof(MsgDelayReqPayload));
    ptpMsgCommonPayloadCtor(BASE_CLASS_REF(msg), payload, payloadSize);
    ptpTimestampFromNetworkOrder(&(PAYLOAD(msg)->_originTimestamp));
    ptpTimestampCtor(&(msg->_txTimestamp));
}

// Default constructor. Creates PTPMessageSync
void ptpMsgDelayReqPortCtor(MsgDelayReqRef msg, NetworkPortRef port)
{
    assert(msg && port);
    ptpMsgDelayReqCtor(msg);

    BASE_CLASS_REF(msg)->_logMeanMsgInterval = 0;
    BASE_CLASS_REF(msg)->_control = DELAY_REQ; // sru: check if it doesn't have to be set to MESSAGE_OTHER
    BASE_CLASS_REF(msg)->_messageType = PTP_DELAY_REQ_MESSAGE;
    BASE_CLASS_REF(msg)->_messageLength = sizeof(MsgDelayReqPayload);
    BASE_CLASS_REF(msg)->_sequenceId = ptpNetPortGetNextDelayReqSequenceId(port);
}

// Destroys the PTPMessageAnnounce interface
void ptpMsgDelayReqDtor(MsgDelayReqRef msg)
{
    if (!msg)
        return;

    ptpTimestampDtor(&(PAYLOAD(msg)->_originTimestamp));
    ptpTimestampDtor(&(msg->_txTimestamp));
    ptpMsgCommonDtor(BASE_CLASS_REF(msg));
}

// Get base class object reference
MsgCommonRef ptpMsgDelayReqGetBase(MsgDelayReqRef msg)
{
    assert(msg);
    return BASE_CLASS_REF(msg);
}

void ptpMsgDelayReqAssign(MsgDelayReqRef msg, const MsgDelayReqRef other)
{
    assert(msg && other);

    if (msg != other) {
        ptpMsgCommonAssign(BASE_CLASS_REF(msg), BASE_CLASS_REF(other));
        ptpTimestampAssign(&(PAYLOAD(msg)->_originTimestamp), &(PAYLOAD(other)->_originTimestamp));
        ptpTimestampAssign(&(msg->_txTimestamp), &(other->_txTimestamp));
    }
}

// Processes PTP messages
void ptpMsgDelayReqProcess(MsgDelayReqRef msg, TimestampRef ingressTime, NetworkPortRef port)
{
    assert(msg);

    if (ptpPortIdentityEqualsTo(ptpNetPortGetPortIdentity(port), ptpMsgCommonGetPortIdentity(BASE_CLASS_REF(msg)))) {

        // Ignore messages from ourself
        return;
    }

    if (ePortStateMaster == ptpNetPortGetState(port)) {

        // create response message
        MsgDelayRespRef resp = ptpRefMsgDelayRespAlloc();
        ptpMsgDelayRespPortCtor(resp, port);

        // assign source/est port identity, sequence id, receipt timestamp
        ptpMsgCommonAssignPortIdentity(ptpMsgDelayRespGetBase(resp), ptpNetPortGetPortIdentity(port));
        ptpMsgCommonSetSequenceId(ptpMsgDelayRespGetBase(resp), BASE_CLASS_REF(msg)->_sequenceId);
        ptpMsgDelayRespAssignRequestingPortIdentity(resp, &(BASE_CLASS_REF(msg)->_sourcePortIdentity));
        ptpMsgDelayRespAssignRequestReceiptTimestamp(resp, ingressTime);

        ptpNetPortLockTxLock(port);
        ptpMsgDelayRespSendPort(resp, port, &(BASE_CLASS_REF(msg)->_sourcePortIdentity));
        ptpNetPortUnlockTxLock(port);

        // release response object
        ptpRefMsgDelayRespRelease(resp);
    }
}

void ptpMsgDelayReqAssignOriginTimestamp(MsgDelayReqRef msg, TimestampRef timestamp)
{
    assert(msg && timestamp);
    ptpTimestampAssign(&(PAYLOAD(msg)->_originTimestamp), timestamp);
}

// Gets origin timestamp value
TimestampRef ptpMsgDelayReqGetOriginTimestamp(MsgDelayReqRef msg)
{
    assert(msg);
    return &(PAYLOAD(msg)->_originTimestamp);
}

uint16_t ptpMsgDelayReqGetMatcingFollowUpSeqId(MsgDelayReqRef msg)
{
    assert(msg);
    return msg->_followUpSeqId;
}

void ptpMsgDelayReqSetMatcingFollowUpSeqId(MsgDelayReqRef msg, uint16_t seqId)
{
    assert(msg);
    msg->_followUpSeqId = seqId;
}

// Get reception/transmission timestamp
TimestampRef ptpMsgDelayReqGetTxTimestamp(MsgDelayReqRef msg)
{
    assert(msg);
    return &(msg->_txTimestamp);
}

// Assembles MsgDelayReq message on the EtherPort payload
Boolean ptpMsgDelayReqSendPort(MsgDelayReqRef msg, NetworkPortRef port, PortIdentityRef destIdentity)
{
    assert(msg && port);

    // create output message payload
    MsgDelayReq outMsg;
    ptpMsgDelayReqAssign(&outMsg, msg);
    ptpMsgCommonBuildHeader(BASE_CLASS_REF(msg), (uint8_t*)(&outMsg));

    // set current time as the origin timestamp
    ptpClockCtxGetTime(&(PAYLOAD(&outMsg)->_originTimestamp));
    ptpTimestampToNetworkOrder(&(PAYLOAD(&outMsg)->_originTimestamp));

    // send payload to the other side
    ptpNetPortSend(port, (uint8_t*)(&outMsg), sizeof(outMsg._payload), destIdentity, &(msg->_txTimestamp), true, false);

    char addrStr[ADDRESS_STRING_LENGTH] = { 0 };
    PTP_LOG_DEBUG("==================================================================");
    PTP_LOG_STATUS("Sent DelayReq message (seq = %d, tx time = %lf s) to %s", BASE_CLASS_REF(msg)->_sequenceId, TIMESTAMP_TO_SECONDS(&(msg->_txTimestamp)), ptpNetPortIdentityToAddressStr(port, destIdentity, addrStr, sizeof(addrStr)));
    PTP_LOG_DEBUG("==================================================================");

    return true;
}
