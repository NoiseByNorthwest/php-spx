/* SPX - A simple profiler for PHP
 * Copyright (C) 2017-2022 Sylvain Lassaut <NoiseByNorthwest@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <time.h>

#include <pthread.h>

#include "spx_profiler_sampler.h"
#include "spx_utils.h"


#define STACK_CAPACITY 2048


typedef struct {
    spx_profiler_t base;
    spx_profiler_t * sampled_profiler;

    size_t sampling_period_us;

    struct {
        pthread_t thread;
        int ready;
        int stop;
    } heartbeat;

    struct {
        struct {
            size_t size;
            spx_php_function_t frames[STACK_CAPACITY];
        } previous, current;
    } stack;
} sampling_profiler_t;


static void * sampling_profiler_heartbeat_handler(void * arg);

static void sampling_profiler_call_start(spx_profiler_t * base_profiler, const spx_php_function_t * function);
static void sampling_profiler_call_end(spx_profiler_t * base_profiler);
static void sampling_profiler_handle_sample(sampling_profiler_t * profiler, int call_end);

static void sampling_profiler_finalize(spx_profiler_t * base_profiler);
static void sampling_profiler_destroy(spx_profiler_t * base_profiler);


spx_profiler_t * spx_profiler_sampler_create(
    spx_profiler_t * sampled_profiler,
    size_t sampling_period_us
) {
    if (sampling_period_us < 1) {
        spx_utils_die("sampling_period_us must be greater than zero");
    }

    sampling_profiler_t * profiler = malloc(sizeof(*profiler));
    if (!profiler) {
        goto error;
    }

    profiler->heartbeat.ready = 1;
    profiler->heartbeat.stop = 0;
    if (
        pthread_create(
            &profiler->heartbeat.thread,
            NULL,
            sampling_profiler_heartbeat_handler,
            profiler
        ) != 0
    ) {
        goto error;
    }

    profiler->base.call_start = sampling_profiler_call_start;
    profiler->base.call_end = sampling_profiler_call_end;
    profiler->base.finalize = sampling_profiler_finalize;
    profiler->base.destroy = sampling_profiler_destroy;

    profiler->sampled_profiler = sampled_profiler;
    profiler->sampling_period_us = sampling_period_us;

    profiler->stack.previous.size = 0;
    profiler->stack.current.size = 0;

    return (spx_profiler_t *) profiler;

error:
    free(profiler);

    return NULL;
}


static void * sampling_profiler_heartbeat_handler(void * arg)
{
    sampling_profiler_t * profiler = arg;
    struct timespec period;

    period.tv_sec = profiler->sampling_period_us / (1000 * 1000);
    period.tv_nsec = (profiler->sampling_period_us % (1000 * 1000)) * 1000;

    while (1) {
        if (__atomic_load_n(&profiler->heartbeat.stop, __ATOMIC_SEQ_CST)) {
            break;
        }

        nanosleep(&period, NULL);
        __atomic_store_n(&profiler->heartbeat.ready, 1, __ATOMIC_SEQ_CST);
    }

    return NULL;
}

static void sampling_profiler_call_start(spx_profiler_t * base_profiler, const spx_php_function_t * function)
{
    sampling_profiler_t * profiler = (sampling_profiler_t *) base_profiler;

    if (profiler->stack.current.size == STACK_CAPACITY) {
        spx_utils_die("STACK_CAPACITY exceeded");
    }

    profiler->stack.current.frames[profiler->stack.current.size] = *function;
    profiler->stack.current.size++;

    sampling_profiler_handle_sample(profiler, 0);
}

static void sampling_profiler_call_end(spx_profiler_t * base_profiler)
{
    sampling_profiler_t * profiler = (sampling_profiler_t *) base_profiler;

    sampling_profiler_handle_sample(profiler, 1);

    profiler->stack.current.size--;
}

static void sampling_profiler_handle_sample(sampling_profiler_t * profiler, int call_end)
{
    if (!__atomic_load_n(&profiler->heartbeat.ready, __ATOMIC_SEQ_CST)) {
        return;
    }

    __atomic_store_n(&profiler->heartbeat.ready, 0, __ATOMIC_SEQ_CST);

    int common_stack_top = 0;
    while (1) {
        if (
            common_stack_top >= profiler->stack.previous.size
            || common_stack_top >= profiler->stack.current.size
        ) {
            break;
        }

        if (
            profiler->stack.previous.frames[common_stack_top].hash_code
                != profiler->stack.current.frames[common_stack_top].hash_code
        ) {
            break;
        }

        /*
            We cannot check the function & class name as the previous call stack related pointers
            may be invalid. This is however not a big issue, aliased functions will only slighlty
            reduce the correctness of the report.
         */

        common_stack_top++;
    }

    int i;

    /* end all previous stack calls down to common stack */

    if (profiler->stack.previous.size > 0) {
        for (i = profiler->stack.previous.size - 1; i >= common_stack_top; i--) {
            profiler->sampled_profiler->call_end(profiler->sampled_profiler);
        }
    }

    /* start all current stack calls (including current top) from common stack */

    for (i = common_stack_top; i < profiler->stack.current.size; i++) {
        profiler->sampled_profiler->call_start(profiler->sampled_profiler, &profiler->stack.current.frames[i]);
    }

    /* copy the current stack to the previous (for next sample) */

    for (i = 0; i < profiler->stack.current.size; i++) {
        profiler->stack.previous.frames[i] = profiler->stack.current.frames[i];
    }

    profiler->stack.previous.size = profiler->stack.current.size;

    if (call_end) {
        profiler->sampled_profiler->call_end(profiler->sampled_profiler);
        /* so we have to remove this frame from the future previous stack */
        profiler->stack.previous.size--;
    }
}

static void sampling_profiler_finalize(spx_profiler_t * base_profiler)
{
    sampling_profiler_t * profiler = (sampling_profiler_t *) base_profiler;

    profiler->sampled_profiler->finalize(profiler->sampled_profiler);
}

static void sampling_profiler_destroy(spx_profiler_t * base_profiler)
{
    sampling_profiler_t * profiler = (sampling_profiler_t *) base_profiler;

    __atomic_store_n(&profiler->heartbeat.stop, 1, __ATOMIC_SEQ_CST);
    pthread_join(profiler->heartbeat.thread, NULL);

    profiler->sampled_profiler->destroy(profiler->sampled_profiler);

    free(profiler);
}
