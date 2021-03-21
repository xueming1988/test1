#ifndef __HOBOT_XIAOWEI_WWE_H__
#define __HOBOT_XIAOWEI_WWE_H__

#ifdef __cplusplus
extern "C" {
#endif

int CCaptCosume_Asr_Weixin(char *buf, int size, void *priv);
int CCaptCosume_initAsr_Weixin(void *priv);
int CCaptCosume_deinitAsr_Weixin(void *priv);

#ifdef __cplusplus
}
#endif

#endif