/*
 * WMA compatible decoder
 * Copyright (c) 2002 The FFmpeg Project.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * @file wmadec.c
 * WMA compatible decoder.
 */

#include <stdlib.h>
#include <string.h>
#include "wmadec.h"
#include "asf.h"
#include "bitstream.h"
#include "types.h"
#include "mdct.h"
#include "wmafixed.h"


//#define TRACE
/* size of blocks */
#define BLOCK_MIN_BITS 7
#define BLOCK_MAX_BITS 11
#define BLOCK_MAX_SIZE (1 << BLOCK_MAX_BITS)

#define BLOCK_NB_SIZES (BLOCK_MAX_BITS - BLOCK_MIN_BITS + 1)

/* XXX: find exact max size */
#define HIGH_BAND_MAX_SIZE 16

#define NB_LSP_COEFS 10

/* XXX: is it a suitable value ? */
#define MAX_CODED_SUPERFRAME_SIZE 16384

#define M_PI    3.14159265358979323846

#define M_PI_F  0x3243f // in fixed 32 format
#define TWO_M_PI_F  0x6487f   //in fixed 32

#define MAX_CHANNELS 2

#define NOISE_TAB_SIZE 8192

#define LSP_POW_BITS 7

#define VLCBITS 7       /*7 is the lowest without glitching*/
#define VLCMAX ((22+VLCBITS-1)/VLCBITS)

#define EXPVLCBITS 7
#define EXPMAX ((19+EXPVLCBITS-1)/EXPVLCBITS)

#define HGAINVLCBITS 9
#define HGAINMAX ((13+HGAINVLCBITS-1)/HGAINVLCBITS)


typedef struct WMADecodeContextPrivate
{
    WMADecodeContext *ectx;
    GetBitContext gb;
    int nb_block_sizes;  /* number of block sizes */
    int version; /* 1 = 0x160 (WMAV1), 2 = 0x161 (WMAV2) */
    int use_bit_reservoir;
    int use_variable_block_len;
    int use_exp_vlc;  /* exponent coding: 0 = lsp, 1 = vlc + delta */
    int use_noise_coding; /* true if perceptual noise is added */
    int byte_offset_bits;
    VLC exp_vlc;
    int exponent_sizes[BLOCK_NB_SIZES];
    uint16_t exponent_bands[BLOCK_NB_SIZES][25];
    int high_band_start[BLOCK_NB_SIZES]; /* index of first coef in high band */
    int coefs_start;               /* first coded coef */
    int coefs_end[BLOCK_NB_SIZES]; /* max number of coded coefficients */
    int exponent_high_sizes[BLOCK_NB_SIZES];
    int exponent_high_bands[BLOCK_NB_SIZES][HIGH_BAND_MAX_SIZE];
    VLC hgain_vlc;

    /* coded values in high bands */
    int high_band_coded[MAX_CHANNELS][HIGH_BAND_MAX_SIZE];
    int high_band_values[MAX_CHANNELS][HIGH_BAND_MAX_SIZE];

    /* there are two possible tables for spectral coefficients */
    VLC coef_vlc[2];
    uint16_t *run_table[2];
    uint16_t *level_table[2];
    /* frame info */
    int frame_len;       /* frame length in samples */
    int frame_len_bits;  /* frame_len = 1 << frame_len_bits */

    /* block info */
    int reset_block_lengths;
    int block_len_bits; /* log2 of current block length */
    int next_block_len_bits; /* log2 of next block length */
    int prev_block_len_bits; /* log2 of prev block length */
    int block_len; /* block length in samples */
    int block_num; /* block number in current frame */
    int block_pos; /* current position in frame */
    uint8_t ms_stereo; /* true if mid/side stereo mode */
    uint8_t channel_coded[MAX_CHANNELS]; /* true if channel is coded */
    int exponents_bsize[MAX_CHANNELS];      // log2 ratio frame/exp. length
    fixed32 exponents[MAX_CHANNELS][BLOCK_MAX_SIZE];
    fixed32 max_exponent[MAX_CHANNELS];
    int16_t coefs1[MAX_CHANNELS][BLOCK_MAX_SIZE];
    fixed32 (*coefs)[MAX_CHANNELS][BLOCK_MAX_SIZE];
    MDCTContext mdct_ctx[BLOCK_NB_SIZES];
    fixed32 *windows[BLOCK_NB_SIZES];
    /* output buffer for one frame and the last for IMDCT windowing */
    fixed32 frame_out[MAX_CHANNELS][BLOCK_MAX_SIZE * 2];
    /* last frame info */
    uint8_t last_superframe[MAX_CODED_SUPERFRAME_SIZE + 4]; /* padding added */
    int last_bitoffset;
    int last_superframe_len;
    fixed32 *noise_table;
    int noise_index;
    fixed32 noise_mult; /* XXX: suppress that and integrate it in the noise array */
    /* lsp_to_curve tables */
    fixed32 lsp_cos_table[BLOCK_MAX_SIZE];
    fixed64 lsp_pow_e_table[256];
    fixed32 lsp_pow_m_table1[(1 << LSP_POW_BITS)];
    fixed32 lsp_pow_m_table2[(1 << LSP_POW_BITS)];

#ifdef TRACE

    int frame_count;
#endif
}
WMADecodeContextPrivate;

typedef struct CoefVLCTable
{
    int n; /* total number of codes */
    const uint32_t *huffcodes; /* VLC bit values */
    const uint8_t *huffbits;   /* VLC bit size */
    const uint16_t *levels; /* table to build run/level tables */
}
CoefVLCTable;

static void wma_lsp_to_curve_init(WMADecodeContextPrivate *s, int frame_len);

static fixed32 coefsarray[MAX_CHANNELS][BLOCK_MAX_SIZE];

//static variables that replace malloced stuff
static fixed32 stat0[2048], stat1[1024], stat2[512], stat3[256], stat4[128];    //these are the MDCT reconstruction windows

static uint16_t *runtabarray[2], *levtabarray[2];                                        //these are VLC lookup tables

static uint16_t runtab0[1336], runtab1[1336], levtab0[1336], levtab1[1336];                //these could be made smaller since only one can be 1336

#define VLCBUF1SIZE 4598
#define VLCBUF2SIZE 3574
#define VLCBUF3SIZE 360
#define VLCBUF4SIZE 540

/*putting these in IRAM actually makes PP slower*/

static VLC_TYPE vlcbuf1[VLCBUF1SIZE][2];
static VLC_TYPE vlcbuf2[VLCBUF2SIZE][2];
static VLC_TYPE vlcbuf3[VLCBUF3SIZE][2];
static VLC_TYPE vlcbuf4[VLCBUF4SIZE][2];



#include "wmadata.h" // PJJ



/*
 * Helper functions for wma_window.
 *
 *
 */

