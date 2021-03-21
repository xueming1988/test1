/**
 * \file spotify_embedded_hal.h
 * \brief The public Spotify Embedded API
 * \copyright Copyright 2016 - 2020 Spotify AB. All rights reserved.
 */

#ifndef _SPOTIFY_EMBEDDED_HAL_H
#define _SPOTIFY_EMBEDDED_HAL_H

#include "spotify_embedded.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup HAL Hardware Abstraction Layer
 *
 * \brief Functionality related to eSDK hardware abstraction layer
 *
 * Purpose of provided API is to abstract eSDK from the hardware it runs on.
 * API consist of several independent parts. Each part can be recognized easily
 * if it has certain \b SpRegister.. function.
 * For example: \ref SpRegisterDnsHALCallbacks is DNS part.
 * \ref SpRegisterSocketHALCallbacks is socket part.
 * Each part can be used separately of each other or together.
 * For socket HAL to work correctly all callbacks must be implemented by the
 * client and set correctly.
 *
 * \note On most platforms there is a default implementation of the HAL that
 * will be used unless explicitly overridden by the \b SpRegister... functions
 * available in this header file.
 */

/**
 * \addtogroup HAL
 * @{
 */

/**
 * \brief Enum describes IP family for which certain operation is taken place
 */
enum SpIPFamily {
    /** \brief IP v4 family */
    kSpIPV4 = 0,
    /** \brief IP v6 family */
    kSpIPV6 = 1
};

/**
 * \brief Struct contains resolved hostname ip address and its family
 */
struct SpSockaddr {
    /** \brief IP protocol family for which lookup is requested */
    enum SpIPFamily family;
    /** \brief Ip address. Network byte order. */
    uint8_t addr[16];
    /** \brief Contains port value if applicable. Host byte order. */
    int port;
};

/**
 * \brief Socket pool IDs
 */
enum SpSocketPool {
    /** \brief Sockets used for backend, streaming, etc */
    kSpSocketPoolGeneral = 0,
    /** \brief Sockets used for ZeroConf */
    kSpSocketPoolZeroConf = 1
};

/**
 * \brief This callback if set by client is called by eSDK to perform
 *         DNS lookup. If expected that lookup could take significant
 *         amount of time client may do it asynchronously in which
 *         case value of kSpAPITryAgain could be returned. \see
 *         SpAPIReturnCode
 *
 * \param hostname Name to be resolved.
 * \param sockaddr Pointer to SpSockaddr structure. \see SpSockaddr.
 *         eSDK expects that result returned from \b gethostbyname or
 *         similar function would be \b memcpy into addr buffer.
 *         \see Note: \b getaddrinfo resolves a linked list of addresses (IPv6, IPv4, ...)
 *         This sockaddr parameter only contains a copy of the first element
 *         of the linked list. As a result, all other elements of the list
 *         will be lost, if any.
 * \param context Context provided in callback register procedure.
 * \return kSpAPINoError - in case of success, kSpAPITryAgain if lookup should
 *         be done again with the same parameters, values < 0 are errors. The
 *         HAL is responsible for having a reasonable timeout so that
 *         kSpAPITryAgain is not returned indefinitely.
 *         Should return \ref kSpAPINotSupported if the client has no support
 *         for some family values.
 * \see SpAPIReturnCode
 *
 * \note The application should not block or call other API functions
 *        in the callback.
 * Port value of \ref SpSockaddr is ignored.
 */
typedef enum SpAPIReturnCode (*SpCallbackPerformDNSLookup)(const char *hostname,
                                                           struct SpSockaddr *sockaddr,
                                                           void *context);

/**
 * \brief Callbacks to be registered with SpRegisterDnsHALCallbacks()
 *
 * Any of the pointers may be NULL. To remove DNS callback at any time call \ref
 * SpRegisterDnsHALCallbacks
 * with SpDnsHALCallbacks which has dns_lookup_callback set to NULL.
 *
 * \note See the documentation of the callback typedefs for information about the
 * individual callbacks.
 */
struct SpDnsHALCallbacks {
    /** \brief DNS lookup callback. If NULL eSDK will use its internal DNS resolve mechanism. */
    SpCallbackPerformDNSLookup dns_lookup_callback;
};

