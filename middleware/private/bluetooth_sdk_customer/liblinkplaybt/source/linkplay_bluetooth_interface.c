#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <semaphore.h>
#include <errno.h>
#include <linux/soundcard.h>
#include <string.h>
#include <inttypes.h>
#include <sys/un.h>
#include "app_ble.h"
#include "app_utils.h"
#include "app_disc.h"
#include "app_mgt.h"
#include "app_dm.h"
#include "app_ble_server.h"
#include "app_xml_utils.h"
#include "app_hs.h"
#include "lp_list.h"
#include "linkplay_bluetooth_interface.h"
#include "json.h"

static LP_LIST_HEAD(list_bt_event);
static LP_LIST_HEAD(list_bt_device);
static LP_LIST_HEAD(list_bt_device_no_eir);

static pthread_mutex_t event_list_mutex = PTHREAD_MUTEX_INITIALIZER;

enum {
    BT_START_FAILED = -1,
    BT_STOP,
    BT_IDLE,
    BT_START_PRE,
    BT_START_PROCESS,
    BT_SERVER_OK,
};

typedef struct _bt_event_node {
    struct lp_list_head entry;
    bt_event_msg event_msg;
} bt_event_node;
typedef struct _BT_DEVICE_CLIENT {
    struct lp_list_head entry;
    BD_ADDR bd_addr; /* BD address peer device. */
    BD_NAME name;    /* Name of peer device. */
    int audio_path;
    int services_inquiry_count;
    int services_pending;
    char profile[32];
} BT_DEVICE_CLIENT;

typedef struct {
    unsigned int bt_todo_services;
    unsigned int bt_cur_service;
    unsigned int bt_support_service;
    unsigned int bt_status;
    unsigned int bt_start_ts;
    int bt_scan_status;
    BD_ADDR connected_bd_addr;
    BD_NAME connected_bd_name;
    linkplay_bt_config *bt_config;
    void *call_back_context;
    linkplay_bluetooth_event_callback event_call_back;
} linkplay_bt_context;

static linkplay_bt_context g_bt_context;
static sem_t g_sem_bt_event;

unsigned int g_debug_level = 0;

/* local functions */
#ifdef HFP_SUPPORT
static int bt_hfp_init(void);
static int bt_hfp_deinit(void);
#endif

static int string_to_bt_address(char *addr, BD_ADDR *bt_addr)
{
    int tmp[BD_ADDR_LEN];

    sscanf(addr, "%02x:%02x:%02x:%02x:%02x:%02x", &tmp[0], &tmp[1], &tmp[2], &tmp[3], &tmp[4],
           &tmp[5]);

    (*bt_addr)[0] = tmp[0];
    (*bt_addr)[1] = tmp[1];
    (*bt_addr)[2] = tmp[2];
    (*bt_addr)[3] = tmp[3];
    (*bt_addr)[4] = tmp[4];
    (*bt_addr)[5] = tmp[5];

    return 0;
}

static int bt_address_to_string(char *addr, BD_ADDR *bt_addr)
{
    sprintf(addr, "%02x:%02x:%02x:%02x:%02x:%02x", (*bt_addr)[0], (*bt_addr)[1], (*bt_addr)[2],
            (*bt_addr)[3], (*bt_addr)[4], (*bt_addr)[5]);

    return 0;
}

static long long tickSincePowerOn(void)
{
    long long sec;
    long long msec;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    sec = ts.tv_sec;
    msec = (ts.tv_nsec + 500000) / 1000000;
    return sec * 1000 + msec;
}

static void timeraddMS(struct timeval *a, unsigned int ms)
{
    a->tv_usec += ms * 1000;
    if (a->tv_usec >= 1000000) {
        a->tv_sec += a->tv_usec / 1000000;
        a->tv_usec %= 1000000;
    }
}

char *bt_get_profile_by_addr__(BD_ADDR bd_addr, struct lp_list_head *headlist)
{
    struct lp_list_head *pos;
    BT_DEVICE_CLIENT *item;

    if (lp_list_number(headlist) <= 0) {
        return NULL;
    }

    lp_list_for_each(pos, headlist)
    {
        item = lp_list_entry(pos, BT_DEVICE_CLIENT, entry);
        if (memcmp(item->bd_addr, bd_addr, BD_ADDR_LEN) == 0) {
            return (char *)item->profile;
        }
    }

    return NULL;
}
BD_ADDR *bt_get_services_pending_addr__(struct lp_list_head *headlist)
{
    struct lp_list_head *pos;
    BT_DEVICE_CLIENT *item;

    if (lp_list_number(headlist) <= 0) {
        return NULL;
    }
    printf("bt_get_services_pending_addr %d \r\n", lp_list_number(headlist));
    lp_list_for_each(pos, headlist)
    {
        item = lp_list_entry(pos, BT_DEVICE_CLIENT, entry);
        if (item->services_pending) {
            if (item->services_inquiry_count++ < 8)
                return &item->bd_addr;
        }
    }

    return NULL;
}

