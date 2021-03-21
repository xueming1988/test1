/*
*created by richard.huang 2018-04-10
*
*/
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

#include <sys/socket.h>
#include <sys/un.h>

#include <errno.h>
#include "lp_list.h"
#include "json.h"
#include "wm_util.h"
#include "curl_https.h"
#include "membuffer.h"
#if !(defined(PLATFORM_AMLOGIC) || defined(PLATFORM_INGENIC) || defined(OPENWRT_AVS))
#define HAVE_NVRAM_API 1
#include "nvram.h"
#else
#define HAVE_NVRAM_API 0
#endif

#include "alexa_debug.h"
#include "alexa_api2.h"
#include "alexa_json_common.h"
#include "alexa_products.h"
#include "avs_publish_api.h"
#include "alexa_authorization.h"
#include "spotify_capability.h"

#define AVS_PUBLISH_API_URL "https://api.amazonalexa.com/v1/devices/@self/capabilities"
//#define AVS_PUBLISH_API_HEADER ("Content-Type: application/json\r\nContent-Length:
//%d\r\nAuthorization: %s{{LWA_ACCESS_TOKEN}}")

/*add by weiqiang.huang for publish api V2 2018-07-30 -- begin*/
#define ALEXA_CAPABILITY_FILE_PATH "/system/workdir/vendor/capabilities.xml"
/*add by weiqiang.huang for publish api V2 2018-07-30 -- end*/

extern WIIMU_CONTEXT *g_wiimu_shm;
extern pthread_t avs_publish_api_pid; // add by weiqiang.huang for STAB-264 2018-08-13

/*Data buffer for https post body*/
static long g_body_buff_pos;
static long g_body_buff_size;

/*Data buffer for receive https reponse*/
static membuffer g_response_header;
static membuffer g_response_body;

const char *spotify_api[6] = {"ExternalMediaPlayer",      "Alexa.PlaybackStateReporter",
                              "Alexa.PlaybackController", "Alexa.PlaylistController",
                              "Alexa.SeekController",     "Alexa.FavoritesController"};

int is_bluetooth_modle_supported(void)
{

    if (1) {
        return 1;
    }

    return 0;
}

/*
"Error Message"                 Description
"Invalid envelope version  "            An invalid parameter was published for envelopeVersion .
"Missing capabilities"              The capabilities list is missing.
"{field} cannot be null or empty"   The type, interface, and/or version declared are invalid.
"Unknown interface {name}, type {x}, version {number} combination "
                                                 An invalid combination of type , interface, and/or
version were provided.
--Sample 400 Error:--
{
    "error": {
        "message": "{{STRING}}"
    }
}

*/
/*note: should call free function to release buffer of @param char**ret_buf */
static int avs_publish_api_400_reponse_json_parse(char *message, char **ret_buf)
{
    int ret = -1;
    json_object *json_message = NULL;

    if (message == NULL || ret_buf == NULL) {
        return -1;
    }
    *ret_buf = NULL;

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_publish_api_400_reponse_json_parse ++++\n");

    json_message = json_tokener_parse(message);
    if (json_message) {
        json_object *err_obj = json_object_object_get(json_message, "error");
        if (err_obj) {
            char *res = JSON_GET_STRING_FROM_OBJECT(err_obj, "message");
            if (res) {
                *ret_buf = strdup(res);
                ret = 0;
                ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_publish_api_400_reponse_json_parse:%s\n", res);
            }
        }

        json_object_put(json_message);
    }

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "avs_publish_api_400_reponse_json_parse ----\n");

    return ret;
}

