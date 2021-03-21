#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/wireless.h>
#include <unistd.h>

#include "WACInterface.h"
#include "wm_util.h"
#include "nvram.h"
#ifndef WL_TOOL
#include "wl_util.h"
#endif
#ifdef RA1_SUPPORT
#include "udhcpd.c"
#endif
extern void mvUtil_TrimStringRight(char *buf);
extern int SocketClientReadWriteMsg(const char *path, char *pIn, int pInlen, char *pOut, int *outlen, int blocking);

// define wac parameters here
#define WAC_HARDWARE_REVISION "HW.1.1.1.1.1"
#define WAC_FIRMWARE_REVISION "FW.1.1.1.1.1"
#define WAC_SERIAL_NUMBER     "SER.1.1.1.1.1"
#define WAC_MODEL     "Device1,1"
#define WAC_MANUFACTURER   "Wiimu"
#define WAC_BUNDLE_SEEDID   "24D4XFAF43"
#define WAC_SUPPORTS_AIRPLAY   1
#define WAC_SUPPORTS_AIRPRINT  0
#define WAC_SUPPORTS2_4G_WIFI  1
#define WAC_SUPPORTS5G_WIFI    1
#define WAC_SUPPORTS_WAKE_ON_WIFI 0
#define WAC_NUM_EA_PROTOCOLS      0
#define WAC_EA_PROTOCOLS       NULL

static char save_apple_ie[256];
static unsigned short save_apple_ie_len = 0;
enum {
    WAC_IE_FLAG_INSERT = 1,
    WAC_IE_FLAG_DELETE = 2,
    WAC_IE_FLAG_DELETE_ALL = 3
};

#define RTPRIV_IOCTL_CONFIG_WACIE   (SIOCIWFIRSTPRIV + 0x1C)
extern char const *nvram_bufget(int _index, char *name);
extern int nvram_bufset(int _index, char *name, char *value);
#define INFO_DEVICENAME_FILE_PATH "/tmp/airplay_name"
#define INFO_PASSWORD_FILE_PATH  "/tmp/airplay_password"


static int get_wifi_softap_interface_name(char *outBuf, int outBufSize)
{
    const char *wlan_name = NULL;

    wlan_name = WIFI_INFC;
    
#ifdef PLATFORM_AMLOGIC
    wlan_name = "wlan1";//ap mode using wlan1
#endif

    memset(outBuf, 0, outBufSize);
    strncpy(outBuf, wlan_name, outBufSize - 1);

    return 0;
}


/* flag=1 insert,  flag=2 remove, flag=3 remove all  */
static int SetWlanIE(char *iface, char *SrcIE, unsigned short IELen, char flag)
{
    int  dataLen = 0, ouiParsed = 0;
    char *p = NULL;
    char pOUI[32] = {0};
    char IEString[256] = {0};
    int ret = 0;
    VNDR_IE_OPT_E opt_flag = (flag == WAC_IE_FLAG_INSERT) ?  VNDR_IE_ELEM_ADD : VNDR_IE_ELEM_DEL;

    size_t i = 0;
    printf("[%s] Flag=[%d],IE=[ ", __func__, flag);

    p = IEString;
    for(i = 0; i < IELen; ++i) {
        printf("%.2X", *(unsigned char *)(SrcIE + i));

        if(i == 0) {
            //DD TAG, Skip it
        } else if(i == 1) {
            //Data len
            dataLen = SrcIE[i];
        } else if(i >= 2 && i <= 4) {
            //OUI
            if(!ouiParsed) {
                sprintf(pOUI, "%02X:%02X:%02X", *(unsigned char *)(SrcIE + 2), *(unsigned char *)(SrcIE + 3),
                        *(unsigned char *)(SrcIE + 4));
                ouiParsed = 1;
            }
        } else {
            sprintf(p, "%02X", *(unsigned char *)(SrcIE + i));
            p += 2;
        }
    }

    printf(" ]\n\n");

    //printf("IEString=[%s]\n", IEString);
#ifdef WL_TOOL
    char cmd[512] = {0};

    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, sizeof(cmd) - 1, "/system/workdir/bin/wl %s 3 %d %s %s", dataLen,
             (flag == WAC_IE_FLAG_INSERT) ? "add_ie" : "del_ie", pOUI, IEString);
    printf("cmd=[%s]\n", cmd);

    system(cmd);
