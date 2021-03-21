
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

#include "asr_notification.h"

#include "asr_cmd.h"
#include "asr_debug.h"
#include "asr_json_common.h"
#include "asr_light_effects.h"
#include "asr_player.h"

#include "common_flags.h"
#include "wm_util.h"
#include "lp_list.h"
#include "clova_dnd.h"
#include "clova_clova.h"

#include "asr_bluetooth.h"

enum {
    e_noti_notify,
    e_noti_line_message,
};

#define TYPE_ACTIONTIMER ("ACTIONTIMER")
#define SND_NOTIFY_FILE ("kr/bell_4.mp3")
#define SND_MESSAGE_FILE ("kr/bell_3.mp3")

typedef struct _noti_item_s {
    int type;
    int light_type;
    int need_block;
    int tts_type;
    char *asset_id;
    char *asset_url;

    struct lp_list_head list;
} noti_item_t;

typedef struct _asr_noti_s {
    int is_running;
    int need_resume;
    int is_bgplay;
    int guide_playing;

    pthread_t noti_pid;
    pthread_mutex_t status_lock;

    sem_t list_sem;
    pthread_mutex_t noti_list_lock;
    struct lp_list_head noti_list;
} asr_noti_t;

static asr_noti_t g_asr_noti = {0};

#define BoolToInt(val) ((val) ? 1 : 0)
#define IntToBool(val) ((val) ? 1 : 0)

static void do_guide_case(asr_noti_t *asr_noti, char *id)
{
    if (StrEq(id, "bell_start.mp3") && asr_noti->guide_playing)
        file_play_stop(e_file_notify);
}

static void check_is_guide(char *id, int *flag)
{
    if (StrEq(id, "bell_start.mp3") || StrEq(id, "boot_button_wakeup.mp3") ||
        StrEq(id, "boot_button_micoff.mp3") || StrEq(id, "boot_button_bluetooth.mp3")) {
        *flag = 1;
    } else {
        *flag = 0;
    }
}

static void clear_noti_list(asr_noti_t *asr_noti, int is_one)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    noti_item_t *list_item = NULL;

    if (asr_noti) {
        pthread_mutex_lock(&asr_noti->noti_list_lock);
        if (!lp_list_empty(&asr_noti->noti_list)) {
            lp_list_for_each_safe(pos, npos, &asr_noti->noti_list)
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
                if (is_one) {
                    break;
                }
            }
        }
        pthread_mutex_unlock(&asr_noti->noti_list_lock);
    }
}

static noti_item_t *check_noti_list_exist(asr_noti_t *asr_noti, char *asset_id)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    noti_item_t *list_item = NULL;

    if (asr_noti) {
        pthread_mutex_lock(&asr_noti->noti_list_lock);
        if (!lp_list_empty(&asr_noti->noti_list)) {
            lp_list_for_each_safe(pos, npos, &asr_noti->noti_list)
            {
                list_item = lp_list_entry(pos, noti_item_t, list);
                if (list_item) {
                    if (StrEq(list_item->asset_id, asset_id)) {
                        break;
                    } else {
                        list_item = NULL;
                    }
                } else {
                    list_item = NULL;
                }
            }
        }
        pthread_mutex_unlock(&asr_noti->noti_list_lock);
    }

    return list_item;
}

static int insert_noti_list(asr_noti_t *asr_noti, int type, int light_type, char *asset_id,
                            char *asset_url, int need_block, int tts_type)
{
    int is_exist = 0;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    noti_item_t *list_item = NULL;

    if (asr_noti) {
        do_guide_case(asr_noti, asset_id);
        list_item = check_noti_list_exist(asr_noti, asset_id);
        if (list_item == NULL) {
            list_item = (noti_item_t *)calloc(1, sizeof(noti_item_t));
        } else {
            is_exist = 1;
        }
        if (list_item) {
            list_item->type = type;
            list_item->light_type = light_type;
            list_item->need_block = need_block;
            list_item->tts_type = tts_type;

            if (is_exist == 0) {
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
                pthread_mutex_lock(&asr_noti->noti_list_lock);
                lp_list_add_tail(&list_item->list, &asr_noti->noti_list);
                pthread_mutex_unlock(&asr_noti->noti_list_lock);
                sem_post(&asr_noti->list_sem);
            }
            return 0;
        } else {
            printf("----------insert_noti_list failed-----------\n");
        }
    }

    return -1;
}

static void asr_notificaion_foucus_manage(void)
{
    if (clova_get_extension_state()) {
        NOTIFY_CMD(ASR_EVENT_PIPE, ASR_STOP_FREE_TALK);
    }
}

