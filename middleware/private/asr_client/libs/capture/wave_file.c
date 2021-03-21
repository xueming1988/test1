/*************************************************************************
    > File Name: wave_file.c
    > Author:
    > Mail:
    > Created Time: Sat 12 May 2018 07:39:29 AM EDT
 ************************************************************************/

#include <stdio.h>
#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <alsa/asoundlib.h>
#include <assert.h>
#include <sys/poll.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <asm/byteorder.h>
#include <mad.h>
#include <alsa/asoundlib.h>
#include <stdint.h>
#include "pthread.h"
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h> /* ioctl */

#include "wm_util.h"

#define _LARGEFILE64_SOURCE

#if CXDISH_6CH_SAMPLE_RATE
#undef USB_CAPTURE_SAMPLERATE
#define USB_CAPTURE_SAMPLERATE CXDISH_6CH_SAMPLE_RATE
#endif
#define SAMPLE_RATE_FAR (USB_CAPTURE_SAMPLERATE)

extern WIIMU_CONTEXT *g_wiimu_shm;

/* it's in chunks like .voc and AMIGA iff, but my source say there
   are in only in this combination, so I combined them in one header;
   it works on all WAVE-file I have
 */
typedef struct {
    u_int magic;  /* 'RIFF' */
    u_int length; /* filelen */
    u_int type;   /* 'WAVE' */
} WaveHeader;

typedef struct {
    u_short format;  /* should be 1 for PCM-code */
    u_short modus;   /* 1 Mono, 2 Stereo */
    u_int sample_fq; /* frequence of sample */
    u_int byte_p_sec;
    u_short byte_p_spl; /* samplesize; 1 or 2 bytes */
    u_short bit_p_spl;  /* 8, 12 or 16 bit */
} WaveFmtBody;

typedef struct {
    u_int type;   /* 'data' */
    u_int length; /* samplecount */
} WaveChunkHeader;

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define COMPOSE_ID(a, b, c, d) ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))
#define LE_SHORT(v) (v)
#define LE_INT(v) (v)
#define BE_SHORT(v) bswap_16(v)
#define BE_INT(v) bswap_32(v)
#elif __BYTE_ORDER == __BIG_ENDIAN
#define COMPOSE_ID(a, b, c, d) ((d) | ((c) << 8) | ((b) << 16) | ((a) << 24))
#define LE_SHORT(v) bswap_16(v)
#define LE_INT(v) bswap_32(v)
#define BE_SHORT(v) (v)
#define BE_INT(v) (v)
#else
#error "Wrong endian"
#endif

#define WAV_RIFF COMPOSE_ID('R', 'I', 'F', 'F')
#define WAV_WAVE COMPOSE_ID('W', 'A', 'V', 'E')
#define WAV_FMT COMPOSE_ID('f', 'm', 't', ' ')
#define WAV_DATA COMPOSE_ID('d', 'a', 't', 'a')
#define WAV_PCM_CODE 1

/* write a WAVE-header */
void begin_wave(int fd, size_t cnt, int channels)
{
    // int channels = Cosume_File.pre_process ? 2 : 1;
    int rate = AMLOGIC_I2S_CAPTURE_SAMPLERATE;
    int format = SND_PCM_FORMAT_S16_LE;

#ifdef USB_CAPDSP_MODULE
    rate = SAMPLE_RATE_FAR;
#else
    if (g_wiimu_shm->priv_cap1 & (1 << CAP1_USB_RECORDER)) {
        rate = SAMPLE_RATE_FAR;
    }
#endif

    WaveHeader h;
    WaveFmtBody f;
    WaveChunkHeader cf, cd;
    int bits;
    u_int tmp;
    u_short tmp2;

    /* WAVE cannot handle greater than 32bit (signed?) int */
    if (cnt == (size_t)-2)
        cnt = 0x7fffff00;

    bits = 16;
    h.magic = WAV_RIFF;
    tmp = cnt + sizeof(WaveHeader) + sizeof(WaveChunkHeader) + sizeof(WaveFmtBody) +
          sizeof(WaveChunkHeader) - 8;
    h.length = LE_INT(tmp);
    h.type = WAV_WAVE;

    cf.type = WAV_FMT;
    cf.length = LE_INT(16);

    f.format = LE_SHORT(WAV_PCM_CODE);
    f.modus = LE_SHORT(channels);
    f.sample_fq = LE_INT(rate);
    tmp2 = channels * snd_pcm_format_physical_width(format) / 8;
    f.byte_p_spl = LE_SHORT(tmp2);
    tmp = (u_int)tmp2 * rate;
    f.byte_p_sec = LE_INT(tmp);
    f.bit_p_spl = LE_SHORT(bits);

    cd.type = WAV_DATA;
    cd.length = LE_INT(cnt);

    if (write(fd, &h, sizeof(WaveHeader)) != sizeof(WaveHeader) ||
        write(fd, &cf, sizeof(WaveChunkHeader)) != sizeof(WaveChunkHeader) ||
        write(fd, &f, sizeof(WaveFmtBody)) != sizeof(WaveFmtBody) ||
        write(fd, &cd, sizeof(WaveChunkHeader)) != sizeof(WaveChunkHeader)) {
        printf("error happen\n");
    }
}

void end_wave(int fd, unsigned int fdcount)
{
#if !CXDISH_6CH_SAMPLE_RATE
    /* only close output */
    WaveChunkHeader cd;
    long long length_seek;
    long long filelen;
    u_int rifflen;

    length_seek = sizeof(WaveHeader) + sizeof(WaveChunkHeader) + sizeof(WaveFmtBody);
    cd.type = WAV_DATA;
    cd.length = LE_INT(fdcount); // > 0x7fffffff ? LE_INT(0x7fffffff) : LE_INT(fdcount);
    filelen = fdcount + 2 * sizeof(WaveChunkHeader) + sizeof(WaveFmtBody) + 4;
    // rifflen = filelen > 0x7fffffff ? LE_INT(0x7fffffff) : LE_INT(filelen);
    rifflen = filelen > 0xffffffff ? LE_INT(0xffffffff) : LE_INT(filelen);
    if (lseek64(fd, 4, SEEK_SET) == 4)
        write(fd, &rifflen, 4);
    if (lseek64(fd, length_seek, SEEK_SET) == length_seek)
        write(fd, &cd, sizeof(WaveChunkHeader));
#endif
    if (fd != 1)
        close(fd);
}
