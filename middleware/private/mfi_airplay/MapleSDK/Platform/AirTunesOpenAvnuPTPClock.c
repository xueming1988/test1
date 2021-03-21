/*
    Copyright (C) 2017-2019 Apple Inc. All Rights Reserved. Not to be used or disclosed without permission from Apple.
 
    This is a reference example of MapleSDK/Platform/AirTunePTPClock.c, referring to the OpenAvnu implementation
    of Apple PTP Profile (Documentation/apple_public_ptp_profile.pdf).
 
    A 3rd party implementation of aPTP for OpenAVNU can be found at https://github.com/rroussel/OpenAvnu/tree/ArtAndLogic-aPTP-changes
 */

#include "AirTunesClock.h"
#include "AirTunesClockPriv.h"
#include "AirTunesPTPClock.h"
#include "DebugServices.h"
#include "TickUtils.h"
#include <fcntl.h> //    For O_* constants
#include <netdb.h> //    For getnameinfo
#include <pthread.h>
#include <string.h> //    For strcpy and strcat
#include <sys/mman.h>
#include <sys/stat.h> //    For mode constants
#include <sys/wait.h>

typedef struct
{
    struct AirTunesClockPrivate header; // An AirTunesClock always starts with this
    pid_t pid; //    The process ID of the PTP process
    in_port_t msg_port; //    The port number to send message to aPTP daemon for add/remove peers
    int msg_fd; //    The file descriptor for the port to send message to PTP daemon for add/remove peers
    int shm_fd; //    The file descriptor to hold the shared memory with PTP process
    pthread_mutex_t* shm_map; //    The memory map pointer to share with aPTP process
} AirTunesPTPClock;

typedef struct {
    int64_t ml_phoffset;
    int64_t ls_phoffset;
    double ml_freqoffset;
    double ls_freqoffset;
    uint64_t local_time;
    uint64_t clockID;
    uint16_t addressRegistrationSocketPort;
} gPtpTimeData;

#define SHM_NAME "/ptp" //    The default shared memory map file path used by aPTP
#define SHM_SIZE (sizeof(gPtpTimeData) + sizeof(pthread_mutex_t))

static int dead_children = 0; /* Dead child processes? */

DEBUG_STATIC void sigchld_handler(int sig);
DEBUG_STATIC void process_children(AirTunesPTPClock* ptp);
DEBUG_STATIC OSStatus AirTunesPTPClock_Finalize(AirTunesClockRef inClock);
DEBUG_STATIC Boolean AirTunesPTPClock_Adjust(AirTunesClockRef inClock, int64_t inOffsetNanoseconds, Boolean inReset);
DEBUG_STATIC void AirTunesPTPClock_GetNetworkTime(AirTunesClockRef inClock, AirTunesTime* outTime);
DEBUG_STATIC void AirTunesPTPClock_UpTicksToNetworkTime(AirTunesClockRef inClock, AirTunesTime* outTime, uint64_t inTicks);
DEBUG_STATIC OSStatus AirTunesPTPClock_ConvertNetworkTimeToUpTicks(AirTunesClockRef inClock, AirTunesTime inNetworkTime, uint64_t* outUpTicks);
DEBUG_STATIC OSStatus AirTunesPTPClock_AddPeer(AirTunesClockRef inClock, CFStringRef inInterfaceName, const void* inSockAddr);
DEBUG_STATIC OSStatus AirTunesPTPClock_RemovePeer(AirTunesClockRef ptp, CFStringRef inInterfaceName, const void* inSockAddr);

// Debugging

ulog_define(AirTunesAvnuClock, kLogLevelTrace, kLogFlags_Default, "AirTunesAvnuClock", NULL);
#define atvu_ucat() &log_category_from_name(AirTunesAvnuClock)
#define atvu_ulog(LEVEL, ...) ulog(atvu_ucat(), (LEVEL), __VA_ARGS__)

#define kPTPProcName "daemon_cl"

