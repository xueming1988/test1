#ifdef LINKPLAY
#include "Platform_linkplay.h"
#include "TickUtils.h"

#ifdef AMLOGIC_A113
#include <resolv.h>
#endif

extern AirPlayReceiverServerRef gAirPlayServer;
WIIMU_CONTEXT *g_wiimu_shm = NULL;

char kAirplayPassword[MAX_AIRPLAY_PSWD_SIZE] = {0};
char kAirplayName[MAX_AIRPLAY_NAME_SIZE] = {0};
char kAirplayGroupUUID[MAX_AIRPLAY_GROUP_UUID_SIZE] = {0};
char kAirplayDeviceID[MAX_AIRPLAY_DEVICEID_SIZE] = {0};

AirplayPrivControlContext gAirplayPrivCtrlCtx;

int g_AirplayWaitingForWacConfiguredMsg = 0;
static int g_AirplayIsReadyToGo = 0;
unsigned int kAirplayIsLegacyMode = 0;
static airplay_notify kAirplayTrackInfo = {{0}, 0, 0, {0}, {0}, {0}, {0}};

#ifdef A98_BELKIN_MODULE
wm_circle_buf_ptr gAirPlayCircluBuffer = NULL;
#endif

ulog_define(AtApp_Linkplay, kLogLevelNotice, kLogFlags_Default, "Linkplay", NULL);
#define at_app_ucat() &log_category_from_name(AtApp_Linkplay)
#define at_app_ulog(LEVEL, ...) ulog(at_app_ucat(), (LEVEL), __VA_ARGS__)
#define at_app_dlog(LEVEL, ...) dlogc(at_app_ucat(), (LEVEL), __VA_ARGS__)

#define MAX_ART_WORK_DATA_SIZE 512*1024
char gMetaArtworkData[MAX_ART_WORK_DATA_SIZE];

#define AIRPLAY_TRACK_META_TXT_CHANGED(NEW_META_TXT) \
    ((0 != strcmp(NEW_META_TXT.title, kAirplayTrackInfo.title)) \
    || (0 != strcmp(NEW_META_TXT.artist, kAirplayTrackInfo.artist)) \
    || (0 != strcmp(NEW_META_TXT.album, kAirplayTrackInfo.album)))


#define AIRPLAY_TRACK_PROGRESS_CHANGED(NEW_META_PROGRESS) \
    ((NEW_META_PROGRESS.lduration != kAirplayTrackInfo.lduration) \
    || (NEW_META_PROGRESS.lposition != kAirplayTrackInfo.lposition))


#define AIRPLAY_TRACK_ARTWORK_CHANGED(NEW_ARTWORK_DATA) \
    (0 != strcmp(NEW_ARTWORK_DATA.album_uri, kAirplayTrackInfo.album_uri))

OSStatus AirPlaySaveFileContent(const char *file_path, char *in_buf, size_t in_buf_size);
void _airplayd_signal_handler(int sig);
static OSStatus platform_linkplay_update_airplay_track_info(airplay_track_info_update_type_e type, const airplay_notify* p_in_data);

static int platform_setup_airplay_device_id(void)
{        
    char tmp[MAX_AIRPLAY_DEVICEID_SIZE] = {0};

    //get softap interface mac addr as DeviceID to ensure that we use the same ID in WAC mode 
    memset(kAirplayDeviceID, 0, sizeof(kAirplayDeviceID));
    
    at_app_ulog(kLogLevelNotice, "Setup Airplay deviceID.\n");

    wm_db_get(NVRAM_CFG_AIRPLAY_DEVID, tmp);
    
    if (strlen(tmp) > 0)
    {
        strncpy(kAirplayDeviceID, tmp, sizeof(kAirplayDeviceID) - 1);
        return 0;
    }
    
wait:    
    
    //If file is not exist, then we keep wait
    if (0 != access(WAC_INTERFACE_ADDRESS_FILE, R_OK))
    {
        usleep(500000);
        goto wait;
    }

    FILE *fp = fopen(WAC_INTERFACE_ADDRESS_FILE, "r");
    if (fp)
    {
        fread(tmp, MAX_AIRPLAY_DEVICEID_SIZE - 1, 1, fp);

        fclose(fp);
        fp = NULL;
        
        char *p = tmp;
        while (*p != '\0')
        {
            if (*p == '\n')
            {
                *p = '\0';
                break;
            }
            
            *p = toupper_safe(*p);
            ++p;
        }

        at_app_ulog(kLogLevelNotice, "WAC interface address [%s] .\n", tmp);
        
        strncpy(kAirplayDeviceID, tmp, sizeof(kAirplayDeviceID) - 1);

        wm_db_set(NVRAM_CFG_AIRPLAY_DEVID, kAirplayDeviceID);
    }
    else
    {
        at_app_ulog(kLogLevelNotice, "Get deviceID for Airplay failed !\n");
        return -1;
    }

    return 0;
}


//===========================================================================================================================
//  AirPlayReceiverServerGetRef
//===========================================================================================================================
AirPlayReceiverServerRef AirPlayReceiverServerGetRef(void)
{
    return (gAirPlayServer);
}

static int _airplayd_init_global_vars(void)
{
    int64_t extra_delay = 0;
    char val_string[64]={0};

    memset(&gAirplayPrivCtrlCtx, 0, sizeof(gAirplayPrivCtrlCtx));
    
    g_wiimu_shm = WiimuContextGet();

    if (g_wiimu_shm == NULL)
    {
        at_app_ulog(kLogLevelNotice, "##Airplay 2 ===> Fatal error, shm init failed!!\n");
        return -1;
    }

    if (0 != platform_setup_airplay_device_id())
    {
        at_app_ulog(kLogLevelNotice, "get deviceID for Airplay failed !\n");
        return -1;
    }

    memset(val_string, 0, sizeof(val_string));
    wm_db_get((char *)MFI_AIRPLAY_EXTRA_DELAY, val_string);

    if (strlen(val_string)) {
        extra_delay = atoll(val_string);
    }

    if(gAirplayPrivCtrlCtx.ExtraAudioDelayMs != extra_delay)
    {
        gAirplayPrivCtrlCtx.ExtraAudioDelayMs = extra_delay;
    }

    return 0;
}

static int _airplayd_get_os_version(char *outOSversion, int buf_size)
{
    char cmd[128], buff[256];
    FILE *fp = NULL;

    memset((void *)&cmd, 0, sizeof(cmd));
    sprintf(cmd, "cat /proc/version");

    if (NULL != (fp = popen(cmd, "r")))
    {
        memset((void *)&buff, 0, sizeof(buff));
        fgets(buff, sizeof(buff) - 1, fp);
        pclose(fp);

        char *exclude = strchr(buff, '(');
        if (exclude){
            *exclude = '\0';
        }

        char os_type[16] = {0}, tmp[16] = {0}, kernel_ver[16] = {0};
        sscanf(buff, "%s %s %s", os_type, tmp, kernel_ver);

        at_app_ulog(kLogLevelNotice, "%s, buf=[%s]\n", __func__, buff);
        
        if (strlen(kernel_ver))
        {
            snprintf(outOSversion, buf_size - 1, OS_VESION_VALUE_FORMAT, kernel_ver);
            at_app_ulog(kLogLevelNotice, "%s, OSversion=[%s]\n", __func__, outOSversion);
            return 0;
        }
    }

    return -1;
}


