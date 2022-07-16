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
#include <stdlib.h>
#include <string.h>

#include "spx_reporter_trace.h"
#include "spx_output_stream.h"
#include "spx_utils.h"

#define BUFFER_CAPACITY 16384

typedef struct {
    spx_profiler_event_type_t event_type;
    const spx_php_function_t * function;
    size_t depth;
    spx_profiler_metric_values_t cum_metric_values;
    spx_profiler_metric_values_t inc_metric_values;
    spx_profiler_metric_values_t exc_metric_values;
} buffer_entry_t;

typedef struct {
    spx_profiler_reporter_t base;

    const char * file_name;
    spx_output_stream_t * output;

    int safe;

    int first;
    size_t buffer_size;
    buffer_entry_t buffer[BUFFER_CAPACITY];
} trace_reporter_t;

static spx_profiler_reporter_cost_t trace_notify(spx_profiler_reporter_t * base_reporter, const spx_profiler_event_t * event);
static void trace_destroy(spx_profiler_reporter_t * base_reporter);

static void flush_buffer(trace_reporter_t * reporter, const int * enabled_metrics);

static void print_header(spx_output_stream_t * output, const int * enabled_metrics);

static void print_row(
    spx_output_stream_t * output,
    const char * prefix,
    const spx_php_function_t * function,
    size_t depth,
    const int * enabled_metrics,
    const spx_profiler_metric_values_t * cum_metric_values,
    const spx_profiler_metric_values_t * inc_metric_values,
    const spx_profiler_metric_values_t * exc_metric_values
);

spx_profiler_reporter_t * spx_reporter_trace_create(const char * file_name, int safe)
{
    trace_reporter_t * reporter = malloc(sizeof(*reporter));
    if (!reporter) {
        return NULL;
    }

    reporter->base.notify = trace_notify;
    reporter->base.destroy = trace_destroy;

    reporter->file_name = file_name ? file_name : "spx_trace.txt.gz";

    reporter->safe = safe;

    reporter->first = 1;
    reporter->buffer_size = 0;

    const int compressed = spx_utils_str_ends_with(reporter->file_name, ".gz");
    reporter->output = spx_output_stream_open(reporter->file_name, compressed);
    if (!reporter->output) {
        spx_profiler_reporter_destroy((spx_profiler_reporter_t *)reporter);

        return NULL;
    }

    return (spx_profiler_reporter_t *) reporter;
}

static spx_profiler_reporter_cost_t trace_notify(spx_profiler_reporter_t * base_reporter, const spx_profiler_event_t * event)
{
    trace_reporter_t * reporter = (trace_reporter_t *) base_reporter;

    if (event->type != SPX_PROFILER_EVENT_FINALIZE) {
        buffer_entry_t * current = &reporter->buffer[reporter->buffer_size];

        current->event_type        = event->type;
        current->function          = &event->callee->function;
        current->depth             = event->depth;
        current->cum_metric_values = *event->cum;

        if (event->type == SPX_PROFILER_EVENT_CALL_END) {
            current->inc_metric_values = *event->inc;
            current->exc_metric_values = *event->exc;
        }

        reporter->buffer_size++;

        if (reporter->buffer_size < BUFFER_CAPACITY && !reporter->safe) {
            return SPX_PROFILER_REPORTER_COST_LIGHT;
        }
    }

    flush_buffer(reporter, event->enabled_metrics);

    if (event->type == SPX_PROFILER_EVENT_FINALIZE) {
        fprintf(
            stderr,
            "\nSPX trace file: %s\n",
            reporter->file_name
        );
    }

    return SPX_PROFILER_REPORTER_COST_HEAVY;
}

static void trace_destroy(spx_profiler_reporter_t * base_reporter)
{
    trace_reporter_t * reporter = (trace_reporter_t *) base_reporter;

    if (reporter->output) {
        spx_output_stream_close(reporter->output);
    }
}

