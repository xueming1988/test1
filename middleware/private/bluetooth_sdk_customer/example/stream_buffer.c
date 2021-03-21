
#include "stream_buffer.h"
#include <string.h>
#include <stdio.h>

int stream_buffer_init(stream_buffer *pbuffer, int node_size, int node_num)
{
    int i;
    stream_node node;

    memset(pbuffer, 0, sizeof(stream_buffer));
    pbuffer->mem = malloc(node_size * node_num);
    if (pbuffer->mem == 0) {
        printf("malloc(node_size*node_num) failed!\n");
        return -1;
    }
    bufferInit(pbuffer->ring, node_num, stream_node);
    if (pbuffer->ring.elems == 0) {
        printf("malloc  ring  element failed!\n");
        return -1;
    }
    for (i = 0; i < node_num; i++) {
        node.len = 0;
        node.buffer = pbuffer->mem + i * node_size;
        bufferElemInit(&pbuffer->ring, node, i);
    }
    return 0;
}

int stream_buffer_get_size(stream_buffer *pbuffer) { return bufferSize(&(pbuffer->ring)); }

stream_node *stream_buffer_get_write_node(stream_buffer *pbuffer)
{
    stream_node *ret = NULL;
    if (isBufferFull(&pbuffer->ring)) {
        return ret;
    }
    bufferWriteElem(&pbuffer->ring, ret);
    return ret;
}

stream_node *stream_buffer_get_read_node(stream_buffer *pbuffer)
{
    stream_node *ret = NULL;
    if (isBufferEmpty(&pbuffer->ring)) {
        return ret;
    }
    bufferReadElem(&pbuffer->ring, ret);
    return ret;
}

int stream_buffer_write_commit(stream_buffer *pbuffer)
{
    if (isBufferFull(&pbuffer->ring)) {
        return -1;
    }

    writeIndexMove(&pbuffer->ring);
    return 0;
}

int stream_buffer_read_commit(stream_buffer *pbuffer)
{
    if (isBufferEmpty(&pbuffer->ring)) {
        return -1;
    }
    readIndexMove(&pbuffer->ring);
    return 0;
}

void stream_buffer_destroy(stream_buffer *pbuffer)
{

    bufferDestroy(&(pbuffer->ring));
    if (pbuffer->mem) {
        free(pbuffer->mem);
        pbuffer->mem = 0;
    }
    memset(pbuffer, 0, sizeof(stream_buffer));
}
