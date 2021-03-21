/*************************************************************************
    > File Name: url_player.c
    > Author:
    > Mail:
    > Created Time: Tue 21 Nov 2017 02:35:49 AM EST
 ************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "cmd_q.h"

#include "asr_cmd.h"
#include "asr_player.h"

#include "asr_debug.h"
#include "asr_bluetooth.h"
#include "asr_json_common.h"
#include "asr_notification.h"
#include "asr_request_json.h"
#include "asr_light_effects.h"

#include "clova_event.h"
#include "clova_clova.h"
#include "clova_audio_player.h"
#include "cid_player_naver.h"
#include "url_player.h"

#include "lguplus_json_common.h"
#include "lguplus_event_queue.h"

#ifndef VOLUME_VALUE_OF_INIT
#define VOLUME_VALUE_OF_INIT 10
#endif

#define os_calloc(n, size) calloc((n), (size))

#define os_free(ptr) ((ptr != NULL) ? free(ptr) : (void)ptr)

#define os_strdup(ptr) ((ptr != NULL) ? strdup(ptr) : NULL)

#define os_power_on_tick() tickSincePowerOn()

#define PLAYER_MANAGER_DEBUG(fmt, ...)                                                             \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d][Player Manager] " fmt,                              \
                (int)(os_power_on_tick() / 3600000), (int)(os_power_on_tick() % 3600000 / 60000),  \
                (int)(os_power_on_tick() % 60000 / 1000), (int)(os_power_on_tick() % 1000),        \
                ##__VA_ARGS__);                                                                    \
    } while (0)

typedef struct _asr_player_manager_s {
    int is_continue;
    int client_type;
    char *cur_speech_token;
    char *dialog_id;
    int cur_speech_state;
    pthread_mutex_t state_lock;

    url_player_t *url_player;
    cid_player_t *cid_player;
} asr_player_manager_t;

static asr_player_manager_t *g_player_manager = NULL;

static void asr_player_focus_manage(void)
{
    if (clova_get_extension_state()) {
        clova_clear_extension_state();
        asr_player_buffer_stop(NULL, e_asr_stop);
    }
    if (get_asr_talk_start()) {
        NOTIFY_CMD(ASR_EVENT_PIPE, ASR_RECO_STOP);
    }
    asr_bluetooth_disconnect(0);
}

int clova_url_notify_cb(e_url_notify_t notify, url_stream_t *url_stream, void *ctx)
{
    int ret = 0;
    asr_player_manager_t *player_manager = (asr_player_manager_t *)ctx;
    PLAYER_MANAGER_DEBUG("--------notify=%d cur_pos=%lld token=%s--------\n", notify,
                         url_stream->cur_pos, url_stream->stream_id);

    if (!url_stream || !player_manager) {
        return -1;
    }

    switch (notify) {
    case e_notify_play_started: {
        asr_player_focus_manage();
        ret = clova_audio_general_cmd(CLOVA_HEADER_NAME_PLAY_STARTED, url_stream->stream_id,
                                      (long)url_stream->cur_pos);
    } break;
    case e_notify_play_stopped: {
        ret = clova_audio_general_cmd(CLOVA_HEADER_NAME_PLAY_STOPPED, url_stream->stream_id,
                                      (long)url_stream->cur_pos);
    } break;
    case e_notify_play_paused: {
        ret = clova_audio_general_cmd(CLOVA_HEADER_NAME_PLAY_PAUSED, url_stream->stream_id,
                                      (long)url_stream->cur_pos);
    } break;
    case e_notify_play_resumed: {
        asr_player_focus_manage();
        ret = clova_audio_general_cmd(CLOVA_HEADER_NAME_PLAY_RESUMED, url_stream->stream_id,
                                      (long)url_stream->cur_pos);
    } break;
    case e_notify_play_near_finished: {

    } break;
    case e_notify_play_finished: {
        ret = clova_audio_general_cmd(CLOVA_HEADER_NAME_PLAY_FINISHED, url_stream->stream_id,
                                      (long)url_stream->cur_pos);
    } break;
    case e_notify_play_error: {
        // Clova doesnot have playbackfailed event, so try to send the playbackfinished event
        ret = clova_audio_general_cmd(CLOVA_HEADER_NAME_PLAY_FINISHED, url_stream->stream_id,
                                      (long)url_stream->cur_pos);
    } break;
    case e_notify_play_delay_passed: {
        ret = clova_audio_general_cmd(CLOVA_HEADER_NAME_PROGRESS_REPORT_DELAY_PASSED,
                                      url_stream->stream_id, (long)url_stream->cur_pos);
    } break;
    case e_notify_play_interval_passed: {
        ret = clova_audio_general_cmd(CLOVA_HEADER_NAME_PROGRESS_REPORT_INTERVAL_PASSED,
                                      url_stream->stream_id, (long)url_stream->cur_pos);
    } break;
    case e_notify_play_position_passed: {
        ret = clova_audio_general_cmd(CLOVA_HEADER_NAME_PROGRESS_REPORT_POSITION_PASSED,
                                      url_stream->stream_id, (long)url_stream->cur_pos);
    } break;
    case e_notify_play_end: {
    } break;
    default: {
    } break;
    }

    return ret;
}

int lguplus_url_notify_cb(e_url_notify_t notify, url_stream_t *url_stream, void *ctx)
{
    int ret = 0;

    switch (notify) {
    case e_notify_play_started: {
        ret = lguplus_event_queue_audio(LGUP_PLAY_STARTED, url_stream->stream_id,
                                        (long)url_stream->play_pos);
        ret = lguplus_event_queue_audio(LGUP_PLAY_PROGRESS_REPORT, url_stream->stream_id,
                                        (long)url_stream->play_pos);
    } break;
    case e_notify_play_stopped: {
        ret = lguplus_event_queue_audio(LGUP_PLAY_STOP, url_stream->stream_id,
                                        (long)url_stream->cur_pos);
    } break;
    case e_notify_play_paused: {
        ret = lguplus_event_queue_audio(LGUP_PLAY_PAUSED, url_stream->stream_id,
                                        (long)url_stream->cur_pos);
    } break;
    case e_notify_play_resumed: {
        ret = lguplus_event_queue_audio(LGUP_PLAY_RESUMED, url_stream->stream_id,
                                        (long)url_stream->cur_pos);
    } break;
    case e_notify_play_near_finished: {

    } break;
    case e_notify_play_finished: {
        ret = lguplus_event_queue_audio(LGUP_PLAY_FINISHED, url_stream->stream_id,
                                        (long)url_stream->cur_pos);
    } break;
    case e_notify_play_error: {
        ret = lguplus_event_queue_audio(LGUP_PLAY_FAILED, url_stream->stream_id,
                                        (long)url_stream->cur_pos);
    } break;
    case e_notify_play_delay_passed: {

    } break;
    case e_notify_play_interval_passed: {
        ret = lguplus_event_queue_audio(LGUP_PLAY_PROGRESS_REPORT, url_stream->stream_id,
                                        (long)url_stream->cur_pos);
    } break;
    case e_notify_play_position_passed: {

    } break;
    case e_notify_play_end: {

    } break;
    default: {

    } break;
    }

    return ret;
}

int url_notify_cb(e_url_notify_t notify, url_stream_t *url_stream, void *ctx)
{
    int ret = 0;
    asr_player_manager_t *player_manager = (asr_player_manager_t *)ctx;
    PLAYER_MANAGER_DEBUG("--------notify=%d cur_pos=%lld token=%s--------\n", notify,
                         url_stream->cur_pos, url_stream->stream_id);

    if (!url_stream || !player_manager) {
        return -1;
    }

    pthread_mutex_lock(&player_manager->state_lock);
    player_manager->client_type = url_stream->client_type;
    pthread_mutex_unlock(&player_manager->state_lock);

    if (url_stream->client_type == e_client_clova) {
        ret = clova_url_notify_cb(notify, url_stream, ctx);
    } else if (url_stream->client_type == e_client_lguplus) {
        ret = lguplus_url_notify_cb(notify, url_stream, ctx);
    }

    return 0;
}

int cid_notify_cb(cid_notify_t notify, cid_type_t cid_type, void *data, void *ctx)
{
    asr_player_manager_t *player_manager = (asr_player_manager_t *)ctx;

    PLAYER_MANAGER_DEBUG("--------notify=%d data=%s--------\n", notify, (char *)data);

    if (data && player_manager) {
        pthread_mutex_lock(&player_manager->state_lock);
        os_free(player_manager->cur_speech_token);
        player_manager->cur_speech_token = os_strdup((char *)data);
        if (e_notify_started == notify) {
            player_manager->cur_speech_state = 1;
        } else {
            player_manager->cur_speech_state = 0;
        }
        pthread_mutex_unlock(&player_manager->state_lock);
    }

    if (e_notify_end == notify) {
        if (player_manager) {
            int flags = 0;
            pthread_mutex_lock(&player_manager->state_lock);
            if (player_manager->is_continue) {
                player_manager->is_continue = 0;
                NOTIFY_CMD(ASR_TTS, TLK_CONTINUE);
            } else {
                flags = 1;
            }
            if (g_player_manager->dialog_id) {
                free(g_player_manager->dialog_id);
                g_player_manager->dialog_id = NULL;
            }
            pthread_mutex_unlock(&player_manager->state_lock);
            if (flags) {
                common_focus_manage_clear(e_focus_recognize);
                asr_notification_resume();
            }
        }
    } else if (e_notify_started == notify) {
        if (!get_asr_talk_start()) {
            asr_light_effects_set(e_effect_speeking);
            asr_light_effects_set(e_effect_listening_off);
        }
        if (data && cid_type != e_cid_pcm) {
            clova_speech_general_cmd(NAME_SPEECHSTARTED, (char *)data);
        }
    } else if (e_notify_finished == notify) {
        asr_light_effects_set(e_effect_speeking_off);
        if (data && cid_type != e_cid_pcm) {
            clova_speech_general_cmd(NAME_SPEECHFINISHED, (char *)data);
        }
    }

    return 0;
}

int asr_player_init(void)
{
    if (g_player_manager == NULL) {
        g_player_manager = (asr_player_manager_t *)os_calloc(1, sizeof(asr_player_manager_t));
        if (g_player_manager) {
            pthread_mutex_init(&g_player_manager->state_lock, NULL);
            g_player_manager->url_player = url_player_create(url_notify_cb, g_player_manager);
            g_player_manager->cid_player = cid_player_create(cid_notify_cb, g_player_manager);
        }
    }

    return 0;
}

int asr_player_uninit(void)
{
    if (g_player_manager) {
        pthread_mutex_lock(&g_player_manager->state_lock);
        os_free(g_player_manager->cur_speech_token);
        os_free(g_player_manager->dialog_id);
        pthread_mutex_unlock(&g_player_manager->state_lock);

        pthread_mutex_destroy(&g_player_manager->state_lock);
        if (g_player_manager->url_player) {
            url_player_destroy(&g_player_manager->url_player);
        }
        if (g_player_manager->cid_player) {
            cid_player_destroy(&g_player_manager->cid_player);
        }
        os_free(g_player_manager);
        g_player_manager = NULL;
    }

    return 0;
}

static e_behavior_t get_play_behavior(char *play_behavior)
{
    e_behavior_t behavior = e_replace_none;

    if (play_behavior) {
        if (StrEq(play_behavior, PLAYBEHAVIOR_REPLACE_ALL)) {
            behavior = e_replace_all;
        } else if (StrEq(play_behavior, PLAYBEHAVIOR_REPLACE_ENQUEUED)) {
            behavior = e_replace_endqueue;
        } else if (StrEq(play_behavior, PLAYBEHAVIOR_ENQUEUE)) {
            behavior = e_endqueue;
        }
    }

    return behavior;
}

static int asr_player_parse_play(json_object *js_obj, int down_cmd)
{
    url_stream_t new_stream = {0};
    char *dialog_id = NULL;

    if (js_obj) {
        json_object *js_header = json_object_object_get(js_obj, KEY_HEADER);
        if (js_header) {
            dialog_id = JSON_GET_STRING_FROM_OBJECT(js_header, KEY_DIALOGREQUESTID);
            pthread_mutex_lock(&g_player_manager->state_lock);
            if (dialog_id) {
                if (StrEq(dialog_id, g_player_manager->dialog_id)) {
                    new_stream.new_dialog = 0;
                } else {
                    new_stream.new_dialog = 1;
                    os_free(g_player_manager->dialog_id);
                    g_player_manager->dialog_id = os_strdup(dialog_id);
                }
            } else {
                if (down_cmd) {
                    new_stream.new_dialog = 1;
                } else {
                    new_stream.new_dialog = 0;
                }
                os_free(g_player_manager->dialog_id);
                g_player_manager->dialog_id = NULL;
            }
            pthread_mutex_unlock(&g_player_manager->state_lock);
            ASR_PRINT(ASR_DEBUG_INFO, "new_dialog (%d)\n", new_stream.new_dialog);
        }

        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            char *play_behavior = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_PLAYBEHAVIOR);
            if (play_behavior) {
                new_stream.behavior = get_play_behavior(play_behavior);
                ASR_PRINT(ASR_DEBUG_INFO, "play_behavior (%d)\n", new_stream.behavior);
            }
            json_object *js_audio_item = json_object_object_get(js_payload, PAYLOAD_AUDIOITEM);
            if (js_audio_item) {
                new_stream.stream_item =
                    JSON_GET_STRING_FROM_OBJECT(js_audio_item, PAYLOAD_AUDIOITEM_ID);
                json_object *js_audio_stream =
                    json_object_object_get(js_audio_item, PAYLOAD_AUDIOITEM_STREAM);
                if (js_audio_stream) {
                    new_stream.stream_id =
                        JSON_GET_STRING_FROM_OBJECT(js_audio_stream, PAYLOAD_STREAM_TOKEN);
                    new_stream.play_pos = JSON_GET_LONG_FROM_OBJECT(
                        js_audio_stream, CLOVA_PAYLOAD_NAME_BEGIN_AT_IN_MILLISECONDS);
                    new_stream.url =
                        JSON_GET_STRING_FROM_OBJECT(js_audio_stream, PAYLOAD_STREAM_URL);
                    new_stream.duration = JSON_GET_LONG_FROM_OBJECT(
                        js_audio_stream, CLOVA_PAYLOAD_DURATION_IN_MILLISECONDS);
                    int url_playable =
                        JSON_GET_BOOL_FROM_OBJECT(js_audio_stream, CLOVA_PAYLOAD_URL_PLAYABLE);
                    if (url_playable == 0)
                        new_stream.audio_stream = js_audio_stream;
                    json_object *js_stream_progress =
                        json_object_object_get(js_audio_stream, PAYLOAD_STREAM_PROGRESS);
                    if (js_stream_progress) {
                        new_stream.play_report_delay = JSON_GET_LONG_FROM_OBJECT(
                            js_stream_progress, PAYLOAD_PROGRESS_DELAY_MS);
                        new_stream.play_report_interval = JSON_GET_LONG_FROM_OBJECT(
                            js_stream_progress, PAYLOAD_PROGRESS_INTERVAL_MS);
                        new_stream.play_report_position = JSON_GET_LONG_FROM_OBJECT(
                            js_stream_progress, CLOVA_HEADER_NAME_PROGRESS_REPORT_POSITION_PASSED);
                    }
                    new_stream.stream_data = json_object_to_json_string(js_audio_stream);
                    new_stream.client_type = e_client_clova;

                    if (g_player_manager && g_player_manager->url_player) {
                        asr_bluetooth_disconnect(1);
                        if (url_playable == 0) {
                            new_stream.url = NULL;
                            url_player_insert_stream(g_player_manager->url_player, &new_stream);
                        } else {
                            url_player_insert_stream(g_player_manager->url_player, &new_stream);
                        }
                        if (new_stream.behavior == e_replace_all &&
                            (dialog_id != NULL || down_cmd == 1)) {
                            asr_light_effects_set(e_effect_music_play);
                        }
                    }
                }
            }
        }

        return 0;
    }

    return -1;
}

/*
Stop Directive
{
    "directive": {
        "header": {
            "namespace": "AudioPlayer",
            "name": "Stop",
            "messageId": "{{STRING}}",
            "dialogRequestId": "{{STRING}}"
        },
        "payload": {
        }
    }
}
*/
static int asr_player_parse_stop(json_object *js_obj)
{
    int ret = -1;

    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            if (g_player_manager && g_player_manager->url_player) {
                ret = url_player_set_cmd(g_player_manager->url_player, e_player_cmd_stop);
                if (ret == 0) {
                    asr_queue_clear_audio();
                }
            }
        }
        return 0;
    }

    return -1;
}

