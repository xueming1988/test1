#ifndef __ASR_AUTHORIZATION_H__
#define __ASR_AUTHORIZATION_H__

#define WM_CONFIG_TOP_DIR "/vendor/wiimu"
#define ASR_TOKEN_PATH WM_CONFIG_TOP_DIR "/asr_token"
#define LGUP_TOKEN_PATH WM_CONFIG_TOP_DIR "/lguplus_token"
#define ASR_PRODUCT_FILE "/system/workdir/misc/clova_products.json"
#define ASR_AUTH_CODE_FILE WM_CONFIG_TOP_DIR "/asr_code"

#define PRODUCTS "products"
#define PROJECT "project"
#define NAME "name"
#define ID "id"
#define SECRET "secret"
#define URL "url"

#define CLOVA_CLIENT_ID "clova_client_id"
#define DEVICE_MODEL "device_model"
#define DEVICE_UUID "device_uuid"

#define CLOVA_AUTH_CODE "clovaAuthCode"
#define CLOVA_CODE "code"
#define CLOVA_AUTH_STATE "clovaAuthState"
#define CLOVA_STATE "state"
#define IS_CLEAR_USER_DATA "isClearUserData"

#define ASR_AUTH_CODE_URL "https://auth.clova.ai/authorize"
#define ASR_AUTH_URL "https://auth.clova.ai/token?grant_type=authorization_code"
#define ASR_REFRESH_URL "https://auth.clova.ai/token?grant_type=refresh_token"
#define ASR_DELETE_URL "https://auth.clova.ai/token?grant_type=delete"

#define ASR_CODE_PATTERN "client_id=%s&device_id=%s&model_id=%s&response_type=%s&state=%s"
#define ASR_TOKEN_PATTERN "client_id=%s&client_secret=%s&code=%s&device_id=%s&model_id=%s"
#define ASR_REFRESH_PATTERN                                                                        \
    "client_id=%s&client_secret=%s&refresh_token=%s&device_id=%s&model_id=%s"
#define ASR_DELETE_PATTERN "client_id=%s&client_secret=%s&access_token=%s&device_id=%s&model_id=%s"

#define AUTH_CODE "code"
#define AUTH_STATE "state"
#define ACCESS_TOKEN "access_token"
#define REFRESH_TOKEN "refresh_token"
#define TOKEN_TYPE "token_type"
#define EXPIRES_IN "expires_in"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * create a thread to get asr access token and refreshtoken freom asr server
 */
int asr_authorization_init(void);

int asr_authorization_login(char *data);

int asr_authorization_read_cfg(void);

/*
 * log out from asr server
 */
int asr_authorization_logout(int flags);

/*
  * get the refresh token from result
  *  return the new refreshtoken
  */
char *asr_authorization_refresh_token(void);

/*
  *  get the access token from result
  *  return the new accesstoken
  */
char *asr_authorization_access_token(void);

void *asr_request_access_token_proc(void *);

int lguplus_authorization_read_cfg(void);

char *lguplus_get_device_token(void);

char *lguplus_get_platform_token(void);

int lguplus_parse_server_url(char *url);

int lguplus_parse_aiif_token(char *token);

int lguplus_parse_naver_token(char *token);

#ifdef __cplusplus
}
#endif

#endif
