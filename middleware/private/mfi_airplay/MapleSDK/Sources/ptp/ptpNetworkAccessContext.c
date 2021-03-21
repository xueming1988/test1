#include "ptpNetworkAccessContext.h"
#include "AtomicUtils.h"
#include "Common/ptpList.h"

#include <assert.h>

typedef struct NetworkAccessContext {
    int _referenceCount;
    char* _interfaceName;
    struct NetIf* _networkInterface;
    PtpThreadRef _selectThread;
    ListNetworkPortRef _networkPorts;
} NetworkAccessContext;

static PtpMutexRef ptpNetworkAccessContextMutexInstance(void);

static void ptpNetworkAccessContextMutexInitializerCallback(void* ctx)
{
    PtpMutexRef* cs = (PtpMutexRef*)ctx;

    if (cs && !(*cs)) {
        *cs = ptpMutexCreate(NULL);
    }
}

/**
 * @brief acquires reference to PTPInterface singleton
 * @return PTPInterface singleton
 */
static PtpMutexRef ptpNetworkAccessContextMutexInstance(void)
{
    // atomically create interfaces mutex object
    static PtpMutexRef cs = NULL;

    // do a quick atomic check whether the interface has already been initialized (it must be non zero)
    if (atomic_add_and_fetch_32((int32_t*)&cs, 0) != 0)
        return cs;

    // interace pointer is zero, execute thread safe callback to allocate it:
    ptpThreadSafeCallback(ptpNetworkAccessContextMutexInitializerCallback, &cs);
    return cs;
}

NetworkAccessContextRef ptpNetworkAccessContextCreate(NetworkPortRef port, const char* ifname, MacAddressRef localAddress)
{
    static NetworkAccessContext g_networkAccessContext;
    NetworkAccessContextRef context = &g_networkAccessContext;

    // lock global network access context mutex
    ptpMutexLock(ptpNetworkAccessContextMutexInstance());

    if (!context->_referenceCount) {
        context->_selectThread = NULL;
        context->_networkInterface = NULL;
        context->_interfaceName = strdup(ifname);

        context->_networkPorts = ptpListNetworkPortCreate();

        // create network interface
        context->_networkInterface = ptpNetIfCreate();
        ptpNetIfCtor(context->_networkInterface, context->_interfaceName);

        // read mac address of our network interface
        ptpNetIfGetMacAddress(context->_networkInterface, localAddress);
    }

    // add port to the list of ports
    ptpListNetworkPortPushBack(context->_networkPorts, port);
    context->_referenceCount++;

    // release global network access context mutex
    ptpMutexUnlock(ptpNetworkAccessContextMutexInstance());
    return context;
}

void ptpNetworkAccessContextRelease(NetworkAccessContextRef context, NetworkPortRef port)
{
    ThreadExitCode exitCode;
    assert(context);

    // lock global network access context mutex
    ptpMutexLock(ptpNetworkAccessContextMutexInstance());
    context->_referenceCount--;

    // remove port from the list of ports
    ListNetworkPortIterator it;
    for (it = ptpListNetworkPortFirst(context->_networkPorts); it != NULL; it = ptpListNetworkPortNext(it)) {
        if (ptpListNetworkPortIteratorValue(it) == port) {
            ptpListNetworkPortErase(context->_networkPorts, it);
            break;
        }
    }

    // if there are no live reference, deallocate resources
    if (context->_referenceCount <= 0) {

        // shut down network interface
        if (context->_networkInterface) {
            ptpNetIfShutdown(context->_networkInterface);
        }

        // release reception thread
        if (ptpThreadExists(context->_selectThread)) {
            ptpThreadJoin(context->_selectThread, &exitCode);
            ptpThreadDestroy(context->_selectThread);
        }

        // release network interface
        if (context->_networkInterface) {
            ptpNetIfDtor(context->_networkInterface);
        }
        free(context->_interfaceName);

        // release network ports list
        ptpListNetworkPortDestroy(context->_networkPorts);
    }

    // release global network access context mutex
    ptpMutexUnlock(ptpNetworkAccessContextMutexInstance());
}

static Boolean ptpNetworkAccessContextProcessingLoop(NetworkAccessContextRef context)
{
    Boolean ok = true;
    assert(context);

    while (1) {

        // Check for messages and process them
        LinkLayerAddress remote;
        Timestamp ingressTime;

        uint8_t buf[PTP_MAX_MESSAGE_SIZE];
        size_t length = sizeof(buf);
        NetResult rrecv;

        assert(context);
        ptpLinkLayerAddressCtor(&remote);

        //
        // receive message from network
        //

        rrecv = ptpNetIfReceive(context->_networkInterface, &remote, buf, &length, &ingressTime);

        // distribute received message to all ports
        ListNetworkPortIterator it;
        for (it = ptpListNetworkPortFirst(context->_networkPorts); it != NULL; it = ptpListNetworkPortNext(it)) {
            ptpNetPortProcessMessage(ptpListNetworkPortIteratorValue(it), &remote, buf, length, ingressTime, rrecv);
        }

        if (rrecv == eNetFatal) {
            PTP_LOG_ERROR("failed while checking for network messages");
            ok = false;
            break;
        }
    }

    return ok;
}

static ThreadExitCode ptpNetworkAccessContextThread(void* arg)
{
    PTP_LOG_VERBOSE("ptpNetworkAccessContextThread");

#if TARGET_OS_BRIDGECO
    // give it a high priority
    unsigned int oldPriority = 0;
    osThreadPriorityChange(osThreadIdentify(), 15, &oldPriority);
#endif

    NetworkAccessContextRef context = (NetworkAccessContextRef)arg;
    return ptpNetworkAccessContextProcessingLoop(context) ? threadExitOk : threadExitErr;
}

Boolean ptpNetworkAccessContextStartProcessing(NetworkAccessContextRef context)
{
    Boolean ret = true;
    assert(context);

    ptpMutexLock(ptpNetworkAccessContextMutexInstance());
    if (!context->_selectThread) {
        PTP_LOG_VERBOSE("Starting event port thread");
        context->_selectThread = ptpThreadCreate();
        ret = context->_selectThread && ptpThreadStart(context->_selectThread, ptpNetworkAccessContextThread, (void*)context);
    }
    ptpMutexUnlock(ptpNetworkAccessContextMutexInstance());
    return ret;
}

NetResult ptpNetworkAccessContextSend(NetworkAccessContextRef context, LinkLayerAddressRef addr, uint8_t* payload, size_t length, TimestampRef txTimestamp, Boolean isEvent)
{
    assert(context);

    ptpMutexLock(ptpNetworkAccessContextMutexInstance());
    NetResult ret = ptpNetIfSend(context->_networkInterface, addr, payload, length, txTimestamp, isEvent);
    ptpMutexUnlock(ptpNetworkAccessContextMutexInstance());
    return ret;
}

Boolean ptpNetworkAccessContextGetLinkSpeed(NetworkAccessContextRef context, uint32_t* speed)
{
    assert(context & &speed);
    return ptpNetIfGetLinkSpeed(context->_networkInterface, speed);
}
