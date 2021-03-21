/*
	Copyright (C) 2005-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

// Microsoft deprecated standard C APIs like fopen so disable those warnings because the replacement APIs are not portable.

#if (!defined(_CRT_SECURE_NO_DEPRECATE))
#define _CRT_SECURE_NO_DEPRECATE 1
#endif

#if 0
#pragma mark == Includes ==
#endif

#include "AirPlayReceiverSession.h"
#include "AirPlayAllocator.h"
#include "AirPlayReceiverCommand.h"
#include "AirPlayReceiverMetrics.h"
#include "AirPlayReceiverSessionPriv.h"
#include "AirPlayReceiverSkewCalculator.h"
#include "AirPlayVersion.h"
#include "AudioConverter.h"

#include "AESUtils.h"
#include "AirPlayNetworkAddress.h"
#include "CFLiteBinaryPlist.h"
#include "CFUtils.h"
#include "CommonServices.h"
#include "DebugServices.h"
#include "HTTPClient.h"
#include "HTTPUtils.h"
#include "MathUtils.h"
#include "NetTransportChaCha20Poly1305.h"
#include "NetUtils.h"
#include "RandomNumberUtils.h"
#include "StringUtils.h"
#include "TickUtils.h"
#include "TimeUtils.h"
#include "UUIDUtils.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "AirPlayCommon.h"
#include "AirPlayReceiverServer.h"
#include "AirPlayReceiverServerPriv.h"
#include "AirPlaySessionManager.h"
#include "AirPlayUtils.h"
#include "AirTunesClock.h"
#include "AirTunesPTPClock.h"

#include CF_HEADER

#if (TARGET_OS_POSIX)
#include <sys/types.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <sched.h>
#include <sys/socket.h>
#if (TARGET_OS_ANDROID)
#include <linux/sysctl.h>
#else
#include <sys/sysctl.h>
#endif
#endif
#include "Platform_linkplay.h"

#if 0
#pragma mark == Constants ==
#endif

//===========================================================================================================================
//	Constants
//===========================================================================================================================

#define kAirPlayInputRingSize 65536 // 371 millieconds at 44100 Hz.
#define kAirTunesRTPSocketBufferSize 524288 // 512 KB / 44100 Hz * 2 byte samples * 2 channels + headers ~= 2.8 seconds.
#define kAirTunesRTPOverTCPSocketBufferSize 16384 //
#define kAirTunesRTCPOSocketBufferSize 8192 //

#ifdef LINKPLAY
#define kAirTunesRTPOffsetResetThreshold 2203 // 50ms  @ 44100 Hz. An absolute diff of more than this resets.
#else
#define kAirTunesRTPOffsetResetThreshold 6615 // 150 ms @ 44100 Hz. An absolute diff of more than this resets.
#endif

#define kAirTunesMaxExcessiveDriftCount 2 // Exceed threshold multiple times for reset

#define kAirTunesRTPOffsetApplyThreshold 441000 // 10 seconds @ 44100 Hz. If delta > this, do an immediate reset.
#define kAirTunesBufferNodeCountUDP 512 // 512 nodes * 352 samples per node = ~4 seconds.
#define kAirTunesRetransmitMaxLoss 128 // Max contiguous loss to try to recover. ~2 second @ 44100 Hz
#define kAirTunesRetransmitCount 512 // Max number of outstanding retransmits.
#define kAirTunesSetRateSkewIntervalMS 200 // How often we'll update our skew compensation for SetRate
#define kAirTunesClockLockIntervalMS 1000 // After a PTP clock switch give the clock some time to lock.

#define kAirTunesPIDMin -10
#define kAirTunesPIDMax 10
#define kAirTunesHoldSkewMaxDurationNs 10000000000

#if (AIRPLAY_BUFFERED)
#define kAirTunesBufferedDefaultSize (8 * 1024 * 1024)
#define kAirTunesBufferedMinSize (3 * 1024 * 1024)
#endif

check_compile_time(kAirTunesBufferNodeCountUDP <= kAirTunesDupWindowSize);
check_compile_time(kAirTunesRetransmitCount <= kAirTunesDupWindowSize);

#define kAirPlayEventTimeoutNS (10ll * kNanosecondsPerSecond) // Timeout in nanosecond for event message
#define kAirPlayClientTypeUUID_MediaRemote CFSTR("1910A70F-DBC0-4242-AF95-115DB30604E1")

// The platform can define kAirTunesSkewLimit to use a different skew limit.

#ifndef kAirTunesSkewLimit
#define kAirTunesSkewLimit (kAirTunesPIDMax)
#endif

#ifndef kAirTunesSkewMin
#define kAirTunesSkewMin (2) // Must be > 0 to avoid div by 0
#endif

#if 0
#pragma mark == Structures ==
#endif

//===========================================================================================================================
//	Structures
//===========================================================================================================================

#if 0
#pragma mark == Prototypes ==
#endif

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

#define AirTunesFreeBufferNode(SESSION, NODE) \
    do {                                      \
        (NODE)->next->prev = (NODE)->prev;    \
        (NODE)->prev->next = (NODE)->next;    \
        (NODE)->next = (SESSION)->freeList;   \
        (SESSION)->freeList = (NODE);         \
        --(SESSION)->busyNodeCount;           \
                                              \
    } while (0)

#define NanosecondsToMilliseconds32(NANOS) \
    ((uint32_t)(((NANOS) == UINT64_MAX) ? 0 : ((NANOS) / kNanosecondsPerMillisecond)))

// General

static void _GetTypeID(void* inContext);
static void _Finalize(CFTypeRef inCF);
static void _EventReplyTimeoutCallback(void* inContext);
static void _PerformPeriodTasks(void* inContext);
static OSStatus _SessionLock(AirPlayReceiverSessionRef inSession);
static OSStatus _SessionUnlock(AirPlayReceiverSessionRef inSession);
static OSStatus _UpdateFeedback(AirPlayReceiverSessionRef inSession, CFDictionaryRef inInput, CFDictionaryRef* outOutput);

#if (AIRPLAY_META_DATA)
static void updateTrackProgress(AirPlayReceiverSessionRef inSession, uint32_t lastPlayedTS, TrackTimingMetaData trackTime);
#endif

// Control/Events

static OSStatus
_ControlSetup(
    AirPlayReceiverSessionRef inSession,
    CFDictionaryRef inRequestParams,
    CFMutableDictionaryRef inResponseParams);
static OSStatus _ControlIdleStateTransition(AirPlayReceiverSessionRef inSession, CFMutableDictionaryRef inResponseParams);
static void _ControlTearDown(AirPlayReceiverSessionRef inSession);
static OSStatus _ControlStart(AirPlayReceiverSessionRef inSession);

// GeneralAudio
#if defined(AIRPLAY_IPV4_ONLY)
static CFTypeRef filterKeepIPv4Addrs(CFTypeRef addressDescription, void* param);
#endif
static CFTypeRef ptpNodeToNetworkAddr(CFTypeRef anObject, void* interfaceParam);
static CFTypeRef networkAddrToString(CFTypeRef inAddress, void* unused);

static void AirPlayReceiverFlushPackets(AirPlayReceiverSessionRef inSession, AirPlayFlushPoint const* flushFrom, AirPlayFlushPoint flushUntil);
#if (AIRPLAY_BUFFERED)
static size_t _GeneralBufferedAudioSize(AirPlayReceiverSessionRef me);
#endif

static CFArrayRef createTimingAddresses(const char* interface, sockaddr_ip selfAddr, Boolean ipv4Only);
static OSStatus _GeneralAudioAddIPAddrs(AirPlayReceiverSessionRef me, CFMutableDictionaryRef inResponseParams);
static OSStatus
_GeneralAudioSetup(
    AirPlayReceiverSessionRef inSession,
    AirPlayStreamType inStreamType,
    CFDictionaryRef inStreamDesc,
    CFMutableDictionaryRef inResponseParams);
static void* _GeneralAudioThread(void* inArg);
static OSStatus _GeneralAudioReceiveRTCP(AirPlayReceiverSessionRef inSession, SocketRef inSock, RTCPType inExpectedType);
static OSStatus _GeneralAudioReceiveRTP(AirPlayReceiverSessionRef inSession, RTPPacket* inPkt, size_t inSize);
static OSStatus
_GeneralAudioProcessPacket(
    AirPlayReceiverSessionRef inSession,
    AirTunesBufferNode* inNode,
    size_t inSize,
    Boolean inIsRetransmit);
static OSStatus
_GeneralAudioDecodePacket(
    AirPlayReceiverSessionRef inSession,
    uint8_t* inAAD,
    size_t inAADSize,
    uint8_t* inSrc,
    size_t inSrcSize,
    uint8_t* inDst,
    size_t inDstSize,
    size_t* outSize);

#if (AIRPLAY_GENERAL_AUDIO_SKEW)
static void _GeneralAdjustForSkew(AirPlayReceiverSessionRef inSession, AirTunesBufferNode* inNode);
#endif

static size_t _GeneralAudioRender(AirPlayReceiverSessionRef inSession, uint32_t inRTPTime, void* inBuffer, size_t inSize, size_t inMinFillSize);
static Boolean _GeneralAudioTrackDups(AirPlayReceiverSessionRef inSession, uint16_t inSeq);
static void _GeneralAudioTrackLosses(AirPlayReceiverSessionRef inSession, AirTunesBufferNode* inNode);
static void _GeneralAudioUpdateLatency(AirPlayReceiverSessionRef inSession);

static OSStatus _RetransmitsSendRequest(AirPlayReceiverSessionRef inSession, uint16_t inSeqStart, uint16_t inSeqCount);
static OSStatus _RetransmitsProcessResponse(AirPlayReceiverSessionRef inSession, RTCPRetransmitResponsePacket* inPkt, size_t inSize);
static void _RetransmitsSchedule(AirPlayReceiverSessionRef inSession, uint16_t inSeqStart, uint16_t inSeqCount);
static void _RetransmitsUpdate(AirPlayReceiverSessionRef inSession, AirTunesBufferNode* inNode, Boolean inIsRetransmit);
static void _RetransmitsAbortAll(AirPlayReceiverSessionRef inSession, const char* inReason);
static void _RetransmitsAbortOne(AirPlayReceiverSessionRef inSession, uint32_t inSeq, const char* inReason);
static void _announcePlaying(AirPlayReceiverSessionRef inSession, Boolean isPlaying);

static OSStatus _ClockSetup(AirPlayReceiverSessionRef inSession, CFDictionaryRef inRequestParams);

// Timing

static OSStatus _TimingInitialize(AirPlayReceiverSessionRef inSession);
static OSStatus _TimingFinalize(AirPlayReceiverSessionRef inSession);
static OSStatus _TimingNegotiate(AirPlayReceiverSessionRef inSession);
static void* _TimingThread(void* inArg);
static OSStatus _TimingSendRequest(AirPlayReceiverSessionRef inSession);
static OSStatus _TimingReceiveResponse(AirPlayReceiverSessionRef inSession, SocketRef inSock);
static OSStatus _TimingProcessResponse(AirPlayReceiverSessionRef inSession, RTCPTimeSyncPacket* inPkt, const AirTunesTime* inTime);
#if (AIRPLAY_GENERAL_AUDIO_SKEW)
static void _TimingProcessTimeAnnounce(AirPlayReceiverSessionRef inSession, const AirTunesTime* inGlobalTime, uint32_t inRTPTime);
#endif

#if (AIRPLAY_BUFFERED)
static OSStatus _receiveBufferedRTP(AirPlayReceiverSessionRef inSession, NetSocketRef dataNetSock);
static OSStatus _bufferedMachineRead(NetSocketRef inSock, BufferedAudioMachine* bufferedMachine);
static void _bufferedMachineReset(BufferedAudioMachine* bufferedMachine);
static OSStatus _bufGetFreeNode(AirPlayReceiverSessionRef inSession, BufferedAudioMachine* bufferedMachine);
static OSStatus _bufReadPacketLength(AirPlayReceiverSessionRef inSession, NetSocketRef dataNetSock, BufferedAudioMachine* bufferedMachine);
static OSStatus _bufReadPacket(NetSocketRef dataNetSock, BufferedAudioMachine* bufferedMachine);
static OSStatus _bufProcessPacket(AirPlayReceiverSessionRef inSession, BufferedAudioMachine* bufferedMachine);
#endif

#define _UsingScreenOrAudio(ME) \
    (((ME)->mainAudioCtx.type != kAirPlayStreamType_Invalid) || ((ME)->altAudioCtx.type != kAirPlayStreamType_Invalid))

static void drift_updateRTPOffsetAndProcessSkew(AirPlayReceiverSessionRef inSession, uint64_t inHostTime, uint32_t inLocalSampleTime);
static OSStatus drift_convertHostTimeToNetworkTimeOnAnchorTimeline(AirPlayReceiverSessionRef inSession, uint64_t inHostTime, AirTunesTime* outNetworkTimeAtInputHostTime);

static OSStatus _RemoteControlSessionSetup(AirPlayReceiverSessionRef inSession,
    CFDictionaryRef inRequestParams,
    CFMutableDictionaryRef outResponseParams);
static OSStatus _CreateAndRegisterRemoteControlSession(AirPlayReceiverSessionRef inSession,
    CFNumberRef inRCSObjRef,
    CFStringRef inClientTypeUUID,
    CFStringRef inClientUUID,
    AirPlayRemoteControlSessionControlType inControlType,
    CFNumberRef inTokenRef);
static void _RemoteControlSessionTearDown(AirPlayReceiverSessionRef inSession, uint64_t playbackStreamID);

static OSStatus _SessionGenerateResponseStreamDesc(AirPlayReceiverSessionRef inSession,
    CFMutableDictionaryRef inResponseParams,
    uint32_t inStreamType,
    uint64_t inStreamID);

// Utils

static OSStatus _AddResponseStream(CFMutableDictionaryRef inResponseParams, CFDictionaryRef inStreamDesc);

static OSStatus _AudioDecoderInitialize(AirPlayReceiverSessionRef inSession);
static OSStatus
_AudioDecoderDecodeFrame(
    AirPlayReceiverSessionRef inSession,
    const uint8_t* inSrcPtr,
    size_t inSrcLen,
    uint8_t* inDstPtr,
    size_t inDstMaxLen,
    size_t* outDstLen);
static OSStatus
_AudioDecoderDecodeCallback(
    AudioConverterRef inAudioConverter,
    UInt32* ioNumberDataPackets,
    AudioBufferList* ioData,
    AudioStreamPacketDescription** outDataPacketDescription,
    void* inUserData);
#define _AudioDecoderConcealLoss(SESSION, PTR, LEN) memset((PTR), 0, (LEN))

int _CompareOSBuildVersionStrings(const char* inVersion1, const char* inVersion2);

#if (AIRPLAY_METRICS)
static CFDictionaryRef _CopyMetrics(AirPlayReceiverSessionRef inSession, OSStatus* outErr);
#endif

static OSStatus
_GetStreamSecurityKeys(
    AirPlayReceiverSessionRef inSession,
    uint64_t streamConnectionID,
    size_t inInputKeyLen,
    uint8_t* outInputKey,
    size_t inOutputKeyLen,
    uint8_t* outOutputKey);

static void _LogStarted(AirPlayReceiverSessionRef inSession, AirPlayReceiverSessionStartInfo* inInfo, OSStatus inStatus);
static void _LogEnded(AirPlayReceiverSessionRef inSession, OSStatus inReason);
static void _LogUpdate(AirPlayReceiverSessionRef inSession, uint64_t inTicks, Boolean inForce);
static void _TearDownStream(AirPlayReceiverSessionRef inSession, AirPlayAudioStreamContext* const ctx, Boolean inIsFinalizing);
static void _UnregisterRoutes(AirPlayReceiverSessionRef inSession);
static void _RouteRemoverApplierFunction(const void* key, const void* value, void* context);

static void notifyPlatformOfSender(AirPlayReceiverSessionRef inSession);

// Audio Buffer Allocation

static void allocator_setup(AirPlayReceiverSessionRef inSession, size_t bufferPoolSize, size_t fixedBlockSize);
static void allocator_finalize(AirPlayReceiverSessionRef inSession);
static AirTunesBufferNode* allocator_getNewNode(AirPlayReceiverSessionRef inSession, size_t packetSize);
static void allocator_freeNode(AirPlayReceiverSessionRef inSession, AirTunesBufferNode* node);

// Sequence Number Math

static Boolean Seq_LT(AirPlayReceiverSessionRef inSession, uint32_t a, uint32_t b);
static Boolean Seq_LE(AirPlayReceiverSessionRef inSession, uint32_t a, uint32_t b);
static uint32_t Seq_Cmp(AirPlayReceiverSessionRef inSession, uint32_t a, uint32_t b);
static uint32_t Seq_Inc(AirPlayReceiverSessionRef inSession, uint32_t a);
static uint32_t Seq_Diff(AirPlayReceiverSessionRef inSession, uint32_t a, uint32_t b);

#define Seq_GE(session, a, b) (!Seq_LT(session, a, b))
#define Seq_GT(session, a, b) (!Seq_LE(session, a, b))

static uint32_t APMod24_DiffWrappedAround(uint32_t a, uint32_t b);
static Boolean APMod24_LT(uint32_t a,
    uint32_t b);
#define Mod24_Cmp(A, B) ((int32_t)(((uint32_t)(A & MASK(24))) - ((uint32_t)(B & MASK(24)))))

// Debugging
extern Boolean kPerformSkewAdjustRightAway;
extern AirplayPrivControlContext gAirplayPrivCtrlCtx;

ulog_define(AirPlayReceiverCore, kLogLevelNotice, kLogFlags_Default, "AirPlay", NULL);
#define atr_ucat() &log_category_from_name(AirPlayReceiverCore)
#define atr_ulog(LEVEL, ...) ulog(atr_ucat(), (LEVEL), __VA_ARGS__)

ulog_define(AirPlayReceiverEvents, kLogLevelNotice, kLogFlags_Default, "AirPlay", NULL);
#define atr_events_ucat() &log_category_from_name(AirPlayReceiverEvents)

ulog_define(AirPlayReceiverStats, kLogLevelNotice, kLogFlags_Default, "AirPlay", "AirPlayReceiverStats:rate=5;3000");
#define atr_stats_ucat() &log_category_from_name(AirPlayReceiverStats)
#define atr_stats_ulog(LEVEL, ...) ulog(atr_stats_ucat(), (LEVEL), __VA_ARGS__)

#if (DEBUG)
#define airtunes_record_rtp_offset(X)                                      \
    do {                                                                   \
        gAirTunesRTPOffsetHistory[gAirTunesRTPOffsetIndex++] = (X);        \
        if (gAirTunesRTPOffsetIndex >= countof(gAirTunesRTPOffsetHistory)) \
            gAirTunesRTPOffsetIndex = 0;                                   \
        if (gAirTunesRTPOffsetCount < countof(gAirTunesRTPOffsetHistory))  \
            ++gAirTunesRTPOffsetCount;                                     \
                                                                           \
    } while (0)
#else
#define airtunes_record_rtp_offset(X)
#endif

#if 0
#pragma mark == Globals ==
#endif

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static const CFRuntimeClass kAirPlayReceiverSessionClass = {
    0, // version
    "AirPlayReceiverSession", // className
    NULL, // init
    NULL, // copy
    _Finalize, // finalize
    NULL, // equal -- NULL means pointer equality.
    NULL, // hash  -- NULL means pointer hash.
    NULL, // copyFormattingDesc
    NULL, // copyDebugDesc
    NULL, // reclaim
    NULL // refcount
};

static dispatch_once_t gAirPlayReceiverSessionInitOnce = 0;
static CFTypeID gAirPlayReceiverSessionTypeID = _kCFRuntimeNotATypeID;
static int32_t gAirTunesRelativeTimeOffset = 0; // Custom adjustment to the real offset for fine tuning.

AirPlayAudioStats gAirPlayAudioStats;

#if (DEBUG)
FILE* gAirTunesFile = NULL;

// Control Variables

int gAirTunesDropMinRate = 0;
int gAirTunesDropMaxRate = 0;
int gAirTunesDropRemaining = 0;
int gAirTunesSkipMinRate = 0;
int gAirTunesSkipMaxRate = 0;
int gAirTunesSkipRemaining = 0;
int gAirTunesLateDrop = 0;
int gAirTunesDebugNoSkewSlew = 0;
int gAirTunesDebugLogAllSkew = 0;
int gAirTunesDebugNoRetransmits = 0;
int gAirTunesDebugPrintPerf = 0;
int gAirTunesDebugPerfMode = 0;
int gAirTunesDebugRetransmitTiming = 1;

// Stats

unsigned int gAirTunesDebugBusyNodeCount = 0;
unsigned int gAirTunesDebugBusyNodeCountLast = 0;
unsigned int gAirTunesDebugBusyNodeCountMax = 0;

uint64_t gAirTunesDebugSentByteCount = 0;
uint64_t gAirTunesDebugRecvByteCount = 0;
uint64_t gAirTunesDebugRecvRTPOriginalByteCount = 0;
uint64_t gAirTunesDebugRecvRTPOriginalByteCountLast = 0;
uint64_t gAirTunesDebugRecvRTPOriginalBytesPerSecAvg = 0;
uint64_t gAirTunesDebugRecvRTPRetransmitByteCount = 0;
uint64_t gAirTunesDebugRecvRTPRetransmitByteCountLast = 0;
uint64_t gAirTunesDebugRecvRTPRetransmitBytesPerSecAvg = 0;
unsigned int gAirTunesDebugIdleTimeoutCount = 0;
unsigned int gAirTunesDebugStolenNodeCount = 0;
unsigned int gAirTunesDebugOldDiscardCount = 0;
unsigned int gAirTunesDebugConcealedGapCount = 0;
unsigned int gAirTunesDebugConcealedEndCount = 0;
unsigned int gAirTunesDebugLateDropCount = 0;
unsigned int gAirTunesDebugSameTimestampCount = 0;
unsigned int gAirTunesDebugLossCounts[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
unsigned int gAirTunesDebugTotalLossCount = 0;
unsigned int gAirTunesDebugMaxBurstLoss = 0;
unsigned int gAirTunesDebugDupCount = 0;
unsigned int gAirTunesDebugMisorderedCount = 0;
unsigned int gAirTunesDebugUnrecoveredPacketCount = 0;
unsigned int gAirTunesDebugUnexpectedRTPOffsetResetCount = 0;
unsigned int gAirTunesDebugHugeSkewResetCount = 0;
unsigned int gAirTunesDebugGlitchCount = 0;
unsigned int gAirTunesDebugTimeSyncHugeRTTCount = 0;
uint64_t gAirTunesDebugTimeAnnounceMinNanos = UINT64_C(0xFFFFFFFFFFFFFFFF);
uint64_t gAirTunesDebugTimeAnnounceMaxNanos = 0;
unsigned int gAirTunesDebugRetransmitActiveCount = 0;
unsigned int gAirTunesDebugRetransmitActiveMax = 0;
unsigned int gAirTunesDebugRetransmitSendCount = 0;
unsigned int gAirTunesDebugRetransmitSendLastCount = 0;
unsigned int gAirTunesDebugRetransmitSendPerSecAvg = 0;
unsigned int gAirTunesDebugRetransmitRecvCount = 0;
unsigned int gAirTunesDebugRetransmitBigLossCount = 0;
unsigned int gAirTunesDebugRetransmitAbortCount = 0;
unsigned int gAirTunesDebugRetransmitFutileAbortCount = 0;
unsigned int gAirTunesDebugRetransmitNoFreeNodesCount = 0;
unsigned int gAirTunesDebugRetransmitNotFoundCount = 0;
unsigned int gAirTunesDebugRetransmitPrematureCount = 0;
unsigned int gAirTunesDebugRetransmitMaxTries = 0;
uint64_t gAirTunesDebugRetransmitMinNanos = UINT64_C(0xFFFFFFFFFFFFFFFF);
uint64_t gAirTunesDebugRetransmitMaxNanos = 0;
uint64_t gAirTunesDebugRetransmitAvgNanos = 0;
uint64_t gAirTunesDebugRetransmitRetryMinNanos = UINT64_C(0xFFFFFFFFFFFFFFFF);
uint64_t gAirTunesDebugRetransmitRetryMaxNanos = 0;

double gAirTunesClockOffsetHistory[512];
unsigned int gAirTunesClockOffsetIndex = 0;
unsigned int gAirTunesClockOffsetCount = 0;

double gAirTunesClockRTTHistory[512];
unsigned int gAirTunesClockRTTIndex = 0;
unsigned int gAirTunesClockRTTCount = 0;

uint32_t gAirTunesRTPOffsetHistory[1024];
unsigned int gAirTunesRTPOffsetIndex = 0;
unsigned int gAirTunesRTPOffsetCount = 0;

// Transients

uint64_t gAirTunesDebugLastPollTicks = 0;
uint64_t gAirTunesDebugPollIntervalTicks = 0;
uint64_t gAirTunesDebugSentByteCountLast = 0;
uint64_t gAirTunesDebugRecvByteCountLast = 0;
uint32_t gAirTunesDebugHighestSeqLast = 0;
uint32_t gAirTunesDebugTotalLossCountLast = 0;
uint32_t gAirTunesDebugRecvCountLast = 0;
uint32_t gAirTunesDebugGlitchCountLast = 0;
#endif

#if 0
#pragma mark == General ==
#endif

//===========================================================================================================================
//	AirPlayReceiverSessionGetTypeID
//===========================================================================================================================

CFTypeID AirPlayReceiverSessionGetTypeID(void)
{
    dispatch_once_f(&gAirPlayReceiverSessionInitOnce, NULL, _GetTypeID);
    return (gAirPlayReceiverSessionTypeID);
}

//===========================================================================================================================
//	AirPlayReceiverSessionCreate
//===========================================================================================================================

OSStatus
AirPlayReceiverSessionCreate(
    AirPlayReceiverSessionRef* outSession,
    const AirPlayReceiverSessionCreateParams* inParams)
{
    AirPlayReceiverServerRef const server = inParams->server;
    OSStatus err;
    AirPlayReceiverSessionRef me;
    size_t extraLen;
    uint64_t ticksPerSec;
    uint64_t ticks;
    AirTunesSource* ats;

    ticksPerSec = UpTicksPerSecond();
    ticks = UpTicks();

    extraLen = sizeof(*me) - sizeof(me->base);
    me = (AirPlayReceiverSessionRef)_CFRuntimeCreateInstance(NULL, AirPlayReceiverSessionGetTypeID(), (CFIndex)extraLen, NULL);
    require_action(me, exit, err = kNoMemoryErr);
    memset(((uint8_t*)me) + sizeof(me->base), 0, extraLen);

    me->queue = AirPlayReceiverServerGetDispatchQueue(server);
    dispatch_retain(me->queue);

    CFRetain(server);
    me->server = server;

    me->connection = inParams->connection;

    // Initialize variables to a good state so we can safely clean up if something fails during init.

    me->transportType = inParams->transportType;
    me->peerAddr = *inParams->peerAddr;
    UUIDGet(me->sessionUUID);
    me->startStatus = kNotInitializedErr;
    me->clientDeviceID = inParams->clientDeviceID;
    me->clientSessionID = inParams->clientSessionID;
    me->clientVersion = inParams->clientVersion;
    me->isRemoteControlSession = inParams->isRemoteControlSession;

    memcpy(me->clientIfMACAddr, inParams->clientIfMACAddr, sizeof(inParams->clientIfMACAddr));

    me->sessionTicks = ticks;
    me->sessionIdle = true;
    me->sessionIdleValid = false;
    me->useEvents = inParams->useEvents;
    me->eventSock = kInvalidSocketRef;
    me->mainAudioCtx.cmdSock = kInvalidSocketRef;
    me->mainAudioCtx.dataSock = kInvalidSocketRef;
    me->altAudioCtx.cmdSock = kInvalidSocketRef;
    me->altAudioCtx.dataSock = kInvalidSocketRef;
    me->rtcpSock = kInvalidSocketRef;
    me->timingSock = kInvalidSocketRef;
    me->timingCmdSock = kInvalidSocketRef;

    me->eventQueue = dispatch_queue_create("AirPlayReceiverSessionEventQueue", NULL);
    require_action(me->eventQueue, exit, err = kNoMemoryErr);

    me->eventReplyTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, me->eventQueue);
    require_action(me->eventReplyTimer, exit, err = kNoMemoryErr);

    dispatch_set_context(me->eventReplyTimer, me);
    dispatch_source_set_event_handler_f(me->eventReplyTimer, _EventReplyTimeoutCallback);
    dispatch_source_set_timer(me->eventReplyTimer, DISPATCH_TIME_FOREVER, DISPATCH_TIME_FOREVER, kNanosecondsPerSecond);
    dispatch_resume(me->eventReplyTimer);

    // Finish initialization.

    err = pthread_mutex_init(&me->mutex, NULL);
    require_noerr(err, exit);
    me->mutexPtr = &me->mutex;

    me->flushLastTicks = ticks;

    me->glitchIntervalTicks = 1 * kSecondsPerMinute * ticksPerSec;
    me->glitchNextTicks = ticks + me->glitchIntervalTicks;
    me->remoteControlSessionIDGenerator = 0; // start from '1'. '0' is reserved. increment first and then use.

    ats = &me->source;
    ats->lastActivityTicks = ticks;
    ats->maxIdleTicks = server->timeoutDataSecs * ticksPerSec;
    ats->perSecTicks = 1 * ticksPerSec;
    ats->perSecLastTicks = 0; // Explicit 0 to note that it's intentional to force an immmediate update.
    ats->lastIdleLogTicks = ticks;
    ats->idleLogIntervalTicks = 10 * ticksPerSec;

    ats->rtcpTIClockRTTMin = +1000000.0;
    ats->rtcpTIClockRTTMax = -1000000.0;
    ats->rtcpTIClockOffsetMin = +1000000.0;
    ats->rtcpTIClockOffsetMax = -1000000.0;

    if (!me->isRemoteControlSession) {
        if (server->delegate.sessionCreated_f)
            server->delegate.sessionCreated_f(server, me, server->delegate.context);

        err = AirPlayReceiverSessionPlatformInitialize(me);
        require_noerr(err, exit);

        notifyPlatformOfSender(me);
    } else {
        // To save power set TCP keep alive to something large for remote control connections.
        // Maybe we can disable it completely in the future.
        int tcpKeepAliveIdleSecs = 600;
        int tcpKeepAliveIntervalSecs = 4;
        int tcpKeepAliveMaxUnansweredProbes = 3;

        SocketSetKeepAliveEx(me->connection->httpCnx->sock, tcpKeepAliveIdleSecs, tcpKeepAliveIntervalSecs, tcpKeepAliveMaxUnansweredProbes);
    }

    if (me->delegate.initialize_f) {
        err = me->delegate.initialize_f(me, me->delegate.context);
        require_noerr(err, exit);
    }
    me->remoteControlSessionIDGenerator = 0; // start from '1'. '0' is reserved. increment first and then use.

#if (AIRPLAY_GENERAL_AUDIO_SKEW)
    if (!me->isRemoteControlSession) {
        PIDInit(&me->source.rtpSkewPIDController, 0.7, 0.2, 1.2, 0.97, kAirTunesPIDMin, kAirTunesPIDMax);

        me->source.rtpSkewPlatformAdjust = AirPlayReceiverSessionPlatformGetBoolean(me, CFSTR(kAirPlayKey__RTPSkewPlatformAdjust), NULL, &err);
        if (err) {
            me->source.rtpSkewPlatformAdjust = 0;
            err = kNoErr;
        }

#if (AIRPLAY_RAMSTAD_SRC)
        CRamstadASRC_init(&ats->resampler); // initialize resampler
#endif // AIRPLAY_RAMSTAD_SRC
    }
#endif // AIRPLAY_GENERAL_AUDIO_SKEW

    *outSession = me;
    me = NULL;
    err = kNoErr;

exit:
    CFReleaseNullSafe(me);
    return (err);
}

//===========================================================================================================================
//	AirPlayReceiverSessionGetDispatchQueue
//===========================================================================================================================

dispatch_queue_t AirPlayReceiverSessionGetDispatchQueue(AirPlayReceiverSessionRef inSession)
{
    return (inSession->queue);
}

//===========================================================================================================================
//	AirPlayReceiverSessionSetDelegate
//===========================================================================================================================

EXPORT_GLOBAL
void AirPlayReceiverSessionSetDelegate(AirPlayReceiverSessionRef inSession, const AirPlayReceiverSessionDelegate* inDelegate)
{
    inSession->delegate = *inDelegate;
}

//===========================================================================================================================
//	AirPlayReceiverSessionControl
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
AirPlayReceiverSessionControl(
    CFTypeRef inSession,
    uint32_t inFlags,
    CFStringRef inCommand,
    CFTypeRef inQualifier,
    CFDictionaryRef inParams,
    CFDictionaryRef* outParams)
{
    AirPlayReceiverSessionRef const session = (AirPlayReceiverSessionRef)inSession;
    OSStatus err;

    if (0) {
    }

    // ModesChanged

    else if (session->delegate.modesChanged_f && CFEqual(inCommand, CFSTR(kAirPlayCommand_ModesChanged))) {
        AirPlayModeState modeState;

        require_action(inParams, exit, err = kParamErr);
        err = AirPlayReceiverSessionMakeModeStateFromDictionary(session, inParams, &modeState);
        require_noerr(err, exit);

        atr_ulog(kLogLevelNotice, "Modes changed: screen %s, mainAudio %s, speech %s (%s), phone %s, turns %s\n",
            AirPlayEntityToString(modeState.screen), AirPlayEntityToString(modeState.mainAudio),
            AirPlayEntityToString(modeState.speech.entity), AirPlaySpeechModeToString(modeState.speech.mode),
            AirPlayEntityToString(modeState.phoneCall), AirPlayEntityToString(modeState.turnByTurn));
        session->delegate.modesChanged_f(session, &modeState, session->delegate.context);
    }

    // RequestUI

    else if (session->delegate.requestUI_f && CFEqual(inCommand, CFSTR(kAirPlayCommand_RequestUI))) {
        CFStringRef url;

        if (inParams)
            url = (CFStringRef)CFDictionaryGetValue(inParams, CFSTR(kAirPlayKey_URL));
        else
            url = NULL;

        atr_ulog(kLogLevelNotice, "Request accessory UI\n");
        session->delegate.requestUI_f(session, url, session->delegate.context);
    }

    // UpdateFeedback

    else if (CFEqual(inCommand, CFSTR(kAirPlayCommand_UpdateFeedback))) {
        err = _UpdateFeedback(session, inParams, outParams);
        require_noerr(err, exit);
    }

    else if (CFEqual(inCommand, CFSTR(kAirPlayKey_DidReceiveData))) {
        uint64_t streamID = 0;
        CFDictionaryRef dataParams = NULL;
        CFDictionaryRef entryDict = NULL;
        CFNumberRef rcsObjRef = NULL;
        CFStringRef clientTypeUUID = NULL;

        require_action_quiet(session->remoteControlSessions, bail, err = kParamErr);
        streamID = CFDictionaryGetUInt64(inParams, CFSTR(kMediaControlHTTPHeader_StreamID), &err);
        rcsObjRef = CFNumberCreateUInt64(streamID);

        dataParams = (CFDictionaryRef)CFDictionaryGetValue(inParams, CFSTR(kAirPlayKey_Params));

        // retrieve token from remoteControlSessions dictionary using streamID as the key
        entryDict = (CFDictionaryRef)CFDictionaryGetValue(session->remoteControlSessions, rcsObjRef);
        require_action_quiet(entryDict, bail, err = kStateErr);

        clientTypeUUID = (CFStringRef)CFDictionaryGetValue(entryDict, CFSTR(kAirPlayKey_ClientTypeUUID));
        require_action(clientTypeUUID, exit, err = kStateErr);

        if (CFEqual(clientTypeUUID, kAirPlayClientTypeUUID_MediaRemote)) {
            err = AirPlaySessionManagerRouteMessage(session->server->sessionMgr, session, rcsObjRef, entryDict, dataParams);
        } else {
            err = kParamErr;
        }

    bail:
        CFRelease(rcsObjRef);
        require_noerr(err, exit);
        goto exit;
    }

    // SetHIDInputMode

    else if (session->delegate.control_f && CFEqual(inCommand, CFSTR(kAirPlayCommand_HIDSetInputMode))) {
        atr_ulog(kLogLevelNotice, "Set HIDInputMode\n");
        session->delegate.control_f(session, inCommand, inQualifier, inParams, outParams, session->delegate.context);
    }

    // iAPSendMessage

    else if (session->delegate.control_f && CFEqual(inCommand, CFSTR(kAirPlayCommand_iAPSendMessage))) {
        atr_ulog(kLogLevelTrace, "iAP Send Message\n");
        session->delegate.control_f(session, inCommand, inQualifier, inParams, outParams, session->delegate.context);
    }

    // Other

    else {
        err = AirPlayReceiverSessionPlatformControl(session, inFlags, inCommand, inQualifier, inParams, outParams);
        goto exit;
    }
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	AirPlayReceiverSessionCopyProperty
//===========================================================================================================================

EXPORT_GLOBAL
CFTypeRef
AirPlayReceiverSessionCopyProperty(
    CFTypeRef inSession,
    uint32_t inFlags,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    OSStatus* outErr)
{
    AirPlayReceiverSessionRef const session = (AirPlayReceiverSessionRef)inSession;
    OSStatus err;
    CFTypeRef value = NULL;

    (void)inFlags;
    (void)inQualifier;

    if (0) {
    }

    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_TransportType))) {
        value = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, (int32_t*)&session->transportType);
        err = value ? kNoErr : kNoMemoryErr;
        goto exit;
    }

#if (AIRPLAY_METRICS)
    // Metrics
    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_Metrics))) {
        value = _CopyMetrics(session, &err);
        goto exit;
    }
#endif

    // Unknown

    else if (session) {
        value = AirPlayReceiverSessionPlatformCopyProperty(session, inFlags, inProperty, inQualifier, &err);
        goto exit;
    }
    err = kNoErr;

exit:
    if (outErr)
        *outErr = err;
    return (value);
}

//===========================================================================================================================
//	AirPlayReceiverSessionSetProperty
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
AirPlayReceiverSessionSetProperty(
    CFTypeRef inSession,
    uint32_t inFlags,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    CFTypeRef inValue)
{
    AirPlayReceiverSessionRef const session = (AirPlayReceiverSessionRef)inSession;
    OSStatus err = kNoErr;

    if (0) {
    }

    // TimelineOffset

    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_TimelineOffset))) {
        int32_t offset;

        offset = (int32_t)CFGetInt64(inValue, NULL);
        require_action((offset >= -250) && (offset <= 250), exit, err = kRangeErr);
        gAirTunesRelativeTimeOffset = offset;
    }

    // Other

    else {
        err = AirPlayReceiverSessionPlatformSetProperty(session, inFlags, inProperty, inQualifier, inValue);
        goto exit;
    }
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	AirPlayReceiverSessionSetHomeKitSecurityContext
//===========================================================================================================================

OSStatus
AirPlayReceiverSessionSetHomeKitSecurityContext(
    AirPlayReceiverSessionRef inSession,
    PairingSessionRef inHKPairVerifySession)
{
    ReplaceCF(&inSession->pairVerifySession, inHKPairVerifySession);
    return (kNoErr);
}

//===========================================================================================================================
//	AirPlayReceiverSessionSetSecurityInfo
//===========================================================================================================================

OSStatus
AirPlayReceiverSessionSetSecurityInfo(
    AirPlayReceiverSessionRef inSession,
    const uint8_t inKey[16],
    const uint8_t inIV[16])
{
    OSStatus err;

    AES_CBCFrame_Final(&inSession->decryptorStorage);
    inSession->decryptor = NULL;

    err = AES_CBCFrame_Init(&inSession->decryptorStorage, inKey, inIV, false);
    require_noerr(err, exit);
    inSession->decryptor = &inSession->decryptorStorage;

    memcpy(inSession->aesSessionKey, inKey, sizeof(inSession->aesSessionKey));
    memcpy(inSession->aesSessionIV, inIV, sizeof(inSession->aesSessionIV));

exit:
    return (err);
}

//===========================================================================================================================
//	_EventReplyTimeoutCallback
//===========================================================================================================================

static void _EventReplyTimeoutCallback(void* inContext)
{
    AirPlayReceiverSessionRef const me = (AirPlayReceiverSessionRef)inContext;
    sockaddr_ip sip;
    size_t len;
    SocketRef sock = kInvalidSocketRef;
    const int connectTimeoutSec = 20;
    OSStatus err;

    require_action_quiet(me->eventPendingMessageCount > 0, exit, err = kNoErr);

    // connect to the sender event socket
    err = HTTPClientGetPeerAddress(me->eventClient, &sip, sizeof(sip), &len);
    require_noerr(err, exit);

    sock = socket(sip.sa.sa_family, SOCK_STREAM, IPPROTO_TCP);
    require_action(IsValidSocket(sock), exit, err = kUnknownErr);

    atr_ulog(kLogLevelWarning, "### Waking up device\n");
    err = SocketConnect(sock, &sip, connectTimeoutSec);
    require((err == ECONNREFUSED), exit);
    err = kNoErr;

exit:
    check_noerr(err);
    ForgetSocket(&sock);
}

//===========================================================================================================================
//	AirPlayReceiverSessionSendCommandForObject
//===========================================================================================================================

static void _AirPlayReceiverSessionSendCommandCompletion(HTTPMessageRef inMsg);

OSStatus AirPlayReceiverSessionSendCommandForObject(
    AirPlayReceiverSessionRef inSession,
    CFNumberRef inObjectRef,
    CFDictionaryRef inRequest,
    AirPlayReceiverSessionCommandCompletionFunc inCompletion,
    void* inContext)
{
    OSStatus err;
    HTTPMessageRef msg = NULL;
    CFDataRef data = NULL;

    require_action(inSession, exit, err = kParamErr);
    require_action_quiet(inSession->sessionStarted, exit, err = kStateErr);
    require_action_quiet(inSession->eventClient, exit, err = kUnsupportedErr);

    err = HTTPMessageCreate(&msg);
    require_noerr(err, exit);

    HTTPClientSetTimeout(inSession->eventClient, inSession->timeoutDataSecs);

    err = HTTPHeader_InitRequest(&msg->header, "POST", kAirPlayCommandPath, kAirTunesHTTPVersionStr);
    require_noerr(err, exit);

    if (inObjectRef) {
        uint64_t objectID = 0;
        CFNumberGetValue(inObjectRef, kCFNumberSInt64Type, &objectID);
        HTTPHeader_SetField(&msg->header, kMediaControlHTTPHeader_StreamID, "%llu", objectID);
    }

    data = CFPropertyListCreateData(NULL, inRequest, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
    require_action(data, exit, err = kUnknownErr);
    err = HTTPMessageSetBody(msg, kMIMEType_AppleBinaryPlist, CFDataGetBytePtr(data), (size_t)CFDataGetLength(data));
    require_noerr(err, exit);

    if (inCompletion) {
        CFRetain(inSession);
        msg->userContext1 = inSession;
        msg->userContext2 = (void*)(uintptr_t)inCompletion;
        msg->userContext3 = inContext;
        msg->completion = _AirPlayReceiverSessionSendCommandCompletion;
    }

    err = HTTPClientSendMessage(inSession->eventClient, msg);
    CFRelease(data);
    if (err && inCompletion)
        CFRelease(inSession);
    require_noerr(err, exit);

exit:
    CFReleaseNullSafe(msg);
    return (err);
}

//===========================================================================================================================
//	AirPlayReceiverSessionSendCommand
//===========================================================================================================================
EXPORT_GLOBAL OSStatus AirPlayReceiverSessionSendCommand(
    AirPlayReceiverSessionRef inSession,
    CFDictionaryRef inRequest,
    AirPlayReceiverSessionCommandCompletionFunc inCompletion,
    void* inContext)
{
    return (AirPlayReceiverSessionSendCommandForObject(inSession, NULL, inRequest, inCompletion, inContext));
}

//===========================================================================================================================
//	_AirPlayReceiverSessionSendCommandCompletion
//===========================================================================================================================

static void _AirPlayReceiverSessionSendCommandCompletion(HTTPMessageRef inMsg)
{
    AirPlayReceiverSessionRef const session = (AirPlayReceiverSessionRef)inMsg->userContext1;
    AirPlayReceiverSessionCommandCompletionFunc const completion = (AirPlayReceiverSessionCommandCompletionFunc)(uintptr_t)inMsg->userContext2;
    OSStatus err;
    CFDictionaryRef response;

    response = CFDictionaryCreateWithBytes(inMsg->bodyPtr, inMsg->bodyLen, &err);
    require_noerr(err, exit);

    err = (OSStatus)CFDictionaryGetInt64(response, CFSTR(kAirPlayKey_Status), NULL);
    require_noerr_quiet(err, exit);

exit:
    if (completion)
        completion(err, err ? NULL : response, inMsg->userContext3);
    CFRelease(session);
    CFReleaseNullSafe(response);
}

//===========================================================================================================================
//	AirPlayReceiverSessionSetup
//===========================================================================================================================

OSStatus
AirPlayReceiverSessionSetup(
    AirPlayReceiverSessionRef me,
    CFDictionaryRef inRequestParams,
    CFDictionaryRef* outResponseParams)
{
    Boolean general = false;
    OSStatus err;
    CFMutableDictionaryRef responseParams;
    CFArrayRef requestStreams;
    CFDictionaryRef requestStreamDesc;
    CFIndex streamIndex, streamCount;
    char clientOSBuildVersion[32], minClientOSBuildVersion[32];
    AirPlayStreamType type;

    atr_ulog(kLogLevelTrace, "Setting up session %llu with %##a %?@\n",
        me->clientSessionID, &me->peerAddr, log_category_enabled(atr_ucat(), kLogLevelVerbose), inRequestParams);

    responseParams = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(responseParams, exit, err = kNoMemoryErr);

    if (!me->controlSetup) {
        err = _ControlSetup(me, inRequestParams, responseParams);
        require_noerr(err, exit);
    }

    // Perform minimum version check

    CFDictionaryGetCString(inRequestParams, CFSTR(kAirPlayKey_OSBuildVersion), clientOSBuildVersion, sizeof(clientOSBuildVersion), &err);
    if (!err) {
        strlcpy(me->clientOSBuildVersion, clientOSBuildVersion, sizeof(me->clientOSBuildVersion));
        err = AirPlayGetMinimumClientOSBuildVersion(me->server, minClientOSBuildVersion, sizeof(minClientOSBuildVersion));
        if (!err) {
            if (_CompareOSBuildVersionStrings(minClientOSBuildVersion, clientOSBuildVersion) > 0)
                require_noerr(err = kVersionErr, exit);
        }
    }

    // Set up each stream.

    requestStreams = CFDictionaryGetCFArray(inRequestParams, CFSTR(kAirPlayKey_Streams), &err);
    streamCount = requestStreams ? CFArrayGetCount(requestStreams) : 0;
    for (streamIndex = 0; streamIndex < streamCount; ++streamIndex) {
        requestStreamDesc = CFArrayGetCFDictionaryAtIndex(requestStreams, streamIndex, &err);
        require_noerr(err, exit);

        type = (AirPlayStreamType)CFDictionaryGetInt64(requestStreamDesc, CFSTR(kAirPlayKey_Type), NULL);
        switch (type) {

        case kAirPlayStreamType_GeneralAudio:
        case kAirPlayStreamType_BufferedAudio:
            err = _GeneralAudioSetup(me, type, requestStreamDesc, responseParams);
            require_noerr(err, exit);
            general = true;
            break;

        case kAirPlayStreamType_RemoteControl:
            err = _RemoteControlSessionSetup(me, requestStreamDesc, responseParams);
            require_noerr(err, exit);
            break;

        default:
            atr_ulog(kLogLevelNotice, "### Unsupported stream type: %d\n", type);
            break;
        }
    }

    if (streamCount > 0 || !me->sessionIdleValid) {
        err = _ControlIdleStateTransition(me, responseParams);
        require_noerr(err, exit);
    }

    // Set up the platform.

    err = AirPlayReceiverSessionPlatformControl(me, kCFObjectFlagDirect, CFSTR(kAirPlayCommand_SetUpStreams), NULL,
        inRequestParams, NULL);
    require_noerr(err, exit);

    if (general) {
        _GeneralAudioUpdateLatency(me);
    }

    *outResponseParams = responseParams;
    responseParams = NULL;
    err = kNoErr;

exit:
    CFReleaseNullSafe(responseParams);
    if (err) {
        atr_ulog(kLogLevelNotice, "### Set up session %llu with %##a failed: %#m %@\n",
            me->clientSessionID, &me->peerAddr, err, inRequestParams);

        if (me->server->delegate.sessionFailed_f)
            me->server->delegate.sessionFailed_f(me->server, err, me->server->delegate.context);

        AirPlayReceiverSessionTearDown(me, inRequestParams, err, NULL);
    }
    return (err);
}

//===========================================================================================================================
//	_ReplyTimerTearDown
//===========================================================================================================================

static void _ForgetReplyTimer(void* inCtx)
{
    AirPlayReceiverSessionRef session = (AirPlayReceiverSessionRef)inCtx;
    dispatch_source_forget(&session->eventReplyTimer);
}

static void _ReplyTimerTearDown(AirPlayReceiverSessionRef inSession)
{
    dispatch_sync_f(inSession->eventQueue, inSession, _ForgetReplyTimer);
}

//===========================================================================================================================
//	AirPlayReceiverSessionTearDown
//===========================================================================================================================

void AirPlayReceiverSessionTearDown(
    AirPlayReceiverSessionRef inSession,
    CFDictionaryRef inParams,
    OSStatus inReason,
    Boolean* outDone)
{
    OSStatus err;
    CFArrayRef streams;
    CFIndex streamIndex, streamCount;
    CFDictionaryRef streamDesc;
    AirPlayStreamType streamType;

    atr_ulog(kLogLevelTrace, "Tearing down session %llu with %##a %?@\n",
        inSession->clientSessionID, &inSession->peerAddr, log_category_enabled(atr_ucat(), kLogLevelVerbose), inParams);

    AirPlayReceiverSessionPlatformControl(inSession, kCFObjectFlagDirect, CFSTR(kAirPlayCommand_TearDownStreams), NULL,
        inParams, NULL);

    streams = inParams ? CFDictionaryGetCFArray(inParams, CFSTR(kAirPlayKey_Streams), NULL) : NULL;
    streamCount = streams ? CFArrayGetCount(streams) : 0;
    for (streamIndex = 0; streamIndex < streamCount; ++streamIndex) {
        streamDesc = CFArrayGetCFDictionaryAtIndex(streams, streamIndex, &err);
        require_noerr(err, exit);

        streamType = (AirPlayStreamType)CFDictionaryGetInt64(streamDesc, CFSTR(kAirPlayKey_Type), NULL);

        atr_ulog(kLogLevelTrace, "Tearing down stream type %d\n", streamType);

        switch (streamType) {
        case kAirPlayStreamType_GeneralAudio:
        case kAirPlayStreamType_BufferedAudio:
            _TearDownStream(inSession, &inSession->mainAudioCtx, false);
            break;

        case kAirPlayStreamType_RemoteControl: {
            uint64_t streamID = CFDictionaryGetInt64(streamDesc, CFSTR(kAirPlayKey_StreamID), NULL);
            _RemoteControlSessionTearDown(inSession, streamID);
        } break;
        default:
            atr_ulog(kLogLevelNotice, "### Unsupported stream type: %d\n", streamType);
            break;
        }
    }

    if (streamCount > 0) {
        err = _ControlIdleStateTransition(inSession, NULL);
        require_noerr(err, exit);

        goto exit;
    }

    _LogEnded(inSession, inReason);

    dispatch_source_forget(&inSession->periodicTimer);
    _TearDownStream(inSession, &inSession->altAudioCtx, false);
    _TearDownStream(inSession, &inSession->mainAudioCtx, false);
    _ReplyTimerTearDown(inSession);
    _ControlTearDown(inSession);
    _TimingFinalize(inSession);

    if (!inSession->isRemoteControlSession) {
        CFArrayRef remoteSessions = NULL;
        if (AirPlaySessionManagerRemoteSessions(inSession->server->sessionMgr, &remoteSessions) == kNoErr) {
            CFIndex index;
            for (index = 0; index < CFArrayGetCount(remoteSessions); index++) {
                AirPlayReceiverSessionRef session = (AirPlayReceiverSessionRef)CFArrayGetValueAtIndex(remoteSessions, index);
                AirPlayReceiverSessionTearDown(session, NULL, kNoErr, NULL);
            }
        }
        CFReleaseNullSafe(remoteSessions);
    }

    _UnregisterRoutes(inSession);
    CFForget(&inSession->remoteControlSessions);

    AirTunesClock_Finalize(inSession->airTunesClock);
    inSession->airTunesClock = NULL;
exit:
    if (outDone)
        *outDone = (streamCount == 0);
}

static void _RouteRemoverApplierFunction(const void* key, const void* value, void* context)
{
    (void)key;

    AirPlaySessionManagerRef sessionManager = (AirPlaySessionManagerRef)context;
    if (CFDictionaryGetTypeID() == CFGetTypeID((CFTypeRef)value)) {
        CFNumberRef token = (CFNumberRef)CFDictionaryGetValue((CFDictionaryRef)value, CFSTR(kAirPlayKey_TokenRef));
        if (token != NULL && CFNumberGetTypeID() == CFGetTypeID(token)) {
            AirPlaySessionManagerUnregisterRoute(sessionManager, token);
        }
    }

    return;
}

static void _UnregisterRoutes(AirPlayReceiverSessionRef inSession)
{
    if (inSession && inSession->remoteControlSessions) {
        CFDictionaryApplyFunction(inSession->remoteControlSessions, _RouteRemoverApplierFunction, inSession->server->sessionMgr);
    }
}

//===========================================================================================================================
//	AirPlayReceiverSessionStart
//===========================================================================================================================

OSStatus
AirPlayReceiverSessionStart(
    AirPlayReceiverSessionRef inSession,
    AirPlayReceiverSessionStartInfo* inInfo)
{
    OSStatus err = kNoErr;
    AirPlayAudioStreamContext* ctx;
    dispatch_source_t source;
    uint64_t nanos;

    inSession->playTicks = UpTicks();

    if (IsValidSocket(inSession->eventSock)) {
        err = _ControlStart(inSession);
        require_noerr(err, exit);
    }

    if (!inSession->isRemoteControlSession) {
        ctx = &inSession->mainAudioCtx;
        if ((ctx->type == kAirPlayStreamType_GeneralAudio) && !ctx->threadPtr) {
            err = pthread_create(&ctx->thread, NULL, _GeneralAudioThread, inSession);
            require_noerr(err, exit);
            ctx->threadPtr = &ctx->thread;
        }

        err = AirPlayReceiverSessionPlatformControl(inSession, kCFObjectFlagDirect, CFSTR(kAirPlayCommand_StartSession),
            NULL, NULL, NULL);
        require_noerr_quiet(err, exit);

        // Start a timer to service things periodically for the master session.

        inSession->source.lastIdleLogTicks = inSession->playTicks;
        inSession->periodicTimer = source = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, inSession->queue);
        require_action(source, exit, err = kUnknownErr);
        dispatch_set_context(source, inSession);
        dispatch_source_set_event_handler_f(source, _PerformPeriodTasks);
        nanos = 500 * kNanosecondsPerMillisecond;
        dispatch_source_set_timer(source, dispatch_time(DISPATCH_TIME_NOW, nanos), nanos, nanos);
        dispatch_resume(source);
    }
    inSession->sessionStarted = true;

    if (inSession->delegate.started_f) {
        inSession->delegate.started_f(inSession, inSession->delegate.context);
    }

exit:
    _LogStarted(inSession, inInfo, err);
    return (err);
}

//===========================================================================================================================
//	AirPlayReceiverFlushPackets
//  Requires sessionLock before being called.
//===========================================================================================================================
static void AirPlayReceiverFlushPackets(AirPlayReceiverSessionRef inSession, AirPlayFlushPoint const* flushFrom, AirPlayFlushPoint flushUntil)
{
    AirTunesBufferNode* curr;
    AirTunesBufferNode* stop;
    AirTunesBufferNode* next;
    CFMutableDictionaryRef flushInfo = NULL;

    stop = inSession->busyListSentinel;

    for (curr = stop->next; curr != stop; curr = next) {
        next = curr->next;

        // If we are flushing a range then keep packets before the 'flush from' sequence
        if (flushFrom && Seq_LT(inSession, curr->seq, flushFrom->seq)) {
            //check( Mod32_LT( curr->rtp->header.ts, flushFrom->ts ) );
            continue;
        }

        // Drop packets in the queue that are earlier than the 'flush until' sequence
        if (Seq_LT(inSession, curr->seq, flushUntil.seq)) {
            allocator_freeNode(inSession, curr);
            continue;
        }
        break;
    }

    // Let the platform know we are doing a flush.
    // Convert the flush-from and flush-until timestamps to sample times.
    // The sample times are mod 2^32.

    uint32_t offsetActive = inSession->source.rtpOffsetActive;

    flushInfo = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require(flushInfo, exit);

    if (flushFrom) {
        uint32_t fromSampleTime32 = flushFrom->ts + offsetActive;
        CFNumberRef fromRef = CFNumberCreateInt64(fromSampleTime32);
        require(fromRef, exit);
        CFDictionaryAddValue(flushInfo, CFSTR(kAirPlayKey_FlushFromSampleTime32), fromRef);
        CFRelease(fromRef);
    }

    uint32_t untilSampleTime32 = flushUntil.ts + offsetActive;
    CFNumberRef untilRef = CFNumberCreateInt64(untilSampleTime32);
    require(untilRef, exit);
    CFDictionaryAddValue(flushInfo, CFSTR(kAirPlayKey_FlushUntilSampleTime32), untilRef);
    CFRelease(untilRef);

    // A general flush that most platforms can handle is kAirPlayCommand_FlushAudio.
    // Platforms that are doing deep buffering might find kAirPlayCommand_FlushFromAudio
    // useful.

    CFStringRef flushControl = flushFrom ? CFSTR(kAirPlayCommand_FlushFromAudio) : CFSTR(kAirPlayCommand_FlushAudio);

    (void)AirPlayReceiverSessionPlatformControl(inSession, kCFObjectFlagDirect, flushControl, NULL, flushInfo, NULL);

exit:
    CFReleaseNullSafe(flushInfo);
    return;
}

//===========================================================================================================================
//	AirPlayReceiverSessionFlushAudio
//===========================================================================================================================

OSStatus
AirPlayReceiverSessionFlushAudio(
    AirPlayReceiverSessionRef inSession,
    AirPlayFlushPoint const* flushFrom,
    AirPlayFlushPoint flushUntil,
    uint32_t* outLastTS)
{
    AirTunesSource* const ats = &inSession->source;
    AirPlayAudioStreamContext* const ctx = &inSession->mainAudioCtx;
    OSStatus err;

    DEBUG_USE_ONLY(err);

    require_action_quiet(inSession->busyListSentinel, exit, atr_ulog(kLogLevelNotice, "### Not playing audio - nothing to flush\n"));

    if (flushFrom) {
        atr_ulog(kLogLevelVerbose, "Flushing from ts %u seq %u\n", flushFrom->ts, flushFrom->seq);
    }
    atr_ulog(kLogLevelVerbose, "Flushing until ts %u seq %u\n", flushUntil.ts, flushUntil.seq);

    _SessionLock(inSession);

    if (!flushFrom) {

        inSession->lastPlayedValid = false;
        ats->receiveCount = 0; // Reset so we don't try to retransmit on the next post-flush packet.
        if (inSession->audioConverter) {
            err = AudioConverterReset(inSession->audioConverter);
            check_noerr(err);
        }
#if (AIRPLAY_GENERAL_AUDIO_SKEW)
        ats->timeAnnounceCount = 0; // Reset so we don't try to play samples again until we get a new timeline.
#endif
#if (AIRPLAY_BUFFERED)
        if (inSession->source.usingAnchorTime && inSession->receiveAudioOverTCP) {
            inSession->source.setRateTiming.anchor.rate = 0;
        }
#endif
        inSession->flushingWithinRange = false;

    } else {
        inSession->flushingWithinRange = true;
        inSession->flushFrom = *flushFrom;
    }

    inSession->flushing = true;
    inSession->flushTimeoutTS = flushUntil.ts + (3 * ctx->sampleRate); // 3 seconds.
    inSession->flushUntil = flushUntil;
    inSession->flushLastTicks = UpTicks();
    inSession->flushUntil = flushUntil;

    ats->rtcpRTDisable = inSession->redundantAudio;

    AirPlayReceiverFlushPackets(inSession, flushFrom, flushUntil);

    atr_ulog(kLogLevelNotice, "Flushing receiver complete: flushSeq %u flushTS %u count %u\n",
        inSession->flushUntil.seq, inSession->flushUntil.ts, inSession->source.receiveCount);

    _RetransmitsAbortAll(inSession, "flush");

    if (outLastTS)
        *outLastTS = inSession->lastPlayedTS;

    _SessionUnlock(inSession);
    _announcePlaying(inSession, false);
exit:
    return (kNoErr);
}

//===========================================================================================================================
//	AirPlayReceiverSessionReadAudio
//===========================================================================================================================

OSStatus
AirPlayReceiverSessionReadAudio2(
    AirPlayReceiverSessionRef inSession,
    AirPlayStreamType inType,
    uint32_t inSampleTime,
    uint64_t inHostTime,
    void* inBuffer,
    size_t inLen,
    size_t inMinLen,
    size_t* outSuppliedLen)
{
    size_t suppliedLen = 0;
    OSStatus err;

    switch (inType) {
    case kAirPlayStreamType_GeneralAudio:
    case kAirPlayStreamType_BufferedAudio:
        _SessionLock(inSession);

        if (inSession->source.usingAnchorTime) {
            drift_updateRTPOffsetAndProcessSkew(inSession, inHostTime, inSampleTime);
        }
#if (AIRPLAY_GENERAL_AUDIO_SKEW)
        else if (inSession->source.timeAnnouncePending) {
            AirTunesTime ntpTime;

            AirTunesClock_GetSynchronizedTimeNearUpTicks(inSession->airTunesClock, &ntpTime, inHostTime);
            _TimingProcessTimeAnnounce(inSession, &ntpTime, inSampleTime);
            inSession->source.timeAnnouncePending = false;
        }
#endif
        suppliedLen = _GeneralAudioRender(inSession, inSampleTime, inBuffer, inLen, inMinLen);
        _SessionUnlock(inSession);
        err = kNoErr;
        break;

    default:
        dlogassert("Bad stream type: %u", inType);
        err = kParamErr;
        goto exit;
    }

exit:
    if (outSuppliedLen) {
        *outSuppliedLen = suppliedLen;
    }

    return (err);
}

OSStatus
AirPlayReceiverSessionReadAudio(
    AirPlayReceiverSessionRef inSession,
    AirPlayStreamType inType,
    uint32_t inSampleTime,
    uint64_t inHostTime,
    void* inBuffer,
    size_t inLen)
{
    return AirPlayReceiverSessionReadAudio2(inSession, inType, inSampleTime, inHostTime, inBuffer, inLen, inLen, NULL);
}

//===========================================================================================================================
//	AirPlayReceiverSessionWriteAudio
//===========================================================================================================================

typedef struct SendAudioContext {
    TAILQ_ENTRY(SendAudioContext)
    list;
    AirPlayAudioStreamContext* ctx;
    AirPlayStreamType type;
    uint32_t sampleTime;
} SendAudioContext;

OSStatus
AirPlayReceiverSessionWriteAudio(
    AirPlayReceiverSessionRef inSession,
    AirPlayStreamType inType,
    uint32_t inSampleTime,
    uint64_t inHostTime,
    const void* inBuffer,
    size_t inLen)
{
    AirPlayAudioStreamContext* const ctx = &inSession->mainAudioCtx;
    size_t len;
    MirroredRingBuffer* const ring = &ctx->inputRing;
    OSStatus err = kNoErr;
    SendAudioContext* sendAudioContext;

    (void)inHostTime;

    len = MirroredRingBufferGetBytesFree(ring);
    require_action_quiet(len >= inLen, exit, err = kNoSpaceErr;
                         atr_ulog(kLogLevelInfo, "### Audio input buffer full: %zu > %zu\n", inLen, len));
    memcpy(MirroredRingBufferGetWritePtr(ring), inBuffer, inLen);
    MirroredRingBufferWriteAdvance(ring, inLen);

    sendAudioContext = malloc(sizeof(*sendAudioContext));
    sendAudioContext->ctx = ctx;
    sendAudioContext->type = inType;
    sendAudioContext->sampleTime = inSampleTime;

    pthread_mutex_lock(ctx->sendAudioMutexPtr);
    TAILQ_INSERT_TAIL(&ctx->sendAudioList, sendAudioContext, list);
    pthread_cond_signal(ctx->sendAudioCondPtr);
    pthread_mutex_unlock(ctx->sendAudioMutexPtr);
exit:
    return (err);
}

//===========================================================================================================================
//	AirPlayReceiverSessionSetUserVersion
//===========================================================================================================================

void AirPlayReceiverSessionSetUserVersion(AirPlayReceiverSessionRef inSession, uint32_t userVersion)
{
    (void)inSession;
    (void)userVersion;
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	_GetTypeID
//===========================================================================================================================

static void _GetTypeID(void* inContext)
{
    (void)inContext;

    gAirPlayReceiverSessionTypeID = _CFRuntimeRegisterClass(&kAirPlayReceiverSessionClass);
    check(gAirPlayReceiverSessionTypeID != _kCFRuntimeNotATypeID);
}

//===========================================================================================================================
//	_Finalize
//===========================================================================================================================

static void _Finalize(CFTypeRef inCF)
{
    AirPlayReceiverSessionRef const session = (AirPlayReceiverSessionRef)inCF;

    atr_ulog(kLogLevelTrace, "Finalize session %llu\n", session->clientSessionID);

    if (session->delegate.finalize_f)
        session->delegate.finalize_f(session, session->delegate.context);
    AirPlayReceiverSessionPlatformFinalize(session);

    dispatch_source_forget(&session->periodicTimer);
    _TearDownStream(session, &session->altAudioCtx, true);
    _TearDownStream(session, &session->mainAudioCtx, true);
    _ReplyTimerTearDown(session);
    _ControlTearDown(session);
    _TimingFinalize(session);

    ForgetCF(&session->remoteControlSessions);

    AirTunesClock_Finalize(session->airTunesClock);
    session->airTunesClock = NULL;

    AES_CBCFrame_Final(&session->decryptorStorage);
    pthread_mutex_forget(&session->mutexPtr);
    session->connection = NULL;
    ForgetCF(&session->server);
    dispatch_forget(&session->queue);
    dispatch_forget(&session->eventQueue);

#if (AIRPLAY_METRICS)
    ForgetCF(&session->metaData);
#endif
}

#if (AIRPLAY_META_DATA)
static void updateTrackProgress(AirPlayReceiverSessionRef inSession, uint32_t lastPlayedTS, TrackTimingMetaData trackTime)
{
    CFMutableDictionaryRef progressDict;

    if (trackTime.startTS <= lastPlayedTS && lastPlayedTS < trackTime.stopTS) {

        progressDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require(progressDict, exit);

        // Progress information in RTP time

        CFDictionarySetInt64(progressDict, CFSTR(kAirPlayKey_StartTimestamp), trackTime.startTS);
        CFDictionarySetInt64(progressDict, CFSTR(kAirPlayKey_CurrentTimestamp), lastPlayedTS);
        CFDictionarySetInt64(progressDict, CFSTR(kAirPlayKey_EndTimestamp), trackTime.stopTS);

        // Progress elapsed tima and duration in seconds

        double elapsed = (double)Mod32_Diff(lastPlayedTS, trackTime.startTS) / inSession->mainAudioCtx.sampleRate;
        double duration = (double)Mod32_Diff(trackTime.stopTS, trackTime.startTS) / inSession->mainAudioCtx.sampleRate;

        CFDictionarySetDouble(progressDict, kAirPlayMetaDataKey_ElapsedTime, elapsed);
        CFDictionarySetDouble(progressDict, kAirPlayMetaDataKey_Duration, duration);

        (void)AirPlayReceiverSessionSetProperty(inSession, kCFObjectFlagAsync, CFSTR(kAirPlayProperty_Progress), NULL, progressDict);
        CFRelease(progressDict);
    }

exit:
    return;
}
#endif

void AirPlayReceiverSessionSetProgress(AirPlayReceiverSessionRef inSession, TrackTimingMetaData trackTime)
{
#if (AIRPLAY_META_DATA)
    inSession->trackTime = trackTime;
#else
    (void)trackTime;
#endif
    _announcePlaying(inSession, true);
}

//===========================================================================================================================
//	_PerformPeriodTasks
//===========================================================================================================================

static void _PerformPeriodTasks(void* inContext)
{
    AirPlayReceiverSessionRef const me = (AirPlayReceiverSessionRef)inContext;
    AirTunesSource* const ats = &me->source;
    uint64_t ticks, idleTicks;
    static uint64_t maxBufferedTicks;

    if (!maxBufferedTicks) {
        maxBufferedTicks = kAirPlayBufferedAudioTimeoutSecs * UpTicksPerSecond();
    }

    // Variables shared with the render thread

    unsigned int receiveCount = ats->receiveCount;
    unsigned int activityCount = ats->activityCount;
    uint32_t lastPlayedTS = me->lastPlayedTS;
#if (AIRPLAY_META_DATA)
    TrackTimingMetaData trackTime = me->trackTime;
#endif

    // Check activity.

    ticks = UpTicks();
    if ((receiveCount == ats->lastReceiveCount) && (activityCount == ats->lastActivityCount)) {
        // If we've been idle for a while then log it.

        idleTicks = ticks - ats->lastActivityTicks;
        if ((ticks - ats->lastIdleLogTicks) > ats->idleLogIntervalTicks) {
            atr_ulog(kLogLevelInfo, "### Idle for %llu seconds\n", idleTicks / UpTicksPerSecond());
            ats->lastIdleLogTicks = ticks;
        }

        // If there hasn't been activity in long time, fail the session.
        uint64_t maxTicks = me->receiveAudioOverTCP ? maxBufferedTicks : ats->maxIdleTicks;
        if ((idleTicks > maxTicks)) {
            atr_ulog(kLogLevelError, "Idle timeout after %llu seconds with no audio\n", idleTicks / UpTicksPerSecond());
            AirPlayReceiverServerControl(me->server, kCFObjectFlagDirect, CFSTR(kAirPlayCommand_SessionDied), me, NULL, NULL);
            goto exit;
        }
    } else {
        ats->lastReceiveCount = receiveCount;
        ats->lastActivityCount = activityCount;
        ats->lastActivityTicks = ticks;
        ats->lastIdleLogTicks = ticks;
    }

    // Check for glitches.

    if (ticks >= me->glitchNextTicks) {
        int glitches;

        ++me->glitchTotalPeriods;
        glitches = me->glitchTotal - me->glitchLast;
        me->glitchLast += glitches;
        if (glitches > 0) {
            ++me->glitchyPeriods;
            atr_ulog(kLogLevelNotice, "### %d glitches in the last minute of %d minute(s) (%d%% glitchy)\n",
                glitches, me->glitchTotalPeriods, (me->glitchyPeriods * 100) / me->glitchTotalPeriods);
        } else {
            atr_ulog(kLogLevelVerbose, "No glitches in the last minute of %d minutes (%d%% glitchy)\n",
                me->glitchTotalPeriods, (me->glitchyPeriods * 100) / me->glitchTotalPeriods);
        }
        me->glitchNextTicks = ticks + me->glitchIntervalTicks;
    }

#if (AIRPLAY_META_DATA)
    if (!me->isLiveBroadcast) {
        updateTrackProgress(me, lastPlayedTS, trackTime);
    }
#endif

    _LogUpdate(me, ticks, false);

exit:
    return;
}

//===========================================================================================================================
//	_SessionLock
//===========================================================================================================================

static OSStatus _SessionLock(AirPlayReceiverSessionRef inSession)
{
    OSStatus err;

    require_action(inSession->mutexPtr, exit, err = kNotInitializedErr);

    err = pthread_mutex_lock(inSession->mutexPtr);
    require_noerr(err, exit);

exit:
    return (err);
}

//===========================================================================================================================
//	_SessionUnlock
//===========================================================================================================================

static OSStatus _SessionUnlock(AirPlayReceiverSessionRef inSession)
{
    OSStatus err;

    require_action(inSession->mutexPtr, exit, err = kNotInitializedErr);

    err = pthread_mutex_unlock(inSession->mutexPtr);
    require_noerr(err, exit);

exit:
    return (err);
}

//===========================================================================================================================
//	_UpdateFeedback
//===========================================================================================================================

static OSStatus _UpdateFeedback(AirPlayReceiverSessionRef inSession, CFDictionaryRef inInput, CFDictionaryRef* outOutput)
{
    OSStatus err;
    CFMutableDictionaryRef feedback = NULL;
    CFMutableArrayRef streams = NULL;
    CFMutableDictionaryRef stream = NULL;

    (void)inInput;

    require_action_quiet(inSession->mainAudioCtx.session || inSession->altAudioCtx.session, exit, err = kNoErr);
    require_action_quiet(outOutput, exit, err = kNoErr);

    feedback = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(feedback, exit, err = kNoMemoryErr);

    streams = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    require_action(streams, exit, err = kNoMemoryErr);
    CFDictionarySetValue(feedback, CFSTR(kAirPlayKey_Streams), streams);

    if (inSession->mainAudioCtx.session) {
        AirPlayTimestampTuple zeroTime;

        stream = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require_action(stream, exit, err = kNoMemoryErr);

        pthread_mutex_lock(inSession->mainAudioCtx.zeroTimeLockPtr);
        zeroTime = inSession->mainAudioCtx.zeroTime;
        pthread_mutex_unlock(inSession->mainAudioCtx.zeroTimeLockPtr);

        CFDictionarySetInt64(stream, CFSTR(kAirPlayKey_Type), inSession->mainAudioCtx.type);
        CFDictionarySetDouble(stream, CFSTR(kAirPlayKey_SampleRate), inSession->mainAudioCtx.rateAvg);

        // 'zeroTime' is only valid if we've had at least one rate update
        if (inSession->mainAudioCtx.rateUpdateCount > 0) {
            CFDictionarySetUInt64(stream, CFSTR(kAirPlayKey_StreamConnectionID), inSession->mainAudioCtx.connectionID);
            CFDictionarySetUInt64(stream, CFSTR(kAirPlayKey_Timestamp), zeroTime.hostTime);
            CFDictionarySetInt64(stream, CFSTR(kAirPlayKey_SampleTime), zeroTime.sampleTime);
        }

        CFArrayAppendValue(streams, stream);
        CFRelease(stream);
        stream = NULL;
    }

    *outOutput = feedback;
    feedback = NULL;
    err = kNoErr;

exit:
    CFReleaseNullSafe(stream);
    CFReleaseNullSafe(streams);
    CFReleaseNullSafe(feedback);
    return (err);
}

#if 0
#pragma mark -
#pragma mark == Control/Events ==
#endif

//===========================================================================================================================
//	_ControlIdleStateTransition
//===========================================================================================================================

static OSStatus _ControlIdleStateTransition(AirPlayReceiverSessionRef inSession, CFMutableDictionaryRef inResponseParams)
{
    OSStatus err;
    Boolean idle;

    idle = !_UsingScreenOrAudio(inSession);
    require_action_quiet((!inSession->sessionIdleValid || (idle != inSession->sessionIdle)), exit, err = kNoErr);

    atr_ulog(kLogLevelTrace, "mainAudio %c. altAudio %c.\n",
        inSession->mainAudioCtx.type != kAirPlayStreamType_Invalid ? 'Y' : 'N',
        inSession->altAudioCtx.type != kAirPlayStreamType_Invalid ? 'Y' : 'N');

    if (idle) {
        if (NULL != inSession->timingThreadPtr)
            _TimingFinalize(inSession);
    } else {

        check(NULL == inSession->timingThreadPtr);

        if (AirTunesClock_Class(inSession->airTunesClock) == kAirTunesClockNTP) {
            if (!IsValidSocket(inSession->timingSock)) {
                err = _TimingInitialize(inSession);
                require_noerr(err, exit);
                if (inResponseParams)
                    CFDictionarySetInt64(inResponseParams, CFSTR(kAirPlayKey_Port_Timing), inSession->timingPortLocal);
            }

            err = _TimingNegotiate(inSession);
            require_noerr(err, exit);
        }
    }

    inSession->sessionIdle = idle;
    inSession->sessionIdleValid = true;
    err = kNoErr;
exit:
    if (err)
        atr_ulog(kLogLevelWarning, "### Control idle state transition failed: %#m\n", err);
    return err;
}

//===========================================================================================================================
//	_ControlSetup
//===========================================================================================================================

static OSStatus
_ControlSetup(
    AirPlayReceiverSessionRef inSession,
    CFDictionaryRef inRequestParams,
    CFMutableDictionaryRef inResponseParams)
{
    CFStringRef timingProtocol;
    OSStatus err = kNoErr;

    require_action(!inSession->controlSetup, exit2, err = kAlreadyInitializedErr);

    timingProtocol = CFDictionaryGetCFString(inRequestParams, CFSTR(kAirPlayKey_TimingProtocol), NULL);
    if (timingProtocol == NULL || !CFEqual(timingProtocol, CFSTR(kAirPlayTimingProtocol_None))) {
        err = _ClockSetup(inSession, inRequestParams);
        require_noerr(err, exit);
    }

    if (timingProtocol && CFEqual(timingProtocol, CFSTR(kAirPlayTimingProtocol_PTP))) {
        _GeneralAudioAddIPAddrs(inSession, inResponseParams);
    }

    CFDictionarySetInt64(inResponseParams, CFSTR(kAirPlayKey_Port_Timing), inSession->timingPortLocal);

    if (inSession->useEvents) {
        err = ServerSocketOpen(inSession->peerAddr.sa.sa_family, SOCK_STREAM, IPPROTO_TCP, -kAirPlayPort_RTSPEvents,
            &inSession->eventPort, kSocketBufferSize_DontSet, &inSession->eventSock);
        require_noerr(err, exit);

        CFDictionarySetInt64(inResponseParams, CFSTR(kAirPlayKey_Port_Event), inSession->eventPort);

        atr_ulog(kLogLevelTrace, "Events set up on port %d\n", inSession->eventPort);
    }
    inSession->controlSetup = true;

exit:
    if (err)
        _ControlTearDown(inSession);

exit2:
    if (err)
        atr_ulog(kLogLevelWarning, "### Control setup failed: %#m\n", err);
    return (err);
}

//===========================================================================================================================
//	_ControlTearDown
//===========================================================================================================================

static void _ForgetEventClient(void* inCtx)
{
    AirPlayReceiverSessionRef session;

    session = (AirPlayReceiverSessionRef)inCtx;
    HTTPClientForget(&session->eventClient);
}

static void _ControlTearDown(AirPlayReceiverSessionRef inSession)
{
    dispatch_sync_f(inSession->eventQueue, inSession, _ForgetEventClient);
    ForgetCF(&inSession->pairVerifySession);
    ForgetSocket(&inSession->eventSock);
    if (inSession->controlSetup) {
        inSession->controlSetup = false;
        atr_ulog(kLogLevelTrace, "Control torn down\n");
    }
}

//===========================================================================================================================
//	_ControlStart
//===========================================================================================================================

static OSStatus _ControlStart(AirPlayReceiverSessionRef inSession)
{
    OSStatus err;
    SocketRef newSock = kInvalidSocketRef;
    sockaddr_ip sip;

    err = SocketAccept(inSession->eventSock, kAirPlayConnectTimeoutSecs, &newSock, &sip);
    require_noerr(err, exit);
    ForgetSocket(&inSession->eventSock);

    err = HTTPClientCreateWithSocket(&inSession->eventClient, newSock);
    require_noerr(err, exit);
    newSock = kInvalidSocketRef;

    HTTPClientSetDispatchQueue(inSession->eventClient, inSession->eventQueue);
    HTTPClientSetLogging(inSession->eventClient, atr_events_ucat());

    // Configure HTTPClient for encryption if needed

    if (inSession->pairVerifySession) {
        NetTransportDelegate delegate;
        uint8_t readKey[32], writeKey[32];

        err = PairingSessionDeriveKey(inSession->pairVerifySession, kAirPlayPairingEventsKeySaltPtr, kAirPlayPairingEventsKeySaltLen,
            kAirPlayPairingEventsKeyReadInfoPtr, kAirPlayPairingEventsKeyReadInfoLen, sizeof(readKey), readKey);
        require_noerr(err, exit);

        err = PairingSessionDeriveKey(inSession->pairVerifySession, kAirPlayPairingEventsKeySaltPtr, kAirPlayPairingEventsKeySaltLen,
            kAirPlayPairingEventsKeyWriteInfoPtr, kAirPlayPairingEventsKeyWriteInfoLen, sizeof(writeKey), writeKey);
        require_noerr(err, exit);

        err = NetTransportChaCha20Poly1305Configure(&delegate, atr_ucat(), readKey, NULL, writeKey, NULL);
        require_noerr(err, exit);
        MemZeroSecure(readKey, sizeof(readKey));
        MemZeroSecure(writeKey, sizeof(writeKey));
        HTTPClientSetTransportDelegate(inSession->eventClient, &delegate);
    }

    atr_ulog(kLogLevelTrace, "Events started on port %d to port %d\n", inSession->eventPort, SockAddrGetPort(&sip));

exit:
    ForgetSocket(&newSock);
    if (err)
        atr_ulog(kLogLevelWarning, "### Event start failed: %#m\n", err);
    return (err);
}

#if 0
#pragma mark -
#pragma mark == GeneralAudio ==
#endif

#if (AIRPLAY_BUFFERED)
//===========================================================================================================================
//	_GeneralBufferedAudioSize
//===========================================================================================================================

static size_t _GeneralBufferedAudioSize(AirPlayReceiverSessionRef me)
{
    int32_t audioBufferSize = kAirTunesBufferedDefaultSize;

    CFNumberRef bufferSizeRef = (CFNumberRef)AirPlayReceiverSessionCopyProperty(me, 0, CFSTR(kAirPlayProperty_BufferedAudioSize), NULL, NULL);
    if (bufferSizeRef) {
        CFNumberGetValue(bufferSizeRef, kCFNumberSInt32Type, &audioBufferSize);
        if (audioBufferSize < kAirTunesBufferedMinSize) {
            audioBufferSize = kAirTunesBufferedMinSize;
            ;
        }
        CFRelease(bufferSizeRef);
    }

    return (size_t)audioBufferSize;
}
#endif

static CFArrayRef createTimingAddresses(const char* interface, sockaddr_ip selfAddr, Boolean ipv4Only)
{
    CFMutableArrayRef addressesForInterface = NULL;
    AirPlayNetworkAddressRef localNetworkAddress = NULL;
    CFArrayRef addressStrings = NULL;
    OSStatus err = kNoErr;

    // Get addresses for the interface this session is on.
    err = AirPlayNetworkAddressCreateForInterface(NULL, interface, ipv4Only, &addressesForInterface);
    require_noerr(err, exit);

    // We didn't find any addresses for our interface so just use our connection's address
    if (CFArrayGetCount(addressesForInterface) == 0) {

        err = AirPlayNetworkAddressCreateWithSocketAddr(NULL, selfAddr, &localNetworkAddress);
        require_noerr(err, exit);

        AirPlayNetworkAddressSetPort(localNetworkAddress, 0);
        CFArrayAppendValue(addressesForInterface, localNetworkAddress);
        CFRelease(localNetworkAddress);
    }

    addressStrings = CFArrayCreateMap(addressesForInterface, networkAddrToString, NULL);

exit:
    CFReleaseNullSafe(addressesForInterface);

    return addressStrings;
}

static OSStatus _GeneralAudioAddIPAddrs(AirPlayReceiverSessionRef me, CFMutableDictionaryRef inResponseParams)
{
#if defined(AIRPLAY_IPV4_ONLY)
    static const Boolean ipv4Only = true;
#else
    static const Boolean ipv4Only = false;
#endif
    CFArrayRef addressStrings = NULL;
    CFMutableDictionaryRef localInfo = NULL;
    sockaddr_ip selfAddr;
    AirPlayNetworkAddressRef selfAddress = NULL;
    CFStringRef selfAddressAsString = NULL;
    OSStatus err = kNoErr;

    selfAddr = me->connection->httpCnx->selfAddr;
    addressStrings = createTimingAddresses(me->connection->ifName, selfAddr, ipv4Only);
    require_action(addressStrings, exit, err = kParamErr);
    require_action(CFArrayGetCount(addressStrings) > 0, exit, err = kParamErr);

    localInfo = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    err = AirPlayNetworkAddressCreateWithSocketAddr(NULL, selfAddr, &selfAddress);
    require_noerr(err, exit);

    AirPlayNetworkAddressSetPort(selfAddress, 0);
    err = AirPlayNetworkAddressCopyStringRepresentation(selfAddress, &selfAddressAsString);
    require_noerr(err, exit);
    check(CFArrayContainsValue(addressStrings, CFRangeMake(0, CFArrayGetCount(addressStrings)), selfAddressAsString));

    // Sender wants a unique id - we'll use a randomly generated uuid.
    uint8_t uuid[16];
    UUIDGet(uuid);

    CFDictionarySetUUIDString(localInfo, CFSTR(kAirPlayTimingPeerInfoKeyPeer_ID), uuid, sizeof(uuid), NULL, kUUIDFlags_None);
    CFDictionarySetValue(localInfo, CFSTR(kAirPlayKey_Addresses), addressStrings);
    CFDictionarySetBoolean(localInfo, CFSTR(kAirPlayKey_SupportsClockPortMatchingOverride), false);

    atr_ulog(kLogLevelNotice, "%s: our ptp info: %@\n", __func__, localInfo);

    CFDictionarySetValue(inResponseParams, CFSTR(kAirPlayKey_TimingPeerInfo), localInfo);

exit:
    CFReleaseNullSafe(selfAddressAsString);
    CFReleaseNullSafe(selfAddress);
    CFReleaseNullSafe(addressStrings);
    CFReleaseNullSafe(localInfo);

    return err;
}

//===========================================================================================================================
//	_GeneralAudioSetup
//===========================================================================================================================

static OSStatus
_GeneralAudioSetup(
    AirPlayReceiverSessionRef me,
    AirPlayStreamType inStreamType,
    CFDictionaryRef inStreamDesc,
    CFMutableDictionaryRef inResponseParams)
{
    AirTunesSource* const ats = &me->source;
    AirPlayAudioStreamContext* const ctx = &me->mainAudioCtx;
    OSStatus err;
    CFMutableDictionaryRef responseStreamDesc;
    AirPlayAudioFormat format;
    AudioStreamBasicDescription asbd;
    sockaddr_ip sip;
    size_t i, n;
    uint32_t latencyMs;
    uint64_t streamConnectionID = 0;
    uint8_t outputKey[32];
    Boolean haveCryptoKeys;
#if (AIRPLAY_BUFFERED)
    size_t audioBufferSize = 0;
#endif

    ctx->sampleTimeOffset = 0;

    require_action(!IsValidSocket(ctx->dataSock), exit2, err = kAlreadyInitializedErr);

    responseStreamDesc = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(responseStreamDesc, exit, err = kNoMemoryErr);

    streamConnectionID = (uint64_t)CFDictionaryGetInt64(inStreamDesc, CFSTR(kAirPlayKey_StreamConnectionID), &err);
    if (err) {
        UUIDData uuid;

        UUIDGet(&uuid);
        ctx->connectionID = ReadBig64(uuid.bytes);
        err = kNoErr;
    } else {
        ctx->connectionID = streamConnectionID;
    }

    if (me->pairVerifySession)
        require_action(streamConnectionID, exit, err = kVersionErr);

    MemZeroSecure(&ctx->outputCryptor, sizeof(ctx->outputCryptor));
    if (CFDictionaryContainsKey(inStreamDesc, CFSTR(kAirPlayKey_SharedEncryptionKey))) {
        size_t sharedKeyLen;
        CFDictionaryGetData(inStreamDesc, CFSTR(kAirPlayKey_SharedEncryptionKey), outputKey, 32, &sharedKeyLen, &err);
        require_noerr(err, exit);
        require_action(sharedKeyLen == 32, exit, err = kSizeErr);
        haveCryptoKeys = true;
    } else if (streamConnectionID && me->pairVerifySession) {
        err = _GetStreamSecurityKeys(me, streamConnectionID, 0, NULL, 32, outputKey);
        require_noerr(err, exit);
        haveCryptoKeys = true;
    } else {
        haveCryptoKeys = false;
    }

    if (haveCryptoKeys) {
        memset(ctx->outputCryptor.nonce, 0, sizeof(ctx->outputCryptor.nonce));
        memcpy(ctx->outputCryptor.key, outputKey, 32);
        ctx->outputCryptor.isValid = true;
    }

    check(inStreamType == kAirPlayStreamType_GeneralAudio || inStreamType == kAirPlayStreamType_BufferedAudio);

    if (inStreamType == kAirPlayStreamType_GeneralAudio)
        ctx->label = "General";
    else if (inStreamType == kAirPlayStreamType_BufferedAudio)
        ctx->label = "Buffered";

    ctx->type = inStreamType;

    me->compressionType = (AirPlayCompressionType)CFDictionaryGetInt64(inStreamDesc, CFSTR(kAirPlayKey_CompressionType), NULL);
    format = (AirPlayAudioFormat)CFDictionaryGetInt64(inStreamDesc, CFSTR(kAirPlayKey_AudioFormat), NULL);
    if (format == kAirPlayAudioFormat_Invalid) {
        if (me->compressionType == kAirPlayCompressionType_PCM)
            format = kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo;
#if (AIRPLAY_ALAC)
        else if (me->compressionType == kAirPlayCompressionType_ALAC)
            format = kAirPlayAudioFormat_ALAC_44KHz_16Bit_Stereo;
#endif
        else if (me->compressionType == kAirPlayCompressionType_AAC_LC)
            format = kAirPlayAudioFormat_AAC_LC_44KHz_Stereo;
        else {
            dlogassert("Bad compression type: 0x%X", me->compressionType);
            err = kUnsupportedErr;
            goto exit;
        }
    }
    err = AirPlayAudioFormatToASBD(format, &asbd, &ctx->bitsPerSample);
    require_noerr(err, exit);
    me->audioFormat = format;

    if (me->compressionType == kAirPlayCompressionType_Undefined) {
        me->compressionType = AudioFormatIDToAirPlayCompressionType(asbd.mFormatID);
    } else {
        check(me->compressionType == AudioFormatIDToAirPlayCompressionType(asbd.mFormatID));
    }

    {
        CFNumberRef formatRef = CFNumberCreateInt64(format);
        (void)AirPlayReceiverSessionSetProperty(me, kCFObjectFlagDirect, CFSTR(kAirPlayProperty_AudioFormat), NULL, formatRef);
        CFRelease(formatRef);
    }
#if (AIRPLAY_META_DATA)
    CFNumberRef defaultFlags = CFNumberCreateUInt64(kAirPlayUIElementFlag_Default);
    if (defaultFlags) {
        (void)AirPlayReceiverSessionSetProperty(me, kCFObjectFlagDirect, CFSTR(kAirPlayProperty_SupportedUIElements), NULL, defaultFlags);
        CFReleaseNullSafe(defaultFlags);
    }
#endif

    ctx->sampleRate = (uint32_t)asbd.mSampleRate;
    ctx->channels = asbd.mChannelsPerFrame;
    ctx->bytesPerUnit = asbd.mBytesPerFrame;
    if (ctx->bytesPerUnit == 0)
        ctx->bytesPerUnit = (RoundUp(ctx->bitsPerSample, 8) * ctx->channels) / 8;

    me->rtpAudioDupsInitialized = false;
    me->redundantAudio = (int)CFDictionaryGetInt64(inStreamDesc, CFSTR(kAirPlayKey_RedundantAudio), NULL);
    me->screen = CFDictionaryGetBoolean(inStreamDesc, CFSTR(kAirPlayKey_UsingScreen), NULL);
    me->rtcpPortRemote = (int)CFDictionaryGetInt64(inStreamDesc, CFSTR(kAirPlayKey_Port_Control), NULL);
    if (me->rtcpPortRemote <= 0)
        me->rtcpPortRemote = kAirPlayFixedPort_RTCPLegacy;
    latencyMs = (uint32_t)CFDictionaryGetInt64(inStreamDesc, CFSTR(kAirPlayKey_AudioLatencyMs), NULL);

    if (latencyMs > 0) {
        me->minLatency = ctx->sampleRate * latencyMs / 1000;
        me->maxLatency = ctx->sampleRate * latencyMs / 1000;
    } else {
        me->minLatency = (uint32_t)CFDictionaryGetInt64(inStreamDesc, CFSTR(kAirPlayKey_LatencyMin), NULL);
        me->maxLatency = (uint32_t)CFDictionaryGetInt64(inStreamDesc, CFSTR(kAirPlayKey_LatencyMax), NULL);
    }
    me->audioQoS = me->screen ? kSocketQoS_AirPlayScreenAudio : kSocketQoS_AirPlayAudio;
    me->framesPerPacket = (uint32_t)CFDictionaryGetInt64(inStreamDesc, CFSTR(kAirPlayKey_FramesPerPacket), NULL);
    i = (me->framesPerPacket + ((me->framesPerPacket / kAirTunesMaxSkewAdjustRate) + 16)) * ctx->bytesPerUnit;
    me->nodeBufferSize = kRTPHeaderSize + Max(kAirTunesMaxPacketSizeUDP, i);

    ctx->rateUpdateNextTicks = 0;
    ctx->rateUpdateIntervalTicks = SecondsToUpTicks(1);
    ctx->rateUpdateCount = 0;
    ctx->rateAvg = (Float32)ctx->sampleRate;

    err = pthread_mutex_init(&ctx->zeroTimeLock, NULL);
    require_noerr(err, exit);
    ctx->zeroTimeLockPtr = &ctx->zeroTimeLock;

// Set up RTP.
#if (AIRPLAY_BUFFERED)
    me->receiveAudioOverTCP = (inStreamType == kAirPlayStreamType_BufferedAudio);
    if (me->receiveAudioOverTCP) {
        me->nodeCount = 0; // And we'll pack them as densely as possible
        audioBufferSize = _GeneralBufferedAudioSize(me);

        err = ServerSocketOpen(me->peerAddr.sa.sa_family, SOCK_STREAM, IPPROTO_TCP, -kAirPlayPort_RTPAudio,
            &me->rtpAudioPort, -kAirTunesRTPOverTCPSocketBufferSize, &me->listenerSock);

        (void)AirPlayReceiverSessionSetProperty(me, kCFObjectFlagDirect, CFSTR(kAirPlayProperty_AirPlayMode), NULL, CFSTR("Buffered AirPlay"));

        // Initialize the Buffered AirPlay State Machine
        _bufferedMachineReset(&me->buffered);
    } else
#endif
    {
        size_t blockSize = sizeof(AirTunesBufferNode) + me->nodeBufferSize;
        me->nodeCount = kAirTunesBufferNodeCountUDP;
        audioBufferSize = me->nodeCount * blockSize;

        err = ServerSocketOpen(me->peerAddr.sa.sa_family, SOCK_DGRAM, IPPROTO_UDP, -kAirPlayPort_RTPAudio,
            &me->rtpAudioPort, -kAirTunesRTPSocketBufferSize, &ctx->dataSock);

        if (!err) {
            SocketSetQoS(ctx->dataSock, me->audioQoS);
        }

        (void)AirPlayReceiverSessionSetProperty(me, kCFObjectFlagDirect, CFSTR(kAirPlayProperty_AirPlayMode), NULL, CFSTR("Legacy AirPlay"));
    }
    require_noerr(err, exit);

    err = OpenSelfConnectedLoopbackSocket(&ctx->cmdSock);
    require_noerr(err, exit);

    // Set up RTCP.

    SockAddrCopy(&me->peerAddr, &sip);
    err = ServerSocketOpen(sip.sa.sa_family, SOCK_DGRAM, IPPROTO_UDP, -kAirPlayPort_RTCPServer, &me->rtcpPortLocal,
        -kAirTunesRTCPOSocketBufferSize, &me->rtcpSock);
    require_noerr(err, exit);

    SocketSetPacketTimestamps(me->rtcpSock, true);
    SocketSetQoS(me->rtcpSock, me->audioQoS);

    SockAddrSetPort(&sip, me->rtcpPortRemote);
    me->rtcpRemoteAddr = sip;
    me->rtcpRemoteLen = SockAddrGetSize(&sip);
    err = connect(me->rtcpSock, &sip.sa, me->rtcpRemoteLen);
    err = map_socket_noerr_errno(me->rtcpSock, err);
    if (err)
        dlog(kLogLevelNotice, "### RTCP connect UDP to %##a failed (using sendto instead): %#m\n", &sip, err);
    me->rtcpConnected = !err;

    // Set up retransmissions.

    if (me->redundantAudio) {
        ats->rtcpRTDisable = true;
    } else {
        ats->rtcpRTListStorage = (AirTunesRetransmitNode*)malloc(kAirTunesRetransmitCount * sizeof(AirTunesRetransmitNode));
        require_action(ats->rtcpRTListStorage, exit, err = kNoMemoryErr);

        n = kAirTunesRetransmitCount - 1;
        for (i = 0; i < n; ++i) {
            ats->rtcpRTListStorage[i].next = &ats->rtcpRTListStorage[i + 1];
        }
        ats->rtcpRTListStorage[i].next = NULL;
        ats->rtcpRTFreeList = ats->rtcpRTListStorage;

        ats->rtcpRTMinRTTNanos = INT64_MAX;
        ats->rtcpRTMaxRTTNanos = INT64_MIN;
        ats->rtcpRTAvgRTTNanos = 100000000; // Default to 100 ms.
        ats->rtcpRTTimeoutNanos = 100000000; // Default to 100 ms.
        ats->rtcpRTDisable = me->redundantAudio;
        ats->retransmitMinNanos = UINT64_MAX;
        ats->retransmitRetryMinNanos = UINT64_MAX;

        {
            ats->retransmitMaxLoss = kAirTunesRetransmitMaxLoss;
        }
    }

    // Set up buffering. The busy list is doubly-linked and circular with a sentinel node to simplify and speed up linked list code.

    allocator_setup(me, audioBufferSize, me->nodeBufferSize);

    me->audioPacketBufferSize = me->nodeBufferSize;
    me->audioPacketBuffer = malloc(me->audioPacketBufferSize);

    me->busyNodeCount = 0;
    me->busyListSentinelStorage.prev = &me->busyListSentinelStorage;
    me->busyListSentinelStorage.next = &me->busyListSentinelStorage;
    me->busyListSentinel = &me->busyListSentinelStorage;

    // Set up temporary buffers.

    if (0) {
    } // Empty if to simplify conditional logic below.
    else if (me->compressionType == kAirPlayCompressionType_AAC_LC) {
        err = _AudioDecoderInitialize(me);
        require_noerr(err, exit);

        n = me->nodeBufferSize;
    } else if (me->compressionType == kAirPlayCompressionType_PCM) {
        n = me->nodeBufferSize;
    }
#if (AIRPLAY_ALAC)
    else if (me->compressionType == kAirPlayCompressionType_ALAC) {
        err = _AudioDecoderInitialize(me);
        require_noerr(err, exit);

        n = AppleLosslessBufferSize(me->framesPerPacket, ctx->channels, ctx->bitsPerSample);
        n = iceil2((uint32_t)n);
    }
#endif
    else {
        dlogassert("Unsupported compression type: %d", me->compressionType);
        err = kUnsupportedErr;
        goto exit;
    }

    me->decodeBufferSize = n;
    me->decodeBuffer = (uint8_t*)malloc(me->decodeBufferSize);
    require_action(me->decodeBuffer, exit, err = kNoMemoryErr);

    me->readBufferSize = n;
    me->readBuffer = (uint8_t*)malloc(me->readBufferSize);
    require_action(me->readBuffer, exit, err = kNoMemoryErr);

    me->skewAdjustBufferSize = n;
    me->skewAdjustBuffer = (uint8_t*)malloc(me->skewAdjustBufferSize);
    require_action(me->skewAdjustBuffer, exit, err = kNoMemoryErr);

    EWMA_FP_Init(&gAirPlayAudioStats.bufferAvg, 0.25, kEWMAFlags_StartWithFirstValue);
    gAirPlayAudioStats.lostPackets = 0;
    gAirPlayAudioStats.unrecoveredPackets = 0;
    gAirPlayAudioStats.latePackets = 0;

    // Add the stream to the response.

    CFDictionarySetInt64(responseStreamDesc, CFSTR(kAirPlayKey_Type), ctx->type);
    CFDictionarySetInt64(responseStreamDesc, CFSTR(kAirPlayKey_Port_Data), me->rtpAudioPort);
    CFDictionarySetInt64(responseStreamDesc, CFSTR(kAirPlayKey_Port_Control), me->rtcpPortLocal);

#if (AIRPLAY_BUFFERED)
    if (ctx->type == kAirPlayStreamType_BufferedAudio) {
        CFDictionarySetInt64(responseStreamDesc, CFSTR(kAirPlayKey_AudioBufferSize), audioBufferSize);
    }
#endif

    err = _AddResponseStream(inResponseParams, responseStreamDesc);
    require_noerr(err, exit);

    ctx->session = me;
    atr_ulog(kLogLevelTrace, "%s audio set up for %s on port %d, RTCP on port %d\n",
        ctx->label, AirPlayAudioFormatToString(format), me->rtpAudioPort, me->rtcpPortLocal);

    // If the session's already started then immediately start the thread process it.

    if (me->sessionStarted && !ctx->threadPtr) {
        err = pthread_create(&ctx->thread, NULL, _GeneralAudioThread, me);
        require_noerr(err, exit);
        ctx->threadPtr = &ctx->thread;
    }

exit:
    CFReleaseNullSafe(responseStreamDesc);
    if (err)
        _TearDownStream(me, ctx, false);

exit2:
    if (err)
        atr_ulog(kLogLevelWarning, "### General audio setup failed: %#m\n", err);
    return (err);
}

//===========================================================================================================================
//	_GeneralAudioThread
//===========================================================================================================================

static void* _GeneralAudioThread(void* inArg)
{
    AirPlayReceiverSessionRef const session = (AirPlayReceiverSessionRef)inArg;
    AirPlayAudioStreamContext* const ctx = &session->mainAudioCtx;
    SocketRef rtpSock = ctx->dataSock;
    SocketRef const rtcpSock = session->rtcpSock;
    SocketRef const cmdSock = ctx->cmdSock;
    fd_set readSet;
    int maxFd;
    int n;
    OSStatus err;
    struct timeval timeout;
    Boolean rtpBuffersAreAvailable = true;
    Boolean done = false;
#if (AIRPLAY_BUFFERED)
    SocketRef newSocket = kInvalidSocketRef;
#endif
    NetSocketRef dataNetSock = NULL;

    SetThreadName("AirPlayAudioReceiver");
    SetCurrentThreadPriority(kAirPlayThreadPriority_AudioReceiver);

#if (AIRPLAY_BUFFERED)
    if (session->receiveAudioOverTCP) {
        err = SocketAccept(session->listenerSock, kAirPlayConnectTimeoutSecs, &newSocket, NULL);
        require_noerr(err, exit);

        /* if( !session->qosDisabled ) */ SocketSetQoS(newSocket, session->audioQoS);

        rtpSock = newSocket;

        ForgetSocket(&session->listenerSock); // don't need listening socket anymore.

        atr_ulog(kLogLevelTrace, "Buffered audio receiver started\n");

        err = NetSocket_CreateWithNative(&dataNetSock, newSocket);
        require_noerr(err, exit);

        newSocket = kInvalidSocketRef; // netSock now owns newSocket.
    }
