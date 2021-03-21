/**
 * Copyright (c) 2006-2014 Spotify AB
 *
 * This file is part of the Spotify Embedded SDK examples suite.
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>

#include "spotify_embedded.h"
#include "webserver.h"
#include "audio.h"
#include "spotify_api.h"
#include "spotify_embedded_play_api.h"
#include "wm_util.h"
#include "lp_list.h"
#include <upnp/ixml.h>
#include "play_list.h"
#include "playlistqueue.h"
#include "spotify_avs_cmd.h"
#include "spotify_avs_macro.h"

typedef enum {
    VOL_MAIN = 0,
    VOL_MCU,
    VOL_AIRPLAY,
    VOL_ALIRPC,
    VOL_SPOTIFY,
    VOL_UPNP,
    VOL_ALEXA,
    VOL_MASTER,
    VOL_PROMPT,
} vol_source_type;

typedef enum { SP_NONE = 0, SP_DISC = 1, SP_RECON = 2 } SP_CON_STAT;

extern WIIMU_CONTEXT *g_wiimu_shm;
extern pthread_cond_t *curl_cond;
extern pthread_mutex_t *curl_mutex;
extern void add_value_element(IXML_Document *, IXML_Element *, char *, char *);
extern void add_value_element_int(IXML_Document *, IXML_Element *, char *, int);
extern void CStandaloneNode_SetVol(int, vol_source_type, int);
extern int start_request(const char *, const char *, const char *);

int g_spotify_print = 0;
pthread_mutex_t spotify_playlist_mutex;
void SpotifyAvsConnectionNotify(enum SpConnectionNotification);
char *GetSpotifyCurrentSource(void);
void dumppresetslots(void);

#define SPOTI_DEBUG(fmt, ...)                                                                      \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d][Spotify ESDK] " fmt,                                \
                (int)(tickSincePowerOn() / 3600000), (int)(tickSincePowerOn() % 3600000 / 60000),  \
                (int)(tickSincePowerOn() % 60000 / 1000), (int)(tickSincePowerOn() % 1000),        \
                ##__VA_ARGS__);                                                                    \
    } while (0)

static struct SpotifyStatus g_spotifyStatus;
static struct SpExampleAudioCallbacks audio_cb;
static pthread_mutex_t mutex_cmd_list = PTHREAD_MUTEX_INITIALIZER;
static void CallbackError(SpError error, void *context);
static void CallbackPlaybackNotify(enum SpPlaybackNotification event, void *context);
static void CallbackPlaybackSeek(uint32_t position_ms, void *context);
static void CallbackPlaybackApplyVolume(uint16_t volume, uint8_t remote, void *context);
static void CallbackConnectionNotify(enum SpConnectionNotification event, void *context);
static void CallbackConnectionMessage(const char *message, void *context);
static void CallbackDebugMessage(const char *debug_message, void *context);
static void CallbackNewCredentials(const char *credentials_blob, const char *username,
                                   void *context);
#ifdef EN_AVS_SPOTIFY
static void SpotifyAvsPlaybackNotify(enum SpPlaybackNotification event);
#endif
struct SpotifyStatus {
    int init_ok; /* SpInit OK */
    int pause_switch;
    int ss_track_metadata_change; /*kSpPlaybackNotifyMetadataChanged */
    int ss_play_status;           /* Play or Pause? */
    int ss_active_status;         /* Active or Inactive? */
    int ss_first_group_volume;    /*Volume first sync*/
    int ss_logged;                /* We are logged in. */
    int ss_track_valid;           /* Metadata are valid? */

    // { for avs spotify
    int ss_avs_online;
    int ss_is_avs_login;
    int ss_is_need_login;
    int is_auto_login;
    int ss_request_token_cnt;
    // }
    int grouped;
    uint32_t ss_volume;    /* Volume level. */
    uint32_t ss_track_pos; /* The track time we are playing. */

    SpNotifyUserCallBack Notifycallbacks;
    struct SpMetadata ss_track; /* metadata for the current track */

    // { for avs spotify
    char *ss_access_token;
    char *ss_user_name;
    char *ss_unique_id;
    char *ss_correlation_token;
    char *ss_backup_cmd;

    long long ss_refresh_time; // ms
    // }
};

#define Retry_login_cnt (6)
#define RETRY_MIN_MS (5000L)

const int g_retry_table[Retry_login_cnt] = {
    2600,  // Retry 1:  1.6s -> .8s to 2.4s
    3000,  // Retry 2:  2.0s -> 1.0s to 3s
    5000,  // Retry 3:  3.00s
    6000,  // Retry 4:  5.00s
    10000, // Retry 5: 10.00s
    20000, // Retry 6: 20.00s
};

typedef struct _sp_cmd_action {
    struct lp_list_head entry;
    SpotifyAction spCmd;
    int params;
} sp_cmd_action;

LP_LIST_HEAD(sp_cmd_list);

#ifdef EN_AVS_SPOTIFY
typedef struct _avs_notify_s {
    struct lp_list_head entry;
    int event;
    unsigned int params;
} avs_notify_t;
LP_LIST_HEAD(avs_notify_list);
static pthread_mutex_t avs_notify_list_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

#define ZEROCONF_CGI_PATH "/goform/spotifyConnect"
#define DEFAULT_KEY_FILE "spotify_appkey.key"
#define SPOTIFY_PRESET_FILE "/vendor/wiimu/spotify.preset"
#define SPOTIFY_USER_FILE "/vendor/wiimu/spotify.user"
#define SPOTIFY_ACTION_FILE "/vendor/wiimu/spotify_action.preset"

#define PRESETS_NUM 30
#define MAX_PRESET_NAME SP_MAX_METADATA_NAME_LENGTH + 1
#define ACTION_LIST_NUM 6

struct Preset {
    char preset_name[MAX_PRESET_NAME];
    uint8_t blob[SP_PRESET_BUFFER_SIZE]; /* buffer to host preset blob. */
    size_t blob_length;                  /* length of the data in the buffer. */
    uint32_t pos;
    uint32_t solt;
    uint32_t preset_key;
    int press_type;
    int volume;
};

struct ActionContext {
    SpotifyAction spCmd;
    unsigned int params;
};

struct Userinfo {
    char username[SP_MAX_USERNAME_LENGTH + 1];
    char blob[SP_MAX_ZEROCONF_BLOB_LENGTH + 1];
};

static struct Preset preset_slots[PRESETS_NUM] = {0};
static struct Userinfo user_logInfo = {0};
uint8_t g_preset_buffer[SP_PRESET_BUFFER_SIZE] = {0};
uint32_t first_slot_index = 0;
static struct Preset action_slots[ACTION_LIST_NUM] = {0};

char g_spotify_modle_name[64] = {0};
char g_spotify_brand_name[64] = {0};

static int g_wait_login = 0;
static SP_CON_STAT sp_conn = SP_NONE;

int g_spotify_active = 0;

#if defined(MARSHALL_ACTON_AVS_TBD_BETA) || defined(MARSHALL_STANMORE_AVS_TBD_BETA) ||             \
    defined(A98_MARSHALL_STEPIN)
const char *clientid = "a56a9ca4c3224149844c062bbebf64a4";
#elif defined(A98_TEUFEL)
const char *clientid = "e344c9a0cd5745fbab413cffb00e6cf9";
#elif defined(A98_SPRINT)
const char *clientid = "f3295707e7164cf081e7e9b76ef218ff";
#elif defined(A98_ALEXA_BELKIN)
const char *clientid = "787ef7e99c104dfeb382691600c86521";
#elif defined(A98SHUNYI_PRJ)
const char *clientid = "92a968f167944b3da67de2e0660b869e";
#elif defined(A98_MOXIE2_256M)
const char *clientid = "fe1105cd4bba45f5bdc06b60aa666e87";
#elif defined(A98_RUSSOUND_TP1)
const char *clientid = "ebbd9c9ed77a4d3ba5976297e3dbe06a";
#elif defined(A98YOUYISHI_LV_HOME) || defined(A98YOUYISHI_LV_CLUTCH)
const char *clientid = "35f1588f40ba489bbf3cd507e5cb6b1a";
#else
const char *clientid = "d7db7f206f7e467b91026ccc69caaec0"; // linkplay default
#endif

#define CheckMetadataValiad(metadata)                                                              \
    (strlen(metadata.album_uri) > 0 || strlen(metadata.artist_uri) > 0 ||                          \
     strlen(metadata.playback_source_uri) > 0)
#ifdef EN_AVS_SPOTIFY
static int SpotifyGetRetryDelay(int retry_cnt)
{
    retry_cnt = retry_cnt < 0 ? 0 : retry_cnt;
    retry_cnt = retry_cnt >= Retry_login_cnt ? Retry_login_cnt - 1 : retry_cnt;
    return (int)g_retry_table[retry_cnt];
}
#endif

static void *SpotifyWaitLoginThread(void *context)
{
    SpError err;
    struct ActionContext *action = (struct ActionContext *)context;
    printf("SpotifyWaitLoginThread  \r\n");

    SpLoginEventClean();

    int tryCount = 2;
    do {
        err = SpExampleWebWaitSpotifyLogin(20000);
    } while (err != kSpErrorOk && tryCount--);

    g_wait_login = 0;
    SpotifyUserCmd(action->spCmd, action->params);

    free(action);

    return NULL;
}

static void savepresettoflash(void)
{
    FILE *file = fopen(SPOTIFY_PRESET_FILE, "wb+");
    if (file) {
        fwrite(&preset_slots[0], 1, PRESETS_NUM * sizeof(struct Preset), file);
        fwrite(&first_slot_index, 1, sizeof(uint32_t), file);
        fflush(file);
        fclose(file);
        sync();
    }
}

static void loadpresetformflash(void)
{
    FILE *file = fopen(SPOTIFY_PRESET_FILE, "rb");
    if (file) {
        fread(&preset_slots[0], 1, PRESETS_NUM * sizeof(struct Preset), file);
        fread(&first_slot_index, 1, sizeof(uint32_t), file);
        fclose(file);
    }
}

static void saveuserinfotoflash(void)
{
    FILE *file = fopen(SPOTIFY_USER_FILE, "wb+");
    if (file) {
        fwrite(&user_logInfo, 1, sizeof(struct Userinfo), file);
        fflush(file);
        fclose(file);
        sync();
    }
}

static void loaduserinfoformflash(void)
{
    FILE *file = fopen(SPOTIFY_USER_FILE, "rb");
    if (file) {
        fread(&user_logInfo, 1, sizeof(struct Userinfo), file);
        fclose(file);
    }
}

void dumpactionslots(void)
{
    int i = 0;
    SPOTI_DEBUG("***** dumpactionlistslots *****\n");
    for (i = 0; i < ACTION_LIST_NUM; i++) {
        SPOTI_DEBUG("action list_name %s,preset_key %d, blob_length %d, pos %d, solt %d, "
                    "press_type %d, volume %d\n",
                    action_slots[i].preset_name, action_slots[i].preset_key,
                    action_slots[i].blob_length, action_slots[i].pos, action_slots[i].solt,
                    action_slots[i].press_type, action_slots[i].volume);
    }
    SPOTI_DEBUG("***** dumpactionlistslots *****\n");
}

