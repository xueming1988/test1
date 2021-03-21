/******************************************************************************

  Copyright (c) 2012 Intel Corporation 
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:
  
   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
  
   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.
  
   3. Neither the name of the Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived from 
      this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

#ifndef PLATFORM_HPP
#define PLATFORM_HPP

#include <stdint.h>
#include <time.h>

/**@file*/

#define PLAT_strncpy( dest, src, max ) strncpy( dest, src, max+1 ) /*!< Provides strncpy */
#define PLAT_snprintf(...) snprintf( __VA_ARGS__ )	/*!< Provides snprintf*/

/**
 * @brief  Converts the unsigned short integer hostshort
 * from host byte order to network byte order.
 * @param s short host byte order
 * @return short value in network order
 */
uint16_t PLAT_htons( uint16_t s );

/**
 * @brief  Converts the unsigned integer hostlong
 * from host byte order to network byte order.
 * @param  l Host long byte order
 * @return value in network byte order
 */
uint32_t PLAT_htonl( uint32_t l );

/**
 * @brief  Converts the unsigned short integer netshort
 * from network byte order to host byte order.
 * @param s Network order short integer
 * @return host order value
 */
uint16_t PLAT_ntohs( uint16_t s );

/**
 * @brief  Converts the unsigned integer netlong
 * from network byte order to host byte order.
 * @param l Long value in network order
 * @return Long value on host byte order
 */
uint32_t PLAT_ntohl( uint32_t l );

/**
 * @brief  Converts a 64-bit word from host to network order
 * @param  x Value to be converted
 * @return Converted value
 */
uint64_t PLAT_htonll(uint64_t x);

/**
 * @brief  Converts a 64 bit word from network to host order
 * @param  x Value to be converted
 * @return Converted value
 */
uint64_t PLAT_ntohll(uint64_t x);

#ifndef _LINUX_TIMEX_H
/*
 * linux_hal_generic_adj.cpp includes linux/timex.h, which precludes definition
 * of time_h, so we can't make this function available there or we will get an
 * error about time_t not having a type.
 */

/**
 * @brief  Converts a time_t structure into a tm structure
 * @param[in]  inTime  The time_t to be converted
 * @param[out] outTm   The tm to store the converted value in
 * @return  An error code
 */
int PLAT_localtime(const time_t * inTime, struct tm * outTm);
#endif


#endif
