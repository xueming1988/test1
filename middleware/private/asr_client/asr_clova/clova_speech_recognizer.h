#ifndef __CLOVA_SPEECH_RECOGNIZER__
#define __CLOVA_SPEECH_RECOGNIZER__

void clova_explicit_init(void);

void clova_explicit_uninit(void);

void clova_set_speechId(const char *value);

char *clova_get_speechId(void);

void clova_set_explicit(int value);

int clova_get_explicit(void);

#endif