#ifdef CPU_ARM
static inline
void vector_fmul_add_add(fixed32 *dst, const fixed32 *data,
                         const fixed32 *window, int n)
{
    /* Block sizes are always power of two */
    asm volatile (
        "0:"
        "ldmia %[d]!, {r0, r1};"
        "ldmia %[w]!, {r4, r5};"
        /* consume the first data and window value so we can use those
         * registers again */
        "smull r8, r9, r0, r4;"
        "ldmia %[dst], {r0, r4};"
        "add   r0, r0, r9, lsl #1;"  /* *dst=*dst+(r9<<1)*/
        "smull r8, r9, r1, r5;"
        "add   r1, r4, r9, lsl #1;"
        "stmia %[dst]!, {r0, r1};"
        "subs  %[n], %[n], #2;"
        "bne   0b;"
        : [d] "+r" (data), [w] "+r" (window), [dst] "+r" (dst), [n] "+r" (n)
        : : "r0", "r1", "r4", "r5", "r8", "r9", "memory", "cc");
}

static inline
void vector_fmul_reverse(fixed32 *dst, const fixed32 *src0, const fixed32 *src1,
                         int len)
{
    /* Block sizes are always power of two */
    asm volatile (
        "add   %[s1], %[s1], %[n], lsl #2;"
        "0:"
        "ldmia %[s0]!, {r0, r1};"
        "ldmdb %[s1]!, {r4, r5};"
        "smull r8, r9, r0, r5;"
        "mov   r0, r9, lsl #1;"
        "smull r8, r9, r1, r4;"
        "mov   r1, r9, lsl #1;"
        "stmia %[dst]!, {r0, r1};"
        "subs  %[n], %[n], #2;"
        "bne   0b;"
        : [s0] "+r" (src0), [s1] "+r" (src1), [dst] "+r" (dst), [n] "+r" (len)
        : : "r0", "r1", "r4", "r5", "r8", "r9", "memory", "cc");
}

#elif defined(CPU_COLDFIRE)

static inline
void vector_fmul_add_add(fixed32 *dst, const fixed32 *data,
                         const fixed32 *window, int n)
{
    /* Block sizes are always power of two. Smallest block is always way bigger
     * than four too.*/
    asm volatile (
        "0:"
        "movem.l (%[d]), %%d0-%%d3;"
        "movem.l (%[w]), %%d4-%%d5/%%a0-%%a1;"
        "mac.l %%d0, %%d4, %%acc0;"
        "mac.l %%d1, %%d5, %%acc1;"
        "mac.l %%d2, %%a0, %%acc2;"
        "mac.l %%d3, %%a1, %%acc3;"
        "lea.l (16, %[d]), %[d];"
        "lea.l (16, %[w]), %[w];"
        "movclr.l %%acc0, %%d0;"
        "movclr.l %%acc1, %%d1;"
        "movclr.l %%acc2, %%d2;"
        "movclr.l %%acc3, %%d3;"
        "add.l %%d0, (%[dst])+;"
        "add.l %%d1, (%[dst])+;"
        "add.l %%d2, (%[dst])+;"
        "add.l %%d3, (%[dst])+;"
        "subq.l #4, %[n];"
        "jne 0b;"
        : [d] "+a" (data), [w] "+a" (window), [dst] "+a" (dst), [n] "+d" (n)
        : : "d0", "d1", "d2", "d3", "d4", "d5", "a0", "a1", "memory", "cc");
}

static inline
void vector_fmul_reverse(fixed32 *dst, const fixed32 *src0, const fixed32 *src1,
                         int len)
{
    /* Block sizes are always power of two. Smallest block is always way bigger
     * than four too.*/
    asm volatile (
        "lea.l (-16, %[s1], %[n]*4), %[s1];"
        "0:"
        "movem.l (%[s0]), %%d0-%%d3;"
        "movem.l (%[s1]), %%d4-%%d5/%%a0-%%a1;"
        "mac.l %%d0, %%a1, %%acc0;"
        "mac.l %%d1, %%a0, %%acc1;"
        "mac.l %%d2, %%d5, %%acc2;"
        "mac.l %%d3, %%d4, %%acc3;"
        "lea.l (16, %[s0]), %[s0];"
        "lea.l (-16, %[s1]), %[s1];"
        "movclr.l %%acc0, %%d0;"
        "movclr.l %%acc1, %%d1;"
        "movclr.l %%acc2, %%d2;"
        "movclr.l %%acc3, %%d3;"
        "movem.l %%d0-%%d3, (%[dst]);"
        "lea.l (16, %[dst]), %[dst];"
        "subq.l #4, %[n];"
        "jne 0b;"
        : [s0] "+a" (src0), [s1] "+a" (src1), [dst] "+a" (dst), [n] "+d" (len)
        : : "d0", "d1", "d2", "d3", "d4", "d5", "a0", "a1", "memory", "cc");
}

#else

static inline void vector_fmul_add_add(fixed32 *dst, const fixed32 *src0, const fixed32 *src1, int len){
    int i;
    for(i=0; i<len; i++)
        dst[i] = fixmul32b(src0[i], src1[i]) + dst[i];
}

static inline void vector_fmul_reverse(fixed32 *dst, const fixed32 *src0, const fixed32 *src1, int len){
    int i;
    src1 += len-1;
    for(i=0; i<len; i++)
        dst[i] = fixmul32b(src0[i], src1[-i]);
}

#endif

/**
  * Apply MDCT window and add into output.
  *
  * We ensure that when the windows overlap their squared sum
  * is always 1 (MDCT reconstruction rule).
  *
  * The Vorbis I spec has a great diagram explaining this process.
  * See section 1.3.2.3 of http://xiph.org/vorbis/doc/Vorbis_I_spec.html
  */
 static void wma_window(WMADecodeContextPrivate *s, fixed32 *in, fixed32 *out)
 {
     //float *in = s->output;
     int block_len, bsize, n;

     /* left part */
     /*previous block was larger, so we'll use the size of the current block to set the window size*/
     if (s->block_len_bits <= s->prev_block_len_bits) {
         block_len = s->block_len;
         bsize = s->frame_len_bits - s->block_len_bits;

         vector_fmul_add_add(out, in, s->windows[bsize], block_len);

     } else {
         /*previous block was smaller or the same size, so use it's size to set the window length*/
         block_len = 1 << s->prev_block_len_bits;
         /*find the middle of the two overlapped blocks, this will be the first overlapped sample*/
         n = (s->block_len - block_len) >> 1;
         bsize = s->frame_len_bits - s->prev_block_len_bits;

         vector_fmul_add_add(out+n, in+n, s->windows[bsize],  block_len);

         memcpy(out+n+block_len, in+n+block_len, n*sizeof(fixed32));
     }
    /* Advance to the end of the current block and prepare to window it for the next block.
     * Since the window function needs to be reversed, we do it backwards starting with the
     * last sample and moving towards the first
     */
     out += s->block_len;
     in += s->block_len;

     /* right part */
     if (s->block_len_bits <= s->next_block_len_bits) {
         block_len = s->block_len;
         bsize = s->frame_len_bits - s->block_len_bits;

         vector_fmul_reverse(out, in, s->windows[bsize], block_len);

     } else {
         block_len = 1 << s->next_block_len_bits;
         n = (s->block_len - block_len) >> 1;
         bsize = s->frame_len_bits - s->next_block_len_bits;

         memcpy(out, in, n*sizeof(fixed32));

         vector_fmul_reverse(out+n, in+n, s->windows[bsize], block_len);

         memset(out+n+block_len, 0, n*sizeof(fixed32));
     }
 }




