/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpList.h"
#include "DebugServices.h"
#include "ptpMemoryPool.h"

#include <assert.h>
#include <stdlib.h>

// set to 1 to enable list consistency runtime checks
#define LIST_CONSISTENCY_CHECK 0

typedef struct ptpListIterator_t {
    void* _data;
    struct ptpListIterator_t* _prev;
    struct ptpListIterator_t* _next;
} ListIterator;

static void ptpListIteratorCtor(void* ptr);
static void ptpListIteratorDtor(void* ptr);
static void ptpListCtor(void* ptr);
static void ptpListDtor(void* ptr);

// ListIterator object constructor
static void ptpListIteratorCtor(void* ptr)
{
    ListIterator* node = (ListIterator*)ptr;
    assert(node);

    node->_data = NULL;
    node->_prev = NULL;
    node->_next = NULL;
}

// ListIterator object destructor
static void ptpListIteratorDtor(void* ptr)
{
    if (!ptr)
        return;

    // not used at the moment
}

// Memory pool for allocating list iterator nodes
DECLARE_MEMPOOL(ListIterator);
DEFINE_MEMPOOL(ListIterator, ptpListIteratorCtor, ptpListIteratorDtor);

typedef struct ptpList_t {
    int _size;
    ListObjectDestructor _releaseFn;
    struct ptpListIterator_t* _head;
    struct ptpListIterator_t* _tail;
} List;

// List object constructor
static void ptpListCtor(void* ptr)
{
    ListRef list = (ListRef)ptr;
    assert(list);

    list->_size = 0;
    list->_releaseFn = NULL;
    list->_head = NULL;
    list->_tail = NULL;
}

// List object destructor
static void ptpListDtor(void* ptr)
{
    ListRef list = (ListRef)ptr;

    if (!list)
        return;

    ptpListClear(list);
}

// Memory pool for List objects
DECLARE_MEMPOOL(List);
DEFINE_MEMPOOL(List, ptpListCtor, ptpListDtor);

#if LIST_CONSISTENCY_CHECK
typedef Boolean (*NodeTestFunc)(ListIteratorRef node);
static int hasLoopInList(ListIteratorRef node);
static size_t nodeCount(ListIteratorRef node, NodeTestFunc testFunc);
static void ptpListCheckConsistency(ListRef list);
#else // LIST_CONSISTENCY_CHECK
#define ptpListCheckConsistency(list) ((void)(list))
#endif

ListRef ptpListCreate(ListObjectDestructor releaseFn)
{
    ListRef list = ptpMemPoolListAllocate();

    if (!list)
        return NULL;

    list->_releaseFn = releaseFn;
    return list;
}

void ptpListDestroy(ListRef list)
{
    ptpMemPoolListDeallocate(list);
}

void ptpListClear(ListRef list)
{
    ListIteratorRef current;
    check(list);

    // iterate through all subitems
    while (list->_head != NULL) {

        current = list->_head;
        list->_head = current->_next;

        // release inner object
        if (list->_releaseFn) {
            list->_releaseFn(current->_data);
        }

        list->_size--;
        ptpMemPoolListIteratorDeallocate(current);
    }

    list->_head = NULL;
    list->_tail = NULL;
}

ListIteratorRef ptpListFirst(ListRef list)
{
    check(list);
    return list->_head;
}

ListIteratorRef ptpListLast(ListRef list)
{
    check(list);
    return list->_tail;
}

ListIteratorRef ptpListNext(ListIteratorRef current)
{
    return current->_next;
}

// insert object into the list before the given iterator positions
ListIteratorRef ptpListInsert(ListRef list, ListIteratorRef it, void* value)
{
    check(list);

    // if iterator is NULL, we will add the new element to the beginning of list
    if (!it)
        it = list->_head;

    ListIteratorRef node = ptpMemPoolListIteratorAllocate();
    node->_data = value;

    // insert new node before the given iterator
    ListIteratorRef prev = it ? it->_prev : NULL;

    // join new item with next node
    if (it)
        it->_prev = node;

    node->_next = it;

    // join new item with the previous node
    if (prev) {
        prev->_next = node;
        node->_prev = prev;
    }

    // if the position iterator was head element, fix it:
    if (list->_head == it)
        list->_head = node;

    if (!list->_tail)
        list->_tail = node;

    // increase list size
    list->_size++;

    ptpListCheckConsistency(list);

    // return newly added node
    return node;
}

