#ifndef __ALEXA_AUTHORIZATION_H__
#define __ALEXA_AUTHORIZATION_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * create a thread to get alexa access token and refreshtoken freom amazon server
 */
int AlexaAuthorizationInit(void);

int AlexaAuthorizationLogIn(char *data);

int alexa_auth_read_cfg(int flag);

/*
 * log out from AVS server
 */
int AlexaAuthorizationLogOut(int flags);

/*
  * get the refresh token from result
  *  return the new refreshtoken
  */
char *AlexaGetRefreshToken(void);

/*
  *  get the access token from result
  *  return the new accesstoken
  */
char *AlexaGetAccessToken(void);

/*
  *  try to get alexa account info from flash
  */
void AlexaGetDeviceInfo(void);

/*
  *  save alexa account info to flash
  */
void AlexaSetDeviceInfo(char *device_id, char *client_id, char *client_secret, char *server_url);

int AlexaLogInWithParam(char *data);

int LifepodAuthorizationLogin(char *data);

char *AlexaGetDeviceInfoJson(void);

int pco_delete_report(void);
int pco_enable_skill(char *data);
int pco_get_skill_enable_status(char *data);
int pco_disable_skill(char *data);

#ifdef __cplusplus
}
#endif

#endif
