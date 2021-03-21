#include <stdio.h>
#include <stdlib.h>

#include "json.h"
#include "asr_cmd.h"
#include "asr_player.h"
#include "asr_json_common.h"
#include "asr_light_effects.h"

#include "clova_audio_player.h"
#include "clova_playback_controller.h"

static int clova_parse_repeat_mode(json_object *json_payload, unsigned long long request_id)
{
    int ret = -1;
    char *repeatMode = NULL;

    if (json_payload == NULL) {
        goto EXIT;
    }

    repeatMode = JSON_GET_STRING_FROM_OBJECT(json_payload, CLOVA_PAYLOAD_NAME_REPEATMODE);
    if (repeatMode == NULL) {
        goto EXIT;
    }

    ret = asr_player_set_repeat_mode(repeatMode, request_id);

EXIT:

    return ret;
}

static int clova_parse_expect_play(json_object *json_payload)
{
    int ret = -1;
    char *obj = NULL;

    if (json_payload == NULL) {
        goto EXIT;
    }

    obj = json_object_to_json_string(json_payload);
    ret = cmd_add_normal_event(NAMESPACE_PLAYBACKCTROLLER, NAME_PLAYCOMMANDISSUED, Val_Obj(obj));

EXIT:

    return ret;
}

int clova_parse_playback_controller_directive(json_object *json_directive,
                                              unsigned long long request_id)
{
    int ret = -1;
    char *name = NULL;
    json_object *json_header = NULL;
    json_object *json_payload = NULL;

    if (json_directive == NULL) {
        goto EXIT;
    }

    json_header = json_object_object_get(json_directive, KEY_HEADER);
    if (json_header == NULL) {
        goto EXIT;
    }

    json_payload = json_object_object_get(json_directive, KEY_PAYLOAD);
    if (json_payload == NULL) {
        goto EXIT;
    }

    name = JSON_GET_STRING_FROM_OBJECT(json_header, KEY_NAME);

    if (StrEq(name, CLOVA_HEADER_NAME_SETREPEATMODE)) {
        ret = clova_parse_repeat_mode(json_payload, request_id);
    } else if (StrEq(name, CLOVA_HEADER_NAME_PAUSE)) {
        ret = asr_player_set_pause();
    } else if (StrEq(name, CLOVA_HEADER_NAME_RESUME)) {
        ret = asr_player_set_resume();
    } else if (StrEq(name, CLOVA_HEADER_NAME_STOP)) {
        ret = asr_player_set_stop(request_id);
    } else if (StrEq(name, CLOVA_HEADER_NAME_NEXT)) {
        ret = asr_player_set_next();
    } else if (StrEq(name, CLOVA_HEADER_NAME_PREVIOUS)) {
        ret = asr_player_set_previous();
    } else if (StrEq(name, CLOVA_HEADER_NAME_EXPECT_NEXT_COMMAND)) {
        ret = cmd_add_playback_event(NAME_NEXTCOMMANDISSUED);
        if (ret == 0) {
            asr_light_effects_set(e_effect_music_play);
        }
    } else if (StrEq(name, CLOVA_HEADER_NAME_EXPECT_PREVIOUS_COMMAND)) {
        ret = cmd_add_playback_event(NAME_PREVIOUSCOMMANDISSUED);
        if (ret == 0) {
            asr_light_effects_set(e_effect_music_play);
        }
    } else if (StrEq(name, CLOVA_HEADER_NAME_EXPECT_PLAY_COMMAND)) {
        ret = clova_parse_expect_play(json_payload);
        if (ret == 0) {
            asr_light_effects_set(e_effect_music_play);
        }
    } else if (StrEq(name, CLOVA_HEADER_NAME_EXPECT_PAUSE_COMMAND)) {
        ret = cmd_add_playback_event(NAME_PAUSECOMMANDISSUED);
    } else if (StrEq(name, CLOVA_HEADER_NAME_EXPECT_RESUME_COMMAND)) {
        if (get_asr_talk_start()) {
            int session_state = get_session_state();
            if (session_state == 1)
                NOTIFY_CMD(ASR_EVENT_PIPE, ASR_RECO_STOP);
#if defined(LGUPLUS_IPTV_ENABLE)
            else if (!session_state)
                iptv_cancel_recording();
#endif
        }
        ret = cmd_add_playback_event(CLOVA_HEADER_NAME_RESUME_COMMAND_ISSUED);
    } else if (StrEq(name, CLOVA_HEADER_NAME_EXPECT_STOP_COMMAND)) {
        ret = cmd_add_playback_event(CLOVA_HEADER_NAME_STOP_COMMAND_ISSUED);
    } else if (StrEq(name, CLOVA_HEADER_NAME_REPLAY)) {
        ret = asr_player_set_replay();
    } else if (StrEq(name, CLOVA_HEADER_NAME_TURN_OFF_REPEAT_MODE)) {
        // Deprecated
    } else if (StrEq(name, CLOVA_HEADER_NAME_TURN_ON_REPEAT_MODE)) {
        // Deprecated
    } else if (StrEq(name, CLOVA_HEADER_NAME_MUTE)) {
        ret = asr_player_set_volume(1, 1);
    } else if (StrEq(name, CLOVA_HEADER_NAME_UNMUTE)) {
        ret = asr_player_set_volume(1, 0);
    } else if (StrEq(name, CLOVA_HEADER_NAME_VOLUME_DOWN)) {
        ret = asr_player_adjust_volume(-1);
    } else if (StrEq(name, CLOVA_HEADER_NAME_VOLUME_UP)) {
        ret = asr_player_adjust_volume(1);
    }

EXIT:

    return ret;
}
