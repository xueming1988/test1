/**
 * \file bt_main.c
 * \brief the bluetooth example code
 * \copyright Copyright 2018 Linkplay Inc. All rights reserved.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "linkplay_bluetooth_interface.h"
#include "common.h"
#include "lp_list.h"

#define MAX_SINK_PAIRED_LIST_NUMBER 5
#define SINK_SAVE_PROFILE "Audio Source"

#define MAX_SOURCE_PAIRED_LIST_NUMBER 5
#define SOURCE_SAVE_PROFILE "Audio Sink"

/* Menu items */
enum {
    APP_MENU_START_SINK = 1,
    APP_MENU_START_SOURCE,
    APP_MENU_SET_VISIBILITY,
    APP_MENU_SART_SCAN,
    APP_MENU_CANNEL_SCAN,
    APP_MENU_PLAYBACK_START,
    APP_MENU_PLAYBACK_PAUSE,
    APP_MENU_PLAYBACK_PREV,
    APP_MENU_PLAYBACK_NEXT,
    APP_MENU_RECONNECT_LAST_SINK,
    APP_MENU_MANUAL_DISCONNECT,
    APP_MENU_SOURCE_CONNECT,
    APP_MENU_SOURCE_PALY_MUSIC,
    APP_MENU_START_HFP,
    APP_MENU_ANSWER_CALL,
    APP_MENU_HANGUP_CALL,
    APP_INIT_BLE,
    APP_AVK_MENU_QUIT = 99
};

enum {
    COMMAND_START_SERVICES = 1,
    COMMAND_SET_VISIBILITY,
    COMMAND_START_SCAN,
    COMMAND_CANNEL_SCAN,
    COMMAND_PLAYBACK_START,
    COMMAND_PLAYBACK_PAUSE,
    COMMAND_PLAYBACK_PREV,
    COMMAND_PLAYBACK_NEXT,
    COMMAND_RECONNECT_LAST_SINK,
    COMMAND_MANUAL_DISCONNECT,
    COMMAND_SOURCE_COMMAND,
    COMMAND_ANSWER_CALL,
    COMMAND_HANGUP_CALL,
    COMMAND_INIT_BLE,
};

void app_bluetooth_display_main_menu(void)
{
    printf("\nBluetooth Main menu:\n");

    printf("    %d  => Start Sink Service\n", APP_MENU_START_SINK);
    printf("    %d  => Start Source Service\n", APP_MENU_START_SOURCE);
    printf("    %d  => Set connectable/discoverable \n", APP_MENU_SET_VISIBILITY);
    printf("    %d  => Scan \n", APP_MENU_SART_SCAN);
    printf("    %d  => Cancel Scan \n", APP_MENU_CANNEL_SCAN);
    printf("    %d  => Playback start \n", APP_MENU_PLAYBACK_START);
    printf("    %d  => Playback pause  \n", APP_MENU_PLAYBACK_PAUSE);
    printf("    %d  => Playback prev \n", APP_MENU_PLAYBACK_PREV);
    printf("    %d  => Playback next  \n", APP_MENU_PLAYBACK_NEXT);
    printf("    %d  => reconnect last sink  \n", APP_MENU_RECONNECT_LAST_SINK);
    printf("    %d  => manual disconnect \n", APP_MENU_MANUAL_DISCONNECT);
    printf("    %d  => source connect \n", APP_MENU_SOURCE_CONNECT);
    printf("    %d  => play source music  \n", APP_MENU_SOURCE_PALY_MUSIC);
    printf("    %d  => Start hfp service\n", APP_MENU_START_HFP);
    printf("    %d  => Answer the phone\n", APP_MENU_ANSWER_CALL);
    printf("    %d  => Hangup the phone\n", APP_MENU_HANGUP_CALL);
    printf("    %d  => Init BLE service \n", APP_INIT_BLE);
    printf("    %d  => Quit\n", APP_AVK_MENU_QUIT);
}

