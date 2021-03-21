#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <errno.h>

#include "json.h"
#include "wm_util.h"
#include "curl_https.h"
#include "membuffer.h"

#include "alexa_debug.h"
#include "alexa_api2.h"
#include "alexa_json_common.h"
#include "alexa_products_macro.h"
#include "alexa_authorization.h"
#include "alexa_msg.h"
#include "alexa_result_parse.h"
#include "alexa_alert.h"
#include "asr_config.h"
#include "lpcrypto.h"

static unsigned char key[] = "~linkplayble@#==";

char *refreshToken =
    "eyJjdHkiOiJKV1QiLCJlbmMiOiJBMjU2R0NNIiwiYWxnIjoiUlNBLU9BRVAifQ.EJLFNi62uTACrsoEvT5__"
    "J4Z9lCjhK6CTShpU4VYII2nxzIGmANsOpNjpkCB8i4ZlBx5YPG-"
    "WD49Hj7WZEIugI69JKKmcHMbonmNi8NXywd8J7aGepT9jhmosnmoO0JzdcGLwd9pfb5wmfVneQdHEK2Llj4yv3TAnH-"
    "C920wgGPgRADZzPPw0ecgiSvC10V8B-ep9qYutWN-B_vqW18i_"
    "NgtVHWRtPehEpU4lMcYz8TYFt5WOgT33tWLL7fk4ePCA4jUXLRXmsOEp1qhteuG1CkJ-"
    "DGKukBLos5zlcCrlXxkFrpDj4UszrQsPn2tJj6QvWWccGsSJ716zlhtMwtTXg.bCSm6Hdg5AmwTfuR.fvdhlZ-_"
    "tDJrjrNnce0cc1JoC7SbQH7aDm7mdnYq5J2_kx4beq2d5q5rpA1kqyR2RTMw5troysWYHakCrdPJ5_EQcLZXPGNU9lOqU_"
    "-79WxPd5fIde7RsTKywHVbGQOz_u-9E9xEXSio2BwDNkG9vYHff4l9sOcsitm_"
    "RvumdY01a1pFktcGbvTs36J90wMG2Koep8QAsO-nzY2olPQyptGsFnotgYv-y-"
    "fQ2Otf0i0WChTsxOgeZl0VjrrcZEMhLyvDALnDUV8u44CgY0QnkXvqgS6KG7hu0OY8rT2eUGbQa5xtqjAHej1rog7O-"
    "Ojt4Wwf-jQkYzV-s6kGnhM4pALvHuPtddVrSL-D5m_"
    "r24mxPOGgRUXbw4GS2bMSDc39LvgOVLElQeruT3lQthPSaLKrYV98_"
    "tiIFCC1DrJ5Kh2LCyozMoMQ8ICJoLTWV4mRz8oobuypmZtTQWCWBTKHRpMeeGAjvmrKZQ1b5pFQGQ9V1vvmf8Trpd5914e"
    "tpKtdjVfu-Gpn_7580EIX2X8xdwprH0ngZYfAOf_VEBVq6jYXIWBlnxIaTwplgnlKSxkrF9RWwXciLJbjXqGQxY-73z_"
    "3rMxixtyOQV1PkRmt7FfwXetLZ-BRYpGoV9twb2t-8PeRsVR8uTtTF-aLFV1GK7UVQ__CYE4djdevfAvHfBgd4fj0s-"
    "3UwqwTKnel7bAbx8h6z_DI_EHD_dgxqEK4EH1pWBY7uEKtKxRLcE7zxg0YJEikjcuWtu3Gggpckg-"
    "kmByLdWtdd6Jtx1IEa_"
    "vVSfKToTrV3KUswaDr0kFLWVPra6JRZ2QcINnPdjkpsCyCZud06g4uV1QKNn8yredhB0uRPdcxcTbvxqId6PCiGqOf_"
    "bgIJYLCM7Hrjv4tewKzaEHoYv6WiNReX9uhhW-fRVZl0V1UNC1eehtinw4RkaUIySvF_-"
    "bo42wgSPfhQQNqWArw0VLGUoEJGm2be4C_"
    "e70WTVmbAkdmCRw7pVnpaIxNuUYY5jGLRu0oEKifm4fEd3UbAZbAz6S42icCUR0vgJFg2z0wJMt-o6CA2ORMLN14-"
    "k99dg0wcpQSfMTFOkf1yLbvIciPkb4UOJ_US7KtASzEOYVrEqPXYFI7paUn7Q9VMPIaEAUVTk-"
    "2oCdInS8bXMPesEnbARiH-YDfMAAEwr8k2wf2jL0ovCv0KSxT0qrAkuqsbWPoHVbR6HnSIo5Oimtl0kOqwdYYyUtHaw-"
    "8WIkWZneARPXIJEsC3PT_734lJ4vk4lyvfgPEofWrnhavhAw.dX8WJEIM9lrB9SXvSRsJcQ";
//-------------------------------------------------------------------------------------------------------------------------------------------
#ifdef BAIDU_DUMI
#define ALEXA_AUTHO2_TOKEN "https://openapi.baidu.com/oauth/2.0/token"
#else
#define ALEXA_AUTHO2_TOKEN "https://api.amazon.com/auth/o2/token"
#endif

/*
Host: https://api.amazon.com
Content-Type: application/json

{
    "grant_type":"",
    "refresh_token":"",
    "client_id":"",
    "client_secret":"",
}

{"access_token":"","refresh_token":"","token_type":"bearer","expires_in":3600}

*/
#ifdef BAIDU_DUMI
#define ALEXA_REFEASH_TOKEN_HEADER                                                                 \
    ("Host: openapi.baidu.com\r\napplication/x-www-form-urlencoded;charset=UTF-8")
#define ALEXA_REFEASH_TOKEN_DATA                                                                   \
    ("grant_type=refresh_token&refresh_token=%s&client_id=%s&client_secret=%s")
#define DEFAULT_REFRESH_TIME (2592000 * 1000)
#else
#define ALEXA_REFEASH_TOKEN_HEADER                                                                 \
    ("Host: api.amazon.com\r\napplication/x-www-form-urlencoded;charset=UTF-8")
#define ALEXA_REFEASH_TOKEN_DATA                                                                   \
    ("grant_type=refresh_token&refresh_token=%s&client_id=%s&client_secret=%s")
#define DEFAULT_REFRESH_TIME (3600 * 1000)
#endif

#define LIFEPOD_REFEASH_TOKEN_DATA ("grant_type=refresh_token&refresh_token=%s&client_id=%s")

//#define ALEXA_TOKEN_DATA
//("grant_type=authorization_code&code=%s&client_id=%s&code_verifier=%s&redirect_uri=%s")
#define ALEXA_TOKEN_DATA                                                                           \
    ("grant_type=authorization_code&code=%s&client_id=%s&client_secret=%s&redirect_uri=%s")
#define ALEXA_LWA_TOKEN_DATA                                                                       \
    ("grant_type=authorization_code&code=%s&client_id=%s&code_verifier=%s&redirect_uri=%s")
#define ALEXA_LWA_REFEASH_TOKEN_DATA ("grant_type=refresh_token&refresh_token=%s&client_id=%s")

#ifdef ASR_3PDA_ENABLE
char client_id[128] = {0};
// char code_verifier[128] = {0};
#endif

#define PCO_ENABLE_SKILL_BODY                                                                      \
    "{ \
	\"accountLinkRequest\": { \
		\"authCode\": \"%s\", \
		\"redirectUri\": \"%s\", \
		\"type\": \"AUTH_CODE\" \
	}, \
	\"skill\": { \
		\"id\": \"%s\", \
		\"stage\": \"development\" \
	} \
}"

#define PCO_DELETE_REPORT                                                                          \
    "{ \
  \"event\": { \
    \"header\": { \
      \"messageId\": \"%s\", \
      \"name\": \"DeleteReport\", \
      \"namespace\": \"Alexa.Discovery\", \
      \"payloadVersion\": \"3\" \
    }, \
    \"payload\": { \
      \"endpoints\": [{ \
          \"endpointId\": \"%s\" \
        }, \
        { \
          \"endpointId\": \"%s\" \
        } \
      ], \
      \"scope\": { \
        \"type\": \"BearerToken\", \
        \"token\": \"%s\" \
      } \
    } \
  } \
}"

#define PCO_ADD_OR_UPDATE_REPORT                                                                   \
    "{ \
	\"event\": { \
		\"header\": { \
			\"namespace\": \"Alexa.Discovery\", \
			\"name\": \"AddOrUpdateReport\", \
			\"payloadVersion\": \"3\", \
			\"messageId\": \"%s\" \
		}, \
		\"payload\": { \
			\"endpoints\": [{ \
				\"endpointId\": \"%s\", \
				\"friendlyName\": \"%s\", \
				\"description\": \"Smart Light by Sample Manufacturer\", \
				\"manufacturerName\": \"Sample Manufacturer\", \
				\"displayCategories\": [ \
					\"LIGHT\" \
				], \
				\"cookie\": { \
					\"extraDetail1\": \"optionalDetailForSkillToReferenceThisDevice\", \
					\"extraDetail2\": \"There can be multiple entries\", \
					\"extraDetail3\": \"use for reference purposes\", \
					\"extraDetail4\": \"Do not use to maintain device state\" \
				}, \
				\"capabilities\": [{ \
						\"type\": \"AlexaInterface\", \
						\"interface\": \"Alexa.ColorTemperatureController\", \
						\"version\": \"3\", \
						\"properties\": { \
							\"supported\": [{ \
								\"name\": \"colorTemperatureInKelvin\" \
							}], \
							\"proactivelyReported\": true, \
							\"retrievable\": true \
						} \
					}, \
					{ \
						\"type\": \"AlexaInterface\", \
						\"interface\": \"Alexa.EndpointHealth\", \
						\"version\": \"3\", \
						\"properties\": { \
							\"supported\": [{ \
								\"name\": \"connectivity\" \
							}], \
							\"proactivelyReported\": true, \
							\"retrievable\": true \
						} \
					}, \
					{ \
						\"type\": \"AlexaInterface\", \
						\"interface\": \"Alexa\", \
						\"version\": \"3\" \
					}, \
					{ \
						\"type\": \"AlexaInterface\", \
						\"interface\": \"Alexa.ColorController\", \
						\"version\": \"3\", \
						\"properties\": { \
							\"supported\": [{ \
								\"name\": \"color\" \
							}], \
							\"proactivelyReported\": true, \
							\"retrievable\": true \
						} \
					}, \
					{ \
						\"type\": \"AlexaInterface\", \
						\"interface\": \"Alexa.PowerController\", \
						\"version\": \"3\", \
						\"properties\": { \
							\"supported\": [{ \
								\"name\": \"powerState\" \
							}], \
							\"proactivelyReported\": true, \
							\"retrievable\": true \
						} \
					}, \
					{ \
						\"type\": \"AlexaInterface\", \
						\"interface\": \"Alexa.BrightnessController\", \
						\"version\": \"3\", \
						\"properties\": { \
							\"supported\": [{ \
								\"name\": \"brightness\" \
							}], \
							\"proactivelyReported\": true, \
							\"retrievable\": true \
						} \
					} \
				] \
			}], \
			\"scope\": { \
				\"type\": \"BearerToken\", \
				\"token\": \"%s\" \
			} \
		} \
	} \
}"
#define PCO_ENABLE_URL ("https://api.amazonalexa.com/v0/skills/%s/enablement")
#define PCO_ADD_OR_UPDATE_REPORT_URL ("https://api.amazonalexa.com/v3/events")
#define PCO_AUTHORIZATION ("Authorization: Bearer %s")
#ifndef RAND_STR_LEN
#define RAND_STR_LEN (36)
#endif
#define CODE_VERIFIER_STR_LEN (43)

// {"error":"expired_token","error_description":"refresh token has been used"}

typedef struct alexa_access_s {
    pthread_mutex_t lock;
    char *alexa_access_code;
    char *redirect_uri;
    char *code_verifier;
    /*the access token return by AMAZON for http request*/
    char *alexa_access_token;
    /*the refresh access token return by AMAZON for get a new access token*/
    char *alexa_refresh_token;
    char *device_type_id;
    char *client_id;
    char *client_secret;
    char *server_url;
    long long expires_in;
    long long refresh_time;
} alexa_access_t;

