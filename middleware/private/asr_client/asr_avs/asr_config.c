#include "wm_db.h"

#ifdef OPENWRT_AVS
#define ALEXA_DEFAULT_DEVICE_NAME ("PIKA")
#define ALEXA_DEFAULT_CLIENT_ID ("amzn1.application-oa2-client.bf4797ac034b451a81c2eac24838d5a9")
#define ALEXA_DEFAULT_CLIENT_SECRET                                                                \
    ("6eafe9f1dcea2e6e2b7da84920b0f0e0220b9a01ede21474da476702e13b0ae0")
#define ALEXA_DEFAULT_HOST ("avs-alexa-na.amazon.com")
#define ALEXA_DEFAULT_LANGUAGE "en-US"
#define NVRAM_ALEXA_ENDPOINT ("alexa_endpoint")
#endif

int asr_config_get(char *param, char *value, int value_len)
{
#ifdef OPENWRT_AVS
    if (0) {
    } else if (!strcmp(NVRAM_ALEXA_DEVICE_ID, param))
        strncpy(value, ALEXA_DEFAULT_DEVICE_NAME, value_len - 1);
    else if (!strcmp(NVRAM_ALEXA_CLIENT_ID, param))
        strncpy(value, ALEXA_DEFAULT_CLIENT_ID, value_len - 1);
    else if (!strcmp(NVRAM_ALEXA_CLIENT_SECRET, param))
        strncpy(value, ALEXA_DEFAULT_CLIENT_SECRET, value_len - 1);
    else if (!strcmp(NVRAM_ALEXA_ENDPOINT, param))
        strncpy(value, ALEXA_DEFAULT_HOST, value_len - 1);
    else
        return wm_db_get(param, value);
    return 1;
#else
    return wm_db_get(param, value);
#endif
}

int asr_config_set(char *param, char *value) { return wm_db_set(param, value); }

int asr_config_set_sync(char *param, char *value) { return wm_db_set_commit(param, value); }

void asr_config_sync(void) { wm_db_commit(); }
