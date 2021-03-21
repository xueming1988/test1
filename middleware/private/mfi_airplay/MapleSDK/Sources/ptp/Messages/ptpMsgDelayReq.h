/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_MESSAGE_DELAY_REQ_H_
#define _PTP_MESSAGE_DELAY_REQ_H_

#include "Common/ptpRefCountedObject.h"
#include "ptpMsgCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

// Provides a class for building the PTP Delay Request message
typedef struct {
    MsgCommon _base;
    Timestamp _originTimestamp;
} MsgDelayReqPayload;

typedef struct {

    // PTPv2 payload
    MsgDelayReqPayload _payload;

    // RX/TX timestamp (depending on master/slave mode)
    Timestamp _txTimestamp;
    uint16_t _followUpSeqId;

} MsgDelayReq;

typedef MsgDelayReq* MsgDelayReqRef;

/**
 * @brief MsgDelayReq constructor. This should be called only on every statically or dynamically allocated MsgDelayReq object.
 * @param msg reference to MsgDelayReq object
 * @param payload pointer to array that contains raw message payload
 * @param payloadSize the size of raw message payload
 */
void ptpMsgDelayReqPayloadCtor(MsgDelayReqRef msg, const uint8_t* payload, size_t payloadSize);

/**
 * @brief MsgDelayReq constructor. This should be called only on every statically or dynamically allocated MsgDelayReq object.
 * @param msg reference to MsgDelayReq object
 * @param port reference to NetworkPort object that provides all necessary data to construct the message object
 */
void ptpMsgDelayReqPortCtor(MsgDelayReqRef msg, NetworkPortRef port);

/**
 * @brief Destructor
 * @param msg reference to MsgDelayReq object
 */
void ptpMsgDelayReqDtor(MsgDelayReqRef msg);

/**
 * @brief Get base class object reference
 * @param msg reference to MsgDelayReq object
 * @return reference to base class object (MsgCommonRef)
 */
MsgCommonRef ptpMsgDelayReqGetBase(MsgDelayReqRef msg);

/**
 * @brief Copy payload of source message object to target message object
 * @param msg reference to target MsgDelayReq object
 * @param other reference to source MsgDelayReq object
 */
void ptpMsgDelayReqAssign(MsgDelayReqRef msg, const MsgDelayReqRef other);

/**
 * @brief Process received message
 * @param msg reference to MsgDelayReq object
 * @param ingressTime message reception time
 * @param port reference to NetworkPort object which received this message
 */
void ptpMsgDelayReqProcess(MsgDelayReqRef msg, TimestampRef ingressTime, NetworkPortRef port);

/**
 * @brief Set the origin timestamp
 * @param msg reference to MsgDelayReq object
 * @param timestamp new value of origin timestamp
 */
void ptpMsgDelayReqAssignOriginTimestamp(MsgDelayReqRef msg, TimestampRef timestamp);

/**
 * @brief Get origin timestamp
 * @param msg reference to MsgDelayReq object
 * @return reference to internal origin timestamp object
 */
TimestampRef ptpMsgDelayReqGetOriginTimestamp(MsgDelayReqRef msg);

/**
 * @brief Set the matching followUp sequence id
 * @param msg reference to MsgDelayReq object
 * @param seqId new value of followUp sequence id
 */
void ptpMsgDelayReqSetMatcingFollowUpSeqId(MsgDelayReqRef msg, uint16_t seqId);

/**
 * @brief Get original followUp sequence id
 * @param msg reference to MsgDelayReq object
 * @return reference to internal origin timestamp object
 */
uint16_t ptpMsgDelayReqGetMatcingFollowUpSeqId(MsgDelayReqRef msg);

/**
 * @brief Get transmission timestamp
 * @param msg reference to MsgDelayReq object
 * @return reference to timestamp which specifies the time when this message was sent to the network
 */
TimestampRef ptpMsgDelayReqGetTxTimestamp(MsgDelayReqRef msg);

/**
 * @brief Send MsgDelayReq message to the given NetworkPort
 * @param msg reference to MsgDelayReq object
 * @param port reference to target NetworkPort object
 * @param destIdentity reference to target PortIdentity object (migh be NULL)
 */
Boolean ptpMsgDelayReqSendPort(MsgDelayReqRef msg, NetworkPortRef port, PortIdentityRef destIdentity);

DECLARE_REFCOUNTED_OBJECT(MsgDelayReq)

#ifdef __cplusplus
}
#endif

#endif // _PTP_MESSAGE_DELAY_REQ_H_
