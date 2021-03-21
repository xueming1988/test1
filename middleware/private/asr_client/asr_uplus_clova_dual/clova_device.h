#ifndef __CLOVA_DEVICE__
#define __CLOVA_DEVICE__

/* namespace */
#define CLOVA_HEADER_NAMESPACE_DEVICE "Device"

/* name */
#define CLOVA_HEADER_NAME_DEVICE_STATE "DeviceState"

/* payload */
/* bluetooth */
#define CLOVA_PAYLOAD_NAME_BLUETOOTH "bluetooth"

#define CLOVA_PAYLOAD_NAME_ACTIONS "actions"
#define CLOVA_PAYLOAD_NAME_BT_CONNECT "BtConnect"
#define CLOVA_PAYLOAD_NAME_BT_DISCONNECT "BtDisconnect"
#define CLOVA_PAYLOAD_NAME_BT_START_PAIRING "BtStartPairing"
#define CLOVA_PAYLOAD_NAME_BT_STOP_PAIRING "BtStopPairing"
#define CLOVA_PAYLOAD_NAME_BT_DELETE "BtDelete"
#define CLOVA_PAYLOAD_NAME_BT_PLAY "BtPlay"

#define CLOVA_PAYLOAD_NAME_TURN_OFF "TurnOff"
#define CLOVA_PAYLOAD_NAME_TURN_ON "TurnOn"

#define CLOVA_PAYLOAD_NAME_BT_LIST "btlist"

#define CLOVA_PAYLOAD_NAME_SCAN_LIST "scanlist"

#define CLOVA_PAYLOAD_NAME_STATE "state"
#define CLOVA_PAYLOAD_NAME_ON "on"
#define CLOVA_PAYLOAD_NAME_OFF "off"

#define CLOVA_PAYLOAD_NAME_PAIRING "pairing"
#define CLOVA_PAYLOAD_NAME_SCANNING "scanning"
#define CLOVA_PAYLOAD_NAME_CONNECTING "connecting"

#define CLOVA_PAYLOAD_NAME_PLAYERINFO "playerInfo"
#define CLOVA_PAYLOAD_NAME_ARTISTNAME "artistName"
#define CLOVA_PAYLOAD_NAME_TRACKTITLE "trackTitle"
#define CLOVA_PAYLOAD_NAME_ALBUMTITLE "albumTitle"

#define CLOVA_PAYLOAD_NAME_STATE_PLAYING "playing"
#define CLOVA_PAYLOAD_NAME_STATE_PAUSED "paused"
#define CLOVA_PAYLOAD_NAME_STATE_STOPPED "stopped"

/* power */
#define CLOVA_PAYLOAD_NAME_POWER "power"

#define CLOVA_PAYLOAD_NAME_TURN_OFF "TurnOff"
#define CLOVA_PAYLOAD_NAME_TURN_ON "TurnOn"

#define CLOVA_PAYLOAD_VALUE_ACTIVE "active"
#define CLOVA_PAYLOAD_VALUE_IDLE "idle"

/* wifi */
#define CLOVA_PAYLOAD_NAME_WIFI "wifi"
#define CLOVA_PAYLOAD_NAME_NETWORKS "networks"
#define CLOVA_PAYLOAD_NAME_WIFI_NAME "name"
#define CLOVA_PAYLOAD_NAME_WIFI_CONNECTED "connected"

/* battery */
#define CLOVA_PAYLOAD_NAME_BATTERY "battery"
#define CLOVA_PAYLOAD_VALUE_CHARGING "charging"

/* volume */
#define CLOVA_PAYLOAD_NAME_VOLUME "volume"

#define CLOVA_PAYLOAD_NAME_DECREASE "Decrease"
#define CLOVA_PAYLOAD_NAME_INCREASE "Increase"
#define CLOVA_PAYLOAD_NAME_SET_VALUE "SetValue"

#define CLOVA_PAYLOAD_NAME_MIN "min"
#define CLOVA_PAYLOAD_NAME_MAX "max"
#define CLOVA_PAYLOAD_NAME_WARNING "warning"
#define CLOVA_PAYLOAD_NAME_VALUE "value"
#define CLOVA_PAYLOAD_NAME_MUTE "mute"

/* { DND */
#define CLOVA_PAYLOAD_NAME_DND "dnd"

#define CLOVA_PAYLOAD_DND_SCHEDULED "scheduled"
#define CLOVA_PAYLOAD_DND_SCHEDULED_STATE "state"
#define CLOVA_PAYLOAD_DND_SCHEDULED_START_TIME "startTime"
#define CLOVA_PAYLOAD_DND_SCHEDULED_END_TIME "endTime"

// "expiredAt": "2017-10-19T02:34:47+09:00"
#define CLOVA_PAYLOAD_DND_ACTION_EXPIREDAT "expiredAt"

/* } */

/* microphone */
#define CLOVA_PAYLOAD_NAME_MICROPHONE "microphone"

#endif
