#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#if defined(COMMS_API_V3)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include "notify_avs.h"
#include "alexa_comms_client.h"
#include "wm_db.h"

#define ALEXA_DEF_HOST_FE ("alexa.fe.gateway.devices.a2z.com")
#define ALEXA_DEF_HOST_EU ("alexa.eu.gateway.devices.a2z.com")
#define ALEXA_DEF_HOST_NA ("alexa.na.gateway.devices.a2z.com")

#define ALEXA_ENDPOINT ("alexa_endpoint")

#define COMMS_DCM_STORAGE "/tmp/CommsDcmStorage"

static SipUserAgentInterfaceCreateParameters g_sua_info = {};
static char *g_deviceTypeId = NULL; // device type id(Amazon ID)
static const char *g_region = NULL;

#ifdef COMMS_APIV2
static SipUserAgentInterfaceCreateParametersV2 g_params_v2 = {};
#endif

static SipUserAgentState g_cur_sua_state = UNREGISTERED;
static int login_trigger_flag = 0;

static void getSipUserAgentStateContext(SOCKET_CONTEXT *msg_socket)
{
    SipUserAgentStateContext sua_state_cxt = SipUserAgentInterface_getSipUserAgentStateContext();
    if (sua_state_cxt.payload) {
        wiimu_log(1, 0, 0, 0, (char *)"TBD:%s: payload = %s, interface = %s,name = %s\n",
                  __FUNCTION__, sua_state_cxt.payload, sua_state_cxt.interface, sua_state_cxt.name);

        wiimu_log(1, 0, 0, 0, (char *)"CHERYL DEBUG, 2. got COMMS CXT\n");
        notify_avs_sua_get_state_cxt(msg_socket, sua_state_cxt.payload);
    } else {
        notify_avs_sua_get_state_cxt(msg_socket, "failed");
    }
}

static void onSendEvent(const char *interface, const char *name, const char *payload)
{
    wiimu_log(1, 0, 0, 0, (char *)"TBD:%s: interface = %s,name = %s\n", __FUNCTION__, interface,
              name);
    if (name && payload) {
        notify_avs_sua_send_events(name, payload);
    }
}
#ifdef COMMS_APIV2
static void onAcquireChannelFocus(void)
{
    wiimu_log(1, 0, 0, 0, (char *)"TBD:%s\n", __FUNCTION__);
    notify_avs_sua_channel_focus(1);
}

static void onReleaseChannelFocus(void)
{
    wiimu_log(1, 0, 0, 0, (char *)"TBD:%s\n", __FUNCTION__);
    notify_avs_sua_channel_focus(0);
}

static void onStateUpdated(const SipUserAgentState previousSipUserAgentState,
                           const SipUserAgentState currentSipUserAgentState,
                           const char *inboundRingtoneUrl)
{
    wiimu_log(1, 0, 0, 0,
              (char *)"TBD:%s: previousSipUserAgentState = %d,currentSipUserAgentState = %d\n",
              __FUNCTION__, previousSipUserAgentState, currentSipUserAgentState);
    if (previousSipUserAgentState || currentSipUserAgentState || inboundRingtoneUrl) {
        notify_avs_sua_state_updated(previousSipUserAgentState, currentSipUserAgentState,
                                     inboundRingtoneUrl);
    }
    g_cur_sua_state = currentSipUserAgentState;
    getSipUserAgentStateContext();
}

#elif defined COMMS_API_V3
static void onAcquireChannelFocus(const CallType callType)
{
    wiimu_log(1, 0, 0, 0, (char *)"TBD:%s\n", __FUNCTION__);
    notify_avs_sua_channel_focus(1);
}

static void onReleaseChannelFocus(const CallType callType)
{
    wiimu_log(1, 0, 0, 0, (char *)"TBD:%s\n", __FUNCTION__);
    notify_avs_sua_channel_focus(0);
}