/**
 * \brief Register HAL-related callbacks. Should be called right after \ref SpInit.
 *
 * \param[in] callbacks Structure with pointers to individual callback functions.
 *                      Any of the pointers in the structure may be NULL.
 * \param[in] context Application-defined pointer that will be passed unchanged as
 *                    the \a context argument to the callbacks.
 * \return Returns an error code.
 *
 * \note To remove DNS callback at any time call \ref SpRegisterDnsHALCallbacks
 * with SpDnsHALCallbacks which has dns_lookup_callback set to NULL.
 * Setting DNS callback make sense only if eSDK uses its own socket implementation.
 * If the client provides whole socket API it is also reponsible for hostname resolving if needed.
 * Setting DNS callbacks with provided socket callbacks makes no effect.
 */
SP_EMB_PUBLIC SpError SpRegisterDnsHALCallbacks(struct SpDnsHALCallbacks *callbacks, void *context);

/**
 * \brief Get eSDK's default DNS callbacks
 *
 * \param[in] callbacks \ref SpDnsHALCallbacks struct to be populated with pointers to callback
 * functions
 *
 * \return Returns an error code.
 *
 * \note These callbacks are made available so they may be wrapped by custom DNS
 * functions provided with \ref SpRegisterDnsHALCallbacks.
 */
SP_EMB_PUBLIC SpError SpGetDefaultDnsHALCallbacks(struct SpDnsHALCallbacks *callbacks);

/**
 * \brief Enum defines type of socket supported by eSDK
 */
enum SpSocketType {
    /** \brief Stream socket type */
    kSpSocketStream = 0,
    /** \brief Datagram socket type */
    kSpSocketDgram = 1
};

/**
 * \brief Enum defines options that can be applied to sockets
 */
enum SpSocketOptions {
    /** \brief Nonblocking mode has to be set by the implementation if possible.
     *  POSIX analog: TCP_NODELAY */
    kSpSocketNonBlocking = 0,
    /** \brief Reuse address mode has to be set if possible. POSIX analog: SO_REUSEADDR. Value 0 or
       1. */
    kSpSocketReuseAddr = 1,
    /** \brief Reuse port mode has to be set if possible. POSIX analog: SO_REUSEPORT. Value 0 or 1.
       */
    kSpSocketReusePort = 2,
    /** \brief Multicast TTL has to be set if possible. POSIX analog: IP_MULTICAST_TTL. Value [0,
       255].*/
    kSpSocketMulticastTTL = 3,
    /** \brief Multicast loop has to be set if possible. POSIX analog: IP_MULTICAST_LOOP. Value 0 or
       1. */
    kSpSocketMulticastLoop = 4,
    /** \brief Set group address if possible. POSIX analog: IP_ADD_MEMBERSHIP.
        value is pointer \ref SpSockaddr. Address is in network byte order. Port field is not
       applicable. */
    kSpSocketMembership = 5,
    /** \brief Set outgoing multicast interface. POSIX analog: IP_MULTICAST_IF.
            value is pointer to \ref SpSockaddr. Address is in network byte order. Port field is not
       applicable. */
    kSpSocketMcastSendIf = 6
};

/**
 * \brief Socket handle type
 */
struct SpSocketHandle {
    /** \brief Client defined socket representation */
    void *handle;
    /** \brief Can be used by the TLS implementation to store connection specific state */
    void *tls;
};