#endif

    while (!done) {
        FD_ZERO(&readSet);
        maxFd = -1;

        // Read from the RTP socket if we have a place to put the audio
        if (rtpBuffersAreAvailable && IsValidSocket(rtpSock)) {
            if ((int)rtpSock > maxFd) {
                maxFd = rtpSock;
            }
            FD_SET(rtpSock, &readSet);
        } else {
            rtpBuffersAreAvailable = true;
        }

        if ((int)rtcpSock > maxFd) {
            maxFd = rtcpSock;
        }

        if ((int)cmdSock > maxFd) {
            maxFd = cmdSock;
        }
        maxFd += 1;

        FD_SET(rtcpSock, &readSet);
        FD_SET(cmdSock, &readSet);

        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        n = select(maxFd, &readSet, NULL, NULL, &timeout);
        err = select_errno(n);
        if (err == EINTR || err == kTimeoutErr)
            continue;
        if (err) {
            dlogassert("select() error: %#m", err);
            usleep(100000);
            continue;
        }

        if (IsValidSocket(rtpSock) && FD_ISSET(rtpSock, &readSet)) {

#if (AIRPLAY_BUFFERED)
            if (session->receiveAudioOverTCP && dataNetSock) {
                err = _receiveBufferedRTP(session, dataNetSock);
            } else
#endif
            {
                err = _GeneralAudioReceiveRTP(session, NULL, 0);
            }

            if (err == kNoMemoryErr) {
                rtpBuffersAreAvailable = false;
            } else if (err == kConnectionErr) {
                atr_ulog(kLogLevelWarning, "Connection error on RTP socket\n");
                rtpSock = kInvalidSocketRef;
            } else if (err != kNoErr) {
                atr_ulog(kLogLevelTrace, "GeneralAudioReceiveRTP  %#m\n", err);
            }
        }
        if (FD_ISSET(rtcpSock, &readSet)) {
            _GeneralAudioReceiveRTCP(session, rtcpSock, kRTCPTypeAny);
        }
        if (FD_ISSET(cmdSock, &readSet)) {
            done = true; // The only event is quit so break if anything is pending.
        }
    }

