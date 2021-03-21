/*
 * Copyright 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * You may not use this file except in compliance with the terms and conditions set forth in the accompanying LICENSE.TXT file.
 *
 * THESE MATERIALS ARE PROVIDED ON AN "AS IS" BASIS.  AMAZON SPECIFICALLY DISCLAIMS, WITH RESPECT TO THESE MATERIALS, ALL
 * WARRANTIES, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, AND NON-INFRINGEMENT.
 */

#include <iostream>
#include <math.h>
#include "./fixed_pt.h"

using namespace std;

/*------------------------------------------------------------------------------------*/
#define MAX_SQRT_INPUT_THR      (UWord32)(MAX_16*MAX_16)

/*---------------------------------------------------------------------------
 |                                                                           |
 |   Function Name : norm_l                                                  |
 |   Produces the number of left shift needed to normalize the 32 bit varia- |
 |   ble l_var1 for positive values on the interval with minimum of          |
 |   1073741824 and maximum of 2147483647, and for negative values on the in-|
 |   terval with minimum of -2147483648 and maximum of -1073741824; in order |
 |   to normalize the result, the following operation must be done :         |
 |   Inputs :                                                                |
 |                                                                           |
 |    L_var1                                                                 |
 |             32 bit long signed integer (Word32) whose value falls in the  |
 |             range : 0x8000 0000 <= var1 <= 0x7fff ffff.                   |
 |                                                                           |
 |    var_out                                                                |
 |             Returns 0 if the argument is 0, or 31 if the argument is -1   |
 ----------------------------------------------------------------------------*/

Word16 _norm_c(Word32 x)
{
    Word16 q;
    Word32 p;

    p = x ^ (x >> 31);

    if(p == 0)
    {
        /* x was 0 or -1 */
       q = (x == 0) ? 0 : 31;
    }
    else
    {
       q = 0;

       while(p < 0x40000000)
       {
         p <<= 1;
         q++;
       }
    }

    return q;
}

/***********************************************************************
 *
 *  Function unsigned short Sqrt_Function(UWord32 x)
 *
 *  Input  :  non-negative integer number x < (2^32 - 2^17).
 *  Output :  Sqware root of x from 0 to 65534.
 **********************************************************************/
#define  SQRT_TABLE_SIZE        (10)
const Word16 Table_Sqrt[SQRT_TABLE_SIZE]=
    {-23697,1562,-12720,23126,23171,
      -1784,8051,-11014,23136,23171};

UWord16 Sqrt_Func(UWord32 x)
{
    Word32 y,z,n2,n,i;
    Word16 s0,s1,s2,s3,s4;
    const Word16 * ptrTsqr;
    UWord32 x1,exp;
    Word32  x2;

    if(x >= MAX_SQRT_INPUT_THR)
    {
       x >>= 2;
       exp = 1;
    }
    else
    {
       exp = 0;
    }

    if(x == 0)
    {
        y = 0;
    }
    else
    {
        x1 = (x << 1);
        n  = (_norm(x1) >> 1);
        n2 =  n + n;
        x1 = (x1 << n2);
        z  =  HIGH16(x1)-0x4000;

        if(z < 0)
        {
            i = 0;
        }
        else
        {
            i = 5;
        }

        ptrTsqr = &Table_Sqrt[i];
        s0 = *ptrTsqr++;   //Table_Sqrt[i]
        s1 = *ptrTsqr++;   //Table_Sqrt[i+1]
        s2 = *ptrTsqr++;   //Table_Sqrt[i+2]
        s3 = *ptrTsqr++;   //Table_Sqrt[i+3]
        s4 = *ptrTsqr++;   //Table_Sqrt[i+4]

        y = s1 + _mpylir(s0,(z << 1));//MPY(s0,z,14);
        y = s2 + _mpylir(y,z);      //MPY(y,z,15);
        y = s3 + _mpylir(y,z);      //MPY(y,z,15);
        y = s4 + _mpylir(y,z);      //MPY(y,z,15);
        y >>= n;

        x2 = x - _mpy(y,y);
        if(x2 > y)
        {
           y += 1;
        }
        else if(x2 < -y)
        {
           y -= 1;
        }
    }

    return ((unsigned short)y << exp);
}

/*-----------------------------------------------------------------------------
 *  MPY:       Multiply   Signed 16 LSB  Signed   16 LSB
 *----------------------------------------------------------------------------*/
Word32   _mpy_c(int src1, int src2)
{
    Word32 tmp;

    tmp = LOW16(src1) * LOW16(src2);

    return tmp;
}

/*---------------------------------------------------------------------------------------------
 * Performs a 16-bit by 32-bit multiply. The lower half of src1 is treated as a signed 16-bit
 * input. The value in src2 is treated as a signed 32-bit value. The product is then rounded
 * into a 32-bit result by adding the value 2^14 and then this sum is right shifted by 15. The
 * lower 32 bits of the result are written into dst.
 *
 * Z    = ((src1_Low * src2 + 2^14) >> 15);
 * int _mpylir(int src1, int src2);     MPYLIR
 *--------------------------------------------------------------------------------------------*/
 Word32 _mpylir_c(Word32 src1, Word32 src2)
 {
   return (Word32)(((Word16)src1 * (Word64)src2 + RND15LSBs) >> SHR15LSBs);
 }

 /*--------------------------------------------------------------------------------------
  * Produces a 32 by 32 multiply with a 64-bit result. The inputs and outputs can be signed or unsigned.
  * long long _mpy32ll(int  src1, int  src2);   MPY32
  *----------------------------------------------------------------------------------------------*/
 Word64 _mpy32ll_c(Word32 src1, Word32 src2)
 {
     Word64 PW64;

     PW64 = (((Word64)src1) * src2);

     return PW64;
 }
