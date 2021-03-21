//
//  AirPlayReceiverMatrics.c
//  AirPlayLP-mac
//
//  Copyright © 2018 Apple Inc. All rights reserved.
//

#include "AirPlayReceiverMetrics.h"
#include "AirPlayReceiverServerPriv.h"
#include "AirPlayReceiverSessionPriv.h"
#include "CFUtils.h"
#include "PrintFUtils.h"
#include "StringUtils.h"

ulog_define(AirPlayReceiverMatrics, kLogLevelNotice, kLogFlags_Default, "AirPlay", NULL);
#define aprm_ucat() &log_category_from_name(AirPlayReceiverMatrics)
#define aprm_ulog(LEVEL, ...) ulog(aprm_ucat(), (LEVEL), __VA_ARGS__)
#define aprm_dlog(LEVEL, ...) dlogc(aprm_ucat(), (LEVEL), __VA_ARGS__)

// Version created on this date.
#define METRICS_VERSION "170907"

static void appendJSONLine(const void* key, const void* value, void* context);
static CFStringRef copyJSONValueDescription(CFTypeRef value);
static CFStringRef createJSONValueFromNumber(CFNumberRef num);
static void dumpArray(const void* value, void* context);
static void dumpDictionary(const void* key, const void* value, void* context);
static CFStringRef metricsDumpSupplement(CFMutableStringRef str, CFTypeRef type);
static HTTPStatus _requestSendJSON(HTTPConnectionRef inCnx, HTTPMessageRef inRequest, const char* str, OSStatus* outErr);
static HTTPStatus _requestSendWithContentType(HTTPConnectionRef inCnx,
    HTTPMessageRef inRequest,
    const char* inContentType,
    const char* str,
    OSStatus* outErr);
//========================
// appendJSONLine
//   Called by CFDictionaryApplyFunction.
//   Appends a JSON line based on the provided
//   key-value.
//========================
static void appendJSONLine(const void* key, const void* value, void* context)
{
    CFTypeRef cfKey = (CFTypeRef)key;
    CFTypeRef cfValue = (CFTypeRef)value;
    CFMutableStringRef str = (CFMutableStringRef)context;

    CFStringRef valueDesc = NULL;

    require(CFGetTypeID(cfKey) == CFStringGetTypeID(), exit);

    valueDesc = copyJSONValueDescription(cfValue);
    require(valueDesc, exit);

    CFStringAppendF(str, ",\n\"%@\": %@", cfKey, valueDesc);
    CFRelease(valueDesc);
exit:
    return;
}

//========================
// copyJSONValueDescription
//   Returns a CFString representation suitable
//   as a JSON value.
//========================
static CFStringRef copyJSONValueDescription(CFTypeRef value)
{
    CFStringRef desc = NULL;
    CFTypeID typeID = CFGetTypeID(value);

    if (typeID == CFBooleanGetTypeID()) {
        desc = (CFBooleanRef)value == kCFBooleanFalse ? CFSTR("false") : CFSTR("true");
        CFRetain(desc);
    } else if (typeID == CFNumberGetTypeID()) {
        desc = createJSONValueFromNumber((CFNumberRef)value);
    } else if (typeID == CFStringGetTypeID()) {
        desc = CFStringCreateF(NULL, "\"%@\"", value);
    }

    if (!desc) {
        desc = (CFStringRef)CFRetain(CFSTR("\"?\""));
    }

    return desc;
}

//========================
// createJSONValueFromNumber
//   Convert the given number to a string
//   suitable as a JSON value.
//========================
static CFStringRef createJSONValueFromNumber(CFNumberRef num)
{
    CFStringRef resultStr = NULL;
    OSStatus err = kNoErr;

    switch (CFNumberGetType(num)) {

    case kCFNumberSInt8Type:
    case kCFNumberCharType:
    case kCFNumberSInt16Type:
    case kCFNumberSInt32Type:
    case kCFNumberSInt64Type:
    case kCFNumberShortType:
    case kCFNumberIntType:
    case kCFNumberLongType:
    case kCFNumberLongLongType:
    case kCFNumberCFIndexType: {
        long long i;
        if (CFNumberGetValue(num, kCFNumberSInt64Type, &i))
            resultStr = CFStringCreateF(&err, "%lld", i);
    } break;

    case kCFNumberFloat32Type:
    case kCFNumberFloat64Type:
    case kCFNumberFloatType:
    case kCFNumberDoubleType:
    default: {
        double f;
        if (CFNumberGetValue(num, kCFNumberDoubleType, &f))
            resultStr = CFStringCreateF(&err, "%f", f);
    } break;
    }

    if (!resultStr) {
        resultStr = (CFStringRef)CFRetain(CFSTR("0"));
    }

    return resultStr;
}

