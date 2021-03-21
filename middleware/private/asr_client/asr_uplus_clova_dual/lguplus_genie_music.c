
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>

#include "json.h"
#include "asr_debug.h"
#include "asr_json_common.h"
#include "lguplus_genie_music.h"

static lguplus_genie_t g_genie = {0};

static char *urldecode(const char *url, char *d_url)
{
    int i;
    int j = 0;
    char *ptr = url;
    char p[2] = {0};

    for (i = 0; i < strlen(ptr); i++) {
        memset(p, 0x0, 2);
        if (ptr[i] != '%') {
            d_url[j++] = ptr[i];
            continue;
        }

        p[0] = ptr[++i];
        p[1] = ptr[++i];

        p[0] = p[0] - 48 - ((p[0] >= 'A') ? 7 : 0) - ((p[0] >= 'a') ? 32 : 0);
        p[1] = p[1] - 48 - ((p[1] >= 'A') ? 7 : 0) - ((p[1] >= 'a') ? 32 : 0);

        d_url[j++] = (unsigned char)(p[0] * 16 + p[1]);
    }

    d_url[j] = '\0';

    return d_url;
}

static size_t get_genie_access_token(void *contents, size_t size, size_t nmemb, void *userp)
{
    char *buf = (char *)contents;
    lguplus_genie_t *genie = (lguplus_genie_t *)userp;
    char *access_token = NULL;
    json_object *js_buf = NULL;
    json_object *js_secret = NULL;

    ASR_PRINT(ASR_DEBUG_TRACE, "genie login result is: %s\n", buf);

    if (buf) {
        js_buf = json_tokener_parse(buf);
        if (js_buf) {
            js_secret = json_object_object_get(js_buf, GENIE_SECRET);
            if (js_secret) {
                access_token = JSON_GET_STRING_FROM_OBJECT(js_secret, GENIE_ACCESS_TOKEN);
                if (access_token) {
                    if (genie->access_token) {
                        free(genie->access_token);
                        genie->access_token = NULL;
                    }
                    genie->access_token = strdup(access_token);
                }
            }

            json_object_put(js_buf);
        }
    }

    ASR_PRINT(ASR_DEBUG_TRACE, "genie access_token is: %s\n", genie->access_token);

    return size * nmemb;
}

static size_t get_genie_resource_url(void *contents, size_t size, size_t nmemb, void *userp)
{
    char *buf = (char *)contents;
    lguplus_genie_t *genie = (lguplus_genie_t *)userp;
    json_object *js_buf = NULL;
    json_object *js_items = NULL;
    json_object *js_item = NULL;
    char *resource_url = NULL;
    char *d_resource_url = NULL;
    int total_item_num = 0;
    int i;

    ASR_PRINT(ASR_DEBUG_TRACE, "genie resource result is: %s\n", buf);

    if (buf) {
        js_buf = json_tokener_parse(buf);
        if (js_buf) {
            js_items = json_object_object_get(js_buf, GENIE_ITEMS);
            if (js_items) {
                total_item_num = json_object_array_length(js_items);
                if (total_item_num > 0) {
                    for (i = 0; i < total_item_num; i++) {
                        js_item = json_object_array_get_idx(js_items, i);
                        if (js_item) {
                            resource_url = JSON_GET_STRING_FROM_OBJECT(js_item, GENIE_RESOURCE_URL);
                            if (resource_url) {
                                d_resource_url = (char *)calloc(1, strlen(resource_url) + 1);
                                d_resource_url = urldecode(resource_url, d_resource_url);
                                if (genie->resource_url) {
                                    free(genie->resource_url);
                                    genie->resource_url = NULL;
                                }
                                genie->resource_url = d_resource_url;
                                ASR_PRINT(ASR_DEBUG_TRACE, "genie resource url is: %s\n",
                                          genie->resource_url);
                            }
                        }
                        js_item = NULL;
                    }
                }
            }

            json_object_put(js_buf);
        }
    }

    return size * nmemb;
}

int lguplus_check_genie_login(void)
{
    int ret = -1;

    ret = g_genie.access_token ? 0 : -1;

    return ret;
}

int lguplus_genie_server_login(void)
{
    CURL *curl = NULL;
    struct curl_slist *slist = NULL;
    int ret = -1;

    curl = curl_easy_init();
    if (curl) {
        slist = curl_slist_append(slist, "Content-Type: application/json");
        curl_slist_append(slist, "From: uplus-ai");
        if (slist) {
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
            curl_easy_setopt(curl, CURLOPT_URL, GENIE_SERVICE_LOGIN_URL);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{\"uno\":309276386}");
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen("{\"uno\":309276386}"));
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &g_genie);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, get_genie_access_token);

            ret = curl_easy_perform(curl);

            curl_slist_free_all(slist);
        }

        curl_easy_cleanup(curl);
    }

    return ret;
}

char *lguplus_genie_get_url(const char *song_id)
{
    CURL *curl = NULL;
    struct curl_slist *slist = NULL;
    char url[128] = {0};
    char *authorization = NULL;
    int ret = -1;

    if (song_id) {
        snprintf(url, sizeof(url), GENIE_SERVICE_RESOURCE_URL "%s", song_id);
    } else {
        return ret;
    }

    curl = curl_easy_init();
    if (curl) {
        slist = curl_slist_append(slist, "Content-Type: application/json");
        curl_slist_append(slist, "From: uplus-ai");
        curl_slist_append(slist, "X-Login-Token: 30927638663735.161");
        curl_slist_append(slist, "X-itn : Y");

        asprintf(&authorization, "Authorization: Bearer %s", g_genie.access_token);
        if (authorization) {
            curl_slist_append(slist, authorization);
            free(authorization);
            authorization = NULL;
        }

        if (slist) {
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &g_genie);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, get_genie_resource_url);

            ret = curl_easy_perform(curl);

            curl_slist_free_all(slist);
        }

        curl_easy_cleanup(curl);
    }

    return g_genie.resource_url;
}
