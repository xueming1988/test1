/*************************************************************************
    > File Name: multi_partparser.cpp
    > Author:
    > Mail:
    > Created Time: Wed 07 Mar 2018 09:30:52 PM EST
 ************************************************************************/

#include <iostream>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <cstdio>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

#include "MultipartParser.h"
#include "MultipartReader.h"

#include "multi_partparser.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _multi_partparser_s {
    MultipartReader parser;
    on_part_begin_t *on_part_begin;
    void *part_begin_ctx;
    on_part_data_t *on_part_data;
    void *part_data_ctx;
    on_part_end_t *on_part_end;
    void *part_end_ctx;
    on_end_t *on_end;
    void *end_ctx;
};

void onPartBegin(const MultipartHeaders &headers, void *userData)
{
    multi_partparser_t *self = (multi_partparser_t *)userData;
    MultipartHeaders::const_iterator it;

    //printf("onPartBegin:\n");
    //MultipartHeaders::const_iterator end = headers.end();
    for(it = headers.begin(); it != headers.end(); it++) {
        //printf("  %s = %s\n", it->first.c_str(), it->second.c_str());
        if(self && self->on_part_begin) {
            self->on_part_begin(self->part_begin_ctx, it->first.c_str(), it->second.c_str());
        }
    }
    //printf("  aaa: %s --\n", headers["aaa"].c_str());
}

void onPartData(const char *buffer, size_t size, void *userData)
{
    multi_partparser_t *self = (multi_partparser_t *)userData;
    if(self && self->on_part_data) {
        self->on_part_data(self->part_data_ctx, buffer, size);
    }
    //printf("onPartData: len(%d) (%s)\n", size, string(buffer, size).c_str());
}

void onPartEnd(void *userData)
{
    //printf("onPartEnd\n");
    multi_partparser_t *self = (multi_partparser_t *)userData;
    if(self && self->on_part_end) {
        self->on_part_end(self->part_end_ctx);
    }
}

void onEnd(void *userData)
{
    //printf("onEnd\n");
    multi_partparser_t *self = (multi_partparser_t *)userData;
    if(self && self->on_end) {
        self->on_end(self->end_ctx);
    }
}

multi_partparser_t *multi_partparser_create(void)
{
    multi_partparser_t *self = new struct _multi_partparser_s;
    if(self) {
        self->parser.reset();
        self->parser.onPartBegin = onPartBegin;
        self->parser.onPartData = onPartData;
        self->parser.onPartEnd = onPartEnd;
        self->parser.onEnd = onEnd;
        self->parser.userData = self;
        return self;
    }

    return NULL;
}

int multi_partparser_destroy(multi_partparser_t **self_p)
{
    multi_partparser_t *self = *self_p;
    if(self) {
        delete self;
        *self_p = NULL;
        return 0;
    }

    return -1;
}

int multi_partparser_set_boundary(multi_partparser_t *self, const char *boundary)
{
    if(self) {
        self->parser.setBoundary(boundary);
    }

    return 0;
}

int multi_partparser_set_part_begin_cb(multi_partparser_t *self, on_part_begin_t *on_part_begin, void *usr_data)
{
    if(self) {
        self->on_part_begin = on_part_begin;
        self->part_begin_ctx = usr_data;
        return 0;
    }

    return -1;
}

int multi_partparser_set_part_data_cb(multi_partparser_t *self, on_part_data_t *on_part_data, void *usr_data)
{
    if(self) {
        self->on_part_data = on_part_data;
        self->part_data_ctx = usr_data;
        return 0;
    }

    return -1;
}

int multi_partparser_set_part_end_cb(multi_partparser_t *self, on_part_end_t *on_part_end, void *usr_data)
{
    if(self) {
        self->on_part_end = on_part_end;
        self->part_end_ctx = usr_data;
        return 0;
    }

    return -1;
}

int multi_partparser_set_end_cb(multi_partparser_t *self, on_end_t *on_end, void *usr_data)
{
    if(self) {
        self->on_end = on_end;
        self->end_ctx = usr_data;
        return 0;
    }

    return -1;
}

size_t multi_partparser_buffer(multi_partparser_t *self, const char *buffer, size_t size)
{
    size_t ret = 0;

    if(self) {
        ret = self->parser.feed(buffer, size);
    }

    return ret;
}

void multi_partparser_debug(multi_partparser_t *self)
{
    if(self) {
        printf("----------------------%s stop(%d)---------------------\n", \
               (char *)self->parser.getErrorMessage(), self->parser.stopped());
    }
}

#ifdef __cplusplus
}
#endif