static void onStateUpdated(const SipUserAgentStateInfo stateInfo)
{
    wiimu_log(1, 0, 0, 0,
              (char *)"TBD:%s: previousSipUserAgentState = %d,currentSipUserAgentState = %d,"
                      "inboundRingtoneUrl :%s, outboundRingbackUrl:%s.\n",
              __FUNCTION__, stateInfo.previousSipUserAgentState, stateInfo.currentSipUserAgentState,
              stateInfo.inboundRingtoneUrl, stateInfo.outboundRingbackUrl);
    if (stateInfo.previousSipUserAgentState || stateInfo.currentSipUserAgentState) {
        if (stateInfo.currentSipUserAgentState == OUTBOUND_LOCAL_RINGING)
            notify_avs_sua_state_updated(stateInfo.previousSipUserAgentState,
                                         stateInfo.currentSipUserAgentState,
                                         stateInfo.outboundRingbackUrl);
        else
            notify_avs_sua_state_updated(stateInfo.previousSipUserAgentState,
                                         stateInfo.currentSipUserAgentState,
                                         stateInfo.inboundRingtoneUrl);
    }
    g_cur_sua_state = stateInfo.currentSipUserAgentState;
}

#endif

static void myOnConsoleLogWrite(Level logLevel, const char *logMessage)
{
    printf("[COMMS DEBUG][LEVEL = %d] %s\n", (int)logLevel, logMessage);
}

#ifdef COMMS_APIV2
bool createSipUserAgentV2(SipUserAgentInterfaceCreateParameters myV1Params)
{
    bool ret = false;

    memset(&g_params_v2, 0, sizeof(SipUserAgentInterfaceCreateParametersV2));

    g_params_v2.audioDevices = myV1Params.audioDevices;
    g_params_v2.audioQualityParameters = myV1Params.audioQualityParameters;
    g_params_v2.sipUserAgentEventObserverInterface = myV1Params.sipUserAgentEventObserverInterface;
    g_params_v2.sipUserAgentStateObserverInterface = myV1Params.sipUserAgentStateObserverInterface;
    g_params_v2.sipUserAgentFocusManagerObserverInterface =
        myV1Params.sipUserAgentFocusManagerObserverInterface;

    // Version 2 default settings, modify as necessary
    g_params_v2.tlsCertificateFilePath = nullptr; //"/etc/ssl/certs";
    g_params_v2.metricsConfigParameters.certificatesDirectoryPath = COMMS_CERTS_DIR_PATH;
    g_params_v2.metricsConfigParameters.deviceTypeId = g_deviceTypeId;
    g_params_v2.metricsConfigParameters.hostAddress = IOT_HOST_URL;

    g_params_v2.logObserverInterface.onLogWrite = myOnConsoleLogWrite;
    //  g_params_v2.sipUserAgentNativeCallObserverInterface.onNativeCall = nullptr;
    //  g_params_v2.sipUserAgentNativeCallObserverInterface.onNativeCallHangup = nullptr;
    //  g_params_v2.sipUserAgentNativeCallObserverInterface.onNativeCallAccept = nullptr;

    ret = SipUserAgentInterface_create_v2(g_params_v2);
    wiimu_log(1, 0, 0, 0, (char *)"!!!!!!! [%s:%d] after SipUserAgentInterface_create_v2 ret(%d)\n",
              __FUNCTION__, __LINE__, ret);

    return ret;
}
#endif

