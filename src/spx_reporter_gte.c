#include <stdlib.h>
#include "spx_reporter_gte.h"

#define BUFFER_CAPACITY 16384

typedef struct {
    spx_profiler_event_type_t event_type;
    const spx_php_function_t * callee;
    spx_profiler_metric_values_t cum;
} buffer_entry_t;

typedef struct {
    spx_profiler_reporter_t base;

    int first;
    size_t buffer_size;
    buffer_entry_t buffer[BUFFER_CAPACITY];
} gte_reporter_t;

static spx_profiler_reporter_cost_t gte_notify(spx_profiler_reporter_t * base_reporter, const spx_profiler_event_t * event);

static void flush_buffer(gte_reporter_t * reporter);

static char * json_escape(char * dst, const char * src, size_t limit);

spx_profiler_reporter_t * spx_reporter_gte_create(spx_output_stream_t * output)
{
    gte_reporter_t * reporter = (gte_reporter_t *) spx_profiler_reporter_create(
        sizeof(*reporter),
        output,
        gte_notify,
        NULL
    );

    if (!reporter) {
        return NULL;
    }

    reporter->first = 1;
    reporter->buffer_size = 0;

    return (spx_profiler_reporter_t *) reporter;
}

static spx_profiler_reporter_cost_t gte_notify(spx_profiler_reporter_t * base_reporter, const spx_profiler_event_t * event)
{
    gte_reporter_t * reporter = (gte_reporter_t *) base_reporter;

    if (event->type != SPX_PROFILER_EVENT_FINALIZE) {
        buffer_entry_t * current = &reporter->buffer[reporter->buffer_size];

        current->event_type = event->type;
        current->callee     = event->callee;
        current->cum        = *event->cum;

        reporter->buffer_size++;

        if (reporter->buffer_size < BUFFER_CAPACITY) {
            return SPX_PROFILER_REPORTER_COST_LIGHT;
        }
    }

    flush_buffer(reporter);

    if (event->type == SPX_PROFILER_EVENT_FINALIZE) {
        spx_output_stream_print(reporter->base.output, "]");
    }

    return SPX_PROFILER_REPORTER_COST_HEAVY;
}

static void flush_buffer(gte_reporter_t * reporter)
{
    char class_name[512];
    char func_name[512];

    size_t i;
    for (i = 0; i < reporter->buffer_size; i++) {
        const buffer_entry_t * entry = &reporter->buffer[i];

        spx_output_stream_printf(
            reporter->base.output,
            "%s{\"name\": \"%s%s%s\", \"cat\": \"PHP\", \"ph\": \"%s\", \"pid\": 0, \"tid\": 0, \"ts\": %lu}",
            reporter->first ? "[\n" : ",\n",
            json_escape(class_name, entry->callee->class_name, sizeof(class_name)),
            entry->callee->call_type,
            json_escape(func_name, entry->callee->func_name, sizeof(func_name)),
            entry->event_type == SPX_PROFILER_EVENT_CALL_START ? "B" : "E",
            (size_t) entry->cum.values[SPX_METRIC_WALL_TIME]
        );

        if (reporter->first) {
            reporter->first = 0;
        }
    }

    reporter->buffer_size = 0;
}

static char * json_escape(char * dst, const char * src, size_t limit)
{
    size_t i = 0;
    while (*src && i < limit - 2) {
        dst[i++] = *src;
        if (*src == '\\') {
            dst[i++] = '\\';
        }

        src++;
    }

    dst[i] = 0;

    return dst;
}
