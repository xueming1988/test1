/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_CLOCK_QUALITY_H_
#define _PTP_CLOCK_QUALITY_H_

#include "Common/ptpList.h"
#include "ptpConstants.h"
#include "ptpMacAddress.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef PACKED_STRUCT_BEGIN struct {

    /**
	 * @brief Class of a clock master when it is grandmaster
	 */
    unsigned char _class;

    /**
	 * @brief Expected time accuracy of a clock master
	 */
    unsigned char _accuracy;

    /**
	 * @brief Representation of an estimate of the PTP variance. The
	 * PTP variance characterizes the precision and frequency stability of the clock
	 * master. The PTP variance is the square of PTPDEV
	 */
    int16_t _offsetScaledLogVariance;

} PACKED_STRUCT_END ClockQuality;

typedef ClockQuality* ClockQualityRef;

/**
 * @brief ClockQuality default constructor. This has to be explicitly called only on a statically allocated ClockQuality object.
 * @param quality reference to ClockQuality object
 */
void ptpClockQualityCtor(ClockQualityRef quality);

/**
 * @brief destructor
 * @param quality reference to ClockQuality object
 */
void ptpClockQualityDtor(ClockQualityRef quality);

/**
 * @brief Convert ClockQuality payload from network to host order
 * @param quality reference to ClockQuality object
 */
void ptpClockQualityFromNetworkOrder(ClockQualityRef quality);

/**
 * @brief Convert ClockQuality payload from host to network order
 * @param quality reference to ClockQuality object
 */
void ptpClockQualityToNetworkOrder(ClockQualityRef quality);

/**
 * @brief Copy ClockQuality payload from source to target object
 * @param quality reference to target ClockQuality object
 * @param other reference to source ClockQuality object
 */
void ptpClockQualityAssign(ClockQualityRef quality, const ClockQualityRef other);

/**
 * @brief Compare two ClockQuality objects for equality
 * @param quality reference to ClockQuality object
 * @param other reference to other ClockQuality object used in comparison
 * @return true if both objects are equal
 */
Boolean ptpClockQualityEqualsTo(ClockQualityRef quality, const ClockQualityRef other);

/**
 * @brief Compare two ClockQuality objects for non-equality
 * @param quality reference to ClockQuality object
 * @param other reference to other ClockQuality object used in comparison
 * @return true if both objects are NOT equal
 */
Boolean ptpClockQualityNotEqualTo(ClockQualityRef quality, const ClockQualityRef other);

#ifdef __cplusplus
}
#endif

#endif // _PTP_CLOCK_QUALITY_H_