static void dumpArray(const void* value, void* context)
{
    CFMutableStringRef str = (CFMutableStringRef)context;
    CFTypeRef cfValue = (CFTypeRef)value;

    metricsDumpSupplement(str, cfValue);
    CFStringAppendF(str, "\n");
}

static void dumpDictionary(const void* key, const void* value, void* context)
{
    CFMutableStringRef str = (CFMutableStringRef)context;
    CFTypeRef cfKey = (CFTypeRef)key;
    CFTypeRef cfValue = (CFTypeRef)value;

    metricsDumpSupplement(str, cfKey);
    CFStringAppendF(str, ":");
    metricsDumpSupplement(str, cfValue);
    CFStringAppendF(str, "\n");
}

static CFStringRef metricsDumpSupplement(CFMutableStringRef str, CFTypeRef type)
{
    CFTypeID typeID = CFGetTypeID(type);

    if (typeID == CFBooleanGetTypeID()) {
        CFStringAppendF(str, "%s", ((CFBooleanRef)type == kCFBooleanFalse ? "False" : "True"));
    }

    else if (typeID == CFDataGetTypeID()) {
        CFDataRef data = (CFDataRef)type;
        CFStringAppendF(str, "%.1H", CFDataGetBytePtr(data), (int)CFDataGetLength(data), 1024);
    }

    else if (typeID == CFArrayGetTypeID()) {
        CFArrayRef theArray = (CFArrayRef)type;
        CFIndex theArrayCount = CFArrayGetCount(theArray);

        CFStringAppendF(str, "[\n");

        if (theArrayCount > 1)
            CFArrayApplyFunction(theArray, CFRangeMake(0, theArrayCount - 1), dumpArray, str);

        if (theArrayCount > 0)
            metricsDumpSupplement(str, CFArrayGetValueAtIndex(theArray, theArrayCount - 1));

        CFStringAppendF(str, "]");
    }

    else if (typeID == CFDictionaryGetTypeID()) {
        CFDictionaryApplyFunction((CFDictionaryRef)type, dumpDictionary, str);
    }

    else {
        CFStringAppendF(str, "%@", type);
    }

    return (CFStringRef)str;
}

static HTTPStatus _requestSendJSON(HTTPConnectionRef inCnx, HTTPMessageRef inRequest, const char* str, OSStatus* outErr)
{
    return _requestSendWithContentType(inCnx, inRequest, kMIMEType_JSON, str, outErr);
}

static HTTPStatus _requestSendWithContentType(
    HTTPConnectionRef inCnx,
    HTTPMessageRef inRequest,
    const char* inContentType,
    const char* str, OSStatus* outErr)
{
    OSStatus err = kNoErr;
    HTTPMessageRef response = inCnx->responseMsg;
    HTTPStatus status;
    const char* httpProtocol;
    char* strCopy = strdup(str);

    httpProtocol = (strnicmp_prefix(inRequest->header.protocolPtr, inRequest->header.protocolLen, "HTTP") == 0)
        ? "HTTP/1.1"
        : kAirTunesHTTPVersionStr;
    err = HTTPHeader_InitResponse(&response->header, httpProtocol, kHTTPStatus_OK, NULL);
    require_noerr_action(err, exit, status = kHTTPStatus_InternalServerError);
    response->bodyLen = 0;

    err = HTTPMessageSetBodyPtr(response, inContentType, strCopy, strlen(strCopy));
    strCopy = NULL;
    status = kHTTPStatus_OK;

exit:
    if (strCopy)
        free(strCopy);
    if (outErr)
        *outErr = err;

    return status;
}

//========================
// AirPlayReceiverMetricsSendJSON
//========================

