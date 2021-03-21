#include <stdio.h>
#include <string.h>
#include "notify_avs.h"

#define AVS_CLIENT ("/tmp/ALEXA_EVENT")

#define Key_Event ("event")
#define Key_Event_type ("event_type")
#define Key_Event_params ("params")

#define Type_SipPhone ("sip_phone")

#define Event_State_Updated ("state_updated")
#define Event_Start_Ring ("start_ring")
#define Event_Stop_Ring ("stop_ring")
#define Event_Show_Loghts ("show_lights")
#define Event_Send_Events ("send_events")
#define Event_Get_State_Cxt ("Get_State_Cxt")
#define Event_Channel_Focus ("channel_focus")

#define Check_String(str) (((char *)str) ? (str) : "")

#define Check_Obj(str) (((char *)str) ? (str) : "{}")

#define Check_Bool(bool_val) ((bool_val) ? "true" : "false")

#define Create_State_Updated_Fmt                                                                   \
    ("{"                                                                                           \
     "\"event_type\":\"%s\","                                                                      \
     "\"event\":\"%s\","                                                                           \
     "\"params\":{"                                                                                \
     "\"pre_state\":%d,"                                                                           \
     "\"cur_state\":%d,"                                                                           \
     "\"url\":\"%s\""                                                                              \
     "}"                                                                                           \
     "}")

#define Create_Send_Events_Fmt                                                                     \
    ("{"                                                                                           \
     "\"event_type\":\"%s\","                                                                      \
     "\"event\":\"%s\","                                                                           \
     "\"params\":{"                                                                                \
     "\"event_name\":\"%s\","                                                                      \
     "\"payload\":%s"                                                                              \
     "}"                                                                                           \
     "}")

#define Create_Update_State_Fmt                                                                    \
    ("{"                                                                                           \
     "\"event_type\":\"%s\","                                                                      \
     "\"event\":\"%s\","                                                                           \
     "\"params\":{"                                                                                \
     "\"state\":%s"                                                                                \
     "}"                                                                                           \
     "}")

#define Create_Channel_Focus_Fmt                                                                   \
    ("{"                                                                                           \
     "\"event_type\":\"%s\","                                                                      \
     "\"event\":\"%s\","                                                                           \
     "\"params\":{"                                                                                \
     "\"channel_on\":%s"                                                                           \
     "}"                                                                                           \
     "}")

int notify_avs_sua_state_updated(const SipUserAgentState previousSipUserAgentState,
                                 const SipUserAgentState currentSipUserAgentState, const char *url)
{
    char *buf = NULL;

    asprintf(&buf, Create_State_Updated_Fmt, Type_SipPhone, Event_State_Updated,
             previousSipUserAgentState, currentSipUserAgentState, Check_String(url));
    if (buf) {
        SocketClientReadWriteMsg(AVS_CLIENT, (char *)buf, strlen(buf), NULL, NULL, 0);
        free(buf);
        return 0;
    }

    return -1;
}

int notify_avs_sua_send_events(const char *event_name, const char *payload)
{
    char *buf = NULL;

    asprintf(&buf, Create_Send_Events_Fmt, Type_SipPhone, Event_Send_Events,
             Check_String(event_name), Check_Obj(payload));
    if (buf) {
        SocketClientReadWriteMsg(AVS_CLIENT, (char *)buf, strlen(buf), NULL, NULL, 0);
        free(buf);
        return 0;
    }

    return -1;
}

int notify_avs_sua_get_state_cxt(SOCKET_CONTEXT *msg_socket, const char *comms_state)
{
    return UnixSocketServer_write(msg_socket, (char *)comms_state, strlen(comms_state));
}

int notify_avs_sua_channel_focus(int channel_on)
{
    char *buf = NULL;

    asprintf(&buf, Create_Channel_Focus_Fmt, Type_SipPhone, Event_Channel_Focus,
             Check_Bool(channel_on));
    if (buf) {
        SocketClientReadWriteMsg(AVS_CLIENT, (char *)buf, strlen(buf), NULL, NULL, 0);
        free(buf);
        return 0;
    }

    return -1;
}
