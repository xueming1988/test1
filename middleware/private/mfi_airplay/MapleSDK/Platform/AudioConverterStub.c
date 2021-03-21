/*
	Copyright (C) 2014-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple. Not to be used or disclosed without permission from Apple.
*/

//=================================================================================================================================
// https://developer.apple.com/library/prerelease/ios/documentation/MusicAudio/Reference/AudioConverterServicesReference/index.html
//=================================================================================================================================

#include "AudioConverter.h"
#include "CommonServices.h"
#include "DebugServices.h"

#include "AppleLosslessDecoder.h"
#ifdef LINKPLAY
#include "aac.h"
#endif
#include "BitUtilities.h"

//===========================================================================================================================
//	Internals
//===========================================================================================================================
typedef struct AudioConverterPrivate* AudioConverterPrivateRef;
struct AudioConverterPrivate {
    uint32_t sourceFormatID;
    uint32_t destFormatID;
    uint32_t sampleRate;
    uint32_t channels;
    uint32_t framesPerPacket;
    void* nativeCodecRef;
};

//===========================================================================================================================
//	AudioConverterNew
//===========================================================================================================================

OSStatus AudioConverterNew(const AudioStreamBasicDescription* inSourceFormat, const AudioStreamBasicDescription* inDestinationFormat, AudioConverterRef* outAudioConverter)
{
    OSStatus err;
    AudioConverterPrivateRef me;

    // Sample rate conversion and mixing are not supported
    if (inDestinationFormat->mSampleRate != inSourceFormat->mSampleRate)
        return kUnsupportedErr;
    if (inDestinationFormat->mChannelsPerFrame != inSourceFormat->mChannelsPerFrame)
        return kUnsupportedErr;

    me = (AudioConverterPrivateRef)calloc(1, sizeof(*me));
    require_action(me, exit, err = kNoMemoryErr);

    // $$$ TODO: The accessory will need to implement support for:
    // AAC LC decode
    // Parameters are provided here to initialize the codec

    me->sourceFormatID = inSourceFormat->mFormatID;
    me->destFormatID = inDestinationFormat->mFormatID;
    me->sampleRate = inDestinationFormat->mSampleRate;
    me->channels = inDestinationFormat->mChannelsPerFrame;
    me->framesPerPacket = inDestinationFormat->mFramesPerPacket;

    switch (me->sourceFormatID) {
    case kAudioFormatAppleLossless:
        require_action_quiet(inDestinationFormat->mFormatID == kAudioFormatLinearPCM, exit, err = kUnsupportedErr);
        err = AppleLosslessDecoder_Create((void*)&me->nativeCodecRef);
        require_noerr(err, exit);
        break;

    case kAudioFormatMPEG4AAC:
        require_action_quiet(inDestinationFormat->mFormatID == kAudioFormatLinearPCM, exit, err = kUnsupportedErr);
        // $$$ TODO: Initialize codec for AAC LC -> PCM decompression
        err = kNotHandledErr;
        
#ifdef LINKPLAY
        err = AacDecoder_Create((void*)&me->nativeCodecRef);
#endif
        break;

    default:
        err = kUnsupportedErr;
        goto exit;
    }
    *outAudioConverter = me;
    me = NULL;

exit:
    if (me)
        AudioConverterDispose((AudioConverterRef)me);
    return (err);
}

//===========================================================================================================================
//	AudioConverterDispose
//===========================================================================================================================

OSStatus AudioConverterDispose(AudioConverterRef inConverter)
{
    AudioConverterPrivateRef const me = (AudioConverterPrivateRef)inConverter;

    // $$$ TODO: Last chance to free any codec resources for this stream.

    if (me->nativeCodecRef) {
        switch (me->sourceFormatID) {
        case kAudioFormatAppleLossless:
            AppleLosslessDecoder_Delete(me->nativeCodecRef);
            break;

        case kAudioFormatMPEG4AAC:
            // $$$ TODO: Free any resources for the AAC LC decoder
#ifdef LINKPLAY            
            AacDecoder_Delete(me->nativeCodecRef);
#endif
            break;
        }
    }
    free(me);
    return (kNoErr);
}