HTTPStatus AirPlayReceiverMetricsSendJSON(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest, CFDictionaryRef inDict)
{
    static const char* jsonError = "{\"error\": true}";
    char buffer[1024];
    HTTPStatus status;
    CFMutableStringRef json = NULL;

    // Default response in case of error.
    strlcpy(buffer, jsonError, sizeof(buffer));

    require(inDict, exit);

    json = CFStringCreateMutable(kCFAllocatorDefault, 0);
    require(inDict, exit);

    CFStringAppendCString(json, "{\n\"version\": " METRICS_VERSION, kCFStringEncodingUTF8);
    CFDictionaryApplyFunction(inDict, appendJSONLine, json);
    CFStringAppendCString(json, "\n}", kCFStringEncodingUTF8);

    Boolean success = CFStringGetCString(json, buffer, sizeof(buffer), kCFStringEncodingUTF8);
    if (!success) {
        strlcpy(buffer, jsonError, sizeof(buffer));
    }

exit:
    status = _requestSendJSON(inCnx->httpCnx, inRequest, buffer, NULL);

    CFReleaseNullSafe(json);

    return status;
}

#if (AIRPLAY_METRICS)

static HTTPStatus _requestSendHTML(HTTPConnectionRef inCnx, HTTPMessageRef inRequest, const char* str, OSStatus* outErr);
static char const* _audioFormatDescription(AirPlayAudioFormat format);
static void _updateTimeStamps(AirPlayReceiverConnectionRef inCnx, CFDictionaryRef tsdict);

static void _updateTimeStamps(AirPlayReceiverConnectionRef inCnx, CFDictionaryRef tsdict)
{
#define kMaxTimeStampEntries 100
    if (inCnx->server == NULL)
        return;

    if (inCnx->server->recordedTimeStamps == NULL) {
        inCnx->server->recordedTimeStamps = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    }

    if (inCnx->server->recordedTimeStamps != NULL) {
        CFArrayAppendValue(inCnx->server->recordedTimeStamps, tsdict);
        if (CFArrayGetCount(inCnx->server->recordedTimeStamps) > kMaxTimeStampEntries) {
            CFArrayRemoveValueAtIndex(inCnx->server->recordedTimeStamps, 0);
        }
    }
}

void AirPlayReceiverMetricsSupplementTimeStampF(AirPlayReceiverConnectionRef inCnx, const char* inFormat, ...)
{
    va_list args;
    CFStringRef supplement;

    va_start(args, inFormat);
    supplement = CFStringCreateV(NULL, inFormat, args);
    va_end(args);

    AirPlayReceiverMetricsSupplementTimeStamp(inCnx, supplement);
    CFRelease(supplement);
}

