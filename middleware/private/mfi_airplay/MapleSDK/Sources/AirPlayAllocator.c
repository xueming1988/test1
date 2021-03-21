/*
 Copyright (C) 2018 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
 */

#include "AirPlayAllocator.h"
#include "AirPlayReceiverSessionPriv.h"

/* When TEST_ALLOCATOR_CLIENT is non-zero the allocators
 * will use malloc/free. When combined with malloc_debug
 * or other runtime address sanitizers this is useful for
 * testing the allocators' clients.
 */
#define TEST_ALLOCATOR_CLIENT 0

#if (TEST_ALLOCATOR_CLIENT)

#if 0
#pragma mark Malloc Allocator (Test)
#endif

static char* testAllocatorContext = "test-allocator";

static BlockAllocContext testAllocatorInit(size_t bufferBlobSize, size_t fixedSize)
{
    (void)bufferBlobSize;
    (void)fixedSize;
    return testAllocatorContext;
}

static void testAllocatorFinalize(BlockAllocContext context)
{
    check_panic(context == testAllocatorContext, "");
    return;
}

static void* testAlloc(BlockAllocContext context, size_t blockSize)
{
    check_panic(context == testAllocatorContext, "");
    return malloc(blockSize);
}

static Boolean testFree(BlockAllocContext context, void* block, size_t blockSize)
{
    check_panic(context == testAllocatorContext, "");
    (void)blockSize;
    free(block);

    return true;
}

BlockAllocator ringAllocator = {
    .init = testAllocatorInit,
    .finalize = testAllocatorFinalize,
    .get = testAlloc,
    .free = testFree
};

BlockAllocator fixedAllocator = {
    .init = testAllocatorInit,
    .finalize = testAllocatorFinalize,
    .get = testAlloc,
    .free = testFree
};

#else // TEST_ALLOCATOR_CLIENT

#if 0
#pragma mark Fixed Size Allocator (Realtime)
#endif

/* The fixed size allocator will allocate blocks of a single
 * fixed size. The blocks are tracked in a singly linked
 * list and can be freed in any order.
 */

typedef struct FixedAllocatorNode {
    struct FixedAllocatorNode* next;
} FixedAllocatorNode;

typedef struct {
    void* blob; // Carve this up in to 'fixedSize' blocks
    void* blobEnd; // Pointer just past 'blob'
    size_t fixedSize; // Hand out blocks of this size
    FixedAllocatorNode* freeList; // List of free nodes. Standard head pointer, null tail list.
} FixedAllocator;

static BlockAllocContext fixedAllocatorInit(size_t bufferBlobSize, size_t fixedSize)
{
    FixedAllocator* fixedBuffer = NULL;
    int i;
    FixedAllocatorNode* header;

    if (fixedSize < sizeof(FixedAllocatorNode)) {
        fixedSize = sizeof(FixedAllocatorNode);
    }

    // The blob needs to be able to hold at least one node
    int nodeCount = (int)(bufferBlobSize / fixedSize);
    require(nodeCount > 0, exit);

    fixedBuffer = calloc(1, sizeof(*fixedBuffer));
    require(fixedBuffer, exit);

    fixedBuffer->blob = (uint8_t*)malloc(bufferBlobSize);
    require(fixedBuffer->blob, exit);

    fixedBuffer->blobEnd = fixedBuffer->blob + bufferBlobSize;
    fixedBuffer->fixedSize = fixedSize;

    // Link the nodes together to form the free list
    void* inBuffer = fixedBuffer->blob;
    fixedBuffer->freeList = inBuffer;

    for (i = 0; i < nodeCount - 1; ++i) {
        header = inBuffer;
        inBuffer += fixedSize;
        header->next = inBuffer;
    }

    header = inBuffer;
    header->next = NULL;

exit:

    if (fixedBuffer && !fixedBuffer->blob) {
        free(fixedBuffer);
        fixedBuffer = NULL;
    }

    return fixedBuffer;
}

