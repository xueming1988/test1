#ifndef voipSDK_h
#define voipSDK_h

#include "deviceCommonDef.h"

#ifndef __NO_VOIP

/*********************** type def begin **************************/

/**
 * @brief voip & wechate message notifys
 */
typedef struct _voip_callback {
    /**
     * @brief recieve voip invite, use roomId and appId to join voip room.
     * @param fromUsername binder uername
     * @param type 0:default 1:AV 2: audio only
     * @code example
        on_voip_invited(const char *fromUsername, int type)
        {
            play_ring_loop("phonering.mp3");
            const auto& iter = std::find_if(myBinderList.cbegin(),
     myBinderList.cend(), [&](DEVICE_BINDER_INFO& info){ return 0 ==
     strcmp(info.username, fromUsername);}); std::stringstream ss; ss <<
     "您收到来自"; if (iter != myBinderList.cend()) { ss << strcmp(info.remark,"")
     != 0 ? info.remark:info.nickname; } else { ss << "未知联系人";
            }
            ss << "的电话，接听还是拒绝？";
            xiaowei_request(voiceid, xiaowei_chat_only_tts, ss.str().c_str(),
     nullptr);
            // play this tts and wakeup, start voice request with 2VA mode (words
     = {"接听","拒绝"};), then you can get the result wether answer or reject. if
     (result == "接听" || (just push answer buttom)) voip_answer(); else if
     (result == "拒绝" || (just push reject buttom)) voip_hangup(); else
     askUserAgain();
        }
     */
    void (*on_voip_invited)(const char *fromUsername, int type);

    /**
     * @brief join voip room result.
     * @param result if result not 0, usually should hangupVoip. Pay attention to
     * switch thread to call voip_hangup().
     * @param isMaster true:caller false:called
     */
    void (*on_voip_join_room)(int result, bool isMaster);

    /**
     * @brief As master(caller), sdk will invite the called once joining room
     * success. This is invite result callback.
     * @param result if result not 0, usually should hangupVoip. Pay attention to
     * switch thread to call voip_hangup().
     */
    void (*on_voip_invite_result)(int result);

    /**
     * @brief voip has been canceled. This will triggered by calling voip_hangup()
     * or hanguped by the other part.
     */
    void (*on_voip_cancel)(bool isMaster);

    /**
     * @brief voip talking start, device need to send and get AV data until
     * finish.
     */
    void (*on_voip_talking)();

    /**
     * @brief voip finish
     */
    void (*on_voip_finish)();
    /**
     * @brief voip param change, client must follow this change ASAP. This
     * callback will trigger before on_voip_talking().
     * @param sampleRate sampleRate for audio, such as 16000, 80000
     * @param sampleLenInms data length for per frame
     * @param channels channel numbers
     */
    void (*on_voip_param_change)(unsigned int sampleRate, unsigned int sampleLenInms,
                                 unsigned int channels, const char *extends);

    /**
     * @brief voip room member change
     * @param currentNumber size of memberIds
     * @param memberIds members
     */
    void (*on_voip_member_change)(unsigned int currentNumber, const int *memberIds,
                                  const char **memberUsernames);

    /**
     * @brief voip av status change
     * @param audioEnableNumber size of audioEnableIds
     * @param videoEnableNumber size of videoEnableIds
     */
    void (*on_voip_av_change)(unsigned int audioEnableNumber, const int *audioEnableIds,
                              unsigned int videoEnableNumber, const int *videoEnableIds);
} VOIP_CALLBACK;

typedef struct _voip_init_param {
    /** audio and video info */
    unsigned int audio_type;       /// 0 silk，1 silk+aac-ld, only silk+aac-ld support
                                   /// for xiaowei
    unsigned int def_close_av;     /// 1:close audio，2:close video, 0:default. for
                                   /// linux device, must be 2
    unsigned int video_ratio;      /// 0:3/4，1:16/19，2:1/1，3:1/2
    unsigned int def_video_length; /// default length for long side, such as 640.(e.g. if
                                   /// video_ratio = 0, it will be 640:480)
    /** device hardware info */
    unsigned int cpu_core_number; /// CPU core number, must > 0
    unsigned int cpu_freq;        /// CPU frequency（unit: MHz）
    unsigned int cpu_arch;        /// CPU architecture（1: armv5，2: armv6，4: armv7）
} VOIP_INIT_PARAM;

/*********************** type def end **************************/

/*********************** interface begin **************************/

