#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "linkplay_bluetooth_interface.h"
#include "common.h"

typedef struct {
    int server_num;
    int attr_num;
    int char_attr_num;
    int ble_mtu;
} GATTS_CONTEXT;
GATTS_CONTEXT g_gatts_context = {0};

static unsigned char g_server_uuid[] = {0x54, 0x54, 0x41, 0x47, 0x79, 0x61, 0x6C, 0x50,
                                        0x6B, 0x6E, 0x69, 0x4C, 0x00, 0x01, 0x00, 0x01};
static unsigned char g_char_uuid[] = {0x52, 0x41, 0x48, 0x43, 0x79, 0x61, 0x6C, 0x50,
                                      0x6B, 0x6E, 0x69, 0x4C, 0x00, 0x01, 0x00, 0x02};

#define LP_BLE_SERVICE_HANDLE_NUM 8
#define BLE_PROTOCOL_VERSION 0x01, 0x00
#define BLE_PROTOCOL_TAG 0x4C, 0x50
#define BLE_ADV_PLATFORM_A98 0x00, 0x20
#define BLE_ADV_APPEARANCE_DATA 0x832 /* Generic Heart rate Sensor */
#define BLE_ADV_VALUE_LEN 0x0E

static void bt_gatts_server_create_callback(void *context, tBLE_SE_CREATE_MSG *p_data)
{
    printf("## bt_gatts_server_create_callback !\n");
    printf("server_if:%d status:%d service_id:%d\n", p_data->server_if, p_data->status,
           p_data->service_id);
    int index = 0;
    GATTS_CONTEXT *pgatts = (GATTS_CONTEXT *)context;
    ble_char_params char_params;

    char_params.server_num = pgatts->server_num;
    char_params.srvc_attr_num = pgatts->attr_num;
    char_params.char_uuid.len = LEN_UUID_128;
    memcpy(char_params.char_uuid.uu.uuid128, g_char_uuid, MAX_UUID_SIZE);
    char_params.is_descript = 0;
    /* Attribute Permissions[Eg: Read-0x1, Write-0x10, Read | Write-0x11] */
    char_params.attribute_permission = BLE_GATT_PERM_READ | BLE_GATT_PERM_WRITE;

    char_params.characteristic_property =
        BLE_GATT_CHAR_PROP_BIT_READ | BLE_GATT_CHAR_PROP_BIT_WRITE | BLE_GATT_CHAR_PROP_BIT_NOTIFY |
        BLE_GATT_CHAR_PROP_BIT_INDICATE;

    if (linkplay_bluetooth_ble_server_add_character(&char_params) < 0) {
        printf("linkplay_bluetooth_ble_server_add_character failed\n");
    }
    g_gatts_context.char_attr_num = char_params.char_attr_num;

    return;
}
static void bt_gatts_add_char_callback(void *context, tBLE_SE_ADDCHAR_MSG *p_data)
{
    printf("## bt_gatts_add_char_callback!\n");
    printf("status: %d\n", p_data->status);
    printf("attr_id: 0x%x\n", p_data->attr_id);
    GATTS_CONTEXT *pgatts = (GATTS_CONTEXT *)context;
    linkplay_bluetooth_ble_server_start_service(pgatts->server_num, pgatts->attr_num);
    return;
}
static int bt_gatts_advertise_active(GATTS_CONTEXT *pgatts)
{
    int state = -1;
    tBLE_ADV_CONFIG ble_config_adv = {0};
    tBLE_ADV_PARAM ble_adv_param;
    char addr[128] = {0};
    BD_ADDR bd_addr;
    UINT8 app_ble_adv_value[BLE_ADV_VALUE_LEN] = {
        BLE_PROTOCOL_TAG,     // TAG
        BLE_ADV_PLATFORM_A98, // platform
        BLE_PROTOCOL_VERSION, // version
        0x00,
        0x00, // pid
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00, // Mac
    };

    ble_config_adv.appearance_data = BLE_ADV_APPEARANCE_DATA;
    ble_config_adv.flag = BLE_ADV_FLAG_MASK;
    ble_config_adv.is_scan_rsp = 0; /* 0 is Active broadcast, 1 is Passive broadcast */

    ble_config_adv.services_128b.list_cmpl = TRUE;
    memcpy(ble_config_adv.services_128b.uuid128, g_server_uuid, MAX_UUID_SIZE);

    /* All the masks/fields that are set will be advertised */
    ble_config_adv.adv_data_mask =
        BLE_AD_BIT_DEV_NAME | BLE_AD_BIT_FLAGS | BLE_AD_BIT_SERVICE_128 | BLE_AD_BIT_APPEARANCE;

    state = linkplay_bluetooth_set_ble_adv_data(&ble_config_adv);
    if (state) {
        printf("linkplay_bluetooth_set_ble_adv_data failed, state = %d.\n", state);
        return -1;
    }

    linkplay_bluetooth_get_addr(addr);
    string_to_bt_address(addr, &bd_addr);

    /* Passive broadcast */
    memset(&ble_config_adv, 0, sizeof(tBLE_ADV_CONFIG));
    /* start advertising */
    ble_config_adv.adv_data_mask = BLE_AD_BIT_MANU | BLE_AD_BIT_DEV_NAME;
    ble_config_adv.is_scan_rsp = 1;
    ble_config_adv.len = BLE_ADV_VALUE_LEN;

    memcpy(ble_config_adv.p_val, app_ble_adv_value, BLE_ADV_VALUE_LEN);

    state = linkplay_bluetooth_set_ble_adv_data(&ble_config_adv);
    if (state) {
        printf("linkplay_bluetooth_set_ble_adv_data failed, state = %d.\n", state);
        return -1;
    }

    /* set Advertising_Interval */
    /* Range: 0x0020 to 0x4000
       Time = Range * 0.625 msec
       Time Range: 20 ms to 10.24 sec */
    memset(&ble_adv_param, 0, sizeof(tBLE_ADV_PARAM));
    ble_adv_param.adv_int_min = 0xa0; /* 100 ms */
    ble_adv_param.adv_int_max = 0xa0; /* 100 ms */
    linkplay_bluetooth_ble_set_adv_param(&ble_adv_param);

    return 0;
}

