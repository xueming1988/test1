#ifndef SIP_USER_AGENT_STATE_OBSERVER_INTERFACE_H_
#define SIP_USER_AGENT_STATE_OBSERVER_INTERFACE_H_

#include "AlexaCommsLibEnums.h"

#ifdef __cplusplus
#include <string>

extern "C" {
#endif

enum SipUserAgentState {
    INVALID                   = 0,
    UNREGISTERED              = 1,
    IDLE                      = 2,
    TRYING                    = 3,
    OUTBOUND_LOCAL_RINGING    = 4,
    OUTBOUND_PROVIDER_RINGING = 5,
    ACTIVE                    = 6,
    INVITED                   = 7,
    INBOUND_RINGING           = 8,
    STOPPING                  = 9
};

typedef struct CallDisplayInfo {
    const char* displayName;
    const char* endpointLabel;
    const char* inboundCalleeName;
    const char* callProviderType;
} CallDisplayInfo;

typedef struct SipUserAgentStateInfo {
	CallType callType;
    SipUserAgentState previousSipUserAgentState;
    SipUserAgentState currentSipUserAgentState;
    CallDisplayInfo callDisplayInfo;
    const char* inboundRingtoneUrl;
    const char* outboundRingbackUrl;
} SipUserAgentStateInfo;

typedef struct SipUserAgentStateObserverInterface {
    /**
    * Observer interface for listening to sipUserAgent state changes to handle User experiences like ringtones and lights.
    */
    void (*onStateUpdated)(SipUserAgentStateInfo stateInfo);

#ifdef __cplusplus
    bool operator==(const struct SipUserAgentStateObserverInterface& other) const {
        return onStateUpdated == other.onStateUpdated;
    }

    bool isValid() const {
        return onStateUpdated != nullptr;
    }
#endif
} SipUserAgentStateObserverInterface;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SIP_USER_AGENT_STATE_OBSERVER_INTERFACE_H_
