/*
	File:    AirPlayMetrics.h
	Package: AirPlayAudioPOSIXReceiver
	Version: AirPlay_Audio_POSIX_Receiver_211.1.p8
	
	Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
	capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
	Apple software is governed by and subject to the terms and conditions of your MFi License,
	including, but not limited to, the restrictions specified in the provision entitled ”Public
	Software”, and is further subject to your agreement to the following additional terms, and your
	agreement that the use, installation, modification or redistribution of this Apple software
	constitutes acceptance of these additional terms. If you do not agree with these additional terms,
	please do not use, install, modify or redistribute this Apple software.
	
	Subject to all of these terms and in consideration of your agreement to abide by them, Apple grants
	you, for as long as you are a current and in good-standing MFi Licensee, a personal, non-exclusive
	license, under Apple's copyrights in this original Apple software (the "Apple Software"), to use,
	reproduce, and modify the Apple Software in source form, and to use, reproduce, modify, and
	redistribute the Apple Software, with or without modifications, in binary form. While you may not
	redistribute the Apple Software in source form, should you redistribute the Apple Software in binary
	form, you must retain this notice and the following text and disclaimers in all such redistributions
	of the Apple Software. Neither the name, trademarks, service marks, or logos of Apple Inc. may be
	used to endorse or promote products derived from the Apple Software without specific prior written
	permission from Apple. Except as expressly stated in this notice, no other rights or licenses,
	express or implied, are granted by Apple herein, including but not limited to any patent rights that
	may be infringed by your derivative works or by other works in which the Apple Software may be
	incorporated.
	
	Unless you explicitly state otherwise, if you provide any ideas, suggestions, recommendations, bug
	fixes or enhancements to Apple in connection with this software (“Feedback”), you hereby grant to
	Apple a non-exclusive, fully paid-up, perpetual, irrevocable, worldwide license to make, use,
	reproduce, incorporate, modify, display, perform, sell, make or have made derivative works of,
	distribute (directly or indirectly) and sublicense, such Feedback in connection with Apple products
	and services. Providing this Feedback is voluntary, but if you do provide Feedback to Apple, you
	acknowledge and agree that Apple may exercise the license granted above without the payment of
	royalties or further consideration to Participant.
	
	The Apple Software is provided by Apple on an "AS IS" basis. APPLE MAKES NO WARRANTIES, EXPRESS OR
	IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY
	AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR
	IN COMBINATION WITH YOUR PRODUCTS.
	
	IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
	PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION
	AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
	(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
	
	Copyright (C) 2012-2013 Apple Inc. All Rights Reserved.
*/

#ifndef	__AirPlayMetrics_h__
#define	__AirPlayMetrics_h__

#include "AirPlayCommon.h"
#include "APSCommonServices.h"
#include "APSDebugServices.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark == General ==
#endif

//===========================================================================================================================
//	General
//===========================================================================================================================

#if 0
#pragma mark == Audio ==
#endif

//===========================================================================================================================
//	AirPlay Audio
//===========================================================================================================================

// Note: These keys are exchanged over the network so don't change them or it'll break backward compatibility.