/* XXX: use same run/length optimization as mpeg decoders */
static void init_coef_vlc(VLC *vlc,
                          uint16_t **prun_table, uint16_t **plevel_table,
                          const CoefVLCTable *vlc_table, int tab)
{
    int n = vlc_table->n;
    const uint8_t *table_bits = vlc_table->huffbits;
    const uint32_t *table_codes = vlc_table->huffcodes;
    const uint16_t *levels_table = vlc_table->levels;
    uint16_t *run_table, *level_table;
    const uint16_t *p;
    int i, l, j, level;


    libwma_init_vlc(vlc, VLCBITS, n, table_bits, 1, 1, table_codes, 4, 4, 0);

    run_table = runtabarray[tab];
    level_table= levtabarray[tab];

    p = levels_table;
    i = 2;
    level = 1;
    while (i < n)
    {
        l = *p++;
        for(j=0;j<l;++j)
        {
            run_table[i] = j;
            level_table[i] = level;
            ++i;
        }
        ++level;
    }
    *prun_table = run_table;
    *plevel_table = level_table;
}

int libwma_decode_init(WMADecodeContext *ctx)
{
    WMADecodeContextPrivate *s = malloc(sizeof(WMADecodeContextPrivate));
    s->ectx = ctx;
    ctx->priv_data = s;
    int i, flags1, flags2;
    fixed32 *window;
    uint8_t *extradata = s->ectx->extradata;
    fixed64 bps1;
    fixed32 high_freq;
    fixed64 bps;
    int sample_rate1;
    int coef_vlc_table;
    //    int filehandle;
    #ifdef CPU_COLDFIRE
    coldfire_set_macsr(EMAC_FRACTIONAL | EMAC_SATURATE);
    #endif

    s->coefs = &coefsarray;

    if (s->ectx->codec_id == ASF_CODEC_ID_WMAV1) {
        s->version = 1;
    } else if (s->ectx->codec_id == ASF_CODEC_ID_WMAV2 ) {
        s->version = 2;
    } else {
        /*one of those other wma flavors that don't have GPLed decoders */
        return -1;
    }

    /* extract flag infos */
    flags1 = 0;
    flags2 = 0;
    if (s->version == 1 && s->ectx->extradata_size >= 4) {
        flags1 = extradata[0] | (extradata[1] << 8);
        flags2 = extradata[2] | (extradata[3] << 8);
    }else if (s->version == 2 && s->ectx->extradata_size >= 6){
        flags1 = extradata[0] | (extradata[1] << 8) |
                 (extradata[2] << 16) | (extradata[3] << 24);
        flags2 = extradata[4] | (extradata[5] << 8);
    }
    s->use_exp_vlc = flags2 & 0x0001;
    s->use_bit_reservoir = flags2 & 0x0002;
    s->use_variable_block_len = flags2 & 0x0004;

    /* compute MDCT block size */
    if (s->ectx->sample_rate <= 16000){
        s->frame_len_bits = 9;
    }else if (s->ectx->sample_rate <= 22050 ||
             (s->ectx->sample_rate <= 32000 && s->version == 1)){
        s->frame_len_bits = 10;
    }else{
        s->frame_len_bits = 11;
    }
    s->frame_len = 1 << s->frame_len_bits;
    if (s-> use_variable_block_len)
    {
        int nb_max, nb;
        nb = ((flags2 >> 3) & 3) + 1;
        if ((s->ectx->bit_rate / s->ectx->nb_channels) >= 32000)
        {
            nb += 2;
        }
        nb_max = s->frame_len_bits - BLOCK_MIN_BITS;        //max is 11-7
        if (nb > nb_max)
            nb = nb_max;
        s->nb_block_sizes = nb + 1;
    }
    else
    {
        s->nb_block_sizes = 1;
    }

    /* init rate dependant parameters */
    s->use_noise_coding = 1;
    high_freq = itofix64(s->ectx->sample_rate) >> 1;


    /* if version 2, then the rates are normalized */
    sample_rate1 = s->ectx->sample_rate;
    if (s->version == 2)
    {
        if (sample_rate1 >= 44100)
            sample_rate1 = 44100;
        else if (sample_rate1 >= 22050)
            sample_rate1 = 22050;
        else if (sample_rate1 >= 16000)
            sample_rate1 = 16000;
        else if (sample_rate1 >= 11025)
            sample_rate1 = 11025;
        else if (sample_rate1 >= 8000)
            sample_rate1 = 8000;
    }

    fixed64 tmp = itofix64(s->ectx->bit_rate);
    fixed64 tmp2 = itofix64(s->ectx->nb_channels * s->ectx->sample_rate);
    bps = fixdiv64(tmp, tmp2);
    fixed64 tim = bps * s->frame_len;
    fixed64 tmpi = fixdiv64(tim,itofix64(8));
    s->byte_offset_bits = libwma_log2(fixtoi64(tmpi+0x8000)) + 2;

    /* compute high frequency value and choose if noise coding should
       be activated */
    bps1 = bps;
    if (s->ectx->nb_channels == 2)
        bps1 = fixmul32(bps,0x1999a);
    if (sample_rate1 == 44100)
    {
        if (bps1 >= 0x9c29)
            s->use_noise_coding = 0;
        else
            high_freq = fixmul32(high_freq,0x6666);
    }
    else if (sample_rate1 == 22050)
    {
        if (bps1 >= 0x128f6)
            s->use_noise_coding = 0;
        else if (bps1 >= 0xb852)
            high_freq = fixmul32(high_freq,0xb333);
        else
            high_freq = fixmul32(high_freq,0x999a);
    }
    else if (sample_rate1 == 16000)
    {
        if (bps > 0x8000)
            high_freq = fixmul32(high_freq,0x8000);
        else
            high_freq = fixmul32(high_freq,0x4ccd);
    }
    else if (sample_rate1 == 11025)
    {
        high_freq = fixmul32(high_freq,0xb333);
    }
    else if (sample_rate1 == 8000)
    {
        if (bps <= 0xa000)
        {
           high_freq = fixmul32(high_freq,0x8000);
        }
        else if (bps > 0xc000)
        {
            s->use_noise_coding = 0;
        }
        else
        {
            high_freq = fixmul32(high_freq,0xa666);
        }
    }
    else
    {
        if (bps >= 0xcccd)
        {
            high_freq = fixmul32(high_freq,0xc000);
        }
        else if (bps >= 0x999a)
        {
            high_freq = fixmul32(high_freq,0x999a);
        }
        else
        {
            high_freq = fixmul32(high_freq,0x8000);
        }
    }

    /* compute the scale factor band sizes for each MDCT block size */
    {
        int a, b, pos, lpos, k, block_len, i, j, n;
        const uint8_t *table;

        if (s->version == 1)
        {
            s->coefs_start = 3;
        }
        else
        {
            s->coefs_start = 0;
        }
        for(k = 0; k < s->nb_block_sizes; ++k)
        {
            block_len = s->frame_len >> k;

            if (s->version == 1)
            {
                lpos = 0;
                for(i=0;i<25;++i)
                {
                    a = wma_critical_freqs[i];
                    b = s->ectx->sample_rate;
                    pos = ((block_len * 2 * a)  + (b >> 1)) / b;
                    if (pos > block_len)
                        pos = block_len;
                    s->exponent_bands[0][i] = pos - lpos;
                    if (pos >= block_len)
                    {
                        ++i;
                        break;
                    }
                    lpos = pos;
                }
                s->exponent_sizes[0] = i;
            }
            else
            {
                /* hardcoded tables */
                table = NULL;
                a = s->frame_len_bits - BLOCK_MIN_BITS - k;
                if (a < 3)
                {
                    if (s->ectx->sample_rate >= 44100)
                        table = exponent_band_44100[a];
                    else if (s->ectx->sample_rate >= 32000)
                        table = exponent_band_32000[a];
                    else if (s->ectx->sample_rate >= 22050)
                        table = exponent_band_22050[a];
                }
                if (table)
                {
                    n = *table++;
                    for(i=0;i<n;++i)
                        s->exponent_bands[k][i] = table[i];
                    s->exponent_sizes[k] = n;
                }
                else
                {
                    j = 0;
                    lpos = 0;
                    for(i=0;i<25;++i)
                    {
                        a = wma_critical_freqs[i];
                        b = s->ectx->sample_rate;
                        pos = ((block_len * 2 * a)  + (b << 1)) / (4 * b);
                        pos <<= 2;
                        if (pos > block_len)
                            pos = block_len;
                        if (pos > lpos)
                            s->exponent_bands[k][j++] = pos - lpos;
                        if (pos >= block_len)
                            break;
                        lpos = pos;
                    }
                    s->exponent_sizes[k] = j;
                }
            }

            /* max number of coefs */
            s->coefs_end[k] = (s->frame_len - ((s->frame_len * 9) / 100)) >> k;
            /* high freq computation */

            fixed32 tmp1 = high_freq*2;            /* high_freq is a fixed32!*/
            fixed32 tmp2=itofix32(s->ectx->sample_rate>>1);
            s->high_band_start[k] = fixtoi32( fixdiv32(tmp1, tmp2) * (block_len>>1) +0x8000);

            /*
            s->high_band_start[k] = (int)((block_len * 2 * high_freq) /
                                          s->ectx->sample_rate + 0.5);*/

            n = s->exponent_sizes[k];
            j = 0;
            pos = 0;
            for(i=0;i<n;++i)
            {
                int start, end;
                start = pos;
                pos += s->exponent_bands[k][i];
                end = pos;
                if (start < s->high_band_start[k])
                    start = s->high_band_start[k];
                if (end > s->coefs_end[k])
                    end = s->coefs_end[k];
                if (end > start)
                    s->exponent_high_bands[k][j++] = end - start;
            }
            s->exponent_high_sizes[k] = j;
        }
    }

    libwma_mdct_init_global();

    for(i = 0; i < s->nb_block_sizes; ++i)
    {
        libwma_mdct_init(&s->mdct_ctx[i], s->frame_len_bits - i + 1, 1);
    }

    /*ffmpeg uses malloc to only allocate as many window sizes as needed.  However, we're really only interested in the worst case memory usage.
    * In the worst case you can have 5 window sizes, 128 doubling up 2048
    * Smaller windows are handled differently.
    * Since we don't have malloc, just statically allocate this
    */
    fixed32 *temp[5];
    temp[0] = stat0;
    temp[1] = stat1;
    temp[2] = stat2;
    temp[3] = stat3;
    temp[4] = stat4;

    /* init MDCT windows : simple sinus window */
    for(i = 0; i < s->nb_block_sizes; i++)
    {
        int n, j;
        fixed32 alpha;
        n = 1 << (s->frame_len_bits - i);
        //window = av_malloc(sizeof(fixed32) * n);
        window = temp[i];

        //fixed32 n2 = itofix32(n<<1);        //2x the window length
        //alpha = fixdiv32(M_PI_F, n2);        //PI / (2x Window length) == PI<<(s->frame_len_bits - i+1)

        //alpha = M_PI_F>>(s->frame_len_bits - i+1);
        alpha = (1<<15)>>(s->frame_len_bits - i+1);   /* this calculates 0.5/(2*n) */
        for(j=0;j<n;++j)
        {
            fixed32 j2 = itofix32(j) + 0x8000;
            window[j] = fsincos(fixmul32(j2,alpha)<<16, 0);        //alpha between 0 and pi/2

        }
        s->windows[i] = window;

    }

    s->reset_block_lengths = 1;

    if (s->use_noise_coding)
    {
        /* init the noise generator */
        if (s->use_exp_vlc)
        {
            s->noise_mult = 0x51f;
            s->noise_table = noisetable_exp;
        }
        else
        {
            s->noise_mult = 0xa3d;
            /* LSP values are simply 2x the EXP values */
            for (i=0;i<NOISE_TAB_SIZE;++i)
                noisetable_exp[i] = noisetable_exp[i]<< 1;
            s->noise_table = noisetable_exp;
        }
#if 0
        {
            unsigned int seed;
            fixed32 norm;
            seed = 1;
            norm = 0;   // PJJ: near as makes any diff to 0!
            for (i=0;i<NOISE_TAB_SIZE;++i)
            {
                seed = seed * 314159 + 1;
                s->noise_table[i] = itofix32((int)seed) * norm;
            }
        }
#endif

         s->hgain_vlc.table = vlcbuf4;
         s->hgain_vlc.table_allocated = VLCBUF4SIZE;
         libwma_init_vlc(&s->hgain_vlc, HGAINVLCBITS, sizeof(hgain_huffbits),
                  hgain_huffbits, 1, 1,
                  hgain_huffcodes, 2, 2, 0);
    }

    if (s->use_exp_vlc)
    {

        s->exp_vlc.table = vlcbuf3;
        s->exp_vlc.table_allocated = VLCBUF3SIZE;

         libwma_init_vlc(&s->exp_vlc, EXPVLCBITS, sizeof(scale_huffbits),
                  scale_huffbits, 1, 1,
                  scale_huffcodes, 4, 4, 0);
    }
    else
    {
        wma_lsp_to_curve_init(s, s->frame_len);
    }

    /* choose the VLC tables for the coefficients */
    coef_vlc_table = 2;
    if (s->ectx->sample_rate >= 32000)
    {
        if (bps1 < 0xb852)
            coef_vlc_table = 0;
        else if (bps1 < 0x128f6)
            coef_vlc_table = 1;
    }

    runtabarray[0] = runtab0; runtabarray[1] = runtab1;
    levtabarray[0] = levtab0; levtabarray[1] = levtab1;

    s->coef_vlc[0].table = vlcbuf1;
    s->coef_vlc[0].table_allocated = VLCBUF1SIZE;
    s->coef_vlc[1].table = vlcbuf2;
    s->coef_vlc[1].table_allocated = VLCBUF2SIZE;


    init_coef_vlc(&s->coef_vlc[0], &s->run_table[0], &s->level_table[0],
                  &coef_vlcs[coef_vlc_table * 2], 0);
    init_coef_vlc(&s->coef_vlc[1], &s->run_table[1], &s->level_table[1],
                  &coef_vlcs[coef_vlc_table * 2 + 1], 1);

    s->last_superframe_len = 0;
    s->last_bitoffset = 0;

    return 0;
}