extern WIIMU_CONTEXT *g_wiimu_shm;
static alexa_access_t g_alexa_access = {0};
static alexa_access_t g_lifepod_access = {0};

/*"21abbca2-4db3-461a-bd77-2ab0251e84f5"*/
static char *alexa_rand_string_36(char *str)
{
    int i = 0;

    if (!str) {
        return NULL;
    }

    srand((unsigned int)NsSincePowerOn());

    for (i = 0; i < RAND_STR_LEN; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            str[i] = '-';
        } else {
            if (rand() % 2) {
                str[i] = (char)(rand() % 9 + '0');
            } else {
                str[i] = (char)(rand() % ('z' - 'a' + 1) + 'a');
            }
        }
    }

    str[RAND_STR_LEN] = '\0';

    return str;
}

static char *lifepod_rand_string_43(char *str)
{
    int i = 0;

    if (!str) {
        return NULL;
    }

    srand((unsigned)time(NULL));

    for (i = 0; i < CODE_VERIFIER_STR_LEN; i++) {
        if (rand() % 3 == 2) {
            str[i] = (char)(rand() % 9 + '0');
        } else if (rand() % 3 == 1) {
            str[i] = (char)(rand() % ('z' - 'a' + 1) + 'a');
        } else {
            str[i] = (char)(rand() % ('Z' - 'A' + 1) + 'A');
        }
    }

    str[CODE_VERIFIER_STR_LEN] = '\0';

    return str;
}

static size_t write_data(void *buffer, size_t size, size_t nmemb, void *stream)
{
    curl_result_t *result = (curl_result_t *)stream;
    membuffer_append(&result->result_body, buffer, (size_t)nmemb * size);

    return nmemb * size;
}

static size_t header_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
    size_t len = size * nmemb;
    curl_result_t *response = (curl_result_t *)stream;
    membuffer_append(&response->result_header, ptr, len);

    return len;
}

static int alexa_save_cfg(char *data, size_t len, int encrypt)
{
    int ret = -1;
    FILE *cfg_file = NULL;

    if (data && len > 0) {
        cfg_file = fopen(ALEXA_TOKEN_PATH, "wb+");
        if (cfg_file) {
            fwrite(data, 1, len, cfg_file);
            fflush(cfg_file);
            fclose(cfg_file);
            ret = 0;
        }
    }

    if (encrypt) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "echo 1 > %s", ALEXA_TOKEN_PATH_SECURE);
        system(cmd);
    } else {
        if (remove(ALEXA_TOKEN_PATH_SECURE))
            fprintf(stderr, "%s remove %s failed\n", __FUNCTION__, ALEXA_TOKEN_PATH_SECURE);
    }
    sync();

    return ret;
}

static int lifepod_save_cfg(char *data, size_t len, int encrypt)
{
    int ret = -1;
    FILE *cfg_file = NULL;

    if (data && len > 0) {
        cfg_file = fopen(LIFEPOD_TOKEN_PATH, "wb+");
        if (cfg_file) {
            fwrite(data, 1, len, cfg_file);
            fflush(cfg_file);
            fclose(cfg_file);
            ret = 0;
        }
    }

    if (encrypt) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "echo 1 > %s", LIFEPOD_TOKEN_PATH_SECURE);
        system(cmd);
    } else {
        if (remove(LIFEPOD_TOKEN_PATH_SECURE))
            fprintf(stderr, "%s remove %s failed\n", __FUNCTION__, LIFEPOD_TOKEN_PATH_SECURE);
    }
    sync();

    return ret;
}

/*
 * parse the access token and refresh frome amazon response
 */
int alexa_result_get_access_token(char *body)
{
    char *refreah_token = NULL;
    char *access_token = NULL;
    long expires_in = 0;

    if (!body) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "input error\n");
        return -1;
    }

    pthread_mutex_lock(&g_alexa_access.lock);

    json_object *json_obj = json_tokener_parse(body);
    if (json_obj) {
        access_token = (char *)JSON_GET_STRING_FROM_OBJECT(json_obj, "access_token");
        if (access_token) {
            if (g_alexa_access.alexa_access_token) {
                free(g_alexa_access.alexa_access_token);
            }
            g_alexa_access.alexa_access_token = strdup(access_token);
        }

        refreah_token = (char *)JSON_GET_STRING_FROM_OBJECT(json_obj, "refresh_token");
        if (refreah_token) {
            if (g_alexa_access.alexa_refresh_token) {
                free(g_alexa_access.alexa_refresh_token);
            }
            g_alexa_access.alexa_refresh_token = strdup(refreah_token);
        }

        expires_in = (long)JSON_GET_LONG_FROM_OBJECT(json_obj, "expires_in");
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "expires_in = %ld\n", expires_in);
        if (expires_in > 0) {
            g_alexa_access.expires_in = ((long long)expires_in - 60)
                                            ? ((long long)expires_in - 60) * 1000
                                            : (long long)expires_in * 1000;
        }
        if (access_token && refreah_token) {
            g_alexa_access.refresh_time = tickSincePowerOn();
#ifdef BAIDU_DUMI
            char *buf = NULL;
            asprintf(&buf, "%s:%s", access_token, refreah_token);
            if (buf) {
                alexa_save_cfg(buf, strlen(buf), 0);
                free(buf);
            }
#endif
        }
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "g_alexa_access.expires_in = %lld  refresh_time = %lld\n",
                    g_alexa_access.expires_in, g_alexa_access.refresh_time);

        json_object_put(json_obj);
    }
    pthread_mutex_unlock(&g_alexa_access.lock);

    return 0;
}

int lifepod_result_get_access_token(char *body)
{
    char *refreah_token = NULL;
    char *access_token = NULL;
    long expires_in = 0;

    if (!body) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "input error\n");
        return -1;
    }

    pthread_mutex_lock(&g_lifepod_access.lock);

    json_object *json_obj = json_tokener_parse(body);
    if (json_obj) {
        access_token = (char *)JSON_GET_STRING_FROM_OBJECT(json_obj, "access_token");
        if (access_token) {
            if (g_lifepod_access.alexa_access_token) {
                free(g_lifepod_access.alexa_access_token);
            }
            g_lifepod_access.alexa_access_token = strdup(access_token);
        }

        refreah_token = (char *)JSON_GET_STRING_FROM_OBJECT(json_obj, "refresh_token");
        if (refreah_token) {
            if (g_lifepod_access.alexa_refresh_token) {
                free(g_lifepod_access.alexa_refresh_token);
            }
            g_lifepod_access.alexa_refresh_token = strdup(refreah_token);
        }

        expires_in = (long)JSON_GET_LONG_FROM_OBJECT(json_obj, "expires_in");
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "expires_in = %ld\n", expires_in);
        if (expires_in > 0) {
            g_lifepod_access.expires_in = ((long long)expires_in - 60)
                                              ? ((long long)expires_in - 60) * 1000
                                              : (long long)expires_in * 1000;
        }
        if (access_token && refreah_token) {
            g_lifepod_access.refresh_time = tickSincePowerOn();
            char *buf = NULL;
            asprintf(&buf, "%s:%s", access_token, refreah_token);
            if (buf) {
                lifepod_save_cfg(buf, strlen(buf), 0);
                free(buf);
            }
        }
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "g_lifepod_access.expires_in = %lld  refresh_time = %lld\n",
                    g_lifepod_access.expires_in, g_lifepod_access.refresh_time);

        json_object_put(json_obj);
    }
    pthread_mutex_unlock(&g_lifepod_access.lock);

    return 0;
}

int alexa_result_error(char *body)
{
    int ret = -1;
    char *error = NULL;

    if (!body) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "input error\n");
        return -1;
    }

    json_object *json_obj = json_tokener_parse(body);
    if (json_obj) {
        error = (char *)JSON_GET_STRING_FROM_OBJECT(json_obj, "error_description");
        if (error) {
            if (!strncmp(error, "refresh token has been used", strlen(error))) {
                ret = 0;
            }
        }
        json_object_put(json_obj);
    }

    return ret;
}

int pco_get_skill_enable_status(char *data)
{
    char url[1024];
    int http_ret = 0;
    int ret = -1;
    char skillId[128];
    char *authorization = NULL;
    char message_id[RAND_STR_LEN + 1] = {0};
    char *token = NULL;

    struct curl_slist *list_header = NULL;
    curl_https_t *curl_https = NULL;
    curl_result_t *https_response = NULL;

    if (data && strlen(data) > 0) {
        char *p1 = data;
        p1 += strlen("skill_id=");
        strncpy(skillId, p1, sizeof(skillId) - 1);

        snprintf(url, sizeof(url) - 1, PCO_ENABLE_URL, skillId);

        asprintf(&authorization, PCO_AUTHORIZATION, g_alexa_access.alexa_access_token);
        asprintf(&token, "Postman-Token: %s", alexa_rand_string_36(message_id));

        list_header = curl_slist_append(list_header, authorization);
        if (list_header) {
            curl_slist_append(list_header, "Cache-Control: no-cache");
            curl_slist_append(list_header, token);
        }

        // ALEXA_PRINT(ALEXA_DEBUG_INFO, "post data len is %d\n", strlen(post_body));

        curl_https = CurlHttpsInit();
        CurlHttpsSetUrl(curl_https, url);
        CurlHttpsSetTimeOut(curl_https, 20L);
        // CurlHttpsSetVerbose(curl_https, 1L);
        CurlHttpsSetHeaderCallBack(curl_https, (void *)header_callback, NULL);
        CurlHttpsSetWriteCallBack(curl_https, (void *)write_data, NULL);
        CurlHttpsSetHeader(curl_https, list_header);
        CurlHttpsSetGet(curl_https, 1);
        // CurlHttpsSetBody(curl_https, post_body);

        CurlHttpsRun(curl_https, 1);
        http_ret = CurlHttpsGetHttpRetCode(curl_https);

        https_response = CurlHttpsGetResult(curl_https);
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "http_ret = %d\n", http_ret);
        if (https_response) {
            if (https_response->result_header.buf) {
                ALEXA_PRINT(ALEXA_DEBUG_INFO, "\nresponse header is:\n%s\n\n",
                            https_response->result_header.buf);
            }

            char *status;
            char *accountLinkStatus;
            json_object *json_obj = json_tokener_parse(https_response->result_body.buf);
            if (json_obj) {
                status = (char *)JSON_GET_STRING_FROM_OBJECT(json_obj, "status");
                json_object *accoutLink = json_object_object_get(json_obj, "accountLink");
                if (accoutLink) {
                    accountLinkStatus = (char *)JSON_GET_STRING_FROM_OBJECT(accoutLink, "status");
                    if ((strncmp(status, "ENABLED", strlen("ENABLED")) == 0 ||
                         strncmp(status, "ENABLING", strlen("ENABLING")) == 0) &&
                        strncmp(accountLinkStatus, "LINKED", strlen("LINKED")) == 0)
                        ret = 0;
                }
                json_object_put(json_obj);
            }
#if 0
            if(https_response->result_body.buf) {
                if(http_ret != 200) {
                    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "\nresponse body is:\n%s\n\n", \
                                https_response->result_body.buf);
                }
                if(http_ret == 200) {
                    ret = alexa_result_get_enable_skill_status(https_response->result_body.buf);
                    printf("JiangTao result_body.buf = %s\n", https_response->result_body.buf);
                }
            }
#endif
        }
#if 0
        if(200 != http_ret) {
            return -1;
        }
#endif
        CurlHttpsUnInit(&curl_https);
        ALEXA_PRINT(ALEXA_DEBUG_INFO, "end --- at %lld\n", tickSincePowerOn());

        if (authorization)
            free(authorization);
        if (token)
            free(token);
    }

    return ret;
}

