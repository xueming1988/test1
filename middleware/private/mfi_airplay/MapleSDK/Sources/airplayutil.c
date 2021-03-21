/*
	Copyright (C) 2010-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "AsyncConnection.h"
#include "CFLiteBinaryPlist.h"
#include "CFUtils.h"
#include "CommandLineUtils.h"
#include "CommonServices.h" // Include early to work around problematic system headers on some systems.
#include "DebugIPCUtils.h"
#include "DebugServices.h"
#include "HTTPClient.h"
#include "HTTPMessage.h"
#include "HTTPUtils.h"
#include "MathUtils.h"
#include "MiscUtils.h"
#include "NetUtils.h"
#include "PrintFUtils.h"
#include "RandomNumberUtils.h"
#include "StringUtils.h"
#include "TickUtils.h"

#include <stdio.h>

#include <dns_sd.h>
#include <errno.h>

#include "AirPlayCommon.h"
#include "AirPlayVersion.h"

#if (TARGET_OS_POSIX)
#include <fcntl.h>
#include <pwd.h>
#include <sys/stat.h>
#if (TARGET_OS_ANDROID)
#include <linux/sysctl.h>
#else
#include <sys/sysctl.h>
#endif
#include <sys/uio.h>
#include <sys/un.h>
#endif

#include "HIDUtils.h"

#include "MFiSAP.h"

//===========================================================================================================================
//	Prototypes
//===========================================================================================================================

static void cmd_control(void);
static void cmd_diagnostic_logging(void);
static void cmd_http(void);
static void cmd_kill_all(void);
static void cmd_logging(void);
static void cmd_mfi(void);
static void cmd_moved_to_cuutil(void);

static void cmd_show(void);
static void cmd_test(void);

//===========================================================================================================================
//	Globals
//===========================================================================================================================

static int gVerbose = 0;
static int gWait = 0;

#if 0
#pragma mark == Command Options ==
#endif

//===========================================================================================================================
//	Command Line
//===========================================================================================================================

// Diagnostic logging

static int gDiagnosticLoggingIncludeBluetooth = false;
static int gDiagnosticLoggingIncludeMDNSResponder = false;

static CLIOption kDiagnosticLoggingOptions[] = {
    CLI_OPTION_BOOLEAN(0, "include-bluetooth", &gDiagnosticLoggingIncludeBluetooth, "Include Bluetooth logging.", NULL),
    CLI_OPTION_BOOLEAN(0, "include-mDNSResponder", &gDiagnosticLoggingIncludeMDNSResponder, "Include mDNSResponder logging.", NULL),
    CLI_OPTION_END()
};

// HTTP

static const char* gHTTPAddress = NULL;
static int gHTTPDecode = false;
static const char* gHTTPMethod = "GET";
static int gHTTPRepeatMs = -1;
static const char* gHTTPURL = NULL;

static CLIOption kHTTPOptions[] = {
    CLI_OPTION_STRING('a', "address", &gHTTPAddress, "address", "Destination DNS name or IP address of accessory.", NULL),
    CLI_OPTION_BOOLEAN('d', "decode", &gHTTPDecode, "Attempt to decode based on content type.", NULL),
    CLI_OPTION_STRING('m', "method", &gHTTPMethod, "method", "HTTP method (e.g. GET, POST, etc.). Defaults to GET.", NULL),
    CLI_OPTION_INTEGER('r', "repeat", &gHTTPRepeatMs, "ms", "Delay between repeats. If not specified, it doesn't repeat.", NULL),
    CLI_OPTION_STRING('u', "URL", &gHTTPURL, "URL", "URL to get. Maybe be relative if an address is specified", NULL),
    CLI_OPTION_END()
};

// Show

static const char* gShowCommand = NULL;

static CLIOption kShowOptions[] = {
    CLI_OPTION_STRING('c', "command", &gShowCommand, "command", "Custom show command to use.", NULL),
    CLI_OPTION_END()
};

// Test

static const char* gTestAddress = NULL;
static int gTestMinSize = 1000;
static int gTestMaxSize = 40000;
static int gTestFPS = 60;
static int gTestMinPause = 100;
static int gTestMaxPause = 3000;
static int gTestMinPauseDelay = 1000;
static int gTestMaxPauseDelay = 5000;
static int gTestMinMeasureBurst = 6;
static int gTestMaxMeasureBurst = 30;
static int gTestReportPeriodMs = -1;
static int gTestTimeoutSecs = -1;

static CLIOption kTestOptions[] = {
    CLI_OPTION_STRING_EX('a', "address", &gTestAddress, "IP/DNS name", "Network address of device to test.", kCLIOptionFlags_Required, NULL),
    CLI_OPTION_INTEGER('f', "fps", &gTestFPS, NULL, "Average frames per second.", NULL),
    CLI_OPTION_INTEGER('s', "min-size", &gTestMinSize, "bytes", "Min frame size to send.", NULL),
    CLI_OPTION_INTEGER('S', "max-size", &gTestMaxSize, "bytes", "Max frame size to send.", NULL),
    CLI_OPTION_INTEGER('p', "min-pause", &gTestMinPause, "ms", "Min time to pause.", NULL),
    CLI_OPTION_INTEGER('P', "max-pause", &gTestMaxPause, "ms", "Max time to pause.", NULL),
    CLI_OPTION_INTEGER('d', "min-pause-delay", &gTestMinPauseDelay, "ms", "Min delay between pauses.", NULL),
    CLI_OPTION_INTEGER('D', "max-pause-delay", &gTestMaxPauseDelay, "ms", "Max delay between pauses.", NULL),
    CLI_OPTION_INTEGER('b', "min-measure-burst", &gTestMinMeasureBurst, "packets", "Min packets before measuring a burst.", NULL),
    CLI_OPTION_INTEGER('B', "max-measure-burst", &gTestMaxMeasureBurst, "packets", "Max packets to measure in a burst.", NULL),
    CLI_OPTION_INTEGER('r', "report-period", &gTestReportPeriodMs, "ms", "How often to report progress.", NULL),
    CLI_OPTION_INTEGER('t', "timeout", &gTestTimeoutSecs, "seconds", "How many seconds to run the test.", NULL),
    CLI_OPTION_END()
};

#if 0
#pragma mark == Command Table ==
#endif

//===========================================================================================================================
//	Command Table
//===========================================================================================================================

#define kMovedToCUutil "Not available in airplayutil anymore. Please use cuutil."

static CLIOption kGlobalOptions[] = {
    CLI_OPTION_VERSION(kAirPlayMarketingVersionStr, kAirPlaySourceVersionStr),
    CLI_OPTION_HELP(),
    CLI_OPTION_BOOLEAN('v', "verbose", &gVerbose, "Increase logging output of commands.", NULL),
    CLI_OPTION_INTEGER(0, "wait", &gWait, "seconds", "Seconds to wait before exiting.", NULL),

    CLI_COMMAND("control", cmd_control, NULL, "Control AirPlay internal state.", NULL),
    CLI_COMMAND_EX("diagnostic-logging", cmd_diagnostic_logging, kDiagnosticLoggingOptions, kCLIOptionFlags_NotCommon, "Enable/disable diagnostic logging.", NULL),
    CLI_COMMAND_EX("error", cmd_moved_to_cuutil, NULL, kCLIOptionFlags_NotCommon, kMovedToCUutil, NULL),
    CLI_COMMAND_HELP(),
    CLI_COMMAND("http", cmd_http, kHTTPOptions, "Download via HTTP.", NULL),
    CLI_COMMAND("ka", cmd_kill_all, NULL, "Does killall of all AirPlay-related processes.", NULL),
    CLI_COMMAND("logging", cmd_logging, NULL, "Show or change the logging configuration.", NULL),
    CLI_COMMAND("mfi", cmd_mfi, NULL, "Tests the MFi auth IC.", NULL),
    CLI_COMMAND("show", cmd_show, kShowOptions, "Shows state.", NULL),
    CLI_COMMAND_EX("test", cmd_test, kTestOptions, kCLIOptionFlags_NotCommon, "Tests network performance.", NULL),
    CLI_COMMAND_VERSION(kAirPlayMarketingVersionStr, kAirPlaySourceVersionStr),

    CLI_OPTION_END()
};

//===========================================================================================================================
//	main
//===========================================================================================================================

int main(int argc, const char** argv)
{
    {
        gProgramLongName = "AirPlay Command Line Utility";
        CLIInit(argc, argv);
        CLIParse(kGlobalOptions, kCLIFlags_None);
    }
    if (gWait < 0)
        for (;;)
            sleep(30);
    if (gWait > 0)
        sleep(gWait);
    return (gExitCode);
}

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	cmd_control
//===========================================================================================================================

static void cmd_control(void)
{
    OSStatus err;
    const char* arg;

    if (gArgI >= gArgC)
        ErrQuit(1, "error: no control command specified\n");
    arg = gArgV[gArgI++];

    err = DebugIPC_PerformF(NULL, NULL,
        "{"
        "%kO=%O"
        "%kO=%s"
        "}",
        kDebugIPCKey_Command, kDebugIPCOpCode_Control,
        kDebugIPCKey_Value, arg);
    if (err)
        ErrQuit(EINVAL, "error: %#m\n", err);
}

//===========================================================================================================================
//	cmd_diagnostic_logging
//===========================================================================================================================

static OSStatus _diagnostic_logging_mDNSResponder(Boolean inShouldEnable)
{
    OSStatus err = kNoErr;
    const char* cmd = NULL;

    cmd = "/usr/bin/killall -SIGTSTP mDNSResponder > /dev/null";
    require_noerr_action(err, exit, ErrQuit(1, "'%s' failed.\n", cmd));

    if (inShouldEnable) {
        cmd = "syslog -c mDNSResponder -i > /dev/null";
        err = systemf(NULL, cmd);
        require_noerr_action(err, exit, ErrQuit(1, "'%s' failed.\n", cmd));

        cmd = "/usr/bin/killall -USR1 mDNSResponder > /dev/null";
        require_noerr_action(err, exit, ErrQuit(1, "'%s' failed.\n", cmd));
    }

exit:
    return (err);
}

static OSStatus _diagnostic_logging_AirPlay(Boolean inShouldEnable)
{
    OSStatus err = kNoErr;
    CFMutableStringRef loggingCmd = NULL;
    const char* loggingCmdPtr = NULL;

    loggingCmd = CFStringCreateMutable(kCFAllocatorDefault, 0);
    require_action(loggingCmd, exit, err = kNoMemoryErr);

    CFStringAppendF(loggingCmd, "airplayutil logging ");

    CFStringAppendF(loggingCmd, "+%s:level=%s,", "AirPlayClientCore", inShouldEnable ? "verbose" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "AirPlayEndpointCore", inShouldEnable ? "verbose" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "AirPlayRouting", inShouldEnable ? "verbose" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "AirPlayBTLE", inShouldEnable ? "verbose" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "AirPlayHALPluginCore", inShouldEnable ? "verbose" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "AirPlayHIDCore", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "AirPlayPairing", inShouldEnable ? "verbose" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "AirPlayEndpointManagerMeta", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "AirPlayHALPluginGuts", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "APBrowser", inShouldEnable ? "verbose" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "APBrowserBTLEManager", inShouldEnable ? "verbose" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "APBrowserBTLEQueryManager", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "APBrowserController", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "APEndpoint", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "APEndpointCollection", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "APEndpointDescriptionAirPlay", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "APEndpointManager", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "APEndpointPlaybackSessionAirPlay", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "APEndpointPlaybackSessionMC", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "APEndpointStreamAudio", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "APEndpointStreamPlayback", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "APEndpointStreamScreen", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "APPairingClientHomeKit", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "APPairingClientLegacy", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "APSenderSessionSDP", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "APSenderSessionAirPlay", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "APTransport", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "APTransportTrafficRegistrar", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "APTransportWifiManagerClient", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "AsyncConnection", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "BonjourBrowser", inShouldEnable ? "verbose" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s,", "HTTPClientCore", inShouldEnable ? "info" : "notice");
    CFStringAppendF(loggingCmd, "%s:level=%s", "WiFiManagerCore", inShouldEnable ? "info" : "notice");

    CFStringAppendF(loggingCmd, " > /dev/null");

    loggingCmdPtr = CFStringGetCStringPtr(loggingCmd, kCFStringEncodingUTF8);
    err = systemf(NULL, loggingCmdPtr);
    require_noerr(err, exit);

exit:
    CFReleaseNullSafe(loggingCmd);
    return (err);
}

static void cmd_diagnostic_logging(void)
{
    OSStatus err = kNoErr;
    const char* param = NULL;
    Boolean shouldEnable = false;

    param = (gArgI < gArgC) ? gArgV[gArgI++] : "";

    if (strcmp(param, "enable") == 0)
        shouldEnable = true;
    else if (strcmp(param, "disable") == 0)
        shouldEnable = false;
    else {
        ErrQuit(1, "error: Bad command '%s'. Must be 'enable' or 'disable'.\n", param);
        goto exit;
    }

    err = _diagnostic_logging_AirPlay(shouldEnable);
    require_noerr(err, exit);
    if (gDiagnosticLoggingIncludeMDNSResponder || !shouldEnable) {
        err = _diagnostic_logging_mDNSResponder(shouldEnable);
        require_noerr(err, exit);
    }

    fprintf(stdout, "Diagnostic logging %s successfully.\n", shouldEnable ? "enabled" : "disabled");

exit:
    if (err) {
        ErrQuit(1, "error: %#m\n", err);
    }
}

//===========================================================================================================================
//	cmd_http
//===========================================================================================================================

static void cmd_http(void)
{
    OSStatus err;
    HTTPClientRef client = NULL;
    dispatch_queue_t queue;
    HTTPMessageRef msg = NULL;
    size_t n;
    const char* valuePtr;
    size_t valueLen;
    Boolean decoded;
    CFTypeRef obj;
    uint32_t count;
    CFAbsoluteTime t;

    if (!gHTTPAddress) {
        CLIHelpCommand("http");
        err = kNoErr;
        goto exit;
    }

    for (count = 1;; ++count) {
        err = HTTPClientCreate(&client);
        require_noerr(err, exit);

        queue = dispatch_queue_create("HTTP", 0);
        require_action(queue, exit, err = kUnknownErr);
        HTTPClientSetDispatchQueue(client, queue);
        dispatch_release(queue);

        err = HTTPClientSetDestination(client, gHTTPAddress, kAirPlayFixedPort_RTSPControl);
        require_noerr(err, exit);

        err = HTTPMessageCreate(&msg);
        require_noerr(err, exit);

        HTTPHeader_InitRequest(&msg->header, gHTTPMethod, gHTTPURL, "HTTP/1.1");
        HTTPHeader_AddFieldF(&msg->header, kHTTPHeader_CSeq, "1"); // Required by some servers.
        HTTPHeader_AddFieldF(&msg->header, kHTTPHeader_UserAgent, kAirPlayUserAgentStr);
        if (stricmp(gHTTPMethod, "GET") != 0)
            HTTPHeader_AddFieldF(&msg->header, kHTTPHeader_ContentLength, "0");
        t = CFAbsoluteTimeGetCurrent();
        err = HTTPClientSendMessageSync(client, msg);
        require_noerr(err, exit);
        t = CFAbsoluteTimeGetCurrent() - t;

        decoded = false;
        if (gHTTPDecode) {
            valuePtr = NULL;
            valueLen = 0;
            HTTPGetHeaderField(msg->header.buf, msg->header.len, kHTTPHeader_ContentType, NULL, NULL, &valuePtr, &valueLen, NULL);
            if (MIMETypeIsPlist(valuePtr, valueLen)) {
                obj = CFBinaryPlistV0CreateWithData(msg->bodyPtr, msg->bodyLen, NULL);
                if (obj) {
                    FPrintF(stdout, "%@", obj);
                    CFRelease(obj);
                    decoded = true;
                }
            }
        }
        if (!decoded) {
            n = fwrite(msg->bodyPtr, 1, msg->bodyLen, stdout);
            err = map_global_value_errno(n == msg->bodyLen, n);
            require_noerr(err, exit);
        }
        ForgetCF(&msg);
        HTTPClientForget(&client);

        if (gVerbose && (gHTTPRepeatMs >= 0))
            fprintf(stderr, "Cycle %u, %f ms\n", count, 1000 * t);

        if (gHTTPRepeatMs > 0)
            usleep(gHTTPRepeatMs * 1000);
        else if (gHTTPRepeatMs < 0)
            break;
    }

exit:
    CFReleaseNullSafe(msg);
    HTTPClientForget(&client);
    if (err)
        ErrQuit(1, "error: %#m\n", err);
}

//===========================================================================================================================
//	cmd_kill_all
//===========================================================================================================================

static void cmd_kill_all(void)
{
    const char* cmd;

#if (TARGET_OS_LINUX)
    cmd = "sudo /etc/init.d/airplay restart";
#else
    ErrQuit(1, "error: not supported on this platform\n");
    cmd = "";
#endif
    systemf("", cmd);
}

//===========================================================================================================================
//	cmd_logging
//===========================================================================================================================

static void cmd_logging(void)
{
    OSStatus err;

    err = DebugIPC_LogControl((gArgI < gArgC) ? gArgV[gArgI++] : NULL);
    if (err)
        ErrQuit(EINVAL, "error: %#m\n", err);
}

//===========================================================================================================================
//	cmd_mfi
//===========================================================================================================================

static void cmd_mfi(void)
{
    OSStatus err;
    uint64_t ticks;
    uint8_t* ptr;
    size_t len;
    uint8_t digest[20];

    ticks = UpTicks();
    err = MFiPlatform_Initialize();
    ticks = UpTicks() - ticks;
    require_noerr(err, exit);
    FPrintF(stderr, "Init time: %llu ms\n", UpTicksToMilliseconds(ticks));

    ticks = UpTicks();
    err = MFiPlatform_CopyCertificate(&ptr, &len);
    ticks = UpTicks() - ticks;
    require_noerr(err, exit);
    FPrintF(stderr, "Certificate (%llu ms):\n%.2H\n", UpTicksToMilliseconds(ticks), ptr, (int)len, (int)len);
    free(ptr);

    memset(digest, 0, sizeof(digest));
    ticks = UpTicks();
    err = MFiPlatform_CreateSignature(digest, sizeof(digest), &ptr, &len);
    ticks = UpTicks() - ticks;
    require_noerr(err, exit);
    FPrintF(stderr, "Signature (%llu ms):\n%.2H\n", UpTicksToMilliseconds(ticks), ptr, (int)len, (int)len);
    free(ptr);

exit:
    MFiPlatform_Finalize();
    if (err)
        ErrQuit(1, "error: %#m\n", err);
}

//===========================================================================================================================
//	cmd_moved_to_cuutil
//===========================================================================================================================

static void cmd_moved_to_cuutil(void)
{
    fprintf(stdout, "%s\n", kMovedToCUutil);
}

#if 0
#pragma mark -
#endif

#if 0
#pragma mark -
#endif

//===========================================================================================================================
//	cmd_show
//===========================================================================================================================

static void cmd_show(void)
{
    OSStatus err;
    const char* command;

    command = gShowCommand ? gShowCommand : "show";

    err = DebugIPC_PerformF(NULL, NULL,
        "{"
        "%kO=%s"
        "%kO=%s"
        "%kO=%i"
        "}",
        kDebugIPCKey_Command, command,
        kDebugIPCKey_Value, (gArgI < gArgC) ? gArgV[gArgI++] : NULL,
        CFSTR("verbose"), gVerbose);
    if (err)
        ErrQuit(EINVAL, "error: %#m\n", err);
}

//===========================================================================================================================
//	cmd_test
//===========================================================================================================================

static void cmd_test(void)
{
    OSStatus err;
    uint8_t* buf;
    AsyncConnectionFlags flags;
    SocketRef sock;
    NetSocketRef netSock;
    char url[256];
    uint64_t fpsDelayTicks, lastFrameTicks;
    uint64_t nowTicks, intervalTicks, lastProgressTicks, endTicks;
    size_t size;
    HTTPHeader header;
    iovec_t iov[2];
    int frameNum;
    uint64_t transactionTicks;
    size_t transactionBytes;
    double mbps, tcpMbps;
    EWMA_FP_Data mbpsAvg;
#if (defined(TCP_MEASURE_SND_BW))
    int option;
    struct tcp_measure_bw_burst burstInfo;
    struct tcp_info tcpInfo;
    socklen_t len;
#endif

    buf = NULL;
    if (gTestMinSize > gTestMaxSize)
        ErrQuit(1, "error: min-size must be >= max-size\n");
    if (gTestMaxSize <= 0)
        ErrQuit(1, "error: max-size must be >= 0\n");
    if (gTestFPS <= 0)
        ErrQuit(1, "error: fps must be >= 0\n");
    if (gTestMinPause > gTestMaxPause)
        ErrQuit(1, "error: min-pause must be >= max-pause\n");
    if (gTestMinPauseDelay > gTestMaxPauseDelay)
        ErrQuit(1, "error: min-pause-delay must be >= max-pause-delay\n");

    fprintf(stderr, "Network test to %s, %d-%d bytes, %d fps, %d-%d ms pauses, %d-%d ms pause delay\n",
        gTestAddress, gTestMinSize, gTestMaxSize, gTestFPS, gTestMinPause, gTestMaxPause,
        gTestMinPauseDelay, gTestMaxPauseDelay);

    fpsDelayTicks = UpTicksPerSecond() / gTestFPS;

    buf = (uint8_t*)calloc(1, (size_t)gTestMaxSize);
    require_action(buf, exit, err = kNoMemoryErr; ErrQuit(1, "error: no memory for %d max-size\n", gTestMaxSize));

    flags = kAsyncConnectionFlag_P2P;
    err = AsyncConnection_ConnectSync(gTestAddress, kAirPlayPort_MediaControl, flags, 0,
        kSocketBufferSize_DontSet, kSocketBufferSize_DontSet, NULL, NULL, &sock);
    require_noerr_action_quiet(err, exit, ErrQuit(1, "error: connected to %s failed: %#m\n", gTestAddress, err));

    err = NetSocket_CreateWithNative(&netSock, sock);
    require_noerr(err, exit);

// Setup measurements.

#if (defined(TCP_MEASURE_SND_BW))
    option = 1;
    err = setsockopt(netSock->nativeSock, IPPROTO_TCP, TCP_MEASURE_SND_BW, &option, (socklen_t)sizeof(option));
    if (err)
        FPrintF(stderr, "### TCP_MEASURE_SND_BW failed: %#m\n", err);

    burstInfo.min_burst_size = (uint32_t)gTestMinMeasureBurst;
    burstInfo.max_burst_size = (uint32_t)gTestMaxMeasureBurst;
    err = setsockopt(netSock->nativeSock, IPPROTO_TCP, TCP_MEASURE_BW_BURST, &burstInfo, (socklen_t)sizeof(burstInfo));
    err = map_socket_noerr_errno(nativeSock, err);
    if (err)
        FPrintF(stderr, "### TCP_MEASURE_BW_BURST failed: %#m\n", err);

    memset(&tcpInfo, 0, sizeof(tcpInfo));
#endif

    // Init stats.

    EWMA_FP_Init(&mbpsAvg, 0.1, kEWMAFlags_StartWithFirstValue);

    nowTicks = UpTicks();
    intervalTicks = (gTestReportPeriodMs > 0) ? MillisecondsToUpTicks(gTestReportPeriodMs) : 0;
    endTicks = (gTestTimeoutSecs >= 0) ? (nowTicks + SecondsToUpTicks(gTestTimeoutSecs)) : kUpTicksForever;

    lastFrameTicks = nowTicks;
    lastProgressTicks = nowTicks;

    // Main loop.

    for (frameNum = 1;; ++frameNum) {
        size = RandomRange(gTestMinSize, gTestMaxSize);
        snprintf(url, sizeof(url), "http://%s/test/%d", gTestAddress, (int)size);
        HTTPHeader_InitRequest(&header, "PUT", url, "HTTP/1.1");
        HTTPHeader_AddFieldF(&header, "Content-Length", "%zu", size);
        err = HTTPHeader_Commit(&header);
        require_noerr(err, exit);

        iov[0].iov_base = header.buf;
        iov[0].iov_len = header.len;
        iov[1].iov_base = (char*)buf;
        iov[1].iov_len = size;
        transactionBytes = header.len + size;
        transactionTicks = UpTicks();
        err = NetSocket_WriteV(netSock, iov, 2, -1);
        require_noerr_action(err, exit, ErrQuit(1, "error: write of %zu bytes failed: %#m\n", size, err));

        err = NetSocket_HTTPReadHeader(netSock, &header, -1);
        require_noerr_action(err, exit, ErrQuit(1, "error: read failed: %#m\n", err));
        require_action(IsHTTPStatusCode_Success(header.statusCode), exit,
            ErrQuit(1, "error: request failed: %d\n", header.statusCode));

        // Measure bandwidth.

        nowTicks = UpTicks();
        transactionTicks = nowTicks - transactionTicks;
        mbps = (8 * ((double)transactionBytes) / (((double)transactionTicks) / ((double)UpTicksPerSecond())) / 1000000);
        EWMA_FP_Update(&mbpsAvg, mbps);

#if (defined(TCP_MEASURE_SND_BW))
        len = (socklen_t)sizeof(tcpInfo);
        getsockopt(netSock->nativeSock, IPPROTO_TCP, TCP_INFO, &tcpInfo, &len);
        tcpMbps = ((double)tcpInfo.tcpi_snd_bw) / 1000000.0;
#else
        tcpMbps = 0;
#endif

        if (intervalTicks == 0) {
            fprintf(stderr, "%4d: Bytes: %6zu  Mbps: %.3f  Avg Mbps: %.3f  TCP Mbps: %0.3f\n",
                frameNum, size, mbps, EWMA_FP_Get(&mbpsAvg), tcpMbps);
        } else if ((nowTicks - lastProgressTicks) >= intervalTicks) {
            lastProgressTicks = nowTicks;
            fprintf(stderr, "Avg Mbps: %.3f  TCP Mbps: %.3f\n", EWMA_FP_Get(&mbpsAvg), tcpMbps);
        }
        if ((gTestTimeoutSecs >= 0) && (nowTicks >= endTicks)) {
            break;
        }

        lastFrameTicks += fpsDelayTicks;
        SleepUntilUpTicks(lastFrameTicks);
    }

exit:
    if (buf)
        free(buf);
    if (err)
        ErrQuit(1, "error: %#m\n", err);
}

#if 0
#pragma mark -
#endif
