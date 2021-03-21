/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpMsgAnnounce.h"
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

//
// PathTraceTLV implementation
//
void ptpPathTraceTlvCtor(PathTraceTLVRef tlv);
void* ptpRefMsgAnnounceAllocateMemFromPool(size_t size);
void ptpRefMsgAnnounceDeallocateMemFromPool(void* ptr);

void ptpPathTraceTlvCtor(PathTraceTLVRef tlv)
{
    assert(tlv);
    tlv->_identityList = ptpListClockIdentityCreate();
    tlv->_tlvType = ptpHtonsu(PATH_TRACE_TLV_TYPE);
}

void ptpPathTraceTlvDtor(PathTraceTLVRef tlv)
{
    if (!tlv)
        return;

    ptpListClockIdentityDestroy(tlv->_identityList);
}

void ptpPathTraceTlvAssign(PathTraceTLVRef tlv, const PathTraceTLVRef other)
{
    if (tlv != other) {

        ListClockIdentityIterator iter;
        ptpListClockIdentityClear(tlv->_identityList);

        // copy clock identities
        for (iter = ptpListClockIdentityFirst(other->_identityList); iter != NULL; iter = ptpListClockIdentityNext(iter)) {

            // get clock identity object
            ClockIdentityRef identity = ptpListClockIdentityIteratorValue(iter);

            // retain it and add it to the list
            ptpRefClockIdentityRetain(identity);
            ptpListClockIdentityPushBack(tlv->_identityList, identity);
        }

        tlv->_tlvType = other->_tlvType;
    }
}

// Parses ClockIdentity from message buffer buffer [in] Message buffer. It should be at least ::PTP_CLOCK_IDENTITY_LENGTH bytes long.
void ptpPathTraceTlvParseClockIdentity(PathTraceTLVRef tlv, const uint8_t* buffer, size_t size)
{
    assert(tlv);
    int length = (int)ptpNtohss(*(const int16_t*)buffer);

    buffer += sizeof(uint16_t);
    size -= sizeof(uint16_t);

    if (size < (unsigned)length) {
        length = (int)size;
    }

    length /= CLOCK_IDENTITY_LENGTH;

    for (; length > 0; --length) {
        ClockIdentityRef add = ptpRefClockIdentityAlloc();
        ptpClockIdentityAssignBytes(add, buffer);
        ptpListClockIdentityPushBack(tlv->_identityList, add);
        buffer += CLOCK_IDENTITY_LENGTH;
    }
}

// Appends new ClockIdentity to internal ClockIdentity list
void ptpPathTraceTlvAppendClockIdentity(PathTraceTLVRef tlv, ClockIdentityRef id)
{
    assert(tlv);
    ptpRefClockIdentityRetain(id);
    ptpListClockIdentityPushBack(tlv->_identityList, id);
}

// Gets TLV value in a byte string format
void ptpPathTraceTlvToByteString(PathTraceTLVRef tlv, uint8_t* byteStr)
{
    assert(tlv);

    memcpy(byteStr, (void*)&(tlv->_tlvType), sizeof(tlv->_tlvType)); // tlvType already in network byte order
    byteStr += sizeof(tlv->_tlvType);

    uint16_t listSize = ptpHtonsu((uint16_t)ptpListClockIdentitySize(tlv->_identityList) * CLOCK_IDENTITY_LENGTH);
    memcpy(byteStr, &listSize, sizeof(listSize));
    byteStr += sizeof(listSize);

    // get first iterator
    ListClockIdentityIterator iter;

    for (iter = ptpListClockIdentityFirst(tlv->_identityList); iter != NULL; iter = ptpListClockIdentityNext(iter)) {
        ptpClockIdentityGetBytes(ptpListClockIdentityIteratorValue(iter), byteStr);
        byteStr += CLOCK_IDENTITY_LENGTH;
    }
}

// Looks for a specific ClockIdentity on the current TLV
Boolean ptpPathTraceTlvHasClockIdentity(PathTraceTLVRef tlv, ClockIdentityRef id)
{
    ListClockIdentityIterator iter;
    assert(tlv);

    for (iter = ptpListClockIdentityFirst(tlv->_identityList); iter != NULL; iter = ptpListClockIdentityNext(iter)) {
        if (ptpClockIdentityEqualsTo(id, ptpListClockIdentityIteratorValue(iter)))
            return true;
    }

    return false;
}