int pco_disable_skill(char *data)
{
    char skillId[128];
    int ret = -1;
    char *authorization;
    char url[1024];
    int http_ret = 0;

    struct curl_slist *list_header = NULL;
    curl_https_t *curl_https = NULL;
    curl_result_t *https_response = NULL;

    if (data && strlen(data) > 0) {

        pco_delete_report();

        char *p1 = data;
        p1 += strlen("skill_id=");
        strncpy(skillId, p1, sizeof(skillId) - 1);

        snprintf(url, sizeof(url) - 1, PCO_ENABLE_URL, skillId);

        asprintf(&authorization, PCO_AUTHORIZATION, g_alexa_access.alexa_access_token);

        list_header = curl_slist_append(list_header, authorization);
        if (list_header) {
            curl_slist_append(list_header, "Cache-Control: no-cache");
        }

        // ALEXA_PRINT(ALEXA_DEBUG_INFO, "post data len is %d\n", strlen(post_body));

        curl_https = CurlHttpsInit();
        CurlHttpsSetUrl(curl_https, url);
        CurlHttpsSetTimeOut(curl_https, 20L);
        // CurlHttpsSetVerbose(curl_https, 1L);
        CurlHttpsSetHeaderCallBack(curl_https, (void *)header_callback, NULL);
        CurlHttpsSetWriteCallBack(curl_https, (void *)write_data, NULL);
        CurlHttpsSetHeader(curl_https, list_header);
        CurlHttpsSetMethod(curl_https, "DELETE");
        // CurlHttpsSetBody(curl_https, post_body);

        CurlHttpsRun(curl_https, 1);
        http_ret = CurlHttpsGetHttpRetCode(curl_https);

        https_response = CurlHttpsGetResult(curl_https);
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "http_ret = %d\n", http_ret);
        if (https_response) {
            if (https_response->result_header.buf) {
                ALEXA_PRINT(ALEXA_DEBUG_INFO, "\nresponse header is:\n%s\n\n",
                            https_response->result_header.buf);
            }

            if (https_response->result_body.buf) {
                if (http_ret != 200) {
                    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "\nresponse body is:\n%s\n\n",
                                https_response->result_body.buf);
                }
                if (http_ret == 200) {
                    ret =
                        0; // alexa_result_get_enable_skill_status(https_response->result_body.buf);
                }
            }
        }

        if (http_ret == 200 || http_ret == 404) {
            ret = 0;
        }

        CurlHttpsUnInit(&curl_https);
        ALEXA_PRINT(ALEXA_DEBUG_INFO, "end --- at %lld\n", tickSincePowerOn());

        if (authorization)
            free(authorization);
    }
    return ret;
}

int pco_delete_report(void)
{
    char message_id[RAND_STR_LEN + 1] = {0};
    char *post_body = NULL;
    char *authorization = NULL;
    int http_ret = 0;
    int ret = -1;
    struct curl_slist *list_header = NULL;
    curl_https_t *curl_https = NULL;
    curl_result_t *https_response = NULL;

    asprintf(&post_body, PCO_DELETE_REPORT, alexa_rand_string_36(message_id), g_wiimu_shm->uuid,
             g_wiimu_shm->uuid, g_alexa_access.alexa_access_token);

    asprintf(&authorization, PCO_AUTHORIZATION, g_alexa_access.alexa_access_token);

    printf("post_body is %s \r\n", post_body);

    list_header = curl_slist_append(list_header, authorization);
    if (list_header) {
        curl_slist_append(list_header, "Content-Type: application/json");
        curl_slist_append(list_header, "Cache-Control: no-cache");
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "post data len is %d\n", strlen(post_body));

    curl_https = CurlHttpsInit();
    CurlHttpsSetUrl(curl_https, PCO_ADD_OR_UPDATE_REPORT_URL);
    CurlHttpsSetTimeOut(curl_https, 20L);
    // CurlHttpsSetVerbose(curl_https, 1L);
    CurlHttpsSetHeaderCallBack(curl_https, (void *)header_callback, NULL);
    CurlHttpsSetWriteCallBack(curl_https, (void *)write_data, NULL);
    CurlHttpsSetHeader(curl_https, list_header);
    CurlHttpsSetBody(curl_https, post_body);

    CurlHttpsRun(curl_https, 1);
    http_ret = CurlHttpsGetHttpRetCode(curl_https);

    https_response = CurlHttpsGetResult(curl_https);
    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "http_ret = %d\n", http_ret);
    if (https_response) {
        if (https_response->result_header.buf) {
            ALEXA_PRINT(ALEXA_DEBUG_INFO, "\nresponse header is:\n%s\n\n",
                        https_response->result_header.buf);
        }

        if (https_response->result_body.buf) {
            if (http_ret != 200) {
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "\nresponse body is:\n%s\n\n",
                            https_response->result_body.buf);
            }
            if (http_ret == 200) {
                ret = 0;
                // ret = alexa_result_get_enable_skill_status(https_response->result_body.buf);
                // printf("JiangTao result_body.buf = %s\n", https_response->result_body.buf);
            }
        }
    }

    CurlHttpsUnInit(&curl_https);
    ALEXA_PRINT(ALEXA_DEBUG_INFO, "end --- at %lld\n", tickSincePowerOn());
    if (post_body)
        free(post_body);
    if (authorization)
        free(authorization);

    if (200 != http_ret) {
        return -1;
    }
    return ret;
}

int pco_addorupdate_report(void)
{
    char message_id[RAND_STR_LEN + 1] = {0};
    char *post_body = NULL;
    char *authorization = NULL;
    int http_ret = 0;
    int ret = -1;
    struct curl_slist *list_header = NULL;
    curl_https_t *curl_https = NULL;
    curl_result_t *https_response = NULL;

    asprintf(&post_body, PCO_ADD_OR_UPDATE_REPORT, alexa_rand_string_36(message_id),
             g_wiimu_shm->uuid, g_wiimu_shm->device_name, g_alexa_access.alexa_access_token);

    asprintf(&authorization, PCO_AUTHORIZATION, g_alexa_access.alexa_access_token);

    // printf("post_body is %s \r\n", post_body);

    list_header = curl_slist_append(list_header, authorization);
    if (list_header) {
        curl_slist_append(list_header, "Content-Type: application/json");
        curl_slist_append(list_header, "Cache-Control: no-cache");
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "post data len is %d\n", strlen(post_body));

    curl_https = CurlHttpsInit();
    CurlHttpsSetUrl(curl_https, PCO_ADD_OR_UPDATE_REPORT_URL);
    CurlHttpsSetTimeOut(curl_https, 20L);
    // CurlHttpsSetVerbose(curl_https, 1L);
    CurlHttpsSetHeaderCallBack(curl_https, (void *)header_callback, NULL);
    CurlHttpsSetWriteCallBack(curl_https, (void *)write_data, NULL);
    CurlHttpsSetHeader(curl_https, list_header);
    CurlHttpsSetBody(curl_https, post_body);

    CurlHttpsRun(curl_https, 1);
    http_ret = CurlHttpsGetHttpRetCode(curl_https);

    https_response = CurlHttpsGetResult(curl_https);
    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "http_ret = %d\n", http_ret);
    if (https_response) {
        if (https_response->result_header.buf) {
            ALEXA_PRINT(ALEXA_DEBUG_INFO, "\nresponse header is:\n%s\n\n",
                        https_response->result_header.buf);
        }

        if (https_response->result_body.buf) {
            if (http_ret != 200) {
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "\nresponse body is:\n%s\n\n",
                            https_response->result_body.buf);
            }
            if (http_ret == 200) {
                ret = 0;
                // ret = alexa_result_get_enable_skill_status(https_response->result_body.buf);
                // printf("JiangTao result_body.buf = %s\n", https_response->result_body.buf);
            }
        }
    }

    CurlHttpsUnInit(&curl_https);
    ALEXA_PRINT(ALEXA_DEBUG_INFO, "end --- at %lld\n", tickSincePowerOn());
    if (post_body)
        free(post_body);
    if (authorization)
        free(authorization);

    if (200 != http_ret) {
        return -1;
    }
    return ret;
}

int pco_enable_skill(char *data)
{
    char *post_body = NULL;
    char *authorization = NULL;
    char url[1024];
    char redirectUri[64];
    int http_ret = 0;
    int ret = -1;
    char skillId[128];
    char oAuthCode[512];
    struct curl_slist *list_header = NULL;
    curl_https_t *curl_https = NULL;
    curl_result_t *https_response = NULL;

    if (data && strlen(data) > 0) {

        char *p1 = data;
        char *p2 = NULL;
        p1 += strlen("code=");
        p2 = strstr(p1, ":skill_id=");
        if (p2) {
            p2[0] = '\0';
            strncpy(oAuthCode, p1, sizeof(oAuthCode) - 1);
            p2 += strlen(":skill_id=");
            p1 = p2;

            p2 = strstr(p1, ":redirectUri=");
            if (p2) {
                p2[0] = '\0';
                strncpy(skillId, p1, sizeof(skillId) - 1);
                p2 += strlen(":redirectUri=");
                p1 = p2;
            }
        }
        strncpy(redirectUri, p1, sizeof(redirectUri) - 1);

        asprintf(&post_body, PCO_ENABLE_SKILL_BODY, oAuthCode, redirectUri, skillId);

        asprintf(&authorization, PCO_AUTHORIZATION, g_alexa_access.alexa_access_token);

        snprintf(url, sizeof(url) - 1, PCO_ENABLE_URL, skillId);

        // printf("post_body is %s \r\n", post_body);

        list_header = curl_slist_append(list_header, authorization);
        if (list_header) {
            curl_slist_append(list_header, "Content-Type: application/json");
            curl_slist_append(list_header, "Cache-Control: no-cache");
        }

        ALEXA_PRINT(ALEXA_DEBUG_INFO, "post data len is %d\n", strlen(post_body));

        curl_https = CurlHttpsInit();
        CurlHttpsSetUrl(curl_https, url);
        CurlHttpsSetTimeOut(curl_https, 20L);
        // CurlHttpsSetVerbose(curl_https, 1L);
        CurlHttpsSetHeaderCallBack(curl_https, (void *)header_callback, NULL);
        CurlHttpsSetWriteCallBack(curl_https, (void *)write_data, NULL);
        CurlHttpsSetHeader(curl_https, list_header);
        CurlHttpsSetBody(curl_https, post_body);

        CurlHttpsRun(curl_https, 1);
        http_ret = CurlHttpsGetHttpRetCode(curl_https);

        https_response = CurlHttpsGetResult(curl_https);
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "http_ret = %d\n", http_ret);
        if (https_response) {
            if (https_response->result_header.buf) {
                ALEXA_PRINT(ALEXA_DEBUG_INFO, "\nresponse header is:\n%s\n\n",
                            https_response->result_header.buf);
            }

            char *status;
            char *accountLinkStatus;
            json_object *json_obj = json_tokener_parse(https_response->result_body.buf);
            if (json_obj) {
                status = (char *)JSON_GET_STRING_FROM_OBJECT(json_obj, "status");
                json_object *accoutLink = json_object_object_get(json_obj, "accountLink");
                if (accoutLink) {
                    accountLinkStatus = (char *)JSON_GET_STRING_FROM_OBJECT(accoutLink, "status");
                    if ((strncmp(status, "ENABLED", strlen("ENABLED")) == 0 ||
                         strncmp(status, "ENABLING", strlen("ENABLING")) == 0) &&
                        strncmp(accountLinkStatus, "LINKED", strlen("LINKED")) == 0) {
                        pco_addorupdate_report();
                        ret = 0;
                    }
                }
                if (http_ret == 409) {
                    char *message = (char *)JSON_GET_STRING_FROM_OBJECT(json_obj, "message");
                    if (strncmp(message, "Enablement already exists",
                                strlen("Enablement already exists")) == 0)
                        ret = 0;
                }

                json_object_put(json_obj);
            }
#if 0
            if(https_response->result_body.buf) {
                if(http_ret != 200) {
                    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "\nresponse body is:\n%s\n\n", \
                                https_response->result_body.buf);
                }
                if(http_ret == 200) {
                    ret = alexa_result_get_enable_skill_status(https_response->result_body.buf);
                    if(!ret)
                        ret = pco_addorupdate_report();
                }
            }
#endif
        }
#if 0
        if(200 != http_ret) {
            return -1;
        }
