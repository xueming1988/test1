/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_NETWORK_ACCESS_CONTEXT_H_
#define _PTP_NETWORK_ACCESS_CONTEXT_H_

#include "Common/ptpLog.h"
#include "Common/ptpMutex.h"
#include "Common/ptpThread.h"
#include "ptpConstants.h"
#include "ptpNetworkPort.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NetworkAccessContext* NetworkAccessContextRef;

/**
 * @brief Acquire singleton of NetworkAccessContext object
 *
 * @param port reference to the NetworkPort object
 * @param ifname name of the network itnerface
 * @param localAddress
 * @return reference to NetworkAccessContext singleton
 */
NetworkAccessContextRef ptpNetworkAccessContextCreate(NetworkPortRef port, const char* ifname, MacAddressRef localAddress);

/**
 * @brief Notify NetworkAccessContext singleton that port has been released
 *
 * @param context reference to NetworkAccessContext singleton
 * @param port reference to the NetworkPort object that was released
 */
void ptpNetworkAccessContextRelease(NetworkAccessContextRef context, NetworkPortRef port);

/**
 * @brief Start network processing thread
 *
 * @param context reference to NetworkAccessContext singleton
 * @return Boolean true if succeeded, false otherwise
 */
Boolean ptpNetworkAccessContextStartProcessing(NetworkAccessContextRef context);

/**
 * @brief Send PTP payload to the network
 *
 * @param context reference to NetworkAccessContext singleton
 * @param addr target address
 * @param payload pointer to the payload
 * @param length length of the payload
 * @param txTimestamp [out] reception timestamp
 * @param isEvent flag indicating if we should send the payload to event socket
 * @return NetResult result of send operation
 */
NetResult ptpNetworkAccessContextSend(NetworkAccessContextRef context, LinkLayerAddressRef addr, uint8_t* payload, size_t length, TimestampRef txTimestamp, Boolean isEvent);

/**
 * @brief Retrieve network speed
 *
 * @param context reference to NetworkAccessContext singleton
 * @param speed returns network speed
 * @return Boolean true if succeeded, false otherwise
 */
Boolean ptpNetworkAccessContextGetLinkSpeed(NetworkAccessContextRef context, uint32_t* speed);

#ifdef __cplusplus
}
#endif

#endif // _PTP_NETWORK_ACCESS_CONTEXT_H_
