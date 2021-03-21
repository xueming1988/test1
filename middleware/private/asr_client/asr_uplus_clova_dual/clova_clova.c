#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "json.h"

#include "asr_debug.h"
#include "asr_cmd.h"
#include "lp_list.h"
#include "cmd_q.h"

#include "asr_json_common.h"
#include "asr_request_json.h"
#include "clova_context.h"

#include "clova_clova.h"

typedef struct {
    int free_talking;
    char *voice_type;
} clova_state_t;

extern int g_downchannel_ready;
static clova_state_t g_clova_state = {0};
static pthread_mutex_t g_state_lock = PTHREAD_MUTEX_INITIALIZER;
#define state_lock() pthread_mutex_lock(&g_state_lock)
#define state_unlock() pthread_mutex_unlock(&g_state_lock)

/*
 * {
 * "directive": {
 * "header": {
 * "messageId": "2ca2ec70-c39d-4741-8a34-8aedd3b24760",
 * "namespace": "Clova",
 * "name": "RequestLogin"
 * },
 * "payload": {}
 *}
 *}
 */
static int clova_parse_expect_login(json_object *json_payload)
{
    int ret = -1;

    if (json_payload == NULL) {
        goto EXIT;
    }

    ret = 0;
// Once the user gets authenticated successfully,
// CIC stops handling old requests of the user.
// If needed, the user has to make the request again.

EXIT:

    return ret;
}

/*
 * {
 * "directive": {
 * "header": {
 * "messageId": "b17df741-2b5b-4db4-a608-85ecb1307b33",
 * "namespace": "Clova",
 * "name": "HandleDelegatedEvent"
 * },
 * "payload": {
 * "delegationId": "99e86204-710a-4e94-b949-a763e78348a7"
 * }
 * }
 * }
 */
static int clova_parse_handle_delegated_event(json_object *json_payload)
{
    int ret = -1;
    char *delegation_id = NULL;

    if (json_payload == NULL) {
        goto EXIT;
    }

    delegation_id = JSON_GET_STRING_FROM_OBJECT(json_payload, CLOVA_PAYLOAD_VAL_DELEGATIONID);
    if (delegation_id) {
        ret = 0;
        // This directive is sent through a downchannel, not as a response to an event.
        char *payload = json_object_to_json_string(json_payload);
        if (payload) {
            cmd_add_normal_event(CLOVA_HEADER_CLOVA_NAMESPACE, CLOVA_HEADER_PROCESSDELEGATEDEVENT,
                                 payload);
        }
    }

EXIT:

    return ret;
}

/*
 * {
 * "directive": {
 * "header": {
 * "messageId": "2ca2ec70-c39d-4741-8a34-8aedd3b24760",
 * "namespace": "Clova",
 * "name": "Hello"
 * },
 * "payload": {}
 * }
 * }
 */
static int clova_parse_hello(json_object *json_payload)
{
    int ret = -1;

    if (json_payload == NULL) {
        goto EXIT;
    }

    g_downchannel_ready = 1;

    ret = 0;

EXIT:

    return ret;
}

/*
 * {
 * "directive": {
 * "header": {
 * "messageId": "2ca2ec70-c39d-4741-8a34-8aedd3b24760",
 * "namespace": "Clova",
 * "name": "Help"
 * },
 * "payload": {}
 * }
 * }
 */
static int clova_parse_help(json_object *json_payload)
{
    int ret = -1;

    if (json_payload == NULL) {
        goto EXIT;
    }

    ret = 0;

EXIT:

    return ret;
}

/*
 * {
 * "directive": {
 * "header": {
 * "namespace": "Clova",
 * "name": "RenderTemplate",
 * "messageId": "c7b9882e-d3bf-45b6-b6f4-2fc47d04e60e",
 * "dialogRequestId": "ca10e609-1d24-4418-bc2a-21b70c5ea1a7"
 * },
 * "payload": {
 * {{TemplateFormat}}
 * }
 * }
 * }
 */
