/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "Common/ptpList.h"
#include "Common/ptpRefCountedObject.h"
#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief peripheral RX/TX delay compensation
 */
typedef struct ptpPeriphDelay_t {
    uint32_t _linkSpeed;
    uint16_t _txDelay;
    uint16_t _rxDelay;
} PeripheralDelay;

typedef PeripheralDelay* PeripheralDelayRef;

/**
 * @brief PeripheralDelay default constructor. This has to be explicitly called only on a statically allocated PeripheralDelay object.
 * @param delay reference to PeripheralDelay object
 */
void ptpPeripheralDelayCtor(PeripheralDelayRef delay);

/**
 * @brief PeripheralDelay destructor
 * @param delay reference to PeripheralDelay object
 */
void ptpPeripheralDelayDtor(PeripheralDelayRef delay);

/**
 * @brief Configure tx/rx delay
 * @param delay reference to PeripheralDelay object
 * @param txDelay new tx delay value
 * @param rxDelay new rx delay value
 */
void ptpPeripheralDelaySet(PeripheralDelayRef delay, uint16_t txDelay, uint16_t rxDelay);

/**
 * @brief Configure link speed
 * @param delay reference to PeripheralDelay object
 * @param linkSpeed new link speed value
 */
void ptpPeripheralDelaySetLinkSpeed(PeripheralDelayRef delay, uint32_t linkSpeed);

/**
 * @brief Configure tx delay
 * @param delay reference to PeripheralDelay object
 * @param txDelay new tx delay value
 */
void ptpPeripheralDelaySetTx(PeripheralDelayRef delay, uint16_t txDelay);

/**
 * @brief Confiogure rx delay
 * @param delay reference to PeripheralDelay object
 * @param rxDelay new rx delay value
 */
void ptpPeripheralDelaySetRx(PeripheralDelayRef delay, uint16_t rxDelay);

/**
 * @brief Retrieve link speed
 * @param delay reference to PeripheralDelay object
 * @return link speed value
 */
uint32_t ptpPeripheralDelayGetLinkSpeed(PeripheralDelayRef delay);

/**
 * @brief Retrieve tx delay
 * @param delay reference to PeripheralDelay object
 * @return tx delay value
 */
uint16_t ptpPeripheralDelayGetTx(PeripheralDelayRef delay);

/**
 * @brief Retrieve rx delay
 * @param delay reference to PeripheralDelay object
 * @return rx delay value
 */
uint16_t ptpPeripheralDelayGetRx(PeripheralDelayRef delay);

DECLARE_REFCOUNTED_OBJECT(PeripheralDelay)
DECLARE_LIST(PeripheralDelay)

#ifdef __cplusplus
}
#endif

// _PTP_PERIPHERAL_DELAY_H_
