/*
	Copyright (C) 2012-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef __AirPlayReceiverServerPriv_h__
#define __AirPlayReceiverServerPriv_h__

#include "AirPlayCommon.h"
#include "AirPlayReceiverServer.h"
#include "AirPlaySessionManager.h"

#include CF_HEADER
#include CF_RUNTIME_HEADER
#include "dns_sd.h"
#include LIBDISPATCH_HEADER

#include "CommonServices.h"
#include "DataBufferUtils.h"
#include "DebugServices.h"
#include "HTTPServer.h"
#if (AIRPLAY_DACP)
#include "AirTunesDACP.h"
#endif

#if (TARGET_OS_POSIX)
#include "NetworkChangeListener.h"
#endif
#include "MFiSAP.h"
#include "PairingUtils.h"

#ifdef __cplusplus
extern "C" {
#endif

//===========================================================================================================================
//	Server Internals
//===========================================================================================================================

typedef struct {
    CFStringRef uuid; // UUID (system pairing identity) of the AirPlay group owner
    Boolean containsLeader; // specifies whether a current AirPlay group contains a speaker group leader
    Boolean supportsRelay;
    Boolean isGroupLeader;
} AirPlayGrouping;

struct AirPlayReceiverServerPrivate {
    CFRuntimeBase base; // CF type info. Must be first.
    void* platformPtr; // Pointer to the platform-specific data.
    dispatch_queue_t queue; // Internal queue used by the server.
    AirPlayReceiverServerDelegate delegate; // Hooks for delegating functionality to external code.

    // Bonjour
    Boolean bonjourRestartPending; // True if we're waiting until playback stops to restart Bonjour.
    uint64_t bonjourStartTicks; // Time when Bonjour was successfully started.
    dispatch_source_t bonjourRetryTimer; // Timer to retry Bonjour registrations if they fail.
    DNSServiceRef bonjourAirPlay; // _airplay._tcp Bonjour service.
#if (AIRPLAY_RAOP_SERVER)
    DNSServiceRef bonjourRAOP; // _raop._tcp Bonjour service for compatibility with older senders.
#endif

    AirPlayGrouping group; // Describe the current grouping configuration.
    AirPlaySessionManagerRef sessionMgr;

    // Servers

    HTTPServerRef httpServer; // HTTP server
    dispatch_queue_t httpQueue; // Internal queue used by the http servers and connections.
#if (AIRPLAY_HTTP_SERVER_LEGACY)
    HTTPServerRef httpServerLegacy; // HTTP server to support legacy BTLE clients that use hardcoded port 5000.
#endif
    uint8_t httpTimedNonceKey[16];

#if (AIRPLAY_VOLUME_CONTROL)
    Float32 volume;
#endif
    char pinFixed[8]; // If non-empty, use a fixed PIN (for QA automation).
    Boolean pinMode; // True if we're using a new, random PIN for each AirPlay session.
    Boolean playing; // True if we're currently playing.
    Boolean serversStarted; // True if the network servers have been started.
    Boolean started; // True if we've been started. Prefs may still disable network servers.
    Boolean denyInterruptions; // True if hijacking is disabled.
    uint8_t deviceID[6]; // Globally unique device ID (i.e. primary MAC address).
    char ifname[IF_NAMESIZE + 1]; // Name of the interface to advertise on (all if empty).
    int timeoutDataSecs; // Timeout for data (defaults to kAirPlayDataTimeoutSecs).
    char* configFilePath; // Path to airplay.conf/airplay.ini
    CFDictionaryRef config;

#if (AIRPLAY_DACP)
    AirTunesDACPClientRef dacpClient;
#endif

#if (AIRPLAY_METRICS)
    CFMutableArrayRef recordedTimeStamps;
#endif

    char* cuPairingIdentityIdentifier;
    CFTypeRef multiZoneAudioHint; // Hint for the audio unit to aid in multizone cases
    CFTypeRef multiZoneDeviceID; // multizone devices must pass this in
    uint64_t mediaPort;
    int8_t powerState;
};

typedef struct AirPlayReceiverLogsPrivate* AirPlayReceiverLogsRef;
struct AirPlayReceiverLogsPrivate {
    CFRuntimeBase base; // CF type info. Must be first.
    AirPlayReceiverServerRef server; // Server that initiated the request.
    Boolean pending; // True if log retrieval is in progress.
    int status; // Status of the most recently completed logs request.
    unsigned int requestID; // Unique ID of the current logs request.
    DataBuffer* dataBuffer; // Buffer to store the current logs request result.
};

struct AirPlayReceiverConnectionPrivate {
    AirPlayReceiverServerRef server;
    HTTPConnectionRef httpCnx; // Underlying HTTP connection for this connection.
    AirPlayReceiverSessionRef session; // Session this connection is associated with.

#if (TARGET_OS_POSIX)
    NetworkChangeListenerRef networkChangeListener;
#endif

    uint64_t clientDeviceID;
    uint8_t clientInterfaceMACAddress[6];
    char clientName[128];
    uint64_t clientSessionID;
    uint32_t clientVersion;
    Boolean didAnnounce;
    Boolean didAudioSetup;
    Boolean didScreenSetup;
    Boolean didRecord;

    char ifName[IF_NAMESIZE + 1]; // Name of the interface the connection was accepted on.

    Boolean httpAuthentication_IsAuthenticated;

    AirPlayCompressionType compressionType;
    uint32_t framesPerPacket;

    AirPlayReceiverLogsRef logs;

    MFiSAPRef MFiSAP;
    Boolean MFiSAPDone;
    Boolean authSetupSuccessful;
    PairingSessionRef pairSetupSessionHomeKit;
    PairingSessionRef pairVerifySessionHomeKit;
    PairingSessionRef addPairingSession;
    PairingSessionRef removePairingSession;
    PairingSessionRef listPairingsSession;

    Boolean pairingVerified;
    Boolean pairingVerifiedAsHomeKit;
    Boolean pairingVerifiedAsHomeKitAdmin;
    int pairingCount;
    int pairDerive;
    uint8_t encryptionKey[16];
    uint8_t encryptionIV[16];
    Boolean usingEncryption;

    uint32_t minLatency, maxLatency;
};

#if 0
#pragma mark
#pragma mark == Server Utils ==
#endif

//===========================================================================================================================
//	Server Utils
//===========================================================================================================================

CF_RETURNS_RETAINED CFDictionaryRef
AirPlayCopyServerInfo(
    AirPlayReceiverServerRef inServer,
    AirPlayReceiverSessionRef inSession,
    CFArrayRef inProperties,
    OSStatus* outErr);

uint64_t AirPlayGetDeviceID(AirPlayReceiverServerRef inServer, uint8_t* outDeviceID);
OSStatus AirPlayGetDeviceName(AirPlayReceiverServerRef inServer, char* inBuffer, size_t inMaxLen);
AirPlayFeatures AirPlayGetFeatures(AirPlayReceiverServerRef inServer);
AirPlayFeatures AirPlayGetRequiredSenderFeatures(AirPlayReceiverServerRef inServer);
OSStatus AirPlayGetMinimumClientOSBuildVersion(AirPlayReceiverServerRef inServer, char* inBuffer, size_t inMaxLen);
OSStatus AirPlayCopyHomeKitPairingIdentity(AirPlayReceiverServerRef inServer, char** outIdentifier, uint8_t outPK[32]);
AirPlayStatusFlags AirPlayGetStatusFlags(AirPlayReceiverServerRef inServer);
void AirPlayReceiverServerSetLogLevel(void);
void AirPlayReceiverServerSetLogPath(void);
HTTPStatus AirPlayReceiverSendRequestPlistResponse(
    HTTPConnectionRef inCnx,
    HTTPMessageRef inMsg,
    CFPropertyListRef inPlist,
    OSStatus* outErr);

#ifdef __cplusplus
}
#endif

#endif // __AirPlayReceiverServerPriv_h__
