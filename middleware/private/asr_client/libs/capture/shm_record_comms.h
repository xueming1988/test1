/*************************************************************************
    > File Name: shm_record.h
    > Author:
    > Mail:
    > Created Time: Sat 12 May 2018 01:17:52 AM EDT
 ************************************************************************/

#ifndef _SHM_RECORD_COMMS_H
#define _SHM_RECORD_COMMS_H

#define MY_DSNOOP_DATA_LEN 32768 * 3

typedef struct {
    int read_idx;
    int write_idx;
    int data_len;
    int max_data_len;
    int sample_rate;
    char data[MY_DSNOOP_DATA_LEN];
} MY_SNOOP;

#ifdef __cplusplus
extern "C" {
#endif

MY_SNOOP *ShmCreateforComms(void);

void ShmRecordWrite(MY_SNOOP *asr_dsnoop, char *buf, int len);

void ShmRecordInit(MY_SNOOP *ptr, int sample_rate);

#ifdef __cplusplus
}
#endif

#endif
