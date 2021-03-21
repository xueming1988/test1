/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_CLOCK_IDENTITY_H_
#define _PTP_CLOCK_IDENTITY_H_

#include "Common/ptpList.h"
#include "Common/ptpRefCountedObject.h"
#include "ptpConstants.h"
#include "ptpMacAddress.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ClockIdentity object. Matches the PTPv2 spec
 */
typedef PACKED_STRUCT_BEGIN struct {
    uint8_t _id[CLOCK_IDENTITY_LENGTH];
} PACKED_STRUCT_END ClockIdentity;

typedef ClockIdentity* ClockIdentityRef;

/**
 * @brief ClockIdentity default constructor. This has to be explicitly called only on a statically allocated ClockIdentity object.
 * @param identity reference to ClockIdentity object
 */
void ptpClockIdentityCtor(ClockIdentityRef identity);

/**
 * @brief ClockIdentity constructor. Initializes ClockIdentity objects using the provided identity payload.
 * @param identity reference to ClockIdentity object
 * @param id pointer to source payload
 */
void ptpClockIdentityIdCtor(ClockIdentityRef identity, uint8_t* id);

/**
 * @brief ClockIdentity destructor
 * @param identity reference to ClockIdentity object
 */
void ptpClockIdentityDtor(ClockIdentityRef identity);

/**
 * @brief Copy payload of source object to target object
 * @param identity reference to target ClockIdentity object
 * @param other reference to source ClockIdentity object
 */
void ptpClockIdentityAssign(ClockIdentityRef identity, const ClockIdentityRef other);

/**
 * @brief Copy mac address into ClockIdentity object
 * @param identity reference to ClockIdentity object
 * @param address reference to source address object
 */
void ptpClockIdentityAssignAddress(ClockIdentityRef identity, const MacAddressRef address);

/**
 * @brief Copy raw data payload into ClockIdentity object
 * @param identity reference to ClockIdentity object
 * @param id raw payload buffer
 */
void ptpClockIdentityAssignBytes(ClockIdentityRef identity, const uint8_t* id);

/**
 * @brief Compare two ClockIdentity objects for equality
 * @param identity reference to ClockIdentity object
 * @param other reference to other ClockIdentity object
 * @return true if both objects are equal, false otherwise
 */
Boolean ptpClockIdentityEqualsTo(ClockIdentityRef identity, const ClockIdentityRef other);

/**
 * @brief Compare two ClockIdentity objects for non-equality
 * @param identity reference to ClockIdentity object
 * @param other reference to other ClockIdentity object
 * @return true if both objects are NOT equal, false otherwise
 */
Boolean ptpClockIdentityNotEqualTo(ClockIdentityRef identity, const ClockIdentityRef other);

/**
 * @brief Check if ClockIdentity object is 'smaller' than other object
 * @param identity reference to ClockIdentity object
 * @param other reference to other ClockIdentity object
 * @return true if 'identity' is smaller than 'other'
 */
Boolean ptpClockIdentityIsSmallerThan(ClockIdentityRef identity, const ClockIdentityRef other);

/**
 * @brief Check if ClockIdentity object is 'greater' than other object
 * @param identity reference to ClockIdentity object
 * @param other reference to other ClockIdentity object
 * @return true if 'identity' is greater than 'other'
 */
Boolean ptpClockIdentityIsGreaterThan(ClockIdentityRef identity, const ClockIdentityRef other);

/**
 * @brief Check if the ClockIdentity object is empty
 * @param identity reference to ClockIdentity object
 * @return true if empty, false otherwise
 */
Boolean ptpClockIdentityIsEmpty(ClockIdentityRef identity);

/**
 * @brief Check if the ClockIdentity object has default value
 * @param identity reference to ClockIdentity object
 * @return true if default, false otherwise
 */
Boolean ptpClockIdentityIsDefault(ClockIdentityRef identity);

/**
 * @brief Retrieve ClockIdentity object as 64bit unsigned integer
 * @param identity reference to ClockIdentity object
 * @return ClockIdentity object as 64bit unsigned integer
 */
uint64_t ptpClockIdentityGet(ClockIdentityRef identity);

/**
 * @brief Retrieve internal ClockIdentity payload
 * @param identity reference to ClockIdentity object
 * @param id [out] target buffer where the payload is stored
 */
void ptpClockIdentityGetBytes(ClockIdentityRef identity, uint8_t* id);

/**
 * @brief Retrieve internal ClockIdentity payload as an uint64
 * @param identity reference to ClockIdentity object
 */
uint64_t ptpClockIdentityGetUInt64(ClockIdentityRef identity);

DECLARE_REFCOUNTED_OBJECT(ClockIdentity)
DECLARE_LIST(ClockIdentity)

#ifdef __cplusplus
}
#endif

#endif // _PTP_CLOCK_IDENTITY_H_
