#include "AirPlayMain.h"
#include "AirPlayCommon.h"
#include "CommonServices.h"
#include "DACPCommon.h" 
#include "AirPlayReceiverServer.h"
#include "AirPlayReceiverPOSIX.h"
#include "MathUtils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wm_api.h"
#include "PlatformRemote.h"


static void ReStartAirplay(void)
{
	FILE* fp=fopen("/tmp/airplay_stop","w");
	if(fp)
		fclose(fp);
}


static int old_volume = -1;
void* AppleRemoteThread(void *arg)
{
	int ret = 0;
	int recv_num;
	char recv_buf[1024];
	AirPlayReceiverServerRef   serverRef;
	SOCKET_CONTEXT msg_socket;

	(void)arg;

	msg_socket.path="/tmp/RequestAppleRemote";
	msg_socket.blocking = 1;
	ret=UnixSocketServer_Init(&msg_socket);

	if(ret <= 0)
	{
		return (void *)-1;
	}

	while(1)
	{
		memset(recv_buf,0x0,sizeof(recv_buf));
		if(UnixSocketServer_accept(&msg_socket) > 0)
	    {
			recv_num = UnixSocketServer_read(&msg_socket,recv_buf,sizeof(recv_buf));
			UnixSocketServer_close(&msg_socket);
			if(recv_num <= 0)
			{
				continue;
			} 
			else
			{
				serverRef = AirPlayReceiverServerGetRef();			
				recv_buf[recv_num]='\0';
				//printf("+++MFI AppleRemote+++ recv %s at %d\n", recv_buf, (int)tickSincePowerOn());
				if( 0 )
				{

				}
				else if(strncmp(recv_buf,"AIRPLAY_STOP",strlen("AIRPLAY_STOP"))==0)
				{
					AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_Stop );
				}else if(strncmp(recv_buf,"AIRPLAY_PAUSE",strlen("AIRPLAY_PAUSE"))==0)
				{
					AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_Pause );
				}
				else if(strncmp(recv_buf,"AIRPLAY_PLAY_PAUSE",strlen("AIRPLAY_PLAY_PAUSE"))==0)
				{
                	AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_PlayPause );
				}
				else if(strncmp(recv_buf,"AIRPLAY_PLAY",strlen("AIRPLAY_PLAY"))==0)
				{
					AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_Play );
				}else if(strncmp(recv_buf,"AIRPLAY_STOP",strlen("AIRPLAY_STOP"))==0)
				{
					ReStartAirplay();
					AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_Stop );
				}else if(strncmp(recv_buf,"AIRPLAY_NEXT",strlen("AIRPLAY_NEXT"))==0)
				{
					AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_NextItem );
				}
				else if(strncmp(recv_buf,"AIRPLAY_PREV",strlen("AIRPLAY_PREV"))==0)
				{
					AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_PrevItem );
				}else if(strncmp(recv_buf,"AIRPLAY_VOLUME_UP",strlen("AIRPLAY_VOLUME_UP"))==0)
				{
					AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_VolumeUp );
				}else if(strncmp(recv_buf,"AIRPLAY_VOLUME_DOWN",strlen("AIRPLAY_VOLUME_DOWN"))==0)
				{
					AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_VolumeDown );
				}else if(strncmp(recv_buf,"AIRPLAY_VOLUME_MUTE",strlen("AIRPLAY_VOLUME_MUTE"))==0)
				{
					AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_SetProperty kDACPProperty_DeviceVolume "=-144.0");
				}
				else if(strncmp(recv_buf,"AIRPLAY_VOLUME_SET:",strlen("AIRPLAY_VOLUME_SET:"))==0)
				{
					char *volume = recv_buf + strlen("AIRPLAY_VOLUME_SET:");
					char dacpcommand[128];
					double volume_f;
					if( strlen(volume) > 0 )
					{
						if( old_volume == atoi(volume))
						{
							continue;
						}
						old_volume = atoi(volume);
						volume_f = atoi(volume) * 1.0;
						volume_f = TranslateValue(volume_f, 0.0, 100.0, kAirTunesMinVolumeDB, kAirTunesMaxVolumeDB);
						memset((void *)dacpcommand, 0, sizeof(dacpcommand));
						snprintf( dacpcommand, 128, "%s%s=%f\n", kDACPCommandStr_SetProperty, kDACPProperty_DeviceVolume, volume_f );
						AirPlayReceiverServerSendDACPCommand( serverRef, dacpcommand );
					}
				}
				else if(strncmp(recv_buf,"AIRPLAY_VOLUME_UNMUTE",strlen("AIRPLAY_VOLUME_UNMUTE"))==0)
				{
					char dacpcommand[128];
					double volume_f = TranslateValue(old_volume * 1.0, 0.0, 100.0, kAirTunesMinVolumeDB, kAirTunesMaxVolumeDB);
					memset((void *)dacpcommand, 0, sizeof(dacpcommand));
					snprintf( dacpcommand, 128, "%s%s=%f\n", kDACPCommandStr_SetProperty, kDACPProperty_DeviceVolume, volume_f );
					AirPlayReceiverServerSendDACPCommand( serverRef, dacpcommand );
				}				
				else if(strncmp(recv_buf,"AIRPLAY_SWITCH_OUT",strlen("AIRPLAY_SWITCH_OUT"))==0)
				{
					AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_SetProperty kDACPProperty_DevicePreventPlayback "=1" );
					AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_SetProperty kDACPProperty_DeviceBusy "=1" );
					AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_SetProperty kDACPProperty_DevicePreventPlayback "=0" );
				}
				else if(strncmp(recv_buf,"AIRPLAY_SWITCH_IN",strlen("AIRPLAY_SWITCH_IN"))==0)
				{
					AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_SetProperty kDACPProperty_DeviceBusy "=0" );
				}
				else if(strncmp(recv_buf,"AIRPLAY_MODE_REPEAT",strlen("AIRPLAY_MODE_REPEAT"))==0)
				{
					AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_RepeatAdvance );
				}
				else if(strncmp(recv_buf,"AIRPLAY_MODE_SHUFFLE",strlen("AIRPLAY_MODE_SHUFFLE"))==0)
				{
					AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_ShuffleToggle );
				}
				else if(strncmp(recv_buf,"AIRPLAY_NAME_SET",strlen("AIRPLAY_NAME_SET"))==0)
				{
					AirPlayReceiverServerPostEvent( serverRef, CFSTR( kAirPlayEvent_PrefsChanged ), NULL, NULL );
				}
				else if(strncmp(recv_buf,"AIRPLAY_PASSWORD_SET",strlen("AIRPLAY_PASSWORD_SET"))==0)
				{
					AirPlayReceiverServerPostEvent( serverRef, CFSTR( kAirPlayEvent_PrefsChanged ), NULL, NULL );
				}
				//printf("--- MFI AppleRemote leave %s at %d-------\n", recv_buf, (int)tickSincePowerOn());
			} 
		} 
	}

	return 0;
}

