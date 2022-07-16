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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "spx_hmap.h"
#include "spx_fmt.h"
#include "spx_profiler_tracer.h"
#include "spx_resource_stats.h"
#include "spx_utils.h"


#define STACK_CAPACITY 2048
#define FUNC_TABLE_CAPACITY 65536

#define METRIC_VALUES_ZERO(m)                \
do {                                         \
    SPX_METRIC_FOREACH(i, {                  \
        (m).values[i] = 0;                   \
    });                                      \
} while (0)

#define METRIC_VALUES_ADD(a, b)              \
do {                                         \
    SPX_METRIC_FOREACH(i, {                  \
        (a).values[i] += (b).values[i];      \
    });                                      \
} while (0)

#define METRIC_VALUES_SUB(a, b)              \
do {                                         \
    SPX_METRIC_FOREACH(i, {                  \
        (a).values[i] -= (b).values[i];      \
    });                                      \
} while (0)

#define METRIC_VALUES_MAX(a, b)              \
do {                                         \
    SPX_METRIC_FOREACH(i, {                  \
        (a).values[i] =                      \
            (a).values[i] > (b).values[i] ?  \
                (a).values[i] :              \
                (b).values[i]                \
        ;                                    \
    });                                      \
} while (0)

typedef struct {
    spx_hmap_t * hmap;
    size_t size;
    spx_profiler_func_table_entry_t entries[FUNC_TABLE_CAPACITY];
} func_table_t;

typedef struct {
    spx_profiler_func_table_entry_t * func_table_entry;
    spx_profiler_metric_values_t start_metric_values;
    spx_profiler_metric_values_t children_metric_values;
} stack_frame_t;

typedef struct {
    spx_profiler_t base;

    int finalized;
    int active;

    int enabled_metrics[SPX_METRIC_COUNT];
    spx_metric_collector_t * metric_collector;

    int calibrated;
    spx_profiler_metric_values_t call_start_noise;
    spx_profiler_metric_values_t call_end_noise;

    spx_profiler_reporter_t * reporter;

    size_t max_depth;
    size_t called;

    spx_profiler_metric_values_t first_metric_values;
    spx_profiler_metric_values_t last_metric_values;
    spx_profiler_metric_values_t cum_metric_values;
    spx_profiler_metric_values_t max_metric_values;

    struct {
        size_t depth;
        stack_frame_t frames[STACK_CAPACITY];
    } stack;

    func_table_t func_table;
} tracing_profiler_t;


static void tracing_profiler_call_start(spx_profiler_t * base_profiler, const spx_php_function_t * function);
static void tracing_profiler_call_end(spx_profiler_t * base_profiler);

static void tracing_profiler_finalize(spx_profiler_t * base_profiler);
static void tracing_profiler_destroy(spx_profiler_t * base_profiler);

static spx_profiler_reporter_cost_t null_reporter_notify(
    spx_profiler_reporter_t * reporter,
    const spx_profiler_event_t * event
);

static void calibrate(tracing_profiler_t * profiler, const spx_php_function_t * function);

static unsigned long func_table_hmap_hash_key(const void * v);
static int func_table_hmap_cmp_key(const void * va, const void * vb);

static spx_profiler_func_table_entry_t * func_table_get_entry(
    func_table_t * func_table,
    const spx_php_function_t * function
);

static void func_table_reset(func_table_t * func_table);

static void fill_event(
    spx_profiler_event_t * event,
    const tracing_profiler_t * profiler,
    spx_profiler_event_type_t type,
    const spx_profiler_func_table_entry_t * caller,
    const spx_profiler_func_table_entry_t * callee,
    const spx_profiler_metric_values_t * inc,
    const spx_profiler_metric_values_t * exc
);

