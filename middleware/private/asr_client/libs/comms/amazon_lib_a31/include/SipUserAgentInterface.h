#ifndef SIP_USER_AGENT_INTERFACE_H_
#define SIP_USER_AGENT_INTERFACE_H_

#include "SipUserAgentEventObserverInterface.h"
#include "SipUserAgentFocusManagerObserverInterface.h"
#include "SipUserAgentStateObserverInterface.h"
#include "AlexaCommsLibStructs.h"

#ifdef __cplusplus
extern "C" {
#endif

bool SipUserAgentInterface_create(SipUserAgentInterfaceCreateParameters sipUserAgentInterfaceCreateParameters);

void SipUserAgentInterface_delete();

bool SipUserAgentInterface_isInitialized();

void SipUserAgentInterface_addSipUserAgentEventObserver(SipUserAgentEventObserverInterface observer);

void SipUserAgentInterface_removeSipUserAgentEventObserver(SipUserAgentEventObserverInterface observer);

void SipUserAgentInterface_addSipUserAgentStateObserver(SipUserAgentStateObserverInterface observer);

void SipUserAgentInterface_removeSipUserAgentStateObserver(SipUserAgentStateObserverInterface observer);

void SipUserAgentInterface_addSipUserAgentFocusManagerObserver(SipUserAgentFocusManagerObserverInterface observer);

void SipUserAgentInterface_removeSipUserAgentFocusManagerObserver(SipUserAgentFocusManagerObserverInterface observer);

bool SipUserAgentInterface_handleDirective(const char* name, const char* payload);

SipUserAgentStateContext SipUserAgentInterface_getSipUserAgentStateContext();

void SipUserAgentInterface_handleNetworkConnectivityEvent(bool connected);

void SipUserAgentInterface_handleAvsConnectivityEvent(bool connected);

void SipUserAgentInterface_muteOther();

void SipUserAgentInterface_unmuteOther();

void SipUserAgentInterface_muteSelf();

void SipUserAgentInterface_unmuteSelf();

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SIP_USER_AGENT_INTERFACE_H_
