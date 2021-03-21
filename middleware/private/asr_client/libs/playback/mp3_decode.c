/*************************************************************************
    > File Name: mp3_decode.c
    > Author:
    > Mail:
    > Created Time: Tue 21 Nov 2017 02:35:55 AM EST
 ************************************************************************/

#include <mad.h>
#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>
#include <alsa/asoundlib.h>

#include "mp3_decode.h"
#include "wm_util.h"

#define MP3_HEADER_LEN (4)
#define OUTPUT_BUFFER_SIZE (8192)
#define INPUT_BUFFER_SIZE (8192)

#define os_power_on_tick() tickSincePowerOn()

#define MP3_DECODE_DEBUG(fmt, ...)                                                                 \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d][MP3_DECODE] " fmt,                                  \
                (int)(os_power_on_tick() / 3600000), (int)(os_power_on_tick() % 3600000 / 60000),  \
                (int)(os_power_on_tick() % 60000 / 1000), (int)(os_power_on_tick() % 1000),        \
                ##__VA_ARGS__);                                                                    \
    } while (0)

struct _mp3_decode_s {
    int header_len;
    int frame_len;
    unsigned char mp3_header[MP3_HEADER_LEN];
    unsigned char back_header[MP3_HEADER_LEN];
    mp3_player_ctx_t player;
    struct mad_stream stream;
    struct mad_frame frame;
    struct mad_synth synth;
    unsigned char input_buf[INPUT_BUFFER_SIZE];
};

static short lcg_rand(void)
{
    static unsigned long lcg_prev = 12345;
    lcg_prev = lcg_prev * 69069 + 3;
    return lcg_prev & 0xffff;
}

static inline short dithered_vol(short sample, int fix_volume)
{
    static short rand_a, rand_b;
    long out;

    out = (long)sample * fix_volume;
    if (fix_volume < 0x10000) {
        rand_b = rand_a;
        rand_a = lcg_rand();
        out += rand_a;
        out -= rand_b;
    }

    return out >> 16;
}

static int scale(mad_fixed_t sample)
{
    sample += (1L << (MAD_F_FRACBITS - 16));
    if (sample >= MAD_F_ONE)
        sample = MAD_F_ONE - 1;
    else if (sample < -MAD_F_ONE)
        sample = -MAD_F_ONE;

    return sample >> (MAD_F_FRACBITS + 1 - 16);
}

mp3_decode_t *decode_init(mp3_player_ctx_t ctx)
{
    mp3_decode_t *self = calloc(1, sizeof(mp3_decode_t));
    if (self) {
        // memset(self->mp3_header, 0x0, sizeof(self->mp3_header));
        // memset(self->back_header, 0x0, sizeof(self->back_header));
        memcpy(&self->player, &ctx, sizeof(mp3_player_ctx_t));
        mad_stream_init(&self->stream);
        mad_frame_init(&self->frame);
        mad_synth_init(&self->synth);
        // memset(self->input_buf, 0x0, sizeof(self->input_buf));
        return self;
    }

    return NULL;
}

int decode_uninit(mp3_decode_t *self)
{
    if (self) {
        mad_synth_finish(&self->synth);
        mad_frame_finish(&self->frame);
        mad_stream_finish(&self->stream);
        if (self->player.device_uninit) {
            self->player.device_uninit(self->player.player_ctx);
        }
        free(self);
    }

    return 0;
}

