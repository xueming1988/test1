/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_MAC_ADDRESS_H_
#define _PTP_MAC_ADDRESS_H_

#include "ptpConstants.h"
#include <stdint.h>

#if !TARGET_OS_BRIDGECO
#include <net/if.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_ADDRESS_LENGTH 14

typedef struct {
    uint8_t _content[MAX_ADDRESS_LENGTH];
} MacAddress;

typedef MacAddress* MacAddressRef;

/**
 * @brief ptpMacAddressInit
 * @param addr reference to MacAddress object
 * @param device pointer to the interface request structure which contains the source mac address data
 */
void ptpMacAddressInit(MacAddressRef addr, const struct ifreq* device);

/**
 * @brief assign content of src address to dst address
 * @param dst reference to target MacAddress object
 * @param src reference to source MacAddress object
 */
void ptpMacAddressAssign(MacAddressRef dst, MacAddressRef src);

/**
 * @brief copy raw address data to provided destination array
 * @param addr reference to MacAddress object
 * @param dest [out] pointer to output array where the MacAddress payload will be stored
 * @param size length of output array
 */
void ptpMacAddressCopy(const MacAddressRef addr, uint8_t* dest, size_t size);

#ifdef __cplusplus
}
#endif

#endif // _PTP_MAC_ADDRESS_H_
