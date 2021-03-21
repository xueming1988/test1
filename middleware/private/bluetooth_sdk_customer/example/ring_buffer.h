#ifndef _ringbuffer_h
#define _ringbuffer_h
#include <stdlib.h>

#define ringBuffer_typedef(STRUCTR_NAME, TYPE)                                                     \
    typedef struct {                                                                               \
        int size;                                                                                  \
        int start;                                                                                 \
        int end;                                                                                   \
        int curSize;                                                                               \
        TYPE *elems;                                                                               \
    } STRUCTR_NAME

#define bufferInit(BUF, SIZE, TYPE)                                                                \
    BUF.size = SIZE;                                                                               \
    BUF.start = 0;                                                                                 \
    BUF.end = 0;                                                                                   \
    BUF.curSize = 0;                                                                               \
    BUF.elems = (TYPE *)calloc(BUF.size, sizeof(TYPE))

#define bufferDestroy(pBUF)                                                                        \
    if ((pBUF)->elems) {                                                                           \
        free((pBUF)->elems);                                                                       \
        (pBUF)->elems = 0;                                                                         \
    }
#define nextStartIndex(pBUF) (((pBUF)->start + 1) % ((pBUF)->size))
#define nextEndIndex(pBUF) (((pBUF)->end + 1) % ((pBUF)->size))
#define isBufferEmpty(pBUF) ((pBUF)->curSize == 0)
#define isBufferFull(pBUF) ((pBUF)->curSize == (pBUF)->size)

#define bufferSize(pBUF) ((pBUF)->curSize)

#define bufferElemInit(pBUF, ELEM, i) (pBUF)->elems[i] = ELEM

#define bufferWriteElem(pBUF, pELEM) pELEM = &((pBUF)->elems[(pBUF)->end])

#define bufferReadElem(pBUF, pELEM) pELEM = &((pBUF)->elems[(pBUF)->start])

#define writeIndexMove(pBUF)                                                                       \
    if (!isBufferFull(pBUF)) {                                                                     \
        (pBUF)->end = nextEndIndex(pBUF);                                                          \
        (pBUF)->curSize = (pBUF)->curSize + 1;                                                     \
    }

#define readIndexMove(pBUF)                                                                        \
    if (!isBufferEmpty(pBUF)) {                                                                    \
        (pBUF)->start = nextStartIndex((pBUF));                                                    \
        (pBUF)->curSize = (pBUF)->curSize - 1;                                                     \
    }

#define bufferWrite(pBUF, ELEM)                                                                    \
    if (!isBufferFull(pBUF)) {                                                                     \
        (pBUF)->elems[(pBUF)->end] = ELEM;                                                         \
        (pBUF)->end = nextEndIndex(pBUF);                                                          \
        (pBUF)->curSize = (pBUF)->curSize + 1;                                                     \
    }

#define bufferRead(pBUF, ELEM)                                                                     \
    if (!isBufferEmpty(pBUF)) {                                                                    \
        ELEM = (pBUF)->elems[(pBUF)->start];                                                       \
        (pBUF)->start = nextStartIndex((pBUF));                                                    \
        (pBUF)->curSize = (pBUF)->curSize - 1;                                                     \
    }

#endif
