/*
	Copyright (C) 2014-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple. Not to be used or disclosed without permission from Apple.
*/

#ifndef __AudioConverter_h__
#define __AudioConverter_h__

#include "CommonServices.h"
#define AudioConverterForget(X) ForgetCustom(X, AudioConverterDispose)

#if HAS_COREAUDIO_HEADERS
#include <AudioToolbox/AudioConverter.h>
#include <AudioUnit/AudioCodec.h>
#include <AudioUnit/AudioCodecPriv.h>

#else

#include "CFUtils.h"

#ifdef __cplusplus
extern "C" {
#endif

//===========================================================================================================================
//	Types
//===========================================================================================================================

typedef struct AudioConverterPrivate* AudioConverterRef;
typedef uint32_t AudioConverterPropertyID;
typedef uint32_t AudioFormatID;
typedef uint32_t UInt32;

enum {
    kAudioCodecPropertyPacketSizeLimitForVBR = 0x70616b6c, // 'pakl'
};

typedef struct
{
    uint32_t mNumberChannels;
    uint32_t mDataByteSize;
    void* mData;

} AudioBuffer;

typedef struct
{
    uint32_t mNumberBuffers;
    AudioBuffer mBuffers[1];

} AudioBufferList;

typedef struct
{
    int64_t mStartOffset;
    uint32_t mVariableFramesInPacket;
    uint32_t mDataByteSize;

} AudioStreamPacketDescription;

typedef OSStatus (*AudioConverterComplexInputDataProc)(
    AudioConverterRef inAudioConverter,
    uint32_t* ioNumberDataPackets,
    AudioBufferList* ioData,
    AudioStreamPacketDescription** outDataPacketDescription,
    void* inUserData);

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================
OSStatus AudioConverterNew(
    const AudioStreamBasicDescription* inSourceFormat,
    const AudioStreamBasicDescription* inDestinationFormat,
    AudioConverterRef* outAudioConverter);
typedef OSStatus (*AudioConverterNew_f)(
    const AudioStreamBasicDescription* inSourceFormat,
    const AudioStreamBasicDescription* inDestinationFormat,
    AudioConverterRef* outAudioConverter);

OSStatus AudioConverterDispose(AudioConverterRef inAudioConverter);
typedef OSStatus (*AudioConverterDispose_f)(AudioConverterRef inAudioConverter);

OSStatus AudioConverterReset(AudioConverterRef inAudioConverter);
typedef OSStatus (*AudioConverterReset_f)(AudioConverterRef inAudioConverter);

OSStatus AudioConverterSetProperty(
    AudioConverterRef inAudioConverter,
    AudioConverterPropertyID inPropertyID,
    uint32_t inSize,
    const void* inData);
typedef OSStatus (*AudioConverterSetProperty_f)(
    AudioConverterRef inAudioConverter,
    AudioConverterPropertyID inPropertyID,
    uint32_t inSize,
    const void* inData);

OSStatus AudioConverterFillComplexBuffer(
    AudioConverterRef inAudioConverter,
    AudioConverterComplexInputDataProc inInputDataProc,
    void* inInputDataProcUserData,
    uint32_t* ioOutputDataPacketSize,
    AudioBufferList* outOutputData,
    AudioStreamPacketDescription* outPacketDescription);
typedef OSStatus (*AudioConverterFillComplexBuffer_f)(
    AudioConverterRef inAudioConverter,
    AudioConverterComplexInputDataProc inInputDataProc,
    void* inInputDataProcUserData,
    uint32_t* ioOutputDataPacketSize,
    AudioBufferList* outOutputData,
    AudioStreamPacketDescription* outPacketDescription);

//---------------------------------------------------------------------------------------------------------------------------
/*!	@function	AudioConverterGetContext / AudioConverterSetContext
	@abstract	Gets/sets a context pointer. Useful for DLL implementors to access their internal state.
*/
void* AudioConverterGetContext(AudioConverterRef inStream);
void AudioConverterSetContext(AudioConverterRef inStream, void* inContext);

#ifdef __cplusplus
}
#endif

#endif // HAS_COREAUDIO_HEADERS

#endif // __AudioConverter_h__
