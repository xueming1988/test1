/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

**********/
// "liveMedia"


#include "MPLAYMP3Source.hh"
#include "GroupsockHelper.hh"
#include <pthread.h>
#include <stdlib.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

////////// MplayerMP3Source //////////

/*mp3 frame head size*/ 
#define MP3_FRAME_HEAD_SIZE 4

// MPEGAudioRet::mErrCode
enum{
      MPEG_AUDIO_OK = 0
    , MPEG_AUDIO_NEED_MORE = -99
};


/**
 * 代表返回结果的结构体
 */
typedef struct _MPEGAudioRet{
    int mErrCode;
    int mNextPos;
} MPEGAudioRet;


/**
 * 代表MPEG音频帧的信息
 */
typedef struct _MPEGAudioFrameInfo{
    
    // MPEG Audio Frame Header /////////////////////////////////////////////////
    
    /**
     * MPEG-1.0: 10
     * MPEG-2.0: 20
     * MPEG-2.5: 25
     * invalid : other
     */
    int mMPEGVersion : 6;
    
    /**
     * Layer I  : 1
     * Layer II : 2
     * Layer III: 3
     * invalid  : other
     */
    int mLayer : 3;
    
    /**
     * in kbits/s
     */
    int mBitrate : 10;
    
    /**
     * in Bytes
     */
    int mPaddingSize : 4;
    
    
    /**
     * Channel mode
     *
     * Joint Stereo (Stereo) - 0
     * Single channel (Mono) - 1
     * Dual channel (Two mono channels) - 2
     * Stereo - 3
     */
    int mChannelMode : 3;
    
    /**
     * Mode extension, Only used in Joint Stereo Channel mode.
     * not process
     */
    int mExtensionMode : 3;
    
    /**
     * Emphasis:
     * The emphasis indication is here to tell the decoder that the file must be 
     * de-emphasized, that means the decoder must 're-equalize' the sound after 
     * a Dolby-like noise suppression. It is rarely used.
     * 
     * 0 - none
     * 1 - 50/15 ms
     * 2 - reserved (invalid)
     * 3 - CCIT J.17
     */
    int mEmphasis : 3;

    
    /**
     * in Hz
     */
    int mSamplerate : 17;
    

    /**
     * Protection off: 0
     * Protection on : 1
     */
    int mProtection : 2;
    

    /**
     * Copyright bit, only informative
     */
    int mCopyrightBit : 2;
    
    /**
     * Original bit, only informative
     */
    int mOriginalBit : 2;
    
    /**
     * 边信息大小(Layer III), in Bytes
     */
    int mSideInfoSize : 7;
    
    
    /**
     * Samples per frame
     */
    int mSamplesPerFrame : 12;
    
    /**
     * 0 - CBR
     * 1 - CBR(INFO)
     * 2 - VBR(XING)
     * 3 - VBR(VBRI)
     */
    int mBitrateType : 3;
    
    /**
     * 2 Bytes
     */
    unsigned short int mCRCValue;
    
    
    // XING, INFO or VBRI Header /////////////////////////////////////////////////////

    /**
     * mTotalFrames
     */
    int mTotalFrames;
    
    /**
     * mTotalBytes
     */
    int mTotalBytes;
    
    /**
     * Quality indicator
     * 
     * From 0 - worst quality to 100 - best quality
     */
    short int mQuality;



    // only for VBRI Header ////////////////////////////////////////////////////
    
    /**
     * Size per table entry in bytes (max 4)
     */
    short int  mEntrySize : 4;
    
    
    /**
     * Number of entries within TOC table
     */
    short int  mEntriesNumInTOCTable;
    
    /**
     * Scale factor of TOC table entries
     */
    short int  mTOCTableFactor;
    
    /**
     * VBRI version
     */
    short int mVBRIVersionID;
    
    /**
     * Frames per table entry
     */
    short int mFramesNumPerTable;
    
    /**
     * VBRI delay
     * 
     * Delay as Big-Endian float.
     */
    unsigned char mVBRIDelay[2]; 
    
    
    ////////////////////////////////////////////////////////////////////////////
 
    /**
     * Frame size
     */
    int mFrameSize;
} MPEGAudioFrameInfo;

