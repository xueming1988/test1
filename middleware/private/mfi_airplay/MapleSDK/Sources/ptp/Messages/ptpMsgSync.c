/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpMsgSync.h"
#include "Common/ptpMemoryPool.h"
#include "ptpClockContext.h"
#include "ptpClockQuality.h"
#include "ptpMsgCommon.h"
#include "ptpMsgDelayReq.h"
#include "ptpNetworkPort.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define PTP_ASSIST_BYTE 0 // PTP_ASSIST(two step flag) byte offset on flags field*/
#define PTP_ASSIST_BIT 1 // PTP_ASSIST(two step flag) bit on PTP_ASSIST byte*/

#define BASE_CLASS_REF(ptr) (&(ptr->_payload._base))
#define PAYLOAD(ptr) (&((ptr)->_payload))

static MemoryPoolRef _ptpMsgSyncMemPool = NULL;

void* ptpRefMsgSyncAllocateMemFromPool(size_t size);
void ptpRefMsgSyncDeallocateMemFromPool(void* ptr);

// callback to allocate required amount of memory
void* ptpRefMsgSyncAllocateMemFromPool(size_t size)
{
    // create memory pool object
    ptpMemPoolCreateOnce(&_ptpMsgSyncMemPool, size, NULL, NULL);

    // allocate one element from the memory pool
    return ptpMemPoolAllocate(_ptpMsgSyncMemPool);
}

// callback to release memory allocated via ptpRefAllocateMem callback
void ptpRefMsgSyncDeallocateMemFromPool(void* ptr)
{
    assert(_ptpMsgSyncMemPool);

    // release one element to the memory pool
    if (ptr)
        ptpMemPoolDeallocate(_ptpMsgSyncMemPool, ptr);
}

static void ptpMsgSyncCtor(MsgSyncRef msg);
DEFINE_REFCOUNTED_OBJECT(MsgSync, ptpRefMsgSyncAllocateMemFromPool, ptpRefMsgSyncDeallocateMemFromPool, ptpMsgSyncCtor, ptpMsgSyncDtor)

static void ptpMsgSyncCtor(MsgSyncRef msg)
{
    assert(msg);

    ptpMsgCommonCtor(BASE_CLASS_REF(msg));
    BASE_CLASS_REF(msg)->_messageType = PTP_SYNC_MESSAGE;
    BASE_CLASS_REF(msg)->_control = SYNC;
    BASE_CLASS_REF(msg)->_messageLength = sizeof(MsgSyncPayload);
    ptpTimestampCtor(&(msg->_rxTimestamp));
}

// Default constructor. Creates PTPMessageSync from given payload
void ptpMsgSyncPayloadCtor(MsgSyncRef msg, TimestampRef rxTimestamp, const uint8_t* payload, size_t payloadSize)
{
    assert(msg && payload);

    if (payloadSize < sizeof(MsgSyncPayload))
        return;

    memcpy(PAYLOAD(msg), payload, sizeof(MsgSyncPayload));
    ptpMsgCommonPayloadCtor(BASE_CLASS_REF(msg), payload, payloadSize);
    ptpTimestampFromNetworkOrder(&(PAYLOAD(msg)->_originTimestamp));
    ptpTimestampAssign(&(msg->_rxTimestamp), rxTimestamp);
}

// Default constructor. Creates PTPMessageSync
void ptpMsgSyncPortCtor(MsgSyncRef msg, NetworkPortRef port)
{
    assert(msg && port);
    ptpMsgSyncCtor(msg);
    ptpTimestampCtor(&(msg->_rxTimestamp));

    BASE_CLASS_REF(msg)->_messageType = PTP_SYNC_MESSAGE; // This is an event message
    BASE_CLASS_REF(msg)->_sequenceId = ptpNetPortGetNextSyncSequenceId(port);
    BASE_CLASS_REF(msg)->_control = SYNC;
    BASE_CLASS_REF(msg)->_messageLength = sizeof(MsgSyncPayload);
    BASE_CLASS_REF(msg)->_flags[PTP_ASSIST_BYTE] |= (0x1 << PTP_ASSIST_BIT);
    BASE_CLASS_REF(msg)->_logMeanMsgInterval = ptpNetPortGetSyncInterval(port);
    ptpClockCtxGetTime(&(PAYLOAD(msg)->_originTimestamp));
}

