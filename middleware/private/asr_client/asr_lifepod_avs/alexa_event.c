
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "wm_util.h"

#include "alexa_event.h"

#include "alexa_cfg.h"
#include "alexa_debug.h"
#include "alexa_json_common.h"
#include "alexa_emplayer_macro.h"
#include "alexa_authorization.h"
#include "alexa_request_json.h"

#include "avs_player.h"

static alexa_conn_t *g_conn = NULL;

#ifdef EN_AVS_ANNOUNCEMENT
#define CHUNCK_OCTET_STREAM ("application/octet-stream")
#define CHUNCK_JSON ("application/json")

#define CONTENT_NONE (-1)
#define CONTENT_JSON (0)
#define CONTENT_AUDIO (1)

int on_part_begin(void *usr_data, const char *key, const char *value)
{
    request_result_t *result = (request_result_t *)usr_data;
    ALEXA_PRINT(ALEXA_DEBUG_INFO, "!!!!!!!! on_part_begin !!!!!!!!!!\n");
    ALEXA_PRINT(ALEXA_DEBUG_INFO, "  %s = %s\n", key, value);
    if (result) {
        result->content_type = CONTENT_NONE;
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
        avs_player_buffer_write_data((char *)buffer, len);
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

    if (result) {
        if (result->content_type == CONTENT_JSON) {
            if (result->json_cache) {
                alexa_result_json_parse(result->json_cache, result->is_next);
                free(result->json_cache);
                result->json_cache = NULL;
            }
        } else if (result->content_type == CONTENT_AUDIO) {
            avs_player_buffer_stop(e_asr_finished);
        }
        result->content_type = CONTENT_NONE;
    }

    return 0;
}

int on_end(void *usr_data)
{
    ALEXA_PRINT(ALEXA_DEBUG_INFO, "!!!!!!!!!!!!!!!!!!!!!!!! onEnd !!!!!!!!!!!!!!!!!!!!!!!!!\n");
    return 0;
}
#endif

static void alexa_free_request(alexa_request_t **request_p)
{
    alexa_request_t *request = *request_p;
    if (request) {
        if (request->authorization) {
            free(request->authorization);
        }
        if (request->body) {
            free(request->body);
        }
        if (request->recv_head && request->recv_body && request->recv_head == request->recv_body) {
            request_result_t *recv_data = (request_result_t *)request->recv_head;
            membuffer_destroy(&recv_data->result_header);
            membuffer_destroy(&recv_data->result_body);
            free(recv_data);
        } else {
            if (request->recv_head) {
                request_result_t *recv_data = (request_result_t *)request->recv_head;
                membuffer_destroy(&recv_data->result_header);
                free(recv_data);
            }
            if (request->recv_body) {
                request_result_t *recv_data = (request_result_t *)request->recv_body;
                membuffer_destroy(&recv_data->result_body);
                free(recv_data);
            }
        }
        free(request);
        *request_p = NULL;
    }
}

static alexa_request_t *alexa_create_request(char *json_body, request_cb_t *head_cb,
                                             request_cb_t *data_cb, request_cb_t *upload_cb,
                                             void *upload_data, int is_next, char *specifiedMrmURI)
{
    int ret = -1;
    alexa_request_t *request = NULL;
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

    request = (alexa_request_t *)calloc(1, sizeof(alexa_request_t));
    if (!request) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "request is malloc failed\n");
        goto Exit;
    }

    request->method = (char *)ALEXA_POST;

#ifdef AVS_MRM_ENABLE
    if (specifiedMrmURI) {
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "specifiedMrmURI [%s]\n", specifiedMrmURI);
        request->path = (char *)ALEXA_MRM_EVENTS_PATH;
    } else
