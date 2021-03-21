/*
 * routines (with C-linkage) that interface between MPlayer
 * and the "LIVE555 Streaming Media" libraries
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

extern "C" {
// on MinGW, we must include windows.h before the things it conflicts
#ifdef __MINGW32__    // with.  they are each protected from
#include <windows.h>  // windows.h, but not the other way around.
#endif
#include "mp_msg.h"
#include "demuxer.h"
#include "demux_rtp.h"
#include "stheader.h"
#include <signal.h>
#include <unistd.h>
}
#include "demux_rtp_internal.h"

#include "BasicUsageEnvironment.hh"
#include "liveMedia.hh"
#include "GroupsockHelper.hh"



static struct sessionState_t {
  FramedSource* source;
  RTPSink* sink;
  RTCPInstance* rtcpInstance;
  Groupsock* rtpGroupsock;
  Groupsock* rtcpGroupsock;
  RTSPServer* rtspServer;
} sessionState;


static UsageEnvironment* env;
static int rtsp_init = 0;

void ao_mp3rtsp_afterPlaying(void* /*clientData*/) {
  // End by closing the media:
    	printf("ao_mp3rtsp_afterPlaying  rtsp_init %d \n",rtsp_init);
  if(rtsp_init)
  {

	  Medium::close(sessionState.rtspServer);
	  Medium::close(sessionState.rtcpInstance);
	  Medium::close(sessionState.sink);
	  delete sessionState.rtpGroupsock;
	  Medium::close(sessionState.source);
	  delete sessionState.rtcpGroupsock;
	  rtsp_init = 0;
	  printf(" cloudy  rtsp_init %d \n",rtsp_init);
  }
}


static void udpSend(char* IP,int Port,char* send_buf,int send_buf_len)
{
  /* socket文件描述符 */  
  int sock_fd;  
  struct sockaddr_in addr_serv;  
  int len;  
  int send_num;  
  
  /* 建立udp socket */  
  sock_fd = socket(AF_INET, SOCK_DGRAM, 0);  
  if(sock_fd < 0)  
  {  
    perror("socket");  
    return;  
  }  
    
  /* 设置address */  
  memset(&addr_serv, 0, sizeof(addr_serv));  
  addr_serv.sin_family = AF_INET;  
  addr_serv.sin_addr.s_addr = inet_addr(IP);  
  addr_serv.sin_port = htons(Port);  
  len = sizeof(addr_serv);
  
  send_num = sendto(sock_fd, send_buf, send_buf_len, 0, (struct sockaddr *)&addr_serv, len);   
  if(send_num < 0)  
  {  
    perror("sendto error:");    
  }  
  
  close(sock_fd);
}

static void demux_mp3rtsp_exit_sighandler(int x){
  // close stream
  printf("demux rtsp rtsp_exit_sighandler %d\n",x);
  ao_mp3rtsp_afterPlaying(NULL);
  exit(0);
}


