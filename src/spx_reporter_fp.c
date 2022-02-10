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
#include <math.h>

#include <unistd.h>

#include "spx_reporter_fp.h"
#include "spx_resource_stats.h"
#include "spx_output_stream.h"
#include "spx_stdio.h"
#include "spx_thread.h"
#include "spx_utils.h"


typedef struct {
    spx_profiler_reporter_t base;

    spx_metric_t focus;
    int inc;
    int rel;
    size_t limit;
    int live;
    int color;

    spx_output_stream_t * output;
    struct {
        int stdout_fd;
        int stderr_fd;
    } fd_backup;

    size_t last_ts_ms;
    size_t last_line_count;
    const spx_profiler_func_table_entry_t ** top_entries;
} fp_reporter_t;

static const SPX_THREAD_TLS fp_reporter_t * entry_cmp_reporter;

static spx_profiler_reporter_cost_t fp_notify(spx_profiler_reporter_t * reporter, const spx_profiler_event_t * event);
static void fp_destroy(spx_profiler_reporter_t * reporter);

static int entry_cmp(const void * a, const void * b);
static int entry_cmp_r(const void * a, const void * b, const fp_reporter_t * reporter);
static size_t print_report(fp_reporter_t * reporter, const spx_profiler_event_t * event);
static const char * get_value_ansi_fmt(double v);

spx_profiler_reporter_t * spx_reporter_fp_create(
    spx_metric_t focus,
    int inc,
    int rel,
    size_t limit,
    int live,
    int color
) {
    fp_reporter_t * reporter = malloc(sizeof(*reporter));
    if (!reporter) {
        return NULL;
    }

    reporter->base.notify = fp_notify;
    reporter->base.destroy = fp_destroy;

    reporter->focus = focus;
    reporter->inc = inc;
    reporter->rel = rel;
    reporter->limit = limit;
    reporter->live = live && isatty(STDOUT_FILENO);
    reporter->color = color && isatty(STDOUT_FILENO);

    reporter->fd_backup.stdout_fd = -1;
    reporter->fd_backup.stderr_fd = -1;
    reporter->output = NULL;
    reporter->top_entries = NULL;

    if (reporter->live) {
        reporter->fd_backup.stdout_fd = spx_stdio_disable(STDOUT_FILENO);
        if (reporter->fd_backup.stdout_fd == -1) {
            goto error;
        }

        reporter->fd_backup.stderr_fd = spx_stdio_disable(STDERR_FILENO);
        if (reporter->fd_backup.stderr_fd == -1) {
            goto error;
        }

        reporter->output = spx_output_stream_dopen(reporter->fd_backup.stdout_fd, 0);
    } else {
        reporter->output = spx_output_stream_dopen(STDERR_FILENO, 0);
    }

    if (!reporter->output) {
        goto error;
    }

    reporter->last_ts_ms = 0;
    reporter->last_line_count = 0;
    
    reporter->top_entries = malloc(limit * sizeof(*reporter->top_entries));
    if (!reporter->top_entries) {
        goto error;
    }

    return (spx_profiler_reporter_t *) reporter;

error:
    spx_profiler_reporter_destroy((spx_profiler_reporter_t *)reporter);

    return NULL;
}

static spx_profiler_reporter_cost_t fp_notify(spx_profiler_reporter_t * reporter, const spx_profiler_event_t * event)
{
    if (event->type == SPX_PROFILER_EVENT_CALL_START) {
        return SPX_PROFILER_REPORTER_COST_LIGHT;
    }

    fp_reporter_t * fp_reporter = (fp_reporter_t *) reporter;
    if (event->type == SPX_PROFILER_EVENT_CALL_END) {
        if (!fp_reporter->live) {
            return SPX_PROFILER_REPORTER_COST_LIGHT;
        }

        size_t ts_ms = spx_resource_stats_wall_time() / (1000 * 1000);
        if (ts_ms - fp_reporter->last_ts_ms < 70) {
            return SPX_PROFILER_REPORTER_COST_LIGHT;
        }

        fp_reporter->last_ts_ms = ts_ms;
    }

    if (fp_reporter->last_line_count > 0) {
        spx_output_stream_print(fp_reporter->output, "\x0D\x1B[2K");
        size_t i;
        for (i = 0; i < fp_reporter->last_line_count; i++) {
            spx_output_stream_print(fp_reporter->output, "\x1B[1A\x1B[2K");
        }
    }

    fp_reporter->last_line_count = print_report(fp_reporter, event);

    return SPX_PROFILER_REPORTER_COST_HEAVY;
}

