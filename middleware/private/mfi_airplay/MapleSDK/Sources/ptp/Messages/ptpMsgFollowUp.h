/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_MESSAGE_FOLLOW_UP_H_
#define _PTP_MESSAGE_FOLLOW_UP_H_

#include "Common/ptpRefCountedObject.h"
#include "ptpClockQuality.h"
#include "ptpMsgCommon.h"
#include "ptpMsgSync.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Provides a scaledNs interface
 * The scaledNs type represents signed values of time and time interval in units of 2e-16 ns.
 */
typedef PACKED_STRUCT_BEGIN struct {
    uint16_t _nanosecondsMsb;
    uint64_t _nanosecondsLsb;
    uint16_t _fractionalNanoseconds;
} PACKED_STRUCT_END ScaledNs;

typedef ScaledNs* ScaledNsRef;

/**
 * @brief ScaledNs constructor
 * @param ns reference to ScaledNs object
 */
void ptpScaledNsCtor(ScaledNsRef ns);

/**
 * @brief ScaledNs destructor
 * @param ns reference to ScaledNs object
 */
void ptpScaledNsDtor(ScaledNsRef ns);

/**
 * @brief Copy value of source object to the target object
 * @param ns reference to target ScaledNs object
 * @param other reference to source ScaledNs object
 */
void ptpScaledNsAssign(ScaledNsRef ns, const ScaledNsRef other);

/**
 * @brief Convert internal payload from network order to host order
 * @param ns reference to target ScaledNs object
 */
void ptpScaledNsFromNetworkOrder(ScaledNsRef ns);

/**
 * @brief Convert internal payload from host order to network order
 * @param ns reference to target ScaledNs object
 */
void ptpScaledNsToNetworkOrder(ScaledNsRef ns);

/**
 * @brief Set the lowest 64bits from the ScaledNs object
 * @param ns reference to target ScaledNs object
 * @param lsb new value of bottom 64bits of ScaledNs object
 */
void ptpScaledNsSetNanosecondsLsb(ScaledNsRef ns, uint64_t lsb);

/**
 * @brief Set the highest 16bits of the ScaledNs object
 * @param ns reference to target ScaledNs object
 * @param msb new value of upper 16bits of ScaledNs object
 */
void ptpScaledNsSetNanosecondsMsb(ScaledNsRef ns, uint16_t msb);

/**
 * @brief Set the fractional nanoseconds
 * @param ns reference to target ScaledNs object
 * @param fractionalNs new nanosecond value
 */
void ptpScaledNsSetFractionalNs(ScaledNsRef ns, uint16_t fractionalNs);

/**
 * @brief Provides a follow-up TLV interface back to the previous packing mode
 */
typedef PACKED_STRUCT_BEGIN struct {
    uint16_t _tlvType;
    uint16_t _lengthField;
    uint8_t _organizationId[3];
    uint8_t _organizationSubTypeMs;
    uint16_t _organizationSubTypeLs;
    int32_t _cumulativeScaledRateOffset;
    uint16_t _gmTimeBaseIndicator;
    ScaledNs _scaledLastGmPhaseChange;
    int32_t _scaledLastGmFreqChange;
} PACKED_STRUCT_END FollowUpTlv;

typedef FollowUpTlv* FollowUpTlvRef;

/**
 * @brief FollowUpTlv constructor
 * @param tlv reference to FollowUpTlv object
 */
void ptpFollowUpTlvCtor(FollowUpTlvRef tlv);

/**
 * @brief FollowUpTlv destructor
 * @param tlv reference to FollowUpTlv object
 */
void ptpFollowUpTlvDtor(FollowUpTlvRef tlv);

/**
 * @brief Copy value of source object to the target object
 * @param tlv reference to target FollowUpTlv object
 * @param other reference to source FollowUpTlv object
 */
void ptpFollowUpTlvAssign(FollowUpTlvRef tlv, const FollowUpTlvRef other);

/**
 * @brief Convert internal payload from network order to host order
 * @param tlv reference to target FollowUpTlv object
 */
void ptpFollowUpTlvFromNetworkOrder(FollowUpTlvRef tlv);

/**
 * @brief Convert internal payload from host order to network order
 * @param tlv reference to target FollowUpTlv object
 */
void ptpFollowUpTlvToNetworkOrder(FollowUpTlvRef tlv);

/**
 * @brief Get the cummulative scaledRateOffset
 * @param tlv reference to target FollowUpTlv object
 * @return cummulative rate offset
 */
int32_t ptpFollowUpTlvGetRateOffset(FollowUpTlvRef tlv);

/**
 * @brief Get the gmTimeBaseIndicator
 * @param tlv reference to target FollowUpTlv object
 * @return time base indicator
 */
uint16_t ptpFollowUpTlvGetGmTimeBaseIndicator(FollowUpTlvRef tlv);

/**
 * @brief Update the scaledLastGmFreqChanged private member
 * @param tlv reference to target FollowUpTlv object
 * @param val new value of lastGmFreqChanged property
 */
void ptpFollowUpTlvSetScaledLastGmFreqChange(FollowUpTlvRef tlv, int32_t val);

/**
 * @brief Get the current scaledLastGmFreqChanged value
 * @param tlv reference to target FollowUpTlv object
 * @return value of lastGmFreqChanged property
 */
int32_t ptpFollowUpTlvGetScaledLastGmFreqChange(FollowUpTlvRef tlv);

/**
 * @brief Set the gmTimeBaseIndicator private member
 * @param tlv reference to target FollowUpTlv object
 * @param tbi new value of gmTimeBaseIndicator property
 */
