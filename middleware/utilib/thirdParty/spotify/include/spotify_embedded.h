/**
 * \file spotify_embedded.h
 * \brief The public Spotify Embedded API
 * \copyright Copyright 2017 - 2020 Spotify AB. All rights reserved.
 */

#ifndef _SPOTIFY_EMBEDDED_H
#define _SPOTIFY_EMBEDDED_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER) && defined(_WIN32)
/* Define standard integer types for Microsoft Visual C++. */
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned uint32_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;
#elif defined(SP_USE_INTERNAL_TYPES)
#include <sp_types.h>
#else
#include <stdint.h>
#endif
#include <stddef.h>

/** \brief Public API export definition */
#ifndef SP_EMB_PUBLIC
#if __GNUC__ >= 4
#define SP_EMB_PUBLIC __attribute__((__visibility__("default")))
#elif defined(_MSC_VER)
#ifdef WINDOWS_UWP
#include <winapifamily.h>
#endif
#ifndef SP_EMB_STATIC_LIBRARY
#ifdef SP_EMB_EXPORTS
#define SP_EMB_PUBLIC __declspec(dllexport)
#else
#define SP_EMB_PUBLIC __declspec(dllimport)
#endif
#else
#define SP_EMB_PUBLIC
#endif
#else
#define SP_EMB_PUBLIC
#endif
#endif

/**
 * \defgroup Session Session
 *
 * \brief Initialization, configuration, connection, event loop
 */

/**
 * \defgroup Playback Playback
 *
 * \brief Playback- and metadata-related functionality
 */

/**
 * \defgroup Preset Preset
 *
 * \brief Preset-related functionality
 */

/**
 * \defgroup ZeroConf ZeroConf
 *
 * \brief Functionality related to the \ref ZeroConf
 */

/**
 * \defgroup Errors Errors
 *
 * \brief Error codes
 */

/**
 * \addtogroup Session
 * @{
 */

/**
 * \brief The version of the API defined in this header file.
 * \see SpInit
 */
#define SP_API_VERSION 57

/**
 * \brief Minimum recommended size of the buffer SpConfig::memory_block
 */
#define SP_RECOMMENDED_MEMORY_BLOCK_SIZE (1024 * 1024)

/**
 * \brief Maximum length of the brand name string (not counting terminating NULL)
 * \see SpConfig
 */
#define SP_MAX_BRAND_NAME_LENGTH (32)

/**
 * \brief Maximum length of the model name string (not counting terminating NULL)
 * \see SpConfig
 */
#define SP_MAX_MODEL_NAME_LENGTH (30)

/**
 * \brief Maximum length of the client id string (not counting terminating NULL)
 * \see SpConfig
 */
#define SP_MAX_CLIENT_ID_LENGTH (32)

/**
 * \brief Maximum length of the os version string (not counting terminating NULL)
 * \see SpConfig
 */
#define SP_MAX_OS_VERSION_LENGTH (64)

/**
 * \brief Maximum length of the device display name (not counting terminating NULL)
 * \see SpSetDisplayName, SpConfig, SpZeroConfVars
 */
#define SP_MAX_DISPLAY_NAME_LENGTH (64)

/**
 * \brief Maximum length of the device's unique ID (not counting terminating NULL)
 * \see SpConfig
 */
#define SP_MAX_UNIQUE_ID_LENGTH (64)

/**
 * \brief Maximum length of usernames (not counting terminating NULL)
 * \see SpZeroConfVars, SpConnectionLoginZeroConf
 */
#define SP_MAX_USERNAME_LENGTH (64)

/**
 * \brief Maximum number of device aliases that can be configured.
 * \see SpConfig.device_aliases
 */
#define SP_MAX_DEVICE_ALIASES (8)

/**
 * \brief A value to use for alias_index when aliases are not used.
 * \see SpConfig.device_aliases
 */
#define SP_NO_ALIAS_SELECTED (-1)

/**
 * @}
 */

/**
 * \addtogroup Playback
 * @{
 */

/**
 * \brief Maximum length of display names in track metadata (not counting terminating NULL)
 * \note It is possible that metadata will be truncted to less than this length.
 * Applications requiring full length metadata should request it from the Spotify
 * web APIs (https://developer.spotify.com/documentation/web-api/)
 * \see SpGetMetadata, SpMetadata
 */
#define SP_MAX_METADATA_NAME_LENGTH (255)

/**
 * \brief Maximum length of URIs in track metadata (not counting terminating NULL)
 * \see SpGetMetadata, SpMetadata
 */
#define SP_MAX_METADATA_URI_LENGTH (127)

/**
 * \brief Maximum length of Track UID in track metadata (not counting terminating NULL)
 * \see SpGetMetadata, SpMetadata
 */
#define SP_MAX_TRACK_UID_LENGTH (64)

/**
 * \brief Maximum length of URLs (not counting terminating NULL)
 * \see SpGetMetadataImageURL
 */
#define SP_MAX_METADATA_IMAGE_URL_LENGTH (255)

/**
* \brief Length of player cookie (not including terminating null)
* \see SpGetPlayerCookie
*/
#define SP_PLAYER_COOKIE_LENGTH (32)

/**
 * \brief Maximum length of Playback-Id (not counting terminating NULL)
 * \see SpGetMetadata, SpMetadata
 */
#define SP_MAX_PLAYBACK_ID_LENGTH (32)

/**
 * @}
 */

/**
 * \addtogroup ZeroConf
 * @{
 */

/**
 * \brief Maximum length of the public key used in ZeroConf logins
 *        (not counting terminating NULL)
 * \see SpZeroConfVars
 */
#define SP_MAX_PUBLIC_KEY_LENGTH (149)

/**
 * \brief Maximum length of the device ID used for ZeroConf logins
 *        (not counting terminating NULL)
 * \see SpZeroConfVars
 */
#define SP_MAX_DEVICE_ID_LENGTH (64)

/**
 * \brief Maximum length of the device type string
 *        (not counting terminating NULL)
 * \see SpZeroConfVars
 */
#define SP_MAX_DEVICE_TYPE_LENGTH (15)

/**
 * \brief Maximum length of the library version string
 *        (not counting terminating NULL)
 * \see SpZeroConfVars
 */
#define SP_MAX_VERSION_LENGTH (30)

/**
 * \brief Maximum length of the group status string
 *        (not counting terminating NULL)
 * \see SpZeroConfVars
 */
#define SP_MAX_GROUP_STATUS_LENGTH (15)

/**
 * \brief Maximum length of the token type used for OAuth logins
 *        (not counting terminating NULL)
 * \see SpZeroConfVars
 */
#define SP_MAX_TOKEN_TYPE_LENGTH (30)

/**
 * \brief Maximum length of the scope used for OAuth login
 *        (not counting terminating NULL)
 * \see SpZeroConfVars
 */
#define SP_MAX_SCOPE_LENGTH (64)

/**
 * \brief Maximum length of the client key.
 *        (not counting terminating NULL)
 * \see SpConnectionLoginZeroConf
 */
#define SP_MAX_CLIENT_KEY_LENGTH (511)

/**
 * \brief Maximum length of the zeroconf blob.
 *        (not counting terminating NULL)
 * \see SpConnectionLoginZeroConf
 */
#define SP_MAX_ZEROCONF_BLOB_LENGTH (2047)

/**
 * \brief Maximum length of the login ID used for ZeroConf logins
 *        (not counting terminating NULL)
 * \see SpConnectionLoginZeroConf
 */
#define SP_MAX_LOGIN_ID_LENGTH (64)

/**
 * \brief Maximum length of the availability string
 *        (not counting terminating NULL)
 * \see SpConnectionLoginZeroConf
 */
#define SP_MAX_AVAILABILITY_LENGTH (15)

/**
 * \brief Maximum length of the Partner Name (TSP_PARTNER_NAME)
 *        (not counting terminating NULL)
 *        The longest partner name when this was written was
 *        "imagination_technologies_mips" at 29 characters
 */
#define SP_MAX_PARTNER_NAME_LENGTH (48)

/**
 * \brief Maximum length of filename fields
 *        (not counting terminating NULL)
 */
#define SP_MAX_FILENAME_LENGTH (63)

/**
 * \brief Maximum length of the preset blob returned by SpGetPreset()
 *
 * \see SpGetPreset()
 */
#define SP_PRESET_BUFFER_SIZE (2064)

/**
 * \brief Value for SpConfig::zeroconf_serve when
 *        disabling builtin ZeroConf stack.
 *        Complete ZeroConf stack must be run externally.
 * \see SpInit
 */
#define SP_ZEROCONF_DISABLED (0)

/**
 * \brief Value for SpConfig::zeroconf_serve when
 *        activating complete builtin ZeroConf stack.
 * \see SpInit
 */
#define SP_ZEROCONF_SERVE (1)

/**
 * \brief Value for SpConfig::zeroconf_serve when
 *        activating builtin ZeroConf http server only
 *        while running the ZeroConf mDNS server externally.
 * \see SpInit
 */
#define SP_ZEROCONF_SERVE_HTTP_ONLY (2)

/**
 * \brief Value for SpConfig::zeroconf_serve when
 *        activating builtin ZeroConf mDNS server only
 *        while running the ZeroConf http server externally.
 * \see SpInit
 */
#define SP_ZEROCONF_SERVE_MDNS_ONLY (3)

/**
 * \brief Value for SpConfig::scope when
 *        implementing a basic streaming device.
 */
#define SP_SCOPE_STREAMING "streaming"

/**
 * \brief Set this bit in the device alias attributes integer
 * (SpDeviceAlias::attributes) to mark a device alias as representing a group.
 *
 * \note \ref SpSetDeviceIsGroup also sets group status, but only for the currently
 * selected alias.
 */
#define SP_DEVICE_ALIAS_ATTRIBUTE_GROUP (1)

/**
 * \brief Set this bit in the global attributes integer
 * (SpConfig::global_attributes) to mark that this device supports voice.
 */
#define SP_GLOBAL_ATTRIBUTE_VOICE (2)

/**
 * @}
 */

/**
 * \addtogroup Errors
 * @{
 */

/**
 * \brief Error codes
 *
 * These are the possible status codes returned by functions in the SDK. They
 * should be used to determine if an action was successful, and if not, why the
 * action failed.
 *
 * \see SpCallbackError
 */
