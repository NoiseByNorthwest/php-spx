#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "spx_hset.h"
#include "spx_fmt.h"
#include "spx_profiler.h"

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
    spx_hset_t * hset;
    size_t size;
    spx_profiler_func_table_entry_t entries[FUNC_TABLE_CAPACITY];
} func_table_t;

typedef struct {
    spx_profiler_func_table_entry_t * func_table_entry;
    spx_profiler_metric_values_t start_metric_values;
    spx_profiler_metric_values_t children_metric_values;
} stack_frame_t;

typedef struct {
    size_t depth;
    stack_frame_t frames[STACK_CAPACITY];
} spx_stack_t;

struct spx_profiler_t {
    int finalized;
    int active;

    int enabled_metrics[SPX_METRIC_COUNT];
    spx_metric_collector_t * metric_collector;

    spx_profiler_reporter_t * reporter;

    size_t max_depth;
    size_t called;

    spx_profiler_metric_values_t first_metric_values;
    spx_profiler_metric_values_t last_metric_values;
    spx_profiler_metric_values_t cum_metric_values;
    spx_profiler_metric_values_t max_metric_values;

    spx_stack_t stack;
    func_table_t func_table;
};

static unsigned long func_table_entry_hash(const void * v);
static int func_table_entry_cmp(const void * va, const void * vb);

static spx_profiler_func_table_entry_t * func_table_get_entry(
    func_table_t * func_table,
    const spx_php_function_t * function
);

static void fill_event(
    const spx_profiler_t * profiler,
    spx_profiler_event_type_t type,
    const spx_php_function_t * caller,
    const spx_php_function_t * callee,
    const spx_profiler_metric_values_t * inc,
    const spx_profiler_metric_values_t * exc,
    spx_profiler_event_t * event
);

spx_profiler_reporter_t * spx_profiler_reporter_create(
    size_t size,
    spx_output_stream_t * output,
    spx_profiler_reporter_notify_func_t notify,
    spx_profiler_reporter_destroy_func_t destroy
) {
    if (size < sizeof(spx_profiler_reporter_t)) {
        fprintf(stderr, "Invalid reporter size\n");
        exit(1);
    }

    spx_profiler_reporter_t * reporter = malloc(size);
    if (!reporter) {
        spx_output_stream_close(reporter->output);

        return NULL;
    }

    reporter->output = output;
    reporter->notify = notify;
    reporter->destroy = destroy;

    return reporter;
}

void spx_profiler_reporter_destroy(spx_profiler_reporter_t * reporter)
{
    if (reporter->destroy) {
        reporter->destroy(reporter);
    }

    spx_output_stream_close(reporter->output);
    free(reporter);
}

spx_profiler_t * spx_profiler_create(
    size_t max_depth,
    const int * enabled_metrics,
    spx_profiler_reporter_t * reporter
) {
    spx_profiler_t * profiler = malloc(sizeof(*profiler));
    if (!profiler) {
        goto error;
    }

    profiler->finalized = 0;
    profiler->active = 1;

    profiler->reporter = reporter;

    SPX_METRIC_FOREACH(i, {
        profiler->enabled_metrics[i] = enabled_metrics[i];
    });

    profiler->metric_collector = NULL;

    profiler->max_depth = max_depth;
    profiler->called = 0;

    profiler->stack.depth = 0;
    profiler->func_table.size = 0;
    profiler->func_table.hset = NULL;

    profiler->metric_collector = spx_metric_collector_create(profiler->enabled_metrics);
    if (!profiler->metric_collector) {
        goto error;
    }

    profiler->func_table.hset = spx_hset_create(
        FUNC_TABLE_CAPACITY,
        func_table_entry_hash,
        func_table_entry_cmp
    );

    if (!profiler->func_table.hset) {
        goto error;
    }

    return profiler;

error:
    if (profiler) {
        spx_profiler_destroy(profiler);
    } else {
        spx_profiler_reporter_destroy(reporter);
    }

    return NULL;
}

