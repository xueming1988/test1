/*
 * RTSP 0.9.x-1.x audio output driver
 *
 * 
 *
 *
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

#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <alloca.h>

#include <stdio.h>
#include <unistd.h>

#include "config.h"
#include "subopt-helper.h"
#include "mixer.h"
#include "mp_msg.h"
#include "help_mp.h"
#include "audio_out.h"
#include "audio_out_internal.h"
#include "libaf/af_format.h"
#include <sys/ioctl.h>
#include <pthread.h>
#include "../libmpdemux/demux_mp3rtsp.h"

#include <sys/types.h>
#include <sys/mman.h>

#include <signal.h>
#include <sys/wait.h>
#include <sys/prctl.h>  


#define RTSP_AUDIO_BUFFER_SIZE (20480 +4)   //128k *2


typedef struct ao_rtsp_data {
	unsigned int size;
	unsigned int wptr;
	unsigned int rptr;
	pthread_mutex_t mutex;
} ao_rtsp_data;


static char * audio_buffer = NULL;
static unsigned int* buffer_size;
static unsigned int* write_ptr;
static unsigned int* read_ptr;
static pthread_mutex_t* ao_mutex;
static 	ao_rtsp_data * pbuffer = NULL;

static pid_t my_pid;
static size_t bytes_per_sample;


//#define DUMP_AUDIO 1

#ifdef DUMP_AUDIO
static int dump_fd = -1; 1
#endif

static const ao_info_t info =
{
    "RTSP server mp3 stream output",
    "mp3rtsp",
    "zl",
    "work as mp3 rtsp server"
};

LIBAO_EXTERN(mp3rtsp)


static void *rtsp_server_thread_func(void *arg)
{
	printf(" rtsp_server_thread_func buffer_size %x write_ptr %x  ,read_ptr %x  ,ao_mutex  %x  \n",buffer_size,write_ptr,read_ptr,ao_mutex);

	ao_run_mp3rstp(&audio_buffer,buffer_size,write_ptr,read_ptr,ao_mutex);

	printf("rtsp_server_thread_func is over  \n  ");

}


/* to set/get/query special features/parameters */
static int control(int cmd, void *arg)
{
 //end switch
  return CONTROL_UNKNOWN;
}



/*
    open & setup audio device
    return: 1=success 0=fail
*/
static int init(int rate_hz, int channels, int format, int flags)
{
	pthread_t rtsp_server_thread;
	static int is_rtsp_init = 0;
	int ret;  
	int size = 0;

#ifdef DUMP_AUDIO
	 dump_fd=open("/tmp/wav", O_RDWR | O_CREAT | O_TRUNC);
#endif
	
	printf(" ao rtsp  init   audio_buffer %x \n",audio_buffer);
#if 0
	if(NULL == audio_buffer)
		audio_buffer = malloc(RTSP_AUDIO_BUFFER_SIZE);

	printf(" ao rtsp 2 init   audio_buffer %x \n",audio_buffer);
	if(NULL == audio_buffer)
	{
		printf("ao_rtsp malloc failed  \n");
		return -1;
	}

#endif
	//buffer_size = RTSP_AUDIO_BUFFER_SIZE;
	//write_ptr = 0;
	//read_ptr = 0;
	
	ao_data.samplerate = 44100;
	ao_data.format = AF_FORMAT_S16_LE;  //AF_FORMAT_S16_LE
	ao_data.channels = 2;
	bytes_per_sample = 32;
	ao_data.bps = ao_data.samplerate * bytes_per_sample;
	ao_data.buffersize = 131584;
	ao_data.outburst = 4096 ;//32896;


	size = sizeof(ao_rtsp_data)+ 64 + RTSP_AUDIO_BUFFER_SIZE;
	printf("sizeof ao_rtsp_data %d size %d\n",sizeof(ao_rtsp_data),size);

	if(NULL == pbuffer)
		pbuffer=(ao_rtsp_data*)mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);  
	if( MAP_FAILED == pbuffer )  
	{  
		printf("ao_buffer mmap fail  \n");	
		return 0;
	}  

	printf("ao rtsp pbuffer %x \n",pbuffer);
	buffer_size = &(pbuffer->size);
	write_ptr = &(pbuffer->wptr);
	read_ptr = &(pbuffer->rptr);
	ao_mutex = &(pbuffer->mutex);
	audio_buffer = (void*)pbuffer + sizeof(ao_rtsp_data) + 32;
	
	printf(" audio_buffer %x  buffer_size %x write_ptr %x  ,read_ptr %x  ,ao_mutex  %x  \n",audio_buffer,buffer_size,write_ptr,read_ptr,ao_mutex);
		
	*buffer_size = RTSP_AUDIO_BUFFER_SIZE;
	*write_ptr = 0;
	*read_ptr = 0;
 
	pthread_mutexattr_t attr;  
	pthread_mutexattr_init(&attr);  

	ret=pthread_mutexattr_setpshared(&attr,PTHREAD_PROCESS_SHARED);	
	if( ret!=0 )  
	{  
		printf("init_mutex pthread_mutexattr_setpshared  %d\n",ret);  
	//	return 0;
	}  
	pthread_mutex_init(ao_mutex, &attr);

	
	if(( my_pid = fork() ) < 0 )
		{
			printf("ao_rtsp fork error  \n");
		}
		else if(my_pid == 0)
		{
			printf("ao_rtsp fork child  ,run rtsp\n");
            prctl(PR_SET_PDEATHSIG, 15); 
			rtsp_server_thread_func(NULL);
			printf("ao_rtsp fork child  , rtsp leave \n");
		}
		else
		{
			printf("ao_rtsp fork child  father ,son is %d\n",my_pid);
		}
		
