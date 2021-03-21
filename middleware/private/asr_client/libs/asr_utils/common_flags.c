

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "common_flags.h"

#define ASR_LOGIN_OK ("AlexaLoginSuccess")
#define NET_GUARD ("/tmp/RequestNetguard")

static char g_ssid[128] = {0};
WIIMU_CONTEXT *g_wiimu_shm = NULL;
#ifdef SOFT_PREP_MODULE
extern int CAIMan_soft_prep_init(void);
extern void CAIMan_soft_prep_deinit(void);
#endif

int common_flags_init(void)
{
    g_wiimu_shm = WiimuContextGet();
    if (g_wiimu_shm) {
        g_wiimu_shm->dnd_mode = 0;
        g_wiimu_shm->asr_ongoing = 0;
        g_wiimu_shm->asr_pause_request = 0;
        g_wiimu_shm->asr_status = 0;
        g_wiimu_shm->alexa_response_status = 0;
    }

    return 0;
}

void *common_flags_get(void)
{
    if (g_wiimu_shm) {
        return (void *)g_wiimu_shm;
    }

    return NULL;
}

void common_falgs_uninit(void)
{
    int i = 0;
    if (g_wiimu_shm) {
        for (i = 0; i < SNDSRC_MAX_NUMB; i++) {
            g_wiimu_shm->snd_ctrl[i].volume[SNDSRC_ALEXA_TTS] = 100;
            g_wiimu_shm->snd_ctrl[i].volume[SNDSRC_ASR] = 100;
        }
        g_wiimu_shm->snd_ctrl[SNDSRC_ALEXA_TTS].mute = 1;
    }
#if defined(SOFT_PREP_MODULE)
// CAIMan_soft_prep_deinit();
#endif
}

int get_internet_flags(void)
{
    if (g_wiimu_shm) {
        if (g_wiimu_shm->netstat != NET_CONN || g_wiimu_shm->internet == 0)
            return 0;
        else
            return g_wiimu_shm->internet;
    }

    return 0;
}

int get_network_setup_status(void)
{
    if (g_wiimu_shm) {
        return g_wiimu_shm->ezlink_active;
    }

    return 0;
}

int get_asr_talk_start(void)
{
    if (g_wiimu_shm) {
        return g_wiimu_shm->asr_ongoing;
    }

    return 0;
}

void set_asr_talk_start(int val)
{
    if (g_wiimu_shm) {
        g_wiimu_shm->asr_ongoing = val;
    }
}

int get_asr_online(void)
{
    if (g_wiimu_shm) {
        return g_wiimu_shm->alexa_online;
    }

    return -1;
}

void set_asr_online_only(int val)
{
    if (g_wiimu_shm) {
        g_wiimu_shm->alexa_online = val;
    }
}

void set_asr_online(int val)
{
    if (g_wiimu_shm) {
        g_wiimu_shm->alexa_online = val;
    }
    if (val > 0) {
        SocketClientReadWriteMsg(NET_GUARD, (char *)ASR_LOGIN_OK, strlen((char *)ASR_LOGIN_OK),
                                 NULL, NULL, 0);
    }
}

int get_dnd_mode(void)
{
    if (g_wiimu_shm) {
        return ((g_wiimu_shm->dnd_mode != 0) ? 1 : 0);
    }

    return 0;
}

void set_dnd_mode(int val)
{
    if (g_wiimu_shm) {
        if (val != 0) {
            g_wiimu_shm->dnd_mode = 1;
        } else {
            g_wiimu_shm->dnd_mode = 0;
        }
    }
}

int get_asr_have_prompt(void)
{
    if (g_wiimu_shm) {
        if (g_wiimu_shm->asr_have_prompt == 1) {
            g_wiimu_shm->smplay_skip_silence = 1;
        }
        return g_wiimu_shm->asr_have_prompt;
    }

    return 0;
}

int get_asr_alert_status(void)
{
    if (g_wiimu_shm) {
        return g_wiimu_shm->alert_status;
    }

    return -1;
}

void update_vad_timeout_count(void)
{
    if (g_wiimu_shm) {
        g_wiimu_shm->times_vad_time_out++;
    }
}

