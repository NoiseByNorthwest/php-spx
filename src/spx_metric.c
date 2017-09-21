#include <string.h>
#include <stdlib.h>

#include "spx_metric.h"
#include "spx_resource_stats.h"
#include "spx_php.h"

#ifdef __GNUC__
#   define ARRAY_INIT_INDEX(idx) [idx] = 
#else
#   define ARRAY_INIT_INDEX(idx)
#endif

const spx_metric_info_t spx_metrics_info[SPX_METRIC_COUNT] = {
    ARRAY_INIT_INDEX(SPX_METRIC_WALL_TIME) {
        "wt",
        "Wall Time",
        SPX_FMT_TIME,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_CPU_TIME) {
        "ct",
        "CPU Time",
        SPX_FMT_TIME,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_IDLE_TIME) {
        "it",
        "Idle Time",
        SPX_FMT_TIME,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_MEMORY) {
        "zm",
        "ZE memory",
        SPX_FMT_MEMORY,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_ROOT_BUFFER) {
        "zr",
        "ZE root buffer",
        SPX_FMT_QUANTITY,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_OBJECT_COUNT) {
        "zo",
        "ZE object count",
        SPX_FMT_QUANTITY,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_ERROR_COUNT) {
        "ze",
        "ZE error count",
        SPX_FMT_QUANTITY,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_IO_BYTES) {
        "io",
        "I/O Bytes",
        SPX_FMT_MEMORY,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_IO_RBYTES) {
        "ior",
        "I/O Read Bytes",
        SPX_FMT_MEMORY,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_IO_WBYTES) {
        "iow",
        "I/O Written Bytes",
        SPX_FMT_MEMORY,
    },
};

spx_metric_t spx_metric_get_by_short_name(const char * short_name)
{
    SPX_METRIC_FOREACH(i, {
        if (0 == strcmp(spx_metrics_info[i].short_name, short_name)) {
            return i;
        }
    });

    return SPX_METRIC_NONE;
}

struct spx_metric_collector_t {
    int enabled_metrics[SPX_METRIC_COUNT];
    double ref_values[SPX_METRIC_COUNT];
    double last_values[SPX_METRIC_COUNT];
};

static void collect_raw_values(const int * enabled_metrics, double * current_values);

spx_metric_collector_t * spx_metric_collector_create(const int * enabled_metrics)
{
    spx_metric_collector_t * collector = malloc(sizeof(*collector));
    if (!collector) {
        return NULL;
    }

    collect_raw_values(enabled_metrics, collector->last_values);

    SPX_METRIC_FOREACH(i, {
        collector->enabled_metrics[i] = enabled_metrics[i];
        collector->ref_values[i] = collector->last_values[i];
    });

    return collector;
}

void spx_metric_collector_destroy(spx_metric_collector_t * collector)
{
    free(collector);
}

void spx_metric_collector_collect(spx_metric_collector_t * collector, double * values)
{
    collect_raw_values(collector->enabled_metrics, collector->last_values);

    SPX_METRIC_FOREACH(i, {
        values[i] = collector->last_values[i] - collector->ref_values[i];
    });
}

void spx_metric_collector_noise_barrier(spx_metric_collector_t * collector)
{
    double current_values[SPX_METRIC_COUNT];
    collect_raw_values(collector->enabled_metrics, current_values);

    SPX_METRIC_FOREACH(i, {
        collector->ref_values[i] += current_values[i] - collector->last_values[i];
    });
}

static void collect_raw_values(const int * enabled_metrics, double * current_values)
{
    int time = 0, cpu_time = 0, io_stats = 0;

    SPX_METRIC_FOREACH(i, {
        if (!enabled_metrics[i]) {
            current_values[i] = 0;

            continue;
        }

        switch (i) {
            case SPX_METRIC_CPU_TIME:
            case SPX_METRIC_IDLE_TIME:
                cpu_time = 1;

            case SPX_METRIC_WALL_TIME:
                time = 1;

                break;

            case SPX_METRIC_IO_BYTES:
            case SPX_METRIC_IO_RBYTES:
            case SPX_METRIC_IO_WBYTES:
                io_stats = 1;

                break;
        }
    });

    if (time) {
        current_values[SPX_METRIC_WALL_TIME] = spx_resource_stats_wall_time();

        if (cpu_time) {
            current_values[SPX_METRIC_CPU_TIME] = spx_resource_stats_cpu_time();
            current_values[SPX_METRIC_IDLE_TIME] = current_values[SPX_METRIC_WALL_TIME] - current_values[SPX_METRIC_CPU_TIME];
        }
    }

    if (enabled_metrics[SPX_METRIC_ZE_MEMORY]) {
        current_values[SPX_METRIC_ZE_MEMORY] = spx_php_zend_memory_usage();
    }

    if (enabled_metrics[SPX_METRIC_ZE_ROOT_BUFFER]) {
        current_values[SPX_METRIC_ZE_ROOT_BUFFER] = spx_php_zend_root_buffer_length();
    }

    if (enabled_metrics[SPX_METRIC_ZE_OBJECT_COUNT]) {
        current_values[SPX_METRIC_ZE_OBJECT_COUNT] = spx_php_zend_object_count();
    }

    if (enabled_metrics[SPX_METRIC_ZE_ERROR_COUNT]) {
        current_values[SPX_METRIC_ZE_ERROR_COUNT] = spx_php_zend_error_count();
    }

    if (io_stats) {
        size_t in;
        size_t out;
        spx_resource_stats_io(&in, &out);

        current_values[SPX_METRIC_IO_RBYTES] = in;
        current_values[SPX_METRIC_IO_WBYTES] = out;
        current_values[SPX_METRIC_IO_BYTES] = in + out;
    }
}