typedef struct {
    int link_state;
    void *source_handle;
} bt_context;
static bt_context g_bt_contex_test;
static pthread_mutex_t command_list_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct _bt_event_node {
    struct lp_list_head entry;
    int cmd_index;
    unsigned int params[6];
} bt_command_node;

static LP_LIST_HEAD(list_bt_command);
extern void *audio_render_pcm_close(void *context, void *pcm_handle);
extern void *audio_render_pcm_open(void *context, UINT8 format, UINT16 sample_rate,
                                   UINT8 num_channel, UINT8 bit_per_sample);
extern void audio_render_pcm_write_data(void *context, void *pcm_handle, int connection,
                                        UINT8 *p_buffer, int frames);
extern void *bt_a2dp_source_init_demo_play(void);
extern set_av_connect_state(void *arg, int connected);
extern void start_call();
extern void end_call();
extern void start_sound_capture_thread();

static void ParseMsgtobtaddress(char *msg, char *addr)
{
    json_object *new_obj = 0;
    char *bt_addr_string;

    new_obj = json_tokener_parse(msg);
    if (is_error(new_obj)) {
        printf("json_tokener_parse failed!! \r\n");
    }
    bt_addr_string = JSON_GET_STRING_FROM_OBJECT(new_obj, "bt_addr");

    if (bt_addr_string)
        strcpy(addr, bt_addr_string);

    json_object_put(new_obj);

    return;
}
int bt_del_history_by_addr(char *addr)
{
    char tmp[32] = {0};
    BD_ADDR bd_addr;
    string_to_bt_address(addr, &bd_addr);

    bt_delete_history_by_addr(bd_addr);
    if (linkplay_bluetooth_unpair_remote_device(bd_addr) != 0) {
        printf("linkplay_bluetooth_unpair_remote_device failed \r\n");
    }

    return 0;
}

