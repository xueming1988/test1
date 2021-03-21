#ifndef __IPTV_MSG_H__
#define __IPTV_MSG_H__

#define HEADER ("header")
#define PAYLOAD ("payload")
#define NAME ("name")

/* Namespace value */
#define IPTV_HEADER_NAMESPACE_CDK ("CDK")

/* Name value */
#define IPTV_HEADER_NAME_APPLICATIONS ("Applications")
#define IPTV_HEADER_NAME_HANDLE_MESSAGE ("HandleMessage")
#define IPTV_HEADER_NAME_SEND_MESSAGE ("SendMessage")

/* Payload nave */
#define APPLICATIONS ("applications")
#define ID ("id")
#define ENABLED ("enabled")
#define AVAILABLE ("available")

int iptv_parse_cdk_directive(json_object *);

int iptv_handle_handle_message(json_object *);

#endif