
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <curl/curl.h>

#include "json.h"
#include "wm_util.h"
#include "asr_cfg.h"
#include "asr_debug.h"
#include "asr_json_common.h"
#include "asr_authorization.h"
#include "asr_session.h"
#include "enc_dec.h"

#include "lguplus_json_common.h"

/*
Content-Type: application/json

{
    "grant_type":"",
    "refresh_token":"",
    "client_id":"",
    "client_secret":"",
}

{"access_token":"","refresh_token":"","token_type":"bearer","expires_in":3600}

*/

#define ASR_LOGIN_HOST ("Host: auth.clova.ai")
#define ASR_AUTHO2_TOKEN ("https://auth.clova.ai/authorize")
#define ASR_REFEASH_TOKEN_DATA                                                                     \
    ("grant_type=refresh_token&refresh_token=%s&client_id=%s&client_secret=%s")
#define DEFAULT_REFRESH_TIME (3600 * 1000)

#define VAL_ACESS_TOKEN ("access_token")
#define VAL_REFRESH_TOKEN ("refresh_token")
#define VAL_EXPIRES_IN ("expires_in")
#define ASR_AUTH_CONTENT_TYPE ("Content-Type: application/x-www-form-urlencoded;charset=UTF-8")
#define NO_CACHE ("Cache-Control: no-cache")

#define CIC_SERVER_URL ("CicServerUrl")
#define CIC_AUTH_SERVER_URL ("CicAuthServerUrl")
#define AIIF_SERVER_URL ("AiifServerUrl")

#define LGUP_DEVICE_TOKEN ("deviceToken")
#define LGUP_PLATFORM_TOKEN ("platformToken")
#define LGUP_NAVER_TOKEN ("NAToken")

// {"error":"expired_token","error_description":"refresh token has been used"}

typedef struct asr_auth_s {
    pthread_mutex_t lock;
    /*the access token */
    char *asr_access_token;
    /*the refresh access token */
    char *asr_refresh_token;
    char *auth_code;
    char *naver_access_token;
    char *device_type_id;
    char *client_id;
    char *client_secret;
    char *model_id;
    char *device_id;
    char *device_ruid;
    char *server_url;
    unsigned long expires_in;
} asr_auth_t;

typedef struct {
    pthread_mutex_t lock;
    char *device_token;
    char *platform_token;
} lgup_auth_t;

int g_downchannel_ready = 0;

static asr_auth_t g_asr_auth = {0};
static lgup_auth_t g_lgup_auth = {0};

static int asr_save_cfg(char *path, char *data, size_t len)
{
    int ret = -1;
    FILE *cfg_file = NULL;
    char *enc_data = NULL;

    if (path && data && len > 0) {
        cfg_file = fopen(path, "wb+");
        if (cfg_file) {
            enc_data = calloc(1, len + 1);
            if (enc_data) {
                enc_dec_str(data, len, enc_data);
                enc_data[len] = '\0';
                fwrite(enc_data, 1, len, cfg_file);
                fflush(cfg_file);
                free(enc_data);
            }
            fclose(cfg_file);
            ret = 0;
        }
    }

    return ret;
}

static size_t obtain_auth_code_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    json_object *json_response = NULL;
    char *code = NULL;
    char *state = NULL;

    json_response = json_tokener_parse(contents);
    if (!json_response)
        return size * nmemb;

    code = JSON_GET_STRING_FROM_OBJECT(json_response, AUTH_CODE);
    state = JSON_GET_STRING_FROM_OBJECT(json_response, AUTH_STATE);

    ASR_PRINT(ASR_DEBUG_TRACE, "auth code is: %s\n", code);
    ASR_PRINT(ASR_DEBUG_TRACE, "auth state is: %s\n", state);

    if (code) {
        if (g_asr_auth.auth_code) {
            free(g_asr_auth.auth_code);
        }
        g_asr_auth.auth_code = strdup(code);
    }

    if (json_response)
        json_object_put(json_response);

    return size * nmemb;
}

