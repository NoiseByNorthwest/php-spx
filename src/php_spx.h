/* SPX - A simple profiler for PHP
 * Copyright (C) 2017-2020 Sylvain Lassaut <NoiseByNorthwest@gmail.com>
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
#if !defined(linux) && !(defined(__APPLE__) && defined(__MACH__))
#   error "Only Linux-based OS or Apple MacOS are supported"
#endif

#ifndef __x86_64__
#   error "Only x86-64 architecture is supported"
#endif

#if ZEND_MODULE_API_NO < 20131226 || ZEND_MODULE_API_NO > 20190902
#   error "Only the following PHP versions are supported: 5.6 to 7.4"
#endif

#if defined(ZTS) && !defined(CONTINUOUS_INTEGRATION)
#   error "ZTS is not yet supported"
#endif

#define PHP_SPX_EXTNAME "SPX"
#define PHP_SPX_VERSION "0.4.7"

extern zend_module_entry spx_module_entry;
