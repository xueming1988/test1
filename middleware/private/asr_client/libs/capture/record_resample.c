/*************************************************************************
    > File Name: record_resample.c
    > Author:
    > Mail:
    > Created Time: Sun 13 May 2018 12:04:22 AM EDT
 ************************************************************************/

#include <stdio.h>
#include <string.h>
#include <malloc.h>

#include "record_resample.h"

// static resample_context_t resample_info[2];
#ifndef NAVER_LINE
extern int mic_channel_test_mode;
#endif

int CCapt_resample_init(resample_context_t *ctx, int org_rate, int new_rate, LIN_POL *pLin_pol)
{
    if (org_rate == new_rate) {
        pLin_pol->same = 1;
    } else {
        pLin_pol->same = 0;
        pLin_pol->pt = 0LL;
        pLin_pol->step = ((uint64_t)org_rate << STEPACCURACY) / (uint64_t)new_rate + 1LL;
    }

    if (ctx) {
        ctx->g_bNeedLastSample = 0;
    }

    return 0;
}

// Fast linear interpolation resample with modest audio quality
static int CCapt_resample_linint(resample_context_t *ctx, char *inbuf, int read_len, char *writebuf,
                                 int *write_len, LIN_POL *pLin_pol)
{
    uint32_t len = 0; // Number of input samples
    uint32_t nch = 1; // Words pre transfer
    uint64_t step = pLin_pol->step;
    int16_t *in16 = ((int16_t *)inbuf);
    int16_t *out16 = ((int16_t *)writebuf);
    uint64_t end = ((((uint64_t)read_len) / 2LL) << STEPACCURACY);
    uint64_t pt = pLin_pol->pt;
    uint16_t tmp;
    int64_t sample1, sample2;
    uint16_t nTotalSample = read_len / 2;
    char bFirstSample = 0;
    int16_t nCurrentSample = 0;

    if (pLin_pol->same) {
        memcpy(writebuf, inbuf, read_len);
        *write_len = read_len;
    } else {
        if (ctx && ctx->g_bNeedLastSample) {
            ctx->g_bNeedLastSample = 0;
            sample1 = ctx->g_last_sample;
            sample2 = in16[0];

            // out = (sample1 * x + sample2 * ((1 << STEPACCURACY) - x)) / (1 << STEPACCURACY)
            out16[len++] = ((sample1 - sample2) * (pt & ((1LL << STEPACCURACY) - 1)) +
                            sample2 * ((1LL << STEPACCURACY))) /
                           (1LL << STEPACCURACY);
            pt += step;
        }

        while (pt < end) {
            nCurrentSample = pt >> STEPACCURACY;

            if (ctx && nCurrentSample >= (nTotalSample - 1)) {
                ctx->g_bNeedLastSample = 1;
                break;
            }

            sample1 = in16[nCurrentSample];
            sample2 = in16[nCurrentSample + 1];

            out16[len++] = ((sample1 - sample2) * (pt & ((1LL << STEPACCURACY) - 1)) +
                            sample2 * ((1LL << STEPACCURACY))) /
                           (1LL << STEPACCURACY);

            pt += step;
        }

        if (ctx && ctx->g_bNeedLastSample == 1) {
            pLin_pol->pt = pt & ((1LL << STEPACCURACY) - 1);
        } else {
            pLin_pol->pt = pt - end;
        }

        if (ctx) {
            ctx->g_last_sample = in16[nTotalSample - 1];
        }
        *write_len = len * 2;
        // printf("in %d bytes, out %d bytes, pt is %llX\n", read_len, *write_len, pLin_pol->pt);
    }

    return 0;
}

// channel: 1 left, 2 right, else (left+right)/2
int CCapt_postprocess(resample_context_t *ctx, char **temp_buf, int *temp_buf_size,
                      LIN_POL *pLin_pol, char *buf, int read_len, char *writebuf, int *write_len,
                      int src_channel, int dst_channel)
{
    int len = read_len;
#ifndef NAVER_LINE
    if (mic_channel_test_mode) {
        memcpy(writebuf, buf, read_len);
        *write_len = read_len;
        return 0;
    }
#endif
    // printf("%s enter\n", __func__);

    if (!*temp_buf) {
        *temp_buf = malloc(read_len);
        *temp_buf_size = read_len;
    } else if (read_len > *temp_buf_size) {
        free(*temp_buf);
        *temp_buf = malloc(read_len);
        *temp_buf_size = read_len;
    }

    if (src_channel == 2) {
        int inStep = 2;
        int outStep = 1;
        int16_t *tin = (int16_t *)buf;
        int16_t *tout = (int16_t *)(*temp_buf);
        len = len / (inStep * 2);
        *write_len = len * 2;
        while (len--) {
            if (dst_channel == 1) {
                *tout = tin[0];
            } else if (dst_channel == 2) {
                *tout = tin[1];
            } else {
                *tout = (tin[0] + tin[1]) / 2;
            }

            //*tout &= 0xFFF0;
            tin += inStep;
            tout += outStep;
        }
    } else if (src_channel == 1) {
        memcpy(*temp_buf, buf, len);
        *write_len = len;
    } else if (src_channel == 4) {
        int inStep = 4;
        int outStep = 1;
        int16_t *tin = (int16_t *)buf;
        int16_t *tout = (int16_t *)(*temp_buf);
        len = len / (inStep * 2);
        *write_len = len * 2;
        while (len--) {
            if (dst_channel == 1) {
                *tout = tin[0];
            } else if (dst_channel == 2) {
                *tout = tin[1];
            } else if (dst_channel == 3) {
                *tout = tin[2];
            } else if (dst_channel == 4) {
                *tout = tin[3];
            } else {
                *tout = (tin[0] + tin[1]) / 2;
            }

            //*tout &= 0xFFF0;
            tin += inStep;
            tout += outStep;
        }
    }
    // interpol
    CCapt_resample_linint(ctx, *temp_buf, *write_len, writebuf, write_len, pLin_pol);
    return 0;
}
