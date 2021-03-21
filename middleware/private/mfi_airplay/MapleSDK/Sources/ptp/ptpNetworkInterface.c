/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpNetworkInterface.h"
#include "Common/ptpPlatform.h"
#include "CommonServices.h"
#include "NetUtils.h"
#include "ptpClockContext.h"
#include "ptpNetworkPort.h"
#include "ptpUtils.h"

#if !TARGET_OS_BRIDGECO
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#if (TARGET_OS_LINUX)
#if (TARGET_OS_ANDROID)
#include <ethtool.h>
#else
#include <linux/ethtool.h>
#endif
#include <linux/net_tstamp.h>
#include <linux/ptp_clock.h>
#include <linux/sockios.h>
#else
#include <ifaddrs.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#endif // TARGET_OS_LINUX

#undef ETHTOOL_GET_TS_INFO

// poll() timeout in milliseconds
#define POLL_TIMEOUT_MS 50

// PTP device name template
#define PTP_DEVICE "/dev/ptpXX"

// position of device id in PTP_DEVICE
#define PTP_DEVICE_IDX_OFFS 8

#define CLOCKFD 3
#define FD_TO_CLOCKID(fd) ((~(clockid_t)(fd) << 3) | CLOCKFD)

#else
#include "Networking/WebUtils/NtpClient.h"
#endif // !TARGET_OS_BRIDGECO

#include <assert.h>
#include <errno.h>
#include <string.h>

// 256b buffer for control metadata / timestamps
#define CTRL_BUFFER_SIZE 256

// 1k buffer for 'junk' data received via recvmsg() when retrieving TX timestamps
#define JUNK_BUFFER_SIZE 1024

struct NetIf {
    MacAddress _localAddress;
    int _eventSocket;
    int _generalSocket;
    int _cmdSocket;
    int _interfaceIndex;
    Boolean _txTimestampsSupported;

#if !TARGET_OS_BRIDGECO
    int _sd;
    int _phcFd;

#ifdef PTP_HW_CROSSTSTAMP
    Boolean _crossTimestampingSupported;
#endif
#endif
};

static Boolean ptpNetIfInitialize(NetIfRef netif, const char* netIfName);
#if !TARGET_OS_BRIDGECO
#if (defined(ETHTOOL_GET_TS_INFO))
static int ptpNetIfFindPtpClockIndex(const char* netIfName);
static Boolean ptpNetIfInitializePtpClock(NetIfRef netif, const char* netIfName);
#endif
#endif
#if (TARGET_OS_LINUX)
#if (defined(ETHTOOL_GET_TS_INFO))
static Boolean ptpNetIfCheckInterfaceTimestampingSupport(NetIfRef netif, const char* netIfName, struct ethtool_ts_info* tsInfo);
static Boolean ptpNetIfEnableHwTimestamps(NetIfRef netif, const char* netIfName, int ifindex, const struct ethtool_ts_info* tsInfo);
static Boolean ptpNetIfEnableSwTimestamps(NetIfRef netif, const char* netIfName, const struct ethtool_ts_info* tsInfo);
#endif
#endif
NetIfRef ptpNetIfCreate(void)
{
    NetIfRef netif = (NetIfRef)malloc(sizeof(struct NetIf));

    if (!netif)
        return NULL;

    return netif;
}

void ptpNetIfDestroy(NetIfRef netif)
{
    if (!netif)
        return;

    ptpNetIfDtor(netif);
    free(netif);
}

void ptpNetIfCtor(NetIfRef netif, const char* netIfName)
{
    assert(netif);

    netif->_eventSocket = -1;
    netif->_generalSocket = -1;
    netif->_cmdSocket = -1;
    netif->_interfaceIndex = -1;
    netif->_txTimestampsSupported = false;
#if !TARGET_OS_BRIDGECO
    netif->_sd = -1;
    netif->_phcFd = -1;
#ifdef PTP_HW_CROSSTSTAMP
    netif->_crossTimestampingSupported = false;
#endif
#endif

    ptpNetIfInitialize(netif, netIfName);
}

void ptpNetIfDtor(NetIfRef netif)
{
    if (netif == NULL) {
        return;
    }

    if (netif->_cmdSocket >= 0) {
        close_compat(netif->_cmdSocket);
        netif->_cmdSocket = -1;
    }

    if (netif->_eventSocket >= 0) {
        close_compat(netif->_eventSocket);
        netif->_eventSocket = -1;
    }

    if (netif->_generalSocket >= 0) {
        close_compat(netif->_generalSocket);
        netif->_generalSocket = -1;
    }

    netif->_txTimestampsSupported = false;
}

#if !TARGET_OS_BRIDGECO
#if (defined(ETHTOOL_GET_TS_INFO))
static int ptpNetIfFindPtpClockIndex(const char* netIfName)
{
#if (TARGET_OS_LINUX)
    if (netIfName == NULL) {
        PTP_LOG_ERROR("%s requires a valid interface name", __FUNCTION__);
        return -1;
    }

    int sd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sd < 0) {
        PTP_LOG_ERROR("failed to open socket");
        return -1;
    }

    struct ethtool_ts_info info;
    struct ifreq ifr;

    memset(&ifr, 0, sizeof(ifr));
    memset(&info, 0, sizeof(info));
    info.cmd = ETHTOOL_GET_TS_INFO;
    strncpy(ifr.ifr_name, netIfName, IFNAMSIZ - 1);
    ifr.ifr_data = (char*)&info;

    if (ioctl(sd, SIOCETHTOOL, &ifr) < 0) {
        PTP_LOG_ERROR("ioctl(SIOETHTOOL) failed!");
        close(sd);
        return -1;
    }

    close(sd);

    PTP_LOG_INFO("found PTP clock index %d", info.phc_index);
    return info.phc_index;
#else
    (void)netIfName;

    return 0;
#endif
}