static void bt_device_list_clear__(struct lp_list_head *headlist)
{
    struct lp_list_head *pos = NULL, *tmp = NULL;

    lp_list_for_each_safe(pos, tmp, headlist)
    {
        BT_DEVICE_CLIENT *item = lp_list_entry(pos, BT_DEVICE_CLIENT, entry);
        lp_list_del(pos);
        free(item);
    }

    return;
}
static void bt_devicelist_add_entry__(char *bd_addr, char *name, char *profile,
                                      int services_pending, struct lp_list_head *headlist)
{
    struct lp_list_head *pos;
    int found = 0;
    BT_DEVICE_CLIENT *item;
    lp_list_for_each(pos, headlist)
    {
        item = lp_list_entry(pos, BT_DEVICE_CLIENT, entry);
        if (memcmp(bd_addr, item->bd_addr, BD_ADDR_LEN) == 0) {
            memcpy(item->name, name, BD_NAME_LEN);
            item->services_inquiry_count = 0;
            if (profile)
                strncpy(item->profile, profile, 32);
            item->services_pending = services_pending;
            found = 1;
            break;
        }
    }
    if (!found) {
        BT_DEVICE_CLIENT *item = (BT_DEVICE_CLIENT *)malloc(sizeof(BT_DEVICE_CLIENT));
        memset(item, 0x0, sizeof(BT_DEVICE_CLIENT));
        memcpy(item->bd_addr, bd_addr, BD_ADDR_LEN);
        memcpy(item->name, name, BD_NAME_LEN);
        item->services_pending = services_pending;
        if (profile)
            strncpy(item->profile, profile, 32);
        lp_list_add_tail(&item->entry, headlist);
    }
}
static char *bt_device_convert_to_scan_result__(struct lp_list_head *headlist)
{
    char tmp[32];
    char *utf8_name = NULL;
    int utf8_name_length = 0;
    json_object *json_root = NULL, *json_array = NULL;
    ;
    struct lp_list_head *pos;
    BT_DEVICE_CLIENT *item = NULL;
    char *strDeviceLists = NULL;
    int connect = 0;
    json_root = json_object_new_object();

    json_object_object_add(json_root, "num", json_object_new_int(lp_list_number(headlist)));
    if (lp_list_number(headlist) > 0) {
        json_array = json_object_new_array();
        lp_list_for_each(pos, headlist)
        {
            connect = 0;
            json_object *json = json_object_new_object();
            item = lp_list_entry(pos, BT_DEVICE_CLIENT, entry);
            json_object_object_add(json, "name", json_object_new_string(item->name));
            bt_address_to_string(tmp, &item->bd_addr);
            json_object_object_add(json, "ad", json_object_new_string(tmp));
            json_object_object_add(json, "role", json_object_new_string(item->profile));
            json_object_array_add(json_array, json);
        }
        json_object_object_add(json_root, "list", json_array);
    }

    strDeviceLists = strdup(json_object_to_json_string(json_root));
    //  printf("strDeviceLists %s \r\n", strDeviceLists);

    json_object_put(json_root);

    return strDeviceLists;
}

static bt_event_node *create_bt_event_node(int msg_type, int notify_event, int notify_value,
                                           char *msg_value)
{
    bt_event_node *event_node = (bt_event_node *)malloc(sizeof(bt_event_node));
    event_node->event_msg.msg_type = msg_type;
    event_node->event_msg.notify_event = notify_event;
    event_node->event_msg.notify_value = notify_value;
    event_node->event_msg.msg_value = msg_value;

    return event_node;
}

static void bt_event_add_in_list(bt_event_node *bt_event)
{
    pthread_mutex_lock(&event_list_mutex);
    lp_list_add_tail(&bt_event->entry, &list_bt_event);
    pthread_mutex_unlock(&event_list_mutex);
    linkplay_bluetooth_schedule_event();
}
static void bt_a2dp_sink_sec_link_callback(tBSA_SEC_EVT event, tBSA_SEC_MSG *p_data)
{
    char *buf = NULL;
    char addr[32];
    bt_event_node *bt_event = NULL;

    switch (event) {
    case BSA_SEC_LINK_UP_EVT:
        bt_address_to_string(addr, &p_data->link_up.bd_addr);
        asprintf(&buf, "{\"bt_addr\":\"%s\"}", addr);
        bt_event = create_bt_event_node(BLUETOOTH_SEC_LINK_EVENT, BT_SEC_LINK_UP_EVT, 0, buf);
        break;
    case BSA_SEC_LINK_DOWN_EVT:
        bt_address_to_string(addr, &p_data->link_down.bd_addr);
        asprintf(&buf, "{\"bt_addr\":\"%s\"}", addr);
        bt_event = create_bt_event_node(BLUETOOTH_SEC_LINK_EVENT, BT_SEC_LINK_DOWN_EVT,
                                        p_data->link_down.status, buf);
        break;
    case BSA_SEC_AUTH_CMPL_EVT:
        bt_address_to_string(addr, &p_data->auth_cmpl.bd_addr);
        asprintf(&buf, "{\"bt_addr\":\"%s\",\"bt_name\":\"%s\",\"success\":%d}", addr,
                 p_data->auth_cmpl.bd_name, p_data->auth_cmpl.success);
        bt_event = create_bt_event_node(BLUETOOTH_SEC_LINK_EVENT, BT_SEC_AUTH_CMPL_EVT, 0, buf);
        break;
    case BSA_SEC_AUTHORIZE_EVT: /* Authorization request */
        asprintf(&buf, "{\"bt_addr\":\"%s\",\"bt_name\":\"%s\"}", addr, p_data->auth_cmpl.bd_name);
        bt_event = create_bt_event_node(BLUETOOTH_SEC_LINK_EVENT, BT_SEC_AUTHORIZE_EVT, 0, buf);
        break;
    default:
        break;
    }

    if (bt_event)
        bt_event_add_in_list(bt_event);
    //   if(buf)
    //       free(buf);
}