exit:
#if (AIRPLAY_BUFFERED)
    NetSocket_Forget(&dataNetSock);
    ForgetSocket(&newSocket);

#endif
    atr_ulog(kLogLevelTrace, "General audio thread exit\n");
    return (NULL);
}

//===========================================================================================================================
//	AirPlayReceiverSessionSetRateAndAnchorTime
//===========================================================================================================================
OSStatus AirPlayReceiverSessionSetRateAndAnchorTime(AirPlayReceiverSessionRef inSession, const AirPlayAnchor* anchor)
{
    AirTunesSource* source = &inSession->source;
    uint64_t anchorTimeHostTicks = 0;
    OSStatus err = kNoErr;
    static const int kTimeLineMisMatchSleepInterval = 10 * 1000; // Try every 10ms
    static const int kTimeLineMismatchMaxTries = 200; // Try 10 times

    require_action(inSession, exit, err = kNotInitializedErr);

    atr_ulog(kLogLevelNotice, "SetRateAndAnchorTime inRate %u rtpTme %u netTime (timeline=%llx secs=%d flags=%x nanoSec=%llu)\n",
             anchor->rate, anchor->rtpTime,
             anchor->netTime.timelineID, anchor->netTime.secs, anchor->netTime.flags, AirTunesTime_ToNanoseconds(&anchor->netTime));

    if (anchor->rate > 0) {
        int tries;
        inSession->playTicks = UpTicks();
        for (tries = 0; tries < kTimeLineMismatchMaxTries; tries++) {
            err = AirTunesClock_ConvertNetworkTimeToUpTicks(inSession->airTunesClock, anchor->netTime, &anchorTimeHostTicks);

            if (err != kAirTunesClockError_TimelineIDMismatch)
                break;

            usleep(kTimeLineMisMatchSleepInterval);
        }
        atr_ulog(kLogLevelNotice, "SetRateAndAnchorTime inSession->receiveAudioOverTCP:%d\n", inSession->receiveAudioOverTCP);

        atr_ulog(kLogLevelNotice, "SetRateAndAnchorTime timeline mismatch %s after #%d tries\n", (err == kNoErr ? "succeeded" : "failed"), tries);
        // For buffered audio we want to abort on a timeline mismatch
        // For realtime audio we start even without a match.
        //require_quiet(!(inSession->receiveAudioOverTCP && err), exit);
        require_quiet(!(err), exit);
    }

    _announcePlaying(inSession, anchor->rate > 0);

    _SessionLock(inSession);
    {
        // Reset state (similar to the flush case)
        inSession->lastPlayedValid = false;

#if (AIRPLAY_BUFFERED)
        source->usingAnchorTime = true;

        source->setRateTiming.anchor = *anchor;
        source->setRateTiming.anchorUpTicks = anchorTimeHostTicks;
        source->setRateTiming.lastUpTicks = anchorTimeHostTicks;
        source->setRateTiming.lastNetworkTime = anchor->netTime;
        source->setRateTiming.lastIdealRTPTime = anchor->rtpTime;
        source->setRateTiming.forceOffsetRecalculation = true;
        source->setRateTiming.lastIdealRTPTimeFracs = 0;
        source->setRateTiming.nextSkewAdjustmentTicks = 0;

        source->rtpOffsetActive = 0;
        source->rtpOffsetMaxSkew = 0;
        source->rtpOffsetSkewResetCount = 0;

        PIDReset(&source->rtpSkewPIDController);
#endif
    }
    _SessionUnlock(inSession);

exit:
    return (err);
}

