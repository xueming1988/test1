/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_NETWORK_PORT_H_
#define _PTP_NETWORK_PORT_H_

#include "Common/ptpLog.h"
#include "Common/ptpMutex.h"
#include "Common/ptpThread.h"
#include "ptpClockContext.h"
#include "ptpConstants.h"
#include "ptpMacAddress.h"
#include "ptpNetworkInterface.h"
#include "ptpPeripheralDelay.h"
#include "ptpPortIdentity.h"

#include "Messages/ptpMsgAnnounce.h"
#include "Messages/ptpMsgDelayReq.h"
#include "Messages/ptpMsgDelayResp.h"
#include "Messages/ptpMsgFollowUp.h"
#include "Messages/ptpMsgSignalling.h"
#include "Messages/ptpMsgSync.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief This object holds information about last processed timestamps
 */
typedef struct {
    NanosecondTs _t1;
    NanosecondTs _t2;
    NanosecondTs _t3;
    NanosecondTs _t4;
} LastTimestamps;

typedef LastTimestamps* LastTimestampsRef;

/**
 * @brief allocates memory for a single instance of NetworkPort object
 * @return dynamically allocated NetworkPort object
 */
NetworkPortRef ptpNetPortCreate(void);

/**
 * @brief calls ptpNetPortDtor() and deallocates memory on provided NetworkPort object
 * @param port reference to the NetworkPort object
 */
void ptpNetPortDestroy(NetworkPortRef port);

/**
 * @brief NetworkPort constructor. This has to be explicitly called only on every statically or dynamically allocated NetworkPort object.
 * @param port reference to the NetworkPort object
 * @param ifname interface name string
 * @param clockContext reference to ClockContext object
 * @param phyDelayList list of peripheral delay entries
 */
void ptpNetPortCtor(NetworkPortRef port, const char* ifname, ClockCtxRef clockContext, const ListPeripheralDelayRef phyDelayList);

/**
 * @brief ptpNetPortDtor
 * @param port reference to the NetworkPort object
 */
void ptpNetPortDtor(NetworkPortRef port);

/**
 * @brief Get the link delay information.
 * @param port reference to the NetworkPort object
 * @return link delay value
 */
NanosecondTs ptpNetPortGetLinkDelay(NetworkPortRef port);

/**
 * @brief ptpNetPortSetLinkDelay
 * @param port reference to the NetworkPort object
 * @param delay new link delay value
 */
void ptpNetPortSetLinkDelay(NetworkPortRef port, NanosecondTs delay);

/**
 * @brief ptpNetPortGetClock
 * @param port reference to the NetworkPort object
 * @return reference to the internal ClockCtx object
 */
ClockCtxRef ptpNetPortGetClock(NetworkPortRef port);

/**
 * @brief Start sync receipt timeout
 * @param port reference to the NetworkPort object
 * @param waitTime sync receipt timeout in nanoseconds
 */
void ptpNetPortStartSyncReceiptTimer(NetworkPortRef port, uint64_t waitTime);

/**
 * @brief Stop sync receipt timeout
 * @param port reference to the NetworkPort object
 */
void ptpNetPortStopSyncReceiptTimer(NetworkPortRef port);

/**
 * @brief Start announce receipt timeout
 * @param port reference to the NetworkPort object
 * @param waitTime announce receipt timeout in nanoseconds
 */
void ptpNetPortStartAnnounceReceiptTimer(NetworkPortRef port, long long unsigned int waitTime);

/**
 * @brief Stop announce receipt timeout
 * @param port reference to the NetworkPort object
 */
void ptpNetPortStopAnnounceReceiptTimer(NetworkPortRef port);

/**
 * @brief Process specified event type
 * @param port reference to the NetworkPort object
 * @param event event type to process
 * @return true if succeeded, false otherwise
 */
Boolean ptpNetPortProcessEvent(NetworkPortRef port, PtpEvent event);

/**
 * @brief Get transmit peripheral delay for given link speed
 * @param port reference to the NetworkPort object
 * @param linkSpeed link speed value we are requesting the delay for
 * @return timestamp object representing transmission delay for given link speed
 */
TimestampRef ptpNetPortGetTxPhyDelay(NetworkPortRef port, uint32_t linkSpeed);

/**
 * @brief Get reception peripheral delay for given link speed
 * @param port reference to the NetworkPort object
 * @param linkSpeed link speed value we are requesting the delay for
 * @return timestamp object representing reception delay for given link speed
 */
