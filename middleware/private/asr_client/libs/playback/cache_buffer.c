/*************************************************************************
    > File Name: cache_buffer.c
    > Author:
    > Mail:
    > Created Time: Tue 21 Nov 2017 02:35:55 AM EST
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

#include "cache_buffer.h"

#if defined(NAVER_LINE)
#include "cid_player_naver.h"
#endif

#define MIN(a, b) (((a) > (b)) ? (b) : (a))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define DEBUG_INFO(fmt, ...)
#define DEBUG_ERROR(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)

#define os_calloc(n, size) calloc((n), (size))
#define os_free(ptr) free((ptr))

struct _cache_buffer_s {
    volatile int state;
    volatile int step;
    volatile int buf_len;
    volatile int write_pos;
    volatile int read_pos;
    volatile int readable_len;
    pthread_mutex_t lock;
    unsigned char *buf;
};

cache_buffer_t *cache_buffer_create(size_t len, size_t step)
{
    DEBUG_INFO("ring_buffer_create start\n");

    if (len <= 0) {
        len = CACHE_BUFFER_LEN;
    }

    cache_buffer_t *self = (cache_buffer_t *)os_calloc(1, sizeof(cache_buffer_t));
    if (self) {
        self->buf = (unsigned char *)os_calloc(1, len);
        if (!self->buf) {
            goto EXIT;
        }
        self->buf_len = (int)len;
        self->write_pos = 0;
        self->read_pos = 0;
        self->readable_len = 0;
        self->state = e_can_read;
        self->step = step;
        pthread_mutex_init(&self->lock, NULL);

        return self;
    }

EXIT:

    if (self) {
        os_free(self);
    }
    DEBUG_ERROR("ring_buffer_create error cause by calloc failed\n");
    return NULL;
}

void cache_buffer_destroy(cache_buffer_t **self_p)
{
    cache_buffer_t *self = *self_p;

    if (self) {
        pthread_mutex_lock(&self->lock);
        if (self->buf) {
            os_free(self->buf);
            self->buf = NULL;
        }
        pthread_mutex_unlock(&self->lock);
        pthread_mutex_destroy(&self->lock);
        os_free(self);
        *self_p = NULL;
    } else {
        DEBUG_ERROR("input error\n");
    }
}

void cache_buffer_reset(cache_buffer_t *self)
{
    if (self) {
        pthread_mutex_lock(&self->lock);
        self->write_pos = 0;
        self->read_pos = 0;
        self->readable_len = 0;
        self->state = e_can_read;
        pthread_mutex_unlock(&self->lock);
        DEBUG_INFO("buffer reset\n");
    } else {
        DEBUG_ERROR("error: input error exit\n");
    }
}

int cache_buffer_write(cache_buffer_t *self, char *data, size_t len)
{
    int write_len = -1;
    int end_read = -1;
    int new_buf_len = 0;
    int new_write_pos = 0;
    unsigned char *new_buf = NULL;

    if (self) {
        pthread_mutex_lock(&self->lock);
        if (self->buf && self->state != e_cache_stop) {
            // Get the buffer usable len
            write_len = self->buf_len - self->readable_len;
            // The buffer is not enough to write
            if (write_len <= len) {
                DEBUG_INFO("???????? The buffer is not enough ???????\n");
                new_buf_len = self->buf_len + (self->step * (len / self->step + 1));
                new_buf = os_calloc(1, new_buf_len);
                if (new_buf) {
                    new_write_pos = (int)(self->write_pos - self->read_pos);
                    DEBUG_INFO("???????? new_write_pos(%d) ???????\n", new_write_pos);
                    if (new_write_pos >= 0) {
                        memcpy(new_buf, self->buf + self->read_pos, self->readable_len);
                    } else {
                        end_read = self->buf_len - self->read_pos;
                        memcpy(new_buf, self->buf + self->read_pos, end_read);
                        memcpy(new_buf + end_read, self->buf, self->write_pos);
                        if ((self->write_pos + end_read) != self->readable_len) {
                            DEBUG_ERROR(
                                "???????? why is different, it doesnot make sence ???????\n");
                            assert(0);
                        }
                    }
                    memcpy(new_buf + self->readable_len, data, len);
                    free(self->buf);

                    self->buf = new_buf;
                    self->buf_len = new_buf_len;
                    self->read_pos = 0;
                    self->readable_len += len;
                    self->write_pos = self->readable_len;
                    write_len = len;
                } else {
                    DEBUG_ERROR("error: calloc failed\n");
                    assert(0);
                }
            } else {
                // Check the buffer is write to the end
                end_read = (self->buf_len - self->write_pos);
                if (end_read >= len) {
                    memcpy(self->buf + self->write_pos, data, len);
                } else {
                    memcpy(self->buf + self->write_pos, data, end_read);
                    memcpy(self->buf, data + end_read, len - end_read);
                }
                self->write_pos = (self->write_pos + len) % self->buf_len;
                self->readable_len += len;
            }
        }
        pthread_mutex_unlock(&self->lock);
    } else {
        DEBUG_ERROR("error: input error exit\n");
    }

    return write_len;
}

int cache_buffer_read(cache_buffer_t *self, char *data, size_t len, e_cache_state_t *state)
{
    int read_len = -1;

    if (self) {
        pthread_mutex_lock(&self->lock);
        if (self->buf && self->state != e_cache_stop) {
            read_len = MIN((self->buf_len - self->read_pos), self->readable_len);
            read_len = read_len > len ? len : read_len;

            if (read_len > 0) {
                memcpy(data, self->buf + self->read_pos, read_len);
                self->readable_len = self->readable_len - read_len;
                self->read_pos = (self->read_pos + read_len) % self->buf_len;
            } else {
                read_len = 0;
            }
        }
        *state = self->state;
        pthread_mutex_unlock(&self->lock);
    } else {
        DEBUG_ERROR("error: input error exit\n");
    }

    return read_len;
}

int cache_buffer_len(cache_buffer_t *self)
{
    if (self) {
        size_t buf_len = 0;
        pthread_mutex_lock(&self->lock);
        buf_len = self->buf_len;
        pthread_mutex_unlock(&self->lock);
        return buf_len;
    }
    DEBUG_ERROR("error: input error exit\n");

    return -1;
}

int cache_buffer_set_state(cache_buffer_t *self, e_cache_state_t state)
{
    if (self) {
        pthread_mutex_lock(&self->lock);
        self->state = state;
        pthread_mutex_unlock(&self->lock);
        return 0;
    }
    DEBUG_ERROR("error: input error exit\n");

    return -1;
}

#if defined(NAVER_LINE)
int cache_buffer_get_type(cache_buffer_t *self)
{
    int type;
    int pos;
    char *ptr;

    if (self) {
        pthread_mutex_lock(&self->lock);
        pos = self->read_pos;
        ptr = self->buf + pos;

        if (!strncmp(ptr, "ID3", strlen("ID3"))) {
            type = e_cid_mp3;
        } else if (!strncmp(ptr, "RIFF", strlen("RIFF"))) {
            type = e_cid_wav;
        }
        pthread_mutex_unlock(&self->lock);
    }

    return type;
}
#endif
