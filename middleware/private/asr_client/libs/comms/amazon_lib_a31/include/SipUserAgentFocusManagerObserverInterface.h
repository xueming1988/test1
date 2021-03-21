#ifndef SIP_USER_AGENT_FOCUS_MANAGER_OBSERVER_INTERFACE_H_
#define SIP_USER_AGENT_FOCUS_MANAGER_OBSERVER_INTERFACE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SipUserAgentFocusManagerObserverInterface
{
  /**
   * Acquire channel focus for SipUserAgent.
   */
  void (*onAcquireCommunicationsChannelFocus)();

  /**
   * Release channel focus for SipUserAgent.
   */
  void (*onReleaseCommunicationsChannelFocus)();

#ifdef __cplusplus
  bool operator==(const struct SipUserAgentFocusManagerObserverInterface & other) const { return onAcquireCommunicationsChannelFocus == other.onAcquireCommunicationsChannelFocus&&onReleaseCommunicationsChannelFocus==other.onReleaseCommunicationsChannelFocus; }
#endif

} SipUserAgentFocusManagerObserverInterface;

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SIP_USER_AGENT_FOCUS_MANAGER_OBSERVER_INTERFACE_H_