/*
ClearQueue Directive
{
    "directive": {
        "header": {
            "namespace": "AudioPlayer",
            "name": "ClearQueue",
            "messageId": "{{STRING}}",
            "dialogRequestId": "{{STRING}}"
        },
        "payload": {
            "clearBehavior": "{{STRING}}"
        }
    }
}
*/
static int asr_player_parse_clear_queue(json_object *js_obj)
{
    int ret = -1;

    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            char *clear_behavior = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_CLEARBEHAVIOR);
            if (clear_behavior) {
                PLAYER_MANAGER_DEBUG("clear_behavior %s\n", clear_behavior);
                url_player_set_cmd(g_player_manager->url_player, e_player_cmd_clear_queue);
                if (StrEq(clear_behavior, PAYLOAD_CLEAR_ALL)) {
                    url_player_set_cmd(g_player_manager->url_player, e_player_cmd_stop);
                    // TODO: Clear the tts player q
                }
            }
            ret = 0;
        }
    }

    return ret;
}

static int asr_player_parse_stream_deliver(json_object *js_obj)
{
    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            char *audio_item_id = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_AUDIOITEM_ID);
            json_object *js_audio_stream =
                json_object_object_get(js_payload, CLOVA_PAYLOAD_AUDIOITEM_STREAM);
            if (js_audio_stream) {
                char *url = JSON_GET_STRING_FROM_OBJECT(js_audio_stream, PAYLOAD_STREAM_URL);
                if (g_player_manager && audio_item_id && url) {
                    url_player_update_stream(g_player_manager->url_player, audio_item_id, url);
                }
            }
        }

        return 0;
    }

    return -1;
}

