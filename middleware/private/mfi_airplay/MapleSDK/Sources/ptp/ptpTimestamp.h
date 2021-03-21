/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_TIMESTAMP_H_
#define _PTP_TIMESTAMP_H_

#include "ptpConstants.h"
#include <stdint.h>

#if !TARGET_OS_BRIDGECO
#include <net/if.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TSTAMP_STRLEN 25 // Maximum size of timestamp string

typedef PACKED_STRUCT_BEGIN struct {

    /**
	 * @brief 48bit seconds value
	 */
    uint16_t _secondsMSB;
    uint32_t _secondsLSB;

    /**
	 * @brief 32bit nanoseconds value
	 */
    uint32_t _nanoseconds;

} PACKED_STRUCT_END Timestamp;

typedef Timestamp* TimestampRef;

/**
 * @brief Allocate memory for a single instance of Timestamp object
 * @return dynamically allocated Timestamp object
 */
TimestampRef ptpTimestampCreate(void);

/**
 * @brief Destroy Timestamp object and deallocate memory
 * @param timestamp reference to Timestamp object
 */
void ptpTimestampDestroy(TimestampRef timestamp);

/**
 * @brief Timestamp default constructor. This has to be explicitly called only on a statically allocated Timestamp object.
 * @param timestamp reference to Timestamp object
 */
void ptpTimestampCtor(TimestampRef timestamp);

/**
 * @brief Timestamp constructor
 * @param timestamp reference to Timestamp object
 * @param nanoseconds nanosecond part
 * @param secondsLow lsb seconds part
 * @param secondsHigh msb seconds part
 */
void ptpTimestampInitCtor(TimestampRef timestamp, uint32_t nanoseconds, uint32_t secondsLow, uint16_t secondsHigh);

/**
 * @brief Timestamp constructor, initializes Timestamp object from given timespec structure
 * @param timestamp reference to Timestamp object
 * @param ts pointer to timespec structure
 */
void ptpTimestampInitFromTsCtor(TimestampRef timestamp, const struct timespec* ts);

/**
 * @brief Timestamp destructor
 * @param timestamp reference to Timestamp object
 */
void ptpTimestampDtor(TimestampRef timestamp);

/**
 * @brief Convert Timestamp object from network order to host order
 * @param timestamp reference to Timestamp object
 */
void ptpTimestampFromNetworkOrder(TimestampRef timestamp);

/**
 * @brief Convert Timestamp object from network order to host order
 * @param timestamp reference to Timestamp object
 */
void ptpTimestampToNetworkOrder(TimestampRef timestamp);

/**
 * @brief Assign content of 'other' timestamp into 'timestamp'
 * @param timestamp reference to Timestamp object
 * @param other reference to other Timestamp object
 */
void ptpTimestampAssign(TimestampRef timestamp, const TimestampRef other);

/**
 * @brief Get text representation of timestamp
 * @param timestamp reference to Timestamp object
 * @param str [out] output string array
 * @param strLen length of output string array
 */
void ptpTimestampToString(TimestampRef timestamp, char* str, uint32_t strLen);

/**
 * @brief Get timestamp value as timespec
 * @param timestamp reference to Timestamp object
 * @return timestamp as timespec value
 */
struct timespec ptpTimestampToTimespec(TimestampRef timestamp);

/**
 * @brief Add timestamp 'second' to 'first' and store the result into 'out'
 * @param out [out] reference to output Timestamp object
 * @param first reference to first Timestamp object
 * @param second reference to second Timestamp object
 */
void ptpTimestampAdd(TimestampRef out, TimestampRef first, const TimestampRef second);

/**
 * @brief Subtract timestamp 'second' from 'first' and store the result into 'out'
 * @param out [out] reference to output Timestamp object
 * @param first reference to first Timestamp object
 * @param second reference to second Timestamp object
 */
void ptpTimestampSub(TimestampRef out, TimestampRef first, const TimestampRef second);

/**
 * @brief Return whole part of timestamp in seconds
 * @param timestamp reference to Timestamp object
 * @return the number of timestamp seconds
 */
uint32_t ptpTimestampGetSeconds(TimestampRef timestamp);

/**
 * @brief Return fractional part of timestamp in microseconds
 * @param timestamp reference to Timestamp object
 * @return fractional part of timestamp in microseconds
 */
uint32_t ptpTimestampGetMicroSeconds(TimestampRef timestamp);

/**
 * @brief Get timestamp value in nanoseconds (we can't use 64bit integer as the value in nanoseconds can use up to 80bits)
 * @param ts reference to Timestamp object
 * @return nanosecond representation of timestamp
 */
double TIMESTAMP_TO_NANOSECONDS(const TimestampRef ts);

/**
 * @brief Get timestamp value in seconds
 * @param ts reference to Timestamp object
 * @return timestamp in seconds
 */
double TIMESTAMP_TO_SECONDS(const TimestampRef ts);

#ifdef __cplusplus
}
#endif

#endif // _PTP_TIMESTAMP_H_
