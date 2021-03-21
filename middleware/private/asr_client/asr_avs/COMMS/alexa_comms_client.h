#ifndef __AVS_COMMS_API_H__
#define __AVS_COMMS_API_H__

#ifdef __cplusplus
extern "C" {
#endif

#define SOCKET_ALEXA_COMMS ("/tmp/ALEXA_COMMS")
#define SOCKET_ALEXA_ASRTTS ("/tmp/AMAZON_ASRTTS")
#define COMMS_CERTS_DIR_PATH ("/etc/ssl/certs/iot")
#define COMMS_CERTS_PROJECT_DIR_PATH ("/system/workdir/script/certs")
#define COMMS_CERTS_DEVKEY_NAME_ENC ("devKey.bin")
#define COMMS_CERTS_DEVCRT_NAME_ENC ("devCrt.bin")
#define COMMS_CERTS_ROOTCA_NAME ("rootCa")
#define COMMS_CERTS_DEVKEY_NAME ("devKey")
#define COMMS_CERTS_DEVCRT_NAME ("devCrt")

// TODO: need read from cfg file
// { For IHOME
#define IOT_HOST_URL ("a7kk1yjx61py5.iot.us-west-2.amazonaws.com")

#if defined(USE_COMMS_LIB)
int acs_init(void);
#endif

#ifdef __cplusplus
}
#endif

#endif
