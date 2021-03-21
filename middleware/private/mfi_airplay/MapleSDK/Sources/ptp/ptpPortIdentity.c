/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
 */

#include "ptpPortIdentity.h"
#include "Common/ptpMemoryPool.h"
#include "Common/ptpPlatform.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static MemoryPoolRef _ptpPortIdentityMemPool = NULL;

void* ptpRefPortIdentityAllocateMemFromPool(uint32_t size);
void ptpRefPortIdentityDeallocateMemFromPool(void* ptr);

// callback to allocate required amount of memory
void* ptpRefPortIdentityAllocateMemFromPool(uint32_t size)
{
    // create memory pool object
    ptpMemPoolCreateOnce(&_ptpPortIdentityMemPool, size, NULL, NULL);

    // allocate one element from the memory pool
    return ptpMemPoolAllocate(_ptpPortIdentityMemPool);
}

// callback to release memory allocated via ptpRefAllocateMem callback
void ptpRefPortIdentityDeallocateMemFromPool(void* ptr)
{
    assert(_ptpPortIdentityMemPool);

    // release one element to the memory pool
    if (ptr)
        ptpMemPoolDeallocate(_ptpPortIdentityMemPool, ptr);
}

DEFINE_REFCOUNTED_OBJECT(PortIdentity, ptpRefPortIdentityAllocateMemFromPool, ptpRefPortIdentityDeallocateMemFromPool, ptpPortIdentityCtor, ptpPortIdentityDtor)

// PortIdentity default constructor. This has to be explicitly called
// only on a statically allocated PortIdentity object.
void ptpPortIdentityCtor(PortIdentityRef identity)
{
    assert(identity);

    identity->_portNumber = 0;
    ptpClockIdentityCtor(&(identity->_clockId));
}

void ptpPortIdentityDtor(PortIdentityRef identity)
{
    if (!identity)
        return;

    // empty
}

// convert PortIdentity to/from network order
void ptpPortIdentityFromNetworkOrder(PortIdentityRef identity)
{
    assert(identity);
    identity->_portNumber = ptpNtohsu(identity->_portNumber);
}

void ptpPortIdentityToNetworkOrder(PortIdentityRef identity)
{
    assert(identity);
    identity->_portNumber = ptpHtonsu(identity->_portNumber);
}

// assign content of 'other' PortIdentity object to an existing identity object
void ptpPortIdentityAssign(PortIdentityRef identity, const PortIdentityRef other)
{
    assert(identity && other);

    if (identity != other) {
        ptpClockIdentityAssign(&(identity->_clockId), (ClockIdentityRef) & (other->_clockId));
        identity->_portNumber = other->_portNumber;
    }
}

// operator !=
Boolean ptpPortIdentityNotEqualTo(PortIdentityRef identity, const PortIdentityRef cmp)
{
    assert(identity && cmp);
    return ptpClockIdentityNotEqualTo((ClockIdentityRef) & (identity->_clockId), &(cmp->_clockId)) || (identity->_portNumber != cmp->_portNumber);
}

// operator ==
Boolean ptpPortIdentityEqualsTo(PortIdentityRef identity, const PortIdentityRef cmp)
{
    assert(identity && cmp);
    return ptpClockIdentityEqualsTo((ClockIdentityRef) & (identity->_clockId), &(cmp->_clockId)) && (identity->_portNumber == cmp->_portNumber);
}

// operator <
Boolean ptpPortIdentityIsSmallerThan(PortIdentityRef identity, const PortIdentityRef cmp)
{
    assert(identity && cmp);
    return ptpClockIdentityIsSmallerThan((ClockIdentityRef) & (identity->_clockId), &(cmp->_clockId)) ? true : (ptpClockIdentityEqualsTo((ClockIdentityRef) & (identity->_clockId), &(cmp->_clockId))) && identity->_portNumber < cmp->_portNumber;
}

// operator >
Boolean ptpPortIdentityIsGreaterThan(PortIdentityRef identity, const PortIdentityRef cmp)
{
    assert(identity && cmp);
    return ptpClockIdentityIsGreaterThan((ClockIdentityRef) & (identity->_clockId), &(cmp->_clockId)) ? true : (ptpClockIdentityEqualsTo((ClockIdentityRef) & (identity->_clockId), &(cmp->_clockId))) && identity->_portNumber > cmp->_portNumber;
}

Boolean ptpPortIdentityIsEmpty(PortIdentityRef identity)
{
    assert(identity);
    return (identity->_portNumber == 0) && ptpClockIdentityIsEmpty(&(identity->_clockId));
}

Boolean ptpPortIdentityHasSameClocks(PortIdentityRef identity, PortIdentityRef other)
{
    assert(identity);
    return other && ptpClockIdentityEqualsTo(&(identity->_clockId), ptpPortIdentityGetClockIdentity(other));
}

void ptpPortIdentityGetClockIdentityBytes(PortIdentityRef identity, uint8_t* id)
{
    assert(identity && id);
    ptpClockIdentityGetBytes(&(identity->_clockId), id);
}

void ptpPortIdentityAssignClockIdentity(PortIdentityRef identity, const ClockIdentityRef clockId)
{
    assert(identity && clockId);
    ptpClockIdentityAssign(&(identity->_clockId), clockId);
}

ClockIdentityRef ptpPortIdentityGetClockIdentity(PortIdentityRef identity)
{
    assert(identity);
    return &(identity->_clockId);
}

uint16_t ptpPortIdentityGetPortNumber(PortIdentityRef identity)
{
    assert(identity && id);
    return identity->_portNumber;
}

void ptpPortIdentitySetPortNumber(PortIdentityRef identity, uint16_t id)
{
    assert(identity);
    identity->_portNumber = id;
}
