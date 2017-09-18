#include <stdlib.h>
#include "spx_reporter_cg.h"

/*

See http://valgrind.org/docs/manual/cl-format.html for full details on callgrind format.
What is implemented here:

HEADER
# callgrind format
[for each metric]
event: <short name> : <name>
[end for]
events: <space separated metric keys>

fl=<callee.file>
fn=<callee.func>
<callee.line> <space separated exc metric values as int>

fl=<caller.file>
fn=<caller.func>
cfi=<callee.file>
cfn=<callee.func>
calls=1 <callee.line>
<caller.line> <space separated inc metric values as int>

*/

#define BUFFER_CAPACITY 16384

typedef struct {
    const spx_php_function_t * caller;
    const spx_php_function_t * callee;
    spx_profiler_metric_values_t inc;
    spx_profiler_metric_values_t exc;
} buffer_entry_t;

typedef struct {
    spx_profiler_reporter_t base;

    int first;
    size_t buffer_size;
    buffer_entry_t buffer[BUFFER_CAPACITY];
} cg_reporter_t;

static spx_profiler_reporter_cost_t cg_notify(spx_profiler_reporter_t * reporter, const spx_profiler_event_t * event);

static void flush_buffer(cg_reporter_t * reporter, const int * enabled_metrics);

static void print_header(spx_output_stream_t * output, const int * enabled_metrics);

static void print_call(
    spx_output_stream_t * output,
    const int * enabled_metrics,
    const spx_php_function_t * caller,
    const spx_php_function_t * callee,
    const spx_profiler_metric_values_t * inc,
    const spx_profiler_metric_values_t * exc
);

spx_profiler_reporter_t * spx_reporter_cg_create(spx_output_stream_t * output)
{
    cg_reporter_t * reporter = (cg_reporter_t *) spx_profiler_reporter_create(sizeof(*reporter), output, cg_notify, NULL);
    if (!reporter) {
        return NULL;
    }

    reporter->first = 1;
    reporter->buffer_size = 0;

    return (spx_profiler_reporter_t *) reporter;
}

static spx_profiler_reporter_cost_t cg_notify(spx_profiler_reporter_t * base_reporter, const spx_profiler_event_t * event)
{
    if (event->type == SPX_PROFILER_EVENT_CALL_START) {
        return SPX_PROFILER_REPORTER_COST_LIGHT;
    }

    cg_reporter_t * reporter = (cg_reporter_t *) base_reporter;

    if (event->type == SPX_PROFILER_EVENT_CALL_END) {
        buffer_entry_t * current = &reporter->buffer[reporter->buffer_size];

        current->caller = event->caller;
        current->callee = event->callee;
        current->inc    = *event->inc;
        current->exc    = *event->exc;

        reporter->buffer_size++;

        if (reporter->buffer_size < BUFFER_CAPACITY) {
            return SPX_PROFILER_REPORTER_COST_LIGHT;
        }
    }

    flush_buffer(reporter, event->enabled_metrics);

    return SPX_PROFILER_REPORTER_COST_HEAVY;    
}

static void flush_buffer(cg_reporter_t * reporter, const int * enabled_metrics)
{
    if (reporter->first) {
        reporter->first = 0;

        print_header(reporter->base.output, enabled_metrics);
    }

    size_t i;
    for (i = 0; i < reporter->buffer_size; i++) {
        print_call(
            reporter->base.output,
            enabled_metrics,
            reporter->buffer[i].caller,
            reporter->buffer[i].callee,
            &reporter->buffer[i].inc,
            &reporter->buffer[i].exc
        );
    }

    reporter->buffer_size = 0;
}

static void print_header(spx_output_stream_t * output, const int * enabled_metrics)
{
    spx_output_stream_print(output, "# callgrind format\n");

    SPX_METRIC_FOREACH(i, {
        if (!enabled_metrics[i]) {
            continue;
        }

        spx_output_stream_printf(
            output,
            "event: %s : %s\n",
            spx_metrics_info[i].short_name,
            spx_metrics_info[i].name
        );
    });

    spx_output_stream_print(output, "events: ");
    
    SPX_METRIC_FOREACH(i, {
        if (!enabled_metrics[i]) {
            continue;
        }

        spx_output_stream_printf(output, "%s ", spx_metrics_info[i].short_name);
    });

    spx_output_stream_print(output, "\n");
}

static void print_call(
    spx_output_stream_t * output,
    const int * enabled_metrics,
    const spx_php_function_t * caller,
    const spx_php_function_t * callee,
    const spx_profiler_metric_values_t * inc,
    const spx_profiler_metric_values_t * exc
) {
    spx_output_stream_printf(
        output,
        "fl=%s\nfn=%s%s%s\n",
        callee->file_name,
        callee->class_name,
        callee->call_type,
        callee->func_name
    );

    spx_output_stream_printf(output, "%d", callee->line);

    SPX_METRIC_FOREACH(i, {
        if (!enabled_metrics[i]) {
            continue;
        }

        spx_output_stream_printf(output, " %d", (int) exc->values[i]);
    });

    spx_output_stream_print(output, "\n\n");

    if (!caller) {
        return;
    }

    spx_output_stream_printf(
        output,
        "fl=%s\nfn=%s%s%s\n",
        caller->file_name,
        caller->class_name,
        caller->call_type,
        caller->func_name
    );

    spx_output_stream_printf(
        output,
        "cfi=%s\ncfn=%s%s%s\n",
        callee->file_name,
        callee->class_name,
        callee->call_type,
        callee->func_name
    );

    spx_output_stream_printf(
        output,
        "calls=1 %d\n",
        callee->line
    );

    spx_output_stream_printf(output, "%d", caller->line);
    SPX_METRIC_FOREACH(i, {
        if (!enabled_metrics[i]) {
            continue;
        }

        spx_output_stream_printf(output, " %d", (int) inc->values[i]);
    });

    spx_output_stream_print(output, "\n\n");
}
