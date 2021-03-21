/**
 * \file spotify_embedded_tls.h
 * \brief The public Spotify Embedded API
 * \copyright Copyright 2016 - 2020 Spotify AB. All rights reserved.
 */

#ifndef _SPOTIFY_EMBEDDED_TLS_H
#define _SPOTIFY_EMBEDDED_TLS_H

#include "spotify_embedded.h"
#include "spotify_embedded_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup TLS TLS Abstraction Layer
 *
 * \brief Functionality related to eSDK TLS callbacks
 */

/**
 * \addtogroup TLS
 * @{
 */

/**
 * \brief This callback is invoked by eSDK to let the TLS library integration
 * perform any one-time initialization.
 *
 * \param context Context provided in callback register function.
 *
 * \return Should return \ref kSpAPINoError if successfully initialized.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef enum SpAPIReturnCode (*SpCallbackTLSInit)(void *context);

/**
 * \brief This callback is invoked by eSDK to let the TLS library integration
 * perform deallocation of resource during teardown.
 *
 * \param context Context provided in callback register function.
 *
 * \return Should return \ref kSpAPINoError if successfully executed.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef enum SpAPIReturnCode (*SpCallbackTLSDeinit)(void *context);

/**
 * \brief This callback is invoked once in the beginning of every TLS
 * connection.
 *
 * The callback receives a socket via the pointer to \ref SpSocketHandle that
 * is already connected to the remote peer. This callback should typically
 * allocate and setup all TLS related resources. The \a tls field of the \ref
 * SpSocketHandle should be used to store any connection specific state that is
 * needed.
 *
 * \param socket Pointer to eSDK socket already connected to the remote peer.
 * \param hostname Hostname of the remote peer.
 * \param context Context provided in callback register function.
 *
 * \return Should return \ref kSpAPINoError if successful.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef enum SpAPIReturnCode (*SpCallbackTLSCreate)(struct SpSocketHandle *socket,
                                                    const char *hostname, void *context);

/**
 * \brief This callback is invoked by eSDK to perform the TLS handshake.
 *
 * The callback is invoked repeatedly to perform the handshake. This callback
 * is invoked repeatedly as long as it returns kSpAPITryAgain.
 *
 * The peer verification is mandatory and the implementation of this callback
 * must validate the peer certificate against a list of trusted CA
 * certificates.
 *
 * \param socket Pointer to eSDK socket already connected to the remote peer.
 * \param hostname Hostname of the remote peer.
 * \param context Context provided in callback register function.
 *
 * \return Should return \ref kSpAPINoError if the TLS handshake completed
 * successfully.
 *
 * Should return \ref kSpAPITryAgain if handshake is still in progress and the
 * callback should be invoked again.
 *
 * Should return \ref kSpAPIGenericError is the handshake failed. Any specific
 * information about the reason for the failure will be returned in the \ref
 * SpCallbackTLSGetError call.
 *
 * \note The application must not block or call other API functions in the callback.
 */
typedef enum SpAPIReturnCode (*SpCallbackTLSHandshake)(struct SpSocketHandle *socket,
                                                       void *context);

/**
 * \brief This callback is invoked by eSDK to read data on a TLS connection
 *
 * \param socket Pointer to eSDK socket already connected to the remote peer.
 * \param buf Pointer to buffer for the data to be received.
 * \param len The size of the buffer (in bytes).
 * \param [out] actual Actual number of bytes received.
 * \param context Context provided in callback register function.
 *
 * \return Should return \ref kSpAPINoError if successful.
 * Any other return value is considered an error. Any specific information
 * about the reason for the failure will be returned in the \ref
 * SpCallbackTLSGetError call.
 *
 * The number of bytes read to the destination buffer is reported via the \a actual
 * parameter. If no data is currently available the \a actual parameter is set to zero.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef enum SpAPIReturnCode (*SpCallbackTLSRead)(struct SpSocketHandle *socket, void *buf,
                                                  size_t len, size_t *actual, void *context);

/**
 * \brief This callback is invoked by eSDK to write data on a TLS connection
 *
 * \param socket Pointer to eSDK socket already connected to the remote peer.
 * \param buf Pointer to buffer with data to be written.
 * \param len Number of bytes of data in buffer.
 * \param [out] actual Number of byte actually written.
 * \param context Context provided in callback register function.
 *
 * \return Should return \ref kSpAPINoError if successful. The number of bytes actually written is
 * returned via the \a actual parameter.
 * Any other return value is considered an error. Any specific information
 * about the reason for the failure will be returned in the \ref
 * SpCallbackTLSGetError call.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef enum SpAPIReturnCode (*SpCallbackTLSWrite)(struct SpSocketHandle *socket, const void *buf,
                                                   size_t len, size_t *actual, void *context);

/**
 * \brief This callback should clean up any resources allocated in the connect callback.
 *
 * \param socket Pointer to eSDK socket already connected to the remote peer.
 * \param context Context provided in callback register function.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef void (*SpCallbackTLSClose)(struct SpSocketHandle *socket, void *context);

/**
 * \brief Callback invoked to get an error message for the last error
 *
 * The implementation of this callback should put a error message in the form
 * of a zero-terminated string in the buffer pointed to by \a buf. This error
 * message should describe the latest error returned by any of the other
 * callback functions.
 *
 * \param socket Pointer to eSDK socket already connected to the remote peer.
 * \param context buf Pointer to buffer that will receive the error message.
 * \param context len Length of the buffer.
 * \param context Context provided in callback register function.
 *
 * \return Should return the error code for the last error, homologous to
 * the error message.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef int (*SpCallbackTLSGetError)(struct SpSocketHandle *socket, char *buf, size_t len,
                                     void *context);

/**
 * \brief Callbacks to be registered with SpRegisterTLSCallbacks()
 *
 * None of the pointers may be NULL.
 *
 * See the documentation of the callback typedefs for information about the
 * individual callbacks.
 */