static size_t obtain_access_token_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    json_object *json_response = NULL;
    char *access_token = NULL;
    char *refresh_token = NULL;
    char *token_type = NULL;
    unsigned long expires_in = 0;
    char *token_info = NULL;
    long cur_time = 0;

    json_response = json_tokener_parse(contents);
    if (!json_response)
        return size * nmemb;

    access_token = JSON_GET_STRING_FROM_OBJECT(json_response, ACCESS_TOKEN);
    refresh_token = JSON_GET_STRING_FROM_OBJECT(json_response, REFRESH_TOKEN);
    token_type = JSON_GET_STRING_FROM_OBJECT(json_response, TOKEN_TYPE);
    expires_in = JSON_GET_LONG_FROM_OBJECT(json_response, EXPIRES_IN);

    ASR_PRINT(ASR_DEBUG_TRACE, "access token is: %s\n", access_token);
    ASR_PRINT(ASR_DEBUG_TRACE, "refresh token is: %s\n", refresh_token);
    ASR_PRINT(ASR_DEBUG_TRACE, "token type is: %s\n", token_type);
    ASR_PRINT(ASR_DEBUG_TRACE, "expires is: %lu\n", expires_in);

    if (access_token && refresh_token) {
        cur_time = time(NULL);
        asprintf(&token_info, "%s:%s:%lu", access_token, refresh_token, cur_time + expires_in);
        if (token_info) {
            asr_save_cfg(ASR_TOKEN_PATH, token_info, strlen(token_info));
            asr_authorization_read_cfg();
            free(token_info);
        }
    }
    if (json_response)
        json_object_put(json_response);

    return size * nmemb;
}

int asr_clear_auth_info(int clear_all)
{
    if (clear_all) {
        if (!access(ASR_TOKEN_PATH, F_OK))
            unlink(ASR_TOKEN_PATH);
        set_asr_have_token(0);
    }

    pthread_mutex_lock(&g_asr_auth.lock);
    if (g_asr_auth.asr_access_token) {
        free(g_asr_auth.asr_access_token);
        g_asr_auth.asr_access_token = NULL;
    }
    if (g_asr_auth.asr_refresh_token) {
        free(g_asr_auth.asr_refresh_token);
        g_asr_auth.asr_refresh_token = NULL;
    }
    g_asr_auth.expires_in = 0;
    pthread_mutex_unlock(&g_asr_auth.lock);

    pthread_mutex_lock(&g_lgup_auth.lock);
    if (g_lgup_auth.device_token) {
        free(g_lgup_auth.device_token);
        g_lgup_auth.device_token = NULL;
    }
    if (g_lgup_auth.platform_token) {
        free(g_lgup_auth.platform_token);
        g_lgup_auth.platform_token = NULL;
    }
    pthread_mutex_unlock(&g_lgup_auth.lock);

    return 0;
}

int asr_check_token_exist(void)
{
    pthread_mutex_lock(&g_asr_auth.lock);
    int ret = g_asr_auth.asr_access_token ? 0 : -1;
    pthread_mutex_unlock(&g_asr_auth.lock);

    return ret;
}

static int asr_read_info(char *info)
{
    json_object *json_info = NULL;
    json_object *json_products = NULL;
    json_object *json_array = NULL;
    char *project = NULL;
    char *name = NULL;
    char *id = NULL;
    char *secret = NULL;
    char *url = NULL;
    int length = 0;
    int i = 0;

    if (!info)
        return -1;

    json_info = json_tokener_parse(info);
    if (!json_info)
        return -1;

    json_products = json_object_object_get(json_info, PRODUCTS);
    if (!json_products) {
        json_object_put(json_info);
        return -1;
    }

    length = json_object_array_length(json_products);

    for (i = 0; i < length; i++) {
        json_array = json_object_array_get_idx(json_products, i);
        if (!json_array) {
            json_object_put(json_info);
            return -1;
        }

        project = JSON_GET_STRING_FROM_OBJECT(json_array, PROJECT);
        name = JSON_GET_STRING_FROM_OBJECT(json_array, NAME);
        id = JSON_GET_STRING_FROM_OBJECT(json_array, ID);
        secret = JSON_GET_STRING_FROM_OBJECT(json_array, SECRET);
        url = JSON_GET_STRING_FROM_OBJECT(json_array, URL);

        if (StrEq(project, "LGUPLUS")) {
            break;
        }
    }

    pthread_mutex_lock(&g_asr_auth.lock);
    if (name)
        g_asr_auth.device_type_id = strdup(name);
    if (id)
        g_asr_auth.client_id = strdup(id);
    if (secret)
        g_asr_auth.client_secret = strdup(secret);
    pthread_mutex_unlock(&g_asr_auth.lock);

    if (json_info)
        json_object_put(json_info);

    return 0;
}

