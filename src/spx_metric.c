#include <string.h>
#include <stdlib.h>

#include "spx_metric.h"
#include "spx_resource_stats.h"
#include "spx_php.h"

#ifdef __GNUC__
#   define ARRAY_INIT_INDEX(idx) [idx] = 
#else
#   error "Please open an issue"
#endif

const spx_metric_info_t spx_metrics_info[SPX_METRIC_COUNT] = {
    ARRAY_INIT_INDEX(SPX_METRIC_WALL_TIME) {
        "wt",
        "Wall Time",
        "Wall Time",
        SPX_FMT_TIME,
        0,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_CPU_TIME) {
        "ct",
        "CPU Time",
        "CPU Time",
        SPX_FMT_TIME,
        0,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_IDLE_TIME) {
        "it",
        "Idle Time",
        "Idle Time",
        SPX_FMT_TIME,
        0,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_MEMORY_USAGE) {
        "zm",
        "ZE memory usage",
        "Zend Engine memory usage",
        SPX_FMT_MEMORY,
        1,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_MEMORY_ALLOC_COUNT) {
        "zmac",
        "ZE alloc count",
        "Zend Engine allocation count",
        SPX_FMT_QUANTITY,
        0,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_MEMORY_ALLOC_BYTES) {
        "zmab",
        "ZE alloc bytes",
        "Zend Engine allocated bytes",
        SPX_FMT_MEMORY,
        0,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_MEMORY_FREE_COUNT) {
        "zmfc",
        "ZE free count",
        "Zend Engine free count",
        SPX_FMT_QUANTITY,
        0,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_MEMORY_FREE_BYTES) {
        "zmfb",
        "ZE free bytes",
        "Zend Engine freed bytes",
        SPX_FMT_MEMORY,
        0,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_GC_RUNS) {
        "zgr",
        "ZE GC runs",
        "Zend Engine GC run count",
        SPX_FMT_QUANTITY,
        0,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_GC_ROOT_BUFFER) {
        "zgb",
        "ZE GC root buffer",
        "Zend Engine GC root buffer length",
        SPX_FMT_QUANTITY,
        1,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_GC_COLLECTED) {
        "zgc",
        "ZE GC collected",
        "Zend Engine GC collected cycle count",
        SPX_FMT_QUANTITY,
        0,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_INCLUDED_FILE_COUNT) {
        "zif",
        "ZE file count",
        "Zend Engine included file count",
        SPX_FMT_QUANTITY,
        0,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_INCLUDED_LINE_COUNT) {
        "zil",
        "ZE line count",
        "Zend Engine included line count",
        SPX_FMT_QUANTITY,
        0,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_USER_CLASS_COUNT) {
        "zuc",
        "ZE class count",
        "Zend Engine user class count",
        SPX_FMT_QUANTITY,
        0,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_USER_FUNCTION_COUNT) {
        "zuf",
        "ZE func. count",
        "Zend Engine user function count",
        SPX_FMT_QUANTITY,
        0,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_USER_OPCODE_COUNT) {
        "zuo",
        "ZE opcodes count",
        "Zend Engine user opcode count",
        SPX_FMT_QUANTITY,
        0,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_OBJECT_COUNT) {
        "zo",
        "ZE object count",
        "Zend Engine object count",
        SPX_FMT_QUANTITY,
        1,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_ERROR_COUNT) {
        "ze",
        "ZE error count",
        "Zend Engine error count",
        SPX_FMT_QUANTITY,
        0,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_IO_BYTES) {
        "io",
        "I/O Bytes",
        "I/O Bytes (reads + writes)",
        SPX_FMT_MEMORY,
        0,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_IO_RBYTES) {
        "ior",
        "I/O Read Bytes",
        "I/O Read Bytes",
        SPX_FMT_MEMORY,
        0,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_IO_WBYTES) {
        "iow",
        "I/O Written Bytes",
        "I/O Written Bytes",
        SPX_FMT_MEMORY,
        0,
    },
};

spx_metric_t spx_metric_get_by_key(const char * key)
{
    SPX_METRIC_FOREACH(i, {
        if (0 == strcmp(spx_metrics_info[i].key, key)) {
            return i;
        }
    });

    return SPX_METRIC_NONE;
}

struct spx_metric_collector_t {
    int enabled_metrics[SPX_METRIC_COUNT];
    double ref_values[SPX_METRIC_COUNT];
    double last_values[SPX_METRIC_COUNT];
    double current_fixed_noise[SPX_METRIC_COUNT];
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
        collector->current_fixed_noise[i] = 0;
    });

    return collector;
}

void spx_metric_collector_destroy(spx_metric_collector_t * collector)
{
    free(collector);
}

