/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_LIST_H_
#define _PTP_LIST_H_

#include "ptpConstants.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ListObjectDestructor)(void*);

typedef struct ptpList_t* ListRef;
typedef struct ptpListIterator_t* ListIteratorRef;

/**
 * @brief Helper macro to declare type safe linked list API
 */

#define DECLARE_LIST(TYPE)                                                                                  \
    typedef void (*ListObject##TYPE##Destructor)(TYPE * obj);                                               \
    typedef ListRef List##TYPE##Ref;                                                                        \
    typedef ListIteratorRef List##TYPE##Iterator;                                                           \
                                                                                                            \
    List##TYPE##Ref ptpList##TYPE##Create(void);                                                            \
                                                                                                            \
    void ptpList##TYPE##Destroy(List##TYPE##Ref list);                                                      \
                                                                                                            \
    void ptpList##TYPE##Clear(List##TYPE##Ref list);                                                        \
                                                                                                            \
    List##TYPE##Iterator ptpList##TYPE##First(List##TYPE##Ref list);                                        \
                                                                                                            \
    List##TYPE##Iterator ptpList##TYPE##Last(List##TYPE##Ref list);                                         \
                                                                                                            \
    List##TYPE##Iterator ptpList##TYPE##Next(List##TYPE##Iterator current);                                 \
                                                                                                            \
    List##TYPE##Iterator ptpList##TYPE##Insert(List##TYPE##Ref list, List##TYPE##Iterator it, TYPE* value); \
                                                                                                            \
    List##TYPE##Iterator ptpList##TYPE##Erase(List##TYPE##Ref list, List##TYPE##Iterator it);               \
                                                                                                            \
    TYPE* ptpList##TYPE##IteratorValue(List##TYPE##Iterator it);                                            \
                                                                                                            \
    TYPE* ptpList##TYPE##Front(List##TYPE##Ref list);                                                       \
                                                                                                            \
    TYPE* ptpList##TYPE##Back(List##TYPE##Ref list);                                                        \
                                                                                                            \
    List##TYPE##Iterator ptpList##TYPE##PopFront(List##TYPE##Ref list);                                     \
                                                                                                            \
    List##TYPE##Iterator ptpList##TYPE##PushBack(List##TYPE##Ref list, TYPE* value);                        \
                                                                                                            \
    unsigned int ptpList##TYPE##Size(List##TYPE##Ref list);

#define DECLARE_LIST_REF(TYPE, TYPEREF)                                                                     \
    typedef void (*ListObject##TYPE##Destructor)(TYPEREF obj);                                              \
    typedef ListRef List##TYPEREF;                                                                          \
    typedef ListIteratorRef List##TYPE##Iterator;                                                           \
                                                                                                            \
    List##TYPEREF ptpList##TYPE##Create(void);                                                              \
                                                                                                            \
    void ptpList##TYPE##Destroy(List##TYPEREF list);                                                        \
                                                                                                            \
    void ptpList##TYPE##Clear(List##TYPEREF list);                                                          \
                                                                                                            \
    List##TYPE##Iterator ptpList##TYPE##First(List##TYPEREF list);                                          \
                                                                                                            \
    List##TYPE##Iterator ptpList##TYPE##Last(List##TYPEREF list);                                           \
                                                                                                            \
    List##TYPE##Iterator ptpList##TYPE##Next(List##TYPE##Iterator current);                                 \
                                                                                                            \
    List##TYPE##Iterator ptpList##TYPE##Insert(List##TYPEREF list, List##TYPE##Iterator it, TYPEREF value); \
                                                                                                            \
    List##TYPE##Iterator ptpList##TYPE##Erase(List##TYPEREF list, List##TYPE##Iterator it);                 \
                                                                                                            \
    TYPEREF ptpList##TYPE##IteratorValue(List##TYPE##Iterator it);                                          \
                                                                                                            \
    TYPEREF ptpList##TYPE##Front(List##TYPEREF list);                                                       \
                                                                                                            \
    TYPEREF ptpList##TYPE##Back(List##TYPEREF list);                                                        \
                                                                                                            \
    List##TYPE##Iterator ptpList##TYPE##PopFront(List##TYPEREF list);                                       \
                                                                                                            \
    List##TYPE##Iterator ptpList##TYPE##PushBack(List##TYPEREF list, TYPEREF value);                        \
                                                                                                            \
    unsigned int ptpList##TYPE##Size(List##TYPEREF list);

/**
 * @brief Helper macro to define type safe linked list API
 */

#define DEFINE_LIST(TYPE, DTOR)                                                                             \
    List##TYPE##Ref ptpList##TYPE##Create() { return ptpListCreate((ListObjectDestructor)DTOR); }           \
                                                                                                            \
    void ptpList##TYPE##Destroy(List##TYPE##Ref list) { ptpListDestroy(list); }                             \
                                                                                                            \
    void ptpList##TYPE##Clear(List##TYPE##Ref list) { ptpListClear(list); }                                 \
                                                                                                            \
    List##TYPE##Iterator ptpList##TYPE##First(List##TYPE##Ref list) { return ptpListFirst(list); }          \
                                                                                                            \
    List##TYPE##Iterator ptpList##TYPE##Last(List##TYPE##Ref list) { return ptpListLast(list); }            \
                                                                                                            \
    List##TYPE##Iterator ptpList##TYPE##Next(List##TYPE##Iterator current) { return ptpListNext(current); } \
                                                                                                            \
    List##TYPE##Iterator ptpList##TYPE##Insert(List##TYPE##Ref list, List##TYPE##Iterator it, TYPE* value)  \
    {                                                                                                       \
        return ptpListInsert(list, it, (void*)value);                                                       \
    }                                                                                                       \
                                                                                                            \
    List##TYPE##Iterator ptpList##TYPE##Erase(List##TYPE##Ref list, List##TYPE##Iterator it)                \
    {                                                                                                       \
        return ptpListErase(list, it);                                                                      \
    }                                                                                                       \
                                                                                                            \
    TYPE* ptpList##TYPE##IteratorValue(List##TYPE##Iterator it) { return (TYPE*)ptpListIteratorValue(it); } \
                                                                                                            \
    TYPE* ptpList##TYPE##Front(List##TYPE##Ref list) { return (TYPE*)ptpListFront(list); }                  \
                                                                                                            \
    TYPE* ptpList##TYPE##Back(List##TYPE##Ref list) { return (TYPE*)ptpListBack(list); }                    \
                                                                                                            \
    List##TYPE##Iterator ptpList##TYPE##PopFront(List##TYPE##Ref list) { return ptpListPopFront(list); }    \
                                                                                                            \
    List##TYPE##Iterator ptpList##TYPE##PushBack(List##TYPE##Ref list, TYPE* value)                         \
    {                                                                                                       \
        return ptpListPushBack(list, (void*)value);                                                         \
    }                                                                                                       \
                                                                                                            \
    unsigned int ptpList##TYPE##Size(List##TYPE##Ref list) { return ptpListSize(list); }

/**
 * @brief allocates memory for a single instance of List object
 * @return dynamically allocated List object
 */
ListRef ptpListCreate(ListObjectDestructor releaseFn);

/**
 * @brief Destroy List object and remove all stored items
 * @param list reference to List object
 */
void ptpListDestroy(ListRef list);

/**
 * @brief Remove all items from the list
 * @param list reference to List object
 */
void ptpListClear(ListRef list);

/**
 * @brief Get first iterator
 * @param list reference to List object
 * @return reference to iterator that points to the first object in the list
 */
ListIteratorRef ptpListFirst(ListRef list);

/**
 * @brief Get last iterator
 * @param list reference to List object
 * @return reference to iterator that points to the last object in the list
 */
ListIteratorRef ptpListLast(ListRef list);

/**
 * @brief Get next iterator
 * @param current reference to the current iterator
 * @return reference to the next iterator in the list (or NULL)
 */
ListIteratorRef ptpListNext(ListIteratorRef current);

/**
 * @brief Insert object into the list before the given iterator positions
 * @param list reference to List object
 * @param it current iterator
 * @param value object value
 * @return reference to newly added iterator
 */
ListIteratorRef ptpListInsert(ListRef list, ListIteratorRef it, void* value);

/**
 * @brief Erase object from the list at given iterator position
 * @param list reference to List object
 * @param it current iterator
 * @return reference to the next valid iterator
 */
ListIteratorRef ptpListErase(ListRef list, ListIteratorRef it);

/**
 * @brief Get pointer to the object from given iterator
 * @param it current iterator
 * @return pointer to the stored object
 */
void* ptpListIteratorValue(ListIteratorRef it);

/**
 * @brief Retrieve first item in the list
 * @param list reference to List object
 * @return pointer to first stored object
 */
void* ptpListFront(ListRef list);

/**
 * @brief Retrieve last item in the list
 * @param list reference to List object
 * @return pointer to last stored object
 */
void* ptpListBack(ListRef list);

/**
 * @brief Remove first item from the list
 * @param list reference to List object
 * @return reference to the next valid iterator
 */
ListIteratorRef ptpListPopFront(ListRef list);

/**
 * @brief Push object to the end of the list
 * @param list reference to List object
 * @param value pointer to the object that has to be pushed to the list
 * @return reference to the next valid iterator
 */
ListIteratorRef ptpListPushBack(ListRef list, void* value);

/**
 * @brief Get the size of list
 * @param list reference to List object
 * @return size of list (the number of stored objects)
 */
unsigned int ptpListSize(ListRef list);

#ifdef __cplusplus
}
#endif

#endif // _PTP_LIST_H_
