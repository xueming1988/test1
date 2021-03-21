
#include <stdio.h>
#include <stdlib.h>

#include "json.h"
#include "asr_json_common.h"
#include "asr_debug.h"
#include "http2_client.h"

#include "clova_update.h"

static UPDATE_T *g_update = NULL;

static json_object *generate_update_json(UPDATE_T *update)
{
    json_object *js_update = NULL;

    if (update) {
        js_update = json_object_new_object();
        if (js_update) {
            json_object_object_add(js_update, CLOVA_PAYLOAD_STATE,
                                   JSON_OBJECT_NEW_STRING(update->state));
            json_object_object_add(js_update, CLOVA_PAYLOAD_VERSION,
                                   JSON_OBJECT_NEW_STRING(update->version));
            if (update->new_version && strlen(update->new_version))
                json_object_object_add(js_update, CLOVA_PAYLOAD_NEW_VERSION,
                                       json_object_new_string(update->new_version));
            if (update->progress >= 0 && update->progress <= 100 &&
                StrEq(update->state, CLOVA_PAYLOAD_DOWNLOADING))
                json_object_object_add(js_update, CLOVA_PAYLOAD_PROGRESS,
                                       json_object_new_int(update->progress));
        }
    }

    return js_update;
}

static char *generate_update_payload(const char *payload_content, int ready, json_object *js_update)
{
    char *payload = NULL;
    char *val_str = NULL;
    json_object *js_payload = NULL;

    if (js_update) {
        js_payload = json_object_new_object();
        if (js_payload) {
            json_object_object_add(js_payload, payload_content, js_update);

            if (ready == 1 || ready == 0) {
                json_object_object_add(js_payload, CLOVA_PAYLOAD_READY,
                                       json_object_new_boolean(ready));
            }

            val_str = json_object_to_json_string(js_payload);
            if (val_str) {
                payload = strdup(val_str);
            }

            json_object_put(js_payload);
        }
    }

    return payload;
}

static int send_report_ready_state(UPDATE_T *update)
{
    int ret = -1;
    char *name_space = NULL;
    char *name = NULL;
    char *payload = NULL;
    char *payload_content = NULL;
    char *cmd = NULL;
    json_object *js_update = NULL;

    name_space = CLOVA_NAMESPACE_SYSTME;
    name = CLOVA_NAME_REPORT_UPDATE_READY_STATE;
    payload_content = CLOVA_PAYLOAD_STATE;

    if (update) {
        js_update = generate_update_json(update);
        if (js_update) {
            payload = generate_update_payload(payload_content, update->ready, js_update);
            if (!payload) {
                json_object_put(js_update);
            } else {
                cmd = create_cmd(name_space, name, payload);
                if (cmd) {
                    ret = insert_cmd_queue(cmd);
                }
            }
        }
    }

    if (payload) {
        free(payload);
    }

    if (cmd) {
        free(cmd);
    }

    return ret;
}

static int send_report_client_state(UPDATE_T *update)
{
    int ret = -1;
    char *name_space = NULL;
    char *name = NULL;
    char *payload = NULL;
    char *payload_content = NULL;
    char *cmd = NULL;
    json_object *js_update = NULL;

    name_space = CLOVA_NAMESPACE_SYSTME;
    name = CLOVA_NAME_REPORT_CLIENT_STATE;
    payload_content = CLOVA_PAYLOAD_UPDATE;

    if (update) {
        js_update = generate_update_json(update);
        if (js_update) {
            payload = generate_update_payload(payload_content, -1, js_update);
            if (!payload) {
                json_object_put(js_update);
            } else {
                cmd = create_cmd(name_space, name, payload);
                if (cmd) {
                    ret = insert_cmd_queue(cmd);
                }
            }
        }
    }

    if (payload) {
        free(payload);
    }

    if (cmd) {
        free(cmd);
    }

    return ret;
}

void update_OTA_version(const char *old_ver, const char *new_ver)
{
    if (g_update && old_ver && new_ver) {
        if (g_update->version) {
            free(g_update->version);
            g_update->version = NULL;
        }
        g_update->version = strdup(old_ver);

        if (g_update->new_version) {
            free(g_update->new_version);
            g_update->new_version = NULL;
        }
        if (strlen(new_ver) && !StrEq(old_ver, new_ver))
            g_update->new_version = strdup(new_ver);

        if (g_update->flag) {
            g_update->flag = 0;
            g_update->ready = 1;
            if (g_update->new_version) {
                volume_pormpt_action(VOLUME_BELL_4);
                sleep(1);
            }
            send_report_ready_state(g_update);
            g_update->ready = 0;
        } else if (g_update->app_flag) {
            g_update->app_flag = 0;
            send_report_client_state(g_update);
        }

        ASR_PRINT(ASR_DEBUG_TRACE, "version: %s, new_version: %s\n", g_update->version,
                  g_update->new_version);
    }
}

