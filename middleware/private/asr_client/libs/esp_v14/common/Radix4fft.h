/*
 * Copyright 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * You may not use this file except in compliance with the terms and conditions set forth in the accompanying LICENSE.TXT file.
 *
 * THESE MATERIALS ARE PROVIDED ON AN "AS IS" BASIS.  AMAZON SPECIFICALLY DISCLAIMS, WITH RESPECT TO THESE MATERIALS, ALL
 * WARRANTIES, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, AND NON-INFRINGEMENT.
 */

#ifndef RADIX4FFT
#define RADIX4FFT

#ifdef __cplusplus
extern "C" {
#endif

#include "./fixed_pt.h"


typedef struct
{
  Word32   MaxFFTPower;
  Word32   CmplxFFTPower;
  Word32   FFTScaleUP;
  Word32   FFTScaleDown;
  Word32   FFTDScRound;
  Word32   RealFFTScaleDown;
  Word32   RealFFTScaleDownRnd;
  Word32   IFFTScaleUP;
  Word32   IFFTScaleDown;
  Word32   IFFTDScRound;
  const    Word32 *pDrTbl;
  const    Scmplx *pTwiddleTable;
}FFT_PARAMS;

/*------------------------------------------------------------------------------------
 *        Init data for Real fft size 256
 *-----------------------------------------------------------------------------------*/
#define  MAXFFTPOWER256                 (8)
#define  MAXFFTCMPLXSIZE256             (1 << MAXFFTPOWER256)
#define  MAXFFTREALSIZE256              (2*MAXFFTCMPLXSIZE256)
#define  TWIDDLETABLESIZE256            ((8  << (MAXFFTPOWER256 - 2)))

#define  CMPLXFFTPOWER256               (7)
#define  CMPLXFFTLENGTH256              (1 << CMPLXFFTPOWER256)
#define  REALFFTLENGTH256               (2*CMPLXFFTLENGTH256)
#define  DRTABLESIZE256                 (1 << CMPLXFFTPOWER256)

#ifndef  SDB_CVX_LKP_TBL
// FBF 256
#define  FFT_OUT_GAIN                   (0)
#define  FFTSCALEUPFBF_1                (5)
#define  FFTSCALEDOWNFBF_1              (FFTSCALEUPFBF_1 - FFT_OUT_GAIN)

#if(FFTSCALEDOWNFBF_1)
  #define  FFTDSCROUNDFBF_1             (1 << (FFTSCALEDOWNFBF_1 - 1))
#else
  #define  FFTDSCROUNDFBF_1             (0)
#endif

#define  REALFFTSCALEDOWNFBF_1          (FFTSCALEDOWNFBF_1 + 1)
#define  REALFFTSCALEDOWNRNDFBF_1       (1 <<  (REALFFTSCALEDOWNFBF_1 - 1))

#define  IFFTSCALEUPFBF_1               (FFT_OUT_GAIN)
#define  IFFTSCALEDOWNFBF_1             (CMPLXFFTPOWER256 + 1 + IFFTSCALEUPFBF_1)

#if(IFFTSCALEDOWNFBF_1)
  #define  IFFTDSCROUNDFBF_1            (1 << (IFFTSCALEDOWNFBF_1 - 1))
#else
  #define  IFFTDSCROUNDFBF_1            (0)
#endif

#define    FFT_INPUT_SHR                (15 - FFTSCALEUPFBF_1)
#define    FFT_INPUT_RND                (1 << (FFT_INPUT_SHR - 1))
#else //FBF 512

#define  FFT_OUT_GAIN                   (0)
#define  FFTSCALEUPFBF_1                (4)
#define  FFTSCALEDOWNFBF_1              (FFTSCALEUPFBF_1 - FFT_OUT_GAIN)

#if(FFTSCALEDOWNFBF_1)
  #define  FFTDSCROUNDFBF_1             (1 <<  (FFTSCALEDOWNFBF_1 - 1))
#else
  #define  FFTDSCROUNDFBF_1             (0)
#endif

#define  REALFFTSCALEDOWNFBF_1          (FFTSCALEDOWNFBF_1 + 1)
#define  REALFFTSCALEDOWNRNDFBF_1       (1 <<  (REALFFTSCALEDOWNFBF_1 - 1))

#define  IFFTSCALEUPFBF_1               (FFT_OUT_GAIN)
#define  IFFTSCALEDOWNFBF_1             (CMPLXFFTPOWER512 + 1 + IFFTSCALEUPFBF_1)

#if(IFFTSCALEDOWNFBF_1)
  #define  IFFTDSCROUNDFBF_1            (1 <<  (IFFTSCALEDOWNFBF_1 - 1))
#else
  #define  IFFTDSCROUNDFBF_1            (0)
#endif

#define    FFT_INPUT_SHR                (15 - FFTSCALEUPFBF_1)
#define    FFT_INPUT_RND                (1 <<  (FFT_INPUT_SHR - 1))

extern const       Scmplx FFT512_TwiddleTable[];
extern const       Word32 FFT512_DrTable[];
#endif


#ifdef AEC_OPTZ_FFT_256
/*------------------------------------------------------------------------------------
 *        Init data for Real fft size 256: for AEC
 *-----------------------------------------------------------------------------------*/
#define  FFT_OUT_SCALE               (3)

#define  FFTSCALEUPAEC_1             (2)
#define  FFTSCALEDOWNAEC_1           (FFTSCALEUPAEC_1 + FFT_OUT_SCALE)

#if(FFTSCALEDOWNAEC_1)
  #define  FFTDSCROUNDAEC_1           (1 <<  (FFTSCALEDOWNAEC_1 - 1))
#else
  #define  FFTDSCROUNDAEC_1           (0)
#endif

#define  REALFFTSCALEDOWNAEC_1        (FFTSCALEDOWNAEC_1 + 1)
#define  REALFFTSCALEDOWNRNDAEC_1     (1 <<  (REALFFTSCALEDOWNAEC_1 - 1))

#define  IFFTSCALEUPAEC_1             (0)
#define  IFFTSCALEDOWNAEC_1           (CMPLXFFTPOWER256 + 1 + IFFTSCALEUPAEC_1 - FFT_OUT_SCALE)

#if(IFFTSCALEDOWNAEC_1)
  #define  IFFTDSCROUNDAEC_1          (1 <<  (IFFTSCALEDOWNAEC_1 - 1))
#else
  #define  IFFTDSCROUNDAEC_1          (0)
#endif
/*---------------------------------------------------------------------------------*/
#define  FFTSCALEUPAEC_2              (0)
#define  FFTSCALEDOWNAEC_2            (FFTSCALEUPAEC_2 + FFT_OUT_SCALE)

#if(FFTSCALEDOWNAEC_2)
  #define  FFTDSCROUNDAEC_2           (1 <<  (FFTSCALEDOWNAEC_2 - 1))
#else
  #define  FFTDSCROUNDAEC_2           (0)
#endif

#define  REALFFTSCALEDOWNAEC_2        (FFTSCALEDOWNAEC_2 + 1)
#define  REALFFTSCALEDOWNRNDAEC_2     (1 <<  (REALFFTSCALEDOWNAEC_2 - 1))

#define  IFFTSCALEUPAEC_2             (2)
#define  IFFTSCALEDOWNAEC_2           (CMPLXFFTPOWER256 + 1 + IFFTSCALEUPAEC_2 - FFT_OUT_SCALE)

#if(IFFTSCALEDOWNAEC_2)
  #define  IFFTDSCROUNDAEC_2          (1 <<  (IFFTSCALEDOWNAEC_2 - 1))
#else
  #define  IFFTDSCROUNDAEC_2          (0)
#endif
/*-----------------------------------------------------------------------------------*/

#define  FFTSCALEUPAEC_3              (2)
#define  FFTSCALEDOWNAEC_3            (FFTSCALEUPAEC_3 + FFT_OUT_SCALE)

#if(FFTSCALEDOWNAEC_3)
  #define  FFTDSCROUNDAEC_3           (1 <<  (FFTSCALEDOWNAEC_3 - 1))
#else
  #define  FFTDSCROUNDAEC_3           (0)
#endif

#define  REALFFTSCALEDOWNAEC_3        (FFTSCALEDOWNAEC_3 + 1)
#define  REALFFTSCALEDOWNRNDAEC_3     (1 <<  (REALFFTSCALEDOWNAEC_3 - 1))

#define  IFFTSCALEUPAEC_3             (0)
#define  IFFTSCALEDOWNAEC_3           (CMPLXFFTPOWER256 + 1 + IFFTSCALEUPAEC_3 - FFT_OUT_SCALE)

#if(IFFTSCALEDOWNAEC_3)
  #define  IFFTDSCROUNDAEC_3          (1 <<  (IFFTSCALEDOWNAEC_3 - 1))
#else
  #define  IFFTDSCROUNDAEC_3          (0)
#endif
/*---------------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------------------*/
#define  REF_FFTSCALEUPAEC              (4)
#define  REF_FFTSCALEDOWNAEC            (REF_FFTSCALEUPAEC + FFT_OUT_SCALE)

#if(REF_FFTSCALEDOWNAEC)
  #define  REF_FFTDSCROUNDAEC           (1 << (REF_FFTSCALEDOWNAEC - 1))
#else
  #define  REF_FFTDSCROUNDAEC           (0)
#endif

#define  REF_REALFFTSCALEDOWNAEC        (REF_FFTSCALEDOWNAEC + 1)
#define  REF_REALFFTSCALEDOWNRNDAEC     (1 << (REF_REALFFTSCALEDOWNAEC - 1))


#define  REF_IFFTSCALEUPAEC             (0)
#define  REF_IFFTSCALEDOWNAEC           (CMPLXFFTPOWER256 + 1 + REF_IFFTSCALEUPAEC - FFT_OUT_SCALE)

#if(REF_IFFTSCALEDOWNAEC)
  #define  REF_IFFTDSCROUNDAEC          (1 << (REF_IFFTSCALEDOWNAEC - 1))
#else
  #define  REF_IFFTDSCROUNDAEC          (0)
#endif

/*---------------------------------------------------------------------------------------*/
#define  FFTSCALEUPAECVss              (4)
#define  FFTSCALEDOWNAECVss            (FFTSCALEUPAECVss)

#if(FFTSCALEDOWNAECVss)
  #define  FFTDSCROUNDAECVss           (1 << (FFTSCALEDOWNAECVss - 1))
#else
  #define  FFTDSCROUNDAECVss           (0)
#endif

#define  REALFFTSCALEDOWNAECVss        (FFTSCALEDOWNAECVss + 1)
#define  REALFFTSCALEDOWNRNDAECVss     (1 << (REALFFTSCALEDOWNAECVss - 1))


#define  IFFTSCALEUPAECVss             (0)
#define  IFFTSCALEDOWNAECVss           (CMPLXFFTPOWER256 + 1 + IFFTSCALEUPAECVss)

#if(IFFTSCALEDOWNAECVss)
  #define  IFFTDSCROUNDAECVss          (1 << (IFFTSCALEDOWNAECVss - 1))
#else
  #define  IFFTDSCROUNDAECVss          (0)
#endif
/*----------------------------------------------------------------------------------------*/

#elif defined AEC_OPTZ_FFT_128   //start  of 128 fft

/*------------------------------------------------------------------------------------
 *        Init data for Real fft size 128: for AEC
 *-----------------------------------------------------------------------------------*/
#define  FFT_OUT_SCALE                (2)

#define  FFTSCALEUPAEC_1              (5)
#define  FFTSCALEDOWNAEC_1            (FFTSCALEUPAEC_1 + FFT_OUT_SCALE)

#if(FFTSCALEDOWNAEC_1)
  #define  FFTDSCROUNDAEC_1           (1 << (FFTSCALEDOWNAEC_1 - 1))
#else
  #define  FFTDSCROUNDAEC_1           (0)
#endif

#define  REALFFTSCALEDOWNAEC_1        (FFTSCALEDOWNAEC_1 + 1)
#define  REALFFTSCALEDOWNRNDAEC_1     (1 <<  (REALFFTSCALEDOWNAEC_1 - 1))

#define  IFFTSCALEUPAEC_1             (0)
#define  IFFTSCALEDOWNAEC_1           (CMPLXFFTPOWER128 + 1 + IFFTSCALEUPAEC_1 - FFT_OUT_SCALE)

#if(IFFTSCALEDOWNAEC_1)
  #define  IFFTDSCROUNDAEC_1          (1 << (IFFTSCALEDOWNAEC_1 - 1))
#else
  #define  IFFTDSCROUNDAEC_1          (0)
#endif
/*---------------------------------------------------------------------------------*/
#define  FFTSCALEUPAEC_2              (0)
#define  FFTSCALEDOWNAEC_2            (FFTSCALEUPAEC_2 + FFT_OUT_SCALE)

#if(FFTSCALEDOWNAEC_2)
  #define  FFTDSCROUNDAEC_2           (1 << (FFTSCALEDOWNAEC_2 - 1))
#else
  #define  FFTDSCROUNDAEC_2           (0)
#endif

#define  REALFFTSCALEDOWNAEC_2        (FFTSCALEDOWNAEC_2 + 1)
#define  REALFFTSCALEDOWNRNDAEC_2     (1 << (REALFFTSCALEDOWNAEC_2 - 1))


#define  IFFTSCALEUPAEC_2             (4)
#define  IFFTSCALEDOWNAEC_2           (CMPLXFFTPOWER128 + 1 + IFFTSCALEUPAEC_2 - FFT_OUT_SCALE)

#if(IFFTSCALEDOWNAEC_2)
  #define  IFFTDSCROUNDAEC_2          (1 << (IFFTSCALEDOWNAEC_2 - 1))
#else
  #define  IFFTDSCROUNDAEC_2          (0)
#endif
/*-----------------------------------------------------------------------------------*/

#define  FFTSCALEUPAEC_3              (5)
#define  FFTSCALEDOWNAEC_3            (FFTSCALEUPAEC_3 + FFT_OUT_SCALE)

#if(FFTSCALEDOWNAEC_3)
  #define  FFTDSCROUNDAEC_3           (1 << (FFTSCALEDOWNAEC_3 - 1))
#else
  #define  FFTDSCROUNDAEC_3           (0)
#endif

#define  REALFFTSCALEDOWNAEC_3        (FFTSCALEDOWNAEC_3 + 1)
#define  REALFFTSCALEDOWNRNDAEC_3     (1 << (REALFFTSCALEDOWNAEC_3 - 1))


#define  IFFTSCALEUPAEC_3             (0)
#define  IFFTSCALEDOWNAEC_3           (CMPLXFFTPOWER128 + 1 + IFFTSCALEUPAEC_3 - FFT_OUT_SCALE)

#if(IFFTSCALEDOWNAEC_3)
  #define  IFFTDSCROUNDAEC_3          (1 << (IFFTSCALEDOWNAEC_3 - 1))
#else
  #define  IFFTDSCROUNDAEC_3          (0)
#endif

/*---------------------------------------------------------------------------------------*/
#define  REF_FFTSCALEUPAEC              (7)
#define  REF_FFTSCALEDOWNAEC            (REF_FFTSCALEUPAEC + FFT_OUT_SCALE)

#if(REF_FFTSCALEDOWNAEC)
  #define  REF_FFTDSCROUNDAEC           (1 << (REF_FFTSCALEDOWNAEC - 1))
#else
  #define  REF_FFTDSCROUNDAEC           (0)
#endif

#define  REF_REALFFTSCALEDOWNAEC        (REF_FFTSCALEDOWNAEC + 1)
#define  REF_REALFFTSCALEDOWNRNDAEC     (1 << (REF_REALFFTSCALEDOWNAEC - 1))


#define  REF_IFFTSCALEUPAEC             (0)
#define  REF_IFFTSCALEDOWNAEC           (CMPLXFFTPOWER128 + 1 + REF_IFFTSCALEUPAEC - FFT_OUT_SCALE)

#if(REF_IFFTSCALEDOWNAEC)
  #define  REF_IFFTDSCROUNDAEC          (1 << (REF_IFFTSCALEDOWNAEC - 1))
#else
  #define  REF_IFFTDSCROUNDAEC          (0)
#endif

/*---------------------------------------------------------------------------------------*/
#define  FFTSCALEUPAECVss              (5)
#define  FFTSCALEDOWNAECVss            (FFTSCALEUPAECVss)

#if(FFTSCALEDOWNAECVss)
  #define  FFTDSCROUNDAECVss           (1 << (FFTSCALEDOWNAECVss - 1))
#else
  #define  FFTDSCROUNDAECVss           (0)
#endif

#define  REALFFTSCALEDOWNAECVss        (FFTSCALEDOWNAECVss + 1)
#define  REALFFTSCALEDOWNRNDAECVss     (1 << (REALFFTSCALEDOWNAECVss - 1))


#define  IFFTSCALEUPAECVss             (0)
#define  IFFTSCALEDOWNAECVss           (CMPLXFFTPOWER128 + 1 + IFFTSCALEUPAECVss)

#if(IFFTSCALEDOWNAECVss)
  #define  IFFTDSCROUNDAECVss          (1 << (IFFTSCALEDOWNAECVss - 1))
#else
  #define  IFFTDSCROUNDAECVss          (0)
#endif
/*----------------------------------------------------------------------------------------*/
#elif defined AEC_OPTZ_FFT_64   //start  of 64 fft

/*------------------------------------------------------------------------------------
 *        Init data for Real fft size 64: for AEC
 *-----------------------------------------------------------------------------------*/
#define  FFT_OUT_SCALE                (2)

#define  FFTSCALEUPAEC_1              (5)
#define  FFTSCALEDOWNAEC_1            (FFTSCALEUPAEC_1 + FFT_OUT_SCALE)

#if(FFTSCALEDOWNAEC_1)
  #define  FFTDSCROUNDAEC_1           (1 << (FFTSCALEDOWNAEC_1 - 1))
#else
  #define  FFTDSCROUNDAEC_1           (0)
#endif

#define  REALFFTSCALEDOWNAEC_1        (FFTSCALEDOWNAEC_1 + 1)
#define  REALFFTSCALEDOWNRNDAEC_1     (1 <<  (REALFFTSCALEDOWNAEC_1 - 1))

#define  IFFTSCALEUPAEC_1             (0)
#define  IFFTSCALEDOWNAEC_1           (CMPLXFFTPOWER128 + 1 + IFFTSCALEUPAEC_1 - FFT_OUT_SCALE)

#if(IFFTSCALEDOWNAEC_1)
  #define  IFFTDSCROUNDAEC_1          (1 << (IFFTSCALEDOWNAEC_1 - 1))
#else
  #define  IFFTDSCROUNDAEC_1          (0)
#endif
/*---------------------------------------------------------------------------------*/
#define  FFTSCALEUPAEC_2              (0)
#define  FFTSCALEDOWNAEC_2            (FFTSCALEUPAEC_2 + FFT_OUT_SCALE)

#if(FFTSCALEDOWNAEC_2)
  #define  FFTDSCROUNDAEC_2           (1 << (FFTSCALEDOWNAEC_2 - 1))
#else
  #define  FFTDSCROUNDAEC_2           (0)
#endif

#define  REALFFTSCALEDOWNAEC_2        (FFTSCALEDOWNAEC_2 + 1)
#define  REALFFTSCALEDOWNRNDAEC_2     (1 << (REALFFTSCALEDOWNAEC_2 - 1))


#define  IFFTSCALEUPAEC_2             (4)
#define  IFFTSCALEDOWNAEC_2           (CMPLXFFTPOWER128 + 1 + IFFTSCALEUPAEC_2 - FFT_OUT_SCALE)

#if(IFFTSCALEDOWNAEC_2)
  #define  IFFTDSCROUNDAEC_2          (1 << (IFFTSCALEDOWNAEC_2 - 1))
#else
  #define  IFFTDSCROUNDAEC_2          (0)
#endif
/*-----------------------------------------------------------------------------------*/

#define  FFTSCALEUPAEC_3              (5)
#define  FFTSCALEDOWNAEC_3            (FFTSCALEUPAEC_3 + FFT_OUT_SCALE)

#if(FFTSCALEDOWNAEC_3)
  #define  FFTDSCROUNDAEC_3           (1 << (FFTSCALEDOWNAEC_3 - 1))
#else
  #define  FFTDSCROUNDAEC_3           (0)
#endif

#define  REALFFTSCALEDOWNAEC_3        (FFTSCALEDOWNAEC_3 + 1)
#define  REALFFTSCALEDOWNRNDAEC_3     (1 << (REALFFTSCALEDOWNAEC_3 - 1))


#define  IFFTSCALEUPAEC_3             (0)
#define  IFFTSCALEDOWNAEC_3           (CMPLXFFTPOWER128 + 1 + IFFTSCALEUPAEC_3 - FFT_OUT_SCALE)

#if(IFFTSCALEDOWNAEC_3)
  #define  IFFTDSCROUNDAEC_3          (1 << (IFFTSCALEDOWNAEC_3 - 1))
#else
  #define  IFFTDSCROUNDAEC_3          (0)
#endif

/*---------------------------------------------------------------------------------------*/
#define  REF_FFTSCALEUPAEC              (7)
#define  REF_FFTSCALEDOWNAEC            (REF_FFTSCALEUPAEC + FFT_OUT_SCALE)

#if(REF_FFTSCALEDOWNAEC)
  #define  REF_FFTDSCROUNDAEC           (1 << (REF_FFTSCALEDOWNAEC - 1))
#else
  #define  REF_FFTDSCROUNDAEC           (0)
#endif

#define  REF_REALFFTSCALEDOWNAEC        (REF_FFTSCALEDOWNAEC + 1)
#define  REF_REALFFTSCALEDOWNRNDAEC     (1 << (REF_REALFFTSCALEDOWNAEC - 1))


#define  REF_IFFTSCALEUPAEC             (0)
#define  REF_IFFTSCALEDOWNAEC           (CMPLXFFTPOWER128 + 1 + REF_IFFTSCALEUPAEC - FFT_OUT_SCALE)

#if(REF_IFFTSCALEDOWNAEC)
  #define  REF_IFFTDSCROUNDAEC          (1 << (REF_IFFTSCALEDOWNAEC - 1))
#else
  #define  REF_IFFTDSCROUNDAEC          (0)
#endif
#elif defined AEC_OPTZ_FFT_32   //start  of 32 fft

/*------------------------------------------------------------------------------------
 *        Init data for Real fft size 32: for AEC
 *-----------------------------------------------------------------------------------*/
#define  FFT_OUT_SCALE                (1)

#define  FFTSCALEUPAEC_1              (5)
#define  FFTSCALEDOWNAEC_1            (FFTSCALEUPAEC_1 + FFT_OUT_SCALE)

#if(FFTSCALEDOWNAEC_1)
  #define  FFTDSCROUNDAEC_1           (1 << (FFTSCALEDOWNAEC_1 - 1))
#else
  #define  FFTDSCROUNDAEC_1           (0)
#endif

#define  REALFFTSCALEDOWNAEC_1        (FFTSCALEDOWNAEC_1 + 1)
#define  REALFFTSCALEDOWNRNDAEC_1     (1 <<  (REALFFTSCALEDOWNAEC_1 - 1))

#define  IFFTSCALEUPAEC_1             (0)
#define  IFFTSCALEDOWNAEC_1           (CMPLXFFTPOWER128 + 1 + IFFTSCALEUPAEC_1 - FFT_OUT_SCALE)

#if(IFFTSCALEDOWNAEC_1)
  #define  IFFTDSCROUNDAEC_1          (1 << (IFFTSCALEDOWNAEC_1 - 1))
#else
  #define  IFFTDSCROUNDAEC_1          (0)
#endif
/*---------------------------------------------------------------------------------*/
#define  FFTSCALEUPAEC_2              (0)
#define  FFTSCALEDOWNAEC_2            (FFTSCALEUPAEC_2 + FFT_OUT_SCALE)

#if(FFTSCALEDOWNAEC_2)
  #define  FFTDSCROUNDAEC_2           (1 << (FFTSCALEDOWNAEC_2 - 1))
#else
  #define  FFTDSCROUNDAEC_2           (0)
#endif

#define  REALFFTSCALEDOWNAEC_2        (FFTSCALEDOWNAEC_2 + 1)
#define  REALFFTSCALEDOWNRNDAEC_2     (1 << (REALFFTSCALEDOWNAEC_2 - 1))


#define  IFFTSCALEUPAEC_2             (4)
#define  IFFTSCALEDOWNAEC_2           (CMPLXFFTPOWER128 + 1 + IFFTSCALEUPAEC_2 - FFT_OUT_SCALE)

#if(IFFTSCALEDOWNAEC_2)
  #define  IFFTDSCROUNDAEC_2          (1 << (IFFTSCALEDOWNAEC_2 - 1))
#else
  #define  IFFTDSCROUNDAEC_2          (0)
#endif
/*-----------------------------------------------------------------------------------*/

#define  FFTSCALEUPAEC_3              (5)
#define  FFTSCALEDOWNAEC_3            (FFTSCALEUPAEC_3 + FFT_OUT_SCALE)

#if(FFTSCALEDOWNAEC_3)
  #define  FFTDSCROUNDAEC_3           (1 << (FFTSCALEDOWNAEC_3 - 1))
#else
  #define  FFTDSCROUNDAEC_3           (0)
#endif

#define  REALFFTSCALEDOWNAEC_3        (FFTSCALEDOWNAEC_3 + 1)
#define  REALFFTSCALEDOWNRNDAEC_3     (1 << (REALFFTSCALEDOWNAEC_3 - 1))


#define  IFFTSCALEUPAEC_3             (0)
#define  IFFTSCALEDOWNAEC_3           (CMPLXFFTPOWER128 + 1 + IFFTSCALEUPAEC_3 - FFT_OUT_SCALE)

#if(IFFTSCALEDOWNAEC_3)
  #define  IFFTDSCROUNDAEC_3          (1 << (IFFTSCALEDOWNAEC_3 - 1))
#else
  #define  IFFTDSCROUNDAEC_3          (0)
#endif

/*---------------------------------------------------------------------------------------*/
#define  REF_FFTSCALEUPAEC              (7)
#define  REF_FFTSCALEDOWNAEC            (REF_FFTSCALEUPAEC + FFT_OUT_SCALE)

#if(REF_FFTSCALEDOWNAEC)
  #define  REF_FFTDSCROUNDAEC           (1 << (REF_FFTSCALEDOWNAEC - 1))
#else
  #define  REF_FFTDSCROUNDAEC           (0)
#endif

#define  REF_REALFFTSCALEDOWNAEC        (REF_FFTSCALEDOWNAEC + 1)
#define  REF_REALFFTSCALEDOWNRNDAEC     (1 << (REF_REALFFTSCALEDOWNAEC - 1))


#define  REF_IFFTSCALEUPAEC             (0)
#define  REF_IFFTSCALEDOWNAEC           (CMPLXFFTPOWER128 + 1 + REF_IFFTSCALEUPAEC - FFT_OUT_SCALE)

#if(REF_IFFTSCALEDOWNAEC)
  #define  REF_IFFTDSCROUNDAEC          (1 << (REF_IFFTSCALEDOWNAEC - 1))
#else
  #define  REF_IFFTDSCROUNDAEC          (0)
#endif
/*---------------------------------------------------------------------------------*/
#endif

extern const       Scmplx FFT256_TwiddleTable[];
extern const       Word32 FFT256_DrTable[];
extern const       Scmplx FFT128_TwiddleTable[];
extern const       Word32 FFT128_DrTable[];
extern const       Scmplx FFT64_TwiddleTable[];
extern const       Word32 FFT64_DrTable[];
extern const       Scmplx FFT32_TwiddleTable[];
extern const       Word32 FFT32_DrTable[];
/*----------------------------------------------------------------------------*/
extern void CmplxMixedRadix4fft(Lcmplx * px,Lcmplx * data_out,FFT_PARAMS * fft_info,Word32 fft_type);

#if defined (USE_75PERCENT_OVERLAP_WINDOW)
#ifdef  STFT_AEC_OPTZ_FFT_256                     // 512 real STFT
#define  STFTSCALEUP                        (5)
#define  STFTSCALEDOWN                      (STFTSCALEUP)
#if(STFTSCALEDOWN)
  #define  STFTDSCROUND                     (1 <<  (STFTSCALEDOWN - 1))
#else
  #define  STFTDSCROUND                     (0)
#endif
#define  REALSTFTSCALEDOWN                  (STFTSCALEDOWN + 1)
#define  REALSTFTSCALEDOWNRND               (1 <<  (STFTSCALEDOWN - 1))
#define  STIFFTSCALEUP                      (0)
#define  STIFFTSCALEDOWN                    (CMPLXFFTPOWER512 + 1)
#define  STIFFTDSCROUND                     (1 <<  (STIFFTSCALEDOWN - 1))
#elif defined STFT_AEC_OPTZ_FFT_128              //256 real STFT
#define  STFTSCALEUP                        (6)
#define  STFTSCALEDOWN                      (STFTSCALEUP)
#if(STFTSCALEDOWN)
  #define  STFTDSCROUND                     (1 <<  (STFTSCALEDOWN - 1))
#else
  #define  STFTDSCROUND                     (0)
#endif
#define  REALSTFTSCALEDOWN                  (STFTSCALEDOWN + 1)
#define  REALSTFTSCALEDOWNRND               (1 <<  (STFTSCALEDOWN - 1))
#define  STIFFTSCALEUP                      (0)
#define  STIFFTSCALEDOWN                    (CMPLXFFTPOWER256 + 1)
#define  STIFFTDSCROUND                     (1 <<  (STIFFTSCALEDOWN - 1))
#endif
#else // 50% OVERLAP
#define  STFTSCALEUP                        (6)
#define  STFTSCALEDOWN                      (STFTSCALEUP)
#if(STFTSCALEDOWN)
  #define  STFTDSCROUND                     (1 <<  (STFTSCALEDOWN - 1))
#else
  #define  STFTDSCROUND                     (0)
#endif
#define  REALSTFTSCALEDOWN                  (STFTSCALEDOWN + 1)
#define  REALSTFTSCALEDOWNRND               (1 <<  (STFTSCALEDOWN - 1))
#define  STIFFTSCALEUP                      (0)
#define  STIFFTSCALEDOWN                    (CMPLXFFTPOWER256 + 1)
#define  STIFFTDSCROUND                     (1 <<  (STIFFTSCALEDOWN - 1))
#endif

extern void CmplxMixedRadix4_ConsParam(Lcmplx * px,Lcmplx * data_out,FFT_PARAMS * fft_info,Word32 fft_type);
/*----------------------------------------------------------------------------------------------------------------*/



#ifdef __cplusplus
}
#endif

#endif
