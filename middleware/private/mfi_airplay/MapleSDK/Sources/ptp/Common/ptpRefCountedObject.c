/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpRefCountedObject.h"
#include "AtomicUtils.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// magic marker
#define MAGIC 0xFDBACBBA

typedef struct {
    void* _data;
    ptpRefDeallocateMem _free;
    ptpRefObjDtor _dtor;
    int _refCount;
    uint32_t _marker;
} ptpRefCountedObject_t;

void* ptpRefAlloc(size_t size, ptpRefAllocateMem alloc, ptpRefDeallocateMem free, ptpRefObjCtor ctor, ptpRefObjDtor dtor)
{
    ptpRefCountedObject_t* refCountedObj;
    char* ptr;

    // allocate memory either using supplied allocator or via calloc()
    if (alloc)
        refCountedObj = (ptpRefCountedObject_t*)alloc(sizeof(ptpRefCountedObject_t) + size);
    else
        refCountedObj = (ptpRefCountedObject_t*)calloc(sizeof(ptpRefCountedObject_t) + size, 1);

    if (refCountedObj == NULL)
        return NULL;

    // set initial ref count
    refCountedObj->_refCount = 1;

    // keep free/release callbacks
    refCountedObj->_free = free;
    refCountedObj->_dtor = dtor;

    // the actual user visible object is located after the ptpRefCountedObject_t structure
    ptr = (char*)refCountedObj;
    ptr += sizeof(ptpRefCountedObject_t);
    refCountedObj->_data = ptr;
    refCountedObj->_marker = 0xFDBACBBA;

    // call constructor
    if (ctor)
        ctor(ptr);

    return (void*)ptr;
}

void ptpRefRetain(void* ptr)
{
    ptpRefCountedObject_t* refCountedObj;
    char* cptr;

    if (!ptr)
        return;

    // get access to underlying structure
    cptr = (char*)ptr;
    cptr -= sizeof(ptpRefCountedObject_t);
    refCountedObj = (ptpRefCountedObject_t*)cptr;
    assert(refCountedObj->_marker == 0xFDBACBBA);

    // atomically increase refcount
    atomic_add_and_fetch_32(&(refCountedObj->_refCount), 1);
}

Boolean ptpRefRelease(void* ptr)
{
    ptpRefCountedObject_t* refCountedObj;
    char* cptr;

    if (!ptr)
        return false;

    // get access to underlying object
    cptr = (char*)ptr;
    cptr -= sizeof(ptpRefCountedObject_t);
    refCountedObj = (ptpRefCountedObject_t*)cptr;
    assert(refCountedObj->_marker == 0xFDBACBBA);

    // atomically decrease refcount
    if (atomic_add_and_fetch_32(&(refCountedObj->_refCount), -1) <= 0) {

        // if we got zero, we must release associated object
        // call destructor if available
        if (refCountedObj->_dtor)
            refCountedObj->_dtor(ptr);

        if (refCountedObj->_free)
            refCountedObj->_free(refCountedObj);
        else
            free(refCountedObj);

        return true;
    } else {
        return false;
    }
}

int ptpRefCount(void* ptr)
{
    ptpRefCountedObject_t* refCountedObj;
    char* cptr;

    // get access to underlying structure
    cptr = (char*)ptr;
    cptr -= sizeof(ptpRefCountedObject_t);
    refCountedObj = (ptpRefCountedObject_t*)cptr;
    assert(refCountedObj->_marker == 0xFDBACBBA);

    // atomically copy refcount
    return atomic_add_and_fetch_32(&(refCountedObj->_refCount), 0);
}
