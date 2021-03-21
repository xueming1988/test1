/*
 Copyright (C) 2018 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
 */

#ifndef AirPlayAllocator_h
#define AirPlayAllocator_h

#include "AirPlayReceiverSession.h"

typedef void* BlockAllocContext;

typedef BlockAllocContext (*BlockAllocInit_f)(size_t bufferPoolSize, size_t fixedSize);
typedef void (*BlockAllocFinalize_f)(BlockAllocContext context);
typedef void* (*BlockAllocGet_f)(BlockAllocContext context, size_t blockSize);
typedef Boolean (*BlockAllocFree_f)(BlockAllocContext context, void* ptr, size_t blockSize);

typedef struct {
    BlockAllocInit_f init;
    BlockAllocFinalize_f finalize;
    BlockAllocGet_f get;
    BlockAllocFree_f free;
} BlockAllocator;

extern BlockAllocator ringAllocator;
extern BlockAllocator fixedAllocator;

#endif /* AirPlayAllocator_h */
