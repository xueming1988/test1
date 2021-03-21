//
//  deviceSDK.h
//  iLinkSDK
//
//  Created by cppwang(王缘) on 2019/5/8.
//  Copyright © 2019 Tencent. All rights reserved.
//

#ifndef voipSDK_h
#define voipSDK_h

#include "deviceCommonDef.h"
#define __NO_VOIP 1
#ifndef __NO_VOIP
CXX_EXTERN_BEGIN_DEVICE

/*********************** type def begin **************************/

/**
 * @brief voip global errcode
 */

/**
 * @brief voip & wechate message notifys
 */
typedef struct _voip_callback {
    /**
     * @brief recieve voip invite, use roomId and appId to join voip room.
     * @code example
        on_voip_invite(const char *fromUsername, unsigned long long roomId, const char *appId)
        {
            play_ring_loop("叮叮叮叮叮叮~~~");
            time = getLocaltime();
            remark = m_contactMap.find(fromUsername)->second.remark;
            xiaowei_request_protocol_tts(voiceid, remark, time, typeVoip);
            //you can get tts"您收到来自爸爸的电话，接听还是拒绝", play this tts and wakeup, start
     voice request, then you can get the result
            if ( is_answer_call == yes || (just push yes buttom) ) start_voip(roomid, appid);
        }
     */
    void (*on_voip_invited)(const char *fromUsername);

    void (*on_voip_join_room)(int result, bool isMaster);

    void (*on_voip_invite_result)(int result);

    void (*on_voip_cancel)(bool isMaster);

    void (*on_voip_talking)();

    void (*on_voip_finish)();

    /**
     * @brief send wechat message result
     */
    void (*on_handup_voip)(const char *fromIlinkid, const char *fromOpenid, const char *groupid,
                           int hangUpType);
} VOIP_CALLBACK;

/**
 * @brief create voip call callback, get roomID and appID here then call voip start interface.
 */

/*********************** type def end **************************/

/*********************** interface begin **************************/

/**
 * @brief start voip service, this is static service, no need stop it. Re-call this interface to
 * change VOIP_CALLBACK
 */
DEVICE_API int voip_service_start(VOIP_CALLBACK *callback);

/**
 * @brief stop voip service
 */
DEVICE_API int voip_service_stop();

/**
 * @brief create voip call, SDK will create a room for VOIP, get create result in callback
 * @param username The target user's name
 * @param appId target appID, set NULL or "" to use default app, DO NOT set any value at present
 */
DEVICE_API int voip_create_call(const char *username, const char *appId);

DEVICE_API int voip_answer();
DEVICE_API int voip_hangup();
DEVICE_API int voip_send_audio_data(const unsigned char *pData, int dataLen, int playDelayMS);
DEVICE_API int voip_get_audio_data(unsigned char *pBuf, int bufLen);
DEVICE_API int voip_send_video_data(const unsigned char *pData, int dataLen, int width, int height,
                                    int format);
DEVICE_API int voip_get_video_data(int &memberId, unsigned char *pBuf, int &dataLen, int &width,
                                   int &height);
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

CXX_EXTERN_END_DEVICE

#endif
#endif /* voipSDK_h */
