
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wm_util.h"

#include "http2_client.h"
#include "http2_client2.h"
#include "multi_partparser.h"

#include "alexa_debug.h"
#include "asr_event.h"
#include "ring_buffer.h"

#include "alexa_authorization.h"
#include "alexa_cfg.h"
#include "alexa_input_controller.h"
#include "alexa_json_common.h"
#include "alexa_request_json.h"
#include "alexa_result_parse.h"

#ifdef EN_AVS_COMMS
#include "alexa_comms.h"
#endif

#include "alexa.h"
#include "alexa_cmd.h"
#include "avs_player.h"
#include "alexa_system.h"
#include "alexa_emplayer.h"

#define PING_PATH ("/ping")
#define EVENTS_PATH ("/v20160207/events")
#define CLUSTER_EVENTS_PATH ("/v20160207/cluster/events")
#define DIRECTIVES_PATH ("/v20160207/directives")

#define EVENT_CONTENTTYPE ("multipart/form-data; boundary=--abcdefg123456")
#define EVENT_PART                                                                                 \
    ("----abcdefg123456\r\n"                                                                       \
     "Content-Disposition: form-data; name=\"metadata\"\r\n"                                       \
     "Content-Type: application/json; charset=UTF-8\r\n\r\n"                                       \
     "%s\r\n----abcdefg123456--\r\n")

#define RECON_PART                                                                                 \
    ("----abcdefg123456\r\n"                                                                       \
     "Content-Disposition: form-data; name=\"metadata\"\r\n"                                       \
     "Content-Type: application/json; charset=UTF-8\r\n\r\n"                                       \
     "%s\r\n\r\n----abcdefg123456\r\n"                                                             \
     "Content-Disposition: form-data; name=\"audio\"\r\n"                                          \
     "Content-Type: application/octet-stream\r\n\r\n")

#define BOUNDARY ("boundary=")
#define CONTENT_TYPE ("Content-Type")
#define CONTENT_ID ("Content-ID")
#define CHUNCK_OCTET_STREAM ("application/octet-stream")
#define CHUNCK_JSON ("application/json")
#define CONTENT_JSON (0)
#define CONTENT_AUDIO (1)

/// Size of CLRF in chars
#define LEADING_CRLF_CHAR_SIZE (2)
/// ASCII value of CR
#define CARRIAGE_RETURN_ASCII (13)
/// ASCII value of LF
#define LINE_FEED_ASCII (10)

enum {
    e_none = -1,
    e_ping,
    e_downchannel,
    e_recognize,
    e_event,

    e_max,
};

typedef struct _request_result_s {
    int http_ret;
    int request_type;
    int content_type;
    int have_tts;
    int have_input_switch;
    int is_first_chunk;
    char *content_id;
    char *json_cache;
    multi_partparser_t *multi_paser;
} request_result_t;

static http2_conn_t *g_conn = NULL;
static http2_conn_t *g_conn2 = NULL;
volatile static int g_timeout_count = 0;
volatile static int g_first_upload = 0;

extern WIIMU_CONTEXT *g_wiimu_shm;

#define UPLOAD_DUMP
#ifdef UPLOAD_DUMP
static FILE *g_fupload = NULL;
#endif

//#define RESULT_DUMP
#ifdef RESULT_DUMP
static FILE *g_result = NULL;
#endif

int report_do_not_disturb_status_to_cloud(void)
{
    if (g_wiimu_shm->do_not_disturb == 1)
        alexa_donotdisturb_cmd(0, NAME_REPORTDONOTDISTURB, true);
    else if (g_wiimu_shm->do_not_disturb == 0)
        alexa_donotdisturb_cmd(0, NAME_REPORTDONOTDISTURB, false);
    return 0;
}

static char *asr_event_get_boundary(char *header)
{
    if (!header) {
        return NULL;
    }

    char *ptr = header;
    char *tmp = NULL;

    while (*ptr) {
        if (StrInclude(ptr, BOUNDARY)) {
            tmp = ptr + strlen(BOUNDARY);
            while (*ptr) {
                if (*ptr == ';' || *ptr == '\r') {
                    break;
                }
                ptr++;
            }
            char *result = (char *)calloc(1, ptr - tmp + 1);
            strncpy(result, tmp, ptr - tmp);
            result[ptr - tmp] = '\0';
            return result;
        }
        ptr++;
    }

    return NULL;
}

int on_part_begin(void *usr_data, const char *key, const char *value)
{
    request_result_t *result = (request_result_t *)usr_data;
    ALEXA_PRINT(ALEXA_DEBUG_INFO, "!!!!!!!! on_part_begin !!!!!!!!\n");
    ALEXA_PRINT(ALEXA_DEBUG_INFO, "\n  %s = %s\n", key, value);
    if (result) {
        result->content_type = -1;
        if (StrEq(key, CONTENT_TYPE)) {
            if (StrEq(value, CHUNCK_OCTET_STREAM)) {
                result->have_tts = 1;
                result->content_type = CONTENT_AUDIO;
            } else if (StrInclude(value, CHUNCK_JSON)) {
                result->content_type = CONTENT_JSON;
                if (result->json_cache) {
                    free(result->json_cache);
                    result->json_cache = NULL;
                }
            }
        }
        if (StrEq(key, CONTENT_ID)) {
            if (result->content_id) {
                avs_player_buffer_stop(result->content_id, e_asr_finished);
                free(result->content_id);
                result->content_id = NULL;
            }
            if (value) {
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "\n  %s = %s\n", key, value);
                result->content_id = strdup(value);
            }
        }
    }
    return 0;
}

