
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "wm_util.h"
#include "lp_list.h"
#include "caiman.h"

#include "asr_debug.h"
#include "asr_json_common.h"
#include "asr_request_json.h"

#include "asr_cmd.h"
#include "asr_cfg.h"
#include "asr_player.h"
#include "asr_authorization.h"

#include "asr_connect.h"
#include "asr_conn_macro.h"

#include "http2_client.h"
#include "multi_partparser.h"
#include "ring_buffer.h"

#include "clova_audio_player.h"
#include "clova_playback_controller.h"
#include "clova_settings.h"

#include "asr_light_effects.h"
#include "common_flags.h"

#include "lguplus_request_json.h"

#define ASR_CONN_INFO(fmt, ...) // fprintf(stderr, "ASR_CONN "fmt, ##__VA_ARGS__)
#define ASR_CONN_TRACE(fmt, ...) fprintf(stderr, "ASR_CONN " fmt, ##__VA_ARGS__)
#define ASR_CONN_ERROR(fmt, ...) fprintf(stderr, "ASR_CONN " fmt, ##__VA_ARGS__)

#define os_calloc(n, size) calloc((n), (size))

#define os_free(ptr) ((ptr != NULL) ? free(ptr) : (void)ptr)

#define os_strdup(ptr) ((ptr != NULL) ? strdup(ptr) : NULL)

#define os_power_on_tick() tickSincePowerOn()

typedef struct _request_item_s {
    http2_request_t *request;
    struct lp_list_head list;
} request_item_t;

struct _asr_connect_s {
    int asr_type;
    int is_online;
    int recognize_cnt;
    int event_exec;
    int timeout_count;
    int first_upload;
    int upload_len;
    /*The ring buffer which cache the record data for recognize*/
    ring_buffer_t *record_data;
    http2_client_t *client;
    // save the request's ptr, need free them when the request finished
    struct lp_list_head request_list;
};

typedef struct _request_result_s {
    int http_ret;
    int request_type;
    int is_multi_part;
    int audio_flag;
    int content_type;
    char *content_id;
    char *json_cache;
    int json_len;
    int json_valid_len;
    multi_partparser_t *multi_paser;
    void *asr_connect;
} request_result_t;

static char *user_agent = NULL;
int h2_reestablish = 0;

