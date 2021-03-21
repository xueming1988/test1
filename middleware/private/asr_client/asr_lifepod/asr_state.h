#ifndef _ASR_STATE_H__
#define _ASR_STATE_H__

// state from avs-device-sdk AudioInputProcessorObserverInterface::State
/// A state observer for an @c AudioInputProcessor.
typedef enum {
    /// In this state, the @c AudioInputProcessor is not waiting for or transmitting speech.
    IDLE,

    /// In this state, the @c AudioInputProcessor is waiting for a call to recognize().
    EXPECTING_SPEECH,

    /// In this state, the @c AudioInputProcessor is actively streaming speech.
    RECOGNIZING,

    /**
    * In this state, the @c AudioInputProcessor has finished streaming and is waiting for completion
    * of an Event.
    * Note that @c recognize() calls are not permitted while in the @c BUSY state.
    */
    BUSY
} AudioInputProcessorObserverState;

#if defined __cplusplus
extern "C" {
#endif
void SetAudioInputProcessorObserverState(AudioInputProcessorObserverState new_state);
AudioInputProcessorObserverState GetAudioInputProcessorObserverState(void *handle);

#if defined __cplusplus
}
#endif

#endif
