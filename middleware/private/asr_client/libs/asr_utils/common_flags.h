
/**
 *
 *
 *
 *
 *
 */

#ifndef __COMMON_FLAGS_H__
#define __COMMON_FLAGS_H__

#include <stdio.h>
#include <string.h>
#include "wm_util.h"

enum {
    CLOSE_TALK,
    NEAR_FIELD,
    FAR_FIELD,
};

#ifdef __cplusplus
extern "C" {
#endif

int common_flags_init(void);

void *common_flags_get(void);

void common_falgs_uninit(void);

int get_internet_flags(void);

int get_network_setup_status(void);

int get_asr_talk_start(void);

void set_asr_talk_start(int val);

int get_asr_online(void);

void set_asr_online(int val);

int get_dnd_mode(void);

void set_dnd_mode(int val);

int get_asr_have_prompt(void);

int get_asr_alert_status(void);

void update_vad_timeout_count(void);

void update_talk_count(void);

/*
 * Check the device is far feild moudle or not
 * Input :
 *         @
 * Output:
 *           0 : close talk moudle
 *           1 : near feild moudle
 *           2 : far feild moudle
 */
int get_asr_profile(void);

const unsigned char *get_wifi_mac_addr(void);

const char *get_wifi_ssid(void);

const char *get_bletooth_name(void);

const char *get_device_id(void);

const char *get_device_dsp_version(void);

int get_device_mcu_version(void);

const char *get_device_firmware_version(void);

int get_asr_have_token(void);

void set_asr_have_token(int val);

void set_block_play_flag(void);

void clear_block_play_flag(void);

int get_net_work_reset_flag(void);

int check_is_bt_play_mode(void);

int check_focus_on_tts(void);

void common_focus_manage_set(e_focus_type_t type);

void common_focus_manage_clear(e_focus_type_t type);

void common_focus_manage_set_bg(e_focus_type_t type);

#ifdef __cplusplus
}
#endif

#endif
