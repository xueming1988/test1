#ifndef __RING_BUFFER_H__
#define __RING_BUFFER_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#if defined __cplusplus || defined __cplusplus__
extern "C" {
#endif

typedef struct _ring_buffer_s ring_buffer_t;

ring_buffer_t *RingBufferCreate(size_t len);
void RingBufferDestroy(ring_buffer_t **_this);
void RingBufferReset(ring_buffer_t *_this);
size_t RingBufferWrite(ring_buffer_t *_this, char *data, size_t len);
size_t RingBufferRead(ring_buffer_t *_this, char *data, size_t len);
size_t RingBufferReadableLen(ring_buffer_t *_this);
size_t RingBufferLen(ring_buffer_t *_this);
int RingBufferSetFinished(ring_buffer_t *_this, int finished);
int RingBufferGetFinished(ring_buffer_t *_this);
int RingBufferSetStop(ring_buffer_t *_this, int stop);

#ifdef __cplusplus || defined __cplusplus__
}
#endif

#endif