static double platform_linkplay_dynamic_vol_percent_to_db(int in_vol_percent)
{
    double db = 0.0;

    if (in_vol_percent <= 0) 
    {
        db =  -144.0f;
    }
    else if (in_vol_percent >= 100)
    {
        db = 0.0f;
    }
    else
    {
        db = (in_vol_percent - 0.5f) * 30 / 100 - 30.0f;
    }

    return db;

}


double platform_linkplay_volume_percent_to_db(void)
{
    double db = -21.038965;//default 30%
    int volume_percent = g_wiimu_shm->volume;

    at_app_ulog(kLogLevelNotice, "audio volume percent [%d]\n", volume_percent);

    db = platform_linkplay_dynamic_vol_percent_to_db(volume_percent);
    
    return db;
}


CFTypeRef
platform_linkplay_handle_session_copy_property(AirPlayReceiverSessionRef inSession,
        CFStringRef inProperty,
        CFTypeRef inQualifier,
        OSStatus *outErr,
        void *inContext)
{
    CFTypeRef value = NULL;
    OSStatus err;

    (void)inSession;
    (void)inQualifier;
    (void)inContext;

    if (CFStringCompare(inProperty, CFSTR(kAirPlayKey__RTPSkewPlatformAdjust), 0) == kCFCompareEqualTo) {
        int d = 0;
        value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, (const void*)&d);
        err = kNoErr;
    }
    else if (CFStringCompare(inProperty, CFSTR(kAirPlayProperty_BufferedAudioSize), 0) == kCFCompareEqualTo)
    {
        int buffSize = MAX_AUDIO_BUFFER_SIZE;
        value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, (const void*)&buffSize);
        err = kNoErr;
    }
    else {
        at_app_ulog(kLogLevelNotice, "Unsupported session copy property request: %@\n", inProperty);
        err = kNotHandledErr;
    }

    if (outErr)
        *outErr = err;

    return (value);
}


CFTypeRef platform_linkplay_handle_server_copy_property(AirPlayReceiverServerRef inServer,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    OSStatus* outErr,
    void* inContext)
{
    (void)inServer;
    (void)inQualifier;
    (void)inContext;

    CFTypeRef value = NULL;
    OSStatus err = kNoErr;

    if (0) {
    }
    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_Password))) {
        value = CFStringCreateWithCString( kCFAllocatorDefault, (const char*)GetGlobalAirplayPassword(),  kCFStringEncodingUTF8);
    }
    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_SerialNumber))) {
        char serialNum[64];
        
        memset(serialNum, 0, sizeof(serialNum));
        snprintf(serialNum, sizeof(serialNum) - 1,"%s", g_wiimu_shm->uuid);

   	    value = CFStringCreateWithCString( kCFAllocatorDefault, (const char*)(serialNum),  kCFStringEncodingUTF8);
    }
#if 0
    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_IPV4SupportOnly)))
    {
        value = CFStringCreateWithCString( kCFAllocatorDefault, (const char*)("0"),  kCFStringEncodingUTF8);/* IPv4 & IPv6 both */
    }
#endif
    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_DeviceID)))
    {
        char devId[64];       

        memset(devId, 0, sizeof(devId));

        if (strlen(kAirplayDeviceID))
        {
            snprintf(devId, sizeof(devId) - 1, "%s", kAirplayDeviceID);
            at_app_ulog(kLogLevelNotice, "Airplay deviceID [%s].\n", devId);
        }
        else
        {
            snprintf(devId, sizeof(devId) - 1, "%02X:%02X:%02X:%02X:%02X:%02X", g_wiimu_shm->mac[0],
                                    g_wiimu_shm->mac[1],g_wiimu_shm->mac[2],g_wiimu_shm->mac[3],g_wiimu_shm->mac[4],g_wiimu_shm->mac[5]);//for test         

            at_app_ulog(kLogLevelNotice, "Warning: Use MAC address  [%s] as DeviceID.\n", devId);
        }
        
        value = CFStringCreateWithCString( kCFAllocatorDefault, (const char*)(devId),  kCFStringEncodingUTF8);
    }
    else if (CFEqual(inProperty, CFSTR(kAirPlayKey_MediaControlPort)))
    {
        char media_port[16];
        memset(media_port, 0, sizeof(media_port));
        snprintf(media_port, sizeof(media_port) - 1, "%d", kAirPlayFixedPort_MediaControl);
        value = CFStringCreateWithCString( kCFAllocatorDefault, (const char*)(media_port),  kCFStringEncodingUTF8);
    }
    else if (CFEqual(inProperty, CFSTR(kAirPlayKey_OSInfo)))
    {
        char os_version[64];
        memset(os_version, 0, sizeof(os_version));
        
        if (0 == _airplayd_get_os_version(os_version, sizeof(os_version)))
        {
            value = CFStringCreateWithCString( kCFAllocatorDefault, (const char*)(os_version),  kCFStringEncodingUTF8);
        }
        else
        {
            value = NULL;
        }
    }
    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_HomeIntegrationData)))
    {
        at_app_ulog(kLogLevelNotice, "copy property %@... \n", inProperty);
        
        FILE *fp = NULL;
        struct stat file_st;
        
        memset(&file_st, 0, sizeof(struct stat));
        if (0 != stat(MFI_AIRPLAY_HM_INTEGRATION_DATA, &file_st))
        {
            err = kUnexpectedErr;
        }
        else
        {
            unsigned char *tmpVal = NULL;
        
            tmpVal = (unsigned char*)calloc(file_st.st_size + 1, 1);
            
            if (NULL == tmpVal)
            {
                err = kNoMemoryErr;
            }
            else
            {
                if (NULL != (fp = fopen(MFI_AIRPLAY_HM_INTEGRATION_DATA, "rb")))
                {
                    fread(tmpVal, file_st.st_size, 1, fp);
                    fclose(fp);
                    value = CFDataCreate( kCFAllocatorDefault, (const uint8_t*)tmpVal, file_st.st_size);
                }
                
                free(tmpVal);
            }
        }
    }
    else {
        at_app_ulog(kLogLevelNotice, "<- Unhandled server copy property %@... \n", inProperty);
        err = kNotHandledErr;
    }

    if (outErr) {
        *outErr = err;
    }

    return (value);
}

OSStatus platform_linkplay_set_airplay_HM_integrationData(CFTypeRef inValue)
{
    OSStatus status = kNoErr;
    unsigned char *tmpVal = NULL;
    
    tmpVal = (unsigned char*)calloc(MAX_HOME_INGEGRATION_SETTING_SIZE, 1);
    if (!tmpVal)
    {
        status = kNoMemoryErr;
    }
    else 
    {
        size_t outLen = 0;
        OSStatus outErr = kNoErr;
        
        CFGetData(inValue, tmpVal, MAX_HOME_INGEGRATION_SETTING_SIZE, &outLen, &outErr);

        at_app_ulog(kLogLevelNotice, "set HM data, outLen=[%d], err=[%d]\n", outLen, outErr);

        if (outErr != kNoErr || outLen <= 0) 
        {
            status = outErr;
        }
        else
        {
            FILE *fp = NULL;
            if (NULL == (fp = fopen(MFI_AIRPLAY_HM_INTEGRATION_DATA, "wb+")))
            {
                status = kNoResourcesErr;
            }
            else
            {
                fwrite(tmpVal, outLen, 1, fp);
                fflush(fp);
                sync();
                fclose(fp);
                status = kNoErr;
            }
        }

        free(tmpVal);
    }

    return status;
}