// Gets the total length of TLV. Total length of TLV is length of type field (UINT16) + length of 'length' field (UINT16) + length of
// identities (each PTP_CLOCK_IDENTITY_LENGTH) in the path
uint32_t ptpPathTraceTlvLength(PathTraceTLVRef tlv)
{
    assert(tlv);
    return 2 * (int)sizeof(uint16_t) + CLOCK_IDENTITY_LENGTH * ptpListClockIdentitySize(tlv->_identityList);
}

char* ptpPathTraceTlvGetIdentitiesString(PathTraceTLVRef tlv)
{
    unsigned int numIdentities = 0;
    size_t idsSize;
    char *ids = NULL, *idsCurr;
    ListClockIdentityIterator it;

    assert(tlv);

    // how many identities do we have?
    if (tlv->_identityList) {
        numIdentities = ptpListClockIdentitySize(tlv->_identityList);

        // allocate buffer that is large enough
        idsSize = numIdentities * (CLOCK_IDENTITY_LENGTH * 3 + 3);
        ids = (char*)malloc(idsSize);
        idsCurr = (char*)ids;

        for (it = ptpListClockIdentityFirst(tlv->_identityList); it != NULL; it = ptpListClockIdentityNext(it)) {
            ClockIdentityRef identity = ptpListClockIdentityIteratorValue(it);

            // is this the first entry?
            if (idsCurr == ids)
                ptpSnprintf(idsCurr, idsSize - (size_t)(idsCurr - ids), "%016llux", ptpClockIdentityGetUInt64(identity));
            else
                ptpSnprintf(idsCurr, idsSize - (size_t)(idsCurr - ids), ", %016llux", ptpClockIdentityGetUInt64(identity));
        }
    }
    return ids;
}

uint16_t ptpPathTraceTlvType(PathTraceTLVRef tlv)
{
    assert(tlv);
    return tlv->_tlvType;
}

//
// MsgAnnounce
//

#define BASE_CLASS_REF(ptr) (&(ptr->_payload._base))
#define PAYLOAD(ptr) (&(ptr->_payload))

static MemoryPoolRef _ptpMsgAnnounceMemPool = NULL;

// callback to allocate required amount of memory
void* ptpRefMsgAnnounceAllocateMemFromPool(size_t size)
{
    // create memory pool object
    ptpMemPoolCreateOnce(&_ptpMsgAnnounceMemPool, size, NULL, NULL);

    // allocate one element from the memory pool
    return ptpMemPoolAllocate(_ptpMsgAnnounceMemPool);
}

// callback to release memory allocated via ptpRefAllocateMem callback
void ptpRefMsgAnnounceDeallocateMemFromPool(void* ptr)
{
    assert(_ptpMsgAnnounceMemPool);

    // release one element to the memory pool
    if (ptr)
        ptpMemPoolDeallocate(_ptpMsgAnnounceMemPool, ptr);
}

static void ptpMsgAnnounceCtor(MsgAnnounceRef msg);
DEFINE_REFCOUNTED_OBJECT(MsgAnnounce, ptpRefMsgAnnounceAllocateMemFromPool, ptpRefMsgAnnounceDeallocateMemFromPool, ptpMsgAnnounceCtor, ptpMsgAnnounceDtor)

// MsgAnnounce default constructor. This has to be explicitly called
// only on a statically allocated PathTraceTLV object.
static void ptpMsgAnnounceCtor(MsgAnnounceRef msg)
{
    assert(msg);

    ptpMsgCommonCtor(BASE_CLASS_REF(msg));
    ptpPathTraceTlvCtor(&(msg->_tlv));
    ptpClockQualityCtor(&(PAYLOAD(msg)->_grandmasterClockQuality));

    BASE_CLASS_REF(msg)->_messageType = PTP_ANNC_MESSAGE;
    BASE_CLASS_REF(msg)->_control = MESSAGE_OTHER;

    PAYLOAD(msg)->_currentUtcOffset = 0;
    PAYLOAD(msg)->_grandmasterPriority1 = 0;
    PAYLOAD(msg)->_grandmasterPriority2 = 0;
    PAYLOAD(msg)->_stepsRemoved = 0;
    PAYLOAD(msg)->_timeSource = 0;
}