int play_Samplerate = 0;


//#define DUMP_AUDIO 1

#ifdef DUMP_AUDIO
static int dump_fd = -1;  1
#endif


MplayerMP3Source*
MplayerMP3Source::createNew(UsageEnvironment& env, char ** pbuffer,
										  unsigned int * psize,
										  unsigned int * pwprt,
										  unsigned int * prprt,
										  pthread_mutex_t* pmutex,
										  int fd) {
								  
  do {

    MplayerMP3Source* newSource = new MplayerMP3Source(env,pbuffer,psize,pwprt,prprt,pmutex,fd);

	printf("MplayerMP3Source createNew pbuffer %x  newSource %x\n",*pbuffer,newSource);
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

unsigned MplayerMP3Source::numPCMBytes() const {
  if (fFileSize < fWAVHeaderSize) return 0;
  return fFileSize - fWAVHeaderSize;
}

void MplayerMP3Source::setScaleFactor(int scale) {
  fScaleFactor = scale;

  if (fScaleFactor < 0 ) {
    // Because we're reading backwards, seek back one sample, to ensure that
    // (i)  we start reading the last sample before the start point, and
    // (ii) we don't hit end-of-file on the first read.
    int const bytesPerSample = (fNumChannels*fBitsPerSample)/8;
  }
}

void MplayerMP3Source::seekToPCMByte(unsigned byteNumber) {

}

unsigned char MplayerMP3Source::getAudioFormat() {
  return fAudioFormat;
}



MplayerMP3Source::MplayerMP3Source(UsageEnvironment& env, char ** pbuffer,
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


  	printf("MplayerMP3Source  pbuffer %x \n",* pbuffer);
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

MplayerMP3Source::~MplayerMP3Source() {
	
#ifdef DUMP_AUDIO
		::close(dump_fd);
#endif
}




enum{
    MPEG_AUDIO_ERR = -199
};



/**
 * 比特率表 (kbits/s)
 */
const short int BitrateTable[2][3][15] =
{
    {
        {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448},
        {0,32,48,56,64 ,80 ,96 ,112,128,160,192,224,256,320,384},
        {0,32,40,48,56 ,64 ,80 ,96 ,112,128,160,192,224,256,320}
    },
    {
        {0,32,48,56,64 ,80 ,96 ,112,128,144,160,176,192,224,256},
        {0,8 ,16,24,32 ,40 ,48 ,56 ,64 ,80 ,96 ,112,128,144,160},
        {0,8 ,16,24,32 ,40 ,48 ,56 ,64 ,80 ,96 ,112,128,144,160}
    }
};


/**
 * 采样率表
 */
int SamplerateTable[3][3] =
{
    {44100,48000,32000 }, // MPEG-1
    {22050,24000,16000 }, // MPEG-2
    {11025,12000,8000  }  // MPEG-2.5
};


int calculateFrame(MPEGAudioFrameInfo * info)
{
    info->mFrameSize = (info->mSamplesPerFrame * info->mBitrate * 1000)
            / (8 * info->mSamplerate)
            + info->mPaddingSize;

//	printf("calculateFrame info->mSamplesPerFrame %d  info->mBitrate %d  info->mSamplerate %d frame size %d \n",info->mSamplesPerFrame,info->mBitrate,info->mSamplerate,info->mFrameSize);
    return MPEG_AUDIO_OK;
}


//==============================================================================
// parseMpegFrameHdr
//==============================================================================
int parseMpegFrameHdr(
        unsigned char * hdrBuf, 
        int bufSize,
        MPEGAudioFrameInfo * info, 
        bool firstFrame)
{
    if(bufSize < 4) // 帧头至少有4个字节
     {
     	printf("parseMpegFrameHdr  %d \n",bufSize);
        return MPEG_AUDIO_NEED_MORE;
     }
    // Protection Bit
    info->mProtection = (hdrBuf[1] & 0x01);
    info->mProtection = (0 == info->mProtection ? 1 : 0);
    
    if(info->mProtection && bufSize < 6)
    {
        // protected by 16 bit CRC following header
        printf("parseMpegFrameHdr mProtection  %d \n",bufSize);
        return MPEG_AUDIO_NEED_MORE;
    }
    
    
    // MPEG版本
    info->mMPEGVersion = ((hdrBuf[1]>>3) & 0x03);
    switch(info->mMPEGVersion)
    {
    case 0:
        info->mMPEGVersion = 25;
        break;
    case 2:
        info->mMPEGVersion = 20;
        break;
    case 3:
        info->mMPEGVersion = 10;
        break;
    default:
        info->mMPEGVersion = 0;
		printf("error  info->mMPEGVersion %d \n",info->mMPEGVersion);
        return MPEG_AUDIO_ERR;
    };
    //printf(" info->mMPEGVersion %d \n",info->mMPEGVersion);
    // Layer index
    info->mLayer = ((hdrBuf[1]>>1) & 0x03);
    switch(info->mLayer)
    {
    case 1:
        info->mLayer = 3;
        break;
    case 2:
        info->mLayer = 2;
        break;
    case 3:
        info->mLayer = 1;
        break;
    default:
        info->mLayer = 0;
		printf("error  info->mLayer %d \n",info->mLayer);
        return MPEG_AUDIO_ERR;
    };
      //printf(" info->mLayer %d \n",info->mLayer);
    // 比特率
    info->mBitrate = ((hdrBuf[2]>>4)&0x0f);
    if(info->mBitrate == 0x0f)
       {
       	 printf("error  info->mBitrate %d \n",info->mBitrate);
       	return MPEG_AUDIO_ERR;
    	}
    unsigned char index_I = (info->mMPEGVersion==10 ? 0 : 1);
    unsigned char index_II = info->mLayer - 1;

    info->mBitrate = BitrateTable[index_I][index_II][info->mBitrate];
     //printf(" info->mBitrate %d \n",info->mBitrate);
    
    // 采样率
    info->mSamplerate = ((hdrBuf[2]>>2)&0x03);
    if(info->mSamplerate == 0x03)
        {
        	 printf("error  info->mSamplerate %d \n",info->mSamplerate);
        return MPEG_AUDIO_ERR;
    	}

    index_I = 2; // MPEG-2.5 by default
    if(info->mMPEGVersion == 20)
        index_I = 1;
    else if(info->mMPEGVersion == 10)
        index_I = 0;

	//printf(" info->mSamplerate  index_I  %d  mSamplerate %d\n",index_I,info->mSamplerate);
    info->mSamplerate = SamplerateTable[index_I][info->mSamplerate];
    play_Samplerate = SamplerateTable[0][0];
    //printf(" info->mSamplerate %d  play_Samplerate %d  read %d \n",info->mSamplerate,play_Samplerate,SamplerateTable[0][0]);
    // Padding size
    info->mPaddingSize = ((hdrBuf[2]>>1)&0x01);
    if(info->mPaddingSize)
    {
        info->mPaddingSize = ( info->mLayer==1 ? 4 : 1 );
    }
    
     //printf(" info->mPaddingSize %d \n",info->mPaddingSize);
    // channel mode
    info->mChannelMode = ((hdrBuf[3]>>6)&0x03);
    switch(info->mChannelMode)
    {
    case 0:
        info->mChannelMode = 3;
        break;
    case 1:
        info->mChannelMode = 0;
        break;
    case 2:
        info->mChannelMode = 2;
        break;
    case 3:
    default:
        info->mChannelMode = 1;
    };
     //printf(" info->mChannelMode %d \n",info->mChannelMode);
    // 在MPEG-1 Layer II中，只有某些比特率和某些模式的组合是允许的。
    // 在MPEG -2/2.5，没有此限制。
    if(info->mMPEGVersion == 10 && info->mLayer == 2)
    {
        if( 32 == info->mBitrate
                || 48 == info->mBitrate
                || 56 == info->mBitrate
                || 80 == info->mBitrate )
        {
            if( info->mChannelMode != 1 )
               {
               printf("return  MPEG_AUDIO_ERR 1");
               return MPEG_AUDIO_ERR;
            	}
        }
        else if( 224 == info->mBitrate
                || 256 == info->mBitrate
                || 320 == info->mBitrate
                || 384 == info->mBitrate )
        {
            if( 1 == info->mChannelMode )
               {
                  printf("return  MPEG_AUDIO_ERR 2");
               return MPEG_AUDIO_ERR;
            	}
        }
    }
    
    //  Extension Mode
    info->mExtensionMode = ((hdrBuf[3]>>4)&0x03);


    info->mCopyrightBit = ((hdrBuf[3]>>3)&0x01);

    info->mOriginalBit = ((hdrBuf[3]>>2)&0x01);


    // The emphasis indication is here to tell the decoder that the file must be 
    // de-emphasized, that means the decoder must 're-equalize' the sound after 
    // a Dolby-like noise suppression. It is rarely used.
    info->mEmphasis = ((hdrBuf[3])&0x03);
    if(0x2 == info->mEmphasis)
       {
        printf("return  info->mEmphasis %d",info->mEmphasis);
       return -1;
    	}
   
    // 每帧数据的采样数
    info->mSamplesPerFrame = 1152;
    if(1 == info->mLayer)
    {
        info->mSamplesPerFrame = 384;
    }
    else if(info->mMPEGVersion != 10 && 3 == info->mLayer)
    {
        info->mSamplesPerFrame = 576;
    }
    
    // 边信息大小
    info->mSideInfoSize = 0;
    if(3 == info->mLayer)
    {
        if(info->mMPEGVersion != 10) // MPEG-2/2.5 (LSF)
        {
            if(info->mChannelMode != 1) // Stereo, Joint Stereo, Dual Channel
                info->mSideInfoSize = 17;
            else // Mono
                info->mSideInfoSize = 9;
        }
        else // MPEG-1.0
        {
            if(info->mChannelMode != 1) // Stereo, Joint Stereo, Dual Channel
                info->mSideInfoSize = 32;
            else // Mono
                info->mSideInfoSize = 17;
        }
    }
    
    
    info->mBitrateType = 0; // common CBR by default
    
    
    
label_get_XING_or_INFO:
    
    calculateFrame(info);
    
    
    return MPEG_AUDIO_OK;
}


// Note: We should change the following to use asynchronous file reading, #####
// as we now do with ByteStreamFileSource. #####
void MplayerMP3Source::doGetNextFrame() {
 
  // Try to read as many bytes as will fit in the buffer provided
  // (or "fPreferredFrameSize" if less)
  int i = 0;
  int j = 0;
  unsigned char* buf = NULL;
  MPEGAudioFrameInfo FrameInfo ;
  MPEGAudioRet ret;
	  
  unsigned const bytesPerSample = (fNumChannels*fBitsPerSample)/8;
  unsigned bytesToRead = 1254;
  unsigned int data_len,tmp;
  unsigned char tmp_buff[30];
	//static unsigned int count = 0;

	 memset(&FrameInfo, 0, sizeof(FrameInfo));
SPACE_WAIT:

	buf =(unsigned char*)(*fpaudio_buffer + (*fread_ptr));

	 
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

	  
	fFrameSize = 0;
	data_len = (*fwrite_ptr-*fread_ptr+*fbuffer_size)%*fbuffer_size;



 #if 1
	  ret.mErrCode = MPEG_AUDIO_NEED_MORE;
	  ret.mNextPos = 0;
  
	  //printf("seek header  %d\n ",data_len);
	   for(i=0 ; i < data_len; i++ )
	   {
		   if((*fread_ptr + i + MP3_FRAME_HEAD_SIZE) > *fbuffer_size)
			   {
	
				  
						  memset(tmp_buff,0,sizeof(tmp_buff));
			//	  *fread_ptr = 0;
				  if(((*fread_ptr)+i) <= *fbuffer_size)
				  {

					 memcpy(tmp_buff,*fpaudio_buffer + (*fread_ptr)+i,1+ *fbuffer_size - ((*fread_ptr)+i));
					 memcpy(tmp_buff+(1+ *fbuffer_size - ((*fread_ptr)+i)),*fpaudio_buffer,10); 	

					for(j=0 ; j < 10; j++ )
					{
	
						if( (*(tmp_buff+j)) ==0xff && ((*(tmp_buff+j+1))&0xe0)==0xe0 )
						{
						   memset(&FrameInfo, 0, sizeof(FrameInfo));
						   ret.mErrCode = parseMpegFrameHdr((unsigned char * )tmp_buff, sizeof(tmp_buff), &FrameInfo, false);
						   if( MPEG_AUDIO_OK == ret.mErrCode 
								   || MPEG_AUDIO_NEED_MORE == ret.mErrCode)
						   {

								*fread_ptr += i + j;

								if(0 == FrameInfo.mFrameSize)	
									*fread_ptr  = 0;//avoid endless loop 
									
								if(*fread_ptr > *fbuffer_size)
									*fread_ptr = *fread_ptr - *fbuffer_size;

								if(*fread_ptr > *fbuffer_size)
									*fread_ptr  = 0;

								printf("oops get data mFrameSize %d *fread_ptr %d  *fbuffer_size %d  \n",FrameInfo.mFrameSize,*fread_ptr,*fbuffer_size);
									bytesToRead = FrameInfo.mFrameSize;
								i = 0x1ff00;  //jump out of loop i
							   break;
						   }
						}

					}  	
					
				  }

				if(0x1ff00 == i)
					{
						printf("get data at end of buffer  bytesToRead %u",bytesToRead);
						break;
					}
				 printf("seek header over buffer size	err  break i %d  FrameInfo.mFrameSize %d  *fread_ptr %u  *fbuffer_size %u\n ",i, FrameInfo.mFrameSize,*fread_ptr,*fbuffer_size);
				 
				*fread_ptr  = 0;
				if(NULL != fmutex)
					pthread_mutex_unlock(fmutex);
				else
					lockf(flockfd,0,0);

				 
				goto SPACE_WAIT;
			   

				  break;
			   }
  
			   
		   // 帧同步标识: 1111 1111 111x xxxxb
		   if( (*(buf+i)) ==0xff && ((*(buf+i+1))&0xe0)==0xe0 )
		   {
			   memset(&FrameInfo, 0, sizeof(FrameInfo));
			   ret.mErrCode = parseMpegFrameHdr((unsigned char * )buf+i, data_len-i, &FrameInfo, false);
			   if( MPEG_AUDIO_OK == ret.mErrCode 
					   || MPEG_AUDIO_NEED_MORE == ret.mErrCode)
			   {

					*fread_ptr += i;
					if(*fread_ptr > *fbuffer_size)
						*fread_ptr = 0;

						bytesToRead = FrameInfo.mFrameSize;
						
					if(0 == bytesToRead)
						*fread_ptr++;  //avoid endless loop 
						
				   break;
			   }
		   }
		   else if(i==data_len-1 && buf[i+1] != 0xff)
		   {
			   i++;
		   }
	   }
  //  printf("seek header over i  %d\n ",i);

#endif

 if(0 != i)
  printf("lost data. get data bytesToRead %d  i %d  \n",bytesToRead,i);







	
  if(bytesToRead > *fbuffer_size)
  	bytesToRead = *fbuffer_size;

 // printf("doGetNextFrame bytesToRead %d  data_len %d \n",bytesToRead,data_len);
  

  if(data_len < bytesToRead )
  {
	 if(NULL != fmutex)
		pthread_mutex_unlock(fmutex);
	 else
	 	lockf(flockfd,0,0);
	 
	printf("no data  bytesToRead %d  data_len %d \n",bytesToRead,data_len);
	 usleep(10000);
	goto SPACE_WAIT;
  }
  if(bytesToRead > data_len)
	 bytesToRead = data_len;




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
 // printf("MPLAYMP3Source doGetNextFrame bytesToRead %d  *fpaudio_buffer %x fwrite_ptr %d  fread_ptr %d data_len %d\n",bytesToRead,*fpaudio_buffer,*fwrite_ptr,*fread_ptr,data_len);

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
  if(FrameInfo.mSamplerate > 0 )
  	fDurationInMicroseconds = fLastPlayTime = 1152 * 1000*1000 / FrameInfo.mSamplerate;
  else
  	fDurationInMicroseconds = fLastPlayTime = 1152 * 1000*1000 / 44100;
 // = (unsigned)((5*fPlayTimePerSample*fFrameSize)/bytesPerSample);

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

Boolean MplayerMP3Source::setInputPort(int /*portIndex*/) {
  return True;
}

double MplayerMP3Source::getAverageLevel() const {
  return 0.0;//##### fix this later
}
