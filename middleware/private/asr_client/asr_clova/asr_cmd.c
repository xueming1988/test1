
#include <stdio.h>

#include "asr_cmd.h"
#include "asr_debug.h"
#include "asr_json_common.h"

#include "clova_clova.h"
#include "clova_audio_player.h"
#include "clova_playback_controller.h"
#include "clova_speech_synthesizer.h"
#include "clova_device_control.h"
#include "clova_settings.h"

cmd_q_t *g_event_queue = NULL;

static int asr_queue_findby_name(char *cmd_js_str, char *event_name)
{
    int ret = -1;
    char *val = NULL;
    json_object *cmd_js = NULL;

    if (!cmd_js_str || !event_name) {
        ASR_PRINT(ASR_DEBUG_ERR, "cmd_js is NULL\n");
        return -1;
    }

    cmd_js = json_tokener_parse(cmd_js_str);
    if (!cmd_js) {
        ASR_PRINT(ASR_DEBUG_INFO, "cmd_js is not a json\n");
        return -1;
    }

    val = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_NAME);
    if (StrEq(val, event_name)) {
        ret = 0;
    }

    int id = JSON_GET_INT_FROM_OBJECT(cmd_js, KEY_id);
    if (id == 1) {
        ret = -1;
    }

    json_object_put(cmd_js);

    return ret;
}

static int event_remove(void *reasion, void *cmd_dst)
{
    int ret = -1;

    if (cmd_dst && reasion) {
        ret = asr_queue_findby_name((char *)cmd_dst, (char *)reasion);
    }

    return ret;
}

cmd_q_t *asr_queue_init(void)
{
    if (g_event_queue) {
        cmd_q_uninit(&g_event_queue);
    }
    g_event_queue = cmd_q_init(NULL);

    return g_event_queue;
}

int asr_queue_uninit(void)
{
    if (g_event_queue) {
        cmd_q_uninit(&g_event_queue);
    }
    return 0;
}

void asr_queue_clear_audio(void)
{
    if (g_event_queue) {
        cmd_q_remove(g_event_queue, (void *)CLOVA_HEADER_NAME_PLAY_STARTED, event_remove);
        cmd_q_remove(g_event_queue, (void *)CLOVA_HEADER_NAME_PLAY_FINISHED, event_remove);
        cmd_q_remove(g_event_queue, (void *)CLOVA_HEADER_NAME_PROGRESS_REPORT_DELAY_PASSED,
                     event_remove);
        cmd_q_remove(g_event_queue, (void *)CLOVA_HEADER_NAME_PROGRESS_REPORT_INTERVAL_PASSED,
                     event_remove);
        cmd_q_remove(g_event_queue, (void *)CLOVA_HEADER_NAME_PROGRESS_REPORT_POSITION_PASSED,
                     event_remove);
    }
}

void asr_queue_clear_report_state(void)
{
    if (g_event_queue) {
        cmd_q_remove(g_event_queue, (void *)CLOVA_HEADER_NAME_REPORT_STATE, event_remove);
    }
}

void asr_queue_clear_alert(void)
{
    if (g_event_queue) {
        cmd_q_remove(g_event_queue, (void *)NAME_SETALERT_SUCCEEDED, event_remove);
        cmd_q_remove(g_event_queue, (void *)NAME_SETALERT_FAILED, event_remove);
        cmd_q_remove(g_event_queue, (void *)NAME_DELETE_ALERT_SUCCEEDED, event_remove);
        cmd_q_remove(g_event_queue, (void *)NAME_DELETE_ALERT_FAILED, event_remove);
        cmd_q_remove(g_event_queue, (void *)NAME_ALERT_STOPPED, event_remove);
        cmd_q_remove(g_event_queue, (void *)NAME_CLEAR_ALERT_SUCCEEDED, event_remove);
        cmd_q_remove(g_event_queue, (void *)NAME_CLEAR_ALERT_FAILED, event_remove);
        cmd_q_remove(g_event_queue, (void *)NAME_ALERT_STARTED, event_remove);
        cmd_q_remove(g_event_queue, (void *)NAME_ALERT_STOPPED, event_remove);
    }
}

/*
 * Create AudioPlayer Cmd
 * Input :
 *         @cmd_name:
 *         @token: the token of current play
 *         @pos: the position of current play
 *
 * Output: int
 */
