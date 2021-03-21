/*****************************************************************************
**
**  Name:           app_manager_main.c
**
**  Description:    Bluetooth Manager menu application
**
**  Copyright (c) 2010-2014, Broadcom Corp., All Rights Reserved.
**  Broadcom Bluetooth Core. Proprietary and confidential.
**
*****************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>

#include "gki.h"
#include "uipc.h"

#include "bsa_api.h"

#include "app_xml_utils.h"

#include "app_disc.h"
#include "app_utils.h"
#include "app_dm.h"

#include "app_services.h"
#include "app_mgt.h"

#include "app_manager.h"

/* Menu items */
enum
{
    APP_MGR_MENU_ABORT_DISC         = 1,
    APP_MGR_MENU_DISCOVERY,
    APP_MGR_MENU_DISCOVERY_TEST,
    APP_MGR_MENU_BOND,
    APP_MGR_MENU_BOND_CANCEL,
    APP_MGR_MENU_UNPAIR,
    APP_MGR_MENU_SVC_DISCOVERY,
    APP_MGR_MENU_DI_DISCOVERY,
    APP_MGR_MENU_STOP_BT,
    APP_MGR_MENU_START_BT,
    APP_MGR_MENU_SP_ACCEPT,
    APP_MGR_MENU_SP_REFUSE,
    APP_MGR_MENU_ENTER_BLE_PASSKEY,
    APP_MGR_MENU_ACT_AS_KB,
    APP_MGR_MENU_READ_CONFIG,
    APP_MGR_MENU_READ_LOCAL_OOB,
    APP_MGR_MENU_ENTER_REMOTE_OOB,
    APP_MGR_MENU_SET_VISIBILTITY,
#if (defined(BLE_INCLUDED) && BLE_INCLUDED == TRUE)
    APP_MGR_MENU_SET_BLE_VISIBILTITY,
#endif
    APP_MGR_MENU_SET_AFH_CHANNELS,
    APP_MGR_MENU_CLASS2,
    APP_MGR_MENU_CLASS1_5,
    APP_MGR_MENU_DUAL_STACK,
    APP_MGR_MENU_LINK_POLICY,
    APP_MGR_MENU_ENTER_PIN_CODE,
    APP_MGR_MENU_REMOTE_NAME,
    APP_MGR_MENU_MONITOR_RSSI,
    APP_MGR_MENU_KILL_SERVER        = 96,
    APP_MGR_MENU_MGR_INIT           = 97,
    APP_MGR_MENU_MGR_CLOSE          = 98,
    APP_MGR_MENU_QUIT               = 99
};

/*
 * Extern variables
 */
extern BD_ADDR                 app_sec_db_addr;    /* BdAddr of peer device requesting SP */
extern tAPP_MGR_CB app_mgr_cb;
static tBSA_SEC_IO_CAP g_sp_caps = 0;
extern tAPP_XML_CONFIG         app_xml_config;

/*
 * Local functions
 */

/*******************************************************************************
 **
 ** Function         app_mgr_display_main_menu
 **
 ** Description      This is the main menu
 **
 ** Parameters       None
 **
 ** Returns          None
 **
 *******************************************************************************/
