/*****************************************************************************
 **
 **  Name:           app_avk_main.c
 **
 **  Description:    Bluetooth Manager application
 **
 **  Copyright (c) 2015, Broadcom Corp., All Rights Reserved.
 **  Broadcom Bluetooth Core. Proprietary and confidential.
 **
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "bsa_api.h"
#include "app_xml_utils.h"

#include "gki.h"
#include "uipc.h"

#include "app_avk.h"
#include "bsa_avk_api.h"
#include "app_xml_param.h"
#include "app_mgt.h"
#include "app_disc.h"
#include "app_utils.h"

/* Menu items */
enum {
    APP_AVK_MENU_DISCOVERY = 1,
    APP_AVK_MENU_REGISTER,
    APP_AVK_MENU_DEREGISTER,
    APP_AVK_MENU_OPEN,
    APP_AVK_MENU_CLOSE,
    APP_AVK_MENU_PLAY_START,
    APP_AVK_MENU_PLAY_STOP,
    APP_AVK_MENU_PLAY_PAUSE,
    APP_AVK_MENU_PLAY_NEXT_TRACK,
    APP_AVK_MENU_PLAY_PREVIOUS_TRACK,
    APP_AVK_MENU_RC_CMD,
    APP_AVK_MENU_GET_ELEMENT_ATTR,
    APP_AVK_MENU_GET_CAPABILITIES,
    APP_AVK_MENU_REGISTER_NOTIFICATION,
    APP_AVK_MENU_SEND_DELAY_RPT,
    APP_AVK_VOLUME_ABS_UP,
    APP_AVK_VOLUME_ABS_DOWN,
    APP_AVK_MENU_QUIT = 99
};

typedef enum {
    BT_LINK_DISCONNECTING,
    BT_LINK_DISCONNECTED,
    BT_LINK_CONNECTING,
    BT_LINK_CONNECTED,
    BT_LINK_CONNECT_FAILED,
} bsa_link_status;


/*******************************************************************************
 **
 ** Function         app_avk_display_main_menu
 **
 ** Description      This is the main menu
 **
 ** Parameters
 **
 ** Returns          void
 **
 *******************************************************************************/
void app_avk_display_main_menu(void)
{
    printf("\nBluetooth AVK Main menu:\n");

    printf("    %d  => Start Discovery\n", APP_AVK_MENU_DISCOVERY);
    printf("    %d  => AVK Register\n", APP_AVK_MENU_REGISTER);
    printf("    %d  => AVK DeRegister\n", APP_AVK_MENU_DEREGISTER);
    printf("    %d  => AVK Open\n", APP_AVK_MENU_OPEN);
    printf("    %d  => AVK Close\n", APP_AVK_MENU_CLOSE);
    printf("    %d  => AVK Start steam play\n", APP_AVK_MENU_PLAY_START);
    printf("    %d  => AVK Stop stream play\n", APP_AVK_MENU_PLAY_STOP);
    printf("    %d  => AVK Pause stream play\n", APP_AVK_MENU_PLAY_PAUSE);
    printf("    %d  => AVK Play next track\n", APP_AVK_MENU_PLAY_NEXT_TRACK);
    printf("    %d  => AVK Play previous track\n", APP_AVK_MENU_PLAY_PREVIOUS_TRACK);
    printf("    %d  => AVRC Command\n", APP_AVK_MENU_RC_CMD);
    printf("    %d  => AVK Send Get Element Attributes Command\n", APP_AVK_MENU_GET_ELEMENT_ATTR);
    printf("    %d  => AVK Get capabilities\n", APP_AVK_MENU_GET_CAPABILITIES);
    printf("    %d  => AVK Register Notification\n", APP_AVK_MENU_REGISTER_NOTIFICATION);
    printf("    %d  => AVK Send Delay Report\n", APP_AVK_MENU_SEND_DELAY_RPT);
    printf("    %d  => Quit\n", APP_AVK_MENU_QUIT);
}


