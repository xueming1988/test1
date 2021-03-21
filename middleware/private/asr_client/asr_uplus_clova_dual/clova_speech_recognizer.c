#include <stdio.h>
#include <stdlib.h>

#include "clova_speech_recognizer.h"

typedef struct {
    int explicit;
    char *speechId;

    pthread_mutex_t mutex;
} clova_speech_recognizer_t;

clova_speech_recognizer_t g_speech_recognizer = {0};

void clova_explicit_init(void)
{
    memset(&g_speech_recognizer, 0x0, sizeof(clova_speech_recognizer_t));

    pthread_mutex_init(&g_speech_recognizer.mutex, NULL);
}

void clova_explicit_uninit(void)
{
    pthread_mutex_lock(&g_speech_recognizer.mutex);
    if (g_speech_recognizer.speechId != NULL) {
        free(g_speech_recognizer.speechId);
        g_speech_recognizer.speechId = NULL;
    }
    pthread_mutex_unlock(&g_speech_recognizer.mutex);

    pthread_mutex_destroy(&g_speech_recognizer.mutex);
}

void clova_set_explicit(int value)
{
    pthread_mutex_lock(&g_speech_recognizer.mutex);
    g_speech_recognizer.explicit = value;
    pthread_mutex_unlock(&g_speech_recognizer.mutex);
}

int clova_get_explicit(void)
{
    int value = 0;

    pthread_mutex_lock(&g_speech_recognizer.mutex);
    value = g_speech_recognizer.explicit;
    pthread_mutex_unlock(&g_speech_recognizer.mutex);

    return value;
}

void clova_set_speechId(const char *value)
{
    pthread_mutex_lock(&g_speech_recognizer.mutex);
    if (g_speech_recognizer.speechId != NULL) {
        free(g_speech_recognizer.speechId);
        g_speech_recognizer.speechId = NULL;
    }
    if (value) {
        g_speech_recognizer.speechId = strdup(value);
    }
    pthread_mutex_unlock(&g_speech_recognizer.mutex);
}

char *clova_get_speechId(void)
{
    char *speechId = NULL;

    pthread_mutex_lock(&g_speech_recognizer.mutex);
    if (g_speech_recognizer.speechId) {
        speechId = strdup(g_speech_recognizer.speechId);
    }
    pthread_mutex_unlock(&g_speech_recognizer.mutex);

    return speechId;
}