//===========================================================================================================================
//	AirPlayReceiverSessionSetPeers
//===========================================================================================================================

OSStatus AirPlayReceiverSessionSetPeers(AirPlayReceiverSessionRef inSession, CFArrayRef peers)
{
    OSStatus err = kNoErr;
    CFArrayRef peerAddrs = NULL;
    CFArrayRef peerAddrsOfRemotes = NULL;
    AirPlayNetworkAddressRef ourAddress = NULL;
    CFStringRef localInterface = NULL;
    CFArrayRef filteredPeers = NULL;
    CFMutableArrayRef addressesForInterface = NULL;

    require_action(inSession, exit, err = kNotInitializedErr);
    require_action(inSession->airTunesClock, exit, err = kNotInitializedErr);
    require_action(peers, exit, err = kNoErr);

    err = AirPlayNetworkAddressCreateWithSocketAddr(kCFAllocatorDefault, inSession->connection->httpCnx->selfAddr, &ourAddress);
    require_noerr(err, exit);
    AirPlayNetworkAddressSetPort(ourAddress, 0);

    localInterface = CFStringCreateWithCString(kCFAllocatorDefault, inSession->connection->ifName, kCFStringEncodingUTF8);
    require_action(localInterface, exit, err = kNoMemoryErr);

    atr_ulog(kLogLevelNotice, "%s: %@ on %@\n", __func__, peers, localInterface);

#if defined(AIRPLAY_IPV4_ONLY)
    filteredPeers = CFArrayCreateMap(peers, filterKeepIPv4Addrs, NULL);
#else
    filteredPeers = (CFArrayRef)CFRetain(peers);
#endif

    err = AirPlayNetworkAddressCreateForInterface(NULL, inSession->connection->ifName, false, &addressesForInterface);
    require_noerr(err, exit);

    peerAddrs = CFArrayCreateMap(filteredPeers, ptpNodeToNetworkAddr, (void*)localInterface);
    peerAddrsOfRemotes = CFArrayCreateDifference(peerAddrs, addressesForInterface);

    err = AirTunesClock_SetPeerList(inSession->airTunesClock, peerAddrsOfRemotes);
    require_noerr(err, exit);

exit:
    CFReleaseNullSafe(addressesForInterface);
    CFReleaseNullSafe(ourAddress);
    CFReleaseNullSafe(localInterface);
    CFReleaseNullSafe(peerAddrs);
    CFReleaseNullSafe(peerAddrsOfRemotes);
    CFReleaseNullSafe(filteredPeers);

    return (err);
}

#if defined(AIRPLAY_IPV4_ONLY)

// Return the address if is it ipv4 and NULL otherwise
static CFTypeRef filterKeepIPv4Addrs(CFTypeRef addressDescription, void* param)
{
    char addressCStr[128];
    CFStringRef resultAddress = NULL;
    OSStatus err = kNoErr;

    (void)param;
    require(CFIsType(addressDescription, CFString), exit);

    CFStringGetCString((CFStringRef)addressDescription, addressCStr, sizeof(addressCStr), kCFStringEncodingUTF8);
    err = StringToIPv4Address(addressCStr, kStringToIPAddressFlagsNone, NULL, NULL, NULL, NULL, NULL);
    require_noerr_quiet(err, exit);

    resultAddress = (CFStringRef)addressDescription;
    CFRetain(resultAddress);
exit:
    return resultAddress;
}
#endif

// Conver the network address to a string
static CFTypeRef networkAddrToString(CFTypeRef inAddress, void* unused)
{
    CFStringRef addressAsString = NULL;
    AirPlayNetworkAddressRef address = (AirPlayNetworkAddressRef)inAddress;
    (void)unused;

    (void)AirPlayNetworkAddressCopyStringRepresentation(address, &addressAsString);

    return addressAsString;
}

static CFTypeRef ptpNodeToNetworkAddr(CFTypeRef addressDescription, void* inInterface)
{
    CFStringRef interface = (CFStringRef)inInterface;
    CFStringRef addressString = (CFStringRef)addressDescription;
    AirPlayNetworkAddressRef address = NULL;

    require(CFIsType(interface, CFString), exit);
    require(CFIsType(addressString, CFString), exit);

    (void)AirPlayNetworkAddressCreateWithString(kCFAllocatorDefault, addressString, interface, &address);

exit:
    return address;
}

//===========================================================================================================================
//	_GeneralAudioReceiveRTCP
//===========================================================================================================================

static OSStatus _GeneralAudioReceiveRTCP(AirPlayReceiverSessionRef inSession, SocketRef inSock, RTCPType inExpectedType)
{
#if (AIRPLAY_GENERAL_AUDIO_SKEW)
    AirTunesSource* const ats = &inSession->source;
    AirTunesTime recvTime;
#endif
    OSStatus err;
    RTCPPacket pkt;
    size_t len;
    uint64_t ticks;
    int tmp;

    // Read the packet and do some very simple validity checks.

    err = SocketRecvFrom(inSock, &pkt, sizeof(pkt), &len, NULL, 0, NULL, &ticks, NULL, NULL);
    if (err == EWOULDBLOCK)
        goto exit;
    require_noerr(err, exit);
    if (len < sizeof(pkt.header)) {
        dlogassert("Bad size: %zu < %zu", sizeof(pkt.header), len);
        err = kSizeErr;
        goto exit;
    }

    tmp = RTCPHeaderExtractVersion(pkt.header.v_p_c);
    if (tmp != kRTPVersion) {
        dlogassert("Bad version: %d", tmp);
        err = kVersionErr;
        goto exit;
    }
    if ((inExpectedType != kRTCPTypeAny) && (pkt.header.pt != inExpectedType)) {
        dlogassert("Wrong packet type: %d vs %d", pkt.header.pt, inExpectedType);
        err = kTypeErr;
        goto exit;
    }

    // Dispatch the packet to the appropriate handler.

    switch (pkt.header.pt) {
#if (AIRPLAY_GENERAL_AUDIO_SKEW)
    case kRTCPTypeTimeSyncResponse:
        require_action(len >= sizeof(pkt.timeSync), exit, err = kSizeErr);
        AirTunesClock_GetSynchronizedTimeNearUpTicks(inSession->airTunesClock, &recvTime, ticks);
        err = _TimingProcessResponse(inSession, &pkt.timeSync, &recvTime);
        break;

    case kRTCPTypeTimeAnnounce:
        require_action(len >= sizeof(pkt.timeAnnounce), exit, err = kSizeErr);
        _SessionLock(inSession);
        ats->timeAnnounceV_P_M = pkt.timeAnnounce.v_p_m;
        ats->timeAnnounceRTPTime = ntohl(pkt.timeAnnounce.rtpTime);
        ats->timeAnnounceNetworkTimeFP = NTPSeconds1970FP(ntohl(pkt.timeAnnounce.ntpTimeHi), ntohl(pkt.timeAnnounce.ntpTimeLo)); // timeAnnounceNetworkTimeFP is common to NTP & PTP timeAnnounce processing
        ats->timeAnnounceRTPApply = ntohl(pkt.timeAnnounce.rtpApply);
        ats->timeAnnouncePending = true;
        _SessionUnlock(inSession);
        break;

    case kRTCPTypePTPTimeAnnounce:

        // Since the Sender's transmission rate is at PTP rate and recievers render rate is also PTP rate
        // We need only set anchor time once. We'll utilise the initial network-time to Sender RTP 2-tuple to anchor
        // playback.
        if (ats->timeAnnounceCount > 0 && ats->setRateTiming.anchor.rate != 0) {
            break;
        }

        require_action(!inSession->receiveAudioOverTCP, exit, err = kUnsupportedErr);
        require_action(len >= sizeof(pkt.ptpTimeAnnounce), exit, err = kSizeErr);

        AirTunesTime now;
        double nowTime;
        uint32_t rtpTime = ntohl(pkt.ptpTimeAnnounce.rtpTime);
        double netTime = ((double)ntoh64(pkt.ptpTimeAnnounce.ptpTime)) / NSEC_PER_SEC;
        uint64_t grandmasterID = ntoh64(pkt.ptpTimeAnnounce.ptpGrandmasterID);

        // This is a workaround to slam the achor time back if more than 2 seconds off.
        // Although this will allow for playback from legacy/ptp - it will not be in sync.
        AirTunesClock_GetSynchronizedTime(inSession->airTunesClock, &now);
        nowTime = AirTunesTime_ToFP(&now);
        if (fabs(nowTime - netTime) > 2.0) {
            atr_ulog(kLogLevelError, "%s: Possible bad anchor time - now - net:%f\n", __func__, nowTime - netTime);
            netTime = nowTime;
        }

        AirPlayAnchor anchor;
        anchor.rate = 1;
        anchor.rtpTime = rtpTime;
        AirTunesTime_FromFP(&anchor.netTime, netTime);
        anchor.netTime.timelineID = grandmasterID;
        OSStatus anchorErr = AirPlayReceiverSessionSetRateAndAnchorTime(inSession, &anchor);

        // If we can't match the grandmaster then wait for the next message.
        if (!anchorErr) {
            _SessionLock(inSession);
            ats->timeAnnounceV_P_M = pkt.ptpTimeAnnounce.v_p_m;
            ats->timeAnnounceRTPTime = rtpTime;
            ats->timeAnnounceNetworkTimeFP = netTime;
            ats->timeAnnounceRTPApply = ntohl(pkt.ptpTimeAnnounce.rtpApply);
            ats->timeAnnouncePTPGrandmasterID = grandmasterID;
            ats->timeAnnouncePending = true;
            _SessionUnlock(inSession);
        }
        break;
#endif
    case kRTCPTypeRetransmitResponse:
        err = _RetransmitsProcessResponse(inSession, &pkt.retransmitAck, len);
        break;

    default:
        dlogassert("Unsupported packet type: %d (%zu bytes)", pkt.header.pt, len);
        err = kUnsupportedErr;
        goto exit;
    }

exit:
    return (err);
}

static void _announcePlaying(AirPlayReceiverSessionRef inSession, Boolean isPlaying)
{
    require_quiet(!inSession->isRemoteControlSession, exit);

    CFNumberRef isPlayingRef = CFNumberCreateInt64(isPlaying ? 1 : 0);
    require(isPlayingRef, exit);

    (void)AirPlayReceiverSessionSetProperty(inSession, kCFObjectFlagDirect, CFSTR(kAirPlayClientProperty_Playing), NULL, isPlayingRef);
    CFRelease(isPlayingRef);

exit:
    return;
}

#if (AIRPLAY_BUFFERED)

static OSStatus _bufferedMachineRead(NetSocketRef inSock, BufferedAudioMachine* bufferedMachine)
{
    size_t bytesRead = 0;
    OSStatus err = kNoErr;

    if (bufferedMachine->bytesToRead > 0) {
        err = NetSocket_Read(inSock, 1, bufferedMachine->bytesToRead, bufferedMachine->readBuffer, &bytesRead, 0);
        bufferedMachine->bytesToRead -= bytesRead;
        bufferedMachine->readBuffer += bytesRead;
    }

    return err;
}

static void _bufferedMachineReset(BufferedAudioMachine* bufferedMachine)
{
    bufferedMachine->state = kBufferedAudioState_ReadingPacketLength;
    bufferedMachine->node = NULL;
    bufferedMachine->packetLength = 0;
    bufferedMachine->readBuffer = (uint8_t*)&bufferedMachine->packetLength;
    bufferedMachine->bytesToRead = sizeof(bufferedMachine->packetLength);
}

static OSStatus _bufGetFreeNode(AirPlayReceiverSessionRef inSession, BufferedAudioMachine* bufferedMachine)
{
    OSStatus err = kNoErr;
    AirTunesBufferNode* node;

    _SessionLock(inSession);
    {
        node = allocator_getNewNode(inSession, bufferedMachine->packetLength);
        if (node) {
            bufferedMachine->node = node;

            // Set up the machine to read the packet from the stream
            bufferedMachine->state = kBufferedAudioState_ReadingPacket;
            bufferedMachine->readBuffer = bufferedMachine->node->data;
            bufferedMachine->bytesToRead = bufferedMachine->packetLength;
        } else {
            err = kNoMemoryErr;
        }
    }
    _SessionUnlock(inSession);

    return err;
}

static OSStatus _bufReadPacketLength(AirPlayReceiverSessionRef inSession, NetSocketRef dataNetSock, BufferedAudioMachine* bufferedMachine)
{
    OSStatus err = kNoErr;

    // Read all, part, or none of the packet length
    err = _bufferedMachineRead(dataNetSock, bufferedMachine);

    // When we've finished reading the packet length then move to the next state.
    if (bufferedMachine->bytesToRead == 0) {

        // Adjust the packet length to be the length of the RTP audio data.
        bufferedMachine->packetLength = ntohs(bufferedMachine->packetLength);
        bufferedMachine->packetLength -= sizeof(bufferedMachine->packetLength);

        // If we read a bad packet length then we're in trouble.
        // We are likely out of sync with the audio data stream.
        if (bufferedMachine->packetLength > inSession->nodeBufferSize) {
            bufferedMachine->packetLength = inSession->nodeBufferSize;
            err = kSizeErr;
        }

        // Set up the machine to allocate a buffer big enough for the packet
        bufferedMachine->state = kBufferedAudioState_GetFreeNode;
        bufferedMachine->readBuffer = NULL;
        bufferedMachine->bytesToRead = 0;
    }

    if (err != kNoErr && err != kTimeoutErr) {
        atr_ulog(kLogLevelWarning, "%s: error reading packet length = %d\n", __func__, err);
    }

    return err;
}

static OSStatus _bufReadPacket(NetSocketRef dataNetSock, BufferedAudioMachine* bufferedMachine)
{
    OSStatus err = kNoErr;

    // Read all, part, or none of the RTP audio packet
    err = _bufferedMachineRead(dataNetSock, bufferedMachine);

    // When we've finished reading the audio packet then go to the next state.
    if (bufferedMachine->bytesToRead == 0) {
        bufferedMachine->state = kBufferedAudioState_ProcessPacket;
    }

    if (err != kNoErr && err != kTimeoutErr) {
        atr_ulog(kLogLevelWarning, "%s  error reading packet = %d\n", __func__, err);
    }

    return err;
}

static OSStatus _bufProcessPacket(AirPlayReceiverSessionRef inSession, BufferedAudioMachine* bufferedMachine)
{
    OSStatus err = kNoErr;

    _SessionLock(inSession);
    {
        err = _GeneralAudioProcessPacket(inSession, bufferedMachine->node, bufferedMachine->packetLength, false);

        // If there was an error processing the packet then put it back on the free list
        if (err) {
            allocator_freeNode(inSession, bufferedMachine->node);
        }
    }
    _SessionUnlock(inSession);

    // Back to the first state
    _bufferedMachineReset(bufferedMachine);

    if (err && err != kSkipErr)
        atr_ulog(kLogLevelWarning, "%s  err = %d\n", __func__, err);

    return err;
}

#endif

#if (AIRPLAY_BUFFERED)

static OSStatus _receiveBufferedRTP(AirPlayReceiverSessionRef inSession, NetSocketRef dataNetSock)
{
    static const int kMaxProcessedStatesCount = 20; // Don't starve the caller when we're busy.
    int processedStatesCount = 0;
    OSStatus err = kNoErr;

    do {
        uint8_t prev_state = inSession->buffered.state;
        switch (inSession->buffered.state) {
        case kBufferedAudioState_GetFreeNode:
            err = _bufGetFreeNode(inSession, &inSession->buffered);
            break;

        case kBufferedAudioState_ReadingPacketLength:
            err = _bufReadPacketLength(inSession, dataNetSock, &inSession->buffered);
            break;

        case kBufferedAudioState_ReadingPacket:
            err = _bufReadPacket(dataNetSock, &inSession->buffered);
            break;

        case kBufferedAudioState_ProcessPacket:
            err = _bufProcessPacket(inSession, &inSession->buffered);
            break;

        default:
            atr_ulog(kLogLevelAssert, "%s unknown buffered machine state = %d\n", __func__, inSession->buffered.state);
            _bufferedMachineReset(&inSession->buffered);
        }

        if (err == kNoErr && inSession->buffered.state == prev_state) {
            break;
        }
    } while (!err && ++processedStatesCount < kMaxProcessedStatesCount);

    return err;
}
#endif

//===========================================================================================================================
//	_GeneralAudioReceiveRTP
//===========================================================================================================================
static OSStatus _GeneralAudioReceiveRTP(AirPlayReceiverSessionRef inSession, RTPPacket* inPkt, size_t inSize)
{
    AirPlayAudioStreamContext* const ctx = &inSession->mainAudioCtx;
    OSStatus err;
    AirTunesBufferNode* node;
    AirTunesBufferNode* stop;
    size_t len = 0;

    // Get a free node.
    _SessionLock(inSession);
    node = allocator_getNewNode(inSession, inSession->nodeBufferSize);

    if (!node) {
        // The buffers are full for legacy AirPlay, steal the oldest busy node.
        stop = inSession->busyListSentinel;
        node = stop->next;
        if (node != stop) {
            node->next->prev = node->prev;
            node->prev->next = node->next;
            --inSession->busyNodeCount;

            atr_stats_ulog(kLogLevelVerbose, "### No free buffer nodes. Stealing oldest busy node.\n");

        } else {
            node = NULL;

            dlogassert("No buffer nodes at all? Probably a bug in the code\n");
            usleep(100000); // Sleep for a moment to avoid hogging the CPU on continual failures.
            err = kNoResourcesErr;
            goto exit;
        }
    }

    // Get the packet. If a packet was passed in then use it directly. Otherwise, read it from the socket.

    if (inPkt) {
        require_action(inSize <= inSession->nodeBufferSize, exit, err = kSizeErr);
        memcpy(node->data, inPkt, inSize);
        len = inSize;
    } else {
        err = SocketRecvFrom(ctx->dataSock, node->data, inSession->nodeBufferSize, &len, NULL, 0, NULL, NULL, NULL, NULL);
        if (err == EWOULDBLOCK)
            goto exit;
        require_noerr(err, exit);
    }

    // Process the packet. Warning: this function MUST either queue the node onto the busy queue and return kNoErr or
    // it MUST return an error and NOT queue the packet. Doing anything else will lead to leaks and/or crashes.

    err = _GeneralAudioProcessPacket(inSession, node, len, inPkt != NULL);
    require_noerr_quiet(err, exit);
    node = NULL;

exit:
    if (node) {
        // Processing the packet failed so put the node back on the free list.
        allocator_freeNode(inSession, node);
    }
    _SessionUnlock(inSession);
    return (err);
}

//===========================================================================================================================
//	_GeneralAudioProcessPacket
//
//	Warning: Assumes the AirTunes lock is held.
//
//	Warning: this function MUST either queue the node onto the busy queue and return kNoErr or it MUST return an error
//	and NOT queue the packet. Doing anything else will lead to leaks and/or crashes.
//
//	Note: The "buf" field of "inNode" is expected to be filled in, but all other fields are filled in by this function.
//===========================================================================================================================