// Default constructor. Creates PTPMessageAnnounce from given payload
void ptpMsgAnnouncePayloadCtor(MsgAnnounceRef msg, TimestampRef ingressTime, const uint8_t* payload, size_t payloadSize)
{
    assert(msg && payload);

    if (payloadSize < sizeof(MsgAnnouncePayload))
        return;

    ptpTimestampCtor(&(msg->_rxTimestamp));
    ptpTimestampAssign(&(msg->_rxTimestamp), ingressTime);

    memcpy(PAYLOAD(msg), payload, sizeof(MsgAnnouncePayload));
    ptpMsgCommonPayloadCtor(BASE_CLASS_REF(msg), payload, payloadSize);

    ptpClockQualityFromNetworkOrder(&(PAYLOAD(msg)->_grandmasterClockQuality));
    PAYLOAD(msg)->_currentUtcOffset = ptpNtohss(PAYLOAD(msg)->_currentUtcOffset);
    PAYLOAD(msg)->_stepsRemoved = ptpNtohsu(PAYLOAD(msg)->_stepsRemoved);

    // Parse TLV if it exists
    const uint8_t* buf = payload + sizeof(MsgAnnouncePayload);
    payloadSize -= sizeof(MsgAnnouncePayload);

    if ((payloadSize > (int)(2 * sizeof(uint16_t))) && (ptpNtohsu(*((const uint16_t*)buf)) == PATH_TRACE_TLV_TYPE)) {

        buf += sizeof(uint16_t);
        payloadSize -= sizeof(uint16_t);
        ptpPathTraceTlvParseClockIdentity(&(msg->_tlv), buf, payloadSize);
    }
}

void ptpMsgAnnouncePortCtor(MsgAnnounceRef msg, NetworkPortRef port)
{
    ClockCtxRef clock;
    ClockIdentityRef id;

    assert(msg && port);
    ptpMsgAnnounceCtor(msg);
    ptpTimestampCtor(&(msg->_rxTimestamp));

    BASE_CLASS_REF(msg)->_messageType = PTP_ANNC_MESSAGE;
    BASE_CLASS_REF(msg)->_control = MESSAGE_OTHER;
    BASE_CLASS_REF(msg)->_sequenceId = ptpNetPortGetNextAnnounceSequenceId(port);
    BASE_CLASS_REF(msg)->_logMeanMsgInterval = ptpNetPortGetAnnounceInterval(port);

    // create refcountable copy of clock identity
    clock = ptpNetPortGetClock(port);
    id = ptpRefClockIdentityAlloc();

    ptpClockIdentityAssign(id, ptpClockCtxGetClockIdentity(clock));
    ptpPathTraceTlvAppendClockIdentity(&(msg->_tlv), id);
    ptpRefClockIdentityRelease(id);

    PAYLOAD(msg)->_currentUtcOffset = ptpClockCtxGetCurrentUtcOffset(clock);
    PAYLOAD(msg)->_grandmasterPriority1 = ptpClockCtxGetPriority1(clock);
    PAYLOAD(msg)->_grandmasterPriority2 = ptpClockCtxGetPriority2(clock);
    PAYLOAD(msg)->_timeSource = ptpClockCtxGetTimeSource(clock);
    PAYLOAD(msg)->_stepsRemoved = 0;

    ptpClockQualityAssign(&(PAYLOAD(msg)->_grandmasterClockQuality), ptpClockCtxGetClockQuality(clock));
    ptpClockIdentityAssign(&(PAYLOAD(msg)->_grandmasterIdentity), ptpClockCtxGetGrandmasterClockIdentity(clock));
}

// Destroys the PTPMessageAnnounce interface
void ptpMsgAnnounceDtor(MsgAnnounceRef msg)
{
    if (!msg)
        return;

    ptpTimestampDtor(&(msg->_rxTimestamp));
    ptpPathTraceTlvDtor(&(msg->_tlv));
    ptpClockQualityDtor(&(PAYLOAD(msg)->_grandmasterClockQuality));
    ptpClockIdentityDtor(&(PAYLOAD(msg)->_grandmasterIdentity));
    ptpMsgCommonDtor(BASE_CLASS_REF(msg));
}

// Get base class object reference
MsgCommonRef ptpMsgAnnounceGetBase(MsgAnnounceRef msg)
{
    assert(msg);
    return BASE_CLASS_REF(msg);
}

