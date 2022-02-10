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


#if defined(linux)
#   include "spx_resource_stats-linux.c"
#elif defined(__APPLE__) && defined(__MACH__)
#   include "spx_resource_stats-macos.c"
#elif defined(__FreeBSD__)
#   include "spx_resource_stats-freebsd.c"
#else
#   error "Your platform is not supported. Please open an issue."
#endif
