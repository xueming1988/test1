/*************************************************************************
    > File Name: shm_record.c
    > Author:
    > Mail:
    > Created Time: Sat 12 May 2018 01:17:43 AM EDT
 ************************************************************************/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/shm.h>

#include "wm_util.h"
#include "shm_record_comms.h"

MY_SNOOP *ShmCreateforComms(void)
{
    MY_SNOOP *shmem = NULL;

    int shmid = shmget(1008, sizeof(MY_SNOOP), 0666 | IPC_CREAT | IPC_EXCL);
    if (shmid != -1) {
        shmem = shmat(shmid, (const void *)0, 0);
        if (shmem) {
            memset(shmem, 0, sizeof(MY_SNOOP));
        }
        // printf("wiimu context create memory %d, mem = 0x%x \r\n", shmid, (unsigned long)shmem);
    } else {
        if (EEXIST == errno || EINVAL == errno) {
            shmid = shmget(1008, 0, 0);
            if (shmid != -1) {
                shmem = shmat(shmid, (const void *)0, 0);
                // printf("wiimu context get memory %d, mem = 0x%x \r\n", shmid, (unsigned
                // long)shmem);
            }
        }
    }
    if (shmem == 0) {
        printf("wiimu context: fail of get share memory\n");
        return 0;
    }

    return shmem;
}

void ShmRecordWrite(MY_SNOOP *shm_ptr, char *buf, int len)
{
    int write_p_rlen = 0;

    if (shm_ptr) {
        if (len > shm_ptr->max_data_len) {
            wiimu_log(1, 0, 0, 0, "%s:%d  src len is too long!\n", __FUNCTION__, __LINE__);
            len = shm_ptr->max_data_len;
        }

        write_p_rlen = shm_ptr->max_data_len - shm_ptr->write_idx;
        if (len <= write_p_rlen) {
            memcpy(&shm_ptr->data[shm_ptr->write_idx], buf, len);
            shm_ptr->write_idx += len;
        } else {
            memcpy(&shm_ptr->data[shm_ptr->write_idx], buf, write_p_rlen);
            memcpy(&shm_ptr->data[0], buf + write_p_rlen, len - write_p_rlen);
            shm_ptr->write_idx = len - write_p_rlen;
        }
    }
}

void ShmRecordInit(MY_SNOOP *ptr, int sample_rate)
{
    if (ptr) {
        ptr->read_idx = 0;
        ptr->write_idx = 0;
        ptr->data_len = 0;
        ptr->max_data_len = MY_DSNOOP_DATA_LEN;
        ptr->sample_rate = sample_rate;
    }
}