TimestampRef ptpNetPortGetRxPhyDelay(NetworkPortRef port, uint32_t linkSpeed);

/**
 * @brief Send event payload to the specified destination port identity
 * @param port reference to the NetworkPort object
 * @param buf pointer to payload
 * @param size length of payload
 * @param destIdentity target port identity
 * @param txTimestamp [out] transmission timestamp
 * @param isEvent true if the payload contains event message, false otherwise
 */
void ptpNetPortSend(NetworkPortRef port, uint8_t* buf, size_t size, PortIdentityRef destIdentity, TimestampRef txTimestamp, Boolean isEvent, Boolean sendToAllUnicast);

/**
 * @brief Process PTP message received from the network
 * @param port reference to the NetworkPort object
 * @param remote PTP message source address
 * @param buf pointer to received payload
 * @param length length of received payload
 * @param ingressTime reception timestamp
 * @param rrecv status flag
 */
void ptpNetPortProcessMessage(NetworkPortRef port, LinkLayerAddressRef remote, uint8_t* buf, size_t length, Timestamp ingressTime, NetResult rrecv);

/**
 * @brief Get the current port state
 * @param port reference to the NetworkPort object
 * @return port state
 */
PortState ptpNetPortGetState(NetworkPortRef port);

/**
 * @brief Set the current port state
 * @param port reference to the NetworkPort object
 * @param state new port state
 */
void ptpNetPortSetState(NetworkPortRef port, PortState state);

/**
 * @brief Retrieve address string describing provided PortIdentity object
 * @param port reference to the NetworkPort object
 * @param destIdentity input PortIdentity
 * @param addrStr [out] output string array
 * @param addrStrLen the length of output string array
 * @return pointer to the address string
 */
char* ptpNetPortIdentityToAddressStr(NetworkPortRef port, PortIdentityRef destIdentity, char* addrStr, size_t addrStrLen);

/**
 * @brief Map PortIdentity to a target LinkLayerAddress
 * @param port reference to the NetworkPort object
 * @param destIdentity PortIdentity to be mapped
 * @param remote [out] LinkLayerAddress object where the given PortIdentity points
 */
void ptpNetPortMapSocketAddr(NetworkPortRef port, PortIdentityRef destIdentity, LinkLayerAddressRef remote);

/**
 * @brief Add mapping of PortIdentity object to LinkLayerAddress
 * @param port reference to the NetworkPort object
 * @param destIdentity mapped PortIdentity object
 * @param remote LinkLayerAddress object that is linked to given PortIdentity
 * @return true if the mapping succeeded, false otherwise
 */
Boolean ptpNetPortAddSockAddrMap(NetworkPortRef port, PortIdentityRef destIdentity, LinkLayerAddressRef remote);

/**
 * @brief Process currently received announce message
 * @param port reference to the NetworkPort object
 * @param announce reference to MsgAnnounce object
 */
void ptpNetPortAnnounceReceived(NetworkPortRef port, MsgAnnounceRef announce);

/**
 * @brief Keep reference to last sync message object
 * @param port reference to the NetworkPort object
 * @param sync reference to MsgSync object
 */
void ptpNetPortSetCurrentSync(NetworkPortRef port, MsgSyncRef sync);

/**
 * @brief Retrieve reference to last sync message object
 * @param port reference to the NetworkPort object
 * @param expectedSyncId id of the sync message we want to retrieve
 * @return reference to the internal MsgSync object
 */
MsgSyncRef ptpNetPortGetCurrentSync(NetworkPortRef port, PortIdentityRef expectedPortIdent, const uint16_t expectedSyncId);

/**
 * @brief Keep reference to last folow up message object
 * @param port reference to the NetworkPort object
 * @param msg reference to MsgFollowUp object
 */
void ptpNetPortSetCurrentFollowUp(NetworkPortRef port, MsgFollowUpRef msg);

/**
 * @brief Retrieve reference to last follow up message object
 * @param port reference to the NetworkPort object
 * @param expectedSyncId id of the follow up message we want to retrieve
 * @return reference to the internal MsgFollowUp object
 */
MsgFollowUpRef ptpNetPortGetCurrentFollowUp(NetworkPortRef port, PortIdentityRef expectedPortIdent, const uint16_t expectedSyncId);

/**
 * @brief Keep reference to last delay request message object
 * @param port reference to the NetworkPort object
 * @param msg reference to MsgDelayReq object
 */