/*
Speak Directive
{
    "directive": {
        "header": {
            "namespace": "SpeechSynthesizer",
            "name": "Speak",
            "messageId": "{{STRING}}",
            "dialogRequestId": "{{STRING}}"
        },
        "payload": {
            "url": "{{STRING}}",
            "format": "{{STRING}}",
            "token": "{{STRING}}"
        }
    }
}
*/
static int asr_player_parse_speak(json_object *js_obj)
{
    int ret = -1;
    char *token = NULL;
    char *url = NULL;
    char *dialog_id = NULL;
    char *dialog_id_dup = NULL;

    if (js_obj) {
        json_object *js_header = json_object_object_get(js_obj, KEY_HEADER);
        if (js_header) {
            dialog_id = JSON_GET_STRING_FROM_OBJECT(js_header, KEY_DIALOGREQUESTID);
            pthread_mutex_lock(&g_player_manager->state_lock);
            if (dialog_id) {
                os_free(g_player_manager->dialog_id);
                g_player_manager->dialog_id = os_strdup(dialog_id);
            } else {
                // os_free(g_player_manager->dialog_id);
                // g_player_manager->dialog_id = NULL;
            }
            if (g_player_manager->dialog_id) {
                dialog_id_dup = os_strdup(g_player_manager->dialog_id);
            }
            pthread_mutex_unlock(&g_player_manager->state_lock);
        }

        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            token = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_TOKEN);
            url = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_URL);
            if (g_player_manager && token && url) {
                cid_player_add_item(g_player_manager->cid_player, dialog_id_dup, token, url);
            }
            ret = 0;
        }
    }

    if (dialog_id_dup) {
        free(dialog_id_dup);
    }

    return ret;
}

