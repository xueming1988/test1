#include <stdio.h>
#include <stdlib.h>

#include "iptv.h"
#include "iptv_shm.h"

int shm_id;

IPTV_OPERATION_RESULT
iptv_init(int role)
{
    IPTV_OPERATION_RESULT result = OPERATION_FAILED;

    shm_id = iptv_shm_creat(role);
    if (shm_id != -1) {
        result = iptv_shm_map(shm_id);
        if (role == LINKPLAY_PLATFORM)
            iptv_shm_init();
    }

    return result;
}

IPTV_OPERATION_RESULT
iptv_uninit(void)
{
    IPTV_OPERATION_RESULT result = OPERATION_FAILED;

    if (shm_id != -1) {
        result = iptv_shm_destory(shm_id);
    }

    return result;
}

void iptv_shm_reset(void) { iptv_shm_init(); }

size_t iptv_pcm_write(char *buf, size_t len)
{
    size_t less_len = len;
    size_t d_len = 0;
    size_t written_len;

    while (less_len > 0) {
        written_len = iptv_shm_write(buf + d_len, less_len);
        if (written_len > 0) {
            d_len += written_len;
            less_len -= written_len;
        } else {
            return len - less_len;
        }
    }

    return len;
}

int iptv_pcm_read(char *buf, size_t len)
{
    size_t less_len = len;
    size_t d_len = 0;
    size_t read_len;

    if (iptv_shm_get_record_switch()) {
        printf("[%s] we need to quit u+tv hearing mode\n", __func__);
        iptv_shm_set_record_switch(0);
        return -2;
    }

    if (iptv_shm_readable())
        return -1;

    while (less_len > 0) {
        read_len = iptv_shm_read(buf + d_len, less_len);
        if (read_len > 0) {
            d_len += read_len;
            less_len -= read_len;
        } else {
            return len - less_len;
        }
    }

    return len;
}

int iptv_is_alive(void)
{
    char command[256];
    char buf[256];
    FILE *fp;
    int count;

    snprintf(command, sizeof(command),
             "ps | grep -v grep | grep -v \"sh -c\" | grep iptvagent | wc -l");

    fp = popen(command, "r");
    if (fp) {
        fgets(buf, sizeof(buf), fp);
        count = atoi(buf);

        pclose(fp);
    }

    return count;
}

void iptv_cancel_recording(void) { iptv_shm_set_record_switch(1); }

int monitor_is_alive(void)
{
    char command[256];
    char buf[256];
    FILE *fp;
    int count;

    snprintf(command, sizeof(command),
             "ps | grep -v grep | grep -v \"sh -c\" | grep memory_monitor.sh | wc -l");

    fp = popen(command, "r");
    if (fp) {
        fgets(buf, sizeof(buf), fp);
        count = atoi(buf);

        pclose(fp);
    }

    return count;
}
