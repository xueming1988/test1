
#ifndef __ASR_BLUETOOTH_H__
#define __ASR_BLUETOOTH_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct json_object json_object;

int asr_bluetooth_init(void);

int asr_bluetooth_uninit(void);

void asr_ble_getpaired_list(void);

// parse directive
int asr_bluetooth_parse_directive(json_object *js_data);

void asr_bluetooth_get_tts(int tts_type);

void asr_bluetooth_reset(void);

// parse the cmd from bluetooth thread to asr client
// TODO: need free after used
char *asr_bluetooth_parse_cmd(char *json_cmd_str);

int asr_bluetooth_state(json_object *json_payload);

int asr_bluetooth_switch(int on_ff_status);

int asr_bluetooth_sound_path_change(int path_flags);

void asr_bluetooth_disconnect(int prepare);

void asr_bluetooth_clear_active(void);

#ifdef __cplusplus
}
#endif

#endif