/* compute x^-0.25 with an exponent and mantissa table. We use linear
   interpolation to reduce the mantissa table size at a small speed
   expense (linear interpolation approximately doubles the number of
   bits of precision). */
static inline fixed32 pow_m1_4(WMADecodeContextPrivate *s, fixed32 x)
{
    union {
        float f;
        unsigned int v;
    } u, t;
    unsigned int e, m;
    fixed32 a, b;

    u.f = fixtof64(x);
    e = u.v >> 23;
    m = (u.v >> (23 - LSP_POW_BITS)) & ((1 << LSP_POW_BITS) - 1);
    /* build interpolation scale: 1 <= t < 2. */
    t.v = ((u.v << LSP_POW_BITS) & ((1 << 23) - 1)) | (127 << 23);
    a = s->lsp_pow_m_table1[m];
    b = s->lsp_pow_m_table2[m];

    /* lsp_pow_e_table contains 32.32 format */
    /* TODO:  Since we're unlikely have value that cover the whole
     * IEEE754 range, we probably don't need to have all possible exponents */

    return (lsp_pow_e_table[e] * (a + fixmul32(b, ftofix32(t.f))) >>32);
}

static void wma_lsp_to_curve_init(WMADecodeContextPrivate *s, int frame_len)
{
    fixed32 wdel, a, b, temp, temp2;
    int i, m;

    wdel = fixdiv32(M_PI_F, itofix32(frame_len));
    temp = fixdiv32(itofix32(1),     itofix32(frame_len));
    for (i=0; i<frame_len; ++i)
    {
        /* TODO: can probably reuse the trig_init values here */
        fsincos((temp*i)<<15, &temp2);
        /* get 3 bits headroom + 1 bit from not doubleing the values */
        s->lsp_cos_table[i] = temp2>>3;

    }
    /* NOTE: these two tables are needed to avoid two operations in
       pow_m1_4 */
    b = itofix32(1);
    int ix = 0;

    /*double check this later*/
    for(i=(1 << LSP_POW_BITS) - 1;i>=0;i--)
    {
        m = (1 << LSP_POW_BITS) + i;
        a = pow_a_table[ix++]<<4;
        s->lsp_pow_m_table1[i] = 2 * a - b;
        s->lsp_pow_m_table2[i] = b - a;
        b = a;
    }

}