int on_part_data(void *usr_data, const char *buffer, size_t size)
{
    request_result_t *result = (request_result_t *)usr_data;
    int len = (int)size;

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "!!!!!!!! data size is %d content_type = %d !!!!!!!!!!\n", size,
                result->content_type);
    if (result && result->content_type == CONTENT_AUDIO) {
        ALEXA_PRINT(ALEXA_DEBUG_INFO, "!!!!!!!! audio size is %d !!!!!!!!!!\n", len);
        if (result->content_id) {
            ALEXA_PRINT(ALEXA_DEBUG_INFO, "!!!!!!!! result->content_id=%s size=%d !!!!!!!!!!\n",
                        result->content_id, len);
            avs_player_buffer_write_data(result->content_id, (char *)buffer, len);
        }

    } else if (result && result->content_type == CONTENT_JSON) {
        ALEXA_PRINT(ALEXA_DEBUG_INFO,
                    "!!!!!!!! on_part_data: len(%d) \n(%.*s)\n !!!!!!!! dataend\n", size, size,
                    buffer);
        if (result->json_cache == NULL) {
            result->json_cache = calloc(1, size + 1);
            if (result->json_cache) {
                memcpy(result->json_cache, buffer, size);
                result->json_cache[size] = '\0';
            }
        } else {
            int old_len = strlen(result->json_cache);
            int new_len = old_len + size + 1;
            result->json_cache = realloc(result->json_cache, new_len);
            if (result->json_cache) {
                memcpy(result->json_cache + old_len, buffer, size);
                result->json_cache[new_len - 1] = '\0';
            }
        }
    }
    return 0;
}

int on_part_end(void *usr_data)
{
    request_result_t *result = (request_result_t *)usr_data;

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "!!!!!!!! on_part_end !!!!!!!!\n");

    if (result) {
        if (result->content_type == CONTENT_JSON) {
            if (result->json_cache) {
                if (directive_check_namespace(result->json_cache, NAMESPACE_INPUTCONTROLLER)) {
                    result->have_input_switch = 1;
                }
                alexa_result_json_parse(result->json_cache, 0);
                free(result->json_cache);
                result->json_cache = NULL;
            }
        } else if (result->content_type == CONTENT_AUDIO) {
            if (result->content_id) {
                avs_player_buffer_stop(result->content_id, e_asr_finished);
                free(result->content_id);
                result->content_id = NULL;
            }
        }
        result->content_type = -1;
    }

    return 0;
}

int on_part_end2(void *usr_data)
{
    request_result_t *result = (request_result_t *)usr_data;

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "!!!!!!!! on_part_end !!!!!!!!\n");

    if (result) {
        if (result->content_type == CONTENT_JSON) {
            if (result->json_cache) {
                if (directive_check_namespace(result->json_cache, NAMESPACE_INPUTCONTROLLER)) {
                    result->have_input_switch = 1;
                }
                lifepod_result_json_parse(result->json_cache, 0);
                free(result->json_cache);
                result->json_cache = NULL;
            }
        } else if (result->content_type == CONTENT_AUDIO) {
            if (result->content_id) {
                avs_player_buffer_stop(result->content_id, e_asr_finished);
                free(result->content_id);
                result->content_id = NULL;
            }
        }
        result->content_type = -1;
    }

    return 0;
}

int on_end(void *usr_data)
{
    ALEXA_PRINT(ALEXA_DEBUG_INFO, "!!!!!!!! onEnd !!!!!!!!\n");

    return 0;
}

static size_t request_read_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
    int want_len = size * nmemb;
    int less_len = size * nmemb;

    if (g_first_upload == 0) {
        g_first_upload = 1;
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "[TICK]===== first upload at %lld\n", tickSincePowerOn());
    }

    while (less_len > 0 && g_wiimu_shm->asr_ongoing == 0) {
        int read_len = 0;

        ring_buffer_t *ring_buffer = (ring_buffer_t *)stream;
        int readable_len = RingBufferReadableLen(ring_buffer);
        int finished = RingBufferGetFinished(ring_buffer);
        if (finished && (readable_len == 0)) {
            if ((want_len - less_len) > 0) {
                ALEXA_PRINT(ALEXA_DEBUG_ERR, "last data want_len - less_len = %d at %lld\n",
                            want_len - less_len, tickSincePowerOn());
                return want_len - less_len;
            } else {
                ALEXA_PRINT(ALEXA_DEBUG_ERR, "[TICK]===== upload finished at %lld\n",
                            tickSincePowerOn());
                return 0;
            }
        }
        if (!finished && (readable_len == 0)) {
            g_timeout_count++;
            if (g_timeout_count > 1500) {
                g_timeout_count = 0;
                ALEXA_PRINT(ALEXA_DEBUG_ERR, "read_callback read time out\n");
                return 0;
            }
            usleep(2000);
            continue;
        }

        read_len = RingBufferRead(ring_buffer, (char *)ptr, less_len);
        if (read_len > 0) {
            g_timeout_count = 0;
#ifdef UPLOAD_DUMP
            if (g_fupload) {
                fwrite((char *)ptr, 1, read_len, g_fupload);
            }
#endif
            // wiimu_log(1,0,0,0,"RingBufferRead and upload %d\n", read_len);
            // ALEXA_PRINT(ALEXA_DEBUG_INFO, "want len %d read len %d\n", (size * nmemb), read_len);
            less_len = less_len - read_len;
            ptr = (char *)ptr + read_len;
        } else if (read_len == 0) {
            usleep(1000);
        } else {
            ALEXA_PRINT(ALEXA_DEBUG_ERR, "no data to read, read error\n");
            return 0;
        }
    }

    if (less_len > 0) {
        ALEXA_PRINT(ALEXA_DEBUG_INFO, "talk_start = %d want_len = %d less_len = %d\n",
                    g_wiimu_shm->asr_ongoing, want_len, less_len);
        return 0;
    }
    return want_len;
}