/*******************************************************************************
 **
 ** Function         app_avk_mgt_callback
 **
 ** Description      This callback function is called in case of server
 **                  disconnection (e.g. server crashes)
 **
 ** Parameters
 **
 ** Returns          TRUE if the event was completely handle, FALSE otherwise
 **
 *******************************************************************************/
static BOOLEAN app_avk_mgt_callback(tBSA_MGT_EVT event, tBSA_MGT_MSG *p_data)
{
    switch(event) {
        case BSA_MGT_STATUS_EVT:
            APP_DEBUG0("BSA_MGT_STATUS_EVT");
            if(p_data->status.enable) {
                APP_DEBUG0("Bluetooth restarted => re-initialize the application");
                app_avk_init(NULL);
            }
            break;

        case BSA_MGT_DISCONNECT_EVT:
            APP_DEBUG1("BSA_MGT_DISCONNECT_EVT reason:%d", p_data->disconnect.reason);
            /* Connection with the Server lost => Just exit the application */
            //exit(-1);
            break;

        default:
            break;
    }
    return FALSE;
}

/*******************************************************************************
 **
 ** Function         app_avk_display_connections
 **
 ** Description      This function displays the connections
 **
 ** Returns          status
 **
 *******************************************************************************/
void app_avk_display_connections(void)
{
    int index;
    tAPP_AVK_CONNECTION *conn = NULL;

    int num_conn = app_avk_num_connections();

    if(num_conn == 0) {
        APP_INFO0("    No connections");
        return;
    }

    for(index = 0; index < APP_AVK_MAX_CONNECTIONS; index++) {
        conn = app_avk_find_connection_by_index(index);
        if(conn->in_use) {
            APP_INFO1("    Connection index %d: - Connected to %02X:%02X:%02X:%02X:%02X:%02X", index,
                      conn->bda_connected[0], conn->bda_connected[1],
                      conn->bda_connected[2], conn->bda_connected[3],
                      conn->bda_connected[4], conn->bda_connected[5]);
        }
    }
}

/*******************************************************************************
 **
 ** Function         main
 **
 ** Description      This is the main function to connect to AVK. It is assumed that an other process handle security.
 **
 ** Parameters
 **
 ** Returns          void
 **
 *******************************************************************************/