spx_profiler_t * spx_profiler_tracer_create(
    size_t max_depth,
    const int * enabled_metrics,
    spx_profiler_reporter_t * reporter
) {
    tracing_profiler_t * profiler = malloc(sizeof(*profiler));
    if (!profiler) {
        goto error;
    }

    profiler->base.call_start = tracing_profiler_call_start;
    profiler->base.call_end = tracing_profiler_call_end;
    profiler->base.finalize = tracing_profiler_finalize;
    profiler->base.destroy = tracing_profiler_destroy;

    profiler->finalized = 0;
    profiler->active = 1;

    profiler->reporter = reporter;

    SPX_METRIC_FOREACH(i, {
        profiler->enabled_metrics[i] = enabled_metrics[i];
    });

    profiler->metric_collector = NULL;

    profiler->calibrated = 0;
    METRIC_VALUES_ZERO(profiler->call_start_noise);
    METRIC_VALUES_ZERO(profiler->call_end_noise);

    profiler->max_depth = max_depth > 0 && max_depth < STACK_CAPACITY ? max_depth : STACK_CAPACITY;
    profiler->called = 0;

    profiler->stack.depth = 0;
    profiler->func_table.size = 0;
    profiler->func_table.hmap = NULL;

    profiler->metric_collector = spx_metric_collector_create(profiler->enabled_metrics);
    if (!profiler->metric_collector) {
        goto error;
    }

    profiler->func_table.hmap = spx_hmap_create(
        FUNC_TABLE_CAPACITY,
        func_table_hmap_hash_key,
        func_table_hmap_cmp_key
    );

    if (!profiler->func_table.hmap) {
        goto error;
    }

    return (spx_profiler_t *) profiler;

error:
    if (profiler) {
        tracing_profiler_destroy((spx_profiler_t *) profiler);
    }

    return NULL;
}

static void tracing_profiler_call_start(spx_profiler_t * base_profiler, const spx_php_function_t * function)
{
    tracing_profiler_t * profiler = (tracing_profiler_t *) base_profiler;

    if (profiler->finalized) {
        return;
    }

    if (!profiler->active) {
        if (profiler->stack.depth == STACK_CAPACITY) {
            fprintf(stderr, "SPX: STACK_CAPACITY (%d) exceeded\n", STACK_CAPACITY);
        }

        goto end;
    }

    if (profiler->called == 0 && !profiler->calibrated) {
        calibrate(profiler, function);
    }

    spx_profiler_metric_values_t cur_metric_values;

    spx_metric_collector_collect(
        profiler->metric_collector,
        cur_metric_values.values
    );

    if (profiler->called == 0) {
        profiler->first_metric_values = cur_metric_values;
        profiler->max_metric_values = cur_metric_values;
    }

    profiler->last_metric_values = cur_metric_values;
    profiler->cum_metric_values = cur_metric_values;
    METRIC_VALUES_SUB(profiler->cum_metric_values, profiler->first_metric_values);
    METRIC_VALUES_MAX(profiler->max_metric_values, cur_metric_values);

    profiler->called++;

    stack_frame_t * frame = &profiler->stack.frames[profiler->stack.depth];
    frame->func_table_entry = func_table_get_entry(
        &profiler->func_table,
        function
    );

    if (!frame->func_table_entry) {
        goto end;
    }

    frame->start_metric_values = cur_metric_values;
    METRIC_VALUES_ZERO(frame->children_metric_values);

    spx_profiler_event_t event;
    fill_event(
        &event,
        profiler,
        SPX_PROFILER_EVENT_CALL_START,
        profiler->stack.depth > 0 ?
            profiler->stack.frames[profiler->stack.depth - 1].func_table_entry : NULL
        ,
        profiler->stack.frames[profiler->stack.depth].func_table_entry,
        NULL,
        NULL
    );

    if (profiler->reporter->notify(profiler->reporter, &event) == SPX_PROFILER_REPORTER_COST_HEAVY) {
        spx_metric_collector_noise_barrier(profiler->metric_collector);
    }

    spx_metric_collector_add_fixed_noise(profiler->metric_collector, profiler->call_start_noise.values);

end:
    profiler->stack.depth++;

    profiler->active = profiler->stack.depth < profiler->max_depth;
}

