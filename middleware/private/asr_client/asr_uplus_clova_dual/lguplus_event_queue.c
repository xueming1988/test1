
#include "json.h"

#include "lguplus_json_common.h"
#include "lguplus_request_json.h"
#include "lguplus_event_queue.h"

static cmd_q_t *g_lguplus_event_queue = NULL;

static int lguplus_queue_findby_name(char *cmd_js_str, char *method)
{
    int ret = -1;
    char *val = NULL;
    json_object *cmd_js = NULL;

    if (!cmd_js_str || !method) {
        fprintf(stderr, "cmd_js is NULL\n");
        return -1;
    }

    cmd_js = json_tokener_parse(cmd_js_str);
    if (!cmd_js) {
        fprintf(stderr, "cmd_js is not a json\n");
        return -1;
    }

    val = JSON_GET_STRING_FROM_OBJECT(cmd_js, KEY_METHOD);
    if (StrEq(val, method)) {
        ret = 0;
    }

    json_object_put(cmd_js);

    return ret;
}

static int lguplus_event_remove(void *reasion, void *cmd_dst)
{
    int ret = -1;

    if (cmd_dst && reasion) {
        ret = lguplus_queue_findby_name((char *)cmd_dst, (char *)reasion);
    }

    return ret;
}

cmd_q_t *lguplus_event_queue_init(void)
{
    if (g_lguplus_event_queue) {
        cmd_q_uninit(&g_lguplus_event_queue);
    }
    g_lguplus_event_queue = cmd_q_init(NULL);

    return g_lguplus_event_queue;
}

int lguplus_event_queue_uninit(void)
{
    if (g_lguplus_event_queue) {
        cmd_q_uninit(&g_lguplus_event_queue);
    }
}

void lguplus_event_queue_clear_recognize(void)
{
    if (g_lguplus_event_queue) {
        cmd_q_remove(g_lguplus_event_queue, (void *)LGUP_SPEECHRECOGNIZE, lguplus_event_remove);
    }
}

int lguplus_event_queue_add(char *schema, char *method, char *message_id, char *transaction_id)
{
    int ret = -1;
    char *buff = NULL;

    if (!schema || !method) {
        return -1;
    }

    Create_Event(buff, schema, method, (message_id ? message_id : DEFAULT_STR),
                 (transaction_id ? transaction_id : DEFAULT_STR), DEFAULT_BODY);
    if (buff) {
        cmd_q_add_tail(g_lguplus_event_queue, buff, strlen(buff));
        free(buff);
        ret = 0;
    }

    return ret;
}

int lguplus_event_queue_add_body(char *schema, char *method, char *message_id, char *transaction_id,
                                 char *body)
{
    int ret = -1;
    char *buff = NULL;

    if (!schema || !method) {
        return -1;
    }

    Create_Event(buff, schema, method, (message_id ? message_id : DEFAULT_STR),
                 (transaction_id ? transaction_id : DEFAULT_STR), (body ? body : DEFAULT_BODY));
    if (buff) {
        cmd_q_add_tail(g_lguplus_event_queue, buff, strlen(buff));
        free(buff);
        ret = 0;
    }

    return ret;
}

int lguplus_event_queue_audio(char *method, char *audio_token, long position)
{
    int ret = -1;
    char *buff = NULL;
    char *body = NULL;

    if (!audio_token || !method) {
        return -1;
    }

    Create_AudioBody(body, method, audio_token, position);
    Create_Event(buff, LGUP_AUDIO, method, DEFAULT_STR, DEFAULT_STR, (body ? body : DEFAULT_BODY));
    if (buff) {
        cmd_q_add_tail(g_lguplus_event_queue, buff, strlen(buff));
        free(buff);
        ret = 0;
    }

    if (body) {
        free(body);
    }

    return ret;
}
