/*
 * Copyright 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * You may not use this file except in compliance with the terms and conditions set forth in the accompanying LICENSE.TXT file.
 *
 * THESE MATERIALS ARE PROVIDED ON AN "AS IS" BASIS.  AMAZON SPECIFICALLY DISCLAIMS, WITH RESPECT TO THESE MATERIALS, ALL
 * WARRANTIES, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, AND NON-INFRINGEMENT.
 */

#ifndef XCORR_CLASS_H
#define XCORR_CLASS_H

#include "../common/Radix4fft.h"
#include "../common/fixed_pt.h"

#define SYS_16K_BLK_SIZE                    (128)

#define  MAX_SEARCH_SAMPLE                  (128)           // Maximum sample index for XCorr calculation
#define  MIN_SEARCH_SAMPLE                  (32)            // Minimum sample index for XCorr calculation
#define  CENTROID_SHR                       (12)
#define  CENTROID_Q_FORMAT                  (15)
#define  CENTROID_GAIN                      (1.0*(1 << CENTROID_Q_FORMAT))
#define  CENTROID_SHIFT                     (CENTROIDQFORMAT)
#define  CENTROID_RND                       (1 << (CENTROID_SHIFT - 1))

#define  CLARITY_Q_FORMAT                   (28)   // should be even
#define  CLARITY_GAIN                       (1.0*(1 << CLARITY_Q_FORMAT/2))
#define  VAD_XCORR_SHR                      (5)
#define  VAD_XCORR_GAIN                     (1.0*(1 << VAD_XCORR_SHR))
#define  CENTROID_THR                       ((Word64)1 << 45)
#define  VAD_CENTROID_SHR                   (12)
#define  VAD_CENTROID_RND                   (1 << (VAD_CENTROID_SHR -1))
#define  VAD_FFT_LEN                        (2*SYS_16K_BLK_SIZE)

#define  VAD_CENTROID_AVR_START             (20)
#define  VAD_CENTROID_AVR_STOP              (80)
/*------------------------------------------------------------------------------------*/
#define  FFT_SCALE_UP_VAD                   (7)
#define  FFT_SCALE_DOWN_VAD                 (FFT_SCALE_UP_VAD)
#define  FFT_SCALE_UPVAD_RND                (1 << (FFT_SCALE_UP_VAD-1))

#define  FFT_DSC_ROUND_VAD                  (1 <<  (FFT_SCALE_DOWN_VAD - 1))

#define  REAL_FFT_SCALE_DOWN_VAD            (FFT_SCALE_DOWN_VAD + 1)
#define  REAL_FFT_SCALE_DOWN_RND_VAD        (1 <<  (REAL_FFT_SCALE_DOWN_VAD - 1))

#define  IFFT_SCALE_UP_VAD                  (0)
#define  IFFT_SCALE_DOWN_VAD                (CMPLXFFTPOWER256 + 1 + IFFT_SCALE_UP_VAD)

#define  IFFT_DSC_ROUND_VAD                 (1 <<  (IFFT_SCALE_DOWN_VAD - 1))

/*--------------------------------------------------------------------------------------*/

class XCORRClass {
private:

 /*--------------------------------------------------------------------
  *  m_centroid     = m_centroidfpt/2^CENTROIDQFORMAT;
  *  m_clarity      = m_clarityfpt/2^(CLARITYQFORMAT/2);
  *  m_frame_energy = m_frame_energyfpt*2^VADXCORRSHR;

  *  m_centroidfpt     = m_centroid*2^CENTROIDQFORMAT;
  *  m_clarityfpt      = m_clarity*2^(CLARITYQFORMAT/2);
  *  m_frame_energyfpt = m_frame_energy/2^VADXCORRSHR;
  *---------------------------------------------------------------------*/

    // stores value of frame energy
    Word64 m_frame_energy;

    //stores value of frame clarity
    Word64 m_clarity;

    //stores fft magnitude
    Word64 m_centroid;

    // frame length
    unsigned int m_frameLength;

   static const UWord32 k_minSearchSample;
   static const UWord32 k_maxSearchSample;

    FFT_PARAMS  VADfft_info;

public:

    // constructor
    XCORRClass(unsigned int N);

    // destructor
    ~XCORRClass();

    // main function that initiates calculation of features
    void calcCorrFeatures(Word16 *data);
    void autocorr(Word16 * x, Word32 * y);

    // get method to obtain calculated clarity
    Word64 getClarity();

    // get method to obtain calculated frame energy
    Word64 getFrameEnergy();

    // get method to obtain calculated frame centroid
    Word64 getFreqCentroid();
};


#endif // XCORR_CLASS_H
