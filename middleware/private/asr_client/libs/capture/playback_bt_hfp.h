#ifndef _PLAYBACK_BT_HFP_H
#define _PLAYBACK_BT_HFP_H

#define BT_HFP_CALL_STARTUP 1
#define BT_HFP_CALL_END 0
#define BT_ALSA_HW "hw:0,3"
#define BT_ALSA_CHANNEL 2

void bluetooth_hfp_start_play(void);
void bluetooth_hfp_stop_play(void);
#endif
