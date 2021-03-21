/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_CONSTANTS_H_
#define _PTP_CONSTANTS_H_

#ifndef TARGET_OS_BRIDGECO
#define TARGET_OS_BRIDGECO 0
#endif

#if TARGET_OS_BRIDGECO
#include "Platform/BridgeCo/Support/AirPlayPlatformIncludes.h"
#endif

#include "CommonServices.h"
#include <inttypes.h>
#include <string.h>

#if !TARGET_OS_BRIDGECO
#include <sys/time.h>
#include <time.h>
#endif

#include "AirPlayCommon.h"

//
// Macros and basic type definitions
//

typedef double NanosecondTs;

#if !TARGET_OS_BRIDGECO
#define PACKED_STRUCT_BEGIN
#define PACKED_STRUCT_END __attribute__((packed))
#endif

#define SECONDS_TO_MILLISECONDS(val) ((double)(val)*1000.0)
#define SECONDS_TO_NANOSECONDS(val) ((double)(val)*1000000000.0)
#define MILLISECONDS_TO_NANOSECONDS(val) ((double)(val)*1000000.0)
#define MILLISECONDS_TO_SECONDS(val) ((double)(val) * (1.0 / 1000.0))
#define NANOSECONDS_TO_SECONDS(val) ((double)(val) * (1.0 / 1000000000.0))
#define NANOSECONDS_TO_MILLISECONDS(val) ((double)(val) * (1.0 / 1000000.0))

//
// General PTP constants
//

#define PTP_VERSION 2 // PTP version
#define PTP_NETWORK_VERSION 1 // PTP Network version
#define PTP_VERSION_STRING AIRPLAY_SDK_REVISION

#define SYNC_RECEIPT_TIMEOUT_MULTIPLIER 8 // Sync receipt timeout multiplier (according to APTP spec, this should be 8x higher than the initial value)
#define ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER 120 // Announce receipt timeout multiplier (from APTP spec)

//For Airplay PTP msg recv max timeout in Nanoseconds
//default is 1s but it is too short to cover bad network.
//Usually we should be recv msg within 1s but to make it more flexible && avoid master clock swithing too frequently.
//If it took too long to recv Sync msg from Master, Then we must restart PTP sync as it is too off.
#define PTP_AVERAGE_MSG_RECV_INTERVAL 3000000000.0

#define LINK_DELAY_INTERVAL 0 // 1 sec
#define SYNC_INTERVAL_INITIAL_LOG -3 // 125 ms (sync interval initial value, log scale)

#define EVENT_PORT 319 // event PTP message port number
#define GENERAL_PORT 320 // general PTP message port number

#define MAX_ROUNDTRIP_TIME_NS MILLISECONDS_TO_NANOSECONDS(80) // upper roundtrip time limit (80 ms)
#define PTP_CLOCK_RATIO_FILTER_ALPHA (63.0 / 64.0) // exponential filter value
#define PTP_CLOCK_RATIO_FILTER_BETA (1.0 - PTP_CLOCK_RATIO_FILTER_ALPHA) // exponential filter complementary value

#define FOREIGN_MASTER_THRESHOLD 2 // the maximum number of queued announce messages in ForeingClock objects

//
// PTP Message constants
//

#define PTP_MAX_MESSAGE_SIZE 128 // maximum supported PTP message size
#define PTP_MAX_SUCCESS_CNT 4
#define PTP_MAX_BAD_DIFF_INITIAL_CNT 5
#define PTP_MAX_BAD_DIFF_CNT 10
#define PATH_TRACE_TLV_TYPE 0x8 // The value that indicates the TLV is a path trace TLV, as specified in 16.2.7.1 and Table 34 of IEEE Std 1588-2008
#define NEGATIVE_TIME_JUMP 0.0 // value indicating negative time jump in follow_up message
#define PTP_MSG_HISTORY_LEN 16 // How many concurrent PTP sequences can we track at once

//
// Network related constants
//

#define CLOCK_IDENTITY_LENGTH 8 // Size of a clock identifier stored in the ClockIndentity class, described at IEEE 802.1AS Clause 8.5.2.4
#define CLOCK_IDENTITY_STR_LEN (CLOCK_IDENTITY_LENGTH * 3 + 1)

#if !defined(AIRPLAY_IPV4_ONLY)
#define ADDRESS_STRING_LENGTH INET6_ADDRSTRLEN
#else
#define ADDRESS_STRING_LENGTH INET_ADDRSTRLEN
#endif

#define ETHER_ADDR_OCTETS 6 // Number of octets in a link layer address

#if !defined(AIRPLAY_IPV4_ONLY)
#define IPV6_ADDR_OCTETS 16 // number of octets in ipv6 link layer address
#endif

#define REMOTE_CLOCK_FILTER_SIZE 5 // averaging filter size
#define REMOTE_CLOCK_SYNC_MIN_COUNT 2 // used to be 3
#define INVALID_LINKDELAY 3600000000000.0
#define REMOTE_CLOCK_DIFF_TOLERANCE MILLISECONDS_TO_NANOSECONDS(1) // maximum remote clock difference required for fast PTP lock

//
// Known network link speed configuration values
//

#define LINKSPEED_10GBIT 10000000
#define LINKSPEED_2_5GBIT 2500000
#define LINKSPEED_1GBIT 1000000
#define LINKSPEED_100MBIT 100000
#define INVALID_LINKSPEED UINT_MAX

#if (!defined(CLOCK_PTP_GET_TIME))
#if (defined(CLOCK_MONOTONIC_RAW))
#define CLOCK_PTP_GET_TIME CLOCK_MONOTONIC_RAW
#elif (defined(CLOCK_MONOTONIC))
#define CLOCK_PTP_GET_TIME CLOCK_MONOTONIC
#else
#warning Should not reach here
#endif
#endif // CLOCK_PTP_GET_TIME

#endif // _PTP_CONSTANTS_H_