static void fp_destroy(spx_profiler_reporter_t * reporter)
{
    fp_reporter_t * fp_reporter = (fp_reporter_t *) reporter;

    if (fp_reporter->top_entries) {
        free(fp_reporter->top_entries);
    }

    if (fp_reporter->output) {
        spx_output_stream_close(fp_reporter->output);
    }

    if (fp_reporter->fd_backup.stdout_fd != -1) {
        spx_stdio_restore(STDOUT_FILENO, fp_reporter->fd_backup.stdout_fd);
    }

    if (fp_reporter->fd_backup.stderr_fd != -1) {
        spx_stdio_restore(STDERR_FILENO, fp_reporter->fd_backup.stderr_fd);
    }
}

static int entry_cmp(const void * a, const void * b)
{
    /*
     *  The use of entry_cmp_reporter TLS variable is required as a workaround for the
     *  absence of qsort_r in some platform.
     *
     *  N.B.: This workaround is not reentrant since reentrancy is not required, it
     *        only have to be thread-safe.
     */
    if (!entry_cmp_reporter) {
        spx_utils_die("entry_cmp_reporter is not set\n");
    }

    return entry_cmp_r(a, b, entry_cmp_reporter);
}

static int entry_cmp_r(const void * a, const void * b, const fp_reporter_t * reporter)
{
    const spx_profiler_func_table_entry_t * entry_a = (*((const spx_profiler_func_table_entry_t **) a));
    const spx_profiler_func_table_entry_t * entry_b = (*((const spx_profiler_func_table_entry_t **) b));

    double high_a, low_a;
    double high_b, low_b;

    if (reporter->inc) {
        high_a = entry_a->stats.inc.values[reporter->focus];
        high_b = entry_b->stats.inc.values[reporter->focus];
        low_a  = entry_a->stats.exc.values[reporter->focus];
        low_b  = entry_b->stats.exc.values[reporter->focus];
    } else {
        high_a = entry_a->stats.exc.values[reporter->focus];
        high_b = entry_b->stats.exc.values[reporter->focus];
        low_a  = entry_a->stats.inc.values[reporter->focus];
        low_b  = entry_b->stats.inc.values[reporter->focus];
    }

    if (high_a != high_b) {
        return high_b - high_a;
    }

    return low_b - low_a;
}