static void fixedAllocatorFinalize(BlockAllocContext context)
{
    FixedAllocator* fixedBuffer = (FixedAllocator*)context;

    require(fixedBuffer, exit);

    if (fixedBuffer->blob) {
        free(fixedBuffer->blob);
    }
    free(fixedBuffer);

exit:
    return;
}

static void* fixedAlloc(BlockAllocContext context, size_t blockSize)
{
    FixedAllocator* fixedBuffer = (FixedAllocator*)context;
    void* block = NULL;

    require(fixedBuffer, exit);
    require(0 < blockSize && blockSize <= fixedBuffer->fixedSize, exit);

    // Return NULL if we're out of free nodes
    require_quiet(fixedBuffer->freeList, exit);

    block = fixedBuffer->freeList;
    fixedBuffer->freeList = fixedBuffer->freeList->next;

    check_panic(fixedBuffer->blob <= block && block < fixedBuffer->blobEnd, "");

exit:
    return block;
}

static Boolean fixedFree(BlockAllocContext context, void* block, size_t blockSize)
{
    FixedAllocator* fixedBuffer = (FixedAllocator*)context;

    require(fixedBuffer, exit);
    require(fixedBuffer->blob <= block && block < fixedBuffer->blobEnd, exit);
    require(0 < blockSize && blockSize <= fixedBuffer->fixedSize, exit);

    FixedAllocatorNode* node = block;
    node->next = fixedBuffer->freeList;
    fixedBuffer->freeList = node;

exit:
    return true;
}

BlockAllocator fixedAllocator = {
    .init = fixedAllocatorInit,
    .finalize = fixedAllocatorFinalize,
    .get = fixedAlloc,
    .free = fixedFree
};

#if 0
#pragma mark Ring Allocator (Buffered)
#endif

/* The ring allocator can allocate variably sized blocks.
 * The ring allocator requires that blocks passed to
 * ringFree() meet one of two conditions:
 *
 * 1) the block to be freed is the oldest allocated block
 * or
 * 2) the block to be freed is the most recently allocated block.
 *
 * In other words the block to be freed must be at either end
 * of the currently used section of the ring.
 */

/*
 * Ring allocator conditions:
 *  Empty: freePtr == busyPtr
 *  Full:  freePtr == busyPtr - 1
 */
typedef struct {
    void* blob; // Carve this up
    void* blobEnd; // Pointer just past 'blob'
    void* freePtr; // Allocate blocks from here
    void* busyPtr; // Start of the oldest allocated block
} RingAllocator;

static BlockAllocContext ringAllocatorInit(size_t bufferBlobSize, size_t fixedSize)
{
    RingAllocator* varBuffer = NULL;
    (void)fixedSize; // This is not a fixed size allocator

    require(bufferBlobSize > 0, exit);

    varBuffer = calloc(1, sizeof(*varBuffer));
    require(varBuffer, exit);

    varBuffer->blob = malloc(bufferBlobSize);
    require(varBuffer->blob, exit);

    varBuffer->blobEnd = varBuffer->blob + bufferBlobSize;
    varBuffer->freePtr = varBuffer->blob;
    varBuffer->busyPtr = varBuffer->blob;

exit:
    if (varBuffer && !varBuffer->blob) {
        free(varBuffer);
        varBuffer = NULL;
    }
    return varBuffer;
}

static void ringAllocatorFinalize(BlockAllocContext context)
{
    RingAllocator* varBuffer = (RingAllocator*)context;

    require_quiet(varBuffer, exit);
    require(varBuffer->blob, exit);

    free(varBuffer->blob);
    free(varBuffer);

exit:
    return;
}