OSStatus AirPlaySaveFileContent(const char *file_path, char *in_buf, size_t in_buf_size)
{
    OSStatus err = kNoErr;

    if (in_buf == NULL || file_path == NULL)
        return kValueErr;

    FILE *fp = fopen(file_path, "w+");//overwrite

    if (fp == NULL)
    {
        at_app_ulog(kLogLevelNotice, "%s, sys err\n", __func__);
        return err;
    }

    fwrite(in_buf, in_buf_size, 1, fp);
    fflush(fp);
    sync();
    fclose(fp);

    return err;
}


OSStatus platform_linkplay_set_group_uuid(CFTypeRef inValue)
{
    char tmpVal[512] ={0};
    memset(tmpVal, 0, sizeof(tmpVal));
    CFGetCString(inValue,tmpVal, sizeof(tmpVal));
    
    at_app_ulog(kLogLevelNotice, "update airplay group uuid [%s]\n", tmpVal);

    if (strcmp(kAirplayGroupUUID, tmpVal))
    {       
        memset(kAirplayGroupUUID, 0, MAX_AIRPLAY_GROUP_UUID_SIZE);
        strncpy(kAirplayGroupUUID, tmpVal, MAX_AIRPLAY_GROUP_UUID_SIZE - 1);
        
        AirPlaySaveFileContent(MFI_AIRPLAY_GROUP_UUID, kAirplayGroupUUID, strlen(kAirplayGroupUUID));
    }
        
    return kNoErr;
}



OSStatus platform_linkplay_set_airplay_pswd(CFTypeRef inValue)
{
    char tmpVal[256] = {0};
    memset(tmpVal, 0, sizeof(tmpVal));
    CFGetCString(inValue, tmpVal, sizeof(tmpVal));

    at_app_ulog(kLogLevelNotice, "update airplay passwd [%s] into [%s]\n", kAirplayPassword, tmpVal);

    if (strcmp(kAirplayPassword, tmpVal))
    {
        wm_db_set("AIRPLAY_PASSWORD", tmpVal);

        memset(kAirplayPassword, 0, MAX_AIRPLAY_PSWD_SIZE);
        strncpy(kAirplayPassword, tmpVal, MAX_AIRPLAY_PSWD_SIZE - 1);

        AirPlaySaveFileContent(MFI_AIRPLAY_PSWD_CONF, kAirplayPassword, strlen(kAirplayPassword));
    }

    return kNoErr;
}

OSStatus platform_linkplay_set_airplay_name(CFTypeRef inValue)
{
    char tmpVal[256] = {0};
    char buf[256] = {'\0'};

    memset(tmpVal, 0, sizeof(tmpVal));
    CFGetCString(inValue, tmpVal, sizeof(tmpVal));

    at_app_ulog(kLogLevelNotice, "update airplay name [%s] into [%s]\n", kAirplayName, tmpVal);

    if (strcmp(kAirplayName, tmpVal))
    {
        memset(kAirplayName, 0, MAX_AIRPLAY_NAME_SIZE);
        strncpy(kAirplayName, tmpVal, MAX_AIRPLAY_NAME_SIZE - 1);

        AirPlaySaveFileContent(MFI_AIRPLAY_NAME_CONF, kAirplayName, strlen(kAirplayName));
        snprintf(buf, sizeof(buf) - 1, "AirplaySetDeviceName:%s", tmpVal);
        SocketClientReadWriteMsg("/tmp/RequestGoheadCmd", buf, strlen(buf), NULL, 0, 0);
    }

    return kNoErr;
}


//===========================================================================================================================
//  AirPlayGetFileLineContent
//===========================================================================================================================
OSStatus AirPlayGetFileLineContent(const char *file_path, char *outBuf, size_t out_buf_size)
{
    char tmpBuff[256] = {0};
    OSStatus err = kPathErr;

    if (outBuf == NULL || file_path == NULL || out_buf_size <= 0)
        return kReadErr;

    if (access(file_path, F_OK) == 0)
    {
        FILE *fp = fopen(file_path, "r+");

        if (fp == NULL)
        {
            at_app_ulog(kLogLevelNotice, "%s, sys err\n", __func__);
            return err;
        }

        memset((void *)tmpBuff, 0, sizeof(tmpBuff));
        fgets(tmpBuff, sizeof tmpBuff, fp);
        fclose(fp);

        if (tmpBuff[strlen(tmpBuff) - 1] == '\n')
            tmpBuff[strlen(tmpBuff) - 1] = '\0';

        memset((void *)outBuf, 0, out_buf_size);
        strncpy(outBuf, tmpBuff, out_buf_size - 1);
    }

    return err;
}


//===========================================================================================================================
//  AirplayReciverMaybeUpdateDevName
//===========================================================================================================================
static OSStatus AirplayReciverMaybeUpdateDevName(AirPlayReceiverServerRef inServer)
{
    OSStatus err = kNotHandledErr;
    char airplayDevName[256] = { 0 }, newDevName[256] = {0};

    AirPlayGetFileLineContent(MFI_AIRPLAY_NAME_CONF, newDevName, sizeof(newDevName));
    AirPlayReceiverServerPlatformGetCString(inServer, CFSTR(kAirPlayKey_Name), NULL, airplayDevName, sizeof(airplayDevName), NULL);
    
    if ((0 == strlen(airplayDevName) && strlen(newDevName))
            || (strlen(airplayDevName) && 0 != strcmp(airplayDevName, newDevName)))
    {
        err = AirPlayReceiverServerPlatformSetCString(inServer, CFSTR(kAirPlayKey_Name), NULL, newDevName, strlen(newDevName));
        at_app_ulog(kLogLevelNotice, "[%s-%d], newDevName=[%s], err=[%d]\n", __func__, __LINE__, newDevName, err);
    }

    return err;
}


//===========================================================================================================================
//  AirplayReciverMaybeUpdateDevPswd
//===========================================================================================================================
static OSStatus AirplayReciverMaybeUpdateDevPswd(AirPlayReceiverServerRef inServer)
{
    OSStatus err = kNotHandledErr;
    char playPassword[256] = { 0 }, new_passwd[256] = {0};

    AirPlayGetFileLineContent(MFI_AIRPLAY_PSWD_CONF, new_passwd, sizeof(new_passwd));
    AirPlayReceiverServerPlatformGetCString(inServer, CFSTR(kAirPlayProperty_Password), NULL, playPassword, sizeof(playPassword), NULL);
    
    if ((0 == strlen(playPassword) && strlen(new_passwd))
            || (strlen(playPassword) && 0 != strcmp(playPassword, new_passwd)))
    {
        err = AirPlayReceiverServerPlatformSetCString(inServer, CFSTR(kAirPlayProperty_Password), NULL, playPassword, strlen(playPassword));
        at_app_ulog(kLogLevelNotice, "[%s-%d], new_passwd=[%s], err=[%d]\n", __func__, __LINE__, new_passwd, err);
    }

    return err;
}


