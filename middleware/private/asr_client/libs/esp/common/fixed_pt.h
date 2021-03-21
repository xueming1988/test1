/*
 * Copyright 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * You may not use this file except in compliance with the terms and conditions set forth in the accompanying LICENSE.TXT file.
 *
 * THESE MATERIALS ARE PROVIDED ON AN "AS IS" BASIS.  AMAZON SPECIFICALLY DISCLAIMS, WITH RESPECT TO THESE MATERIALS, ALL
 * WARRANTIES, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, AND NON-INFRINGEMENT.
 */

#ifndef FIXED_PT_H
#define FIXED_PT_H

#define                         REALFFT      (1)
#define                         REALIFFT     (0)

typedef  float                  REAL;
typedef  short                  Word16;
typedef  int                    Word32;
typedef  long long              Word64;
typedef  unsigned short         UWord16;
typedef  unsigned int           UWord32;
typedef  unsigned long long     UWord64;

#define  MAX_16                 (Word16)(0x7fff)
#define  MIN_16                 (Word16)(0x8000)
#define  MAX_32                 (Word32)(0x7fffffff)
#define  MIN_32                 (Word32)(0x80000000)

#ifdef ti_targets_C64P      // long is 40bits for TI DSP
    typedef long                Word40;
    typedef unsigned long       UWord40;
#else
    typedef long long           Word40;
    typedef unsigned long long  UWord40;
#endif

#define   MAX(X,Y)              (((X) > (Y) ? (X) : (Y)))
#define   MIN(X,Y)              (((X) < (Y) ? (X) : (Y)))
#define   SAT16s(X)             ((X) > MAX_16 ? MAX_16 : ((X) < MIN_16 ? MIN_16 : (X)))
#define   SAT32s(X)             ((X) > MAX_32 ? MAX_32 : ((X) < MIN_32 ? MIN_32 : (X)))

#define   HIGH16(x)             (( Word16)((x) >> 16))
#define   LOW16(x)              (( Word16)((x) & 0xffff))
#define   ULOW16(x)             ((uWord16)((x) & 0xffff))

#define   SHR15LSBs             (15)
#define   RND15LSBs             (1 << 14)

typedef  struct
{
  Word32 r;
  Word32 i;
}Lcmplx;

typedef  struct
{
  Word16 r;
  Word16 i;
}Scmplx;

#define _mpylir      _mpylir_c
#define _norm        _norm_c
#define _mpy         _mpy_c
#define _mpy32ll     _mpy32ll_c

/*-----------------------------------------------------------------------------
 * Produces the number of left shift needed to normalize the 32 bit variable.
 *-----------------------------------------------------------------------------*/
Word16    _norm_c(Word32 x);
/*------------------------------------------------------------------------------
 *  Fixed point Sqrt
 *------------------------------------------------------------------------------*/
UWord16   Sqrt_Func(UWord32 x);
/*-----------------------------------------------------------------------------
 *  Multiply   Signed 16 LSB (src1)  Signed   16 LSB (src2)
 *----------------------------------------------------------------------------*/
Word32   _mpy_c(int src1, int src2);
/*-----------------------------------------------------------------------------
 *  Performs a 16-bit by 32-bit multiply.
 *----------------------------------------------------------------------------*/
Word32   _mpylir_c(Word32 src1, Word32 src2);
/*-----------------------------------------------------------------------------
 *  Produces a 32 by 32 multiply with a 64-bit result; with signed and unsigned
 *  input parameters
 *----------------------------------------------------------------------------*/
Word64   _mpy32ll_c(Word32 src1, Word32 src2);

#endif // FIXED_PT_H