typedef enum {
    /** \brief The operation was successful. */
    kSpErrorOk = 0,

    /** \brief The operation failed due to an unspecified issue. */
    kSpErrorFailed = 1,

    /**
     * \brief The library could not be initialized.
     * \see SpInit
     */
    kSpErrorInitFailed = 2,

    /**
     * \brief The library could not be initialized because of an incompatible API version.
     *
     * When calling SpInit(), you are required to set the field SpConfig::api_version
     * to SP_API_VERSION. This error indicates that the library that the application
     * is linked against was built for a different SP_API_VERSION. There might be
     * an issue with the include or library paths in the build environment,
     * or the wrong Embedded SDK shared object is loaded at runtime.
     *
     * \see SpInit, SpConfig::api_version
     */
    kSpErrorWrongAPIVersion = 3,

    /**
     * \brief An unexpected NULL pointer was passed as an argument to a function.
     */
    kSpErrorNullArgument = 4,

    /** \brief An unexpected argument value was passed to a function. */
    kSpErrorInvalidArgument = 5,

    /** \brief A function was invoked before SpInit() or after SpFree() was called. */
    kSpErrorUninitialized = 6,

    /** \brief SpInit() was called more than once. */
    kSpErrorAlreadyInitialized = 7,

    /**
     * \brief Login to Spotify failed because of invalid credentials.
     * \see SpConnectionLoginPassword, SpConnectionLoginBlob, SpConnectionLoginZeroConf
     */
    kSpErrorLoginBadCredentials = 8,

    /** \brief The operation requires a Spotify Premium account */
    kSpErrorNeedsPremium = 9,

    /** \brief The Spotify user is not allowed to log in from this country. */
    kSpErrorTravelRestriction = 10,

    /**
     * \brief The application has been banned by Spotify.
     *
     * This most likely means that the client_id specified in SpConfig::client_id
     * has been denylisted.
     */
    kSpErrorApplicationBanned = 11,

    /**
     * \brief An unspecified login error occurred.
     *
     * In order to help debug the issue, the application should register the callback
     * SpCallbackDebugMessage(), which receives additional information
     * about the error.
     */
    kSpErrorGeneralLoginError = 12,

    /**
     * \brief The operation is not supported.
     */
    kSpErrorUnsupported = 13,

    /**
     * \brief The operation is not supported if the device is not the active playback
     *        device.
     * \see SpPlaybackIsActiveDevice
     */
    kSpErrorNotActiveDevice = 14,

    /**
     * \brief The API has been rate-limited.
     *
     * The API is rate-limited if it is asked to perform too many actions
     * in a short amount of time.
     */
    kSpErrorAPIRateLimited = 15,

    /**
     * \brief The eSDK API was used from a callback.
     *
     * This eSDK API function cannot be used from callbacks.
     */
    kSpErrorReentrancyDetected = 16,

    /**
     * \brief The eSDK API was used from multiple threads.
     *
     * This eSDK API function does not support multi-threading.
     *
     * \see \ref threading
     */
    kSpErrorMultiThreadingDetected = 17,

    /**
     * \brief The eSDK API cannot be performed right now.
     *
     * This eSDK API cannot be performed right now, but you are free to retry.
     */
    kSpErrorTryAgain = 18,

    /**
     * \brief Logout failed during SpFree() call.
     */
    kSpErrorDuringLogout = 19,

    /**
     * \brief Permanent connection error. eSDK ceased attempts to reconnect.
     */
    kSpErrorPermanentConnectionError = 21,

    /**
     * \brief Failed to get cryptographic random data from the platform.
     */
    kSpErrorEntropyFailure = 22,

    /**
         * \brief Error range reserved for ZeroConf-related errors.
         *
         * \see \ref codes
         */
    kSpErrorZeroConfErrorStart = 100,

    /** \brief ZeroConf Web server problem or critically malformed request
         */
    kSpErrorZeroConfBadRequest = 102,

    /** \brief Fallback when no other ZeroConf error applies
         */
    kSpErrorZeroConfUnknown = 103,

    /** \brief ZeroConf device does not implement this feature
         */
    kSpErrorZeroConfNotImplemented = 104,

    /** \brief Spotify not installed (where applicable)
         */
    kSpErrorZeroConfNotInstalled = 105,

    /** \brief Spotify not loaded (where applicable)
         */
    kSpErrorZeroConfNotLoaded = 106,

    /** \brief Spotify client not authorized to play
         */
    kSpErrorZeroConfNotAuthorized = 107,

    /** \brief Spotify cannot be loaded (where applicable)
         */
    kSpErrorZeroConfCannotLoad = 108,

    /** \brief Device system needs update (where applicable)
         */
    kSpErrorZeroConfSystemUpdateRequired = 109,

    /** \brief Spotify client application needs update
         */
    kSpErrorZeroConfSpotifyUpdateRequired = 110,

    /** \brief Spotify returned error when trying to login
         */
    kSpErrorZeroConfLoginFailed = 202,

    /** \brief ZeroConf login failed due to an invalid public key
         */
    kSpErrorZeroConfInvalidPublicKey = 203,

    /** \brief ZeroConf HTTP request has no action parameter
         */
    kSpErrorZeroConfMissingAction = 301,

    /** \brief ZeroConf HTTP request has unrecognized action parameter
         */
    kSpErrorZeroConfInvalidAction = 302,

    /** \brief Incorrect or insufficient ZeroConf arguments supplied for requested action
         */
    kSpErrorZeroConfInvalidArguments = 303,

    /** \brief Attempted Spotify action but no valid Spotify session is available (where applicable)
         */
    kSpErrorZeroConfNoSpotifySession = 401,

    /** \brief A Spotify API call returned an error not covered by other error messages
     */
    kSpErrorZeroConfSpotifyError = 402,

    /**
     * \brief Error range reserved for playback-related errors.
     */
    kSpErrorPlaybackErrorStart = 1000,

    /** \brief An unspecified playback error occurred.
     *
     * In order to help debug the issue, the application should register the callback
     * SpCallbackDebugMessage(), which receives additional information
     * about the error.
     */
    kSpErrorGeneralPlaybackError = 1001,

    /**
     * \brief The application has been rate-limited.
     *
     * The application is rate-limited if it requests the playback of too many
     * tracks within a given amount of time.
     */
    kSpErrorPlaybackRateLimited = 1002,

    /**
     * \brief The Spotify user has reached a capping limit that is in effect
     *        in this country and/or for this track.
     */
    kSpErrorPlaybackCappingLimitReached = 1003,

    /**
     * \brief Cannot change track while ad is playing.
     */
    kSpErrorAdIsPlaying = 1004,

    /**
     * \brief The track is (temporarily) corrupt in the Spotify catalogue.
     *
     * This track will be skipped because it cannot be downloaded.  This is a
     * temporary issue with the Spotify catalogue that will be resolved.  The
     * error is for informational purposes only.  No action is required.
     */
    kSpErrorCorruptTrack = 1005,

    /**
     * \brief Unable to read all tracks from the playing context.
     *
     * Playback of the Spotify context (playlist, album, artist, radio, etc) will
     * stop early because eSDK is unable to retrieve more tracks.
     *
     * This could be caused by temporary communication or server problems, or
     * by the underlying context being removed or shortened during playback (for
     * instance, the user deleted all tracks in the playlist while listening.)
     */
    kSpErrorContextFailed = 1006,

    /**
     * \brief The item that was being prefetched was unavailable, and cannot be fetched.
     * This could be due to an invalid URI, changes in track availability, or geographical
     * limitations.
     * This is a permanent error, and the item should not be tried again.
     */
    kSpErrorPrefetchItemUnavailable = 1007,

    /**
     * \brief An item is already actively being prefetched. You must stop the current prefetch
     * request to start another one.
     * This error is only relevant for builds with offline storage enabled.
     */
    kSpAlreadyPrefetching = 1008,

    /**
     * \brief A permanent error was encountered while reading to a registered file storage callback.
     * This error is only relevant for builds with offline storage enabled.
     */
    kSpStorageReadError = 1009,

    /**
     * \brief A permanent error was encountered while writing to a registered file storage callback.
     * This error is only relevant for builds with offline storage enabled.
     */
    kSpStorageWriteError = 1010,

    /**
     * \brief Prefetched item was not fully downloaded or failed. If error happens prefetch can be
     * retried.
     * This error is only relevant for builds with offline storage enabled.
     */
    kSpPrefetchDownloadFailed = 1011,

    /**
     * \brief Current API call cannot be completed because eSDK is busy.
     * Same API call should be done sometime later with same arguments.
     */
    kSpErrorBusy = 1012,

    /**
     * \brief Current API call cannot be completed because the said operation
     * is not available at the moment.
     */
    kSpErrorUnavailable = 1013,

    /**
     * \brief This eSDK API is not allowed due to current license restrictions
     */
    kSpErrorNotAllowed = 1014,

    /**
     * \brief Current API call cannot be completed since it's not connected
     * to Spotify.
     */
    kSpErrorNetworkRequired = 1015,

    /**
     * \brief Current API call cannot be completed without being logged in.
     */
    kSpErrorNotLoggedIn = 1016,

    /**
     * \brief Used in callbacks to notify the application that the action is not yet complete.
     */
    kSpErrorInProgress = 1017,

    /**
     * \brief Used in SpCallbackError callback to notify the application that playback
     * initiation failed.
     */
    kSpErrorPlaybackInitiation = 1018,
    /**
     * \brief Used in SpCallbackError callback to notify the application that
     * recalling and playing a preset failed.
     */
    kSpErrorPresetFailed = 1019,

    /**
     * \brief Used in SpCallbackError callback to notify the application that
     * the request was rejected as invalid.
     */
    kSpErrorInvalidRequest = 1020

} SpError;

/**
 * \brief Enum describes return codes that public API functions can report to eSDK
 */
enum SpAPIReturnCode {
    /**
     * \brief This code should be used when API call was successful
     */
    kSpAPINoError = 0,
    /**
     * \brief This code means operation cannot be performed right now.
     * Same API call should be done sometime later with same arguments.
     */
    kSpAPITryAgain = -10000,
    /**
     * \brief Use to notify about any DNS lookup error that cannot be retried
     */
    kSpAPIDNSLookupError = -10001,
    /**
     * \brief API call has failed
     */
    kSpAPIGenericError = -10002,
    /**
     * \brief Requested feature is not supported by platform
     */
    kSpAPINotSupported = -10003,
    /**
     * \brief End of file/socket reached
     */
    kSpAPIEOF = -10004,
    /**
     * \brief Requested resource does not exist
     */
    kSpAPINotFound = -10005
};

/**
 * @}
 */

/**
 * \addtogroup Playback
 * @{
 */

/**
 * \brief Valid bitrate values.
 * \see SpPlaybackSetBitRate
 */
enum SpPlaybackBitrate {
    /**
     * \brief Set bitrate to the default (currently High).
     */
    kSpPlaybackBitrateDefault = 0,

    /**
     * \brief Set bitrate to 96 kbit/s
     */
    kSpPlaybackBitrateLow = 1,

    /**
     * \brief Set bitrate to 160 kbit/s
     */
    kSpPlaybackBitrateNormal = 2,

    /**
     * \brief Set bitrate to 320 kbit/s
     */
    kSpPlaybackBitrateHigh = 3
};

/**
 * \brief Playback-related notification events
 * \see SpCallbackPlaybackNotify
 */
enum SpPlaybackNotification {
    /**
     * \brief Playback has started or has resumed
     *
     * If the device is the active speaker (according to SpPlaybackIsActiveDevice()),
     * and it has audio data that was delivered by audio data callback function
     * left in its buffers, the application must resume playing the audio data.
     *
     * If the device is not the active speaker, it means that another device
     * that is being observed has resumed playback. See \ref observing.
     *
     * \see SpPlaybackPlay, SpPlaybackIsPlaying, \ref observing
     */
    kSpPlaybackNotifyPlay = 0,

    /**
     * \brief Playback has been paused
     *
     * If the device is the active speaker (according to SpPlaybackIsActiveDevice()),
     * the application must stop playing audio immediately.
     *
     * If the device is not the active speaker, it means that another device
     * that is being observed has paused. See \ref observing.
     *
     * \note The application is not supposed to discard audio data that has been
     * delivered by audio data callback function but that has not been played yet.
     * This audio data still needs to be played when playback resumes by calling #SpPlaybackPlay()
     * (#kSpPlaybackNotifyPlay).
     *
     * \see SpPlaybackPause, SpPlaybackIsPlaying, \ref observing
     */
    kSpPlaybackNotifyPause = 1,

    /**
     * \brief The current track or its metadata has changed
     *
     * This event occurs in several cases:
     * - When SpPlayUri is called
     * - When SpPlayPreset is called
     * - When SpPlaybackSkipToNext is called
     * - When SpPlaybackSkipToPrev is called
     * - Once any of the above actions were issued over connect
     * - Once a track has progressed naturally
     */

    /**
     *
     * It should not happen:
     * - on pause and resume
     * - on seek
     * - in-track bitrate changes for any reason
     *
     * If your application displays metadata of the current track, use SpGetMetadata() to
     * reload the metadata when you receive this event.
     *
     * Applications that allow free users to login can check if the current track
     * is an advertisement using SpPlaybackIsAdPlaying(). This information can be
     * used to change the display or available controls for advertisements.
     *
     * \see SpPlaybackIsAdPlaying \ref observing
     *
     * Note : kSpPlaybackNotifyTrackChanged is only sent if the current
     * track was changed. kSpPlaybackNotifyMetadataChanged is a more general
     * event that can be used to detect all kinds of UI changes. If you
     * display information about the previous and next tracks you should
     * use kSpPlaybackNotifyMetadataChanged to detect updates to those
     * as well.  For example, if the upcoming track is changed then
     * kSpPlaybackNotifyTrackChanged will not be sent but
     * kSpPlaybackNotifyMetadataChanged will be.
     *
     */
    kSpPlaybackNotifyTrackChanged = 2,

    /**
     * \brief Playback has skipped to the next track
     *
     * This event occurs when SpPlaybackSkipToNext() was invoked or when
     * the user skipped to the next track using Spotify Connect. It does not
     * occur when playback goes to a new track after the previous track has
     * reached the end.
     *
     * \see SpPlaybackSkipToNext
     * \deprecated Use kSpPlaybackNotifyMetadataChanged instead.
     */
    kSpPlaybackNotifyNext = 3,

    /**
     * \brief Playback as skipped to the previous track
     * \see SpPlaybackSkipToPrev
     * \deprecated Use kSpPlaybackNotifyMetadataChanged instead.
     */
    kSpPlaybackNotifyPrev = 4,

    /**
     * \brief "Shuffle" was switched on
     *
     * \see SpPlaybackEnableShuffle, SpPlaybackIsShuffled, \ref observing
     */
    kSpPlaybackNotifyShuffleOn = 5,

    /**
     * \brief "Shuffle" was switched off
     *
     * \see SpPlaybackEnableShuffle, SpPlaybackIsShuffled, \ref observing
     */
    kSpPlaybackNotifyShuffleOff = 6,

    /**
     * \brief "Repeat" was switched on
     *
     * \see SpPlaybackEnableRepeat, SpPlaybackIsRepeated, SpPlaybackGetRepeatMode \ref observing
     */
    kSpPlaybackNotifyRepeatOn = 7,

    /**
     * \brief "Repeat" was switched off
     *
     * \see SpPlaybackEnableRepeat, SpPlaybackIsRepeated, \ref observing
     */
    kSpPlaybackNotifyRepeatOff = 8,

    /**
     * \brief This device has become the active playback device
     *
     * This event occurs when the users moves playback to this device using
     * Spotify Connect, or when playback is moved to this device as a side-effect
     * of invoking one of the SpPlayback...() functions.
     *
     * When this event occurs, it may be a good time to initialize the audio
     * pipeline of the application.  You should not unpause when you receive this
     * event -- wait for kSpPlaybackNotifyPlay.
     *
     * \see SpPlaybackIsActiveDevice
     */
    kSpPlaybackNotifyBecameActive = 9,

    /**
     * \brief This device is no longer the active playback device
     *
     * This event occurs when the user moves playback to a different device using
     * Spotify Connect.
     *
     * When this event occurs, the application must stop producing audio
     * immediately. The application should not take any other action. Specifically,
     * the application must not invoke any of the SpPlayback...() functions
     * unless requested by some subsequent user interaction.
     *
     * \see SpPlaybackIsActiveDevice
     */
    kSpPlaybackNotifyBecameInactive = 10,

    /**
     * \brief This device has temporarily lost permission to stream audio from Spotify
     *
     * A user can only stream audio on one of her devices at any given time.
     * If playback is started on a different device, this event may occur.
     * If the other device is Spotify Connect-enabled, the event
     * kSpPlaybackNotifyBecameInactive may occur instead of or in addition to this
     * event.
     *
     * When this event occurs, the application must stop producing audio
     * immediately. The application should not take any other action. Specifically,
     * the application must not invoke any of the SpPlayback...() functions
     * unless requested by some subsequent user interaction.
     */
    kSpPlaybackNotifyLostPermission = 11,

    /**
     * \brief The application should flush its audio buffers
     *
     * This event occurs for example when seeking to a different position within
     * a track. If possible, the application should discard all samples that it
     * has received in SpCallbackPlaybackAudioData() but that it has not played yet.
     *
     * \note eSDK expects exactly 0 samples to remain in external buffers once this
     * event has been received. It is therefore necessary to block completion of
     * SpCallbackPlaybackNotify() until all buffers have been flushed. If this
     * is not the case, eSDK may calculate an incorrect playback position
     * resulting in discrepancies between eSDK's state and the Spotify app's UI.
     */
    kSpPlaybackEventAudioFlush = 12,

    /**
     * \brief The library will not send any more audio data
     *
     * This event occurs when the library reaches the end of a playback context
     * and has no more audio to deliver.  This occurs, for instance, at the end
     * of a playlist when repeat is disabled.  When the application receives this
     * event, it should finish playing out all of its buffered audio.
     */
    kSpPlaybackNotifyAudioDeliveryDone = 13,

