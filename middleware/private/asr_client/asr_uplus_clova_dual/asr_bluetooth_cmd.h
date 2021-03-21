
#ifndef __ASR_BLUETOOTH_CMD_H__
#define __ASR_BLUETOOTH_CMD_H__

#define Format_Bluetooth_Cmd                                                                       \
    ("GNOTIFY=BLUETOOTH_DIRECTIVE:{"                                                               \
     "\"cmd_type\":\"bluetooth\","                                                                 \
     "\"cmd\":\"%s\","                                                                             \
     "\"params\":%s"                                                                               \
     "}")

#define Create_Bluetooth_Cmd(buff, cmd, parmas)                                                    \
    do {                                                                                           \
        asprintf(&(buff), Format_Bluetooth_Cmd, (cmd), (parmas));                                  \
    } while (0)

#define Format_Bluetooth_Scan_Param                                                                \
    ("{"                                                                                           \
     "\"timeout_ms\":%ld"                                                                          \
     "}")

#define Create_Bluetooth_Scan_Params(buff, timeout_ms)                                             \
    do {                                                                                           \
        asprintf(&(buff), Format_Bluetooth_Scan_Param, timeout_ms);                                \
    } while (0)

#define Format_Bluetooth_EnterDiscoverableMode_Param                                               \
    ("{"                                                                                           \
     "\"duration\":%ld"                                                                            \
     "}")

#define Create_Bluetooth_EnterDiscoverableMode_Params(buff, durationInSeconds)                     \
    do {                                                                                           \
        asprintf(&(buff), Format_Bluetooth_EnterDiscoverableMode_Param, durationInSeconds);        \
    } while (0)

#define Format_Bluetooth_Comm_Param                                                                \
    ("{"                                                                                           \
     "\"uuid\":\"%s\","                                                                            \
     "\"role\":\"%s\""                                                                             \
     "}")

#define Create_Bluetooth_Comm_Params(buff, uuid, role)                                             \
    do {                                                                                           \
        asprintf(&(buff), Format_Bluetooth_Comm_Param, uuid, role);                                \
    } while (0)

#define Format_Bluetooth_ConnectByProfile_Param                                                    \
    ("{"                                                                                           \
     "\"name\":\"%s\","                                                                            \
     "\"version\":\"%s\""                                                                          \
     "}")

#define Create_Bluetooth_ConnectByProfile_Params(buff, name, version)                              \
    do {                                                                                           \
        asprintf(&(buff), Format_Bluetooth_ConnectByProfile_Param, name, version);                 \
    } while (0)

#define Format_Bluetooth_Sound_Path_Param                                                          \
    ("{"                                                                                           \
     "\"uuid\":\"%s\","                                                                            \
     "\"flags\":%d"                                                                                \
     "}")

#define Create_Bluetooth_Sound_Path_Params(buff, uuid, flags)                                      \
    do {                                                                                           \
        asprintf(&(buff), Format_Bluetooth_Sound_Path_Param, uuid, flags);                         \
    } while (0)

#define AVS_BluetoothAPI ("/tmp/bt_manager")

#define Bluetooth_Cmd_Send(buf, buf_len, blocked)                                                  \
    do {                                                                                           \
        SocketClientReadWriteMsg(AVS_BluetoothAPI, buf, buf_len, NULL, NULL, blocked);             \
    } while (0)

#endif
