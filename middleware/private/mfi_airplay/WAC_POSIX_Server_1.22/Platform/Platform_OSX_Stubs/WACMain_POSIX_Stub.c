/*
    File:    WACMain_POSIX_Stub.c
    Package: WACServer
    Version: WACServer-1.14

    Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
    capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
    Apple software is governed by and subject to the terms and conditions of your MFi License,
    including, but not limited to, the restrictions specified in the provision entitled ?ùPublic
    Software?? and is further subject to your agreement to the following additional terms, and your
    agreement that the use, installation, modification or redistribution of this Apple software
    constitutes acceptance of these additional terms. If you do not agree with these additional terms,
    please do not use, install, modify or redistribute this Apple software.

    Subject to all of these terms and in?consideration of your agreement to abide by them, Apple grants
    you, for as long as you are a current and in good-standing MFi Licensee, a personal, non-exclusive
    license, under Apple's copyrights in this original Apple software (the "Apple Software"), to use,
    reproduce, and modify the Apple Software in source form, and to use, reproduce, modify, and
    redistribute the Apple Software, with or without modifications, in binary form. While you may not
    redistribute the Apple Software in source form, should you redistribute the Apple Software in binary
    form, you must retain this notice and the following text and disclaimers in all such redistributions
    of the Apple Software. Neither the name, trademarks, service marks, or logos of Apple Inc. may be
    used to endorse or promote products derived from the Apple Software without specific prior written
    permission from Apple. Except as expressly stated in this notice, no other rights or licenses,
    express or implied, are granted by Apple herein, including but not limited to any patent rights that
    may be infringed by your derivative works or by other works in which the Apple Software may be
    incorporated.

    Unless you explicitly state otherwise, if you provide any ideas, suggestions, recommendations, bug
    fixes or enhancements to Apple in connection with this software (?úFeedback??, you hereby grant to
    Apple a non-exclusive, fully paid-up, perpetual, irrevocable, worldwide license to make, use,
    reproduce, incorporate, modify, display, perform, sell, make or have made derivative works of,
    distribute (directly or indirectly) and sublicense, such Feedback in connection with Apple products
    and services. Providing this Feedback is voluntary, but if you do provide Feedback to Apple, you
    acknowledge and agree that Apple may exercise the license granted above without the payment of
    royalties or further consideration to Participant.

    The Apple Software is provided by Apple on an "AS IS" basis. APPLE MAKES NO WARRANTIES, EXPRESS OR
    IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY
    AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR
    IN COMBINATION WITH YOUR PRODUCTS.

    IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION
    AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
    (INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

    Copyright (C) 2013 Apple Inc. All Rights Reserved.
*/
#include <unistd.h>
#include <signal.h>
#include <unistd.h>
#include "WACInterface.h"
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <sys/un.h>

#include "Common.h"
#include "Debug.h"
#include "PlatformLogging.h"
#include "WACServerAPI.h"
#include "wm_util.h"

#define WACMsgSockPath    "/tmp/RequestWACServer"

#define MAX_RETRY_START_COUNT   (10)
static bool g_stopWACServer;
static WACContext_t g_inContext;
static WACPlatformParameters_t myParam;
static WACCallbackMessage_t g_WacSetupStatus = WACCallbackMessage_Initializing;
static long ts_since_config_received = 0;
bool gAirplayRecvdConfiguredMsg = false;

bool gWacIsWaitingForConfiguredMsg = false;

extern bool _gWACBonjourServRegistered;
extern OSStatus RemoveWACBonjourService( WACContext_t * const inContext );

#define IS_WAC_MODE_TIMEOUT(X)  (tickSincePowerOn() >= (X + (WAC_STANDARD_SETUP_TIMEOUT_SECONDS * 1000)))
#define IS_WAC_WIFI_CONNECTING_TIMEOUT(X)  (tickSincePowerOn() >= (X + (WAC_SERVER_CONNECT_WIFI_TIMEOUT * 1000)))

int WacSetUpInternalSockMsg(SOCKET_CONTEXT *ret_msg_socket);
static void SetWacConfigStatus(char *status);

