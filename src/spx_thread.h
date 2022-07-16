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


#ifndef SPX_THREAD_H_DEFINED
#define SPX_THREAD_H_DEFINED

#ifdef ZTS
#   if !defined(__CYGWIN__) && defined(WIN32)
#       define SPX_THREAD_TLS __declspec(thread)
#   else
#       define SPX_THREAD_TLS __thread
#   endif
#else
#   define SPX_THREAD_TLS
#endif

#endif /* SPX_THREAD_H_DEFINED */
