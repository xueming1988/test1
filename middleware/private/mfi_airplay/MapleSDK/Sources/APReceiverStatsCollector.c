/*
	Copyright (C) 2015-2019 Apple Inc. All Rights Reserved.
*/

#include "APReceiverStatsCollector.h"

#include "CFUtils.h"
#include "DebugServices.h"
#include "LogUtils.h"
#include "MathUtils.h"
#include "MiscUtils.h"
#include "PrintFUtils.h"
#include "StringUtils.h"
#include "TickUtils.h"

#include CF_HEADER
#include CF_RUNTIME_HEADER

#include "AirPlayCommon.h" // WiFi stats
#include "AirPlayUtils.h" // WiFi stats

#if 0
#pragma mark == Constants ==
#endif

#if 0
#pragma mark == Logging ==
#endif

ulog_define(APReceiverStatsCollector, kLogLevelNotice, kLogFlags_Default, "APReceiverStatsCollector", NULL);
#define ap_ucat() &log_category_from_name(APReceiverStatsCollector)
#define ap_ulog(LEVEL, ...) ulog(ap_ucat(), (LEVEL), __VA_ARGS__)
#define ap_dlog(LEVEL, ...) dlogc(ap_ucat(), (LEVEL), __VA_ARGS__)

ulog_define(APReceiverStatsCollectorTiming, kLogLevelTrace, kLogFlags_None, "APReceiverStatsCollector", NULL);
#define ap_timing_ulog(LEVEL, ...) ulog(&log_category_from_name(APReceiverStatsCollectorTiming), (LEVEL), __VA_ARGS__)
#define ap_timing_ulog_private(LEVEL, ...) ulog(&log_category_from_name(APReceiverStatsCollectorTiming), (LEVEL), __VA_ARGS__)

#if 0
#pragma mark == Structures ==
#endif

struct OpaqueAPReceiverStatsCollector {
    CFRuntimeBase base; // must be the first field
    dispatch_queue_t queue;
    AirPlayStreamType type;
    uint64_t streamID;
    RTPJitterBufferContext* jitterBufferContextPtr;

    // Stats counters
    int32_t audioBufferedMs;
    int32_t audioLostPackets;
    int32_t audioUnrecoveredPackets;
    int32_t audioLatePackets;

    int32_t audioLateSamples;
    int32_t audioGaps;
    int32_t audioSkipped;

    int32_t screenFrameCount;
    int32_t screenLastFrameCount;

    int32_t screenLatencyMs;
    EWMA_FP_Data screenAheadAverage;

    CFMutableStringRef frameStatsHeader;

    CFAbsoluteTime lastShowTime;

    // Screen frame timestamps
    uint64_t lastFrameTimestamp;
    int32_t sendTimestampCount;
    int32_t recvTimestampCount;

    // State
    Boolean logEnabled;
};

#if 0
#pragma mark == CF Stuff ==
#endif

static void aprscreen_Finalize(CFTypeRef inObj)
{
    APReceiverStatsCollectorRef collector = (APReceiverStatsCollectorRef)inObj;

    ap_ulog(kLogLevelTrace, "Finalizing stats collector %p\n", collector);

    if (collector->frameStatsHeader) {
        CFRelease(collector->frameStatsHeader);
        collector->frameStatsHeader = NULL;
    }
    if (collector->queue) {
        dispatch_release(collector->queue);
        collector->queue = NULL;
    }
}

static CFStringRef aprstats_CopyDebugDescription(CFTypeRef inObj)
{
    APReceiverStatsCollectorRef collector = (APReceiverStatsCollectorRef)inObj;
    CFStringRef str = NULL;

    str = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
        CFSTR("<APReceiverStatsCollector %p>"), (void*)collector);

    return str;
}

static const CFRuntimeClass kAPReceiverStatsCollectorClass = {
    0, // version
    "APReceiverStatsCollector", // className
    NULL, // init
    NULL, // copy
    aprscreen_Finalize, // finalize
    NULL, // equal -- NULL means pointer equality.
    NULL, // hash  -- NULL means pointer hash.
    NULL, // copyFormattingDesc
    aprstats_CopyDebugDescription, // copyDebugDesc
    NULL, // reclaim
    NULL // refcount
};