    /**
     * \brief Playback changed to a different Spotify context
     *
     * This event occurs when playback starts or changes to a different context
     * than was playing before, such as a change in album or playlist.  This is
     * an informational event that does not require action, but may be used to
     * update the UI display, such as whether the user is playing from a preset.
     *
     * \deprecated Use kSpPlaybackNotifyMetadataChanged instead.
     */
    kSpPlaybackNotifyContextChanged = 14,

    /**
     * \brief Application accepted all samples from the current track
     *
     * This is an informative event that indicates that all samples from the
     * current track have been delivered to and accepted by the application.
     * The track has not finished yet (\see kSpPlaybackNotifyTrackChanged).
     * No action is necessary by the application, but this event may be used
     * to store track boundary information if desired.
     */
    kSpPlaybackNotifyTrackDelivered = 15,

    /**
     * \brief Metadata is changed
     *
     * This event occurs when playback starts or changes to a different context,
     * when a track switch occurs, etc. This is an informational event that does
     * not require action, but should be used to keep the UI display updated with
     * the latest metadata information.
     *
     * Applications that allow free users to login can check if the current track
     * is an advertisement using SpPlaybackIsAdPlaying(). This information can be
     * used to change the display or available controls for advertisements.
     */
    kSpPlaybackNotifyMetadataChanged = 16,

    /**
     * \brief Playback is not allowed without network
     *
     * This event occurs when there has been no connection to Spotify for a
     * period of time.
     *
     * When this event occurs, the application should stop producing audio
     * immediately. The application should not take any other action.
     * If the user takes any action to affect playback, kSpErrorNetworkRequired
     * will be returned.
     *
     * Once network connection is restored (\see kSpConnectionNotifyReconnect),
     * application can resume playback by calling SpPlaybackPlay().
     * Playback is not resumed automatically.
     */
    kSpPlaybackNotifyNetworkRequired = 17,

    /**
     * \brief Download of the current track stalled due to network outage
     *
     * This event occurs when the currently playing track has run out of
     * data to play due to network outage.
     *
     * This is an informative event that indicates that there is no more
     * data to play for the current track until network connections are
     * re-established. If network is not available within a certain amount
     * of time playback will be automatically paused, this is to prevent
     * startling effects for the user when track playback resumes unexpectedly
     * after a long time period of silence.
     *
     * Once network connection is restored (\see kSpConnectionNotifyReconnect),
     * playback should resume autmatically (unless \see kSpPlaybackNotifyPause
     * occured due to timeout).
     */
    kSpPlaybackNotifyTrackDownloadStalled = 18,

    /**
     * \brief Queued track is accepted by the playback-service
     *
     * This event occurs when a call to SpQueueUri has been accepted.
     *
     * Normally there is no need to monitor specifically for this as
     * kSpPlaybackNotifyMetadataChanged is enough to detect UI related changes.
     *
     * A distinct usecase for this notification is if one wants to queue several tracks
     * within a very short time period where waiting for this notification
     * between each call will help avoid getting rate-limited.
     */
    kSpPlaybackNotifyQueuedTrackAccepted = 19
};

/**
 * @}
 */

/**
 * \addtogroup Session
 * @{
 */

/**
 * \brief Notifications related to the connection to Spotify
 * \see SpCallbackConnectionNotify
 */
enum SpConnectionNotification {
    /**
     * \brief The user has successfully logged in to Spotify
     * \see SpConnectionLoginBlob, SpConnectionIsLoggedIn
     */
    kSpConnectionNotifyLoggedIn = 0,

    /**
     * \brief The user has been logged out of Spotify
     *
     * This can occur as a result of invoking SpConnectionLogout() or
     * because of a permanent connection error.
     * To log in again, the application must invoke SpConnectionLoginBlob().
     *
     * \note This notification is not received when a temporary connection error occurs.
     * In those cases, #kSpConnectionNotifyTemporaryError is sent.
     *
     * \see SpConnectionIsLoggedIn
     */
    kSpConnectionNotifyLoggedOut = 1,

    /**
     * \brief A temporary connection error occurred. The library will automatically retry.
     *
     * A temporary error can be due to lost network connectivity. The library will
     * try to reconnect from time to time. If the application is able to detect
     * that the network has become unavailable (e.g., when the network cable
     * is unplugged), it should call SpConnectionSetConnectivity()
     * with #kSpConnectivityNone. When the network becomes available again, it
     * should invoke SpConnectionSetConnectivity() with a value other than
     * #kSpConnectivityNone, which will cause the library to try to reconnect
     * immediately.
     *
     * In order to help debug the issue, the application should register the callback
     * SpCallbackDebugMessage(), which receives additional information
     * about the error.
     */
    kSpConnectionNotifyTemporaryError = 2,

    /**
     * \brief The connection to Spotify has been lost
     *
     * The application can use this to indicate a connectivity issue. No other action is necessary.
     * In particular, the application is not supposed to re-login the user.
     *
     * \note This notification will be sent before #kSpConnectionNotifyTemporaryError is sent.
     */
    kSpConnectionNotifyDisconnect = 3,

    /**
     * \brief The connection to Spotify has been (re-)established.
     *
     * This notification will be sent when the connection to Spotify has been re-established after
     * a prior #kSpConnectionNotifyDisconnect or the first time a connection is established to
     * Spotify.
     */
    kSpConnectionNotifyReconnect = 4,

    /**
     * \brief The connected user account type has changed (became premium, for example).
     *
     * No action is necessary, but new features may be available if used in conjunction
     * with the Spotify Web API.
     */
    kSpConnectionNotifyProductTypeChanged = 5,

    /**
     * \brief The SpZeroConfVars has changed.
     *
     * Retrieve the new values using SpZeroConfGetVars().
     */
    kSpConnectionNotifyZeroConfVarsChanged = 6,

    /**
     * \brief The eSDK is transmitting data
     *
     * This notification is sent every time the eSDK begins sending data. It can be used
     * to trigger integration-specific data transmissions to coincide with the eSDK
     * transmissions, to save energy for wireless applications.
     *
     * \note This notification will not be sent more than about once per second.
     */
    kSpConnectionNotifyTransmittingData = 7
};

/**
 * \brief Device type reported to client applications
 */
enum SpDeviceType {
    /**
     * \brief Laptop or desktop computer device
     */
    kSpDeviceTypeComputer = 1,

    /**
     * \brief Tablet PC device
     */
    kSpDeviceTypeTablet = 2,

    /**
     * \brief Smartphone device
     */
    kSpDeviceTypeSmartphone = 3,

    /**
     * \brief Speaker device
     */
    kSpDeviceTypeSpeaker = 4,

    /**
     * \brief Television device
     */
    kSpDeviceTypeTV = 5,

    /**
     * \brief Audio/Video receiver device
     */
    kSpDeviceTypeAVR = 6,

    /**
     * \brief Set-Top Box device
     */
    kSpDeviceTypeSTB = 7,

    /**
     * \brief Audio dongle device
     */
    kSpDeviceTypeAudioDongle = 8,

    /**
     * \brief Game console device
     */
    kSpDeviceTypeGameConsole = 9,

    /**
     * \brief Chromecast Video
     */
    kSpDeviceTypeCastVideo = 10,

    /**
     * \brief Chromecast Audio
     */
    kSpDeviceTypeCastAudio = 11,

    /**
     * \brief Automobile
     */
    kSpDeviceTypeAutomobile = 12,

    /**
     * \brief Smartwatch
     */

    kSpDeviceTypeSmartwatch = 13,
    /**
     * \brief Chromebook
     */
    kSpDeviceTypeChromebook = 14
};

/**
 * @}
 */

/**
 * \addtogroup Playback
 * @{
 */

/**
 * \brief Metadata track selector
 * \see SpGetMetadata
 */
enum SpMetadataTrack {
    /**
     * \brief Index of the before previous track in the track list
     */
    kSpMetadataTrackBeforePrevious = -2,

    /**
     * \brief Index of the previous track in the track list
     */
    kSpMetadataTrackPrevious = -1,

    /**
     * \brief Index of the current track in the track list
     */
    kSpMetadataTrackCurrent = 0,

    /**
     * \brief Index of the next track in the track list
     */
    kSpMetadataTrackNext = 1,

    /**
     * \brief Index of the after next track in the track list
     */
    kSpMetadataTrackAfterNext = 2
};

/**
 * @}
 */

/**
 * \addtogroup Session
 * @{
 */

/**
 * \brief Type of network connection
 * \see SpConnectionSetConnectivity
 */
enum SpConnectivity {
    /**
     * \brief The device is not connected to the network
     */
    kSpConnectivityNone = 0,

    /**
     * \brief The device is connected to a wired network
     */
    kSpConnectivityWired = 1,

    /**
     * \brief The device is connected to a wireless network
     */
    kSpConnectivityWireless = 2,

    /**
     * \brief The device uses a mobile data connection
     */
    kSpConnectivityMobile = 3
};

/**
 * @}
 */

/**
 * \addtogroup Playback
 * @{
 */

/**
 * \brief The track is already paused
 */
#define SP_PLAYBACK_RESTRICTION_ALREADY_PAUSED 1
/**
 * \brief The track is already playing
 */
#define SP_PLAYBACK_RESTRICTION_NOT_PAUSED 2
/**
 * \brief Licensing rules disallow this action
 */
#define SP_PLAYBACK_RESTRICTION_LICENSE_DISALLOW 4
/**
 * \brief Action can't be performed while an ad is playing
 */
#define SP_PLAYBACK_RESTRICTION_AD_DISALLOW 8
/**
 * \brief There is no track before the current one in
 * the currently playing context.
 */
#define SP_PLAYBACK_RESTRICTION_NO_PREV_TRACK 16
/**
 * \brief There is no track after the current one in
 * the currently playing context.
 */
#define SP_PLAYBACK_RESTRICTION_NO_NEXT_TRACK 32
/**
 * \brief The action is restricted, but no reason is provided
 * This means that eSDK has not retrieved the restrictions from
 * the backend yet and therefore the action is not allowed right now.
 * As soon as eSDK retrieves the information, the notification
 * kSpPlaybackNotifyMetadataChanged will be sent, and the application
 * can check the field again.
 */
#define SP_PLAYBACK_RESTRICTION_UNKNOWN 64
/**
 * \brief The action is restricted for context level reasons
 */
#define SP_PLAYBACK_RESTRICTION_ENDLESS_CONTEXT 128

/**
 * \brief Playback restrictions
 */
struct SpPlaybackRestrictions {
    /**
     * \brief Bitfield of reasons the pause action is unavailable
     */
    uint32_t disallow_pausing_reasons;
    /**
     * \brief Bitfield of reasons the resume action is unavailable
     */
    uint32_t disallow_resuming_reasons;
    /**
     * \brief Bitfield of reasons seeking is unavailable
     */
    uint32_t disallow_seeking_reasons;
    /**
     * \brief Bitfield of reasons peeking on the previous track is unavailable
     */
    uint32_t disallow_peeking_prev_reasons;
    /**
     * \brief Bitfield of reasons peeking on the next track is unavailable
     */
    uint32_t disallow_peeking_next_reasons;
    /**
     * \brief Bitfield of reasons skipping to the previous track is unavailable
     */
    uint32_t disallow_skipping_prev_reasons;
    /**
     * \brief Bitfield of reasons skipping to the next track is unavailable
     */
    uint32_t disallow_skipping_next_reasons;
    /**
     * \brief Bitfield of reasons setting repeat context is not allowed
     */
    uint32_t disallow_toggling_repeat_context_reasons;
    /**
     * \brief Bitfield of reasons setting repeat track is not allowed
     */
    uint32_t disallow_toggling_repeat_track_reasons;
    /**
     * \brief Bitfield of reasons toggling shuffle is not allowed
     */
    uint32_t disallow_toggling_shuffle_reasons;
};

/**
 * \brief Track metadata
 * \see SpGetMetadata
 */
struct SpMetadata {
    /**
     * \brief Display name of the playback source.
     * E.g., the name of the playlist from which playback
     * was initiated (UTF-8-encoded)
     */
    char playback_source[SP_MAX_METADATA_NAME_LENGTH + 1];

    /**
     * \brief Spotify URI of the playback source (in the form "spotify:xxxxxx:xxxxxxx...")
     *
     * \note Applications can use the Spotify web APIs
     * (https://developer.spotify.com/documentation/web-api/)
     * to get detailed information about the item.
     */
    char playback_source_uri[SP_MAX_METADATA_URI_LENGTH + 1];

    /**
     * \brief Display name of the track (UTF-8-encoded)
     */
    char track[SP_MAX_METADATA_NAME_LENGTH + 1];

    /**
     * \brief Spotify URI of the track (in the form "spotify:track:xxxxxxx...")
     *
     * \note Applications can use the Spotify web APIs
     * (https://developer.spotify.com/documentation/web-api/)
     * to get detailed information about the item.
     */
    char track_uri[SP_MAX_METADATA_URI_LENGTH + 1];

    /**
     * \brief Display name of the artist of the track (UTF-8-encoded)
     */
    char artist[SP_MAX_METADATA_NAME_LENGTH + 1];

    /**
     * \brief Spotify URI of the artist of the track (in the form
     *        "spotify:artist:xxxxxxx...")
     *
     * \note Applications can use the Spotify web APIs
     * (https://developer.spotify.com/documentation/web-api/)
     * to get detailed information about the item.
     */
    char artist_uri[SP_MAX_METADATA_URI_LENGTH + 1];

    /**
     * \brief Display name of the track's album (UTF-8-encoded)
     */
    char album[SP_MAX_METADATA_NAME_LENGTH + 1];

    /**
     * \brief Spotify URI of the track's album (in the form "spotify:album:xxxxxxx...")
     *
     * \note Applications can use the Spotify web APIs
     * (https://developer.spotify.com/documentation/web-api/)
     * to get detailed information about the item.
     */
    char album_uri[SP_MAX_METADATA_URI_LENGTH + 1];

    /**
     * \brief Spotify URI of the album's cover art image (in the form "spotify:image:xxxxxxx...")
     *
     * Use the function SpGetMetadataImageURL() to convert this to an HTTP URL
     * to the actual image file.
     */
    char album_cover_uri[SP_MAX_METADATA_URI_LENGTH + 1];

