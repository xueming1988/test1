//
//  AirPlayReceiverMetrics.h
//  AirPlayLP-mac
//
//  Copyright © 2018 Apple Inc. All rights reserved.
//

#ifndef AirPlayReceiverMatrics_h
#define AirPlayReceiverMatrics_h

#include "AirPlayCommon.h"
#include "AirPlayReceiverServer.h"
#include "AirPlayReceiverSession.h"
#include "CFCompat.h"
#include "CommonServices.h"
#include <stdio.h>

#include "HTTPClient.h"
#include "HTTPMessage.h"

#if (AIRPLAY_METRICS)

// [Dictionary] Collection of keys for performance, error, etc. information.
#define kAirPlayProperty_Metrics "metrics"
#define kAirPlayProperty_MetricsHistory "metrics_history"

// Keys in the kAirPlayProperty_Metrics dictionary
#define kAirPlayMetricKey_Status "stat" // [Number:OSStatus] 0 = success, non-zero = failure reason.
#define kAirPlayMetricKey_Volume "volm" // [Number] Volume in DB
#define kAirPlayMetricKey_Rate "rate" // [Number] 0 = not playing, non-zero = playing. (Buffered Only)
#define kAirPlayMetricKey_TransportType "tpty" // [Number:NetTransportType] Ethernet, WiFi, etc.
#define kAirPlayMetricKey_AudioFormat "audf" // [Number] audio stream's format
#define kAirPlayMetricKey_DurationSecs "durs" // [Number] How many seconds the session lasted. Truncated to 30 seconds.
#define kAirPlayMetricKey_StartupMSecs "strs" // [Number] Milliseconds between when the session was created and playback started.
#define kAirPlayMetricKey_GlitchPeriods "glch" // [Number] Number of 1 minute periods with glitches.
#define kAirPlayMetricKey_Glitches "glct" // [Number] Number of glitches in session.
#define kAirPlayMetricKey_LostPackets "lost" // [Number] Number of lost audio packets.
#define kAirPlayMetricKey_UnrecoveredPackets "unre" // [Number] Number of unrecovered audio packets.
#define kAirPlayMetricKey_LatePackets "late" // [Number] Number of late audio lackets.
#define kAirPlayMetricKey_TotalNodes "nodN" // [Number] Number of buffer nodes allocated
#define kAirPlayMetricKey_BusyNodes "nodB" // [Number] Number of buffer nodes that are filled
#define kAirPlayMetricKey_NodeSize "nodS" // [Number] Size in bytes of each node
#define kAirPlayMetricKey_ZombieNodes "nodZ" // [Number] Number of nodes that couldn't be freed
#define kAirPlayMetricKey_RemoteConnectionsCount "rmCn" // [Number] Number of remote connections open
#define kAirPlayMetricKey_RemoteCommandsCount "rmCt" // [Number] Number of remote commands processed
#define kAirPlayMetricKey_AvgBuffer "bfms" // [Number] ms. of audio in buffers
#define kAirPlayMetricKey_AudioBytesPoolSize "abps" // [Number] Audio pool size in bytes
#define kAirPlayMetricKey_AudioBytesPoolUsed "abpu" // [Number] Bytes used in the audio pool
#define kAirPlayMetricKey_CompressionPercent "acmp" // [Number] Audio compression percent
#define kAirPlayMetricKey_Mode "mode" // [String] Session's AirPlay mode
#define kAirPlayMetricKey_ClockingMethod "clck" // [String] Session's clocking
#define kAirPlayMetricKey_ClockTimeline "cltl" // [Number] timeline for network clock
#define kAirPlayMetricKey_ClockTimeSecs "clts" // [Number] network clock time - secs
#define kAirPlayMetricKey_ClockTimelineFracs "cltf" // [Number] network clock time - fract secs
#define kAirPlayMetricKey_Skew "skew" // [Number] Measured skew
#define kAirPlayMetricKey_SamplesPerSkewAdjust "sadj" // [Number] Samples per skew adjustment
#define kAirPlayMetricKey_SkewResetCount "srnt" // [Number] Times skew compensation has been reset
#define kAirPlayMetricKey_ClientName "clnt" // [String] [String] Client name from RECORD
#define kAirPlayMetricKey_ClientSessionID "csid" // [Number] Client’s session id

#define kAirPlayMetricKey_RTPOffset "rpOF" // [Number] RTP Offset
#define kAirPlayMetricKey_lastPacketTS "pkTS" // [Number] Last timestamp taken from a render packet
#define kAirPlayMetricKey_lastRenderPacketTS "rlTS" // [Number] Time stamp for the last packet seen in render thread

#define kAirPlayMetricKey_lastRenderGapStartTS "rnTS" // [Number] Render thread looking for a packet TS >= this
#define kAirPlayMetricKey_lastRenderGapLimitTS "rnLM" // [Number] Render thread looking for a packet TS < this

#define kAirPlayMetricKey_AnchorTime "anTm" // [Number] Time from last SetRateAndAnchorTime
#define kAirPlayMetricKey_AnchorTimeline "antl" // [Number] Anchor timeline ID
#define kAirPlayMetricKey_AnchorRTP "anRP" // [Number] RTP from last SetRateAndAnchorTime

#define kAirPlayMetricKey_lastNetRTP "lNTP" // [Number] Last RTP received off the network.
#define kAirPlayMetricKey_lastNetSeq "lNSq" // [Number] Last sequence number received off the network.

void AirPlayReceiverMetricsUpdateTimeStamp(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest);
HTTPStatus AirPlayReceiverMetrics(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest);
HTTPStatus AirPlayReceiverHistory(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest);
HTTPStatus AirPlayReceiverMetricsProcess(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest);
void AirPlayReceiverMetricsSupplementTimeStampF(AirPlayReceiverConnectionRef inCnx, const char* inFormat, ...);
void AirPlayReceiverMetricsSupplementTimeStamp(AirPlayReceiverConnectionRef inCnx, CFTypeRef supplement);

#endif

HTTPStatus AirPlayReceiverMetricsSendJSON(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest, CFDictionaryRef dict);

#endif /* AirPlayReceiverMatrics_h */
