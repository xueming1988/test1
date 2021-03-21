
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include <json.h>

#include "wm_api.h"
#include "wm_util.h"
#include "wm_db.h"

#include "asr_cfg.h"

#if defined(YD_SENSORY_MODULE)
#if defined(UPLUS_LINE)
#define ASR_SEARCH_ID_DEFAULT 3 // clova
//#define ASR_SEARCH_ID2_DEFAULT 1 //UplusTV
#define ASR_SEARCH_ID2_DEFAULT 5 // aladdin
#else
#define ASR_SEARCH_ID_DEFAULT 3
#define ASR_SEARCH_ID2_DEFAULT 3
#endif
#else
#define ASR_SEARCH_ID_DEFAULT 4
#endif

#ifndef ASR_HAVE_PROMPT_DEFAULT
#define ASR_HAVE_PROMPT_DEFAULT 1
#endif

#define DEFAULT_CH_ASR 1     // left channel for ASR
#define DEFAULT_CH_TRIGGER 1 // left channel for Trigger

#if defined(UPLUS_AIIF)
#define ASR_DEF_HOST ("dev-pub-cic.clova.ai")
#else
#define ASR_DEF_HOST ("prod-ni-cic.clova.ai")
// TODO: we need change the endpoint when the server is ready for release for DONUT
//#define ASR_DEF_HOST      ("beta-cic.clova.ai")
#endif

#define LGUP_DEF_HOST ("daiif.ai.uplus.co.kr")

#define ASR_DEF_LAN ("asr_default_lan")
#define ASR_LANGUAGE ("asr_language")
#define ASR_ENDPOINT ("asr_endpoint")
#define LGUP_ENDPOINT ("lgup_endpoint")

#define LAST_PAIED_BLE_MAC ("last_paired_ble_mac")
#define LAST_PAIED_BLE_DEVNAME ("last_paired_ble_devname")

extern WIIMU_CONTEXT *g_wiimu_shm;
static pthread_mutex_t g_mutex_cfg = PTHREAD_MUTEX_INITIALIZER;
#define cfg_lock() pthread_mutex_lock(&g_mutex_cfg)
#define cfg_unlock() pthread_mutex_unlock(&g_mutex_cfg)

int asr_config_get(char *param, char *value, int value_len) { return wm_db_get(param, value); }

int asr_config_set(char *param, char *value) { return wm_db_set(param, value); }

int asr_config_set_sync(char *param, char *value) { return wm_db_set_commit(param, value); }

void asr_config_sync(void) { wm_db_commit(); }

void asr_set_default_language(int flag)
{
    int def = asr_get_default_language();
    if (flag != def) {
        asr_config_set_sync(ASR_DEF_LAN, (flag == 1) ? "1" : "0");
    }
}

int asr_get_default_language(void)
{
    int is_already_default = 0;
    char buf[256] = {0};
    asr_config_get(ASR_DEF_LAN, buf, sizeof(buf));
    if (strlen(buf) > 0) {
        printf("asr_default_lan=%s\n", buf);
        is_already_default = atoi(buf);
    }

    return is_already_default;
}

int asr_set_language(char *location)
{
    char *lan = NULL;

    if (!location || strlen(location) <= 0) {
        printf("asr_set_language failed\n");
        return -1;
    }

    lan = asr_get_language();
    if (!lan || (lan && strcmp(lan, location))) {
        printf("asr_set_language old(%s) new(%s)\n", lan, location);
        asr_config_set_sync(ASR_LANGUAGE, location);
    }
    return 0;
}

char nv_buf[256] = {0};
char *asr_get_language(void)
{
    memset(nv_buf, 0x0, sizeof(nv_buf));
    cfg_lock();
    asr_config_get(ASR_LANGUAGE, nv_buf, sizeof(nv_buf));
    cfg_unlock();
    if (strlen(nv_buf) > 0) {
        printf("ASR_LANGUAGE=%s\n", nv_buf);
        return nv_buf;
    }

    return NULL;
}

