/*
	Copyright (C) 2001-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "UUIDUtils.h"

#include "CommonServices.h"
#include "DebugServices.h"
#include "RandomNumberUtils.h"

//===========================================================================================================================
//	UUIDGet
//===========================================================================================================================

void UUIDGet(void* outUUID)
{
    RandomBytes(outUUID, 16);
    UUIDSetVersion(outUUID, kUUIDVersion_4_Random);
    UUIDMakeRFC4122(outUUID);
}

#if 0
#pragma mark -
#endif

#if (!EXCLUDE_UNIT_TESTS)
//===========================================================================================================================
//	UUIDUtils_Test
//===========================================================================================================================

OSStatus UUIDUtils_Test(void)
{
    OSStatus err;
    UUIDData x;

    UUIDClear(&x);
    UUIDGet(&x);
    require_action(!UUIDIsNull(&x), exit, err = kResponseErr);
    err = kNoErr;

exit:
    printf("UUIDUtils_Test: %s\n", !err ? "PASSED" : "FAILED");
    return (err);
}
#endif // !EXCLUDE_UNIT_TESTS
