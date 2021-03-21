
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#include "wm_util.h"
#include "lp_list.h"

#include "tick_timer.h"

#define os_calloc(n, size) calloc((n), (size))

#define os_free(ptr) ((ptr != NULL) ? free(ptr) : (void)ptr)

#define os_power_on_tick() tickSincePowerOn()

#define TICK_DEBUG(fmt, ...)                                                                       \
    do {                                                                                           \
        fprintf(stderr, "[%02d:%02d:%02d.%03d][TICK TIMER] " fmt,                                  \
                (int)(os_power_on_tick() / 3600000), (int)(os_power_on_tick() % 3600000 / 60000),  \
                (int)(os_power_on_tick() % 60000 / 1000), (int)(os_power_on_tick() % 1000),        \
                ##__VA_ARGS__);                                                                    \
    } while (0)

typedef struct _tick_item_s {
    tick_job_t *job;
    struct lp_list_head job_item;
} tick_item_t;

struct _tick_timer_s {
    int is_running;
    pthread_t timer_proc;
    pthread_mutex_t list_lock;
    struct lp_list_head job_list;
};

static void tick_sleep(unsigned seconds)
{
    int err = 0;
    struct timeval tv;

    tv.tv_sec = seconds;
    tv.tv_usec = 0;

    do {
        err = select(0, NULL, NULL, NULL, &tv);
    } while (err < 0 && errno == EINTR);
}

static void tick_msleep(unsigned long mSec)
{
    int err = 0;
    struct timeval tv;

    tv.tv_sec = mSec / 1000;
    tv.tv_usec = (mSec % 1000) * 1000;

    do {
        err = select(0, NULL, NULL, NULL, &tv);
    } while (err < 0 && errno == EINTR);
}

static void tick_usleep(unsigned long uSec)
{
    int err = 0;
    struct timeval tv;

    tv.tv_sec = uSec / 1000000;
    tv.tv_usec = uSec % 1000000;

    do {
        err = select(0, NULL, NULL, NULL, &tv);
    } while (err < 0 && errno == EINTR);
}

tick_item_t *job_is_exist(tick_timer_t *self, tick_job_t *new_job)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    tick_item_t *tick_item = NULL;

    if (self && new_job) {
        pthread_mutex_lock(&self->list_lock);
        if (!lp_list_empty(&self->job_list)) {
            lp_list_for_each_safe(pos, npos, &self->job_list)
            {
                tick_item = lp_list_entry(pos, tick_item_t, job_item);
                if (tick_item && tick_item->job && tick_item->job->job_id == new_job->job_id) {
                    memcpy(tick_item->job, new_job, sizeof(tick_job_t));
                    break;
                } else {
                    tick_item = NULL;
                }
            }
        }
        pthread_mutex_unlock(&self->list_lock);
    }

    return tick_item;
}

void job_clean(tick_timer_t *self)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    tick_item_t *tick_item = NULL;

    if (self) {
        pthread_mutex_lock(&self->list_lock);
        if (!lp_list_empty(&self->job_list)) {
            lp_list_for_each_safe(pos, npos, &self->job_list)
            {
                tick_item = lp_list_entry(pos, tick_item_t, job_item);
                if (tick_item) {
                    lp_list_del(&tick_item->job_item);
                    if (tick_item->job) {
                        os_free(tick_item->job);
                    }
                    os_free(tick_item);
                }
            }
        }
        pthread_mutex_unlock(&self->list_lock);
    }
}

void job_check_start(tick_timer_t *self, ms_tick_t cur_tick)
{
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    tick_item_t *tick_item = NULL;
    do_jobs_cb_t *do_job = NULL;
    void *job_ctx = NULL;

    if (self) {
        pthread_mutex_lock(&self->list_lock);
        if (!lp_list_empty(&self->job_list)) {
            lp_list_for_each_safe(pos, npos, &self->job_list)
            {
                tick_item = lp_list_entry(pos, tick_item_t, job_item);
                if (tick_item && tick_item->job && cur_tick >= tick_item->job->start_time) {
                    do_job = tick_item->job->do_job;
                    job_ctx = tick_item->job->job_ctx;
                    if (tick_item->job->need_repeat) {
                        tick_item->job->start_time = cur_tick + tick_item->job->period_time;
                    } else {
                        lp_list_del(&tick_item->job_item);
                        if (tick_item->job) {
                            os_free(tick_item->job);
                        }
                        os_free(tick_item);
                    }
                    break;
                }
            }
        }
        pthread_mutex_unlock(&self->list_lock);
    }

    if (do_job) {
        do_job(job_ctx);
    }
}

