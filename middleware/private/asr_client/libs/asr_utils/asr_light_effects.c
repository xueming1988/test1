
#include <stdio.h>
#include <string.h>

#include "asr_light_effects.h"
#include "wm_util.h"

int asr_light_effects_set(e_effect_type_t type)
{
    switch (type) {
    case e_effect_idle: {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDIDLE",
                                 strlen("GNOTIFY=MCULEDIDLE"), NULL, NULL, 0);
    } break;
    case e_effect_listening: {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDLISTENING",
                                 strlen("GNOTIFY=MCULEDLISTENING"), NULL, NULL, 0);
    } break;
    case e_effect_listening_off: {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDUNLISTENING",
                                 strlen("GNOTIFY=MCULEDUNLISTENING"), NULL, NULL, 0);
    } break;
    case e_effect_thinking: {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDTHINKING",
                                 strlen("GNOTIFY=MCULEDTHINKING"), NULL, NULL, 0);
    } break;
    case e_effect_unthinking: {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDUNTHINKING",
                                 strlen("GNOTIFY=MCULEDUNTHINKING"), NULL, NULL, 0);
    } break;
    case e_effect_speeking: {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDPLAYTTS",
                                 strlen("GNOTIFY=MCULEDPLAYTTS"), NULL, NULL, 0);
    } break;
    case e_effect_speeking_off: {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDUNPLAYTTS",
                                 strlen("GNOTIFY=MCULEDUNPLAYTTS"), NULL, NULL, 0);
    } break;
    case e_effect_error: {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDRECOERROR",
                                 strlen("GNOTIFY=MCULEDRECOERROR"), NULL, NULL, 0);
    } break;
    case e_effect_notify_on: {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDENNOTI",
                                 strlen("GNOTIFY=MCULEDENNOTI"), NULL, NULL, 0);
    } break;
    case e_effect_notify_off: {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDDISNOTI",
                                 strlen("GNOTIFY=MCULEDDISNOTI"), NULL, NULL, 0);
    } break;
    case e_effect_message_on: {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDMESSAGEON",
                                 strlen("GNOTIFY=MCULEDMESSAGEON"), NULL, NULL, 0);
    } break;
    case e_effect_message_off: {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDMESSAGEOFF",
                                 strlen("GNOTIFY=MCULEDMESSAGEOFF"), NULL, NULL, 0);
    } break;
    case e_effect_ota_on: {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=OTA_SUCCESS",
                                 strlen("GNOTIFY=OTA_SUCCESS"), NULL, NULL, 0);
    } break;
    case e_effect_ota_off: {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDCLOSEOTA",
                                 strlen("GNOTIFY=MCULEDCLOSEOTA"), NULL, NULL, 0);
    } break;
    case e_effect_music_play: {
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=LEDMUSICPLAY",
                                 strlen("GNOTIFY=LEDMUSICPLAY"), NULL, NULL, 0);
    } break;
    default:
        break;
    }

    return 0;
}