#else
    unsigned char pkt_flag = 0;

    pkt_flag |= (1 << WL_BEACON_FLAG_BIT); /* beacon frame */
    pkt_flag |= (1 << WL_PROBE_RESP_FLAG_BIT); /* probe resp frame */

    wl_util_vendor_ie_opt(opt_flag, pkt_flag, (const char *) iface, dataLen, (const char *)pOUI, (const char *)IEString);
#endif

    fprintf(stderr, "WAC IE %s finish, update backup data.\n", (VNDR_IE_ELEM_ADD == opt_flag) ? "add" : "del");

    if(VNDR_IE_ELEM_ADD == opt_flag) {
        size_t cp_len = IELen > sizeof(save_apple_ie) ?  sizeof(save_apple_ie) : IELen;
        memcpy((void *)&save_apple_ie, (void *)SrcIE, cp_len);
        save_apple_ie_len = cp_len;
    } else if(VNDR_IE_ELEM_DEL == opt_flag) {
        memset((void *)&save_apple_ie, 0, sizeof(save_apple_ie));
        save_apple_ie_len = 0;
    }

    return ret;
}

#if 0
static void restart_airplay(void)
{
    printf("[WAC]%s\n", __FUNCTION__);
    // tell airplay config maybe changed, such as airplay name and password
    SocketClientReadWriteMsg("/tmp/RequestAirplay", "updateConfig", strlen("updateConfig"), NULL, NULL, 0);
}
#endif

static int get_mac_by_ifname(const char *ifname, char *outMAC)
{
    struct ifreq ifr;
    struct ifconf ifc;
    char buf[1024];

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if(sock == -1)
        return -1;

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if(ioctl(sock, SIOCGIFCONF, &ifc) == -1)
        goto exit;

    struct ifreq *it = ifc.ifc_req;
    const struct ifreq *const end = it + (ifc.ifc_len / sizeof(struct ifreq));

    for(; it != end; ++it) {
        if(strcmp(ifname, it->ifr_name))
            continue;
        strcpy(ifr.ifr_name, it->ifr_name);
        if(ioctl(sock, SIOCGIFFLAGS, &ifr) == 0 &&
                ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
            memcpy(outMAC, ifr.ifr_hwaddr.sa_data, 6);
            close(sock);
            return 0;
        }
    }

exit:
    close(sock);
    return -1;
}


void platform_save_airplay_password(const char *const inPlayPassword)
{
    char buf[256];
    printf("[WAC]%s(%s)\n", __FUNCTION__, inPlayPassword);
    if(inPlayPassword) {
//  nvram_bufset((int)RT2860_NVRAM, (char *)NVRAM_AIRPLAY_PASSWORD, (char *)inPlayPassword);
        wm_db_set((char *)NVRAM_AIRPLAY_PASSWORD, (char *)inPlayPassword);
        memset((void *)&buf, 0, sizeof(buf));
        sprintf(buf, "echo %s > %s", inPlayPassword, INFO_PASSWORD_FILE_PATH);
        system(buf);
    } else {
//  nvram_bufset((int)RT2860_NVRAM, (char *)NVRAM_AIRPLAY_PASSWORD, (char *)"");
        wm_db_set((char *)NVRAM_AIRPLAY_PASSWORD, (char *)"");
        memset((void *)&buf, 0, sizeof(buf));
        sprintf(buf, "rm %s", INFO_PASSWORD_FILE_PATH);
        system(buf);
    }
// nvram_commit((int)RT2860_NVRAM);
}