/*
 *
 */
static int asr_player_parse_expect_speech(json_object *js_obj)
{
    int ret = 0;
    int explicit = -1;
    char *expectSpeechId = NULL;
    stream_state_t *stream_state = NULL;

    if (js_obj) {
        json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            explicit = JSON_GET_BOOL_FROM_OBJECT(js_payload, CLOVA_PAYLOAD_EXPLICIT_NAME);
            clova_set_explicit(explicit);

            expectSpeechId =
                JSON_GET_STRING_FROM_OBJECT(js_payload, CLOVA_PAYLOAD_EXPECTSPEECHID_NAME);
            if (expectSpeechId) {
                clova_set_speechId(expectSpeechId);
            }
        }

        if (g_player_manager && g_player_manager->url_player) {
            stream_state = url_player_get_state(g_player_manager->url_player, e_client_clova);
            if (explicit == 0 && stream_state && stream_state->state == e_playing) {
                /* do nothing */
            } else {
                pthread_mutex_lock(&g_player_manager->state_lock);
                g_player_manager->is_continue = 1;
                pthread_mutex_unlock(&g_player_manager->state_lock);
            }
            url_player_free_state(stream_state);
        }

        return 0;
    }

    return -1;
}

/*
 *
 */
int asr_player_parse_directvie(json_object *js_data, int down_cmd)
{
    int ret = -1;
    char *name = NULL;
    char *name_space = NULL;

    if (js_data) {
        json_object *js_header = json_object_object_get(js_data, KEY_HEADER);
        if (js_header) {
            name_space = JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            name = JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
        }

        if (StrEq(name_space, NAMESPACE_AUDIOPLAYER)) {
            if (StrEq(name, NAME_PLAY)) {
                ret = asr_player_parse_play(js_data, down_cmd);
            } else if (StrEq(name, NAME_STOP)) {
                ret = asr_player_parse_stop(js_data);
            } else if (StrEq(name, NAME_CLEARQUEUE)) {
                ret = asr_player_parse_clear_queue(js_data);
            } else if (StrEq(name, CLOVA_HEADER_STREAM_DELIVER)) {
                ret = asr_player_parse_stream_deliver(js_data);
            } else if (StrEq(name, CLOVA_HEADER_EXPECT_REPORT_PLAYBACK_STATE)) {
                ret = clova_audio_report_state_cmd(CLOVA_HEADER_NAME_REPORT_PLAYBACK_STATE);
            }
        } else if (StrEq(name_space, NAMESPACE_SPEECHSYNTHESIZER)) {
            if (StrEq(name, NAME_SPEAK)) {
                ret = asr_player_parse_speak(js_data);
            }
        } else if (StrEq(name_space, NAMESPACE_SPEECHRECOGNIZER)) {
            if (StrEq(name, NAME_EXPECTSPEECH)) {
                asr_player_parse_expect_speech(js_data);
            }
        }
    }

    return ret;
}

