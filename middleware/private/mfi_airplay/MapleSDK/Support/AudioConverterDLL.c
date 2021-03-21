/*
	Copyright (C) 2013-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
	
	AudioConverter adapter that delegates functionality to a DLL.
	
	This defaults to loading the DLL from "libAudioConverter.so".
	These can be overridden in the makefile with the following:
	
	CFLAGS += -DAUDIO_CONVERTER_DLL_PATH\"/some/other/path/libAudioConverter.so\"
*/

#include "AudioConverter.h"
#include "CommonServices.h"
#include "DebugServices.h"

#include <dlfcn.h>

//===========================================================================================================================
//	AudioConverter
//===========================================================================================================================

#if (defined(AUDIO_CONVERTER_DLL_PATH))
#define kAudioConverterDLLPath AUDIO_CONVERTER_DLL_PATH
#else
#define kAudioConverterDLLPath "libAudioConverter.so"
#endif

#define FIND_SYM(NAME) (NAME##_f)(uintptr_t) dlsym(me->dllHandle, #NAME);

struct AudioConverterPrivate {
    void* context; // Context for DLLs.
    void* dllHandle; // Handle to the DLL implementing the internals.
    AudioConverterNew_f new_f;
    AudioConverterDispose_f dispose_f;
    AudioConverterReset_f reset_f;
    AudioConverterSetProperty_f setProperty_f;
    AudioConverterFillComplexBuffer_f fillComplexBuffer_f;
};

//===========================================================================================================================
//	AudioConverterNew
//===========================================================================================================================

OSStatus AudioConverterNew(
    const AudioStreamBasicDescription* inSourceFormat,
    const AudioStreamBasicDescription* inDestinationFormat,
    AudioConverterRef* outConverter)
{
    OSStatus err;
    AudioConverterRef me;

    me = calloc(1, sizeof(*me));
    require_action(me, exit, err = kNoMemoryErr);

    // Note: this uses RTLD_NODELETE to avoid re-initialization issues with global log categories if the DLL is unloaded
    // and reloaded. Log categories we know about are removed on finalize, but DLL developers may not be as thorough.

    me->dllHandle = dlopen(kAudioConverterDLLPath, RTLD_LAZY | RTLD_LOCAL | RTLD_NODELETE);
    require_action(me->dllHandle, exit, err = kPathErr);

    me->new_f = FIND_SYM(AudioConverterNew);
    me->dispose_f = FIND_SYM(AudioConverterDispose);
    me->reset_f = FIND_SYM(AudioConverterReset);
    me->setProperty_f = FIND_SYM(AudioConverterSetProperty);
    me->fillComplexBuffer_f = FIND_SYM(AudioConverterFillComplexBuffer);

    if (me->new_f) {
        AudioConverterRef context;
        err = me->new_f(inSourceFormat, inDestinationFormat, &context);
        require_noerr(err, exit);
        AudioConverterSetContext(me, context);
    }

    *outConverter = me;
    me = NULL;
    err = kNoErr;

exit:
    CFReleaseNullSafe(me);
    return (err);
}

//===========================================================================================================================
//	AudioConverterDispose
//===========================================================================================================================
OSStatus AudioConverterDispose(AudioConverterRef me)
{
    if (me->dispose_f) {
        me->dispose_f(me->context);
    }
    if (me->dllHandle) {
        dlclose(me->dllHandle);
        me->dllHandle = NULL;
    }
    return kNoErr;
}

//===========================================================================================================================
//	AudioConverterReset
//===========================================================================================================================
OSStatus AudioConverterReset(AudioConverterRef inAudioConverter)
{
    return (inAudioConverter->reset_f ? inAudioConverter->reset_f(inAudioConverter->context) : kUnsupportedErr);
}

//===========================================================================================================================
//	AudioConverterSetProperty
//===========================================================================================================================
OSStatus AudioConverterSetProperty(
    AudioConverterRef inAudioConverter,
    AudioConverterPropertyID inPropertyID,
    uint32_t inSize,
    const void* inData)
{
    return (inAudioConverter->setProperty_f ? inAudioConverter->setProperty_f(inAudioConverter->context, inPropertyID, inSize, inData) : kUnsupportedErr);
}

//===========================================================================================================================
//	AudioConverterFillComplexBuffer
//===========================================================================================================================
OSStatus AudioConverterFillComplexBuffer(
    AudioConverterRef inAudioConverter,
    AudioConverterComplexInputDataProc inInputDataProc,
    void* inInputDataProcUserData,
    uint32_t* ioOutputDataPacketSize,
    AudioBufferList* outOutputData,
    AudioStreamPacketDescription* outPacketDescription)
{
    return (inAudioConverter->fillComplexBuffer_f ? inAudioConverter->fillComplexBuffer_f(inAudioConverter->context, inInputDataProc, inInputDataProcUserData, ioOutputDataPacketSize, outOutputData, outPacketDescription) : kUnsupportedErr);
}
//===========================================================================================================================
//	ScreenStreamGetContext
//===========================================================================================================================

void* AudioConverterGetContext(AudioConverterRef me)
{
    return (me->context);
}

//===========================================================================================================================
//	ScreenStreamSetContext
//===========================================================================================================================

void AudioConverterSetContext(AudioConverterRef me, void* inContext)
{
    me->context = inContext;
}
