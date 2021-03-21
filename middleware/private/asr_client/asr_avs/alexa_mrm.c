#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
//#include <errno.h>
#include <sys/un.h>

#include <time.h>
#include <pthread.h>

#include "alexa_debug.h"
#include "alexa_json_common.h"
#include "alexa_request_json.h"
#include "alexa_result_parse.h"

#include "wm_util.h"

static char *gMrmClusterContext = NULL;
static pthread_mutex_t g_mutex_mrm_cluster_cntx = PTHREAD_MUTEX_INITIALIZER;

#define lock_mrm_cluster_cntx() pthread_mutex_lock(&g_mutex_mrm_cluster_cntx)
#define unlock_mrm_cluster_cntx() pthread_mutex_unlock(&g_mutex_mrm_cluster_cntx)

char *avs_mrm_parse_cmd(char *json_cmd_str)
{
    json_object *json_cmd = NULL;
    json_object *json_cmd_parmas = NULL;
    const char *cmd_params_str = NULL;
    char *event = NULL;
    char *avs_cmd = NULL;

    if (AlexaDisableMrm()) {
        return NULL;
    }

    if (!json_cmd_str) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "input is NULL\n");
        return NULL;
    }

    json_cmd = json_tokener_parse(json_cmd_str);
    if (!json_cmd) {
        ALEXA_PRINT(ALEXA_DEBUG_ERR, "input is not a json\n");
        return NULL;
    }

    event = JSON_GET_STRING_FROM_OBJECT(json_cmd, Key_Event);
    if (event) {
        json_cmd_parmas = json_object_object_get(json_cmd, Key_Event_params);
        if (json_cmd_parmas) {
            cmd_params_str = json_object_to_json_string(json_cmd_parmas);
        }

        if (StrEq(event, Val_avs_mrm_forwardDirective)) {
            alexa_result_json_parse((char *)cmd_params_str, 0);
        } else if (StrEq(event, Val_avs_mrm_updateClusterCntx)) {
            lock_mrm_cluster_cntx();

            if (gMrmClusterContext)
                free(gMrmClusterContext);

            gMrmClusterContext = strdup(cmd_params_str);

            unlock_mrm_cluster_cntx();
        } else if (StrEq(event, Val_avs_mrm_clearClusterCntx)) {
            lock_mrm_cluster_cntx();

            if (gMrmClusterContext)
                free(gMrmClusterContext);

            gMrmClusterContext = NULL;

            unlock_mrm_cluster_cntx();
        }
    }

    if (json_cmd) {
        json_object_put(json_cmd);
    }

    return avs_cmd;
}

int avs_mrm_cluster_context_state(json_object *js_context_list)
{
    int ret = 0;

    if (AlexaDisableMrm()) {
        return 0;
    }

    lock_mrm_cluster_cntx();

    ALEXA_PRINT(ALEXA_DEBUG_TRACE, " add MRM clusterContext at [%p]\n", gMrmClusterContext);

    if (gMrmClusterContext && js_context_list) {
        json_object *json_state = json_tokener_parse(gMrmClusterContext);

        if (!json_state) {
            ALEXA_PRINT(ALEXA_DEBUG_ERR, "input is not a json\n");
            ret = -1;
            goto end;
        }

        json_object_array_add(js_context_list, json_state);
    }

end:

    unlock_mrm_cluster_cntx();

    return ret;
}

int avs_mrm_notify_network_state_change(int online)
{
    if (AlexaDisableMrm()) {
        return 0;
    }

    SocketClientReadWriteMsg("/tmp/alexa_mrm", "NetworkStateChange", strlen("NetworkStateChange"),
                             0, 0, 0);

    return 0;
}
