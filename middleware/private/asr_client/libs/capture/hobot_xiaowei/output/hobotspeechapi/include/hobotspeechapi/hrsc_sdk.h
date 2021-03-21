/*
 * Copyright (c) 2018 xiaomi.
 *
 * Unpublished copyright. All rights reserved. This material contains
 * proprietary information that should be used or copied only within
 * xiaomi, except with written permission of xiaomi.
 *
 * @file:    hrsc_sdk.h
 * @brief:   definition of voice preprocess module
 * @author:  xulongqiu@xiaomi.com
 * @version: 1.3
 * @date:    2018-06-15 13:01:58
 */

#ifndef __HRSC_SDK_H__
#define __HRSC_SDK_H__

#if defined(__cplusplus)
extern "C" {
#endif
#define HRSC_SDK_VERSION 3

typedef unsigned long long hrsc_time_stamp_t;

typedef enum {
    HRSC_LOG_LVL_ERROR = 0,
    HRSC_LOG_LVL_WARNING,
    HRSC_LOG_LVL_INFO,
    HRSC_LOG_LVL_DEBUG,
    HRSC_LOG_LVL_TRACE,

    HRSC_LOG_LVL_MAX
} hrsc_log_lvl_t;

typedef enum {
    HRSC_AUDIO_FORMAT_PCM_16_BIT = 0,
    HRSC_AUDIO_FORMAT_PCM_8_BIT,
    HRSC_AUDIO_FORMAT_PCM_24_BIT,
    HRSC_AUDIO_FORMAT_PCM_32_BIT,
} hrsc_audio_format_t;

typedef enum {
    HRSC_EVENT_WAKE_NORMAL = 0,
    HRSC_EVENT_WAKE_ONESHOT,
    HRSC_EVENT_WAIT_ASR_TIMEOUT,
    HRSC_EVENT_VAD_BEGIN,
    HRSC_EVENT_VAD_MID,
    HRSC_EVENT_VAD_END,
} hrsc_event_t;

typedef enum {
    HRSC_STATUS_WAKEUP = 0,
    HRSC_STATUS_UNWAKEUP,
    HRSC_STATUS_ASR_PAUSE,
    HRSC_STATUS_ASR_RESUME,

} hrsc_status_t;

typedef enum {
    /**
    * @brief: just channel mix & resample
    */
    HRSC_EFFECT_MODE_NONE = 0,
    /**
    * @brief: voice recognition
    */
    HRSC_EFFECT_MODE_ASR,
    HRSC_EFFECT_MODE_VOIP,
} hrsc_effect_mode_t;

typedef enum {
    HRSC_PARAS_TYPE_EFFECT_MODE = 0,
    HRSC_PARAS_TYPE_WAIT_ASR_TIMEOUT,
    HRSC_PARAS_TYPE_VAD_TIMEOUT,
    HRSC_PARAS_TYPE_WAKEUP_DATA_SWITCH,    // 0 disable, 1 enable
    HRSC_PARAS_TYPE_VAD_SWITCH,            // 0 disable, 1 enable
    HRSC_PARAS_TYPE_PROCESSED_DATA_SWITCH, // 0, disable, 1 enable
    HRSC_PARAS_TYPE_VOIP_DATA_SWITCH,      // 0, disable, 1 enable
} hrsc_paras_type_t;

typedef enum {
    HRSC_CALLBACK_DATA_ALL = 0,
    HRSC_CALLBACK_DATA_HEADER,
    HRSC_CALLBACK_DATA_MIDDLE,
    HRSC_CALLBACK_DATA_TAIL
} hrsc_callback_data_type_t;

typedef struct paras_data_s {
    hrsc_paras_type_t type;
    void *value;
} hrsc_paras_t;

/**
 * @brief: specifies the input or output audio format to be used by the effect module
 */
typedef struct {
    unsigned int samplingRate;
    unsigned int channels;
    hrsc_audio_format_t format;
} hrsc_audio_config_t;

typedef struct {
    /**
    * @brief: number of bytes in buffer
    */
    unsigned int size;
    /**
    * @brief: ms, timestamp of first frame
    */
    hrsc_time_stamp_t start_time;
    union {
        /**
        * @brief: raw pointer to start of buffer, get buffer by hrsc_get_buffer
        */
        void *raw;
        unsigned int *s32;
        unsigned short *s16;
        unsigned char *u8;
    };
} hrsc_audio_buffer_t;

typedef struct callback_data_s {
    hrsc_callback_data_type_t type;
    hrsc_audio_buffer_t buffer;
    float angle;

    /**
    * @brief: just for wakeup, the score of wakup
    */
    float score;
} hrsc_callback_data_t;

/**
 * @brief: is used to configure audio parameters and callback functions.
 */
typedef struct {

    /**
     * @brief : input auido format
     */
    hrsc_audio_config_t inputCfg;
    /**
     * @brief : target output format
     */
    hrsc_audio_config_t outputCfg;
    /**
     * @brief : channel index of reference
     */
    unsigned char ref_ch_index;
    /**
     * @brief : ms, pcm data before wakeup word
     */
    unsigned int wakeup_prefix;
    /**
     * @brief : ms, pcm data after wakeup word
     */
    unsigned int wakeup_suffix;
    /**
    * @brief : ms, how much time for waitting asr data after wakeup
    */
    unsigned int wait_asr_timeout;
    /**
     * @brief : ms, threshold time for judgment of vad end
     */
    unsigned int vad_timeout;
    /**
     * @brief : voice active detect switch, 0 disable, 1 enable
     */
    unsigned int vad_switch;
    /**
     * @brief : 0: do not send wakeup data to user, 1: send
     */
    unsigned int wakeup_data_switch;
    /**
     * @brief : 0: do not send processed data to user, 1: send
     */
    unsigned int processed_data_switch;
    /**
      * @brief : 0.0 ~ 1.0, do not send wakeup event when score < target_score
      */
    float target_score;
    /**
     * @brief : work mode, see hrsc_effect_mode_t
     */
    hrsc_effect_mode_t effect_mode;
    /**
     * @brief : config file path if needed, e.g., "/vendor/etc/hrsc_cfg.cfg"
     */
    const char *cfg_file_path;
    /**
     * @brief : config license string for security check
     */
    const char *licenseStr;
    /**
     * @brief : activate string, e.g., ""
     */
    char *activeBuff;
    /**
     * @brief : private data pointer
     */
    void *priv;

    /**
    * @brief log callback
    */
    void (*log_callback)(hrsc_log_lvl_t lvl, const char *tag, const char *format, ...);

    /**
     * @brief notify user when a new event happen
     * @param cookie, hrsc_effect_config_t->priv
     * @param event, see hrsc_event_t
     */
    void (*event_callback)(const void *cookie, hrsc_event_t event);
    /**
     * @brief send the wake up data to user
     * @param cookie, hrsc_effect_config_t->priv
     * @param data, see hrsc_callback_data_t
     * @param keyword_index: wakeup keyword index, ignore if just support one keyword
     */
    void (*wakeup_data_callback)(const void *cookie, const hrsc_callback_data_t *data,
                                 const int keyword_index);
    /**
     * @brief asr data callback handler
     * @param cookie, hrsc_effect_config_t->priv
     * @param data, see hrsc_callback_data_t
     */
    void (*asr_data_callback)(const void *cookie, const hrsc_callback_data_t *data);
    /**
     * @brief voip data callback handler
     * @param cookie, hrsc_effect_config_t->priv
     * @param data, see hrsc_callback_data_t
     */
    void (*voip_data_callback)(const void *cookie, const hrsc_callback_data_t *data);
    /**
     * @brief send processed data to user
     * @param cookie, hrsc_effect_config_t->priv
     * @param data, see hrsc_callback_data_t
     */
    void (*processed_data_callback)(const void *cookie, const hrsc_callback_data_t *data);

    /**
     * @brief send auth code to user
     * @param code auth result code
     */
    void (*secure_auth_callback)(int code);
} hrsc_effect_config_t;

/**
 * @brief Initialize voice preprocess module
 * @param config: see hrsc_effect_config_t
 * @return 0 if successful, error code otherwise
 */
int hrsc_init(const hrsc_effect_config_t *config);

/**
 * @brief Start voice preprocess module
 * @return 0 if successful, error code otherwise
 */
int hrsc_start();

/**
 * @brief Stop voice preprocess module
 * @return 0 if successful, error code otherwise
 */
int hrsc_stop();

/**
 * @brief Release the sources of voice preprocess module
 * @return 0 if successful, error code otherwise
 */
int hrsc_release();

/**
 * @brief set status, e.g., user cancle the wakeup
 * @param st: see hrsc_status_t
 */
void hrsc_set_status(hrsc_status_t st);

/**
 * @brief get hrsc status
 * @return see hrsc_status_t
 */
hrsc_status_t hrsc_get_status(void);

/**
 * @brief send the record data to voice process module
 * @param input: record data buffer
 */
void hrsc_process(hrsc_audio_buffer_t *input);

/**
 * @brief 触发REF信号与MIC信号的赔偿
 * @return
 */
void hrsc_trigger_audio_compensate();

/**
 * @brief set parameters
 * @param para: see hrsc_paras_t
 * @return 0 if successfull, error code otherwise
 */
int hrsc_set_paras(const hrsc_paras_t *para);

/**
 * @brief get parameters
 * @param type: see hrsc_paras_type_t
 * @param value:
 */
void hrsc_get_paras(hrsc_paras_type_t type, void *value);
#if HRSC_SDK_VERSION > 1
/**
 * @brief get hrsc sdk version index_sequence_for
 * @return: string of version info which contains factory info and version info, size < 128
 */
const char *hrsc_get_version_info(void);
#endif
#if defined(__cplusplus)
}
#endif

#endif /*__HRSC_SDK_H__*/