/* NOTE: We use the same code as Vorbis here */
/* XXX: optimize it further with SSE/3Dnow */
static void wma_lsp_to_curve(WMADecodeContextPrivate *s,
                             fixed32 *out,
                             fixed32 *val_max_ptr,
                             int n,
                             fixed32 *lsp)
{
    int i, j;
    fixed32 p, q, w, v, val_max, temp, temp2;

    val_max = 0;
    for(i=0;i<n;++i)
    {
        /* shift by 2 now to reduce rounding error,
         * we can renormalize right before pow_m1_4
         */

        p = 0x8000<<5;
        q = 0x8000<<5;
        w = s->lsp_cos_table[i];

        for (j=1;j<NB_LSP_COEFS;j+=2)
        {
            /* w is 5.27 format, lsp is in 16.16, temp2 becomes 5.27 format */
            temp2 = ((w - (lsp[j - 1]<<11)));
            temp = q;

            /* q is 16.16 format, temp2 is 5.27, q becomes 16.16 */
            q = fixmul32b(q, temp2 )<<4;
            p = fixmul32b(p, (w - (lsp[j]<<11)))<<4;
        }

        /* 2 in 5.27 format is 0x10000000 */
        p = fixmul32(p, fixmul32b(p, (0x10000000 - w)))<<3;
        q = fixmul32(q, fixmul32b(q, (0x10000000 + w)))<<3;

        v = (p + q) >>9;  /* p/q end up as 16.16 */
        v = pow_m1_4(s, v);
        if (v > val_max)
            val_max = v;
        out[i] = v;
    }

    *val_max_ptr = val_max;
}

/* decode exponents coded with LSP coefficients (same idea as Vorbis) */
static void decode_exp_lsp(WMADecodeContextPrivate *s, int ch)
{
    fixed32 lsp_coefs[NB_LSP_COEFS];
    int val, i;

    for (i = 0; i < NB_LSP_COEFS; ++i)
    {
        if (i == 0 || i >= 8)
            val = get_bits(&s->gb, 3);
        else
            val = get_bits(&s->gb, 4);
        lsp_coefs[i] = lsp_codebook[i][val];
    }

    wma_lsp_to_curve(s,
                     s->exponents[ch],
                     &s->max_exponent[ch],
                     s->block_len,
                     lsp_coefs);
}

/* decode exponents coded with VLC codes */
static int decode_exp_vlc(WMADecodeContextPrivate *s, int ch)
{
    int last_exp, n, code;
    const uint16_t *ptr, *band_ptr;
    fixed32 v, max_scale;
    fixed32 *q,*q_end;

    /*accommodate the 60 negative indices */
    const fixed32 *pow_10_to_yover16_ptr = &pow_10_to_yover16[61];

    band_ptr = s->exponent_bands[s->frame_len_bits - s->block_len_bits];
    ptr = band_ptr;
    q = s->exponents[ch];
    q_end = q + s->block_len;
    max_scale = 0;


    if (s->version == 1)        //wmav1 only
    {
        last_exp = get_bits(&s->gb, 5) + 10;
        /* XXX: use a table */
        v = pow_10_to_yover16_ptr[last_exp];
        max_scale = v;
        n = *ptr++;
        do
        {
            *q++ = v;
        }
        while (--n);
    }
    else
        last_exp = 36;

    while (q < q_end)
    {
        code = get_vlc2(&s->gb, s->exp_vlc.table, EXPVLCBITS, EXPMAX);
        if (code < 0)
        {
            return -1;
        }
        /* NOTE: this offset is the same as MPEG4 AAC ! */
        last_exp += code - 60;
        /* XXX: use a table */
        v = pow_10_to_yover16_ptr[last_exp];
        if (v > max_scale)
        {
            max_scale = v;
        }
        n = *ptr++;
        do
        {
            *q++ = v;

        }
        while (--n);
    }

    s->max_exponent[ch] = max_scale;
    return 0;
}

