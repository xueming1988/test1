/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_LINK_LAYER_ADDR_
#define _PTP_LINK_LAYER_ADDR_

#include "Common/ptpList.h"
#include "ptpConstants.h"

#if !TARGET_OS_BRIDGECO
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t _ipV4Address;
#if !defined(AIRPLAY_IPV4_ONLY)
    uint8_t _ipV6Address[IPV6_ADDR_OCTETS];
#endif
    uint16_t _port;
    int _ipVersion;
} LinkLayerAddress;

typedef LinkLayerAddress* LinkLayerAddressRef;

/**
 * @brief allocates memory for a single instance of LinkLayerAddress object
 * @return dynamically allocated LinkLayerAddress object
 */
LinkLayerAddressRef ptpLinkLayerAddressCreate(void);

/**
 * @brief Destroy LinkLayerAddress object
 * @param addr reference to LinkLayerAddress object
 */
void ptpLinkLayerAddressDestroy(LinkLayerAddressRef addr);

/**
 * @brief LinkLayerAddress default constructor. This has to be explicitly called only on a statically allocated LinkLayerAddress object.
 * @param address reference to LinkLayerAddress object
 */
void ptpLinkLayerAddressCtor(LinkLayerAddressRef address);

/**
 * @brief LinkLayerAddress constructor. Initializes LinkLayerAddress object from given .
 * @param address reference to LinkLayerAddress object
 * @param addrString address string
 * @param portNumber port number
 */
void ptpLinkLayerAddressCtorEx(LinkLayerAddressRef address, const char* addrString, uint16_t portNumber);

/**
 * @brief LinkLayerAddress destructor
 * @param address reference to LinkLayerAddress object
 */
void ptpLinkLayerAddressDtor(LinkLayerAddressRef address);

/**
 * @brief Assign sockaddr address to given LinkLayerAddress
 * @param address reference to LinkLayerAddress object
 * @param other pointer to source sockaddr structure
 */
void ptpLinkLayerAddressAssignSockaddr(LinkLayerAddressRef address, const struct sockaddr* other);

/**
 * @brief Assign sockaddr_in address to given LinkLayerAddress
 * @param address reference to LinkLayerAddress object
 * @param other pointer to source sockaddr_in structure
 */
void ptpLinkLayerAddressAssignSockaddrIn(LinkLayerAddressRef address, const struct sockaddr_in* other);
#if !defined(AIRPLAY_IPV4_ONLY)
/**
 * @brief Assign sockaddr_in6 address to given LinkLayerAddress
 * @param address reference to LinkLayerAddress object
 * @param other pointer to source sockaddr_in6 structure
 */
void ptpLinkLayerAddressAssignSockaddrIn6(LinkLayerAddressRef address, const struct sockaddr_in6* other);
#endif

/**
 * @brief Check if the LinkLayerAddress object is valid
 * @param address reference to LinkLayerAddress object
 * @return true if the object is valid, false otherwise
 */
Boolean ptpLinkLayerAddressIsValid(LinkLayerAddressRef address);

/**
 * @brief Copy source LinkLayerAddress object into target LinkLayerAddress object
 * @param address reference to target LinkLayerAddress object
 * @param other reference to source LinkLayerAddress object
 */
void ptpLinkLayerAddressAssign(LinkLayerAddressRef address, const LinkLayerAddressRef other);

/**
 * @brief Compare if two LinkLayerAddress objects are equal
 * @param address reference to LinkLayerAddress object
 * @param cmp reference to other LinkLayerAddress object
 * @return true if both objects are equal, false otherwise
 */
Boolean ptpLinkLayerAddressEqualsTo(LinkLayerAddressRef address, const LinkLayerAddressRef cmp);

/**
 * @brief Retrieve address as in_addr object
 * @param address reference to LinkLayerAddress object
 * @param dest [out] buffer where the address is stored
 */
void ptpLinkLayerAddressGetAddress(LinkLayerAddressRef address, struct in_addr* dest);

#if !defined(AIRPLAY_IPV4_ONLY)
/**
 * @brief Retrieve address as in6_addr object
 * @param address reference to LinkLayerAddress object
 * @param dest [out] buffer where the address is stored
 */
void ptpLinkLayerAddressGetAddress6(LinkLayerAddressRef address, struct in6_addr* dest);
#endif

/**
 * @brief Retrieve port
 * @param address reference to LinkLayerAddress object
 * @return port number
 */
uint16_t ptpLinkLayerAddressGetPort(LinkLayerAddressRef address);

/**
 * @brief Set port
 * @param address reference to LinkLayerAddress object
 * @param number port number
 */
void ptpLinkLayerAddressSetPort(LinkLayerAddressRef address, uint16_t number);

/**
 * @brief Get human readable address string from provided LinkLayerAddress object
 * @param address reference to LinkLayerAddress object
 * @param addrStr [out] output buffer where the address string is stored
 * @param addrStrLen length of output buffer
 * @return pointer to the address string buffer
 */
char* ptpLinkLayerAddressString(LinkLayerAddressRef address, char* addrStr, size_t addrStrLen);

/**
 * @brief Retrieve ip address version
 * @param address reference to LinkLayerAddress object
 * @return 4 (ipv4) or 6 (ipv6)
 */
int ptpLinkLayerAddressIpVersion(LinkLayerAddressRef address);

DECLARE_LIST(LinkLayerAddress)

#ifdef __cplusplus
}
#endif

#endif // _PTP_LINK_LAYER_ADDR_