// Destroys the PTPMessageAnnounce interface
void ptpMsgSyncDtor(MsgSyncRef msg)
{
    if (!msg)
        return;

    ptpTimestampDtor(&(PAYLOAD(msg)->_originTimestamp));
    ptpTimestampDtor(&(msg->_rxTimestamp));
    ptpMsgCommonDtor(BASE_CLASS_REF(msg));
}

// Get base class object reference
MsgCommonRef ptpMsgSyncGetBase(MsgSyncRef msg)
{
    assert(msg);
    return BASE_CLASS_REF(msg);
}

void ptpMsgSyncAssign(MsgSyncRef msg, const MsgSyncRef other)
{
    assert(msg && other);

    if (msg != other) {
        ptpMsgCommonAssign(BASE_CLASS_REF(msg), BASE_CLASS_REF(other));
        PAYLOAD(msg)->_originTimestamp = PAYLOAD(other)->_originTimestamp;
    }
}

// Processes PTP messages
void ptpMsgSyncProcess(MsgSyncRef msg, NetworkPortRef port)
{
    assert(msg);

    PortState state = ptpNetPortGetState(port);

    PTP_LOG_VERBOSE("PTPMessageSync::processMessage   state:%d", state);

    if (ePortStateInit == state) {

        // Do nothing Sync messages should be ignored when in init state
        return;
    }

    PortIdentityRef syncPort = ptpMsgCommonGetPortIdentity(BASE_CLASS_REF(msg));
    if (ptpPortIdentityEqualsTo(ptpNetPortGetPortIdentity(port), syncPort)) {

        // Ignore messages from self
        return;
    }

    uint16_t syncId = ptpMsgCommonGetSequenceId(BASE_CLASS_REF(msg));

    // do we already have Sync message for this sequence id? Then we MUST NOT store this Sync message
    // as it is most probably repeated Sync received from a different IP protocol version
    // (Apple devices use to send Sync/FUP sequences to both IPv4 and IPv6 addresses)
    if (ptpNetPortGetCurrentSync(port, syncPort, syncId)) {
        PTP_LOG_DEBUG("Repeated Sync message #%d received, ignoring...", syncId);
        return;
    }

    // assign the new sync object
    ptpNetPortSetCurrentSync(port, msg);
    PTP_LOG_DEBUG("New SYNC sequence id: %d", syncId);

    // If we already have a matching FollowUp then process it.
    MsgFollowUpRef followUpMsg = ptpNetPortGetCurrentFollowUp(port, syncPort, syncId);
    if (followUpMsg) {
        PTP_LOG_DEBUG("ptpMsgSyncProcess: Handling Sync-Followup inversion");
        ptpMsgFollowUpSendDelayRequest(followUpMsg, port);
    }
}

// Gets origin timestamp value
TimestampRef ptpMsgSyncGetOriginTimestamp(MsgSyncRef msg)
{
    assert(msg);
    return &(PAYLOAD(msg)->_originTimestamp);
}

// Get reception timestamp
TimestampRef ptpMsgSyncGetRxTimestamp(MsgSyncRef msg)
{
    assert(msg);
    return &(msg->_rxTimestamp);
}

// Assembles MsgSync message on the EtherPort payload
Boolean ptpMsgSyncSendPort(MsgSyncRef msg, NetworkPortRef port, PortIdentityRef destIdentity)
{
    assert(msg && port);
    MsgSync outMsg;
    ptpMsgSyncAssign(&outMsg, msg);
    ptpMsgCommonBuildHeader(BASE_CLASS_REF(msg), (uint8_t*)&outMsg);

    // Get timestamp
    ptpClockCtxGetTime(&(PAYLOAD(&outMsg)->_originTimestamp));
    ptpTimestampToNetworkOrder(&(PAYLOAD(&outMsg)->_originTimestamp));

    // and send it to the other side
    ptpNetPortSend(port, (uint8_t*)&outMsg, sizeof(outMsg._payload), destIdentity, &(PAYLOAD(msg)->_originTimestamp), true, true);

    char addrStr[ADDRESS_STRING_LENGTH] = { 0 };
    PTP_LOG_DEBUG("==================================================================");
    PTP_LOG_STATUS("Sent Sync message (seq = %d, tx time = %lf s) to %s", BASE_CLASS_REF(msg)->_sequenceId, TIMESTAMP_TO_SECONDS(&(PAYLOAD(msg)->_originTimestamp)), ptpNetPortIdentityToAddressStr(port, destIdentity, addrStr, sizeof(addrStr)));
    PTP_LOG_DEBUG("==================================================================");
    return true;
}