int clova_audio_general_cmd(char *cmd_name, char *token, long pos)
{
    int ret = -1;
    char *buff = NULL;

    if (!token || !cmd_name || !g_event_queue) {
        return -1;
    }

    buff = create_audio_general_cmd(cmd_name, token, pos);
    if (buff) {
        cmd_q_add_tail(g_event_queue, buff, strlen(buff));
        free(buff);
        ret = 0;
    }

    if (ret != 0) {
        printf("[%s:%d] failed\n", __FUNCTION__, __LINE__);
    }

    return ret;
}

int clova_audio_report_state_cmd(char *cmd_name)
{
    int ret = -1;
    char *buff = NULL;

    if (!cmd_name || !g_event_queue) {
        return -1;
    }

    buff = create_audio_report_state_cmd(cmd_name);
    if (buff) {
        cmd_q_add_tail(g_event_queue, buff, strlen(buff));
        free(buff);
        ret = 0;
    }

    if (ret != 0) {
        printf("[%s:%d] failed\n", __FUNCTION__, __LINE__);
    }

    return ret;
}

int clova_audio_stream_request_cmd(char *cmd_name, char *audioItemId, json_object *audioStream)
{
    int ret = -1;
    char *buff = NULL;

    if (!cmd_name || !audioItemId || !audioStream || !g_event_queue) {
        return -1;
    }

    buff = create_stream_request_cmd(cmd_name, audioItemId, audioStream);
    if (buff) {
        cmd_q_add_tail(g_event_queue, buff, strlen(buff));
        free(buff);
        ret = 0;
    }

    if (ret != 0) {
        printf("[%s:%d] failed\n", __FUNCTION__, __LINE__);
    }

    return ret;
}

int clova_playback_general_cmd(char *cmd_name, char *device_id)
{
    int ret = -1;
    char *buff = NULL;

    if (!cmd_name || !g_event_queue) {
        return -1;
    }

    buff = create_playback_general_cmd(cmd_name, device_id);
    if (buff) {
        cmd_q_add_tail(g_event_queue, buff, strlen(buff));
        free(buff);
        ret = 0;
    }

    if (ret != 0) {
        printf("[%s:%d] failed\n", __FUNCTION__, __LINE__);
    }

    return ret;
}

int clova_settings_request_sync_event(int syncWithStorage)
{
    int ret = -1;
    char *buff = NULL;

    if (!g_event_queue) {
        return -1;
    }

    buff = create_settings_request_sync_event(syncWithStorage);
    if (buff) {
        cmd_q_add_tail(g_event_queue, buff, strlen(buff));
        free(buff);
        ret = 0;
    }

    if (ret != 0) {
        printf("[%s:%d] failed\n", __FUNCTION__, __LINE__);
    }

    return ret;
}

int clova_settings_report_event(void)
{
    int ret = -1;
    char *buff = NULL;

    if (!g_event_queue) {
        return -1;
    }

    buff = create_settings_general_event(CLOVA_HEADER_NAME_REPORT);
    if (buff) {
        cmd_q_add_tail(g_event_queue, buff, strlen(buff));
        free(buff);
        ret = 0;
    }

    if (ret != 0) {
        printf("[%s:%d] failed\n", __FUNCTION__, __LINE__);
    }

    return ret;
}

int clova_settings_request_store_event(void)
{
    int ret = -1;
    char *buff = NULL;

    if (!g_event_queue) {
        return -1;
    }

    buff = create_settings_general_event(CLOVA_HEADER_NAME_REQUEST_STORE_SETTINGS);
    if (buff) {
        cmd_q_add_tail(g_event_queue, buff, strlen(buff));
        free(buff);
        ret = 0;
    }

    if (ret != 0) {
        printf("[%s:%d] failed\n", __FUNCTION__, __LINE__);
    }

    return ret;
}

int clova_settings_request_version_spec_event(void)
{
    int ret = -1;
    char *buff = NULL;

    if (!g_event_queue) {
        return -1;
    }

    buff = create_settings_general_event(CLOVA_HEADER_NAME_REQUEST_VERSION_SPEC);
    if (buff) {
        cmd_q_add_tail(g_event_queue, buff, strlen(buff));
        free(buff);
        ret = 0;
    }

    if (ret != 0) {
        printf("[%s:%d] failed\n", __FUNCTION__, __LINE__);
    }

    return ret;
}

/*
 * Create system Cmd
 * Input :
 *         @cmd_name:
 *         @token:
 *
 * Output: int
 */
int clova_speech_general_cmd(char *cmd_name, char *token)
{
    int ret = -1;
    char *buff = NULL;

    if (!cmd_name || !token || !g_event_queue) {
        return -1;
    }

    buff = create_speech_general_cmd(cmd_name, token);
    if (buff) {
        if (StrEq(cmd_name, NAME_SPEECHSTARTED)) {
            cmd_q_remove(g_event_queue, (void *)NAME_SPEECHSTARTED, event_remove);
        }
        cmd_q_add_tail(g_event_queue, buff, strlen(buff));
        free(buff);
        ret = 0;
    }

    if (ret != 0) {
        printf("[%s:%d] failed\n", __FUNCTION__, __LINE__);
    }

    return ret;
}