//===========================================================================================================================
//	AudioConverterReset
//===========================================================================================================================

OSStatus AudioConverterReset(AudioConverterRef inConverter)
{
    (void)inConverter;

    // $$$ TODO: Discard any data buffered by the codec
    return (kNoErr);
}

static OSStatus _AudioConverterSetPropertyALAC(AudioConverterPrivateRef const me, AudioConverterPropertyID inPropertyID, uint32_t inSize, const void* inData)
{
    switch (inPropertyID) {
    case kAudioConverterDecompressionMagicCookie: {
        const ALACParams* const alacParams = (const ALACParams*)inData;
        if (inSize != sizeof(*alacParams)) {
            return kSizeErr;
        }
        return AppleLosslessDecoder_Configure(me->nativeCodecRef, alacParams);
    } break;
    default:
        return kUnsupportedErr;
    }
}

static OSStatus _AudioConverterSetPropertyAACDecode(AudioConverterPrivateRef const me, AudioConverterPropertyID inPropertyID, uint32_t inSize, const void* inData)
{
    (void)inSize;
    (void)inData;
    (void)me;

#ifdef LINKPLAY
    switch (inPropertyID) {
    case kAudioConverterDecompressionMagicCookie: {

        const AACParams* const aacParams = (const AACParams*)inData;
        if (inSize != sizeof(*aacParams)) {
            return kSizeErr;
        }
        return AacDecoder_SetConfigure(me->nativeCodecRef, aacParams);
    } break;
    default:
        return kUnsupportedErr;
    }
#endif

    return kNoErr;
}

//===========================================================================================================================
//	AudioConverterSetProperty
//===========================================================================================================================

OSStatus AudioConverterSetProperty(AudioConverterRef inConverter, AudioConverterPropertyID inPropertyID, uint32_t inSize, const void* inData)
{
    AudioConverterPrivateRef const me = (AudioConverterPrivateRef)inConverter;

    if (!me->nativeCodecRef)
        return kStateErr;
    switch (me->sourceFormatID) {
    case kAudioFormatAppleLossless:
        return _AudioConverterSetPropertyALAC(me, inPropertyID, inSize, inData);

    case kAudioFormatMPEG4AAC:
        return _AudioConverterSetPropertyAACDecode(me, inPropertyID, inSize, inData);

    default:
        return kUnsupportedErr;
    }
}

static OSStatus _AudioConverterFillComplexBufferALACDecode(
    AudioConverterRef inConverter,
    AudioConverterComplexInputDataProc inInputDataProc,
    void* inInputDataProcUserData,
    uint32_t* ioOutputDataPacketSize,
    AudioBufferList* outOutputData,
    AudioStreamPacketDescription* outPacketDescription)
{
    AudioConverterPrivateRef const me = (AudioConverterPrivateRef)inConverter;
    OSStatus err;
    BitBuffer bits;
    AudioBufferList bufferList;
    uint32_t packetCount;
    AudioStreamPacketDescription* packetDesc;
    uint32_t sampleCount;

    (void)outPacketDescription;

    packetCount = 1;
    packetDesc = NULL;
    err = inInputDataProc(inConverter, &packetCount, &bufferList, &packetDesc, inInputDataProcUserData);
    require_noerr_quiet(err, exit);

    BitBufferInit(&bits, (uint8_t*)bufferList.mBuffers[0].mData, bufferList.mBuffers[0].mDataByteSize);
    sampleCount = *ioOutputDataPacketSize;
    err = AppleLosslessDecoder_Decode(me->nativeCodecRef, &bits, (uint8_t*)outOutputData->mBuffers[0].mData,
        sampleCount, 2, &sampleCount);
    require_noerr_quiet(err, exit);

    *ioOutputDataPacketSize = sampleCount;

exit:
    return (err);
}