#ifdef IHOME_ALEXA
static void saveactionlisttoflash(void)
{
    FILE *file = fopen(SPOTIFY_ACTION_FILE, "wb+");
    if (file) {
        fwrite(&action_slots[0], 1, ACTION_LIST_NUM * sizeof(struct Preset), file);
        fflush(file);
        fclose(file);
        sync();
    }
}
#endif

static void loadactionlistformflash(void)
{
    FILE *file = fopen(SPOTIFY_ACTION_FILE, "rb");
    if (file) {
        fread(&action_slots[0], 1, ACTION_LIST_NUM * sizeof(struct Preset), file);
        fclose(file);
    }
}

int checkpresetkey(int preset_key)
{
    int i = 0;
    int ret = 0;
    for (; i < PRESETS_NUM; i++) {
        if (preset_slots[i].preset_key == preset_key + 1) {
            ret = 1;
            break;
        }
    }
    return ret;
}

int getpresetindex(char *playsource, int savePreset)
{
    int i = 0;
    int solt = -1;

    for (i = 0; i < PRESETS_NUM; i++) {
        if (strcmp(preset_slots[i].preset_name, playsource) == 0) {
            if (savePreset)
                solt = i;
            else
                return -2;
        }
        if (solt < 0 && strlen(preset_slots[i].preset_name) <= 0) {
            solt = i;
        }
    }
    if (solt < 0) {
        for (i = first_slot_index; i < PRESETS_NUM; i++) {
            if (preset_slots[i].preset_key <= 0 && !(preset_slots[i].press_type)) {
                solt = i;
                break;
            }
        }
        if (solt < 0) {
            for (i = 0; i < first_slot_index; i++) {
                if (preset_slots[i].preset_key <= 0 && !(preset_slots[i].press_type)) {
                    solt = i;
                    break;
                }
            }
        }
        first_slot_index = solt + 1;
        if (first_slot_index >= PRESETS_NUM)
            first_slot_index = 0;
    }
    SPOTI_DEBUG("get preset solt index: %d, first_slot_index %d, savePresetKey %d\n", solt,
                first_slot_index, savePreset);

    return solt;
}

void dumppresetslots()
{
    int i = 0;
    SPOTI_DEBUG("***** dumppresetslots *****\n");
    for (i = 0; i < PRESETS_NUM; i++) {
        SPOTI_DEBUG("preset_name %s,preset_key %d, blob_length %d, pos %d, solt %d, press_type %d, "
                    "volume %d\n",
                    preset_slots[i].preset_name, preset_slots[i].preset_key,
                    preset_slots[i].blob_length, preset_slots[i].pos, preset_slots[i].solt,
                    preset_slots[i].press_type, preset_slots[i].volume);
    }
    SPOTI_DEBUG("first_slot_index %d\n", first_slot_index);
    SPOTI_DEBUG("***** dumppresetslots *****\n");
}

int getPresetIndexBykey(int preset_key)
{
    int i = 0;
    int solt = -1;
    for (i = 0; i < PRESETS_NUM; i++) {
        if (preset_slots[i].preset_key == preset_key + 1) {
            solt = i;
            break;
        }
    }
    SPOTI_DEBUG("get preset solt index %d, by preset_key %d\n", solt, preset_key + 1);

    return solt;
}

char *GenerateSpotifyListtoXMLString(char *name)
{
    IXML_Document *doc;
    IXML_Element *root;
    IXML_Element *child;
    IXML_Element *list;
    char *result = NULL;
    int i = 0;
    int index = 0;

    SPOTI_DEBUG("GenerateSpotifyPresettoXMLString ++ \r\n");
    doc = ixmlDocument_createDocument();
    if (!doc) {
        SPOTI_DEBUG("GenerateSpotifyPresettoXMLString error \r\n");
        return NULL;
    }
    root = ixmlDocument_createElement(doc, "SpotifyList");
    if (!root) {
        SPOTI_DEBUG("GenerateSpotifyPresettoXMLString ixml_createElement error.\n");
        return NULL;
    }
    pthread_mutex_lock(&spotify_playlist_mutex);
    ixmlNode_appendChild((IXML_Node *)doc, (IXML_Node *)root);
    child = ixmlDocument_createElement(doc, "SpotifyInfo");
    ixmlNode_appendChild((IXML_Node *)root, (IXML_Node *)child);
    add_value_element(doc, child, "Name", "SpotifyPresetList");
    add_value_element(doc, child, "Source", "SPOTIFY");
    for (i = 0; i < PRESETS_NUM; i++) {
        char buf[8] = {0};
        if (strlen(preset_slots[i].preset_name) > 0) {
            snprintf(buf, 8, "List%d", ++index);
            list = ixmlDocument_createElement(doc, buf);
            ixmlNode_appendChild((IXML_Node *)root, (IXML_Node *)list);
            add_value_element(doc, list, "PresetName", preset_slots[i].preset_name);
            add_value_element_int(doc, list, "PresetKey", preset_slots[i].preset_key);
            add_value_element_int(doc, list, "Press_type", preset_slots[i].press_type);
            add_value_element_int(doc, list, "Volume", preset_slots[i].volume);
        }
    }
    add_value_element_int(doc, child, "ListNumber", index);
    if (doc != NULL) {
        result = ixmlDocumenttoString(doc);
        ixmlDocument_free(doc);
    }
    pthread_mutex_unlock(&spotify_playlist_mutex);
    SPOTI_DEBUG("GenerateSpotifyPresettoXMLString -- %s \r\n", result ? result : "null");

    return result;
}

char *GenerateSpotifyPresetMaptoKeytoXMLString(int include_preset, int include_press)
{
    IXML_Document *doc;
    IXML_Element *root;
    IXML_Element *child;
    IXML_Element *current;
    IXML_Element *list;
    char *result = NULL;
    int index = 0;
    int i = 0;

    SPOTI_DEBUG("GenerateSpotifyPresetMaptoKeytoXMLString ++ \r\n");
    doc = ixmlDocument_createDocument();
    if (!doc) {
        SPOTI_DEBUG("GenerateSpotifyPresetMaptoKeytoXMLString error \r\n");
        return NULL;
    }
    pthread_mutex_lock(&spotify_playlist_mutex);
    root = ixmlDocument_createElement(doc, "SpotifyList");
    ixmlNode_appendChild((IXML_Node *)doc, (IXML_Node *)root);
    if (SpPlaybackIsActiveDevice() && GetSpotifyCurrentSource()) {
        current = ixmlDocument_createElement(doc, "CurrentPreset");
        ixmlNode_appendChild((IXML_Node *)root, (IXML_Node *)current);
        add_value_element(doc, current, "PresetName", GetSpotifyCurrentSource());
    }
    list = ixmlDocument_createElement(doc, "SpotifyInfo");
    ixmlNode_appendChild((IXML_Node *)root, (IXML_Node *)list);
    add_value_element(doc, list, "Name", "SpotifyPresetMaptoKey");
    add_value_element(doc, list, "Source", "SPOTIFY");
    for (i = 0; i < ACTION_LIST_NUM; i++) {
        char buf[8] = {0};
        if (include_press && !(action_slots[i].press_type)) {
            continue;
        }
        snprintf(buf, 8, "List%d", ++index);
        child = ixmlDocument_createElement(doc, buf);
        ixmlNode_appendChild((IXML_Node *)root, (IXML_Node *)child);
        add_value_element(doc, child, "PresetName", action_slots[i].preset_name);
        add_value_element_int(doc, child, "PresetKey", action_slots[i].preset_key);
        add_value_element_int(doc, child, "PressType", action_slots[i].press_type);
        add_value_element_int(doc, child, "Volume", action_slots[i].volume);
    }
    add_value_element_int(doc, list, "ListNumber", index);
    if (doc != NULL) {
        result = ixmlDocumenttoString(doc);
        ixmlDocument_free(doc);
    }
    pthread_mutex_unlock(&spotify_playlist_mutex);
    SPOTI_DEBUG("GenerateSpotifyPresetMaptoKeytoXMLString -- \r\n");

    return result;
}

#ifdef IHOME_ALEXA
static struct Preset *LoadPresetByName(char *name)
{
    int i = 0;
    for (i = 0; i < PRESETS_NUM; i++) {
        if (strcmp(preset_slots[i].preset_name, name) == 0) {
            preset_slots[i].solt = i;
            return &preset_slots[i];
        }
    }

    return NULL;
}
#endif

int getFreeActionIndex(void)
{
    int i = 0;
    for (i = 0; i < ACTION_LIST_NUM; i++) {
        if (strlen(action_slots[i].preset_name) <= 0)
            return i;
    }
    return -1;
}

int haveSameActionlistByName(char *name)
{
    int ret = 0;
    int i = 0;
    for (i = 0; i < ACTION_LIST_NUM; i++) {
        if (strncmp(action_slots[i].preset_name, name, strlen(name)) == 0) {
            ret = 1;
            break;
        }
    }
    SPOTI_DEBUG("%s: name %s, ret %d \n", __func__, name, ret);
    return ret;
}

int UpdateSpotifyList(play_list *playlist)
{
    int ret = 0;

#ifdef IHOME_ALEXA
    int i = 0;
    ret = -1;
    pthread_mutex_lock(&spotify_playlist_mutex);
    for (i = 0; i < ACTION_LIST_NUM; i++) {
        if (strcmp(action_slots[i].preset_name, playlist->list_name) == 0) {
            action_slots[i].press_type = playlist->info.press_type;
            action_slots[i].volume = playlist->info.volume;
            ret = 0;
            break;
        }
    }
    if (ret != 0) {
        int index = getFreeActionIndex();
        if (index >= 0) {
            struct Preset *preset = NULL;
            char *ptr = NULL;
            char name[256] = {0};
            snprintf(name, 256, "%s", playlist->list_name);
            if ((ptr = strstr(name, QUEUE_NAME_EXTERN)) != NULL) {
                *ptr = '\0';
            }
            preset = LoadPresetByName(name);
            if (preset) {
                memcpy(&action_slots[index], preset, sizeof(struct Preset));
                preset->press_type = 1; // preset_slots[i]  cannot delete
                strncpy(action_slots[index].preset_name, playlist->list_name, MAX_PRESET_NAME - 1);
                action_slots[index].press_type = playlist->info.press_type;
                action_slots[index].volume = playlist->info.volume;
                ret = 0;
            }
        }
    }
    if (ret == 0) {
        savepresettoflash();
        saveactionlisttoflash();
    }
    pthread_mutex_unlock(&spotify_playlist_mutex);
#endif

    return ret;
}

int GetSpotifyListByPressType(int press_type, int *volume)
{
    int i = 0;
    int ret = -1;
    pthread_mutex_lock(&spotify_playlist_mutex);
    for (i = 0; i < ACTION_LIST_NUM; i++) {
        if (action_slots[i].press_type == press_type) {
            ret = action_slots[i].solt;
            if (volume)
                *volume = action_slots[i].volume;
            break;
        }
    }
    pthread_mutex_unlock(&spotify_playlist_mutex);
    return ret;
}

