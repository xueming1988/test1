#ifndef ALEXA_COMMS_ERROR_CODE_H_
#define ALEXA_COMMS_ERROR_CODE_H_

typedef enum AlexaCommsErrorCode
{
    // Success return value - no error occurred
    AC_SUCCESS = 0,

    // A generic error. Not enough information for a specific error code
    AC_INTERNAL_ERROR = -1,

    // Not initialized
    AC_NOT_INITIALIZED = -2,

    // Already initialized
    AC_ALREADY_INITIALIZED = -3,

    // Invalid argument
    AC_INVALID_ARGUMENT = -4,

    // Failed to generate JSON
    AC_FAILED_TO_GENERATE_JSON = -5,

    // Not in a call
    AC_NOT_IN_CALL = -6,

    // Already in a call
    AC_ALREADY_IN_CALL = -7,

    // Failed to create a call object
    AC_FAILED_TO_CREATE_CALL = -8,

    // Invalid phone number
    AC_INVALID_PHONE_NUMBER = -9,

    // Invalid state
    AC_INVALID_STATE = -10,

    /// Error when actual sending of DTMF failed
    AC_DTMF_FAILED = -11

} AlexaCommsErrorCode;

#endif  // ALEXA_COMMS_ERROR_CODE_H_
