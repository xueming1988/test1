/*************************************************************************
    > File Name: cmd_q.h
    > Author:
    > Mail:
    > Created Time: Tue 21 Nov 2017 02:35:55 AM EST
 ************************************************************************/

#ifndef __CMD_Q_H__
#define __CMD_Q_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cmd_q_s cmd_q_t;

typedef int(replace_cb_t)(void *cmd_src, void *cmd_dst);
typedef int(remove_cb_t)(void *reasion, void *cmd_dst);

cmd_q_t *cmd_q_init(replace_cb_t *replace_cb);

int cmd_q_uninit(cmd_q_t **self_p);

int cmd_q_add(cmd_q_t *self, void *cmd_ptr, size_t len);

int cmd_q_add_tail(cmd_q_t *self, void *cmd_ptr, size_t len);

int cmd_q_replace(cmd_q_t *self, void *cmd_ptr, size_t len);

int cmd_q_remove(cmd_q_t *self, void *reasion, remove_cb_t remove_cb);

void *cmd_q_pop(cmd_q_t *self);

int cmd_free(void **pop_data);

int cmd_q_clear(cmd_q_t *self);

int cmd_q_test(void);

#ifdef __cplusplus
}
#endif

#endif