static OSStatus
_GeneralAudioProcessPacket(
    AirPlayReceiverSessionRef inSession,
    AirTunesBufferNode* inNode,
    size_t inSize,
    Boolean inIsRetransmit)
{
    OSStatus err;
    RTPPacket* rtp;
    uint32_t pktSeq;
    uint32_t pktTS;
    AirTunesBufferNode* curr;
    AirTunesBufferNode* stop;

    // Validate and parse the packet.

    if (inSize < kRTPHeaderSize) {
        dlogassert("Packet too small: %zu bytes", inSize);
        err = kSizeErr;
        goto exit;
    }
    rtp = (RTPPacket*)inNode->data;
    rtp->header.seq = ntohs(rtp->header.seq);
    rtp->header.ts = pktTS = ntohl(rtp->header.ts);
    rtp->header.ssrc = ntohl(rtp->header.ssrc);
    inNode->rtp = rtp;
    inNode->ptr = inNode->data + kRTPHeaderSize;
    inNode->size = inSize - kRTPHeaderSize;
    inNode->ts = pktTS;
    inNode->nodeId = inSession->nodeId++;

    // For Realtime AirPlay we use 16 bit RTP sequence numbers.
    // For Buffered AirPlay we use 24 bit sequence numbers where the
    // third byte lives in the otherwise unused Marker and Payload Type field.
    inNode->seq = rtp->header.seq;
    if (inSession->receiveAudioOverTCP) {
        inNode->seq += (uint32_t)rtp->header.m_pt << 16;

        // For Buffered AirPlay we expect the sequence numbers to come in order.
        // If they don't then log a warning.
        uint32_t expectedSeq = Seq_Inc(inSession, inSession->lastNetSeq);
        if (inSession->lastNetSeq > 0 && inNode->seq != expectedSeq) {
            long errDelta = (long)expectedSeq - inNode->seq;
            atr_ulog(kLogLevelWarning, "%s: unexpected Seq=%u(0x%x). Expected %u(0x%x) Delta %ld(0x%lx)\n", __func__, inNode->seq, inNode->seq, expectedSeq, expectedSeq, errDelta, errDelta);
        }
    }

    inSession->lastNetTS = pktTS;
    inSession->lastNetSeq = inNode->seq;

    pktSeq = inNode->seq;

    if (!inSession->receiveAudioOverTCP) {
        if (_GeneralAudioTrackDups(inSession, (uint16_t)pktSeq)) {
            err = kDuplicateErr;
            goto exit;
        }
    }

    if (!inIsRetransmit)
        _GeneralAudioTrackLosses(inSession, inNode);

    if (!inSession->receiveAudioOverTCP) {
        if (!inSession->source.rtcpRTDisable)
            _RetransmitsUpdate(inSession, inNode, inIsRetransmit);
    }

    if (!inIsRetransmit)
        increment_wrap(inSession->source.receiveCount, 1);

    // If we're flushing then drop packets with timestamps before the end of the flush. Because of retranmissions
    // and out-of-order packets, we may receive pre-flush packets *after* post-flush packets. We want to drop all
    // pre-flush packets, but keep post-flush packets so we keep flush mode active until we hit the flush timeout.

    if (inSession->flushing) {

        if (Seq_GE(inSession, pktSeq, inSession->flushUntil.seq)) {
            atr_ulog(kLogLevelNotice, "ProcessPkt Flush Complete flushSeq %u flushTS %u count %u Seq # %u TS %u\n",
                inSession->flushUntil.seq, inSession->flushUntil.ts, (inSession->source.receiveCount - 1), pktSeq, pktTS);
            inSession->flushing = false;
            inSession->source.receiveCount = 1;
        } else {
            if (inSession->flushingWithinRange && Seq_LT(inSession, pktSeq, inSession->flushFrom.seq)) {
                // Accept all packets that have sequence numbers less than the flush starting sequence number.

                //check( Mod32_LT( pktTS, inSession->flushFrom.ts ) );
            } else if (Seq_LT(inSession, pktSeq, inSession->flushUntil.seq)) {
                //check( Mod32_LT( pktTS, inSession->flushUntil.ts ) );

                atr_ulog(kLogLevelTrace, "ProcessPkt Flush pkt flushSeq %u flushTS %u count %u Seq # %u TS %u\n",
                    inSession->flushUntil.seq, inSession->flushUntil.ts, inSession->source.receiveCount, pktSeq, pktTS);
                err = kSkipErr;
                goto exit;
            } else {
                atr_ulog(kLogLevelError, "ProcessPkt Flush unexpected packet info flushSeq %u flushTS %u count %u Seq # %u TS %u\n",
                    inSession->flushUntil.seq, inSession->flushUntil.ts, inSession->source.receiveCount, pktSeq, pktTS);
                err = kUnknownErr;
                goto exit;
            }
        }
    }

    // Insert the new node. Start at the end since it'll usually be at or near the end. Drop if time slot already taken.

    stop = inSession->busyListSentinel;

    // The packets come in order for buffered AirPlay,
    // No need to search the packet list for the proper place.
    if (inSession->receiveAudioOverTCP) {
        curr = stop->prev;

    } else {
        for (curr = stop->prev; (curr != stop) && Mod32_GT(curr->rtp->header.ts, pktTS); curr = curr->prev) {
        }
        if ((curr != stop) && (curr->rtp->header.ts == pktTS)) {
            dlogassert("Duplicate timestamp not caught earlier? seq %u ts %u", pktSeq, pktTS);
            err = kDuplicateErr;
            goto exit;
        }
    }
    inNode->prev = curr;
    inNode->next = curr->next;
    inNode->next->prev = inNode;
    inNode->prev->next = inNode;
    ++inSession->busyNodeCount;
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	_GeneralAudioGenerateAADForPacket
//===========================================================================================================================

static OSStatus
_GeneralAudioGenerateAADForPacket(
    const RTPPacket* inRTPPacket,
    uint8_t* aad,
    size_t inAADSize)
{
    uint32_t tsNetworkOrder = htonl(inRTPPacket->header.ts);
    uint32_t ssrcNetworkOrder = htonl(inRTPPacket->header.ssrc);

    check(inAADSize == sizeof(inRTPPacket->header.ts) + sizeof(inRTPPacket->header.ssrc));
    (void)inAADSize;

    memcpy(aad, &tsNetworkOrder, sizeof(tsNetworkOrder));
    memcpy(&aad[sizeof(tsNetworkOrder)], &ssrcNetworkOrder, sizeof(ssrcNetworkOrder));

    return (kNoErr);
}

//===========================================================================================================================
//	_GeneralAudioDecryptPacket
//===========================================================================================================================

static OSStatus
_GeneralAudioDecryptPacket(
    AirPlayReceiverSessionRef inSession,
    uint8_t* inAAD,
    size_t inAADSize,
    uint8_t* inSrc,
    size_t inSrcSize,
    uint8_t* inDst,
    size_t* outSize)
{
    OSStatus err;
    size_t len = 0;
    AirPlayAudioStreamContext* const ctx = &inSession->mainAudioCtx;

    if (ctx->outputCryptor.isValid) {
        // The last 24 bytes of the payload are nonce and the auth tag. Both are LE. The rest of the payload is BE.

        chacha20_poly1305_init_64x64(&ctx->outputCryptor.state, ctx->outputCryptor.key, &inSrc[inSrcSize - 8]);
        chacha20_poly1305_add_aad(&ctx->outputCryptor.state, inAAD, inAADSize);
        len = chacha20_poly1305_decrypt(&ctx->outputCryptor.state, inSrc, inSrcSize - 24, inDst);
        len += chacha20_poly1305_verify(&ctx->outputCryptor.state, &inDst[len], &inSrc[inSrcSize - 24], &err);
        require_noerr(err, exit);
        require_action(len == inSrcSize - 24, exit, err = kInternalErr);

        // Increment nonce - for debugging only

        LittleEndianIntegerIncrement(ctx->outputCryptor.nonce, sizeof(ctx->outputCryptor.nonce));
    } else {
        err = AES_CBCFrame_Update(inSession->decryptor, inSrc, inSrcSize, inDst);
        require_noerr(err, exit);
        len = inSrcSize;
    }

    *outSize = len;
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	_GeneralAudioDecodePacket
//
//	Warning: Assumes the AirTunes lock is held.
//===========================================================================================================================

static OSStatus
_GeneralAudioDecodePacket(
    AirPlayReceiverSessionRef inSession,
    uint8_t* inAAD,
    size_t inAADSize,
    uint8_t* inSrc,
    size_t inSrcSize,
    uint8_t* inDst,
    size_t inDstSize,
    size_t* outSize)
{
    OSStatus err;
    Boolean decrypt;
    int decompress;
    size_t size;
    uint32_t percent;

    // inSrc may be the same as inDst, but otherwise they cannot overlap. Neither can overlap the temp buffer.

    if (inSrc != inDst)
        check_ptr_overlap(inSrc, inSrcSize, inDst, inDstSize);
    check_ptr_overlap(inSrc, inSrcSize, inSession->decodeBuffer, inSession->decodeBufferSize);
    check_ptr_overlap(inDst, inDstSize, inSession->decodeBuffer, inSession->decodeBufferSize);

    decrypt = (inSession->decryptor != NULL);
    decrypt = (decrypt || inSession->mainAudioCtx.outputCryptor.isValid);
    decompress = (inSession->compressionType != kAirPlayCompressionType_PCM);
    if (decrypt && decompress) // Encryption and Compression (most common case)
    {
        // Decrypt into a temp buffer then decompress back into the node buffer. Note: decoder result is in host byte order.

        require_action(inSrcSize <= inSession->decodeBufferSize, exit, err = kSizeErr);

        err = _GeneralAudioDecryptPacket(
            inSession,
            inAAD,
            inAADSize,
            inSrc,
            inSrcSize,
            inSession->decodeBuffer,
            &size);
        require_noerr(err, exit);
        inSrcSize = size;

        size = 0;
        err = _AudioDecoderDecodeFrame(inSession, inSession->decodeBuffer, inSrcSize, inDst, inDstSize, &size);
        require_noerr(err, exit);
    } else if (decrypt) // Encryption only
    {
        // Decrypt into a temp buffer then swap to host byte order back into the node buffer.

        require_action(inSrcSize <= inSession->decodeBufferSize, exit, err = kSizeErr);

        err = _GeneralAudioDecryptPacket(
            inSession,
            inAAD,
            inAADSize,
            inSrc,
            inSrcSize,
            inSession->decodeBuffer,
            &size);
        require_noerr(err, exit);

        BigToHost16Mem(inSession->decodeBuffer, size, inDst);
    } else if (decompress) // Compression only
    {
        // Decompress into a temp buffer then copy back into the node buffer. Note: decoder result is in host byte order.
        // This does an extra memcpy due to the lack of in-place decoding, but this case is not normally used.

        size = 0;
        err = _AudioDecoderDecodeFrame(inSession, inSrc, inSrcSize, inSession->decodeBuffer,
            inSession->decodeBufferSize, &size);
        require_noerr(err, exit);

        require_action(size <= inDstSize, exit, err = kSizeErr);
        memcpy(inDst, inSession->decodeBuffer, size);
    } else // No Encryption or Compression (raw data)
    {
        require_action(inSrcSize <= inDstSize, exit, err = kSizeErr);

        BigToHost16Mem(inSrc, inSrcSize, inDst);
        size = inSrcSize;
    }

    // Track the amount of compression we're getting.

    percent = (size > 0) ? ((uint32_t)((inSrcSize * (100 * 100)) / size)) : 0;
    inSession->compressionPercentAvg = ((inSession->compressionPercentAvg * 63) + percent) / 64;

    *outSize = size;
    err = kNoErr;

exit:
    return (err);
}

#if (AIRPLAY_GENERAL_AUDIO_SKEW)
//===========================================================================================================================
//	audioSession_adjustForSkew
//
//	Warning: Assumes the AirTunes lock is held.
//===========================================================================================================================

static void _GeneralAdjustForSkew(AirPlayReceiverSessionRef inSession, AirTunesBufferNode* inNode)
{
    AirPlayAudioStreamContext* const ctx = &inSession->mainAudioCtx;
    AirTunesSource* const source = &inSession->source;
    uint8_t* const buf = inSession->skewAdjustBuffer;

    DEBUG_USE_ONLY(inSession);
    check_string(IsAligned(((uintptr_t)inNode->ptr), 4), "packet data ptr not 4-byte aligned...may crash");
    check_string((inNode->size % ctx->bytesPerUnit) == 0, "size not a multiple of 4...must be 16-bit, 2 channel");

#if (AIRPLAY_RAMSTAD_SRC)

    // compute rate of input samples to output samples.
    double sampleRate = (double)ctx->sampleRate;

    double rate = 1. + (.001 * source->rtpSkewAdjust) / sampleRate; // 1000 seconds to consume all skew. That is what the add/drop code appears to be doing.
#define CLAMP(X, LO, HI) MAX(LO, MIN(HI, X))
    rate = CLAMP(rate, .99, 1.01); // equivalent to (44100 +/- kAirTunesMaxSkewAdjustRate) / 44100.
    CRamstadASRC_setRate(&source->resampler, rate); // set the resampler rate

    // calculate the number of output samples that can be produced from the input.
    int numInputSamples = (int)(inNode->size / ctx->bytesPerUnit);
    int numOutputSamples = CRamstadASRC_outputSamplesForInputSamples(&source->resampler, numInputSamples, NULL);

    // copy input to temp buffer.
    check(inNode->size <= (inSession->nodeBufferSize - kRTPHeaderSize));
    check(inNode->size <= inSession->skewAdjustBufferSize);
    check(ctx->bytesPerUnit == 4); // two channels of int16, interleaved.
    memcpy(buf, inNode->ptr, inNode->size);

    // get pointers to the interleaved channels.
    const int16_t* inL = (const int16_t*)buf;
    const int16_t* inR = inL + 1;
    int16_t* outL = (int16_t*)inNode->ptr;
    int16_t* outR = outL + 1;

    // do the resampling.
    CRamstadASRC_processStereoInt16(&source->resampler, inL, inR, outL, outR, numInputSamples, numOutputSamples, 2, 2);

    // update times.
    int bytesCopied = numOutputSamples * ctx->bytesPerUnit;
    int deltaSamples = numOutputSamples - numInputSamples;
    inNode->size = bytesCopied;
    // inNode->ts is supposed to be the time of the start of the buffer, so it seems wrong to adjust inNode->ts by the amount of stretch after the start of the buffer, but that is what the add/drop code appears to be doing. The more correct thing, it seems to me, would be to adjust the time of the next buffer time by the stretch of this one.
    inNode->ts -= deltaSamples;
    source->rtpOffset += deltaSamples;
    source->rtpOffsetActive += deltaSamples;

//printf("X rtpSkewAdjust %f  rate %f  deltaTime %d  rtpOffset %d   n %d\n", source->rtpSkewAdjust, rate, deltaSamples, (int)source->rtpOffset, numOutputSamples);

#else // AIRPLAY_RAMSTAD_SRC. use add/drop
    const uint32_t* src; // Note: 32-bit ptrs used to copy 2, 16-bit channels in one shot.
    const uint32_t* end;
    uint32_t* dst;
    size_t deltaBytes;
    uint32_t deltaTime;
    uint32_t tmp;

    if (source->rtpSkewAdjust > 0) // Source is faster than us so drop samples to speed us up.
    {
        // Note: This drops every Nth sample so src stays at or ahead of dst and they don't collide.

        src = (const uint32_t*)inNode->ptr;
        end = src + (inNode->size / ctx->bytesPerUnit);
        dst = (uint32_t*)inNode->ptr;
        for (; src < end; ++src) {
            if (++source->rtpSkewAdjustIndex >= source->rtpSkewSamplesPerAdjust) {
                source->rtpSkewAdjustIndex = 0;
            } else {
                *dst++ = *src;
            }
        }
        deltaBytes = inNode->size - ((size_t)(((uintptr_t)dst) - ((uintptr_t)inNode->ptr)));
        deltaTime = (uint32_t)(deltaBytes / ctx->bytesPerUnit);

        inNode->size -= deltaBytes;
        inNode->ts += deltaTime;
        source->rtpOffset -= deltaTime;
        source->rtpOffsetActive -= deltaTime;
    } else if (source->rtpSkewAdjust < 0) // Source is slower than us so insert samples to slow us down.
    {
        // Copy into a temporary buffer since we can't expand it in-place.
        // $$$ TO DO: Copy it backward so it can be expanded in-place.

        check(inNode->size <= (inSession->nodeBufferSize - kRTPHeaderSize));
        check(inNode->size <= inSession->skewAdjustBufferSize);
        memcpy(buf, inNode->ptr, inNode->size);

        // Copy from the temporary buffer back to the node and insert an extra sample every Nth sample.

        src = (const uint32_t*)buf;
        end = src + (inNode->size / ctx->bytesPerUnit);
        dst = (uint32_t*)inNode->ptr;
        for (; src < end; ++src) {
            tmp = *src;
            if (++source->rtpSkewAdjustIndex >= source->rtpSkewSamplesPerAdjust) {
                source->rtpSkewAdjustIndex = 0;
                *dst++ = tmp;
            }
            *dst++ = tmp;
        }
        check(((size_t)(((uintptr_t)dst) - ((uintptr_t)inSession->audioPacketBuffer))) <= inSession->audioPacketBufferSize);

        deltaBytes = ((size_t)(((uintptr_t)dst) - ((uintptr_t)inNode->ptr))) - inNode->size;
        deltaTime = (uint32_t)(deltaBytes / ctx->bytesPerUnit);

        inNode->size += deltaBytes;
        inNode->ts -= deltaTime;
        source->rtpOffset += deltaTime;
        source->rtpOffsetActive += deltaTime;
    }
#endif // AIRPLAY_RAMSTAD_SRC
}
#endif // AIRPLAY_GENERAL_AUDIO_SKEW

//===========================================================================================================================
//	_GeneralAudioRender
//
//	Warning: Assumes the AirTunes lock is held.
//===========================================================================================================================

static size_t _GeneralAudioRender(AirPlayReceiverSessionRef inSession, uint32_t inRTPTime, void* inBuffer, size_t inSize, size_t inMinFillSize)
{
    AirPlayAudioStreamContext* const ctx = &inSession->mainAudioCtx;
    AirTunesSource* const src = &inSession->source;
    uint32_t const bytesPerUnit = ctx->bytesPerUnit;
    uint32_t nowTS, limTS, minTS, maxTS, srcTS, endTS, pktTS, delta;
    uint32_t pktSeq, pktGap;
    Boolean some;
    uint8_t* dst;
    uint8_t* lim;
    int glitchCount;
    AirTunesBufferNode* curr;
    AirTunesBufferNode* stop;
    AirTunesBufferNode* next;
    int cap;
    size_t size;
    size_t suppliedSize = 0;
    OSStatus err;
#if (DEBUG)
    uint8_t* minLim;
#endif

    nowTS = inRTPTime - ctx->sampleTimeOffset;
    limTS = nowTS + (uint32_t)(inSize / bytesPerUnit);
    minTS = nowTS + (uint32_t)(inMinFillSize / bytesPerUnit);
    maxTS = limTS + kAirTunesRTPOffsetApplyThreshold;
    pktSeq = 0;
    some = false;
    dst = (uint8_t*)inBuffer;
    lim = dst + inSize;
    glitchCount = 0;

#if (DEBUG)
    minLim = dst + inMinFillSize;
#endif

#if (AIRPLAY_GENERAL_AUDIO_SKEW)
    if (src->usingAnchorTime) {
        if (src->setRateTiming.anchor.rate == 0)
            goto exit;
    } else {
        if (inSession->source.timeAnnounceCount == 0)
            goto exit;
    }
#endif

    // Process all applicable packets for this timing window.

    stop = inSession->busyListSentinel;

    // If we have any queued nodes then we always have
    // to return some bytes in order to properly fill
    // with silence.

    if (stop != stop->next && inMinFillSize == 0) {
        check(inSize >= bytesPerUnit);
        inMinFillSize = bytesPerUnit;
        minTS = nowTS + (uint32_t)(inMinFillSize / bytesPerUnit);
    }

    for (curr = stop->next; curr != stop; curr = next) {
        next = curr->next;

        // Reap zombie nodes.

        if (curr->size == 0) {
            allocator_freeNode(inSession, curr);
            inSession->zombieNodeCount--;
            continue;
        }

        // Apply the new RTP offset if we have a pending reset and we've reached the time when we should apply it.

        pktTS = curr->ts;
        if (src->rtpOffsetApply && Mod32_GE(pktTS, src->rtpOffsetApplyTimestamp)) {
            atr_ulog(kLogLevelNotice, "Applying RTP offset %u (was=%u, ts=%u, apply=%u)\n",
                src->rtpOffset, src->rtpOffsetActive, pktTS, src->rtpOffsetApplyTimestamp);

            src->rtpOffsetActive = src->rtpOffset;
            src->rtpOffsetApply = false;
            inSession->stutterCreditPending = true;
        }

        // Calculate the desired playout time of this packet and exit if we haven't reached its play time yet.
        // Packets are sorted by timestamp so if this packet isn't ready yet, none after it can be either.
        // If the apply time is way in the future, apply immediately since there was probably a timeline reset.

        srcTS = pktTS + src->rtpOffsetActive + inSession->audioLatencyOffset;
// srcTS = nowTS;
#if (AIRPLAY_METRICS)
        inSession->lastPacketTS = pktTS;
        inSession->lastRenderPacketTS = srcTS;
        inSession->lastRenderGapStartTS = nowTS;
        inSession->lastRenderGapLimitTS = limTS;
#endif

        if (src->rtpOffsetApply && Mod32_GT(srcTS, maxTS)) {
            atr_ulog(kLogLevelNotice, "Force apply RTP offset (srcTS=%u maxTS=%u)\n", srcTS, maxTS);

            src->rtpOffsetActive = src->rtpOffset;
            src->rtpOffsetApply = false;
            inSession->stutterCreditPending = true;

            srcTS = pktTS + src->rtpOffsetActive + inSession->audioLatencyOffset;
        }
        if (Mod32_GE(srcTS, limTS))
            break;
        some = true;

        // Track packet losses.

        pktSeq = curr->seq;

        uint32_t deltaSeq = Seq_Diff(inSession, pktSeq, inSession->lastPlayedSeq);
        if (inSession->lastPlayedValid && (deltaSeq > 1)) {
            if (inSession->receiveAudioOverTCP) {
                int32_t expectedSeq = Seq_Inc(inSession, inSession->lastPlayedSeq);
                long errDelta = (long)expectedSeq - pktSeq;
                atr_ulog(kLogLevelWarning, "%s: unexpected Seq=%u(0x%x). Expected %u(0x%x) Delta=%ld(0x%lx)\n", __func__, pktSeq, pktSeq, expectedSeq, expectedSeq, errDelta, errDelta);
            }

            pktGap = deltaSeq - 1;
            gAirPlayAudioStats.unrecoveredPackets += pktGap;
            atr_stats_ulog(kLogLevelEmergency, "### Unrecovered packets: %u-%u (%u) %u total\n",
                inSession->lastPlayedSeq + 1, pktSeq, pktGap, gAirPlayAudioStats.unrecoveredPackets); //notice->emerg.
        }
        inSession->lastPlayedTS = pktTS;
        inSession->lastPlayedSeq = pktSeq;
        inSession->lastPlayedValid = true;

        // Decrypt and decompress the packet (if we haven't already on a previous pass).

        if (curr->ptr == curr->rtp->payload) {
            static const size_t aadSize = sizeof(curr->rtp->header.ts) + sizeof(curr->rtp->header.ssrc);
            uint8_t aad[aadSize];

            err = _GeneralAudioGenerateAADForPacket(curr->rtp, aad, aadSize);
            require_noerr(err, exit);

            err = _GeneralAudioDecodePacket(inSession, aad, aadSize, curr->ptr, curr->size, inSession->audioPacketBuffer,
                inSession->audioPacketBufferSize, &curr->size);
            curr->ptr = inSession->audioPacketBuffer;

            if (err || (curr->size == 0)) {
                allocator_freeNode(inSession, curr);
                continue;
            }
        }

        // If the packet is too old, free it and move to the next packet.

        endTS = srcTS + (uint32_t)(curr->size / bytesPerUnit);
        if (Mod32_LE(endTS, nowTS)) {
            gAirPlayAudioStats.latePackets += 1;
            atr_stats_ulog(kLogLevelNotice, "Discarding late packet: seq %u ts %u-%u (%u ms), %u total\n",
                pktSeq, nowTS, srcTS, AirTunesSamplesToMs(nowTS - srcTS), gAirPlayAudioStats.latePackets);

            _RetransmitsAbortOne(inSession, pktSeq, "OLD");
            allocator_freeNode(inSession, curr);
            continue;
        }

        // This packet has at least some samples within the timing window. If the packet starts after the current
        // time then there's a gap (e.g. packet loss) so we need to conceal the gap with simulated data.

        if (Mod32_LT(nowTS, srcTS)) {
            delta = srcTS - nowTS;
            atr_ulog(kLogLevelNotice, "Concealed %d unit gap (%u vs %u), curr seq %u\n", delta, nowTS, srcTS, pktSeq);
            _RetransmitsAbortOne(inSession, pktSeq, "GAP");
            size = delta * bytesPerUnit;
            check_ptr_bounds(inBuffer, inSize, dst, size);
            _AudioDecoderConcealLoss(inSession, dst, size);
            dst += size;
            nowTS += delta;
            ++glitchCount;
        }

        // If the packet has samples before the timing window then skip those samples because they're too late.

        if (Mod32_LT(srcTS, nowTS)) {
            gAirPlayAudioStats.latePackets += 1;
            atr_stats_ulog(kLogLevelEmergency, "Dropped %d late units (%u vs %u), %u total\n",
                Mod32_Diff(nowTS, srcTS), srcTS, nowTS, gAirPlayAudioStats.latePackets); //verbose->emerg

            _RetransmitsAbortOne(inSession, pktSeq, "LATE");
            delta = nowTS - srcTS;
            size = delta * bytesPerUnit;
            curr->ptr += size;
            curr->size -= size;
            curr->ts += delta;
            srcTS = nowTS;
            ++glitchCount;
        }

        if (gAirplayPrivCtrlCtx.RequestSkipSkew)
        {
            if (gAirplayPrivCtrlCtx.TsSinceAudioStarted == 0)
            {
                gAirplayPrivCtrlCtx.TsSinceAudioStarted = UpTicks();
            }
            else if (UpTicks() - gAirplayPrivCtrlCtx.TsSinceAudioStarted >= kAirTunesHoldSkewMaxDurationNs)
            {
                gAirplayPrivCtrlCtx.RequestSkipSkew = 0;
            }
        }

        if(!gAirplayPrivCtrlCtx.RequestSkipSkew){
#if (AIRPLAY_GENERAL_AUDIO_SKEW)\
            // Perform any skew compensation necessary to keep us from getting too far ahead of or behind the source.
            // Note: if the node doesn't point to the beginning of the payload, we've already skew adjusted this packet.
            // Note: this adjusts this packet's data and timestamp and the overall RTP offset for the next time around.
            // Note: when the platform supports skew compensation, rtpSkewAdjusting is always false so we don't do it.
            if (inSession->source.rtpSkewAdjusting && (curr->ptr == curr->rtp->payload || curr->ptr == inSession->audioPacketBuffer)) {
                _GeneralAdjustForSkew(inSession, curr);
                endTS = srcTS + (uint32_t)(curr->size / bytesPerUnit);
            }
#endif // AIRPLAY_GENERAL_AUDIO_SKEW
        }
        
        // Copy the packet data into the playout buffer. If all of the data doesn't fit in our buffer then only
        // copy as much as we can and adjust the packet (pointers, sizes, timestamps) so we process the rest of
        // it next time. If we processed the whole packet then move it to the free list so it can be reused.

        cap = Mod32_GT(endTS, limTS);
        if (cap)
            endTS = limTS;

        delta = endTS - srcTS;
        size = delta * bytesPerUnit;
        check_ptr_bounds(inSession->audioPacketBuffer, inSession->audioPacketBufferSize, curr->ptr, size);
        check_ptr_bounds(inBuffer, inSize, dst, size);
        memcpy(dst, curr->ptr, size);
        dst += size;
        nowTS += delta;

        if (cap) {
            curr->ptr += size;
            curr->size -= size;
            curr->ts += delta;
            check_ptr_bounds(inSession->audioPacketBuffer, inSession->audioPacketBufferSize, curr->ptr, curr->size);
            check_string(curr->size > 0, "capped packet to fit, but nothing left over?");
            check_string(dst == lim, "capped packet, but more room in buffer?");
            break;
        } else {
            allocator_freeNode(inSession, curr);
        }
        if (dst >= lim)
            break;
    }

exit:
    // If there wasn't enough data to fill the minimum fill part of the buffer then fill the remaining space with simulated data.

    if (Mod32_LT(nowTS, minTS)) {
        delta = minTS - nowTS;
        size = delta * bytesPerUnit;
        check_ptr_bounds(inBuffer, inSize, dst, size);
        _AudioDecoderConcealLoss(inSession, dst, size);
        dst += size;
        nowTS += delta;
        ++glitchCount;

        atr_ulog(kLogLevelTrace, "Concealed %d units at end (ts=%u)\n", delta, nowTS);

        if (some)
            _RetransmitsAbortOne(inSession, pktSeq, "UNDERRUN");
    }
    check(dst >= minLim);
    check(dst <= lim);
    check(nowTS >= minTS);
    check(nowTS <= limTS);

    suppliedSize = dst - (uint8_t*)inBuffer;
    check(suppliedSize >= inMinFillSize);
    check(suppliedSize <= inSize);

    // Update to account for glitches. This tries to be conservative and ignore glitches due to no packets,
    // flushing, or when we expect a glitch (RTP reset). If there's too many glitches, use silence.

    if (!some || inSession->flushing) {
        glitchCount = 0;
    } else if ((glitchCount > 0) && inSession->stutterCreditPending) {
        glitchCount = 0;
        inSession->stutterCreditPending = false;
    }
    inSession->glitchTotal += glitchCount;
    if (glitchCount > 0) {
        atr_ulog(kLogLevelChatty, "Glitch: %d new, %u session\n", glitchCount, inSession->glitchTotal);
    }

    // Update stats.

    if (inSession->lastPlayedValid && log_category_enabled(atr_stats_ucat(), kLogLevelNotice)) {
        double samples = (long)inSession->lastRTPTS - (long)inSession->lastPlayedTS;
        if (samples > 0) {
            EWMA_FP_Update(&gAirPlayAudioStats.bufferAvg, samples);
            atr_ulog(kLogLevelChatty, "RTP Buffer: %3d ms, %3.2f ms avg\n",
                AirTunesSamplesToMs(samples), AirTunesSamplesToMs(EWMA_FP_Get(&gAirPlayAudioStats.bufferAvg)));
        }
    }

    return suppliedSize;
}

//===========================================================================================================================
//	_GeneralAudioTrackDups
//
//	Warning: Assumes the AirTunes lock is held.
//===========================================================================================================================

static Boolean _GeneralAudioTrackDups(AirPlayReceiverSessionRef inSession, uint16_t inSeq)
{
    size_t i;
    int diff;

    if (inSession->rtpAudioDupsInitialized) {
        diff = Seq_Cmp(inSession, inSeq, inSession->rtpAudioDupsLastSeq);
        if (diff > 0) {
        } else if (diff == 0)
            goto dup;
        else if (diff <= -kAirTunesDupWindowSize)
            goto dup;

        i = inSeq % kAirTunesDupWindowSize;
        if (inSession->rtpAudioDupsArray[i] == inSeq)
            goto dup;
        inSession->rtpAudioDupsArray[i] = inSeq;
    } else {
        for (i = 0; i < kAirTunesDupWindowSize; ++i) {
            inSession->rtpAudioDupsArray[i] = inSeq;
        }
        inSession->rtpAudioDupsInitialized = true;
    }
    inSession->rtpAudioDupsLastSeq = inSeq;
    return (false);

dup:
    if (!inSession->redundantAudio) {
        atr_stats_ulog(kLogLevelInfo, "### Duplicate packet seq %u\n", inSeq);
    }
    return (true);
}

//===========================================================================================================================
//	_GeneralAudioTrackLosses
//
//	Warning: Assumes the AirTunes lock is held.
//===========================================================================================================================

static void _GeneralAudioTrackLosses(AirPlayReceiverSessionRef inSession, AirTunesBufferNode* inNode)
{
    Boolean updateLast;
    uint32_t seqCurr;
    uint32_t seqNext;
    uint32_t seqLoss;

    updateLast = true;
    seqCurr = inNode->seq;
    if (inSession->source.receiveCount > 0 && !inSession->receiveAudioOverTCP) {
        seqNext = Seq_Inc(inSession, inSession->lastRTPSeq);
        if (seqCurr == seqNext) {
            // Expected case of the receiving the next packet in the sequence so nothing special needs to be done.
        } else if (Seq_GT(inSession, seqCurr, seqNext)) {
            seqLoss = seqCurr - seqNext;
            gAirPlayAudioStats.lostPackets += seqLoss;
            if (seqLoss > inSession->source.maxBurstLoss)
                inSession->source.maxBurstLoss = seqLoss;
            if (seqLoss <= inSession->source.retransmitMaxLoss) {
                atr_stats_ulog(kLogLevelNotice, "### Lost packets %u-%u (+%u, %u total)\n",
                    seqNext, seqCurr, seqLoss, gAirPlayAudioStats.lostPackets);
                if (!inSession->source.rtcpRTDisable)
                    _RetransmitsSchedule(inSession, seqNext, seqLoss);
            } else {
                atr_ulog(kLogLevelNotice, "### Burst packet loss %u-%u (+%u, %u total)\n",
                    seqNext, seqCurr, seqLoss, gAirPlayAudioStats.lostPackets);
                ++inSession->source.bigLossCount;
                _RetransmitsAbortAll(inSession, "BURST");
            }
        } else if (seqCurr == inSession->lastRTPSeq) {
            dlogassert("Duplicate sequence number not caught earlier? seq %u", seqCurr);
            updateLast = false;
        } else if (inSession->redundantAudio) {
            updateLast = false; // Don't track misorders since they are intentional in redundant mode.
        } else {
            atr_ulog(kLogLevelNotice, "### Misordered packet seq %u -> %u\n", inSession->lastRTPSeq, seqCurr);
            updateLast = false;
        }
    }

    if (updateLast) {
        inSession->lastRTPSeq = seqCurr;
        inSession->lastRTPTS = inNode->rtp->header.ts;
    }
}

//===========================================================================================================================
//	_GeneralAudioUpdateLatency
//===========================================================================================================================

static void _GeneralAudioUpdateLatency(AirPlayReceiverSessionRef inSession)
{
    inSession->audioLatencyOffset = inSession->minLatency + gAirTunesRelativeTimeOffset;

    atr_ulog(kLogLevelVerbose, "Audio Latency Offset %u, Sender %d, Relative %d\n",
        inSession->audioLatencyOffset, inSession->minLatency, gAirTunesRelativeTimeOffset);
}

//===========================================================================================================================
//	_RetransmitsSendRequest
//
//	Warning: Assumes the AirTunes lock is held.
//===========================================================================================================================

static OSStatus _RetransmitsSendRequest(AirPlayReceiverSessionRef inSession, uint16_t inSeqStart, uint16_t inSeqCount)
{
    OSStatus err;
    RTCPRetransmitRequestPacket pkt;
    ssize_t n;
    size_t size;

    size = kRTCPRetransmitRequestPacketMinSize;
    pkt.v_p = RTCPHeaderInsertVersion(0, kRTPVersion);
    pkt.pt = kRTCPTypeRetransmitRequest;
    pkt.length = htons((uint16_t)((size / 4) - 1));
    pkt.seqStart = htons(inSeqStart);
    pkt.seqCount = htons(inSeqCount);

    if (inSession->rtcpConnected) {
        n = send(inSession->rtcpSock, (char*)&pkt, size, 0);
    } else {
        n = sendto(inSession->rtcpSock, (char*)&pkt, size, 0, &inSession->rtcpRemoteAddr.sa, inSession->rtcpRemoteLen);
    }
    err = map_socket_value_errno(inSession->rtcpSock, n == (ssize_t)size, n);
    require_noerr(err, exit);
    ++inSession->source.retransmitSendCount;

exit:
    return (err);
}

//===========================================================================================================================
//	_RetransmitsProcessResponse
//===========================================================================================================================

static OSStatus _RetransmitsProcessResponse(AirPlayReceiverSessionRef inSession, RTCPRetransmitResponsePacket* inPkt, size_t inSize)
{
    OSStatus err;

    if (inSize == (offsetof(RTCPRetransmitResponsePacket, payload) + sizeof_field(RTCPRetransmitResponsePacket, payload.fail))) {
        inPkt->payload.fail.seq = ntohs(inPkt->payload.fail.seq);
        _SessionLock(inSession);
        _RetransmitsAbortOne(inSession, inPkt->payload.fail.seq, "FUTILE");
        _SessionUnlock(inSession);
        ++inSession->source.retransmitFutileCount;
        err = kNoErr;
        goto exit;
    }
    if (inSize < (offsetof(RTCPRetransmitResponsePacket, payload) + offsetof(RTPPacket, payload))) {
        dlogassert("Retransmit packet too small (%zu bytes)", inSize);
        err = kSizeErr;
        goto exit;
    }

    err = _GeneralAudioReceiveRTP(inSession, &inPkt->payload.rtp,
        inSize - offsetof(RTCPRetransmitResponsePacket, payload));
    ++inSession->source.retransmitReceiveCount;

exit:
    return (err);
}

//===========================================================================================================================
//	_RetransmitsSchedule
//
//	Warning: Assumes the AirTunes lock is held.
//===========================================================================================================================

static void _RetransmitsSchedule(AirPlayReceiverSessionRef inSession, uint16_t inSeqStart, uint16_t inSeqCount)
{
    uint16_t i;
    AirTunesRetransmitNode** next;
    AirTunesRetransmitNode* node;
    uint64_t nowNanos;

    for (next = &inSession->source.rtcpRTBusyList; *next; next = &(*next)->next) {
    }
    nowNanos = UpNanoseconds();
    for (i = 0; i < inSeqCount; ++i) {
        // Get a free node.

        node = inSession->source.rtcpRTFreeList;
        if (!node) {
            atr_stats_ulog(kLogLevelWarning, "### No free retransmit nodes, dropping retransmit of seq %u#%u, %u\n",
                inSeqStart, inSeqCount, i);
            break;
        }
        inSession->source.rtcpRTFreeList = node->next;

        // Schedule a retransmit request.

        node->next = NULL;
        node->seq = inSeqStart + i;
        node->tries = 0;
        node->startNanos = nowNanos;
        node->nextNanos = nowNanos;
        *next = node;
        next = &node->next;
    }
}

//===========================================================================================================================
//	_RetransmitsUpdate
//
//	Warning: Assumes the AirTunes lock is held.
//===========================================================================================================================

static void _RetransmitsUpdate(AirPlayReceiverSessionRef inSession, AirTunesBufferNode* inNode, Boolean inIsRetransmit)
{
    AirTunesSource* const ats = &inSession->source;
    uint16_t pktSeq;
    AirTunesRetransmitNode** next;
    AirTunesRetransmitNode* curr;
    uint64_t nowNanos;
    uint64_t ageNanos;
    int64_t rttNanos;
    int credits;
    Boolean outlier;

    nowNanos = UpNanoseconds();
    pktSeq = (uint16_t)inNode->seq;

    // Search for the completed retransmit.

    for (next = &ats->rtcpRTBusyList; ((curr = *next) != NULL) && (curr->seq != pktSeq); next = &curr->next) {
    }
    if (curr) {
        if (inIsRetransmit) {
            ageNanos = nowNanos - curr->startNanos;
            if (ageNanos < ats->retransmitMinNanos)
                ats->retransmitMinNanos = ageNanos;
            if (ageNanos > ats->retransmitMaxNanos)
                ats->retransmitMaxNanos = ageNanos;
            if ((ageNanos > ats->retransmitMinNanos) && (ageNanos < ats->retransmitMaxNanos)) {
                ats->retransmitAvgNanos = ((ats->retransmitAvgNanos * 63) + ageNanos) / 64;
            }
            // If multiple requests have gone out, this may be a response to an earlier request and not the most
            // recent one so following Karn's algorithm, only consider the RTT if we've only sent 1 request.

            if (curr->tries <= 1) {
                outlier = false;
                rttNanos = nowNanos - curr->sentNanos;
                if (rttNanos < ats->rtcpRTMinRTTNanos) {
                    ats->rtcpRTMinRTTNanos = rttNanos;
                    outlier = true;
                }
                if (rttNanos > ats->rtcpRTMaxRTTNanos) {
                    ats->rtcpRTMaxRTTNanos = rttNanos;
                    outlier = true;
                }
                if (!outlier) {
                    int64_t errNanos;
                    int64_t absErrNanos;

                    errNanos = rttNanos - ats->rtcpRTAvgRTTNanos;
                    absErrNanos = (errNanos < 0) ? -errNanos : errNanos;
                    ats->rtcpRTAvgRTTNanos = ats->rtcpRTAvgRTTNanos + (errNanos / 8);
                    ats->rtcpRTDevRTTNanos = ats->rtcpRTDevRTTNanos + ((absErrNanos - ats->rtcpRTDevRTTNanos) / 4);
                    ats->rtcpRTTimeoutNanos = (2 * ats->rtcpRTAvgRTTNanos) + (4 * ats->rtcpRTDevRTTNanos);
                    if (ats->rtcpRTTimeoutNanos > 100000000) // Cap at 100 ms
                    {
                        ats->rtcpRTTimeoutNanos = 100000000;
                    }
                }
            }
        }

        *next = curr->next;
        curr->next = ats->rtcpRTFreeList;
        ats->rtcpRTFreeList = curr;
    } else if (inIsRetransmit) {
        atr_stats_ulog(kLogLevelInfo, "### Retransmit seq %u not found\n", pktSeq);
        ++ats->retransmitNotFoundCount;
    }

    // Retry retransmits that have timed out.

    credits = 3;
    for (curr = ats->rtcpRTBusyList; curr; curr = curr->next) {
        if (nowNanos < curr->nextNanos)
            continue;
        ageNanos = nowNanos - curr->startNanos;
        if (curr->tries++ > 0) {
            if (ageNanos < ats->retransmitRetryMinNanos)
                ats->retransmitRetryMinNanos = ageNanos;
            if (ageNanos > ats->retransmitRetryMaxNanos)
                ats->retransmitRetryMaxNanos = ageNanos;
        }
        curr->sentNanos = nowNanos;
        curr->nextNanos = nowNanos + ats->rtcpRTTimeoutNanos;
        _RetransmitsSendRequest(inSession, curr->seq, 1);
        if (--credits == 0)
            break;
    }
}

//===========================================================================================================================
//	_RetransmitsAbortAll
//
//	Warning: Assumes the AirTunes lock is held.
//===========================================================================================================================

static void _RetransmitsAbortAll(AirPlayReceiverSessionRef inSession, const char* inReason)
{
    AirTunesSource* const ats = &inSession->source;
    AirTunesRetransmitNode* curr;

    if (ats->rtcpRTBusyList)
        atr_stats_ulog(kLogLevelInfo, "### Aborting all retransmits (%s)\n", inReason);

    while ((curr = ats->rtcpRTBusyList) != NULL) {
        ats->rtcpRTBusyList = curr->next;
        curr->next = ats->rtcpRTFreeList;
        ats->rtcpRTFreeList = curr;
    }
}

//===========================================================================================================================
//	_RetransmitsAbortOne
//
//	Warning: Assumes the AirTunes lock is held.
//===========================================================================================================================

static void _RetransmitsAbortOne(AirPlayReceiverSessionRef inSession, uint32_t inSeq, const char* inReason)
{
    AirTunesSource* const ats = &inSession->source;
    AirTunesRetransmitNode** next;
    AirTunesRetransmitNode* curr;
    uint64_t nowNanos;

    if (inSession->receiveAudioOverTCP) {
        return;
    }

    if (ats->rtcpRTBusyList)
        atr_ulog(kLogLevelInfo, "### Aborting retransmits <= %u (%s)\n", inSeq, inReason);

    nowNanos = UpNanoseconds();
    for (next = &ats->rtcpRTBusyList; (curr = *next) != NULL;) {
        if (Seq_LE(inSession, curr->seq, inSeq)) {
            atr_ulog(kLogLevelVerbose, "    ### Abort retransmit %5u  T %2u  A %10llu \n",
                curr->seq, curr->tries, nowNanos - curr->startNanos);

            *next = curr->next;
            curr->next = ats->rtcpRTFreeList;
            ats->rtcpRTFreeList = curr;
            continue;
        }
        next = &curr->next;
    }
}

#if 0
#pragma mark -
#pragma mark == MainAltAudio ==
#endif

//===========================================================================================================================
//	_MainAudioSetup
//===========================================================================================================================

static void configureClockSettings(AirPlayReceiverSessionRef inSession, AirTunesClockSettings* settings)
{
    uint32_t powerState = (uint32_t)AirPlayReceiverSessionPlatformGetInt64(inSession, CFSTR(kAirPlayProperty_PTPPowerState), NULL, NULL);

    switch (powerState) {
    case kPTPPowerStateNoBattery:
        settings->ptp.priority1 = 248;
        settings->ptp.priority2 = 248;
        break;
    case kPTPPowerStateBatteryPowered:
        settings->ptp.priority1 = 250;
        settings->ptp.priority2 = 248;
        break;
    case kPTPPowerStateBatteryWallPowered:
        settings->ptp.priority1 = 250;
        settings->ptp.priority2 = 247;
        break;
    default:
        settings->ptp.priority1 = 248;
        settings->ptp.priority2 = 248;
        atr_ulog(kLogLevelWarning, "%s: unrecognized PTPPowerState = %d\n", __ROUTINE__, powerState);
    }
    settings->ptp.clientDeviceID = inSession->clientDeviceID;
}

//===========================================================================================================================
//	_ClockSetup
//===========================================================================================================================
static OSStatus _ClockSetup(AirPlayReceiverSessionRef inSession, CFDictionaryRef inRequestParams)
{
    OSStatus err = kNoErr;
    int timingPortRemote = 0;
    CFStringRef timingProtocol = NULL;
    AirTunesClockSettings settings;

    timingProtocol = (CFStringRef)(CFDictionaryContainsKey(inRequestParams, CFSTR(kAirPlayKey_TimingProtocol)) ? CFDictionaryGetValue(inRequestParams, CFSTR(kAirPlayKey_TimingProtocol)) : CFSTR(kAirPlayTimingProtocol_NTP));

    configureClockSettings(inSession, &settings);

    err = AirTunesClock_Create(timingProtocol, &settings, &inSession->airTunesClock);
    require_noerr(err, exit);

    if (AirTunesClock_Class(inSession->airTunesClock) == kAirTunesClockNTP) {
        timingPortRemote = (int)CFDictionaryGetInt64(inRequestParams, CFSTR(kAirPlayKey_Port_Timing), NULL);
        if (timingPortRemote <= 0)
            timingPortRemote = kAirPlayFixedPort_TimeSyncLegacy;
        inSession->timingPortRemote = timingPortRemote;

        err = _TimingInitialize(inSession);
        require_noerr(err, exit);
    }

    (void)AirPlayReceiverSessionSetProperty(inSession, kCFObjectFlagDirect, CFSTR(kAirPlayProperty_AirPlayClocking), NULL, timingProtocol);

exit:

    return (err);
}

#if 0
#pragma mark -
#pragma mark == Timing ==
#endif

//===========================================================================================================================
//	_TimingInitialize
//===========================================================================================================================

static OSStatus _TimingInitialize(AirPlayReceiverSessionRef inSession)
{
    OSStatus err;
    sockaddr_ip sip;

    // Set up a socket to send and receive timing-related info.

    SockAddrCopy(&inSession->peerAddr, &sip);
    err = ServerSocketOpen(sip.sa.sa_family, SOCK_DGRAM, IPPROTO_UDP, -kAirPlayPort_TimeSyncClient,
        &inSession->timingPortLocal, kSocketBufferSize_DontSet, &inSession->timingSock);
    require_noerr(err, exit);

    SocketSetPacketTimestamps(inSession->timingSock, true);
    SocketSetQoS(inSession->timingSock, kSocketQoS_NTP);

    // Connect to the server address to avoid the IP stack doing a temporary connect on each send.
    // Using connect also allows us to receive ICMP errors if the server goes away.

    SockAddrSetPort(&sip, inSession->timingPortRemote);
    inSession->timingRemoteAddr = sip;
    inSession->timingRemoteLen = SockAddrGetSize(&sip);
    err = connect(inSession->timingSock, &sip.sa, inSession->timingRemoteLen);
    err = map_socket_noerr_errno(inSession->timingSock, err);
    if (err)
        dlog(kLogLevelNotice, "### Timing connect UDP to %##a failed (using sendto instead): %#m\n", &sip, err);
    inSession->timingConnected = !err;

    // Set up a socket for sending commands to the thread.

    err = OpenSelfConnectedLoopbackSocket(&inSession->timingCmdSock);
    require_noerr(err, exit);

    atr_ulog(kLogLevelTrace, "Timing set up on port %d to port %d\n", inSession->timingPortLocal, inSession->timingPortRemote);

exit:
    if (err) {
        atr_ulog(kLogLevelWarning, "### Timing setup failed: %#m\n", err);
        _TimingFinalize(inSession);
    }
    return (err);
}

//===========================================================================================================================
//	_TimingFinalize
//===========================================================================================================================

static OSStatus _TimingFinalize(AirPlayReceiverSessionRef inSession)
{
    OSStatus err;
    Boolean wasStarted;

    DEBUG_USE_ONLY(err);
    wasStarted = IsValidSocket(inSession->timingSock);

    // Signal the thread to quit and wait for it to signal back that it exited.

    if (inSession->timingThreadPtr) {
        err = SendSelfConnectedLoopbackMessage(inSession->timingCmdSock, "q", 1);
        check_noerr(err);

        err = pthread_join(inSession->timingThread, NULL);
        check_noerr(err);
        inSession->timingThreadPtr = NULL;
    }

    // Clean up resources.

    ForgetSocket(&inSession->timingCmdSock);
    ForgetSocket(&inSession->timingSock);
    if (wasStarted)
        atr_ulog(kLogLevelTrace, "Timing finalized\n");
    return (kNoErr);
}

//===========================================================================================================================
//	_TimingNegotiate
//===========================================================================================================================

static OSStatus _TimingNegotiate(AirPlayReceiverSessionRef inSession)
{
    SocketRef const timingSock = inSession->timingSock;
    OSStatus err;
    int nFailure;
    int nSuccess;
    int nTimeouts;
    int nSendErrors;
    int nRecvErrors;
    OSStatus lastSendError;
    OSStatus lastRecvError;
    fd_set readSet;
    int n;
    struct timeval timeout;

    require_action(inSession->timingThreadPtr == NULL, exit, err = kAlreadyInitializedErr);

    nFailure = 0;
    nSuccess = 0;
    nTimeouts = 0;
    nRecvErrors = 0;
    lastSendError = kNoErr;
    lastRecvError = kNoErr;
    FD_ZERO(&readSet);
    for (;;) {
        // Send a request.

        nSendErrors = 0;
        for (;;) {
            err = _TimingSendRequest(inSession);
            if (!err)
                break;
            atr_ulog(kLogLevelWarning, "### Time sync send error: %#m\n", err);
            usleep(100000);
            if (err != lastSendError) {
                atr_ulog(kLogLevelNotice, "Time negotiate send error: %d\n", (int)err);
                lastSendError = err;
            }
            if (++nSendErrors >= 64) {
                atr_ulog(kLogLevelError, "Too many time negotiate send failures: %d\n", (int)err);
                goto exit;
            }
        }

        // Receive and process the response.

        for (;;) {
            FD_SET(timingSock, &readSet);
            timeout.tv_sec = 0;
            timeout.tv_usec = 500 * 1000;
            n = select(timingSock + 1, &readSet, NULL, NULL, &timeout);
            err = select_errno(n);
            if (err) {
                atr_ulog(kLogLevelWarning, "### Time sync select() error: %#m\n", err);
#if (TARGET_OS_POSIX)
                if (err == EINTR)
                    continue;
#endif
                ++nTimeouts;
                ++nFailure;
                if (err != lastRecvError) {
                    atr_ulog(kLogLevelNotice, "Time negotiate receive error: %d\n", (int)err);
                    lastRecvError = err;
                }
                break;
            }

            err = _TimingReceiveResponse(inSession, timingSock);
            if (err) {
                if (err == ECONNREFUSED) {
                    goto exit;
                }
                ++nRecvErrors;
                ++nFailure;
                atr_ulog(kLogLevelWarning, "### Time sync receive error: %#m\n", err);
                if (err != lastRecvError) {
                    atr_ulog(kLogLevelNotice, "Time negotiate receive error: %d\n", (int)err);
                    lastRecvError = err;
                }
                if (err == kDuplicateErr) {
                    DrainUDPSocket(timingSock, 500, NULL);
                }
            } else {
                ++nSuccess;
            }
            break;
        }
        if (nSuccess >= 3)
            break;
        if (nFailure >= 64) {
            atr_ulog(kLogLevelError, "Too many time negotiate failures: G=%d B=%d R=%d T=%d\n",
                nSuccess, nFailure, nRecvErrors, nTimeouts);
            err = kTimeoutErr;
            goto exit;
        }
    }

    // Start the timing thread to keep our clock sync'd.

    err = pthread_create(&inSession->timingThread, NULL, _TimingThread, inSession);
    require_noerr(err, exit);
    inSession->timingThreadPtr = &inSession->timingThread;

    atr_ulog(kLogLevelTrace, "Timing started\n");

exit:
    return (err);
}

//===========================================================================================================================
//	_TimingThread
//===========================================================================================================================

static void* _TimingThread(void* inArg)
{
    AirPlayReceiverSessionRef const session = (AirPlayReceiverSessionRef)inArg;
    SocketRef const sock = session->timingSock;
    SocketRef const cmdSock = session->timingCmdSock;
    fd_set readSet;
    int maxFd = MAX(sock, cmdSock) + 1;
    struct timeval timeout;
    int n;
    OSStatus err;

    SetThreadName("AirPlayTimeSyncClient");
    SetCurrentThreadPriority(kAirPlayThreadPriority_TimeSyncClient);
    atr_ulog(kLogLevelTrace, "Timing thread starting\n");

    for (;;) {
        FD_ZERO(&readSet);
        FD_SET(sock, &readSet);
        FD_SET(cmdSock, &readSet);
        timeout.tv_sec = 2;
        timeout.tv_usec = Random32() % 1000000;
        n = select(maxFd, &readSet, NULL, NULL, &timeout);
        err = select_errno(n);
        if (err == EINTR)
            continue;
        if (err == kTimeoutErr) {
            _TimingSendRequest(session);
            continue;
        }
        if (err) {
            dlogassert("select() error: %#m", err);
            usleep(100000);
            continue;
        }
        if (FD_ISSET(sock, &readSet))
            _TimingReceiveResponse(session, sock);
        if (FD_ISSET(cmdSock, &readSet))
            break; // The only event is quit so break if anything is pending.
    }
    atr_ulog(kLogLevelTrace, "Timing thread exit\n");
    return (NULL);
}

//===========================================================================================================================
//	_TimingSendRequest
//
//	Note: This function does not need the AirTunes lock because it only accesses variables from a single thread at a time.
//		  These variables are only accessed once by the RTSP thread during init and then only by the timing thread.
//===========================================================================================================================

static OSStatus _TimingSendRequest(AirPlayReceiverSessionRef inSession)
{
    OSStatus err;
    AirTunesSource* src;
    RTCPTimeSyncPacket pkt;
    AirTunesTime now;
    ssize_t n;

    src = &inSession->source;

    // Build and send the request. The response is received asynchronously.

    pkt.v_p_m = RTCPHeaderInsertVersion(0, kRTPVersion);
    pkt.pt = kRTCPTypeTimeSyncRequest;
    pkt.length = htons((sizeof(pkt) / 4) - 1);
    pkt.rtpTimestamp = 0;
    pkt.ntpOriginateHi = 0;
    pkt.ntpOriginateLo = 0;
    pkt.ntpReceiveHi = 0;
    pkt.ntpReceiveLo = 0;

    AirTunesClock_GetSynchronizedTime(inSession->airTunesClock, &now);
    src->rtcpTILastTransmitTimeHi = now.secs + kNTPvsUnixSeconds;
    src->rtcpTILastTransmitTimeLo = (uint32_t)(now.frac >> 32);
    pkt.ntpTransmitHi = htonl(src->rtcpTILastTransmitTimeHi);
    pkt.ntpTransmitLo = htonl(src->rtcpTILastTransmitTimeLo);

    if (inSession->timingConnected) {
        n = send(inSession->timingSock, (char*)&pkt, sizeof(pkt), 0);
    } else {
        n = sendto(inSession->timingSock, (char*)&pkt, sizeof(pkt), 0, &inSession->timingRemoteAddr.sa,
            inSession->timingRemoteLen);
    }
    err = map_socket_value_errno(inSession->timingSock, n == (ssize_t)sizeof(pkt), n);
    require_noerr_quiet(err, exit);
    increment_wrap(src->rtcpTISendCount, 1);

exit:
    if (err)
        atr_ulog(kLogLevelNotice, "### NTP send request failed: %#m\n", err);
    return (err);
}

//===========================================================================================================================
//	_TimingReceiveResponse
//===========================================================================================================================

static OSStatus _TimingReceiveResponse(AirPlayReceiverSessionRef inSession, SocketRef inSock)
{
    OSStatus err;
    RTCPPacket pkt;
    size_t len;
    uint64_t ticks;
    int tmp;
    AirTunesTime recvTime;

    err = SocketRecvFrom(inSock, &pkt, sizeof(pkt), &len, NULL, 0, NULL, &ticks, NULL, NULL);
    if (err == EWOULDBLOCK)
        goto exit;
    require_noerr(err, exit);
    if (len < sizeof(pkt.header)) {
        dlogassert("Bad size: %zu < %zu", sizeof(pkt.header), len);
        err = kSizeErr;
        goto exit;
    }

    tmp = RTCPHeaderExtractVersion(pkt.header.v_p_c);
    if (tmp != kRTPVersion) {
        dlogassert("Bad version: %d", tmp);
        err = kVersionErr;
        goto exit;
    }
    if (pkt.header.pt != kRTCPTypeTimeSyncResponse) {
        dlogassert("Wrong packet type: %d", pkt.header.pt);
        err = kTypeErr;
        goto exit;
    }

    require_action(len >= sizeof(pkt.timeSync), exit, err = kSizeErr);
    AirTunesClock_GetSynchronizedTimeNearUpTicks(inSession->airTunesClock, &recvTime, ticks);
    err = _TimingProcessResponse(inSession, &pkt.timeSync, &recvTime);

exit:
    return (err);
}

//===========================================================================================================================
//	_TimingProcessResponse
//
//	Note: This function does not need the AirTunes lock because it only accesses variables from a single thread at a time.
//		  These variables are only accessed once by the RTSP thread during init and then only by the timing thread.
//===========================================================================================================================

static OSStatus _TimingProcessResponse(AirPlayReceiverSessionRef inSession, RTCPTimeSyncPacket* inPkt, const AirTunesTime* inTime)
{
    AirTunesSource* const src = &inSession->source;
    OSStatus err;
    uint64_t t1;
    uint64_t t2;
    uint64_t t3;
    uint64_t t4;
    double offset;
    double rtt;
    unsigned int i;
    Boolean useMeasurement;
    Boolean forceStep;

    inPkt->rtpTimestamp = ntohl(inPkt->rtpTimestamp);
    inPkt->ntpOriginateHi = ntohl(inPkt->ntpOriginateHi);
    inPkt->ntpOriginateLo = ntohl(inPkt->ntpOriginateLo);
    inPkt->ntpReceiveHi = ntohl(inPkt->ntpReceiveHi);
    inPkt->ntpReceiveLo = ntohl(inPkt->ntpReceiveLo);
    inPkt->ntpTransmitHi = ntohl(inPkt->ntpTransmitHi);
    inPkt->ntpTransmitLo = ntohl(inPkt->ntpTransmitLo);

    // Make sure this response is for the last request we made and is not a duplicate response.

    if ((inPkt->ntpOriginateHi != src->rtcpTILastTransmitTimeHi) || (inPkt->ntpOriginateLo != src->rtcpTILastTransmitTimeLo)) {
        err = kDuplicateErr;
        goto exit;
    }
    src->rtcpTILastTransmitTimeHi = 0; // Zero so we don't try to process a duplicate.
    src->rtcpTILastTransmitTimeLo = 0;

    // Calculate the relative offset between clocks.
    //
    // Client:  T1           T4
    // ----------------------------->
    //           \           ^
    //            \         /
    //             v       /
    // ----------------------------->
    // Server:     T2     T3
    //
    // Clock offset in NTP units     = ((T2 - T1) + (T3 - T4)) / 2.
    // Round-trip delay in NTP units =  (T4 - T1) - (T3 - T2)

    t1 = (((uint64_t)inPkt->ntpOriginateHi) << 32) | inPkt->ntpOriginateLo;
    t2 = (((uint64_t)inPkt->ntpReceiveHi) << 32) | inPkt->ntpReceiveLo;
    t3 = (((uint64_t)inPkt->ntpTransmitHi) << 32) | inPkt->ntpTransmitLo;
    t4 = (((uint64_t)(inTime->secs + kNTPvsUnixSeconds)) << 32) + (inTime->frac >> 32);

    offset = 0.5 * ((((double)((int64_t)(t2 - t1))) * kNTPFraction) + (((double)((int64_t)(t3 - t4))) * kNTPFraction));
    rtt = (((double)((int64_t)(t4 - t1))) * kNTPFraction) - (((double)((int64_t)(t3 - t2))) * kNTPFraction);

    // Update round trip time stats.

    if (rtt < src->rtcpTIClockRTTMin)
        src->rtcpTIClockRTTMin = rtt;
    if (rtt > src->rtcpTIClockRTTMax)
        src->rtcpTIClockRTTMax = rtt;
    if (src->rtcpTIResponseCount == 0)
        src->rtcpTIClockRTTAvg = rtt;
    src->rtcpTIClockRTTAvg = ((15.0 * src->rtcpTIClockRTTAvg) + rtt) * (1.0 / 16.0);

    // Update clock offset stats. If this is first time ever or the first time after a clock step, reset the stats.

    if (src->rtcpTIResetStats || (src->rtcpTIResponseCount == 0)) {
        for (i = 0; i < countof(src->rtcpTIClockOffsetArray); ++i) {
            src->rtcpTIClockDelayArray[i] = 1000.0;
            src->rtcpTIClockOffsetArray[i] = 0.0;
        }
        src->rtcpTIClockIndex = 0;
        src->rtcpTIClockOffsetAvg = 0.0;
        src->rtcpTIClockOffsetMin = offset;
        src->rtcpTIClockOffsetMax = offset;
    }

    //Only use measurements with a short RTT (delay)
    //Typical NTP 8 stage shift register as per https://www.eecis.udel.edu/~mills/ntp/html/filter.html

    useMeasurement = true;
    for (i = 0; i < countof(src->rtcpTIClockDelayArray); ++i) {
        if (rtt > src->rtcpTIClockDelayArray[i]) {
            useMeasurement = false;
            break;
        }
    }

    src->rtcpTIClockDelayArray[src->rtcpTIClockIndex] = rtt;
    src->rtcpTIClockOffsetArray[src->rtcpTIClockIndex] = offset;
    src->rtcpTIClockIndex++;
    if (src->rtcpTIClockIndex >= countof(src->rtcpTIClockOffsetArray)) {
        src->rtcpTIClockIndex = 0;
    }

    src->rtcpTIClockOffsetAvg = ((15.0 * src->rtcpTIClockOffsetAvg) + offset) * (1.0 / 16.0);
    if (offset < src->rtcpTIClockOffsetMin)
        src->rtcpTIClockOffsetMin = offset;
    if (offset > src->rtcpTIClockOffsetMax)
        src->rtcpTIClockOffsetMax = offset;

    if (useMeasurement) {
        // Sync our local clock to the server's clock. If this is the first sync, always step.

        forceStep = (src->rtcpTIResponseCount == 0);
        src->rtcpTIResetStats = AirTunesClock_Adjust(inSession->airTunesClock, (int64_t)(offset * 1E9), forceStep);
        if (src->rtcpTIResetStats && !forceStep)
            ++src->rtcpTIStepCount;
        ++src->rtcpTIResponseCount;
    }
    err = kNoErr;

exit:
    return (err);
}

#if (AIRPLAY_GENERAL_AUDIO_SKEW)
//===========================================================================================================================
//	_TimingProcessTimeAnnounce
//
//	Warning: Assumes the AirTunes lock is held.
//===========================================================================================================================

static void _TimingProcessTimeAnnounce(AirPlayReceiverSessionRef inSession, const AirTunesTime* inGlobalTime, uint32_t inRTPTime)
{
    AirPlayAudioStreamContext* const ctx = &inSession->mainAudioCtx;
    AirTunesSource* const src = &inSession->source;
    double globalTimeFP;
    Boolean marker, unexpected, logReset, logHuge;
    unsigned int oldAnnounceCount;
    uint32_t oldRTPOffsetAvg, oldRTPOffsetActive, oldRTPOffset, rtpOffset;
    uint32_t oldUDiff, udiff;
    double offset;
    uint32_t uskew;
    int sskew;

    debug_trace_init_begin(uint64_t, traceTime, UpNanoseconds());

    globalTimeFP = AirTunesTime_ToFP(inGlobalTime);
    logReset = false;
    logHuge = false;

    // The following assignments are not really necessary, but GCC can't figure out they're not uninitialized.

    oldRTPOffsetAvg = 0;
    oldRTPOffsetActive = 0;
    oldRTPOffset = 0;
    oldUDiff = 0;
    unexpected = false;

    // Determine the offset between our RTP time and the server's RTP time at the specified NTP time.
    offset = ctx->sampleRate * (globalTimeFP - src->timeAnnounceNetworkTimeFP);
    rtpOffset = (inRTPTime - ((uint32_t)offset)) - src->timeAnnounceRTPTime;
    airtunes_record_rtp_offset(rtpOffset);

    // If this is the first time or if the instantaneous offset is too far off the avg then reset.

    oldAnnounceCount = src->timeAnnounceCount;
    marker = RTCPTimeAnnounceExtractMarker(src->timeAnnounceV_P_M);
    udiff = Mod32_Diff(rtpOffset, src->rtpOffsetAvg);
    if ((oldAnnounceCount == 0) || marker || (udiff > kAirTunesRTPOffsetResetThreshold)) {
        unexpected = ((oldAnnounceCount != 0) && !marker);
        if (unexpected) {
            ++src->rtpOffsetSkewResetCount;
            debug_increment_saturate(gAirTunesDebugUnexpectedRTPOffsetResetCount, UINT_MAX);
        }

        oldRTPOffsetAvg = src->rtpOffsetAvg;
        oldRTPOffsetActive = src->rtpOffsetActive;
        oldUDiff = udiff;
        src->rtpOffset = rtpOffset;
        src->rtpOffsetAvg = rtpOffset;
        src->rtpSkewAdjusting = false;
        if (oldAnnounceCount == 0) // If this is the first announcement, force an immediate apply.
        {
            src->rtpOffsetActive = src->rtpOffsetAvg;
            src->rtpOffsetApply = false;
            inSession->stutterCreditPending = true;

            atr_ulog(kLogLevelNotice, "%s: setting rtpOffsetActive = %u\n", __func__, src->rtpOffsetActive);
        } else if (!src->rtpOffsetApply) // Only update the apply info if an apply isn't already pending.
        {
            src->rtpOffsetApplyTimestamp = src->timeAnnounceRTPApply;
            src->rtpOffsetApply = true;
        }
        PIDReset(&src->rtpSkewPIDController);
        udiff = 0;
        logReset = true;
    }

    // Update RTP offset stats.

    if (Mod32_GT(rtpOffset, src->rtpOffsetAvg))
        src->rtpOffsetAvg += (udiff / 16);
    else
        src->rtpOffsetAvg -= (udiff / 16);
    if (udiff > src->rtpOffsetMaxDelta)
        src->rtpOffsetMaxDelta = udiff;

    // Update skew calculations.

    uskew = Mod32_Diff(src->rtpOffset, src->rtpOffsetAvg);
    sskew = (int)(src->rtpOffset - src->rtpOffsetAvg);
    if (uskew > kAirTunesRTPOffsetResetThreshold) // Too much skew to gradually adjust so just reset it.
    {
        debug_increment_saturate(gAirTunesDebugHugeSkewResetCount, UINT_MAX);
        oldRTPOffset = src->rtpOffset;
        oldRTPOffsetAvg = src->rtpOffsetAvg;
        oldRTPOffsetActive = src->rtpOffsetActive;
        src->rtpOffset = src->rtpOffsetAvg;
        src->rtpOffsetActive = src->rtpOffsetAvg;

        atr_ulog(kLogLevelNotice, "%s: setting rtpOffsetActive = %u\n", __func__, src->rtpOffsetActive);

        src->rtpOffsetApply = false;
        src->rtpSkewAdjusting = false;
        logHuge = true;
        inSession->stutterCreditPending = true;
        PIDReset(&src->rtpSkewPIDController);
    } else if (debug_false_conditional(gAirTunesDebugNoSkewSlew)) {
        src->rtpSkewAdjusting = false;
    } else if (src->rtpSkewPlatformAdjust) {
        src->rtpSkewAdjust = sskew;
        AirPlayReceiverSessionPlatformSetInt64(inSession, CFSTR(kAirPlayProperty_Skew), NULL, sskew);
    } else {
        src->rtpSkewAdjust = (int)(1000.0 * PIDUpdate(&src->rtpSkewPIDController, sskew));
        if (src->rtpSkewAdjust != 0) {
            src->rtpSkewSamplesPerAdjust = (ctx->sampleRate * 1000) / AbsoluteValue(src->rtpSkewAdjust);
            if (src->rtpSkewSamplesPerAdjust < kAirTunesMaxSkewAdjustRate) {
                src->rtpSkewSamplesPerAdjust = kAirTunesMaxSkewAdjustRate;
            }
            src->rtpSkewAdjusting = true;
        } else {
            src->rtpSkewAdjusting = false;
        }
    }
    if ((uskew > 5) || debug_false_conditional(gAirTunesDebugLogAllSkew)) {
        atr_ulog(kLogLevelVerbose, "RTP Skew %+4d, %+7d mHz, adjust %8u\n",
            sskew, src->rtpSkewAdjust, src->rtpSkewSamplesPerAdjust);
    }
    if (uskew > src->rtpOffsetMaxSkew)
        src->rtpOffsetMaxSkew = uskew;
    increment_wrap(src->timeAnnounceCount, 1);

    atr_ulog(kLogLevelChatty, "RTP Sync: New %10u, Curr %10u, Avg %10u, Last %10u, UDiff %4u, Max Delta %4u, Skew %+4d\n",
        rtpOffset, src->rtpOffset, src->rtpOffsetAvg, src->rtpOffsetLast, udiff, src->rtpOffsetMaxDelta, sskew);
    src->rtpOffsetLast = rtpOffset;

    debug_trace_end(traceTime, UpNanoseconds(), gAirTunesDebugTimeAnnounceMinNanos, gAirTunesDebugTimeAnnounceMaxNanos);

    if (logReset) {
        atr_ulog(unexpected ? kLogLevelNotice : kLogLevelVerbose,
            "Reset RTP offset to %u (m=%d, a=%u, d=%u, avg=%u, active=%u, ts=%u, apply=%u) %s\n",
            rtpOffset, marker, oldAnnounceCount, oldUDiff, oldRTPOffsetAvg, oldRTPOffsetActive,
            src->timeAnnounceRTPTime, src->timeAnnounceRTPApply, unexpected ? "UNEXPECTED" : "");
    }
    if (logHuge) {
        atr_ulog(kLogLevelWarning, "Huge RTP skew reset to %u (was=%u, active=%u, apply=%u, skew=%+d)\n",
            oldRTPOffsetAvg, oldRTPOffset, oldRTPOffsetActive, src->timeAnnounceRTPApply, sskew);
    }
}
#endif // AIRPLAY_GENERAL_AUDIO_SKEW

//===========================================================================================================================
//	_updateRTPOffsetAndProcessSkew
//===========================================================================================================================

#if AIRPLAY_DEBUG_AUDIO_TIMING

static void logAudioCallBackTIming(uint64_t inHostTime, uint32_t inLocalSampleTime, AirTunesSource* source, AirTunesTime const* netTime)
{

    double elapsedHostTimeMS = UpTicksToMilliseconds(inHostTime - source->setRateTiming.anchorUpTicks);
    double elapsedSampleTimeMS = inLocalSampleTime * 1.0e3 / 44100.0;

    AirTunesTime tempNetworkTime = *netTime;
    AirTunesTime_Sub(&tempNetworkTime, &source->setRateTiming.anchor.netTime);
    double elapsedNetworkTimeMS = AirTunesTime_ToFP(&tempNetworkTime) * 1000;

    atr_ulog(kLogLevelNotice, "%s, deltaHostTimeMS, %.3f, deltaSampleTimeMS, %.3f, deltaNetworkTimeMS, %.3f)\n", __func__,
        elapsedHostTimeMS - source->lastElapsedHostTimeMS,
        elapsedSampleTimeMS - source->lastElapsedSampleTimeMS,
        elapsedNetworkTimeMS - source->lastElapsedNetworkTimeMS);

    source->lastElapsedHostTimeMS = elapsedHostTimeMS;
    source->lastElapsedSampleTimeMS = elapsedSampleTimeMS;
    source->lastElapsedNetworkTimeMS = elapsedNetworkTimeMS;
}
#endif // AIRPLAY_DEBUG_AUDIO_TIMING

static void drift_updateRTPOffsetAndProcessSkew(AirPlayReceiverSessionRef inSession, uint64_t inHostTime, uint32_t inLocalSampleTime)
{
    OSStatus err = kNoErr;
    AirTunesTime networkTimeAtInputHostTime = { 0, 0, 0, 0 }; // inHostTime converted to NetworkTime

    AirTunesSource* source = &inSession->source;
    AirPlayAudioStreamContext* const ctx = &inSession->mainAudioCtx;
    AirPlayReceiverSkewCalculationContext skewAdjustmentParams;
    static int skewdir = 0;

    // Time announce packets are used when sending realtime with PTP
    if (inSession->source.timeAnnouncePending) {
        inSession->source.timeAnnouncePending = false;
        increment_wrap(inSession->source.timeAnnounceCount, 1);
    }
    
    //printf("[%s:%d], AnchorRate:%u nowTicks:%llu nextSkewAdjustMents:%llu\n", __func__, __LINE__, source->setRateTiming.anchor.rate, UpTicks(), source->setRateTiming.nextSkewAdjustmentTicks);
    if (kPerformSkewAdjustRightAway) {
        source->setRateTiming.nextSkewAdjustmentTicks = 0;
        skewdir = 0;
    }

    require_quiet(source->setRateTiming.anchor.rate != 0, exit);
    require_quiet(UpTicks() > source->setRateTiming.nextSkewAdjustmentTicks, exit);

    // Given a hostTime/rtpTime tuple, calculate how many samples we are off from where we should be

    err = drift_convertHostTimeToNetworkTimeOnAnchorTimeline(inSession, inHostTime, &networkTimeAtInputHostTime);
    if (err == kNotInitializedErr) {
        source->setRateTiming.nextSkewAdjustmentTicks = UpTicks() + MillisecondsToUpTicks(kAirTunesClockLockIntervalMS);
    } else if (err == kExecutionStateErr) {
        AirPlayReceiverServerCommandDeselectSource(inSession->server);
    }
    require_noerr_quiet(err, exit);

    check(networkTimeAtInputHostTime.timelineID == source->setRateTiming.anchor.netTime.timelineID);

    // Perform the skew calculations

    memset(&skewAdjustmentParams, 0, sizeof(skewAdjustmentParams));
    skewAdjustmentParams.input.sampleRate = ctx->sampleRate;
    skewAdjustmentParams.input.rtpOffsetResetThreshold = kAirTunesRTPOffsetResetThreshold;
    skewAdjustmentParams.input.forceOffsetRecalculation = source->setRateTiming.forceOffsetRecalculation;
    skewAdjustmentParams.input.rtpOffsetActive = source->rtpOffsetActive;
    skewAdjustmentParams.input.anchorNetworkTime = source->setRateTiming.anchor.netTime;
    skewAdjustmentParams.input.anchorRTPTime = source->setRateTiming.anchor.rtpTime;
    skewAdjustmentParams.input.anchorRTPTimeFracs = source->setRateTiming.anchorRTPTimeFracs;
    skewAdjustmentParams.input.rtpExcessiveDriftCount = source->excessiveDriftCount;
    skewAdjustmentParams.input.rtpMaxExcessiveDriftCount = kAirTunesMaxExcessiveDriftCount;

#if AIRPLAY_DEBUG_AUDIO_TIMING
    logAudioCallBackTIming(inHostTime, inLocalSampleTime, source, &networkTimeAtInputHostTime);
#endif

    err = AirPlayReceiverSkewCalculation(&skewAdjustmentParams, networkTimeAtInputHostTime, inLocalSampleTime);
    require_noerr(err, exit);

    source->excessiveDriftCount = skewAdjustmentParams.output.rtpExcessiveDriftCount;

#if (AIRPLAY_GENERAL_AUDIO_SKEW)
    if(skewAdjustmentParams.output.samplesToSkewDouble > 88)
        skewdir++;
    else if(skewAdjustmentParams.output.samplesToSkewDouble < -88)
        skewdir--;
    else
        skewdir = 0;
    
    if(abs(skewdir) <= 25)
        skewAdjustmentParams.output.samplesToSkewDouble = 0;
    
    // Communicate to the appropriate mechanism the number of samples we should skew
    skewAdjustmentParams.output.samplesToSkewDouble = Clamp(skewAdjustmentParams.output.samplesToSkewDouble, -kAirTunesSkewLimit, kAirTunesSkewLimit);

    if (debug_false_conditional(gAirTunesDebugNoSkewSlew)) {
        source->rtpSkewAdjusting = false;
    } else if (source->rtpSkewPlatformAdjust) {
        AirPlayReceiverSessionPlatformSetDouble(inSession, CFSTR(kAirPlayProperty_Skew), NULL, skewAdjustmentParams.output.samplesToSkewDouble);
        source->rtpSkewAdjust = skewAdjustmentParams.output.samplesToSkewDouble * 1000;
    } else {
        double skewAdjustMagnitude;
        source->rtpSkewAdjust = (1000.0 * PIDUpdate(&source->rtpSkewPIDController, skewAdjustmentParams.output.samplesToSkewDouble));
        skewAdjustMagnitude = AbsoluteValue(source->rtpSkewAdjust);

        if (skewAdjustMagnitude > kAirTunesSkewMin * 1000) {
            source->rtpSkewSamplesPerAdjust = (unsigned int)((ctx->sampleRate * 1000) / skewAdjustMagnitude);
            if (source->rtpSkewSamplesPerAdjust < kAirTunesMaxSkewAdjustRate) {
                source->rtpSkewSamplesPerAdjust = kAirTunesMaxSkewAdjustRate;
            }
            source->rtpSkewAdjusting = true;
        } else {
            source->rtpSkewAdjust = 0;
            source->rtpSkewSamplesPerAdjust = 0;
            source->rtpSkewAdjusting = false;
        }
    }
#endif

    // [5] Lastly, update our timing variables for the next call

    source->setRateTiming.forceOffsetRecalculation = false;
    source->rtpOffsetActive = skewAdjustmentParams.output.rtpOffsetActive;
    source->setRateTiming.lastNetworkTime = networkTimeAtInputHostTime;
    source->setRateTiming.lastUpTicks = inHostTime;
    source->setRateTiming.lastIdealRTPTime = skewAdjustmentParams.output.rtpTimeWhereWeShouldBeU32;
    source->setRateTiming.lastIdealRTPTimeFracs = skewAdjustmentParams.output.rtpTimeWhereWeShouldBeFracs;

    check(0 <= source->setRateTiming.lastIdealRTPTimeFracs);
    check(source->setRateTiming.lastIdealRTPTimeFracs < 1.);

    if (skewAdjustmentParams.output.stutterCreditPending) {
        inSession->stutterCreditPending = skewAdjustmentParams.output.stutterCreditPending;
        inSession->source.rtpOffsetSkewResetCount++;
        PIDReset(&inSession->source.rtpSkewPIDController);
    }

    if (fabs(skewAdjustmentParams.output.samplesToSkewDouble) > source->rtpOffsetMaxSkew) {
        source->rtpOffsetMaxSkew = fabs(skewAdjustmentParams.output.samplesToSkewDouble);
    }

    source->setRateTiming.nextSkewAdjustmentTicks = UpTicks() + (uint64_t)((kAirTunesSetRateSkewIntervalMS / 1000.) * UpTicksPerSecond());

exit:
    return;
}

//===========================================================================================================================
//	drift_convertHostTimeToNetworkTimeOnAnchorTimeline
//
//  Converts an inHostTime to a networkTime, updating the anchor if the clock has changed changed GMs
//
//===========================================================================================================================

static OSStatus drift_convertHostTimeToNetworkTimeOnAnchorTimeline(AirPlayReceiverSessionRef inSession, uint64_t inHostTime, AirTunesTime* outNetworkTimeAtInputHostTime)
{
    OSStatus err = kNoErr;
    AirTunesTime networkTimeAtInputHostTime = { 0, 0, 0, 0 };
    Boolean success = false;
    const uint32_t maxAttempts = 3;
    uint32_t itr = 0;
    AirTunesSource* source = &inSession->source;

    for (itr = 0; itr < maxAttempts; itr++) {
        AirTunesTime newNetworkAnchorTime = { 0, 0, 0, 0 };

        AirTunesClock_GetSynchronizedTimeNearUpTicks(inSession->airTunesClock, &networkTimeAtInputHostTime, inHostTime);

        if (AirTunesTimelines_Equal(networkTimeAtInputHostTime, source->setRateTiming.anchor.netTime)) {
            success = true;
            break;
        }

        if (networkTimeAtInputHostTime.flags & AirTunesTimeFlag_StateErr) {
            err = kExecutionStateErr;
            break;
        }

        // Return kNotInitializedErr if we don't have a valid time or if we want
        // to have the clock settle.
        if (networkTimeAtInputHostTime.flags == AirTunesTimeFlag_Invalid) {
            err = kNotInitializedErr;
            break;
        }

        atr_ulog(kLogLevelNotice, "%s: timeline changed from %llx to %llx\n", __func__, source->setRateTiming.anchor.netTime.timelineID, networkTimeAtInputHostTime.timelineID);

        // If it's a different GM, convert the last hostTime to networkTime on our GM timeline and try again.
        AirTunesClock_GetSynchronizedTimeNearUpTicks(inSession->airTunesClock, &newNetworkAnchorTime, source->setRateTiming.lastUpTicks);

        if (!AirTunesTime_IsValid(&networkTimeAtInputHostTime)) {
            atr_ulog(kLogLevelNotice, "%s: invalid networkTimeAtInputHostTime for lastUpTicks\n", __func__, err);
            break;
        }

        source->setRateTiming.anchor.netTime = newNetworkAnchorTime;
        source->setRateTiming.anchorUpTicks = source->setRateTiming.lastUpTicks;
        source->setRateTiming.anchor.rtpTime = source->setRateTiming.lastIdealRTPTime;
        source->setRateTiming.anchorRTPTimeFracs = source->setRateTiming.lastIdealRTPTimeFracs;

        atr_ulog(kLogLevelNotice, "Updating anchor time to host: %llu; net: %1.3f(%llx); rtp: %1.3f (%u)\n",
            source->setRateTiming.anchorUpTicks,
            AirTunesTime_ToFP(&source->setRateTiming.anchor.netTime),
            source->setRateTiming.anchor.netTime.timelineID,
            (source->setRateTiming.anchor.rtpTime + source->setRateTiming.anchorRTPTimeFracs),
            source->setRateTiming.anchor.rtpTime);
    }

    if (success) {
        *outNetworkTimeAtInputHostTime = networkTimeAtInputHostTime;
    } else if (!err) {
        // The above loop really shouldn't take more than twice through. If we didn't succeed there's likely something
        // seriously wrong with the clock.

        atr_ulog(kLogLevelError, "### Error updating our anchor to a consistent timeline.\n");
        err = kTimeoutErr;
    }

    return (err);
}

#if 0
#pragma mark == AirPlay Remote Control Session ==
#endif

//===========================================================================================================================
//	_RemoteControlSessionSetup
//===========================================================================================================================

static OSStatus _RemoteControlSessionSetup(AirPlayReceiverSessionRef inSession,
    CFDictionaryRef inRequestParams,
    CFMutableDictionaryRef outResponseParams)
{
    (void)inSession;
    (void)inRequestParams;

    OSStatus err = kNoErr;
    uint64_t streamID = 0;
    CFStringRef clientTypeUUID = NULL;
    CFStringRef clientUUID = NULL;
    CFNumberRef tokenRef = NULL;
    CFNumberRef rcsObjRef = NULL;

    AirPlayRemoteControlSessionControlType controlType = kAirPlayRemoteControlSessionControlType_None;

    streamID = ++inSession->remoteControlSessionIDGenerator;
    err = _SessionGenerateResponseStreamDesc(inSession, outResponseParams, kAirPlayStreamType_RemoteControl, streamID);
    require_noerr(err, bail);

    clientTypeUUID = (CFStringRef)CFDictionaryGetValue(inRequestParams, CFSTR(kAirPlayKey_ClientTypeUUID));
    require_action(clientTypeUUID, bail, err = kParamErr);

    clientUUID = (CFStringRef)CFDictionaryGetValue(inRequestParams, CFSTR(kAirPlayKey_ClientUUID));
    require_action(clientUUID, bail, err = kParamErr);

    controlType = (AirPlayRemoteControlSessionControlType)CFDictionaryGetInt64(inRequestParams, CFSTR(kAirPlayKey_ControlType), NULL);
    if (controlType == kAirPlayRemoteControlSessionControlType_None) {
        controlType = kAirPlayRemoteControlSessionControlType_Direct;
        if (CFEqual(clientTypeUUID, kAirPlayClientTypeUUID_MediaRemote))
            controlType = kAirPlayRemoteControlSessionControlType_Relay;
    }

    rcsObjRef = CFNumberCreate(NULL, kCFNumberSInt64Type, &streamID);
    err = AirPlaySessionManagerRegisterNewRoute(inSession->server->sessionMgr, inSession, rcsObjRef,
        clientTypeUUID, clientUUID, controlType, &tokenRef);
    require_noerr(err, bail);

    err = _CreateAndRegisterRemoteControlSession(inSession, rcsObjRef, clientTypeUUID, clientUUID, controlType, tokenRef);
    require_noerr(err, bail);

    atr_ulog(kLogLevelNotice, "RCS setup successful\n");

bail:
    CFReleaseNullSafe(rcsObjRef);
    return (err);
}

//===========================================================================================================================
//	_CreateAndRegisterRemoteControlSession
//===========================================================================================================================

static OSStatus _CreateAndRegisterRemoteControlSession(
    AirPlayReceiverSessionRef inSession,
    CFNumberRef inRCSObjRef,
    CFStringRef inClientTypeUUID,
    CFStringRef inClientUUID,
    AirPlayRemoteControlSessionControlType inControlType,
    CFNumberRef inTokenRef)
{
    OSStatus err = kNoErr;
    CFMutableDictionaryRef entryDict = NULL; // contains clientTypeUUID and token corresponding to master controller

    require_action(inRCSObjRef, bail, err = kParamErr);

    // create the remoteControlSessions dictionary needed to map messages from master to remotes and remotes to master
    if (!inSession->remoteControlSessions) {
        inSession->remoteControlSessions = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require_action(inSession->remoteControlSessions, bail, err = kNoMemoryErr);
    }

    entryDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(entryDict, bail, err = kNoMemoryErr);

    CFDictionarySetValue(entryDict, CFSTR(kAirPlayKey_ClientTypeUUID), inClientTypeUUID);
    CFDictionarySetValue(entryDict, CFSTR(kAirPlayKey_ClientUUID), inClientUUID);
    CFDictionarySetInt64(entryDict, CFSTR(kAirPlayKey_ControlType), inControlType);
    CFDictionarySetValue(entryDict, CFSTR(kAirPlayKey_TokenRef), inTokenRef);
    CFDictionarySetValue(inSession->remoteControlSessions, inRCSObjRef, entryDict);

bail:
    CFReleaseNullSafe(entryDict);
    return (err);
}

//===========================================================================================================================
//	_AirPlayReceiverSessionResponseArrived -	this function is hit when the HTTP message containing the command reaches the
//												sender, the sender sends back a response and the response arrives at the receiver.
//===========================================================================================================================

typedef struct
{
    AirPlayReceiverSessionRef session;
    dispatch_semaphore_t responseArrivedSema;
    OSStatus remoteControlSessionCreationStatus;
} AirPlayReceiverSessionResponseArrivedContext;

static void _AirPlayReceiverSessionResponseArrived(OSStatus inStatus, CFDictionaryRef inResponse, void* inContext)
{
    AirPlayReceiverSessionResponseArrivedContext* context = (AirPlayReceiverSessionResponseArrivedContext*)inContext;
    OSStatus err = kNoErr;
    AirPlayReceiverSessionRef session = NULL;

    atr_ulog(kLogLevelNotice, "RCS creation status: %d\n", inStatus);
    if (inResponse) {
        atr_ulog(kLogLevelNotice, "RCS creation response: %@ \n", inResponse);
    }
    require_action(context, bail, err = kParamErr);
    session = context->session;
    require_action(session, bail, err = kParamErr);

    context->remoteControlSessionCreationStatus = inStatus;
    dispatch_semaphore_signal(context->responseArrivedSema);

    atr_ulog(kLogLevelNotice, "Signalling complete\n");
bail:
    if (err)
        atr_ulog(kLogLevelNotice, "error servicing RCS creation response: %d\n", err);
}

//===========================================================================================================================
//	AirPlayReceiverSessionDeleteAndUnregisterRemoteControlSession
//===========================================================================================================================

OSStatus AirPlayReceiverSessionDeleteAndUnregisterRemoteControlSession(
    AirPlayReceiverSessionRef inSession,
    CFNumberRef inStreamIDRef)
{
    OSStatus err = kNoErr;

    require_quiet(inSession, bail);
    require_quiet(inStreamIDRef, bail);
    require_quiet(inSession->remoteControlSessions, bail);

    CFDictionaryRemoveValue(inSession->remoteControlSessions, inStreamIDRef);
bail:
    return (err);
}

//===========================================================================================================================
//	AirPlayReceiverSessionCreateRemoteControlSessionOnSender
//===========================================================================================================================

OSStatus AirPlayReceiverSessionCreateRemoteControlSessionOnSender(
    AirPlayReceiverSessionRef inSession,
    CFNumberRef inTokenRef,
    CFStringRef inClientTypeUUID,
    CFStringRef inClientUUID,
    CFNumberRef* outRCSObjRef)
{
    OSStatus err = kNoErr;
    CFMutableDictionaryRef requestParams = NULL;
    uint64_t streamID = 0;
    AirPlayReceiverSessionResponseArrivedContext context;
    CFNumberRef rcsObjRef = NULL;

    // generate a new streamID which identifies the new session between receiver and sender
    streamID = ++inSession->remoteControlSessionIDGenerator;

    context.remoteControlSessionCreationStatus = 0;
    context.responseArrivedSema = NULL;
    context.session = NULL;

    requestParams = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(requestParams, bail, err = kNoMemoryErr);

    CFDictionarySetValue(requestParams, CFSTR(kAirPlayKey_Type), CFSTR(kAirPlayCommand_RemoteControlSessionCreate));
    CFDictionarySetInt64(requestParams, CFSTR(kAirPlayKey_StreamID), streamID);
    CFDictionarySetValue(requestParams, CFSTR(kAirPlayKey_ClientTypeUUID), inClientTypeUUID);
    CFDictionarySetValue(requestParams, CFSTR(kAirPlayKey_ClientUUID), inClientUUID);

    context.session = (AirPlayReceiverSessionRef)CFRetain(inSession);
    context.responseArrivedSema = dispatch_semaphore_create(0);
    context.remoteControlSessionCreationStatus = kNotInitializedErr;
    require_action(context.responseArrivedSema, bail, err = kNoMemoryErr);

    err = AirPlayReceiverSessionSendCommandForObject(inSession, NULL, requestParams, _AirPlayReceiverSessionResponseArrived, &context);
    require_noerr(err, bail);

    dispatch_semaphore_wait(context.responseArrivedSema, DISPATCH_TIME_FOREVER);

    rcsObjRef = CFNumberCreate(NULL, kCFNumberSInt64Type, &streamID);
    err = _CreateAndRegisterRemoteControlSession(inSession, rcsObjRef, inClientTypeUUID, inClientUUID, kAirPlayRemoteControlSessionControlType_Relay, inTokenRef);
    require_noerr(err, bail);

    atr_ulog(kLogLevelNotice, "RCS creation status: %d\n", context.remoteControlSessionCreationStatus);
    err = context.remoteControlSessionCreationStatus;

    if (outRCSObjRef)
        *outRCSObjRef = rcsObjRef;

bail:
    CFReleaseNullSafe(rcsObjRef);
    CFReleaseNullSafe(requestParams);
    CFReleaseNullSafe(context.session);
    dispatch_release_null_safe(context.responseArrivedSema);

    return (err);
}

//===========================================================================================================================
//	_RemoteControlSessionTearDown
//===========================================================================================================================

static void _RemoteControlSessionTearDown(AirPlayReceiverSessionRef inSession, uint64_t inStreamID)
{
    OSStatus err = kNoErr;
    CFDictionaryRef rcsDict = NULL;
    CFNumberRef streamIDRef = NULL;
    CFNumberRef tokenRef = NULL;

    require_action(inStreamID, bail, err = kParamErr);

    streamIDRef = CFNumberCreate(NULL, kCFNumberSInt64Type, &inStreamID);
    require_action(streamIDRef, bail, err = kNoMemoryErr);
    rcsDict = (CFDictionaryRef)CFDictionaryGetValue(inSession->remoteControlSessions, streamIDRef);
    // retrieve token from rcsDict
    tokenRef = (CFNumberRef)CFDictionaryGetValue(rcsDict, CFSTR(kAirPlayKey_TokenRef));
    require_action(tokenRef, bail, err = kStateErr);

    err = AirPlaySessionManagerUnregisterRoute(inSession->server->sessionMgr, tokenRef);
    require_noerr(err, bail);

    err = AirPlayReceiverSessionDeleteAndUnregisterRemoteControlSession(inSession, streamIDRef);
    require_noerr(err, bail);

bail:
    CFReleaseNullSafe(streamIDRef);
    atr_ulog(kLogLevelNotice, "Remote Control Session torn down for session = %p, streamID = %llu, err = %d\n", inSession, inStreamID, err);
    return;
}

#if 0
#pragma mark -
#pragma mark == Utils ==
#endif

//===========================================================================================================================
//	_AddResponseStream
//===========================================================================================================================

static OSStatus _AddResponseStream(CFMutableDictionaryRef inResponseParams, CFDictionaryRef inStreamDesc)
{
    OSStatus err;
    CFMutableArrayRef responseStreams;

    responseStreams = (CFMutableArrayRef)CFDictionaryGetCFArray(inResponseParams, CFSTR(kAirPlayKey_Streams), NULL);
    if (!responseStreams) {
        responseStreams = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
        require_action(responseStreams, exit, err = kNoMemoryErr);
        CFArrayAppendValue(responseStreams, inStreamDesc);
        CFDictionarySetValue(inResponseParams, CFSTR(kAirPlayKey_Streams), responseStreams);
        CFRelease(responseStreams);
    } else {
        CFArrayAppendValue(responseStreams, inStreamDesc);
    }
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	_AudioDecoderInitialize
//===========================================================================================================================

static OSStatus _AudioDecoderInitialize(AirPlayReceiverSessionRef inSession)
{
    AirPlayAudioStreamContext* const ctx = &inSession->mainAudioCtx;
    OSStatus err;
    AudioStreamBasicDescription inputFormat;
    AudioStreamBasicDescription outputFormat;

    if (0) {
    } // Empty if to simplify conditional logic below.
    else if (inSession->compressionType == kAirPlayCompressionType_AAC_LC) {
        ASBD_FillAAC_LC(&inputFormat, ctx->sampleRate, ctx->channels);
    }
#if (AIRPLAY_ALAC)
    else if (inSession->compressionType == kAirPlayCompressionType_ALAC) {
        ASBD_FillALAC(&inputFormat, ctx->sampleRate, ctx->bitsPerSample, ctx->channels);
    }
#endif
    else {
        dlogassert("Bad compression type: %d", inSession->compressionType);
        err = kUnsupportedErr;
        goto exit;
    }

    ASBD_FillPCM(&outputFormat, ctx->sampleRate, ctx->bitsPerSample, ctx->bitsPerSample, ctx->channels);
    err = AudioConverterNew(&inputFormat, &outputFormat, &inSession->audioConverter);
    require_noerr(err, exit);

#if (AIRPLAY_ALAC)
    if (inputFormat.mFormatID == kAudioFormatAppleLossless) {
        ALACParams params;

        params.frameLength = htonl(inputFormat.mFramesPerPacket);
        params.compatibleVersion = 0;
        params.bitDepth = (uint8_t)ctx->bitsPerSample;
        params.pb = 40;
        params.mb = 10;
        params.kb = 14;
        params.numChannels = (uint8_t)ctx->channels;
        params.maxRun = htons(255);
        params.maxFrameBytes = htonl(0);
        params.avgBitRate = htonl(0);
        params.sampleRate = htonl(ctx->sampleRate);

        err = AudioConverterSetProperty(inSession->audioConverter, kAudioConverterDecompressionMagicCookie,
            (UInt32)sizeof(params), &params);
        require_noerr(err, exit);
    }
#endif
#ifdef LINKPLAY
    else if(inputFormat.mFormatID == kAudioFormatMPEG4AAC) {
        AACParams aacParams;

        aacParams.sampleRate = (ctx->sampleRate);
        aacParams.numChannels = (uint8_t)ctx->channels;
        aacParams.frameLength = (inputFormat.mFramesPerPacket);

        err = AudioConverterSetProperty(inSession->audioConverter, kAudioConverterDecompressionMagicCookie,
                                        (UInt32)sizeof(AACParams), &aacParams);

        require_noerr(err, exit);
    }
#endif

exit:
    return (err);
}

//===========================================================================================================================
//	_AudioDecoderDecodeFrame
//===========================================================================================================================

static OSStatus
_AudioDecoderDecodeFrame(
    AirPlayReceiverSessionRef inSession,
    const uint8_t* inSrcPtr,
    size_t inSrcLen,
    uint8_t* inDstPtr,
    size_t inDstMaxLen,
    size_t* outDstLen)
{
    AirPlayAudioStreamContext* const ctx = &inSession->mainAudioCtx;
    OSStatus err;
    UInt32 frameCount;
    AudioBufferList bufferList;

    inSession->encodedDataPtr = inSrcPtr;
    inSession->encodedDataEnd = inSrcPtr + inSrcLen;

    frameCount = inSession->framesPerPacket;
    bufferList.mNumberBuffers = 1;
    bufferList.mBuffers[0].mNumberChannels = ctx->channels;
    bufferList.mBuffers[0].mDataByteSize = (uint32_t)inDstMaxLen;
    bufferList.mBuffers[0].mData = inDstPtr;

    err = AudioConverterFillComplexBuffer(inSession->audioConverter, _AudioDecoderDecodeCallback, inSession,
        &frameCount, &bufferList, NULL);
    if (err == kUnderrunErr)
        err = kNoErr;
    require_noerr(err, exit);

    *outDstLen = frameCount * ctx->bytesPerUnit;

exit:
    return (err);
}

//===========================================================================================================================
//	_AudioDecoderDecodeCallback
//
//	See <http://developer.apple.com/library/mac/#qa/qa2001/qa1317.html> for AudioConverterFillComplexBuffer callback details.
//===========================================================================================================================

static OSStatus
_AudioDecoderDecodeCallback(
    AudioConverterRef inAudioConverter,
    UInt32* ioNumberDataPackets,
    AudioBufferList* ioData,
    AudioStreamPacketDescription** outDataPacketDescription,
    void* inUserData)
{
    AirPlayReceiverSessionRef const session = (AirPlayReceiverSessionRef)inUserData;
    AirPlayAudioStreamContext* const ctx = &session->mainAudioCtx;

    (void)inAudioConverter;

    if (session->encodedDataPtr != session->encodedDataEnd) {
        check(*ioNumberDataPackets > 0);
        *ioNumberDataPackets = 1;

        ioData->mNumberBuffers = 1;
        ioData->mBuffers[0].mNumberChannels = ctx->channels;
        ioData->mBuffers[0].mDataByteSize = (UInt32)(session->encodedDataEnd - session->encodedDataPtr);
        ioData->mBuffers[0].mData = (void*)session->encodedDataPtr;
        session->encodedDataPtr = session->encodedDataEnd;

        session->encodedPacketDesc.mStartOffset = 0;
        session->encodedPacketDesc.mVariableFramesInPacket = 0;
        session->encodedPacketDesc.mDataByteSize = ioData->mBuffers[0].mDataByteSize;
        *outDataPacketDescription = &session->encodedPacketDesc;

        return (kNoErr);
    }

    *ioNumberDataPackets = 0;
    return (kUnderrunErr);
}

//===========================================================================================================================
//	_CompareOSBuildVersionStrings
//===========================================================================================================================

int _CompareOSBuildVersionStrings(const char* inVersion1, const char* inVersion2)
{
    int result;
    int major1, major2, build1, build2;
    char minor1, minor2;

    result = sscanf(inVersion1, "%d%c%d", &major1, &minor1, &build1);
    require_action(result == 3, exit, result = -1);
    minor1 = (char)toupper_safe(minor1);

    result = sscanf(inVersion2, "%d%c%d", &major2, &minor2, &build2);
    require_action(result == 3, exit, result = 1);
    minor2 = (char)toupper_safe(minor2);

    result = (major1 != major2) ? (major1 - major2) : ((minor1 != minor2) ? (minor1 - minor2) : (build1 - build2));

exit:
    return (result);
}

#if (AIRPLAY_METRICS)
//===========================================================================================================================
//	_CopyMetrics
//===========================================================================================================================
static CFDictionaryRef _CopyMetrics(AirPlayReceiverSessionRef inSession, OSStatus* outErr)
{
    CFDictionaryRef result = NULL;
    CFMutableDictionaryRef metrics;
    OSStatus err;

    // Keep the compiler happy about our decisions
    metrics = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(metrics, exit, err = kNoMemoryErr);

    if (inSession) {
        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_Status), inSession->startStatus);
#if (AIRPLAY_VOLUME_CONTROL)
        CFDictionarySetDouble(metrics, CFSTR(kAirPlayMetricKey_Volume), inSession->server->volume);
#endif
        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_TransportType), inSession->transportType);
        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_AudioFormat), inSession->audioFormat);

        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_DurationSecs), UpTicksToSeconds(UpTicks() - inSession->sessionTicks));
        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_StartupMSecs), UpTicksToMilliseconds(inSession->playTicks - inSession->sessionTicks));

        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_Glitches), inSession->glitchTotal);
        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_GlitchPeriods), inSession->glitchyPeriods);

        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_TotalNodes), inSession->nodeCount);
        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_BusyNodes), inSession->busyNodeCount);
        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_NodeSize), inSession->nodeBufferSize);
        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_ZombieNodes), inSession->zombieNodeCount);

        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_RemoteConnectionsCount), AirPlaySessionManagerRemoteSessionCount(inSession->server->sessionMgr));

        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_RemoteCommandsCount), AirPlaySessionManagerRelayCount(inSession->server->sessionMgr));

        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_AudioBytesPoolSize), inSession->audioBytesPoolSize);
        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_AudioBytesPoolUsed), inSession->audioBytesPoolUsed);

        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_lastPacketTS), inSession->lastPacketTS);
        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_lastRenderPacketTS), inSession->lastRenderPacketTS);
        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_lastRenderGapStartTS), inSession->lastRenderGapStartTS);
        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_lastRenderGapLimitTS), inSession->lastRenderGapLimitTS);

        CFDictionarySetDouble(metrics, CFSTR(kAirPlayMetricKey_Skew), inSession->source.rtpSkewAdjust);
        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_SamplesPerSkewAdjust), inSession->source.rtpSkewSamplesPerAdjust);
        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_SkewResetCount), inSession->source.rtpOffsetSkewResetCount);

        CFDictionarySetCString(metrics, CFSTR(kAirPlayMetricKey_Mode),
            inSession->receiveAudioOverTCP ? "Buffered" : "Legacy",
            kSizeCString);
        CFDictionarySetUInt64(metrics, CFSTR(kAirPlayMetricKey_ClientSessionID), inSession->clientSessionID);
        CFDictionarySetUInt64(metrics, CFSTR(kAirPlayMetricKey_Rate), inSession->source.setRateTiming.anchor.rate);

        int64_t avgBuffer_ms = AirTunesSamplesToMs(EWMA_FP_Get(&gAirPlayAudioStats.bufferAvg));
        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_AvgBuffer), avgBuffer_ms);

        CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_CompressionPercent), inSession->compressionPercentAvg);

        if (inSession->airTunesClock) {
            const char* clockStr = AirTunesClock_Description(inSession->airTunesClock);
            CFDictionarySetCString(metrics, CFSTR(kAirPlayMetricKey_ClockingMethod), clockStr, kSizeCString);

            AirTunesTime clockTime;
            AirTunesClock_GetSynchronizedTime(inSession->airTunesClock, &clockTime);

            if (clockTime.flags & AirTunesTimeFlag_Invalid) {
                clockTime.secs = 0;
                clockTime.frac = 0;
            }
            double time = clockTime.secs + (double)(clockTime.frac >> 32) / ((uint64_t)1 << 32);
            CFDictionarySetUInt64(metrics, CFSTR(kAirPlayMetricKey_ClockTimeline), clockTime.timelineID);
            CFDictionarySetDouble(metrics, CFSTR(kAirPlayMetricKey_ClockTimeSecs), time);
        }

        CFDictionarySetUInt64(metrics, CFSTR(kAirPlayMetricKey_RTPOffset), inSession->source.rtpOffsetActive);
        CFDictionarySetUInt64(metrics, CFSTR(kAirPlayMetricKey_lastPacketTS), inSession->lastPacketTS);

        double anchorTime = inSession->source.setRateTiming.anchor.netTime.secs;
        anchorTime += inSession->source.setRateTiming.anchor.netTime.frac / 18446744073709551616.0;

        CFDictionarySetDouble(metrics, CFSTR(kAirPlayMetricKey_AnchorTime), anchorTime);
        CFDictionarySetUInt64(metrics, CFSTR(kAirPlayMetricKey_AnchorTimeline), inSession->source.setRateTiming.anchor.netTime.timelineID);
        CFDictionarySetUInt64(metrics, CFSTR(kAirPlayMetricKey_AnchorRTP), inSession->source.setRateTiming.anchor.rtpTime);
    }

    CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_LostPackets), gAirPlayAudioStats.lostPackets);
    CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_UnrecoveredPackets), gAirPlayAudioStats.unrecoveredPackets);
    CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_LatePackets), gAirPlayAudioStats.latePackets);

    CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_lastNetSeq), inSession->lastNetSeq);
    CFDictionarySetInt64(metrics, CFSTR(kAirPlayMetricKey_lastNetRTP), inSession->lastNetTS);

    result = metrics;
    metrics = NULL;
    err = kNoErr;