static void tracing_profiler_call_end(spx_profiler_t * base_profiler)
{
    tracing_profiler_t * profiler = (tracing_profiler_t *) base_profiler;

    if (profiler->finalized) {
        return;
    }

    if (profiler->stack.depth == 0) {
        spx_utils_die("Cannot rewind below 0 depth");
    }

    profiler->stack.depth--;

    profiler->active = profiler->stack.depth < profiler->max_depth;

    if (!profiler->active) {
        return;
    }

    spx_profiler_metric_values_t cur_metric_values;

    spx_metric_collector_collect(
        profiler->metric_collector,
        cur_metric_values.values
    );

    profiler->last_metric_values = cur_metric_values;
    profiler->cum_metric_values = cur_metric_values;
    METRIC_VALUES_SUB(profiler->cum_metric_values, profiler->first_metric_values);
    METRIC_VALUES_MAX(profiler->max_metric_values, cur_metric_values);

    stack_frame_t * frame = &profiler->stack.frames[profiler->stack.depth];
    if (!frame->func_table_entry) {
        return;
    }

    spx_profiler_func_table_entry_t * entry = frame->func_table_entry;

    spx_profiler_metric_values_t inc_metric_values = cur_metric_values;
    METRIC_VALUES_SUB(inc_metric_values, frame->start_metric_values);

    spx_profiler_metric_values_t exc_metric_values = inc_metric_values;
    METRIC_VALUES_SUB(exc_metric_values, frame->children_metric_values);

    size_t cycle_depth = 0;
    int i;
    for (i = profiler->stack.depth - 1; i >= 0; i--) {
        stack_frame_t * parent_frame = &profiler->stack.frames[i];
        if (!parent_frame->func_table_entry) {
            continue;
        }

        if (i == profiler->stack.depth - 1) {
            METRIC_VALUES_ADD(parent_frame->children_metric_values, inc_metric_values);
        }

        if (parent_frame->func_table_entry == entry) {
            cycle_depth++;

            if (cycle_depth == 1) {
                METRIC_VALUES_SUB(parent_frame->children_metric_values, exc_metric_values);
            }
        }
    }

    entry->stats.called++;
    if (entry->stats.max_cycle_depth < cycle_depth) {
        entry->stats.max_cycle_depth = cycle_depth;
    }

    if (cycle_depth == 0) {
        METRIC_VALUES_ADD(entry->stats.inc, inc_metric_values);
        METRIC_VALUES_ADD(entry->stats.exc, exc_metric_values);
    }

    spx_profiler_event_t event;
    fill_event(
        &event,
        profiler,
        SPX_PROFILER_EVENT_CALL_END,
        profiler->stack.depth > 0 ?
            profiler->stack.frames[profiler->stack.depth - 1].func_table_entry : NULL
        ,
        profiler->stack.frames[profiler->stack.depth].func_table_entry,
        &inc_metric_values,
        &exc_metric_values
    );

    if (profiler->reporter->notify(profiler->reporter, &event) == SPX_PROFILER_REPORTER_COST_HEAVY) {
        spx_metric_collector_noise_barrier(profiler->metric_collector);
    }

    spx_metric_collector_add_fixed_noise(profiler->metric_collector, profiler->call_end_noise.values);
}

static void tracing_profiler_finalize(spx_profiler_t * base_profiler)
{
    tracing_profiler_t * profiler = (tracing_profiler_t *) base_profiler;

    /*
     *  Explicit remaining stack frames unwinding
     */
    profiler->active = 1;
    while (profiler->stack.depth > 0) {
        tracing_profiler_call_end((spx_profiler_t *) profiler);
    }

    profiler->finalized = 1;

    spx_profiler_event_t event;
    fill_event(&event, profiler, SPX_PROFILER_EVENT_FINALIZE, NULL, NULL, NULL, NULL);
    profiler->reporter->notify(profiler->reporter, &event);
}

static void tracing_profiler_destroy(spx_profiler_t * base_profiler)
{
    tracing_profiler_t * profiler = (tracing_profiler_t *) base_profiler;

    if (profiler->metric_collector) {
        spx_metric_collector_destroy(profiler->metric_collector);
    }

    func_table_reset(&profiler->func_table);

    if (profiler->func_table.hmap) {
        spx_hmap_destroy(profiler->func_table.hmap);
    }

    free(profiler);
}

static spx_profiler_reporter_cost_t null_reporter_notify(
    spx_profiler_reporter_t * reporter,
    const spx_profiler_event_t * event
) {
    return SPX_PROFILER_REPORTER_COST_LIGHT;
}

static void calibrate(tracing_profiler_t * profiler, const spx_php_function_t * function)
{
    profiler->calibrated = 1;

    spx_profiler_reporter_t null_reporter = {
        null_reporter_notify,
        NULL
    };

    spx_profiler_reporter_t * const orig_reporter = profiler->reporter;
    profiler->reporter = &null_reporter;

    const size_t iter_count = 50000;
    int i;
    size_t start, avg_noise;

    start = spx_resource_stats_cpu_time();

    for (i = 0; i < iter_count; i++) {
        tracing_profiler_call_start((spx_profiler_t *) profiler, function);
        if (profiler->stack.depth > 5) {
            profiler->stack.depth = 5;
        }
    }

    avg_noise = (spx_resource_stats_cpu_time() - start) / iter_count;

    profiler->call_start_noise.values[SPX_METRIC_WALL_TIME] = avg_noise;
    profiler->call_start_noise.values[SPX_METRIC_CPU_TIME] = avg_noise;

    start = spx_resource_stats_cpu_time();

    for (i = 0; i < iter_count; i++) {
        tracing_profiler_call_end((spx_profiler_t *) profiler);
        profiler->stack.depth++;
    }

    avg_noise = (spx_resource_stats_cpu_time() - start) / iter_count;

    profiler->call_end_noise.values[SPX_METRIC_WALL_TIME] = avg_noise;
    profiler->call_end_noise.values[SPX_METRIC_CPU_TIME] = avg_noise;

    profiler->reporter = orig_reporter;
    profiler->called = 0;
    profiler->stack.depth = 0;
    func_table_reset(&profiler->func_table);
}

