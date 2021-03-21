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
// NOTE: Samples are returned in little-endian order (the same order in which
// they were stored in the file).
// C++ header

#ifndef _MPLAYER_AUDIO_MP3_SOURCE_HH
#define _MPLAYER_AUDIO_MP3_SOURCE_HH

#ifndef _AUDIO_INPUT_DEVICE_HH
#include "AudioInputDevice.hh"
#endif


class MplayerMP3Source: public AudioInputDevice {
public:

  static MplayerMP3Source* createNew(UsageEnvironment& env, char ** pbuffer,
										  unsigned int * psize,
										  unsigned int * pwprt,
										  unsigned int * prprt,
										  pthread_mutex_t* pmutex,
										  int fd);

  unsigned numPCMBytes() const;
  void setScaleFactor(int scale);
  void seekToPCMByte(unsigned byteNumber);

  unsigned char getAudioFormat();

protected:
  MplayerMP3Source(UsageEnvironment& env, char ** pbuffer,
										  unsigned int * psize,
										  unsigned int * pwprt,
										  unsigned int * prprt,
										  pthread_mutex_t* pmutex,
										  int fd);
	// called only by createNew()

  virtual ~MplayerMP3Source();

private:
  // redefined virtual functions:
  virtual void doGetNextFrame();
  virtual Boolean setInputPort(int portIndex);
  virtual double getAverageLevel() const;

private:
  double fPlayTimePerSample; // useconds
  unsigned fPreferredFrameSize;
  unsigned fLastPlayTime; // useconds
  unsigned fWAVHeaderSize;
  unsigned fFileSize;
  int fScaleFactor;

  unsigned char fAudioFormat;
  char ** fpaudio_buffer;
  unsigned int * fbuffer_size;
  unsigned int * fwrite_ptr;
  unsigned int * fread_ptr;
  pthread_mutex_t* fmutex;
  int flockfd;
};

#endif