static char *asr_read_cfg(char *path, int *data_len)
{
    size_t total_bytes = 0;
    char *tmp = NULL;
    char *data = NULL;
    char *dec_data = NULL;

    if (!path || !data_len) {
        return NULL;
    }

    FILE *_file = fopen(path, "rb");
    if (_file) {
        fseek(_file, 0, SEEK_END);
        size_t total_bytes = ftell(_file);
        if (total_bytes <= 0) {
            ASR_PRINT(ASR_DEBUG_ERR, "ftell '%s' error\n", path);
            fclose(_file);
            return NULL;
        }
        fseek(_file, 0, SEEK_SET);
        data = (char *)calloc(1, total_bytes + 1);
        if (data) {
            int ret = fread(data, 1, total_bytes, _file);
            data[total_bytes] = '\0';
            dec_data = calloc(1, total_bytes + 1);
            if (dec_data) {
                enc_dec_str(data, total_bytes, dec_data);
                dec_data[total_bytes] = '\0';
                *data_len = ret;
            }
            free(data);
        }
        fclose(_file);
    }

    return dec_data;
}

int asr_obtain_auth_code(void)
{
    char *client_id = NULL;
    char *device_id = NULL;
    char *model_id = NULL;
    char state[32] = {0};
    CURL *curl = NULL;
    char *post_data = NULL;
    struct curl_slist *slist = NULL;
    char tmp1[128] = {0};
    char tmp2[128] = {0};
    char *response = NULL;
    int ret = -1;

    pthread_mutex_lock(&g_asr_auth.lock);
    client_id = g_asr_auth.client_id;
    device_id = g_asr_auth.device_id;
    model_id = g_asr_auth.model_id;
    pthread_mutex_unlock(&g_asr_auth.lock);

    curl = curl_easy_init();
    if (!curl)
        return ret;

    time_t time1;
    struct tm *time2;

    time1 = time(NULL);
    time2 = localtime(&time1);
    srand((unsigned int)NsSincePowerOn());

    snprintf(state, sizeof(state), TIME_PATTERN, time2->tm_year + 1900, time2->tm_mon + 1,
             time2->tm_mday, time2->tm_hour + 9, time2->tm_min, time2->tm_sec, rand() % 1000);

    slist = curl_slist_append(slist, "");
    if (slist == NULL) {
        curl_easy_cleanup(curl);
        return ret;
    }

    snprintf(tmp1, sizeof(tmp1), "Authorization: Bearer %s", g_asr_auth.naver_access_token);
    snprintf(tmp2, sizeof(tmp2), "Accept: application/json");

    curl_slist_append(slist, tmp1);
    curl_slist_append(slist, tmp2);

    asprintf(&post_data, ASR_CODE_PATTERN, client_id, device_id, model_id, "code", state);

    ASR_PRINT(ASR_DEBUG_TRACE, "post data is: %s\n", post_data);

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(curl, CURLOPT_URL, ASR_AUTH_CODE_URL);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, obtain_auth_code_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)response);

    ret = curl_easy_perform(curl);

    curl_slist_free_all(slist);
    curl_easy_cleanup(curl);

    if (post_data) {
        free(post_data);
        post_data = NULL;
    }

    return ret;
}

