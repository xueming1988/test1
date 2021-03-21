/*
	Copyright (C) 2013-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "HIDUtils.h"

#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>

#include "CFUtils.h"
#include "CommonServices.h"
#include "StringUtils.h"
#include "ThreadUtils.h"
#include "UUIDUtils.h"

#include CF_RUNTIME_HEADER
#include LIBDISPATCH_HEADER

//===========================================================================================================================
//	Internals
//===========================================================================================================================

static pthread_mutex_t gVirtualHIDLock = PTHREAD_MUTEX_INITIALIZER;
static CFMutableArrayRef gVirtualHIDDevices = NULL;
static uint8_t gNextDeviceID = 0;
static void* gHIDSession = NULL;

ulog_define(HIDUtils, kLogLevelVerbose, kLogFlags_Default, "HID", NULL);
#define hid_dlog(LEVEL, ...) dlogc(&log_category_from_name(HIDUtils), (LEVEL), __VA_ARGS__)
#define hid_ulog(LEVEL, ...) ulog(&log_category_from_name(HIDUtils), (LEVEL), __VA_ARGS__)

//===========================================================================================================================
//	HIDDevice
//===========================================================================================================================

struct HIDDevicePrivate {
    CFRuntimeBase base; // CF type info. Must be first.
    CFNumberRef countryCode; // Country code.
    CFStringRef displayUUID; // Display this HID device is associated with (or NULL if none).
    CFStringRef name; // Name of the device (e.g. "ShuttleXpress").
    CFNumberRef productID; // USB product ID of device.
    CFDataRef reportDescriptor; // HID descriptor to describe reports for this device.
    CFNumberRef sampleRate; // Sample rate of the device.
    CFNumberRef vendorID; // USB vendor ID of device.
    uint8_t hidDeviceID; // Locally unique ID
};

static void _HIDDeviceGetTypeID(void* inContext);
static void _HIDDeviceFinalize(CFTypeRef inCF);

static dispatch_once_t gHIDDeviceInitOnce = 0;
static CFTypeID gHIDDeviceTypeID = _kCFRuntimeNotATypeID;
static const CFRuntimeClass kHIDDeviceClass = {
    0, // version
    "HIDDevice", // className
    NULL, // init
    NULL, // copy
    _HIDDeviceFinalize, // finalize
    NULL, // equal -- NULL means pointer equality.
    NULL, // hash  -- NULL means pointer hash.
    NULL, // copyFormattingDesc
    NULL, // copyDebugDesc
    NULL, // reclaim
    NULL // refcount
};

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	HIDCopyDevices
//===========================================================================================================================

CFArrayRef HIDCopyDevices(OSStatus* outErr)
{
    OSStatus err;
    CFArrayRef value = NULL;

    if (gVirtualHIDDevices) {
        value = CFArrayCreateCopy(NULL, gVirtualHIDDevices);
        require_action(value, exit, err = kNoMemoryErr);
    } else {
        value = CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
        require_action(value, exit, err = kNoMemoryErr);
    }
    err = kNoErr;

exit:
    if (outErr)
        *outErr = err;
    return (value);
}

//===========================================================================================================================
//	HIDSetSession
//===========================================================================================================================

void HIDSetSession(void* inContext)
{
    gHIDSession = inContext;
}

#if 0
#pragma mark -
#pragma mark == HIDDevice ==
#endif

//===========================================================================================================================
//	HIDDeviceGetTypeID
//===========================================================================================================================

CFTypeID HIDDeviceGetTypeID(void)
{
    dispatch_once_f(&gHIDDeviceInitOnce, NULL, _HIDDeviceGetTypeID);
    return (gHIDDeviceTypeID);
}

static void _HIDDeviceGetTypeID(void* inContext)
{
    (void)inContext;

    gHIDDeviceTypeID = _CFRuntimeRegisterClass(&kHIDDeviceClass);
    check(gHIDDeviceTypeID != _kCFRuntimeNotATypeID);
}

//===========================================================================================================================
//	HIDDeviceCreateVirtual
//===========================================================================================================================

OSStatus HIDDeviceCreateVirtual(HIDDeviceRef* outDevice, CFDictionaryRef inProperties)
{
    OSStatus err;
    HIDDeviceRef me;
    size_t extraLen;
    CFTypeRef property;
    uint8_t uuid[16];
    char cstr[128];
    int64_t s64;

    extraLen = sizeof(*me) - sizeof(me->base);
    me = (HIDDeviceRef)_CFRuntimeCreateInstance(NULL, HIDDeviceGetTypeID(), (CFIndex)extraLen, NULL);
    require_action(me, exit, err = kNoMemoryErr);
    memset(((uint8_t*)me) + sizeof(me->base), 0, extraLen);

    if (inProperties) {
        err = CFDictionaryGetUUID(inProperties, kHIDDeviceProperty_DisplayUUID, uuid);
        if (!err) {
            UUIDtoCString(uuid, false, cstr);
            me->displayUUID = CFStringCreateWithCString(NULL, cstr, kCFStringEncodingUTF8);
            require_action(me->displayUUID, exit, err = kUnknownErr);
        }

        property = CFDictionaryGetCFString(inProperties, kHIDDeviceProperty_Name, NULL);
        if (property) {
            CFRetain(property);
            me->name = (CFStringRef)property;
        }

        property = CFDictionaryCopyCFData(inProperties, kHIDDeviceProperty_ReportDescriptor, NULL, NULL);
        if (property)
            me->reportDescriptor = (CFDataRef)property;

        s64 = CFDictionaryGetInt64(inProperties, kHIDDeviceProperty_SampleRate, &err);
        if (!err) {
            me->sampleRate = CFNumberCreateInt64(s64);
            require_action(me->sampleRate, exit, err = kUnknownErr);
        }
    }

    *outDevice = me;
    me = NULL;
    err = kNoErr;

exit:
    CFReleaseNullSafe(me);
    return (err);
}

//===========================================================================================================================
//	_HIDDeviceFinalize
//===========================================================================================================================

static void _HIDDeviceFinalize(CFTypeRef inCF)
{
    HIDDeviceRef const me = (HIDDeviceRef)inCF;

    ForgetCF(&me->countryCode);
    ForgetCF(&me->displayUUID);
    ForgetCF(&me->name);
    ForgetCF(&me->productID);
    ForgetCF(&me->reportDescriptor);
    ForgetCF(&me->sampleRate);
    ForgetCF(&me->vendorID);
    hid_dlog(kLogLevelVerbose, "HID device finalized\n");
}

//===========================================================================================================================
//	HIDDeviceCopyProperty
//===========================================================================================================================

CFTypeRef HIDDeviceCopyProperty(HIDDeviceRef inDevice, CFStringRef inProperty, CFTypeRef inQualifier, OSStatus* outErr)
{
    OSStatus err;
    CFTypeRef value = NULL;

    (void)inQualifier;

    if (0) {
    }

    // CountryCode

    else if (CFEqual(inProperty, kHIDDeviceProperty_CountryCode)) {
        value = inDevice->countryCode;
        if (value)
            CFRetain(value);
    }

    // DisplayUUID

    else if (CFEqual(inProperty, kHIDDeviceProperty_DisplayUUID)) {
        value = inDevice->displayUUID;
        require_action_quiet(value, exit, err = kNotFoundErr);
        CFRetain(value);
    }

    // ProductID

    else if (CFEqual(inProperty, kHIDDeviceProperty_ProductID)) {
        value = inDevice->productID;
        if (value)
            CFRetain(value);
    }

    // Name

    else if (CFEqual(inProperty, kHIDDeviceProperty_Name)) {
        value = inDevice->name;
        CFRetain(value);
    }

    // ReportDescriptor

    else if (CFEqual(inProperty, kHIDDeviceProperty_ReportDescriptor)) {
        value = inDevice->reportDescriptor;
        if (value) {
            CFRetain(value);
        } else {
            err = kNotFoundErr;
            goto exit;
        }
    }

    // SampleRate

    else if (CFEqual(inProperty, kHIDDeviceProperty_SampleRate)) {
        value = inDevice->sampleRate;
        if (value)
            CFRetain(value);
    }

    // VendorID

    else if (CFEqual(inProperty, kHIDDeviceProperty_VendorID)) {
        value = inDevice->vendorID;
        if (value)
            CFRetain(value);
    }

    // Unknown...

    else {
        err = kNotFoundErr;
        goto exit;
    }
    err = kNoErr;

exit:
    if (outErr)
        *outErr = err;
    return (value);
}

//===========================================================================================================================
//	HIDDeviceCopyID
//===========================================================================================================================

CFStringRef HIDDeviceCopyID(HIDDeviceRef inDevice)
{
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%X"), inDevice->hidDeviceID);
}

//===========================================================================================================================
//	HIDDeviceSetProperty
//===========================================================================================================================

OSStatus HIDDeviceSetProperty(HIDDeviceRef inDevice, CFStringRef inProperty, CFTypeRef inQualifier, CFTypeRef inValue)
{
    OSStatus err;
    int64_t s64;
    CFNumberRef num;

    (void)inQualifier;

    if (0) {
    }

    // CountryCode

    else if (CFEqual(inProperty, kHIDDeviceProperty_CountryCode)) {
        s64 = CFGetInt64(inValue, &err);
        require_noerr(err, exit);

        num = CFNumberCreateInt64(s64);
        require_action(num, exit, err = kUnknownErr);

        CFReleaseNullSafe(inDevice->countryCode);
        inDevice->countryCode = num;
    }

    // DisplayUUID

    else if (CFEqual(inProperty, kHIDDeviceProperty_DisplayUUID)) {
        require_action(!inValue || CFIsType(inValue, CFString), exit, err = kTypeErr);
        CFRetainNullSafe(inValue);
        CFReleaseNullSafe(inDevice->displayUUID);
        inDevice->displayUUID = (CFStringRef)inValue;
    }

    // Name

    else if (CFEqual(inProperty, kHIDDeviceProperty_Name)) {
        require_action(!inValue || CFIsType(inValue, CFString), exit, err = kTypeErr);
        CFRetainNullSafe(inValue);
        CFReleaseNullSafe(inDevice->name);
        inDevice->name = (CFStringRef)inValue;
    }

    // ProductID

    else if (CFEqual(inProperty, kHIDDeviceProperty_ProductID)) {
        s64 = CFGetInt64(inValue, &err);
        require_noerr(err, exit);

        num = CFNumberCreateInt64(s64);
        require_action(num, exit, err = kUnknownErr);

        CFReleaseNullSafe(inDevice->productID);
        inDevice->productID = num;
    }

    // ReportDescriptor

    else if (CFEqual(inProperty, kHIDDeviceProperty_ReportDescriptor)) {
        require_action(!inValue || CFIsType(inValue, CFData), exit, err = kTypeErr);
        CFRetainNullSafe(inValue);
        CFReleaseNullSafe(inDevice->reportDescriptor);
        inDevice->reportDescriptor = (CFDataRef)inValue;
    }

    // SampleRate

    else if (CFEqual(inProperty, kHIDDeviceProperty_SampleRate)) {
        s64 = CFGetInt64(inValue, &err);
        require_noerr(err, exit);

        num = CFNumberCreateInt64(s64);
        require_action(num, exit, err = kUnknownErr);

        CFReleaseNullSafe(inDevice->sampleRate);
        inDevice->sampleRate = num;
    }

    // VendorID

    else if (CFEqual(inProperty, kHIDDeviceProperty_VendorID)) {
        s64 = CFGetInt64(inValue, &err);
        require_noerr(err, exit);

        num = CFNumberCreateInt64(s64);
        require_action(num, exit, err = kUnknownErr);

        CFReleaseNullSafe(inDevice->vendorID);
        inDevice->vendorID = num;
    }

    // Unknown...

    else {
        err = kNotHandledErr;
        goto exit;
    }
    err = kNoErr;

exit:
    return (err);
}

//===========================================================================================================================
//	HIDDevicePostReport
//===========================================================================================================================

OSStatus
AirPlayReceiverSessionSendHIDReport(
    void* inSession,
    uint32_t deviceUID,
    const uint8_t* inPtr,
    size_t inLen);

OSStatus HIDDevicePostReport(HIDDeviceRef inDevice, const void* inReportPtr, size_t inReportLen)
{
    OSStatus err = kNoErr;
    if (gHIDSession)
        err = AirPlayReceiverSessionSendHIDReport(gHIDSession, inDevice->hidDeviceID, inReportPtr, inReportLen);
    return err;
}

#if 0
#pragma mark -
#pragma mark == Utils ==
#endif

//===========================================================================================================================
//	HIDRegisterDevice
//===========================================================================================================================

OSStatus HIDRegisterDevice(HIDDeviceRef inDevice)
{
    OSStatus err;

    pthread_mutex_lock(&gVirtualHIDLock);
    inDevice->hidDeviceID = gNextDeviceID++;

    if (!gVirtualHIDDevices) {
        gVirtualHIDDevices = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
        require_action(gVirtualHIDDevices, exit, err = kNoMemoryErr);
    }
    CFArrayAppendValue(gVirtualHIDDevices, inDevice);
    hid_ulog(kLogLevelNotice, "Registered HID %''@, %X\n", inDevice->name, inDevice->hidDeviceID);
    err = kNoErr;

exit:
    pthread_mutex_unlock(&gVirtualHIDLock);
    return (err);
}

//===========================================================================================================================
//	HIDDeregisterDevice
//===========================================================================================================================

OSStatus HIDDeregisterDevice(HIDDeviceRef inDevice)
{
    CFIndex i, n;
    HIDDeviceRef device;

    pthread_mutex_lock(&gVirtualHIDLock);

    n = gVirtualHIDDevices ? CFArrayGetCount(gVirtualHIDDevices) : 0;
    for (i = 0; i < n; ++i) {
        device = (HIDDeviceRef)CFArrayGetValueAtIndex(gVirtualHIDDevices, i);
        if (device == inDevice) {
            hid_ulog(kLogLevelNotice, "Deregistered HID %''@, %X\n", inDevice->name, inDevice->hidDeviceID);
            CFArrayRemoveValueAtIndex(gVirtualHIDDevices, i);
            --i;
            --n;
        }
    }
    if (n == 0) {
        ForgetCF(&gVirtualHIDDevices);
        gNextDeviceID = 0;
    }

    pthread_mutex_unlock(&gVirtualHIDLock);
    return (kNoErr);
}

//===========================================================================================================================
//	HIDPostReport
//===========================================================================================================================

OSStatus HIDPostReport(uint32_t inUID, const void* inReportPtr, size_t inReportLen)
{
    OSStatus err = kNoErr;
    if (gHIDSession)
        err = AirPlayReceiverSessionSendHIDReport(gHIDSession, inUID, inReportPtr, inReportLen);
    return err;
}
