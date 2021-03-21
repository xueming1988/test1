/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpLinkLayerAddress.h"
#include "Common/ptpLog.h"
#include "Common/ptpMemoryPool.h"
#include "Common/ptpPlatform.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

DECLARE_MEMPOOL(LinkLayerAddress)
DEFINE_MEMPOOL(LinkLayerAddress, ptpLinkLayerAddressCtor, ptpLinkLayerAddressDtor)

// dynamically allocate LinkLayerAddress object
LinkLayerAddressRef ptpLinkLayerAddressCreate()
{
    return ptpMemPoolLinkLayerAddressAllocate();
}

// deallocate dynamically allocated LinkLayerAddress object
void ptpLinkLayerAddressDestroy(LinkLayerAddressRef addr)
{
    ptpMemPoolLinkLayerAddressDeallocate(addr);
}

static void ptpListLinkLayerAddressObjectDtor(LinkLayerAddressRef addr)
{
    ptpLinkLayerAddressDestroy(addr);
}

DEFINE_LIST(LinkLayerAddress, ptpListLinkLayerAddressObjectDtor)

// LinkLayerAddress default constructor. This has to be explicitly called
// only on a statically allocated LinkLayerAddress object.
void ptpLinkLayerAddressCtor(LinkLayerAddressRef address)
{
    assert(address);

    address->_ipVersion = 0;
#if !defined(AIRPLAY_IPV4_ONLY)
    memset(address->_ipV6Address, 0, sizeof(address->_ipV6Address));
#endif
    address->_ipV4Address = 0;
    address->_port = 0;
}

void ptpLinkLayerAddressCtorEx(LinkLayerAddressRef address, const char* addrString, uint16_t port)
{
    int ok;
    assert(address && addrString);
    address->_port = port;

#if !defined(AIRPLAY_IPV4_ONLY)
    memset(address->_ipV6Address, 0, sizeof(address->_ipV6Address));
    address->_ipV4Address = 0;
    if (strchr(addrString, ':')) {
        struct in6_addr dest;
        address->_ipVersion = 6;
        ok = inet_pton(AF_INET6, addrString, &dest);
        memcpy(address->_ipV6Address, dest.s6_addr, sizeof(dest.s6_addr));
    } else
#endif
    {
        address->_ipVersion = 4;
        struct sockaddr_in dest;
        ok = inet_pton(AF_INET, addrString, &(dest.sin_addr));
        address->_ipV4Address = dest.sin_addr.s_addr;
    }

    PTP_LOG_VERBOSE("ip = %s, port = %d, version: %d", (!addrString ? "EMPTY" : addrString), address->_port, address->_ipVersion);

    if (0 == ok) {
        PTP_LOG_ERROR("Error constructing LinkLayerAddress: %s does not contain a valid address string.", addrString);
    } else if (-1 == ok) {
        PTP_LOG_ERROR("Error constructing LinkLayerAddress address family error: %#m", errno);
    }
}

void ptpLinkLayerAddressDtor(LinkLayerAddressRef address)
{
    assert(address);

    (void)address;
    // empty
}

void ptpLinkLayerAddressAssignSockaddr(LinkLayerAddressRef address, const struct sockaddr* other)
{
    assert(address);

    if (AF_INET == other->sa_family) {

        char buf[INET_ADDRSTRLEN];
        struct in_addr source;

        address->_ipVersion = 4;
        const struct sockaddr_in* dest = (const struct sockaddr_in*)other;
        address->_ipV4Address = dest->sin_addr.s_addr;

        source.s_addr = address->_ipV4Address;
        inet_ntop(AF_INET, &source, buf, sizeof(buf));

        PTP_LOG_VERBOSE("LinkLayerAddress::LinkLayerAddress  struct sockaddr  ipv4  rawAddress:%s", buf);
    }
#if !defined(AIRPLAY_IPV4_ONLY)
    else if (AF_INET6 == other->sa_family) {

        address->_ipVersion = 6;
        const struct sockaddr_in6* dest = (const struct sockaddr_in6*)other;
        //ok = inet_pton(AF_INET6, other->sa_data, &(dest->sin6_addr));
        memcpy(address->_ipV6Address, dest->sin6_addr.s6_addr, sizeof(dest->sin6_addr.s6_addr));

        char buf[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &(dest->sin6_addr.s6_addr), buf, sizeof(buf));
        PTP_LOG_VERBOSE("LinkLayerAddress::LinkLayerAddress  struct sockaddr  ipv6  rawAddress:%s", buf);
    }
#endif
    else {
        PTP_LOG_ERROR("Error constructing LinkLayAddress from struct sockaddr, "
                      "invalid family %d",
            other->sa_family);
    }

    char addrStr[ADDRESS_STRING_LENGTH] = { 0 };
    ptpLinkLayerAddressString(address, addrStr, sizeof(addrStr));
    PTP_LOG_VERBOSE("LinkLayerAddress::LinkLayerAddress  struct sockaddr  address:%s", addrStr);
}