#endif
        CurlHttpsUnInit(&curl_https);
        ALEXA_PRINT(ALEXA_DEBUG_INFO, "end --- at %lld\n", tickSincePowerOn());
        if (post_body)
            free(post_body);
        if (authorization)
            free(authorization);
    }

    return ret;
}

int generate_alexa_token(char *access_token_param)
{
    char *p1 = access_token_param;
    char *p2 = NULL;
#ifdef ASR_3PDA_ENABLE
    char *p3 = NULL;
    char client_id[128] = {0};
    char code_verifier[128] = {0};
#endif
    char code[128] = {0};
    char redirect_uri[256] = {0};
    char *post_body = NULL;
    int ret = 0;
    int http_ret = 0;

    struct curl_slist *list_header = NULL;
    curl_https_t *curl_https = NULL;
    curl_result_t *https_response = NULL;

    /* command=setTokenParams:code=ANWEWMudmHFwcDMRNizv:redirect_uri=https://a000.linkplay.com/alexa.php
     */
    p1 += strlen("code=");
#ifdef ASR_3PDA_ENABLE
    p2 = strstr(p1, ":redirect_uri=");
    if (p2) {
        p2[0] = '\0';
        strncpy(code, p1, sizeof(code) - 1);
        p2 += strlen(":redirect_uri=");
        p1 = p2;

        p3 = strstr(p1, ":client_id=");
        if (p3) {
            p3[0] = '\0';
            strncpy(redirect_uri, p1, sizeof(redirect_uri) - 1);
            p3 += strlen(":client_id=");
            p1 = p3;

            p3 = strstr(p1, ":code_verifier=");
            if (p3) {
                p3[0] = '\0';
                strncpy(client_id, p1, sizeof(client_id) - 1);
                p3 += strlen(":code_verifier=");
                p1 = p3;

                char buffer[8192];
                memset(buffer, 0, sizeof(buffer));
                snprintf(buffer, sizeof(buffer) - 1, "echo -e \"%s\" > %s ; sync", client_id,
                         PDA_CLIENT_REFRESH_TOKEN_CLIENTID);
                system(buffer);

                strncpy(code_verifier, p1, sizeof(code_verifier) - 1);
            }
            printf("client_id = %s, code_verifier = %s\n", client_id, code_verifier);
        } else {
            system("rm -rf " PDA_CLIENT_REFRESH_TOKEN_CLIENTID);
            strncpy(redirect_uri, p1, sizeof(redirect_uri) - 1);
        }
    }
#else
    p2 = strstr(p1, ":redirect_uri=");
    if (p2) {
        p2[0] = '\0';
        strncpy(code, p1, sizeof(code) - 1);
        p2 += strlen(":redirect_uri=");
        p1 = p2;
    }
    strncpy(redirect_uri, p1, sizeof(redirect_uri) - 1);
#endif
    pthread_mutex_lock(&g_alexa_access.lock);
    if (g_alexa_access.alexa_access_code) {
        free(g_alexa_access.alexa_access_code);
        g_alexa_access.alexa_access_code = NULL;
    }
    g_alexa_access.alexa_access_code = strdup(code);

    if (g_alexa_access.redirect_uri) {
        free(g_alexa_access.redirect_uri);
        g_alexa_access.redirect_uri = NULL;
    }
    g_alexa_access.redirect_uri = strdup(redirect_uri);

#ifdef ASR_3PDA_ENABLE
    if ((strlen(client_id) != 0) && (strlen(code_verifier) != 0))
        asprintf(&post_body, ALEXA_LWA_TOKEN_DATA, g_alexa_access.alexa_access_code, client_id,
                 code_verifier, g_alexa_access.redirect_uri);
    else
#endif
        asprintf(&post_body, ALEXA_TOKEN_DATA, g_alexa_access.alexa_access_code,
                 g_alexa_access.client_id, g_alexa_access.client_secret,
                 g_alexa_access.redirect_uri);
    pthread_mutex_unlock(&g_alexa_access.lock);

    printf("post_body is %s \r\n", post_body);

    list_header = curl_slist_append(list_header, ALEXA_TOKEN_HOST);
    if (list_header) {
        curl_slist_append(list_header,
                          "Content-Type: application/x-www-form-urlencoded;charset=UTF-8");
        curl_slist_append(list_header, "Cache-Control: no-cache");
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "post data len is %d\n", strlen(post_body));

    curl_https = CurlHttpsInit();
    CurlHttpsSetUrl(curl_https, ALEXA_TOKEN_URL);
    CurlHttpsSetTimeOut(curl_https, 20L);
    // CurlHttpsSetVerbose(curl_https, 1L);
    CurlHttpsSetHeaderCallBack(curl_https, (void *)header_callback, NULL);
    CurlHttpsSetWriteCallBack(curl_https, (void *)write_data, NULL);
    CurlHttpsSetHeader(curl_https, list_header);
    CurlHttpsSetBody(curl_https, post_body);

    CurlHttpsRun(curl_https, 1);
    http_ret = CurlHttpsGetHttpRetCode(curl_https);

    https_response = CurlHttpsGetResult(curl_https);
    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "http_ret = %d\n", http_ret);
    if (https_response) {
        if (https_response->result_header.buf) {
            ALEXA_PRINT(ALEXA_DEBUG_INFO, "\nresponse header is:\n%s\n\n",
                        https_response->result_header.buf);
        }

        if (https_response->result_body.buf) {
            if (http_ret != 200) {
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "\nresponse body is:\n%s\n\n",
                            https_response->result_body.buf);
            }
            alexa_unauthor_json_parse(https_response->result_body.buf);
            if (http_ret == 200) {
                alexa_result_get_access_token(https_response->result_body.buf);
            }
        }
    }

    if (200 != http_ret) {
#ifndef BAIDU_DUMI
        pthread_mutex_lock(&g_alexa_access.lock);
        if (g_alexa_access.alexa_access_token) {
            free(g_alexa_access.alexa_access_token);
            g_alexa_access.alexa_access_token = NULL;
        }
        g_alexa_access.refresh_time = 0;
        g_alexa_access.expires_in = 0;
        pthread_mutex_unlock(&g_alexa_access.lock);
        ret = -1;
#else
        ret = alexa_result_error(https_response->result_body.buf);
        if (ret != 0) {
            pthread_mutex_lock(&g_alexa_access.lock);
            if (g_alexa_access.alexa_access_token) {
                free(g_alexa_access.alexa_access_token);
                g_alexa_access.alexa_access_token = NULL;
            }
            g_alexa_access.refresh_time = 0;
            g_alexa_access.expires_in = 0;
            pthread_mutex_unlock(&g_alexa_access.lock);
            ret = -1;
        }
#endif
    }

    CurlHttpsUnInit(&curl_https);
    ALEXA_PRINT(ALEXA_DEBUG_INFO, "end --- at %lld\n", tickSincePowerOn());
    if (post_body)
        free(post_body);

    return ret;
}

/*
 * send a http1.0 get authorization request to amazon server
 */
static int alexa_refresh_acess_token(char *refresh_access_token)
{
    int ret = 0;
    int http_ret = 0;
    char *post_body = NULL;
    struct curl_slist *list_header = NULL;
    curl_https_t *curl_https = NULL;
    curl_result_t *https_response = NULL;

    if (!refresh_access_token) {
        return -1;
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "start +++ at %lld\n", tickSincePowerOn());
    pthread_mutex_lock(&g_alexa_access.lock);
#ifdef ASR_3PDA_ENABLE
    if (strlen(client_id) != 0)
        asprintf(&post_body, ALEXA_LWA_REFEASH_TOKEN_DATA, refresh_access_token, client_id);
    else
#endif
        asprintf(&post_body, ALEXA_REFEASH_TOKEN_DATA, refresh_access_token,
                 g_alexa_access.client_id, g_alexa_access.client_secret);
    pthread_mutex_unlock(&g_alexa_access.lock);
#ifndef SECURITY_IMPROVE
    ALEXA_PRINT(ALEXA_DEBUG_ERR, "\npost data is:\n%s\n", post_body);
#endif
    if (!post_body) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "post data malloc failed\n");
        return -1;
    }

    list_header = curl_slist_append(list_header, ALEXA_LOGIN_HOST);
    if (list_header) {
        curl_slist_append(list_header,
                          "Content-Type: application/x-www-form-urlencoded;charset=UTF-8");
        curl_slist_append(list_header, "Cache-Control: no-cache");
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "post data len is %d\n", strlen(post_body));
    curl_https = CurlHttpsInit();
    CurlHttpsSetUrl(curl_https, ALEXA_AUTHO2_TOKEN);
    CurlHttpsSetTimeOut(curl_https, 20L);
    // CurlHttpsSetVerbose(curl_https, 1L);
    CurlHttpsSetHeaderCallBack(curl_https, (void *)header_callback, NULL);
    CurlHttpsSetWriteCallBack(curl_https, (void *)write_data, NULL);
    CurlHttpsSetHeader(curl_https, list_header);
    CurlHttpsSetBody(curl_https, post_body);
    CurlHttpsRun(curl_https, 1);
    http_ret = CurlHttpsGetHttpRetCode(curl_https);

    https_response = CurlHttpsGetResult(curl_https);
    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "http_ret = %d\n", http_ret);
    if (https_response) {
        if (https_response->result_header.buf) {
            ALEXA_PRINT(ALEXA_DEBUG_INFO, "\nresponse header is:\n%s\n\n",
                        https_response->result_header.buf);
        }

        if (https_response->result_body.buf) {
            if (http_ret != 200) {
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "\nresponse body is:\n%s\n\n",
                            https_response->result_body.buf);
            }
            alexa_unauthor_json_parse(https_response->result_body.buf);
            if (http_ret == 200) {
                alexa_result_get_access_token(https_response->result_body.buf);
            }
        }
    }

    if (200 != http_ret) {
#ifndef BAIDU_DUMI
        pthread_mutex_lock(&g_alexa_access.lock);
        if (g_alexa_access.alexa_access_token) {
            free(g_alexa_access.alexa_access_token);
            g_alexa_access.alexa_access_token = NULL;
        }
        g_alexa_access.refresh_time = 0;
        g_alexa_access.expires_in = 0;
        pthread_mutex_unlock(&g_alexa_access.lock);
        ret = -1;
#else
        ret = alexa_result_error(https_response->result_body.buf);
        if (ret != 0) {
            pthread_mutex_lock(&g_alexa_access.lock);
            if (g_alexa_access.alexa_access_token) {
                free(g_alexa_access.alexa_access_token);
                g_alexa_access.alexa_access_token = NULL;
            }
            g_alexa_access.refresh_time = 0;
            g_alexa_access.expires_in = 0;
            pthread_mutex_unlock(&g_alexa_access.lock);
            ret = -1;
        }
#endif
    }

    if (post_body) {
        free(post_body);
    }

    CurlHttpsUnInit(&curl_https);

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "end --- at %lld\n", tickSincePowerOn());

    return ret;
}