static dispatch_once_t gAPReceiverStatsCollectorInitOnce = 0;
static CFTypeID gAPReceiverStatsCollectorTypeID = _kCFRuntimeNotATypeID;

static void _GetTypeID(void* inContext)
{
    (void)inContext;

    gAPReceiverStatsCollectorTypeID = _CFRuntimeRegisterClass(&kAPReceiverStatsCollectorClass);
    check(gAPReceiverStatsCollectorTypeID != _kCFRuntimeNotATypeID);
}

CFTypeID APReceiverStatsCollectorGetTypeID(void)
{
    dispatch_once_f(&gAPReceiverStatsCollectorInitOnce, NULL, _GetTypeID);
    return gAPReceiverStatsCollectorTypeID;
}

#if 0
#pragma mark == Creation ==
#endif

APReceiverStatsCollectorRef APReceiverStatsCollectorCreate(uint64_t inStreamID, AirPlayStreamType inType)
{
    OSStatus err = kNoErr;
    APReceiverStatsCollectorRef collector = NULL;
    size_t extra = sizeof(struct OpaqueAPReceiverStatsCollector) - sizeof(CFRuntimeBase);

    ap_ulog(kLogLevelTrace, "Creating APReceiverStatsCollector\n");

    collector = (APReceiverStatsCollectorRef)_CFRuntimeCreateInstance(kCFAllocatorDefault, APReceiverStatsCollectorGetTypeID(), extra, NULL);
    require_action(collector != NULL, bail, err = kNoMemoryErr);
    memset((char*)collector + sizeof(CFRuntimeBase), 0, sizeof(struct OpaqueAPReceiverStatsCollector) - sizeof(CFRuntimeBase));
    collector->screenLastFrameCount = 0;

    collector->queue = dispatch_queue_create("com.apple.airplay.receiver.statscollector", NULL);
    require_action(collector->queue, bail, err = kNoMemoryErr);

    collector->screenLatencyMs = kAirPlayScreenLatencyWiFiMs;
    EWMA_FP_Init(&collector->screenAheadAverage, 0.5, kEWMAFlags_StartWithFirstValue);

    collector->streamID = inStreamID;
    collector->type = inType;

    collector->jitterBufferContextPtr = NULL;

    ap_ulog(kLogLevelTrace, "Created stats collector %p\n", collector);

bail:
    if (err) {
        ap_ulog(kLogLevelError, "### APReceiverStatsCollectorCreate failed, error: %#m\n", err);
        if (collector) {
            CFRelease(collector);
            collector = NULL;
        }
    }

    return collector;
}

#if 0
#pragma mark == API ==
#endif

// Setup

