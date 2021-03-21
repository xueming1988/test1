#ifndef ALEXA_COMMS_LIB_STRUCTS_H_
#define ALEXA_COMMS_LIB_STRUCTS_H_

#include "SipUserAgentEventObserverInterface.h"
#include "SipUserAgentFocusManagerObserverInterface.h"
#include "SipUserAgentStateObserverInterface.h"
#include "LogObserverInterface.h"
#include "SipUserAgentNativeCallObserverInterface.h"


typedef struct AudioDevice {
    const char* driverName;
    const char* deviceName;
    float audioGain;
    int latencyInMilliseconds;
} AudioDevice;

typedef struct AudioDevices {
    AudioDevice outputDevice;
    AudioDevice inputDevice;
} AudioDevices;

typedef struct AudioQualityParameters {
    int echoCancellationTailLengthInMilliseconds;
    int opusBitrate;
} AudioQualityParameters;

typedef struct SipUserAgentStateContext {
    const char* interface;
    const char* name;
    const char* payload;
} SipUserAgentStateContext;

typedef struct MetricsConfigParameters {
    const char* certificatesDirectoryPath;
    const char* deviceTypeId;
    const char* hostAddress;
} MetricsConfigParameters;

typedef struct SipUserAgentInterfaceCreateParameters {
    AudioDevices audioDevices;
    AudioQualityParameters audioQualityParameters;
    SipUserAgentEventObserverInterface sipUserAgentEventObserverInterface;
    SipUserAgentStateObserverInterface sipUserAgentStateObserverInterface;
    SipUserAgentFocusManagerObserverInterface sipUserAgentFocusManagerObserverInterface;
} SipUserAgentInterfaceCreateParameters;

typedef struct SipUserAgentInterfaceCreateParametersV2 {
    AudioDevices audioDevices;
    AudioQualityParameters audioQualityParameters;
    SipUserAgentEventObserverInterface sipUserAgentEventObserverInterface;
    SipUserAgentStateObserverInterface sipUserAgentStateObserverInterface;
    SipUserAgentFocusManagerObserverInterface sipUserAgentFocusManagerObserverInterface;

    const char* tlsCertificateFilePath;                                                   // path to file containing certificates to use for connecting to SIP signalling server [required]
    MetricsConfigParameters metricsConfigParameters;
    SipUserAgentNativeCallObserverInterface sipUserAgentNativeCallObserverInterface;
    LogObserverInterface logObserverInterface;
} SipUserAgentInterfaceCreateParametersV2;

#endif // ALEXA_COMMS_LIB_STRUCTS_H_
