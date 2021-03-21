/*
 * ShairPort Initializer - Network Initializer/Mgr for Hairtunes RAOP
 * Copyright (c) M. Andrew Webster 2011
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <fcntl.h>
#include <sys/prctl.h>
#include <signal.h>
#include "socketlib.h"
#include "shairport.h"
#include "hairtunes.h"
#include <linux/socket.h>
#include <linux/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <netinet/if_ether.h>
#include <linux/sockios.h>
#include "DMAP.h"

#include <cm1_msg_comm.h>
#if	defined(PLATFORM_MTK)
#include "nvram.h"
#endif
#include "Responder.h"

#include "wm_util.h"
#include "common.h"
#ifndef TRUE
    #define TRUE (-1)
#endif
#ifndef FALSE
    #define FALSE (0)
#endif

WIIMU_CONTEXT* g_wiimu_shm = 0;

// TEMP
int kCurrentLogLevel = LOG_INFO;
int bufferStartFill = 220;
int gPid=-1;
int only_spotify_server = 0;

#ifdef _WIN32
    #define DEVNULL "nul"
#else
    #define DEVNULL "/dev/null"
#endif
#define RSA_LOG_LEVEL LOG_DEBUG_VV
#define SOCKET_LOG_LEVEL LOG_DEBUG_VV
#define HEADER_LOG_LEVEL LOG_DEBUG
#define AVAHI_LOG_LEVEL LOG_DEBUG


typedef struct _airplay_notify{
	char notify[32];
	long lposition;
	long lduration;
	char title[512];
	char artist[512];
	char album[512];
	char album_uri[128];
}airplay_notify;

char g_hwstr[32]={0};
char g_ServerName[64]={0};
int g_port = 0;

void *cmdSocket_airplay(void *arg);

static void handleClient(int pSock, char *pPassword, char *pHWADDR);
static void writeDataToClient(int pSock, struct shairbuffer *pResponse);
static int readDataFromClient(int pSock, struct shairbuffer *pClientBuffer);

static int parseMessage(struct connection *pConn,unsigned char *pIpBin, unsigned int pIpBinLen, char *pHWADDR);
static void propogateCSeq(struct connection *pConn);

static void cleanupBuffers(struct connection *pConnection);
static void cleanup(struct connection *pConnection);

//static int startAvahi(const char *pHwAddr, const char *pServerName, int pPort);

static int getAvailChars(struct shairbuffer *pBuf);
static void addToShairBuffer(struct shairbuffer *pBuf, char *pNewBuf);
static void addNToShairBuffer(struct shairbuffer *pBuf, char *pNewBuf, int pNofNewBuf);

static char *getTrimmedMalloc(char *pChar, int pSize, int pEndStr, int pAddNL);
static char *getTrimmed(char *pChar, int pSize, int pEndStr, int pAddNL, char *pTrimDest);

static void slog(int pLevel, char *pFormat, ...);
static int isLogEnabledFor(int pLevel);

static void initConnection(struct connection *pConn, struct keyring *pKeys, struct comms *pComms, int pSocket, char *pPassword);
static void closePipe(int *pPipe);
static void initBuffer(struct shairbuffer *pBuf, int pNumChars);

static void setKeys(struct keyring *pKeys, char *pIV, char* pAESKey, char *pFmtp);
static RSA *loadKey(void);

static int StartAdvertisement(const char *pHWStr, const char *pServerName, int pPort);

// RSA Encrypt
RSA *rsa;

static void handle_sigchld(int signo) {
    int status;
    waitpid(-1, &status, WNOHANG);
    debug(1, "===== child of %d exit ===\n", getpid());
}

char tAoDriver[56] = "";
char tAoDeviceName[56] = "";
char tAoDeviceId[56] = "";
extern int  ao_rtsp ;

#define ETH_ALEN 6
int getIfMac(char *ifname,  char *if_hw)
{
   struct ifreq req;
   int err,i;

   int s=socket(AF_INET,SOCK_DGRAM,0);
   if(s < 0)
   {
	 return -1;
   }
   strncpy(req.ifr_name, ifname,IFNAMSIZ-1);
   req.ifr_name[IFNAMSIZ-1] = '\0';
   req.ifr_hwaddr.sa_family=ARPHRD_ETHER;
   err=ioctl(s,SIOCGIFHWADDR,&req);
   close(s);
   if(err != -1)
   {
      memcpy(if_hw,req.ifr_hwaddr.sa_data,ETH_ALEN);
      for(i=0;i<ETH_ALEN;i++)
      {
        //printf("%02x",(unsigned char)if_hw[i]);
      }
   }
   return err;
}
extern shairport_cfg config;
static airplay_notify _airplay_notify;
static int ablum_cover_send = 1;

int main(int argc, char **argv)
{
    // unfortunately we can't just IGN on non-SysV systems
    struct sigaction sa;
    sa.sa_handler = handle_sigchld;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGCHLD, &sa, NULL) < 0)
    {
        perror("sigaction");
        return 1;
    }

	g_wiimu_shm = WiimuContextGet();
	if(!g_wiimu_shm || g_wiimu_shm->legal_flag == 0)
	{
		fprintf(stderr, "Illegal ROM for airplay\n");
		system("smplayer /system/workdir/misc/Voice-prompt/common/error.mp3 &");
		return -1;
	}
	g_wiimu_shm->airplay_volume_notify_ts = 0;

#ifdef EXPIRE_CHECK_ENABLE
	extern int run_expire_check_thread(char *process, int expire_days, int failed_flag, char *command, int interval);
	run_expire_check_thread("airplay", EXPIRE_DAYS, 3, NULL, 30);
#endif

    char tHWID[HWID_SIZE] = {0,51,52,53,54,55};
    char tHWID_Hex[HWID_SIZE * 2 + 1];
    memset(tHWID_Hex, 0, sizeof(tHWID_Hex));

    char tServerName[56] = "Airplay";
    char tPassword[56] = "";
	ao_rtsp = AO_RTSP_DISABLE;

    struct addrinfo *tAddrInfo;
    int  tSimLevel = 0;
    int  tUseKnownHWID = FALSE;
    int  tDaemonize = FALSE;
    int  tPort = PORT;
	char *pp;

    char *arg;
    mkdir("/tmp/web", 0755);


	while(g_wiimu_shm->system_ready_flag == 0)
		sleep(1);
	wiimu_log(1, 0,0, 0,"airplay enter");

    while ((arg = *++argv))
    {
        if (!strcmp(arg, "-a"))
        {
            strncpy(tServerName, *++argv, 55);
            argc--;
        }
        else if (!strncmp(arg, "--apname=", 9))
        {
            strncpy(tServerName, arg+9, 55);
        }
		else if (!strncmp(arg, "--rtsp", 6))
        {
            ao_rtsp = AO_RTSP_ENABLE;
			debug(1, "audio out is rtsp \n");
        }
        else if (!strncmp(arg, "--ao_driver=",12 ))
        {
            strncpy(tAoDriver, arg+12, 55);
        }
        else if (!strncmp(arg, "--ao_devicename=",16 ))
        {
            strncpy(tAoDeviceName, arg+16, 55);
        }
        else if (!strncmp(arg, "--ao_deviceid=",14 ))
        {
            strncpy(tAoDeviceId, arg+14, 55);
        }
        else if (!strncmp(arg, "--apname=", 9))
        {
            strncpy(tServerName, arg+9, 55);
        }
        else if (!strcmp(arg, "-p"))
        {
            strncpy(tPassword, *++argv, 55);
            argc--;
        }
        else if (!strncmp(arg, "--password=",11 ))
        {
            strncpy(tPassword, arg+11, 55);
        }
        else if (!strcmp(arg, "-o"))
        {
            tPort = atoi(*++argv);
            argc--;
        }
        else if (!strncmp(arg, "--server_port=", 14))
        {
            tPort = atoi(arg+14);
        }
        else if (!strcmp(arg, "-b"))
        {
            bufferStartFill = atoi(*++argv);
            argc--;
        }
        else if (!strncmp(arg, "--buffer=", 9))
        {
            bufferStartFill = atoi(arg + 9);
        }
        else if (!strcmp(arg, "-k"))
        {
            tUseKnownHWID = TRUE;
        }
        else if (!strcmp(arg, "-q") || !strncmp(arg, "--quiet", 7))
        {
            kCurrentLogLevel = 0;
        }
        else if (!strcmp(arg, "-d"))
        {
            tDaemonize = TRUE;
            kCurrentLogLevel = 0;
        }
        else if (!strcmp(arg, "-v"))
        {
            kCurrentLogLevel = LOG_DEBUG;
        }
        else if (!strcmp(arg, "-v2"))
        {
            kCurrentLogLevel = LOG_DEBUG_V;
        }
        else if (!strcmp(arg, "-vv") || !strcmp(arg, "-v3"))
        {
            kCurrentLogLevel = LOG_DEBUG_VV;
        }
        else if (!strcmp(arg, "-h") || !strcmp(arg, "--help"))
        {
            slog(LOG_INFO, "Airplay version 0.05 C port - Airport Express emulator\n");
            slog(LOG_INFO, "Usage:\nairplay [OPTION...]\n\nOptions:\n");
            slog(LOG_INFO, "  -a, --apname=AirPort    Sets Airport name\n");
            slog(LOG_INFO, "  -p, --password=secret   Sets Password (not working)\n");
            slog(LOG_INFO, "  -o, --server_port=5002  Sets Port for Avahi/dns-sd/howl\n");
            slog(LOG_INFO, "  -b, --buffer=282        Sets Number of frames to buffer before beginning playback\n");
            slog(LOG_INFO, "  -d                      Daemon mode\n");
            slog(LOG_INFO, "  -q, --quiet             Supresses all output.\n");
            slog(LOG_INFO, "  -v,-v2,-v3,-vv          Various debugging levels\n");
            slog(LOG_INFO, "\n");
            return 0;
        }else if (!strcmp(arg, "--spotify"))
        {
           only_spotify_server = 1;
        }
    }

	if(strlen(g_wiimu_shm->group_name))
		strcpy(tServerName, g_wiimu_shm->group_name);
	else if(strlen(g_wiimu_shm->device_name))
		strcpy(tServerName, g_wiimu_shm->device_name);
	else if(strlen(g_wiimu_shm->ssid))
		strcpy(tServerName, g_wiimu_shm->ssid);

    if (tDaemonize)
    {
        int tPid = fork();
        if (tPid < 0)
        {
            exit(1); // Error on fork
        }
        else if (tPid > 0)
        {
            exit(0);
        }
        else
        {
            setsid();
            int tIdx = 0;
            for (tIdx = getdtablesize(); tIdx >= 0; --tIdx)
            {
                close(tIdx);
            }
            tIdx = open(DEVNULL, O_RDWR);
			if(tIdx > 0)
			{
	            dup(tIdx);
	            dup(tIdx);
			}
        }
    }
    srandom ( time(NULL) );
    // Copy over empty 00's
    //tPrintHWID[tIdx] = tAddr[0];

#ifdef  X86
    char IF[]={"eth0"};
#else
    char IF[]={WIFI_INFC};
#endif
    if(!getIfMac(IF, tHWID))
    {
        tUseKnownHWID = TRUE;
    }

    int tIdx = 0;
    for (tIdx=0;tIdx<HWID_SIZE;tIdx++)
    {
        if (tIdx > 0)
        {
            if (!tUseKnownHWID)
            {
                int tVal = ((random() % 80) + 33);
                tHWID[tIdx] = tVal;
            }
            //tPrintHWID[tIdx] = tAddr[tIdx];
        }
        sprintf(tHWID_Hex+(tIdx*2), "%02X",(unsigned char)tHWID[tIdx]);
    }
    //tPrintHWID[HWID_SIZE] = '\0';

    slog(LOG_INFO, "LogLevel: %d\n", kCurrentLogLevel);
    slog(LOG_INFO, "AirName: %s\n", tServerName);
    //slog(LOG_INFO, "HWID: %.*s\n", HWID_SIZE, tHWID+1);
    slog(LOG_INFO, "HWID_Hex(%d): %s\n", strlen(tHWID_Hex), tHWID_Hex);
	rsa = loadKey();
    config.delay = (2205000+240000);		// Set default Audio Latency
    config.latency = (2205000+240000);

    if (tSimLevel >= 1)
    {
#ifdef SIM_INCL
        sim(tSimLevel, tTestValue, tHWID);
#endif
        return(1);
    }
    else
    {
  //      startAvahi(tHWID_Hex, tServerName, tPort);
  	strcpy(g_hwstr,tHWID_Hex);
    strcpy(g_ServerName,tServerName);
	g_port= tPort;


	 pid_t wpid;
     if((wpid = fork())<0)
	 {

	 }
	 else if(wpid == 0)
	 {
	     debug(1, "==airplay child process(advertise)%d started ==\n", getpid());
	     StartAdvertisement(tHWID_Hex, tServerName, tPort);
	     debug(1, "==airplay child process(advertise)%d exit ==\n", getpid());
		 cmdSocket_airplay(NULL);
		  _exit(127);
	 }
	 else
	 {

	 }

	 while(only_spotify_server)
	 {
		sleep(10);
	 }

        slog(LOG_DEBUG_V, "Starting connection server: specified server port: %d\n", tPort);
        int tServerSock = setupListenServer(&tAddrInfo, tPort);
        if (tServerSock < 0)
        {
            freeaddrinfo(tAddrInfo);
            slog(LOG_INFO, "Error setting up server socket on port %d, try specifying a different port\n", tPort);
            exit(1);
        }

	//	pthread_t a_thread;
	// pthread_create(&a_thread,NULL,cmdSocket_airplay,NULL);


		fd_set rfds;
		struct timeval to;

        int tClientSock = 0;
	#ifdef AF_INET6
		struct sockaddr_in6* so = NULL;
	#else
		struct sockaddr_in* so = NULL;
	#endif
		char ipstr[64];
		char ipstr_last[64]="0.0.0.0";
        while (1)
        {
            //debug(3, "Waiting for clients to connect, pid=%d\n", getpid());
#ifdef S11_EVB_YIJIA
			if(access("/tmp/linein_force", R_OK)==0)
			{
				sleep(1);
				continue;
			}
#endif
			FD_ZERO(&rfds);
			FD_SET(tServerSock, &rfds);
			to.tv_sec = 1;
			to.tv_usec = 0;

			int res = select(tServerSock + 1, &rfds,NULL , NULL, &to);

			if (res > 0 && FD_ISSET(tServerSock, &rfds))
			{
				FD_CLR(tServerSock,&rfds);
	            tClientSock = acceptClient(tServerSock, tAddrInfo);
#ifdef AF_INET6
				so = (struct sockaddr_in6 *)tAddrInfo->ai_addr;
		        inet_ntop(AF_INET6, &so->sin6_addr, ipstr, sizeof ipstr);
				debug(1, "from %s ; %d\r\n",ipstr,ntohs(so->sin6_port));
#else
    			strcpy(ipstr,inet_ntoa(so->sin_addr));
				debug(1, "from %s ; %d\r\n",ipstr,ntohs(so->sin_port));

#endif

	            if (tClientSock >= 0)
	            {
#ifdef  AIRPLAY_SINGLE_CLIENT
	                if(gPid > 0 && strncmp(ipstr_last,ipstr,strlen(ipstr_last)) !=0)
	                {
						strcpy(ipstr_last,ipstr);
	                    kill(gPid, 9);
	                    slog(LOG_INFO, "kill old child process: %d\n",gPid);
	                  //  sleep(1);
	                }
#endif  //AIRPLAY_SINGLE_CLIENT
					strcpy(ipstr_last,ipstr);

	                int tPid = fork();
	                if (tPid == 0)
	                {
	                    prctl(PR_SET_PDEATHSIG, 15);
	                    freeaddrinfo(tAddrInfo);
	                    tAddrInfo = NULL;
	                  	slog(LOG_INFO, "...Accepted Client Connection..\n");
	                 //   close(tServerSock);
	                    debug(1, "==airplay child process(client_handler) %d started ==\n", getpid());
	                    handleClient(tClientSock, tPassword, tHWID);
					  	slog(LOG_INFO, "----- handleClient -----\n");
	                    debug(1, "==airplay child process(client_handler) %d exit\n", getpid());
	                    exit(0);
	                }
	                else if(tPid < 0)
	                {
						slog(LOG_INFO, "fork error!!!!!!! \n");
	                }
					else
	                {
	                    slog(LOG_INFO, "Child now busy handling new client\n");
	                  	close(tClientSock);
						gPid = tPid;
	                }
	            }
	            else
	            {
	                // failed to init server socket....try waiting a moment...
	                sleep(2);
	            }
			}
        }
    }

    slog(LOG_DEBUG_VV, "Finished, and waiting to clean everything up\n");
    sleep(1);
    if (tAddrInfo != NULL)
    {
        freeaddrinfo(tAddrInfo);
    }
	RSA_free(rsa);
    return 0;
}

static int findEnd(char *tReadBuf)
{
    // find \n\n, \r\n\r\n, or \r\r is found
    int tIdx = 0;
    int tLen = strlen(tReadBuf);
    for (tIdx = 0; tIdx < tLen; tIdx++)
    {
        if (tReadBuf[tIdx] == '\r')
        {
            if (tIdx + 1 < tLen)
            {
                if (tReadBuf[tIdx+1] == '\r')
                {
                    return(tIdx+1);
                }
                else if (tIdx+3 < tLen)
                {
                    if (tReadBuf[tIdx+1] == '\n' &&
                        tReadBuf[tIdx+2] == '\r' &&
                        tReadBuf[tIdx+3] == '\n')
                    {
                        return(tIdx+3);
                    }
                }
            }
        }
        else if (tReadBuf[tIdx] == '\n')
        {
            if (tIdx + 1 < tLen && tReadBuf[tIdx+1] == '\n')
            {
                return(tIdx + 1);
            }
        }
    }
    // Found nothing
    return -1;
}

static void handleClient(int pSock, char *pPassword, char *pHWADDR)
{
    slog(LOG_DEBUG_VV, "In Handle Client\n");
    fflush(stdout);

    socklen_t len;
    struct sockaddr_storage addr;
#ifdef AF_INET6
    unsigned char ipbin[INET6_ADDRSTRLEN];
#else
    unsigned char ipbin[INET_ADDRSTRLEN];
#endif
    unsigned int ipbinlen;
    int port;
    char ipstr[64];

    len = sizeof addr;
    if(getsockname(pSock, (struct sockaddr*)&addr, &len) != 0)
    {
    	slog(LOG_DEBUG, "getsockname failed \n");
		return;
    }

    // deal with both IPv4 and IPv6:
    if (addr.ss_family == AF_INET)
    {
        slog(LOG_INFO, "Constructing ipv4 address\n");
        struct sockaddr_in *s = (struct sockaddr_in *)&addr;
        port = ntohs(s->sin_port);
        inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
        memcpy(ipbin, &s->sin_addr, 4);
        ipbinlen = 4;
    }
    else // AF_INET6
    {
        slog(LOG_INFO, "Constructing ipv6 address\n");
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
        port = ntohs(s->sin6_port);
        inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);

        union
        {
            struct sockaddr_in6 s;
            unsigned char bin[sizeof(struct sockaddr_in6)];
        } addr;
	  	memcpy(&addr.s, &s->sin6_addr, sizeof(struct sockaddr_in6));

        if (memcmp(&addr.bin[0], "\x00\x00\x00\x00" "\x00\x00\x00\x00" "\x00\x00\xff\xff", 12) == 0)
        {
            // its ipv4...
            memcpy(ipbin, &addr.bin[12], 4);
            ipbinlen = 4;
        }
        else
        {
            memcpy(ipbin, &s->sin6_addr, 16);
            ipbinlen = 16;
        }
    }

    slog(LOG_DEBUG_V, "Peer IP address: %s\n", ipstr);
    slog(LOG_DEBUG_V, "Peer port      : %d\n", port);

    int tMoreDataNeeded = 1;
    struct keyring     tKeys;
    struct comms       tComms;
    struct connection  tConn;
    initConnection(&tConn, &tKeys, &tComms, pSock, pPassword);
	remove("/tmp/airplay_stop");

    while (1)
    {
        tMoreDataNeeded = 1;

        initBuffer(&tConn.recv, 80); // Just a random, small size to seed the buffer with.
        initBuffer(&tConn.resp, 80);

        int tError = FALSE;
        while (1 == tMoreDataNeeded)
        {
            tError = readDataFromClient(pSock, &(tConn.recv));
            if (!tError && strlen(tConn.recv.data) > 0)
            {
              //  slog(LOG_INFO, "Finished Reading some data from client\n");
                // parse client request
                tMoreDataNeeded = parseMessage(&tConn, ipbin, ipbinlen, pHWADDR);
                if (1 == tMoreDataNeeded)
                {
                    slog(LOG_DEBUG, "\n\nNeed to read more data\n");
                }
                else if (-1 == tMoreDataNeeded) // Forked process down below ended.
                {
                    slog(LOG_DEBUG, "Forked Process ended...cleaning up\n");
                    writeDataToClient(pSock, &(tConn.resp));
                    cleanup(&tConn);
                    return;
                }
                // if more data needed,
            }
            else if(tError > 0)
            {
				//
            }else
            {
                slog(LOG_DEBUG, "Error reading from socket, closing client\n");
                // Error reading data....quit.
               cleanup(&tConn);
                return;
            }
        }
   //     slog(LOG_INFO, "Writing: %d chars to socket\n", tConn.resp.current);
        //tConn->resp.data[tConn->resp.current-1] = '\0';
        writeDataToClient(pSock, &(tConn.resp));
        // Finished reading one message...
        cleanupBuffers(&tConn);
    }
    cleanup(&tConn);
    fflush(stdout);
}

static void writeDataToClient(int pSock, struct shairbuffer *pResponse)
{
    slog(LOG_DEBUG_VV, "\n----Beg Send Response Header----\n%.*s\n", pResponse->current, pResponse->data);
    send(pSock, pResponse->data, pResponse->current,0);
    slog(LOG_DEBUG_VV, "----Send Response Header----\n");
}

static int readDataFromClient(int pSock, struct shairbuffer *pClientBuffer)
{
    char tReadBuf[MAX_SIZE];
    tReadBuf[0] = '\0';

    int tRetval = 1;
    int tEnd = -1;

	struct timeval to;
	fd_set rfds;

	//debug(1,"readDataFromClient++ \n");

    while (tRetval > 0 && tEnd < 0)
    {

		if(access("/tmp/airplay_stop", R_OK)==0 )
		{
			slog(LOG_DEBUG, "airplay_stop\n");
			return -1;
		}
        // Read from socket until \n\n, \r\n\r\n, or \r\r is found
        debug(5, "Waiting To Read...\n");
        fflush(stdout);
		FD_ZERO(&rfds);
		FD_SET(pSock, &rfds);
		to.tv_sec = 1;
    	to.tv_usec = 0;

		int res = select(pSock + 1, &rfds, NULL, NULL, &to);
		if (res < 0) {
			usleep(10000);
			continue;
		} else if (res > 0 && FD_ISSET(pSock, &rfds))
		{
				memset(tReadBuf,0x0,MAX_SIZE);
		        tRetval = recv(pSock, tReadBuf, MAX_SIZE , 0);
				if(tRetval <=0)
				{
					slog(LOG_INFO, "tRetval %d \n", tRetval);
					return -1;
				}
			//	debug(5,"tRetval %d \n",tRetval);
		        // if new buffer contains the end of request string, only copy partial buffer?
		        tEnd = findEnd(tReadBuf);
			//	debug(5,"tEnd %d \n",tEnd);
		        if (tEnd >= 0)
		        {
		            if (pClientBuffer->marker == 0)
		            {
		                pClientBuffer->marker = tEnd+1; // Marks start of content
		            }

		            slog(SOCKET_LOG_LEVEL, "Found end of http request at: %d\n", tEnd);
		            fflush(stdout);
		        }
		        else
		        {
		            tEnd = MAX_SIZE;
		            slog(SOCKET_LOG_LEVEL, "Read %d of data so far\n", tRetval);
		            fflush(stdout);
		        }

		        pClientBuffer->markend += tRetval; // Marks end of content

		        if (tRetval > 0)
		        {
		            // Copy read data into tReceive;
		            slog(SOCKET_LOG_LEVEL, "Read %d data, using %d of it\n", tRetval, tEnd);
		            addNToShairBuffer(pClientBuffer, tReadBuf, tRetval);
		            slog(LOG_DEBUG_VV, "Finished copying data\n");
		        }
		    }
		    if (tEnd + 1 != tRetval)
		    {
		        debug(5, "Read more data after end of http request. %d instead of %d\n", tRetval, tEnd+1);
		    }
		    debug(5, "Finished Reading Data:\n%s\nEndOfData\n", pClientBuffer->data);
		    fflush(stdout);
		    usleep(10000);
    }

//	debug(5,"readDataFromClient-- \n");
    return 0;

}

static char *getFromBuffer(char *pBufferPtr, const char *pField, int pLenAfterField, int *pReturnSize, char *pDelims)
{
    slog(LOG_DEBUG_V, "GettingFromBuffer: %s\n", pField);
    char* tFound = strstr(pBufferPtr, pField);
    int tSize = 0;
    if (tFound != NULL)
    {
        tFound += (strlen(pField) + pLenAfterField);
        int tIdx = 0;
        char tDelim = pDelims[tIdx];
        char *tShortest = NULL;
        char *tEnd = NULL;
        while (tDelim != '\0')
        {
            tDelim = pDelims[tIdx++]; // Ensures that \0 is also searched.
            tEnd = strchr(tFound, tDelim);
            if (tEnd != NULL && (NULL == tShortest || tEnd < tShortest))
            {
                tShortest = tEnd;
            }
        }

        tSize = (int) (tShortest - tFound);
        slog(LOG_DEBUG_VV, "Found %.*s  length: %d\n", tSize, tFound, tSize);
        if (pReturnSize != NULL)
        {
            *pReturnSize = tSize;
        }
    }
    else
    {
        slog(LOG_DEBUG_V, "Not Found\n");
    }
    return tFound;
}

static char *getFromHeader(char *pHeaderPtr, const char *pField, int *pReturnSize)
{
    return getFromBuffer(pHeaderPtr, pField, 2, pReturnSize, "\r\n");
}

static char *getFromContent(char *pContentPtr, const char* pField, int *pReturnSize)
{
    return getFromBuffer(pContentPtr, pField, 1, pReturnSize, "\r\n");
}

static char *getFromSetup(char *pContentPtr, const char* pField, int *pReturnSize)
{
    return getFromBuffer(pContentPtr, pField, 1, pReturnSize, ";\r\n");
}
static int	strnicmpx( const void *inS1, size_t inN, const char *inS2 )
{
	const unsigned char *		s1;
	const unsigned char *		s2;
	int							c1;
	int							c2;

	s1 = (const unsigned char *) inS1;
	s2 = (const unsigned char *) inS2;
	while( inN-- > 0 )
	{
		c1 = tolower( *s1 );
		c2 = tolower( *s2 );
		if( c1 < c2 ) return( -1 );
		if( c1 > c2 ) return(  1 );
		if( c2 == 0 ) return(  0 );

		++s1;
		++s2;
	}
	if( *s2 != 0 ) return( -1 );
	return( 0 );
}

// Handles compiling the Apple-Challenge, HWID, and Server IP Address
// Into the response the airplay client is expecting.
static int buildAppleResponse(struct connection *pConn, unsigned char *pIpBin,
                              unsigned int pIpBinLen, char *pHWID)
{
    // Find Apple-Challenge
    char *tResponse = NULL;

    int tFoundSize = 0;
    char* tFound = getFromHeader(pConn->recv.data, "Apple-Challenge", &tFoundSize);
    if (tFound != NULL)
    {
        char tTrim[tFoundSize + 2];
        getTrimmed(tFound, tFoundSize, TRUE, TRUE, tTrim);
        slog(LOG_DEBUG_VV, "HeaderChallenge:  [%s] len: %d  sizeFound: %d\n", tTrim, strlen(tTrim), tFoundSize);
        int tChallengeDecodeSize = 16;
        char *tChallenge = decode_base64((unsigned char *)tTrim, tFoundSize, &tChallengeDecodeSize);
        slog(LOG_DEBUG_VV, "Challenge Decode size: %d  expected 16\n", tChallengeDecodeSize);

        int tCurSize = 0;
        unsigned char tChalResp[38];

        memcpy(tChalResp, tChallenge, tChallengeDecodeSize);
        tCurSize += tChallengeDecodeSize;

        memcpy(tChalResp+tCurSize, pIpBin, pIpBinLen);
        tCurSize += pIpBinLen;

        memcpy(tChalResp+tCurSize, pHWID, HWID_SIZE);
        tCurSize += HWID_SIZE;

        int tPad = 32 - tCurSize;
        if (tPad > 0)
        {
            memset(tChalResp+tCurSize, 0, tPad);
            tCurSize += tPad;
        }

        char *tTmp = encode_base64((unsigned char *)tChalResp, tCurSize);
        slog(LOG_DEBUG_VV, "Full sig: %s\n", tTmp);
        free(tTmp);

        int tSize = RSA_size(rsa);
        unsigned char tTo[tSize];
        RSA_private_encrypt(tCurSize, (unsigned char *)tChalResp, tTo, rsa, RSA_PKCS1_PADDING);

        // Wrap RSA Encrypted binary in Base64 encoding
        tResponse = encode_base64(tTo, tSize);
        int tLen = strlen(tResponse);
        while (tLen > 1 && tResponse[tLen-1] == '=')
        {
            tResponse[tLen-1] = '\0';
        }
        free(tChallenge);
     //   RSA_free(rsa);
    }

    if (tResponse != NULL)
    {
        // Append to current response
        addToShairBuffer(&(pConn->resp), "Apple-Response: ");
        addToShairBuffer(&(pConn->resp), tResponse);
        addToShairBuffer(&(pConn->resp), "\r\n");
        free(tResponse);
        return TRUE;
    }
    return FALSE;
}

#define AIRPLAY_ALBUM_PATH "/tmp/web/airplay.jpg"
static void set_album_path(char *path, char *dest)
{
	char cmd[256];
	char buf[128]={0};
	int ret;
	FILE *fp;
	sprintf(cmd, "md5sum %s", path);
	fp = popen(cmd, "r");
	if( fp )
	{
		ret = fread(buf, 1,32, fp);
		pclose(fp);
		if(ret == 32)
		{
			buf[32]=0;
			system("rm -f /tmp/web/airplay_*.jpg");
			sprintf(cmd, "mv %s /tmp/web/airplay_%s.jpg ", AIRPLAY_ALBUM_PATH, buf);
			system(cmd);
			system("rm -f " AIRPLAY_ALBUM_PATH);
			sprintf(cmd, "ln -s /tmp/web/airplay_%s.jpg %s", buf, AIRPLAY_ALBUM_PATH);
			system(cmd);
			sprintf(dest, "/data/airplay_%s.jpg", buf);
			return;
		}
	}
	sprintf(dest, "%s", AIRPLAY_ALBUM_PATH);
}
//parseMessage(tConn->recv.data, tConn->recv.mark, &tConn->resp, ipstr, pHWADDR, tConn->keys);
static int parseMessage(struct connection *pConn, unsigned char *pIpBin, unsigned int pIpBinLen, char *pHWID)
{
    int tReturn = 0; // 0 = good, 1 = Needs More Data, -1 = close client socket.
    int need_body = 0;
	char bodybuf[256]= {0};
    if (pConn->resp.data == NULL)
    {
        initBuffer(&(pConn->resp), MAX_SIZE);
    }

    char *tContent = getFromHeader(pConn->recv.data, "Content-Length", NULL);
    if (tContent != NULL)
    {
        int tContentSize = atoi(tContent);
        if (pConn->recv.marker == 0 || pConn->recv.markend-pConn->recv.marker != tContentSize)
        {
            if (isLogEnabledFor(HEADER_LOG_LEVEL))
            {
                slog(HEADER_LOG_LEVEL, "Content-Length: %s value -> %d\n", tContent, tContentSize);
                if (pConn->recv.marker != 0)
                {
                    slog(HEADER_LOG_LEVEL, "ContentPtr has %d, but needs %d\n",
                         strlen(pConn->recv.data+pConn->recv.marker), tContentSize);
                }
            }
            // check if value in tContent > 2nd read from client.
            return 1; // means more content-length needed
        }
    }
    else
    {
        slog(LOG_DEBUG_VV, "No content, header only\n");
    }

	//fprintf(stderr,"%s \n", pConn->recv.data);
    // "Creates" a new Response Header for our response message
    addToShairBuffer(&(pConn->resp), "RTSP/1.0 200 OK\r\n");

    if (isLogEnabledFor(LOG_INFO))
    {
        int tLen = strchr(pConn->recv.data, ' ') - pConn->recv.data;
        if (tLen < 0 || tLen > 20)
        {
            tLen = 20;
        }
        if (!strncmp(pConn->recv.data, "OPTIONS", 7) == 0)
        {
           // slog(LOG_INFO, "********** RECV %.*s **********\n", tLen, pConn->recv.data);
        }

    }

    if (pConn->password != NULL)
    {

    }

    if (buildAppleResponse(pConn, pIpBin, pIpBinLen, pHWID)) // need to free sig
    {
        slog(LOG_DEBUG_V, "Added AppleResponse to Apple-Challenge request\n");
    }

    //debug(1, "\n\n\npConn->recv.data[%s] END\n\n", pConn->recv.data);
    // Find option, then based on option, do different actions.
    if (strncmp(pConn->recv.data, "OPTIONS", 7) == 0)
    {
        propogateCSeq(pConn);
        addToShairBuffer(&(pConn->resp),
                         "Public: ANNOUNCE, RECORD, FLUSH, OPTIONS, DESCRIBE, SETUP, TEARDOWN, PAUSE, GET_PARAMETER, SET_PARAMETER\r\n");
    }
    else if (!strncmp(pConn->recv.data, "ANNOUNCE", 8))
    {
        char *tContent = pConn->recv.data + pConn->recv.marker;
        int tSize = 0;
        char *tHeaderVal = getFromContent(tContent, "a=aesiv", &tSize); // Not allocated memory, just pointing
        if (tSize > 0)
        {
            int tKeySize = 0;
            char tEncodedAesIV[tSize + 2];
            getTrimmed(tHeaderVal, tSize, TRUE, TRUE, tEncodedAesIV);
            slog(LOG_DEBUG_VV, "AESIV: [%.*s] Size: %d  Strlen: %d\n", tSize, tEncodedAesIV, tSize, strlen(tEncodedAesIV));
            char *tDecodedIV =  decode_base64((unsigned char*) tEncodedAesIV, tSize, &tSize);

            // grab the key, copy it out of the receive buffer
            tHeaderVal = getFromContent(tContent, "a=rsaaeskey", &tKeySize);
            char tEncodedAesKey[tKeySize + 2]; // +1 for nl, +1 for \0
            getTrimmed(tHeaderVal, tKeySize, TRUE, TRUE, tEncodedAesKey);
            slog(LOG_DEBUG_VV, "AES KEY: [%s] Size: %d  Strlen: %d\n", tEncodedAesKey, tKeySize, strlen(tEncodedAesKey));
            // remove base64 coding from key
            char *tDecodedAesKey = decode_base64((unsigned char*) tEncodedAesKey,
                                                 tKeySize, &tKeySize);  // Need to free DecodedAesKey

            // Grab the formats
            int tFmtpSize = 0;
            char *tFmtp = getFromContent(tContent, "a=fmtp", &tFmtpSize);  // Don't need to free
            tFmtp = getTrimmedMalloc(tFmtp, tFmtpSize, TRUE, FALSE); // will need to free
            slog(LOG_DEBUG_VV, "Format: %s\n", tFmtp);


            // Decrypt the binary aes key
            char *tDecryptedKey = malloc(RSA_size(rsa) * sizeof(char)); // Need to Free Decrypted key
            //char tDecryptedKey[RSA_size(rsa)];
            if (RSA_private_decrypt(tKeySize, (unsigned char *)tDecodedAesKey,
                                    (unsigned char*) tDecryptedKey, rsa, RSA_PKCS1_OAEP_PADDING) >= 0)
            {
                slog(LOG_DEBUG, "Decrypted AES key from RSA Successfully\n");
            }
            else
            {
                slog(LOG_INFO, "Error Decrypting AES key from RSA\n");
            }
            free(tDecodedAesKey);
          //  RSA_free(rsa);

            setKeys(pConn->keys, tDecodedIV, tDecryptedKey, tFmtp);

            propogateCSeq(pConn);
        }
    }
    else if (!strncmp(pConn->recv.data, "SETUP", 5))
    {
        // Setup pipes
		// mono ////////////////////
		// send a PLAY_AIRPLAY message to m1ui2
		char mv_msg[256];
		int tSize = 0;
		char *tFound,* tDacp,*tActiveRemote;

		SocketClientReadWriteMsg("/tmp/Requesta01controller","setPlayerCmd:switchtoairplay", strlen("setPlayerCmd:switchtoairplay"),NULL,NULL,0);
		system("pkill imuzop");

		strcpy(mv_msg,"AIRPLAY_PLAY");
		///////////////////////////
		//huaijing send DACP ID and Active-Remote to apple remote
		tDacp = NULL;
	    tFound  =getFromContent(pConn->recv.data, "DACP-ID:", &tSize);
		if(tFound != NULL)
		{
			tDacp=malloc(tSize+1);
			memcpy(tDacp,tFound,tSize);
			tDacp[tSize]='\0';
		}

		tActiveRemote = NULL;
		tFound  =getFromContent(pConn->recv.data, "Active-Remote:", &tSize);
		if(tFound != NULL)
		{
			tActiveRemote=malloc(tSize+1);
			memcpy(tActiveRemote,tFound,tSize);
			tActiveRemote[tSize]='\0';
		}

		if(tDacp != NULL)
		{
			strcat(mv_msg,"DACP-ID:");
			strcat(mv_msg,tDacp);
			free(tDacp);
		}


		if(tActiveRemote != NULL)
		{
			strcat(mv_msg,"ACTIVE_REMOTE:");
			strcat(mv_msg,tActiveRemote);
			free(tActiveRemote);
		}
		char *user_agent_found = getFromContent(pConn->recv.data, "User-Agent", &tSize);
		SocketClientReadWriteMsg("/tmp/Requesta01controller",mv_msg,strlen(mv_msg),NULL,NULL,0);

		/////////////////////
        struct comms *tComms = pConn->hairtunes;
        if (! (pipe(tComms->in) == 0 && pipe(tComms->out) == 0))
        {
            slog(LOG_INFO, "Error setting up hairtunes communications...some things probably wont work very well.\n");
        }

        // Setup fork
        char dataPort[8] = "6001";  // get this from dup()'d stdout of child pid
        char controlPort[8] = "6001";
        char ntpPort[8] = "6002";
        int tDataport=0;
        int tControlport = 6001;
        int tTimingport = 6002;

		tFound	=getFromSetup(pConn->recv.data, "control_port", &tSize);
		if( tFound )
			getTrimmed(tFound, tSize, 1, 0, controlPort);
		tFound = getFromSetup(pConn->recv.data, "timing_port", &tSize);
		if( tFound )
			getTrimmed(tFound, tSize, 1, 0, ntpPort);

		slog(LOG_DEBUG_VV, "converting %s and %s from str->int\n", controlPort, ntpPort);
		tControlport = atoi(controlPort);
		tTimingport = atoi(ntpPort);

		slog(LOG_DEBUG_V, "Got %d for CPort and %d for TPort\n", tControlport, tTimingport);
		char *tRtp = NULL;

        int tPid = fork();
        if (tPid == 0)
        {

            prctl(PR_SET_PDEATHSIG, 15);


            char *tPipe = NULL;

            // *************************************************
            // ** Setting up Pipes, AKA no more debug/output  **
            // *************************************************
            dup2(tComms->in[0],0);   // Input to child
            closePipe(&(tComms->in[0]));
            closePipe(&(tComms->in[1]));

            dup2(tComms->out[1], 1); // Output from child
            closePipe(&(tComms->out[1]));
            closePipe(&(tComms->out[0]));

            struct keyring *tKeys = pConn->keys;
            pConn->keys = NULL;
            pConn->hairtunes = NULL;

			char *name=NULL;				// In the User-Agent header
			int version = 0;
			if(user_agent_found)
			{
				char *p = strchr(user_agent_found, '/');			// Extract name and version number
				int i=0;
				if(p)
				{
					*p = '\0';
					for(i=0; i<strlen(user_agent_found);++i)
					{
						if( *(user_agent_found+i) != ' ' )
							break;
					}
					name = user_agent_found+i;
					p = p+1;
					version = atoi(p);
					slog(LOG_DEBUG, " user_agent_found	name=[%s] version=%d\n", name, version);
					player_set_device_type( name, version); // set the device type
				}
				else
				{
					player_set_device_type( "unknow", -1); // set the device type
				}
			}
			else
			{
				player_set_device_type( "unknow", -1); // set the device type
			}

            // Free up any recv buffers, etc..
            if (pConn->clientSocket != -1)
            {
                close(pConn->clientSocket);
                pConn->clientSocket = -1;
            }
            cleanupBuffers(pConn);

            debug(1, "==airplay child process(hairtunes)%d started ==\n", getpid());
            hairtunes_init(tKeys->aeskey, tKeys->aesiv, tKeys->fmt, tControlport, tTimingport,
                           tDataport, tRtp, tPipe, tAoDriver, tAoDeviceName, tAoDeviceId,
                           bufferStartFill);

            // Quit when finished.
            slog(LOG_DEBUG, "Returned from hairtunes init....returning -1, should close out this whole side of the fork\n");
            debug(1, "==airplay child process(hairtunes)%d exit ==\n", getpid());
            exit(-1);
        }
        else if (tPid >0)
        {
            // Ensure Connection has access to the pipe.
            closePipe(&(tComms->in[0]));
            closePipe(&(tComms->out[1]));

            char tFromHairtunes[80];
			memset(tFromHairtunes,0x0,80);
            int tRead = read(tComms->out[0], tFromHairtunes, 79);
            if (tRead <= 0)
            {
                slog(LOG_INFO, "Error reading port from hairtunes function, assuming default port: %d\n", tDataport);
            }
            else
            {
                int tSize = 0;
                char *tPortStr = getFromHeader(tFromHairtunes, "dataport", &tSize);
                if (tPortStr != NULL)
                {
                    if( 3 == sscanf(tPortStr, "%d %d %d", &tDataport, &tControlport, &tTimingport) )
                    {
                        sprintf(&dataPort, "%d", tDataport);
                        sprintf(&controlPort, "%d", tControlport);
                        sprintf(&ntpPort, "%d", tTimingport);
                    }
                }
            }
            //  READ Ports from here?close(pConn->hairtunes_pipes[0]);
            propogateCSeq(pConn);
            int tSize = 0;
            char *tTransport = getFromHeader(pConn->recv.data, "Transport", &tSize);
            addToShairBuffer(&(pConn->resp), "Transport: ");
			#if 0
		    char *p = NULL;
	   	    if(tSize > 0 && (p=strstr(tTransport,"control_port=")) != NULL)
	    	{
				addNToShairBuffer(&(pConn->resp), tTransport, p-tTransport-1);
	    	}else
	    	{
            	addNToShairBuffer(&(pConn->resp), tTransport, tSize);
	    	}
			#else
			addToShairBuffer(&(pConn->resp), "RTP/AVP/UDP;unicast;interleaved=0-1;mode=record");
			#endif
            addToShairBuffer(&(pConn->resp), ";control_port=");
            addToShairBuffer(&(pConn->resp), controlPort);
            addToShairBuffer(&(pConn->resp), ";timing_port=");
            addToShairBuffer(&(pConn->resp), ntpPort);
            addToShairBuffer(&(pConn->resp), ";server_port=");
            addToShairBuffer(&(pConn->resp), controlPort);
            addToShairBuffer(&(pConn->resp), "\r\nSession: DEADBEEF\r\n");
            debug(1, "==control_port=%s, timing_port=%s, server_port=%s\n", controlPort, ntpPort, dataPort);
        }
        else
        {
            slog(LOG_INFO, "Error forking process....dere' be errors round here.\n");
            return -1;
        }
    }
    else if (!strncmp(pConn->recv.data, "TEARDOWN", 8))
    {
        // Be smart?  Do more finish up stuff...
        addToShairBuffer(&(pConn->resp), "Connection: close\r\n");
        propogateCSeq(pConn);
        //close(pConn->hairtunes->in[1]);
        slog(LOG_DEBUG, "Tearing down connection, closing pipes\n");
        //close(pConn->hairtunes->out[0]);
        tReturn = -1;  // Close client socket, but sends an ACK/OK packet first
    }
    else if (!strncmp(pConn->recv.data, "FLUSH", 5))
    {
    	int tSize = 0;
	    int seq = 0;
	    unsigned long rtp_tsp;
	    unsigned long rtptime = 0;
	    char temp[64];


		char *hdr = getFromSetup(pConn->recv.data, "RTP-Info", &tSize);
	    if (hdr)
	    {
		    char *p;
		    p = strstr(hdr, "seq=");
		    if (p)
		    {
			    p = strchr(p, '=') + 1;
			    seq = atoi(p);
			    p = strstr(hdr, "rtptime=");
			    if (p)
			    {
				    p = strchr(p, '=') + 1;
				    rtptime = strtoul(p, NULL, 0);
				    debug(3, "Received seq: %04X, rtptime: %lu\n", seq, rtptime);
#if 0
				    char *resphdr = malloc(32);
				    sprintf(resphdr, "rtptime=%lu", rtp_tsp);
				    debug(1, "Reporting RTP-Info: %s\n", resphdr);
					if( rtp_tsp )
						addToShairBuffer(&(pConn->resp), resphdr);
#endif
			    }
		    }
	    }
		sprintf(temp, "flush_rtp:-1:%lu:%lu\n", seq, rtptime);
		if( rtptime )
		{
			debug(3, "Received seq: %04X, rtptime: %lu[%s]\n", seq, rtptime, temp);
			write(pConn->hairtunes->in[1], temp, strlen(temp));
			fsync(pConn->hairtunes->in[1]);
		}
		else
		{
			debug(3, "!!!Flush Received seq: %04X, rtptime not found\n", seq);
			write(pConn->hairtunes->in[1], "flush\n", 6);
			fsync(pConn->hairtunes->in[1]);
		}
        propogateCSeq(pConn);
    }
    else if (!strncmp(pConn->recv.data, "SET_PARAMETER", 13))
    {
        propogateCSeq(pConn);
        int tSize = 0;
        char *contentType = getFromHeader(pConn->recv.data, "Content-Type", &tSize);
        char *tVol = getFromHeader(pConn->recv.data, "volume", &tSize);
		char *dmap = getFromHeader(pConn->recv.data, "application/x-dmap-tagged", &tSize);
		//char *jpeg = getFromHeader(pConn->recv.data, "image/jpeg", &tSize);
		char *tProcess = getFromHeader(pConn->recv.data, "progress", &tSize);
		static int ablum_cover_got = 0;
        slog(LOG_DEBUG_VV, "About to write [vol: %.*s] data to hairtunes\n", tSize, tVol);

		if( contentType && !strncmp("image/jpeg", contentType, 10))
		{
			char *contentLen = getFromHeader(pConn->recv.data, "Content-Length", &tSize);
			int len = 0;
			int ret = 0;
			if( contentLen )
				len = atoi(contentLen);
			// got image before meta data
			if( dmap )
				ablum_cover_send = 0;
			//wiimu_log(1, 0, 0, 0,"AIRPLAY dmap recved for album cover";
			debug(3, "%s %d ====ablum_cover_send=%d contentLen:[%d]%s ==\n",
				__FUNCTION__, __LINE__, ablum_cover_send, len, contentLen);
			if( len > 0 )
			{
				system("rm -f " AIRPLAY_ALBUM_PATH );
				int fd = open(AIRPLAY_ALBUM_PATH, O_RDWR | O_TRUNC | O_CREAT, 0644);
				char *tContent = pConn->recv.data + pConn->recv.marker;
				int left = len;
				if( fd >= 0 )
				{
					while(left > 0 )
					{
						ret = write(fd, tContent, len);
						if( ret <= 0 )
							break;
						left -= ret;
					}
					close(fd);

					if( left == 0 )
					{
						ablum_cover_got = 1;
					}

					debug(3, "%s %d write jpeg file, len=%d/%d left=%d ablum_cover_got=%d\n",
						__FUNCTION__, __LINE__, ret, len, left, ablum_cover_got);
				}
				else
					debug(1, "%s %d write jpeg file failed\n", __FUNCTION__, __LINE__);

			}

			// refresh ablum cover
			if( ablum_cover_send && ablum_cover_got )
			{
				set_album_path(AIRPLAY_ALBUM_PATH, &_airplay_notify.album_uri);
				debug(3, "%s %d refresh album %s\n", __FUNCTION__, __LINE__, _airplay_notify.album_uri);
				SocketClientReadWriteMsg("/tmp/Requesta01controller", &_airplay_notify,sizeof(airplay_notify),NULL,NULL,0);
			}
		}

		if(tVol)
		{
			double f = atof(tVol);
            int hwVol;
			char sendBuf[32];
            if(f<-30.0)
            {
                hwVol=0;
            }
            else
            {
               hwVol=(f+30) *100 / 30;
            }

            if(hwVol>100)
            {
                hwVol=100;
            }
			if( tickSincePowerOn()-g_wiimu_shm->airplay_volume_notify_ts > 3000)
			{
	            sprintf(sendBuf,"setPlayerCmd:airplay_vol:%d", hwVol);
	            SocketClientReadWriteMsg("/tmp/Requesta01controller",sendBuf,strlen(sendBuf),NULL,NULL,0);
			}
		}
		else if(dmap)
		{
			struct dmap_t* pdmap_t = NULL;
			char *songtitle;
			char *songartis;
			char *songalbum;
			airplay_notify *pairplay_notify=&_airplay_notify;
			ablum_cover_send = 0;

			dmap_p dmap = dmap_create();
			dmap_parse(dmap, pConn->recv.data+pConn->recv.marker, pConn->recv.markend-pConn->recv.marker);

			pdmap_t = dmap_container_at_index(dmap,0);
			songtitle = (char *)dmap_string_for_atom_identifer(pdmap_t, "dmap.itemname");
			songartis = (char *)dmap_string_for_atom_identifer(pdmap_t, "daap.songartist");
			songalbum = (char *)dmap_string_for_atom_identifer(pdmap_t, "daap.songalbum");

			memset(pairplay_notify,0,sizeof(airplay_notify));

			strcpy(pairplay_notify->notify,"AIRPLAY_MEDADATA");

			if(songtitle != NULL)
			  strcpy(pairplay_notify->title,songtitle);
			else
			  strcpy(pairplay_notify->title,"unknown");

			if(songartis != NULL)
			  strcpy(pairplay_notify->artist,songartis);
			else
			   strcpy(pairplay_notify->artist,"unknown");

			if(songalbum != NULL)
			  strcpy(pairplay_notify->album,songalbum);
			else
			   strcpy(pairplay_notify->album,"unknown");

			// app must add device url prefix, example "http://10.10.10.254/data/airplay.jpg"
			if(ablum_cover_got == 1)
			{
				//strcpy(pairplay_notify->album_uri, "/data/airplay.jpg");
				set_album_path(AIRPLAY_ALBUM_PATH, &pairplay_notify->album_uri);
			}
			else
				strcpy(pairplay_notify->album_uri,"unknown");

			//wiimu_log(1, 0, 0,0, "AIRPLAY dmap recved for metadata";
			//fprintf(stderr,"  itemname %s ablum_cover_got=%d[%s]\n",
			//	(char *)dmap_string_for_atom_identifer(pdmap_t, "dmap.itemname"), ablum_cover_got, pairplay_notify->album_uri);
			//fprintf(stderr,"  songartist %s \n", (char *)dmap_string_for_atom_identifer(pdmap_t, "daap.songartist"));
			//fprintf(stderr,"  songalbum %s \n", (char *)dmap_string_for_atom_identifer(pdmap_t, "daap.songalbum"));



			dmap_destroy(dmap);

			SocketClientReadWriteMsg("/tmp/Requesta01controller",pairplay_notify,sizeof(airplay_notify),NULL,NULL,0);
			ablum_cover_send = 1;
			ablum_cover_got = 0;

		}
		else if(tProcess)
		{
			char szStart[16]={0};
			char szCurr[16]={0};
			char szEnd[16]={0};
			char *p=NULL;
			long long lstart,lcur,lend;
			airplay_notify *pairplay_notify=(airplay_notify *)malloc(sizeof(airplay_notify));
			//fprintf(stderr," tProcess  %s \n",  tProcess);
			p=strstr(tProcess,"/");
			if(p)
			{
				memcpy(szStart,tProcess,p-tProcess);
				szStart[p-tProcess]='\0';
				tProcess=p+1;
				p=strstr(tProcess,"/");
				if(p)
				{
					memcpy(szCurr,tProcess,p-tProcess);
					szCurr[p-tProcess]='\0';
					tProcess=p+1;
					if(strlen(tProcess) < sizeof(szEnd)-1)
					{
						memcpy(szEnd,tProcess,strlen(tProcess));
						szEnd[strlen(tProcess)]='\0';
					}
				}
			}
			memset(pairplay_notify,0,sizeof(airplay_notify));
			strcpy(pairplay_notify->notify,"AIRPLAY_POSITION");

			lstart=atoll(szStart);
			lcur=atoll(szCurr);
			lend=atoll(szEnd);

			//fprintf(stderr," start  %lld \n", lstart);
			//fprintf(stderr," current  %lld \n", lcur);
			//fprintf(stderr," end  %lld \n", lend);

			pairplay_notify->lposition = (lcur-lstart)/44100*1000;
			pairplay_notify->lduration = (lend-lstart)/44100*1000;

			SocketClientReadWriteMsg("/tmp/Requesta01controller",pairplay_notify,sizeof(airplay_notify),NULL,NULL,0);

			free(pairplay_notify);

		}
		//else if(jpeg)
		//{
		//	fprintf(stderr," save  airplay.jpeg \n");
		//	FILE *file=fopen("/tmp/airplay.jpeg","w+");
		//	fwrite(pConn->recv.data+pConn->recv.marker,1,pConn->recv.markend-pConn->recv.marker,file);
		//	fclose(file);
		//	sync();
		//}
        slog(LOG_DEBUG_VV, "Finished writing data write data to hairtunes\n");
    }
	else if (!strncmp(pConn->recv.data, "GET_PARAMETER", 13))
	{
		char *pBodyStart = strstr(pConn->recv.data,"\r\n\r\n");
		if(pBodyStart)
		{
			pBodyStart+=4;
			if(strnicmpx(pBodyStart,strlen("volume"),"volume") == 0)
			{
				char temp[64]= {0};
				propogateCSeq(pConn);
				need_body = 1;

				int n = snprintf( bodybuf, sizeof( bodybuf ), "volume: %f\r\n", (float)(g_wiimu_shm->volume*30)/(float)100-(float)30.0);
				addToShairBuffer(&(pConn->resp), "Content-Type: text/parameters\r\n");
				sprintf(temp,"Content-Length: %d \r\n",n);
				addToShairBuffer(&(pConn->resp), temp);
				need_body = 1;
			}else if(strnicmpx(pBodyStart,strlen("name"),"name") == 0)
			{
				char temp[64]= {0};
				propogateCSeq(pConn);
				need_body = 1;
				int n = snprintf( bodybuf, sizeof( bodybuf ), "name: %s\r\n", g_ServerName);
				addToShairBuffer(&(pConn->resp), "Content-Type: text/parameters\r\n");
				sprintf(temp,"Content-Length: %d \r\n",n);
				addToShairBuffer(&(pConn->resp), temp);
				need_body = 1;
			}
		}

	}
	else if (!strncmp(pConn->recv.data, "RECORD", 6))
	{
		char *pBodyStart = strstr(pConn->recv.data,"\r\n\r\n");
		char temp[64]= {0};
		int seq = 0;
		unsigned long rtptime = 0;
		int rtp_mode = 0;
		int tSize = 0;
		char *hdr = getFromHeader(pConn->recv.data, "RTP-Info", &tSize);
		if (hdr) {
			char *p;
			p = strstr(hdr, "seq=");
 			if (p)
 			{
				p = strchr(p, '=') + 1;
				seq = atoi(p);
				p = strstr(hdr, "rtptime=");
				if (p)
				{
					p = strchr(p, '=') + 1;
					rtptime = strtoul(p, NULL, 0);
					rtp_mode = 1;
				}
 			}
		}
		propogateCSeq(pConn);
		need_body = 1;
		int n = snprintf( bodybuf, sizeof( bodybuf ), "Audio-Latency: %d\r\n", platform_get_audio_latency_us()/1000);
		addToShairBuffer(&(pConn->resp), "Content-Type: text/parameters\r\n");
		sprintf(temp,"Content-Length: %d \r\n",n);
		// workaround for TuneBlade
		char *user_agent = getFromHeader(pConn->recv.data, "User-Agent", &tSize);
		if(user_agent && strstr(user_agent, "TuneBlade"))
		{
			need_body = 0;
			rtp_mode = 0;
			addToShairBuffer(&(pConn->resp), bodybuf);
		}
		else
			addToShairBuffer(&(pConn->resp), temp);
		sprintf(temp, "flush_rtp:%d:%lu:%lu\n", rtp_mode, seq, rtptime);
		if( rtptime )
		write(pConn->hairtunes->in[1], temp, strlen(temp));
		slog(LOG_INFO, "RECORD rtptime=%d, rtp_mode=%d[%s]\n", rtptime, rtp_mode, pConn->recv.data);
	}
    else
    {
        slog(LOG_DEBUG, "\n\nUn-Handled recv: %s\n", pConn->recv.data);
        propogateCSeq(pConn);
    }

    addToShairBuffer(&(pConn->resp), "\r\n");

	if(need_body)
	{
		addToShairBuffer(&(pConn->resp),bodybuf);
	}

    return tReturn;
}

// Copies CSeq value from request, and adds standard header values in.
static void propogateCSeq(struct connection *pConn) //char *pRecvBuffer, struct shairbuffer *pConn->recp.data)
{
    int tSize=0;
    char *tRecPtr = getFromHeader(pConn->recv.data, "CSeq", &tSize);
    addToShairBuffer(&(pConn->resp), "Audio-Jack-Status: connected; type=analog\r\n");
    addToShairBuffer(&(pConn->resp), "CSeq: ");
    addNToShairBuffer(&(pConn->resp), tRecPtr, tSize);
    addToShairBuffer(&(pConn->resp), "\r\n");
}

void cleanupBuffers(struct connection *pConn)
{
    if (pConn->recv.data != NULL)
    {
        free(pConn->recv.data);
        pConn->recv.data = NULL;
    }
    if (pConn->resp.data != NULL)
    {
        free(pConn->resp.data);
        pConn->resp.data = NULL;
    }
}
static void cleanup(struct connection *pConn)
{
    cleanupBuffers(pConn);
    if (pConn->hairtunes != NULL)
    {

        closePipe(&(pConn->hairtunes->in[0]));
        closePipe(&(pConn->hairtunes->in[1]));
        closePipe(&(pConn->hairtunes->out[0]));
        closePipe(&(pConn->hairtunes->out[1]));
    }
    if (pConn->keys != NULL)
    {
        if (pConn->keys->aesiv != NULL)
        {
            free(pConn->keys->aesiv);
        }
        if (pConn->keys->aeskey != NULL)
        {
            free(pConn->keys->aeskey);
        }
        if (pConn->keys->fmt != NULL)
        {
            free(pConn->keys->fmt);
        }
        pConn->keys = NULL;
    }
    if (pConn->clientSocket != -1)
    {
        fsync(pConn->clientSocket);
        debug(1, "==airplay %s close client %d, wait 100ms\n", __FUNCTION__, pConn->clientSocket);
        usleep(100000);
        close(pConn->clientSocket);
        pConn->clientSocket = -1;
    }
	SocketClientReadWriteMsg("/tmp/Requesta01controller","AIRPLAY_STOP",strlen("AIRPLAY_STOP"),NULL,NULL,0);

}
//static char audioServiceType[] = "_raop._tcp.";
//static char* audioServiceText[]= {"tp=UDP","sm=false","sv=false","ek=1","md=0,2","et=0,1","cn=0,1","ch=2","ss=16","sr=44100","pw=false","vn=65537","txtvers=1"};
//static int audioServiceTextLen = 13;
static int mDNS_InstanceID = -1 ;
static char audioServiceType[] = "_raop._tcp.";
static char* audioServiceText[]= {"txtvers=1","ch=2","cn=1","ek=1","da=true","sm=false","et=0,1","md=0,1,2","pw=false","sv=false","sr=44100","ss=16","tp=UDP","vn=65537","vs=115.2"};
static int audioServiceTextLen = 15;
int mDNS_LoggingEnabled=0;
int mDNS_PacketLoggingEnabled=0;
int mDNS_DebugMode=0;          // If non-zero, LogMsg() writes to stderr instead of syslog

static  int StopAdvertisement()
{
	debug(1, "===%s %d ===\n", __FUNCTION__, __LINE__);
	mDMS_DeregisterServices();
	//mDNS_release(mDNS_InstanceID);
	return 0;
}

static char* SpotifyText[]= {"VERSION=1.0","CPath=/goform/spotifyConnect"};

static int StartAdvertisement(const char *pHWStr, const char *pServerName, int pPort)
{
	 int tMaxServerName = 50;
        char tName[100 + HWID_SIZE + 3]={0};
        char tPort[SERVLEN];
        strncat(tName,pHWStr,32);
        strcat(tName, "@");
        strncat(tName, pServerName, tMaxServerName);
	debug(1, "===%s %d [%s]===\n", __FUNCTION__, __LINE__, tName);
	if( mDNS_InstanceID == -1 )
		mDNS_InstanceID = mDNS_init();
	if(!only_spotify_server)
	  mDNS_publishOneService(tName,audioServiceType,pPort,audioServiceText,audioServiceTextLen);
#ifndef SPOTIFY_MODULE_REMOVE
	//mDNS_publishOneService(tName,"_spotify-connect._tcp",5356,SpotifyText,2);
#endif
	return 0;
}
/*
static int startAvahi(const char *pHWStr, const char *pServerName, int pPort)
{
    int tMaxServerName = 25; // Something reasonable?  iPad showed 21, iphone 25
    int tPid = fork();
    if (tPid == 0)
    {
        char tName[100 + HWID_SIZE + 3];
        prctl(PR_SET_PDEATHSIG, 15);
        if (strlen(pServerName) > tMaxServerName)
        {
            slog(LOG_INFO,"Hey dog, we see you like long server names, "
                 "so we put a strncat in our command so we don't buffer overflow, while you listen to your flow.\n"
                 "We just used the first %d characters.  Pick something shorter if you want\n", tMaxServerName);
        }

        tName[0] = '\0';
        char tPort[SERVLEN];
        sprintf(tPort, "%d", pPort);
        strcat(tName, pHWStr);
        strcat(tName, "@");
        strncat(tName, pServerName, tMaxServerName);
        slog(AVAHI_LOG_LEVEL, "Avahi/DNS-SD Name: %s\n", tName);

	mDNS_InstanceID = mDNS_init();
	mDNS_publishOneService(tName,audioServiceType,pPort,audioServiceText,audioServiceTextLen);
#if 0
        execlp("avahi-publish-service", "avahi-publish-service", tName,
               "_raop._tcp", tPort, "tp=UDP","sm=false","sv=false","ek=1","et=0,1",
               "cn=0,1","ch=2","ss=16","sr=44100","pw=false","vn=3","txtvers=1", NULL);
        execlp("dns-sd", "dns-sd", "-R", tName,
               "_raop._tcp", ".", tPort, "tp=UDP","sm=false","sv=false","ek=1","et=0,1",
               "cn=0,1","ch=2","ss=16","sr=44100","pw=false","vn=3","txtvers=1", NULL);
#endif
		sleep(1);
        execlp("mDNSPublish", "mDNSPublish", tName,
               "_raop._tcp", tPort,
               "tp=UDP","sm=false","sv=false","ek=1","md=0,2","et=0,1","cn=0,1","ch=2","ss=16","sr=44100","pw=false","vn=65537","txtvers=1", NULL);
		// md 0 dmap , 1 jpeg, 2 progress
        if (errno == -1)
        {
            perror("error");
        }

        slog(LOG_INFO, "Bad error... couldn't find or failed to run: avahi-publish-service OR dns-sd OR mDNSPublish\n");
        exit(1);
    }
    else
    {
        slog(LOG_DEBUG_VV, "Avahi/DNS-SD started on PID: %d\n", tPid);
    }
    return tPid;
}
*/
static int getAvailChars(struct shairbuffer *pBuf)
{
    return(pBuf->maxsize / sizeof(char)) - pBuf->current;
}

