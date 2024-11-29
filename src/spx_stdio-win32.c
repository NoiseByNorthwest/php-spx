/* SPX - A simple profiler for PHP
 * Copyright (C) 2017-2024 Sylvain Lassaut <NoiseByNorthwest@gmail.com>
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

#pragma warning(push, 0)

int spx_stdio_disabling_supported(void)
{
    return 0;
}

int spx_stdio_disable(int fd)
{
    return -1;
}

int spx_stdio_restore(int fd, int copy)
{
    return -1;
}

#pragma warning(pop)
