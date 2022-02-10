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


#ifndef SPX_METRIC_H_DEFINED
#define SPX_METRIC_H_DEFINED

#include "spx_fmt.h"

#define SPX_METRIC_FOREACH(it, block)            \
do {                                             \
    size_t it;                                   \
    for (it = 0; it < SPX_METRIC_COUNT; it++) {  \
        block                                    \
    }                                            \
} while (0)

typedef enum {
    SPX_METRIC_WALL_TIME,
    SPX_METRIC_CPU_TIME,
    SPX_METRIC_IDLE_TIME,

    SPX_METRIC_ZE_MEMORY_USAGE,
    SPX_METRIC_ZE_MEMORY_ALLOC_COUNT,
    SPX_METRIC_ZE_MEMORY_ALLOC_BYTES,
    SPX_METRIC_ZE_MEMORY_FREE_COUNT,
    SPX_METRIC_ZE_MEMORY_FREE_BYTES,
    SPX_METRIC_ZE_GC_RUNS,
    SPX_METRIC_ZE_GC_ROOT_BUFFER,
    SPX_METRIC_ZE_GC_COLLECTED,
    SPX_METRIC_ZE_INCLUDED_FILE_COUNT,
    SPX_METRIC_ZE_INCLUDED_LINE_COUNT,
    SPX_METRIC_ZE_USER_CLASS_COUNT,
    SPX_METRIC_ZE_USER_FUNCTION_COUNT,
    SPX_METRIC_ZE_USER_OPCODE_COUNT,
    SPX_METRIC_ZE_OBJECT_COUNT,
    SPX_METRIC_ZE_ERROR_COUNT,

    SPX_METRIC_MEM_OWN_RSS,

    SPX_METRIC_IO_BYTES,
    SPX_METRIC_IO_RBYTES,
    SPX_METRIC_IO_WBYTES,

    SPX_METRIC_COUNT,
    SPX_METRIC_NONE,
} spx_metric_t;

typedef struct {
    const char * key;
    const char * short_name;
    const char * name;
    spx_fmt_value_type_t type;
    int releasable;
    size_t (*handler)(void);
} spx_metric_info_t;

extern const spx_metric_info_t spx_metric_info[SPX_METRIC_COUNT];

spx_metric_t spx_metric_get_by_key(const char * key);

typedef struct spx_metric_collector_t spx_metric_collector_t;

spx_metric_collector_t * spx_metric_collector_create(const int * enabled_metrics);
void spx_metric_collector_destroy(spx_metric_collector_t * collector);

void spx_metric_collector_collect(spx_metric_collector_t * collector, double * values);
void spx_metric_collector_noise_barrier(spx_metric_collector_t * collector);
void spx_metric_collector_add_fixed_noise(spx_metric_collector_t * collector, const double * noise);

#endif /* SPX_METRIC_H_DEFINED */