static size_t request_data_receive(void *buffer, size_t size, size_t nmemb, void *stream)
{
    char *ptr = (char *)buffer;
    size_t len = nmemb * size;

#ifdef RESULT_DUMP
    if (g_result) {
        fwrite((char *)ptr, 1, len, g_result);
    }
#endif

    request_result_t *result = (request_result_t *)stream;
    if (result) {
        if (result->http_ret >= 400) {
            if (alexa_unauthor_json_parse((char *)buffer) == 0) {
                wiimu_log(1, 0, 0, 0, "auth failed, device deregisted???\n");
            } else if (result->http_ret == 429) {
                // TODO: find a way to fixed this too many requests error
                // alexa_clear_authinfo();
            }
        }

        if (result->request_type == e_recognize) {
            if (result->is_first_chunk == 0 && !g_wiimu_shm->asr_ongoing) {
                while (*ptr && len--) {
                    if (StrInclude(ptr, ALEXA_CONTENT_STREAM)) {
                        ptr += (strlen(ALEXA_CONTENT_STREAM) + 4) /*\r\n\r\n*/;
                        len -= (strlen(ALEXA_CONTENT_STREAM) + 4) /*\r\n\r\n*/;
                        focus_process_lock();
                        g_wiimu_shm->TTSPlayStatus = 1;
                        focus_process_unlock();
                        avs_player_need_unmute(VOLUME_VALUE_OF_INIT);
                        break;
                    }
                    ptr++;
                }
            }
        }

        if (result->multi_paser && buffer) {
            ptr = (char *)buffer;
            len = (int)size * nmemb;
            if (result->is_first_chunk == 0) {
                // This code is comfrom AVS-SDK
                if (len >= LEADING_CRLF_CHAR_SIZE && CARRIAGE_RETURN_ASCII == ptr[0] &&
                    LINE_FEED_ASCII == ptr[1]) {
                    ptr += LEADING_CRLF_CHAR_SIZE;
                    len -= LEADING_CRLF_CHAR_SIZE;
                }
                result->is_first_chunk = 1;
            }
            multi_partparser_buffer(result->multi_paser, (char *)ptr, len);
        }
    }

    return nmemb * size;
}

static size_t lifepod_request_data_receive(void *buffer, size_t size, size_t nmemb, void *stream)
{
    char *ptr = (char *)buffer;
    size_t len = nmemb * size;

    request_result_t *result = (request_result_t *)stream;
    if (result) {
        if (result->http_ret >= 400) {
            if (alexa_unauthor_json_parse((char *)buffer) == 0) {
                wiimu_log(1, 0, 0, 0, "auth failed, device deregisted???\n");
            } else if (result->http_ret == 429) {
                // TODO: find a way to fixed this too many requests error
                // alexa_clear_authinfo();
            }
        }

        if (result->request_type == e_recognize) {
            if (result->is_first_chunk == 0 && !g_wiimu_shm->asr_ongoing) {
                while (*ptr && len--) {
                    if (StrInclude(ptr, ALEXA_CONTENT_STREAM)) {
                        ptr += (strlen(ALEXA_CONTENT_STREAM) + 4) /*\r\n\r\n*/;
                        len -= (strlen(ALEXA_CONTENT_STREAM) + 4) /*\r\n\r\n*/;
                        focus_process_lock();
                        g_wiimu_shm->TTSPlayStatus = 1;
                        focus_process_unlock();
                        avs_player_need_unmute(VOLUME_VALUE_OF_INIT);
                        break;
                    }
                    ptr++;
                }
            }
        }

        if (result->multi_paser && buffer) {
            ptr = (char *)buffer;
            len = (int)size * nmemb;
            if (result->is_first_chunk == 0) {
                // This code is comfrom AVS-SDK
                if (len >= LEADING_CRLF_CHAR_SIZE && CARRIAGE_RETURN_ASCII == ptr[0] &&
                    LINE_FEED_ASCII == ptr[1]) {
                    ptr += LEADING_CRLF_CHAR_SIZE;
                    len -= LEADING_CRLF_CHAR_SIZE;
                }
                result->is_first_chunk = 1;
            }
            multi_partparser_buffer(result->multi_paser, (char *)ptr, len);
        }
    }

    return nmemb * size;
}