void ptpNetPortSetCurrentDelayReq(NetworkPortRef port, MsgDelayReqRef msg);

/**
 * @brief Retrieve reference to last delay request message object
 * @param port reference to the NetworkPort object
 * @param expectedSyncId id of the delay request message we want to retrieve
 * @return reference to the internal MsgDelayReq object
 */
MsgDelayReqRef ptpNetPortGetCurrentDelayReq(NetworkPortRef port, const uint16_t expectedSyncId);

/**
 * @brief Retrieve reference to internal PortIdentity object
 * @param port reference to the NetworkPort object
 * @return reference to the internal PortIdentity object
 */
PortIdentityRef ptpNetPortGetPortIdentity(NetworkPortRef port);

/**
 * @brief Acquire reference to processing mutex
 * @param port reference to the NetworkPort object
 * @return reference to processing mutex
 */
PtpMutexRef ptpNetPortGetProcessingLock(NetworkPortRef port);

/**
 * @brief Set the number of synchronization iterations
 * @param port reference to the NetworkPort object
 * @param cnt new sync count value
 */
void ptpNetPortSetSyncCount(NetworkPortRef port, unsigned int cnt);

/**
 * @brief Increment sync count
 * @param port reference to the NetworkPort object
 */
void ptpNetPortIncSyncCount(NetworkPortRef port);

/**
 * @brief Get current sync count value
 * @param port reference to the NetworkPort object
 * @return sync count value
 */
uint32_t ptpNetPortGetSyncCount(NetworkPortRef port);

/**
 * @brief Increments announce sequence id and returns Next announce sequence id.
 * @param port reference to the NetworkPort object
 * @return
 */
uint16_t ptpNetPortGetNextAnnounceSequenceId(NetworkPortRef port);

/**
 * @brief Increment signal sequence id and returns Next signal sequence id.
 * @param port reference to the NetworkPort object
 * @return next signal sequence id
 */
uint16_t ptpNetPortGetNextSignalSequenceId(NetworkPortRef port);

/**
 * @brief Increment sync sequence ID and returns Next sync sequence id.
 * @param port reference to the NetworkPort object
 * @return next sync sequence id
 */
uint16_t ptpNetPortGetNextSyncSequenceId(NetworkPortRef port);

/**
 * @brief Increments Delay sequence ID and returns Next Delay sequence id.
 * @param port reference to the NetworkPort object
 * @return next delay sequence id
 */
uint16_t ptpNetPortGetNextDelayReqSequenceId(NetworkPortRef port);

/**
 * @brief Get the lastGmTimeBaseIndicator
 * @param port reference to the NetworkPort object
 * @return lastGmTimeBaseIndicator
 */
uint16_t ptpNetPortGetLastGmTimeBaseIndicator(NetworkPortRef port);

/**
 * @brief Set the lastGmTimeBaseIndicator
 * @param port reference to the NetworkPort object
 * @param gmTimeBaseIndicator new value of lastGmTimeBaseIndicator
 */
void ptpNetPortSetLastGmTimeBaseIndicator(NetworkPortRef port, uint16_t gmTimeBaseIndicator);

/**
 * @brief Get the sync interval value
 * @param port reference to the NetworkPort object
 * @return value of sync interval
 */
signed char ptpNetPortGetSyncInterval(NetworkPortRef port);

/**
 * @brief Set the sync interval value
 * @param port reference to the NetworkPort object
 * @param val new value of sync interval
 */
void ptpNetPortSetSyncInterval(NetworkPortRef port, signed char val);

/**
 * @brief Add one address to the list of unicast destination nodes
 * @param port reference to the NetworkPort object
 * @param addrStr address to add
 */
void ptpNetPortAddUnicastSendNode(NetworkPortRef port, const char* addrStr);

/**
 * @brief Remove given address from the list of unicast destination nodes
 * @param port reference to the NetworkPort object
 * @param addrStr address to remove
 */
void ptpNetPortDeleteUnicastSendNode(NetworkPortRef port, const char* addrStr);

/**
 * @brief Dump the content of list of unicast destination nodes
 * @param port reference to the NetworkPort object
 */
void ptpNetPortDumpUnicastSendNodes(NetworkPortRef port);

/**
 * @brief Retrieve reference to last timestamps object
 * @param port reference to the NetworkPort object
 * @return reference to internal LastTimestamps object
 */
