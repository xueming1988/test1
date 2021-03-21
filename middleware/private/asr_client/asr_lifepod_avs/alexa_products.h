#ifndef __ALEXA_PRODUCTS_H__
#define __ALEXA_PRODUCTS_H__

#ifdef BAIDU_DUMI // baidu

#include "baidu_products_macro.h"

#elif defined(TENCENT) // tencent

#include "tencent_products_macro.h"

#else

#include "alexa_products_macro.h"

#endif

#if 0
typedef struct {
    char *projectName;
    char *deviceName;
    char *clientId;
    char *clientSecret;
    char *serverUrl;
    char *DTID;
} AlexaDeviceInfo;

#define JSON_KEY_PRODUCTS "products"
#define JSON_KEY_PROJECT_NAME "project"
#define JSON_KEY_DEVICE_NAME "name"
#define JSON_KEY_CLIENT_ID "id"
#define JSON_KEY_CLIENT_SECRET "secret"
#define JSON_KEY_SERVER_URL "url"
#define JSON_KEY_DTID "DTID"
#endif

int get_alexa_device_info(char *path, char *device_name, AlexaDeviceInfo *result);
void free_alexa_device_info(AlexaDeviceInfo *p);

#endif
