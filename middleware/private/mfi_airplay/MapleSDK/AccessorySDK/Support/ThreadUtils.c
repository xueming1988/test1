/*
	Copyright (C) 2010-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
*/

#include "ThreadUtils.h"

#if (TARGET_MACH)
#include <mach/mach.h>
#endif

#if (TARGET_OS_WINDOWS)
//===========================================================================================================================
//	SetThreadName
//===========================================================================================================================

// See <http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx>.
#pragma pack(push, 8)
typedef struct
{
    DWORD dwType; // Must be 0x1000.
    LPCSTR szName; // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags; // Reserved for future use, must be zero.

} THREADNAME_INFO;
#pragma pack(pop)

OSStatus SetThreadName(const char* inName)
{
    THREADNAME_INFO info;

    info.dwType = 0x1000;
    info.szName = (LPCSTR)inName;
    info.dwThreadID = GetCurrentThreadId();
    info.dwFlags = 0;

    __try {
        RaiseException(0x406D1388 /* MS_VC_EXCEPTION */, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return (kNoErr);
}
#endif

//===========================================================================================================================
//	SetCurrentThreadPriority
//===========================================================================================================================

OSStatus SetCurrentThreadPriority(int inPriority)
{
    OSStatus err;

    if (inPriority == kThreadPriority_TimeConstraint) {
#if (TARGET_MACH)
        thread_time_constraint_policy_data_t policyInfo;
        mach_msg_type_number_t policyCount;
        boolean_t getDefault;

        getDefault = true;
        policyCount = THREAD_TIME_CONSTRAINT_POLICY_COUNT;
        err = thread_policy_get(mach_thread_self(), THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t)&policyInfo,
            &policyCount, &getDefault);
        require_noerr(err, exit);

        err = thread_policy_set(mach_thread_self(), THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t)&policyInfo,
            THREAD_TIME_CONSTRAINT_POLICY_COUNT);
        require_noerr(err, exit);
#else
        dlogassert("Platform doesn't support time constraint threads");
        err = kUnsupportedErr;
        goto exit;
#endif
    }
#if (TARGET_OS_NETBSD)
    else {
        int policy;
        struct sched_param sched;

        if (pthread_getschedparam(pthread_self(), &policy, &sched) == 0) {
            int max = sched_get_priority_max(policy);
            int min = sched_get_priority_min(policy);

            if (inPriority >= min && inPriority <= max) {
                sched.sched_priority = inPriority;
                err = pthread_setschedparam(pthread_self(), policy, &sched);
                require_noerr(err, exit);
            }
        }
        err = kNoErr;
    }
#elif (TARGET_OS_POSIX)
    else {
        int policy;
        struct sched_param sched;

        err = pthread_getschedparam(pthread_self(), &policy, &sched);
        require_noerr(err, exit);

        sched.sched_priority = inPriority;
        err = pthread_setschedparam(pthread_self(), SCHED_RR, &sched);
        require_noerr(err, exit);
    }
#elif (TARGET_OS_WINDOWS)
    else {
        BOOL good;
        int priority;

        if (inPriority >= 64)
            priority = THREAD_PRIORITY_TIME_CRITICAL;
        else if (inPriority >= 52)
            priority = THREAD_PRIORITY_HIGHEST;
        else if (inPriority >= 32)
            priority = THREAD_PRIORITY_ABOVE_NORMAL;
        else if (inPriority >= 31)
            priority = THREAD_PRIORITY_NORMAL;
        else if (inPriority >= 11)
            priority = THREAD_PRIORITY_BELOW_NORMAL;
        else if (inPriority >= 1)
            priority = THREAD_PRIORITY_LOWEST;
        else
            priority = THREAD_PRIORITY_IDLE;

        good = SetThreadPriority(GetCurrentThread(), priority);
        err = map_global_value_errno(good, good);
        require_noerr(err, exit);
    }
#else
    else {
        dlogassert("Platform doesn't support setting thread priority");
        err = kUnsupportedErr;
        goto exit;
    }
#endif

exit:
    return (err);
}