char *GetGlobalAirplayDevName(void)
{
    if (0 == strlen(kAirplayName))  //not inited
    {
        char new_name[256] = {0};
        AirPlayGetFileLineContent(MFI_AIRPLAY_NAME_CONF, new_name, sizeof(new_name));
        if (strlen(new_name))
        {
            memset(kAirplayName, 0, sizeof(kAirplayName));
            strncpy(kAirplayName, new_name, sizeof(kAirplayName) - 1);
        }
    }

    return (char *)&kAirplayName;
}


char *GetGlobalAirplayPassword(void)
{
    if (0 == strlen(kAirplayPassword))  //not inited
    {
        char new_passwd[256] = {0};
        AirPlayGetFileLineContent(MFI_AIRPLAY_PSWD_CONF, new_passwd, sizeof(new_passwd));
        if (strlen(new_passwd))
        {
            memset(kAirplayPassword, 0, sizeof(kAirplayPassword));
            strncpy(kAirplayPassword, new_passwd, sizeof(kAirplayPassword) - 1);
        }
    }

    return (char *)&kAirplayPassword;
}


//===========================================================================================================================
//  AirPlayReciverHandleIncomingControl
//===========================================================================================================================
OSStatus AirPlayReciverHandleIncomingControl(
    AirPlayReceiverServerRef inServer,
    CFStringRef inCommand,
    CFTypeRef inQualifier,
    CFDictionaryRef inParams,
    CFDictionaryRef *outParams,
    void *inContext)
{
    OSStatus err = kNotHandledErr;

    (void)(inServer);
    (void)(inQualifier);
    (void)(inQualifier);
    (void)(inContext);
    (void)(inParams);
    (void)(outParams);

    if (0) {
    }
    else if (CFEqual(inCommand, CFSTR(kAirPlayProperty_Password)))
    {
        err = AirplayReciverMaybeUpdateDevPswd(inServer);
    }
    else if (CFEqual(inCommand, CFSTR(kAirPlayKey_Name)))
    {
        err = AirplayReciverMaybeUpdateDevName(inServer);
    }
    else if (CFEqual(inCommand, CFSTR(kAirPlayCommand_UpdateConfigs)))
    {
        OSStatus err1 = AirplayReciverMaybeUpdateDevName(inServer);
        OSStatus err2 = AirplayReciverMaybeUpdateDevPswd(inServer);

        if (err1 == kNoErr || err2 == kNoErr)
        {
            err = kNoErr;
        }
    }

    return err;
}


static OSStatus platform_linkplay_update_airplay_track_info(airplay_track_info_update_type_e type, const airplay_notify* p_in_data)
{
    OSStatus err = kNoErr;
    char notify_meta_Msg[] = "AIRPLAY_MEDADATA";
    char notify_pos_Msg[] = "AIRPLAY_POSITION";
    
    if(NULL == p_in_data)
        return err;

    switch (type)
    {
        case airplay_track_info_update_meta_txt:
        {
            strncpy(kAirplayTrackInfo.notify, notify_meta_Msg, sizeof(kAirplayTrackInfo.notify) - 1);
        
            memset(kAirplayTrackInfo.title, 0, sizeof(kAirplayTrackInfo.title));
            strncpy(kAirplayTrackInfo.title, p_in_data->title, sizeof(kAirplayTrackInfo.title) - 1);

            memset(kAirplayTrackInfo.artist, 0, sizeof(kAirplayTrackInfo.artist));
            strncpy(kAirplayTrackInfo.artist, p_in_data->artist, sizeof(kAirplayTrackInfo.artist) - 1);

            memset(kAirplayTrackInfo.album, 0, sizeof(kAirplayTrackInfo.album));
            strncpy(kAirplayTrackInfo.album, p_in_data->album, sizeof(kAirplayTrackInfo.album) - 1);
        }
        break;

        case airplay_track_info_update_progress:
        {
            strncpy(kAirplayTrackInfo.notify, notify_pos_Msg, sizeof(kAirplayTrackInfo.notify) - 1);

            kAirplayTrackInfo.lduration = p_in_data->lduration;
            kAirplayTrackInfo.lposition = p_in_data->lposition;
        }
        break;

        case airplay_track_info_update_artwork_data:
        {
            strncpy(kAirplayTrackInfo.notify, notify_meta_Msg, sizeof(kAirplayTrackInfo.notify) - 1);

            memset(kAirplayTrackInfo.album_uri, 0, sizeof(kAirplayTrackInfo.album_uri));
            strncpy(kAirplayTrackInfo.album_uri, p_in_data->album_uri, sizeof(kAirplayTrackInfo.album_uri) - 1);
        }
        break;

        default:
            break;
    }

    return err;
}


static OSStatus AirplayReciverGetSupportedMimeTypeTag(char *mimeTypeString, char *outTag, int outTagSize)
{
    if(mimeTypeString == NULL || outTag == NULL || outTagSize <= 0)
    {
        return kNoErr;
    }
    char *pStr=mimeTypeString;
    for (; *pStr!='\0'; pStr++)
        *pStr = tolower(*pStr);

    memset(outTag, 0, outTagSize);

    if (strstr(mimeTypeString, "none"))
    {
        strncpy(outTag, "none", outTagSize - 1);
    }
    else if (strstr(mimeTypeString, "jpeg2000"))
    {
        strncpy(outTag, "jpeg2000", outTagSize - 1);
    }
    else if (strstr(mimeTypeString, "jpg") || strstr(mimeTypeString, "jpeg"))
    {
        strncpy(outTag, "jpeg", outTagSize - 1);
    }
    else if (strstr(mimeTypeString, "gif"))
    {
        strncpy(outTag, "gif", outTagSize - 1);
    }
    else if (strstr(mimeTypeString, "bmp"))
    {
        strncpy(outTag, "bmp", outTagSize - 1);
    }
    else if (strstr(mimeTypeString, "png"))
    {
        strncpy(outTag, "png", outTagSize - 1);
    }
    else if (strstr(mimeTypeString, "tiff"))
    {
        strncpy(outTag, "tiff", outTagSize - 1);
    }
    else if (strstr(mimeTypeString, "pict"))
    {
        strncpy(outTag, "pict", outTagSize - 1);
    }
    else
    {
        return kUnsupportedErr;
    }
    
    return kNoErr;
}

