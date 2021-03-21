#ifndef __NOTIFY_AVS_H__
#define __NOTIFY_AVS_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "wm_util.h"
#include "SipUserAgentInterface.h"

#ifdef __cplusplus
extern "C" {
#endif

int notify_avs_sua_state_updated(const SipUserAgentState previousSipUserAgentState,
                                 const SipUserAgentState currentSipUserAgentState, const char *url);

int notify_avs_sua_send_events(const char *event_name, const char *payload);

int notify_avs_sua_get_state_cxt(SOCKET_CONTEXT *msg_socket, const char *comms_state);

int notify_avs_sua_channel_focus(int channel_on);

#ifdef __cplusplus
}
#endif

#endif
