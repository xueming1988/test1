/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpMemoryPool.h"
#include "AtomicUtils.h"
#include <assert.h>
#include <string.h>

/*
 *  recommended default memory pool block size
 */
#define PTP_MEMORY_POOL_DEFAULT_BLOCK_SIZE 4096

union ptpMemPoolSlot_t {
    void* _element;
    union ptpMemPoolSlot_t* _next;
};

struct ptpMemPool_t {

    union ptpMemPoolSlot_t* _currentMemoryBlob;
    union ptpMemPoolSlot_t* _currentSlot;
    union ptpMemPoolSlot_t* _lastSlot;
    union ptpMemPoolSlot_t* _freeSlots;

    PtpMutexRef _cs;
    size_t _objectSize;
    size_t _blockSize;
    int _allocatedObjects;

    MemPoolObjectCtor _construct;
    MemPoolObjectCtor _destruct;
};

struct ptpMemPoolCreateArg {
    MemoryPoolRef* _poolRefPtr;
    size_t _size;
    MemPoolObjectCtor _construct;
    MemPoolObjectDtor _destruct;
};

static void ptpMemPoolCreateCallback(void* param);

static void ptpMemPoolCreateCallback(void* param)
{
    struct ptpMemPoolCreateArg* arg = (struct ptpMemPoolCreateArg*)param;

    if (arg && arg->_poolRefPtr && (*(arg->_poolRefPtr) == NULL)) {
        *(arg->_poolRefPtr) = ptpMemPoolCreate(arg->_size, arg->_construct, arg->_destruct);
    }
}

void ptpMemPoolCreateOnce(MemoryPoolRef* poolRefPtr, size_t objectSize, MemPoolObjectCtor construct, MemPoolObjectDtor destruct)
{
    // do a quick atomic check whether the memory pool has already been initialized (it must be non zero)
    if (atomic_add_and_fetch_32((int32_t*)poolRefPtr, 0) != 0)
        return;

    // memory pool is zero, execute thread safe callback to allocate it:
    struct ptpMemPoolCreateArg arg = { poolRefPtr, objectSize, construct, destruct };
    ptpThreadSafeCallback(ptpMemPoolCreateCallback, &arg);
}

MemoryPoolRef ptpMemPoolCreate(size_t objectSize, MemPoolObjectCtor construct, MemPoolObjectDtor destruct)
{
    MemoryPoolRef pool = (MemoryPoolRef)malloc(sizeof(struct ptpMemPool_t));

    if (!pool)
        return NULL;

    pool->_objectSize = objectSize;
    pool->_blockSize = PTP_MEMORY_POOL_DEFAULT_BLOCK_SIZE;
    pool->_cs = ptpMutexCreate(NULL);
    pool->_currentMemoryBlob = NULL;
    pool->_currentSlot = NULL;
    pool->_lastSlot = NULL;
    pool->_freeSlots = NULL;
    pool->_allocatedObjects = 0;
    pool->_construct = construct;
    pool->_destruct = destruct;

    return pool;
}

void ptpMemPoolDestroy(MemoryPoolRef mp)
{
    if (!mp)
        return;

    ptpMutexLock(mp->_cs);
    union ptpMemPoolSlot_t* curr = mp->_currentMemoryBlob;

    while (curr != 0) {
        union ptpMemPoolSlot_t* prev = curr->_next;

        // call destructor on element
        if (mp->_destruct)
            mp->_destruct(curr->_element);

        free(curr);
        curr = prev;
    }
    ptpMutexUnlock(mp->_cs);
    ptpMutexDestroy(mp->_cs);
    free(mp);
}

static size_t ptpMemPoolSlotSize(MemoryPoolRef mp)
{
    assert(mp);

    // round it to the multiple of 32 bytes
    size_t typeSize = mp->_objectSize;
    size_t padding = 32;

    typeSize = (typeSize & (padding - 1)) ? ((typeSize & ~(padding - 1)) + padding) : typeSize;
    return typeSize;
}

static void ptpMemPoolAllocateBlock(MemoryPoolRef mp)
{
    assert(mp);

    // Allocate space for the new block and store a pointer to the previous one
    unsigned char* newBlob = (unsigned char*)malloc(mp->_blockSize);

    if (!newBlob)
        return;

    // get access to the memory slot
    union ptpMemPoolSlot_t* slot = (union ptpMemPoolSlot_t*)newBlob;

    // link it with the previous blob
    slot->_next = mp->_currentMemoryBlob;

    // and set the current blob to the new blob
    mp->_currentMemoryBlob = slot;

    // Pad block body to staisfy the alignment requirements for elements
    unsigned char* firstEmptySlot = (unsigned char*)((unsigned char*)newBlob + sizeof(union ptpMemPoolSlot_t*));

    // align it to the nearest address that is a multiple of 32
    unsigned long slotAddr = (unsigned long)firstEmptySlot;
    unsigned long padding = 32;

    if (slotAddr & (padding - 1)) {
        slotAddr = (slotAddr & ~(padding - 1)) + padding;
        firstEmptySlot = (unsigned char*)slotAddr;
    }

    // how many objects can we squeeze into the memory?
    size_t numObjects = (size_t)(newBlob + mp->_blockSize - firstEmptySlot) / ptpMemPoolSlotSize(mp);

    // address of the first valid slot
    mp->_currentSlot = (union ptpMemPoolSlot_t*)(firstEmptySlot);

    // address of the last valid slot
    mp->_lastSlot = (union ptpMemPoolSlot_t*)((char*)mp->_currentSlot + numObjects * ptpMemPoolSlotSize(mp));
}

void* ptpMemPoolAllocate(MemoryPoolRef mp)
{
    assert(mp);
    ptpMutexLock(mp->_cs);

    if (mp->_freeSlots != 0) {
        void* result = mp->_freeSlots;
        mp->_freeSlots = mp->_freeSlots->_next;
        mp->_allocatedObjects++;
        ptpMutexUnlock(mp->_cs);

        // call constructor on object
        if (mp->_construct)
            mp->_construct(result);

        return result;
    } else {
        if (mp->_currentSlot >= mp->_lastSlot)
            ptpMemPoolAllocateBlock(mp);

        // get current item
        void* ret = mp->_currentSlot;

        // size of type
        size_t typeSize = ptpMemPoolSlotSize(mp);

        // initialize allocated memory with zeros
        memset(ret, 0, typeSize);

        // jump to the next free position
        mp->_currentSlot = (union ptpMemPoolSlot_t*)((char*)mp->_currentSlot + typeSize);
        mp->_allocatedObjects++;
        ptpMutexUnlock(mp->_cs);

        // call constructor on object
        if (mp->_construct)
            mp->_construct(ret);

        return ret;
    }
}

void ptpMemPoolDeallocate(MemoryPoolRef mp, void* p)
{
    assert(mp);

    if (!p)
        return;

    // call destructor on object
    if (mp->_destruct)
        mp->_destruct(p);

    ptpMutexLock(mp->_cs);

    // initialize released memory with zeros
    memset(p, 0, ptpMemPoolSlotSize(mp));

    ((union ptpMemPoolSlot_t*)p)->_next = mp->_freeSlots;
    mp->_freeSlots = (union ptpMemPoolSlot_t*)(p);
    mp->_allocatedObjects--;
    ptpMutexUnlock(mp->_cs);
}