int avk_main(int argc, char **argv)
{
    int choice, avrcp_evt;
    int connection_index;
    UINT16 delay;

    tAPP_AVK_CONNECTION *connection = NULL;

    /* Open connection to BSA Server */
    app_mgt_init();
    if(app_mgt_open(NULL, app_avk_mgt_callback) < 0) {
        APP_ERROR0("Unable to connect to server");
        return -1;
    }

    /* Init XML state machine */
    app_xml_init();

    app_avk_init(NULL);

    do {

        app_avk_display_main_menu();

        choice = app_get_choice("Select action");

        switch(choice) {

            case APP_AVK_MENU_DISCOVERY:
                /* Example to perform Device discovery (in blocking mode) */
                app_disc_start_regular(NULL);
                break;

            case APP_AVK_MENU_REGISTER:
                /* Example to register AVK connection end point*/
                app_avk_register(0);
                break;
            case APP_AVK_MENU_DEREGISTER:
                /* Example to de-register an AVK end point */
                app_avk_deregister();
                break;
            case APP_AVK_MENU_OPEN:
                /* Example to Open AVK connection (connect device)*/
                app_avk_open(-1);
                break;

            case APP_AVK_MENU_CLOSE:
                /* Example to Close AVK connection (disconnect device)*/
                printf("Choose connection index\n");
                app_avk_display_connections();
                connection_index = app_get_choice("\n");
                connection = app_avk_find_connection_by_index(connection_index);
                if(connection)
                    app_avk_close(connection->bda_connected);
                else
                    printf("Unknown choice:%d\n", connection_index);
                break;

            case APP_AVK_MENU_PLAY_START:
                /* Example to start stream play */
                printf("Choose connection index\n");
                app_avk_display_connections();
                connection_index = app_get_choice("\n");
                connection = app_avk_find_connection_by_index(connection_index);
                if(connection)
                    app_avk_play_start(connection);
                else
                    printf("Unknown choice:%d\n", connection_index);
                break;

            case APP_AVK_MENU_PLAY_STOP:
                /* Example to stop stream play */
                printf("Choose connection index\n");
                app_avk_display_connections();
                connection_index = app_get_choice("\n");
                connection = app_avk_find_connection_by_index(connection_index);
                if(connection)
                    app_avk_play_stop(connection);
                else
                    printf("Unknown choice:%d\n", connection_index);
                break;

            case APP_AVK_MENU_PLAY_PAUSE:
                /* Example to pause stream play */
                printf("Choose connection index\n");
                app_avk_display_connections();
                connection_index = app_get_choice("\n");
                connection = app_avk_find_connection_by_index(connection_index);
                if(connection)
                    app_avk_play_pause(connection);
                else
                    printf("Unknown choice:%d\n", connection_index);
                break;

            case APP_AVK_MENU_PLAY_NEXT_TRACK:
                /* Example to play next track */
                printf("Choose connection index\n");
                app_avk_display_connections();
                connection_index = app_get_choice("\n");
                connection = app_avk_find_connection_by_index(connection_index);
                if(connection)
                    app_avk_play_next_track(connection);
                else
                    printf("Unknown choice:%d\n", connection_index);
                break;

            case APP_AVK_MENU_PLAY_PREVIOUS_TRACK:
                /* Example to play previous track */
                printf("Choose connection index\n");
                app_avk_display_connections();
                connection_index = app_get_choice("\n");
                connection = app_avk_find_connection_by_index(connection_index);
                if(connection)
                    app_avk_play_previous_track(connection);
                else
                    printf("Unknown choice:%d\n", connection_index);
                break;

            case APP_AVK_MENU_RC_CMD:
                /* Example to send AVRC cmd */
                printf("Choose connection index\n");
                app_avk_display_connections();
                connection_index = app_get_choice("\n");
                connection = app_avk_find_connection_by_index(connection_index);
                if(connection)
                    app_avk_rc_cmd(connection->rc_handle);
                else
                    printf("Unknown choice:%d\n", connection_index);
                break;

            case APP_AVK_MENU_GET_ELEMENT_ATTR:
                /* Example to send Vendor Dependent command */
                printf("Choose connection index\n");
                app_avk_display_connections();
                connection_index = app_get_choice("\n");
                connection = app_avk_find_connection_by_index(connection_index);
                if(connection)
                    app_avk_send_get_element_attributes_cmd(connection->rc_handle);
                else
                    printf("Unknown choice:%d\n", connection_index);
                break;

            case APP_AVK_MENU_GET_CAPABILITIES:
                /* Example to send Vendor Dependent command */
                printf("Choose connection index\n");
                app_avk_display_connections();
                connection_index = app_get_choice("\n");
                connection = app_avk_find_connection_by_index(connection_index);
                if(connection)
                    app_avk_send_get_capabilities(connection->rc_handle);
                else
                    printf("Unknown choice:%d\n", connection_index);
                break;

            case APP_AVK_MENU_REGISTER_NOTIFICATION:
                /* Example to send Vendor Dependent command */
                printf("Choose connection index\n");
                app_avk_display_connections();
                connection_index = app_get_choice("\n");
                connection = app_avk_find_connection_by_index(connection_index);
                if(connection) {
                    printf("Choose Event ID\n");
                    printf("    1=play_status 2=track_change 5=play_pos 8=app_set\n");
                    avrcp_evt = app_get_choice("\n");
                    app_avk_send_register_notification(avrcp_evt, connection->rc_handle);
                } else
                    printf("Unknown choice:%d\n", connection_index);
                break;

            case APP_AVK_MENU_SEND_DELAY_RPT:
                APP_INFO0("delay value(0.1ms unit)");
                delay = app_get_choice("\n");
                app_avk_send_delay_report(delay);
                break;
            /*
            case APP_AVK_VOLUME_ABS_UP:
            printf("Choose connection index\n");
            app_avk_display_connections();
            connection_index = app_get_choice("\n");
            connection = app_avk_find_connection_by_index(connection_index);
            if(connection)
            {
               app_avk_cb.volume += 8;
               if(app_avk_cb.volume > 127)
                app_avk_cb.volume = 127;
               app_avk_reg_notfn_rsp(app_avk_cb.volume,connection->rc_handle,connection->volChangeLabel,AVRC_EVT_VOLUME_CHANGE,BTA_AV_RSP_CHANGED);
               printf("volume up change locally at TG\n");
             }break;
            case APP_AVK_VOLUME_ABS_DOWN:
            printf("Choose connection index\n");
            app_avk_display_connections();
            connection_index = app_get_choice("\n");
            connection = app_avk_find_connection_by_index(connection_index);
            if(connection){
                app_avk_cb.volume -= 8;
                if(app_avk_cb.volume < 0 )
                    app_avk_cb.volume = 0;
                app_avk_reg_notfn_rsp(app_avk_cb.volume,connection->rc_handle,connection->volChangeLabel,AVRC_EVT_VOLUME_CHANGE,BTA_AV_RSP_CHANGED);
                printf("volume down change locally at TG\n");}
                break;
                */
            case APP_AVK_MENU_QUIT:
                printf("main: Bye Bye\n");
                break;

            default:
                printf("main: Unknown choice:%d\n", choice);
                break;
        }
    } while(choice != 99);  /* While user don't exit application */

    /* Terminate the profile */
    app_avk_end();


    /* Close BSA before exiting (to release resources) */
    app_mgt_close();

    return 0;
}

