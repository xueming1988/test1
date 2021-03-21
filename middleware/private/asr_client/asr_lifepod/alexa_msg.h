#ifndef __ALEXA_MSG_ROUTE_H__
#define __ALEXA_MSG_ROUTE_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum alexa_notify_msg_e {
    ALEXA_NOTIFY_MSG_INVALID = -1,
    ALEXA_NOTIFY_MSG_NEED_AUTH = 0,
    ALEXA_NOTIFY_MSG_READY,
    ALEXA_NOTIFY_MSG_CONNECT_INTERNET_ERROR,
    ALEXA_NOTIFY_MSG_SET_TOKEN,
} ALEXA_NOTIFY_MSG_E;

int alexa_notify_msg(ALEXA_NOTIFY_MSG_E MsgType);

#ifdef __cplusplus
}
#endif

#endif /* __ALEXA_MSG_ROUTE_H__ */