int asr_init_language(void)
{
    char nv_buf[256] = {0};

    asr_config_get(ASR_LANGUAGE, nv_buf, sizeof(nv_buf));
    //  char *buf = nvram_get(RT2860_NVRAM, ASR_LANGUAGE);
    if (strlen(nv_buf) > 0) {
        printf("ASR_LANGUAGE=%s\n", nv_buf);
        return -1;
    }

    return asr_set_language(LOCALE_US);
}

int asr_set_endpoint(char *endpoint)
{
    if (!endpoint || strlen(endpoint) <= 0) {
        printf("asr_set_endpoint failed\n");
        return -1;
    }

    asr_set_default_language(0);
    cfg_lock();
    asr_config_set_sync(ASR_ENDPOINT, endpoint);
    cfg_unlock();

    return 0;
}

char *asr_get_endpoint(void)
{
    memset(nv_buf, 0x0, sizeof(nv_buf));

    cfg_lock();
    asr_config_get(ASR_ENDPOINT, nv_buf, sizeof(nv_buf));
    cfg_unlock();
    if (strlen(nv_buf) > 0) {
        printf("endpoint is=%s\n", nv_buf);
        return nv_buf;
    }

#if defined(AMLOGIC_A113) // A113test
    return ASR_DEF_HOST;
#else
    return NULL;
#endif
}

int asr_init_endpoint(void)
{
    char buf[256] = {0};
    asr_config_get(ASR_ENDPOINT, buf, sizeof(buf));

    if (strlen(buf) > 0) {
        printf("ASR_ENDPOINT=%s\n", buf);
        return -1;
    }

    return asr_set_endpoint(ASR_DEF_HOST);
}

static void get_asr_ch_from_nvram(void)
{
    int val = DEFAULT_CH_ASR;
    char buf[256] = {0};
    asr_config_get(NVRAM_ASR_CH_ASR, buf, sizeof(buf));
    if (strlen(buf) > 0)
        val = atoi(buf);
    g_wiimu_shm->asr_ch_asr = val;
#if defined(SOFT_PREP_HOBOT_MODULE)
    printf("#### get_asr_ch_from_nvram = %d", g_wiimu_shm->asr_ch_asr);
    g_wiimu_shm->asr_ch_asr = 1; // force set leftchannel as asr_channel
#endif
}

static void get_trigger_ch_from_nvram(void)
{
    int val = DEFAULT_CH_TRIGGER;
    char buf[256] = {0};
    asr_config_get(NVRAM_ASR_CH_TRIGGER, buf, sizeof(buf));
    if (strlen(buf) > 0)
        val = atoi(buf);
    g_wiimu_shm->asr_ch_trigger = val;
#if defined(SOFT_PREP_HOBOT_MODULE)
    printf("#### get_trigger_ch_from_nvram = %d", g_wiimu_shm->asr_ch_trigger);
    g_wiimu_shm->asr_ch_trigger = 1; // force set leftchannel
#endif
}

static void get_prompt_setting_from_nvram(void)
{
    int val = ASR_HAVE_PROMPT_DEFAULT;
    char buf[256] = {0};
    asr_config_get(NVRAM_ASR_HAVE_PROMPT, buf, sizeof(buf));
    if (strlen(buf) > 0) {
        val = atoi(buf);
        g_wiimu_shm->asr_have_prompt = (val == 1) ? 1 : 0;
    }
}

void set_prompt_setting_from_settings(int enabled)
{
    char *ptr = NULL;

    g_wiimu_shm->asr_have_prompt = enabled;

    if (enabled)
        ptr = "1";
    else
        ptr = "0";

    asr_config_set(NVRAM_ASR_HAVE_PROMPT, ptr);
    asr_config_sync();
}

void set_local_tone_from_settings(const char *tone_type)
{
    char *ptr = NULL;

    if (tone_type) {
        ptr = tone_type;
        ptr += strlen("V00");

        asr_config_set(NVRAM_ASR_LOCAL_TONE_INDEX, ptr);
        asr_config_sync();
    }
}

