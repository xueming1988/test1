#ifndef __IPTV_SHM_H__
#define __IPTV_SHM_H__

#define SHM_PATH_NAME "/system/workdir/bin/asr_tts"

#define PCM_LEN (32 * 10 * 1024) // 10 seconds pcm data

typedef struct {
    char buf[PCM_LEN];
    size_t buf_len;
    volatile size_t readable_len;
    volatile size_t write_pos;
    volatile size_t read_pos;
} RING_BUFFER_T;

typedef struct {
    RING_BUFFER_T ring_buffer;
    volatile int record_switch; // 0 is enable, 1 is disable
} IPTV_SHM_T;

int iptv_shm_creat(int);

IPTV_OPERATION_RESULT
iptv_shm_map(int);

size_t iptv_shm_write(char *, size_t);

size_t iptv_shm_read(char *, size_t);

void iptv_shm_init(void);

IPTV_OPERATION_RESULT
iptv_shm_destory(int);

int iptv_shm_readable(void);

void iptv_shm_set_record_switch(int);

int iptv_shm_get_record_switch(void);

#endif