static int lifepod_refresh_acess_token(char *refresh_access_token)
{
    int ret = 0;
    int http_ret = 0;
    char *post_body = NULL;
    char *encode_refresh_token = NULL;
    struct curl_slist *list_header = NULL;
    curl_https_t *curl_https = NULL;
    curl_result_t *https_response = NULL;

    if (!refresh_access_token) {
        return -1;
    }

    encode_refresh_token = curl_escape(refresh_access_token, strlen(refresh_access_token));
    if (!encode_refresh_token) {
        return -1;
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "start +++ at %lld\n", tickSincePowerOn());
    pthread_mutex_lock(&g_lifepod_access.lock);
    asprintf(&post_body, LIFEPOD_REFEASH_TOKEN_DATA, encode_refresh_token,
             g_lifepod_access.client_id);
    pthread_mutex_unlock(&g_lifepod_access.lock);
#ifndef SECURITY_IMPROVE
    ALEXA_PRINT(ALEXA_DEBUG_ERR, "\npost data is:\n%s\n", post_body);
#endif
    if (!post_body) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "post data malloc failed\n");
        return -1;
    }

    list_header = curl_slist_append(list_header, ALEXA_LOGIN_HOST);
    if (list_header) {
        curl_slist_append(list_header,
                          "Content-Type: application/x-www-form-urlencoded;charset=UTF-8");
        curl_slist_append(list_header, "Cache-Control: no-cache");
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "post data len is %d\n", strlen(post_body));
    curl_https = CurlHttpsInit();
    CurlHttpsSetUrl(curl_https, LIFEPOD_TOKEN_URL);
    CurlHttpsSetTimeOut(curl_https, 20L);
    // CurlHttpsSetVerbose(curl_https, 1L);
    CurlHttpsSetHeaderCallBack(curl_https, (void *)header_callback, NULL);
    CurlHttpsSetWriteCallBack(curl_https, (void *)write_data, NULL);
    CurlHttpsSetHeader(curl_https, list_header);
    CurlHttpsSetBody(curl_https, post_body);
    CurlHttpsRun(curl_https, 1);
    http_ret = CurlHttpsGetHttpRetCode(curl_https);

    https_response = CurlHttpsGetResult(curl_https);
    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "http_ret = %d\n", http_ret);
    if (https_response) {
        if (https_response->result_header.buf) {
            ALEXA_PRINT(ALEXA_DEBUG_INFO, "\nresponse header is:\n%s\n\n",
                        https_response->result_header.buf);
        }

        if (https_response->result_body.buf) {
            if (http_ret != 200) {
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "\nresponse body is:\n%s\n\n",
                            https_response->result_body.buf);
            }
            // alexa_unauthor_json_parse(https_response->result_body.buf);
            if (http_ret == 200) {
                lifepod_result_get_access_token(https_response->result_body.buf);
            }
        }
    }

    if (200 != http_ret) {
        pthread_mutex_lock(&g_lifepod_access.lock);
        if (g_lifepod_access.alexa_access_token) {
            free(g_lifepod_access.alexa_access_token);
            g_lifepod_access.alexa_access_token = NULL;
        }
        g_lifepod_access.refresh_time = 0;
        g_lifepod_access.expires_in = 0;
        pthread_mutex_unlock(&g_lifepod_access.lock);
        ret = -1;
    }

    if (post_body) {
        free(post_body);
    }

    if (encode_refresh_token) {
        curl_free(encode_refresh_token);
    }

    CurlHttpsUnInit(&curl_https);

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "end --- at %lld\n", tickSincePowerOn());

    return ret;
}

/*
 *  The access token need to refresh per 3600 seconds
 *  We fresh earlier for make the access token usable
 *  make a request to amazon server per 2400 seconds
 */
void *alexa_fresh_access_token_proc(void *arg)
{
    long long cur_time = 0;

    while (1) {
        cur_time = tickSincePowerOn();
        if (g_wiimu_shm->internet == 1) {
            if ((access(ALEXA_TOKEN_PATH, 0)) == 0) { // if access_token exist
                if (((cur_time >= (g_alexa_access.expires_in + g_alexa_access.refresh_time)) &&
                     (g_alexa_access.refresh_time != 0)) ||
                    (!g_alexa_access.alexa_access_token)) {
                    char *alexa_refresh_token = NULL;
                    alexa_refresh_token = AlexaGetRefreshToken();
                    alexa_refresh_acess_token(alexa_refresh_token);
                    if (alexa_refresh_token) {
                        free(alexa_refresh_token);
                    }
                }
            }
        } else {
            pthread_mutex_lock(&g_alexa_access.lock);
            if (g_alexa_access.alexa_access_token) {
                free(g_alexa_access.alexa_access_token);
                g_alexa_access.alexa_access_token = NULL;
            }
            g_alexa_access.refresh_time = 0;
            g_alexa_access.expires_in = 0;
            pthread_mutex_unlock(&g_alexa_access.lock);
        }

        sleep(1);
    }

    return 0; // pthread_exit(0);
}

void *lifepod_fresh_access_token_proc(void *arg)
{
    long long cur_time = 0;

    while (1) {
        cur_time = tickSincePowerOn();
        if (g_wiimu_shm->internet == 1) {
            if ((access(LIFEPOD_TOKEN_PATH, 0)) == 0) { // if access_token exist
                if (((cur_time >= (g_lifepod_access.expires_in + g_lifepod_access.refresh_time)) &&
                     (g_lifepod_access.refresh_time != 0)) ||
                    (!g_lifepod_access.alexa_access_token)) {
                    char *lifepod_refresh_token = NULL;
                    lifepod_refresh_token = LifepodGetRefreshToken();
                    lifepod_refresh_acess_token(lifepod_refresh_token);
                    if (lifepod_refresh_token) {
                        free(lifepod_refresh_token);
                    }
                }
            }
        } else {
            pthread_mutex_lock(&g_lifepod_access.lock);
            if (g_lifepod_access.alexa_access_token) {
                free(g_lifepod_access.alexa_access_token);
                g_lifepod_access.alexa_access_token = NULL;
            }
            g_lifepod_access.refresh_time = 0;
            g_lifepod_access.expires_in = 0;
            pthread_mutex_unlock(&g_lifepod_access.lock);
        }

        sleep(1);
    }

    return 0; // pthread_exit(0);
}

static int get_alexa_info_from_server(char *project_name, char *out_path)
{
// not verified yet
#if 1
    return -1;
#else
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "curl https://a000.linkplay.com/alexa_profile.php?prj=%s"
                               " -k --connect-timeout 15 --retry 2 -Y 1000 -y 10 > %s\n",
             project_name, out_path);
    printf("run[%s]\n", cmd);
    system(cmd);
    if (access(out_path, R_OK) == 0 && get_file_len(out_path) > 0)
        return 0;
    return -1;
#endif
}

int remove_cfg(void)
{
    char buff[64] = {0};

    if ((access(ALEXA_TOKEN_PATH, 0)) == 0) { // if access_token not exist
        memset(buff, 0x0, sizeof(buff));
        snprintf(buff, sizeof(buff), "rm -rf %s && sync", ALEXA_TOKEN_PATH);
        system(buff);
        snprintf(buff, sizeof(buff), "rm -rf %s && sync", ALEXA_TOKEN_PATH_SECURE);
        system(buff);
    } else {
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "the %s broken/not exist\n", ALEXA_TOKEN_PATH);
    }

    if ((access("/vendor/wiimu/alexa_alarm", 0)) == 0) { // if access_token not exist
        if ((access("/vendor/wiimu/alexa_alarm_NamedTimerAndReminder", 0)) == 0) {
            system("rm -rf /vendor/wiimu/alexa_alarm_NamedTimerAndReminder & sync");
        }
        if ((access("/tmp/NamedTimerAndReminder_Audio", 0)) == 0) {
            system("rm -rf /tmp/NamedTimerAndReminder_Audio & sync");
        }
        system("rm -rf /vendor/wiimu/alexa_alarm && sync");
        killPidbyName("alexa_alert");
        alert_reset();
        system("alexa_alert &");
    }

    return 0;
}

int alexa_auth_read_cfg(int flag)
{
    int total_bytes = 0;
    char *tmp = NULL;
    int string_len = 0;
    char *decrypt_string = NULL;
    char *decrypt_string1 = NULL;
    lp_aes_cbc_ctx_t aes_cbc_param;

    memset(&aes_cbc_param, 0, sizeof(lp_aes_cbc_ctx_t));
    memcpy(aes_cbc_param.key, key, sizeof(aes_cbc_param.key));
    aes_cbc_param.key_bits = 16 * 8;
    aes_cbc_param.padding_mode = LP_CIPHER_PADDING_MODE_ZERO;
    aes_cbc_param.is_base64_format = 1;
    FILE *_file = fopen(ALEXA_TOKEN_PATH, "rb");
    if (!_file) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "Open file '%s' error\n", ALEXA_TOKEN_PATH);
        return -1;
    } else {
        fseek(_file, 0, SEEK_END);
        total_bytes = (int)ftell(_file);
        if (total_bytes <= 0) {
            ALEXA_PRINT(ALEXA_DEBUG_ERR, "ftell '%s' error\n", ALEXA_TOKEN_PATH);
            fclose(_file);
            return -1;
        }
        fseek(_file, 0, SEEK_SET);
        char *token = (char *)calloc(1, total_bytes + 1);
        if (token) {
            int ret = fread(token, 1, total_bytes, _file);
            token[total_bytes] = '\0';
            if (ret > 0) {
                tmp = strchr(token, ':');
                if (!tmp) {
                    pthread_mutex_lock(&g_alexa_access.lock);
                    if (g_alexa_access.alexa_refresh_token) {
                        free(g_alexa_access.alexa_refresh_token);
                        g_alexa_access.alexa_refresh_token = NULL;
                    }
                    if (g_alexa_access.alexa_access_token) {
                        free(g_alexa_access.alexa_access_token);
                        g_alexa_access.alexa_access_token = NULL;
                    }
                    pthread_mutex_unlock(&g_alexa_access.lock);

                    remove_cfg();
                    fclose(_file);
                    free(token);
                    return -1;
                }

                tmp = token;
                while (*tmp != ':') {
                    tmp++;
                }

                pthread_mutex_lock(&g_alexa_access.lock);
                if (flag) {
                    if (g_alexa_access.alexa_access_token) {
                        free(g_alexa_access.alexa_access_token);
                        g_alexa_access.alexa_access_token = NULL;
                    }
                    string_len = (int)(tmp - token + 1);
                    if (string_len > 0) {
                        g_alexa_access.alexa_access_token = (char *)calloc(1, (tmp - token + 1));
                        strncpy(g_alexa_access.alexa_access_token, token, tmp - token);
                        g_alexa_access.alexa_access_token[tmp - token] = '\0';
                        if (access(ALEXA_TOKEN_PATH_SECURE, 0) == 0) {
                            decrypt_string =
                                (char *)calloc(1, strlen(g_alexa_access.alexa_access_token));
                            if (decrypt_string) {
                                lp_aes_cbc_decrypt(
                                    (unsigned char *)g_alexa_access.alexa_access_token,
                                    strlen(g_alexa_access.alexa_access_token),
                                    (unsigned char *)decrypt_string,
                                    strlen(g_alexa_access.alexa_access_token), &aes_cbc_param);
                                ALEXA_PRINT(ALEXA_DEBUG_INFO, "decrypt_access_token is %s \r\n",
                                            decrypt_string);
                                if (g_alexa_access.alexa_access_token) {
                                    free(g_alexa_access.alexa_access_token);
                                    g_alexa_access.alexa_access_token = NULL;
                                }
                                g_alexa_access.alexa_access_token = decrypt_string;
                            }
                        }
                    }
                    g_alexa_access.refresh_time = tickSincePowerOn();
                    g_alexa_access.expires_in = DEFAULT_REFRESH_TIME;
                }
                if (g_alexa_access.alexa_refresh_token) {
                    free(g_alexa_access.alexa_refresh_token);
                    g_alexa_access.alexa_refresh_token = NULL;
                }
                string_len = (int)((token + ret) - tmp);
                if (string_len > 0) {
                    g_alexa_access.alexa_refresh_token = (char *)calloc(1, ((token + ret) - tmp));
                    strncpy(g_alexa_access.alexa_refresh_token, tmp + 1, (token + ret) - tmp - 1);
                    if (access(ALEXA_TOKEN_PATH_SECURE, 0) == 0) {
                        decrypt_string1 =
                            (char *)calloc(1, strlen(g_alexa_access.alexa_refresh_token));
                        if (decrypt_string1) {
                            lp_aes_cbc_decrypt((unsigned char *)g_alexa_access.alexa_refresh_token,
                                               strlen(g_alexa_access.alexa_refresh_token),
                                               (unsigned char *)decrypt_string1,
                                               strlen(g_alexa_access.alexa_refresh_token),
                                               &aes_cbc_param);
                            ALEXA_PRINT(ALEXA_DEBUG_INFO, "decrypt_string1 is %s \r\n",
                                        decrypt_string1);

                            if (g_alexa_access.alexa_refresh_token) {
                                free(g_alexa_access.alexa_refresh_token);
                                g_alexa_access.alexa_refresh_token = NULL;
                            }
                            g_alexa_access.alexa_refresh_token = decrypt_string1;
                        }
                    }
                }
                pthread_mutex_unlock(&g_alexa_access.lock);
            }

            free(token);
        }
        fclose(_file);
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "\n alexa_refresh_token is:\n%s\n",
                g_alexa_access.alexa_refresh_token);
    return 0;
}