static void init_asr_feature(void)
{
    if (0 == (far_field_judge(g_wiimu_shm))) {
        g_wiimu_shm->asr_feature_flag = 0;
        return;
    }
    get_prompt_setting_from_nvram();
}

int get_asr_trigger1_keyword_index(void)
{
    int id = 0;
    char buf[256] = {0};
    asr_config_get(NVRAM_ASR_KEYWORD_INDEX, buf, sizeof(buf));
    if (strlen(buf) > 0)
        id = atoi(buf);

    if (id < 0 || id > 20)
        id = 0;
    g_wiimu_shm->trigger1_keyword_index = id;
    return id;
}

int get_asr_trigger2_keyword_index(void)
{
    int id = 0;
    char buf[256] = {0};
    asr_config_get(NVRAM_ASR_KEYWORD_INDEX2, buf, sizeof(buf));
    if (strlen(buf) > 0)
        id = atoi(buf);

    if (id < 0 || id > 20)
        id = 0;
    g_wiimu_shm->trigger2_keyword_index = id;
    return id;
}

void set_asr_keyword_cfg_as_index(int index)
{
#if defined(SOFT_PREP_HOBOT_MODULE)
    if (index == 1) { // we need to sync the hisf_config.ini for hobot "clova" keyword
        system("cp -rf /system/workdir/lib/hisf_config_clova.ini /tmp/hisf_reconfig.ini");
    } else {
        system("cp -rf /system/workdir/lib/hisf_config.ini /tmp/hisf_reconfig.ini");
    }
#endif
}

int set_asr_trigger1_keyword_index(int index)
{
    char buff[32] = {0};

    snprintf(buff, sizeof(buff), "%d", index);
    asr_config_set(NVRAM_ASR_KEYWORD_INDEX, buff);
    asr_config_sync();
    set_asr_keyword_cfg_as_index(index);

    g_wiimu_shm->trigger1_keyword_index = index;

    return 0;
}

int asr_init_keyword_index(void)
{
    int index = get_asr_trigger1_keyword_index();

    set_asr_keyword_cfg_as_index(index);

    return 0;
}

int get_asr_search_id(void)
{
    int id = ASR_SEARCH_ID_DEFAULT;
    char buf[256] = {0};
    asr_config_get(NVRAM_ASR_SEARCH_ID, buf, sizeof(buf));
    if (strlen(buf) > 0)
        id = atoi(buf);

    if (id < 1 || id > 20)
        id = ASR_SEARCH_ID_DEFAULT;
    g_wiimu_shm->asr_search_id = id;
    return id;
}

int get_asr_search_id2(void)
{
    int id = ASR_SEARCH_ID2_DEFAULT;
    char buf[256] = {0};
    asr_config_get(NVRAM_ASR_SEARCH_ID2, buf, sizeof(buf));
    if (strlen(buf) > 0)
        id = atoi(buf);

    if (id < 1 || id > 20)
        id = ASR_SEARCH_ID2_DEFAULT;
    g_wiimu_shm->asr_search_id2 = id;
    return id;
}

int asr_set_last_paired_ble(char *uuid, char *friendly_name, int want_clear)
{
    int ret = -1;

    if (want_clear == 0) {
        if (uuid && strlen(uuid) > 0) {
            asr_config_set(LAST_PAIED_BLE_MAC, uuid);
            ret = 0;
        }
        if (friendly_name && strlen(friendly_name) > 0) {
            asr_config_set(LAST_PAIED_BLE_DEVNAME, friendly_name);
            ret = 0;
        }
    } else if (want_clear == 1) {
        asr_config_set(LAST_PAIED_BLE_MAC, "");
        asr_config_set(LAST_PAIED_BLE_DEVNAME, "");
        ret = 0;
    }
    if (ret == 0) {
        printf("uuid=%s friendly_name=%s\n", uuid, friendly_name);
        asr_config_sync();
    }

    return ret;
}

