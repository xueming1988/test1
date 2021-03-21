#ifndef SIP_USER_AGENT_NATIVE_CALL_OBSERVER_INTERFACE_H_
#define SIP_USER_AGENT_NATIVE_CALL_OBSERVER_INTERFACE_H_

#include "AlexaCommsErrorCode.h"

#ifdef __cplusplus
extern "C" {
#endif

struct NativeCallContext;

typedef struct SipUserAgentNativeCallObserverInterface {
    // Notify clients to make an outgoing native call when using voice
    //
    // This callback function will be called when user tries to make an outgoing
    // native call.  e.g., "Alexa, call 4252408888" or "Alexa, call Cheng's mobile"
    //
    // The clients should initiate the dial of the native call and should not
    // block this function
    AlexaCommsErrorCode (*onNativeCallDial)(const char* phoneNumber);

    // Notify clients to pick up an incoming native call when using voice
    //
    // This callback function will be called when user tries to pick up the incoming
    // native call.  e.g., "Alexa, pick up" or "Alexa answer"
    //
    // The clients should accept the native call and should not block this function.
    // SipUserAgentInterface_notifyNativeCallActivated() should be called when the
    // native call becomes active
    //
    AlexaCommsErrorCode (*onNativeCallAccept)();

    // Notify clients to reject an incoming native call, cancel an outgoing native
    // call or hang up an active native call when using voice
    //
    // This callback function will be called when
    // * user tries to reject the incoming native call.  e.g., "Alexa, reject"
    //
    // This callback function will be called when
    // * user tries to cancel the outgoing native call.  e.g., "Alexa, cancel"
    //
    // This callback function will be called when
    // * user tries to hang up the active native call.  e.g., "Alexa, hang up"
    //
    // The clients should reject, cancel or hang up the native call accordingly and
    // should not block this function.
    // SipUserAgentInterface_notifyNativeCallTerminated() should be called when the
    // native call is terminated
    //
    AlexaCommsErrorCode (*onNativeCallHangup)();

    // Notify clients to send DTMF signal during a native call when using voice
    //
    // This callback function will be called when user tries to press digital button
    // using voice during a native call.  e.g., "Alexa, press 1" or "Alexa, press 123"
    //
    // The clients should send DTMF signal during the native call and should not
    // block this function.
    // SipUserAgentInterface_notifyNativeCallResult() should be called with event type
    // EVENTTYPE_SENDDTMF whether DTMF signal is sent successfully or unsuccessfully.
    //
    AlexaCommsErrorCode (*onNativeCallSendDTMF)(const char* signal, int durationInMillis);

    // Request the call context from clients
    //
    // This callback function will be called in SipUserAgentInterface_create() and
    // SipUserAgentInterface_getSipUserAgentContext()
    //
    AlexaCommsErrorCode (*onNativeCallContextRequest)(NativeCallContext* outCallContext);

#ifdef __cplusplus
    bool operator==(const struct SipUserAgentNativeCallObserverInterface& other) const {
        return onNativeCallDial == other.onNativeCallDial &&
               onNativeCallAccept == other.onNativeCallAccept &&
               onNativeCallHangup == other.onNativeCallHangup &&
               onNativeCallSendDTMF == other.onNativeCallSendDTMF &&
               onNativeCallContextRequest == other.onNativeCallContextRequest;
    }

    bool isValid() const {
        return onNativeCallDial != nullptr &&
               onNativeCallAccept != nullptr &&
               onNativeCallHangup != nullptr &&
               onNativeCallSendDTMF != nullptr &&
               onNativeCallContextRequest != nullptr;
    }
#endif
} SipUserAgentNativeCallObserverInterface;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SIP_USER_AGENT_NATIVE_CALL_OBSERVER_INTERFACE_H_