static void addToShairBuffer(struct shairbuffer *pBuf, char *pNewBuf)
{
    addNToShairBuffer(pBuf, pNewBuf, strlen(pNewBuf));
}

static void addNToShairBuffer(struct shairbuffer *pBuf, char *pNewBuf, int pNofNewBuf)
{
    int tAvailChars = getAvailChars(pBuf);
    if (pNofNewBuf > tAvailChars)
    {
        int tNewSize = pBuf->maxsize * 2 + MAX_SIZE + sizeof(char);
        char *tTmpBuf = malloc(tNewSize);

        tTmpBuf[0] = '\0';
        memset(tTmpBuf, 0, tNewSize/sizeof(char));
        memcpy(tTmpBuf, pBuf->data, pBuf->current);
        free(pBuf->data);

        pBuf->maxsize = tNewSize;
        pBuf->data = tTmpBuf;
    }
    memcpy(pBuf->data + pBuf->current, pNewBuf, pNofNewBuf);
    pBuf->current += pNofNewBuf;
    if (getAvailChars(pBuf) > 1)
    {
        pBuf->data[pBuf->current] = '\0';
    }
}

static char *getTrimmedMalloc(char *pChar, int pSize, int pEndStr, int pAddNL)
{
    int tAdditionalSize = 0;
    if (pEndStr)
        tAdditionalSize++;
    if (pAddNL)
        tAdditionalSize++;
    char *tTrimDest = malloc(sizeof(char) * (pSize + tAdditionalSize));
    return getTrimmed(pChar, pSize, pEndStr, pAddNL, tTrimDest);
}

