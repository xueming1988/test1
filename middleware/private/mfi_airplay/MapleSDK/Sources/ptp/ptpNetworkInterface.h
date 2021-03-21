/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_NETWORK_INTERFACE_H_
#define _PTP_NETWORK_INTERFACE_H_

#include "ptpClockContext.h"
#include "ptpConstants.h"
#include "ptpLinkLayerAddress.h"
#include "ptpMacAddress.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    nNetFail,
    eNetFatal,
    eNetSucceed
} NetResult;

typedef struct NetIf* NetIfRef;

/**
 * @brief allocates memory for a single instance of NetIf object
 * @return dynamically allocated NetIf object
 */
NetIfRef ptpNetIfCreate(void);

/**
 * @brief calls ptpNetIfDtor() and deallocates memory on provided NetIf object
 * @param netif reference to the NetIf object
 */
void ptpNetIfDestroy(NetIfRef netif);

/**
 * @brief Object constructor. This has to be explicitly called on a statically allocated NetIf object.
 * @param netif reference to a valid NetIf memory
 * @param netIfName the name of network interface where PTP will run
 * @param clock reference to ClockCtx object
 */
void ptpNetIfCtor(NetIfRef netif, const char* netIfName);

/**
 * @brief Object destructor. Deinitializes all internal properties and resources
 * @param netif reference to NetIfRef object
 */
void ptpNetIfDtor(NetIfRef netif);

/**
 * @brief ptpNetIfSend
 * @param netif reference to the NetIf object
 * @param addr target LinkLayer address
 * @param payload pointer to message payload
 * @param length the length of message payload
 * @param txTimestamp [out] reception timestamp
 * @param isEvent flag indicating if we should send the payload to event socket
 * @return operation result
 */
NetResult ptpNetIfSend(NetIfRef netif, LinkLayerAddressRef addr, uint8_t* payload, size_t length, TimestampRef txTimestamp, Boolean isEvent);

/**
 * @brief ptpNetIfReceiveEvent
 * @param netif reference to the NetIf object
 * @param addr target LinkLayer address
 * @param payload [out] pointer to the memory where message payload will be stored
 * @param length [out] the length of payload buffer
 * @param ingressTime [out] reception timestamp
 * @return operation result
 */
NetResult ptpNetIfReceive(NetIfRef netif, LinkLayerAddressRef addr, uint8_t* payload, size_t* length, TimestampRef ingressTime);

/**
 * @brief Retrieve mac address of the network interface
 * @param netif reference to the NetIf object
 * @param addr [out] reference to MacAddress object where the mac address is stored
 */
void ptpNetIfGetMacAddress(NetIfRef netif, MacAddressRef addr);

/**
 * @brief Shut down all operations on network interface
 * @param netif reference to the NetIf object
 */
void ptpNetIfShutdown(NetIfRef netif);

/**
 * @brief Retrieve network interface speed
 * @param netif reference to the NetIf object
 * @param speed [out] network interface speed
 * @return
 */
Boolean ptpNetIfGetLinkSpeed(NetIfRef netif, uint32_t* speed);

#ifdef __cplusplus
}
#endif

#endif // _PTP_NETWORK_INTERFACE_H_
