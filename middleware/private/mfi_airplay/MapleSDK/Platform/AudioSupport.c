//
//  Copyright (c) 2015 Apple Inc. All rights reserved.
//

#include "AirPlayCommon.h"
#include "AudioUtils.h"
#include <CoreUtils/CFUtils.h>

#if (!defined(kAudioStreamAudioType_Compatibility))
#define kAudioStreamAudioType_Compatibility CFSTR("compatibility")
#endif

// [Number] Stream type. See AudioStreamType.
#define kAudioSessionKey_Type CFSTR("type")

// [String] Type of audio content (e.g. telephony, media, etc.). See kAudioStreamAudioType_*.
#define kAudioSessionKey_AudioType CFSTR("audioType")

// [Number] Number of audio channels (e.g. 2 for stereo).
#define kAudioSessionKey_Channels CFSTR("ch")
// [Number] Number of samples per second (e.g. 44100).
#define kAudioSessionKey_SampleRate CFSTR("sr")

// [Number] Bit size of each audio sample (e.g. "16").
#define kAudioSessionKey_SampleSize CFSTR("ss")
// [Number] Input latency in microseconds.
#define kAudioSessionKey_InputLatencyMicros CFSTR("inputLatencyMicros")

// [Number] Output latency in microseconds.
#define kAudioSessionKey_OutputLatencyMicros CFSTR("outputLatencyMicros")

CFArrayRef GetAudioFormats(OSStatus* outErr);
CFArrayRef GetAudioLatencies(OSStatus* outErr);

static CFDictionaryRef
_CreateAudioFormatDictionary(
    AudioStreamType inStreamType,
    CFStringRef inAudioType,
    AirPlayAudioFormat inInputFormats,
    AirPlayAudioFormat inOutputFormats,
    OSStatus* outErr)
{
    CFDictionaryRef result = NULL;
    OSStatus err;
    CFMutableDictionaryRef dict;

    dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(dict, exit, err = kNoMemoryErr);

    if (inStreamType != kAudioStreamType_Invalid)
        CFDictionarySetInt64(dict, CFSTR(kAirPlayKey_Type), inStreamType);
    if (inAudioType)
        CFDictionarySetValue(dict, CFSTR(kAirPlayKey_AudioType), inAudioType);
    if (inInputFormats)
        CFDictionarySetInt64(dict, CFSTR(kAirPlayKey_AudioInputFormats), inInputFormats);
    if (inOutputFormats)
        CFDictionarySetInt64(dict, CFSTR(kAirPlayKey_AudioOutputFormats), inOutputFormats);

    result = dict;
    dict = NULL;
    err = kNoErr;
exit:
    CFReleaseNullSafe(dict);
    if (outErr)
        *outErr = err;
    return (result);
}

