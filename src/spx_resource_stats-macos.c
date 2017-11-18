#define _GNU_SOURCE
#include <sys/time.h>
#include <sys/resource.h>

#include "spx_resource_stats.h"
#include "spx_thread.h"

void spx_resource_stats_init(void)
{
}

void spx_resource_stats_shutdown(void)
{
}


size_t spx_resource_stats_wall_time(void)
{
    struct timeval tv;
    int ret = 0;
    ret = gettimeofday(&tv, NULL);
    if (ret == 0) {
        return tv.tv_sec * 1000 * 1000 + tv.tv_usec;
    }
    return ret;
}

size_t spx_resource_stats_cpu_time(void)
{
    struct rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return (ru.ru_utime.tv_sec  + ru.ru_stime.tv_sec ) * 1000 * 1000 +
           (ru.ru_utime.tv_usec + ru.ru_stime.tv_usec);
}

void spx_resource_stats_io(size_t * in, size_t * out)
{
    // MacOS doesn't expose any per-process I/O counters equivalent to linux
    // procfs.
    *in = 0;
    *out = 0;
}
