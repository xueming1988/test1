/*
 Copyright (C) 2018 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
 */

#ifndef AirPlayReceiverCommand_h
#define AirPlayReceiverCommand_h

#include "AirPlayCommon.h"
#include "AirPlayReceiverServer.h"
#include "AirPlayReceiverSession.h"

typedef enum {
    MRMediaRemoteCommandPlay = 0,
    MRMediaRemoteCommandPause = 1,
    MRMediaRemoteCommandTogglePlayPause = 2,
    MRMediaRemoteCommandStop = 3,
    MRMediaRemoteCommandNextTrack = 4,
    MRMediaRemoteCommandPreviousTrack = 5,
    MRMediaRemoteCommandAdvanceShuffleMode = 6,
    MRMediaRemoteCommandAdvanceRepeatMode = 7,
} MRMediaRemoteCommand;

#define AirPlayReceiverServerCommandNextTrack(inServer) AirPlayReceiverServerSendMediaCommandWithOptions((inServer), CFSTR(kAPMediaRemote_NextTrack), NULL)
#define AirPlayReceiverServerCommandPreviousTrack(inServer) AirPlayReceiverServerSendMediaCommandWithOptions((inServer), CFSTR(kAPMediaRemote_PreviousTrack), NULL)
#define AirPlayReceiverServerCommandShuffleToggle(inServer) AirPlayReceiverServerSendMediaCommandWithOptions((inServer), CFSTR(kAPMediaRemote_ShuffleToggle), NULL)
#define AirPlayReceiverServerCommandRepeatToggle(inServer) AirPlayReceiverServerSendMediaCommandWithOptions((inServer), CFSTR(kAPMediaRemote_RepeatToggle), NULL)
#define AirPlayReceiverServerCommandVolumeDown(inServer) AirPlayReceiverServerSendMediaCommandWithOptions((inServer), CFSTR(kAPMediaRemote_VolumeDown), NULL)
#define AirPlayReceiverServerCommandVolumeUp(inServer) AirPlayReceiverServerSendMediaCommandWithOptions((inServer), CFSTR(kAPMediaRemote_VolumeUp), NULL)

//---------------------------------------------------------
/*! @function    AirPlayReceiverCommandVolume
 @abstract       Send the player a command to set the volume.
 
 @param          inServer
 @param          volume                   Decibel value of desired volume.
 */
OSStatus AirPlayReceiverServerCommandVolume(AirPlayReceiverServerRef inServer, Float32 volume);

//---------------------------------------------------------
/*! @function    AirPlayReceiverCommandPlayPause
 @abstract       Send the player a command to toggle its play/pause state.
 
 @param          inServer
 */
OSStatus AirPlayReceiverServerCommandPlayPause(AirPlayReceiverServerRef inServer);

//---------------------------------------------------------
/*! @function    AirPlayReceiverCommandPlay
 @abstract       Send a command to the player to play.
 
 @param          inServer
 */
OSStatus AirPlayReceiverServerCommandPlay(AirPlayReceiverServerRef inServer);

//---------------------------------------------------------
/*! @function    AirPlayReceiverServerCommandPause
 @abstract       Send a command to the player to pause.
 
 @param          inServer
 */
OSStatus AirPlayReceiverServerCommandPause(AirPlayReceiverServerRef inServer);

//---------------------------------------------------------
/*! @function    AirPlayReceiverCommandDeselectSource
 @abstract
 
 @param          inServer
 */
OSStatus AirPlayReceiverServerCommandDeselectSource(AirPlayReceiverServerRef inServer);

#if defined(SessionBasedCommandSupport_Deprecated)

#define AirPlayReceiverSessionSendMediaCommand(session, command) AirPlayReceiverSessionSendMediaCommandWithOptions((session), (command), NULL)
#define AirPlayReceiverCommandNextTrack(inSession) AirPlayReceiverSessionSendMediaCommandWithOptions((inSession), CFSTR(kAPMediaRemote_NextTrack), NULL)
#define AirPlayReceiverCommandPreviousTrack(inSession) AirPlayReceiverSessionSendMediaCommandWithOptions((inSession), CFSTR(kAPMediaRemote_PreviousTrack), NULL)
#define AirPlayReceiverCommandShuffleToggle(inSession) AirPlayReceiverSessionSendMediaCommandWithOptions((inSession), CFSTR(kAPMediaRemote_ShuffleToggle), NULL)
#define AirPlayReceiverCommandRepeatToggle(inSession) AirPlayReceiverSessionSendMediaCommandWithOptions((inSession), CFSTR(kAPMediaRemote_RepeatToggle), NULL)
#define AirPlayReceiverCommandVolumeDown(inSession) AirPlayReceiverSessionSendMediaCommandWithOptions((inSession), CFSTR(kAPMediaRemote_VolumeDown), NULL)
#define AirPlayReceiverCommandVolumeUp(inSession) AirPlayReceiverSessionSendMediaCommandWithOptions((inSession), CFSTR(kAPMediaRemote_VolumeUp), NULL)

OSStatus AirPlayReceiverCommandVolume(AirPlayReceiverSessionRef inSession, Float32 volume);
OSStatus AirPlayReceiverCommandPlayPause(AirPlayReceiverSessionRef inSession);
OSStatus AirPlayReceiverCommandPlay(AirPlayReceiverSessionRef inSession);
OSStatus AirPlayReceiverCommandPause(AirPlayReceiverSessionRef inSession);
OSStatus AirPlayReceiverCommandDeselectSource(AirPlayReceiverSessionRef inSession);

#endif /* SessionBasedCommandSupport_Deprecated */

#endif /* AirPlayReceiverCommand_h */
