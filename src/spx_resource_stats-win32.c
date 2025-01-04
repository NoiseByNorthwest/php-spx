/* SPX - A simple profiler for PHP
 * Copyright (C) 2017-2025 Sylvain Lassaut <NoiseByNorthwest@gmail.com>
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

#include "spx_resource_stats.h"

void spx_resource_stats_init(void)
{
}

void spx_resource_stats_shutdown(void)
{
}

size_t spx_resource_stats_wall_time(void)
{
    // FIXME implement it
    return 0;
}

size_t spx_resource_stats_cpu_time(void)
{
    // FIXME implement it if possible
    return 0;
}

size_t spx_resource_stats_own_rss(void)
{
    // FIXME implement it if possible
    return 0;
}

void spx_resource_stats_io(size_t * in, size_t * out)
{
    // FIXME implement it if possible
    *in = 0;
    *out = 0;
}