static void aprstats_configureScreen(APReceiverStatsCollectorRef inCollector, CFDictionaryRef inOptions)
{
    OSStatus err = kNoErr;
    CFArrayRef sendTimestampNames = NULL;
    size_t count, i;

    ap_ulog(kLogLevelTrace, "Configuring stats collector %p with screen options: %@\n", inCollector, inOptions);

    inCollector->screenLatencyMs = CFDictionaryGetSInt32(inOptions, CFSTR("latencyMs"), &err);

    // Create frame stats header

    inCollector->sendTimestampCount = 0;
    inCollector->recvTimestampCount = 0;

    if (inCollector->frameStatsHeader)
        CFRelease(inCollector->frameStatsHeader);

    inCollector->frameStatsHeader = CFStringCreateMutable(kCFAllocatorDefault, 0);

    CFStringAppendF(inCollector->frameStatsHeader, "%10s", "Timestamp");
    CFStringAppendF(inCollector->frameStatsHeader, " %6s", "Frame");

    // Sender side timestamp names
    sendTimestampNames = CFDictionaryGetCFArray(inOptions, CFSTR("timestampInfo"), NULL);
    count = sendTimestampNames ? CFArrayGetCount(sendTimestampNames) : 0;
    check(count <= 16); // check against the hard limit
    count = Min(count, 16);
    if (count > 0) {
        CFDictionaryRef dict = NULL;
        CFStringRef str = NULL;

        for (i = 0; i < count; i++) {
            dict = CFArrayGetCFDictionaryAtIndex(sendTimestampNames, i, NULL);
            if (!dict)
                continue;

            str = CFDictionaryGetCFString(dict, CFSTR("name"), NULL);
            if (!str)
                continue;

            if (CFEqual(str, CFSTR("HIDIn")))
                str = CFSTR("DeltF");
            CFStringAppendF(inCollector->frameStatsHeader, " %5@", str);
            inCollector->sendTimestampCount++;
        }
    }

    // Receiver side timestamp names
    CFStringAppendF(inCollector->frameStatsHeader, " %5s", "RcvSt"); // ReceivedStart
    CFStringAppendF(inCollector->frameStatsHeader, " %5s", "RcvHd"); // ReceivedHeader
    CFStringAppendF(inCollector->frameStatsHeader, " %5s", "RcvBd"); // ReceivedBody
    CFStringAppendF(inCollector->frameStatsHeader, " %5s", "CMBuf"); // CMBlockBufferWrap
    inCollector->recvTimestampCount += 4;

    // The rest
    CFStringAppendF(inCollector->frameStatsHeader, " %s", "Total");
    CFStringAppendF(inCollector->frameStatsHeader, " %s", "Ahead");
    CFStringAppendF(inCollector->frameStatsHeader, " %s", "ReEn");
    CFStringAppendF(inCollector->frameStatsHeader, " %s", " Bytes");
}

typedef struct
{
    APReceiverStatsCollectorRef collector;
    CFDictionaryRef options;
} CollectorOptionsContext;

static void _APReceiverStatsCollectorSetScreenOptions(void* inCtx)
{
    CollectorOptionsContext* context = (CollectorOptionsContext*)inCtx;
    aprstats_configureScreen(context->collector, context->options);
}

void APReceiverStatsCollectorSetScreenOptions(APReceiverStatsCollectorRef inCollector, CFDictionaryRef inOptions)
{
    CollectorOptionsContext context;

    if (inCollector && inOptions) {
        context.collector = inCollector;
        context.options = inOptions;

        dispatch_sync_f(inCollector->queue, &context, _APReceiverStatsCollectorSetScreenOptions);
    }
}

void APReceiverStatsCollectorSetJitterBufferContextPtr(APReceiverStatsCollectorRef inCollector, RTPJitterBufferContext* inJitterBufferContextPtr)
{
    if (inCollector) {
        inCollector->jitterBufferContextPtr = inJitterBufferContextPtr;
    }
}

// Logging

typedef struct
{
    APReceiverStatsCollectorRef collector;
    Boolean enable;
} LogEnableContext;

static void _aprstats_logEnable(void* inCtx)
{
    LogEnableContext* context = (LogEnableContext*)inCtx;

    context->collector->logEnabled = context->enable;
}

static void aprstats_logEnable(APReceiverStatsCollectorRef inCollector, Boolean inEnable)
{
    LogEnableContext context;

    if (inCollector) {
        context.collector = inCollector;
        context.enable = inEnable;

        dispatch_sync_f(inCollector->queue, &context, _aprstats_logEnable);
    }
}

void APReceiverStatsCollectorEnableLogging(APReceiverStatsCollectorRef inCollector)
{
    aprstats_logEnable(inCollector, true);
}

void APReceiverStatsCollectorDisableLogging(APReceiverStatsCollectorRef inCollector)
{
    aprstats_logEnable(inCollector, false);
}

// Show stats

CFNumberRef APReceiverStatsCollectorCopyStreamID(APReceiverStatsCollectorRef inCollector)
{
    CFNumberRef streamID = NULL;

    if (inCollector) {
        streamID = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &inCollector->streamID);
    }

    return streamID;
}