static OSStatus AirplayRecieverUpdateMetaArtworkData(CFTypeRef inValue)
{
    OSStatus err1 = kNoErr, err2 = kNoErr;
    char mimeType[12] ={0};
    size_t realArtworkDataSize = 0;
    airplay_notify airplayMetaNotify;
    char mimeTag[16] = {0};
    char dataFilePath[128] = "/tmp/web/";
    char artworkDataFilePrefix[64] = "AirplayArtWorkData.";
    char cmd[128]={0};

    if(inValue == NULL)
    {
        return kOptionErr;
    }

    memset(&airplayMetaNotify, 0, sizeof(airplayMetaNotify));

    //Get meta mime type    
    CFDictionaryGetCString((CFDictionaryRef)inValue, kAirPlayMetaDataKey_ArtworkMIMEType, mimeType, sizeof(mimeType), &err1);

    if(strlen(mimeType) <= 0)
        return kOptionErr;

    at_app_ulog(kLogLevelNotice, "update metaArtwork mimeType to [%s] \n", mimeType);

    //is it supported or not ?
    if (kNoErr != AirplayReciverGetSupportedMimeTypeTag(mimeType, mimeTag, sizeof(mimeTag)))
    {
        at_app_ulog(kLogLevelNotice,"[%s:%d] unSupported mime type [%s]!!\n", __func__, __LINE__, mimeType);
        return kFormatErr;
    }

    //remove previous data from storage.
    memset(cmd, 0, sizeof(cmd));
    snprintf(cmd, sizeof(cmd), "rm -f %s%s*", dataFilePath,artworkDataFilePrefix);
    system(cmd);
   
    //if there is no mime for this track, clear the data
    if (strcasecmp(mimeTag, "none") == 0)
    {
        platform_linkplay_update_airplay_track_info(airplay_track_info_update_artwork_data, &airplayMetaNotify);
        return kNoErr;
    }

    //Get meta artwork data 
    memset(gMetaArtworkData, 0, MAX_ART_WORK_DATA_SIZE);
    CFDictionaryGetData((CFDictionaryRef)inValue, kAirPlayMetaDataKey_ArtworkData, gMetaArtworkData, MAX_ART_WORK_DATA_SIZE, &realArtworkDataSize, &err2);
    
    if (realArtworkDataSize > MAX_ART_WORK_DATA_SIZE || (realArtworkDataSize <= 0))
    {
        at_app_ulog(kLogLevelNotice,"[%s:%d] metaArtworkData size error [%d], max [%d] !\n", __func__, __LINE__, realArtworkDataSize, MAX_ART_WORK_DATA_SIZE);
        return kOptionErr;
    }
    
    if (err1 == kNoErr && err2 == kNoErr)
    {
        //gererate the name.
        strncat(dataFilePath, artworkDataFilePrefix, sizeof(dataFilePath) - strlen(dataFilePath) - 1);
        strncat(dataFilePath, mimeTag, sizeof(dataFilePath) - strlen(dataFilePath) - 1);        

        FILE *fp = fopen(dataFilePath, "wb+");
        if (fp)
        {
            fwrite(gMetaArtworkData, realArtworkDataSize, 1, fp);
            fflush(fp);
            fclose(fp);
            
            struct in_addr ipAddr;
            char devIpaddrString[32]={0};
            
            int retValue = GetIPaddress(WAN_INTERFACE_NAME);
            
            if (-1 == retValue || 0 == retValue)
            {
                at_app_ulog(kLogLevelNotice,"get Dev [%s] IPaddr fail, ret [%d]!!", WAN_INTERFACE_NAME, retValue);
                return kValueErr;
            }
            else
            {
                ipAddr.s_addr = retValue;
            
                memset(devIpaddrString, 0, sizeof(devIpaddrString));
                strncpy(devIpaddrString, inet_ntoa(ipAddr), sizeof(devIpaddrString) - 1);
            }

            memset(airplayMetaNotify.album_uri, 0, sizeof(airplayMetaNotify.album_uri));
            snprintf(airplayMetaNotify.album_uri, sizeof(airplayMetaNotify.album_uri), "https://%s/data/%s%s", devIpaddrString, artworkDataFilePrefix, mimeTag);

            platform_linkplay_update_airplay_track_info(airplay_track_info_update_artwork_data, &airplayMetaNotify);

            at_app_ulog(kLogLevelNotice, "album_uri [%s]\n", airplayMetaNotify.album_uri);
        }
    } else {
        return err1;
    }
    
    return kNoErr;
}


