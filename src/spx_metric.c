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


#include <string.h>
#include <stdlib.h>

#include "spx_metric.h"
#include "spx_resource_stats.h"
#include "spx_thread.h"
#include "spx_php.h"

#ifdef __GNUC__
#   define ARRAY_INIT_INDEX(idx) [idx] = 
#else
#   error "Please open an issue"
#endif


struct spx_metric_collector_t {
    int enabled_metrics[SPX_METRIC_COUNT];
    double ref_values[SPX_METRIC_COUNT];
    double last_values[SPX_METRIC_COUNT];
    double current_fixed_noise[SPX_METRIC_COUNT];
};


static size_t metric_handler_idle_time(void);
static size_t metric_handler_io_bytes(void);
static size_t metric_handler_io_r_bytes(void);
static size_t metric_handler_io_w_bytes(void);

static void memoize_io_stats(void);
static size_t memoized_metric_value(spx_metric_t metric);

static void collect_raw_values(const int * enabled_metrics, double * current_values);

const spx_metric_info_t spx_metric_info[SPX_METRIC_COUNT] = {
    ARRAY_INIT_INDEX(SPX_METRIC_WALL_TIME) {
        "wt",
        "Wall time",
        "Wall time",
        SPX_FMT_TIME,
        0,
        spx_resource_stats_wall_time,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_CPU_TIME) {
        "ct",
        "CPU time",
        "CPU time",
        SPX_FMT_TIME,
        0,
        spx_resource_stats_cpu_time,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_IDLE_TIME) {
        "it",
        "Idle time",
        "Idle time",
        SPX_FMT_TIME,
        0,
        metric_handler_idle_time,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_MEMORY_USAGE) {
        "zm",
        "ZE memory usage",
        "Zend Engine memory usage",
        SPX_FMT_MEMORY,
        1,
        spx_php_zend_memory_usage,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_MEMORY_ALLOC_COUNT) {
        "zmac",
        "ZE alloc count",
        "Zend Engine allocation count",
        SPX_FMT_QUANTITY,
        0,
        spx_php_zend_memory_alloc_count,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_MEMORY_ALLOC_BYTES) {
        "zmab",
        "ZE alloc bytes",
        "Zend Engine allocated bytes",
        SPX_FMT_MEMORY,
        0,
        spx_php_zend_memory_alloc_bytes,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_MEMORY_FREE_COUNT) {
        "zmfc",
        "ZE free count",
        "Zend Engine free count",
        SPX_FMT_QUANTITY,
        0,
        spx_php_zend_memory_free_count,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_MEMORY_FREE_BYTES) {
        "zmfb",
        "ZE free bytes",
        "Zend Engine freed bytes",
        SPX_FMT_MEMORY,
        0,
        spx_php_zend_memory_free_bytes,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_GC_RUNS) {
        "zgr",
        "ZE GC runs",
        "Zend Engine GC run count",
        SPX_FMT_QUANTITY,
        0,
        spx_php_zend_gc_run_count,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_GC_ROOT_BUFFER) {
        "zgb",
        "ZE GC root buffer",
        "Zend Engine GC root buffer length",
        SPX_FMT_QUANTITY,
        1,
        spx_php_zend_gc_root_buffer_length,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_GC_COLLECTED) {
        "zgc",
        "ZE GC collected",
        "Zend Engine GC collected cycle count",
        SPX_FMT_QUANTITY,
        0,
        spx_php_zend_gc_collected_count,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_INCLUDED_FILE_COUNT) {
        "zif",
        "ZE file count",
        "Zend Engine included file count",
        SPX_FMT_QUANTITY,
        0,
        spx_php_zend_included_file_count,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_INCLUDED_LINE_COUNT) {
        "zil",
        "ZE line count",
        "Zend Engine included line count",
        SPX_FMT_QUANTITY,
        0,
        spx_php_zend_included_line_count,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_USER_CLASS_COUNT) {
        "zuc",
        "ZE class count",
        "Zend Engine user class count",
        SPX_FMT_QUANTITY,
        0,
        spx_php_zend_class_count,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_USER_FUNCTION_COUNT) {
        "zuf",
        "ZE func. count",
        "Zend Engine user function count",
        SPX_FMT_QUANTITY,
        0,
        spx_php_zend_function_count,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_USER_OPCODE_COUNT) {
        "zuo",
        "ZE opcodes count",
        "Zend Engine user opcode count",
        SPX_FMT_QUANTITY,
        0,
        spx_php_zend_opcode_count,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_OBJECT_COUNT) {
        "zo",
        "ZE object count",
        "Zend Engine object count",
        SPX_FMT_QUANTITY,
        1,
        spx_php_zend_object_count,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_ZE_ERROR_COUNT) {
        "ze",
        "ZE error count",
        "Zend Engine error count",
        SPX_FMT_QUANTITY,
        0,
        spx_php_zend_error_count,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_MEM_OWN_RSS) {
        "mor",
        "Own RSS",
        "Process's own RSS",
        SPX_FMT_MEMORY,
        1,
        spx_resource_stats_own_rss,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_IO_BYTES) {
        "io",
        "I/O Bytes",
        "I/O Bytes (reads + writes)",
        SPX_FMT_MEMORY,
        0,
        metric_handler_io_bytes,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_IO_RBYTES) {
        "ior",
        "I/O Read Bytes",
        "I/O Read Bytes",
        SPX_FMT_MEMORY,
        0,
        metric_handler_io_r_bytes,
    },
    ARRAY_INIT_INDEX(SPX_METRIC_IO_WBYTES) {
        "iow",
        "I/O Written Bytes",
        "I/O Written Bytes",
        SPX_FMT_MEMORY,
        0,
        metric_handler_io_w_bytes,
    },
};

