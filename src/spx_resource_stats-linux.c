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

#include <time.h>
#include <stdio.h>
#include <string.h>

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
    int procfs_status_fd;
    int procfs_io_fd;
    size_t io_r_noise;
    char lg_buf[2 * 1024];
} context;

void spx_resource_stats_init(void)
{
    context.init = 1;

    context.procfs_status_fd = open("/proc/self/status", O_RDONLY);

    char procfs_io_file[64];
    snprintf(
        procfs_io_file,
        sizeof(procfs_io_file),
        "/proc/self/task/%lu/io",
        syscall(SYS_gettid)
    );

    context.procfs_io_fd = open(procfs_io_file, O_RDONLY);
    context.io_r_noise = 0;
}

void spx_resource_stats_shutdown(void)
{
    if (!context.init) {
        return;
    }

    if (context.procfs_status_fd != -1) {
        close(context.procfs_status_fd);
        context.procfs_status_fd = -1;
    }

    if (context.procfs_io_fd != -1) {
        close(context.procfs_io_fd);
        context.procfs_io_fd = -1;
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

size_t spx_resource_stats_own_rss(void)
{
    if (context.procfs_status_fd == -1) {
        return 0;
    }

    lseek(context.procfs_status_fd, 0, SEEK_SET);

    context.io_r_noise += read(
        context.procfs_status_fd,
        context.lg_buf,
        sizeof(context.lg_buf)
    );

    size_t stat_name_start = 0;
    int reading_rss_anonymous = 0;
    size_t rss_anonymous = 0;
    size_t i;
    for (i = 0; i < sizeof(context.lg_buf); i++) {
        const char c = context.lg_buf[i];
        if (c == '\n') {
            if (reading_rss_anonymous) {
                break;
            }

            stat_name_start = i + 1;

            continue;
        }

        if (c == ':') {
            if (0 == strncmp(
                "RssAnon",
                context.lg_buf + stat_name_start,
                i - stat_name_start)
            ) {
                reading_rss_anonymous = 1;
            }

            continue;
        }

        if (reading_rss_anonymous) {
            if ('0' <= c && c <= '9') {
                rss_anonymous *= 10;
                rss_anonymous += c - '0';
            }

            continue;
        }
    }

    /* KB to B */
    rss_anonymous *= 1024;

    return rss_anonymous;
}

void spx_resource_stats_io(size_t * in, size_t * out)
{
    *in = 0;
    *out = 0;

    if (context.procfs_io_fd == -1) {
        return;
    }

    lseek(context.procfs_io_fd, 0, SEEK_SET);

    char buf[64];
    context.io_r_noise += read(context.procfs_io_fd, buf, sizeof(buf));

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

    *in -= context.io_r_noise;
}
