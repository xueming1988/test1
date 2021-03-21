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

#ifndef _WMADEC_H
#define _WMADEC_H

#include <inttypes.h>

typedef struct WMADecodeContext
{
    int sample_rate;
    int nb_channels;
    int bit_rate;
    int block_align;
    int codec_id;
    uint8_t *extradata;
    int extradata_size;
    void *priv_data;
} WMADecodeContext;

int libwma_decode_init(WMADecodeContext* ctx);
void libwma_decode_end(WMADecodeContext* ctx);
int libwma_decode_superframe(WMADecodeContext* ctx,
                             int16_t *samples, int *data_size,
                             const uint8_t *buf, int buf_size);
#endif
