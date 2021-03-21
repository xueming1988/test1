/*
 Copyright (C) 2014-2019 Apple Inc. All Rights Reserved.
 */

#ifndef AirPlayNetworkAddress_h_
#define AirPlayNetworkAddress_h_

#include "CommonServices.h"
#include "NetUtils.h"

#ifdef __cplusplus
extern "C" {
#endif

#if 0
#pragma mark -
#pragma mark == Types ==
#endif

typedef struct OpaqueAirPlayNetworkAddress* AirPlayNetworkAddressRef; // CFType

#if 0
#pragma mark -
#pragma mark == API ==
#endif

CFTypeID AirPlayNetworkAddressGetTypeID(void);

OSStatus AirPlayNetworkAddressCreateWithSocketAddr(CFAllocatorRef inAllocator, sockaddr_ip inSockAddr, AirPlayNetworkAddressRef* outTransportNetworkAddress);

OSStatus AirPlayNetworkAddressCreateForInterface(CFAllocatorRef inAllocator, const char* inInterface, Boolean inIpv4Only, CFMutableArrayRef* outAddresses);

OSStatus AirPlayNetworkAddressCreateWithString(CFAllocatorRef inAllocator, CFStringRef inIPPort, CFStringRef inInterface, AirPlayNetworkAddressRef* outTransportNetworkAddress);

CFStringRef AirPlayNetworkAddressGetInterface(AirPlayNetworkAddressRef inTransportNetworkAddress);

sockaddr_ip AirPlayNetworkAddressGetSocketAddr(AirPlayNetworkAddressRef inTransportNetworkAddress);
uint16_t AirPlayNetworkAddressGetPort(AirPlayNetworkAddressRef inTransportNetworkAddress);
void AirPlayNetworkAddressSetPort(AirPlayNetworkAddressRef inTransportNetworkAddress, uint16_t inPort);

OSStatus AirPlayNetworkAddressCopyStringRepresentation(AirPlayNetworkAddressRef inTransportNetworkAddress, CFStringRef* outStringRepresentation);

#ifdef __cplusplus
}
#endif

#endif // AirPlayNetworkAddress_h_
