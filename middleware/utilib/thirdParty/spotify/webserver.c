/**
 * Copyright (c) 2013-2014 Spotify AB
 *
 * This file is part of the Spotify Embedded SDK examples suite.
 */

#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <sys/time.h>

#include "spotify_embedded.h"

#include "webserver.h"
#include "spotify_os.h"
#include "wm_util.h"
#include "json.h"

#ifdef _WIN32
#include <windows.h>
#define snprintf sprintf_s // not really the same but will do
#endif

extern void timeraddMS(struct timeval *, unsigned int);

static SpError g_login_error;
static sem_t g_sem_loginevent;

int SpExampleDNSCache(char *host)
{
    struct addrinfo hints = {0};
    struct addrinfo *ailist, *cur;
    char buf[256] = {0};
    struct sockaddr_in four;

    hints.ai_family = AF_INET;
    int value = getaddrinfo(host, NULL, &hints, &ailist);

    if (value != 0) {
        return -1;
    }
    for (cur = ailist; cur; cur = cur->ai_next) {
        if (cur->ai_family == AF_INET) {
            memcpy(&four, (void *)((struct sockaddr_in *)cur->ai_addr), sizeof(struct sockaddr_in));
            break;
        }
    }
    freeaddrinfo(ailist);
    // dns cache
    sprintf(buf, "sed -i '/%s/d' /etc/hosts ; sed -i '1 i %s %s' /etc/hosts", host,
            inet_ntoa(four.sin_addr), host);
    system(buf);

    return 0;
}

void SpInitLoginEvent(void) { sem_init(&g_sem_loginevent, 0, 0); }

void SpLoginEventClean(void)
{
    int tryret;
    do {
        tryret = sem_trywait(&g_sem_loginevent);
    } while (!tryret);
}

void SpExampleWebserverReportLoginStatus(SpError error, int signal)
{
    printf("Spotify login status: %d\n", error);
    g_login_error = error;
    if (signal)
        sem_post(&g_sem_loginevent);
}

SpError SpExampleWebWaitSpotifyLogin(int waitms)
{
    struct timeval now;
    struct timespec timeToWait;
    printf("Spotify WebWaitSpotifyLogin ++ \r\n");
    gettimeofday(&now, NULL);
    timeraddMS(&now, waitms); // wait 20s for timeout
    timeToWait.tv_sec = now.tv_sec;
    timeToWait.tv_nsec = now.tv_usec * 1000;
    sem_timedwait(&g_sem_loginevent, &timeToWait);
    printf("Spotify WebWaitSpotifyLogin -- \r\n");

    return g_login_error;
}
