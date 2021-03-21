/*
 * Copyright 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * You may not use this file except in compliance with the terms and conditions set forth in the accompanying LICENSE.TXT file.
 *
 * THESE MATERIALS ARE PROVIDED ON AN "AS IS" BASIS.  AMAZON SPECIFICALLY DISCLAIMS, WITH RESPECT TO THESE MATERIALS, ALL
 * WARRANTIES, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, AND NON-INFRINGEMENT.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "./Radix4fft.h"

const Scmplx FFT256_TwiddleTable[TWIDDLETABLESIZE256] = {
    {32767, 0},{32767, 0},{32767, 0},{32729, -1608},{32758, -804},{32679, -2410},
    {32610, -3212},{32729, -1608},{32413, -4808},{32413, -4808},{32679, -2410},{31972, -7179},
    {32138, -6392},{32610, -3212},{31357, -9512},{31786, -7962},{32522, -4011},{30572, -11793},
    {31357, -9512},{32413, -4808},{29622, -14010},{30853, -11039},{32286, -5602},{28511, -16151},
    {30274, -12540},{32138, -6392},{27246, -18205},{29622, -14010},{31972, -7179},{25833, -20160},
    {28899, -15446},{31786, -7962},{24279, -22005},{28106, -16846},{31581, -8739},{22595, -23732},
    {27246, -18205},{31357, -9512},{20788, -25330},{26320, -19520},{31114, -10278},{18868, -26790},
    {25330, -20788},{30853, -11039},{16846, -28106},{24279, -22005},{30572, -11793},{14733, -29269},
    {23170, -23170},{30274, -12540},{12540, -30273},{22006, -24279},{29957, -13279},{10279, -31114},
    {20788, -25330},{29622, -14010},{7962, -31786},{19520, -26319},{29269, -14733},{5602, -32285},
    {18205, -27245},{28899, -15446},{3212, -32610},{16846, -28106},{28511, -16151},{804, -32758},
    {15447, -28899},{28106, -16846},{-1608, -32728},{14010, -29622},{27684, -17531},{-4011, -32521},
    {12540, -30273},{27246, -18205},{-6392, -32138},{11039, -30852},{26791, -18868},{-8739, -31581},
    {9512, -31357},{26320, -19520},{-11039, -30852},{7962, -31786},{25833, -20160},{-13279, -29957},
    {6393, -32138},{25330, -20788},{-15446, -28899},{4808, -32413},{24812, -21403},{-17531, -27684},
    {3212, -32610},{24279, -22005},{-19520, -26319},{1608, -32728},{23732, -22595},{-21403, -24812},
    {0, -32768},{23170, -23170},{-23170, -23170},{-1608, -32728},{22595, -23732},{-24812, -21403},
    {-3212, -32610},{22006, -24279},{-26319, -19520},{-4808, -32413},{21403, -24812},{-27684, -17531},
    {-6392, -32138},{20788, -25330},{-28899, -15446},{-7962, -31786},{20160, -25832},{-29957, -13279},
    {-9512, -31357},{19520, -26319},{-30852, -11039},{-11039, -30852},{18868, -26790},{-31581, -8739},
    {-12540, -30273},{18205, -27245},{-32138, -6392},{-14010, -29622},{17531, -27684},{-32521, -4011},
    {-15446, -28899},{16846, -28106},{-32728, -1608},{-16846, -28106},{16151, -28511},{-32758, 804},
    {-18205, -27245},{15447, -28899},{-32610, 3212},{-19520, -26319},{14733, -29269},{-32285, 5602},
    {-20788, -25330},{14010, -29622},{-31786, 7962},{-22005, -24279},{13279, -29957},{-31114, 10279},
    {-23170, -23170},{12540, -30273},{-30273, 12540},{-24279, -22005},{11793, -30572},{-29269, 14733},
    {-25330, -20788},{11039, -30852},{-28106, 16846},{-26319, -19520},{10279, -31114},{-26790, 18868},
    {-27245, -18205},{9512, -31357},{-25330, 20788},{-28106, -16846},{8740, -31581},{-23732, 22595},
    {-28899, -15446},{7962, -31786},{-22005, 24279},{-29622, -14010},{7180, -31972},{-20160, 25833},
    {-30273, -12540},{6393, -32138},{-18205, 27246},{-30852, -11039},{5602, -32285},{-16151, 28511},
    {-31357, -9512},{4808, -32413},{-14010, 29622},{-31786, -7962},{4011, -32521},{-11793, 30572},
    {-32138, -6392},{3212, -32610},{-9512, 31357},{-32413, -4808},{2411, -32679},{-7179, 31972},
    {-32610, -3212},{1608, -32728},{-4808, 32413},{-32728, -1608},{804, -32758},{-2410, 32679},
    {0, 0},{0, 0},{0, 0},{0, 0},{0, 0},{0, 0},
};

const  Word32  FFT256_DrTable[DRTABLESIZE256] = {
    0, 64, 32, 96, 16, 80, 48, 112, 8, 72, 40, 104, 24, 88, 56, 120, 4, 68, 36, 100, 20,
    84, 52, 116, 12, 76, 44, 108, 28, 92, 60, 124, 2, 66, 34, 98, 18, 82, 50, 114, 10, 74,
    42, 106, 26, 90, 58, 122, 6, 70, 38, 102, 22, 86, 54, 118, 14, 78, 46, 110, 30, 94, 62,
    126, 1, 65, 33, 97, 17, 81, 49, 113, 9, 73, 41, 105, 25, 89, 57, 121, 5, 69, 37, 101,
    21, 85, 53, 117, 13, 77, 45, 109, 29, 93, 61, 125, 3, 67, 35, 99, 19, 83, 51, 115, 11,
    75, 43, 107, 27, 91, 59, 123, 7, 71, 39, 103, 23, 87, 55, 119, 15, 79, 47, 111, 31, 95,
    63, 127,
};



void CmplxMixedRadix4fft(Lcmplx * px,Lcmplx* data_out,FFT_PARAMS * pr_fft_info,Word32 fft_type)
{ 
   Word32 cmplxlength,BlkSize,Index,Step,CmplxPow;
   Word32 diffpow,NL,N,n1,n2,k,n,row,tidx;
   Lcmplx * pBlock,* pElem;
   Word32 Stage,NumBlocks,NewBlkSize;
   Word32 r1p,r2p,r1m,r2m,r_re,r_im;
   Lcmplx z0,z1,z2,z3,inp0,inp1,inp2,inp3; 
   Lcmplx tmp0,tmp1,tmp2,tmp3;
   Scmplx W0,W1,W2;
   const  Scmplx * pW;
   const  Word32 * pDrTbl;

   pW          =  pr_fft_info->pTwiddleTable;
   CmplxPow    =  pr_fft_info->CmplxFFTPower;
   diffpow     =  pr_fft_info->MaxFFTPower - CmplxPow;

   cmplxlength = (1 << CmplxPow);
   N           =  cmplxlength;
   NL          = (N >> 1);

   if (fft_type == REALIFFT)
   {                            //Inverse fft
    Step = (3 << (diffpow-1));      
   
    Index = 1;
    for(k = 1; k < NL; k++)
    {
        Index  += Step;
        W0      = pW[Index];
        inp0    = px[k];                   //X[k]  
        inp1    = px[N-k];                //X[N-k]
        inp0.i  =-inp0.i;               //inp0=conj(X[k]

        inp2.r  = inp1.i - inp0.i;     //j(conj(X[k])-X[N-k])
        inp2.i  = inp0.r - inp1.r;

        //  (conj(X[k])-X[N-k])*factor[0]
        r_re  = (inp2.r >> 15)   *W0.r-(inp2.i >> 15) *W0.i +
               (((inp2.r&0x7fff)*W0.r-(inp2.i&0x7fff)*W0.i) >> 15);

        r_im  = (inp2.r >> 15)   *W0.i+(inp2.i >> 15) *W0.r +
               (((inp2.r&0x7fff)*W0.i+(inp2.i&0x7fff)*W0.r) >> 15);

        r1p    = inp0.r + inp1.r;
        r2p    = inp0.i + inp1.i;

        tmp1.r = r1p    + r_re;    //a + b
        tmp2.r = r1p    - r_re;    //a - b

        tmp1.i = r_im   + r2p;       //a + b
        tmp2.i = r_im   - r2p;       //a - b

        px[k]  = tmp1;
        px[N-k]= tmp2;
    }

//Special case k=0
     inp2    =  px[0];  
     inp3    =  px[NL]; // Special case k=N/2

     tmp1.r  =  inp2.r + inp2.i;  //a + b 
     tmp1.i  =  inp2.r - inp2.i;  //a - b
     px[0]   =  tmp1;
     
     tmp2.r  = (  inp3.r  << 1); 
     tmp2.i  = ((-inp3.i) << 1);
     px[NL]  =    tmp2;
   }

   for(Stage = 0; Stage < (CmplxPow-1); Stage += 2)         
   {
     NumBlocks = (1 << Stage);             //Mumber of Blocks   
     BlkSize   = (cmplxlength >> Stage);   //Size of blocks  
     NewBlkSize= (BlkSize >> 2);           //Size of resulting Blocks
     pBlock    =  px;                      //points to blocks at stage  

    //   calculations for first row=0
    for(k = 0; k < NumBlocks; k++)
    {
        pElem = pBlock;
        Index = 0;

        inp0    = pElem[Index];
        Index  += NewBlkSize;   
         
        inp1    = pElem[Index];
        Index  += NewBlkSize;   

        inp2    = pElem[Index];
        Index  += NewBlkSize; 

        inp3    = pElem[Index];
/*---------------------------------------------------------------------*/
        tmp0.r = inp0.r + inp2.r;       //a + b  
        tmp2.r = inp0.r - inp2.r;       //a - b