void platform_save_wac_name(const char *const inName)
{
    char buf[256];
    printf("[WAC]%s(%s)\n", __FUNCTION__, inName);
    if(inName) {

#if 0
        nvram_bufset((int)RT2860_NVRAM, (char *)NVRAM_WAC_NAME, (char *)inName);
#else
//  char const *pp=nvram_bufget((int)RT2860_NVRAM ,(char *)"DeviceName");
        char pp[64] = {0};
        wm_db_get("DeviceName", pp);
        if(strlen(pp) > 0 && !strcmp(inName, pp))
            return;

        // update devicename if wac name changed
//  nvram_bufset((int)RT2860_NVRAM, (char *)"DeviceName", (char *)inName);
//  nvram_commit(RT2860_NVRAM);

        wm_db_set("DeviceName", (char *)inName);

        memset((void *)&buf, 0, sizeof(buf));
        sprintf(buf, "echo -n \"%s\" > %s", inName, INFO_DEVICENAME_FILE_PATH);
        system(buf);

        memset((void *)&buf, 0, sizeof(buf));
        sprintf(buf, "NameChanged:%s", inName);
        SocketClientReadWriteMsg("/tmp/Requesta01controller", buf, strlen(buf), NULL, NULL, 0);
#endif

    }
}

int platform_join_wifi(const char *const inSSID, const uint8_t *const inWiFiPSK, size_t inWiFiPSKLen)
{
    //printf("[WAC]%s(%s %s %d)\n", __FUNCTION__, inSSID, inWiFiPSK, inWiFiPSKLen);
    char ap_info[256];
    char buf[128];
    char buf1[128];
    int i;

    memset((void *)&ap_info, 0, sizeof(ap_info));
    memset((void *)&buf, 0, sizeof(buf));
    memset((void *)&buf1, 0, sizeof(buf1));

    for(i = 0; i < (int)strlen(inSSID); ++i) {
        sprintf(buf1 + 2 * i, "%.2x", *(unsigned char *)(inSSID + i));
    }

    if(inWiFiPSKLen > 0) {
        
        for(i = 0; i < (int)inWiFiPSKLen; ++i) {
            sprintf(buf + 2 * i, "%.2x", *(unsigned char *)(inWiFiPSK + i));
        }
        
        sprintf(ap_info, "WACSetWiFiNetwork:%s:%s", buf1, buf);        
    } else {
        sprintf(ap_info, "WACSetWiFiNetwork:%s", buf1);
    }

    printf("[WAC] Sending network setup msg [%s]\n", ap_info);
    
    SocketClientReadWriteMsg("/tmp/RequestNetguard", ap_info, strlen(ap_info), 0, 0, 0);
        
    return 0;
}

#ifdef RA1_SUPPORT
static void get_wac_ssid(char *ssid, int ssid_len)
{
    char buf[256];
    char const *pp;
    if(ssid == NULL || ssid_len < 0)
        return;

    pp = nvram_bufget((int)RT2860_NVRAM, (char *)"SSID1");
    printf("%s SSID1[%s]len=%d\n", __FUNCTION__, pp, strlen(pp));
    if(pp && strlen(pp) > 0) {
        strcpy(buf, pp);
        mvUtil_TrimStringRight(buf);
        memset((void *)ssid, 0, ssid_len);
        if((int)strlen(buf) + 4 <= ssid_len)
            sprintf(ssid, "%s_WAC", buf);
        else
            sprintf(ssid, "%s", "WAC");
        printf("[WAC]%s(%s)\n", __FUNCTION__, ssid);
    }
}
#endif


