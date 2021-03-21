/*
	File:    PlatformRemote.c
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
	
	Copyright (C) 2012-2014 Apple Inc. All Rights Reserved.
*/

#include "AirPlayMain.h"
#include "AirPlayCommon.h"
#include "CommonServices.h"
#include "DACPCommon.h" 
#include "AirPlayReceiverServer.h"
#include "AirPlayReceiverPOSIX.h"
#include "MathUtils.h"

#include <stdio.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>

#include "PlatformInternal.h"
#include "PlatformRemote.h"

/* 
 * Macros
 */
#define DATA_SIZE       64
#define COMMAND_SIZE    64

#define FILE_NAME   "/tmp/command"
#define TAIL        "tail -f "
#define RM          "rm -Rf "
#define TOUCH       "touch "

#define PLAY        "play"      // no arguments
#define STOP        "stop"      // no arguments
#define PAUSE       "pause"     // no arguments
#define PLAYPAUSE   "playpause" // no arguments
#define NEXT        "next"      // no arguments
#define PREV        "prev"      // no arguments
#define REPEAT      "repeat"    // no arguments
#define SHUFFLE     "shuffle"   // no arguments
#define MUTE        "mute"      // no arguments
#define UNMUTE      "unmute"    // no arguments
#define VOLUP       "vol+"      // no arguments
#define VOLDOWN     "vol-"      // no arguments
#define DOCK        "dock"      // no arguments
#define UNDOCK      "undock"    // no arguments
#define PASSWORD    "password"  // argument = new passord
                                // no argument = Reset password
#define NAME        "name"      // argument = new name

/* 
 * Enums & typedefs 
 */
enum {
    ePlay = 1,
    eStop,
    ePause,
    ePlayPause,
    eNext,
    ePrev,
    eRepeat,
    eShuffle,
    eMute,
    eUnmute,
    eVolumeUp,
    eVolumeDown,
    eDock,
    eUndock,
    ePassword,
    eName
};


/* 
 * Prototypes 
 */
int whichCommand( char *commandStr);


int whichCommand( char *commandStr)
{
    if ( commandStr == NULL ) return( 0 );

    if (0) {}
    else if (!strcmp(commandStr, PLAY))         return( ePlay );
    else if (!strcmp(commandStr, STOP))         return( eStop );
    else if (!strcmp(commandStr, PAUSE))        return( ePause );
    else if (!strcmp(commandStr, PLAYPAUSE))    return( ePlayPause );
    else if (!strcmp(commandStr, NEXT))         return( eNext );
    else if (!strcmp(commandStr, PREV))         return( ePrev );
    else if (!strcmp(commandStr, REPEAT))       return( eRepeat );
    else if (!strcmp(commandStr, SHUFFLE))      return( eShuffle );
    else if (!strcmp(commandStr, MUTE))         return( eMute );
    else if (!strcmp(commandStr, UNMUTE))       return( eUnmute );
    else if (!strcmp(commandStr, VOLUP))        return( eVolumeUp );
    else if (!strcmp(commandStr, VOLDOWN))      return( eVolumeDown );
    else if (!strcmp(commandStr, DOCK))         return( eDock );
    else if (!strcmp(commandStr, UNDOCK))       return( eUndock );
    else if (!strcmp(commandStr, PASSWORD))     return( ePassword );
    else if (!strcmp(commandStr, NAME))         return( eName );
    else return( 0 );
}


void* platformRemoteSignalThreadmain(void *inArg)
{
    FILE *pf;
    char data[DATA_SIZE];
    char dacpcommand[COMMAND_SIZE];

    (void) inArg;

    system( RM FILE_NAME );
    system( TOUCH FILE_NAME );

    // Setup our pipe for reading and execute our command.
    pf = popen( TAIL FILE_NAME, "r" );

    if(!pf)
    {
        fprintf(stderr, "Could not open pipe for output.\n");
        return( NULL );
    }

    printf( "Remote Command File = %s\n", FILE_NAME );

    while (1)
    {
        char *command;
        AirPlayReceiverServerRef   serverRef;

        // Grab data from process execution
        fgets(data, DATA_SIZE , pf);

        /* dump tokens */
        command = strtok( data, " \n");

        if ( command == NULL ) break;

        serverRef = AirPlayReceiverServerGetRef();

        switch ( whichCommand( command ) )
        {
            char *arg1;

            case ePlay:
                printf("play request\n");
                AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_Play );
                break;

            case eStop:
                printf("stop request\n");
                AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_Stop );
                break;

            case ePause:
                printf("pause request\n");
                AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_Pause );
                break;

            case ePlayPause:
                printf("playpause request\n");
                AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_PlayPause );
                break;

            case eNext:
                printf("next request\n");
                AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_NextItem );
                break;

            case ePrev:
                printf("prev request\n");
                AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_PrevItem );
                break;

            case eRepeat:
                printf("repeat toggle request\n");
                AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_RepeatAdvance );
                break;

            case eShuffle:
                printf("shuffle toggle request\n");
                AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_ShuffleToggle );
                break;

            case eMute:
                printf("mute request\n");
                AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_SetProperty kDACPProperty_DeviceVolume "=-144.0");
                break;

            case eUnmute:
            {
                extern double   gCurrentVolumeLevel;
                double setVolume = TranslateValue( gCurrentVolumeLevel, 0.0, 1.0, kAirTunesMinVolumeDB, kAirTunesMaxVolumeDB );

                printf("unmute request [volume level=%f]\n", setVolume);
                snprintf( dacpcommand, COMMAND_SIZE, "%s%s=%f\n", kDACPCommandStr_SetProperty, kDACPProperty_DeviceVolume, setVolume );
                AirPlayReceiverServerSendDACPCommand( serverRef, dacpcommand);
                break;
            }

            case eVolumeUp:
                printf("vol+ request\n");
                AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_VolumeUp );
                break;

            case eVolumeDown:
                printf("vol- request\n");
                AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_VolumeDown );
                break;

            case eDock:
                printf("dock request\n");
                AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_SetProperty kDACPProperty_DevicePreventPlayback "=1" );
                AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_SetProperty kDACPProperty_DeviceBusy "=1" );
                AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_SetProperty kDACPProperty_DevicePreventPlayback "=0" );
                break;

            case eUndock:
                printf("undock request\n");
                AirPlayReceiverServerSendDACPCommand( serverRef, kDACPCommandStr_SetProperty kDACPProperty_DeviceBusy "=0" );
                break;

            case ePassword:
                arg1 = strtok( NULL, " \n");
                handlePasswordChange( arg1 );
                break;

            case eName:
                arg1 = strtok( NULL, " \n");
                handleFriendlyNameChange( arg1 );
                break;

            default:
                printf("unknown request %s\n", data);
                break;
        }
    }

    pclose(pf);

    return(NULL);
}

