/*************************************************************************
    > File Name: shm_record_sharp.h
    > Author:
    > Mail:
    > Created Time: Sat 12 May 2018 01:17:52 AM EDT
 ************************************************************************/

#ifndef _SHM_RECORD_SHARP_H
#define _SHM_RECORD_SHARP_H

#include <stdio.h>
#include <string.h>
#include <pthread.h>

#define FOXCOM_DSNOOP_DATA_LEN (192 * 1024)

typedef struct {
    size_t buf_len;
    volatile size_t write_pos;
    volatile size_t read_pos;
    volatile size_t readable_len;
    pthread_mutex_t lock;
    int fin_flags;
    char buf[FOXCOM_DSNOOP_DATA_LEN];
} FOXCOM_SNOOP;

#ifdef __cplusplus
extern "C" {
#endif

void FoxcomRingBufferInit(FOXCOM_SNOOP **ring_buffer);

void CVadFlush(void);

int foxcom_save_rdata(char *data, size_t size, FOXCOM_SNOOP *_this);

#ifdef __cplusplus
}
#endif

#endif
