/******************************************************************************

  Copyright (c) 2009-2012, Intel Corporation 
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

#ifndef AVBTS_OSTIMER_HPP
#define AVBTS_OSTIMER_HPP

/**@file*/

/**
 * @brief OSTimer generic interface
 */
class OSTimer {
public:
	/**
	 * @brief Sleep for a given amount of time
	 * @param  micro Time in micro-seconds
	 * @return Implmentation specific
	 */
	virtual unsigned long sleep(unsigned long micro) = 0;

	/*
	 * Virtual destructor
	 */
	virtual ~OSTimer() = 0;
};

inline OSTimer::~OSTimer() {}

/**
 * @brief Provides factory design patter for OSTimer class
 */
class OSTimerFactory {
public:
	/**
	 * @brief Creates the OSTimer
	 * @return Pointer to OSTimer object
	 */
	virtual OSTimer *createTimer() const = 0;

	/*
	 * Destroys the OSTimer previsouly created
	 */
	virtual ~OSTimerFactory() = 0;
};

inline OSTimerFactory::~OSTimerFactory() {}

#endif
