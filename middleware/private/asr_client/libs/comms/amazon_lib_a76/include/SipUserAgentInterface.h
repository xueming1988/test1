#ifndef SIP_USER_AGENT_INTERFACE_H_
#define SIP_USER_AGENT_INTERFACE_H_

#include "AlexaCommsAPI.h"

#include "AlexaCommsLibStructs.h"
#include "SipUserAgentEventObserverInterface.h"
#include "SipUserAgentFocusManagerObserverInterface.h"
#include "SipUserAgentStateObserverInterface.h"
#include "LogObserverInterface.h"

#ifdef __cplusplus
extern "C" {
#endif

// deprecated because of incomplete TLS certificate and native call support
ALEXACOMMSLIB_API ALEXACOMMSLIB_DEPRECATED bool SipUserAgentInterface_create(SipUserAgentInterfaceCreateParameters params);

ALEXACOMMSLIB_API bool SipUserAgentInterface_create_v2(SipUserAgentInterfaceCreateParametersV2 params);

ALEXACOMMSLIB_API void SipUserAgentInterface_delete();

ALEXACOMMSLIB_API bool SipUserAgentInterface_isInitialized();

ALEXACOMMSLIB_API void SipUserAgentInterface_addSipUserAgentEventObserver(SipUserAgentEventObserverInterface observer);

ALEXACOMMSLIB_API void SipUserAgentInterface_removeSipUserAgentEventObserver(SipUserAgentEventObserverInterface observer);

ALEXACOMMSLIB_API void SipUserAgentInterface_addSipUserAgentStateObserver(SipUserAgentStateObserverInterface observer);

ALEXACOMMSLIB_API void SipUserAgentInterface_removeSipUserAgentStateObserver(SipUserAgentStateObserverInterface observer);

ALEXACOMMSLIB_API void SipUserAgentInterface_addSipUserAgentFocusManagerObserver(SipUserAgentFocusManagerObserverInterface observer);

ALEXACOMMSLIB_API void SipUserAgentInterface_removeSipUserAgentFocusManagerObserver(SipUserAgentFocusManagerObserverInterface observer);

ALEXACOMMSLIB_API void SipUserAgentInterface_addSipUserAgentNativeCallObserver(SipUserAgentNativeCallObserverInterface observer);

ALEXACOMMSLIB_API void SipUserAgentInterface_removeSipUserAgentNativeCallObserver(SipUserAgentNativeCallObserverInterface observer);

ALEXACOMMSLIB_API void SipUserAgentInterface_addLogObserver(LogObserverInterface observer);

ALEXACOMMSLIB_API void SipUserAgentInterface_addLogObserverWithLogLevel(LogObserverInterface observer, Level logLevel);

ALEXACOMMSLIB_API void SipUserAgentInterface_removeLogObserver(LogObserverInterface observer);

ALEXACOMMSLIB_API void SipUserAgentInterface_setLogLevel(LogObserverInterface observer, int logLevel);

ALEXACOMMSLIB_API bool SipUserAgentInterface_handleDirective(const char* name, const char* payload);

ALEXACOMMSLIB_API SipUserAgentStateContext SipUserAgentInterface_getSipUserAgentStateContext();

ALEXACOMMSLIB_API void SipUserAgentInterface_handleNetworkConnectivityEvent(bool connected);

ALEXACOMMSLIB_API void SipUserAgentInterface_handleAvsConnectivityEvent(bool connected);

ALEXACOMMSLIB_API void SipUserAgentInterface_acceptCall();

ALEXACOMMSLIB_API void SipUserAgentInterface_stopCall();

ALEXACOMMSLIB_API void SipUserAgentInterface_muteOther();

ALEXACOMMSLIB_API void SipUserAgentInterface_unmuteOther();

ALEXACOMMSLIB_API void SipUserAgentInterface_muteSelf();

ALEXACOMMSLIB_API void SipUserAgentInterface_unmuteSelf();

ALEXACOMMSLIB_API void SipUserAgentInterface_enableNative();

ALEXACOMMSLIB_API void SipUserAgentInterface_disableNative();

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SIP_USER_AGENT_INTERFACE_H_