void AirPlayReceiverMetricsSupplementTimeStamp(AirPlayReceiverConnectionRef inCnx, CFTypeRef supplement)
{
    const void* keys[] = { CFSTR("supplement") };
    const void* values[] = { supplement };
    CFDictionaryRef tsdict = CFDictionaryCreate(NULL, keys, values, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_quiet(tsdict != NULL, exit);

    _updateTimeStamps(inCnx, tsdict);
exit:
    CFReleaseNullSafe(tsdict);
}

void AirPlayReceiverMetricsUpdateTimeStamp(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    const char* const methodPtr = inRequest->header.methodPtr;
    size_t const methodLen = inRequest->header.methodLen;
    const char* const pathPtr = inRequest->header.url.pathPtr;
    size_t const pathLen = inRequest->header.url.pathLen;
    CFStringRef method = NULL;
    CFStringRef path = NULL;
    CFStringRef addr = NULL;
    CFMutableDictionaryRef tsdict = NULL;

    require_quiet(strnicmp_suffix(pathPtr, pathLen, "/metrics"), exit);
    require_quiet(strnicmp_suffix(pathPtr, pathLen, "/history"), exit);
    require_quiet(strnicmp_suffix(pathPtr, pathLen, "/feedback"), exit);
    require_quiet(strnicmp_prefix(pathPtr, pathLen, "/favicon"), exit);
    require_quiet(strnicmp_prefix(pathPtr, pathLen, "/apple-touch-icon-precomposed.png"), exit);
    require_quiet(strnicmp_prefix(pathPtr, pathLen, "/apple-touch-icon.png"), exit);
    require_quiet(strnicmp_prefix(pathPtr, pathLen, "/info"), exit);
    require_quiet(strnicmp_prefix(pathPtr, pathLen, "/command"), exit);

    tsdict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require_quiet(tsdict != NULL, exit);

    method = CFStringCreateWithFormat(NULL, NULL, CFSTR("%.*s"), (int)methodLen, methodPtr);
    path = CFStringCreateWithFormat(NULL, NULL, CFSTR("%.*s"), (int)pathLen, pathPtr);
    addr = CFStringCreateF(NULL, "%##a", &(inCnx->httpCnx->peerAddr));

    CFDictionarySetDouble(tsdict, CFSTR("ts"), CFAbsoluteTimeGetCurrent());
    CFDictionarySetValue(tsdict, CFSTR("method"), method);
    CFDictionarySetValue(tsdict, CFSTR("path"), path);
    CFDictionarySetValue(tsdict, CFSTR("addr"), addr);

    _updateTimeStamps(inCnx, tsdict);

exit:
    CFReleaseNullSafe(addr);
    CFReleaseNullSafe(method);
    CFReleaseNullSafe(path);
    CFReleaseNullSafe(tsdict);
}

HTTPStatus AirPlayReceiverMetrics(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
#define kBufferSize (16 * 1024)

    static char* buffer = NULL;
    static const char* kInRange = "inRange";
    static const char* kOutOfRange = "outOfRange";

    char mode[64] = { 0 };
    char clock[64] = { 0 };
    char clientName[128] = { 0 };
    char firmware[256] = { 0 };
    char ts[1024 * 8] = { 0 };
    char duration[24] = { 0 };
    char name[PATH_MAX + 1] = { 0 };
    OSStatus err;
    AirPlayReceiverSessionRef session = NULL;
    CFDictionaryRef metrics = NULL;
    int64_t audioFormat = kAirPlayAudioFormat_Invalid;
    int64_t nodeSize = 0;
    int64_t totalNodes = 0;
    int64_t busyNodes = 0;
    int64_t zombieNodes = 0;
    int64_t avgBuffer_ms = 0;
    double audioCompressionPercent = 0;
    double audioBitRate = 0;

    int64_t late = 0;
    int64_t lost = 0;
    int64_t unrecovered = 0;
    int64_t glitches = 0;
    int64_t glitchPeriods = 0;
    int64_t skew = 0;
    int64_t samplesPerAdjust = 0;
    int64_t skewResetCount = 0;
    int64_t audioBytesPoolTotal = 0;
    int64_t audioBytesPoolUsed = 0;
    double audioBytesPoolPercent = 0.0;

    int64_t numRemoteConnections = 0;
    int64_t numRemoteCommands = 0;

    int64_t rtpOffset = 0;
    int64_t lastPacketTS = 0;
    int64_t lastRenderPacketTS = 0;
    int64_t lastRenderGapStartTS = 0;
    int64_t lastRenderGapLimitTS = 0;
    int64_t lastNetSeq = 0;
    int64_t lastNetRTP = 0;

    const char* tsRangeStr = "";
    uint64_t clockTimeline = 0;
    double clockSecs = 0;
    double anchorTime = 0;
    uint64_t anchorTimeLine = 0;
    uint64_t anchorRTP = 0;
    const char* timelinesMatch = "";
    double startupTime = 0;
    const char* startUpGood = "";

    static const char* html = "<!DOCTYPE html>"
                              "<html>"
                              "<head>"
                              "<meta charset=\"utf-8\">"
                              "<meta http-equiv=\"refresh\" content=\"2\">"
                              "<style type=\"text/css\">"
                              "body { background-color: #fff; color: #111; margin: 10; padding: 1; font:monospaced; }"
                              ".wrapper { width: 80%%; margin: 20px auto 40px auto; }"
                              ".datatable { width: 100%%; border: 1px solid #d6dde6; border-collapse: collapse; font:monospaced }"
                              ".datatable td { border: 1px solid #d6dde6; padding: 0.3em; }"
                              ".datatable th { border: 1px solid #828282; background-color: #bcbcbc; font-weight: bold; text-align: left; padding-left: 0.3em; }"
                              ".datatable caption { font: bold 110%% Arial, Helvetica, sans-serif; color: #33517a; text-align: left; padding: 0.4em 0 0.8em 0; }"
                              ".datatable tr:nth-child(odd) { background-color: #dfe7f2; color: #000000; }"
                              ".outOfRange { color: red }"
                              ".inRange { color: green }"
                              "</style>"
                              "</head>"
                              "<body>"
                              "<div class=wrapper>"
                              "<h3>%s, v%s %s</h3>"
                              "<table class=datatable>"
                              "<tr> <td>Client / Duration</td> <td>%s</td> <td>%s</td> </tr>"
                              "<tr> <td>Clock / Audio Format</td> <td>%s</td> <td>%s %s</td> </tr>"
                              "<tr> <td>Compressed Bit Rate / Startup Time</td> <td>%.1f kbs (%.1f%%)</td> <td><span class=\"%s\">%.1f ms.</span></td> </tr>"
                              "<tr> <td>Anchor timeline / Seconds (RTP)</td> <td>%llx</td> <td>%.3f (%lld)</td> </tr>"
                              "<tr> <td>Clock timeline / Seconds (&Delta;)</td> <td><span class=\"%s\">%llx</span></td> <td>%.3f (%.3f)</td> </tr>"
                              "<tr> <td>Buffered Audio</td> <td>%lld ms.</td> <td>%lld of %lld bytes (%.1f&#37)</td> </tr>"
                              "<tr> <td>Node Size (Total Nodes) / Filled Nodes (Zombies)</td> <td>%lld (%lld)</td> <td>%lld (%lld)</td> </tr>"
                              "<tr> <td>Remotes / Relays</td> <td>%lld</td> <td>%lld</td> </tr>"
                              "<tr> <td>Glitch Periods / Glitches</td> <td>%lld</td> <td>%lld</td> </tr>"
                              "<tr> <td>Late / Lost Packets</td> <td>%lld</td> <td>%lld</td> </tr>"
                              "<tr> <td>Unrecovered Packets / Skew Reset Count </td> <td>%lld</td> <td>%lld</td> </tr>"
                              "<tr> <td>Skew / Samples per Adjust</td> <td>%lld</td> <td>%lld</td> </tr>"
                              "<tr> <td>Last Net Seq / RTP</td> <td>%lld (0x%llx)</td> <td>%lld (0x%llx)</td> </tr>"
                              "<tr> <td>RTP Offset / Packet RTP (32 bit sum) </td> <td>%lld</td> <td>%lld (%lld)</td> </tr>"
                              "<tr> <td>Adjusted Packet RTP / AU Request - Limit</td> <td><span class=\"%s\">%lld</span></td> <td>%lld -   %lld</td> </tr>"
                              "</table>"
                              "%s"
                              "</body>"
                              "</html>\n";

    AirPlayGetDeviceName(inCnx->server, name, sizeof(name));

    AirPlayReceiverServerGetCString(inCnx->server, CFSTR(kAirPlayKey_FirmwareRevision), NULL, firmware, sizeof(firmware), NULL);

    session = AirPlaySessionManagerCopyMasterSession(inCnx->server->sessionMgr);
    if (session != NULL) {
        metrics = (CFDictionaryRef)AirPlayReceiverSessionCopyProperty(session, kCFObjectFlagDirect, CFSTR(kAirPlayProperty_Metrics), NULL, NULL);
    }

    if (metrics) {
        CFDictionaryGetCString(metrics, CFSTR(kAirPlayMetricKey_Mode), mode, sizeof(mode), NULL);

        // Anchor
        anchorTime = CFDictionaryGetDouble(metrics, CFSTR(kAirPlayMetricKey_AnchorTime), NULL);
        anchorTimeLine = CFDictionaryGetUInt64(metrics, CFSTR(kAirPlayMetricKey_AnchorTimeline), NULL);
        anchorRTP = CFDictionaryGetUInt64(metrics, CFSTR(kAirPlayMetricKey_AnchorRTP), NULL);

        // Clock information
        CFDictionaryGetCString(metrics, CFSTR(kAirPlayMetricKey_ClockingMethod), clock, sizeof(clock), NULL);
        clockTimeline = CFDictionaryGetUInt64(metrics, CFSTR(kAirPlayMetricKey_ClockTimeline), NULL);
        clockSecs = CFDictionaryGetDouble(metrics, CFSTR(kAirPlayMetricKey_ClockTimeSecs), NULL);
        timelinesMatch = clockTimeline == anchorTimeLine ? kInRange : kOutOfRange;

        CFDictionaryGetCString(metrics, CFSTR(kAirPlayMetricKey_ClientName), clientName, sizeof(clientName), NULL);

        audioFormat = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_AudioFormat), NULL);
        nodeSize = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_NodeSize), NULL);
        totalNodes = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_TotalNodes), NULL);
        busyNodes = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_BusyNodes), NULL);
        zombieNodes = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_ZombieNodes), NULL);

        // Remote Connections
        numRemoteConnections = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_RemoteConnectionsCount), NULL);
        numRemoteCommands = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_RemoteCommandsCount), NULL);

        avgBuffer_ms = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_AvgBuffer), NULL);
        audioBytesPoolTotal = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_AudioBytesPoolSize), NULL);
        audioBytesPoolUsed = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_AudioBytesPoolUsed), NULL);
        audioBytesPoolPercent = audioBytesPoolTotal ? 100.0 * audioBytesPoolUsed / audioBytesPoolTotal : 0.0;

        audioCompressionPercent = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_CompressionPercent), NULL) / 100.0;

        // Convert to kilobits per second.
        //Fix: assumes 44100 16-bit stereo.
        audioBitRate = 44100 * 2 * 2 * 8 * audioCompressionPercent / 100 / 1000.0;

        late = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_LatePackets), NULL);
        lost = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_LostPackets), NULL);
        unrecovered = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_UnrecoveredPackets), NULL);
        glitches = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_Glitches), NULL);
        glitchPeriods = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_GlitchPeriods), NULL);

        skew = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_Skew), NULL);
        samplesPerAdjust = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_SamplesPerSkewAdjust), NULL);
        skewResetCount = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_SkewResetCount), NULL);

        lastPacketTS = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_lastPacketTS), NULL);
        rtpOffset = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_RTPOffset), NULL);

        lastRenderPacketTS = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_lastRenderPacketTS), NULL);
        lastRenderGapStartTS = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_lastRenderGapStartTS), NULL);
        lastRenderGapLimitTS = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_lastRenderGapLimitTS), NULL);

        lastNetSeq = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_lastNetSeq), NULL);
        lastNetRTP = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_lastNetRTP), NULL);

        tsRangeStr = lastRenderGapStartTS <= lastRenderPacketTS && lastRenderPacketTS < lastRenderGapLimitTS
            ? kInRange
            : kOutOfRange;
        {
            int64_t sessionDuration = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_DurationSecs), NULL);

            int secs = sessionDuration % 60;
            int mins = (sessionDuration % 3600) / 60;
            ;
            int hrs = (sessionDuration % 86400) / 3600;
            int days = (sessionDuration % (86400 * 30)) / 86400;

            if (days > 0) {
                snprintf(duration, sizeof(duration), "%d %02d:%02d:%02d", days, hrs, mins, secs);
            } else if (hrs > 0) {
                snprintf(duration, sizeof(duration), "%02d:%02d:%02d", hrs, mins, secs);
            } else {
                snprintf(duration, sizeof(duration), "%02d:%02d", mins, secs);
            }
        }

        startupTime = CFDictionaryGetInt64(metrics, CFSTR(kAirPlayMetricKey_StartupMSecs), NULL);
        startUpGood = startupTime <= 500 ? kInRange : kOutOfRange;

        strlcat(ts, "<p><table class=datatable>  <tr><th width=20%% scope=\"col\">Time "
                    "Stamp</th width=20%%><th>Address</th></th width=20%%><th>Method</th><th width=40%% scope=\"col\">Path</th></tr>",
            sizeof(ts));
        CFArrayRef tsa = (CFArrayRef)inCnx->server->recordedTimeStamps;
        if (tsa) {
#define kMaxHistory 80
            CFIndex count = 0;
            CFIndex idx;
            for (idx = CFArrayGetCount(tsa) - 1; idx > 0 && count++ < kMaxHistory; idx--) {
                CFDictionaryRef dict = (CFDictionaryRef)CFArrayGetTypedValueAtIndex(tsa, idx, CFDictionaryGetTypeID(), NULL);
                if (dict) {
                    char entry[1675];
                    char method[32] = { 0 };
                    char path[1024] = { 0 };
                    char addr[256] = { 0 };

                    CFDictionaryRef supplement = (CFDictionaryRef)CFDictionaryGetValue(dict, CFSTR("supplement"));
                    if (supplement) {
                        CFMutableStringRef supplementStr = CFStringCreateMutable(NULL, 0);
                        if (supplementStr) {
                            metricsDumpSupplement(supplementStr, supplement);
                            CFStringFindAndReplace(supplementStr, CFSTR("\n"), CFSTR("<br>"), CFRangeMake(0, CFStringGetLength(supplementStr)), 0x0);
                            (void)CFStringGetCString(supplementStr, path, sizeof(path), kCFStringEncodingUTF8);
                            CFRelease(supplementStr);
                        }
                        snprintf(entry, sizeof(entry), "<tr> <td></td> <td></td> <td></td> <td>%s</td> </tr>\n", path);
                    } else {
                        CFDictionaryGetCString(dict, CFSTR("method"), method, sizeof(method), NULL);
                        CFDictionaryGetCString(dict, CFSTR("path"), path, sizeof(path), NULL);
                        CFDictionaryGetCString(dict, CFSTR("addr"), addr, sizeof(addr), NULL);
                        snprintf(entry, sizeof(entry), "<tr> <td>%.3lf</td> <td>%s</td> <td>%s</td> <td>%s</td> </tr>\n", CFDictionaryGetDouble(dict, CFSTR("ts"), NULL), addr, method, path);
                    }
                    strlcat(ts, entry, sizeof(ts));
                }
            }
        }
        strlcat(ts, "</table>", sizeof(ts));

        CFRelease(metrics);
    }

    if (buffer == NULL) {
        buffer = (char*)malloc(kBufferSize);
        require_action_quiet(buffer != NULL, exit, err = kNoMemoryErr);
    }

    snprintf(buffer, kBufferSize, html, name, firmware, __DATE__, clientName, duration,
        clock, mode, _audioFormatDescription(audioFormat),
        audioBitRate, audioCompressionPercent, startUpGood, startupTime,
        anchorTimeLine, anchorTime, anchorRTP,
        timelinesMatch, clockTimeline, clockSecs, clockSecs - anchorTime,
        avgBuffer_ms, audioBytesPoolUsed, audioBytesPoolTotal, audioBytesPoolPercent,
        nodeSize, totalNodes, busyNodes, zombieNodes,
        numRemoteConnections, numRemoteCommands,
        glitchPeriods, glitches, late, lost,
        unrecovered, skewResetCount,
        skew, samplesPerAdjust,
        lastNetSeq, lastNetSeq, lastNetRTP, lastNetRTP,
        rtpOffset, lastPacketTS, (lastPacketTS + rtpOffset) & 0x0FFFFFFFF,
        tsRangeStr, lastRenderPacketTS, lastRenderGapStartTS, lastRenderGapLimitTS,
        ts);

    err = _requestSendHTML(inCnx->httpCnx, inRequest, buffer, NULL);