#endif
    {
        request->path = (char *)ALEXA_EVENTS_PATH;
    }

    request->content_type = (char *)ALEXA_EVENT_CONTENTTYPE;
    asprintf(&request->authorization, "Bearer %s", access_token);
    if (!request->authorization) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "authorization data malloc failed\n");
        goto Exit;
    }

    if (upload_cb && upload_data) {
        asprintf(&request->body, ALEXA_RECON_PART, json_body);
    } else {
        asprintf(&request->body, ALEXA_EVENT_PART, json_body);
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

    membuffer_init(&recv_data->result_header);
    membuffer_init(&recv_data->result_body);
#ifdef EN_AVS_ANNOUNCEMENT
    recv_data->multi_paser = NULL;
#endif

    if (head_cb) {
        request->recv_head_cb = head_cb;
        request->recv_head = (void *)recv_data;
    } else {
        request->recv_head_cb = NULL;
        request->recv_head = NULL;
    }
    if (data_cb) {
        request->recv_body_cb = data_cb;
        request->recv_body = (void *)recv_data;
    } else {
        request->recv_body_cb = NULL;
        request->recv_body = NULL;
    }

    if (upload_cb && upload_data) {
        recv_data->is_talk = 1;
        request->upload_cb = upload_cb;
        request->upload_data = upload_data;
    } else {
        recv_data->is_next = is_next;
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
            alexa_free_request(&request);
            request = NULL;
        }
    }

    return request;
}

char *alexa_event_get_boundary(char *header)
{
#define BOUNDARY "boundary="
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

int alexa_check_audio_event(char *cmd_js_str)
{
    int ret = -1;
    json_object *cmd_js = NULL;
    json_object *cmd_param_js = NULL;
    char *name_space = NULL;
    char *event_name = NULL;
    char *play_event_name = NULL;

    if (!cmd_js_str) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "cmd_js is NULL\n");
        return -1;
    }

    cmd_js = json_tokener_parse(cmd_js_str);
    if (!cmd_js) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "cmd_js is not a json\n");
        return -1;
    }

    name_space = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_NAMESPACE);
    event_name = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_NAME);

    cmd_param_js = json_object_object_get(cmd_js, KEY_params);
    if (cmd_param_js) {
        play_event_name = JSON_GET_STRING_FROM_OBJECT(cmd_param_js, Key_eventName);
        ALEXA_PRINT(ALEXA_DEBUG_INFO, "play_event_name(%s)\n", play_event_name);
    }

    if ((StrEq(name_space, NAMESPACE_PLAYBACKCTROLLER)) ||
        (StrEq(name_space, NAMESPACE_Bluetooth)) || (StrEq(event_name, NAME_PLAYSTARTED)) ||
        (StrEq(event_name, NAME_PLAYFAILED)) || (StrEq(event_name, NAME_PLAYFINISHED)) ||
        (StrEq(event_name, NAME_PLAYNEARLYFIN)) || (StrEq(event_name, NAME_SPEECHFINISHED)) ||
        (StrEq(name_space, NAMESPACE_ExternalMediaPlayer) && StrEq(event_name, Val_Login)) ||
        (StrEq(name_space, NAMESPACE_ExternalMediaPlayer) &&
         (StrEq(play_event_name, Val_NoContext) || StrEq(play_event_name, Val_NoPrevious) ||
          StrEq(play_event_name, Val_NoNext))) ||
        StrEq(name_space, NAMESPACE_SIPUSERAGENT)) {
        ret = 0;
    }

    json_object_put(cmd_js);

    return ret;
}

int alexa_check_event_name(char *cmd_js_str, char *event_name)
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

    // this is for when need talk continue, we neednot to send NAME_SPEECHFINISHED to main cmd proc
    int id = JSON_GET_INT_FROM_OBJECT(cmd_js, KEY_id);
    if (id == 1 && StrEq(event_name, NAME_SPEECHFINISHED)) {
        ret = -1;
    }

    json_object_put(cmd_js);

    return ret;
}