void *noti_proc(void *args)
{
    asr_noti_t *asr_noti = (asr_noti_t *)args;
    int focus_flags = 0;

    printf(
        "-----------------------------------noti_proc------------------------------------------\n");
    if (!asr_noti) {
        ASR_PRINT(ASR_DEBUG_INFO, "args is NULL\n");
        return NULL;
    }

    asr_noti->is_running = 1;
    while (asr_noti->is_running) {
        struct lp_list_head *pos = NULL;
        struct lp_list_head *npos = NULL;
        noti_item_t *list_item = NULL;

        sem_wait(&asr_noti->list_sem);
        pthread_mutex_lock(&asr_noti->noti_list_lock);
        if (!lp_list_empty(&asr_noti->noti_list)) {
            lp_list_for_each_safe(pos, npos, &asr_noti->noti_list)
            {
                list_item = lp_list_entry(pos, noti_item_t, list);
                if (list_item && list_item->asset_id && list_item->asset_url) {
                    printf("----------- going to start play item number is %d --------------\n",
                           lp_list_number(&asr_noti->noti_list));
                    lp_list_del(&list_item->list);
                    break;
                } else {
                    list_item = NULL;
                }
            }
        }
        pthread_mutex_unlock(&asr_noti->noti_list_lock);

        if (list_item) {
            check_is_guide(list_item->asset_id, &asr_noti->guide_playing);

            asr_notificaion_foucus_manage();

            if (list_item->asset_url && !clova_get_extension_state() && !get_asr_talk_start()) {
                focus_flags = 1;
                common_focus_manage_set(e_focus_notification);
            } else {
                focus_flags = 0;
                common_focus_manage_set_bg(e_focus_notification);
            }

            if (focus_flags == 0) {
                pthread_mutex_lock(&g_asr_noti.status_lock);
                g_asr_noti.is_bgplay = 1;
                g_asr_noti.need_resume = 0;
                pthread_mutex_unlock(&g_asr_noti.status_lock);
            }

            if (focus_flags) {
                if (list_item->light_type == e_light_notify) {
                    asr_light_effects_set(e_effect_notify_on);
                } else if (list_item->light_type == e_light_line_message) {
                    asr_light_effects_set(e_effect_message_on);
                } else if (list_item->light_type == e_light_internet_error) {
                    asr_light_effects_set(e_effect_error);
                } else if (list_item->light_type == e_light_close_ota) {
                    asr_light_effects_set(e_effect_ota_on);
                }
            }

            if (list_item->asset_url && list_item->asset_id) {
                if (StrInclude(list_item->asset_url, "http")) {
                    asr_player_start_play_url(e_stream_type_notify, list_item->asset_url, 0);
                    while (1) {
                        if (focus_flags == 0) {
                            pthread_mutex_lock(&g_asr_noti.status_lock);
                            if (g_asr_noti.need_resume == 1) {
                                focus_flags = 1;
                                g_asr_noti.need_resume = 0;
                            }
                            pthread_mutex_unlock(&g_asr_noti.status_lock);
                            if (focus_flags) {
                                common_focus_manage_set(e_focus_notification);
                                if (list_item->light_type == e_light_notify) {
                                    asr_light_effects_set(e_effect_notify_on);
                                } else if (list_item->light_type == e_light_line_message) {
                                    asr_light_effects_set(e_effect_message_on);
                                } else if (list_item->light_type == e_light_internet_error) {
                                    asr_light_effects_set(e_effect_error);
                                }
                            }
                        }
                        int ret = asr_player_get_play_url_state(e_stream_type_notify);
                        if (ret == e_status_ex_exit || ret == -1) {
                            printf("----------- play finished ----------------------\n");
                            break;
                        } else {
                            usleep(10000);
                        }
                    }
                } else {
                    if (list_item->need_block) {
                        set_block_play_flag();
                    }

                    if (StrEq(list_item->asset_url, "bell_start.mp3")) {
                        set_guide_status(1);
                    }
                    file_play_with_path(list_item->asset_url, 1, 100, e_file_notify);
                    if (StrInclude(list_item->asset_url, "boot_button") && get_guide_status()) {
                        set_guide_status(0);
                    }
                    if (list_item->tts_type >= e_tts_connect &&
                        list_item->tts_type <= e_tts_pairing) {
                        asr_bluetooth_get_tts(list_item->tts_type);
                    }
                    if (list_item->need_block) {
                        clear_block_play_flag();
                    }
                }
            }
            if (list_item->light_type) {
                if (list_item->type == e_noti_notify) {
                    asr_light_effects_set(e_effect_notify_off);
                }
                if (list_item->light_type == e_light_close_ota) {
                    release_button_block(); // if donut boot up after ota, we should release button
                                            // block
                    asr_light_effects_set(e_effect_ota_off);
                }
            }
            if (list_item->asset_id) {
                free(list_item->asset_id);
            }
            if (list_item->asset_url) {
                free(list_item->asset_url);
            }
            free(list_item);
        } else {
            usleep(10000);
        }

        if (focus_flags)
            common_focus_manage_clear(e_focus_notification);

        pthread_mutex_lock(&asr_noti->noti_list_lock);
        if (lp_list_empty(&asr_noti->noti_list)) {
            focus_flags = 0;
            pthread_mutex_lock(&g_asr_noti.status_lock);
            g_asr_noti.is_bgplay = 0;
            pthread_mutex_unlock(&g_asr_noti.status_lock);
        }
        pthread_mutex_unlock(&asr_noti->noti_list_lock);
    }

    return NULL;
}

