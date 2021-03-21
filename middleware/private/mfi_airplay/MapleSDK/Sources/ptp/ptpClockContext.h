/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_CLOCK_CONTEXT_H_
#define _PTP_CLOCK_CONTEXT_H_

#include "Common/ptpMemoryPool.h"
#include "Common/ptpTimerQueue.h"
#include "Messages/ptpMsgAnnounce.h"
#include "Messages/ptpMsgFollowUp.h"
#include "ptpClockIdentity.h"
#include "ptpClockQuality.h"
#include "ptpInterface.h"
#include "ptpMacAddress.h"
#include "ptpPortIdentity.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Ptp Event type
 */
typedef enum {
    evNoEvent = 0, // Initial event
    evInitializing, // Initialization of state machine
    evLinkIsUp, // Triggered when link comes up.
    evLinkIsDown, // Triggered when link goes down.
    evStateChange, // State has changed
    evScheduledSyncTimeout, // Sync interval expired
    evSyncReceiptTimeout, // Sync receipt timeout expired
    evScheduledAnnounceTimeout, // Announce interval expired
    evAnnounceReceiptTimeout, // Announce receipt timeout expired
    evFailed, // State machine failed
} PtpEvent;

typedef struct ClockCtx* ClockCtxRef;

/**
 * @brief Allocate memory for a single instance of ClockCtx object
 * @return dynamically allocated ClockCtx object
 */
ClockCtxRef ptpClockCtxCreate(void);

/**
 * @brief Call ptpClockCtxDtor() and deallocates memory on provided ClockCtx object
 * @param clock reference to the ClockCtx object
 */
void ptpClockCtxDestroy(ClockCtxRef clock);

/**
 * @brief Object constructor. This has to be explicitly called on a statically allocated ClockCtx object.
 * @param clock reference to a valid ClockCtx memory
 * @param instanceId PTP instance identifier
 * @param priority1 clock priority
 * @param priority2 clock priority
 */
void ptpClockCtxCtor(ClockCtxRef clock, uint64_t instanceId, uint8_t priority1, uint8_t priority2);

/**
 * @brief Object destructor. Deinitializes all internal properties and resources
 * @param clock reference to ClockCtx object
 */
void ptpClockCtxDtor(ClockCtxRef clock);

/**
 * @brief Deinitializes ClockCtx object and prepares it for destruction
 * @param clock reference to ClockCtx object
 */
void ptpClockCtxDeinitialize(ClockCtxRef clock);

/**
 * @brief Assigns reference to NetworkPort object
 * @param clock reference to ClockCtx object
 * @param port
 */
void ptpClockCtxSetPort(ClockCtxRef clock, NetworkPortRef port);

/**
 * @brief Retrieve clock identity
 * @param clock reference to ClockCtx object
 * @return reference to clock identity object
 */
ClockIdentityRef ptpClockCtxGetClockIdentity(ClockCtxRef clock);

/**
 * @brief Set the ClockIdentity created from MacAddress object
 * @param clock reference to ClockCtx object
 * @param addr reference to MacAddress object
 */
void ptpClockCtxAssignClockIdentity(ClockCtxRef clock, const MacAddressRef addr);

/**
 * @brief Retrieve reference to grandmaster clock identity
 * @param clock reference to ClockCtx object
 * @return reference to clock identity
 */
ClockIdentityRef ptpClockCtxGetGrandmasterClockIdentity(ClockCtxRef clock);

/**
 * @brief Copy grandmaster identity from provided ClockIdentity object
 * @param clock reference to ClockCtx object
 * @param id reference to clock identity
 */
void ptpClockCtxAssignGrandmasterClockIdentity(ClockCtxRef clock, ClockIdentityRef id);

/**
 * @brief Retrieve reference to grandmaster clock quality
 * @param clock reference to ClockCtx object
 * @return reference to grandmaster clock quality
 */
ClockQualityRef ptpClockCtxGetGrandmasterClockQuality(ClockCtxRef clock);

/**
 * @brief Copy clock quality from provided ClockQuality object
 * @param clock reference to ClockCtx object
 * @param clock reference to ClockCtx objectQuality reference to clock quality object
 */
void ptpClockCtxAssignGrandmasterClockQuality(ClockCtxRef clock, ClockQualityRef clockQuality);

/**
 * @brief Retrieve reference to clock quality
 * @param clock reference to ClockCtx object
 * @return reference to clock quality
 */
ClockQualityRef ptpClockCtxGetClockQuality(ClockCtxRef clock);

/**
 * @brief Retrieve grandmaster priority 1 property
 * @param clock reference to ClockCtx object
 * @return value of grandmaster priority 1
 */
unsigned char ptpClockCtxGetGrandmasterPriority1(ClockCtxRef clock);

/**
 * @brief Set grandmaster priority 1 property
 * @param clock reference to ClockCtx object
 * @param new grandmaster priority 1 value
 */
void ptpClockCtxSetGrandmasterPriority1(ClockCtxRef clock, unsigned char priority1);

/**
 * @brief Retrieve grandmaster priority 2 property
 * @param clock reference to ClockCtx object
 * @return value of grandmaster priority 2
 */
unsigned char ptpClockCtxGetGrandmasterPriority2(ClockCtxRef clock);

/**
 * @brief Set grandmaster priority 2 property
 * @param clock reference to ClockCtx object
 * @param value of grandmaster priority 2
 */
void ptpClockCtxSetGrandmasterPriority2(ClockCtxRef clock, unsigned char priority2);

/**
 * @brief Retrieve grandmaster steps removed property
 * @param clock reference to ClockCtx object
 * @return value of grandmaster steps removed
 */
