#ifndef ALEXA_COMMS_LIB_STRUCTS_H_
#define ALEXA_COMMS_LIB_STRUCTS_H_

#include "SipUserAgentEventObserverInterface.h"
#include "SipUserAgentFocusManagerObserverInterface.h"
#include "SipUserAgentStateObserverInterface.h"
#include "LogObserverInterface.h"
#include "MetricObserverInterface.h"
#include "SipUserAgentNativeCallObserverInterface.h"

typedef struct AudioDevice {
    const char* driverName;
    const char* deviceName;
} AudioDevice;

typedef struct AudioDevices {
    AudioDevice outputDevice;
    AudioDevice inputDevice;
} AudioDevices;

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

typedef struct DeviceClientMetricConfigParameters {
	const char* deviceTypeId;
	const char* region;
	const char* storagePath;
} DeviceClientMetricConfigParameters;

// The max size of a phone number
#define PHONE_NUMBER_MAX_SIZE 128

// Context of a native call
typedef struct NativeCallContext
{
    // Phone number
    char phoneNumber[PHONE_NUMBER_MAX_SIZE];

    // State of the native call
    NativeCallState nativeCallState;

    // Direction of the native call
    NativeCallDirection nativeCallDirection;
} NativeCallContext;

typedef struct SipUserAgentInterfaceCreateParameters {
    AudioDevices audioDevices;
    SipUserAgentEventObserverInterface sipUserAgentEventObserverInterface;
    SipUserAgentStateObserverInterface sipUserAgentStateObserverInterface;
    SipUserAgentFocusManagerObserverInterface sipUserAgentFocusManagerObserverInterface;

    const char* ipAddress;
    const char* tlsCertificateFilePath;
    MetricsConfigParameters metricsConfigParameters;
    DeviceClientMetricConfigParameters dcmConfigParameters;
    SipUserAgentNativeCallObserverInterface sipUserAgentNativeCallObserverInterface;
    LogObserverInterface logObserverInterface;
    MetricObserverInterface metricObserverInterface;
} SipUserAgentInterfaceCreateParameters;

#endif // ALEXA_COMMS_LIB_STRUCTS_H_