static unsigned long func_table_hmap_hash_key(const void * v)
{
    return ((const spx_php_function_t *) v)->hash_code;
}

static int func_table_hmap_cmp_key(const void * va, const void * vb)
{
    const spx_php_function_t * a = va;
    const spx_php_function_t * b = vb;

    int n;

    n = strcmp(a->func_name, b->func_name);
    if (n != 0) {
        return n;
    }

    n = strcmp(a->class_name, b->class_name);
    if (n != 0) {
        return n;
    }

    return 0;
}

static spx_profiler_func_table_entry_t * func_table_get_entry(
    func_table_t * func_table,
    const spx_php_function_t * function
) {
    if (func_table->size == FUNC_TABLE_CAPACITY) {
        return spx_hmap_get_value(func_table->hmap, function);
    }

    int new = 0;
    spx_hmap_entry_t * hmap_entry = spx_hmap_ensure_entry(
        func_table->hmap,
        function,
        &new
    );

    if (!hmap_entry) {
        spx_utils_die("Function table hash index failure\n");
    }

    if (!new) {
        return spx_hmap_entry_get_value(hmap_entry);
    }

    func_table->size++;
    if (func_table->size == FUNC_TABLE_CAPACITY) {
        fprintf(stderr, "SPX: FUNC_TABLE_CAPACITY (%d) reached\n", FUNC_TABLE_CAPACITY);
    }

    const size_t idx = func_table->size - 1;
    spx_profiler_func_table_entry_t * entry = &func_table->entries[idx];

    entry->idx = idx;
    entry->function = *function;

    /*
     *  Review needed: workaround for a lifespan issue
     *  These allocations should be useless in many cases...
     */
    entry->function.func_name = strdup(entry->function.func_name);
    entry->function.class_name = strdup(entry->function.class_name);
    if (
        !entry->function.func_name
        || !entry->function.class_name
    ) {
        spx_utils_die("Cannot dup function / class name\n");
    }

    entry->stats.called = 0;
    entry->stats.max_cycle_depth = 0;
    METRIC_VALUES_ZERO(entry->stats.inc);
    METRIC_VALUES_ZERO(entry->stats.exc);

    spx_hmap_entry_set_value(hmap_entry, entry);
    spx_hmap_set_entry_key(func_table->hmap, hmap_entry, &entry->function);

    return entry;
}

static void func_table_reset(func_table_t * func_table)
{
    /*
     *  Free duped function/class names, see workaround in func_table_get_entry()
     */
    size_t i;
    for (i = 0; i < func_table->size; i++) {
        spx_profiler_func_table_entry_t * entry = &func_table->entries[i];

        free((char *)entry->function.func_name);
        free((char *)entry->function.class_name);
    }

    func_table->size = 0;
    spx_hmap_reset(func_table->hmap);
}

static void fill_event(
    spx_profiler_event_t * event,
    const tracing_profiler_t * profiler,
    spx_profiler_event_type_t type,
    const spx_profiler_func_table_entry_t * caller,
    const spx_profiler_func_table_entry_t * callee,
    const spx_profiler_metric_values_t * inc,
    const spx_profiler_metric_values_t * exc
) {
    event->type = type;

    event->enabled_metrics = profiler->enabled_metrics;

    event->called = profiler->called;
    event->max = &profiler->max_metric_values;
    event->cum = &profiler->cum_metric_values;

    event->func_table.size = profiler->func_table.size;
    event->func_table.capacity = FUNC_TABLE_CAPACITY;
    event->func_table.entries = profiler->func_table.entries;

    event->depth = profiler->stack.depth;

    event->caller = caller;
    event->callee = callee;

    event->inc = inc;
    event->exc = exc;
}