int on_part_begin(void *usr_data, const char *key, const char *value)
{
    request_result_t *result = (request_result_t *)usr_data;
    ASR_PRINT(ASR_DEBUG_INFO, "!!!!!!!! on_part_begin !!!!!!!!\n");
    ASR_PRINT(ASR_DEBUG_INFO, "\n  %s = %s\n", key, value);
    if (result) {
        asr_connect_t *self = (asr_connect_t *)result->asr_connect;
        result->content_type = -1;
        if (StrEq(key, CONTENT_TYPE)) {
            if (self->asr_type == e_asr_clova) {
                if (StrEq(value, CHUNCK_OCTET_STREAM)) {
                    result->content_type = CONTENT_AUDIO;
                } else if (StrEq(value, CHUNCK_JSON)) {
                    result->content_type = CONTENT_JSON;
                    if (result->json_cache) {
                        free(result->json_cache);
                        result->json_cache = NULL;
                    }
                }
            } else if (self->asr_type == e_asr_lguplus) {
                if (StrEq(value, LGUP_CHUNCK_OCTET_STREAM)) {
                    result->content_type = CONTENT_AUDIO;
                    if (result->content_id == NULL) {
                        result->audio_flag = 1;
                        result->content_id = lguplus_create_cid_token();
                        asr_player_buffer_start_pcm(result->content_id);
                    }
                } else if (StrEq(value, LGUP_CHUNCK_JSON)) {
                    result->content_type = CONTENT_JSON;
                    if (result->json_cache) {
                        free(result->json_cache);
                        result->json_cache = NULL;
                    }
                }
            }
        }
        if (StrEq(key, CONTENT_ID)) {
            if (result->content_id) {
                free(result->content_id);
                asr_player_buffer_stop(result->content_id, e_asr_finished);
                result->content_id = NULL;
            }
            if (value) {
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

    ASR_PRINT(ASR_DEBUG_INFO, "!!!!!!!! data size is %d content_type = %d !!!!!!!!!!\n", size,
              result->content_type);
    if (result) {
        asr_connect_t *self = (asr_connect_t *)result->asr_connect;
        if (result->content_type == CONTENT_AUDIO) {
            ASR_PRINT(ASR_DEBUG_INFO, "!!!!!!!! audio size is %d !!!!!!!!!!\n", len);
            if (self->asr_type == e_asr_lguplus) {
                if (result->content_id) {
                    asr_player_buffer_write_data(result->content_id, (char *)buffer, len);
                }
            } else {
                if (result->content_id) {
                    ASR_PRINT(ASR_DEBUG_INFO, "!!!!!!!! result->content_id=%s !!!!!!!!!!\n",
                              result->content_id);
                    asr_player_buffer_write_data(result->content_id, (char *)buffer, len);
                }
            }
        } else if (result->content_type == CONTENT_JSON) {
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
    request_result_t *result = (request_result_t *)usr_data;

    ASR_PRINT(ASR_DEBUG_INFO, "!!!!!!!! on_part_end !!!!!!!!\n");

    if (result) {
        asr_connect_t *self = (asr_connect_t *)result->asr_connect;
        if (result->content_type == CONTENT_JSON) {
            if (self->asr_type == e_asr_lguplus) {
                if (result->json_cache) {
                    lguplus_parse_directive_message(result->request_type, result->json_cache);
                    free(result->json_cache);
                    result->json_cache = NULL;
                }
            } else {
                if (result->json_cache) {
                    if (result->request_type == e_downchannel) {
                        asr_result_json_parse(result->json_cache, 1);
                    } else {
                        asr_result_json_parse(result->json_cache, 0);
                    }
                    free(result->json_cache);
                    result->json_cache = NULL;
                }
            }
        } else if (result->content_type == CONTENT_AUDIO) {
            if (self->asr_type == e_asr_clova) {
                if (result->content_id) {
                    asr_player_buffer_stop(result->content_id, e_asr_finished);
                    free(result->content_id);
                    result->content_id = NULL;
                }
            }
        }
        result->content_type = -1;
    }

    return 0;
}

int on_end(void *usr_data)
{
    request_result_t *result = (request_result_t *)usr_data;

    ASR_PRINT(ASR_DEBUG_INFO, "!!!!!!!! onEnd !!!!!!!!\n");

    if (result) {
        asr_connect_t *self = (asr_connect_t *)result->asr_connect;
        if (self->asr_type == e_asr_lguplus) {
            ASR_PRINT(ASR_DEBUG_INFO, "!!!!!!!! onEnd !!!!!!!!\n");
        }
    }

    return 0;
}

static char *asr_header_get_boundary(char *header)
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

static size_t request_read_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
    int want_len = size * nmemb;
    int less_len = size * nmemb;

    asr_connect_t *self = (asr_connect_t *)stream;
    if (!self) {
        ASR_PRINT(ASR_DEBUG_ERR, "the connect is NULL\n");
        return 0;
    }

    if (self->first_upload == 0) {
        self->first_upload = 1;
        ASR_PRINT(ASR_DEBUG_INFO, "[TICK]===== first upload at %lld\n", os_power_on_tick());
    }

    while (!get_asr_talk_start() && less_len > 0) {
        int read_len = 0;
        ring_buffer_t *ring_buffer = (ring_buffer_t *)self->record_data;
        int readable_len = RingBufferReadableLen(ring_buffer);
        int finished = RingBufferGetFinished(ring_buffer);
        if (finished && (readable_len == 0)) {
            if ((want_len - less_len) > 0) {
                ASR_PRINT(ASR_DEBUG_INFO, "last data want_len - less_len = %d at %lld\n",
                          want_len - less_len, os_power_on_tick());
                return want_len - less_len;
            } else {
                ASR_PRINT(ASR_DEBUG_INFO, "[TICK]===== upload finished at %lld\n",
                          os_power_on_tick());
                return 0;
            }
        }
        if (!finished && (readable_len == 0)) {
            self->timeout_count++;
            if (self->timeout_count > 1500) {
                self->timeout_count = 0;
                ASR_PRINT(ASR_DEBUG_INFO, "read_callback read time out\n");
                return 0;
            }
            usleep(2000);
            continue;
        }

        if (self->asr_type == e_asr_lguplus) {
            if (self->upload_len >= LGUPLUS_BUFFER_LEN) {
                return 0;
            } else if (self->upload_len >= LGUPLUS_RECORD_LEN) {
                int len = (want_len - less_len) + strlen(ASR_RECON_PART_END);
                ASR_PRINT(ASR_DEBUG_INFO, "self->upload_len=%d len=%d\n", self->upload_len, len);
                memcpy(ptr, ASR_RECON_PART_END, strlen(ASR_RECON_PART_END));
                self->upload_len += strlen(ASR_RECON_PART_END);

                return len;
            }
        }

        read_len = RingBufferRead(ring_buffer, (char *)ptr, less_len);
        if (read_len > 0) {
            self->timeout_count = 0;
            less_len = less_len - read_len;
            ptr = (char *)ptr + read_len;
            self->upload_len += read_len;
        } else if (read_len == 0) {
            usleep(1000);
        } else {
            ASR_PRINT(ASR_DEBUG_INFO, "no data to read, read error\n");
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
    char *tmp = (char *)buffer;
    char *ptr = (char *)buffer;
    char *ptr1 = NULL;
    char buf[32] = {0};
    size_t len = nmemb * size;
    asr_connect_t *self = NULL;

    request_result_t *result = (request_result_t *)stream;
    if (result) {
        self = (asr_connect_t *)result->asr_connect;
        if (self->asr_type == e_asr_lguplus) {
            if (result->is_multi_part == 0) {
                char *boundary = asr_header_get_boundary((char *)buffer);
                if (boundary) {
                    if (result->multi_paser) {
                        result->is_multi_part = 1;
                        ASR_PRINT(ASR_DEBUG_INFO, "++++++ boundary is %s ++++++\n", boundary);
                        multi_partparser_set_boundary(result->multi_paser, boundary);
                    }
                    if (len > 2 * strlen(boundary)) {
                        ptr = strstr((char *)buffer, boundary);
                        ptr += strlen(boundary);
                        ptr = strstr(ptr, boundary);
                        ptr -= 2 /*\r\n*/;
                        len = len - (ptr - (char *)buffer);
                    }
                    free(boundary);
                }
            }

            if (result->is_multi_part == 1 && result->request_type != e_downchannel) {
                multi_partparser_buffer(result->multi_paser, ptr, len);
            }

            if (result->is_multi_part == 0 || result->request_type == e_downchannel) {
                ASR_PRINT(ASR_DEBUG_INFO, "is_multi_part(%d) data buffer is:\n%s\n",
                          result->is_multi_part, (char *)ptr);
                if (result->json_cache == NULL && result->json_len == 0) {
                    ptr = strstr((char *)buffer, "Content-Length");
                    if (ptr) {
                        ptr += strlen("Content-Length") + 2;
                        ptr1 = strchr(ptr, '\r');
                        if (ptr1) {
                            strncpy(buf, ptr, ptr1 - ptr);
                            result->json_len = atoi(buf);
                            result->json_cache = calloc(1, result->json_len + 100);
                        }
                    }

                    ptr = strstr((char *)buffer, "header");
                    if (ptr && result->json_cache) {
                        ptr -= 2;
                        strncpy(result->json_cache, ptr, strlen(ptr));
                        if (strlen(result->json_cache) == result->json_len) {
                            lguplus_parse_directive_message(result->request_type,
                                                            result->json_cache);
                            free(result->json_cache);
                            result->json_cache = NULL;
                            result->json_len = 0;
                            result->json_valid_len = 0;
                        } else {
                            result->json_valid_len += result->json_len;
                        }
                    } else if (ptr) {
                        ptr -= 2;
                        result->json_cache = calloc(1, strlen(ptr) + 1);
                        result->json_len = strlen(ptr);
                        strncpy(result->json_cache, ptr, result->json_len);
                        lguplus_parse_directive_message(result->request_type, result->json_cache);
                        free(result->json_cache);
                        result->json_cache = NULL;
                        result->json_len = 0;
                        result->json_valid_len = 0;
                    }
                } else {
                    strncpy(result->json_cache + result->json_valid_len, ptr, strlen(ptr));
                    if (strlen(result->json_cache) >= result->json_len) {
                        lguplus_parse_directive_message(result->request_type, result->json_cache);
                        free(result->json_cache);
                        result->json_cache = NULL;
                        result->json_len = 0;
                        result->json_valid_len = 0;
                    } else {
                        result->json_valid_len += strlen(ptr);
                    }
                }
            }
        } else if (self->asr_type == e_asr_clova) {
            if (result->http_ret >= 400) {
                if (asr_unauthor_json_parse((char *)buffer) == 0) {
                    wiimu_log(1, 0, 0, 0, "auth failed, device deregisted???\n");
                }
            }
            if (result->request_type == e_recognize && !get_asr_talk_start()) {
                while (*ptr && len--) {
                    if (StrInclude(ptr, CONTENT_STREAM)) {
                        result->audio_flag = 1;
                        ptr += (strlen(CONTENT_STREAM) + 4) /*\r\n\r\n*/;
                        len -= (strlen(CONTENT_STREAM) + 4) /*\r\n\r\n*/;
                        if (result->request_type == e_recognize) {
                            // asr_player_need_unmute();
                        }
                        break;
                    }
                    ptr++;
                }
            }
            if (result->multi_paser && tmp) {
                ptr = (char *)tmp;
                len = (int)size * nmemb;
                multi_partparser_buffer(result->multi_paser, (char *)ptr, len);
            }
        }
    }

    return nmemb * size;
}

static size_t request_head_recieve(void *ptr, size_t size, size_t nmemb, void *stream)
{
    size_t len = size * nmemb;
    char *boundary = NULL;
    request_result_t *result = (request_result_t *)stream;

    if (result) {
        if (StrInclude((char *)ptr, HEADER_HTTP2_0)) {
            char *http_ret_str = ptr + strlen(HEADER_HTTP2_0);
            if (http_ret_str) {
                result->http_ret = atoi(http_ret_str);
                ASR_PRINT(ASR_DEBUG_INFO, "---- request ret = %d\n", result->http_ret);
            }
            asr_connect_t *self = (asr_connect_t *)result->asr_connect;
            if (self->asr_type == e_asr_lguplus) {
                if (result->request_type == e_downchannel) {
                    if (result->http_ret == 200) {
                        ASR_PRINT(ASR_DEBUG_INFO,
                                  "-------- lguplus down_channel OK at %lld --------\n",
                                  os_power_on_tick());
                        asr_system_add_ping_timer(1);
                        asr_system_add_inactive_timer();
                        self->is_online = 1;
                    }
                }
                if (result->request_type >= e_recognize) {
                    self->event_exec = 0;
                }
            } else if (self->asr_type == e_asr_clova) {
                if (result->request_type == e_downchannel) {
                    if (result->http_ret == 200) {
                        ASR_PRINT(ASR_DEBUG_INFO,
                                  "-------- clova down_channel OK at %lld --------\n",
                                  os_power_on_tick());
                        clova_alerts_request_sync_cmd(NAME_ALERT_REQUEST_SYNC);
                        int is_default = asr_get_default_language();
                        if (is_default == 0) {
                            char *set_lan = asr_get_language();
                            if (set_lan) {
                                asr_set_default_language(1);
                            }
                        }
                        asr_system_add_ping_timer(0);
                        self->is_online = 1;
                        if (h2_reestablish) {
                            h2_reestablish = 0;
                            set_asr_online_only(1);
                        } else {
                            set_asr_online(1);
                        }
                    } else if (result->http_ret == 403) {
                        self->is_online = 0;
                        set_asr_online(0);
                    }
                    if (result->http_ret != 200) {
                        notify_auth_failed(DOWNSTREAM_CREATION_ERROR);
                    }
                } else if (result->request_type == e_recognize) {
                    if (result->http_ret == 200) {
                        asr_player_buffer_stop(NULL, e_asr_stop);
                    }
                } else if (result->request_type == e_event) {
                    self->event_exec = 0;
                }
            }
        }

        boundary = asr_header_get_boundary(ptr);
        if (boundary) {
            ASR_PRINT(ASR_DEBUG_INFO, "++++++ boundary is %s ++++++\n", boundary);
            if (result->multi_paser) {
                result->is_multi_part = 1;
                multi_partparser_set_boundary(result->multi_paser, boundary);
            }
            free(boundary);
        }
    }

    return len;
}

static void asr_free_result(asr_connect_t *self, request_result_t *result)
{
    if (result) {
        if (result->json_cache) {
            free(result->json_cache);
        }
        if (result->content_id) {
            free(result->content_id);
        }
        if (result->multi_paser) {
            multi_partparser_destroy(&result->multi_paser);
        }
        free(result);
    }
}

static void asr_need_talk_stop(int error_code)
{
    char buff[64] = {0};

    if (get_asr_talk_start() == 0) {
        ASR_PRINT(ASR_DEBUG_INFO, "talk to stop %d\n", error_code);
        snprintf(buff, sizeof(buff), TLK_STOP_N, error_code);
        NOTIFY_CMD(ASR_TTS, buff);
    }
}

static void asr_recognize_exit(asr_connect_t *self, http2_request_t *request)
{
    if (request && self) {
        int http_ret = request->http2_ret;
        int error_code = 0;
        request_result_t *result = (request_result_t *)request->recv_body;
        if (self->asr_type == e_asr_clova) {
            if (result && result->request_type == e_recognize) {
                if (get_asr_talk_start() == 0 && result->audio_flag == 0 &&
                    (http_ret == 200 || http_ret == 204)) {
                    ASR_PRINT(ASR_DEBUG_INFO, "talk_start is %d http_ret is %d internet %d\n",
                              get_asr_talk_start(), http_ret, get_internet_flags());
                    error_code = ERR_ASR_PROPMT_STOP;
                }
                if (asr_player_get_state(e_asr_status)) {
                    error_code = ERR_ASR_CONTINUE;
                }
                self->recognize_cnt--;
                if (self->recognize_cnt < 0) {
                    self->recognize_cnt = 0;
                }
                if (error_code != ERR_ASR_CONTINUE && self->recognize_cnt == 0) {
                    asr_need_talk_stop(error_code);
                    if (get_asr_talk_start() == 0 && result->audio_flag == 0) {
                        asr_light_effects_set(e_effect_idle);
                    }
                    common_focus_manage_clear(e_focus_recognize);
                }
            }
        } else if (self->asr_type == e_asr_lguplus) {
            if (result->request_type == e_tts) {
                if (result->content_id) {
                    asr_player_buffer_stop(result->content_id, e_asr_finished);
                }
                asr_need_talk_stop(error_code);
                if (get_asr_talk_start() == 0 && result->audio_flag == 0) {
                    asr_light_effects_set(e_effect_idle);
                }
                common_focus_manage_clear(e_focus_recognize);
            }
        }
    }
}

static void asr_free_request(asr_connect_t *self, http2_request_t **request_p)
{
    ASR_PRINT(ASR_DEBUG_INFO, "+++++ start at %lld\n", os_power_on_tick());

    http2_request_t *request = *request_p;
    if (request) {
        if (self) {
            asr_recognize_exit(self, request);
        }
        if (request->content_body) {
            free(request->content_body);
        }
        if (request->recv_head && request->recv_body && request->recv_head == request->recv_body) {
            request_result_t *recv_data = (request_result_t *)request->recv_head;
            asr_free_result(self, recv_data);
        } else {
            if (request->recv_head) {
                request_result_t *recv_data = (request_result_t *)request->recv_head;
                asr_free_result(self, recv_data);
            }
            if (request->recv_body) {
                request_result_t *recv_data = (request_result_t *)request->recv_body;
                asr_free_result(self, recv_data);
            }
        }
        free(request);
        *request_p = NULL;
    }
    ASR_PRINT(ASR_DEBUG_INFO, "----- end at %lld\n", os_power_on_tick());
}

static http2_request_t *asr_create_request(asr_connect_t *self, int request_type, char *json_body,
                                           int time_out)
{
    int ret = -1;
    http2_request_t *request = NULL;
    request_result_t *recv_data = NULL;
    char *content_part = NULL;

    ASR_PRINT(ASR_DEBUG_INFO, "+++++ start at %lld\n", os_power_on_tick());

    if (!json_body && (request_type >= e_recognize)) {
        ASR_PRINT(ASR_DEBUG_ERR, "input is error\n");
        goto Exit;
    }

    request = (http2_request_t *)calloc(1, sizeof(http2_request_t));
    if (!request) {
        ASR_PRINT(ASR_DEBUG_ERR, "request is malloc failed\n");
        goto Exit;
    }

    if (self->asr_type == e_asr_clova) {
        if (request_type == e_recognize) {
            content_part = RECON_PART;
            self->recognize_cnt++;
        } else if (request_type == e_event) {
            content_part = EVENT_PART;
        }
    } else if (self->asr_type == e_asr_lguplus) {
        if (request_type == e_recognize) {
            content_part = LGUP_RECON_PART;
        } else {
            content_part = LGUP_EVENT_PART;
        }
    }

    if (content_part) {
        asprintf(&request->content_body, content_part, json_body);
        if (!request->content_body) {
            ASR_PRINT(ASR_DEBUG_ERR, "request_body is NULL\n");
            goto Exit;
        }
        request->content_body_left = strlen(request->content_body);
    }

    recv_data = (request_result_t *)calloc(1, sizeof(request_result_t));
    if (!recv_data) {
        ASR_PRINT(ASR_DEBUG_ERR, "recv_data is malloc failed\n");
        goto Exit;
    }

    recv_data->json_cache = NULL;
    recv_data->request_type = request_type;
    recv_data->multi_paser = multi_partparser_create();
    recv_data->asr_connect = self;
    if (!recv_data->multi_paser) {
        ASR_PRINT(ASR_DEBUG_ERR, "multi_paser is malloc failed\n");
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

    if (request_type != e_downchannel) {
        request->last_time_ms = 0;
        request->time_out_ms = (time_out > 0) ? (time_out * 1000) : 0;
    }

    if (request_type == e_recognize) {
        self->first_upload = 0;
        self->timeout_count = 0;
        self->upload_len = 0;
        request->upload_cb = request_read_data;
        request->upload_data = self;
    } else {
        request->upload_cb = NULL;
        request->upload_data = NULL;
    }

    ret = 0;

Exit:

    if (ret != 0) {
        if (request) {
            asr_free_request(self, &request);
            request = NULL;
        }
    }

    ASR_PRINT(ASR_DEBUG_INFO, "----- end at %lld\n", os_power_on_tick());

    return request;
}

static int asr_connect_remove_request(asr_connect_t *self, http2_request_t *request)
{
    int ret = -1;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    request_item_t *list_item = NULL;

    ASR_PRINT(ASR_DEBUG_INFO, "+++++ start at %lld\n", os_power_on_tick());

    if (self) {
        if (!lp_list_empty(&self->request_list)) {
            lp_list_for_each_safe(pos, npos, &self->request_list)
            {
                list_item = lp_list_entry(pos, request_item_t, list);
                if (list_item) {
                    if (request != NULL) {
                        if (list_item->request == request) {
                            lp_list_del(&list_item->list);
                            asr_free_request(self, &list_item->request);
                            free(list_item);
                            list_item = NULL;
                            ret = 0;
                            break;
                        }
                    } else {
                        lp_list_del(&list_item->list);
                        asr_free_request(self, &list_item->request);
                        free(list_item);
                        list_item = NULL;
                        ret = 0;
                    }
                }
            }
        }
        ASR_PRINT(ASR_DEBUG_INFO, "type = %s request number is %d\n",
                  (self->asr_type == e_asr_clova) ? "clova" : "lguplus",
                  lp_list_number(&self->request_list));
    }

    ASR_PRINT(ASR_DEBUG_INFO, "----- end at %lld\n", os_power_on_tick());

    return ret;
}

static void asr_connect_insert_request(asr_connect_t *self, http2_request_t *request)
{
    request_item_t *list_item = NULL;

    ASR_PRINT(ASR_DEBUG_INFO, "+++++ start at %lld\n", os_power_on_tick());

    if (self && request) {
        list_item = (request_item_t *)calloc(1, sizeof(request_item_t));
        if (list_item) {
            list_item->request = request;
            lp_list_add(&list_item->list, &self->request_list);
        } else {
            ASR_PRINT(ASR_DEBUG_ERR, "malloc failed\n");
        }
    }
    ASR_PRINT(ASR_DEBUG_INFO, "----- end at %lld\n", os_power_on_tick());
}

int request_exit_cb(void *request_ptr, void *ctx)
{
    int ret = -1;
    asr_connect_t *self = (asr_connect_t *)ctx;
    http2_request_t *request = (http2_request_t *)request_ptr;

    if (self && request) {
        ret = asr_connect_remove_request(self, request);
        ASR_PRINT(ASR_DEBUG_INFO, "asr_connect_remove_request ret=%d\n", ret);
    }

    return ret;
}

asr_connect_t *asr_connect_create(int asr_type, ring_buffer_t *ring_buffer)
{
    asr_connect_t *self = NULL;

    self = (asr_connect_t *)os_calloc(1, sizeof(asr_connect_t));
    if (self) {
        self->asr_type = asr_type;
        self->record_data = ring_buffer;
        if (!self->record_data) {
            ASR_PRINT(ASR_DEBUG_ERR, "ring_buffer is NULL\n");
            goto Exit;
        }
        self->client = http2_client_create(request_exit_cb, self);
        if (!self->client) {
            goto Exit;
        }
        LP_INIT_LIST_HEAD(&self->request_list);
        return self;
    }

Exit:

    asr_connect_destroy(&self, asr_type);
    return self;
}

void asr_connect_destroy(asr_connect_t **self_p, int asr_type)
{
    asr_connect_t *self = NULL;

    self = *self_p;
    if (self) {
        asr_connect_remove_request(self, NULL);
        if (self->client) {
            http2_client_destroy(&self->client);
        }
        self->record_data = NULL;
        os_free(self);
        *self_p = NULL;
        if (asr_type == e_asr_clova) {
            if (h2_reestablish == 1) {
                h2_reestablish = 0;
            } else {
                h2_reestablish = 1;
            }
            set_asr_online(0);
        }
    }
}

int asr_connect_init(asr_connect_t *self, const char *host_url, int port, long conn_time_out)
{
    int conn_fd = -1;

    if (self && self->client) {
        conn_fd = http2_client_init(self->client, host_url, port, conn_time_out);
        return conn_fd;
    }

    return -1;
}

int asr_connect_can_do_request(asr_connect_t *self)
{
    int ret = 0;

    if (self) {
        ret = self->is_online && (self->event_exec == 0);
    }

    return ret;
}

int asr_connect_io_state(asr_connect_t *self)
{
    int io_status = IO_IDLE;

    if (self) {
        io_status = http2_client_io_status(self->client);
    }

    return io_status;
}

int asr_connect_cancel_stream(asr_connect_t *self, int stream_id)
{
    int ret = -1;

    if (self && stream_id > 0) {
        ret = http2_client_request_cancel(self->client, stream_id);
    }

    return ret;
}

int asr_connect_write(asr_connect_t *self)
{
    int ret = -1;

    if (self) {
        ret = http2_client_send(self->client);
    }

    return ret;
}

int asr_connect_read(asr_connect_t *self)
{
    int ret = -1;

    if (self) {
        ret = http2_client_recv(self->client);
    }

    return ret;
}

int asr_connect_submit_request(asr_connect_t *self, int request_type, char *state_json,
                               char *cmd_js_str, int time_out)
{
    int stream_id = -1;
    http2_request_t *request = NULL;
    char *access_token = NULL;
    char *authorization = NULL;
    char *method = NULL;
    char *path = NULL;
    char *content_type = NULL;
    char *json_body = NULL;

    ASR_PRINT(ASR_DEBUG_INFO, "+++++ start request_type=%d at %lld\n", request_type,
              os_power_on_tick());

    if (!self) {
        ASR_PRINT(ASR_DEBUG_ERR, "connect is NULL\n");
        goto Exit;
    }

    if (self->asr_type == e_asr_clova) {
        access_token = asr_authorization_access_token();
        if (!access_token) {
            ASR_PRINT(ASR_DEBUG_ERR, "access_token is NULL\n");
            goto Exit;
        }

        asprintf(&authorization, HEADER_AUTH, access_token);
        if (!authorization) {
            ASR_PRINT(ASR_DEBUG_ERR, "authorization data malloc failed\n");
            goto Exit;
        }
        if (request_type == e_ping) {
            method = (char *)METHOD_GET;
            path = (char *)PING_PATH;
            content_type = NULL;
            ASR_PRINT(ASR_DEBUG_TRACE, "[CLOVA (Heartbeat-ping)]\n");
        } else if (request_type == e_downchannel) {
            method = (char *)METHOD_GET;
            path = (char *)DIRECTIVES_PATH;
            content_type = NULL;
        } else if (request_type >= e_recognize) {
            method = (char *)METHOD_POST;
            path = (char *)EVENTS_PATH;
            content_type = (char *)EVENT_CONTENTTYPE;
        }
        if (request_type == e_recognize || request_type == e_event) {
            ASR_PRINT(ASR_DEBUG_INFO, "cmd_js_str is:\n%s\n", cmd_js_str);
            json_body = asr_create_event_body(state_json, cmd_js_str);
            if (!json_body) {
                ASR_PRINT(ASR_DEBUG_ERR, "json_body is NULL\n");
                goto Exit;
            }
        }
    } else if (self->asr_type == e_asr_lguplus) {
        access_token = lguplus_get_platform_token();
        if (!access_token) {
            ASR_PRINT(ASR_DEBUG_ERR, "----- platform_token not ready +++ at %lld\n",
                      tickSincePowerOn());
            goto Exit;
        }

        asprintf(&authorization, LGUP_HEADER_AUTH, access_token);
        if (!authorization) {
            ASR_PRINT(ASR_DEBUG_ERR, "authorization data malloc failed\n");
            goto Exit;
        }

        ASR_PRINT(ASR_DEBUG_INFO, "authorization = %s\n", authorization);

        method = (char *)METHOD_POST;
        content_type = (char *)EVENT_CONTENTTYPE;

        // int request_type = e_none;
        json_body = lguplus_create_request_data(cmd_js_str, &request_type);
        if (!json_body) {
            ASR_PRINT(ASR_DEBUG_ERR, "json_body is NULL\n");
            goto Exit;
        }
        if (request_type == e_ping) {
            path = (char *)LGUP_PING_PATH;
        } else if (request_type == e_downchannel) {
            path = (char *)LGUP_DIRECTIVES_PATH;
        } else if (request_type == e_recognize || request_type == e_event) {
            path = (char *)LGUP_EVENTS_PATH;
        } else if (request_type == e_tts) {
            path = (char *)LGUP_TTS_PATH;
        }

        if (request_type == e_recognize) {
            int finished = RingBufferGetFinished(self->record_data);
            if (finished) {
                ASR_PRINT(ASR_DEBUG_INFO, "recoed is finished\n");
                goto Exit;
            }
        }
    }

    request = asr_create_request(self, request_type, json_body, time_out);
    if (!request) {
        ASR_PRINT(ASR_DEBUG_ERR, "request is NULL\n");
        goto Exit;
    }

    if (!user_agent) {
        char *fw_ver = get_device_firmware_version();
        if (fw_ver) {
            if (strstr(fw_ver, "Linkplay.")) {
                fw_ver += strlen("Linkplay.");
            }
            asprintf(&user_agent, REQUEST_USER_AGENT, fw_ver);
        }
    }

    if (self->asr_type == e_asr_clova) {
        if (request_type >= e_recognize) {
            const http2_header_t http2_header[] = {MAKE_NV_CS(HTTP2_METHOD, method),
                                                   MAKE_NV_CS(HTTP2_PATH, path),
                                                   MAKE_NV_CS(HTTP2_SCHEME, PROTO_HTTPS),
                                                   MAKE_NV_CS(HTTP2_AUTHORITY, HOST_URL),
                                                   MAKE_NV_CS(HTTP2_AUTHORIZATION, authorization),
                                                   MAKE_NV_CS(HTTP2_ACCEPT, "*/*"),
                                                   MAKE_NV_CS(HTTP2_USERAGENT, user_agent),
                                                   MAKE_NV_CS(HTTP2_CONTENTTYPE, content_type)};
            stream_id = http2_client_submit_request(
                self->client, http2_header, sizeof(http2_header) / sizeof(http2_header_t), request);
        } else {
            const http2_header_t http2_header[] = {MAKE_NV_CS(HTTP2_METHOD, method),
                                                   MAKE_NV_CS(HTTP2_PATH, path),
                                                   MAKE_NV_CS(HTTP2_SCHEME, PROTO_HTTPS),
                                                   MAKE_NV_CS(HTTP2_AUTHORITY, HOST_URL),
                                                   MAKE_NV_CS(HTTP2_AUTHORIZATION, authorization),
                                                   MAKE_NV_CS(HTTP2_ACCEPT, "*/*"),
                                                   MAKE_NV_CS(HTTP2_USERAGENT, user_agent)};
            stream_id = http2_client_submit_request(
                self->client, http2_header, sizeof(http2_header) / sizeof(http2_header_t), request);
        }
    } else if (self->asr_type == e_asr_lguplus) {
        const http2_header_t http2_header[] = {MAKE_NV_CS(HTTP2_METHOD, method),
                                               MAKE_NV_CS(HTTP2_PATH, path),
                                               MAKE_NV_CS(HTTP2_SCHEME, PROTO_HTTPS),
                                               MAKE_NV_CS(HTTP2_AUTHORITY, LGUP_HOST_URL),
                                               MAKE_NV_CS(HTTP2_AUTHORIZATION, authorization),
                                               MAKE_NV_CS(HTTP2_ACCEPT, "*/*"),
                                               MAKE_NV_CS(HTTP2_USERAGENT, user_agent),
                                               MAKE_NV_CS(HTTP2_CONTENTTYPE, content_type)};
        stream_id = http2_client_submit_request(
            self->client, http2_header, sizeof(http2_header) / sizeof(http2_header_t), request);
    }

    if (stream_id >= 0) {
        // insert to request list
        if (self->asr_type == e_asr_clova) {
            if (request_type == e_event) {
                self->event_exec = 1;
            }
        } else if (self->asr_type == e_asr_lguplus) {
            if (request_type >= e_recognize) {
                self->event_exec = 1;
            }
        }
        asr_connect_insert_request(self, request);
    } else {
        asr_free_request(self, &request);
    }
    ASR_PRINT(ASR_DEBUG_INFO, "add event OK: request=%p stream_id=%d %lld\n", request, stream_id,
              os_power_on_tick());
Exit:

    if (access_token) {
        free(access_token);
    }

    if (authorization) {
        free(authorization);
    }

    if (json_body) {
        free(json_body);
    }

    ASR_PRINT(ASR_DEBUG_INFO, "----- end at %lld\n", os_power_on_tick());

    return stream_id;
}

int asr_connect_check_time_out(asr_connect_t *self)
{
    int ret = 0;
    int need_clear = 0;
    long long time_now = 0;
    http2_request_t *request = NULL;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    request_item_t *list_item = NULL;

    ASR_PRINT(ASR_DEBUG_INFO, "+++++ start at %lld\n", os_power_on_tick());

    if (self && self->client) {
        if (!lp_list_empty(&self->request_list)) {
            lp_list_for_each_safe(pos, npos, &self->request_list)
            {
                list_item = lp_list_entry(pos, request_item_t, list);
                if (list_item && list_item->request) {
                    request = list_item->request;
                    time_now = os_power_on_tick();
                    if (request->stream_id > 0 &&
                        (time_now > request->last_time_ms + request->time_out_ms) &&
                        (request->time_out_ms > 0) && (request->status <= REQUEST_EXIT)) {
                        ASR_PRINT(
                            ASR_DEBUG_INFO,
                            "time out streamid id (%d) status(%d) time_now - last_time = (%lld)\n",
                            request->stream_id, request->status, time_now - request->last_time_ms);
                        need_clear = 1;
                        self->client->error_cnt++;
                        if (request && request->recv_body) {
                            request_result_t *result = (request_result_t *)request->recv_body;
                            if (result->request_type == e_ping) {
                                self->client->error_cnt = 0;
                                ret = -1;
                            }
                        }
                    }
                    break;
                }
            }
        }
        if (self->client->error_cnt > ERROR_MAX_CNT) {
            self->client->error_cnt = 0;
            ret = -1;
        }
        if (need_clear) {
            http2_client_request_exit(self->client, request->stream_id);
        }
    }
    ASR_PRINT(ASR_DEBUG_INFO, "----- end at %lld\n", os_power_on_tick());

    return ret;
}
