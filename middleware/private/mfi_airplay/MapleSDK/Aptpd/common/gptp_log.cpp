/*************************************************************************************************************
Copyright (c) 2012-2016, Harman International Industries, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS LISTED "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS LISTED BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*************************************************************************************************************/

#include <gptp_log.hpp>

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <platform.hpp>

// MS VC++ 2013 has C++11 but not C11 support, use this to get millisecond resolution
#include <chrono>

#include <mutex>
std::mutex gLogKeeper;

#ifdef GENIVI_DLT
DLT_DECLARE_CONTEXT(dlt_con_gptp);
#endif

void gptplogRegister(void)
{
#ifdef GENIVI_DLT
	DLT_REGISTER_APP("GPTP","OpenAVB gPTP");
	DLT_REGISTER_CONTEXT(dlt_con_gptp, "GNRL", "General Context");
#endif
}

void gptplogUnregister(void)
{
#ifdef GENIVI_DLT
	DLT_UNREGISTER_CONTEXT(dlt_con_gptp);
	DLT_UNREGISTER_APP();
#endif
}

void gptpLog(GPTP_LOG_LEVEL level, const char *tag, const char *path, int line, const char *fmt, ...)
{
	char msg[1024];

	std::lock_guard<std::mutex> guard(gLogKeeper);
	
	va_list args;
	va_start(args, fmt);
	vsprintf(msg, fmt, args);

#ifndef GENIVI_DLT
	std::chrono::system_clock::time_point cNow = std::chrono::system_clock::now();
	time_t tNow = std::chrono::system_clock::to_time_t(cNow);
	struct tm tmNow;
	PLAT_localtime(&tNow, &tmNow);
	std::chrono::system_clock::duration roundNow = cNow - std::chrono::system_clock::from_time_t(tNow);
	long int millis = (long int) std::chrono::duration_cast<std::chrono::milliseconds>(roundNow).count();

	if (path) {
		fprintf(stderr, "%s: GPTP [%2.2d:%2.2d:%2.2d:%3.3ld] [%s:%u] %s\n",
			   tag, tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec, millis, path, line, msg);
	}
	else {
		fprintf(stderr, "%s: GPTP [%2.2d:%2.2d:%2.2d:%3.3ld] %s\n",
			   tag, tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec, millis, msg);
	}
#else
	DltLogLevelType dlt_level; 

	switch (level) {
	case GPTP_LOG_LVL_CRITICAL:
		dlt_level = DLT_LOG_FATAL;
		break;
	case GPTP_LOG_LVL_ERROR:
		dlt_level = DLT_LOG_ERROR;
		break;
	case GPTP_LOG_LVL_EXCEPTION:
	case GPTP_LOG_LVL_WARNING:
		dlt_level = DLT_LOG_WARN;
		break;
	case GPTP_LOG_LVL_INFO:
	case GPTP_LOG_LVL_STATUS:
		dlt_level = DLT_LOG_INFO;
		break;
	case GPTP_LOG_LVL_DEBUG:
		dlt_level = DLT_LOG_DEBUG;
		break;
	case GPTP_LOG_LVL_VERBOSE:
		dlt_level = DLT_LOG_VERBOSE;
		break;
	default:
		dlt_level = DLT_LOG_INFO;
		break;
	}

	DLT_LOG(dlt_con_gptp, dlt_level, DLT_STRING(msg));
#endif

}

