
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wm_util.h"

#include "alexa_cfg.h"
#include "alexa_json_common.h"
#include "asr_config.h"

/*
 *  UK: https://avs-alexa-eu.amazon.com
 */
#ifdef BAIDU_DUMI
#define ALEXA_DEF_HOST ("dueros-h2.baidu.com")
#elif defined(TENCENT)
#define ALEXA_DEF_HOST ("tvs.html5.qq.com")
#else
//#define ALEXA_DEF_HOST      ("avs-alexa-na.amazon.com")
//#define ALEXA_DEF_HOST      ("avs-alexa-eu.amazon.com")
/*
Asia            Australia, Japan, New Zealand https://alexa.fe.gateway.devices.a2z.com Europe
Austria, France,
Germany, India, Italy, Spain, United Kingdom      https://alexa.eu.gateway.devices.a2z.com North
America   Canada,
Mexico, United States                                      https://alexa.na.gateway.devices.a2z.com
*/
#define ALEXA_DEF_HOST_FE ("alexa.fe.gateway.devices.a2z.com")
#define ALEXA_DEF_HOST_EU ("alexa.eu.gateway.devices.a2z.com")
#define ALEXA_DEF_HOST_NA ("alexa.na.gateway.devices.a2z.com")
#define LIFEPOD_PRODUCTION_HOST ("avs.lifepod.com")
#define LIFEPOD_DEF_HOST LIFEPOD_PRODUCTION_HOST

#define CHECK_HOST_EU(language)                                                                    \
    (strcmp(language, "Ger_de") == 0 || strcmp(language, "de-DE") == 0 ||                          \
     strcmp(language, "en_uk") == 0 || strcmp(language, "en-GB") == 0 ||                           \
     strcmp(language, "french") == 0 || strcmp(language, "fr-FR") == 0 ||                          \
     strcmp(language, "en_ir") == 0 || strcmp(language, "en-IR") == 0 ||                           \
     strcmp(language, "italian") == 0 || strcmp(language, "it-IT") == 0 ||                         \
     strcmp(language, "en_in") == 0 || strcmp(language, "en-IN") == 0)

#define CHECK_HOST_FE(language) (strcmp(language, "ja_jp") == 0 || strcmp(language, "ja-JP") == 0)

#endif
#define ALEXA_DEF_LAN ("asr_default_lan")
#define ALEXA_LANGUAGE ("alexa_language")
#define ALEXA_ENDPOINT ("alexa_endpoint")

extern WIIMU_CONTEXT *g_wiimu_shm;
static pthread_mutex_t g_mutex_cfg = PTHREAD_MUTEX_INITIALIZER;
#define cfg_lock() pthread_mutex_lock(&g_mutex_cfg)
#define cfg_unlock() pthread_mutex_unlock(&g_mutex_cfg)

void asr_set_default_language(int flag)
{
    int def = asr_get_default_language();
    if (flag != def) {
        asr_config_set_sync(ALEXA_DEF_LAN, (flag == 1) ? "1" : "0");
    }
}

int asr_get_default_language(void)
{
    int is_already_default = 0;
    char buf[256] = {0};
    asr_config_get(ALEXA_DEF_LAN, buf, sizeof(buf));
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
        fprintf(stderr, "asr_set_language failed\n");
        return -1;
    }

    lan = asr_get_language();
    if (!lan || (lan && strcmp(lan, location))) {
        printf("asr_set_language old(%s) new(%s)\n", lan, location);
        asr_config_set_sync(ALEXA_LANGUAGE, location);
    }
    return 0;
}
char nv_buf[256] = {0};
char *asr_get_language(void)
{
    asr_config_get(ALEXA_LANGUAGE, nv_buf, sizeof(nv_buf));
    if (strlen(nv_buf) > 0) {
        fprintf(stderr, "alexa_language is=%s\n", nv_buf);
        return nv_buf;
    }

    return NULL;
}

