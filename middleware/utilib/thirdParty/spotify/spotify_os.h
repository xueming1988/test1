/**
 * Copyright (c) 2013-2014 Spotify AB
 *
 * This file is part of the Spotify Embedded SDK examples suite.
 */

#ifndef _SPOTIFY_OS_H
#define _SPOTIFY_OS_H

#include "spotify_embedded.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef HANDLE MUTEX_T;
typedef HANDLE THREAD_T;
#define THREAD_ROUTINE(f) DWORD WINAPI f
#define MUTEX_INIT(mtx) (((mtx) = CreateMutex(NULL, FALSE, NULL)) != NULL)
#define MUTEX_DESTROY(mtx) CloseHandle((mtx))
#define MUTEX_LOCK(mtx) WaitForSingleObject((mtx), INFINITE)
#define MUTEX_UNLOCK(mtx) ReleaseMutex((mtx))
#define CREATE_THREAD(thr, func, arg)                                                              \
    (((thr) = CreateThread(NULL, 0, (func), arg, 0, NULL)) != NULL)
#define SLEEP(ms) Sleep((ms))
#elif defined(__unix__) || defined(__APPLE__)
#include <unistd.h>
#include <pthread.h>
typedef pthread_mutex_t MUTEX_T;
typedef pthread_t THREAD_T;
#define THREAD_ROUTINE(f) void *(f)
#define MUTEX_INIT(mtx) (pthread_mutex_init(&(mtx), NULL) == 0)
#define MUTEX_DESTROY(mtx) pthread_mutex_destroy(&(mtx))
#define MUTEX_LOCK(mtx) pthread_mutex_lock(&(mtx))
#define MUTEX_UNLOCK(mtx) pthread_mutex_unlock(&(mtx))
#define CREATE_THREAD(thr, func, arg) (pthread_create(&(thr), NULL, (func), arg) == 0)
#define SLEEP(ms) usleep((ms)*1000)
#endif

const char *OsDeviceId(void);
SpError OsSetupZeroconf(const char *name, int port, const char *cgipath);
int OsKeyPress(void);

#endif