void platform_softap_start(const uint8_t *inIE, size_t inIELen)
{
    int  ret = 0;
    char wifi_name[64] = {0};

    printf("[WAC][%s]Len=(%d)\n", __FUNCTION__, (int)inIELen);

    get_wifi_softap_interface_name(wifi_name, sizeof(wifi_name));
    printf("[WAC][%s] wlan Intf_name=[%s]\n", __FUNCTION__, wifi_name);

    ret = SetWlanIE(wifi_name, (char *)inIE, (unsigned short)inIELen, (char)WAC_IE_FLAG_INSERT);

    printf("%s InsertWlanIE ret =[ %d ]\n", __FUNCTION__, ret);

#ifdef RA1_SUPPORT
    char ssid[64];
    char buf[256];
    get_wac_ssid(ssid, sizeof(ssid));

    memset((void *)&buf, 0, sizeof(buf));
    sprintf(buf, "iwpriv ra1 set SSID=\"%s\"", ssid);
    system(buf);
    system("ifconfig ra1 10.10.10.253 up");
    udhcpd_start("ra1");
#endif
}

void platform_softap_stop(void)
{
    printf("[WAC]%s\n", __FUNCTION__);
    // do not stop ie here, if config fails, should keep broadcasting  apple device ie beacon
#if 0
    if(save_apple_ie_len > 0)
        SetWlanIE("ra0", (char *)save_apple_ie, save_apple_ie_len, (char)WAC_IE_FLAG_DELETE);
#endif

#ifdef RA1_SUPPORT
    // stop temp softap and dhcpd
    system("ifconfig ra1 down");
    udhcpd_stop();
#endif
}


void platform_get_wac_name(char *name)
{
    char temp[64] = {0};
    wm_db_get("DeviceName", temp);

    if(strlen(temp) == 0)
        strcpy(name, "Your Speaker Name");
    else
        strcpy(name, temp);
}

void platform_get_mac_address(char *mac)
{
    char wlan_name[64] = {0};

    get_wifi_softap_interface_name(wlan_name, sizeof(wlan_name));
    
    if(get_mac_by_ifname((const char*)wlan_name, mac))
        printf("[WAC]%s GET mac failed\n", __FUNCTION__);
}

int platform_is_wac_unconfigured(void)
{
    char temp[64] = {0};
    wm_db_get(NVRAM_WAC_STATUS, temp);

    if(strlen(temp) && !strcmp(temp, WAC_STATUS_CONFIGURED)) {
        printf("[WAC] %s WAC is configured\n", __func__);
        return false;
    }

    return true;
}

static void update_wac_status_indicator(int flag)
{
    printf("[WAC]%s(%d)\n", __FUNCTION__, flag);
#if 0
    if(flag == WAC_FLAG_UNCONFIGURED)
        system("/system/workdir/script/wac_status.sh blink &");
    else if(flag == WAC_FLAG_CONFIGURED)
        system("/system/workdir/script/wac_status.sh on &");
    else // WAC_FLAG_UNCONFIGURED_TIMEOUT
        system("/system/workdir/script/wac_status.sh off &");
#else
    if(flag == WAC_FLAG_UNCONFIGURED)
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=WACMODE:ON", strlen("GNOTIFY=WACMODE:ON"), NULL, 0, 0);
    else
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=WACMODE:OFF", strlen("GNOTIFY=WACMODE:OFF"), NULL, 0, 0);
#endif
}

void platform_update_wac_status(int flag)
{
    printf("[WAC]%s(%d)\n", __FUNCTION__, flag);
// nvram_bufset((int)RT2860_NVRAM, (char *)NVRAM_WAC_STATUS,
//  (flag==WAC_FLAG_CONFIGURED)?(char *)WAC_STATUS_CONFIGURED:(char *)WAC_STATUS_UNCONFIGURED);
// nvram_commit((int)RT2860_NVRAM);

    wm_db_set(NVRAM_WAC_STATUS, (flag == WAC_FLAG_CONFIGURED) ? (char *)WAC_STATUS_CONFIGURED :
              (char *)WAC_STATUS_UNCONFIGURED);

    /* timeout */
    if(flag == 0)
        platform_softap_stop();

    update_wac_status_indicator(flag);

    /* stop broadcast the apple ie only if timeout or configured */
    if(flag == WAC_FLAG_CONFIGURED || flag == WAC_FLAG_UNCONFIGURED_TIMEOUT) {
        char wifi_name[64] = {0};
                
        get_wifi_softap_interface_name(wifi_name, sizeof(wifi_name));
    
        printf("[WAC][%s] wlan Intf_name=[%s]\n", __FUNCTION__, wifi_name);
        
        if(save_apple_ie_len > 0)
            SetWlanIE(wifi_name, (char *)save_apple_ie, save_apple_ie_len, (char)WAC_IE_FLAG_DELETE);

        save_apple_ie_len = 0;

        if(access("/tmp/hide_ssid", R_OK) == 0) {
//   system("iwpriv ra0 set HideSSID=1");
//   system("ifconfig ra0 down");
//   system("ifconfig ra0 up");
            system("echo 1 > /tmp/ssid_is_hidden");
        }
        system("rm -f /tmp/wac_mode");
    } else {
        system("echo 1 > /tmp/wac_mode");
        if(access("/tmp/ssid_is_hidden", R_OK) == 0) {
//   system("iwpriv ra0 set HideSSID=0");
//   system("ifconfig ra0 down");
//   system("ifconfig ra0 up");
            system("rm -f /tmp/ssid_is_hidden");
        }
    }
}