int lifepod_auth_read_cfg(int flag)
{
    int total_bytes = 0;
    char *tmp = NULL;
    int string_len = 0;
    char *decrypt_string = NULL;
    char *decrypt_string1 = NULL;
    lp_aes_cbc_ctx_t aes_cbc_param;

    memset(&aes_cbc_param, 0, sizeof(lp_aes_cbc_ctx_t));
    memcpy(aes_cbc_param.key, key, sizeof(aes_cbc_param.key));
    aes_cbc_param.key_bits = 16 * 8;
    aes_cbc_param.padding_mode = LP_CIPHER_PADDING_MODE_ZERO;
    aes_cbc_param.is_base64_format = 1;
    FILE *_file = fopen(LIFEPOD_TOKEN_PATH, "rb");
    if (!_file) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "Open file '%s' error\n", LIFEPOD_TOKEN_PATH);
        return -1;
    } else {
        fseek(_file, 0, SEEK_END);
        total_bytes = (int)ftell(_file);
        if (total_bytes <= 0) {
            ALEXA_PRINT(ALEXA_DEBUG_ERR, "ftell '%s' error\n", LIFEPOD_TOKEN_PATH);
            fclose(_file);
            return -1;
        }
        fseek(_file, 0, SEEK_SET);
        char *token = (char *)calloc(1, total_bytes + 1);
        if (token) {
            int ret = fread(token, 1, total_bytes, _file);
            token[total_bytes] = '\0';
            if (ret > 0) {
                tmp = strchr(token, ':');
                if (!tmp) {
                    pthread_mutex_lock(&g_lifepod_access.lock);
                    if (g_lifepod_access.alexa_refresh_token) {
                        free(g_lifepod_access.alexa_refresh_token);
                        g_lifepod_access.alexa_refresh_token = NULL;
                    }
                    if (g_lifepod_access.alexa_access_token) {
                        free(g_lifepod_access.alexa_access_token);
                        g_lifepod_access.alexa_access_token = NULL;
                    }
                    pthread_mutex_unlock(&g_lifepod_access.lock);

                    // remove_cfg();
                    fclose(_file);
                    free(token);
                    return -1;
                }

                tmp = token;
                while (*tmp != ':') {
                    tmp++;
                }

                pthread_mutex_lock(&g_lifepod_access.lock);
                if (flag) {
                    if (g_lifepod_access.alexa_access_token) {
                        free(g_lifepod_access.alexa_access_token);
                        g_lifepod_access.alexa_access_token = NULL;
                    }
                    string_len = (int)(tmp - token + 1);
                    if (string_len > 0) {
                        g_lifepod_access.alexa_access_token = (char *)calloc(1, (tmp - token + 1));
                        strncpy(g_lifepod_access.alexa_access_token, token, tmp - token);
                        g_lifepod_access.alexa_access_token[tmp - token] = '\0';
                        if (access(LIFEPOD_TOKEN_PATH_SECURE, 0) == 0) {
                            decrypt_string =
                                (char *)calloc(1, strlen(g_lifepod_access.alexa_access_token));
                            if (decrypt_string) {
                                lp_aes_cbc_decrypt(
                                    (unsigned char *)g_lifepod_access.alexa_access_token,
                                    strlen(g_lifepod_access.alexa_access_token),
                                    (unsigned char *)decrypt_string,
                                    strlen(g_lifepod_access.alexa_access_token), &aes_cbc_param);
                                ALEXA_PRINT(ALEXA_DEBUG_INFO, "decrypt_access_token is %s \r\n",
                                            decrypt_string);
                                if (g_lifepod_access.alexa_access_token) {
                                    free(g_lifepod_access.alexa_access_token);
                                    g_lifepod_access.alexa_access_token = NULL;
                                }
                                g_lifepod_access.alexa_access_token = decrypt_string;
                            }
                        }
                    }
                    g_lifepod_access.refresh_time = tickSincePowerOn();
                    g_lifepod_access.expires_in = DEFAULT_REFRESH_TIME;
                }
                if (g_lifepod_access.alexa_refresh_token) {
                    free(g_lifepod_access.alexa_refresh_token);
                    g_lifepod_access.alexa_refresh_token = NULL;
                }
                string_len = (int)((token + ret) - tmp);
                if (string_len > 0) {
                    g_lifepod_access.alexa_refresh_token = (char *)calloc(1, ((token + ret) - tmp));
                    strncpy(g_lifepod_access.alexa_refresh_token, tmp + 1, (token + ret) - tmp - 1);
                    if (access(LIFEPOD_TOKEN_PATH_SECURE, 0) == 0) {
                        decrypt_string1 =
                            (char *)calloc(1, strlen(g_lifepod_access.alexa_refresh_token));
                        if (decrypt_string1) {
                            lp_aes_cbc_decrypt(
                                (unsigned char *)g_lifepod_access.alexa_refresh_token,
                                strlen(g_lifepod_access.alexa_refresh_token),
                                (unsigned char *)decrypt_string1,
                                strlen(g_lifepod_access.alexa_refresh_token), &aes_cbc_param);
                            ALEXA_PRINT(ALEXA_DEBUG_INFO, "decrypt_string1 is %s \r\n",
                                        decrypt_string1);

                            if (g_lifepod_access.alexa_refresh_token) {
                                free(g_lifepod_access.alexa_refresh_token);
                                g_lifepod_access.alexa_refresh_token = NULL;
                            }
                            g_lifepod_access.alexa_refresh_token = decrypt_string1;
                        }
                    }
                }
                pthread_mutex_unlock(&g_lifepod_access.lock);
            }

            free(token);
        }
        fclose(_file);
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "\n alexa_refresh_token is:\n%s\n",
                g_lifepod_access.alexa_refresh_token);
    return 0;
}

int LifepodLogInWithAuthcode(char *data)
{
    char *p1 = data;
    char *p2 = NULL;
    char *p3 = NULL;
    char code_verifier[128] = {0};
    char client_id[128] = {0};
    char code[128] = {0};
    char redirect_uri[256] = {0};
    char *post_body = NULL;
    int ret = 0;
    int http_ret = 0;
    int encode = 1;

    struct curl_slist *list_header = NULL;
    curl_https_t *curl_https = NULL;
    curl_result_t *https_response = NULL;
    char *url_encode_refresh_token = NULL;
    /* command=setLifepodAuthCode:code=:redirect_uri=:client_id=:code_verifier= */
    p1 += strlen("code=");

    p2 = strstr(p1, ":redirect_uri=");
    if (p2) {
        p2[0] = '\0';
        strncpy(code, p1, sizeof(code) - 1);
        p2 += strlen(":redirect_uri=");
        p1 = p2;

        p3 = strstr(p1, ":client_id=");
        if (p3) {
            p3[0] = '\0';
            strncpy(redirect_uri, p1, sizeof(redirect_uri) - 1);
            p3 += strlen(":client_id=");
            p1 = p3;

            p3 = strstr(p1, ":code_verifier=");
            if (p3) {
                p3[0] = '\0';
                strncpy(client_id, p1, sizeof(client_id) - 1);
                p3 += strlen(":code_verifier=");
                p1 = p3;
            }

            p3 = strstr(p1, ":os");
            if (p3) {
                p3[0] = '\0';
                encode = 0;
            }
            strncpy(code_verifier, p1, sizeof(code_verifier) - 1);
            printf("client_id = %s, code_verifier = %s, code = %s, redirect_uri= %s\n", client_id,
                   code_verifier, code, redirect_uri);
        }
    } else {
        return -1;
    }

    pthread_mutex_lock(&g_lifepod_access.lock);

    if (!encode)
        url_encode_refresh_token = strdup(code);
    else
        url_encode_refresh_token = curl_escape(code, strlen(code));

    if (g_lifepod_access.alexa_access_code) {
        free(g_lifepod_access.alexa_access_code);
        g_lifepod_access.alexa_access_code = NULL;
    }
    g_lifepod_access.alexa_access_code = strdup(url_encode_refresh_token);
    if (!encode)
        free(url_encode_refresh_token);
    else
        curl_free(url_encode_refresh_token);

    if (g_lifepod_access.redirect_uri) {
        free(g_lifepod_access.redirect_uri);
        g_lifepod_access.redirect_uri = NULL;
    }
    g_lifepod_access.redirect_uri = strdup(redirect_uri);

    if ((strlen(client_id) != 0) && (strlen(code_verifier) != 0))
        asprintf(&post_body, ALEXA_LWA_TOKEN_DATA, g_lifepod_access.alexa_access_code, client_id,
                 code_verifier, g_lifepod_access.redirect_uri);

    pthread_mutex_unlock(&g_lifepod_access.lock);

    printf("post_body is %s \r\n", post_body);

    list_header = curl_slist_append(list_header, ALEXA_TOKEN_HOST);
    if (list_header) {
        curl_slist_append(list_header,
                          "Content-Type: application/x-www-form-urlencoded;charset=UTF-8");
        curl_slist_append(list_header, "Cache-Control: no-cache");
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "post data len is %d\n", strlen(post_body));

    curl_https = CurlHttpsInit();
    CurlHttpsSetUrl(curl_https, LIFEPOD_TOKEN_URL);
    CurlHttpsSetTimeOut(curl_https, 20L);
    // CurlHttpsSetVerbose(curl_https, 1L);
    CurlHttpsSetHeaderCallBack(curl_https, (void *)header_callback, NULL);
    CurlHttpsSetWriteCallBack(curl_https, (void *)write_data, NULL);
    CurlHttpsSetHeader(curl_https, list_header);
    CurlHttpsSetBody(curl_https, post_body);

    CurlHttpsRun(curl_https, 1);
    http_ret = CurlHttpsGetHttpRetCode(curl_https);

    https_response = CurlHttpsGetResult(curl_https);
    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "http_ret = %d\n", http_ret);
    if (https_response) {
        if (https_response->result_header.buf) {
            ALEXA_PRINT(ALEXA_DEBUG_INFO, "\nresponse header is:\n%s\n\n",
                        https_response->result_header.buf);
        }

        if (https_response->result_body.buf) {
            if (http_ret != 200) {
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "\nresponse body is:\n%s\n\n",
                            https_response->result_body.buf);
            }
            // alexa_unauthor_json_parse(https_response->result_body.buf);
            if (http_ret == 200) {
                lifepod_result_get_access_token(https_response->result_body.buf);
            }
        }
    }

    if (200 != http_ret) {
        pthread_mutex_lock(&g_lifepod_access.lock);
        if (g_lifepod_access.alexa_access_token) {
            free(g_lifepod_access.alexa_access_token);
            g_lifepod_access.alexa_access_token = NULL;
        }
        g_lifepod_access.refresh_time = 0;
        g_lifepod_access.expires_in = 0;
        pthread_mutex_unlock(&g_lifepod_access.lock);
        ret = -1;
    }

    CurlHttpsUnInit(&curl_https);
    ALEXA_PRINT(ALEXA_DEBUG_INFO, "end --- at %lld\n", tickSincePowerOn());
    if (post_body)
        free(post_body);
    return ret;
}