CFArrayRef GetAudioFormats(OSStatus* outErr)
{
    CFArrayRef result = NULL;
    OSStatus err;
    CFMutableArrayRef audioFormatsArray;
    CFMutableDictionaryRef dict = NULL;
    AirPlayAudioFormat inputFormats, outputFormats;

    audioFormatsArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    require_action_quiet(audioFormatsArray, exit, err = kNotHandledErr);

    // Main Audio - Compatibility
    inputFormats = kAirPlayAudioFormat_PCM_8KHz_16Bit_Mono | kAirPlayAudioFormat_PCM_16KHz_16Bit_Mono | kAirPlayAudioFormat_PCM_24KHz_16Bit_Mono;
    outputFormats = kAirPlayAudioFormat_PCM_8KHz_16Bit_Mono | kAirPlayAudioFormat_PCM_16KHz_16Bit_Mono | kAirPlayAudioFormat_PCM_24KHz_16Bit_Mono | kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo | kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo;
    dict = _CreateAudioFormatDictionary(kAudioStreamType_MainAudio, kAudioStreamAudioType_Compatibility, inputFormats, outputFormats, &err);
    require_noerr(err, exit);

    CFArrayAppendValue(audioFormatsArray, dict);
    ForgetCF(&dict);

    // Alt Audio - Compatibility
    inputFormats = 0;
    outputFormats = kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo | kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo;
    dict = _CreateAudioFormatDictionary(kAudioStreamType_AltAudio, kAudioStreamAudioType_Compatibility, inputFormats, outputFormats, &err);
    require_noerr(err, exit);

    CFArrayAppendValue(audioFormatsArray, dict);
    ForgetCF(&dict);

    // Main Audio - Alert
    inputFormats = 0;
    outputFormats = kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo | kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo | kAirPlayAudioFormat_OPUS_48KHz_Mono;
    //kAirPlayAudioFormat_AAC_ELD_48KHz_Stereo |
    //kAirPlayAudioFormat_AAC_ELD_44KHz_Stereo; */
    dict = _CreateAudioFormatDictionary(kAudioStreamType_MainAudio, kAudioStreamAudioType_Alert, inputFormats, outputFormats, &err);
    require_noerr(err, exit);

    CFArrayAppendValue(audioFormatsArray, dict);
    ForgetCF(&dict);

    // Main Audio - Default
    inputFormats = kAirPlayAudioFormat_PCM_24KHz_16Bit_Mono | kAirPlayAudioFormat_PCM_16KHz_16Bit_Mono | kAirPlayAudioFormat_OPUS_24KHz_Mono | kAirPlayAudioFormat_OPUS_16KHz_Mono;
    //kAirPlayAudioFormat_AAC_ELD_16KHz_Mono |
    //kAirPlayAudioFormat_AAC_ELD_24KHz_Mono;
    outputFormats = kAirPlayAudioFormat_PCM_24KHz_16Bit_Mono | kAirPlayAudioFormat_PCM_16KHz_16Bit_Mono | kAirPlayAudioFormat_OPUS_24KHz_Mono | kAirPlayAudioFormat_OPUS_16KHz_Mono;
    //kAirPlayAudioFormat_AAC_ELD_16KHz_Mono |
    //kAirPlayAudioFormat_AAC_ELD_24KHz_Mono;
    dict = _CreateAudioFormatDictionary(kAudioStreamType_MainAudio, kAudioStreamAudioType_Default, inputFormats, outputFormats, &err);
    require_noerr(err, exit);

    CFArrayAppendValue(audioFormatsArray, dict);
    ForgetCF(&dict);

    // Main Audio - Media
    inputFormats = 0;
    outputFormats = kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo | kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo;
    dict = _CreateAudioFormatDictionary(kAudioStreamType_MainAudio, kAudioStreamAudioType_Media, inputFormats, outputFormats, &err);
    require_noerr(err, exit);

    CFArrayAppendValue(audioFormatsArray, dict);
    ForgetCF(&dict);

    // Main Audio - Telephony
    inputFormats = kAirPlayAudioFormat_PCM_24KHz_16Bit_Mono | kAirPlayAudioFormat_PCM_16KHz_16Bit_Mono | kAirPlayAudioFormat_OPUS_24KHz_Mono | kAirPlayAudioFormat_OPUS_16KHz_Mono;
    //kAirPlayAudioFormat_AAC_ELD_16KHz_Mono |
    //kAirPlayAudioFormat_AAC_ELD_24KHz_Mono;
    outputFormats = kAirPlayAudioFormat_PCM_24KHz_16Bit_Mono | kAirPlayAudioFormat_PCM_16KHz_16Bit_Mono | kAirPlayAudioFormat_OPUS_24KHz_Mono | kAirPlayAudioFormat_OPUS_16KHz_Mono;
    //kAirPlayAudioFormat_AAC_ELD_16KHz_Mono |
    //kAirPlayAudioFormat_AAC_ELD_24KHz_Mono;
    dict = _CreateAudioFormatDictionary(kAudioStreamType_MainAudio, kAudioStreamAudioType_Telephony, inputFormats, outputFormats, &err);
    require_noerr(err, exit);

    CFArrayAppendValue(audioFormatsArray, dict);
    ForgetCF(&dict);

    // Main Audio - SpeechRecognition
    inputFormats = kAirPlayAudioFormat_PCM_24KHz_16Bit_Mono | kAirPlayAudioFormat_OPUS_24KHz_Mono;
    //kAirPlayAudioFormat_AAC_ELD_24KHz_Mono;
    outputFormats = kAirPlayAudioFormat_PCM_24KHz_16Bit_Mono | kAirPlayAudioFormat_OPUS_24KHz_Mono;
    //kAirPlayAudioFormat_AAC_ELD_24KHz_Mono;
    dict = _CreateAudioFormatDictionary(kAudioStreamType_MainAudio, kAudioStreamAudioType_SpeechRecognition, inputFormats, outputFormats, &err);
    require_noerr(err, exit);

    CFArrayAppendValue(audioFormatsArray, dict);
    ForgetCF(&dict);

    // Alt Audio - Default
    inputFormats = 0;
    outputFormats = kAirPlayAudioFormat_PCM_44KHz_16Bit_Stereo | kAirPlayAudioFormat_PCM_48KHz_16Bit_Stereo | kAirPlayAudioFormat_OPUS_48KHz_Mono;
    //kAirPlayAudioFormat_AAC_ELD_48KHz_Stereo |
    //kAirPlayAudioFormat_AAC_ELD_44KHz_Stereo;
    dict = _CreateAudioFormatDictionary(kAudioStreamType_AltAudio, kAudioStreamAudioType_Default, inputFormats, outputFormats, &err);
    require_noerr(err, exit);

    CFArrayAppendValue(audioFormatsArray, dict);
    ForgetCF(&dict);

    // Main High - Media
    inputFormats = 0;
    outputFormats = kAirPlayAudioFormat_AAC_LC_48KHz_Stereo | kAirPlayAudioFormat_AAC_LC_44KHz_Stereo;
    dict = _CreateAudioFormatDictionary(kAudioStreamType_MainHighAudio, kAudioStreamAudioType_Media, inputFormats, outputFormats, &err);
    require_noerr(err, exit);

    CFArrayAppendValue(audioFormatsArray, dict);

    result = audioFormatsArray;
    audioFormatsArray = NULL;
    err = kNoErr;

exit:
    CFReleaseNullSafe(dict);
    CFReleaseNullSafe(audioFormatsArray);
    if (outErr)
        *outErr = err;
    return (result);
}