static size_t print_report(fp_reporter_t * reporter, const spx_profiler_event_t * event)
{
    if (event->func_table.size == 0) {
        return 0;
    }

    size_t limit = reporter->limit;
    if (limit > event->func_table.size) {
        limit = event->func_table.size;
    }

    size_t i;
    for (i = 0; i < limit; i++) {
        reporter->top_entries[i] = &event->func_table.entries[i];
    }

    for (i = limit; i < event->func_table.size; i++) {
        const spx_profiler_func_table_entry_t * current = &event->func_table.entries[i];
        size_t j;
        for (j = 0; j < limit; j++) {
            if (entry_cmp_r(&reporter->top_entries[j], &current, reporter) > 0) {
                reporter->top_entries[j] = current;

                break;
            }
        }
    }

    /*
     *  This side effect is required as a workaround for the absence of qsort_r
     *  in some platform.
     *  See related comment in entry_cmp implementation.
     */
    entry_cmp_reporter = reporter;

    qsort(
        reporter->top_entries,
        limit,
        sizeof(*reporter->top_entries),
        entry_cmp
    );

    spx_output_stream_print(reporter->output, "\n*** SPX Report ***\n\nGlobal stats:\n\n");
    size_t line_count = 5;

    spx_output_stream_printf(reporter->output, "  %-20s: ", "Called functions");
    spx_fmt_print_value(
        reporter->output,
        SPX_FMT_QUANTITY,
        event->called
    );

    spx_output_stream_print(reporter->output, "\n");
    line_count++;

    spx_output_stream_printf(reporter->output, "  %-20s: ", "Distinct functions");
    spx_fmt_print_value(
        reporter->output,
        SPX_FMT_QUANTITY,
        event->func_table.size
    );

    if (event->func_table.size == event->func_table.capacity) {
        spx_output_stream_print(reporter->output, "+");
    }

    spx_output_stream_print(reporter->output, "\n\n");
    line_count += 2;

    SPX_METRIC_FOREACH(i, {
        if (!event->enabled_metrics[i]) {
            continue;
        }

        spx_output_stream_printf(reporter->output, "  %-20s: ", spx_metric_info[i].short_name);
        spx_fmt_print_value(
            reporter->output,
            spx_metric_info[i].type,
            event->max->values[i]
        );

        spx_output_stream_print(reporter->output, "\n");
        line_count++;
    });

    spx_output_stream_print(reporter->output, "\nFlat profile:\n\n");
    line_count += 3;

    spx_fmt_row_t * fmt_row = spx_fmt_row_create();

    SPX_METRIC_FOREACH(i, {
        if (!event->enabled_metrics[i]) {
            continue;
        }

        spx_fmt_row_add_tcell(fmt_row, 2, spx_metric_info[i].short_name);
    });

    spx_fmt_row_print(fmt_row, reporter->output);
    spx_fmt_row_reset(fmt_row);

    SPX_METRIC_FOREACH(i, {
        if (!event->enabled_metrics[i]) {
            continue;
        }

        spx_fmt_row_add_tcell(
            fmt_row,
            1,
            i == reporter->focus && reporter->inc ? "*Inc." : "Inc."
        );

        spx_fmt_row_add_tcell(
            fmt_row,
            1,
            i == reporter->focus && !reporter->inc ? "*Exc." : "Exc."
        );
    });

    spx_fmt_row_add_tcell(fmt_row, 1, "Called");
    spx_fmt_row_add_tcell(fmt_row, 0, "Function");

    spx_fmt_row_print(fmt_row, reporter->output);
    spx_fmt_row_print_sep(fmt_row, reporter->output);
    spx_fmt_row_reset(fmt_row);

    line_count += 3;

    for (i = 0; i < limit; i++) {
        const spx_profiler_func_table_entry_t * entry = reporter->top_entries[i];

        SPX_METRIC_FOREACH(i, {
            if (!event->enabled_metrics[i]) {
                continue;
            }

            double inc = entry->stats.inc.values[i];
            double exc = entry->stats.exc.values[i];
            spx_fmt_value_type_t type = spx_metric_info[i].type;

            if (reporter->rel) {
                type = SPX_FMT_PERCENTAGE;
                inc /= event->max->values[i];
                exc /= event->max->values[i];
            }

            const char * inc_ansi_fmt = NULL;
            const char * exc_ansi_fmt = NULL;

            if (reporter->color) {
                inc_ansi_fmt = get_value_ansi_fmt(
                    entry->stats.inc.values[i] / event->max->values[i]
                );

                exc_ansi_fmt = get_value_ansi_fmt(
                    entry->stats.exc.values[i] / event->max->values[i]
                );
            }

            spx_fmt_row_add_ncellf(fmt_row, 1, type, inc, inc_ansi_fmt);
            spx_fmt_row_add_ncellf(fmt_row, 1, type, exc, exc_ansi_fmt);
        });

        spx_fmt_row_add_ncell(fmt_row, 1, SPX_FMT_QUANTITY, entry->stats.called);

        char cycle_depth_str[32] = {0};
        if (entry->stats.max_cycle_depth > 0) {
            snprintf(
                cycle_depth_str,
                sizeof(cycle_depth_str),
                "%lu@",
                entry->stats.max_cycle_depth
            );
        }

        char func_name[256];

        snprintf(
            func_name,
            sizeof(func_name),
            "%s%s%s%s",
            cycle_depth_str,
            entry->function.class_name,
            entry->function.class_name[0] ? "::" : "",
            entry->function.func_name
        );

        spx_fmt_row_add_tcell(fmt_row, 0, func_name);

        spx_fmt_row_print(fmt_row, reporter->output);
        spx_fmt_row_reset(fmt_row);
    }

    line_count += limit;

    spx_fmt_row_destroy(fmt_row);

    spx_output_stream_print(reporter->output, "\n");
    spx_output_stream_flush(reporter->output);

    line_count++;

    return line_count;
}

static const char * get_value_ansi_fmt(double v)
{
    static const char * colors[] = {
        "102;30",
        "42;30",
        "42;33",
        "42;93",
        "42;93;1",
        "43;92;1",
        "43;92",
        "43;32",
        "103;30",
        "43;30",
        "43;31",
        "43;91",
        "43;91;1",
        "41;93;1",
        "41;93",
        "41;33",
        "41",
        "101",
    };

    if (v < 0) {
        v = 0;
    }

    if (v > 1) {
        v = 1;
    }

    return colors[(size_t) round(v * (sizeof(colors) / sizeof(*colors) - 1))];
}