static void aprstats_showStats(APReceiverStatsCollectorRef inCollector)
{
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    double delta = now - inCollector->lastShowTime;
    int32_t count = 0, receivedFPS = 0, latencyMs = 0;
    int32_t aheadMs = (int32_t)EWMA_FP_Get(&inCollector->screenAheadAverage);

    if (delta < 0.5)
        return; // don't do it too often
    inCollector->lastShowTime = now;

    count = inCollector->screenFrameCount;
    receivedFPS = (int32_t)((count - inCollector->screenLastFrameCount) / delta + 0.5);
    inCollector->screenLastFrameCount = count;

    latencyMs = inCollector->screenLatencyMs - aheadMs;
    latencyMs = Max(latencyMs, inCollector->screenLatencyMs);

    if (inCollector->logEnabled) {
        ap_ulog(kLogLevelNotice | kLogLevelFlagContinuation,
            "R-FPS: %u  "
            "Latency: %d  "
            "Ahead: %d  "
            "A Buff: %u ms  "
            "A Lost: %u  "
            "A Unrec: %u  "
            "A Late: %u  "
            "MA Late %u  "
            "MA Gaps %u  "
            "MA Skipped %u  "
            "AA Late %u  "
            "AA Gaps %u  "
            "AA Skipped %u\n",
            receivedFPS,
            latencyMs,
            aheadMs,
            inCollector->audioBufferedMs,
            inCollector->audioLostPackets,
            inCollector->audioUnrecoveredPackets,
            inCollector->audioLatePackets);
    }
}

static void _APReceiverStatsCollectorShowStats(void* inCtx)
{
    APReceiverStatsCollectorRef collector = (APReceiverStatsCollectorRef)inCtx;

    if (collector->logEnabled)
        aprstats_showStats(collector);
}

void APReceiverStatsCollectorShowStats(APReceiverStatsCollectorRef inCollector)
{
    if (inCollector) {
        dispatch_sync_f(inCollector->queue, inCollector, _APReceiverStatsCollectorShowStats);
    }
}

static CFMutableDictionaryRef _MainHighAudioCopyStats(APReceiverStatsCollectorRef inCollector)
{
    CFMutableDictionaryRef statsDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    if (statsDict) {
        CFDictionarySetInt64(statsDict, CFSTR(kAirPlayMetricKey_AudioBufferedMs), (int64_t)inCollector->audioBufferedMs);
        CFDictionarySetInt64(statsDict, CFSTR(kAirPlayMetricKey_AudioLostPackets), (int64_t)inCollector->audioLostPackets);
        CFDictionarySetInt64(statsDict, CFSTR(kAirPlayMetricKey_AudioUnrecoveredPackets), (int64_t)inCollector->audioUnrecoveredPackets);
        CFDictionarySetInt64(statsDict, CFSTR(kAirPlayMetricKey_AudioLatePackets), (int64_t)inCollector->audioLatePackets);
    }

    return statsDict;
}

static CFMutableDictionaryRef _MainAltCopyStats(APReceiverStatsCollectorRef inCollector)
{
    CFMutableDictionaryRef statsDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    if (inCollector->jitterBufferContextPtr) {
        inCollector->audioLateSamples = inCollector->jitterBufferContextPtr->nLate;
        inCollector->audioGaps = inCollector->jitterBufferContextPtr->nGaps;
        inCollector->audioSkipped = inCollector->jitterBufferContextPtr->nSkipped;
    } else {
        inCollector->audioLateSamples = 0;
        inCollector->audioGaps = 0;
        inCollector->audioSkipped = 0;
    }

    if (statsDict) {
        CFDictionarySetInt64(statsDict, CFSTR(kAirPlayMetricKey_AudioLateSamples), (int64_t)inCollector->audioLateSamples);
        CFDictionarySetInt64(statsDict, CFSTR(kAirPlayMetricKey_AudioGaps), (int64_t)inCollector->audioGaps);
        CFDictionarySetInt64(statsDict, CFSTR(kAirPlayMetricKey_AudioSkipped), (int64_t)inCollector->audioSkipped);
    }

    return statsDict;
}