int asr_init_language(void)
{
    asr_config_get(ALEXA_LANGUAGE, nv_buf, sizeof(nv_buf));
    //  char *buf = nvram_get(RT2860_NVRAM, ALEXA_LANGUAGE);
    if (strlen(nv_buf) > 0) {
        fprintf(stderr, "alexa_language=%s\n", nv_buf);
        return -1;
    }

    if (strcmp(g_wiimu_shm->language, "Ger_de") == 0 || strcmp(g_wiimu_shm->language, "de-DE") == 0)
        return asr_set_language(LOCALE_DE);
    else if (strcmp(g_wiimu_shm->language, "en_uk") == 0 ||
             strcmp(g_wiimu_shm->language, "en-GB") == 0)
        return asr_set_language(LOCALE_UK);
    else if (strcmp(g_wiimu_shm->language, "en_in") == 0 ||
             strcmp(g_wiimu_shm->language, "en-IN") == 0)
        return asr_set_language(LOCALE_IN);
    else if (strcmp(g_wiimu_shm->language, "ja_jp") == 0 ||
             strcmp(g_wiimu_shm->language, "ja-JP") == 0)
        return asr_set_language(LOCALE_JP);
    else if (strcmp(g_wiimu_shm->language, "en_ca") == 0 ||
             strcmp(g_wiimu_shm->language, "en-CA") == 0)
        return asr_set_language(LOCALE_CA);
    else if (strcmp(g_wiimu_shm->language, "en_ir") == 0 ||
             strcmp(g_wiimu_shm->language, "en-IR") == 0)
        return asr_set_language(LOCALE_UK);
    else if (strcmp(g_wiimu_shm->language, "french") == 0 ||
             strcmp(g_wiimu_shm->language, "fr-FR") == 0)
        return asr_set_language(LOCALE_FR);
    else if (strcmp(g_wiimu_shm->language, "italian") == 0 ||
             strcmp(g_wiimu_shm->language, "it-IT") == 0)
        return asr_set_language(LOCALE_IT);
    else if (strcmp(g_wiimu_shm->language, "spanish") == 0 ||
             strcmp(g_wiimu_shm->language, "es-ES") == 0)
        return asr_set_language(LOCALE_ES);
    else if (strcmp(g_wiimu_shm->language, "es_mx") == 0 ||
             strcmp(g_wiimu_shm->language, "mx-ES") == 0)
        return asr_set_language(LOCALE_MX);
    else if (strcmp(g_wiimu_shm->language, "fr_ca") == 0 ||
             strcmp(g_wiimu_shm->language, "fr-CA") == 0)
        return asr_set_language(LOCALE_CA_FR);
    else if (strcmp(g_wiimu_shm->language, "portuguese") == 0 ||
             strcmp(g_wiimu_shm->language, "pt-BR") == 0)
        return asr_set_language(LOCALE_PT_BR);
    else
        return asr_set_language(LOCALE_US);
}

int asr_set_endpoint(char *endpoint)
{
    int ret = -1;

    if (!endpoint || strlen(endpoint) <= 0) {
        fprintf(stderr, "asr_set_endpoint failed\n");
        return ret;
    }

    cfg_lock();
    char tmp_buf[256] = {0};
    asr_config_get(ALEXA_ENDPOINT, tmp_buf, sizeof(tmp_buf));
    if ((strlen(tmp_buf) > 0 && strcmp(endpoint, tmp_buf) != 0) || (strlen(tmp_buf) == 0)) {
        fprintf(stderr, "old endpoint is=%s new endpoint is %s\n", tmp_buf, endpoint);
        asr_config_set_sync(ALEXA_ENDPOINT, endpoint);
        ret = 0;
    }
    cfg_unlock();
    // if (ret == 0) {
    //    asr_set_default_language(0);
    //}

    return ret;
}

char *asr_get_endpoint(void)
{
    cfg_lock();
    asr_config_get(ALEXA_ENDPOINT, nv_buf, sizeof(nv_buf));
    cfg_unlock();
    if (strlen(nv_buf) > 0) {
        fprintf(stderr, "endpoint is=%s\n", nv_buf);
        return nv_buf;
    }

#if defined(AMLOGIC_A113) // A113test
    return LIFEPOD_DEF_HOST;
#else
    return NULL;
#endif
}

int asr_init_endpoint(void)
{
    int need_def = 0;
    char *default_host = (char *)LIFEPOD_DEF_HOST;
    char buf[256] = {0};
    asr_config_get(ALEXA_ENDPOINT, buf, sizeof(buf));
#ifdef BAIDU_DUMI
    if ((!strstr(buf, "baidu"))) {
        need_def = 1;
    }
#elif defined(TENCENT)
    if ((!strstr(buf, "qq"))) {
        need_def = 1;
    }
#else
    if (!strstr(buf, "lifepod")) {
        need_def = 1;
    }
    if (strlen(buf) == 0) {
        if (CHECK_HOST_EU(g_wiimu_shm->language)) {
            need_def = 1;
            default_host = ALEXA_DEF_HOST_EU;
        } else if (CHECK_HOST_FE(g_wiimu_shm->language)) {
            need_def = 1;
            default_host = ALEXA_DEF_HOST_FE;
        }
    }
#endif
    if (need_def == 0 && strlen(buf) > 0) {
        fprintf(stderr, "alexa_endpoint=%s\n", buf);
        return -1;
    }

    fprintf(stderr, "need_def=%d alexa_endpoint=%s\n", need_def, buf);
    return asr_set_endpoint(default_host);
}

int asr_init_cfg(void)
{
    int ret = 0;

    // asr_set_default_language(0);
    ret = asr_init_language();
    ret += asr_init_endpoint();

    return ret;
}