static void bt_a2dp_source_event_cback(tBSA_AV_EVT event, tBSA_AV_MSG *p_data, void *args)
{
    char *buf = NULL;
    char addr[32];
    bt_event_node *bt_event = NULL;

    switch (event) {
    case BSA_AV_OPEN_EVT:
        bt_address_to_string(addr, &p_data->open.bd_addr);
        asprintf(&buf, "{\"bt_addr\":\"%s\"}", addr);
        bt_event = create_bt_event_node(BLUETOOTH_SOURCE_EVENT, BT_SOURCE_CONNECTED_EVT,
                                        p_data->open.status, buf);
        break;
    case BSA_AV_CLOSE_EVT:
        bt_event =
            create_bt_event_node(BLUETOOTH_SOURCE_EVENT, BT_SOURCE_DISCONNECTED_EVT, 0, NULL);
        break;
    case BSA_AV_START_EVT:
        bt_event = create_bt_event_node(BLUETOOTH_SOURCE_EVENT, BT_SOURCE_AV_START_EVT,
                                        p_data->start.status, NULL);
        break;
    case BSA_AV_STOP_EVT:
        bt_event = create_bt_event_node(BLUETOOTH_SOURCE_EVENT, BT_SOURCE_AV_STOP_EVT, 0, NULL);
        break;
    case BSA_AV_CFG_RSP_EVT: {
        unsigned char samp_freq = p_data->cfg_rsp.codec[3] & A2D_SBC_IE_SAMP_FREQ_MSK;
        int samp_rate = 48000;
        if (A2D_SBC_IE_SAMP_FREQ_48 == samp_freq) {
            samp_rate = 48000;
        } else if (A2D_SBC_IE_SAMP_FREQ_44 == samp_freq) {
            samp_rate = 44100;
        }
        bt_event =
            create_bt_event_node(BLUETOOTH_SOURCE_EVENT, BT_SOURCE_CFG_RSP_EVT, samp_rate, NULL);
    } break;
    case BSA_AV_REMOTE_CMD_EVT:
        bt_event = create_bt_event_node(BLUETOOTH_SOURCE_EVENT, BT_SOURCE_REMOTE_CMD_EVT,
                                        p_data->remote_cmd.rc_id, NULL);
        break;
    default:
        break;
    }

    if (bt_event)
        bt_event_add_in_list(bt_event);
}

