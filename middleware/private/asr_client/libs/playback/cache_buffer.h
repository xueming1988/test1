/*************************************************************************
    > File Name: cache_buffer.h
    > Author:
    > Mail:
    > Created Time: Tue 21 Nov 2017 02:35:55 AM EST
    > This buffer is for cache the raw data, when the buffer size is
      not enough, it can increase itself, and have it's status
 ************************************************************************/

#ifndef __CACHE_BUFFER_H__
#define __CACHE_BUFFER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CACHE_BUFFER_STEP (32 * 1024)
#define CACHE_BUFFER_LEN (32 * 1024)

typedef enum {
    e_can_read,
    e_cache_finished,
    e_cache_stop,
} e_cache_state_t;

typedef struct _cache_buffer_s cache_buffer_t;

cache_buffer_t *cache_buffer_create(size_t len, size_t step);

void cache_buffer_destroy(cache_buffer_t **self);

void cache_buffer_reset(cache_buffer_t *self);

int cache_buffer_write(cache_buffer_t *self, char *data, size_t len);

int cache_buffer_read(cache_buffer_t *self, char *data, size_t len, e_cache_state_t *state);

int cache_buffer_len(cache_buffer_t *self);

int cache_buffer_set_state(cache_buffer_t *self, e_cache_state_t state);

#ifdef __cplusplus
}
#endif

#endif
