/*
 	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
 */

#include "ptpTimestamp.h"
#include "Common/ptpMemoryPool.h"
#include "Common/ptpPlatform.h"

#include <assert.h>
#include <string.h>

// Maximum value of nanoseconds in one second
#define MAX_NANOSECONDS 1000000000

DECLARE_MEMPOOL(Timestamp)
DEFINE_MEMPOOL(Timestamp, ptpTimestampCtor, ptpTimestampDestroy)

// dynamically allocate Timestamp object
TimestampRef ptpTimestampCreate()
{
    return ptpMemPoolTimestampAllocate();
}

// deallocate dynamically allocated Timestamp object
void ptpTimestampDestroy(TimestampRef timestamp)
{
    ptpMemPoolTimestampDeallocate(timestamp);
}

// default constructor
void ptpTimestampCtor(TimestampRef timestamp)
{
    assert(timestamp);

    timestamp->_nanoseconds = 0;
    timestamp->_secondsLSB = 0;
    timestamp->_secondsMSB = 0;
}

void ptpTimestampDtor(TimestampRef timestamp)
{
    if (!timestamp)
        return;

    // empty
}

void ptpTimestampFromNetworkOrder(TimestampRef timestamp)
{
    assert(timestamp);
    timestamp->_secondsMSB = ptpNtohsu(timestamp->_secondsMSB);
    timestamp->_secondsLSB = ptpNtohlu(timestamp->_secondsLSB);
    timestamp->_nanoseconds = ptpNtohlu(timestamp->_nanoseconds);
}

void ptpTimestampToNetworkOrder(TimestampRef timestamp)
{
    assert(timestamp);
    timestamp->_secondsMSB = ptpHtonsu(timestamp->_secondsMSB);
    timestamp->_secondsLSB = ptpHtonlu(timestamp->_secondsLSB);
    timestamp->_nanoseconds = ptpHtonlu(timestamp->_nanoseconds);
}

// explicit data constructor
void ptpTimestampInitCtor(TimestampRef timestamp, uint32_t ns, uint32_t secondsLow, uint16_t secondsHigh)
{
    assert(timestamp);

    timestamp->_nanoseconds = ns;
    timestamp->_secondsLSB = secondsLow;
    timestamp->_secondsMSB = secondsHigh;
}

// timespec constructor
void ptpTimestampInitFromTsCtor(TimestampRef timestamp, const struct timespec* ts)
{
    assert(timestamp && ts);

    int seclen = sizeof(ts->tv_sec) - sizeof(timestamp->_secondsLSB);
    if (seclen > 0) {
        timestamp->_secondsMSB = (uint16_t)(ts->tv_sec >> (sizeof(ts->tv_sec) - (uint32_t)seclen) * 8);
        timestamp->_secondsLSB = ts->tv_sec & 0xFFFFFFFF;
    } else {
        timestamp->_secondsMSB = 0;
        timestamp->_secondsLSB = (uint32_t)(ts->tv_sec);
    }
    timestamp->_nanoseconds = (uint32_t)(ts->tv_nsec);
}

// assign content of 'other' timestamp into 'timestamp'
void ptpTimestampAssign(TimestampRef timestamp, const TimestampRef other)
{
    assert(timestamp && other);

    if (timestamp != other) {
        timestamp->_nanoseconds = other->_nanoseconds;
        timestamp->_secondsLSB = other->_secondsLSB;
        timestamp->_secondsMSB = other->_secondsMSB;
    }
}

// get text representation of timestamp
void ptpTimestampToString(TimestampRef timestamp, char* str, uint32_t strLen)
{
    assert(timestamp && str);
    ptpSnprintf((char*)str, strLen, "%hu %u.%09u", timestamp->_secondsMSB, timestamp->_secondsLSB, timestamp->_nanoseconds);
}

// get timestamp value as timespec
struct timespec ptpTimestampToTimespec(TimestampRef timestamp)
{
    struct timespec ts;
    assert(timestamp);

    ts.tv_sec = (long)(((uint64_t)timestamp->_secondsMSB << 32) | timestamp->_secondsLSB);
    ts.tv_nsec = timestamp->_nanoseconds;
    return ts;
}

