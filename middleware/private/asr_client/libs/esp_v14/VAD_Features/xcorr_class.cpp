/*
 * Copyright 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * You may not use this file except in compliance with the terms and conditions set forth in the accompanying LICENSE.TXT file.
 *
 * THESE MATERIALS ARE PROVIDED ON AN "AS IS" BASIS.  AMAZON SPECIFICALLY DISCLAIMS, WITH RESPECT TO THESE MATERIALS, ALL
 * WARRANTIES, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, AND NON-INFRINGEMENT.
 */

#include <string.h>
#include "../common/fixed_pt.h"
#include "./xcorr_class.h"

const unsigned int XCORRClass::k_maxSearchSample = MAX_SEARCH_SAMPLE;
const unsigned int XCORRClass::k_minSearchSample = MIN_SEARCH_SAMPLE;

XCORRClass::~XCORRClass()
{

}

XCORRClass::XCORRClass(unsigned int N)
: m_frameLength(N), m_frame_energy(0.0), m_clarity(0.0), m_centroid(0.0) 
{
     /*--------------------------------------------------------------
     * Word32   MaxFFTPower;
     * Word32   CmplxFFTPower;
     * Word32   FFTScaleUP;
     * Word32   FFTScaleDown;
     * Word32   FFTDScRound;
     * Word32   RealFFTScaleDown;
     * Word32   RealFFTScaleDownRnd;
     * Word32   IFFTScaleUP;
     * Word32   IFFTScaleDown;
     * Word32   IFFTDScRound;
     * const    Word32 *pDrTbl;
     * const    Scmplx *pTwiddleTable;
     *--------------------------------------------------------------*/
    VADfft_info.MaxFFTPower             =  MAXFFTPOWER256;
    VADfft_info.CmplxFFTPower           =  CMPLXFFTPOWER256;
    VADfft_info.pDrTbl                  =  FFT256_DrTable;
    VADfft_info.pTwiddleTable           =  FFT256_TwiddleTable;

    VADfft_info.FFTScaleUP              =  FFT_SCALE_UP_VAD;
    VADfft_info.FFTScaleDown            =  FFT_SCALE_DOWN_VAD;
    VADfft_info.FFTDScRound             =  FFT_DSC_ROUND_VAD;
    VADfft_info.RealFFTScaleDown        =  REAL_FFT_SCALE_DOWN_VAD;
    VADfft_info.RealFFTScaleDownRnd     =  REAL_FFT_SCALE_DOWN_RND_VAD;
    VADfft_info.IFFTScaleUP             =  IFFT_SCALE_UP_VAD;
    VADfft_info.IFFTScaleDown           =  IFFT_SCALE_DOWN_VAD;
    VADfft_info.IFFTDScRound            =  IFFT_DSC_ROUND_VAD;

}