/**
 * \brief This callback if set by client is called by eSDK to create socket of certain type and
 * family.
 *
 * \param family Socket family as specified in \ref SpIPFamily
 * \param type Socket type as specified in \ref SpSocketType
 * \param pool_id Socket pool ID to create the new socket from
 * \param socket Pointer to valid \ref SpSocketHandle instance in case of success. Unchanged in case
 * of failure.
 * \param context Context provided in callback register procedure.
 *
 * \return Should return \ref kSpAPINoError in case of successful socket creation.
 * Should return \ref kSpAPIGenericError in case of socket creation failed.
 * Should return \ref kSpAPINotSupported if the client has no support for some family/type values.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef enum SpAPIReturnCode (*SpCallbackSocketCreate)(enum SpIPFamily family,
                                                       enum SpSocketType type,
                                                       enum SpSocketPool pool_id,
                                                       struct SpSocketHandle **socket,
                                                       void *context);

/**
 * \brief This callback if set by client is called by eSDK to set specific options on previously
 * created socket.
 *
 * \param socket Socket instance to perform action with.
 * \param option One of \ref SpSocketOptions options.
 * \param value Option value. Depends on particular \ref SpSocketOptions option.
 * \param context Context provided in callback register procedure.
 *
 * \return Should return \ref kSpAPINoError in case of successful option set.
 * Should return \ref kSpAPIGenericError in case of option set failed.
 * Should return \ref kSpAPINotSupported if requested option is not supported by the platform.
 * \ref SpCallbackSocketError can be called to get underlying OS error.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef enum SpAPIReturnCode (*SpCallbackSocketSetOption)(struct SpSocketHandle *socket,
                                                          enum SpSocketOptions option, void *value,
                                                          void *context);

/**
 * \brief This callback if set by client is called by eSDK to close previously opened socket.
 *
 * \param socket Socket instance to perform action with.
 * \param context Context provided in callback register procedure.
 *
 * \return Should return \ref kSpAPINoError in case of successful socket close.
 * Should return \ref kSpAPIGenericError in case of socket close failed.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef enum SpAPIReturnCode (*SpCallbackSocketClose)(struct SpSocketHandle *socket, void *context);

/**
 * \brief This callback if set by client is called by eSDK to bind to provided socket.
 *
 * \param socket Socket instance to perform action with.
 * \param port Port which eSDK would listen to (host byte order). If (*port) is zero, the bound port
 * should be stored in (*port).
 * \param context Context provided in callback register procedure.
 *
 * \return Should return \ref kSpAPINoError in case of successful listen call.
 * Should return \ref kSpAPIGenericError in case of failed listen call.
 * Should return \ref kSpAPINotSupported if the client has no bind.
 * \ref SpCallbackSocketError can be called to get underlying OS error.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef enum SpAPIReturnCode (*SpCallbackSocketBind)(struct SpSocketHandle *socket, int *port,
                                                     void *context);

/**
 * \brief This callback if set by client is called by eSDK to start listening provided socket.
 *        This callbacks has no effect on UPD sockets.
 *
 * \param socket Socket instance to perform action with.
 * \param backlog Parameter defines the maximum length for the queue of pending connections.
 * \param context Context provided in callback register procedure.
 *
 * \return Should return \ref kSpAPINoError in case of successful listen call.
 * Should return \ref kSpAPIGenericError in case of failed listen call.
 * Should return \ref kSpAPINotSupported if the client has no listen.
 * \ref SpCallbackSocketError can be called to get underlying OS error.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef enum SpAPIReturnCode (*SpCallbackSocketListen)(struct SpSocketHandle *socket, int backlog,
                                                       void *context);

/**
 * \brief This callback if set by client is called by eSDK to connect to specified host and remote
 * port.
 *
 * \param socket Socket instance to perform action with.
 * \param addr Address to connect to.
 * \param context Context provided in callback register procedure.
 *
 * \return Should return \ref kSpAPINoError in case of successful connection.
 * Should return \ref kSpAPIGenericError in case of failed connection.
 * If the client wants to perform asynchronous DNS lookup in that call one shall return
 * kSpAPITryAgain.
 * If the client wants to perform synchronous DNS lookup and it failed one shall return
 * kSpAPIDNSLookupError.
 * eSDK will perform this call some time later in that case.
 * \ref SpCallbackSocketError can be called to get underlying OS error.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef enum SpAPIReturnCode (*SpCallbackSocketConnect)(struct SpSocketHandle *socket,
                                                        const struct SpSockaddr *addr,
                                                        void *context);

/**
 * \brief This callback if set by client is called by eSDK to accept connection on provided socket.
 *
 * \param socket Socket instance to perform action with.
 * \param pool_id Socket pool ID to create the new socket from
 * \param out_socket Socket created by accepting connection.
 * \param context Context provided in callback register procedure.
 *
 * \return Should return \ref kSpAPINoError in case of successful accept.
 * Should return \ref kSpAPIGenericError in case of failed accept.
 * Should return \ref kSpAPINotSupported if the client has no accept.
 * \ref SpCallbackSocketError can be called to get underlying OS error.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef enum SpAPIReturnCode (*SpCallbackSocketAccept)(struct SpSocketHandle *socket,
                                                       enum SpSocketPool pool_id,
                                                       struct SpSocketHandle **out_socket,
                                                       void *context);

/**
 * \brief This callback if set by client is called by eSDK to read the data from socket.
 *
 * \param socket Socket instance to perform action with.
 * \param data Buffer to read data into.
 * \param data_size Amount of requested data to read.
 * \param bytes_read Pointer to store how much bytes actually read. If NULL, read is still
 * performed. Should be unchanged if read failed.
 * \param context Context provided in callback register procedure.
 *
 * \return Should return \ref kSpAPINoError in case of successful read.
 * Should return \ref kSpAPIGenericError in case of read has failed.
 * Should return \ref kSpAPIEOF in case of end of stream reached.
 * \ref SpCallbackSocketError can be called to get underlying OS error.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef enum SpAPIReturnCode (*SpCallbackSocketRead)(struct SpSocketHandle *socket, void *data,
                                                     int data_size, int *bytes_read, void *context);

/**
 * \brief This callback if set by client is called by eSDK to write data to socket.
 *
 * \param socket Socket instance to perform action with.
 * \param data Buffer with data to be written.
 * \param data_size Amount of data to write.
 * \param bytes_written Pointer to store how much bytes actually written. If NULL, write is still
 * performed. Should unchanged if write failed.
 * \param context Context provided in callback register procedure.
 *
 * \return Should return \ref kSpAPINoError in case of successful socket write.
 * Should return \ref kSpAPIGenericError in case of socket write has failed.
 * \ref SpCallbackSocketError can be called to get underlying OS error.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef enum SpAPIReturnCode (*SpCallbackSocketWrite)(struct SpSocketHandle *socket,
                                                      const void *data, int data_size,
                                                      int *bytes_written, void *context);

/**
 * \brief This callback if set by client is called by eSDK to read the data from socket addressed by
 * \ref SpSockaddr instance.
 *
 * \param socket Socket instance to perform action with.
 * \param data Buffer to read data into.
 * \param data_size Amount of requested data to read.
 * \param addr Platform defined addr pointer. eSDK does not modify it. Just pass through to
 * write_to.
 * \param bytes_read Pointer to store how much bytes actually read. If NULL, read is still
 * performed. Should be unchanged if read failed.
 * \param context Context provided in callback register procedure.
 *
 * \return Should return \ref kSpAPINoError in case of successful read.
 * Should return \ref kSpAPIGenericError in case of read has failed.
 * Should return \ref kSpAPIEOF in case of end of stream reached.
 * Should return \ref kSpAPINotSupported if the client has no recvfrom.
 * \ref SpCallbackSocketError can be called to get underlying OS error.
 *
 * \note The application should not block or call other API functions in the callback.
 * This callback should mirror the behavior of recvfrom for connected/non-connected sockets.
 */
