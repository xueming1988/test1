
#ifndef __LGUP_GENIE_MUSIC_H__
#define __LGUP_GENIE_MUSIC_H__

#define GENIE_SERVICE_LOGIN_URL "https://ext-api.genie.co.kr/authorize/key"
#define GENIE_SERVICE_RESOURCE_URL "https://ext-api.genie.co.kr/api/v1/tracks/"

#define GENIE_SECRET "secret"
#define GENIE_ACCESS_TOKEN "access_token"
#define GENIE_ITEMS "items"
#define GENIE_RESOURCE_URL "resource_url"

typedef struct {
    char *access_token;
    char *resource_url;
} lguplus_genie_t;

#endif