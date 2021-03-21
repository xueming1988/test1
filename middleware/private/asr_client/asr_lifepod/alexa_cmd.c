
#include <stdio.h>
#include <stdbool.h>

#include "alexa_cmd.h"
#include "alexa_json_common.h"

/*add by weiqiang.huang for MAR-385 Equalizer controller 2018-08-30 -begin*/
int alexa_equalizer_controller_cmd(int cmd_id, char *cmd_name, const char *payload_param)
{
    int ret = -1;
    char *buff = NULL;

    if (!payload_param || !cmd_name) {
        return -1;
    }

    Create_Cmd(buff, cmd_id, NAMESPACE_EQUALIZERCONTROLLER, cmd_name, payload_param);
    if (buff) {
        NOTIFY_CMD(ALEXA_CMD_FD, buff);
        free(buff);
        ret = 0;
    }

    if (ret != 0) {
        printf("[%s:%d] failed\n", __FUNCTION__, __LINE__);
    }

    return ret;
}
/*add by weiqiang.huang for MAR-385 Equalizer controller 2018-08-30 -end*/

/*
 * Create AudioPlayer Cmd
 * Input :
 *         @cmd_name:
 *         @token: the token of current play
 *         @pos: the position of current play
 *
 * Output: int
 */
int alexa_audio_cmd(int cmd_id, char *cmd_name, char *token, long pos, long duration, int err_num)
{
    int ret = -1;
    char *buff = NULL;

    if (!token || !cmd_name) {
        return -1;
    }

    // make sure position never < 0
    if (pos < 0) {
        printf("[%s:%d] pos = %ld\n", __FUNCTION__, __LINE__, pos);
        pos = 0;
    }

    buff = create_audio_cmd(cmd_id, cmd_name, token, pos, duration, err_num);
    if (buff) {
        NOTIFY_CMD(ALEXA_CMD_FD, buff);
        free(buff);
        ret = 0;
    }

    if (ret != 0) {
        printf("[%s:%d] failed\n", __FUNCTION__, __LINE__);
    }

    return ret;
}

int alexa_playback_cmd(int cmd_id, char *cmd_name)
{
    int ret = -1;
    char *buff = NULL;

    if (!cmd_name) {
        return -1;
    }

    buff = create_playback_cmd(cmd_id, cmd_name);
    if (buff) {
        NOTIFY_CMD(ALEXA_CMD_FD, buff);
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
int alexa_speech_cmd(int cmd_id, char *cmd_name, char *token)
{
    int ret = -1;
    char *buff = NULL;

    if (!cmd_name || !token) {
        return -1;
    }

    buff = create_speech_cmd(cmd_id, cmd_name, token);
    if (buff) {
        NOTIFY_CMD(ALEXA_CMD_FD, buff);
        free(buff);
        ret = 0;
    }

    if (ret != 0) {
        printf("[%s:%d] failed\n", __FUNCTION__, __LINE__);
    }

    return ret;
}

/*
 * Create speaker Cmd
 * Input :
 *         @cmd_name:
 *         @volume:
 *         @muted:
 *
 * Output: int
 */
int alexa_speeker_cmd(int cmd_id, char *cmd_name, int volume, int muted)
{
    int ret = -1;
    char *buff = NULL;

    if (!cmd_name) {
        return -1;
    }

    buff = create_speeker_cmd(cmd_id, cmd_name, volume, muted);
    if (buff) {
        NOTIFY_CMD(ALEXA_CMD_FD, buff);
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
int alexa_alerts_cmd(int cmd_id, char *cmd_name, const char *token)
{
    int ret = -1;
    char *buff = NULL;

    if (!cmd_name || !token) {
        return -1;
    }

    buff = create_alerts_cmd(cmd_id, cmd_name, token);
    if (buff) {
        NOTIFY_CMD(ALEXA_CMD_FD, buff);
        free(buff);
        ret = 0;
    }

    if (ret != 0) {
        printf("[%s:%d] failed\n", __FUNCTION__, __LINE__);
    }
    return ret;
}
int alexa_donotdisturb_cmd(int cmd_id, char *cmd_name, bool enabled)
{
    int ret = -1;
    char *buff = NULL;
    if (!cmd_name) {
        return -1;
    }

    buff = create_donotdisturb_cmd(cmd_id, cmd_name, enabled);
    if (buff) {
        NOTIFY_CMD(ALEXA_CMD_FD, buff);
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
int alexa_system_cmd(int cmd_id, char *cmd_name, long inactive_ts, char *locale, char *err_msg)
{
    int ret = -1;
    char *buff = NULL;

    if (!cmd_name) {
        return -1;
    }

    buff = create_system_cmd(cmd_id, cmd_name, inactive_ts, locale, err_msg);
    if (buff) {
        NOTIFY_CMD(ALEXA_CMD_FD, buff);
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
int alexa_settings_cmd(int cmd_id, char *cmd_name, char *language)
{
    int ret = -1;
    char *buff = NULL;

    if (!language) {
        return -1;
    }

    buff = create_settings_cmd(cmd_id, cmd_name, language);
    if (buff) {
        NOTIFY_CMD(ALEXA_CMD_FD, buff);
        free(buff);
        ret = 0;
    }

    if (ret != 0) {
        printf("[%s:%d] failed\n", __FUNCTION__, __LINE__);
    }

    return ret;
}

int alexa_string_cmd(char *cmd_prefix, char *string)
{
    if (!cmd_prefix || !string) {
        return -1;
    }

    char *buff = NULL;
    asprintf(&buff, "%s:%s", cmd_prefix, string);
    if (buff) {
        NOTIFY_CMD(ALEXA_CMD_FD, buff);
        free(buff);
        return 0;
    }

    return -1;
}
