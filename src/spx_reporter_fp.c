#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "spx_reporter_fp.h"
#include "spx_thread.h"

typedef struct {
    spx_profiler_reporter_t base;

    spx_metric_t focus;
    int inc;
    int rel;
    size_t limit;
    int live;

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

spx_profiler_reporter_t * spx_reporter_fp_create(
    spx_output_stream_t * output,
    spx_metric_t focus,
    int inc,
    int rel,
    size_t limit,
    int live
) {
    fp_reporter_t * reporter = (fp_reporter_t *) spx_profiler_reporter_create(
        sizeof(*reporter),
        output,
        fp_notify,
        fp_destroy
    );

    if (!reporter) {
        return NULL;
    }

    reporter->focus = focus;
    reporter->inc = inc;
    reporter->rel = rel;
    reporter->limit = limit;
    reporter->live = live;

    reporter->last_ts_ms = 0;
    reporter->last_line_count = 0;
    
    reporter->top_entries = malloc(limit * sizeof(*reporter->top_entries));
    if (!reporter->top_entries) {
        free(reporter);

        return NULL;
    }

    return (spx_profiler_reporter_t *) reporter;
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

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME_COARSE, &ts);

        size_t ts_ms = ts.tv_sec * 1000 + ts.tv_nsec / (1000 * 1000);

        if (ts_ms - fp_reporter->last_ts_ms < 70) {
            return SPX_PROFILER_REPORTER_COST_LIGHT;
        }

        fp_reporter->last_ts_ms = ts_ms;
    }

    if (fp_reporter->last_line_count > 0) {
        spx_output_stream_print(fp_reporter->base.output, "\x0D\x1B[2K");
        size_t i;
        for (i = 0; i < fp_reporter->last_line_count; i++) {
            spx_output_stream_print(fp_reporter->base.output, "\x1B[1A\x1B[2K");
        }
    }

    fp_reporter->last_line_count = print_report(fp_reporter, event);

    return SPX_PROFILER_REPORTER_COST_HEAVY;
}

static void fp_destroy(spx_profiler_reporter_t * reporter)
{
    fp_reporter_t * fp_reporter = (fp_reporter_t *) reporter;

    free(fp_reporter->top_entries);
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
        fprintf(stderr, "entry_cmp_reporter is not set\n");
        exit(1);
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

    spx_output_stream_print(reporter->base.output, "\n*** SPX Report ***\n\nGlobal stats:\n\n");
    size_t line_count = 5;

    spx_output_stream_printf(reporter->base.output, "  %-20s: ", "Called functions");
    spx_fmt_print_value(
        reporter->base.output,
        SPX_FMT_QUANTITY,
        event->called
    );

    spx_output_stream_print(reporter->base.output, "\n");
    line_count++;

    spx_output_stream_printf(reporter->base.output, "  %-20s: ", "Distinct functions");
    spx_fmt_print_value(
        reporter->base.output,
        SPX_FMT_QUANTITY,
        event->func_table.size
    );

    if (event->func_table.size == event->func_table.capacity) {
        spx_output_stream_print(reporter->base.output, "+");
    }

    spx_output_stream_print(reporter->base.output, "\n\n");
    line_count += 2;

    SPX_METRIC_FOREACH(i, {
        if (!event->enabled_metrics[i]) {
            continue;
        }

        spx_output_stream_printf(reporter->base.output, "  %-20s: ", spx_metrics_info[i].name);
        spx_fmt_print_value(
            reporter->base.output,
            spx_metrics_info[i].type,
            event->max->values[i]
        );

        spx_output_stream_print(reporter->base.output, "\n");
        line_count++;
    });

    spx_output_stream_print(reporter->base.output, "\nFlat profile:\n\n");
    line_count += 3;

    spx_fmt_row_t * fmt_row = spx_fmt_row_create();

    SPX_METRIC_FOREACH(i, {
        if (!event->enabled_metrics[i]) {
            continue;
        }

        spx_fmt_row_add_tcell(fmt_row, 2, spx_metrics_info[i].name);
    });

    spx_fmt_row_print(fmt_row, reporter->base.output);
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

    spx_fmt_row_print(fmt_row, reporter->base.output);
    spx_fmt_row_print_sep(fmt_row, reporter->base.output);
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
            spx_fmt_value_type_t type = spx_metrics_info[i].type;

            if (reporter->rel) {
                type = SPX_FMT_PERCENTAGE;
                inc /= event->max->values[i];
                exc /= event->max->values[i];
            }

            spx_fmt_row_add_ncell(fmt_row, 1, type, inc);
            spx_fmt_row_add_ncell(fmt_row, 1, type, exc);
        });

        spx_fmt_row_add_ncell(fmt_row, 1, SPX_FMT_QUANTITY, entry->stats.called);

        char func_name[256];

        snprintf(
            func_name,
            sizeof(func_name),
            "%s%s%s",
            entry->function.class_name,
            entry->function.call_type,
            entry->function.func_name
        );

        spx_fmt_row_add_tcell(fmt_row, 0, func_name);

        spx_fmt_row_print(fmt_row, reporter->base.output);
        spx_fmt_row_reset(fmt_row);
    }

    line_count += limit;

    spx_fmt_row_destroy(fmt_row);

    spx_output_stream_print(reporter->base.output, "\n");
    spx_output_stream_flush(reporter->base.output);

    line_count++;

    return line_count;
}