char *asr_player_state(int request_type)
{
    int need_free = 0;
    char *url_player_state = NULL;
    char *audio_url = NULL;
    char *speech_token = NULL;
    char *speech_state = PLAYER_FINISHED;
    char *play_state = PLAYER_IDLE;
    long play_offset_ms = 0;
    int volume = 0;
    int muted = 0;
    int repeat = 0;
    json_object *stream = NULL;
    long long duration = 0;
    char *repeat_mode = "NONE";
    stream_state_t *stream_state = NULL;

    if (g_player_manager) {
        pthread_mutex_lock(&g_player_manager->state_lock);
        speech_token = os_strdup(g_player_manager->cur_speech_token);
        speech_state = g_player_manager->cur_speech_state ? PLAYER_PLAYING : PLAYER_FINISHED;
        pthread_mutex_unlock(&g_player_manager->state_lock);

        if (g_player_manager->url_player) {
            stream_state = url_player_get_state(g_player_manager->url_player, e_client_clova);
            if (stream_state) {
                switch (stream_state->state) {
                case e_stopped: {
                    play_state = PLAYER_STOPPED;
                } break;
                case e_paused: {
                    play_state = PLAYER_PAUSED;
                } break;
                case e_playing: {
                    play_state = PLAYER_PLAYING;
                } break;
                case e_idle:
                default: {
                    play_state = PLAYER_IDLE;
                } break;
                }
                if (stream_state->stream_data) {
                    stream = json_tokener_parse(stream_state->stream_data);
                }
                play_offset_ms = (long)stream_state->cur_pos;
                duration = stream_state->duration;

                volume = stream_state->volume;
                muted = stream_state->muted;
                repeat = stream_state->repeat;
            }

            repeat_mode = repeat ? "REPEAT_ONE" : "NONE";
        }

        // get the url's play status
        /* clova need to report STOPPED status if we wish to get another song */
        if (stream && StrEq(play_state, PLAYER_IDLE)) {
            play_state = PLAYER_STOPPED;
        }
    }

    volume = (volume * 15) / 100 + (((volume * 15) % 100 > 50) ? 1 : 0);
    create_asr_state(&url_player_state, request_type, duration, play_offset_ms, Val_Str(play_state),
                     repeat_mode, stream, volume, muted, Val_Str(speech_token), 0,
                     Val_Str(speech_state));

    os_free(speech_token);
    url_player_free_state(stream_state);
    if (stream) {
        json_object_put(stream);
    }

    return url_player_state;
}