int asr_notificaion_init(void)
{
    int ret = -1;

    memset(&g_asr_noti, 0x0, sizeof(asr_noti_t));
    pthread_mutex_init(&g_asr_noti.status_lock, NULL);
    pthread_mutex_init(&g_asr_noti.noti_list_lock, NULL);
    LP_INIT_LIST_HEAD(&g_asr_noti.noti_list);
    sem_init(&g_asr_noti.list_sem, 0, 0);

    ret = pthread_create(&g_asr_noti.noti_pid, NULL, noti_proc, (void *)&g_asr_noti);
    if (ret != 0) {
        g_asr_noti.noti_pid = 0;
        printf("notificaion_init %d thread create failed\n", __LINE__);
        return ret;
    }

    return ret;
}

void asr_notificaion_uninit(void)
{
    g_asr_noti.is_running = 0;
    sem_post(&g_asr_noti.list_sem);
    if (g_asr_noti.noti_pid > 0) {
        pthread_join(g_asr_noti.noti_pid, NULL);
        g_asr_noti.noti_pid = 0;
    }
    // Clear list
    clear_noti_list(&g_asr_noti, 0);
    pthread_mutex_destroy(&g_asr_noti.status_lock);
    sem_destroy(&g_asr_noti.list_sem);
}

int asr_parse_assets(int type, int is_new, char *light, json_object *assets,
                     json_object *js_asset_order)
{
    int ret = -1;
    int idx = 0;
    int idx_order = 0;
    int num = 0;
    int num_order = 0;
    int light_type = e_light_none;
    char *asset_url = NULL;
    char *asset_id = NULL;
    json_object *assets_one = NULL;
    json_object *assets_order = NULL;
    char *asset_order_id = NULL;

    if (assets && js_asset_order) {
        num = json_object_array_length(assets);
        num_order = json_object_array_length(js_asset_order);
        for (idx_order = 0; idx_order < num_order; idx_order++) {
            assets_order = json_object_array_get_idx(js_asset_order, idx_order);
            if (assets_order) {
                asset_order_id = json_object_get_string(assets_order);
                for (idx = 0; idx < num; idx++) {
                    assets_one = json_object_array_get_idx(assets, idx);
                    if (assets_one) {
                        asset_id = JSON_GET_STRING_FROM_OBJECT(assets_one, PAYLOAD_AssetId);
                        asset_url = JSON_GET_STRING_FROM_OBJECT(assets_one, PAYLOAD_Asset_url);
                        if (StrEq(asset_id, asset_order_id) && asset_id && asset_url) {
                            if (StrEq(light, "DEFAULT")) {
                                light_type = e_light_notify;
                            }
                            if (!StrInclude(asset_url, "http")) {
                                if (type == e_noti_notify) {
                                    asset_url = SND_NOTIFY_FILE;
                                } else {
                                    light_type = e_light_line_message;
                                    asset_url = SND_MESSAGE_FILE;
                                }
                            }
                            ret = insert_noti_list(&g_asr_noti, type, light_type, asset_id,
                                                   asset_url, 0, 0);
                            break;
                        }
                    }
                }
            }
        }
    }

    return ret;
}