exit:
    CFReleaseNullSafe(session);
    return err;
}

HTTPStatus AirPlayReceiverHistory(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    char buffer[8196];
    HTTPStatus status;
    CFMutableStringRef json = NULL;
    CFArrayRef timestamps = NULL;

    // Default response in case of error.
    strlcpy(buffer, "{\"error\": true}\n", sizeof(buffer));

    require_quiet(inCnx, exit);
    require_quiet(inCnx->server, exit);

    timestamps = (CFArrayRef)AirPlayReceiverServerCopyProperty(inCnx->server, 0, CFSTR(kAirPlayProperty_MetricsHistory), NULL, NULL);
    require_quiet(timestamps, exit);

    json = CFStringCreateMutable(kCFAllocatorDefault, 0);
    require_quiet(json, exit);

    CFStringAppendCString(json, "{\n\"version\": " METRICS_VERSION, kCFStringEncodingUTF8);
    if (timestamps) {
        CFIndex idx;
        for (idx = CFArrayGetCount(timestamps) - 1; idx > 0; idx--) {
            CFDictionaryRef dict = (CFDictionaryRef)CFArrayGetTypedValueAtIndex(timestamps, idx, CFDictionaryGetTypeID(), NULL);
            if (dict == NULL)
                continue;
            CFStringAppendCString(json, "{\n", kCFStringEncodingUTF8);
            CFDictionaryApplyFunction(dict, appendJSONLine, json);
            CFStringAppendCString(json, "\n}", kCFStringEncodingUTF8);
        }
    }
    CFStringAppendCString(json, "\n}", kCFStringEncodingUTF8);
    if (!CFStringGetCString(json, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
        strlcpy(buffer, "{\"error\": true}\n", sizeof(buffer));
    }
exit:
    status = inCnx ? _requestSendJSON(inCnx->httpCnx, inRequest, buffer, NULL) : kHTTPStatus_BadRequest;

    CFReleaseNullSafe(json);
    CFReleaseNullSafe(timestamps);

    return status;
}

static char const* _audioFormatDescription(AirPlayAudioFormat format)
{
    char const* formatDescription = "Unknown";

    switch (format) {
    case kAirPlayAudioFormat_Invalid:
        formatDescription = "";
        break;
    case kAirPlayAudioFormat_ALAC_44KHz_16Bit_Stereo:
        formatDescription = "ALAC, 16 bit at 44.1KHz";
        break;
    case kAirPlayAudioFormat_ALAC_44KHz_24Bit_Stereo:
        formatDescription = "ALAC, 24 bit at 44.1KHz";
        break;
    case kAirPlayAudioFormat_ALAC_48KHz_16Bit_Stereo:
        formatDescription = "ALAC, 16 bit at 48KHz";
        break;
    case kAirPlayAudioFormat_ALAC_48KHz_24Bit_Stereo:
        formatDescription = "ALAC, 24 bit at 48KHz";
        break;
    case kAirPlayAudioFormat_AAC_LC_44KHz_Stereo:
        formatDescription = "AAC-LC, 44.1KHz";
        break;
    case kAirPlayAudioFormat_AAC_LC_48KHz_Stereo:
        formatDescription = "AAC-LC, 48KHz";
        break;
    }

    return formatDescription;
}
//===========================================================================================================================
//    AirPlayReceiverMetricsProcess
//===========================================================================================================================

#define GetHeaderValue(req, name, outVal, outValLen) \
    HTTPGetHeaderField((req)->header.buf, (req)->header.len, name, NULL, NULL, outVal, outValLen, NULL)

HTTPStatus AirPlayReceiverMetricsProcess(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest)
{
    HTTPStatus status;
    CFDictionaryRef metrics = NULL;
    const char* src;
    size_t len;

    AirPlayReceiverSessionRef session = AirPlaySessionManagerCopyMasterSession(inCnx->server->sessionMgr);
    if (session != NULL) {
        metrics = (CFDictionaryRef)AirPlayReceiverSessionCopyProperty(session, kCFObjectFlagDirect, CFSTR(kAirPlayProperty_Metrics), NULL, NULL);
    }

    src = NULL;
    len = 0;
    GetHeaderValue(inRequest, kHTTPHeader_ContentType, &src, &len);

    if (metrics && strnicmp_prefix(src, len, kMIMEType_JSON) == 0) {
        status = AirPlayReceiverMetricsSendJSON(inCnx, inRequest, metrics);

    } else {
        status = AirPlayReceiverSendRequestPlistResponse(inCnx->httpCnx, inRequest, metrics, NULL);
    }

    CFReleaseNullSafe(metrics);
    CFReleaseNullSafe(session);
    return (status);
}

//===========================================================================================================================
//    _requestSendHTML
//===========================================================================================================================

static HTTPStatus _requestSendHTML(HTTPConnectionRef inCnx, HTTPMessageRef inRequest, const char* str, OSStatus* outErr)
{
    return _requestSendWithContentType(inCnx, inRequest, kMIMEType_TextHTML, str, outErr);
}

#endif // AIRPLAY_METRICS
