/*
	Copyright (C) 2012-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef __AirPlayUtils_h_
#define __AirPlayUtils_h_

#include "AirPlayCommon.h"
#include "AudioConverter.h"
#include "ChaCha20Poly1305.h"
#include <sys/queue.h>

#include CF_HEADER
#include COREAUDIO_HEADER
#include LIBDISPATCH_HEADER

#ifdef __cplusplus
extern "C" {
#endif

//===========================================================================================================================
// Constants
//===========================================================================================================================

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

OSStatus
AirPlayAudioFormatToASBD(
    AirPlayAudioFormat inFormat,
    AudioStreamBasicDescription* outASBD,
    uint32_t* outBitsPerChannel);

OSStatus AirPlayAudioFormatToPCM(AirPlayAudioFormat inFormat, AudioStreamBasicDescription* outASBD);

AudioFormatID AirPlayCompressionTypeToAudioFormatID(AirPlayCompressionType inCompressionType);
AirPlayCompressionType AudioFormatIDToAirPlayCompressionType(AudioFormatID inFormatID);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlayCreateModesDictionary
	@abstract	Creates dictionary for the initial modes or the params to a changeModes request.
	
	@param		inChanges	Changes being request. Initialize with AirPlayModeChangesInit() then set fields.
	@param		inReason	Optional reason for the change. Mainly for diagnostics. May be NULL.
	@param		outErr		Optional error code to indicate a reason for failure. May be NULL.
*/
CFDictionaryRef AirPlayCreateModesDictionary(const AirPlayModeChanges* inChanges, CFStringRef inReason, OSStatus* outErr);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlay_DeriveAESKeySHA512
	@abstract	Derives a new pair of aesKey and aesIV for given master key using SHA-512.
	
	@param		inMasterKeyPtr	Master key.
	@param		inMasterKeyLen	Master key length.
	@param		inKeySaltPtr	Key salt.
	@param		inKeySaltLen	Key salt length.
	@param		inIVSaltPtr		IV salt.
	@param		inIVSaltLen		IV salt length.
	@param		outKey			16-byte place for derived key.
	@param		outIV			16-byte place for derived IV.
*/
void AirPlay_DeriveAESKeySHA512(
    const void* inMasterKeyPtr,
    size_t inMasterKeyLen,
    const void* inKeySaltPtr,
    size_t inKeySaltLen,
    const void* inIVSaltPtr,
    size_t inIVSaltLen,
    uint8_t outKey[16],
    uint8_t outIV[16]);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AirPlay_DeriveAESKeySHA512ForScreen
	@abstract	Derives a new pair of aesKey and aesIV for given master key and screen stream connection ID using SHA-512.
	
	@param		inMasterKeyPtr				Master key.
	@param		inMasterKeyLen				Master key length.
	@param		inScreenStreamConnectionID	Should be unique within session.
	@param		outKey						16-byte place for derived key.
	@param		outIV						16-byte place for derived IV.
*/
void AirPlay_DeriveAESKeySHA512ForScreen(
    const void* inMasterKeyPtr,
    size_t inMasterKeyLen,
    uint64_t inScreenStreamConnectionID,
    uint8_t outKey[16],
    uint8_t outIV[16]);

//===========================================================================================================================
//	RTPJitterBuffer
//===========================================================================================================================

typedef struct RTPJitterBufferContext RTPJitterBufferContext;
typedef struct RTPPacketNode RTPPacketNode;

TAILQ_HEAD(RTPPacketNodeList, RTPPacketNode);
typedef struct RTPPacketNodeList RTPPacketNodeList;

struct RTPPacketNode {
    TAILQ_ENTRY(RTPPacketNode)
    list;
    RTPSavedPacket pkt; // Full RTP packet with header and payload.
    uint8_t* ptr; // Ptr to RTP payload. Note: this may not point to the beginning of the payload.
    dispatch_semaphore_t decodeLock; // Lock to protect node while decoding (no reading from / writing to node while decoding).
    uint8_t* decodeBuffer; // Intermediate decode buffer.
    RTPJitterBufferContext* jitterBuffer; // Owning jitter buffer context.
};

