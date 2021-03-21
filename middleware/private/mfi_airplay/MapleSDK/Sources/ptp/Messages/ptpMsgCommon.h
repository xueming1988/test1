/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_MESSAGE_COMMON_H_
#define _PTP_MESSAGE_COMMON_H_

#include "Common/ptpMutex.h"
#include "Common/ptpPlatform.h"
#include "Common/ptpRefCountedObject.h"
#include "ptpConstants.h"
#include "ptpLinkLayerAddress.h"
#include "ptpPortIdentity.h"
#include "ptpTimestamp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PTPv2 Message Type. IEEE 1588-2008 Clause 13.3.2.2
 */
typedef enum {
    PTP_SYNC_MESSAGE = 0,
    PTP_DELAY_REQ_MESSAGE = 1,
    PTP_FOLLOWUP_MESSAGE = 8,
    PTP_DELAY_RESP_MESSAGE = 9,
    PTP_ANNC_MESSAGE = 0xB,
    PTP_SIGNALLING_MESSAGE = 0xC,
    PTP_MANAGEMENT_MESSAGE = 0xD
} MessageType;

/**
 * @brief PTPv1 Legacy message type
 */
typedef enum {
    SYNC,
    DELAY_REQ,
    FOLLOWUP,
    DELAY_RESP,
    MANAGEMENT,
    MESSAGE_OTHER
} LegacyMessageType;

/**
 * @brief Ptp message header. Matches the PTPv2 spec.
 */
typedef PACKED_STRUCT_BEGIN struct {
    uint8_t _messageType; // messageType
    uint8_t _version; // versionPTP
    uint16_t _messageLength; // PTP message length
    uint8_t _domainNumber; // PTP domain number
    uint8_t _reserved1; // Not used
    uint8_t _flags[2]; // PTP flags field
    int64_t _correctionField; // Correction Field (IEEE 1588-2008 table 21)
    uint32_t _reserved2; // Not used
    PortIdentity _sourcePortIdentity; // PortIdentity from source
    uint16_t _sequenceId; // PTP message sequence ID
    uint8_t _control; // Control message type of LegacyMessageType
    int8_t _logMeanMsgInterval; // LogMessageInterval (IEEE 1588-2008 table 24)
} PACKED_STRUCT_END MsgCommon;

typedef MsgCommon* MsgCommonRef;
typedef struct NetworkPort* NetworkPortRef;

/**
 * @brief Provides a class for building the PTP Delay Response message.
 */
typedef PACKED_STRUCT_BEGIN struct {
    MsgCommon _base;
    Timestamp _requestReceiptTimestamp;
    PortIdentity _requestingPortIdentity;
} PACKED_STRUCT_END MsgDelayResp;

typedef MsgDelayResp* MsgDelayRespRef;

/**
 * @brief MsgCommon constructor. This should be called only on every statically or dynamically allocated MsgCommon object.
 * @param msg reference to MsgCommon object
 * @param payload pointer to array that contains raw message payload
 * @param payloadSize the size of raw message payload
 */
void ptpMsgCommonPayloadCtor(MsgCommonRef msg, const uint8_t* payload, size_t payloadSize);

/**
 * @brief MsgCommon constructor. This should be called only on every statically or dynamically allocated MsgCommon object.
 * @param msg reference to MsgCommon object
 * @param port reference to NetworkPort object that provides all necessary data to construct the message object
 */
void ptpMsgCommonCtor(MsgCommonRef msg);

/**
 * @brief Destructor
 * @param msg reference to MsgCommon object
 */
void ptpMsgCommonDtor(MsgCommonRef msg);

/**
 * @brief Copy payload of source message object to target message object
 * @param msg reference to target MsgCommon object
 * @param other reference to source MsgCommon object
 */
void ptpMsgCommonAssign(MsgCommonRef msg, const MsgCommonRef other);

/**
 * @brief Recalculate time synchronization properties
 * @param msg reference to target MsgCommon object
 * @param port
 */