typedef enum SpAPIReturnCode (*SpCallbackSocketReadFrom)(struct SpSocketHandle *socket, void *data,
                                                         int data_size, const void **addr,
                                                         int *bytes_read, void *context);

/**
 * \brief This callback if set by client is called by eSDK to write data to socket addressed by \ref
 * SpSockaddr instance.
 *
 * \param socket Socket instance to perform action with.
 * \param data Buffer with data to be written.
 * \param data_size Amount of data to write.
 * \param addr Pointer received in the call to \ref SpCallbackSocketReadFrom.
 * \param bytes_written Pointer to store how much bytes actually written. If NULL, write is still
 * performed. Should unchanged if write failed.
 * \param context Context provided in callback register procedure.
 *
 * \return Should return \ref kSpAPINoError in case of successful socket write.
 * Should return \ref kSpAPIGenericError in case of socket write has failed.
 * Should return \ref kSpAPINotSupported if the client has no sendto.
 * \ref SpCallbackSocketError can be called to get underlying OS error.
 *
 * \note The application should not block or call other API functions in the callback.
 * This callback should mirror the behavior of writeto for connected/non-connected sockets.
 */
typedef enum SpAPIReturnCode (*SpCallbackSocketWriteTo)(struct SpSocketHandle *socket,
                                                        const void *data, int data_size,
                                                        const void *addr, int *bytes_written,
                                                        void *context);

/**
 * \brief This callback if set by client is called by eSDK to get underlying OS error code.
 *
 * \param socket Socket instance for which error code is requested.
 * \param context Context provided in callback register procedure.
 *
 * \return Should return underlying OS error or 0 if no error occurred.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef int (*SpCallbackSocketError)(struct SpSocketHandle *socket, void *context);

/**
 * \brief This callback if set by client is called by eSDK to figure out if socket is readable.
 *
 * \param socket Socket instance to perform action with.
 * \param context Context provided in callback register procedure.
 *
 * \return 0 - not readable, 1 - readable.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef int (*SpCallbackSocketReadable)(struct SpSocketHandle *socket, void *context);

/**
 * \brief This callback if set by client is called by eSDK to figure out if socket is writable.
 *
 * \param socket Socket instance to perform action with.
 * \param context Context provided in callback register procedure.
 *
 * \return 0 - not writable, 1 - writable.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef int (*SpCallbackSocketWriteable)(struct SpSocketHandle *socket, void *context);

/**
 * \brief This callback if set by client is called by eSDK to get local interface addresses.
 * If the clent can't get local interfaces implementation should fill provided *num_addrs with 0.
 *
 * \param addrs Pointer to array of \ref SpSockaddr to store the data. Data in network byte order.
 * \param num_addrs Array size on input, number of items stored in the array on output.
 * \param context Context provided in callback register procedure.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef void (*SpCallbackLocalAddresses)(struct SpSockaddr *addrs, int *num_addrs, void *context);

/**
 * \brief This callback if set by client is called by eSDK to provide platform
 * defined representation of address that can be used in \ref SpCallbackSocketReadFrom
 * and \ref SpCallbackSocketWriteTo. Client responsible for providing returned pointer lifetime.
 *
 * \param addr Address to convert. Network byte order.
 * \param context Context provided in callback register procedure.
 *
 * \return Platform defined address representation to be used in ReadFrom/WriteTo or NULL.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef const void *(*SpCallbackSocketAddress)(const struct SpSockaddr *addr, void *context);

/**
 * \brief This callback if set by client is called by eSDK to pump network layer.
 *
 * \param max_wait_ms Maximum time to wait in one pump call
 * \param context Context provided in callback register procedure.
 *
 * \return Should return \ref kSpAPINoError in case of successful socket write.
 * Should return \ref kSpAPIGenericError in case of socket write has failed.
 *
 * \note The application should not block or call other API functions in the callback.
 */
