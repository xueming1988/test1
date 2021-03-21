#include <stdio.h>
#include <stdlib.h>

#include "wm_util.h"

int notify_auth_done(void)
{
    return (SocketClientReadWriteMsg("/tmp/Requesta01controller", "GNOTIFY=ClovaAuth:0:succeed",
                                     strlen("GNOTIFY=ClovaAuth:0:succeed"), NULL, NULL, 0));
}

int notify_auth_failed(const char *error_message)
{
    char cmd[512] = {0};

    snprintf(cmd, sizeof(cmd), "GNOTIFY=ClovaAuth:1:%s", error_message);
    return (SocketClientReadWriteMsg("/tmp/Requesta01controller", cmd, strlen(cmd), NULL, NULL, 0));
}

int notify_enter_oobe_mode(int network_reset)
{
    char buf[64] = {0};

    if (network_reset != -1)
        snprintf(buf, sizeof(buf), "GUARD_WPS_CLIENT:%d", network_reset);
    else
        snprintf(buf, sizeof(buf), "GUARD_WPS_CLIENT");

    SocketClientReadWriteMsg("/tmp/RequestNetguard", buf, strlen(buf), NULL, NULL, 0);

    return 0;
}

int notify_clear_wifi_history(void)
{
    char buf[64] = {0};

    snprintf(buf, sizeof(buf), "ClearRouterHistory");
    SocketClientReadWriteMsg("/tmp/RequestNetguard", buf, strlen(buf), NULL, NULL, 0);

    return 0;
}

int notify_clear_wifi_history2(void)
{
    char buf[64] = {0};

    snprintf(buf, sizeof(buf), "KeepConnectionClearRouterHistory");
    SocketClientReadWriteMsg("/tmp/RequestNetguard", buf, strlen(buf), NULL, NULL, 0);

    return 0;
}

int notify_disconnect_bt(void)
{
    NotifyMessage(BT_MANAGER_SOCKET, "btdisconnectall");

    return 0;
}

int notify_quit_oobe_mode(void)
{
    SocketClientReadWriteMsg("/tmp/RequestNetguard", "CANCEL_WPS_CLIENT:1",
                             strlen("CANCEL_WPS_CLIENT:1"), NULL, NULL, 0);

    return 0;
}

int notify_do_factory_reset(void)
{
    return (SocketClientReadWriteMsg("/tmp/RequestGoheadCmd", "restoreToDefault",
                                     strlen("restoreToDefault"), NULL, NULL, 0));
}

int notify_switch_keyword_type(int index)
{
    char cmd[256] = {0};

    snprintf(cmd, sizeof(cmd), "talksetKeywordIndex:%d", index);

    SocketClientReadWriteMsg("/tmp/RequestASRTTS", cmd, strlen(cmd), NULL, NULL, 0);

    return 0;
}

int request_start_update(void)
{
    int ret = -1;

    ret = SocketClientReadWriteMsg("/tmp/a01remoteupdate", "NaverForceUpgrade",
                                   strlen("NaverForceUpgrade"), NULL, NULL, 0);

    return ret;
}

int request_update_check(void)
{
    int ret = -1;

    ret = SocketClientReadWriteMsg("/tmp/a01remoteupdate", "NaverForceCheck",
                                   strlen("NaverForceCheck"), NULL, NULL, 0);

    return ret;
}
int notify_internet_back(void)
{
    int ret = -1;

    ret = SocketClientReadWriteMsg("/tmp/a01remoteupdate", "NaverInternetBack",
                                   strlen("NaverInternetBack"), NULL, NULL, 0);

    return ret;
}

int notify_ota_state(int state)
{
    int ret = -1;

    if (state) {
        ret = SocketClientReadWriteMsg("/tmp/request_common_rpc", "GNOTIFY=UPGRADE_UPDATE_SUCCESS",
                                       strlen("GNOTIFY=UPGRADE_UPDATE_SUCCESS"), NULL, NULL, 0);
    } else {
        ret = SocketClientReadWriteMsg("/tmp/request_common_rpc", "GNOTIFY=UPGRADE_UPDATE_FAILED",
                                       strlen("GNOTIFY=UPGRADE_UPDATE_FAILED"), NULL, NULL, 0);
    }

    return ret;
}

int send_json_message(const char *message)
{
    int ret = -1;

    if (message) {
        ret = SocketClientReadWriteMsg("/tmp/request_common_rpc", message, strlen(message), NULL,
                                       NULL, 0);
    }

    return ret;
}

int notify_update_new_token(const char *new_token)
{
    int ret = -1;
    char message[128];

    if (new_token) {
        snprintf(message, sizeof(message), "NaverUpdateToken:%s", new_token);
        ret = SocketClientReadWriteMsg("/tmp/a01remoteupdate", message, strlen(message), NULL, NULL,
                                       0);
    }

    return ret;
}