/****************************************************************************
 *
 *		Audio Input Library
 *		-------------------
 *
 ****************************************************************************
 *
 *	Description:	Audio input interface.
 *
 *	Copyright:		Copyright DSP Concepts 2006-2012, All rights reserved.
 *
 ***************************************************************************/

/**
 * @addtogroup AudioInputLibrary
 * @{
 */

#ifndef _WAVEFORMAT_H_
#define _WAVEFORMAT_H_

#include "utiltypes.h"

#if !defined(WIN32) || defined(NEEDWAVEFORMAT)

/** Microsoft WAV format. */
struct WAVEFORMATEX
{
    unsigned short      wFormatTag;         /**< format type */
    unsigned short      nChannels;          /**< number of channels (i.e. mono, stereo...) */
    unsigned int        nSamplesPerSec;     /**< sample rate */
    unsigned int        nAvgBytesPerSec;    /**< for buffer estimation */
    unsigned short      nBlockAlign;        /**< block size of data */
    unsigned short      wBitsPerSample;     /**< number of bits per sample of mono data */
    unsigned short      cbSize;             /**< the count in bytes of the size of */
};

#endif	// WIN32

#endif	// _WAVEFORMAT_H_

/**
 * @}
 * End of file.
 */