void update_talk_count(void)
{
    if (g_wiimu_shm) {
        g_wiimu_shm->times_talk_on++;
        fprintf(stderr, "do_talkstart req_from=%d", g_wiimu_shm->times_talk_on);
    }
}

int get_asr_profile(void)
{
    if (g_wiimu_shm) {
        if (far_field_judge(g_wiimu_shm)) {
            return FAR_FIELD;
        } else {
            return CLOSE_TALK;
        }
    }

    return -1;
}

const unsigned char *get_wifi_mac_addr(void)
{
    if (g_wiimu_shm) {
        return (unsigned char *)g_wiimu_shm->mac;
    }

    return NULL;
}

const char *get_wifi_ssid(void)
{
    if (g_wiimu_shm && strlen(g_wiimu_shm->essid)) {
        memset(g_ssid, 0x0, sizeof(g_ssid));
        hex2ascii(g_wiimu_shm->essid, g_ssid, strlen(g_wiimu_shm->essid), sizeof(g_ssid));
        return g_ssid;
    }

    return NULL;
}

const char *get_bletooth_name(void)
{
    if (g_wiimu_shm && strlen(g_wiimu_shm->ssid)) {
        return g_wiimu_shm->ssid;
    }

    return NULL;
}

const char *get_device_sn(void)
{
    if (g_wiimu_shm) {
        return (char *)g_wiimu_shm->dev_sn;
    }

    return NULL;
}

const char *get_device_id(void)
{
    if (g_wiimu_shm) {
        return (char *)g_wiimu_shm->dev_id;
    }

    return NULL;
}

const char *get_model_id(void)
{
    if (g_wiimu_shm) {
        return (char *)g_wiimu_shm->dev_model_name;
    }

    return NULL;
}

const char *get_device_ruid(void)
{
    if (g_wiimu_shm) {
        return (char *)g_wiimu_shm->dev_ruid;
    }

    return NULL;
}

const char *get_device_firmware_version(void)
{
    if (g_wiimu_shm) {
        return (char *)g_wiimu_shm->release_version;
    }

    return NULL;
}

int get_device_mcu_version(void)
{
    if (g_wiimu_shm) {
        return g_wiimu_shm->mcu_ver;
    }

    return 0;
}

const char *get_device_dsp_version(void)
{
    if (g_wiimu_shm) {
        return (char *)g_wiimu_shm->asr_dsp_fw_ver;
    }

    return NULL;
}

int get_keyword_type(void)
{
    if (g_wiimu_shm) {
        return g_wiimu_shm->trigger1_keyword_index;
    }

    return 0;
}

void restore_volume_to_factory_settings(void)
{
    int factory_volume = 53;
    char buf[128] = {0};

    snprintf(buf, sizeof(buf), "setPlayerCmd:vol:%d", factory_volume);
    SocketClientReadWriteMsg("/tmp/Requesta01controller", buf, strlen(buf), 0, 0, 0);
}

void set_alarm_sound(const char *alarm_sound)
{
    int index = 0;
    char *ptr = NULL;

    if (alarm_sound) {
        ptr = strstr(alarm_sound, "AlarmSound");
        if (ptr) {
            ptr += strlen("AlarmSound");
            index = atoi(ptr);
            g_wiimu_shm->alarm_sound = index;
        }
    }
}

int get_alarm_sound(void) { return g_wiimu_shm->alarm_sound; }

void restore_system_volume(int flag)
{
    if (g_wiimu_shm->volume_bk != -1) {
        if (flag || (!flag && !g_wiimu_shm->alert_status)) {
            char buff[128] = {0};
            printf("========== volume_bk = %d volume = %d ==============\n", g_wiimu_shm->volume_bk,
                   g_wiimu_shm->volume);
            snprintf(buff, sizeof(buff), "setPlayerCmd:vol:%d", g_wiimu_shm->volume_bk);
            SocketClientReadWriteMsg("/tmp/Requesta01controller", buff, strlen(buff), 0, 0, 0);
            g_wiimu_shm->volume_bk = -1;
        }
    }
}

void set_alert_volume(const char *alert_volume)
{
    unsigned char vol_step[16] = {0, 6, 13, 20, 27, 33, 40, 47, 53, 60, 67, 73, 80, 87, 93, 100};
    int index;

    if (alert_volume) {
        index = atoi(alert_volume);
        g_wiimu_shm->alert_volume = vol_step[index];
    }
}