#define kAirPlayMetricKey_Status					"stat" // [Number:OSStatus] 0 = success, non-zero = failure reason.
#define kAirPlayMetricKey_TransportType				"tpty" // [Number:NetTransportType] Ethernet, WiFi, etc.
#define kAirPlayMetricKey_DurationSecs				"durs" // [Number] How many seconds the session lasted. Truncated to 30 seconds.
#define kAirPlayMetricKey_CompressionPercent		"cmpp" // [Number] Compressed sized compared to original (70 means 70% of the original).
#define kAirPlayMetricKey_Glitches					"glch" // [Number] Number of 1 minute periods with glitches.
#define kAirPlayMetricKey_RetransmitSends			"rtsn" // [Number] Number of retransmit requests we sent.
#define kAirPlayMetricKey_RetransmitReceives		"rtrc" // [Number] Number of retransmit responses we've received.
#define kAirPlayMetricKey_FutileRetransmits			"rtfu" // [Number] Number of futile retransmit responses we've received.
#define kAirPlayMetricKey_RetransmitNotFounds		"rtnf" // [Number] Number of retransmit responses received without a request (late).
#define kAirPlayMetricKey_RetransmitMinMs			"rtmn" // [Number] Min milliseconds for a retransmit response.
#define kAirPlayMetricKey_RetransmitMaxMs			"rtmx" // [Number] Max milliseconds for a retransmit response.
#define kAirPlayMetricKey_RetransmitAvgMs			"rtav" // [Number] Average milliseconds for a retransmit response.
#define kAirPlayMetricKey_RetransmitRetryMinMs		"rtrm" // [Number] Min milliseconds for a retransmit response after a retry.
#define kAirPlayMetricKey_RetransmitRetryMaxMs		"rtrx" // [Number] Max milliseconds for a retransmit response after a retry.
#define kAirPlayMetricKey_LostPackets				"lost" // [Number] Number of lost audio packets.
#define kAirPlayMetricKey_UnrecoveredPackets		"unre" // [Number] Number of unrecovered audio packets.
#define kAirPlayMetricKey_LatePackets				"late" // [Number] Number of late audio lackets.
#define kAirPlayMetricKey_MaxBurstLoss				"mxbr" // [Number] Largest contiguous number of lost packets
#define kAirPlayMetricKey_BigLosses					"bigl" // [Number] Number of times we abort retransmits requests because the loss was too big.
#define kAirPlayMetricKey_NTPRTTMin					"nrmn" // [Number] Minimum RTT for NTP exchanges.
#define kAirPlayMetricKey_NTPRTTMax					"nrmx" // [Number] Maximum RTT for NTP exchanges.
#define kAirPlayMetricKey_NTPRTTAvg					"nrav" // [Number] Average RTT for NTP exchanges.
#define kAirPlayMetricKey_NTPOffsetMin				"nomn" // [Number] Minimum clock offset for NTP exchanges.
#define kAirPlayMetricKey_NTPOffsetMax				"nomx" // [Number] Maximum clock offset for NTP exchanges.
#define kAirPlayMetricKey_NTPOffsetAvg				"noav" // [Number] Average clock offset for NTP exchanges.
#define kAirPlayMetricKey_NTPOutliers				"nout" // [Number] Number of NTP exchanges with RTT's too large to use.
#define kAirPlayMetricKey_NTPSteps					"nstp" // [Number] Number of times the NTP clock was stepped (excludes the initial step).
#define kAirPlayMetricKey_RTPMaxSkew				"rmxs" // [Number] Max RTP timestamp skew.
#define kAirPlayMetricKey_RTPSkewResets				"rskr" // [Number] Number of timeline resets due to too much skew.
#define kAirPlayMetricKey_DACPPauses				"dpau" // [Number] Number of times the user pressed pause on the IR remote.
#define kAirPlayMetricKey_DACPNext					"dnxt" // [Number] Number of times the user pressed next on the IR remote.
#define kAirPlayMetricKey_DACPPrevious				"dprv" // [Number] Number of times the user pressed previous on the IR remote.

typedef struct
{
	const uint8_t *	sessionUUID;			// Random UUID generated for each session to associate sessions.
	int32_t			status;					// [OSStatus] 0 = success, non-zero = failure reason.
	uint32_t		transportType;			// Transport of client (e.g. Ethernet, WiFi, etc.).
	uint32_t		compressionType;		// [AirPlayCompressionType] Type of audio compressed used.
	uint32_t		encryptionType;			// [AirPlayEncryptionType] Type of audio encryption used.
	uint32_t		bonjourMs;				// Milliseconds to resolve SRV, A/AAAA via Bonjour.
	uint32_t		connectMs;				// Milliseconds to connect.
	uint32_t		authMs;					// Milliseconds to authenticate after connecting.
	uint32_t		announceMs;				// Milliseconds for RTSP announce.
	uint32_t		setupAudioMs;			// Milliseconds for RTSP setup for audio.
	uint32_t		recordMs;				// Milliseconds for RTSP record.
	const char *	serverModel;			// Server device (e.g. "Device1,1").
	const char *	serverVersion;			// Server AirPlay source version (e.g. "180.58").
	uint32_t		latencyMs;				// Milliseconds of latency the client is targeting.
	
}	AirPlayAudioSessionStartedOnClientParams;

typedef struct
{
	const uint8_t *	sessionUUID;			// Random UUID generated for each session to associate sessions.
	int32_t			reason;					// [OSStatus] Reason session ended.
	uint32_t		durationSecs;			// How many seconds the session lasted. Truncated to 30 seconds.
	uint32_t		slowKeepAlives;			// Number of keep alives that were deferred because a previous one hadn't completed.
	uint32_t		retransmits;			// Number of retransmits that occurred during the session.
	uint32_t		futileRetransmits;		// Number of futile retransmits that occurred during the session.
	
}	AirPlayAudioSessionEndedOnClientParams;

