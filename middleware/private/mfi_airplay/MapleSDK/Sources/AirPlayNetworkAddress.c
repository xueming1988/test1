/*
 Copyright (C) 2014-2019 Apple Inc. All Rights Reserved.
 */

#include "AirPlayNetworkAddress.h"

#include "CFUtils.h"
#include "StringUtils.h"

#include <ifaddrs.h>

#include CF_HEADER
#include CF_RUNTIME_HEADER

#if 0
#pragma mark == Constants ==
#endif

//===========================================================================================================================
//    Constants
//===========================================================================================================================

enum { kIPPortCStringLength = 128 };

#if 0
#pragma mark == Structures ==
#endif

//===========================================================================================================================
//    Structures
//===========================================================================================================================

struct OpaqueAirPlayNetworkAddress {
    CFRuntimeBase base; // CF type info. Must be first.
    sockaddr_ip socketAddr;
    CFStringRef interface;
};

#if 0
#pragma mark == Prototypes ==
#endif

//===========================================================================================================================
//    Prototypes
//===========================================================================================================================

static void networkAddress_getTypeID(void* inContext);
static void networkAddress_Finalize(CFTypeRef inObj);
static Boolean networkAddress_Equal(CFTypeRef inObj1, CFTypeRef inObj2);
static CFStringRef networkAddress_CopyDebugDescription(CFTypeRef inObj);

#if 0
#pragma mark == CFType Hookup ==
#endif

//===========================================================================================================================
//    CFType Hookup
//===========================================================================================================================

static const CFRuntimeClass kAPSNetworkAddressClass = {
    0, // version
    "APSNetworkAddress", // className
    NULL, // init
    NULL, // copy
    networkAddress_Finalize, // finalize
    networkAddress_Equal, // equal
    NULL, // hash  -- NULL means pointer hash.
    NULL, // copyFormattingDesc
    networkAddress_CopyDebugDescription, // copyDebugDesc
    NULL, // reclaim
    NULL // refcount
};

#if 0
#pragma mark == Types ==
#endif

//===========================================================================================================================
//    Types
//===========================================================================================================================

CFTypeID AirPlayNetworkAddressGetTypeID(void)
{
    static dispatch_once_t sInitOnce = 0;
    static CFTypeID sTypeID = _kCFRuntimeNotATypeID;

    dispatch_once_f(&sInitOnce, &sTypeID, networkAddress_getTypeID);
    return (sTypeID);
}

static void networkAddress_getTypeID(void* inContext)
{
    CFTypeID* typeID = (CFTypeID*)inContext;

    *typeID = _CFRuntimeRegisterClass(&kAPSNetworkAddressClass);
    check(*typeID != _kCFRuntimeNotATypeID);
}

#if 0
#pragma mark == Creation ==
#endif

//===========================================================================================================================
//    Creation
//===========================================================================================================================

OSStatus AirPlayNetworkAddressCreateWithSocketAddr(CFAllocatorRef inAllocator, sockaddr_ip inSockAddr, AirPlayNetworkAddressRef* outTransportNetworkAddress)
{
    OSStatus err = kNoErr;
    AirPlayNetworkAddressRef networkAddress = NULL;
    size_t extraLen = 0;

    extraLen = sizeof(*networkAddress) - sizeof(networkAddress->base);
    networkAddress = (AirPlayNetworkAddressRef)_CFRuntimeCreateInstance(inAllocator, AirPlayNetworkAddressGetTypeID(), (CFIndex)extraLen, NULL);
    require_action(networkAddress, exit, err = kNoMemoryErr);

    memset(((uint8_t*)networkAddress) + sizeof(networkAddress->base), 0, extraLen);

    networkAddress->socketAddr = inSockAddr;

    *outTransportNetworkAddress = networkAddress;
    networkAddress = NULL;

exit:
    CFReleaseNullSafe(networkAddress);

    return (err);
}

OSStatus AirPlayNetworkAddressCreateWithString(CFAllocatorRef inAllocator, CFStringRef inIPPort, CFStringRef inInterface, AirPlayNetworkAddressRef* outTransportNetworkAddress)
{
    OSStatus err = kNoErr;
    char IPPort[kIPPortCStringLength] = { 0 };
    sockaddr_ip sockAddr;
    CFStringRef scopedIPAddr = NULL;
    char ipAddrStr[kIPPortCStringLength];

    Boolean success = CFStringGetCString(inIPPort, ipAddrStr, sizeof(ipAddrStr), kCFStringEncodingUTF8);
    Boolean isV6LinkLocal = success && !stricmp_prefix(ipAddrStr, "fe80:");
    isV6LinkLocal = false; //rb force

    // ipv6 link local addresses need the interface (scope) they are associated with
    if (inInterface && isV6LinkLocal) {
        scopedIPAddr = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@%%%@"), inIPPort, inInterface);
    } else {
        scopedIPAddr = (CFStringRef)CFRetain(inIPPort);
    }

    CFStringGetCString(scopedIPAddr, IPPort, (CFIndex)kIPPortCStringLength, kCFStringEncodingUTF8);
    err = StringToSockAddr(IPPort, &sockAddr, sizeof(sockAddr), NULL);
    require_noerr(err, exit);

    err = AirPlayNetworkAddressCreateWithSocketAddr(inAllocator, sockAddr, outTransportNetworkAddress);
    require_noerr(err, exit);

    CFRetainNullSafe(inInterface);
    (*outTransportNetworkAddress)->interface = inInterface;

exit:
    CFReleaseNullSafe(scopedIPAddr);

    return (err);
}

