#ifndef ALEXA_COMMS_LIB_STRUCTS_H_
#define ALEXA_COMMS_LIB_STRUCTS_H_
#include "SipUserAgentEventObserverInterface.h"
#include "SipUserAgentFocusManagerObserverInterface.h"
#include "SipUserAgentStateObserverInterface.h"

 typedef struct AudioDevice
 {
	 char* driverName;
	 char* deviceName;
	 float audioGain;
	 int   latencyInMilliseconds;
 }AudioDevice;

 typedef struct AudioDevices
 {
	 AudioDevice outputDevice;
	 AudioDevice inputDevice;
 }AudioDevices;

 typedef struct AudioQualityParameters
 {
	 int echoCancellationTailLengthInMilliseconds;
	 int opusBitrate;
 }AudioQualityParameters;

 typedef struct SipUserAgentStateContext
 {
	 char* interface;
	 char* name;
	 char* payload;
 }SipUserAgentStateContext;

 typedef struct SipUserAgentInterfaceCreateParameters
 {
	 AudioDevices audioDevices;
	 AudioQualityParameters audioQualityParameters;
	 SipUserAgentEventObserverInterface sipUserAgentEventObserverInterface;
	 SipUserAgentStateObserverInterface sipUserAgentStateObserverInterface;
	 SipUserAgentFocusManagerObserverInterface sipUserAgentFocusManagerObserverInterface;
 }SipUserAgentInterfaceCreateParameters;

#endif // ALEXA_COMMS_LIB_STRUCTS_H_
