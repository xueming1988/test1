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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "m_option.h"

#include "stream/stream.h"
#include "demuxer.h"
#include "stheader.h"


static int channels = 2;
static int samplerate = 44100;
static int samplesize = 2;
static int bitrate = 0;
static int format = 0x6172642E; // '.dra'

const m_option_t demux_dra_opts[] = {
  { "channels", &channels, CONF_TYPE_INT,CONF_RANGE,1,8, NULL },
  { "rate", &samplerate, CONF_TYPE_INT,CONF_RANGE,1000,8*48000, NULL },
  { "samplesize", &samplesize, CONF_TYPE_INT,CONF_RANGE,1,8, NULL },
  { "bitrate", &bitrate, CONF_TYPE_INT,CONF_MIN,0,0, NULL },
  { "format", &format, CONF_TYPE_INT, CONF_MIN, 0 , 0, NULL },
  {NULL, NULL, 0, 0, 0, 0, NULL}
};

#define DRA_FOURCC(a,b,c,d) ((a<<24)|(b<<16)|(c<<8)|(d))

static int demux_dra_check_file(demuxer_t* demuxer){
    int flags=0;
    int no=0;

    while(1)
    {
    	int i;
    	int skipped=8;
    	off_t len=stream_read_dword(demuxer->stream);
    	unsigned int id=stream_read_dword(demuxer->stream);
    	if(stream_eof(demuxer->stream)) break; // EOF
    	if (len == 1) /* real size is 64bits - cjb */
    	{
    	    len = stream_read_qword(demuxer->stream);
    	    skipped += 8;
    	}
    	if(len<8) break; // invalid chunk

    	switch(id){
    	case DRA_FOURCC('f','t','y','p'): {
    	  unsigned int tmp;
    	  // File Type Box (ftyp):
    	  // char[4]  major_brand	   (eg. 'isom')
    	  // int      minor_version	   (eg. 0x00000000)
    	  // char[4]  compatible_brands[]  (eg. 'mp41')
    	  // compatible_brands list spans to the end of box
    	  tmp = stream_read_dword(demuxer->stream);
    	  switch(tmp) {
    	    case DRA_FOURCC('d','r','a','1'):
    	      mp_msg(MSGT_DEMUX,MSGL_INFO,"DRA: File Type Major Brand: dra\n");
    	        return DEMUXER_TYPE_DRA;
    	    default:
    	      tmp = be2me_32(tmp);
    	      mp_msg(MSGT_DEMUX,MSGL_WARN,"ISO: Unknown File Type Major Brand: %.4s\n",(char *)&tmp);
            return 0;
    	  }
    	  mp_msg(MSGT_DEMUX,MSGL_V,"ISO: File Type Minor Version: %d\n",
    	      stream_read_dword(demuxer->stream));
    	  skipped += 8;
    	  // List all compatible brands
    	  for(i = 0; i < ((len-16)/4); i++) {
    	    tmp = be2me_32(stream_read_dword(demuxer->stream));
    	    mp_msg(MSGT_DEMUX,MSGL_V,"ISO: File Type Compatible Brand #%d: %.4s\n",i,(char *)&tmp);
    	    skipped += 4;
    	  }
    	  } break;
        }
    }

    return 0;
}

static demuxer_t* demux_dra_open(demuxer_t* demuxer) {
  sh_audio_t* sh_audio;
  WAVEFORMATEX* w;

  sh_audio = new_sh_audio(demuxer,0, NULL);
  sh_audio->wf = w = malloc(sizeof(*w));
  w->wFormatTag = sh_audio->format = format;
  w->nChannels = sh_audio->channels = channels;
  w->nSamplesPerSec = sh_audio->samplerate = samplerate;
  if (bitrate > 999)
    w->nAvgBytesPerSec = bitrate/8;
  else if (bitrate > 0)
    w->nAvgBytesPerSec = bitrate*125;
  else
    w->nAvgBytesPerSec = samplerate*samplesize*channels;
  w->nBlockAlign = channels*samplesize;
  sh_audio->samplesize = samplesize;
  w->wBitsPerSample = 8*samplesize;
  w->cbSize = 0;

  demuxer->movi_start = demuxer->stream->start_pos;
  demuxer->movi_end = demuxer->stream->end_pos;

  demuxer->audio->id = 0;
  demuxer->audio->sh = sh_audio;
  sh_audio->ds = demuxer->audio;
  sh_audio->needs_parsing = 1;

  return demuxer;
}

static int demux_dra_fill_buffer(demuxer_t* demuxer, demux_stream_t *ds) {
  sh_audio_t* sh_audio = demuxer->audio->sh;
  int l = 4096;
  off_t spos = stream_tell(demuxer->stream);
  demux_packet_t*  dp;

  if(demuxer->stream->eof)
    return 0;

  dp = new_demux_packet(l);
  dp->pos = (spos - demuxer->movi_start);
  //dp->pts = (spos - demuxer->movi_start)  / (float)(sh_audio->wf->nAvgBytesPerSec);
  if(sh_audio->i_bps > 0)
  {
    dp->pts = dp->pos / (double)sh_audio->i_bps;
  }
  else
  {
    dp->pts = MP_NOPTS_VALUE;
  }

  l = stream_read(demuxer->stream,dp->buffer,l);
  resize_demux_packet(dp, l);
  ds_add_packet(ds,dp);

  return 1;
}

static void demux_dra_seek(demuxer_t *demuxer,float rel_seek_secs,float audio_delay,int flags){
  stream_t* s = demuxer->stream;
  sh_audio_t* sh_audio = demuxer->audio->sh;
  off_t base,pos;

  base = (flags & SEEK_ABSOLUTE) ? demuxer->movi_start : stream_tell(s);
  if(flags & SEEK_FACTOR)
    pos = base + ((demuxer->movi_end - demuxer->movi_start)*rel_seek_secs);
  else
    pos = base + (rel_seek_secs*sh_audio->i_bps);

  pos -= (pos % (sh_audio->channels * sh_audio->samplesize) );
  stream_seek(s,pos);
}

const demuxer_desc_t demuxer_desc_dra = {
  "DRA audio demuxer",
  "draaudio",
  "draaudio",
  "?",
  "",
  DEMUXER_TYPE_DRA,
  0, // no autodetect
  demux_dra_check_file,
  demux_dra_fill_buffer,
  demux_dra_open,
  NULL,
  demux_dra_seek,
};