/*
--api version--
Alerts              1.0, 1.1 Yes
AudioPlayer             1.0 Yes
Bluetooth           1.0 Optional
Notifications       1.0 Yes
PlaybackController  1.0 Yes
Settings            1.0 Yes
Speaker                 1.0 Yes
SpeechRecognizer    1.0, 2.0 Yes
SpeechSynthesizer   1.0 Yes
System              1.0 Yes
TemplateRuntime     1.0 Optional
*/
/*
body jason sample
{
    "envelopeVersion": "20160207",
    "capabilities": [
        {
            "type": "AlexaInterface",
            "interface": "Alerts",
            "version": "1.1"
        },
        {
            "type": "AlexaInterface",
            "interface": "AudioPlayer",
            "version": "1.0"
        },
        ...
        // This should be a complete list of
        // interfaces that your product supports.
    ]
}
*/
static json_object *avs_publish_api_create_capability_obj(const char *p_interface,
                                                          const char *p_version)
{
    json_object *capability_obj = NULL;
    json_object *type_obj = NULL;
    json_object *interface_obj = NULL;
    json_object *version_obj = NULL;

    if (p_interface == NULL || p_version == NULL) {
        return NULL;
    }

    capability_obj = json_object_new_object();
    if (capability_obj) {
        type_obj = json_object_new_string("AlexaInterface");
        if (type_obj == NULL) {
            goto Error_leb;
        }
        json_object_object_add(capability_obj, "type", type_obj);

        interface_obj = json_object_new_string(p_interface);
        if (interface_obj == NULL) {
            goto Error_leb;
        }
        json_object_object_add(capability_obj, "interface", interface_obj);

        version_obj = json_object_new_string(p_version);
        if (version_obj == NULL) {
            goto Error_leb;
        }
        json_object_object_add(capability_obj, "version", version_obj);
    }

    if (0) {
    Error_leb:
        json_object_put(capability_obj);
        capability_obj = NULL;
    }

    return capability_obj;
}

static json_object *avs_publish_api_create_capabilities_obj(void)
{
    json_object *capabilities_obj = NULL;

    capabilities_obj = json_object_new_array();
    if (capabilities_obj) {
        {
            // Alerts    1.0, 1.1    Yes
            json_object *tmp_obj = avs_publish_api_create_capability_obj("Alerts", "1.1");
            if (tmp_obj == NULL) {
                goto Error_leb;
            }
            json_object_array_add(capabilities_obj, tmp_obj);
        }
        {
            // AudioPlayer       1.0 Yes
            json_object *tmp_obj = avs_publish_api_create_capability_obj("AudioPlayer", "1.0");
            if (tmp_obj == NULL) {
                goto Error_leb;
            }
            json_object_array_add(capabilities_obj, tmp_obj);
        }
        if (is_bluetooth_modle_supported()) {
            // Bluetooth 1.0 Optional
            json_object *tmp_obj = avs_publish_api_create_capability_obj("Bluetooth", "1.0");
            if (tmp_obj == NULL) {
                goto Error_leb;
            }
            json_object_array_add(capabilities_obj, tmp_obj);
        }
        {
            // Notifications 1.0 Yes
            json_object *tmp_obj = avs_publish_api_create_capability_obj("Notifications", "1.0");
            if (tmp_obj == NULL) {
                goto Error_leb;
            }
            json_object_array_add(capabilities_obj, tmp_obj);
        }
        {
            // PlaybackController    1.0 Yes
            json_object *tmp_obj =
                avs_publish_api_create_capability_obj("PlaybackController", "1.0");
            if (tmp_obj == NULL) {
                goto Error_leb;
            }
            json_object_array_add(capabilities_obj, tmp_obj);
        }
        {
            // Settings  1.0 Yes
            json_object *tmp_obj = avs_publish_api_create_capability_obj("Settings", "1.0");
            if (tmp_obj == NULL) {
                goto Error_leb;
            }
            json_object_array_add(capabilities_obj, tmp_obj);
        }
        {
            // Speaker   1.0 Yes
            json_object *tmp_obj = avs_publish_api_create_capability_obj("Speaker", "1.0");
            if (tmp_obj == NULL) {
                goto Error_leb;
            }
            json_object_array_add(capabilities_obj, tmp_obj);
        }
        {
            // SpeechRecognizer  1.0, 2.0 Yes
            json_object *tmp_obj = avs_publish_api_create_capability_obj("SpeechRecognizer", "2.0");
            if (tmp_obj == NULL) {
                goto Error_leb;
            }
            json_object_array_add(capabilities_obj, tmp_obj);
        }
        {
            // SpeechSynthesizer 1.0 Yes
            json_object *tmp_obj =
                avs_publish_api_create_capability_obj("SpeechSynthesizer", "1.0");
            if (tmp_obj == NULL) {
                goto Error_leb;
            }
            json_object_array_add(capabilities_obj, tmp_obj);
        }
        {
            // System    1.0 Yes
            json_object *tmp_obj = avs_publish_api_create_capability_obj("System", "1.0");
            if (tmp_obj == NULL) {
                goto Error_leb;
            }
            json_object_array_add(capabilities_obj, tmp_obj);
        }
        {
            // TemplateRuntime   1.0 Optional
            json_object *tmp_obj = avs_publish_api_create_capability_obj("TemplateRuntime", "1.0");
            if (tmp_obj == NULL) {
                goto Error_leb;
            }
            json_object_array_add(capabilities_obj, tmp_obj);
        }
        if (get_spotify_capability()) {
            {
                // ExternalMediaPlayer   1.0 Optional
                json_object *tmp_obj =
                    avs_publish_api_create_capability_obj("ExternalMediaPlayer", "1.0");
                if (tmp_obj == NULL) {
                    goto Error_leb;
                }
                json_object_array_add(capabilities_obj, tmp_obj);
            }
            {
                // Alexa.PlaybackStateReporter   1.0 Optional
                json_object *tmp_obj =
                    avs_publish_api_create_capability_obj("Alexa.PlaybackStateReporter", "1.0");
                if (tmp_obj == NULL) {
                    goto Error_leb;
                }
                json_object_array_add(capabilities_obj, tmp_obj);
            }
            {
                // "Alexa.PlaybackController"   1.0 Optional
                json_object *tmp_obj =
                    avs_publish_api_create_capability_obj("Alexa.PlaybackController", "1.0");
                if (tmp_obj == NULL) {
                    goto Error_leb;
                }
                json_object_array_add(capabilities_obj, tmp_obj);
            }
            {
                // Alexa.PlaylistController   1.0 Optional
                json_object *tmp_obj =
                    avs_publish_api_create_capability_obj("Alexa.PlaylistController", "1.0");
                if (tmp_obj == NULL) {
                    goto Error_leb;
                }
                json_object_array_add(capabilities_obj, tmp_obj);
            }
            {
                // Alexa.SeekController   1.0 Optional
                json_object *tmp_obj =
                    avs_publish_api_create_capability_obj("Alexa.SeekController", "1.0");
                if (tmp_obj == NULL) {
                    goto Error_leb;
                }
                json_object_array_add(capabilities_obj, tmp_obj);
            }
            {
                // Alexa.FavoritesController   1.0 Optional
                json_object *tmp_obj =
                    avs_publish_api_create_capability_obj("Alexa.FavoritesController", "1.0");
                if (tmp_obj == NULL) {
                    goto Error_leb;
                }
                json_object_array_add(capabilities_obj, tmp_obj);
            }
        }
        // if MCU disable MRM then remove this capability
        if (!AlexaDisableMrm()) {
#ifdef AVS_MRM_ENABLE
            // MRM Whole Home Audio
            json_object *tmp_obj = avs_publish_api_create_capability_obj("WholeHomeAudio", "1.0");
            if (tmp_obj == NULL) {
                goto Error_leb;
            }
            json_object_array_add(capabilities_obj, tmp_obj);
#endif
        }
    }

    if (0) {
    Error_leb:
        json_object_put(capabilities_obj);
        capabilities_obj = NULL;
    }

    return capabilities_obj;
}

