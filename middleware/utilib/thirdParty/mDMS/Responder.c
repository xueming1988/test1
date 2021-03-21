#include "Responder.h"
#include "mDNSEmbeddedAPI.h"
#include "mDNSClientAPI.h"// Defines the interface to the client layer above
#include "mDNSPosix.h"    // Defines the specific types needed to run mDNS on this platform
#include <assert.h>
#include <stdio.h>			// For printf()
#include <stdlib.h>			// For exit() etc.
#include <string.h>			// For strlen() etc.
#include <unistd.h>			// For select()
#include <errno.h>			// For errno, EINTR
//#include <signal.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>

#include <pthread.h>


static mDNS mDNSStorage; // mDNS core uses this to store its globals
static mDNS_PlatformSupport PlatformStorage; // Stores this platform's globals

static const char *gProgramName = "mDNSResponderPosix";

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark ***** Command Line Arguments
#endif

static const char kDefaultServiceType[] = "_raop._tcp.";
static const char kDefaultServiceDomain[] = "local.";
enum {
	kDefaultPortNumber = 548
};
static const char *gRichTextHostName = "3DB4DA366AE6@huaijing-air";
static const char *gServiceType = kDefaultServiceType;
static const char *gServiceDomain = kDefaultServiceDomain;
static mDNSu8 gServiceText[sizeof(RDataBody)];
static mDNSu16 gServiceTextLen = 0;
static int gPortNumber = kDefaultPortNumber;
//static const char *gServiceFile = "";


#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark ***** Registration
#endif

typedef struct PosixService PosixService;

struct PosixService {
	ServiceRecordSet coreServ;
	PosixService *next;
	int serviceID;
};

static PosixService *gServiceList = NULL;

static void RegistrationCallback(mDNS * const m,
		ServiceRecordSet * const thisRegistration, mStatus status)
// mDNS core calls this routine to tell us about the status of
// our registration.  The appropriate action to take depends
// entirely on the value of status.
{
	switch (status) {

	case mStatus_NoError:
		debugf("Callback: %##s Name Registered",
				thisRegistration->RR_SRV.resrec.name.c);
		// Do nothing; our name was successfully registered.  We may
		// get more call backs in the future.
		break;

	case mStatus_NameConflict:
		debugf("Callback: %##s Name Conflict",
				thisRegistration->RR_SRV.resrec.name.c);

		// In the event of a conflict, this sample RegistrationCallback
		// just calls mDNS_RenameAndReregisterService to automatically
		// pick a new unique name for the service. For a device such as a
		// printer, this may be appropriate.  For a device with a user
		// interface, and a screen, and a keyboard, the appropriate response
		// may be to prompt the user and ask them to choose a new name for
		// the service.
		//
		// Also, what do we do if mDNS_RenameAndReregisterService returns an
		// error.  Right now I have no place to send that error to.

		status = mDNS_RenameAndReregisterService(m, thisRegistration, mDNSNULL);
		assert(status == mStatus_NoError);
		break;

	case mStatus_MemFree:
		debugf("Callback: %##s Memory Free",
				thisRegistration->RR_SRV.resrec.name.c);

		// When debugging is enabled, make sure that thisRegistration
		// is not on our gServiceList.

#if !defined(NDEBUG)
		{
			PosixService *cursor;

			cursor = gServiceList;
			while (cursor != NULL) {
				assert(&cursor->coreServ != thisRegistration);
				cursor = cursor->next;
			}
		}
#endif
		//printf("%s ++ %d\n",__func__,__LINE__);
		if(NULL != thisRegistration){
			mDNSPlatformMemFree(thisRegistration);
		}

		//printf("%s ++ %d\n",__func__,__LINE__);
		break;

	default:
		debugf("Callback: %##s Unknown Status %d",
				thisRegistration->RR_SRV.resrec.name.c, status);
		break;
	}
}

static int gServiceID = 0;

