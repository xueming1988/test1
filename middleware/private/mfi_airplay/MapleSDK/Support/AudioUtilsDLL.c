/*
	Copyright (C) 2013-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
	
	AudioStream adapter that delegates functionality to a DLL.
	
	This defaults to loading the DLL from "libAudioStream.so".
	These can be overridden in the makefile with the following:
	
	CFLAGS += -DAUDIO_STREAM_DLL_PATH\"/some/other/path/libAudioStream.so\"
*/

#include "AudioUtils.h"

#include <dlfcn.h>
#include <stdlib.h>

#include "CommonServices.h"
#include "DebugServices.h"

#include CF_HEADER
#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

//===========================================================================================================================
//	AudioStream
//===========================================================================================================================

#if (defined(AUDIO_STREAM_DLL_PATH))
#define kAudioStreamDLLPath AUDIO_STREAM_DLL_PATH
#else
#define kAudioStreamDLLPath "libAudioStream.so"
#endif

#define FIND_SYM(NAME) (NAME##_f)(uintptr_t) dlsym(me->dllHandle, #NAME);

struct AudioStreamPrivate {
    CFRuntimeBase base; // CF type info. Must be first.
    void* context; // Context for DLLs.
    void* dllHandle; // Handle to the DLL implementing the internals.
    AudioStreamInitialize_f initialize_f;
    AudioStreamFinalize_f finalize_f;
    AudioStreamSetInputCallback_f setInputCallback_f;
    AudioStreamSetOutputCallback_f setOutputCallback_f;
    _AudioStreamCopyProperty_f copyProperty_f;
    _AudioStreamSetProperty_f setProperty_f;
    AudioStreamSetFormat_f setFormat_f;
    AudioStreamSetDelegateContext_f setDelegateContext_f;
    AudioStreamPrepare_f prepare_f;
    AudioStreamStart_f start_f;
    AudioStreamStop_f stop_f;
};

static void _AudioStreamGetTypeID(void* inContext);
static void _AudioStreamFinalize(CFTypeRef inCF);

static const CFRuntimeClass kAudioStreamClass = {
    0, // version
    "AudioStream", // className
    NULL, // init
    NULL, // copy
    _AudioStreamFinalize, // finalize
    NULL, // equal -- NULL means pointer equality.
    NULL, // hash  -- NULL means pointer hash.
    NULL, // copyFormattingDesc
    NULL, // copyDebugDesc
    NULL, // reclaim
    NULL // refcount
};

static dispatch_once_t gAudioStreamInitOnce = 0;
static CFTypeID gAudioStreamTypeID = _kCFRuntimeNotATypeID;

//===========================================================================================================================
//	AudioStreamGetTypeID
//===========================================================================================================================

CFTypeID AudioStreamGetTypeID(void)
{
    dispatch_once_f(&gAudioStreamInitOnce, NULL, _AudioStreamGetTypeID);
    return (gAudioStreamTypeID);
}

static void _AudioStreamGetTypeID(void* inContext)
{
    (void)inContext;

    gAudioStreamTypeID = _CFRuntimeRegisterClass(&kAudioStreamClass);
    check(gAudioStreamTypeID != _kCFRuntimeNotATypeID);
}

//===========================================================================================================================
//	AudioStreamCreate
//===========================================================================================================================

OSStatus AudioStreamCreate(AudioStreamRef* outStream)
{
    OSStatus err;
    AudioStreamRef me;
    size_t extraLen;

    extraLen = sizeof(*me) - sizeof(me->base);
    me = (AudioStreamRef)_CFRuntimeCreateInstance(NULL, AudioStreamGetTypeID(), (CFIndex)extraLen, NULL);
    require_action(me, exit, err = kNoMemoryErr);
    memset(((uint8_t*)me) + sizeof(me->base), 0, extraLen);

    // Note: this uses RTLD_NODELETE to avoid re-initialization issues with global log categories if the DLL is unloaded
    // and reloaded. Log categories we know about are removed on finalize, but DLL developers may not be as thorough.

    me->dllHandle = dlopen(kAudioStreamDLLPath, RTLD_LAZY | RTLD_LOCAL | RTLD_NODELETE);
    require_action(me->dllHandle, exit, err = kPathErr);

    me->initialize_f = FIND_SYM(AudioStreamInitialize);
    me->finalize_f = FIND_SYM(AudioStreamFinalize);
    me->setInputCallback_f = FIND_SYM(AudioStreamSetInputCallback);
    me->setOutputCallback_f = FIND_SYM(AudioStreamSetOutputCallback);
    me->copyProperty_f = FIND_SYM(_AudioStreamCopyProperty);
    me->setProperty_f = FIND_SYM(_AudioStreamSetProperty);
    me->setFormat_f = FIND_SYM(AudioStreamSetFormat);
    me->setDelegateContext_f = FIND_SYM(AudioStreamSetDelegateContext);
    me->prepare_f = FIND_SYM(AudioStreamPrepare);
    me->start_f = FIND_SYM(AudioStreamStart);
    me->stop_f = FIND_SYM(AudioStreamStop);

    if (me->initialize_f) {
        err = me->initialize_f(me);
        require_noerr(err, exit);
    }

    *outStream = me;
    me = NULL;
    err = kNoErr;

exit:
    CFReleaseNullSafe(me);
    return (err);
}