int linkplay_bluetooth_config_init(const char *bt_name, UINT8 *btaddr, tAPP_MGT_CUSTOM_CBACK *mgt_custom_callback)
{
    return app_manager_init(bt_name, btaddr, mgt_custom_callback);
}

int linkplay_bluetooth_connect_server(void)
{
    /* Open connection to BSA Server */
    app_mgt_init();
    if(app_mgt_open(NULL, app_avk_mgt_callback) < 0) {
        APP_ERROR0("Unable to connect to server");
        return -1;
    }
    return 0;
}
int linkplay_bluetooth_disconnect_server(void)
{
    return app_mgt_close();
}

int linkplay_bluetooth_avk_init(tAvkCallback pcb, tsecuritycback spcb)
{
    /* Init XML state machine */
    app_xml_init();
    app_mgr_set_callback(spcb);

    return app_avk_init(pcb);
}

void linkplay_bluetooth_avk_deinit(void)
{
    /* Terminate the profile */
    app_avk_end();
}

int linkplay_bluetooth_avk_register(int b_support_aac_decode)
{
    return app_avk_register(b_support_aac_decode);
}

void linkplay_bluetooth_avk_deregister(void)
{
    app_avk_deregister();
}

int linkplay_bluetooth_auto_reconnect(int index)
{
    return app_avk_open(index);
}
int linkplay_bluetooth_connect_byaddr(BD_ADDR *bd_addr_in)
{
    return app_avk_open_by_addr(bd_addr_in);
}

int linkplay_bluetooth_avk_play_stop(int index)
{
    int ret = -1;
    tAPP_AVK_CONNECTION *connection = NULL;
    connection = app_avk_find_connection_by_index(index);
    if(connection) {
        app_avk_play_stop(connection);
        ret = 0;
    } else {
        printf("%s unknow connection index\r\n", __func__);
    }
    return ret;
}
int linkplay_bluetooth_avk_close(int index)
{
    tAPP_AVK_CONNECTION *connection = NULL;
    connection = app_avk_find_connection_by_index(index);
    if(connection)
         app_avk_close(connection->bda_connected);
}
int linkplay_bluetooth_avk_play_stop_all(void)
{
    app_avk_stop_all();
}

