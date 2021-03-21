#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include "alexa_notification.h"
#include "alexa_debug.h"
#include "alexa_json_common.h"
#include "wm_util.h"
#include "lp_list.h"
#include "alexa_request_json.h"

extern WIIMU_CONTEXT *g_wiimu_shm;

extern void block_voice_prompt(char *path, int block);

typedef struct _alexa_noti_s {
    int is_running;
    int last_visual;
    int is_enable;
    char *cur_asset_id;

    pthread_t noti_pid;
    pthread_mutex_t status_lock;
} alexa_noti_t;

typedef struct _noti_item_s {
    int visual_indicator;
    int audio_indicator;
    char *asset_id;
    char *asset_url;

    struct lp_list_head list;
} noti_item_t;

static alexa_noti_t g_alexa_noti;

LP_LIST_HEAD(g_noti_list);
static pthread_mutex_t g_mutex_noti_list = PTHREAD_MUTEX_INITIALIZER;
#define noti_list_lock() pthread_mutex_lock(&g_mutex_noti_list)
#define noti_list_unlock() pthread_mutex_unlock(&g_mutex_noti_list)
#define BoolToInt(val) ((val) ? 1 : 0)
#define IntToBool(val) ((val) ? 1 : 0)

static void clear_noti_list(int clear_one)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    noti_item_t *list_item = NULL;

    noti_list_lock();
    if (!lp_list_empty(&g_noti_list)) {
        lp_list_for_each_safe(pos, npos, &g_noti_list)
        {
            list_item = lp_list_entry(pos, noti_item_t, list);
            if (list_item) {
                lp_list_del(&list_item->list);
                if (list_item->asset_id) {
                    free(list_item->asset_id);
                }
                if (list_item->asset_url) {
                    free(list_item->asset_url);
                }
                free(list_item);
            }
            if (clear_one) {
                break;
            }
        }
    }
    noti_list_unlock();
}

static int insert_noti_list(int visual_indicator, int audio_indicator, char *asset_id,
                            char *asset_url)
{
    noti_item_t *list_item = NULL;

    list_item = (noti_item_t *)calloc(1, sizeof(noti_item_t));
    if (list_item) {
        list_item->visual_indicator = visual_indicator;
        list_item->audio_indicator = audio_indicator;
        if (asset_id) {
            list_item->asset_id = strdup(asset_id);
        } else {
            list_item->asset_id = NULL;
        }
        if (asset_url) {
            list_item->asset_url = strdup(asset_url);
        } else {
            list_item->asset_url = NULL;
        }

        noti_list_lock();
        lp_list_add_tail(&list_item->list, &g_noti_list);
        noti_list_unlock();

        return 0;
    } else {
        printf("----------insert_noti_list failed-----------n");
    }

    return -1;
}

void *noti_proc(void *args)
{
    alexa_noti_t *alexa_noti = (alexa_noti_t *)args;

    if (!alexa_noti) {
        printf("alexa_noti is NULL\n");
        return NULL;
    }
    printf(
        "-----------------------------------noti_proc------------------------------------------\n");

    alexa_noti->is_running = 1;
    while (alexa_noti && alexa_noti->is_running) {
        int visual_indicator = 0;
        int need_play_prompt = 0;
        struct lp_list_head *pos = NULL;
        struct lp_list_head *npos = NULL;
        noti_item_t *list_item = NULL;

        usleep(50000);
        noti_list_lock();
        if (!lp_list_empty(&g_noti_list)) {
            lp_list_for_each_safe(pos, npos, &g_noti_list)
            {
                list_item = lp_list_entry(pos, noti_item_t, list);
                if (list_item) {
                    lp_list_del(&list_item->list);
                    printf("-----------going to start play----------------------\n");
                    visual_indicator = list_item->visual_indicator;
                    if (list_item->asset_url && list_item->asset_id) {
                        need_play_prompt = 1;
                    }
                    break;
                }
            }
        }
        noti_list_unlock();

        if (list_item) {
            if (visual_indicator && alexa_noti) {
                pthread_mutex_lock(&alexa_noti->status_lock);
                if (alexa_noti->is_enable == 1) {
                    SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDENNOTI",
                                             strlen("GNOTIFY=MCULEDENNOTI"), NULL, NULL, 0);
                }
                pthread_mutex_unlock(&alexa_noti->status_lock);
            }
            if (need_play_prompt) {
#if 0
                char *buffer = NULL;
                asprintf(&buffer, "imuzop '%s'", list_item->asset_url);
                if(buffer) {
                    system(buffer);
                    free(buffer);
                } else
#endif
                {
                    // volume_pormpt_direct("common/alexa_notification.mp3", 1, 0);
                    block_voice_prompt("common/alexa_notification.mp3", 1);
                }
            }
            if (list_item->asset_id) {
                free(list_item->asset_id);
            }
            if (list_item->asset_url) {
                free(list_item->asset_url);
            }
            free(list_item);
        }
        usleep(100000);
    }

    return NULL;
}

