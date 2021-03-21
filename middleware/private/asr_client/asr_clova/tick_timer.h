/**
 *
 *
 *
 *
 *
 */

#ifndef __TICK_TIMER_H__
#define __TICK_TIMER_H__

#include <stdio.h>
#include <string.h>

typedef unsigned long long ms_tick_t;
typedef int(do_jobs_cb_t)(void *ctx);
typedef struct _tick_timer_s tick_timer_t;

typedef struct _tick_job_s {
    int job_id;
    int repeat_count;
    ms_tick_t start_time;
    ms_tick_t period_time;
    do_jobs_cb_t *do_job;
    void *job_ctx;
} tick_job_t;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * create the tick_time_t obj
 */
tick_timer_t *tick_timer_create(void);

/*
 * destroy the tick_time_t obj
 */
void tick_timer_destroy(tick_timer_t **self_p);

/*
 * add/update timer a job
 */
int tick_timer_add_item(tick_timer_t *self, tick_job_t *new_job);

/*
 * delete a timer job
 */
int tick_timer_del_item(tick_timer_t *self, int job_id);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_PROCESSOR_H */