DEBUG_STATIC void sigchld_handler(int sig)
{
    (void)sig;

    // We have dead children who's exit code needs to be collected
    dead_children = 1;
}

DEBUG_STATIC void process_children(AirTunesPTPClock* ptp)
{
    pid_t pid;
    int status;
    int code;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFSIGNALED(status)) {
            atvu_ulog(kLogLevelError, "ptp process %d stopped on signal %d\n", pid, WTERMSIG(status));
        } else {
            if ((code = WEXITSTATUS(status)) != 0)
                atvu_ulog(kLogLevelTrace, "ptp process %d stopped with status %d\n", pid, code);
            else
                atvu_ulog(kLogLevelTrace, "ptp process %d exited with no errors\n", pid);
        }

        if (pid == ptp->pid)
            ptp->pid = 0; // Forget the ptp child
    }

    // As long as we weren't interrupted by a signal all children have been processed
    if (!(pid < 0 && errno == EINTR))
        dead_children = 0;
}

static OSStatus PTPClockLaunch(AirTunesPTPClock* ptp, CFStringRef inInterfaceName)
{
    OSStatus err = kNoErr;
    char ptpPath[PATH_MAX] = { 0 }; //    readlink does not null terminate!

    if (readlink("/proc/self/exe", ptpPath, PATH_MAX) == -1) {
        err = errno;
    } else {
        //    Set ptpPath string to be the common directory path
        char* rSlash = strrchr(ptpPath, '/');
        strcpy(rSlash + 1, "ptp/");

        //    Copy that common path into cfgPath and add the config file name to it
        char cfgPath[PATH_MAX];
        strcpy(cfgPath, ptpPath);
        strcat(cfgPath, "aptp_cfg.ini");

        //    Redirect the PTP logs to a separate file
        char logPath[PATH_MAX];
        strcpy(logPath, ptpPath);
        strcat(logPath, "aptp.log");

        //    Now we can re-modify ptpPath and add the executable file name to the end
        strcat(ptpPath, kPTPProcName);

        //    Launch the ptp process
        char cInterface[32];
        Boolean success = CFStringGetCString(inInterfaceName, cInterface, sizeof(cInterface), kCFStringEncodingASCII);
        require_action(success, exit, err = kParamErr);

        system("killall " kPTPProcName " >/dev/null 2>&1"); //    In case there were any leftovers from before

        // Setup signal handler and block SIGCHLD until after the fork
        sigset(SIGCHLD, sigchld_handler);
        sighold(SIGCHLD);

        char* arglist[] = { ptpPath, cInterface, "-F", cfgPath, "-G", "root", "-L", NULL };

        ptp->pid = fork();

        if (ptp->pid == 0) {
            // Child process
            int fd = creat(logPath, S_IRUSR | S_IWUSR);
            dup2(fd, 1);
            dup2(fd, 2);
            close(fd);

            // Unblock signals before the exec
            sigrelse(SIGCHLD);

            execv(ptpPath, arglist);

            perror(ptpPath);
            exit(100 + errno);
        } else {
            // Parent continues...
            sigrelse(SIGCHLD);

            require_action(ptp->pid > 0, exit, err = errno);

            atvu_ulog(kLogLevelTrace, "PTPClockLaunch: launched PID %d\n", (int)ptp->pid);

            // Give child a chance to get up to speed
            usleep(200000);
        }
    }

exit:
    return err;
}

static OSStatus PTPClockReadShm(AirTunesPTPClock* ptp, gPtpTimeData* shmData)
{
    OSStatus err = kNoErr;

    // Process any children that have exited
    if (dead_children)
        process_children(ptp);

    // If we have a ptp child read the shared memory
    if (ptp->pid > 0) {
        pthread_mutex_lock(ptp->shm_map);

        memcpy(shmData, (void*)ptp->shm_map + sizeof(pthread_mutex_t), sizeof(gPtpTimeData));

        pthread_mutex_unlock(ptp->shm_map);

        if (shmData->addressRegistrationSocketPort == 0)
            err = kParamErr;
    } else {
        atvu_ulog(kLogLevelNotice, "PTPClockReadShm ptp process died!!!\n");
        err = kExecutionStateErr;
    }

    return err;
}

