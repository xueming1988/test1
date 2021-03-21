
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "alexa_recognize.h"

#ifndef RAND_STR_LEN
#define RAND_STR_LEN (36)
#endif

typedef struct {
    int state;

    pthread_mutex_t state_lock;

    int64_t start_sample;
    int64_t end_sample;

    char dialog_id[RAND_STR_LEN + 1];

} avs_reco_state_t;

static avs_reco_state_t g_reco_state;

int avs_recognize_init(void)
{
    memset(&g_reco_state, 0x0, sizeof(avs_reco_state_t));
    pthread_mutex_init(&g_reco_state.state_lock, NULL);
}

int avs_recognize_uninit(void) { pthread_mutex_destroy(&g_reco_state.state_lock); }

int avs_recognize_parse_directive(json_object *js_obj) {}

int avs_recognize_state(json_object *json_context_list) {}

int avs_recognize_get_state(void)
{
    int flag = 0;

    pthread_mutex_lock(&g_reco_state.state_lock);
    flag = g_reco_state.state;
    pthread_mutex_unlock(&g_reco_state.state_lock);

    return flag;
}

int avs_recognize_set_state(int flags)
{
    pthread_mutex_lock(&g_reco_state.state_lock);
    g_reco_state.state = flags;
    pthread_mutex_unlock(&g_reco_state.state_lock);

    return 0;
}