static CFDictionaryRef
_CreateLatencyDictionary(
    AudioStreamType inStreamType,
    CFStringRef inAudioType,
    uint32_t inSampleRate,
    uint32_t inSampleSize,
    uint32_t inChannels,
    uint32_t inInputLatency,
    uint32_t inOutputLatency,
    OSStatus* outErr)
{
    CFDictionaryRef result = NULL;
    OSStatus err;
    CFMutableDictionaryRef latencyDict;

    latencyDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(latencyDict, exit, err = kNoMemoryErr);

    if (inStreamType != kAudioStreamType_Invalid)
        CFDictionarySetInt64(latencyDict, kAudioSessionKey_Type, inStreamType);
    if (inAudioType)
        CFDictionarySetValue(latencyDict, kAudioSessionKey_AudioType, inAudioType);
    if (inSampleRate > 0)
        CFDictionarySetInt64(latencyDict, kAudioSessionKey_SampleRate, inSampleRate);
    if (inSampleSize > 0)
        CFDictionarySetInt64(latencyDict, kAudioSessionKey_SampleSize, inSampleSize);
    if (inChannels > 0)
        CFDictionarySetInt64(latencyDict, kAudioSessionKey_Channels, inChannels);
    CFDictionarySetInt64(latencyDict, kAudioSessionKey_InputLatencyMicros, inInputLatency);
    CFDictionarySetInt64(latencyDict, kAudioSessionKey_OutputLatencyMicros, inOutputLatency);

    result = latencyDict;
    latencyDict = NULL;
    err = kNoErr;
exit:
    CFReleaseNullSafe(latencyDict);
    if (outErr)
        *outErr = err;
    return (result);
}