int DeleteSpotifyListByPressType(int press_type)
{
    int ret = -1;

#ifdef IHOME_ALEXA
    int i = 0;
    char name[256] = {0};
    SPOTI_DEBUG("DeleteSpotifyListByPressType press_type %d ++\n", press_type);
    pthread_mutex_lock(&spotify_playlist_mutex);
    for (i = 0; i < ACTION_LIST_NUM; i++) {
        if (action_slots[i].press_type == press_type) {
            snprintf(name, 256, "%s", action_slots[i].preset_name);
            memset(&action_slots[i], 0, sizeof(struct Preset));
            ret = 0;
        }
    }
    if (ret == 0) {
        char *ptr = NULL;
        struct Preset *preset = 0;
        if ((ptr = strstr(name, QUEUE_NAME_EXTERN)) != NULL) {
            *ptr = '\0';
        }
        if (haveSameActionlistByName(name) == 0) {
            preset = LoadPresetByName(name);
            if (preset) {
                preset->press_type = 0; // preset_slots[i]  can delete
                ret = 0;
                savepresettoflash();
            }
        }
        saveactionlisttoflash();
    }
    pthread_mutex_unlock(&spotify_playlist_mutex);
    SPOTI_DEBUG("DeleteSpotifyListByPressType ret %d --\n", ret);
#endif

    return ret;
}

int DeleteSpotifyListByName(char *playsource, int key)
{
    int ret = -1;

#ifdef IHOME_ALEXA
    int i = 0;
    char name[256] = {0};
    pthread_mutex_lock(&spotify_playlist_mutex);
    if (playsource) {
        for (i = 0; i < ACTION_LIST_NUM; i++) {
            if (strcmp(action_slots[i].preset_name, playsource) == 0) {
                if (action_slots[i].preset_key > 0 || action_slots[i].press_type) {
                    ret = -2;
                    break;
                }
                snprintf(name, 256, "%s", action_slots[i].preset_name);
                memset(&action_slots[i], 0, sizeof(struct Preset));
                ret = 0;
            }
        }
        if (ret == 0) {
            char *ptr = NULL;
            struct Preset *preset = 0;
            if ((ptr = strstr(name, QUEUE_NAME_EXTERN)) != NULL) {
                *ptr = '\0';
            }
            if (haveSameActionlistByName(name)) {
                preset = LoadPresetByName(name);
                if (preset) {
                    preset->press_type = 0; // preset_slots[i]  can delete
                    ret = 0;
                    savepresettoflash();
                }
            }
            saveactionlisttoflash();
        }
    }
    pthread_mutex_unlock(&spotify_playlist_mutex);
#endif

    return ret;
}

int DeleteSpotifyPresetByKey(int preset_key)
{
    int ret = -1;
#ifdef IHOME_ALEXA
    int i = 0;
    pthread_mutex_lock(&spotify_playlist_mutex);
    for (i = 0; i < PRESETS_NUM; i++) {
        if (preset_slots[i].preset_key == preset_key + 1) {
            preset_slots[i].preset_key = 0;
            ret = 0;
        }
    }
    if (ret == 0)
        savepresettoflash();
    pthread_mutex_unlock(&spotify_playlist_mutex);
#endif

    return ret;
}

static uint8_t CallbackSavePreset(int preset_id, uint32_t playback_position, const uint8_t *buffer,
                                  size_t buff_size, SpError error, void *context)
{
    if (error != kSpErrorOk || buffer == NULL) {
        return 0;
    }
    SPOTI_DEBUG("CallbackSavePreset preset_id %d\r\n", preset_id);
    pthread_mutex_lock(&spotify_playlist_mutex);
    if (preset_id >= 0 && preset_id < PRESETS_NUM) {
        preset_slots[preset_id].blob_length = buff_size;
        preset_slots[preset_id].pos = playback_position;
        memcpy(preset_slots[preset_id].blob, buffer, buff_size);
        savepresettoflash();
    }
    pthread_mutex_unlock(&spotify_playlist_mutex);

    return 1;
}

static struct Preset *LoadPreset(int slot) { return &preset_slots[slot]; }

char *GetSpotifyCurrentSource(void)
{
    return strlen(g_spotifyStatus.ss_track.playback_source) > 0
               ? g_spotifyStatus.ss_track.playback_source
               : g_spotifyStatus.ss_track.album;
}

#ifdef EN_AVS_SPOTIFY
int SpotifyDelAvsNotify(int event, unsigned int params)
{
    avs_notify_t *avs_notify = NULL;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;

    pthread_mutex_lock(&avs_notify_list_lock);
    if (lp_list_number(&avs_notify_list) > 0) {
        lp_list_for_each_safe(pos, npos, &avs_notify_list)
        {
            avs_notify = lp_list_entry(pos, avs_notify_t, entry);
            if (avs_notify) {
                if (avs_notify->event == event && avs_notify->params == params) {
                    lp_list_del(&avs_notify->entry);
                    free(avs_notify);
                    avs_notify = NULL;
                }
            }
        }
    }
    pthread_mutex_unlock(&avs_notify_list_lock);

    return 0;
}

int SpotifyAddAvsNotify(int event, unsigned int params)
{
    avs_notify_t *avs_notify_new = NULL;

    // replace the same event
    SpotifyDelAvsNotify(event, params);

    avs_notify_new = (avs_notify_t *)malloc(sizeof(avs_notify_t));
    memset(avs_notify_new, 0x0, sizeof(avs_notify_t));
    avs_notify_new->event = event;
    avs_notify_new->params = params;
    pthread_mutex_lock(&avs_notify_list_lock);
    lp_list_add_tail(&avs_notify_new->entry, &avs_notify_list);
    pthread_mutex_unlock(&avs_notify_list_lock);

    return 0;
}

avs_notify_t *GetAvsNotify(void)
{
    struct lp_list_head *pos = NULL;
    avs_notify_t *avs_notify = NULL;

    pthread_mutex_lock(&avs_notify_list_lock);
    if (lp_list_number(&avs_notify_list) > 0) {
        lp_list_for_each(pos, &avs_notify_list)
        {
            avs_notify = lp_list_entry(pos, avs_notify_t, entry);
            lp_list_del(&avs_notify->entry);
            break;
        }
    }
    pthread_mutex_unlock(&avs_notify_list_lock);

    return avs_notify;
}

// Need to free the string
char *SpotifyPlayerState(int need_login)
{
    char *player_state = NULL;
    int is_guest = 0;
    int is_logged = 0;
    int is_active = 0;

    const char *user_name = SpGetCanonicalUsername();
    if (g_spotifyStatus.ss_user_name && user_name &&
        !StrEq(user_name, g_spotifyStatus.ss_user_name)) {
        is_guest = 1;
    }
    is_active = SpPlaybackIsActiveDevice();
    if (need_login == 0) {
        is_logged = SpConnectionIsLoggedIn();
    }

    player_state = avs_set_player_state(g_spotifyStatus.ss_unique_id, (char *)user_name, is_logged,
                                        is_guest, g_spotifyStatus.init_ok, is_active);

    return player_state;
}

// Need to free the string
char *SpotifyPlaybackMedia(void)
{
    char *playback_media = NULL;
    playback_media = avs_set_media(
        g_spotifyStatus.ss_track.playback_source, g_spotifyStatus.ss_track.playback_source_uri,
        g_spotifyStatus.ss_track.track, g_spotifyStatus.ss_track.track_uri, NULL,
        g_spotifyStatus.ss_track.artist, g_spotifyStatus.ss_track.artist_uri,
        g_spotifyStatus.ss_track.album, g_spotifyStatus.ss_track.album_uri,
        g_spotifyStatus.ss_track.album_cover_uri, g_spotifyStatus.ss_track.duration_ms);

    return playback_media;
}

// Need to free the string ["", "", ""]
int SpotifyPlayBackSupportOps(char *support_ops)
{
    SpError err = kSpErrorFailed;
    struct SpMetadata meta_data_next = {0};

    if (SpPlaybackIsPlaying() > 0) {
        strcat(support_ops, "[\"Pause\"");
        strcat(support_ops, ",\"Stop\"");
    } else {
        strcat(support_ops, "[\"Play\"");
    }
    strcat(support_ops, ",\"Previous\"");
    err = SpGetMetadata(&meta_data_next, kSpMetadataTrackNext);
    if ((err == kSpErrorOk && CheckMetadataValiad(meta_data_next)) || !SpPlaybackIsRepeated()) {
        strcat(support_ops, ",\"Next\"");
    }
    if (!SpPlaybackIsShuffled()) {
        strcat(support_ops, ",\"EnableShuffle\"");
    } else {
        strcat(support_ops, ",\"DisableShuffle\"");
    }
    if (CheckMetadataValiad(g_spotifyStatus.ss_track)) {
        strcat(support_ops, ",\"StartOver\"");
        strcat(support_ops, ",\"AdjustSeekPosition\"");
        strcat(support_ops, ",\"SetSeekPosition\"");
    }
    if (!SpPlaybackIsRepeated()) {
        strcat(support_ops, ",\"EnableRepeat\"");
    } else {
        strcat(support_ops, ",\"DisableRepeat\"");
    }
    strcat(support_ops, "]");

    return 0;
}

// Need to free the string
char *SpotifyPlayBackState(uint32_t position_ms)
{
    char *playback_state = NULL;
    char *playback_media = NULL;
    char support_ops[258] = {0};
    int play_status = 0;

    playback_media = SpotifyPlaybackMedia();
    SpotifyPlayBackSupportOps(support_ops);
    if (position_ms == 0) {
        position_ms = SpPlaybackGetPosition();
    }
    if (SpPlaybackIsPlaying() > 0 && SpPlaybackIsActiveDevice() > 0) {
        play_status = 1;
    }
    Create_Playback_State(playback_state, Val_state(play_status), Val_Array(support_ops),
                          Val_Media(playback_media), position_ms,
                          Val_shuffle(SpPlaybackIsShuffled()), Val_repeat(SpPlaybackIsRepeated()),
                          Val_favorite(0));

    if (playback_media) {
        free(playback_media);
    }

    return playback_state;
}

SpError SpotifyAvsErrorNotify(char *err_name, uint32_t err_no, char *err_des, int fatal)
{
    SpError err = kSpErrorFailed;
    char *event = NULL;
    char *event_params = NULL;

    Create_PlayerError_Params(event_params, Val_Str(g_spotifyStatus.ss_correlation_token),
                              Val_Str(err_name), err_no, Val_Str(err_des), Val_Bool(fatal));
    Create_Emplayer_Cmd(event, Event_PlayerError, Val_Obj(event_params));

    if (event && g_spotifyStatus.Notifycallbacks) {
        g_spotifyStatus.Notifycallbacks(sp_nofity_avs, (unsigned int)event, 0);
        err = kSpErrorOk;
    }

    if (event_params) {
        free(event_params);
    }
    if (event) {
        free(event);
    }

    return err;
}

