/*************************************************************************
    > File Name: cmd_q.c
    > Author:
    > Mail:
    > Created Time: Tue 21 Nov 2017 02:35:55 AM EST
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "cmd_q.h"

#include "lp_list.h"

#define MAX_Q_SIZE (1000)

typedef struct cmd_item_s {
    void *data;
    struct lp_list_head cmd_item;
} cmd_item_t;

struct cmd_q_s {
    pthread_mutex_t list_lock;
    replace_cb_t *replace_cb;
    struct lp_list_head cmd_list;
};

cmd_q_t *cmd_q_init(replace_cb_t *replace_cb)
{
    cmd_q_t *cmd_q = NULL;

    cmd_q = (cmd_q_t *)calloc(1, sizeof(cmd_q_t));
    if (cmd_q) {
        pthread_mutex_init(&cmd_q->list_lock, NULL);
        cmd_q->replace_cb = replace_cb;
        LP_INIT_LIST_HEAD(&cmd_q->cmd_list);
    } else {
        printf("cmd_q_init calloc failed\n");
    }

    return cmd_q;
}

int cmd_q_uninit(cmd_q_t **self_p)
{
    int ret = -1;
    cmd_q_t *self = *self_p;

    if (self) {
        ret = cmd_q_clear(self);
        pthread_mutex_destroy(&self->list_lock);
        free(self);
        *self_p = NULL;
    }

    return ret;
}

int cmd_q_add(cmd_q_t *self, void *cmd_ptr, size_t len)
{
    int ret = -1;
    cmd_item_t *item_cmd = NULL;

    if (cmd_ptr && self) {
        pthread_mutex_lock(&self->list_lock);
        if (lp_list_number(&self->cmd_list) > MAX_Q_SIZE) {
            pthread_mutex_unlock(&self->list_lock);
            goto exit;
        }
        pthread_mutex_unlock(&self->list_lock);

        item_cmd = (cmd_item_t *)calloc(1, sizeof(cmd_item_t));
        if (item_cmd) {
            item_cmd->data = calloc(1, len + 1);
            if (item_cmd->data) {
                memcpy(item_cmd->data, cmd_ptr, len);
                pthread_mutex_lock(&self->list_lock);
                lp_list_add(&item_cmd->cmd_item, &self->cmd_list);
                pthread_mutex_unlock(&self->list_lock);
                ret = 0;
            } else {
                free(item_cmd);
            }
        }
    }

exit:

    return ret;
}

int cmd_q_add_tail(cmd_q_t *self, void *cmd_ptr, size_t len)
{
    int ret = -1;
    cmd_item_t *item_cmd = NULL;

    if (cmd_ptr && self) {
        pthread_mutex_lock(&self->list_lock);
        if (lp_list_number(&self->cmd_list) > MAX_Q_SIZE) {
            pthread_mutex_unlock(&self->list_lock);
            goto exit;
        }
        pthread_mutex_unlock(&self->list_lock);

        item_cmd = (cmd_item_t *)calloc(1, sizeof(cmd_item_t));
        if (item_cmd) {
            item_cmd->data = calloc(1, len + 1);
            if (item_cmd->data) {
                memcpy(item_cmd->data, cmd_ptr, len);
                pthread_mutex_lock(&self->list_lock);
                lp_list_add_tail(&item_cmd->cmd_item, &self->cmd_list);
                pthread_mutex_unlock(&self->list_lock);
                ret = 0;
            } else {
                free(item_cmd);
            }
        }
    }

exit:

    return ret;
}

int cmd_q_replace(cmd_q_t *self, void *cmd_ptr, size_t len)
{
    int ret = -1;
    int rv = -1;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    cmd_item_t *item_cmd = NULL;
    void *replace_data = NULL;

    if (cmd_ptr && self && self->replace_cb) {
        pthread_mutex_lock(&self->list_lock);
        if (!lp_list_empty(&self->cmd_list)) {
            lp_list_for_each_safe(pos, npos, &self->cmd_list)
            {
                item_cmd = lp_list_entry(pos, cmd_item_t, cmd_item);
                if (item_cmd) {
                    rv = self->replace_cb(cmd_ptr, item_cmd->data);
                    if (rv == 0) {
                        replace_data = calloc(1, len + 1);
                        if (replace_data) {
                            memcpy(replace_data, cmd_ptr, len);
                            free(item_cmd->data);
                            item_cmd->data = replace_data;
                            ret = 0;
                        }
                        break;
                    }
                }
            }
        }
        pthread_mutex_unlock(&self->list_lock);
    }

    return ret;
}

int cmd_q_remove(cmd_q_t *self, void *reasion, remove_cb_t remove_cb)
{
    int ret = -1;
    int rv = -1;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    cmd_item_t *item_cmd = NULL;

    if (self && remove_cb) {
        pthread_mutex_lock(&self->list_lock);
        if (!lp_list_empty(&self->cmd_list)) {
            lp_list_for_each_safe(pos, npos, &self->cmd_list)
            {
                item_cmd = lp_list_entry(pos, cmd_item_t, cmd_item);
                if (item_cmd) {
                    rv = remove_cb(reasion, item_cmd->data);
                    if (rv == 0) {
                        lp_list_del(&item_cmd->cmd_item);
                        free(item_cmd->data);
                        free(item_cmd);
                    }
                }
            }
        }
        pthread_mutex_unlock(&self->list_lock);
    }

    return ret;
}

void *cmd_q_pop(cmd_q_t *self)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    cmd_item_t *item_cmd = NULL;
    void *cmd_data = NULL;

    if (self) {
        pthread_mutex_lock(&self->list_lock);
        if (!lp_list_empty(&self->cmd_list)) {
            lp_list_for_each_safe(pos, npos, &self->cmd_list)
            {
                item_cmd = lp_list_entry(pos, cmd_item_t, cmd_item);
                if (item_cmd) {
                    lp_list_del(&item_cmd->cmd_item);
                    cmd_data = item_cmd->data;
                    free(item_cmd);
                    break;
                }
            }
        }
        pthread_mutex_unlock(&self->list_lock);
    }

    return cmd_data;
}

int cmd_free(void **pop_data)
{
    if (*pop_data) {
        free(*pop_data);
        *pop_data = NULL;
        return 0;
    }

    return -1;
}

int cmd_q_clear(cmd_q_t *self)
{
    int ret = -1;
    void *data = NULL;

    if (self) {
        while (1) {
            data = cmd_q_pop(self);
            if (!data) {
                ret = 0;
                break;
            }
            cmd_free(&data);
        }
    }

    return ret;
}

int replace_test(void *cmd_src, void *cmd_dst)
{
    int ret = -1;

    if (cmd_src && cmd_dst) {
        if (!memcmp(cmd_dst, "test3", strlen("test3"))) {
            ret = 0;
        }
    }

    return ret;
}

int remove_test(void *reasion, void *cmd_dst)
{
    int ret = -1;

    if (cmd_dst && reasion) {
        if (!memcmp(cmd_dst, reasion, strlen((char *)reasion))) {
            ret = 0;
        }
    }

    return ret;
}

#ifdef X86
int main(int argc, const char **argv)
#else
int cmd_q_test(void)
#endif
{
    cmd_q_t *cmd_q = cmd_q_init(replace_test);

    cmd_q_add(cmd_q, "test1", strlen("test1"));
    cmd_q_add(cmd_q, "test2", strlen("test2"));
    cmd_q_add(cmd_q, "test3", strlen("test3"));
    cmd_q_add(cmd_q, "test4", strlen("test4"));
    cmd_q_add_tail(cmd_q, "test5", strlen("test5"));
    cmd_q_replace(cmd_q, "test7", strlen("test7"));
    cmd_q_remove(cmd_q, "test4", remove_test);

    void *data = NULL;
    while (1) {
        data = cmd_q_pop(cmd_q);
        if (!data) {
            break;
        }
        printf("1 pop data is: %s \n", (char *)data);
        cmd_free(&data);
    }

    cmd_q_add_tail(cmd_q, "test1", strlen("test1"));
    cmd_q_add_tail(cmd_q, "test2", strlen("test2"));
    cmd_q_add(cmd_q, "test3", strlen("test3"));
    cmd_q_add(cmd_q, "test4", strlen("test4"));
    cmd_q_add_tail(cmd_q, "test5", strlen("test5"));

    while (1) {
        data = cmd_q_pop(cmd_q);
        if (!data) {
            break;
        }
        printf("2 pop data is: %s \n", (char *)data);
        cmd_free(&data);
    }

    cmd_q_uninit(&cmd_q);
    return 0;
}