void XCORRClass::calcCorrFeatures(Word16 * data)
{
    UWord32 uk;
    Word16  xdata[VAD_FFT_LEN];
    Word16 * dataPtr;
    Word16 * ptrdataIn;
    Word32  vadfftBuffIn[VAD_FFT_LEN];
    Word32  vadfftBuffOut[VAD_FFT_LEN];
    Word32  vadxcorrbuff[VAD_FFT_LEN/2];
    Word32  clarityfpt,frame_energyfpt;

    Word32  MaxClarity,MinClarity,tmp32;
    Word32  tmp32r,tmp32i;
    Word32  * ptrxcorr;
    Lcmplx  * ptrfftin;
    Lcmplx  * ptrfftout;
    UWord32 utmp32;
    UWord64 utmp64;
    Word64  Acc64centroid,Acc64totalsum;

     ptrdataIn= data;
     dataPtr  = &xdata[0];
     memset(dataPtr,  0,  VAD_FFT_LEN*sizeof(Word16));
     memcpy(dataPtr, ptrdataIn, (VAD_FFT_LEN/2)*sizeof(Word16));

     ptrxcorr = &vadxcorrbuff[0];
     memset(ptrxcorr,  0, (VAD_FFT_LEN/2)*sizeof(Word32));
/*-------------------------------------------------------------------------------------------*/
    ptrfftin      = (Lcmplx *)vadfftBuffIn;
    ptrfftout     = (Lcmplx *)vadfftBuffOut;

    memset(ptrfftin,      0, (VAD_FFT_LEN)*sizeof(Word32));
    memset(ptrfftout,     0, (VAD_FFT_LEN)*sizeof(Word32));

    for(uk = 0; uk < VAD_FFT_LEN/4; uk++)
    {
        ptrfftin[uk].r = ((Word32)data[2*uk]   << VADfft_info.FFTScaleUP);
        ptrfftin[uk].i = ((Word32)data[2*uk+1] << VADfft_info.FFTScaleUP);
    }

    CmplxMixedRadix4fft(ptrfftin,ptrfftout,&VADfft_info,REALFFT);

    Acc64centroid = 0; Acc64totalsum = 0;

   for (uk = VAD_CENTROID_AVR_START; uk <= VAD_CENTROID_AVR_STOP; uk += 2)
   {
     tmp32r   =  ptrfftout[uk].r;
     tmp32i   =  ptrfftout[uk].i;

     utmp64   =  _mpy32ll(tmp32r,tmp32r);
     utmp64  +=  _mpy32ll(tmp32i,tmp32i);

     Acc64totalsum  +=  utmp64;        //m_frame_energy
     Acc64centroid  +=  (utmp64*uk);   //m_centroid
   }

   m_frame_energy = Acc64totalsum;

   //centroid value
   Acc64centroid= ((Acc64centroid + VAD_CENTROID_RND) >> VAD_CENTROID_SHR);
   m_centroid = Acc64centroid;

   autocorr(dataPtr,ptrxcorr);

   frame_energyfpt = ptrxcorr[0];      //m_frame_energy/32

    /*-------------------------------------------------------------------------
     * Clarity for each frame is defined as:
     * clarity = Dmin / Dmax
     * Here "D" is beta * sqrt(2*(R_0 - R_k))
     * R_0 is the autocorrelation at lag 0
     * R_k is the autocorrelation at lag k
     * for speech "k" is limited by minSearchSample and maxSearchSample
     * minSearchSample is determined by maxSearchFreq
     * maxSearchSample is determined by minSearchFreq
     * see setSearchFreqs() for easy formulae.
     * the following loop searches for max and min of "D"
     * the max and min values are stored in maxClaSearchVal and minClaSearchVal
     *--------------------------------------------------------------------------*/
    uk     = MIN_SEARCH_SAMPLE;
    tmp32  = frame_energyfpt - ptrxcorr[uk];
    MaxClarity = tmp32;
    MinClarity = tmp32;

    for (uk = MIN_SEARCH_SAMPLE+1; uk < MAX_SEARCH_SAMPLE; uk++)
    {
        tmp32  = frame_energyfpt - ptrxcorr[uk];

        if(MaxClarity < tmp32)
	    {
           MaxClarity = tmp32;
		}
        else if(MinClarity > tmp32)
		{
           MinClarity = tmp32;
		}
    }

    if (MaxClarity <= 0)
	{
        clarityfpt = 0;
	}
    else
    {
        utmp64     = (((UWord64)MinClarity) << CLARITY_Q_FORMAT);
        utmp32     = utmp64/((UWord32)MaxClarity);
        clarityfpt = Sqrt_Func(utmp32);
    }

    m_clarity      = (Word64)(clarityfpt);

   
    return;
}

// returns the m_clarity value
Word64 XCORRClass::getClarity()
{
    return m_clarity;
}

Word64 XCORRClass::getFrameEnergy()
{
    return m_frame_energy;
}

Word64 XCORRClass::getFreqCentroid()
{
    return m_centroid;
}

void XCORRClass::autocorr(Word16 * x, Word32 * y)
{
    int m, n;
    short *ptrh,*ptrx;
    short x0,x1,x2,x3,x4,x5,x6,x7,x8,h0,h1,h2,h3,h4,h5,h6,h7;
    Word40 sum0, sum1;

    for (m = 0; m < SYS_16K_BLK_SIZE; m += 2)
    {
        sum0 = 0;
        sum1 = 0;
        ptrh = &x[0];
        ptrx = &x[m];
        x0   = *ptrx++;
        for (n = m; n < SYS_16K_BLK_SIZE; n += 8)
        {
            x1 = *ptrx++;
            x2 = *ptrx++;
            x3 = *ptrx++;
            x4 = *ptrx++;
            x5 = *ptrx++;
            x6 = *ptrx++;
            x7 = *ptrx++;
            x8 = *ptrx++;

            h0 = *ptrh++;
            h1 = *ptrh++;
            h2 = *ptrh++;
            h3 = *ptrh++;
            h4 = *ptrh++;
            h5 = *ptrh++;
            h6 = *ptrh++;
            h7 = *ptrh++;

            sum0 += x0 * h0;
            sum1 += x1 * h0;
            sum0 += x1 * h1;
            sum1 += x2 * h1;
            sum0 += x2 * h2;
            sum1 += x3 * h2;
            sum0 += x3 * h3;
            sum1 += x4 * h3;
            sum0 += x4 * h4;
            sum1 += x5 * h4;
            sum0 += x5 * h5;
            sum1 += x6 * h5;
            sum0 += x6 * h6;
            sum1 += x7 * h6;
            sum0 += x7 * h7;
            sum1 += x8 * h7;
            x0    = x8;
        }

        y[m]   = (Word32)(sum0 >> VAD_XCORR_SHR);
        y[m+1] = (Word32)(sum1 >> VAD_XCORR_SHR);
    }

     return;
}


