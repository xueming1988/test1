#ifndef _stream_buffer_h
#define _stream_ringbuffer_h

#include "ring_buffer.h"

typedef struct _stream_node {
    unsigned int len;
    char *buffer;
} stream_node;

/* 定义结构体 stream_ring_buffer  */
ringBuffer_typedef(stream_ring_buffer, stream_node);

typedef struct _stream_buffer {
    stream_ring_buffer ring;
    char *mem;
} stream_buffer;

int stream_buffer_init(stream_buffer *pbuffer, int node_size, int node_num);
void stream_buffer_destroy(stream_buffer *pbuffer);
int stream_buffer_get_size(stream_buffer *pbuffer);
stream_node *stream_buffer_get_write_node(stream_buffer *pbuffer);
stream_node *stream_buffer_get_read_node(stream_buffer *pbuffer);
int stream_buffer_write_commit(stream_buffer *pbuffer);
int stream_buffer_read_commit(stream_buffer *pbuffer);
#endif