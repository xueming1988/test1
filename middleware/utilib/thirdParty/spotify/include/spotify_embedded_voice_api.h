/**
 * \file spotify_embedded_voice_api.h
 * \brief The public Spotify Embedded API
 * \copyright Copyright 2017 - 2019 Spotify AB. All rights reserved.
 *
 */

#ifndef _SPOTIFY_EMBEDDED_VOICE_API_H
#define _SPOTIFY_EMBEDDED_VOICE_API_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \addtogroup Playback
 * @{
 */

/**
 * \brief Perform Natural Language voice action
 *
 * Using this call might start playback, 'pulling' it from any other Spotify
 * Connect client active on the same account.
 *
 * \param[in] action Natural Language operation
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpPerformVoiceAction(const char *action);

/**
 * \brief Callback for notifying the application about the status of a requested
 *        voice action
 *
 * To register this callback, use the function SpRegisterVoiceCallbacks().
 *
 * \param[in] status The result of the last action
 * \param[in] ack_id Acknowledgment ID of the last action (if successful, otherwise NULL)
 * \param[in] context Context pointer that was passed when registering the callback
 *
 * \note The application should not block or call API functions that are not allowed
 * in the callback.
 */
typedef void (*SpCallbackVoiceActionStatus)(SpError status, const char *ack_id, void *context);

/**
 * \brief Callbacks to be registered with SpRegisterVoiceCallbacks()
 *
 * Any of the pointers may be NULL.
 *
 * See the documentation of the callback typedefs for information about the
 * individual callbacks.
 */
struct SpVoiceCallbacks {
    /** \brief Voice action status callback */
    SpCallbackVoiceActionStatus on_action;
};

/**
 * \brief Register callbacks related to voice
 *
 * \param[in] cb Structure with pointers to individual callback functions.
 *              Any of the pointers in the structure may be NULL.
 * \param[in] context Application-defined pointer that will be passed unchanged as
 *              the \a context argument to the callbacks.
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpRegisterVoiceCallbacks(struct SpVoiceCallbacks *cb, void *context);

/**
 * @}
 */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _SPOTIFY_EMBEDDED_VOICE_API_H */