static int bluetooth_event_callback(bt_event_msg *msg, void *context)
{

    printf("%s %d \r\n", __FUNCTION__, __LINE__);
    bt_context *bt_contex_t = (bt_context *)context;

    switch (msg->msg_type) {
    case BLUETOOTH_SERVICES_EVENT: {
        switch (msg->notify_event) {
        case BT_DISC_NEW_EVT:
            printf("%s \r\n", msg->msg_value);

            break;
        case BT_DISC_CMPL_EVT:
            printf("scan end\r\n");
            source_stream_init(bt_contex_t->source_handle);
            break;
        default:
            printf("not handle msg->notify_event %d, Ignore.\n", msg->notify_event);
            break;
        }
    } break;
    case BLUETOOTH_SEC_LINK_EVENT: {
        switch (msg->notify_event) {

        case BT_SEC_LINK_UP_EVT:
            printf("link up %s \r\n", msg->msg_value);
            break;
        case BT_SEC_LINK_DOWN_EVT:
            printf("link down %s \r\n", msg->msg_value);
            if (msg->notify_value == 0x13) {
                printf("Remote terminate bluetooth connection.\n");
            } else if (msg->notify_value == 0x8) {
                printf("BT connection timeout(beyond distance maybe)!\n");

                // auto reconnect
            } else if (msg->notify_value == 0x16) {
                printf("connection terminated by local host!\n");
            }
            break;
        }
    } break;
    case BLUETOOTH_SINK_EVENT:
        switch (msg->notify_event) {
        case BT_SINK_CONNECTED_EVT:
            if (msg->notify_value == 0) {
                BD_ADDR remote_bd_addr;
                char addr_string[32];
                char remove_addr[32];
                REMOTE_DEVICE_INFO remote_device_info;
                printf("sink conencted %s \r\n", msg->msg_value);

                ParseMsgtobtaddress(msg->msg_value, addr_string);
                string_to_bt_address(addr_string, &remote_bd_addr);

                char *pofile = bt_get_profile_by_addr(remote_bd_addr, GetBtListHistoryHead());
                if (pofile && strcmp(pofile, SINK_SAVE_PROFILE)) {
                    printf("skip avk connect event, since this profile not match\r\n");
                    break;
                }

                bt_contex_t->link_state = 1;
                if (bt_check_history_full(MAX_SINK_PAIRED_LIST_NUMBER, SINK_SAVE_PROFILE,
                                          addr_string, remove_addr)) {
                    bt_del_history_by_addr(remove_addr);
                }

                if (linkplay_bluetooth_get_remote_device_by_addr(&remote_device_info,
                                                                 remote_bd_addr) == kBtErrorOk) {
                    bt_devicelist_add_entry_sort(remote_device_info.bt_addr,
                                                 remote_device_info.bt_name, SINK_SAVE_PROFILE,
                                                 GetBtListHistoryHead());
                }
            }
            break;
        case BT_SINK_DISCONNECTED_EVT: {
            BD_ADDR remote_bd_addr;
            char addr_string[32];
            printf("disconencted %s \r\n", msg->msg_value);
            ParseMsgtobtaddress(msg->msg_value, addr_string);
            string_to_bt_address(addr_string, &remote_bd_addr);

            char *pofile = bt_get_profile_by_addr(remote_bd_addr, GetBtListHistoryHead());
            if (pofile && strcmp(pofile, SINK_SAVE_PROFILE)) {
                printf("skip avk connect event, since this profile not match\r\n");
                break;
            }
            bt_contex_t->link_state = 0;
        } break;
        case BT_SINK_STREAM_START_EVT:
            printf("BT_SINK_STREAM_START_EVT \r\n");
            break;
        case BT_SINK_STREAM_STOP_EVT:
            printf("BT_SINK_STREAM_STOP_EVT \r\n");
            break;
        case BT_SINK_AVRCP_OPEN_EVT:
            printf("BT_SINK_AVRCP_OPEN_EVT \r\n");
            break;
        case BT_SINK_AVRCP_CLOSE_EVT:
            printf("BT_SINK_AVRCP_CLOSE_EVT \r\n");
            break;
        case BT_SINK_REMOTE_RC_PLAY_EVT:
            printf("BT_SINK_REMOTE_RC_PLAY_EVT \r\n");
            break;
        case BT_SINK_REMOTE_RC_PAUSE_EVT:
            printf("BT_SINK_REMOTE_RC_PAUSE_EVT \r\n");
            break;
        case BT_SINK_REMOTE_RC_STOP_EVT:
            printf("BT_SINK_REMOTE_RC_STOP_EVT \r\n");
            break;
        case BT_SINK_REMOTE_RC_TRACK_CHANGE:
            printf("BT_SINK_REMOTE_RC_STOP_EVT \r\n");
            linkplay_bluetooth_get_element(0); // get element
            break;
        case BT_SINK_GET_ELEM_ATTR_EVT:
            printf("BT_SINK_GET_ELEM_ATTR_EVT \r\n");
            printf("%s \r\n", msg->msg_value);
            break;
        case BT_SINK_SET_ABS_VOL_CMD_EVT: {
            int volume;
            int index;
            unsigned char volume_set_phone[33] = {
                0,  3,  7,  11, 15, 19, 23, 27, 31, 35,  39,  43,  47,  51,  55,  59, 63,
                67, 71, 75, 79, 83, 87, 91, 95, 99, 103, 107, 111, 115, 119, 123, 127};
            unsigned char volume_set_dsp[33] = {0,  3,  6,  9,  12, 15, 18, 21, 24, 27, 30,
                                                33, 36, 39, 42, 45, 48, 51, 54, 57, 60, 63,
                                                66, 69, 72, 75, 78, 81, 84, 87, 90, 93, 100};
            for (index = 32; index >= 0; index--) {
                if (volume_set_phone[index] <= msg->msg_value)
                    break;
            }
            if (index >= 32)
                index = 32;

            if (index < 0)
                index = 0;
            volume = volume_set_dsp[index];
            printf("phone volume:  set avk volume\n");
            // set platfrom volume
        }
        }
        break;
    case BLUETOOTH_SOURCE_EVENT:
        switch (msg->notify_event) {
        case BT_SOURCE_CONNECTED_EVT:
            if (msg->notify_value == 0) {
                BD_ADDR remote_bd_addr;
                char addr_string[32];
                char remove_addr[32];
                REMOTE_DEVICE_INFO remote_device_info;
                printf("source conencted %s \r\n", msg->msg_value);

                if (bt_check_history_full(MAX_SINK_PAIRED_LIST_NUMBER, SINK_SAVE_PROFILE,
                                          addr_string, remove_addr)) {
                    bt_del_history_by_addr(remove_addr);
                }

                ParseMsgtobtaddress(msg->msg_value, addr_string);
                string_to_bt_address(addr_string, &remote_bd_addr);

                if (linkplay_bluetooth_get_remote_device_by_addr(&remote_device_info,
                                                                 remote_bd_addr) == kBtErrorOk) {
                    bt_devicelist_add_entry_sort(remote_device_info.bt_addr,
                                                 remote_device_info.bt_name, SOURCE_SAVE_PROFILE,
                                                 GetBtListHistoryHead());
                }
                set_av_connect_state(bt_contex_t->source_handle, 1);
                bt_contex_t->link_state = 1;
            }
            break;
        case BT_SOURCE_DISCONNECTED_EVT:
            set_av_connect_state(bt_contex_t->source_handle, 0);
            bt_contex_t->link_state = 0;
            break;
        case BT_SOURCE_AV_START_EVT:
            printf("status %d \r\n", msg->notify_value);
            if (msg->notify_value == 0) {
                // successful
            }
            break;
        case BT_SOURCE_AV_STOP_EVT:

            break;
        case BT_SOURCE_CFG_RSP_EVT:
            printf("frequency   %d \r\n", msg->notify_value);
            set_av_set_sample_rate(bt_contex_t->source_handle, msg->notify_value);

            break;
        case BT_SOURCE_REMOTE_CMD_EVT:
            printf("avrcpid  value %d \r\n", msg->notify_value);
#if 0
                    /* Operation ID list for Passthrough commands
                    */
#define AVRC_ID_SELECT 0x00     /* select */
#define AVRC_ID_UP 0x01         /* up */
#define AVRC_ID_DOWN 0x02       /* down */
#define AVRC_ID_LEFT 0x03       /* left */
#define AVRC_ID_RIGHT 0x04      /* right */
#define AVRC_ID_RIGHT_UP 0x05   /* right-up */
#define AVRC_ID_RIGHT_DOWN 0x06 /* right-down */
#define AVRC_ID_LEFT_UP 0x07    /* left-up */
#define AVRC_ID_LEFT_DOWN 0x08  /* left-down */
#define AVRC_ID_ROOT_MENU 0x09  /* root menu */
#define AVRC_ID_SETUP_MENU 0x0A /* setup menu */
#define AVRC_ID_CONT_MENU 0x0B  /* contents menu */
#define AVRC_ID_FAV_MENU 0x0C   /* favorite menu */
#define AVRC_ID_EXIT 0x0D       /* exit */
#define AVRC_ID_0 0x20          /* 0 */
#define AVRC_ID_1 0x21          /* 1 */
#define AVRC_ID_2 0x22          /* 2 */
#define AVRC_ID_3 0x23          /* 3 */
#define AVRC_ID_4 0x24          /* 4 */
#define AVRC_ID_5 0x25          /* 5 */
#define AVRC_ID_6 0x26          /* 6 */
#define AVRC_ID_7 0x27          /* 7 */
#define AVRC_ID_8 0x28          /* 8 */
#define AVRC_ID_9 0x29          /* 9 */
#define AVRC_ID_DOT 0x2A        /* dot */
#define AVRC_ID_ENTER 0x2B      /* enter */
#define AVRC_ID_CLEAR 0x2C      /* clear */
#define AVRC_ID_CHAN_UP 0x30    /* channel up */
#define AVRC_ID_CHAN_DOWN 0x31  /* channel down */
#define AVRC_ID_PREV_CHAN 0x32  /* previous channel */
#define AVRC_ID_SOUND_SEL 0x33  /* sound select */
#define AVRC_ID_INPUT_SEL 0x34  /* input select */
#define AVRC_ID_DISP_INFO 0x35  /* display information */
#define AVRC_ID_HELP 0x36       /* help */
#define AVRC_ID_PAGE_UP 0x37    /* page up */
#define AVRC_ID_PAGE_DOWN 0x38  /* page down */
#define AVRC_ID_POWER 0x40      /* power */
#define AVRC_ID_VOL_UP 0x41     /* volume up */
#define AVRC_ID_VOL_DOWN 0x42   /* volume down */
#define AVRC_ID_MUTE 0x43       /* mute */
#define AVRC_ID_PLAY 0x44       /* play */
#define AVRC_ID_STOP 0x45       /* stop */
#define AVRC_ID_PAUSE 0x46      /* pause */
#define AVRC_ID_RECORD 0x47     /* record */
#define AVRC_ID_REWIND 0x48     /* rewind */
#define AVRC_ID_FAST_FOR 0x49   /* fast forward */
#define AVRC_ID_EJECT 0x4A      /* eject */
#define AVRC_ID_FORWARD 0x4B    /* forward */
#define AVRC_ID_BACKWARD 0x4C   /* backward */
#define AVRC_ID_ANGLE 0x50      /* angle */
#define AVRC_ID_SUBPICT 0x51    /* subpicture */
#define AVRC_ID_F1 0x71         /* F1 */
#define AVRC_ID_F2 0x72         /* F2 */
#define AVRC_ID_F3 0x73         /* F3 */
#define AVRC_ID_F4 0x74         /* F4 */
#define AVRC_ID_F5 0x75         /* F5 */
#define AVRC_ID_VENDOR 0x7E     /* vendor unique */
#define AVRC_KEYPRESSED_RELEASE 0x80
#endif
            break;
        }

        break;
    case BLUETOOTH_HFP_EVENT:

#ifdef HFP_SUPPORT
        switch (msg->notify_event) {
        case BT_HFP_AUDIO_OPEN_EVT:
            start_call();
            break;
        case BT_HFP_AUDIO_CLOSE_EVT:
            end_call();
            break;
        case BT_HFP_CONNECTED_EVT:
            if (msg->msg_value)
                printf("hfp connect buf: %s\n", msg->msg_value);
            break;
        case BT_HFP_DISCONNECTED_EVT:
            printf("hfp disconnect\n");
            break;
        case BT_HFP_RING_EVT:
            printf("hfp ring alert\n");
            break;
        case BT_HFP_CLIP_EVT:
            printf("hfp clip val: %s\n", msg->msg_value);
            break;
        case BT_HFP_CIND_EVT:
            printf("hfp cind val: %s\n", msg->msg_value);
            break;
        case BT_HFP_CIEV_EVT:
            printf("hfp ciev val: %s\n", msg->msg_value);
            break;
        case BT_HFP_SPEAKER_VOL_SET_EVT:
            /* volume val: 0 ~ 15 */
            printf("hfp volume val: %s\n", msg->msg_value);
        }
#endif
        break;
    }
}
static bt_command_node *create_bt_command_node(int cmd_index, unsigned int *param, int num)
{
    bt_command_node *command_node = (bt_command_node *)malloc(sizeof(bt_command_node));
    command_node->cmd_index = cmd_index;
    if (num > 0)
        memcpy(command_node->params, param, num * sizeof(unsigned int));
    return command_node;
}