void asr_player_get_audio_info(char **params)
{
    char *play_state = PLAYER_IDLE;
    long play_offset_ms = 0;
    json_object *stream = NULL;
    long long duration = 0;
    char *repeat_mode = NULL;
    char *token = NULL;
    int repeat = 0;
    stream_state_t *stream_state = NULL;

    if (g_player_manager) {
        if (g_player_manager->url_player) {
            stream_state = url_player_get_state(g_player_manager->url_player, e_client_clova);
            if (stream_state) {
                switch (stream_state->state) {
                case e_stopped: {
                    play_state = PLAYER_STOPPED;
                } break;
                case e_paused: {
                    play_state = PLAYER_PAUSED;
                } break;
                case e_playing: {
                    play_state = PLAYER_PLAYING;
                } break;
                case e_idle:
                default: {
                    play_state = PLAYER_IDLE;
                } break;
                }
                if (stream_state->stream_data) {
                    stream = json_tokener_parse(stream_state->stream_data);
                }
                play_offset_ms = (long)stream_state->cur_pos;
                duration = stream_state->duration;
                token = stream_state->stream_id;
                repeat = stream_state->repeat;
            }
            repeat_mode = (repeat) ? "REPEAT_ONE" : "NONE";
        }
    }

    create_audio_report_state_params(params, duration, play_offset_ms, Val_Str(play_state),
                                     repeat_mode, stream, token);
    url_player_free_state(stream_state);
    if (stream) {
        json_object_put(stream);
    }
}

int asr_player_force_stop(void)
{
    int ret = 0;

    if (g_player_manager && g_player_manager->url_player) {
        // TODO: Clear the tts player q
        url_player_set_cmd(g_player_manager->url_player, e_player_cmd_clear_queue);
        url_player_set_cmd(g_player_manager->url_player, e_player_cmd_stop);
        asr_queue_clear_audio();
    }

    return 0;
}

int asr_player_buffer_start_pcm(char *content_id)
{
    int ret = -1;
    char *pcm_id = NULL;

    if (g_player_manager && content_id) {
        asprintf(&pcm_id, "pcm:%s", content_id);
        if (pcm_id) {
            ret = cid_player_add_item(g_player_manager->cid_player, content_id, content_id, pcm_id);
            free(pcm_id);
        }
    }

    return ret;
}

// e_asr_stop: stop current tts
// e_asr_finished: tts cache finished
int asr_player_buffer_stop(char *content_id, int type)
{
    int ret = -1;

    if (g_player_manager && g_player_manager->cid_player) {
        if (type == e_asr_start) {
            pthread_mutex_lock(&g_player_manager->state_lock);
            g_player_manager->is_continue = 0;
            pthread_mutex_unlock(&g_player_manager->state_lock);
        } else if (type == e_asr_stop) {
            pthread_mutex_lock(&g_player_manager->state_lock);
            g_player_manager->cur_speech_state = 0;
            g_player_manager->is_continue = 0;
            pthread_mutex_unlock(&g_player_manager->state_lock);
            ret = cid_player_stop(g_player_manager->cid_player, 1);
        } else if (type == e_asr_finished) {
            ret = cid_player_cache_finished(g_player_manager->cid_player, content_id);
        }
    }

    return ret;
}

