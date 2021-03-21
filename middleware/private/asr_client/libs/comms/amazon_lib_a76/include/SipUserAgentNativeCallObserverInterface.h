#ifndef SIP_USER_AGENT_NATIVE_CALL_OBSERVER_INTERFACE_H_
#define SIP_USER_AGENT_NATIVE_CALL_OBSERVER_INTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SipUserAgentNativeCallObserverInterface {
    /**
    * Observer interface for listening to native call handling.
    */
    void (*onNativeCall)(const char* phoneNumber);

#ifdef __cplusplus
    bool operator==(const struct SipUserAgentNativeCallObserverInterface& other) const {
        return onNativeCall == other.onNativeCall;
    }

    bool isValid() const {
        return onNativeCall != nullptr;
    }
#endif
} SipUserAgentNativeCallObserverInterface;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SIP_USER_AGENT_NATIVE_CALL_OBSERVER_INTERFACE_H_