static size_t request_head_recieve(void *ptr, size_t size, size_t nmemb, void *stream)
{
    size_t len = size * nmemb;
    char *boundary = NULL;
    request_result_t *result = (request_result_t *)stream;

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "header buffer is:\n%s\n", (char *)ptr);
    if (result) {
        if (StrInclude((char *)ptr, HEADER_HTTP2_0)) {
            char *http_ret_str = ptr + strlen(HEADER_HTTP2_0);
            if (http_ret_str) {
                result->http_ret = atoi(http_ret_str);
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "---- request ret = %d\n", result->http_ret);
            }
            if (result->request_type == e_downchannel) {
                if (result->http_ret == 200 && result->is_first_chunk == 0) {
                    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "-------- down_channel OK at %lld --------\n",
                                tickSincePowerOn());
                    alexa_system_cmd(0, NAME_SYNCHORNIZESTATE, 0, NULL);
                    alexa_dump_info.on_line = 1;
                    g_wiimu_shm->alexa_online = 1;
                    avs_system_add_ping_timer();
                    NOTIFY_CMD(NET_GUARD, Alexa_LoginOK);
#ifdef AVS_IOT_CTRL
                    NOTIFY_CMD(AWS_CLIENT, Alexa_LoginOK);
#endif
                } else if (result->http_ret == 403) {
                    result->is_first_chunk = 0;
                }
            }
        }

        boundary = asr_event_get_boundary(ptr);
        if (boundary) {
            ALEXA_PRINT(ALEXA_DEBUG_TRACE, "-------- boundary is %s -------- \n", boundary);
            if (result->multi_paser) {
                multi_partparser_set_boundary(result->multi_paser, boundary);
            }
            free(boundary);
        }
    }

    return len;
}

static size_t lifepod_request_head_recieve(void *ptr, size_t size, size_t nmemb, void *stream)
{
    size_t len = size * nmemb;
    char *boundary = NULL;
    request_result_t *result = (request_result_t *)stream;

    ALEXA_PRINT(ALEXA_DEBUG_INFO, "lifepod header buffer is:\n%s\n", (char *)ptr);
    if (result) {
        if (StrInclude((char *)ptr, HEADER_HTTP2_0)) {
            char *http_ret_str = ptr + strlen(HEADER_HTTP2_0);
            if (http_ret_str) {
                result->http_ret = atoi(http_ret_str);
                ALEXA_PRINT(ALEXA_DEBUG_TRACE, "---- lifepod request ret = %d\n", result->http_ret);
            }
            if (result->request_type == e_downchannel) {
                if (result->http_ret == 200 && result->is_first_chunk == 0) {
                    ALEXA_PRINT(ALEXA_DEBUG_TRACE,
                                "-------- lifepod down_channel OK at %lld --------\n",
                                tickSincePowerOn());
                    lifepod_system_cmd(0, NAME_SYNCHORNIZESTATE, 0, NULL);
                    lifepod_dump_info.on_line = 1;
                    // g_wiimu_shm->alexa_online = 1;
                    lifepod_system_add_ping_timer();
                    // NOTIFY_CMD(NET_GUARD, Alexa_LoginOK);
                } else if (result->http_ret == 403) {
                    result->is_first_chunk = 0;
                }
            }
        }

        boundary = asr_event_get_boundary(ptr);
        if (boundary) {
            ALEXA_PRINT(ALEXA_DEBUG_TRACE, "-------- lifepod boundary is %s -------- \n", boundary);
            if (result->multi_paser) {
                multi_partparser_set_boundary(result->multi_paser, boundary);
            }
            free(boundary);
        }
    }

    return len;
}

static void asr_event_free_result(request_result_t *result)
{
    if (result) {
        if (result->have_tts == 0 && result->have_input_switch == 1) {
            avs_input_controller_do_cmd();
        }
        if (result->json_cache) {
            free(result->json_cache);
        }
        avs_player_buffer_stop(result->content_id, e_asr_finished);
        if (result->content_id) {
            free(result->content_id);
        }
        if (result->multi_paser) {
            multi_partparser_destroy(&result->multi_paser);
        }
        if (result->request_type == e_recognize) {
#ifdef UPLOAD_DUMP
            if (g_fupload) {
                fclose(g_fupload);
                g_fupload = NULL;
            }
#endif
#ifdef RESULT_DUMP
            if (g_result) {
                fclose(g_result);
                g_result = NULL;
            }
#endif
        }
        free(result);
    }
}

static void asr_event_free_request(http2_request_t **request_p)
{
    http2_request_t *request = *request_p;
    if (request) {
        if (request->authorization) {
            free(request->authorization);
        }
        if (request->body) {
            free(request->body);
        }
        if (request->recv_head && request->recv_body && request->recv_head == request->recv_body) {
            request_result_t *recv_data = (request_result_t *)request->recv_head;
            asr_event_free_result(recv_data);
        } else {
            if (request->recv_head) {
                request_result_t *recv_data = (request_result_t *)request->recv_head;
                asr_event_free_result(recv_data);
            }
            if (request->recv_body) {
                request_result_t *recv_data = (request_result_t *)request->recv_body;
                asr_event_free_result(recv_data);
            }
        }
        free(request);
        *request_p = NULL;
    }
}