static void bt_gatts_start_callback(void *context, tBLE_SE_START_MSG *p_data)
{
    printf("## bt_gatts_start_callback!\n");
    printf("status: %d\n", p_data->status);
    bt_gatts_advertise_active((GATTS_CONTEXT *)context);
    linkplay_bluetooth_ble_set_visibility(TRUE, TRUE);
    return;
}
static void bt_gatts_req_read_callback(void *context, tBLE_SE_READ_MSG *p_data)
{
    tBLE_SE_SENDRSP send_server_resp;
    UINT8 attribute_value[GATT_MAX_ATTR_LEN] = {0x11, 0x22, 0x33, 0x44};

    printf("## bt_gatts_req_read_callback!\n");
    linkplay_ble_SeSendRspInit(&send_server_resp);
    send_server_resp.conn_id = p_data->conn_id;
    send_server_resp.trans_id = p_data->trans_id;
    send_server_resp.status = p_data->status;
    send_server_resp.handle = p_data->handle;
    send_server_resp.offset = p_data->offset;
    send_server_resp.len = 4;
    send_server_resp.auth_req = 0;
    memcpy(send_server_resp.value, attribute_value, GATT_MAX_ATTR_LEN);
    printf("BSA_BLE_SE_READ_EVT: send_server_resp.conn_id:%d, send_server_resp.trans_id:%d",
           send_server_resp.conn_id, send_server_resp.trans_id, send_server_resp.status);
    printf("BSA_BLE_SE_READ_EVT: send_server_resp.status:%d,send_server_resp.auth_req:%d",
           send_server_resp.status, send_server_resp.auth_req);
    printf("BSA_BLE_SE_READ_EVT: send_server_resp.handle:%d, send_server_resp.offset:%d, "
           "send_server_resp.len:%d",
           send_server_resp.handle, send_server_resp.offset, send_server_resp.len);
    linkplay_ble_SeSendRsp(&send_server_resp);
}
static void bt_gatts_req_write_callback(void *context, tBLE_SE_WRITE_MSG *p_data)
{
    int i;

    printf("## bt_gatts_req_write_callback!\n");
    printf("status: %d\n", p_data->status);
    printf("conn_id: %d, handle: %d\n", p_data->conn_id, p_data->handle);

    for (i = 0; i < p_data->len; i++) {
        printf("value[%d] = 0x%x\n", i, p_data->value[i]);
    }

    bt_gatts_send_indication(p_data->value, p_data->len);
}

