/**
 * Copyright (c) 2013-2014 Spotify AB
 *
 * This file is part of the Spotify Embedded SDK examples suite.
 */

#ifndef _SPOTIFY_WEBSERVER_H

/**
 * If a login was initiated from an "addUser" request, this function is used
 * by the application to report login success (if kSpConnectionLoggedIn is received)
 * or login failure (if SpCallbackError() is invoked).
 */
void SpExampleWebserverReportLoginStatus(SpError error, int signal);

int SpExampleDNSCache(char *host);

SpError SpExampleWebWaitSpotifyLogin(int waitms);

void SpInitLoginEvent(void);

void SpLoginEventClean(void);

#define _SPOTIFY_WEBSERVER_H
#endif