//===========================================================================================================================
//  AirPlayReciverHandleIncomingControl
//===========================================================================================================================
OSStatus AirPlayReceiverSessionSetPropertyHandler(
    AirPlayReceiverSessionRef inSession,
    CFStringRef inProperty,
    CFTypeRef inQualifier,
    CFTypeRef inValue,
    void *inContext)
{
    OSStatus err = kNoErr;

    (void)(inSession);
    (void)(inValue);
    (void)(inQualifier);
    (void)(inContext);
    (void)(inProperty);
    
    if (0) {
    }
    else if (CFEqual(inProperty, CFSTR(kAirPlayClientProperty_Playing)))
    {
        static int64_t previous_playing_state = 0;

        int64_t playing = 0;

        CFNumberGetValue((CFNumberRef)inValue, kCFNumberSInt64Type, &playing);

        if (playing != previous_playing_state)
        {
            at_app_ulog(kLogLevelNotice, "Airplay streaming is now [%s]. \n", playing ? "started" : "stopped");
            previous_playing_state = playing;
            
            if (playing)
            {                
                gAirplayPrivCtrlCtx.RequestSilence = 1;

                gAirplayPrivCtrlCtx.RequestFadeIn = 1;
                gAirplayPrivCtrlCtx.RequestFadeOut = 0;
                gAirplayPrivCtrlCtx.RequiredFadeVolume = 0;
                gAirplayPrivCtrlCtx.isPlaying = true;

                //Play-Pause
                gAirplayPrivCtrlCtx.TsSinceAudioStarted = UpTicks();
                SocketClientReadWriteMsg("/tmp/Requesta01controller", "AIRPLAY_PLAY", strlen("AIRPLAY_PLAY"), NULL, NULL, 0);
            }
            else
            {
                gAirplayPrivCtrlCtx.RequestFadeIn = 0;
                gAirplayPrivCtrlCtx.RequestFadeOut = 1;
                gAirplayPrivCtrlCtx.RequiredFadeVolume = AIRPLAY_FADE_MAX_VOLUME;
                gAirplayPrivCtrlCtx.isPlaying = false;

                if(true == gAirplayPrivCtrlCtx.AudioOutputPlaybackThreadExited)//airplay is really stopped as the thread is terminated.
                {
                    at_app_ulog(kLogLevelNotice, "Airplay streaming now is really Stopped. \n");
                    SocketClientReadWriteMsg("/tmp/Requesta01controller", "AIRPLAY_STOP", strlen("AIRPLAY_STOP"), NULL, NULL, 0);
                }
                else
                {
                    at_app_ulog(kLogLevelNotice, "Airplay streaming now is paused. \n");
                    SocketClientReadWriteMsg("/tmp/Requesta01controller", "AIRPLAY_PAUSE", strlen("AIRPLAY_PAUSE"), NULL, NULL, 0);
                }
            }
        }
    }
    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_Progress)))
    {
        static TrackTimingMetaData trackTime = {0, 0};
        static uint32_t curTmstmp = 0;

        TrackTimingMetaData __trackTime = {0, 0};
        uint32_t __curTmstmp = 0;

        __trackTime.startTS = CFDictionaryGetInt64((CFDictionaryRef)inValue, CFSTR(kAirPlayKey_StartTimestamp), &err);
        __curTmstmp = CFDictionaryGetInt64((CFDictionaryRef)inValue, CFSTR(kAirPlayKey_CurrentTimestamp), &err);
        __trackTime.stopTS = CFDictionaryGetInt64((CFDictionaryRef)inValue, CFSTR(kAirPlayKey_EndTimestamp), &err);

        if (__curTmstmp != curTmstmp || 0 != memcmp(&trackTime, &__trackTime, sizeof(TrackTimingMetaData)))
        {
            curTmstmp = __curTmstmp;
            memcpy(&trackTime, &__trackTime, sizeof(TrackTimingMetaData));

            airplay_notify airplay_progress_stat;

            int sampleRate = 44100;

            memset(&airplay_progress_stat, 0, sizeof(airplay_notify));
            airplay_progress_stat.lduration = ((__trackTime.stopTS - __trackTime.startTS) / sampleRate) * 1000;/* seconds */
            airplay_progress_stat.lposition = ((curTmstmp - __trackTime.startTS) / sampleRate) * 1000;/* seconds */

            if (AIRPLAY_TRACK_PROGRESS_CHANGED(airplay_progress_stat))
            {
                platform_linkplay_update_airplay_track_info(airplay_track_info_update_progress, &airplay_progress_stat);

                if (airplay_progress_stat.lduration <= airplay_progress_stat.lposition)
                {
                    at_app_ulog(kLogLevelNotice, "we have been playing till end of this track !\n");

                    gAirplayPrivCtrlCtx.TrackChanged = 1;
                    
                    gAirplayPrivCtrlCtx.TsSinceAudioStarted = 0;
                    gAirplayPrivCtrlCtx.RequestSkipSkew = 1;
                }
            
                SocketClientReadWriteMsg("/tmp/Requesta01controller", (char*)&kAirplayTrackInfo,sizeof(airplay_notify),NULL,NULL,0);
            }
        }
    }
    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_MetaData)))
    {
        airplay_notify new_metaData;
        OSStatus err1 = kNoErr, err2 = kNoErr, err3 = kNoErr, err4 = kNoErr;

        memset(&new_metaData, 0, sizeof(new_metaData));

        CFDictionaryGetCString((CFDictionaryRef)inValue, kAirPlayMetaDataKey_Title, new_metaData.title, sizeof(new_metaData.title), &err1);
        CFDictionaryGetCString((CFDictionaryRef)inValue, kAirPlayMetaDataKey_Artist, new_metaData.artist, sizeof(new_metaData.artist), &err2);
        CFDictionaryGetCString((CFDictionaryRef)inValue, kAirPlayMetaDataKey_Album, new_metaData.album, sizeof(new_metaData.album), &err3);

        err4 = AirplayRecieverUpdateMetaArtworkData(inValue);
        at_app_ulog(kLogLevelVerbose, "err1 err2 err3 err4:%d %d %d %d \n", err1, err2, err3, err4);

        if ( (kNoErr == err1 && kNoErr == err2 && kNoErr == err3) && AIRPLAY_TRACK_META_TXT_CHANGED(new_metaData))
        {
            at_app_ulog(kLogLevelNotice, "Title [%s]\n", new_metaData.title);
            at_app_ulog(kLogLevelNotice, "Artist [%s]\n", new_metaData.artist);
            at_app_ulog(kLogLevelNotice, "Album [%s]\n", new_metaData.album);

            gAirplayPrivCtrlCtx.TrackChanged = 1;
            gAirplayPrivCtrlCtx.TsSinceAudioStarted = 0;
            gAirplayPrivCtrlCtx.RequestSkipSkew = 1;

            //we will notify meta data only when it changes.
            platform_linkplay_update_airplay_track_info(airplay_track_info_update_meta_txt, &new_metaData);

            SocketClientReadWriteMsg("/tmp/Requesta01controller", (char *)&kAirplayTrackInfo, sizeof(airplay_notify), NULL, NULL, 0);
        }
        // else if ((kNoErr == err4) && AIRPLAY_TRACK_ARTWORK_CHANGED(new_metaData)) {
        else if ((kNoErr == err4)) {
            at_app_ulog(kLogLevelNotice, "album_uri [%s]\n", new_metaData.album_uri);

            gAirplayPrivCtrlCtx.TrackChanged = 1;
            gAirplayPrivCtrlCtx.TsSinceAudioStarted = 0;
            gAirplayPrivCtrlCtx.RequestSkipSkew = 1;

            SocketClientReadWriteMsg("/tmp/Requesta01controller", (char *)&kAirplayTrackInfo, sizeof(airplay_notify), NULL, NULL, 0);
        }

    }
    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_AudioFormat)))
    {
        int64_t audio_format = 0;
        CFNumberGetValue((CFNumberRef)inValue, kCFNumberSInt64Type, &audio_format);
        at_app_ulog(kLogLevelNotice, "audio format [%d]\n", audio_format);
    }
    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_AirPlayMode)))
    {
        char airplay_mode[64] = {0};
        CFStringGetCString((CFStringRef)inValue, airplay_mode, sizeof(airplay_mode), kCFStringEncodingUTF8);
        at_app_ulog(kLogLevelNotice, "airplay mode [%s]\n", airplay_mode);
        
        if (0 == strcmp(airplay_mode, "Legacy AirPlay")){
            kAirplayIsLegacyMode = 1;
        } else {
            kAirplayIsLegacyMode = 0;
        }        
    }
    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_AirPlayClocking)))
    {
        char clocking_str[64] = {0};
        CFStringGetCString((CFStringRef)inValue, clocking_str, sizeof(clocking_str), kCFStringEncodingUTF8);
        at_app_ulog(kLogLevelNotice, "airplay clocking [%s]\n", clocking_str);
    }
    else if (CFEqual(inProperty, CFSTR(kAirPlayProperty_ClientIP)))
    {
        char address_string[64] = {0};
        CFStringGetCString((CFStringRef)inValue, address_string, sizeof(address_string), kCFStringEncodingUTF8);
        at_app_ulog(kLogLevelNotice, "sender address [%s]\n", address_string);
    }
    else
    {
        at_app_ulog(kLogLevelNotice, "--> unHandled session property '%@' \n", inProperty);
        err = kNotHandledErr;
    }

    return err;
}