static SPX_THREAD_TLS struct {
    int memoized;
    size_t value;
} memoized_metric_values[SPX_METRIC_COUNT];

spx_metric_t spx_metric_get_by_key(const char * key)
{
    SPX_METRIC_FOREACH(i, {
        if (0 == strcmp(spx_metric_info[i].key, key)) {
            return i;
        }
    });

    return SPX_METRIC_NONE;
}

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
        if (!collector->enabled_metrics[i]) {
            continue;
        }

        if (!spx_metric_info[i].releasable) {
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

static size_t metric_handler_idle_time(void)
{
    return memoized_metric_value(SPX_METRIC_WALL_TIME) - memoized_metric_value(SPX_METRIC_CPU_TIME);
}

static size_t metric_handler_io_bytes(void)
{
    return memoized_metric_value(SPX_METRIC_IO_RBYTES) + memoized_metric_value(SPX_METRIC_IO_WBYTES);
}

static size_t metric_handler_io_w_bytes(void)
{
    memoize_io_stats();

    return memoized_metric_value(SPX_METRIC_IO_WBYTES);
}

static size_t metric_handler_io_r_bytes(void)
{
    memoize_io_stats();

    return memoized_metric_value(SPX_METRIC_IO_RBYTES);
}

static void memoize_io_stats(void)
{
    if (
        memoized_metric_values[SPX_METRIC_IO_RBYTES].memoized
        && memoized_metric_values[SPX_METRIC_IO_WBYTES].memoized
    ) {
        return;
    }

    size_t in, out;
    spx_resource_stats_io(&in, &out);

    memoized_metric_values[SPX_METRIC_IO_RBYTES].value = in;
    memoized_metric_values[SPX_METRIC_IO_RBYTES].memoized = 1;

    memoized_metric_values[SPX_METRIC_IO_WBYTES].value = out;
    memoized_metric_values[SPX_METRIC_IO_WBYTES].memoized = 1;
}

static size_t memoized_metric_value(spx_metric_t metric)
{
    if (!memoized_metric_values[metric].memoized) {
        memoized_metric_values[metric].value = spx_metric_info[metric].handler();
        memoized_metric_values[metric].memoized = 1;
    }

    return memoized_metric_values[metric].value;
}

static void collect_raw_values(const int * enabled_metrics, double * current_values)
{
    SPX_METRIC_FOREACH(i, {
        memoized_metric_values[i].memoized = 0;
    });

    SPX_METRIC_FOREACH(i, {
        if (!enabled_metrics[i]) {
            current_values[i] = 0;

            continue;
        }

        current_values[i] = memoized_metric_value(i);
    });
}
