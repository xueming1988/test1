/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpMsgSignalling.h"
#include "Common/ptpMemoryPool.h"
#include "ptpClockContext.h"
#include "ptpNetworkPort.h"

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

//
// SignallingTlv
//

DEFINE_REFCOUNTED_OBJECT(SignallingTlv, NULL, NULL, ptpSignallingTlvCtor, ptpSignallingTlvDtor)

void* ptpRefMsgSignallingAllocateMemFromPool(size_t size);
void ptpRefMsgSignallingDeallocateMemFromPool(void* ptr);

// Builds the Signalling Msg Interval Request TLV interface
void ptpSignallingTlvCtor(SignallingTlvRef tlv)
{
    assert(tlv);

    tlv->_tlvType = 0x3;
    tlv->_lengthField = 12;
    tlv->_organizationId[0] = (uint8_t)'\x00';
    tlv->_organizationId[1] = (uint8_t)'\x80';
    tlv->_organizationId[2] = (uint8_t)'\xC2';
    tlv->_organizationSubTypeMs = 0;
    tlv->_organizationSubTypeLs = 2;
    tlv->_linkDelayInterval = 0;
    tlv->_timeSyncInterval = 0;
    tlv->_announceInterval = 0;
    tlv->_flags = 3;
    tlv->_reserved = 0;
}

// destructor
void ptpSignallingTlvDtor(SignallingTlvRef tlv)
{
    if (!tlv)
        return;

    // empty
}

void ptpSignallingTlvAssign(SignallingTlvRef tlv, const SignallingTlvRef other)
{
    assert(tlv && other);

    if (tlv != other) {
        memcpy(tlv, other, sizeof(*tlv));
    }
}

// convert Signalling to/from network order
void ptpSignallingTlvFromNetworkOrder(SignallingTlvRef tlv)
{
    assert(tlv);

    tlv->_tlvType = ptpNtohsu(tlv->_tlvType);
    tlv->_lengthField = ptpNtohsu(tlv->_lengthField);
    tlv->_organizationSubTypeLs = ptpNtohsu(tlv->_organizationSubTypeLs);
    tlv->_reserved = ptpNtohsu(tlv->_reserved);
}

void ptpSignallingTlvToNetworkOrder(SignallingTlvRef tlv)
{
    assert(tlv);

    tlv->_tlvType = ptpHtonsu(tlv->_tlvType);
    tlv->_lengthField = ptpHtonsu(tlv->_lengthField);
    tlv->_organizationSubTypeLs = ptpHtonsu(tlv->_organizationSubTypeLs);
    tlv->_reserved = ptpHtonsu(tlv->_reserved);
}

//  Gets the link delay interval.
uint8_t ptpSignallingTlvGetLinkDelayInterval(SignallingTlvRef tlv)
{
    assert(tlv);
    return tlv->_linkDelayInterval;
}

//  Sets the link delay interval.
void ptpSignallingTlvSetLinkDelayInterval(SignallingTlvRef tlv, uint8_t linkDelayInterval)
{
    assert(tlv);
    tlv->_linkDelayInterval = linkDelayInterval;
}

//  Gets the time sync interval.
uint8_t ptpSignallingTlvGetTimeSyncInterval(SignallingTlvRef tlv)
{
    assert(tlv);
    return tlv->_timeSyncInterval;
}

//  Sets the time sync interval.
void ptpSignallingTlvSetTimeSyncInterval(SignallingTlvRef tlv, uint8_t timeSyncInterval)
{
    assert(tlv);
    tlv->_timeSyncInterval = timeSyncInterval;
}

//  Gets the announce interval.
uint8_t ptpSignallingTlvGetAnnounceInterval(SignallingTlvRef tlv)
{
    assert(tlv);
    return tlv->_announceInterval;
}

//  Sets the announce interval.
void ptpSignallingTlvSetAnnounceInterval(SignallingTlvRef tlv, uint8_t announceInterval)
{
    assert(tlv);
    tlv->_announceInterval = announceInterval;
}

//
// MsgSignalling
//

#define BASE_CLASS_REF(ptr) (&(ptr->_base))

static MemoryPoolRef _ptpMsgSignallingMemPool = NULL;

// callback to allocate required amount of memory
void* ptpRefMsgSignallingAllocateMemFromPool(size_t size)
{
    // create memory pool object
    ptpMemPoolCreateOnce(&_ptpMsgSignallingMemPool, size, NULL, NULL);

    // allocate one element from the memory pool
    return ptpMemPoolAllocate(_ptpMsgSignallingMemPool);
}

// callback to release memory allocated via ptpRefAllocateMem callback
void ptpRefMsgSignallingDeallocateMemFromPool(void* ptr)
{
    assert(_ptpMsgSignallingMemPool);

    // release one element to the memory pool
    if (ptr)
        ptpMemPoolDeallocate(_ptpMsgSignallingMemPool, ptr);
}

static void ptpMsgSignallingCtor(MsgSignallingRef msg);
DEFINE_REFCOUNTED_OBJECT(MsgSignalling, ptpRefMsgSignallingAllocateMemFromPool, ptpRefMsgSignallingDeallocateMemFromPool, ptpMsgSignallingCtor, ptpMsgSignallingDtor)