static void* ringAlloc(BlockAllocContext context, size_t blockSize)
{
    RingAllocator* varBuffer = (RingAllocator*)context;
    void* packet = NULL;

    require(varBuffer, exit);
    require(blockSize > 0, exit);
    check(varBuffer->blob);
    check(varBuffer->blob <= varBuffer->freePtr && varBuffer->freePtr <= varBuffer->blobEnd);
    check(varBuffer->blob <= varBuffer->busyPtr && varBuffer->busyPtr < varBuffer->blobEnd);

    // The free pointer is in front of or equal to the busy pointer.
    if (varBuffer->freePtr >= varBuffer->busyPtr) {

        // Does the block fit before the end of the blob?
        if (varBuffer->freePtr + blockSize <= varBuffer->blobEnd) {
            packet = varBuffer->freePtr;
            varBuffer->freePtr += blockSize;

            // If the block fits in the free space at the head of the blob
            // then wrap the free pointer and try to allocate again.
        } else if (varBuffer->blob + blockSize < varBuffer->busyPtr) {
            varBuffer->freePtr = varBuffer->blob;
            packet = ringAlloc(varBuffer, blockSize);
        }

        // The free pointer is behind the busy pointer so see if we
        // have room up to just short of the busy pointer.
    } else if (varBuffer->freePtr + blockSize < varBuffer->busyPtr) {
        packet = varBuffer->freePtr;
        varBuffer->freePtr += blockSize;
    }

exit:
    check_panic(!packet || (varBuffer->blob <= packet && packet < varBuffer->blobEnd), "");
    check_panic(!varBuffer || (varBuffer->blob <= varBuffer->freePtr && varBuffer->freePtr <= varBuffer->blobEnd), "");
    check_panic(!varBuffer || (varBuffer->blob <= varBuffer->busyPtr && varBuffer->busyPtr < varBuffer->blobEnd), "");

    return packet;
}

static Boolean ringFree(BlockAllocContext context, void* block, size_t blockSize)
{
    RingAllocator* varBuffer = (RingAllocator*)context;
    Boolean success = false;

    require(varBuffer, exit);
    check(varBuffer->blob);
    check(varBuffer->blob <= block && block < varBuffer->blobEnd);
    check(varBuffer->blob <= varBuffer->freePtr && varBuffer->freePtr <= varBuffer->blobEnd);
    check(varBuffer->blob <= varBuffer->busyPtr && varBuffer->busyPtr < varBuffer->blobEnd);

    /* Is this a pushback - freeing the newest block?
     */
    if ((varBuffer->freePtr - blockSize == block) || (varBuffer->freePtr == varBuffer->blob && varBuffer->blobEnd - blockSize == block)) {
        varBuffer->freePtr = block;

        /* Better be freeing the oldest block.
     */
    } else {
        /* If the block doesn't fit at the end then it would have been allocated
         * at the start of the blob.
         */
        void* tmpBusyPtr = varBuffer->busyPtr;
        if (tmpBusyPtr + blockSize > varBuffer->blobEnd) {
            tmpBusyPtr = varBuffer->blob;
        }

        //check( block == tmpBusyPtr );
        require_quiet(block == tmpBusyPtr, exit);

        varBuffer->busyPtr = tmpBusyPtr + blockSize;

        if (varBuffer->busyPtr == varBuffer->blobEnd) {
            varBuffer->busyPtr = varBuffer->blob;
        }
    }

    success = true;

exit:
    check_panic(!varBuffer || (varBuffer->blob <= block && block < varBuffer->blobEnd), "");
    check_panic(!varBuffer || (varBuffer->blob <= varBuffer->freePtr && varBuffer->freePtr <= varBuffer->blobEnd), "");
    check_panic(!varBuffer || (varBuffer->blob <= varBuffer->busyPtr && varBuffer->busyPtr < varBuffer->blobEnd), "");

    return success;
}

BlockAllocator ringAllocator = {
    .init = ringAllocatorInit,
    .finalize = ringAllocatorFinalize,
    .get = ringAlloc,
    .free = ringFree
};

#endif // TEST_ALLOCATOR_CLIENT