void spx_profiler_destroy(spx_profiler_t * profiler)
{
    spx_profiler_reporter_destroy(profiler->reporter);

    if (profiler->metric_collector) {
        spx_metric_collector_destroy(profiler->metric_collector);
    }

    /*
     *  Free duped function/class names, see workaround in func_table_get_entry()
     */
    size_t i;
    for (i = 0; i < profiler->func_table.size; i++) {
        spx_profiler_func_table_entry_t * entry = &profiler->func_table.entries[i];

        free((char *)entry->function.func_name);
        if (*entry->function.class_name) {
            free((char *)entry->function.class_name);
        }
    }

    if (profiler->func_table.hset) {
        spx_hset_destroy(profiler->func_table.hset);
    }

    free(profiler);
}

void spx_profiler_call_start(
    spx_profiler_t * profiler,
    const spx_php_function_t * function
) {
    if (profiler->finalized) {
        return;
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

    if (
        profiler->stack.depth >= STACK_CAPACITY ||
        (profiler->max_depth > 0 && profiler->stack.depth >= profiler->max_depth)
    ) {
        profiler->active = 0;
        if (profiler->stack.depth == STACK_CAPACITY) {
            fprintf(stderr, "SPX: STACK_CAPACITY (%d) exceeded\n", STACK_CAPACITY);
        }
    } else {
        profiler->active = 1;
    }

    if (!profiler->active) {
        goto end;
    }

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
        profiler,
        SPX_PROFILER_EVENT_CALL_START,
        profiler->stack.depth > 0 ?
            &profiler->stack.frames[profiler->stack.depth - 1].func_table_entry->function : NULL
        ,
        &profiler->stack.frames[profiler->stack.depth].func_table_entry->function,
        NULL,
        NULL,
        &event
    );

    if (profiler->reporter->notify(profiler->reporter, &event) == SPX_PROFILER_REPORTER_COST_HEAVY) {
        spx_metric_collector_noise_barrier(profiler->metric_collector);
    }

end:
    profiler->stack.depth++;
}

void spx_profiler_call_end(spx_profiler_t * profiler)
{
    if (profiler->finalized) {
        return;
    }

    profiler->stack.depth--;

    spx_profiler_metric_values_t cur_metric_values;

    spx_metric_collector_collect(
        profiler->metric_collector,
        cur_metric_values.values
    );

    profiler->last_metric_values = cur_metric_values;
    profiler->cum_metric_values = cur_metric_values;
    METRIC_VALUES_SUB(profiler->cum_metric_values, profiler->first_metric_values);
    METRIC_VALUES_MAX(profiler->max_metric_values, cur_metric_values);

    if (!profiler->active) {
        return;
    }

    stack_frame_t * frame = &profiler->stack.frames[profiler->stack.depth];
    if (!frame->func_table_entry) {
        return;
    }

    spx_profiler_func_table_entry_t * entry = frame->func_table_entry;

    spx_profiler_metric_values_t inc_metric_values = cur_metric_values;
    METRIC_VALUES_SUB(inc_metric_values, frame->start_metric_values);

    spx_profiler_metric_values_t exc_metric_values = inc_metric_values;
    METRIC_VALUES_SUB(exc_metric_values, frame->children_metric_values);

    int i, cyclic = 0;
    for (i = profiler->stack.depth - 1; i >= 0; i--) {
        stack_frame_t * parent_frame = &profiler->stack.frames[i];
        if (!parent_frame->func_table_entry) {
            continue;
        }

        if (parent_frame->func_table_entry == entry) {
            cyclic = 1;

            break;
        }

        METRIC_VALUES_ADD(parent_frame->children_metric_values, exc_metric_values);
    }

    entry->stats.called++;
    if (cyclic) {
        size_t cycle_depth = 0;
        for (i = 0; i < profiler->stack.depth; i++) {
            if (profiler->stack.frames[i].func_table_entry == entry) {
                cycle_depth++;
            }
        }

        if (entry->stats.max_cycle_depth < cycle_depth) {
            entry->stats.max_cycle_depth = cycle_depth;
        }
    } else {
        METRIC_VALUES_ADD(entry->stats.inc, inc_metric_values);
        METRIC_VALUES_ADD(entry->stats.exc, exc_metric_values);
    }

    spx_profiler_event_t event;
    fill_event(
        profiler,
        SPX_PROFILER_EVENT_CALL_END,
        profiler->stack.depth > 0 ?
            &profiler->stack.frames[profiler->stack.depth - 1].func_table_entry->function : NULL
        ,
        &profiler->stack.frames[profiler->stack.depth].func_table_entry->function,
        &inc_metric_values,
        &exc_metric_values,
        &event
    );

    if (profiler->reporter->notify(profiler->reporter, &event) == SPX_PROFILER_REPORTER_COST_HEAVY) {
        spx_metric_collector_noise_barrier(profiler->metric_collector);
    }
}

