#ifndef SIP_USER_AGENT_NATIVE_CALL_OBSERVER_INTERFACE_H_
#define SIP_USER_AGENT_NATIVE_CALL_OBSERVER_INTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SipUserAgentNativeCallObserverInterface {
    /**
    * Observer interface for listening to native call started.
    */
    void (*onNativeCall)(const char *phoneNumber);

    /**
    * Observer interface for listening to native call hangup.
    */
    void (*onNativeCallHangup)();

    /**
    * Observer interface for listening to incoming native call accepted.
    */
    void (*onNativeCallAccept)();

#ifdef __cplusplus
    bool operator==(const struct SipUserAgentNativeCallObserverInterface &other) const
    {
        return onNativeCall == other.onNativeCall && onNativeCallHangup == other.onNativeCallHangup
               && onNativeCallAccept == other.onNativeCallAccept;
    }

    bool isValid() const
    {
        return onNativeCall != nullptr && onNativeCallHangup != nullptr && onNativeCallAccept != nullptr;
    }
#endif
} SipUserAgentNativeCallObserverInterface;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SIP_USER_AGENT_NATIVE_CALL_OBSERVER_INTERFACE_H_