int asr_obtain_access_token(void)
{
    char *client_id = NULL;
    char *client_secret = NULL;
    char *code = NULL;
    char *device_id = NULL;
    char *model_id = NULL;
    CURL *curl = NULL;
    char *post_data = NULL;
    struct curl_slist *slist = NULL;
    char *mac = NULL;
    char tmp1[64] = {0};
    char tmp2[128] = {0};
    char *response = NULL;
    int ret = -1;

    pthread_mutex_lock(&g_asr_auth.lock);
    client_id = g_asr_auth.client_id;
    client_secret = g_asr_auth.client_secret;
    code = g_asr_auth.auth_code;
    device_id = g_asr_auth.device_id;
    model_id = g_asr_auth.model_id;
    pthread_mutex_unlock(&g_asr_auth.lock);

    curl = curl_easy_init();
    if (!curl)
        return ret;

    slist = curl_slist_append(slist, "");
    if (slist == NULL) {
        curl_easy_cleanup(curl);
        return ret;
    }

    mac = get_wifi_mac_addr();
    snprintf(tmp1, sizeof(tmp1), "X-DEVICE-NAME: CLOVA-DONUT-%X%02X", 0xf & mac[4], mac[5]);
    snprintf(tmp2, sizeof(tmp2), "X-DEVICE-RUID: %s", g_asr_auth.device_ruid);

    curl_slist_append(slist, tmp1);
    curl_slist_append(slist, tmp2);

    asprintf(&post_data, ASR_TOKEN_PATTERN, client_id, client_secret, code, device_id, model_id);

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(curl, CURLOPT_URL, ASR_AUTH_URL);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, obtain_access_token_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)response);

    ret = curl_easy_perform(curl);

    curl_slist_free_all(slist);
    curl_easy_cleanup(curl);

    if (post_data) {
        free(post_data);
        post_data = NULL;
    }

    return ret;
}

int asr_refresh_access_token(void)
{
    char *client_id = NULL;
    char *client_secret = NULL;
    char *refresh_token = NULL;
    char *device_id = NULL;
    char *model_id = NULL;
    CURL *curl = NULL;
    char *post_data = NULL;
    struct curl_slist *slist = NULL;
    char *mac = NULL;
    char tmp1[64] = {0};
    char tmp2[128] = {0};
    char *response = NULL;
    int ret = -1;

    pthread_mutex_lock(&g_asr_auth.lock);
    client_id = g_asr_auth.client_id;
    client_secret = g_asr_auth.client_secret;
    refresh_token = g_asr_auth.asr_refresh_token;
    device_id = g_asr_auth.device_id;
    model_id = g_asr_auth.model_id;
    pthread_mutex_unlock(&g_asr_auth.lock);

    curl = curl_easy_init();
    if (!curl)
        return ret;

    slist = curl_slist_append(slist, "");
    if (slist == NULL) {
        curl_easy_cleanup(curl);
        return ret;
    }

    mac = get_wifi_mac_addr();
    snprintf(tmp1, sizeof(tmp1), "X-DEVICE-NAME: CLOVA-DONUT-%X%02X", 0xf & mac[4], mac[5]);
    snprintf(tmp2, sizeof(tmp2), "X-DEVICE-RUID: %s", g_asr_auth.device_ruid);

    curl_slist_append(slist, tmp1);
    curl_slist_append(slist, tmp2);

    asprintf(&post_data, ASR_REFRESH_PATTERN, client_id, client_secret, refresh_token, device_id,
             model_id);

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(curl, CURLOPT_URL, ASR_REFRESH_URL);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, obtain_access_token_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)response);

    ret = curl_easy_perform(curl);

    curl_slist_free_all(slist);
    curl_easy_cleanup(curl);

    if (post_data) {
        free(post_data);
        post_data = NULL;
    }

    return ret;
}

