/*
 Copyright (C) 2018 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
 */

#include "AirPlayReceiverCommand.h"
#include "AirPlayReceiverServerPriv.h"
#include "AirPlayReceiverSession.h"
#include "AirPlayReceiverSessionPriv.h"
#include "AirPlayVersion.h"

ulog_define(AirPlayReceiverCommand, kLogLevelNotice, kLogFlags_Default, "AirPlayCmd", NULL);
#define aprc_ucat() &log_category_from_name(AirPlayReceiverCommand)
#define aprc_ulog(LEVEL, ...) ulog(aprc_ucat(), (LEVEL), __VA_ARGS__)
#define aprc_dlog(LEVEL, ...) dlogc(aprc_ucat(), (LEVEL), __VA_ARGS__)

EXPORT_GLOBAL
OSStatus
AirPlayReceiverServerCommandVolume(
    AirPlayReceiverServerRef inServer,
    Float32 volume)
{
    OSStatus err = kNoErr;
#if (AIRPLAY_VOLUME_CONTROL)
    CFMutableDictionaryRef options = NULL;
    AirPlayReceiverSessionRef masterSession = NULL;
    if (AirPlaySessionManagerIsMasterSessionActive(inServer->sessionMgr))
        masterSession = AirPlaySessionManagerCopyMasterSession(inServer->sessionMgr);

    double dbVolume = (volume == kAirTunesSilenceVolumeDB ? kAirTunesSilenceVolumeDB : Clamp(volume, kAirTunesMinVolumeDB, kAirTunesMaxVolumeDB));

    if (masterSession && masterSession->clientVersion >= kAirPlaySourceVersion_MediaRemoteVolumeCommad_Min) {
        // The kAPMediaRemote_DeviceVolumeChanged command wants
        // a normalized (0..1) db value.
        double scaledDBVolume = (dbVolume - kAirTunesMinVolumeDB) / (kAirTunesMaxVolumeDB - kAirTunesMinVolumeDB);

        options = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        require_action(options, exit, err = kNoMemoryErr);

        CFDictionarySetDouble(options, CFSTR(kAirPlayProperty_Volume), scaledDBVolume);

        err = AirPlayReceiverServerSendMediaCommandWithOptions(inServer, CFSTR(kAPMediaRemote_DeviceVolumeChanged), options);

        if (err == kNoErr) {
            CFNumberRef cfDBVolume = CFNumberCreate(NULL, kCFNumberDoubleType, &dbVolume);
            AirPlayReceiverSessionPlatformSetProperty(masterSession, 0x0, CFSTR(kAirPlayProperty_Volume), NULL, cfDBVolume);
            ForgetCF(&cfDBVolume);
        }
    }
#if (AIRPLAY_DACP)
    else {
        char volumeCommandStr[48] = "";
        snprintf(volumeCommandStr, sizeof(volumeCommandStr), "setproperty?dmcp.device-volume=%f", dbVolume);
        err = AirTunesDACPClient_ScheduleCommand(inServer->dacpClient, volumeCommandStr);
    }
#endif
    inServer->volume = dbVolume;
    aprc_ulog(kLogLevelNotice, "%s:server volume set to %.1f\n", __ROUTINE__, inServer->volume);
exit:
    CFReleaseNullSafe(masterSession);
    ForgetCF(&options);
#endif
    return (err);
}

EXPORT_GLOBAL
OSStatus AirPlayReceiverServerCommandPlayPause(AirPlayReceiverServerRef inServer)
{
    AirPlayReceiverSessionRef masterSession = NULL;

    if (AirPlaySessionManagerIsMasterSessionActive(inServer->sessionMgr))
        masterSession = AirPlaySessionManagerCopyMasterSession(inServer->sessionMgr);

    if (masterSession && masterSession->receiveAudioOverTCP) {
        AirPlayAnchor anchor = masterSession->source.setRateTiming.anchor;
        anchor.rate = masterSession->source.setRateTiming.anchor.rate ? 0 : 1;
        if (!anchor.rate) {
            AirPlayReceiverSessionSetRateAndAnchorTime(masterSession, &anchor);
        }
    }

    CFReleaseNullSafe(masterSession);

    return AirPlayReceiverServerSendMediaCommandWithOptions(inServer, CFSTR(kAPMediaRemote_PlayPause), NULL);
}

EXPORT_GLOBAL
OSStatus AirPlayReceiverServerCommandPlay(AirPlayReceiverServerRef inServer)
{
    return AirPlayReceiverServerSendMediaCommandWithOptions(inServer, CFSTR(kAPMediaRemote_Play), NULL);
}

EXPORT_GLOBAL
OSStatus AirPlayReceiverServerCommandPause(AirPlayReceiverServerRef inServer)
{
    AirPlayReceiverSessionRef masterSession = NULL;

    if (AirPlaySessionManagerIsMasterSessionActive(inServer->sessionMgr))
        masterSession = AirPlaySessionManagerCopyMasterSession(inServer->sessionMgr);

    if (masterSession && masterSession->receiveAudioOverTCP) {
        AirPlayAnchor anchor = masterSession->source.setRateTiming.anchor;
        anchor.rate = masterSession->source.setRateTiming.anchor.rate = 0;
        AirPlayReceiverSessionSetRateAndAnchorTime(masterSession, &anchor);
    }

    CFReleaseNullSafe(masterSession);

    return AirPlayReceiverServerSendMediaCommandWithOptions(inServer, CFSTR(kAPMediaRemote_Pause), NULL);
}

EXPORT_GLOBAL
OSStatus AirPlayReceiverServerCommandDeselectSource(AirPlayReceiverServerRef inServer)
{
    // This will cause any sender to tear down the connection
    OSStatus err = AirPlayReceiverServerSendMediaCommandWithOptions(inServer, CFSTR(kAPMediaRemote_PreventPlayback), NULL);

    // If we are using DACP then tell the sender to re-enable playback.
    // If we are not using DACP this call will fail because there is no session but
    // playback is re-enabled automatically.
    (void)AirPlayReceiverServerSendMediaCommandWithOptions((inServer), CFSTR(kAPMediaRemote_AllowPlayback), NULL);

    return err;
}

#if defined(SessionBasedCommandSupport_Deprecated)

OSStatus AirPlayReceiverCommandPlayPause(AirPlayReceiverSessionRef inSession)
{
    return AirPlayReceiverServerCommandPlayPause(inSession->server);
}

OSStatus AirPlayReceiverCommandPlay(AirPlayReceiverSessionRef inSession)
{
    return AirPlayReceiverServerCommandPlay(inSession->server);
}

OSStatus AirPlayReceiverCommandPause(AirPlayReceiverSessionRef inSession)
{
    return AirPlayReceiverServerCommandPause(inSession->server);
}

OSStatus AirPlayReceiverCommandVolume(AirPlayReceiverSessionRef inSession, Float32 volume)
{
    return AirPlayReceiverServerCommandVolume(inSession->server, volume);
}

OSStatus AirPlayReceiverCommandDeselectSource(AirPlayReceiverSessionRef inSession)
{
    return AirPlayReceiverServerCommandDeselectSource(inSession->server);
}

OSStatus
AirPlayReceiverSessionSendMediaCommandWithOptions(
    AirPlayReceiverSessionRef inSession,
    CFStringRef command,
    CFDictionaryRef params)
{
    return AirPlayReceiverServerSendMediaCommandWithOptions(inSession->server, command, params);
}

#endif /* SessionBasedCommandSupport_Deprecated */
