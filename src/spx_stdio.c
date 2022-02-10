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


#if !defined(__unix__) && !(defined(__APPLE__) && defined(__MACH__))
#   error "Your platform is not supported"
#endif

#include <stdio.h>
#include <unistd.h>

static FILE * null_output;
static int null_output_initialized;

int spx_stdio_disable(int fd)
{
    if (!null_output_initialized) {
        null_output_initialized = 1;
        null_output = fopen("/dev/null", "w");
    }

    if (!null_output) {
        return -1;
    }

    int copy = dup(fd);
    if (copy == -1) {
        return -1;
    }

    if (dup2(fileno(null_output), fd) == -1) {
        close(copy);

        return -1;
    }

    return copy;
}

int spx_stdio_restore(int fd, int copy)
{
    if (dup2(copy, fd) == -1) {
        return -1;
    }

    close(copy);

    return fd;
}
