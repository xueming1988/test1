/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#ifndef _PTP_CONDITION_H_
#define _PTP_CONDITION_H_

#include "ptpConstants.h"

/**
 * @brief Condition implementation
 */
typedef struct ptpCondition_t* PtpConditionRef;

/**
 * @brief Allocate new condition object
 * @return reference to condition object
 */
PtpConditionRef ptpConditionCreate(void);

/**
 * @brief Destroy condition object allocated via ptpConditionCreate()
 * @param mutex reference to condition object
 */
void ptpConditionDestroy(PtpConditionRef condition);

/**
 * @brief Wait until the condition is set
 * @param mutex reference to condition object
 */
Boolean ptpConditionWait(PtpConditionRef condition);

/**
 * @brief Set the condition (this will unblock any thread waiting for condition via ptpConditionWait() call)
 * @param mutex reference to condition object
 */
Boolean ptpConditionSignal(PtpConditionRef condition);

#endif // _PTP_CONDITION_H_
