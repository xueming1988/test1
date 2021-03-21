#include <stdio.h>

#include "asr_state.h"

static AudioInputProcessorObserverState _AudioInputProcessorObserverState = IDLE;

void SetAudioInputProcessorObserverState(AudioInputProcessorObserverState new_state)
{
    printf("%s %d => %d\n", __FUNCTION__, _AudioInputProcessorObserverState, new_state);
    _AudioInputProcessorObserverState = new_state;
}

AudioInputProcessorObserverState GetAudioInputProcessorObserverState(void *handle)
{
    return _AudioInputProcessorObserverState;
}
