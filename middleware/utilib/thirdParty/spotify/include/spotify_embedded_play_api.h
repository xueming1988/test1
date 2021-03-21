/**
 * \file spotify_embedded_play_api.h
 * \brief The public Spotify Embedded API
 * \copyright Copyright 2016 Spotify AB. All rights reserved.
 *
 */

#ifndef _SPOTIFY_EMBEDDED_PLAY_API_H
#define _SPOTIFY_EMBEDDED_PLAY_API_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \addtogroup Playback
 * @{
 */

/**
 * \brief Indicates that the index value is not set.
 * Note: Can only be used with eSDK 3.
 * \see SpPlayUri
 */
#define SP_NO_INDEX (-1)

/**
 * \brief Maximum length of the type of playback source
 * \see SpSourceInfo, SpPlayUri
 */
#define SP_MAX_SOURCE_TYPE_LENGTH (63)

/**
 * \brief Maximum length of the source URIs
 * \see SpSourceInfo, SpPlayUri
 */
#define SP_MAX_SOURCE_URI_LENGTH (511)

/**
 * \brief Metadata for identifying where a playback request originated from
 * \see SpPlayUri
 */
struct SpSourceInfo {
    /**
     * \brief The type of playlist/context/UI view that caused this playback to start.
     * Note: If set, this MUST be one of the following strings (unless told otherwise):
     *
     * - "album"
     * - "artist"
     * - "playlist"
     * - "playlistfolder"
     * - "radio"
     * - "search"
     * - "toplist"
     * - "unknown"
     *
     * Please contact Spotify if you have a common play source that is not obviously
     * represented in this list.
     *
     */
    char type[SP_MAX_SOURCE_TYPE_LENGTH + 1];

    /**
     * \brief The URI of the parent container, if there is one.
     *
     * For example, if the user selects an album from an artist view, the URI
     * passed to SpPlayUri() is the album to play, and the URI in this source
     * field should be the URI of the artist.
     */
    char uri[SP_MAX_SOURCE_URI_LENGTH + 1];

    /**
     * \brief The URI of the track that is expected to play
     *
     * If the URI to play is a container of multiple tracks (ex: playlist,
     * artist, album), this field can optionally be filled with the track
     * URI that is expected to play.
     * \if tpapi_extensions
     * This can be used to adjust the index when the context has changed
     * or when tracks are unavailable.
     * \endif
     *
     */
    char expected_track_uri[SP_MAX_SOURCE_URI_LENGTH + 1];
};

/**
 * \brief Start local playback of any Spotify URI
 *
 * This call starts playback of a Spotify URI, such as a playlist, album, artist,
 * or track.  Valid Spotify URIs can be obtained via the Spotify Web API.
 *
 * Using this call will 'pull' playback from any other Spotify Connect client
 * active on the same account.
 *
 * Note that there may be a lag between the introduction of new URI types and
 * support for playing them with this call.
 *
 * \param[in] uri URI of resource (ex: spotify:album:6tSdnCBm5HCAjwNxWfUC7m)
 * \param[in] index The zero based index of the item to start playing in the resource
 *                       (for instance, '4' would play the 5th track in a playlist).
 *                       Set to zero if resource does not have multiple items.
 *                       In eSDK 3, this can be set to SP_NO_INDEX if no specific
 *                       index is required. In case of non-shuffled context
 *                       this will result in first track of the context being
 *                       played whereas in shuffled context this will result
 *                       in a random track being played.
 * \param[in] offset_ms The time offset to start playing specified track from.
 *                      Set to zero to start from the beginning.
 * \param[in] source Metadata about what user action caused this playback event,
 *                   and the expected result.  This information is used to correct
 *                   behavior when the exact request isn't possible, and to enable
 *                   better recommendations and suggestions for users.  This field
 *                   is optional, but MUST be provided whenever possible.  Failure
 *                   to fill in this data accurately will result in a downgraded
 *                   Spotify experience.
 * \param[in] alias_index The index of the device alias to start playback on.
 *                   If aliases aren't used, pass -1.
 *
 * \see SpSourceInfo, SP_NO_INDEX
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpPlayUri(const char *uri, int index, int offset_ms,
                                const struct SpSourceInfo *source, int alias_index);

/**
 * \brief Start local playback of any Spotify URI with an optional track UID
 *
 * This call will start playback of any Spotify context URI. In addition,
 * it allows the client to provide a UID to start the playback from a given
 * track in the context.
 *
 * The main difference between UIDs and the expected_track_uri parameter in
 * SpPlayUri is that while one track URI can occur more than once in the
 * context, UIDs are unique for each track in it.
 *
 * Valid Spotify URIs can be obtained via the Spotify Web API. Track UIDs are
 * currently available when resolving contexts from a small subset of services.
 *
 * Using this call will 'pull' playback from any other Spotify Connect client
 * active on the same account.
 *
 * \param[in] uri URI of resource (ex: spotify:album:6tSdnCBm5HCAjwNxWfUC7m)
 * \param[in] from_uid The UID to identify a unique track in the context.
 * \param[in] offset_ms The time offset to start playing specified track from.
 *                      Set to zero to start from the beginning.
 * \param[in] source Metadata about what user action caused this playback event,
 *                   and the expected result.  This information is used to correct
 *                   behavior when the exact request isn't possible, and to enable
 *                   better recommendations and suggestions for users.  This field
 *                   is optional, but MUST be provided whenever possible.  Failure
 *                   to fill in this data accurately will result in a downgraded
 *                   Spotify experience.
 * \param[in] alias_index The index of the device alias to start playback on.
 *                   If aliases aren't used, pass -1.
 *
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpPlayContextUri(const char *uri, const char *from_uid, int offset_ms,
                                       const struct SpSourceInfo *source, int alias_index);

/**
 * \brief Queue a URI. The track will be in queue only after
 * \ref kSpPlaybackNotifyQueuedTrackAccepted event is raised, but for UI purposes one
 * can choose to monitor for \ref kSpPlaybackNotifyMetadataChanged instead.
 *
 * \param[in] uri URI to queue.
 *
 * \return Returns an error code.
 */
SP_EMB_PUBLIC SpError SpQueueUri(const char *uri);

/**
 * \brief Become active without starting to play
 *
 * \param[in] alias_index The index of the device alias that should become active.
 *                   If aliases aren't used, pass -1.
 *
 * This call makes the device active, without starting to play anything, if
 * playback is paused or no device is active. The device is active after
 * the \ref kSpPlaybackNotifyBecameActive event is received. If another
 * Connect-enabled device was active and playing, it will be interrupted.
 *
 * If the currently active device is playing, this call behaves like
 * SpPlaybackPlay().
 *
 * If device aliases are used, this function will switch the selected
 * alias. If the selected alias was changed during playback, the playback
 * will continue uninterrupted with the new alias.
 *
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpPlaybackBecomeActiveDevice(int alias_index);

/**
 * @}
 */

/**
 * \addtogroup Preset
 * @{
 */

/**
 * \brief Extension for SpPlayPreset: Play a saved preset from different position,
 * optionally with source info.
 *
 * This call extends \see SpPlayPreset with extra parameters to start from a
 * different playback position than was saved in the preset.  The preset will
 * normally resume from the beginning of the track that was playing when the
 * playlist was saved.  The extra arguments in SpPlayPresetEx() allow you to
 * specify a track index relative to the saved preset index, and a position
 * within the track to start from.  This can be used to resume from the last
 * playback position without calling SpPresetSubscribe() frequently.
 *
 * You can also pass a SpSourceInfo struct in the same way as for SpPlayUri.
 * This makes it possible to substitute SpPlayUri calls with SpPlayPresetEx
 * if you want to resume playback.
 *
 * Note: This is an experimental extension, and is not guaranteed to be stable.
 *
 * \param[in] preset_id Same as \see SpPlayPreset()
 * \param[in] buffer Preset blob obtained with SpPresetSubscribe()
 * \param[in] buff_size The size of \a buffer
 * \param[in] relative_index Index relative to the track index saved in the
 *                           preset.  For instance, if the preset is saved on
 *                           track 10 and relative_index is 2, the track that
 *                           plays will be the 12th in the context.  Negative
 *                           indexing is permitted.  Set to 0 to start from the
 *                           index saved in the preset.
 * \param[in] offset_ms The time offset to start playing specified track from.
 *                      Set to zero to start from beginning.  Specified in
 *                      milliseconds.
 * \param[in] source Metadata about what user action caused this playback event,
 *                   and the expected result. This should be filled in if
 *                   SpPlayPresetEx() is invoked as a substitute for SpPlayUri().
 * \param[in] alias_index The index of the device alias to start playback on.
 *                   If aliases aren't used, pass -1.
 *
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpPlayPresetEx(int preset_id, uint8_t *buffer, size_t buff_size,
                                     int relative_index, uint32_t offset_ms,
                                     const struct SpSourceInfo *source, int alias_index);

/**
 * @}
 */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _SPOTIFY_EMBEDDED_PLAY_API_H */
