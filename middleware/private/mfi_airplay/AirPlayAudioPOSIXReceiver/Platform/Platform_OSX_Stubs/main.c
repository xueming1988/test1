/*
	File:    main.c
	Package: AirPlayAudioPOSIXReceiver
	Version: AirPlay_Audio_POSIX_Receiver_211.1.p8
	
	Disclaimer: IMPORTANT: This Apple software is supplied to you, by Apple Inc. ("Apple"), in your
	capacity as a current, and in good standing, Licensee in the MFi Licensing Program. Use of this
	Apple software is governed by and subject to the terms and conditions of your MFi License,
	including, but not limited to, the restrictions specified in the provision entitled ”Public
	Software”, and is further subject to your agreement to the following additional terms, and your
	agreement that the use, installation, modification or redistribution of this Apple software
	constitutes acceptance of these additional terms. If you do not agree with these additional terms,
	please do not use, install, modify or redistribute this Apple software.
	
	Subject to all of these terms and in consideration of your agreement to abide by them, Apple grants
	you, for as long as you are a current and in good-standing MFi Licensee, a personal, non-exclusive
	license, under Apple's copyrights in this original Apple software (the "Apple Software"), to use,
	reproduce, and modify the Apple Software in source form, and to use, reproduce, modify, and
	redistribute the Apple Software, with or without modifications, in binary form. While you may not
	redistribute the Apple Software in source form, should you redistribute the Apple Software in binary
	form, you must retain this notice and the following text and disclaimers in all such redistributions
	of the Apple Software. Neither the name, trademarks, service marks, or logos of Apple Inc. may be
	used to endorse or promote products derived from the Apple Software without specific prior written
	permission from Apple. Except as expressly stated in this notice, no other rights or licenses,
	express or implied, are granted by Apple herein, including but not limited to any patent rights that
	may be infringed by your derivative works or by other works in which the Apple Software may be
	incorporated.
	
	Unless you explicitly state otherwise, if you provide any ideas, suggestions, recommendations, bug
	fixes or enhancements to Apple in connection with this software (“Feedback”), you hereby grant to
	Apple a non-exclusive, fully paid-up, perpetual, irrevocable, worldwide license to make, use,
	reproduce, incorporate, modify, display, perform, sell, make or have made derivative works of,
	distribute (directly or indirectly) and sublicense, such Feedback in connection with Apple products
	and services. Providing this Feedback is voluntary, but if you do provide Feedback to Apple, you
	acknowledge and agree that Apple may exercise the license granted above without the payment of
	royalties or further consideration to Participant.
	
	The Apple Software is provided by Apple on an "AS IS" basis. APPLE MAKES NO WARRANTIES, EXPRESS OR
	IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY
	AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR
	IN COMBINATION WITH YOUR PRODUCTS.
	
	IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES
	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
	PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION
	AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
	(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
	
	Copyright (C) 2007-2014 Apple Inc. All Rights Reserved.
*/

#include "PlatformCommonServices.h"
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <sys/types.h>
#include <fcntl.h>

#include "AirPlayMain.h"
#include "AirPlayReceiverServer.h"
#include "wm_api.h"
#include "PlatformRemote.h"

