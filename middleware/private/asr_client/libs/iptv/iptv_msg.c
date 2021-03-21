#include <stdio.h>
#include <stdlib.h>

#include "wm_util.h"
#include "json.h"
#include "asr_json_common.h"
#include "iptv_msg.h"

int iptv_parse_cdk_directive(json_object *js_directive)
{
    int ret = -1;
    json_object *js_header;
    json_object *js_payload;
    char *name;

    if (js_directive) {
        js_header = json_object_object_get(js_directive, HEADER);
        js_payload = json_object_object_get(js_directive, PAYLOAD);

        if (js_header) {
            name = JSON_GET_STRING_FROM_OBJECT(js_header, NAME);
            if (StrEq(name, IPTV_HEADER_NAME_HANDLE_MESSAGE))
                ret = iptv_handle_handle_message(js_payload);
        }
    }

    return ret;
}

int iptv_handle_handle_message(json_object *js_payload)
{
    int ret = -1;
    char *payload;
    char buf[1024];

    if (js_payload) {
        payload = json_object_get_string(js_payload);
        snprintf(buf, sizeof(buf), "GNOTIFY=CDK_HANDLE_MESSAGE:%s", payload);
        SocketClientReadWriteMsg(RPC_LOCAL_SOCKET_PARTH, buf, strlen(buf), NULL, NULL, 0);
        ret = 0;
    }

    return ret;
}
