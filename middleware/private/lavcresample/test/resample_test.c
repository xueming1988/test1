#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <alsa/asoundlib.h>

#include "af_lavcresample.h"

static snd_pcm_t* handle;
#define SAMPLE_RATE  44100
FILE *_file = NULL;

static int file_open(const void *arg)
{

    const char *path = (const char *)arg;
	int _total_bytes  = 0;

    _file = fopen(path, "rb");
    if ( _file == NULL)
    {
		//printf("open '%s' failed, err=%d\n", path, errno);
		return -1;
    }
	fseek(_file, 0, SEEK_END);
	_total_bytes = ftell(_file);
	fseek(_file, 0, SEEK_SET);


	return _total_bytes;
}

static void file_close(void)
{
    if (_file != NULL)
    {
		fclose(_file);
		_file = NULL;
    }
}

int read_frame(unsigned char *buff, int size)
{ 
	int ret = fread(buff, 1, size, _file);

	if(ret <= 0)
		ret = -1;
	return ret;
}

int main(int argc, char *argv[])
{
	int err;
	char buffer[8192];
	af_instance_t af = {0};
	char *play_out;
	int i;
	if(argc < 2)
			return -1;
	printf("%s %d \r\n",__FUNCTION__,__LINE__);
	if(file_open(argv[1]) > 0)
	 {
	 		printf("%s %d \r\n",__FUNCTION__,__LINE__);
			if ((err = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
					printf("Playback open error: %s/n", snd_strerror(err));
			}
			printf("%s %d \r\n",__FUNCTION__,__LINE__);

			if ((err = snd_pcm_set_params(handle,
								SND_PCM_FORMAT_S16_LE,
								SND_PCM_ACCESS_RW_INTERLEAVED,
								2, 
								SAMPLE_RATE, //sample rate in Hz
								0, //soft_resample
								200000)) < 0) {   /* latency: us 0.2sec */
					printf("Playback open error: %s/n", snd_strerror(err));
					exit(EXIT_FAILURE);
				}
			af_open(&af);
			printf("%s %d \r\n",__FUNCTION__,__LINE__);

			char *arg="44100:16:0:5:0.0";
			af.control(&af,AF_CONTROL_COMMAND_LINE,arg);

			af_data_t data;
			data.audio = buffer;
			data.rate = 8000;
			data.nch = 1;
			data.bps = 2;
			data.format = AF_FORMAT_S16_LE;
			af.control(&af,AF_CONTROL_REINIT,&data);
			printf("%s %d \r\n",__FUNCTION__,__LINE__);
		 for(;;)
		 {
		 	 data.len = read_frame(data.audio,8192);
			// printf("data.len %d \r\n",data.len);
			 if(data.len > 0)
			 {
			 	af_data_t *outdata;
			 	 outdata=af.play(&af, &data);
				 
				// 单声道转多声道输出
				// printf("data.len %d \r\n",data.len);
				 play_out = malloc(outdata->len*2);
				 {
					 unsigned short *out_steroe = (unsigned short *)play_out;
					 unsigned short *in_mono = (unsigned short *)outdata->audio;
					 for(i = 0 ; i< outdata->len/2;i++)
					 {
						 *out_steroe++ = *in_mono;
						 *out_steroe++ = *in_mono;
						 in_mono++;
					 }
				 }
				
				 int alsa_frames = snd_pcm_writei(handle, play_out, outdata->len/2);
			     if (alsa_frames < 0)
			     {
			          snd_pcm_recover(handle, alsa_frames, 0);
			     }
				 free(play_out);
			 }else
			 {
				break;
			 }
		 }

	 }
	file_close();
	snd_pcm_drain(handle);
	snd_pcm_close(handle);
	af.uninit(&af);

	return 0;
}