int avs_notificaion_init(void)
{
    int ret = -1;

    memset(&g_alexa_noti, 0x0, sizeof(alexa_noti_t));
    pthread_mutex_init(&g_alexa_noti.status_lock, NULL);

    ret = pthread_create(&g_alexa_noti.noti_pid, NULL, noti_proc, (void *)&g_alexa_noti);
    if (ret != 0) {
        g_alexa_noti.noti_pid = 0;
        printf("notificaion_init %d thread create failed\n", __LINE__);
        return ret;
    }

    return ret;
}

void avs_notificaion_uninit(void)
{
    g_alexa_noti.is_running = 0;

    if (g_alexa_noti.noti_pid > 0) {
        pthread_join(g_alexa_noti.noti_pid, NULL);
        g_alexa_noti.noti_pid = 0;
    }

    pthread_mutex_destroy(&g_alexa_noti.status_lock);

    // Clear list
    clear_noti_list(0);
}

/*
   *
    This directive instructs your client to display a persistent visual indicator
    and/or play an audio file when a new notification is available. Your client
    may receive multiple SetIndicator directives in a short period of time. If
    directives overlap, consider these rules:

        1. If the assetId of the current directive matches the assetId of the incoming
        directive, DO NOT play the asset.

        2. If the assetId of the current directive does not match the assetId of the
        incoming directive, play the asset of the incoming directive AFTER playback
        of the current asset is finished.

    {
        "directive": {
            "header": {
                "namespace": "Notifications",
                "name": "SetIndicator",
                "messageId": "606dee5a-0cae-4238-ad0c-ba9c262e2285"
            },
            "payload": {
                "persistVisualIndicator": true,
                "playAudioIndicator": true,
                "asset": {
                    "assetId": "default_notification_sound",
                    "url":
   "https://s3-us-west-2.amazonaws.com/notificationindicator/default_notification_sound.mp3"
                }
            }
        }
    }

    persistVisualIndicator:
        Specifies your product must display a persistent visual indicator (if
        applicable) when a notification is delivered. For example: If
        persistVisualIndicator is true, your product must display a persistent
        indicator after the notification is delivered. However, if
        persistVisualIndicator is false, no visual indicators are required after the
        initial notification is delivered.

    playAudioIndicator:
        Specifies your product must play an audio indicator when a notification is
        delivered.

    asset:
        Contains information about the audio asset that must be played if
        playAudioIndicator is true.

    assetId:
        A unique identifier for the asset.

    url:
        The asset url. Your product is expected to download and cache the asset. If
        the product is offline, or if the asset isn't available, your product should
        play the default notification

*/
int avs_parse_notificaion_set(json_object *js_payload)
{
    int ret = 0;
    char *asset_url = NULL;
    char *asset_id = NULL;
    int is_need_play = 0;

    if (js_payload) {
        json_bool visual_indicator = JSON_GET_BOOL_FROM_OBJECT(js_payload, PAYLOAD_VisualIndicator);
        json_bool audio_indicator = JSON_GET_BOOL_FROM_OBJECT(js_payload, PAYLOAD_AudioIndicator);
        if (audio_indicator) {
            // TODO: audio indicator
            json_object *js_asset = json_object_object_get(js_payload, PAYLOAD_Asset);
            if (js_asset) {
                asset_id = JSON_GET_STRING_FROM_OBJECT(js_asset, PAYLOAD_AssetId);
                if (asset_id) {
                    pthread_mutex_lock(&g_alexa_noti.status_lock);
                    if (!g_alexa_noti.cur_asset_id) {
                        g_alexa_noti.cur_asset_id = strdup(asset_id);
                        is_need_play = 1;
                    } else if (StrEq(asset_id, g_alexa_noti.cur_asset_id)) {
                        is_need_play = 0;
                    }
                    pthread_mutex_unlock(&g_alexa_noti.status_lock);
                }
                if (1 || is_need_play) {
                    asset_url = JSON_GET_STRING_FROM_OBJECT(js_asset, PAYLOAD_Asset_url);
                }
            }
        }

        pthread_mutex_lock(&g_alexa_noti.status_lock);
        g_alexa_noti.last_visual = BoolToInt(visual_indicator);
        g_alexa_noti.is_enable = 1;
        pthread_mutex_unlock(&g_alexa_noti.status_lock);

        ret = insert_noti_list(BoolToInt(visual_indicator), BoolToInt(audio_indicator), asset_id,
                               asset_url);
    }

    return ret;
}