/*---------------------------------------------------------------------*/
        tmp0.i = inp0.i + inp2.i;       //a + b
        tmp2.i = inp0.i - inp2.i;       //a - b
/*---------------------------------------------------------------------*/
        tmp3.r = inp1.r + inp3.r;      //a + b
        inp0.i = inp1.r - inp3.r;      //a - b
/*---------------------------------------------------------------------*/
        tmp3.i = inp3.i + inp1.i;      //a + b      
        inp0.r = inp3.i - inp1.i;      //a - b
/*---------------------------------------------------------------------*/
        pElem    = pBlock;
        tidx       = 0;
/*---------------------------------------------------------------------*/
        z1.r   = tmp0.r - tmp3.r;     //a + b        
        z0.r   = tmp0.r + tmp3.r;     //a - b                 
/*---------------------------------------------------------------------*/
        z1.i   = tmp0.i - tmp3.i;     //a + b      
        z0.i   = tmp0.i + tmp3.i;     //a - b

        pElem[tidx]= z0;
        tidx      += NewBlkSize;

        pElem[tidx]= z1;
        tidx      += NewBlkSize;
/*---------------------------------------------------------------------*/
        z3.r   = tmp2.r + inp0.r;    //a + b        
        z2.r   = tmp2.r - inp0.r;    //a - b