static void bt_a2dp_sink_event_callback(tBSA_AVK_EVT event, tBSA_AVK_MSG *p_data)
{
    char *buf = NULL;
    char addr[32];
    bt_event_node *bt_event = NULL;

    switch (event) {
    case BSA_AVK_OPEN_EVT:
        bt_address_to_string(addr, &p_data->open.bd_addr);
        asprintf(&buf, "{\"bt_addr\":\"%s\"}", addr);
        bt_event = create_bt_event_node(BLUETOOTH_SINK_EVENT, BT_SINK_CONNECTED_EVT,
                                        p_data->open.status, buf);
        break;
    case BSA_AVK_CLOSE_EVT:
        bt_address_to_string(addr, &p_data->close.bd_addr);
        asprintf(&buf, "{\"bt_addr\":\"%s\"}", addr);
        bt_event = create_bt_event_node(BLUETOOTH_SINK_EVENT, BT_SINK_DISCONNECTED_EVT, 0, buf);
        break;
    case BSA_AVK_SET_ABS_VOL_CMD_EVT:
        bt_event = create_bt_event_node(BLUETOOTH_SINK_EVENT, BT_SINK_SET_ABS_VOL_CMD_EVT,
                                        p_data->abs_volume.abs_volume_cmd.volume, NULL);
        break;
    case BSA_AVK_START_EVT:
        bt_event = create_bt_event_node(BLUETOOTH_SINK_EVENT, BT_SINK_STREAM_START_EVT, 0, NULL);
        break;
    case BSA_AVK_STOP_EVT:
        bt_event = create_bt_event_node(BLUETOOTH_SINK_EVENT, BT_SINK_STREAM_STOP_EVT, 0, NULL);
        break;
    case BSA_AVK_RC_OPEN_EVT:
        bt_event = create_bt_event_node(BLUETOOTH_SINK_EVENT, BT_SINK_AVRCP_OPEN_EVT, 0, NULL);
        break;
    case BSA_AVK_RC_CLOSE_EVT:
        bt_event = create_bt_event_node(BLUETOOTH_SINK_EVENT, BT_SINK_AVRCP_CLOSE_EVT, 0, NULL);
        break;
    case BSA_AVK_REGISTER_NOTIFICATION_EVT:
        if (p_data->reg_notif.rsp.event_id == AVRC_EVT_TRACK_CHANGE) {
            bt_event =
                create_bt_event_node(BLUETOOTH_SINK_EVENT, BT_SINK_REMOTE_RC_TRACK_CHANGE, 0, NULL);
        }
        break;
    case BSA_AVK_GET_PLAY_STATUS_EVT:
        if (p_data->get_play_status.rsp.play_status == AVRC_PLAYSTATE_PLAYING) {
            bt_event =
                create_bt_event_node(BLUETOOTH_SINK_EVENT, BT_SINK_REMOTE_RC_PLAY_EVT, 0, NULL);
        } else if (p_data->get_play_status.rsp.play_status == AVRC_PLAYSTATE_PAUSED) {
            bt_event =
                create_bt_event_node(BLUETOOTH_SINK_EVENT, BT_SINK_REMOTE_RC_PAUSE_EVT, 0, NULL);
        } else if (p_data->get_play_status.rsp.play_status == AVRC_PLAYSTATE_STOPPED) {
            bt_event =
                create_bt_event_node(BLUETOOTH_SINK_EVENT, BT_SINK_REMOTE_RC_STOP_EVT, 0, NULL);
        }
        break;
    case BSA_AVK_GET_ELEM_ATTR_EVT: {
        int i = 0;
        UINT32 attr_id;
        tBSA_AVK_GET_ELEMENT_ATTR_MSG *p_elem_attr = &p_data->elem_attr;
        json_object *json_root = NULL;
        json_root = json_object_new_object();

        for (i = 0; i < p_elem_attr->num_attr; i++) {
            attr_id = p_elem_attr->attr_entry[i].attr_id;
            if (attr_id == AVRC_MEDIA_ATTR_ID_TITLE) {
                json_object_object_add(
                    json_root, "title",
                    json_object_new_string(p_elem_attr->attr_entry[i].name.data));
            } else if (attr_id == AVRC_MEDIA_ATTR_ID_ARTIST) {
                json_object_object_add(
                    json_root, "artist",
                    json_object_new_string(p_elem_attr->attr_entry[i].name.data));
            } else if (attr_id == AVRC_MEDIA_ATTR_ID_ALBUM) {
                json_object_object_add(
                    json_root, "album",
                    json_object_new_string(p_elem_attr->attr_entry[i].name.data));
            } else if (attr_id == AVRC_MEDIA_ATTR_ID_PLAYING_TIME) {
                json_object_object_add(
                    json_root, "playtime",
                    json_object_new_string(p_elem_attr->attr_entry[i].name.data));
            }
        }
        buf = strdup(json_object_to_json_string(json_root));
        json_object_put(json_root);

        bt_event = create_bt_event_node(BLUETOOTH_SINK_EVENT, BT_SINK_GET_ELEM_ATTR_EVT, 0, buf);
    } break;
    }

    if (bt_event)
        bt_event_add_in_list(bt_event);
    //   if(buf)
    //       free(buf);
}
static void bt_event_generic_disc_cback_services(tBSA_DISC_EVT event, tBSA_DISC_MSG *p_data)
{
    bt_event_node *bt_event = NULL;

    if (event == BSA_DISC_NEW_EVT) {
        pthread_mutex_lock(&event_list_mutex);
        char profile[32] = {0};
        tAPP_DISCOVERY_CB *app_discovery_cb = linkplay_bluetooth_get_discovery_cb();
        app_service_mask_to_string(app_discovery_cb->devs[0].device.services, profile);
        printf("profile 0x%x %s %s \r\n", app_discovery_cb->devs[0].device.services,
               app_discovery_cb->devs[0].device.name, profile);
        bt_devicelist_add_entry__(app_discovery_cb->devs[0].device.bd_addr,
                                  app_discovery_cb->devs[0].device.name, profile, 0,
                                  &list_bt_device_no_eir);
        bt_devicelist_add_entry__(app_discovery_cb->devs[0].device.bd_addr,
                                  app_discovery_cb->devs[0].device.name, profile, 0,
                                  &list_bt_device);
        char *result = bt_device_convert_to_scan_result__(&list_bt_device);
        bt_event = create_bt_event_node(BLUETOOTH_SERVICES_EVENT, BT_DISC_NEW_EVT, 0, result);
        pthread_mutex_unlock(&event_list_mutex);
    } else if (event == BSA_DISC_CMPL_EVT) {
        pthread_mutex_lock(&event_list_mutex);
        BD_ADDR *ad_addr_services_pending = bt_get_services_pending_addr__(&list_bt_device_no_eir);
        pthread_mutex_unlock(&event_list_mutex);
        if (ad_addr_services_pending) {
            linkplay_bluetooth_disc_services_by_addr(
                *ad_addr_services_pending, BSA_A2DP_SRC_SERVICE_MASK | BSA_A2DP_SERVICE_MASK,
                bt_event_generic_disc_cback_services);
        } else {
            bt_event = create_bt_event_node(BLUETOOTH_SERVICES_EVENT, BT_DISC_CMPL_EVT, 0, NULL);
        }
    }

    if (bt_event)
        bt_event_add_in_list(bt_event);
}
static void bt_event_generic_disc_cback(tBSA_DISC_EVT event, tBSA_DISC_MSG *p_data)
{
    bt_event_node *bt_event = NULL;

    if (event == BSA_DISC_NEW_EVT) {
        pthread_mutex_lock(&event_list_mutex);
        int index;
        tAPP_DISCOVERY_CB *app_discovery_cb = linkplay_bluetooth_get_discovery_cb();
        for (index = 0; index < APP_DISC_NB_DEVICES; index++) {
            if (app_discovery_cb->devs[index].in_use != FALSE) {
                char profile[32] = {0};
                if (app_discovery_cb->devs[index].device.eir_data[0]) {
                    app_disc_parse_eir(app_discovery_cb->devs[index].device.eir_data, profile);
                }

                if (strlen(profile) == 0) {
                    app_service_mask_to_string(app_discovery_cb->devs[index].device.services,
                                               profile);
                }
                if (strlen(profile) == 0) {
                    char *save_profile = NULL;
                    if ((save_profile =
                             bt_get_profile_by_addr__(app_discovery_cb->devs[index].device.bd_addr,
                                                      &list_bt_device_no_eir)) != NULL) {
                        strncpy(profile, save_profile, sizeof(profile) - 1);
                    }

                    if (strlen(profile) == 0) {
                        bt_devicelist_add_entry__(app_discovery_cb->devs[index].device.bd_addr,
                                                  app_discovery_cb->devs[index].device.name,
                                                  profile, 1, &list_bt_device_no_eir);
                    }
                } else {
                    if (strlen(app_discovery_cb->devs[index].device.name) > 0)
                        bt_devicelist_add_entry__(app_discovery_cb->devs[index].device.bd_addr,
                                                  app_discovery_cb->devs[index].device.name,
                                                  profile, 0, &list_bt_device);
                }
            }
        }
        char *result = bt_device_convert_to_scan_result__(&list_bt_device);
        bt_event = create_bt_event_node(BLUETOOTH_SERVICES_EVENT, BT_DISC_NEW_EVT, 0, result);
        pthread_mutex_unlock(&event_list_mutex);
    } else if (event == BSA_DISC_CMPL_EVT) {
        pthread_mutex_lock(&event_list_mutex);
        BD_ADDR *ad_addr_services_pending = bt_get_services_pending_addr__(&list_bt_device_no_eir);
        pthread_mutex_unlock(&event_list_mutex);
        if (ad_addr_services_pending) {
            linkplay_bluetooth_disc_services_by_addr(
                *ad_addr_services_pending, BSA_A2DP_SRC_SERVICE_MASK | BSA_A2DP_SERVICE_MASK,
                bt_event_generic_disc_cback_services);
        } else {
            bt_event = create_bt_event_node(BLUETOOTH_SERVICES_EVENT, BT_DISC_CMPL_EVT, 0, NULL);
        }
    }

    if (bt_event)
        bt_event_add_in_list(bt_event);

    return;
}

