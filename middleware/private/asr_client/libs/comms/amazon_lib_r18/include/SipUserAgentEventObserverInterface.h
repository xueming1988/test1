#ifndef SIP_USER_AGENT_EVENT_OBSERVER_INTERFACE_H_
#define SIP_USER_AGENT_EVENT_OBSERVER_INTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SipUserAgentEventObserverInterface {
    /**
     * Send SipUserAgent events to cloud.
     */
    void (*onSendEvent)(const char *_interface, const char *_name, const char *_payload);

#ifdef __cplusplus
    bool operator==(const struct SipUserAgentEventObserverInterface &other) const
    {
        return onSendEvent == other.onSendEvent;
    }

    bool isValid() const
    {
        return onSendEvent != nullptr;
    }
#endif
} SipUserAgentEventObserverInterface;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SIP_USER_AGENT_EVENT_OBSERVER_INTERFACE_H_
