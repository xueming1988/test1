/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpClockDataset.h"
#include "ptpLog.h"
#include <assert.h>

void ptpClockDatasetCtor(ClockDatasetRef ds)
{
    assert(ds);

    ds->_priority1 = 0;
    ds->_priority2 = 0;
    ds->_stepsRemoved = 0;

    ptpClockIdentityCtor(&(ds->_identity));
    ptpClockQualityCtor(&(ds->_quality));
    ptpPortIdentityCtor(&(ds->_sender));
    ptpPortIdentityCtor(&(ds->_receiver));
}

void ptpClockDatasetDtor(ClockDatasetRef ds)
{
    if (!ds)
        return;

    ptpClockIdentityDtor(&(ds->_identity));
    ptpClockQualityDtor(&(ds->_quality));
    ptpPortIdentityDtor(&(ds->_sender));
    ptpPortIdentityDtor(&(ds->_receiver));
}

DECLARE_MEMPOOL(ClockDataset)
DEFINE_MEMPOOL(ClockDataset, ptpClockDatasetCtor, ptpClockDatasetDtor)

ClockDatasetRef ptpClockDatasetCreate()
{
    return ptpMemPoolClockDatasetAllocate();
}

ClockDatasetRef ptpClockDatasetCreateCopy(ClockDatasetRef ds)
{
    if (!ds)
        return NULL;

    ClockDatasetRef copy = ptpClockDatasetCreate();

    copy->_priority1 = ds->_priority1;
    copy->_priority2 = ds->_priority2;
    copy->_stepsRemoved = ds->_stepsRemoved;
    ptpClockIdentityAssign(&(copy->_identity), &(ds->_identity));
    ptpClockQualityAssign(&(copy->_quality), &(ds->_quality));
    ptpPortIdentityAssign(&(copy->_sender), &(ds->_sender));
    ptpPortIdentityAssign(&(copy->_receiver), &(ds->_receiver));
    return copy;
}

void ptpClockDatasetDestroy(ClockDatasetRef ds)
{
    if (ds)
        ptpMemPoolClockDatasetDeallocate(ds);
}

Boolean ptpClockDatasetIsBetterThan(ClockDatasetRef dataSetA, ClockDatasetRef dataSetB)
{
#define A_BETTER true
#define B_BETTER false

    assert(dataSetA);

    if (!dataSetB)
        return A_BETTER;

#if (DEBUG)
    PTP_LOG_DEBUG("%s: Clock A -> gmID:%016llx priority1:%3d qualityClass:%3d qualityAccuracy:%3d logVariance:%3d priority2:%3d stepsRemoved:%d",
        __func__,
        ntoh64(*(uint64_t*)(dataSetA->_identity._id)),
        dataSetA->_priority1,
        dataSetA->_quality._class,
        dataSetA->_quality._accuracy,
        dataSetA->_quality._offsetScaledLogVariance,
        dataSetA->_priority2,
        dataSetA->_stepsRemoved);

    PTP_LOG_DEBUG("%s: Clock B -> gmID:%016llx priority1:%3d qualityClass:%3d qualityAccuracy:%3d logVariance:%3d priority2:%3d stepsRemoved:%d",
        __func__,
        ntoh64(*(uint64_t*)(dataSetB->_identity._id)),
        dataSetB->_priority1,
        dataSetB->_quality._class,
        dataSetB->_quality._accuracy,
        dataSetB->_quality._offsetScaledLogVariance,
        dataSetB->_priority2,
        dataSetB->_stepsRemoved);
#endif

    if (dataSetA->_priority1 != dataSetB->_priority1)
        return dataSetA->_priority1 < dataSetB->_priority1 ? A_BETTER : B_BETTER;

    if (dataSetA->_quality._class != dataSetB->_quality._class)
        return dataSetA->_quality._class < dataSetB->_quality._class ? A_BETTER : B_BETTER;

    if (dataSetA->_quality._accuracy != dataSetB->_quality._accuracy)
        return dataSetA->_quality._accuracy < dataSetB->_quality._accuracy ? A_BETTER : B_BETTER;

    if (dataSetA->_quality._offsetScaledLogVariance != dataSetB->_quality._offsetScaledLogVariance)
        return dataSetA->_quality._offsetScaledLogVariance < dataSetB->_quality._offsetScaledLogVariance ? A_BETTER : B_BETTER;

    if (dataSetA->_priority2 != dataSetB->_priority2)
        return dataSetA->_priority2 < dataSetB->_priority2 ? A_BETTER : B_BETTER;

    int cmp = memcmp(dataSetA->_identity._id, dataSetB->_identity._id, sizeof(dataSetA->_identity._id));
    if (cmp == 0) {
        if (dataSetA->_stepsRemoved != dataSetB->_stepsRemoved)
            return dataSetA->_stepsRemoved < dataSetB->_stepsRemoved ? A_BETTER : B_BETTER;

        if (ptpPortIdentityNotEqualTo(&(dataSetA->_sender), &(dataSetB->_sender)))
            return ptpPortIdentityIsSmallerThan(&(dataSetA->_sender), &(dataSetB->_sender)) ? A_BETTER : B_BETTER;
    }

    return cmp < 0 ? A_BETTER : B_BETTER;
}
