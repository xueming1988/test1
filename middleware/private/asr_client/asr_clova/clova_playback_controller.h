#ifndef __CLOVA_PLAYBACK_CONTROLLER__
#define __CLOVA_PLAYBACK_CONTROLLER__

/* namespace */
#define CLOVA_HEADER_NAMESPACE_PLAYBACK_CONTROLLER "PlaybackController"

/* Directive Part */
/* Name */
#define CLOVA_HEADER_NAME_EXPECT_NEXT_COMMAND "ExpectNextCommand"
#define CLOVA_HEADER_NAME_EXPECT_PAUSE_COMMAND "ExpectPauseCommand"
#define CLOVA_HEADER_NAME_EXPECT_PLAY_COMMAND "ExpectPlayCommand"
#define CLOVA_HEADER_NAME_EXPECT_PREVIOUS_COMMAND "ExpectPreviousCommand"
#define CLOVA_HEADER_NAME_EXPECT_RESUME_COMMAND "ExpectResumeCommand"
#define CLOVA_HEADER_NAME_EXPECT_STOP_COMMAND "ExpectStopCommand"
#define CLOVA_HEADER_NAME_MUTE "Mute"
#define CLOVA_HEADER_NAME_NEXT "Next"
#define CLOVA_HEADER_NAME_PAUSE "Pause"
#define CLOVA_HEADER_NAME_PREVIOUS "Previous"
#define CLOVA_HEADER_NAME_REPLAY "Replay"
#define CLOVA_HEADER_NAME_RESUME "Resume"
#define CLOVA_HEADER_NAME_SETREPEATMODE "SetRepeatMode"
#define CLOVA_HEADER_NAME_STOP "Stop"
#define CLOVA_HEADER_NAME_TURN_OFF_REPEAT_MODE "TurnOffRepeatMode"
#define CLOVA_HEADER_NAME_TURN_ON_REPEAT_MODE "TurnOnRepeatMode"
#define CLOVA_HEADER_NAME_UNMUTE "Unmute"
#define CLOVA_HEADER_NAME_VOLUME_DOWN "VolumeDown"
#define CLOVA_HEADER_NAME_VOLUME_UP "VolumeUp"

/* SetRepeatMode payload */
#define CLOVA_PAYLOAD_NAME_REPEATMODE "repeatMode"
#define CLOVA_PAYLOAD_VALUE_NONE "NONE"
#define CLOVA_PAYLOAD_VALUE_REPEAT_ONE "REPEAT_ONE"

/* Event Part */
/* Name */
#define CLOVA_HEADER_NAME_RESUME_COMMAND_ISSUED "ResumeCommandIssued"
#define CLOVA_HEADER_NAME_SET_REPEAT_MODE_COMMAND_ISSUED "SetRepeatModeCommandIssued"
#define CLOVA_HEADER_NAME_STOP_COMMAND_ISSUED "StopCommandIssued"

/* NextCommandIssued payload */
#define CLOVA_PAYLOAD_NAME_DEVICE_ID "deviceId"

#endif