int asr_delete_access_token(void)
{
    char *client_id = NULL;
    char *client_secret = NULL;
    char *access_token = NULL;
    char *device_id = NULL;
    char *model_id = NULL;
    CURL *curl = NULL;
    char *post_data = NULL;
    struct curl_slist *slist = NULL;
    char *mac = NULL;
    char tmp1[64] = {0};
    char tmp2[128] = {0};
    char *response = NULL;
    int ret = -1;

    pthread_mutex_lock(&g_asr_auth.lock);
    client_id = g_asr_auth.client_id;
    client_secret = g_asr_auth.client_secret;
    access_token = g_asr_auth.asr_access_token;
    device_id = g_asr_auth.device_id;
    model_id = g_asr_auth.model_id;
    pthread_mutex_unlock(&g_asr_auth.lock);

    curl = curl_easy_init();
    if (!curl)
        return ret;

    slist = curl_slist_append(slist, "");
    if (slist == NULL) {
        curl_easy_cleanup(curl);
        return ret;
    }

    mac = get_wifi_mac_addr();
    snprintf(tmp1, sizeof(tmp1), "X-DEVICE-NAME: CLOVA-DONUT-%X%02X", 0xf & mac[4], mac[5]);
    snprintf(tmp2, sizeof(tmp2), "X-DEVICE-RUID: %s", g_asr_auth.device_ruid);

    curl_slist_append(slist, tmp1);
    curl_slist_append(slist, tmp2);

    asprintf(&post_data, ASR_DELETE_PATTERN, client_id, client_secret, access_token, device_id,
             model_id);

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(curl, CURLOPT_URL, ASR_DELETE_URL);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);

    ret = curl_easy_perform(curl);

    curl_slist_free_all(slist);
    curl_easy_cleanup(curl);

    if (post_data) {
        free(post_data);
        post_data = NULL;
    }

    return ret;
}

/*
 *  The access token need to refresh per 3600 seconds
 *  We fresh earlier for make the access token usable
 *  make a request to asr server per 2400 seconds
 */
void *asr_fresh_access_token_proc(void *arg)
{
    unsigned long cur_time = 0;
    unsigned long exp_time = 0;

    while (1) {
        pthread_mutex_lock(&g_asr_auth.lock);
        exp_time = g_asr_auth.expires_in;
        pthread_mutex_unlock(&g_asr_auth.lock);

        if (get_internet_flags() == 1) {
            cur_time = time(NULL);
            if (cur_time >= exp_time - 3600 && exp_time > 0) {
                ASR_PRINT(ASR_DEBUG_TRACE, "cur_time(%ld)- exp_time(%ld) = %ld\n", cur_time,
                          exp_time, cur_time - exp_time);
                asr_refresh_access_token();
                asr_session_disconncet(ASR_CLOVA);
            }
            sleep(60);
        }
        sleep(3);
    }

    return NULL;
}

int remove_cfg(void)
{
    asr_clear_alert_cfg();

    return 0;
}

static int asr_authorization_read_product_info(void)
{
    FILE *fp = NULL;
    int size = 0;
    char *info = NULL;

    if (access(ASR_PRODUCT_FILE, F_OK))
        return -1;

    if ((fp = fopen(ASR_PRODUCT_FILE, "r")) == NULL)
        return -1;

    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    info = (char *)calloc(1, size);
    if (!info) {
        fclose(fp);
        return -1;
    }

    if ((fread(info, sizeof(char), size, fp)) != size) {
        if (info) {
            free(info);
            info = NULL;
        }
        fclose(fp);
        return -1;
    }

    asr_read_info(info);

    if (info) {
        free(info);
        info = NULL;
    }

    fclose(fp);

    return 0;
}

int asr_authorization_parse_cfg(char *data)
{
    int ret = -1;
    char *tmp1 = NULL;
    char *tmp2 = NULL;

    if (data) {
        tmp1 = strchr(data, ':');
        if (!tmp1) {
            return -1;
        }
        //*tmp1 = '\0';
        tmp1++;

        tmp2 = strchr(tmp1, ':');
        if (!tmp2) {
            return -1;
        }
        //*tmp2 = '\0';
        tmp2++;

        pthread_mutex_lock(&g_asr_auth.lock);
        if (g_asr_auth.asr_access_token) {
            free(g_asr_auth.asr_access_token);
            g_asr_auth.asr_access_token = NULL;
        }
        if (g_asr_auth.asr_refresh_token) {
            free(g_asr_auth.asr_refresh_token);
            g_asr_auth.asr_refresh_token = NULL;
        }
        g_asr_auth.asr_access_token = (char *)calloc(1, tmp1 - data);
        strncpy(g_asr_auth.asr_access_token, data, tmp1 - data - 1);
        g_asr_auth.asr_access_token[tmp1 - data - 1] = '\0';
        g_asr_auth.asr_refresh_token = (char *)calloc(1, tmp2 - tmp1);
        strncpy(g_asr_auth.asr_refresh_token, tmp1, tmp2 - tmp1 - 1);
        g_asr_auth.asr_refresh_token[tmp2 - tmp1 - 1] = '\0';
        g_asr_auth.expires_in = atol(tmp2);
        pthread_mutex_unlock(&g_asr_auth.lock);
        ret = 0;
    }

    ASR_PRINT(ASR_DEBUG_TRACE, "\nasr_access_token is:\n%s\n", g_asr_auth.asr_access_token);
    ASR_PRINT(ASR_DEBUG_TRACE, "\nasr_refresh_token is:\n%s\n", g_asr_auth.asr_refresh_token);
    ASR_PRINT(ASR_DEBUG_TRACE, "\nexpires_in is:\n%lu\n", g_asr_auth.expires_in);

    return ret;
}