void *tick_timer_proc(void *args)
{
    int count = 20;
    struct timeval tv;
    tick_timer_t *self = (tick_timer_t *)args;
    ms_tick_t cur_tick = os_power_on_tick();

    while (self->is_running) {
        tv.tv_sec = 0;
        tv.tv_usec = 200 * 1000; // 200ms

        int ret = select(0, NULL, NULL, NULL, &tv);
        switch (ret) {
        case -1:
            TICK_DEBUG("Error!\n");
            break;
        case 0:
            if (count == 0) {
                // TICK_DEBUG("timeout expires.\n");
                count = 50;
            } else {
                count--;
            }
            cur_tick = os_power_on_tick();
            job_check_start(self, cur_tick);
            break;
        default:
            TICK_DEBUG("default\n");
            break;
        }
    }

    return NULL;
}

/*
 *
 */
tick_timer_t *tick_timer_create(void)
{
    int ret = -1;
    tick_timer_t *self = NULL;

    self = os_calloc(1, sizeof(tick_timer_t));
    if (self) {
        self->is_running = 1;
        LP_INIT_LIST_HEAD(&self->job_list);
        /*
         * create a thread to do the job
         */
        ret = pthread_create(&self->timer_proc, NULL, tick_timer_proc, (void *)self);
        if (ret) {
            TICK_DEBUG("pthread_create tick_timer_proc failed\n");
            goto Exit;
        }
        return self;
    }

Exit:
    if (self) {
        os_free(self);
    }

    return NULL;
}

/*
 *
 */
void tick_timer_destroy(tick_timer_t **self_p)
{
    tick_timer_t *self = *self_p;
    if (self) {
        job_clean(self);
        self->is_running = 0;
        if (self->timer_proc > 0) {
            pthread_join(self->timer_proc, NULL);
        }
        os_free(self);
        *self_p = NULL;
    }
}

/*
 *
 */
int tick_timer_add_item(tick_timer_t *self, tick_job_t *new_job)
{
    int ret = -1;
    tick_item_t *tick_item = NULL;

    if (self && new_job) {
        tick_item = job_is_exist(self, new_job);
        if (tick_item == NULL) {
            tick_item = (tick_item_t *)os_calloc(1, sizeof(tick_item_t));
            if (tick_item) {
                tick_item->job = (tick_job_t *)os_calloc(1, sizeof(tick_job_t));
                if (tick_item->job) {
                    memcpy(tick_item->job, new_job, sizeof(tick_job_t));
                    pthread_mutex_lock(&self->list_lock);
                    lp_list_add(&tick_item->job_item, &self->job_list);
                    pthread_mutex_unlock(&self->list_lock);
                    ret = 0;
                } else {
                    os_free(tick_item);
                }
            }
        } else {
            ret = 0;
        }
    }

    return ret;
}

/*
 * delete a job
 */
int tick_timer_del_item(tick_timer_t *self, int job_id)
{
    int ret = -1;
    struct lp_list_head *pos = NULL;
    struct lp_list_head *npos = NULL;
    tick_item_t *tick_item = NULL;

    if (self) {
        pthread_mutex_lock(&self->list_lock);
        if (!lp_list_empty(&self->job_list)) {
            lp_list_for_each_safe(pos, npos, &self->job_list)
            {
                tick_item = lp_list_entry(pos, tick_item_t, job_item);
                if (tick_item && tick_item->job && tick_item->job->job_id == job_id) {
                    lp_list_del(&tick_item->job_item);
                    if (tick_item->job) {
                        os_free(tick_item->job);
                    }
                    os_free(tick_item);
                    ret = 0;
                    break;
                } else {
                    tick_item = NULL;
                }
            }
        }
        pthread_mutex_unlock(&self->list_lock);
    }

    return ret;
}