Boolean ptpNetIfInitializePtpClock(NetIfRef netif, const char* netIfName)
{
    //
    // find the correct PTP clock interface
    //

    int ptpClockIndex = ptpNetIfFindPtpClockIndex(netIfName);

    if (ptpClockIndex < 0) {
        PTP_LOG_ERROR("Failed to find PTP device index");
        return false;
    }
#if TARGET_OS_LINUX
    //
    // open clock device
    //

    char ptpDevice[] = PTP_DEVICE;
    size_t bufferSize = sizeof(ptpDevice) - PTP_DEVICE_IDX_OFFS;
    long nBytes = snprintf(ptpDevice + PTP_DEVICE_IDX_OFFS, bufferSize, "%d", ptpClockIndex);
    if (nBytes < 0 || (size_t)nBytes >= bufferSize) {
        PTP_LOG_ERROR("Failed to write clock index %d", ptpClockIndex);
        return false;
    }
    PTP_LOG_INFO("Using clock device: %s", ptpDevice);

    netif->_phcFd = open(ptpDevice, O_RDWR);
    if ((netif->_phcFd == -1) || (FD_TO_CLOCKID(netif->_phcFd) == -1)) {
        PTP_LOG_ERROR("Failed to open PTP clock device");
        return false;
    }
#endif

#ifdef PTP_HW_CROSSTSTAMP

    //
    // is precise cross timestamping available?
    //

    struct ptp_clock_caps ptpCapability;
    if (ioctl(netif->_phcFd, PTP_CLOCK_GETCAPS, &ptpCapability) == -1) {
        PTP_LOG_ERROR("Failed to query PTP clock capabilities");
        return false;
    }

    netif->_crossTimestampingSupported = ptpCapability.cross_timestamping;
    PTP_LOG_INFO("Cross timestamping %s supported", netif->_crossTimestampingSupported ? "IS" : "IS NOT");
#endif
    return true;
}
#endif

#if (defined(ETHTOOL_GET_TS_INFO))
static Boolean ptpNetIfCheckInterfaceTimestampingSupport(NetIfRef netif, const char* netIfName, struct ethtool_ts_info* tsInfo)
{
    int err = 0;
    struct ifreq ifRequest;
    assert(netif && netIfName && tsInfo);

    memset(tsInfo, 0, sizeof(*tsInfo));
    memset(&ifRequest, 0, sizeof(ifRequest));

    tsInfo->cmd = ETHTOOL_GET_TS_INFO;
    strncpy(ifRequest.ifr_name, netIfName, IFNAMSIZ - 1);
    ifRequest.ifr_data = (char*)tsInfo;
    err = ioctl(netif->_sd, SIOCETHTOOL, &ifRequest);
    return (err >= 0);
}

static Boolean ptpNetIfEnableHwTimestamps(NetIfRef netif, const char* netIfName, int ifindex, const struct ethtool_ts_info* tsInfo)
{
    assert(netif && netIfName && tsInfo);

    // check if HW timestamps are possible
    unsigned int tsFlags = 0;

    // Request tx timestamps generated by the network adapter
    tsFlags |= SOF_TIMESTAMPING_TX_HARDWARE;

    // Request rx timestamps generated by the network adapter
    tsFlags |= SOF_TIMESTAMPING_RX_HARDWARE;

    // Report hardware timestamps as generated by
    tsFlags |= SOF_TIMESTAMPING_RAW_HARDWARE;

    if ((tsInfo->so_timestamping & tsFlags) != tsFlags) {
        PTP_LOG_WARNING("Hardware timestamps are not supported on interface %s", netIfName);
        return false;
    }

    struct ifreq device;
    struct hwtstamp_config hwconfig;

    memset(&device, 0, sizeof(device));
    memset(&hwconfig, 0, sizeof(hwconfig));

    //
    // retrieve network device name
    //

    device.ifr_ifindex = ifindex;
    int err = ioctl(netif->_sd, SIOCGIFNAME, &device);
    if (-1 == err) {
        PTP_LOG_ERROR("Failed to get interface name for %s: %#m", netIfName, errno);
        return false;
    }

    //
    // try to configure hardware timestamping in the network interface driver
    //

    device.ifr_data = (char*)&hwconfig;
    hwconfig.rx_filter = HWTSTAMP_FILTER_PTP_V2_EVENT;
    hwconfig.tx_type = HWTSTAMP_TX_ON;

    err = ioctl(netif->_sd, SIOCSHWTSTAMP, &device);

    if (-1 != err) {

        err = setsockopt(netif->_sd, SOL_SOCKET, SO_TIMESTAMPING, &tsFlags, sizeof(tsFlags));

        if (-1 == err) {
            PTP_LOG_ERROR("Failed to configure hardware timestamping on interface %s: %#m", netIfName, errno);
        } else {

            // initialize clock device for hw timestamping
            if (!ptpNetIfInitializePtpClock(netif, netIfName)) {
                PTP_LOG_ERROR("Failed to configure clock device for HW timestamping on interface %s", netIfName);
            } else {
                int flags = 1;
                err = setsockopt(netif->_sd, SOL_SOCKET, SO_SELECT_ERR_QUEUE, &flags, sizeof(flags));

                if (err < 0) {
                    PTP_LOG_ERROR("Failed to configure SO_SELECT_ERR_QUEUE on interface %s: %#m", netIfName, errno);
                    return false;
                }

                PTP_LOG_INFO("Hardware timestamping configured successfully on interface %s", netIfName);
                return true;
            }
        }
    }

    return false;
}

Boolean ptpNetIfEnableSwTimestamps(NetIfRef netif, const char* netIfName, const struct ethtool_ts_info* tsInfo)
{
    assert(netif && netIfName && tsInfo);
    unsigned int tsFlags = 0;

    // Request rx timestamps when data enters the kernel. These timestamps
    // are generated just after a device driver hands a packet to the
    // kernel receive stack.
    tsFlags |= SOF_TIMESTAMPING_RX_SOFTWARE;

    // Request tx timestamps when data leaves the kernel. These timestamps
    // are generated in the device driver as close as possible, but always
    // prior to, passing the packet to the network interface. Hence, they
    // require driver support and may not be available for all devices.
    tsFlags |= SOF_TIMESTAMPING_TX_SOFTWARE;

    // Report any software timestamps when available.
    tsFlags |= SOF_TIMESTAMPING_SOFTWARE;

    if ((tsInfo->so_timestamping & tsFlags) != tsFlags) {
        PTP_LOG_WARNING("Software timestamps are not supported on interface %s", netIfName);
        return false;
    }

    int err = setsockopt(netif->_sd, SOL_SOCKET, SO_TIMESTAMPING, &tsFlags, sizeof(tsFlags));

    if (-1 == err) {
        PTP_LOG_ERROR("Failed to configure software timestamping on interface %s: %#m", netIfName, errno);
    } else {

        int flags = 1;
        err = setsockopt(netif->_sd, SOL_SOCKET, SO_SELECT_ERR_QUEUE, &flags, sizeof(flags));

        if (err < 0) {
            PTP_LOG_ERROR("Failed to configure SO_SELECT_ERR_QUEUE on interface %s: %#m", netIfName, errno);
            return false;
        }

        PTP_LOG_INFO("Software timestamping configured successfully on interface %s", netIfName);
        return true;
    }

    return false;
}
#endif
#endif

