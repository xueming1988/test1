/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_REF_COUNTED_OBJECT_H_
#define _PTP_REF_COUNTED_OBJECT_H_

#include "ptpConstants.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Reference counted object
 */

/**
 * @brief Callback to allocate required amount of memory
 */
typedef void* (*ptpRefAllocateMem)(size_t size);

/**
 * @brief Callback to release memory allocated via ptpRefAllocateMem callback
 */
typedef void (*ptpRefDeallocateMem)(void*);

/**
 * @brief Callback to object constructor
 */
typedef void (*ptpRefObjCtor)(void*);

/**
 * @brief Callback to object destructor
 */
typedef void (*ptpRefObjDtor)(void*);

#define DECLARE_REFCOUNTED_OBJECT(NAME)   \
    NAME* ptpRef##NAME##Alloc(void);      \
                                          \
    void ptpRef##NAME##Retain(NAME* obj); \
                                          \
    void ptpRef##NAME##Release(NAME* obj);

#define DEFINE_REFCOUNTED_OBJECT(NAME, ALLOC, FREE, CTOR, DTOR)                                           \
                                                                                                          \
    NAME* ptpRef##NAME##Alloc(void)                                                                       \
    {                                                                                                     \
        NAME* obj = (NAME*)ptpRefAlloc(sizeof(NAME), (ptpRefAllocateMem)ALLOC, (ptpRefDeallocateMem)FREE, \
            (ptpRefObjCtor)CTOR, (ptpRefObjDtor)DTOR);                                                    \
        return obj;                                                                                       \
    }                                                                                                     \
                                                                                                          \
    void ptpRef##NAME##Retain(NAME* obj)                                                                  \
    {                                                                                                     \
        ptpRefRetain((void*)obj);                                                                         \
    }                                                                                                     \
                                                                                                          \
    void ptpRef##NAME##Release(NAME* obj)                                                                 \
    {                                                                                                     \
        ptpRefRelease((void*)obj);                                                                        \
    }

/**
 * @brief Allocate memory of given size and set the reference count to 1
 * @param size size of memory to allocate
 * @param allocate reference to callback which allocates required amount of memory
 * @param free reference to callback which deallocates memory allocated via allocate() param
 * @param ctor object constructor callback
 * @param dtor object destructor callback
 * @return pointer to allocated reference counted memory
 */
void* ptpRefAlloc(size_t size, ptpRefAllocateMem allocate, ptpRefDeallocateMem free, ptpRefObjCtor ctor, ptpRefObjDtor dtor);

/**
 * @brief Retains object (increase retain count by one). Call this function on a pointer allocated via ref_alloc() to increase retain count of this object
 * @param ptr pointer to object previously allocated via ptpRefAlloc()
 */
void ptpRefRetain(void* ptr);

/**
 * @brief Decreases retain count of given object by 1 and eventually free() the allocated memory if the reference counter reaches zero.
 * @param ptr pointer to object previously allocated via ptpRefAlloc()
 * @return True if the object was deallocated, otherwise false
 */
Boolean ptpRefRelease(void* ptr);

/**
 * @brief Get the current reference count for the object
 * @param ptr pointer to object previously allocated via ptpRefAlloc()
 * @return the number of existing references to the object
 */
int ptpRefCount(void* ptr);

#ifdef __cplusplus
}
#endif

#endif // _PTP_REF_COUNTED_OBJECT_H_