void ptpLinkLayerAddressAssignSockaddrIn(LinkLayerAddressRef address, const struct sockaddr_in* other)
{
    char buf[INET_ADDRSTRLEN];
    const char* resultBuf = NULL;

    assert(address);

#if !defined(AIRPLAY_IPV4_ONLY)
    memset(address->_ipV6Address, 0, sizeof(address->_ipV6Address));
#endif

    address->_ipVersion = 4;

    resultBuf = inet_ntop(other->sin_family, &other->sin_addr, buf, sizeof(buf));
    if (NULL == resultBuf) {
        PTP_LOG_ERROR("Error constructing LinkLayAddress from struct sockaddr_in.");
    } else {
        address->_ipV4Address = other->sin_addr.s_addr;
    }
}

#if !defined(AIRPLAY_IPV4_ONLY)
/**
 * @brief Convert 'other' to 'address' and store text representation of 'other' into provided buffer
 * @param address [out] reference to target LinkLayerAddress object
 * @param other pointer to the source socket address
 * @param addrStr [out] output text buffer
 * @param addrStrLen length of the output text buffer
 */
static void ptpLinkLayerAddressFixBSDLinkLocal(LinkLayerAddressRef address, const struct sockaddr_in6* other, char* addrStr, size_t addrStrLen)
{
    assert(address && addrStr && (addrStrLen >= ADDRESS_STRING_LENGTH));

    (void)addrStrLen;

    // convert 'other' to string
    inet_ntop(AF_INET6, &(other->sin6_addr), (void*)addrStr, INET6_ADDRSTRLEN);

    // On BSD based platforms, the scope ID is embedded into the address
    // itself for link local addresses as the second 16 bit word so we need
    // to remove it - it can't be used on POSIX conformant platforms.
    // !!!...
    // This assumes link local addresses, all bets are off for global
    // addresses.
    // ...!!!
    char* pos = strchr(addrStr, ':');

    if ((pos != NULL) && (!strncmp(addrStr, "fe80", 4) || !strncmp(addrStr, "FE80", 4)) && (pos[1] != ':')) {

        // skip the embedded scope ID
        char* rest = strchr(pos + 1, ':');

        if (rest)
            memmove(pos, rest, strlen(rest) + 1);

        struct in6_addr dest;
        inet_pton(AF_INET6, addrStr, &dest);
        memcpy(address->_ipV6Address, dest.s6_addr, sizeof(dest.s6_addr));
    } else {
        memcpy(address->_ipV6Address, other->sin6_addr.s6_addr, sizeof(other->sin6_addr.s6_addr));
    }

    PTP_LOG_VERBOSE("ptpLinkLayerAddressFixBSDLinkLocal address = %s", addrStr);
}

void ptpLinkLayerAddressAssignSockaddrIn6(LinkLayerAddressRef address, const struct sockaddr_in6* other)
{
    assert(address);

    memset(address->_ipV6Address, 0, sizeof(address->_ipV6Address));

    if (!IN6_IS_ADDR_V4MAPPED(&(other->sin6_addr))) {
        address->_ipV4Address = 0;
        address->_ipVersion = 6;

        char buf[INET6_ADDRSTRLEN];
        const char* resultBuf = inet_ntop(other->sin6_family, &other->sin6_addr, buf, sizeof(buf));
        if (NULL == resultBuf) {
            PTP_LOG_ERROR("Error constructing LinkLayAddress from struct sockaddr_in6.");
        } else {
            PTP_LOG_VERBOSE("LinkLayerAddress ipv6 address = %s, sin6_port: %d, sin6_family: %d", buf, other->sin6_port, other->sin6_family);
            ptpLinkLayerAddressFixBSDLinkLocal(address, other, buf, sizeof(buf));
        }
    } else {
        uint8_t* ptr = (uint8_t*)&(other->sin6_addr.s6_addr[12]);
        address->_ipV4Address = *((uint32_t*)ptr);
        address->_ipVersion = 4;
    }
}

static Boolean ptpLinkLayerAddressIsIpv6Wildcard(LinkLayerAddressRef address)
{
    assert(address);

    const uint16_t* words = (uint16_t*)(address->_ipV6Address);
    return words[0] == 0 && words[1] == 0 && words[2] == 0 && words[3] == 0 && words[4] == 0 && words[5] == 0 && words[6] == 0 && words[7] == 0;
}
#endif

