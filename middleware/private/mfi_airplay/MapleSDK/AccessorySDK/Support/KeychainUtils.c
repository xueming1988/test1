/*
	Copyright (C) 2011-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "KeychainUtils.h"

#include "CFUtils.h"
#include "PrintFUtils.h"

//===========================================================================================================================
//	KeychainAddFormatted
//===========================================================================================================================

OSStatus KeychainAddFormatted(CFTypeRef* outResult, const char* inAttributesFormat, ...)
{
    OSStatus err;
    va_list args;

    va_start(args, inAttributesFormat);
    err = KeychainAddFormattedVAList(outResult, inAttributesFormat, args);
    va_end(args);
    return (err);
}

//===========================================================================================================================
//	KeychainAddFormattedVAList
//===========================================================================================================================

OSStatus KeychainAddFormattedVAList(CFTypeRef* outResult, const char* inAttributesFormat, va_list inArgs)
{
    OSStatus err;
    CFMutableDictionaryRef attributesDict;

    err = CFPropertyListCreateFormattedVAList(NULL, &attributesDict, inAttributesFormat, inArgs);
    require_noerr(err, exit);

    err = SecItemAdd(attributesDict, outResult);
    CFRelease(attributesDict);

exit:
    return (err);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	KeychainCopyMatchingFormatted
//===========================================================================================================================

CFTypeRef KeychainCopyMatchingFormatted(OSStatus* outErr, const char* inFormat, ...)
{
    CFTypeRef result;
    va_list args;

    va_start(args, inFormat);
    result = KeychainCopyMatchingFormattedVAList(outErr, inFormat, args);
    va_end(args);
    return (result);
}

//===========================================================================================================================
//	KeychainCopyMatchingFormattedVAList
//===========================================================================================================================

CFTypeRef KeychainCopyMatchingFormattedVAList(OSStatus* outErr, const char* inFormat, va_list inArgs)
{
    CFTypeRef result;
    OSStatus err;
    CFMutableDictionaryRef query;

    result = NULL;

    err = CFPropertyListCreateFormattedVAList(NULL, &query, inFormat, inArgs);
    require_noerr(err, exit);

    err = SecItemCopyMatching(query, &result);
    CFRelease(query);

exit:
    if (outErr)
        *outErr = err;
    return (result);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	KeychainDeleteFormatted
//===========================================================================================================================

OSStatus KeychainDeleteFormatted(const char* inFormat, ...)
{
    OSStatus err;
    va_list args;

    va_start(args, inFormat);
    err = KeychainDeleteFormattedVAList(inFormat, args);
    va_end(args);
    return (err);
}

//===========================================================================================================================
//	KeychainDeleteFormattedVAList
//===========================================================================================================================

OSStatus KeychainDeleteFormattedVAList(const char* inFormat, va_list inArgs)
{
    OSStatus err;
    CFMutableDictionaryRef query;

    err = CFPropertyListCreateFormattedVAList(NULL, &query, inFormat, inArgs);
    require_noerr(err, exit);

    err = SecItemDelete(query);
    CFRelease(query);

exit:
    return (err);
}

#if (!KEYCHAIN_LITE_ENABLED)
//===========================================================================================================================
//	KeychainDeleteItemByPersistentRef
//===========================================================================================================================

OSStatus KeychainDeleteItemByPersistentRef(CFDataRef inRef, CFDictionaryRef inAttrs)
{
    OSStatus err;

#if (TARGET_OS_MACOSX && !COMMON_SERVICES_NO_CORE_SERVICES)
    // Work around <radar:17355300> by checking if the item is kSecAttrSynchronizable or not.
    // If it's kSecAttrSynchronizable then a use persistent ref with SecItemDelete/KeychainDeleteFormatted.
    // If it's not kSecAttrSynchronizable, work around <radar:12559935> by using SecKeychainItemDelete.

    if (CFDictionaryGetBoolean(inAttrs, kSecAttrSynchronizable, NULL)) {
        err = KeychainDeleteFormatted("{%kO=%O}", kSecValuePersistentRef, inRef);
        require_noerr(err, exit);
    } else {
        SecKeychainItemRef itemRef;

        err = SecKeychainItemCopyFromPersistentReference(inRef, &itemRef);
        require_noerr(err, exit);

        err = SecKeychainItemDelete(itemRef);
        CFRelease(itemRef);
        require_noerr(err, exit);
    }
#else
    (void)inAttrs;

    err = KeychainDeleteFormatted("{%kO=%O}", kSecValuePersistentRef, inRef);
    require_noerr(err, exit);
#endif

exit:
    return (err);
}
#endif

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	KeychainUpdateFormatted
//===========================================================================================================================

OSStatus KeychainUpdateFormatted(CFTypeRef inRefOrQuery, const char* inAttributesToUpdateFormat, ...)
{
    OSStatus err;
    va_list args;

    va_start(args, inAttributesToUpdateFormat);
    err = KeychainUpdateFormattedVAList(inRefOrQuery, inAttributesToUpdateFormat, args);
    va_end(args);
    return (err);
}

//===========================================================================================================================
//	KeychainUpdateFormattedVAList
//===========================================================================================================================

OSStatus KeychainUpdateFormattedVAList(CFTypeRef inRefOrQuery, const char* inAttributesToUpdateFormat, va_list inArgs)
{
    OSStatus err;
    CFMutableDictionaryRef attributesToUpdateDict;

    err = CFPropertyListCreateFormattedVAList(NULL, &attributesToUpdateDict, inAttributesToUpdateFormat, inArgs);
    require_noerr(err, exit);

    err = SecItemUpdate((CFDictionaryRef)inRefOrQuery, attributesToUpdateDict);
    CFRelease(attributesToUpdateDict);

exit:
    return (err);
}

#if 0
#pragma mark -
#endif

#if (!EXCLUDE_UNIT_TESTS)
//===========================================================================================================================
//	KeychainUtils_Test
//===========================================================================================================================

OSStatus KeychainUtils_Test(int inPrint)
{
    OSStatus err;
    CFTypeRef results;

    KeychainDeleteFormatted(
        "{"
        "%kO=%O"
        "%kO=%O"
        "%kO=%O"
        "}",
        kSecClass, kSecClassGenericPassword,
        kSecAttrAccount, CFSTR("00:11:22:aa:bb:cc"),
        kSecAttrService, CFSTR("Keychain Test Device"));

    err = KeychainAddFormatted(
        NULL,
        "{"
        "%kO=%O" // 1
        "%kO=%O" // 2
        "%kO=%O" // 3
        "%kO=%O" // 4
        "%kO=%D" // 5
        "%kO=%O" // 5
        "}",
        kSecClass, kSecClassGenericPassword, // 1
        kSecAttrAccount, CFSTR("00:11:22:aa:bb:cc"), // 2
        kSecAttrDescription, CFSTR("Keychain Test Device password"), // 3
        kSecAttrService, CFSTR("Keychain Test Device"), // 4
        kSecValueData, "password", 8, // 5
        kSecAttrLabel, CFSTR("Keychain Test")); // 6
    require_noerr(err, exit);

    results = KeychainCopyMatchingFormatted(&err,
        "{"
        "%kO=%O" // 1
        "%kO=%O" // 2
        "%kO=%O" // 3
        "}",
        kSecClass, kSecClassGenericPassword, // 1
        kSecAttrAccount, CFSTR("00:11:22:aa:bb:cc"), // 2
        kSecReturnAttributes, kCFBooleanTrue); // 4
    require_noerr(err, exit);
    if (inPrint)
        FPrintF(stderr, "%@\n", results);
    CFRelease(results);

    results = KeychainCopyMatchingFormatted(&err,
        "{"
        "%kO=%O" // 1
        "%kO=%O" // 2
        "%kO=%O" // 3
        "%kO=%O" // 4
        "%kO=%O" // 5
        "}",
        kSecClass, kSecClassGenericPassword, // 1
        kSecAttrAccount, CFSTR("00:11:22:aa:bb:cc"), // 2
        kSecReturnData, kCFBooleanTrue, // 3
        kSecReturnAttributes, kCFBooleanTrue, // 4
        kSecReturnRef, kCFBooleanTrue); // 5
    require_noerr(err, exit);
    if (inPrint)
        FPrintF(stderr, "%@\n", results);

    err = KeychainUpdateFormatted(results, "{%kO=%D}", kSecValueData, "test", 4);
    CFRelease(results);
    require_noerr(err, exit);

    results = KeychainCopyMatchingFormatted(&err,
        "{"
        "%kO=%O" // 1
        "%kO=%O" // 2
        "%kO=%O" // 3
        "%kO=%O" // 4
        "%kO=%O" // 5
        "}",
        kSecClass, kSecClassGenericPassword, // 1
        kSecAttrDescription, CFSTR("Keychain Test Device password"), // 2
        kSecReturnData, kCFBooleanTrue, // 3
        kSecReturnAttributes, kCFBooleanTrue, // 4
        kSecMatchLimitAll, kCFBooleanTrue); // 5
    require_noerr(err, exit);
    if (inPrint)
        FPrintF(stderr, "%@\n", results);
    CFRelease(results);

    KeychainDeleteFormatted(
        "{"
        "%kO=%O"
        "%kO=%O"
        "%kO=%O"
        "}",
        kSecClass, kSecClassGenericPassword,
        kSecAttrAccount, CFSTR("00:11:22:aa:bb:cc"),
        kSecAttrService, CFSTR("Keychain Test Device"));

exit:
    printf("KeychainUtils_Test: %s\n", !err ? "PASSED" : "FAILED");
    return (err);
}
#endif // !EXCLUDE_UNIT_TESTS
