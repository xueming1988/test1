#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include "wm_util.h"
#include "alexa_msg.h"
#include "alexa_debug.h"

extern WIIMU_CONTEXT *g_wiimu_shm;

int alexa_notify_msg(ALEXA_NOTIFY_MSG_E MsgType)
{
    if (!SUPPORT_EUFY_NETWORK_SETUP) {
        return 0;
    }

    switch (MsgType) {
    case ALEXA_NOTIFY_MSG_NEED_AUTH: {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=AlexaErr_NeedAuth",
                                 strlen("GNOTIFY=AlexaErr_NeedAuth"), NULL, NULL, 0);
        SocketClientReadWriteMsg("/tmp/RequestNetguard", "GNOTIFY=AlexaErr_NeedAuth",
                                 strlen("GNOTIFY=AlexaErr_NeedAuth"), NULL, NULL, 0);
    } break;

    case ALEXA_NOTIFY_MSG_READY: {
        // SocketClientReadWriteMsg("/tmp/RequestIOGuard","GNOTIFY=AlexaIsReady",
        // strlen("GNOTIFY=AlexaIsReady"),NULL,NULL,0);
        SocketClientReadWriteMsg("/tmp/RequestNetguard", "GNOTIFY=AlexaIsReady",
                                 strlen("GNOTIFY=AlexaIsReady"), NULL, NULL, 0);
    } break;

    case ALEXA_NOTIFY_MSG_CONNECT_INTERNET_ERROR: {
        SocketClientReadWriteMsg("/tmp/RequestNetguard", "GNOTIFY=AlexaConnectInternetErr",
                                 strlen("GNOTIFY=AlexaConnectInternetErr"), NULL, NULL, 0);
    } break;
    case ALEXA_NOTIFY_MSG_SET_TOKEN: {
        SocketClientReadWriteMsg("/tmp/RequestNetguard", "GNOTIFY=AlexaSetToken",
                                 strlen("GNOTIFY=AlexaSetToken"), NULL, NULL, 0);
    }

    default:
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "Unknown MsgType=[%d]\n", MsgType);
        break;
    }

    return 0;
}
