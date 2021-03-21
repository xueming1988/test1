#ifndef __CXDISH_CTROL_H__
#define __CXDISH_CTROL_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "wm_util.h"

#ifdef __cplusplus
extern "C" {
#endif

// 0:succeed of else failed
int cxdish_log_out(char **des);

// 0:succeed of else failed
int cxdish_para_get(char *des[]);

int cxdish_clock_change(void);

// return 0:succeed or else failed
int cxdish_mic_value_judge(char *val);

// 0:ALL channel mute 1:left channel on 2:right channel on 3:ALL channel on
void cxdish_mic_channel_set(int mic_val, SOCKET_CONTEXT *pmsg_socket, char *recv_buf);

int cxdish_disable_ssp(void);

int cxdish_enable_ssp(void);

void cxdish_change_boost(void);

int cxdish_output_to_mic(void);

int cxdish_output_to_ref(void);

int cxdish_output_to_processed(void);

void cxdish_set_adc_gain(int id, int val);

int dsp_serial_get(char *recv_buf);

void cxdish_call_boost(int flags);

int read_dsp_version(WIIMU_CONTEXT *shm);

void dsp_init(void);

void cxdish_set_mic_distance(int val);

void cxdish_set_noise_gating(int val);

void cxdish_set_drc_gain_func(int high_input, int high_output, int low_input, int low_output,
                              int k_high, int k_middle, int k_low);

#ifdef __cplusplus
}
#endif

#endif