void ptpMsgCommonCalculate(MsgDelayRespRef delayResponse, NetworkPortRef port);

/**
 * @brief Retrieve PTP message flags
 * @param msg reference to target MsgCommon object
 * @return message flags
 */
unsigned char* ptpMsgCommonGetFlags(MsgCommonRef msg);

/**
 * @brief Get the sequenceId value from a ptp message
 * @param msg reference to target MsgCommon object
 * @return sequence id value
 */
uint16_t ptpMsgCommonGetSequenceId(MsgCommonRef msg);

/**
 * @brief Set the sequence ID value to the PTP message.
 * @param msg reference to target MsgCommon object
 * @param seq new sequence id value
 */
void ptpMsgCommonSetSequenceId(MsgCommonRef msg, uint16_t seq);

/**
 * @brief Get the MessageType field from the PTP message.
 * @param msg reference to target MsgCommon object
 * @return message type value
 */
MessageType ptpMsgCommonGetMessageType(MsgCommonRef msg);

/**
 * @brief Get the transport specific flag from the PTP message
 * @param msg reference to MsgCommon object
 * @return transport specific field
 */
uint8_t ptpMsgCommonGetTransportSpecific(MsgCommonRef msg);

/**
 * @brief Check if message is of event type
 * @param msg reference to target MsgCommon object
 * @return true if the message is an event message, false otherwise
 */
Boolean ptpMsgCommonIsEvent(MsgCommonRef msg);

/**
 * @brief Get the correctionField value
 * @param msg reference to target MsgCommon object
 * @return value of correction field property
 */
int64_t ptpMsgCommonGetCorrectionField(MsgCommonRef msg);

/**
 * @brief Set the correction field.
 * @param msg reference to target MsgCommon object
 * @param correctionAmount new value of correction field
 */
void ptpMsgCommonSetCorrectionField(MsgCommonRef msg, long long correction);

/**
 * @brief Get internal PortIdentity object
 * @param msg reference to target MsgCommon object
 * @return reference to internal PortIdentity object
 */
PortIdentityRef ptpMsgCommonGetPortIdentity(MsgCommonRef msg);

/**
 * @brief Set internal PortIdentity value
 * @param msg reference to target MsgCommon object
 * @param identity reference to source PortIdentity object whose value will be copied to the internal PortIdentity object
 */
void ptpMsgCommonAssignPortIdentity(MsgCommonRef msg, PortIdentityRef identity);

/**
 * @brief Process received message
 * @param msg reference to MsgCommon object
 * @param ingressTime message reception time
 * @param port reference to NetworkPort object which received this message
 */
void ptpMsgCommonProcessMessage(MsgCommonRef msg, TimestampRef ingressTime, NetworkPortRef port);

/**
 * @brief Build PTP common header
 * @param msg reference to target MsgCommon object
 * @param buf target buffer where the message payload will be stored. It must be at least sizeof(MsgCommon) bytes long!
 */
void ptpMsgCommonBuildHeader(MsgCommonRef msg, uint8_t* buf);

/**
 * @brief Allocate new message object from provided raw data
 * @param buf payload buffer
 * @param size size of payload buffer
 * @param remote remote address of device which sent the message
 * @param port port object that retrieved this message
 * @param ingressTime message reception time
 * @return reference to dynamically allocated MsgCommon object
 */
MsgCommonRef ptpMsgCommonBuildPTPMessage(const uint8_t* buf, size_t size, const LinkLayerAddressRef remote, const NetworkPortRef port, const ClockIdentityRef expectedClockIdentity, const TimestampRef ingressTime);

/**
 * @brief ptpMsgCommonDumpHeader
 * @param msg reference to target MsgCommon object
 */
void ptpMsgCommonDumpHeader(MsgCommonRef msg);

#ifdef __cplusplus
}
#endif

#endif // _PTP_MESSAGE_COMMON_H_
