/*----------------------------------------------------------------------------
 * Name:    alexa_alert.h
 * Purpose:
 * Version:
 * Note(s):
 *----------------------------------------------------------------------------
 *
 * Copyright (c) 2016 wiimu. All rights reserved.
 *----------------------------------------------------------------------------
 * History:
 *         Revision          2016/05/26        Feng Yong
 *----------------------------------------------------------------------------*/

#ifndef __ALEXA_ALERT_H__
#define __ALEXA_ALERT_H__
#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

int alert_init(void);

int alert_get_all_alert(json_object *pjson_head, json_object *pjson_active_head);

int alert_set(char *ptoken, char *ptype, char *pschtime);

void alert_delete(const char *ptoken);

int alert_start(char *ptoken);

void alert_stop(char *ptoken);

void alert_enter_foreground(char *ptoken);

void alert_enter_background(char *ptoken);

int alert_status_get(void);

void alert_delete_ACK(char *ptoken);

void alert_local_delete(void);

void alert_reset(void);

#if defined(IHOME_ALEXA)
int alert_common_for_app(char *cmd);

int alert_get_for_app(SOCKET_CONTEXT *pmsg_socket, char *pbuf);
#endif

int alert_set_ForNamedTimerAndReminder(char *ptoken, char *ptype, char *pschtime,
                                       const char *json_string);

#ifdef __cplusplus
}
#endif

#endif