static int bt_a2dp_source_start(void)
{
    APP_DEBUG0("");

    linkplay_bluetooth_av_start_service(bt_a2dp_source_event_cback);

    return 0;
}

static int bt_a2dp_sink_init(void)
{
    int ret = 0;

    APP_DEBUG0("");

    ret = linkplay_bluetooth_avk_init(bt_a2dp_sink_event_callback, bt_a2dp_sink_sec_link_callback);
    if (ret == -1) {
        printf("bt_a2dp_sink_init failed.\n");
        return -1;
    }
    ret = linkplay_bluetooth_avk_register(g_bt_context.bt_config->support_aac_decode);
    if (ret == -1) {
        linkplay_bluetooth_avk_deinit();
        printf("bt_a2dp_sink_init failed.\n");
        return -1;
    }

    return 0;
}

static int bt_check_bsa_server(void)
{
    const char *bt_avk_name = "/tmp/bt-avk-fifo";
    const char *bt_socket_name = "/tmp/bt-daemon-socket";

    if (access(bt_socket_name, 0) || access(bt_avk_name, 0)) {
        return -1;
    } else {
        return 0;
    }
}

static int bsa_init_config(BD_ADDR bd_addr, char *name)
{
    APP_DEBUG0("");

    return linkplay_bluetooth_config_init(name, bd_addr);
}

static void bt_process()
{
    int status = g_bt_context.bt_status;
    switch (status) {
    case BT_STOP:
        break;
    case BT_START_FAILED:
        g_bt_context.bt_config->bas_server_init_call_back(0);
        bt_event_add_in_list(
            create_bt_event_node(BLUETOOTH_SERVICES_EVENT, BT_BSA_SERVER_INIT_FAILED, 0, NULL));
        status = BT_STOP;
        break;
    case BT_START_PRE:
        g_bt_context.bt_start_ts = tickSincePowerOn();
        if (!bt_check_bsa_server()) {
            status = BT_START_PROCESS;
        } else if (tickSincePowerOn() > g_bt_context.bt_start_ts + 50000 * 100) {
            status = BT_START_FAILED;
        } else {
        }
        break;
    case BT_START_PROCESS:
        if (bsa_init_config(g_bt_context.bt_config->bt_addr, g_bt_context.bt_config->bt_name) ==
            0) {
            status = BT_SERVER_OK;
            bt_event_add_in_list(create_bt_event_node(BLUETOOTH_SERVICES_EVENT,
                                                      BT_BSA_SERVER_INIT_SUCCESSFUL, 0, NULL));
        } else {
            status = BT_START_FAILED;
        }
        break;
    }

    g_bt_context.bt_status = status;
}

