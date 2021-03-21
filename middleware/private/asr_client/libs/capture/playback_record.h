/*************************************************************************
    > File Name: playback_record.h
    > Author:
    > Mail:
    > Created Time: Sat 12 May 2018 11:23:43 PM EDT
 ************************************************************************/

#ifndef _PLAYBACK_RECORD_H
#define _PLAYBACK_RECORD_H

#ifdef __cplusplus
extern "C" {
#endif

int CCaptCosume_Playback(char *buf, int size, void *priv);
int CCaptCosume_initPlayback(void *priv);
int CCaptCosume_deinitPlayback(void *priv);
void StartRecordTest(void);
void StopRecordTest(void);
void ClosePlaybackHandle(void);
void Notify_IOGuard(void);

#ifdef __cplusplus
}
#endif

#endif