void ptpMsgAnnounceAssign(MsgAnnounceRef msg, const MsgAnnounceRef other)
{
    assert(msg && other);

    if (msg != other) {
        ptpMsgCommonAssign(BASE_CLASS_REF(msg), BASE_CLASS_REF(other));
        ptpPathTraceTlvAssign(&(msg->_tlv), &(other->_tlv));
        ptpClockQualityAssign(&(PAYLOAD(msg)->_grandmasterClockQuality), &(PAYLOAD(other)->_grandmasterClockQuality));
        ptpClockIdentityAssign(&(PAYLOAD(msg)->_grandmasterIdentity), &(PAYLOAD(other)->_grandmasterIdentity));

        PAYLOAD(msg)->_currentUtcOffset = PAYLOAD(other)->_currentUtcOffset;
        PAYLOAD(msg)->_grandmasterPriority1 = PAYLOAD(other)->_grandmasterPriority1;
        PAYLOAD(msg)->_grandmasterPriority2 = PAYLOAD(other)->_grandmasterPriority2;
        PAYLOAD(msg)->_stepsRemoved = PAYLOAD(other)->_stepsRemoved;
        PAYLOAD(msg)->_timeSource = PAYLOAD(other)->_timeSource;
    }
}

Boolean ptpMsgAnnounceCompare(MsgAnnounceRef msg, MsgAnnounceRef other)
{
    assert(msg & other);

    if (PAYLOAD(msg)->_grandmasterPriority1 != PAYLOAD(other)->_grandmasterPriority1)
        return false;

    if (ptpClockQualityNotEqualTo(&(PAYLOAD(msg)->_grandmasterClockQuality), &(PAYLOAD(other)->_grandmasterClockQuality)))
        return false;

    if (PAYLOAD(msg)->_grandmasterPriority2 != PAYLOAD(other)->_grandmasterPriority2)
        return false;

    if (ptpClockIdentityNotEqualTo(&(PAYLOAD(msg)->_grandmasterIdentity), &(PAYLOAD(other)->_grandmasterIdentity)))
        return false;

    if (PAYLOAD(msg)->_stepsRemoved != PAYLOAD(other)->_stepsRemoved)
        return false;

    return true;
}

// Gets grandmaster's priority1 value
unsigned char ptpMsgAnnounceGetGrandmasterPriority1(MsgAnnounceRef msg)
{
    assert(msg);
    return PAYLOAD(msg)->_grandmasterPriority1;
}

// Gets grandmaster's priority2 value
unsigned char ptpMsgAnnounceGetGrandmasterPriority2(MsgAnnounceRef msg)
{
    assert(msg);
    return PAYLOAD(msg)->_grandmasterPriority2;
}

// Gets grandmaster clock quality
ClockQualityRef ptpMsgAnnounceGetGrandmasterClockQuality(MsgAnnounceRef msg)
{
    assert(msg);
    return &(PAYLOAD(msg)->_grandmasterClockQuality);
}

// Gets the steps removed value. See IEEE 802.1AS-2011 Clause 10.3.3
uint16_t ptpMsgAnnounceGetStepsRemoved(MsgAnnounceRef msg)
{
    assert(msg);
    return PAYLOAD(msg)->_stepsRemoved;
}

// Gets grandmaster identity value
void ptpMsgAnnounceGetGrandmasterIdentityBytes(MsgAnnounceRef msg, char* identity)
{
    assert(msg && identity);
    ptpClockIdentityGetBytes(&(PAYLOAD(msg)->_grandmasterIdentity), (uint8_t*)identity);
}

PortIdentityRef ptpMsgAnnounceGetPortIdentity(MsgAnnounceRef msg)
{
    return ptpMsgCommonGetPortIdentity(BASE_CLASS_REF(msg));
}

// Gets grandmaster's clockIdentity value
ClockIdentityRef ptpMsgAnnounceCopyGrandmasterClockIdentity(MsgAnnounceRef msg)
{
    assert(msg);

    ClockIdentityRef identity = ptpRefClockIdentityAlloc();
    ptpClockIdentityAssign(identity, &(PAYLOAD(msg)->_grandmasterIdentity));
    return identity;
}