void apply_alert_volume(void)
{
    if (g_wiimu_shm->volume_bk == -1)
        g_wiimu_shm->volume_bk = g_wiimu_shm->volume;

    char buff[128] = {0};
    snprintf(buff, sizeof(buff), "setPlayerCmd:vol:%d", g_wiimu_shm->alert_volume);
    SocketClientReadWriteMsg("/tmp/Requesta01controller", buff, strlen(buff), 0, 0, 0);
}

int get_backup_volume(void)
{
    if (g_wiimu_shm) {
        return g_wiimu_shm->volume_bk;
    }

    return 0;
}

void init_bk_volume(void) { g_wiimu_shm->volume_bk = -1; }

int get_asr_have_token(void)
{
    if (g_wiimu_shm) {
        return g_wiimu_shm->has_asr_token;
    }

    return 0;
}

void set_asr_have_token(int val)
{
    if (g_wiimu_shm) {
        g_wiimu_shm->has_asr_token = val;
    }
}

void set_block_play_flag(void)
{
    if (g_wiimu_shm) {
        g_wiimu_shm->ignore_btn_event = 1;
    }
}

void clear_block_play_flag(void)
{
    if (g_wiimu_shm && g_wiimu_shm->ignore_btn_event == 1) {
        g_wiimu_shm->ignore_btn_event = 0;
    }
}

int get_net_work_reset_flag(void)
{
    if (g_wiimu_shm) {
        return g_wiimu_shm->ezlink_active;
    }

    return 0;
}

int check_is_bt_play_mode(void)
{
    if (g_wiimu_shm) {
        return (g_wiimu_shm->play_mode == PLAYER_MODE_BT);
    }

    return 0;
}

void set_play_mode(int mode)
{
    if (g_wiimu_shm) {
        if (g_wiimu_shm->play_mode != mode)
            g_wiimu_shm->play_mode = mode;
    }
}

int check_focus_on_tts(void)
{
    int cur_focus = -1;

    if (g_wiimu_shm) {
        cur_focus = WMUtil_Snd_Ctrl_Current_Focus(g_wiimu_shm);
    }

    return (cur_focus == SNDSRC_ALEXA_TTS);
}

int check_force_ota(void)
{
    if (g_wiimu_shm) {
        return g_wiimu_shm->ota_upgrade_force;
    }

    return 0;
}

int get_ota_block_status(void)
{
    if (g_wiimu_shm) {
        return g_wiimu_shm->ota_block_disabled;
    }

    return 1;
}

void common_focus_manage_set(e_focus_type_t type)
{
    if (g_wiimu_shm) {
        focus_manage_set(g_wiimu_shm, type);
    }
}

void common_focus_manage_clear(e_focus_type_t type)
{
    if (g_wiimu_shm) {
        focus_manage_clear(g_wiimu_shm, type);
    }
}

void common_focus_manage_set_bg(e_focus_type_t type)
{
    if (g_wiimu_shm) {
        focus_manage_set_bg(g_wiimu_shm, type);
    }
}

void release_button_block(void)
{
    if (g_wiimu_shm) {
        g_wiimu_shm->button_block = 0;
    }
}

void set_session_state(int val)
{
    if (g_wiimu_shm) {
        g_wiimu_shm->asr_status = val;
    }
}

int get_session_state(void)
{
    if (g_wiimu_shm) {
        return g_wiimu_shm->asr_status;
    }

    return 0;
}

int get_device_status(void)
{
    if (g_wiimu_shm)
        return g_wiimu_shm->device_status;

    return 0;
}

int get_focus_state(int index)
{
    if (g_wiimu_shm)
        return g_wiimu_shm->snd_enable[index];

    return -1;
}

int get_bt_sound_path(void)
{
    if (g_wiimu_shm) {
        return g_wiimu_shm->bt_av_audio_path;
    }

    return 0;
}

void set_guide_status(int status)
{
    if (g_wiimu_shm) {
        g_wiimu_shm->guide_playing = status;
    }
}

int get_guide_status(void)
{
    if (g_wiimu_shm) {
        return g_wiimu_shm->guide_playing;
    }

    return 0;
}
