/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_FOREIGN_CLOCK_H_
#define _PTP_FOREIGN_CLOCK_H_

#include "Common/ptpList.h"
#include "Common/ptpLog.h"
#include "Common/ptpMutex.h"
#include "Common/ptpThread.h"
#include "Messages/ptpMsgAnnounce.h"
#include "ptpClockContext.h"
#include "ptpClockDataset.h"
#include "ptpConstants.h"
#include "ptpNetworkPort.h"

#ifdef __cplusplus
extern "C" {
#endif

DECLARE_LIST(MsgAnnounce)

typedef struct {
    // port identity of this master
    PortIdentity _portIdentity;

    // list of received announce messages
    ListMsgAnnounceRef _messages;

    // owner
    NetworkPortRef _port;
} ForeignClock;

typedef ForeignClock* ForeignClockRef;

/**
 * @brief allocates memory for a single instance of ForeignClock object
 * @param port reference to the NetworkPortRef object
 * @param announce reference to the MsgAnnounce object
 * @return dynamically allocated ForeignClock object
 */
ForeignClockRef ptpForeignClockCreate(NetworkPortRef port, MsgAnnounceRef announce);

/**
 * @brief calls ptpForeignClockDtor() and deallocates memory on provided ForeignClock object
 * @param port reference to the ForeignClock object
 */
void ptpForeignClockDestroy(ForeignClockRef clock);

/**
 * @brief ptpForeignClockDtor
 * @param clock reference to the ForeignClock object
 */
void ptpForeignClockDtor(ForeignClockRef clock);

/**
 * @brief Check if the provided Announce object belongs to our ForeignClock object (based on PortIdentity)
 * @param clock reference to the ForeignClock object
 * @param announce reference to the MsgAnnounce object
 * @return true if Announce belongs to ForeignClock object, false otherwise
 */
Boolean ptpForeignClockMatchesAnnounce(ForeignClockRef clock, MsgAnnounceRef announce);

/**
 * @brief Insert announce message to the list of received announce messages
 * @param clock reference to the ForeignClock object
 * @param announce reference to the MsgAnnounce object
 * @return true if a significant change on master occured
 */
Boolean ptpForeignClockInsertAnnounce(ForeignClockRef clock, MsgAnnounceRef announce);

/**
 * @brief Clear all cached announce messages
 * @param clock reference to the ForeignClock object
 */
void ptpForeignClockClear(ForeignClockRef clock);

int ptpForeignClockAnnounceMsgSize(ForeignClockRef clock);

/**
 * @brief Check if the clock object has enough cached announce messages
 * @param clock reference to the ForeignClock object
 * @return true if the clock has enough announce messages, false otherwise
 */
Boolean ptpForeignClockHasEnoughMessages(ForeignClockRef clock);

/**
 * @brief Prune all invalid cached announce messages
 * @param clock reference to the ForeignClock object
 */
void ptpForeignClockPruneOldMessages(ForeignClockRef clock);

/**
 * @brief Retrieve ClockDataset information from provided ForeignClock object
 * @param clock reference to ForeignClock object
 * @return reference to ClockDataset object
 */
ClockDatasetRef ptpForeignClockGetDataset(ForeignClockRef clock);

/**
 * @brief Get internal port identity
 * @param clock reference to ForeignClock object
 * @return reference to PortIdentity object
 */
PortIdentityRef ptpForeignClockGetPortIdentity(ForeignClockRef clock);

/**
 * @brief Compare two foreign clocks
 * @param clock reference to ForeignClock object
 * @param other reference to other ForeignClock object
 * @return true if clock is better than other, false otherwise
 */
Boolean ptpForeignClockIsBetterThan(ForeignClockRef clock, ForeignClockRef other);

DECLARE_LIST(ForeignClock)

#ifdef __cplusplus
}
#endif

#endif // _PTP_FOREIGN_CLOCK_H_
