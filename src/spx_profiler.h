#ifndef SPX_PROFILER_H_DEFINED
#define SPX_PROFILER_H_DEFINED

#include <stddef.h>

#include "spx_output_stream.h"
#include "spx_metric.h"
#include "spx_php.h"

typedef struct {
    double values[SPX_METRIC_COUNT];
} spx_profiler_metric_values_t;

typedef struct {
    size_t called;
    size_t max_cycle_depth;
    spx_profiler_metric_values_t inc;
    spx_profiler_metric_values_t exc;
} spx_profiler_func_stats_t;

typedef struct {
    size_t idx;
    spx_php_function_t function;
    spx_profiler_func_stats_t stats;
} spx_profiler_func_table_entry_t;

typedef enum {
    SPX_PROFILER_EVENT_CALL_START,
    SPX_PROFILER_EVENT_CALL_END,
    SPX_PROFILER_EVENT_FINALIZE,
} spx_profiler_event_type_t;

typedef struct {
    spx_profiler_event_type_t type;

    const int * enabled_metrics;

    size_t called;
    const spx_profiler_metric_values_t * max;
    const spx_profiler_metric_values_t * cum;

    struct {
        size_t size;
        size_t capacity;
        const spx_profiler_func_table_entry_t * entries;
    } func_table;

    size_t depth;

    const spx_profiler_func_table_entry_t * caller;
    const spx_profiler_func_table_entry_t * callee;
    
    const spx_profiler_metric_values_t * inc;
    const spx_profiler_metric_values_t * exc;
} spx_profiler_event_t;

typedef enum {
    SPX_PROFILER_REPORTER_COST_LIGHT,
    SPX_PROFILER_REPORTER_COST_HEAVY,
} spx_profiler_reporter_cost_t;

typedef struct spx_profiler_reporter_t {
    spx_profiler_reporter_cost_t (*notify) (
        struct spx_profiler_reporter_t * reporter,
        const spx_profiler_event_t * event
    );

    void (*destroy) (struct spx_profiler_reporter_t * reporter);
} spx_profiler_reporter_t;

void spx_profiler_reporter_destroy(spx_profiler_reporter_t * reporter);

typedef struct spx_profiler_t {
    void (*call_start)(struct spx_profiler_t * profiler, const spx_php_function_t * function);
    void (*call_end)(struct spx_profiler_t * profiler);

    void (*finalize)(struct spx_profiler_t * profiler);
    void (*destroy)(struct spx_profiler_t * profiler);
} spx_profiler_t;

#endif /* SPX_PROFILER_H_DEFINED */