typedef struct
{
	const uint8_t *	sessionUUID;			// Random UUID generated for each session to associate sessions.
	int32_t			status;					// [OSStatus] 0 = success, non-zero = failure reason.
	uint32_t		transportType;			// Transport of server (e.g. Ethernet, WiFi, etc.).
	const char *	clientModel;			// Model of client device (e.g. "Device1,1").
	const char *	clientVersion;			// Build version of client device (e.g. "10A362").
	uint32_t		compressionType;		// [AirPlayCompressionType] Type of audio compressed used.
	uint32_t		latencyMs;				// Milliseconds of latency the client is targeting.
	uint32_t		clientBonjourMs;		// Milliseconds the client took to resolve SRV, A/AAAA via Bonjour.
	uint32_t		clientConnectMs;		// Milliseconds the client took to connect.
	uint32_t		clientAuthMs;			// Milliseconds the client took to authenticate after connecting.
	uint32_t		clientAnnounceMs;		// Milliseconds the client took for RTSP announce.
	uint32_t		clientSetupAudioMs;		// Milliseconds the client took for RTSP setup for audio.
	uint32_t		serverRecordMs;			// Milliseconds for RTSP record (includes NTP negotiation).
	
}	AirPlayAudioSessionStartedOnServerParams;

typedef struct
{
	const uint8_t *	sessionUUID;			// Random UUID generated for each session to associate sessions.
	int32_t			reason;					// [OSStatus] Reason session ended.
	uint32_t		durationSecs;			// How many seconds the session lasted. Truncated to 30 seconds.
	uint32_t		compressionPercent;		// Compressed sized compared to original (70 means 70% of the original).
	uint32_t		glitches;				// Number of 1 minute periods with glitches.
	uint32_t		retransmitSends;		// Number of retransmit requests we sent.
	uint32_t		retransmitReceives;		// Number of retransmit responses we've received.
	uint32_t		futileRetransmits;		// Number of futile retransmit responses we've received.
	uint32_t		retransmitNotFounds;	// Number of retransmit responses received without a request (late).
	uint32_t		retransmitMinMs;		// Min milliseconds for a retransmit response.
	uint32_t		retransmitMaxMs;		// Max milliseconds for a retransmit response.
	uint32_t		retransmitAvgMs;		// Average milliseconds for a retransmit response.
	uint32_t		retransmitRetryMinMs;	// Min milliseconds for a retransmit response after a retry.
	uint32_t		retransmitRetryMaxMs;	// Max milliseconds for a retransmit response after a retry.
	uint32_t		lostPackets;			// Number of lost audio packets.
	uint32_t		unrecoveredPackets;		// Number of unrecovered audio packets.
	uint32_t		latePackets;			// Number of late audio lackets.
	uint32_t		maxBurstLoss;			// Largest contiguous number of lost packets
	uint32_t		bigLosses;				// Number of times we abort retransmits requests because the loss was too big.
	uint32_t		ntpRTTMin;				// Minimum RTT for NTP exchanges.
	uint32_t		ntpRTTMax;				// Maximum RTT for NTP exchanges.
	uint32_t		ntpRTTAvg;				// Average RTT for NTP exchanges.
	uint32_t		ntpOffsetMin;			// Minimum clock offset for NTP exchanges.
	uint32_t		ntpOffsetMax;			// Maximum clock offset for NTP exchanges.
	uint32_t		ntpOffsetAvg;			// Average clock offset for NTP exchanges.
	uint32_t		ntpOutliers;			// Number of NTP exchanges with RTT's too large to use.
	uint32_t		ntpSteps;				// Number of times the NTP clock was stepped (excludes the initial step).
	uint32_t		rtpMaxSkew;				// Max RTP timestamp skew.
	uint32_t		rtpSkewResets;			// Number of timeline resets due to too much skew.
	uint32_t		dacpPauses;				// Number of times the user pressed pause on the IR remote.
	uint32_t		dacpNext;				// Number of times the user pressed next on the IR remote.
	uint32_t		dacpPrevious;			// Number of times the user pressed previous on the IR remote.
	
}	AirPlayAudioSessionEndedOnServerParams;

#ifdef __cplusplus
}
#endif

#endif	// __AirPlayMetrics_h__