/*
  *  Judge cmd_js_str is cmd_type/new cmd or not
  *  retrun:
  *        < 0: not an json cmd
  *        = 0: is an json cmd
  *        = 1: is the cmd_type cmd
  */
int alexa_check_cmd(char *cmd_js_str, char *cmd_type)
{
    int ret = -1;
    json_object *json_cmd = NULL;
    char *cmd_params_str = NULL;
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

static int alexa_check_is_next(char *cmd_js_str)
{
    int ret = -1;
    char *val = NULL;
    json_object *cmd_js = NULL;

    if (!cmd_js_str) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "cmd_js is NULL\n");
        return -1;
    }

    cmd_js = json_tokener_parse(cmd_js_str);
    if (!cmd_js) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "cmd_js is not a json\n");
        return -1;
    }

    val = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_NAME);
    // check is cache for the next item
    int id = JSON_GET_INT_FROM_OBJECT(cmd_js, KEY_id);
    if (id == 1 && StrEq(val, NAME_PLAYNEARLYFIN)) {
        ret = 0;
    }

    json_object_put(cmd_js);

    return ret;
}

/*
 * send an event request use HTTP2
 * Input :
 *       event_type: the event type of the request
 *       event_name: the event name of the request
 *       token: the token will used by the http2 body's payload
 */
int alexa_do_event(char *state_json, char *cmd_js_str, request_cb_t *head_cb, request_cb_t *data_cb,
                   upload_cb_t *upload_cb, void *upload_data, result_exec_cb_t *result_exec,
                   void *result_ctx, int time_out)
{
    int ret = 0;
    int is_next = 0;
    int have_cid_start = -1;
    char *json_body = NULL;
    alexa_request_t *request = NULL;
    char *avsMRMUri = NULL;

    if (!g_conn) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "downchannel is not OK\n");
        return -2;
    }

    if (!cmd_js_str || !state_json) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "cmd_js_str is NULL\n");
        return -1;
    }

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "+++++ start at %lld\n", tickSincePowerOn());

    json_body = alexa_create_event_body(state_json, cmd_js_str, &avsMRMUri);
    if (!json_body) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "json_body is NULL\n");
        ret = -1;
        goto Exit;
    }

    if (alexa_check_is_next(cmd_js_str) == 0) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "is cache for next item to play\n");
        is_next = 1;
    }

    request = alexa_create_request(json_body, head_cb, data_cb, upload_cb, upload_data, is_next,
                                   avsMRMUri);
    if (!request) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "alexa_create_request failed\n");
        ret = -1;
        goto Exit;
    }

    if (result_exec) {
        // printf("alexa_do_event 1\n");
        ret = result_exec(TTS_START, request->http_ret, request->recv_body, result_ctx);
        // printf("alexa_do_event 1 ret=%d\n", ret);
    }

    if (ret == 0) {
#if MAX_RECORD_TIME_IN_MS
        if (upload_cb && upload_data) {
            time_out =
                20 + ((MAX_RECORD_TIME_IN_MS < 8000) ? 0 : (MAX_RECORD_TIME_IN_MS - 8000) / 1000);
        }
#endif
        ret = AlexaNghttp2WaitFin(g_conn, request, time_out);

        // result parse
        if (request && result_exec) {
            ret = result_exec(TTS_PLAYING, request->http_ret, request->recv_body, result_ctx);
            // printf("alexa_do_event 2\n");
        }

        if (result_exec) {
            result_exec(TTS_FINISHED, request->http_ret, request->recv_body, result_ctx);
            // printf("alexa_do_event 3\n");
        }

        // result parse
        if (request && result_exec) {
            ret = result_exec(TTS_STOPED, request->http_ret, request->recv_body, result_ctx);
            // printf("alexa_do_event 4\n");
        }
    }