int asr_authorization_read_cfg(void)
{
    int data_len = 0;
    char *tmp1 = NULL;
    char *tmp2 = NULL;
    char *data = NULL;

    data = asr_read_cfg(ASR_TOKEN_PATH, &data_len);
    if (data) {
        ASR_PRINT(ASR_DEBUG_INFO, "data = %s at %lld\n", data, tickSincePowerOn());
        asr_authorization_parse_cfg(data);
        free(data);
        return 0;
    }

    set_asr_have_token(0);
    return -1;
}

/*
 * create a thread to get asr access token and refreshtoken freom asr server
 */
int asr_authorization_init(void)
{
    int ret = 0;
    pthread_t fresh_access_token_pid;

    ASR_PRINT(ASR_DEBUG_INFO, "start at %lld\n", tickSincePowerOn());

    pthread_mutex_init(&g_asr_auth.lock, NULL);

    pthread_mutex_lock(&g_asr_auth.lock);
    g_asr_auth.device_id = get_device_id();
    g_asr_auth.model_id = get_model_id();
    g_asr_auth.device_ruid = get_device_ruid();
    g_asr_auth.asr_access_token = NULL;
    g_asr_auth.asr_refresh_token = NULL;
    g_asr_auth.expires_in = 0;
    pthread_mutex_unlock(&g_asr_auth.lock);

    ret = asr_authorization_read_product_info();
    ret = lguplus_authorization_read_cfg();
    ret = asr_authorization_read_cfg();
    ret = pthread_create(&fresh_access_token_pid, NULL, asr_fresh_access_token_proc, NULL);
    if (ret) {
        ASR_PRINT(ASR_DEBUG_ERR, "pthread_create asr_fresh_access_token_proc failed\n");
        return -1;
    }

    ASR_PRINT(ASR_DEBUG_INFO, "end at %lld\n", tickSincePowerOn());

    return 0;
}

int asr_authorization_login(char *data)
{
    int ret = -1;
    char *tmp = NULL;
    int less_len = 0;
    FILE *cfg_file = NULL;

    ASR_PRINT(ASR_DEBUG_INFO, "login start ++++++\n");

    if (data && strlen(data) > 0) {
        ret = asr_authorization_parse_cfg(data);
        if (ret == 0) {
            ret = asr_save_cfg(ASR_TOKEN_PATH, data, strlen(data));
            if (ret != 0) {
                ret = -2;
            } else {
            }
        }
    }

    if (ret != 0) {
        ASR_PRINT(ASR_DEBUG_ERR, "login failed: %s -----\n",
                  ((ret == -2) ? "open file error" : "input error"));
    }

    return ret;
}

/*
 * log out from asr server
 */
int asr_authorization_logout(int flags)
{
    pthread_mutex_lock(&g_asr_auth.lock);
    if (g_asr_auth.asr_refresh_token) {
        free(g_asr_auth.asr_refresh_token);
        g_asr_auth.asr_refresh_token = NULL;
    }
    if (g_asr_auth.asr_access_token) {
        free(g_asr_auth.asr_access_token);
        g_asr_auth.asr_access_token = NULL;
    }
    g_asr_auth.expires_in = 0;
    pthread_mutex_unlock(&g_asr_auth.lock);

    if (flags) {
        remove_cfg();
    }
    asr_session_disconncet(ASR_CLOVA);

    return 0;
}