static int bt_dispatch_event(void)
{
    bt_event_node *item = NULL;
    int ret = -1;
    struct lp_list_head *pos = NULL, *tmp = NULL;
    int num = 0;

    do {
        pthread_mutex_lock(&event_list_mutex);
        num = lp_list_number(&list_bt_event);
        if (num <= 0) {
            pthread_mutex_unlock(&event_list_mutex);
            break;
        }

        lp_list_for_each_safe(pos, tmp, &list_bt_event)
        {
            item = lp_list_entry(pos, bt_event_node, entry);
            lp_list_del(pos);
            break;
        }
        pthread_mutex_unlock(&event_list_mutex);

        if (item) {
            g_bt_context.event_call_back(&item->event_msg, g_bt_context.call_back_context);
            if (item->event_msg.msg_value)
                free(item->event_msg.msg_value);
            free(item);
        }
    } while (1);

    return ret;
}

BtError linkplay_bluetooth_registerEventCallbacks(linkplay_bluetooth_event_callback callback,
                                                  void *context)
{
    g_bt_context.event_call_back = callback;
    g_bt_context.call_back_context = context;

    return kBtErrorOk;
}

BtError linkplay_bluetooth_schedule_event(void) { sem_post(&g_sem_bt_event); }
BtError linkplay_bluetooth_event_dump(int timeoutms)
{
    BtError bt_error = kBtErrorOk;
    struct timeval now;
    struct timespec timeToWait;

    if (g_bt_context.bt_status == BT_STOP) {
        return kBtErrorInitFailed;
    } else if (g_bt_context.bt_status != BT_SERVER_OK) {
        bt_process();
        return KBtErrorInitProcessing;
    }
    if (timeoutms >= 0) {
        gettimeofday(&now, NULL);
        timeraddMS(&now, timeoutms);
        timeToWait.tv_sec = now.tv_sec;
        timeToWait.tv_nsec = now.tv_usec * 1000;
        int ret = sem_timedwait(&g_sem_bt_event, &timeToWait);
        if (ret == 0) {
        }
    } else {
        sem_wait(&g_sem_bt_event);
    }

    bt_dispatch_event();

    return bt_error;
}