static CFMutableDictionaryRef _ScreenCopyStats(APReceiverStatsCollectorRef inCollector)
{
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    double delta = now - inCollector->lastShowTime;
    int32_t count = inCollector->screenFrameCount, aheadMs = (int32_t)EWMA_FP_Get(&inCollector->screenAheadAverage);
    CFMutableDictionaryRef statsDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    if (statsDict) {
        CFDictionarySetInt64(statsDict, CFSTR(kAirPlayMetricKey_ReceivedFPS), (int64_t)(count - inCollector->screenLastFrameCount) / delta + 0.5);
        CFDictionarySetInt64(statsDict, CFSTR(kAirPlayMetricKey_LatencyMs), (int64_t)Max(inCollector->screenLatencyMs - aheadMs, inCollector->screenLatencyMs));
        CFDictionarySetInt64(statsDict, CFSTR(kAirPlayMetricKey_AheadMs), (int64_t)aheadMs);

        inCollector->screenLastFrameCount = count;
        inCollector->lastShowTime = now;
    }

    return statsDict;
}

void APReceiverStatsCollectorCopyStats(APReceiverStatsCollectorRef inCollector, CFDictionaryRef* outStatsDictionary)
{
    CFMutableDictionaryRef statsDict = NULL;

    if (inCollector) {
        switch (inCollector->type) {
        case kAirPlayStreamType_MainHighAudio:
            statsDict = _MainHighAudioCopyStats(inCollector);
            break;

        case kAirPlayStreamType_MainAudio:
        case kAirPlayStreamType_AltAudio:
            statsDict = _MainAltCopyStats(inCollector);
            break;

        case kAirPlayStreamType_Screen:
            statsDict = _ScreenCopyStats(inCollector);
            break;
        default:
            break;
        }

        if (statsDict)
            CFDictionarySetInt64(statsDict, CFSTR(kAirPlayMetricKey_StreamType), (int64_t)inCollector->type);

        if (outStatsDictionary) {
            *outStatsDictionary = statsDict;
        } else {
            CFRelease(statsDict);
        }
    }
}

// Audio stats

void APReceiverStatsCollectorUpdateAudioBufferDuration(APReceiverStatsCollectorRef inCollector, int32_t inDurationMs)
{
    if (inCollector) {
        // Just set it
        inCollector->audioBufferedMs = inDurationMs;
    }
}

void APReceiverStatsCollectorAddAudioLostPacketCount(APReceiverStatsCollectorRef inCollector, int32_t inCount)
{
    if (inCollector)
        inCollector->audioLostPackets += inCount;
}

void APReceiverStatsCollectorAddAudioUnrecoveredPacketCount(APReceiverStatsCollectorRef inCollector, int32_t inCount)
{
    if (inCollector)
        inCollector->audioUnrecoveredPackets += inCount;
}

void APReceiverStatsCollectorAddAudioLatePacketCount(APReceiverStatsCollectorRef inCollector, int32_t inCount)
{
    if (inCollector)
        inCollector->audioLatePackets += inCount;
}

// Screen stats

typedef struct
{
    CFTypeRef* assigneePtr;
    CFTypeRef newValue;
} CFReleaseAssignAndRetainContext;

// Frame stats

#define kMaxPrintMs 999

#define MillisecondsToNTP(MS) (((MS)*UINT64_C(0x100000000)) / 1000)
#define NTPFractionToMilliseconds(NTP) \
    ((uint32_t)((1000 * ((NTP)&UINT32_C(0xFFFFFFFF))) / UINT32_C(0xFFFFFFFF)))

