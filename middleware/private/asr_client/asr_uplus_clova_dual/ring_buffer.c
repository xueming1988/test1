
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ring_buffer.h"

#define ASR_RECON_PART_END ("\r\n----abcdefg123456--\r\n")

#if MAX_RECORD_TIME_IN_MS
#define RING_BUFFER_LEN ((MAX_RECORD_TIME_IN_MS)*32 + 10240)
#else
#define RING_BUFFER_LEN (512 * 1024)
#endif

struct _ring_buffer_s {
    char *buf;
    size_t buf_len;
    volatile size_t write_pos;
    volatile size_t read_pos;
    volatile size_t readable_len;
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

        printf("[%s:%d] ring_buffer_create succeed\n", __FUNCTION__, __LINE__);
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
            free(ring_buffer);
            printf("[%s:%d] ring_buffer_destroy succeed\n", __FUNCTION__, __LINE__);
        }
    } else {
        printf("[%s:%d] input error\n", __FUNCTION__, __LINE__);
    }
}

void RingBufferReset(ring_buffer_t *_this)
{
    if (_this) {
        _this->write_pos = 0;
        _this->read_pos = 0;
        _this->readable_len = 0;
        _this->fin_flags = 0;
    } else {
        printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);
    }
}

size_t RingBufferWrite(ring_buffer_t *_this, char *data, size_t len)
{
    size_t write_len = 0;
    size_t end_len = 0;

    if (_this) {
        write_len = _this->buf_len - _this->readable_len;
        write_len = write_len > len ? len : write_len;
        end_len = _this->buf_len - _this->write_pos;

        if (write_len && _this->fin_flags != 1) {
            if (write_len <= end_len) {
                memcpy(_this->buf + _this->write_pos, data, write_len);
            } else {
                memcpy(_this->buf + _this->write_pos, data, end_len);
                memcpy(_this->buf, data + end_len, write_len - end_len);
            }
            _this->write_pos = (_this->write_pos + write_len) % _this->buf_len;
            _this->readable_len += write_len;

            return write_len;
        } else {
            printf("[%s:%d] ring buffer is full\n", __FUNCTION__, __LINE__);
            return 0;
        }
    }

    printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);
    return -1;
}

size_t RingBufferRead(ring_buffer_t *_this, char *data, size_t len)
{
    size_t read_len = 0;
    size_t end_len = 0;

    if (_this) {
        read_len = _this->readable_len;
        read_len = read_len > len ? len : read_len;
        end_len = _this->buf_len - _this->read_pos;

        if (read_len > 0) {
            if (read_len <= end_len) {
                memcpy(data, _this->buf + _this->read_pos, read_len);
            } else {
                memcpy(data, _this->buf + _this->read_pos, end_len);
                memcpy(data + end_len, _this->buf, read_len - end_len);
            }
            _this->readable_len -= read_len;
            _this->read_pos = (_this->read_pos + read_len) % _this->buf_len;

            return read_len;
        } else {
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
        return _this->readable_len;
    }

    printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);
    return -1;
}

size_t RingBufferLen(ring_buffer_t *_this)
{
    if (_this) {
        return _this->buf_len;
    }

    printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);
    return -1;
}

int RingBufferSetFinished(ring_buffer_t *_this, int finished)
{
    size_t write_len = 0;
    size_t end_len = 0;
    size_t len = strlen(ASR_RECON_PART_END);
    char *data = ASR_RECON_PART_END;

    if (_this) {
        write_len = _this->buf_len - _this->readable_len;
        write_len = write_len > len ? len : write_len;
        end_len = _this->buf_len - _this->write_pos;

        if (write_len > 0) {
            if (write_len <= end_len) {
                memcpy(_this->buf + _this->write_pos, data, write_len);
            } else {
                memcpy(_this->buf + _this->write_pos, data, end_len);
                memcpy(_this->buf, data + end_len, write_len - end_len);
            }
            _this->write_pos = (_this->write_pos + write_len) % _this->buf_len;
            _this->readable_len += write_len;
        }
        _this->fin_flags = 1;

        return 0;
    }

    printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);
    return -1;
}

int RingBufferSetStop(ring_buffer_t *_this, int stop)
{
    size_t write_len = 0;
    size_t end_len = 0;
    size_t len = strlen(ASR_RECON_PART_END);
    char *data = ASR_RECON_PART_END;

    if (_this) {
        _this->write_pos = 0;
        _this->read_pos = 0;
        _this->readable_len = 0;
        write_len = _this->buf_len - _this->readable_len;
        write_len = write_len > len ? len : write_len;
        end_len = _this->buf_len - _this->write_pos;

        if (write_len > 0) {
            if (write_len <= end_len) {
                memcpy(_this->buf + _this->write_pos, data, write_len);
            } else {
                memcpy(_this->buf + _this->write_pos, data, end_len);
                memcpy(_this->buf, data + end_len, write_len - end_len);
            }
            _this->write_pos = (_this->write_pos + write_len) % _this->buf_len;
            _this->readable_len += write_len;
        }
        _this->fin_flags = 1;

        return 0;
    }

    printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);
    return -1;
}

int RingBufferGetFinished(ring_buffer_t *_this)
{
    if (_this) {
        return _this->fin_flags;
    }

    printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);
    return -1;
}