int AlexaLogInWithParam(char *data)
{
    int ret = -1;
    char *save_buf = NULL;
    char *encrypt_access_token = NULL;
    char *encrypt_refresh_token = NULL;
    char *decrypt_access_token = NULL;
    char *decrypt_refresh_token = NULL;
    lp_aes_cbc_ctx_t aes_cbc_param;

    memset(&aes_cbc_param, 0, sizeof(lp_aes_cbc_ctx_t));
    memcpy(aes_cbc_param.key, key, sizeof(aes_cbc_param.key));
    aes_cbc_param.key_bits = 16 * 8;
    aes_cbc_param.padding_mode = LP_CIPHER_PADDING_MODE_ZERO;
    aes_cbc_param.is_base64_format = 1;

    // ALEXA_PRINT(ALEXA_DEBUG_TRACE, "login start ++++++\n");
    printf("data is %s , strlen(data) is %d \r\n", data, strlen(data));
    if (data && strlen(data) > 0) {
        printf("************************************ \r\n");

        pthread_mutex_lock(&g_alexa_access.lock);
        if (g_alexa_access.alexa_refresh_token) {
            free(g_alexa_access.alexa_refresh_token);
            g_alexa_access.alexa_refresh_token = NULL;
        }

        if (g_alexa_access.alexa_access_token) {
            free(g_alexa_access.alexa_access_token);
            g_alexa_access.alexa_access_token = NULL;
        }
        pthread_mutex_unlock(&g_alexa_access.lock);

        printf("************************************ \r\n");
        ret = generate_alexa_token(data);

        if (ret == 0 && g_alexa_access.alexa_access_token) {
            pthread_mutex_lock(&g_alexa_access.lock);

            encrypt_access_token = (char *)calloc(1, strlen(g_alexa_access.alexa_access_token) * 2);
            if (encrypt_access_token) {
                lp_aes_cbc_encrypt((unsigned char *)g_alexa_access.alexa_access_token,
                                   strlen(g_alexa_access.alexa_access_token),
                                   (unsigned char *)encrypt_access_token,
                                   strlen(g_alexa_access.alexa_access_token) * 2, &aes_cbc_param);
            } else {
                fprintf(stderr, "%s: malloc encrypt alexa_access_token fail\n", __func__);
            }
            encrypt_refresh_token =
                (char *)calloc(1, strlen(g_alexa_access.alexa_refresh_token) * 2);
            if (encrypt_refresh_token) {
                lp_aes_cbc_encrypt((unsigned char *)g_alexa_access.alexa_refresh_token,
                                   strlen(g_alexa_access.alexa_refresh_token),
                                   (unsigned char *)encrypt_refresh_token,
                                   strlen(g_alexa_access.alexa_refresh_token) * 2, &aes_cbc_param);
            } else {
                fprintf(stderr, "%s: malloc encrypt alexa_refresh_token fail\n", __func__);
            }
            // printf("encrypt_refresh_token is %s \r\n encrypt_access_token %s \r\n",
            //        encrypt_refresh_token ? encrypt_refresh_token : "", encrypt_access_token ?
            //        encrypt_access_token : "");

            /*
            printf("------------------------------------------------------ \r\n");
            decrypt_access_token = aes_decode_128_key(encrypt_access_token,
            strlen(encrypt_access_token), key);
            printf("decrypt_access_token is %s \r\n", decrypt_access_token);
            decrypt_refresh_token = aes_decode_128_key(encrypt_refresh_token,
            strlen(encrypt_refresh_token), key);
            printf("decrypt_access_token is %s \r\n", decrypt_refresh_token);
            printf("------------------------------------------------------ \r\n");
            */

            asprintf(&save_buf, "%s:%s", encrypt_access_token ? encrypt_access_token : "",
                     encrypt_refresh_token ? encrypt_refresh_token : "");
            ALEXA_PRINT(ALEXA_DEBUG_INFO, "strlen(save_buf) is %s \r\n", save_buf);
            alexa_save_cfg(save_buf, strlen(save_buf), 1);

            pthread_mutex_unlock(&g_alexa_access.lock);
            printf("------------------------------------------------------ \r\n");
        }
    }

    if (ret != 0) {
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "login failed: %s -----\n",
                    ((ret == -2) ? "open file error" : "input error"));
    }

    if (encrypt_access_token)
        free(encrypt_access_token);

    if (encrypt_refresh_token)
        free(encrypt_refresh_token);

    if (decrypt_access_token)
        free(decrypt_access_token);

    if (decrypt_refresh_token)
        free(decrypt_refresh_token);

    if (save_buf)
        free(save_buf);

    return ret;
}

/*
 * create a thread to get alexa access token and refreshtoken freom amazon server
 */
int AlexaAuthorizationInit(void)
{
    int ret = 0;
    pthread_t fresh_access_token_pid;

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "start at %lld\n", tickSincePowerOn());

    pthread_mutex_init(&g_alexa_access.lock, NULL);
    pthread_mutex_lock(&g_alexa_access.lock);
    g_alexa_access.alexa_access_token = NULL;
    g_alexa_access.alexa_refresh_token = NULL;
    g_alexa_access.refresh_time = 0;
    g_alexa_access.expires_in = 0;
    pthread_mutex_unlock(&g_alexa_access.lock);

    AlexaGetDeviceInfo();
#ifdef S11_EVB_EUFY_DOT
    if (g_wiimu_shm->asr_tts_crash) {
        fprintf(stderr, "[Eufy] asr_tts crash, restarting\n");
        ret = alexa_auth_read_cfg(0);
        g_wiimu_shm->asr_tts_crash = 0;
    }
#else
#ifdef BAIDU_DUMI
    ret = alexa_auth_read_cfg(1);
#else
    ret = alexa_auth_read_cfg(0);
#endif
#endif
#ifndef LIFEPOD_CLOUD
    ret = pthread_create(&fresh_access_token_pid, NULL, alexa_fresh_access_token_proc, NULL);
    if (ret) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "pthread_create alexa_fresh_access_token_proc failed\n");
        return -1;
    }
#endif
    ALEXA_PRINT(ALEXA_DEBUG_INFO, "end at %lld\n", tickSincePowerOn());

    return 0;
}

int LifepodAuthorizationInit(void)
{
    int ret = 0;
    pthread_t fresh_access_token_pid;

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "start at %lld\n", tickSincePowerOn());

    pthread_mutex_init(&g_lifepod_access.lock, NULL);
    pthread_mutex_lock(&g_lifepod_access.lock);
    g_lifepod_access.alexa_access_token = NULL;
    g_lifepod_access.alexa_refresh_token = NULL;
    g_lifepod_access.refresh_time = 0;
    g_lifepod_access.expires_in = 0;
    pthread_mutex_unlock(&g_lifepod_access.lock);

    LifepodGetDeviceInfo();

    ret = lifepod_auth_read_cfg(0);

    ret = pthread_create(&fresh_access_token_pid, NULL, lifepod_fresh_access_token_proc, NULL);
    if (ret) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "pthread_create lifepod_fresh_access_token_proc failed\n");
        return -1;
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "end at %lld\n", tickSincePowerOn());

    return 0;
}

int LifepodAuthorizationLogin(char *data)
{
    int ret = -1;

    if (data && strlen(data) > 0) {
        {
            pthread_mutex_lock(&g_alexa_access.lock);
            if (g_alexa_access.alexa_refresh_token) {
                free(g_alexa_access.alexa_refresh_token);
                g_alexa_access.alexa_refresh_token = NULL;
            }
            g_alexa_access.alexa_refresh_token = strdup(refreshToken);
            g_alexa_access.alexa_access_token = strdup(data);
            // printf("JiangTao g_alexa_access refreshToken = %s, accessToken = %s\n", refreshToken,
            // data);
            ret = alexa_save_cfg(data, strlen(data), 0);
            if (ret != 0) {
                ret = -2;
            }
            pthread_mutex_unlock(&g_alexa_access.lock);
        }
    }
    return ret;
}

int AlexaAuthorizationLogIn(char *data)
{
    int ret = -1;
    char *tmp = NULL;
    int less_len = 0;

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "login start ++++++\n");

    if (data && strlen(data) > 0) {
        tmp = strchr(data, ':');
        less_len = tmp - data;
        if (tmp && less_len > 1) {
            pthread_mutex_lock(&g_alexa_access.lock);
#ifndef BAIDU_DUMI
            if (g_alexa_access.alexa_refresh_token) {
                free(g_alexa_access.alexa_refresh_token);
                g_alexa_access.alexa_refresh_token = NULL;
            }
#else
            if (g_alexa_access.alexa_access_token) {
                free(g_alexa_access.alexa_access_token);
                g_alexa_access.alexa_access_token = NULL;
            }
            g_alexa_access.alexa_access_token = (char *)calloc(1, (tmp - data + 1));
            strncpy(g_alexa_access.alexa_access_token, data, tmp - data);
            g_alexa_access.alexa_access_token[tmp - data] = '\0';
            g_alexa_access.refresh_time = tickSincePowerOn();
            g_alexa_access.expires_in = DEFAULT_REFRESH_TIME;
#endif
            g_alexa_access.alexa_refresh_token = strdup(tmp + 1);
#ifdef SECURITY_IMPROVE
            ALEXA_PRINT(ALEXA_DEBUG_TRACE, "login is OK\n");
#else
            ALEXA_PRINT(ALEXA_DEBUG_TRACE, "login OK is:\n%s\n",
                        g_alexa_access.alexa_refresh_token);
#endif
            ret = alexa_save_cfg(data, strlen(data), 0);
            if (ret != 0) {
                ret = -2;
            }
            pthread_mutex_unlock(&g_alexa_access.lock);
        }
    }

    if (ret != 0) {
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "login failed: %s -----\n",
                    ((ret == -2) ? "open file error" : "input error"));
    }

    return ret;
}

/*
 * log out from AVS server
 */
int AlexaAuthorizationLogOut(int flags)
{
    pthread_mutex_lock(&g_alexa_access.lock);
    if (g_alexa_access.alexa_refresh_token) {
        free(g_alexa_access.alexa_refresh_token);
        g_alexa_access.alexa_refresh_token = NULL;
    }
    if (g_alexa_access.alexa_access_token) {
        free(g_alexa_access.alexa_access_token);
        g_alexa_access.alexa_access_token = NULL;
    }
    g_alexa_access.refresh_time = 0;
    g_alexa_access.expires_in = 0;
#ifdef ASR_3PDA_ENABLE
    memset(client_id, 0, sizeof(client_id));
#endif
    pthread_mutex_unlock(&g_alexa_access.lock);

    if (flags) {
        remove_cfg();
    }

    return 0;
}

/*
  * get the refresh token from result
  */
char *AlexaGetRefreshToken(void)
{
    char *refreah_token = NULL;
    pthread_mutex_lock(&g_alexa_access.lock);
    if (g_alexa_access.alexa_refresh_token) {
        refreah_token = strdup(g_alexa_access.alexa_refresh_token);
    }
    pthread_mutex_unlock(&g_alexa_access.lock);
    return refreah_token;
}

char *LifepodGetRefreshToken(void)
{
    char *refreah_token = NULL;
    pthread_mutex_lock(&g_lifepod_access.lock);
    if (g_lifepod_access.alexa_refresh_token) {
        refreah_token = strdup(g_lifepod_access.alexa_refresh_token);
    }
    pthread_mutex_unlock(&g_lifepod_access.lock);
    return refreah_token;
}

/*
  * get the access token from result
  */
char *AlexaGetAccessToken(void)
{
    char *access_token = NULL;
    pthread_mutex_lock(&g_alexa_access.lock);
    if (g_alexa_access.alexa_access_token) {
        access_token = strdup(g_alexa_access.alexa_access_token);
    }
    pthread_mutex_unlock(&g_alexa_access.lock);
    return access_token;
}

char *LifepodGetAccessToken(void)
{
    char *access_token = NULL;
    pthread_mutex_lock(&g_lifepod_access.lock);
    if (g_lifepod_access.alexa_access_token) {
        access_token = strdup(g_lifepod_access.alexa_access_token);
    }
    pthread_mutex_unlock(&g_lifepod_access.lock);
    return access_token;
}

