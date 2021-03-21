// Copyright © 2020 Tencent. All rights reserved.
#pragma once
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include "deviceSDK.h"
#include "xiaoweiSDK.h"

namespace xiaowei_sdk_interface
{
void get_qqid_info(void);
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
void XiaoweiLoginInit();
#ifdef __cplusplus
}
#endif /* __cplusplus */

extern void XiaoweiEngineStart();
extern void XiaoweiTextReq(const std::string &text);
extern void XiaoweiVoiceReq();
extern void XiaoweiCommonReq(const std::string &cmd, const std::string &sub_cmd,
                             const std::string &param);
extern void voip_service_init(void);
extern void xiaowei_ota_config_init(void);
extern void xiaowei_query_ota(void);
extern void xiaowei_get_sdk_ver(void);
extern void xiaowei_get_uin_info(void);

}; // namespace xw_test
