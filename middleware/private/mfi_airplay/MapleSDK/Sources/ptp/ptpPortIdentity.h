/*
 	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
 */

#ifndef _PTP_PORT_IDENTITY_H_
#define _PTP_PORT_IDENTITY_H_

#include "ptpClockIdentity.h"
#include "ptpConstants.h"
#include "ptpList.h"
#include "ptpRefCountedObject.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PortIdentity object. Matches the PTPv2 spec
 */
typedef PACKED_STRUCT_BEGIN struct {
    ClockIdentity _clockId;
    uint16_t _portNumber;
} PACKED_STRUCT_END PortIdentity;

typedef PortIdentity* PortIdentityRef;

/**
 * @brief PortIdentity default constructor. This has to be explicitly called only on a statically allocated PortIdentity object.
 * @param identity reference to PortIdentity object
 */
void ptpPortIdentityCtor(PortIdentityRef identity);

/**
 * @brief PortIdentity destructor
 * @param identity reference to PortIdentity object
 */
void ptpPortIdentityDtor(PortIdentityRef identity);

/**
 * @brief Convert PortIdentity object from network order to host order
 * @param identity reference to PortIdentity object
 */
void ptpPortIdentityFromNetworkOrder(PortIdentityRef identity);

/**
 * @brief Convert PortIdentity object from host order to network order
 * @param identity reference to PortIdentity object
 */
void ptpPortIdentityToNetworkOrder(PortIdentityRef identity);

/**
 * @brief Assign content of 'other' PortIdentity object to an existing identity object
 * @param identity reference to target PortIdentity object
 * @param other reference of source PortIdentity object
 */
void ptpPortIdentityAssign(PortIdentityRef identity, const PortIdentityRef other);

/**
 * @brief Compare if two PortIdentity objects are equal
 * @param identity reference to PortIdentity object
 * @param cmp reference to other PortIdentity object
 * @return true if both objects are equal, false otherwise
 */
Boolean ptpPortIdentityEqualsTo(PortIdentityRef identity, const PortIdentityRef cmp);

/**
 * @brief Compare if two PortIdentity objects are non-equal
 * @param identity reference to PortIdentity object
 * @param cmp reference to other PortIdentity object
 * @return true if both objects are NOT equal, false otherwise
 */
Boolean ptpPortIdentityNotEqualTo(PortIdentityRef identity, const PortIdentityRef cmp);

/**
 * @brief Return true if 'identity' object is smaller than 'cmp' object, otherwise returns false
 * @param identity reference to PortIdentity object
 * @param cmp reference to other PortIdentity object
 * @return true if 'identity' object is smaller than 'cmp' object
 */
Boolean ptpPortIdentityIsSmallerThan(PortIdentityRef identity, const PortIdentityRef cmp);

/**
 * @brief Return true if 'identity' object is greater than 'cmp' object, otherwise returns false
 * @param identity reference to PortIdentity object
 * @param cmp reference to other PortIdentity object
 * @return true if 'identity' object is greater than 'cmp' object
 */
Boolean ptpPortIdentityIsGreaterThan(PortIdentityRef identity, const PortIdentityRef cmp);

/**
 * @brief Return true if 'identity' object is empty (initialized with default constructor)
 * @param identity reference to PortIdentity object
 * @return true if the identity is empty, false otherwise
 */
Boolean ptpPortIdentityIsEmpty(PortIdentityRef identity);

/**
 * @brief Return true if both objects have the same clocks assigned
 * @param identity reference to PortIdentity object reference to PortIdentity object
 * @param other reference to other PortIdentity object that will be compared with the first one
 * @return true if both objects have the same clocks assigned
 */
Boolean ptpPortIdentityHasSameClocks(PortIdentityRef identity, PortIdentityRef other);

/**
 * @brief Read the clock identity payload into provided 'id' pointer.
 * @param identity reference to PortIdentity object reference to PortIdentity object
 * @param id [out] destination array where the clock identity payload will be stored
 */
void ptpPortIdentityGetClockIdentityBytes(PortIdentityRef identity, uint8_t* id);

/**
 * @brief Update clock identity object with a content from provided 'clockId' object
 * @param identity reference to PortIdentity object reference to PortIdentity object
 * @param clockId reference to the source ClockIdentity object
 */
void ptpPortIdentityAssignClockIdentity(PortIdentityRef identity, const ClockIdentityRef clockId);

/**
 * @brief Retrieve reference to internal ClockIdentity object
 * @param identity reference to PortIdentity object reference to PortIdentity object
 * @return reference to internal ClockIdentity object
 */
ClockIdentityRef ptpPortIdentityGetClockIdentity(PortIdentityRef identity);

/**
 * @brief Retrieve port number
 * @param identity reference to PortIdentity object reference to PortIdentity object
 * @return port number
 */
uint16_t ptpPortIdentityGetPortNumber(PortIdentityRef identity);

/**
 * @brief Set the new port number
 * @param identity reference to PortIdentity object reference to PortIdentity object
 * @param id new port number value
 */
void ptpPortIdentitySetPortNumber(PortIdentityRef identity, uint16_t id);

DECLARE_REFCOUNTED_OBJECT(PortIdentity)

#ifdef __cplusplus
}
#endif

#endif
