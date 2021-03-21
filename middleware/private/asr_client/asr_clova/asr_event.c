
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "wm_util.h"
#include "caiman.h"

#include "asr_debug.h"
#include "asr_json_common.h"
#include "asr_request_json.h"

#include "asr_cmd.h"
#include "asr_cfg.h"
#include "asr_event.h"
#include "asr_player.h"
#include "asr_authorization.h"
#include "asr_light_effects.h"

#include "ring_buffer.h"

#include "clova_audio_player.h"
#include "clova_playback_controller.h"
#include "clova_settings.h"

#define PING_PATH ("/ping")
#define EVENTS_PATH ("/v1/events")
#define DIRECTIVES_PATH ("/v1/directives")
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

#if defined(LGUPLUS_IPTV_ENABLE)
#define REQUEST_USER_AGENT ("CLOVA/ON_PLUS/%s (Android 7.1.1;qcom;clovasdk=1.6.0)")
#else
#define REQUEST_USER_AGENT ("CLOVA/DONUT/%s (Android 7.1.1;qcom;clovasdk=1.6.0)")
#endif

#define BOUNDARY ("boundary=")
#define CONTENT_TYPE ("Content-Type")
#define CONTENT_ID ("Content-Id")
#define CHUNCK_OCTET_STREAM ("application/octet-stream")
#define CHUNCK_JSON ("application/json; charset=utf-8")
#define CONTENT_JSON (0)
#define CONTENT_AUDIO (1)

#define HTTP_CONTENT_STREAM ("Content-Type: application/octet-stream")
#define ASR_UPLOAD_FILE ("/tmp/web/asr_upload.pcm")

extern int g_downchannel_ready;
static http2_conn_t *g_conn = NULL;
volatile static int g_timeout_count = 0;
volatile static int g_first_upload = 0;

//#define UPLOAD_DUMP
#ifdef UPLOAD_DUMP
FILE *g_fupload = NULL;
#endif

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
    char *content_id;
    char *json_cache;
    multi_partparser_t *multi_paser;
} request_result_t;

int on_part_begin(void *usr_data, const char *key, const char *value)
{
    http2_request_t *request = (http2_request_t *)usr_data;
    if (request) {
        request_result_t *result = (request_result_t *)request->recv_body;
        ASR_PRINT(ASR_DEBUG_INFO, "!!!!!!!! on_part_begin !!!!!!!!\n");
        ASR_PRINT(ASR_DEBUG_INFO, "\n  %s = %s\n", key, value);
        if (result) {
            result->content_type = -1;
            if (StrEq(key, CONTENT_TYPE)) {
                if (StrEq(value, CHUNCK_OCTET_STREAM)) {
                    result->content_type = CONTENT_AUDIO;
                } else if (StrEq(value, CHUNCK_JSON)) {
                    result->content_type = CONTENT_JSON;
                    if (result->json_cache) {
                        free(result->json_cache);
                        result->json_cache = NULL;
                    }
                }
            }
            if (StrEq(key, CONTENT_ID)) {
                if (result->content_id) {
                    asr_player_buffer_stop(result->content_id, e_asr_finished);
                    free(result->content_id);
                    result->content_id = NULL;
                }
                if (value) {
                    result->content_id = strdup(value);
                }
            }
        }
    }
    return 0;
}

