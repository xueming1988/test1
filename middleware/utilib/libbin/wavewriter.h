 /****************************************************************************
 *
 *		Wave writer.
 *		-----------
 *
 ****************************************************************************
 *
 *	Description:	Wave writer header.
 *
 *	Copyright:		Copyright DSP Concepts 2006-2017, All rights reserved.
 *
 ***************************************************************************/

 #ifndef _WAVEWRITER_H_
 #define _WAVEWRITER_H_

#ifdef WIN32
#include "StdAfx.h"
#endif
#include "stdio.h"
#ifdef LINUX
#include "string.h"
#elif !defined(NEEDWAVEFORMAT)
#include "Mmreg.h"
#endif


#ifndef WV_HEADER_DEFINED
#define WV_HEADER_DEFINED
struct  WV_HEADER
{
    char                RIFF[4];        // RIFF Header      Magic header
    unsigned int        ChunkSize;      // RIFF Chunk Size = sizeof(WV_HEADER) - 2 * sizeof(int) = 36
    char                WAVE[4];        // WAVE Header
    char                fmt[4];         // FMT header
    unsigned int        Subchunk1Size;  // Size of the fmt chunk = 16 (following through bitsPerSample)
    unsigned short      AudioFormat;    // Audio format 1=PCM,6=mulaw,7=alaw, 257=IBM Mu-Law, 258=IBM A-Law, 259=ADPCM
    unsigned short      NumOfChan;      // Number of channels 1=Mono 2=Sterio
    unsigned int        SamplesPerSec;  // Sampling Frequency in Hz
    unsigned int        bytesPerSec;    // bytes per second
    unsigned short      blockAlign;     // 2=16-bit mono, 4=16-bit stereo
    unsigned short      bitsPerSample;  // Number of bits per sample
    char                Subchunk2ID[4]; // "data"  string
    unsigned int        Subchunk2Size;  // Sampled data length
};
#endif

#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct _GUID {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[ 8 ];
} GUID;	// size = 16
#endif

#ifndef WV_HEADER2_DEFINED
#define WV_HEADER2_DEFINED
struct WV_HEADER2
{
    char                RIFF[4];        // RIFF Header      Magic header
    unsigned int        ChunkSize;      // RIFF Chunk Size = sizeof(WV_HEADER2) - 2 * sizeof(int) = ??
    char                WAVE[4];        // WAVE Header
    char                fmt[4];         // FMT header
    unsigned int        Subchunk1Size;  // Size of the fmt chunk = 40 (following through SubFormat)
    unsigned short      AudioFormat;    // Audio format 1=PCM,6=mulaw,7=alaw, 257=IBM Mu-Law, 258=IBM A-Law, 259=ADPCM
    unsigned short      NumOfChan;      // Number of channels 1=Mono 2=Stereo, etc.
    unsigned int        SamplesPerSec;  // Sampling Frequency in Hz
    unsigned int        bytesPerSec;    // bytes per second
    unsigned short      blockAlign;     // 2=16-bit mono, 4=16-bit stereo
    unsigned short      bitsPerSample;  // Number of bits per sample
	unsigned short      cbSize;			// size of WAVEFORMATEX
	unsigned short      wValidBitsPerSample;	// should be 32
	unsigned int        dwChannelMask;	// should be 0
	GUID                SubFormat;		// either KSDATAFORMAT_SUBTYPE_PCM or KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
    char                Subchunk2ID[4]; // "data"  string
    unsigned int        Subchunk2Size;  // Sampled data length
};
#endif

class CWaveWriter
{
public:
	FILE *m_fp;

	char m_filename[256];

	/** Format. */
	WAVEFORMATEX m_fmt;

public:
	unsigned int m_total_written;

	unsigned int m_channels;

	unsigned int m_rate;

	unsigned int m_bitWidth;

	bool m_useFloat;

	CWaveWriter(const char *fileName, int channels, int rate, int width, bool useFloat = false);

	~CWaveWriter()
	{
		Close();
	}

	int Open();

	int Close();

	int WriteSamples(void *pSamples, unsigned int samples);
};

#endif	// _WAVEWRITER_H_

