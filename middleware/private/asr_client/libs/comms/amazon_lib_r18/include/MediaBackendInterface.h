/* Copyright 2018-present Amazon.com, Inc. or its affiliates. All Rights Reserved. */

#ifndef ALEXA_COMMS_MEDIA_BACKEND_INTERFACE_H_
#define ALEXA_COMMS_MEDIA_BACKEND_INTERFACE_H_

#include "AlexaCommsAPI.h"

#include <stdbool.h>
#include <stdint.h>

#include "MediaErrorCodes.h"


/*
*
* MediaBackendInterface v1.0.0
*
* This is the common C interface for all Media Backend types
*
* @note Audio format for input/output is fixed: Mono, 16 kHz, 16-bit signed PCM
* @note See ProxyMediaBackend.cpp for the Proxy Media Backend documentation
*
*/

#ifdef __cplusplus
extern "C"
{
#endif

// Media Backend API version
struct MediaBackendVersion {
    uint32_t    major;
    uint32_t    minor;
    uint32_t    patch;
    const char *strVersion;
};

#ifdef USE_PROXY_BACKEND
// Proxy Media Backend Config
struct MediaBackendConfig {
    bool        logReadLatency;                             //! logs the read latency when calling MediaBackend_readAudio
    bool        enableResync;                               //! disable/enable audio input resynchronization
    uint32_t    bufferSizeInMilliseconds;                   //! buffer size in milliseconds
    uint32_t    maxReadRetryCount;                          //! max number of retries (0 for infinite retries)
    uint32_t    readTimeOutInMilliseconds;                  //! read time out in msec (0 for no time out)
    uint32_t
    targetReadLatencyInMilliseconds;            //! how far behind the writer should be from the reader after resynchronizing
    uint32_t
    readLatencyThresholdInMilliseconds;         //! max read latency in msec before audio input is resynchronized
};

// Proxy Media Backend Debug Info
struct MediaBackendDebugInfo {
    uint32_t    resyncCount;                                //! number of times the audio input has been resynchronized
    uint32_t    readRetryCount;                             //! number of times reading from an audio input has been retried
    uint64_t    samplesRead;                                //! total samples read from audio input stream
    uint64_t    samplesWritten;                             //! total samples written to audio output stream
};

// Note: Mediabackend_createMaster is not exported in the DSO as it is for Alexa Comms lib internal use only
MediaError MediaBackend_createMaster(const MediaBackendConfig &config);

#else
struct MediaBackendConfig {};
struct MediaBackendDebugInfo {};
#endif

//! @brief Get version
ALEXACOMMSLIB_API MediaBackendVersion MediaBackend_getVersion();

//! @brief Get a default config
ALEXACOMMSLIB_API MediaBackendConfig MediaBackend_getDefaultConfig();

//! @brief Get debug info
ALEXACOMMSLIB_API MediaBackendDebugInfo MediaBackend_getDebugInfo();

//! @brief Create backend
ALEXACOMMSLIB_API MediaError MediaBackend_create(MediaBackendConfig config);

//! @brief Destroy backend
ALEXACOMMSLIB_API MediaError MediaBackend_destroy();

//! @brief Create audio input stream
ALEXACOMMSLIB_API MediaError MediaBackend_createAudioInputStream();

//! @brief Read audio samples from audio input stream
//! @note Reads numSamples of audio from the audio input stream to given output buffer
//        Blocks until all samples are available to read
//! @param[in] outputBuffer Buffer to write samples into
//! @param[in] numSamples Number of samples to read
ALEXACOMMSLIB_API MediaError MediaBackend_readAudio(int16_t *outputBuffer, uint32_t numSamples);

//! @brief Create audio input stream
ALEXACOMMSLIB_API MediaError MediaBackend_destroyAudioInputStream();

//! @brief Create audio output stream
ALEXACOMMSLIB_API MediaError MediaBackend_createAudioOutputStream();

//! @brief Write audio samples
//! @note Writes numSamples of audio from given buffer to audio output stream
//        Overwrites if output buffer is full
//! @param[in] inputBuffer Buffer to read samples from
//! @param[in] numSamples Number of samples to write
ALEXACOMMSLIB_API MediaError MediaBackend_writeAudio(const int16_t *inputBuffer, uint32_t numSamples);

//! @brief Destroy audio output stream
ALEXACOMMSLIB_API MediaError MediaBackend_destroyAudioOutputStream();

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ALEXA_COMMS_MEDIA_BACKEND_INTERFACE_H_