int avs_notificaion_clear(void)
{
    // Stop all notifications and clear the list
    clear_noti_list(0);
    pthread_mutex_lock(&g_alexa_noti.status_lock);
    if (g_alexa_noti.cur_asset_id) {
        free(g_alexa_noti.cur_asset_id);
        g_alexa_noti.cur_asset_id = NULL;
    }
    g_alexa_noti.last_visual = 0;
    g_alexa_noti.is_enable = 0;
    SocketClientReadWriteMsg("/tmp/RequestIOGuard", "GNOTIFY=MCULEDDISNOTI",
                             strlen("GNOTIFY=MCULEDDISNOTI"), NULL, NULL, 0);
    pthread_mutex_unlock(&g_alexa_noti.status_lock);

    return 0;
}

/*
    This directive instructs your client to clear all active visual and audio
    indicators.

        1. If an audio indicator is playing when this directive is received, it should
            be stopped immediately.

        2. If any visual indicators are set when this directive is received, they should
            be cleared immediately.
    {
        "directive": {
            "header": {
                "namespace": "Notifications",
                "name": "ClearIndicator",
                "messageId": "{{STRING}}"
            },
            "payload": {
            }
        }
    }
*/
int avs_parse_notificaion_clear(json_object *js_payload)
{
    avs_notificaion_clear();

    return 0;
}

int avs_notificaion_parse_directive(json_object *js_obj)
{
    int ret = -1;

    if (js_obj) {
        json_object *js_header = json_object_object_get(js_obj, KEY_HEADER);
        if (js_header) {
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAMESPACE);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_MESSAGEID);
            JSON_GET_STRING_FROM_OBJECT(js_header, KEY_DIALOGREQUESTID);

            char *name = JSON_GET_STRING_FROM_OBJECT(js_header, KEY_NAME);
            json_object *js_payload = json_object_object_get(js_obj, KEY_PAYLOAD);
            if (js_payload) {
                if (StrEq(name, NAME_SetNotifications)) {
                    ret = avs_parse_notificaion_set(js_payload);
                } else if (StrEq(name, NAME_ClearNotifications)) {
                    ret = avs_parse_notificaion_clear(js_payload);
                }
            }
        }
    }

    return ret;
}

/*
{
    "header": {
        "namespace": "Notifications",
        "name": "IndicatorState"
    },
    "payload": {
        "isEnabled": {{BOOLEAN}},
        "isVisualIndicatorPersisted": {{BOOLEAN}}
    }
}
*/
int avs_notificaion_state(json_object *json_context_list)
{
    int ret = -1;
    json_object *notificaion_payload = NULL;
    json_bool last_visual = 0;
    json_bool is_enable = 0;

    pthread_mutex_lock(&g_alexa_noti.status_lock);
    last_visual = IntToBool(g_alexa_noti.last_visual);
    is_enable = IntToBool(g_alexa_noti.is_enable);
    pthread_mutex_unlock(&g_alexa_noti.status_lock);

    if (json_context_list) {
        notificaion_payload = json_object_new_object();
        if (notificaion_payload) {
            json_object_object_add(notificaion_payload, PAYLOAD_IsVisualIndicatorPersisted,
                                   json_object_new_boolean(last_visual));
            json_object_object_add(notificaion_payload, PAYLOAD_IsEnable,
                                   json_object_new_boolean(is_enable));
        }
        ret = alexa_context_list_add(json_context_list, NAMESPACE_NOTIFICATIONS,
                                     NAME_IndicatorState, notificaion_payload);
    }

    if (ret != 0) {
        if (notificaion_payload) {
            json_object_put(notificaion_payload);
        }
    }

    return -1;
}

int avs_notificaion_test_event(FILE *test_file, json_object *json_cmd)
{
    int ret = -1;
    const char *data_write = NULL;
    json_object *test_json = NULL;

    if (json_cmd) {
        ret = 0;
    } else {
        test_json = json_object_new_array();
        avs_notificaion_state(test_json);
        if (test_json) {
            data_write = json_object_to_json_string(test_json);
            if (data_write) {
                fwrite(data_write, 1, strlen(data_write), test_file);
                fwrite("\n", 1, 1, test_file);
                ret = 0;
            }
            json_object_put(test_json);
        }
    }

    return ret;
}
