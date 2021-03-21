/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpForeignClock.h"
#include "Common/ptpLog.h"
#include "Common/ptpMutex.h"
#include "ptpClockContext.h"
#include "ptpNetworkInterface.h"
#include "ptpNetworkPort.h"

#include <assert.h>
#include <math.h>
#include <string.h>

/**
 * List of Announce messages
 */
static void ptpListMsgAnnounceObjectDtor(MsgAnnounceRef msg);

static void ptpListMsgAnnounceObjectDtor(MsgAnnounceRef msg)
{
    ptpRefMsgAnnounceRelease(msg);
}

DEFINE_LIST(MsgAnnounce, ptpListMsgAnnounceObjectDtor);

static void ptpForeignClockCtor(ForeignClockRef clock, NetworkPortRef port, MsgAnnounceRef announce);

static void ptpForeignClockCtor(ForeignClockRef clock, NetworkPortRef port, MsgAnnounceRef announce)
{
    assert(clock && port);
    clock->_port = port;
    clock->_messages = ptpListMsgAnnounceCreate();
    ptpForeignClockInsertAnnounce(clock, announce);

    ptpPortIdentityCtor(&(clock->_portIdentity));
    ptpPortIdentityAssign(&(clock->_portIdentity), ptpMsgAnnounceGetPortIdentity(announce));
}

void ptpForeignClockDtor(ForeignClockRef clock)
{
    if (!clock)
        return;

    ptpListMsgAnnounceDestroy(clock->_messages);
    ptpPortIdentityDtor(&(clock->_portIdentity));
}

DECLARE_MEMPOOL(ForeignClock)
DEFINE_MEMPOOL(ForeignClock, NULL, ptpForeignClockDtor)

ForeignClockRef ptpForeignClockCreate(NetworkPortRef port, MsgAnnounceRef announce)
{
    ForeignClockRef clock = ptpMemPoolForeignClockAllocate();
    ptpForeignClockCtor(clock, port, announce);
    return clock;
}

void ptpForeignClockDestroy(ForeignClockRef clock)
{
    ptpMemPoolForeignClockDeallocate(clock);
}

static void ptpListForeignClockObjectDtor(ForeignClockRef identity)
{
    ptpForeignClockDestroy(identity);
}

Boolean ptpForeignClockMatchesAnnounce(ForeignClockRef clock, MsgAnnounceRef announce)
{
    assert(clock && announce);
    return ptpPortIdentityEqualsTo(&(clock->_portIdentity), ptpMsgAnnounceGetPortIdentity(announce));
}

void ptpForeignClockClear(ForeignClockRef clock)
{
    assert(clock);
    ptpListMsgAnnounceClear(clock->_messages);
}

int ptpForeignClockAnnounceMsgSize(ForeignClockRef clock)
{
    if(clock == NULL)
        return 0;

    return ptpListMsgAnnounceSize(clock->_messages);
}

Boolean ptpForeignClockHasEnoughMessages(ForeignClockRef clock)
{
    assert(clock);
    return (ptpListMsgAnnounceSize(clock->_messages) >= FOREIGN_MASTER_THRESHOLD);
}

void ptpForeignClockPruneOldMessages(ForeignClockRef clock)
{
    assert(clock);
    Timestamp now;
    ptpClockCtxGetTime(&now);

    // make sure we don't have too many messages in the list
    while (ptpListMsgAnnounceSize(clock->_messages) > FOREIGN_MASTER_THRESHOLD) {
        ptpListMsgAnnounceErase(clock->_messages, ptpListMsgAnnounceLast(clock->_messages));
    }

    while (ptpListMsgAnnounceSize(clock->_messages)) {

        // get last element
        ListMsgAnnounceIterator it = ptpListMsgAnnounceLast(clock->_messages);

        // get last message in the list
        MsgAnnounceRef lastMsg = ptpListMsgAnnounceIteratorValue(it);

        // check if the announce has already expired
        if (!ptpMsgAnnounceHasExpired(lastMsg, &now)) {

            // no, it hasn't expired yet, we can break the loop as all other messages
            // are younger
            break;
        }

        // erase last message - it has expired
        PTP_LOG_VERBOSE("Erasing expired announce message from master %016llx",
            ptpClockIdentityGetUInt64(ptpPortIdentityGetClockIdentity(&(clock->_portIdentity))));
        ptpListMsgAnnounceErase(clock->_messages, it);
    }
}

ClockDatasetRef ptpForeignClockGetDataset(ForeignClockRef clock)
{
    if (!clock)
        return NULL;

    // take the newest announce message from both clock and other object
    MsgAnnounceRef announce = clock ? ptpListMsgAnnounceFront(clock->_messages) : NULL;

    if (!announce)
        return NULL;

    return ptpMsgAnnounceGetDataset(announce, clock->_port);
}

PortIdentityRef ptpForeignClockGetPortIdentity(ForeignClockRef clock)
{
    assert(clock);
    return &(clock->_portIdentity);
}

Boolean ptpForeignClockIsBetterThan(ForeignClockRef clock, ForeignClockRef other)
{
    assert(clock && other);

    // extract clock dataset from each announce
    ClockDatasetRef dataSetA = ptpForeignClockGetDataset(clock);
    ClockDatasetRef dataSetB = ptpForeignClockGetDataset(other);

    // compare clock datasets
    Boolean ret = ptpClockDatasetIsBetterThan(dataSetA, dataSetB);
    ptpClockDatasetDestroy(dataSetA);
    ptpClockDatasetDestroy(dataSetB);
    return ret;
}

Boolean ptpForeignClockInsertAnnounce(ForeignClockRef clock, MsgAnnounceRef announce)
{
    Boolean thresholdReached = false;
    Boolean diff = false;

    assert(closk && announce);

    // get rid of old messages
    ptpForeignClockPruneOldMessages(clock);

    // first message is taken into account
    if (ptpListMsgAnnounceSize(clock->_messages) == 0) {
        ptpRefMsgAnnounceRetain(announce);
        ptpListMsgAnnounceInsert(clock->_messages, ptpListMsgAnnounceFirst(clock->_messages), announce);
        return true;
    }

    // if we still have enough messages there, check if the threshold was reached
    if (FOREIGN_MASTER_THRESHOLD - 1 == ptpListMsgAnnounceSize(clock->_messages)) {
        thresholdReached = true;
    }

    // add this announcement
    ptpRefMsgAnnounceRetain(announce);
    ptpListMsgAnnounceInsert(clock->_messages, ptpListMsgAnnounceFirst(clock->_messages), announce);
    PTP_LOG_VERBOSE("Adding announce message to foreign master %016llx", ptpClockIdentityGetUInt64(ptpPortIdentityGetClockIdentity(&(clock->_portIdentity))));

    // compare two newest messages in the list, if they have the same information
    if (ptpListMsgAnnounceSize(clock->_messages) > 1) {
        MsgAnnounceRef prevAnnounce = ptpListMsgAnnounceIteratorValue(ptpListMsgAnnounceNext(ptpListMsgAnnounceFirst(clock->_messages)));
        diff = !ptpMsgAnnounceCompare(announce, prevAnnounce);
    }

    return thresholdReached || diff;
}

// linked list of PortIdentity smart objects
DEFINE_LIST(ForeignClock, ptpListForeignClockObjectDtor)
