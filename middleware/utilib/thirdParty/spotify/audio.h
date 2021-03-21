/**
 * Copyright (c) 2013-2014 Spotify AB
 *
 * This file is part of the Spotify Embedded SDK examples suite.
 */

#ifndef _AUDIO_H
#define _AUDIO_H

#include "spotify_embedded.h"

typedef SpError (*SpExampleAudioInit)(void);
typedef SpError (*SpExampleAudioDeInit)(void);
typedef SpError (*SpExampleAudioFlush)(void);
typedef size_t (*SpExampleCallbackAudioData)(const int16_t *samples, size_t sample_count,
                                             const struct SpSampleFormat *sample_format,
                                             uint32_t *samples_buffered, void *context);
typedef void (*SpExampleAudioPause)(int pause);
typedef void (*SpExampleAudioSetVolume)(int vol);
typedef void (*SpExampleAudioTrackChanged)(int duration_ms);
typedef uint32_t (*SpExampleGetBufferedMs)(void);
typedef void (*SpExampleResetWrittenFrames)(void);
typedef uint32_t (*SpExampleGetWrittenFrames)(void);

struct SpExampleAudioCallbacks {
    SpExampleAudioInit audio_init;
    SpExampleAudioDeInit audio_deinit;
    SpExampleAudioFlush audio_flush;
    SpExampleCallbackAudioData audio_data;
    SpExampleAudioPause audio_pause;
    SpExampleAudioSetVolume audio_volume;
    SpExampleAudioTrackChanged audio_changed;
    SpExampleGetBufferedMs audio_get_buffered_ms;
    SpExampleGetWrittenFrames audio_get_written_frames;
    SpExampleResetWrittenFrames audio_reset_written_frames;
};

void SpExampleAudioGetCallbacks(struct SpExampleAudioCallbacks *callbacks);

#endif // _AUDIO_H
