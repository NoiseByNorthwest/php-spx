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


#ifndef SPX_PROFILER_SAMPLER_H_DEFINED
#define SPX_PROFILER_SAMPLER_H_DEFINED

#include <stddef.h>

#include "spx_profiler.h"

spx_profiler_t * spx_profiler_sampler_create(
    spx_profiler_t * sampled_profiler,
    size_t sampling_period_us
);

#endif /* SPX_PROFILER_SAMPLER_H_DEFINED */