static Boolean ptpNetIfInitializeTimestamping(NetIfRef netif, const char* netIfName, int ifindex, int socketFd)
{
#ifdef ETHTOOL_GET_TS_INFO
    // keep the reference to socket descriptor which will be configured for timestamping
    netif->_sd = socketFd;

    struct ethtool_ts_info tsInfo;
    if (ptpNetIfCheckInterfaceTimestampingSupport(netif, netIfName, &tsInfo)) {

        // activate HW timestamps if available
        if (ptpNetIfEnableHwTimestamps(netif, netIfName, ifindex, &tsInfo))
            return true;

        // if HW timestamps are not supported, try to activate SW timestamps
        if (ptpNetIfEnableSwTimestamps(netif, netIfName, &tsInfo))
            return true;

        // Neither HW nor SW timestamps are supported on this interface, revert to SO_TIMESTAMPNS software based timestamping
        PTP_LOG_WARNING("Neither hardware nor software timestamps are supported for %s", netIfName);

    } else {
        PTP_LOG_WARNING("Could not retrieve ethtool timestamping capabilities for %s", netIfName);
    }
    return true;
#else
    (void)ifindex;
    (void)netif;
    (void)netIfName;
    int val = 1;
    Boolean result = true;

#if defined(SO_BINTIME) /* FreeBSD */
    PTP_LOG_INFO("netInitTimestamping: trying to use SO_BINTIME\n");
    if (setsockopt(socketFd, SOL_SOCKET, SO_BINTIME, &val, sizeof(int)) < 0) {
        PTP_LOG_INFO("netInitTimestamping: failed to enable SO_BINTIME: %#m\n", errno);
        result = false;
    }
#else
    result = false;
#endif

/* fallback method */
#if defined(SO_TIMESTAMP) /* Linux, Apple, FreeBSD */
    if (!result) {
        PTP_LOG_INFO("netInitTimestamping: trying to use SO_TIMESTAMP\n");
        if (setsockopt(socketFd, SOL_SOCKET, SO_TIMESTAMP, &val, sizeof(int)) < 0) {
            PTP_LOG_ERROR("netInitTimestamping: failed to enable SO_TIMESTAMP: %#m\n", errno);
            result = false;
        } else {
            result = true;
        }
    }
#endif
    return result;
#endif
}