static inline char * WacStateToString(WACCallbackMessage_t status)
{
    char *p = NULL;

    switch (status)
    {
        case WACCallbackMessage_Initializing:
            p = "Initializing";
            break;

        case WACCallbackMessage_Ready:
            p = "Ready";
            break;

        case WACCallbackMessage_ConfigStart:
            p = "ConfigStart";
            break;

        case WACCallbackMessage_ConfigReceived:
            p = "ConfigReceived";
            break;

        case WACCallbackMessage_ConfigComplete:
            p = "ConfigComplete";
            break;

        case WACCallbackMessage_Stopped:
            p = "Stopped";
            break;

        case WACCallbackMessage_Error:
            p = "Error";
            break;

        default:
            p = "Unknown";
            break;
    }

    return p;
}


static void updateWACState(int state)
{
    g_WacSetupStatus = state;
    wiimu_log(1, 0, 0, 0, "[WAC-Server : updateWACState] Current status [ %d ] [ %s ] \n", g_WacSetupStatus, WacStateToString(g_WacSetupStatus));
}

static void WACCallbackFunc(WACContext_t *wac, WACCallbackMessage_t callbackMsg)
{
    wiimu_log(1, 0, 0, 0, "[WACCallbackFunc]: begin, callback=%d \n", callbackMsg);

    if (wac->shouldStop) 
    {
        g_stopWACServer = true;
        wiimu_log(1, 0, 0, 0, "[WACCallbackFunc]: should stop !! \n");
    }

    switch(callbackMsg) {
        case WACCallbackMessage_Initializing:
            updateWACState(WAC_SERVER_STATE_INIT);
            break;

        case WACCallbackMessage_Ready:
            updateWACState(WAC_SERVER_STATE_READY);
            break;

        case WACCallbackMessage_ConfigStart:
            updateWACState(WAC_SERVER_STATE_CONFIG_START);
            break;

        case WACCallbackMessage_ConfigReceived:
            ts_since_config_received = tickSincePowerOn();
            updateWACState(WAC_SERVER_STATE_CONFIG_RECEVIED);
            break;

        case WACCallbackMessage_ConfigComplete:
            updateWACState(WAC_SERVER_STATE_CONFIG_COMPLETE);
            myParam.isUnconfigured = 0;
            break;

        case WACCallbackMessage_Stopped:
            updateWACState(WAC_SERVER_STATE_STOPPED);
            g_stopWACServer = true;
            break;

        case WACCallbackMessage_Error:
            updateWACState(WAC_SERVER_STATE_ERROR);
            break;

        default :
            break;
    }
    //printf("[WACCallbackFunc]: end \n");
}

static void wakeup_http(WACContext_t *const inContext)
{
    sem_post(&inContext->httpHeaderReadSemaphore);
}


static OSStatus query_wac_network_setup_status(void)
{
    OSStatus status = kUnknownErr;

    if (WACCallbackMessage_ConfigReceived == g_WacSetupStatus)
    {
        char response[64] = {0};
        int ret_len = 0;
        
        memset(response, 0, sizeof(response));
        SocketClientReadWriteMsg("/tmp/RequestNetguard", "GetWlanConnectState", strlen("GetWlanConnectState"), response, &ret_len, 0);

        if (ret_len > 0)
        {
            response[ret_len] = '\0';
            
            if (!strcmp("OK", response)) 
            {
                status = kNoErr;

                /*
                ** If we don't de-register wac bonjour service, IOS device may think device is still under wait for config status.
                ** Then IOS device will re-send us auth/config msg etc.
                ** de-register it once network connected so that we will not recv any wac setup msg in new network 
                */
                if (_gWACBonjourServRegistered && kNoErr == RemoveWACBonjourService(&g_inContext))
                {
                    wiimu_log(1, 0, 0, 0, "[WAC-Server] Removed wac bonjour service !!\n");
                    _gWACBonjourServRegistered = false;
                }

                if (true == myParam.isUnconfigured)
                {
                    myParam.isUnconfigured = false;
                    SetWacConfigStatus(WAC_STATUS_CONFIGURED);
#if 0  /* notify msg in netguard */                  
                    //Tell airplay ready to go
                    SocketClientReadWriteMsg("/tmp/RequestAirplay", WAC_MSG_AIRPLAY_READY_TO_GO_MSG, strlen(WAC_MSG_AIRPLAY_READY_TO_GO_MSG), NULL, NULL, 0);
#endif
                }
            }
        }

        if (status != kNoErr && IS_WAC_WIFI_CONNECTING_TIMEOUT(ts_since_config_received))
        {
            //reset ts to prevent log flooding
            ts_since_config_received = tickSincePowerOn();
            
            //Connect wifi time out 
            status = kTimeoutErr;
        }
    }

    return status;
}


