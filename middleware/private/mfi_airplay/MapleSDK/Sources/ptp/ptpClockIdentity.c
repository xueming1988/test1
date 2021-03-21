/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpClockIdentity.h"
#include "Common/ptpPlatform.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

DEFINE_REFCOUNTED_OBJECT(ClockIdentity, NULL, NULL, ptpClockIdentityCtor, ptpClockIdentityDtor)

static void ptpListClockIdentityObjectDtor(ClockIdentityRef identity)
{
    ptpRefClockIdentityRelease(identity);
}

DEFINE_LIST(ClockIdentity, ptpListClockIdentityObjectDtor)

void ptpClockIdentityIdCtor(ClockIdentityRef identity, uint8_t* id)
{
    assert(identity);
    ptpClockIdentityAssignBytes(identity, id);
}

void ptpClockIdentityCtor(ClockIdentityRef identity)
{
    assert(identity);
    memset(identity->_id, 0, sizeof(identity->_id));
}

void ptpClockIdentityDtor(ClockIdentityRef identity)
{
    if (!identity)
        return;

    // empty
}

void ptpClockIdentityAssign(ClockIdentityRef identity, const ClockIdentityRef other)
{
    assert(identity && other);

    if (identity != other) {
        memcpy(identity->_id, other->_id, sizeof(identity->_id));
    }
}

Boolean ptpClockIdentityEqualsTo(const ClockIdentityRef identity, const ClockIdentityRef other)
{
    assert(identity && other);
    return memcmp(identity->_id, other->_id, sizeof(identity->_id)) == 0;
}

Boolean ptpClockIdentityNotEqualTo(const ClockIdentityRef identity, const ClockIdentityRef other)
{
    assert(identity && other);
    return memcmp(identity->_id, other->_id, sizeof(identity->_id)) != 0;
}

Boolean ptpClockIdentityIsSmallerThan(ClockIdentityRef identity, const ClockIdentityRef other)
{
    assert(identity && other);
    return memcmp(identity->_id, other->_id, sizeof(identity->_id)) < 0 ? true : false;
}

Boolean ptpClockIdentityIsGreaterThan(ClockIdentityRef identity, const ClockIdentityRef other)
{
    assert(identity && other);
    return memcmp(identity->_id, other->_id, sizeof(identity->_id)) > 0 ? true : false;
}

Boolean ptpClockIdentityIsEmpty(ClockIdentityRef identity)
{
    assert(identity);

    uint8_t emptyIdentity[CLOCK_IDENTITY_LENGTH];
    memset(emptyIdentity, 0, sizeof(emptyIdentity));
    return (memcmp(identity->_id, emptyIdentity, sizeof(emptyIdentity)) == 0);
}

Boolean ptpClockIdentityIsDefault(ClockIdentityRef identity)
{
    assert(identity);

    uint8_t defaultIdentity[CLOCK_IDENTITY_LENGTH];
    memset(defaultIdentity, 0xFF, sizeof(defaultIdentity));
    return (memcmp(identity->_id, defaultIdentity, sizeof(defaultIdentity)) == 0);
}

uint64_t ptpClockIdentityGet(ClockIdentityRef identity)
{
    assert(identity);

    // copy clock identity to aligned local variable
    // to make sure that ptpNtohllu() works correctly
    uint64_t out;
    memcpy(&out, identity->_id, sizeof(out));

    // convert identity to host endian and return it
    return ptpNtohllu(out);
}

void ptpClockIdentityGetBytes(ClockIdentityRef identity, uint8_t* id)
{
    assert(identity);
    memcpy(id, identity->_id, sizeof(identity->_id));
}

void ptpClockIdentityAssignBytes(ClockIdentityRef identity, const uint8_t* id)
{
    assert(identity);

    if (id != NULL) {
        memcpy(identity->_id, id, sizeof(identity->_id));
    }
}

void ptpClockIdentityAssignAddress(ClockIdentityRef identity, const MacAddressRef address)
{
    uint64_t tmp1 = 0;
    uint32_t tmp2;

    assert(identity && address);

    ptpMacAddressCopy(address, (uint8_t*)(&tmp1), sizeof(tmp1));
    tmp2 = tmp1 & 0xFFFFFF;
    tmp1 >>= 24;
    tmp1 <<= 16;
    tmp1 |= 0xFEFF;
    tmp1 <<= 24;
    tmp1 |= tmp2;
    memcpy(identity->_id, &tmp1, CLOCK_IDENTITY_LENGTH);
}

uint64_t ptpClockIdentityGetUInt64(ClockIdentityRef identity)
{
    return ptpNtohllu(*(uint64_t*)(identity));
}