static CFMutableDictionaryRef aprstats_createFrameTimingDictionary(
    APReceiverStatsCollectorRef inCollector,
    const APReceiverStatsCollectorFrameStats* inFrameStats)
{
    CFMutableDictionaryRef frameStatsDict = NULL;
    int64_t delta = 0;
    const uint16_t* tsSrc;
    const uint16_t* tsEnd;
    uint64_t ts, ts2;
    char buf[256];
    char* dst;
    char* lim;
    int count, i;

    frameStatsDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    buf[0] = '\0';
    dst = buf;
    lim = buf + sizeof(buf);

    ts = inFrameStats->anchorTimestamp;
    if (ts == 0)
        ts = inFrameStats->startTimestamp;
    delta = (int64_t)(ts - inCollector->lastFrameTimestamp);
    delta = (delta > 0) ? NTPFractionToMilliseconds(delta) : 0;
    snprintf_add(&dst, lim, "%5d ", (int)Min(delta, kMaxPrintMs));
    inCollector->lastFrameTimestamp = ts;

    // Sender side timestamps
    ts = inFrameStats->startTimestamp;
    count = (inCollector->sendTimestampCount > 0) ? inCollector->sendTimestampCount - 1 : 0;
    count = Min(count, inFrameStats->sendTimestampCount);
    tsSrc = inFrameStats->sendTimestampDeltas;
    tsEnd = tsSrc + count;
    for (; tsSrc < tsEnd; tsSrc++) {
        snprintf_add(&dst, lim, "%5d ", (int)Clamp(*tsSrc, 0, kMaxPrintMs));
        ts += MillisecondsToNTP(*tsSrc);
    }

    // Receiver side timestamps
    // If set, the number of timestamp names should be the same as the number of timestamps
    check(inCollector->recvTimestampCount == 0 || inCollector->recvTimestampCount == inFrameStats->recvTimestampCount);
    for (i = 0; i < inFrameStats->recvTimestampCount; i++) {
        ts2 = inFrameStats->recvTimestamps[i];
        delta = (ts2 > ts) ? NTPFractionToMilliseconds(ts2 - ts) : 0;
        snprintf_add(&dst, lim, "%5d ", (int)Min(delta, kMaxPrintMs));
        ts = ts2;
    }

    // Save deltas
    CFDictionarySetCString(frameStatsDict, CFSTR("deltasMs"), buf, kSizeCString);

    // Total
    delta = (ts > inFrameStats->startTimestamp) ? NTPFractionToMilliseconds(ts - inFrameStats->startTimestamp) : 0;
    CFDictionarySetInt64(frameStatsDict, CFSTR("totalMs"), (int64_t)delta);

    return frameStatsDict;
}

static void aprstats_updateAndCopyFrameStats(
    APReceiverStatsCollectorRef inCollector,
    const APReceiverStatsCollectorFrameStats* inFrameStats,
    CFDictionaryRef* outFrameStatsDict)
{
    uint64_t nowTicks = UpTicks();
    int32_t aheadMs = 0;

    inCollector->screenFrameCount++;

    if (inFrameStats->displayTicks >= nowTicks) {
        aheadMs = (int32_t)UpTicksToMilliseconds(inFrameStats->displayTicks - nowTicks);
    } else {
        aheadMs = -(int32_t)UpTicksToMilliseconds(nowTicks - inFrameStats->displayTicks);
    }
    EWMA_FP_Update(&inCollector->screenAheadAverage, aheadMs);

    if (aheadMs < -inCollector->screenLatencyMs)
        ap_ulog(kLogLevelNotice, "*** Stats collector %p: late screen frame (%d ms)\n", inCollector, aheadMs);

    if (outFrameStatsDict) {
        CFMutableDictionaryRef frameStatsDict = aprstats_createFrameTimingDictionary(inCollector, inFrameStats);
        CFMutableStringRef preLine = CFStringCreateMutable(kCFAllocatorDefault, 0);

        CFStringAppendF(preLine, "%10.0llu %6u ", UpTicksToMilliseconds(inFrameStats->displayTicks), inCollector->screenFrameCount);
        CFDictionarySetValue(frameStatsDict, CFSTR("preLine"), preLine);
        CFRelease(preLine);

        CFDictionarySetInt64(frameStatsDict, CFSTR("aheadMs"), (int64_t)aheadMs);
        CFDictionarySetInt64(frameStatsDict, CFSTR("reencodeCount"), (int64_t)inFrameStats->reencodeCount);
        CFDictionarySetInt64(frameStatsDict, CFSTR("frameSize"), (int64_t)inFrameStats->frameSize);
        CFDictionarySetInt64(frameStatsDict, CFSTR("recvTicks"), (int64_t)nowTicks);

        if (inCollector->frameStatsHeader && (uint32_t)inCollector->screenFrameCount % 100 == 0) {
            CFDictionarySetValue(frameStatsDict, CFSTR("header"), inCollector->frameStatsHeader);
        }
        *outFrameStatsDict = frameStatsDict;
    }
}

typedef struct
{
    APReceiverStatsCollectorRef collector;
    const APReceiverStatsCollectorFrameStats* inFrameStats;
    CFDictionaryRef* outFrameStatsDict;
} UpdateAndCopyFrameStatsContext;