static int clova_parse_render_template(json_object *json_payload)
{
    int ret = -1;

    if (json_payload == NULL) {
        goto EXIT;
    }

    ret = 0;

EXIT:

    return ret;
}

/*
 * {
 * "directive": {
 * "header": {
 * "namespace": "Clova",
 * "name": "RenderText",
 * "messageId": "6f9d8b54-21dc-4811-8e14-b9b60717cbd4",
 * "dialogRequestId": "5690395e-8c0d-4123-9d3f-937eaa9285dd"
 * },
 * "payload": {
 * "text": "I will play some upbeat music for you."
 * }
 * }
 * }
 */
static int clova_parse_render_text(json_object *json_payload)
{
    int ret = -1;

    if (json_payload == NULL) {
        goto EXIT;
    }

    ret = 0;

EXIT:

    return ret;
}

/*
 * {
 * "directive": {
 * "header": {
 * "namespace": "Clova",
 * "name": "StartExtension",
 * "messageId": "103b8847-9109-42a5-8d3f-fc370a243d6f",
 * "dialogRequestId": "8b509a36-9081-4783-b1cd-58d406205956"
 * },
 * "payload": {
 * "extension": "SampleExtension"
 * }
 * }
 * }
 */
static int clova_parse_start_extension(json_object *json_payload)
{
    int ret = -1;
    char *extension = NULL;

    if (json_payload == NULL) {
        goto EXIT;
    }

    extension = JSON_GET_STRING_FROM_OBJECT(json_payload, CLOVA_PAYLOAD_KEY_EXTENSION);
    if (StrEq(extension, CLOVA_PAYLOAD_VAL_FREETALKING)) {
        state_lock();
        g_clova_state.free_talking = 1;
        NOTIFY_CMD(ASR_EVENT_PIPE, ASR_FREE_TALK_START);
        state_unlock();
        ret = 0;
    }

EXIT:

    return ret;
}

/*
 * {
 * "directive": {
 * "header": {
 * "namespace": "Clova",
 * "name": "FinishExtension",
 * "messageId": "80855309-77b8-403f-bec5-b50f565d24a2",
 * "dialogRequestId": "3db18cee-caac-4101-891f-b5f5c2e7fa9c"
 * },
 * "payload": {
 * "extension": "SampleExtension"
 * }
 * }
 * }
*/
static int clova_parse_finished_extension(json_object *json_payload)
{
    int ret = -1;
    char *extension = NULL;

    if (json_payload == NULL) {
        goto EXIT;
    }

    extension = JSON_GET_STRING_FROM_OBJECT(json_payload, CLOVA_PAYLOAD_KEY_EXTENSION);
    if (StrEq(extension, CLOVA_PAYLOAD_VAL_FREETALKING)) {
        state_lock();
        g_clova_state.free_talking = 0;
        state_unlock();
        ret = 0;
    }

EXIT:

    return ret;
}

/*
 * parse the Clova Directives
 */