int asr_player_buffer_write_data(char *content_id, char *data, int len)
{
    int ret = -1;

    if (g_player_manager && g_player_manager->cid_player) {
        ret = cid_player_write_data(g_player_manager->cid_player, content_id, data, len);
    }
    return ret;
}

int asr_player_get_client_type(void)
{
    int client_type = 0;

    if (g_player_manager) {
        pthread_mutex_lock(&g_player_manager->state_lock);
        client_type = g_player_manager->client_type;
        pthread_mutex_unlock(&g_player_manager->state_lock);
    }

    return client_type;
}

int asr_player_get_state(int type)
{
    int ret = 0;

    if (g_player_manager) {
        pthread_mutex_lock(&g_player_manager->state_lock);
        if (type == e_tts_player) {
            ret = g_player_manager->cur_speech_state;
        } else if (type == e_asr_status) {
            ret = g_player_manager->is_continue;
        }
        pthread_mutex_unlock(&g_player_manager->state_lock);
    }

    return ret;
}

int asr_player_need_unmute(void)
{
    int is_muted = 0;
    int volume = 0;
    char buff[128] = {0};

    if (g_player_manager && g_player_manager->url_player) {
        url_player_get_value(g_player_manager->url_player, e_value_volume, &volume);
        url_player_get_value(g_player_manager->url_player, e_value_mute, &is_muted);

        // if mute ,switch to unmute
        if (is_muted) {
            url_player_set_value(g_player_manager->url_player, e_value_mute, 0);
        }
        if (VOLUME_VALUE_OF_INIT > volume || is_muted) {
            // set value as TAP did
            if (volume < VOLUME_VALUE_OF_INIT) {
                volume = VOLUME_VALUE_OF_INIT;
            }
            url_player_set_value(g_player_manager->url_player, e_value_volume, volume);
        }
    }

    return 0;
}

int asr_player_get_volume(int get_mute)
{
    int value = 0;

    if (g_player_manager && g_player_manager->url_player) {
        if (get_mute == 0) {
            url_player_get_value(g_player_manager->url_player, e_value_volume, &value);
        } else {
            url_player_get_value(g_player_manager->url_player, e_value_mute, &value);
        }
    }

    return value;
}

int asr_player_set_volume(int set_mute, int val)
{
    if (g_player_manager && g_player_manager->url_player) {
        if (set_mute) {
            url_player_set_value(g_player_manager->url_player, e_value_mute, val);
        } else {
            url_player_set_value(g_player_manager->url_player, e_value_volume, val);
        }
    }

    return 0;
}

int asr_player_set_repeat_mode(char *mode)
{
    int ret = -1;

    if (mode == NULL) {
        return -1;
    }

    if (g_player_manager && g_player_manager->url_player) {
        if (StrEq(mode, "REPEAT_ONE")) {
            ret = url_player_set_value(g_player_manager->url_player, e_value_repeat, 1);
        } else if (StrEq(mode, "NONE")) {
            ret = url_player_set_value(g_player_manager->url_player, e_value_repeat, 0);
        } else {
        }
    }

    return ret;
}

int asr_player_set_pause(void)
{
    int ret = -1;

    if (g_player_manager && g_player_manager->url_player) {
        ret = url_player_set_cmd(g_player_manager->url_player, e_player_cmd_pause);
    }

    return ret;
}

int asr_player_set_resume(void)
{
    int ret = -1;

    if (g_player_manager && g_player_manager->url_player) {
        ret = url_player_set_cmd(g_player_manager->url_player, e_player_cmd_resume);
    }

    return ret;
}

int asr_player_set_stop(void)
{
    int ret = -1;

    if (g_player_manager && g_player_manager->url_player) {
        pthread_mutex_lock(&g_player_manager->state_lock);
        if (g_player_manager->cur_speech_state) {
            common_focus_manage_clear(e_focus_recognize);
        }
        pthread_mutex_unlock(&g_player_manager->state_lock);
        if (check_focus_on_tts()) {
            asr_player_buffer_stop(NULL, e_asr_stop);
        } else {
            ret = url_player_set_cmd(g_player_manager->url_player, e_player_cmd_stop);
        }
    }

    return ret;
}

int asr_player_set_next(void)
{
    int ret = -1;

    if (g_player_manager && g_player_manager->url_player) {
        ret = url_player_set_cmd(g_player_manager->url_player, e_player_cmd_next);
    }

    return ret;
}