void app_mgr_display_main_menu(void)
{
    APP_INFO0("Bluetooth Application Manager Main menu:");
    APP_INFO1("\t%d => Abort Discovery", APP_MGR_MENU_ABORT_DISC);
    APP_INFO1("\t%d => Discovery", APP_MGR_MENU_DISCOVERY);
    APP_INFO1("\t%d => Discovery test", APP_MGR_MENU_DISCOVERY_TEST);
    APP_INFO1("\t%d => Bonding", APP_MGR_MENU_BOND);
    APP_INFO1("\t%d => Cancel Bonding", APP_MGR_MENU_BOND_CANCEL);
    APP_INFO1("\t%d => Remove device from security database", APP_MGR_MENU_UNPAIR);
    APP_INFO1("\t%d => Services Discovery (all services)", APP_MGR_MENU_SVC_DISCOVERY);
    APP_INFO1("\t%d => Device Id Discovery", APP_MGR_MENU_DI_DISCOVERY);
    APP_INFO1("\t%d => Stop Bluetooth", APP_MGR_MENU_STOP_BT);
    APP_INFO1("\t%d => Restart Bluetooth", APP_MGR_MENU_START_BT);
    APP_INFO1("\t%d => Accept Simple Pairing", APP_MGR_MENU_SP_ACCEPT);
    APP_INFO1("\t%d => Refuse Simple Pairing", APP_MGR_MENU_SP_REFUSE);
    APP_INFO1("\t%d => Enter BLE Passkey", APP_MGR_MENU_ENTER_BLE_PASSKEY);
    if (g_sp_caps == BSA_SEC_IO_CAP_IN)
        APP_INFO1("\t%d => Set Default Simple Pairing caps", APP_MGR_MENU_ACT_AS_KB);
    else
        APP_INFO1("\t%d => Act As HID Keyboard (SP passkey entry)", APP_MGR_MENU_ACT_AS_KB);

    APP_INFO1("\t%d => Read Device configuration", APP_MGR_MENU_READ_CONFIG);
    APP_INFO1("\t%d => Read Local Out Of Band data", APP_MGR_MENU_READ_LOCAL_OOB);
    APP_INFO1("\t%d => Enter remote Out Of Band data", APP_MGR_MENU_ENTER_REMOTE_OOB);
    APP_INFO1("\t%d => Set device visibility", APP_MGR_MENU_SET_VISIBILTITY);
#if (defined(BLE_INCLUDED) && BLE_INCLUDED == TRUE)
    APP_INFO1("\t%d => Set device BLE visibility", APP_MGR_MENU_SET_BLE_VISIBILTITY);
#endif
    APP_INFO1("\t%d => Set AFH Configuration", APP_MGR_MENU_SET_AFH_CHANNELS);
    APP_INFO1("\t%d => Set Tx Power Class2 (specific FW needed)", APP_MGR_MENU_CLASS2);
    APP_INFO1("\t%d => Set Tx Power Class1.5 (specific FW needed)", APP_MGR_MENU_CLASS1_5);
    APP_INFO1("\t%d => Change Dual Stack Mode (currently:%s)",
            APP_MGR_MENU_DUAL_STACK, app_mgr_get_dual_stack_mode_desc());
    APP_INFO1("\t%d => Set Link Policy", APP_MGR_MENU_LINK_POLICY);
    APP_INFO1("\t%d => Enter Passkey", APP_MGR_MENU_ENTER_PIN_CODE);
    APP_INFO1("\t%d => Get Remote Device Name", APP_MGR_MENU_REMOTE_NAME);
    APP_INFO1("\t%d => RSSI Measurement", APP_MGR_MENU_MONITOR_RSSI);
    APP_INFO1("\t%d => Kill BSA server", APP_MGR_MENU_KILL_SERVER);
    APP_INFO1("\t%d => Connect to BSA server", APP_MGR_MENU_MGR_INIT);
    APP_INFO1("\t%d => Disconnect from BSA server", APP_MGR_MENU_MGR_CLOSE);
    APP_INFO1("\t%d => Quit", APP_MGR_MENU_QUIT);
}

/*******************************************************************************
 **
 ** Function         app_mgr_mgt_callback
 **
 ** Description      This callback function is called in case of server
 **                  disconnection (e.g. server crashes)
 **
 ** Parameters
 **
 ** Returns          void
 **
 *******************************************************************************/
static BOOLEAN app_mgr_mgt_callback(tBSA_MGT_EVT event, tBSA_MGT_MSG *p_data)
{
    switch(event)
    {
    case BSA_MGT_STATUS_EVT:
        printf("BSA_MGT_STATUS_EVT");
        if (p_data->status.enable)
        {
            APP_DEBUG0("Bluetooth restarted => re-initialize the application");
            //app_mgr_config();
        }
        break;

    case BSA_MGT_DISCONNECT_EVT:
        printf("BSA_MGT_DISCONNECT_EVT reason:%d", p_data->disconnect.reason);
        //exit(-1);
        break;

    default:
        break;
    }
    return FALSE;
}
#if 0
/*******************************************************************************
 **
 ** Function         main
 **
 ** Description      This is the main function
 **
 ** Parameters
 **
 ** Returns          void
 **
 *******************************************************************************/