int asr_get_last_paired_ble_uuid(char *ble_uuid, int len)
{
    int ret = -1;
    if (ble_uuid) {
        asr_config_get(LAST_PAIED_BLE_MAC, ble_uuid, len);
        if (strlen(ble_uuid) > 0) {
            ret = 0;
        }
    }

    return ret;
}

int asr_get_last_paired_ble_name(char *ble_name, int len)
{
    int ret = -1;
    if (ble_name) {
        asr_config_get(LAST_PAIED_BLE_DEVNAME, ble_name, len);
        if (strlen(ble_name) > 0) {
            ret = 0;
        }
    }

    return ret;
}

int asr_set_handle_free_mode(int on_off)
{
    if (on_off == e_mode_on) {
        g_wiimu_shm->asr_handsfree_disabled = 0;
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=handsfree_enable",
                                 strlen("GNOTIFY=handsfree_enable"), NULL, NULL, 0);
        asr_config_set(NVRAM_ASR_HANDSFREE_DISABLED, "0");
    } else if (on_off == e_mode_off) {
        g_wiimu_shm->asr_handsfree_disabled = 1;
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=handsfree_disable",
                                 strlen("GNOTIFY=handsfree_disable"), NULL, NULL, 0);
        asr_config_set(NVRAM_ASR_HANDSFREE_DISABLED, "1");
    }

    asr_config_sync();

    return 0;
}

int asr_set_privacy_mode(int on_off)
{
    if (on_off == e_mode_on) {
        if (!g_wiimu_shm->privacy_mode)
            g_wiimu_shm->privacy_mode = 1;
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=PRIVACY_ENA",
                                 strlen("GNOTIFY=PRIVACY_ENA"), NULL, NULL, 0);
    } else if (on_off == e_mode_off) {
        if (g_wiimu_shm->privacy_mode)
            g_wiimu_shm->privacy_mode = 0;
        SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=PRIVACY_DIS",
                                 strlen("GNOTIFY=PRIVACY_DIS"), NULL, NULL, 0);
    }

    asr_config_sync();

    return 0;
}

int asr_get_privacy_mode(void)
{
    int privacy_mode = 0;

    if (g_wiimu_shm) {
        privacy_mode = g_wiimu_shm->privacy_mode;
    }

    return privacy_mode;
}

char lgup_nv_buf[256] = {0};
int lguplus_set_endpoint(char *endpoint)
{
    if (!endpoint || strlen(endpoint) <= 0) {
        printf("asr_set_endpoint failed\n");
        return -1;
    }

    asr_set_default_language(0);
    cfg_lock();
    asr_config_set_sync(LGUP_ENDPOINT, endpoint);
    cfg_unlock();

    return 0;
}

int lguplus_init_endpoint(void)
{
    char buf[256] = {0};
    asr_config_get(LGUP_ENDPOINT, buf, sizeof(buf));

    if (strlen(buf) > 0) {
        printf("ASR_ENDPOINT=%s\n", buf);
        return -1;
    }

    return lguplus_set_endpoint(LGUP_DEF_HOST);
}

char *lguplus_get_endpoint(void)
{
    memset(lgup_nv_buf, 0x0, sizeof(lgup_nv_buf));

    cfg_lock();
    asr_config_get(LGUP_ENDPOINT, lgup_nv_buf, sizeof(lgup_nv_buf));
    cfg_unlock();
    if (strlen(lgup_nv_buf) > 0) {
        printf("endpoint is=%s\n", lgup_nv_buf);
        return lgup_nv_buf;
    }

#if defined(AMLOGIC_A113) // A113test
    return LGUP_DEF_HOST;
#else
    return NULL;
#endif
}

int asr_init_cfg(void)
{
    int ret = 0;

    asr_set_default_language(0);
    ret = asr_init_language();
    ret = asr_init_endpoint();
    ret = asr_init_keyword_index();

#if defined(UPLUS_AIIF)
    ret = lguplus_init_endpoint();
#endif

    get_trigger_ch_from_nvram();
    get_asr_ch_from_nvram();
    init_asr_feature();

    return ret;
}
