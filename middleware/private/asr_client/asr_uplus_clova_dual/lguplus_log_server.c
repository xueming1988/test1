
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>

#include "asr_debug.h"

#include "lguplus_log_server.h"

static void create_tran_id(char *tran_id, int len, const char *sn)
{
    time_t time1;
    struct tm *time2;
    char time3[18] = {0};

    if (tran_id && sn) {
        time1 = time(NULL);
        time2 = localtime(&time1);
        srand((unsigned int)NsSincePowerOn());

        snprintf(time3, sizeof(time3), TIME_PATTERN, time2->tm_year + 1900, time2->tm_mon + 1,
                 time2->tm_mday, time2->tm_hour + 9, time2->tm_min, time2->tm_sec, rand() % 100);
        snprintf(tran_id, len, TRANSACTIONID_PATTERN, time3, "SVC_001", sn);
    }
}

static void create_auth_key(char *auth_key, int len, const char *tran_id, const char *sn) {}

int lguplus_upload_log_file(const char *log_path)
{
    CURL *curl = NULL;
    struct curl_slist *slist = NULL;
    struct curl_httppost *formpost = NULL;
    struct curl_httppost *lastptr = NULL;
    char *tmp = NULL;
    char sn[32] = {0};
    char tran_id[64] = {0};
    char auth_key[64] = {0};
    char buf[128] = {0};
    int ret = -1;

    tmp = get_device_sn();
    strncpy(sn, tmp, sizeof(sn));

    create_tran_id(tran_id, sizeof(tran_id), sn);

    create_auth_key(auth_key, sizeof(auth_key), tran_id, sn);

    ASR_PRINT(ASR_DEBUG_TRACE, "uploading is starting...\n");

    curl = curl_easy_init();
    if (curl) {
        slist = curl_slist_append(slist, "");
        if (slist) {
            curl_slist_append(slist, "Content-Type: multipart/form-data");
            curl_slist_append(slist, "clientIp: 111.111.111.111");
            curl_slist_append(slist, "devInfo: SPK");
            curl_slist_append(slist, "osInfo: Linux");
            curl_slist_append(slist, "nwInfo: test");
            curl_slist_append(slist, "devModel: Linkplay");
            curl_slist_append(slist, "carrierType: L");

            snprintf(buf, sizeof(buf), "authkey: %s", auth_key);
            curl_slist_append(slist, buf);

            memset(buf, 0, sizeof(buf));
            snprintf(buf, sizeof(buf), "serial: %s", sn);
            curl_slist_append(slist, buf);

            memset(buf, 0, sizeof(buf));
            snprintf(buf, sizeof(buf), "transactionid: %s", tran_id);
            curl_slist_append(slist, buf);

            curl_easy_setopt(curl, CURLOPT_URL, LOG_SERVER);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, slist);

            curl_formadd(&formpost, &lastptr, CURLFORM_PTRNAME, "file", CURLFORM_FILE, log_path,
                         CURLFORM_END);
            curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

            ret = curl_easy_perform(curl);

            curl_slist_free_all(slist);
            curl_formfree(formpost);
        }

        curl_easy_cleanup(curl);
    }

    ASR_PRINT(ASR_DEBUG_TRACE, "uploading is completed, ret is %d\n", ret);

    return ret;
}
