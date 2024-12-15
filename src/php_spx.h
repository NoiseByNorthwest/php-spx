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


#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include "main/php.h"

/* linux 2.6+ or OSX */
#if !defined(linux) && !(defined(__APPLE__) && defined(__MACH__)) && !defined(__FreeBSD__) && !defined(_WIN32)
#   error "Only Linux-based OSes, Apple MacOS, FreeBSD and Windows are supported"
#endif

#if (defined(_MSC_VER) && !(defined(_M_X64) || defined(_M_ARM64))) || (!defined(_MSC_VER) && !(defined(__x86_64__) || defined(__aarch64__)))
#   error "Only x86-64 and ARM64 architectures are supported"
#endif

#if ZEND_MODULE_API_NO < 20100525 || ZEND_MODULE_API_NO > 20240924
#   error "Only the following PHP versions are supported: 5.4 to 8.4"
#endif

#define PHP_SPX_EXTNAME "SPX"
#define PHP_SPX_VERSION "0.4.17"

extern zend_module_entry spx_module_entry;
