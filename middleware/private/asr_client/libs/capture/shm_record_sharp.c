/*************************************************************************
    > File Name: shm_record_sharp.c
    > Author:
    > Mail:
    > Created Time: Sat 12 May 2018 01:17:43 AM EDT
 ************************************************************************/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/shm.h>

#include "wm_util.h"
#include "shm_record_sharp.h"

static FOXCOM_SNOOP *ShmCreateforComms(void)
{
    FOXCOM_SNOOP *shmem = 0;
    int shmid = shmget(10011, sizeof(FOXCOM_SNOOP), 0666 | IPC_CREAT | IPC_EXCL);
    if (shmid != -1) {
        shmem = shmat(shmid, (const void *)0, 0);
        if (shmem) {
            memset(shmem, 0, sizeof(FOXCOM_SNOOP));
        }
        // printf("wiimu context create memory %d, mem = 0x%x \r\n", shmid, (unsigned long)shmem);
    } else {
        if (EEXIST == errno || EINVAL == errno) {
            shmid = shmget(10011, 0, 0);
            if (shmid != -1)
                shmem = shmat(shmid, (const void *)0, 0);
            // printf("wiimu context get memory %d, mem = 0x%x \r\n", shmid, (unsigned long)shmem);
        }
    }
    if (shmem == 0) {
        printf("wiimu context: fail of get share memory\n");
        return 0;
    }
    return shmem;
}

void FoxcomRingBufferInit(FOXCOM_SNOOP **ring_buffer)
{
    printf("[%s:%d] Foxcomring_buffer_create start\n", __FUNCTION__, __LINE__);
    (*ring_buffer) = ShmCreateforComms();
    if (ring_buffer) {
        (*ring_buffer)->buf_len = FOXCOM_DSNOOP_DATA_LEN;
        (*ring_buffer)->write_pos = 0;
        (*ring_buffer)->read_pos = 0;
        (*ring_buffer)->readable_len = 0;
        (*ring_buffer)->fin_flags = 0;
        pthread_mutexattr_t mutextattr;
        pthread_mutexattr_init(&mutextattr);
        pthread_mutexattr_setpshared(&mutextattr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&(*ring_buffer)->lock, &mutextattr);
        printf("%s:FoxcomRingBufferInit share mem ok\n", __func__);
    }
}

void FoxcomRingBufferReinit(FOXCOM_SNOOP *ring_buffer)
{
    ring_buffer->buf_len = FOXCOM_DSNOOP_DATA_LEN;
    ring_buffer->write_pos = 0;
    ring_buffer->read_pos = 0;
    ring_buffer->readable_len = 0;
    ring_buffer->fin_flags = 0;
    memset(ring_buffer->buf, 0, FOXCOM_DSNOOP_DATA_LEN);
}

void CVadFlush(void) { FoxcomRingBufferReinit(gFoxcom_snoop); }

static size_t FoxcomRingBufferWrite(FOXCOM_SNOOP *_this, char *data, size_t len)
{
    size_t write_len = 0;

    if (_this) {
        pthread_mutex_lock(&_this->lock);
        // printf("%s:lock ++\n",__func__);
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
            // printf("%s:lock --\n",__func__);
            return write_len;
        } else {
            printf("[%s:%d] ring buffer is full\n", __FUNCTION__, __LINE__);
            pthread_mutex_unlock(&_this->lock);
            // printf("%s:lock --\n",__func__);
            return 0;
        }
    }

    printf("[%s:%d] error: input error exit\n", __FUNCTION__, __LINE__);
    return -1;
}

int foxcom_save_rdata(char *data, size_t size, FOXCOM_SNOOP *_this)
{
    size_t less_len = size;
    printf("%s: start _this=%s\n", __func__, _this);
    while (less_len > 0 && (_this != NULL)) {
        int write_len = FoxcomRingBufferWrite(_this, data, less_len);
        if (write_len > 0) {
            // printf("%s:writedatelen=%d\n",__func__,write_len);
            less_len -= write_len;
            data += write_len;
        } else if (write_len <= 0) {
            wiimu_log(1, 0, 0, 0, "foxcomRingBufferWrite return %d\n", write_len);
            return less_len;
        }

        if (write_len == 0)
            usleep(5000);
    }

    return size;
}
