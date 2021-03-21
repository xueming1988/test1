#ifndef __ALEXA_BLUETOOTH_INTERFACE_H__
#define __ALEXA_BLUETOOTH_INTERFACE_H__

#define Format_Internal_cmd_type ("cmd_type")
#define Format_Internal_cmd ("cmd")
#define Format_Internal_params ("params")

#define Cmd_Scan ("scan")
#define Cmd_Pair ("pair")
#define Cmd_UnPair ("unpair")
#define Cmd_ConnectById ("connectbyid")
#define Cmd_ConnectByProfile ("connectbyprofile")
#define Cmd_Disconnect ("disconnect")
#define Cmd_EnterDiscoverableMode ("enable_discoverable")
#define Cmd_ExitDiscoverableMode ("disable_discoverable")
#define Cmd_MediaControl ("media_control")
#define Cmd_getpaired_list ("getpaired_list")

#define Format_Internal_BluetoothAPI_Directive_Cmd                                                 \
    ("GNOTIFY=BLUETOOTH_DIRECTIVE:{"                                                               \
     "\"cmd_type\":\"bluetooth\","                                                                 \
     "\"cmd\":\"%s\","                                                                             \
     "\"params\":%s"                                                                               \
     "}")

#define Create_Internal_BluetoothAPI_Directive_Cmd(buff, cmd, parmas)                              \
    do {                                                                                           \
        asprintf(&(buff), Format_Internal_BluetoothAPI_Directive_Cmd, (cmd), (parmas));            \
    } while (0)

#define Format_Internal_BluetoothAPI_Directive_Scan_param                                          \
    ("{"                                                                                           \
     "\"timeout_ms\":%ld"                                                                          \
     "}")

#define Create_Internal_BluetoothAPI_Directive_Scan_Params(buff, timeout_ms)                       \
    do {                                                                                           \
        asprintf(&(buff), Format_Internal_BluetoothAPI_Directive_Scan_param, timeout_ms);          \
    } while (0)

#define Format_Internal_BluetoothAPI_Directive_EnterDiscoverableMode_param                         \
    ("{"                                                                                           \
     "\"duration\":%ld"                                                                            \
     "}")

#define Create_Internal_BluetoothAPI_Directive_EnterDiscoverableMode_Params(buff,                  \
                                                                            durationInSeconds)     \
    do {                                                                                           \
        asprintf(&(buff), Format_Internal_BluetoothAPI_Directive_EnterDiscoverableMode_param,      \
                 durationInSeconds);                                                               \
    } while (0)

#define Format_Internal_BluetoothAPI_Directive_PAIR_param                                          \
    ("{"                                                                                           \
     "\"uuid\":\"%s\""                                                                             \
     "}")

#define Create_Internal_BluetoothAPI_Directive_PAIR_Params(buff, uuid)                             \
    do {                                                                                           \
        asprintf(&(buff), Format_Internal_BluetoothAPI_Directive_PAIR_param, uuid);                \
    } while (0)

#define Format_Internal_BluetoothAPI_Directive_UNPAIR_param                                        \
    (Format_Internal_BluetoothAPI_Directive_PAIR_param)

#define Create_Internal_BluetoothAPI_Directive_UNPAIR_Params(buff, uuid)                           \
    do {                                                                                           \
        asprintf(&(buff), Format_Internal_BluetoothAPI_Directive_UNPAIR_param, uuid);              \
    } while (0)

#define Format_Internal_BluetoothAPI_Directive_ConnectById_param                                   \
    ("{"                                                                                           \
     "\"uuid\":\"%s\","                                                                            \
     "\"role\":\"%s\""                                                                             \
     "}")

#define Create_Internal_BluetoothAPI_Directive_ConnectById_Params(buff, uuid, role)                \
    do {                                                                                           \
        asprintf(&(buff), Format_Internal_BluetoothAPI_Directive_ConnectById_param, uuid, role);   \
    } while (0)

#define Format_Internal_BluetoothAPI_Directive_ConnectByProfile_param                              \
    ("{"                                                                                           \
     "\"name\":\"%s\","                                                                            \
     "\"version\":\"%s\""                                                                          \
     "}")

#define Create_Internal_BluetoothAPI_Directive_ConnectByProfile_Params(buff, name, version)        \
    do {                                                                                           \
        asprintf(&(buff), Format_Internal_BluetoothAPI_Directive_ConnectByProfile_param, name,     \
                 version);                                                                         \
    } while (0)

#define Format_Internal_BluetoothAPI_Directive_Disconnect_param                                    \
    (Format_Internal_BluetoothAPI_Directive_PAIR_param)

#define Create_Internal_BluetoothAPI_Directive_Disconnect_Params(buff, uuid)                       \
    do {                                                                                           \
        asprintf(&(buff), Format_Internal_BluetoothAPI_Directive_Disconnect_param, uuid);          \
    } while (0)

#define Format_Internal_BluetoothAPI_Directive_MediaControl_param                                  \
    ("{"                                                                                           \
     "\"action\":\"%s\""                                                                           \
     "}")

#define Create_Internal_BluetoothAPI_Directive_MediaControl_Params(buff, MediaControl_action)      \
    do {                                                                                           \
        asprintf(&(buff), Format_Internal_BluetoothAPI_Directive_MediaControl_param,               \
                 MediaControl_action);                                                             \
    } while (0)

#define AVS_BluetoothAPI ("/tmp/bt_manager")

#define BluetoothAPI_Directive_Send(buf, buf_len, blocked)                                         \
    do {                                                                                           \
        SocketClientReadWriteMsg(AVS_BluetoothAPI, buf, buf_len, NULL, NULL, blocked);             \
    } while (0)

#endif
