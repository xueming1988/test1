/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**********/
// "liveMedia"
// Copyright (c) 1996-2006 Live Networks, Inc.  All rights reserved.
// A WAV audio file source
// Implementation

#include "MPLAYFileSource.hh"
#include "GroupsockHelper.hh"
#include <pthread.h>
#include <stdlib.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

////////// MplayerFileSource //////////

//#define DUMP_AUDIO 1

#ifdef DUMP_AUDIO
static int dump_fd = -1;  1
#endif


MplayerFileSource*
MplayerFileSource::createNew(UsageEnvironment& env, char ** pbuffer,
										  unsigned int * psize,
										  unsigned int * pwprt,
										  unsigned int * prprt,
										  pthread_mutex_t* pmutex,
										  int fd) {
								  
  do {

    MplayerFileSource* newSource = new MplayerFileSource(env,pbuffer,psize,pwprt,prprt,pmutex,fd);

	printf("MplayerFileSource createNew pbuffer %x  newSource %x\n",*pbuffer,newSource);
	if (newSource == NULL) {
      // The WAV file header was apparently invalid.
      break;
    }

    newSource->fFileSize = 0;
	printf(" newSource->fFileSize = 0; \n");
    return newSource;
  } while (0);

  return NULL;
}

unsigned MplayerFileSource::numPCMBytes() const {
  if (fFileSize < fWAVHeaderSize) return 0;
  return fFileSize - fWAVHeaderSize;
}

void MplayerFileSource::setScaleFactor(int scale) {
  fScaleFactor = scale;

  if (fScaleFactor < 0 ) {
    // Because we're reading backwards, seek back one sample, to ensure that
    // (i)  we start reading the last sample before the start point, and
    // (ii) we don't hit end-of-file on the first read.
    int const bytesPerSample = (fNumChannels*fBitsPerSample)/8;
  }
}

void MplayerFileSource::seekToPCMByte(unsigned byteNumber) {

}

unsigned char MplayerFileSource::getAudioFormat() {
  return fAudioFormat;
}



MplayerFileSource::MplayerFileSource(UsageEnvironment& env, char ** pbuffer,
										  unsigned int * psize,
										  unsigned int * pwprt,
										  unsigned int * prprt,
										  pthread_mutex_t* pmutex,
										  int fd)
  : AudioInputDevice(env, 0, 0, 0, 0)/* set the real parameters later */,
    fLastPlayTime(0), fWAVHeaderSize(0), fFileSize(0), fScaleFactor(1),
    fAudioFormat(0){
  // Check the WAV file header for validity.
  // Note: The following web pages contain info about the WAV format:
  // http://www.technology.niagarac.on.ca/courses/comp630/WavFileFormat.html
  // http://ccrma-www.stanford.edu/CCRMA/Courses/422/projects/WaveFormat/
  // http://www.ringthis.com/dev/wave_format.htm
  // http://www.lightlink.com/tjweber/StripWav/Canon.html
  // http://www.borg.com/~jglatt/tech/wave.htm
  // http://www.wotsit.org/download.asp?f=wavecomp


  do {

	fpaudio_buffer = pbuffer;
	fbuffer_size = psize;
	fwrite_ptr = pwprt;
	fread_ptr = prprt;
	fmutex = pmutex;
	flockfd = fd;


  	printf("MplayerFileSource  pbuffer %x \n",* pbuffer);
    fAudioFormat = (unsigned char)1;//WA_PCM;
    fNumChannels = (unsigned char)2;
	fSamplingFrequency = 44100;
    fBitsPerSample = (unsigned char)16;
   
    // The header is good; the remaining data are the sample bytes.
    fWAVHeaderSize = 0;


	#ifdef DUMP_AUDIO
	 dump_fd=open("/tmp/wav1", O_RDWR | O_CREAT | O_TRUNC);
	#endif
  } while (0);

  
  fPlayTimePerSample = 1e6/(double)fSamplingFrequency;

  // Although PCM is a sample-based format, we group samples into
  // 'frames' for efficient delivery to clients.  Set up our preferred
  // frame size to be close to 20 ms, if possible, but always no greater
  // than 1400 bytes (to ensure that it will fit in a single RTP packet)
  unsigned maxSamplesPerFrame = (1400*8)/(fNumChannels*fBitsPerSample);
  unsigned desiredSamplesPerFrame = (unsigned)(0.02*fSamplingFrequency);
  unsigned samplesPerFrame = desiredSamplesPerFrame < maxSamplesPerFrame
    ? desiredSamplesPerFrame : maxSamplesPerFrame;
  fPreferredFrameSize = (samplesPerFrame*fNumChannels*fBitsPerSample)/8;
}

MplayerFileSource::~MplayerFileSource() {
	
#ifdef DUMP_AUDIO
		::close(dump_fd);
#endif
}