SpError SpotifyAvsEventNotify(char *event_name, char *player_event, uint32_t param)
{
    SpError err = kSpErrorFailed;
    char *event = NULL;
    char *event_params = NULL;
    char *player_state = NULL;
    char *playback_state = NULL;

    if (StrEq(event_name, Event_ChangeReport) || StrEq(event_name, Event_Logout)) {
        player_state = SpotifyPlayerState(0);
        playback_state = SpotifyPlayBackState(param);
        Create_Comm_Params(event_params, Val_Bool(1), Val_Obj(player_state),
                           Val_Obj(playback_state));
    } else if (StrEq(event_name, Event_Login)) {
        player_state = SpotifyPlayerState(0);
        playback_state = SpotifyPlayBackState(0);
        Create_Login_Params(event_params, Val_Bool(param), Val_Bool(1), Val_Obj(player_state),
                            Val_Obj(playback_state));
    } else if (StrEq(event_name, Event_RequestToken)) {
        player_state = SpotifyPlayerState(0);
        playback_state = SpotifyPlayBackState(param);
        Create_Comm_Params(event_params, Val_Bool(1), Val_Obj(player_state),
                           Val_Obj(playback_state));
    } else if (StrEq(event_name, Event_PlayerEvent)) {
        player_state = SpotifyPlayerState(0);
        playback_state = SpotifyPlayBackState(param);
        Create_PlayerEvent_Params(event_params, Val_Str(g_spotifyStatus.ss_correlation_token),
                                  Val_Str(player_event), Val_Bool(1), Val_Obj(player_state),
                                  Val_Obj(playback_state));
    }
    Create_Emplayer_Cmd(event, event_name, Val_Obj(event_params));

    if (event && g_spotifyStatus.Notifycallbacks) {
        g_spotifyStatus.Notifycallbacks(sp_nofity_avs, (unsigned int)event, 0);
        err = kSpErrorOk;
    }

    if (event_params) {
        free(event_params);
    }
    if (player_state) {
        free(player_state);
    }
    if (playback_state) {
        free(playback_state);
    }
    if (event) {
        free(event);
    }

    return err;
}

SpError SpotifyBeforeAvsAction(char *json_cmd_str)
{
    SpError err = kSpErrorOk;
    uint8_t is_logged = SpConnectionIsLoggedIn();

    if (g_spotifyStatus.ss_is_avs_login == 0 && is_logged == 0) {
        if (g_spotifyStatus.ss_access_token == NULL) {
            err = SpotifyAvsEventNotify(Event_RequestToken, NULL, 0);
            g_spotifyStatus.ss_request_token_cnt = Retry_login_cnt;
            int retry_delay = SpotifyGetRetryDelay(g_spotifyStatus.ss_request_token_cnt);
            g_spotifyStatus.ss_refresh_time = tickSincePowerOn() + retry_delay;
        } else {
            err = SpConnectionLoginOauthToken(g_spotifyStatus.ss_access_token);
            g_spotifyStatus.ss_is_avs_login = 1;
        }
    }

    if (g_spotifyStatus.ss_is_avs_login == 1 || is_logged == 0) {
        err = kSpErrorFailed;
    }

    if (err == kSpErrorFailed && json_cmd_str) {
        if (g_spotifyStatus.ss_backup_cmd) {
            free(g_spotifyStatus.ss_backup_cmd);
            g_spotifyStatus.ss_backup_cmd = NULL;
        }
        g_spotifyStatus.ss_backup_cmd = strdup(json_cmd_str);
    }

    return err;
}

SpError SpotifyAvsCmdParse(char *json_cmd_str)
{
    SpError err = kSpErrorOk;
    struct SpMetadata metadata = {0};
    json_object *json_cmd = NULL;
    json_object *json_cmd_params = NULL;
    char *cmd = NULL;

    if (!json_cmd_str) {
        SPOTI_DEBUG("json_cmd_str is NULL\n");
        return err;
    }

    json_cmd = json_tokener_parse(json_cmd_str);
    if (!json_cmd) {
        SPOTI_DEBUG("json_cmd_str is not a json\n");
        goto Exit;
    }

    json_cmd_params = json_object_object_get(json_cmd, Key_Cmd_params);

    cmd = JSON_GET_STRING_FROM_OBJECT(json_cmd, Key_Cmd);
    if (StrEq(cmd, Cmd_online)) {
        if (json_cmd_params) {
            json_bool is_online = JSON_GET_BOOL_FROM_OBJECT(json_cmd_params, Key_online);
            if (is_online) {
                int is_logged = SpConnectionIsLoggedIn();
                if (g_spotifyStatus.ss_access_token == NULL || 0 == is_logged) {
                    err = SpotifyAvsEventNotify(Event_ChangeReport, NULL, 0);
                    /* first time to online, need to get token from AVS */
                    g_spotifyStatus.ss_refresh_time = tickSincePowerOn();
                }
                g_spotifyStatus.ss_avs_online = 1;
            } else {
                g_spotifyStatus.ss_avs_online = 0;
            }
        }
    } else if (StrEq(cmd, Cmd_Login)) {
        if (json_cmd_params) {
            char *access_token = JSON_GET_STRING_FROM_OBJECT(json_cmd_params, Key_accessToken);
            // This is not need to set user_name, when login success we can get the user name from
            // esdk
            // char * user_name = JSON_GET_STRING_FROM_OBJECT(json_cmd_params, Key_userName);
            long refresh_time = JSON_GET_LONG_FROM_OBJECT(json_cmd_params, Key_refreshTime);
            json_bool force_login = JSON_GET_BOOL_FROM_OBJECT(json_cmd_params, Key_forceLogin);

            if (access_token && strlen(access_token) > 0) {
                if (g_spotifyStatus.ss_access_token) {
                    free(g_spotifyStatus.ss_access_token);
                    g_spotifyStatus.ss_access_token = NULL;
                }
                g_spotifyStatus.ss_request_token_cnt = 0;
                g_spotifyStatus.ss_refresh_time = tickSincePowerOn() + refresh_time;
                g_spotifyStatus.ss_access_token = strdup(access_token);
                int is_login = SpConnectionIsLoggedIn();
                if (is_login == 0 || force_login) {
                    if (is_login == 1) {
                        SpConnectionLogout();
                        g_spotifyStatus.ss_is_need_login = 1;
                    } else {
                        SpConnectionLoginOauthToken(access_token);
                    }
                    g_spotifyStatus.ss_is_avs_login = 1;
                }
            }
        }
    } else if (StrEq(cmd, Cmd_Logout)) {
        int is_login = SpConnectionIsLoggedIn();
        if (is_login) {
            SpConnectionLogout();
        }
        if (g_spotifyStatus.ss_access_token) {
            free(g_spotifyStatus.ss_access_token);
            g_spotifyStatus.ss_access_token = NULL;
        }
        if (g_spotifyStatus.ss_user_name) {
            free(g_spotifyStatus.ss_user_name);
            g_spotifyStatus.ss_user_name = NULL;
        }
        g_spotifyStatus.ss_refresh_time = 0;
        g_spotifyStatus.ss_request_token_cnt = 0;
    } else if (StrEq(cmd, Cmd_Play)) {
        err = SpotifyBeforeAvsAction(json_cmd_str);
        if (kSpErrorOk != err) {
            SPOTI_DEBUG("--SpotifyBeforeAvsAction (%s)error--\n", cmd);
        } else if (json_cmd_params) {
            char *correlation_token =
                JSON_GET_STRING_FROM_OBJECT(json_cmd_params, Key_correlationToken);
            char *play_uri = JSON_GET_STRING_FROM_OBJECT(json_cmd_params, Key_playbackContextToken);
            if (correlation_token) {
                if (g_spotifyStatus.ss_correlation_token) {
                    free(g_spotifyStatus.ss_correlation_token);
                    g_spotifyStatus.ss_correlation_token = NULL;
                }
                g_spotifyStatus.ss_correlation_token = strdup(correlation_token);
            }
            if (play_uri) {
                if (g_spotifyStatus.ss_access_token) {
                    struct SpZeroConfVars vars = {0};
                    err = SpZeroConfGetVars(&vars);
                    g_spotifyStatus.Notifycallbacks(sp_notify_playstatus, 0, 0);
                    start_request(g_spotifyStatus.ss_access_token, play_uri, vars.device_id);
                }
            }
        }
    } else if (StrEq(cmd, Cmd_TransportControl)) {
        err = SpotifyBeforeAvsAction(json_cmd_str);
        if (kSpErrorOk != err) {
            SPOTI_DEBUG("--SpotifyBeforeAvsAction (%s)error--\n", cmd);
        } else if (json_cmd_params) {
            char *action = JSON_GET_STRING_FROM_OBJECT(json_cmd_params, Key_action);
            long offset_ms = JSON_GET_LONG_FROM_OBJECT(json_cmd_params, Key_offsetms);

            if (StrEq(action, Val_Play)) {
                err = SpPlaybackPlay(-1);
            } else if (StrEq(action, Val_Next)) {
                err = SpPlaybackSkipToNext();
            } else if (StrEq(action, Val_Pause)) {
                err = SpPlaybackPause();
            } else if (StrEq(action, Val_Stop)) {
                err = SpPlaybackPause();
            } else if (StrEq(action, Val_Rewind)) {

            } else if (StrEq(action, Val_Favorite)) {

            } else if (StrEq(action, Val_FastForward)) {

            } else if (StrEq(action, Val_Previous)) {
                err = SpGetMetadata(&metadata, kSpMetadataTrackPrevious);
                if (kSpErrorOk != err || (!CheckMetadataValiad(metadata))) {
                    SpPlaybackSeek(0);
                } else {
                    err = SpPlaybackSkipToPrev();
                }
            } else if (StrEq(action, Val_StartOver)) {
                err = SpPlaybackSeek(0);
            } else if (StrEq(action, Val_UnFavorite)) {

            } else if (StrEq(action, Val_EnableRepeat)) {
                err = SpPlaybackEnableRepeat(1);
            } else if (StrEq(action, Val_EnableShuffle)) {
                err = SpPlaybackEnableShuffle(1);
            } else if (StrEq(action, Val_DisableRepeat)) {
                err = SpPlaybackEnableRepeat(0);
            } else if (StrEq(action, Val_EnableRepeatOne)) {
                err = SpPlaybackEnableRepeat(1);
            } else if (StrEq(action, Val_DisableShuffle)) {
                err = SpPlaybackEnableShuffle(0);
            } else if (StrEq(action, Val_SetSeekPosition)) {
                SPOTI_DEBUG("==== seek to %ld\n", offset_ms);
                err = SpPlaybackSeek(offset_ms);
            } else if (StrEq(action, Val_AdjustSeekPosition)) {
                long position_ms = offset_ms + SpPlaybackGetPosition();
                SPOTI_DEBUG("==== seek to %ld, position_ms=%ld\n", offset_ms, position_ms);
                position_ms = position_ms > 0 ? position_ms : 0;
                err = SpPlaybackSeek(position_ms);
            }
        }
    }

Exit:
    if (json_cmd) {
        json_object_put(json_cmd);
    }

    return err;
}
#endif

