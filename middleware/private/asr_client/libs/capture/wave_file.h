/*************************************************************************
    > File Name: wave_file.h
    > Author:
    > Mail:
    > Created Time: Sat 12 May 2018 07:39:33 AM EDT
 ************************************************************************/

#ifndef _WAVE_FILE_H
#define _WAVE_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

void begin_wave(int fd, size_t cnt, int channels);

void end_wave(int fd, unsigned int fdcount);

#ifdef __cplusplus
}
#endif

#endif