uint16_t ptpClockCtxGetGrandmasterStepsRemoved(ClockCtxRef clock);

/**
 * @brief Set grandmaster steps removed property
 * @param clock reference to ClockCtx object
 * @param value of grandmaster steps removed
 */
void ptpClockCtxSetGrandmasterStepsRemoved(ClockCtxRef clock, uint16_t stepsRemoved);

/**
 * @brief Retrieve last announce message port information
 * @param clock reference to ClockCtx object
 * @return port identity of the last announce message
 */
PortIdentityRef ptpClockCtxGetLastAnnouncePort(ClockCtxRef clock);

/**
 * @brief Retrieve last announce message port information
 * @param clock reference to ClockCtx object
 * @return port identity of the last announce message
 */
void ptpClockCtxSetLastAnnouncePort(ClockCtxRef clock, PortIdentityRef port);

/**
 * @brief Get UTC offset
 * @param clock reference to ClockCtx object
 * @return UTC offset as 16bit unsigned integer
 */
int16_t ptpClockCtxGetCurrentUtcOffset(ClockCtxRef clock);

/**
 * @brief Get time source property
 * @param clock reference to ClockCtx object
 * @return value of time source property
 */
uint8_t ptpClockCtxGetTimeSource(ClockCtxRef clock);

/**
 * @brief Get priority 1 property
 * @param clock reference to ClockCtx object
 * @return value of priority 1 property
 */
unsigned char ptpClockCtxGetPriority1(ClockCtxRef clock);

/**
 * @brief Set priority 1 property
 * @param clock reference to ClockCtx object
 * @param value new priority 1 value
 */
void ptpClockCtxSetPriority1(ClockCtxRef clock, unsigned char value);

/**
 * @brief Get priority 2 property
 * @param clock reference to ClockCtx object
 * @return value of priority 2
 */
unsigned char ptpClockCtxGetPriority2(ClockCtxRef clock);

/**
 * @brief Get reference to FollowUp TLV object used when PTP is in master mode
 * @param clock reference to ClockCtx object
 * @return reference to FollowUp TLV object
 */
FollowUpTlvRef ptpClockCtxGetFollowUpTlvTx(ClockCtxRef clock);

/**
 * @brief Get reference to received FollowUp TLV object (PTP is in slave mode)
 * @param clock reference to ClockCtx object
 * @return reference to received FollowUp TLV object
 */
FollowUpTlvRef ptpClockCtxGetFollowUTlvRx(ClockCtxRef clock);

/**
 * @brief Update FollowUp TLV object used in PTP master mode
 * @param clock reference to ClockCtx object
 */
void ptpClockCtxUpdateFollowUpTx(ClockCtxRef clock);

/**
 * @brief Schedule execution of event of given type on NetworkPort targed in defined timeout
 * @param clock reference to ClockCtx object
 * @param port reference to NetworkPort object
 * @param event type of scheduled event
 * @param ns relative event time in nanoseconds
 */
void ptpClockCtxScheduleEvent(ClockCtxRef clock, NetworkPortRef port, PtpEvent event, uint64_t ns);

/**
 * @brief Check if the event of given type is scheduled for execution
 * @param clock reference to ClockCtx object
 * @param event type of scheduled event
 * @return true if the event is scheduled, false otherwise
 */
Boolean ptpClockCtxIsEventScheduled(ClockCtxRef clock, PtpEvent event);

/**
 * @brief Remove all occurences of event of given type from execution scheduler
 * @param clock reference to ClockCtx object
 * @param event type of event to remove
 */
void ptpClockCtxUnscheduledEvent(ClockCtxRef clock, PtpEvent event);

/**
 * @brief Retrieve ratio between master clock and local clock
 * @param clock reference to ClockCtx object
 * @param masterTime time on master
 * @param syncTime time on slave
 * @return clock ratio between master and slave
 */
double ptpClockCtxCalcmasterToLocalRatio(ClockCtxRef clock, const NanosecondTs masterTime, const NanosecondTs syncTime);

/**
 * @brief Retrieve offset between master clock and local clock
 * @param clock reference to ClockCtx object
 * @return offset between master and local clock
 */
NanosecondTs ptpClockCtxMasterOffset(ClockCtxRef clock);

/**
 * @brief Update clock context with a new synchronization data
 * @param clock reference to ClockCtx object
 * @param masterToLocalPhase phase between master clock and local clock
 * @param masterToLocalRatio ratio between master clock and local clock
 * @param syncArrivalTime the local time when sync packet was received
 * @param syncCount the number of finished synchronization operations
 * @param portState the current state of the port
 * @param masterClockId id of master clock
 */
void ptpClockCtxSyncComplete(ClockCtxRef clock, NanosecondTs masterToLocalPhase, NanosecondTs masterToLocalRatio,
    NanosecondTs syncArrivalTime, unsigned syncCount, PortState portState, uint64_t masterClockId);

/**
 * @brief Reset linked IPC object
 * @param clock reference to ClockCtx object
 * @param masterClockId id of master clock
 */
void ptpClockCtxIpcReset(ClockCtxRef clock, uint64_t masterClockId);

/**
 * @brief Retrieve current time
 * @param timestamp [out] reference to Timestamp object where the current time will be stored
 */
void ptpClockCtxGetTime(TimestampRef timestamp);

/**
 * @brief Retrieve ClockDataset object describing the ClockCtx object
 * @param clock reference to ClockCtx object
 * @return reference to ClockDataset object
 */
ClockDatasetRef ptpClockCtxDefaultDataset(ClockCtxRef clock);

#ifdef __cplusplus
}
#endif

#endif // _PTP_CLOCK_CONTEXT_H_
