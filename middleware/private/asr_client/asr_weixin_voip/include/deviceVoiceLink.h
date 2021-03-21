#ifndef __TX_DECODE_ENGINE__H__
#define __TX_DECODE_ENGINE__H__
#include "deviceCommonDef.h"

enum VOICE_LINK_RESULT {
    voice_link_error_null = 0,
    voice_link_error_inited = 1,
    voice_link_error_no_memory = 2,
    voice_link_error_create_lock_fail = 3,
    voice_link_error_create_thread_fail = 4,
    voice_link_error_param = 1001,
};

#define MAX_SSID_LEN 256
#define MAX_PSWD_LEN 256
#define MAX_TOKEN_LEN 256

typedef struct xw_voicelink_param {
    char ssid[MAX_SSID_LEN];     /// ssid, utf-8 encode
    char password[MAX_PSWD_LEN]; /// password, utf-8 encode
    char token[MAX_TOKEN_LEN];   /// token to verify this voice-link
} XW_VOICELINK_PARAM;

/**
 * @brief if decode failed, please check:
 * check 1：Audio data is 16 bit, sigle channel, pcm format.
 * check 2：Samplerate set by init function is same with the actual.
 * check 3：No interrupt during recording.
 * check 4: data length is correct. eg. sample rate is 8000Hz, 20ms <--> 160
 * samples, nlen = 160. nlen != 320.
 */

typedef void (*VL_FUNC_NOTIFY)(XW_VOICELINK_PARAM *pparam);

/**
 * @brief init decoder
 * @param func callback when process success. Get wifi info and token here.
 * @param samplerate 16000 is recommended.
 */
DEVICE_API int voicelink_init_decoder(VL_FUNC_NOTIFY func, int samplerate);

/**
 * @brief Destroy voice-link module, call this after voice-link process finish
 * to release memory.
 *
 */
DEVICE_API void voicelink_uninit_decoder();

/**
 * @brief fill audio data multi-times by this function after
 * voicelink_init_decoder(...).
 * @param audio 16bit, singl channel, pcm data. 20ms per frame is recommended.
 * @param nlen length of audio(not size of it). eg. sample rate is 8000Hz, 20ms
 * <--> 160 samples, nlen = 160. nlen != 320.
 *
 */
DEVICE_API void voicelink_fill_audio(signed short *audio, int nlen);

#endif