static void *platform_linkplay_internal_msg_handler(void *arg)
{
    int recv_num;
    char recv_buf[4096];
    int ret;
    SOCKET_CONTEXT msg_socket;
    char cmd[128];
    (void)arg;

    msg_socket.path = "/tmp/RequestAirplay";
    msg_socket.blocking = 1;

RE_INIT:
    ret = UnixSocketServer_Init(&msg_socket);

    if (ret < 0)
    {
        printf("IO UnixSocketServer_Init fail\n");
        sleep(5);
        goto RE_INIT;
    }

    while (1)
    {
        ret = UnixSocketServer_accept(&msg_socket);
        if (ret <= 0)
        {
            printf("airplayd IO UnixSocketServer_accept fail\n");
            if (ret < 0)
            {
                UnixSocketServer_deInit(&msg_socket);
                sleep(5);
                goto RE_INIT;
            }
        }
        else
        {
            memset(recv_buf, 0x0, sizeof(recv_buf));
            recv_num = UnixSocketServer_read(&msg_socket, recv_buf, sizeof(recv_buf));
            if (recv_num <= 0)
            {
                printf("IO UnixSocketServer_recv failed\r\n");
                if (recv_num < 0)
                {
                    UnixSocketServer_close(&msg_socket);
                    UnixSocketServer_deInit(&msg_socket);
                    sleep(5);
                    goto RE_INIT;
                }
            }
            else
            {
                recv_buf[recv_num] = 0;

                at_app_ulog(kLogLevelNotice, "Airplay recvd internal msg [%s]\n", recv_buf);

                if (strncmp(recv_buf, WAC_MSG_WAIT_FOR_CONFIGURED_MSG, strlen(WAC_MSG_WAIT_FOR_CONFIGURED_MSG)) == 0)
                {
                    g_AirplayWaitingForWacConfiguredMsg = 1;
                }
                else if (strncmp(recv_buf, WAC_MSG_AIRPLAY_READY_TO_GO_MSG, strlen(WAC_MSG_AIRPLAY_READY_TO_GO_MSG)) == 0)
                {
                    g_AirplayIsReadyToGo = 1;
                }
                else if (strncmp(recv_buf, "IPChanged", strlen("IPChanged")) == 0)
                {
                #ifdef AMLOGIC_A113
                    res_init();
                #endif
                    //AirPlayStopMain();
                    //AirPlayStartMain();
                }
                else if (strncmp(recv_buf, "NameChanged", strlen("NameChanged")) == 0)
                {
                    memset((void *)cmd, 0, sizeof(cmd));
                    int fd = open("/tmp/airplay_name", O_RDWR | O_CREAT | O_TRUNC, 0666);
                    if (fd >= 0)
                    {
                        write(fd, recv_buf + strlen("NameChanged:"), strlen(recv_buf + strlen("NameChanged:")));
                        close(fd);
                    }

                    if (kNoErr == AirPlayReciverServerUpdateAirPlayDevName(AirPlayReceiverServerGetRef()))
                    {
                        AirPlayReceiverServerUpdateRegisteration(AirPlayReceiverServerGetRef());
                    }
                }
                else if (strncmp(recv_buf, "setAirplayPassword", strlen("setAirplayPassword")) == 0)
                {
                    memset((void *)cmd, 0, sizeof(cmd));
                    sprintf(cmd, "echo -n \"%s\" > /tmp/airplay_password", recv_buf + strlen("setAirplayPassword:"));
                    system(cmd);

                    printf("%s save airplay password[%s]\n", __FUNCTION__, recv_buf + strlen("setAirplayPassword:"));
                    memset((void *)&cmd, 0, sizeof(cmd));
                    sprintf(cmd, "nvram_set AIRPLAY_PASSWORD \"%s\"&", recv_buf + strlen("setAirplayPassword:"));
                    system(cmd);

                    if (kNoErr == AirPlayReciverServerUpdateAirPlayPassword(AirPlayReceiverServerGetRef()))
                    {
                        AirPlayReceiverServerUpdateRegisteration(AirPlayReceiverServerGetRef());
                    }
                }
                else if (strncmp(recv_buf, "stopAirplay", strlen("stopAirplay")) == 0)
                {
                    // notify clients to stop
                    SocketClientReadWriteMsg("/tmp/RequestAppleRemote", "AIRPLAY_SWITCH_OUT", strlen("AIRPLAY_SWITCH_OUT"), NULL, NULL, 0);
                    //AirTunesServer_StopAllConnections();
                    AirPlayReceiverServerStop(AirPlayReceiverServerGetRef());
                }
                else if (strncmp(recv_buf, "startAirplay", strlen("startAirplay")) == 0)
                {
                    AirPlayReceiverServerStart(AirPlayReceiverServerGetRef());
                }
                else if (strncmp(recv_buf, "updateConfig", strlen("updateConfig")) == 0)
                {
                    if (kNoErr == AirPlayReciverServerUpdateConfigs(AirPlayReceiverServerGetRef()))
                    {
                        AirPlayReceiverServerUpdateRegisteration(AirPlayReceiverServerGetRef());
                    }
                }
                else if (strncmp(recv_buf, "isNameChangeAllowed", strlen("isNameChangeAllowed")) == 0)
                {
                    Boolean isPartOfHome = HIIsAccessControlEnabled(AirPlayReceiverServerGetRef());

                    at_app_ulog(kLogLevelNotice, "device name change is %s !\n", (isPartOfHome ? "not Allowed" : "Allowed"));
 
                    if (isPartOfHome)
                    {
                        /* true means device has added into a Home, name change is not permitted. */
                        UnixSocketServer_write(&msg_socket, "false", strlen("false"));
                    }
                    else
                    {
                        UnixSocketServer_write(&msg_socket,"true", strlen("true"));
                    }
                }
                else if (strncmp(recv_buf, "SetAirplayExtraDelay", strlen("SetAirplayExtraDelay")) == 0)
                {
                    char *pData = recv_buf + strlen("SetAirplayExtraDelay") + 1;
                    int64_t extra_delay = 0;
                    extra_delay = atoll(pData);

                    at_app_ulog(kLogLevelNotice, "update airplay extra delay [%lld] to [%lld]\n", gAirplayPrivCtrlCtx.ExtraAudioDelayMs, extra_delay);

                    if(gAirplayPrivCtrlCtx.ExtraAudioDelayMs != extra_delay)
                    {
                        char val_string[64]={0};

                        gAirplayPrivCtrlCtx.ExtraAudioDelayMs = extra_delay;
                        gAirplayPrivCtrlCtx.RequestForceSkewAdjust = true;

                        snprintf(val_string, sizeof(val_string), "%lld", extra_delay);
                        wm_db_set((char *)MFI_AIRPLAY_EXTRA_DELAY, val_string);
                    }
                }
                else if (strncmp(recv_buf, "GetAirplayExtraDelay", strlen("GetAirplayExtraDelay")) == 0)
                {
                    char resp_msg[128];

                    memset(resp_msg, 0, sizeof(resp_msg));
                    snprintf(resp_msg, sizeof(resp_msg), MFI_AIRPLAY_EXTRA_DELAY_MSG,
                             gAirplayPrivCtrlCtx.ExtraAudioDelayMs);

                    at_app_ulog(kLogLevelNotice,"Airplay Extra Delay Msg [%s]\n", resp_msg);
                    UnixSocketServer_write(&msg_socket, resp_msg, strlen(resp_msg));
                }
            }
            
            UnixSocketServer_close(&msg_socket);
        }
    }

    return NULL;
}


