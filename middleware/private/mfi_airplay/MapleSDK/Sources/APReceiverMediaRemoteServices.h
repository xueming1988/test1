/*
	Copyright (C) Apple Inc. All Rights Reserved.
 */

#ifndef APRECEIVERMEDIAREMOTESERVICES_H
#define APRECEIVERMEDIAREMOTESERVICES_H

#ifdef __cplusplus
extern "C" {
#endif

#define kAPReceiverMediaRemoteServicesName CFSTR("com.apple.airplay.receiver.mediaremote.services")

#define kAPReceiverMediaRemoteServicesEvent_DidReceiveMediaRemoteData CFSTR("didReceiveMediaRemoteData") // received data from sender; Session -> XPC Server -> XPC client -> MediaRemote client
#define kAPReceiverMediaRemoteServicesEvent_DidCloseCommunicationChannel CFSTR("didCloseCommunicationChannel")

#define kAPReceiverMediaRemoteServicesCommand_Connect CFSTR("connect") // ping/initial connection from XPC client
#define kAPReceiverMediaRemoteServicesCommand_SendData CFSTR("sendData") // send data from receiver to sender; XPC client -> XPC server -> Session -> Sender
#define kAPReceiverMediaRemoteServicesCommand_CopyProperty CFSTR("copyProperty") // copy a property from sender to receiver
#define kAPReceiverMediaRemoteServicesCommandParam_ObjectID CFSTR("objectID")
#define kAPReceiverMediaRemoteServicesCommandParam_Data CFSTR("data")
#define kAPReceiverMediaRemoteServicesCommandParam_ReqProcRef CFSTR("reqProcRef")

#define kAPReceiverMediaRemoteServicesCommandParam_PropertyKey CFSTR("propertyKey")
#define kAPReceiverMediaRemoteServicesCommandParam_PropertyValue CFSTR("propertyValue")

#define kAPReceiverMediaRemoteServicesProperty_AirPlaySecuritySetting CFSTR("AirPlaySecuritySetting") // CFDictionary
#define kAPReceiverMediaRemoteServicesPropertyAirPlaySecuritySetting_Password CFSTR("password")
#define kAPReceiverMediaRemoteServicesPropertyAirPlaySecuritySetting_SecurityMode CFSTR("securityMode")

typedef enum {
    kAPReceiverMediaRemoteServicesAirPlaySecuritySettting_SecurityNoneForInfra = 0,
    kAPReceiverMediaRemoteServicesAirPlaySecuritySettting_SecurityPINFirstTime = 1,
    kAPReceiverMediaRemoteServicesAirPlaySecuritySettting_SecurityPassword = 2,
    kAPReceiverMediaRemoteServicesAirPlaySecuritySettting_SecurityPINEveryTime = 3,
} APReceiverMediaRemoteServicesAirPlaySecuritySettting_SecurityMode;

#ifdef __cplusplus
}
#endif

#endif // APRECEIVERMEDIAREMOTESERVICES_H
