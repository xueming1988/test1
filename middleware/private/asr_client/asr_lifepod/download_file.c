#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <curl/curl.h>

#include "download_file.h"
#include "wm_util.h"

int curl_download(char *url, char *header, char *data, char *file)
{
    int ret = 0;
    static CURL *share_handle = NULL;
    CURL *curl_handle = NULL;
    FILE *fp = NULL;
    CURLcode res;
    int tryCount = 2;
    long http_ret = 0;

    fp = fopen(file, "wb+");
    if (!fp) {
        wiimu_log(1, 0, 0, 0, "open %s failed \n", file);
        ret = -1;
        goto err;
    }

    curl_handle = curl_easy_init();
    if (!share_handle) {
        share_handle = curl_share_init();
        curl_share_setopt(share_handle, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    }

    if (curl_handle) {
        if (share_handle) {
            curl_easy_setopt(curl_handle, CURLOPT_SHARE, share_handle);
        }
        curl_easy_setopt(curl_handle, CURLOPT_URL, url);
        curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 20);
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl_handle, CURLOPT_DNS_CACHE_TIMEOUT, 60);
        curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 30);
        curl_easy_setopt(curl_handle, CURLOPT_SERVER_RESPONSE_TIMEOUT, 30);

        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1);

    retry:
        /* Perform the request, res will get the return code */
        wiimu_log(1, 0, 0, 0, "curl_easy_perform(%s) ++ \n", url);
        res = curl_easy_perform(curl_handle);

        /* Check for errors */
        if (res != CURLE_OK) {
            wiimu_log(1, 0, 0, 0, "curl_easy_perform() failed: %d,%s\r\n", res,
                      curl_easy_strerror(res));
            if (CURLE_OPERATION_TIMEOUTED == res || CURLE_COULDNT_RESOLVE_HOST == res ||
                CURLE_COULDNT_CONNECT == res || CURLE_SSL_CONNECT_ERROR == res) {
                if (tryCount--)
                    goto retry;
            }
        }

        wiimu_log(1, 0, 0, 0, "curl_easy_perform() --\n");

        curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_ret);
        if (http_ret != 200) {
            ret = -1;
            wiimu_log(1, 0, 0, 0, "error message\n\n");
        } else {
            ret = 0;
            wiimu_log(1, 0, 0, 0, "OK message\n\n");
        }

        curl_easy_cleanup(curl_handle);
    } else {
        wiimu_log(1, 0, 0, 0, "curl_handle is NULL\n");
    }

    fclose(fp);
err:
    return ret;
}
