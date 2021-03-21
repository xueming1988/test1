
#ifndef __ASR_CFG_H__
#define __ASR_CFG_H__

enum {
    e_mode_off,
    e_mode_on,
};

#ifdef __cplusplus
extern "C" {
#endif

int asr_config_get(char *param, char *value, int value_len);

int asr_config_set(char *param, char *value);

int asr_config_set_sync(char *param, char *value);

void asr_config_sync(void);

int asr_init_cfg(void);

void asr_set_default_language(int flag);

int asr_get_default_language(void);

int asr_set_language(char *location);

char *asr_get_language();

int asr_set_endpoint(char *endpoint);

char *asr_get_endpoint(void);

int asr_set_last_paired_ble(char *uuid, char *friendly_name, int want_clear);

int asr_get_last_paired_ble_uuid(char *ble_uuid, int len);

int asr_get_last_paired_ble_name(char *ble_name, int len);

int asr_set_handle_free_mode(int on_off);

int asr_set_privacy_mode(int on_off);

int asr_get_privacy_mode(void);

int set_asr_trigger1_keyword_index(int index);

#ifdef __cplusplus
}
#endif

#endif // __ALEXA_DEBUG_H__
