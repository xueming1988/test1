#ifndef __CLOVA_AUDIO_PLAYER__
#define __CLOVA_AUDIO_PLAYER__

/* namespace */
#define CLOVA_HEADER_NAMESPACE_AUDIOPLAYER "AudioPlayer"

/* Directive Part */
/* Name */
#define CLOVA_HEADER_CLEAR_QUEUE "ClearQueue"
#define CLOVA_HEADER_EXPECT_REPORT_PLAYBACK_STATE "ExpectReportPlaybackState"
#define CLOVA_HEADER_PLAY "Play"
#define CLOVA_HEADER_STREAM_DELIVER "StreamDeliver"

/* Play payload */
#define CLOVA_PAYLOAD_NAME_AUDIO_ITEM_ID "audioItemId"
#define CLOVA_PAYLOAD_NAME_BEGIN_AT_IN_MILLISECONDS "beginAtInMilliseconds"
#define CLOVA_PAYLOAD_NAME_STREAM "stream"
#define CLOVA_PAYLOAD_NAME_TOKEN "token"
#define CLOVA_PAYLOAD_DURATION_IN_MILLISECONDS "durationInMilliseconds"
#define CLOVA_PAYLOAD_URL_PLAYABLE "urlPlayable"
#define CLOVA_PAYLOAD_AUDIOITEM_STREAM "audioStream"

/* PlayFinished payload */
#define CLOVA_PAYLOAD_NAME_OFFSET_IN_MILLISECONDS "offsetInMilliseconds"

/* Event Part */
/* Name */
#define CLOVA_HEADER_NAME_PLAY_FINISHED "PlayFinished"
#define CLOVA_HEADER_NAME_PLAY_PAUSED "PlayPaused"
#define CLOVA_HEADER_NAME_PLAY_RESUMED "PlayResumed"
#define CLOVA_HEADER_NAME_PLAY_STARTED "PlayStarted"
#define CLOVA_HEADER_NAME_PLAY_STOPPED "PlayStopped"
#define CLOVA_HEADER_NAME_PROGRESS_REPORT_DELAY_PASSED "ProgressReportDelayPassed"
#define CLOVA_HEADER_NAME_PROGRESS_REPORT_INTERVAL_PASSED "ProgressReportIntervalPassed"
#define CLOVA_HEADER_NAME_PROGRESS_REPORT_POSITION_PASSED "ProgressReportPositionPassed"
#define CLOVA_HEADER_NAME_REPORT_PLAYBACK_STATE "ReportPlaybackState"
#define CLOVA_HEADER_NAME_STREAM_REQUESTED "StreamRequested"

/* ReportPlaybackState payload */
#define CLOVA_PAYLOAD_NAME_PLAYER_ACTIVITY "playerActivity"
#define CLOVA_PAYLOAD_NAME_REPEATMODE "repeatMode"
#define CLOVA_PAYLOAD_NAME_TOTAL_IN_MILLISECONDES "totalInMilliseconds"
#define CLOVA_PAYLOAD_NAME_AUDIO_STREAM "audioStream"

#endif
