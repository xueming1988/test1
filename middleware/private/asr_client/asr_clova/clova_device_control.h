#ifndef __CLOVA_DEVICE_CONTROL__
#define __CLOVA_DEVICE_CONTROL__

/* namespace */
#define CLOVA_HEADER_NAMESPACE_DEVICE_CONTROL "DeviceControl"

/* Directive Part */
/* Name */
#define CLOVA_HEADER_NAME_BT_CONNECT "BtConnect"
#define CLOVA_HEADER_NAME_BT_CONNECT_BY_PIN_CODE "BtConnectByPINCode"
#define CLOVA_HEADER_NAME_BT_DISCONNECT "BtDisconnect"
#define CLOVA_HEADER_NAME_BT_START_PAIRING "BtStartPairing"
#define CLOVA_HEADER_NAME_BT_STOP_PAIRING "BtStopPairing"
#define CLOVA_HEADER_NAME_BT_RESCAN "BtRescan"
#define CLOVA_HEADER_NAME_BT_DELETE "BtDelete"
#define CLOVA_HEADER_NAME_BT_PLAY "BtPlay"

#define CLOVA_HEADER_NAME_DECREASE "Decrease"
#define CLOVA_HEADER_NAME_EXPECT_REPORT_STATE "ExpectReportState"
#define CLOVA_HEADER_NAME_INCREASE "Increase"
#define CLOVA_HEADER_NAME_LAUNCH_APP "LaunchApp"
#define CLOVA_HEADER_NAME_OPEN_SCREEN "OpenScreen"
#define CLOVA_HEADER_NAME_SET_VALUE "SetValue"
#define CLOVA_HEADER_NAME_SYNCHRONIZE_STATE "SynchronizeState"
#define CLOVA_HEADER_NAME_TURN_OFF "TurnOff"
#define CLOVA_HEADER_NAME_TURN_ON "TurnOn"

/* BtConnect payload */
#define CLOVA_PAYLOAD_NAME_NAME "name"
#define CLOVA_PAYLOAD_NAME_ADDRESS "address"
#define CLOVA_PAYLOAD_NAME_ROLE "role"
#define CLOVA_PAYLOAD_NAME_CONNECTED "connected"

/* BtConnectByPINCode payload */
#define CLOVA_PAYLOAD_NAME_PIN_CODE "pinCode"

/* BtDisconnect payload */

/* BtStartPairing payload */

/* BtStopPairing payload */

/* Decrease payload */
#define CLOVA_PAYLOAD_NAME_TARGET "target"

/* ExpectReportState payload */
#define CLOVA_PAYLOAD_NAME_DURATION_IN_SECONDS "durationInSeconds"
#define CLOVA_PAYLOAD_NAME_INTERVAL_IN_SECONDS "intervalInSeconds"

/* Increase payload */

/* LaunchApp payload */

/* OpenScreen payload */

/* SetValue payload */
#define CLOVA_PAYLOAD_NAME_VALUE "value"
#define CLOVA_PAYLOAD_VALUE_VOLUME "volume"
#define CLOVA_PAYLOAD_VALUE_SCREEN_BRIGHTNESS "screenbrightness"
#define CLOVA_PAYLOAD_VALUE_CHANNEL "channel"

/* SynchronizeState payload */
#define CLOVA_PAYLOAD_NAME_DEVICE_ID "deviceid"
#define CLOVA_PAYLOAD_NAME_DEVICE_STATE "deviceState"

/* TurnOff payload */

/* TurnOn payload */

/* Event Part */
/* Name */
#define CLOVA_HEADER_NAME_ACTION_EXECUTED "ActionExecuted"
#define CLOVA_HEADER_NAME_ACTION_FAILED "ActionFailed"
#define CLOVA_HEADER_NAME_REPORT_STATE "ReportState"

/* ActionExecuted payload */
#define CLOVA_PAYLOAD_NAME_COMMAND "command"

/* DND Directive */
#define CLOVA_HEADER_NAME_SETDND "SetDND"
#define CLOVA_HEADER_NAME_CANCELDND "CancelDND"

#endif
