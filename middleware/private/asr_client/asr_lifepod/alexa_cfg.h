
#ifndef __ALEXA_CFG_H__
#define __ALEXA_CFG_H__

#ifdef __cplusplus
extern "C" {
#endif

int asr_init_cfg(void);

void asr_set_default_language(int flag);

int asr_get_default_language(void);

int asr_set_language(char *location);

char *asr_get_language(void);

int asr_set_endpoint(char *endpoint);

char *asr_get_endpoint(void);

#ifdef __cplusplus
}
#endif

#endif // __ALEXA_DEBUG_H__
