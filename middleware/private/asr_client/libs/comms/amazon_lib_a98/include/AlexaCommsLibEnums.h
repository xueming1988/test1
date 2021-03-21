#ifndef ALEXA_COMMS_LIB_ENUMS_H_
#define ALEXA_COMMS_LIB_ENUMS_H_

typedef enum CallType {
    CALLTYPE_NO_CALL    = 0,
    CALLTYPE_SIP        = 1,
    CALLTYPE_NATIVE     = 2
} CallType;

// State of a native call
// CALLSTATE_NO_CALL - there is no native call
// CALLSTATE_RINGING - there is a native call ringing and not accepted.  It's either
//                 an incoming call or an outgoing call
// CALLSTATE_ACTIVE - there is an active call.  It's either an incoming call or an
//                outgoing call
typedef enum NativeCallState
{
    CALLSTATE_NO_CALL,
    CALLSTATE_RINGING,
    CALLSTATE_ACTIVE
} NativeCallState;

// Direction of a native call
// CALLDIRECTION_UNKNOWN - the direction of the native call is unknown or there is no
//                     native call
// CALLDIRECTION_INCOMING - the native call is an incoming call
// CALLDIRECTION_OUTGOING - the native call is an outgoing call
typedef enum NativeCallDirection
{
    CALLDIRECTION_UNKNOWN,
    CALLDIRECTION_INCOMING,
    CALLDIRECTION_OUTGOING
} NativeCallDirection;

// EventType of a native call
//    EVENTTYPE_DIAL  -  clients make an outgoing native call
//    EVENTTYPE_ACCEPT  -  clients pick up an incoming native call
//    EVENTTYPE_HANGUP  -  clients reject an incoming native call,cancel a outgoing native
//                         call or hang up an active native call
//    EVENTTYPE_CONTEXTREQUEST  -  Request the call context from clients
//    EVENTTYPE_SENDDTMF  -  clients send DTMF signal during a native call
typedef enum NativeCallEventType
{
    EVENTTYPE_DIAL,
    EVENTTYPE_ACCEPT,
    EVENTTYPE_HANGUP,
    EVENTTYPE_CONTEXTREQUEST,
    EVENTTYPE_SENDDTMF
} NativeCallEventType;

typedef enum DTMFTone
{
    DTMF_ZERO,
    DTMF_ONE,
    DTMF_TWO,
    DTMF_THREE ,
    DTMF_FOUR,
    DTMF_FIVE,
    DTMF_SIX,
    DTMF_SEVEN,
    DTMF_EIGHT,
    DTMF_NINE,
    DTMF_STAR,
    DTMF_POUND
} DTMFTone;

#endif // ALEXA_COMMS_LIB_ENUMS_H_
