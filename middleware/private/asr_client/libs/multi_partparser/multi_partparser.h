/*************************************************************************
    > File Name: multi_partparser.h
    > Author:
    > Mail:
    > Created Time: Wed 07 Mar 2018 09:31:16 PM EST
 ************************************************************************/

#ifndef __MULTI_PARTPARSER_H__
#define __MULTI_PARTPARSER_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _multi_partparser_s multi_partparser_t;

typedef int (on_part_begin_t)(void *usr_data, const char *key, const char *value);
typedef int (on_part_data_t)(void *usr_data, const char *buffer, size_t size);
typedef int (on_part_end_t)(void *usr_data);
typedef int (on_end_t)(void *usr_data);

/*
 *
 *
 */
multi_partparser_t *multi_partparser_create(void);

/*
 *
 *
 */
int multi_partparser_destroy(multi_partparser_t **self_p);

/*
 *
 *
 */
int multi_partparser_set_boundary(multi_partparser_t *self, const char *boundary);

/*
 *
 *
 */
int multi_partparser_set_part_begin_cb(multi_partparser_t *self, on_part_begin_t *on_part_begin, void *usr_data);

/*
 *
 *
 */
int multi_partparser_set_part_data_cb(multi_partparser_t *self, on_part_data_t *on_part_data, void *usr_data);

/*
 *
 *
 */
int multi_partparser_set_part_end_cb(multi_partparser_t *self, on_part_end_t *on_part_end, void *usr_data);

/*
 *
 *
 */
int multi_partparser_set_end_cb(multi_partparser_t *self, on_end_t *on_end, void *usr_data);

/*
 *
 *
 */
size_t multi_partparser_buffer(multi_partparser_t *self, const char *buffer, size_t size);

/*
 *
 *
 */
void multi_partparser_debug(multi_partparser_t *self);

#ifdef __cplusplus
}
#endif

#endif
