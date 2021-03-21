#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "help_mp.h"

#include "ad_internal.h"
#include "dec_audio.h"

#include "mpbswap.h"

static ad_info_t info = 
{
	"libwma Rockbox-based WMA decoder",
	"libwma",
	"Matt Campbell",
	"www.rockbox.org",
	""
};

LIBAD_EXTERN(libwma)

#define assert(x)

#include "libwma/wmadec.h"

static int preinit(sh_audio_t *sh)
{
  sh->audio_out_minsize=192000;
  return 1;
}

static int init(sh_audio_t *sh_audio)
{
    int x;
    WMADecodeContext *wma_context;

    mp_msg(MSGT_DECAUDIO,MSGL_V,"libwma Rockbox-based WMA decoder\n");
    wma_context = malloc(sizeof(WMADecodeContext));
    memset(wma_context, 0, sizeof(WMADecodeContext));
    sh_audio->context=wma_context;

    wma_context->sample_rate = sh_audio->samplerate;
    wma_context->bit_rate = sh_audio->i_bps * 8;
    if(sh_audio->wf){
	wma_context->nb_channels = sh_audio->wf->nChannels;
	wma_context->sample_rate = sh_audio->wf->nSamplesPerSec;
	wma_context->bit_rate = sh_audio->wf->nAvgBytesPerSec * 8;
	wma_context->block_align = sh_audio->wf->nBlockAlign;
    }
    else
	wma_context->nb_channels = audio_output_channels;
    wma_context->codec_id = sh_audio->format;

    /* alloc extra data */
    if (sh_audio->wf && sh_audio->wf->cbSize > 0) {
        wma_context->extradata = malloc(sh_audio->wf->cbSize);
        wma_context->extradata_size = sh_audio->wf->cbSize;
        memcpy(wma_context->extradata, (char *)sh_audio->wf + sizeof(WAVEFORMATEX), 
               wma_context->extradata_size);
    }

    /* open it */
    if (libwma_decode_init(wma_context) < 0) {
        mp_msg(MSGT_DECAUDIO,MSGL_ERR, MSGTR_CantOpenCodec);
        return 0;
    }
   mp_msg(MSGT_DECAUDIO,MSGL_V,"INFO: libwma init OK!\n");
   
   // Decode at least 1 byte:  (to get header filled)
   x=decode_audio(sh_audio,sh_audio->a_buffer,1,sh_audio->a_buffer_size);
   if(x>0) sh_audio->a_buffer_len=x;

  sh_audio->channels=wma_context->nb_channels;
  sh_audio->samplerate=wma_context->sample_rate;
  sh_audio->i_bps=wma_context->bit_rate/8;
  if(sh_audio->wf){
      // If the decoder uses the wrong number of channels all is lost anyway.
      // sh_audio->channels=sh_audio->wf->nChannels;
      if (sh_audio->wf->nSamplesPerSec)
      sh_audio->samplerate=sh_audio->wf->nSamplesPerSec;
      if (sh_audio->wf->nAvgBytesPerSec)
      sh_audio->i_bps=sh_audio->wf->nAvgBytesPerSec;
  }
  sh_audio->samplesize=2;
  return 1;
}

static void uninit(sh_audio_t *sh)
{
    WMADecodeContext *wma_context = sh->context;

    free(wma_context->extradata);
    free(wma_context);
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
    WMADecodeContext *wma_context = sh->context;
    switch(cmd){
    case ADCTRL_RESYNC_STREAM:
        libwma_decode_superframe(wma_context, NULL, NULL, NULL, 0);
    return CONTROL_TRUE;
    }
    return CONTROL_UNKNOWN;
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
    unsigned char *start=NULL;
    int y,len=-1;
    while(len<minlen){
	int len2=maxlen;
	double pts;
	int x=ds_get_packet_pts(sh_audio->ds,&start, &pts);
	if(x<=0) break; // error
	if (pts != MP_NOPTS_VALUE) {
	    sh_audio->pts = pts;
	    sh_audio->pts_bytes = 0;
	}
	y=libwma_decode_superframe(sh_audio->context,(int16_t*)buf,&len2,start,x);
	if(y<0){ mp_msg(MSGT_DECAUDIO,MSGL_V,"lavc_audio: error\n");break; }
	if(y<x) sh_audio->ds->buffer_pos+=y-x;  // put back data (HACK!)
	if(len2>0){
	  //len=len2;break;
	  if(len<0) len=len2; else len+=len2;
	  buf+=len2;
	  maxlen -= len2;
	  sh_audio->pts_bytes += len2;
	}
        mp_dbg(MSGT_DECAUDIO,MSGL_DBG2,"Decoded %d -> %d  \n",y,len2);
    }
  return len;
}
