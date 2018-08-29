#define _GNU_SOURCE
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