// Must free returned ptr
static char *getTrimmed(char *pChar, int pSize, int pEndStr, int pAddNL, char *pTrimDest)
{
    int tSize = pSize;
    if (pEndStr)
    {
        tSize++;
    }
    if (pAddNL)
    {
        tSize++;
    }

    memset(pTrimDest, 0, tSize);
    memcpy(pTrimDest, pChar, pSize);
    if (pAddNL)
    {
        pTrimDest[pSize] = '\n';
    }
    if (pEndStr)
    {
        pTrimDest[tSize-1] = '\0';
    }
    return pTrimDest;
}

void slog(int pLevel, char *pFormat, ...)
{
#ifdef SHAIRPORT_LOG
    if (isLogEnabledFor(pLevel))
    {
        va_list argp;
        va_start(argp, pFormat);
        vprintf(pFormat, argp);
        va_end(argp);
    }
#endif
}

static int isLogEnabledFor(int pLevel)
{
    if (pLevel <= kCurrentLogLevel)
    {
        return TRUE;
    }
    return FALSE;
}

static void initConnection(struct connection *pConn, struct keyring *pKeys,
                           struct comms *pComms, int pSocket, char *pPassword)
{
    pConn->hairtunes = pComms;
    if (pKeys != NULL)
    {
        pConn->keys = pKeys;
        pConn->keys->aesiv = NULL;
        pConn->keys->aeskey = NULL;
        pConn->keys->fmt = NULL;
    }
    pConn->recv.data = NULL;  // Pre-init buffer expected to be NULL
    pConn->resp.data = NULL;  // Pre-init buffer expected to be NULL
    pConn->clientSocket = pSocket;
    if (strlen(pPassword) >0)
    {
        pConn->password = pPassword;
    }
    else
    {
        pConn->password = NULL;
    }
}