static http2_request_t *asr_event_create_request(char *json_body, void *upload_data,
                                                 char *request_path)
{
    int ret = -1;
    http2_request_t *request = NULL;
    request_result_t *recv_data = NULL;
    char *access_token = NULL;

    if (!json_body) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "input is error\n");
        return NULL;
    }

    access_token = AlexaGetAccessToken();
    if (!access_token) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "access_token is NULL\n");
        return NULL;
    }

    request = (http2_request_t *)calloc(1, sizeof(http2_request_t));
    if (!request) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "request is malloc failed\n");
        goto Exit;
    }

    request->method = (char *)HTTP_POST;
#ifdef AVS_MRM_ENABLE
    if (request_path) {
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "request_url [%s]\n", request_path);
        request->path = (char *)CLUSTER_EVENTS_PATH;
    } else
#endif
    {
        request->path = (char *)EVENTS_PATH;
    }

    request->content_type = (char *)EVENT_CONTENTTYPE;
    asprintf(&request->authorization, "Bearer %s", access_token);
    if (!request->authorization) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "authorization data malloc failed\n");
        goto Exit;
    }

    if (upload_data) {
        asprintf(&request->body, RECON_PART, json_body);
    } else {
        asprintf(&request->body, EVENT_PART, json_body);
    }
    if (!request->body) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "request_body is NULL\n");
        goto Exit;
    }
    request->body_len = strlen(request->body);
    request->body_left = strlen(request->body);

    recv_data = (request_result_t *)calloc(1, sizeof(request_result_t));
    if (!recv_data) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "recv_data is malloc failed\n");
        goto Exit;
    }

    recv_data->json_cache = NULL;
    recv_data->multi_paser = multi_partparser_create();
    if (!recv_data->multi_paser) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "multi_paser is malloc failed\n");
        goto Exit;
    }
    multi_partparser_set_part_begin_cb(recv_data->multi_paser, on_part_begin, recv_data);
    multi_partparser_set_part_data_cb(recv_data->multi_paser, on_part_data, recv_data);
    multi_partparser_set_part_end_cb(recv_data->multi_paser, on_part_end, recv_data);
    multi_partparser_set_end_cb(recv_data->multi_paser, on_end, recv_data);

    request->recv_head_cb = request_head_recieve;
    request->recv_head = (void *)recv_data;
    request->recv_body_cb = request_data_receive;
    request->recv_body = (void *)recv_data;

    if (upload_data) {
        recv_data->request_type = e_recognize;
        request->upload_cb = request_read_data;
        request->upload_data = upload_data;
        g_timeout_count = 0;
        g_first_upload = 0;
#ifdef UPLOAD_DUMP
        if (alexa_dump_info.record_to_sd && strlen(g_wiimu_shm->mmc_path) > 0) {
            struct tm *pTm = gmtime(&alexa_dump_info.last_dialog_ts);
            char timestr[20];
            char upload_file[128];

            snprintf(timestr, sizeof(timestr), ALEXA_DATE_FMT, pTm->tm_year + 1900, pTm->tm_mon + 1,
                     pTm->tm_mday, pTm->tm_hour, pTm->tm_min, pTm->tm_sec);

            snprintf(upload_file, sizeof(upload_file), ALEXA_UPLOAD_FMT, g_wiimu_shm->mmc_path,
                     timestr);
            g_fupload = fopen(upload_file, "wb+");
            system("rm -rf " ALEXA_UPLOAD_FILE);
        } else {
            g_fupload = fopen(ALEXA_UPLOAD_FILE, "wb+");
        }
#endif
#ifdef RESULT_DUMP
        g_result = fopen(ALEXA_ERROR_FILE, "wb+");
#endif
    } else {
        recv_data->request_type = e_event;
        request->upload_cb = NULL;
        request->upload_data = NULL;
    }

    ret = 0;

Exit:

    if (access_token) {
        free(access_token);
    }

    if (ret != 0) {
        if (request) {
            asr_event_free_request(&request);
            request = NULL;
        }
    }

    return request;
}

static http2_request_t *lifepod_event_create_request(char *json_body, void *upload_data,
                                                     char *request_path)
{
    int ret = -1;
    http2_request_t *request = NULL;
    request_result_t *recv_data = NULL;
    char *access_token = NULL;

    if (!json_body) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "input is error\n");
        return NULL;
    }

    access_token = LifepodGetAccessToken();
    if (!access_token) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "access_token is NULL\n");
        return NULL;
    }

    request = (http2_request_t *)calloc(1, sizeof(http2_request_t));
    if (!request) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "request is malloc failed\n");
        goto Exit;
    }

    request->method = (char *)HTTP_POST;
#ifdef AVS_MRM_ENABLE
    if (request_path) {
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "request_url [%s]\n", request_path);
        request->path = (char *)CLUSTER_EVENTS_PATH;
    } else