int clova_parse_clova_directive(json_object *json_directive)
{
    int ret = -1;
    char *name = NULL;
    json_object *json_header = NULL;
    json_object *json_payload = NULL;

    if (json_directive == NULL) {
        goto EXIT;
    }

    json_header = json_object_object_get(json_directive, KEY_HEADER);
    if (json_header == NULL) {
        goto EXIT;
    }

    json_payload = json_object_object_get(json_directive, KEY_PAYLOAD);
    if (json_payload == NULL) {
        goto EXIT;
    }

    name = JSON_GET_STRING_FROM_OBJECT(json_header, KEY_NAME);

    if (StrEq(name, CLOVA_HEADER_EXPECTLOGIN)) {
        ret = clova_parse_expect_login(json_payload);
    } else if (StrEq(name, CLOVA_HEADER_HANDLEDELEGATEDEVENT)) {
        ret = clova_parse_handle_delegated_event(json_payload);
    } else if (StrEq(name, CLOVA_HEADER_HELLO)) {
        ret = clova_parse_hello(json_payload);
    } else if (StrEq(name, CLOVA_HEADER_HELP)) {
        ret = clova_parse_help(json_payload);
    } else if (StrEq(name, CLOVA_HEADER_RENDERTEMPLATE)) {
        ret = clova_parse_render_template(json_payload);
    } else if (StrEq(name, CLOVA_HEADER_RENDERTEXT)) {
        ret = clova_parse_render_text(json_payload);
    } else if (StrEq(name, CLOVA_HEADER_STARTEXTENSION)) {
        ret = clova_parse_start_extension(json_payload);
    } else if (StrEq(name, CLOVA_HEADER_FINISHEXTENSION)) {
        ret = clova_parse_finished_extension(json_payload);
    } else {
        ASR_PRINT(ASR_DEBUG_ERR, "unknown directive (%s.%s)\n", CLOVA_HEADER_CLOVA_NAMESPACE, name);
    }

EXIT:

    return ret;
}

int clova_get_extension_state(void)
{
    int ret = -1;

    state_lock();
    ret = g_clova_state.free_talking;
    state_unlock();

    return ret;
}

void clova_clear_extension_state(void)
{
    state_lock();
    g_clova_state.free_talking = 0;
    state_unlock();
}

/*
 * {
 * "header": {
 *     "namespace": "Clova",
 *     "name": "FreetalkState"
 * },
 * "payload": {
 *     "foreground": true,
 *     "reprompt": false
 * }
 * }​​
 */
int clova_clova_extension_state(json_object *context_list)
{
    char *voice_type = "V001";
    json_object *extension_payload = NULL;
    json_object *assistant_payload = NULL;
    int ret = -1;

    if (context_list == NULL) {
        goto EXIT;
    }

    extension_payload = json_object_new_object();
    if (extension_payload == NULL) {
        goto EXIT;
    }

    assistant_payload = json_object_new_object();
    if (assistant_payload == NULL) {
        goto EXIT;
    }

    state_lock();
    json_object_object_add(extension_payload, CLOVA_PAYLOAD_FOREGROUND,
                           json_object_new_boolean(g_clova_state.free_talking));
    json_object_object_add(extension_payload, CLOVA_PAYLOAD_REPROMPT,
                           json_object_new_boolean(!g_clova_state.free_talking));

    if (g_clova_state.voice_type && strlen(g_clova_state.voice_type) > 0) {
        voice_type = g_clova_state.voice_type;
    }
    json_object_object_add(assistant_payload, CLOVA_PAYLOAD_VOICETYPE,
                           JSON_OBJECT_NEW_STRING(voice_type));
    state_unlock();

    ret = asr_context_list_add(context_list, CLOVA_HEADER_CLOVA_NAMESPACE,
                               CLOVA_HEADER_FREETALKSTATE, extension_payload);
    if (ret != 0) {
        if (extension_payload) {
            json_object_put(extension_payload);
            extension_payload = NULL;
        }
    }
    ret = asr_context_list_add(context_list, CLOVA_HEADER_CLOVA_NAMESPACE,
                               CLOVA_HEADER_ASSISTANTSTATE, assistant_payload);
    if (ret != 0) {
        if (assistant_payload) {
            json_object_put(assistant_payload);
            assistant_payload = NULL;
        }
    }

EXIT:
    if (ret != 0) {
        if (extension_payload) {
            json_object_put(extension_payload);
        }
        if (assistant_payload) {
            json_object_put(assistant_payload);
        }
    }

    return ret;
}

int clova_set_voice_type(char *voice_type)
{
    int ret = -1;

    if (voice_type && strlen(voice_type) > 0) {
        state_lock();
        if (g_clova_state.voice_type) {
            free(g_clova_state.voice_type);
            g_clova_state.voice_type = NULL;
        }
        g_clova_state.voice_type = strdup(voice_type);
        state_unlock();
    }

    return ret;
}
