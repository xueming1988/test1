#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "iptv.h"
#include "iptv_shm.h"

static IPTV_SHM_T *g_shm_buf = NULL;

int iptv_shm_creat(int role)
{
    key_t shm_key;
    int shm_id = -1;
    size_t size;
    int oflag;

    printf("[%s] ++\n", __func__);

    shm_key = ftok(SHM_PATH_NAME, 0);
    printf("shm_key = %d\n", shm_key);
    if (shm_key != -1) {
        if (role == LINKPLAY_PLATFORM) {
            size = sizeof(IPTV_SHM_T);
            oflag = 0666 | IPC_CREAT | IPC_EXCL;
        } else if (role == CUSTOMER_PLATFORM) {
            size = 0;
            oflag = 0666;
        }

        shm_id = shmget(shm_key, size, oflag);
        printf("shm_id = %d\n", shm_id);

        // for asr_tts crash case
        if (shm_id == -1 && role == LINKPLAY_PLATFORM) {
            size = 0;
            oflag = 0666;
            shm_id = shmget(shm_key, size, oflag);
            printf("shm_id = %d\n", shm_id);
        }
    }

    printf("[%s] --\n", __func__);

    return shm_id;
}

IPTV_OPERATION_RESULT
iptv_shm_map(int shm_id)
{
    void *ptr;
    IPTV_OPERATION_RESULT result = OPERATION_FAILED;
    printf("[%s] ++\n", __func__);

    if (shm_id != -1) {
        ptr = shmat(shm_id, NULL, 0);
        printf("ptr = %p\n", ptr);

        if (ptr != (void *)-1) {
            g_shm_buf = (IPTV_SHM_T *)ptr;
            result = OPERATION_SUCCEED;
        }
    }

    printf("[%s] --\n", __func__);

    return result;
}

size_t iptv_shm_write(char *pcm, size_t len)
{
    RING_BUFFER_T *ptr = &g_shm_buf->ring_buffer;
    size_t writable_len;
    size_t valid_writable_len;

    if (ptr) {
        writable_len = ptr->buf_len - ptr->readable_len;
        valid_writable_len = len > writable_len ? writable_len : len;

        if (valid_writable_len) {
            if (ptr->write_pos + valid_writable_len <= ptr->buf_len)
                memcpy(ptr->buf + ptr->write_pos, pcm, valid_writable_len);
            else {
                size_t length = ptr->buf_len - ptr->write_pos;
                memcpy(ptr->buf + ptr->write_pos, pcm, length);
                memcpy(ptr->buf, pcm + length, valid_writable_len - length);
            }

            ptr->write_pos = (ptr->write_pos + valid_writable_len) % ptr->buf_len;
            ptr->readable_len += valid_writable_len;
        }
    }

    return valid_writable_len;
}

size_t iptv_shm_read(char *pcm, size_t len)
{
    RING_BUFFER_T *ptr = &g_shm_buf->ring_buffer;
    size_t readable_len;
    size_t valid_readable_len;

    if (ptr) {
        readable_len = ptr->readable_len;
        valid_readable_len = len > readable_len ? readable_len : len;

        if (valid_readable_len) {
            if (ptr->read_pos + valid_readable_len <= ptr->buf_len) {
                memcpy(pcm, ptr->buf + ptr->read_pos, valid_readable_len);
            } else {
                size_t length = ptr->buf_len - ptr->read_pos;
                memcpy(pcm, ptr->buf + ptr->read_pos, len);
                memcpy(pcm, ptr->buf, valid_readable_len - length);
            }

            ptr->read_pos = (ptr->read_pos + valid_readable_len) % ptr->buf_len;
            ptr->readable_len -= valid_readable_len;
        }
    }

    return valid_readable_len;
}

void iptv_shm_init(void)
{
    RING_BUFFER_T *ptr = &g_shm_buf->ring_buffer;

    if (ptr) {
        ptr->buf_len = PCM_LEN;
        ptr->readable_len = 0;
        ptr->read_pos = 0;
        ptr->write_pos = 0;
    }
}

IPTV_OPERATION_RESULT
iptv_shm_destory(int shm_id)
{
    int ret;
    IPTV_OPERATION_RESULT result = OPERATION_FAILED;

    if (shm_id != -1) {
        ret = shmdt(g_shm_buf);
        if (!ret) {
            ret = shmctl(shm_id, IPC_RMID, NULL);
            if (!ret)
                result = OPERATION_SUCCEED;
        }
    }

    return result;
}

int iptv_shm_readable(void)
{
    RING_BUFFER_T *ptr = &g_shm_buf->ring_buffer;

    if (ptr) {
        if (ptr->readable_len + ptr->read_pos + ptr->write_pos)
            return 0;
    }

    return -1;
}

void iptv_shm_set_record_switch(int value)
{
    if (g_shm_buf)
        g_shm_buf->record_switch = value;
}

int iptv_shm_get_record_switch(void)
{
    if (g_shm_buf)
        return g_shm_buf->record_switch;

    return 0;
}