    /**
     * \brief Spotify URI of the original track before relinking (in the form
     * "spotify:track:xxxxxxx...")
     *
     * If the track was relinked this field would contain Spotify URI of the original track
     * before relinking. If track was not relinked this would be equal to the track_uri.
     * This field is helpful to identify whether the track is already in the user collection.
     *
     * \note Typical use case would be: user adds track to the collecion, later on platform
     * integration
     * code wants to check if the current track is in the user collection. Due to track relinking
     * the \ref track_uri might differ from the one that is saved in the collection previously. In
     * order
     * to mitigate this issue integration could should use original_track_uri for that purpose.
     * For more info about track relinking refer to track relinking guide on developer.spotify.com
     *
     * \note Applications can use the Spotify web APIs
     * (https://developer.spotify.com/documentation/web-api/)
     * to get detailed information about the item.
     */
    char original_track_uri[SP_MAX_METADATA_URI_LENGTH + 1];

    /**
     * \brief Playback duration of the track in milliseconds
     */
    uint32_t duration_ms;

    /**
     * \brief Index of the track in the currently playing context
     *
     * This is the index of the track in the currently playing context (eg. playlist, artist,
     * radio, etc).  Since the contents of the underlying context can change, the index
     * is correct from the time when the track is loaded, but may occasionally be out of
     * date if the context is actively changing.
     *
     * \note Index is less than zero if the track is from a different context.
     */
    int32_t index;

    /**
     * \brief Track UID of the track in the currently playing context.
     *
     * \see SpPlayContextUri
     */
    char track_uid[SP_MAX_TRACK_UID_LENGTH + 1];

    /**
     * \brief Index of the track in the original (unchanged) playing context
     *
     * It's not affected by shuffle logic.
     */
    uint32_t original_index;

    /**
     * \brief The bitrate of the track in kbps. 0 means "unplayable".
     */
    uint32_t bitrate;

    /**
     * \brief Restrictions that apply to playback and transitions related to this track
     *
     * Restriction reasons are provided as bitfields inside this struct. Each member of this
     * struct represents one individual action that might be restricted, and it's value is
     * a combination of the reasons that are currently in effect. A value of zero means that
     * the action is not currently restricted in any way.
     *
     * \note Example usage:
     * SpMetadata metadata;
     * SpGetMetadata(&metadata, 0);
     * if (metadata.playback_restrictions.disallow_skipping_next_reasons != 0) {
     *   // General case for skipping to next track not being allowed
     *   …
     * }
     * If (metadata.playback_restrictions.disallow_skipping_next_reasons &
     *     SP_PLAYBACK_RESTRICTION_LICENSE_DISALLOW != 0) {
     *   // Specific case of not allowed to skip due to licensing rules
     *   …
     * }
     *
     * \note This functionality is supported only from version 3.0 of the Spotify Embedded SDK
     * and higher. Older versions will never report restriction reasons for any action.
     */
    struct SpPlaybackRestrictions playback_restrictions;

    /**
     * \brief Playback-id of this playback of this specific track
     *
     * Playback-id uniquely identifies a single playback of a track. If same
     * track exists twice in a context, each instance will have a unique
     * playback-id. It is valid for only one playback of a track and once the
     * track is played or skipped-over, the playback-id is no longer valid e.g
     * if you skip a track and then come back to it using skip-prev, the playback-id
     * changes.
     */
    char playback_id[SP_MAX_PLAYBACK_ID_LENGTH + 1];
};

/**
 * @}
 */

/**
 * \addtogroup ZeroConf
 * @{
 */

/**
 * \brief ZeroConf DeviceAlias
 *
 * This structure contains information about a single device alias,
 * as returned by SpZeroConfGetVars.
 *
 * \see SpZeroConfGetVars
 */
struct SpZeroConfDeviceAlias {
    /**
     * \brief String to be sent in the "id" field of the alias in the "getInfo" response.
     */
    uint32_t id;

    /**
     * \brief Boolean (0 = "false", 1 = "true") to be sent in the "name" field of the
     * alias in the "getInfo" response.
     */
    int is_group;

    /**
     * \brief String to be sent in the "name" field of the alias in the "getInfo" response.
     */
    char display_name[SP_MAX_DISPLAY_NAME_LENGTH + 1];
};

/**
 * \brief ZeroConf variables
 *
 * This structure contains the fields that the application needs for
 * ZeroConf, mainly what to send in the response to the "getInfo" request.
 * See the \ref ZeroConf manual for more information.
 *
 * \warning [*] The application is responsible for properly escaping special
 * characters in the strings contained in this structure before sending
 * them as part of the "getInfo" JSON response. See http://www.json.org/
 * for the requirements.
 *
 * \see SpZeroConfGetVars
 */
struct SpZeroConfVars {
    /**
     * \brief String to be sent in the "publicKey" field of the "getInfo" response[*].
     */
    char public_key[SP_MAX_PUBLIC_KEY_LENGTH + 1];

    /**
     * \brief String to be sent in the "deviceID" field of the "getInfo" response[*].
     */
    char device_id[SP_MAX_DEVICE_ID_LENGTH + 1];

    /**
     * \brief String to be sent in the "remoteName" field of the "getInfo" response[*].
     */
    char remote_name[SP_MAX_DISPLAY_NAME_LENGTH + 1];

    /**
     * \brief String to be sent in the "deviceType" field of the "getInfo" response[*].
     */
    char device_type[SP_MAX_DEVICE_TYPE_LENGTH + 1];

    /**
     * \brief String to be sent in the "libraryVersion" field of the "getInfo" response[*].
     */
    char library_version[SP_MAX_VERSION_LENGTH + 1];

    /**
     * \brief Integer to be sent as string in the "resolverVersion" field of the "getInfo"
     * response[*].
     */
    int resolver_version;

    /**
     * \brief String to be sent in the "groupStatus" field of the "getInfo" response[*].
     */
    char group_status[SP_MAX_GROUP_STATUS_LENGTH + 1];

    /**
     * \brief Current internal ZeroConf webserver port number. To be used when running an
     *        external mDNS server together with an internal webserver.
     */
    int webserver_current_port;

    /**
     * \brief String to be sent in the "tokenType" field of the "getInfo" response[*].
     */
    char token_type[SP_MAX_TOKEN_TYPE_LENGTH + 1];

    /**
     * \brief String to be sent in the "clientID" field of the "getInfo" response[*].
     */
    char client_id[SP_MAX_CLIENT_ID_LENGTH + 1];

    /**
     * \brief String to be sent in the "scope" field of the "getInfo" response[*].
     */
    char scope[SP_MAX_SCOPE_LENGTH + 1];

    /**
     * \brief String to be sent in the "availability" field of the "getInfo" response[*].
     */
    char availability[SP_MAX_AVAILABILITY_LENGTH + 1];

    /**
     * \brief Integer to be sent in the "productID" field of the "getInfo" response[*].
     */
    uint32_t product_id;

    /**
     * \brief Array of SpZeroConfDeviceAlias to be sent in the "aliases" field of the "getInfo"
     * response[*].
     */
    struct SpZeroConfDeviceAlias aliases[SP_MAX_DEVICE_ALIASES];

    /**
     * \brief Integer to be sent in the "alias_count" field of the "getInfo" response[*].
     */
    uint32_t alias_count;
};

/**
 * @}
 */

/**
 * \addtogroup Playback
 * @{
 */

/**
 * \brief Sample format of the audio data sent in SpCallbackPlaybackAudioData()
 *
 * \note The contents of this should be ignored when SpCallbackPlaybackAudioData()
 * is invoked with \a sample_count = 0.
 */
struct SpSampleFormat {
    /** \brief Number of channels (1 = mono, 2 = stereo) */
    int channels;

    /** \brief Sample rate in Hz (such as 22050, 44100 or 48000) */
    int sample_rate;

    /** \brief If audio playback is normalized, this value is the suggested replay gain
     * adjustment in milli-dBs (1000 = +1 dB, -1000 = -1 dB, 0 = no change)
     *
     * Convert to gain factor as 10^(replay_gain_mdb/20000)
     *
     * \note To be able to apply the gain, peak limiting might be needed.
     */
    int replay_gain_mdb;
};

/**
 * \brief Redelivery mode types
 */
typedef enum {
    /**
     * \brief Redelivery is activated
     */
    kSpRedeliveryModeActivated = 1,

    /**
     * \brief Redelivery is deactivated
     */
    kSpRedeliveryModeDeactivated = 0
} SpReDeliveryMode;

/**
 * @}
 */

/**
 * \addtogroup Errors
 * @{
 */

/**
 * \brief Callback for reporting errors to the application
 *
 * To register this callback, set the field SpConfig::error_callback when invoking
 * the function SpInit().
 *
 * \param[in] error Error code
 * \param[in] context Context pointer that was passed when registering the callback
 *
 * \note The application should not block or call API functions that are not allowed
 * in the callback.
 */
typedef void (*SpCallbackError)(SpError error, void *context);

/**
 * @}
 */

/**
 * \addtogroup Playback
 * @{
 */

/**
 * \brief Callback for notifying the application about playback-related events
 *
 * To register this callback, use the function SpRegisterPlaybackCallbacks().
 *
 * \param[in] event Type of event
 * \param[in] context Context pointer that was passed when registering the callback
 *
 * \note The application should not block or call API functions that are not allowed
 * in the callback.
 */
typedef void (*SpCallbackPlaybackNotify)(enum SpPlaybackNotification event, void *context);

/**
 * \brief Callback for sending audio data to the application
 *
 * To register this callback, use the function SpRegisterPlaybackCallbacks().
 *
 * \param[in] samples Pointer to 16-bit PCM data. The buffer contains
 *              \a sample_count samples, whereby each sample contains the data
 *              for a single audio channel.
 * \param[in] sample_count Number of samples in the \a samples buffer. This is
 *              always a multiple of \a sample_format->channels. This may be 0
 *              (see notes).
 * \param[in] sample_format Information about the format of the audio data.
 *              See the note below.
 * \param[out] samples_buffered The number of samples that the application
 *              has received but that have not been played yet. See the note below.
 * \param[in] context Context pointer that was passed when registering the callback
 * \return The number of samples that the application accepted.
 *
 * If the device is active (SpPlaybackIsActiveDevice() returns true) and playback
 * is not paused (SpPlaybackIsPlaying() returns true), the application
 * should play back the audio data.
 *
 * If the device is not active or playback is paused, or if audio data arrives
 * faster than it is played, the application can place the audio data into a buffer,
 * and return the total amount of buffered data in \a samples_buffered. See also
 * the notes below.
 *
 * If the application wants to reject the audio data (e.g., if its buffers are full),
 * it should return 0. The audio data will be re-delivered by the eSDK until the
 * application is ready to accept it.
 *
 * \note \a sample_format can change at any time as playback goes from one track
 * to the next. The application should check the format during every invocation
 * of the callback and reinitialize the audio pipeline if necessary. The sample
 * format can change before the application receives a notification about the
 * track change (such as #kSpPlaybackNotifyTrackChanged). If \a sample_count is 0,
 * the contents of \a sample_format should be ignored.
 * \par
 *
 * \note It is important to return an accurate value in \a samples_buffered.
 * The library uses this value to calculate an accurate playback position within
 * the track. For example, if the library has delivered 1.5 seconds of audio data,
 * but the application is buffering half a second of audio data, the actual
 * playback position is 1 second.
 *  - By default, when \a samples_buffered is 0, the library will calculate the
 *    playback position (SpPlaybackGetPosition()) and the notifications that are
 *    sent when playback reaches the end of a track (#kSpPlaybackNotifyTrackChanged)
 *    based on the amount of audio data that was delivered in SpCallbackPlaybackAudioData().
 *  - However, audio data is delivered faster than the playback happens (1.5 times
 *    playback speed). This means, as long as the application accepts samples in
 *    SpCallbackPlaybackAudioData(), these samples will be calculated as "consumed" and
 *    the playback position will move ahead. Eventually, the last sample for a track
 *    will be delivered in SpCallbackPlaybackAudioData(). This is when the library
 *    sends the event #kSpPlaybackNotifyTrackChanged and SpGetMetadata() starts
 *    returning metadata for the next track.
 *  - To adjust playback position and notifications for the amount of data that the
 *    application has buffered but that have not been played yet, the application
 *    should set \a samples_buffered to the amount of samples in all buffers. This
 *    will make sure that SpPlaybackGetPosition() is adjusted, the notifications
 *    will be sent at the correct time, and SpGetMetadata() returns the data
 *    for the correct track.
 *  - eSDK will deliver samples for the next track before the current track has
 *    finished playing to enable gapless playback. The application should
 *    continually accept as many samples as it can buffer and report the
 *    total number in samples_buffered.
 *  - If the application has buffered data, SpCallbackPlaybackAudioData() may
 *    be invoked with \a samples_count = 0 after all audio data has been delivered
 *    by the library. In this case, the application should make sure to update
 *    \a samples_buffered (since the application's buffers are draining) and to
 *    return 0 from the callback.
 * \par
 *
 * \note Most tracks are stereo, 44100 Hz, so it is a good idea to initialize
 * the audio pipeline to this format when the application starts.
 * \par
 *
 * \note The application should not block or call API functions that are not allowed
 * in the callback.
 */
typedef size_t (*SpCallbackPlaybackAudioData)(const int16_t *samples, size_t sample_count,
                                              const struct SpSampleFormat *sample_format,
                                              uint32_t *samples_buffered, void *context);

/**
 * \brief Callback to notify the application of a change in the playback position
 *
 * To register this callback, use the function SpRegisterPlaybackCallbacks().
 *
 * This callback is invoked when SpPlaybackSeek() is invoked or when the user
 * seeks to a position within the track using Spotify Connect.
 *
 * \param[in] position_ms New position within the track in milliseconds
 * \param[in] context Context pointer that was passed when registering the callback
 *
 * \note The application should not block or call API functions that are not allowed
 * in the callback.
 *
 * \see \ref observing
 */
typedef void (*SpCallbackPlaybackSeek)(uint32_t position_ms, void *context);

