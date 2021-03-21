
#ifndef __ALEXA_ARBITRATION_H__
#define __ALEXA_ARBITRATION_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifdef X86
#define ALEXA_VADVAL_FILE "vad.txt"
#define ALEXA_VADOUT_FILE "vadout.txt"
#define ALEXA_HPFILTERED_FILE "hpfiltered.pcm"
#else
#define ALEXA_VADVAL_FILE "/tmp/web/vad.txt"
#define ALEXA_VADOUT_FILE "/tmp/web/vadout.txt"
#define ALEXA_HPFILTERED_FILE "/tmp/web/hpfiltered.pcm"
#endif

int alexa_arbitration(char *vadin_name, unsigned int *engr_voise, unsigned int *engr_noise);

#ifdef __cplusplus
}
#endif

#endif