void AlexaGetDeviceInfo(void)
{
    char device_id[256];
    char client_id[256];
    char client_secret[256];
    char server_url[256];

    pthread_mutex_lock(&g_alexa_access.lock);
    if (asr_config_get(NVRAM_ALEXA_DEVICE_ID, device_id, sizeof(device_id)) >= 0 &&
        asr_config_get(NVRAM_ALEXA_CLIENT_ID, client_id, sizeof(client_id)) >= 0 &&
        asr_config_get(NVRAM_ALEXA_CLIENT_SECRET, client_secret, sizeof(client_secret)) >= 0) {
        if (strlen(device_id) && strlen(client_id) && strlen(client_secret)) {
            if (g_alexa_access.device_type_id) {
                free(g_alexa_access.device_type_id);
            }
            if (g_alexa_access.client_id) {
                free(g_alexa_access.client_id);
            }
            if (g_alexa_access.client_secret) {
                free(g_alexa_access.client_secret);
            }

            if (g_alexa_access.server_url) {
                free(g_alexa_access.server_url);
            }
#if ALEXA_BETA
            g_alexa_access.device_type_id = strdup(ALEXA_DEFAULT_DEVICE_NAME_BETA);
#else
            g_alexa_access.device_type_id = strdup(device_id);
#endif
            g_alexa_access.client_id = strdup(client_id);
            g_alexa_access.client_secret = strdup(client_secret);
            if (asr_config_get(NVRAM_ALEXA_SERVER_URL, server_url, sizeof(server_url)) >= 0 &&
                strlen(server_url))
                g_alexa_access.server_url = strdup(server_url);
            else
                g_alexa_access.server_url = NULL;
        }
    } else {
        AlexaDeviceInfo device_info;
        memset((void *)&device_info, 0, sizeof(device_info));
        // 1. try get alexa device info from nvram(set by server or app)
        if (g_alexa_access.device_type_id && g_alexa_access.client_id &&
            g_alexa_access.client_secret) {
            ALEXA_PRINT(ALEXA_DEBUG_ERR, "get_alexa_device_info from nvram succeed!\n");
        }
        // 2. try get alexa device info from server
        else if (!get_alexa_info_from_server(g_wiimu_shm->project_name, "/tmp/alexa.json") &&
                 !get_alexa_device_info("/tmp/alexa.json", g_wiimu_shm->project_name,
                                        &device_info)) {
            ALEXA_PRINT(ALEXA_DEBUG_ERR, "get_alexa_device_info from server succeed!\n");
            pthread_mutex_unlock(&g_alexa_access.lock);
            AlexaSetDeviceInfo(device_info.deviceName, device_info.clientId,
                               device_info.clientSecret, device_info.serverUrl);
            pthread_mutex_lock(&g_alexa_access.lock);
        }
        // 3. try get alexa device info from local file
        else if (!get_alexa_device_info(ALEXA_PRODUCTS_FILE_PATH, g_wiimu_shm->project_name,
                                        &device_info)) {
            ALEXA_PRINT(ALEXA_DEBUG_ERR, "get_alexa_device_info from file succeed!\n");
#if ALEXA_BETA
            g_alexa_access.device_type_id = strdup(ALEXA_DEFAULT_DEVICE_NAME_BETA);
#else
            g_alexa_access.device_type_id = strdup(device_info.deviceName);
#endif
            g_alexa_access.client_id = strdup(device_info.clientId);
            g_alexa_access.client_secret = strdup(device_info.clientSecret);
            g_alexa_access.server_url =
                strlen(device_info.serverUrl) ? strdup(device_info.serverUrl) : NULL;
        }
// 4. hard-coded
#if defined(ALEXA_DEFAULT_DEVICE_NAME) && defined(ALEXA_DEFAULT_CLIENT_ID) &&                      \
    defined(ALEXA_DEFAULT_CLIENT_SECRET)
        else {
#if ALEXA_BETA
            g_alexa_access.device_type_id = strdup(ALEXA_DEFAULT_DEVICE_NAME_BETA);
#else
            g_alexa_access.device_type_id = strdup(ALEXA_DEFAULT_DEVICE_NAME);
#endif
            g_alexa_access.client_id = (char *)calloc(1, sizeof(ALEXA_DEFAULT_CLIENT_ID));
            if (!g_alexa_access.client_id) {
                ALEXA_PRINT(ALEXA_DEBUG_ERR, "calloc g_alexa_access.client_id failed\n");
            } else {
                lp_rc4_enc_str_by_key_type(
                    (unsigned char *)ALEXA_DEFAULT_CLIENT_ID, sizeof(ALEXA_DEFAULT_CLIENT_ID) - 1,
                    (unsigned char *)g_alexa_access.client_id, LP_RC4_KEY_TYPE_AVS);
            }

            g_alexa_access.client_secret = (char *)calloc(1, sizeof(ALEXA_DEFAULT_CLIENT_SECRET));
            if (!g_alexa_access.client_secret) {
                ALEXA_PRINT(ALEXA_DEBUG_ERR, "calloc g_alexa_access.client_secret failed\n");
            } else {
                lp_rc4_enc_str_by_key_type((unsigned char *)ALEXA_DEFAULT_CLIENT_SECRET,
                                           sizeof(ALEXA_DEFAULT_CLIENT_SECRET) - 1,
                                           (unsigned char *)g_alexa_access.client_secret,
                                           LP_RC4_KEY_TYPE_AVS);
            }

#ifdef ALEXA_DEFAULT_SERVER_URL
            g_alexa_access.server_url = strdup(ALEXA_DEFAULT_SERVER_URL);
#else
            g_alexa_access.server_url = NULL;
#endif
            ALEXA_PRINT(ALEXA_DEBUG_ERR, "get_alexa_device_info failed, use default\n");
        }
#endif
        free_alexa_device_info(&device_info);
    }
    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "name:%s\n", g_alexa_access.device_type_id);
    // ALEXA_PRINT(ALEXA_DEBUG_TRACE, "client_id:%s\n", g_alexa_access.client_id);
    // ALEXA_PRINT(ALEXA_DEBUG_TRACE, "client_secret:%s\n", g_alexa_access.client_secret);
    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "server_url:%s\n", g_alexa_access.server_url);

    pthread_mutex_unlock(&g_alexa_access.lock);
}

void AlexaSetDeviceInfo(char *device_id, char *client_id, char *client_secret, char *server_url)
{
    pthread_mutex_lock(&g_alexa_access.lock);
    asr_config_set(NVRAM_ALEXA_DEVICE_ID, device_id);
    asr_config_set(NVRAM_ALEXA_CLIENT_ID, client_id);
    asr_config_set(NVRAM_ALEXA_CLIENT_SECRET, client_secret);
    asr_config_set(NVRAM_ALEXA_SERVER_URL, (server_url && strlen(server_url)) ? server_url : "");

    if (g_alexa_access.device_type_id) {
        free(g_alexa_access.device_type_id);
    }
    if (g_alexa_access.client_id) {
        free(g_alexa_access.client_id);
    }
    if (g_alexa_access.client_secret) {
        free(g_alexa_access.client_secret);
    }

    if (g_alexa_access.server_url) {
        free(g_alexa_access.server_url);
    }

#if ALEXA_BETA
    g_alexa_access.device_type_id = strdup(ALEXA_DEFAULT_DEVICE_NAME_BETA);
#else
    g_alexa_access.device_type_id = strdup(device_id);
#endif
    g_alexa_access.client_id = strdup(client_id);
    g_alexa_access.client_secret = strdup(client_secret);

    if (server_url && strlen(server_url))
        g_alexa_access.server_url = strdup(server_url);
    else
        g_alexa_access.server_url = NULL;
    pthread_mutex_unlock(&g_alexa_access.lock);
    // nvram_commit(RT2860_NVRAM);
    SocketClientReadWriteMsg("/tmp/RequestIOGuard", "NVRAM_COMMIT", strlen("NVRAM_COMMIT"), NULL,
                             NULL, 0);
}

#define JSON_FMT_ALEXA_DEVICE_INFO                                                                 \
    "{ \"name\": \"%s\", \"client_id\": \"%s\", \"client_secret\": \"%s\", \"url\": \"%s\" }"
char *AlexaGetDeviceInfoJson(void)
{
    char *result = NULL;
    pthread_mutex_lock(&g_alexa_access.lock);
    if (g_alexa_access.device_type_id && g_alexa_access.client_id && g_alexa_access.client_secret) {
        asprintf(&result, JSON_FMT_ALEXA_DEVICE_INFO, g_alexa_access.device_type_id,
                 g_alexa_access.client_id, g_alexa_access.client_secret,
                 g_alexa_access.server_url ? g_alexa_access.server_url : "");

        pthread_mutex_unlock(&g_alexa_access.lock);
        return result;
    }
    pthread_mutex_unlock(&g_alexa_access.lock);

    AlexaGetDeviceInfo();
    pthread_mutex_lock(&g_alexa_access.lock);
    if (g_alexa_access.device_type_id && g_alexa_access.client_id && g_alexa_access.client_secret) {
        asprintf(&result, JSON_FMT_ALEXA_DEVICE_INFO, g_alexa_access.device_type_id,
                 g_alexa_access.client_id, g_alexa_access.client_secret,
                 g_alexa_access.server_url ? g_alexa_access.server_url : "");
    }
    pthread_mutex_unlock(&g_alexa_access.lock);

    return result;
}

void LifepodGetDeviceInfo(void)
{
    pthread_mutex_lock(&g_lifepod_access.lock);
    g_lifepod_access.device_type_id = strdup(LIFEPOD_DEFAULT_DEVICE_NAME_BETA);
    g_lifepod_access.client_id = (char *)calloc(1, sizeof(LIFEPOD_DEFAULT_CLIENT_ID));
    if (!g_lifepod_access.client_id) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "calloc g_lifepod_access.client_id failed\n");
    }
    lp_rc4_enc_str_by_key_type((unsigned char *)LIFEPOD_DEFAULT_CLIENT_ID,
                               sizeof(LIFEPOD_DEFAULT_CLIENT_ID) - 1,
                               (unsigned char *)g_lifepod_access.client_id, LP_RC4_KEY_TYPE_AVS);

    g_lifepod_access.client_secret = (char *)calloc(1, sizeof(LIFEPOD_DEFAULT_CLIENT_SECRET));
    if (!g_lifepod_access.client_secret) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "calloc g_lifepod_access.client_secret failed\n");
    }
    lp_rc4_enc_str_by_key_type(
        (unsigned char *)LIFEPOD_DEFAULT_CLIENT_SECRET, sizeof(LIFEPOD_DEFAULT_CLIENT_SECRET) - 1,
        (unsigned char *)g_lifepod_access.client_secret, LP_RC4_KEY_TYPE_AVS);

    g_lifepod_access.server_url = (char *)calloc(1, sizeof(LIFEPOD_DEFAULT_SERVER_URL));
    if (!g_lifepod_access.server_url) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "calloc g_lifepod_access.server_url failed\n");
    }
    lp_rc4_enc_str_by_key_type((unsigned char *)LIFEPOD_DEFAULT_SERVER_URL,
                               sizeof(LIFEPOD_DEFAULT_SERVER_URL) - 1,
                               (unsigned char *)g_lifepod_access.server_url, LP_RC4_KEY_TYPE_AVS);
    pthread_mutex_unlock(&g_lifepod_access.lock);
}

#define JSON_FMT_LIFEPOD_DEVICE_INFO                                                               \
    "{ \"name\": \"%s\", \"client_id\": \"%s\", \"client_secret\": \"%s\", \"url\": \"%s\", "      \
    "\"code_verifier\": \"%s\" }"
char *LifepodGetDeviceInfoJson(void)
{
    char *result = NULL;
    char code_verifier[CODE_VERIFIER_STR_LEN + 1] = {0};
    pthread_mutex_lock(&g_lifepod_access.lock);
    if (g_lifepod_access.device_type_id && g_lifepod_access.client_id) {
        asprintf(&result, JSON_FMT_LIFEPOD_DEVICE_INFO, g_lifepod_access.device_type_id,
                 g_lifepod_access.client_id, g_lifepod_access.client_secret,
                 g_lifepod_access.server_url ? g_lifepod_access.server_url : "",
                 lifepod_rand_string_43(code_verifier));

        pthread_mutex_unlock(&g_lifepod_access.lock);
        return result;
    }
    pthread_mutex_unlock(&g_lifepod_access.lock);

    LifepodGetDeviceInfo();
    pthread_mutex_lock(&g_lifepod_access.lock);
    if (g_lifepod_access.device_type_id && g_lifepod_access.client_id) {
        asprintf(&result, JSON_FMT_LIFEPOD_DEVICE_INFO, g_lifepod_access.device_type_id,
                 g_lifepod_access.client_id, g_lifepod_access.client_secret,
                 g_lifepod_access.server_url ? g_lifepod_access.server_url : "",
                 lifepod_rand_string_43(code_verifier));
    }
    pthread_mutex_unlock(&g_lifepod_access.lock);

    return result;
}