// Note: We should change the following to use asynchronous file reading, #####
// as we now do with ByteStreamFileSource. #####
void MplayerFileSource::doGetNextFrame() {
 
  // Try to read as many bytes as will fit in the buffer provided
  // (or "fPreferredFrameSize" if less)
  
  unsigned const bytesPerSample = (fNumChannels*fBitsPerSample)/8;
  unsigned bytesToRead = fPreferredFrameSize - fPreferredFrameSize%bytesPerSample;
  unsigned int data_len,tmp;
	//static unsigned int count = 0;

SPACE_WAIT:


	 
  if(NULL != fmutex)
  		pthread_mutex_lock(fmutex);
	else
		lockf(flockfd,1,0);

#if 1

	if(NULL == *fpaudio_buffer)
	{
		if(NULL != fmutex)
			pthread_mutex_unlock(fmutex);
		else
			lockf(flockfd,0,0);
		
		printf("doGetNextFrame fpaudio_buffer is NULL \n");
		return;
	}
  if(bytesToRead > *fbuffer_size)
  	bytesToRead = *fbuffer_size;

 	fFrameSize = 0;
  data_len = (*fwrite_ptr-*fread_ptr+*fbuffer_size)%*fbuffer_size;

 // printf("doGetNextFrame bytesToRead %d  data_len %d \n",bytesToRead,data_len);
  

  if(data_len < bytesToRead )
  {
	 if(NULL != fmutex)
		pthread_mutex_unlock(fmutex);
	 else
	 	lockf(flockfd,0,0);
	 
	//printf("no data  bytesToRead %d  data_len %d \n",bytesToRead,data_len);
	 usleep(10000);
	goto SPACE_WAIT;
  }
 // if(bytesToRead > data_len)
//	 bytesToRead = data_len;

	
  if(0 !=bytesToRead)
	{
	  if((*fread_ptr + bytesToRead) <= *fbuffer_size)
	  {
		memcpy(fTo,*fpaudio_buffer + (*fread_ptr),bytesToRead);
		*fread_ptr += bytesToRead;
		#ifdef DUMP_AUDIO
		write(dump_fd,*fpaudio_buffer + (*fread_ptr),bytesToRead);
		#endif
	//	printf("less fread_ptr %d bytesToRead %d fbuffer_size %d\n",*fread_ptr,bytesToRead,*fbuffer_size );
	  }
	  else
	  {
	  	if(*fread_ptr > *fbuffer_size)
	  	{
	  		printf("*fread_ptr  %d > *fpaudio_buffer %d  dif %d\n ",*fread_ptr,*fbuffer_size,*fbuffer_size - *fread_ptr);
	  		*fread_ptr = 0;
			 
	  	}
		else
		{
		tmp = *fbuffer_size - *fread_ptr;
		memcpy(fTo,*fpaudio_buffer + (*fread_ptr),tmp);
		#ifdef DUMP_AUDIO
		write(dump_fd,*fpaudio_buffer + (*fread_ptr),tmp);
		#endif
		
		memcpy(fTo + tmp,*fpaudio_buffer,bytesToRead -tmp);

		#ifdef DUMP_AUDIO
		write(dump_fd,*fpaudio_buffer,bytesToRead -tmp);
		#endif
		
		 *fread_ptr = bytesToRead -tmp;
		}
		//printf("more fread_ptr %d bytesToRead %d fbuffer_size %d tmp %d\n",*fread_ptr,bytesToRead,*fbuffer_size,tmp );
	  }
	  fFrameSize = bytesToRead;
	  //count += bytesToRead;
	//  printf("read  count %d \n",count);
	}

#endif 
 
  if(NULL != fmutex)
		pthread_mutex_unlock(fmutex);
	else
		lockf(flockfd,0,0);
 // printf("mplayfilesource doGetNextFrame bytesToRead %d  *fpaudio_buffer %x fwrite_ptr %d  fread_ptr %d data_len %d\n",bytesToRead,*fpaudio_buffer,*fwrite_ptr,*fread_ptr,data_len);

  // Set the 'presentation time' and 'duration' of this frame:
  if (fPresentationTime.tv_sec == 0 && fPresentationTime.tv_usec == 0) {
    // This is the first frame, so use the current time:
    gettimeofday(&fPresentationTime, NULL);
  } else {
    // Increment by the play time of the previous data:
    unsigned uSeconds	= fPresentationTime.tv_usec + fLastPlayTime;
    fPresentationTime.tv_sec += uSeconds/1000000;
    fPresentationTime.tv_usec = uSeconds%1000000;
  }

  // Remember the play time of this data:
  fDurationInMicroseconds = fLastPlayTime
    = (unsigned)((fPlayTimePerSample*fFrameSize)/bytesPerSample);

  // Switch to another task, and inform the reader that he has data:
#if defined(__WIN32__) || defined(_WIN32)
  // HACK: One of our applications that uses this source uses an
  // implementation of scheduleDelayedTask() that performs very badly
  // (chewing up lots of CPU time, apparently polling) on Windows.
  // Until this is fixed, we just call our "afterGetting()" function
  // directly.  This avoids infinite recursion, as long as our sink
  // is discontinuous, which is the case for the RTP sink that
  // this application uses. #####
  afterGetting(this);
#else
  nextTask() = envir().taskScheduler().scheduleDelayedTask(0,
			(TaskFunc*)FramedSource::afterGetting, this);
#endif
//	 usleep(800);
}

Boolean MplayerFileSource::setInputPort(int /*portIndex*/) {
  return True;
}

double MplayerFileSource::getAverageLevel() const {
  return 0.0;//##### fix this later
}