struct SpTLSCallbacks {
    /** \brief Callback that performs one-time initialization */
    SpCallbackTLSInit init;
    /** \brief Callback that performs release of resources allocated during init */
    SpCallbackTLSDeinit deinit;
    /** \brief Callback invoked once per connection to initialize TLS context */
    SpCallbackTLSCreate create;
    /** \brief Callback invoked repeatedly to perform the TLS handshake */
    SpCallbackTLSHandshake handshake;
    /** \brief Callback for reading from the TLS data stream */
    SpCallbackTLSRead read;
    /** \brief Callback for writing to the TLS data stream */
    SpCallbackTLSWrite write;
    /** \brief Callback invoked to cleanup any TLS context before closing the socket */
    SpCallbackTLSClose close;
    /** \brief Callback invoked to get an error message for the last error */
    SpCallbackTLSGetError get_error;
};

/**
 * \brief Register TLS-related callbacks.
 *
 * \param[in] callbacks Structure with pointers to individual callback functions.
 *                      All pointers must be valid.
 * \param[in] context Application-defined pointer that will be passed unchanged as
 *                   the \a context argument to the callbacks.
 *
 * \return Returns an error code
 *
 * \note A call to this function has to be performed before \ref SpInit is
 * called. Calling this function when eSDK is initialized will fail with
 * kSpErrorAlreadyInitialized.
 */
SP_EMB_PUBLIC SpError SpRegisterTLSCallbacks(struct SpTLSCallbacks *callbacks, void *context);

/**
 * \brief Add root certificate to the eSDK TLS stack.
 *
 * \param[in] certificate Either a binary DER certificate or a string with a PEM-format certificate.
 * \note If the buffer is a PEM-format certificate, it must be NULL-terminated.
 * \param[in] length The length of certificate.
 * \note If the buffer is a PEM-format certificate, this length must include the NULL termination.
 * \param[out] underlying_error If this pointer is non-NULL, and there is a certificate error,
 *                                the error from the underlying TLS stack (currently MbedTLS) will
 *                                be stored here. May be NULL.
 *
 * \return Returns \ref kSpErrorInvalidArgument if the certificate could not be parsed
 *                 \ref kSpErrorAlreadyInitialized if the function is called between \ref SpInit
 *                                                 and \ref SpFree.
 *                 If the return value is \ref kSpErrorInvalidArgument and
 *                 underlying_error is non-NULL, there will be an error code
 *                 from the underlying TLS stack (currently MbedTLS)
 *                 returned in *underlying_error.
 *
 * \note Calls to this function have to be performed before \ref SpInit is
 * called or after \ref SpFree.
 */
SP_EMB_PUBLIC SpError SpTLSAddCARootCert(const uint8_t *certificate, size_t length,
                                         int *underlying_error);

/**
 * \brief Remove all certificates loaded on the TLS stack and frees the
 * memory used by them.
 *
 * \return kSpErrorUnavailable if the function is called between SpInit and SpFree.
 */
SP_EMB_PUBLIC SpError SpTLSFreeCARootCerts(void);
/**
 * @}
 */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _SPOTIFY_EMBEDDED_TLS_H */
