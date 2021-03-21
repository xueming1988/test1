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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "mp_msg.h"
#include "ad_internal.h"

static const ad_info_t info = {
	"libbypass mpeg audio decoder",
	"libbypass",
	"ZL",
	"libbypass...",
	"no decoder "
};

LIBAD_EXTERN(libbypass)



static int preinit(sh_audio_t *sh){

  sh->audio_out_minsize=4096;
  sh->audio_in_minsize=256;
	printf("libbypass preinit \n");
  return 1;
}

static int read_frame(sh_audio_t *sh){
  int len;
 // printf("libbypass read_frame \n");

while((len=demux_read_data(sh->ds,&sh->a_in_buffer[sh->a_in_buffer_len],
          sh->a_in_buffer_size-sh->a_in_buffer_len))>0){
  sh->a_in_buffer_len+=len;
}

return 0;
}

static int init(sh_audio_t *sh){
 

  read_frame(sh);
 
   printf("libbypass init \n ");
  sh->channels= 2;
  sh->samplerate=44100;
  if (sh->i_bps < 1)
    sh->i_bps=44100 * 32;
  sh->samplesize=2;

  return 1;
}

static void uninit(sh_audio_t *sh){
	printf("libbypass uninit   \n ");

}



static int decode_audio(sh_audio_t *sh,unsigned char *buf,int minlen,int maxlen){
  int len=0;

  read_frame(sh);

  if(maxlen < sh->a_in_buffer_len)
  {
	memcpy(buf,sh->a_in_buffer,maxlen);

	memcpy(sh->a_in_buffer,sh->a_in_buffer+maxlen,(sh->a_in_buffer_len - maxlen));
	sh->a_in_buffer_len = sh->a_in_buffer_len - maxlen;
	len = maxlen;
//	  printf("libbypass decode_audio %d \n",maxlen);
  }
  else
  {
	memcpy(buf,sh->a_in_buffer,sh->a_in_buffer_len);
	//  printf("libbypass decode_audio 2  %d \n",sh->a_in_buffer_len);
	len = sh->a_in_buffer_len;
	sh->a_in_buffer_len = 0;
	
  }


  return len;
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...){
    // various optional functions you MAY implement:
    switch(cmd){
      case ADCTRL_RESYNC_STREAM:
	
	return CONTROL_TRUE;
      case ADCTRL_SKIP_FRAME:
        read_frame(sh);
	return CONTROL_TRUE;
    }
  return CONTROL_UNKNOWN;
}
