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


/* _GNU_SOURCE is implicitly defined since PHP 8.2 https://github.com/php/php-src/pull/8807 */
#ifndef _GNU_SOURCE
#   define _GNU_SOURCE
#endif

#include <sys/time.h>
#include <sys/resource.h>

#include "spx_resource_stats.h"

static inline size_t spx_resource_stats_wall_time_coarse(void);
static inline size_t spx_resource_stats_cpu_time_coarse(void);

void spx_resource_stats_init(void)
{
}

void spx_resource_stats_shutdown(void)
{
}

#define TIMESPEC_TO_NS(ts) ((ts).tv_sec * 1000 * 1000 * 1000 + (ts).tv_nsec)

size_t spx_resource_stats_wall_time(void)
{
#if (__MAC_OS_X_VERSION_MIN_REQUIRED < 101200)
    return spx_resource_stats_wall_time_coarse();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return TIMESPEC_TO_NS(ts);
#endif
}

size_t spx_resource_stats_cpu_time(void)
{
#if (__MAC_OS_X_VERSION_MIN_REQUIRED < 101200)
    return spx_resource_stats_cpu_time_coarse();
#else
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return TIMESPEC_TO_NS(ts);
#endif
}

size_t spx_resource_stats_own_rss(void)
{
    return 0;
}

void spx_resource_stats_io(size_t * in, size_t * out)
{
    // MacOS doesn't expose any per-process I/O counters equivalent to linux
    // procfs.
    *in = 0;
    *out = 0;
}

// Coarser (usec) wall time for macOS < Sierra
static inline size_t spx_resource_stats_wall_time_coarse(void)
{
    struct timeval tv;
    int ret = 0;
    ret = gettimeofday(&tv, NULL);
    if (ret == 0) {
        return 1000 * (
            tv.tv_sec * 1000 * 1000
                + tv.tv_usec
        );
    }
    return ret;
}

// Coarser (usec) cpu use time for macOS < Sierra
static inline size_t spx_resource_stats_cpu_time_coarse(void)
{
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);

    return 1000 * (
        (ru.ru_utime.tv_sec  + ru.ru_stime.tv_sec ) * 1000 * 1000
            + (ru.ru_utime.tv_usec + ru.ru_stime.tv_usec)
    );
}