BtError linkplay_bt_server_init(linkplay_bt_config *bt_config)
{
    int todo_service = 0;
    pthread_t p_bt_tid;
    linkplay_bt_context *bt_context = &g_bt_context;

    printf("\r\n**** linkplay bluetooth sdk ver: %s-%d ****\r\n", __DATE__,
           LINKPLAY_BLUETOOTH_SDK_VERSION);

    memset(bt_context, 0x0, sizeof(bt_context));

    bt_context->bt_status = BT_START_PRE;
    bt_context->bt_cur_service = 0;
    bt_context->bt_config = (linkplay_bt_config *)malloc(sizeof(linkplay_bt_config));
    memcpy(bt_context->bt_config, bt_config, sizeof(linkplay_bt_config));
    sem_init(&g_sem_bt_event, 0, 0);

    if (bt_config->bas_server_init_call_back)
        bt_config->bas_server_init_call_back(1);

    return kBtErrorOk;
}
BtError linkplay_bluetooth_start_services(int services)
{

    if (services & BT_A2DP_SINK) {
        bt_a2dp_sink_init();
    }

    if (services & BT_A2DP_SOURCE) {
        bt_a2dp_source_start();
    }
#ifdef HFP_SUPPORT
    if (services & BT_HFP_HF) {
        bt_hfp_init();
    }
#endif
    return kBtErrorOk;
}
BtError linkplay_bluetooth_stop_services(int services) {}
static int check_params_vaild(int role, int index)
{
    int ret = 1;
    // printf("role %d, index %d \r\n",role,index);
    if (role != BT_ROLE_SINK && role != BT_ROLE_SOURCE) {
        ret = 0;
    }

    if (index < 0 && index >= 3) {
        ret = 0;
    }

    return ret;
}
BtError linkplay_bluetooth_playback_resume(int role, int index)
{
    BtError bt_error;

    if (!check_params_vaild(role, index)) {
        return KBtErrorParamsInvaild;
    }

    if (role == BT_ROLE_SINK) {
        bt_error = linkplay_bluetooth_avk_play_start(index);
    } else if (role == BT_ROLE_SOURCE) {
        bt_error = linkplay_bluetooth_av_resume_play(index);
    }

    return bt_error;
}
BtError linkplay_bluetooth_playback_pause(int role, int index)
{
    BtError bt_error;

    if (!check_params_vaild(role, index)) {
        return KBtErrorParamsInvaild;
    }

    if (role == BT_ROLE_SINK) {
        bt_error = linkplay_bluetooth_avk_play_pause(index);
    } else if (role == BT_ROLE_SOURCE) {
        bt_error = linkplay_bluetooth_av_pause_play(index);
    }

    return bt_error;
}
BtError linkplay_bluetooth_playback_next(int role, int index)
{
    BtError bt_error;

    if (!check_params_vaild(role, index) || role != BT_ROLE_SINK) {
        return KBtErrorParamsInvaild;
    }

    bt_error = linkplay_bluetooth_avk_next_music(index);

    return bt_error;
}
BtError linkplay_bluetooth_playback_prev(int role, int index)
{
    BtError bt_error;

    if (!check_params_vaild(role, index) || role != BT_ROLE_SINK) {
        return KBtErrorParamsInvaild;
    }

    bt_error = linkplay_bluetooth_avk_prev_music(index);

    return bt_error;
}
BtError linkplay_bluetooth_play_stop(int role, int index)
{
    BtError bt_error;

    if (!check_params_vaild(role, index)) {
        return KBtErrorParamsInvaild;
    }

    if (role == BT_ROLE_SINK) {
        bt_error = linkplay_bluetooth_avk_play_stop(index);
    } else if (role == BT_ROLE_SOURCE) {
        bt_error = linkplay_bluetooth_av_stop_play();
    }

    return bt_error;
}
BtError linkplay_bluetooth_close(int role, int index)
{
    BtError bt_error;

    if (!check_params_vaild(role, index)) {
        return KBtErrorParamsInvaild;
    }

    if (role == BT_ROLE_SINK) {
        bt_error = linkplay_bluetooth_avk_close(index);
    } else if (role == BT_ROLE_SOURCE) {
        bt_error = linkplay_bluetooth_av_close();
    }

    return bt_error;
}
BtError linkplay_bluetooth_play_stop_all(int role)
{
    BtError bt_error = kBtErrorFailed;

    if (role == BT_ROLE_SINK) {
        bt_error = linkplay_bluetooth_avk_play_stop_all();
    } else if (role == BT_ROLE_SOURCE) {
        bt_error = linkplay_bluetooth_av_stop_play();
    }

    return bt_error;
}
BtError linkplay_bluetooth_close_all(int role)
{
    BtError bt_error = kBtErrorFailed;

    if (role == BT_ROLE_SINK) {
        bt_error = linkplay_bluetooth_avk_close_all();
    } else if (role == BT_ROLE_SOURCE) {
        bt_error = linkplay_bluetooth_av_close();
    }

    return bt_error;
}
BtError linkplay_bluetooth_set_abs_volume(int role, int volume, int index)
{
    BtError bt_error = kBtErrorFailed;

    if (!check_params_vaild(role, index)) {
        return KBtErrorParamsInvaild;
    }

    if (role == BT_ROLE_SINK) {
        bt_error = linkplay_bluetooth_set_abs_vol(volume, index);
    } else if (role == BT_ROLE_SOURCE) {
        bt_error = linkplay_bluetooth_av_rc_send_abs_vol_command(volume);
    }

    return bt_error;
}
BtError linkplay_bluetooth_connect_by_addr(int role, BD_ADDR *mac, int timeoutms)
{
    BtError bt_error = kBtErrorFailed;

    if (role == BT_ROLE_SINK) {
        bt_error = app_avk_open_by_addr(mac, timeoutms);
    } else if (role == BT_ROLE_SOURCE) {
        bt_error = linkplay_bluetooth_av_open_by_addr(mac, timeoutms);
    }
    if (bt_error == ALREADY_CONNECTING)
        bt_error = KBtAlreadyConnecting;
    else if (bt_error == BT_DEVICES_NO_HISTORY)
        bt_error = kBtDevicesNoHistory;
    else if (bt_error == BT_HISTORY_NOT_FOUND)
        bt_error = kBtHistoryNotFound;
    else if (bt_error == BT_CONNECTED_FAILED)
        bt_error = kBtConnectFailed;
    else if (bt_error == BT_CONNECTED_SUCCESSED)
        bt_error = kBtErrorOk;

    return bt_error;
}
BtError linkplay_bluetooth_start_scan(int duration)
{
    pthread_mutex_lock(&event_list_mutex);
    bt_device_list_clear__(&list_bt_device);
    pthread_mutex_unlock(&event_list_mutex);
    linkplay_bluetooth_disc_stop_regular();
    return linkplay_bluetooth_disc_start_regular(bt_event_generic_disc_cback, duration);
}
BtError linkplay_bluetooth_cancel_scan(void) { return linkplay_bluetooth_disc_stop_regular(); }
BtError linkplay_bluetooth_registerAudioRenderCallBack(pcm_render_call_back callback, void *context)
{
    return app_avk_set_pcm_call_back_function(callback, context);
}

BtError linkplay_bluetooth_source_start_stream(tLINKPLAY_AV_MEDIA_FEED_CFG_PCM *pcm_config)
{
    return linkplay_bluetooth_av_start_stream(pcm_config);
}
BtError linkplay_bluetooth_source_send_stream(void *audio_buf, int length) // multi-thread protect
{
    return linkplay_bluetooth_av_send_stream(audio_buf, length);
}
BtError linkplay_bluetooth_stop_stream(void) { return linkplay_bluetooth_av_stop_stream(); }

int linkplay_bluetooth_get_connection_index_by_addr(BD_ADDR bd_addr)
{
    return app_avk_get_index_by_ad_addr(bd_addr);
}
BtError linkplay_bluetooth_get_remote_device_by_addr(REMOTE_DEVICE_INFO *remote_device,
                                                     BD_ADDR bt_addr)
{

    tAPP_XML_REM_DEVICE *rem_device = linkplay_get_remote_device_by_addr(bt_addr);

    if (rem_device) {
        memcpy(remote_device->bt_addr, rem_device->bd_addr, BD_ADDR_LEN);
        memcpy(remote_device->bt_name, rem_device->name, BD_NAME_LEN);
        remote_device->support_services = rem_device->trusted_services;

        return kBtErrorOk;
    }

    return kBtErrorFailed;
}
BtError linkplay_bluetooth_get_element(int index)
{
    return linkplay_bluetooth_avk_send_get_element_att_cmd(index);
}