/*
{
    "directive": {
        "header": {
            "namespace": "Notifier",
            "name": "SetIndicator",
            "messageId": "29745c13-0d70-408e-a4cc-946afba67524"
        },
        "payload": {
            "assets": [{
                "assetId": "3ea201e8-135f-42fd-a75c-f125331ff9bd",
                "url": "clova://notifier/sound/default"
            }],
            "assetPlayOrder": ["3ea201e8-135f-42fd-a75c-f125331ff9bd"],
            "new": true,
            "light": "DEFAULT"
        }
    }
}
*/
int asr_parse_notificaion_set(json_object *js_payload)
{
    int ret = 0;

    if (js_payload) {
        json_bool is_new = JSON_GET_BOOL_FROM_OBJECT(js_payload, PAYLOAD_new);
        char *light = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_light);
        json_object *js_assets = json_object_object_get(js_payload, PAYLOAD_Assets);
        json_object *js_asset_order = json_object_object_get(js_payload, PAYLOAD_assetPlayOrder);
        if (js_assets && js_asset_order) {
            ret = asr_parse_assets(e_noti_line_message, is_new, light, js_assets, js_asset_order);
        }
    }

    return ret;
}

int asr_notificaion_clear(void)
{
    // Stop all notifications and clear the list
    pthread_mutex_lock(&g_asr_noti.status_lock);
    g_asr_noti.need_resume = 0;
    pthread_mutex_unlock(&g_asr_noti.status_lock);
    asr_bluetooth_clear_tts();
    clear_noti_list(&g_asr_noti, 1);
    sem_post(&g_asr_noti.list_sem);
    asr_player_stop_play_url(e_stream_type_notify);
    asr_light_effects_set(e_effect_notify_off);
    asr_light_effects_set(e_effect_message_off);

    return 0;
}

/*
{
    "light": "DEFAULT",
    "assets": [
        {
            "assetId": "e5179318-d061-42f9-af1b-417180142934",
            "url": "clova://notifier/sound/default"
        },
        {
            "assetId": "9d3df3c1-d0b4-4375-84a6-67c7ae000292",
            "url":
"https://steaming.example.com/3325-b5c75045b4ae426885343f9b6abd0bfc-1508160634257"
        }
    ],
    "assetPlayOrder": [
        "e5179318-d061-42f9-af1b-417180142934",
        "9d3df3c1-d0b4-4375-84a6-67c7ae000292"
    ]
}
*/
int asr_parse_notificaion_notify(json_object *js_payload)
{
    int ret = 0;

    if (js_payload) {
        char *light = JSON_GET_STRING_FROM_OBJECT(js_payload, PAYLOAD_light);
        json_object *js_assets = json_object_object_get(js_payload, PAYLOAD_Assets);
        json_object *js_asset_order = json_object_object_get(js_payload, PAYLOAD_assetPlayOrder);
        if (js_assets && js_asset_order) {
            ret = asr_parse_assets(e_noti_notify, 0, light, js_assets, js_asset_order);
        }
    }

    return ret;
}

int asr_notificaion_parse_directive(json_object *js_obj)
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
                    if (!clova_get_dnd_on()) {
                        ret = asr_parse_notificaion_set(js_payload);
                    }
                } else if (StrEq(name, NAME_ClearNotifications)) {
                    if (!clova_get_dnd_on()) {
                        ret = asr_notificaion_clear();
                    }
                } else if (StrEq(name, NAME_Notify)) {
                    ret = asr_parse_notificaion_notify(js_payload);
                }
            }
        }
    }

    return ret;
}

int asr_notificaion_add_local_notify(char *js_str)
{
    int ret = -1;

    if (!js_str) {
        ASR_PRINT(ASR_DEBUG_ERR, "input is NULL\n");
        return ret;
    }

    json_object *js_obj = json_tokener_parse(js_str);
    if (js_obj) {
        char *item_id = JSON_GET_STRING_FROM_OBJECT(js_obj, "item_id");
        char *file_path = JSON_GET_STRING_FROM_OBJECT(js_obj, "filepath");
        int light_effect = JSON_GET_INT_FROM_OBJECT(js_obj, "light_effect");
        int need_block = JSON_GET_INT_FROM_OBJECT(js_obj, "need_block");
        int tts_type = JSON_GET_INT_FROM_OBJECT(js_obj, "tts_type");

        ret = insert_noti_list(&g_asr_noti, e_noti_notify, light_effect, item_id, file_path,
                               need_block, tts_type);

        json_object_put(js_obj);
    }

    return ret;
}

void asr_notification_resume(void)
{
    pthread_mutex_lock(&g_asr_noti.status_lock);
    if (g_asr_noti.is_bgplay == 1) {
        g_asr_noti.need_resume = 1;
    }
    pthread_mutex_unlock(&g_asr_noti.status_lock);
}