/* return 0 if OK. return 1 if last block of frame. return -1 if
   unrecorrable error. */
static int wma_decode_block(WMADecodeContextPrivate *s)
{
    int n, v, a, ch, code, bsize;
    int coef_nb_bits, total_gain;
    int nb_coefs[MAX_CHANNELS];
    fixed32 mdct_norm;

    /*DEBUGF("***decode_block: %d  (%d samples of %d in frame)\n",  s->block_num, s->block_len, s->frame_len);*/

   /* compute current block length */
    if (s->use_variable_block_len)
    {
        n = libwma_log2(s->nb_block_sizes - 1) + 1;

        if (s->reset_block_lengths)
        {
            s->reset_block_lengths = 0;
            v = get_bits(&s->gb, n);
            if (v >= s->nb_block_sizes)
            {
                return -2;
            }
            s->prev_block_len_bits = s->frame_len_bits - v;
            v = get_bits(&s->gb, n);
            if (v >= s->nb_block_sizes)
            {
                return -3;
            }
            s->block_len_bits = s->frame_len_bits - v;
        }
        else
        {
            /* update block lengths */
            s->prev_block_len_bits = s->block_len_bits;
            s->block_len_bits = s->next_block_len_bits;
        }
        v = get_bits(&s->gb, n);

        if (v >= s->nb_block_sizes)
            return -4;

        s->next_block_len_bits = s->frame_len_bits - v;
    }
    else
    {
        /* fixed block len */
        s->next_block_len_bits = s->frame_len_bits;
        s->prev_block_len_bits = s->frame_len_bits;
        s->block_len_bits = s->frame_len_bits;
    }
    /* now check if the block length is coherent with the frame length */
    s->block_len = 1 << s->block_len_bits;

    if ((s->block_pos + s->block_len) > s->frame_len)
    {
        return -5;  //oddly 32k sample from tracker fails here
    }

    if (s->ectx->nb_channels == 2)
    {
        s->ms_stereo = get_bits(&s->gb, 1);
    }
    v = 0;
    for (ch = 0; ch < s->ectx->nb_channels; ++ch)
    {
        a = get_bits(&s->gb, 1);
        s->channel_coded[ch] = a;
        v |= a;
    }
    /* if no channel coded, no need to go further */
    /* XXX: fix potential framing problems */
    if (!v)
    {
        goto next;
    }

    bsize = s->frame_len_bits - s->block_len_bits;

    /* read total gain and extract corresponding number of bits for
       coef escape coding */
    total_gain = 1;
    for(;;)
    {
        a = get_bits(&s->gb, 7);
        total_gain += a;
        if (a != 127)
        {
            break;
        }
    }

    if (total_gain < 15)
        coef_nb_bits = 13;
    else if (total_gain < 32)
        coef_nb_bits = 12;
    else if (total_gain < 40)
        coef_nb_bits = 11;
    else if (total_gain < 45)
        coef_nb_bits = 10;
    else
        coef_nb_bits = 9;

    /* compute number of coefficients */
    n = s->coefs_end[bsize] - s->coefs_start;

    for(ch = 0; ch < s->ectx->nb_channels; ++ch)
    {
        nb_coefs[ch] = n;
    }
    /* complex coding */
    if (s->use_noise_coding)
    {

        for(ch = 0; ch < s->ectx->nb_channels; ++ch)
        {
            if (s->channel_coded[ch])
            {
                int i, n, a;
                n = s->exponent_high_sizes[bsize];
                for(i=0;i<n;++i)
                {
                    a = get_bits(&s->gb, 1);
                    s->high_band_coded[ch][i] = a;
                    /* if noise coding, the coefficients are not transmitted */
                    if (a)
                        nb_coefs[ch] -= s->exponent_high_bands[bsize][i];
                }
            }
        }
        for(ch = 0; ch < s->ectx->nb_channels; ++ch)
        {
            if (s->channel_coded[ch])
            {
                int i, n, val, code;

                n = s->exponent_high_sizes[bsize];
                val = (int)0x80000000;
                for(i=0;i<n;++i)
                {
                    if (s->high_band_coded[ch][i])
                    {
                        if (val == (int)0x80000000)
                        {
                            val = get_bits(&s->gb, 7) - 19;
                        }
                        else
                        {
                            //code = get_vlc(&s->gb, &s->hgain_vlc);
                            code = get_vlc2(&s->gb, s->hgain_vlc.table, HGAINVLCBITS, HGAINMAX);
                            if (code < 0)
                            {
                                return -6;
                            }
                            val += code - 18;
                        }
                        s->high_band_values[ch][i] = val;
                    }
                }
            }
        }
    }

    /* exponents can be reused in short blocks. */
    if ((s->block_len_bits == s->frame_len_bits) || get_bits(&s->gb, 1))
    {
        for(ch = 0; ch < s->ectx->nb_channels; ++ch)
        {
            if (s->channel_coded[ch])
            {
                if (s->use_exp_vlc)
                {
                    if (decode_exp_vlc(s, ch) < 0)
                    {
                        return -7;
                    }
                }
                else
                {
                    decode_exp_lsp(s, ch);
                }
                s->exponents_bsize[ch] = bsize;
            }
        }
    }

    /* parse spectral coefficients : just RLE encoding */
    for(ch = 0; ch < s->ectx->nb_channels; ++ch)
    {
        if (s->channel_coded[ch])
        {
            VLC *coef_vlc;
            int level, run, sign, tindex;
            int16_t *ptr, *eptr;
            const int16_t *level_table, *run_table;

            /* special VLC tables are used for ms stereo because
               there is potentially less energy there */
            tindex = (ch == 1 && s->ms_stereo);
            coef_vlc = &s->coef_vlc[tindex];
            run_table = s->run_table[tindex];
            level_table = s->level_table[tindex];
            /* XXX: optimize */
            ptr = &s->coefs1[ch][0];
            eptr = ptr + nb_coefs[ch];
            memset(ptr, 0, s->block_len * sizeof(int16_t));

            for(;;)
            {
                code = get_vlc2(&s->gb, coef_vlc->table, VLCBITS, VLCMAX);
                //code = get_vlc(&s->gb, coef_vlc);
                if (code < 0)
                {
                    return -8;
                }
                if (code == 1)
                {
                    /* EOB */
                    break;
                }
                else if (code == 0)
                {
                    /* escape */
                    level = get_bits(&s->gb, coef_nb_bits);
                    /* NOTE: this is rather suboptimal. reading
                       block_len_bits would be better */
                    run = get_bits(&s->gb, s->frame_len_bits);
                }
                else
                {
                    /* normal code */
                    run = run_table[code];
                    level = level_table[code];
                }
                sign = get_bits(&s->gb, 1);
                if (!sign)
                    level = -level;
                ptr += run;
                if (ptr >= eptr)
                {
                    break;
                }
                *ptr++ = level;


                /* NOTE: EOB can be omitted */
                if (ptr >= eptr)
                    break;
            }
        }
        if (s->version == 1 && s->ectx->nb_channels >= 2)
        {
            align_get_bits(&s->gb);
        }
    }

    {
        int n4 = s->block_len >> 1;
        //mdct_norm = 0x10000;
        //mdct_norm = fixdiv32(mdct_norm,itofix32(n4));

        mdct_norm = 0x10000>>(s->block_len_bits-1);        //theres no reason to do a divide by two in fixed precision ...

        if (s->version == 1)
        {
            mdct_norm *= fixtoi32(fixsqrt32(itofix32(n4))); /* PJJ : exercise this path */
        }
    }


    /* finally compute the MDCT coefficients */
    for(ch = 0; ch < s->ectx->nb_channels; ++ch)
    {
        if (s->channel_coded[ch])
        {
            int16_t *coefs1;
            fixed32 *exponents, *exp_ptr;
            fixed32 *coefs, atemp;
            fixed64 mult;
            fixed64 mult1;
            fixed32 noise, temp1, temp2, mult2;
            int i, j, n, n1, last_high_band, esize;
            fixed32 exp_power[HIGH_BAND_MAX_SIZE];

            //total_gain, coefs1, mdctnorm are lossless

            coefs1 = s->coefs1[ch];
            exponents = s->exponents[ch];
            esize = s->exponents_bsize[ch];
            coefs = (*(s->coefs))[ch];

            n=0;

          /*
          *  Previously the IMDCT was run in 17.15 precision to avoid overflow. However rare files could
          *  overflow here as well, so switch to 17.15 during coefs calculation.
          */


            if (s->use_noise_coding)
            {
                /*TODO:  mult should be converted to 32 bit to speed up noise coding*/

                mult = fixdiv64(pow_table[total_gain+20],Fixed32To64(s->max_exponent[ch]));
                mult = mult* mdct_norm;        //what the hell?  This is actually fixed64*2^16!
                mult1 = mult;

                /* very low freqs : noise */
                for(i = 0;i < s->coefs_start; ++i)
                {
                    *coefs++ = fixmul32( (fixmul32(s->noise_table[s->noise_index],(*exponents++))>>4),Fixed32From64(mult1)) >>1;
                    s->noise_index = (s->noise_index + 1) & (NOISE_TAB_SIZE - 1);
                }

                n1 = s->exponent_high_sizes[bsize];

                /* compute power of high bands */
                exp_ptr = exponents +
                          s->high_band_start[bsize] -
                          s->coefs_start;
                last_high_band = 0; /* avoid warning */
                for (j=0;j<n1;++j)
                {
                    n = s->exponent_high_bands[s->frame_len_bits -
                                               s->block_len_bits][j];
                    if (s->high_band_coded[ch][j])
                    {
                        fixed32 e2, v;
                        e2 = 0;
                        for(i = 0;i < n; ++i)
                        {
                            /*v is noramlized later on so its fixed format is irrelevant*/
                            v = exp_ptr[i]>>4;
                            e2 += fixmul32(v, v)>>3;
                        }
                         exp_power[j] = e2/n; /*n is an int...*/
                        last_high_band = j;
                    }
                    exp_ptr += n;
                }

                /* main freqs and high freqs */
                for(j=-1;j<n1;++j)
                {
                    if (j < 0)
                    {
                        n = s->high_band_start[bsize] -
                            s->coefs_start;
                    }
                    else
                    {
                        n = s->exponent_high_bands[s->frame_len_bits -
                                                   s->block_len_bits][j];
                    }
                    if (j >= 0 && s->high_band_coded[ch][j])
                    {
                        /* use noise with specified power */
                        fixed32 tmp = fixdiv32(exp_power[j],exp_power[last_high_band]);
                        mult1 = (fixed64)fixsqrt32(tmp);
                        /* XXX: use a table */
                        /*mult1 is 48.16, pow_table is 48.16*/
                        mult1 = mult1 * pow_table[s->high_band_values[ch][j]+20] >> PRECISION;

                        /*this step has a fairly high degree of error for some reason*/
                           mult1 = fixdiv64(mult1,fixmul32(s->max_exponent[ch],s->noise_mult));

                         mult1 = mult1*mdct_norm>>PRECISION;
                        for(i = 0;i < n; ++i)
                        {
                            noise = s->noise_table[s->noise_index];
                            s->noise_index = (s->noise_index + 1) & (NOISE_TAB_SIZE - 1);
                            *coefs++ = fixmul32((fixmul32(*exponents,noise)>>4),Fixed32From64(mult1)) >>1;
                            ++exponents;
                        }
                    }
                    else
                    {
                        /* coded values + small noise */
                        for(i = 0;i < n; ++i)
                        {
                            // PJJ: check code path
                            noise = s->noise_table[s->noise_index];
                            s->noise_index = (s->noise_index + 1) & (NOISE_TAB_SIZE - 1);

                           /*don't forget to renormalize the noise*/
                           temp1 = (((int32_t)*coefs1++)<<16) + (noise>>4);
                           temp2 = fixmul32(*exponents, mult>>17);
                           *coefs++ = fixmul32(temp1, temp2);
                           ++exponents;
                        }
                    }
                }

                /* very high freqs : noise */
                n = s->block_len - s->coefs_end[bsize];
                mult2 = fixmul32(mult>>16,exponents[-1]) ;  /*the work around for 32.32 vars are getting stupid*/
                for (i = 0; i < n; ++i)
                {
                    /*renormalize the noise product and then reduce to 17.15 precison*/
                    *coefs++ = fixmul32(s->noise_table[s->noise_index],mult2) >>5;

                    s->noise_index = (s->noise_index + 1) & (NOISE_TAB_SIZE - 1);
                }
            }
            else
            {
                /*Noise coding not used, simply convert from exp to fixed representation*/


                fixed32 mult3 = (fixed32)(fixdiv64(pow_table[total_gain+20],Fixed32To64(s->max_exponent[ch])));
                mult3 = fixmul32(mult3, mdct_norm);

                n = nb_coefs[ch];

                /* XXX: optimize more, unrolling this loop in asm might be a good idea */
                for(i = 0;i < s->coefs_start; i++)
                    *coefs++ = 0;
                for(i = 0;i < n; ++i)
                {
                    atemp = (coefs1[i] * mult3)>>1;
                    *coefs++=fixmul32(atemp,exponents[i<<bsize>>esize]);
                }
                n = s->block_len - s->coefs_end[bsize];
                memset(coefs, 0, n*sizeof(fixed32));
            }
        }
    }



    if (s->ms_stereo && s->channel_coded[1])
    {
        fixed32 a, b;
        int i;
        fixed32 (*coefs)[MAX_CHANNELS][BLOCK_MAX_SIZE]  = (s->coefs);

        /* nominal case for ms stereo: we do it before mdct */
        /* no need to optimize this case because it should almost
           never happen */
        if (!s->channel_coded[0])
        {
            memset((*(s->coefs))[0], 0, sizeof(fixed32) * s->block_len);
            s->channel_coded[0] = 1;
        }

        for(i = 0; i < s->block_len; ++i)
        {
            a = (*coefs)[0][i];
            b = (*coefs)[1][i];
            (*coefs)[0][i] = a + b;
            (*coefs)[1][i] = a - b;
        }
    }

    for(ch = 0; ch < s->ectx->nb_channels; ++ch)
    {
        if (s->channel_coded[ch])
        {
            static fixed32  output[BLOCK_MAX_SIZE * 2];

            int n4, index, n;

            n = s->block_len;
            n4 = s->block_len >>1;

            libwma_imdct_calc(&s->mdct_ctx[bsize],
                          output,
                          (*(s->coefs))[ch]);


            /* add in the frame */
            index = (s->frame_len / 2) + s->block_pos - n4;

            wma_window(s, output, &s->frame_out[ch][index]);



            /* specific fast case for ms-stereo : add to second
               channel if it is not coded */
            if (s->ms_stereo && !s->channel_coded[1])
            {
                wma_window(s, output, &s->frame_out[1][index]);
            }
        }
    }
next:
    /* update block number */
    ++s->block_num;
    s->block_pos += s->block_len;
    if (s->block_pos >= s->frame_len)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/* decode a frame of frame_len samples */
static int wma_decode_frame(WMADecodeContextPrivate *s, int16_t *samples)
{
    int ret, i, n, ch, incr;
    int16_t *ptr;
    fixed32 *iptr;
   // rb->splash(HZ, "in wma_decode_frame");

    /* read each block */
    s->block_num = 0;
    s->block_pos = 0;


    for(;;)
    {
        ret = wma_decode_block(s);
        if (ret < 0)
        {

            DEBUGF("wma_decode_block failed with code %d\n", ret);
            return -1;
        }
        if (ret)
        {
            break;
        }
    }

    n = s->frame_len;
    incr = s->ectx->nb_channels;
    for(ch = 0; ch < s->ectx->nb_channels; ++ch)
    {
        ptr = samples + ch;
        iptr = s->frame_out[ch];

        for (i=0;i<n;++i)
        {
            *ptr = (*iptr++) >> 16;
            ptr += incr;
        }
        /* prepare for next block */
        memmove(&s->frame_out[ch][0], &s->frame_out[ch][s->frame_len],
                s->frame_len * sizeof(fixed32));

    }

    return 0;
}

void libwma_decode_end(WMADecodeContext *ctx)
{
    WMADecodeContextPrivate *s = ctx->priv_data;
    free(s);
}

int libwma_decode_superframe(WMADecodeContext *ctx,
                             int16_t *samples, int *data_size,
                             const uint8_t *buf, int buf_size)
{
    WMADecodeContextPrivate *s = ctx->priv_data;
    int nb_frames, bit_offset, i, pos, len;
    uint8_t *q;
    int16_t *samples_start = samples;

    if(buf_size==0){
        s->last_superframe_len = 0;
        return 0;
    }
    if (buf_size < s->ectx->block_align)
        return 0;
    buf_size = s->ectx->block_align;

    init_get_bits(&s->gb, buf, buf_size*8);

    if (s->use_bit_reservoir) {
        /* read super frame header */
        skip_bits(&s->gb, 4); /* super frame index */
        nb_frames = get_bits(&s->gb, 4) - 1;

        bit_offset = get_bits(&s->gb, s->byte_offset_bits + 3);

        if (s->last_superframe_len > 0) {
            //        printf("skip=%d\n", s->last_bitoffset);
            /* add bit_offset bits to last frame */
            if ((s->last_superframe_len + ((bit_offset + 7) >> 3)) >
                MAX_CODED_SUPERFRAME_SIZE)
                goto fail;
            q = s->last_superframe + s->last_superframe_len;
            len = bit_offset;
            while (len > 7) {
                *q++ = (get_bits)(&s->gb, 8);
                len -= 8;
            }
            if (len > 0) {
                *q++ = (get_bits)(&s->gb, len) << (8 - len);
            }

            /* XXX: bit_offset bits into last frame */
            init_get_bits(&s->gb, s->last_superframe, MAX_CODED_SUPERFRAME_SIZE*8);
            /* skip unused bits */
            if (s->last_bitoffset > 0)
                skip_bits(&s->gb, s->last_bitoffset);
            /* this frame is stored in the last superframe and in the
               current one */
            if (wma_decode_frame(s, samples) < 0)
                goto fail;
            samples += s->ectx->nb_channels * s->frame_len;
        }

        /* read each frame starting from bit_offset */
        pos = bit_offset + 4 + 4 + s->byte_offset_bits + 3;
        init_get_bits(&s->gb, buf + (pos >> 3), (MAX_CODED_SUPERFRAME_SIZE - (pos >> 3))*8);
        len = pos & 7;
        if (len > 0)
            skip_bits(&s->gb, len);

        s->reset_block_lengths = 1;
        for(i=0;i<nb_frames;i++) {
            if (wma_decode_frame(s, samples) < 0)
                goto fail;
            samples += s->ectx->nb_channels * s->frame_len;
        }

        /* we copy the end of the frame in the last frame buffer */
        pos = get_bits_count(&s->gb) + ((bit_offset + 4 + 4 + s->byte_offset_bits + 3) & ~7);
        s->last_bitoffset = pos & 7;
        pos >>= 3;
        len = buf_size - pos;
        if (len > MAX_CODED_SUPERFRAME_SIZE || len < 0) {
            goto fail;
        }
        s->last_superframe_len = len;
        memcpy(s->last_superframe, buf + pos, len);
    } else {
        /* single frame decode */
        if (wma_decode_frame(s, samples) < 0)
            goto fail;
        samples += s->ectx->nb_channels * s->frame_len;
    }

//av_log(NULL, AV_LOG_ERROR, "%d %d %d %d outbytes:%d eaten:%d\n", s->frame_len_bits, s->block_len_bits, s->frame_len, s->block_len,        (int8_t *)samples - (int8_t *)data, s->ectx->block_align);

    *data_size = (int8_t *)samples - (int8_t *)samples_start;
    return s->ectx->block_align;
 fail:
    /* when error, we reset the bit reservoir */
    s->last_superframe_len = 0;
    return -1;
}