void spx_profiler_finalize(spx_profiler_t * profiler)
{
    /*
     *  Explicit remaining stack frames unwinding
     */
    profiler->active = 1;
    while (profiler->stack.depth > 0) {
        spx_profiler_call_end(profiler);
    }

    profiler->finalized = 1;

    spx_profiler_event_t event;
    fill_event(profiler, SPX_PROFILER_EVENT_FINALIZE, NULL, NULL, NULL, NULL, &event);
    profiler->reporter->notify(profiler->reporter, &event);
}

static unsigned long func_table_entry_hash(const void * v)
{
    return ((const spx_profiler_func_table_entry_t *) v)->function.hash_code;
}

static int func_table_entry_cmp(const void * va, const void * vb)
{
    const spx_php_function_t a = ((const spx_profiler_func_table_entry_t *) va)->function;
    const spx_php_function_t b = ((const spx_profiler_func_table_entry_t *) vb)->function;

    int n;

    n = strcmp(a.func_name, b.func_name);
    if (n != 0) {
        return n;
    }

    n = strcmp(a.class_name, b.class_name);
    if (n != 0) {
        return n;
    }

    return 0;
}

static spx_profiler_func_table_entry_t * func_table_get_entry(
    func_table_t * func_table,
    const spx_php_function_t * function
) {
    spx_profiler_func_table_entry_t tmp_entry;
    tmp_entry.function = *function;

    int new = 0;
    spx_hset_entry_t * hset_entry = NULL;

    if (func_table->size == FUNC_TABLE_CAPACITY) {
        hset_entry = spx_hset_get_existing_entry(
            func_table->hset,
            &tmp_entry
        );

        if (!hset_entry) {
            return NULL;
        }
    } else {
        hset_entry = spx_hset_get_entry(
            func_table->hset,
            &tmp_entry,
            &new
        );

        if (!hset_entry) {
            /*
             *  FIXME: this kind of error handling (present in some other places) is
             *  quite unfair, especially in ZTS context.
             */
            fprintf(stderr, "SPX: Function table hash index failure\n");
            exit(1);
        }
    }

    if (!new) {
        return spx_hset_entry_get_value(hset_entry);
    }

    func_table->size++;
    if (func_table->size == FUNC_TABLE_CAPACITY) {
        fprintf(stderr, "SPX: FUNC_TABLE_CAPACITY (%d) exceeded\n", FUNC_TABLE_CAPACITY);
    }

    spx_profiler_func_table_entry_t * entry = &func_table->entries[func_table->size - 1];

    entry->function = *function;

    /*
     *  Review needed: workaround for a lifespan issue, see related comments in spx_php.c
     */
    entry->function.func_name = strdup(entry->function.func_name);
    if (*entry->function.class_name) {
        entry->function.class_name = strdup(entry->function.class_name);
    }
    
    entry->stats.called = 0;
    entry->stats.max_cycle_depth = 0;
    METRIC_VALUES_ZERO(entry->stats.inc);
    METRIC_VALUES_ZERO(entry->stats.exc);

    spx_hset_entry_set_value(
        func_table->hset,
        hset_entry,
        entry
    );

    return entry;
}

static void fill_event(
    const spx_profiler_t * profiler,
    spx_profiler_event_type_t type,
    const spx_php_function_t * caller,
    const spx_php_function_t * callee,
    const spx_profiler_metric_values_t * inc,
    const spx_profiler_metric_values_t * exc,
    spx_profiler_event_t * event
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