exit:
    CFReleaseNullSafe(metrics);

    if (outErr)
        *outErr = err;
    return (result);
}

#endif

//===========================================================================================================================
//	_GetStreamSecurityKeys
//===========================================================================================================================

static OSStatus
_GetStreamSecurityKeys(
    AirPlayReceiverSessionRef inSession,
    uint64_t streamConnectionID,
    size_t inInputKeyLen,
    uint8_t* outInputKey,
    size_t inOutputKeyLen,
    uint8_t* outOutputKey)
{
    OSStatus err;
    char* streamKeySaltPtr = NULL;
    size_t streamKeySaltLen = 0;

    require_action(inSession->pairVerifySession, exit, err = kStateErr);

    streamKeySaltLen = asprintf(&streamKeySaltPtr, "%s%llu", kAirPlayPairingDataStreamKeySaltPtr, (unsigned long long)streamConnectionID);

    if (outOutputKey) {
        err = PairingSessionDeriveKey(inSession->pairVerifySession, streamKeySaltPtr, streamKeySaltLen,
            kAirPlayPairingDataStreamKeyOutputInfoPtr, kAirPlayPairingDataStreamKeyOutputInfoLen, inOutputKeyLen, outOutputKey);
        require_noerr(err, exit);
    }

    if (outInputKey) {
        err = PairingSessionDeriveKey(inSession->pairVerifySession, streamKeySaltPtr, streamKeySaltLen,
            kAirPlayPairingDataStreamKeyInputInfoPtr, kAirPlayPairingDataStreamKeyInputInfoLen, inInputKeyLen, outInputKey);
        require_noerr(err, exit);
    }
    err = kNoErr;

exit:
    MemZeroSecure(streamKeySaltPtr, streamKeySaltLen);
    FreeNullSafe(streamKeySaltPtr);
    return (err);
}