static bool init_SipUserAgentInterfaceInfo()
{

    // these pj settings only valiable when mqtt disablede by remove /etc/ssl/cert/loT/*
    memset(&g_sua_info, 0, sizeof(SipUserAgentInterfaceCreateParameters));
    g_sua_info.audioDevices.outputDevice.driverName = (char *)"ALSA";
    g_sua_info.audioDevices.outputDevice.deviceName = (char *)"default";

    g_sua_info.audioDevices.inputDevice.driverName = (char *)"ALSA";
    g_sua_info.audioDevices.inputDevice.deviceName = (char *)"input";

#ifndef COMMS_API_V3
    g_sua_info.audioDevices.outputDevice.audioGain = 3.0;
    g_sua_info.audioDevices.outputDevice.latencyInMilliseconds = 100;
    g_sua_info.audioDevices.inputDevice.audioGain = 3.0;
    g_sua_info.audioDevices.inputDevice.latencyInMilliseconds = 10;
    g_sua_info.audioQualityParameters.echoCancellationTailLengthInMilliseconds = 200;
    g_sua_info.audioQualityParameters.opusBitrate = 48000;
#endif
    g_sua_info.sipUserAgentEventObserverInterface.onSendEvent = onSendEvent;
    g_sua_info.sipUserAgentFocusManagerObserverInterface.onAcquireCommunicationsChannelFocus =
        onAcquireChannelFocus;
    g_sua_info.sipUserAgentFocusManagerObserverInterface.onReleaseCommunicationsChannelFocus =
        onReleaseChannelFocus;
    g_sua_info.sipUserAgentStateObserverInterface.onStateUpdated = onStateUpdated;

#ifdef COMMS_API_V3
    g_sua_info.tlsCertificateFilePath = nullptr;
    g_sua_info.metricsConfigParameters.certificatesDirectoryPath = nullptr;
    g_sua_info.metricsConfigParameters.deviceTypeId = nullptr;
    g_sua_info.metricsConfigParameters.hostAddress = nullptr;
    g_sua_info.dcmConfigParameters.deviceTypeId = g_deviceTypeId;
    g_sua_info.dcmConfigParameters.region = g_region;
    g_sua_info.dcmConfigParameters.storagePath = COMMS_DCM_STORAGE;
    g_sua_info.logObserverInterface.onLogWrite = myOnConsoleLogWrite;
#endif

#if defined(COMMS_API_V3)
    AlexaCommsErrorCode ret = AC_SUCCESS;
    do {
        ret = SipUserAgentInterface_create(g_sua_info);
        if (ret != AC_SUCCESS && ret != AC_ALREADY_INITIALIZED) {
            wiimu_log(1, 0, 0, 0, (char *)"SipUserAgentInterface_create failed! (err=%d)...\n",
                      ret);
            SipUserAgentInterface_delete();
        } else {
            break;
        }
    } while (1);

#else
    bool ret = false;
    do {
        ret = createSipUserAgentV2(g_sua_info);
        if (!ret) {
            wiimu_log(1, 0, 0, 0, (char *)"SipUserAgentInterface_create failed! retry...\n");
            SipUserAgentInterface_delete();
        }
    } while (!ret);
#endif

    wiimu_log(1, 0, 0, 0, (char *)"SipUserAgentInterface_create succeed! (ret=%d)...\n", ret);
    return ret;
}

static void handle_sua_net_conn(bool net_conn)
{
    static bool pre_net_conn = false;
    bool sua_isInitialized = SipUserAgentInterface_isInitialized();

    wiimu_log(1, 0, 0, 0, (char *)"CHERYL DEBUG, %s, net_conn = %d\n", __FUNCTION__, net_conn);

    if (sua_isInitialized) {
        if (pre_net_conn ^ net_conn) {
#if !defined(COMMS_API_V3)
            SipUserAgentInterface_handleNetworkConnectivityEvent(net_conn);
#else
            SipUserAgentInterface_handleNetworkConnectivityEvent(net_conn, nullptr);
#endif
        }
    } else if (net_conn) {
        SocketClientReadWriteMsg("/tmp/AMAZON_ASRTTS", (char *)"comms_check_alexa_conn_stat",
                                 strlen("comms_check_alexa_conn_stat"), NULL, 0, 0);
    }
    pre_net_conn = net_conn;
}

static void handle_sua_avs_conn(bool avs_conn)
{
    static bool pre_avs_conn = false;
    bool sua_isInitialized = SipUserAgentInterface_isInitialized();

    wiimu_log(1, 0, 0, 0, (char *)"CHERYL DEBUG, %s, avs_conn = %d\n", __FUNCTION__, avs_conn);

    // if avs connected change caused by login or logout, do not send event.
    if (login_trigger_flag) {
        login_trigger_flag = 0;
        pre_avs_conn = avs_conn;
        return;
    }

    if (sua_isInitialized) {
        if (pre_avs_conn ^ avs_conn)
            SipUserAgentInterface_handleAvsConnectivityEvent(avs_conn);
    } else if (avs_conn) {
        init_SipUserAgentInterfaceInfo();
    }

    pre_avs_conn = avs_conn;
}

static void handle_sua_login()
{
    wiimu_log(1, 0, 0, 0, (char *)"CHERYL DEBUG, %s\n", __FUNCTION__);
    login_trigger_flag = 1;
    init_SipUserAgentInterfaceInfo();
}