void spx_metric_collector_collect(spx_metric_collector_t * collector, double * values)
{
    double current_values[SPX_METRIC_COUNT];
    collect_raw_values(collector->enabled_metrics, current_values);

    /*
     *  This branch is required to fix cpu / wall time inconsistency (cpu > wall time within a single thread).
     */
    if (
        collector->enabled_metrics[SPX_METRIC_WALL_TIME] &&
        collector->enabled_metrics[SPX_METRIC_CPU_TIME]
    ) {
        const double ct_surplus =
            (current_values[SPX_METRIC_CPU_TIME] - collector->last_values[SPX_METRIC_CPU_TIME])
            - (current_values[SPX_METRIC_WALL_TIME] - collector->last_values[SPX_METRIC_WALL_TIME])
        ;

        if (ct_surplus > 0) {
            collector->ref_values[SPX_METRIC_CPU_TIME] += ct_surplus;
            collector->ref_values[SPX_METRIC_IDLE_TIME] -= ct_surplus;
        }
    }

    SPX_METRIC_FOREACH(i, {
        /* FIXME this branch should concern all non releasable metrics */
        if (i == SPX_METRIC_WALL_TIME || i == SPX_METRIC_CPU_TIME) {
            const double diff = current_values[i] - collector->last_values[i];
            if (collector->current_fixed_noise[i] > diff) {
                collector->current_fixed_noise[i] = diff;
            }
        }

        collector->ref_values[i] += collector->current_fixed_noise[i];
        collector->current_fixed_noise[i] = 0;

        collector->last_values[i] = current_values[i];
        values[i] = collector->last_values[i] - collector->ref_values[i];
    });
}

void spx_metric_collector_noise_barrier(spx_metric_collector_t * collector)
{
    double current_values[SPX_METRIC_COUNT];
    collect_raw_values(collector->enabled_metrics, current_values);

    SPX_METRIC_FOREACH(i, {
        collector->current_fixed_noise[i] += current_values[i] - collector->last_values[i];
    });
}

void spx_metric_collector_add_fixed_noise(spx_metric_collector_t * collector, const double * noise)
{
    SPX_METRIC_FOREACH(i, {
        collector->current_fixed_noise[i] += noise[i];
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

    if (enabled_metrics[SPX_METRIC_ZE_MEMORY_USAGE]) {
        current_values[SPX_METRIC_ZE_MEMORY_USAGE] = spx_php_zend_memory_usage();
    }

    if (enabled_metrics[SPX_METRIC_ZE_MEMORY_ALLOC_COUNT]) {
        current_values[SPX_METRIC_ZE_MEMORY_ALLOC_COUNT] = spx_php_zend_memory_alloc_count();
    }

    if (enabled_metrics[SPX_METRIC_ZE_MEMORY_ALLOC_BYTES]) {
        current_values[SPX_METRIC_ZE_MEMORY_ALLOC_BYTES] = spx_php_zend_memory_alloc_bytes();
    }

    if (enabled_metrics[SPX_METRIC_ZE_MEMORY_FREE_COUNT]) {
        current_values[SPX_METRIC_ZE_MEMORY_FREE_COUNT] = spx_php_zend_memory_free_count();
    }

    if (enabled_metrics[SPX_METRIC_ZE_MEMORY_FREE_BYTES]) {
        current_values[SPX_METRIC_ZE_MEMORY_FREE_BYTES] = spx_php_zend_memory_free_bytes();
    }

    if (enabled_metrics[SPX_METRIC_ZE_GC_RUNS]) {
        current_values[SPX_METRIC_ZE_GC_RUNS] = spx_php_zend_gc_run_count();
    }

    if (enabled_metrics[SPX_METRIC_ZE_GC_ROOT_BUFFER]) {
        current_values[SPX_METRIC_ZE_GC_ROOT_BUFFER] = spx_php_zend_gc_root_buffer_length();
    }

    if (enabled_metrics[SPX_METRIC_ZE_GC_COLLECTED]) {
        current_values[SPX_METRIC_ZE_GC_COLLECTED] = spx_php_zend_gc_collected_count();
    }

    if (enabled_metrics[SPX_METRIC_ZE_INCLUDED_FILE_COUNT]) {
        current_values[SPX_METRIC_ZE_INCLUDED_FILE_COUNT] = spx_php_zend_included_file_count();
    }

    if (enabled_metrics[SPX_METRIC_ZE_INCLUDED_LINE_COUNT]) {
        current_values[SPX_METRIC_ZE_INCLUDED_LINE_COUNT] = spx_php_zend_included_line_count();
    }

    if (enabled_metrics[SPX_METRIC_ZE_USER_CLASS_COUNT]) {
        current_values[SPX_METRIC_ZE_USER_CLASS_COUNT] = spx_php_zend_class_count();
    }

    if (enabled_metrics[SPX_METRIC_ZE_USER_FUNCTION_COUNT]) {
        current_values[SPX_METRIC_ZE_USER_FUNCTION_COUNT] = spx_php_zend_function_count();
    }

    if (enabled_metrics[SPX_METRIC_ZE_USER_OPCODE_COUNT]) {
        current_values[SPX_METRIC_ZE_USER_OPCODE_COUNT] = spx_php_zend_opcode_count();
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
