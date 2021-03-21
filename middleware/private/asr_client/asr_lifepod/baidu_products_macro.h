
#ifndef __BAIDU_PRODUCTS_MACRO_H__
#define __BAIDU_PRODUCTS_MACRO_H__

#ifndef ALEXA_BETA
#define ALEXA_BETA (0)
#endif

#define ALEXA_PRODUCTS_FILE_PATH ("/system/workdir/misc/config.bin")

#if defined(S11_EVB_EUFY_DOT)
#define ALEXA_DEFAULT_DEVICE_NAME ("Halo")
#define ALEXA_DEFAULT_DEVICE_NAME_BETA ("Halo")
//#define ALEXA_DEFAULT_CLIENT_ID               ("Knahud3h4y8IZzBCRqCPNQ5rTQRKGnSl")
//#define ALEXA_DEFAULT_CLIENT_SECRET         ("TMERAMSP8l02XQPFm56n61RC9dhUp8Xq")
#define ALEXA_DEFAULT_CLIENT_ID                                                                    \
    ("\xba\xe8\x97\xd1\x48\x24\x7e\xa7\x77\x3d\x5e\x1f\xcb\x4f\x89\xfa\x57\x2e\x47\x61\x50\x8a"    \
     "\x78\x13\x40\x0\x83\xc5\x46\x5d\xe2\x55")
#define ALEXA_DEFAULT_CLIENT_SECRET                                                                \
    ("\xa5\xcb\xb3\xeb\x7c\xd\x1e\x9f\x7b\x28\x56\x64\xc9\x64\x9b\xff\x68\x6a\x32\x5f\x28\xea\x1f" \
     "\x22\x2d\x35\xb9\xdb\x71\xb\xe9\x48")
#define ALEXA_DEFAULT_SERVER_URL ("")

#else

#define ALEXA_DEFAULT_DEVICE_NAME ("muzo")
#define ALEXA_DEFAULT_DEVICE_NAME_BETA ("muzo")
//#define ALEXA_DEFAULT_CLIENT_ID             ("BXLmbwFj9TnUw2WmhWYeQ2AMyENdHby9")
//#define ALEXA_DEFAULT_CLIENT_SECRET         ("dVr3ARUO3fXv0oMoBLxIwztgbPZMzvZf")
#define ALEXA_DEFAULT_CLIENT_ID                                                                    \
    ("\xb3\xde\xba\xd4\x5f\x37\xb\xa5\x7a\x10\x8\x3\xe6\x7\x9c\xd4\x6d\x8\x5d\x54\x4f\xe9\xc\x2c"  \
     "\x6d\x14\x9f\xea\x49\x51\xc8\x0")
#define ALEXA_DEFAULT_CLIENT_SECRET                                                                \
    ("\x95\xd0\x84\x8a\x7c\x12\x18\x80\x70\x22\x3e\x20\xa1\x5a\x86\xd6\x47\x13\x7c\x78\x69\xa1"    \
     "\x39\x6\x76\x1\x8b\xc3\x7b\x45\xeb\x5f")

#define ALEXA_DEFAULT_SERVER_URL ("")

#endif

#endif
