/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpMacAddress.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

void ptpMacAddressInit(MacAddressRef addr, const struct ifreq* device)
{
    assert(addr);

#ifdef HAVE_STRUCT_IFREQ_IFR_HWADDR
    const uint8_t* sourceMacAddress = (const uint8_t*)(&device->ifr_hwaddr.sa_data);
#else
    const uint8_t* sourceMacAddress = (const uint8_t*)(&device->ifr_addr.sa_data);
#endif

    const uint8_t* kOctetEnd = addr->_content + MAX_ADDRESS_LENGTH;
    const uint8_t* kSourceMacEnd = sourceMacAddress + MAX_ADDRESS_LENGTH;

    uint8_t* begin;

    for (begin = addr->_content; begin < kOctetEnd && sourceMacAddress < kSourceMacEnd; ++begin, ++sourceMacAddress) {
        *begin = *sourceMacAddress;
    }
}

// assign content of src address to dst address
void ptpMacAddressAssign(MacAddressRef dst, MacAddressRef src)
{
    assert(dst && src);
    memcpy(dst->_content, src->_content, MAX_ADDRESS_LENGTH);
}

void ptpMacAddressCopy(const MacAddressRef addr, uint8_t* dest, size_t size)
{
    assert(addr);

    if (dest != NULL) {
        memcpy(dest, addr->_content, size <= MAX_ADDRESS_LENGTH ? size : MAX_ADDRESS_LENGTH);
    }
}
