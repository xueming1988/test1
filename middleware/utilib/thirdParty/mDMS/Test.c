/*
 * Test.c
 *
 *  Created on: 2012-7-30
 *      Author: randy
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h> 
#include <net/route.h>
#include <net/if.h>

#include <sys/stat.h>

#include <pthread.h>


#include "Responder.h"
int mDNS_LoggingEnabled=0;
int mDNS_PacketLoggingEnabled=0;
int mDNS_DebugMode=0;          // If non-zero, LogMsg() writes to stderr instead of syslog

static char* name_audio = "003C33234A33@huaijing";
static char* name_video = "huaijing";

static int port = 4515;

static char videoServiceType[] = "_airplay._tcp.";
static char* videoServiceText[] =  {"deviceid=00:3C:33:23:4A:33", "features=0x177"/*0x39f7*/,"flags=0x4","model=WiimuTV3,2",  "srcvers=160.10" ,"vv=1"/*, "aed=1.1.4.0"*/};
static int videoServiceTextLen = 6;


//static char audioServiceType[] = "_raop._tcp.";
//static char* audioServiceText[]= {"txtvers=1","ch=2","cn=0,1","da=true","et=0,1","ft=0x77","md=0,1,2","pw=false","sv=false","sr=44100","ss=16","tp=UDP","vn=65537","vs=160.10","vv=1","am=WiimuTV3,2","sf=0x4"};
//static int audioServiceTextLen = 17;

static char audioServiceType[] = "_raop._tcp.";
static char* audioServiceText[]= {"tp=UDP",  "sv=false", "da=true","ek=1", "et=0,1", "md=0,1,2","cn=0,1", "ch=2", "ss=16", "sr=44100", "pw=false", "ft=0x177","txtvers=1","vs=160.10","vv=1", "vn=65537", "am=WiimuTV3,2","sf=0x4"};
static int audioServiceTextLen = 18;




int main()
{
	int mDNSID =0;
	
	mDNSID=mDNS_init();
//	mDNS_publishOneService(name_video,videoServiceType,port,videoServiceText,videoServiceTextLen);
	mDNS_publishOneService(name_audio,audioServiceType,port,audioServiceText,audioServiceTextLen);
	{
		/*
		// unsigned char pKey[1024*4];
		 char *pKey ;
		int sizeKey;
		pKey=decode_base64(strPrivateKey,strlen(strPrivateKey), &sizeKey);

		printf("sizeKey = %d \r\n",sizeKey);
		
		const unsigned char* _key = pKey;

		pRSA = RSA_new();
		d2i_RSAPrivateKey(&pRSA, &_key, sizeKey);
		*/
	}

	char in;
	while((in = getchar()) != 'q')
	{
		switch(in){
		case 's':
			mDNSID=mDNS_init();
			break;
		case 'a':
			mDNS_publishOneService(name_audio,audioServiceType,port,audioServiceText,audioServiceTextLen);
			break;
		case 'v':
		//	mDNS_publishOneService(name_video,videoServiceType,port,videoServiceText,videoServiceTextLen);
			mDNS_publishOneService(name_audio,audioServiceType,port,audioServiceText,audioServiceTextLen);
			break;
		case 'r':
			//mDNS_release(mDNSID);
			mDMS_DeregisterServices();
			break;
		}
	}



  //  if (pRSA)
//		RSA_free(pRSA);
}
