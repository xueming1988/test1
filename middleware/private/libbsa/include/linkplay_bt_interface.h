#ifndef __LINKPLAY_BT_INTERFACE_H__
#define __LINKPLAY_BT_INTERFACE_H__

#include "bt_types.h"
typedef unsigned char   UINT8;
typedef unsigned short  UINT16;
typedef unsigned int    UINT32;
typedef unsigned char   BOOLEAN;
#define BD_ADDR_LEN			6
#define BD_NAME_LEN			248

typedef UINT8		BD_ADDR[BD_ADDR_LEN];
typedef UINT8		BD_NAME[BD_NAME_LEN + 1];	/* Device name */

#define APP_BLE_MAIN_DEFAULT_APPL_UUID    9000
#define APP_BLE_MAIN_INVALID_UUID         0
#define APP_BLE_ADV_VALUE_LEN             10 /*This is temporary value, Total Adv data including all fields should be <31bytes*/
#define APP_BLE_ADDR_LEN                  20
#define BLE_ADV_APPEARANCE_DATA         0x832	/* Generic Heart rate Sensor */

typedef enum {
BT_LINK_DISCONNECTING,
BT_LINK_DISCONNECTED,
BT_LINK_CONNECTING,
BT_LINK_CONNECTED,
BT_LINK_CONNECT_FAILED,
} bsa_link_status;

typedef struct ble_create_service_params {
	int	server_num;	/* server number which is distributed */
	int	attr_num;
	tBT_UUID service_uuid;	/*service uuid */
	UINT16  num_handle;
	BOOLEAN	is_primary;
} ble_create_service_params;
typedef struct bsa_ble_add_character_params {
	int server_num;
	int srvc_attr_num;
	int char_attr_num;
	int is_descript;
	int attribute_permission;
	int characteristic_property;
	tBT_UUID char_uuid;
} ble_char_params;
typedef struct bsa_ble_server_indication {
	int server_num;
	int attr_num;
	int length_of_data;
	UINT8 *value;
	BOOLEAN need_confirm;	 /* TRUE: Indicate, FALSE: Notify */
} ble_server_indication;
#define NOTIFY_CONN_MAX_NUM  20
typedef struct bsa_ble_character_notify_data {
ble_char_params char_data;
int conn_id[NOTIFY_CONN_MAX_NUM];
} ble_char_notify;



int linkplay_bluetooth_ble_start(void);
int linkplay_bluetooth_ble_stop(void);
//int linkplay_bluetooth_ble_server_register(UINT16 uuid, tBSA_BLE_CBACK *p_cback);
int linkplay_bluetooth_ble_server_create_service(ble_create_service_params *params);
int linkplay_bluetooth_set_visibility(BOOLEAN discoverable, BOOLEAN connectable);
int linkplay_bluetooth_ble_set_visibility(BOOLEAN discoverable, BOOLEAN connectable);
int linkplay_bluetooth_ble_server_add_character(ble_char_params *params);
int  linkplay_bluetooth_ble_server_start_service(int server_num,int srvc_attr_num);
//int linkplay_bluetooth_set_ble_adv_data(tBSA_DM_BLE_ADV_CONFIG *p_data);
//int linkplay_bluetooth_ble_set_adv_param(tBSA_DM_BLE_ADV_PARAM *p_req);
int linkplay_bluetooth_ble_server_deregister(int server_num);
int linkplay_ble_server_send_indication(ble_server_indication *params);
//tAPP_BLE_CB *linkplay_bluetooth_ble_get_cb(void);



#endif