typedef enum SpAPIReturnCode (*SpCallbackPump)(unsigned max_wait_ms, void *context);

/**
 * \brief Callbacks to be registered with SpRegisterSocketHALCallbacks()
 *
 * Any of the pointers may be NULL.
 *
 * See the documentation of the callback typedefs for information about the
 * individual callbacks.
 */
struct SpSocketHALCallbacks {
    /** \brief Callback to create socket instance */
    SpCallbackSocketCreate socket_create;
    /** \brief Callback to set options on created socket */
    SpCallbackSocketSetOption socket_set_option;
    /** \brief Callback to close the socket */
    SpCallbackSocketClose socket_close;
    /** \brief Callback to bind to socket */
    SpCallbackSocketBind socket_bind;
    /** \brief Callback to start listening the socket */
    SpCallbackSocketListen socket_listen;
    /** \brief Callback to connect to socket */
    SpCallbackSocketConnect socket_connect;
    /** \brief Callback to accept connection on socket */
    SpCallbackSocketAccept socket_accept;
    /** \brief Callback to read data from socket */
    SpCallbackSocketRead socket_read;
    /** \brief Callback to write data to socket */
    SpCallbackSocketWrite socket_write;
    /** \brief Callback to read data from socket pointed by address */
    SpCallbackSocketReadFrom socket_read_from;
    /** \brief Callback to write data to socket pointed by address */
    SpCallbackSocketWriteTo socket_write_to;
    /** \brief Callback to get OS error on socket */
    SpCallbackSocketError socket_error;
    /** \brief Callback to get readable state on socket */
    SpCallbackSocketReadable socket_readable;
    /** \brief Callback to get writable state on socket */
    SpCallbackSocketWriteable socket_writable;
    /** \brief Callback to get local interface addresses */
    SpCallbackLocalAddresses local_addresses;
    /** \brief Callback to platform address representation */
    SpCallbackSocketAddress socket_address;
    /** \brief Callback to pump network layer */
    SpCallbackPump on_pump;
};

/**
 * \brief Register socket HAL-related callbacks. To remove callbacks call
 * SpRegisterSocketHALCallbacks
 * with SpSocketHALCallbacks initialized to zeros.
 *
 * \param[in] callbacks Structure with pointers to individual callback functions.
 *                      Either all pointers are NULL or all are valid.
 * \param[in] context Application-defined pointer that will be passed unchanged as
 *                   the \a context argument to the callbacks.
 *
 * \return Returns an error code
 *
 * \note A call to this function has to be performed before \ref SpInit is
 * called. Calling this function when eSDK is initialized will fail with
 * kSpErrorAlreadyInitialized.
 */
SP_EMB_PUBLIC SpError SpRegisterSocketHALCallbacks(struct SpSocketHALCallbacks *callbacks,
                                                   void *context);

/**
 * \brief Get eSDK's default socket callbacks
 *
 * \param[in] callbacks \ref SpSocketHALCallbacks struct to be populated with pointers to callback
 * functions
 * \param[in] context the default context that is being used.
 *
 * \return Returns an error code.
 *
 * \note These callbacks are made available so they may be wrapped by custom socket
 * functions provided with \ref SpRegisterSocketHALCallbacks.
 */
SP_EMB_PUBLIC SpError SpGetDefaultSocketHALCallbacks(struct SpSocketHALCallbacks *callbacks,
                                                     void **context);

/**
 * @}
 */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _SPOTIFY_EMBEDDED_HAL_H */
