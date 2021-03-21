/*
	Copyright (C) 2001-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "MathUtils.h"

#include <math.h>

//===========================================================================================================================
//	iceil2
//
//	Rounds x up to the closest integral power of 2. Based on code from the book Hacker's Delight.
//===========================================================================================================================

uint32_t iceil2(uint32_t x)
{
    check(x <= UINT32_C(0x80000000));

    x = x - 1;
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);
    return (x + 1);
}

#if 0
#pragma mark -
#pragma mark == Debugging ==
#endif

#if (!EXCLUDE_UNIT_TESTS)
//===========================================================================================================================
//	MathUtils_Test
//===========================================================================================================================

OSStatus MathUtils_Test(int inPrint)
{
    OSStatus err;
    EWMA_FP_Data ewma;

    EWMA_FP_Init(&ewma, 0.1, kEWMAFlags_StartWithFirstValue);
    EWMA_FP_Update(&ewma, 71);
    if (inPrint)
        dlog(kLogLevelMax, "%.2f\n", EWMA_FP_Get(&ewma));
    EWMA_FP_Update(&ewma, 70);
    if (inPrint)
        dlog(kLogLevelMax, "%.2f\n", EWMA_FP_Get(&ewma));
    EWMA_FP_Update(&ewma, 69);
    if (inPrint)
        dlog(kLogLevelMax, "%.2f\n", EWMA_FP_Get(&ewma));
    EWMA_FP_Update(&ewma, 68);
    if (inPrint)
        dlog(kLogLevelMax, "%.2f\n", EWMA_FP_Get(&ewma));
    EWMA_FP_Update(&ewma, 64);
    if (inPrint)
        dlog(kLogLevelMax, "%.2f\n", EWMA_FP_Get(&ewma));
    EWMA_FP_Update(&ewma, 65);
    if (inPrint)
        dlog(kLogLevelMax, "%.2f\n", EWMA_FP_Get(&ewma));
    EWMA_FP_Update(&ewma, 72);
    if (inPrint)
        dlog(kLogLevelMax, "%.2f\n", EWMA_FP_Get(&ewma));
    EWMA_FP_Update(&ewma, 78);
    if (inPrint)
        dlog(kLogLevelMax, "%.2f\n", EWMA_FP_Get(&ewma));
    EWMA_FP_Update(&ewma, 75);
    if (inPrint)
        dlog(kLogLevelMax, "%.2f\n", EWMA_FP_Get(&ewma));
    EWMA_FP_Update(&ewma, 75);
    if (inPrint)
        dlog(kLogLevelMax, "%.2f\n", EWMA_FP_Get(&ewma));
    EWMA_FP_Update(&ewma, 75);
    if (inPrint)
        dlog(kLogLevelMax, "%.2f\n", EWMA_FP_Get(&ewma));
    EWMA_FP_Update(&ewma, 70);
    if (inPrint)
        dlog(kLogLevelMax, "%.2f\n", EWMA_FP_Get(&ewma));
    require_action(RoundTo(EWMA_FP_Get(&ewma), .01) == 71.50, exit, err = -1);

    require_action(iceil2(0) == 0, exit, err = kResponseErr);
    require_action(iceil2(1) == 1, exit, err = kResponseErr);
    require_action(iceil2(2) == 2, exit, err = kResponseErr);
    require_action(iceil2(32) == 32, exit, err = kResponseErr);
    require_action(iceil2(4096) == 4096, exit, err = kResponseErr);
    require_action(iceil2(UINT32_C(0x80000000)) == UINT32_C(0x80000000), exit, err = kResponseErr);
    require_action(iceil2(3) == 4, exit, err = kResponseErr);
    require_action(iceil2(17) == 32, exit, err = kResponseErr);
    require_action(iceil2(2289) == 4096, exit, err = kResponseErr);
    require_action(iceil2(26625) == 32768, exit, err = kResponseErr);
    require_action(iceil2(2146483648) == UINT32_C(0x80000000), exit, err = kResponseErr);
    require_action(iceil2(2147483647) == UINT32_C(0x80000000), exit, err = kResponseErr);

    err = kNoErr;

exit:
    printf("MathUtils_Test: %s\n", !err ? "PASSED" : "FAILED");
    return (err);
}
#endif // !EXCLUDE_UNIT_TESTS