#endif
    {
        request->path = (char *)EVENTS_PATH;
    }

    request->content_type = (char *)EVENT_CONTENTTYPE;
    asprintf(&request->authorization, "Bearer %s", access_token);
    if (!request->authorization) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "authorization data malloc failed\n");
        goto Exit;
    }

    if (upload_data) {
        asprintf(&request->body, RECON_PART, json_body);
    } else {
        asprintf(&request->body, EVENT_PART, json_body);
    }
    if (!request->body) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "request_body is NULL\n");
        goto Exit;
    }
    request->body_len = strlen(request->body);
    request->body_left = strlen(request->body);

    recv_data = (request_result_t *)calloc(1, sizeof(request_result_t));
    if (!recv_data) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "recv_data is malloc failed\n");
        goto Exit;
    }

    recv_data->json_cache = NULL;
    recv_data->multi_paser = multi_partparser_create();
    if (!recv_data->multi_paser) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "multi_paser is malloc failed\n");
        goto Exit;
    }
    multi_partparser_set_part_begin_cb(recv_data->multi_paser, on_part_begin, recv_data);
    multi_partparser_set_part_data_cb(recv_data->multi_paser, on_part_data, recv_data);
    multi_partparser_set_part_end_cb(recv_data->multi_paser, on_part_end2, recv_data);
    multi_partparser_set_end_cb(recv_data->multi_paser, on_end, recv_data);

    request->recv_head_cb = lifepod_request_head_recieve;
    request->recv_head = (void *)recv_data;
    request->recv_body_cb = lifepod_request_data_receive;
    request->recv_body = (void *)recv_data;

    if (upload_data) {
        recv_data->request_type = e_recognize;
        request->upload_cb = request_read_data;
        request->upload_data = upload_data;
        g_timeout_count = 0;
        g_first_upload = 0;
#ifdef UPLOAD_DUMP
        if (alexa_dump_info.record_to_sd && strlen(g_wiimu_shm->mmc_path) > 0) {
            struct tm *pTm = gmtime(&alexa_dump_info.last_dialog_ts);
            char timestr[20];
            char upload_file[128];

            snprintf(timestr, sizeof(timestr), ALEXA_DATE_FMT, pTm->tm_year + 1900, pTm->tm_mon + 1,
                     pTm->tm_mday, pTm->tm_hour, pTm->tm_min, pTm->tm_sec);

            snprintf(upload_file, sizeof(upload_file), ALEXA_UPLOAD_FMT, g_wiimu_shm->mmc_path,
                     timestr);
            g_fupload = fopen(upload_file, "wb+");
            system("rm -rf " ALEXA_UPLOAD_FILE);
        } else {
            g_fupload = fopen(ALEXA_UPLOAD_FILE, "wb+");
        }
#endif
#ifdef RESULT_DUMP
        g_result = fopen(ALEXA_ERROR_FILE, "wb+");
#endif
    } else {
        recv_data->request_type = e_event;
        request->upload_cb = NULL;
        request->upload_data = NULL;
    }

    ret = 0;

Exit:

    if (access_token) {
        free(access_token);
    }

    if (ret != 0) {
        if (request) {
            asr_event_free_request(&request);
            request = NULL;
        }
    }

    return request;
}

/*
 *  Judge cmd_js_str is cmd_type/new cmd or not
 *  retrun:
 *        < 0: not an json cmd
 *        = 0: is an json cmd
 *        = 1: is the cmd_type cmd
 */
int asr_event_cmd_type(char *cmd_js_str, char *cmd_type)
{
    int ret = -1;
    json_object *json_cmd = NULL;
    char *event_type = NULL;

    if (!cmd_js_str) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "input is NULL\n");
        return -1;
    }

    json_cmd = json_tokener_parse(cmd_js_str);
    if (!json_cmd) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "cmd_js is not a json\n");
        return -1;
    }

    ret = 0;
    event_type = JSON_GET_STRING_FROM_OBJECT(json_cmd, Key_Event_type);
    if (!event_type) {
        ALEXA_PRINT(ALEXA_DEBUG_INFO, "cmd none name found\n");
        goto Exit;
    }

    if (StrEq(event_type, cmd_type)) {
        ret = 1;
    }

Exit:
    json_object_put(json_cmd);

    return ret;
}

int asr_event_findby_name(char *cmd_js_str, char *event_name)
{
    int ret = -1;
    char *val = NULL;
    json_object *cmd_js = NULL;

    if (!cmd_js_str || !event_name) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "cmd_js is NULL\n");
        return -1;
    }

    cmd_js = json_tokener_parse(cmd_js_str);
    if (!cmd_js) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "cmd_js is not a json\n");
        return -1;
    }

    val = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_NAME);
    if (StrEq(val, event_name)) {
        ret = 0;
    }

    json_object_put(cmd_js);

    return ret;
}

void asr_event_sync_state(void)
{
    int is_default = asr_get_default_language();
    if (is_default == 0) {
        char *set_lan = asr_get_language();
        if (set_lan) {
            asr_set_default_language(1);
            alexa_settings_cmd(0, NAME_SettingUpdated, set_lan);
        }
    }
    avs_system_update_soft_version();
    report_do_not_disturb_status_to_cloud();
    avs_emplayer_notify_online(1);
#ifdef EN_AVS_COMMS
    avs_comms_state_change(e_state_online, e_on);
#endif
#ifdef AVS_MRM_ENABLE
    /* notify avs mrm to update avs connection */
    SocketClientReadWriteMsg("/tmp/alexa_mrm", "GNOTIFY=AlexaConnectionStatus:1",
                             strlen("GNOTIFY=AlexaConnectionStatus:1"), NULL, NULL, 1);
#endif
}