static json_object *json_object_object_dup(json_object *src_js)
{
    json_object *obj_dup = NULL;
    char *str_str = NULL;

    if (src_js) {
        str_str = (char *)json_object_to_json_string(src_js);
        obj_dup = json_tokener_parse(str_str);
    }

    return obj_dup;
}

static char *avs_publish_api_get_conf(void)
{
    int ret = 0;
    int len = 0;
    FILE *fd = NULL;
    char *post_body = NULL;

    fd = fopen(ALEXA_CAPABILITY_FILE_PATH, "rb");
    if (fd) {
        fseek(fd, 0, SEEK_END);
        len = ftell(fd);
        fseek(fd, 0, SEEK_SET);

        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Capability file size is %d\n", len);
        post_body = calloc(1, len + 1);
        if (post_body) {
            int size = (int)fread(post_body, 1, len, fd);
            if (size != len) {
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "read capability failed\n");
                ret = -1;
                goto failed;
            }
            post_body[len] = '\0';
            if (!get_spotify_capability() || AlexaDisableMrm()) {
                json_object *js_obj = json_tokener_parse(post_body);
                if (js_obj) {
                    json_object *list = json_object_object_get(js_obj, "capabilities");
                    json_object *new_list = json_object_new_array();
                    if (list) {
                        int len = json_object_array_length(list);
                        if (len > 0) {
                            int i = 0;
                            for (; i < len; i++) {
                                json_object *item = json_object_array_get_idx(list, i);
                                if (item) {
                                    char *name = JSON_GET_STRING_FROM_OBJECT(item, "interface");
                                    if ((name &&
                                         (!get_spotify_capability() &&
                                          (!strncmp(name, spotify_api[0], strlen(name)) ||
                                           !strncmp(name, spotify_api[0], strlen(name)) ||
                                           !strncmp(name, spotify_api[1], strlen(name)) ||
                                           !strncmp(name, spotify_api[2], strlen(name)) ||
                                           !strncmp(name, spotify_api[3], strlen(name)) ||
                                           !strncmp(name, spotify_api[4], strlen(name)) ||
                                           !strncmp(name, spotify_api[5], strlen(name))))) ||
                                        (name && AlexaDisableMrm() &&
                                         !strncmp(name, "WholeHomeAudio", strlen(name)))) {
                                        ; // remove this item
                                    } else {
                                        json_object *tmp = json_object_object_dup(item);
                                        if (tmp) {
                                            json_object_array_add(new_list, tmp);
                                        }
                                    }
                                }
                            }
                        }
                        json_object_object_del(js_obj, "capabilities");
                        json_object_object_add(js_obj, "capabilities", new_list);
                    }
                    free(post_body);
                    post_body = strdup(json_object_to_json_string(js_obj));
                    json_object_put(js_obj);
                }
            }
        }
    }