static void closePipe(int *pPipe)
{
    if (*pPipe != -1)
    {
        close(*pPipe);
        *pPipe = -1;
    }
}

static void initBuffer(struct shairbuffer *pBuf, int pNumChars)
{
    if (pBuf->data != NULL)
    {
        slog(LOG_DEBUG_VV, "Hrm, buffer wasn't cleaned up....trying to free\n");
        free(pBuf->data);
        slog(LOG_DEBUG_VV, "Free didn't seem to seg fault....huzzah\n");
    }
    pBuf->current = 0;
    pBuf->marker = 0;
	pBuf->markend = 0;
    pBuf->maxsize = sizeof(char) * pNumChars;
    pBuf->data = malloc(pBuf->maxsize);
    memset(pBuf->data, 0, pBuf->maxsize);
}

static void setKeys(struct keyring *pKeys, char *pIV, char* pAESKey, char *pFmtp)
{
    if (pKeys->aesiv != NULL)
    {
        free(pKeys->aesiv);
    }
    if (pKeys->aeskey != NULL)
    {
        free(pKeys->aeskey);
    }
    if (pKeys->fmt != NULL)
    {
        free(pKeys->fmt);
    }
    pKeys->aesiv = pIV;
    pKeys->aeskey = pAESKey;
    pKeys->fmt = pFmtp;
}