struct RTPJitterBufferContext {
    dispatch_semaphore_t nodeLock; // Lock to protect node lists.
    pthread_t decodeThread; // Thread to offload decoding work.
    pthread_t* decodeThreadPtr; // Ptr to decodeThread when valid.
    pthread_cond_t decodeCondition; // Condition to signal when decode work is ready.
    pthread_cond_t* decodeConditionPtr; // Ptr to decodeCondition when valid.
    pthread_mutex_t decodeMutex; // Mutex for signaling decodeCondition.
    pthread_mutex_t* decodeMutexPtr; // Ptr to decodeMutex when valid.
    Boolean decodeDone; // Sentinal for terminating decodeThread.
    RTPPacketNode* packets; // Backing store for all the packets.
    RTPPacketNodeList freeList; // List of free nodes.
    RTPPacketNodeList preparedList; // List of nodes prepared for reading, sorted by timestamp.
    RTPPacketNodeList receivedList; // List of nodes received (but not prepared), sorted by timestamp.
    uint32_t nodesAllocated; // Allocated size of the Jitter Buffer in nodes.
    uint32_t nodesUsed; // Depth of the Jitter Buffer in nodes.
    AudioStreamBasicDescription inputFormat; // Format of sample data in enqueued nodes.
    AudioStreamBasicDescription outputFormat; // Format of sample data read from buffer (must be PCM).
    AudioConverterRef decoder; // AudioConverter instance for decoding.
    AudioStreamPacketDescription packetDescription; // Description of encoded packet.
    uint8_t* decodeBuffers; // Backing store for all packet decode buffers.
    uint32_t bufferMs; // Milliseconds of audio to buffer before starting.
    uint32_t nextTS; // Next timestamp to read from.
    uint64_t startTicks; // Ticks when we should start playing audio.
    Boolean disabled; // True if all input data should be dropped.
    Boolean buffering; // True if we're buffering until the high watermark is reached.
    uint32_t nLate; // Number of times samples were dropped because of being late.
    uint32_t nGaps; // Number of times samples that were missing (e.g. lost packet).
    uint32_t nSkipped; // Number of times we had to skip samples (before timing window).
    uint32_t nRebuffer; // Number of times we had to re-buffer because we ran dry.
    const char* label; // Optional label for logging.
    dispatch_queue_t logQueue; // Queue to keep logging off of time critical threads / locks.
};

OSStatus
RTPJitterBufferInit(
    RTPJitterBufferContext* ctx,
    const AudioStreamBasicDescription* inInputFormat,
    const AudioStreamBasicDescription* inOutputFormat,
    uint32_t inBufferMs);
void RTPJitterBufferFree(RTPJitterBufferContext* ctx);
void RTPJitterBufferReset(RTPJitterBufferContext* ctx, Float64 inDelta);
OSStatus RTPJitterBufferGetFreeNode(RTPJitterBufferContext* ctx, RTPPacketNode** outNode);
void RTPJitterBufferPutFreeNode(RTPJitterBufferContext* ctx, RTPPacketNode* inNode);
OSStatus RTPJitterBufferPutBusyNode(RTPJitterBufferContext* ctx, RTPPacketNode* inNode);
OSStatus RTPJitterBufferRead(RTPJitterBufferContext* ctx, void* inBuffer, size_t inLen);

//===========================================================================================================================
// ChaChaPoly encryption/decryption
//===========================================================================================================================

typedef struct
{
    Boolean isValid;
    chacha20_poly1305_state state; // Encryption/Decryption context
    uint8_t key[32]; // Key used to initialize encryption/decryption context.
    uint8_t nonce[8]; // Packet counter.

} ChaChaPolyCryptor;

#ifdef __cplusplus
}
#endif

#endif // __AirPlayUtils_h_