void lifepod_event_sync_state(void)
{
    int is_default = asr_get_default_language();
    if (is_default == 0) {
        char *set_lan = asr_get_language();
        if (set_lan) {
            asr_set_default_language(1);
            alexa_settings_cmd(0, NAME_SettingUpdated, set_lan);
        }
    }
    avs_system_update_soft_version();
    report_do_not_disturb_status_to_cloud();
    avs_emplayer_notify_online(1);
}

/*
 * send an event request use HTTP2
 * Input :
 *       event_type: the event type of the request
 *       event_name: the event name of the request
 *       token: the token will used by the http2 body's payload
 */
int asr_event_exec(char *state_json, char *cmd_js_str, void *upload_data, int time_out)
{
    int ret = E_ERROR_INIT;
    char *request_path = NULL;
    char *json_body = NULL;
    http2_request_t *request = NULL;

    if (!g_conn) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "downchannel is not OK\n");
        return E_ERROR_OFFLINE;
    }

    if (!cmd_js_str || !state_json) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "cmd_js_str is NULL\n");
        return E_ERROR_INIT;
    }

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "+++++ start at %lld\n", tickSincePowerOn());

    json_body = alexa_create_event_body(state_json, cmd_js_str, &request_path);
    if (!json_body) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "json_body is NULL\n");
        ret = E_ERROR_INIT;
        goto Exit;
    }

    request = asr_event_create_request(json_body, upload_data, request_path);
    if (!request) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "asr_event_create_request failed\n");
        ret = E_ERROR_INIT;
        goto Exit;
    }
    ret = http2_conn_do_request(g_conn, request, time_out);
    if (ret == -1) {
        ret = E_ERROR_TIMEOUT;
    } else {
        ret = request->http_ret;
    }
    request_result_t *recv_data = (request_result_t *)request->recv_body;
    if ((recv_data && recv_data->request_type == e_recognize) && request->http_ret != 200) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "talk_start is %d http_ret is %d internet %d\n",
                    g_wiimu_shm->asr_ongoing, request->http_ret, g_wiimu_shm->internet);
    }

Exit:
    if (json_body) {
        free(json_body);
    }
    if (request_path) {
        free(request_path);
    }
    if (request) {
        asr_event_free_request(&request);
    }

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "----- ret=%d end at %lld\n", ret, tickSincePowerOn());

    return ret;
}

int lifepod_event_exec(char *state_json, char *cmd_js_str, void *upload_data, int time_out)
{
    int ret = E_ERROR_INIT;
    char *request_path = NULL;
    char *json_body = NULL;
    http2_request_t *request = NULL;

    if (!g_conn2) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "downchannel is not OK\n");
        return E_ERROR_OFFLINE;
    }

    if (!cmd_js_str || !state_json) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "cmd_js_str is NULL\n");
        return E_ERROR_INIT;
    }

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "+++++ start at %lld\n", tickSincePowerOn());

    json_body = lifepod_create_event_body(state_json, cmd_js_str, &request_path);
    if (!json_body) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "json_body is NULL\n");
        ret = E_ERROR_INIT;
        goto Exit;
    }

    request = lifepod_event_create_request(json_body, upload_data, request_path);
    if (!request) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "lifepod_event_create_request failed\n");
        ret = E_ERROR_INIT;
        goto Exit;
    }
    ret = http2_conn_do_request2(g_conn2, request, time_out);
    if (ret == -1) {
        ret = E_ERROR_TIMEOUT;
    } else {
        ret = request->http_ret;
    }
    request_result_t *recv_data = (request_result_t *)request->recv_body;
    if ((recv_data && recv_data->request_type == e_recognize) && request->http_ret != 200) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "talk_start is %d http_ret is %d internet %d\n",
                    g_wiimu_shm->asr_ongoing, request->http_ret, g_wiimu_shm->internet);
    }

Exit:
    if (json_body) {
        free(json_body);
    }
    if (request_path) {
        free(request_path);
    }
    if (request) {
        asr_event_free_request(&request);
    }

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "----- ret=%d end at %lld\n", ret, tickSincePowerOn());

    return ret;
}

