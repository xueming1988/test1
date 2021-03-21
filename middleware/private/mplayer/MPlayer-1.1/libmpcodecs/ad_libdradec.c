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
	"libDraDec decoder",
	"libDraDec",
	"",
	"",
	""
};

LIBAD_EXTERN(libdradec)

#define assert(x)

#include <DraExport.h>

#define BYTEORDER	1
#define DRAINITMODE 1

#if BYTEORDER
#define DRASYNCWORD 0xff7f
#else
#define DRASYNCWORD 0x7fff
#endif

typedef struct _DRACONTEXT
{
    void * pDraDecoder;
    dra_cfg DraCfg;
}DRACONTEXT;

int DraFindSync(unsigned char ** ppInOutBuf, int len)
{
	unsigned short temp;
    unsigned short *ptr;
    int i=0;
    ptr = (unsigned short *) *ppInOutBuf;
	while(len > i * sizeof(unsigned short))
	{
        temp = ptr[i];
		if(DRASYNCWORD == temp)
		{
            *ppInOutBuf += i * sizeof(unsigned short);
			return 1;
		}
        i ++;
	}
	return 0;
}

int LoadDraFrame(sh_audio_t *sh, unsigned char* pOutBuf, int* FrameLen)
{
	int i;
	long bit4frmlen = 0;
	int frmlen = 0;
	unsigned short temp;
    unsigned short *pReadData;
    unsigned char * pInput;
    int len;
    if((len=demux_read_data(sh->ds,&sh->a_in_buffer[sh->a_in_buffer_len],
          sh->a_in_buffer_size-sh->a_in_buffer_len))>0 || ((!len) && (sh->a_in_buffer_len>0)))
    {
        sh->a_in_buffer_len+=len;
        pInput = sh->a_in_buffer;
    	if(!DraFindSync(&pInput, sh->a_in_buffer_len))
    	{
            sh->a_in_buffer_len = 0;
    		return 0;
    	}
        pReadData = (unsigned short*) pInput;
        temp = pReadData[0];
    	if(DRASYNCWORD !=temp)
    	{
    		mp_msg(MSGT_CPLAYER, MSGL_ERR,"The DRA sync word error");
    		return 0;
    	}	
        
        temp = pReadData[1];
#if BYTEORDER
    	temp = 	(temp >> 8)|(temp << 8);
#endif
    	
    	bit4frmlen = ((temp&0x8000)==0) ? 10 : 13;
    	frmlen = temp<<1;
    	frmlen = (frmlen>>(16-bit4frmlen)) * 4;
    	if(frmlen>4096)
    		return 0;

        memcpy(pOutBuf, pInput, frmlen);
        len = pInput - (unsigned char *)sh->a_in_buffer + frmlen;
	    memmove(sh->a_in_buffer, sh->a_in_buffer+len, sh->a_in_buffer_size-len);
        sh->a_in_buffer_len -= len;
    	*FrameLen=frmlen;
    }
    else
    {
        return 0;
    }
	return 1;
}

static int preinit(sh_audio_t *sh)
{
  sh->audio_in_minsize=4096;
  sh->audio_out_minsize=192000;
  return 1;
}

static int init(sh_audio_t *sh_audio)
{
    DRACONTEXT *dra_context;
    unsigned char indata[4096];
    int FrameLen;
	dra_audio_info AudioInfo;
    int hr;

    dra_context = malloc(sizeof(DRACONTEXT));
    memset(dra_context, 0, sizeof(DRACONTEXT));
    sh_audio->context=dra_context;
	dra_context->DraCfg.initMode = DRAINITMODE;

	dra_context->DraCfg.byteOrder=BYTEORDER;

	if(!LoadDraFrame(sh_audio,indata,&FrameLen))
	{
		mp_msg(MSGT_CPLAYER, MSGL_ERR,"The dra file cannot find frame \n");
		return -1;
	}
	hr = DRA_InitDecode(&dra_context->pDraDecoder,&dra_context->DraCfg,indata,FrameLen,&AudioInfo);
    if(hr)
    {
        //error happened
        return -1;
    }
    //mp_msg(MSGT_CPLAYER, MSGL_ERR, "dradec init ainfo:\n SampleRate:%d\n nChannelNormal:%d\n nChannelLfe:%d\n JIC:%d\n SumDiff:%d\n\n" ,AudioInfo.SampleRate ,AudioInfo.nChannelNormal ,AudioInfo.nChannelLfe ,AudioInfo.JIC ,AudioInfo.SumDiff);
    sh_audio->wf->nChannels=sh_audio->channels=AudioInfo.nChannelNormal;
    sh_audio->wf->nSamplesPerSec=sh_audio->samplerate=AudioInfo.SampleRate;
    //sh_audio->i_bps=dra_context->bit_rate/8;

  return 1;
}

static void uninit(sh_audio_t *sh)
{
    DRACONTEXT *dra_context = sh->context;

    if(dra_context->pDraDecoder)
    {
        DRA_Release(&dra_context->pDraDecoder);
    }
    if(dra_context)
    {
        free(dra_context);
    }
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...)
{
    DRACONTEXT *dra_context = sh->context;
    unsigned char indata[4096];
    int FrameLen;
    switch(cmd){
    case ADCTRL_RESYNC_STREAM:
	    LoadDraFrame(sh,indata,&FrameLen);
        return CONTROL_TRUE;
    }
    return CONTROL_UNKNOWN;
}

static int decode_audio(sh_audio_t *sh_audio,unsigned char *buf,int minlen,int maxlen)
{
    int len = 0;
    int hr;
    int FrameLen;
    unsigned char indata[4096];
	int DownMixMode;
	dra_frame_info FrameInfo;
    DRACONTEXT *dra_context;
    dra_context = sh_audio->context;

    while(len<minlen && LoadDraFrame(sh_audio,indata,&FrameLen))
    {
    	int len2;
    	hr=DRA_DecodeFrame(dra_context->pDraDecoder,indata,FrameLen,buf,&len2,DownMixMode,16,&FrameInfo);
		if(hr)
		{
			mp_msg(MSGT_CPLAYER, MSGL_ERR,"DRA error %d\n",hr);
			if(hr>1 && hr !=5)
			{
                return -1;
			}
		}

        sh_audio->i_bps = FrameInfo.fInstantBitrate * (1000 / 8);

    	if(len2>0)
        {
            len+=len2;
            buf+=len2;
            maxlen -= len2;
            sh_audio->pts_bytes += len2;
    	}
    }
    return len;
}