#ifdef BLE_NOTIFY_ENABLE
SpError SpotifyBLENotify(uint32_t position_ms)
{
    SpError err = kSpErrorFailed;
    char *event = NULL;

    Create_BLE_Cmd(event, position_ms);

    if (event && g_spotifyStatus.Notifycallbacks) {
        g_spotifyStatus.Notifycallbacks(sp_nofity_ble, (unsigned int)event, 0);
        err = kSpErrorOk;
    }

    if (event) {
        free(event);
    }

    return err;
}
#endif
int judge_device_is_group(void)
{
    int ret;
    if (g_wiimu_shm->slave_num > 0 || g_wiimu_shm->slave_notified)
        ret = 1;
    else
        ret = 0;
    return ret;
}
int SpotifyIntelUserCmd(SpotifyAction spCmd, int params)
{
    SpError err = kSpErrorOk;
    if (!g_spotifyStatus.init_ok) {
        return -1;
    }

    wiimu_log(1, 0, 0, 0, "SpotifyIntelUserCmd %d", spCmd);

#ifdef EN_AVS_SPOTIFY
    if (spCmd == AvsSpotifyAction) {
        err = SpotifyAvsCmdParse((char *)params);
    } else
#endif
        if (spCmd == SavePresetAction) {
        int preset_key = params;
        SPOTI_DEBUG("preset_key %d \r\n", preset_key);
        pthread_mutex_lock(&spotify_playlist_mutex);
#ifdef IHOME_ALEXA
        int preset_slot = getpresetindex(GetSpotifyCurrentSource(), 1);
#else
        int preset_slot = params;
#endif
        if (preset_slot >= 0 && preset_slot < PRESETS_NUM) {
            err = SpPresetSubscribe(preset_slot, g_preset_buffer, sizeof(g_preset_buffer));
            if (err == kSpErrorOk) {
                strncpy(preset_slots[preset_slot].preset_name, GetSpotifyCurrentSource(),
                        MAX_PRESET_NAME - 1);
                preset_slots[preset_slot].preset_name[MAX_PRESET_NAME - 1] = '\0';
                preset_slots[preset_slot].preset_key = params + 1;
                SPOTI_DEBUG("preset_slots[%d].preset_key %d \r\n", preset_slot,
                            preset_slots[preset_slot].preset_key);
            }
        }
        pthread_mutex_unlock(&spotify_playlist_mutex);
    } else if (spCmd == AddPlaylistAction) {
        pthread_mutex_lock(&spotify_playlist_mutex);
        int preset_slot = getpresetindex(GetSpotifyCurrentSource(), 0);
        if (preset_slot >= 0 && preset_slot < PRESETS_NUM) {
            err = SpPresetSubscribe(preset_slot, g_preset_buffer, sizeof(g_preset_buffer));
            if (err == kSpErrorOk) {
                SPOTI_DEBUG("%s: AddPlaylistAction playsource is %s\n ", __func__,
                            GetSpotifyCurrentSource());
                strncpy(preset_slots[preset_slot].preset_name, GetSpotifyCurrentSource(),
                        MAX_PRESET_NAME - 1);
                preset_slots[preset_slot].preset_name[MAX_PRESET_NAME - 1] = '\0';
            }
        } else if (preset_slot == -2) {
            err = kSpErrorOk;
        }
        pthread_mutex_unlock(&spotify_playlist_mutex);
    } else if (spCmd == PlayPlaylistAction) {
        struct Preset *preset = NULL;
        pthread_mutex_lock(&spotify_playlist_mutex);
        if (params >= 0 && params < PRESETS_NUM) {
            preset = LoadPreset(params);
        }

        if (preset) {
            preset->solt = params;
            if (!SpConnectionIsLoggedIn()) {
                err = SpConnectionLoginBlob(user_logInfo.username, user_logInfo.blob);
                if (err == kSpErrorOk && !g_wait_login) {
                    static pthread_t athread = 0;
                    pthread_attr_t attr;
                    pthread_attr_init(&attr);
                    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
                    struct ActionContext *action_context = malloc(sizeof(struct ActionContext));
                    action_context->spCmd = spCmd;
                    action_context->params = params;
                    g_wait_login = 1;
                    pthread_create(&athread, &attr, SpotifyWaitLoginThread, (void *)action_context);
                    pthread_attr_destroy(&attr);
                }
            } else {
                SPOTI_DEBUG("SpPlayPreset ++ \r\n");
                err = SpPlayPreset(params, preset->pos, preset->blob, preset->blob_length, -1);
                SPOTI_DEBUG("SpPlayPreset err %d \r\n", err);
            }
        }
        pthread_mutex_unlock(&spotify_playlist_mutex);
    } else if (spCmd == PlayPresetAction) {
        struct Preset *preset = NULL;
        int solt = -1;

        pthread_mutex_lock(&spotify_playlist_mutex);
        if (params >= 0 && params < PRESETS_NUM) {
            solt = getPresetIndexBykey(params);
            if (solt >= 0)
                preset = LoadPreset(solt);
        }

        if (preset) {
            preset->solt = solt;
            if (!SpConnectionIsLoggedIn()) {
                err = SpConnectionLoginBlob(user_logInfo.username, user_logInfo.blob);
                if (err == kSpErrorOk && !g_wait_login) {
                    static pthread_t athread = 0;
                    pthread_attr_t attr;
                    pthread_attr_init(&attr);
                    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
                    struct ActionContext *action_context = malloc(sizeof(struct ActionContext));
                    action_context->spCmd = spCmd;
                    action_context->params = params;
                    g_wait_login = 1;
                    pthread_create(&athread, &attr, SpotifyWaitLoginThread, (void *)action_context);
                    pthread_attr_destroy(&attr);
                }
            } else {
                SPOTI_DEBUG("SpPlayPreset ++ \r\n");
                err = SpPlayPreset(solt, preset->pos, preset->blob, preset->blob_length, -1);
                SPOTI_DEBUG("SpPlayPreset err %d \r\n", err);
            }
        }
        pthread_mutex_unlock(&spotify_playlist_mutex);

    } else if (spCmd == QuitAction) {
        err = SpConnectionLogout();
    } else if (spCmd == ConnenctAction) {
        if (params == 1) {
            err = SpConnectionSetConnectivity(kSpConnectivityWireless);
        } else {
            err = SpConnectionSetConnectivity(kSpConnectivityNone);
        }
    } else if (spCmd == SetNameAction) {
#ifdef WMRMPLAY_ENABLE
        if (judge_device_is_group() == 1) {
            SpSetDeviceIsGroup(1);
            if (strlen(g_wiimu_shm->group_name) > 0)
                SpSetDisplayName((const char *)g_wiimu_shm->group_name);
        } else if (judge_device_is_group() == 0) {
            SpSetDeviceIsGroup(0);
            if (strlen(g_wiimu_shm->device_name) > 0)
                SpSetDisplayName((const char *)g_wiimu_shm->device_name);
        }
#else
        err = SpSetDisplayName((const char *)params);
#endif
    } else if (SpPlaybackIsActiveDevice() == 0) {
        if (spCmd == PlayPauseAction || spCmd == PlaybackAction) {
            g_spotifyStatus.Notifycallbacks(sp_nofity_playctrl, 0, 0);
            err = SpPlaybackPlay(-1);
            audio_cb.audio_pause(0);

        } else {
            err = kSpErrorFailed;
        }
    } else if (spCmd == PlayPauseAction) {
        if (g_spotifyStatus.ss_play_status == 1) {
            g_spotifyStatus.Notifycallbacks(sp_nofity_playctrl, 1, 0);
            err = SpPlaybackPause();
            audio_cb.audio_pause(1);
            g_spotifyStatus.ss_play_status = 0;
        } else {
            g_spotifyStatus.Notifycallbacks(sp_nofity_playctrl, 0, 0);
            err = SpPlaybackPlay(-1);
            audio_cb.audio_pause(0);
            g_spotifyStatus.ss_play_status = 1;
        }
    } else if (spCmd == PlaybackAction) {
        g_spotifyStatus.Notifycallbacks(sp_nofity_playctrl, 0, 0);
        err = SpPlaybackPlay(-1);
        audio_cb.audio_pause(0);
        g_spotifyStatus.ss_play_status = 1;
    } else if (spCmd == PauseAction) {
        g_spotifyStatus.Notifycallbacks(sp_nofity_playctrl, 1, 0);
        err = SpPlaybackPause();
        audio_cb.audio_pause(1);
        g_spotifyStatus.ss_play_status = 0;
    } else if (spCmd == ShuffleAction) {
        if (params != (int)SpPlaybackIsShuffled()) {
            err = SpPlaybackEnableShuffle(params);
        }
    } else if (spCmd == RepeatAction) {
        if (params != (int)SpPlaybackGetRepeatMode()) {
            err = SpPlaybackEnableRepeat(params);
        }
    } else if (spCmd == BitrateAction) {
        err = SpPlaybackSetBitrate(params);
    } else if (spCmd == PreviousAction) {
        uint32_t postion = SpPlaybackGetPosition();
        if (postion >= 3000) {
            err = SpPlaybackSeek(0);
        } else {
            err = SpPlaybackSkipToPrev();
        }
    } else if (spCmd == NextAction)
        err = SpPlaybackSkipToNext();
    else if (spCmd == VolumeSetAction) {
        params = params * 65535 / 100;
        g_spotifyStatus.ss_volume = params;
        err = SpPlaybackUpdateVolume(params);
    } else if (spCmd == SeekAction) {
        err = SpPlaybackSeek(params);
    } else if (spCmd == StopAction) {
        err = SpPlaybackPause();
        SpPlaybackSetRedeliveryMode(kSpRedeliveryModeActivated);
        audio_cb.audio_deinit();
        g_spotifyStatus.pause_switch = 1;
    } else if (spCmd == logoutAction) {
        audio_cb.audio_deinit();
        err = SpConnectionLogout();
    } else if (spCmd == RelativeSeekAction) {
        long position_ms = SpPlaybackGetPosition() + params;
        SPOTI_DEBUG("==== seek to %d, position_ms=%ld\n", params, position_ms);
        position_ms = position_ms > 0 ? position_ms : 0;
        err = SpPlaybackSeek(position_ms);
    }

    wiimu_log(1, 0, 0, 0, "SpotifyUserCmd return err = %d", err);

    return err;
}

int SpotifyUserCmd(SpotifyAction spCmd, unsigned int params)
{
    sp_cmd_action *cmd_action = NULL;

    pthread_mutex_lock(&mutex_cmd_list);
    if (lp_list_number(&sp_cmd_list) > 10) {
        SPOTI_DEBUG("sp_cmd_list queue full \r\n");
        goto end;
    }
    cmd_action = (sp_cmd_action *)malloc(sizeof(sp_cmd_action));
    memset(cmd_action, 0x0, sizeof(sp_cmd_action));
    cmd_action->spCmd = spCmd;
    if (cmd_action->spCmd == SetNameAction || cmd_action->spCmd == AvsSpotifyAction) {
        cmd_action->params = (int)strdup((char *)params);
    } else {
        cmd_action->params = params;
    }
    lp_list_add_tail(&cmd_action->entry, &sp_cmd_list);

end:
    pthread_mutex_unlock(&mutex_cmd_list);

    return 0;
}

sp_cmd_action *GetVaildSpCmdAction(void)
{
    struct lp_list_head *pos = NULL;
    sp_cmd_action *cmd_action = NULL;

    pthread_mutex_lock(&mutex_cmd_list);

    if (lp_list_number(&sp_cmd_list) > 0) {
        lp_list_for_each(pos, &sp_cmd_list)
        {
            cmd_action = lp_list_entry(pos, sp_cmd_action, entry);
            lp_list_del(&cmd_action->entry);
            break;
        }
    }
    pthread_mutex_unlock(&mutex_cmd_list);

    return cmd_action;
}