/**
 * \brief Callback to notify the application of a volume change using Spotify Connect
 *
 * To register this callback, use the function SpRegisterPlaybackCallbacks().
 *
 * This callback is invoked in two cases:
 *  - When the user changes the playback volume using Spotify Connect.
 *  - When the application invoked SpPlaybackUpdateVolume().
 *
 * In both cases, the application is responsible for applying the new volume to
 * its audio output. The application should never invoke SpPlaybackUpdateVolume()
 * from this callback, as this might result in an endless loop.
 *
 * \param[in] volume Volume in the range 0 (silence) to 65535 (max volume)
 * \param[in] remote Set to 1 if the volume was changed using Spotify Connect, 0 otherwise
 * \param[in] context Context pointer that was passed when registering the callback
 *
 * \note The application should not block or call API functions that are not allowed
 * in the callback.
 */
typedef void (*SpCallbackPlaybackApplyVolume)(uint16_t volume, uint8_t remote, void *context);

/**
 * \brief Callback for receiving preset tokens
 *
 * To register this callback, use the function SpRegisterPlaybackCallbacks().
 *
 * After subscribing to receive preset tokens for the currently playing context,
 * this callback will be called periodically with an updated token that can be
 * used to restore playback at a later time with SpPlayPreset().  This token
 * must be saved to persistent storage, and must replace any previous token for
 * the same preset.
 *
 * The rate at which this callback is called may vary based on the type of
 * context playing.  It will typically be called once per playing track, but
 * may be called more or less often to satisfy the requirements of the expected
 * Spotify user experience.
 *
 * \param[in] preset_id The value specified to SpPresetSubscribe()
 * \param[in] playback_position Current playback time in milliseconds.  This
 *              must be saved persistently with the preset token, and specified
 *              when calling SpPlayPreset().
 * \param[in] buffer The buffer provided to SpPresetSubscribe(), filled with
 *              the binary preset token to save.  This may be NULL if an error
 *              has occurred.
 * \param[in] buff_size The number of binary bytes in \a buffer to save.
 * \param[in] error An error code if a problem occurred, or kSpErrorOk.
 * \param[in] context The context provided when the callback was registered.
 * \return 1 if token was saved, 0 if it could not be saved
 *
 * \note The application should not block or call API functions that are not allowed
 * in the callback.
 *
 * \see SpPresetSubscribe, SpPresetUnsubscribe, SpPresetPlay
 * callback.
 */
typedef uint8_t (*SpCallbackSavePreset)(int preset_id, uint32_t playback_position,
                                        const uint8_t *buffer, size_t buff_size, SpError error,
                                        void *context);

/**
 * \brief Callbacks to be registered with SpRegisterPlaybackCallbacks()
 *
 * Any of the pointers may be NULL.
 *
 * See the documentation of the callback typedefs for information about the
 * individual callbacks.
 */
struct SpPlaybackCallbacks {
    /** \brief Notification callback */
    SpCallbackPlaybackNotify on_notify;
    /** \brief Audio data callback */
    SpCallbackPlaybackAudioData on_audio_data;
    /** \brief Seek position callback */
    SpCallbackPlaybackSeek on_seek;
    /** \brief Apply volume callback */
    SpCallbackPlaybackApplyVolume on_apply_volume;
    /** \brief Preset token callback */
    SpCallbackSavePreset on_save_preset;
};

/**
 * @}
 */

/**
 * \addtogroup Session
 * @{
 */

/**
 * \brief Callback for notifying the application about events related
 *        to the connection to Spotify
 *
 * To register this callback, use the function SpRegisterConnectionCallbacks().
 *
 * \param[in] event Type of event
 * \param[in] context Context pointer that was passed when registering the callback
 *
 * \note The application should not block or call API functions that are not allowed
 * in the callback.
 */
typedef void (*SpCallbackConnectionNotify)(enum SpConnectionNotification event, void *context);

/**
 * \brief Callback for passing a login blob to the application
 *
 * To register this callback, use the function SpRegisterConnectionCallbacks().
 *
 * The application may save the \a credentials_blob for subsequent logins
 * using the function SpConnectionLoginBlob(). The application should also discard
 * any credentials blobs for this user that it received previously, either
 * through this callback or through ZeroConf (see the \ref ZeroConf manual).
 *
 * Note: If credentials_blob is an empty string, the application MUST delete
 * any existing saved credentials for the account, and must not attempt to
 * login again with the empty credentials.  This happens when a permanent
 * logout is requested.
 *
 * \param[in] credentials_blob Credentials to be passed to SpConnectionLoginBlob()
 * \param[in] username user name to be passed to SpConnectionLoginBlob()
 * \param[in] context Context pointer that was passed when registering the callback
 *
 * \note The application should not block or call API functions that are not allowed
 * in the callback.
 *
 * \see SpConnectionLoginBlob
 */
typedef void (*SpCallbackConnectionNewCredentials)(const char *credentials_blob,
                                                   const char *username, void *context);

/**
 * \brief Callback for sending a message to the user
 *
 * To register this callback, use the function SpRegisterConnectionCallbacks().
 *
 * This callback is invoked when Spotify wants to display a message to the user.
 * The message is meant to be displayed to the user as is and should not be interpreted
 * by the application (the format of the messages may change without notice).
 * If the application does not have a graphical user interface, it can safely
 * ignore this callback.
 *
 * \param[in] message Message to be displayed to the user.
 * \param[in] context Context pointer that was passed when registering the callback
 *
 * \note The application should not block or call API functions that are not allowed
 * in the callback.
 */
typedef void (*SpCallbackConnectionMessage)(const char *message, void *context);

/**
 * \brief Callback for sending debug messages/trace logs
 *
 * To register this callback, use the function SpRegisterDebugCallbacks().
 *
 * In special builds of the library, this callback receives debug messages
 * that the application may write to its logs. The application should not
 * interpret the messages (the format of the messages may change without notice).
 *
 * \param[in] debug_message Message to be logged
 * \param[in] context Context pointer that was passed when registering the callback
 *
 * \note The application should not block or call API functions that are not allowed
 * in the callback.
 */
typedef void (*SpCallbackDebugMessage)(const char *debug_message, void *context);

/**
 * \brief Callbacks to be registered with SpRegisterDebugCallbacks()
 *
 * Any of the pointers may be NULL.
 *
 * See the documentation of the callback typedefs for information about the
 * individual callbacks.
 */
struct SpDebugCallbacks {
    /** \brief Debug message callback */
    SpCallbackDebugMessage on_message;
};

/**
 * \brief Callbacks to be registered with SpRegisterConnectionCallbacks()
 *
 * Any of the pointers may be NULL.
 *
 * See the documentation of the callback typedefs for information about the
 * individual callbacks.
 */
struct SpConnectionCallbacks {
    /** \brief Notification callback */
    SpCallbackConnectionNotify on_notify;
    /** \brief Credentials blob callback */
    SpCallbackConnectionNewCredentials on_new_credentials;
    /** \brief Connection message callback */
    SpCallbackConnectionMessage on_message;
};

/**
 * @}
 */

/**
 * \addtogroup Session
 * @{
 */

/**
 * \brief Device alias definition
 *
 * This struct is used to define (optional) device aliases. It's a part of the SpConfig struct
 * which will be passed to \ref SpInit to initialize the eSDK.
 */
struct SpDeviceAlias {
    /**
     * \brief A UTF-8 encoded display name for an alias of the application/device.
     *
     * The string will be truncated to at most SP_MAX_DISPLAY_NAME_LENGTH bytes
     * (depending on the UTF-8-encoded characters), not counting the terminating NULL.
     */
    const char *display_name;

    /**
     * \brief Attributes for this device alias.
     *
     * Attributes is an unsigned integer interpreted as a bitfield.
     * Different attributes ar OR:ed together and stored in the integer.
     *
     * Example:
     * \code{.c}
     *   SpDeviceAlias alias = {0}; // or memset()
     *   aliases.display_name = "My group";
     *   aliases.attributes = SP_DEVICE_ALIAS_ATTRIBUTE_GROUP |
     * SP_DEVICE_ALIAS_SOME_OTHER_ATTRIBUTE;
     * \endcode
     */
    uint32_t attributes;
};

/**
 * \brief Configuration
 * \see SpInit and \ref init
 */
struct SpConfig {
    /**
     * \brief The version of the API contained in this header file. Must be
     *        set to SP_API_VERSION.
     */
    int api_version;

    /**
     * \brief Pointer to a memory block to be used by the library
     *
     * The block is suggested to be at least SP_RECOMMENDED_MEMORY_BLOCK_SIZE
     * bytes in size.  Smaller values are accepted, but may lead to degraded
     * performance, or playback failure.
     */
    void *memory_block;

    /**
     * \brief Size of the \a memory_block buffer in bytes
     */
    uint32_t memory_block_size;

    /**
     * \brief A NULL-terminated character string that uniquely identifies the
     *        device (such as a MAC address)
     *
     * The string will be truncated to SP_MAX_UNIQUE_ID_LENGTH characters,
     * not counting the terminating NULL.
     *
     * The library may use this to distinguish this device from other Spotify
     * Connect-enabled devices that the users has. On any given device, the
     * ID should not change between calls to SpInit().
     *
     * \warning If the unique ID collides with other devices that the user has,
     * the device might not be usable with Spotify Connect. Therefore, it is
     * important to minimize the chance of such collisions while still making
     * sure that the unique ID does not change between sessions. (A MAC address
     * or the device's serial number should work well. The device's model name
     * or its IP address will not work.)
     *
     * \warning If the unique ID changes, any credentials blob received previously
     * through SpCallbackConnectionNewCredentials() is no longer valid. You will
     * receive an error when invoking SpConnectionLoginBlob().
     *
     * SpInit() returns kSpErrorInvalidArgument if this is not specified.
     */
    const char *unique_id;

    /**
     * \brief A UTF-8-encoded display name for the application/device
     *
     * When using Spotify Connect, this is the name that the Spotify app
     * will use in the UI to refer to this instance of the application/this device.
     *
     * The string will be truncated to at most SP_MAX_DISPLAY_NAME_LENGTH bytes
     * (depending on the UTF-8-encoded characters), not counting the terminating NULL.
     *
     * The display name can be changed later with SpSetDisplayName().
     *
     * \note If device aliases are used, display_name must be set to NULL. If device
     * aliases are not used, display_name must be defined and not empty.
     *
     * SpInit() returns kSpErrorInvalidArgument if this is not specified.
     */
    const char *display_name;

    /**
     * \brief The global attributes is a bitfield where each attribute is OR:ed together
     * and stored in this integer.
     *
     * These attributes must be valid for each device alias.
     *
     * Example:
         * \code{.c}
     *   conf.global_attributes = SP_GLOBAL_ATTRIBUTE_VOICE | SP_GLOBAL_ATTRIBUTE_SOME_OTHER;
     * \endcode
         */
    uint32_t global_attributes;

    /**
     * \brief Device alias definitions. These are optional, if you don't want to
     *        define aliases this array must be zeroed.
     * \see SpDeviceAlias
     *
     * Device aliases can make your device appear as several devices in Spotify apps.
     * This can be useful when a multi-room solution wants to present several
     * configurations simultaneously.
     *
     * A user could switch between these aliases by selecting a different alias
     * for playback through the Spotify apps (or by other means where applicable
     * like through voice control), and let an integration perform different
     * audio routing depending on which alias was selected, without disrupting
     * the current session or any ongoing streaming.
     *
     * The array has SP_MAX_DEVICE_ALIASES slots. Any slot which contains
     * NULL as the alias name is ignored.
     *
     * \note If device aliases are used, SpConfig::display_name should not be set.
     */
    struct SpDeviceAlias device_aliases[SP_MAX_DEVICE_ALIASES];

    /**
     * \brief A NULL-terminated string containing the brand name of the hardware
     *        device (for hardware integrations)
     *
     * This should be an ASCII string containing only letters, digits,
     * "_" (underscore), "-" (hyphen) and "." (period).
     *
     * SpInit() returns kSpErrorInvalidArgument if this is not specified or if
     * the string is longer than SP_MAX_BRAND_NAME_LENGTH characters.
     */
    const char *brand_name;

    /**
     * \brief A UTF-8-encoded brand name of the hardware device
     *        (for hardware integrations). Should be very similar to brand_name.
     *
     * The string will be truncated to at most SP_MAX_DISPLAY_NAME_LENGTH bytes
     * (depending on the UTF-8-encoded characters), not counting the terminating NULL.
     *
     */
    const char *brand_display_name;

    /**
     * \brief A NULL-terminated string containing the model name of the hardware
     *        device (for hardware integrations)
     *
     * This should be an ASCII string containing only letters, digits,
     * "_" (underscore), "-" (hyphen) and "." (period).
     *
     * SpInit() returns kSpErrorInvalidArgument if this is not specified or if
     * the string is longer than SP_MAX_MODEL_NAME_LENGTH characters.
     */
    const char *model_name;

    /**
     * \brief A UTF-8-encoded model name of the hardware device
     *        (for hardware integrations)
     *
     * The string will be truncated to at most SP_MAX_DISPLAY_NAME_LENGTH bytes
     * (depending on the UTF-8-encoded characters), not counting the terminating NULL.
     *
     */
    const char *model_display_name;

    /**
     * \brief A NULL-terminated string containing the client id of the application
     *
     * The Client ID identifies the application using Spotify, Register your
     * application <a href="https://developer.spotify.com/dashboard/applications">here</a>.
     *
     * This can be an ASCII string containing only hexadecimal characters, or NULL
     *
     * SpInit() returns kSpErrorInvalidArgument if this is invalid or longer than
     * SP_MAX_CLIENT_ID_LENGTH characters.
     */
    const char *client_id;

    /**
     * \brief An integer enumerating the product for this partner
     *
     * The (Client ID, Product ID) pair will uniquely identify this product
     * in the Spotify backend. That could determine things like license,
     * icons, and other behaviour as needed.
     */
    uint32_t product_id;

    /**
     * \brief A NULL-terminated string containing the OAuth scope requested when
     *        authenticating with the Spotify backend.
     *
     * This can be a comma-separated string of Spotify scopes, or NULL (which
     * would mean the default SP_SCOPE_STREAMING = "streaming")
     *
     * SpInit() returns kSpErrorInvalidArgument if this is invalid or longer than
     * SP_MAX_SCOPE_LENGTH characters.
     */
    const char *scope;

    /**
     * \brief A NULL-terminated string containing the os version running on the
     * hardware
     *
     * This should be an ASCII string containing only printable characters or NULL.
     *
     * SpInit() returns kSpErrorInvalidArgument if this is invalid or longer than
     * SP_MAX_OS_VERSION_LENGTH characters.
     */
    const char *os_version;

