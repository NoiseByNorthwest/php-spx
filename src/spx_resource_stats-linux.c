/* SPX - A simple profiler for PHP
 * Copyright (C) 2018 Sylvain Lassaut <NoiseByNorthwest@gmail.com>
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
    int init;
    int io_fd;
    size_t io_noise;
} context;

void spx_resource_stats_init(void)
{
    context.init = 1;
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
    if (!context.init) {
        return;
    }

    if (context.io_fd != -1) {
        close(context.io_fd);
        context.io_fd = -1;
    }
}

#define TIMESPEC_TO_NS(ts) ((ts).tv_sec * 1000 * 1000 * 1000 + (ts).tv_nsec)

size_t spx_resource_stats_wall_time(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return TIMESPEC_TO_NS(ts);
}

size_t spx_resource_stats_cpu_time(void)
{
    struct timespec ts;

    /*
     *  Linux implementation of CLOCK_PROCESS_CPUTIME_ID does not require to stick
     *  the current thread to the same CPU.
     */
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);

    return TIMESPEC_TO_NS(ts);
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
