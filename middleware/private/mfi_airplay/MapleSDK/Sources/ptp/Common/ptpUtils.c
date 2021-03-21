/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpUtils.h"
#include <inttypes.h>
#include <stdlib.h>

#define compare(x, y) ((x) > (y) ? 1 : (x) < (y) ? -1 : 0)

static int compareInt64(const void* a, const void* b)
{
    return compare((*(int64_t*)a), (*(int64_t*)b));
}

static int compareUInt64(const void* a, const void* b)
{
    return compare((*(uint64_t*)a), (*(uint64_t*)b));
}

static int compareDouble(const void* a, const void* b)
{
    return compare((*(double*)a), (*(double*)b));
}

void quickSortInt64(int64_t* arr, int start, int end)
{
    qsort(arr, end - start, sizeof(int64_t), compareInt64);
}

void quickSortUint64(uint64_t* arr, int start, int end)
{
    qsort(arr, end - start, sizeof(uint64_t), compareUInt64);
}

void quickSortDouble(double* arr, int start, int end)
{
    qsort(arr, end - start, sizeof(double), compareDouble);
}