int manager_main(int argc, char **argv)
{
    int choice;
    int i;
    unsigned int afh_ch1,afh_ch2,passkey;
    BOOLEAN no_init = FALSE;
    int mode;
    BOOLEAN discoverable, connectable;
    tBSA_SEC_PIN_CODE_REPLY pin_code_reply;

    BD_ADDR bd_addr;
    int uarr[16];
    char szInput[64];
    int  x = 0;
    int length = 0;
    tBSA_DM_LP_MASK policy_mask = 0;
    BOOLEAN set = FALSE;
    int i_set = 0;

    /* Check the CLI parameters */
    for (i = 1; i < argc; i++)
    {
        char *arg = argv[i];
        if (*arg != '-')
        {
            APP_ERROR1("Unsupported parameter #%d : %s", i+1, argv[i]);
            exit(-1);
        }
        /* Bypass the first '-' char */
        arg++;
        /* check if the second char is also a '-'char */
        if (*arg == '-') arg++;
        switch (*arg)
        {
        case 'n': /* No init */
            no_init = TRUE;
            break;
        default:
            break;
        }
    }


    /* Init App manager */
    if (!no_init)
    {
        app_mgt_init();
        if (app_mgt_open(NULL, app_mgr_mgt_callback) < 0)
        {
            APP_ERROR0("Unable to connect to server");
            return -1;
        }

        /* Init XML state machine */
        app_xml_init();

        if (app_mgr_config())
        {
            APP_ERROR0("Couldn't configure successfully, exiting");
            return -1;
        }
        g_sp_caps = app_xml_config.io_cap;
    }

    /* Display FW versions */
    app_mgr_read_version();

    /* Get the current Stack mode */
    mode = app_dm_get_dual_stack_mode();
    if (mode < 0)
    {
        APP_ERROR0("app_dm_get_dual_stack_mode failed");
        return -1;
    }
    else
    {
        /* Save the current DualStack mode */
        app_mgr_cb.dual_stack_mode = mode;
        APP_INFO1("Current DualStack mode:%s", app_mgr_get_dual_stack_mode_desc());
    }

    do
    {
        app_mgr_display_main_menu();

        choice = app_get_choice("Select action");

        switch(choice)
        {
        case APP_MGR_MENU_ABORT_DISC:
            app_disc_abort();
            break;

        case APP_MGR_MENU_DISCOVERY:
            /* Example to perform Device discovery */
            app_disc_start_regular(NULL);
            break;

        case APP_MGR_MENU_DISCOVERY_TEST:
            /* Example to perform specific discovery */
            app_mgr_discovery_test();
            break;

        case APP_MGR_MENU_BOND:
            /* Example to Bound  */
            app_mgr_sec_bond();
            break;

        case APP_MGR_MENU_BOND_CANCEL:
            /* Example to cancel Bound  */
            app_mgr_sec_bond_cancel();
            break;

        case APP_MGR_MENU_UNPAIR:
            /* Unpair device */
            app_mgr_sec_unpair();
            break;

        case APP_MGR_MENU_SVC_DISCOVERY:
            /* Example to perform Device Services discovery */
            app_disc_start_services(BSA_ALL_SERVICE_MASK);
            break;

        case APP_MGR_MENU_DI_DISCOVERY:
            /* Example to perform Device Id discovery */
            /* Note that the synchronization (wait discovery complete) must be adapted to the application */
            app_mgr_di_discovery();
            break;

        case APP_MGR_MENU_STOP_BT:
            /* Example of function to Stop the Local Bluetooth device */
            app_mgr_set_bt_config(FALSE);
            break;

        case APP_MGR_MENU_START_BT:
            /* Example of function to Restart the Local Bluetooth device */
            app_mgr_set_bt_config(TRUE);
            break;

        case APP_MGR_MENU_SP_ACCEPT:
            /* Example of function to Accept Simple Pairing */
            app_mgr_sp_cfm_reply(TRUE, app_sec_db_addr);
            break;

        case APP_MGR_MENU_SP_REFUSE:
            /* Example of function to Refuse Simple Pairing */
            app_mgr_sp_cfm_reply(FALSE, app_sec_db_addr);
            break;

        case APP_MGR_MENU_ENTER_BLE_PASSKEY:
            BSA_SecPinCodeReplyInit(&pin_code_reply);

            printf("Enter Passkey: ");
            scanf("%u", &pin_code_reply.ble_passkey);

            APP_INFO1("BLE pass key=%d",pin_code_reply.ble_passkey);
            pin_code_reply.is_ble = TRUE;
            pin_code_reply.ble_accept = TRUE;
            app_mgr_send_pincode(pin_code_reply);
            break;

        case APP_MGR_MENU_ACT_AS_KB:
            if (g_sp_caps == BSA_SEC_IO_CAP_IN)
                app_mgr_sec_set_sp_cap(g_sp_caps = app_xml_config.io_cap);
            else
                app_mgr_sec_set_sp_cap(g_sp_caps = BSA_SEC_IO_CAP_IN);
            break;

        case APP_MGR_MENU_READ_CONFIG:
            /* Read and print the device current configuration */
            app_mgr_get_bt_config();
            break;

        case APP_MGR_MENU_READ_LOCAL_OOB:
            /* Read local oob data */
            app_mgr_read_oob_data();
            break;

        case APP_MGR_MENU_ENTER_REMOTE_OOB:
            /* Read local oob data */
            app_mgr_set_remote_oob();
            break;

        case APP_MGR_MENU_SET_VISIBILTITY:
             /* Choose discoverability*/
             choice = app_get_choice("Set BR/EDR Discoverability (0=Not Discoverable, 1=Discoverable)");
             if(choice > 0)
             {
                 discoverable = TRUE;
                 APP_INFO0("    Device will be discoverable");
             }
             else
             {
                 discoverable = FALSE;
                 APP_INFO0("    Device will not be discoverable");
             }

             /* Choose connectability */
             choice = app_get_choice("Set BR/EDR Connectability (0=Not Connectable, 1=Connectable)");
             if(choice > 0)
             {
                 connectable = TRUE;
                 APP_INFO0("    Device will be connectable");
             }
             else
             {
                 connectable = FALSE;
                 APP_INFO0("    Device will not be connectable");
             }

             app_dm_set_visibility(discoverable, connectable);
             break;

#if (defined(BLE_INCLUDED) && BLE_INCLUDED == TRUE)
        case APP_MGR_MENU_SET_BLE_VISIBILTITY:
            /* Choose discoverability*/
            choice = app_get_choice("Set BLE Discoverability (0=Not Discoverable, 1=Discoverable)");
            if(choice > 0)
            {
                discoverable = TRUE;
                APP_INFO0("    Device will be discoverable");
            }
            else
            {
                discoverable = FALSE;
                APP_INFO0("    Device will not be discoverable");
            }

            /* Choose connectability */
            choice = app_get_choice("Set BLE Connectability (0=Not Connectable, 1=Connectable)");
            if(choice > 0)
            {
                connectable = TRUE;
                APP_INFO0("    Device will be connectable");
            }
            else
            {
                connectable = FALSE;
                APP_INFO0("    Device will not be connectable");
            }

            app_dm_set_ble_visibility(discoverable, connectable);
            break;
#endif

        case APP_MGR_MENU_SET_AFH_CHANNELS:
            /* Choose first channel*/
            APP_INFO0("    Enter First AFH CHannel (79 = complete channel map):");
            afh_ch1 = app_get_choice("");
            APP_INFO0("    Enter Last AFH CHannel:");
            afh_ch2 = app_get_choice("");
            app_dm_set_channel_map(afh_ch1,afh_ch2);
            break;

        case APP_MGR_MENU_CLASS2:
            app_dm_set_tx_class(BSA_DM_TX_POWER_CLASS_2);
            break;

        case APP_MGR_MENU_CLASS1_5:
            app_dm_set_tx_class(BSA_DM_TX_POWER_CLASS_1_5);
            break;

        case APP_MGR_MENU_DUAL_STACK:
            app_mgr_change_dual_stack_mode();
            break;

        case APP_MGR_MENU_LINK_POLICY:
        {
            APP_INFO0("Enter BDA of Peer device: AABBCCDDEEFF");
            memset(szInput, 0, sizeof(szInput));
            length = app_get_string("Enter BDA: ", szInput, sizeof(szInput));
            sscanf (szInput, "%02x%02x%02x%02x%02x%02x",
                           &uarr[0], &uarr[1], &uarr[2], &uarr[3], &uarr[4], &uarr[5]);

            for(x=0; x < 6; x++)
                bd_addr[x] = (UINT8) uarr[x];

            APP_INFO0("    Enter policy: 1 = MASTER_SLAVE_SWITCH, 2 = HOLD_MODE, 3 = SNIFF, 4 = PARK");
            int i_policy_mask = app_get_choice("");
            switch(i_policy_mask)
            {
                case 1:
                    policy_mask = BSA_DM_LP_SWITCH;
                    break;
                case 2:
                    policy_mask = BSA_DM_LP_HOLD;
                    break;
                case 3:
                    policy_mask = BSA_DM_LP_SNIFF;
                    break;
                case 4:
                    policy_mask = BSA_DM_LP_PARK;
                    break;
            }

            APP_INFO0("    Enter type of change, 1 = set, 2 = unset");
            i_set = app_get_choice("");
            if(i_set == 1)
                set = TRUE;

            app_mgr_set_link_policy(bd_addr, policy_mask, set);

            break;
        }

        case APP_MGR_MENU_ENTER_PIN_CODE:
            printf("Enter Passkey: ");
            scanf("%u", &passkey);
            app_mgr_send_passkey(passkey);
            break;

        case APP_MGR_MENU_REMOTE_NAME:
        {
            APP_INFO0("Enter BDA of remote device: AABBCCDDEEFF");
            memset(szInput, 0, sizeof(szInput));
            length = app_get_string("Enter BDA: ", szInput, sizeof(szInput));

            if (!length || (sscanf (szInput, "%02x%02x%02x%02x%02x%02x",
                                &uarr[0], &uarr[1], &uarr[2], &uarr[3], &uarr[4], &uarr[5]) != 6))
            {
                APP_ERROR0("BD address not entered correctly");
                break;
            }

            for(x=0; x < 6; x++)
                bd_addr[x] = (UINT8) uarr[x];

            app_disc_read_remote_device_name(bd_addr,BSA_TRANSPORT_BR_EDR);
        }
        break;

        case APP_MGR_MENU_MONITOR_RSSI:
            choice = app_get_choice("Enable/Disable RSSI Monitoring (1 = enable, 0 = disable)");
            if(choice > 0)
            {
                choice = app_get_choice("Enter RSSI measurement period in seconds");
                APP_INFO0("Enabling RSSI Monitoring");
                app_dm_monitor_rssi(TRUE,choice);
            }
            else
            {
                APP_INFO0("Disabling RSSI Monitoring");
                app_dm_monitor_rssi(FALSE,0);

            }
            break;

        case APP_MGR_MENU_KILL_SERVER:
            app_mgt_kill_server();
            break;

        case APP_MGR_MENU_MGR_INIT:
            if (app_mgt_open(NULL, app_mgr_mgt_callback) < 0)
            {
                APP_ERROR0("Unable to connect to server");
                break;
            }

            /* Init XML state machine */
            app_xml_init();

            if (app_mgr_config())
            {
                APP_ERROR0("app_mgr_config failed");
            }
            break;

        case APP_MGR_MENU_MGR_CLOSE:
            app_mgt_close();
            break;

        case APP_MGR_MENU_QUIT:
            APP_INFO0("Bye Bye");
            break;

        default:
            APP_ERROR1("Unknown choice:%d", choice);
            break;
        }
    } while (choice != APP_MGR_MENU_QUIT);      /* While user don't exit application */

    app_mgt_close();

    return 0;
}
#endif
int app_manager_init(const char *bt_name,UINT8 *btaddr, tAPP_MGT_CUSTOM_CBACK *mgt_custom_callback)
{
    int choice;
    int i;
    unsigned int afh_ch1,afh_ch2,passkey;
    BOOLEAN no_init = FALSE;
    int mode;
    BOOLEAN discoverable, connectable;
    tBSA_SEC_PIN_CODE_REPLY pin_code_reply;

    BD_ADDR bd_addr;
    int uarr[16];
    char szInput[64];
    int  x = 0;
    int length = 0;
    tBSA_DM_LP_MASK policy_mask = 0;
    BOOLEAN set = FALSE;
    int i_set = 0;

    /* Init App manager */
    if (!no_init)
    {
        app_mgt_init();
        if (app_mgt_open("/tmp/", mgt_custom_callback) < 0)
        {
            APP_ERROR0("Unable to connect to server");
            return -1;
        }

        /* Init XML state machine */
        app_xml_init();

        if (app_mgr_config(bt_name,btaddr))
        {
            APP_ERROR0("Couldn't configure successfully, exiting");
            return -1;
        }
        g_sp_caps = app_xml_config.io_cap;
    }

    /* Display FW versions */
    app_mgr_read_version();

    /* Get the current Stack mode */
    mode = app_dm_get_dual_stack_mode();
    if (mode < 0)
    {
        APP_ERROR0("app_dm_get_dual_stack_mode failed");
        return -1;
    }
    else
    {
        /* Save the current DualStack mode */
        app_mgr_cb.dual_stack_mode = mode;
        APP_INFO1("Current DualStack mode:%s", app_mgr_get_dual_stack_mode_desc());
    }

    return 0;
}

