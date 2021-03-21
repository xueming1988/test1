//
//  Copyright © 2018 Apple Inc. All rights reserved.
//

#ifndef __HomeIntegrationUtilities_h__
#define __HomeIntegrationUtilities_h__

#include "AirPlayCommon.h"
#include "AirPlayReceiverServer.h"
#include "PairingUtils.h"

OSStatus
AirPlayReceiverPairingDelegateCopyIdentity(
    Boolean inAllowCreate,
    char** outIdentifier,
    uint8_t outPK[32],
    uint8_t outSK[32],
    void* inContext);

OSStatus
AirPlayReceiverPairingDelegateFindPeerAirPlay(
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    uint8_t outPK[32],
    uint64_t* outPermissions,
    void* inContext);

OSStatus
AirPlayReceiverPairingDelegateSavePeerAirPlay(
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    const uint8_t inPK[32],
    void* inContext);

OSStatus
AirPlayReceiverPairingDelegateFindPeerHomeKit(
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    uint8_t outPK[32],
    uint64_t* outPermissions,
    void* inContext);

OSStatus
AirPlayReceiverPairingDelegateSavePeerHomeKit(
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    const uint8_t inPK[32],
    void* inContext);

OSStatus AirPlayReceiverPairingDelegateAddPairing(CFDictionaryRef inAccessory, void* inContext);
OSStatus AirPlayReceiverPairingDelegateRemovePairing(CFDictionaryRef inAccessory, void* inContext);
CFArrayRef AirPlayReceiverPairingDelegateCreateListPairings(OSStatus* outErr, void* inContext);

#if 0
#pragma mark -
#endif
OSStatus HISetAccessControlEnabled(AirPlayReceiverServerRef server, Boolean enabled);
Boolean HIIsAccessControlEnabled(AirPlayReceiverServerRef server);

OSStatus HISetAccessControlLevel(AirPlayReceiverServerRef server, CFNumberRef inValue);
AirPlayHomeKitAccessControlLevel HIGetAccessControlLevel(AirPlayReceiverServerRef server);

#endif /* __HomeIntegrationUtilities_h__ */
