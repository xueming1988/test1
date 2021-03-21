/*
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>
#include "ring_buffer.h"

#define ALEXA_RECON_PART_END ("\r\n--this-is-a-boundary--\r\n")

#define RING_BUFFER_LEN (512 * 1024)

struct _ring_buffer_s {
    unsigned char *buf;
    size_t buf_len;
    volatile size_t write_pos;
    volatile size_t read_pos;
    volatile size_t readable_len;
    pthread_mutex_t lock;
    int fin_flags;
};

ring_buffer_t *RingBufferCreate(size_t len)
{
    printf("[%s:%d] ring_buffer_create start\n", __FUNCTION__, __LINE__);

    if (len <= 0) {
        len = RING_BUFFER_LEN;
    }

    ring_buffer_t *ring_buffer = (ring_buffer_t *)calloc(1, sizeof(ring_buffer_t));
    if (ring_buffer) {
        ring_buffer->buf = (char *)calloc(1, len);
        if (!ring_buffer->buf) {
            goto EXIT;
        }
        ring_buffer->buf_len = len;
        ring_buffer->write_pos = 0;
        ring_buffer->read_pos = 0;
        ring_buffer->readable_len = 0;
        ring_buffer->fin_flags = 0;
        pthread_mutex_init(&ring_buffer->lock, NULL);

        return ring_buffer;
    }

EXIT:
    if (ring_buffer) {
        free(ring_buffer);
    }
    printf("[%s:%d] ring_buffer_create error cause by calloc failed\n", __FUNCTION__, __LINE__);
    return NULL;
}

void RingBufferDestroy(ring_buffer_t **_this)
{
    printf("[%s:%d] ring_buffer_destroy start\n", __FUNCTION__, __LINE__);
    if (_this) {
        ring_buffer_t *ring_buffer = *_this;
        if (ring_buffer) {
            if (ring_buffer->buf) {
                free(ring_buffer->buf);
            }
            pthread_mutex_destroy(&ring_buffer->lock);
            free(ring_buffer);
        }
    } else {
        printf("[%s:%d] input error\n", __FUNCTION__, __LINE__);
    }
}

void RingBufferReset(ring_buffer_t *_this)
{
    printf("[%s:%d] circle_buffer_reset start\n", __FUNCTION__, __LINE__);

    if (_this) {
        pthread_mutex_lock(&_this->lock);
        _this->write_pos = 0;
        _this->read_pos = 0;
        _this->readable_len = 0;
        _this->fin_flags = 0;
        pthread_mutex_unlock(&_this->lock);
    } else {
        printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);
    }
}

size_t RingBufferWrite(ring_buffer_t *_this, char *data, size_t len)
{
    size_t write_len = 0;

    if (_this) {
        pthread_mutex_lock(&_this->lock);
        write_len = _this->buf_len - _this->readable_len;
        write_len = write_len > len ? len : write_len;
        if (write_len && _this->fin_flags != 1) {
            write_len = (_this->buf_len - _this->write_pos) > write_len
                            ? write_len
                            : (_this->buf_len - _this->write_pos);

            memcpy(_this->buf + _this->write_pos, data, write_len);
            _this->write_pos = (_this->write_pos + write_len) % _this->buf_len;
            _this->readable_len += write_len;
            pthread_mutex_unlock(&_this->lock);
            return write_len;
        } else {
            printf("[%s:%d] ring buffer is full\n", __FUNCTION__, __LINE__);
            pthread_mutex_unlock(&_this->lock);
            return 0;
        }
    }

    printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);
    return -1;
}

size_t RingBufferRead(ring_buffer_t *_this, char *data, size_t len)
{
    size_t read_len = 0;

    if (_this) {
        pthread_mutex_lock(&_this->lock);
        read_len = (_this->buf_len - _this->read_pos) > _this->readable_len
                       ? _this->readable_len
                       : (_this->buf_len - _this->read_pos);
        read_len = read_len > len ? len : read_len;

        if (read_len > 0) {
            memcpy(data, _this->buf + _this->read_pos, read_len);
            _this->readable_len = _this->readable_len - read_len;
            _this->read_pos = (_this->read_pos + read_len) % _this->buf_len;
            pthread_mutex_unlock(&_this->lock);
            // printf("[--------%s:%d] read size is %d read_pos is %d\n", __FUNCTION__, __LINE__,
            // read_len, _this->read_pos);
            return read_len;
        } else {
            pthread_mutex_unlock(&_this->lock);
            printf("[%s:%d] ring buffer is empty\n", __FUNCTION__, __LINE__);
            return 0;
        }
    }

    printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);
    return -1;
}

size_t RingBufferReadableLen(ring_buffer_t *_this)
{
    if (_this) {
        size_t readable_len = 0;
        pthread_mutex_lock(&_this->lock);
        readable_len = _this->readable_len;
        pthread_mutex_unlock(&_this->lock);
        return readable_len;
    }
    printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);

    return -1;
}

size_t RingBufferLen(ring_buffer_t *_this)
{
    if (_this) {
        size_t buf_len = 0;
        pthread_mutex_lock(&_this->lock);
        buf_len = _this->buf_len;
        pthread_mutex_unlock(&_this->lock);
        return buf_len;
    }
    printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);

    return -1;
}

int RingBufferSetFinished(ring_buffer_t *_this, int finished)
{
    size_t write_len = 0;
    size_t len = 0;
    char *data = NULL;

    if (_this) {
        data = ALEXA_RECON_PART_END;
        len = strlen(ALEXA_RECON_PART_END);
        pthread_mutex_lock(&_this->lock);
        write_len = _this->buf_len - _this->readable_len;
        write_len = write_len > len ? len : write_len;
        if (write_len > 0) {
            write_len = (_this->buf_len - _this->write_pos) > write_len
                            ? write_len
                            : (_this->buf_len - _this->write_pos);
            memcpy(_this->buf + _this->write_pos, data, write_len);
            _this->write_pos = (_this->write_pos + write_len) % _this->buf_len;
            _this->readable_len += write_len;
            //printf("[%s:%d] end boundary write_len(%d) len(%d)\n", \
            //    __FUNCTION__, __LINE__, write_len, len);
        }
        _this->fin_flags = 1;
        pthread_mutex_unlock(&_this->lock);
        return 0;
    }
    printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);

    return -1;
}

int RingBufferSetStop(ring_buffer_t *_this, int stop)
{
    size_t write_len = 0;
    size_t len = 0;
    char *data = NULL;

    if (_this) {
        data = ALEXA_RECON_PART_END;
        len = strlen(ALEXA_RECON_PART_END);
        pthread_mutex_lock(&_this->lock);

        _this->write_pos = 0;
        _this->read_pos = 0;
        _this->readable_len = 0;

        write_len = _this->buf_len - _this->readable_len;
        write_len = write_len > len ? len : write_len;
        if (write_len > 0) {
            write_len = (_this->buf_len - _this->write_pos) > write_len
                            ? write_len
                            : (_this->buf_len - _this->write_pos);
            memcpy(_this->buf + _this->write_pos, data, write_len);
            _this->write_pos = (_this->write_pos + write_len) % _this->buf_len;
            _this->readable_len += write_len;
            //printf("[%s:%d] end boundary write_len(%d) len(%d)\n", \
            //    __FUNCTION__, __LINE__, write_len, len);
        }
        _this->fin_flags = 1;
        pthread_mutex_unlock(&_this->lock);
        return 0;
    }
    printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);

    return -1;
}

int RingBufferGetFinished(ring_buffer_t *_this)
{
    if (_this) {
        int finished = 0;
        pthread_mutex_lock(&_this->lock);
        finished = _this->fin_flags;
        pthread_mutex_unlock(&_this->lock);
        return finished;
    }
    printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);

    return -1;
}