    /**
     * \brief The device type that best describes this product
     *
     * This device type will be reported to client applications and might
     * result in a suitable icon being shown, etc.
     *
     * SpInit() returns kSpErrorInvalidArgument if this is invalid
     */
    enum SpDeviceType device_type;

    /**
     * \brief The maximum bitrate to use for playback
     *
     * Leave as default to use the maximum available bitrate (high).
     * The bitrate may drop automatically to compensate for low network speeds.
     * Use SpSetPlaybackBitrate to change the bitrate.
     */
    enum SpPlaybackBitrate max_bitrate;

    /**
     * \brief Pointer to a callback function that will receive error notifications
     */
    SpCallbackError error_callback;

    /**
     * \brief Application-defined pointer that will be passed unchanged as
     *        the \a context argument to the \a error_callback callback.
     */
    void *error_callback_context;

    /**
     * \if internal_zeroconf
     * \brief Control builtin ZeroConf stack. Set this field to
     *        SP_ZEROCONF_DISABLED, SP_ZEROCONF_SERVE,
     *        SP_ZEROCONF_SERVE_HTTP_ONLY or SP_ZEROCONF_SERVE_MDNS_ONLY.
     *
     * \note When enabling this, make sure the fields SpConfig::host_name,
     *       SpConfig::zeroconf_port and SpConfig::zeroconf_port_range are set.
     *       When zeroconf_serve is set to SP_ZEROCONF_SERVE_MDNS_ONLY,
     *       host_name and zeroconf_port must specify an external webserver,
     *       which will be announces by built-in mDNS. The external webserver
     *       has to be started before SpInit() is called. The announced path is
     *       host_name:zeroconf_port/zc
     * \else
     * \brief <em>Not applicable in this eSDK configuration.</em>
     * \endif
     */
    int zeroconf_serve;

    /**
     * \if internal_zeroconf
     * \brief Hostname used in the mDNS service discovery. Must be unique
     *        on the local network and less than 32 characters.
     *
     * \note Only needs to be set if zeroconf_serve = SP_ZEROCONF_SERVE or
     *       SP_ZEROCONF_SERVE_MDNS_ONLY.
     * \else
     * \brief <em>Not applicable in this eSDK configuration.</em>
     * \endif
     */
    const char *host_name;

    /**
     * \if internal_zeroconf
     * \brief Port to run the ZeroConf http server on. Will use a system
     *        assigned port if this is set to 0. DO NOT set this to 5353,
     *        that port is reserved for mDNS.
     * \else
     * \brief <em>Not applicable in this eSDK configuration.</em>
     * \endif
     */
    int zeroconf_port;

    /**
     * \if internal_zeroconf
     * \brief If SpConfig::zeroconf_port is set, the number of ports to try
     *        (from SpConfig::zeroconf_port to SpConfig::zeroconf_port +
     *        SpConfig::zeroconf_port_range - 1) when binding the http server to a port.
     *
     * \note If the ZeroConf http server is slow to respond after a logout, or
     *       doesn't respond at all, try to increase this value.
     * \else
     * \brief <em>Not applicable in this eSDK configuration.</em>
     * \endif
     */
    int zeroconf_port_range;
};

/**
 * \brief Initialize the library
 *
 * \param[in] conf Configuration parameters
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpInit(struct SpConfig *conf);

/**
 * \brief Shut down the library
 *
 * If a user is currently logged in, the application should first call
 * SpConnectionLogout() and wait for the #kSpConnectionNotifyLoggedOut event,
 * otherwise SpFree() may take several seconds.
 *
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpFree(void);

/**
 * \brief Retrieve a version string for the library
 *
 * \return Version string
 *
 * \note This API can be invoked from a callback.
 */
SP_EMB_PUBLIC const char *SpGetLibraryVersion(void);

/**
 * \brief Set the type of network connection of the device
 *
 * When the application detects that the device has lost network connection,
 * it should call this function with #kSpConnectivityNone.
 * When network connection is restored, the application should call this
 * function with one of the other values of SpConnectivity. The library will
 * then immediately retry to reconnect to Spotify (rather than waiting for the
 * next retry timeout).
 *
 * The library may use the type of network connection to adapt its streaming
 * and buffering strategies. Currently, however, all types of network connection
 * are treated the same.
 *
 * \param[in] connectivity Type of connection
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpConnectionSetConnectivity(enum SpConnectivity connectivity);

/**
 * \brief Get the connectivity that was set with SpConnectionSetConnectivity()
 *
 * The library does not detect the type of network connection by itself.
 * It only updates it if the application calls SpConnectionSetConnectivity().
 * If SpConnectionSetConnectivity() was never called, the connection defaults
 * to kSpConnectivityWired.
 *
 * \return Type of connection
 *
 * \note This API can be invoked from a callback.
 */
SP_EMB_PUBLIC enum SpConnectivity SpConnectionGetConnectivity(void);

/**
 * \brief Log in a user to Spotify using a credentials blob
 *
 * \param[in] username Spotify username. UTF-8 encoded. Must not be longer than
 *              SP_MAX_USERNAME_LENGTH bytes (not UTF-8-encoded characters),
 *              not counting the terminating NULL.
 *              (For users that log in via Facebook, this is an email address.)
 * \param[in] credentials_blob Credentials blob received via ZeroConf or
 *              in the callback SpCallbackConnectionNewCredentials().  Note:
 *              if the credentials_blob is an empty string, this function
 *              should not be called or it will return kSpErrorFailed.
 * \return Returns an error code
 *
 * \note The login is performed asynchronously. The return value only indicates
 * whether the library is able to perform the login attempt. The status of
 * the login will be reported via callbacks:
 *
 *  - If the login is successful, the callback SpCallbackConnectionNotify()
 *    is called with the #kSpConnectionNotifyLoggedIn event.
 *  - If a login error occurs, the callback SpCallbackError() is called.
 *  - The application will receive a new credentials blob in the
 *    callback SpCallbackConnectionNewCredentials(). The application should
 *    store the new blob for subsequent logins using this function.
 *
 * \note The blob can only be used for subsequent logins as long as the value of
 * SpConfig::unique_id does not change. If SpConfig::unique_id has changed
 * since the blob was received, this function returns an error and you will
 * receive a debug message similar to "Parsing ZeroConf blob failed with code -3".
 *
 * \see SpConnectionIsLoggedIn
 */
SP_EMB_PUBLIC SpError SpConnectionLoginBlob(const char *username, const char *credentials_blob);

/**
 * \brief Log in a user to Spotify using a password
 *
 * \param[in] username Spotify username. UTF-8 encoded. Must not be longer than
 *              SP_MAX_USERNAME_LENGTH bytes (not UTF-8-encoded characters),
 *              not counting the terminating NULL.
 *              (For users that log in via Facebook, this is an email address.)
 * \param[in] password Password
 * \return Returns an error code
 *
 * \note The login is performed asynchronously. The return value only indicates
 * whether the library is able to perform the login attempt. The status of
 * the login will be reported via callbacks:
 *
 *  - If the login is successful, the callback SpCallbackConnectionNotify()
 *    is called with the #kSpConnectionNotifyLoggedIn event.
 *  - If a login error occurs, the callback SpCallbackError() is called.
 *
 * Returns kSpErrorGeneralLoginError if a connection is not present.
 * For logging in offline please use SpConnectionLoginBlob.
 *
 * Applications must not store the password. Instead, they should implement the
 * callback SpCallbackConnectionNewCredentials() and store the credentials blob
 * that they receive for subsequent logins using the function SpConnectionLoginBlob().
 *
 * \note Spotify Connect-enabled hardware devices must not use this function.
 * Such devices must implement the \ref ZeroConf and use
 * the function SpConnectionLoginBlob() instead.
 *
 * \see SpConnectionIsLoggedIn
 */
SP_EMB_PUBLIC SpError SpConnectionLoginPassword(const char *username, const char *password);

/**
 * \brief Log in a user to Spotify using a Spotify OAuth token
 *
 * \param[in] oauth_token Spotify OAuth access token with "streaming" scope.
 *              See https://developer.spotify.com/documentation/general/guides/authorization-guide/.
 * \return Returns an error code
 *
 * \note The login is performed asynchronously. The return value only indicates
 * whether the library is able to perform the login attempt. The status of
 * the login will be reported via callbacks:
 *
 *  - If the login is successful, the callback SpCallbackConnectionNotify()
 *    is called with the #kSpConnectionNotifyLoggedIn event.
 *  - If a login error occurs, the callback SpCallbackError() is called.
 *
 * For subsequent logins the SpCallbackConnectionNewCredentials() callback
 * should be implemented and the received credentials blob should be stored and
 * used. (Note that the OAuth access token itself expires after a short time.
 * The credentials blob returned by the callback allows you to re-login even
 * after the token has expired.)
 *
 * \note Spotify Connect-enabled hardware devices that implement the
 * \ref ZeroConf stack must use the function SpConnectionLoginBlob() instead.
 *
 * \see SpConnectionIsLoggedIn
 */
SP_EMB_PUBLIC SpError SpConnectionLoginOauthToken(const char *oauth_token);

/**
 * \brief Log the user out of Spotify
 *
 * \return Returns an error code
 *
 * \note The logout is performed asynchronously. The logout is complete
 * when the callback SpCallbackConnectionNotify() is called with the event
 * #kSpConnectionNotifyLoggedOut.
 *
 * \see SpConnectionIsLoggedIn
 */
SP_EMB_PUBLIC SpError SpConnectionLogout(void);

/**
 * \brief Is the user logged in to Spotify
 *
 * \retval 1 The user is logged in
 * \retval 0 The user is not logged in
 *
 * \see kSpConnectionNotifyLoggedIn, kSpConnectionNotifyLoggedOut,
 *      SpConnectionLoginBlob, SpConnectionLogout
 *
 * \note This API can be invoked from a callback.
 */
SP_EMB_PUBLIC uint8_t SpConnectionIsLoggedIn(void);

/**
 * \brief Get last Ack ID
 *
 * \note This API is currently undocumented.
 */
SP_EMB_PUBLIC const char *SpConnectionGetAckId(void);

/**
 * \brief Get the canonical username of the logged in user
 *
 * This function returns the canonical username of the logged in user, which is
 * the unique username used for identifying a specific user for things like
 * playlists and the Spotify Web API.
 *
 * This username might differ from the username used to login.  A user can
 * login with an e-mail address or non-canonical unicode.  This function will
 * return the canonicalized version of the username after a successful login.
 *
 * \note The canonical username should not be stored persistently.  Always
 * store the username as provided by the user, not the canonicalized version.
 *
 * \return Returns a string containing the username, or NULL if no user is
 * logged in.
 *
 * \note This API can be invoked from a callback.
 */
SP_EMB_PUBLIC const char *SpGetCanonicalUsername(void);

/**
 * \brief Set the display name for the application/device
 *
 * This function can be used to change the display name that was passed to
 * SpInit() in the field SpConfig::display_name.
 *
 * \param[in] display_name A UTF-8-encoded display name
 * \return Returns an error code
 *
 * \note The display name is not allowed to be an empty string.
 */
SP_EMB_PUBLIC SpError SpSetDisplayName(const char *display_name);

/**
 * \brief Set the volume steps the device is capable of.
 *
 *  This function will indicate the number of intermediate steps from
 *  ´<min_volume>´ to ´<max_volume>´ that the device supports.
 *  If there's no volume control ability it must be set to zero to inform
 *  that no volume control is possible at all.
 *  The default number of steps if this function is not called is 16.
 *
 *  \note There's no commitment from the other Connect clients to respect
 *  the volume steps. It's important to call this function passing zero
 *  if no volume control is possible though.
 *
 * \param[in] steps the number of volume steps the device can support.
 *                  0 means no volume steps at all.
 *                  The max value that is possible to set is 65535.
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpSetVolumeSteps(uint32_t steps);

/**
 * \brief Control if the device represents a group
 *
 * A group is a number of devices all playing back the same
 * sound synchronized. Setting this status correctly will
 * allow Spotify clients to display the correct metadata for
 * this device.
 *
 * \note If device aliases are used, this function should not be used to set the
 * group status. Instead, \ref SpSetDeviceAliases should be used to update group
 * status individually for each alias.
 *
 * \param[in] is_group 0: Indicate that this device is a single stand-alone device.
 *                     1: Indicate that this device represents a group.
 *
 */
SP_EMB_PUBLIC SpError SpSetDeviceIsGroup(int is_group);

/**
 * \brief Enable Connect functionality for this device
 *
 * A device with enabled Connect functionality will show up in other devices'
 * Connect pickers, and will be able to both control them and be controlled.
 *
 * The Spotify embedded library will enable Connect functionality by default
 *
 */
SP_EMB_PUBLIC SpError SpEnableConnect(void);

/**
 * \brief Disable Connect functionality for this device
 *
 * A device that disables Connect will not be able to control playback on
 * other devices, or be controlled by them.
 *
 */
SP_EMB_PUBLIC SpError SpDisableConnect(void);

/**
 * \brief Return the currently selected device alias.
 *
 * \return The currently selected device alias or -1 if no alias selected.
 */
SP_EMB_PUBLIC int SpGetSelectedDeviceAlias(void);

/**
 * \brief Allow the library to perform asynchronous tasks and process events
 *
 * \return Returns an error code
 *
 * This function must be called periodically as long as the library is in use.
 * Most of the API functions perform their tasks asynchronously. In order for
 * the tasks to be performed, SpPumpEvents() must be invoked. Most of the
 * callbacks are invoked while SpPumpEvents() is executed. (Some callbacks
 * are invoked directly by other API functions.)
 *
 * Note:
 * The suggested time interval to call this function is 10ms.
 * This function should not be called from a callback.
 *
 * A typical usage pattern looks like this:
 *
 * \code{.c}
 * int quit = 0;
 * SpInit(&conf);
 * SpConnectionLoginBlob(username, blob);
 * while (!quit) {
 *   // Process tasks and invoke callbacks
 *   SpPumpEvents();
 *   // Here the application should update its UI and
 *   // call other Spotify Embedded APIs in reaction
 *   // to the callback events that it received.
 * };
 * SpFree();
 * \endcode
 */
SP_EMB_PUBLIC SpError SpPumpEvents(void);

