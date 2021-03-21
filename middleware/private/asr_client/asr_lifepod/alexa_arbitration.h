
#ifndef __ALEXA_ARBITRATION_H__
#define __ALEXA_ARBITRATION_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef X86
#define ESP_DEBUG_OUTPUT_FILE "energyVal.csv"
#define ESP_DEBUG_VADOUT_FILE "vad.txt"
#else
#define ESP_DEBUG_OUTPUT_FILE "/tmp/web/energyVal.csv"
#define ESP_DEBUG_VADOUT_FILE "/tmp/web/vad.txt"
#endif

int alexa_arbitration(char *vadin_name, int *engr_voise, int *engr_noise);

#ifdef __cplusplus
}
#endif

#endif