static OSStatus PTPClockOpenMsgPort(AirTunesPTPClock* ptp)
{
    OSStatus err;
    gPtpTimeData shmData;

    atvu_ulog(kLogLevelTrace, "PTPClockOpenMsgPort\n");

    if ((err = PTPClockReadShm(ptp, &shmData)) == 0) {
        ptp->msg_port = shmData.addressRegistrationSocketPort;
        if ((ptp->msg_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
            err = errno;
    }
    return err;
}

//===========================================================================================================================
//    AirTunesPTPClock_Create
//===========================================================================================================================

OSStatus AirTunesPTPClock_Create(AirTunesClockSettings* ptpSettings, AirTunesClockRef* outRef)
{
    OSStatus err = kNoErr;
    AirTunesPTPClock* ptp = (AirTunesPTPClock*)calloc(1, sizeof(AirTunesPTPClock));
    require_action(ptp, exit, err = kNoMemoryErr);

    ptp->header.class = kAirTunesClockPTP;
    ptp->header.vtable.finalize = AirTunesPTPClock_Finalize;
    ptp->header.vtable.adjust = AirTunesPTPClock_Adjust;
    ptp->header.vtable.getNetworkTime = AirTunesPTPClock_GetNetworkTime;
    ptp->header.vtable.upTicksToNetworkTime = AirTunesPTPClock_UpTicksToNetworkTime;
    ptp->header.vtable.netTimeToUpTicks = AirTunesPTPClock_ConvertNetworkTimeToUpTicks;
    ptp->header.vtable.addPeer = AirTunesPTPClock_AddPeer;
    ptp->header.vtable.removePeer = AirTunesPTPClock_RemovePeer;
    (void)ptpSettings;

    //    As a hack, the clock creation code is moved to AddPeer, so that we know which interface to use
    dead_children = 0;
    ptp->shm_fd = -1;
    ptp->msg_fd = -1;

    *outRef = &ptp->header;
exit:
    // Clean up on error
    if (err && ptp) {
        AirTunesClock_Finalize(&ptp->header);
        *outRef = NULL;
    }

    return (err);
}

//===========================================================================================================================
//    AirTunesPTPClock_Finalize
//===========================================================================================================================

DEBUG_STATIC OSStatus AirTunesPTPClock_Finalize(AirTunesClockRef inClock)
{
    OSStatus err = kNoErr;

    atvu_ulog(kLogLevelTrace, "AirTunesPTPClock_Finalize\n");

    if (inClock) {
        AirTunesPTPClock* ptp = (AirTunesPTPClock*)inClock;

        if (ptp->shm_map && ptp->shm_map != MAP_FAILED) {
            if (munmap(ptp->shm_map, SHM_SIZE) == -1)
                err = errno;
            ptp->shm_map = NULL;
        }

        close(ptp->shm_fd);
        close(ptp->msg_fd);

        ptp->shm_fd = -1;
        ptp->msg_fd = -1;

        if (ptp->pid > 0) {
            int count;
            kill(ptp->pid, SIGTERM);
            atvu_ulog(kLogLevelNotice, "%s: SIGTERM  %d\n", __func__, ptp->pid);
            for (count = 0; ptp->pid > 0 && count <= 5; count++) {
                usleep(20 * 1000);
                process_children(ptp);
            }

            /* If too slow to die, kill it  */
            if (ptp->pid > 0) {
                kill(ptp->pid, SIGKILL);
                atvu_ulog(kLogLevelNotice, "%s: SIGKILL  %d\n", __func__, ptp->pid);
                for (count = 0; ptp->pid > 0 && count <= 5; count++) {
                    usleep(20 * 1000);
                    process_children(ptp);
                }
            }
            ptp->pid = kInvalidFD;
        }

        free(inClock);
        inClock = NULL;
    }

    return (err);
}

//===========================================================================================================================
//    AirTunesPTPClock_Adjust
//===========================================================================================================================

DEBUG_STATIC Boolean AirTunesPTPClock_Adjust(AirTunesClockRef inClock, int64_t inOffsetNanoseconds, Boolean inReset)
{
    // Can't adjust PTP clock.
    (void)inClock;
    (void)inOffsetNanoseconds;
    return (inReset);
}

//===========================================================================================================================
//    AirTunesPTPClock_GetNetworkTime
//===========================================================================================================================
static OSStatus PTPClockGetTimeInNano(AirTunesPTPClock* ptp, uint64_t* nanoTime, uint64_t* clockID)
{
    struct timespec sysClock;
    OSStatus err = clock_gettime(CLOCK_REALTIME, &sysClock);
    require_noerr_action(err, exit, err = errno);
    uint64_t sysTime = sysClock.tv_sec * NSEC_PER_SEC + sysClock.tv_nsec;

    //    Get the PTP data in the shared memory
    gPtpTimeData shmData;
    err = PTPClockReadShm(ptp, &shmData);
    require_noerr(err, exit);
    *clockID = shmData.clockID;

    //    Since local_time is always much larger than ml_phoffset, it is safer to do the minus operation first
    *nanoTime = shmData.local_time - shmData.ml_phoffset + shmData.ml_freqoffset * ((double)sysTime - shmData.local_time);
exit:
    return err;
}

DEBUG_STATIC void AirTunesPTPClock_GetNetworkTime(AirTunesClockRef inClock, AirTunesTime* outTime)
{
    AirTunesPTPClock* ptp = (AirTunesPTPClock*)inClock;
    uint64_t localTime;
    if (kNoErr == PTPClockGetTimeInNano(ptp, &localTime, &outTime->timelineID)) {
        AirTunesTime_FromNanoseconds(outTime, localTime);
    }
    //    If we failed to get the PTP data, just leave the "outTime" content as is
}

//===========================================================================================================================
//    AirTunesPTPClock_GetSynchronizedTimeNearUpTicks
//===========================================================================================================================

DEBUG_STATIC void AirTunesPTPClock_UpTicksToNetworkTime(AirTunesClockRef inClock, AirTunesTime* outTime, uint64_t inTicks)
{
    AirTunesPTPClock* ptp = (AirTunesPTPClock*)inClock;
    uint64_t nowNetNanos;
    OSStatus err;

    if ((err = PTPClockGetTimeInNano(ptp, &nowNetNanos, &outTime->timelineID)) || outTime->timelineID == 0)
        AirTunesTime_Invalidate(outTime, err == kExecutionStateErr ? AirTunesTimeFlag_StateErr : AirTunesTimeFlag_Invalid);
    else {
        uint64_t nowTicks = UpTicks();
        if (inTicks >= nowTicks) {
            nowNetNanos = nowNetNanos + UpTicksToNanoseconds(inTicks - nowTicks);
        } else {
            nowNetNanos = nowNetNanos - UpTicksToNanoseconds(nowTicks - inTicks);
        }
        AirTunesTime_FromNanoseconds(outTime, nowNetNanos);
    }
}

//===========================================================================================================================
//    AirTunesPTPClock_ConvertNetworkTimeToUpTicks
//===========================================================================================================================
OSStatus AirTunesPTPClock_ConvertNetworkTimeToUpTicks(AirTunesClockRef inClock, AirTunesTime inNetworkTime, uint64_t* outTicks)
{
    AirTunesPTPClock* ptp = (AirTunesPTPClock*)inClock;
    uint64_t nowNano, clockID;
    OSStatus err = PTPClockGetTimeInNano(ptp, &nowNano, &clockID);
    if (!err && clockID != inNetworkTime.timelineID) {
        err = kAirTunesClockError_TimelineIDMismatch;
        atvu_ulog(kLogLevelNotice, "%s: Timeline Mismatch desired=%llx timeline=%llx\n", __func__, inNetworkTime.timelineID, clockID);
    }
    if (!err) {
        uint64_t inNano = AirTunesTime_ToNanoseconds(&inNetworkTime);
        uint64_t nowTicks = UpTicks();
        if (inNano >= nowNano) {
            *outTicks = nowTicks + NanosecondsToUpTicks(inNano - nowNano);
        } else {
            *outTicks = nowTicks - NanosecondsToUpTicks(nowNano - inNano);
        }
    }

    return err;
}
//===========================================================================================================================
//    AirTunesPTPClock_RemovePeer
//===========================================================================================================================
typedef enum { AddressApiAdd = 1,
    AddressApiDelete } AddressApiMessageType;
typedef struct {
    uint8_t opCode;
    uint8_t ipAddress[45];
} MessageData;

static OSStatus PTPClockSendMessage(AirTunesPTPClock* ptp, AddressApiMessageType opCode, const struct sockaddr* inSockAddr)
{
    OSStatus err = kNoErr;

    //    Get the IP(v6) address string off the record
    MessageData msgData = { opCode, { 0 } }; //    PTP daemon doesn't stop reading at the terminating '\0'
    socklen_t sockLen = 0;
    if (inSockAddr->sa_family == AF_INET)
        sockLen = sizeof(struct sockaddr_in);
#ifdef AF_INET6
    else if (inSockAddr->sa_family == AF_INET6)
        sockLen = sizeof(struct sockaddr_in6);
#endif //AF_INET6
    require_action(sockLen, exit, err = kParamErr);
    err = getnameinfo(inSockAddr, sockLen, (char*)msgData.ipAddress, sizeof(msgData.ipAddress), NULL, 0, NI_NUMERICHOST);
    require_noerr(err, exit);
    atvu_ulog(kLogLevelInfo, "Device at ip address %s is %s the clock group.\n", msgData.ipAddress, AddressApiAdd == opCode ? "added into" : "removed from");

    //    Send the message block to the message port where aPTP listens
    struct timeval timeout = { 0, 16000 }; // 16 ms
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(ptp->msg_fd, &fds);
    if (1 == select(ptp->msg_fd + 1, NULL, &fds, NULL, &timeout) && FD_ISSET(ptp->msg_fd, &fds)) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(ptp->msg_port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (-1 == sendto(ptp->msg_fd, &msgData, sizeof(msgData), 0, (const struct sockaddr*)&addr, sizeof(addr)))
            err = errno;
        require_noerr(err, exit);
    } else
        require_action(0, exit, err = errno);
exit:
    return err;
}

OSStatus AirTunesPTPClock_RemovePeer(AirTunesClockRef inClock, CFStringRef inInterfaceName, const void* inSockAddr)
{
    (void)inInterfaceName;
    return PTPClockSendMessage((AirTunesPTPClock*)inClock, AddressApiDelete, (const struct sockaddr*)inSockAddr);
}

//===========================================================================================================================
//    AirTunesPTPClock_AddPeer
//===========================================================================================================================

DEBUG_STATIC OSStatus AirTunesPTPClock_AddPeer(AirTunesClockRef inClock, CFStringRef inInterfaceName, const void* inSockAddr)
{
    AirTunesPTPClock* ptp = (AirTunesPTPClock*)inClock;
    OSStatus err = kNoErr;

    if (ptp->pid <= 0) {
        err = PTPClockLaunch(ptp, inInterfaceName);
        require_noerr(err, exit);

        ptp->shm_fd = shm_open(SHM_NAME, O_RDWR, 0);
        require_action(-1 != ptp->shm_fd, exit, err = errno);

        ptp->shm_map = (pthread_mutex_t*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, ptp->shm_fd, 0);
        require_action(MAP_FAILED != ptp->shm_map, exit, err = errno);

        err = PTPClockOpenMsgPort(ptp);
        require_noerr(err, exit);
    }

    err = PTPClockSendMessage(ptp, AddressApiAdd, (const struct sockaddr*)inSockAddr);

exit:

    return err;
}