static void *SpotifyMainLoop(void *context)
{
    int err = 0;
    long long cur_time = 0;
    long long time_start = 0;
    uint32_t old_postion = 0;
    uint32_t old_duration = 0;
    int metadata_changed = 0;
    struct SpMetadataOut MetadataOut;
    struct SpotifyStatus *pSpotifyStatus = (struct SpotifyStatus *)context;
    struct SpZeroConfVars zc_vars = {0};

    SpZeroConfGetVars(&zc_vars);
    SPOTI_DEBUG("libraryVersion %s \r\n", zc_vars.library_version);
    while (1) {
        err = SpPumpEvents();
        if (kSpErrorOk != err) {
            SPOTI_DEBUG("SpPumpEvents %d \r\n", err);
            break;
        }
#ifdef WMRMPLAY_ENABLE
        if (judge_device_is_group() && !pSpotifyStatus->grouped) {
            SpSetDeviceIsGroup(1);
            if (strlen(g_wiimu_shm->group_name) > 0)
                SpSetDisplayName((const char *)g_wiimu_shm
                                     ->group_name); // Replace the group name with deviceName ...
            pSpotifyStatus->grouped = 1;
        } else if (!judge_device_is_group() && pSpotifyStatus->grouped) {
            SpSetDeviceIsGroup(0);
            if (strlen(g_wiimu_shm->device_name) > 0)
                SpSetDisplayName((const char *)g_wiimu_shm->device_name); // after unplug slave unit
            pSpotifyStatus->grouped = 0;
        }
#endif

#ifdef EN_AVS_SPOTIFY
        avs_notify_t *avs_notify = GetAvsNotify();
        if (avs_notify) {
            if (avs_notify->event >= 0) {
                if (avs_notify->params == 1) {
                    SpotifyAvsConnectionNotify((enum SpConnectionNotification)avs_notify->event);
                } else if (avs_notify->params == 0) {
                    SpotifyAvsPlaybackNotify((enum SpPlaybackNotification)avs_notify->event);
                }
            }
            free(avs_notify);
        }
#endif
        sp_cmd_action *cmd_action = GetVaildSpCmdAction();
        if (cmd_action) {
            SpotifyIntelUserCmd(cmd_action->spCmd, cmd_action->params);
            if (cmd_action->spCmd == SetNameAction || cmd_action->spCmd == AvsSpotifyAction) {
                free((void *)cmd_action->params);
            } else if (cmd_action->spCmd == QuitAction) {
                break;
            }
            free(cmd_action);
        }

#if defined(SPTOFY_CERTIFICATION) || defined(EN_AVS_SPOTIFY)
        if (pSpotifyStatus->ss_first_group_volume) {
            pSpotifyStatus->ss_first_group_volume = 0;
            int volume = g_wiimu_shm->volume;
            if (volume > 50) {
                pSpotifyStatus->ss_volume = 65535 * 50 / 100;
                CStandaloneNode_SetVol(50, VOL_SPOTIFY, 0);
            } else {
                pSpotifyStatus->ss_volume = g_wiimu_shm->volume * 65535 / 100;
            }

            SpPlaybackUpdateVolume(pSpotifyStatus->ss_volume);
        }
#endif

        cur_time = tickSincePowerOn();
        if (cur_time - time_start >= 1000L) {
            time_start = tickSincePowerOn();
            metadata_changed = 0;

            // sp disconnects and reconnects, the music maybe stops, need to resume
            if (sp_conn == SP_RECON) {
                if (!SpPlaybackIsPlaying()) {
                    SpPlaybackPlay(-1);
                    SPOTI_DEBUG("Resume playing when reconnected\n");
                }
                sp_conn = SP_NONE;
            }

            if (pSpotifyStatus->ss_active_status) {
                uint32_t cur_postion = 0;
                cur_postion = SpPlaybackGetPosition();
                if (cur_postion != old_postion ||
                    old_duration != pSpotifyStatus->ss_track.duration_ms) {
                    old_postion = cur_postion;
                    old_duration = pSpotifyStatus->ss_track.duration_ms;
                    if (pSpotifyStatus->Notifycallbacks)
                        pSpotifyStatus->Notifycallbacks(sp_notify_postion, old_postion,
                                                        old_duration);
                }

                if (SpPlaybackIsPlaying()) {
                    if (SpPlaybackIsRedeliveryModeActivated() == kSpRedeliveryModeActivated) {
                        SpPlaybackSetRedeliveryMode(kSpRedeliveryModeDeactivated);
                    }
                }

                if (pSpotifyStatus->ss_track_metadata_change) {
                    metadata_changed = 1;
                    pSpotifyStatus->ss_track_metadata_change = 0;
                    memset(&MetadataOut, 0x0, sizeof(struct SpMetadataOut));
                    memcpy(MetadataOut.playback_source, pSpotifyStatus->ss_track.playback_source,
                           SP_MAX_METADATA_NAME_LENGTH + 1);
                    memcpy(MetadataOut.playback_source_uri,
                           pSpotifyStatus->ss_track.playback_source_uri,
                           SP_MAX_METADATA_URI_LENGTH + 1);
                    memcpy(MetadataOut.track, pSpotifyStatus->ss_track.track,
                           SP_MAX_METADATA_NAME_LENGTH + 1);
                    memcpy(MetadataOut.track_uri, pSpotifyStatus->ss_track.track_uri,
                           SP_MAX_METADATA_URI_LENGTH + 1);
                    memcpy(MetadataOut.artist, pSpotifyStatus->ss_track.artist,
                           SP_MAX_METADATA_NAME_LENGTH + 1);
                    memcpy(MetadataOut.album, pSpotifyStatus->ss_track.album,
                           SP_MAX_METADATA_NAME_LENGTH + 1);
                    MetadataOut.duration_ms = pSpotifyStatus->ss_track.duration_ms;
                    MetadataOut.playback_restrictions = 0;

                    if (pSpotifyStatus->ss_track.playback_restrictions.disallow_seeking_reasons &
                        (SP_PLAYBACK_RESTRICTION_LICENSE_DISALLOW |
                         SP_PLAYBACK_RESTRICTION_AD_DISALLOW)) {
                        MetadataOut.playback_restrictions |= 1 << 0;
                    }

                    if (pSpotifyStatus->ss_track.playback_restrictions
                            .disallow_skipping_prev_reasons &
                        (SP_PLAYBACK_RESTRICTION_LICENSE_DISALLOW |
                         SP_PLAYBACK_RESTRICTION_AD_DISALLOW)) {
                        MetadataOut.playback_restrictions |= 1 << 1;
                    }

                    if (pSpotifyStatus->ss_track.playback_restrictions
                            .disallow_skipping_next_reasons &
                        (SP_PLAYBACK_RESTRICTION_LICENSE_DISALLOW |
                         SP_PLAYBACK_RESTRICTION_AD_DISALLOW)) {
                        MetadataOut.playback_restrictions |= 1 << 2;
                    }

                    if ((pSpotifyStatus->ss_track.playback_restrictions
                             .disallow_toggling_repeat_context_reasons &
                         (SP_PLAYBACK_RESTRICTION_LICENSE_DISALLOW |
                          SP_PLAYBACK_RESTRICTION_AD_DISALLOW)) ||
                        (pSpotifyStatus->ss_track.playback_restrictions
                             .disallow_toggling_repeat_track_reasons &
                         (SP_PLAYBACK_RESTRICTION_LICENSE_DISALLOW |
                          SP_PLAYBACK_RESTRICTION_AD_DISALLOW))) {
                        MetadataOut.playback_restrictions |= 1 << 3;
                    }

                    if (pSpotifyStatus->ss_track.playback_restrictions
                            .disallow_toggling_shuffle_reasons &
                        (SP_PLAYBACK_RESTRICTION_LICENSE_DISALLOW |
                         SP_PLAYBACK_RESTRICTION_AD_DISALLOW)) {
                        MetadataOut.playback_restrictions |= 1 << 4;
                    }

                    if (strlen(pSpotifyStatus->ss_track.album_cover_uri) > 0) {
                        SpGetMetadataImageURL(pSpotifyStatus->ss_track.album_cover_uri,
                                              MetadataOut.album_cover_uri,
                                              sizeof(MetadataOut.album_cover_uri));
                    }
                }

                if (metadata_changed) {
                    pSpotifyStatus->Notifycallbacks(sp_notify_metadata, (unsigned int)&MetadataOut,
                                                    0);
                }

                pSpotifyStatus->Notifycallbacks(sp_nofity_loopmode, SpPlaybackIsShuffled(),
                                                SpPlaybackGetRepeatMode());
            }

#ifdef EN_AVS_SPOTIFY
            int is_logged = SpConnectionIsLoggedIn();
            static int crash = 0;
            int need_to_login = 0;
            if (((((!pSpotifyStatus->ss_access_token) || (is_logged == 0)) ||
                  (pSpotifyStatus->ss_access_token && (is_logged == 1))) &&
                 (pSpotifyStatus->ss_request_token_cnt < Retry_login_cnt) &&
                 pSpotifyStatus->ss_avs_online == 1 &&
                 (pSpotifyStatus->ss_refresh_time > 0 &&
                  cur_time - pSpotifyStatus->ss_refresh_time >= 0)))
                need_to_login = 1;

            if (g_wiimu_shm->a01controller_crash > 0 && crash != g_wiimu_shm->a01controller_crash) {
                crash = g_wiimu_shm->a01controller_crash;
                need_to_login = 1;
                wiimu_log(1, 0, 0, 0, "%s need to login because a01 crash %d", __FUNCTION__, crash);
            }

            if (need_to_login) {
                if (is_logged) {
                    if (pSpotifyStatus->ss_access_token) {
                        free(pSpotifyStatus->ss_access_token);
                        pSpotifyStatus->ss_access_token = NULL;
                    }
                }
                err = SpotifyAvsEventNotify(Event_RequestToken, NULL, 0);
                pSpotifyStatus->ss_request_token_cnt++;
                int retry_delay = SpotifyGetRetryDelay(pSpotifyStatus->ss_request_token_cnt);
                pSpotifyStatus->ss_refresh_time = tickSincePowerOn() + retry_delay;
            } else if (pSpotifyStatus->ss_access_token && is_logged == 1 &&
                       pSpotifyStatus->ss_request_token_cnt > 0) {
                pSpotifyStatus->ss_request_token_cnt = 0;
            }
#endif
        }
    }

    SpFree();

    return NULL;
}

void StrcpyCheckSpecial(char *dest, char *src)
{
    char c;
    do {
        c = *src++;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            (c == '_') || (c == '-') || (c == '.')) {
            *dest++ = c;
        }
    } while (c != '\0');

    *dest = '\0';
}