extern "C" int ao_run_mp3rstp(char ** pbuffer,unsigned int * psize,
						unsigned int * pwprt,
						unsigned int * prprt,
						pthread_mutex_t* pmutex) {

	char sendBuf[512];

  do {
	TaskScheduler* scheduler = BasicTaskScheduler::createNew();
	env = BasicUsageEnvironment::createNew(*scheduler);

	printf("ao_run_mp3rstp psize %d  plen %d \n",*psize,*pwprt);
	
	signal(SIGTERM,demux_mp3rtsp_exit_sighandler);
	signal(SIGKILL,demux_mp3rtsp_exit_sighandler);
	signal(SIGINT,demux_mp3rtsp_exit_sighandler);
	signal(SIGABRT,demux_mp3rtsp_exit_sighandler);
//	signal(SIGALRM,demux_mp3rtsp_exit_sighandler);
    signal(SIGTERM, demux_mp3rtsp_exit_sighandler); // kill
   // signal(SIGHUP, demux_mp3rtsp_exit_sighandler); // kill -HUP  /  xterm closed
    signal(SIGINT, demux_mp3rtsp_exit_sighandler); // Interrupt from keyboard
    signal(SIGQUIT, demux_mp3rtsp_exit_sighandler); // Quit from keyboard
   // signal(SIGPIPE, demux_mp3rtsp_exit_sighandler); // Some window managers cause this
#ifdef CONFIG_SIGHANDLER
    // fatal errors:
    signal(SIGBUS, demux_mp3rtsp_exit_sighandler); // bus error
#ifndef __WINE__                      // hack: the Wine executable will crash else
//    signal(SIGSEGV, demux_mp3rtsp_exit_sighandler); // segfault
#endif
    signal(SIGILL, demux_mp3rtsp_exit_sighandler); // illegal instruction
 //   signal(SIGFPE, demux_mp3rtsp_exit_sighandler); // floating point exc.
    signal(SIGABRT, demux_mp3rtsp_exit_sighandler); // abort()
#endif

	printf("signal init  \n");

#if 1
		MplayerMP3Source* pcmSource
		  = MplayerMP3Source::createNew(*env,pbuffer,psize,pwprt,prprt,pmutex,0 );
		if (pcmSource == NULL) {
			printf("MplayerMP3Source createNew failed \n");
			return -1;
		}
	
		sessionState.source = pcmSource;
		rtsp_init = 1;
		 printf(" cloudy init rtsp_init %d \n",rtsp_init);

#if 0
		sessionState.source = EndianSwap16::createNew(*env, pcmSource);
#endif
		if (sessionState.source == NULL) {
		  printf("MplayerMP3Source Unable to create a little->bit-endian order \n");
		  return -1;
		}
	 
		struct in_addr destinationAddress;
		int          i_fd = -1;
		char uuid_str[10];
		char gp_ip[30];
		unsigned char k0 = 1, k1 =1;
		memset(uuid_str,0,sizeof(uuid_str));
		memset(gp_ip,0,sizeof(gp_ip));
		i_fd = open( "/tmp/mv_uuid", O_RDONLY );
		if( i_fd >= 0 )
		{
			read( i_fd, uuid_str, sizeof(uuid_str));
			
			
            sscanf( uuid_str, "%x %x ",&k0, &k1 );

			printf("uuid_str %s  k0 %x k1 %x \n",uuid_str,k0,k1);
			close( i_fd );
		}

		sprintf(gp_ip,"232.114.%d.%d",k0,k1);	
		destinationAddress.s_addr = inet_addr(gp_ip); //chooseRandomIPv4SSMAddress(*env);
		printf("gp_ip  %s  \n  ",gp_ip);
		// Note: This is a multicast address.  If you wish instead to stream
		// using unicast, then you should use the "testOnDemandRTSPServer"
		// test program - not this test program - as a model.
	
		const unsigned short rtpPortNum = 2222;
		const unsigned short rtcpPortNum = rtpPortNum+1;
		const unsigned char ttl = 1;
	  
		const Port rtpPort(rtpPortNum);
		const Port rtcpPort(rtcpPortNum);
	
		sessionState.rtpGroupsock
		  = new Groupsock(*env, destinationAddress, rtpPort, ttl);
		sessionState.rtpGroupsock->multicastSendOnly(); // we're a SSM source
		sessionState.rtcpGroupsock
		  = new Groupsock(*env, destinationAddress, rtcpPort, ttl);
		sessionState.rtcpGroupsock->multicastSendOnly(); // we're a SSM source
	
	
		// Create an appropriate audio RTP sink (using "SimpleRTPSink")
		// from the RTP 'groupsock':
#if 0
		sessionState.sink
		  = SimpleRTPSink::createNew(*env, sessionState.rtpGroupsock,
						 10, 44100,
						 "audio", "L16", 2);
#endif	
		sessionState.sink
	  	 = MPEG1or2AudioRTPSink::createNew(*env, sessionState.rtpGroupsock);
	
		// Create (and start) a 'RTCP instance' for this RTP sink:
		  const unsigned estimatedSessionBandwidth = 160;//(44100*32)/1000;
			  // in kbps; for RTCP b/w share
		  const unsigned maxCNAMElen = 100;
		  unsigned char CNAME[maxCNAMElen+1];
		  gethostname((char*)CNAME, maxCNAMElen);
		  CNAME[maxCNAMElen] = '\0'; // just in case
		  sessionState.rtcpInstance
			= RTCPInstance::createNew(*env, sessionState.rtcpGroupsock,
						  estimatedSessionBandwidth, CNAME,
						  sessionState.sink, NULL /* we're a server */,
						  True /* we're a SSM source*/);
		  // Note: This starts RTCP running automatically
		
		  // Create and start a RTSP server to serve this stream:
		  sessionState.rtspServer = RTSPServer::createNew(*env, 8554);
		  if (sessionState.rtspServer == NULL) {
			printf("rtsp server create failed  \n ");
			return -1;
		  }
		  
		  ServerMediaSession* sms
			= ServerMediaSession::createNew(*env, "rtspStream", "rtsp server",
		   "Session streamed by \"testWAVAudiotreamer\"", True/*SSM*/);
	  sms->addSubsession(PassiveServerMediaSubsession::createNew(*sessionState.sink, sessionState.rtcpInstance));
	  sessionState.rtspServer->addServerMediaSession(sms);
	
	  char* url = sessionState.rtspServer->rtspURL(sms);
	  printf("Play this stream using the URL  %s \n",url);

		//mono, send it by UDP
		memset(sendBuf,0x0,sizeof(sendBuf));
		sprintf(sendBuf,"PLAY_THIS_STREAM_URL %s",url);
		udpSend("127.0.0.1",7011,sendBuf,strlen(sendBuf));
		//////////////////////////////////////////////////

	  
	  delete[] url;
	
	  // Finally, start the streaming:
	  sessionState.sink->startPlaying(*sessionState.source, ao_mp3rtsp_afterPlaying, NULL);
	
	
	
	
		env->taskScheduler().doEventLoop(); // does not return
	
#endif
  } while (0);

  
  return 0;
}