// add timestamp 'second' to 'first' and store the result into 'out'
// out = first + second
void ptpTimestampAdd(TimestampRef out, TimestampRef timestamp, const TimestampRef other)
{
    uint32_t nanoseconds;
    uint32_t seconds_ls;
    uint16_t seconds_ms;
    Boolean carry;

    assert(out && timestamp && other);

    nanoseconds = timestamp->_nanoseconds;
    nanoseconds += other->_nanoseconds;
    carry = nanoseconds < timestamp->_nanoseconds || nanoseconds >= MAX_NANOSECONDS;
    nanoseconds -= carry ? MAX_NANOSECONDS : 0;

    seconds_ls = timestamp->_secondsLSB;
    seconds_ls += other->_secondsLSB;
    seconds_ls += carry ? 1 : 0;
    carry = seconds_ls < timestamp->_secondsLSB;

    seconds_ms = timestamp->_secondsMSB;
    seconds_ms += other->_secondsMSB;
    seconds_ms += carry ? 1 : 0;
    carry = seconds_ms < timestamp->_secondsMSB;

    ptpTimestampInitCtor(out, nanoseconds, seconds_ls, seconds_ms);
}

// subtract timestamp 'second' from 'first' and store the result into 'out'
// out = first - second
void ptpTimestampSub(TimestampRef out, TimestampRef timestamp, const TimestampRef other)
{
    uint32_t nanoseconds;
    uint32_t seconds_ls;
    uint16_t seconds_ms;

    Boolean carry, borrow_this;
    unsigned borrow_total = 0;

    assert(out && timestamp && other);

    borrow_this = timestamp->_nanoseconds < other->_nanoseconds;
    nanoseconds = ((borrow_this ? MAX_NANOSECONDS : 0) + timestamp->_nanoseconds) - other->_nanoseconds;
    carry = nanoseconds > MAX_NANOSECONDS;
    nanoseconds -= carry ? MAX_NANOSECONDS : 0;
    borrow_total += borrow_this ? 1 : 0;

    seconds_ls = carry ? 1 : 0;
    seconds_ls += timestamp->_secondsLSB;
    borrow_this = borrow_total > seconds_ls || seconds_ls - borrow_total < other->_secondsLSB;
    seconds_ls = borrow_this ? seconds_ls - other->_secondsLSB + (uint32_t)-1 : (seconds_ls - borrow_total) - other->_secondsLSB;
    borrow_total = borrow_this ? borrow_total + 1 : 0;

    seconds_ms = carry ? 1 : 0;
    seconds_ms += timestamp->_secondsMSB;
    borrow_this = borrow_total > seconds_ms || seconds_ms - borrow_total < other->_secondsMSB;
    seconds_ms = (uint16_t)(borrow_this ? seconds_ms - other->_secondsMSB + (uint32_t)-1 : (seconds_ms - borrow_total) - other->_secondsMSB);
    borrow_total = borrow_this ? borrow_total + 1 : 0;

    ptpTimestampInitCtor(out, nanoseconds, seconds_ls, seconds_ms);
}

// returns whole part of timestamp in seconds
uint32_t ptpTimestampGetSeconds(TimestampRef timestamp)
{
    assert(timestamp);
    return timestamp->_secondsLSB;
}

// returns fractional part of timestamp in microseconds
uint32_t ptpTimestampGetMicroSeconds(TimestampRef timestamp)
{
    assert(timestamp);
    return (uint32_t)(timestamp->_nanoseconds / 1000);
}

// get Timestamp in seconds
double TIMESTAMP_TO_NANOSECONDS(const TimestampRef timestamp)
{
    if (!timestamp)
        return 0;

    double seconds = (((int64_t)(timestamp->_secondsMSB)) << 32) + timestamp->_secondsLSB;
    return SECONDS_TO_NANOSECONDS(seconds) + (double)timestamp->_nanoseconds;
}

double TIMESTAMP_TO_SECONDS(const TimestampRef timestamp)
{
    if (!timestamp)
        return 0;

    double seconds = (((int64_t)(timestamp->_secondsMSB)) << 32) + timestamp->_secondsLSB;
    return seconds + NANOSECONDS_TO_SECONDS((double)timestamp->_nanoseconds);
}