int SpotifyConnectInit(const char *unique_id, const char *dname,
                       SpNotifyUserCallBack pSpNotifycallbacks, int device_type_input,
                       char *modelname, char *brandname, char *modeldisplayname,
                       char *branddisplayname)
{
    struct SpConfig conf = {0};
    int err = 0;

    static char spotify_modle_name[64] = {0};
    static char spotify_brand_name[64] = {0};

    uint8_t *pmemory_block = NULL;
    int web = 5356;
    int device_type = kSpDeviceTypeAudioDongle;

    int buffer_size = SP_RECOMMENDED_MEMORY_BLOCK_SIZE * 4;
    struct SpConnectionCallbacks connection_callbacks;
    struct SpDebugCallbacks debug_callbacks;
    struct SpPlaybackCallbacks playback_callbacks;
#if defined(SPTOFY_CERTIFICATION)
    SPOTI_DEBUG("Spotify Certification enable \r\n");
#endif

    SPOTI_DEBUG("SpotifyConnectInit ++ %s %s %s %s %s\r\n", dname, modelname, brandname,
                modeldisplayname, branddisplayname);
    SpExampleAudioGetCallbacks(&audio_cb);

    memset(&g_spotifyStatus, 0x0, sizeof(struct SpotifyStatus));
    g_spotifyStatus.Notifycallbacks = pSpNotifycallbacks;

    pthread_mutex_init(&spotify_playlist_mutex, NULL);
    if (device_type_input == 1) {
        device_type = kSpDeviceTypeSpeaker;
    }

    StrcpyCheckSpecial(spotify_modle_name, modelname);
    StrcpyCheckSpecial(spotify_brand_name, brandname);

    strcpy(g_spotify_modle_name, spotify_modle_name);
    strcpy(g_spotify_brand_name, spotify_brand_name);

    SPOTI_DEBUG("spotify_modle_name %s \r\n", spotify_modle_name);
    SPOTI_DEBUG("spotify_brand_name %s \r\n", spotify_brand_name);

    memset(&connection_callbacks, 0, sizeof(connection_callbacks));
    connection_callbacks.on_notify = CallbackConnectionNotify;
    connection_callbacks.on_message = CallbackConnectionMessage;
    connection_callbacks.on_new_credentials = CallbackNewCredentials;

    memset(&debug_callbacks, 0, sizeof(debug_callbacks));
    debug_callbacks.on_message = CallbackDebugMessage;

    memset(&playback_callbacks, 0, sizeof(playback_callbacks));
    playback_callbacks.on_notify = CallbackPlaybackNotify;
    playback_callbacks.on_audio_data = audio_cb.audio_data;
    playback_callbacks.on_seek = CallbackPlaybackSeek;
    playback_callbacks.on_apply_volume = CallbackPlaybackApplyVolume;
    playback_callbacks.on_save_preset = CallbackSavePreset;

    pmemory_block = (uint8_t *)malloc(buffer_size);
    if (pmemory_block == NULL) {
        SPOTI_DEBUG("Invalid buffer !!\n");
        err = -1;
        goto end;
    }

    conf.api_version = SP_API_VERSION;
    conf.memory_block = pmemory_block;
    conf.memory_block_size = buffer_size;
    conf.client_id = clientid;
#if defined(A98_MARSHALL_STEPIN)
    conf.product_id = 3;
#elif defined(MARSHALL_STANMORE_AVS_TBD_BETA) || defined(A98_TEUFEL) || defined(A98_MOXIE2_256M) || defined(A98YOUYISHI_LV_CLUTCH)
    conf.product_id = 2;
#else
    conf.product_id = 1;
#endif
    conf.error_callback = CallbackError;
    conf.error_callback_context = &g_spotifyStatus;
    conf.display_name = dname;
    conf.unique_id = unique_id;
    conf.brand_name = g_spotify_brand_name;
    conf.model_name = g_spotify_modle_name;

    strncpy(spotify_modle_name, modeldisplayname, sizeof(spotify_modle_name) - 1);
    strncpy(spotify_brand_name, branddisplayname, sizeof(spotify_brand_name) - 1);

    conf.brand_display_name = spotify_brand_name;
    conf.model_display_name = spotify_modle_name;

    conf.device_type = (enum SpDeviceType)device_type;
    conf.zeroconf_serve = SP_ZEROCONF_SERVE;
    conf.zeroconf_port = web;
    conf.zeroconf_port_range = 100;
    conf.scope = SP_SCOPE_STREAMING;
    conf.host_name = unique_id;
#ifdef ASR_ALEXA2
    conf.global_attributes = SP_GLOBAL_ATTRIBUTE_VOICE;
#endif

    SpRegisterDebugCallbacks(&debug_callbacks, &g_spotifyStatus);

    g_spotifyStatus.ss_unique_id = (char *)unique_id;
    g_spotifyStatus.ss_user_name = NULL;
    if (kSpErrorOk != (err = SpInit(&conf)))
        goto end;

    loadpresetformflash();
    loaduserinfoformflash();
    loadactionlistformflash();

    SpRegisterConnectionCallbacks(&connection_callbacks, &g_spotifyStatus);
    SpRegisterPlaybackCallbacks(&playback_callbacks, &g_spotifyStatus);

    g_spotifyStatus.init_ok = 1;
    SpInitLoginEvent();

    if (strlen(user_logInfo.username) > 0 && strlen(user_logInfo.blob) > 0) {
        SPOTI_DEBUG("Spotify boot autologin \r\n");
        SpConnectionLoginBlob(user_logInfo.username, user_logInfo.blob);
        g_spotifyStatus.is_auto_login = 1;
// default set mode, notify to MCU
#if defined(SPOTIFY_NOTIFY_DISABLE)
        g_spotifyStatus.Notifycallbacks(
            sp_nofity_login, 0,
            0); // default:1,1, param1:0 do not notify to mcu,param2:1 set spotify mode
#else
        g_spotifyStatus.Notifycallbacks(
            sp_nofity_login, 1,
            1); // default:1,1, param1:0 do not notify to mcu,param2:1 set spotify mode
#endif
    }

    SpotifyMainLoop(&g_spotifyStatus);

    SPOTI_DEBUG("SpotifyConnectInit -- \r\n");
    return 0;

end:
    SPOTI_DEBUG("Exiting with error = %d\n", err);
    SpFree();

    if (pmemory_block)
        free(pmemory_block);

    return err;
}

static void CallbackError(SpError error, void *context)
{
    SPOTI_DEBUG("Spotify Error: %d\n", error);
    // If a login was initiated by the webserver, inform it that the login failed.
    // (We inform in any case, the webserver will ignore this if it is not
    // actually waiting for a login.)
    // SpExampleWebserverReportLoginStatus(error,1);
    switch (error) {
    case kSpErrorOk:

        break;
    case kSpErrorInitFailed:

        break;
    case kSpErrorWrongAPIVersion:
    case kSpErrorApplicationBanned:

        break;
    case kSpErrorUnsupported:
    case kSpErrorNullArgument:
    case kSpErrorInvalidArgument:
    case kSpErrorUninitialized:
    case kSpErrorAlreadyInitialized:

        break;
    case kSpErrorLoginBadCredentials:
    case kSpErrorTravelRestriction:
    case kSpErrorGeneralLoginError:

        break;
    default:
        break;
    }

#ifdef EN_AVS_SPOTIFY
    if (kSpErrorOk != error) {
        SpotifyAvsErrorNotify(NULL, error, NULL, 0);
    }
#endif
}

#ifdef EN_AVS_SPOTIFY
void SpotifyAvsConnectionNotify(enum SpConnectionNotification event)
{
    switch (event) {
    case kSpConnectionNotifyLoggedIn: {
        const char *cano_user_name = SpGetCanonicalUsername();
        if (g_spotifyStatus.ss_is_avs_login) {
            g_spotifyStatus.ss_is_avs_login = 0;
            if (cano_user_name) {
                if (g_spotifyStatus.ss_user_name &&
                    strcmp(g_spotifyStatus.ss_user_name, cano_user_name)) {
                    free(g_spotifyStatus.ss_user_name);
                    g_spotifyStatus.ss_user_name = NULL;
                    g_spotifyStatus.ss_user_name = strdup(cano_user_name);
                } else if (NULL == g_spotifyStatus.ss_user_name) {
                    g_spotifyStatus.ss_user_name = strdup(cano_user_name);
                }
            }
            SpotifyAvsEventNotify(Event_Login, NULL, 1);
            if (g_spotifyStatus.ss_backup_cmd) {
                SpotifyAvsCmdParse(g_spotifyStatus.ss_backup_cmd);
                free(g_spotifyStatus.ss_backup_cmd);
                g_spotifyStatus.ss_backup_cmd = NULL;
            }
        } else {
            if (g_spotifyStatus.ss_backup_cmd) {
                g_spotifyStatus.ss_backup_cmd = NULL;
            }
            if (g_spotifyStatus.is_auto_login ||
                (cano_user_name && g_spotifyStatus.ss_user_name &&
                 !strcmp(g_spotifyStatus.ss_user_name, cano_user_name))) {
                g_spotifyStatus.is_auto_login = 0;
                SpotifyAvsEventNotify(Event_Login, NULL, 1);
            } else {
                SpotifyAvsEventNotify(Event_Login, NULL, 0);
            }
        }
        SPOTI_DEBUG("Spotify Logged in successfully status(%d) is_auto_login(%d) ss_user_name(%s) "
                    "user_name(%s)\n",
                    SpConnectionIsLoggedIn(), g_spotifyStatus.is_auto_login,
                    g_spotifyStatus.ss_user_name, cano_user_name);
    } break;
    case kSpConnectionNotifyLoggedOut:
        SpotifyAvsEventNotify(Event_Logout, NULL, 0);
        if (g_spotifyStatus.ss_is_need_login == 1) {
            g_spotifyStatus.ss_is_need_login = 0;
            if (g_spotifyStatus.ss_is_avs_login == 1) {
                SpConnectionLoginOauthToken(g_spotifyStatus.ss_access_token);
            }
        }
        break;
    case kSpConnectionNotifyDisconnect:
        SpotifyAvsEventNotify(Event_PlayerEvent, Val_Disconnect, 0);
        break;
    case kSpConnectionNotifyReconnect:
        SpotifyAvsEventNotify(Event_PlayerEvent, Val_Reconnect, 0);
        break;
    case kSpConnectionNotifyTemporaryError:
        SpotifyAvsEventNotify(Event_PlayerEvent, Val_TemporaryError, 0);
        break;
    default:
        break;
    }
}
#endif

static void CallbackConnectionNotify(enum SpConnectionNotification event, void *context)
{
    switch (event) {
    case kSpConnectionNotifyLoggedIn:
        SPOTI_DEBUG("Spotify Logged in successfully status(%d)\n", SpConnectionIsLoggedIn());
        // If a login was initiated by the webserver, inform it that the login succeeded.
        // (We inform in any case, the webserver will ignore this if it is not
        // actually waiting for a login.)
        SpExampleWebserverReportLoginStatus(kSpErrorOk, 1);
        break;
    case kSpConnectionNotifyLoggedOut:
        SPOTI_DEBUG("Spotify Logged out\n");
        break;
    case kSpConnectionNotifyDisconnect:
        SPOTI_DEBUG("Spotify Disconnect\n");
        SPOTI_DEBUG("login status: %d\n", SpConnectionIsLoggedIn());
        SPOTI_DEBUG("active status: %d\n", SpPlaybackIsActiveDevice());
        SPOTI_DEBUG("playback status: %d\n", SpPlaybackIsPlaying());
        if (SpConnectionIsLoggedIn() && SpPlaybackIsActiveDevice() && SpPlaybackIsPlaying()) {
            sp_conn = SP_DISC;
        }
        SPOTI_DEBUG("Sp Connect status: %d\n", sp_conn);
        break;
    case kSpConnectionNotifyReconnect:
        SPOTI_DEBUG("Spotify Reconnect\n");
        if (sp_conn == SP_DISC) {
            sp_conn = SP_RECON;
        }
        SPOTI_DEBUG("Sp Connect status: %d\n", sp_conn);
        break;
    case kSpConnectionNotifyTemporaryError:
        SPOTI_DEBUG("kSpConnectionNotifyTemporaryError\n");
        SpExampleWebserverReportLoginStatus(kSpErrorFailed, 1);
        break;
    default:
        break;
    }
#ifdef EN_AVS_SPOTIFY
    SpotifyAddAvsNotify((int)event, 1);
#endif
}

