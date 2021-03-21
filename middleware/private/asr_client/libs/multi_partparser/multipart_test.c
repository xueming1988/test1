/*************************************************************************
    > File Name: multipart_test.c
    > Author:
    > Mail:
    > Created Time: Mon 12 Mar 2018 09:34:22 PM EDT
 ************************************************************************/

#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <cstdio>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

#include "multi_partparser.h"

#define BOUNDARY "------abcde123"

int write_file = 0;
FILE *debug_file = NULL;

int on_part_begin(void *usr_data, const char *key, const char *value)
{
    printf("  %s = %s\n", key, value);
    if(!strncmp(value, "application/octet-stream", strlen("application/octet-stream"))) {
        printf("is audio data:\n");
        write_file = 1;
        debug_file = fopen("./tts.mp3", "wb+");
    }
    return 0;
}

int on_part_data(void *usr_data, const char *buffer, size_t size)
{

    if(write_file) {
        printf("onPartData: len(%d) \n", size);
        if(debug_file) {
            fwrite((char *)buffer, 1, size, debug_file);
        }
    } else {
        printf("onPartData: len(%d) (%s)\n", size, buffer);
    }
    return 0;
}

int on_part_end(void *usr_data)
{
    printf("onPartEnd\n");
    if(write_file) {
        write_file = 0;
        if(debug_file) {
            fclose(debug_file);
            debug_file = NULL;
        }
    }
    return 0;
}

int on_end(void *usr_data)
{
    printf("onEnd\n");
    return 0;
}

int main(int argc, const char **argv)
{
    char *input_file = NULL;
    char *boundary;
    struct timeval stime, etime;
    struct stat sbuf;
    multi_partparser_t *multi_partparser;

    if(argc != 3) {
        printf("./%s input_file bounfary\n", argv[0]);
        return -1;
    }

    input_file = (char *)argv[1];
    boundary = (char *)argv[2];
    printf("input_file=%s boundary=%s\n", input_file, boundary);
    stat(input_file, &sbuf);

    size_t bufsize = 16384;
    char *buf = (char *) malloc(bufsize);

    gettimeofday(&stime, NULL);
    printf("------------\n");
    //BOUNDARY
    multi_partparser = multi_partparser_create();
    multi_partparser_set_boundary(multi_partparser, boundary);
    multi_partparser_set_part_begin_cb(multi_partparser, on_part_begin, NULL);
    multi_partparser_set_part_data_cb(multi_partparser, on_part_data, NULL);
    multi_partparser_set_part_end_cb(multi_partparser, on_part_end, NULL);
    multi_partparser_set_end_cb(multi_partparser, on_end, NULL);

    FILE *f = fopen(input_file, "rb");
    while(!feof(f)) {
        size_t len = fread(buf, 1, bufsize, f);
        size_t fed = 0;
        do {
            if(len - fed > 0) {
                size_t ret = multi_partparser_buffer(multi_partparser, buf + fed, len - fed);
                if(ret > 0) {
                    fed += ret;
                } else {
                    break;
                }
            }
        } while(fed < len);
    }
    multi_partparser_debug(multi_partparser);
    fclose(f);
    multi_partparser_destroy(&multi_partparser);

    gettimeofday(&etime, NULL);
    unsigned long long a = (unsigned long long) stime.tv_sec * 1000000 + stime.tv_usec;
    unsigned long long b = (unsigned long long) etime.tv_sec * 1000000 + etime.tv_usec;
    printf("(C++)    Total: %.2fs   Per run: %.2fs   Throughput: %.2f MB/sec\n",
           (b - a) / 1000000.0,
           (b - a) / 1000000.0,
           ((unsigned long long) sbuf.st_size) / ((b - a) / 1000000.0) / 1024.0 / 1024.0);

    return 0;
}