/**
 * \brief Register callbacks related to the connection to Spotify
 *
 * \param[in] cb Structure with pointers to individual callback functions.
 *              Any of the pointers in the structure may be NULL.
 * \param[in] context Application-defined pointer that will be passed unchanged as
 *              the \a context argument to the callbacks.
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpRegisterConnectionCallbacks(struct SpConnectionCallbacks *cb,
                                                    void *context);

/**
 * \brief Register a callback that receives debug messages/trace logs
 *
 * These callbacks can be registered before SpInit() has been called, in order
 * to receive debug logs that occur during initialization.
 *
 * \param[in] cb Structure with pointers to individual callback functions.
 * \param[in] context Application-defined pointer that will be passed unchanged as
 *              the \a context argument to the callback.
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpRegisterDebugCallbacks(struct SpDebugCallbacks *cb, void *context);

/**
 * @}
 */

/**
 * \addtogroup Playback
 * @{
 */

/**
 * \brief Register playback-related callbacks
 *
 * \param[in] cb Structure with pointers to individual callback functions.
 *              Any of the pointers in the structure may be NULL.
 * \param[in] context Application-defined pointer that will be passed unchanged as
 *              the \a context argument to the callbacks.
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpRegisterPlaybackCallbacks(struct SpPlaybackCallbacks *cb, void *context);

/**
 * \brief Retrieve metadata for a track in the current track list
 *
 * \param[out] metadata Structure to be filled with the metadata for the track
 * \param[in] relative_index Track index relative to the current track. Some
 *              relative indices are defined in the enum SpMetadataTrack.
 * \return Returns an error code. Returns kSpErrorFailed if \a relative_index
 *      is out of range.
 *
 * \note This API can be invoked from a callback.
 *
 * \note Be aware that many APIs that change the currently playing context
 *      are asynchronous, and the changes will not be immediately reflected
 *      in the metadata returned by SpGetMetadata(). For example, when calling
 *      SpPlaybackSkipToNext(), SpPlaybackEnableShuffle(), etc., the metadata
 *      returned by SpGetMetadata() might be unchanged while the command is
 *      being processed (which involves network communication). The notification
 *      #kSpPlaybackNotifyMetadataChanged will be sent as soon as SpGetMetadata()
 *      would return a different result for any \a relative_index defined in
 *      the enum #SpMetadataTrack.
 */
SP_EMB_PUBLIC SpError SpGetMetadata(struct SpMetadata *metadata, int relative_index);

/**
 * \brief Return the HTTP URL to an image file from a spotify:image: URI
 *
 * \param[in] image_uri image URI returned in SpMetadata::album_cover_uri
 * \param[out] image_url Pointer to a buffer that will be filled with HTTP
 *             URL.
 * \param[in] image_url_size size of the image_url buffer.
 *            SP_MAX_METADATA_IMAGE_URL_LENGTH is the max amount od data
 *            that can be returned in image_url.
 * \return Returns an error code. Returns kSpErrorFailed if the buffer is not
 *      big enough.
 *
 * \note This API can be invoked from a callback.
 */
SP_EMB_PUBLIC SpError SpGetMetadataImageURL(const char *image_uri, char *image_url,
                                            size_t image_url_size);

/**
 * \brief Obtain player cookie for current playback.
 *
 * The obtained player cookie can then be used to get more detailed metadata for current
 * playback from Spotify's backend using Spotify Web API.
 *
 * \param[out] player_cookie Pointer to a buffer where the player cookie will be copied.
 *             This buffer will be reset even if there is no player cookie available.
 * \param[in] player_cookie_size Size of the player_cookie buffer. Player cookie length
 *            is defined SP_PLAYER_COOKIE_LENGTH and the buffer should be at least
 *            SP_PLAYER_COOKIE_LENGTH+1 in size.
 *
 * \return Returns an error code. Returns kSpErrorUnsupported if the build configuration
 *      doesn't support player cookies.
 *
 * \note Experimental, subject to change
 */
SP_EMB_PUBLIC SpError SpGetPlayerCookie(char *player_cookie, size_t player_cookie_size);

/**
 * \brief Start or resume playback
 *
 * If the device is not currently active, calling this function will make
 * this device start playing the track that has been playing on another
 * Connect-enabled device.
 * \param[in] alias_index The index of the device alias to start playback on.
 *                        If aliases aren't used, pass -1.
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpPlaybackPlay(int alias_index);

/**
 * \brief Pause playback
 *
 * If the device is not the active speaker (SpPlaybackIsActiveDevice()),
 * the error code #kSpErrorNotActiveDevice is returned.
 *
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpPlaybackPause(void);

/**
 * \brief Skip playback to the next track in the track list
 *
 * If the device is not the active speaker (SpPlaybackIsActiveDevice()),
 * the error code #kSpErrorNotActiveDevice is returned.
 *
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpPlaybackSkipToNext(void);

/**
 * \brief Skip playback to the previous track in the track list
 *
 * If the device is not the active speaker (SpPlaybackIsActiveDevice()),
 * the error code #kSpErrorNotActiveDevice is returned.
 *
 * \note This function will try to skip to the previous track regardless
 *       of the current playback position. If the desired behaviour is to only
 *       skip to the previous track UNLESS the current playback position is beyond
 *       3 seconds, the following code example is suggested as a base:
 * \code{.c}
 * if (SpPlaybackGetPosition() / 1000 >= 3)
 *    SpPlaybackSeek(0);
 * else
 *    SpPlaybackSkipToPrev();
 * \endcode
 *
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpPlaybackSkipToPrev(void);

/**
 * \brief Seek to a position within the current track
 *
 * If the device is not the active speaker (SpPlaybackIsActiveDevice()),
 * the error code #kSpErrorNotActiveDevice is returned.
 *
 * \param[in] position_ms Position within the track in milliseconds
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpPlaybackSeek(uint32_t position_ms);

/**
 * \brief Seek a relative amount of time within the current track
 *
 * If the device is not the active speaker (SpPlaybackIsActiveDevice()),
 * the error code #kSpErrorNotActiveDevice is returned.
 *
 * \param[in] time_ms Amount of time to seek within the current track, negative
 *            values seek backwards and positive values seek forward.
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpPlaybackSeekRelative(int32_t time_ms);

/**
 * \brief Get the current playback position within the track
 * \return Playback position in milliseconds
 * \see \ref observing
 *
 * \note This API can be invoked from a callback.
 */
SP_EMB_PUBLIC uint32_t SpPlaybackGetPosition(void);

/**
 * \brief Request a change to the playback volume
 *
 * It is the application's responsibility to apply the volume change to its
 * audio output. This function merely notifies the library of the volume
 * change, so that the library can inform other Spotify Connect-enabled devices.
 *
 * Calling this function invokes the SpCallbackPlaybackApplyVolume() callback,
 * which the application can use to apply the actual volume change.
 *
 * \note When the library is initialized, it assumes a volume level of 65535
 * (maximum volume). The application must invoke SpPlaybackUpdateVolume() at some
 * point after calling SpInit() to inform the library of the actual volume level
 * of the device's audio output.
 *
 * \param volume Volume in the range 0 (silence) to 65535 (full volume)
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpPlaybackUpdateVolume(uint16_t volume);

/**
 * \brief Get the playback volume level
 *
 * This returns the last volume level that the application set using
 * SpPlaybackUpdateVolume() or that was reported to the application
 * using SpCallbackPlaybackApplyVolume().
 *
 * \return Volume level in the range 0 (silence) to 65535 (full volume).
 *
 * \note This API can be invoked from a callback.
 */
SP_EMB_PUBLIC uint16_t SpPlaybackGetVolume(void);

/**
 * \brief Is the playback status playing or paused
 *
 * \retval 1 Playback status is playing
 * \retval 0 Playback status is paused (or no playback has been started at all)
 *
 * \see kSpPlaybackNotifyPlay, kSpPlaybackNotifyPause,
 *      SpPlaybackPlay, SpPlaybackPause, \ref observing
 *
 * \note This API can be invoked from a callback.
 *
 * \note The result of this API is analogous to the playback notifications
 * kSpPlaybackNotifyPlay and kSpPlaybackNotifypause.
 */
SP_EMB_PUBLIC uint8_t SpPlaybackIsPlaying(void);

/**
 * \brief Is the the current track an Ad or not
 *
 * \retval 1 The current playing track is an Ad
 * \retval 0 The current playing track is not an Ad.
 *
 * \see kSpPlaybackNotifyTrackChanged
 *
 * \note This API can be invoked from a callback.
 */
SP_EMB_PUBLIC uint8_t SpPlaybackIsAdPlaying(void);

/**
 * \brief Is "shuffle" mode enabled
 *
 * \retval 1 Shuffle is enabled
 * \retval 0 Shuffle is disabled
 *
 * \see kSpPlaybackNotifyShuffleOn, kSpPlaybackNotifyShuffleOff,
 *      SpPlaybackEnableShuffle, \ref observing
 *
 * \note This API can be invoked from a callback.
 */
SP_EMB_PUBLIC uint8_t SpPlaybackIsShuffled(void);

/**
 * \brief Is "repeat" mode enabled
 *
 * \retval 1 Repeat is enabled
 * \retval 0 Repeat is disabled
 *
 * \see kSpPlaybackNotifyRepeatOn, kSpPlaybackNotifyRepeatOff,
 *      SpPlaybackEnableRepeat, \ref observing
 *
 * \note This API can be invoked from a callback.
 */
SP_EMB_PUBLIC uint8_t SpPlaybackIsRepeated(void);

/**
 * \brief What "repeat" mode is on
 *
 * \retval 0 Repeat is disabled
 * \retval 1 Repeat Context is enabled
 * \retval 2 Repeat Track is enabled
 *
 * \see kSpPlaybackNotifyRepeatOn, kSpPlaybackNotifyRepeatOff,
 *      SpPlaybackEnableRepeat, \ref observing
 *
 * \note This API can be invoked from a callback.
 */
SP_EMB_PUBLIC uint8_t SpPlaybackGetRepeatMode(void);

/**
 * \brief Is the device the active playback device
 *
 * \retval 1 The device is the active playback device
 * \retval 0 Another device is the active playback device
 *
 * \see kSpPlaybackNotifyBecameActive, kSpPlaybackNotifyBecameInactive, \ref observing
 *
 * \note This API can be invoked from a callback.
 */
SP_EMB_PUBLIC uint8_t SpPlaybackIsActiveDevice(void);

/**
 * \brief Enable or disable "shuffle" mode
 *
 * \param[in] enable 1 to enable, 0 to disable
 * \return Returns an error code
 *
 * The callback SpCallbackPlaybackNotify() will be called with the
 * event #kSpPlaybackNotifyShuffleOn or #kSpPlaybackNotifyShuffleOff.
 *
 * If the device is not the active speaker (SpPlaybackIsActiveDevice()),
 * the error code #kSpErrorNotActiveDevice is returned.
 *
 * \note The change to the shuffle mode might not take effect if the API is
 * invoked in the time window between requesting playback of a new context
 * (e.g., by calling SpPlayUri), and playback of the new context actually
 * starting.
 *
 * \see SpPlaybackIsShuffled
 */
SP_EMB_PUBLIC SpError SpPlaybackEnableShuffle(uint8_t enable);

/**
 * \brief Enable or disable "repeat" mode
 *
 * \param[in] enable 0 to disable, 1 to Repeat Context, 2 to Repeat Track
 *		The Repeat values where previously called Repeat and Repeat-1.
 * \return Returns an error code
 *
 * The callback SpCallbackPlaybackNotify() will be called with the
 * event #kSpPlaybackNotifyRepeatOn or #kSpPlaybackNotifyRepeatOff.
 *
 * If the device is not the active speaker (SpPlaybackIsActiveDevice()),
 * the error code #kSpErrorNotActiveDevice is returned.
 *
 * \see SpPlaybackIsRepeated
 */
SP_EMB_PUBLIC SpError SpPlaybackEnableRepeat(uint8_t enable);

/**
 * \brief Cycle through the available repeat modes.
 *
 * Cycles through repeat modes (repeat off, repeat context, repeat track)
 * given their current availability.
 * If for example repeat context is enabled and repeat track is disallowed
 * due to restrictions, this API will go directly to repeat off.
 *
 * @return Returns an error code.
 */
SP_EMB_PUBLIC SpError SpPlaybackCycleRepeatMode(void);

/**
 * \brief Change the bitrate at which compressed audio data is delivered.
 *
 * This will take effect for the next chunk of audio data that is streamed
 * from the backend. The format or sample rate of the audio data that is
 * received does not change.
 *
 * \param[in] bitrate The bitrate to be set
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpPlaybackSetBitrate(enum SpPlaybackBitrate bitrate);

/**
 * \brief Allow or disallow the device to start playback
 *
 * On some platforms, there might be certain situations in which playback
 * should be disallowed temporarily. In this case, when the user tries to
 * start playback on the device using the mobile application, the device
 * should be marked as "Unavailable for playback" in the UI.
 *
 * \note This functionality is reserved for specific integration scenarios.
 *       In most cases, when integrating the SDK into a device, this API must
 *       not be used. If the device is unable to play
 *       (for example, if a firmware upgrade is about to be performed), the
 *       application shall log out, shut down the library, and stop announcing
 *       the device via the \ref ZeroConf.)
 *
 * \param[in] can_play 2 to allow playback (default), 1 to disallow playback
 *            without becoming inactive, the playback will be paused,
 *            0 to disallow playback and become inactive.
 *
 * \note If the device is currently the active device when setting \a can_play to 0,
 *       the notification #kSpPlaybackNotifyBecameInactive will be sent.
 *       Playback-related APIs (SpPlaybackPlay(), ...) will return an error code.
 *       This setting will persist across logout/login.
 *
 * \see SpPlaybackIsAvailableToPlay
 *
 */
SP_EMB_PUBLIC SpError SpPlaybackSetAvailableToPlay(uint8_t can_play);

/**
 * \brief Is the device available for playback
 *
 * \retval 1 The device is available for playback
 * \retval 0 The device can't accept playback request nor
 *           start playback either.
 *
 * \see SpPlaybackSetAvailableToPlay
 */
SP_EMB_PUBLIC uint8_t SpPlaybackIsAvailableToPlay(void);

