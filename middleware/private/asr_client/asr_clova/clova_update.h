
#ifndef __CLOVA_UPDATE_H__

// Namespace
#define CLOVA_NAMESPACE_SYSTME ("System")

// Name
#define CLOVA_NAME_START_UPDATE ("StartUpdate")
#define CLOVA_NAME_EXCEPTION ("Exception")
#define CLOVA_NAME_EXPECT_REPORT_UPDATE_READY_STATE ("ExpectReportUpdateReadyState")
#define CLOVA_NAME_EXPECT_REPORT_CLIENT_STATE ("ExpectReportClientState")
#define CLOVA_NAME_REPORT_UPDATE_READY_STATE ("ReportUpdateReadyState")
#define CLOVA_NAME_REPORT_CLIENT_STATE ("ReportClientState")

// Inside Payload
#define CLOVA_PAYLOAD_READY ("ready")
#define CLOVA_PAYLOAD_STATE ("state")
#define CLOVA_PAYLOAD_UPDATE ("update")
#define CLOVA_PAYLOAD_VERSION ("version")
#define CLOVA_PAYLOAD_NEW_VERSION ("newVersion")
#define CLOVA_PAYLOAD_PROGRESS ("progress")
#define CLOVA_PAYLOAD_LATEST ("LATEST")
#define CLOVA_PAYLOAD_AVAILABLE ("AVAILABLE")
#define CLOVA_PAYLOAD_DOWNLOADING ("DOWNLOADING")
#define CLOVA_PAYLOAD_UPDATING ("UPDATING")
#define CLOVA_PAYLOAD_HOLDING_DISTURB ("HOLDING_DISTURB")
#define CLOVA_PAYLOAD_HOLDING ("HOLDING")
#define CLOVA_PAYLOAD_FAIL_DOWNLOAD ("FAIL_DOWNLOAD")
#define CLOVA_PAYLOAD_FAIL_DOWNLOADING ("FAIL_DOWNLOADING")

// else
#define VERSION_FILE ("/system/workdir/MVver")

typedef enum { OTA_NONE, OTA_DOWNLOAD_FAIL } OTA_STATE_T;

typedef struct {
    int error;
    int flag;
    int enabled;
    int ready;
    int app_flag;
    int progress;
    char *state;
    char *version;
    char *new_version;
} UPDATE_T;

void update_OTA_version(const char *, const char *);

void update_OTA_state(const char *);

void update_OTA_process(const int);

#endif