//===========================================================================================================================
//	_AudioStreamFinalize
//===========================================================================================================================

static void _AudioStreamFinalize(CFTypeRef inCF)
{
    AudioStreamRef const me = (AudioStreamRef)inCF;

    if (me->finalize_f)
        me->finalize_f(me);
    if (me->dllHandle) {
        dlclose(me->dllHandle);
        me->dllHandle = NULL;
    }
}

//===========================================================================================================================
//	AudioStreamGetContext
//===========================================================================================================================

void* AudioStreamGetContext(AudioStreamRef me)
{
    return (me->context);
}

//===========================================================================================================================
//	AudioStreamSetContext
//===========================================================================================================================

void AudioStreamSetContext(AudioStreamRef me, void* inContext)
{
    me->context = inContext;
}

//===========================================================================================================================
//	AudioStreamSetInputCallback
//===========================================================================================================================

void AudioStreamSetInputCallback(AudioStreamRef me, AudioStreamInputCallback_f inFunc, void* inContext)
{
    if (me->setInputCallback_f)
        me->setInputCallback_f(me, inFunc, inContext);
}

//===========================================================================================================================
//	AudioStreamSetOutputCallback
//===========================================================================================================================

void AudioStreamSetOutputCallback(AudioStreamRef me, AudioStreamOutputCallback_f inFunc, void* inContext)
{
    if (me->setOutputCallback_f)
        me->setOutputCallback_f(me, inFunc, inContext);
}

//===========================================================================================================================
//	_AudioStreamCopyProperty
//===========================================================================================================================

CFTypeRef _AudioStreamCopyProperty(CFTypeRef inObject, CFStringRef inProperty, OSStatus* outErr)
{
    AudioStreamRef const me = (AudioStreamRef)inObject;

    if (me->copyProperty_f)
        return (me->copyProperty_f(inObject, inProperty, outErr));
    if (outErr)
        *outErr = kUnsupportedErr;
    return (NULL);
}

//===========================================================================================================================
//	_AudioStreamSetProperty
//===========================================================================================================================

OSStatus _AudioStreamSetProperty(CFTypeRef inObject, CFStringRef inProperty, CFTypeRef inValue)
{
    AudioStreamRef const me = (AudioStreamRef)inObject;

    return (me->setProperty_f ? me->setProperty_f(inObject, inProperty, inValue) : kUnsupportedErr);
}

//===========================================================================================================================
//	AudioStreamSetFormat
//===========================================================================================================================

void AudioStreamSetFormat(AudioStreamRef me, const AudioStreamBasicDescription* inFormat)
{
    if (me->setFormat_f)
        me->setFormat_f(me, inFormat);
}

//===========================================================================================================================
//	AudioStreamSetDelegateContext
//===========================================================================================================================

void AudioStreamSetDelegateContext(AudioStreamRef me, void* inContext)
{
    if (me->setDelegateContext_f)
        me->setDelegateContext_f(me, inContext);
}

//===========================================================================================================================
//	AudioStreamPrepare
//===========================================================================================================================

OSStatus AudioStreamPrepare(AudioStreamRef me)
{
    return (me->prepare_f ? me->prepare_f(me) : kUnsupportedErr);
}

//===========================================================================================================================
//	AudioStreamStart
//===========================================================================================================================

OSStatus AudioStreamStart(AudioStreamRef me)
{
    return (me->start_f ? me->start_f(me) : kUnsupportedErr);
}

//===========================================================================================================================
//	AudioStreamStop
//===========================================================================================================================

void AudioStreamStop(AudioStreamRef me, Boolean inDrain)
{
    if (me->stop_f)
        me->stop_f(me, inDrain);
}

#if 0
#pragma mark -
#endif
