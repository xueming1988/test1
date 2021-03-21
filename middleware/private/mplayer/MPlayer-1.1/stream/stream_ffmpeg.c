/*
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <unistd.h>
#include "config.h"

#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "mp_msg.h"
#include "stream.h"
#include "m_option.h"
#include "m_struct.h"
#include "av_helpers.h"
#include "tcp.h"
#include "network.h"
#include "wm_util.h"

extern WIIMU_CONTEXT* g_wiimu_shm;
static int seek(stream_t *s, off_t newpos)
{
    s->pos = newpos;
    if (avio_seek(s->priv, s->pos, SEEK_SET) < 0) {
        s->eof = 1;
        return 0;
    }
    return 1;
}

static int fill_buffer(stream_t *s, char *buffer, int max_len)
{
    int r = avio_read(s->priv, buffer, max_len);
	if(r <= 0)
	{
		if(g_wiimu_shm->imuzop_gapless == 1)
		{
			int len = 0;
			int size = 0;
			int flags = AVIO_FLAG_READ | AVIO_FLAG_DIRECT;
			AVIOContext *ctx = NULL;
			//char url[4096];
			char real_url[2048] = {0};

			SocketClientReadWriteMsg("/tmp/AMAZON_ASRTTS","getNextUrl",strlen("getNextUrl"),real_url,&len,0);//
			//hex2ascii(url + strlen("hex://"),real_url, strlen(url)-strlen("hex://"), 2048);			

			//fprintf(stderr, "ffmpeg read %d, return %d	(eof %d, pos %lld, start %lld, end %lld)\n", 
			//	max_len, r, s->eof, s->pos, s->start_pos, s->end_pos);
			if(strlen(real_url))
			{
				fprintf(stderr, "Gapless playback open the next url: %s\n", real_url);
				if(s->priv)
					avio_close(s->priv);
				s->priv = 0;
			    if (avio_open(&ctx, real_url, flags) < 0)
			    {
					fprintf(stderr, "failed to open the next url\n");
			    	goto out;
			    }

			    /*if(strncasecmp(real_url, "https://", 8) == 0)
			    {
					char *start = real_url + strlen("https://");
					char *end = strstr(start, "/");
					char host[128] = {0};
					int fd = -1;
					if(end)
					{
						strncpy(host, start, end-start);
						fd = connect2Server(host,443,1);
						if (fd > 0) closesocket(fd);
					}
			    }*/
			    
			    s->priv = ctx;
			    size = avio_size(ctx);
			    if (size >= 0)
			        s->end_pos += size;
				g_wiimu_shm->imuzop_end_pos = s->end_pos;
			    s->type = STREAMTYPE_FILE;
			    s->seek = seek;
			    if (!ctx->seekable) {
			        s->type = STREAMTYPE_STREAM;
			        s->seek = NULL;
			    }	
				//fprintf(stderr, "ffmpeg (eof %d, pos %lld, start %lld, end %lld)\n", 
				//	s->eof, s->pos, s->start_pos, s->end_pos);

				r = avio_read(s->priv, buffer, max_len);
				//fprintf(stderr, "ffmpeg read2 %d, return %d\n", max_len, r);
			}
			else
				g_wiimu_shm->imuzop_gapless = 2;
		}
	}
	if (r == AVERROR_EOF) {
		r = 0;
		s->eof = 1;
	}

out:
    return (r <= 0) ? -1 : r;
}

static int write_buffer(stream_t *s, char *buffer, int len)
{
    AVIOContext *ctx = s->priv;
    avio_write(s->priv, buffer, len);
    avio_flush(s->priv);
    if (ctx->error)
        return -1;
    return len;
}

static int control(stream_t *s, int cmd, void *arg)
{
    AVIOContext *ctx = s->priv;
    int64_t size, ts;
    double pts;
    switch(cmd) {
    case STREAM_CTRL_GET_SIZE:
        size = avio_size(s->priv);
        if(size >= 0) {
            *(off_t *)arg = size;
            return 1;
        }
        break;
    case STREAM_CTRL_SEEK_TO_TIME:
        pts = *(double *)arg;
        ts = pts * AV_TIME_BASE;
        if (!ctx->read_seek)
            break;
        ts = ctx->read_seek(s->priv, -1, ts, 0);
        if (ts >= 0)
            return 1;
        break;
    }
    return STREAM_UNSUPPORTED;
}

static void close_f(stream_t *stream)
{
	if(stream->priv)
   	 avio_close(stream->priv);
	stream->priv = 0;
}

static const char prefix[] = "ffmpeg://";

static int open_f(stream_t *stream, int mode, void *opts, int *file_format)
{
    int flags = 0;
    const char *filename;
    AVIOContext *ctx = NULL;
    int res = STREAM_ERROR;
    int64_t size;
    int dummy;

    init_avformat();
    avformat_network_init();
    if (mode == STREAM_READ)
        flags = AVIO_FLAG_READ;
    else if (mode == STREAM_WRITE)
        flags = AVIO_FLAG_WRITE;
    else {
        mp_msg(MSGT_OPEN, MSGL_ERR, "[ffmpeg] Unknown open mode %d\n", mode);
        res = STREAM_UNSUPPORTED;
        goto out;
    }

#ifdef AVIO_FLAG_DIRECT
    flags |= AVIO_FLAG_DIRECT;
#else
    mp_msg(MSGT_OPEN, MSGL_WARN, "[ffmpeg] No support for AVIO_FLAG_DIRECT, might cause performance and other issues.\n"
                                 "Please update to and rebuild against an FFmpeg version supporting it.\n");
#endif

    if (stream->url)
        filename = stream->url;
    else {
        mp_msg(MSGT_OPEN, MSGL_ERR, "[ffmpeg] No URL\n");
        goto out;
    }
    if (!strncmp(filename, prefix, strlen(prefix)))
        filename += strlen(prefix);
    dummy = !strncmp(filename, "rtsp:", 5);
    mp_msg(MSGT_OPEN, MSGL_V, "[ffmpeg] Opening %s\n", filename);

    if (!dummy && avio_open(&ctx, filename, flags) < 0)
        goto out;

    /*if(strncasecmp(filename, "https://", 8) == 0)
    {
		char *start = filename + strlen("https://");
		char *end = strstr(start, "/");
		char host[128] = {0};
		int fd = -1;
		if(end)
		{
			strncpy(host, start, end-start);
			fd = connect2Server(host,443,1);
			if (fd > 0) closesocket(fd);
		}
    }*/

	wiimu_log(1, 0, 0, 0, "[IMUZOP %lld] ffmpeg open url %s, flags=0x%x", tickSincePowerOn(), filename, flags);
    stream->priv = ctx;
    size = dummy ? 0 : avio_size(ctx);
    if (size >= 0)
        stream->end_pos = size;
	g_wiimu_shm->imuzop_end_pos = stream->end_pos;
    stream->type = STREAMTYPE_FILE;
    stream->seek = seek;
    if (dummy || !ctx->seekable) {
        stream->type = STREAMTYPE_STREAM;
        stream->seek = NULL;
    }
    if (!dummy) {
        stream->fill_buffer = fill_buffer;
        stream->write_buffer = write_buffer;
        stream->control = control;
        stream->close = close_f;
    }
    res = STREAM_OK;

out:
    return res;
}

const stream_info_t stream_info_ffmpeg = {
  "FFmpeg",
  "ffmpeg",
  "",
  "",
  open_f,
  { "ffmpeg", "rtmp", "rtsp", "https", "HTTPS", NULL },
  NULL,
  1 // Urls are an option string
};