static mStatus RegisterOneService(const char * richTextHostName,
		const char * serviceType, const char * serviceDomain,
		const mDNSu8 text[], mDNSu16 textLen, long portNumber) {
	mStatus status;
	PosixService * thisServ;
	domainlabel name;
	domainname type;
	domainname domain;
	//printf("RegisterOneService ++ \r\n");
	status = mStatus_NoError;
	thisServ = (PosixService *) mDNSPlatformMemAllocate(sizeof(*thisServ));
	if (thisServ == NULL) {
		status = mStatus_NoMemoryErr;
	}
	if (status == mStatus_NoError) {
		MakeDomainLabelFromLiteralString(&name, richTextHostName);
		MakeDomainNameFromDNSNameString(&type, serviceType);
		MakeDomainNameFromDNSNameString(&domain, serviceDomain);
		status = mDNS_RegisterService(&mDNSStorage, &thisServ->coreServ, &name,
				&type, &domain, // Name, type, domain
				NULL, mDNSOpaque16fromIntVal(portNumber), // Host and port
				text, textLen, // TXT data, length
				NULL, 0, // Subtypes
				mDNSInterface_Any, // Interace ID
				RegistrationCallback, thisServ,0); // Callback and context
	}
	if (status == mStatus_NoError) {
		thisServ->serviceID = gServiceID;
		gServiceID += 1;

		thisServ->next = gServiceList;
		gServiceList = thisServ;

		if (gMDNSPlatformPosixVerboseLevel > 0) {
			//fprintf(stderr,
			//		"%s: Registered service %d, name '%s', type '%s', port %ld\n",
			//		gProgramName, thisServ->serviceID, richTextHostName,
			//		serviceType, portNumber);
		}
	} else {
		if (thisServ != NULL) {
			mDNSPlatformMemFree(thisServ); //free
			thisServ = NULL;
		}
	}
	return status;
}

static void DeregisterOurServices(void) {
	PosixService *thisServ;
	int thisServID;

	while (gServiceList != NULL) {
		thisServ = gServiceList;
		gServiceList = thisServ->next;

		thisServID = thisServ->serviceID;

		mDNS_DeregisterService(&mDNSStorage, &thisServ->coreServ);

		if (gMDNSPlatformPosixVerboseLevel > 0) {
			//fprintf(stderr, "%s: Deregistered service %d\n", gProgramName,
			//		thisServ->serviceID);
		}
	}
}

#if COMPILER_LIKES_PRAGMA_MARK
#pragma mark **** Main
#endif

#ifdef NOT_HAVE_DAEMON

// The version of Solaris that I tested on didn't have the daemon
// call.  This implementation was basically stolen from the
// Mac OS X standard C library.

static int daemon(int nochdir, int noclose)
{
	int fd;

	switch (fork()) {
		case -1:
		return (-1);
		case 0:
		break;
		default:
		_exit(0);
	}

	if (setsid() == -1)
	return (-1);

	if (!nochdir)
	(void)chdir("/");

	if (!noclose && (fd = _open("/dev/null", O_RDWR, 0)) != -1) {
		(void)dup2(fd, STDIN_FILENO);
		(void)dup2(fd, STDOUT_FILENO);
		(void)dup2(fd, STDERR_FILENO);
		if (fd > 2)
		(void)_close(fd);
	}
	return (0);
}

#endif /* NOT_HAVE_DAEMON */

static pthread_t m_thread = 0;
static volatile mDNSBool running = mDNSfalse;
static int g_InstanceId = 0;
extern long long timeMsSince2000(void);
static int is_ip_changed(void)
{
	static int ra0_ip = -1, apcli0_ip = -1, eth2_ip = -1;
	int new_ra0_ip = -1, new_apcli0_ip = -1, new_eth2_ip = -1;
	int ret = 0;
	static long long old_ts = 0;
	long long delta_ts_minutes = (timeMsSince2000() - old_ts)/1000/60;
	old_ts = timeMsSince2000();
	GetHostIP(&new_ra0_ip);
	GetEthIP(&new_eth2_ip);
	GetStationIP(&new_apcli0_ip);
	if( (ra0_ip != new_ra0_ip) || (apcli0_ip != new_apcli0_ip) ||
		( eth2_ip != new_eth2_ip ) )
	{
		fprintf(stderr, "\n\n===%s (ra0 %.8x apcli %.8x eth2 %.8x) ==> (ra0 %.8x apcli %.8x eth2 %.8x)\n",
			__FUNCTION__, ra0_ip, apcli0_ip, eth2_ip, new_ra0_ip, new_apcli0_ip, new_eth2_ip);
		ra0_ip = new_ra0_ip;
		eth2_ip = new_eth2_ip;
		apcli0_ip = new_apcli0_ip;
		return 1;
	}
	// reset every 24 hours
	if( delta_ts_minutes > 24*60 || delta_ts_minutes < -24*60 )
	{
		fprintf(stderr, "%s ===delta_ts_minutes = %lld old_ts=%lld \n", __FUNCTION__, delta_ts_minutes);
		return 1;
	}
	return 0;
}

