/*
	Copyright (C) 2012-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "PairingUtils.h"

#include "CFUtils.h"
#include "ChaCha20Poly1305.h"
#include "CommonServices.h"
#include "CryptoHashUtils.h"
#include "DebugServices.h"
#include "OPACKUtils.h"
#include "PrintFUtils.h"
#include "RandomNumberUtils.h"
#include "SHAUtils.h"
#include "SRPUtils.h"
#include "StringUtils.h"
#include "TLVUtils.h"
#include "ThreadUtils.h"
#include "TickUtils.h"
#include "UUIDUtils.h"

#include CF_HEADER
#include CF_RUNTIME_HEADER
#include CURVE25519_HEADER
#include ED25519_HEADER
#include LIBDISPATCH_HEADER
#include SHA_HEADER
#if (TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES)
#include <Security/Security.h>
#endif

//===========================================================================================================================

// PAIRING_KEYCHAIN

#if (!defined(PAIRING_KEYCHAIN))
#if (KEYCHAIN_ENABLED)
#define PAIRING_KEYCHAIN 1
#else
#define PAIRING_KEYCHAIN 0
#endif
#endif

#if (PAIRING_KEYCHAIN)
#include "KeychainUtils.h"
#endif

// PAIRING_MFI_CLIENT: 1=Enable client-side code to support MFi pairing.

#if (!defined(PAIRING_MFI_CLIENT))
#if (TARGET_OS_DARWIN && !COMMON_SERVICES_NO_CORE_SERVICES)
#define PAIRING_MFI_CLIENT 1
#else
#define PAIRING_MFI_CLIENT 0
#endif
#endif

// PAIRING_MFI_SERVER: 1=Enable server-side code to support MFi pairing.

#if (!defined(PAIRING_MFI_SERVER))
#if (TARGET_OS_WINDOWS)
#define PAIRING_MFI_SERVER 0
#else
#define PAIRING_MFI_SERVER 1
#endif
#endif

#if (PAIRING_MFI_CLIENT || PAIRING_MFI_SERVER)
#include "MFiSAP.h"
#endif

//===========================================================================================================================

#if (PAIRING_KEYCHAIN)
#define kPairingKeychainTypeCode_Identity 0x70724964 // 'prId' (for use with kSecAttrType).
#define kPairingKeychainTypeCode_Peer 0x70725065 // 'prPe' (for use with kSecAttrType).
#endif

#if (PAIRING_MFI_CLIENT || PAIRING_MFI_SERVER)
#define kMFiPairSetupSaltPtr "MFi-Pair-Setup-Salt"
#define kMFiPairSetupSaltLen sizeof_string(kMFiPairSetupSaltPtr)
#define kMFiPairSetupInfoPtr "MFi-Pair-Setup-Info"
#define kMFiPairSetupInfoLen sizeof_string(kMFiPairSetupInfoPtr)
#endif

#define kPairResumeRequestInfoPtr "Pair-Resume-Request-Info"
#define kPairResumeRequestInfoLen sizeof_string(kPairResumeRequestInfoPtr)
#define kPairResumeResponseInfoPtr "Pair-Resume-Response-Info"
#define kPairResumeResponseInfoLen sizeof_string(kPairResumeResponseInfoPtr)
#define kPairResumeSessionIDSaltPtr "Pair-Resume-SessionID-Salt"
#define kPairResumeSessionIDSaltLen sizeof_string(kPairResumeSessionIDSaltPtr)
#define kPairResumeSessionIDInfoPtr "Pair-Resume-SessionID-Info"
#define kPairResumeSessionIDInfoLen sizeof_string(kPairResumeSessionIDInfoPtr)
#define kPairResumeSharedSecretInfoPtr "Pair-Resume-Shared-Secret-Info"
#define kPairResumeSharedSecretInfoLen sizeof_string(kPairResumeSharedSecretInfoPtr)

#define kPairSetupSRPUsernamePtr "Pair-Setup"
#define kPairSetupSRPUsernameLen sizeof_string(kPairSetupSRPUsernamePtr)

#define kPairSetupEncryptSaltPtr "Pair-Setup-Encrypt-Salt"
#define kPairSetupEncryptSaltLen sizeof_string(kPairSetupEncryptSaltPtr)
#define kPairSetupEncryptInfoPtr "Pair-Setup-Encrypt-Info"
#define kPairSetupEncryptInfoLen sizeof_string(kPairSetupEncryptInfoPtr)

#define kPairSetupAccessorySignSaltPtr "Pair-Setup-Accessory-Sign-Salt"
#define kPairSetupAccessorySignSaltLen sizeof_string(kPairSetupAccessorySignSaltPtr)
#define kPairSetupAccessorySignInfoPtr "Pair-Setup-Accessory-Sign-Info"
#define kPairSetupAccessorySignInfoLen sizeof_string(kPairSetupAccessorySignInfoPtr)

#define kPairSetupControllerSignSaltPtr "Pair-Setup-Controller-Sign-Salt"
#define kPairSetupControllerSignSaltLen sizeof_string(kPairSetupControllerSignSaltPtr)
#define kPairSetupControllerSignInfoPtr "Pair-Setup-Controller-Sign-Info"
#define kPairSetupControllerSignInfoLen sizeof_string(kPairSetupControllerSignInfoPtr)

#define kPairVerifyECDHSaltPtr "Pair-Verify-ECDH-Salt"
#define kPairVerifyECDHSaltLen sizeof_string(kPairVerifyECDHSaltPtr)
#define kPairVerifyECDHInfoPtr "Pair-Verify-ECDH-Info"
#define kPairVerifyECDHInfoLen sizeof_string(kPairVerifyECDHInfoPtr)

#define kPairVerifyEncryptSaltPtr "Pair-Verify-Encrypt-Salt"
#define kPairVerifyEncryptSaltLen sizeof_string(kPairVerifyEncryptSaltPtr)
#define kPairVerifyEncryptInfoPtr "Pair-Verify-Encrypt-Info"
#define kPairVerifyEncryptInfoLen sizeof_string(kPairVerifyEncryptInfoPtr)

#define kPairVerifyResumeSessionIDSaltPtr "Pair-Verify-ResumeSessionID-Salt"
#define kPairVerifyResumeSessionIDSaltLen sizeof_string(kPairVerifyResumeSessionIDSaltPtr)
#define kPairVerifyResumeSessionIDInfoPtr "Pair-Verify-ResumeSessionID-Info"
#define kPairVerifyResumeSessionIDInfoLen sizeof_string(kPairVerifyResumeSessionIDInfoPtr)

// TLV items

#define kMaxTLVSize 16000

#define kTLVType_Method 0x00 // Pairing method to use. See kTLVMethod_*.
#define kTLVType_Identifier 0x01 // Identifier of the peer.
#define kTLVType_Salt 0x02 // 16+ bytes of random salt.
#define kTLVType_PublicKey 0x03 // Curve25519, SRP public key, or Ed25519 public key.
#define kTLVType_Proof 0x04 // SRP proof.
#define kTLVType_EncryptedData 0x05 // Encrypted bytes. Use AuthTag to authenticate.
#define kTLVType_State 0x06 // State of the pairing process.
#define kTLVType_Error 0x07 // Error code. Missing means no error. See kTLVError_*.
#define kTLVType_RetryDelay 0x08 // Seconds to delay until retrying setup.
#define kTLVType_Certificate 0x09 // X.509 Certificate.
#define kTLVType_Signature 0x0A // Ed25519, MFi auth IC, or AppleID signature.
#define kTLVType_Permissions 0x0B // Permission. See PairingPermissions.
#define kTLVType_FragmentData 0x0C // Non-last fragment of data. If length is 0, it's an ack.
#define kTLVType_FragmentLast 0x0D // Last fragment of data.
#define kTLVType_SessionID 0x0E // Identifier to save/resume a session.
#define kTLVType_TTL 0x0F // Time-to-live in seconds.
#define kTLVType_ExtraData 0x10 // Extra data during pair-setup.
#define kTLVType_Info 0x11 // OPACK-encoded dictionary of info about the device.
#define kTLVType_ACL 0x12 // OPACK-encoded dictionary of ACLs.
#define kTLVType_Flags 0x13 // PairingFlags
#define kTLVType_ValidationData 0x14 // AppleID Validation Data.
#define kTLVType_MFiAuthToken 0x15 // MFi software authentication token.
#define kTLVType_MFiProductType 0x16 // MFi product type identifier string.
#define kTLVType_SerialNumber 0x17 // Accessory serial number.
#define kTLVType_Separator 0xFF // Zero-length TLV that separates different TLVs in a list.

#define kTLVDescriptors \
    "\x00"              \
    "Method\0"          \
    "\x01"              \
    "Identifier\0"      \
    "\x02"              \
    "Salt\0"            \
    "\x03"              \
    "Public Key\0"      \
    "\x04"              \
    "Proof\0"           \
    "\x05"              \
    "EncryptedData\0"   \
    "\x06"              \
    "State\0"           \
    "\x07"              \
    "Error\0"           \
    "\x08"              \
    "RetryDelay\0"      \
    "\x09"              \
    "Certificate\0"     \
    "\x0A"              \
    "Signature\0"       \
    "\x0B"              \
    "Permissions\0"     \
    "\x0C"              \
    "FragmentData\0"    \
    "\x0D"              \
    "FragmentLast\0"    \
    "\x0E"              \
    "SessionID\0"       \
    "\x0F"              \
    "TTL\0"             \
    "\x10"              \
    "ExtraData\0"       \
    "\x11"              \
    "Info\0"            \
    "\x12"              \
    "ACL\0"             \
    "\x13"              \
    "Flags\0"           \
    "\x14"              \
    "ValidationData\0"  \
    "\x15"              \
    "MFiAuthToken\0"    \
    "\x16"              \
    "MFiProductType\0"  \
    "\x17"              \
    "SerialNumber\0"    \
    "\xFF"              \
    "Separator\0"       \
    "\x00"

// TLVError

#define kTLVError_Reserved0 0x00 // Must not be used in any TLV.
#define kTLVError_Unknown 0x01 // Generic error to handle unexpected errors.
#define kTLVError_Authentication 0x02 // Setup code or signature verification failed.
#define kTLVError_Backoff 0x03 // Client must look at <RetryDelay> TLV item and wait before retrying.
#define kTLVError_UnknownPeer 0x04 // Peer is not paired.
#define kTLVError_MaxPeers 0x05 // Server cannot accept any more pairings.
#define kTLVError_MaxTries 0x06 // Server reached its maximum number of authentication attempts
#define kTLVError_Permission 0x07 // Peer lacks required ACL.

// TLVMethod

#define kTLVMethod_PairSetup 0 // Pair-setup.
#define kTLVMethod_MFiPairSetup 1 // MFi pair-setup.
#define kTLVMethod_Verify 2 // Pair-verify.
#define kTLVMethod_AddPairing 3 // Add-Pairing.
#define kTLVMethod_RemovePairing 4 // Remove-Pairing.
#define kTLVMethod_ListPairings 5 // List-Pairings.
#define kTLVMethod_Resume 6 // Pair-resume.

// Errors

#define PairingStatusToOSStatus(X) ( \
    ((X) == kTLVError_Reserved0) ? kValueErr : ((X) == kTLVError_Unknown) ? kUnknownErr : ((X) == kTLVError_Authentication) ? kAuthenticationErr : ((X) == kTLVError_Backoff) ? kBackoffErr : ((X) == kTLVError_UnknownPeer) ? kNotFoundErr : ((X) == kTLVError_MaxPeers) ? kNoSpaceErr : ((X) == kTLVError_MaxTries) ? kCountErr : ((X) == kTLVError_Permission) ? kPermissionErr : kUnknownErr)

#define OSStatusToPairingStatus(X) ( \
    ((X) == kUnknownErr) ? kTLVError_Unknown : ((X) == kAuthenticationErr) ? kTLVError_Authentication : ((X) == kBackoffErr) ? kTLVError_Backoff : ((X) == kNotFoundErr) ? kTLVError_UnknownPeer : ((X) == kNoSpaceErr) ? kTLVError_MaxPeers : ((X) == kCountErr) ? kTLVError_MaxTries : ((X) == kPermissionErr) ? kTLVError_Permission : kTLVError_Unknown)

// States

#define kPairingStateInvalid 0

#define kPairResumeStateM1 1 // Controller -> Accessory  -- Resume session.
#define kPairResumeStateM2 2 // Accessory  -> Controller -- Resume response.
#define kPairResumeStateDone 3

#define kPairSetupStateM1 1 // Controller -> Accessory  -- Start Request
#define kPairSetupStateM2 2 // Accessory  -> Controller -- Start Response
#define kPairSetupStateM3 3 // Controller -> Accessory  -- Verify Request
#define kPairSetupStateM4 4 // Accessory  -> Controller -- Verify Response
#define kPairSetupStateM5 5 // Controller -> Accessory  -- Exchange Request
#define kPairSetupStateM6 6 // Accessory  -> Controller -- Exchange Response
#define kPairSetupStateDone 7

#define kPairVerifyStateM1 1 // Controller -> Accessory  -- Start Request
#define kPairVerifyStateM2 2 // Accessory  -> Controller -- Start Response
#define kPairVerifyStateM3 3 // Controller -> Accessory  -- Verify Request
#define kPairVerifyStateM4 4 // Accessory  -> Controller -- Verify Response
#define kPairVerifyStateDone 5

#define kAdminPairingStateM1 1 // Controller -> Accessory  -- Request
#define kAdminPairingStateM2 2 // Accessory  -> Controller -- Response
#define kAdminPairingStateDone 3

//===========================================================================================================================

struct PairingSessionPrivate {
    CFRuntimeBase base; // CF type info. Must be first.
    LogCategory* ucat; // Category to use for all logging.
    PairingDelegate delegate; // Delegate to customize behavior.
    PairingSessionType type; // Type of pairing operation to perform.
    PairingFlags flags; // Flags to control pairing.
    size_t mtuPayload; // Max number of payload bytes per fragment.
    size_t mtuTotal; // Max number of bytes to output from PairingSessionExchange.
    uint8_t state; // Current state of the pairing process.
    uint8_t* inputFragmentBuf; // Input fragment being reassembled.
    size_t inputFragmentLen; // Number of bytes in inputFragmentBuf.
    uint8_t* outputFragmentBuf; // Output fragment being fragmented.
    size_t outputFragmentLen; // Number of bytes in outputFragmentBuf.
    size_t outputFragmentOffset; // Number of bytes already sent from outputFragmentBuf.
    Boolean outputDone; // True if the output should be marked done on the last fragment.
    CFDictionaryRef acl; // ACL granted (PairSetup) or required (PairVerify).
    char* activeIdentifierPtr; // Malloc'd identifier ptr being used for the current exchange.
    size_t activeIdentifierLen; // Number of bytes in activeIdentifierPtr.
    uint8_t* ourExtraDataPtr; // Malloc'd extra data to send to the peer.
    size_t ourExtraDataLen; // Number of bytes in ourExtraDataPtr.
    uint8_t* peerExtraDataPtr; // Malloc'd extra received from the peer.
    size_t peerExtraDataLen; // Number of bytes in peerExtraDataPtr.
    char* identifierPtr; // Malloc'd identifier ptr set manually.
    size_t identifierLen; // Number of bytes in identifierPtr.
    CFTypeRef pairedPeer; // CUPairedPeer object if PairSetup successful.
    CFDictionaryRef peerACL; // ACLs requested by the peer.
    uint8_t* peerIdentifierPtr; // Malloc'd peer identifier ptr.
    size_t peerIdentifierLen; // Number of bytes in peerIdentifierPtr.
    CFDictionaryRef peerInfo; // Info received from the peer during PairSetup.
    CFDictionaryRef peerInfoAdditional; // Info to add to peer info when saving.
    CFDictionaryRef selfInfo; // Self info from this library with user-specified self info added.
    CFDictionaryRef selfInfoAdditional; // Info to send to the peer during PairSetup.
    CFMutableDictionaryRef properties; // Properties for the session.
    uint8_t* resumeDataPtr; // Malloc'd resume request or response data.
    size_t resumeDataLen; // Number of bytes in resumeDataPtr.
    uint64_t resumeMaxTicks; // Number of ticks resume state remains valid.
    uint64_t resumeSessionID; // Unique ID for saving/resuming a session.
    char* setupCodePtr; // Malloc'd setup code ptr.
    size_t setupCodeLen; // Number of bytes in setupCodePtr.
    Boolean setupCodeFailed; // True if we tried a setup code and it failed.
    char setupCodeBuf[12]; // Holds a setup code string (e.g. "123-45-678").
    Boolean showingSetupCode; // True if we're currently showing a setup code to the user.
    uint8_t key[32]; // Key for encrypting/decrypting data.
    uint8_t ourCurvePK[32]; // Our Curve25519 public key.
    uint8_t ourCurveSK[32]; // Our Curve25519 secret key.
    uint8_t ourEdPK[32]; // Our Ed25519 public key.
    uint8_t ourEdSK[32]; // Our Ed25519 secret key.
    uint8_t peerCurvePK[32]; // Peer's Curve25519 public key.
    uint8_t peerEdPK[32]; // Peer's Ed25519 public key.
    uint8_t sharedSecret[32]; // Curve25519 shared secret.

    // Pair-setup.

    PairingFlags requestFlags; // Flags client request.
    uint8_t requestMethod; // Method client requested. See kTLVMethod_*.
    SRPRef srpCtx; // SRP context for pair setup.
    uint8_t* srpPKPtr; // Malloc'd SRP public key from the peer.
    size_t srpPKLen; // Number of bytes in srpPKPtr.
    uint8_t* srpSaltPtr; // Malloc'd SRP salt from the peer.
    size_t srpSaltLen; // Number of bytes in srpSaltPtr.
    uint8_t* srpSharedSecretPtr; // SRP-derived shared secret.
    size_t srpSharedSecretLen; // Number of bytes in srpSharedSecretPtr.

#if (PAIRING_KEYCHAIN)
    // Keychain support.

    CFStringRef keychainAccessGroup; // Keychain group used for identity and peer items.
    CFStringRef keychainIdentityLabel; // Prefix for the label of the identity Keychain item.
    uint32_t keychainIdentityType; // Type used for programmatic searches of pairing Keychain identities.
    CFStringRef keychainIdentityDesc; // Describes how the Keychain identity item is used.
    CFStringRef keychainPeerLabel; // Prefix for the label of peer Keychain items.
    uint32_t keychainPeerType; // Type used for programmatic searches of pairing Keychain peers.
    CFStringRef keychainPeerDesc; // Describes how Keychain peer items are used.
    PairingKeychainFlags keychainFlags; // Flags for controlling Keychain operations.
#endif

    // Handlers

    CFTypeRef dispatchQueue; // Queue for handlers.

    PairingSessionEventHandler_f eventHandler_f; // Handle events.
    void* eventHandler_ctx; // Context for handler.

    PairingSessionAddPairingHandler_f addPairingHandler_f; // Adds controller pairings.
    void* addPairingHandler_ctx;

    PairingSessionRemovePairingHandler_f removePairingHandler_f; // Removes controller pairings.
    void* removePairingHandler_ctx;

    PairingSessionListPairingsHandler_f listPairingsHandler_f; // Lists controller pairings.
    void* listPairingsHandler_ctx;

    PairingSessionCreateSignatureHandler_f createSignatureHandler_f; // Creates MFi signatures.
    void* createSignatureHandler_ctx; // Context for handler.
};

// PairingResumeState

typedef struct PairingResumeState* PairingResumeStateRef;
struct PairingResumeState {
    PairingResumeStateRef next; // Next state in the list.
    uint8_t* peerIdentifierPtr; // malloc'd peer identifier.
    size_t peerIdentifierLen; // Number of bytes in peerIdentifierPtr.
    uint8_t* selfIdentifierPtr; // malloc'd self identifier.
    size_t selfIdentifierLen; // Number of bytes in selfIdentifierPtr.
    uint64_t expireTicks; // UpTicks when the session was saved (for expiring).
    uint8_t sharedSecret[32]; // Shared secret saved from pair-verify.
    uint64_t sessionID; // Unique ID of state.
};

//===========================================================================================================================

static void _PairingSessionGetTypeID(void* inContext);
static void _PairingSessionFinalize(CFTypeRef inCF);
static void _PairingSessionReset(PairingSessionRef inSession);
static void _PairingSessionInvalidate(void* inArg);

// Segmentation and Reassembly.

static OSStatus
_ProgressInput(
    PairingSessionRef inSession,
    const uint8_t** ioInputPtr,
    size_t* ioInputLen,
    uint8_t** outInputStorage,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone);
static OSStatus
_ProgressOutput(
    PairingSessionRef me,
    uint8_t* inOutputPtr,
    size_t inOutputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* ioDone);

// PairResume.

static OSStatus
_ResumePairingClientExchange(
    PairingSessionRef me,
    const void* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone);
static OSStatus
_ResumePairingServerExchange(
    PairingSessionRef me,
    const void* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone);

// PairSetup.

static OSStatus
_SetupClientExchange(
    PairingSessionRef inSession,
    const void* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone);
static OSStatus
_SetupServerExchange(
    PairingSessionRef inSession,
    const void* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone);

// PairVerify Client.

static OSStatus
_VerifyClientExchange(
    PairingSessionRef inSession,
    const void* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone);
static OSStatus
_VerifyClientM1(
    PairingSessionRef me,
    const uint8_t* const inInputPtr,
    const uint8_t* const inInputEnd,
    uint8_t** outOutputPtr,
    size_t* outOutputLen);
static OSStatus
_VerifyClientM2(
    PairingSessionRef me,
    const uint8_t* const inInputPtr,
    const uint8_t* const inInputEnd,
    uint8_t** outOutputPtr,
    size_t* outOutputLen);
static OSStatus
_VerifyClientM3(
    PairingSessionRef me,
    uint8_t** outOutputPtr,
    size_t* outOutputLen);
static OSStatus
_VerifyClientM4(
    PairingSessionRef me,
    const uint8_t* const inInputPtr,
    const uint8_t* const inInputEnd);

// PairVerify Server.

static OSStatus
_VerifyServerExchange(
    PairingSessionRef inSession,
    const void* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone);
static OSStatus
_VerifyServerM1(
    PairingSessionRef me,
    const uint8_t* const inInputPtr,
    const uint8_t* const inInputEnd,
    uint8_t** outOutputPtr,
    size_t* outOutputLen);
static OSStatus
_VerifyServerM2(
    PairingSessionRef me,
    uint8_t** outOutputPtr,
    size_t* outOutputLen);
static OSStatus
_VerifyServerM3(
    PairingSessionRef me,
    const uint8_t* const inInputPtr,
    const uint8_t* const inInputEnd,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone);
static OSStatus
_VerifyServerM4(
    PairingSessionRef me,
    uint8_t** outOutputPtr,
    size_t* outOutputLen);

// AdminPairing.

static OSStatus
_AdminPairingClientExchange(
    PairingSessionRef me,
    const void* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone);
static OSStatus
_AdminPairingClientM1(
    PairingSessionRef me,
    const uint8_t* const inInputPtr,
    const uint8_t* const inInputEnd,
    uint8_t** outOutputPtr,
    size_t* outOutputLen);
static OSStatus
_AdminPairingClientM2(
    PairingSessionRef me,
    const uint8_t* const inInputPtr,
    const uint8_t* const inInputEnd,
    const uint8_t* const inStateEnd);
static OSStatus
_AdminPairingClientParsePairings(
    PairingSessionRef me,
    const uint8_t* inInputPtr,
    const uint8_t* const inInputEnd);
static OSStatus
_AdminPairingServerExchange(
    PairingSessionRef me,
    const void* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone);
static OSStatus
_AdminPairingServerM1(
    PairingSessionRef me,
    const uint8_t* const inInputPtr,
    const uint8_t* const inInputEnd,
    uint8_t** outOutputPtr,
    size_t* outOutputLen);
static OSStatus _AdminPairingServerAppendPairings(TLV8Buffer* inTLVBuffer, CFArrayRef inPairings);

// Utilities.

static CFDictionaryRef _PairingSessionIntersectACL(PairingSessionRef me, CFDictionaryRef inACL, OSStatus* outErr);
static void _PairingSessionIntersectACLApplier(const void* inKey, const void* inValue, void* inContext);
static OSStatus _PairingSessionVerifyACL(PairingSessionRef me, CFDictionaryRef inACL);
static void _PairingSessionVerifyACLApplier(const void* inKey, const void* inValue, void* inContext);

static int32_t _PairingThrottle(void);
static void _PairingResetThrottle(void);
static OSStatus
_PairingFindResumeState(
    uint64_t inSessionID,
    uint8_t** outIdentifierPtr,
    size_t* outIdentifierLen,
    uint8_t* inSharedSecretBuf,
    size_t inSharedSecretLen);
static void _PairingRemoveResumeSessionID(uint64_t inSessionID);
static OSStatus
_PairingSaveResumeState(
    PairingSessionRef me,
    const void* inPeerIdentifierPtr,
    size_t inPeerIdentifierLen,
    const void* inSelfIdentifierPtr,
    size_t inSelfIdentifierLen,
    uint64_t inSessionID,
    const void* inSharedSecretPtr,
    size_t inSharedSecretLen);
static void _PairingFreeResumeState(PairingResumeStateRef inState);

// Keychain support.

#if (PAIRING_KEYCHAIN)

static OSStatus _PairingSessionDeleteIdentity(PairingSessionRef inSession);
static OSStatus
_PairingSessionGetOrCreateIdentityKeychain(
    PairingSessionRef inSession,
    Boolean inAllowCreate,
    char** outIdentifier,
    uint8_t* outPK,
    uint8_t* outSK);
static OSStatus
_PairingSessionCopyIdentityKeychain(
    PairingSessionRef inSession,
    char** outIdentifier,
    uint8_t* outPK,
    uint8_t* outSK);
static OSStatus
_PairingSessionCreateIdentityKeychain(
    PairingSessionRef inSession,
    char** outIdentifier,
    uint8_t* outPK,
    uint8_t* outSK);
static CFArrayRef
_PairingSessionCopyPeers(
    PairingSessionRef inSession,
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    OSStatus* outErr);
static OSStatus
_PairingSessionDeletePeer(
    PairingSessionRef inSession,
    const void* inIdentifierPtr,
    size_t inIdentifierLen);
static OSStatus
PairingSessionFindPeerEx(
    PairingSessionRef me,
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    uint8_t* outPK,
    CFDictionaryRef* outACL);
static OSStatus
_PairingSessionFindPeerKeychain(
    PairingSessionRef inSession,
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    uint8_t* outPK,
    CFDictionaryRef* outACL);
#if (PAIRING_KEYCHAIN)
static CFArrayRef _PairingSessionListControllersKeychain(PairingSessionRef me, OSStatus* outErr);
static OSStatus _PairingSessionRemoveControllerKeychain(PairingSessionRef me, CFDictionaryRef inInfo);
static OSStatus _PairingSessionSaveControllerKeychain(PairingSessionRef me, CFDictionaryRef inInfo);
#endif
static OSStatus
_PairingSessionSavePeerKeychain(
    PairingSessionRef me,
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    const uint8_t* inPK,
    PairingPermissions inPermissions);

#endif // PAIRING_KEYCHAIN

//===========================================================================================================================

static dispatch_once_t gPairingSessionInitOnce = 0;
static CFTypeID gPairingSessionTypeID = _kCFRuntimeNotATypeID;
static const CFRuntimeClass kPairingSessionClass = {
    0, // version
    "PairingSession", // className
    NULL, // init
    NULL, // copy
    _PairingSessionFinalize, // finalize
    NULL, // equal -- NULL means pointer equality.
    NULL, // hash  -- NULL means pointer hash.
    NULL, // copyFormattingDesc
    NULL, // copyDebugDesc
    NULL, // reclaim
    NULL // refcount
};

static pthread_mutex_t gPairingGlobalLock = PTHREAD_MUTEX_INITIALIZER;
static uint64_t gPairingThrottleNextTicks = 0;
static uint32_t gPairingThrottleCounter = 0;
static uint32_t gPairingMaxResumeSessions = 8;
static uint32_t gPairingMaxTries = 0;
static PairingResumeStateRef gPairingResumeStateList = NULL;
static uint32_t gPairingTries = 0;

ulog_define(Pairing, kLogLevelTrace, kLogFlags_Default, "Pairing", NULL);
#define pair_ucat() &log_category_from_name(Pairing)
#define pair_dlog(SESSION, LEVEL, ...) dlogc((SESSION)->ucat, (LEVEL), __VA_ARGS__)
#define pair_ulog(SESSION, LEVEL, ...) ulog((SESSION)->ucat, (LEVEL), __VA_ARGS__)

//===========================================================================================================================

CFTypeID PairingSessionGetTypeID(void)
{
    dispatch_once_f(&gPairingSessionInitOnce, NULL, _PairingSessionGetTypeID);
    return (gPairingSessionTypeID);
}

static void _PairingSessionGetTypeID(void* inContext)
{
    (void)inContext;

    gPairingSessionTypeID = _CFRuntimeRegisterClass(&kPairingSessionClass);
    check(gPairingSessionTypeID != _kCFRuntimeNotATypeID);
}

//===========================================================================================================================

OSStatus PairingSessionCreate(PairingSessionRef* outSession, const PairingDelegate* inDelegate, PairingSessionType inType)
{
    OSStatus err;
    PairingSessionRef me;
    size_t extraLen;

    extraLen = sizeof(*me) - sizeof(me->base);
    me = (PairingSessionRef)_CFRuntimeCreateInstance(NULL, PairingSessionGetTypeID(), (CFIndex)extraLen, NULL);
    require_action(me, exit, err = kNoMemoryErr);
    memset(((uint8_t*)me) + sizeof(me->base), 0, extraLen);

    me->ucat = pair_ucat();
    me->type = inType;
    me->mtuPayload = SIZE_MAX - 2;
    me->mtuTotal = SIZE_MAX;
    me->resumeMaxTicks = SecondsToUpTicks(48 * kSecondsPerHour);
    if (inDelegate)
        me->delegate = *inDelegate;
    else
        PairingDelegateInit(&me->delegate);

#if (PAIRING_KEYCHAIN)
    // Set up default Keychain info.

    PairingSessionSetKeychainInfo(me, CFSTR("com.apple.pairing"),
        kPairingKeychainTypeCode_Identity, CFSTR("Pairing Identity"), CFSTR("Pairing Identity"),
        kPairingKeychainTypeCode_Peer, CFSTR("Paired Peer"), CFSTR("Paired Peer"),
        kPairingKeychainFlags_None);
#endif

    *outSession = me;
    me = NULL;
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================

static void _PairingSessionFinalize(CFTypeRef inCF)
{
    PairingSessionRef const me = (PairingSessionRef)inCF;

    if (me->showingSetupCode) {
        if (me->delegate.hideSetupCode_f)
            me->delegate.hideSetupCode_f(me->delegate.context);
        me->showingSetupCode = false;
    }
    _PairingSessionReset(me);
    CFForget(&me->acl);
    CFForget(&me->pairedPeer);
    CFForget(&me->peerACL);
    ForgetPtrLen(&me->ourExtraDataPtr, &me->ourExtraDataLen);
    ForgetPtrLen(&me->peerExtraDataPtr, &me->peerExtraDataLen);
    ForgetPtrLen(&me->identifierPtr, &me->identifierLen);
    ForgetPtrLen(&me->peerIdentifierPtr, &me->peerIdentifierLen);
    CFForget(&me->peerInfo);
    CFForget(&me->peerInfoAdditional);
    CFForget(&me->selfInfo);
    CFForget(&me->selfInfoAdditional);
    ForgetPtrLen(&me->setupCodePtr, &me->setupCodeLen);
    ForgetPtrLen(&me->resumeDataPtr, &me->resumeDataLen);
    CFForget(&me->properties);
#if (PAIRING_KEYCHAIN)
    CFForget(&me->keychainAccessGroup);
    CFForget(&me->keychainIdentityLabel);
    CFForget(&me->keychainIdentityDesc);
    CFForget(&me->keychainPeerLabel);
    CFForget(&me->keychainPeerDesc);
#endif
#if (defined(OS_OBJECT_USE_OBJC) && OS_OBJECT_USE_OBJC)
    CFForget(&me->dispatchQueue);
#else
    if (me->dispatchQueue) {
        dispatch_release((dispatch_queue_t)me->dispatchQueue);
        me->dispatchQueue = NULL;
    }
#endif
    MemZeroSecure(((uint8_t*)me) + sizeof(me->base), sizeof(*me) - sizeof(me->base));
}

//===========================================================================================================================

static void _PairingSessionReset(PairingSessionRef me)
{
    me->state = kPairingStateInvalid;
    ForgetPtrLen(&me->inputFragmentBuf, &me->inputFragmentLen);
    ForgetPtrLen(&me->outputFragmentBuf, &me->outputFragmentLen);
    me->outputFragmentOffset = 0;
    me->outputDone = false;
    ForgetPtrLen(&me->activeIdentifierPtr, &me->activeIdentifierLen);
    MemZeroSecure(me->key, sizeof(me->key));
    SRPForget(&me->srpCtx);
    ForgetPtrLen(&me->srpPKPtr, &me->srpPKLen);
    ForgetPtrLen(&me->srpSaltPtr, &me->srpSaltLen);
    ForgetPtrLenSecure(&me->srpSharedSecretPtr, &me->srpSharedSecretLen);
    MemZeroSecure(me->ourCurveSK, sizeof(me->ourCurveSK));
    MemZeroSecure(me->ourEdSK, sizeof(me->ourEdSK));
    MemZeroSecure(me->sharedSecret, sizeof(me->sharedSecret));
}

//===========================================================================================================================

void PairingSessionInvalidate(PairingSessionRef me)
{
#if (defined(OS_OBJECT_USE_OBJC) && OS_OBJECT_USE_OBJC)
    dispatch_queue_t queue = (__bridge dispatch_queue_t)me->dispatchQueue;
#else
    dispatch_queue_t queue = (dispatch_queue_t)me->dispatchQueue;
#endif
    if (queue) {
        CFRetain(me);
        dispatch_async_f(queue, me, _PairingSessionInvalidate);
    }
}

static void _PairingSessionInvalidate(void* inArg)
{
    PairingSessionRef const me = (PairingSessionRef)inArg;

    if (me->eventHandler_f) {
        me->eventHandler_f(kPairingSessionEventTypeInvalidated, NULL, me->eventHandler_ctx);
        me->eventHandler_f = NULL;
    }
    CFRelease(me);
}

//===========================================================================================================================

OSStatus PairingSessionSetACL(PairingSessionRef me, CFDictionaryRef inACL)
{
    ReplaceCF(&me->acl, inACL);
    return (kNoErr);
}

//===========================================================================================================================

void PairingSessionSetDispatchQueue(PairingSessionRef me, dispatch_queue_t inQueue)
{
#if (defined(OS_OBJECT_USE_OBJC) && OS_OBJECT_USE_OBJC)
    CFTypeRef cfQueue = CFBridgingRetain(inQueue);
    CFReleaseNullSafe(me->dispatchQueue);
    me->dispatchQueue = cfQueue;
#else
    if (inQueue)
        dispatch_retain(inQueue);
    if (me->dispatchQueue)
        dispatch_release((dispatch_queue_t)me->dispatchQueue);
    me->dispatchQueue = (CFTypeRef)inQueue;
#endif
}

//===========================================================================================================================

uint8_t* PairingSessionCopyExtraData(PairingSessionRef me, size_t* outLen, OSStatus* outErr)
{
    uint8_t* ptr = NULL;
    size_t len = 0;
    OSStatus err;

    require_action_quiet(me->peerExtraDataPtr, exit, err = kNotFoundErr);
    ptr = (uint8_t*)malloc((me->peerExtraDataLen > 0) ? me->peerExtraDataLen : 1);
    require_action(ptr, exit, err = kNoMemoryErr);
    len = me->peerExtraDataLen;
    memcpy(ptr, me->peerExtraDataPtr, len);
    err = kNoErr;

exit:
    if (outLen)
        *outLen = len;
    if (outErr)
        *outErr = err;
    return (ptr);
}

//===========================================================================================================================

OSStatus PairingSessionSetExtraData(PairingSessionRef me, const void* inPtr, size_t inLen)
{
    OSStatus err;
    uint8_t* ptr;

    if (inPtr) {
        ptr = (uint8_t*)malloc((inLen > 0) ? inLen : 1);
        require_action(ptr, exit, err = kNoMemoryErr);
        if (inLen > 0)
            memcpy(ptr, inPtr, inLen);
    } else {
        ptr = NULL;
    }
    FreeNullSafe(me->ourExtraDataPtr);
    me->ourExtraDataPtr = ptr;
    me->ourExtraDataLen = inLen;
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================

void PairingSessionSetFlags(PairingSessionRef me, PairingFlags inFlags)
{
    me->flags = inFlags;
}

//===========================================================================================================================

OSStatus PairingSessionSetIdentifier(PairingSessionRef me, const void* inPtr, size_t inLen)
{
    return (ReplaceString(&me->identifierPtr, &me->identifierLen, inPtr, inLen));
}

//===========================================================================================================================

CFDictionaryRef PairingSessionCopyPeerInfo(PairingSessionRef me, OSStatus* outErr)
{
    CFMutableDictionaryRef peerInfo = NULL;
    OSStatus err;

    if (me->peerInfo) {
        peerInfo = CFDictionaryCreateMutableCopy(NULL, 0, me->peerInfo);
        require_action(peerInfo, exit, err = kNoMemoryErr);
    }
    if (me->peerInfoAdditional) {
        if (peerInfo) {
            CFDictionaryMergeDictionary(peerInfo, me->peerInfoAdditional);
        } else {
            peerInfo = CFDictionaryCreateMutableCopy(NULL, 0, me->peerInfoAdditional);
            require_action(peerInfo, exit, err = kNoMemoryErr);
        }
    }
    require_action_quiet(peerInfo, exit, err = kNotFoundErr);
    CFDictionaryRemoveValue(peerInfo, CFSTR(kPairingKey_AltIRK)); // Remove AltIRK so it's not reported twice.
    err = kNoErr;

exit:
    if (outErr)
        *outErr = err;
    return (peerInfo);
}

//===========================================================================================================================

OSStatus PairingSessionSetAdditionalPeerInfo(PairingSessionRef me, CFDictionaryRef inInfo)
{
    ReplaceCF(&me->peerInfoAdditional, inInfo);
    return (kNoErr);
}

//===========================================================================================================================

//===========================================================================================================================

OSStatus PairingSessionSetAdditionalSelfInfo(PairingSessionRef me, CFDictionaryRef inInfo)
{
    ReplaceCF(&me->selfInfoAdditional, inInfo);
    return (kNoErr);
}

#if (PAIRING_KEYCHAIN)
//===========================================================================================================================

void PairingSessionSetKeychainInfo(
    PairingSessionRef me,
    CFStringRef inAccessGroup,
    uint32_t inIdentityType,
    CFStringRef inIdentityLabel,
    CFStringRef inIdentityDesc,
    uint32_t inPeerType,
    CFStringRef inPeerLabel,
    CFStringRef inPeerDesc,
    PairingKeychainFlags inFlags)
{
    if (inAccessGroup) {
        if (CFEqual(inAccessGroup, kPairingKeychainAccessGroup_None)) {
            CFForget(&me->keychainAccessGroup);
        } else {
            ReplaceCF(&me->keychainAccessGroup, inAccessGroup);
        }
    }
    if (inIdentityType)
        me->keychainIdentityType = inIdentityType;
    if (inIdentityLabel)
        ReplaceCF(&me->keychainIdentityLabel, inIdentityLabel);
    if (inIdentityDesc)
        ReplaceCF(&me->keychainIdentityDesc, inIdentityDesc);
    if (inPeerType)
        me->keychainPeerType = inPeerType;
    if (inPeerLabel)
        ReplaceCF(&me->keychainPeerLabel, inPeerLabel);
    if (inPeerDesc)
        ReplaceCF(&me->keychainPeerDesc, inPeerDesc);
    me->keychainFlags = inFlags;
}
#endif

//===========================================================================================================================

void PairingSessionSetLogging(PairingSessionRef me, LogCategory* inLogCategory)
{
    me->ucat = inLogCategory;
}

//===========================================================================================================================

void PairingSessionSetMaxTries(PairingSessionRef me, int inMaxTries)
{
    (void)me;

    gPairingMaxTries = (uint32_t)inMaxTries;
}

//===========================================================================================================================

OSStatus PairingSessionSetMTU(PairingSessionRef me, size_t inMTU)
{
    OSStatus err;
    size_t len;

    len = TLV8MaxPayloadBytesForTotalBytes(inMTU);
    require_action(len > 0, exit, err = kSizeErr);

    me->mtuPayload = len;
    me->mtuTotal = inMTU;
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================

char* PairingSessionCopyPeerIdentifier(PairingSessionRef me, size_t* outLen, OSStatus* outErr)
{
    char* ptr = NULL;
    size_t len = 0;
    OSStatus err;

    require_action_quiet(me->peerIdentifierPtr, exit, err = kNotFoundErr);
    ptr = strndup((const char*)me->peerIdentifierPtr, me->peerIdentifierLen);
    require_action(ptr, exit, err = kNoMemoryErr);
    len = me->peerIdentifierLen;
    err = kNoErr;

exit:
    if (outLen)
        *outLen = len;
    if (outErr)
        *outErr = err;
    return (ptr);
}

//===========================================================================================================================

CFTypeRef PairingSessionCopyProperty(PairingSessionRef me, CFStringRef inKey, OSStatus* outErr)
{
    CFTypeRef result = NULL;
    OSStatus err;

    require_action_quiet(me->properties, exit, err = kNotFoundErr);
    result = CFDictionaryGetValue(me->properties, inKey);
    require_action_quiet(result, exit, err = kNotFoundErr);
    CFRetain(result);
    err = kNoErr;

exit:
    if (outErr)
        *outErr = err;
    return (result);
}

//===========================================================================================================================

OSStatus PairingSessionSetProperty(PairingSessionRef me, CFStringRef inKey, CFTypeRef inValue)
{
    OSStatus err;

    if (!me->properties) {
        me->properties = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require_action(me->properties, exit, err = kNoMemoryErr);
    }
    if (inValue)
        CFDictionarySetValue(me->properties, inKey, inValue);
    else
        CFDictionaryRemoveValue(me->properties, inKey);
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================

OSStatus PairingSessionGetResumeInfo(PairingSessionRef me, uint64_t* outSessionID)
{
    OSStatus err;

    require_action(
        (((me->type == kPairingSessionType_VerifyClient) || (me->type == kPairingSessionType_VerifyServer)) && (me->state == kPairVerifyStateDone)) || (((me->type == kPairingSessionType_ResumeClient) || (me->type == kPairingSessionType_ResumeServer)) && (me->state == kPairResumeStateDone)), exit, err = kStateErr);

    *outSessionID = me->resumeSessionID;
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================

OSStatus
PairingSessionSetResumeInfo(
    PairingSessionRef me,
    uint64_t inSessionID,
    const void* inResumeRequestPtr,
    size_t inResumeRequestLen)
{
    OSStatus err;
    uint8_t* ptr;

    ptr = (uint8_t*)malloc((inResumeRequestLen > 0) ? inResumeRequestLen : 1);
    require_action(ptr, exit, err = kNoMemoryErr);
    if (inResumeRequestLen > 0)
        memcpy(ptr, inResumeRequestPtr, inResumeRequestLen);

    FreeNullSafe(me->resumeDataPtr);
    me->resumeDataPtr = ptr;
    me->resumeDataLen = inResumeRequestLen;
    me->resumeSessionID = inSessionID;
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================

OSStatus PairingSessionSetMaxResumeSessions(PairingSessionRef me, uint32_t inMaxSessions)
{
    (void)me;

    gPairingMaxResumeSessions = inMaxSessions;
    return (kNoErr);
}

//===========================================================================================================================

OSStatus PairingSessionSetResumeTTL(PairingSessionRef me, uint32_t inSeconds)
{
    me->resumeMaxTicks = SecondsToUpTicks(inSeconds);
    return (kNoErr);
}

//===========================================================================================================================

OSStatus PairingSessionSetSetupCode(PairingSessionRef me, const void* inPtr, size_t inLen)
{
    return (ReplaceString(&me->setupCodePtr, &me->setupCodeLen, inPtr, inLen));
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================

void PairingSessionSetEventHandler(PairingSessionRef me, PairingSessionEventHandler_f inHandler, void* inContext)
{
    me->eventHandler_f = inHandler;
    me->eventHandler_ctx = inContext;
}

//===========================================================================================================================

void PairingSessionSetAddPairingHandler(
    PairingSessionRef me,
    PairingSessionAddPairingHandler_f inHandler,
    void* inContext)
{
    me->addPairingHandler_f = inHandler;
    me->addPairingHandler_ctx = inContext;
}

//===========================================================================================================================

void PairingSessionSetRemovePairingHandler(
    PairingSessionRef me,
    PairingSessionRemovePairingHandler_f inHandler,
    void* inContext)
{
    me->removePairingHandler_f = inHandler;
    me->removePairingHandler_ctx = inContext;
}

//===========================================================================================================================

void PairingSessionSetListPairingsHandler(
    PairingSessionRef me,
    PairingSessionListPairingsHandler_f inHandler,
    void* inContext)
{
    me->listPairingsHandler_f = inHandler;
    me->listPairingsHandler_ctx = inContext;
}

//===========================================================================================================================

void PairingSessionSetCreateSignatureHandler(
    PairingSessionRef me,
    PairingSessionCreateSignatureHandler_f inHandler,
    void* inContext)
{
    me->createSignatureHandler_f = inHandler;
    me->createSignatureHandler_ctx = inContext;
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================

OSStatus
PairingSessionExchange(
    PairingSessionRef me,
    const void* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone)
{
    const uint8_t* inputPtr = (const uint8_t*)inInputPtr;
    uint8_t* inputStorage = NULL;
    uint8_t* outputPtr = NULL;
    size_t outputLen = 0;
    OSStatus err;

    require_action(me, exit, err = kBadReferenceErr);
    err = _ProgressInput(me, &inputPtr, &inInputLen, &inputStorage, &outputPtr, &outputLen, outDone);
    require_noerr_quiet(err, exit);
    if (outputPtr) {
        *outOutputPtr = outputPtr;
        *outOutputLen = outputLen;
        outputPtr = NULL;
        goto exit;
    }
    
    pair_ulog(me, kLogLevelNotice, "PairingSessionExchange type [%d]\n", me->type);

    switch (me->type) {
    case kPairingSessionType_SetupClient:
        err = _SetupClientExchange(me, inputPtr, inInputLen, &outputPtr, &outputLen, outDone);
        break;

    case kPairingSessionType_SetupServer:
        err = _SetupServerExchange(me, inputPtr, inInputLen, &outputPtr, &outputLen, outDone);
        break;

    case kPairingSessionType_VerifyClient:
        err = _VerifyClientExchange(me, inputPtr, inInputLen, &outputPtr, &outputLen, outDone);
        break;

    case kPairingSessionType_VerifyServer:
        err = _VerifyServerExchange(me, inputPtr, inInputLen, &outputPtr, &outputLen, outDone);
        break;

    case kPairingSessionType_ResumeClient:
        err = _ResumePairingClientExchange(me, inputPtr, inInputLen, &outputPtr, &outputLen, outDone);
        break;

    case kPairingSessionType_ResumeServer:
        err = _ResumePairingServerExchange(me, inputPtr, inInputLen, &outputPtr, &outputLen, outDone);
        break;

    case kPairingSessionType_AddPairingClient:
    case kPairingSessionType_RemovePairingClient:
    case kPairingSessionType_ListPairingsClient:
        err = _AdminPairingClientExchange(me, inputPtr, inInputLen, &outputPtr, &outputLen, outDone);
        break;

    case kPairingSessionType_AddPairingServer:
    case kPairingSessionType_RemovePairingServer:
    case kPairingSessionType_ListPairingsServer:
        err = _AdminPairingServerExchange(me, inputPtr, inInputLen, &outputPtr, &outputLen, outDone);
        break;

    default:
        pair_ulog(me, kLogLevelWarning, "### Bad pair type: %d\n", me->type);
        err = kStateErr;
        break;
    }
    require_noerr_quiet(err, exit);

    err = _ProgressOutput(me, outputPtr, outputLen, outOutputPtr, outOutputLen, outDone);
    require_noerr_quiet(err, exit);
    outputPtr = NULL;

exit:
    FreeNullSafe(inputStorage);
    FreeNullSafe(outputPtr);
    if (err && (err != kAsyncNoErr))
        _PairingSessionReset(me);
    return (err);
}

//===========================================================================================================================

OSStatus
PairingSessionDeriveKey(
    PairingSessionRef me,
    const void* inSaltPtr,
    size_t inSaltLen,
    const void* inInfoPtr,
    size_t inInfoLen,
    size_t inKeyLen,
    uint8_t* outKey)
{
    OSStatus err;

    switch (me->type) {
    case kPairingSessionType_SetupClient:
    case kPairingSessionType_SetupServer:
        require_action(me->state == kPairSetupStateDone, exit, err = kStateErr);
        require_action(me->srpSharedSecretPtr, exit, err = kStateErr);
        require_action(me->srpSharedSecretLen > 0, exit, err = kStateErr);
        CryptoHKDF(kCryptoHashDescriptor_SHA512, me->srpSharedSecretPtr, me->srpSharedSecretLen, inSaltPtr, inSaltLen,
            inInfoPtr, inInfoLen, inKeyLen, outKey);
        break;

    case kPairingSessionType_VerifyClient:
    case kPairingSessionType_VerifyServer:
        require_action(me->state == kPairVerifyStateDone, exit, err = kStateErr);
        CryptoHKDF(kCryptoHashDescriptor_SHA512, me->sharedSecret, sizeof(me->sharedSecret), inSaltPtr, inSaltLen,
            inInfoPtr, inInfoLen, inKeyLen, outKey);
        break;

    case kPairingSessionType_ResumeClient:
    case kPairingSessionType_ResumeServer:
        require_action(me->state == kPairResumeStateDone, exit, err = kStateErr);
        CryptoHKDF(kCryptoHashDescriptor_SHA512, me->sharedSecret, sizeof(me->sharedSecret), inSaltPtr, inSaltLen,
            inInfoPtr, inInfoLen, inKeyLen, outKey);
        break;

    default:
        err = kUnsupportedErr;
        goto exit;
    }
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================

static OSStatus
_ProgressInput(
    PairingSessionRef me,
    const uint8_t** ioInputPtr,
    size_t* ioInputLen,
    uint8_t** outInputStorage,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone)
{
    const uint8_t* const inputPtr = *ioInputPtr;
    const uint8_t* const inputEnd = inputPtr + *ioInputLen;
    uint8_t* inputStorage = NULL;
    uint8_t* outputPtr = NULL;
    size_t outputLen = 0;
    uint8_t* storage = NULL;
    Boolean done = false;
    OSStatus err;
    const uint8_t* ptr;
    uint8_t* tmp;
    size_t len, newLen;
    Boolean last = false;
    TLV8Buffer tlv;

    TLV8BufferInit(&tlv, kMaxTLVSize);

    // Look for fragment data. If it's not found then it's not fragmented so exit and pass the data through unmodified.

    err = TLV8GetOrCopyCoalesced(inputPtr, inputEnd, kTLVType_FragmentData, &ptr, &len, &storage, NULL);
    if (err == kNotFoundErr) {
        err = TLV8GetOrCopyCoalesced(inputPtr, inputEnd, kTLVType_FragmentLast, &ptr, &len, &storage, NULL);
        last = true;
    }
    require_action_quiet(err != kNotFoundErr, exit, err = kNoErr);
    require_noerr_quiet(err, exit);

    // If it's 0-byte fragment then it's an ack to a previously sent fragment so send our next output fragment.

    if (len == 0) {
        len = me->outputFragmentLen - me->outputFragmentOffset;
        len = Min(len, me->mtuPayload);
        require_action(len > 0, exit, err = kInternalErr);

        last = ((me->outputFragmentOffset + len) == me->outputFragmentLen);
        err = TLV8BufferAppend(&tlv, last ? kTLVType_FragmentLast : kTLVType_FragmentData,
            &me->outputFragmentBuf[me->outputFragmentOffset], len);
        require_noerr(err, exit);
        err = TLV8BufferDetach(&tlv, &outputPtr, &outputLen);
        require_noerr(err, exit);
        check(outputLen <= me->mtuTotal);

        if (last) {
            done = me->outputDone;
            free(me->outputFragmentBuf);
            me->outputFragmentBuf = NULL;
            me->outputFragmentLen = 0;
            me->outputFragmentOffset = 0;
            me->outputDone = false;
        } else {
            me->outputFragmentOffset += len;
        }
        goto exit;
    }

    // Append the new fragment.

    newLen = me->inputFragmentLen + len;
    require_action_quiet(newLen > me->inputFragmentLen, exit, err = kOverrunErr); // Detect wrapping.
    require_action_quiet(newLen <= kMaxTLVSize, exit, err = kSizeErr);
    tmp = (uint8_t*)realloc(me->inputFragmentBuf, newLen);
    require_action(tmp, exit, err = kNoMemoryErr);
    memcpy(&tmp[me->inputFragmentLen], ptr, len);
    ForgetMem(&storage);

    if (last) {
        // It's the last fragment so return the reassembled data as a single input chunk.

        *ioInputPtr = tmp;
        *ioInputLen = newLen;
        inputStorage = tmp;
        me->inputFragmentBuf = NULL;
        me->inputFragmentLen = 0;
    } else {
        // Save the partially reassembled data and output an empty fragment to indicate an ack.

        me->inputFragmentBuf = tmp;
        me->inputFragmentLen = newLen;

        err = TLV8BufferAppend(&tlv, kTLVType_FragmentData, NULL, 0);
        require_noerr(err, exit);
        err = TLV8BufferDetach(&tlv, &outputPtr, &outputLen);
        require_noerr(err, exit);
        check(outputLen <= me->mtuTotal);
    }

exit:
    TLV8BufferFree(&tlv);
    ForgetMem(&storage);
    *outInputStorage = inputStorage;
    *outOutputPtr = outputPtr;
    *outOutputLen = outputLen;
    *outDone = done;
    return (err);
}

//===========================================================================================================================

static OSStatus
_ProgressOutput(
    PairingSessionRef me,
    uint8_t* inOutputPtr,
    size_t inOutputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* ioDone)
{
    OSStatus err;
    TLV8Buffer tlv;

    TLV8BufferInit(&tlv, kMaxTLVSize);

    // If the doesn't need to be fragmented then return it unmodified.

    if (inOutputLen <= me->mtuTotal) {
        *outOutputPtr = inOutputPtr;
        *outOutputLen = inOutputLen;
        err = kNoErr;
        goto exit;
    }

    // Save off the complete buffer for sending in fragments.

    require_action(!me->outputFragmentBuf && (me->outputFragmentLen == 0), exit, err = kExecutionStateErr);
    me->outputFragmentBuf = inOutputPtr;
    me->outputFragmentLen = inOutputLen;
    me->outputFragmentOffset = me->mtuPayload;
    me->outputDone = *ioDone;

    // Return an MTU-sized fragment.

    err = TLV8BufferAppend(&tlv, kTLVType_FragmentData, inOutputPtr, me->mtuPayload);
    require_noerr(err, exit);
    err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
    require_noerr(err, exit);
    check(*outOutputLen <= me->mtuTotal);
    *ioDone = false;

exit:
    TLV8BufferFree(&tlv);
    return (err);
}

//===========================================================================================================================

static OSStatus
_ResumePairingClientExchange(
    PairingSessionRef me,
    const void* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone)
{
    const uint8_t* const inputPtr = (const uint8_t*)inInputPtr;
    const uint8_t* const inputEnd = inputPtr + inInputLen;
    Boolean done = false;
    OSStatus err;
    TLV8Buffer tlv;
    const uint8_t* ptr;
    size_t len;
    uint8_t* eptr = NULL;
    size_t elen;
    uint8_t key[32];
    uint8_t buf[40];

    TLV8BufferInit(&tlv, kMaxTLVSize);

    if (me->state == kPairingStateInvalid)
        me->state = kPairResumeStateM1;
    if (inInputLen > 0) {
        err = TLV8Get(inputPtr, inputEnd, kTLVType_State, &ptr, &len, NULL);
        require_noerr(err, exit);
        require_action(len == 1, exit, err = kSizeErr);
        require_action(*ptr == me->state, exit, err = kStateErr);
    }
    
    pair_ulog(me, kLogLevelWarning, "[%s:%d] Pair-resume client state: %d\n", __func__, __LINE__, me->state);

    switch (me->state) {
    // M1: Controller -> Accessory -- Resume Request.

    case kPairResumeStateM1:
        require_action(inInputLen == 0, exit, err = kParamErr);
        require_action(me->resumeDataPtr, exit, err = kNotPreparedErr);

        ForgetPtrLen(&me->peerIdentifierPtr, &me->peerIdentifierLen);
        err = _PairingFindResumeState(me->resumeSessionID, &me->peerIdentifierPtr, &me->peerIdentifierLen,
            me->sharedSecret, sizeof(me->sharedSecret));
        require_noerr_action_quiet(err, exit, err = kLookErr);

        // Generate new, random ECDH key pair. This is pair-verify-compatible and only used if resume fails.

        err = RandomBytes(me->ourCurveSK, sizeof(me->ourCurveSK));
        require_noerr(err, exit);
        CryptoHKDF(kCryptoHashDescriptor_SHA512, me->ourCurveSK, sizeof(me->ourCurveSK),
            kPairVerifyECDHSaltPtr, kPairVerifyECDHSaltLen,
            kPairVerifyECDHInfoPtr, kPairVerifyECDHInfoLen,
            sizeof(me->ourCurveSK), me->ourCurveSK);
        curve25519(me->ourCurvePK, me->ourCurveSK, NULL);

        // Encrypt the resume payload.

        eptr = (uint8_t*)malloc(me->resumeDataLen + 16);
        require_action(eptr, exit, err = kNoMemoryErr);

        check_compile_time_code(sizeof(buf) >= 40);
        memcpy(&buf[0], me->ourCurvePK, sizeof(me->ourCurvePK));
        WriteLittle64(&buf[32], me->resumeSessionID);
        CryptoHKDF(kCryptoHashDescriptor_SHA512, me->sharedSecret, sizeof(me->sharedSecret),
            buf, 40,
            kPairResumeRequestInfoPtr, kPairResumeRequestInfoLen,
            sizeof(key), key);
        chacha20_poly1305_encrypt_all_64x64(key, (const uint8_t*)"PR-Msg01", NULL, 0,
            me->resumeDataPtr, me->resumeDataLen, eptr, &eptr[me->resumeDataLen]);
        MemZeroSecure(key, sizeof(key));

        // Build the request TLV.

        err = TLV8BufferAppend(&tlv, kTLVType_State, &me->state, sizeof(me->state));
        require_noerr(err, exit);
        err = TLV8BufferAppendUInt64(&tlv, kTLVType_Method, kTLVMethod_Resume);
        require_noerr(err, exit);
        err = TLV8BufferAppendUInt64(&tlv, kTLVType_SessionID, me->resumeSessionID);
        require_noerr(err, exit);
        err = TLV8BufferAppend(&tlv, kTLVType_EncryptedData, eptr, me->resumeDataLen + 16);
        require_noerr(err, exit);
        err = TLV8BufferAppend(&tlv, kTLVType_PublicKey, me->ourCurvePK, sizeof(me->ourCurvePK));
        require_noerr(err, exit);
        err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
        require_noerr(err, exit);

        pair_ulog(me, kLogLevelTrace, "Pair-resume client M1 -- resume request\n%?{end}%1{tlv8}\n",
            !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, *outOutputPtr, (int)*outOutputLen);
        me->state = kPairResumeStateM2;
        break;

    // M2: Accessory -> Controller -- Resume Response.

    case kPairResumeStateM2:
        pair_ulog(me, kLogLevelTrace, "Pair-resume client M2 -- resume response\n%?{end}%1{tlv8}\n",
            !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, inInputPtr, (int)inInputLen);

        err = TLV8GetBytes(inputPtr, inputEnd, kTLVType_Method, 1, 1, &buf[0], NULL, NULL);
        if (err || (buf[0] != kTLVMethod_Resume)) {
            pair_ulog(me, kLogLevelNotice, "Pair-resume client M2 for ID %llu failed %#m...doing pair-verify\n",
                me->resumeSessionID, err);
            me->state = kPairVerifyStateM2;
            me->type = kPairingSessionType_VerifyClient;
            err = _VerifyClientExchange(me, inInputPtr, inInputLen, outOutputPtr, outOutputLen, outDone);
            goto exit;
        }

        me->resumeSessionID = TLV8GetUInt64(inputPtr, inputEnd, kTLVType_SessionID, &err, NULL);
        require_noerr(err, exit);

        // Derive the encryption key.

        check_compile_time_code(sizeof(buf) >= 40);
        memcpy(&buf[0], me->ourCurvePK, sizeof(me->ourCurvePK));
        WriteLittle64(&buf[32], me->resumeSessionID);
        CryptoHKDF(kCryptoHashDescriptor_SHA512, me->sharedSecret, sizeof(me->sharedSecret),
            buf, 40,
            kPairResumeResponseInfoPtr, kPairResumeResponseInfoLen,
            sizeof(key), key);

        // Verify and decrypt the resume response.

        eptr = TLV8CopyCoalesced(inputPtr, inputEnd, kTLVType_EncryptedData, &elen, NULL, &err);
        require_noerr(err, exit);
        require_action(elen >= 16, exit, err = kSizeErr);
        elen -= 16;
        err = chacha20_poly1305_decrypt_all_64x64(key, (const uint8_t*)"PR-Msg02", NULL, 0, eptr, elen,
            eptr, &eptr[elen]);
        MemZeroSecure(key, sizeof(key));
        require_noerr(err, exit);

        if (me->delegate.resumeResponse_f) {
            err = me->delegate.resumeResponse_f(eptr, elen, me->delegate.context);
            require_noerr_quiet(err, exit);
        }

        // Derive a new shared secret for future communication.

        CryptoHKDF(kCryptoHashDescriptor_SHA512, me->sharedSecret, sizeof(me->sharedSecret),
            buf, 40,
            kPairResumeSharedSecretInfoPtr, kPairResumeSharedSecretInfoLen,
            sizeof(me->sharedSecret), me->sharedSecret);
        _PairingSaveResumeState(me, me->peerIdentifierPtr, me->peerIdentifierLen, me->identifierPtr, me->identifierLen,
            me->resumeSessionID, me->sharedSecret, sizeof(me->sharedSecret));

        *outOutputPtr = NULL;
        *outOutputLen = 0;
        me->state = kPairResumeStateDone;
        done = true;
        pair_ulog(me, kLogLevelTrace, "Pair-resume client done\n");
        break;

    default:
        break;
    }
    err = kNoErr;

exit:
    *outDone = done;
    TLV8BufferFree(&tlv);
    ForgetMem(&eptr);
    if (err) {
        pair_ulog(me, kLogLevelNotice, "### Pair-verify client state %d failed: %#m\n%?{end}%1{tlv8}\n",
            me->state, err, !log_category_enabled(me->ucat, kLogLevelInfo), kTLVDescriptors, inInputPtr, (int)inInputLen);
    }
    return (err);
}

//===========================================================================================================================

static OSStatus
_ResumePairingServerExchange(
    PairingSessionRef me,
    const void* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone)
{
    const uint8_t* const inputPtr = (const uint8_t*)inInputPtr;
    const uint8_t* const inputEnd = inputPtr + inInputLen;
    Boolean done = false;
    OSStatus err;
    TLV8Buffer tlv;
    const uint8_t* ptr;
    size_t len;
    uint8_t* eptr = NULL;
    size_t elen;
    uint8_t key[32];
    uint8_t buf[40];
    uint8_t* responsePtr = NULL;
    size_t responseLen = 0;

    TLV8BufferInit(&tlv, kMaxTLVSize);

    err = TLV8Get(inputPtr, inputEnd, kTLVType_State, &ptr, &len, NULL);
    require_noerr(err, exit);
    require_action(len == 1, exit, err = kSizeErr);
    if (*ptr == kPairResumeStateM1)
        _PairingSessionReset(me);
    if (me->state == kPairingStateInvalid)
        me->state = kPairResumeStateM1;

    pair_ulog(me, kLogLevelWarning, "[%s:%d] Pair-resume server state: %d\n", __func__, __LINE__, me->state);
    
    require_action(*ptr == me->state, exit, err = kStateErr);

    switch (me->state) {
    // M1: Controller -> Accessory -- Resume Request.

    case kPairResumeStateM1:
        pair_ulog(me, kLogLevelTrace, "Pair-resume server M1 -- resume request\n%?{end}%1{tlv8}\n",
            !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, inInputPtr, (int)inInputLen);

        err = TLV8GetBytes(inputPtr, inputEnd, kTLVType_Method, 1, 1, &buf[0], NULL, NULL);
        require_noerr(err, exit);
        require_action(buf[0] == kTLVMethod_Resume, exit, err = kCommandErr);

        // Look up the session.

        me->resumeSessionID = TLV8GetUInt64(inputPtr, inputEnd, kTLVType_SessionID, &err, NULL);
        require_noerr(err, exit);
        ForgetPtrLen(&me->peerIdentifierPtr, &me->peerIdentifierLen);
        err = _PairingFindResumeState(me->resumeSessionID, &me->peerIdentifierPtr, &me->peerIdentifierLen,
            me->sharedSecret, sizeof(me->sharedSecret));
        if (err) {
            pair_ulog(me, kLogLevelNotice, "Pair-resume server M1 for ID %llu failed %#m...doing pair-verify\n",
                me->resumeSessionID, err);
            me->state = kPairingStateInvalid;
            me->type = kPairingSessionType_VerifyClient;
            err = _VerifyClientExchange(me, inInputPtr, inInputLen, outOutputPtr, outOutputLen, outDone);
            goto exit;
        }

        // Derive the encryption key.

        err = TLV8GetBytes(inputPtr, inputEnd, kTLVType_PublicKey, 32, 32, me->peerCurvePK, NULL, NULL);
        require_noerr(err, exit);
        check_compile_time_code(sizeof(buf) >= 40);
        memcpy(&buf[0], me->peerCurvePK, sizeof(me->peerCurvePK));
        WriteLittle64(&buf[32], me->resumeSessionID);
        CryptoHKDF(kCryptoHashDescriptor_SHA512, me->sharedSecret, sizeof(me->sharedSecret),
            buf, 40,
            kPairResumeRequestInfoPtr, kPairResumeRequestInfoLen,
            sizeof(key), key);

        // Verify and decrypt the resume request.

        eptr = TLV8CopyCoalesced(inputPtr, inputEnd, kTLVType_EncryptedData, &elen, NULL, &err);
        require_noerr(err, exit);
        require_action(elen >= 16, exit, err = kSizeErr);
        elen -= 16;
        err = chacha20_poly1305_decrypt_all_64x64(key, (const uint8_t*)"PR-Msg01", NULL, 0, eptr, elen,
            eptr, &eptr[elen]);
        MemZeroSecure(key, sizeof(key));
        require_noerr(err, exit);

        // Invalidate the old session and generate a new sessionID.

        _PairingRemoveResumeSessionID(me->resumeSessionID);
        check_compile_time_code(sizeof(buf) >= 40);
        err = RandomBytes(&buf[32], 8);
        require_noerr(err, exit);
        CryptoHKDF(kCryptoHashDescriptor_SHA512, &buf[32], 8,
            kPairResumeSessionIDSaltPtr, kPairResumeSessionIDSaltLen,
            kPairResumeSessionIDInfoPtr, kPairResumeSessionIDInfoLen,
            8, &buf[32]);
        me->resumeSessionID = ReadLittle64(&buf[32]);

        // Process the request and get the response.

        if (me->delegate.resumeRequest_f) {
            err = me->delegate.resumeRequest_f(me->peerIdentifierPtr, me->peerIdentifierLen, eptr, elen,
                &responsePtr, &responseLen, me->delegate.context);
            require_noerr_quiet(err, exit);
        }
        ForgetMem(&eptr);

        // M2: Accessory -> Controller -- Resume Response.

        me->state = kPairResumeStateM2;

        // Encrypt the response data.

        elen = responseLen + 16;
        eptr = (uint8_t*)malloc(elen);
        require_action(eptr, exit, err = kNoMemoryErr);
        if (responseLen > 0)
            memcpy(eptr, responsePtr, responseLen);
        CryptoHKDF(kCryptoHashDescriptor_SHA512, me->sharedSecret, sizeof(me->sharedSecret),
            buf, 40,
            kPairResumeResponseInfoPtr, kPairResumeResponseInfoLen,
            sizeof(key), key);
        chacha20_poly1305_encrypt_all_64x64(key, (const uint8_t*)"PR-Msg02", NULL, 0,
            responsePtr, responseLen, eptr, &eptr[responseLen]);
        MemZeroSecure(key, sizeof(key));

        // Derive a new shared secret for future communication.

        CryptoHKDF(kCryptoHashDescriptor_SHA512, me->sharedSecret, sizeof(me->sharedSecret),
            buf, 40,
            kPairResumeSharedSecretInfoPtr, kPairResumeSharedSecretInfoLen,
            sizeof(me->sharedSecret), me->sharedSecret);
        _PairingSaveResumeState(me, me->peerIdentifierPtr, me->peerIdentifierLen, me->identifierPtr, me->identifierLen,
            me->resumeSessionID, me->sharedSecret, sizeof(me->sharedSecret));

        // Build the request TLV.

        err = TLV8BufferAppend(&tlv, kTLVType_State, &me->state, sizeof(me->state));
        require_noerr(err, exit);
        err = TLV8BufferAppendUInt64(&tlv, kTLVType_Method, kTLVMethod_Resume);
        require_noerr(err, exit);
        err = TLV8BufferAppendUInt64(&tlv, kTLVType_SessionID, me->resumeSessionID);
        require_noerr(err, exit);
        err = TLV8BufferAppend(&tlv, kTLVType_EncryptedData, eptr, elen);
        require_noerr(err, exit);
        err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
        require_noerr(err, exit);

        pair_ulog(me, kLogLevelTrace, "Pair-resume server M2 -- resume response\n%?{end}%1{tlv8}\n",
            !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, *outOutputPtr, *outOutputLen);
        me->state = kPairResumeStateDone;
        done = true;
        pair_ulog(me, kLogLevelTrace, "Pair-resume server done\n");
        break;

    default:
        pair_ulog(me, kLogLevelWarning, "### Pair-resume server bad state: %d\n", me->state);
        err = kStateErr;
        goto exit;
    }
    err = kNoErr;

exit:
    *outDone = done;
    TLV8BufferFree(&tlv);
    ForgetMem(&responsePtr);
    ForgetMem(&eptr);
    if (err) {
        pair_ulog(me, kLogLevelNotice, "### Pair-resume server state %d failed: %#m\n%?{end}%1{tlv8}\n",
            me->state, err, !log_category_enabled(me->ucat, kLogLevelInfo), kTLVDescriptors, inInputPtr, (int)inInputLen);
    }
    return (err);
}

//===========================================================================================================================

static OSStatus
_SetupClientExchange(
    PairingSessionRef me,
    const void* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone)
{
    const uint8_t* const inputPtr = (const uint8_t*)inInputPtr;
    const uint8_t* const inputEnd = inputPtr + inInputLen;
    Boolean done = false;
    OSStatus err;
    TLV8Buffer tlv, etlv;
    const uint8_t* ptr;
    uint8_t* mptr;
    size_t len;
    uint8_t* storage = NULL;
    uint8_t* eptr = NULL;
    size_t elen;
    uint8_t* eend;
    uint8_t* clientPKPtr = NULL;
    size_t clientPKLen = 0;
    uint8_t* responsePtr = NULL;
    size_t responseLen = 0;
    uint8_t sig[64];
#if (PAIRING_MFI_CLIENT)
    const uint8_t* sigPtr;
    size_t sigLen;
    uint8_t* sigStorage = NULL;
    uint8_t msg[32];
#endif
    PairingFlags flags;
    uint64_t u64;

    TLV8BufferInit(&tlv, kMaxTLVSize);
    TLV8BufferInit(&etlv, kMaxTLVSize);
    if (me->state == kPairingStateInvalid)
        me->state = kPairSetupStateM1;

    if (inInputLen > 0) {
        err = TLV8Get(inputPtr, inputEnd, kTLVType_State, &ptr, &len, NULL);
        require_noerr(err, exit);
        require_action(len == 1, exit, err = kSizeErr);
        require_action(*ptr == me->state, exit, err = kStateErr);
    }
    pair_ulog(me, kLogLevelWarning, "[%s:%d] Pair-setup client state: %d\n", __func__, __LINE__, me->state);

    switch (me->state) {
    // M1: Controller -> Accessory -- Start Request.

    case kPairSetupStateM1:
        require_action(inInputLen == 0, exit, err = kParamErr);

        if (0) {
        }
#if (PAIRING_MFI_CLIENT)
        else if (me->flags & kPairingFlag_MFi)
            u64 = kTLVMethod_MFiPairSetup;
#else
        else if (me->flags & kPairingFlag_MFi) {
            dlogassert("No MFi support");
            err = kUnsupportedErr;
            goto exit;
        }
#endif
        else
            u64 = kTLVMethod_PairSetup;
        err = TLV8BufferAppendUInt64(&tlv, kTLVType_Method, u64);
        require_noerr(err, exit);
        err = TLV8BufferAppend(&tlv, kTLVType_State, &me->state, sizeof(me->state));
        require_noerr(err, exit);
        if (me->flags & kPairingFlag_NonLegacy) {
            u64 = me->flags & kPairingFlag_RemoteSafe;
            if (u64) {
                err = TLV8BufferAppendUInt64(&tlv, kTLVType_Flags, u64);
                require_noerr(err, exit);
            }
        }
        err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
        require_noerr(err, exit);

        me->state = kPairSetupStateM2;

        pair_ulog(me, kLogLevelTrace, "Pair-setup client M1 -- start request\n%?{end}%1{tlv8}\n",
            !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, *outOutputPtr, (int)*outOutputLen);
        break;

    // M2: Accessory -> Controller -- Start Response.

    case kPairSetupStateM2:
        pair_ulog(me, kLogLevelTrace, "Pair-setup client M2 -- start response\n%?{end}%1{tlv8}\n",
            !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, inInputPtr, (int)inInputLen);

        err = TLV8Get(inputPtr, inputEnd, kTLVType_Error, &ptr, &len, NULL);
        if (!err) {
            // If the accessory returned a back-off error then tell the delegate to retry at the specified time.

            require_action(len == 1, exit, err = kSizeErr);
            if (*ptr == kTLVError_Backoff) {
                u64 = TLV8GetUInt64(inputPtr, inputEnd, kTLVType_RetryDelay, &err, NULL);
                require_noerr(err, exit);
                require_action(u64 <= INT32_MAX, exit, err = kRangeErr);

                _PairingSessionReset(me);

                require_action_quiet(me->delegate.promptForSetupCode_f, exit, err = kNotPreparedErr);
                flags = me->flags | kPairingFlag_Throttle;
                err = me->delegate.promptForSetupCode_f(flags, (int32_t)u64, me->delegate.context);
                require_noerr_quiet(err, exit);
                err = kAsyncNoErr;
                goto exit;
            }
            err = PairingStatusToOSStatus(*ptr);
            pair_ulog(me, kLogLevelNotice, "### Pair-setup client M2 bad status: 0x%X, %#m\n", *ptr, err);
            goto exit;
        }

        SRPForget(&me->srpCtx);
        err = SRPCreate(&me->srpCtx);
        require_noerr(err, exit);

        ForgetPtrLen(&me->srpSaltPtr, &me->srpSaltLen);
        me->srpSaltPtr = TLV8CopyCoalesced(inputPtr, inputEnd, kTLVType_Salt, &me->srpSaltLen, NULL, &err);
        require_noerr(err, exit);
        require_action(me->srpSaltLen >= 16, exit, err = kSizeErr);

        ForgetPtrLen(&me->srpPKPtr, &me->srpPKLen);
        me->srpPKPtr = TLV8CopyCoalesced(inputPtr, inputEnd, kTLVType_PublicKey, &me->srpPKLen, NULL, &err);
        require_noerr(err, exit);
        require_action(me->srpPKLen > 0, exit, err = kSizeErr);

        me->state = kPairSetupStateM3;

        // Ask the delegate to prompt the user and call us again after it sets it.

        if (!me->setupCodePtr || (me->setupCodeLen == 0)) {
            flags = me->flags;
            if (me->setupCodeFailed)
                flags |= kPairingFlag_Incorrect;
            require_action_quiet(me->delegate.promptForSetupCode_f, exit, err = kNotPreparedErr);
            err = me->delegate.promptForSetupCode_f(flags, -1, me->delegate.context);
            require_noerr_quiet(err, exit);
            require_action_quiet(me->setupCodePtr && (me->setupCodeLen > 0), exit, err = kAsyncNoErr);
        }
        inInputLen = 0;
        FALLTHROUGH;

    // M3: Controller -> Accessory -- Verify Request.

    case kPairSetupStateM3:
        require_action(inInputLen == 0, exit, err = kParamErr);
        require_action(me->setupCodePtr, exit, err = kNotPreparedErr);
        require_action(me->setupCodeLen > 0, exit, err = kNotPreparedErr);
        require_action(me->srpCtx, exit, err = kExecutionStateErr);
        require_action(me->srpPKPtr, exit, err = kExecutionStateErr);
        require_action(me->srpPKLen > 0, exit, err = kExecutionStateErr);
        require_action(me->srpSaltPtr, exit, err = kExecutionStateErr);
        require_action(me->srpSaltLen > 0, exit, err = kExecutionStateErr);

        ForgetPtrLen(&me->srpSharedSecretPtr, &me->srpSharedSecretLen);
        err = SRPClientStart(me->srpCtx, kSRPParameters_3072_SHA512, kPairSetupSRPUsernamePtr, kPairSetupSRPUsernameLen,
            me->setupCodePtr, me->setupCodeLen, me->srpSaltPtr, me->srpSaltLen, me->srpPKPtr, me->srpPKLen,
            &clientPKPtr, &clientPKLen, &me->srpSharedSecretPtr, &me->srpSharedSecretLen, &responsePtr, &responseLen);
        require_noerr(err, exit);
        ForgetPtrLen(&me->srpPKPtr, &me->srpPKLen);
        ForgetPtrLen(&me->srpSaltPtr, &me->srpSaltLen);

        err = TLV8BufferAppend(&tlv, kTLVType_State, &me->state, sizeof(me->state));
        require_noerr(err, exit);
        err = TLV8BufferAppend(&tlv, kTLVType_PublicKey, clientPKPtr, clientPKLen);
        require_noerr(err, exit);
        err = TLV8BufferAppend(&tlv, kTLVType_Proof, responsePtr, responseLen);
        require_noerr(err, exit);
        err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
        require_noerr(err, exit);

        me->state = kPairSetupStateM4;

        pair_ulog(me, kLogLevelTrace, "Pair-setup  client M3 -- verify request\n%?{end}%1{tlv8}\n",
            !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, *outOutputPtr, (int)*outOutputLen);
        break;

    // M4: Accessory -> Controller -- Verify Response.

    case kPairSetupStateM4:
        require_action(me->srpCtx, exit, err = kExecutionStateErr);
        require_action(me->srpSharedSecretPtr, exit, err = kExecutionStateErr);
        require_action(me->srpSharedSecretLen > 0, exit, err = kExecutionStateErr);

        pair_ulog(me, kLogLevelTrace, "Pair-setup client M4 -- verify response\n%?{end}%1{tlv8}\n",
            !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, inInputPtr, (int)inInputLen);

        err = TLV8Get(inputPtr, inputEnd, kTLVType_Error, &ptr, &len, NULL);
        if (!err) {
            // If the accessory said the setup code was incorrect then tell the delegate so it can tell the user.

            require_action(len == 1, exit, err = kSizeErr);
            if (*ptr == kTLVError_Authentication) {
                pair_ulog(me, kLogLevelTrace, "### Pair-setup client wrong setup code\n");
                _PairingSessionReset(me);
                ForgetPtrLen(&me->setupCodePtr, &me->setupCodeLen);
                me->setupCodeFailed = true;

                require_action_quiet(me->delegate.promptForSetupCode_f, exit, err = kNotPreparedErr);
                err = me->delegate.promptForSetupCode_f(me->flags | kPairingFlag_Incorrect, -1, me->delegate.context);
                require_noerr_quiet(err, exit);
                err = kAsyncNoErr;
                goto exit;
            }
            err = PairingStatusToOSStatus(*ptr);
            pair_ulog(me, kLogLevelNotice, "### Pair-setup client M4 bad status: 0x%X, %#m\n", *ptr, err);
            goto exit;
        }

        err = TLV8GetOrCopyCoalesced(inputPtr, inputEnd, kTLVType_Proof, &ptr, &len, &storage, NULL);
        require_noerr(err, exit);
        err = SRPClientVerify(me->srpCtx, ptr, len);
        ForgetMem(&storage);
        require_noerr_action_quiet(err, exit, err = kAuthenticationErr);
        SRPForget(&me->srpCtx);

        CryptoHKDF(kCryptoHashDescriptor_SHA512, me->srpSharedSecretPtr, me->srpSharedSecretLen,
            kPairSetupEncryptSaltPtr, kPairSetupEncryptSaltLen,
            kPairSetupEncryptInfoPtr, kPairSetupEncryptInfoLen,
            sizeof(me->key), me->key);

#if (PAIRING_MFI_CLIENT)
        if (me->flags & kPairingFlag_MFi) {
            CFDataRef data;

            // Verify and decrypt sub-TLV.

            eptr = TLV8CopyCoalesced(inputPtr, inputEnd, kTLVType_EncryptedData, &elen, NULL, &err);
            require_noerr(err, exit);
            require_action(elen > 16, exit, err = kSizeErr);
            elen -= 16;
            eend = eptr + elen;
            err = chacha20_poly1305_decrypt_all_64x64(me->key, (const uint8_t*)"PS-Msg04", NULL, 0,
                eptr, elen, eptr, eend);
            require_noerr(err, exit);

            // Verify the MFi auth IC signature of a challenge derived from the SRP shared secret.

            err = TLV8GetOrCopyCoalesced(eptr, eend, kTLVType_Signature, &sigPtr, &sigLen, &sigStorage, NULL);
            require_noerr(err, exit);
            err = TLV8GetOrCopyCoalesced(eptr, eend, kTLVType_Certificate, &ptr, &len, &storage, NULL);
            require_noerr(err, exit);

            check_compile_time_code(sizeof(msg) >= 32);
            CryptoHKDF(kCryptoHashDescriptor_SHA512, me->srpSharedSecretPtr, me->srpSharedSecretLen,
                kMFiPairSetupSaltPtr, kMFiPairSetupSaltLen,
                kMFiPairSetupInfoPtr, kMFiPairSetupInfoLen,
                32, msg);
            err = MFiPlatform_VerifySignature(msg, 32, sigPtr, sigLen, ptr, len);
            require_noerr(err, exit);
            ForgetMem(&eptr);
            ForgetMem(&sigStorage);

            // Save the certificate so the user can perform additional validation of it (e.g. fraud checks).

            data = CFDataCreate(NULL, ptr, (CFIndex)len);
            ForgetMem(&storage);
            require_action(data, exit, err = kNoMemoryErr);
            err = PairingSessionSetProperty(me, kPairingSessionPropertyCertificate, data);
            CFRelease(data);
            require_noerr(err, exit);
        }
#endif

        if (me->flags & kPairingFlag_Transient) {
            me->state = kPairSetupStateDone;
            *outOutputPtr = NULL;
            *outOutputLen = 0;
            done = true;
            pair_ulog(me, kLogLevelTrace, "Pair-setup transient client done -- server authenticated\n");
            break;
        }
        me->state = kPairSetupStateM5;

        // M5: Controller -> Accessory -- Exchange Request.

        ForgetPtrLen(&me->activeIdentifierPtr, &me->activeIdentifierLen);
        err = PairingSessionCopyIdentity(me, true, &me->activeIdentifierPtr, me->ourEdPK, me->ourEdSK);
        require_noerr_quiet(err, exit);
        me->activeIdentifierLen = strlen(me->activeIdentifierPtr);
        require_action(me->activeIdentifierLen > 0, exit, err = kIDErr);

        // Generate signature of controller's info.

        len = 32 + me->activeIdentifierLen + 32;
        storage = (uint8_t*)malloc(len);
        require_action(storage, exit, err = kNoMemoryErr);
        CryptoHKDF(kCryptoHashDescriptor_SHA512, me->srpSharedSecretPtr, me->srpSharedSecretLen,
            kPairSetupControllerSignSaltPtr, kPairSetupControllerSignSaltLen,
            kPairSetupControllerSignInfoPtr, kPairSetupControllerSignInfoLen,
            32, &storage[0]);
        memcpy(&storage[32], me->activeIdentifierPtr, me->activeIdentifierLen);
        memcpy(&storage[32 + me->activeIdentifierLen], me->ourEdPK, 32);
        Ed25519_sign(sig, storage, len, me->ourEdPK, me->ourEdSK);
        ForgetMem(&storage);

        // Build sub-TLV of controller's info and encrypt it.

        err = TLV8BufferAppend(&etlv, kTLVType_Identifier, me->activeIdentifierPtr, me->activeIdentifierLen);
        require_noerr(err, exit);
        err = TLV8BufferAppend(&etlv, kTLVType_PublicKey, me->ourEdPK, 32);
        require_noerr(err, exit);
        err = TLV8BufferAppend(&etlv, kTLVType_Signature, sig, 64);
        require_noerr(err, exit);
        if (me->acl) {
            CFDataRef data = OPACKEncoderCreateData(me->acl, kOPACKFlag_None, &err);
            require_noerr(err, exit);
            require_action(data, exit, err = kInternalErr);
            err = TLV8BufferAppend(&etlv, kTLVType_ACL, CFDataGetBytePtr(data), (size_t)CFDataGetLength(data));
            CFRelease(data);
            require_noerr(err, exit);
        }
        if (me->ourExtraDataPtr) {
            err = TLV8BufferAppend(&etlv, kTLVType_ExtraData, me->ourExtraDataPtr, me->ourExtraDataLen);
            require_noerr(err, exit);
        }

        storage = (uint8_t*)malloc(etlv.len + 16);
        require_action(storage, exit, err = kNoMemoryErr);
        chacha20_poly1305_encrypt_all_64x64(me->key, (const uint8_t*)"PS-Msg05", NULL, 0,
            etlv.ptr, etlv.len, storage, &storage[etlv.len]);
        err = TLV8BufferAppend(&tlv, kTLVType_EncryptedData, storage, etlv.len + 16);
        require_noerr(err, exit);
        ForgetMem(&storage);

        err = TLV8BufferAppend(&tlv, kTLVType_State, &me->state, sizeof(me->state));
        require_noerr(err, exit);
        err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
        require_noerr(err, exit);

        me->state = kPairSetupStateM6;

        pair_ulog(me, kLogLevelTrace, "Pair-setup client M5 -- exchange request\n%?{end}%1{tlv8}\n",
            !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, *outOutputPtr, (int)*outOutputLen);
        break;

    // M6: Accessory -> Controller -- Exchange Response.

    case kPairSetupStateM6:
        require_action(me->srpSharedSecretPtr, exit, err = kExecutionStateErr);
        require_action(me->srpSharedSecretLen > 0, exit, err = kExecutionStateErr);

        pair_ulog(me, kLogLevelTrace, "Pair-setup client M6 -- exchange response\n%?{end}%1{tlv8}\n",
            !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, inInputPtr, (int)inInputLen);

        err = TLV8Get(inputPtr, inputEnd, kTLVType_Error, &ptr, &len, NULL);
        if (!err) {
            require_action(len == 1, exit, err = kSizeErr);
            err = PairingStatusToOSStatus(*ptr);
            pair_ulog(me, kLogLevelNotice, "### Pair-setup client M6 bad status: 0x%X, %#m\n", *ptr, err);
            goto exit;
        }

        // Verify and decrypt sub-TLV.

        eptr = TLV8CopyCoalesced(inputPtr, inputEnd, kTLVType_EncryptedData, &elen, NULL, &err);
        require_noerr(err, exit);
        require_action(elen > 16, exit, err = kSizeErr);
        elen -= 16;
        eend = eptr + elen;
        err = chacha20_poly1305_decrypt_all_64x64(me->key, (const uint8_t*)"PS-Msg06", NULL, 0, eptr, elen, eptr, eend);
        require_noerr(err, exit);

        FreeNullSafe(me->peerExtraDataPtr);
        me->peerExtraDataLen = 0;
        me->peerExtraDataPtr = TLV8CopyCoalesced(eptr, eend, kTLVType_ExtraData, &me->peerExtraDataLen, NULL, NULL);

        CFForget(&me->peerInfo);
        mptr = TLV8CopyCoalesced(eptr, eend, kTLVType_Info, &len, NULL, NULL);
        if (mptr) {
            me->peerInfo = (CFDictionaryRef)OPACKDecodeBytes(mptr, len, kOPACKFlag_None, &err);
            free(mptr);
            require_noerr(err, exit);
            require_action(me->peerInfo, exit, err = kInternalErr);
            require_action(CFIsType(me->peerInfo, CFDictionary), exit, err = kTypeErr; CFForget(&me->peerInfo));
        }

        // Verify signature of accessory's info.

        ForgetPtrLen(&me->peerIdentifierPtr, &me->peerIdentifierLen);
        me->peerIdentifierPtr = TLV8CopyCoalesced(eptr, eend, kTLVType_Identifier, &me->peerIdentifierLen, NULL, &err);
        require_noerr(err, exit);
        require_action(me->peerIdentifierLen > 0, exit, err = kSizeErr);

        err = TLV8GetBytes(eptr, eend, kTLVType_PublicKey, 32, 32, me->peerEdPK, NULL, NULL);
        require_noerr(err, exit);

        err = TLV8GetBytes(eptr, eend, kTLVType_Signature, 64, 64, sig, NULL, NULL);
        require_noerr(err, exit);

        len = 32 + me->peerIdentifierLen + 32;
        storage = (uint8_t*)malloc(len);
        require_action(storage, exit, err = kNoMemoryErr);
        CryptoHKDF(kCryptoHashDescriptor_SHA512, me->srpSharedSecretPtr, me->srpSharedSecretLen,
            kPairSetupAccessorySignSaltPtr, kPairSetupAccessorySignSaltLen,
            kPairSetupAccessorySignInfoPtr, kPairSetupAccessorySignInfoLen,
            32, &storage[0]);
        memcpy(&storage[32], me->peerIdentifierPtr, me->peerIdentifierLen);
        memcpy(&storage[32 + me->peerIdentifierLen], me->peerEdPK, 32);

        err = Ed25519_verify(storage, len, sig, me->peerEdPK);
        require_noerr_action_quiet(err, exit, err = kAuthenticationErr);
        ForgetMem(&storage);
        ForgetMem(&eptr);

        err = PairingSessionSavePeer(me, me->peerIdentifierPtr, me->peerIdentifierLen, me->peerEdPK);
        require_noerr_quiet(err, exit);

        ForgetPtrLen(&me->setupCodePtr, &me->setupCodeLen);
        me->setupCodeFailed = false;

        me->state = kPairSetupStateDone;
        *outOutputPtr = NULL;
        *outOutputLen = 0;
        done = true;
        pair_ulog(me, kLogLevelTrace, "Pair-setup client done -- server authenticated\n");
        break;

    default:
        pair_ulog(me, kLogLevelWarning, "### Pair-setup client bad state: %d\n", me->state);
        err = kStateErr;
        goto exit;
    }
    err = kNoErr;

exit:
    *outDone = done;
    TLV8BufferFree(&tlv);
    TLV8BufferFree(&etlv);
    ForgetMem(&storage);
    ForgetMem(&eptr);
    ForgetPtrLen(&clientPKPtr, &clientPKLen);
    ForgetPtrLen(&responsePtr, &responseLen);
#if (PAIRING_MFI_CLIENT)
    ForgetMem(&sigStorage);
#endif
    if (err && (err != kAsyncNoErr)) {
        pair_ulog(me, kLogLevelNotice, "### Pair-setup client state %d failed: %#m\n%?{end}%1{tlv8}\n",
            me->state, err, !log_category_enabled(me->ucat, kLogLevelInfo), kTLVDescriptors, inInputPtr, (int)inInputLen);
    }
    return (err);
}

//===========================================================================================================================

static OSStatus
_SetupServerExchange(
    PairingSessionRef me,
    const void* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone)
{
    const uint8_t* const inputPtr = (const uint8_t*)inInputPtr;
    const uint8_t* const inputEnd = inputPtr + inInputLen;
    Boolean done = false;
    OSStatus err;
    TLV8Buffer tlv, etlv;
    const uint8_t* ptr;
    uint8_t* mptr;
    size_t len;
    uint8_t* storage = NULL;
    uint8_t* eptr = NULL;
    size_t elen;
    uint8_t* eend;
    uint8_t* serverPKPtr = NULL;
    size_t serverPKLen = 0;
    const uint8_t* clientPKPtr;
    size_t clientPKLen;
    uint8_t* clientPKStorage = NULL;
    const uint8_t* proofPtr;
    size_t proofLen;
    uint8_t* proofStorage = NULL;
    uint8_t* responsePtr = NULL;
    size_t responseLen = 0;
    char tempStr[64];
    uint8_t salt[16];
    uint8_t sig[64];
#if (PAIRING_MFI_SERVER)
    uint8_t msg[32];
#endif
    uint8_t u8;
    int32_t s32;
#if (PAIRING_MFI_SERVER)
    uint8_t digest[20];
#endif

    TLV8BufferInit(&tlv, kMaxTLVSize);
    TLV8BufferInit(&etlv, kMaxTLVSize);

    err = TLV8Get(inputPtr, inputEnd, kTLVType_State, &ptr, &len, NULL);
    require_noerr(err, exit);
    require_action(len == 1, exit, err = kSizeErr);
    if (*ptr == kPairSetupStateM1)
        _PairingSessionReset(me);
    if (me->state == kPairingStateInvalid)
        me->state = kPairSetupStateM1;

    pair_ulog(me, kLogLevelWarning, "[%s:%d] Pair-setup server state: %d, *ptr=%d\n", __func__, __LINE__, me->state, *ptr);
    
    require_action(*ptr == me->state, exit, err = kStateErr);

    switch (me->state) {
    // M1: Controller -> Accessory -- Start Request.

    case kPairSetupStateM1:
        pair_ulog(me, kLogLevelWarning, "[%s:%d] Pair-setup server state M1\n", __func__, __LINE__);
    
        pair_ulog(me, kLogLevelTrace, "Pair-setup server M1 -- start request\n%?{end}%1{tlv8}\n",
            !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, inInputPtr, (int)inInputLen);

        err = TLV8GetBytes(inputPtr, inputEnd, kTLVType_Method, 1, 1, &u8, NULL, NULL);
        require_noerr(err, exit);
        if (u8 == kTLVMethod_PairSetup) {
        }
#if (PAIRING_MFI_SERVER)
        else if (u8 == kTLVMethod_MFiPairSetup) {
        }
#endif
        else {
            pair_ulog(me, kLogLevelNotice, "### Pair-setup server unsupported method: %u\n", u8);
            err = kUnsupportedErr;
            goto exit;
        }
        me->requestMethod = u8;
        me->requestFlags = (PairingFlags)TLV8GetUInt64(inputPtr, inputEnd, kTLVType_Flags, NULL, NULL);
        me->requestFlags &= kPairingFlag_RemoteSafe;

        s32 = _PairingThrottle();
        if (s32 >= 0) {
            pair_ulog(me, kLogLevelNotice, "Pair-setup server throttling for %d second(s)\n", s32);
            err = TLV8BufferAppendUInt64(&tlv, kTLVType_Error, kTLVError_Backoff);
            require_noerr(err, exit);
            err = TLV8BufferAppendUInt64(&tlv, kTLVType_RetryDelay, (uint32_t)s32);
            require_noerr(err, exit);
            err = TLV8BufferAppendUInt64(&tlv, kTLVType_State, kPairSetupStateM2);
            require_noerr(err, exit);
            err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
            require_noerr(err, exit);
            goto exit;
        } else if (s32 == kCountErr) {
            pair_ulog(me, kLogLevelNotice, "### Pair-setup server disabled after too many attempts\n");
            err = TLV8BufferAppendUInt64(&tlv, kTLVType_Error, kTLVError_MaxTries);
            require_noerr(err, exit);
            err = TLV8BufferAppendUInt64(&tlv, kTLVType_State, kPairSetupStateM2);
            require_noerr(err, exit);
            err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
            require_noerr(err, exit);
            goto exit;
        }

        me->state = kPairSetupStateM2;

        // M2: Accessory -> Controller -- Start Response.

        if (!me->setupCodePtr || (me->setupCodeLen == 0)) {
            require_action(me->delegate.showSetupCode_f, exit, err = kNotPreparedErr);
            err = me->delegate.showSetupCode_f(me->requestFlags, tempStr, sizeof(tempStr), me->delegate.context);
            require_noerr_quiet(err, exit);
            me->showingSetupCode = true;
            require_action(strlen(tempStr) >= 4, exit, err = kSizeErr);

            err = PairingSessionSetSetupCode(me, tempStr, kSizeCString);
            require_noerr(err, exit);
        }

        SRPForget(&me->srpCtx);
        err = SRPCreate(&me->srpCtx);
        require_noerr(err, exit);

        err = SRPServerStart(me->srpCtx, kSRPParameters_3072_SHA512, kPairSetupSRPUsernamePtr, kPairSetupSRPUsernameLen,
            me->setupCodePtr, me->setupCodeLen, sizeof(salt), salt, &serverPKPtr, &serverPKLen);
        require_noerr(err, exit);

        err = TLV8BufferAppend(&tlv, kTLVType_State, &me->state, sizeof(me->state));
        require_noerr(err, exit);
        err = TLV8BufferAppend(&tlv, kTLVType_Salt, salt, sizeof(salt));
        require_noerr(err, exit);
        err = TLV8BufferAppend(&tlv, kTLVType_PublicKey, serverPKPtr, serverPKLen);
        require_noerr(err, exit);
        err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
        require_noerr(err, exit);

        me->state = kPairSetupStateM3;

        pair_ulog(me, kLogLevelTrace, "Pair-setup server M2 -- start response\n%?{end}%1{tlv8}\n",
            !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, *outOutputPtr, *outOutputLen);
        break;

    // M3: Controller -> Accessory -- Verify Request.

    case kPairSetupStateM3:
        pair_ulog(me, kLogLevelWarning, "[%s:%d] Pair-setup server state M3\n", __func__, __LINE__);
        
        require_action(me->srpCtx, exit, err = kExecutionStateErr);

        pair_ulog(me, kLogLevelTrace, "Pair-setup server M3 -- verify request\n%?{end}%1{tlv8}\n",
            !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, inInputPtr, (int)inInputLen);

        err = TLV8GetOrCopyCoalesced(inputPtr, inputEnd, kTLVType_PublicKey, &clientPKPtr, &clientPKLen,
            &clientPKStorage, NULL);
        require_noerr(err, exit);

        err = TLV8GetOrCopyCoalesced(inputPtr, inputEnd, kTLVType_Proof, &proofPtr, &proofLen, &proofStorage, NULL);
        require_noerr(err, exit);

        ForgetPtrLenSecure(&me->srpSharedSecretPtr, &me->srpSharedSecretLen);
        err = SRPServerVerify(me->srpCtx, clientPKPtr, clientPKLen, proofPtr, proofLen,
            &me->srpSharedSecretPtr, &me->srpSharedSecretLen, &responsePtr, &responseLen);
        if (err) {
            pair_ulog(me, kLogLevelTrace, "### Pair-setup server wrong setup code\n");

            err = TLV8BufferAppendUInt64(&tlv, kTLVType_Error, kTLVError_Authentication);
            require_noerr(err, exit);
            err = TLV8BufferAppendUInt64(&tlv, kTLVType_State, kPairSetupStateM4);
            require_noerr(err, exit);
            err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
            require_noerr(err, exit);

            _PairingSessionReset(me);
            goto exit;
        }

        if (me->delegate.hideSetupCode_f)
            me->delegate.hideSetupCode_f(me->delegate.context);
        me->showingSetupCode = false;
        ForgetPtrLen(&me->setupCodePtr, &me->setupCodeLen);
        _PairingResetThrottle();

        me->state = kPairSetupStateM4;

        // M4: Accessory -> Controller -- Verify Response.

        err = TLV8BufferAppend(&tlv, kTLVType_State, &me->state, sizeof(me->state));
        require_noerr(err, exit);
        err = TLV8BufferAppend(&tlv, kTLVType_Proof, responsePtr, responseLen);
        require_noerr(err, exit);
        SRPForget(&me->srpCtx);

        CryptoHKDF(kCryptoHashDescriptor_SHA512, me->srpSharedSecretPtr, me->srpSharedSecretLen,
            kPairSetupEncryptSaltPtr, kPairSetupEncryptSaltLen,
            kPairSetupEncryptInfoPtr, kPairSetupEncryptInfoLen,
            sizeof(me->key), me->key);

#if (PAIRING_MFI_SERVER)
        if (me->requestMethod == kTLVMethod_MFiPairSetup) {
            // Use the MFi auth IC to sign a challenge derived from the SRP shared secret.

            check_compile_time_code(sizeof(msg) >= 32);
            CryptoHKDF(kCryptoHashDescriptor_SHA512, me->srpSharedSecretPtr, me->srpSharedSecretLen,
                kMFiPairSetupSaltPtr, kMFiPairSetupSaltLen,
                kMFiPairSetupInfoPtr, kMFiPairSetupInfoLen,
                32, msg);
            SHA1(msg, 32, digest);
            err = MFiPlatform_CreateSignature(digest, sizeof(digest), &storage, &len);
            require_noerr(err, exit);
            err = TLV8BufferAppend(&etlv, kTLVType_Signature, storage, len);
            require_noerr(err, exit);
            ForgetMem(&storage);

            err = MFiPlatform_CopyCertificate(&storage, &len);
            require_noerr(err, exit);
            err = TLV8BufferAppend(&etlv, kTLVType_Certificate, storage, len);
            require_noerr(err, exit);
            ForgetMem(&storage);

            storage = (uint8_t*)malloc(etlv.len + 16);
            require_action(storage, exit, err = kNoMemoryErr);
            chacha20_poly1305_encrypt_all_64x64(me->key, (const uint8_t*)"PS-Msg04", NULL, 0,
                etlv.ptr, etlv.len, storage, &storage[etlv.len]);
            err = TLV8BufferAppend(&tlv, kTLVType_EncryptedData, storage, etlv.len + 16);
            require_noerr(err, exit);
            ForgetMem(&storage);
        }
#endif

        err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
        require_noerr(err, exit);

        pair_ulog(me, kLogLevelTrace, "Pair-setup server M4 -- verify response\n%?{end}%1{tlv8}\n",
            !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, *outOutputPtr, *outOutputLen);

        if (me->flags & kPairingFlag_Transient) {
            me->state = kPairSetupStateDone;
            done = true;
            pair_ulog(me, kLogLevelTrace, "Pair-setup transient server done -- client authenticated\n");
            break;
        }
        me->state = kPairSetupStateM5;
        break;

    // M5: Controller -> Accessory -- Exchange Request.

    case kPairSetupStateM5:
        pair_ulog(me, kLogLevelWarning, "[%s:%d] Pair-setup server state M5\n", __func__, __LINE__);
        require_action(me->srpSharedSecretPtr, exit, err = kExecutionStateErr);
        require_action(me->srpSharedSecretLen > 0, exit, err = kExecutionStateErr);

        pair_ulog(me, kLogLevelTrace, "Pair-setup server M5 -- exchange request\n%?{end}%1{tlv8}\n",
            !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, inInputPtr, (int)inInputLen);

        // Verify and decrypt sub-TLV.

        eptr = TLV8CopyCoalesced(inputPtr, inputEnd, kTLVType_EncryptedData, &elen, NULL, &err);
        require_noerr(err, exit);
        require_action(elen > 16, exit, err = kSizeErr);
        elen -= 16;
        eend = eptr + elen;
        err = chacha20_poly1305_decrypt_all_64x64(me->key, (const uint8_t*)"PS-Msg05", NULL, 0, eptr, elen, eptr, eend);
        require_noerr(err, exit);

        CFForget(&me->peerACL);
        mptr = TLV8CopyCoalesced(eptr, eend, kTLVType_ACL, &len, NULL, NULL);
        if (mptr) {
            CFDictionaryRef peerACL;

            peerACL = (CFDictionaryRef)OPACKDecodeBytes(mptr, len, kOPACKFlag_None, &err);
            free(mptr);
            require_noerr(err, exit);
            require_action(peerACL, exit, err = kInternalErr);
            require_action(CFIsType(peerACL, CFDictionary), exit, err = kTypeErr; CFRelease(peerACL));

            me->peerACL = _PairingSessionIntersectACL(me, peerACL, &err);
            if (err) {
                pair_ulog(me, kLogLevelNotice, "### Pair-verify server M5 requested ACL not allowed: %#m, %@\n",
                    err, peerACL);
                CFRelease(peerACL);
                err = TLV8BufferAppendUInt64(&tlv, kTLVType_Error, OSStatusToPairingStatus(err));
                require_noerr(err, exit);
                err = TLV8BufferAppendUInt64(&tlv, kTLVType_State, kPairSetupStateM6);
                require_noerr(err, exit);
                err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
                require_noerr(err, exit);
                goto exit;
            }
            CFRelease(peerACL);
        }

        FreeNullSafe(me->peerExtraDataPtr);
        me->peerExtraDataLen = 0;
        me->peerExtraDataPtr = TLV8CopyCoalesced(eptr, eend, kTLVType_ExtraData, &me->peerExtraDataLen, NULL, NULL);

        CFForget(&me->peerInfo);
        mptr = TLV8CopyCoalesced(eptr, eend, kTLVType_Info, &len, NULL, NULL);
        if (mptr) {
            me->peerInfo = (CFDictionaryRef)OPACKDecodeBytes(mptr, len, kOPACKFlag_None, &err);
            free(mptr);
            require_noerr(err, exit);
            require_action(me->peerInfo, exit, err = kInternalErr);
            require_action(CFIsType(me->peerInfo, CFDictionary), exit, err = kTypeErr; CFForget(&me->peerInfo));
        }

        // Verify signature of controller's info.

        ForgetPtrLen(&me->peerIdentifierPtr, &me->peerIdentifierLen);
        me->peerIdentifierPtr = TLV8CopyCoalesced(eptr, eend, kTLVType_Identifier, &me->peerIdentifierLen, NULL, &err);
        require_noerr(err, exit);
        require_action(me->peerIdentifierLen > 0, exit, err = kSizeErr);

        err = TLV8GetBytes(eptr, eend, kTLVType_PublicKey, 32, 32, me->peerEdPK, NULL, NULL);
        require_noerr(err, exit);

        err = TLV8GetBytes(eptr, eend, kTLVType_Signature, 64, 64, sig, NULL, NULL);
        require_noerr(err, exit);

        len = 32 + me->peerIdentifierLen + 32;
        storage = (uint8_t*)malloc(len);
        require_action(storage, exit, err = kNoMemoryErr);
        CryptoHKDF(kCryptoHashDescriptor_SHA512, me->srpSharedSecretPtr, me->srpSharedSecretLen,
            kPairSetupControllerSignSaltPtr, kPairSetupControllerSignSaltLen,
            kPairSetupControllerSignInfoPtr, kPairSetupControllerSignInfoLen,
            32, &storage[0]);
        memcpy(&storage[32], me->peerIdentifierPtr, me->peerIdentifierLen);
        memcpy(&storage[32 + me->peerIdentifierLen], me->peerEdPK, 32);

        err = Ed25519_verify(storage, len, sig, me->peerEdPK);
        if (err) {
            pair_ulog(me, kLogLevelNotice, "### Pair-setup server bad signature: %#m\n", err);

            err = TLV8BufferAppendUInt64(&tlv, kTLVType_Error, kTLVError_Authentication);
            require_noerr(err, exit);
            err = TLV8BufferAppendUInt64(&tlv, kTLVType_State, kPairSetupStateM6);
            require_noerr(err, exit);
            err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
            require_noerr(err, exit);

            _PairingSessionReset(me);
            goto exit;
        }
        ForgetMem(&storage);

        err = PairingSessionSavePeer(me, me->peerIdentifierPtr, me->peerIdentifierLen, me->peerEdPK);
        if (err) {
            pair_ulog(me, kLogLevelWarning, "### Pair-setup server save peer failed: %#m\n", err);
            err = TLV8BufferAppendUInt64(&tlv, kTLVType_Error, OSStatusToPairingStatus(err));
            require_noerr(err, exit);
            err = TLV8BufferAppendUInt64(&tlv, kTLVType_State, kPairSetupStateM6);
            require_noerr(err, exit);
            err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
            require_noerr(err, exit);
            goto exit;
        }

        me->state = kPairSetupStateM6;

        // M6: Accessory -> Controller -- Exchange Response.

        ForgetPtrLen(&me->activeIdentifierPtr, &me->activeIdentifierLen);
        err = PairingSessionCopyIdentity(me, true, &me->activeIdentifierPtr, me->ourEdPK, me->ourEdSK);
        require_noerr_quiet(err, exit);
        me->activeIdentifierLen = strlen(me->activeIdentifierPtr);
        require_action(me->activeIdentifierLen > 0, exit, err = kIDErr);

        // Generate signature of accessory's info.

        len = 32 + me->activeIdentifierLen + 32;
        storage = (uint8_t*)malloc(len);
        require_action(storage, exit, err = kNoMemoryErr);
        CryptoHKDF(kCryptoHashDescriptor_SHA512, me->srpSharedSecretPtr, me->srpSharedSecretLen,
            kPairSetupAccessorySignSaltPtr, kPairSetupAccessorySignSaltLen,
            kPairSetupAccessorySignInfoPtr, kPairSetupAccessorySignInfoLen,
            32, &storage[0]);
        memcpy(&storage[32], me->activeIdentifierPtr, me->activeIdentifierLen);
        memcpy(&storage[32 + me->activeIdentifierLen], me->ourEdPK, 32);
        Ed25519_sign(sig, storage, len, me->ourEdPK, me->ourEdSK);
        ForgetMem(&storage);

        // Build sub-TLV of accessory's info and encrypt it.

        err = TLV8BufferAppend(&etlv, kTLVType_Identifier, me->activeIdentifierPtr, me->activeIdentifierLen);
        require_noerr(err, exit);
        err = TLV8BufferAppend(&etlv, kTLVType_PublicKey, me->ourEdPK, 32);
        require_noerr(err, exit);
        err = TLV8BufferAppend(&etlv, kTLVType_Signature, sig, 64);
        require_noerr(err, exit);
        if (me->ourExtraDataPtr) {
            err = TLV8BufferAppend(&etlv, kTLVType_ExtraData, me->ourExtraDataPtr, me->ourExtraDataLen);
            require_noerr(err, exit);
        }

        storage = (uint8_t*)malloc(etlv.len + 16);
        require_action(storage, exit, err = kNoMemoryErr);
        chacha20_poly1305_encrypt_all_64x64(me->key, (const uint8_t*)"PS-Msg06", NULL, 0,
            etlv.ptr, etlv.len, storage, &storage[etlv.len]);
        err = TLV8BufferAppend(&tlv, kTLVType_EncryptedData, storage, etlv.len + 16);
        require_noerr(err, exit);
        ForgetMem(&storage);

        err = TLV8BufferAppend(&tlv, kTLVType_State, &me->state, sizeof(me->state));
        require_noerr(err, exit);
        err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
        require_noerr(err, exit);

        me->state = kPairSetupStateDone;
        done = true;

        pair_ulog(me, kLogLevelTrace, "Pair-setup server M6 -- exchange response\n%?{end}%1{tlv8}\n",
            !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, *outOutputPtr, *outOutputLen);
        pair_ulog(me, kLogLevelTrace, "Pair-setup server done -- client authenticated\n");
        break;

    default:
        pair_ulog(me, kLogLevelWarning, "### Pair-setup server bad state: %d\n", me->state);
        err = kStateErr;
        goto exit;
    }
    err = kNoErr;

exit:
    *outDone = done;
    TLV8BufferFree(&tlv);
    TLV8BufferFree(&etlv);
    ForgetMem(&eptr);
    ForgetMem(&storage);
    ForgetPtrLen(&serverPKPtr, &serverPKLen);
    ForgetMem(&clientPKStorage);
    ForgetMem(&proofStorage);
    ForgetPtrLen(&responsePtr, &responseLen);
    if (err && (err != kAsyncNoErr)) {
        pair_ulog(me, kLogLevelNotice, "### Pair-setup server state %d failed: %#m\n%?{end}%1{tlv8}\n",
            me->state, err, !log_category_enabled(me->ucat, kLogLevelInfo), kTLVDescriptors, inInputPtr, (int)inInputLen);
    }
    return (err);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================

static OSStatus
_VerifyClientExchange(
    PairingSessionRef me,
    const void* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone)
{
    const uint8_t* const inputPtr = (const uint8_t*)inInputPtr;
    const uint8_t* const inputEnd = inputPtr + inInputLen;
    Boolean done = false;
    OSStatus err;

    if (me->state == kPairingStateInvalid)
        me->state = kPairVerifyStateM1;
    if (inInputLen > 0) {
        const uint8_t* ptr;
        size_t len;

        err = TLV8Get(inputPtr, inputEnd, kTLVType_State, &ptr, &len, NULL);
        require_noerr(err, exit);
        require_action(len == 1, exit, err = kSizeErr);
        require_action(*ptr == me->state, exit, err = kStateErr);
    }
    switch (me->state) {
    case kPairVerifyStateM1:
        err = _VerifyClientM1(me, inputPtr, inputEnd, outOutputPtr, outOutputLen);
        require_noerr(err, exit);
        break;

    case kPairVerifyStateM2:
        err = _VerifyClientM2(me, inputPtr, inputEnd, outOutputPtr, outOutputLen);
        require_noerr_quiet(err, exit);
        break;

    // Note: M3 is handled inside M2 since it's the response to M2.

    case kPairVerifyStateM4:
        err = _VerifyClientM4(me, inputPtr, inputEnd);
        require_noerr_quiet(err, exit);
        *outOutputPtr = NULL;
        *outOutputLen = 0;
        done = true;
        break;

    default:
        pair_ulog(me, kLogLevelWarning, "### Pair-verify client bad state: %d\n", me->state);
        err = kStateErr;
        goto exit;
    }
    err = kNoErr;

exit:
    *outDone = done;
    if (err) {
        pair_ulog(me, kLogLevelNotice, "### Pair-verify client state %d failed: %#m\n%?{end}%1{tlv8}\n",
            me->state, err, !log_category_enabled(me->ucat, kLogLevelInfo), kTLVDescriptors, inInputPtr, (int)inInputLen);
    }
    return (err);
}

//===========================================================================================================================

static OSStatus
_VerifyClientM1(
    PairingSessionRef me,
    const uint8_t* const inInputPtr,
    const uint8_t* const inInputEnd,
    uint8_t** outOutputPtr,
    size_t* outOutputLen)
{
    OSStatus err;
    TLV8Buffer tlv;

    TLV8BufferInit(&tlv, kMaxTLVSize);
    require_action(inInputPtr == inInputEnd, exit, err = kParamErr);

    // Generate new, random ECDH key pair.

    err = RandomBytes(me->ourCurveSK, sizeof(me->ourCurveSK));
    require_noerr(err, exit);
    CryptoHKDF(kCryptoHashDescriptor_SHA512, me->ourCurveSK, sizeof(me->ourCurveSK),
        kPairVerifyECDHSaltPtr, kPairVerifyECDHSaltLen,
        kPairVerifyECDHInfoPtr, kPairVerifyECDHInfoLen,
        sizeof(me->ourCurveSK), me->ourCurveSK);
    curve25519(me->ourCurvePK, me->ourCurveSK, NULL);

    // Build the request TLV.

    err = TLV8BufferAppend(&tlv, kTLVType_State, &me->state, sizeof(me->state));
    require_noerr(err, exit);
    err = TLV8BufferAppend(&tlv, kTLVType_PublicKey, me->ourCurvePK, sizeof(me->ourCurvePK));
    require_noerr(err, exit);
    err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
    require_noerr(err, exit);
    me->state = kPairVerifyStateM2;

    pair_ulog(me, kLogLevelTrace, "Pair-verify client M1 -- start request\n%?{end}%1{tlv8}\n",
        !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, *outOutputPtr, (int)*outOutputLen);

exit:
    TLV8BufferFree(&tlv);
    return (err);
}

//===========================================================================================================================

static OSStatus
_VerifyClientM2(
    PairingSessionRef me,
    const uint8_t* const inInputPtr,
    const uint8_t* const inInputEnd,
    uint8_t** outOutputPtr,
    size_t* outOutputLen)
{
    OSStatus err;
    TLV8Buffer tlv;
    uint8_t* eptr = NULL;
    uint8_t* eend;
    size_t elen, len;
    uint8_t* storage = NULL;
    CFDictionaryRef acl = NULL;
    uint8_t sig[64];

    TLV8BufferInit(&tlv, kMaxTLVSize);
    pair_ulog(me, kLogLevelTrace, "Pair-verify client M2 -- start response\n%?{end}%1{tlv8}\n",
        !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, inInputPtr, (int)(inInputEnd - inInputPtr));

    // Generate shared secret and derive encryption key.

    err = TLV8GetBytes(inInputPtr, inInputEnd, kTLVType_PublicKey, 32, 32, me->peerCurvePK, NULL, NULL);
    require_noerr(err, exit);
    curve25519(me->sharedSecret, me->ourCurveSK, me->peerCurvePK);
    require_action(Curve25519ValidSharedSecret(me->sharedSecret), exit, err = kMalformedErr);

    CryptoHKDF(kCryptoHashDescriptor_SHA512, me->sharedSecret, sizeof(me->sharedSecret),
        kPairVerifyEncryptSaltPtr, kPairVerifyEncryptSaltLen,
        kPairVerifyEncryptInfoPtr, kPairVerifyEncryptInfoLen,
        sizeof(me->key), me->key);

    // Verify and decrypt sub-TLV.

    eptr = TLV8CopyCoalesced(inInputPtr, inInputEnd, kTLVType_EncryptedData, &elen, NULL, &err);
    require_noerr(err, exit);
    require_action(elen > 16, exit, err = kSizeErr);
    elen -= 16;
    eend = eptr + elen;
    err = chacha20_poly1305_decrypt_all_64x64(me->key, (const uint8_t*)"PV-Msg02", NULL, 0, eptr, elen, eptr, eend);
    require_noerr(err, exit);

    {
        // Look up peer's LTPK.

        ForgetPtrLen(&me->peerIdentifierPtr, &me->peerIdentifierLen);
        me->peerIdentifierPtr = TLV8CopyCoalesced(eptr, eend, kTLVType_Identifier, &me->peerIdentifierLen, NULL, &err);
        require_noerr(err, exit);
        require_action(me->peerIdentifierLen > 0, exit, err = kSizeErr);

        err = PairingSessionFindPeerEx(me, me->peerIdentifierPtr, me->peerIdentifierLen, me->peerEdPK, &acl);
        require_noerr_quiet(err, exit);
        ForgetMem(&storage);

        // Verify signature of the peer's info.

        err = TLV8GetBytes(eptr, eend, kTLVType_Signature, 64, 64, sig, NULL, NULL);
        require_noerr(err, exit);

        len = 32 + me->peerIdentifierLen + 32;
        storage = (uint8_t*)malloc(len);
        require_action(storage, exit, err = kNoMemoryErr);
        memcpy(&storage[0], me->peerCurvePK, 32);
        memcpy(&storage[32], me->peerIdentifierPtr, me->peerIdentifierLen);
        memcpy(&storage[32 + me->peerIdentifierLen], me->ourCurvePK, 32);
        err = Ed25519_verify(storage, len, sig, me->peerEdPK);
        require_noerr_action_quiet(err, exit, err = kAuthenticationErr);
        ForgetMem(&storage);
    }

    err = _PairingSessionVerifyACL(me, acl);
    if (err) {
        pair_ulog(me, kLogLevelNotice, "### Pair-verify client -- server lacks ACL: %@\n", me->acl);

        err = TLV8BufferAppendUInt64(&tlv, kTLVType_Error, kTLVError_Permission);
        require_noerr(err, exit);
        err = TLV8BufferAppendUInt64(&tlv, kTLVType_State, kPairVerifyStateM3);
        require_noerr(err, exit);
        err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
        require_noerr(err, exit);

        _PairingSessionReset(me);
        goto exit;
    }

    me->state = kPairVerifyStateM3;
    err = _VerifyClientM3(me, outOutputPtr, outOutputLen);
    require_noerr_quiet(err, exit);

exit:
    CFReleaseNullSafe(acl);
    FreeNullSafe(storage);
    FreeNullSafe(eptr);
    TLV8BufferFree(&tlv);
    return (err);
}

//===========================================================================================================================

static OSStatus
_VerifyClientM3(
    PairingSessionRef me,
    uint8_t** outOutputPtr,
    size_t* outOutputLen)
{
    OSStatus err;
    TLV8Buffer tlv, etlv;
    size_t len;
    uint8_t* storage = NULL;
    uint8_t sig[64];

    TLV8BufferInit(&tlv, kMaxTLVSize);
    TLV8BufferInit(&etlv, kMaxTLVSize);

    {
        // Generate signature of the peer's info.

        ForgetPtrLen(&me->activeIdentifierPtr, &me->activeIdentifierLen);
        err = PairingSessionCopyIdentity(me, false, &me->activeIdentifierPtr, me->ourEdPK, me->ourEdSK);
        require_noerr_quiet(err, exit);
        me->activeIdentifierLen = strlen(me->activeIdentifierPtr);
        require_action(me->activeIdentifierLen > 0, exit, err = kIDErr);

        len = 32 + me->activeIdentifierLen + 32;
        storage = (uint8_t*)malloc(len);
        require_action(storage, exit, err = kNoMemoryErr);
        memcpy(&storage[0], me->ourCurvePK, 32);
        memcpy(&storage[32], me->activeIdentifierPtr, me->activeIdentifierLen);
        memcpy(&storage[32 + me->activeIdentifierLen], me->peerCurvePK, 32);
        Ed25519_sign(sig, storage, len, me->ourEdPK, me->ourEdSK);
        ForgetMem(&storage);

        err = TLV8BufferAppend(&etlv, kTLVType_Identifier, me->activeIdentifierPtr, me->activeIdentifierLen);
        require_noerr(err, exit);
        err = TLV8BufferAppend(&etlv, kTLVType_Signature, sig, 64);
        require_noerr(err, exit);
    }

    // Encrypt the sub-TLV of our info.

    storage = (uint8_t*)malloc(etlv.len + 16);
    require_action(storage, exit, err = kNoMemoryErr);
    chacha20_poly1305_encrypt_all_64x64(me->key, (const uint8_t*)"PV-Msg03", NULL, 0,
        etlv.ptr, etlv.len, storage, &storage[etlv.len]);
    err = TLV8BufferAppend(&tlv, kTLVType_EncryptedData, storage, etlv.len + 16);
    require_noerr(err, exit);
    ForgetMem(&storage);

    err = TLV8BufferAppend(&tlv, kTLVType_State, &me->state, sizeof(me->state));
    require_noerr(err, exit);
    err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
    require_noerr(err, exit);
    me->state = kPairVerifyStateM4;

    pair_ulog(me, kLogLevelTrace, "Pair-verify client M3 -- finish request\n%?{end}%1{tlv8}\n",
        !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, *outOutputPtr, (int)*outOutputLen);

exit:
    FreeNullSafe(storage);
    TLV8BufferFree(&etlv);
    TLV8BufferFree(&tlv);
    return (err);
}

//===========================================================================================================================

static OSStatus
_VerifyClientM4(
    PairingSessionRef me,
    const uint8_t* const inInputPtr,
    const uint8_t* const inInputEnd)
{
    OSStatus err;
    TLV8Buffer tlv;
    const uint8_t* ptr;
    size_t len;

    TLV8BufferInit(&tlv, kMaxTLVSize);
    pair_ulog(me, kLogLevelTrace, "Pair-verify client M4 -- finish response\n%?{end}%1{tlv8}\n",
        !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, inInputPtr, (int)(inInputEnd - inInputPtr));

    err = TLV8Get(inInputPtr, inInputEnd, kTLVType_Error, &ptr, &len, NULL);
    if (!err) {
        require_action(len == 1, exit, err = kSizeErr);
        err = PairingStatusToOSStatus(*ptr);
        pair_ulog(me, kLogLevelNotice, "### Pair-verify client M4 bad status: 0x%X, %#m\n", *ptr, err);
        require_noerr_quiet(err, exit);
    }
    if (me->flags & kPairingFlag_Resume) {
        uint8_t buf[8];

        CryptoHKDF(kCryptoHashDescriptor_SHA512, me->sharedSecret, sizeof(me->sharedSecret),
            kPairVerifyResumeSessionIDSaltPtr, kPairVerifyResumeSessionIDSaltLen,
            kPairVerifyResumeSessionIDInfoPtr, kPairVerifyResumeSessionIDInfoLen,
            8, buf);
        me->resumeSessionID = ReadLittle64(buf);
        _PairingSaveResumeState(me, me->peerIdentifierPtr, me->peerIdentifierLen, me->identifierPtr, me->identifierLen,
            me->resumeSessionID, me->sharedSecret, sizeof(me->sharedSecret));
    }
    me->state = kPairVerifyStateDone;
    err = kNoErr;
    pair_ulog(me, kLogLevelTrace, "Pair-verify client done\n");

exit:
    TLV8BufferFree(&tlv);
    return (err);
}

//===========================================================================================================================

static OSStatus
_VerifyServerExchange(
    PairingSessionRef me,
    const void* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone)
{
    const uint8_t* const inputPtr = (const uint8_t*)inInputPtr;
    const uint8_t* const inputEnd = inputPtr + inInputLen;
    Boolean done = false;
    OSStatus err;
    const uint8_t* ptr;
    size_t len;

    err = TLV8Get(inputPtr, inputEnd, kTLVType_State, &ptr, &len, NULL);
    require_noerr(err, exit);
    require_action(len == 1, exit, err = kSizeErr);
    if (*ptr == kPairVerifyStateM1)
        _PairingSessionReset(me);
    if (me->state == kPairingStateInvalid)
        me->state = kPairVerifyStateM1;
    require_action(*ptr == me->state, exit, err = kStateErr);

    switch (me->state) {
    case kPairVerifyStateM1:
        err = _VerifyServerM1(me, inputPtr, inputEnd, outOutputPtr, outOutputLen);
        require_noerr(err, exit);
        break;

    // Note: M2 is handled inside M1 since it's the response to M1.

    case kPairVerifyStateM3:
        err = _VerifyServerM3(me, inputPtr, inputEnd, outOutputPtr, outOutputLen, &done);
        require_noerr_quiet(err, exit);
        break;

    // Note: M4 is handled inside M3 since it's the response to M3.

    default:
        pair_ulog(me, kLogLevelWarning, "### Pair-verify server bad state: %d\n", me->state);
        err = kStateErr;
        goto exit;
    }
    err = kNoErr;
    pair_ulog(me, kLogLevelNotice, "### Pair-verify server ok, state: %d\n", me->state);

exit:
    *outDone = done;
    if (err) {
        pair_ulog(me, kLogLevelNotice, "### Pair-verify server state %d failed: %#m\n%?{end}%1{tlv8}\n",
            me->state, err, !log_category_enabled(me->ucat, kLogLevelInfo), kTLVDescriptors, inInputPtr, (int)inInputLen);
    }
    return (err);
}

//===========================================================================================================================

static OSStatus
_VerifyServerM1(
    PairingSessionRef me,
    const uint8_t* const inInputPtr,
    const uint8_t* const inInputEnd,
    uint8_t** outOutputPtr,
    size_t* outOutputLen)
{
    OSStatus err;

    pair_ulog(me, kLogLevelTrace, "Pair-verify server M1 -- start request\n%?{end}%1{tlv8}\n",
        !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, inInputPtr, (int)(inInputEnd - inInputPtr));

    // Generate new, random ECDH key pair.

    err = RandomBytes(me->ourCurveSK, sizeof(me->ourCurveSK));
    require_noerr(err, exit);
    CryptoHKDF(kCryptoHashDescriptor_SHA512, me->ourCurveSK, sizeof(me->ourCurveSK),
        kPairVerifyECDHSaltPtr, kPairVerifyECDHSaltLen,
        kPairVerifyECDHInfoPtr, kPairVerifyECDHInfoLen,
        sizeof(me->ourCurveSK), me->ourCurveSK);
    curve25519(me->ourCurvePK, me->ourCurveSK, NULL);

    // Generate shared secret.

    err = TLV8GetBytes(inInputPtr, inInputEnd, kTLVType_PublicKey, 32, 32, me->peerCurvePK, NULL, NULL);
    require_noerr(err, exit);
    curve25519(me->sharedSecret, me->ourCurveSK, me->peerCurvePK);
    require_action(Curve25519ValidSharedSecret(me->sharedSecret), exit, err = kMalformedErr);

    me->state = kPairVerifyStateM2;
    err = _VerifyServerM2(me, outOutputPtr, outOutputLen);
    require_noerr_quiet(err, exit);

exit:
    return (err);
}

//===========================================================================================================================

static OSStatus
_VerifyServerM2(
    PairingSessionRef me,
    uint8_t** outOutputPtr,
    size_t* outOutputLen)
{
    OSStatus err;
    TLV8Buffer tlv, etlv;
    size_t len;
    uint8_t* storage = NULL;
    uint8_t sig[64];

    TLV8BufferInit(&tlv, kMaxTLVSize);
    TLV8BufferInit(&etlv, kMaxTLVSize);

    {
        ForgetPtrLen(&me->activeIdentifierPtr, &me->activeIdentifierLen);
        err = PairingSessionCopyIdentity(me, false, &me->activeIdentifierPtr, me->ourEdPK, me->ourEdSK);
        require_noerr_quiet(err, exit);
        me->activeIdentifierLen = strlen(me->activeIdentifierPtr);
        require_action(me->activeIdentifierLen > 0, exit, err = kIDErr);

        len = 32 + me->activeIdentifierLen + 32;
        storage = (uint8_t*)malloc(len);
        require_action(storage, exit, err = kNoMemoryErr);
        memcpy(&storage[0], me->ourCurvePK, 32);
        memcpy(&storage[32], me->activeIdentifierPtr, me->activeIdentifierLen);
        memcpy(&storage[32 + me->activeIdentifierLen], me->peerCurvePK, 32);
        Ed25519_sign(sig, storage, len, me->ourEdPK, me->ourEdSK);
        ForgetMem(&storage);

        err = TLV8BufferAppend(&etlv, kTLVType_Identifier, me->activeIdentifierPtr, me->activeIdentifierLen);
        require_noerr(err, exit);
        err = TLV8BufferAppend(&etlv, kTLVType_Signature, sig, 64);
        require_noerr(err, exit);
    }

    // Encrypt the sub-TLV.

    storage = (uint8_t*)malloc(etlv.len + 16);
    require_action(storage, exit, err = kNoMemoryErr);
    CryptoHKDF(kCryptoHashDescriptor_SHA512, me->sharedSecret, sizeof(me->sharedSecret),
        kPairVerifyEncryptSaltPtr, kPairVerifyEncryptSaltLen,
        kPairVerifyEncryptInfoPtr, kPairVerifyEncryptInfoLen,
        sizeof(me->key), me->key);
    chacha20_poly1305_encrypt_all_64x64(me->key, (const uint8_t*)"PV-Msg02", NULL, 0,
        etlv.ptr, etlv.len, storage, &storage[etlv.len]);
    err = TLV8BufferAppend(&tlv, kTLVType_EncryptedData, storage, etlv.len + 16);
    require_noerr(err, exit);
    ForgetMem(&storage);

    me->state = kPairVerifyStateM2;

    err = TLV8BufferAppend(&tlv, kTLVType_State, &me->state, sizeof(me->state));
    require_noerr(err, exit);
    err = TLV8BufferAppend(&tlv, kTLVType_PublicKey, me->ourCurvePK, 32);
    require_noerr(err, exit);
    err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
    require_noerr(err, exit);

    pair_ulog(me, kLogLevelTrace, "Pair-verify server M2 -- start response\n%?{end}%1{tlv8}\n",
        !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, *outOutputPtr, *outOutputLen);
    me->state = kPairVerifyStateM3;

exit:
    FreeNullSafe(storage);
    TLV8BufferFree(&etlv);
    TLV8BufferFree(&tlv);
    return (err);
}

//===========================================================================================================================

static OSStatus
_VerifyServerM3(
    PairingSessionRef me,
    const uint8_t* const inInputPtr,
    const uint8_t* const inInputEnd,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone)
{
    OSStatus err;
    Boolean done = false;
    TLV8Buffer tlv;
    const uint8_t* ptr;
    size_t len;
    uint8_t* eptr = NULL;
    uint8_t* eend;
    size_t elen;
    uint8_t* storage = NULL;
    uint8_t sig[64];
    CFDictionaryRef acl = NULL;

    TLV8BufferInit(&tlv, kMaxTLVSize);

    pair_ulog(me, kLogLevelTrace, "Pair-verify server M3 -- finish request\n%?{end}%1{tlv8}\n",
        !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, inInputPtr, (int)(inInputEnd - inInputPtr));

    err = TLV8Get(inInputPtr, inInputEnd, kTLVType_Error, &ptr, &len, NULL);
    if (!err) {
        require_action(len == 1, exit, err = kSizeErr);
        err = PairingStatusToOSStatus(*ptr);
        pair_ulog(me, kLogLevelNotice, "### Pair-verify server M3 bad status: 0x%X, %#m\n", *ptr, err);
        require_noerr_quiet(err, exit);
    }

    // Verify and decrypt sub-TLV.

    eptr = TLV8CopyCoalesced(inInputPtr, inInputEnd, kTLVType_EncryptedData, &elen, NULL, &err);
    require_noerr(err, exit);
    require_action(elen >= 16, exit, err = kSizeErr);
    elen -= 16;
    eend = eptr + elen;
    err = chacha20_poly1305_decrypt_all_64x64(me->key, (const uint8_t*)"PV-Msg03", NULL, 0, eptr, elen, eptr, eend);
    if (err) {
        pair_ulog(me, kLogLevelNotice, "### Pair-verify server bad auth tag\n");

        err = TLV8BufferAppendUInt64(&tlv, kTLVType_Error, kTLVError_Authentication);
        require_noerr(err, exit);
        err = TLV8BufferAppendUInt64(&tlv, kTLVType_State, kPairVerifyStateM4);
        require_noerr(err, exit);
        err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
        require_noerr(err, exit);

        _PairingSessionReset(me);
        goto exit;
    }

    {
        // Look up peer's LTPK.

        ForgetPtrLen(&me->peerIdentifierPtr, &me->peerIdentifierLen);
        me->peerIdentifierPtr = TLV8CopyCoalesced(eptr, eend, kTLVType_Identifier, &me->peerIdentifierLen, NULL, &err);
        require_noerr(err, exit);
        require_action(me->peerIdentifierLen > 0, exit, err = kSizeErr);

        err = PairingSessionFindPeerEx(me, me->peerIdentifierPtr, me->peerIdentifierLen, me->peerEdPK, &acl);
        if (err) {
            pair_ulog(me, kLogLevelNotice, "### Pair-verify server unknown peer: %.*s\n",
                (int)me->peerIdentifierLen, me->peerIdentifierPtr);

            err = TLV8BufferAppendUInt64(&tlv, kTLVType_Error, kTLVError_Authentication);
            require_noerr(err, exit);
            err = TLV8BufferAppendUInt64(&tlv, kTLVType_State, kPairVerifyStateM4);
            require_noerr(err, exit);
            err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
            require_noerr(err, exit);

            _PairingSessionReset(me);
            goto exit;
        }
        ForgetMem(&storage);

        // Verify signature of peer's info.

        err = TLV8GetBytes(eptr, eend, kTLVType_Signature, 64, 64, sig, NULL, NULL);
        require_noerr(err, exit);

        len = 32 + me->peerIdentifierLen + 32;
        storage = (uint8_t*)malloc(len);
        require_action(storage, exit, err = kNoMemoryErr);
        memcpy(&storage[0], me->peerCurvePK, 32);
        memcpy(&storage[32], me->peerIdentifierPtr, me->peerIdentifierLen);
        memcpy(&storage[32 + me->peerIdentifierLen], me->ourCurvePK, 32);
        err = Ed25519_verify(storage, len, sig, me->peerEdPK);
        ForgetMem(&storage);
    }
    if (err) {
        pair_ulog(me, kLogLevelNotice, "### Pair-verify server bad signature: %#m\n", err);

        err = TLV8BufferAppendUInt64(&tlv, kTLVType_Error, kTLVError_Authentication);
        require_noerr(err, exit);
        err = TLV8BufferAppendUInt64(&tlv, kTLVType_State, kPairVerifyStateM4);
        require_noerr(err, exit);
        err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
        require_noerr(err, exit);

        _PairingSessionReset(me);
        goto exit;
    }

    err = _PairingSessionVerifyACL(me, acl);
    if (err) {
        pair_ulog(me, kLogLevelNotice, "### Pair-verify server -- client lacks ACL: %@\n", me->acl);

        err = TLV8BufferAppendUInt64(&tlv, kTLVType_Error, kTLVError_Permission);
        require_noerr(err, exit);
        err = TLV8BufferAppendUInt64(&tlv, kTLVType_State, kPairVerifyStateM4);
        require_noerr(err, exit);
        err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
        require_noerr(err, exit);

        _PairingSessionReset(me);
        goto exit;
    }

    me->state = kPairVerifyStateM4;
    err = _VerifyServerM4(me, outOutputPtr, outOutputLen);
    require_noerr(err, exit);
    done = true;

exit:
    *outDone = done;
    CFReleaseNullSafe(acl);
    FreeNullSafe(storage);
    FreeNullSafe(eptr);
    TLV8BufferFree(&tlv);
    return (err);
}

//===========================================================================================================================

static OSStatus
_VerifyServerM4(
    PairingSessionRef me,
    uint8_t** outOutputPtr,
    size_t* outOutputLen)
{
    OSStatus err;
    TLV8Buffer tlv;

    TLV8BufferInit(&tlv, kMaxTLVSize);
    err = TLV8BufferAppend(&tlv, kTLVType_State, &me->state, sizeof(me->state));
    require_noerr(err, exit);
    err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
    require_noerr(err, exit);

    if (me->flags & kPairingFlag_Resume) {
        uint8_t buf[8];

        CryptoHKDF(kCryptoHashDescriptor_SHA512, me->sharedSecret, sizeof(me->sharedSecret),
            kPairVerifyResumeSessionIDSaltPtr, kPairVerifyResumeSessionIDSaltLen,
            kPairVerifyResumeSessionIDInfoPtr, kPairVerifyResumeSessionIDInfoLen,
            8, buf);
        me->resumeSessionID = ReadLittle64(buf);
        _PairingSaveResumeState(me, me->peerIdentifierPtr, me->peerIdentifierLen, me->identifierPtr, me->identifierLen,
            me->resumeSessionID, me->sharedSecret, sizeof(me->sharedSecret));
    }

    pair_ulog(me, kLogLevelTrace, "Pair-verify server M4 -- finish response\n%?{end}%1{tlv8}\n",
        !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, *outOutputPtr, *outOutputLen);
    me->state = kPairVerifyStateDone;
    pair_ulog(me, kLogLevelTrace, "Pair-verify server done\n");

exit:
    TLV8BufferFree(&tlv);
    return (err);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================

static OSStatus
_AdminPairingClientExchange(
    PairingSessionRef me,
    const void* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone)
{
    const uint8_t* const inputPtr = (const uint8_t*)inInputPtr;
    const uint8_t* const inputEnd = inputPtr + inInputLen;
    const uint8_t* stateEnd = NULL;
    Boolean done = false;
    OSStatus err;

    if (me->state == kPairingStateInvalid)
        me->state = kAdminPairingStateM1;
    if (inInputLen > 0) {
        const uint8_t* ptr;
        size_t len;

        err = TLV8Get(inputPtr, inputEnd, kTLVType_State, &ptr, &len, &stateEnd);
        require_noerr_quiet(err, exit);
        require_action_quiet(len == 1, exit, err = kSizeErr);
        require_action_quiet(*ptr == me->state, exit, err = kStateErr);
    }
    switch (me->state) {
    case kAdminPairingStateM1:
        err = _AdminPairingClientM1(me, inputPtr, inputEnd, outOutputPtr, outOutputLen);
        require_noerr_quiet(err, exit);
        break;

    case kAdminPairingStateM2:
        err = _AdminPairingClientM2(me, inputPtr, inputEnd, stateEnd);
        require_noerr_quiet(err, exit);
        *outOutputPtr = NULL;
        *outOutputLen = 0;
        done = true;
        break;

    default:
        pair_ulog(me, kLogLevelWarning, "### AdminPairing client bad state: %d\n", me->state);
        err = kStateErr;
        goto exit;
    }

exit:
    *outDone = done;
    if (err) {
        pair_ulog(me, kLogLevelNotice, "### AdminPairing client state %d failed: %#m\n%?{end}%1{tlv8}\n",
            me->state, err, !log_category_enabled(me->ucat, kLogLevelInfo), kTLVDescriptors, inInputPtr, (int)inInputLen);
    }
    return (err);
}

//===========================================================================================================================

static OSStatus
_AdminPairingClientM1(
    PairingSessionRef me,
    const uint8_t* const inInputPtr,
    const uint8_t* const inInputEnd,
    uint8_t** outOutputPtr,
    size_t* outOutputLen)
{
    OSStatus err;
    TLV8Buffer tlv;
    const void* ptr;
    size_t len;
    void* buf = NULL;
    CFDataRef data;
    PairingPermissions permissions;

    TLV8BufferInit(&tlv, kMaxTLVSize);
    require_action_quiet(inInputPtr == inInputEnd, exit, err = kParamErr);

    err = TLV8BufferAppend(&tlv, kTLVType_State, &me->state, sizeof(me->state));
    require_noerr(err, exit);

    // AddPairing.

    if (me->type == kPairingSessionType_AddPairingClient) {
        err = TLV8BufferAppendUInt64(&tlv, kTLVType_Method, kTLVMethod_AddPairing);
        require_noerr(err, exit);

        // Get identifier.

        ptr = CFDictionaryGetOrCopyBytes(me->properties, kPairingSessionPropertyControllerIdentifier, &len, &buf, &err);
        require_quiet(ptr, exit);
        err = TLV8BufferAppend(&tlv, kTLVType_Identifier, ptr, len);
        require_noerr(err, exit);
        ForgetMem(&buf);

        // Get PublicKey.

        data = CFDictionaryGetCFData(me->properties, kPairingSessionPropertyControllerPublicKey, &err);
        require(data, exit);
        err = TLV8BufferAppend(&tlv, kTLVType_PublicKey, CFDataGetBytePtr(data), (size_t)CFDataGetLength(data));
        require_noerr(err, exit);

        // Get permissions.

        permissions = CFDictionaryGetUInt32(me->properties, kPairingSessionPropertyPermissions, &err);
        require_noerr(err, exit);
        err = TLV8BufferAppendUInt64(&tlv, kTLVType_Permissions, permissions);
        require_noerr(err, exit);
    }

    // RemovePairing.

    else if (me->type == kPairingSessionType_RemovePairingClient) {
        err = TLV8BufferAppendUInt64(&tlv, kTLVType_Method, kTLVMethod_RemovePairing);
        require_noerr(err, exit);

        ptr = CFDictionaryGetOrCopyBytes(me->properties, kPairingSessionPropertyControllerIdentifier, &len, &buf, &err);
        require_quiet(ptr, exit);
        err = TLV8BufferAppend(&tlv, kTLVType_Identifier, ptr, len);
        require_noerr(err, exit);
        ForgetMem(&buf);
    }

    // ListPairings.

    else if (me->type == kPairingSessionType_ListPairingsClient) {
        err = TLV8BufferAppendUInt64(&tlv, kTLVType_Method, kTLVMethod_ListPairings);
        require_noerr(err, exit);
    }

    // Unsupported.

    else {
        pair_ulog(me, kLogLevelWarning, "### AdminPairing client bad session type %d\n", me->type);
        err = kParamErr;
        goto exit;
    }

    // Return response.

    err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
    require_noerr(err, exit);

    me->state = kAdminPairingStateM2;
    pair_ulog(me, kLogLevelTrace, "AdminPairing client M1 -- request\n%?{end}%1{tlv8}\n",
        !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, *outOutputPtr, (int)*outOutputLen);

exit:
    FreeNullSafe(buf);
    TLV8BufferFree(&tlv);
    return (err);
}

//===========================================================================================================================

static OSStatus
_AdminPairingClientM2(
    PairingSessionRef me,
    const uint8_t* const inInputPtr,
    const uint8_t* const inInputEnd,
    const uint8_t* const inStateEnd)
{
    OSStatus err;
    const uint8_t* ptr;
    size_t len;

    pair_ulog(me, kLogLevelTrace, "AdminPairing client M2 -- response\n%?{end}%1{tlv8}\n",
        !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, inInputPtr, (int)(inInputEnd - inInputPtr));

    err = TLV8Get(inInputPtr, inInputEnd, kTLVType_Error, &ptr, &len, NULL);
    if (!err) {
        require_action_quiet(len == 1, exit, err = kSizeErr);
        err = PairingStatusToOSStatus(*ptr);
        pair_ulog(me, kLogLevelNotice, "### AdminPairing client M2 bad status: 0x%X, %#m\n", *ptr, err);
        require_noerr_quiet(err, exit);
    }

    if (me->type == kPairingSessionType_ListPairingsClient) {
        err = _AdminPairingClientParsePairings(me, inStateEnd, inInputEnd);
        require_noerr_quiet(err, exit);
    }

    me->state = kAdminPairingStateDone;
    err = kNoErr;
    pair_ulog(me, kLogLevelTrace, "AdminPairing client done\n");

exit:
    return (err);
}

//===========================================================================================================================

static OSStatus
_AdminPairingClientParsePairings(
    PairingSessionRef me,
    const uint8_t* inInputPtr,
    const uint8_t* const inInputEnd)
{
    OSStatus err;
    CFMutableArrayRef pairings;
    CFMutableDictionaryRef pairingInfo = NULL;

    pairings = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    require_action(pairings, exit, err = kNoMemoryErr);

    for (;;) {
        uint8_t type;
        const uint8_t* ptr;
        size_t len;
        uint64_t u64;

        err = TLV8GetNext(inInputPtr, inInputEnd, &type, &ptr, &len, &inInputPtr);
        if (err)
            break;

        if (!pairingInfo) {
            pairingInfo = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            require_action(pairingInfo, exit, err = kNoMemoryErr);
        }
        switch (type) {
        case kTLVType_Identifier:
            err = CFDictionarySetCString(pairingInfo, CFSTR(kPairingKey_Identifier), ptr, len);
            require_noerr_quiet(err, exit);
            break;

        case kTLVType_Permissions:
            u64 = TLVParseUInt64(ptr, len, &err);
            require_noerr_quiet(err, exit);
            err = CFDictionarySetUInt64(pairingInfo, CFSTR(kPairingKey_Permissions), u64);
            require_noerr_quiet(err, exit);
            break;

        case kTLVType_PublicKey:
            err = CFDictionarySetData(pairingInfo, CFSTR(kPairingKey_PublicKey), ptr, len);
            require_noerr_quiet(err, exit);
            break;

        case kTLVType_Separator:
            if (CFDictionaryGetCount(pairingInfo) > 0) {
                CFArrayAppendValue(pairings, pairingInfo);
            }
            CFRelease(pairingInfo);
            pairingInfo = NULL;
            break;

        default:
            break;
        }
    }
    if (pairingInfo) {
        if (CFDictionaryGetCount(pairingInfo) > 0) {
            CFArrayAppendValue(pairings, pairingInfo);
        }
        CFRelease(pairingInfo);
        pairingInfo = NULL;
    }

    err = PairingSessionSetProperty(me, kPairingSessionPropertyPairings, pairings);
    require_noerr(err, exit);

exit:
    CFReleaseNullSafe(pairingInfo);
    CFReleaseNullSafe(pairings);
    return (err);
}

//===========================================================================================================================

static OSStatus
_AdminPairingServerExchange(
    PairingSessionRef me,
    const void* inInputPtr,
    size_t inInputLen,
    uint8_t** outOutputPtr,
    size_t* outOutputLen,
    Boolean* outDone)
{
    const uint8_t* const inputPtr = (const uint8_t*)inInputPtr;
    const uint8_t* const inputEnd = inputPtr + inInputLen;
    Boolean done = false;
    OSStatus err;
    const uint8_t* ptr;
    size_t len;

    err = TLV8Get(inputPtr, inputEnd, kTLVType_State, &ptr, &len, NULL);
    require_noerr_quiet(err, exit);
    require_action_quiet(len == 1, exit, err = kSizeErr);
    if (*ptr == kAdminPairingStateM1)
        _PairingSessionReset(me);
    if (me->state == kPairingStateInvalid)
        me->state = kAdminPairingStateM1;
    require_action_quiet(*ptr == me->state, exit, err = kStateErr);

    switch (me->state) {
    case kAdminPairingStateM1:
        err = _AdminPairingServerM1(me, inputPtr, inputEnd, outOutputPtr, outOutputLen);
        require_noerr_quiet(err, exit);
        done = true;
        break;

    // Note: M2 is handled inside M1 since it's the response to M1.

    default:
        pair_ulog(me, kLogLevelWarning, "### AdminPairing server bad state: %d\n", me->state);
        err = kStateErr;
        goto exit;
    }

exit:
    *outDone = done;
    if (err) {
        pair_ulog(me, kLogLevelNotice, "### AdminPairing server state %d failed: %#m\n%?{end}%1{tlv8}\n",
            me->state, err, !log_category_enabled(me->ucat, kLogLevelInfo), kTLVDescriptors, inInputPtr, (int)inInputLen);
    }
    return (err);
}

//===========================================================================================================================

static OSStatus
_AdminPairingServerM1(
    PairingSessionRef me,
    const uint8_t* const inInputPtr,
    const uint8_t* const inInputEnd,
    uint8_t** outOutputPtr,
    size_t* outOutputLen)
{
    OSStatus err;
    TLV8Buffer tlv;
    CFMutableDictionaryRef info = NULL;
    uint8_t u8;
    const uint8_t* ptr;
    size_t len;
    uint8_t* buf = NULL;
    uint64_t u64;
    PairingPermissions permissions;
    CFArrayRef pairings = NULL;

    TLV8BufferInit(&tlv, kMaxTLVSize);

    me->state = kAdminPairingStateM2;
    err = TLV8BufferAppend(&tlv, kTLVType_State, &me->state, sizeof(me->state));
    require_noerr(err, exit);

    // AddPairing.

    if (me->type == kPairingSessionType_AddPairingServer) {
        err = TLV8GetBytes(inInputPtr, inInputEnd, kTLVType_Method, 1, 1, &u8, NULL, NULL);
        require_noerr_quiet(err, exit);
        require_action_quiet(u8 == kTLVMethod_AddPairing, exit, err = kParamErr);

        info = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require_action(info, exit, err = kNoMemoryErr);

        // Get identifier.

        err = TLV8GetOrCopyCoalesced(inInputPtr, inInputEnd, kTLVType_Identifier, &ptr, &len, &buf, NULL);
        require_noerr_quiet(err, exit);
        err = CFDictionarySetCString(info, CFSTR(kPairingKey_Identifier), ptr, len);
        require_noerr_quiet(err, exit);
        ForgetMem(&buf);

        // Get public key.

        err = TLV8GetOrCopyCoalesced(inInputPtr, inInputEnd, kTLVType_PublicKey, &ptr, &len, &buf, NULL);
        require_noerr_quiet(err, exit);
        err = CFDictionarySetData(info, CFSTR(kPairingKey_PublicKey), ptr, len);
        require_noerr_quiet(err, exit);
        ForgetMem(&buf);

        // Get permissions.

        u64 = TLV8GetUInt64(inInputPtr, inInputEnd, kTLVType_Permissions, &err, NULL);
        require_noerr_quiet(err, exit);
        permissions = (PairingPermissions)u64;
        require_action_quiet(permissions == u64, exit, err = kRangeErr);
        err = CFDictionarySetUInt64(info, CFSTR(kPairingKey_Permissions), permissions);
        require_noerr_quiet(err, exit);

        // Add the pairing.

        if (me->addPairingHandler_f) {
            err = me->addPairingHandler_f(info, me->addPairingHandler_ctx);
        } else {
#if (PAIRING_KEYCHAIN)
            err = _PairingSessionSaveControllerKeychain(me, info);
#else
            err = kUnsupportedErr;
#endif
        }
    }

    // RemovePairing.

    else if (me->type == kPairingSessionType_RemovePairingServer) {
        err = TLV8GetBytes(inInputPtr, inInputEnd, kTLVType_Method, 1, 1, &u8, NULL, NULL);
        require_noerr_quiet(err, exit);
        require_action_quiet(u8 == kTLVMethod_RemovePairing, exit, err = kParamErr);

        info = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require_action(info, exit, err = kNoMemoryErr);

        // Get identifier.

        err = TLV8GetOrCopyCoalesced(inInputPtr, inInputEnd, kTLVType_Identifier, &ptr, &len, &buf, NULL);
        require_noerr_quiet(err, exit);
        err = CFDictionarySetCString(info, CFSTR(kPairingKey_Identifier), ptr, len);
        require_noerr_quiet(err, exit);
        ForgetMem(&buf);

        // Remove the pairing.

        if (me->removePairingHandler_f) {
            err = me->removePairingHandler_f(info, me->removePairingHandler_ctx);
        } else {
#if (PAIRING_KEYCHAIN)
            err = _PairingSessionRemoveControllerKeychain(me, info);
#else
            err = kUnsupportedErr;
#endif
        }
    }

    // ListPairings.

    else if (me->type == kPairingSessionType_ListPairingsServer) {
        err = TLV8GetBytes(inInputPtr, inInputEnd, kTLVType_Method, 1, 1, &u8, NULL, NULL);
        require_noerr_quiet(err, exit);
        require_action_quiet(u8 == kTLVMethod_ListPairings, exit, err = kParamErr);

        info = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require_action(info, exit, err = kNoMemoryErr);

        if (me->listPairingsHandler_f) {
            pairings = me->listPairingsHandler_f(&err, me->listPairingsHandler_ctx);
        } else {
#if (PAIRING_KEYCHAIN)
            pairings = _PairingSessionListControllersKeychain(me, &err);
#else
            err = kUnsupportedErr;
            goto exit;
#endif
        }
        require(pairings, exit);
        require_noerr(err, exit);

        err = _AdminPairingServerAppendPairings(&tlv, pairings);
        require_noerr(err, exit);
    }

    // Unsupported.

    else {
        pair_ulog(me, kLogLevelWarning, "### AdminPairing server bad session type %d\n", me->type);
        err = kParamErr;
        goto exit;
    }
    if (err) {
        pair_ulog(me, kLogLevelNotice, "### AdminPairing server failed for %##@: %#m\n", info, err);
        err = TLV8BufferAppendUInt64(&tlv, kTLVType_Error, OSStatusToPairingStatus(err));
        require_noerr(err, exit);
    }

    // Send response.

    err = TLV8BufferDetach(&tlv, outOutputPtr, outOutputLen);
    require_noerr(err, exit);

    pair_ulog(me, kLogLevelTrace, "AdminPairing server M4 -- response\n%?{end}%1{tlv8}\n",
        !log_category_enabled(me->ucat, kLogLevelChatty), kTLVDescriptors, *outOutputPtr, *outOutputLen);
    me->state = kAdminPairingStateDone;
    pair_ulog(me, kLogLevelTrace, "AdminPairing server done\n");

exit:
    CFReleaseNullSafe(pairings);
    FreeNullSafe(buf);
    CFReleaseNullSafe(info);
    TLV8BufferFree(&tlv);
    return (err);
}

//===========================================================================================================================

static OSStatus _AdminPairingServerAppendPairings(TLV8Buffer* inTLVBuffer, CFArrayRef inPairings)
{
    OSStatus err;
    CFIndex i, n;

    n = CFArrayGetCount(inPairings);
    for (i = 0; i < n; ++i) {
        CFDictionaryRef pairingInfo;
        CFStringRef identifierStr;
        const char* identifierCPtr;
        char* identifierCBuf;
        size_t identifierCLen;
        CFDataRef publicKeyData;
        PairingPermissions permissions;

        if (i > 0) {
            err = TLV8BufferAppend(inTLVBuffer, kTLVType_Separator, NULL, 0);
            require_noerr(err, exit);
        }

        pairingInfo = CFArrayGetCFDictionaryAtIndex(inPairings, i, &err);
        require(pairingInfo, exit);

        // Add identifier.

        identifierStr = CFDictionaryGetCFString(pairingInfo, CFSTR(kPairingKey_Identifier), &err);
        require(identifierStr, exit);
        err = CFStringGetOrCopyCStringUTF8(identifierStr, &identifierCPtr, &identifierCBuf, &identifierCLen);
        require_noerr(err, exit);
        err = TLV8BufferAppend(inTLVBuffer, kTLVType_Identifier, identifierCPtr, identifierCLen);
        FreeNullSafe(identifierCBuf);
        require_noerr(err, exit);

        // Add public key.

        publicKeyData = CFDictionaryGetCFData(pairingInfo, CFSTR(kPairingKey_PublicKey), &err);
        require(publicKeyData, exit);
        err = TLV8BufferAppend(inTLVBuffer, kTLVType_PublicKey, CFDataGetBytePtr(publicKeyData),
            (size_t)CFDataGetLength(publicKeyData));
        require_noerr(err, exit);

        // Add permissions.

        permissions = CFDictionaryGetUInt32(pairingInfo, CFSTR(kPairingKey_Permissions), NULL);
        err = TLV8BufferAppendUInt64(inTLVBuffer, kTLVType_Permissions, permissions);
        require_noerr(err, exit);
    }
    err = kNoErr;

exit:
    return (err);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================

typedef struct
{
    CFMutableDictionaryRef intersectACL;
    CFDictionaryRef ourACL;
    OSStatus err;

} PairingSessionIntersectACLContext;

static CFDictionaryRef _PairingSessionIntersectACL(PairingSessionRef me, CFDictionaryRef inACL, OSStatus* outErr)
{
    PairingSessionIntersectACLContext ctx = { NULL, me->acl, kNoErr };
    CFDictionaryRef result = NULL;

    // Reject if any requested ACLs aren't in the allowed ACLs.
    // The result is a dictionary of allowed ACLs that the peer is requesting.

    require_quiet(inACL && (CFDictionaryGetCount(inACL) > 0), exit);
    require_action_quiet(me->acl && (CFDictionaryGetCount(me->acl) > 0), exit, ctx.err = kPermissionErr);
    CFDictionaryApplyFunction(inACL, _PairingSessionIntersectACLApplier, &ctx);
    require_noerr_quiet(ctx.err, exit);
    result = ctx.intersectACL;
    ctx.intersectACL = NULL;

exit:
    CFReleaseNullSafe(ctx.intersectACL);
    if (outErr)
        *outErr = ctx.err;
    return (result);
}

static void _PairingSessionIntersectACLApplier(const void* inKey, const void* inValue, void* inContext)
{
    PairingSessionIntersectACLContext* const ctx = (PairingSessionIntersectACLContext*)inContext;
    CFTypeRef ourValue;

    require_noerr_quiet(ctx->err, exit);

    ourValue = CFDictionaryGetValue(ctx->ourACL, inKey);
    require_action_quiet(ourValue, exit, ctx->err = kPermissionErr);
    require_action_quiet(CFEqual(ourValue, inValue), exit, ctx->err = kPermissionErr);

    if (!ctx->intersectACL) {
        ctx->intersectACL = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        require_action(ctx->intersectACL, exit, ctx->err = kNoMemoryErr);
    }
    CFDictionarySetValue(ctx->intersectACL, inKey, ourValue);

exit:
    return;
}

//===========================================================================================================================

typedef struct
{
    CFDictionaryRef peerACL;
    OSStatus result;

} PairingSessionVerifyACLContext;

static OSStatus _PairingSessionVerifyACL(PairingSessionRef me, CFDictionaryRef inACL)
{
    PairingSessionVerifyACLContext ctx = { inACL, kNoErr };

    require_quiet(me->acl && (CFDictionaryGetCount(me->acl) > 0), exit);
    require_action_quiet(inACL, exit, ctx.result = kPermissionErr);
    CFDictionaryApplyFunction(me->acl, _PairingSessionVerifyACLApplier, &ctx);

exit:
    return (ctx.result);
}

static void _PairingSessionVerifyACLApplier(const void* inKey, const void* inValue, void* inContext)
{
    PairingSessionVerifyACLContext* const ctx = (PairingSessionVerifyACLContext*)inContext;
    CFDictionaryRef peerACL;
    Boolean b;

    (void)inValue;
    if (ctx->result)
        return;

    peerACL = ctx->peerACL;
    b = CFDictionaryGetBoolean(peerACL, inKey, NULL);
    if (b)
        return;

    // Admin ACL allows all other ACLs.

    b = CFDictionaryGetBoolean(peerACL, CFSTR(kPairingACL_Admin), NULL);
    if (b)
        return;

    // Developer ACL allows ScreenCapture.

    if (CFEqual(inKey, CFSTR(kPairingACL_ScreenCapture))) {
        b = CFDictionaryGetBoolean(peerACL, CFSTR(kPairingACL_Developer), NULL);
        if (b)
            return;
    }

    ctx->result = kPermissionErr;
}

//===========================================================================================================================

static int32_t _PairingThrottle(void)
{
    uint64_t ticksPerSec, nowTicks, deltaTicks, secs;
    int32_t result;

    pthread_mutex_lock(&gPairingGlobalLock);

    if (gPairingMaxTries > 0) {
        require_action_quiet(gPairingTries < gPairingMaxTries, exit, result = kCountErr);
        ++gPairingTries;
        result = kSkipErr;
        goto exit;
    }

    ticksPerSec = UpTicksPerSecond();
    nowTicks = UpTicks();
    if (gPairingThrottleNextTicks == 0)
        gPairingThrottleNextTicks = nowTicks;
    if (gPairingThrottleNextTicks > nowTicks) {
        deltaTicks = gPairingThrottleNextTicks - nowTicks;
        secs = deltaTicks / ticksPerSec;
        if (deltaTicks % ticksPerSec)
            ++secs;
        result = (int32_t)secs;
    } else {
        gPairingThrottleCounter = (gPairingThrottleCounter > 0) ? (gPairingThrottleCounter * 2) : 1;
        if (gPairingThrottleCounter > 10800)
            gPairingThrottleCounter = 10800; // 3 hour cap.
        gPairingThrottleNextTicks = nowTicks + (gPairingThrottleCounter * ticksPerSec);
        result = kSkipErr;
    }

exit:
    pthread_mutex_unlock(&gPairingGlobalLock);
    return (result);
}

//===========================================================================================================================

static void _PairingResetThrottle(void)
{
    pthread_mutex_lock(&gPairingGlobalLock);
    gPairingThrottleNextTicks = 0;
    gPairingThrottleCounter = 0;
    gPairingTries = 0;
    pthread_mutex_unlock(&gPairingGlobalLock);
}

//===========================================================================================================================

static OSStatus
_PairingFindResumeState(
    uint64_t inSessionID,
    uint8_t** outIdentifierPtr,
    size_t* outIdentifierLen,
    uint8_t* inSharedSecretBuf,
    size_t inSharedSecretLen)
{
    OSStatus err;
    PairingResumeStateRef state;
    uint64_t nowTicks;

    nowTicks = UpTicks();
    pthread_mutex_lock(&gPairingGlobalLock);
    for (state = gPairingResumeStateList; state; state = state->next) {
        if (state->sessionID == inSessionID) {
            require_action_quiet(nowTicks < state->expireTicks, exit, err = kTimeoutErr);
            require_action(inSharedSecretLen == sizeof(state->sharedSecret), exit, err = kSizeErr);

            if (outIdentifierPtr && outIdentifierLen) {
                uint8_t* ptr;

                require_action(state->peerIdentifierLen > 0, exit, err = kSizeErr);
                ptr = (uint8_t*)malloc(state->peerIdentifierLen);
                require_action(ptr, exit, err = kNoMemoryErr);
                memcpy(ptr, state->peerIdentifierPtr, state->peerIdentifierLen);
                *outIdentifierPtr = ptr;
                *outIdentifierLen = state->peerIdentifierLen;
            }
            memcpy(inSharedSecretBuf, state->sharedSecret, inSharedSecretLen);
            err = kNoErr;
            goto exit;
        }
    }
    err = kNotFoundErr;

exit:
    pthread_mutex_unlock(&gPairingGlobalLock);
    return (err);
}

//===========================================================================================================================

static void _PairingRemoveResumeSessionID(uint64_t inSessionID)
{
    PairingResumeStateRef* next;
    PairingResumeStateRef curr;

    pthread_mutex_lock(&gPairingGlobalLock);
    for (next = &gPairingResumeStateList; (curr = *next) != NULL;) {
        if (curr->sessionID == inSessionID) {
            *next = curr->next;
            _PairingFreeResumeState(curr);
            continue;
        }
        next = &curr->next;
    }
    pthread_mutex_unlock(&gPairingGlobalLock);
}

//===========================================================================================================================

static OSStatus
_PairingSaveResumeState(
    PairingSessionRef me,
    const void* inPeerIdentifierPtr,
    size_t inPeerIdentifierLen,
    const void* inSelfIdentifierPtr,
    size_t inSelfIdentifierLen,
    uint64_t inSessionID,
    const void* inSharedSecretPtr,
    size_t inSharedSecretLen)
{
    OSStatus err;
    PairingResumeStateRef* next;
    PairingResumeStateRef curr;
    PairingResumeStateRef state = NULL;
    uint32_t i;

    pthread_mutex_lock(&gPairingGlobalLock);

    // Remove any previous resume state for this identifier.

    for (next = &gPairingResumeStateList; (curr = *next) != NULL;) {
        if (MemEqual(inPeerIdentifierPtr, inPeerIdentifierLen, curr->peerIdentifierPtr, curr->peerIdentifierLen) && (!(me->flags & kPairingFlag_UniqueResume) || MemEqual(inSelfIdentifierPtr, inSelfIdentifierLen, curr->selfIdentifierPtr, curr->selfIdentifierLen))) {
            *next = curr->next;
            _PairingFreeResumeState(curr);
            continue;
        }
        next = &curr->next;
    }

    // Remove the oldest states to make room for the new one.

    i = 0;
    for (next = &gPairingResumeStateList; (curr = *next) != NULL;) {
        if (++i > gPairingMaxResumeSessions) {
            *next = curr->next;
            _PairingFreeResumeState(curr);
            continue;
        }
        next = &curr->next;
    }

    // Save the new resume state.

    state = (PairingResumeStateRef)calloc(1, sizeof(*state));
    require_action(state, exit, err = kNoMemoryErr);

    require_action(inPeerIdentifierLen > 0, exit, err = kSizeErr);
    state->peerIdentifierPtr = (uint8_t*)malloc(inPeerIdentifierLen);
    require_action(state->peerIdentifierPtr, exit, err = kNoMemoryErr);
    memcpy(state->peerIdentifierPtr, inPeerIdentifierPtr, inPeerIdentifierLen);
    state->peerIdentifierLen = inPeerIdentifierLen;

    state->selfIdentifierPtr = (uint8_t*)malloc(inSelfIdentifierLen ? inSelfIdentifierLen : 1);
    require_action(state->selfIdentifierPtr, exit, err = kNoMemoryErr);
    if (inSelfIdentifierLen > 0)
        memcpy(state->selfIdentifierPtr, inSelfIdentifierPtr, inSelfIdentifierLen);
    state->selfIdentifierLen = inSelfIdentifierLen;

    require_action(inSharedSecretLen == sizeof(state->sharedSecret), exit, err = kSizeErr);
    memcpy(state->sharedSecret, inSharedSecretPtr, inSharedSecretLen);
    state->sessionID = inSessionID;
    state->expireTicks = UpTicks() + me->resumeMaxTicks;

    for (next = &gPairingResumeStateList; (curr = *next) != NULL; next = &curr->next) {
    }
    state->next = NULL;
    *next = state;
    state = NULL;
    err = kNoErr;

exit:
    pthread_mutex_unlock(&gPairingGlobalLock);
    _PairingFreeResumeState(state);
    return (err);
}

//===========================================================================================================================

static void _PairingFreeResumeState(PairingResumeStateRef inState)
{
    if (!inState)
        return;
    ForgetPtrLen(&inState->peerIdentifierPtr, &inState->peerIdentifierLen);
    ForgetPtrLen(&inState->selfIdentifierPtr, &inState->selfIdentifierLen);
    free(inState);
}

#if 0
#pragma mark -
#endif

#if (PAIRING_KEYCHAIN)
//===========================================================================================================================

OSStatus PairingSessionDeleteIdentity(PairingSessionRef me)
{
    OSStatus err;

    pthread_mutex_lock(&gPairingGlobalLock);
    err = _PairingSessionDeleteIdentity(me);
    pthread_mutex_unlock(&gPairingGlobalLock);
    return (err);
}

#if (KEYCHAIN_LITE_ENABLED)
//===========================================================================================================================

static OSStatus _PairingSessionDeleteIdentity(PairingSessionRef me)
{
    return (KeychainDeleteFormatted(
        "{"
        "%kO=%O" // class
        "%kO=%i" // type
        "%kO=%O" // service (user-visible "where").
        "%kO=%O" // matchLimit
        "}",
        kSecClass, kSecClassGenericPassword,
        kSecAttrType, me->keychainIdentityType,
        kSecAttrService, me->keychainIdentityLabel,
        kSecMatchLimit, kSecMatchLimitAll));
}
#else
//===========================================================================================================================

static OSStatus _PairingSessionDeleteIdentity(PairingSessionRef me)
{
    OSStatus err;
    CFArrayRef items;
    CFIndex i, n;
    CFDictionaryRef attrs;
    uint32_t type;
    CFDataRef persistentRef;

    // Work around <radar:8782844> (SecItemDelete and kSecMatchLimitAll) by getting all and deleting individually.

    items = (CFArrayRef)KeychainCopyMatchingFormatted(NULL,
        "{"
        "%kO=%O" // class
#if (!TARGET_IPHONE_SIMULATOR)
        "%kO=%O" // accessGroup
#endif
        "%kO=%i" // type
        "%kO=%O" // synchronizable
        "%kO=%O" // returnAttributes
        "%kO=%O" // returnRef
        "%kO=%O" // matchLimit
        "}",
        kSecClass, kSecClassGenericPassword,
#if (!TARGET_IPHONE_SIMULATOR)
        kSecAttrAccessGroup, me->keychainAccessGroup,
#endif
        kSecAttrType, me->keychainIdentityType,
        kSecAttrSynchronizable, kSecAttrSynchronizableAny,
        kSecReturnAttributes, kCFBooleanTrue,
        kSecReturnPersistentRef, kCFBooleanTrue,
        kSecMatchLimit, kSecMatchLimitAll);
    n = items ? CFArrayGetCount(items) : 0;
    for (i = 0; i < n; ++i) {
        attrs = CFArrayGetCFDictionaryAtIndex(items, i, NULL);
        check(attrs);
        if (!attrs)
            continue;

        type = (uint32_t)CFDictionaryGetInt64(attrs, kSecAttrType, &err);
        check_noerr(err);
        if (type != me->keychainIdentityType)
            continue;

        persistentRef = CFDictionaryGetCFData(attrs, kSecValuePersistentRef, &err);
        check_noerr(err);
        if (err)
            continue;

        err = KeychainDeleteItemByPersistentRef(persistentRef, attrs);
        check_noerr(err);
    }
    CFReleaseNullSafe(items);
    return (kNoErr);
}
#endif // KEYCHAIN_LITE_ENABLED
#endif // PAIRING_KEYCHAIN

//===========================================================================================================================

OSStatus
PairingSessionCopyIdentity(
    PairingSessionRef me,
    Boolean inAllowCreate,
    char** outIdentifier,
    uint8_t* outPK,
    uint8_t* outSK)
{
    OSStatus err;

    if (me->delegate.copyIdentity_f) {
        err = me->delegate.copyIdentity_f(inAllowCreate, outIdentifier, outPK, outSK, me->delegate.context);
        require_noerr_quiet(err, exit);
    } else {
#if (PAIRING_KEYCHAIN)
        err = _PairingSessionGetOrCreateIdentityKeychain(me, inAllowCreate, outIdentifier, outPK, outSK);
        require_noerr_quiet(err, exit);
#else
        dlogassert("No copyIdentity_f");
        err = kNotPreparedErr;
        goto exit;
#endif
    }

exit:
    return (err);
}

#if (PAIRING_KEYCHAIN)
//===========================================================================================================================

static OSStatus
_PairingSessionGetOrCreateIdentityKeychain(
    PairingSessionRef me,
    Boolean inAllowCreate,
    char** outIdentifier,
    uint8_t* outPK,
    uint8_t* outSK)
{
    OSStatus err;
    int tries, maxTries;

    pthread_mutex_lock(&gPairingGlobalLock);

    // Retry on transient failures since the identity may be created by other processes and may collide with us.
    // This is mainly an issue with debug tools since production code should be properly serialized between processes.

    err = kUnknownErr;
    maxTries = 10;
    for (tries = 1; tries <= maxTries; ++tries) {
        if (tries != 1)
            usleep(20000);
        err = _PairingSessionCopyIdentityKeychain(me, outIdentifier, outPK, outSK);
        if (!err)
            goto exit;
        if (err == errSecAuthFailed)
            break;
        require_quiet(inAllowCreate, exit);

        err = _PairingSessionCreateIdentityKeychain(me, outIdentifier, outPK, outSK);
        if (!err)
            goto exit;
        pair_ulog(me, kLogLevelInfo, "### Create %@ failed (try %d of %d): %#m\n",
            me->keychainIdentityLabel, tries, maxTries, err);
    }
    pair_ulog(me, kLogLevelWarning, "### Failed to create %@ after %d tries: %#m\n",
        me->keychainIdentityLabel, maxTries, err);

exit:
    pthread_mutex_unlock(&gPairingGlobalLock);
    return (err);
}

//===========================================================================================================================
//	_PairingSessionCopyIdentityKeychain
//
//	Assumes global pairing lock is held.
//===========================================================================================================================

static OSStatus _PairingSessionCopyIdentityKeychain(PairingSessionRef me, char** outIdentifier, uint8_t* outPK, uint8_t* outSK)
{
    OSStatus err;
    CFDictionaryRef identity;
    size_t len;
    char* identifier;
    char pkAndSKHex[132];
    const char* src;
    const char* end;

    identity = (CFDictionaryRef)KeychainCopyMatchingFormatted(&err,
        "{"
        "%kO=%O" // class
#if (!TARGET_IPHONE_SIMULATOR)
        "%kO=%O" // accessGroup
#endif
        "%kO=%i" // type
        "%kO=%O" // synchronizable
        "%kO=%O" // returnAttributes
        "%kO=%O" // returnData
        "}",
        kSecClass, kSecClassGenericPassword,
#if (!TARGET_IPHONE_SIMULATOR)
        kSecAttrAccessGroup, me->keychainAccessGroup,
#endif
        kSecAttrType, me->keychainIdentityType,
        kSecAttrSynchronizable, kSecAttrSynchronizableAny,
        kSecReturnAttributes, kCFBooleanTrue,
        kSecReturnData, (outPK || outSK) ? kCFBooleanTrue : NULL);
    require_noerr_quiet(err, exit);

    if (outIdentifier) {
        identifier = CFDictionaryCopyCString(identity, kSecAttrAccount, &err);
        require_noerr(err, exit);
        *outIdentifier = identifier;
    }

    // Parse the hex public and secret key in the format: <hex pk> "+" <hex sk>.

    if (outPK || outSK) {
        len = 0;
        CFDictionaryGetData(identity, kSecValueData, pkAndSKHex, sizeof(pkAndSKHex), &len, &err);
        src = pkAndSKHex;
        end = src + len;
        err = HexToData(src, len, kHexToData_DefaultFlags, outPK, 32, &len, NULL, &src);
        require_noerr(err, exit);
        require_action(len == 32, exit, err = kSizeErr);

        if (outSK) {
            require_action((src < end) && (*src == '+'), exit, err = kMalformedErr);
            ++src;
            err = HexToData(src, (size_t)(end - src), kHexToData_DefaultFlags, outSK, 32, &len, NULL, NULL);
            require_noerr(err, exit);
            require_action(len == 32, exit, err = kSizeErr);
        }
    }

exit:
    CFReleaseNullSafe(identity);
    MemZeroSecure(pkAndSKHex, sizeof(pkAndSKHex));
    return (err);
}

//===========================================================================================================================
//	_PairingSessionCreateIdentityKeychain
//
//	Assumes global pairing lock is held.
//===========================================================================================================================

static OSStatus _PairingSessionCreateIdentityKeychain(PairingSessionRef me, char** outIdentifier, uint8_t* outPK, uint8_t* outSK)
{
    OSStatus err;
    uint8_t uuid[16];
    const char* identifierPtr;
    size_t identifierLen;
    char identifierBuf[64];
    uint8_t pk[32];
    uint8_t sk[32];
    char pkAndSKHex[132];
    char* label;
    char* identifier;

    // Delete any existing identity first to self-repair and avoid dups.

    _PairingSessionDeleteIdentity(me);

    // Create a new identity and store it in the Keychain.

    identifierPtr = me->identifierPtr;
    identifierLen = me->identifierLen;
    if (!identifierPtr || (identifierLen == 0)) {
        UUIDGet(uuid);
        UUIDtoCString(uuid, false, identifierBuf);
        identifierPtr = identifierBuf;
        identifierLen = strlen(identifierBuf);
    }
    Ed25519_make_key_pair(pk, sk);
    SNPrintF(pkAndSKHex, sizeof(pkAndSKHex), "%.3H+%.3H", pk, 32, 32, sk, 32, 32);

    label = NULL;
    ASPrintF(&label, "%@: %.*s", me->keychainIdentityLabel, (int)identifierLen, identifierPtr);
    require_action(label, exit, err = kNoMemoryErr);

    err = KeychainAddFormatted(NULL,
        "{"
        "%kO=%O" // class
#if (!TARGET_IPHONE_SIMULATOR)
        "%kO=%O" // accessGroup
#endif
        "%kO=%O" // accessible
        "%kO=%i" // type (4-char-code type)
        "%kO=%s" // label (user-visible name)
        "%kO=%O" // description (user-visible kind)
        "%kO=%.*s" // account (identifier)
        "%kO=%O" // service (user-visible "where").
        "%kO=%D" // password (hex public+secret keys)
        "%kO=%O" // synchronizable
        "}",
        kSecClass, kSecClassGenericPassword,
#if (!TARGET_IPHONE_SIMULATOR)
        kSecAttrAccessGroup, me->keychainAccessGroup,
#endif
        kSecAttrAccessible, kSecAttrAccessibleAlways,
        kSecAttrType, me->keychainIdentityType,
        kSecAttrLabel, label,
        kSecAttrDescription, me->keychainIdentityDesc,
        kSecAttrAccount, (int)identifierLen, identifierPtr,
        kSecAttrService, me->keychainIdentityLabel,
        kSecValueData, pkAndSKHex, (int)strlen(pkAndSKHex),
        kSecAttrSynchronizable, (me->keychainFlags & kPairingKeychainFlag_iCloudIdentity) ? kCFBooleanTrue : NULL);
    free(label);
    require_noerr_quiet(err, exit);

    if (outIdentifier) {
        identifier = strndup(identifierPtr, identifierLen);
        require_action(identifier, exit, err = kNoMemoryErr);
        *outIdentifier = identifier;
    }
    if (outPK)
        memcpy(outPK, pk, 32);
    if (outSK)
        memcpy(outSK, sk, 32);
    pair_ulog(me, kLogLevelNotice, "Created %@: %.*s\n", me->keychainIdentityLabel, (int)identifierLen, identifierPtr);

exit:
    MemZeroSecure(sk, sizeof(sk));
    MemZeroSecure(pkAndSKHex, sizeof(pkAndSKHex));
    return (err);
}

//===========================================================================================================================

CFDictionaryRef
PairingSessionCopyPeer(
    PairingSessionRef me,
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    OSStatus* outErr)
{
    CFDictionaryRef result = NULL;
    OSStatus err;
    CFArrayRef peers;
    CFDictionaryRef peer;

    peers = _PairingSessionCopyPeers(me, inIdentifierPtr, inIdentifierLen, &err);
    require_noerr_quiet(err, exit);
    require_action_quiet(CFArrayGetCount(peers) > 0, exit, err = kNotFoundErr);

    peer = CFArrayGetCFDictionaryAtIndex(peers, 0, &err);
    require_noerr(err, exit);
    CFRetain(peer);
    result = peer;

exit:
    CFReleaseNullSafe(peers);
    if (outErr)
        *outErr = err;
    return (result);
}

//===========================================================================================================================

CFArrayRef PairingSessionCopyPeers(PairingSessionRef me, OSStatus* outErr)
{
    return (_PairingSessionCopyPeers(me, NULL, 0, outErr));
}

static CFArrayRef
_PairingSessionCopyPeers(
    PairingSessionRef me,
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    OSStatus* outErr)
{
    CFArrayRef peers = NULL;
    CFMutableArrayRef tempPeers = NULL;
    OSStatus err;
    CFArrayRef results;
    CFIndex i, n;
    CFDictionaryRef attrs;
    CFStringRef identifier;
    CFDataRef data = NULL;
    CFDataRef data2;
    CFDictionaryRef info;
    CFMutableDictionaryRef peer;
    uint8_t pk[32];
    size_t len;

    if (inIdentifierLen == kSizeCString)
        inIdentifierLen = strlen((const char*)inIdentifierPtr);

    pthread_mutex_lock(&gPairingGlobalLock);

    // Note: Keychain APIs prevent getting all items and secret data. So get all attributes then get secret data individually.

    results = (CFArrayRef)KeychainCopyMatchingFormatted(&err,
        "{"
        "%kO=%O" // class
#if (!TARGET_IPHONE_SIMULATOR)
        "%kO=%O" // accessGroup
#endif
        "%kO=%i" // type
        "%kO=%?.*s" // account (identifier)
        "%kO=%O" // synchronizable
        "%kO=%O" // returnAttributes
        "%kO=%O" // matchLimit
        "}",
        kSecClass, kSecClassGenericPassword,
#if (!TARGET_IPHONE_SIMULATOR)
        kSecAttrAccessGroup, me->keychainAccessGroup,
#endif
        kSecAttrType, me->keychainPeerType,
        kSecAttrAccount, (inIdentifierPtr && (inIdentifierLen > 0)), (int)inIdentifierLen, inIdentifierPtr,
        kSecAttrSynchronizable, kSecAttrSynchronizableAny,
        kSecReturnAttributes, kCFBooleanTrue,
        kSecMatchLimit, kSecMatchLimitAll);
    if (err == errSecItemNotFound)
        err = kNoErr;
    require_noerr_quiet(err, exit);

    tempPeers = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    require_action(tempPeers, exit, err = kNoMemoryErr);

    n = results ? CFArrayGetCount(results) : 0;
    for (i = 0; i < n; ++i) {
        attrs = CFArrayGetCFDictionaryAtIndex(results, i, &err);
        check_noerr(err);
        if (err)
            continue;

        peer = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require_action(peer, exit, err = kNoMemoryErr);

        // Identifier

        identifier = CFDictionaryGetCFString(attrs, kSecAttrAccount, &err);
        require_noerr(err, skip);
        CFDictionarySetValue(peer, CFSTR(kPairingKey_Identifier), identifier);

        // Public Key

        data = (CFDataRef)KeychainCopyMatchingFormatted(&err,
            "{"
            "%kO=%O" // class
#if (!TARGET_IPHONE_SIMULATOR)
            "%kO=%O" // accessGroup
#endif
            "%kO=%i" // type
            "%kO=%O" // synchronizable
            "%kO=%O" // account (identifier)
            "%kO=%O" // returnData
            "}",
            kSecClass, kSecClassGenericPassword,
#if (!TARGET_IPHONE_SIMULATOR)
            kSecAttrAccessGroup, me->keychainAccessGroup,
#endif
            kSecAttrType, me->keychainPeerType,
            kSecAttrSynchronizable, kSecAttrSynchronizableAny,
            kSecAttrAccount, identifier,
            kSecReturnData, kCFBooleanTrue);
        require_noerr_quiet(err, skip);

        len = 0;
        err = HexToData(CFDataGetBytePtr(data), (size_t)CFDataGetLength(data), kHexToData_DefaultFlags,
            pk, sizeof(pk), &len, NULL, NULL);
        require_noerr(err, skip);
        require(len == 32, skip);
        CFDictionarySetData(peer, CFSTR(kPairingKey_PublicKey), pk, len);

        // Info

        data2 = CFDictionaryGetCFData(attrs, kSecAttrGeneric, NULL);
        if (data2) {
            info = (CFDictionaryRef)CFPropertyListCreateWithData(NULL, data2, 0, NULL, NULL);
            if (info && !CFIsType(info, CFDictionary)) {
                dlogassert("Bad pairing info type: %@", info);
                CFRelease(info);
                info = NULL;
            }
            if (info) {
                CFTypeRef obj;

                CFDictionarySetValue(peer, CFSTR(kPairingKey_Info), info);

                obj = CFDictionaryGetValue(info, kPairingKeychainInfoKey_Permissions);
                if (obj)
                    CFDictionarySetValue(peer, CFSTR(kPairingKey_Permissions), obj);

                CFRelease(info);
            }
        }

        CFArrayAppendValue(tempPeers, peer);

    skip:
        CFRelease(peer);
        CFForget(&data);
    }

    peers = tempPeers;
    tempPeers = NULL;
    err = kNoErr;

exit:
    CFReleaseNullSafe(tempPeers);
    CFReleaseNullSafe(results);
    CFForget(&data);
    if (outErr)
        *outErr = err;
    pthread_mutex_unlock(&gPairingGlobalLock);
    return (peers);
}

//===========================================================================================================================

OSStatus PairingSessionDeletePeer(PairingSessionRef me, const void* inIdentifierPtr, size_t inIdentifierLen)
{
    OSStatus err;

    pthread_mutex_lock(&gPairingGlobalLock);
    err = _PairingSessionDeletePeer(me, inIdentifierPtr, inIdentifierLen);
    pthread_mutex_unlock(&gPairingGlobalLock);
    return (err);
}

#if (KEYCHAIN_LITE_ENABLED)
//===========================================================================================================================

static OSStatus _PairingSessionDeletePeer(PairingSessionRef me, const void* inIdentifierPtr, size_t inIdentifierLen)
{
    return (KeychainDeleteFormatted(
        "{"
        "%kO=%O" // class
        "%kO=%i" // type
        "%kO=%?.*s" // account (identifier)
        "%kO=%O" // matchLimit
        "}",
        kSecClass, kSecClassGenericPassword,
        kSecAttrType, me->keychainPeerType,
        kSecAttrAccount, (inIdentifierPtr && (inIdentifierLen > 0)), (int)inIdentifierLen, inIdentifierPtr,
        kSecMatchLimit, kSecMatchLimitAll));
}
#else
//===========================================================================================================================

static OSStatus _PairingSessionDeletePeer(PairingSessionRef me, const void* inIdentifierPtr, size_t inIdentifierLen)
{
    OSStatus err;
    CFArrayRef items;
    CFIndex i, n;
    CFDictionaryRef attrs;
    uint32_t type;
    char* cptr;
    CFDataRef persistentRef;
    Boolean b;

    if (inIdentifierLen == kSizeCString)
        inIdentifierLen = strlen((const char*)inIdentifierPtr);

    // Work around <radar:8782844> (SecItemDelete and kSecMatchLimitAll) by getting all and deleting individually.

    items = (CFArrayRef)KeychainCopyMatchingFormatted(NULL,
        "{"
        "%kO=%O" // class
#if (!TARGET_IPHONE_SIMULATOR)
        "%kO=%O" // accessGroup
#endif
        "%kO=%i" // type
        "%kO=%O" // synchronizable
        "%kO=%O" // returnAttributes
        "%kO=%O" // returnRef
        "%kO=%O" // matchLimit
        "}",
        kSecClass, kSecClassGenericPassword,
#if (!TARGET_IPHONE_SIMULATOR)
        kSecAttrAccessGroup, me->keychainAccessGroup,
#endif
        kSecAttrType, me->keychainPeerType,
        kSecAttrSynchronizable, kSecAttrSynchronizableAny,
        kSecReturnAttributes, kCFBooleanTrue,
        kSecReturnPersistentRef, kCFBooleanTrue,
        kSecMatchLimit, kSecMatchLimitAll);
    n = items ? CFArrayGetCount(items) : 0;
    for (i = 0; i < n; ++i) {
        attrs = CFArrayGetCFDictionaryAtIndex(items, i, NULL);
        check(attrs);
        if (!attrs)
            continue;

        type = (uint32_t)CFDictionaryGetInt64(attrs, kSecAttrType, &err);
        check_noerr(err);
        if (type != me->keychainPeerType)
            continue;

        if (inIdentifierPtr) {
            cptr = CFDictionaryCopyCString(attrs, kSecAttrAccount, &err);
            check_noerr(err);
            if (err)
                continue;
            b = (strnicmpx((const char*)inIdentifierPtr, inIdentifierLen, cptr) == 0);
            free(cptr);
            if (!b)
                continue;
        }

        persistentRef = CFDictionaryGetCFData(attrs, kSecValuePersistentRef, &err);
        check_noerr(err);
        if (err)
            continue;

        err = KeychainDeleteItemByPersistentRef(persistentRef, attrs);
        check_noerr(err);
    }
    CFReleaseNullSafe(items);
    return (kNoErr);
}
#endif // KEYCHAIN_LITE_ENABLED
#endif // PAIRING_KEYCHAIN

//===========================================================================================================================

OSStatus PairingSessionFindPeer(PairingSessionRef me, const void* inIdentifierPtr, size_t inIdentifierLen, uint8_t* outPK)
{
    return (PairingSessionFindPeerEx(me, inIdentifierPtr, inIdentifierLen, outPK, NULL));
}

//===========================================================================================================================

static OSStatus
PairingSessionFindPeerEx(
    PairingSessionRef me,
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    uint8_t* outPK,
    CFDictionaryRef* outACL)
{
    OSStatus err;
    uint64_t permissions;

    if (me->delegate.findPeer_f) {
        err = me->delegate.findPeer_f(inIdentifierPtr, inIdentifierLen, outPK, &permissions, me->delegate.context);
        require_noerr_action_quiet(err, exit, err = kNotFoundErr);

        if (outACL && (permissions & kPairingPermissionsAdmin)) {
            CFMutableDictionaryRef aclDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            require_action(aclDict, exit, err = kNoMemoryErr);
            CFDictionarySetValue(aclDict, CFSTR(kPairingACL_Admin), kCFBooleanTrue);
            *outACL = aclDict;
        }
    } else {
        (void)outACL;
#if (PAIRING_KEYCHAIN)
        err = _PairingSessionFindPeerKeychain(me, inIdentifierPtr, inIdentifierLen, outPK, outACL);
        require_noerr_action_quiet(err, exit, err = kNotFoundErr);
#else
        dlogassert("No findPeer_f");
        err = kNotPreparedErr;
        goto exit;
#endif
    }

exit:
    return (err);
}

#if (PAIRING_KEYCHAIN)
//===========================================================================================================================

static OSStatus
_PairingSessionFindPeerKeychain(
    PairingSessionRef me,
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    uint8_t* outPK,
    CFDictionaryRef* outACL)
{
    OSStatus err;
    CFDictionaryRef peerInfo;
    CFDataRef data;
    size_t len;
    CFDictionaryRef infoDict = NULL;
    CFDictionaryRef acl = NULL;

    pthread_mutex_lock(&gPairingGlobalLock);

    if (inIdentifierLen == kSizeCString)
        inIdentifierLen = strlen((const char*)inIdentifierPtr);
    peerInfo = (CFDictionaryRef)KeychainCopyMatchingFormatted(&err,
        "{"
        "%kO=%O" // class
#if (!TARGET_IPHONE_SIMULATOR)
        "%kO=%O" // accessGroup
#endif
        "%kO=%i" // type
        "%kO=%O" // synchronizable
        "%kO=%.*s" // account (identifier)
        "%kO=%O" // returnAttributes
        "%kO=%O" // returnData
        "}",
        kSecClass, kSecClassGenericPassword,
#if (!TARGET_IPHONE_SIMULATOR)
        kSecAttrAccessGroup, me->keychainAccessGroup,
#endif
        kSecAttrType, me->keychainPeerType,
        kSecAttrSynchronizable, kSecAttrSynchronizableAny,
        kSecAttrAccount, (int)inIdentifierLen, inIdentifierPtr,
        kSecReturnAttributes, kCFBooleanTrue,
        kSecReturnData, kCFBooleanTrue);
    require_noerr_quiet(err, exit);

    // Extract key.

    data = CFDictionaryGetCFData(peerInfo, kSecValueData, NULL);
    require_action_quiet(data, exit, err = kValueErr);

    len = 0;
    err = HexToData(CFDataGetBytePtr(data), (size_t)CFDataGetLength(data), kHexToData_DefaultFlags,
        outPK, 32, &len, NULL, NULL);
    require_noerr(err, exit);
    require_action(len == 32, exit, err = kSizeErr);

    // Extract info.

    data = CFDictionaryGetCFData(peerInfo, kSecAttrGeneric, NULL);
    if (data) {
        infoDict = (CFDictionaryRef)CFPropertyListCreateWithData(kCFAllocatorDefault, data, kCFPropertyListImmutable, NULL, NULL);
        require_action(infoDict != NULL, exit, err = kUnsupportedDataErr);
    }

    if (CFIsType(infoDict, CFDictionary)) {
        acl = CFDictionaryGetCFDictionary(infoDict, CFSTR(kPairingKey_ACL), NULL);
    }
    if (outACL) {
        CFRetainNullSafe(acl);
        *outACL = acl;
    }

exit:
    CFReleaseNullSafe(infoDict);
    CFReleaseNullSafe(peerInfo);
    pthread_mutex_unlock(&gPairingGlobalLock);
    return (err);
}

//===========================================================================================================================

static CFArrayRef _PairingSessionListControllersKeychain(PairingSessionRef me, OSStatus* outErr)
{
    CFArrayRef result = NULL;
    OSStatus err;
    CFMutableArrayRef controllers;
    CFArrayRef peers = NULL;
    CFIndex i, n;
    CFMutableDictionaryRef controllerInfo = NULL;

    controllers = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    require_action(controllers, exit, err = kNoMemoryErr);

    peers = PairingSessionCopyPeers(me, &err);
    n = peers ? CFArrayGetCount(peers) : 0;
    for (i = 0; i < n; ++i) {
        CFDictionaryRef peer;
        CFStringRef identifier;
        CFDataRef publicKey;
        PairingPermissions permissions;

        peer = CFArrayGetCFDictionaryAtIndex(peers, i, &err);
        require_noerr(err, exit);

        controllerInfo = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require_action(controllerInfo, exit, err = kNoMemoryErr);

        identifier = CFDictionaryGetCFString(peer, CFSTR(kPairingKey_Identifier), &err);
        require_quiet(identifier, exit);
        CFDictionarySetValue(controllerInfo, CFSTR(kPairingKey_Identifier), identifier);

        publicKey = CFDictionaryGetCFData(peer, CFSTR(kPairingKey_PublicKey), &err);
        require_quiet(publicKey, exit);
        CFDictionarySetValue(controllerInfo, CFSTR(kPairingKey_PublicKey), publicKey);

        permissions = CFDictionaryGetUInt32(peer, CFSTR(kPairingKey_Permissions), NULL);
        CFDictionarySetUInt64(controllerInfo, CFSTR(kPairingKey_Permissions), permissions);

        CFArrayAppendValue(controllers, controllerInfo);
        CFRelease(controllerInfo);
        controllerInfo = NULL;
    }

    result = controllers;
    controllers = NULL;

exit:
    CFReleaseNullSafe(controllerInfo);
    CFReleaseNullSafe(peers);
    CFReleaseNullSafe(controllers);
    if (outErr)
        *outErr = err;
    return (result);
}

//===========================================================================================================================

static OSStatus _PairingSessionRemoveControllerKeychain(PairingSessionRef me, CFDictionaryRef inInfo)
{
    OSStatus err;
    const uint8_t* identifierPtr;
    size_t identifierLen;
    void* identifierBuf = NULL;

    identifierPtr = CFDictionaryGetOrCopyBytes(inInfo, CFSTR(kPairingKey_Identifier), &identifierLen, &identifierBuf, &err);
    require_quiet(identifierPtr, exit);

    err = _PairingSessionDeletePeer(me, identifierPtr, identifierLen);
    require_noerr(err, exit);

exit:
    FreeNullSafe(identifierBuf);
    return (err);
}

//===========================================================================================================================

static OSStatus _PairingSessionSaveControllerKeychain(PairingSessionRef me, CFDictionaryRef inInfo)
{
    OSStatus err;
    CFDataRef pkData;
    const uint8_t* identifierPtr;
    size_t identifierLen;
    void* identifierBuf = NULL;
    PairingPermissions permissions;

    identifierPtr = CFDictionaryGetOrCopyBytes(inInfo, CFSTR(kPairingKey_Identifier), &identifierLen, &identifierBuf, &err);
    require_quiet(identifierPtr, exit);

    pkData = CFDictionaryGetCFData(inInfo, CFSTR(kPairingKey_PublicKey), &err);
    require_quiet(pkData, exit);
    require_action(CFDataGetLength(pkData) == 32, exit, err = kSizeErr);

    permissions = CFDictionaryGetUInt32(inInfo, CFSTR(kPairingKey_Permissions), NULL);

    err = _PairingSessionSavePeerKeychain(me, identifierPtr, identifierLen, CFDataGetBytePtr(pkData), permissions);
    require_noerr(err, exit);

exit:
    FreeNullSafe(identifierBuf);
    return (err);
}
#endif // PAIRING_KEYCHAIN

//===========================================================================================================================

OSStatus
PairingSessionSavePeer(
    PairingSessionRef me,
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    const uint8_t* inPK)
{
    OSStatus err;

    if (inIdentifierLen == kSizeCString)
        inIdentifierLen = strlen((const char*)inIdentifierPtr);

    if (me->delegate.savePeer_f) {
        err = me->delegate.savePeer_f(inIdentifierPtr, inIdentifierLen, inPK, me->delegate.context);
        require_noerr_quiet(err, exit);
    } else {
#if (PAIRING_KEYCHAIN)
        err = _PairingSessionSavePeerKeychain(me, inIdentifierPtr, inIdentifierLen, inPK, kPairingPermissionsDefault);
        require_noerr_quiet(err, exit);
#else
        dlogassert("No savePeer_f");
        err = kNotPreparedErr;
        goto exit;
#endif
    }

exit:
    return (err);
}

#if (PAIRING_KEYCHAIN)
//===========================================================================================================================

static OSStatus
_PairingSessionSavePeerKeychain(
    PairingSessionRef me,
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    const uint8_t* inPK,
    PairingPermissions inPermissions)
{
    OSStatus err;
    CFMutableDictionaryRef infoDict;
    CFDataRef infoData = NULL;
    char hex[(32 * 2) + 1];
    char* label;

    pthread_mutex_lock(&gPairingGlobalLock);

    infoDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_action(infoDict, exit, err = kNoMemoryErr);
    if (inPermissions != kPairingPermissionsDefault) {
        CFDictionarySetUInt64(infoDict, kPairingKeychainInfoKey_Permissions, inPermissions);
    }
    if (inPermissions & kPairingPermissionsAdmin) {
        CFMutableDictionaryRef aclDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require_action(aclDict, exit, err = kNoMemoryErr);
        CFDictionarySetValue(aclDict, CFSTR(kPairingACL_Admin), kCFBooleanTrue);
        CFDictionarySetValue(infoDict, CFSTR(kPairingKey_ACL), aclDict);
        CFRelease(aclDict);
    }
    if (CFDictionaryGetCount(infoDict) > 0) {
        infoData = CFPropertyListCreateData(NULL, infoDict, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
        CFRelease(infoDict);
        require_action(infoData, exit, err = kUnknownErr);
    } else {
        CFRelease(infoDict);
    }

    // Delete any existing peer first to self-repair and avoid dups.

    _PairingSessionDeletePeer(me, inIdentifierPtr, inIdentifierLen);

    // Create the new Keychain entry.

    label = NULL;
    ASPrintF(&label, "%@: %.*s", me->keychainPeerLabel, (int)inIdentifierLen, inIdentifierPtr);
    require_action(label, exit, err = kNoMemoryErr);

    DataToHexCString(inPK, 32, hex);

    err = KeychainAddFormatted(NULL,
        "{"
        "%kO=%O" // class
#if (!TARGET_IPHONE_SIMULATOR)
        "%kO=%O" // accessGroup
#endif
        "%kO=%O" // accessible
        "%kO=%i" // type (4-char-code type)
        "%kO=%s" // label (user-visible name)
        "%kO=%O" // description (user-visible kind)
        "%kO=%.*s" // account (identifier)
        "%kO=%O" // service (user-visible "where")
        "%kO=%O" // generic (custom info)
        "%kO=%D" // valueData (hex public key)
        "%kO=%O" // synchronizable
        "}",
        kSecClass, kSecClassGenericPassword,
#if (!TARGET_IPHONE_SIMULATOR)
        kSecAttrAccessGroup, me->keychainAccessGroup,
#endif
        kSecAttrAccessible, (me->keychainFlags & kPairingKeychainFlag_HighSecurity) ? kSecAttrAccessibleWhenUnlocked : kSecAttrAccessibleAlways,
        kSecAttrType, me->keychainPeerType,
        kSecAttrLabel, label,
        kSecAttrDescription, me->keychainPeerDesc,
        kSecAttrAccount, (int)inIdentifierLen, inIdentifierPtr,
        kSecAttrService, me->keychainPeerLabel,
        kSecAttrGeneric, infoData,
        kSecValueData, hex, 32 * 2,
        kSecAttrSynchronizable, (me->keychainFlags & kPairingKeychainFlag_iCloudPeers) ? kCFBooleanTrue : NULL);
    free(label);
    require_quiet(err != errSecAuthFailed, exit);
    require_noerr(err, exit);

exit:
    if (err)
        pair_ulog(me, kLogLevelWarning, "### Save %@ %.*s failed: %#m\n",
            me->keychainPeerLabel, (int)inIdentifierLen, inIdentifierPtr, err);
    CFReleaseNullSafe(infoData);
    pthread_mutex_unlock(&gPairingGlobalLock);
    return (err);
}

//===========================================================================================================================

OSStatus
PairingSessionUpdatePeerInfo(
    PairingSessionRef me,
    const void* inIdentifierPtr,
    size_t inIdentifierLen,
    CFDictionaryRef inInfo)
{
    OSStatus err;
    CFMutableDictionaryRef query = NULL;
    CFDataRef infoData;

    if (inIdentifierLen == kSizeCString)
        inIdentifierLen = strlen((const char*)inIdentifierPtr);

    pthread_mutex_lock(&gPairingGlobalLock);

    err = CFPropertyListCreateFormatted(NULL, &query,
        "{"
        "%kO=%O" // class
#if (!TARGET_IPHONE_SIMULATOR)
        "%kO=%O" // accessGroup
#endif
        "%kO=%i" // type
        "%kO=%.*s" // account (identifier)
        "%kO=%O" // synchronizable
        "}",
        kSecClass, kSecClassGenericPassword,
#if (!TARGET_IPHONE_SIMULATOR)
        kSecAttrAccessGroup, me->keychainAccessGroup,
#endif
        kSecAttrType, me->keychainPeerType,
        kSecAttrAccount, (int)inIdentifierLen, inIdentifierPtr,
        kSecAttrSynchronizable, kSecAttrSynchronizableAny);
    require_noerr(err, exit);

    infoData = CFPropertyListCreateData(NULL, inInfo, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
    require_action(infoData, exit, err = kUnknownErr);

    err = KeychainUpdateFormatted(query,
        "{"
        "%kO=%O" // generic (custom info)
        "}",
        kSecAttrGeneric, infoData);
    CFRelease(infoData);
    require_noerr_quiet(err, exit);

exit:
    if (err)
        pair_ulog(me, kLogLevelWarning, "### Update %@ %.*s info failed: %#m\n",
            me->keychainPeerLabel, (int)inIdentifierLen, inIdentifierPtr, err);
    CFReleaseNullSafe(query);
    pthread_mutex_unlock(&gPairingGlobalLock);
    return (err);
}
#endif // PAIRING_KEYCHAIN