LastTimestampsRef ptpNetPortGetLastTimestamps(NetworkPortRef port);

/**
 * @brief Set the last timestamps object value
 * @param port reference to the NetworkPort object
 * @param timestamps source LastTimestamps object
 */
void ptpNetPortSetLastTimestamps(NetworkPortRef port, const LastTimestampsRef timestamps);

/**
 * @brief Set local time value
 * @param port reference to the NetworkPort object
 * @return last stored local time
 */
NanosecondTs ptpNetPortGetLastLocalTime(NetworkPortRef port);

/**
 * @brief ptpNetPortSetLastLocaltime
 * @param port reference to the NetworkPort object
 * @param value new value of local time
 */
void ptpNetPortSetLastLocaltime(NetworkPortRef port, NanosecondTs value);

/**
 * @brief Get the last remote time
 * @param port reference to the NetworkPort object
 * @return last stored remote time
 */
NanosecondTs ptpNetPortGetLastRemoteTime(NetworkPortRef port);

/**
 * @brief Set remote time value
 * @param port reference to the NetworkPort object
 * @param time new value of remote time
 */
void ptpNetPortSetLastRemoteTime(NetworkPortRef port, NanosecondTs time);

/**
 * @brief Get the number of failed synchronizations in a row
 * @param port reference to the NetworkPort object
 * @return the number of failed synchronizations
 */
int ptpNetPortGetBadDiffCount(NetworkPortRef port);

/**
 * @brief Set the new number of failed synchronizations
 * @param port reference to the NetworkPort object
 * @param value new count of failed synchronization attempts
 */
void ptpNetPortSetBadDiffCount(NetworkPortRef port, int value);

/**
 * @brief Get the number of successfull synchronizations in a row
 * @param port reference to the NetworkPort object
 * @return the number of successful synchronizations
 */
int ptpNetPortGetGoodSyncCount(NetworkPortRef port);

/**
 * @brief Set the new number of successful synchronizations
 * @param port reference to the NetworkPort object
 * @param value new count of successful synchronization attempts
 */
void ptpNetPortSetGoodSyncCount(NetworkPortRef port, int value);

/**
 * @brief Get the current announce interval
 * @param port reference to the NetworkPort object
 * @return anounce interval
 */
signed char ptpNetPortGetAnnounceInterval(NetworkPortRef port);

/**
 * @brief Set the current announce interval
 * @param port reference to the NetworkPort object
 * @param val new value of announce interval
 */
void ptpNetPortSetAnnounceInterval(NetworkPortRef port, signed char val);

/**
 * @brief Set the link speed
 * @param port reference to the NetworkPort object
 * @param linkSpeed new link speed value
 */
void ptpNetPortSetLinkSpeed(NetworkPortRef port, uint32_t linkSpeed);

/**
 * @brief Retrieve link speed
 * @param port reference to the NetworkPort object
 * @return link speed value
 */
uint32_t ptpNetPortGetLinkSpeed(NetworkPortRef port);

/**
 * @brief Lock transmit mutex
 * @param port reference to the NetworkPort object
 * @return true if the lock succeeded, false otherwise
 */
Boolean ptpNetPortLockTxLock(NetworkPortRef port);

/**
 * @brief Unlock transmit mutex
 * @param port reference to the NetworkPort object
 * @return true if the unlock succeeded, false otherwise
 */
Boolean ptpNetPortUnlockTxLock(NetworkPortRef port);

/**
 * @brief Retrieve number bad diffs
 * @param port reference to the NetworkPort object
 * @return
 */
int ptpNetPortGetMaxBadDiffCount(NetworkPortRef port);

/**
 * @brief update the bad diff count with new value
 * @param port reference to the NetworkPort object
 * @param value new value for bad diffs
 */
void ptpNetPortSetMaxBadDiffCount(NetworkPortRef port, int value);

/**
 * @brief Retrieve the number of success count
 * @param port reference to the NetworkPort object
 * @return value of number of times we have succesful diffs
 */
int ptpNetPortGetSuccessDiffCount(NetworkPortRef port);

/**
 * @brief update the successful diffs with new value
 * @param port reference to the NetworkPort object
 * @param value new value for successful diffs
 */
void ptpNetPortSetSuccessDiffCount(NetworkPortRef port, int value);

DECLARE_LIST_REF(NetworkPort, NetworkPortRef);

#ifdef __cplusplus
}
#endif

#endif // _PTP_NETWORK_PORT_H_