static Boolean ptpNetIfInitialize(NetIfRef netif, const char* netIfName)
{
    struct ifreq device;
    int err;
    int ifindex = -1;

    assert(netif && netIfName);

// initialize ipv4/ipv6 general socket
#if !defined(AIRPLAY_IPV4_ONLY)
    netif->_generalSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    int v6OnlyValue = 0;
    setsockopt(netif->_generalSocket, SOL_SOCKET, IPV6_V6ONLY, &v6OnlyValue, sizeof(v6OnlyValue));
#else
    netif->_generalSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#endif
    if (netif->_generalSocket == -1) {
        PTP_LOG_ERROR("failed to open general socket: %#m", errno);
        return false;
    }

    err = OpenSelfConnectedLoopbackSocket(&netif->_cmdSocket);
    if (err != kNoErr || netif->_cmdSocket == -1) {
        PTP_LOG_ERROR("failed to open general socket: %#m", errno);
        return false;
    }

// initialize ipv4/ipv6 event socket
#if !defined(AIRPLAY_IPV4_ONLY)
    netif->_eventSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    setsockopt(netif->_eventSocket, SOL_SOCKET, IPV6_V6ONLY, &v6OnlyValue, sizeof(v6OnlyValue));
#else
    netif->_eventSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#endif
    PTP_LOG_VERBOSE("createInterface  net_iface_l->netif->_eventSocket: %d", netif->_eventSocket);

    if (netif->_eventSocket == -1) {
        PTP_LOG_ERROR("failed to open event socket: %#m", errno);
        return false;
    }

    memset(&device, 0, sizeof(device));
    strncpy(device.ifr_name, netIfName, IFNAMSIZ - 1);

    PTP_LOG_VERBOSE("device.ifr_name: %s", device.ifr_name);

    // allow address reuse
    int tmp = 1;
    if ((setsockopt(netif->_eventSocket, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(int)) < 0) || (setsockopt(netif->_generalSocket, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof(int)) < 0)) {
        PTP_LOG_WARNING("Failed to set socket reuse\n");
    }

#if SO_BINDTODEVICE
    // Bind each socket to a specific interface
    setsockopt(netif->_eventSocket, SOL_SOCKET, SO_BINDTODEVICE, device.ifr_name, (socklen_t)strlen(device.ifr_name));
    setsockopt(netif->_generalSocket, SOL_SOCKET, SO_BINDTODEVICE, device.ifr_name, (socklen_t)strlen(device.ifr_name));
#endif

#if SIOCGIFHWADDR
    err = ioctl(netif->_eventSocket, SIOCGIFHWADDR, &device);

    if (err == -1) {
        PTP_LOG_ERROR("Failed to get interface address: %#m", errno);
        return false;
    }

    PTP_LOG_VERBOSE("device.ifr_hwaddr.sa_data: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x", device.ifr_hwaddr.sa_data[0],
        device.ifr_hwaddr.sa_data[1], device.ifr_hwaddr.sa_data[2], device.ifr_hwaddr.sa_data[3], device.ifr_hwaddr.sa_data[4],
        device.ifr_hwaddr.sa_data[5], device.ifr_hwaddr.sa_data[6], device.ifr_hwaddr.sa_data[7], device.ifr_hwaddr.sa_data[8],
        device.ifr_hwaddr.sa_data[9], device.ifr_hwaddr.sa_data[10], device.ifr_hwaddr.sa_data[11], device.ifr_hwaddr.sa_data[12],
        device.ifr_hwaddr.sa_data[13]);
#else
    struct ifaddrs* ifaList = NULL;
    struct ifaddrs* ifa = NULL;

    (void)getifaddrs(&ifaList);

    for (ifa = ifaList; ifa; ifa = ifa->ifa_next) {

        if (!ifa->ifa_addr) {
            // Skip if address not valid.
            continue;
        }

        if ((ifa->ifa_addr->sa_family == AF_LINK) && (strcmp(ifa->ifa_name, device.ifr_name) == 0)) {
            const struct sockaddr_dl* const sdl = (const struct sockaddr_dl*)ifa->ifa_addr;

            if (sdl->sdl_alen == 6) {
// get interface index
#if (TARGET_OS_BRIDGECO)
                device.ifr_ifindex = if_nametoindex(ifa->ifa_name);
#else
                ifindex = if_nametoindex(ifa->ifa_name);
#endif

// get hw address of network interface
#if (defined(HAVE_STRUCT_IFREQ_IFR_HWADDR) || TARGET_OS_BRIDGECO)
                memset(device.ifr_hwaddr.sa_data, 0, 6);
                memcpy(device.ifr_hwaddr.sa_data, &sdl->sdl_data[sdl->sdl_nlen], 6);
#else
                if (sdl->sdl_type == IFT_ETHER || sdl->sdl_type == IFT_L2VLAN) {
#ifndef LLADDR
#define LLADDR(s) ((caddr_t)((s)->sdl_data + (s)->sdl_nlen))
#endif
                    memset(device.ifr_addr.sa_data, 0, sizeof(device.ifr_addr.sa_data));
                    memcpy(device.ifr_addr.sa_data, LLADDR(sdl), sizeof(sdl->sdl_data));
                    PTP_LOG_STATUS("sdl_nlen:%d, sizeof sa_data:%d data:%.1H\n", sdl->sdl_nlen, sizeof(device.ifr_addr.sa_data), device.ifr_addr.sa_data, sizeof(device.ifr_addr.sa_data), sizeof(device.ifr_addr.sa_data));
                }
#endif /* HAVE_STRUCT_IFREQ_IFR_HWADDR */
                break;
            }
        }
    }

    // release address list
    if (ifaList)
        freeifaddrs(ifaList);
#endif

    // Set the interface's mac address
    ptpMacAddressInit(&(netif->_localAddress), &device);

#if SIOCGIFINDEX
    err = ioctl(netif->_eventSocket, SIOCGIFINDEX, &device);
    if (err == -1) {
        PTP_LOG_ERROR("Failed to get interface index: %#m", errno);
        return false;
    }
#endif

#if (TARGET_OS_LINUX || TARGET_OS_BRIDGECO)
    ifindex = device.ifr_ifindex;
    netif->_interfaceIndex = ifindex;
#else
    netif->_interfaceIndex = ifindex;
#endif

    PTP_LOG_VERBOSE("NetworkInterface::initialize():  ifindex: %d", ifindex);

    struct sockaddr* evntAddr;
    struct sockaddr* genAddr;
    size_t evntAddrSize;
    size_t genAddrSize;

#if !defined(AIRPLAY_IPV4_ONLY)
    struct sockaddr_in6 evntIpv6;
    struct sockaddr_in6 genIpv6;

    memset(&evntIpv6, 0, sizeof(evntIpv6));
    evntIpv6.sin6_port = ptpHtonsu(EVENT_PORT);
    evntIpv6.sin6_family = AF_INET6;
    evntIpv6.sin6_addr = in6addr_any;
    evntAddrSize = sizeof(evntIpv6);
    evntAddr = (struct sockaddr*)&evntIpv6;

    memset(&genIpv6, 0, sizeof(evntIpv6));
    genIpv6.sin6_port = ptpHtonsu(GENERAL_PORT);
    genIpv6.sin6_family = AF_INET6;
    genIpv6.sin6_addr = in6addr_any;
    genAddrSize = sizeof(genIpv6);
    genAddr = (struct sockaddr*)&genIpv6;
#else
    struct sockaddr_in evntIpv4;
    struct sockaddr_in genIpv4;

    memset(&evntIpv4, 0, sizeof(evntIpv4));
    evntIpv4.sin_port = ptpHtonsu(EVENT_PORT);
    evntIpv4.sin_family = AF_INET;
    evntIpv4.sin_addr.s_addr = INADDR_ANY;
    evntAddrSize = sizeof(evntIpv4);
    evntAddr = (struct sockaddr*)(&evntIpv4);

    memset(&genIpv4, 0, sizeof(evntIpv4));
    genIpv4.sin_port = ptpHtonsu(GENERAL_PORT);
    genIpv4.sin_family = AF_INET;
    genIpv4.sin_addr.s_addr = INADDR_ANY;
    genAddrSize = sizeof(genIpv4);
    genAddr = (struct sockaddr*)(&genIpv4);
#endif

    err = bind(netif->_eventSocket, evntAddr, (socklen_t)evntAddrSize);
    if (-1 == err) {
        PTP_LOG_ERROR("Call to bind() for netif->_eventSocket failed %#m", errno);
        check_panic(errno != EACCES, "Process doesn't have permission to bind to PTP Event port 319");
        return false;
    }

    err = bind(netif->_generalSocket, genAddr, (socklen_t)genAddrSize);
    if (-1 == err) {
        PTP_LOG_ERROR("Call to bind() for netif->_generalSocket failed %#m", errno);
        check_panic(errno != EACCES, "Process doesn't have permission to bind to PTP General port 320");
        return false;
    }

#ifdef SO_RCVBUF
#define UNICAST_MAX_DESTINATIONS 100
    /* try increasing receive buffers for unicast Sync processing */
    uint32_t n = 0;
    socklen_t nlen = sizeof(n);

    if (getsockopt(netif->_eventSocket, SOL_SOCKET, SO_RCVBUF, &n, &nlen) == -1 || n < (UNICAST_MAX_DESTINATIONS * 1024)) {
        n = UNICAST_MAX_DESTINATIONS * 1024;
        if (setsockopt(netif->_eventSocket, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n)) < 0) {
            PTP_LOG_ERROR("Failed to increase event socket receive buffer\n");
        }
    }
    if (getsockopt(netif->_generalSocket, SOL_SOCKET, SO_RCVBUF, &n, &nlen) == -1 || n < (UNICAST_MAX_DESTINATIONS * 1024)) {
        n = UNICAST_MAX_DESTINATIONS * 1024;
        if (setsockopt(netif->_generalSocket, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n)) < 0) {
            PTP_LOG_ERROR("Failed to increase general socket receive buffer\n");
        }
    }