#define AIRPORT_PRIVATE_KEY \
"-----BEGIN RSA PRIVATE KEY-----\n" \
"MIIEpQIBAAKCAQEA59dE8qLieItsH1WgjrcFRKj6eUWqi+bGLOX1HL3U3GhC/j0Qg90u3sG/1CUt\n" \
"wC5vOYvfDmFI6oSFXi5ELabWJmT2dKHzBJKa3k9ok+8t9ucRqMd6DZHJ2YCCLlDRKSKv6kDqnw4U\n" \
"wPdpOMXziC/AMj3Z/lUVX1G7WSHCAWKf1zNS1eLvqr+boEjXuBOitnZ/bDzPHrTOZz0Dew0uowxf\n" \
"/+sG+NCK3eQJVxqcaJ/vEHKIVd2M+5qL71yJQ+87X6oV3eaYvt3zWZYD6z5vYTcrtij2VZ9Zmni/\n" \
"UAaHqn9JdsBWLUEpVviYnhimNVvYFZeCXg/IdTQ+x4IRdiXNv5hEewIDAQABAoIBAQDl8Axy9XfW\n" \
"BLmkzkEiqoSwF0PsmVrPzH9KsnwLGH+QZlvjWd8SWYGN7u1507HvhF5N3drJoVU3O14nDY4TFQAa\n" \
"LlJ9VM35AApXaLyY1ERrN7u9ALKd2LUwYhM7Km539O4yUFYikE2nIPscEsA5ltpxOgUGCY7b7ez5\n" \
"NtD6nL1ZKauw7aNXmVAvmJTcuPxWmoktF3gDJKK2wxZuNGcJE0uFQEG4Z3BrWP7yoNuSK3dii2jm\n" \
"lpPHr0O/KnPQtzI3eguhe0TwUem/eYSdyzMyVx/YpwkzwtYL3sR5k0o9rKQLtvLzfAqdBxBurciz\n" \
"aaA/L0HIgAmOit1GJA2saMxTVPNhAoGBAPfgv1oeZxgxmotiCcMXFEQEWflzhWYTsXrhUIuz5jFu\n" \
"a39GLS99ZEErhLdrwj8rDDViRVJ5skOp9zFvlYAHs0xh92ji1E7V/ysnKBfsMrPkk5KSKPrnjndM\n" \
"oPdevWnVkgJ5jxFuNgxkOLMuG9i53B4yMvDTCRiIPMQ++N2iLDaRAoGBAO9v//mU8eVkQaoANf0Z\n" \
"oMjW8CN4xwWA2cSEIHkd9AfFkftuv8oyLDCG3ZAf0vrhrrtkrfa7ef+AUb69DNggq4mHQAYBp7L+\n" \
"k5DKzJrKuO0r+R0YbY9pZD1+/g9dVt91d6LQNepUE/yY2PP5CNoFmjedpLHMOPFdVgqDzDFxU8hL\n" \
"AoGBANDrr7xAJbqBjHVwIzQ4To9pb4BNeqDndk5Qe7fT3+/H1njGaC0/rXE0Qb7q5ySgnsCb3DvA\n" \
"cJyRM9SJ7OKlGt0FMSdJD5KG0XPIpAVNwgpXXH5MDJg09KHeh0kXo+QA6viFBi21y340NonnEfdf\n" \
"54PX4ZGS/Xac1UK+pLkBB+zRAoGAf0AY3H3qKS2lMEI4bzEFoHeK3G895pDaK3TFBVmD7fV0Zhov\n" \
"17fegFPMwOII8MisYm9ZfT2Z0s5Ro3s5rkt+nvLAdfC/PYPKzTLalpGSwomSNYJcB9HNMlmhkGzc\n" \
"1JnLYT4iyUyx6pcZBmCd8bD0iwY/FzcgNDaUmbX9+XDvRA0CgYEAkE7pIPlE71qvfJQgoA9em0gI\n" \
"LAuE4Pu13aKiJnfft7hIjbK+5kyb3TysZvoyDnb3HOKvInK7vXbKuU4ISgxB2bB3HcYzQMGsz1qJ\n" \
"2gG0N5hvJpzwwhbhXqFKA4zaaSrw622wDniAK5MlIE0tIAKKP4yxNGjoD2QYjhBGuhvkWKY=\n" \
"-----END RSA PRIVATE KEY-----"