failed:
    if (ret != 0) {
        if (post_body) {
            free(post_body);
            post_body = NULL;
        }
    }
    if (fd) {
        fclose(fd);
    }

    return post_body;
}

static int avs_publish_api_create_body_str(char **buff)
{
    int ret = -1;
    char *body_str = NULL;
    json_object *body_obj = NULL;
    json_object *envelopeVersion_val = NULL;
    json_object *capabilities_obj = NULL;

    body_obj = json_object_new_object();
    if (body_obj) {
        envelopeVersion_val = json_object_new_string("20160207");
        if (envelopeVersion_val == NULL) {
            goto Error_leb;
        }
        json_object_object_add(body_obj, "envelopeVersion", envelopeVersion_val);

        capabilities_obj = avs_publish_api_create_capabilities_obj();
        if (capabilities_obj == NULL) {
            goto Error_leb;
        }
        json_object_object_add(body_obj, "capabilities", capabilities_obj);

        body_str = (char *)json_object_to_json_string(body_obj);
        if (body_str) {
            *buff = strdup(body_str);
            ret = 0;
        }
    }

Error_leb:
    json_object_put(body_obj);

    return ret;
}

/*
Content-Type: application/json
Content-Length: {{INTEGER}}
Authorization: {{LWA_ACCESS_TOKEN}}
*/
int avs_publish_api_create_header_list(struct curl_slist **plist_header, long body_size,
                                       const char *access_token)
{
    struct curl_slist *list_header = NULL;
    int ret = -1;
    char *Conent_length = NULL;
    char *Authorization = NULL;

    if (plist_header == NULL) {
        return 0;
    }

    list_header = curl_slist_append(list_header, "Content-Type: application/json");
    if (list_header) {
        asprintf(&Conent_length, "Content-Length: %ld", body_size);
        asprintf(&Authorization, "Authorization: Bearer %s", access_token);

        curl_slist_append(list_header, Conent_length);
        curl_slist_append(list_header, Authorization);
        curl_slist_append(list_header, "Expect:");

        ret = 0;
    }

    *plist_header = list_header;

    free(Conent_length);
    free(Authorization);

    return ret;
}

size_t read_body_buf_callback(char *buffer, size_t size, size_t nitems, void *instream)
{
    // curl_off_t nread;
    /* in real-world cases, this would probably get this data differently
    as this fread() stuff is exactly what the library already would do
    by default internally */
    // size_t retcode = fread(ptr, size, nmemb, stream);
    // nread = (curl_off_t)retcode;
    // fprintf(stderr, "*** We read %" CURL_FORMAT_CURL_OFF_T
    //" bytes from file\n", nread);
    // return retcode;

    size_t ret = size * nitems;

    if (g_body_buff_pos >= g_body_buff_size) {
        return 0;
    }

    if (ret + g_body_buff_pos <= g_body_buff_size) {
        memcpy(buffer, instream + g_body_buff_pos, ret);
        g_body_buff_pos += ret;
    } else {
        ret = g_body_buff_size - g_body_buff_pos;

        memcpy(buffer, instream + g_body_buff_pos, ret);

        g_body_buff_pos += ret;
    }

    return ret;
}

