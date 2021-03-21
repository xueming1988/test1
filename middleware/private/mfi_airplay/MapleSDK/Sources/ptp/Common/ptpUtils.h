/*
 	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
 */

#ifndef _PTP_UTILS_H_
#define _PTP_UTILS_H_

#include "ptpConstants.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief perform quick sort on given array of 64bit signed integers
 * @param arr pointer to the array for sorting
 * @param start index of first item to sort
 * @param end index of last item to sort
 */
void quickSortInt64(int64_t* arr, int start, int end);

/**
 * @brief perform quick sort on given array of 64bit unsigned integers
 * @param arr pointer to the array for sorting
 * @param start index of first item to sort
 * @param end index of last item to sort
 */
void quickSortUint64(uint64_t* arr, int start, int end);

/**
 * @brief perform quick sort on given array of doubles
 * @param arr pointer to the array for sorting
 * @param start index of first item to sort
 * @param end index of last item to sort
 */
void quickSortDouble(double* arr, int start, int end);

#ifdef __cplusplus
}
#endif

#endif // _PTP_UTILS_H_
