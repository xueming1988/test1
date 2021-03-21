
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
#include "enc_dec.h"
#include "asr_bluetooth.h"

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

// {"error":"expired_token","error_description":"refresh token has been used"}

typedef struct asr_auth_s {
    pthread_mutex_t lock;
    /*the access token */
    char *asr_access_token;
    /*the refresh access token */
    char *asr_refresh_token;
    char *auth_code;
    char *device_type_id;
    char *client_id;
    char *client_secret;
    char *model_id;
    char *device_id;
    char *device_ruid;
    char *server_url;
    long expires_in;
} asr_auth_t;

int g_downchannel_ready = 0;
static asr_auth_t g_asr_auth = {0};

static int asr_save_cfg(char *data, size_t len)
{
    int ret = -1;
    FILE *cfg_file = NULL;
    char *enc_data = NULL;

    if (data && len > 0) {
        cfg_file = fopen(ASR_TOKEN_PATH, "wb+");
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
            set_asr_have_token(1);
        }
    }

    return ret;
}

static void *wait_h2_connection_ready_proc(void *arg)
{
    int count = 45;

    g_downchannel_ready = 0;

    while (count) {
        if (g_downchannel_ready) {
            notify_auth_done();
            break;
        }

        count--;
        sleep(1);
    }

    if (!count && get_ota_block_status())
        notify_auth_failed(DOWNSTREAM_CREATION_ERROR);

    pthread_exit(0);
}

