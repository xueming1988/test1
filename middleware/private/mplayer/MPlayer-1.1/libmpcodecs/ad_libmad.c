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
    "libmad mpeg audio decoder",
    "libmad",
    "A'rpi",
    "libmad...",
    "based on Xine's libmad/xine_decoder.c"
};

LIBAD_EXTERN(libmad)

#include <mad.h>


typedef struct mad_decoder_s {

  struct mad_synth  synth;
  struct mad_stream stream;
  struct mad_frame  frame;

  int have_frame;

  int               output_sampling_rate;
  int               output_open;
  int               output_mode;

} mad_decoder_t;

static int mp3_temp_len = 0;
static char* mp3_temp_buf = 0;
static int mp3_temp_size = 16384;//65536*2;

static int preinit(sh_audio_t *sh){

  mad_decoder_t *this = calloc(1, sizeof(mad_decoder_t));
  sh->context = this;

  mad_synth_init  (&this->synth);
  mad_stream_init (&this->stream);
  mad_frame_init  (&this->frame);

  sh->audio_out_minsize=2*4608;
  sh->audio_in_minsize=4096;
  mp3_temp_len = 0;

  return 1;
}

//----------------------- mp3 audio frame header parser -----------------------
static const unsigned short tabsel_123[2][3][16] = {
    {   {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0},
        {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
        {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0}
    },

    {   {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
        {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
        {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}
    }
};

static const int freqs[9] = { 44100, 48000, 32000,   // MPEG 1.0
                              22050, 24000, 16000,   // MPEG 2.0
                              11025, 12000,  8000
                            };  // MPEG 2.5

/*
 * return frame size or -1 (bad frame)
 */
static int mp_get_mp3_header(unsigned char *hbuf, int *chans, int *srate, int *spf, int *mpa_layer, int *br)
{
    int stereo, lsf, framesize, padding, bitrate_index, sampling_frequency, divisor;
    int bitrate;
    int layer;
    static const int mult[3] = { 12000, 144000, 144000 };
    unsigned int newhead =
        hbuf[0] << 24 |
        hbuf[1] << 16 |
        hbuf[2] <<  8 |
        hbuf[3];

    if((newhead & 0xffe00000) != 0xffe00000 || (newhead & 0x00000c00) == 0x00000c00) {
        return -1;
    }

    layer = 4 - ((newhead >> 17) & 3);
    if(layer == 4) {
        fprintf(stderr, "mp_decode_mp3_header wrong layer\n");
        return -1;
    }

    sampling_frequency = (newhead >> 10) & 0x3; // valid: 0..2
    if(sampling_frequency == 3) {
        fprintf(stderr, "mp_decode_mp3_header wrong sampling_frequency\n");
        return -1;
    }

    if(newhead & (1 << 20)) {
        // MPEG 1.0 (lsf==0) or MPEG 2.0 (lsf==1)
        lsf = !(newhead & (1 << 19));
        sampling_frequency += lsf * 3;
    } else {
        // MPEG 2.5
        lsf = 1;
        sampling_frequency += 6;
    }

    bitrate_index = (newhead >> 12) & 0xf; // valid: 1..14
    padding   = (newhead >> 9) & 0x1;

    stereo    = (((newhead >> 6) & 0x3) == 3) ? 1 : 2;

    bitrate = tabsel_123[lsf][layer - 1][bitrate_index];
    framesize = bitrate * mult[layer - 1];

    if(!framesize) {
        fprintf(stderr, "mp_decode_mp3_header wrong framesize\n");
        return -1;
    }

    divisor = layer == 3 ? (freqs[sampling_frequency] << lsf) : freqs[sampling_frequency];
    framesize /= divisor;
    framesize += padding;
    if(layer == 1)
        framesize *= 4;

    if(srate)
        *srate = freqs[sampling_frequency];
    if(spf) {
        if(layer == 1)
            *spf = 384;
        else if(layer == 2)
            *spf = 1152;
        else if(sampling_frequency > 2) // not 1.0
            *spf = 576;
        else
            *spf = 1152;
    }
    if(mpa_layer) *mpa_layer = layer;
    if(chans) *chans = stereo;
    if(br) *br = bitrate;

    return framesize;
}
#define mp_decode_mp3_header(hbuf)  mp_get_mp3_header(hbuf,NULL,NULL,NULL,NULL,NULL)
#define ID3V2_TAG_HEAD_LEN 10

static int CheckID3v2(char* buf) {
    int id3_len = 0;
    if(buf[0]=='I' && buf[1] == 'D' && buf[2] == '3') { //find ID3V2
        unsigned int flag = buf[5];
        id3_len = ((buf[6] & 0x7f)<<21)+((buf[7] & 0x7f)<<14)+((buf[8] & 0x7f)<<7)+(buf[9] & 0x7f);
        id3_len += 10; //include header

        if(flag & 0x10) {
            id3_len += 10; //include footer
        }
        if(flag & 0x40) {
            id3_len += 10; //include extender header
        }
    }
    return id3_len;
}

static int IsID3V2(char* buf) {
    if(buf[0]=='I' && buf[1] == 'D' && buf[2] == '3')
    {
        if(buf[3] > 0 && buf[3] < 5)
            return 1;
        else
            fprintf(stderr, "ID3V2, version is not 2,3,4, ignore it\n");
    }
    return 0;
}

static int read_frame(sh_audio_t *sh){
    mad_decoder_t *this = sh->context;
    int len;
    if(mp3_temp_buf == 0)
        mp3_temp_buf = malloc(mp3_temp_size);

    while((len=demux_read_data(sh->ds, &mp3_temp_buf[mp3_temp_len],
        mp3_temp_size - mp3_temp_len))>0 || mp3_temp_len > MAD_BUFFER_GUARD ) {  
        
        {
            if(len < 0) len = 0;
            //FILE* fold=fopen("/tmp/old.mp3", "ab+");
            //if(fold) {
            //  fwrite(&mp3_temp_buf[mp3_temp_len], 1, len, fold);
            //  fclose(fold);
            //}
        }
        mp3_temp_len += len;
        if(mp3_temp_len > MAD_BUFFER_GUARD) {
            int num_bytes = 0;
            unsigned char hdr[4];
            int sync_pos = 0;
            int frame_size = 0;
            int is_id3 = 0;
            while(sync_pos + 4 <= mp3_temp_len) {
                is_id3 = IsID3V2(&mp3_temp_buf[sync_pos]);
                if(is_id3)
                    break;
                memcpy(hdr, &mp3_temp_buf[sync_pos], 4);
                frame_size = mp_decode_mp3_header(hdr);
                if(frame_size > 0)
                  break;
                sync_pos++;
            }

            if(sync_pos) {
                num_bytes = mp3_temp_len - sync_pos;
                memmove(mp3_temp_buf, mp3_temp_buf + sync_pos, num_bytes);
                mp3_temp_len = num_bytes;
            }
            //fprintf(stderr, "libmad decode read_frame %d, all_len %d, valid mp3 len=%d\n", len, mp3_temp_len, sh->a_in_buffer_len);

            if(is_id3 && mp3_temp_len >= ID3V2_TAG_HEAD_LEN) {
                int id3_len = CheckID3v2(mp3_temp_buf);
                //fprintf(stderr, "ID3V2 find, len %d bytes\n", id3_len);
                if(id3_len > mp3_temp_size) {
                    mp3_temp_size = id3_len;
                    mp3_temp_buf = realloc(mp3_temp_buf, mp3_temp_size);
                    //fprintf(stderr, "ID3V2 find, len %d bytes, realloc mp3_temp_buf \n", id3_len);
                }
                if(mp3_temp_len >= id3_len ) {
                    num_bytes = mp3_temp_len - id3_len;
                    memmove(mp3_temp_buf, mp3_temp_buf + id3_len, num_bytes);
                    mp3_temp_len = num_bytes;
                    //fprintf(stderr, "ID3V2, skip %d bytes\n", id3_len);
                }
            }

            if(frame_size > 0 && mp3_temp_len >= frame_size) {
                //fprintf(stderr, "ID3V2 find, framesize %d bytes\n", frame_size);
                memcpy(&sh->a_in_buffer[sh->a_in_buffer_len], mp3_temp_buf, frame_size);
                {
                    //FILE* fnew=fopen("/tmp/new.mp3", "ab+");
                    //if(fnew) {
                    //  fwrite(&sh->a_in_buffer[sh->a_in_buffer_len], 1, frame_size, fnew);
                    //  fclose(fnew);
                    //}
                }           
                sh->a_in_buffer_len += frame_size;
                
                num_bytes = mp3_temp_len - frame_size;
                memmove(mp3_temp_buf, mp3_temp_buf + frame_size, num_bytes);
                mp3_temp_len = num_bytes;

                if(sh->a_in_buffer_len > frame_size)
                    break;
            }
        }
    }

  while(sh->a_in_buffer_len > MAD_BUFFER_GUARD){
    int ret;
    int num_bytes = 0;

    mad_stream_buffer (&this->stream, sh->a_in_buffer, sh->a_in_buffer_len);
    ret = mad_stream_sync(&this->stream);
    if(ret != 0)
    {   
        num_bytes = MAD_BUFFER_GUARD;
        memmove(sh->a_in_buffer, sh->a_in_buffer + sh->a_in_buffer_len - MAD_BUFFER_GUARD, num_bytes);
        sh->a_in_buffer_len = num_bytes;
        fprintf(stderr, "libmad resync fail, why? (we have skip the tag data)\n");
        break;
    }

    num_bytes = (char*)this->stream.ptr.byte - (char*)sh->a_in_buffer;
    if(num_bytes)
    {
        memmove(sh->a_in_buffer, this->stream.ptr.byte, sh->a_in_buffer_len - num_bytes);
        sh->a_in_buffer_len -= num_bytes;
        fprintf(stderr, "libmad resync %d data, why? (we have skip the tag data)\n", num_bytes);
    }
    mad_stream_buffer (&this->stream, sh->a_in_buffer, sh->a_in_buffer_len);
    ret=mad_frame_decode (&this->frame, &this->stream);
    if (this->stream.next_frame) {
        num_bytes = (char*)sh->a_in_buffer+sh->a_in_buffer_len - (char*)this->stream.next_frame;
        memmove(sh->a_in_buffer, this->stream.next_frame, num_bytes);
        mp_msg(MSGT_DECAUDIO,MSGL_DBG2,"libmad: %d bytes processed\n",sh->a_in_buffer_len-num_bytes);
        sh->a_in_buffer_len = num_bytes;
    }
    if (ret == 0 || this->stream.error == MAD_ERROR_BADPART3LEN) return 1; // OK!!!
    // error! try to resync!
    if(this->stream.error==MAD_ERROR_BUFLEN) break;
  }
    //huaijing fixed end buffer not decoded
    if(len == 0)
    {
          while(1){
            int ret;
            mad_stream_buffer (&this->stream, sh->a_in_buffer, sh->a_in_buffer_len);
            ret=mad_frame_decode (&this->frame, &this->stream);
            if (this->stream.next_frame) {
                int num_bytes =
                    (char*)sh->a_in_buffer+sh->a_in_buffer_len - (char*)this->stream.next_frame;
                memmove(sh->a_in_buffer, this->stream.next_frame, num_bytes);
                mp_msg(MSGT_DECAUDIO,MSGL_DBG2,"libmad: %d bytes processed\n",sh->a_in_buffer_len-num_bytes);
                sh->a_in_buffer_len = num_bytes;
            }
            if (ret == 0) return 1; // OK!!!
            
            // error! try to resync!
            if(this->stream.error==MAD_ERROR_BUFLEN) {
                //fprintf(stderr, "libmad decode fail and read return 0, EOS?\n");
                break;
            }
        }
    }
//mp_msg(MSGT_DECAUDIO,MSGL_INFO,"Cannot sync MAD frame\n");
return 0;
}

static int init(sh_audio_t *sh){
  mad_decoder_t *this = sh->context;
  int count = 50;
  while(count-- > 0) {
    this->have_frame=read_frame(sh);
    if(this->have_frame) break;
  }
  fprintf(stderr, "libmad decode init, have frame %d\n", this->have_frame);
  if(!this->have_frame) return 0; // failed to sync...


  sh->channels=(this->frame.header.mode == MAD_MODE_SINGLE_CHANNEL) ? 1 : 2;
  sh->samplerate=this->frame.header.samplerate;
  if (sh->i_bps < 1)
    sh->i_bps=this->frame.header.bitrate/8;
  sh->samplesize=2;

  return 1;
}

static void uninit(sh_audio_t *sh){
  mad_decoder_t *this = sh->context;
  mad_synth_finish (&this->synth);
  mad_frame_finish (&this->frame);
  mad_stream_finish(&this->stream);
  free(sh->context);
}

/* utility to scale and round samples to 16 bits */
static inline signed int scale(mad_fixed_t sample) {
  /* round */
  sample += (1L << (MAD_F_FRACBITS - 16));

  /* clip */
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  /* quantize */
  return sample >> (MAD_F_FRACBITS + 1 - 16);
}

static int decode_audio(sh_audio_t *sh,unsigned char *buf,int minlen,int maxlen){
  mad_decoder_t *this = sh->context;
  int len=0;

  while(len<minlen && len+4608<=maxlen){
    if(!this->have_frame) this->have_frame=read_frame(sh);
    if(!this->have_frame) break; // failed to sync... or EOF
    this->have_frame=0;

    mad_synth_frame (&this->synth, &this->frame);

    { unsigned int         nchannels, nsamples;
      mad_fixed_t const   *left_ch, *right_ch;
      struct mad_pcm      *pcm = &this->synth.pcm;
      uint16_t            *output = (uint16_t*) buf;

      nchannels = pcm->channels;
      nsamples  = pcm->length;
      left_ch   = pcm->samples[0];
      right_ch  = pcm->samples[1];

      len+=2*nchannels*nsamples;
      buf+=2*nchannels*nsamples;

      while (nsamples--) {
        /* output sample(s) in 16-bit signed little-endian PCM */

        *output++ = scale(*left_ch++);

        if (nchannels == 2)
          *output++ = scale(*right_ch++);

      }
    }
  }

  //fprintf(stderr, "libmad decode audio return %d\n", len);
  return len?len:-1;
}

static int control(sh_audio_t *sh,int cmd,void* arg, ...){
  mad_decoder_t *this = sh->context;
    // various optional functions you MAY implement:
    switch(cmd){
      case ADCTRL_RESYNC_STREAM:
    this->have_frame=0;
    mad_synth_init  (&this->synth);
    mad_stream_init (&this->stream);
    mad_frame_init  (&this->frame);
    mp3_temp_len = 0;
    return CONTROL_TRUE;
      case ADCTRL_SKIP_FRAME:
        this->have_frame=read_frame(sh);
    return CONTROL_TRUE;
    }
  return CONTROL_UNKNOWN;
}
