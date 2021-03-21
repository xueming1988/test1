#ifndef SIP_USER_AGENT_FOCUS_MANAGER_OBSERVER_INTERFACE_H_
#define SIP_USER_AGENT_FOCUS_MANAGER_OBSERVER_INTERFACE_H_

#include "AlexaCommsLibEnums.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SipUserAgentFocusManagerObserverInterface {
    /**
     * Acquire channel focus for SipUserAgent.
     */
    void (*onAcquireCommunicationsChannelFocus)(const CallType callType);

    /**
     * Release channel focus for SipUserAgent.
     */
    void (*onReleaseCommunicationsChannelFocus)(const CallType callType);

#ifdef __cplusplus
    bool operator==(const struct SipUserAgentFocusManagerObserverInterface& other) const {
        return onAcquireCommunicationsChannelFocus == other.onAcquireCommunicationsChannelFocus
               && onReleaseCommunicationsChannelFocus == other.onReleaseCommunicationsChannelFocus;
    }

    bool isValid() const {
        return onAcquireCommunicationsChannelFocus != nullptr && onReleaseCommunicationsChannelFocus != nullptr;
    }
#endif
} SipUserAgentFocusManagerObserverInterface;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SIP_USER_AGENT_FOCUS_MANAGER_OBSERVER_INTERFACE_H_
