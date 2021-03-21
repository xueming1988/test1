/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpPeripheralDelay.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

DEFINE_REFCOUNTED_OBJECT(PeripheralDelay, NULL, NULL, ptpPeripheralDelayCtor, ptpPeripheralDelayDtor)

static void ptpListPeripheralDelayObjectDtor(PeripheralDelayRef delay)
{
    ptpRefPeripheralDelayRelease(delay);
}

DEFINE_LIST(PeripheralDelay, ptpListPeripheralDelayObjectDtor)

void ptpPeripheralDelayCtor(PeripheralDelayRef delay)
{
    assert(delay);
    delay->_linkSpeed = 0;
    ptpPeripheralDelaySet(delay, 0, 0);
}

void ptpPeripheralDelayDtor(PeripheralDelayRef delay)
{
    if (!delay)
        return;

    // empty
}

void ptpPeripheralDelaySet(PeripheralDelayRef delay, uint16_t tx_delay, uint16_t rx_delay)
{
    assert(delay);
    delay->_txDelay = tx_delay;
    delay->_rxDelay = rx_delay;
}

void ptpPeripheralDelaySetLinkSpeed(PeripheralDelayRef delay, uint32_t linkSpeed)
{
    assert(delay);
    delay->_linkSpeed = linkSpeed;
}

void ptpPeripheralDelaySetTx(PeripheralDelayRef delay, uint16_t tx_delay)
{
    assert(delay);
    delay->_txDelay = tx_delay;
}

void ptpPeripheralDelaySetRx(PeripheralDelayRef delay, uint16_t rx_delay)
{
    assert(delay);
    delay->_rxDelay = rx_delay;
}

uint32_t ptpPeripheralDelayGetLinkSpeed(PeripheralDelayRef delay)
{
    assert(delay);
    return delay->_linkSpeed;
}

uint16_t ptpPeripheralDelayGetTx(PeripheralDelayRef delay)
{
    assert(delay);
    return delay->_txDelay;
}

uint16_t ptpPeripheralDelayGetRx(PeripheralDelayRef delay)
{
    assert(delay);
    return delay->_rxDelay;
}
