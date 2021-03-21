#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/un.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <stdbool.h>
#include "json.h"
#include "alexa_debug.h"
#include "alexa_cmd.h"
#include "alexa_json_common.h"
#include "alexa_donotdisturb.h"

int avs_donot_disturb_parse_directive(json_object *js_data)
{
    int ret = -1;
    if (js_data) {
        json_object *json_header = json_object_object_get(js_data, KEY_HEADER);
        if (json_header) {
            char *name_space = JSON_GET_STRING_FROM_OBJECT(json_header, KEY_NAMESPACE);
            char *name = JSON_GET_STRING_FROM_OBJECT(json_header, KEY_NAME);
            if (name && name_space) {
                if (StrEq(name_space, NAMESPACE_DONOTDISTURB)) {
                    ret = 0;
                    json_object *payload = NULL;
                    payload = json_object_object_get(js_data, KEY_PAYLOAD);
                    bool enabled = JSON_GET_BOOL_FROM_OBJECT(payload, PAYLOAD_ENABLED);
                    if (enabled)
                        NOTIFY_CMD(IO_GUARD, "GNOTIFY=DoNotDisturbOn");
                    else
                        NOTIFY_CMD(IO_GUARD, "GNOTIFY=DoNotDisturbOff");
                    alexa_donotdisturb_cmd(0, NAME_REPORTDONOTDISTURB, enabled);
                }
            }
        }
    }
    if (ret != 0) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "prase donot_disturb directive message error\n");
    }
    return ret;
}
