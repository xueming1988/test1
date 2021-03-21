/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _MEMORY_POOL_H_
#define _MEMORY_POOL_H_

#include "ptpMutex.h"
#include "ptpThreadSafeCallback.h"
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * object constructor/destructor callback prototypes
 */
typedef void (*MemPoolObjectCtor)(void*);
typedef void (*MemPoolObjectDtor)(void*);

typedef struct ptpMemPool_t* MemoryPoolRef;

/**
 * @brief Helper macro to declare type safe memory pool API
 */

#define DECLARE_MEMPOOL(TYPE)                   \
    typedef MemoryPoolRef MemPool##TYPE##Ref;   \
    typedef void (*MemPool##TYPE##Ctor)(TYPE*); \
    typedef void (*MemPool##TYPE##Dtor)(TYPE*); \
                                                \
    void ptpMemPool##TYPE##Destroy(void);       \
                                                \
    TYPE* ptpMemPool##TYPE##Allocate(void);     \
                                                \
    void ptpMemPool##TYPE##Deallocate(TYPE* p);

/**
 * @brief Helper macro to define type safe memory pool API and object
 */

#define DEFINE_MEMPOOL(TYPE, CTOR, DTOR)                                     \
    static MemPool##TYPE##Ref TYPE##Pool = NULL;                             \
    void ptpMemPool##TYPE##Destroy(void)                                     \
    {                                                                        \
        if (TYPE##Pool)                                                      \
            ptpMemPoolDestroy(TYPE##Pool);                                   \
    }                                                                        \
                                                                             \
    TYPE* ptpMemPool##TYPE##Allocate(void)                                   \
    {                                                                        \
        ptpMemPoolCreateOnce((MemoryPoolRef*)&(TYPE##Pool),                  \
            sizeof(TYPE), (MemPoolObjectCtor)CTOR, (MemPoolObjectDtor)DTOR); \
        return (TYPE*)ptpMemPoolAllocate(TYPE##Pool);                        \
    }                                                                        \
                                                                             \
    void ptpMemPool##TYPE##Deallocate(TYPE* p)                               \
    {                                                                        \
        if (TYPE##Pool)                                                      \
            ptpMemPoolDeallocate(TYPE##Pool, (void*)p);                      \
    }

/**
 * @brief Defines memory pool for objects where the amount of allocated memory
 * doesn't equal to sizeof(TYPE)
 */

#define DEFINE_MEMPOOL_DYNAMIC(TYPE, ALLOC_SIZE, CTOR, DTOR)               \
    MemPool##TYPE##Ref TYPE##Pool = NULL;                                  \
    void ptpMemPool##TYPE##Destroy()                                       \
    {                                                                      \
        if (TYPE##Pool)                                                    \
            ptpMemPoolDestory(TYPE##Pool);                                 \
    }                                                                      \
                                                                           \
    TYPE* ptpMemPool##TYPE##Allocate()                                     \
    {                                                                      \
        ptpMemPoolCreateOnce((MemoryPoolRef*)&(TYPE##Pool),                \
            ALLOC_SIZE, (MemPoolObjectCtor)CTOR, (MemPoolObjectDtor)DTOR); \
        return (TYPE*)ptpMemPoolAllocate(TYPE##Pool);                      \
    }                                                                      \
                                                                           \
    void ptpMemPool##TYPE##Deallocate(TYPE* p)                             \
    {                                                                      \
        if (TYPE##Pool)                                                    \
            ptpMemPoolDeallocate(TYPE##Pool, (void*)p);                    \
    }

/**
 * @brief Create memory pool object
 * @param typeSize the size of one allocated object
 * @param construct reference to object constructor (or NULL)
 * @param destruct reference to object destructor (or NULL)
 * @return reference to MemoryPool object
 */
MemoryPoolRef ptpMemPoolCreate(size_t objectSize, MemPoolObjectCtor construct, MemPoolObjectDtor destruct);

/**
 * @brief Atomically create memory pool object if the passed pool argument points to NULL
 * @param pool reference to MemoryPool object
 * @param typeSize the size of one allocated object
 * @param construct reference to object constructor (or NULL)
 * @param destruct reference to object destructor (or NULL)
 * @return reference to MemoryPool object
 */
void ptpMemPoolCreateOnce(MemoryPoolRef* poolRefPtr, size_t objectSize, MemPoolObjectCtor construct, MemPoolObjectDtor destruct);

/**
 * @brief Destroy memory pool object
 * @param pool reference to MemoryPool object
 */
void ptpMemPoolDestroy(MemoryPoolRef pool);

/**
 * @brief Allocate one element from the memory pool
 * @param pool reference to MemoryPool object
 * @return pointer to allocated memory
 */
void* ptpMemPoolAllocate(MemoryPoolRef pool);

/**
 * @brief Release one element to the memory pool
 * @param pool reference to MemoryPool object
 * @param ptr pointer to memory previously allocated via ptpMemPoolAllocate()
 */
void ptpMemPoolDeallocate(MemoryPoolRef pool, void* ptr);

#ifdef __cplusplus
}
#endif

#endif // _MEMORY_POOL_H_
