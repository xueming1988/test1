/*
	Copyright (C) 2012-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
 */

#ifndef AirPlayReceiverLogRequest_h
#define AirPlayReceiverLogRequest_h

#include "AirPlayReceiverServerPriv.h"
#include "CommonServices.h"
#include <stdio.h>

void AirPlayReceiverLogRequestInit(void);
HTTPStatus AirPlayReceiverLogRequestGetLogs(AirPlayReceiverConnectionRef inCnx, HTTPMessageRef inRequest);

#endif /* AirPlayReceiverLogRequest_h */