void update_OTA_state(const char *state)
{
    if (g_update && state) {
        if (StrEq(state, CLOVA_PAYLOAD_AVAILABLE)) {
            if (StrEq(g_update->state, CLOVA_PAYLOAD_LATEST)) {
                free(g_update->state);
                g_update->state = strdup(state);
            }
        } else if (StrEq(state, CLOVA_PAYLOAD_DOWNLOADING)) {
            if (StrEq(g_update->state, CLOVA_PAYLOAD_AVAILABLE)) {
                free(g_update->state);
                g_update->state = strdup(state);
            }
        } else if (StrEq(state, CLOVA_PAYLOAD_UPDATING)) {
            if (StrEq(g_update->state, CLOVA_PAYLOAD_DOWNLOADING) ||
                StrEq(g_update->state, CLOVA_PAYLOAD_HOLDING)) {
                free(g_update->state);
                g_update->state = strdup(state);
                g_update->progress = -1;
                send_report_client_state(g_update);
                free(g_update->state);
                g_update->state = strdup(CLOVA_PAYLOAD_LATEST);
                g_update->error = OTA_NONE;
            }
        } else if (StrEq(state, CLOVA_PAYLOAD_HOLDING_DISTURB)) {
            if (StrEq(g_update->state, CLOVA_PAYLOAD_DOWNLOADING)) {
                free(g_update->state);
                g_update->state = strdup(CLOVA_PAYLOAD_HOLDING);
            }
        } else if (StrEq(state, CLOVA_PAYLOAD_FAIL_DOWNLOAD)) {
            if (StrEq(g_update->state, CLOVA_PAYLOAD_DOWNLOADING)) {
                g_update->progress = -1;
                if (g_update->enabled) {
                    g_update->enabled = 0;
                    send_report_ready_state(g_update);
                }
                free(g_update->state);
                g_update->state = strdup(CLOVA_PAYLOAD_LATEST);
                if (g_update->new_version) {
                    free(g_update->new_version);
                }
                g_update->new_version = NULL;
                g_update->error = OTA_DOWNLOAD_FAIL;
            }
        }
        ASR_PRINT(ASR_DEBUG_TRACE, "state: %s\n", g_update->state);
    }
}

void update_OTA_process(const int progress)
{
    if (g_update) {
        g_update->progress = progress;
        send_report_client_state(g_update);

        ASR_PRINT(ASR_DEBUG_TRACE, "downloading progress: %d\n", g_update->progress);
    }
}

static int handle_start_update(UPDATE_T *update)
{
    int ret = -1;

    ret = request_start_update();

    return ret;
}

static int handle_report_ready_state(UPDATE_T *update)
{
    int ret = -1;

    if (update) {
        if (StrEq(update->state, CLOVA_PAYLOAD_DOWNLOADING)) {
            update->ready = 1;
            ret = send_report_ready_state(update);
            update->ready = 0;
        } else if (StrEq(update->state, CLOVA_PAYLOAD_HOLDING)) {
            ret = request_start_update();
        } else {
            ret = request_update_check();
            update->flag = 1;
            update->enabled = 1;
        }
    }

    return ret;
}

static int handle_report_client_state(UPDATE_T *update)
{
    int ret = -1;

    if (update) {
        if (!update->app_flag)
            update->app_flag = 1;
        ret = request_update_check();
    }

    return ret;
}

int clova_system_parse_directive(json_object *js_obj)
{
    int ret = -1;
    char *name = NULL;
    char *name_space = NULL;
    char *dialog_id = NULL;
    json_object *js_header = NULL;
    json_object *js_payload = NULL;

    if (js_obj) {
        js_header = json_object_object_get(js_obj, KEY_HEADER);
        if (js_header) {
            name_space = JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            name = JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            dialog_id = JSON_GET_STRING_FROM_OBJECT(js_header, KEY_DIALOGREQUESTID);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
        }

        js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
        if (js_payload) {
            if (StrEq(name, CLOVA_NAME_START_UPDATE)) {
                ret = handle_start_update(g_update);
            } else if (StrEq(name, CLOVA_NAME_EXPECT_REPORT_UPDATE_READY_STATE)) {
                ret = handle_report_ready_state(g_update);
            } else if (StrEq(name, CLOVA_NAME_EXPECT_REPORT_CLIENT_STATE)) {
                ret = handle_report_client_state(g_update);
            } else if (StrEq(name, CLOVA_NAME_EXCEPTION)) {
                int code = JSON_GET_INT_FROM_OBJECT(js_payload, "code");
                if (code == 412) {
                    http2_conn_exit();
                } else if (code == 500) {
                    volume_pormpt_action(VOLUME_ALEXA_TROUBLE);
                }
            }
        }
    }

    return ret;
}

void check_ota_state(void)
{
    if (g_update && g_update->error == OTA_DOWNLOAD_FAIL) {
        if (get_internet_flags() == 1) {
            notify_internet_back();
        }
    }
}

void report_ota_result(void) { send_report_client_state(g_update); }

int init_update(void)
{
    char *tmp = NULL;

RETRY:
    g_update = (UPDATE_T *)calloc(1, sizeof(UPDATE_T));

    if (g_update) {
        g_update->error = OTA_NONE;
        g_update->progress = -1;
        if (g_update->state)
            free(g_update->state);
        g_update->state = strdup(CLOVA_PAYLOAD_LATEST);
        tmp = get_device_firmware_version();
        if (tmp) {
            if (strstr(tmp, "Linkplay.")) {
                tmp += strlen("Linkplay.");
            }
            if (g_update->version) {
                free(g_update->version);
                g_update->version = NULL;
            }
            if (tmp) {
                g_update->version = strdup(tmp);
            }
        }

        return 0;
    } else
        goto RETRY;
}