void linkplay_bluetooth_set_debug_level(unsigned int level) { g_debug_level = level; }

void linkplay_bluetooth_av_rc_change(int event) { app_av_rc_change_play_status(event); }

/****** Bluetooth hfp interface *****/
#ifdef HFP_SUPPORT
static void bt_hfp_event_callback(tBSA_HS_EVT event, tBSA_HS_MSG *p_data)
{
    char *buf = NULL;
    char addr[32] = {0};
    bt_event_node *bt_event = NULL;
    json_object *json = NULL;

    switch (event) {
    case BSA_HS_AUDIO_OPEN_EVT: /* Audio Open Event */
        bt_event = create_bt_event_node(BLUETOOTH_HFP_EVENT, BT_HFP_AUDIO_OPEN_EVT,
                                        p_data->open.status, NULL);
        break;
    case BSA_HS_AUDIO_CLOSE_EVT: /* Audio Close event */
        bt_event = create_bt_event_node(BLUETOOTH_HFP_EVENT, BT_HFP_AUDIO_CLOSE_EVT,
                                        p_data->hdr.status, NULL);
        break;
    case BSA_HS_CONN_EVT: /* Service level connection */
        bt_address_to_string(addr, &p_data->conn.bd_addr);
        asprintf(&buf, "{\"bt_addr\":\"%s\"}", addr);
        bt_event = create_bt_event_node(BLUETOOTH_HFP_EVENT, BT_HFP_CONNECTED_EVT,
                                        p_data->conn.status, buf);
        break;
    case BSA_HS_CLOSE_EVT: /* Connection Closed (for info)*/
        // bt_address_to_string(addr, &p_data->conn.bd_addr);
        // asprintf(&buf, "{\"bt_addr\":\"%s\"}", addr);
        bt_event = create_bt_event_node(BLUETOOTH_HFP_EVENT, BT_HFP_DISCONNECTED_EVT,
                                        p_data->hdr.status, buf);
        break;
    case BSA_HS_RING_EVT:
        bt_event = create_bt_event_node(BLUETOOTH_HFP_EVENT, BT_HFP_RING_EVT, 0, NULL);
        break;
    case BSA_HS_CLIP_EVT:
        printf("##BSA_HS_CLIP_EVT val %s, num %d\n", p_data->val.str, p_data->val.num);
        char *ptr = strstr(p_data->val.str, "\"");
        char phone_num[256] = {0};
        if (ptr) {
            ptr++;
            strncpy(phone_num, ptr, 255);
            ptr = strstr(phone_num, "\"");
            if (ptr) {
                *ptr = '\0';
            }
        }
        asprintf(&buf, "{\"caller_id\":\"%s\"}", phone_num);
        bt_event = create_bt_event_node(BLUETOOTH_HFP_EVENT, BT_HFP_CLIP_EVT, 0, buf);
        break;
    case BSA_HS_CIND_EVT: /* CIND event */
        json = json_object_new_object();
        json_object_object_add(json, "cind_val", json_object_new_string(p_data->val.str));
        buf = strdup(json_object_to_json_string(json));
        bt_event = create_bt_event_node(BLUETOOTH_HFP_EVENT, BT_HFP_CIND_EVT, 0, buf);
        json_object_put(json);
        break;
    case BSA_HS_CIEV_EVT: /* CIEV event */
        asprintf(&buf, "{\"ciev_val\":\"%s\"}", p_data->val.str);
        bt_event = create_bt_event_node(BLUETOOTH_HFP_EVENT, BT_HFP_CIEV_EVT, 0, buf);
        break;
    case BSA_HS_VGS_EVT:
        asprintf(&buf, "{\"set_speaker_vol:\":\"%d\"}", p_data->val.num);
        bt_event = create_bt_event_node(BLUETOOTH_HFP_EVENT, BT_HFP_SPEAKER_VOL_SET_EVT, 0, buf);
        break;
    }

    if (bt_event)
        bt_event_add_in_list(bt_event);
}

static int bt_hfp_init(void)
{
    APP_DEBUG0("");
    int ret = 0;
    ret = linkplay_bluetooth_hfp_init(bt_hfp_event_callback);
    return ret;
}

static int bt_hfp_deinit(void)
{
    app_hs_stop(); /* Start Headset service*/
    return 0;
}

int linkplay_bluetooth_is_calling(void) { return app_hs_is_calling(); }

int linkplay_bluetooth_answer_call(void) { return linkplay_bluetooth_hfp_answer_call(); }

int linkplay_bluetooth_hangup_call(void) { return linkplay_bluetooth_hfp_hangup_call(); }

int linkplay_bluetooth_dial_num(char *num) { return linkplay_bluetooth_hfp_dial_num(num); }

int linkplay_bluetooth_dial_last_num(void) { return linkplay_bluetooth_hfp_dial_last_num(); }

UINT32 linkplay_bluetooth_get_hfp_capture_rate(void) { return app_hs_get_pcm_capture_rate(); }

int linkplay_bluetooth_enable_battery_indicate(void) { return app_hs_enable_battery_indicate(); }

int linkplay_bluetooth_set_battery_indicate(UINT32 level)
{
    return app_hs_set_battery_indicate(level);
}
#endif // HFP_SUPPORT
