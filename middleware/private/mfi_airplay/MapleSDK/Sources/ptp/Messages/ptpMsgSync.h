/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_MESSAGE_SYNC_H_
#define _PTP_MESSAGE_SYNC_H_

#include "Common/ptpRefCountedObject.h"
#include "ptpMsgCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Provides a class for building the PTP Sync message
 */
typedef struct {
    MsgCommon _base;
    Timestamp _originTimestamp;
} MsgSyncPayload;

typedef struct {

    /**
	 * @brief PTPv2 payload
	 */
    MsgSyncPayload _payload;

    /**
	 * @brief RX timestamp
	 */
    Timestamp _rxTimestamp;

} MsgSync;

typedef MsgSync* MsgSyncRef;

/**
 * @brief MsgSync constructor. This should be called only on every statically or dynamically allocated MsgSync object.
 * @param msg reference to MsgSync object
 * @param payload pointer to array that contains raw message payload
 * @param payloadSize the size of raw message payload
 */
void ptpMsgSyncPayloadCtor(MsgSyncRef msg, TimestampRef rxTimestamp, const uint8_t* payload, size_t payloadSize);

/**
 * @brief MsgSync constructor. This should be called only on every statically or dynamically allocated MsgSync object.
 * @param msg reference to MsgSync object
 * @param port reference to NetworkPort object that provides all necessary data to construct the message object
 */
void ptpMsgSyncPortCtor(MsgSyncRef msg, NetworkPortRef port);

/**
 * @brief Destructor
 * @param msg reference to MsgSync object
 */
void ptpMsgSyncDtor(MsgSyncRef msg);

/**
 * @brief Get base class object reference
 * @param msg reference to MsgSync object
 * @return reference to base class object (MsgCommonRef)
 */
MsgCommonRef ptpMsgSyncGetBase(MsgSyncRef msg);

/**
 * @brief Copy payload of source message object to target message object
 * @param msg reference to target MsgSync object
 * @param other reference to source MsgSync object
 */
void ptpMsgSyncAssign(MsgSyncRef msg, const MsgSyncRef other);

/**
 * @brief Process received message
 * @param msg reference to MsgSync object
 * @param port reference to NetworkPort object which received this message
 */
void ptpMsgSyncProcess(MsgSyncRef msg, NetworkPortRef port);

/**
 * @brief Get origin timestamp value
 * @param msg reference to MsgSync object
 * @return reference to internal origin timestamp object
 */
TimestampRef ptpMsgSyncGetOriginTimestamp(MsgSyncRef msg);

/**
 * @brief Get reception timestamp
 * @param msg reference to MsgSync object
 * @return reference to internal reception timestamp object
 */
TimestampRef ptpMsgSyncGetRxTimestamp(MsgSyncRef msg);

/**
 * @brief Send PTPMessageAnnounce message to the given NetworkPort
 * @param msg reference to MsgSync object
 * @param port reference to target NetworkPort object
 * @param destIdentity reference to target PortIdentity object (migh be NULL)
 */
Boolean ptpMsgSyncSendPort(MsgSyncRef msg, NetworkPortRef port, PortIdentityRef destIdentity);

DECLARE_REFCOUNTED_OBJECT(MsgSync)

#ifdef __cplusplus
}
#endif

#endif // _PTP_MESSAGE_SYNC_H_