//===========================================================================================================================
//	_LogStarted
//===========================================================================================================================

static void _LogStarted(AirPlayReceiverSessionRef inSession, AirPlayReceiverSessionStartInfo* inInfo, OSStatus inStatus)
{
    inSession->startStatus = inStatus;
    inInfo->recordMs = (uint32_t)(UpTicksToMilliseconds(UpTicks() - inSession->playTicks));

    atr_ulog(kLogLevelNotice,
        "%s(%s) %s session started from %##a, %#m\n", AIRPLAY_SDK_REVISION, AIRPLAY_SDK_BUILD, inSession->isRemoteControlSession ? "relay" : "master", &inSession->peerAddr, inStatus);
}

//===========================================================================================================================
//	_LogEnded
//===========================================================================================================================

static void _LogEnded(AirPlayReceiverSessionRef inSession, OSStatus inReason)
{
    DataBuffer db;
#if (TARGET_OS_POSIX)
    char buf[2048];
#else
    char buf[512];
#endif
    uint32_t const durationSecs = (uint32_t)UpTicksToSeconds(UpTicks() - inSession->sessionTicks);
    const AirTunesSource* const ats = &inSession->source;
    uint32_t const retransmitMinMs = NanosecondsToMilliseconds32(ats->retransmitMinNanos);
    uint32_t const retransmitMaxMs = NanosecondsToMilliseconds32(ats->retransmitMaxNanos);
    uint32_t const retransmitAvgMs = NanosecondsToMilliseconds32(ats->retransmitAvgNanos);
    uint32_t const retransmitRetryMinMs = NanosecondsToMilliseconds32(ats->retransmitRetryMinNanos);
    uint32_t const retransmitRetryMaxMs = NanosecondsToMilliseconds32(ats->retransmitRetryMaxNanos);
    uint32_t const ntpRTTMin = (uint32_t)(1000 * ats->rtcpTIClockRTTMin);
    uint32_t const ntpRTTMax = (uint32_t)(1000 * ats->rtcpTIClockRTTMax);
    uint32_t const ntpRTTAvg = (uint32_t)(1000 * ats->rtcpTIClockRTTAvg);

    DataBuffer_Init(&db, buf, sizeof(buf), 10000);
    DataBuffer_AppendF(&db, "AirPlay%s session ended: Dur=%u seconds Reason=%#m",
        inSession->isRemoteControlSession ? " remote control" : "",
        durationSecs, inReason);
    if (!inSession->isRemoteControlSession) {
        DataBuffer_AppendF(&db, "\nGlitches:    %d%%, %d total, %d glitchy minute(s)\n",
            (inSession->glitchTotalPeriods > 0) ? ((inSession->glitchyPeriods * 100) / inSession->glitchTotalPeriods) : 0,
            inSession->glitchTotal, inSession->glitchyPeriods);
        DataBuffer_AppendF(&db, "Retransmits: "
                                "%u sent, %u received, %u futile, %u not found, %u/%u/%u ms min/max/avg, %u/%u ms retry min/max\n",
            ats->retransmitSendCount, ats->retransmitReceiveCount, ats->retransmitFutileCount, ats->retransmitNotFoundCount,
            retransmitMinMs, retransmitMaxMs, retransmitAvgMs, retransmitRetryMinMs, retransmitRetryMaxMs);
        DataBuffer_AppendF(&db, "Packets:     %u lost, %u unrecovered, %u late, %u max burst, %u big losses, %d%% compression\n",
            gAirPlayAudioStats.lostPackets, gAirPlayAudioStats.unrecoveredPackets, gAirPlayAudioStats.latePackets,
            ats->maxBurstLoss, ats->bigLossCount, inSession->compressionPercentAvg / 100);
        DataBuffer_AppendF(&db, "Time Sync:   "
                                "%u/%u/%u ms min/max/avg RTT, %d/%d/%d µS min/max/avg offset, %u step(s)\n",
            ntpRTTMin, ntpRTTMax, ntpRTTAvg,
            (int32_t)(1000000 * ats->rtcpTIClockOffsetMin),
            (int32_t)(1000000 * ats->rtcpTIClockOffsetMax),
            (int32_t)(1000000 * ats->rtcpTIClockOffsetAvg), ats->rtcpTIStepCount);
    }
    atr_ulog(kLogLevelNotice, "%.*s\n", (int)DataBuffer_GetLen(&db), DataBuffer_GetPtr(&db));
    DataBuffer_Free(&db);
}