/*---------------------------------------------------------------------*/
        z3.i   = tmp2.i + inp0.i;    //a + b  
        z2.i   = tmp2.i - inp0.i;    //a - b
/*---------------------------------------------------------------------*/
        pElem[tidx]= z2;
        tidx      += NewBlkSize;

        pElem[tidx]= z3;
        pBlock    += BlkSize;
    }

    Step  =  (3 << (diffpow + Stage));
    Index =   0;

    for(row = 1; row < NewBlkSize; row++)
    {
        Index += Step;
        tidx   = Index;

        W0  = pW[tidx++];
        W1  = pW[tidx++];
        W2  = pW[tidx];

        pBlock = px;	   
       
        for(k = 0; k < NumBlocks; k++)
        {
            pElem = pBlock + row;

            tidx   = 0;
            inp0   = pElem[tidx];
            tidx  += NewBlkSize;

            inp1  = pElem[tidx];
            tidx += NewBlkSize;

            inp2  = pElem[tidx];
            tidx += NewBlkSize;

            inp3 = pElem[tidx];

            tmp0.r = inp0.r + inp2.r;       //a+b
            tmp2.r = inp0.r - inp2.r;       //a-b

            tmp0.i = inp0.i + inp2.i;       //a+b
            tmp2.i = inp0.i - inp2.i;       //a-b

            tmp3.r = inp1.r + inp3.r; 
            tmp3.i = inp1.i + inp3.i;

            inp0.r = inp3.i  - inp1.i; 
            inp0.i = inp1.r  - inp3.r;
    /*-------------------------------------------------------*/	 
            z0.r   = tmp0.r   + tmp3.r;        //a+b 
            z1.r   = tmp0.r   - tmp3.r;        //a-b

            z0.i   = tmp0.i   + tmp3.i;        //a+b 
            z1.i   = tmp0.i   - tmp3.i;        //a-b

            z3.r   = tmp2.r + inp0.r;  //a+b
            z2.r   = tmp2.r - inp0.r;  //a-b

            z3.i   = tmp2.i + inp0.i;  //a+b
            z2.i   = tmp2.i - inp0.i;  //a-b

 //Multiplication with Twiddle factors and shifts
            pElem       = pBlock + row;
            tidx        = 0;
            pElem[tidx] = z0;
            tidx       += NewBlkSize;
  /*--------------------------------------------------------------------------*/
            tmp0.r= (z1.r >> 15)   *W0.r-(z1.i >> 15) *W0.i + 
                (((z1.r&0x7fff)*W0.r-(z1.i&0x7fff)*W0.i) >> 15);
                 tmp0.i= (z1.r >> 15)   *W0.i+(z1.i>>15)   *W0.r +
                (((z1.r&0x7fff)*W0.i+(z1.i&0x7fff)*W0.r) >> 15); 

            pElem[tidx] = tmp0;
            tidx       += NewBlkSize;
  /*--------------------------------------------------------------------------*/  
            tmp1.r= (z2.r >> 15)   *W1.r-(z2.i >> 15) *W1.i + 
                (((z2.r&0x7fff)*W1.r-(z2.i&0x7fff)*W1.i) >> 15);
                 tmp1.i= (z2.r >> 15)   *W1.i+(z2.i >> 15) *W1.r +
                (((z2.r&0x7fff)*W1.i+(z2.i&0x7fff)*W1.r) >> 15); 

                 pElem[tidx] = tmp1;
                 tidx       += NewBlkSize;
  /*--------------------------------------------------------------------------*/  
            tmp2.r= (z3.r >> 15)   *W2.r-(z3.i >> 15) *W2.i + 
                (((z3.r&0x7fff)*W2.r-(z3.i&0x7fff)*W2.i) >> 15);
                 tmp2.i= (z3.r >> 15)   *W2.i+(z3.i >> 15) *W2.r +
                (((z3.r&0x7fff)*W2.i+(z3.i&0x7fff)*W2.r) >> 15); 

                 pElem[tidx] = tmp2;
  /*--------------------------------------------------------------------------*/ 

        pBlock += BlkSize;
        }
    }
   }

   if(Stage < CmplxPow)
   {
        NumBlocks = (1 << Stage);            //Mumber of Blocks   
         pBlock = px;                         //points to blocks at stage

         for(k = 0; k < NumBlocks; k++)
        {
            inp0 = pBlock[0];
            inp1 = pBlock[1];    
   /*----------------------------------------------------------------*/       
            z0.r = inp0.r + inp1.r;       //a+b  
            z1.r = inp0.r - inp1.r;       //a-b

            z0.i = inp0.i + inp1.i;       //a+b
            z1.i = inp0.i - inp1.i;       //a-b
    /*----------------------------------------------------------------*/      
            pBlock[0] = z0;
            pBlock[1] = z1;
            pBlock   += 2;
        }
   }

   pDrTbl      =  pr_fft_info->pDrTbl;
  
   if(fft_type == REALFFT)
   {
      Step = (3 << (diffpow - 1));      
      Index = 1;
          
      inp0   =  px[0];
      inp1   =  px[1];

      tmp2.r = inp0.r + inp0.i;  //a+b 
      tmp2.i = inp0.r - inp0.i;  //a-b
      
      inp2.r = ((tmp2.r + pr_fft_info->FFTDScRound) >> pr_fft_info->FFTScaleDown);
      inp2.i = ((tmp2.i + pr_fft_info->FFTDScRound) >> pr_fft_info->FFTScaleDown);
      data_out[0] =  inp2;

      tmp1.r =  inp1.r;
      tmp1.i = -inp1.i;

      inp1.r =   ((tmp1.r + pr_fft_info->FFTDScRound) >> pr_fft_info->FFTScaleDown);
      inp1.i =  -((tmp1.i + pr_fft_info->FFTDScRound) >> pr_fft_info->FFTScaleDown);     //To create conlex conj
      data_out[NL]=  inp1;
      
      for(k = 1; k < NL; k++)
      {
        Index += Step;
        W0     = pW[Index];

        n1     =   pDrTbl[k];
        n2     =   pDrTbl[N-k];

        inp0   = px[n1];     
        inp3   = px[n2];      

        inp1.r = inp3.r + inp0.r;   // a + b
        inp2.i = inp3.r - inp0.r;   // a - b
        
        inp2.r = inp0.i + inp3.i;   // a + b
        inp1.i = inp0.i - inp3.i;   // a - b

        r_re = (inp2.r >> 15) *W0.r-(inp2.i >> 15) *W0.i+
             (((inp2.r&0x7fff)*W0.r-(inp2.i&0x7fff)*W0.i) >> 15);

        r_im =(inp2.r >> 15) *W0.i+(inp2.i >> 15) *W0.r+
            (((inp2.r&0x7fff)*W0.i+(inp2.i&0x7fff)*W0.r) >> 15);

        r1p    = inp1.r + r_re;   //a + b
        r1m    = inp1.r - r_re;   //a - b

        r2p    = r_im   + inp1.i; //a + b
        r2m    = r_im   - inp1.i; //a - b

        inp2.r = (( r1p + pr_fft_info->RealFFTScaleDownRnd) >> pr_fft_info->RealFFTScaleDown);
        inp3.r = (( r1m + pr_fft_info->RealFFTScaleDownRnd) >> pr_fft_info->RealFFTScaleDown);

        inp2.i = -((r2p + pr_fft_info->RealFFTScaleDownRnd)  >> pr_fft_info->RealFFTScaleDown);  //To create conlex conj
        inp3.i = -((r2m + pr_fft_info->RealFFTScaleDownRnd)  >> pr_fft_info->RealFFTScaleDown);  //To create conlex conj

        data_out[k]   =  inp2;
        data_out[N-k] =  inp3;
     }
  }
  else
  {
        tmp0        = px[0];
        tmp1.r      = ((tmp0.r + pr_fft_info->IFFTDScRound) >> pr_fft_info->IFFTScaleDown);
        tmp1.i      = ((tmp0.i + pr_fft_info->IFFTDScRound) >> pr_fft_info->IFFTScaleDown);

        data_out[0] = tmp1;

        for(n1 = 1; n1 < N; n1++)
        {
          n           = pDrTbl[n1];
          tmp0        = px[n];
          tmp1.r      = ((tmp0.r + pr_fft_info->IFFTDScRound) >> pr_fft_info->IFFTScaleDown);
          tmp1.i      = ((tmp0.i + pr_fft_info->IFFTDScRound) >> pr_fft_info->IFFTScaleDown);

          data_out[n1] = tmp1; 
        }
  }

  return;
}


