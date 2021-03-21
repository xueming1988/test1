#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Common/ptpLog.h"
#include "Common/ptpThread.h"
#include "ptpClockContext.h"
#include "ptpInterface.h"

#define PTP_INSTANCE_ID 0

// this is a global switch that controls printing of AirPlay debug messages
extern int debug_messages_enabled;
extern int debug_global_log_level;

int main(int argc, char* argv[])
{
    // enable all AirPlay logs
    debug_messages_enabled = true;
    debug_global_log_level = 0;

#ifdef PTP_USE_DEDICATED_LOGGING_THREAD
    ptpLogInit();
#endif

    ptpLogConfigure(5);

    if (argc > 2) {
        printf("starting PTP server connection on interface %s to peer %s\n", argv[1], argv[2]);
        ptpTest(PTP_INSTANCE_ID, argv[1], argv[2]);
        ptpTest(PTP_INSTANCE_ID + 1, argv[1], argv[2]);
    } else {
        printf("usage: ptpTest <interface_name> <remote_peer_ip>\n");
        exit(1);
    }

    while (1) {

        // retrieve current time
        Timestamp timestamp;
        ptpClockCtxGetTime(&timestamp);

        // convert it to nanoseconds
        double sysTimeNs = TIMESTAMP_TO_NANOSECONDS(&timestamp);

        // get last synchronization data
        PtpTimeData ptpTimeData;
        ptpRefresh(PTP_INSTANCE_ID, &ptpTimeData);
        ptpRefresh(PTP_INSTANCE_ID + 1, &ptpTimeData);

        // calculate remote time as a sum of systemTime and master clock offset
        NanosecondTs remoteTimeNs = sysTimeNs - ptpTimeData._masterToLocalPhase;
        PTP_LOG_INFO("local time: %lf s, master time: %lf s, master/local offset: %lf s", NANOSECONDS_TO_SECONDS(sysTimeNs), NANOSECONDS_TO_SECONDS(remoteTimeNs), -NANOSECONDS_TO_SECONDS(ptpTimeData._masterToLocalPhase));

        static NanosecondTs oldPhase = 0.0;
        NanosecondTs diff = ptpTimeData._masterToLocalPhase - oldPhase;
        PTP_LOG_INFO("master clock phase diff: %lf ms", NANOSECONDS_TO_MILLISECONDS(diff));

        if (oldPhase == 0.0)
            oldPhase = ptpTimeData._masterToLocalPhase;

        ptpThreadSleep(1000000);

        static int cnt = 0;

        if (cnt++ >= 500)
            break;
    }

    ptpStop(PTP_INSTANCE_ID);
    ptpStop(PTP_INSTANCE_ID + 1);

#ifdef PTP_USE_DEDICATED_LOGGING_THREAD
    ptpLogDeinit();
#endif
}