#endif /* SO_RCVBUF */

#ifdef SO_SNDTIMEO
    struct timeval timeout;
    timeout.tv_sec = 120;
    timeout.tv_usec = 0;

    if (setsockopt(netif->_eventSocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout)) < 0) {
        PTP_LOG_ERROR("Failed to increase event socket receive buffer\n");
    }
#endif

#if (defined(IP_PKTINFO) || defined(IP_RECVDSTADDR))
    int val = 1;
#endif
#if (defined(IP_PKTINFO))
    setsockopt(netif->_eventSocket, IPPROTO_IP, IP_PKTINFO, &val, sizeof(int));
#endif /* IP_PKTINFO */

#if (defined(IP_RECVDSTADDR))
    setsockopt(netif->_eventSocket, IPPROTO_IP, IP_RECVDSTADDR, &val, sizeof(int));
#endif

    if (!ptpNetIfInitializeTimestamping(netif, netIfName, ifindex, netif->_eventSocket)) {
        PTP_LOG_ERROR("initialization of timestamping support failed\n");
        return false;
    }

    // TX timestamps are most probably supported at this moment
    netif->_txTimestampsSupported = true;
    return true;
}

void ptpNetIfShutdown(NetIfRef netif)
{
    if (netif != NULL && netif->_cmdSocket >= 0) {
        SendSelfConnectedLoopbackMessage(netif->_cmdSocket, "q", 1);
    }
}

void ptpNetIfGetMacAddress(NetIfRef netif, MacAddressRef addr)
{
    assert(netif);
    ptpMacAddressAssign(addr, &(netif->_localAddress));
}

#if !TARGET_OS_BRIDGECO
static void ptpNetIfTimespecDiff(struct timespec* start, struct timespec* stop, struct timespec* result)
{
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }
}

static void ptpNetIfTimespecAdd(struct timespec* start, struct timespec* stop, struct timespec* result)
{
    result->tv_sec = start->tv_sec + stop->tv_sec;
    result->tv_nsec = start->tv_nsec + stop->tv_nsec;

    if (result->tv_nsec > kNanosecondsPerSecond) {
        result->tv_nsec -= kNanosecondsPerSecond;
        result->tv_sec++;
    } else if (result->tv_nsec < 0) {
        result->tv_nsec += kNanosecondsPerSecond;
        result->tv_sec--;
    }
}

#if (TARGET_OS_LINUX)
static inline int64_t ptpNetIfClockTimeToNanoseconds(struct ptp_clock_time t)
{
    return t.sec * (int64_t)kNanosecondsPerSecond + t.nsec;
}

static inline void ptpNetIfClockTimeToTimestamp(struct ptp_clock_time* t, TimestampRef timestamp)
{
    timestamp->_secondsLSB = t->sec & 0xFFFFFFFF;
    timestamp->_secondsMSB = (uint16_t)(t->sec >> sizeof(timestamp->_secondsLSB) * 8);
    timestamp->_nanoseconds = t->nsec;
}
#endif

static Boolean ptpNetIfCompensateTimestamp(NetIfRef netif, TimestampRef timestamp, const Boolean isHardwareTs)
{
    if (isHardwareTs) {
#ifdef PTP_HW_CROSSTSTAMP
        Timestamp driverTs;
        Timestamp systemTs;
        Timestamp diffTs;
        Boolean driverTimestampCompensated = false;

        if (netif->_crossTimestampingSupported) {

            struct ptp_sys_offset_precise offset;
            memset(&offset, 0, sizeof(offset));

            // get precise cross timestamp info
            if (ioctl(netif->_phcFd, PTP_SYS_OFFSET_PRECISE, &offset) == 0) {

                ptpNetIfClockTimeToTimestamp(&offset.device, &driverTs);
                ptpNetIfClockTimeToTimestamp(&offset.sys_realtime, &systemTs);

                // calculate difference between CLOCK_REALTIME and driver timestamp
                ptpTimestampSub(&diffTs, &systemTs, &driverTs);

                // apply difference to timestamp. timestamp will now be in CLOCK_REALTIME domain
                ptpTimestampAdd(timestamp, timestamp, &diffTs);

                // we have succeeded
                driverTimestampCompensated = true;
            }
        }

        if (!driverTimestampCompensated)
#else
        (void)netif;
#endif
#if (defined(PTP_SYS_OFFSET))
        {
            struct ptp_sys_offset offset;
            memset(&offset, 0, sizeof(offset));
            offset.n_samples = PTP_MAX_SAMPLES;

            // retrieve PTP_MAX_SAMPLES measurement of cross timestamps between system and phc time
            if (ioctl(netif->_phcFd, PTP_SYS_OFFSET, &offset) == -1) {

                // failed. We can't convert driver timestamps to system timestamps. The PTP has to rely on software
                // timestamping only
                return false;
            }

            // iterate through all measurements
            int64_t diffs[PTP_MAX_SAMPLES] = { 0 };

            for (unsigned int i = 0; i < offset.n_samples; ++i) {
                // store difference between average system time (two consecutive measurements) and matching PTP clock timestamp
                diffs[i] = (ptpNetIfClockTimeToNanoseconds(offset.ts[2 * i + 2]) + ptpNetIfClockTimeToNanoseconds(offset.ts[2 * i + 0])) / 2 - ptpNetIfClockTimeToNanoseconds(offset.ts[2 * i + 1]);
            }

            // sort the differences
            quickSortInt64(diffs, 0, PTP_MAX_SAMPLES);

            // and pick the smallest value as a diff
            int64_t smallestDiff = diffs[0];

            // new timestamp value
            uint64_t timestampNs = TIMESTAMP_TO_NANOSECONDS(timestamp) + smallestDiff;

            // convert it back to timestamp
            uint64_t seconds = timestampNs / 1000000000;
            uint32_t ns = timestampNs - seconds * 1000000000;
            ptpTimestampInitCtor(timestamp, ns, seconds, 0);
        }
#endif
    }

    // these timestamps are similar to those retrieved via clock_gettime(CLOCK_REALTIME).
    // as our main clock uses CLOCK_MONOTONIC_RAW, we have co calculate offset between
    // CLOCK_REALTIME and CLOCK_MONOTONIC_RAW and apply it here
    struct timespec tsRealtime, tsRaw, tsDiff;
    clock_gettime(CLOCK_REALTIME, &tsRealtime);
    clock_gettime(CLOCK_PTP_GET_TIME, &tsRaw);

    // calculate clock compensation
    ptpNetIfTimespecDiff(&tsRealtime, &tsRaw, &tsDiff);

    // get timestamp in timespec format
    struct timespec ts = ptpTimestampToTimespec(timestamp);

    // add offset
    ptpNetIfTimespecAdd(&ts, &tsDiff, &ts);

    // convert it back to Timestamp
    ptpTimestampInitFromTsCtor(timestamp, &ts);
    return true;
}