int clova_speech_request_cmd(char *text, char *lang)
{
    int ret = -1;
    char *buff = NULL;

    if (!text || !lang || !g_event_queue) {
        return -1;
    }

    buff = create_speech_request_cmd(text, lang);
    if (buff) {
        cmd_q_remove(g_event_queue, (void *)CLOVA_HEADER_NAME_REQUEST, event_remove);
        cmd_q_add_tail(g_event_queue, buff, strlen(buff));
        free(buff);
        ret = 0;
    }

    if (ret != 0) {
        printf("[%s:%d] failed\n", __FUNCTION__, __LINE__);
    }

    return ret;
}

/*
 * Create alerts Cmd
 * Input :
 *         @cmd_name:
 *         @token:
 *
 * Output: int
 */
int clova_alerts_general_cmd(char *cmd_name, char *token, char *type)
{
    int ret = -1;
    char *buff = NULL;

    if (!cmd_name || !type || !g_event_queue) {
        return -1;
    }

    buff = create_alerts_general_cmd(cmd_name, token, type);
    if (buff) {
        cmd_q_add_tail(g_event_queue, buff, strlen(buff));
        free(buff);
        ret = 0;
    }

    if (ret != 0) {
        printf("[%s:%d] failed\n", __FUNCTION__, __LINE__);
    }

    return ret;
}

int clova_alerts_request_sync_cmd(char *cmd_name)
{
    int ret = -1;
    char *buff = NULL;

    if (!cmd_name || !g_event_queue) {
        return -1;
    }

    buff = create_alerts_request_sync_cmd(cmd_name);
    if (buff) {
        cmd_q_add_tail(g_event_queue, buff, strlen(buff));
        free(buff);
        ret = 0;
    }

    if (ret != 0) {
        printf("[%s:%d] failed\n", __FUNCTION__, __LINE__);
    }

    return ret;
}

int clova_device_control_action_cmd(char *name, char *target, char *command)
{
    int ret = -1;
    char *buff = NULL;

    if (!name || !target || !command || !g_event_queue) {
        return -1;
    }

    buff = create_device_control_action_cmd(name, target, command);
    if (buff) {
        cmd_q_add_tail(g_event_queue, buff, strlen(buff));
        free(buff);
        ret = 0;
    }

    if (ret != 0) {
        printf("[%s:%d] failed\n", __FUNCTION__, __LINE__);
    }

    return ret;
}

int clova_device_control_report_cmd(char *name)
{
    int ret = -1;
    char *buff = NULL;

    if (!name || !g_event_queue) {
        return -1;
    }

    buff = create_device_control_report_cmd(name);
    if (buff) {
        cmd_q_add_tail(g_event_queue, buff, strlen(buff));
        free(buff);
        ret = 0;
    }

    if (ret != 0) {
        printf("[%s:%d] failed\n", __FUNCTION__, __LINE__);
    }

    return ret;
}

int cmd_add_playback_event(char *name)
{
    int ret = -1;

    if (g_event_queue && name) {
        char *local_cmd = create_playback_general_cmd(name, NULL);
        if (local_cmd) {
            cmd_q_add_tail(g_event_queue, local_cmd, strlen(local_cmd));
            free(local_cmd);
            ret = 0;
        }
    }

    return ret;
}

int cmd_add_normal_event(char *name_space, char *name, char *payload)
{
    int ret = -1;
    char *normal_cmd = NULL;

    if (g_event_queue && name_space && name) {
        Create_Cmd(normal_cmd, name_space, name, Val_Obj(payload));
        if (normal_cmd) {
            if (StrEq(name, CLOVA_HEADER_PROCESSDELEGATEDEVENT)) {
                cmd_q_remove(g_event_queue, (void *)CLOVA_HEADER_PROCESSDELEGATEDEVENT,
                             event_remove);
            }
            cmd_q_add_tail(g_event_queue, normal_cmd, strlen(normal_cmd));
            free(normal_cmd);
            ret = 0;
        }
    }

    return ret;
}

int insert_cmd_queue(const char *new_cmd)
{
    int ret = -1;

    if (new_cmd && g_event_queue) {
        ret = cmd_q_add_tail(g_event_queue, new_cmd, strlen(new_cmd));
    }

    return ret;
}
