
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "wm_util.h"

int main(int argc, const char *argv[])
{
    int sleep_time = 5;

    if (argc != 2) {
        printf("%s sleep ms\n", argv[0]);
        return -1;
    }

    sleep_time = atoi(argv[1]);
    if (sleep_time < 0) {
        sleep_time = 5000;
    }

    while (1) {
        SocketClientReadWriteMsg("/tmp/RequestASRTTS", "talkstart:0", strlen("talkstart:0"), NULL,
                                 NULL, 0);
        usleep(sleep_time * 1000);
    }

    return 0;
}
