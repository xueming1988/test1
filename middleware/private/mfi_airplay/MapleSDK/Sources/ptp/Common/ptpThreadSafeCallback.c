/*
	Copyright (C) 2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ptpThreadSafeCallback.h"
#include "AtomicUtils.h"
#include "ptpThread.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !TARGET_OS_BRIDGECO
#include <pthread.h>
#endif

static int32_t g_safeCallbackState = 1;

void ptpThreadSafeCallback(ThreadSafeCallback callback, void* arg)
{
    while (1) {

        // get id of our thread
        pthread_t self = pthread_self();
        int32_t self32 = (uint32_t)(self);

        // check if the value of g_safeCallbackState was 1 - if that is the case, atomically set it to our thread id.
        // return the previous value of g_safeCallbackState
        int32_t prev = atomic_val_compare_and_swap_32(&g_safeCallbackState, 1, self32);

        if (prev == 1) {

            // Got lock. We can now safely call the provided callback function
            if (callback)
                callback(arg);

            // release 'lock'
            atomic_val_compare_and_swap_32(&g_safeCallbackState, self32, 1);
            break;

        } else if (prev == self32) {

            // we already own the lock (recursive call)
            if (callback)
                callback(arg);

            // no need to release the lock as this was a recursive call
            break;

        } else {

            // Another thread is holding the resource. Sleep and give it another chance in a while:
            ptpThreadSleep(1000);
        }
    }
}