static void handle_sua_logout()
{
    bool sua_isInitialized = SipUserAgentInterface_isInitialized();

    wiimu_log(1, 0, 0, 0, (char *)"CHERYL DEBUG, %s sua_isInitialized = %d\n", __func__,
              sua_isInitialized);
    if (sua_isInitialized)
        SipUserAgentInterface_delete();
}

static void handle_sua_mute_other(bool mute_other)
{
    if (g_cur_sua_state != ACTIVE)
        return;
    if (mute_other) {
        SipUserAgentInterface_muteOther();
    } else {
        SipUserAgentInterface_unmuteOther();
    }
}

static void handle_sua_mute_self(bool mute_self)
{
    if (g_cur_sua_state != ACTIVE)
        return;
    if (mute_self) {
        SipUserAgentInterface_muteSelf();
    } else {
        SipUserAgentInterface_unmuteSelf();
    }
}

static void *alexa_comms_proc(void *)
{
    int recv_num = 0;
    char *recv_buf = (char *)malloc(65536);
    int ret = 0;

    SOCKET_CONTEXT msg_socket;
    msg_socket.path = (char *)SOCKET_ALEXA_COMMS;
    msg_socket.blocking = 1;

    if (!recv_buf) {
        fprintf(stderr, "%s malloc failed\n", __FUNCTION__);
        exit(-1);
    }

RE_INIT:

    ret = UnixSocketServer_Init(&msg_socket);
    if (ret <= 0) {
        wiimu_log(1, 0, 0, 0, (char *)"ALEXA_COMMS UnixSocketServer_Init failed\n");
        sleep(5);
        goto RE_INIT;
    }

    SocketClientReadWriteMsg(SOCKET_ALEXA_ASRTTS, (char *)"comms_check_network_conn_stat",
                             strlen("comms_check_network_conn_stat"), NULL, 0, 0);
    while (1) {
        ret = UnixSocketServer_accept(&msg_socket);
        if (ret <= 0) {
            wiimu_log(1, 0, 0, 0, (char *)"ALEXA_COMMS UnixSocketServer_accept fail\n");
            UnixSocketServer_deInit(&msg_socket);
            sleep(5);
            goto RE_INIT;
        } else {
            memset(recv_buf, 0x0, 65536);
            recv_num = UnixSocketServer_read(&msg_socket, recv_buf, 65536);
            if (recv_num <= 0) {
                wiimu_log(1, 0, 0, 0, (char *)"ALEXA_COMMS recv failed\r\n");
                if (recv_num < 0) {
                    UnixSocketServer_close(&msg_socket);
                    UnixSocketServer_deInit(&msg_socket);
                    sleep(5);
                    goto RE_INIT;
                }
            } else {
                recv_buf[recv_num] = '\0';
                wiimu_log(1, 0, 0, 0, (char *)"ALEXA_COMMS recv_buf %.100s ++++++\n", recv_buf);
                if (strncmp(recv_buf, "handle_call_directive:", strlen("handle_call_directive:")) ==
                    0) {
                    char *name = (char *)calloc(1, recv_num);
                    char *payload = NULL;
                    char *name_str = NULL;
                    char *name_end = NULL;
                    if (name) {
                        name_str = strstr(recv_buf, "NAME=") + strlen("NAME=");
                        name_end = strstr(recv_buf, "PAYLOAD=");
                        strncpy(name, name_str, name_end - name_str);
                        payload = strstr(recv_buf, "PAYLOAD=") + strlen("PAYLOAD=");
                        if (payload) {
                            ret = SipUserAgentInterface_handleDirective((const char *)name,
                                                                        (const char *)payload);
#ifdef COMMS_APIV2
                            if (!ret) {
                                wiimu_log(1, 0, 0, 0,
                                          (char *)"ALEXA_COMMS ERROR!! "
                                                  "SipUserAgentInterface_handleDirective can not "
                                                  "handle this directive!\n");
                            }
#elif defined COMMS_API_V3
                            if (AC_SUCCESS == ret) {
                                wiimu_log(1, 0, 0, 0,
                                          (char *)"COMMS lib handle this Directive successful\n");
                            } else {
                                wiimu_log(1, 0, 0, 0,
                                          (char *)"ALEXA_COMMS ERROR!! "
                                                  "SipUserAgentInterface_handleDirective handle "
                                                  "this directive unsuccess errno=%d!\n",
                                          ret);
                            }
#endif
                        }
                        free(name);
                    }
                } else if (strncmp(recv_buf, "report_network_conn_state:",
                                   strlen("report_network_conn_state:")) == 0) {
                    bool connected = (bool)atoi(recv_buf + strlen("report_network_conn_state:"));
                    handle_sua_net_conn(connected);
                } else if (strncmp(recv_buf, "report_alexa_conn_state:",
                                   strlen("report_alexa_conn_state:")) == 0) {
                    bool alexa_conn = (bool)atoi(recv_buf + strlen("report_alexa_conn_state:"));
                    handle_sua_avs_conn(alexa_conn);
                } else if (strncmp(recv_buf, "report_alexa_login", strlen("report_alexa_login")) ==
                           0) {
                    handle_sua_login();
                } else if (strncmp(recv_buf, "report_alexa_logout",
                                   strlen("report_alexa_logout")) == 0) {
                    handle_sua_logout();
                } else if (strncmp(recv_buf, "call_mute_ctrl:", strlen("call_mute_ctrl:")) == 0) {
                    bool mute_other = (bool)atoi(recv_buf + strlen("call_mute_ctrl:"));
                    handle_sua_mute_other(mute_other);
                } else if (strncmp(recv_buf, "call_mute_mic:", strlen("call_mute_mic:")) == 0) {
                    bool mute_self = (bool)atoi(recv_buf + strlen("call_mute_mic:"));
                    handle_sua_mute_self(mute_self);
                }
#if defined(COMMS_API_V3)
                else if (strncmp(recv_buf, "update_status_context",
                                 strlen("update_status_context")) == 0) {
                    getSipUserAgentStateContext(&msg_socket);
                }
#endif
                else {
                    wiimu_log(1, 0, 0, 0, (char *)"unknown message\n");
                }
                wiimu_log(1, 0, 0, 0, (char *)"ALEXA_COMMS recv_buf------\n");
            }

            UnixSocketServer_close(&msg_socket);
        }
    }

    return NULL;
}

