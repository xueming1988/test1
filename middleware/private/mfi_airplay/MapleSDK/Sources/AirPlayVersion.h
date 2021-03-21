/*
	Copyright (C) 2007-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#define kAirPlayMarketingVersion 2.0
#define kAirPlayMarketingVersionStr StringifyExpansion(kAirPlayMarketingVersion)

#define kAirPlayProtocolMajorVersion 1
#define kAirPlayProtocolMinorVersion 1
#define kAirPlayProtocolVersion kAirPlayProtocolMajorVersion.kAirPlayProtocolMinorVersion
#define kAirPlayProtocolVersionStr StringifyExpansion(kAirPlayProtocolVersion)

#define kAirPlaySourceVersion 366.0
#define kAirPlaySourceVersionStr StringifyExpansion(kAirPlaySourceVersion)

#define kAirPlayUserAgentStr ("AirPlay/" kAirPlaySourceVersionStr)

#define KAirPlayPosixSourceVersion 20

#define kAirPlaySourceVersion_MediaRemoteCommads_Min 3200000 //320.xx.xx (placeholder for checking for any version prior to media remote commands support)
#define kAirPlaySourceVersion_MediaRemoteVolumeCommad_Min 3650000
