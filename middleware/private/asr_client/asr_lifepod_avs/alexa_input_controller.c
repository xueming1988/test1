/*
*created by richard.huang 2018-07-27
*for Input Controller (v3.0)
*amazone reference document :
*https://developer.amazon.com/zh/docs/alexa-voice-service/inputcontroller.html
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/un.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/wait.h>
#include <semaphore.h>

#include "json.h"
#include "alexa_debug.h"
#include "alexa_json_common.h"
#include "alexa_input_controller.h"

static char *g_input_mode = NULL;
static pthread_mutex_t g_state_lock = PTHREAD_MUTEX_INITIALIZER;

int avs_input_controller_cmd_notify(char *cmdstr)
{
    int ret = -1;
    char *cmd_buf = NULL;

    /*INPUTMODESWITCH*/
    asprintf(&cmd_buf, "GNOTIFY=INPUTMODESWITCH+%s", cmdstr);
    if (cmd_buf) {
        if (0 <= SocketClientReadWriteMsg("/tmp/RequestIOGuard", cmd_buf, strlen(cmd_buf), NULL,
                                          NULL, 0)) {
            ret = 0;
        }
    }

    free(cmd_buf);

    return ret;
}

/*
"000" "setPlayerCmd:switchmode_by_mcu:wifi"
*/
static int avs_input_controller_cmd_swith_to_wifi(void)
{
    SocketClientReadWriteMsg("/tmp/Requesta01controller", "setPlayerCmd:switchmode:wifi",
                             strlen("setPlayerCmd:switchmode:wifi"), NULL, NULL, 0);
    return 0;
    // return avs_input_controller_cmd_notify("MCU+PLM-000");
}

/*
"004" "setPlayerCmd:switchmode_by_mcu:udisk"
*/
static int avs_input_controller_cmd_swith_to_udisk(void)
{
    SocketClientReadWriteMsg("/tmp/Requesta01controller", "setPlayerCmd:switchmode:udisk",
                             strlen("setPlayerCmd:switchmode:udisk"), NULL, NULL, 0);
    return 0;
    // return avs_input_controller_cmd_notify("MCU+PLM-004");
}

/*
"005" "setPlayerCmd:switchmode_by_mcu:line-in"
*/
static int avs_input_controller_cmd_swith_to_aux(void)
{
    SocketClientReadWriteMsg("/tmp/Requesta01controller", "setPlayerCmd:switchmode:line-in",
                             strlen("setPlayerCmd:switchmode:line-in"), NULL, NULL, 0);
    return 0;
    // return avs_input_controller_cmd_notify("MCU+PLM-005");
}

/*
"006" "setPlayerCmd:switchmode_by_mcu:bluetooth"
*/
static int avs_input_controller_cmd_swith_to_bluetooth(void)
{
    SocketClientReadWriteMsg("/tmp/Requesta01controller", "setPlayerCmd:switchmode:bluetooth",
                             strlen("setPlayerCmd:switchmode:bluetooth"), NULL, NULL, 0);
    return 0;
    // return avs_input_controller_cmd_notify("MCU+PLM-006");
}

/*
"007" "setPlayerCmd:switchmode_by_mcu:mirror"
*/
static int avs_input_controller_cmd_swith_to_mirror(void)
{
    SocketClientReadWriteMsg("/tmp/Requesta01controller", "setPlayerCmd:switchmode:mirror",
                             strlen("setPlayerCmd:switchmode:mirror"), NULL, NULL, 0);
    return 0;
    // return avs_input_controller_cmd_notify("MCU+PLM-007");
}

/*
"008" "setPlayerCmd:switchmode_by_mcu:optical"
*/
static int avs_input_controller_cmd_swith_to_optical(void)
{
    SocketClientReadWriteMsg("/tmp/Requesta01controller", "setPlayerCmd:switchmode:optical",
                             strlen("setPlayerCmd:switchmode:optical"), NULL, NULL, 0);
    return 0;
    // return avs_input_controller_cmd_notify("MCU+PLM-008");
}

/*
"009" "setPlayerCmd:switchmode_by_mcu:RCA"
*/
static int avs_input_controller_cmd_swith_to_RCA(void)
{
    SocketClientReadWriteMsg("/tmp/Requesta01controller", "setPlayerCmd:switchmode:RCA",
                             strlen("setPlayerCmd:switchmode:RCA"), NULL, NULL, 0);
    return 0;
    // return avs_input_controller_cmd_notify("MCU+PLM-009");
}

/*
"010" "setPlayerCmd:switchmode_by_mcu:co-axial"
*/
static int avs_input_controller_cmd_swith_to_co_axial(void)
{
    SocketClientReadWriteMsg("/tmp/Requesta01controller", "setPlayerCmd:switchmode:co-axial",
                             strlen("setPlayerCmd:switchmode:co-axial"), NULL, NULL, 0);
    return 0;
    // return avs_input_controller_cmd_notify("MCU+PLM-010");
}

/*
"011" "setPlayerCmd:switchmode_by_mcu:FM"
*/
static int avs_input_controller_cmd_swith_to_fm(void)
{
    SocketClientReadWriteMsg("/tmp/Requesta01controller", "setPlayerCmd:switchmode:FM",
                             strlen("setPlayerCmd:switchmode:FM"), NULL, NULL, 0);
    return 0;
    // return avs_input_controller_cmd_notify("MCU+PLM-011");
}