static size_t avs_publish_api_reponse_header_callback(void *ptr, size_t size, size_t nmemb,
                                                      void *stream)
{
    size_t ret_size = nmemb * size;
    membuffer *resp_header = (membuffer *)stream;

    // assert(resp_header == &g_response_header);

    membuffer_append(resp_header, ptr, ret_size);

    return ret_size;
}

static size_t avs_publish_api_reponse_write_data(void *buffer, size_t size, size_t nmemb,
                                                 void *stream)
{
    size_t ret_size = nmemb * size;
    membuffer *resp_body = (membuffer *)stream;

    // char *str = malloc(ret_size + 1);
    // memset(str, 0, (ret_size + 1));
    // memcpy(str, buffer, ret_size);

    // ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Reponse data1 is: %s\n", str);
    // free(str);

    // assert(resp_body == &g_response_body);

    membuffer_append(resp_body, buffer, ret_size);

    // ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Reponse data is: %s\n", resp_body->buf);

    return ret_size;
}

/*
 * send a http1.0 get authorization request to amazon server
 */
int avs_publish_api_put_request(const char *access_token)
{
    char *post_body = NULL;
    struct curl_slist *list_header = NULL;
    CURL *curl = NULL;
    long response_code;
    CURLcode res;
    int tryCount = 3;
    int ret = 0;

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "start +++ at %lld\n", tickSincePowerOn());

    // get http put body string
    if (0 > avs_publish_api_create_body_str(&post_body) || !post_body) {
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "post data malloc failed\n");
        goto Error_leb;
    }
    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "\nHttp put data is:\n%s\n", post_body);
    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Put data len is %d\n", strlen(post_body));

    g_body_buff_pos = 0;
    g_body_buff_size = strlen(post_body);

    // get http header infor
    if (0 > avs_publish_api_create_header_list(&list_header, g_body_buff_size, access_token) ||
        list_header == NULL) {
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "create header list failed\n");
        goto Error_leb;
    }

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, AVS_PUBLISH_API_URL);

        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 3600);

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        // curl_easy_setopt(curl, CURLOPT_CAPATH, "/etc/ssl/certs");
        curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");

        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl/" LIBCURL_VERSION);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); // must be set, otherwise will crash

        // use https put methord
        curl_easy_setopt(curl, CURLOPT_PUT, 1L);

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list_header);

        curl_easy_setopt(curl, CURLOPT_READDATA, post_body);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_body_buf_callback);

        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 35L);
        curl_easy_setopt(curl, CURLOPT_MAXCONNECTS, 10);
        // curl_easy_setopt(curl, CURLOPT_SERVER_RESPONSE_TIMEOUT, 15);

        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        // init buffer to receive reponse data
        membuffer_init(&g_response_header);
        membuffer_init(&g_response_body);

        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION,
                         (void *)avs_publish_api_reponse_header_callback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void *)&g_response_header);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (void *)avs_publish_api_reponse_write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&g_response_body);

        if (0) {
        http_retry:
            /* Perform the request, res will get the return code */
            membuffer_destroy(&g_response_header);
            membuffer_destroy(&g_response_body);
            g_body_buff_pos = 0;
        }

        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res != CURLE_OK) {
            ALEXA_PRINT(ALEXA_DEBUG_TRACE, "curl_easy_perform failed: %d, %s\r\n", res,
                        curl_easy_strerror(res));
            if (CURLE_OPERATION_TIMEOUTED == res || CURLE_COULDNT_RESOLVE_HOST == res ||
                CURLE_COULDNT_CONNECT == res) {
                if (tryCount--) {
                    goto http_retry;
                }
            }
        }

        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Response Header infor: %s.\n", g_response_header.buf);

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        switch (response_code) {
        case 204:
            /*No Content;
               The request was successfully processed.*/
            ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Response code 204, successed!\n");
            ret = 0;
            break;

        case 400:
            /*Bad Request
               There was an issue with the payload. A 400 error will return a JSON document
               that includes an error object with details describing the error.
            */
            {
                char *err_message = NULL;
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Response code 400, Bad Request!\n");

                if (0 ==
                    avs_publish_api_400_reponse_json_parse(g_response_body.buf, &err_message)) {
                    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "400 Mesage is: %s!\n", err_message);
                    free(err_message);
                }
            }
            ret = 400;
            break;

        case 403:
            /*Forbidden
               Authentication failed.*/
            ALEXA_PRINT(
                ALEXA_DEBUG_TRACE,
                "Response code 403, Authentication failed, check your access token please!\n");
            ret = 403;
            break;

        case 500:
            /*Internal Service Error
               An unknown problem was encountered when processing or storing the capabilities.
               Retry after 1 second, then back off exponentially until 256 seconds
               and retry every 256 seconds until an HTTP 204 response is received.*/
            ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Response code 500, retry again!\n");
            tryCount = 3;
            goto http_retry;
            break;

        default:
            ALEXA_PRINT(ALEXA_DEBUG_TRACE, "unknow response code: %ld !\n", response_code);
            break;
        }

        curl_easy_cleanup(curl);
    }