static void CallbackConnectionMessage(const char *message, void *context)
{
    wiimu_log(1, 0, 0, 0, "SPOTIFY CONNECTION MESSAGE: %s\n", message);
}

#ifdef EN_AVS_SPOTIFY
void SpotifyAvsPlaybackNotify(enum SpPlaybackNotification event)
{
    SPOTI_DEBUG("SpotifyAvsPlaybackNotify: %d\n", event);
    switch (event) {
    case kSpPlaybackNotifyPlay:
        SpotifyAvsEventNotify(Event_PlayerEvent, Val_PlaybackStarted, 0);
        break;
    case kSpPlaybackNotifyPause:
        SpotifyAvsEventNotify(Event_PlayerEvent, Val_PlaybackStopped, 0);
        break;
    case kSpPlaybackNotifyTrackChanged:
        SpotifyAvsEventNotify(Event_PlayerEvent, Val_TrackChanged, 0);
        break;
    case kSpPlaybackNotifyNext:
        SpotifyAvsEventNotify(Event_PlayerEvent, Val_Next, 0);
        break;
    case kSpPlaybackNotifyPrev:
        SpotifyAvsEventNotify(Event_PlayerEvent, Val_Previous, 0);
        break;
    case kSpPlaybackNotifyShuffleOn:
        SpotifyAvsEventNotify(Event_PlayerEvent, Val_ShuffleOn, 0);
        break;
    case kSpPlaybackNotifyShuffleOff:
        SpotifyAvsEventNotify(Event_PlayerEvent, Val_ShuffleOff, 0);
        break;
    case kSpPlaybackNotifyRepeatOn:
        SpotifyAvsEventNotify(Event_PlayerEvent, Val_RepeatOn, 0);
        break;
    case kSpPlaybackNotifyRepeatOff:
        SpotifyAvsEventNotify(Event_PlayerEvent, Val_RepeatOff, 0);
        break;
    case kSpPlaybackNotifyBecameActive:
        SpotifyAvsEventNotify(Event_PlayerEvent, Val_BecameActive, 0);
        break;
    case kSpPlaybackNotifyBecameInactive:
        SpotifyAvsEventNotify(Event_PlayerEvent, Val_BecameInactive, 0);
        break;
    case kSpPlaybackNotifyLostPermission:
        SpotifyAvsEventNotify(Event_PlayerEvent, Val_PlaybackFailed, 0);
        break;
    case kSpPlaybackEventAudioFlush:
        SpotifyAvsEventNotify(Event_PlayerEvent, Val_AudioFlush, 0);
        break;
    case kSpPlaybackNotifyAudioDeliveryDone:
        SpotifyAvsEventNotify(Event_PlayerEvent, Val_PlaybackFinished, 0);
        break;
    default:
        break;
    }
}
#endif

static void CallbackPlaybackNotify(enum SpPlaybackNotification event, void *context)
{
#ifdef EN_AVS_SPOTIFY
    int notify_enent = 0;
#endif
    struct SpotifyStatus *pSpotifyStatus = (struct SpotifyStatus *)context;

    switch (event) {
    case kSpPlaybackNotifyPlay:
        SPOTI_DEBUG("Playback status: playing\n");
        if (pSpotifyStatus->ss_active_status) {
            SPOTI_DEBUG("CallbackPlaybackNotify playing at %lld ms \n", tickSincePowerOn());
            if (pSpotifyStatus->pause_switch) {
                pSpotifyStatus->pause_switch = 0;
                memset(&pSpotifyStatus->ss_track, 0, sizeof(struct SpMetadata));
                if (SpGetMetadata(&pSpotifyStatus->ss_track, kSpMetadataTrackCurrent) ==
                    kSpErrorOk) {
                    pSpotifyStatus->ss_track_metadata_change = 1;
                }
            }
            pSpotifyStatus->Notifycallbacks(sp_notify_playstatus, 0, 0);
            if (!pSpotifyStatus->ss_play_status)
                audio_cb.audio_pause(0);
        }
        pSpotifyStatus->ss_play_status = 1;
        break;
    case kSpPlaybackNotifyPause:
        SPOTI_DEBUG("Playback status: pause\n");
        if (pSpotifyStatus->ss_active_status) {
            if (pSpotifyStatus->ss_play_status) {
                audio_cb.audio_pause(1);
#ifdef EN_AVS_SPOTIFY
                notify_enent = 1;
#endif
            }
            pSpotifyStatus->Notifycallbacks(sp_notify_playstatus, 1, 0);
        }
        pSpotifyStatus->ss_play_status = 0;
        break;
    case kSpPlaybackNotifyTrackChanged:
        SPOTI_DEBUG("kSpPlaybackNotifyTrackChanged \r\n");
#if defined(EN_PLAYCTRL_NOTIFY_SPOTIFY)
        if (pSpotifyStatus->ss_active_status) {
            pSpotifyStatus->Notifycallbacks(sp_nofity_playctrl, 0, 0);
        }
#endif
        break;
    case kSpPlaybackNotifyMetadataChanged: {
        if (pSpotifyStatus->ss_active_status) {
            memset(&pSpotifyStatus->ss_track, 0, sizeof(struct SpMetadata));
            if (SpGetMetadata(&pSpotifyStatus->ss_track, kSpMetadataTrackCurrent) == kSpErrorOk) {
                pSpotifyStatus->ss_track_metadata_change = 1;
            }
        }
    } break;
    case kSpPlaybackNotifyNext:
        SPOTI_DEBUG("kSpPlaybackNotifyNext \r\n");
        audio_cb.audio_flush();
        break;
    case kSpPlaybackNotifyPrev:
        SPOTI_DEBUG("kSpPlaybackNotifyPrev \r\n");
        audio_cb.audio_flush();
        break;
    case kSpPlaybackNotifyShuffleOn:
        SPOTI_DEBUG("Shuffle status: 1\n");
        break;
    case kSpPlaybackNotifyShuffleOff:
        SPOTI_DEBUG("Shuffle status: 0\n");
        break;
    case kSpPlaybackNotifyRepeatOn:
        SPOTI_DEBUG("Repeat status: 1\n");
        break;
    case kSpPlaybackNotifyRepeatOff:
        SPOTI_DEBUG("Repeat status: 0\n");
        break;
    case kSpPlaybackNotifyBecameActive:
        SPOTI_DEBUG("Became active speaker\n");
        pSpotifyStatus->ss_active_status = 1;
        g_spotify_active = 1;
        pSpotifyStatus->ss_play_status = 0;
        pSpotifyStatus->ss_first_group_volume = 1;
        pSpotifyStatus->pause_switch = 0;
        pSpotifyStatus->Notifycallbacks(sp_notify_active, 1, 0);
        // sdk 3.15a default is pause
        if (pSpotifyStatus->ss_play_status) {
            audio_cb.audio_pause(0);
            pSpotifyStatus->Notifycallbacks(sp_notify_playstatus, 0, 0);
        } else {
            audio_cb.audio_pause(1);
            pSpotifyStatus->Notifycallbacks(sp_notify_playstatus, 1, 0);
        }
        break;
    case kSpPlaybackNotifyBecameInactive:
        SPOTI_DEBUG("Not active speaker anymore\n");
        pSpotifyStatus->ss_active_status = 0;
        g_spotify_active = 0;

#ifdef WMRMPLAY_ENABLE
        audio_cb.audio_deinit();
#else
        if (pSpotifyStatus->ss_play_status) {
            audio_cb.audio_pause(1);
        }
#endif

        pSpotifyStatus->ss_play_status = 0;
        pSpotifyStatus->pause_switch = 0;
        pSpotifyStatus->Notifycallbacks(sp_notify_active, 0, 0);
        break;
    case kSpPlaybackNotifyLostPermission:
        SPOTI_DEBUG("kSpPlaybackNotifyLostPermission\n");
        if (pSpotifyStatus->ss_active_status) {
#ifdef WMRMPLAY_ENABLE
            audio_cb.audio_deinit();
#else
            if (pSpotifyStatus->ss_play_status)
                audio_cb.audio_pause(1);
#endif
            pSpotifyStatus->ss_play_status = 0;
            pSpotifyStatus->Notifycallbacks(sp_notify_playstatus, 1, 0);
        }
        break;
    case kSpPlaybackEventAudioFlush:
        SPOTI_DEBUG("kSpPlaybackEventAudioFlush\n");
        audio_cb.audio_flush();
        break;
    case kSpPlaybackNotifyAudioDeliveryDone:
        if (curl_cond && curl_mutex) {
            pthread_mutex_lock(curl_mutex);
            pthread_cond_signal(curl_cond);
            pthread_mutex_unlock(curl_mutex);
        }
        break;
    case kSpPlaybackNotifyTrackDelivered:
        break;
    default:
        break;
    }
#ifdef EN_AVS_SPOTIFY
    if (SpPlaybackIsActiveDevice()) {
        if (kSpPlaybackNotifyPause == event) {
            if (notify_enent == 1) {
                SpotifyAddAvsNotify((int)event, 0);
            }
        } else {
            SpotifyAddAvsNotify((int)event, 0);
        }
    } else if (event == kSpPlaybackNotifyBecameInactive) {
        SpotifyAddAvsNotify((int)event, 0);
    }
#endif
}

static void CallbackNewCredentials(const char *credentials_blob, const char *username,
                                   void *context)
{
    /* Save blob to disk for subsequent logins with SpConnectionLoginBlob(). */
    if (credentials_blob == NULL)
        return;

    const char *onicalusername = username;
    memset(&user_logInfo, 0x0, sizeof(user_logInfo));
    if (onicalusername) {
        strncpy(user_logInfo.username, onicalusername, sizeof(user_logInfo.username) - 1);
    }
    strncpy(user_logInfo.blob, credentials_blob, sizeof(user_logInfo.blob) - 1);
    saveuserinfotoflash();
}

static void CallbackPlaybackSeek(uint32_t position_ms, void *context)
{
#ifdef EN_AVS_SPOTIFY
    SPOTI_DEBUG("Playback status: seeked to %u\n", position_ms);
    SpotifyAvsEventNotify(Event_PlayerEvent, Val_Seek, position_ms);
#endif
#ifdef BLE_NOTIFY_ENABLE
    SpotifyBLENotify(position_ms);
#endif
}

static void CallbackPlaybackApplyVolume(uint16_t volume, uint8_t remote, void *context)
{
    SPOTI_DEBUG("CallbackPlaybackApplyVolume volume now %u remote %d\n", volume, remote);
    struct SpotifyStatus *pSpotifyStatus = (struct SpotifyStatus *)context;
    if (pSpotifyStatus->ss_volume != volume && remote) {
        pSpotifyStatus->Notifycallbacks(sp_nofity_volume, volume * 100 / 65535, 0);
        pSpotifyStatus->ss_volume = volume;
    }
}

void CallbackDebugMessage(const char *debug_message, void *context)
{
    if (g_spotify_print) {
        wiimu_log(1, 0, 0, 0, "SPOTIFY MESSAGE: %s", debug_message);
    }
}
