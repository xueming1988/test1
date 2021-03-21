#ifndef __ASR_CONFIG_H__
#define __ASR_CONFIG_H__

int asr_config_get(char *param, char *value, int value_len);
int asr_config_set(char *param, char *value);
int asr_config_set_sync(char *param, char *value);
void asr_config_sync(void);

#endif /* __ASR_CONFIG_H__ */