Boolean ptpLinkLayerAddressIsValid(LinkLayerAddressRef address)
{
    assert(address);

    // in case of ipv4, the address is considered to be valid, if it is non-zero
    if (address->_ipVersion == 4) {
        return (address->_ipV4Address != 0);
    }
#if !defined(AIRPLAY_IPV4_ONLY)
    else if (address->_ipVersion == 6) {

        if (ptpLinkLayerAddressIsIpv6Wildcard(address))
            return false;

        return true;
    }
#endif
    else {
        return false;
    }
}

void ptpLinkLayerAddressAssign(LinkLayerAddressRef address, const LinkLayerAddressRef other)
{
    assert(address);

    if (address != other) {

#if !defined(AIRPLAY_IPV4_ONLY)
        memcpy(address->_ipV6Address, other->_ipV6Address, sizeof(address->_ipV6Address));
#endif

        address->_ipV4Address = other->_ipV4Address;
        address->_port = other->_port;
        address->_ipVersion = other->_ipVersion;
    }
}

// compares 'address' with 'cmp' and returns true if they are equal, false otherwise
Boolean ptpLinkLayerAddressEqualsTo(LinkLayerAddressRef address, const LinkLayerAddressRef cmp)
{
    Boolean equalsTo = false;

    assert(address);
    assert(cmp);

#if !defined(AIRPLAY_IPV4_ONLY)
    if (address->_ipVersion == cmp->_ipVersion) {
        equalsTo = address->_ipVersion == 4 ? address->_ipV4Address == cmp->_ipV4Address : memcmp(address->_ipV6Address, cmp->_ipV6Address, IPV6_ADDR_OCTETS) == 0;
    }
#else
    equalsTo = address->_ipV4Address == cmp->_ipV4Address;
#endif

    return equalsTo && (address->_port == cmp->_port || address->_port == 0 || cmp->_port == 0);
}

void ptpLinkLayerAddressGetAddress(LinkLayerAddressRef address, struct in_addr* dest)
{
    assert(address);

#if !defined(AIRPLAY_IPV4_ONLY)
    if (address->_ipVersion != 4) {
        PTP_LOG_ERROR("Invalid coversion...This address is version %d but converting to 4", address->_ipVersion);
        return;
    }
#endif

    // Convert the raw values to a string for use with inet_pton
    char addrStr[ADDRESS_STRING_LENGTH] = { 0 };
    ptpLinkLayerAddressString(address, addrStr, sizeof(addrStr));

    // Convert the address from string
    inet_pton(AF_INET, addrStr, dest);
}

#if !defined(AIRPLAY_IPV4_ONLY)
void ptpLinkLayerAddressGetAddress6(LinkLayerAddressRef address, struct in6_addr* dest)
{
    assert(address);

    char addrStr[ADDRESS_STRING_LENGTH] = { 0 };
    ptpLinkLayerAddressString(address, addrStr, sizeof(addrStr));
    PTP_LOG_VERBOSE("address->_ipVersion: %d, address:%s", address->_ipVersion, addrStr);

    // Convert the address from string
    inet_pton(AF_INET6, addrStr, dest);
}
#endif

uint16_t ptpLinkLayerAddressGetPort(LinkLayerAddressRef address)
{
    assert(address);
    return address->_port;
}

void ptpLinkLayerAddressSetPort(LinkLayerAddressRef address, uint16_t port)
{
    assert(address);
    address->_port = port;
}

char* ptpLinkLayerAddressString(LinkLayerAddressRef address, char* addrStr, size_t addrStrLen)
{
    assert(address && addrStr && (addrStrLen >= ADDRESS_STRING_LENGTH));
    (void)addrStrLen;

    if (4 == address->_ipVersion) {

        struct in_addr source;
        source.s_addr = address->_ipV4Address;
        inet_ntop(AF_INET, &source, (char*)addrStr, INET_ADDRSTRLEN);
    }
#if !defined(AIRPLAY_IPV4_ONLY)
    else {

        // get sockaddr_in6 copy
        struct sockaddr_in6 other;
        other.sin6_family = AF_INET6;
        other.sin6_flowinfo = 0;
        other.sin6_port = address->_port;
        memcpy(other.sin6_addr.s6_addr, address->_ipV6Address, sizeof(address->_ipV6Address));

        // fix linklocal issues
        ptpLinkLayerAddressFixBSDLinkLocal(address, &other, addrStr, addrStrLen);
    }
#endif
    return addrStr;
}

int ptpLinkLayerAddressIpVersion(LinkLayerAddressRef address)
{
    assert(address);
    return address->_ipVersion;
}