int on_part_data(void *usr_data, const char *buffer, size_t size)
{
    http2_request_t *request = (http2_request_t *)usr_data;

    if (request) {
        request_result_t *result = (request_result_t *)request->recv_body;
        int len = (int)size;

        ASR_PRINT(ASR_DEBUG_INFO, "!!!!!!!! data size is %d content_type = %d !!!!!!!!!!\n", size,
                  result->content_type);
        if (result && result->content_type == CONTENT_AUDIO) {
            ASR_PRINT(ASR_DEBUG_INFO, "!!!!!!!! audio size is %d !!!!!!!!!!\n", len);
            if (result->content_id) {
                ASR_PRINT(ASR_DEBUG_INFO, "!!!!!!!! result->content_id=%s !!!!!!!!!!\n",
                          result->content_id);
                asr_player_buffer_write_data(result->content_id, (char *)buffer, len);
            }

        } else if (result && result->content_type == CONTENT_JSON) {
            ASR_PRINT(ASR_DEBUG_INFO,
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
    }
    return 0;
}

int on_part_end(void *usr_data)
{
    http2_request_t *request = (http2_request_t *)usr_data;

    if (request) {
        request_result_t *result = (request_result_t *)request->recv_body;
        ASR_PRINT(ASR_DEBUG_INFO, "!!!!!!!! on_part_end !!!!!!!!\n");
        if (result) {
            if (result->content_type == CONTENT_JSON) {
                if (result->json_cache) {
                    asr_result_json_parse(result->json_cache, request->request_id);
                    free(result->json_cache);
                    result->json_cache = NULL;
                }
            } else if (result->content_type == CONTENT_AUDIO) {
                if (result->content_id) {
                    asr_player_buffer_stop(result->content_id, e_asr_finished);
                    free(result->content_id);
                    result->content_id = NULL;
                }
            }
            result->content_type = -1;
        }
    }

    return 0;
}

int on_end(void *usr_data)
{
    // http2_request_t *request = (http2_request_t *)usr_data;
    // request_result_t *result = (request_result_t *)request->recv_body;
    ASR_PRINT(ASR_DEBUG_INFO, "!!!!!!!! onEnd !!!!!!!!\n");

    return 0;
}

static size_t request_read_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
    int want_len = size * nmemb;
    int less_len = size * nmemb;

    if (g_first_upload == 0) {
        g_first_upload = 1;
        ASR_PRINT(ASR_DEBUG_ERR, "[TICK]===== first upload at %lld\n", tickSincePowerOn());
    }

    while (less_len > 0) {
        int read_len = 0;

        ring_buffer_t *ring_buffer = (ring_buffer_t *)stream;
        int readable_len = RingBufferReadableLen(ring_buffer);
        int finished = RingBufferGetFinished(ring_buffer);
        if (finished && (readable_len == 0)) {
            if ((want_len - less_len) > 0) {
                ASR_PRINT(ASR_DEBUG_ERR, "last data want_len - less_len = %d at %lld\n",
                          want_len - less_len, tickSincePowerOn());
                return want_len - less_len;
            } else {
                ASR_PRINT(ASR_DEBUG_ERR, "[TICK]===== upload finished at %lld\n",
                          tickSincePowerOn());
                return 0;
            }
        }
        if (!finished && (readable_len == 0)) {
            g_timeout_count++;
            if (g_timeout_count > 1500) {
                g_timeout_count = 0;
                ASR_PRINT(ASR_DEBUG_ERR, "read_callback read time out\n");
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
            // ASR_PRINT(ASR_DEBUG_INFO, "want len %d read len %d\n", (size * nmemb), read_len);
            less_len = less_len - read_len;
            ptr = (char *)ptr + read_len;
        } else if (read_len == 0) {
            usleep(1000);
        } else {
            ASR_PRINT(ASR_DEBUG_ERR, "no data to read, read error\n");
            return 0;
        }
    }

    if (less_len > 0) {
        ASR_PRINT(ASR_DEBUG_INFO, "talk_start = %d want_len = %d less_len = %d\n",
                  get_asr_talk_start(), want_len, less_len);
        return 0;
    }
    return want_len;
}

static size_t request_data_receive(void *buffer, size_t size, size_t nmemb, void *stream)
{
    char *ptr = (char *)buffer;
    size_t len = nmemb * size;

    request_result_t *result = (request_result_t *)stream;
    if (result) {
        if (result->http_ret >= 400) {
            if (asr_unauthor_json_parse((char *)buffer) == 0) {
                wiimu_log(1, 0, 0, 0, "auth failed, device deregisted???\n");
            }
        }
        if (result->multi_paser && buffer) {
            ptr = (char *)buffer;
            len = (int)size * nmemb;
            multi_partparser_buffer(result->multi_paser, (char *)ptr, len);
        }
    }

    return nmemb * size;
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

static size_t request_head_recieve(void *ptr, size_t size, size_t nmemb, void *stream)
{
    size_t len = size * nmemb;
    char *boundary = NULL;
    request_result_t *result = (request_result_t *)stream;

    ASR_PRINT(ASR_DEBUG_INFO, "header buffer is:\n%s\n", (char *)ptr);
    if (result) {
        if (StrInclude((char *)ptr, HEADER_HTTP2_0)) {
            char *http_ret_str = ptr + strlen(HEADER_HTTP2_0);
            if (http_ret_str) {
                result->http_ret = atoi(http_ret_str);
                ASR_PRINT(ASR_DEBUG_TRACE, "---- request ret = %d\n", result->http_ret);
            }
            if (result->request_type == e_downchannel) {
                if (result->http_ret == 200) {
                    ASR_PRINT(ASR_DEBUG_TRACE, "-------- down_channel OK at %lld --------\n",
                              tickSincePowerOn());
                    if (!get_asr_alert_status())
                        clova_alerts_request_sync_cmd(NAME_ALERT_REQUEST_SYNC);
                    clova_settings_request_sync_event(1);
                    clova_settings_request_version_spec_event();
                    int is_default = asr_get_default_language();
                    if (is_default == 0) {
                        char *set_lan = asr_get_language();
                        if (set_lan) {
                            asr_set_default_language(1);
                        }
                    }
                    set_asr_online(1);
                    asr_system_add_ping_timer();
                } else if (result->http_ret == 401) {
                    asr_handle_unauth_case();
                } else if (result->http_ret == 403) {
                    set_asr_online(0);
                } else {
                    notify_auth_failed(DOWNSTREAM_CREATION_ERROR);
                }
            } else {
                if (result->http_ret == 401) /* disconnected from app, enter OOBE mode */
                    cic_conn_unauth();
            }
        }

        boundary = asr_event_get_boundary(ptr);
        if (boundary) {
            ASR_PRINT(ASR_DEBUG_INFO, "-------- boundary is %s -------- \n", boundary);
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
        if (result->json_cache) {
            free(result->json_cache);
        }
        if (result->content_id) {
            asr_player_buffer_stop(result->content_id, e_asr_finished);
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
        if (request->user_agent) {
            free(request->user_agent);
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

static char *asr_event_get_user_agent(void)
{
    char *fw_ver = NULL;
    char *user_agent = NULL;

    fw_ver = get_device_firmware_version();
    if (fw_ver) {
        if (strstr(fw_ver, "Linkplay.")) {
            fw_ver += strlen("Linkplay.");
        }
        asprintf(&user_agent, REQUEST_USER_AGENT, fw_ver);
    }

    return user_agent;
}

static char *asr_event_get_authorization(void)
{
    char *authorization = NULL;
    char *access_token = NULL;

    access_token = asr_authorization_access_token();
    if (!access_token) {
        ASR_PRINT(ASR_DEBUG_ERR, "access_token is NULL\n");
        return NULL;
    }

    asprintf(&authorization, "Bearer %s", access_token);

    free(access_token);

    return authorization;
}

static http2_request_t *asr_event_create_request(char *json_body, void *upload_data)
{
    int ret = -1;
    http2_request_t *request = NULL;
    request_result_t *recv_data = NULL;

    if (!json_body) {
        ASR_PRINT(ASR_DEBUG_ERR, "input is error\n");
        return NULL;
    }

    request = (http2_request_t *)calloc(1, sizeof(http2_request_t));
    if (!request) {
        ASR_PRINT(ASR_DEBUG_ERR, "request is malloc failed\n");
        goto Exit;
    }

    request->method = (char *)HTTP_POST;
    request->path = (char *)EVENTS_PATH;
    request->content_type = (char *)EVENT_CONTENTTYPE;
    request->authorization = asr_event_get_authorization();
    if (!request->authorization) {
        ASR_PRINT(ASR_DEBUG_ERR, "authorization data malloc failed\n");
        goto Exit;
    }

    request->user_agent = asr_event_get_user_agent();
    if (!request->user_agent) {
        ASR_PRINT(ASR_DEBUG_ERR, "user_agent is create failed\n");
        goto Exit;
    }

    if (upload_data) {
        asprintf(&request->body, RECON_PART, json_body);
    } else {
        asprintf(&request->body, EVENT_PART, json_body);
    }
    if (!request->body) {
        ASR_PRINT(ASR_DEBUG_ERR, "request_body is NULL\n");
        goto Exit;
    }
    request->body_len = strlen(request->body);
    request->body_left = strlen(request->body);

    recv_data = (request_result_t *)calloc(1, sizeof(request_result_t));
    if (!recv_data) {
        ASR_PRINT(ASR_DEBUG_ERR, "recv_data is malloc failed\n");
        goto Exit;
    }

    recv_data->json_cache = NULL;
    recv_data->content_id = NULL;
    recv_data->multi_paser = multi_partparser_create();
    if (!recv_data->multi_paser) {
        ASR_PRINT(ASR_DEBUG_ERR, "multi_paser is malloc failed\n");
        goto Exit;
    }
    multi_partparser_set_part_begin_cb(recv_data->multi_paser, on_part_begin, request);
    multi_partparser_set_part_data_cb(recv_data->multi_paser, on_part_data, request);
    multi_partparser_set_part_end_cb(recv_data->multi_paser, on_part_end, request);
    multi_partparser_set_end_cb(recv_data->multi_paser, on_end, request);

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
        g_fupload = fopen(ASR_UPLOAD_FILE, "wb+");
#endif
    } else {
        recv_data->request_type = e_event;
        request->upload_cb = NULL;
        request->upload_data = NULL;
    }

    ret = 0;

Exit:

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
    char *cmd_params_str = NULL;
    char *event_type = NULL;

    if (!cmd_js_str) {
        ASR_PRINT(ASR_DEBUG_ERR, "input is NULL\n");
        return -1;
    }

    json_cmd = json_tokener_parse(cmd_js_str);
    if (!json_cmd) {
        ASR_PRINT(ASR_DEBUG_INFO, "cmd_js is not a json\n");
        return -1;
    }

    ret = 0;
    event_type = JSON_GET_STRING_FROM_OBJECT(json_cmd, Key_Event_type);
    if (!event_type) {
        ASR_PRINT(ASR_DEBUG_INFO, "cmd none name found\n");
        goto Exit;
    }

    if (StrEq(event_type, cmd_type)) {
        ret = 1;
    }

Exit:
    json_object_put(json_cmd);

    return ret;
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
    int ret = 0;
    char *json_body = NULL;
    http2_request_t *request = NULL;

    if (!g_conn) {
        ASR_PRINT(ASR_DEBUG_ERR, "downchannel is not OK\n");
        return -2;
    }

    if (!cmd_js_str || !state_json) {
        ASR_PRINT(ASR_DEBUG_ERR, "cmd_js_str is NULL\n");
        return -1;
    }

    ASR_PRINT(ASR_DEBUG_TRACE, "+++++ start at %lld\n", tickSincePowerOn());

    json_body = asr_create_event_body(state_json, cmd_js_str);
    if (!json_body) {
        ASR_PRINT(ASR_DEBUG_ERR, "json_body is NULL\n");
        ret = -1;
        goto Exit;
    }

    request = asr_event_create_request(json_body, upload_data);
    if (!request) {
        ASR_PRINT(ASR_DEBUG_ERR, "asr_event_create_request failed\n");
        ret = -1;
        goto Exit;
    }

    if (!ret) {
        ret = http2_conn_do_request(g_conn, request, time_out);
        int http_ret = request->http_ret;
        request_result_t *recv_data = (request_result_t *)request->recv_body;
        if (recv_data && recv_data->request_type == e_recognize) {
            if (http_ret != 200) {
                ASR_PRINT(ASR_DEBUG_ERR, "talk_start is %d http_ret is %d internet %d\n",
                          get_asr_talk_start(), http_ret, get_internet_flags());
                set_session_state(0);
                if ((get_focus_state(1) == SNDSRC_ALERT) && get_asr_alert_status()) {
                    apply_alert_volume();
                }
                ret = (ret < 0 || http_ret > 400 || http_ret <= 0) ? ERR_ASR_EXCEPTION
                                                                   : ERR_ASR_PROPMT_STOP;
            }
            asr_light_effects_set(e_effect_unthinking);
        }
    }

Exit:
    if (json_body) {
        free(json_body);
    }
    if (request) {
        if (request->http_ret == 0 || request->http_ret >= 400) {
            ret = -3;
        }
        asr_event_free_request(&request);
    }

    ASR_PRINT(ASR_DEBUG_TRACE, "----- end at %lld\n", tickSincePowerOn());

    return ret;
}

int asr_event_down_channel(void)
{
    int ret = 0;
    char *host_url = NULL;
    char *authorization = NULL;
    char *user_agent = NULL;
    http2_request_t *request = NULL;
    request_result_t *recv_data = NULL;

    ASR_PRINT(ASR_DEBUG_TRACE, "+++++ start at %lld\n", tickSincePowerOn());
    authorization = asr_event_get_authorization();
    if (!authorization) {
        ASR_PRINT(ASR_DEBUG_ERR, "authorization data malloc failed\n");
        ret = -1;
        goto EXIT_FREE;
    }

    user_agent = asr_event_get_user_agent();
    if (!user_agent) {
        ASR_PRINT(ASR_DEBUG_ERR, "user_agent is create failed\n");
        ret = -1;
        goto EXIT_FREE;
    }

    request = (http2_request_t *)calloc(1, sizeof(http2_request_t));
    if (!request) {
        ASR_PRINT(ASR_DEBUG_ERR, "request calloc failed\n");
        ret = -1;
        goto EXIT_FREE;
    }

    recv_data = (request_result_t *)calloc(1, sizeof(request_result_t));
    if (!recv_data) {
        ASR_PRINT(ASR_DEBUG_ERR, "recv_data calloc failed\n");
        ret = -1;
        goto EXIT_FREE;
    }

    recv_data->request_type = e_downchannel;
    recv_data->json_cache = NULL;
    recv_data->content_id = NULL;
    recv_data->multi_paser = multi_partparser_create();
    if (!recv_data->multi_paser) {
        ASR_PRINT(ASR_DEBUG_ERR, "multi_paser is malloc failed\n");
        goto EXIT_FREE;
    }
    multi_partparser_set_part_begin_cb(recv_data->multi_paser, on_part_begin, request);
    multi_partparser_set_part_data_cb(recv_data->multi_paser, on_part_data, request);
    multi_partparser_set_part_end_cb(recv_data->multi_paser, on_part_end, request);
    multi_partparser_set_end_cb(recv_data->multi_paser, on_end, request);

    request->method = (char *)HTTP_GET;
    request->path = (char *)DIRECTIVES_PATH;
    request->authorization = authorization;
    request->user_agent = user_agent;
    request->recv_head_cb = request_head_recieve;
    request->recv_head = (void *)recv_data;

    request->recv_body_cb = request_data_receive;
    request->recv_body = (void *)recv_data;

    host_url = asr_get_endpoint();
    g_conn = http2_conn_create(host_url, HOST_PORT, 10L);
    if (g_conn) {
        http2_conn_keep_alive(g_conn, request, 0);
    } else {
        ASR_PRINT(ASR_DEBUG_TRACE, "http2_conn_create failed\n");
    }

    http2_conn_destroy(&g_conn);
    g_conn = NULL;
    g_downchannel_ready = 0;

EXIT_FREE:

    if (user_agent) {
        free(user_agent);
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

    ASR_PRINT(ASR_DEBUG_TRACE, "----- end at %lld\n", tickSincePowerOn());

    return ret;
}

int asr_event_ping(void)
{
    int ret = 0;
    char *authorization = NULL;
    char *user_agent = NULL;
    http2_request_t *request = NULL;
    request_result_t *recv_data = NULL;

    if (!g_conn) {
        ASR_PRINT(ASR_DEBUG_ERR, "downchannel is not OK\n");
        return NULL;
    }

    ASR_PRINT(ASR_DEBUG_TRACE, "+++++ start at %lld\n", tickSincePowerOn());
    authorization = asr_event_get_authorization();
    if (!authorization) {
        ASR_PRINT(ASR_DEBUG_ERR, "authorization data malloc failed\n");
        ret = -1;
        goto EXIT_FREE;
    }

    user_agent = asr_event_get_user_agent();
    if (!user_agent) {
        ASR_PRINT(ASR_DEBUG_ERR, "user_agent is create failed\n");
        ret = -1;
        goto EXIT_FREE;
    }

    request = (http2_request_t *)calloc(1, sizeof(http2_request_t));
    if (!request) {
        ASR_PRINT(ASR_DEBUG_ERR, "request calloc failed\n");
        ret = -1;
        goto EXIT_FREE;
    }

    recv_data = (request_result_t *)calloc(1, sizeof(request_result_t));
    if (!recv_data) {
        ASR_PRINT(ASR_DEBUG_ERR, "recv_data calloc failed\n");
        ret = -1;
        goto EXIT_FREE;
    }

    recv_data->request_type = e_ping;
    recv_data->json_cache = NULL;
    recv_data->content_id = NULL;
    recv_data->multi_paser = multi_partparser_create();
    if (!recv_data->multi_paser) {
        ASR_PRINT(ASR_DEBUG_ERR, "multi_paser is malloc failed\n");
        goto EXIT_FREE;
    }

    request->method = (char *)HTTP_GET;
    request->path = (char *)PING_PATH;
    request->authorization = authorization;
    request->user_agent = user_agent;
    request->recv_head_cb = request_head_recieve;
    request->recv_head = (void *)recv_data;

    request->recv_body_cb = request_data_receive;
    request->recv_body = (void *)recv_data;

    ret = http2_conn_do_request(g_conn, request, 5);
    printf("Ping result is %d\n", request->http_ret);

    if (!request->http_ret)
        http2_conn_exit();

EXIT_FREE:

    if (authorization) {
        free(authorization);
    }

    if (user_agent) {
        free(user_agent);
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

    ASR_PRINT(ASR_DEBUG_TRACE, "----- end at %lld\n", tickSincePowerOn());

    return NULL;
}