TimestampRef ptpMsgAnnounceRxTimestamp(MsgAnnounceRef msg)
{
    assert(msg);
    return &(msg->_rxTimestamp);
}

Boolean ptpMsgAnnounceHasExpired(MsgAnnounceRef msg, TimestampRef now)
{
#define NSEC2SEC 1000000000LL

    double t1, t2, tmo;
    assert(msg && now);

    t1 = TIMESTAMP_TO_NANOSECONDS(ptpMsgAnnounceRxTimestamp(msg));
    t2 = TIMESTAMP_TO_NANOSECONDS(now);

    if (BASE_CLASS_REF(msg)->_logMeanMsgInterval < -63) {
        tmo = 0;
    } else if (BASE_CLASS_REF(msg)->_logMeanMsgInterval > 31) {
        tmo = INT64_MAX;
    } else if (BASE_CLASS_REF(msg)->_logMeanMsgInterval < 0) {
        tmo = 4LL * NSEC2SEC / (1 << -BASE_CLASS_REF(msg)->_logMeanMsgInterval);
    } else {
        tmo = 4LL * (1 << BASE_CLASS_REF(msg)->_logMeanMsgInterval) * NSEC2SEC;
    }

    //Allow msg received within 3s.
    if (tmo <= 1000000000LL){
        tmo = 3 * tmo;
    }

    if ((t2 - t1) >= tmo)
        return true;

    return false;
}

ClockDatasetRef ptpMsgAnnounceGetDataset(MsgAnnounceRef msg, NetworkPortRef port)
{
    assert(msg && port);

    ClockDatasetRef ds = ptpClockDatasetCreate();
    ds->_priority1 = ptpMsgAnnounceGetGrandmasterPriority1(msg);
    ds->_priority2 = ptpMsgAnnounceGetGrandmasterPriority2(msg);
    ds->_stepsRemoved = ptpMsgAnnounceGetStepsRemoved(msg);

    ClockIdentityRef gmClock = ptpMsgAnnounceCopyGrandmasterClockIdentity(msg);
    ptpClockIdentityAssign(&(ds->_identity), gmClock);
    ptpRefClockIdentityRelease(gmClock);

    ptpClockQualityAssign(&(ds->_quality), ptpMsgAnnounceGetGrandmasterClockQuality(msg));

    ptpPortIdentityAssign(&(ds->_sender), ptpMsgAnnounceGetPortIdentity(msg));
    ptpPortIdentityAssign(&(ds->_receiver), ptpNetPortGetPortIdentity(port));
    return ds;
}

// Processes PTP message
void ptpMsgAnnounceProcess(MsgAnnounceRef msg, NetworkPortRef port)
{
    ClockCtxRef clock;
    assert(msg && port);

    if (ptpPortIdentityEqualsTo(ptpNetPortGetPortIdentity(port), ptpMsgCommonGetPortIdentity(BASE_CLASS_REF(msg)))) {

        // Ignore messages from self
        return;
    }

    clock = ptpNetPortGetClock(port);

    // Delete announce receipt timeout
    ptpNetPortStopAnnounceReceiptTimer(port);

    if (PAYLOAD(msg)->_stepsRemoved < 255) {

        // Reject Announce message from myself
        ClockIdentityRef ourClockIdentity = ptpClockCtxGetClockIdentity(clock);

        // is this a new announce?
        if (!ptpClockIdentityEqualsTo(ptpPortIdentityGetClockIdentity(ptpMsgAnnounceGetPortIdentity(msg)), ourClockIdentity) || ptpPathTraceTlvHasClockIdentity(&(msg->_tlv), ourClockIdentity)) {

            // Process received announce message on the port
            ptpNetPortAnnounceReceived(port, msg);
        }
    }

    // schedule announce receipt timeout
    ptpNetPortStartAnnounceReceiptTimer(port, (uint64_t)(ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER * (pow((double)2, ptpNetPortGetAnnounceInterval(port)) * 1000000000.0)));
}