static size_t obtain_access_token_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    json_object *json_response = NULL;
    char *access_token = NULL;
    char *refresh_token = NULL;
    char *token_type = NULL;
    long expires_in = 0;
    char *token_info = NULL;
    long cur_time = 0;
    pthread_t wait_h2_connection_ready_tid = 0;
    pthread_attr_t attr;
    char *error = NULL;

    ASR_PRINT(ASR_DEBUG_TRACE, "%s\n", contents);

    json_response = json_tokener_parse(contents);
    if (!json_response)
        return size * nmemb;

    access_token = JSON_GET_STRING_FROM_OBJECT(json_response, ACCESS_TOKEN);
    refresh_token = JSON_GET_STRING_FROM_OBJECT(json_response, REFRESH_TOKEN);
    token_type = JSON_GET_STRING_FROM_OBJECT(json_response, TOKEN_TYPE);
    expires_in = JSON_GET_LONG_FROM_OBJECT(json_response, EXPIRES_IN);
    error = JSON_GET_STRING_FROM_OBJECT(json_response, "error");

    if (access_token && refresh_token) {
        cur_time = time(NULL);
        asprintf(&token_info, "%s:%s:%ld", access_token, refresh_token, cur_time + expires_in);
        if (token_info) {
            asr_save_cfg(token_info, strlen(token_info));
            asr_authorization_read_cfg();
            free(token_info);
            http2_conn_exit();

            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(
                &attr, PTHREAD_CREATE_DETACHED); // release resource automatically when exits
            pthread_create(&wait_h2_connection_ready_tid, &attr, wait_h2_connection_ready_proc,
                           NULL);
            pthread_attr_destroy(&attr);
        }
    } else if (error) {
        if (StrEq(error, "unauthorized_client")) {
            asr_clear_auth_info(1);
            notify_enter_oobe_mode(-1);
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

    return 0;
}

char *asr_get_prepare_info(void)
{
    json_object *json_data = NULL;
    char *data = NULL;
    char *val_str = NULL;

    json_data = json_object_new_object();
    if (!json_data)
        return NULL;

    pthread_mutex_lock(&g_asr_auth.lock);
    json_object_object_add(json_data, CLOVA_CLIENT_ID,
                           JSON_OBJECT_NEW_STRING(g_asr_auth.client_id));
    json_object_object_add(json_data, DEVICE_MODEL, JSON_OBJECT_NEW_STRING(g_asr_auth.model_id));
    json_object_object_add(json_data, DEVICE_UUID, JSON_OBJECT_NEW_STRING(g_asr_auth.device_id));
    pthread_mutex_unlock(&g_asr_auth.lock);

    val_str = json_object_to_json_string(json_data);
    if (val_str) {
        data = strdup(val_str);
    }

    if (json_data)
        json_object_put(json_data);

    return data;
}

int asr_check_token_exist(void)
{
    pthread_mutex_lock(&g_asr_auth.lock);
    int ret = g_asr_auth.asr_access_token ? 0 : -1;
    pthread_mutex_unlock(&g_asr_auth.lock);

    return ret;
}

int asr_set_prepare_info(char *info)
{
    json_object *json_data = NULL;
    char *auth_code = NULL;
    char *auth_state = NULL;
    int is_clear = 0;
    int ret = -1;
    pthread_t request_access_token_tid = 0;
    pthread_attr_t attr;

    if (!info)
        return ret;

    json_data = json_tokener_parse(info);
    if (!json_data)
        return ret;

    auth_code = JSON_GET_STRING_FROM_OBJECT(json_data, CLOVA_AUTH_CODE);
    if (!auth_code)
        auth_code = JSON_GET_STRING_FROM_OBJECT(json_data, CLOVA_CODE);
    auth_state = JSON_GET_STRING_FROM_OBJECT(json_data, CLOVA_AUTH_STATE);
    if (!auth_state)
        auth_state = JSON_GET_STRING_FROM_OBJECT(json_data, CLOVA_STATE);
    is_clear = JSON_GET_BOOL_FROM_OBJECT(json_data, IS_CLEAR_USER_DATA);

    if (!auth_code)
        return ret;

    pthread_mutex_lock(&g_asr_auth.lock);
    if (g_asr_auth.auth_code) {
        free(g_asr_auth.auth_code);
        g_asr_auth.auth_code = NULL;
    }
    g_asr_auth.auth_code = strdup(auth_code);
    pthread_mutex_unlock(&g_asr_auth.lock);

    if (is_clear) {
        notify_clear_wifi_history2();
        asr_bluetooth_reset();
    }

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(
        &attr, PTHREAD_CREATE_DETACHED); // release resource automatically when exits
    ret = pthread_create(&request_access_token_tid, &attr, asr_request_access_token_proc, NULL);
    pthread_attr_destroy(&attr);

    if (json_data)
        json_object_put(json_data);

    return ret;
}

int asr_handle_network_only(const char *info)
{
    int network_only;

    if (!info)
        return -1;

    network_only = atoi(info);
    if (!network_only) {
        asr_clear_auth_info(0);
        asr_config_set("dnd_now_end_time", "");
        asr_config_sync();
    }

    return 0;
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

        if (StrEq(project, g_asr_auth.model_id)) {
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

static char *asr_read_cfg(int *data_len)
{
    int total_bytes = 0;
    char *tmp = NULL;
    char *data = NULL;
    char *dec_data = NULL;

    FILE *_file = fopen(ASR_TOKEN_PATH, "rb");
    if (_file) {
        fseek(_file, 0, SEEK_END);
        size_t total_bytes = ftell(_file);
        if (total_bytes <= 0) {
            ASR_PRINT(ASR_DEBUG_ERR, "ftell '%s' error\n", ASR_TOKEN_PATH);
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

/*
 * parse the access token and refresh frome asr response
 */
int asr_result_parse(char *body)
{
    char *refresh_token = NULL;
    char *access_token = NULL;
    long expires_in = 0;

    if (!body) {
        ASR_PRINT(ASR_DEBUG_ERR, "input error\n");
        return -1;
    }

    pthread_mutex_lock(&g_asr_auth.lock);

    json_object *json_obj = json_tokener_parse(body);
    if (json_obj) {
        access_token = (char *)JSON_GET_STRING_FROM_OBJECT(json_obj, VAL_ACESS_TOKEN);
        if (access_token) {
            if (g_asr_auth.asr_access_token) {
                free(g_asr_auth.asr_access_token);
            }
            g_asr_auth.asr_access_token = strdup(access_token);
        }

        refresh_token = (char *)JSON_GET_STRING_FROM_OBJECT(json_obj, VAL_REFRESH_TOKEN);
        if (refresh_token) {
            if (g_asr_auth.asr_refresh_token) {
                free(g_asr_auth.asr_refresh_token);
            }
            g_asr_auth.asr_refresh_token = strdup(refresh_token);
        }

        expires_in = (long)JSON_GET_LONG_FROM_OBJECT(json_obj, VAL_EXPIRES_IN);
        ASR_PRINT(ASR_DEBUG_ERR, "expires_in = %ld\n", expires_in);
        if (expires_in > 0) {
            g_asr_auth.expires_in = ((long long)expires_in - 60)
                                        ? ((long long)expires_in - 60) * 1000
                                        : (long long)expires_in * 1000;
        }
        if (access_token && refresh_token) {
        }

        json_object_put(json_obj);
    }
    pthread_mutex_unlock(&g_asr_auth.lock);

    return 0;
}

int asr_result_error(char *body)
{
    int ret = -1;
    char *error = NULL;

    if (!body) {
        ASR_PRINT(ASR_DEBUG_ERR, "input error\n");
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

int asr_obtain_access_token(void)
{
    CURL *curl = NULL;
    char *post_data = NULL;
    struct curl_slist *slist = NULL;
    char *mac = NULL;
    char tmp1[64] = {0};
    char tmp2[128] = {0};
    char *response = NULL;
    int ret = -1;

    curl = curl_easy_init();
    if (curl == NULL) {
        ASR_PRINT(ASR_DEBUG_ERR, "curl_easy_init error\n");
        goto Exit;
    }

    slist = curl_slist_append(slist, "");
    if (slist == NULL) {
        ASR_PRINT(ASR_DEBUG_ERR, "curl_slist_append error\n");
        goto Exit;
    }

    pthread_mutex_lock(&g_asr_auth.lock);
    mac = get_wifi_mac_addr();
    if (mac) {
        snprintf(tmp1, sizeof(tmp1), "X-DEVICE-NAME: %s", get_bletooth_name());
    }
    snprintf(tmp2, sizeof(tmp2), "X-DEVICE-RUID: %s", g_asr_auth.device_ruid);
    asprintf(&post_data, ASR_TOKEN_PATTERN, g_asr_auth.client_id, g_asr_auth.client_secret,
             g_asr_auth.auth_code, g_asr_auth.device_id, g_asr_auth.model_id);
    pthread_mutex_unlock(&g_asr_auth.lock);
    if (!post_data) {
        ASR_PRINT(ASR_DEBUG_ERR, "asprintf error\n");
        goto Exit;
    }

    curl_slist_append(slist, tmp1);
    curl_slist_append(slist, tmp2);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(curl, CURLOPT_URL, ASR_AUTH_URL);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, obtain_access_token_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)response);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); // must be set, otherwise will crash

    ret = curl_easy_perform(curl);

    if (ret) {
        notify_auth_failed(ACCESS_TOKEN_GET_FAILED);
    }

Exit:
    if (slist) {
        curl_slist_free_all(slist);
    }
    if (curl) {
        curl_easy_cleanup(curl);
    }
    if (post_data) {
        free(post_data);
        post_data = NULL;
    }

    return ret;
}

int asr_refresh_access_token(void)
{
    CURL *curl = NULL;
    char *post_data = NULL;
    struct curl_slist *slist = NULL;
    char *mac = NULL;
    char tmp1[64] = {0};
    char tmp2[128] = {0};
    char *response = NULL;
    int ret = -1;

    curl = curl_easy_init();
    if (curl == NULL) {
        ASR_PRINT(ASR_DEBUG_ERR, "curl_easy_init error\n");
        goto Exit;
    }

    slist = curl_slist_append(slist, "");
    if (slist == NULL) {
        ASR_PRINT(ASR_DEBUG_ERR, "curl_slist_append error\n");
        goto Exit;
    }

    pthread_mutex_lock(&g_asr_auth.lock);
    mac = get_wifi_mac_addr();
    if (mac) {
        snprintf(tmp1, sizeof(tmp1), "X-DEVICE-NAME: %s", get_bletooth_name());
    }
    snprintf(tmp2, sizeof(tmp2), "X-DEVICE-RUID: %s", g_asr_auth.device_ruid);
    asprintf(&post_data, ASR_REFRESH_PATTERN, g_asr_auth.client_id, g_asr_auth.client_secret,
             g_asr_auth.asr_refresh_token, g_asr_auth.device_id, g_asr_auth.model_id);
    pthread_mutex_unlock(&g_asr_auth.lock);
    if (!post_data) {
        ASR_PRINT(ASR_DEBUG_ERR, "asprintf error\n");
        goto Exit;
    }

    curl_slist_append(slist, tmp1);
    curl_slist_append(slist, tmp2);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(curl, CURLOPT_URL, ASR_REFRESH_URL);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, obtain_access_token_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)response);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); // must be set, otherwise will crash

    ret = curl_easy_perform(curl);

Exit:
    if (slist) {
        curl_slist_free_all(slist);
    }
    if (curl) {
        curl_easy_cleanup(curl);
    }
    if (post_data) {
        free(post_data);
        post_data = NULL;
    }

    return ret;
}

int asr_delete_access_token(void)
{
    CURL *curl = NULL;
    char *post_data = NULL;
    struct curl_slist *slist = NULL;
    char *mac = NULL;
    char tmp1[64] = {0};
    char tmp2[128] = {0};
    char *response = NULL;
    int ret = -1;

    curl = curl_easy_init();
    if (curl == NULL) {
        ASR_PRINT(ASR_DEBUG_ERR, "curl_easy_init error\n");
        goto Exit;
    }

    slist = curl_slist_append(slist, "");
    if (slist == NULL) {
        ASR_PRINT(ASR_DEBUG_ERR, "curl_slist_append error\n");
        goto Exit;
    }

    pthread_mutex_lock(&g_asr_auth.lock);
    mac = get_wifi_mac_addr();
    if (mac) {
        snprintf(tmp1, sizeof(tmp1), "X-DEVICE-NAME: %s", get_bletooth_name());
    }
    snprintf(tmp2, sizeof(tmp2), "X-DEVICE-RUID: %s", g_asr_auth.device_ruid);
    asprintf(&post_data, ASR_DELETE_PATTERN, g_asr_auth.client_id, g_asr_auth.client_secret,
             g_asr_auth.asr_access_token, g_asr_auth.device_id, g_asr_auth.model_id);
    pthread_mutex_unlock(&g_asr_auth.lock);
    if (!post_data) {
        ASR_PRINT(ASR_DEBUG_ERR, "asprintf error\n");
        goto Exit;
    }

    curl_slist_append(slist, tmp1);
    curl_slist_append(slist, tmp2);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
    curl_easy_setopt(curl, CURLOPT_URL, ASR_DELETE_URL);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); // must be set, otherwise will crash

    ret = curl_easy_perform(curl);

Exit:
    if (slist) {
        curl_slist_free_all(slist);
    }
    if (curl) {
        curl_easy_cleanup(curl);
    }
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
    long cur_time = 0;
    long exp_time = 0;
    int is_running = 1;

    while (is_running) {
        pthread_mutex_lock(&g_asr_auth.lock);
        exp_time = g_asr_auth.expires_in;
        pthread_mutex_unlock(&g_asr_auth.lock);

        if (get_internet_flags() == 1) {
            cur_time = time(NULL);
            if (cur_time >= exp_time - 3600 && exp_time > 0) {
                ASR_PRINT(ASR_DEBUG_TRACE, "cur_time(%ld)- exp_time(%ld) = %ld\n", cur_time,
                          exp_time, cur_time - exp_time);
                asr_refresh_access_token();
            }
            sleep(60);
        }
        sleep(3);
    }

    return NULL;
}

void *asr_request_access_token_proc(void *arg)
{
    int count = 60;

    while (count) {
        if (get_internet_flags() == 1) {
            asr_obtain_access_token();
            break;
        } else {
            ASR_PRINT(ASR_DEBUG_ERR, "asr_request_access_token_proc internet=%d\r\n",
                      get_internet_flags());
        }
        count--;
        sleep(1);
    }

    if (!count) {
        notify_auth_failed(ACCESS_TOKEN_GET_FAILED);
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
    if (size <= 0) {
        fclose(fp);
        return -1;
    }

    info = (char *)calloc(1, size + 1);
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
    ASR_PRINT(ASR_DEBUG_TRACE, "\nexpires_in is:\n%lld\n", g_asr_auth.expires_in);

    return ret;
}

int asr_authorization_read_cfg(void)
{
    int data_len = 0;
    char *tmp1 = NULL;
    char *tmp2 = NULL;
    char *data = NULL;

    data = asr_read_cfg(&data_len);
    if (data) {
        ASR_PRINT(ASR_DEBUG_TRACE, "data = %s at %lld\n", data, tickSincePowerOn());
        asr_authorization_parse_cfg(data);
        free(data);
        set_asr_have_token(1);
        notify_update_new_token(g_asr_auth.asr_access_token);
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

    ASR_PRINT(ASR_DEBUG_TRACE, "start at %lld\n", tickSincePowerOn());

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
    ret = asr_authorization_read_cfg();
    ret = pthread_create(&fresh_access_token_pid, NULL, asr_fresh_access_token_proc, NULL);
    if (ret) {
        ASR_PRINT(ASR_DEBUG_ERR, "pthread_create asr_fresh_access_token_proc failed\n");
        return -1;
    }

    ASR_PRINT(ASR_DEBUG_TRACE, "end at %lld\n", tickSincePowerOn());

    return 0;
}

int asr_authorization_login(char *data)
{
    int ret = -1;
    char *tmp = NULL;
    int less_len = 0;
    FILE *cfg_file = NULL;

    ASR_PRINT(ASR_DEBUG_TRACE, "login start ++++++\n");

    if (data && strlen(data) > 0) {
        ret = asr_authorization_parse_cfg(data);
        if (ret == 0) {
            ret = asr_save_cfg(data, strlen(data));
            if (ret != 0) {
                ret = -2;
            } else {
            }
        }
    }

    if (ret != 0) {
        ASR_PRINT(ASR_DEBUG_TRACE, "login failed: %s -----\n",
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
    http2_conn_exit();

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

void asr_handle_unauth_case(void) { asr_refresh_access_token(); }