static void flush_buffer(trace_reporter_t * reporter, const int * enabled_metrics)
{
    if (reporter->first) {
        reporter->first = 0;

        print_header(reporter->output, enabled_metrics);
    }

    size_t i;
    for (i = 0; i < reporter->buffer_size; i++) {
        const buffer_entry_t * entry = &reporter->buffer[i];

        print_row(
            reporter->output,
            entry->event_type == SPX_PROFILER_EVENT_CALL_START ? "+" : "-",
            entry->function,
            entry->depth,
            enabled_metrics,
            &entry->cum_metric_values,
            entry->event_type == SPX_PROFILER_EVENT_CALL_END ? &entry->inc_metric_values : NULL,
            entry->event_type == SPX_PROFILER_EVENT_CALL_END ? &entry->exc_metric_values : NULL
        );

        if (reporter->safe) {
            spx_output_stream_flush(reporter->output);
        }
    }

    reporter->buffer_size = 0;
}

static void print_header(spx_output_stream_t * output, const int * enabled_metrics)
{
    spx_fmt_row_t * fmt_row = spx_fmt_row_create();

    SPX_METRIC_FOREACH(i, {
        if (!enabled_metrics[i]) {
            continue;
        }

        spx_fmt_row_add_tcell(fmt_row, 3, spx_metric_info[i].short_name);
    });

    spx_fmt_row_print(fmt_row, output);
    spx_fmt_row_reset(fmt_row);

    SPX_METRIC_FOREACH(i, {
        if (!enabled_metrics[i]) {
            continue;
        }

        spx_fmt_row_add_tcell(fmt_row, 1, "Cum.");
        spx_fmt_row_add_tcell(fmt_row, 1, "Inc.");
        spx_fmt_row_add_tcell(fmt_row, 1, "Exc.");
    });

    spx_fmt_row_add_tcell(fmt_row, 1, "Depth");
    spx_fmt_row_add_tcell(fmt_row, 0, "Function");

    spx_fmt_row_print(fmt_row, output);
    spx_fmt_row_print_sep(fmt_row, output);
    spx_fmt_row_destroy(fmt_row);
}

static void print_row(
    spx_output_stream_t * output,
    const char * prefix,
    const spx_php_function_t * function,
    size_t depth,
    const int * enabled_metrics,
    const spx_profiler_metric_values_t * cum_metric_values,
    const spx_profiler_metric_values_t * inc_metric_values,
    const spx_profiler_metric_values_t * exc_metric_values
) {
    spx_fmt_row_t * fmt_row = spx_fmt_row_create();

    SPX_METRIC_FOREACH(i, {
        if (!enabled_metrics[i]) {
            continue;
        }

        spx_fmt_row_add_ncell(
            fmt_row,
            1,
            spx_metric_info[i].type,
            cum_metric_values->values[i]
        );

        const double inc = inc_metric_values ? inc_metric_values->values[i] : 0;
        spx_fmt_row_add_ncell(
            fmt_row,
            1,
            spx_metric_info[i].type,
            inc
        );

        const double exc = exc_metric_values ? exc_metric_values->values[i] : 0;
        spx_fmt_row_add_ncell(
            fmt_row,
            1,
            spx_metric_info[i].type,
            exc
        );
    });

    spx_fmt_row_add_ncell(
        fmt_row,
        1,
        SPX_FMT_QUANTITY,
        depth + 1
    );

    char format[32];
    snprintf(
        format,
        sizeof(format),
        "%%%lus%%s%%s%%s",
        depth + 1
    );

    char func_name[256];
    snprintf(
        func_name,
        sizeof(func_name),
        format,
        prefix,
        function->class_name,
        function->class_name[0] ? "::" : "",
        function->func_name
    );

    spx_fmt_row_add_tcell(fmt_row, 0, func_name);

    spx_fmt_row_print(fmt_row, output);
    spx_fmt_row_destroy(fmt_row);
}