static int determine_wac_server_stop(long time_since_in_wac_mode)
{
    if (kTimeoutErr == query_wac_network_setup_status())
    {
        wiimu_log(1, 0, 0, 0, "[WAC-Server] Connecting WiFi network time out  !!\n");
    }

    if (g_stopWACServer || g_inContext.shouldStop) 
    {        
        wiimu_log(1, 0, 0, 0, "[WAC-Server] get stop [%d] [%d] ! \n",g_stopWACServer, g_inContext.shouldStop);

        char wac_cfg_status[32] = {0};
        wm_db_get(NVRAM_WAC_STATUS, wac_cfg_status);

        if (0 == strcmp(wac_cfg_status, WAC_STATUS_CONFIGURED)) 
        {
            myParam.isUnconfigured = false;
        }
        else 
        {
            myParam.isUnconfigured = true;
        }

        return true;
    }

    if (IS_WAC_MODE_TIMEOUT(time_since_in_wac_mode)) 
    {
        wiimu_log(1, 0, 0, 0, "[WAC-Server] Have been in WAC status for %d seconds, Exit WAC mode ! \n", (WAC_STANDARD_SETUP_TIMEOUT_SECONDS));
        return true;
    }

    return false;
}

int WacSetUpInternalSockMsg(SOCKET_CONTEXT *ret_msg_socket)
{
    int ret = -1;
    SOCKET_CONTEXT msg_socket;
    msg_socket.path = strdup(WACMsgSockPath);
    msg_socket.blocking = 1;

    ret = UnixSocketServer_Init(&msg_socket);

    if(ret <= 0) 
    {
        wiimu_log(1, 0, 0, 0, "WAC socket client init Failed !!==>(err %d,%s)\n", errno, strerror(errno));
        return -1;
    }

    memcpy(ret_msg_socket, &msg_socket, sizeof(msg_socket));
    
    return 0;
}

static int WacHandleInternalMsg(SOCKET_CONTEXT *msg_socket)
{
    int ret = 0;
    int recv_num = 0;
    char recv_buf[1024];
    socklen_t t;
    struct sockaddr_un remote;

    if(msg_socket == NULL)
        return -1;

    t = sizeof(struct sockaddr_un);
    if ((msg_socket->sock[1] = accept(msg_socket->sock[0], (struct sockaddr *) & remote, &t)) == -1)
    {
        wiimu_log(1, 0, 0, 0, "UnixSocketServer_accept failed!!==>(err %d,%s)\n", errno, strerror(errno));
        return -1;
    }

    memset(recv_buf, 0, sizeof(recv_buf));

    recv_num = UnixSocketServer_read(msg_socket, recv_buf, sizeof(recv_buf));

    if (recv_num <= 0) 
    {
        wiimu_log(1, 0, 0, 0, "%s msg recv failed!!==>(err %d,%s)\n", errno, strerror(errno));
        UnixSocketServer_close(msg_socket);
        return -1;
    }

    recv_buf[recv_num] = '\0';

    wiimu_log(1, 0, 0, 0, "Wac Server recvd msg [%s]", recv_buf);
        
    if (0 == strncmp(recv_buf, WAC_MSG_AIRPLAY_RECVED_CONFIGURED, strlen(WAC_MSG_AIRPLAY_RECVED_CONFIGURED)))
    {                    
        gAirplayRecvdConfiguredMsg = true;

        wiimu_log(1, 0, 0, 0, "[WAC-Server] Airplay received /confgiured Msg !!\n");

        if (gWacIsWaitingForConfiguredMsg) 
        {
            wakeup_http(&g_inContext);
        } 
        else
        {
            wiimu_log(1, 0, 0, 0, "[WAC-Server] Wac is not ready for configed message yet !!\n");
        }
    }
    else if (0 == strncmp(recv_buf, WAC_MSG_STOP_SERVER, strlen(WAC_MSG_STOP_SERVER)))
    {
        g_stopWACServer = true;
    }
    else
    {
        wiimu_log(1, 0, 0, 0, "unhandled WAC internal msg [%s]", recv_buf);
    }       

    UnixSocketServer_close(msg_socket);

    return ret;
}

