/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_MESSAGE_ANNOUNCE_H_
#define _PTP_MESSAGE_ANNOUNCE_H_

#include "Common/ptpRefCountedObject.h"
#include "ptpClockDataset.h"
#include "ptpClockQuality.h"
#include "ptpMsgCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef PACKED_STRUCT_BEGIN struct {
    uint16_t _tlvType;
    ListClockIdentityRef _identityList;
} PACKED_STRUCT_END PathTraceTLV;

typedef PathTraceTLV* PathTraceTLVRef;

/**
 * @brief PathTraceTLV destructor
 * @param tlv reference to PathTraceTLV object
 */
void ptpPathTraceTlvDtor(PathTraceTLVRef tlv);

/**
 * @brief assign 'other' object to 'tlv'
 * @param tlv reference to target PathTraceTLV object
 * @param other reference to source PathTraceTLV object
 */
void ptpPathTraceTlvAssign(PathTraceTLVRef tlv, const PathTraceTLVRef other);

/**
 * @brief Parses ClockIdentity from message buffer buffer [in] Message buffer
 * @param tlv reference to PathTraceTLV object
 * @param buffer [in] buffer that contains clock identity
 * @param size the length of input buffer. It must be at least PTP_CLOCK_IDENTITY_LENGTH bytes long
 */
void ptpPathTraceTlvParseClockIdentity(PathTraceTLVRef tlv, const uint8_t* buffer, size_t size);

/**
 * @brief Appends new ClockIdentity to internal ClockIdentity list
 * @param tlv reference to PathTraceTLV object
 * @param id reference to ClockIdentity object
 */
void ptpPathTraceTlvAppendClockIdentity(PathTraceTLVRef tlv, ClockIdentityRef id);

/**
 * @brief Gets TLV value in a byte string format
 * @param tlv reference to PathTraceTLV object
 * @param byteStr [out] output buffer where the TLV payload is stored. It has to be at least CLOCK_IDENTITY_LENGTH bytes long
 */
void ptpPathTraceTlvToByteString(PathTraceTLVRef tlv, uint8_t* byteStr);

/**
 * @brief Looks for a specific ClockIdentity on the current TLV
 * @param tlv reference to PathTraceTLV object
 * @param id reference to ClockIdentity to look for
 * @return true if the TLV contains given ClockIdentity object, false otherwise
 */
Boolean ptpPathTraceTlvHasClockIdentity(PathTraceTLVRef tlv, ClockIdentityRef id);

/**
 * @brief Gets the total length of TLV. Total length of TLV is length of type field (UINT16) + length of 'length' field (UINT16) + length of identities (each PTP_CLOCK_IDENTITY_LENGTH) in the path
 * @param tlv reference to PathTraceTLV object
 * @return the length of TLV structure
 */
uint32_t ptpPathTraceTlvLength(PathTraceTLVRef tlv);

/**
 * @brief Returns identities string. The caller has to release returned memory at the end.
 * @param tlv reference to PathTraceTLV object
 * @return malloced() identities string. Caller MUST free() it afterwards
 */
char* ptpPathTraceTlvGetIdentitiesString(PathTraceTLVRef tlv);

/**
 * @brief Get the TLV type
 * @param tlv reference to PathTraceTLV object
 * @return
 */
uint16_t ptpPathTraceTlvType(PathTraceTLVRef tlv);

/**
 * @brief The PTPMessageAnnounce class is used to create announce messages on the 802.1AS format when building the ptp messages.
 */

typedef PACKED_STRUCT_BEGIN struct {
    MsgCommon _base;
    Timestamp _originTimestamp;
    int16_t _currentUtcOffset;
    uint8_t _reserved;
    uint8_t _grandmasterPriority1;
    ClockQuality _grandmasterClockQuality;
    uint8_t _grandmasterPriority2;
    ClockIdentity _grandmasterIdentity;
    uint16_t _stepsRemoved;
    uint8_t _timeSource;
} PACKED_STRUCT_END MsgAnnouncePayload;

typedef struct {

    /**
	 * @brief PTPv2 generic payload
	 */
    MsgAnnouncePayload _payload;

    /**
	 * @brief Reception timestamp
	 */
    Timestamp _rxTimestamp;

    /**
	 * @brief Non-serialized TLV property
	 */
    PathTraceTLV _tlv;

} MsgAnnounce;

typedef MsgAnnounce* MsgAnnounceRef;

/**
 * @brief MsgAnnounce constructor. This should be called only on every statically or dynamically allocated MsgAnnounce object.
 * @param msg reference to MsgAnnounce object
 * @param ingressTime the time when message was received
 * @param payload pointer to array that contains raw message payload
 * @param payloadSize the size of raw message payload
 */
void ptpMsgAnnouncePayloadCtor(MsgAnnounceRef msg, TimestampRef ingressTime, const uint8_t* payload, size_t payloadSize);