static void *platform_linkplay_remote_control_handler(void *arg)
{
    int recv_num;
    char recv_buf[4096];
    int ret;
    SOCKET_CONTEXT msg_socket;
    (void)arg;

    msg_socket.path = "/tmp/RequestAppleRemote";
    msg_socket.blocking = 1;

RE_INIT:
    ret = UnixSocketServer_Init(&msg_socket);

    if (ret < 0)
    {
        printf("IO UnixSocketServer_Init fail\n");
        sleep(5);
        goto RE_INIT;
    }

    while (1)
    {
        ret = UnixSocketServer_accept(&msg_socket);
        if (ret <= 0)
        {
            printf("airplayd IO UnixSocketServer_accept fail\n");
            if (ret < 0)
            {
                UnixSocketServer_deInit(&msg_socket);
                sleep(5);
                goto RE_INIT;
            }
        }
        else
        {
            memset(recv_buf, 0x0, sizeof(recv_buf));
            recv_num = UnixSocketServer_read(&msg_socket, recv_buf, sizeof(recv_buf));
            if (recv_num <= 0)
            {
                printf("IO UnixSocketServer_recv failed\r\n");
                if (recv_num < 0)
                {
                    UnixSocketServer_close(&msg_socket);
                    UnixSocketServer_deInit(&msg_socket);
                    sleep(5);
                    goto RE_INIT;
                }
            }
            else
            {
                recv_buf[recv_num] = 0;
                
                at_app_ulog(kLogLevelNotice, "Airplay recvd remote ctrl [%s]\n", recv_buf);

                AirPlayReceiverServerRef inServer = AirPlayReceiverServerGetRef();

                if (0){
                }
                else if(strncmp(recv_buf,"AIRPLAY_STOP",strlen("AIRPLAY_STOP"))==0)
                {
                    AirPlayReceiverServerCommandDeselectSource(inServer);
                }else if(strncmp(recv_buf,"AIRPLAY_PAUSE",strlen("AIRPLAY_PAUSE"))==0)
                {
                    AirPlayReceiverServerCommandPause(inServer);
                }
                else if(strncmp(recv_buf,"AIRPLAY_PLAY_PAUSE",strlen("AIRPLAY_PLAY_PAUSE"))==0)
                {
                    AirPlayReceiverServerCommandPlayPause(inServer);
                }
                else if(strncmp(recv_buf,"AIRPLAY_PLAY",strlen("AIRPLAY_PLAY"))==0)
                {
                    AirPlayReceiverServerCommandPlay(inServer);
                }else if(strncmp(recv_buf,"AIRPLAY_NEXT",strlen("AIRPLAY_NEXT"))==0)
                {
                    AirPlayReceiverServerCommandNextTrack(inServer);

                    if(gAirplayPrivCtrlCtx.isPlaying == false && false == gAirplayPrivCtrlCtx.AudioOutputPlaybackThreadExited)
                    {
                        AirPlayReceiverServerCommandPlay(inServer);
                    }
                }
                else if (strncmp(recv_buf, "AIRPLAY_PREV", strlen("AIRPLAY_PREV")) == 0)
                {
                    AirPlayReceiverServerCommandPreviousTrack(inServer);

                    if(gAirplayPrivCtrlCtx.isPlaying == false && false == gAirplayPrivCtrlCtx.AudioOutputPlaybackThreadExited)
                    {
                        AirPlayReceiverServerCommandPlay(inServer);
                    }
                }
                else if (strncmp(recv_buf, "AIRPLAY_VOLUME_UP", strlen("AIRPLAY_VOLUME_UP")) == 0)
                {
                    AirPlayReceiverServerCommandVolumeUp(inServer);
                }
                else if (strncmp(recv_buf, "AIRPLAY_VOLUME_DOWN", strlen("AIRPLAY_VOLUME_DOWN")) == 0)
                {
                    AirPlayReceiverServerCommandVolumeDown(inServer);
                }
                else if (strncmp(recv_buf, "AIRPLAY_SWITCH_OUT", strlen("AIRPLAY_SWITCH_OUT")) == 0)
                {
                    AirPlayReceiverServerCommandDeselectSource(inServer);
                }
                else if (strncmp(recv_buf, "AIRPLAY_VOLUME_SET", strlen("AIRPLAY_VOLUME_SET")) == 0)
                {
                    char *pVolstr = recv_buf + strlen("AIRPLAY_VOLUME_SET:");

                    int vol = atoi(pVolstr);
                    
                    double db = platform_linkplay_dynamic_vol_percent_to_db(vol);

                    AirPlayReceiverServerCommandVolume(inServer, db);
                }
            }

            UnixSocketServer_close(&msg_socket);
        }
    }

    return NULL;
}


void _airplayd_signal_handler(int sig)
{
    switch (sig)
    {
        case SIGKILL:
        case SIGTERM:
        {
            at_app_ulog(kLogLevelNotice, "airplayd exit wit sig [%d]\n", sig);
            AirPlayReceiverServerStop(gAirPlayServer);
            CFReleaseNullSafe(gAirPlayServer);
            exit(-sig);
        }
        break;

        default:
            at_app_ulog(kLogLevelNotice, "Unhandled sig [%d]\n", sig);
            break;
    }
}

static int _get_iface_ip(char *ifname)
{
    int sock, ret;
    struct ifreq ifr;
    struct sockaddr_in *sin;
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        return 0;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);
    ret = ioctl(sock, SIOCGIFADDR, &ifr);
    close(sock);
    if (ret)
        return 0;
    sin = (struct sockaddr_in *)&ifr.ifr_addr;
    return sin->sin_addr.s_addr;
}

static int wait_for_wac_setup_complete(void)
{
    char wac_status[64] = {0};

    wm_db_get(NVRAM_WAC_STATUS, wac_status);

    if (0 == strcmp(wac_status, "CONFIGURED"))
    {
        at_app_ulog(kLogLevelNotice, "Device already configured via WAC.\n");
        return 0;
    }

    at_app_ulog(kLogLevelNotice, "Warning==>Device is not configured via WAC !!\n");
    
wait:
    if(_get_iface_ip("wlan0") || _get_iface_ip("eth0")) {
	at_app_ulog(kLogLevelNotice, "Device already has IP address.\n");
	return 0;
    }
    //if device is not ready, waiting here 
    if (!g_AirplayIsReadyToGo)
    {
        usleep(10000);
        goto wait;
    }

    return 0;
}


int platform_linkplay_init(void)
{
    OSStatus err;
    pthread_t cmd_thread;

    signal(SIGKILL, _airplayd_signal_handler);
    signal(SIGTERM, _airplayd_signal_handler);

    err = _airplayd_init_global_vars();
    require_noerr(err, exit);

    err = pthread_create(&cmd_thread, NULL, platform_linkplay_internal_msg_handler, NULL);
    require_noerr(err, exit);

    err = pthread_create(&cmd_thread, NULL, platform_linkplay_remote_control_handler, NULL);
    require_noerr(err, exit);

    wait_for_wac_setup_complete();

#ifdef A98_BELKIN_MODULE
    if(0 != wm_circle_buffer_shm_init(&gAirPlayCircluBuffer))
    {
        at_app_ulog(kLogLevelNotice, "Airplay init circle buffer failed !!\n");
        return 0;
    }
#endif

exit:
    return (err ? 1 : 0);
}

#endif /* LINKPLAY */

