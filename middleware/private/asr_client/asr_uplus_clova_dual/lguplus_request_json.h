
#ifndef __LGUPLUS_REQUEST_JSON__
#define __LGUPLUS_REQUEST_JSON__

#ifdef __cplusplus
extern "C" {
#endif

char *lguplus_create_cid_token(void);

char *lguplus_create_request_data(char *cmd_json, int *type);

#ifdef __cplusplus
}
#endif

#endif