static void ptpMsgSignallingCtor(MsgSignallingRef msg)
{
    assert(msg);

    ptpMsgCommonCtor(BASE_CLASS_REF(msg));
    BASE_CLASS_REF(msg)->_messageType = PTP_SIGNALLING_MESSAGE;
    BASE_CLASS_REF(msg)->_messageLength = sizeof(MsgSignalling);
    BASE_CLASS_REF(msg)->_control = MESSAGE_OTHER;
    BASE_CLASS_REF(msg)->_logMeanMsgInterval = 0x7F; // 802.1AS 2011 10.5.2.2.11 logMessageInterval (Integer8)
    BASE_CLASS_REF(msg)->_sequenceId = 0;

    ptpPortIdentityCtor(&(msg->_portIdentity));
    ptpSignallingTlvCtor(&(msg->_tlv));
}

// Default constructor. Creates PTPMessageSync from given payload
void ptpMsgSignallingPayloadCtor(MsgSignallingRef msg, const uint8_t* payload, size_t payloadSize)
{
    assert(msg && payload);

    if (payloadSize < sizeof(MsgSignalling))
        return;

    memcpy(msg, payload, sizeof(MsgSignalling));
    ptpMsgCommonPayloadCtor(BASE_CLASS_REF(msg), payload, payloadSize);
    ptpSignallingTlvFromNetworkOrder(&(msg->_tlv));
}

// Default constructor. Creates PTPMessageSync
void ptpMsgSignallingPortCtor(MsgSignallingRef msg, NetworkPortRef port)
{
    assert(msg && port);
    ptpMsgSignallingCtor(msg);
    BASE_CLASS_REF(msg)->_sequenceId = ptpNetPortGetNextSignalSequenceId(port);
}

// Destroys the PTPMessageAnnounce interface
void ptpMsgSignallingDtor(MsgSignallingRef msg)
{
    if (!msg)
        return;

    ptpPortIdentityDtor(&(msg->_portIdentity));
    ptpSignallingTlvDtor(&(msg->_tlv));
    ptpMsgCommonDtor(BASE_CLASS_REF(msg));
}

// Get base class object reference
MsgCommonRef ptpMsgSignallingGetBase(MsgSignallingRef msg)
{
    assert(msg);
    return BASE_CLASS_REF(msg);
}

void ptpMsgSignallingAssign(MsgSignallingRef msg, const MsgSignallingRef other)
{
    assert(msg && other);

    if (msg != other) {
        ptpMsgCommonAssign(BASE_CLASS_REF(msg), &(other->_base));

        ptpPortIdentityAssign(&(msg->_portIdentity), &(other->_portIdentity));
        ptpSignallingTlvAssign(&(msg->_tlv), &(other->_tlv));
    }
}

// Assembles PTPMessageFollowUp message on the EtherPort payload
Boolean ptpMsgSignallingSendPort(MsgSignallingRef msg, NetworkPortRef port, PortIdentityRef destIdentity)
{
    assert(msg && port);

    MsgSignalling outMsg;
    ptpMsgSignallingAssign(&outMsg, msg);
    ptpMsgCommonBuildHeader(BASE_CLASS_REF(msg), (uint8_t*)&outMsg);
    ptpPortIdentityToNetworkOrder(&(outMsg._portIdentity));
    ptpSignallingTlvToNetworkOrder(&(outMsg._tlv));

    Timestamp timestamp;
    ptpNetPortSend(port, (uint8_t*)&outMsg, sizeof(outMsg), destIdentity, &timestamp, false, true);

    char addrStr[ADDRESS_STRING_LENGTH] = { 0 };
    PTP_LOG_DEBUG("==================================================================");
    PTP_LOG_STATUS("Sent Signalling message (seq = %d, tx time = %lf s) to %s", BASE_CLASS_REF(msg)->_sequenceId, TIMESTAMP_TO_SECONDS(&timestamp), ptpNetPortIdentityToAddressStr(port, destIdentity, addrStr, sizeof(addrStr)));
    PTP_LOG_DEBUG("==================================================================");

    return true;
}

// Processes PTP messages
void ptpMsgSignallingProcess(MsgSignallingRef msg)
{
    assert(msg);

    PTP_LOG_STATUS("Signalling Link Delay Interval: %d", ptpSignallingTlvGetLinkDelayInterval(&(msg->_tlv)));
    PTP_LOG_STATUS("Signalling Sync Interval      : %d", ptpSignallingTlvGetTimeSyncInterval(&(msg->_tlv)));
    PTP_LOG_STATUS("Signalling Announce Interval  : %d", ptpSignallingTlvGetAnnounceInterval(&(msg->_tlv)));
}

// Sets the signalling intervals
void ptpMsgSignallingSetintervals(MsgSignallingRef msg, uint8_t linkDelayInterval, uint8_t timeSyncInterval, uint8_t announceInterval)
{
    assert(msg);

    ptpSignallingTlvSetLinkDelayInterval(&(msg->_tlv), linkDelayInterval);
    ptpSignallingTlvSetTimeSyncInterval(&(msg->_tlv), timeSyncInterval);
    ptpSignallingTlvSetAnnounceInterval(&(msg->_tlv), announceInterval);
}