Error_leb:
    if (post_body) {
        free(post_body);
    }

    if (list_header) {
        curl_slist_free_all(list_header);
    }

    membuffer_destroy(&g_response_header);
    membuffer_destroy(&g_response_body);

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "end --- at %lld\n", tickSincePowerOn());

    return ret;
}

/*add by weiqiang.huang for publish api V2 2018-07-30 -- begin*/
static int avs_publish_api_put_request_v2(const char *access_token)
{
    char *post_body = NULL;
    struct curl_slist *list_header = NULL;
    CURL *curl = NULL;
    long response_code;
    CURLcode res;
    int tryCount = 3;
    int ret = -1;

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "v2 start +++ at %lld\n", tickSincePowerOn());

    // get capability file information
    post_body = avs_publish_api_get_conf();
    if (!post_body) {
        ALEXA_PRINT(ALEXA_DEBUG_TRACE,
                    "Error, Capability configure file lost, get default value!\n");
        // get http put body string
        if (0 > avs_publish_api_create_body_str(&post_body) || !post_body) {
            ALEXA_PRINT(ALEXA_DEBUG_TRACE, "post data malloc failed\n");
            goto Error_leb;
        }
    }

    if (!post_body) {
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "post_body is NULl\n");
        goto Error_leb;
    }

    g_body_buff_pos = 0;
    g_body_buff_size = strlen(post_body);
    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "\nHttp put data is:\n%s\n", post_body);

    // get http header infor
    if (0 > avs_publish_api_create_header_list(&list_header, g_body_buff_size, access_token) ||
        list_header == NULL) {
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "create header list failed\n");
        goto Error_leb;
    }

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, AVS_PUBLISH_API_URL);

        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 3600);

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        // curl_easy_setopt(curl, CURLOPT_CAPATH, "/etc/ssl/certs");
        curl_easy_setopt(curl, CURLOPT_CAINFO, "/etc/ssl/certs/ca-certificates.crt");

        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl/" LIBCURL_VERSION);
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L); // must be set, otherwise will crash

        // use https put methord
        curl_easy_setopt(curl, CURLOPT_PUT, 1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list_header);

        curl_easy_setopt(curl, CURLOPT_READDATA, post_body);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_body_buf_callback);

        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 35L);
        curl_easy_setopt(curl, CURLOPT_MAXCONNECTS, 10);
        // curl_easy_setopt(curl, CURLOPT_SERVER_RESPONSE_TIMEOUT, 15);

        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

        // init buffer to receive reponse data
        membuffer_init(&g_response_header);
        membuffer_init(&g_response_body);

        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION,
                         (void *)avs_publish_api_reponse_header_callback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, (void *)&g_response_header);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (void *)avs_publish_api_reponse_write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&g_response_body);

        if (0) {
        http_retry:
            /* Perform the request, res will get the return code */
            membuffer_destroy(&g_response_header);
            membuffer_destroy(&g_response_body);
            g_body_buff_pos = 0;
        }

        res = curl_easy_perform(curl);
        /* Check for errors */
        if (res != CURLE_OK) {
            ALEXA_PRINT(ALEXA_DEBUG_TRACE, "curl_easy_perform failed: %d, %s\r\n", res,
                        curl_easy_strerror(res));
            if (CURLE_OPERATION_TIMEOUTED == res || CURLE_COULDNT_RESOLVE_HOST == res ||
                CURLE_COULDNT_CONNECT == res) {
                if (tryCount--) {
                    goto http_retry;
                }
            }
        }

        ALEXA_PRINT(ALEXA_DEBUG_INFO, "Response Header infor: %s.\n", g_response_header.buf);

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        switch (response_code) {
        case 204:
            /*No Content;
               The request was successfully processed.*/
            ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Response code 204, successed!\n");
            ret = 0;
            break;

        case 400:
            /*Bad Request
               There was an issue with the payload. A 400 error will return a JSON document
               that includes an error object with details describing the error.
            */
            {
                char *err_message = NULL;
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Response code 400, Bad Request!\n");

                if (0 ==
                    avs_publish_api_400_reponse_json_parse(g_response_body.buf, &err_message)) {
                    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "400 Mesage is: %s!\n", err_message);
                    free(err_message);
                }
            }
            ret = 400;
            break;

        case 403:
            /*Forbidden
               Authentication failed.*/
            ALEXA_PRINT(
                ALEXA_DEBUG_TRACE,
                "Response code 403, Authentication failed, check your access token please!\n");
            ret = 403;
            break;

        case 500:
            /*Internal Service Error
               An unknown problem was encountered when processing or storing the capabilities.
               Retry after 1 second, then back off exponentially until 256 seconds
               and retry every 256 seconds until an HTTP 204 response is received.*/
            ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Response code 500, retry again!\n");
            tryCount = 3;
            goto http_retry;
            break;

        default:
            ALEXA_PRINT(ALEXA_DEBUG_TRACE, "unknow response code: %ld !\n", response_code);
            break;
        }

        curl_easy_cleanup(curl);
    }