// erase object from the list
ListIteratorRef ptpListErase(ListRef list, ListIteratorRef it)
{
    ListIteratorRef ret = NULL;

    check(list);
    check(it);

    // was our item the head item?
    if (it == list->_head) {

        list->_head = list->_head->_next;

        if (list->_head)
            list->_head->_prev = NULL;

        // the item is now unlinked:
        it->_next = NULL;

        // we will return the new head
        ret = list->_head;
    }

    // was our item the tail item?
    if (it == list->_tail) {

        list->_tail = list->_tail->_prev;

        if (list->_tail)
            list->_tail->_next = NULL;

        // the item is now unlinked:
        it->_prev = NULL;

        // we will return NULL as this was the last item
        ret = NULL;
    }

    if (it->_prev) {
        it->_prev->_next = it->_next;

        if (it->_next)
            it->_next->_prev = it->_prev;

        // we will return next item
        ret = it->_next;
    }

    if (it->_next) {
        it->_next->_prev = it->_prev;

        if (it->_prev)
            it->_prev->_next = it->_next;

        // we will return next item
        ret = it->_next;
    }

    // release inner object
    if (list->_releaseFn) {
        list->_releaseFn(it->_data);
    }

    ptpMemPoolListIteratorDeallocate(it);

    // decrease list size
    list->_size--;

    // it must be always >= 0!
    check(list->_size >= 0);

    ptpListCheckConsistency(list);

    // return next node
    return ret;
}

// get pointer to the object from given iterator
void* ptpListIteratorValue(ListIteratorRef it)
{
    check(it);
    return it->_data;
}

// retrieve first item in the list
void* ptpListFront(ListRef list)
{
    check(list);

    if (list->_head)
        return list->_head->_data;

    return NULL;
}

// retrieve last item in the lsit
void* ptpListBack(ListRef list)
{
    check(list);

    if (list->_tail)
        return list->_tail->_data;

    return NULL;
}

// remove first item from the list
ListIteratorRef ptpListPopFront(ListRef list)
{
    struct ptpListIterator_t* current;
    check(list);

    if (list->_head) {

        current = list->_head;

        // release inner object
        if (list->_releaseFn) {
            list->_releaseFn(current->_data);
        }

        // jump to the next item
        list->_head = list->_head->_next;

        if (list->_head)
            list->_head->_prev = NULL;

        // if the next item is invalid, fix the tail
        if (!list->_head)
            list->_tail = NULL;

        ptpMemPoolListIteratorDeallocate(current);
        list->_size--;

        ptpListCheckConsistency(list);

        return list->_head;
    }

    ptpListCheckConsistency(list);

    return NULL;
}

// push object to the end of the list
ListIteratorRef ptpListPushBack(ListRef list, void* value)
{
    check(list);

    ListIteratorRef node = ptpMemPoolListIteratorAllocate();
    node->_data = value;

    if (list->_tail) {
        list->_tail->_next = node;
        node->_prev = list->_tail;
        list->_tail = node;
    } else {
        list->_head = list->_tail = node;
    }

    // increase list size
    list->_size++;

    ptpListCheckConsistency(list);

    // return newly added node
    return node;
}

unsigned int ptpListSize(ListRef list)
{
    check(list);
    return (unsigned int)(list->_size);
}

#if LIST_CONSISTENCY_CHECK

static int hasLoopInList(ListIteratorRef node)
{
    ListIteratorRef tortoise = node;
    ListIteratorRef hare = node;

    while (1) {

        if (!hare)
            return false;

        hare = hare->_next;

        if (!hare)
            return false;

        hare = hare->_next;
        tortoise = tortoise->_next;

        if (hare == tortoise)
            return true;
    }
}

static size_t nodeCount(ListIteratorRef node, NodeTestFunc testFunc)
{
    size_t numItems = 0;
    while (node) {
        if (testFunc(node))
            numItems++;
        node = node->_next;
    }

    return numItems;
}

// check list in both directions
static void ptpListCheckConsistency(ListRef list)
{
    ListIteratorRef current;

    check(list);
    check(!hasLoopInList(list->_head));

    size_t numNodes = nodeCount(list->_head, NULL);
    check(numNodes == (size_t)list->_size);

    current = list->_tail;
    while (current) {
        current = current->_prev;
        numNodes--;
    }

    check(numNodes == 0);
}
#endif
