/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_MESSAGE_SIGNALLING_H_
#define _PTP_MESSAGE_SIGNALLING_H_

#include "Common/ptpRefCountedObject.h"
#include "ptpClockQuality.h"
#include "ptpMsgCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SIGNALLING_INTERVAL_NO_CHANGE ((uint8_t)-128)

/**
 * @brief Provides a Signalling Msg Interval Request TLV interface
 */
typedef PACKED_STRUCT_BEGIN struct {
    uint16_t _tlvType;
    uint16_t _lengthField;
    uint8_t _organizationId[3];
    uint8_t _organizationSubTypeMs;
    uint16_t _organizationSubTypeLs;
    uint8_t _linkDelayInterval;
    uint8_t _timeSyncInterval;
    uint8_t _announceInterval;
    uint8_t _flags;
    uint16_t _reserved;
} PACKED_STRUCT_END SignallingTlv;

typedef SignallingTlv* SignallingTlvRef;

/**
 * @brief SignallingTlv constructor
 * @param tlv reference to SignallingTlv object
 */
void ptpSignallingTlvCtor(SignallingTlvRef tlv);

/**
 * @brief SignallingTlv destructor
 * @param tlv reference to SignallingTlv object
 */
void ptpSignallingTlvDtor(SignallingTlvRef tlv);

/**
 * @brief Copy value of source object to the target object
 * @param tlv reference to target SignallingTlv object
 * @param other reference to source SignallingTlv object
 */
void ptpSignallingTlvAssign(SignallingTlvRef tlv, const SignallingTlvRef other);

/**
 * @brief Convert internal payload from network order to host order
 * @param tlv reference to target SignallingTlv object
 */
void ptpSignallingTlvFromNetworkOrder(SignallingTlvRef tlv);

/**
 * @brief Convert internal payload from host order to network order
 * @param tlv reference to target SignallingTlv object
 */
void ptpSignallingTlvToNetworkOrder(SignallingTlvRef tlv);

/**
 * @brief Get the link delay interval.
 * @param tlv reference to target SignallingTlv object
 * @return value of link delay interval
 */
uint8_t ptpSignallingTlvGetLinkDelayInterval(SignallingTlvRef tlv);

/**
 * @brief Set the link delay interval.
 * @param tlv reference to target SignallingTlv object
 * @param new value of link delay interval
 */
void ptpSignallingTlvSetLinkDelayInterval(SignallingTlvRef tlv, uint8_t linkDelayInterval);

/**
 * @brief Get the time sync interval.
 * @param tlv reference to target SignallingTlv object
 * @return value of time sync interval
 */
uint8_t ptpSignallingTlvGetTimeSyncInterval(SignallingTlvRef tlv);

/**
 * @brief Set the time sync interval.
 * @param tlv reference to target SignallingTlv object
 * @param new value of time sync interval
 */
void ptpSignallingTlvSetTimeSyncInterval(SignallingTlvRef tlv, uint8_t timeSyncInterval);

/**
 * @brief Get the announce interval.
 * @param tlv reference to target SignallingTlv object
 * @return value of announce interval
 */
uint8_t ptpSignallingTlvGetAnnounceInterval(SignallingTlvRef tlv);

/**
 * @brief Set the announce interval.
 * @param tlv reference to target SignallingTlv object
 * @param new value of announceInterval
 */
void ptpSignallingTlvSetAnnounceInterval(SignallingTlvRef tlv, uint8_t announceInterval);

DECLARE_REFCOUNTED_OBJECT(SignallingTlv)

/**
 * @brief Provides a class for building a PTP signalling message
 */
typedef PACKED_STRUCT_BEGIN struct {
    MsgCommon _base;
    PortIdentity _portIdentity;
    SignallingTlv _tlv;
} PACKED_STRUCT_END MsgSignalling;

typedef MsgSignalling* MsgSignallingRef;

/**
 * @brief MsgSignalling constructor. This should be called only on every statically or dynamically allocated MsgSignalling object.
 * @param msg reference to MsgSignalling object
 * @param payload pointer to array that contains raw message payload
 * @param payloadSize the size of raw message payload
 */
void ptpMsgSignallingPayloadCtor(MsgSignallingRef msg, const uint8_t* payload, size_t payloadSize);

/**
 * @brief MsgSignalling constructor. This should be called only on every statically or dynamically allocated MsgSignalling object.
 * @param msg reference to MsgSignalling object
 * @param port reference to NetworkPort object that provides all necessary data to construct the message object
 */
void ptpMsgSignallingPortCtor(MsgSignallingRef msg, NetworkPortRef port);

/**
 * @brief Destructor
 * @param msg reference to MsgSignalling object
 */
void ptpMsgSignallingDtor(MsgSignallingRef msg);

/**
 * @brief Get base class object reference
 * @param msg reference to MsgSignalling object
 * @return reference to base class object (MsgCommonRef)
 */
MsgCommonRef ptpMsgSignallingGetBase(MsgSignallingRef msg);

/**
 * @brief Copy payload of source message object to target message object
 * @param msg reference to target MsgSignalling object
 * @param other reference to source MsgSignalling object
 */
void ptpMsgSignallingAssign(MsgSignallingRef msg, const MsgSignallingRef other);

/**
 * @brief Send PTPMessageAnnounce message to the given NetworkPort
 * @param msg reference to MsgSignalling object
 * @param port reference to target NetworkPort object
 * @param destIdentity reference to target PortIdentity object (migh be NULL)
 */
Boolean ptpMsgSignallingSendPort(MsgSignallingRef msg, NetworkPortRef port, PortIdentityRef destIdentity);

/**
 * @brief Process received message
 * @param msg reference to MsgSignalling object
 */
void ptpMsgSignallingProcess(MsgSignallingRef msg);

/**
 * @brief Set the signalling intervals
 * @param msg reference to MsgSignalling object
 * @param linkDelayInterval new value of link delay interval
 * @param timeSyncInterval new value of time sync interval
 * @param announceInterval new value of announce interval
 */
void ptpMsgSignallingSetintervals(MsgSignallingRef msg, uint8_t linkDelayInterval, uint8_t timeSyncInterval, uint8_t announceInterval);

DECLARE_REFCOUNTED_OBJECT(MsgSignalling)

#ifdef __cplusplus
}
#endif

#endif // _PTP_MESSAGE_SIGNALLING_H_