Error_leb:
    if (post_body) {
        free(post_body);
    }

    if (list_header) {
        curl_slist_free_all(list_header);
    }

    membuffer_destroy(&g_response_header);
    membuffer_destroy(&g_response_body);

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "end --- at %lld\n", tickSincePowerOn());

    return ret;
}
/*add by weiqiang.huang for publish api V2 2018-07-30 -- end*/

void *alexa_publish_supported_api_proc(void *arg)
{
    int flag = 1;
    int res = -1;
    pthread_detach(pthread_self());
    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "start +++ at %lld\n", tickSincePowerOn());

#if 0 /* Per amazon request, we do need republish API on each registration and boot */
#if !HAVE_NVRAM_API
    char buf[32] = {0};
    wm_db_get(AVS_PUBLISH_API_STATUS, buf);
#else
    char *buf = nvram_get(RT2860_NVRAM, AVS_PUBLISH_API_STATUS);
#endif

    if(buf != NULL && strcmp("1", buf) == 0) {
        flag = 0; //already published, do nothing
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "support API already published!\n");
    } else {
        flag  = 1;
    }
#endif

    while (flag) {
        char *alexa_access_token = NULL;

        alexa_access_token = LifepodGetAccessToken();

        res = -1; /* re-init res flag */

        if (g_wiimu_shm->internet == 1 && alexa_access_token) {

            /*modify by weiqiang.huang for publish api V2 2018-07-30 -- begin*/
            res = avs_publish_api_put_request_v2(alexa_access_token);
            /*modify by weiqiang.huang for publish api V2 2018-07-30 -- end*/

            if (0 == res) {
                flag = 0; // publish success

#if 0
                //record status into nvram
#if !HAVE_NVRAM_API
                wm_db_set(AVS_PUBLISH_API_STATUS, "1");
#else
                nvram_set(RT2860_NVRAM, AVS_PUBLISH_API_STATUS, "1");
#endif
#endif

                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Publish API information successed!\n");
            } else if (403 == res) {
                // what should we do when encounter 403 error!;
                ALEXA_PRINT(ALEXA_DEBUG_TRACE,
                            "Publish API response 403 error,check your http put body, please!\n");
            } else if (400 == res) {
                // what should we do when encounter 400 error!;
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Publish API response 400 Authentication failed!\n");
            }
        }

        if (alexa_access_token) {
            free(alexa_access_token);
        }

        if (0 == res) {
            /* cancel the unnecessary sleep if published successfully */
            break;
        }

        sleep(2);
    }

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "end +++ at %lld\n", tickSincePowerOn());

    avs_publish_api_pid = 0; // add by weiqiang.huang for STAB-264 2018-08-13

    return 0; // pthread_exit(0);
}