//----------------------- mp3 audio frame header parser -----------------------
static const uint16_t tabsel_123[2][3][16] = {
    {{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0},
     {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
     {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0}},

    {{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
     {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
     {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}}};

static const int freqs[9] = {44100, 48000, 32000, // MPEG 1.0
                             22050, 24000, 16000, // MPEG 2.0
                             11025, 12000, 8000}; // MPEG 2.5

/*
 * return frame size or -1 (bad frame)
 */
static inline int mp_get_mp3_header(unsigned char *hbuf)
{
    int stereo, lsf, framesize, padding, bitrate_index, sampling_frequency, divisor;
    int bitrate;
    int layer;
    static const int mult[3] = {12000, 144000, 144000};
    uint32_t newhead = hbuf[0] << 24 | hbuf[1] << 16 | hbuf[2] << 8 | hbuf[3];

    if ((newhead & 0xffe00000) != 0xffe00000 || (newhead & 0x00000c00) == 0x00000c00) {
        return -1;
    }

    layer = 4 - ((newhead >> 17) & 3);
    if (layer == 4) {
        MP3_DECODE_DEBUG("wrong layer\n");
        return -1;
    }

    sampling_frequency = (newhead >> 10) & 0x3; // valid: 0..2
    if (sampling_frequency == 3) {
        MP3_DECODE_DEBUG("wrong sampling_frequency\n");
        return -1;
    }

    if (newhead & (1 << 20)) {
        // MPEG 1.0 (lsf==0) or MPEG 2.0 (lsf==1)
        lsf = !(newhead & (1 << 19));
        sampling_frequency += lsf * 3;
    } else {
        // MPEG 2.5
        lsf = 1;
        sampling_frequency += 6;
    }

    bitrate_index = (newhead >> 12) & 0xf; // valid: 1..14
    padding = (newhead >> 9) & 0x1;
    stereo = (((newhead >> 6) & 0x3) == 3) ? 1 : 2;

    bitrate = tabsel_123[lsf][layer - 1][bitrate_index];
    framesize = bitrate * mult[layer - 1];

    if (!framesize) {
        MP3_DECODE_DEBUG("wrong framesize\n");
        return -1;
    }

    divisor = layer == 3 ? (freqs[sampling_frequency] << lsf) : freqs[sampling_frequency];
    framesize /= divisor;
    framesize += padding;
    if (layer == 1) {
        framesize *= 4;
    }

    return framesize;
}

static inline int decode_find_mp3_frame(unsigned char *buf, int buf_len, int *index)
{
    int frame_len = 0;
    int less_len = buf_len;
    unsigned char *ptr = buf;

    while (less_len >= MP3_HEADER_LEN) {
        // find a mp3 frame, return the frame's size
        frame_len = mp_get_mp3_header(ptr);
        if (frame_len <= 0) {
            // not a mp3 frame
        } else {
            *index = buf_len - less_len;
            break;
        }
        ptr++;
        less_len--;
    }

    return frame_len;
}

static int decode_read(mp3_decode_t *self, char *read_start, int read_size, int *net_is_slow)
{
    int read_len = 0;
    int head_len = 0;
    int frame_len = 0;
    int index = -1;
    unsigned char cache_buff[MP3_HEADER_LEN * 2] = {0};

    // The frame is read finished or the fist time read data, then to find the next mp3 frame
    if (self->frame_len == 0) {
        // Only read 4 bytes each time
        read_len = self->player.read_data(self->player.player_ctx,
                                          (char *)self->mp3_header + self->header_len,
                                          MP3_HEADER_LEN - self->header_len);
        if (read_len <= 0) {
            if (read_len == 0) {
                *net_is_slow = 1;
            }
            return read_len;
        } else if (read_len < (MP3_HEADER_LEN - self->header_len)) {
            self->header_len += read_len;
            return 0;
        }
        self->header_len = 0;

        memcpy(cache_buff, self->back_header, MP3_HEADER_LEN);
        memcpy(cache_buff + MP3_HEADER_LEN, self->mp3_header, MP3_HEADER_LEN);
        memcpy(self->back_header, self->mp3_header, MP3_HEADER_LEN);

        // To find a mp3 frame, return the frame's size(include header), the index is where is find
        // the mp3 frame from the cache buff
        frame_len = decode_find_mp3_frame(cache_buff, MP3_HEADER_LEN * 2, &index);
        if (frame_len <= 0) {
            // not a mp3 frame, drop this data
            return 0;
        } else {
            // Must clear the cache buff
            memset(self->back_header, 0x0, MP3_HEADER_LEN);
            memset(self->mp3_header, 0x0, MP3_HEADER_LEN);
            self->frame_len = frame_len;
        }
    }

    if (self->frame_len > 0) {
        // Only need copy the header data when find the mp3 frame header
        if (index > 0) {
            head_len = MP3_HEADER_LEN * 2 - index;
            memcpy(read_start, cache_buff + index, head_len);
        }

        read_len = self->player.read_data(self->player.player_ctx, read_start + head_len,
                                          self->frame_len - head_len);
        if (read_len > 0) {
            // read length need include the header data
            read_len = read_len + head_len;
            self->frame_len -= read_len;
            self->frame_len = self->frame_len > 0 ? self->frame_len : 0;
        } else if (read_len == 0) {
            *net_is_slow = 1;
        }
    } else {
        // Never been here!!!!!!!
        MP3_DECODE_DEBUG("================== Never Been Here ================== \n");
        read_len = -1;
    }

    return read_len;
}

int decode_start(mp3_decode_t *self, int *dec_size)
{
    int ret = MP3_DEC_RET_OK;
    int net_is_slow = 0;

    if (!self || !self->player.read_data) {
        MP3_DECODE_DEBUG("input is NULL\n");
        return MP3_DEC_RET_EXIT;
    }

    // get decode data
    if (self->stream.buffer == NULL || self->stream.error == MAD_ERROR_BUFLEN) {
        int read_size = 0;
        int remain_size = 0;
        unsigned char *read_start = NULL;

        if (self->stream.next_frame != NULL) {
            remain_size = self->stream.bufend - self->stream.next_frame;
            memmove(self->input_buf, self->stream.next_frame, remain_size);
            read_start = self->input_buf + remain_size;
            read_size = INPUT_BUFFER_SIZE - remain_size;
        } else {
            read_size = INPUT_BUFFER_SIZE;
            read_start = self->input_buf;
            remain_size = 0;
        }

        read_size = decode_read(self, (char *)read_start, read_size, &net_is_slow);
        if (read_size == 0) {
            if (net_is_slow == 0) {
                self->stream.error = MAD_ERROR_BUFLEN;
                *dec_size = 0;
                ret = MP3_DEC_RET_WARN;
            } else {
                ret = MP3_DEC_RET_NETSLOW;
            }
            goto exit;
        } else if (read_size > 0) {
            *dec_size = read_size;
        } else {
            ret = MP3_DEC_RET_EXIT;
            goto exit;
        }

        mad_stream_buffer(&self->stream, self->input_buf, read_size + remain_size);
        self->stream.error = 0;
    }

    mad_stream_sync(&self->stream);
    if (mad_frame_decode(&self->frame, &self->stream)) {
        if (MAD_RECOVERABLE(self->stream.error)) {
            if (self->stream.error != MAD_ERROR_LOSTSYNC) {
                MP3_DECODE_DEBUG("mad_frame_decode wran : code 0x%x !\n", self->stream.error);
            }
            if (self->stream.error == MAD_ERROR_BADDATAPTR) {
                // mad_stream_skip(&self->stream, self->stream.skiplen);
            }
            ret = MP3_DEC_RET_WARN;
            goto exit;
        } else if (self->stream.error == MAD_ERROR_BUFLEN) {
            // MP3_DECODE_DEBUG("mad_frame_decode under run : code 0x%x !\n", self->stream.error);
            ret = MP3_DEC_RET_WARN;
            goto exit;
        } else {
            MP3_DECODE_DEBUG("!!!!!!!mad_frame_decode error code 0x%x !\n", self->stream.error);
            ret = MP3_DEC_RET_EXIT;
            goto exit;
        }
    }

    mad_synth_frame(&self->synth, &self->frame);
    ret = MP3_DEC_RET_OK;

exit:
    return ret;
}

int decode_play(mp3_decode_t *self)
{
    int fmt = 0;
    unsigned int nchannels, nsamples, n;
    mad_fixed_t const *left_ch, *right_ch;
    unsigned char output_buf[OUTPUT_BUFFER_SIZE] = {0};
    unsigned short *output_ptr = NULL;

    if (!self || !self->player.device_init || !self->player.play) {
        MP3_DECODE_DEBUG("input is NULL\n");
        return -1;
    }

    nchannels = self->synth.pcm.channels;
    n = nsamples = self->synth.pcm.length;
    left_ch = self->synth.pcm.samples[0];
    right_ch = self->synth.pcm.samples[1];

    fmt = SND_PCM_FORMAT_S16_LE;
    self->player.device_init(self->player.player_ctx, fmt, &self->synth.pcm.samplerate,
                             self->synth.pcm.channels);

    output_ptr = (unsigned short *)output_buf;
    while (nsamples--) {
        int fix_volume = 0x10000;
        signed int sample;
        sample = scale(*left_ch++);

        if (self->player.get_volume) {
            fix_volume = self->player.get_volume(self->player.player_ctx);
        }
        *(output_ptr++) = dithered_vol(sample & 0xffff, fix_volume);

        if (nchannels == 2) {
            sample = scale(*right_ch++);
            *(output_ptr++) = dithered_vol(sample & 0xffff, fix_volume);
        } else if (nchannels == 1) {
            *(output_ptr++) = dithered_vol(sample & 0xffff, fix_volume);
        }
    }

    return self->player.play(self->player.player_ctx, (unsigned short *)output_buf, n);
}