Exit:
    if (json_body) {
        free(json_body);
        json_body = NULL;
    }

    if (avsMRMUri)
        free(avsMRMUri);

    if (request) {
        if (request->http_ret == 0 || request->http_ret >= 400) {
            ret = -3;
        }
        alexa_free_request(&request);
        request = NULL;
    }

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "----- end at %lld\n", tickSincePowerOn());

    return ret;
}

/*
 *  https://developer.amazon.com/public/solutions/alexa/alexa-voice-service/docs/managing-an-http-2-connection
 *
 *
 */
int alexa_down_channel(request_cb_t *head_cb, request_cb_t *data_cb)
{
    int ret = 0;
    char *host_url = NULL;
    char *access_token = NULL;
    char *authorization = NULL;
    alexa_request_t *request = NULL;
    request_result_t *recv_data = NULL;

    access_token = AlexaGetAccessToken();
    if (!access_token) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "----- token not ready +++ at %lld\n", tickSincePowerOn());
        return -1;
    }

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "+++++ start at %lld\n", tickSincePowerOn());
    asprintf(&authorization, "Bearer %s", access_token);
    if (!authorization) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "authorization data malloc failed\n");
        ret = -1;
        goto EXIT_FREE;
    }

    request = (alexa_request_t *)calloc(1, sizeof(alexa_request_t));
    if (!request) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "request calloc failed\n");
        ret = -1;
        goto EXIT_FREE;
    }

    recv_data = (request_result_t *)calloc(1, sizeof(request_result_t));
    if (!recv_data) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "recv_data calloc failed\n");
        ret = -1;
        goto EXIT_FREE;
    }

    membuffer_init(&recv_data->result_header);
    membuffer_init(&recv_data->result_body);
#ifdef EN_AVS_ANNOUNCEMENT
    recv_data->multi_paser = avs_multi_parse_init();
    if (!recv_data->multi_paser) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "multi_paser is malloc failed\n");
        goto EXIT_FREE;
    }
    avs_multi_parse_set_part_begin_cb(recv_data->multi_paser, on_part_begin, recv_data);
    avs_multi_parse_set_part_data_cb(recv_data->multi_paser, on_part_data, recv_data);
    avs_multi_parse_set_part_end_cb(recv_data->multi_paser, on_part_end, recv_data);
    avs_multi_parse_set_end_cb(recv_data->multi_paser, on_end, recv_data);
#endif
    request->method = (char *)ALEXA_GET;
    request->path = (char *)ALEXA_DIRECTIVES_PATH;
    request->authorization = authorization;

    request->recv_head_cb = head_cb;
    request->recv_head = (void *)recv_data;

    request->recv_body_cb = data_cb;
    request->recv_body = (void *)recv_data;

    host_url = asr_get_endpoint();
    g_conn = AlexaNghttp2Create(host_url, ALEXA_HOST_PORT, 10L);
    if (g_conn) {
        AlexaNghttp2Run(g_conn, request, 0);
    } else {
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "AlexaNghttp2Create failed\n");
    }
#ifdef S11_EVB_EUFY_DOT
    SocketClientReadWriteMsg("/tmp/RequestNetguard", "EufyAlexaLoginFail",
                             strlen("EufyAlexaLoginFail"), 0, 0, 0);
#endif

    AlexaNghttp2Destroy(&g_conn);
    g_conn = NULL;

EXIT_FREE:

    if (access_token) {
        free(access_token);
    }

    if (authorization) {
        free(authorization);
        authorization = NULL;
    }

    if (recv_data) {
        membuffer_destroy(&recv_data->result_header);
        membuffer_destroy(&recv_data->result_body);
#ifdef EN_AVS_ANNOUNCEMENT
        if (recv_data->multi_paser) {
            avs_multi_parse_uninit(&recv_data->multi_paser);
        }
#endif
        free(recv_data);
        recv_data = NULL;
    }

    if (request) {
        free(request);
        request = NULL;
    }

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, "----- end at %lld\n", tickSincePowerOn());

    return ret;
}
