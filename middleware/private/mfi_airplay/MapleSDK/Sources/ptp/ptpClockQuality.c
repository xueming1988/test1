/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpClockQuality.h"
#include "Common/ptpPlatform.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

// ClockQuality default constructor. This has to be explicitly called
// only on a statically allocated ClockQuality object.
void ptpClockQualityCtor(ClockQualityRef quality)
{
    assert(quality);
    quality->_accuracy = 0;
    quality->_class = 0;
    quality->_offsetScaledLogVariance = 0;
}

void ptpClockQualityDtor(ClockQualityRef quality)
{
    if (!quality)
        return;

    // empty
}

void ptpClockQualityFromNetworkOrder(ClockQualityRef quality)
{
    assert(quality);
    quality->_offsetScaledLogVariance = ptpNtohss(quality->_offsetScaledLogVariance);
}

void ptpClockQualityToNetworkOrder(ClockQualityRef quality)
{
    assert(quality);
    quality->_offsetScaledLogVariance = ptpHtonss(quality->_offsetScaledLogVariance);
}

void ptpClockQualityAssign(ClockQualityRef quality, const ClockQualityRef other)
{
    assert(quality);

    if (other) {
        quality->_accuracy = other->_accuracy;
        quality->_class = other->_class;
        quality->_offsetScaledLogVariance = other->_offsetScaledLogVariance;
    }
}

Boolean ptpClockQualityEqualsTo(ClockQualityRef quality, const ClockQualityRef other)
{
    assert(quality && other);
    return (memcmp(quality, other, sizeof(ClockQuality)) == 0);
}

Boolean ptpClockQualityNotEqualTo(ClockQualityRef quality, const ClockQualityRef other)
{
    assert(quality && other);
    return (memcmp(quality, other, sizeof(ClockQuality)) != 0);
}