void ptpFollowUpTlvSetGMTimeBaseIndicator(FollowUpTlvRef tlv, uint16_t tbi);

/**
 * @brief Increment time base indicator by one
 * @param tlv reference to target FollowUpTlv object
 */
void ptpFollowUpTlvIncrementGMTimeBaseIndicator(FollowUpTlvRef tlv);

/**
 * @brief Get the current gmTimeBaseIndicator value
 * @param tlv reference to target FollowUpTlv object
 * @return value of gmTimeBaseIndicator property
 */
uint16_t ptpFollowUpTlvGetGMTimeBaseIndicator(FollowUpTlvRef tlv);

/**
 * @brief Set the scaledLastGmPhaseChange private member
 * @param tlv reference to target FollowUpTlv object
 * @param pc new value of scaledLastGmPhaseChange property
 */
void ptpFollowUpTlvAssignScaledLastGmPhaseChange(FollowUpTlvRef tlv, ScaledNsRef pc);

/**
 * @brief Get the scaledLastGmPhaseChange private member value
 * @param tlv reference to target FollowUpTlv object
 * @return value of scaledLastGmPhaseChange property
 */
ScaledNsRef ptpFollowUpTlvGetScaledLastGmPhaseChange(FollowUpTlvRef tlv);

DECLARE_REFCOUNTED_OBJECT(FollowUpTlv)

/**
 * @brief Provides a class for a class for building a PTP follow up message
 */
typedef PACKED_STRUCT_BEGIN struct {
    MsgCommon _base;
    Timestamp _preciseOriginTimestamp;
    FollowUpTlv _tlv;
} PACKED_STRUCT_END MsgFollowUp;

typedef MsgFollowUp* MsgFollowUpRef;

/**
 * @brief MsgFollowUp constructor. This should be called only on every statically or dynamically allocated MsgFollowUp object.
 * @param msg reference to MsgFollowUp object
 * @param payload pointer to array that contains raw message payload
 * @param payloadSize the size of raw message payload
 */
void ptpMsgFollowUpPayloadCtor(MsgFollowUpRef msg, const uint8_t* payload, size_t payloadSize);

/**
 * @brief MsgFollowUp constructor. This should be called only on every statically or dynamically allocated MsgFollowUp object.
 * @param msg reference to MsgFollowUp object
 * @param port reference to NetworkPort object that provides all necessary data to construct the message object
 */
void ptpMsgFollowUpPortCtor(MsgFollowUpRef msg, NetworkPortRef port);

/**
 * @brief Destructor
 * @param msg reference to MsgFollowUp object
 */
void ptpMsgFollowUpDtor(MsgFollowUpRef msg);

/**
 * @brief Get base class object reference
 * @param msg reference to MsgFollowUp object
 * @return reference to base class object (MsgCommonRef)
 */
MsgCommonRef ptpMsgFollowUpGetBase(MsgFollowUpRef msg);

/**
 * @brief Copy payload of source message object to target message object
 * @param msg reference to target MsgFollowUp object
 * @param other reference to source MsgFollowUp object
 */
void ptpMsgFollowUpAssign(MsgFollowUpRef msg, const MsgFollowUpRef other);

/**
 * @brief Send PTPMessageAnnounce message to the given NetworkPort
 * @param msg reference to MsgFollowUp object
 * @param port reference to target NetworkPort object
 * @param destIdentity reference to target PortIdentity object (migh be NULL)
 */
Boolean ptpMsgFollowUpSendPort(MsgFollowUpRef msg, NetworkPortRef port, PortIdentityRef destIdentity);

/**
 * @brief Send PTPMessageDelayRequest message to the given NetworkPort
 * @param msg reference to MsgFollowUp object
 * @param port reference to target NetworkPort object
 * @param destIdentity reference to target PortIdentity object (migh be NULL)
 */
void ptpMsgFollowUpSendDelayRequest(MsgFollowUpRef msg, NetworkPortRef port);

/**
 * @brief Process received message
 * @param msg reference to MsgFollowUp object
 * @param port reference to NetworkPort object which received this message
 */
void ptpMsgFollowUpProcess(MsgFollowUpRef msg, NetworkPortRef port);

/**
 * @brief Get the precise origin timestamp value
 * @param msg reference to MsgFollowUp object
 * @return reference to internal precise origin timestamp
 */
TimestampRef ptpMsgFollowUpGetPreciseOriginTimestamp(MsgFollowUpRef msg);

/**
 * @brief Set the precise origin timestamp value
 * @param msg reference to MsgFollowUp object
 * @param timestamp new value of precise origin timestamp
 */
void ptpMsgFollowUpSetPreciseOriginTimestamp(MsgFollowUpRef msg, TimestampRef timestamp);

/**
 * @brief Get reference to internal MsgFollowUpTlv object
 * @param msg reference to MsgFollowUp object
 * @return reference to internal MsgFollowUpTlv object
 */
FollowUpTlvRef ptpMsgGetFollowUpTlv(MsgFollowUpRef msg);

/**
 * @brief Set the properties of internal FollowUpTlv object from given FollowUpTlv object
 * @param msg reference to target FollowUp object
 * @param fup reference to source FollowUp object
 */
void ptpMsgFollowUpSetClockSourceTime(MsgFollowUpRef msg, FollowUpTlvRef fup);

DECLARE_REFCOUNTED_OBJECT(MsgFollowUp)

#ifdef __cplusplus
}
#endif

#endif // _PTP_MESSAGE_FOLLOW_UP_H_