static void bt_command_add_in_list(bt_command_node *bt_command)
{
    pthread_mutex_lock(&command_list_mutex);
    lp_list_add_tail(&bt_command->entry, &list_bt_command);
    pthread_mutex_unlock(&command_list_mutex);
    linkplay_bluetooth_schedule_event();
}

static int bt_dispatch_command(void)
{
    bt_command_node *item = NULL;
    int ret = -1;
    struct lp_list_head *pos = NULL, *tmp = NULL;
    int num = 0;
    BtError bt_error;

    do {
        pthread_mutex_lock(&command_list_mutex);
        num = lp_list_number(&list_bt_command);
        if (num <= 0) {
            pthread_mutex_unlock(&command_list_mutex);
            break;
        }

        lp_list_for_each_safe(pos, tmp, &list_bt_command)
        {
            item = lp_list_entry(pos, bt_command_node, entry);
            lp_list_del(pos);
            break;
        }
        pthread_mutex_unlock(&command_list_mutex);
        printf("%s %d item=%d \r\n", __FUNCTION__, __LINE__, item);
        if (item) {
            switch (item->cmd_index) {
            case COMMAND_START_SERVICES:
                bt_error = linkplay_bluetooth_start_services(item->params[0]);
                break;
            case COMMAND_SET_VISIBILITY:
                bt_error = linkplay_bluetooth_set_visibility(item->params[0], item->params[1]);
                break;
            case COMMAND_START_SCAN:
                bt_error = linkplay_bluetooth_start_scan(4);
                break;
            case COMMAND_CANNEL_SCAN:
                bt_error = linkplay_bluetooth_cancel_scan();
                break;
            case COMMAND_PLAYBACK_START:
                bt_error = linkplay_bluetooth_playback_resume(item->params[0], 0);
                break;
            case COMMAND_PLAYBACK_PAUSE:
                bt_error = linkplay_bluetooth_playback_pause(item->params[0], 0);
                break;
            case COMMAND_PLAYBACK_PREV:
                bt_error = linkplay_bluetooth_playback_prev(0, 0);
                break;
            case COMMAND_PLAYBACK_NEXT:
                bt_error = linkplay_bluetooth_playback_next(0, 0);
                break;
            case COMMAND_RECONNECT_LAST_SINK: {
                BD_ADDR last_addr;
                if (bt_get_last_connection_addr(last_addr, SINK_SAVE_PROFILE)) {

                    printf("%02x:%02x:%02x:%02x:%02x:%02x \r\n", last_addr[0], last_addr[1],
                           last_addr[2], last_addr[3], last_addr[4], last_addr[5]);
                    bt_error = linkplay_bluetooth_connect_by_addr(BT_ROLE_SINK, &last_addr, 6000);
                }
            } break;
            case COMMAND_MANUAL_DISCONNECT:
                linkplay_bluetooth_play_stop_all(item->params[0]);
                linkplay_bluetooth_close_all(item->params[0]);
                break;
            case COMMAND_SOURCE_COMMAND: {
                BD_ADDR addr;
                string_to_bt_address(item->params[0], &addr);
                printf("%02x:%02x:%02x:%02x:%02x:%02x \r\n", addr[0], addr[1], addr[2], addr[3],
                       addr[4], addr[5]);
                bt_error = linkplay_bluetooth_connect_by_addr(BT_ROLE_SOURCE, &addr, 3000);
                free(item->params[0]);
            } break;
#ifdef HFP_SUPPORT
            case COMMAND_ANSWER_CALL:
                printf("%s %d  linkplay_bluetooth_answer_call \r\n", __FUNCTION__, __LINE__);
                linkplay_bluetooth_answer_call();
                break;
            case COMMAND_HANGUP_CALL:
                linkplay_bluetooth_hangup_call();
                break;
#endif
            case COMMAND_INIT_BLE:
                bt_gatts_init();
                break;
            default:
                bt_error = -99;
                break;
            }
            printf("bt_dispatch_command %d ret %d\r\n", item->cmd_index, bt_error);
        }
    } while (1);

    return ret;
}
static void bt_start_bsa_server(int enable)
{
    printf("%s %d \r\n", __FUNCTION__, __LINE__);
    if (enable) {
        system("echo 0 > /sys/class/rfkill/rfkill0/state");
        system("pkill bsa_server");
        system("echo 1 > /sys/class/rfkill/rfkill0/state");
        // system("bsa_server -d /dev/ttyS1 -p /etc/bluetooth/BCM4345C0.hcd  -r13 -u /tmp/ >
        // /dev/null &");
        // AP6212A use bcm43430 frimware
        system(
            "bsa_server -d /dev/ttyS1 -p /etc/bluetooth/BCM43430.hcd  -r13 -u /tmp/ > /dev/null &");

        // enable bt snoop
        //  system("bsa_server -d /dev/ttyS1 -p /etc/bluetooth/BCM43430.hcd  -r13 -u /tmp/ -b
        //  /tmp/web/btsnoop.cfa > /tmp/web/bsa_log.txt &");

    } else {
        system("echo 0 > /sys/class/rfkill/rfkill0/state");
        system("pkill bsa_server");
    }
    printf("%s %d \r\n", __FUNCTION__, __LINE__);

    return 0;
}

