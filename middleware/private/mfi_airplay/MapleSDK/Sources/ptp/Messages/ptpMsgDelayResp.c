/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpMsgDelayResp.h"
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

#define BASE_CLASS_REF(ptr) (&(ptr->_base))

static MemoryPoolRef _ptpMsgDelayRespMemPool = NULL;

void* ptpRefMsgDelayRespAllocateMemFromPool(size_t size);
void ptpRefMsgDelayRespDeallocateMemFromPool(void* ptr);

// callback to allocate required amount of memory
void* ptpRefMsgDelayRespAllocateMemFromPool(size_t size)
{
    // create memory pool object
    ptpMemPoolCreateOnce(&_ptpMsgDelayRespMemPool, size, NULL, NULL);

    // allocate one element from the memory pool
    return ptpMemPoolAllocate(_ptpMsgDelayRespMemPool);
}

// callback to release memory allocated via ptpRefAllocateMem callback
void ptpRefMsgDelayRespDeallocateMemFromPool(void* ptr)
{
    assert(_ptpMsgDelayRespMemPool);

    // release one element to the memory pool
    if (ptr)
        ptpMemPoolDeallocate(_ptpMsgDelayRespMemPool, ptr);
}

static void ptpMsgDelayRespCtor(MsgDelayRespRef msg);
DEFINE_REFCOUNTED_OBJECT(MsgDelayResp, ptpRefMsgDelayRespAllocateMemFromPool, ptpRefMsgDelayRespDeallocateMemFromPool, ptpMsgDelayRespCtor, ptpMsgDelayRespDtor)

static void ptpMsgDelayRespCtor(MsgDelayRespRef msg)
{
    assert(msg);

    ptpMsgCommonCtor(BASE_CLASS_REF(msg));
    ptpPortIdentityCtor(&(msg->_requestingPortIdentity));
    ptpTimestampCtor(&(msg->_requestReceiptTimestamp));

    BASE_CLASS_REF(msg)->_messageType = PTP_DELAY_RESP_MESSAGE;
    BASE_CLASS_REF(msg)->_control = DELAY_RESP;
    BASE_CLASS_REF(msg)->_messageLength = sizeof(MsgDelayResp);
}

// Default constructor. Creates PTPMessageSync from given payload
void ptpMsgDelayRespPayloadCtor(MsgDelayRespRef msg, const uint8_t* payload, size_t payloadSize)
{
    assert(msg && payload);

    if (payloadSize < sizeof(MsgDelayResp))
        return;

    memcpy(msg, payload, sizeof(MsgDelayResp));
    ptpMsgCommonPayloadCtor(BASE_CLASS_REF(msg), payload, payloadSize);

    ptpPortIdentityFromNetworkOrder(&(msg->_requestingPortIdentity));
    ptpTimestampFromNetworkOrder(&(msg->_requestReceiptTimestamp));
}

// Default constructor. Creates PTPMessageSync
void ptpMsgDelayRespPortCtor(MsgDelayRespRef msg, NetworkPortRef port)
{
    assert(msg && port);
    ptpMsgDelayRespCtor(msg);
    ptpPortIdentityCtor(&(msg->_requestingPortIdentity));
    ptpTimestampCtor(&(msg->_requestReceiptTimestamp));

    BASE_CLASS_REF(msg)->_logMeanMsgInterval = 0;
    BASE_CLASS_REF(msg)->_control = DELAY_RESP; // sru: check if it doesn't have to be set to MESSAGE_OTHER
    BASE_CLASS_REF(msg)->_messageType = PTP_DELAY_RESP_MESSAGE;
    BASE_CLASS_REF(msg)->_messageLength = sizeof(MsgDelayResp);

    ptpPortIdentityAssign(&(msg->_requestingPortIdentity), ptpNetPortGetPortIdentity(port));
}

// Destroys the PTPMessageAnnounce interface
void ptpMsgDelayRespDtor(MsgDelayRespRef msg)
{
    if (!msg)
        return;

    ptpMsgCommonDtor(BASE_CLASS_REF(msg));
}

// Get base class object reference
MsgCommonRef ptpMsgDelayRespGetBase(MsgDelayRespRef msg)
{
    assert(msg);
    return BASE_CLASS_REF(msg);
}

void ptpMsgDelayRespAssign(MsgDelayRespRef msg, const MsgDelayRespRef other)
{
    assert(msg && other);

    if (msg != other) {

        ptpMsgCommonAssign(BASE_CLASS_REF(msg), &(other->_base));
        ptpTimestampAssign(&(msg->_requestReceiptTimestamp), &(other->_requestReceiptTimestamp));
        ptpPortIdentityAssign(&(msg->_requestingPortIdentity), &(other->_requestingPortIdentity));
    }
}