/*
  * get the refresh token from result
  */
char *asr_authorization_refresh_token(void)
{
    char *refreah_token = NULL;
    pthread_mutex_lock(&g_asr_auth.lock);
    if (g_asr_auth.asr_refresh_token) {
        refreah_token = strdup(g_asr_auth.asr_refresh_token);
    }
    pthread_mutex_unlock(&g_asr_auth.lock);
    return refreah_token;
}

/*
  * get the access token from result
  */
char *asr_authorization_access_token(void)
{
    char *access_token = NULL;
    pthread_mutex_lock(&g_asr_auth.lock);
    if (g_asr_auth.asr_access_token) {
        access_token = strdup(g_asr_auth.asr_access_token);
    }
    pthread_mutex_unlock(&g_asr_auth.lock);
    return access_token;
}

void *asr_auth_proc(void *arg)
{
    int count = 60 * 3;
    json_object *js_auth_result = NULL;
    char s_expire[64] = {0};
    char message[256] = {0};

    while (count--) {
        pthread_testcancel();
        ASR_PRINT(ASR_DEBUG_INFO, "count is: %d\n", count);
        if (get_internet_flags()) {
            asr_obtain_auth_code();
            asr_obtain_access_token();
            break;
        }
        sleep(1);
    }

    if (g_asr_auth.asr_access_token && g_asr_auth.asr_refresh_token) {
        js_auth_result = json_object_new_object();
        if (js_auth_result) {
            snprintf(s_expire, sizeof(s_expire), "%lu", g_asr_auth.expires_in);

            json_object_object_add(js_auth_result, "AccessToken",
                                   json_object_new_string(g_asr_auth.asr_access_token));
            json_object_object_add(js_auth_result, "RefreshToken",
                                   json_object_new_string(g_asr_auth.asr_refresh_token));
            json_object_object_add(js_auth_result, "Expires_in", json_object_new_string(s_expire));
            json_object_object_add(js_auth_result, "publish_type", json_object_new_string("N"));

            snprintf(message, sizeof(message), "%s%s", "GNOTIFY=CIC_TOKEN:",
                     json_object_to_json_string(js_auth_result));
            send_json_message(message);

            json_object_put(js_auth_result);
        }
    } else {
        volume_pormpt_action(VOLUME_P_03);
    }

    ASR_PRINT(ASR_DEBUG_INFO, "autho proc finished\n");

    pthread_exit(0);
}

void asr_handle_unauth_case(void) { asr_refresh_access_token(); }

int lguplus_authorization_read_cfg(void)
{
    int data_len = 0;
    char *tmp1 = NULL;
    char *data = NULL;

    data = asr_read_cfg(LGUP_TOKEN_PATH, &data_len);
    if (data) {
        tmp1 = strchr(data, ':');
        if (!tmp1) {
            free(data);
            return -1;
        }
        *tmp1 = '\0';
        tmp1++;

        pthread_mutex_lock(&g_lgup_auth.lock);
        if (g_lgup_auth.device_token) {
            free(g_lgup_auth.device_token);
            g_lgup_auth.device_token = NULL;
        }
        if (g_lgup_auth.platform_token) {
            free(g_lgup_auth.platform_token);
            g_lgup_auth.platform_token = NULL;
        }
        g_lgup_auth.device_token = (char *)calloc(1, tmp1 - data + 1);
        strncpy(g_lgup_auth.device_token, data, tmp1 - data);
        g_lgup_auth.device_token[tmp1 - data - 1] = '\0';
        g_lgup_auth.platform_token = (char *)calloc(1, data_len - (tmp1 - data) + 1);
        strncpy(g_lgup_auth.platform_token, tmp1, data_len - (tmp1 - data));
        g_lgup_auth.platform_token[data_len - (tmp1 - data)] = '\0';
        pthread_mutex_unlock(&g_lgup_auth.lock);

        free(data);
    }

    ASR_PRINT(ASR_DEBUG_TRACE, "device_token is: %s\n", g_lgup_auth.device_token);
    ASR_PRINT(ASR_DEBUG_TRACE, "platform_token is: %s\n", g_lgup_auth.platform_token);

    return 0;
}