/**
 * @brief MsgAnnounce constructor. This should be called only on every statically or dynamically allocated MsgAnnounce object.
 * @param msg reference to MsgAnnounce object
 * @param port reference to NetworkPort object that provides all necessary data to construct the message object
 */
void ptpMsgAnnouncePortCtor(MsgAnnounceRef msg, NetworkPortRef port);

/**
 * @brief Destructor
 * @param msg reference to MsgAnnounce object
 */
void ptpMsgAnnounceDtor(MsgAnnounceRef msg);

/**
 * @brief Get base class object reference
 * @param msg reference to MsgAnnounce object
 * @return reference to base class object (MsgCommonRef)
 */
MsgCommonRef ptpMsgAnnounceGetBase(MsgAnnounceRef msg);

/**
 * @brief Copy payload of source message object to target message object
 * @param msg reference to target MsgAnnounce object
 * @param other reference to source MsgAnnounce object
 */
void ptpMsgAnnounceAssign(MsgAnnounceRef msg, const MsgAnnounceRef other);

/**
 * @brief Compare two announce message objects
 * @param msg reference to source MsgAnnounce object
 * @param other reference to other MsgAnnounce object
 * @return true if MsgAnnounce objects are equal, false otherwise
 */
Boolean ptpMsgAnnounceCompare(MsgAnnounceRef msg, MsgAnnounceRef other);

/**
 * @brief Get portID of the sender of the Announce message
 * @param msg reference to target MsgAnnounce object
 */
PortIdentityRef ptpMsgAnnounceGetPortIdentity(MsgAnnounceRef msg);

/**
 * @brief Get priority1 value from MsgAnnounce object
 * @param msg reference to MsgAnnounce object
 * @return value of priority1 property
 */
unsigned char ptpMsgAnnounceGetGrandmasterPriority1(MsgAnnounceRef msg);

/**
 * @brief Get priority2 value from MsgAnnounce object
 * @param msg reference to MsgAnnounce object
 * @return value of priority2 property
 */
unsigned char ptpMsgAnnounceGetGrandmasterPriority2(MsgAnnounceRef msg);

/**
 * @brief Get grandmaster clock quality
 * @param msg reference to MsgAnnounce object
 * @return reference to internal ClockQuality object
 */
ClockQualityRef ptpMsgAnnounceGetGrandmasterClockQuality(MsgAnnounceRef msg);

/**
 * @brief Gets the steps removed value. See IEEE 802.1AS-2011 Clause 10.3.3
 * @param msg reference to MsgAnnounce object
 * @return the value of 'steps removed' property
 */
uint16_t ptpMsgAnnounceGetStepsRemoved(MsgAnnounceRef msg);

/**
 * @brief Get grandmaster identity value
 * @param msg reference to MsgAnnounce object
 * @param identity [out] target buffer where identity payload is stored
 */
void ptpMsgAnnounceGetGrandmasterIdentityBytes(MsgAnnounceRef msg, char* identity);

/**
 * @brief Gets grandmaster's clockIdentity value
 * @param msg reference to MsgAnnounce object
 * @return reference to internal ClockIdentity object. The caller MUST release this reference afterwards!
 */
ClockIdentityRef ptpMsgAnnounceCopyGrandmasterClockIdentity(MsgAnnounceRef msg);

/**
 * @brief Get reception timestamp
 * @param msg reference to MsgAnnounce object
 * @return reception timestamp
 */
TimestampRef ptpMsgAnnounceRxTimestamp(MsgAnnounceRef msg);

/**
 * @brief Check if the message has expired
 * @param msg reference to MsgAnnounce object
 * @param now reference time to compare to
 * @return true if the message has expired, false otherwise
 */
Boolean ptpMsgAnnounceHasExpired(MsgAnnounceRef msg, TimestampRef now);

/**
 * @brief Retrieve ClockDataset information from provided announce object
 * @param msg reference to MsgAnnounce object
 * @param port reference to NetworkPortRef object
 * @return reference to ClockDataset object
 */
ClockDatasetRef ptpMsgAnnounceGetDataset(MsgAnnounceRef msg, NetworkPortRef port);

/**
 * @brief Process received message
 * @param msg reference to MsgAnnounce object
 * @param port reference to NetworkPort object which received this message
 */
void ptpMsgAnnounceProcess(MsgAnnounceRef msg, NetworkPortRef port);

/**
 * @brief Send PTPMessageAnnounce message to the given NetworkPort
 * @param msg reference to MsgAnnounce object
 * @param port reference to target NetworkPort object
 * @param destIdentity reference to target PortIdentity object (migh be NULL)
 */
void ptpMsgAnnounceSendPort(MsgAnnounceRef msg, NetworkPortRef port, PortIdentityRef destIdentity);

/**
 * @brief Dump content of message to log console
 * @param msg reference to MsgAnnounce object
 */
void ptpMsgAnnounceVerboseLog(MsgAnnounceRef msg);

DECLARE_REFCOUNTED_OBJECT(MsgAnnounce)

#ifdef __cplusplus
}
#endif

#endif // _PTP_MESSAGE_ANNOUNCE_H_
