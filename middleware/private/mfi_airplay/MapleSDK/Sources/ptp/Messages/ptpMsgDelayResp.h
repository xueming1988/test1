/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_MESSAGE_DELAY_RESP_H_
#define _PTP_MESSAGE_DELAY_RESP_H_

#include "Common/ptpRefCountedObject.h"
#include "ptpMsgCommon.h"
#include "ptpPortIdentity.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MsgDelayResp constructor. This should be called only on every statically or dynamically allocated MsgDelayResp object.
 * @param msg reference to MsgDelayResp object
 * @param payload pointer to array that contains raw message payload
 * @param payloadSize the size of raw message payload
 */
void ptpMsgDelayRespPayloadCtor(MsgDelayRespRef msg, const uint8_t* payload, size_t payloadSize);

/**
 * @brief MsgDelayResp constructor. This should be called only on every statically or dynamically allocated MsgDelayResp object.
 * @param msg reference to MsgDelayResp object
 * @param port reference to NetworkPort object that provides all necessary data to construct the message object
 */
void ptpMsgDelayRespPortCtor(MsgDelayRespRef msg, NetworkPortRef port);

/**
 * @brief Destructor
 * @param msg reference to MsgDelayResp object
 */
void ptpMsgDelayRespDtor(MsgDelayRespRef msg);

/**
 * @brief Get base class object reference
 * @param msg reference to MsgDelayResp object
 * @return reference to base class object (MsgCommonRef)
 */
MsgCommonRef ptpMsgDelayRespGetBase(MsgDelayRespRef msg);

/**
 * @brief Copy payload of source message object to target message object
 * @param msg reference to target MsgDelayResp object
 * @param other reference to source MsgDelayResp object
 */
void ptpMsgDelayRespAssign(MsgDelayRespRef msg, const MsgDelayRespRef other);

/**
 * @brief Process received message
 * @param msg reference to MsgDelayResp object
 * @param port reference to NetworkPort object which received this message
 */
void ptpMsgDelayRespProcess(MsgDelayRespRef msg, NetworkPortRef port);

/**
 * @brief Send PTPMessageAnnounce message to the given NetworkPort
 * @param msg reference to MsgDelayResp object
 * @param port reference to target NetworkPort object
 * @param destIdentity reference to target PortIdentity object (migh be NULL)
 */
Boolean ptpMsgDelayRespSendPort(MsgDelayRespRef msg, NetworkPortRef port, PortIdentityRef destIdentity);

/**
 * @brief Set the request receipt timestamp
 * @param msg reference to MsgDelayResp object
 * @param timestamp
 */
void ptpMsgDelayRespAssignRequestReceiptTimestamp(MsgDelayRespRef msg, TimestampRef timestamp);

/**
 * @brief Get the request receipt timestamp
 * @param msg reference to MsgDelayResp object
 * @return reference to internal request receipt timestamp
 */
TimestampRef ptpMsgDelayRespGetRequestReceiptTimestamp(MsgDelayRespRef msg);

/**
 * @brief Set requesting port identity
 * @param msg reference to MsgDelayResp object
 * @param identity new value of requesting port identity
 */
void ptpMsgDelayRespAssignRequestingPortIdentity(MsgDelayRespRef msg, PortIdentityRef identity);

/**
 * @brief Get requesting port identity
 * @param msg reference to MsgDelayResp object
 * @return reference to internal PortIdentity object
 */
PortIdentityRef ptpMsgDelayRespGetRequestingPortIdentity(MsgDelayRespRef msg);

DECLARE_REFCOUNTED_OBJECT(MsgDelayResp)

#ifdef __cplusplus
}
#endif

#endif // _PTP_MESSAGE_DELAY_RESP_H_
