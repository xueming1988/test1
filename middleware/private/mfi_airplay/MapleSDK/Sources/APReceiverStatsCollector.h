/*
	Copyright (C) 2015-2019 Apple Inc. All Rights Reserved.
*/

#ifndef APRECEIVERSTATSCOLLECTOR_H
#define APRECEIVERSTATSCOLLECTOR_H

#include "CommonServices.h"
#include "NetUtils.h"

#include "AirPlayCommon.h"
#include "AirPlayUtils.h"

#ifdef __cplusplus
extern "C" {
#endif

// A CF type
typedef struct OpaqueAPReceiverStatsCollector* APReceiverStatsCollectorRef;

EXPORT_GLOBAL CFTypeID APReceiverStatsCollectorGetTypeID(void);

// Will return NULL if stats collection is disabled
EXPORT_GLOBAL APReceiverStatsCollectorRef APReceiverStatsCollectorCreate(uint64_t inStreamID, AirPlayStreamType inType);

// All collector functions below are NULL-safe (that is they do nothing if inCollector is NULL)

// Initial setup
EXPORT_GLOBAL void APReceiverStatsCollectorSetScreenOptions(APReceiverStatsCollectorRef inCollector, CFDictionaryRef inOptions);
EXPORT_GLOBAL void APReceiverStatsCollectorSetJitterBufferContextPtr(APReceiverStatsCollectorRef inCollector, RTPJitterBufferContext* inJitterBufferContextPtr);

// Logging
EXPORT_GLOBAL void APReceiverStatsCollectorEnableLogging(APReceiverStatsCollectorRef inCollector);
EXPORT_GLOBAL void APReceiverStatsCollectorDisableLogging(APReceiverStatsCollectorRef inCollector);

// Update log stats (if enabled)
EXPORT_GLOBAL CFNumberRef APReceiverStatsCollectorCopyStreamID(APReceiverStatsCollectorRef inCollector);
EXPORT_GLOBAL void APReceiverStatsCollectorShowStats(APReceiverStatsCollectorRef inCollector);
EXPORT_GLOBAL void APReceiverStatsCollectorCopyStats(APReceiverStatsCollectorRef inCollector, CFDictionaryRef* outStatsDictionary);

// Audio stats
EXPORT_GLOBAL void APReceiverStatsCollectorUpdateAudioBufferDuration(APReceiverStatsCollectorRef inCollector, int32_t inDurationMs);
EXPORT_GLOBAL void APReceiverStatsCollectorAddAudioLostPacketCount(APReceiverStatsCollectorRef inCollector, int32_t inCount);
EXPORT_GLOBAL void APReceiverStatsCollectorAddAudioUnrecoveredPacketCount(APReceiverStatsCollectorRef inCollector, int32_t inCount);
EXPORT_GLOBAL void APReceiverStatsCollectorAddAudioLatePacketCount(APReceiverStatsCollectorRef inCollector, int32_t inCount);

// Frame stats
typedef struct {
    uint64_t displayTicks; // frame display time in host UpTicks
    uint64_t startTimestamp; // earliest sending side timestamp
    uint64_t anchorTimestamp; // anchor time for inter-frame deltas
    uint16_t* sendTimestampDeltas; // sending side timestamp deltas
    uint64_t* recvTimestamps; // receiving side timestamps
    int32_t sendTimestampCount; // number of sending side timestamps
    int32_t recvTimestampCount; // number of receiving side timestamps
    int32_t frameSize;
    int32_t reencodeCount;
} APReceiverStatsCollectorFrameStats;

void APReceiverStatsCollectorUpdateAndCopyFrameStats(
    APReceiverStatsCollectorRef inCollector,
    const APReceiverStatsCollectorFrameStats* inFrameStats,
    CFDictionaryRef* outFrameStatsDict);

// Utility function to log CFDictionary created by APReceiverStatsCollectorUpdateAndCopyFrameStats() and
// additional image timing info (if available)
void APReceiverStatsCollectorLogFrameStats(CFDictionaryRef inFrameStats, CFDictionaryRef inImageTimingInfo);

#define kAirPlayMetricKey_AudioLostPackets "lost"
#define kAirPlayMetricKey_AudioUnrecoveredPackets "unre"
#define kAirPlayMetricKey_AudioLatePackets "late"
#define kAirPlayMetricKey_AudioLateSamples "lasp"
#define kAirPlayMetricKey_AudioGaps "gaps"
#define kAirPlayMetricKey_AudioSkipped "skip"
#define kAirPlayMetricKey_LastShowTime "last"
#define kAirPlayMetricKey_ReceivedFPS "rfps"
#define kAirPlayMetricKey_LatencyMs "ltnc"
#define kAirPlayMetricKey_AheadMs "ahea"
#define kAirPlayMetricKey_AudioBufferedMs "abms"
#define kAirPlayMetricKey_StreamType "stty"

#ifdef __cplusplus
}
#endif

#endif // APRECEIVERSTATSCOLLECTOR_H