void platform_get_wac_device_info(WACPlatformParameters_t *param)
{
    if(!param)
        return;

    param->hardwareRevision = (char *)WAC_HARDWARE_REVISION;
    param->serialNumber = (char *)WAC_SERIAL_NUMBER;
    param->model = (char *)WAC_MODEL;
    param->firmwareRevision = (char *)WAC_FIRMWARE_REVISION;
    param->manufacturer = (char *)WAC_MANUFACTURER;
    param->preferredAppBundleSeedIdentifier = (char *)WAC_BUNDLE_SEEDID;

    param->supportsAirPlay = WAC_SUPPORTS_AIRPLAY;
    param->supportsAirPrint = WAC_SUPPORTS_AIRPRINT;
    param->supports2_4GHzWiFi = WAC_SUPPORTS2_4G_WIFI;
    param->supports5GHzWiFi = WAC_SUPPORTS5G_WIFI;
    param->supportsWakeOnWireless = WAC_SUPPORTS_WAKE_ON_WIFI;
    param->numSupportedExternalAccessoryProtocols = 0;
    param->supportedExternalAccessoryProtocols = WAC_EA_PROTOCOLS;

    param->isUnconfigured = platform_is_wac_unconfigured(); //
    platform_get_mac_address((char *)&param->macAddress);
    platform_get_wac_name((char *)&param->name);

    printf("[WAC]%s, name[%s]mac[%02X:%02X:%02X:%02X:%02X:%02X]unconfigured=%d\n", __FUNCTION__,
           param->name,
           param->macAddress[0], param->macAddress[1], param->macAddress[2],
           param->macAddress[3], param->macAddress[4], param->macAddress[5],
           param->isUnconfigured);
}


void platform_prepare_airplay_name_and_password(void)
{
    char buf[256];
    char cmd[256];
    char pp[256];

// pp=nvram_bufget((int)RT2860_NVRAM ,(char *)NVRAM_AIRPLAY_PASSWORD);
    memset((void *)&buf, 0, sizeof(buf));
    memset((void *)&cmd, 0, sizeof(cmd));
    memset((void *)&pp, 0, sizeof(pp));

    wm_db_get((char *)NVRAM_AIRPLAY_PASSWORD, pp);

    if(strlen(pp) > 0) {
        strcpy(buf, pp);
        mvUtil_TrimStringRight(buf);
        sprintf(cmd, "echo -n \"%s\" > %s", buf, INFO_PASSWORD_FILE_PATH);
    } else {
        sprintf(cmd, "echo -n \"\" > %s", INFO_PASSWORD_FILE_PATH);
    }
    system(cmd);

    memset((void *)&buf, 0, sizeof(buf));
    memset((void *)&cmd, 0, sizeof(cmd));
    platform_get_wac_name(buf);
    sprintf(cmd, "echo -n \"%s\" > %s", buf, INFO_DEVICENAME_FILE_PATH);
    system(cmd);
}