int asr_event_down_channel(void)
{
    int ret = 0;
    char *host_url = NULL;
    char *access_token = NULL;
    char *authorization = NULL;
    http2_request_t *request = NULL;
    request_result_t *recv_data = NULL;

    access_token = AlexaGetAccessToken();
    if (!access_token) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "----- token not ready +++ at %lld\n", tickSincePowerOn());
        return E_ERROR_UNAUTH;
    }

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "+++++ start at %lld\n", tickSincePowerOn());
    asprintf(&authorization, "Bearer %s", access_token);
    if (!authorization) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "authorization data malloc failed\n");
        ret = E_ERROR_INIT;
        goto EXIT_FREE;
    }

    request = (http2_request_t *)calloc(1, sizeof(http2_request_t));
    if (!request) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "request calloc failed\n");
        ret = E_ERROR_INIT;
        goto EXIT_FREE;
    }

    recv_data = (request_result_t *)calloc(1, sizeof(request_result_t));
    if (!recv_data) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "recv_data calloc failed\n");
        ret = E_ERROR_INIT;
        goto EXIT_FREE;
    }

    recv_data->request_type = e_downchannel;
    recv_data->json_cache = NULL;
    recv_data->multi_paser = multi_partparser_create();
    if (!recv_data->multi_paser) {
        ret = E_ERROR_INIT;
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "multi_paser is malloc failed\n");
        goto EXIT_FREE;
    }
    multi_partparser_set_part_begin_cb(recv_data->multi_paser, on_part_begin, recv_data);
    multi_partparser_set_part_data_cb(recv_data->multi_paser, on_part_data, recv_data);
    multi_partparser_set_part_end_cb(recv_data->multi_paser, on_part_end, recv_data);
    multi_partparser_set_end_cb(recv_data->multi_paser, on_end, recv_data);

    request->method = (char *)HTTP_GET;
    request->path = (char *)DIRECTIVES_PATH;
    request->authorization = authorization;

    request->recv_head_cb = request_head_recieve;
    request->recv_head = (void *)recv_data;

    request->recv_body_cb = request_data_receive;
    request->recv_body = (void *)recv_data;

    host_url = asr_get_endpoint();
    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "host_url = %s\n", host_url);
    g_conn = http2_conn_create(host_url, HOST_PORT, 10L);
    if (g_conn) {
        ret = http2_conn_keep_alive(g_conn, request, 0);
    } else {
        ret = E_ERROR_INIT;
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "http2_conn_create failed\n");
    }

#ifdef S11_EVB_EUFY_DOT
    SocketClientReadWriteMsg("/tmp/RequestNetguard", Alexa_LoginFailed, strlen(Alexa_LoginFailed),
                             0, 0, 0);
#endif

    http2_conn_destroy(&g_conn);
    g_conn = NULL;

EXIT_FREE:

    if (access_token) {
        free(access_token);
    }

    if (authorization) {
        free(authorization);
    }

    if (recv_data) {
        if (recv_data->multi_paser) {
            multi_partparser_destroy(&recv_data->multi_paser);
        }
        free(recv_data);
    }

    if (request) {
        free(request);
    }

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "----- end at %lld\n", tickSincePowerOn());

    return ret;
}

int lifepod_event_down_channel(void)
{
    int ret = 0;
    char *host_url = NULL;
    char *access_token = NULL;
    char *authorization = NULL;
    http2_request_t *request = NULL;
    request_result_t *recv_data = NULL;

    access_token = LifepodGetAccessToken();
    if (!access_token) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "----- token not ready +++ at %lld\n", tickSincePowerOn());
        return E_ERROR_UNAUTH;
    }

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "+++++ start at %lld\n", tickSincePowerOn());
    asprintf(&authorization, "Bearer %s", access_token);
    if (!authorization) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "authorization data malloc failed\n");
        ret = E_ERROR_INIT;
        goto EXIT_FREE;
    }

    request = (http2_request_t *)calloc(1, sizeof(http2_request_t));
    if (!request) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "request calloc failed\n");
        ret = E_ERROR_INIT;
        goto EXIT_FREE;
    }

    recv_data = (request_result_t *)calloc(1, sizeof(request_result_t));
    if (!recv_data) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "recv_data calloc failed\n");
        ret = E_ERROR_INIT;
        goto EXIT_FREE;
    }

    recv_data->request_type = e_downchannel;
    recv_data->json_cache = NULL;
    recv_data->multi_paser = multi_partparser_create();
    if (!recv_data->multi_paser) {
        ret = E_ERROR_INIT;
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "multi_paser is malloc failed\n");
        goto EXIT_FREE;
    }
    multi_partparser_set_part_begin_cb(recv_data->multi_paser, on_part_begin, recv_data);
    multi_partparser_set_part_data_cb(recv_data->multi_paser, on_part_data, recv_data);
    multi_partparser_set_part_end_cb(recv_data->multi_paser, on_part_end2, recv_data);
    multi_partparser_set_end_cb(recv_data->multi_paser, on_end, recv_data);

    request->method = (char *)HTTP_GET;
    request->path = (char *)DIRECTIVES_PATH;
    request->authorization = authorization;

    request->recv_head_cb = lifepod_request_head_recieve;
    request->recv_head = (void *)recv_data;

    request->recv_body_cb = lifepod_request_data_receive;
    request->recv_body = (void *)recv_data;

    // host_url = asr_get_endpoint();
    host_url = "avs.lifepod.com";
    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "host_url = %s\n", host_url);
    g_conn2 = http2_conn_create2(host_url, HOST_PORT, 10L);
    if (g_conn2) {
        ret = http2_conn_keep_alive2(g_conn2, request, 0);
    } else {
        ret = E_ERROR_INIT;
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "http2_conn_create2 failed\n");
    }

    http2_conn_destroy2(&g_conn2);
    g_conn2 = NULL;

EXIT_FREE:

    if (access_token) {
        free(access_token);
    }

    if (authorization) {
        free(authorization);
    }

    if (recv_data) {
        if (recv_data->multi_paser) {
            multi_partparser_destroy(&recv_data->multi_paser);
        }
        free(recv_data);
    }

    if (request) {
        free(request);
    }

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "----- end at %lld\n", tickSincePowerOn());

    return ret;
}