static void *bt_main_thread(void *args)
{
    BtError bt_err;
    int timeoutms = -1;
    linkplay_bt_config bt_config = {0};
    pcm_render_call_back callback;

    strncpy(bt_config.bt_name, "linkplaybt_demo", sizeof(bt_config.bt_name) - 1);
    bt_config.bas_server_init_call_back = bt_start_bsa_server;
    bt_config.support_aac_decode = FALSE;

    bt_err = linkplay_bt_server_init(&bt_config);
    if (bt_err != kBtErrorOk) {
        printf("linkplay_bt_server_init error \r\n");
        exit(1);
    }

    linkplay_bluetooth_registerEventCallbacks(bluetooth_event_callback, &g_bt_contex_test);

    callback.pcm_open_call_fn = audio_render_pcm_open;
    callback.pcm_close_call_fn = audio_render_pcm_close;
    callback.pcm_write_data_fn = audio_render_pcm_write_data;
    linkplay_bluetooth_registerAudioRenderCallBack(callback, NULL);
    bt_load_history();
    g_bt_contex_test.source_handle = bt_a2dp_source_init_demo_play();

    while (1) {
        bt_err = linkplay_bluetooth_event_dump(timeoutms);

        if (bt_err == kBtErrorInitFailed) {
            printf("bt server init error \r\n");
            exit(1);
        } else if (bt_err == KBtErrorInitProcessing) {
            timeoutms = 50;
            continue;
        } else if (bt_err == kBtErrorOk) {
            timeoutms = -1;
        }

        bt_dispatch_command();
    }

    return NULL;
}

