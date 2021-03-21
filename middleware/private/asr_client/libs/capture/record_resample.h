/*************************************************************************
    > File Name: record_resample.h
    > Author:
    > Mail:
    > Created Time: Sun 13 May 2018 12:04:25 AM EDT
 ************************************************************************/

#ifndef _RECORD_RESAMPLE_H
#define _RECORD_RESAMPLE_H

#include <stdio.h>
#include <stdint.h>

#define STEPACCURACY 32

typedef struct lin_pol_s {
    unsigned long long step;
    unsigned long long pt;
    int len;
    int same;
} LIN_POL;

typedef struct {
    LIN_POL g_lin_pol;
    char g_bNeedLastSample;
    int16_t g_last_sample;
    int inited;
} resample_context_t;

#ifdef __cplusplus
extern "C" {
#endif

int CCapt_resample_init(resample_context_t *ctx, int org_rate, int new_rate, LIN_POL *pLin_pol);

int CCapt_postprocess(resample_context_t *ctx, char **temp_buf, int *temp_buf_size,
                      LIN_POL *pLin_pol, char *buf, int read_len, char *writebuf, int *write_len,
                      int src_channel, int dst_channel);

#ifdef __cplusplus
}
#endif

#endif
