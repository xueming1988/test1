/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_CLOCK_DATASET_H_
#define _PTP_CLOCK_DATASET_H_

#include "Common/ptpMemoryPool.h"
#include "ptpClockIdentity.h"
#include "ptpClockQuality.h"
#include "ptpPortIdentity.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t _priority1;
    uint8_t _priority2;
    uint16_t _stepsRemoved;
    ClockIdentity _identity;
    ClockQuality _quality;
    PortIdentity _sender;
    PortIdentity _receiver;
} ClockDataset;

typedef ClockDataset* ClockDatasetRef;

/**
 * @brief allocates memory for a single instance of ClockDataset object
 * @return dynamically allocated ClockDataset object
 */
ClockDatasetRef ptpClockDatasetCreate(void);

/**
 * @brief Create dynamically allocated copy of provided ClockDataset object
 * @param ds reference to ClockDataset object
 * @return dynamically allocated copy of provided ClockDataset object
 */
ClockDatasetRef ptpClockDatasetCreateCopy(ClockDatasetRef ds);

/**
 * @brief calls ptpClockDatasetDtor() and deallocates memory on provided ClockDataset object
 * @param ds reference to the ClockDataset object
 */
void ptpClockDatasetDestroy(ClockDatasetRef ds);

/**
 * @brief ClockDataset constructor. This has to be explicitly called only on every statically or dynamically allocated ClockDataset object.
 * @param ds reference to the ClockDataset object
 */
void ptpClockDatasetCtor(ClockDatasetRef ds);

/**
 * @brief ptpClockDatasetDtor
 * @param ds reference to the ClockDataset object
 */
void ptpClockDatasetDtor(ClockDatasetRef ds);

/**
 * @brief Compare two ClockDataset objects
 * @param dataSetA reference to first ClockDataset object
 * @param dataSetB reference to second ClockDataset object
 * @return true if dataSetA is better than dataSetB
 */
Boolean ptpClockDatasetIsBetterThan(ClockDatasetRef dataSetA, ClockDatasetRef dataSetB);

#ifdef __cplusplus
}
#endif

#endif // _PTP_CLOCK_DATASET_H_
