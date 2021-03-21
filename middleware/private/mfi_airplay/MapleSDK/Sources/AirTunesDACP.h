/*
	Copyright (C) 2008-2019 Apple Inc. All Rights Reserved.
*/

#ifndef __AirTunesDACP_h__
#define __AirTunesDACP_h__

#include "CommonServices.h"
#include "DebugServices.h"

#ifdef __cplusplus
extern "C" {
#endif

//---------------------------------------------------------------------------------------------------------------------------
/*!	@group		AirTunesDACPClient
	@abstract	Async DACP client for AirTunes.
*/

typedef struct AirTunesDACPClient* AirTunesDACPClientRef;

OSStatus AirTunesDACPClient_Create(AirTunesDACPClientRef* outClient);
void AirTunesDACPClient_Delete(AirTunesDACPClientRef inClient);

typedef void (*AirTunesDACPClientCallBackPtr)(OSStatus inError, void* inContext);
OSStatus
AirTunesDACPClient_SetCallBack(
    AirTunesDACPClientRef inClient,
    AirTunesDACPClientCallBackPtr inCallBack,
    void* inContext);

OSStatus AirTunesDACPClient_SetDACPID(AirTunesDACPClientRef inClient, uint64_t inDACPID);
OSStatus AirTunesDACPClient_SetActiveRemoteID(AirTunesDACPClientRef inClient, uint32_t inActiveRemoteID);
OSStatus AirTunesDACPClient_StartSession(AirTunesDACPClientRef inClient);
OSStatus AirTunesDACPClient_StopSession(AirTunesDACPClientRef inClient);
OSStatus AirTunesDACPClient_ScheduleCommand(AirTunesDACPClientRef inClient, const char* inCommand);

//! @endgroup

#ifdef __cplusplus
}
#endif

#endif // __AirTunesDACP_h__