/*
"012" "setPlayerCmd:switchmode_by_mcu:line-in2",
*/
static int avs_input_controller_cmd_swith_to_linein2(void)
{
    SocketClientReadWriteMsg("/tmp/Requesta01controller", "setPlayerCmd:switchmode:line-in2",
                             strlen("setPlayerCmd:switchmode:line-in2"), NULL, NULL, 0);
    return 0;
    // return avs_input_controller_cmd_notify("MCU+PLM-012");
}

/*
"013" "setPlayerCmd:switchmode_by_mcu:XLR"
*/
static int avs_input_controller_cmd_swith_to_XLR(void)
{
    SocketClientReadWriteMsg("/tmp/Requesta01controller", "setPlayerCmd:switchmode:XLR",
                             strlen("setPlayerCmd:switchmode:XLR"), NULL, NULL, 0);
    return 0;
    // return avs_input_controller_cmd_notify("MCU+PLM-013");
}

/*
 "014" "setPlayerCmd:switchmode_by_mcu:TFcard",
*/
static int avs_input_controller_cmd_swith_to_TFcard(void)
{
    SocketClientReadWriteMsg("/tmp/Requesta01controller", "setPlayerCmd:switchmode:TFcard",
                             strlen("setPlayerCmd:switchmode:TFcard"), NULL, NULL, 0);
    return 0;
    // return avs_input_controller_cmd_notify("MCU+PLM-014");
}

/*
 "015" "setPlayerCmd:switchmode_by_mcu:HDMI"
*/
static int avs_input_controller_cmd_swith_to_HDMI(void)
{
    SocketClientReadWriteMsg("/tmp/Requesta01controller", "setPlayerCmd:switchmode:HDMI",
                             strlen("setPlayerCmd:switchmode:HDMI"), NULL, NULL, 0);
    return 0;
    // return avs_input_controller_cmd_notify("MCU+PLM-015");
}

static int avs_input_controller_switch(char *input_name)
{
    int ret = -1;

    if (StrEq(input_name, "WiFi")) {
        avs_input_controller_cmd_swith_to_wifi();
    } else if (StrEq(input_name, "UDisk")) {
        avs_input_controller_cmd_swith_to_udisk();
    } else if (StrEq(input_name, "AUX")) {
        avs_input_controller_cmd_swith_to_aux();
    } else if (StrEq(input_name, "Bluetooth")) {
        avs_input_controller_cmd_swith_to_bluetooth();
    } else if (StrEq(input_name, "Mirror")) {
        avs_input_controller_cmd_swith_to_mirror();
    } else if (StrEq(input_name, "Optical")) {
        avs_input_controller_cmd_swith_to_optical();
    } else if (StrEq(input_name, "RCA")) {
        avs_input_controller_cmd_swith_to_RCA();
    } else if (StrEq(input_name, "Coaxial")) {
        avs_input_controller_cmd_swith_to_co_axial();
    } else if (StrEq(input_name, "FM")) {
        avs_input_controller_cmd_swith_to_fm();
    } else if (StrEq(input_name, "AUX2")) {
        avs_input_controller_cmd_swith_to_linein2();
    } else if (StrEq(input_name, "XLR")) {
        avs_input_controller_cmd_swith_to_XLR();
    } else if (StrEq(input_name, "USB")) {
        avs_input_controller_cmd_swith_to_TFcard();
    } else if (StrEq(input_name, "HDMI")) {
        avs_input_controller_cmd_swith_to_HDMI();
    } else {
        ALEXA_PRINT(ALEXA_DEBUG_TRACE, "Error, input controller received unknow mode: %s\n",
                    input_name ? input_name : "unknown");
    }

    return ret;
}

/*-----
AVS InputController directive format:
{
  “directive”: {
      “header”: {
          “namespace”: “Alexa.InputController”,
          “name”: “SelectInput”,
          "messageId": "{{STRING}}",
          "dialogRequestId": "{{STRING}}"
      },
      “payload”: {
          "input": "{{STRING}}"
      }
  }
}
-----*/

int avs_input_controller_parse_directive(json_object *js_data)
{
    int ret = -1;

    if (js_data) {
        json_object *json_header = json_object_object_get(js_data, KEY_HEADER);
        json_object *json_payload = json_object_object_get(js_data, KEY_PAYLOAD);
        if (json_header && json_payload) {
            char *name_sapce = JSON_GET_STRING_FROM_OBJECT(json_header, KEY_NAMESPACE);
            char *name = JSON_GET_STRING_FROM_OBJECT(json_header, KEY_NAME);
            char *input_name = JSON_GET_STRING_FROM_OBJECT(json_payload, KEY_INPUT);
            if (name_sapce && name && input_name && StrEq(name_sapce, NAMESPACE_INPUTCONTROLLER) &&
                StrEq(name, NAME_SELECTINPUT)) {
                ret = 0;
                pthread_mutex_lock(&g_state_lock);
                if (g_input_mode) {
                    free(g_input_mode);
                    g_input_mode = NULL;
                }
                if (input_name) {
                    g_input_mode = strdup(input_name);
                }
                pthread_mutex_unlock(&g_state_lock);
            }
        }
    }

    if (ret != 0) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "prase input controller directive message error\n");
    }

    return ret;
}

void avs_input_controller_do_cmd(void)
{
    pthread_mutex_lock(&g_state_lock);
    if (g_input_mode) {
        avs_input_controller_switch(g_input_mode);
        free(g_input_mode);
        g_input_mode = NULL;
    }
    pthread_mutex_unlock(&g_state_lock);
}