static Boolean ptpNetIfExtractTimestamp(NetIfRef netif, struct cmsghdr* cmsg, TimestampRef timestamp)
{
    struct timespec* ts = ((struct timespec*)CMSG_DATA(cmsg));
    struct timespec localTs;
    struct timespec* selectedTs = NULL;
    Boolean isHardwareTs = false;

    if (false) {

    }
#if (defined(SO_TIMESTAMPING))
    else if (cmsg->cmsg_type == SO_TIMESTAMPING) {

        if (cmsg->cmsg_len < (3 * sizeof(struct timespec)))
            return false;

        // get access to timespec array containing up to 3 timestamps:
        // software, hardware transformed to system time and hardware raw timestamps
        // do we have both hardware timestamps present?
        if (ts[2].tv_sec || ts[2].tv_nsec) {
            // take hardware timestamp
            isHardwareTs = true;
            selectedTs = &ts[2];
        } else {
            // otherwise take software timestamps
            selectedTs = &ts[0];
        }
    }
#endif
#if (defined(SO_TIMESTAMPNS))
    else if (cmsg->cmsg_type == SO_TIMESTAMPNS) {
        if (cmsg->cmsg_len < sizeof(struct timespec))
            return false;

        // SO_TIMESTAMPNS contains only one timestamp
        selectedTs = &ts[0];
    }
#endif
#if (defined(SO_TIMESTAMP))
    else if (cmsg->cmsg_type == SO_TIMESTAMP) {
        if (cmsg->cmsg_len < sizeof(struct timeval))
            return false;

        struct timeval* tv = (struct timeval*)&ts[0];
        localTs.tv_sec = tv->tv_sec;
        localTs.tv_nsec = (long)tv->tv_usec * 1000;
        selectedTs = &localTs;
    }
#endif
#if (defined(SO_BINTIME))
    else if (cmsg->cmsg_type == SO_BINTIME) {
        if (cmsg->cmsg_len < sizeof(struct bintime))
            return false;

        struct bintime* bt = (struct bintime*)CMSG_DATA(cmsg);
        bintime2timespec(bt, &localTs);
        selectedTs = &localTs;
    }
#endif
    else if (cmsg->cmsg_type != SO_ACCEPTCONN) { // unsupported cmsg_type
        PTP_LOG_ERROR("************ unsupported cmsg_type == %d\n", cmsg->cmsg_type);
        (void)localTs;
        return false;
    }

    if (!selectedTs)
        return false;

    // convert timespec to Timestamp
    ptpTimestampInitFromTsCtor(timestamp, selectedTs);

    // compensate timestamp offset
    return ptpNetIfCompensateTimestamp(netif, timestamp, isHardwareTs);
}
#endif /* !TARGET_OS_BRIDGECO */

