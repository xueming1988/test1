/*
 * Copyright (C) Linkplay Technology - All Rights Reserved
 *
 * Unauthorized copying of this file, via any medium is strictly prohibited
 * Proprietary and confidential
 * <huaijing.wang@linkplay.com>, june. 2020
 *
 * Linkplay skew source files
 *
 */

#ifndef _LPUTILS_SKEW_HEADER__
#define _LPUTILS_SKEW_HEADER__

#ifdef __cplusplus
extern "C" {
#endif
void *lp_skew_init(int channel, int bytes_per_sample, int sampleRate);
void lp_skew_deinit(void *p);
int lp_do_skew(void *p, unsigned char *data, int samples, int skewAdjustSample, unsigned char  **outdata);
void lp_skew_PIDInit(void *p,
                     double pGain,
                     double iGain,
                     double dGain,
                     double dPole,
                     double iMin,
                     double iMax);
double lp_skew_PIDUpdate(void *p, double input);
void lp_skew_PIDReset(void *p);

#ifdef __cplusplus
}
#endif

#endif //_LPUTILS_SKEW_HEADER__