//===========================================================================================================================
//	_LogUpdate
//===========================================================================================================================

static void _LogUpdate(AirPlayReceiverSessionRef inSession, uint64_t inTicks, Boolean inForce)
{
    (void)inSession;
    (void)inTicks;
    (void)inForce;
}

//===========================================================================================================================
//	_TearDownStream
//===========================================================================================================================

static void _TearDownStream(AirPlayReceiverSessionRef inSession, AirPlayAudioStreamContext* const ctx, Boolean inIsFinalizing)
{
    OSStatus err;

    DEBUG_USE_ONLY(err);

    if (!inIsFinalizing)
        _announcePlaying(inSession, false);
    inSession->source.isPlayingNow = false;
    inSession->source.usingAnchorTime = false;

    if (ctx->threadPtr) {
        err = SendSelfConnectedLoopbackMessage(ctx->cmdSock, "q", 1);
        check_noerr(err);

        err = pthread_join(ctx->thread, NULL);
        check_noerr(err);
        ctx->threadPtr = NULL;
    }
    if (ctx->sendAudioThreadPtr) {
        pthread_mutex_lock(ctx->sendAudioMutexPtr);
        ctx->sendAudioDone = 1;
        pthread_cond_signal(ctx->sendAudioCondPtr);
        pthread_mutex_unlock(ctx->sendAudioMutexPtr);
        pthread_join(ctx->sendAudioThread, NULL);
        ctx->sendAudioThreadPtr = NULL;
        pthread_mutex_forget(&ctx->sendAudioMutexPtr);
        pthread_cond_forget(&ctx->sendAudioCondPtr);
    }
    if (ctx->zeroTimeLockPtr) {
        pthread_mutex_forget(&ctx->zeroTimeLockPtr);
    }
    ForgetSocket(&ctx->cmdSock);
    ForgetSocket(&ctx->dataSock);
    RTPJitterBufferFree(&ctx->jitterBuffer);
    AudioConverterForget(&ctx->inputConverter);
    MirroredRingBufferFree(&ctx->inputRing);

    if (ctx == &inSession->mainAudioCtx) {
        inSession->flushing = false;
        inSession->rtpAudioPort = 0;
        ForgetSocket(&inSession->rtcpSock);
        ForgetMem(&inSession->source.rtcpRTListStorage);
        allocator_finalize(inSession);
        ForgetMem(&inSession->audioPacketBuffer);
        ForgetMem(&inSession->decodeBuffer);
        ForgetMem(&inSession->readBuffer);
        ForgetMem(&inSession->skewAdjustBuffer);
        AudioConverterForget(&inSession->audioConverter);
        inSession->source.receiveCount = 0;

        inSession->busyNodeCount = 0;
        inSession->busyListSentinelStorage.prev = &inSession->busyListSentinelStorage;
        inSession->busyListSentinelStorage.next = &inSession->busyListSentinelStorage;
        inSession->busyListSentinel = &inSession->busyListSentinelStorage;
    }

    ctx->session = NULL;
    ctx->connectionID = 0;
    if (ctx->type != kAirPlayStreamType_Invalid) {
        ctx->type = kAirPlayStreamType_Invalid;
        atr_ulog(kLogLevelTrace, "%s audio torn down\n", ctx->label);
    }
}

#if 0
#pragma mark -
#pragma mark == Sequence Number Math ==
#endif

#define MASK(BITS) ((1 << (BITS)) - 1)

/*!	@function	Seq_LT
    @abstract	Comparison; Returns true if for the sequence numbers a < b.
 */
static Boolean Seq_LT(AirPlayReceiverSessionRef inSession, uint32_t a, uint32_t b)
{
    return inSession->receiveAudioOverTCP ? APMod24_LT(a, b) : // Buffered AirPlay, 24 bit sequence numbers
        Mod16_LT(a, b); // Realtime AirPlay, 16 bit sequence numbers
}

static Boolean Seq_LE(AirPlayReceiverSessionRef inSession, uint32_t a, uint32_t b)
{
    uint32_t mask = inSession->receiveAudioOverTCP ? MASK(24) : MASK(16);

    return (Seq_LT(inSession, a, b) || ((a & mask) == (b & mask)));
}

static uint32_t Seq_Cmp(AirPlayReceiverSessionRef inSession, uint32_t a, uint32_t b)
{
    return inSession->receiveAudioOverTCP ? Mod24_Cmp(a, b) : Mod16_Cmp(a, b);
}

static uint32_t Seq_Inc(AirPlayReceiverSessionRef inSession, uint32_t a)
{
    uint32_t mask = inSession->receiveAudioOverTCP ? MASK(24) : MASK(16);
    return (a + 1) & mask;
}

static uint32_t Seq_Diff(AirPlayReceiverSessionRef inSession, uint32_t a, uint32_t b)
{
    uint32_t mask = inSession->receiveAudioOverTCP ? MASK(24) : MASK(16);
    return (a - b) & mask;
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	APMod24_DiffWrappedAround
 @abstract	Calculates a-b; handles wrap-around if b > a
 
 @param		a					Value to be compared
 b					Value compared against
 */

static uint32_t APMod24_DiffWrappedAround(uint32_t a, uint32_t b)
{
    return (a - b) & MASK(24);
}

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	APMod24_LT
 @abstract	Comparison; Returns true if a < b.
 
 @param		a					Value to be compared
 b					Value compared against
 */

static Boolean APMod24_LT(uint32_t a, uint32_t b)
{
    static const uint32_t HALF_RANGE_24BIT = 1 << 23;
    a &= MASK(24);
    b &= MASK(24);

    // 2 conditions:
    // 		b - a should be < 2^23.
    // 		a != b.
    return ((APMod24_DiffWrappedAround(b, a) < HALF_RANGE_24BIT) && (a != b));
}

#if 0
#pragma mark -
#pragma mark == Helpers ==
#endif

void AirPlayReceiverSessionBumpActivityCount(AirPlayReceiverSessionRef inSession)
{
    if (inSession) {
        _SessionLock(inSession);
        ++inSession->source.activityCount;
        _SessionUnlock(inSession);
    }
}

//===========================================================================================================================
//	AirPlayReceiverSessionChangeModes
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
AirPlayReceiverSessionChangeModes(
    AirPlayReceiverSessionRef inSession,
    const AirPlayModeChanges* inChanges,
    CFStringRef inReason,
    AirPlayReceiverSessionCommandCompletionFunc inCompletion,
    void* inContext)
{
    OSStatus err;
    CFMutableDictionaryRef request;
    CFDictionaryRef params;

    request = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(request, exit, err = kNoMemoryErr);
    CFDictionarySetValue(request, CFSTR(kAirPlayKey_Type), CFSTR(kAirPlayCommand_ChangeModes));

    params = AirPlayCreateModesDictionary(inChanges, inReason, &err);
    require_noerr(err, exit);
    CFDictionarySetValue(request, CFSTR(kAirPlayKey_Params), params);
    CFRelease(params);

    err = AirPlayReceiverSessionSendCommand(inSession, request, inCompletion, inContext);
    require_noerr(err, exit);

exit:
    CFReleaseNullSafe(request);
    return (err);
}

//===========================================================================================================================
//	AirPlayReceiverSessionChangeAppState
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
AirPlayReceiverSessionChangeAppState(
    AirPlayReceiverSessionRef inSession,
    AirPlaySpeechMode inSpeechMode,
    AirPlayTriState inPhoneCall,
    AirPlayTriState inTurnByTurn,
    CFStringRef inReason,
    AirPlayReceiverSessionCommandCompletionFunc inCompletion,
    void* inContext)
{
    OSStatus err;
    AirPlayModeChanges changes;

    AirPlayModeChangesInit(&changes);
    if (inSpeechMode != kAirPlaySpeechMode_NotApplicable)
        changes.speech = inSpeechMode;
    if (inPhoneCall != kAirPlayTriState_NotApplicable)
        changes.phoneCall = inPhoneCall;
    if (inTurnByTurn != kAirPlayTriState_NotApplicable)
        changes.turnByTurn = inTurnByTurn;

    err = AirPlayReceiverSessionChangeModes(inSession, &changes, inReason, inCompletion, inContext);
    require_noerr(err, exit);

exit:
    return (err);
}

//===========================================================================================================================
//	AirPlayReceiverSessionChangeResourceMode
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
AirPlayReceiverSessionChangeResourceMode(
    AirPlayReceiverSessionRef inSession,
    AirPlayResourceID inResourceID,
    AirPlayTransferType inType,
    AirPlayTransferPriority inPriority,
    AirPlayConstraint inTakeConstraint,
    AirPlayConstraint inBorrowOrUnborrowConstraint,
    CFStringRef inReason,
    AirPlayReceiverSessionCommandCompletionFunc inCompletion,
    void* inContext)
{
    OSStatus err;
    AirPlayModeChanges changes;

    AirPlayModeChangesInit(&changes);
    if (inResourceID == kAirPlayResourceID_MainScreen) {
        changes.screen.type = inType;
        changes.screen.priority = inPriority;
        changes.screen.takeConstraint = inTakeConstraint;
        changes.screen.borrowOrUnborrowConstraint = inBorrowOrUnborrowConstraint;
    } else if (inResourceID == kAirPlayResourceID_MainAudio) {
        changes.mainAudio.type = inType;
        changes.mainAudio.priority = inPriority;
        changes.mainAudio.takeConstraint = inTakeConstraint;
        changes.mainAudio.borrowOrUnborrowConstraint = inBorrowOrUnborrowConstraint;
    } else {
        dlogassert("Bad resource ID: %d\n", inResourceID);
        err = kParamErr;
        goto exit;
    }

    err = AirPlayReceiverSessionChangeModes(inSession, &changes, inReason, inCompletion, inContext);
    require_noerr(err, exit);

exit:
    return (err);
}

//===========================================================================================================================
//	AirPlayReceiverSessionMakeModeStateFromDictionary
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
AirPlayReceiverSessionMakeModeStateFromDictionary(
    AirPlayReceiverSessionRef inSession,
    CFDictionaryRef inDict,
    AirPlayModeState* outModes)
{
    OSStatus err;
    CFArrayRef array;
    CFIndex i, n;
    CFDictionaryRef dict;
    int x;

    (void)inSession;

    AirPlayModeStateInit(outModes);

    // AppStates

    array = CFDictionaryGetCFArray(inDict, CFSTR(kAirPlayKey_AppStates), NULL);
    n = array ? CFArrayGetCount(array) : 0;
    for (i = 0; i < n; ++i) {
        dict = CFArrayGetCFDictionaryAtIndex(array, i, &err);
        require_noerr(err, exit);

        x = (int)CFDictionaryGetInt64(dict, CFSTR(kAirPlayKey_AppStateID), NULL);
        switch (x) {
        case kAirPlayAppStateID_PhoneCall:
            x = (int)CFDictionaryGetInt64(dict, CFSTR(kAirPlayKey_Entity), &err);
            require_noerr(err, exit);
            outModes->phoneCall = x;
            break;

        case kAirPlayAppStateID_Speech:
            x = (int)CFDictionaryGetInt64(dict, CFSTR(kAirPlayKey_Entity), &err);
            require_noerr(err, exit);
            outModes->speech.entity = x;

            x = (int)CFDictionaryGetInt64(dict, CFSTR(kAirPlayKey_SpeechMode), &err);
            require_noerr(err, exit);
            outModes->speech.mode = x;
            break;

        case kAirPlayAppStateID_TurnByTurn:
            x = (int)CFDictionaryGetInt64(dict, CFSTR(kAirPlayKey_Entity), &err);
            require_noerr(err, exit);
            outModes->turnByTurn = x;
            break;

        case kAirPlayAppStateID_NotApplicable:
            break;

        default:
            atr_ulog(kLogLevelNotice, "### Ignoring unknown app state %@\n", dict);
            break;
        }
    }

    // Resources

    array = CFDictionaryGetCFArray(inDict, CFSTR(kAirPlayKey_Resources), NULL);
    n = array ? CFArrayGetCount(array) : 0;
    for (i = 0; i < n; ++i) {
        dict = CFArrayGetCFDictionaryAtIndex(array, i, &err);
        require_noerr(err, exit);

        x = (AirPlayAppStateID)CFDictionaryGetInt64(dict, CFSTR(kAirPlayKey_ResourceID), NULL);
        switch (x) {
        case kAirPlayResourceID_MainScreen:
            x = (int)CFDictionaryGetInt64(dict, CFSTR(kAirPlayKey_Entity), &err);
            require_noerr(err, exit);
            outModes->screen = x;
            break;

        case kAirPlayResourceID_MainAudio:
            x = (int)CFDictionaryGetInt64(dict, CFSTR(kAirPlayKey_Entity), &err);
            require_noerr(err, exit);
            outModes->mainAudio = x;
            break;

        case kAirPlayResourceID_NotApplicable:
            break;

        default:
            atr_ulog(kLogLevelNotice, "### Ignoring unknown resource state %@\n", dict);
            break;
        }
    }
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	AirPlayReceiverSessionRequestSiriAction
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
AirPlayReceiverSessionRequestSiriAction(
    AirPlayReceiverSessionRef inSession,
    AirPlaySiriAction inAction,
    AirPlayReceiverSessionCommandCompletionFunc inCompletion,
    void* inContext)
{
    OSStatus err;
    CFMutableDictionaryRef request;
    CFMutableDictionaryRef params;

    request = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(request, exit, err = kNoMemoryErr);
    CFDictionarySetValue(request, CFSTR(kAirPlayKey_Type), CFSTR(kAirPlayCommand_RequestSiri));

    params = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(params, exit, err = kNoMemoryErr);
    CFDictionarySetInt64(params, CFSTR(kAirPlayKey_SiriAction), (int64_t)inAction);
    CFDictionarySetValue(request, CFSTR(kAirPlayKey_Params), params);
    CFRelease(params);

    err = AirPlayReceiverSessionSendCommand(inSession, request, inCompletion, inContext);
    require_noerr(err, exit);

exit:
    CFReleaseNullSafe(request);
    return (err);
}

//===========================================================================================================================
//	AirPlayReceiverSessionRequestUI
//===========================================================================================================================

EXPORT_GLOBAL
OSStatus
AirPlayReceiverSessionRequestUI(
    AirPlayReceiverSessionRef inSession,
    CFStringRef inURL,
    AirPlayReceiverSessionCommandCompletionFunc inCompletion,
    void* inContext)
{
    OSStatus err;
    CFMutableDictionaryRef request;
    CFMutableDictionaryRef params;

    request = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(request, exit, err = kNoMemoryErr);
    CFDictionarySetValue(request, CFSTR(kAirPlayKey_Type), CFSTR(kAirPlayCommand_RequestUI));

    if (inURL) {
        params = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require_action(params, exit, err = kNoMemoryErr);
        CFDictionarySetValue(params, CFSTR(kAirPlayKey_URL), inURL);
        CFDictionarySetValue(request, CFSTR(kAirPlayKey_Params), params);
        CFRelease(params);
    }

    err = AirPlayReceiverSessionSendCommand(inSession, request, inCompletion, inContext);
    require_noerr(err, exit);

exit:
    CFReleaseNullSafe(request);
    return (err);
}

//===========================================================================================================================
//	AirPlayReceiverSessionSetNightMode
//===========================================================================================================================

OSStatus
AirPlayReceiverSessionSetNightMode(
    AirPlayReceiverSessionRef inSession,
    Boolean inNightMode,
    AirPlayReceiverSessionCommandCompletionFunc inCompletion,
    void* inContext)
{
    OSStatus err;
    CFMutableDictionaryRef request;
    CFMutableDictionaryRef params;

    request = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(request, exit, err = kNoMemoryErr);
    CFDictionarySetValue(request, CFSTR(kAirPlayKey_Type), CFSTR(kAirPlayCommand_SetNightMode));

    params = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(params, exit, err = kNoMemoryErr);
    CFDictionarySetValue(params, CFSTR(kAirPlayKey_NightMode), inNightMode ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(request, CFSTR(kAirPlayKey_Params), params);
    CFRelease(params);

    err = AirPlayReceiverSessionSendCommand(inSession, request, inCompletion, inContext);
    require_noerr(err, exit);

exit:
    CFReleaseNullSafe(request);
    return (err);
}

//===========================================================================================================================
//	AirPlayReceiverSessionSetLimitedUI
//===========================================================================================================================

OSStatus
AirPlayReceiverSessionSetLimitedUI(
    AirPlayReceiverSessionRef inSession,
    Boolean inLimitUI,
    AirPlayReceiverSessionCommandCompletionFunc inCompletion,
    void* inContext)
{
    OSStatus err;
    CFMutableDictionaryRef request;
    CFMutableDictionaryRef params;

    request = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(request, exit, err = kNoMemoryErr);
    CFDictionarySetValue(request, CFSTR(kAirPlayKey_Type), CFSTR(kAirPlayCommand_SetLimitedUI));

    params = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(params, exit, err = kNoMemoryErr);
    CFDictionarySetValue(params, CFSTR(kAirPlayKey_LimitedUI), inLimitUI ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(request, CFSTR(kAirPlayKey_Params), params);
    CFRelease(params);

    err = AirPlayReceiverSessionSendCommand(inSession, request, inCompletion, inContext);
    require_noerr(err, exit);

exit:
    CFReleaseNullSafe(request);
    return (err);
}

//===========================================================================================================================
//	AirPlayReceiverSessionSendHIDReport
//===========================================================================================================================

OSStatus
AirPlayReceiverSessionSendHIDReport(
    AirPlayReceiverSessionRef inSession,
    uint32_t deviceUID,
    const uint8_t* inPtr,
    size_t inLen)
{
    OSStatus err;
    CFMutableDictionaryRef request;
    CFStringRef uid;

    uid = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%X"), deviceUID);
    require_action(uid, exit, err = kNoMemoryErr);

    request = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(request, exit, err = kNoMemoryErr);
    CFDictionarySetValue(request, CFSTR(kAirPlayKey_Type), CFSTR(kAirPlayCommand_HIDSendReport));
    CFDictionarySetValue(request, CFSTR(kAirPlayKey_UUID), uid);
    CFRelease(uid);
    CFDictionarySetData(request, CFSTR(kAirPlayKey_HIDReport), inPtr, inLen);

    err = AirPlayReceiverSessionSendCommand(inSession, request, NULL, NULL);
    CFRelease(request);
    require_noerr(err, exit);

exit:
    return (err);
}

static OSStatus
_SessionGenerateResponseStreamDesc(
    AirPlayReceiverSessionRef inSession,
    CFMutableDictionaryRef inResponseParams,
    uint32_t inStreamType,
    uint64_t inStreamID)
{
    OSStatus err = kNoErr;
    CFMutableDictionaryRef responseStreamDesc = NULL;

    require_action(inSession, bail, err = kParamErr);
    require_action(inResponseParams, bail, err = kParamErr);

    responseStreamDesc = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(responseStreamDesc, bail, err = kNoMemoryErr);

    CFDictionarySetInt64(responseStreamDesc, CFSTR(kAirPlayKey_Type), inStreamType);
    CFDictionarySetInt64(responseStreamDesc, CFSTR(kAirPlayKey_StreamID), inStreamID);

    err = _AddResponseStream(inResponseParams, responseStreamDesc);
    require_noerr(err, bail);

bail:
    CFReleaseNullSafe(responseStreamDesc);
    return (err);
}

static void notifyPlatformOfSender(AirPlayReceiverSessionRef inSession)
{
    AirPlayNetworkAddressRef addrRef = NULL;
    CFStringRef addressString = NULL;
    OSStatus err;

    err = AirPlayNetworkAddressCreateWithSocketAddr(kCFAllocatorDefault, inSession->peerAddr, &addrRef);
    require_noerr(err, exit);

    err = AirPlayNetworkAddressCopyStringRepresentation(addrRef, &addressString);
    require_noerr(err, exit);

    (void)AirPlayReceiverSessionSetProperty(inSession, kCFObjectFlagAsync, CFSTR(kAirPlayProperty_ClientIP), NULL, addressString);

exit:
    CFReleaseNullSafe(addressString);
    CFReleaseNullSafe(addrRef);
}

// We'll round our allocations to pointer sized byte boundaries.
#define ROUNDUP_PTR_SIZE(aSize) (((aSize) + (sizeof(void*) - 1)) & ~(sizeof(void*) - 1))

static void allocator_setup(AirPlayReceiverSessionRef inSession, size_t bufferPoolSize, size_t fixedPacketSize)
{
    size_t fixedBlockSize = ROUNDUP_PTR_SIZE(sizeof(AirTunesBufferNode) + fixedPacketSize);

    /* Realtime AirPlay needs to use the fixedAllocator since it will release
	 * buffers out of order. Buffered AirPlay can use the fixedAllocator or
	 * the ringAllocator. The ringAllocator makes better use of memory.
	 */
    inSession->blockAllocator = inSession->receiveAudioOverTCP ? ringAllocator : fixedAllocator;

    inSession->blockAllocContext = inSession->blockAllocator.init(bufferPoolSize, fixedBlockSize);

    inSession->audioBytesPoolSize = bufferPoolSize;
    inSession->audioBytesPoolUsed = 0;
}

static void allocator_finalize(AirPlayReceiverSessionRef inSession)
{
    require_quiet(inSession->blockAllocContext, exit);
    inSession->blockAllocator.finalize(inSession->blockAllocContext);
    inSession->blockAllocContext = NULL;

exit:
    memset(&inSession->blockAllocator, 0, sizeof(inSession->blockAllocator));
}

AirTunesBufferNode* allocator_getNewNode(AirPlayReceiverSessionRef inSession, size_t packetSize)
{
    // Add room for the AirTunesBufferNode at the start of the block
    size_t nodeSize = ROUNDUP_PTR_SIZE(sizeof(AirTunesBufferNode) + packetSize);
    AirTunesBufferNode* node = inSession->blockAllocator.get(inSession->blockAllocContext, nodeSize);
    require_quiet(node, exit);

    node->next = node->prev = NULL;
    node->data = (void*)node + sizeof(AirTunesBufferNode);
    node->allocatedSize = nodeSize;

    inSession->audioBytesPoolUsed += nodeSize;

exit:
    return node;
}

void allocator_freeNode(AirPlayReceiverSessionRef inSession, AirTunesBufferNode* node)
{
    require(node, exit);

    AirTunesBufferNode* next = node->next;
    AirTunesBufferNode* prev = node->prev;

    // Try to free the node - it might fail.
    Boolean freeSuccessful = inSession->blockAllocator.free(inSession->blockAllocContext, node, node->allocatedSize);

    if (freeSuccessful) {

        /* If the node is in the busy doubly-linked list then
		 * unlink it.
		 */
        if (next) {
            check(node->prev);

            next->prev = prev;
            prev->next = next;
            --inSession->busyNodeCount;
        }
        inSession->audioBytesPoolUsed -= node->allocatedSize;

        /* If we couldn't free the node right now then mark the size as 0 so
	 * it doesn't play any audio. The node will be freed when the render
	 * thread gets to it.
	 */
    } else {
        inSession->zombieNodeCount++;
        node->size = 0;
    }

exit:
    return;
}