// Processes PTP messages
void ptpMsgDelayRespProcess(MsgDelayRespRef msg, NetworkPortRef port)
{
    assert(msg);

    if (ptpPortIdentityEqualsTo(ptpNetPortGetPortIdentity(port), ptpMsgCommonGetPortIdentity(BASE_CLASS_REF(msg)))) {

        // Ignore messages from ourself
        PTP_LOG_VERBOSE("Ignoring Delay Response message from self.");
        return;
    }

    // check if we have all required messages ready and do the calculation
    ptpMsgCommonCalculate(msg, port);
}

// Overload of the base method to inject the Apple Vendor PTP Profile specific
// values.
static void ptpMsgDelayRespBuildCommonHeader(MsgDelayRespRef msg, TimestampRef currentTime, uint8_t* buf)
{
    assert(msg);

    // Compute correction field.
    // This is the time between reception of delay request message and the current time
    double residenceTime = TIMESTAMP_TO_NANOSECONDS(currentTime) - TIMESTAMP_TO_NANOSECONDS(&(msg->_requestReceiptTimestamp));
    BASE_CLASS_REF(msg)->_correctionField = ((int64_t)residenceTime) << 16;
    ptpMsgCommonBuildHeader(BASE_CLASS_REF(msg), buf);
}

// Assembles MsgDelayResp message on the EtherPort payload
Boolean ptpMsgDelayRespSendPort(MsgDelayRespRef msg, NetworkPortRef port, PortIdentityRef destIdentity)
{
    assert(msg && port);

    MsgDelayResp outMsg;
    ptpMsgDelayRespAssign(&outMsg, msg);

    // build base header - this also calculates the correction filed
    // so timestamp must be known at this moment!
    Timestamp timestamp;
    ptpClockCtxGetTime(&timestamp);

    ptpMsgDelayRespBuildCommonHeader(msg, &timestamp, (uint8_t*)(&outMsg));
    ptpPortIdentityToNetworkOrder(&(outMsg._requestingPortIdentity));
    ptpTimestampToNetworkOrder(&(outMsg._requestReceiptTimestamp));

    PTP_LOG_VERBOSE("requestingPortIdentity   : %016llx", ptpClockIdentityGetUInt64(ptpPortIdentityGetClockIdentity(&(msg->_requestingPortIdentity))));
    PTP_LOG_VERBOSE("sourcePortIdentity       : %016llx", ptpClockIdentityGetUInt64(ptpPortIdentityGetClockIdentity(&(BASE_CLASS_REF(msg)->_sourcePortIdentity))));
    PTP_LOG_VERBOSE("request receipt timestamp: %lf s", TIMESTAMP_TO_SECONDS(&(msg->_requestReceiptTimestamp)));
    ptpNetPortSend(port, (uint8_t*)(&outMsg), sizeof(outMsg), destIdentity, NULL, false, false);

    char addrStr[ADDRESS_STRING_LENGTH] = { 0 };
    PTP_LOG_DEBUG("==================================================================");
    PTP_LOG_STATUS("Sent Delay Response message (seq = %d, tx time = %lf s) to %s", BASE_CLASS_REF(msg)->_sequenceId, TIMESTAMP_TO_SECONDS(&timestamp), ptpNetPortIdentityToAddressStr(port, destIdentity, addrStr, sizeof(addrStr)));
    PTP_LOG_DEBUG("==================================================================");

    return true;
}

// Sets the request receipt timestamp
void ptpMsgDelayRespAssignRequestReceiptTimestamp(MsgDelayRespRef msg, TimestampRef timestamp)
{
    assert(msg);
    ptpTimestampAssign(&(msg->_requestReceiptTimestamp), timestamp);
}

// Gets the request receipt timestamp
TimestampRef ptpMsgDelayRespGetRequestReceiptTimestamp(MsgDelayRespRef msg)
{
    assert(msg);
    return &(msg->_requestReceiptTimestamp);
}

// Sets requesting port identity
void ptpMsgDelayRespAssignRequestingPortIdentity(MsgDelayRespRef msg, PortIdentityRef identity)
{
    assert(msg);
    ptpPortIdentityAssign(&(msg->_requestingPortIdentity), identity);
}

// Gets requesting port identity
PortIdentityRef ptpMsgDelayRespGetRequestingPortIdentity(MsgDelayRespRef msg)
{
    assert(msg);
    return &(msg->_requestingPortIdentity);
}
