/*****************************************************************************
**
**  Name:           app_ble_server.h
**
**  Description:    Bluetooth BLE include file
**
**  Copyright (c) 2014, Broadcom Corp., All Rights Reserved.
**  Broadcom Bluetooth Core. Proprietary and confidential.
**
*****************************************************************************/
#ifndef APP_BLE_SERVER_H
#define APP_BLE_SERVER_H

#include "bsa_api.h"
#include "app_ble.h"

typedef struct{
	void (*bt_gatts_server_create_cb)(void *context,tBSA_BLE_SE_CREATE_MSG *p_data);
	void (*bt_gatts_add_char_cb)(void *context,tBSA_BLE_SE_ADDCHAR_MSG *p_data);
	void (*bt_gatts_start_cb)(void *context,tBSA_BLE_SE_START_MSG *p_data);
	void (*bt_gatts_req_read_cb)(void *context,tBSA_BLE_SE_READ_MSG *p_data);
	void (*bt_gatts_req_write_cb)(void *context,tBSA_BLE_SE_WRITE_MSG *p_data);
	void (*bt_gatts_connection_open_cb)(void *context,tBSA_BLE_SE_OPEN_MSG *p_data);
	void (*bt_gatts_connection_close_cb)(void *context,tBSA_BLE_SE_CLOSE_MSG *p_data);
	void (*bt_gatts_set_mtu_cb)(void *context,tBSA_BLE_SE_MTU_MSG *p_data);
	void *context;
}BT_APP_GATTS_CB_FUNC_T;

/*******************************************************************************
 **
 ** Function         app_ble_server_find_free_space
 **
 ** Description      find free space for BLE server application
 **
 ** Parameters
 **
 ** Returns          positive number(include 0) if successful, error code otherwise
 **
 *******************************************************************************/
int app_ble_server_find_free_space(void);

/*******************************************************************************
 **
 ** Function         app_ble_server_display
 **
 ** Description      display BLE server
 **
 ** Parameters
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_ble_server_display(void);

/*******************************************************************************
 **
 ** Function         app_ble_server_find_reg_pending_index
 **
 ** Description      find registration pending index
 **
 ** Parameters
 **
 ** Returns          positive number(include 0) if successful, error code otherwise
 **
 *******************************************************************************/
int app_ble_server_find_reg_pending_index(void);

/*******************************************************************************
 **
 ** Function         app_ble_server_find_index_by_interface
 **
 ** Description      find BLE server index by interface
 **
 ** Parameters    if_num: interface number
 **
 ** Returns          positive number(include 0) if successful, error code otherwise
 **
 *******************************************************************************/
int app_ble_server_find_index_by_interface(tBSA_BLE_IF if_num);

/*******************************************************************************
 **
 ** Function         app_ble_server_find_index_by_conn_id
 **
 ** Description      find BLE server index by connection ID
 **
 ** Parameters       conn_id: Connection ID
 **
 ** Returns          positive number(include 0) if successful, error code otherwise
 **
 *******************************************************************************/
int app_ble_server_find_index_by_conn_id(UINT16 conn_id);

/*
 * BLE Server functions
 */
/*******************************************************************************
 **
 ** Function        app_ble_server_register
 **
 ** Description     Register server app
 **
 ** Parameters      uuid: uuid number of application
 **
 ** Returns         status: 0 if success / -1 otherwise
 **
 *******************************************************************************/
int app_ble_server_register(tBT_UUID *uuid, BT_APP_GATTS_CB_FUNC_T *p_cback);

/*******************************************************************************
 **
 ** Function        app_ble_server_deregister
 **
 ** Description     Deregister server app
 **
 ** Parameters      None
 **
 ** Returns         status: 0 if success / -1 otherwise
 **
 *******************************************************************************/
int app_ble_server_deregister(int num);

/*******************************************************************************
 **
 ** Function        app_ble_server_create_service
 **
 ** Description     create service
 **
 ** Parameters      None
 **
 ** Returns         status: 0 if success / -1 otherwise
 **
 *******************************************************************************/
int app_ble_server_create_service(int server_num, tBT_UUID *service, UINT16 num_handle,
                                  BOOLEAN is_primary);

/*******************************************************************************
 **
 ** Function        app_ble_server_add_char
 **
 ** Description     Add charcter to service
 **
 ** Parameters      None
 **
 ** Returns         status: 0 if success / -1 otherwise
 **
 *******************************************************************************/
int app_ble_server_add_char(int server_num, int srvc_attr_num, tBT_UUID *char_uuid, int is_descript,
                            int attribute_permission, int characteristic_property);
/*******************************************************************************
 **
 ** Function        app_ble_server_start_service
 **
 ** Description     Start Service
 **
 ** Parameters      None
 **
 ** Returns         status: 0 if success / -1 otherwise
 **
 *******************************************************************************/
int app_ble_server_start_service(int num, int attr_num);

/*******************************************************************************
 **
 ** Function        app_ble_server_send_indication
 **
 ** Description     Send indication to client
 **
 ** Parameters      None
 **
 ** Returns         status: 0 if success / -1 otherwise
 **
 *******************************************************************************/
int app_ble_server_send_indication(int num, int attr_num, int length_of_data, UINT8 *value,
                                   BOOLEAN need_confirm);

/*******************************************************************************
**
** Function         app_ble_server_profile_cback
**
** Description      BLE Server Profile callback.
**
** Returns          void
**
*******************************************************************************/
void app_ble_server_profile_cback(tBSA_BLE_EVT event, tBSA_BLE_MSG *p_data);

/*******************************************************************************
 **
 ** Function        app_ble_server_open
 **
 ** Description     This is the ble open connection to ble client
 **
 ** Parameters      None
 **
 ** Returns         status: 0 if success / -1 otherwise
 **
 *******************************************************************************/
int app_ble_server_open(void);

/*******************************************************************************
 **
 ** Function        app_ble_server_close
 **
 ** Description     This is the ble close connection
 **
 ** Parameters      None
 **
 ** Returns         status: 0 if success / -1 otherwise
 **
 *******************************************************************************/
int app_ble_server_close(int server_num);
#endif