static OSStatus StartWACServer(void)
{
    int retryStart = 0;
    OSStatus status = kNoErr;

    status = WACServerStart(&g_inContext, (void *)WACCallbackFunc);

restart:
    
    if (status != kNoErr && retryStart < MAX_RETRY_START_COUNT) 
    {
        wiimu_log(1, 0, 0, 0, "[WAC-Server] retry start WAC server before sleep 1 sec! \n");
        
        retryStart ++;
        
        sleep(1);
        
        status = WACServerStart(&g_inContext, (void *)WACCallbackFunc);

        if (status != kNoErr)
        {
            goto restart;
        }
    }

    return status;
}


static void SetWacConfigStatus(char *status)
{
    wm_db_set(NVRAM_WAC_STATUS, status);
}

OSStatus main(int argc, char **argv)
{    
    (void)argc;
    (void)argv;
    
    // PLATFORM_TO_DO

    fd_set readfds;
    struct timeval timeout;
    long ts_since_in_wac_mode = 0;

    SOCKET_CONTEXT msg_socket;

    plat_log_trace();
    
    wiimu_log(1, 0, 0, 0, "[WAC-Server]============= WAC main() ===== start here!!\n\n");

    if (0 != WacSetUpInternalSockMsg(&msg_socket))
    {
        wiimu_log(1, 0, 0, 0, "WAC fatal error, WacSetUpInternalSockMsg failed!!!");
        return -1;
    }

    //Reset wac status to unconfigured
    SetWacConfigStatus(WAC_STATUS_UNCONFIGURED);
    
    platform_get_wac_device_info((WACPlatformParameters_t *)&myParam);
    
    g_inContext.platformParams = (WACPlatformParameters_t *)&myParam;
    
    updateWACState(WAC_SERVER_STATE_NONE);
    
    platform_prepare_airplay_name_and_password();

#if 0
    if (false == myParam.isUnconfigured) {
        
        printf("[WAC] WAC is configured, should not enter wac mode unless doing factory reset operation\n");
        
        goto out;
    }
#endif
    
    if (kNoErr != StartWACServer()) 
    {
        wiimu_log(1, 0, 0, 0, "Start WAC Server error");
        goto out;
    }
    
    platform_update_wac_status(WAC_FLAG_UNCONFIGURED);

    const int timeout_ms_per_loop = 10; /*10ms*/
    timeout.tv_sec = 0;
    timeout.tv_usec = timeout_ms_per_loop * 1000;
    
    ts_since_in_wac_mode = tickSincePowerOn();
    
    while(1) 
    {
        FD_ZERO(&readfds);
        FD_SET(msg_socket.sock[0], &readfds);/* watch wac msg socket */

        int max_fd = msg_socket.sock[0];
        timeout.tv_sec = 0;
        timeout.tv_usec = timeout_ms_per_loop * 1000;
        int ret = select(max_fd + 1, &readfds, NULL, NULL, &timeout);
        if (ret < 0) 
        {
            if(errno != EINTR)
                wiimu_log(1, 0, 0, 0, "Select msg error on WAC server\r\n");

            //do not quit
            continue;
        }

        /* Got socket msg */
        if (ret > 0)
        {
            if (FD_ISSET(msg_socket.sock[0], &readfds))
            {
                WacHandleInternalMsg(&msg_socket);
            }
        }
        else if (ret == 0)/* Timeout */ 
        {
            if (true == determine_wac_server_stop(ts_since_in_wac_mode))
            {
                goto out;
            }
        }
    }
    
out:
    
    WACServerStop(&g_inContext);

    //update wac cfg status
    platform_update_wac_status(myParam.isUnconfigured ? WAC_FLAG_UNCONFIGURED_TIMEOUT : WAC_FLAG_CONFIGURED);

    //Tell airplay ready to go
    SocketClientReadWriteMsg("/tmp/RequestAirplay", WAC_MSG_AIRPLAY_READY_TO_GO_MSG, strlen(WAC_MSG_AIRPLAY_READY_TO_GO_MSG), NULL, NULL, 0);

    sleep(3);

    //Start airplay bonjour if not
    SocketClientReadWriteMsg("/tmp/RequestAirplay", "startAirplay", strlen("startAirplay"), NULL, NULL, 0);

    //Update airplay service per WAC result
    SocketClientReadWriteMsg("/tmp/RequestAirplay", "updateConfig", strlen("updateConfig"), NULL, NULL, 0);
    
    UnixSocketServer_deInit(&msg_socket);
        
    printf("[WAC-Server]============= WAC main() ===== stop! \n\n");
    
    return kNoErr;
}