void *cmdSocket_airplay(void *arg)
{
	int recv_num;
	char recv_buf[4096];
	int msg, ret;
	SOCKET_CONTEXT msg_socket;
	char cmd[128];

	msg_socket.path="/tmp/RequestAirplay";
	msg_socket.blocking = 1;
	
RE_INIT:
	ret=UnixSocketServer_Init(&msg_socket);
	printf("airplayd IO UnixSocketServer_Init return %d\n", ret);
	if(ret < 0)
	{
		printf("IO UnixSocketServer_Init fail\n");
		sleep(5);
		goto RE_INIT;
	}

	while(1)
	{
		ret = UnixSocketServer_accept(&msg_socket);
		if(ret <=0)
		{
			printf("airplayd IO UnixSocketServer_accept fail\n");
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
				printf("IO UnixSocketServer_recv failed\r\n");
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
				if(strncmp(recv_buf, "IPChanged", strlen("IPChanged")) ==0)
				{
					//AirPlayStopMain();
					//AirPlayStartMain();
				}
				else if(strncmp(recv_buf,"NameChanged",strlen("NameChanged"))==0)
				{					
					memset((void *)cmd, 0, sizeof(cmd));
					int fd = open("/tmp/airplay_name", O_RDWR | O_CREAT | O_TRUNC);
					if( fd >= 0 )
					{
						write(fd, recv_buf+strlen("NameChanged:"), strlen(recv_buf+strlen("NameChanged:")));
						close(fd);
					}
					//sprintf(cmd, "echo -n \"%s\" > /tmp/airplay_name", recv_buf+strlen("NameChanged:"));
					//system(cmd);
					//printf("%s run cmd[%s]\n", __FUNCTION__, cmd);
					/* notify airplay about name change */
					AirPlayReceiverServerPostEvent( AirPlayReceiverServerGetRef(), CFSTR( kAirPlayEvent_PrefsChanged ), NULL, NULL );
				}
				else if(strncmp(recv_buf,"setAirplayPassword",strlen("setAirplayPassword"))==0)
				{					
					memset((void *)cmd, 0, sizeof(cmd));
					sprintf(cmd, "echo -n \"%s\" > /tmp/airplay_password", recv_buf+strlen("setAirplayPassword:"));
					system(cmd);

					printf("%s save airplay password[%s]\n", __FUNCTION__, recv_buf+strlen("setAirplayPassword:"));
					memset((void *)&cmd, 0, sizeof(cmd));
					sprintf(cmd, "nvram_set 2860 AIRPLAY_PASSWORD \"%s\"&", recv_buf+strlen("setAirplayPassword:"));
					system(cmd);
					
					/* notify airplay about password change */
					AirPlayReceiverServerPostEvent( AirPlayReceiverServerGetRef(), CFSTR( kAirPlayEvent_PrefsChanged ), NULL, NULL );
				}
				else if(strncmp(recv_buf,"stopAirplay",strlen("stopAirplay"))==0)
				{
					// notify clients to stop 
					SocketClientReadWriteMsg("/tmp/RequestAppleRemote","AIRPLAY_SWITCH_OUT",strlen("AIRPLAY_SWITCH_OUT"),NULL,NULL,0);
					//AirTunesServer_StopAllConnections();
					AirPlayReceiverServerPostEvent( AirPlayReceiverServerGetRef(), CFSTR( kAirPlayCommand_StopServer ), NULL, NULL );
				}
				else if(strncmp(recv_buf,"startAirplay",strlen("startAirplay"))==0)
				{
					AirPlayReceiverServerPostEvent( AirPlayReceiverServerGetRef(), CFSTR( kAirPlayCommand_StartServer ), NULL, NULL );
				}				
				else if(strncmp(recv_buf, "updateConfig", strlen("updateConfig")) == 0)
				{
					AirPlayReceiverServerPostEvent( AirPlayReceiverServerGetRef(), CFSTR( kAirPlayEvent_PrefsChanged ), NULL, NULL );
				}
			}
			UnixSocketServer_close(&msg_socket);
		}
	}

	return NULL;
}

int	main( int argc, const char *argv[] )
{
    int     ret = 0;

    (void) argc;
    (void) argv;

	pthread_t a_thread;
	pthread_create(&a_thread,NULL, cmdSocket_airplay,NULL);

	pthread_t b_thread;
	pthread_create(&b_thread,NULL, AppleRemoteThread,NULL);

    printf("Linux Platform Main Start build[%s][%s]%d\n", __DATE__, __TIME__,kAirTunesPlayoutDelay);

    // Start AirPlay
    ret = AirPlayStartMain();

    // wait forever
    pause();

    // Stop AirPlay
    ret = AirPlayStopMain();

    printf("Linux Platform Main End ret=%d\n", ret);

    return(0);
}