static void sigroutine(int signum)
{
    fprintf(stderr, "******** avs_comms got signal %d  ***********\n", signum);
    switch (signum) {
    case SIGINT:
    case SIGABRT:
    case SIGTERM:
    case SIGKILL:
    case SIGQUIT:
    case SIGBUS:
    case SIGSYS:
    case SIGILL:
        exit(-1);
    }
    sleep(1);
}

static void get_region(void)
{
    char buf[64];
    if (wm_db_get((char *)ALEXA_ENDPOINT, buf) < 0) {
        g_region = "NA";
    } else {
        wiimu_log(1, 0, 0, 0, (char *)"endpoint is %s\n", buf);
        if (strcmp(buf, ALEXA_DEF_HOST_FE) == 0)
            g_region = "FE";
        else if (strcmp(buf, ALEXA_DEF_HOST_EU) == 0)
            g_region = "EU";
        else
            g_region = "NA";
    }
}

int main(int argc, char **argv)
{
    int sig_count = 0;
    for (sig_count = 0; sig_count < 32; sig_count++) {
        if (sig_count != SIGSEGV && sig_count != SIGILL) {
            signal(sig_count, sigroutine);
        }
    }
    wiimu_log(1, 0, 0, 0, (char *)"avs_comms enter, argv[1] = %s\n", argv[1]);
    if (argv[1])
        g_deviceTypeId = strdup(argv[1]);
    else
        wiimu_log(1, 0, 0, 0, (char *)"ERROR, DTID is NULL!\n");
    if (mkdir(COMMS_DCM_STORAGE, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0)
        wiimu_log(1, 0, 0, 0, (char *)"ERROR, mkdir %s error: %s", COMMS_DCM_STORAGE,
                  strerror(errno));
    get_region();
    alexa_comms_proc(NULL);
    wiimu_log(1, 0, 0, 0, (char *)"avs_comms finish\n");
    return 0;
}