// Assembles PTPMessageAnnounce message on the EtherPort payload
void ptpMsgAnnounceSendPort(MsgAnnounceRef msg, NetworkPortRef port, PortIdentityRef destIdentity)
{
    assert(msg && port);

    // we will try to use stack based buffer whenever possible to reduce memory fragmentation:
    uint8_t buf[sizeof(MsgAnnouncePayload) + 4 * sizeof(PathTraceTLV)];
    uint8_t* outMsg = buf;

    MsgAnnouncePayload* payload = (MsgAnnouncePayload*)buf;
    memcpy(payload, PAYLOAD(msg), sizeof(MsgAnnouncePayload));

    // set length (this is dynamic)
    BASE_CLASS_REF(msg)->_messageLength = (uint16_t)(sizeof(MsgAnnouncePayload) + ptpPathTraceTlvLength(&(msg->_tlv)));

    // use dynamic memory in case our stack based buffer is not large enough:
    if (BASE_CLASS_REF(msg)->_messageLength > sizeof(buf)) {
        outMsg = (uint8_t*)malloc(BASE_CLASS_REF(msg)->_messageLength);
    }

    // set common header
    ptpMsgCommonBuildHeader(BASE_CLASS_REF(msg), outMsg);

    // convert properties from host order to network order
    ptpClockQualityToNetworkOrder(&(payload->_grandmasterClockQuality));
    payload->_currentUtcOffset = ptpHtonss(payload->_currentUtcOffset);
    payload->_stepsRemoved = ptpHtonsu(payload->_stepsRemoved);

    // serialize tlv
    ptpPathTraceTlvToByteString(&(msg->_tlv), outMsg + sizeof(MsgAnnouncePayload));
    ptpMsgAnnounceVerboseLog(msg);

    Timestamp timestamp;
    ptpNetPortSend(port, outMsg, BASE_CLASS_REF(msg)->_messageLength, destIdentity, &timestamp, false, true);

    char addrStr[ADDRESS_STRING_LENGTH] = { 0 };
    PTP_LOG_DEBUG("==================================================================");
    PTP_LOG_STATUS("Sent Announce message (seq = %d, tx time = %lf s) to %s", BASE_CLASS_REF(msg)->_sequenceId, TIMESTAMP_TO_SECONDS(&timestamp), ptpNetPortIdentityToAddressStr(port, destIdentity, addrStr, sizeof(addrStr)));
    PTP_LOG_DEBUG("==================================================================");

    // release buffer if it was dynamically allocated
    if (outMsg != buf) {
        free(outMsg);
    }
}

void ptpMsgAnnounceVerboseLog(MsgAnnounceRef msg)
{
    assert(msg);
#if PTP_LOG_VERBOSE_ON
    ClockIdentityRef grandmasterClock = ptpMsgAnnounceCopyGrandmasterClockIdentity(msg);

    PTP_LOG_VERBOSE("grandmasterIdentity\t\t\t: %016llx", ptpClockIdentityGetUInt64(grandmasterClock));
    ptpRefClockIdentityRelease(grandmasterClock);

    PTP_LOG_VERBOSE("grandmasterClockQuality.cq_class\t\t: %d", PAYLOAD(msg)->_grandmasterClockQuality._class);
    PTP_LOG_VERBOSE("grandmasterClockQuality.clockAccuracy\t: %d ", PAYLOAD(msg)->_grandmasterClockQuality._accuracy);
    PTP_LOG_VERBOSE("grandmasterClockQuality.offsetScaledLogVariance: %d", PAYLOAD(msg)->_grandmasterClockQuality._offsetScaledLogVariance);

    char* identities = ptpPathTraceTlvGetIdentitiesString(&(msg->_tlv));
    PTP_LOG_VERBOSE("tlv type\t\t\t: %d", ptpPathTraceTlvType(&(msg->_tlv)));
    PTP_LOG_VERBOSE("tlv identities\t\t: %s", (identities ? identities : "NULL"));
    if (identities)
        free(identities);

    PTP_LOG_VERBOSE("Utc offset\t\t\t: %d", PAYLOAD(msg)->_currentUtcOffset);
    PTP_LOG_VERBOSE("Time source\t\t\t: %d", PAYLOAD(msg)->_timeSource);
    PTP_LOG_VERBOSE("Steps removed\t\t: %d", PAYLOAD(msg)->_stepsRemoved);
    PTP_LOG_VERBOSE("Grandmaster priority 1\t: %d", PAYLOAD(msg)->_grandmasterPriority1);
    PTP_LOG_VERBOSE("Grandmaster priority 2\t: %d", PAYLOAD(msg)->_grandmasterPriority2);
#else
    (void)msg;
#endif
}