static OSStatus _AudioConverterFillComplexBufferAACDecode(AudioConverterRef inConverter, AudioConverterComplexInputDataProc inInputDataProc, void* inInputDataProcUserData, uint32_t* ioOutputDataPacketSize, AudioBufferList* outOutputData, AudioStreamPacketDescription* outPacketDescription)
{
    AudioConverterPrivateRef const me = (AudioConverterPrivateRef)inConverter;
    OSStatus err;

    (void)outPacketDescription;
    (void)me;

    AudioBufferList bufferList;
    uint32_t packetCount;
    AudioStreamPacketDescription* packetDesc;

    // $$$ TODO: Callback will provide number of frames per packet in ioOutputDataPacketSize
    if (*ioOutputDataPacketSize < kAudioSamplesPerPacket_AAC_LC) {
        return kSizeErr;
    }

    packetCount = 1;
    packetDesc = NULL;
    // $$$ TODO: Request 1 packet of AAC LC through callback to decode kAudioSamplesPerPacket_AAC_LC of output.
    // The codec is responsible for consuming all bytes provided.
    err = inInputDataProc(inConverter, &packetCount, &bufferList, &packetDesc, inInputDataProcUserData);
    require_noerr_quiet(err, exit);

    // $$$ TODO: Push AAC LC input into decoder and return PCM data into supplied buffer
    // input parameters: bufferList.mBuffers[ 0 ].mData, bufferList.mBuffers[ 0 ].mDataByteSize
    // output parameters: outOutputData->mBuffers[ 0 ].mData, outOutputData->mBuffers[ 0 ].mDataByteSize

#ifdef LINKPLAY
    uint32_t sampleCount = 0;
    err = AacDecoder_Decode((aac_decoder_ref)(me->nativeCodecRef), bufferList.mBuffers[0].mData, 
                   bufferList.mBuffers[0].mDataByteSize, (uint8_t*)outOutputData->mBuffers[0].mData, &sampleCount);

    require_noerr_quiet(err, exit);

    outOutputData->mBuffers[0].mDataByteSize = sampleCount;

    *ioOutputDataPacketSize = sampleCount;

#endif

    if (err == kNoErr && outPacketDescription) {
        outPacketDescription[0].mStartOffset = 0;
        outPacketDescription[0].mVariableFramesInPacket = 0;
        outPacketDescription[0].mDataByteSize = outOutputData->mBuffers[0].mDataByteSize;
        err = kNoErr;
    }

    // $$$ TODO: Set the number of samples produced
    //*ioOutputDataPacketSize = kAudioSamplesPerPacket_AAC_LC;
exit:
    return err;
}

//===========================================================================================================================
//	AudioConverterFillComplexBuffer
//===========================================================================================================================

OSStatus AudioConverterFillComplexBuffer(AudioConverterRef inConverter, AudioConverterComplexInputDataProc inInputDataProc,
    void* inInputDataProcUserData,
    uint32_t* ioOutputDataPacketSize,
    AudioBufferList* outOutputData,
    AudioStreamPacketDescription* outPacketDescription)
{
    AudioConverterPrivateRef const me = (AudioConverterPrivateRef)inConverter;

    if (!me->nativeCodecRef)
        return kStateErr;

    switch (me->sourceFormatID) {
    case kAudioFormatAppleLossless:
        return _AudioConverterFillComplexBufferALACDecode(inConverter, inInputDataProc, inInputDataProcUserData, ioOutputDataPacketSize, outOutputData, outPacketDescription);

    case kAudioFormatMPEG4AAC:
        // AAC LC to PCM
        return _AudioConverterFillComplexBufferAACDecode(inConverter, inInputDataProc, inInputDataProcUserData, ioOutputDataPacketSize, outOutputData, outPacketDescription);

    default:
        return kUnsupportedErr;
    }
}