static RSA *loadKey(void)
{
    BIO *tBio = BIO_new_mem_buf(AIRPORT_PRIVATE_KEY, -1);
    RSA *rsa = PEM_read_bio_RSAPrivateKey(tBio, NULL, NULL, NULL); //NULL, NULL, NULL);
    BIO_free(tBio);
    slog(RSA_LOG_LEVEL, "RSA Key: %d\n", RSA_check_key(rsa));
    return rsa;
}

void *cmdSocket_airplay(void *arg)
{
	int recv_num;
	char recv_buf[4096];
	int msg, ret;
	SOCKET_CONTEXT msg_socket;

	msg_socket.path="/tmp/RequestAirplay";
	msg_socket.blocking = 1;

RE_INIT:
	ret=UnixSocketServer_Init(&msg_socket);
	//printf("Airplay UnixSocketServer_Init return %d\n", ret);
	if(ret < 0)
	{
		debug(1, "Airplay UnixSocketServer_Init fail\n");
		sleep(5);
		goto RE_INIT;
	}

	while(1)
	{
		ret = UnixSocketServer_accept(&msg_socket);
		if(ret <=0)
		{
			debug(1, "Airplay UnixSocketServer_accept fail\n");
			if(ret < 0)
			{
				UnixSocketServer_deInit(&msg_socket);
				sleep(5);
				goto RE_INIT;
			}
		}
		else
	    {
			memset(recv_buf,0x0,sizeof(recv_buf));
			recv_num = UnixSocketServer_read(&msg_socket,recv_buf,sizeof(recv_buf));
			if(recv_num <= 0)
			{
				debug(1, "Airplay UnixSocketServer_recv failed\r\n");
				if(recv_num < 0)
				{
					UnixSocketServer_close(&msg_socket);
					UnixSocketServer_deInit(&msg_socket);
					sleep(5);
					goto RE_INIT;
				}
			}
			else
			{
				recv_buf[recv_num] = 0;
				debug(1, "airplay recv local msg[%s]\n", recv_buf);
				if(strncmp(recv_buf, "IPChanged", strlen("IPChanged")) ==0)
				{
					StopAdvertisement();
					StartAdvertisement(g_hwstr,g_ServerName,g_port);
				}else if(strncmp(recv_buf,"NameChanged",strlen("NameChanged"))==0)
				{
					memset(g_ServerName,0x0,sizeof(g_ServerName));
					strncpy(g_ServerName,recv_buf+strlen("NameChanged:"),sizeof(g_ServerName)-1);
					StopAdvertisement();
					StartAdvertisement(g_hwstr,g_ServerName,g_port);

				}else if(strncmp(recv_buf,"PowerOff",strlen("PowerOff"))==0)
				{
					StopAdvertisement();
				}
				else if(strncmp(recv_buf, "MdnsIPChanged", strlen("MdnsIPChanged")) ==0)
				{
					StopAdvertisement();
					StartAdvertisement(g_hwstr,g_ServerName,g_port);
				}

			}
			UnixSocketServer_close(&msg_socket);
		}
	}

	return NULL;
}