char *lguplus_get_device_token(void)
{
    char *device_token = NULL;
    pthread_mutex_lock(&g_lgup_auth.lock);
    if (g_lgup_auth.device_token) {
        device_token = strdup(g_lgup_auth.device_token);
    }
    pthread_mutex_unlock(&g_lgup_auth.lock);
    return device_token;
}

char *lguplus_get_platform_token(void)
{
    char *platform_token = NULL;
    pthread_mutex_lock(&g_lgup_auth.lock);
    if (g_lgup_auth.platform_token) {
        platform_token = strdup(g_lgup_auth.platform_token);
    }
    pthread_mutex_unlock(&g_lgup_auth.lock);
    return platform_token;
}

int lguplus_parse_server_url(char *url)
{
    json_object *json_url = NULL;
    char *cic_server = NULL;
    char *cic_auth = NULL;
    char *aiif_server = NULL;
    char tmp[128] = {0};

    if (!url)
        return -1;

    json_url = json_tokener_parse(url);
    if (!json_url)
        return -1;

    cic_server = JSON_GET_STRING_FROM_OBJECT(json_url, CIC_SERVER_URL);
    cic_auth = JSON_GET_STRING_FROM_OBJECT(json_url, CIC_AUTH_SERVER_URL);
    aiif_server = JSON_GET_STRING_FROM_OBJECT(json_url, AIIF_SERVER_URL);

    ASR_PRINT(ASR_DEBUG_TRACE, "cic_server is: %s\n", cic_server);
    ASR_PRINT(ASR_DEBUG_TRACE, "cic_auth is: %s\n", cic_auth);
    ASR_PRINT(ASR_DEBUG_TRACE, "aiif_server is: %s\n", aiif_server);

    if (json_url) {
        json_object_put(json_url);
    }

    return 0;
}

int lguplus_parse_aiif_token(char *token)
{
    json_object *json_token = NULL;
    char *device_token = NULL;
    char *aiif_token = NULL;
    FILE *fp = NULL;
    char tmp[256] = {0};

    if (!token)
        return -1;

    json_token = json_tokener_parse(token);
    if (!json_token)
        return -1;

    device_token = JSON_GET_STRING_FROM_OBJECT(json_token, LGUP_DEVICE_TOKEN);
    aiif_token = JSON_GET_STRING_FROM_OBJECT(json_token, LGUP_PLATFORM_TOKEN);

    snprintf(tmp, sizeof(tmp), "%s:%s", device_token, aiif_token);

    asr_save_cfg(LGUP_TOKEN_PATH, tmp, strlen(tmp));
    lguplus_authorization_read_cfg();

    return 0;
}

/*
{
    "NAToken":
"AAAAO118s0PMsd3F1LETNZCiWYzBnOIKPpj4IKpWH7e+4lxOih5QrH1ezFkKH5WyQZtt76B9OkY7VtWSSCKSnfY+Sis="
}
*/
int lguplus_parse_naver_token(char *token)
{
    int ret = -1;
    json_object *json_token = NULL;
    char *naver_token = NULL;
    static pthread_t asr_auth_tid = 0;
    pthread_attr_t attr;

    if (!token)
        return -1;

    if (asr_auth_tid) {
        pthread_cancel(asr_auth_tid);
        asr_auth_tid = 0;
    }

    json_token = json_tokener_parse(token);
    if (!json_token)
        return -1;

    naver_token = JSON_GET_STRING_FROM_OBJECT(json_token, LGUP_NAVER_TOKEN);

    if (naver_token) {
        if (g_asr_auth.naver_access_token) {
            free(g_asr_auth.naver_access_token);
        }
        g_asr_auth.naver_access_token = strdup(naver_token);
    }

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    ret = pthread_create(&asr_auth_tid, &attr, asr_auth_proc, NULL);

    return ret;
}