CFArrayRef GetAudioLatencies(OSStatus* outErr)
{
    CFArrayRef result = NULL;
    OSStatus err;

    // $$$ TODO: obtain audio latencies for all audio formats and audio types supported by the underlying hardware.
    // Audio latencies are reported as an ordered array of dictionaries (from least restrictive to the most restrictive).
    // Each dictionary contains the following keys:
    //		[kAudioSessionKey_Type] - if not specified, then latencies are good for all stream types
    //		[kAudioSessionKey_AudioType] - if not specified, then latencies are good for all audio types
    //		[kAudioSessionKey_SampleRate] - if not specified, then latencies are good for all sample rates
    //		[kAudioSessionKey_SampleSize] - if not specified, then latencies are good for all sample sizes
    //		[kAudioSessionKey_Channels] - if not specified, then latencies are good for all channel counts
    //		[kAudioSessionKey_CompressionType] - if not specified, then latencies are good for all compression types
    //		kAudioSessionKey_InputLatencyMicros
    //		kAudioSessionKey_OutputLatencyMicros

    CFMutableArrayRef audioLatenciesArray;
    CFDictionaryRef dict = NULL;

    audioLatenciesArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    require_action(audioLatenciesArray, exit, err = kNoMemoryErr);

    // MainAudio catch all - set 0 latencies for now - $$$ TODO set real latencies

    dict = _CreateLatencyDictionary(
        kAudioStreamType_MainAudio, // inStreamType
        NULL, // inAudioType
        0, // inSampleRate,
        0, // inSampleSize,
        0, // inChannels,
        0, // inInputLatency,
        0, // inOutputLatency,
        &err);
    require_noerr(err, exit);

    CFArrayAppendValue(audioLatenciesArray, dict);
    ForgetCF(&dict);

    // MainAudio default latencies - set 0 latencies for now - $$$ TODO set real latencies

    dict = _CreateLatencyDictionary(
        kAudioStreamType_MainAudio, // inStreamType
        kAudioStreamAudioType_Default, // inAudioType
        0, // inSampleRate,
        0, // inSampleSize,
        0, // inChannels,
        0, // inInputLatency,
        0, // inOutputLatency,
        &err);
    require_noerr(err, exit);

    CFArrayAppendValue(audioLatenciesArray, dict);
    ForgetCF(&dict);

    // MainAudio media latencies - set 0 latencies for now - $$$ TODO set real latencies

    dict = _CreateLatencyDictionary(
        kAudioStreamType_MainAudio, // inStreamType
        kAudioStreamAudioType_Media, // inAudioType
        0, // inSampleRate,
        0, // inSampleSize,
        0, // inChannels,
        0, // inInputLatency,
        0, // inOutputLatency,
        &err);
    require_noerr(err, exit);

    CFArrayAppendValue(audioLatenciesArray, dict);
    ForgetCF(&dict);

    // MainAudio telephony latencies - set 0 latencies for now - $$$ TODO set real latencies

    dict = _CreateLatencyDictionary(
        kAudioStreamType_MainAudio, // inStreamType
        kAudioStreamAudioType_Telephony, // inAudioType
        0, // inSampleRate,
        0, // inSampleSize,
        0, // inChannels,
        0, // inInputLatency,
        0, // inOutputLatency,
        &err);
    require_noerr(err, exit);

    CFArrayAppendValue(audioLatenciesArray, dict);
    ForgetCF(&dict);

    // MainAudio SpeechRecognition latencies - set 0 latencies for now - $$$ TODO set real latencies

    dict = _CreateLatencyDictionary(
        kAudioStreamType_MainAudio, // inStreamType
        kAudioStreamAudioType_SpeechRecognition, // inAudioType
        0, // inSampleRate,
        0, // inSampleSize,
        0, // inChannels,
        0, // inInputLatency,
        0, // inOutputLatency,
        &err);
    require_noerr(err, exit);

    CFArrayAppendValue(audioLatenciesArray, dict);
    ForgetCF(&dict);

    // Main Audio alert latencies - set 0 latencies for now - $$$ TODO set real latencies

    dict = _CreateLatencyDictionary(
        kAudioStreamType_MainAudio, // inStreamType
        kAudioStreamAudioType_Alert, // inAudioType
        0, // inSampleRate,
        0, // inSampleSize,
        0, // inChannels,
        0, // inInputLatency,
        0, // inOutputLatency,
        &err);
    require_noerr(err, exit);

    CFArrayAppendValue(audioLatenciesArray, dict);
    ForgetCF(&dict);

    // AltAudio catch all latencies - set 0 latencies for now - $$$ TODO set real latencies

    dict = _CreateLatencyDictionary(
        kAudioStreamType_AltAudio, // inStreamType
        NULL, // inAudioType
        0, // inSampleRate,
        0, // inSampleSize,
        0, // inChannels,
        0, // inInputLatency,
        0, // inOutputLatency,
        &err);
    require_noerr(err, exit);

    CFArrayAppendValue(audioLatenciesArray, dict);
    ForgetCF(&dict);

    // AltAudio Media latencies - set 0 latencies for now - $$$ TODO set real latencies

    dict = _CreateLatencyDictionary(
        kAudioStreamType_AltAudio, // inStreamType
        kAudioStreamAudioType_Default, // inAudioType
        0, // inSampleRate,
        0, // inSampleSize,
        0, // inChannels,
        0, // inInputLatency,
        0, // inOutputLatency,
        &err);
    require_noerr(err, exit);

    CFArrayAppendValue(audioLatenciesArray, dict);
    ForgetCF(&dict);

    // MainHighAudio Media latencies (wireless only) - set 0 latencies for now - $$$ TODO set real latencies

    dict = _CreateLatencyDictionary(
        kAudioStreamType_MainHighAudio, // inStreamType
        kAudioStreamAudioType_Media, // inAudioType
        0, // inSampleRate,
        0, // inSampleSize,
        0, // inChannels,
        0, // inInputLatency,
        0, // inOutputLatency,
        &err);
    require_noerr(err, exit);

    CFArrayAppendValue(audioLatenciesArray, dict);
    ForgetCF(&dict);

    // $$$ TODO add more latencies dictionaries as needed

    result = audioLatenciesArray;
    audioLatenciesArray = NULL;
    err = kNoErr;

exit:
    CFReleaseNullSafe(dict);
    CFReleaseNullSafe(audioLatenciesArray);
    if (outErr)
        *outErr = err;
    return (result);
}