/**
 * @brief start voip service. Re-call this interface will stop the previous one
 * and start new instance.
 * @param callback nonnullable
 * @param param set null to use default param.
 */
DEVICE_API int voip_service_start(VOIP_CALLBACK *callback, VOIP_INIT_PARAM *param);

/**
 * @brief stop voip service
 */
DEVICE_API int voip_service_stop();

/**
 * @brief create voip call, SDK will create a room for VOIP, get create result
 * in callback
 * @param username The target user's name
 * @param appId target appID, set NULL or "" to use default app, DO NOT set any
 * value at present
 * @param type 0:default 1:AV 2: audio only
 */
DEVICE_API int voip_create_call(const char *username, const char *appId, int type);

/**
 * @brief answer voip for the recently invite.
 */
DEVICE_API int voip_answer();

/**
 * @brief hangup voip for the currently calling or reject the invite.
 */
DEVICE_API int voip_hangup();

/**
 * @brief send audio data. need to call this continually during voip talking,
 * @param pData pcm format,16bit
 * @param dataLen length of pData, must same with the reesult of
 * on_voip_param_change(...).
 * @param playDelayMS play delay for the remote.
 * @code
 * // save the params in callback.
 * on_voip_param_change(unsigned int sampleRate, unsigned int sampleLenInms,
 * unsigned int channels, const char *extends) { m_sampleRate = sampleRate;
 *     m_sampleLenInms = sampleLenInms;
 *     m_channels = channels;
 * }
 *
 * .....
 * // call voip_send_audio_data() per sampleLenInms aftere on_voip_talking()
 * triggered. SetloopTask(interval = sampleLenInms, []{
 *     voip_send_audio_data(m_dataBuf, (m_sampleRate/1000) * m_sampleLenInms *
 * sizeof(short) * m_channels, m_playDelayMs);
 * });
 *
 *
 */
DEVICE_API int voip_send_audio_data(const unsigned char *pData, int dataLen, int playDelayMS);

/**
 * @brief get audio data. need to call this continually during voip talking.
 * @param pBuf pcm format
 * @param bufLen length of pData.
 */
DEVICE_API int voip_get_audio_data(unsigned char *pBuf, int bufLen);

/**
 * @brief open or close audio and video
 */
DEVICE_API int voip_switch_AV(bool isOpenAudio, bool isOpenVideo);

/**
 * @brief get if someone is talking
 * @param memberId voip member id, get from voip_get_vad_members(...)
 */
DEVICE_API int voip_get_voice_activity(int memberId);

/**
 * @brief reserved
 */
DEVICE_API int voip_put_externalplayAudioData(unsigned char *pData, int samplerate, int channels,
                                              int samples, int format);

/**
 * @brief set app cmd
 */
DEVICE_API int voip_set_app_cmd(int type, unsigned char *pParameter, int len);

/**
 * @brief get vad members.
 * @param pMemberlist output param. All vad members will list there. list size
 * should no less than room member number.
 */
DEVICE_API int voip_get_vad_members(int *pMemberlist);

#if (defined(__android__) || defined(__ANDROID__))
/**
 * @brief send video data, Android only
 */
DEVICE_API int voip_send_video_data(const unsigned char *pData, int dataLen, int width, int height,
                                    int format);

/**
 * @brief get video data, Android only
 */
DEVICE_API int voip_get_video_data(int *memberId, unsigned char *pBuf, int *dataLen, int *width,
                                   int *height);

DEVICE_API int voip_video_pre_enc_process_cloud(unsigned char *pImg, unsigned char *pDst, int nLen,
                                                void *pFormat);

DEVICE_API int voip_set_video_resolution(const unsigned char *pVideoResoPb, int pbLen);

#endif
/**
 * @brief send wechat message
 * @param msgId just like vocieID, size must = 33
 * @param username target username
 * @param content a json string, define will comming soon
 * @code
     char msgID[33] = {0};
     wechat_send_text_msg(msgId, username, content);
 */
DEVICE_API int wechat_send_text_msg(char *msgId, const char *username, const char *content);
DEVICE_API int wechat_send_image_msg(char *msgId, const char *username, const char *content);
DEVICE_API int wechat_send_video_msg(char *msgId, const char *username, const char *content);
DEVICE_API int wechat_send_iot_msg(char *msgId, const char *username, const char *type,
                                   const char *content);

/*********************** interface end **************************/

#endif

#endif /* voipSDK_h */