static void networkAddress_Finalize(CFTypeRef inObj)
{
    AirPlayNetworkAddressRef networkAddress = (AirPlayNetworkAddressRef)inObj;
    ForgetCF(&networkAddress->interface);

    memset(&networkAddress->socketAddr, 0, sizeof(sockaddr_ip));
}

static Boolean networkAddress_Equal(CFTypeRef inObj1, CFTypeRef inObj2)
{
    sockaddr_ip sockAddr1;
    sockaddr_ip sockAddr2;

    if (CFGetTypeID(inObj2) != AirPlayNetworkAddressGetTypeID() || CFGetTypeID(inObj2) != AirPlayNetworkAddressGetTypeID()) {
        return (false);
    }

    sockAddr1 = AirPlayNetworkAddressGetSocketAddr((AirPlayNetworkAddressRef)inObj1);
    sockAddr2 = AirPlayNetworkAddressGetSocketAddr((AirPlayNetworkAddressRef)inObj2);

    if (sockAddr1.sa.sa_family != sockAddr2.sa.sa_family) {
        return (false);
    }

    if (SockAddrGetPort(&sockAddr1.sa) != SockAddrGetPort(&sockAddr2.sa)) {
        return (false);
    }

    return (SockAddrCompareAddr(&sockAddr1.sa, &sockAddr2.sa) == 0);
}

static CFStringRef networkAddress_CopyDebugDescription(CFTypeRef inObj)
{
    OSStatus err = kNoErr;
    char IPPortString[kIPPortCStringLength] = { 0 };
    AirPlayNetworkAddressRef networkAddress = (AirPlayNetworkAddressRef)inObj;
    CFMutableStringRef description = NULL;

    description = CFStringCreateMutable(kCFAllocatorDefault, 0);
    require(description, exit);

    err = SockAddrToString(&networkAddress->socketAddr, kSockAddrStringFlagsNone, IPPortString);
    require_noerr(err, exit);

    CFStringAppendF(description, "<APSNetworkAddress %p '%s'>", inObj, IPPortString);

exit:
    return (description);
}

#if 0
#pragma mark == API ==
#endif

//===========================================================================================================================
//    API
//===========================================================================================================================

CFStringRef AirPlayNetworkAddressGetInterface(AirPlayNetworkAddressRef inTransportNetworkAddress)
{
    return inTransportNetworkAddress->interface;
}

sockaddr_ip AirPlayNetworkAddressGetSocketAddr(AirPlayNetworkAddressRef inTransportNetworkAddress)
{
    return inTransportNetworkAddress->socketAddr;
}

uint16_t AirPlayNetworkAddressGetPort(AirPlayNetworkAddressRef inTransportNetworkAddress)
{
    return SockAddrGetPort(&inTransportNetworkAddress->socketAddr);
}

void AirPlayNetworkAddressSetPort(AirPlayNetworkAddressRef inTransportNetworkAddress, uint16_t inPort)
{
    SockAddrSetPort(&inTransportNetworkAddress->socketAddr, inPort);
}

OSStatus AirPlayNetworkAddressCopyStringRepresentation(AirPlayNetworkAddressRef inTransportNetworkAddress, CFStringRef* outStringRepresentation)
{
    OSStatus err = kNoErr;
    char IPPortString[kIPPortCStringLength] = { 0 };

    require(outStringRepresentation, exit);

    err = SockAddrToString(&inTransportNetworkAddress->socketAddr, kSockAddrStringFlagsNoScope, IPPortString);
    require_noerr(err, exit);

    *outStringRepresentation = CFStringCreateWithCString(kCFAllocatorDefault, IPPortString, kCFStringEncodingUTF8);

exit:
    return err;
}

OSStatus AirPlayNetworkAddressCreateForInterface(CFAllocatorRef inAllocator, const char* inInterface, Boolean inIpv4Only, CFMutableArrayRef* outAddresses)
{
    struct ifaddrs* allocatedAddrs = NULL;
    struct ifaddrs* addrs = NULL;
    CFMutableArrayRef addresses = NULL;
    OSStatus err = kNoErr;

    check(outAddresses);

    addresses = CFArrayCreateMutable(inAllocator, 0, &kCFTypeArrayCallBacks);
    check(addresses);

    require(inInterface, exit);

    err = getifaddrs(&allocatedAddrs);
    require_noerr(err, exit);

    for (addrs = allocatedAddrs; addrs != NULL; addrs = addrs->ifa_next) {

        if (!(addrs->ifa_flags & IFF_UP))
            continue; // Skip inactive.
        if (!addrs->ifa_addr)
            continue; // Skip no addr.
        if (addrs->ifa_addr->sa_family != AF_INET && addrs->ifa_addr->sa_family != AF_INET6)
            continue; // Has to be ipv4 or ipv6.
        if (inIpv4Only && addrs->ifa_addr->sa_family != AF_INET)
            continue; // Unless told ipv4 only

        if (addrs->ifa_name && strcmp(addrs->ifa_name, inInterface) == 0) {
            AirPlayNetworkAddressRef address = NULL;
            err = AirPlayNetworkAddressCreateWithSocketAddr(inAllocator, *(sockaddr_ip*)addrs->ifa_addr, &address);
            if (!err) {
                CFArrayAppendValue(addresses, address);
                CFForget(&address);
            }
        }
    }

    freeifaddrs(allocatedAddrs);

    *outAddresses = addresses;
    CFRetain(*outAddresses);

exit:
    CFReleaseNullSafe(addresses);

    return err;
}