static void _APReceiverStatsCollectorUpdateAndCopyFrameStats(void* inCtx)
{
    UpdateAndCopyFrameStatsContext* context = (UpdateAndCopyFrameStatsContext*)inCtx;

    aprstats_updateAndCopyFrameStats(context->collector, context->inFrameStats, context->outFrameStatsDict);
}

void APReceiverStatsCollectorUpdateAndCopyFrameStats(
    APReceiverStatsCollectorRef inCollector,
    const APReceiverStatsCollectorFrameStats* inFrameStats,
    CFDictionaryRef* outFrameStatsDict)
{
    UpdateAndCopyFrameStatsContext context;

    if (inCollector) {
        context.collector = inCollector;
        context.inFrameStats = inFrameStats;
        context.outFrameStatsDict = outFrameStatsDict;

        dispatch_sync_f(inCollector->queue, &context, _APReceiverStatsCollectorUpdateAndCopyFrameStats);
    }
}

void APReceiverStatsCollectorLogFrameStats(CFDictionaryRef inFrameStats, CFDictionaryRef inImageTimingInfo)
{
    OSStatus err = kNoErr;
    CFStringRef header = NULL;
    char line[256];
    char* dst;
    char* lim;
    int32_t totalMs = 0, aheadMs = 0, reencodeCount = 0, frameSize = 0;

    require_quiet(inFrameStats, bail);

    totalMs = CFDictionaryGetSInt32(inFrameStats, CFSTR("totalMs"), &err);
    aheadMs = CFDictionaryGetSInt32(inFrameStats, CFSTR("aheadMs"), &err);
    reencodeCount = CFDictionaryGetSInt32(inFrameStats, CFSTR("reencodeCount"), &err);
    frameSize = CFDictionaryGetSInt32(inFrameStats, CFSTR("frameSize"), &err);

    line[0] = '\0';
    dst = line;
    lim = line + sizeof(line);

    if (inImageTimingInfo) {
        uint64_t imgTS[6] = { 0 };
        uint64_t ts = 0, ts2 = 0;
        int32_t delta = 0;
        int i;

        imgTS[0] = ts;

        for (i = 0; i < (int)countof(imgTS); i++) {
            ts2 = imgTS[i];
            if (ts2 != 0)
                ts = ts2;
            else
                imgTS[i] = ts;
        }

        ts = 0;
        ts = CFDictionaryGetInt64(inFrameStats, CFSTR("recvTicks"), &err);
        delta = (imgTS[0] > ts) ? (int32_t)UpTicksToMilliseconds(imgTS[0] - ts) : 0;
        snprintf_add(&dst, lim, "%5d ", Min(delta, kMaxPrintMs));
        totalMs += delta;

        for (i = 1; i < 5; i++) {
            delta = (imgTS[i] > imgTS[i - 1]) ? (int32_t)UpTicksToMilliseconds(imgTS[i] - imgTS[i - 1]) : 0;
            snprintf_add(&dst, lim, "%5d ", Min(delta, kMaxPrintMs));
            totalMs += delta;
        }
    }

    snprintf_add(&dst, lim, "%5d ", Min(totalMs, kMaxPrintMs));
    snprintf_add(&dst, lim, "%+5d ", Clamp(aheadMs, -kMaxPrintMs, kMaxPrintMs));
    snprintf_add(&dst, lim, "%4d ", reencodeCount);
    snprintf_add(&dst, lim, "%6d", frameSize);

    header = (CFStringRef)CFDictionaryGetValue(inFrameStats, CFSTR("header"));
    if (header) {
        ap_timing_ulog(kLogLevelVerbose, "\n");
        ap_timing_ulog(kLogLevelVerbose, "%@\n", header);
        ap_timing_ulog(kLogLevelVerbose, "\n");
    }

    ap_timing_ulog(kLogLevelVerbose, "%@%@%s\n",
        CFDictionaryGetValue(inFrameStats, CFSTR("preLine")),
        CFDictionaryGetValue(inFrameStats, CFSTR("deltasMs")),
        line);

bail:
    return;
}
