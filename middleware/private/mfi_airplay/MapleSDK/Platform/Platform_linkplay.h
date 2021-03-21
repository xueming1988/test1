#ifdef LINKPLAY
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <ctype.h>
#include "CFUtils.h"
#include "DebugServices.h"
#include "NetUtils.h"
#include "StringUtils.h"

#include "AirPlayReceiverCommand.h"

#include "AirPlayReceiverServer.h"
#include "AirPlayReceiverSession.h"
#include "AirPlayReceiverSessionPriv.h"
#include "AirPlayUtils.h"
#include "AirPlayVersion.h"
#include "AudioUtils.h"
#include "AirPlayHomeIntegrationUtilities.h"

#include "wm_util.h"

#define MAX_AIRPLAY_NAME_SIZE 256
#define MAX_AIRPLAY_PSWD_SIZE 256
#define MAX_HOME_INGEGRATION_SETTING_SIZE     4*1024
#define MAX_AIRPLAY_GROUP_UUID_SIZE 512

#define MAX_AUDIO_BUFFER_SIZE (8 * 1024 * 1024) /* default 8M, min 3M */

#define OS_VESION_FILE "/proc/version"

#define WAC_INTERFACE_ADDRESS_FILE "/sys/class/net/wlan1/address"

#define WAN_INTERFACE_NAME "wlan0"

#if defined(PLATFORM_AMLOGIC)
#define OS_VESION_VALUE_FORMAT "Linux %s ARM"
#else
#define OS_VESION_VALUE_FORMAT "Linux %s MIPS32"
#endif

#define MAX_AIRPLAY_DEVICEID_SIZE 32
#define NVRAM_CFG_AIRPLAY_DEVID "AIRPLAY_DEVID"


#define AIRPLAY_FADE_MAX_VOLUME 0x10000

typedef struct _airplay_notify{
	char notify[32];
	long lposition;
	long lduration;
	char title[512];
	char artist[512];
	char album[512];
	char album_uri[128];
}airplay_notify;

typedef enum
{
    airplay_track_info_update_meta_txt = 0,
    airplay_track_info_update_progress,
    airplay_track_info_update_artwork_data,
}airplay_track_info_update_type_e;


typedef struct{
    //skew
    long long TsSinceAudioStarted;
    unsigned int RequestSkipSkew;

    //Silence
    int RequestSilence;

    //FadeInOut
    int RequestFadeIn;
    int RequestFadeOut;
    int RequiredFadeVolume;

    //Track change
    unsigned int TrackChanged;
    unsigned int SampleRate;

    Boolean isPlaying;

    Boolean AudioOutputPlaybackThreadExited;
    int64_t ExtraAudioDelayMs;
    Boolean RequestForceSkewAdjust;
}AirplayPrivControlContext;

extern char kAirplayPassword[];
extern char kAirplayName[];
extern int g_AirplayWaitingForWacConfiguredMsg;
extern AirplayPrivControlContext gAirplayPrivCtrlCtx;

#define MFI_AIRPLAY_GROUP_UUID "/vendor/wiimu/airplay_guuid"
#define MFI_AIRPLAY_NAME_CONF "/tmp/airplay_name"
#define MFI_AIRPLAY_PSWD_CONF "/tmp/airplay_password"
#define MFI_AIRPLAY_HM_INTEGRATION_DATA "/vendor/wiimu/airplay2HMdata"
#define MFI_AIRPLAY_EXTRA_DELAY "MFI_AIRPLAY_EXTRA_DELAY"
#define MFI_AIRPLAY_EXTRA_DELAY_MSG "{\"AirplayExtraDelay\":%lld}"

#define AirPlayReciverServerUpdateAirPlayPassword(SERVER)\
    AirPlayReceiverServerControl((SERVER), kCFObjectFlagDirect, CFSTR(kAirPlayProperty_Password), NULL, NULL, NULL)

#define AirPlayReciverServerUpdateAirPlayDevName(SERVER)\
    AirPlayReceiverServerControl((SERVER), kCFObjectFlagDirect, CFSTR(kAirPlayKey_Name), NULL, NULL, NULL)

#define AirPlayReciverServerUpdateConfigs(SERVER)\
    AirPlayReceiverServerControl((SERVER), kCFObjectFlagDirect, CFSTR(kAirPlayCommand_UpdateConfigs), NULL, NULL, NULL)


OSStatus AirPlayGetFileLineContent(const char *file_path, char *outBuf, size_t out_buf_size);

OSStatus AirPlayReciverHandleIncomingControl(
    AirPlayReceiverServerRef inServer,
    CFStringRef inCommand,
    CFTypeRef inQualifier,
    CFDictionaryRef inParams,
    CFDictionaryRef *outParams,
    void *inContext);

char *GetGlobalAirplayPassword(void);
char *GetGlobalAirplayDevName(void);

extern WIIMU_CONTEXT *g_wiimu_shm;

AirPlayReceiverServerRef AirPlayReceiverServerGetRef(void);

int platform_linkplay_init(void);

CFTypeRef platform_linkplay_handle_server_copy_property(AirPlayReceiverServerRef inServer,
        CFStringRef inProperty,
        CFTypeRef inQualifier,
        OSStatus *outErr,
        void *inContext);

CFTypeRef
platform_linkplay_handle_session_copy_property(AirPlayReceiverSessionRef inSession,
        CFStringRef inProperty,
        CFTypeRef inQualifier,
        OSStatus *outErr,
        void *inContext);

OSStatus AirPlayReceiverSessionSetPropertyHandler(
    AirPlayReceiverSessionRef inSession,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    CFTypeRef inValue,
    void *inContext);

OSStatus platform_linkplay_set_airplay_name(CFTypeRef inValue);
OSStatus platform_linkplay_set_airplay_pswd(CFTypeRef inValue);
OSStatus platform_linkplay_set_airplay_HM_integrationData(CFTypeRef inValue);
OSStatus platform_linkplay_set_group_uuid(CFTypeRef inValue);
double platform_linkplay_volume_percent_to_db(void);

#endif