static int app_get_user_choice(const char *querystring)
{
    int neg, value, c, base;
    int count = 0;

    base = 10;
    neg = 1;
    printf("%s => ", querystring);
    value = 0;
    do {
        c = getchar();

        if ((count == 0) && (c == '\n')) {
            return -1;
        }
        count++;

        if ((c >= '0') && (c <= '9')) {
            value = (value * base) + (c - '0');
        } else if ((c >= 'a') && (c <= 'f')) {
            value = (value * base) + (c - 'a' + 10);
        } else if ((c >= 'A') && (c <= 'F')) {
            value = (value * base) + (c - 'A' + 10);
        } else if (c == '-') {
            neg *= -1;
        } else if (c == 'x') {
            base = 16;
        }

    } while ((c != EOF) && (c != '\n'));

    return value * neg;
}

int main(int argc, char **argv)
{
    int choice;
    int connection_index;
    UINT16 delay;
    pthread_t p_bt_tid;
    bt_command_node *bt_command;
    unsigned int params[6];
    linkplay_bluetooth_set_debug_level(0xff);

    int err = pthread_create(&p_bt_tid, NULL, bt_main_thread, NULL);
    if (err != 0)
        printf("can't create thread: %s\n", strerror(err));

    if (argc >= 2) {
        bt_command = NULL;
        params[0] =
            BT_A2DP_SINK | BT_HFP_HF; /** important  !!!!!!!!! sink must start before source */
        bt_command = create_bt_command_node(COMMAND_START_SERVICES, params, 1);
        bt_command_add_in_list(bt_command);
        params[0] = 1;
        params[1] = 1;
        bt_command = create_bt_command_node(COMMAND_SET_VISIBILITY, params, 2);
        bt_command_add_in_list(bt_command);
        while (1)
            sleep(1);
    }

    do {

        app_bluetooth_display_main_menu();

        choice = app_get_user_choice("Select action");
        bt_command = NULL;

        switch (choice) {
        case APP_MENU_START_SINK:
            params[0] = BT_A2DP_SINK; /** important  !!!!!!!!! sink must start before source */
            bt_command = create_bt_command_node(COMMAND_START_SERVICES, params, 1);
            break;
        case APP_MENU_START_SOURCE:
            params[0] = BT_A2DP_SOURCE;
            bt_command = create_bt_command_node(COMMAND_START_SERVICES, params, 1);
            break;
        case APP_MENU_SET_VISIBILITY:
            printf("Choose discoverable \n");
            printf(" 0 , 1 \n");
            params[0] = app_get_user_choice("\n");
            printf("Choose connectable \n");
            printf(" 0 , 1 \n");
            params[1] = app_get_user_choice("\n");
            bt_command = create_bt_command_node(COMMAND_SET_VISIBILITY, params, 2);
            break;
        case APP_MENU_SART_SCAN:
            bt_command = create_bt_command_node(COMMAND_START_SCAN, params, 0);
            break;
        case APP_MENU_CANNEL_SCAN:
            bt_command = create_bt_command_node(COMMAND_CANNEL_SCAN, params, 0);
            break;
        case APP_MENU_PLAYBACK_START:
            printf("Choose Role \n");
            printf(" 0 SINK , 1 SOURCE \n");
            params[0] = app_get_user_choice("\n");
            bt_command = create_bt_command_node(COMMAND_PLAYBACK_START, params, 1);
            break;
        case APP_MENU_PLAYBACK_PAUSE:
            printf("Choose Role \n");
            printf(" 0 SINK , 1 SOURCE \n");
            params[0] = app_get_user_choice("\n");
            bt_command = create_bt_command_node(COMMAND_PLAYBACK_PAUSE, params, 1);
            break;
        case APP_MENU_PLAYBACK_PREV:
            bt_command = create_bt_command_node(COMMAND_PLAYBACK_PREV, params, 0);
            break;
        case APP_MENU_PLAYBACK_NEXT:
            bt_command = create_bt_command_node(COMMAND_PLAYBACK_NEXT, params, 0);
            break;
        case APP_MENU_RECONNECT_LAST_SINK:
            bt_command = create_bt_command_node(COMMAND_RECONNECT_LAST_SINK, params, 0);
            break;
        case APP_MENU_MANUAL_DISCONNECT:
            printf("Choose Role \n");
            printf(" 0 SINK , 1 SOURCE \n");
            params[0] = app_get_user_choice("\n");
            bt_command = create_bt_command_node(COMMAND_MANUAL_DISCONNECT, params, 1);
            break;
        case APP_MENU_SOURCE_CONNECT:
            printf("Enter addr ");
            char *input = (char *)malloc(32);
            int count = 0;
            do {
                char c = getchar();

                if (c == '\n' || (c == EOF)) {
                    break;
                }
                input[count++] = c;
            } while (count < 32);
            input[count] = 0;

            params[0] = (unsigned int)input;
            bt_command = create_bt_command_node(COMMAND_SOURCE_COMMAND, params, 1);

            break;
        case APP_MENU_SOURCE_PALY_MUSIC:
            system("/system/workdir/bin/smplayer /vendor/zypf.mp3 ");
            break;
#ifdef HFP_SUPPORT
        case APP_MENU_START_HFP:
            start_sound_capture_thread();
            params[0] = BT_HFP_HF;
            bt_command = create_bt_command_node(COMMAND_START_SERVICES, params, 1);
            break;
        case APP_MENU_ANSWER_CALL:
            bt_command = create_bt_command_node(COMMAND_ANSWER_CALL, params, 0);
            break;
        case APP_MENU_HANGUP_CALL:
            bt_command = create_bt_command_node(COMMAND_HANGUP_CALL, params, 0);
            break;
#endif
        case APP_INIT_BLE:
            bt_command = create_bt_command_node(COMMAND_INIT_BLE, params, 0);
            break;
        default:
            printf("main: Unknown choice:%d\n", choice);
            break;
        }

        if (bt_command)
            bt_command_add_in_list(bt_command);

    } while (choice != 99); /* While user don't exit application */

    return 0;
}
