#define _GNU_SOURCE
#include <time.h>
#include <stdio.h>

#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>

#include "spx_resource_stats.h"
#include "spx_thread.h"

static SPX_THREAD_TLS struct {
    int io_fd;
    size_t io_noise;
} context;

void spx_resource_stats_init(void)
{
    char io_file[64];
    snprintf(
        io_file,
        sizeof(io_file),
        "/proc/self/task/%lu/io",
        syscall(SYS_gettid)
    );

    context.io_fd = open(io_file, O_RDONLY);
    context.io_noise = 0;
}

void spx_resource_stats_shutdown(void)
{
    if (context.io_fd != -1) {
        close(context.io_fd);
    }
}

#define TIMESPEC_TO_US(ts) ((ts).tv_sec * 1000 * 1000 + (ts).tv_nsec / 1000)

size_t spx_resource_stats_wall_time(void)
{
    struct timespec ts;

    /*
     *  CLOCK_REALTIME is used here for wall time because the performance drop (observed on 3.13.0 kernel)
     *  with CLOCK_MONOTONIC_RAW or even CLOCK_MONOTONIC will cause more damages on accuracy than
     *  potential concurrent system clock adjustments.
     *  However it might be best to have optimal clock dispatching according to kernel version. 
     */
    clock_gettime(CLOCK_REALTIME, &ts);

    return TIMESPEC_TO_US(ts);
}

size_t spx_resource_stats_cpu_time(void)
{
    struct timespec ts;
    /*
     *  Linux implementation of CLOCK_PROCESS_CPUTIME_ID does not require to stick
     *  the current thread to the same CPU.
     */
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);

    return TIMESPEC_TO_US(ts);
}

void spx_resource_stats_io(size_t * in, size_t * out)
{
    *in = 0;
    *out = 0;

    if (context.io_fd == -1) {
        return;
    }

    lseek(context.io_fd, 0, SEEK_SET);

    char buf[64];
    context.io_noise += read(context.io_fd, buf, sizeof(buf));

    const char * p;
    size_t * cur_num = in;
    for (p = buf; p != buf + sizeof(buf); p++) {
        /* procfs -> ASCII */
        if ('0' <= *p && *p <= '9') {
            *cur_num *= 10;
            *cur_num += *p - '0';

            continue;
        }

        if (*p == '\n') {
            if (cur_num == out) {
                break;
            }

            cur_num = out;
        }
    }

    *in -= context.io_noise;
}