/**
 * \brief Increase the underrun counter of the current track
 *
 * If playback underruns have been detected in the current track,
 * use this API to report it to eSDK. This should only be called
 * when there was no data to play at all and there was a audible
 * glitch or gap for the user. It should only be called if audio
 * was expected to be played and there was audio before.
 * For example if eSDK is active and playing, but there is an
 * underrun, report it. If eSDK is active and was requested to
 * play something, but it never started, do not report it.
 * If eSDK is active and playing and user skips, there is an
 * expected gap, so report an underrun only if audio data started
 * being delivered from eSDK and then stopped.
 *
 * \param[in] count A counter of how many underruns happened. Some audio stacks have
 * only a getUnderrunCount, so more than one could have occurred since the last one.
 *
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpPlaybackIncreaseUnderrunCount(uint32_t count);

/**
 * \brief Set a limit on the download speed
 *
 * By calling this function eSDK will attempt to limit how fast it downloads a track.
 * In some use cases it is preferred to not use the full bandwidth. At the beginning
 * of a download eSDK will do a burst download and then try to obey the limit.
 *
 * \param[in] max_bytes_per_sec approximate bandwidth budget for downloads. To use default bandwidth
 * specify 0.
 *
 * \return Returns an error code
 *
 * \note eSDK is not guaranteed to stay strictly below the limit but will not exceed it by much
 * It is also not guaranteed to use all available bandwidth.
 */
SP_EMB_PUBLIC SpError SpPlaybackSetBandwidthLimit(uint32_t max_bytes_per_sec);

/**
 * \brief Get the number of seconds eSDK has buffered internally
 *
 * By calling this function eSDK will calculate how much of the current track is
 * buffered internally from the current position. This function does not include
 * data that has already been delivered to the application. The result is approximate
 * and not 100% accurate. It can be used to give information to the user of how much
 * has been buffered.
 *
 * \param[in] seconds the variable where the result will be set
 *
 * \return Returns an error code
 *
 * \note Result is approximate.
 */
SP_EMB_PUBLIC SpError SpPlaybackGetAvailableSeconds(uint32_t *seconds);

/**
 * \brief Activates redelivery of audio data on play or resume playback
 *
 * This function should be called to activate or deactivate audio
 * redelivery for the next calls to SpPlaybackPlay.
 *
 * When the client application can't keep unplayed audio in its playback buffers
 * (for example when audio from some other source was played while Spotify was
 * paused) the eSDK should be notified that redelivery of audio data is needed.
 * The audio data is redelivered from the last playback position reported by the
 * integration with the same precision as seek.
 * eSDK will need to redownload the data that was already delivered to integration
 * and therefore there will be a penalty of increased data consumption and latencies.
 *
 * Only use this function when unplayed audio is discarded.
 *
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpPlaybackSetRedeliveryMode(SpReDeliveryMode mode);

/**
 * \brief Gets the status of redelivery mode
 *
 * When redelivery mode is activated or deactivated through the API
 * SpPlaybackSetRedeliveryMode, an internal state is updated to keep
 * track of the redelivery behavior.
 * This API exposes this internal state.
 *
 * \return Returns redelivery mode status:
 * kSpRedeliveryModeActivated or kSpRedeliveryModeDeactivated
 */
SP_EMB_PUBLIC SpReDeliveryMode SpPlaybackIsRedeliveryModeActivated(void);

/**
 * @}
 */

/**
 * \addtogroup Preset
 * @{
 */

/**
 * \brief Subscribe to receive "preset" tokens for use with SpPlayPreset()
 *
 * This function subscribes to receive periodic "preset" tokens that represent
 * the currently playing context.  They can be used to resume from the current
 * context and position with SpPlayPreset().  This allows implementation of
 * presets, which can be mapped to physical or virtual buttons, to allow users
 * to save their favorite contexts for quick access.  The tokens must be saved
 * to persistent storage and be maintained across power cycles.
 *
 * SpPresetSubscribe() must be provided a buffer to store the preset tokens in.
 * This buffer must be a pointer to valid, writeable memory until the callback
 * is unregistered with SpPresetUnsubscribe(), SpConnectionLogout() or SpFree(),
 * or whenever the playing context changes.
 *
 * The tokens are delivered asynchronously via the SpCallbackSavePreset
 * callback, which must be registered with SpRegisterPlaybackCallbacks() prior
 * to calling SpPresetSubscribe().
 *
 * Each time the SpCallbackSavePreset callback is called, any previously saved
 * token for the current preset must be discarded and replaced with the updated
 * value.
 *
 * \note The buffer passed to SpPresetSubscribe() must be #SP_PRESET_BUFFER_SIZE,
 *       but the actual preset tokens received from the SpCallbackSavePreset
 *       callback will typically be much smaller.
 *
 * \note A subscription is only for the currently playing context, and only one
 *       subscription is possible at a time.
 *
 * \param[in] preset_id An integer indicating which preset this subscription is
 *              for.  For products with numbered presets, this should match the
 *              number of the preset button.  For use-cases with no integer
 *              mapping, this should be specified as -1.
 * \param[in] buffer Pointer to a buffer that will be filled with the preset
 *              tokens.  This must be a valid memory location until the preset
 *              is unregistered.
 * \param[in] buff_size Input: \a buff_size specifies the size of the
 *              buffer pointed to by \a buffer. The buffer should be big enough
 *              to hold #SP_PRESET_BUFFER_SIZE bytes.
 * \return Returns an error code
 *
 * \note At times, some contexts may not be possible to save as presets.  Errors
 *       will be indicated via the SpCallbackSavePreset callback.
 */
SP_EMB_PUBLIC SpError SpPresetSubscribe(int preset_id, uint8_t *buffer, size_t buff_size);

/**
 * \brief Unsubscribe from a previously subscribed preset
 *
 * SpPresetUnsubscribe() unsubscribes from receiving preset callbacks.  This is
 * the opposite of SpPresetSubscribe().  After calling this function, the
 * memory buffer provided to SpPresetSubscribe() is released by eSDK and can
 * be deallocated.
 *
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpPresetUnsubscribe(void);

/**
 * \brief Recall and play a saved or default preset
 *
 * SpPlayPreset() accepts a preset token that was previously received from
 * using SpPresetSubscribe(), restores the context and playback state to
 * as it was when the token was saved, and starts playback.
 *
 * For default user-recommended playlists, SpPlayPreset() can be used with
 * buffer set to NULL.  Spotify will pick a default playlist to play based
 * on popularity and user recommendations.  The correct preset_id should
 * still be provided to guarantee a good mixture of recommended playlists.
 * Default presets do not require subscribing with SpPresetSubscribe().
 *
 * Integrations should always use SpPresetSubscribe to subscribe to token
 * updates in conjunction with using SpPlayPreset.
 *
 * \param[in] preset_id An integer indicating which preset this token is
 *              for.  For products with numbered presets, this should match the
 *              number of the preset button, starting from 1.  For use-cases
 *              with no integer mapping, this should be specified as -1.
 * \param[in] playback_position The playback position specified with the preset
 *              token from the SpCallbackSavePreset callback.
 * \param[in] buffer Preset blob obtained with SpPresetSubscribe(), or NULL to
 *             play a default user-recommended playlist.
 * \param[in] buff_size The size of \a buffer, or 0 if buffer is NULL.
 * \param[in] alias_index The index of the device alias to start playback on.
 *                        If aliases aren't used, pass -1.
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpPlayPreset(int preset_id, uint32_t playback_position, uint8_t *buffer,
                                   size_t buff_size, int alias_index);

/**
 * @}
 */

/**
 * \addtogroup ZeroConf
 * @{
 */

/**
 * \brief Get variables for ZeroConf, mainly the "getInfo" request
 *
 * The application should use this function to retrieve the data that it should
 * send in the response to the "getInfo" request. See the \ref ZeroConf
 * manual for more information. There are also other fields here that might be
 * needed for ZeroConf.
 *
 * \param[out] vars Structure to be filled with the variables
 *
 * \note This API can be invoked from a callback.
 */
SP_EMB_PUBLIC SpError SpZeroConfGetVars(struct SpZeroConfVars *vars);

/**
 * \brief Temporarily pause ZeroConf mDNS annoucements
 *
 * \note This call requries ZeroConf to be started (by setting SpConfig::zeroconf_serve
 *       when calling SpInit())
 *
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpZeroConfAnnouncePause(void);

/**
 * \brief Resume ZeroConf mDNS annoucement after calling SpZeroConfAnnouncePause()
 *
 * \note This call requries ZeroConf to be started (by setting SpConfig::zeroconf_serve
 *       when calling SpInit())
 *
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpZeroConfAnnounceResume(void);

/**
 * \brief Log in a user to Spotify using a ZeroConf credentials blob
 *
 * This function logs in with the information that the application receives
 * in the "addUser" ZeroConf request. See the \ref ZeroConf manual.
 *
 * \param[in] username Spotify username. UTF-8 encoded. Must not be longer than
 *              SP_MAX_USERNAME_LENGTH bytes (not UTF-8-encoded characters),
 *              not counting the terminating NULL.
 * \param[in] zero_conf_blob Credentials blob from the "blob" field of the request.
 *              Must not be longer than SP_MAX_ZEROCONF_BLOB_LENGTH bytes,
 *              not counting the terminating NULL.
 * \param[in] client_key Client key from the "clientKey" field of the request.
 *              This may be NULL if not supplied in the "addUser" request.
 *              Must not be longer than SP_MAX_CLIENT_KEY_LENGTH bytes,
 *              not counting the terminating NULL.
 * \param[in] login_id Login ID from the "loginId" field of the request.
 *              This may be NULL if not supplied in the "addUser" request.
 *              Must not be longer than SP_MAX_LOGIN_ID_LENGTH bytes,
 *              not counting the terminating NULL.
 * \param[in] token_type Token type from the "tokenType" field of the request.
 *              This may be NULL if not supplied in the "addUser" request.
 *              Must not be longer than SP_MAX_TOKEN_TYPE_LENGTH bytes,
 *              not counting the terminating NULL.
 * \return Returns an error code
 *
 * \note The login is performed asynchronously. The return value only indicates
 * whether the library is able to perform the login attempt. The status of
 * the login will be reported via callbacks:
 *
 *  - If the login is successful, the callback SpCallbackConnectionNotify()
 *    is called with the #kSpConnectionNotifyLoggedIn event.
 *  - If a login error occurs, the callback SpCallbackError() is called.
 *  - The application will receive a new credentials blob in the
 *    callback SpCallbackConnectionNewCredentials(). The application should
 *    store the new blob for subsequent logins using the function
 *    SpConnectionLoginBlob().
 *
 * \see SpConnectionIsLoggedIn
 */
SP_EMB_PUBLIC SpError SpConnectionLoginZeroConf(const char *username, const char *zero_conf_blob,
                                                const char *client_key, const char *login_id,
                                                const char *token_type);

/**
 * This function can be used to get the brand name. If the field
 * SpConfig::brand_display_name was set at SpInit(), it returns that,
 * otherwise it returns what was set in the mandatory field SpConfig::brand_name.
 *
 * \return The UTF-8-encoded brand name
 *
 * \note This API can be invoked from a callback.
 */
SP_EMB_PUBLIC const char *SpGetBrandName(void);

/**
 * This function can be used to get the model name. If the field
 * SpConfig::model_display_name was set at SpInit(), it returns that,
 * otherwise it returns what was set in the mandatory field SpConfig::model_name.
 *
 * \return The UTF-8-encoded model name
 *
 * \note This API can be invoked from a callback.
 */
SP_EMB_PUBLIC const char *SpGetModelName(void);

/**
 * \brief Callback for receiving selected device alias from the backend
 *
 * To register this callback, use the function SpRegisterDeviceAliasCallbacks().
 *
 * This callback is invoked whenever the selected device alias is updated.
 * This can happen when, for example, the user selects an alias from Spotify
 * Connect device picker.
 *
 * \param[in] alias_index Index of the device alias which was selected
 * \param[in] context Context pointer that was passed when registering the callback
 *
 * \note The application should not block or call API functions that are not allowed
 * in the callback.
 */
typedef void (*SpCallbackSelectedDeviceAliasChanged)(uint32_t alias_index, void *context);

/**
 * \brief Callback for knowing when the device alias list has been updated after
 * call to \ref SpSetDeviceAliases
 *
 * To register this callback, use the function SpRegisterDeviceAliasCallbacks().
 *
 * This callback is invoked when the operation started by call to
 * \ref SpSetDeviceAliases has finished.
 *
 * \param[in] error_code if the update was successful, the value is kSpErrorOk
 * \param[in] context Context pointer that was passed when registering the callback
 *
 * \note The application should not block or call API functions that are not allowed
 * in the callback.
 */
typedef void (*SpCallbackDeviceAliasesUpdateDone)(SpError error_code, void *context);

/**
 * \brief Callbacks to be registered with SpRegisterDeviceAliasCallbacks()
 *
 * Any of the pointers may be NULL.
 *
 * See the documentation of the callback typedefs for information about the
 * individual callbacks.
 */
struct SpDeviceAliasCallbacks {
    /** \brief Selected device alias updated callback */
    SpCallbackSelectedDeviceAliasChanged on_selected_device_alias_changed;
    /** \brief Device alias list updated */
    SpCallbackDeviceAliasesUpdateDone on_device_aliases_update_done;
};

/**
 * \brief Register callbacks related to device aliases
 *
 * \param[in] cb Structure with pointers to individual callback functions.
 *              Any of the pointers in the structure may be NULL.
 * \param[in] context Application-defined pointer that will be passed unchanged as
 *              the \a context argument to the callbacks.
 * \return Returns an error code
 */
SP_EMB_PUBLIC SpError SpRegisterDeviceAliasCallbacks(struct SpDeviceAliasCallbacks *cb,
                                                     void *context);

/**
 * \brief Update the device alias definitions.
 *
 * Call this whenever the current alias defintions are updated.
 * The id values for the aliases must be unique within the array.
 *
 * \param[in] aliases Pointer to an array of SpDeviceAlias structs filled in
 * with the new alias names and corresponding attributes. The array size must be
 * of size SP_MAX_DEVICE_ALIASES.
 */
SP_EMB_PUBLIC SpError SpSetDeviceAliases(const struct SpDeviceAlias *aliases);

/**
 * @}
 */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