NetResult ptpNetIfSend(NetIfRef netif, LinkLayerAddressRef addr, uint8_t* payload, size_t length, TimestampRef txTimestamp, Boolean isEvent)
{
    struct sockaddr_in* remote;
    socklen_t remoteSize;
    int err;

#if !TARGET_OS_BRIDGECO
    struct msghdr msg;
    struct cmsghdr* cmsg;
    uint8_t control[CTRL_BUFFER_SIZE];
    uint8_t junk[JUNK_BUFFER_SIZE];
#endif

    int socket = 0;
#if defined(AIRPLAY_IPV4_ONLY)
    struct sockaddr_in remoteIpv4;
    memset(&remoteIpv4, 0, sizeof(remoteIpv4));
    remoteIpv4.sin_family = AF_INET;
    remoteIpv4.sin_port = ptpHtonsu(ptpLinkLayerAddressGetPort(addr));
    ptpLinkLayerAddressGetAddress(addr, &(remoteIpv4.sin_addr));
    remote = (struct sockaddr_in*)&remoteIpv4;
    remoteSize = sizeof(remoteIpv4);
#else
    struct sockaddr_in6 remoteIpv6;
    if (4 == ptpLinkLayerAddressIpVersion(addr)) {
#if !TARGET_OS_DARWIN
        struct sockaddr_in remoteIpv4;
        memset(&remoteIpv4, 0, sizeof(remoteIpv4));
        remoteIpv4.sin_family = AF_INET;
        remoteIpv4.sin_port = ptpHtonsu(ptpLinkLayerAddressGetPort(addr));
        ptpLinkLayerAddressGetAddress(addr, &(remoteIpv4.sin_addr));
        remote = (struct sockaddr_in*)&remoteIpv4;
        remoteSize = sizeof(remoteIpv4);
#else
        memset(&remoteIpv6, 0, sizeof(remoteIpv6));
        remoteIpv6.sin6_family = AF_INET6;
        remoteIpv6.sin6_port = ptpHtonsu(ptpLinkLayerAddressGetPort(addr));
        remoteIpv6.sin6_scope_id = netif->_interfaceIndex;
        remoteIpv6.sin6_addr.__u6_addr.__u6_addr8[10] = 0xff;
        remoteIpv6.sin6_addr.__u6_addr.__u6_addr8[11] = 0xff;
        remoteIpv6.sin6_addr.__u6_addr.__u6_addr32[3] = addr->_ipV4Address;
        remote = (struct sockaddr_in*)&remoteIpv6;
        remoteSize = sizeof(remoteIpv6);
#endif
    } else {
        memset(&remoteIpv6, 0, sizeof(remoteIpv6));
        remoteIpv6.sin6_family = AF_INET6;
        remoteIpv6.sin6_port = ptpHtonsu(ptpLinkLayerAddressGetPort(addr));
        remoteIpv6.sin6_scope_id = netif->_interfaceIndex;
        ptpLinkLayerAddressGetAddress6(addr, &(remoteIpv6.sin6_addr));
        remote = (struct sockaddr_in*)&remoteIpv6;
        remoteSize = sizeof(remoteIpv6);
    }
#endif

    if (isEvent) {
        socket = netif->_eventSocket;
    } else {
        socket = netif->_generalSocket;
    }

    // use system timestamp as a backup
    if (txTimestamp)
        ptpClockCtxGetTime(txTimestamp);

#if !TARGET_OS_BRIDGECO
    err = (int)sendto(socket, payload, length, 0, (void*)remote, remoteSize);
#else
    err = (int)sendto(socket, (char*)payload, length, 0, (void*)remote, remoteSize);
#endif

    if (err < 0) {

        char addrStr[ADDRESS_STRING_LENGTH] = { 0 };
        PTP_LOG_ERROR("Failed to send to ip '%s': %#m", ptpLinkLayerAddressString(addr, addrStr, sizeof(addrStr)), errno);
        return eNetFatal;
    } else {

        // for non-event packets, we can't (and don't have to) use packet level timestamping,
        // so we will return our system timestamp instead:
        if (!isEvent)
            return eNetSucceed;

        // we do the same if TX timestamps are not supported
        if (!netif->_txTimestampsSupported)
            return eNetSucceed;

#if !TARGET_OS_BRIDGECO
        if (err > 0) {
            int got = 0;

            struct pollfd pollfd;
            memset(&pollfd, 0, sizeof(pollfd));
            pollfd.fd = socket;
#if (TARGET_OS_LINUX)
            pollfd.events = POLLPRI;
#else
            pollfd.events = POLLIN;
#endif
            int ret = poll(&pollfd, 1, POLL_TIMEOUT_MS);

            if (ret < 1) {

                // it makes no sense to continue, we have to use software timestamps
                if (!ret)
                    PTP_LOG_WARNING("Poll timed out after %d ms", POLL_TIMEOUT_MS);
                else
                    PTP_LOG_ERROR("Poll failed: %#m", errno);

                netif->_txTimestampsSupported = false;

#if (defined(SO_TIMESTAMPING) && (defined(SO_TIMESTAMPNS) || defined(SO_TIMESTAMP)))
                unsigned int val = 0;
                if (setsockopt(socket, SOL_SOCKET, SO_TIMESTAMPING, &val, sizeof(int)) < 0) {
                    PTP_LOG_ERROR("Failed to unset SO_TIMESTAMPING");
                }

#if (defined(SO_TIMESTAMPNS))
                val = 1;
                PTP_LOG_WARNING("SO_TIMESTAMPING TX software timestamp failure - reverting to SO_TIMESTAMPNS used ONLY for RX timestamps");
                if (setsockopt(socket, SOL_SOCKET, SO_TIMESTAMPNS, &val, sizeof(int)) < 0) {
                    PTP_LOG_ERROR("Failed to revert to SO_TIMESTAMPNS");
                }
#elif (defined(SO_TIMESTAMP))
                val = 1;
                PTP_LOG_WARNING("SO_TIMESTAMPING TX software timestamp failure - reverting to SO_TIMESTAMP used ONLY for RX timestamps");
                if (setsockopt(socket, SOL_SOCKET, SO_TIMESTAMP, &val, sizeof(int)) < 0) {
                    PTP_LOG_ERROR("Failed to revert to SO_TIMESTAMP");
                }
#endif
#endif

                return eNetSucceed;
#if (TARGET_OS_LINUX)
            } else if (!(pollfd.revents & POLLPRI)) {
#else
            } else if (!(pollfd.revents & POLLIN)) {
#endif
                PTP_LOG_ERROR("Poll failed: woke up on non ERR event");
                return eNetSucceed;
            }

            struct iovec iov;

            // we can't use actual payload as this array gets overwritten by sk_buf structure + the actual payload
            iov.iov_base = junk;
            iov.iov_len = sizeof(junk);

            msg.msg_iov = &iov;
            msg.msg_iovlen = 1;
            msg.msg_name = remote;
            msg.msg_namelen = remoteSize;
            msg.msg_control = control;
            msg.msg_controllen = sizeof(control);
            msg.msg_flags = 0;
#if (defined(MSG_ERRQUEUE))
            got = (int)recvmsg(socket, &msg, MSG_ERRQUEUE | MSG_DONTWAIT);
#else
            got = (int)recvmsg(socket, &msg, MSG_DONTWAIT);
#endif
            if (got < 1) {
                if (got == -1 && errno != EAGAIN && errno != EINTR) {
                    PTP_LOG_ERROR("%s, recvmsg(%#m) failed", __func__, errno);
                }
#if (!TARGET_OS_LINUX && !TARGET_OS_BRIDGECO)
                return eNetSucceed;
#endif
            }

            if (msg.msg_flags & MSG_TRUNC) {
                PTP_LOG_ERROR("%s, received truncated message.\n", __func__);
                return eNetSucceed;
            }

            if (msg.msg_flags & MSG_CTRUNC) {
                PTP_LOG_ERROR("%s, truncated ancillary data.\n", __func__);
                return eNetSucceed;
            }

            // Retrieve the timestamp
            if (txTimestamp) {

                cmsg = CMSG_FIRSTHDR(&msg);

                while (cmsg != NULL) {
                    if (cmsg->cmsg_level == SOL_SOCKET) {
                        Timestamp timestamp;
                        if (ptpNetIfExtractTimestamp(netif, cmsg, &timestamp))
                            ptpTimestampAssign(txTimestamp, &timestamp);
                    }

                    cmsg = CMSG_NXTHDR(&msg, cmsg);
                }
            }
        }
#endif
    }

    return eNetSucceed;
}