#if 0
	if(0 == is_rtsp_init)
	{
		is_rtsp_init = 1;
		pthread_create(&rtsp_server_thread, NULL, rtsp_server_thread_func, NULL);
	}	
	else
	{
		printf("rtsp thread already init   \n");
	}
#endif
	
	return 1;
} // end init


/* close audio device */
static void uninit(int immed)
{
	unsigned int data_len;
	int drain_count = 0;
	if(my_pid > 0)
	{

DRAIN_DATA:
#if 1
		 pthread_mutex_lock(ao_mutex);

#ifdef DUMP_AUDIO
		close(dump_fd);
#endif

		if(drain_count < 30)
		{
			drain_count ++;
			data_len = (*write_ptr-*read_ptr+*buffer_size)%(*buffer_size);
			if(data_len > 100)
			{
				pthread_mutex_unlock(ao_mutex);
				usleep(200000);
					printf("ao rtsp uninit  drain data\n" ); 
				goto DRAIN_DATA;
			}
		}
		audio_buffer = NULL;

		pthread_mutex_unlock(ao_mutex);

		kill(my_pid,SIGTERM);
    	waitpid(my_pid,NULL,0);
		printf("ao rtsp uninit  audio_buffer %x\n",audio_buffer ); 
		pthread_mutex_destroy(ao_mutex);
		munmap((void *)pbuffer, sizeof(ao_rtsp_data)+ 64 + RTSP_AUDIO_BUFFER_SIZE);  
		pbuffer = NULL;
#endif
	}
}

static void audio_pause(void)
{
   
}

static void audio_resume(void)
{
    
     //   mp_ao_resume_refill(&audio_out_rtsp, prepause_space);
}

/* stop playing and empty buffers (for seeking/pause) */
static void reset(void)
{
  
    return;
}

/*
    plays 'len' bytes of 'data'
    returns: number of bytes played
    modified last at 29.06.02 by jp
    thanxs for marius <marius@rospot.com> for giving us the light ;)
*/

static int play(void* data, int len, int flags)
{ 
  int res = 0;
  unsigned int remain_size;
  unsigned int tmp = 0;
//	static unsigned int count =0;
	
CHK_SPACE:
  pthread_mutex_lock(ao_mutex);

  if(NULL == audio_buffer)
  {
	pthread_mutex_unlock(ao_mutex);
	printf("rtsp play audio_buffer is NULL \n ");
	return len ;
  }
  remain_size = (*read_ptr-(*write_ptr) -4 +*buffer_size)%(*buffer_size);

#if 1

  if(remain_size < len)
  {
	 pthread_mutex_unlock(ao_mutex);
	 printf("rtsp play no space  read_ptr %d write_ptr %d remain_size %d \n",*read_ptr,*write_ptr,remain_size);
	 usleep(10000);
	 goto CHK_SPACE;
  }
#endif

	if(*write_ptr > (*buffer_size))
	{
		printf("ao rtsp overwrite buffer %d  \n ",*write_ptr);
	}
	
	if(remain_size >= len)
	{
	  if((*write_ptr +len) <= (*buffer_size))
		{
	   	memcpy(audio_buffer+(*write_ptr),data,len);
		#ifdef DUMP_AUDIO
		write(dump_fd,audio_buffer+(*write_ptr),len);
		#endif
		*write_ptr += len;
		}
	  else
	  	{
	  	 tmp = *buffer_size - (*write_ptr);
		 memcpy(audio_buffer+(*write_ptr),data,tmp);
		#ifdef DUMP_AUDIO
		write(dump_fd,audio_buffer+(*write_ptr),tmp);
		#endif
		 
		 memcpy(audio_buffer,data + tmp,(len-tmp));
		#ifdef DUMP_AUDIO
		write(dump_fd,audio_buffer,(len-tmp));
		#endif
		 *write_ptr = (len-tmp);
	  	}
	//	count += len;
	//	printf("write count %d \n",count);

	} 
	else
		len = 0;
  pthread_mutex_unlock(ao_mutex);

 // printf("ao_rtsp write %d  read_ptr %d  write_ptr %d  remain_size %d tmp %d \n",len,read_ptr,write_ptr,remain_size,tmp);


   usleep(200000);
  return len ;
}

/* how many byes are free in the buffer */
static int get_space(void)
{
    int ret;
	pthread_mutex_lock(ao_mutex);
	ret = (*read_ptr-(*write_ptr) - 4 +(*buffer_size))%(*buffer_size);
	pthread_mutex_unlock(ao_mutex);

    return ret;
}

/* delay in seconds between first and last sample in buffer */
static float get_delay(void)
{

	return 0;
}