void* threadCallback()
{
	mStatus status;
	//printf("running = %d \r\n",running);

		while (running) {
		//	printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!11 I am responder thread  !!!!\n");
			int nfds = 0;
			fd_set readfds;
			struct timeval timeout;
			int result;

			// 1. Set up the fd_set as usual here.
			// This example client has no file descriptors of its own,
			// but a real application would call FD_SET to add them to the set here
			FD_ZERO(&readfds);

			// 2. Set up the timeout.
			// This example client has no other work it needs to be doing,
			// so we set an effectively infinite timeout
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;

			// 3. Give the mDNSPosix layer a chance to add its information to the fd_set and timeout
			mDNSPosixGetFDSet(&mDNSStorage, &nfds, &readfds, &timeout);

			// 4. Call select as normal
			verbosedebugf("select(%d, %d.%06d)", nfds, timeout.tv_sec,
					timeout.tv_usec);
			timeout.tv_sec = 1;

			//printf("Responder::before select() nfds:%d\n",nfds);
			result = select(nfds, &readfds, NULL, NULL, &timeout);

			if (result < 0) {
				verbosedebugf("select() returned %d errno %d", result, errno);
				printf("Responder::select() returned %d errno %s", result, strerror(errno));
				if (errno != EINTR) running = mDNSfalse;
				else
				  running = mDNStrue;
			} else {
				// 5. Call mDNSPosixProcessFDSet to let the mDNSPosix layer do its work
				mDNSPosixProcessFDSet(&mDNSStorage, &readfds);
				//usleep(100);

				// 6. This example client has no other work it needs to be doing,
				// but a real client would do its work here
				// ... (do work) ...
			}

			if( is_ip_changed() )
			{
				mDNSPlatformPosixRefreshInterfaceList(&mDNSStorage);
				SocketClientReadWriteMsg("/tmp/RequestAirplay","MdnsIPChanged",strlen("MdnsIPChanged"),NULL,NULL,0);
			}
		}
	//	DeregisterOurServices();
	//	mDNS_Close(&mDNSStorage);
		return NULL;
}


int mDNS_init()
{
	if(running == mDNStrue){
		//printf("mDNS thread is already running~\n");
		return g_InstanceId;
	}

	mStatus status;
	status = mDNS_Init(&mDNSStorage, &PlatformStorage, mDNS_Init_NoCache,
			mDNS_Init_ZeroCacheSize, mDNS_Init_AdvertiseLocalAddresses,
			mDNS_Init_NoInitCallback, mDNS_Init_NoInitCallbackContext);
	if (status != mStatus_NoError){
		printf("mDNS_Init error~  %d\n",__LINE__);
		return -1;
	}

	running = mDNStrue;
	int ret;
	if((ret = pthread_create(&m_thread, NULL, threadCallback, NULL)) != 0){
		printf("mDNS thread create error~\n");
		running = mDNSfalse;
		mDNS_Close(&mDNSStorage);
		return -1;
	}
	else{
		//printf("mDNS thread is running~\n");
	}
	g_InstanceId++;
	return g_InstanceId;
}


int mDNS_publishOneService(char* name, char * serviceType, int port, char* text[], int textLen)
{
	mStatus status;

	if(running == mDNSfalse){
		printf("mDNS is not inited!! -_- \n");
		return -1;
	}

	gRichTextHostName = name;
	gServiceType = serviceType;
	gPortNumber = port;
	gServiceTextLen = 0;
	int i;
	for (i = 0; i < textLen;  i++) {
		gServiceText[gServiceTextLen] = strlen(text[i]);
		mDNSPlatformMemCopy(gServiceText + gServiceTextLen + 1,text[i],gServiceText[gServiceTextLen]);
		gServiceTextLen += 1 + gServiceText[gServiceTextLen];
	}

	status = mStatus_UnknownErr;
	if (gRichTextHostName[0] != 0) {
		status = RegisterOneService(gRichTextHostName, gServiceType,
		gServiceDomain, gServiceText, gServiceTextLen, gPortNumber);
	}
	if(status == mStatus_NoError){
		printf("mDNS Registered service succeed ^_^ ^_^ ^_^!! name '%s', type '%s', port %ld\n",gRichTextHostName,gServiceType,gPortNumber);
		return 0;
	}else{
		printf("mDNS Registered service failed -_- -_- -_- !! name '%s', type '%s', port %ld\n",gRichTextHostName,gServiceType,gPortNumber);
		return -1;
	}
}
void mDMS_DeregisterServices()
{
	DeregisterOurServices();

}
int mDNS_release(int instanceId)
{
	mStatus status;
	int result;
	//printf("mDNS_release \r\n");
	if(running == mDNSfalse || instanceId != g_InstanceId){
		return 0;
	}

	if(m_thread != 0){
		running = mDNSfalse;
		//printf("wait thread over~\n");
		pthread_join(m_thread,NULL);
		m_thread = NULL;
		//printf("thread is over~\n");
	}

	DeregisterOurServices();
	mDNS_Close(&mDNSStorage);

	if (status == mStatus_NoError) {
		result = 0;
	} else {
		result = 2;
	}
	if ((result != 0) || (gMDNSPlatformPosixVerboseLevel > 0)) {
		//fprintf(stderr, "%s: Finished with status %ld, result %d\n",
		//		gProgramName, status, result);
	}

	return result;
}