NetResult ptpNetIfReceive(NetIfRef netif, LinkLayerAddressRef addr, uint8_t* payload, size_t* length, TimestampRef ingressTime)
{
    fd_set readfds;
    int err = 0;
    struct msghdr msg;
    uint8_t control[CTRL_BUFFER_SIZE];
#if !TARGET_OS_BRIDGECO
    struct cmsghdr* cmsg;
#endif

    // by default, we will use 16ms timeout
    struct timeval nextTimeout = { 0, 16000 }; // 16 ms

    struct sockaddr_storage remoteAddress;
    size_t remoteSize = sizeof(struct sockaddr_storage);

    struct iovec sgentry;
    NetResult ret = eNetSucceed;

    assert(netif && length);

    int maxfd = MAX(MAX(netif->_eventSocket, netif->_generalSocket), netif->_cmdSocket);

    // try to receive all packets from the interface, when the 'drain' flag is set
    while (ret == eNetSucceed) {
        struct timeval timeout = nextTimeout;

        FD_ZERO(&readfds);
        FD_SET(netif->_eventSocket, &readfds);
        FD_SET(netif->_generalSocket, &readfds);
        FD_SET(netif->_cmdSocket, &readfds);

#if TARGET_OS_BRIDGECO
        err = platform_select(maxfd + 1, &readfds, NULL, NULL, &timeout);
#else
        err = select(maxfd + 1, &readfds, NULL, NULL, &timeout);
#endif

        if (err == 0) {

            // no data available on socket, break the loop
            ret = nNetFail;
            break;

        } else if (err == -1) {

            if (errno == EINTR) {
                // Caught signal
                PTP_LOG_ERROR("select() recv signal");
                ret = nNetFail;
            } else if (errno == EAGAIN) {
                PTP_LOG_ERROR("select() recv EAGAIN");
                continue;
            } else {
                PTP_LOG_ERROR("select() failed");
                ret = eNetFatal;
            }
        } else {
            int socket = -1;

            if (FD_ISSET(netif->_cmdSocket, &readfds))
                socket = netif->_cmdSocket;
            else if (FD_ISSET(netif->_eventSocket, &readfds))
                socket = netif->_eventSocket;
            else if (FD_ISSET(netif->_generalSocket, &readfds))
                socket = netif->_generalSocket;

            if (socket == netif->_cmdSocket) {
                ret = eNetFatal;
            } else {
                memset(&msg, 0, sizeof(msg));
                memset(&remoteAddress, 0, remoteSize);

                sgentry.iov_base = payload;
                sgentry.iov_len = *length;

                msg.msg_iov = &sgentry;
                msg.msg_iovlen = 1;
                msg.msg_name = &remoteAddress;
                msg.msg_namelen = (socklen_t)remoteSize;
                msg.msg_control = control;
                msg.msg_controllen = sizeof(control);

                err = (int)recvmsg(socket, &msg, MSG_DONTWAIT);

                if (err < 0) {

#if !TARGET_OS_BRIDGECO
                    if (errno == ENOMSG) {
                        PTP_LOG_ERROR("Got ENOMSG: %s:%d", __FILE__, __LINE__);
                        ret = nNetFail;
                    } else
#endif
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // there is no message to receive, break the loop
                        ret = nNetFail;
                        break;
                    } else {
                        PTP_LOG_ERROR("recvmsg() failed: %#m", errno);
                        ret = eNetFatal;
                    }
                } else {

                    if (AF_INET == remoteAddress.ss_family) {
                        struct sockaddr_in* address = (struct sockaddr_in*)&remoteAddress;
                        ptpLinkLayerAddressAssignSockaddrIn(addr, address);
                    }
#if !defined(AIRPLAY_IPV4_ONLY)
                    else {
                        struct sockaddr_in6* address = (struct sockaddr_in6*)&remoteAddress;
                        ptpLinkLayerAddressAssignSockaddrIn6(addr, address);
                    }
#endif
                    ptpClockCtxGetTime(ingressTime);

                    if (err > 0 && !(payload[0] & 0x8)) {
#if TARGET_OS_BRIDGECO
                        // try to read driver timestamp
                        unsigned int iDriverTimestamp = 0;
                        if (t_getsockopt(socket, SO_REC_TIMESTAMP, &iDriverTimestamp) == 0) {
                            // convert driver timestamp to system time
                            struct timespec driverTs;
                            driverTimestampToTimespec(iDriverTimestamp, &driverTs);

                            // and keep it as our ingress time
                            ptpTimestampInitFromTsCtor(ingressTime, &driverTs);
                        }
#else
                        /* Retrieve the timestamp */
                        cmsg = CMSG_FIRSTHDR(&msg);
                        while (cmsg != NULL) {
                            if (cmsg->cmsg_level == SOL_SOCKET) {
                                Timestamp timestamp;
                                if (ptpNetIfExtractTimestamp(netif, cmsg, &timestamp)) {
                                    ptpTimestampAssign(ingressTime, &timestamp);
                                    break;
                                }
                            }
                            cmsg = CMSG_NXTHDR(&msg, cmsg);
                        }
#endif
                    }
                    // we now have one valid message
                    break;
                }
            }
        }
    }

    *length = (size_t)err;
    return ret;
}

Boolean ptpNetIfGetLinkSpeed(NetIfRef netif, uint32_t* speed)
{
#if TARGET_OS_BRIDGECO
    *speed = LINKSPEED_100MBIT;
#elif (TARGET_OS_LINUX)
    struct ifreq ifr;
    struct ethtool_cmd edata;

    int sd;

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        PTP_LOG_ERROR("Unable to open INET socket!");
        return false;
    }

    ifr.ifr_ifindex = netif->_interfaceIndex;
    if (ioctl(sd, SIOCGIFNAME, &ifr) == -1) {
        PTP_LOG_ERROR("SIOCGIFNAME failed: %#m", errno);
        close(sd);
        return false;
    }

    ifr.ifr_data = (char*)&edata;
    edata.cmd = ETHTOOL_GSET;

    if (ioctl(sd, SIOCETHTOOL, &ifr) == -1) {
        PTP_LOG_ERROR("SIOCETHTOOL failed, unable to detect link speed (error = %#m)", errno);
        close(sd);
        return false;
    }
    close(sd);

    switch (ethtool_cmd_speed(&edata)) {
    default:
        PTP_LOG_ERROR("Unknown or invalid speed!");
        return false;

    case SPEED_100:
        *speed = LINKSPEED_100MBIT;
        break;

    case SPEED_1000:
        *speed = LINKSPEED_1GBIT;
        break;

    case SPEED_2500:
        *speed = LINKSPEED_2_5GBIT;
        break;

    case SPEED_10000:
        *speed = LINKSPEED_10GBIT;
        break;
    }

#else
    (void)netif;
    (void)speed;
    return false;
#endif
    PTP_LOG_STATUS("Link Speed: %d kb/sec", *speed);
    return true;
}