int app_manager_deinit(void)
{
    app_mgt_close();

    return 0;
}

int linkplay_bluetooth_sec_bond(BD_ADDR *bd_addr)
{
    tBSA_SEC_BOND sec_bond;
	tBSA_STATUS status;

    BSA_SecBondInit(&sec_bond);
    bdcpy(sec_bond.bd_addr, *bd_addr);
    status = BSA_SecBond(&sec_bond);
    if (status != BSA_SUCCESS)
    {
        APP_ERROR1("BSA_SecBond failed:%d", status);
        return -1;
    }

    return 0;
}
int linkplay_bluetooth_unpair_remote_device(BD_ADDR *addr)
{
	int index = 0;
	int status = 0;
	tBSA_SEC_REMOVE_DEV sec_remove;
	
    xml_db_lock();	
	status = app_mgr_read_remote_devices();
	if (status == -1) {
		printf("app_mgr_read_remote_devices failed!\n");
		xml_db_unlock();
		return -1;
	}

	for (index = 0; index < APP_NUM_ELEMENTS(app_xml_remote_devices_db); index++) {
		if ((app_xml_remote_devices_db[index].in_use != FALSE) &&
				(bdcmp(app_xml_remote_devices_db[index].bd_addr, *addr) == 0))
		{
			printf("Delete name: %s\n", app_xml_remote_devices_db[index].name);
			/* Remove the device from Security database (BSA Server) */
			BSA_SecRemoveDeviceInit(&sec_remove);
			bdcpy(sec_remove.bd_addr, app_xml_remote_devices_db[index].bd_addr);
			status = BSA_SecRemoveDevice(&sec_remove);

			memset(&app_xml_remote_devices_db[index], 0, sizeof(tAPP_XML_REM_DEVICE));
			break;
		}
	}
	for (index = index; index < (APP_NUM_ELEMENTS(app_xml_remote_devices_db) - 1); index++) {
		if (app_xml_remote_devices_db[index + 1].in_use != FALSE) {
			memcpy(&app_xml_remote_devices_db[index], &app_xml_remote_devices_db[index + 1], sizeof(tAPP_XML_REM_DEVICE));
		} else {
			memset(&app_xml_remote_devices_db[index], 0, sizeof(tAPP_XML_REM_DEVICE));
		}
	}

	status = app_mgr_write_remote_devices();
	if (status < 0)
	{
		APP_ERROR1("app_mgr_write_remote_devices failed:%d", status);
		xml_db_unlock();
		return -1;
	}
	
	xml_db_unlock();

	return 0;
}
int linkplay_bluetooth_set_local_name(char *name)
{
	return  app_mgr_set_local_name(name);
}

int linkplay_bluetooth_get_addr(char *addr)
{
    return app_mgr_get_bt_addr(addr);
}

int linkplay_bluetooth_pincode_reply(BD_ADDR bd_addr, unsigned char *pin_code, unsigned char pin_len)
{
    int ret = -1;
    tBSA_SEC_PIN_CODE_REPLY pin_code_reply = {0};

    APP_INFO1("PinCodeReply addr %02x:%02x:%02x:%02x:%02x:%02x, pin_code %s, len %d", 
              bd_addr[0], bd_addr[1], bd_addr[2], bd_addr[3], bd_addr[4], bd_addr[5],
              pin_code?pin_code:"", pin_len);
    if(pin_code && pin_len < PIN_CODE_LEN) {
        BSA_SecPinCodeReplyInit(&pin_code_reply);
        bdcpy(pin_code_reply.bd_addr, bd_addr);
        pin_code_reply.pin_len = pin_len;
        strncpy((char *)pin_code_reply.pin_code, (char *)pin_code, pin_len);
        /* note that this code will not work if pin_len = 16 */
        pin_code_reply.pin_code[PIN_CODE_LEN - 1] = '\0';
        ret = BSA_SecPinCodeReply(&pin_code_reply);
    }
    return ret;
}