int asr_player_set_previous(void)
{
    int ret = -1;

    if (g_player_manager && g_player_manager->url_player) {
        ret = url_player_set_cmd(g_player_manager->url_player, e_player_cmd_prev);
    }

    return ret;
}

int asr_player_set_replay(void)
{
    int ret = -1;

    if (g_player_manager && g_player_manager->url_player) {
        ret = url_player_set_cmd(g_player_manager->url_player, e_player_cmd_replay);
    }

    return ret;
}

int asr_player_start_play_url(int type, const char *url, long long play_offset_ms)
{
    int ret = -1;

    if (g_player_manager && g_player_manager->url_player) {
        ret = url_player_start_play_url(g_player_manager->url_player, type, url, play_offset_ms);
    }

    return ret;
}

void asr_player_stop_play_url(int type)
{
    if (g_player_manager && g_player_manager->url_player) {
        url_player_stop_play_url(g_player_manager->url_player, type);
    }
}

int asr_player_get_play_url_state(int type)
{
    int ret = -1;

    if (g_player_manager && g_player_manager->url_player) {
        ret = url_player_get_play_url_state(g_player_manager->url_player, type);
    }

    return ret;
}

int lguplus_play_audio(char *play_behavior, char *report_interval, char *item_id, char *url)
{
    url_stream_t new_stream = {0};

    new_stream.behavior = get_play_behavior(play_behavior);
    new_stream.play_report_interval = atoi(report_interval) * 1000;
    new_stream.stream_item = item_id;
    new_stream.stream_id = item_id;
    new_stream.url = url;
    new_stream.client_type = e_client_lguplus;

    if (g_player_manager && g_player_manager->url_player) {
        url_player_insert_stream(g_player_manager->url_player, &new_stream);
    }

    return 0;
}

/*
"audioPlayer":
{
    "header": {
    "schema": "audioPlayer",
    "method": "state"
},
    "state": {
        "offset": "12345",
        "itemId": "A12345",
        "activity": "STOP"
    }
}
*/
int lguplus_audio_state(json_object *js_state)
{
    int ret = -1;
    char *play_state = PLAYER_IDLE;
    long play_offset_ms = 0;
    int volume = 0;
    int muted = 0;
    long long duration = 0;
    stream_state_t *stream_state = NULL;

    if (g_player_manager) {
        if (g_player_manager->url_player) {
            stream_state = url_player_get_state(g_player_manager->url_player, e_client_lguplus);
            if (stream_state) {
                switch (stream_state->state) {
                case e_stopped: {
                    play_state = LGUP_STATE_STOP;
                } break;
                case e_paused: {
                    play_state = LGUP_STATE_PAUSE;
                } break;
                case e_playing: {
                    play_state = LGUP_STATE_PLAY;
                } break;
                case e_idle:
                default: {
                    play_state = LGUP_STATE_IDLE;
                } break;
                }
                play_offset_ms = (long)stream_state->cur_pos;
                duration = stream_state->duration;
                volume = stream_state->volume;
                muted = stream_state->muted;
            }
        }
        {
            json_object *audio_state = json_object_new_object();
            json_object *header = json_object_new_object();
            json_object *state = json_object_new_object();
            if (header && state) {
                json_object_object_add(header, "schema", JSON_OBJECT_NEW_STRING("AudioPlayer"));
                json_object_object_add(header, "method", JSON_OBJECT_NEW_STRING("state"));

                json_object_object_add(state, "offset", json_object_new_int(play_offset_ms));
                json_object_object_add(state, "itemId",
                                       JSON_OBJECT_NEW_STRING(stream_state->stream_id));
                json_object_object_add(state, "activity", JSON_OBJECT_NEW_STRING(play_state));

                json_object_object_add(audio_state, "header", header);
                json_object_object_add(audio_state, "state", state);
            }
            json_object_object_add(js_state, "audioPlayer", audio_state);
        }
        {
            json_object *volume_state = json_object_new_object();
            json_object *header = json_object_new_object();
            json_object *state = json_object_new_object();
            if (header && state) {
                json_object_object_add(header, "schema", JSON_OBJECT_NEW_STRING("Volume"));
                json_object_object_add(header, "method", JSON_OBJECT_NEW_STRING("state"));

                json_object_object_add(state, "volume", json_object_new_int(volume));
                json_object_object_add(state, "mute", json_object_new_boolean(muted));

                json_object_object_add(volume_state, "header", header);
                json_object_object_add(volume_state, "state", state);
            }
            json_object_object_add(js_state, "volume", volume_state);
        }
        ret = 0;
    }

    url_player_free_state(stream_state);

    return ret;
}

void lguplus_do_talkcontinue(void)
{
    pthread_mutex_lock(&g_player_manager->state_lock);
    g_player_manager->is_continue = 1;
    pthread_mutex_unlock(&g_player_manager->state_lock);
}