int linkplay_bluetooth_avk_close_all(void)
{
    app_avk_close_all();
}
int linkplay_bluetooth_set_abs_vol(UINT8 volume, int index)
{
     printf("linkplay_bluetooth_set_abs_vol %d \r\n", volume);
     return app_avk_set_abs_vol(volume,index);
}
int linkplay_bluetooth_sync_abs_vol(UINT8 volume, int index)
{
	app_avk_sync_vol(volume);
	return 0;
}
int linkplay_bluetooth_avk_prev_music(int index)
{
    int ret = -1;
    tAPP_AVK_CONNECTION *connection = NULL;
	
	printf("linkplay_bluetooth_avk_prev_music %d \n", index);

    connection = app_avk_find_connection_by_index(index);
    if(connection) {
        app_avk_play_previous_track(connection);
        ret = 0;
    } else {
        printf("Unknown choice:%d\n", index);
        ret = -1;
    }

    return ret;

}
int linkplay_bluetooth_avk_next_music(int index)
{
    int ret = -1;
    tAPP_AVK_CONNECTION *connection = NULL;
	
	printf("linkplay_bluetooth_avk_next_music %d \n", index);

    connection = app_avk_find_connection_by_index(index);
    if(connection) {
        app_avk_play_next_track(connection);
        ret = 0;
    } else {
        printf("Unknown choice:%d\n", index);
        ret = -1;
    }

    return ret;
}

int linkplay_bluetooth_avk_play_pause(int index)
{
    int ret = -1;
    tAPP_AVK_CONNECTION *connection = NULL;

    connection = app_avk_find_connection_by_index(index);
    if(connection) {
        app_avk_play_pause(connection);
        ret = 0;
    } else {
        printf("Unknown choice:%d\n", index);
        ret = -1;
    }

    return ret;
}
int linkplay_bluetooth_avk_play_start(int index)
{
    int ret = -1;
    tAPP_AVK_CONNECTION *connection = NULL;

    connection = app_avk_find_connection_by_index(index);
    if(connection) {
        app_avk_play_start(connection);
        ret = 0;
    } else {
        printf("Unknown choice:%d\n", index);
        ret = -1;
    }

    return ret;
}

int linkplay_bluetooth_avk_register_notification(int evt, int index)
{
    int ret = -1;
    tAPP_AVK_CONNECTION *connection = NULL;

    connection = app_avk_find_connection_by_index(index);
    if(connection) {
        app_avk_send_register_notification(evt, connection->rc_handle);
        ret = 0;
    } else {
        printf("Unknown choice:%d\n", index);
        ret = -1;
    }

    return ret;
}

int linkplay_bluetooth_get_link_status(int index)
{
    int ret = BT_LINK_DISCONNECTED;
    tAPP_AVK_CONNECTION *connection = NULL;

	if(index == -1)
	{
		if(avk_is_open_pending()) 
		{
	       ret = BT_LINK_CONNECTING;
	    }
	}else
	{
	    connection = app_avk_find_connection_by_index(index);
	    if(connection) {
	        if(avk_is_open_pending()) {
	            ret = BT_LINK_CONNECTING;
	        } else if(connection->is_open) {
	            ret = BT_LINK_CONNECTED;
	        } else {
	            ret = BT_LINK_DISCONNECTED;
	        }
	    }
	}

    return ret;
}
int linkplay_bluetooth_avk_send_get_element_att_cmd(int index)
{
    int ret = -1;
    tAPP_AVK_CONNECTION *connection = NULL;

    connection = app_avk_find_connection_by_index(index);
    if(connection) {
       // app_avk_send_get_element_attributes_cmd(connection->rc_handle);
       app_avk_rc_get_element_attr_command(connection->rc_handle);
        ret = 0;
    } else {
        printf("Unknown choice:%d\n", index);
        ret = -1;
    }

	return 0;
}

tAPP_XML_REM_DEVICE * linkplay_get_remote_device_by_addr(BD_ADDR bd_addr)
{
   tAPP_XML_REM_DEVICE * rem_device = NULL;

    xml_db_lock();
    if(app_read_xml_remote_devices() == -1) {
        xml_db_unlock();
        return NULL;
    }
	rem_device = app_xml_find_dev_db(app_xml_remote_devices_db,
                                   APP_NUM_ELEMENTS(app_xml_remote_devices_db),
                                  bd_addr);
    xml_db_unlock();
    return rem_device;
}