static void bt_gatts_connection_open_callback(void *context, tBLE_SE_OPEN_MSG *p_data)
{
    printf("## bt_gatts_connection_open_callback!\n");
}
static void bt_gatts_connection_close_callback(void *context, tBLE_SE_CLOSE_MSG *p_data) {}
static void bt_gatts_set_mtu_callback(void *context, tBLE_SE_MTU_MSG *p_data)
{
    printf("## bt_gatts_set_mtu_callback %d !\n", p_data->mtu);
    g_gatts_context.ble_mtu = p_data->mtu;
}
int bt_gatts_send_indication(char *s, int value_len)
{
    int i = 0;
    int server_num = 0;
    int char_attr_num = 0;
    int total_send_len = 0;
    int len = 0;
    char *send_buf = NULL;
    ble_server_indication ble_indication;

    send_buf = (unsigned char *)malloc(g_gatts_context.ble_mtu * sizeof(unsigned char) + 1);

    if (!send_buf) {
        printf("malloc send buf failed\n");
        return -1;
    }

    server_num = g_gatts_context.server_num;
    char_attr_num = g_gatts_context.char_attr_num;

    printf("server_num %d char_attr_num %d \r\n", server_num, char_attr_num);

    while (total_send_len < value_len) {
        if (value_len <= g_gatts_context.ble_mtu) {
            len = value_len;
        } else {
            if (value_len - total_send_len < g_gatts_context.ble_mtu) {
                len = value_len - total_send_len;
            } else {
                len = g_gatts_context.ble_mtu;
            }
        }
        memset(&ble_indication, 0x0, sizeof(ble_server_indication));
        memcpy(send_buf, s + total_send_len, len);
        send_buf[len] = '\0';
        ble_indication.value = send_buf;
        ble_indication.server_num = server_num;
        ble_indication.attr_num = char_attr_num;
        ble_indication.need_confirm = 0;
        ble_indication.length_of_data = len;

        /* send client indication */
        int ret = linkplay_ble_server_send_indication(&ble_indication);
        printf("linkplay_ble_server_send_indication %d \r\n", ret);

        total_send_len += len;
    }

    if (send_buf)
        free(send_buf);

    return 0;
}

int ble_service_server_register(void)
{
    int i = 0;
    int state = 0;
    tBLE_UUID server_uuid;
    tBT_APP_GATTS_CB_FUNC_T func;

    func.bt_gatts_server_create_cb = bt_gatts_server_create_callback;
    func.bt_gatts_add_char_cb = bt_gatts_add_char_callback;
    func.bt_gatts_start_cb = bt_gatts_start_callback;
    func.bt_gatts_req_read_cb = bt_gatts_req_read_callback;
    func.bt_gatts_req_write_cb = bt_gatts_req_write_callback;
    func.bt_gatts_connection_open_cb = bt_gatts_connection_open_callback;
    func.bt_gatts_connection_close_cb = bt_gatts_connection_close_callback;
    func.bt_gatts_set_mtu_cb = bt_gatts_set_mtu_callback;
    func.context = &g_gatts_context;

    server_uuid.len = LEN_UUID_128;
    memcpy(server_uuid.uu.uuid128, g_server_uuid, MAX_UUID_SIZE);
    g_gatts_context.server_num = linkplay_bluetooth_ble_server_register(&server_uuid, &func);
    if (g_gatts_context.server_num < 0) {
        printf("linkplay_bluetooth_ble_server_register failed %d \r\n", g_gatts_context.server_num);
    }
    ble_create_service_params params;
    params.server_num = g_gatts_context.server_num;
    memcpy(&params.service_uuid, &server_uuid, sizeof(tBLE_UUID));
    params.num_handle = LP_BLE_SERVICE_HANDLE_NUM;
    params.is_primary = 1;
    state = linkplay_bluetooth_ble_server_create_service(&params);
    if (state) {
        printf("linkplay_bluetooth_ble_server_create_service failed, state = %d.\n", state);
        return -1;
    }
    g_gatts_context.attr_num = params.attr_num;

    return 0;
}

int bt_gatts_init(void)
{
    int state = 0;
    state = linkplay_bluetooth_ble_start();
    if (state) {
        printf("bt_gatts_init failed, state = %d.\n", state);
        return -1;
    }

    state = ble_service_server_register();

    if (state != 0) {
        linkplay_bluetooth_ble_stop();
        printf("ERROR: ble_service_and_char_create failed !\n");
        return -1;
    }

    g_gatts_context.ble_mtu = 150;

    return 0;
}
